// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "writepattern.h"
#include "util.h"           // for linereader

#include "utils.h"          // for Warning, TimeInSeconds, Poller, event_checker, etc
#include "prefs.h"          // for allowundo, etc
#include "status.h"         // for ErrorMessage, etc
#include "file.h"           // for WritePattern, LoadPattern, etc
#include "algos.h"          // for *_ALGO, algo_type, CreateNewUniverse, etc
#include "layer.h"          // for currlayer, RestoreRule, etc
#include "view.h"           // for UpdatePatternAndStatus, draw_pending, etc
#include "undo.h"           // for UndoRedo
#include "control.h"

#ifdef ANDROID_GUI
    #include "jnicalls.h"		// for UpdateStatus, BeginProgress, etc
#endif

#ifdef IOS_GUI
    #import "PatternViewController.h"   // for UpdateStatus, BeginProgress, etc
#endif

#ifdef WEB_GUI
    #include "webcalls.h"		// for UpdateStatus, BeginProgress, etc
#endif

// -----------------------------------------------------------------------------

bool generating = false;        // currently generating pattern?
int minexpo = 0;                // step exponent at maximum delay (must be <= 0)
double begintime, endtime;      // for timing info
double begingen, endgen;        // ditto

const char* empty_pattern = "All cells are dead.";

// -----------------------------------------------------------------------------

bool SaveStartingPattern()
{
    if ( currlayer->algo->getGeneration() > currlayer->startgen ) {
        // don't do anything if current gen count > starting gen
        return true;
    }

    // save current rule, dirty flag, scale, location, etc
    currlayer->startname = currlayer->currname;
    currlayer->startrule = currlayer->algo->getrule();
    currlayer->startdirty = currlayer->dirty;
    currlayer->startmag = currlayer->view->getmag();
    currlayer->startx = currlayer->view->x;
    currlayer->starty = currlayer->view->y;
    currlayer->startbase = currlayer->currbase;
    currlayer->startexpo = currlayer->currexpo;
    currlayer->startalgo = currlayer->algtype;

    // if this layer is a clone then save some settings in other clones
    if (currlayer->cloneid > 0) {
        for ( int i = 0; i < numlayers; i++ ) {
            Layer* cloneptr = GetLayer(i);
            if (cloneptr != currlayer && cloneptr->cloneid == currlayer->cloneid) {
                cloneptr->startname = cloneptr->currname;
                cloneptr->startx = cloneptr->view->x;
                cloneptr->starty = cloneptr->view->y;
                cloneptr->startmag = cloneptr->view->getmag();
                cloneptr->startbase = cloneptr->currbase;
                cloneptr->startexpo = cloneptr->currexpo;
            }
        }
    }

    // save current selection
    currlayer->startsel = currlayer->currsel;

    if ( !currlayer->savestart ) {
        // no need to save pattern (use currlayer->startfile as the starting pattern)
        if (currlayer->startfile.empty())
            Warning("Bug in SaveStartingPattern: startfile is empty!");
        return true;
    }

    currlayer->startfile = currlayer->tempstart;     // ResetPattern will load tempstart

    // save starting pattern in tempstart file
    if ( currlayer->algo->hyperCapable() ) {
        // much faster to save pattern in a macrocell file
        const char* err = WritePattern(currlayer->tempstart.c_str(), MC_format,
                                       no_compression, 0, 0, 0, 0);
        if (err) {
            ErrorMessage(err);
            // don't allow user to continue generating
            return false;
        }
    } else {
        // can only save as RLE if edges are within getcell/setcell limits
        bigint top, left, bottom, right;
        currlayer->algo->findedges(&top, &left, &bottom, &right);
        if ( OutsideLimits(top, left, bottom, right) ) {
            ErrorMessage("Starting pattern is outside +/- 10^9 boundary.");
            // don't allow user to continue generating
            return false;
        }
        int itop = top.toint();
        int ileft = left.toint();
        int ibottom = bottom.toint();
        int iright = right.toint();
        // use XRLE format so the pattern's top left location and the current
        // generation count are stored in the file
        const char* err = WritePattern(currlayer->tempstart.c_str(), XRLE_format, no_compression,
                                       itop, ileft, ibottom, iright);
        if (err) {
            ErrorMessage(err);
            // don't allow user to continue generating
            return false;
        }
    }

    return true;
}

