/*** /
 
 This file is part of Golly, a Game of Life Simulator.
 Copyright (C) 2012 Andrew Trevorrow and Tomas Rokicki.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 
 Web site:  http://sourceforge.net/projects/golly
 Authors:   rokicki@gmail.com  andrew@trevorrow.com
 
 / ***/

#include "wx/wxprec.h"      // for compilers that support precompilation
#ifndef WX_PRECOMP
    #include "wx/wx.h"      // for all others include the necessary headers
#endif

#include "wx/dir.h"         // for wxDir
#include "wx/file.h"        // for wxFile

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "util.h"           // for linereader

#include "wxgolly.h"        // for wxGetApp, statusptr, viewptr, bigview
#include "wxutils.h"        // for BeginProgress, GetString, etc
#include "wxprefs.h"        // for allowundo, etc
#include "wxrule.h"         // for ChangeRule
#include "wxhelp.h"         // for LoadLexiconPattern
#include "wxstatus.h"       // for statusptr->...
#include "wxselect.h"       // for Selection
#include "wxview.h"         // for viewptr->...
#include "wxscript.h"       // for inscript, PassKeyToScript
#include "wxmain.h"         // for MainFrame
#include "wxundo.h"         // for undoredo->...
#include "wxalgos.h"        // for *_ALGO, algo_type, CreateNewUniverse, etc
#include "wxlayer.h"        // for currlayer, etc
#include "wxrender.h"       // for DrawView
#include "wxtimeline.h"     // for TimelineExists, UpdateTimelineBar, etc

#include <stdexcept>        // for std::runtime_error and std::exception
#include <sstream>          // for std::ostringstream

// This module implements Control menu functions.

// -----------------------------------------------------------------------------

bool MainFrame::SaveStartingPattern()
{
    if ( currlayer->algo->getGeneration() > currlayer->startgen ) {
        // don't do anything if current gen count > starting gen
        return true;
    }
    
    // save current rule, dirty flag, scale, location, etc
    currlayer->startname = currlayer->currname;
    currlayer->startrule = wxString(currlayer->algo->getrule(), wxConvLocal);
    currlayer->startdirty = currlayer->dirty;
    currlayer->startmag = viewptr->GetMag();
    viewptr->GetPos(currlayer->startx, currlayer->starty);
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
        // no need to save pattern; ResetPattern will load currfile
        currlayer->startfile.Clear();
        return true;
    }
    
    // save starting pattern in tempstart file
    // use CanWriteFormat(MC_format)???
    if ( currlayer->algo->hyperCapable() ) {
        // much faster to save pattern in a macrocell file
        const char* err = WritePattern(currlayer->tempstart, MC_format,
                                       no_compression, 0, 0, 0, 0);
        if (err) {
            statusptr->ErrorMessage(wxString(err,wxConvLocal));
            // don't allow user to continue generating
            return false;
        }
    } else {
        // can only save as RLE if edges are within getcell/setcell limits
        bigint top, left, bottom, right;
        currlayer->algo->findedges(&top, &left, &bottom, &right);      
        if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
            statusptr->ErrorMessage(_("Starting pattern is outside +/- 10^9 boundary."));
            // don't allow user to continue generating
            return false;
        }
        int itop = top.toint();
        int ileft = left.toint();
        int ibottom = bottom.toint();
        int iright = right.toint();      
        // use XRLE format so the pattern's top left location and the current
        // generation count are stored in the file
        const char* err = WritePattern(currlayer->tempstart, XRLE_format,
                                       no_compression,
                                       itop, ileft, ibottom, iright);
        if (err) {
            statusptr->ErrorMessage(wxString(err,wxConvLocal));
            // don't allow user to continue generating
            return false;
        }
    }
    
    currlayer->startfile = currlayer->tempstart;   // ResetPattern will load tempstart
    return true;
}

// -----------------------------------------------------------------------------

void MainFrame::ResetPattern(bool resetundo)
{
    if (currlayer->algo->getGeneration() == currlayer->startgen) return;
    
    if (generating) {
        // terminate generating loop and set command_pending flag
        Stop();
        command_pending = true;
        cmdevent.SetId(ID_RESET);
        /* can't use wxPostEvent here because Yield processes all pending events:
        // send Reset command to event queue
        wxCommandEvent resetevt(wxEVT_COMMAND_MENU_SELECTED, ID_RESET);
        wxPostEvent(this->GetEventHandler(), resetevt);
        */
        return;
    }
    
    if (inscript) stop_after_script = true;
    
    if (currlayer->algo->getGeneration() < currlayer->startgen) {
        // if this happens then startgen logic is wrong
        Warning(_("Current gen < starting gen!"));
        return;
    }
    
    if (currlayer->startfile.IsEmpty() && currlayer->currfile.IsEmpty()) {
        // if this happens then savestart logic is wrong
        Warning(_("Starting pattern cannot be restored!"));
        return;
    }
    
    if (allowundo && !currlayer->stayclean && inscript) {
        // script called reset()
        SavePendingChanges();
        currlayer->undoredo->RememberGenStart();
    }
    
    // save current algo and rule
    algo_type oldalgo = currlayer->algtype;
    wxString oldrule = wxString(currlayer->algo->getrule(), wxConvLocal);
    
    // restore pattern and settings saved by SaveStartingPattern;
    // first restore algorithm
    currlayer->algtype = currlayer->startalgo;
    
    // restore starting pattern
    if ( currlayer->startfile.IsEmpty() ) {
        // restore pattern from currfile
        LoadPattern(currlayer->currfile, wxEmptyString);
    } else {
        // restore pattern from startfile
        LoadPattern(currlayer->startfile, wxEmptyString);
    }
    
    if (currlayer->algo->getGeneration() != currlayer->startgen) {
        // LoadPattern failed to reset the gen count to startgen
        // (probably because the user deleted the starting pattern)
        // so best to clear the pattern and reset the gen count
        CreateUniverse();
        currlayer->algo->setGeneration(currlayer->startgen);
    }
    
    // ensure savestart flag is correct
    currlayer->savestart = !currlayer->startfile.IsEmpty();
    
    // restore settings saved by SaveStartingPattern
    RestoreRule(currlayer->startrule);
    currlayer->currname = currlayer->startname;
    currlayer->dirty = currlayer->startdirty;
    if (restoreview) {
        viewptr->SetPosMag(currlayer->startx, currlayer->starty, currlayer->startmag);
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
                UpdateLayerItem(i);
            }
        }
    }
    
    // restore selection
    currlayer->currsel = currlayer->startsel;
    
    // switch to default colors if algo/rule changed
    wxString newrule = wxString(currlayer->algo->getrule(), wxConvLocal);
    if (oldalgo != currlayer->algtype || oldrule != newrule) {
        UpdateLayerColors();
    }
    
    // update window title in case currname, rule or dirty flag changed;
    // note that UpdateLayerItem(currindex) gets called
    SetWindowTitle(currlayer->currname);
    UpdateEverything();
    
    if (allowundo && !currlayer->stayclean) {
        if (inscript) {
            // script called reset() so remember gen change
            // (RememberGenStart was called above)
            currlayer->undoredo->RememberGenFinish();
        } else if (resetundo) {
            // wind back the undo history to the starting pattern
            currlayer->undoredo->SyncUndoHistory();
        }
    }
}

// -----------------------------------------------------------------------------

void MainFrame::RestorePattern(bigint& gen, const wxString& filename,
                               bigint& x, bigint& y, int mag, int base, int expo)
{
    // called to undo/redo a generating change
    if (gen == currlayer->startgen) {
        // restore starting pattern (false means don't call SyncUndoHistory)
        ResetPattern(false);
    } else {
        // restore pattern in given filename;
        // false means don't update status bar (algorithm should NOT change)
        LoadPattern(filename, wxEmptyString, false);
        
        if (currlayer->algo->getGeneration() != gen) {
            // filename could not be loaded for some reason,
            // so best to clear the pattern and set the expected gen count
            CreateUniverse();
            currlayer->algo->setGeneration(gen);
        }
        
        // restore step size and set increment
        currlayer->currbase = base;
        currlayer->currexpo = expo;
        SetGenIncrement();
        
        // restore position and scale, if allowed
        if (restoreview) viewptr->SetPosMag(x, y, mag);
        
        UpdatePatternAndStatus();
    }
}

// -----------------------------------------------------------------------------

