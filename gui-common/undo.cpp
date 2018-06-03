// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "bigint.h"
#include "lifealgo.h"
#include "writepattern.h"   // for MC_format, XRLE_format

#include "select.h"         // for Selection
#include "view.h"           // for OutsideLimits, nopattupdate, etc
#include "utils.h"          // for Warning, Fatal, etc
#include "algos.h"          // for algo_type
#include "layer.h"          // for currlayer, numclones, MarkLayerDirty, etc
#include "prefs.h"          // for allowundo, etc
#include "control.h"        // for RestorePattern
#include "file.h"           // for SetPatternTitle
#include "undo.h"

// -----------------------------------------------------------------------------

const char* lack_of_memory = "Due to lack of memory, some changes can't be undone!";

// the following prefixes are used when creating temporary file names
const char* genchange_prefix = "gg_";
const char* setgen_prefix = "gs_";
const char* dupe1_prefix = "g1_";
const char* dupe2_prefix = "g2_";
const char* dupe3_prefix = "g3_";
const char* dupe4_prefix = "g4_";
const char* dupe5_prefix = "g5_";
const char* dupe6_prefix = "g6_";

// -----------------------------------------------------------------------------

// the next two classes are needed because Golly allows multiple starting points
// (by setting the generation count back to 0), so we need to ensure that a Reset
// goes back to the correct starting info

class VariableInfo {
public:
    VariableInfo() {}
    ~VariableInfo() {}
    // note that we have to remember pointer to layer and not its index
    // (the latter can change if user adds/deletes/moves a layer)
    Layer* layerptr;
    std::string savename;
    bigint savex;
    bigint savey;
    int savemag;
    int savebase;
    int saveexpo;
};

class StartingInfo {
public:
    StartingInfo(StartingInfo* dupe, Layer* oldlayer, Layer* newlayer);
    ~StartingInfo();
    void Restore();
    void RemoveClone(Layer* cloneptr);

    // this info is the same in each clone
    bool savedirty;
    algo_type savealgo;
    std::string saverule;
    
    // this info can be different in each clone
    VariableInfo layer[MAX_LAYERS];
    int count;
};

// -----------------------------------------------------------------------------

StartingInfo::StartingInfo(StartingInfo* dupe, Layer* oldlayer, Layer* newlayer)
{
    if (dupe) {
        // called from DuplicateHistory so duplicate given starting info
        savedirty = dupe->savedirty;
        savealgo = dupe->savealgo;
        saverule = dupe->saverule;
        
        // new duplicate layer is not a clone
        count = 0;
        for (int n = 0; n < dupe->count; n++) {
            if (dupe->layer[n].layerptr == oldlayer) {
                layer[0].layerptr = newlayer;
                layer[0].savename = dupe->layer[n].savename;
                layer[0].savex = dupe->layer[n].savex;
                layer[0].savey = dupe->layer[n].savey;
                layer[0].savemag = dupe->layer[n].savemag;
                layer[0].savebase = dupe->layer[n].savebase;
                layer[0].saveexpo = dupe->layer[n].saveexpo;
                count = 1;
                break;
            }
        }

    } else {
        // save current starting info (set by most recent SaveStartingPattern)
        savedirty = currlayer->startdirty;
        savealgo = currlayer->startalgo;
        saverule = currlayer->startrule;
        
        // save variable info for currlayer and its clones (if any)
        count = 0;
        for (int i = 0; i < numlayers; i++) {
            Layer* lptr = GetLayer(i);
            if (lptr == currlayer ||
                (lptr->cloneid > 0 && lptr->cloneid == currlayer->cloneid)) {
                layer[count].layerptr = lptr;
                layer[count].savename = lptr->startname;
                layer[count].savex = lptr->startx;
                layer[count].savey = lptr->starty;
                layer[count].savemag = lptr->startmag;
                layer[count].savebase = lptr->startbase;
                layer[count].saveexpo = lptr->startexpo;
                count++;
            }
        }
        if (count == 0) Warning("Bug detected in StartingInfo ctor!");
    }
}

// -----------------------------------------------------------------------------

StartingInfo::~StartingInfo()
{
    // no need to do anything
}

// -----------------------------------------------------------------------------

void StartingInfo::Restore()
{
    // restore starting info (for use by next ResetPattern)
    currlayer->startdirty = savedirty;
    currlayer->startalgo = savealgo;
    currlayer->startrule = saverule;
    
    // restore variable info for currlayer and its clones (if any);
    // note that currlayer might have changed since the starting info
    // was saved, and there might be more or fewer clones
    for (int i = 0; i < numlayers; i++) {
        Layer* lptr = GetLayer(i);
        for (int n = 0; n < count; n++) {
            if (lptr == layer[n].layerptr) {
                lptr->startname = layer[n].savename;
                lptr->startx = layer[n].savex;
                lptr->starty = layer[n].savey;
                lptr->startmag = layer[n].savemag;
                lptr->startbase = layer[n].savebase;
                lptr->startexpo = layer[n].saveexpo;
                break;
            }
        }
    }
}

// -----------------------------------------------------------------------------

void StartingInfo::RemoveClone(Layer* cloneptr)
{
    for (int n = 0; n < count; n++) {
        if (layer[n].layerptr == cloneptr) {
            layer[n].layerptr = NULL;
            return;
        }
    }
}

// -----------------------------------------------------------------------------

// encapsulate change info stored in undo/redo lists

typedef enum {
    cellstates,          // one or more cell states were changed
    fliptb,              // selection was flipped top-bottom
    fliplr,              // selection was flipped left-right
    rotatecw,            // selection was rotated clockwise
    rotateacw,           // selection was rotated anticlockwise
    rotatepattcw,        // pattern was rotated clockwise
    rotatepattacw,       // pattern was rotated anticlockwise
    namechange,          // layer name was changed

    // WARNING: code in UndoChange/RedoChange assumes only changes < selchange
    // can alter the layer's dirty state; ie. the olddirty/newdirty flags are
    // not used for all the following changes

    selchange,           // selection was changed
    genchange,           // pattern was generated
    setgen,              // generation count was changed
    rulechange,          // rule was changed
    algochange,          // algorithm was changed
    scriptstart,         // later changes were made by script
    scriptfinish         // earlier changes were made by script
} change_type;

