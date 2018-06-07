// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include <string.h>        // for strlen and strcpy

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
    #include "wx/wx.h"     // for all others include the necessary headers
#endif

#include <limits.h>        // for INT_MAX
#include "wx/filename.h"   // for wxFileName

#include "wxgolly.h"       // for wxGetApp, mainptr, viewptr, statusptr
#include "wxmain.h"        // for mainptr->...
#include "wxselect.h"      // for Selection
#include "wxedit.h"        // for ToggleEditBar, ToggleAllStates, UpdateEditBar
#include "wxview.h"        // for viewptr->...
#include "wxstatus.h"      // for statusptr->...
#include "wxutils.h"       // for Warning, Beep, IsScriptFile
#include "wxprefs.h"       // for gollydir, allowundo, etc
#include "wxundo.h"        // for undoredo->...
#include "wxalgos.h"       // for *_ALGO, algoinfo
#include "wxlayer.h"       // for currlayer, SyncClones
#include "wxtimeline.h"    // for TimelineExists, ToggleTimelineBar
#include "wxlua.h"         // for RunLuaScript, AbortLuaScript
#include "wxperl.h"        // for RunPerlScript, AbortPerlScript
#include "wxpython.h"      // for RunPythonScript, AbortPythonScript
#include "wxoverlay.h"     // for curroverlay
#include "wxscript.h"

// =============================================================================

// exported globals:
bool inscript = false;     // a script is running?
bool pass_key_events;      // pass keyboard events to script?
bool pass_mouse_events;    // pass mouse events to script?
bool pass_file_events;     // pass file events to script?
bool canswitch;            // can user switch layers while script is running?
bool stop_after_script;    // stop generating pattern after running script?
bool autoupdate;           // update display after each change to current universe?
bool allowcheck;           // allow event checking?
bool showprogress;         // script can display the progress dialog?
wxString scripterr;        // Lua/Perl/Python error message
wxString mousepos;         // current mouse position
wxString scripttitle;      // window title set by settitle command
wxString rle3path;         // path of .rle3 file to be sent to 3D.lua via GSF_getevent

// local globals:
static bool luascript = false;      // a Lua script is running?
static bool plscript = false;       // a Perl script is running?
static bool pyscript = false;       // a Python script is running?
static bool showtitle;              // need to update window title?
static bool updateedit;             // need to update edit bar?
static bool exitcalled;             // GSF_exit was called?
static wxString scriptchars;        // non-escape chars saved by PassKeyToScript
static wxString scriptloc;          // location of script file
static wxArrayString eventqueue;    // FIFO queue for keyboard/mouse events

// constants:
const int maxcomments = 128 * 1024; // maximum comment size

// -----------------------------------------------------------------------------

void DoAutoUpdate()
{
    if (autoupdate) {
        inscript = false;
        mainptr->UpdatePatternAndStatus(true);  // call Update()
        if (showtitle) {
            mainptr->SetWindowTitle(wxEmptyString);
            showtitle = false;
        }
        inscript = true;

        #ifdef __WXGTK__
            // needed on Linux to see update immediately
            insideYield = true;
            wxGetApp().Yield(true);
            insideYield = false;
        #endif
    }
}

// -----------------------------------------------------------------------------

void ShowTitleLater()
{
    // called from SetWindowTitle when inscript is true;
    // show title at next update (or at end of script)
    showtitle = true;
}

// -----------------------------------------------------------------------------

void ChangeWindowTitle(const wxString& name)
{
    if (autoupdate) {
        // update title bar right now
        inscript = false;
        mainptr->SetWindowTitle(name);
        inscript = true;
        showtitle = false;       // update has been done
    } else {
        // show it later but must still update currlayer->currname and menu item
        mainptr->SetWindowTitle(name);
        // showtitle is now true
    }
}

// =============================================================================

// The following Golly Script Functions are used to reduce code duplication.
// They are called by corresponding pl_* and py_* functions in wxperl.cpp
// and wxpython.cpp respectively.

const char* GSF_open(const wxString& filename, int remember)
{
    // convert non-absolute filename to absolute path relative to scriptloc
    wxFileName fullname(filename);
    if (!fullname.IsAbsolute()) fullname = scriptloc + filename;
    
    // return error message here if file doesn't exist
    wxString fullpath = fullname.GetFullPath();
    if (!wxFileName::FileExists(fullpath)) {
        return "open error: given file does not exist.";
    }
    
    // temporarily set pass_file_events to false so OpenFile won't pass
    // a file event back to this script
    bool savepass = pass_file_events;
    pass_file_events = false;
    
    // only add file to Open Recent submenu if remember flag is non-zero
    mainptr->OpenFile(fullpath, remember != 0);
    DoAutoUpdate();
    
    // restore pass_file_events
    pass_file_events = savepass;
    
    return NULL;
}

// -----------------------------------------------------------------------------

const char* GSF_save(const wxString& filename, const char* format, int remember)
{
    // convert non-absolute filename to absolute path relative to scriptloc
    wxFileName fullname(filename);
    if (!fullname.IsAbsolute()) fullname = scriptloc + filename;
    
    // only add file to Open Recent submenu if remember flag is non-zero
    return mainptr->SaveFile(fullname.GetFullPath(), wxString(format,wxConvLocal), remember != 0);
}

// -----------------------------------------------------------------------------

const char* GSF_setdir(const char* dirname, const wxString& newdir)
{
    wxString dirpath = newdir;
    if (dirpath.Last() != wxFILE_SEP_PATH) dirpath += wxFILE_SEP_PATH;
    if (!wxFileName::DirExists(dirpath)) {
        return "New directory does not exist.";
    }
    
    if (strcmp(dirname, "app") == 0) {
        return "Application directory cannot be changed.";
        
    } else if (strcmp(dirname, "data") == 0) {
        return "Data directory cannot be changed.";
        
    } else if (strcmp(dirname, "temp") == 0) {
        return "Temporary directory cannot be changed.";
        
    } else if (strcmp(dirname, "rules") == 0) {
        userrules = dirpath;
        
    } else if (strcmp(dirname, "files") == 0 ||
               strcmp(dirname, "patterns") == 0) {  // deprecated
        // change filedir and update panel if currently shown
        mainptr->SetFileDir(dirpath);
        
    } else if (strcmp(dirname, "download") == 0) {
        downloaddir = dirpath;
        
    } else {
        return "Unknown directory name.";
    }
    
    return NULL;   // success
}

// -----------------------------------------------------------------------------

const char* GSF_getdir(const char* dirname)
{
    wxString dirpath;
    
    if      (strcmp(dirname, "app") == 0)        dirpath = gollydir;
    else if (strcmp(dirname, "data") == 0)       dirpath = datadir;
    else if (strcmp(dirname, "temp") == 0)       dirpath = tempdir;
    else if (strcmp(dirname, "rules") == 0)      dirpath = userrules;
    else if (strcmp(dirname, "files") == 0)      dirpath = filedir;
    else if (strcmp(dirname, "patterns") == 0)   dirpath = filedir;         // deprecated
    else if (strcmp(dirname, "scripts") == 0)    dirpath = filedir;         // ditto
    else if (strcmp(dirname, "download") == 0)   dirpath = downloaddir;
    else {
        return NULL;   // unknown directory name
    }
    
    // make sure directory path ends with separator
    if (dirpath.Last() != wxFILE_SEP_PATH) dirpath += wxFILE_SEP_PATH;
    
    // need to be careful converting Unicode wxString to char*
    static wxCharBuffer dirbuff;
    #ifdef __WXMAC__
        // we need to convert dirpath to decomposed UTF8 so fopen will work
        dirbuff = dirpath.fn_str();
    #else
        dirbuff = dirpath.mb_str(wxConvLocal);
    #endif
    return (const char*) dirbuff;
}

// -----------------------------------------------------------------------------

const char* GSF_os()
{
    // return a string that specifies the current operating system
    #if defined(__WXMSW__)
        return "Windows";
    #elif defined(__WXMAC__)
        return "Mac";
    #elif defined(__WXGTK__)
        return "Linux";
    #else 
        return "unknown";
    #endif
}

// -----------------------------------------------------------------------------