const char* MainFrame::ChangeGenCount(const char* genstring, bool inundoredo)
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
    if (inscript) stop_after_script = true;
    
    if (newgen == oldgen) return NULL;
    
    if (!inundoredo && allowundo && !currlayer->stayclean && inscript) {
        // script called setgen()
        SavePendingChanges();
    }
    
    // need IsParityShifted() method???
    if (currlayer->algtype == QLIFE_ALGO && newgen.odd() != oldgen.odd()) {
        // qlife stores pattern in different bits depending on gen parity,
        // so we need to create a new qlife universe, set its gen, copy the
        // current pattern to the new universe, then switch to that universe
        bigint top, left, bottom, right;
        currlayer->algo->findedges(&top, &left, &bottom, &right);
        if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
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
        if ( !viewptr->CopyRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                                currlayer->algo, newalgo, false, _("Copying pattern")) ) {
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

void MainFrame::SetGeneration()
{
    if (generating) {
        // terminate generating loop and set command_pending flag
        Stop();
        command_pending = true;
        cmdevent.SetId(ID_SETGEN);
        return;
    }
    
    bigint oldgen = currlayer->algo->getGeneration();
    wxString result;
    wxString prompt = _("Enter a new generation count:");
    prompt += _("\n(+n/-n is relative to current count)");
    if ( GetString(_("Set Generation"), prompt,
                   wxString(oldgen.tostring(), wxConvLocal), result) ) {
        
        const char* err = ChangeGenCount(result.mb_str(wxConvLocal));
        
        if (err) {
            Warning(wxString(err,wxConvLocal));
        } else {
            // Reset/Undo/Redo items might become enabled or disabled
            // (we need to do this if user clicked "Generation=..." text)
            UpdateMenuItems(IsActive());
        }
    }
}

// -----------------------------------------------------------------------------

void MainFrame::GoFaster()
{
    if (TimelineExists()) {
        PlayTimelineFaster();
    } else {
        currlayer->currexpo++;
        SetGenIncrement();
        // only need to refresh status bar
        UpdateStatus();
        if (generating && currlayer->currexpo < 0) {
            whentosee -= statusptr->GetCurrentDelay();
        }
    }
}

// -----------------------------------------------------------------------------

void MainFrame::GoSlower()
{
    if (TimelineExists()) {
        PlayTimelineSlower();
    } else {
        if (currlayer->currexpo > minexpo) {
            currlayer->currexpo--;
            SetGenIncrement();
            // only need to refresh status bar
            UpdateStatus();
            if (generating && currlayer->currexpo < 0) {
                if (currlayer->currexpo == -1) {
                    // need to initialize whentosee rather than increment it
                    whentosee = stopwatch->Time() + statusptr->GetCurrentDelay();
                } else {
                    whentosee += statusptr->GetCurrentDelay();
                }
            }
        } else {
            Beep();
        }
    }
}

// -----------------------------------------------------------------------------

void MainFrame::SetBaseStep()
{
    int i;
    if ( GetInteger(_("Set Base Step"),
                    _("Temporarily change the current base step:"),
                    currlayer->currbase, 2, MAX_BASESTEP, &i) ) {
        currlayer->currbase = i;
        SetGenIncrement();
        UpdateStatus();
    }
}

// -----------------------------------------------------------------------------

void MainFrame::DisplayPattern()
{
    // this routine is similar to UpdatePatternAndStatus() but if tiled windows
    // exist it only updates the current tile if possible; ie. it's not a clone
    // and tile views aren't synchronized
    
    if (tilelayers && numlayers > 1 && !syncviews && currlayer->cloneid == 0) {
        // only update the current tile
        viewptr->Refresh(false);
#ifdef __WXMAC__
        if (!showstatus) viewptr->Update();
        // else let statusptr->Update() update viewport
#else
        viewptr->Update();
#endif
    } else {
        // update main viewport window, possibly including all tile windows
        // (tile windows are children of bigview)
        if (numlayers > 1 && (stacklayers || tilelayers)) {
            bigview->Refresh(false);
#ifdef __WXMAC__
            if (!showstatus) bigview->Update();
            // else let statusptr->Update() update viewport
#else
            bigview->Update();
#endif
        } else {
            viewptr->Refresh(false);
#ifdef __WXMAC__
            if (!showstatus) viewptr->Update();
            // else let statusptr->Update() update viewport
#else
            viewptr->Update();
#endif
        }
    }
    
    if (showstatus) {
        statusptr->CheckMouseLocation(IsActive());
        statusptr->Refresh(false);
        statusptr->Update();
    }
}

// -----------------------------------------------------------------------------

static void JoinTwistedEdges(lifealgo* curralgo)
{
    // set grid edges
    int gl = curralgo->gridleft.toint();
    int gt = curralgo->gridtop.toint();
    int gr = curralgo->gridright.toint();
    int gb = curralgo->gridbottom.toint();
    
    // border edges are 1 cell outside grid edges
    int bl = gl - 1;
    int bt = gt - 1;
    int br = gr + 1;
    int bb = gb + 1;
    
    if (curralgo->htwist && curralgo->vtwist) {
        // cross-surface
        //  eg. :C4,3
        //  a l k j i d
        //  l A B C D i
        //  h E F G H e
        //  d I J K L a
        //  i d c b a l
        
        for (int x = gl; x <= gr; x++) {
            int twistedx = gr - x + gl;
            int state = curralgo->getcell(twistedx, gt);
            if (state > 0) curralgo->setcell(x, bb, state);
            state = curralgo->getcell(twistedx, gb);
            if (state > 0) curralgo->setcell(x, bt, state);
        }
        
        for (int y = gt; y <= gb; y++) {
            int twistedy = gb - y + gt;
            int state = curralgo->getcell(gl, twistedy);
            if (state > 0) curralgo->setcell(br, y, state);
            state = curralgo->getcell(gr, twistedy);
            if (state > 0) curralgo->setcell(bl, y, state);
        }
        
        // copy grid's corner cells to SAME corners in border
        // (these cells are topologically different to non-corner cells)
        curralgo->setcell(bl, bt, curralgo->getcell(gl, gt));
        curralgo->setcell(br, bt, curralgo->getcell(gr, gt));
        curralgo->setcell(br, bb, curralgo->getcell(gr, gb));
        curralgo->setcell(bl, bb, curralgo->getcell(gl, gb));
        
    } else if (curralgo->htwist) {
        // Klein bottle with top and bottom edges twisted 180 degrees
        //  eg. :K4*,3
        //  i l k j i l
        //  d A B C D a
        //  h E F G H e
        //  l I J K L i
        //  a d c b a d
        
        for (int x = gl; x <= gr; x++) {
            int twistedx = gr - x + gl;
            int state = curralgo->getcell(twistedx, gt);
            if (state > 0) curralgo->setcell(x, bb, state);
            state = curralgo->getcell(twistedx, gb);
            if (state > 0) curralgo->setcell(x, bt, state);
        }
        
        for (int y = gt; y <= gb; y++) {
            // join left and right edges with no twist
            int state = curralgo->getcell(gl, y);
            if (state > 0) curralgo->setcell(br, y, state);
            state = curralgo->getcell(gr, y);
            if (state > 0) curralgo->setcell(bl, y, state);
        }
        
        // do corner cells
        curralgo->setcell(bl, bt, curralgo->getcell(gl, gb));
        curralgo->setcell(br, bt, curralgo->getcell(gr, gb));
        curralgo->setcell(bl, bb, curralgo->getcell(gl, gt));
        curralgo->setcell(br, bb, curralgo->getcell(gr, gt));
        
    } else { // curralgo->vtwist
        // Klein bottle with left and right edges twisted 180 degrees
        //  eg. :K4,3*
        //  d i j k l a
        //  l A B C D i
        //  h E F G H e
        //  d I J K L a
        //  l a b c d i
        
        for (int x = gl; x <= gr; x++) {
            // join top and bottom edges with no twist
            int state = curralgo->getcell(x, gt);
            if (state > 0) curralgo->setcell(x, bb, state);
            state = curralgo->getcell(x, gb);
            if (state > 0) curralgo->setcell(x, bt, state);
        }
        
        for (int y = gt; y <= gb; y++) {
            int twistedy = gb - y + gt;
            int state = curralgo->getcell(gl, twistedy);
            if (state > 0) curralgo->setcell(br, y, state);
            state = curralgo->getcell(gr, twistedy);
            if (state > 0) curralgo->setcell(bl, y, state);
        }
        
        // do corner cells
        curralgo->setcell(bl, bt, curralgo->getcell(gr, gt));
        curralgo->setcell(br, bt, curralgo->getcell(gl, gt));
        curralgo->setcell(bl, bb, curralgo->getcell(gr, gb));
        curralgo->setcell(br, bb, curralgo->getcell(gl, gb));
    }
}

// -----------------------------------------------------------------------------

static void JoinTwistedAndShiftedEdges(lifealgo* curralgo)
{
    // set grid edges
    int gl = curralgo->gridleft.toint();
    int gt = curralgo->gridtop.toint();
    int gr = curralgo->gridright.toint();
    int gb = curralgo->gridbottom.toint();
    
    // border edges are 1 cell outside grid edges
    int bl = gl - 1;
    int bt = gt - 1;
    int br = gr + 1;
    int bb = gb + 1;
    
    if (curralgo->hshift != 0) {
        // Klein bottle with shift by 1 on twisted horizontal edge (with even number of cells)
        //  eg. :K4*+1,3
        //  j i l k j i
        //  d A B C D a
        //  h E F G H e
        //  l I J K L i
        //  b a d c b a
        
        int state, twistedx, shiftedx;
        
        for (int x = gl; x <= gr; x++) {
            // join top and bottom edges with a twist and then shift by 1
            twistedx = gr - x + gl;
            shiftedx = twistedx - 1; if (shiftedx < gl) shiftedx = gr;
            state = curralgo->getcell(shiftedx, gb);
            if (state > 0) curralgo->setcell(x, bt, state);
            
            state = curralgo->getcell(shiftedx, gt);
            if (state > 0) curralgo->setcell(x, bb, state);
        }
        
        for (int y = gt; y <= gb; y++) {
            // join left and right edges with no twist or shift
            state = curralgo->getcell(gl, y);
            if (state > 0) curralgo->setcell(br, y, state);
            state = curralgo->getcell(gr, y);
            if (state > 0) curralgo->setcell(bl, y, state);
        }
        
        // do corner cells
        shiftedx = gl - 1; if (shiftedx < gl) shiftedx = gr;
        curralgo->setcell(bl, bt, curralgo->getcell(shiftedx, gb));
        curralgo->setcell(bl, bb, curralgo->getcell(shiftedx, gt));
        shiftedx = gr - 1; if (shiftedx < gl) shiftedx = gr;
        curralgo->setcell(br, bt, curralgo->getcell(shiftedx, gb));
        curralgo->setcell(br, bb, curralgo->getcell(shiftedx, gt));
        
    } else { // curralgo->vshift != 0
        // Klein bottle with shift by 1 on twisted vertical edge (with even number of cells)
        //  eg. :K3,4*+1
        //  f j k l d
        //  c A B C a
        //  l D E F j
        //  i G H I g
        //  f J K L d
        //  c a b c a
        
        int state, twistedy, shiftedy;
        
        for (int x = gl; x <= gr; x++) {
            // join top and bottom edges with no twist or shift
            state = curralgo->getcell(x, gt);
            if (state > 0) curralgo->setcell(x, bb, state);
            state = curralgo->getcell(x, gb);
            if (state > 0) curralgo->setcell(x, bt, state);
        }
        
        for (int y = gt; y <= gb; y++) {
            // join left and right edges with a twist and then shift by 1
            twistedy = gb - y + gt;
            shiftedy = twistedy - 1; if (shiftedy < gt) shiftedy = gb;
            state = curralgo->getcell(gr, shiftedy);
            if (state > 0) curralgo->setcell(bl, y, state);
            
            state = curralgo->getcell(gl, shiftedy);
            if (state > 0) curralgo->setcell(br, y, state);
        }
        
        // do corner cells
        shiftedy = gt - 1; if (shiftedy < gt) shiftedy = gb;
        curralgo->setcell(bl, bt, curralgo->getcell(gr, shiftedy));
        curralgo->setcell(br, bt, curralgo->getcell(gl, shiftedy));
        shiftedy = gb - 1; if (shiftedy < gt) shiftedy = gb;
        curralgo->setcell(bl, bb, curralgo->getcell(gr, shiftedy));
        curralgo->setcell(br, bb, curralgo->getcell(gl, shiftedy));
    }
}

// -----------------------------------------------------------------------------

static void JoinShiftedEdges(lifealgo* curralgo,
                             int gwd, int ght,        // grid wd and ht
                             int hshift, int vshift)  // horizontal and vertical shifts
{
    // set grid edges
    int gl = curralgo->gridleft.toint();
    int gt = curralgo->gridtop.toint();
    int gr = curralgo->gridright.toint();
    int gb = curralgo->gridbottom.toint();
    
    // border edges are 1 cell outside grid edges
    int bl = gl - 1;
    int bt = gt - 1;
    int br = gr + 1;
    int bb = gb + 1;
    
    if (hshift != 0) {
        // torus with horizontal shift
        //  eg. :T4+1,3
        //  k l i j k l
        //  d A B C D a
        //  h E F G H e
        //  l I J K L i
        //  a b c d a b
        
        int state, shiftedx;
        
        for (int x = gl; x <= gr; x++) {
            // join top and bottom edges with a horizontal shift
            shiftedx = x - hshift;
            if (shiftedx < gl) shiftedx += gwd; else if (shiftedx > gr) shiftedx -= gwd;
            state = curralgo->getcell(shiftedx, gb);
            if (state > 0) curralgo->setcell(x, bt, state);
            
            shiftedx = x + hshift;
            if (shiftedx < gl) shiftedx += gwd; else if (shiftedx > gr) shiftedx -= gwd;
            state = curralgo->getcell(shiftedx, gt);
            if (state > 0) curralgo->setcell(x, bb, state);
        }
        
        for (int y = gt; y <= gb; y++) {
            // join left and right edges with no shift
            state = curralgo->getcell(gl, y);
            if (state > 0) curralgo->setcell(br, y, state);
            
            state = curralgo->getcell(gr, y);
            if (state > 0) curralgo->setcell(bl, y, state);
        }
        
        // do corner cells
        shiftedx = gr - hshift;
        if (shiftedx < gl) shiftedx += gwd; else if (shiftedx > gr) shiftedx -= gwd;
        curralgo->setcell(bl, bt, curralgo->getcell(shiftedx, gb));
        shiftedx = gl - hshift;
        if (shiftedx < gl) shiftedx += gwd; else if (shiftedx > gr) shiftedx -= gwd;
        curralgo->setcell(br, bt, curralgo->getcell(shiftedx, gb));
        shiftedx = gr + hshift;
        if (shiftedx < gl) shiftedx += gwd; else if (shiftedx > gr) shiftedx -= gwd;
        curralgo->setcell(bl, bb, curralgo->getcell(shiftedx, gt));
        shiftedx = gl + hshift;
        if (shiftedx < gl) shiftedx += gwd; else if (shiftedx > gr) shiftedx -= gwd;
        curralgo->setcell(br, bb, curralgo->getcell(shiftedx, gt));
        
    } else { // vshift != 0
        // torus with vertical shift
        //  eg. :T4,3+1
        //  h i j k l a
        //  l A B C D e
        //  d E F G H i
        //  h I J K L a
        //  l a b c d e
        
        int state, shiftedy;
        
        for (int x = gl; x <= gr; x++) {
            // join top and bottom edges with no shift
            state = curralgo->getcell(x, gt);
            if (state > 0) curralgo->setcell(x, bb, state);
            
            state = curralgo->getcell(x, gb);
            if (state > 0) curralgo->setcell(x, bt, state);
        }
        
        for (int y = gt; y <= gb; y++) {
            // join left and right edges with a vertical shift
            shiftedy = y - vshift;
            if (shiftedy < gt) shiftedy += ght; else if (shiftedy > gb) shiftedy -= ght;
            state = curralgo->getcell(gr, shiftedy);
            if (state > 0) curralgo->setcell(bl, y, state);
            
            shiftedy = y + vshift;
            if (shiftedy < gt) shiftedy += ght; else if (shiftedy > gb) shiftedy -= ght;
            state = curralgo->getcell(gl, shiftedy);
            if (state > 0) curralgo->setcell(br, y, state);
        }
        
        // do corner cells
        shiftedy = gb - vshift;
        if (shiftedy < gt) shiftedy += ght; else if (shiftedy > gb) shiftedy -= ght;
        curralgo->setcell(bl, bt, curralgo->getcell(gr, shiftedy));
        shiftedy = gb + vshift;
        if (shiftedy < gt) shiftedy += ght; else if (shiftedy > gb) shiftedy -= ght;
        curralgo->setcell(br, bt, curralgo->getcell(gl, shiftedy));
        shiftedy = gt - vshift;
        if (shiftedy < gt) shiftedy += ght; else if (shiftedy > gb) shiftedy -= ght;
        curralgo->setcell(bl, bb, curralgo->getcell(gr, shiftedy));
        shiftedy = gt + vshift;
        if (shiftedy < gt) shiftedy += ght; else if (shiftedy > gb) shiftedy -= ght;
        curralgo->setcell(br, bb, curralgo->getcell(gl, shiftedy));
    }
}

// -----------------------------------------------------------------------------

static void JoinAdjacentEdges(lifealgo* curralgo,
                              int pt, int pl, int pb, int pr)    // pattern edges
{
    // set grid edges
    int gl = curralgo->gridleft.toint();
    int gt = curralgo->gridtop.toint();
    int gr = curralgo->gridright.toint();
    int gb = curralgo->gridbottom.toint();
    
    // border edges are 1 cell outside grid edges
    int bl = gl - 1;
    int bt = gt - 1;
    int br = gr + 1;
    int bb = gb + 1;
    
    // sphere
    //  eg. :S3
    //  a a d g c
    //  a A B C g
    //  b D E F h
    //  c G H I i
    //  g c f i i
    
    // copy live cells in top edge to left border
    for (int x = pl; x <= pr; x++) {
        int state;
        int skip = curralgo->nextcell(x, gt, state);
        if (skip < 0) break;
        x += skip;
        if (state > 0) curralgo->setcell(bl, gt + (x - gl), state);
    }
    
    // copy live cells in left edge to top border
    for (int y = pt; y <= pb; y++) {
        // no point using nextcell() here -- edge is only 1 cell wide
        int state = curralgo->getcell(gl, y);
        if (state > 0) curralgo->setcell(gl + (y - gt), bt, state);
    }
    
    // copy live cells in bottom edge to right border
    for (int x = pl; x <= pr; x++) {
        int state;
        int skip = curralgo->nextcell(x, gb, state);
        if (skip < 0) break;
        x += skip;
        if (state > 0) curralgo->setcell(br, gt + (x - gl), state);
    }
    
    // copy live cells in right edge to bottom border
    for (int y = pt; y <= pb; y++) {
        // no point using nextcell() here -- edge is only 1 cell wide
        int state = curralgo->getcell(gr, y);
        if (state > 0) curralgo->setcell(gl + (y - gt), bb, state);
    }
    
    // copy grid's corner cells to SAME corners in border
    curralgo->setcell(bl, bt, curralgo->getcell(gl, gt));
    curralgo->setcell(br, bt, curralgo->getcell(gr, gt));
    curralgo->setcell(br, bb, curralgo->getcell(gr, gb));
    curralgo->setcell(bl, bb, curralgo->getcell(gl, gb));
}

// -----------------------------------------------------------------------------

static void JoinEdges(lifealgo* curralgo,
                      int gwd, int ght,                  // grid wd and ht
                      int pt, int pl, int pb, int pr)    // pattern edges
{
    // set grid edges
    int gl = curralgo->gridleft.toint();
    int gt = curralgo->gridtop.toint();
    int gr = curralgo->gridright.toint();
    int gb = curralgo->gridbottom.toint();
    
    // border edges are 1 cell outside grid edges
    int bl = gl - 1;
    int bt = gt - 1;
    int br = gr + 1;
    int bb = gb + 1;
    
    if (ght > 0) {
        // copy live cells in top edge to bottom border
        for (int x = pl; x <= pr; x++) {
            int state;
            int skip = curralgo->nextcell(x, gt, state);
            if (skip < 0) break;
            x += skip;
            if (state > 0) curralgo->setcell(x, bb, state);
        }
        // copy live cells in bottom edge to top border
        for (int x = pl; x <= pr; x++) {
            int state;
            int skip = curralgo->nextcell(x, gb, state);
            if (skip < 0) break;
            x += skip;
            if (state > 0) curralgo->setcell(x, bt, state);
        }
    }
    
    if (gwd > 0) {
        // copy live cells in left edge to right border
        for (int y = pt; y <= pb; y++) {
            // no point using nextcell() here -- edge is only 1 cell wide
            int state = curralgo->getcell(gl, y);
            if (state > 0) curralgo->setcell(br, y, state);
        }
        // copy live cells in right edge to left border
        for (int y = pt; y <= pb; y++) {
            // no point using nextcell() here -- edge is only 1 cell wide
            int state = curralgo->getcell(gr, y);
            if (state > 0) curralgo->setcell(bl, y, state);
        }
    }
    
    if (gwd > 0 && ght > 0) {
        // copy grid's corner cells to opposite corners in border
        curralgo->setcell(bl, bt, curralgo->getcell(gr, gb));
        curralgo->setcell(br, bt, curralgo->getcell(gl, gb));
        curralgo->setcell(br, bb, curralgo->getcell(gl, gt));
        curralgo->setcell(bl, bb, curralgo->getcell(gr, gt));
    }
}

// -----------------------------------------------------------------------------

bool MainFrame::CreateBorderCells(lifealgo* curralgo)
{
    // no need to do anything if there is no pattern or if the grid is a bounded plane
    if (curralgo->isEmpty() || curralgo->boundedplane) return true;
    
    int gwd = curralgo->gridwd;
    int ght = curralgo->gridht;
    
    bigint top, left, bottom, right;
    curralgo->findedges(&top, &left, &bottom, &right);
    
    // no need to do anything if pattern is completely inside grid edges
    if ( (gwd == 0 || (curralgo->gridleft < left && curralgo->gridright > right)) &&
        (ght == 0 || (curralgo->gridtop < top && curralgo->gridbottom > bottom)) ) {
        return true;
    }
    
    // if grid has infinite width or height then pattern might be too big to use setcell/getcell
    if ( (gwd == 0 || ght == 0) && viewptr->OutsideLimits(top, left, bottom, right) ) {
        statusptr->ErrorMessage(_("Pattern is too big!"));
        // return false so caller can exit step() loop
        return false;
    }
    
    if (curralgo->sphere) {
        // to get a sphere we join top edge with left edge, and right edge with bottom edge;
        // note that grid must be square (gwd == ght)
        int pl = left.toint();
        int pt = top.toint();
        int pr = right.toint();      
        int pb = bottom.toint();
        JoinAdjacentEdges(curralgo, pt, pl, pb, pr);
        
    } else if (curralgo->htwist || curralgo->vtwist) {
        // Klein bottle or cross-surface
        if ( (curralgo->htwist && curralgo->hshift != 0 && (gwd & 1) == 0) ||
            (curralgo->vtwist && curralgo->vshift != 0 && (ght & 1) == 0) ) {
            // Klein bottle with shift is only possible if the shift is on the
            // twisted edge and that edge has an even number of cells
            JoinTwistedAndShiftedEdges(curralgo);
        } else {
            JoinTwistedEdges(curralgo);
        }
        
    } else if (curralgo->hshift != 0 || curralgo->vshift != 0) {
        // torus with horizontal or vertical shift
        JoinShiftedEdges(curralgo, gwd, ght, curralgo->hshift, curralgo->vshift);
        
    } else {
        // unshifted torus or infinite tube
        int pl = left.toint();
        int pt = top.toint();
        int pr = right.toint();      
        int pb = bottom.toint();
        JoinEdges(curralgo, gwd, ght, pt, pl, pb, pr);
    }
    
    curralgo->endofpattern();
    return true;
}

// -----------------------------------------------------------------------------

void MainFrame::ClearRect(lifealgo* curralgo, int top, int left, int bottom, int right)
{
    int cx, cy, v;
    for ( cy = top; cy <= bottom; cy++ ) {
        for ( cx = left; cx <= right; cx++ ) {
            int skip = curralgo->nextcell(cx, cy, v);
            if (skip + cx > right)
                skip = -1;           // pretend we found no more live cells
            if (skip >= 0) {
                // found next live cell so delete it
                cx += skip;
                curralgo->setcell(cx, cy, 0);
            } else {
                cx = right + 1;     // done this row
            }
        }
    }
}

// -----------------------------------------------------------------------------

bool MainFrame::DeleteBorderCells(lifealgo* curralgo)
{
    // no need to do anything if there is no pattern
    if (curralgo->isEmpty()) return true;
    
    int gwd = curralgo->gridwd;
    int ght = curralgo->gridht;
    
    // need to find pattern edges because pattern may have expanded beyond grid
    // (typically by 2 cells, but could be more if rule allows births in empty space)
    bigint top, left, bottom, right;
    curralgo->findedges(&top, &left, &bottom, &right);
    
    // no need to do anything if grid encloses entire pattern
    if ( (gwd == 0 || (curralgo->gridleft <= left && curralgo->gridright >= right)) &&
        (ght == 0 || (curralgo->gridtop <= top && curralgo->gridbottom >= bottom)) ) {
        return true;
    }
    
    if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
        statusptr->ErrorMessage(_("Pattern is too big!"));
        // return false so caller can exit step() loop
        return false;
    }
    
    // set pattern edges
    int pl = left.toint();
    int pt = top.toint();
    int pr = right.toint();      
    int pb = bottom.toint();
    
    // set grid edges
    int gl = curralgo->gridleft.toint();
    int gt = curralgo->gridtop.toint();
    int gr = curralgo->gridright.toint();
    int gb = curralgo->gridbottom.toint();
    
    if (ght > 0 && pt < gt) {
        // delete live cells above grid
        ClearRect(curralgo, pt, pl, gt-1, pr);
        pt = gt; // reduce size of rect below
    }
    
    if (ght > 0 && pb > gb) {
        // delete live cells below grid
        ClearRect(curralgo, gb+1, pl, pb, pr);
        pb = gb; // reduce size of rect below
    }
    
    if (gwd > 0 && pl < gl) {
        // delete live cells left of grid
        ClearRect(curralgo, pt, pl, pb, gl-1);
    }
    
    if (gwd > 0 && pr > gr) {
        // delete live cells right of grid
        ClearRect(curralgo, pt, gr+1, pb, pr);
    }
    
    curralgo->endofpattern();
    return true;
}

