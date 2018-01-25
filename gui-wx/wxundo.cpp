// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
    #include "wx/wx.h"     // for all others include the necessary headers
#endif

#include "wx/filename.h"   // for wxFileName

#include "bigint.h"
#include "lifealgo.h"
#include "writepattern.h"  // for MC_format, XRLE_format

#include "wxgolly.h"       // for mainptr, viewptr
#include "wxmain.h"        // for mainptr->...
#include "wxselect.h"      // for Selection
#include "wxview.h"        // for viewptr->...
#include "wxutils.h"       // for Warning, Fatal
#include "wxscript.h"      // for inscript
#include "wxalgos.h"       // for algo_type
#include "wxlayer.h"       // for currlayer, numclones, MarkLayerDirty, etc
#include "wxprefs.h"       // for allowundo, GetAccelerator, etc
#include "wxundo.h"

// -----------------------------------------------------------------------------

const wxString lack_of_memory = _("Due to lack of memory, some changes can't be undone!");
const wxString to_gen = _("to Gen ");

// the following prefixes are used when creating temporary file names
// (only use 3 characters because longer strings are truncated on Windows)
const wxString genchange_prefix = wxT("gg_");
const wxString setgen_prefix = wxT("gs_");
const wxString dupe1_prefix = wxT("g1_");
const wxString dupe2_prefix = wxT("g2_");
const wxString dupe3_prefix = wxT("g3_");
const wxString dupe4_prefix = wxT("g4_");
const wxString dupe5_prefix = wxT("g5_");
const wxString dupe6_prefix = wxT("g6_");

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
    wxString savename;
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
    wxString saverule;
    
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
        if (count == 0) Warning(_("Bug detected in StartingInfo ctor!"));
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
    scriptstart,         // later changes were made by script
    scriptfinish,        // earlier changes were made by script
    
    // WARNING: code in UndoChange/RedoChange assumes only changes < selchange
    // can alter the layer's dirty state; ie. the olddirty/newdirty flags are
    // not used for all the following changes
    
    selchange,           // selection was changed
    genchange,           // pattern was generated
    setgen,              // generation count was changed
    rulechange,          // rule was changed
    algochange           // algorithm was changed
} change_type;

class ChangeNode: public wxObject {
public:
    ChangeNode(change_type id);
    ~ChangeNode();
    
    bool DoChange(bool undo);
    // do the undo/redo; if it returns false (eg. user has aborted a lengthy
    // rotate/flip operation) then cancel the undo/redo
    
    void ChangeCells(bool undo);
    // change cell states using cellinfo
    
    change_type changeid;                   // specifies the type of change
    wxString suffix;                        // action string for Undo/Redo item
    bool olddirty;                          // layer's dirty state before change
    bool newdirty;                          // layer's dirty state after change
    
    // cellstates info
    cell_change* cellinfo;                  // dynamic array of cell changes
    unsigned int cellcount;                 // number of cell changes in array
    
    // rotatecw/rotateacw/selchange info
    Selection oldsel, newsel;               // old and new selections
    
    // genchange info
    bool scriptgen;                         // gen change was done by script?
    wxString oldfile, newfile;              // old and new pattern files
    bigint oldgen, newgen;                  // old and new generation counts
    bigint oldx, oldy, newx, newy;          // old and new positions
    int oldmag, newmag;                     // old and new scales
    int oldbase, newbase;                   // old and new base steps
    int oldexpo, newexpo;                   // old and new step exponents
    StartingInfo* startinfo;                // saves starting info for ResetPattern
    // also uses oldsel, newsel
    // and oldtempstart, newtempstart
    
    // setgen info
    bigint oldstartgen, newstartgen;        // old and new startgen values
    bool oldsave, newsave;                  // old and new savestart states
    wxString oldtempstart, newtempstart;    // old and new tempstart paths
    wxString oldcurrfile, newcurrfile;      // old and new currfile paths
    // also uses oldgen, newgen
    // and startinfo
    
    // namechange info
    wxString oldname, newname;              // old and new layer names
    Layer* whichlayer;                      // which layer was changed
    // also uses oldsave, newsave
    // and oldcurrfile, newcurrfile
    
    // rulechange info
    wxString oldrule, newrule;              // old and new rules
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
    oldfile = wxEmptyString;
    newfile = wxEmptyString;
    oldtempstart = wxEmptyString;
    newtempstart = wxEmptyString;
    oldcurrfile = wxEmptyString;
    newcurrfile = wxEmptyString;
}

// -----------------------------------------------------------------------------

bool delete_all_temps = false;   // ok to delete all temporary files?

ChangeNode::~ChangeNode()
{
    if (startinfo) delete startinfo;
    if (cellinfo) free(cellinfo);
    
    // it's always ok to delete oldfile and newfile if they exist
    
    if (!oldfile.IsEmpty() && wxFileExists(oldfile)) {
        wxRemoveFile(oldfile);
    }
    
    if (!newfile.IsEmpty() && wxFileExists(newfile)) {
        wxRemoveFile(newfile);
    }

    if (delete_all_temps) {
        // we're in ClearUndoRedo so it's safe to delete oldtempstart/newtempstart/oldcurrfile/newcurrfile
        // if they are in tempdir and not being used to store the current layer's starting pattern
        // (the latter condition allows user to Reset after disabling undo/redo)
        
        if (!oldtempstart.IsEmpty() && wxFileExists(oldtempstart) &&
            oldtempstart.StartsWith(tempdir) && oldtempstart != currlayer->currfile) {
            wxRemoveFile(oldtempstart);
            //printf("removed oldtempstart: %s\n", (const char*)oldtempstart.mb_str(wxConvLocal)); fflush(stdout);
        }
        
        if (!newtempstart.IsEmpty() && wxFileExists(newtempstart) &&
            newtempstart.StartsWith(tempdir) && newtempstart != currlayer->currfile) {
            wxRemoveFile(newtempstart);
            //printf("removed newtempstart: %s\n", (const char*)newtempstart.mb_str(wxConvLocal)); fflush(stdout);
        }
        
        if (!oldcurrfile.IsEmpty() && wxFileExists(oldcurrfile) &&
            oldcurrfile.StartsWith(tempdir) && oldcurrfile != currlayer->currfile) {
            wxRemoveFile(oldcurrfile);
            //printf("removed oldcurrfile: %s\n", (const char*)oldcurrfile.mb_str(wxConvLocal)); fflush(stdout);
        }
        
        if (!newcurrfile.IsEmpty() && wxFileExists(newcurrfile) &&
            newcurrfile.StartsWith(tempdir) && newcurrfile != currlayer->currfile) {
            wxRemoveFile(newcurrfile);
            //printf("removed newcurrfile: %s\n", (const char*)newcurrfile.mb_str(wxConvLocal)); fflush(stdout);
        }
    }
}