class ChangeNode {
public:
    ChangeNode(change_type id);
    ~ChangeNode();

    bool DoChange(bool undo);
    // do the undo/redo; if it returns false (eg. user has aborted a lengthy
    // rotate/flip operation) then cancel the undo/redo

    void ChangeCells(bool undo);
    // change cell states using cellinfo

    change_type changeid;                   // specifies the type of change
    bool olddirty;                          // layer's dirty state before change
    bool newdirty;                          // layer's dirty state after change

    // cellstates info
    cell_change* cellinfo;                  // dynamic array of cell changes
    unsigned int cellcount;                 // number of cell changes in array

    // rotatecw/rotateacw/selchange info
    Selection oldsel, newsel;               // old and new selections

    // genchange info
    bool scriptgen;                         // gen change was done by script?
    std::string oldfile, newfile;           // old and new pattern files
    bigint oldgen, newgen;                  // old and new generation counts
    bigint oldx, oldy, newx, newy;          // old and new positions
    int oldmag, newmag;                     // old and new scales
    int oldbase, newbase;                   // old and new base steps
    int oldexpo, newexpo;                   // old and new step exponents
    StartingInfo* startinfo;                // saves starting info for ResetPattern
    // also uses oldsel, newsel

    // setgen info
    bigint oldstartgen, newstartgen;            // old and new startgen values
    bool oldsave, newsave;                      // old and new savestart states
    std::string oldtempstart, newtempstart;     // old and new tempstart paths
    std::string oldcurrfile, newcurrfile;       // old and new currfile paths
    // also uses oldgen, newgen
    // and startinfo

    // namechange info
    std::string oldname, newname;           // old and new layer names
    Layer* whichlayer;                      // which layer was changed
    // also uses oldsave, newsave
    // and oldcurrfile, newcurrfile

    // rulechange info
    std::string oldrule, newrule;           // old and new rules
    // also uses oldsel, newsel

    // algochange info
    algo_type oldalgo, newalgo;             // old and new algorithm types
    // also uses oldrule, newrule
    // and oldsel, newsel
};

// -----------------------------------------------------------------------------

ChangeNode::ChangeNode(change_type id)
{
    changeid = id;
    startinfo = NULL;
    whichlayer = NULL;      // simplifies UndoRedo::DeletingClone
    cellinfo = NULL;
    cellcount = 0;
    oldfile.clear();
    newfile.clear();
    oldtempstart.clear();
    newtempstart.clear();
    oldcurrfile.clear();
    newcurrfile.clear();
}

// -----------------------------------------------------------------------------

bool delete_all_temps = false;   // ok to delete all temporary files?

ChangeNode::~ChangeNode()
{
    if (startinfo) delete startinfo;
    if (cellinfo) free(cellinfo);
    
    // it's always ok to delete oldfile and newfile if they exist

    if (!oldfile.empty() && FileExists(oldfile)) {
        RemoveFile(oldfile);
    }
    
    if (!newfile.empty() && FileExists(newfile)) {
        RemoveFile(newfile);
    }

    if (delete_all_temps) {
        // we're in ClearUndoRedo so it's safe to delete oldtempstart/newtempstart/oldcurrfile/newcurrfile
        // if they are in tempdir and not being used to store the current layer's starting pattern
        // (the latter condition allows user to Reset after disabling undo/redo)
        
        if (!oldtempstart.empty() && FileExists(oldtempstart) &&
            oldtempstart.compare(0,tempdir.length(),tempdir) == 0 && oldtempstart != currlayer->currfile) {
            RemoveFile(oldtempstart);
        }
        
        if (!newtempstart.empty() && FileExists(newtempstart) &&
            newtempstart.compare(0,tempdir.length(),tempdir) == 0 && newtempstart != currlayer->currfile) {
            RemoveFile(newtempstart);
        }
        
        if (!oldcurrfile.empty() && FileExists(oldcurrfile) &&
            oldcurrfile.compare(0,tempdir.length(),tempdir) == 0 && oldcurrfile != currlayer->currfile) {
            RemoveFile(oldcurrfile);
        }
        
        if (!newcurrfile.empty() && FileExists(newcurrfile) &&
            newcurrfile.compare(0,tempdir.length(),tempdir) == 0 && newcurrfile != currlayer->currfile) {
            RemoveFile(newcurrfile);
        }
    }
}

// -----------------------------------------------------------------------------

void ChangeNode::ChangeCells(bool undo)
{
    // avoid possible pattern update during a setcell call (can happen if cellcount is large)
    nopattupdate = true;

    // change state of cell(s) stored in cellinfo array
    if (undo) {
        // we must undo the cell changes in reverse order in case
        // a script has changed the same cell more than once
        unsigned int i = cellcount;
        while (i > 0) {
            i--;
            currlayer->algo->setcell(cellinfo[i].x, cellinfo[i].y, cellinfo[i].oldstate);
        }
    } else {
        unsigned int i = 0;
        while (i < cellcount) {
            currlayer->algo->setcell(cellinfo[i].x, cellinfo[i].y, cellinfo[i].newstate);
            i++;
        }
    }
    if (cellcount > 0) currlayer->algo->endofpattern();

    nopattupdate = false;
}

// -----------------------------------------------------------------------------