// -----------------------------------------------------------------------------

bool MainFrame::StepPattern()
{
    lifealgo* curralgo = currlayer->algo;
    if (curralgo->gridwd > 0 || curralgo->gridht > 0) {
        // bounded grid, so temporarily set the increment to 1 so we can call
        // CreateBorderCells() and DeleteBorderCells() around each step()
        int savebase = currlayer->currbase;
        int saveexpo = currlayer->currexpo;
        bigint inc = curralgo->getIncrement();
        curralgo->setIncrement(1);
        while (inc > 0) {
            if (wxGetApp().Poller()->checkevents()) {
                SetGenIncrement();         // restore correct increment
                return false;
            }
            if (savebase != currlayer->currbase || saveexpo != currlayer->currexpo) {
                // user changed step base/exponent, so best to simply exit loop
                break;
            }
            if (!CreateBorderCells(curralgo)) {
                SetGenIncrement();         // restore correct increment
                return false;
            }
            curralgo->step();
            if (!DeleteBorderCells(curralgo)) {
                SetGenIncrement();         // restore correct increment
                return false;
            }
            inc -= 1;
        }
        // safe way to restore correct increment in case user altered step base/exponent
        SetGenIncrement();
    } else {
        if (wxGetApp().Poller()->checkevents()) return false;
        curralgo->step();
    }
    
    if (currlayer->autofit) viewptr->FitInView(0);
    
    if (!IsIconized()) DisplayPattern();
    
    /*!!! enable this code if we ever implement isPeriodic()
    if (autostop) {
        int period = curralgo->isPeriodic();
        if (period > 0) {
            if (period == 1) {
                if (curralgo->isEmpty()) {
                    statusptr->DisplayMessage(_("Pattern is empty."));
                } else {
                    statusptr->DisplayMessage(_("Pattern is stable."));
                }
            } else {
                wxString s;
                s.Printf(_("Pattern is oscillating (period = %d)."), period);
                statusptr->DisplayMessage(s);
            }
            return false;
        }
    }
    */
    
    return true;
}