// -----------------------------------------------------------------------------

void SetGenIncrement()
{
    if (currlayer->currexpo > 0) {
        bigint inc = 1;
        int maxexpo = 1 ;
        if (currlayer->currbase <= 10000) {
           int mantissa = currlayer->currbase ;
           int himantissa = 0x7fffffff ;
           while (mantissa > 1 && 0 == (mantissa & 1))
              mantissa >>= 1 ;
           if (mantissa == 1) {
              maxexpo = 0x7fffffff ;
           } else {
              int p = mantissa ;
              while (p <= himantissa / mantissa) {
                 p *= mantissa ;
                 maxexpo++ ;
              }
           }
        }
        if (currlayer->currexpo > maxexpo)
           currlayer->currexpo = maxexpo ;
        // set increment to currbase^currexpo
        int i = currlayer->currexpo;
        while (i > 0) {
            if (currlayer->currbase > 10000) {
               inc = currlayer->currbase ;
            } else {
               inc.mul_smallint(currlayer->currbase);
            }
            i--;
        }
        currlayer->algo->setIncrement(inc);
    } else {
        currlayer->algo->setIncrement(1);
    }
}

// -----------------------------------------------------------------------------

void SetStepExponent(int newexpo)
{
    currlayer->currexpo = newexpo;
    if (currlayer->currexpo < minexpo) currlayer->currexpo = minexpo;
    SetGenIncrement();
}

// -----------------------------------------------------------------------------

void SetMinimumStepExponent()
{
    // set minexpo depending on mindelay and maxdelay
    minexpo = 0;
    if (mindelay > 0) {
        int d = mindelay;
        minexpo--;
        while (d < maxdelay) {
            d *= 2;
            minexpo--;
        }
    }
}

// -----------------------------------------------------------------------------

void ResetPattern(bool resetundo)
{
    if (currlayer->algo->getGeneration() == currlayer->startgen) return;

    if (currlayer->algo->getGeneration() < currlayer->startgen) {
        // if this happens then startgen logic is wrong
        Warning("Current gen < starting gen!");
        return;
    }

    if (currlayer->startfile.empty()) {
        // if this happens then savestart logic is wrong
        Warning("Starting pattern cannot be restored!");
        return;
    }

    // save current algo and rule
    algo_type oldalgo = currlayer->algtype;
    std::string oldrule = currlayer->algo->getrule();

    // restore pattern and settings saved by SaveStartingPattern;
    // first restore algorithm
    currlayer->algtype = currlayer->startalgo;

    // restore starting pattern
    LoadPattern(currlayer->startfile.c_str(), "");

    if (currlayer->algo->getGeneration() != currlayer->startgen) {
        // LoadPattern failed to reset the gen count to startgen
        // (probably because the user deleted the starting pattern)
        // so best to clear the pattern and reset the gen count
        CreateUniverse();
        currlayer->algo->setGeneration(currlayer->startgen);
        std::string msg = "Failed to reset pattern from this file:\n";
        msg += currlayer->startfile;
        Warning(msg.c_str());
    }

    // restore settings saved by SaveStartingPattern
    RestoreRule(currlayer->startrule.c_str());
    currlayer->currname = currlayer->startname;
    currlayer->dirty = currlayer->startdirty;
    if (restoreview) {
        currlayer->view->setpositionmag(currlayer->startx, currlayer->starty, currlayer->startmag);
    }

    // restore step size and set increment
    currlayer->currbase = currlayer->startbase;
    currlayer->currexpo = currlayer->startexpo;
    SetGenIncrement();

    // if this layer is a clone then restore some settings in other clones
    if (currlayer->cloneid > 0) {
        for ( int i = 0; i < numlayers; i++ ) {
            Layer* cloneptr = GetLayer(i);
            if (cloneptr != currlayer && cloneptr->cloneid == currlayer->cloneid) {
                cloneptr->currname = cloneptr->startname;
                if (restoreview) {
                    cloneptr->view->setpositionmag(cloneptr->startx, cloneptr->starty,
                                                   cloneptr->startmag);
                }
                cloneptr->currbase = cloneptr->startbase;
                cloneptr->currexpo = cloneptr->startexpo;
                // also synchronize dirty flags and update items in Layer menu
                cloneptr->dirty = currlayer->dirty;
                //!!! UpdateLayerItem(i);
            }
        }
    }

    // restore selection
    currlayer->currsel = currlayer->startsel;

    // switch to default colors if algo/rule changed
    std::string newrule = currlayer->algo->getrule();
    if (oldalgo != currlayer->algtype || oldrule != newrule) {
        UpdateLayerColors();
    }

    // update window title in case currname, rule or dirty flag changed
    //!!! SetWindowTitle(currlayer->currname);

    if (allowundo) {
        if (resetundo) {
            // wind back the undo history to the starting pattern
            currlayer->undoredo->SyncUndoHistory();
        }
    }
}

