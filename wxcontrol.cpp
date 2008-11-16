                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2008 Andrew Trevorrow and Tomas Rokicki.

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

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
   #include "wx/wx.h"      // for all others include the necessary headers
#endif

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"

#include "wxgolly.h"       // for wxGetApp, statusptr, viewptr, bigview
#include "wxutils.h"       // for BeginProgress, GetString, etc
#include "wxprefs.h"       // for allowundo, etc
#include "wxrule.h"        // for ChangeRule
#include "wxhelp.h"        // for LoadLexiconPattern
#include "wxstatus.h"      // for statusptr->...
#include "wxselect.h"      // for Selection
#include "wxview.h"        // for viewptr->...
#include "wxscript.h"      // for inscript, PassKeyToScript
#include "wxmain.h"        // for MainFrame
#include "wxundo.h"        // for undoredo->...
#include "wxalgos.h"       // for *_ALGO, algo_type, CreateNewUniverse, etc
#include "wxlayer.h"       // for currlayer, etc
#include "wxrender.h"      // for DrawView

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
   currlayer->startwarp = currlayer->warp;
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
            cloneptr->startwarp = cloneptr->warp;
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
   //!!! use CanWriteFormat(MC_format)???
   if ( currlayer->algo->hyperCapable() ) {
      // much faster to save pattern in a macrocell file
      const char* err = WritePattern(currlayer->tempstart, MC_format, 0, 0, 0, 0);
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
   // first restore step size, algorithm and starting pattern
   currlayer->warp = currlayer->startwarp;
   currlayer->algtype = currlayer->startalgo;

   if ( currlayer->startfile.IsEmpty() ) {
      // restore pattern from currfile
      LoadPattern(currlayer->currfile, wxEmptyString);
   } else {
      // restore pattern from startfile
      LoadPattern(currlayer->startfile, wxEmptyString);
   }
   // gen count has been reset to startgen
   
   // ensure savestart flag is correct
   currlayer->savestart = !currlayer->startfile.IsEmpty();
   
   // restore settings saved by SaveStartingPattern
   currlayer->currname = currlayer->startname;
   currlayer->algo->setrule(currlayer->startrule.mb_str(wxConvLocal));
   currlayer->dirty = currlayer->startdirty;
   if (restoreview) {
      viewptr->SetPosMag(currlayer->startx, currlayer->starty, currlayer->startmag);
   }

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
            cloneptr->warp = cloneptr->startwarp;
            // also synchronize dirty flags and update items in Layer menu
            cloneptr->dirty = currlayer->dirty;
            mainptr->UpdateLayerItem(i);
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
         currlayer->undoredo->RememberGenFinish();
      } else if (resetundo) {
         // wind back the undo history to the starting pattern
         currlayer->undoredo->SyncUndoHistory();
      }
   }
}

// -----------------------------------------------------------------------------