// -----------------------------------------------------------------------------

void MainFrame::GeneratePattern()
{
    if (generating || viewptr->drawingcells || viewptr->waitingforclick) {
        Beep();
        return;
    }
    
    if (currlayer->algo->isEmpty()) {
        statusptr->ErrorMessage(empty_pattern);
        return;
    }
    
    if (currlayer->algo->isrecording()) {
        // don't attempt to save starting pattern here (let DeleteTimeline do it)
    } else if (!SaveStartingPattern()) {
        return;
    }
    
    // GeneratePattern is never called while running a script so no need
    // to test inscript or currlayer->stayclean
    if (allowundo) currlayer->undoredo->RememberGenStart();
    
    // for DisplayTimingInfo
    begintime = stopwatch->Time();
    begingen = currlayer->algo->getGeneration().todouble();
    
    // for hyperspeed
    int hypdown = 64;
    
    generating = true;               // avoid re-entry
    wxGetApp().PollerReset();
    
#ifdef __WXMSW__
    wxMenuBar* mbar = GetMenuBar();
    if (mbar) {
        // remove any accelerators from the Next Gen and Next Step menu items
        // so their keyboard shortcuts can be used to stop generating;
        // this is necessary on Windows because Golly won't see any
        // key events for a disabled menu item
        RemoveAccelerator(mbar, ID_NEXT, DO_NEXTGEN);
        RemoveAccelerator(mbar, ID_STEP, DO_NEXTSTEP);
    }
#endif
    UpdateUserInterface(IsActive());
    
    // only show hashing info while generating, otherwise Mac app can crash
    // after a paste due to hlifealgo::resize() calling lifestatus() which
    // then causes the viewport to be repainted for some inexplicable reason
    lifealgo::setVerbose( currlayer->showhashinfo );
    
    if (currlayer->currexpo < 0)
        whentosee = stopwatch->Time() + statusptr->GetCurrentDelay();
    
    while (true) {
        if (currlayer->currexpo < 0) {
            // slow down by only doing one gen every GetCurrentDelay() millisecs
            long currmsec = stopwatch->Time();
            if (currmsec >= whentosee) {
                if (!StepPattern()) break;
                // add delay to current time rather than currmsec
                whentosee = stopwatch->Time() + statusptr->GetCurrentDelay();
            } else {
                // process events while we wait
                if (wxGetApp().Poller()->checkevents()) break;
                // don't hog CPU but keep sleep duration short (ie. <= mindelay)
                wxMilliSleep(1);
            }
        } else {
            // currexpo >= 0 so advance pattern by currlayer->algo->getIncrement() gens
            if (!StepPattern()) break;
            if (currlayer->algo->isrecording()) {
                if (showtimeline) UpdateTimelineBar(IsActive());
                if (currlayer->algo->getframecount() == MAX_FRAME_COUNT) {
                    wxString msg;
                    msg.Printf(_("No more frames can be recorded (maximum = %d)."), MAX_FRAME_COUNT);
                    Warning(msg);
                    break;
                }
            } else if (currlayer->hyperspeed && currlayer->algo->hyperCapable()) {
                hypdown--;
                if (hypdown == 0) {
                    hypdown = 64;
                    GoFaster();
                }
            }
        }
    }
    
    generating = false;
    
    lifealgo::setVerbose(0);
    
    // for DisplayTimingInfo
    endtime = stopwatch->Time();
    endgen = currlayer->algo->getGeneration().todouble();
    
#ifdef __WXMSW__
    if (mbar) {
        // restore accelerators removed above
        SetAccelerator(mbar, ID_NEXT, DO_NEXTGEN);
        SetAccelerator(mbar, ID_STEP, DO_NEXTSTEP);
    }
#endif
    
    // display the final pattern
    if (currlayer->autofit) viewptr->FitInView(0);
    if (command_pending || draw_pending) {
        // let the pending command/draw do the update below
    } else {
        UpdateEverything();
    }
    
    // GeneratePattern is never called while running a script so no need
    // to test inscript or currlayer->stayclean; note that we must call
    // RememberGenFinish BEFORE processing any pending command
    if (allowundo) currlayer->undoredo->RememberGenFinish();
    
    // stop recording any timeline before processing any pending command
    if (currlayer->algo->isrecording()) {
        currlayer->algo->stoprecording();
        if (currlayer->algo->getframecount() > 0) {
            // probably best to go to last frame
            currlayer->currframe = currlayer->algo->getframecount() - 1;
            currlayer->autoplay = 0;
            currlayer->tlspeed = 0;
            currlayer->algo->gotoframe(currlayer->currframe);
            if (currlayer->autofit) viewptr->FitInView(1);
        }
        if (!showtimeline) ToggleTimelineBar();
        UpdateUserInterface(true);
    }
    
    DoPendingAction(true);     // true means we can restart generating loop
}

// -----------------------------------------------------------------------------