// -----------------------------------------------------------------------------

void RestorePattern(bigint& gen, const char* filename,
                    bigint& x, bigint& y, int mag, int base, int expo)
{
    // called to undo/redo a generating change
    if (gen == currlayer->startgen) {
        // restore starting pattern (false means don't call SyncUndoHistory)
        ResetPattern(false);
    } else {
        // restore pattern in given filename
        LoadPattern(filename, "");

        if (currlayer->algo->getGeneration() != gen) {
            // best to clear the pattern and set the expected gen count
            CreateUniverse();
            currlayer->algo->setGeneration(gen);
            std::string msg = "Could not restore pattern from this file:\n";
            msg += filename;
            Warning(msg.c_str());
        }

        // restore step size and set increment
        currlayer->currbase = base;
        currlayer->currexpo = expo;
        SetGenIncrement();

        // restore position and scale, if allowed
        if (restoreview) currlayer->view->setpositionmag(x, y, mag);

        UpdatePatternAndStatus();
    }
}

// -----------------------------------------------------------------------------

const char* ChangeGenCount(const char* genstring, bool inundoredo)
{
    // disallow alphabetic chars in genstring
    for (unsigned int i = 0; i < strlen(genstring); i++)
        if ( (genstring[i] >= 'a' && genstring[i] <= 'z') ||
             (genstring[i] >= 'A' && genstring[i] <= 'Z') )
            return "Alphabetic character is not allowed in generation string.";

    bigint oldgen = currlayer->algo->getGeneration();
    bigint newgen(genstring);

    if (genstring[0] == '+' || genstring[0] == '-') {
        // leading +/- sign so make newgen relative to oldgen
        bigint relgen = newgen;
        newgen = oldgen;
        newgen += relgen;
        if (newgen < bigint::zero) newgen = bigint::zero;
    }

    // set stop_after_script BEFORE testing newgen == oldgen so scripts
    // can call setgen("+0") to prevent further generating
    //!!! if (inscript) stop_after_script = true;

    if (newgen == oldgen) return NULL;

    if (!inundoredo && allowundo && !currlayer->stayclean && inscript) {
        // script called setgen()
        //!!! SavePendingChanges();
    }

    // need IsParityShifted() method???
    if (currlayer->algtype == QLIFE_ALGO && newgen.odd() != oldgen.odd()) {
        // qlife stores pattern in different bits depending on gen parity,
        // so we need to create a new qlife universe, set its gen, copy the
        // current pattern to the new universe, then switch to that universe
        bigint top, left, bottom, right;
        currlayer->algo->findedges(&top, &left, &bottom, &right);
        if ( OutsideLimits(top, left, bottom, right) ) {
            return "Pattern is too big to copy.";
        }
        // create a new universe of same type and same rule
        lifealgo* newalgo = CreateNewUniverse(currlayer->algtype);
        const char* err = newalgo->setrule(currlayer->algo->getrule());
        if (err) {
            delete newalgo;
            return "Current rule is no longer valid!";
        }
        newalgo->setGeneration(newgen);
        // copy pattern
        if ( !CopyRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                       currlayer->algo, newalgo, false, "Copying pattern") ) {
            delete newalgo;
            return "Failed to copy pattern.";
        }
        // switch to new universe
        delete currlayer->algo;
        currlayer->algo = newalgo;
        SetGenIncrement();
    } else {
        currlayer->algo->setGeneration(newgen);
    }

    if (!inundoredo) {
        // save some settings for RememberSetGen below
        bigint oldstartgen = currlayer->startgen;
        bool oldsave = currlayer->savestart;

        // may need to change startgen and savestart
        if (oldgen == currlayer->startgen || newgen <= currlayer->startgen) {
            currlayer->startgen = newgen;
            currlayer->savestart = true;
        }

        if (allowundo && !currlayer->stayclean) {
            currlayer->undoredo->RememberSetGen(oldgen, newgen, oldstartgen, oldsave);
        }
    }

    UpdateStatus();
    return NULL;
}