const char* GSF_setalgo(const char* algostring)
{
    // find index for given algo name
    char* algoname = ReplaceDeprecatedAlgo((char*) algostring);
    algo_type algoindex = -1;
    for (int i = 0; i < NumAlgos(); i++) {
        if (strcmp(algoname, GetAlgoName(i)) == 0) {
            algoindex = i;
            break;
        }
    }
    if (algoindex < 0) return "Unknown algorithm.";
    
    if (algoindex != currlayer->algtype) {
        mainptr->ChangeAlgorithm(algoindex);
        if (algoindex != currlayer->algtype) {
            return "Algorithm could not be changed (pattern is too big to convert).";
        } else {
            // rule might have changed
            ChangeWindowTitle(wxEmptyString);
            // pattern might have changed or colors might have changed
            DoAutoUpdate();
        }
    }
    
    return NULL;
}

// -----------------------------------------------------------------------------

const char* GSF_setrule(const char* rulestring)
{
    wxString oldrule = wxString(currlayer->algo->getrule(),wxConvLocal);
    int oldmaxstate = currlayer->algo->NumCellStates() - 1;
    
    // selection might change if grid becomes smaller,
    // so save current selection for RememberRuleChange/RememberAlgoChange
    viewptr->SaveCurrentSelection();
    
    // inscript should be true but play safe
    if (allowundo && !currlayer->stayclean && inscript) {
        // note that we must save pending gen changes BEFORE changing rule
        // otherwise temporary files will store incorrect rule info
        SavePendingChanges();
    }
    
    const char* err;
    if (rulestring == NULL || rulestring[0] == 0) {
        // set normal Life
        err = currlayer->algo->setrule("B3/S23");
    } else {
        err = currlayer->algo->setrule(rulestring);
    }
    if (err) {
        // try to find another algorithm that supports the new rule
        for (int i = 0; i < NumAlgos(); i++) {
            if (i != currlayer->algtype) {
                lifealgo* tempalgo = CreateNewUniverse(i);
                err = tempalgo->setrule(rulestring);
                delete tempalgo;
                if (!err) {
                    // change the current algorithm and switch to the new rule
                    mainptr->ChangeAlgorithm(i, wxString(rulestring,wxConvLocal));
                    if (i != currlayer->algtype) {
                        RestoreRule(oldrule);
                        return "Algorithm could not be changed (pattern is too big to convert).";
                    } else {
                        ChangeWindowTitle(wxEmptyString);
                        DoAutoUpdate();
                        return NULL;
                    }
                }
            }
        }
        RestoreRule(oldrule);
        return "Given rule is not valid in any algorithm.";
    }
    
    // check if the rule string changed, or the number of states changed
    // (the latter might happen if user edited a table/tree file)
    wxString newrule = wxString(currlayer->algo->getrule(),wxConvLocal);
    int newmaxstate = currlayer->algo->NumCellStates() - 1;
    if (oldrule != newrule || oldmaxstate != newmaxstate) {
        // show new rule in main window's title but don't change name
        ChangeWindowTitle(wxEmptyString);
        
		// if pattern exists and is at starting gen then ensure savestart is true
		// so that SaveStartingPattern will save pattern to suitable file
		// (and thus undo/reset will work correctly)
		if (currlayer->algo->getGeneration() == currlayer->startgen && !currlayer->algo->isEmpty()) {
			currlayer->savestart = true;
		}
        
        // if grid is bounded then remove any live cells outside grid edges
        if (currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0) {
            mainptr->ClearOutsideGrid();
        }
        
        // rule change might have changed the number of cell states;
        // if there are fewer states then pattern might change
        if (newmaxstate < oldmaxstate && !currlayer->algo->isEmpty()) {
            mainptr->ReduceCellStates(newmaxstate);
        }
        
        if (allowundo && !currlayer->stayclean) {
            currlayer->undoredo->RememberRuleChange(oldrule);
        }
    }
    
    // switch to default colors and icons for new rule (we need to do this even if
    // oldrule == newrule in case there's a new/changed .colors or .icons file)
    UpdateLayerColors();
    
    // pattern or colors or icons might have changed
    DoAutoUpdate();
    
    return NULL;
}

// -----------------------------------------------------------------------------

const char* GSF_setgen(const char* genstring)
{
    const char* err = mainptr->ChangeGenCount(genstring);
    if (!err) DoAutoUpdate();
    
    return err;
}

// -----------------------------------------------------------------------------

const char* GSF_setpos(const char* x, const char* y)
{
    // disallow alphabetic chars in x,y
    int i;
    int xlen = (int)strlen(x);
    for (i = 0; i < xlen; i++)
        if ( (x[i] >= 'a' && x[i] <= 'z') || (x[i] >= 'A' && x[i] <= 'Z') )
            return "Illegal character in x value.";
    
    int ylen = (int)strlen(y);
    for (i = 0; i < ylen; i++)
        if ( (y[i] >= 'a' && y[i] <= 'z') || (y[i] >= 'A' && y[i] <= 'Z') )
            return "Illegal character in y value.";
    
    bigint bigx(x);
    bigint bigy(y);
    
    // check if x,y is outside bounded grid
    if ( (currlayer->algo->gridwd > 0 &&
            (bigx < currlayer->algo->gridleft || bigx > currlayer->algo->gridright)) ||
         (currlayer->algo->gridht > 0 &&
            (bigy < currlayer->algo->gridtop || bigy > currlayer->algo->gridbottom)) ) {
        return "Given position is outside grid boundary.";
    }
    
    viewptr->SetPosMag(bigx, bigy, viewptr->GetMag());
    DoAutoUpdate();
    
    return NULL;
}

// -----------------------------------------------------------------------------

void GSF_setname(const wxString& name, int index)
{
    if (name.length() == 0) return;
    
    // inscript should be true but play safe
    if (allowundo && !currlayer->stayclean && inscript)
        SavePendingChanges();
    
    if (index == currindex) {
        // save old name for RememberNameChange
        wxString oldname = currlayer->currname;
        
        // show new name in main window's title;
        // also sets currlayer->currname and updates menu item
        ChangeWindowTitle(name);
        
        if (allowundo && !currlayer->stayclean) {
            // note that currfile and savestart/dirty flags don't change
            currlayer->undoredo->RememberNameChange(oldname, currlayer->currfile,
                                                    currlayer->savestart, currlayer->dirty);
        }
    } else {
        // temporarily change currlayer (used in RememberNameChange)
        Layer* savelayer = currlayer;
        currlayer = GetLayer(index);
        wxString oldname = currlayer->currname;
        
        currlayer->currname = name;
        
        if (allowundo && !currlayer->stayclean) {
            // note that currfile and savestart/dirty flags don't change
            currlayer->undoredo->RememberNameChange(oldname, currlayer->currfile,
                                                    currlayer->savestart, currlayer->dirty);
        }
        
        // restore currlayer
        currlayer = savelayer;
        
        // show name in given layer's menu item
        mainptr->UpdateLayerItem(index);
    }
}

// -----------------------------------------------------------------------------

const char* GSF_setcell(int x, int y, int newstate)
{
    // check if x,y is outside bounded grid
    if ( (currlayer->algo->gridwd > 0 &&
            (x < currlayer->algo->gridleft.toint() ||
             x > currlayer->algo->gridright.toint())) ||
         (currlayer->algo->gridht > 0 &&
            (y < currlayer->algo->gridtop.toint() ||
             y > currlayer->algo->gridbottom.toint())) ) {
        return "Given cell is outside grid boundary.";
    }
    
    int oldstate = currlayer->algo->getcell(x, y);
    if (newstate != oldstate) {
        if (currlayer->algo->setcell(x, y, newstate) < 0) {
            return "State value is out of range.";
        }
        currlayer->algo->endofpattern();
        if (allowundo && !currlayer->stayclean) {
            ChangeCell(x, y, oldstate, newstate);
        }
        MarkLayerDirty();
        DoAutoUpdate();
    }
    return NULL;
}

// -----------------------------------------------------------------------------

const char* GSF_paste(int x, int y, const char* mode)
{
    // check if x,y is outside bounded grid
    if ( (currlayer->algo->gridwd > 0 &&
            (x < currlayer->algo->gridleft.toint() ||
             x > currlayer->algo->gridright.toint())) ||
         (currlayer->algo->gridht > 0 &&
            (y < currlayer->algo->gridtop.toint() ||
             y > currlayer->algo->gridbottom.toint())) ) {
        return "Given cell is outside grid boundary.";
    }
    
    if (!mainptr->ClipboardHasText())
        return "No pattern in clipboard.";
    
    // temporarily change selection and paste mode
    Selection oldsel = currlayer->currsel;
    const char* oldmode = GetPasteMode();
    
    wxString modestr = wxString(mode, wxConvLocal);
    if      (modestr.IsSameAs(wxT("and"), false))  SetPasteMode("And");
    else if (modestr.IsSameAs(wxT("copy"), false)) SetPasteMode("Copy");
    else if (modestr.IsSameAs(wxT("or"), false))   SetPasteMode("Or");
    else if (modestr.IsSameAs(wxT("xor"), false))  SetPasteMode("Xor");
    else return "Unknown mode.";
    
    // create huge selection rect so no possibility of error message
    currlayer->currsel.SetRect(x, y, INT_MAX, INT_MAX);
    
    viewptr->PasteClipboard(true);      // true = paste to selection
    
    // restore selection and paste mode
    currlayer->currsel = oldsel;
    SetPasteMode(oldmode);
    
    DoAutoUpdate();
    return NULL;
}