void MainFrame::DoPendingAction(bool restart)
{
    if (command_pending) {
        command_pending = false;
        
        int id = cmdevent.GetId();
        switch (id) {
                // don't restart the generating loop after some commands
            case wxID_NEW:          NewPattern(); break;
            case wxID_OPEN:         OpenPattern(); break;
            case ID_OPEN_CLIP:      OpenClipboard(); break;
            case ID_RESET:          ResetPattern(); break;
            case ID_SETGEN:         SetGeneration(); break;
            case ID_UNDO:           currlayer->undoredo->UndoChange(); break;
            case ID_ADD_LAYER:      AddLayer(); break;
            case ID_DUPLICATE:      DuplicateLayer(); break;
            case ID_LOAD_LEXICON:   LoadLexiconPattern(); break;
            default:
                if ( id > ID_OPEN_RECENT && id <= ID_OPEN_RECENT + numpatterns ) {
                    OpenRecentPattern(id);
                    
                } else if ( id > ID_RUN_RECENT && id <= ID_RUN_RECENT + numscripts ) {
                    OpenRecentScript(id);
                    if (restart && !stop_after_script) {
                        wxCommandEvent goevt(wxEVT_COMMAND_MENU_SELECTED, ID_START);
                        wxPostEvent(this->GetEventHandler(), goevt);
                        // avoid clearing status message due to script like density.py
                        keepmessage = true;
                    }
                    
                } else if ( id == ID_RUN_SCRIPT ) {
                    OpenScript();
                    if (restart && !stop_after_script) {
                        wxCommandEvent goevt(wxEVT_COMMAND_MENU_SELECTED, ID_START);
                        wxPostEvent(this->GetEventHandler(), goevt);
                        // avoid clearing status message due to script like density.py
                        keepmessage = true;
                    }
                    
                } else if ( id == ID_RUN_CLIP ) {
                    RunClipboard();
                    if (restart && !stop_after_script) {
                        wxCommandEvent goevt(wxEVT_COMMAND_MENU_SELECTED, ID_START);
                        wxPostEvent(this->GetEventHandler(), goevt);
                        // avoid clearing status message due to script like density.py
                        keepmessage = true;
                    }
                    
                } else if ( id >= ID_LAYER0 && id <= ID_LAYERMAX ) {
                    int oldcloneid = currlayer->cloneid;
                    SetLayer(id - ID_LAYER0);
                    // continue generating if new layer is a clone of old layer
                    if (restart && currlayer->cloneid > 0 && currlayer->cloneid == oldcloneid) {
                        wxCommandEvent goevt(wxEVT_COMMAND_MENU_SELECTED, ID_START);
                        wxPostEvent(this->GetEventHandler(), goevt);
                    }
                    
                } else if ( id == ID_DEL_LAYER ) {
                    int wasclone = currlayer->cloneid > 0 &&
                    ((currindex == 0 && currlayer->cloneid == GetLayer(1)->cloneid) ||
                     (currindex > 0 && currlayer->cloneid == GetLayer(currindex-1)->cloneid));
                    DeleteLayer();
                    // continue generating if new layer is/was a clone of old layer
                    if (restart && wasclone) {
                        wxCommandEvent goevt(wxEVT_COMMAND_MENU_SELECTED, ID_START);
                        wxPostEvent(this->GetEventHandler(), goevt);
                    }
                    
                } else {
                    // temporarily pretend the tool/layer/edit bars are not showing
                    // to avoid Update[Tool/Layer/Edit]Bar changing button states
                    bool saveshowtool = showtool;    showtool = false;
                    bool saveshowlayer = showlayer;  showlayer = false;
                    bool saveshowedit = showedit;    showedit = false;
                    
                    // process the pending command
                    cmdevent.SetEventType(wxEVT_COMMAND_MENU_SELECTED);
                    cmdevent.SetEventObject(this);
                    this->GetEventHandler()->ProcessEvent(cmdevent);
                    
                    // restore tool/layer/edit bar flags
                    showtool = saveshowtool;
                    showlayer = saveshowlayer;
                    showedit = saveshowedit;
                    
                    if (restart) {
                        // call GeneratePattern again
                        wxCommandEvent goevt(wxEVT_COMMAND_MENU_SELECTED, ID_START);
                        wxPostEvent(this->GetEventHandler(), goevt);
                    }
                }
        }
    }
    
    if (draw_pending) {
        draw_pending = false;
        
        // temporarily pretend the tool/layer/edit bars are not showing
        // to avoid Update[Tool/Layer/Edit]Bar changing button states
        bool saveshowtool = showtool;    showtool = false;
        bool saveshowlayer = showlayer;  showlayer = false;
        bool saveshowedit = showedit;    showedit = false;
        
        UpdateEverything();
        
        // do the drawing
        mouseevent.SetEventType(wxEVT_LEFT_DOWN);
        mouseevent.SetEventObject(viewptr);
        viewptr->GetEventHandler()->ProcessEvent(mouseevent);
        while (viewptr->drawingcells) {
            wxGetApp().Yield(true);
            wxMilliSleep(5);             // don't hog CPU
        }
        
        // restore tool/layer/edit bar flags
        showtool = saveshowtool;
        showlayer = saveshowlayer;
        showedit = saveshowedit;
        
        if (restart) {
            // call GeneratePattern again
            wxCommandEvent goevt(wxEVT_COMMAND_MENU_SELECTED, ID_START);
            wxPostEvent(this->GetEventHandler(), goevt);
        }
    }
}

// -----------------------------------------------------------------------------

void MainFrame::DisplayTimingInfo()
{
    if (viewptr->waitingforclick) return;
    if (generating) {
        endtime = stopwatch->Time();
        endgen = currlayer->algo->getGeneration().todouble();
    }
    if (endtime > begintime) {
        double secs = (double)(endtime - begintime) / 1000.0;
        double gens = endgen - begingen;
        wxString s;
        s.Printf(_("%g gens in %g secs (%g gens/sec)."), gens, secs, gens/secs);
        statusptr->DisplayMessage(s);
    }
}

// -----------------------------------------------------------------------------

void MainFrame::Stop()
{
    if (inscript) {
        PassKeyToScript(WXK_ESCAPE);
    } else if (generating) {
        wxGetApp().PollerInterrupt();
    }
}

// -----------------------------------------------------------------------------

// this global flag is used to avoid re-entrancy in NextGeneration()
// due to holding down the space/tab key
static bool inNextGen = false;

void MainFrame::NextGeneration(bool useinc)
{
    if (inNextGen) return;
    inNextGen = true;
    
    if (!inscript && generating) {
        // we must be in GeneratePattern() loop, so abort it
        Stop();
        inNextGen = false;
        return;
    }
    
    if (viewptr->drawingcells || viewptr->waitingforclick) {
        Beep();
        inNextGen = false;
        return;
    }
    
    // best if generating stops after running a script like oscar.py or goto.py
    if (inscript) stop_after_script = true;
    
    lifealgo* curralgo = currlayer->algo;
    if (curralgo->isEmpty()) {
        statusptr->ErrorMessage(empty_pattern);
        inNextGen = false;
        return;
    }
    
    if (!SaveStartingPattern()) {
        inNextGen = false;
        return;
    }
    
    if (allowundo) {
        if (currlayer->stayclean) {
            // script has called run/step after a new/open command has set
            // stayclean true by calling MarkLayerClean
            if (curralgo->getGeneration() == currlayer->startgen) {
                // starting pattern has just been saved so we need to remember
                // this gen change in case user does a Reset after script ends
                // (RememberGenFinish will be called at the end of RunScript)
                if (currlayer->undoredo->savegenchanges) {
                    // script must have called reset command, so we need to call
                    // RememberGenFinish to match earlier RememberGenStart
                    currlayer->undoredo->savegenchanges = false;
                    currlayer->undoredo->RememberGenFinish();
                }
                currlayer->undoredo->RememberGenStart();
            }
        } else {
            // !currlayer->stayclean
            if (inscript) {
                // pass in false so we don't test savegenchanges flag;
                // ie. we only want to save pending cell changes here
                SavePendingChanges(false);
            }
            currlayer->undoredo->RememberGenStart();
        }
    }
    
    // curralgo->step() calls checkevents() so set generating flag
    generating = true;
    
    // only show hashing info while generating
    lifealgo::setVerbose( currlayer->showhashinfo );
    
    // avoid doing some things if NextGeneration is called from a script;
    // ie. by a run/step command
    if (!inscript) {
        if (useinc) {
            // for DisplayTimingInfo
            begintime = stopwatch->Time();
            begingen = curralgo->getGeneration().todouble();
        }
        wxGetApp().PollerReset();
        viewptr->CheckCursor(IsActive());
    }
    
    bool boundedgrid = (curralgo->gridwd > 0 || curralgo->gridht > 0);
    
    if (useinc) {
        // step by current increment
        if (curralgo->getIncrement() > bigint::one && !inscript) {
            UpdateToolBar(IsActive());
            UpdateMenuItems(IsActive());
        }
        if (boundedgrid) {
            // temporarily set the increment to 1 so we can call CreateBorderCells()
            // and DeleteBorderCells() around each step()
            int savebase = currlayer->currbase;
            int saveexpo = currlayer->currexpo;
            bigint inc = curralgo->getIncrement();
            curralgo->setIncrement(1);
            while (inc > 0) {
                if (wxGetApp().Poller()->checkevents()) break;
                if (savebase != currlayer->currbase || saveexpo != currlayer->currexpo) {
                    // user changed step base/exponent, so reset increment to 1
                    inc = curralgo->getIncrement();
                    curralgo->setIncrement(1);
                }
                if (!CreateBorderCells(curralgo)) break;
                curralgo->step();
                if (!DeleteBorderCells(curralgo)) break;
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
        if (boundedgrid) CreateBorderCells(curralgo);
        curralgo->step();
        if (boundedgrid) DeleteBorderCells(curralgo);
        curralgo->setIncrement(saveinc);
    }
    
    generating = false;
    
    lifealgo::setVerbose(0);
    
    if (!inscript) {
        if (useinc) {
            // for DisplayTimingInfo (we add 1 millisec here in case it took < 1 millisec)
            endtime = stopwatch->Time() + 1;
            endgen = curralgo->getGeneration().todouble();
        }
        // autofit is only used when doing many gens
        if (currlayer->autofit && useinc && curralgo->getIncrement() > bigint::one)
            viewptr->FitInView(0);
        UpdateEverything();
    }
    
    // we must call RememberGenFinish BEFORE processing any pending command
    if (allowundo && !currlayer->stayclean)
        currlayer->undoredo->RememberGenFinish();
    
    // process any pending command seen via checkevents() in curralgo->step()
    if (!inscript)
        DoPendingAction(false);     // false means don't restart generating loop
    
    inNextGen = false;
}

// -----------------------------------------------------------------------------

void MainFrame::ToggleAutoFit()
{
    currlayer->autofit = !currlayer->autofit;
    
    // we only use autofit when generating; that's why the Auto Fit item
    // is in the Control menu and not in the View menu
    if (generating && currlayer->autofit) {
        viewptr->FitInView(0);
        UpdateEverything();
    }
}

// -----------------------------------------------------------------------------

void MainFrame::ToggleHyperspeed()
{
    currlayer->hyperspeed = !currlayer->hyperspeed;
}

// -----------------------------------------------------------------------------

void MainFrame::ToggleHashInfo()
{
    currlayer->showhashinfo = !currlayer->showhashinfo;
    
    // only show hashing info while generating
    if (generating) lifealgo::setVerbose( currlayer->showhashinfo );
}

// -----------------------------------------------------------------------------

void MainFrame::ClearOutsideGrid()
{
    // check current pattern and clear any live cells outside bounded grid
    bool patternchanged = false;
    bool savechanges = allowundo && !currlayer->stayclean;
    
    // might also need to truncate selection
    currlayer->currsel.CheckGridEdges();
    
    if (currlayer->algo->isEmpty()) return;
    
    // check if current pattern is too big to use nextcell/setcell
    bigint top, left, bottom, right;
    currlayer->algo->findedges(&top, &left, &bottom, &right);
    if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
        statusptr->ErrorMessage(_("Pattern too big to check (outside +/- 10^9 boundary)."));
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
    BeginProgress(_("Checking cells outside grid"));
    
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
                abort = AbortProgress(accumcount / maxcount, wxEmptyString);
                if (abort) break;
            }
        }
        if (abort) break;
    }
    
    curralgo->endofpattern();
    EndProgress();
    
    if (patternchanged) {
        statusptr->ErrorMessage(_("Pattern was truncated (live cells were outside grid)."));
    }
}