// -----------------------------------------------------------------------------

void DisplayTimingInfo()
{
    endtime = TimeInSeconds();
    if (endtime <= begintime) endtime = begintime + 0.000001;
    endgen = currlayer->algo->getGeneration().todouble();

    double secs = endtime - begintime;
    double gens = endgen - begingen;
    char s[128];
    sprintf(s, "%g gens in %g secs (%g gens/sec).", gens, secs, gens/secs);
    DisplayMessage(s);
}

// -----------------------------------------------------------------------------

bool StartGenerating()
{
    if (generating) Warning("Bug detected in StartGenerating!");

    lifealgo* curralgo = currlayer->algo;
    if (curralgo->isEmpty()) {
        ErrorMessage(empty_pattern);
        return false;
    }

    if (!SaveStartingPattern()) {
        return false;
    }

    if (allowundo) currlayer->undoredo->RememberGenStart();

    // only show hashing info while generating
    lifealgo::setVerbose(currlayer->showhashinfo);

    // for DisplayTimingInfo
    begintime = TimeInSeconds();
    begingen = curralgo->getGeneration().todouble();

    generating = true;

    PollerReset();

    // caller will start a repeating timer
    return true;
}

// -----------------------------------------------------------------------------

void StopGenerating()
{
    if (!generating) Warning("Bug detected in StopGenerating!");
    
    generating = false;

    PollerInterrupt();

    if (showtiming) DisplayTimingInfo();
    lifealgo::setVerbose(0);

    if (event_checker > 0) {
        // we're currently in the event poller somewhere inside step(), so we must let
        // step() complete and only call RememberGenFinish at the end of NextGeneration
    } else {
        if (allowundo) currlayer->undoredo->RememberGenFinish();
    }

    // caller will stop the timer
}

// -----------------------------------------------------------------------------

void NextGeneration(bool useinc)
{
    lifealgo* curralgo = currlayer->algo;
    bool boundedgrid = curralgo->unbounded && (curralgo->gridwd > 0 || curralgo->gridht > 0);

    if (generating) {
        // we were called via timer so StartGenerating has already checked
        // if the pattern is empty, etc (note that useinc is also true)

    } else {
        // we were called via Next/Step button
        if (curralgo->isEmpty()) {
            ErrorMessage(empty_pattern);
            return;
        }

        if (!SaveStartingPattern()) {
            return;
        }

        if (allowundo) currlayer->undoredo->RememberGenStart();

        // only show hashing info while generating
        lifealgo::setVerbose(currlayer->showhashinfo);

        if (useinc && curralgo->getIncrement() > bigint::one) {
            // for DisplayTimingInfo
            begintime = TimeInSeconds();
            begingen = curralgo->getGeneration().todouble();
        }

        PollerReset();
    }

    if (useinc) {
        // step by current increment
        if (boundedgrid) {
            // temporarily set the increment to 1 so we can call CreateBorderCells()
            // and DeleteBorderCells() around each step()
            int savebase = currlayer->currbase;
            int saveexpo = currlayer->currexpo;
            bigint inc = curralgo->getIncrement();
            curralgo->setIncrement(1);
            while (inc > 0) {
                if (Poller()->checkevents()) break;
                if (savebase != currlayer->currbase || saveexpo != currlayer->currexpo) {
                    // user changed step base/exponent, so reset increment to 1
                    inc = curralgo->getIncrement();
                    curralgo->setIncrement(1);
                }
                if (!curralgo->CreateBorderCells()) break;
                curralgo->step();
                if (!curralgo->DeleteBorderCells()) break;
                inc -= 1;
            }
            // safe way to restore correct increment in case user altered base/expo in above loop
            SetGenIncrement();
        } else {
            curralgo->step();
        }
    } else {
        // step by 1 gen
        bigint saveinc = curralgo->getIncrement();
        curralgo->setIncrement(1);
        if (boundedgrid) curralgo->CreateBorderCells();
        curralgo->step();
        if (boundedgrid) curralgo->DeleteBorderCells();
        curralgo->setIncrement(saveinc);
    }

    if (!generating) {
        if (showtiming && useinc && curralgo->getIncrement() > bigint::one) DisplayTimingInfo();
        lifealgo::setVerbose(0);
        if (allowundo) currlayer->undoredo->RememberGenFinish();
    }

    // autofit is only used when doing many gens
    if (currlayer->autofit && (generating || useinc)) {
        FitInView(0);
    }

    if (draw_pending) {
        draw_pending = false;
        TouchBegan(pendingx, pendingy);
    }
}