// -----------------------------------------------------------------------------

const char* GSF_checkpos(lifealgo* algo, int x, int y)
{
    // check that x,y is within bounded grid
    if ( (algo->gridwd > 0 &&
            (x < algo->gridleft.toint() || x > algo->gridright.toint())) ||
         (algo->gridht > 0 &&
            (y < algo->gridtop.toint() || y > algo->gridbottom.toint())) ) {
        return "Cell is outside grid boundary.";
    }
    return NULL;
}

// -----------------------------------------------------------------------------

const char* GSF_checkrect(int x, int y, int wd, int ht)
{
    if (wd <= 0) return "Rectangle width must be > 0.";
    if (ht <= 0) return "Rectangle height must be > 0.";
    
    // check that rect is completely within bounded grid
    if ( (currlayer->algo->gridwd > 0 &&
            (x < currlayer->algo->gridleft.toint() ||
             x > currlayer->algo->gridright.toint() ||
             x+wd-1 < currlayer->algo->gridleft.toint() ||
             x+wd-1 > currlayer->algo->gridright.toint())) ||
         (currlayer->algo->gridht > 0 &&
            (y < currlayer->algo->gridtop.toint() ||
             y > currlayer->algo->gridbottom.toint() ||
             y+ht-1 < currlayer->algo->gridtop.toint() ||
             y+ht-1 > currlayer->algo->gridbottom.toint())) ) {
        return "Rectangle is outside grid boundary.";
    }
    return NULL;
}

// -----------------------------------------------------------------------------

int GSF_hash(int x, int y, int wd, int ht)
{
    // calculate a hash value for pattern in given rect
    int hash = 31415962;
    int right = x + wd - 1;
    int bottom = y + ht - 1;
    int cx, cy;
    int v = 0;
    lifealgo* curralgo = currlayer->algo;
    bool multistate = curralgo->NumCellStates() > 2;
    
    for ( cy=y; cy<=bottom; cy++ ) {
        int yshift = cy - y;
        for ( cx=x; cx<=right; cx++ ) {
            int skip = curralgo->nextcell(cx, cy, v);
            if (skip >= 0) {
                // found next live cell in this row (v is >= 1 if multistate)
                cx += skip;
                if (cx <= right) {
                    // need to use a good hash function for patterns like AlienCounter.rle
                    hash = (hash * 1000003) ^ yshift;
                    hash = (hash * 1000003) ^ (cx - x);
                    if (multistate) hash = (hash * 1000003) ^ v;
                }
            } else {
                cx = right;  // done this row
            }
        }
    }
    
    return hash;
}

// -----------------------------------------------------------------------------

void GSF_select(int x, int y, int wd, int ht)
{
    if (wd < 1 || ht < 1) {
        // remove any existing selection
        viewptr->SaveCurrentSelection();
        currlayer->currsel.Deselect();
        viewptr->RememberNewSelection(_("Deselection"));
    } else {
        // set selection edges
        viewptr->SaveCurrentSelection();
        currlayer->currsel.SetRect(x, y, wd, ht);
        viewptr->RememberNewSelection(_("Selection"));
    }
}

// -----------------------------------------------------------------------------