// -----------------------------------------------------------------------------

void MainFrame::ReduceCellStates(int newmaxstate)
{
    // check current pattern and reduce any cell states > newmaxstate
    bool patternchanged = false;
    bool savechanges = allowundo && !currlayer->stayclean;
    
    // check if current pattern is too big to use nextcell/setcell
    bigint top, left, bottom, right;
    currlayer->algo->findedges(&top, &left, &bottom, &right);
    if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
        statusptr->ErrorMessage(_("Pattern too big to check (outside +/- 10^9 boundary)."));
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
    BeginProgress(_("Checking cell states"));
    
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
                abort = AbortProgress(accumcount / maxcount, wxEmptyString);
                if (abort) break;
            }
        }
        if (abort) break;
    }
    
    curralgo->endofpattern();
    EndProgress();
    
    if (patternchanged) {
        statusptr->ErrorMessage(_("Pattern has changed (new rule has fewer states)."));
    }
}

// -----------------------------------------------------------------------------

void MainFrame::ShowRuleDialog()
{
    if (inscript || viewptr->waitingforclick) return;
    
    if (generating) {
        // terminate generating loop and set command_pending flag
        Stop();
        command_pending = true;
        cmdevent.SetId(ID_SETRULE);
        return;
    }
    
    algo_type oldalgo = currlayer->algtype;
    wxString oldrule = wxString(currlayer->algo->getrule(), wxConvLocal);
    int oldmaxstate = currlayer->algo->NumCellStates() - 1;
    
    // selection might change if grid becomes smaller,
    // so save current selection for RememberRuleChange/RememberAlgoChange
    viewptr->SaveCurrentSelection();
    
    if (ChangeRule()) {
        // if ChangeAlgorithm was called then we're done
        if (currlayer->algtype != oldalgo) {
            // except we have to call UpdateEverything here now that the main window is active
            UpdateEverything();
            return;
        }
        
        // show new rule in window title (but don't change file name);
        // even if the rule didn't change we still need to do this because
        // the user might have simply added/deleted a named rule
        SetWindowTitle(wxEmptyString);
        
        // check if the rule string changed, or the number of states changed
        // (the latter might happen if user edited a table/tree file)
        wxString newrule = wxString(currlayer->algo->getrule(), wxConvLocal);
        int newmaxstate = currlayer->algo->NumCellStates() - 1;
        if (oldrule != newrule || oldmaxstate != newmaxstate) {
            // if grid is bounded then remove any live cells outside grid edges
            if (currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0) {
                ClearOutsideGrid();
            }
            
            // rule change might have changed the number of cell states;
            // if there are fewer states then pattern might change
            if (newmaxstate < oldmaxstate && !currlayer->algo->isEmpty()) {
                ReduceCellStates(newmaxstate);
            }
            
            if (allowundo) {
                currlayer->undoredo->RememberRuleChange(oldrule);
            }
        }
        
        // switch to default colors and icons for new rule (we need to do this even if
        // oldrule == newrule in case there's a new/changed .colors or .icons file)
        UpdateLayerColors();
        
        // pattern or colors or icons might have changed
        UpdateEverything();
    }
}

// -----------------------------------------------------------------------------

void MainFrame::ChangeAlgorithm(algo_type newalgotype, const wxString& newrule, bool inundoredo)
{
    if (newalgotype == currlayer->algtype) return;
    
    // check if current pattern is too big to use nextcell/setcell
    bigint top, left, bottom, right;
    if ( !currlayer->algo->isEmpty() ) {
        currlayer->algo->findedges(&top, &left, &bottom, &right);
        if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
            statusptr->ErrorMessage(_("Pattern cannot be converted (outside +/- 10^9 boundary)."));
            return;
        }
    }
    
    if (generating) {
        // terminate generating loop and set command_pending flag
        Stop();
        command_pending = true;
        cmdevent.SetId(ID_ALGO0 + newalgotype);
        return;
    }
    
    // save changes if undo/redo is enabled and script isn't constructing a pattern
    // and we're not undoing/redoing an earlier algo change
    bool savechanges = allowundo && !currlayer->stayclean && !inundoredo;
    if (savechanges && inscript) {
        // note that we must save pending gen changes BEFORE changing algo type
        // otherwise temporary files won't be the correct type (mc or rle)
        SavePendingChanges();
    }
    
    // selection might change if grid becomes smaller,
    // so save current selection for RememberAlgoChange
    if (savechanges) viewptr->SaveCurrentSelection();
    
    bool rulechanged = false;
    wxString oldrule = wxString(currlayer->algo->getrule(), wxConvLocal);
    
    // change algorithm type, reset step size, and update status bar immediately
    algo_type oldalgo = currlayer->algtype;
    currlayer->algtype = newalgotype;
    currlayer->currbase = algoinfo[newalgotype]->defbase;
    currlayer->currexpo = 0;
    UpdateStatus();
    
    // create a new universe of the requested flavor
    lifealgo* newalgo = CreateNewUniverse(newalgotype);
    
    if (inundoredo) {
        // switch to given newrule
        const char* err = newalgo->setrule( newrule.mb_str(wxConvLocal) );
        if (err) newalgo->setrule( newalgo->DefaultRule() );
    } else {
        const char* err;
        if (newrule.IsEmpty()) {
            // try to use same rule
            err = newalgo->setrule( currlayer->algo->getrule() );
        } else {
            // switch to newrule
            err = newalgo->setrule( newrule.mb_str(wxConvLocal) );
            rulechanged = true;
        }
        if (err) {
            wxString defrule = wxString(newalgo->DefaultRule(), wxConvLocal);
            if (newrule.IsEmpty() && oldrule.Find(':') >= 0) {
                // switch to new algo's default rule, but preserve the topology in oldrule
                // so we can do things like switch from "LifeHistory:T30,20" in RuleLoader
                // to "B3/S23:T30,20" in QuickLife
                if (defrule.Find(':') >= 0) {
                    // default rule shouldn't have a suffix but play safe and remove it
                    defrule = defrule.BeforeFirst(':');
                }
                defrule += wxT(":");
                defrule += oldrule.AfterFirst(':');
            }
            err = newalgo->setrule( defrule.mb_str(wxConvLocal) );
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
        BeginProgress(_("Converting pattern"));
        
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
                    abort = AbortProgress(accumcount / maxcount, wxEmptyString);
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
        if (rulechanged) {
            // show new rule in window title (but don't change file name)
            SetWindowTitle(wxEmptyString);
            
            // if pattern exists and is at starting gen then set savestart true
            // so that SaveStartingPattern will save pattern to suitable file
            // (and thus ResetPattern will work correctly)
            if ( currlayer->algo->getGeneration() == currlayer->startgen &&
                !currlayer->algo->isEmpty() ) {
                currlayer->savestart = true;
            }
            
            if (newrule.IsEmpty()) {
                if (patternchanged) {
                    statusptr->ErrorMessage(_("Rule has changed and pattern has changed."));
                } else {
                    // don't beep
                    statusptr->DisplayMessage(_("Rule has changed."));
                }
            } else {
                if (patternchanged) {
                    statusptr->ErrorMessage(_("Algorithm has changed and pattern has changed."));
                } else {
                    // don't beep
                    statusptr->DisplayMessage(_("Algorithm has changed."));
                }
            }
        } else if (patternchanged) {
            statusptr->ErrorMessage(_("Pattern has changed."));
        }
        
        if (!inscript) {
            UpdateEverything();
        }
    }
    
    if (savechanges) {
        currlayer->undoredo->RememberAlgoChange(oldalgo, oldrule);
    }
}

// -----------------------------------------------------------------------------

static wxString CreateTABLE(const wxString& tablepath)
{
    wxString contents = wxT("\n@TABLE\n\n");
    // append contents of .table file
    FILE* f = fopen(tablepath.mb_str(wxConvLocal), "r");
    if (f) {
        const int MAXLINELEN = 4095;
        char linebuf[MAXLINELEN + 1];
        linereader reader(f);
        while (true) {
            if (reader.fgets(linebuf, MAXLINELEN) == 0) break;
            contents += wxString(linebuf, wxConvLocal);
            contents += wxT("\n");
        }
        reader.close();
    } else {
        std::ostringstream oss;
        oss << "Could not read .table file:\n" << tablepath.mb_str(wxConvLocal);
        throw std::runtime_error(oss.str().c_str());
    }
    return contents;
}

// -----------------------------------------------------------------------------