// -----------------------------------------------------------------------------

void ClearOutsideGrid()
{
    // check current pattern and clear any live cells outside bounded grid
    bool patternchanged = false;
    bool savechanges = allowundo && !currlayer->stayclean;

    // might also need to truncate selection
    currlayer->currsel.CheckGridEdges();

    if (!currlayer->algo->unbounded) {
        if (currlayer->algo->clipped_cells.size() > 0) {
            // cells outside the grid were clipped
            if (savechanges) {
                for (size_t i = 0; i < currlayer->algo->clipped_cells.size(); i += 3) {
                    int x = currlayer->algo->clipped_cells[i];
                    int y = currlayer->algo->clipped_cells[i+1];
                    int s = currlayer->algo->clipped_cells[i+2];
                    currlayer->undoredo->SaveCellChange(x, y, s, 0);
                }
            }
            currlayer->algo->clipped_cells.clear();
            patternchanged = true;
        }

    } else {
        // algo uses an unbounded grid
        if (currlayer->algo->isEmpty()) return;
    
        // check if current pattern is too big to use nextcell/setcell
        bigint top, left, bottom, right;
        currlayer->algo->findedges(&top, &left, &bottom, &right);
        if ( OutsideLimits(top, left, bottom, right) ) {
            ErrorMessage("Pattern too big to check (outside +/- 10^9 boundary).");
            return;
        }
    
        int itop = top.toint();
        int ileft = left.toint();
        int ibottom = bottom.toint();
        int iright = right.toint();
    
        // no need to do anything if pattern is entirely within grid
        int gtop = currlayer->algo->gridtop.toint();
        int gleft = currlayer->algo->gridleft.toint();
        int gbottom = currlayer->algo->gridbottom.toint();
        int gright = currlayer->algo->gridright.toint();
        if (currlayer->algo->gridwd == 0) {
            // grid has infinite width
            gleft = INT_MIN;
            gright = INT_MAX;
        }
        if (currlayer->algo->gridht == 0) {
            // grid has infinite height
            gtop = INT_MIN;
            gbottom = INT_MAX;
        }
        if (itop >= gtop && ileft >= gleft && ibottom <= gbottom && iright <= gright) {
            return;
        }
    
        int ht = ibottom - itop + 1;
        int cx, cy;
    
        // for showing accurate progress we need to add pattern height to pop count
        // in case this is a huge pattern with many blank rows
        double maxcount = currlayer->algo->getPopulation().todouble() + ht;
        double accumcount = 0;
        int currcount = 0;
        bool abort = false;
        int v = 0;
        BeginProgress("Checking cells outside grid");
    
        lifealgo* curralgo = currlayer->algo;
        for ( cy=itop; cy<=ibottom; cy++ ) {
            currcount++;
            for ( cx=ileft; cx<=iright; cx++ ) {
                int skip = curralgo->nextcell(cx, cy, v);
                if (skip >= 0) {
                    // found next live cell in this row
                    cx += skip;
                    if (cx < gleft || cx > gright || cy < gtop || cy > gbottom) {
                        // clear cell outside grid
                        if (savechanges) currlayer->undoredo->SaveCellChange(cx, cy, v, 0);
                        curralgo->setcell(cx, cy, 0);
                        patternchanged = true;
                    }
                    currcount++;
                } else {
                    cx = iright;  // done this row
                }
                if (currcount > 1024) {
                    accumcount += currcount;
                    currcount = 0;
                    abort = AbortProgress(accumcount / maxcount, "");
                    if (abort) break;
                }
            }
            if (abort) break;
        }
    
        curralgo->endofpattern();
        EndProgress();
    }
    
    if (patternchanged) {
        ErrorMessage("Pattern was truncated (live cells were outside grid).");
    }
}