// -----------------------------------------------------------------------------

void ChangeNode::ChangeCells(bool undo)
{
    // avoid possible pattern update during a setcell call (can happen if cellcount is large)
    viewptr->nopattupdate = true;

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
    
    viewptr->nopattupdate = false;
}

// -----------------------------------------------------------------------------

bool ChangeNode::DoChange(bool undo)
{
    switch (changeid) {
        case cellstates:
            if (cellcount > 0) {
                ChangeCells(undo);
                mainptr->UpdatePatternAndStatus();
            }
            break;
            
        case fliptb:
        case fliplr:
            // pass in true so FlipSelection won't save changes or call MarkLayerDirty
            if (!viewptr->FlipSelection(changeid == fliptb, true))
                return false;
            break;
            
        case rotatepattcw:
        case rotatepattacw:
            // pass in true so RotateSelection won't save changes or call MarkLayerDirty
            if (!viewptr->RotateSelection(changeid == rotatepattcw ? !undo : undo, true))
                return false;
            break;
            
        case rotatecw:
        case rotateacw:
            if (cellcount > 0) {
                ChangeCells(undo);
            }
            // rotate selection edges
            if (undo) {
                currlayer->currsel = oldsel;
            } else {
                currlayer->currsel = newsel;
            }
            viewptr->DisplaySelectionSize();
            mainptr->UpdatePatternAndStatus();
            break;
            
        case selchange:
            if (undo) {
                currlayer->currsel = oldsel;
            } else {
                currlayer->currsel = newsel;
            }
            if (viewptr->SelectionExists()) viewptr->DisplaySelectionSize();
            mainptr->UpdatePatternAndStatus();
            break;
            
        case genchange:
            currlayer->currfile = oldcurrfile;
            if (startinfo) {
                // restore starting info for use by ResetPattern
                startinfo->Restore();
            }
            if (undo) {
                currlayer->tempstart = oldtempstart;    // in case script called reset()
                currlayer->currsel = oldsel;
                mainptr->RestorePattern(oldgen, oldfile, oldx, oldy, oldmag, oldbase, oldexpo);
            } else {
                currlayer->tempstart = newtempstart;    // in case script called reset()
                currlayer->currsel = newsel;
                mainptr->RestorePattern(newgen, newfile, newx, newy, newmag, newbase, newexpo);
            }
            break;
            
        case setgen:
            if (undo) {
                mainptr->ChangeGenCount(oldgen.tostring(), true);
                currlayer->startgen = oldstartgen;
                currlayer->savestart = oldsave;
                currlayer->tempstart = oldtempstart;
                currlayer->currfile = oldcurrfile;
                if (startinfo) {
                    // restore starting info for use by ResetPattern
                    startinfo->Restore();
                }
            } else {
                mainptr->ChangeGenCount(newgen.tostring(), true);
                currlayer->startgen = newstartgen;
                currlayer->savestart = newsave;
                currlayer->tempstart = newtempstart;
                currlayer->currfile = newcurrfile;
            }
            // Reset item may become enabled/disabled
            mainptr->UpdateMenuItems();
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
                    if (olddirty == newdirty) mainptr->SetWindowTitle(currlayer->currname);
                    // if olddirty != newdirty then UndoChange/RedoChange will call
                    // MarkLayerClean/MarkLayerDirty (and they call SetWindowTitle)
                } else {
                    // whichlayer is non-active clone so only update Layer menu items
                    for (int i = 0; i < numlayers; i++)
                        mainptr->UpdateLayerItem(i);
                }
            }
            break;
            
        case rulechange:
            if (undo) {
                RestoreRule(oldrule);
                currlayer->currsel = oldsel;
            } else {
                RestoreRule(newrule);
                currlayer->currsel = newsel;
            }
            // show new rule in window title (file name doesn't change)
            mainptr->SetWindowTitle(wxEmptyString);
            if (cellcount > 0) {
                ChangeCells(undo);
            }
            // switch to default colors for new rule
            UpdateLayerColors();
            mainptr->UpdateEverything();
            break;
            
        case algochange:
            // pass in true so ChangeAlgorithm won't call RememberAlgoChange
            if (undo) {
                mainptr->ChangeAlgorithm(oldalgo, oldrule, true);
                currlayer->currsel = oldsel;
            } else {
                mainptr->ChangeAlgorithm(newalgo, newrule, true);
                currlayer->currsel = newsel;
            }
            // show new rule in window title (file name doesn't change)
            mainptr->SetWindowTitle(wxEmptyString);
            if (cellcount > 0) {
                ChangeCells(undo);
            }
            // ChangeAlgorithm has called UpdateLayerColors()
            mainptr->UpdateEverything();
            break;
            
        case scriptstart:
        case scriptfinish:
            // should never happen
            Warning(_("Bug detected in DoChange!"));
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
    prevfile = wxEmptyString;     // play safe for ClearUndoRedo
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
            cell_change* newptr = (cell_change*) realloc(cellarray, maxchanges * 2 * sizeof(cell_change));
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
            Warning(_("Bug detected in ForgetCellChanges!"));
        }
        numchanges = 0;      // reset for next SaveCellChange
        maxchanges = 0;      // ditto
        badalloc = false;
    }
}