static wxString CreateTREE(const wxString& treepath)
{
    wxString contents = wxT("\n@TREE\n\n");
    // append contents of .tree file
    FILE* f = fopen(treepath.mb_str(wxConvLocal), "r");
    if (f) {
        const int MAXLINELEN = 4095;
        char linebuf[MAXLINELEN + 1];
        linereader reader(f);
        while (true) {
            if (reader.fgets(linebuf, MAXLINELEN) == 0) break;
            contents += wxString(linebuf, wxConvLocal);
            contents += wxT("\n");
        }
        reader.close();
    } else {
        std::ostringstream oss;
        oss << "Could not read .tree file:\n" << treepath.mb_str(wxConvLocal);
        throw std::runtime_error(oss.str().c_str());
    }
    return contents;
}

// -----------------------------------------------------------------------------

static wxString CreateCOLORS(const wxString& colorspath)
{
    wxString contents = wxT("\n@COLORS\n\n");
    FILE* f = fopen(colorspath.mb_str(wxConvLocal), "r");
    if (f) {
        const int MAXLINELEN = 4095;
        char linebuf[MAXLINELEN + 1];
        linereader reader(f);
        while (true) {
            if (reader.fgets(linebuf, MAXLINELEN) == 0) break;
            int skip = 0;
            if (strncmp(linebuf, "color", 5) == 0 ||
                strncmp(linebuf, "gradient", 8) == 0) {
                // strip off everything before 1st digit
                while (linebuf[skip] && (linebuf[skip] < '0' || linebuf[skip] > '9')) {
                    skip++;
                }
            }
            contents += wxString(linebuf + skip, wxConvLocal);
            contents += wxT("\n");
        }
        reader.close();
    } else {
        std::ostringstream oss;
        oss << "Could not read .colors file:\n" << colorspath.mb_str(wxConvLocal);
        throw std::runtime_error(oss.str().c_str());
    }
    return contents;
}

// -----------------------------------------------------------------------------

static wxString CreateStateColors(wxImage image, int numicons)
{
    wxString contents = wxT("\n@COLORS\n\n");
    
    // if the last icon has only 1 color then assume it is the extra 15x15 icon
    // supplied to set the color of state 0
    if (numicons > 1) {
        wxImage icon = image.GetSubImage(wxRect((numicons-1)*15, 0, 15, 15));
        if (icon.CountColours(1) == 1) {
            unsigned char* idata = icon.GetData();
            unsigned char R = idata[0];
            unsigned char G = idata[1];
            unsigned char B = idata[2];
            contents += wxString::Format(wxT("0 %d %d %d\n"), R, G, B);
            numicons--;
        }
    }
    
    // set non-icon colors for each live state to the average of the non-black pixels
    // in each 15x15 icon (note we've skipped the extra icon detected above)
    for (int i = 0; i < numicons; i++) {
        wxImage icon = image.GetSubImage(wxRect(i*15, 0, 15, 15));
        int nbcount = 0;   // non-black pixels
        int totalR = 0;
        int totalG = 0;
        int totalB = 0;
        unsigned char* idata = icon.GetData();
        for (int y = 0; y < 15; y++) {
            for (int x = 0; x < 15; x++) {
                long pos = (y * 15 + x) * 3;
                unsigned char R = idata[pos];
                unsigned char G = idata[pos+1];
                unsigned char B = idata[pos+2];
                if (R > 0 || G > 0 || B > 0) {
                    // non-black pixel
                    nbcount++;
                    totalR += R;
                    totalG += G;
                    totalB += B;
                }
            }
        }
        if (nbcount > 0) {
            contents += wxString::Format(wxT("%d %d %d %d\n"), i+1, totalR/nbcount, totalG/nbcount, totalB/nbcount);
        } else {
            // unlikely, but avoid div by zero
            contents += wxString::Format(wxT("%d 0 0 0\n"), i+1);
        }
    }
    
    return contents;
}

// -----------------------------------------------------------------------------

static wxString hex2(int i)
{
    // convert given number from 0..255 into 2 hex digits
    wxString result = wxT("xx");
    const char* hexdigit = "0123456789ABCDEF";
    result[0] = hexdigit[i / 16];
    result[1] = hexdigit[i % 16];
    return result;
}

// -----------------------------------------------------------------------------

static wxString CreateXPM(const wxString& iconspath, wxImage image, int size, int numicons)
{
    // create XPM data for given set of icons
    wxString contents = wxT("\nXPM\n");
    
    int charsperpixel = 1;
    const char* cindex = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    wxImageHistogram histogram;
    int numcolors = image.ComputeHistogram(histogram);
    if (numcolors > 256) {
        std::ostringstream oss;
        oss << "Image in " << iconspath.mb_str(wxConvLocal) << " has more than 256 colors.";
        throw std::runtime_error(oss.str().c_str());
    }
    if (numcolors > 26) charsperpixel = 2;   // AABA..PA, ABBB..PB, ... , APBP..PP
    
    contents += wxT("/* width height num_colors chars_per_pixel */\n");
    contents += wxString::Format(wxT("\"%d %d %d %d\"\n"), size, size*numicons, numcolors, charsperpixel);
    
    contents += wxT("/* colors */\n");
    int n = 0;
    for (wxImageHistogram::iterator entry = histogram.begin(); entry != histogram.end(); ++entry) {
        unsigned long key = entry->first;
        unsigned char R = (key & 0xFF0000) >> 16;
        unsigned char G = (key & 0x00FF00) >> 8;
        unsigned char B = (key & 0x0000FF);
        if (R == 0 && G == 0 && B == 0) {
            // nicer to show . or .. for black pixels
            contents += wxT("\".");
            if (charsperpixel == 2) contents += wxT(".");
            contents += wxT(" c #000000\"\n");
        } else {
            wxString hexcolor = wxT("#");
            hexcolor += hex2(R);
            hexcolor += hex2(G);
            hexcolor += hex2(B);
            contents += wxT("\"");
            if (charsperpixel == 1) {
                contents += cindex[n];
            } else {
                contents += cindex[n % 16];
                contents += cindex[n / 16];
            }
            contents += wxT(" c ");
            contents += hexcolor;
            contents += wxT("\"\n");
        }
        n++;
    }
    
    for (int i = 0; i < numicons; i++) {
        contents += wxString::Format(wxT("/* icon for state %d */\n"), i+1);
        wxImage icon = image.GetSubImage(wxRect(i*15, 0, size, size));
        unsigned char* idata = icon.GetData();
        for (int y = 0; y < size; y++) {
            contents += wxT("\"");
            for (int x = 0; x < size; x++) {
                long pos = (y * size + x) * 3;
                unsigned char R = idata[pos];
                unsigned char G = idata[pos+1];
                unsigned char B = idata[pos+2];
                if (R == 0 && G == 0 && B == 0) {
                    // nicer to show . or .. for black pixels
                    contents += wxT(".");
                    if (charsperpixel == 2) contents += wxT(".");
                } else {
                    n = 0;
                    unsigned long thisRGB = wxImageHistogram::MakeKey(R,G,B);
                    for (wxImageHistogram::iterator entry = histogram.begin(); entry != histogram.end(); ++entry) {
                        if (thisRGB == entry->first) break;
                        n++;
                    }
                    if (charsperpixel == 1) {
                        contents += cindex[n];
                    } else {
                        contents += cindex[n % 16];
                        contents += cindex[n / 16];
                    }
                }
            }
            contents += wxT("\"\n");
        }
    }
    
    return contents;
}

// -----------------------------------------------------------------------------

static wxString CreateICONS(const wxString& iconspath, bool nocolors)
{
    wxString contents = wxT("\n@ICONS\n");
    wxImage image;
    if (image.LoadFile(iconspath)) {
        int wd = image.GetWidth();
        int ht = image.GetHeight();
        if (ht != 15 && ht != 22) {
            std::ostringstream oss;
            oss << "Image in " << iconspath.mb_str(wxConvLocal) <<
                " has incorrect height (should be 15 or 22).";
            throw std::runtime_error(oss.str().c_str());
        }
        if (wd % 15 > 0) {
            std::ostringstream oss;
            oss << "Image in " << iconspath.mb_str(wxConvLocal) <<
                " has incorrect width (should be multiple of 15).";
            throw std::runtime_error(oss.str().c_str());
        }
        int numicons = wd / 15;
        
        if (nocolors && MultiColorImage(image)) {
            // there was no .colors file and .icons file is multi-color,
            // so prepend a @COLORS section that sets non-icon colors
            contents = CreateStateColors(image.GetSubImage(wxRect(0,0,wd,15)), numicons) + contents;
        }
        
        if (ht == 15) {
            contents += CreateXPM(iconspath, image, 15, numicons);
        } else {
            contents += CreateXPM(iconspath, image.GetSubImage(wxRect(0,0,wd,15)), 15, numicons);
            contents += CreateXPM(iconspath, image.GetSubImage(wxRect(0,15,wd,7)), 7, numicons);
        }
    } else {
        std::ostringstream oss;
        oss << "Could not load image from .icons file:\n" << iconspath.mb_str(wxConvLocal);
        throw std::runtime_error(oss.str().c_str());
    }
    return contents;
}

// -----------------------------------------------------------------------------

static void CreateOneRule(const wxString& rulefile, const wxString& folder,
                          const wxSortedArrayString& allfiles, wxString& htmlinfo)
{
    wxString rulename = rulefile.BeforeLast('.');
    wxString prefix = rulename.BeforeLast('-');
    wxString tablefile = rulename + wxT(".table");
    wxString treefile = rulename + wxT(".tree");
    wxString colorsfile = rulename + wxT(".colors");
    wxString iconsfile = rulename + wxT(".icons");
    wxString tabledata, treedata, colordata, icondata;
    
    if (allfiles.Index(tablefile) != wxNOT_FOUND)
        tabledata = CreateTABLE(folder + tablefile);
    
    if (allfiles.Index(treefile) != wxNOT_FOUND)
        treedata = CreateTREE(folder + treefile);
    
    if (allfiles.Index(colorsfile) != wxNOT_FOUND) {
        colordata = CreateCOLORS(folder + colorsfile);
    } else if (prefix.length() > 0) {
        // check for shared .colors file
        wxString sharedcolors = prefix + wxT(".colors");
        if (allfiles.Index(sharedcolors) != wxNOT_FOUND)
            colordata = CreateCOLORS(folder + sharedcolors);
    }
    
    if (allfiles.Index(iconsfile) != wxNOT_FOUND) {
        icondata = CreateICONS(folder + iconsfile, colordata.length() == 0);
    } else if (prefix.length() > 0) {
        // check for shared .icons file
        wxString sharedicons = prefix + wxT(".icons");
        if (allfiles.Index(sharedicons) != wxNOT_FOUND)
            icondata = CreateICONS(folder + sharedicons, colordata.length() == 0);
    }
    
    wxString contents = wxT("@RULE ") + rulename + wxT("\n");
    contents += tabledata;
    contents += treedata;
    contents += colordata;
    contents += icondata;
    
    // write contents to .rule file
    wxString rulepath = folder + rulefile;
    wxFile outfile(rulepath, wxFile::write);
    if (outfile.IsOpened()) {
        outfile.Write(contents);
        outfile.Close();
    } else {
        std::ostringstream oss;
        oss << "Could not create rule file:\n" << rulepath.mb_str(wxConvLocal);
        throw std::runtime_error(oss.str().c_str());
    }
    
    // append created file to htmlinfo
    htmlinfo += _("<a href=\"open:");
    htmlinfo += folder;
    htmlinfo += rulefile;
    htmlinfo += _("\">");
    htmlinfo += rulefile;
    htmlinfo += _("</a><br>\n");
}