// -----------------------------------------------------------------------------

void ReduceCellStates(int newmaxstate)
{
    // check current pattern and reduce any cell states > newmaxstate
    bool patternchanged = false;
    bool savechanges = allowundo && !currlayer->stayclean;

    // check if current pattern is too big to use nextcell/setcell
    bigint top, left, bottom, right;
    currlayer->algo->findedges(&top, &left, &bottom, &right);
    if ( OutsideLimits(top, left, bottom, right) ) {
        ErrorMessage("Pattern too big to check (outside +/- 10^9 boundary).");
        return;
    }

    int itop = top.toint();
    int ileft = left.toint();
    int ibottom = bottom.toint();
    int iright = right.toint();
    int ht = ibottom - itop + 1;
    int cx, cy;

    // for showing accurate progress we need to add pattern height to pop count
    // in case this is a huge pattern with many blank rows
    double maxcount = currlayer->algo->getPopulation().todouble() + ht;
    double accumcount = 0;
    int currcount = 0;
    bool abort = false;
    int v = 0;
    BeginProgress("Checking cell states");

    lifealgo* curralgo = currlayer->algo;
    for ( cy=itop; cy<=ibottom; cy++ ) {
        currcount++;
        for ( cx=ileft; cx<=iright; cx++ ) {
            int skip = curralgo->nextcell(cx, cy, v);
            if (skip >= 0) {
                // found next live cell in this row
                cx += skip;
                if (v > newmaxstate) {
                    // reduce cell's current state to largest state
                    if (savechanges) currlayer->undoredo->SaveCellChange(cx, cy, v, newmaxstate);
                    curralgo->setcell(cx, cy, newmaxstate);
                    patternchanged = true;
                }
                currcount++;
            } else {
                cx = iright;  // done this row
            }
            if (currcount > 1024) {
                accumcount += currcount;
                currcount = 0;
                abort = AbortProgress(accumcount / maxcount, "");
                if (abort) break;
            }
        }
        if (abort) break;
    }

    curralgo->endofpattern();
    EndProgress();

    if (patternchanged) {
        ErrorMessage("Pattern has changed (new rule has fewer states).");
    }
}

// -----------------------------------------------------------------------------

void ChangeRule(const std::string& rulestring)
{
    std::string oldrule = currlayer->algo->getrule();
    int oldmaxstate = currlayer->algo->NumCellStates() - 1;

    // selection might change if grid becomes smaller,
    // so save current selection for RememberRuleChange/RememberAlgoChange
    SaveCurrentSelection();

    const char* err = currlayer->algo->setrule( rulestring.c_str() );
    if (err) {
        // try to find another algorithm that supports the given rule
        for (int i = 0; i < NumAlgos(); i++) {
            if (i != currlayer->algtype) {
                lifealgo* tempalgo = CreateNewUniverse(i);
                err = tempalgo->setrule( rulestring.c_str() );
                delete tempalgo;
                if (!err) {
                    // change the current algorithm and switch to the new rule
                    ChangeAlgorithm(i, rulestring.c_str());
                    if (i != currlayer->algtype) {
                        RestoreRule(oldrule.c_str());
                        Warning("Algorithm could not be changed (pattern is too big to convert).");
                    } else {
                        UpdateEverything();
                    }
                    return;
                }
            }
        }
        // should only get here if .rule file contains some sort of error
        RestoreRule(oldrule.c_str());
        Warning("New rule is not valid in any algorithm!");
        return;
    }

    std::string newrule = currlayer->algo->getrule();
    int newmaxstate = currlayer->algo->NumCellStates() - 1;
    if (oldrule != newrule || oldmaxstate != newmaxstate) {
        UpdateStatus();
        
		// if pattern exists and is at starting gen then ensure savestart is true
		// so that SaveStartingPattern will save pattern to suitable file
		// (and thus undo/reset will work correctly)
		if (currlayer->algo->getGeneration() == currlayer->startgen && !currlayer->algo->isEmpty()) {
			currlayer->savestart = true;
		}

        // if grid is bounded then remove any live cells outside grid edges
        if (currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0) {
            ClearOutsideGrid();
        }

		// new rule might have changed the number of cell states;
		// if there are fewer states then pattern might change
		if (newmaxstate < oldmaxstate && !currlayer->algo->isEmpty()) {
			ReduceCellStates(newmaxstate);
		}

        if (allowundo && !currlayer->stayclean) {
            currlayer->undoredo->RememberRuleChange(oldrule.c_str());
        }
    }

    // set colors and icons for new rule
    UpdateLayerColors();

    // pattern or colors or icons might have changed
    UpdateEverything();
}