// -----------------------------------------------------------------------------

bool UndoRedo::RememberCellChanges(const wxString& action, bool olddirty)
{
    if (numchanges > 0) {
        if (numchanges < maxchanges) {
            // reduce size of cellarray
            cell_change* newptr = (cell_change*) realloc(cellarray, numchanges * sizeof(cell_change));
            if (newptr != NULL) cellarray = newptr;
            // in the unlikely event that newptr is NULL, cellarray should
            // still point to valid data
        }
        
        // clear the redo history
        WX_CLEAR_LIST(wxList, redolist);
        UpdateRedoItem(wxEmptyString);
        
        // add cellstates node to head of undo list
        ChangeNode* change = new ChangeNode(cellstates);
        if (change == NULL) Fatal(_("Failed to create cellstates node!"));
        
        change->suffix = action;
        change->cellinfo = cellarray;
        change->cellcount = numchanges;
        change->olddirty = olddirty;
        change->newdirty = true;
        
        undolist.Insert(change);
        
        // update Undo item in Edit menu
        UpdateUndoItem(change->suffix);
        
        numchanges = 0;      // reset for next SaveCellChange
        maxchanges = 0;      // ditto
        
        if (badalloc) {
            Warning(lack_of_memory);
            badalloc = false;
        }
        return true;   // at least one cell changed state
    }
    return false;     // no cells changed state (SaveCellChange wasn't called)
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberFlip(bool topbot, bool olddirty)
{
    // clear the redo history
    WX_CLEAR_LIST(wxList, redolist);
    UpdateRedoItem(wxEmptyString);
    
    // add fliptb/fliplr node to head of undo list
    ChangeNode* change = new ChangeNode(topbot ? fliptb : fliplr);
    if (change == NULL) Fatal(_("Failed to create flip node!"));
    
    change->suffix = _("Flip");
    change->olddirty = olddirty;
    change->newdirty = true;
    
    undolist.Insert(change);
    
    // update Undo item in Edit menu
    UpdateUndoItem(change->suffix);
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberRotation(bool clockwise, bool olddirty)
{
    // clear the redo history
    WX_CLEAR_LIST(wxList, redolist);
    UpdateRedoItem(wxEmptyString);
    
    // add rotatepattcw/rotatepattacw node to head of undo list
    ChangeNode* change = new ChangeNode(clockwise ? rotatepattcw : rotatepattacw);
    if (change == NULL) Fatal(_("Failed to create simple rotation node!"));
    
    change->suffix = _("Rotation");
    change->olddirty = olddirty;
    change->newdirty = true;
    
    undolist.Insert(change);
    
    // update Undo item in Edit menu
    UpdateUndoItem(change->suffix);
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberRotation(bool clockwise, Selection& oldsel, Selection& newsel,
                                bool olddirty)
{
    // clear the redo history
    WX_CLEAR_LIST(wxList, redolist);
    UpdateRedoItem(wxEmptyString);
    
    // add rotatecw/rotateacw node to head of undo list
    ChangeNode* change = new ChangeNode(clockwise ? rotatecw : rotateacw);
    if (change == NULL) Fatal(_("Failed to create rotation node!"));
    
    change->suffix = _("Rotation");
    change->oldsel = oldsel;
    change->newsel = newsel;
    change->olddirty = olddirty;
    change->newdirty = true;
    
    // if numchanges == 0 we still need to rotate selection edges
    if (numchanges > 0) {
        if (numchanges < maxchanges) {
            // reduce size of cellarray
            cell_change* newptr = (cell_change*) realloc(cellarray, numchanges * sizeof(cell_change));
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
    
    undolist.Insert(change);
    
    // update Undo item in Edit menu
    UpdateUndoItem(change->suffix);
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberSelection(const wxString& action)
{
    if (currlayer->savesel == currlayer->currsel) {
        // selection has not changed
        return;
    }
    
    if (mainptr->generating) {
        // don't record selection changes while a pattern is generating;
        // RememberGenStart and RememberGenFinish will remember the overall change
        return;
    }
    
    // clear the redo history
    WX_CLEAR_LIST(wxList, redolist);
    UpdateRedoItem(wxEmptyString);
    
    // add selchange node to head of undo list
    ChangeNode* change = new ChangeNode(selchange);
    if (change == NULL) Fatal(_("Failed to create selchange node!"));
    
    if (currlayer->currsel.Exists()) {
        change->suffix = action;
    } else {
        change->suffix = _("Deselection");
    }
    change->oldsel = currlayer->savesel;
    change->newsel = currlayer->currsel;
    
    undolist.Insert(change);
    
    // update Undo item in Edit menu
    UpdateUndoItem(change->suffix);
}

// -----------------------------------------------------------------------------

void UndoRedo::SaveCurrentPattern(const wxString& tempfile)
{
    const char* err = NULL;
    if ( currlayer->algo->hyperCapable() ) {
        // save hlife pattern in a macrocell file
        err = mainptr->WritePattern(tempfile, MC_format, no_compression, 0, 0, 0, 0);
    } else {
        // can only save RLE file if edges are within getcell/setcell limits
        bigint top, left, bottom, right;
        currlayer->algo->findedges(&top, &left, &bottom, &right);      
        if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
            err = "Pattern is too big to save.";
        } else {
            // use XRLE format so the pattern's top left location and the current
            // generation count are stored in the file
            err = mainptr->WritePattern(tempfile, XRLE_format, no_compression,
                                        top.toint(), left.toint(),
                                        bottom.toint(), right.toint());
        }
    }
    if (err) Warning(wxString(err,wxConvLocal));
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberGenStart()
{
    startcount++;
    if (startcount > 1) {
        // return immediately and ignore next RememberGenFinish call
        // (this probably can't happen any more, but play safe)
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
    viewptr->GetPos(prevx, prevy);
    prevmag = viewptr->GetMag();
    prevbase = currlayer->currbase;
    prevexpo = currlayer->currexpo;
    
    if (!inscript) {
        // make sure Undo and Redo items show correct actions while generating
        UpdateUndoItem(to_gen + wxString(prevgen.tostring(), wxConvLocal));
        UpdateRedoItem(wxEmptyString);
    }
    
    if (prevgen == currlayer->startgen) {
        // we can just reset to starting pattern
        prevfile = wxEmptyString;
    } else {
        // save current pattern in a unique temporary file
        prevfile = wxFileName::CreateTempFileName(tempdir + genchange_prefix);
        
        // if head of undo list is a genchange node then we can copy that
        // change node's newfile to prevfile; this makes consecutive generating
        // runs faster (setting prevfile to newfile would be even faster but it's
        // difficult to avoid the file being deleted if the redo list is cleared)
        if (!undolist.IsEmpty()) {
            wxList::compatibility_iterator node = undolist.GetFirst();
            ChangeNode* change = (ChangeNode*) node->GetData();
            if (change->changeid == genchange) {
                if (wxCopyFile(change->newfile, prevfile, true)) {
                    return;
                } else {
                    Warning(_("Failed to copy temporary file!"));
                    // continue and call SaveCurrentPattern
                }
            }
        }
        
        SaveCurrentPattern(prevfile);
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
    
    // generation count might not have changed (can happen in Linux app)
    if (prevgen == currlayer->algo->getGeneration()) {
        // delete prevfile created by RememberGenStart
        if (!prevfile.IsEmpty() && wxFileExists(prevfile)) {
            wxRemoveFile(prevfile);
        }
        prevfile = wxEmptyString;
        return;
    }

    // currlayer->tempstart will need to change if script calls reset()
    wxString oldtempstart = currlayer->tempstart;
    
    wxString fpath;
    if (currlayer->algo->getGeneration() == currlayer->startgen) {
        // script called reset() so just use starting pattern
        fpath = wxEmptyString;

        // if script generates pattern then tempstart will be clobbered by
        // SaveStartingPattern, so change currlayer->tempstart to a new temporary file
        currlayer->tempstart = wxFileName::CreateTempFileName(tempdir + wxT("gr_"));

    } else {
        // save finishing pattern in a unique temporary file
        fpath = wxFileName::CreateTempFileName(tempdir + genchange_prefix);
        SaveCurrentPattern(fpath);
    }
    
    // clear the redo history
    WX_CLEAR_LIST(wxList, redolist);
    UpdateRedoItem(wxEmptyString);
    
    // add genchange node to head of undo list
    ChangeNode* change = new ChangeNode(genchange);
    if (change == NULL) Fatal(_("Failed to create genchange node!"));
    
    change->suffix = to_gen + wxString(prevgen.tostring(), wxConvLocal);
    change->scriptgen = inscript;
    change->oldgen = prevgen;
    change->newgen = currlayer->algo->getGeneration();
    change->oldfile = prevfile;
    change->newfile = fpath;
    change->oldx = prevx;
    change->oldy = prevy;
    viewptr->GetPos(change->newx, change->newy);
    change->oldmag = prevmag;
    change->newmag = viewptr->GetMag();
    change->oldbase = prevbase;
    change->newbase = currlayer->currbase;
    change->oldexpo = prevexpo;
    change->newexpo = currlayer->currexpo;
    change->oldsel = prevsel;
    change->newsel = currlayer->currsel;
    change->oldtempstart = oldtempstart;
    change->newtempstart = currlayer->tempstart;

    // also remember the file containing the starting pattern
    // (in case it is changed by by RememberSetGen or RememberNameChange)
    change->oldcurrfile = currlayer->currfile;

    if (change->oldgen == currlayer->startgen) {
        // save starting info set by recent SaveStartingPattern call
        // (the info will be restored when redoing this genchange node)
        change->startinfo = new StartingInfo(NULL, NULL, NULL);
    }
    
    // prevfile has been saved in change->oldfile (~ChangeNode will delete it)
    prevfile = wxEmptyString;
    
    undolist.Insert(change);
    
    // update Undo item in Edit menu
    UpdateUndoItem(change->suffix);
}

// -----------------------------------------------------------------------------

void UndoRedo::AddGenChange()
{
    // add a genchange node to empty undo list
    if (!undolist.IsEmpty())
        Warning(_("AddGenChange bug: undo list NOT empty!"));
    
    // use starting pattern info for previous state
    prevgen = currlayer->startgen;
    prevsel = currlayer->startsel;
    prevx = currlayer->startx;
    prevy = currlayer->starty;
    prevmag = currlayer->startmag;
    prevbase = currlayer->startbase;
    prevexpo = currlayer->startexpo;
    prevfile = wxEmptyString;
    
    // pretend RememberGenStart was called
    startcount = 1;
    
    // avoid RememberGenFinish returning early if inscript is true
    savegenchanges = false;
    RememberGenFinish();
    
    if (undolist.IsEmpty())
        Warning(_("AddGenChange bug: undo list is empty!"));
}

// -----------------------------------------------------------------------------

void UndoRedo::SyncUndoHistory()
{
    // reset startcount for the next RememberGenStart call
    startcount = 0;

    // synchronize undo history due to a ResetPattern call;
    // wind back the undo list to just past the genchange node that
    // matches the current layer's starting gen count
    wxList::compatibility_iterator node;
    ChangeNode* change;
    while (!undolist.IsEmpty()) {
        node = undolist.GetFirst();
        change = (ChangeNode*) node->GetData();
        
        // remove node from head of undo list and append it to redo list
        undolist.Erase(node);
        redolist.Insert(change);
        
        if (change->changeid == genchange && change->oldgen == currlayer->startgen) {
            if (change->scriptgen) {
                // gen change was done by a script so keep winding back the undo list
                // until the scriptstart node, or until the list is empty
                while (!undolist.IsEmpty()) {
                    node = undolist.GetFirst();
                    change = (ChangeNode*) node->GetData();
                    if (change->changeid == scriptstart) {
                        undolist.Erase(node);
                        redolist.Insert(change);
                        break;
                    }
                    // undo this change so Reset and Undo restore to the same pattern
                    UndoChange();
                }
            }
            // update Undo/Redo items so they show the correct suffix
            UpdateUndoRedoItems();
            return;
        }
    }
    
    // should never get here
    Warning(_("Bug detected in SyncUndoHistory!"));
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberSetGen(bigint& oldgen, bigint& newgen,
                              bigint& oldstartgen, bool oldsave)
{
    wxString oldtempstart = currlayer->tempstart;
    wxString oldcurrfile = currlayer->currfile;
    if (oldgen > oldstartgen && newgen <= oldstartgen) {
        // if pattern is generated then tempstart will be clobbered by
        // SaveStartingPattern, so change tempstart to a new temporary file
        currlayer->tempstart = wxFileName::CreateTempFileName(tempdir + setgen_prefix);
        
        // also need to update currfile (currlayer->savestart is true)
        if (!currlayer->savestart) Warning(_("Bug in RememberSetGen: savestart is false!"));
        currlayer->currfile = currlayer->tempstart;
    }
    
    // clear the redo history
    WX_CLEAR_LIST(wxList, redolist);
    UpdateRedoItem(wxEmptyString);
    
    // add setgen node to head of undo list
    ChangeNode* change = new ChangeNode(setgen);
    if (change == NULL) Fatal(_("Failed to create setgen node!"));
    
    change->suffix = _("Set Generation");
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
        
    undolist.Insert(change);
    
    // update Undo item in Edit menu
    UpdateUndoItem(change->suffix);
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberNameChange(const wxString& oldname, const wxString& oldcurrfile,
                                  bool oldsave, bool olddirty)
{
    if (oldname == currlayer->currname && oldcurrfile == currlayer->currfile &&
        oldsave == currlayer->savestart && olddirty == currlayer->dirty) return;
    
    // clear the redo history
    WX_CLEAR_LIST(wxList, redolist);
    UpdateRedoItem(wxEmptyString);
    
    // add namechange node to head of undo list
    ChangeNode* change = new ChangeNode(namechange);
    if (change == NULL) Fatal(_("Failed to create namechange node!"));
    
    change->suffix = _("Name Change");
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
    
    undolist.Insert(change);
    
    // update Undo item in Edit menu
    UpdateUndoItem(change->suffix);
}

// -----------------------------------------------------------------------------

void UndoRedo::DeletingClone(int index)
{
    // the given cloned layer is about to be deleted, so we need to go thru the
    // undo/redo lists and fix up any nodes that have pointers to that clone
    // (very ugly, but I don't see any better solution if we're going to allow
    // cloned layers to have different names)
    
    Layer* cloneptr = GetLayer(index);
    wxList::compatibility_iterator node;
    
    node = undolist.GetFirst();
    while (node) {
        ChangeNode* change = (ChangeNode*) node->GetData();
        if (change->whichlayer == cloneptr) {
            change->whichlayer = NULL;
        }
        if (change->startinfo) {
            change->startinfo->RemoveClone(cloneptr);
        }
        node = node->GetNext();
    }
    
    node = redolist.GetFirst();
    while (node) {
        ChangeNode* change = (ChangeNode*) node->GetData();
        if (change->whichlayer == cloneptr) {
            change->whichlayer = NULL;
        }
        if (change->startinfo) {
            change->startinfo->RemoveClone(cloneptr);
        }
        node = node->GetNext();
    }
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberRuleChange(const wxString& oldrule)
{
    wxString newrule = wxString(currlayer->algo->getrule(), wxConvLocal);
    if (oldrule == newrule) return;
    
    // clear the redo history
    WX_CLEAR_LIST(wxList, redolist);
    UpdateRedoItem(wxEmptyString);
    
    // add rulechange node to head of undo list
    ChangeNode* change = new ChangeNode(rulechange);
    if (change == NULL) Fatal(_("Failed to create rulechange node!"));
    
    change->suffix = _("Rule Change");
    change->oldrule = oldrule;
    change->newrule = newrule;
    
    // selection might have changed if grid became smaller
    change->oldsel = currlayer->savesel;
    change->newsel = currlayer->currsel;
    
    // SaveCellChange may have been called
    if (numchanges > 0) {
        if (numchanges < maxchanges) {
            // reduce size of cellarray
            cell_change* newptr = (cell_change*) realloc(cellarray, numchanges * sizeof(cell_change));
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
    
    undolist.Insert(change);
    
    // update Undo item in Edit menu
    UpdateUndoItem(change->suffix);
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberAlgoChange(algo_type oldalgo, const wxString& oldrule)
{
    // clear the redo history
    WX_CLEAR_LIST(wxList, redolist);
    UpdateRedoItem(wxEmptyString);
    
    // add algochange node to head of undo list
    ChangeNode* change = new ChangeNode(algochange);
    if (change == NULL) Fatal(_("Failed to create algochange node!"));
    
    change->suffix = _("Algorithm Change");
    change->oldalgo = oldalgo;
    change->newalgo = currlayer->algtype;
    change->oldrule = oldrule;
    change->newrule = wxString(currlayer->algo->getrule(), wxConvLocal);
    
    // selection might have changed if grid became smaller
    change->oldsel = currlayer->savesel;
    change->newsel = currlayer->currsel;
    
    // SaveCellChange may have been called
    if (numchanges > 0) {
        if (numchanges < maxchanges) {
            // reduce size of cellarray
            cell_change* newptr = (cell_change*) realloc(cellarray, numchanges * sizeof(cell_change));
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
    
    undolist.Insert(change);
    
    // update Undo item in Edit menu
    UpdateUndoItem(change->suffix);
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberScriptStart()
{
    if (!undolist.IsEmpty()) {
        wxList::compatibility_iterator node = undolist.GetFirst();
        ChangeNode* change = (ChangeNode*) node->GetData();
        if (change->changeid == scriptstart) {
            // ignore consecutive RememberScriptStart calls made by RunScript
            // due to cloned layers
            if (numclones == 0) Warning(_("Unexpected RememberScriptStart call!"));
            return;
        }
    }
    
    // add scriptstart node to head of undo list
    ChangeNode* change = new ChangeNode(scriptstart);
    if (change == NULL) Fatal(_("Failed to create scriptstart node!"));
    
    change->suffix = _("Script Changes");
    
    // remmember dirty flag at start of script
    change->olddirty = currlayer->dirty;
    
    undolist.Insert(change);
    
    // update Undo action and clear Redo action
    UpdateUndoItem(change->suffix);
    UpdateRedoItem(wxEmptyString);
}

// -----------------------------------------------------------------------------

void UndoRedo::RememberScriptFinish()
{
    if (undolist.IsEmpty()) {
        // this can happen if RunScript calls RememberScriptFinish multiple times
        // due to cloned layers AND the script made no changes
        if (numclones == 0) {
            // there should be at least a scriptstart node (see ClearUndoRedo)
            Warning(_("Bug detected in RememberScriptFinish!"));
        }
        return;
    }
    
    // if head of undo list is a scriptstart node then simply remove it
    // and return (ie. the script didn't make any changes)
    wxList::compatibility_iterator node = undolist.GetFirst();
    ChangeNode* change = (ChangeNode*) node->GetData();
    if (change->changeid == scriptstart) {
        undolist.Erase(node);
        delete change;
        return;
    } else if (change->changeid == scriptfinish) {
        // ignore consecutive RememberScriptFinish calls made by RunScript
        // due to cloned layers
        if (numclones == 0) Warning(_("Unexpected RememberScriptFinish call!"));
        return;
    }
    
    // add scriptfinish node to head of undo list
    change = new ChangeNode(scriptfinish);
    if (change == NULL) Fatal(_("Failed to create scriptfinish node!"));
    
    change->suffix = _("Script Changes");
    
    // remmember dirty flag at end of script
    change->newdirty = currlayer->dirty;
    
    undolist.Insert(change);
    
    // update Undo item in Edit menu
    UpdateUndoItem(change->suffix);
}

// -----------------------------------------------------------------------------

bool UndoRedo::CanUndo()
{
    // we need to allow undo if generating even though undo list might be empty
    // (selecting Undo will stop generating and add genchange node to undo list)
    if (allowundo && mainptr->generating) return true;
    
    return !undolist.IsEmpty() && !inscript &&
           !viewptr->waitingforclick && !viewptr->drawingcells &&
           !viewptr->selectingcells;
}

// -----------------------------------------------------------------------------

bool UndoRedo::CanRedo()
{
    return !redolist.IsEmpty() && !inscript && !mainptr->generating &&
           !viewptr->waitingforclick && !viewptr->drawingcells &&
           !viewptr->selectingcells;
}

// -----------------------------------------------------------------------------

static void ChangeDirtyFlag(bool newdirty)
{
    // change dirty flag to newdirty, update window title and Layer menu items
    if (newdirty) {
        currlayer->dirty = false;   // make sure it changes to true
        MarkLayerDirty();
    } else {
        MarkLayerClean(currlayer->currname);
    }
}

// -----------------------------------------------------------------------------

void UndoRedo::UndoChange()
{
    if (!CanUndo()) return;
    
    if (mainptr->generating) {
        mainptr->command_pending = true;
        mainptr->cmdevent.SetId(ID_UNDO);
        mainptr->Stop();
        return;
    }
    
    // prevent re-entrancy if DoChange calls checkevents
    if (insideYield) return;
    
    // get change info from head of undo list and do the change
    wxList::compatibility_iterator node = undolist.GetFirst();
    ChangeNode* change = (ChangeNode*) node->GetData();
    
    if (change->changeid == scriptfinish) {
        // undo all changes between scriptfinish and scriptstart nodes;
        // first remove scriptfinish node from undo list and add it to redo list
        undolist.Erase(node);
        redolist.Insert(change);
        
        bool dirty_at_end = change->newdirty;
        
        while (change->changeid != scriptstart) {
            // call UndoChange recursively; temporarily set doingscriptchanges so
            // 1) UndoChange won't return if DoChange is aborted
            // 2) user won't see any intermediate pattern/status updates
            // 3) Undo/Redo items won't be updated
            doingscriptchanges = true;
            UndoChange();
            doingscriptchanges = false;
            node = undolist.GetFirst();
            if (node == NULL) Fatal(_("Bug in UndoChange!"));
            change = (ChangeNode*) node->GetData();
        }
        
        // update dirty flag if it was changed by script
        if (change->olddirty != dirty_at_end) {
            ChangeDirtyFlag(change->olddirty);
        }
        
        mainptr->UpdatePatternAndStatus();
        // continue below so that scriptstart node is removed from undo list
        // and added to redo list
        
    } else {
        // user might abort the undo (eg. a lengthy rotate/flip)
        if (!change->DoChange(true) && !doingscriptchanges) return;
        
        if (!doingscriptchanges && change->changeid < selchange && change->olddirty != change->newdirty) {
            ChangeDirtyFlag(change->olddirty);
        }
    }
    
    // remove node from head of undo list (doesn't delete node's data)
    undolist.Erase(node);
    
    // add change to head of redo list
    redolist.Insert(change);
    
    // update Undo/Redo items in Edit menu
    UpdateUndoRedoItems();
}

// -----------------------------------------------------------------------------

void UndoRedo::RedoChange()
{
    if (!CanRedo()) return;
    
    /* can't redo while generating -- redo list will be empty
    if (mainptr->generating) {
        mainptr->command_pending = true;
        mainptr->cmdevent.SetId(ID_REDO);
        mainptr->Stop();
        return;
    }
    */
    
    // prevent re-entrancy if DoChange calls checkevents
    if (insideYield) return;
    
    // get change info from head of redo list and do the change
    wxList::compatibility_iterator node = redolist.GetFirst();
    ChangeNode* change = (ChangeNode*) node->GetData();
    
    if (change->changeid == scriptstart) {
        // redo all changes between scriptstart and scriptfinish nodes;
        // first remove scriptstart node from redo list and add it to undo list
        redolist.Erase(node);
        undolist.Insert(change);
        
        bool dirty_at_start = change->olddirty;
        
        while (change->changeid != scriptfinish) {
            // call RedoChange recursively; temporarily set doingscriptchanges so
            // 1) RedoChange won't return if DoChange is aborted
            // 2) user won't see any intermediate pattern/status updates
            // 3) Undo/Redo items won't be updated
            doingscriptchanges = true;
            RedoChange();
            doingscriptchanges = false;
            node = redolist.GetFirst();
            if (node == NULL) Fatal(_("Bug in RedoChange!"));
            change = (ChangeNode*) node->GetData();
        }
        
        // update dirty flag if it was changed by script
        if (change->newdirty != dirty_at_start) {
            ChangeDirtyFlag(change->newdirty);
        }
        
        mainptr->UpdatePatternAndStatus();
        // continue below so that scriptfinish node is removed from redo list
        // and added to undo list
        
    } else {
        // user might abort the redo (eg. a lengthy rotate/flip)
        if (!change->DoChange(false) && !doingscriptchanges) return;
        
        if (!doingscriptchanges && change->changeid < selchange && change->olddirty != change->newdirty) {
            ChangeDirtyFlag(change->newdirty);
        }
    }
    
    // remove node from head of redo list (doesn't delete node's data)
    redolist.Erase(node);
    
    // add change to head of undo list
    undolist.Insert(change);
    
    // update Undo/Redo items in Edit menu
    UpdateUndoRedoItems();
}

// -----------------------------------------------------------------------------

void UndoRedo::UpdateUndoRedoItems()
{
    if (inscript) return;   // update Undo/Redo items at end of script
    
    if (doingscriptchanges) return;
    
    if (undolist.IsEmpty()) {
        UpdateUndoItem(wxEmptyString);
    } else {
        wxList::compatibility_iterator node = undolist.GetFirst();
        ChangeNode* change = (ChangeNode*) node->GetData();
        if (change->changeid == genchange) {
            change->suffix = to_gen + wxString(change->oldgen.tostring(), wxConvLocal);
        }
        UpdateUndoItem(change->suffix);
    }
    
    if (redolist.IsEmpty()) {
        UpdateRedoItem(wxEmptyString);
    } else {
        wxList::compatibility_iterator node = redolist.GetFirst();
        ChangeNode* change = (ChangeNode*) node->GetData();
        if (change->changeid == genchange) {
            change->suffix = to_gen + wxString(change->newgen.tostring(), wxConvLocal);
        }
        UpdateRedoItem(change->suffix);
    }
}

// -----------------------------------------------------------------------------

void UndoRedo::UpdateUndoItem(const wxString& action)
{
    if (inscript) return;   // update Undo/Redo items at end of script
    
    wxMenuBar* mbar = mainptr->GetMenuBar();
    if (mbar) {
        wxString label = _("Undo ");
        label += action;
        label += GetAccelerator(DO_UNDO);
        mbar->SetLabel(ID_UNDO, label);
    }
}

// -----------------------------------------------------------------------------

void UndoRedo::UpdateRedoItem(const wxString& action)
{
    if (inscript) return;   // update Undo/Redo items at end of script
    
    wxMenuBar* mbar = mainptr->GetMenuBar();
    if (mbar) {
        wxString label = _("Redo ");
        label += action;
        label += GetAccelerator(DO_REDO);
        mbar->SetLabel(ID_REDO, label);
    }
}

// -----------------------------------------------------------------------------

void UndoRedo::ClearUndoRedo()
{
    // free cellarray in case there were SaveCellChange calls not followed
    // by ForgetCellChanges or RememberCellChanges
    ForgetCellChanges();
    
    if (startcount > 0) {
        // RememberGenStart was not followed by RememberGenFinish
        if (!prevfile.IsEmpty() && wxFileExists(prevfile)) {
            wxRemoveFile(prevfile);
        }
        prevfile = wxEmptyString;
        startcount = 0;
    }
    
    // set flag so ChangeNode::~ChangeNode() can delete all temporary files
    delete_all_temps = true;

    // clear the undo/redo lists (and delete each node's data)
    WX_CLEAR_LIST(wxList, undolist);
    WX_CLEAR_LIST(wxList, redolist);
    
    delete_all_temps = false;
    
    if (inscript) {
        // script has called a command like new() so add a scriptstart node
        // to the undo list to match the final scriptfinish node
        RememberScriptStart();
        // reset flags to indicate no pending cell/gen changes
        savecellchanges = false;
        savegenchanges = false;
    } else {
        UpdateUndoItem(wxEmptyString);
        UpdateRedoItem(wxEmptyString);
    }
}

// -----------------------------------------------------------------------------

bool CopyTempFiles(ChangeNode* srcnode, ChangeNode* destnode, const wxString& tempstart1)
{
    // if srcnode has any existing temporary files then, if necessary, create new
    // temporary file names in the destnode and copy each file
    bool allcopied = true;
    
    if ( !srcnode->oldfile.IsEmpty() && wxFileExists(srcnode->oldfile) ) {
        destnode->oldfile = wxFileName::CreateTempFileName(tempdir + dupe1_prefix);
        if ( !wxCopyFile(srcnode->oldfile, destnode->oldfile, true) )
            allcopied = false;
    }
    
    if ( !srcnode->newfile.IsEmpty() && wxFileExists(srcnode->newfile) ) {
        destnode->newfile = wxFileName::CreateTempFileName(tempdir + dupe2_prefix);
        if ( !wxCopyFile(srcnode->newfile, destnode->newfile, true) )
            allcopied = false;
    }
    
    if ( !srcnode->oldcurrfile.IsEmpty() && wxFileExists(srcnode->oldcurrfile) ) {
        if (srcnode->oldcurrfile == currlayer->tempstart) {
            // the file has already been copied to tempstart1 by Layer::Layer()
            destnode->oldcurrfile = tempstart1;
        } else if (srcnode->oldcurrfile.StartsWith(tempdir)) {
            destnode->oldcurrfile = wxFileName::CreateTempFileName(tempdir + dupe3_prefix);
            if ( !wxCopyFile(srcnode->oldcurrfile, destnode->oldcurrfile, true) )
                allcopied = false;
        }
    }
    
    if ( !srcnode->newcurrfile.IsEmpty() && wxFileExists(srcnode->newcurrfile) ) {
        if (srcnode->newcurrfile == srcnode->oldcurrfile) {
            // use destnode->oldcurrfile set above or earlier in DuplicateHistory
            destnode->newcurrfile = destnode->oldcurrfile;
        } else if (srcnode->newcurrfile == currlayer->tempstart) {
            // the file has already been copied to tempstart1 by Layer::Layer()
            destnode->newcurrfile = tempstart1;
        } else if (srcnode->newcurrfile.StartsWith(tempdir)) {
            destnode->newcurrfile = wxFileName::CreateTempFileName(tempdir + dupe4_prefix);
            if ( !wxCopyFile(srcnode->newcurrfile, destnode->newcurrfile, true) )
                allcopied = false;
        }
    }
    
    if ( !srcnode->oldtempstart.IsEmpty() && wxFileExists(srcnode->oldtempstart) ) {
        if (srcnode->oldtempstart == currlayer->tempstart) {
            // the file has already been copied to tempstart1 by Layer::Layer()
            destnode->oldtempstart = tempstart1;
        } else if (srcnode->oldtempstart.StartsWith(tempdir)) {
            destnode->oldtempstart = wxFileName::CreateTempFileName(tempdir + dupe5_prefix);
            if ( !wxCopyFile(srcnode->oldtempstart, destnode->oldtempstart, true) )
                allcopied = false;
        }
    }
    
    if ( !srcnode->newtempstart.IsEmpty() && wxFileExists(srcnode->newtempstart) ) {
        if (srcnode->newtempstart == srcnode->oldtempstart) {
            // use destnode->oldtempstart set above or earlier in DuplicateHistory
            destnode->newtempstart = destnode->oldtempstart;
        } else if (srcnode->newtempstart == currlayer->tempstart) {
            // the file has already been copied to tempstart1 by Layer::Layer()
            destnode->newtempstart = tempstart1;
        } else if (srcnode->newtempstart.StartsWith(tempdir)) {
            destnode->newtempstart = wxFileName::CreateTempFileName(tempdir + dupe6_prefix);
            if ( !wxCopyFile(srcnode->newtempstart, destnode->newtempstart, true) )
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
    WX_CLEAR_LIST(wxList, undolist);
    WX_CLEAR_LIST(wxList, redolist);    // should already be empty but play safe
    
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
    if ( !prevfile.IsEmpty() && wxFileExists(prevfile) ) {
        prevfile = wxFileName::CreateTempFileName(tempdir + genchange_prefix);
        if ( !wxCopyFile(history->prevfile, prevfile, true) ) {
            Warning(_("Could not copy prevfile!"));
            return;
        }
    }
    
    // do a deep copy of dynamically allocated data
    cellarray = NULL;
    if (numchanges > 0 && history->cellarray) {
        cellarray = (cell_change*) malloc(maxchanges * sizeof(cell_change));
        if (cellarray == NULL) {
            Warning(_("Could not allocate cellarray!"));
            return;
        }
        // copy history->cellarray data to this cellarray
        memcpy(cellarray, history->cellarray, numchanges * sizeof(cell_change));
    } 
    
    wxList::compatibility_iterator node;
    
    // build a new undolist using history->undolist
    node = history->undolist.GetFirst();
    while (node) {
        ChangeNode* change = (ChangeNode*) node->GetData();
        
        ChangeNode* newchange = new ChangeNode(change->changeid);
        if (newchange == NULL) {
            Warning(_("Failed to copy undolist!"));
            WX_CLEAR_LIST(wxList, undolist);
            return;
        }
        
        // shallow copy the change node
        *newchange = *change;
        
        // deep copy any dynamically allocated data
        if (change->cellinfo) {
            int bytes = change->cellcount * sizeof(cell_change);
            newchange->cellinfo = (cell_change*) malloc(bytes);
            if (newchange->cellinfo == NULL) {
                Warning(_("Could not copy undolist!"));
                WX_CLEAR_LIST(wxList, undolist);
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
        if (!CopyTempFiles(change, newchange, newlayer->tempstart)) {
            Warning(_("Failed to copy temporary file in undolist!"));
            WX_CLEAR_LIST(wxList, undolist);
            return;
        }
        
        undolist.Append(newchange);
        node = node->GetNext();
    }
    
    // build a new redolist using history->redolist
    node = history->redolist.GetFirst();
    while (node) {
        ChangeNode* change = (ChangeNode*) node->GetData();
        
        ChangeNode* newchange = new ChangeNode(change->changeid);
        if (newchange == NULL) {
            Warning(_("Failed to copy redolist!"));
            WX_CLEAR_LIST(wxList, redolist);
            return;
        }
        
        // shallow copy the change node
        *newchange = *change;
        
        // deep copy any dynamically allocated data
        if (change->cellinfo) {
            int bytes = change->cellcount * sizeof(cell_change);
            newchange->cellinfo = (cell_change*) malloc(bytes);
            if (newchange->cellinfo == NULL) {
                Warning(_("Could not copy redolist!"));
                WX_CLEAR_LIST(wxList, redolist);
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
        if (!CopyTempFiles(change, newchange, newlayer->tempstart)) {
            Warning(_("Failed to copy temporary file in redolist!"));
            WX_CLEAR_LIST(wxList, redolist);
            return;
        }
        
        redolist.Append(newchange);
        node = node->GetNext();
    }
}