bool GSF_setoption(const char* optname, int newval, int* oldval)
{
    if (strcmp(optname, "autofit") == 0) {
        *oldval = currlayer->autofit ? 1 : 0;
        if (*oldval != newval) {
            mainptr->ToggleAutoFit();
            // autofit option only applies to a generating pattern
            // DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "boldspacing") == 0) {
        *oldval = boldspacing;
        if (newval < 2) newval = 2;
        if (newval > MAX_SPACING) newval = MAX_SPACING;
        if (*oldval != newval) {
            boldspacing = newval;
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "drawingstate") == 0) {
        *oldval = currlayer->drawingstate;
        if (newval < 0) newval = 0;
        if (newval >= currlayer->algo->NumCellStates())
            newval = currlayer->algo->NumCellStates() - 1;
        if (*oldval != newval) {
            currlayer->drawingstate = newval;
            if (autoupdate) {
                UpdateEditBar();
                updateedit = false;
            } else {
                // update edit bar in next GSF_update call
                updateedit = true;
            }
        }
        
    } else if (strcmp(optname, "fullscreen") == 0) {
        *oldval = mainptr->fullscreen ? 1 : 0;
        if (*oldval != newval) {
            mainptr->ToggleFullScreen();
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "hyperspeed") == 0) {
        *oldval = currlayer->hyperspeed ? 1 : 0;
        if (*oldval != newval)
            mainptr->ToggleHyperspeed();
        
    } else if (strcmp(optname, "mindelay") == 0) {
        *oldval = mindelay;
        if (newval < 0) newval = 0;
        if (newval > MAX_DELAY) newval = MAX_DELAY;
        if (*oldval != newval) {
            mindelay = newval;
            mainptr->UpdateStepExponent();
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "maxdelay") == 0) {
        *oldval = maxdelay;
        if (newval < 0) newval = 0;
        if (newval > MAX_DELAY) newval = MAX_DELAY;
        if (*oldval != newval) {
            maxdelay = newval;
            mainptr->UpdateStepExponent();
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "opacity") == 0) {
        *oldval = opacity;
        if (newval < 1) newval = 1;
        if (newval > 100) newval = 100;
        if (*oldval != newval) {
            opacity = newval;
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "restoreview") == 0) {
        *oldval = restoreview ? 1 : 0;
        if (*oldval != newval) {
            restoreview = !restoreview;
            // no need for DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "savexrle") == 0) {
        *oldval = savexrle ? 1 : 0;
        if (*oldval != newval) {
            savexrle = !savexrle;
            // no need for DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "showallstates") == 0) {
        *oldval = showallstates ? 1 : 0;
        if (*oldval != newval) {
            ToggleAllStates();
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "showboldlines") == 0) {
        *oldval = showboldlines ? 1 : 0;
        if (*oldval != newval) {
            showboldlines = !showboldlines;
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "showbuttons") == 0) {
        *oldval = controlspos;
        if (newval < 0) newval = 0;
        if (newval > 4) newval = 4;
        if (*oldval != newval) {
            // update position of translucent buttons
            controlspos = newval;
            int wd, ht;
            viewptr->GetClientSize(&wd, &ht);
            viewptr->SetViewSize(wd, ht);
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "showeditbar") == 0) {
        *oldval = showedit ? 1 : 0;
        if (*oldval != newval) {
            ToggleEditBar();
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "showexact") == 0) {
        *oldval = showexact ? 1 : 0;
        if (*oldval != newval) {
            mainptr->ToggleExactNumbers();
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "showgrid") == 0) {
        *oldval = showgridlines ? 1 : 0;
        if (*oldval != newval) {
            showgridlines = !showgridlines;
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "showhashinfo") == 0) {
        *oldval = currlayer->showhashinfo ? 1 : 0;
        if (*oldval != newval)
            mainptr->ToggleHashInfo();
        
    } else if (strcmp(optname, "showpopulation") == 0) {
        *oldval = showpopulation ? 1 : 0;
        if (*oldval != newval) {
            mainptr->ToggleShowPopulation();
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "showicons") == 0) {
        *oldval = showicons ? 1 : 0;
        if (*oldval != newval) {
            viewptr->ToggleCellIcons();
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "showlayerbar") == 0) {
        *oldval = showlayer ? 1 : 0;
        if (*oldval != newval) {
            ToggleLayerBar();
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "showoverlay") == 0) {
        *oldval = showoverlay ? 1 : 0;
        if (*oldval != newval) {
            showoverlay = !showoverlay;
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "showprogress") == 0) {
        *oldval = showprogress ? 1 : 0;
        if (*oldval != newval) {
            showprogress = !showprogress;
            // no need for DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "showfiles") == 0 ||
               strcmp(optname, "showpatterns") == 0) {      // deprecated
        *oldval = showfiles ? 1 : 0;
        if (*oldval != newval) {
            mainptr->ToggleShowFiles();
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "showscripts") == 0) {
        *oldval = 0;
        if (*oldval != newval) {
            // deprecated so do nothing
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "showscrollbars") == 0) {
        *oldval = showscrollbars ? 1 : 0;
        if (*oldval != newval) {
            mainptr->ToggleScrollBars();
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "showstatusbar") == 0) {
        *oldval = showstatus ? 1 : 0;
        if (*oldval != newval) {
            mainptr->ToggleStatusBar();
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "showtimeline") == 0) {
        *oldval = showtimeline ? 1 : 0;
        if (*oldval != newval) {
            ToggleTimelineBar();
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "showtoolbar") == 0) {
        *oldval = showtool ? 1 : 0;
        if (*oldval != newval) {
            mainptr->ToggleToolBar();
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "smartscale") == 0) {
        *oldval = smartscale ? 1 : 0;
        if (*oldval != newval) {
            viewptr->ToggleSmarterScaling();
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "swapcolors") == 0) {
        *oldval = swapcolors ? 1 : 0;
        if (*oldval != newval) {
            viewptr->ToggleCellColors();
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "synccursors") == 0) {
        *oldval = synccursors ? 1 : 0;
        if (*oldval != newval) {
            ToggleSyncCursors();
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "syncviews") == 0) {
        *oldval = syncviews ? 1 : 0;
        if (*oldval != newval) {
            ToggleSyncViews();
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "switchlayers") == 0) {
        *oldval = canswitch ? 1 : 0;
        if (*oldval != newval) {
            canswitch = !canswitch;
            // no need for DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "stacklayers") == 0) {
        *oldval = stacklayers ? 1 : 0;
        if (*oldval != newval) {
            ToggleStackLayers();
            DoAutoUpdate();
        }
        
    } else if (strcmp(optname, "tilelayers") == 0) {
        *oldval = tilelayers ? 1 : 0;
        if (*oldval != newval) {
            ToggleTileLayers();
            DoAutoUpdate();
        }
        
        // this option is deprecated (use setalgo command)
    } else if (strcmp(optname, "hashing") == 0) {
        *oldval = (currlayer->algtype == HLIFE_ALGO) ? 1 : 0;
        if (*oldval != newval) {
            mainptr->ChangeAlgorithm(newval ? HLIFE_ALGO : QLIFE_ALGO);
            DoAutoUpdate();
        }
        
    } else {
        // unknown option
        return false;
    }
    
    if (*oldval != newval) {
        mainptr->UpdateMenuItems();
    }
    
    return true;
}

// -----------------------------------------------------------------------------

bool GSF_getoption(const char* optname, int* optval)
{
    if      (strcmp(optname, "autofit") == 0)           *optval = currlayer->autofit ? 1 : 0;
    else if (strcmp(optname, "boldspacing") == 0)       *optval = boldspacing;
    else if (strcmp(optname, "drawingstate") == 0)      *optval = currlayer->drawingstate;
    else if (strcmp(optname, "fullscreen") == 0)        *optval = mainptr->fullscreen ? 1 : 0;
    else if (strcmp(optname, "hyperspeed") == 0)        *optval = currlayer->hyperspeed ? 1 : 0;
    else if (strcmp(optname, "mindelay") == 0)          *optval = mindelay;
    else if (strcmp(optname, "maxdelay") == 0)          *optval = maxdelay;
    else if (strcmp(optname, "opacity") == 0)           *optval = opacity;
    else if (strcmp(optname, "restoreview") == 0)       *optval = restoreview ? 1 : 0;
    else if (strcmp(optname, "savexrle") == 0)          *optval = savexrle ? 1 : 0;
    else if (strcmp(optname, "showallstates") == 0)     *optval = showallstates ? 1 : 0;
    else if (strcmp(optname, "showboldlines") == 0)     *optval = showboldlines ? 1 : 0;
    else if (strcmp(optname, "showbuttons") == 0)       *optval = controlspos;
    else if (strcmp(optname, "showeditbar") == 0)       *optval = showedit ? 1 : 0;
    else if (strcmp(optname, "showexact") == 0)         *optval = showexact ? 1 : 0;
    else if (strcmp(optname, "showgrid") == 0)          *optval = showgridlines ? 1 : 0;
    else if (strcmp(optname, "showhashinfo") == 0)      *optval = currlayer->showhashinfo ? 1 : 0;
    else if (strcmp(optname, "showpopulation") == 0)    *optval = showpopulation ? 1 : 0;
    else if (strcmp(optname, "showicons") == 0)         *optval = showicons ? 1 : 0;
    else if (strcmp(optname, "showlayerbar") == 0)      *optval = showlayer ? 1 : 0;
    else if (strcmp(optname, "showoverlay") == 0)       *optval = showoverlay ? 1 : 0;
    else if (strcmp(optname, "showprogress") == 0)      *optval = showprogress ? 1 : 0;
    else if (strcmp(optname, "showfiles") == 0)         *optval = showfiles ? 1 : 0;
    else if (strcmp(optname, "showpatterns") == 0)      *optval = showfiles ? 1 : 0;        // deprecated
    else if (strcmp(optname, "showscripts") == 0)       *optval = 0;                        // ditto
    else if (strcmp(optname, "showscrollbars") == 0)    *optval = showscrollbars ? 1 : 0;
    else if (strcmp(optname, "showstatusbar") == 0)     *optval = showstatus ? 1 : 0;
    else if (strcmp(optname, "showtimeline") == 0)      *optval = showtimeline ? 1 : 0;
    else if (strcmp(optname, "showtoolbar") == 0)       *optval = showtool ? 1 : 0;
    else if (strcmp(optname, "smartscale") == 0)        *optval = smartscale ? 1 : 0;
    else if (strcmp(optname, "stacklayers") == 0)       *optval = stacklayers ? 1 : 0;
    else if (strcmp(optname, "swapcolors") == 0)        *optval = swapcolors ? 1 : 0;
    else if (strcmp(optname, "switchlayers") == 0)      *optval = canswitch ? 1 : 0;
    else if (strcmp(optname, "synccursors") == 0)       *optval = synccursors ? 1 : 0;
    else if (strcmp(optname, "syncviews") == 0)         *optval = syncviews ? 1 : 0;
    else if (strcmp(optname, "tilelayers") == 0)        *optval = tilelayers ? 1 : 0;
    // this option is deprecated (use getalgo command)
    else if (strcmp(optname, "hashing") == 0)
        *optval = (currlayer->algtype == HLIFE_ALGO) ? 1 : 0;
    else {
        // unknown option
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------------

bool GSF_setcolor(const char* colname, wxColor& newcol, wxColor& oldcol)
{
    if (strncmp(colname, "livecells", 9) == 0) {
        // livecells0..livecells9 are deprecated; get and set color of state 1
        oldcol.Set(currlayer->cellr[1], currlayer->cellg[1], currlayer->cellb[1]);
        if (oldcol != newcol) {
            currlayer->cellr[1] = newcol.Red();
            currlayer->cellg[1] = newcol.Green();
            currlayer->cellb[1] = newcol.Blue();
            UpdateIconColors();
            UpdateCloneColors();
            DoAutoUpdate();
        }
        
    } else if (strcmp(colname, "deadcells") == 0) {
        // deprecated; can now use setcolors([0,r,g,b])
        oldcol.Set(currlayer->cellr[0], currlayer->cellg[0], currlayer->cellb[0]);
        if (oldcol != newcol) {
            currlayer->cellr[0] = newcol.Red();
            currlayer->cellg[0] = newcol.Green();
            currlayer->cellb[0] = newcol.Blue();
            UpdateIconColors();
            UpdateCloneColors();
            DoAutoUpdate();
        }
        
    } else if (strcmp(colname, "border") == 0) {
        oldcol = *borderrgb;
        if (oldcol != newcol) {
            *borderrgb = newcol;
            DoAutoUpdate();
        }
        
    } else if (strcmp(colname, "paste") == 0) {
        oldcol = *pastergb;
        if (oldcol != newcol) {
            *pastergb = newcol;
            DoAutoUpdate();
        }
        
    } else if (strcmp(colname, "select") == 0) {
        oldcol = *selectrgb;
        if (oldcol != newcol) {
            *selectrgb = newcol;
            DoAutoUpdate();
        }
        
    } else if (strcmp(colname, "hashing") == 0) {      // deprecated
        oldcol = algoinfo[HLIFE_ALGO]->statusrgb;
        if (oldcol != newcol) {
            algoinfo[HLIFE_ALGO]->statusrgb = newcol;
            UpdateStatusBrushes();
            DoAutoUpdate();
        }
        
    } else if (strcmp(colname, "nothashing") == 0) {   // deprecated
        oldcol = algoinfo[QLIFE_ALGO]->statusrgb;
        if (oldcol != newcol) {
            algoinfo[QLIFE_ALGO]->statusrgb = newcol;
            UpdateStatusBrushes();
            DoAutoUpdate();
        }
        
    } else {
        // look for algo name
        char* algoname = ReplaceDeprecatedAlgo((char*) colname);
        for (int i = 0; i < NumAlgos(); i++) {
            if (strcmp(algoname, GetAlgoName(i)) == 0) {
                oldcol = algoinfo[i]->statusrgb;
                if (oldcol != newcol) {
                    algoinfo[i]->statusrgb = newcol;
                    UpdateStatusBrushes();
                    DoAutoUpdate();
                }
                return true;
            }
        }
        // unknown color name
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------------

bool GSF_getcolor(const char* colname, wxColor& color)
{
    if (strncmp(colname, "livecells", 9) == 0) {
        // livecells0..livecells9 are deprecated; return color of state 1
        color.Set(currlayer->cellr[1], currlayer->cellg[1], currlayer->cellb[1]);
    }
    else if (strcmp(colname, "deadcells") == 0) {
        // deprecated; can now use getcolors(0)
        color.Set(currlayer->cellr[0], currlayer->cellg[0], currlayer->cellb[0]);
    }
    else if (strcmp(colname, "border") == 0)     color = *borderrgb;
    else if (strcmp(colname, "paste") == 0)      color = *pastergb;
    else if (strcmp(colname, "select") == 0)     color = *selectrgb;
    // next two are deprecated
    else if (strcmp(colname, "hashing") == 0)    color = algoinfo[HLIFE_ALGO]->statusrgb;
    else if (strcmp(colname, "nothashing") == 0) color = algoinfo[QLIFE_ALGO]->statusrgb;
    else {
        // look for algo name
        char* algoname = ReplaceDeprecatedAlgo((char*) colname);
        for (int i = 0; i < NumAlgos(); i++) {
            if (strcmp(algoname, GetAlgoName(i)) == 0) {
                color = algoinfo[i]->statusrgb;
                return true;
            }
        }
        // unknown color name
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------------

void GSF_getevent(wxString& event, int get)
{
    if (get) {
        pass_key_events = true;     // future keyboard events will call PassKeyToScript
        pass_mouse_events = true;   // future mouse events will call PassClickToScript
        pass_file_events = true;    // future open file evenst will call PassFileToScript
        
        // rle3path is non-empty if Golly has just seen a .rle3 file and started up 3D.lua
        if (rle3path[0]) {
            event = wxT("file ") + rle3path;
            rle3path = wxEmptyString;
            return;
        }
        
    } else {
        // tell Golly to handle future keyboard/mouse/file events
        pass_key_events = false;
        pass_mouse_events = false;
        pass_file_events = false;
        // clear any pending events so event is set to empty string below
        eventqueue.Clear();
    }
    
    if (eventqueue.IsEmpty()) {
        event = wxEmptyString;
    } else {
        // get event at start of queue, then remove it
        event = eventqueue[0];
        eventqueue.RemoveAt(0);
    }
}

// -----------------------------------------------------------------------------

#if defined(__WXMAC__) && wxCHECK_VERSION(2,9,0)
    // wxMOD_CONTROL has been changed to mean Command key down (sheesh!)
    #define wxMOD_CONTROL wxMOD_RAW_CONTROL
    #define ControlDown RawControlDown
#endif

static void AppendModifiers(int modifiers, wxString& event)
{
    // reverse of GetModifiers
    if (modifiers == wxMOD_NONE) {
        event += wxT("none");
    } else {
        if (modifiers & wxMOD_ALT)       event += wxT("alt");
#ifdef __WXMAC__
        if (modifiers & wxMOD_CMD)       event += wxT("cmd");
        if (modifiers & wxMOD_CONTROL)   event += wxT("ctrl");
#else
        if (modifiers & wxMOD_CMD)       event += wxT("ctrl");
        if (modifiers & wxMOD_META)      event += wxT("meta");
#endif
        if (modifiers & wxMOD_SHIFT)     event += wxT("shift");
    }
}

// -----------------------------------------------------------------------------

static int GetModifiers(const wxString& modstring)
{
    // reverse of AppendModifiers
    int modifiers = wxMOD_NONE;
    if (modstring != wxT("none")) {
        if (modstring.Contains(wxT("alt")))     modifiers |= wxMOD_ALT;
        if (modstring.Contains(wxT("cmd")))     modifiers |= wxMOD_CMD;
        if (modstring.Contains(wxT("ctrl")))    modifiers |= wxMOD_CONTROL;
        if (modstring.Contains(wxT("meta")))    modifiers |= wxMOD_META;
        if (modstring.Contains(wxT("shift")))   modifiers |= wxMOD_SHIFT;
    }
    return modifiers;
}

// -----------------------------------------------------------------------------

const char* GSF_doevent(const wxString& event)
{
    if (event.length() > 0) {
        if (event.StartsWith(wxT("key ")) && event.length() > 7) {
            // parse event string like "key x altshift"
            int key = event[4];
            if (event[4] == 'f' && event[5] >= '1' && event[5] <= '9') {
                // parse function key (f1 to f24)
                if (event[6] == ' ') {
                    // f1 to f9
                    key = WXK_F1 + (event[5] - '1');
                } else if (event[6] >= '0' && event[6] <= '9') {
                    // f10 to f24
                    key = WXK_F1 + 10 * (event[5] - '0') + (event[6] - '0') - 1;
                    if (key > WXK_F24)
                        return "Bad function key (must be f1 to f24).";
                } else {
                    return "Bad function key (must be f1 to f24).";
                }
            } else if (event[5] != ' ') {
                // parse special char name like space, tab, etc
                // must match reverse conversion in PassKeyToScript and PassKeyUpToScript
                if (event.Contains(wxT("space")))      key = ' '; else
                if (event.Contains(wxT("home")))       key = WXK_HOME; else
                if (event.Contains(wxT("end")))        key = WXK_END; else
                if (event.Contains(wxT("pageup")))     key = WXK_PAGEUP; else
                if (event.Contains(wxT("pagedown")))   key = WXK_PAGEDOWN; else
                if (event.Contains(wxT("help")))       key = WXK_HELP; else
                if (event.Contains(wxT("insert")))     key = WXK_INSERT; else
                if (event.Contains(wxT("delete")))     key = WXK_DELETE; else
                if (event.Contains(wxT("tab")))        key = WXK_TAB; else
                if (event.Contains(wxT("enter")))      key = WXK_RETURN; else
                if (event.Contains(wxT("return")))     key = WXK_RETURN; else
                if (event.Contains(wxT("left")))       key = WXK_LEFT; else
                if (event.Contains(wxT("right")))      key = WXK_RIGHT; else
                if (event.Contains(wxT("up")))         key = WXK_UP; else
                if (event.Contains(wxT("down")))       key = WXK_DOWN; else
                return "Unknown key.";
            }
            
            viewptr->ProcessKey(key, GetModifiers(event.AfterLast(' ')));
            
            if (showtitle) {
                // update window title
                inscript = false;
                mainptr->SetWindowTitle(wxEmptyString);
                inscript = true;
                showtitle = false;
            }
            
        } else if (event.StartsWith(wxT("zoom"))) {
            // parse event string like "zoomin 10 20" or "zoomout 10 20"
            wxString xstr = event.AfterFirst(' ');
            wxString ystr = xstr.AfterFirst(' ');
            xstr = xstr.BeforeFirst(' ');
            if (!xstr.IsNumber()) return "Bad x value.";
            if (!ystr.IsNumber()) return "Bad y value.";
            int x = wxAtoi(xstr);
            int y = wxAtoi(ystr);

            // x,y is pixel position in viewport
            if (event.StartsWith(wxT("zoomin"))) {
                viewptr->TestAutoFit();
                if (currlayer->view->getmag() < MAX_MAG) {
                    currlayer->view->zoom(x, y);
                }
            } else {
                viewptr->TestAutoFit();
                currlayer->view->unzoom(x, y);
            }

            inscript = false;
            mainptr->UpdatePatternAndStatus();
            bigview->UpdateScrollBars();
            inscript = true;
            mainptr->UpdateUserInterface();
            
        } else if (event.StartsWith(wxT("click "))) {
            // parse event string like "click 10 20 left altshift"
            wxString xstr = event.AfterFirst(' ');
            wxString ystr = xstr.AfterFirst(' ');
            xstr = xstr.BeforeFirst(' ');
            ystr = ystr.BeforeFirst(' ');
            if (!xstr.IsNumber()) return "Bad x value.";
            if (!ystr.IsNumber()) return "Bad y value.";
            bigint x(xstr.mb_str(wxConvLocal));
            bigint y(ystr.mb_str(wxConvLocal));
            
            int button;
            if (event.Contains(wxT(" left "))) button = wxMOUSE_BTN_LEFT; else
                if (event.Contains(wxT(" middle "))) button = wxMOUSE_BTN_MIDDLE; else
                    if (event.Contains(wxT(" right "))) button = wxMOUSE_BTN_RIGHT; else
                        return "Unknown button.";
            
            if (viewptr->CellVisible(x, y) && viewptr->CellInGrid(x, y)) {
                // convert x,y cell position to pixel position in viewport
                pair<int,int> xy = currlayer->view->screenPosOf(x, y, currlayer->algo);
                int mods = GetModifiers(event.AfterLast(' '));
                
                viewptr->ProcessClick(xy.first, xy.second, button, mods);
                
                if (showtitle) {
                    // update window title
                    inscript = false;
                    mainptr->SetWindowTitle(wxEmptyString);
                    inscript = true;
                    showtitle = false;
                }
            } else {
                // ignore click if x,y is outside viewport or grid
                return NULL;
            }

        } else if (event.StartsWith(wxT("kup "))) {
            // ignore key up event
            return NULL;

        } else if (event.StartsWith(wxT("mup "))) {
            // ignore mouse up event
            return NULL;

        } else if (event.StartsWith(wxT("file "))) {
            // ignore file event (scripts can call GSF_open)
            return NULL;

        } else if (event.StartsWith(wxT("o"))) {
            // ignore oclick/ozoomin/ozoomout event in overlay
            return NULL;

        } else {
            return "Unknown event.";
        }
    }
    return NULL;
}

// -----------------------------------------------------------------------------

// the following is deprecated (use GSF_getevent)

char GSF_getkey()
{
    pass_key_events = true;   // future keyboard events will call PassKeyToScript
    
    if (scriptchars.length() == 0) {
        // return empty string
        return '\0';
    } else {
        // return first char in scriptchars and then remove it
        char ch = scriptchars.GetChar(0);
        scriptchars = scriptchars.AfterFirst(ch);
        return ch;
    }
}

// -----------------------------------------------------------------------------

// the following is deprecated (use GSF_doevent)

void GSF_dokey(const char* ascii)
{
    if (*ascii) {
        // convert ascii char to corresponding wx key code;
        // note that PassKeyToScript does the reverse conversion
        int key;
        switch (*ascii) {
            case 127:   // treat delete like backspace
            case 8:     key = WXK_BACK;   break;
            case 9:     key = WXK_TAB;    break;
            case 10:    // treat linefeed like return
            case 13:    key = WXK_RETURN; break;
            case 28:    key = WXK_LEFT;   break;
            case 29:    key = WXK_RIGHT;  break;
            case 30:    key = WXK_UP;     break;
            case 31:    key = WXK_DOWN;   break;
            default:    key = *ascii;
        }
        
        // we can't handle modifiers here
        viewptr->ProcessKey(key, wxMOD_NONE);
        
        if (showtitle) {
            // update window title
            inscript = false;
            mainptr->SetWindowTitle(wxEmptyString);
            inscript = true;
            showtitle = false;
        }
    }
}

// -----------------------------------------------------------------------------

void GSF_update()
{
    // update viewport, status bar, and possibly other bars
    inscript = false;
    
    // pass in true so that Update() is called
    mainptr->UpdatePatternAndStatus(true);
    
    if (showtitle) {
        mainptr->SetWindowTitle(wxEmptyString);
        showtitle = false;
    }
    
    if (updateedit) {
        UpdateEditBar();
        updateedit = false;
    }
    
    inscript = true;

    #ifdef __WXGTK__
        // needed on Linux to see update immediately
        insideYield = true;
        wxGetApp().Yield(true);
        insideYield = false;
    #endif
}

// -----------------------------------------------------------------------------

void GSF_exit(const wxString& errmsg)
{
    if (!errmsg.IsEmpty()) {
        // display given error message
        inscript = false;
        statusptr->ErrorMessage(errmsg);
        inscript = true;
        // make sure status bar is visible
        if (!showstatus) mainptr->ToggleStatusBar();
    }
    
    exitcalled = true;   // prevent CheckScriptError changing message
}

#ifdef __WXMAC__
    // convert path to decomposed UTF8 so fopen will work
    #define CURRFILE currlayer->currfile.fn_str()
#else
    #define CURRFILE currlayer->currfile.mb_str(wxConvLocal)
#endif

// -----------------------------------------------------------------------------

const char* GSF_getpath()
{
    // need to be careful converting Unicode wxString to char*
    static wxCharBuffer path;
    path = CURRFILE;
    return (const char*) path;
}

// -----------------------------------------------------------------------------

const char* GSF_getinfo()
{
    // comment buffer
    static char comments[maxcomments];

    // buffer for receiving comment data (allocated by readcomments)
    char *commptr = NULL;

    // read the comments in the pattern file
    const char* err = readcomments(CURRFILE, &commptr);
    if (err) {
        free(commptr);
        return "";
    }

    // copy the comments and truncate to buffer size if longer
    strncpy(comments, commptr, maxcomments);
    comments[maxcomments - 1] = '\0';
    free(commptr);
    return comments;
}

// =============================================================================

void CheckScriptError(const wxString& ext)
{
    if (scripterr.IsEmpty()) return;    // no error
    
    if (scripterr.Find(wxString(abortmsg,wxConvLocal)) >= 0) {
        // error was caused by AbortLuaScript/AbortPerlScript/AbortPythonScript
        // so don't display scripterr
    } else {
        wxString errtype;
        if (ext.IsSameAs(wxT("lua"), false)) {
            errtype = _("Lua error:");
        } else if (ext.IsSameAs(wxT("pl"), false)) {
            errtype = _("Perl error:");
            scripterr.Replace(wxT(". at "), wxT("\nat "));
        } else {
            errtype = _("Python error:");
            scripterr.Replace(wxT("  File \"<string>\", line 1, in ?\n"), wxT(""));
        }
        Beep();
        #ifdef __WXMAC__
            wxSetCursor(*wxSTANDARD_CURSOR);
        #endif
        wxMessageBox(scripterr, errtype, wxOK | wxICON_EXCLAMATION, wxGetActiveWindow());
    }
    
    // don't change status message if GSF_exit was used to stop script
    if (!exitcalled) statusptr->DisplayMessage(_("Script aborted."));
}

// -----------------------------------------------------------------------------

void ChangeCell(int x, int y, int oldstate, int newstate)
{
    // first check if there are any pending gen changes that need to be remembered
    if (currlayer->undoredo->savegenchanges) {
        currlayer->undoredo->savegenchanges = false;
        currlayer->undoredo->RememberGenFinish();
    }
    
    // setcell/putcells command is changing state of cell at x,y
    currlayer->undoredo->SaveCellChange(x, y, oldstate, newstate);
    if (!currlayer->undoredo->savecellchanges) {
        currlayer->undoredo->savecellchanges = true;
        // save layer's dirty state for next RememberCellChanges call
        currlayer->savedirty = currlayer->dirty;
    }
}

// -----------------------------------------------------------------------------

void SavePendingChanges(bool checkgenchanges)
{
    // this should only be called if inscript && allowundo && !currlayer->stayclean
    if ( !(inscript && allowundo && !currlayer->stayclean) )
        Warning(_("Bug detected in SavePendingChanges!"));
    
    if (currlayer->undoredo->savecellchanges) {
        currlayer->undoredo->savecellchanges = false;
        // remember accumulated cell changes
        currlayer->undoredo->RememberCellChanges(_("bug1"), currlayer->savedirty);
        // action string should never be seen
    }
    
    if (checkgenchanges && currlayer->undoredo->savegenchanges) {
        currlayer->undoredo->savegenchanges = false;
        // remember accumulated gen changes
        currlayer->undoredo->RememberGenFinish();
    }
}

// -----------------------------------------------------------------------------

void RunScript(const wxString& filename)
{
    if (TimelineExists()) {
        statusptr->ErrorMessage(_("You can't run a script if there is a timeline."));
        return;
    }
    
    // use these flags to allow re-entrancy
    bool already_inscript = inscript;
    bool in_luascript = luascript;
    bool in_plscript = plscript;
    bool in_pyscript = pyscript;
    wxString savecwd;
    
    if (!wxFileName::FileExists(filename)) {
        Warning(_("The script file does not exist:\n") + filename);
        return;
    }
    
    if (already_inscript) {
        // save current directory so we can restore it below
        savecwd = scriptloc;
    } else {
        mainptr->showbanner = false;
        statusptr->ClearMessage();
        scripttitle.Clear();
        scripterr.Clear();
        scriptchars.Clear();
        eventqueue.Clear();
        canswitch = false;
        stop_after_script = false;
        autoupdate = false;
        exitcalled = false;
        allowcheck = true;
        showprogress = true;
        showtitle = false;
        updateedit = false;
        pass_key_events = false;
        pass_mouse_events = false;
        pass_file_events = false;
        wxGetApp().PollerReset();
    }
    
    // temporarily change current directory to location of script
    wxFileName fullname(filename);
    fullname.Normalize();
    scriptloc = fullname.GetPath();
    if ( scriptloc.Last() != wxFILE_SEP_PATH ) scriptloc += wxFILE_SEP_PATH;
    wxSetWorkingDirectory(scriptloc);
    
    wxString fpath = fullname.GetFullPath();
    #ifdef __WXMAC__
        // use decomposed UTF8 so interpreter can open names with non-ASCII chars
        fpath = wxString(fpath.fn_str(),wxConvLocal);
    #endif
    
    if (!already_inscript) {
        if (allowundo) {
            // save each layer's dirty state for use by next RememberCellChanges call
            for ( int i = 0; i < numlayers; i++ ) {
                Layer* layer = GetLayer(i);
                layer->savedirty = layer->dirty;
                // at start of script there are no pending cell/gen changes
                layer->undoredo->savecellchanges = false;
                layer->undoredo->savegenchanges = false;
                // add a special node to indicate that the script is about to start so
                // that all changes made by the script can be undone/redone in one go;
                // note that the UndoRedo ctor calls RememberScriptStart if the script
                // creates a new non-cloned layer, and we let RememberScriptStart handle
                // multiple calls if this layer is a clone
                layer->undoredo->RememberScriptStart();
            }
        }
        
        inscript = true;
        
        mainptr->UpdateUserInterface();
        
        // temporarily remove accelerators from all menu items
        // so keyboard shortcuts can be passed to script
        mainptr->UpdateMenuAccelerators();
    }
    
    wxString ext = filename.AfterLast('.');
    if (ext.IsSameAs(wxT("lua"), false)) {
        luascript = true;
        RunLuaScript(fpath);
    } else if (ext.IsSameAs(wxT("pl"), false)) {
        plscript = true;
        RunPerlScript(fpath);
    } else if (ext.IsSameAs(wxT("py"), false)) {
        pyscript = true;
        RunPythonScript(fpath);
    } else {
        // should never happen
        luascript = false;
        plscript = false;
        pyscript = false;
        Warning(_("Unexpected extension in script file:\n") + filename);
    }
    
    if (already_inscript) {
        // restore current directory saved above
        scriptloc = savecwd;
        wxSetWorkingDirectory(scriptloc);
        
        // display any Lua/Perl/Python error message
        CheckScriptError(ext);
        if (!scripterr.IsEmpty()) {
            if (in_luascript) {
                // abort the calling Lua script
                AbortLuaScript();
            } else if (in_pyscript) {
                // abort the calling Python script
                AbortPythonScript();
            } else if (in_plscript) {
                // abort the calling Perl script
                AbortPerlScript();
            }
        }
        
        luascript = in_luascript;
        plscript = in_plscript;
        pyscript = in_pyscript;
        
    } else {
        // already_inscript is false
        
        // tidy up the undo/redo history for each layer; note that some calls
        // use currlayer (eg. RememberGenFinish) so we temporarily set currlayer
        // to each layer -- this is a bit yukky but should be safe as long as we
        // synchronize clone info, especially currlayer->algo ptrs because they
        // can change if the script called new()
        SyncClones();
        Layer* savelayer = currlayer;
        for ( int i = 0; i < numlayers; i++ ) {
            currlayer = GetLayer(i);
            if (allowundo) {
                if (currlayer->undoredo->savecellchanges) {
                    currlayer->undoredo->savecellchanges = false;
                    // remember pending cell change(s)
                    if (currlayer->stayclean)
                        currlayer->undoredo->ForgetCellChanges();
                    else
                        currlayer->undoredo->RememberCellChanges(_("bug2"), currlayer->savedirty);
                    // action string should never be seen
                }
                if (currlayer->undoredo->savegenchanges) {
                    currlayer->undoredo->savegenchanges = false;
                    // remember pending gen change(s); no need to test stayclean flag
                    // (if it's true then NextGeneration called RememberGenStart)
                    currlayer->undoredo->RememberGenFinish();
                }
                // add special node to indicate that the script has finished;
                // we let RememberScriptFinish handle multiple calls if this
                // layer is a clone
                currlayer->undoredo->RememberScriptFinish();
            }
            // reset the stayclean flag in case it was set by MarkLayerClean
            currlayer->stayclean = false;
        }
        currlayer = savelayer;
        
        // must reset inscript AFTER RememberGenFinish
        inscript = false;
        
        // restore current directory to location of Golly app
        wxSetWorkingDirectory(gollydir);
        
        luascript = false;
        plscript = false;
        pyscript = false;
        
        // update Undo/Redo items based on current layer's history
        if (allowundo) currlayer->undoredo->UpdateUndoRedoItems();
        
        // display any error message
        CheckScriptError(ext);
        
        if (!scripttitle.IsEmpty()) {
            scripttitle.Clear();
            showtitle = true;
        }
        
        // update title, menu bar, cursor, viewport, status bar, tool bar, etc
        if (showtitle) mainptr->SetWindowTitle(wxEmptyString);
        mainptr->UpdateEverything();
        
        // restore accelerators that were cleared above
        mainptr->UpdateMenuAccelerators();
    }
}

// -----------------------------------------------------------------------------

void PassOverlayClickToScript(int ox, int oy, int button, int modifiers)
{
    // build a string like "oclick 30 50 left none" and add to event queue
    // for possible consumption by GSF_getevent
    wxString clickinfo;
    clickinfo.Printf(wxT("oclick %d %d"), ox, oy);
    if (button == wxMOUSE_BTN_LEFT)     clickinfo += wxT(" left ");
    if (button == wxMOUSE_BTN_MIDDLE)   clickinfo += wxT(" middle ");
    if (button == wxMOUSE_BTN_RIGHT)    clickinfo += wxT(" right ");
    AppendModifiers(modifiers, clickinfo);
    eventqueue.Add(clickinfo);
}

// -----------------------------------------------------------------------------

void PassClickToScript(const bigint& x, const bigint& y, int button, int modifiers)
{
    // build a string like "click 10 20 left altshift" and add to event queue
    // for possible consumption by GSF_getevent
    wxString clickinfo = wxT("click ");
    clickinfo += wxString(x.tostring('\0'),wxConvLocal);
    clickinfo += wxT(" ");
    clickinfo += wxString(y.tostring('\0'),wxConvLocal);
    if (button == wxMOUSE_BTN_LEFT)     clickinfo += wxT(" left ");
    if (button == wxMOUSE_BTN_MIDDLE)   clickinfo += wxT(" middle ");
    if (button == wxMOUSE_BTN_RIGHT)    clickinfo += wxT(" right ");
    AppendModifiers(modifiers, clickinfo);
    eventqueue.Add(clickinfo);
}

// -----------------------------------------------------------------------------

void PassMouseUpToScript(int button)
{
    // build a string like "mup left" and add to event queue
    // for possible consumption by GSF_getevent
    wxString minfo = wxT("mup ");
    if (button == wxMOUSE_BTN_LEFT)     minfo += wxT("left");
    if (button == wxMOUSE_BTN_MIDDLE)   minfo += wxT("middle");
    if (button == wxMOUSE_BTN_RIGHT)    minfo += wxT("right");
    eventqueue.Add(minfo);
}

// -----------------------------------------------------------------------------

void PassZoomInToScript(int x, int y)
{
    int ox, oy;
    if (showoverlay && curroverlay->PointInOverlay(x, y, &ox, &oy)
                    && !curroverlay->TransparentPixel(ox, oy)) {
        // zoom in to the overlay pixel at ox,oy
        wxString zinfo;
        zinfo.Printf(wxT("ozoomin %d %d"), ox, oy);
        eventqueue.Add(zinfo);
    
    } else {
        // zoom in to the viewport pixel at x,y (note that it's best not to
        // pass the corresponding cell position because a doevent call will result
        // in unwanted drifting due to conversion back to a pixel position)
        wxString zinfo;
        zinfo.Printf(wxT("zoomin %d %d"), x, y);
        eventqueue.Add(zinfo);
    }
}

// -----------------------------------------------------------------------------

void PassZoomOutToScript(int x, int y)
{
    int ox, oy;
    if (showoverlay && curroverlay->PointInOverlay(x, y, &ox, &oy)
                    && !curroverlay->TransparentPixel(ox, oy)) {
        // zoom out from the overlay pixel at ox,oy
        wxString zinfo;
        zinfo.Printf(wxT("ozoomout %d %d"), ox, oy);
        eventqueue.Add(zinfo);
    
    } else {
        // zoom out from the viewport pixel at x,y
        wxString zinfo;
        zinfo.Printf(wxT("zoomout %d %d"), x, y);
        eventqueue.Add(zinfo);
    }
}

// -----------------------------------------------------------------------------

void PassKeyUpToScript(int key)
{
    // build a string like "kup x" and add to event queue
    // for possible consumption by GSF_getevent
    wxString keyinfo = wxT("kup ");
    if (key > ' ' && key <= '~') {
        // displayable ASCII
        if (key >= 'A' && key <= 'Z') key += 32;  // convert A..Z to a..z to match case in key event
        keyinfo += wxChar(key);
    } else if (key >= WXK_F1 && key <= WXK_F24) {
        // function key
        keyinfo += wxString::Format(wxT("f%d"), key - WXK_F1 + 1);
    } else {
        // convert some special key codes to names like space, tab, delete, etc
        // (must match reverse conversion in GSF_doevent)
        switch (key) {
            case ' ':               keyinfo += wxT("space");      break;
            case WXK_HOME:          keyinfo += wxT("home");       break;
            case WXK_END:           keyinfo += wxT("end");        break;
            case WXK_PAGEUP:        keyinfo += wxT("pageup");     break;
            case WXK_PAGEDOWN:      keyinfo += wxT("pagedown");   break;
            case WXK_HELP:          keyinfo += wxT("help");       break;
            case WXK_INSERT:        keyinfo += wxT("insert");     break;
            case WXK_BACK:          // treat backspace like delete
            case WXK_DELETE:        keyinfo += wxT("delete");     break;
            case WXK_TAB:           keyinfo += wxT("tab");        break;
            case WXK_NUMPAD_ENTER:  // treat enter like return
            case WXK_RETURN:        keyinfo += wxT("return");     break;
            case WXK_LEFT:          keyinfo += wxT("left");       break;
            case WXK_RIGHT:         keyinfo += wxT("right");      break;
            case WXK_UP:            keyinfo += wxT("up");         break;
            case WXK_DOWN:          keyinfo += wxT("down");       break;
            case WXK_ADD:           keyinfo += wxT("+");          break;
            case WXK_SUBTRACT:      keyinfo += wxT("-");          break;
            case WXK_DIVIDE:        keyinfo += wxT("/");          break;
            case WXK_MULTIPLY:      keyinfo += wxT("*");          break;
            default:                return;  // ignore all other key codes
        }
    }
    eventqueue.Add(keyinfo);
}

// -----------------------------------------------------------------------------

void PassKeyToScript(int key, int modifiers)
{
    if (key == WXK_ESCAPE) {
        if (mainptr->generating) {
            // interrupt a run() or step() command
            wxGetApp().PollerInterrupt();
        }
        if (luascript) AbortLuaScript();
        if (plscript) AbortPerlScript();
        if (pyscript) AbortPythonScript();
    } else {
        // build a string like "key x altshift" and add to event queue
        // for possible consumption by GSF_getevent
        wxString keyinfo = wxT("key ");
        if (key > ' ' && key <= '~') {
            // displayable ASCII
            keyinfo += wxChar(key);
        } else if (key >= WXK_F1 && key <= WXK_F24) {
            // function key
            keyinfo += wxString::Format(wxT("f%d"), key - WXK_F1 + 1);
        } else {
            // convert some special key codes to names like space, tab, delete, etc
            // (must match reverse conversion in GSF_doevent)
            switch (key) {
                case ' ':               keyinfo += wxT("space");      break;
                case WXK_HOME:          keyinfo += wxT("home");       break;
                case WXK_END:           keyinfo += wxT("end");        break;
                case WXK_PAGEUP:        keyinfo += wxT("pageup");     break;
                case WXK_PAGEDOWN:      keyinfo += wxT("pagedown");   break;
                case WXK_HELP:          keyinfo += wxT("help");       break;
                case WXK_INSERT:        keyinfo += wxT("insert");     break;
                case WXK_BACK:          // treat backspace like delete
                case WXK_DELETE:        keyinfo += wxT("delete");     break;
                case WXK_TAB:           keyinfo += wxT("tab");        break;
                case WXK_NUMPAD_ENTER:  // treat enter like return
                case WXK_RETURN:        keyinfo += wxT("return");     break;
                case WXK_LEFT:          keyinfo += wxT("left");       break;
                case WXK_RIGHT:         keyinfo += wxT("right");      break;
                case WXK_UP:            keyinfo += wxT("up");         break;
                case WXK_DOWN:          keyinfo += wxT("down");       break;
                case WXK_ADD:           keyinfo += wxT("+");          break;
                case WXK_SUBTRACT:      keyinfo += wxT("-");          break;
                case WXK_DIVIDE:        keyinfo += wxT("/");          break;
                case WXK_MULTIPLY:      keyinfo += wxT("*");          break;
                default:                return;  // ignore all other key codes
            }
        }
        keyinfo += wxT(" ");
        AppendModifiers(modifiers, keyinfo);
        eventqueue.Add(keyinfo);
        
        // NOTE: following code is for deprecated getkey() command
        
        // convert wx key code to corresponding ascii char (if possible)
        // so that scripts can be platform-independent;
        // note that GSF_dokey does the reverse conversion
        char ascii;
        if (key >= ' ' && key <= '~') {
            if (modifiers == wxMOD_SHIFT && key >= 'a' && key <= 'z') {
                // let script see A..Z
                ascii = key - 32;
            } else {
                ascii = key;
            }
        } else {
            switch (key) {
                case WXK_DELETE:     // treat delete like backspace
                case WXK_BACK:       ascii = 8;     break;
                case WXK_TAB:        ascii = 9;     break;
                case WXK_NUMPAD_ENTER: // treat enter like return
                case WXK_RETURN:     ascii = 13;    break;
                case WXK_LEFT:       ascii = 28;    break;
                case WXK_RIGHT:      ascii = 29;    break;
                case WXK_UP:         ascii = 30;    break;
                case WXK_DOWN:       ascii = 31;    break;
                case WXK_ADD:        ascii = '+';   break;
                case WXK_SUBTRACT:   ascii = '-';   break;
                case WXK_DIVIDE:     ascii = '/';   break;
                case WXK_MULTIPLY:   ascii = '*';   break;
                default:             return;  // ignore all other key codes
            }
        }
        // save ascii char for possible consumption by GSF_getkey
        scriptchars += ascii;
    }
}

// -----------------------------------------------------------------------------

void PassFileToScript(const wxString& filepath)
{
    wxString fileinfo = wxT("file ");
    fileinfo += filepath;
    eventqueue.Add(fileinfo);
}

// -----------------------------------------------------------------------------

void FinishScripting()
{
    // called when main window is closing (ie. app is quitting)
    if (inscript) {
        if (mainptr->generating) {
            // interrupt a run() or step() command
            wxGetApp().PollerInterrupt();
        }
        if (luascript) AbortLuaScript();
        if (plscript) AbortPerlScript();
        if (pyscript) AbortPythonScript();
        wxSetWorkingDirectory(gollydir);
        inscript = false;
    }
    
    FinishLuaScripting();
    FinishPerlScripting();
    FinishPythonScripting();
}