// -----------------------------------------------------------------------------

void ChangeAlgorithm(algo_type newalgotype, const char* newrule, bool inundoredo)
{
    if (newalgotype == currlayer->algtype) return;

    // check if current pattern is too big to use nextcell/setcell
    bigint top, left, bottom, right;
    if ( !currlayer->algo->isEmpty() ) {
        currlayer->algo->findedges(&top, &left, &bottom, &right);
        if ( OutsideLimits(top, left, bottom, right) ) {
            ErrorMessage("Pattern cannot be converted (outside +/- 10^9 boundary).");
            return;
        }
    }

    // save changes if undo/redo is enabled and script isn't constructing a pattern
    // and we're not undoing/redoing an earlier algo change
    bool savechanges = allowundo && !currlayer->stayclean && !inundoredo;
    if (savechanges && inscript) {
        // note that we must save pending gen changes BEFORE changing algo type
        // otherwise temporary files won't be the correct type (mc or rle)
        //!!! SavePendingChanges();
    }

    // selection might change if grid becomes smaller,
    // so save current selection for RememberAlgoChange
    if (savechanges) SaveCurrentSelection();

    bool rulechanged = false;
    std::string oldrule = currlayer->algo->getrule();

    // change algorithm type, reset step size, and update status bar
    algo_type oldalgo = currlayer->algtype;
    currlayer->algtype = newalgotype;
    currlayer->currbase = algoinfo[newalgotype]->defbase;
    currlayer->currexpo = 0;
    UpdateStatus();

    // create a new universe of the requested flavor
    lifealgo* newalgo = CreateNewUniverse(newalgotype);

    if (inundoredo) {
        // switch to given newrule
        const char* err = newalgo->setrule(newrule);
        if (err) newalgo->setrule(newalgo->DefaultRule());
    } else {
        const char* err;
        if (newrule[0] == 0) {
            // try to use same rule
            err = newalgo->setrule(currlayer->algo->getrule());
        } else {
            // switch to newrule
            err = newalgo->setrule(newrule);
            rulechanged = true;
        }
        if (err) {
            std::string defrule = newalgo->DefaultRule();
            size_t oldpos = oldrule.find(':');
            if (newrule[0] == 0 && oldpos != std::string::npos) {
                // switch to new algo's default rule, but preserve the topology in oldrule
                // so we can do things like switch from "LifeHistory:T30,20" in RuleLoader
                // to "B3/S23:T30,20" in QuickLife
                size_t defpos = defrule.find(':');
                if (defpos != std::string::npos) {
                    // default rule shouldn't have a suffix but play safe and remove it
                    defrule = defrule.substr(0, defpos);
                }
                defrule += ":";
                defrule += oldrule.substr(oldpos+1);
            }
            err = newalgo->setrule(defrule.c_str());
            // shouldn't ever fail but play safe
            if (err) newalgo->setrule( newalgo->DefaultRule() );
            rulechanged = true;
        }
    }

    // set same gen count
    newalgo->setGeneration( currlayer->algo->getGeneration() );

    bool patternchanged = false;
    if ( !currlayer->algo->isEmpty() ) {
        // copy pattern in current universe to new universe
        int itop = top.toint();
        int ileft = left.toint();
        int ibottom = bottom.toint();
        int iright = right.toint();
        int ht = ibottom - itop + 1;
        int cx, cy;

        // for showing accurate progress we need to add pattern height to pop count
        // in case this is a huge pattern with many blank rows
        double maxcount = currlayer->algo->getPopulation().todouble() + ht;
        double accumcount = 0;
        int currcount = 0;
        bool abort = false;
        int v = 0;
        BeginProgress("Converting pattern");

        // set newalgo's grid edges so we can save cells that are outside grid
        int gtop = newalgo->gridtop.toint();
        int gleft = newalgo->gridleft.toint();
        int gbottom = newalgo->gridbottom.toint();
        int gright = newalgo->gridright.toint();
        if (newalgo->gridwd == 0) {
            // grid has infinite width
            gleft = INT_MIN;
            gright = INT_MAX;
        }
        if (newalgo->gridht == 0) {
            // grid has infinite height
            gtop = INT_MIN;
            gbottom = INT_MAX;
        }

        // need to check for state change if new algo has fewer states than old algo
        int newmaxstate = newalgo->NumCellStates() - 1;

        lifealgo* curralgo = currlayer->algo;
        for ( cy=itop; cy<=ibottom; cy++ ) {
            currcount++;
            for ( cx=ileft; cx<=iright; cx++ ) {
                int skip = curralgo->nextcell(cx, cy, v);
                if (skip >= 0) {
                    // found next live cell in this row
                    cx += skip;
                    if (cx < gleft || cx > gright || cy < gtop || cy > gbottom) {
                        // cx,cy is outside grid
                        if (savechanges) currlayer->undoredo->SaveCellChange(cx, cy, v, 0);
                        // no need to clear cell from curralgo (that universe will soon be deleted)
                        patternchanged = true;
                    } else {
                        if (v > newmaxstate) {
                            // reduce v to largest state in new algo
                            if (savechanges) currlayer->undoredo->SaveCellChange(cx, cy, v, newmaxstate);
                            v = newmaxstate;
                            patternchanged = true;
                        }
                        newalgo->setcell(cx, cy, v);
                    }
                    currcount++;
                } else {
                    cx = iright;  // done this row
                }
                if (currcount > 1024) {
                    accumcount += currcount;
                    currcount = 0;
                    abort = AbortProgress(accumcount / maxcount, "");
                    if (abort) break;
                }
            }
            if (abort) break;
        }

        newalgo->endofpattern();
        EndProgress();
    }

    // delete old universe and point current universe to new universe
    delete currlayer->algo;
    currlayer->algo = newalgo;
    SetGenIncrement();

    // if new grid is bounded then we might need to truncate the selection
    if (currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0) {
        currlayer->currsel.CheckGridEdges();
    }

    // switch to default colors for new algo+rule
    UpdateLayerColors();

    if (!inundoredo) {
        // if pattern exists and is at starting gen then set savestart true
        // so that SaveStartingPattern will save pattern to suitable file
        // (and thus ResetPattern will work correctly)
        if (currlayer->algo->getGeneration() == currlayer->startgen && !currlayer->algo->isEmpty()) {
            currlayer->savestart = true;
        }

        if (rulechanged) {
            if (newrule[0] == 0) {
                if (patternchanged) {
                    ErrorMessage("Rule has changed and pattern has changed.");
                } else {
                    // don't beep
                    DisplayMessage("Rule has changed.");
                }
            } else {
                if (patternchanged) {
                    ErrorMessage("Algorithm has changed and pattern has changed.");
                } else {
                    // don't beep
                    DisplayMessage("Algorithm has changed.");
                }
            }
        } else if (patternchanged) {
            ErrorMessage("Pattern has changed.");
        }
    }

    if (savechanges) {
        currlayer->undoredo->RememberAlgoChange(oldalgo, oldrule.c_str());
    }

    if (!inundoredo && !inscript) {
        // do this AFTER RememberAlgoChange so Undo button becomes enabled
        UpdateEverything();
    }
}