bool ChangeNode::DoChange(bool undo)
{
    switch (changeid) {
        case cellstates:
            if (cellcount > 0) ChangeCells(undo);
            break;

        case fliptb:
        case fliplr:
            // pass in true so FlipSelection won't save changes or call MarkLayerDirty
            if (!FlipSelection(changeid == fliptb, true)) return false;
            break;

        case rotatepattcw:
        case rotatepattacw:
            // pass in true so RotateSelection won't save changes or call MarkLayerDirty
            if (!RotateSelection(changeid == rotatepattcw ? !undo : undo, true)) return false;
            break;

        case rotatecw:
        case rotateacw:
            if (cellcount > 0) ChangeCells(undo);
            // rotate selection edges
            if (undo) {
                currlayer->currsel = oldsel;
            } else {
                currlayer->currsel = newsel;
            }
            DisplaySelectionSize();
            break;

        case selchange:
            if (undo) {
                currlayer->currsel = oldsel;
            } else {
                currlayer->currsel = newsel;
            }
            if (SelectionExists()) DisplaySelectionSize();
            break;

        case genchange:
            currlayer->currfile = oldcurrfile;
            if (undo) {
                currlayer->currsel = oldsel;
                RestorePattern(oldgen, oldfile.c_str(), oldx, oldy, oldmag, oldbase, oldexpo);
            } else {
                if (startinfo) {
                    // restore starting info for use by ResetPattern
                    startinfo->Restore();
                }
                currlayer->currsel = newsel;
                RestorePattern(newgen, newfile.c_str(), newx, newy, newmag, newbase, newexpo);
            }
            break;

        case setgen:
            if (undo) {
                ChangeGenCount(oldgen.tostring(), true);
                currlayer->startgen = oldstartgen;
                currlayer->savestart = oldsave;
                currlayer->tempstart = oldtempstart;
                currlayer->currfile = oldcurrfile;
                if (startinfo) {
                    // restore starting info for use by ResetPattern
                    startinfo->Restore();
                }
            } else {
                ChangeGenCount(newgen.tostring(), true);
                currlayer->startgen = newstartgen;
                currlayer->savestart = newsave;
                currlayer->tempstart = newtempstart;
                currlayer->currfile = newcurrfile;
            }
            break;

        case namechange:
            if (whichlayer == NULL) {
                // the layer has been deleted so ignore name change
            } else {
                // note that if whichlayer != currlayer then we're changing the
                // name of a non-active cloned layer
                if (undo) {
                    whichlayer->currname = oldname;
                    currlayer->currfile = oldcurrfile;
                    currlayer->savestart = oldsave;
                } else {
                    whichlayer->currname = newname;
                    currlayer->currfile = newcurrfile;
                    currlayer->savestart = newsave;
                }
                if (whichlayer == currlayer) {
                    if (olddirty == newdirty) SetPatternTitle(currlayer->currname.c_str());
                    // if olddirty != newdirty then UndoChange/RedoChange will call
                    // MarkLayerClean/MarkLayerDirty (they call SetPatternTitle)
                }
            }
            break;

        case rulechange:
            if (undo) {
                RestoreRule(oldrule.c_str());
                currlayer->currsel = oldsel;
            } else {
                RestoreRule(newrule.c_str());
                currlayer->currsel = newsel;
            }
            if (cellcount > 0) {
                ChangeCells(undo);
            }
            // switch to default colors for new rule
            UpdateLayerColors();
            break;

        case algochange:
            // pass in true so ChangeAlgorithm won't call RememberAlgoChange
            if (undo) {
                ChangeAlgorithm(oldalgo, oldrule.c_str(), true);
                currlayer->currsel = oldsel;
            } else {
                ChangeAlgorithm(newalgo, newrule.c_str(), true);
                currlayer->currsel = newsel;
            }
            if (cellcount > 0) {
                ChangeCells(undo);
            }
            // ChangeAlgorithm has called UpdateLayerColors()
            break;

        case scriptstart:
        case scriptfinish:
            // should never happen
            Warning("Bug detected in DoChange!");
            break;
    }
    return true;
}

// -----------------------------------------------------------------------------

UndoRedo::UndoRedo()
{
    numchanges = 0;               // for 1st SaveCellChange
    maxchanges = 0;               // ditto
    badalloc = false;             // true if malloc/realloc fails
    cellarray = NULL;             // play safe
    savecellchanges = false;      // no script cell changes are pending
    savegenchanges = false;       // no script gen changes are pending
    doingscriptchanges = false;   // not undoing/redoing script changes
    prevfile.clear();             // play safe for ClearUndoRedo
    startcount = 0;               // unfinished RememberGenStart calls

    // need to remember if script has created a new layer (not a clone)
    if (inscript) RememberScriptStart();
}

// -----------------------------------------------------------------------------

UndoRedo::~UndoRedo()
{
    ClearUndoRedo();
}

// -----------------------------------------------------------------------------

void UndoRedo::ClearUndoHistory()
{
    while (!undolist.empty()) {
        delete undolist.front();
        undolist.pop_front();
    }
}

// -----------------------------------------------------------------------------

void UndoRedo::ClearRedoHistory()
{
    while (!redolist.empty()) {
        delete redolist.front();
        redolist.pop_front();
    }
}

// -----------------------------------------------------------------------------

void UndoRedo::SaveCellChange(int x, int y, int oldstate, int newstate)
{
    if (numchanges == maxchanges) {
        if (numchanges == 0) {
            // initially allocate room for 1 cell change
            maxchanges = 1;
            cellarray = (cell_change*) malloc(maxchanges * sizeof(cell_change));
            if (cellarray == NULL) {
                badalloc = true;
                return;
            }
            // ~ChangeNode or ForgetCellChanges will free cellarray
        } else {
            // double size of cellarray
            cell_change* newptr =
            (cell_change*) realloc(cellarray, maxchanges * 2 * sizeof(cell_change));
            if (newptr == NULL) {
                badalloc = true;
                return;
            }
            cellarray = newptr;
            maxchanges *= 2;
        }
    }

    cellarray[numchanges].x = x;
    cellarray[numchanges].y = y;
    cellarray[numchanges].oldstate = oldstate;
    cellarray[numchanges].newstate = newstate;

    numchanges++;
}

// -----------------------------------------------------------------------------

void UndoRedo::ForgetCellChanges()
{
    if (numchanges > 0) {
        if (cellarray) {
            free(cellarray);
        } else {
            Warning("Bug detected in ForgetCellChanges!");
        }
        numchanges = 0;      // reset for next SaveCellChange
        maxchanges = 0;      // ditto
        badalloc = false;
    }
}

// -----------------------------------------------------------------------------