void MainFrame::RestorePattern(bigint& gen, const wxString& filename,
                               bigint& x, bigint& y, int mag, int warp)
{
   // called to undo/redo a generating change
   if (gen == currlayer->startgen) {
      // restore starting pattern (false means don't call SyncUndoHistory)
      ResetPattern(false);
   } else {
      // restore pattern in given filename
      currlayer->warp = warp;

      // false means don't update status bar (algorithm should NOT change)
      LoadPattern(filename, wxEmptyString, false);
      
      if (gen != currlayer->algo->getGeneration()) {
         // current gen will be 0 if filename could not be loaded
         // for some reason, so best to set correct gen count
         currlayer->algo->setGeneration(gen);
      }
      
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

   //!!! need IsParityShifted() method???
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
      newalgo->setrule(currlayer->algo->getrule());
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
   currlayer->warp++;
   SetGenIncrement();
   // only need to refresh status bar
   UpdateStatus();
   if (generating && currlayer->warp < 0) {
      whentosee -= statusptr->GetCurrentDelay();
   }
}

// -----------------------------------------------------------------------------

void MainFrame::GoSlower()
{
   if (currlayer->warp > minwarp) {
      currlayer->warp--;
      SetGenIncrement();
      // only need to refresh status bar
      UpdateStatus();
      if (generating && currlayer->warp < 0) {
         if (currlayer->warp == -1) {
            // need to initialize whentosee rather than increment it
            whentosee = stopwatch->Time() + statusptr->GetCurrentDelay();
         } else {
            whentosee += statusptr->GetCurrentDelay();
         }
      }
   } else {
      wxBell();
   }
}

// -----------------------------------------------------------------------------

void MainFrame::DisplayPattern()
{
   // this routine is only used by GeneratePattern();
   // it's similar to UpdatePatternAndStatus() but if tiled windows exist
   // it only updates the current tile if possible; ie. it's not a clone
   // and tile views aren't synchronized
   
   if (!IsIconized()) {
      if (tilelayers && numlayers > 1 && !syncviews && currlayer->cloneid == 0) {
         // only update the current tile
         #ifdef __WXMSW__
            viewptr->Refresh(false);
            viewptr->Update();
         #else
            // avoid background being erased on Mac/Linux!!!???
            wxClientDC dc(viewptr);
            DrawView(dc, viewptr->tileindex);
         #endif
      } else {
         // update main viewport window, possibly including all tile windows
         // (tile windows are children of bigview)
         if (numlayers > 1 && (stacklayers || tilelayers)) {
            bigview->Refresh(false);
            bigview->Update();
         } else {
            #ifdef __WXMSW__
               viewptr->Refresh(false);
               viewptr->Update();
            #else
               // avoid background being erased on Mac/Linux!!!???
               wxClientDC dc(viewptr);
               DrawView(dc, viewptr->tileindex);
            #endif
         }
      }
      if (showstatus) {
         statusptr->CheckMouseLocation(IsActive());
         statusptr->Refresh(false);
         statusptr->Update();
      }
   }
}

// -----------------------------------------------------------------------------

void MainFrame::GeneratePattern()
{
   if (generating || viewptr->drawingcells || viewptr->waitingforclick) {
      wxBell();
      return;
   }

   lifealgo* curralgo = currlayer->algo;
   if (curralgo->isEmpty()) {
      statusptr->ErrorMessage(empty_pattern);
      return;
   }
   
   if (!SaveStartingPattern()) {
      return;
   }
      
   // GeneratePattern is never called while running a script so no need
   // to test inscript or currlayer->stayclean
   if (allowundo) currlayer->undoredo->RememberGenStart();

   // for DisplayTimingInfo
   begintime = stopwatch->Time();
   begingen = curralgo->getGeneration().todouble();

   // for hyperspeed
   int hypdown = 64;
   
   generating = true;               // avoid recursion
   wxGetApp().PollerReset();
   UpdateUserInterface(IsActive());
   
   // only show hashing info while generating, otherwise Mac app can crash
   // after a paste due to hlifealgo::resize() calling lifestatus() which
   // then causes the viewport to be repainted for some inexplicable reason
   lifealgo::setVerbose( currlayer->showhashinfo );

   if (currlayer->warp < 0)
      whentosee = stopwatch->Time() + statusptr->GetCurrentDelay();
   
   while (true) {
      if (currlayer->warp < 0) {
         // slow down by only doing one gen every GetCurrentDelay() millisecs
         long currmsec = stopwatch->Time();
         if (currmsec >= whentosee) {
            if (wxGetApp().Poller()->checkevents()) break;
            curralgo->step();
            if (currlayer->autofit) viewptr->FitInView(0);
            DisplayPattern();
            // add delay to current time rather than currmsec
            whentosee = stopwatch->Time() + statusptr->GetCurrentDelay();
         } else {
            // process events while we wait
            if (wxGetApp().Poller()->checkevents()) break;
            // don't hog CPU
            wxMilliSleep(1);     // keep small (ie. <= mindelay)
         }
      } else {
         // warp >= 0 so only show results every curralgo->getIncrement() gens
         if (wxGetApp().Poller()->checkevents()) break;
         curralgo->step();
         if (currlayer->autofit) viewptr->FitInView(0);
         DisplayPattern();
         if (currlayer->hyperspeed && curralgo->hyperCapable()) {
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
   endgen = curralgo->getGeneration().todouble();
   
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
   
   DoPendingAction(true);     // true means can restart generating loop
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
         case wxID_UNDO:         currlayer->undoredo->UndoChange(); break;
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
               cmdevent.SetEventObject(mainptr);
               mainptr->ProcessEvent(cmdevent);
               
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
      viewptr->ProcessEvent(mouseevent);
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

void MainFrame::Stop()
{
   if (inscript) {
      PassKeyToScript(WXK_ESCAPE);
   } else if (generating) {
      wxGetApp().PollerInterrupt();
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
      s.Printf(_("%g gens in %g secs (%g gens/sec)"), gens, secs, gens / secs);
      statusptr->DisplayMessage(s);
   }
}

// -----------------------------------------------------------------------------

void MainFrame::NextGeneration(bool useinc)
{
   if (generating || viewptr->drawingcells || viewptr->waitingforclick) {
      // don't play sound here because it'll be heard if user holds down tab key
      // wxBell();
      return;
   }

   // best if generating stops after running a script like oscar.py or goto.py
   if (inscript) stop_after_script = true;

   lifealgo* curralgo = currlayer->algo;
   if (curralgo->isEmpty()) {
      statusptr->ErrorMessage(empty_pattern);
      return;
   }
   
   if (!SaveStartingPattern()) {
      return;
   }

   if (allowundo) {
      if (currlayer->stayclean) {
         // script has called run/step after a command (eg. new)
         // has set stayclean true by calling MarkLayerClean
         if (curralgo->getGeneration() == currlayer->startgen) {
            // starting pattern has just been saved so we need to remember
            // this gen change in case user does a Reset after script ends
            // (RememberGenFinish will be called at the end of RunScript)
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

   // curralgo->step() calls checkevents so set generating flag to avoid recursion
   generating = true;

   // only show hashing info while generating
   lifealgo::setVerbose( currlayer->showhashinfo );
   
   // avoid doing some things if NextGeneration is called from a script;
   // ie. by a run/step command
   if (!inscript) {
      wxGetApp().PollerReset();
      viewptr->CheckCursor(IsActive());
   }

   if (useinc) {
      // step by current increment
      if (curralgo->getIncrement() > bigint::one && !inscript) {
         UpdateToolBar(IsActive());
         UpdateMenuItems(IsActive());
      }
      curralgo->step();
   } else {
      // make sure we only step by one gen
      bigint saveinc = curralgo->getIncrement();
      curralgo->setIncrement(1);
      curralgo->step();
      curralgo->setIncrement(saveinc);
   }

   generating = false;

   lifealgo::setVerbose(0);
   
   if (!inscript) {
      // autofit is only used when doing many gens
      if (currlayer->autofit && useinc && curralgo->getIncrement() > bigint::one)
         viewptr->FitInView(0);
      UpdateEverything();
   }

   if (allowundo && !currlayer->stayclean)
      currlayer->undoredo->RememberGenFinish();
   
   if (!inscript)
      DoPendingAction(false);     // true means don't restart generating loop
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
   if (generating)
      lifealgo::setVerbose( currlayer->showhashinfo );
}

// -----------------------------------------------------------------------------

void MainFrame::SetWarp(int newwarp)
{
   currlayer->warp = newwarp;
   if (currlayer->warp < minwarp) currlayer->warp = minwarp;
   SetGenIncrement();
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
      
      // check if rule actually changed
      wxString newrule = wxString(currlayer->algo->getrule(), wxConvLocal);
      if (oldrule != newrule) {
         // rule change might have changed the number of cell states;
         // if there are fewer states then pattern might change
         int newmaxstate = currlayer->algo->NumCellStates() - 1;
         if (newmaxstate < oldmaxstate && !currlayer->algo->isEmpty()) {
            ReduceCellStates(newmaxstate);
         }
         
         // pattern might have changed or new rule might have changed colors
         UpdateEverything();
         
         if (allowundo) {
            currlayer->undoredo->RememberRuleChange(oldrule);
         }
      }
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
   
   bool rulechanged = false;
   wxString oldrule = wxString(currlayer->algo->getrule(), wxConvLocal);

   // change algorithm type and update status bar immediately
   algo_type oldalgo = currlayer->algtype;
   currlayer->algtype = newalgotype;
   currlayer->warp = 0;
   UpdateStatus();

   // create a new universe of the requested flavor
   lifealgo* newalgo = CreateNewUniverse(newalgotype);
   
   if (inundoredo) {
      // switch to given newrule (no error should occur)
      const char* err = newalgo->setrule( newrule.mb_str(wxConvLocal) );
      if (err) Warning(_("Bug detected in ChangeAlgorithm!"));
   } else {
      const char* err;
      if (newrule.IsEmpty()) {
         // try to use same rule
         err = newalgo->setrule( currlayer->algo->getrule() );
      } else {
         // switch to newrule (ChangeRule has called ChangeAlgorithm)
         err = newalgo->setrule( newrule.mb_str(wxConvLocal) );
         rulechanged = true;
      }
      if (err) {
         // switch to new algo's default rule
         newalgo->setrule( newalgo->DefaultRule() );
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
      
      lifealgo* curralgo = currlayer->algo;
      
      // need to check for state change if new algo has fewer states than old algo
      int newmaxstate = newalgo->NumCellStates() - 1;
   
      for ( cy=itop; cy<=ibottom; cy++ ) {
         currcount++;
         for ( cx=ileft; cx<=iright; cx++ ) {
            int skip = curralgo->nextcell(cx, cy, v);
            if (skip >= 0) {
               // found next live cell in this row
               cx += skip;
               if (v > newmaxstate) {
                  // reduce v to largest state in new algo
                  if (savechanges) currlayer->undoredo->SaveCellChange(cx, cy, v, newmaxstate);
                  v = newmaxstate;
                  patternchanged = true;
               }
               newalgo->setcell(cx, cy, v);
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
               statusptr->ErrorMessage(_("Rule has changed and pattern has changed (new algorithm has fewer states)."));
            } else {
               // don't beep
               statusptr->DisplayMessage(_("Rule has changed."));
            }
         } else {
            // ChangeRule called ChangeAlgorithm
            if (patternchanged) {
               statusptr->ErrorMessage(_("Algorithm has changed and pattern has changed (new algorithm has fewer states)."));
            } else {
               // don't beep
               statusptr->DisplayMessage(_("Algorithm has changed."));
            }
         }
      } else if (patternchanged) {
         statusptr->ErrorMessage(_("Pattern has changed (new algorithm has fewer states)."));
      }
      
      if (!inscript) {
         UpdateEverything();
      }
   }

   if (savechanges) {
      currlayer->undoredo->RememberAlgoChange(oldalgo, oldrule);
   }
}