// -----------------------------------------------------------------------------

static int ConvertRules(const wxString& folder, bool supplied, wxString& htmlinfo)
{
    int oldcount = 0;
    wxDir dir(folder);
    if (!dir.IsOpened()) {
        std::ostringstream oss;
        oss << "Failed to open directory:\n" << folder.mb_str(wxConvLocal);
        throw std::runtime_error(oss.str().c_str());
    }

    htmlinfo += wxT("<p>\n");
    if (supplied) {
        htmlinfo += _("New .rule files created in the supplied Rules folder:<br>\n(");
    } else {
        htmlinfo += _("New .rule files created in your rules folder:<br>\n(");
    }
    htmlinfo += folder;
    htmlinfo += _(")<p>\n");
    
    // build an array of all files in the given folder
    // (using a sorted array speeds up Index calls)
    wxSortedArrayString allfiles;
    wxString filename, rulefile;
    bool found = dir.GetFirst(&filename, wxEmptyString, wxDIR_FILES | wxDIR_HIDDEN);
    while (found) {
        allfiles.Add(filename);
        found = dir.GetNext(&filename);
    }

    // create an array of candidate .rule files
    wxSortedArrayString candidates;
    for (size_t n = 0; n < allfiles.GetCount(); n++) {
        filename = allfiles[n];
        if ( filename.EndsWith(wxT(".colors")) || filename.EndsWith(wxT(".icons")) ||
             filename.EndsWith(wxT(".table")) || filename.EndsWith(wxT(".tree")) ) {
            // add .rule file to candidates if it hasn't been added yet
            rulefile = filename.BeforeLast('.') + wxT(".rule");
            if (candidates.Index(rulefile) == wxNOT_FOUND) {
                candidates.Add(rulefile);
            }
            oldcount++;
        }
    }
    
    // look for .rule files of the form foo-*.rule and ignore any foo.rule
    // entries in candidates if foo.table and foo.tree don't exist
    // (ie. foo.colors and/or foo.icons is shared by foo-*.table/tree)
    wxSortedArrayString ignore;
    for (size_t n = 0; n < candidates.GetCount(); n++) {
        rulefile = candidates[n];
        wxString prefix = rulefile.BeforeLast('-');
        if (prefix.length() > 0) {
            wxString tablefile = prefix + wxT(".table");
            wxString treefile = prefix + wxT(".tree");
            if ( (allfiles.Index(tablefile) == wxNOT_FOUND) &&
                 (allfiles.Index(treefile) == wxNOT_FOUND) ) {
                wxString sharedfile = prefix + wxT(".rule");
                if (candidates.Index(sharedfile) != wxNOT_FOUND) {
                    ignore.Add(sharedfile);
                }
            }
        }
        // also ignore any existing .rule files
        if (allfiles.Index(rulefile) != wxNOT_FOUND) {
            ignore.Add(rulefile);
        }
    }
    
    // non-ignored candidates are the .rule files that need to be created
    int rulecount = 0;
    for (size_t n = 0; n < candidates.GetCount(); n++) {
        rulefile = candidates[n];
        if (ignore.Index(rulefile) == wxNOT_FOUND) {
            CreateOneRule(rulefile, folder, allfiles, htmlinfo);
            rulecount++;
        }
    }
    
    if (rulecount == 0) {
        htmlinfo += _("None.\n");
    }
    
    return oldcount;
}

// -----------------------------------------------------------------------------

static void ShowCreatedRules(wxString& htmlinfo)
{
    wxString header = _("<html><title>Converted Rules</title>\n");
    header += _("<body bgcolor=\"#FFFFCE\">\n");
    htmlinfo = header + htmlinfo;
    htmlinfo += _("\n</body></html>");

    wxString htmlfile = tempdir + _("converted-rules.html");
    wxFile outfile(htmlfile, wxFile::write);
    if (outfile.IsOpened()) {
        outfile.Write(htmlinfo);
        outfile.Close();
        ShowHelp(htmlfile);
    } else {
        Warning(_("Could not create html file:\n") + htmlfile);
    }
}

// -----------------------------------------------------------------------------

static void DeleteOldRules(const wxString& folder)
{
    wxDir dir(folder);
    if (dir.IsOpened()) {
        // build an array of all files in the given folder
        wxArrayString allfiles;
        wxString filename;
        bool found = dir.GetFirst(&filename, wxEmptyString, wxDIR_FILES | wxDIR_HIDDEN);
        while (found) {
            allfiles.Add(filename);
            found = dir.GetNext(&filename);
        }
        // delete all the .table/tree/colors/icons files
        for (size_t n = 0; n < allfiles.GetCount(); n++) {
            filename = allfiles[n];
            if ( filename.EndsWith(wxT(".colors")) || filename.EndsWith(wxT(".icons")) ||
                 filename.EndsWith(wxT(".table")) || filename.EndsWith(wxT(".tree")) ) {
                wxRemoveFile(folder + filename);
            }
        }
    }
}

// -----------------------------------------------------------------------------

void MainFrame::ConvertOldRules()
{
    if (inscript || viewptr->waitingforclick) return;
    
    if (generating) {
        // terminate generating loop and set command_pending flag
        Stop();
        command_pending = true;
        cmdevent.SetId(ID_CONVERT);
        return;
    }
    
    // look for deprecated .table/tree/colors/icons files and create corresponding .rule files
    
    wxString htmlinfo;
    bool aborted = false;
    int depcount = 0;       // number of deprecated files
    try {
        // look in the supplied Rules folder first, then in the user's rules folder
        depcount += ConvertRules(rulesdir, true, htmlinfo);
        depcount += ConvertRules(userrules, false, htmlinfo);
    }
    catch(const std::exception& e) {
        // display message set by throw std::runtime_error(...)
        Warning(wxString(e.what(),wxConvLocal));
        aborted = true;
        // nice to also show error message in help window
        htmlinfo += _("\n<p>*** CONVERSION ABORTED DUE TO ERROR ***\n<p>");
        htmlinfo += wxString(e.what(),wxConvLocal);
    }
    
    // display the results in the help window
    ShowCreatedRules(htmlinfo);
    
    if (!aborted && depcount > 0) {
        // ask user if it's ok to delete all the deprecated files
        int answer = wxMessageBox(_("Do you want to delete all the old .table/tree/colors/icons files?"),
                                  _("Delete deprecated files?"),
                                  wxICON_QUESTION | wxYES_NO, wxGetActiveWindow());
        if (answer == wxYES) {
            DeleteOldRules(rulesdir);
            DeleteOldRules(userrules);
        }
    }
}

// -----------------------------------------------------------------------------

wxString MainFrame::CreateRuleFiles(const wxSortedArrayString& deprecated,
                                    const wxSortedArrayString& ziprules)
{
    // use the given list of deprecated .table/tree/colors/icons files
    // (recently extracted from a .zip file and installed in userrules)
    // to create new .rule files, except those in ziprules (they were in
    // .zip file and have already been installed)
    wxString htmlinfo;
    bool aborted = false;
    try {
        // create an array of candidate .rule files to be created
        wxString rulefile;
        wxSortedArrayString candidates;
        for (size_t n = 0; n < deprecated.GetCount(); n++) {
            // add .rule file to candidates if it hasn't been added yet
            // and isn't in ziprules
            rulefile = deprecated[n].BeforeLast('.') + wxT(".rule");
            if ( (candidates.Index(rulefile) == wxNOT_FOUND) &&
                 (ziprules.Index(rulefile) == wxNOT_FOUND) ) {
                candidates.Add(rulefile);
            }
        }
        
        // look for .rule files of the form foo-*.rule and ignore any foo.rule
        // entries in candidates if foo.table and foo.tree don't exist
        // (ie. foo.colors and/or foo.icons is shared by foo-*.table/tree)
        wxSortedArrayString ignore;
        for (size_t n = 0; n < candidates.GetCount(); n++) {
            rulefile = candidates[n];
            wxString prefix = rulefile.BeforeLast('-');
            if (prefix.length() > 0) {
                wxString tablefile = prefix + wxT(".table");
                wxString treefile = prefix + wxT(".tree");
                if ( (deprecated.Index(tablefile) == wxNOT_FOUND) &&
                     (deprecated.Index(treefile) == wxNOT_FOUND) ) {
                    wxString sharedfile = prefix + wxT(".rule");
                    if (candidates.Index(sharedfile) != wxNOT_FOUND) {
                        ignore.Add(sharedfile);
                    }
                }
            }
            // unlike ConvertRules, we will overwrite any existing .rule files
            // (not in ziprules) in case the zip file's contents have changed
        }
        
        // non-ignored candidates are the .rule files that need to be created
        for (size_t n = 0; n < candidates.GetCount(); n++) {
            rulefile = candidates[n];
            if (ignore.Index(rulefile) == wxNOT_FOUND) {
                CreateOneRule(rulefile, userrules, deprecated, htmlinfo);
            }
        }
    }
    catch(const std::exception& e) {
        // display message set by throw std::runtime_error(...)
        Warning(wxString(e.what(),wxConvLocal));
        aborted = true;
        // nice to also show error message in help window
        htmlinfo += _("\n<p>*** CONVERSION ABORTED DUE TO ERROR ***\n<p>");
        htmlinfo += wxString(e.what(),wxConvLocal);
    }
    
    if (!aborted) {
        // delete all the deprecated files
        for (size_t n = 0; n < deprecated.GetCount(); n++) {
            wxRemoveFile(userrules + deprecated[n]);
        }
    }
    
    return htmlinfo;
}