bool UndoRedo::RememberCellChanges(const char* action, bool olddirty)
{
    if (numchanges > 0) {
        if (numchanges < maxchanges) {
            // reduce size of cellarray
            cell_change* newptr =
            (cell_change*) realloc(cellarray, numchanges * sizeof(cell_change));
            if (newptr != NULL) cellarray = newptr;
            // in the unlikely event that newptr is NULL, cellarray should
            // still point to valid data
        }

        ClearRedoHistory();

        // add cellstates node to head of undo list
        ChangeNode* change = new ChangeNode(cellstates);
        if (change == NULL) Fatal("Failed to create cellstates node!");

        change->cellinfo = cellarray;
        change->cellcount = numchanges;
        change->olddirty = olddirty;
        change->newdirty = true;

        undolist.push_front(change);

        numchanges = 0;      // reset for next SaveCellChange
        maxchanges = 0;      // ditto

        if (badalloc) {
            Warning(lack_of_memory);
            badalloc = false;
        }
        return true;    // at least one cell changed state
    }
    return false;       // no cells changed state (SaveCellChange wasn't called)
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberFlip(bool topbot, bool olddirty)
{
    ClearRedoHistory();

    // add fliptb/fliplr node to head of undo list
    ChangeNode* change = new ChangeNode(topbot ? fliptb : fliplr);
    if (change == NULL) Fatal("Failed to create flip node!");

    change->olddirty = olddirty;
    change->newdirty = true;

    undolist.push_front(change);
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberRotation(bool clockwise, bool olddirty)
{
    ClearRedoHistory();

    // add rotatepattcw/rotatepattacw node to head of undo list
    ChangeNode* change = new ChangeNode(clockwise ? rotatepattcw : rotatepattacw);
    if (change == NULL) Fatal("Failed to create simple rotation node!");

    change->olddirty = olddirty;
    change->newdirty = true;

    undolist.push_front(change);
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberRotation(bool clockwise, Selection& oldsel, Selection& newsel,
                                bool olddirty)
{
    ClearRedoHistory();

    // add rotatecw/rotateacw node to head of undo list
    ChangeNode* change = new ChangeNode(clockwise ? rotatecw : rotateacw);
    if (change == NULL) Fatal("Failed to create rotation node!");

    change->oldsel = oldsel;
    change->newsel = newsel;
    change->olddirty = olddirty;
    change->newdirty = true;

    // if numchanges == 0 we still need to rotate selection edges
    if (numchanges > 0) {
        if (numchanges < maxchanges) {
            // reduce size of cellarray
            cell_change* newptr =
            (cell_change*) realloc(cellarray, numchanges * sizeof(cell_change));
            if (newptr != NULL) cellarray = newptr;
        }

        change->cellinfo = cellarray;
        change->cellcount = numchanges;

        numchanges = 0;      // reset for next SaveCellChange
        maxchanges = 0;      // ditto
        if (badalloc) {
            Warning(lack_of_memory);
            badalloc = false;
        }
    }

    undolist.push_front(change);
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberSelection(const char* action)
{
    if (currlayer->savesel == currlayer->currsel) {
        // selection has not changed
        return;
    }

    if (generating) {
        // don't record selection changes while a pattern is generating;
        // RememberGenStart and RememberGenFinish will remember the overall change
        return;
    }

    ClearRedoHistory();

    // add selchange node to head of undo list
    ChangeNode* change = new ChangeNode(selchange);
    if (change == NULL) Fatal("Failed to create selchange node!");

    change->oldsel = currlayer->savesel;
    change->newsel = currlayer->currsel;

    undolist.push_front(change);
}

// -----------------------------------------------------------------------------

void UndoRedo::SaveCurrentPattern(const char* tempfile)
{
    const char* err = NULL;
    if ( currlayer->algo->hyperCapable() ) {
        // save hlife pattern in a macrocell file
        err = WritePattern(tempfile, MC_format, no_compression, 0, 0, 0, 0);
    } else {
        // can only save RLE file if edges are within getcell/setcell limits
        bigint top, left, bottom, right;
        currlayer->algo->findedges(&top, &left, &bottom, &right);
        if ( OutsideLimits(top, left, bottom, right) ) {
            err = "Pattern is too big to save.";
        } else {
            // use XRLE format so the pattern's top left location and the current
            // generation count are stored in the file
            err = WritePattern(tempfile, XRLE_format, no_compression,
                               top.toint(), left.toint(), bottom.toint(), right.toint());
        }
    }
    if (err) Warning(err);
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberGenStart()
{
    startcount++;
    if (startcount > 1) {
        // return immediately and ignore next RememberGenFinish call;
        // this can happen in Linux app if user holds down space bar
        return;
    }

    if (inscript) {
        if (savegenchanges) return;   // ignore consecutive run/step command
        savegenchanges = true;
        // we're about to do first run/step command of a (possibly long)
        // sequence, so save starting info
    }

    // save current generation, selection, position, scale, speed, etc
    prevgen = currlayer->algo->getGeneration();
    prevsel = currlayer->currsel;
    prevx = currlayer->view->x;
    prevy = currlayer->view->y;
    prevmag = currlayer->view->getmag();
    prevbase = currlayer->currbase;
    prevexpo = currlayer->currexpo;

    if (prevgen == currlayer->startgen) {
        // we can just reset to starting pattern
        prevfile.clear();
    } else {
        // save starting pattern in a unique temporary file
        prevfile = CreateTempFileName(genchange_prefix);

        // if head of undo list is a genchange node then we can copy that
        // change node's newfile to prevfile; this makes consecutive generating
        // runs faster (setting prevfile to newfile would be even faster but it's
        // difficult to avoid the file being deleted if the redo list is cleared)
        if (!undolist.empty()) {
            std::list<ChangeNode*>::iterator node = undolist.begin();
            ChangeNode* change = *node;
            if (change->changeid == genchange) {
                if (CopyFile(change->newfile, prevfile)) {
                    return;
                } else {
                    Warning("Failed to copy temporary file!");
                    // continue and call SaveCurrentPattern
                }
            }
        }

        SaveCurrentPattern(prevfile.c_str());
    }
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberGenFinish()
{
    startcount--;
    if (startcount > 0) return;

    if (startcount < 0) {
        // this can happen if a script has pending gen changes that need
        // to be remembered (ie. savegenchanges is now false) so reset
        // startcount for the next RememberGenStart call
        startcount = 0;
    }

    if (inscript && savegenchanges) return;   // ignore consecutive run/step command

    // generation count might not have changed (can happen in Linux app and iOS Golly)
    if (prevgen == currlayer->algo->getGeneration()) {
        // delete prevfile created by RememberGenStart
        if (!prevfile.empty() && FileExists(prevfile)) {
            RemoveFile(prevfile);
        }
        prevfile.clear();
        return;
    }

    std::string fpath;
    if (currlayer->algo->getGeneration() == currlayer->startgen) {
        // this can happen if script called reset() so just use starting pattern
        fpath.clear();
    } else {
        // save finishing pattern in a unique temporary file
        fpath = CreateTempFileName(genchange_prefix);
        SaveCurrentPattern(fpath.c_str());
    }

    ClearRedoHistory();

    // add genchange node to head of undo list
    ChangeNode* change = new ChangeNode(genchange);
    if (change == NULL) Fatal("Failed to create genchange node!");

    change->scriptgen = inscript;
    change->oldgen = prevgen;
    change->newgen = currlayer->algo->getGeneration();
    change->oldfile = prevfile;
    change->newfile = fpath;
    change->oldx = prevx;
    change->oldy = prevy;
    change->newx = currlayer->view->x;
    change->newy = currlayer->view->y;
    change->oldmag = prevmag;
    change->newmag = currlayer->view->getmag();
    change->oldbase = prevbase;
    change->newbase = currlayer->currbase;
    change->oldexpo = prevexpo;
    change->newexpo = currlayer->currexpo;
    change->oldsel = prevsel;
    change->newsel = currlayer->currsel;

    // also remember the file containing the starting pattern
    // (in case it is changed by by RememberSetGen or RememberNameChange)
    change->oldcurrfile = currlayer->currfile;

    if (change->oldgen == currlayer->startgen) {
        // save starting info set by recent SaveStartingPattern call
        // (the info will be restored when redoing this genchange node)
        change->startinfo = new StartingInfo(NULL, NULL, NULL);
    }

    // prevfile has been saved in change->oldfile (~ChangeNode will delete it)
    prevfile.clear();

    undolist.push_front(change);
}

// -----------------------------------------------------------------------------

void UndoRedo::AddGenChange()
{
    // add a genchange node to empty undo list
    if (!undolist.empty())
        Warning("AddGenChange bug: undo list NOT empty!");

    // use starting pattern info for previous state
    prevgen = currlayer->startgen;
    prevsel = currlayer->startsel;
    prevx = currlayer->startx;
    prevy = currlayer->starty;
    prevmag = currlayer->startmag;
    prevbase = currlayer->startbase;
    prevexpo = currlayer->startexpo;
    prevfile.clear();

    // pretend RememberGenStart was called
    startcount = 1;

    // avoid RememberGenFinish returning early if inscript is true
    savegenchanges = false;
    RememberGenFinish();

    if (undolist.empty())
        Warning("AddGenChange bug: undo list is empty!");
}

// -----------------------------------------------------------------------------

void UndoRedo::SyncUndoHistory()
{
    // reset startcount for the next RememberGenStart call
    startcount = 0;

    // synchronize undo history due to a ResetPattern call;
    // wind back the undo list to just past the genchange node that
    // matches the current layer's starting gen count
    std::list<ChangeNode*>::iterator node;
    ChangeNode* change;
    while (!undolist.empty()) {
        node = undolist.begin();
        change = *node;

        // remove node from head of undo list and prepend it to redo list
        undolist.erase(node);
        redolist.push_front(change);

        if (change->changeid == genchange && change->oldgen == currlayer->startgen) {
            if (change->scriptgen) {
                // gen change was done by a script so keep winding back the undo list
                // until the scriptstart node, or until the list is empty
                while (!undolist.empty()) {
                    node = undolist.begin();
                    change = *node;
                    if (change->changeid == scriptstart) {
                        undolist.erase(node);
                        redolist.push_front(change);
                        break;
                    }
                    // undo this change so Reset and Undo restore to the same pattern
                    UndoChange();
                }
            }
            return;
        }
    }
    
    // should never get here
    Warning("Bug detected in SyncUndoHistory!");
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberSetGen(bigint& oldgen, bigint& newgen,
                              bigint& oldstartgen, bool oldsave)
{
    std::string oldtempstart = currlayer->tempstart;
    std::string oldcurrfile = currlayer->currfile;
    if (oldgen > oldstartgen && newgen <= oldstartgen) {
        // if pattern is generated then tempstart will be clobbered by
        // SaveStartingPattern, so change tempstart to a new temporary file
        currlayer->tempstart = CreateTempFileName(setgen_prefix);

        // also need to update currfile (currlayer->savestart is true)
        currlayer->currfile = currlayer->tempstart;
    }

    ClearRedoHistory();

    // add setgen node to head of undo list
    ChangeNode* change = new ChangeNode(setgen);
    if (change == NULL) Fatal("Failed to create setgen node!");

    change->oldgen = oldgen;
    change->newgen = newgen;
    change->oldstartgen = oldstartgen;
    change->newstartgen = currlayer->startgen;
    change->oldsave = oldsave;
    change->newsave = currlayer->savestart;
    change->oldtempstart = oldtempstart;
    change->newtempstart = currlayer->tempstart;
    change->oldcurrfile = oldcurrfile;
    change->newcurrfile = currlayer->currfile;

    if (oldtempstart != currlayer->tempstart) {
        // save starting info set by most recent SaveStartingPattern call
        // (the info will be restored when undoing this setgen node)
        change->startinfo = new StartingInfo(NULL, NULL, NULL);
    }

    undolist.push_front(change);
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberNameChange(const char* oldname, const char* oldcurrfile,
                                  bool oldsave, bool olddirty)
{
    if (oldname == currlayer->currname && oldcurrfile == currlayer->currfile &&
        oldsave == currlayer->savestart && olddirty == currlayer->dirty) return;

    ClearRedoHistory();

    // add namechange node to head of undo list
    ChangeNode* change = new ChangeNode(namechange);
    if (change == NULL) Fatal("Failed to create namechange node!");

    change->oldname = oldname;
    change->newname = currlayer->currname;
    change->oldcurrfile = oldcurrfile;
    change->newcurrfile = currlayer->currfile;
    change->oldsave = oldsave;
    change->newsave = currlayer->savestart;
    change->olddirty = olddirty;
    change->newdirty = currlayer->dirty;

    // cloned layers share the same undo/redo history but each clone can have
    // a different name, so we need to remember which layer was changed
    change->whichlayer = currlayer;

    undolist.push_front(change);
}

// -----------------------------------------------------------------------------

void UndoRedo::DeletingClone(int index)
{
    // the given cloned layer is about to be deleted, so we need to go thru the
    // undo/redo lists and fix up any nodes that have pointers to that clone
    // (very ugly, but I don't see any better solution if we're going to allow
    // cloned layers to have different names)

    Layer* cloneptr = GetLayer(index);
    std::list<ChangeNode*>::iterator node;

    node = undolist.begin();
    while (node != undolist.end()) {
        ChangeNode* change = *node;
        if (change->whichlayer == cloneptr) {
            change->whichlayer = NULL;
        }
        if (change->startinfo) {
            change->startinfo->RemoveClone(cloneptr);
        }
        node++;
    }

    node = redolist.begin();
    while (node != redolist.end()) {
        ChangeNode* change = *node;
        if (change->whichlayer == cloneptr) {
            change->whichlayer = NULL;
        }
        if (change->startinfo) {
            change->startinfo->RemoveClone(cloneptr);
        }
        node++;
    }
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberRuleChange(const char* oldrule)
{
    std::string newrule = currlayer->algo->getrule();
    if (oldrule == newrule) return;

    ClearRedoHistory();

    // add rulechange node to head of undo list
    ChangeNode* change = new ChangeNode(rulechange);
    if (change == NULL) Fatal("Failed to create rulechange node!");

    change->oldrule = oldrule;
    change->newrule = newrule;

    // selection might have changed if grid became smaller
    change->oldsel = currlayer->savesel;
    change->newsel = currlayer->currsel;

    // SaveCellChange may have been called
    if (numchanges > 0) {
        if (numchanges < maxchanges) {
            // reduce size of cellarray
            cell_change* newptr =
            (cell_change*) realloc(cellarray, numchanges * sizeof(cell_change));
            if (newptr != NULL) cellarray = newptr;
        }

        change->cellinfo = cellarray;
        change->cellcount = numchanges;

        numchanges = 0;      // reset for next SaveCellChange
        maxchanges = 0;      // ditto
        if (badalloc) {
            Warning(lack_of_memory);
            badalloc = false;
        }
    }

    undolist.push_front(change);
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberAlgoChange(algo_type oldalgo, const char* oldrule)
{
    ClearRedoHistory();

    // add algochange node to head of undo list
    ChangeNode* change = new ChangeNode(algochange);
    if (change == NULL) Fatal("Failed to create algochange node!");

    change->oldalgo = oldalgo;
    change->newalgo = currlayer->algtype;
    change->oldrule = oldrule;
    change->newrule = currlayer->algo->getrule();

    // selection might have changed if grid became smaller
    change->oldsel = currlayer->savesel;
    change->newsel = currlayer->currsel;

    // SaveCellChange may have been called
    if (numchanges > 0) {
        if (numchanges < maxchanges) {
            // reduce size of cellarray
            cell_change* newptr =
            (cell_change*) realloc(cellarray, numchanges * sizeof(cell_change));
            if (newptr != NULL) cellarray = newptr;
        }

        change->cellinfo = cellarray;
        change->cellcount = numchanges;

        numchanges = 0;      // reset for next SaveCellChange
        maxchanges = 0;      // ditto
        if (badalloc) {
            Warning(lack_of_memory);
            badalloc = false;
        }
    }

    undolist.push_front(change);
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberScriptStart()
{
    if (!undolist.empty()) {
        std::list<ChangeNode*>::iterator node = undolist.begin();
        ChangeNode* change = *node;
        if (change->changeid == scriptstart) {
            // ignore consecutive RememberScriptStart calls made by RunScript
            // due to cloned layers
            if (numclones == 0) Warning("Unexpected RememberScriptStart call!");
            return;
        }
    }

    // add scriptstart node to head of undo list
    ChangeNode* change = new ChangeNode(scriptstart);
    if (change == NULL) Fatal("Failed to create scriptstart node!");

    undolist.push_front(change);
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberScriptFinish()
{
    if (undolist.empty()) {
        // this can happen if RunScript calls RememberScriptFinish multiple times
        // due to cloned layers AND the script made no changes
        if (numclones == 0) {
            // there should be at least a scriptstart node (see ClearUndoRedo)
            Warning("Bug detected in RememberScriptFinish!");
        }
        return;
    }

    // if head of undo list is a scriptstart node then simply remove it
    // and return (ie. the script didn't make any changes)
    std::list<ChangeNode*>::iterator node = undolist.begin();
    ChangeNode* change = *node;
    if (change->changeid == scriptstart) {
        undolist.erase(node);
        delete change;
        return;
    } else if (change->changeid == scriptfinish) {
        // ignore consecutive RememberScriptFinish calls made by RunScript
        // due to cloned layers
        if (numclones == 0) Warning("Unexpected RememberScriptFinish call!");
        return;
    }

    // add scriptfinish node to head of undo list
    change = new ChangeNode(scriptfinish);
    if (change == NULL) Fatal("Failed to create scriptfinish node!");

    undolist.push_front(change);
}

// -----------------------------------------------------------------------------

bool UndoRedo::CanUndo()
{
    // we need to allow undo if generating even though undo list might be empty
    // (selecting Undo will stop generating and add genchange node to undo list)
    if (allowundo && generating) return true;

    return !undolist.empty() && !inscript;
}

// -----------------------------------------------------------------------------

bool UndoRedo::CanRedo()
{
    return !redolist.empty() && !inscript && !generating;
}

// -----------------------------------------------------------------------------

void UndoRedo::UndoChange()
{
    if (!CanUndo()) return;

    // get change info from head of undo list and do the change
    std::list<ChangeNode*>::iterator node = undolist.begin();
    ChangeNode* change = *node;

    if (change->changeid == scriptfinish) {
        // undo all changes between scriptfinish and scriptstart nodes;
        // first remove scriptfinish node from undo list and add it to redo list
        undolist.erase(node);
        redolist.push_front(change);

        while (change->changeid != scriptstart) {
            // call UndoChange recursively; temporarily set doingscriptchanges so
            // UndoChange won't return if DoChange is aborted
            doingscriptchanges = true;
            UndoChange();
            doingscriptchanges = false;
            node = undolist.begin();
            if (node == undolist.end()) Fatal("Bug in UndoChange!");
            change = *node;
        }
        // continue below so that scriptstart node is removed from undo list
        // and added to redo list

    } else {
        // user might abort the undo (eg. a lengthy rotate/flip)
        if (!change->DoChange(true) && !doingscriptchanges) return;
    }

    // remove node from head of undo list (doesn't delete node's data)
    undolist.erase(node);

    if (change->changeid < selchange && change->olddirty != change->newdirty) {
        // change dirty flag
        if (change->olddirty) {
            currlayer->dirty = false;  // make sure it changes
            MarkLayerDirty();
        } else {
            MarkLayerClean(currlayer->currname.c_str());
        }
    }

    // add change to head of redo list
    redolist.push_front(change);
}

// -----------------------------------------------------------------------------

void UndoRedo::RedoChange()
{
    if (!CanRedo()) return;

    // get change info from head of redo list and do the change
    std::list<ChangeNode*>::iterator node = redolist.begin();
    ChangeNode* change = *node;

    if (change->changeid == scriptstart) {
        // redo all changes between scriptstart and scriptfinish nodes;
        // first remove scriptstart node from redo list and add it to undo list
        redolist.erase(node);
        undolist.push_front(change);

        while (change->changeid != scriptfinish) {
            // call RedoChange recursively; temporarily set doingscriptchanges so
            // RedoChange won't return if DoChange is aborted
            doingscriptchanges = true;
            RedoChange();
            doingscriptchanges = false;
            node = redolist.begin();
            if (node == redolist.end()) Fatal("Bug in RedoChange!");
            change = *node;
        }
        // continue below so that scriptfinish node is removed from redo list
        // and added to undo list

    } else {
        // user might abort the redo (eg. a lengthy rotate/flip)
        if (!change->DoChange(false) && !doingscriptchanges) return;
    }

    // remove node from head of redo list (doesn't delete node's data)
    redolist.erase(node);

    if (change->changeid < selchange && change->olddirty != change->newdirty) {
        // change dirty flag
        if (change->newdirty) {
            currlayer->dirty = false;  // make sure it changes
            MarkLayerDirty();
        } else {
            MarkLayerClean(currlayer->currname.c_str());
        }
    }

    // add change to head of undo list
    undolist.push_front(change);
}

// -----------------------------------------------------------------------------

void UndoRedo::ClearUndoRedo()
{
    // free cellarray in case there were SaveCellChange calls not followed
    // by ForgetCellChanges or RememberCellChanges
    ForgetCellChanges();

    if (startcount > 0) {
        // RememberGenStart was not followed by RememberGenFinish
        if (!prevfile.empty() && FileExists(prevfile)) {
            RemoveFile(prevfile);
        }
        prevfile.clear();
        startcount = 0;
    }
    
    // set flag so ChangeNode::~ChangeNode() can delete all temporary files
    delete_all_temps = true;

    // clear the undo/redo lists (and delete each node's data)
    ClearUndoHistory();
    ClearRedoHistory();
    
    delete_all_temps = false;

    if (inscript) {
        // script has called a command like new() so add a scriptstart node
        // to the undo list to match the final scriptfinish node
        RememberScriptStart();
        // reset flags to indicate no pending cell/gen changes
        savecellchanges = false;
        savegenchanges = false;
    }
}

// -----------------------------------------------------------------------------

bool CopyTempFiles(ChangeNode* srcnode, ChangeNode* destnode, const char* tempstart1)
{
    // if srcnode has any existing temporary files then, if necessary, create new
    // temporary file names in the destnode and copy such files
    bool allcopied = true;

    if ( !srcnode->oldfile.empty() && FileExists(srcnode->oldfile) ) {
        destnode->oldfile = CreateTempFileName(dupe1_prefix);
        if ( !CopyFile(srcnode->oldfile, destnode->oldfile) )
            allcopied = false;
    }
    
    if ( !srcnode->newfile.empty() && FileExists(srcnode->newfile) ) {
        destnode->newfile = CreateTempFileName(dupe2_prefix);
        if ( !CopyFile(srcnode->newfile, destnode->newfile) )
            allcopied = false;
    }
    
    if ( !srcnode->oldcurrfile.empty() && FileExists(srcnode->oldcurrfile) ) {
        if (srcnode->oldcurrfile == currlayer->tempstart) {
            // the file has already been copied to tempstart1 by Layer::Layer()
            destnode->oldcurrfile = tempstart1;
        } else if (srcnode->oldcurrfile.compare(0,tempdir.length(),tempdir) == 0) {
            destnode->oldcurrfile = CreateTempFileName(dupe3_prefix);
            if ( !CopyFile(srcnode->oldcurrfile, destnode->oldcurrfile) )
                allcopied = false;
        }
    }
    
    if ( !srcnode->newcurrfile.empty() && FileExists(srcnode->newcurrfile) ) {
        if (srcnode->newcurrfile == srcnode->oldcurrfile) {
            // use destnode->oldcurrfile set above or earlier in DuplicateHistory
            destnode->newcurrfile = destnode->oldcurrfile;
        } else if (srcnode->newcurrfile == currlayer->tempstart) {
            // the file has already been copied to tempstart1 by Layer::Layer()
            destnode->newcurrfile = tempstart1;
        } else if (srcnode->newcurrfile.compare(0,tempdir.length(),tempdir) == 0) {
            destnode->newcurrfile = CreateTempFileName(dupe4_prefix);
            if ( !CopyFile(srcnode->newcurrfile, destnode->newcurrfile) )
                allcopied = false;
        }
    }
    
    if ( !srcnode->oldtempstart.empty() && FileExists(srcnode->oldtempstart) ) {
        if (srcnode->oldtempstart == currlayer->tempstart) {
            // the file has already been copied to tempstart1 by Layer::Layer()
            destnode->oldtempstart = tempstart1;
        } else if (srcnode->oldtempstart.compare(0,tempdir.length(),tempdir) == 0) {
            destnode->oldtempstart = CreateTempFileName(dupe5_prefix);
            if ( !CopyFile(srcnode->oldtempstart, destnode->oldtempstart) )
                allcopied = false;
        }
    }
    
    if ( !srcnode->newtempstart.empty() && FileExists(srcnode->newtempstart) ) {
        if (srcnode->newtempstart == srcnode->oldtempstart) {
            // use destnode->oldtempstart set above or earlier in DuplicateHistory
            destnode->newtempstart = destnode->oldtempstart;
        } else if (srcnode->newtempstart == currlayer->tempstart) {
            // the file has already been copied to tempstart1 by Layer::Layer()
            destnode->newtempstart = tempstart1;
        } else if (srcnode->newtempstart.compare(0,tempdir.length(),tempdir) == 0) {
            destnode->newtempstart = CreateTempFileName(dupe6_prefix);
            if ( !CopyFile(srcnode->newtempstart, destnode->newtempstart) )
                allcopied = false;
        }
    }
    
    return allcopied;
}

// -----------------------------------------------------------------------------

void UndoRedo::DuplicateHistory(Layer* oldlayer, Layer* newlayer)
{
    UndoRedo* history = oldlayer->undoredo;

    // clear the undo/redo lists; note that UndoRedo::UndoRedo has added
    // a scriptstart node to undolist if inscript is true, but we don't
    // want that here because the old layer's history will already have one
    ClearUndoHistory();
    ClearRedoHistory();

    // safer to do our own shallow copy (avoids setting undolist/redolist)
    savecellchanges = history->savecellchanges;
    savegenchanges = history->savegenchanges;
    doingscriptchanges = history->doingscriptchanges;
    numchanges = history->numchanges;
    maxchanges = history->maxchanges;
    badalloc = history->badalloc;
    prevfile = history->prevfile;
    prevgen = history->prevgen;
    prevx = history->prevx;
    prevy = history->prevy;
    prevmag = history->prevmag;
    prevbase = history->prevbase;
    prevexpo = history->prevexpo;
    prevsel = history->prevsel;
    startcount = history->startcount;

    // copy existing temporary file to new name
    if ( !prevfile.empty() && FileExists(prevfile) ) {
        prevfile = CreateTempFileName(genchange_prefix);
        if ( !CopyFile(history->prevfile, prevfile) ) {
            Warning("Could not copy prevfile!");
            return;
        }
    }

    // do a deep copy of dynamically allocated data
    cellarray = NULL;
    if (numchanges > 0 && history->cellarray) {
        cellarray = (cell_change*) malloc(maxchanges * sizeof(cell_change));
        if (cellarray == NULL) {
            Warning("Could not allocate cellarray!");
            return;
        }
        // copy history->cellarray data to this cellarray
        memcpy(cellarray, history->cellarray, numchanges * sizeof(cell_change));
    }

    std::list<ChangeNode*>::iterator node;

    // build a new undolist using history->undolist
    node = history->undolist.begin();
    while (node != undolist.end()) {
        ChangeNode* change = *node;

        ChangeNode* newchange = new ChangeNode(change->changeid);
        if (newchange == NULL) {
            Warning("Failed to copy undolist!");
            ClearUndoHistory();
            return;
        }

        // shallow copy the change node
        *newchange = *change;

        // deep copy any dynamically allocated data
        if (change->cellinfo) {
            int bytes = change->cellcount * sizeof(cell_change);
            newchange->cellinfo = (cell_change*) malloc(bytes);
            if (newchange->cellinfo == NULL) {
                Warning("Could not copy undolist!");
                ClearUndoHistory();
                return;
            }
            memcpy(newchange->cellinfo, change->cellinfo, bytes);
        }
        
        if (change->startinfo) {
            newchange->startinfo = new StartingInfo(change->startinfo, oldlayer, newlayer);
        }

        // if node is a name change then update whichlayer
        if (newchange->changeid == namechange) {
            if (change->whichlayer == oldlayer) {
                newchange->whichlayer = newlayer;
            } else {
                newchange->whichlayer = NULL;
            }
        }

        // copy any existing temporary files to new names
        if (!CopyTempFiles(change, newchange, newlayer->tempstart.c_str())) {
            Warning("Failed to copy temporary file in undolist!");
            ClearUndoHistory();
            return;
        }

        undolist.push_back(newchange);
        node++;
    }

    // build a new redolist using history->redolist
    node = history->redolist.begin();
    while (node != redolist.end()) {
        ChangeNode* change = *node;

        ChangeNode* newchange = new ChangeNode(change->changeid);
        if (newchange == NULL) {
            Warning("Failed to copy redolist!");
            ClearRedoHistory();
            return;
        }

        // shallow copy the change node
        *newchange = *change;

        // deep copy any dynamically allocated data
        if (change->cellinfo) {
            int bytes = change->cellcount * sizeof(cell_change);
            newchange->cellinfo = (cell_change*) malloc(bytes);
            if (newchange->cellinfo == NULL) {
                Warning("Could not copy redolist!");
                ClearRedoHistory();
                return;
            }
            memcpy(newchange->cellinfo, change->cellinfo, bytes);
        }
        
        if (change->startinfo) {
            newchange->startinfo = new StartingInfo(change->startinfo, oldlayer, newlayer);
        }

        // if node is a name change then update whichlayer to point to new layer
        if (newchange->changeid == namechange) {
            if (change->whichlayer == oldlayer) {
                newchange->whichlayer = newlayer;
            } else {
                newchange->whichlayer = NULL;
            }
        }

        // copy any existing temporary files to new names
        if (!CopyTempFiles(change, newchange, newlayer->tempstart.c_str())) {
            Warning("Failed to copy temporary file in redolist!");
            ClearRedoHistory();
            return;
        }

        redolist.push_back(newchange);
        node++;
    }
}
