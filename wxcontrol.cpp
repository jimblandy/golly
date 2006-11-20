                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2006 Andrew Trevorrow and Tomas Rokicki.

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

#include "wxgolly.h"       // for wxGetApp, statusptr, viewptr
#include "wxutils.h"       // for BeginProgress, etc
#include "wxprefs.h"       // for hashing, etc
#include "wxrule.h"        // for ChangeRule
#include "wxstatus.h"      // for statusptr->...
#include "wxview.h"        // for viewptr->...
#include "wxscript.h"      // for inscript, PassKeyToScript
#include "wxmain.h"        // for MainFrame

// Control menu functions:

// -----------------------------------------------------------------------------

void MainFrame::ChangeGoToStop()
{
   /* single go/stop button is not yet implemented!!!
   gostopbutt->SetBitmapLabel(tbBitmaps[stop_index]);
   gostopbutt->Refresh(false, NULL);
   gostopbutt->Update();
   gostopbutt->SetToolTip(_("Stop generating"));
   */
}

// -----------------------------------------------------------------------------

void MainFrame::ChangeStopToGo()
{
   /* single go/stop button is not yet implemented!!!
   gostopbutt->SetBitmapLabel(tbBitmaps[go_index]);
   gostopbutt->Refresh(false, NULL);
   gostopbutt->Update();
   gostopbutt->SetToolTip(_("Start generating"));
   */
}

// -----------------------------------------------------------------------------

bool MainFrame::SaveStartingPattern()
{
   if ( curralgo->getGeneration() > startgen ) {
      // don't do anything if current gen count > starting gen
      return true;
   }
   
   // save current rule, scale, location, step size and hashing option
   startrule = wxString(curralgo->getrule(), wxConvLocal);
   startmag = viewptr->GetMag();
   viewptr->GetPos(startx, starty);
   startwarp = warp;
   starthash = hashing;
   
   if ( !savestart ) {
      // no need to save pattern; ResetPattern will load currfile
      // (note that currfile == tempstart if pattern created via OpenClipboard)
      startfile.Clear();
      return true;
   }

   // save starting pattern in tempstart file
   if ( hashing ) {
      // much faster to save hlife pattern in a macrocell file
      const char *err = WritePattern(tempstart, MC_format, 0, 0, 0, 0);
      if (err) {
         statusptr->ErrorMessage(wxString(err,wxConvLocal));
         // don't allow user to continue generating
         return false;
      }
   } else {
      // can only save qlife pattern if edges are within getcell/setcell limits
      bigint top, left, bottom, right;
      curralgo->findedges(&top, &left, &bottom, &right);      
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
      const char *err = WritePattern(tempstart, XRLE_format,
                                     itop, ileft, ibottom, iright);
      if (err) {
         statusptr->ErrorMessage(wxString(err,wxConvLocal));
         // don't allow user to continue generating
         return false;
      }
   }
   
   startfile = tempstart;   // ResetPattern will load tempstart
   return true;
}

// -----------------------------------------------------------------------------

void MainFrame::GoFaster()
{
   warp++;
   SetGenIncrement();
   // only need to refresh status bar
   UpdateStatus();
   if (generating && warp < 0) {
      whentosee -= statusptr->GetCurrentDelay();
   }
}

// -----------------------------------------------------------------------------

void MainFrame::GoSlower()
{
   if (warp > minwarp) {
      warp--;
      SetGenIncrement();
      // only need to refresh status bar
      UpdateStatus();
      if (generating && warp < 0) {
         whentosee += statusptr->GetCurrentDelay();
      }
   } else {
      wxBell();
   }
}

// -----------------------------------------------------------------------------

void MainFrame::GeneratePattern()
{
   if (generating || viewptr->drawingcells || viewptr->waitingforclick) {
      wxBell();
      return;
   }
   
   if (curralgo->isEmpty()) {
      statusptr->ErrorMessage(empty_pattern);
      return;
   }
   
   if (!SaveStartingPattern()) {
      return;
   }

   // for DisplayTimingInfo
   begintime = stopwatch->Time();
   begingen = curralgo->getGeneration().todouble();
   
   generating = true;               // avoid recursion
   ChangeGoToStop();
   wxGetApp().PollerReset();
   UpdateUserInterface(IsActive());
   
   if (warp < 0) {
      whentosee = stopwatch->Time() + statusptr->GetCurrentDelay();
   }
   int hypdown = 64;

   while (true) {
      if (warp < 0) {
         // slow down by only doing one gen every GetCurrentDelay() millisecs
         long currmsec = stopwatch->Time();
         if (currmsec >= whentosee) {
            curralgo->step();
            if (autofit) viewptr->FitInView(0);
            // don't call UpdateEverything() -- no need to update menu/tool/scroll bars
            UpdatePatternAndStatus();
            if (wxGetApp().Poller()->checkevents()) break;
            // add delay to current time rather than currmsec
            // otherwise pauses can occur in Win app???
            whentosee = stopwatch->Time() + statusptr->GetCurrentDelay();
         } else {
            // process events while we wait
            if (wxGetApp().Poller()->checkevents()) break;
            // don't hog CPU
            wxMilliSleep(1);     // keep small (ie. <= mindelay)
         }
      } else {
         // warp >= 0 so only show results every curralgo->getIncrement() gens
         curralgo->step();
         if (autofit) viewptr->FitInView(0);
         // don't call UpdateEverything() -- no need to update menu/tool/scroll bars
         UpdatePatternAndStatus();
         if (wxGetApp().Poller()->checkevents()) break;
         if (hyperspeed && curralgo->hyperCapable()) {
            hypdown--;
            if (hypdown == 0) {
               hypdown = 64;
               GoFaster();
            }
         }
      }
   }

   generating = false;

   // for DisplayTimingInfo
   endtime = stopwatch->Time();
   endgen = curralgo->getGeneration().todouble();
   
   ChangeStopToGo();
   
   // display the final pattern
   if (autofit) viewptr->FitInView(0);
   UpdateEverything();
}

// -----------------------------------------------------------------------------

void MainFrame::StopGenerating()
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
      endgen = curralgo->getGeneration().todouble();
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

void MainFrame::AdvanceOutsideSelection()
{
   if (generating || viewptr->drawingcells || viewptr->waitingforclick) return;

   if (!viewptr->SelectionExists()) {
      statusptr->ErrorMessage(no_selection);
      return;
   }

   if (curralgo->isEmpty()) {
      statusptr->ErrorMessage(empty_outside);
      return;
   }
   
   bigint top, left, bottom, right;
   curralgo->findedges(&top, &left, &bottom, &right);

   // check if selection encloses entire pattern
   if ( viewptr->seltop <= top && viewptr->selbottom >= bottom &&
        viewptr->selleft <= left && viewptr->selright >= right ) {
      statusptr->ErrorMessage(empty_outside);
      return;
   }

   // check if selection is completely outside pattern edges;
   // can't do this if qlife because it uses gen parity to decide which bits to draw
   if ( hashing &&
         ( viewptr->seltop > bottom || viewptr->selbottom < top ||
           viewptr->selleft > right || viewptr->selright < left ) ) {
      generating = true;
      ChangeGoToStop();
      wxGetApp().PollerReset();

      // step by one gen without changing gen count
      bigint savegen = curralgo->getGeneration();
      bigint saveinc = curralgo->getIncrement();
      curralgo->setIncrement(1);
      curralgo->step();
      curralgo->setIncrement(saveinc);
      curralgo->setGeneration(savegen);
   
      generating = false;
      ChangeStopToGo();
      
      // if pattern expanded then may need to clear ONE edge of selection!!!
      viewptr->ClearSelection();
      UpdateEverything();
      return;
   }

   // check that pattern is within setcell/getcell limits
   if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
      statusptr->ErrorMessage(_("Pattern is outside +/- 10^9 boundary."));
      return;
   }
   
   // create a new universe of same type
   lifealgo *newalgo;
   if ( hashing ) {
      newalgo = new hlifealgo();
      newalgo->setMaxMemory(maxhashmem);
   } else {
      newalgo = new qlifealgo();
   }
   newalgo->setpoll(wxGetApp().Poller());
   newalgo->setGeneration( curralgo->getGeneration() );
   
   // copy (and kill) live cells in selection to new universe
   int iseltop = viewptr->seltop.toint();
   int iselleft = viewptr->selleft.toint();
   int iselbottom = viewptr->selbottom.toint();
   int iselright = viewptr->selright.toint();
   if ( !viewptr->CopyRect(iseltop, iselleft, iselbottom, iselright,
                           curralgo, newalgo, true, _("Saving and erasing selection")) ) {
      // aborted, so best to restore selection
      if ( !newalgo->isEmpty() ) {
         newalgo->findedges(&top, &left, &bottom, &right);
         viewptr->CopyRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                           newalgo, curralgo, false, _("Restoring selection"));
      }
      delete newalgo;
      UpdateEverything();
      return;
   }
   
   // advance current universe by 1 generation
   generating = true;
   ChangeGoToStop();
   wxGetApp().PollerReset();
   curralgo->setIncrement(1);
   curralgo->step();
   generating = false;
   ChangeStopToGo();
   
   // note that we have to copy advanced pattern to new universe because
   // qlife uses gen parity to decide which bits to draw
   
   if ( !curralgo->isEmpty() ) {
      // find new edges and copy current pattern to new universe,
      // except for any cells that were created in selection
      curralgo->findedges(&top, &left, &bottom, &right);
      int itop = top.toint();
      int ileft = left.toint();
      int ibottom = bottom.toint();
      int iright = right.toint();
      int ht = ibottom - itop + 1;
      int cx, cy;
   
      // for showing accurate progress we need to add pattern height to pop count
      // in case this is a huge pattern with many blank rows
      double maxcount = curralgo->getPopulation().todouble() + ht;
      double accumcount = 0;
      int currcount = 0;
      bool abort = false;
      BeginProgress(_("Copying advanced pattern"));
   
      for ( cy=itop; cy<=ibottom; cy++ ) {
         currcount++;
         for ( cx=ileft; cx<=iright; cx++ ) {
            int skip = curralgo->nextcell(cx, cy);
            if (skip >= 0) {
               // found next live cell in this row
               cx += skip;
               
               // only copy cell if outside selection
               if ( cx < iselleft || cx > iselright ||
                    cy < iseltop || cy > iselbottom ) {
                  newalgo->setcell(cx, cy, 1);
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
   
   // switch to new universe (best to do this even if aborted)
   savestart = true;
   delete curralgo;
   curralgo = newalgo;
   SetGenIncrement();
   UpdateEverything();
}

// -----------------------------------------------------------------------------

void MainFrame::AdvanceSelection()
{
   if (generating || viewptr->drawingcells || viewptr->waitingforclick) return;

   if (!viewptr->SelectionExists()) {
      statusptr->ErrorMessage(no_selection);
      return;
   }

   if (curralgo->isEmpty()) {
      statusptr->ErrorMessage(empty_selection);
      return;
   }
   
   bigint top, left, bottom, right;
   curralgo->findedges(&top, &left, &bottom, &right);

   // check if selection is completely outside pattern edges
   if ( viewptr->seltop > bottom || viewptr->selbottom < top ||
        viewptr->selleft > right || viewptr->selright < left ) {
      statusptr->ErrorMessage(empty_selection);
      return;
   }

   // check if selection encloses entire pattern;
   // can't do this if qlife because it uses gen parity to decide which bits to draw
   if ( hashing &&
        viewptr->seltop <= top && viewptr->selbottom >= bottom &&
        viewptr->selleft <= left && viewptr->selright >= right ) {
      generating = true;
      ChangeGoToStop();
      wxGetApp().PollerReset();

      // step by one gen without changing gen count
      bigint savegen = curralgo->getGeneration();
      bigint saveinc = curralgo->getIncrement();
      curralgo->setIncrement(1);
      curralgo->step();
      curralgo->setIncrement(saveinc);
      curralgo->setGeneration(savegen);
   
      generating = false;
      ChangeStopToGo();
      
      // only need to clear 1-cell thick strips just outside selection!!!
      viewptr->ClearOutsideSelection();
      UpdateEverything();
      return;
   }
   
   // find intersection of selection and pattern to minimize work
   if (viewptr->seltop > top) top = viewptr->seltop;
   if (viewptr->selleft > left) left = viewptr->selleft;
   if (viewptr->selbottom < bottom) bottom = viewptr->selbottom;
   if (viewptr->selright < right) right = viewptr->selright;

   // check that intersection is within setcell/getcell limits
   if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
      statusptr->ErrorMessage(selection_too_big);
      return;
   }
   
   // create a new temporary universe
   lifealgo *tempalgo;
   tempalgo = new qlifealgo();         // qlife's setcell/getcell are faster
   tempalgo->setpoll(wxGetApp().Poller());
   
   // copy live cells in selection to temporary universe
   if ( viewptr->CopyRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                          curralgo, tempalgo, false, _("Saving selection")) ) {
      if ( tempalgo->isEmpty() ) {
         statusptr->ErrorMessage(empty_selection);
      } else {
         // advance temporary universe by one gen
         generating = true;
         ChangeGoToStop();
         wxGetApp().PollerReset();
         tempalgo->setIncrement(1);
         tempalgo->step();
         generating = false;
         ChangeStopToGo();
         
         // temporary pattern might have expanded
         bigint temptop, templeft, tempbottom, tempright;
         tempalgo->findedges(&temptop, &templeft, &tempbottom, &tempright);
         if (temptop < top) top = temptop;
         if (templeft < left) left = templeft;
         if (tempbottom > bottom) bottom = tempbottom;
         if (tempright > right) right = tempright;

         // but ignore live cells created outside selection edges
         if (top < viewptr->seltop) top = viewptr->seltop;
         if (left < viewptr->selleft) left = viewptr->selleft;
         if (bottom > viewptr->selbottom) bottom = viewptr->selbottom;
         if (right > viewptr->selright) right = viewptr->selright;
         
         // copy all cells in new selection from tempalgo to curralgo
         viewptr->CopyAllRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                              tempalgo, curralgo, _("Copying advanced selection"));
         savestart = true;

         UpdateEverything();
      }
   }
   
   delete tempalgo;
}

// -----------------------------------------------------------------------------

void MainFrame::NextGeneration(bool useinc)
{
   if (generating || viewptr->drawingcells || viewptr->waitingforclick) {
      // don't play sound here because it'll be heard if user holds down tab key
      // wxBell();
      return;
   }

   if (curralgo->isEmpty()) {
      statusptr->ErrorMessage(empty_pattern);
      return;
   }
   
   if (!SaveStartingPattern()) {
      return;
   }
      
   // curralgo->step() calls checkevents so set generating flag to avoid recursion
   generating = true;
   
   // avoid doing some things if NextGeneration is called from a script
   if (!inscript) {
      ChangeGoToStop();
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

   if (!inscript) {
      ChangeStopToGo();
      // autofit is only used when doing many gens
      if (autofit && useinc && curralgo->getIncrement() > bigint::one)
         viewptr->FitInView(0);
      UpdateEverything();
   }
}

// -----------------------------------------------------------------------------

void MainFrame::ToggleAutoFit()
{
   autofit = !autofit;
   // we only use autofit when generating; that's why the Auto Fit item
   // is in the Control menu and not in the View menu
   if (autofit && generating) {
      viewptr->FitInView(0);
      UpdateEverything();
   }
}

// -----------------------------------------------------------------------------

void MainFrame::ToggleHashing()
{
   if (generating) return;

   if ( global_liferules.hasB0notS8 && !hashing ) {
      statusptr->ErrorMessage(_("Hashing cannot be used with a B0-not-S8 rule."));
      return;
   }

   // check if current pattern is too big to use getcell/setcell
   bigint top, left, bottom, right;
   if ( !curralgo->isEmpty() ) {
      curralgo->findedges(&top, &left, &bottom, &right);
      if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
         statusptr->ErrorMessage(_("Pattern cannot be converted (outside +/- 10^9 boundary)."));
         // ask user if they want to continue anyway???
         return;
      }
   }

   // toggle hashing option and update status bar immediately
   hashing = !hashing;
   warp = 0;
   UpdateStatus();

   // create a new universe of the right flavor
   lifealgo *newalgo;
   if ( hashing ) {
      newalgo = new hlifealgo();
      newalgo->setMaxMemory(maxhashmem);
   } else {
      newalgo = new qlifealgo();
   }
   newalgo->setpoll(wxGetApp().Poller());
   
   // even though universes share a global rule table we still need to call setrule
   // due to internal differences in the handling of Wolfram rules
   newalgo->setrule( (char*)curralgo->getrule() );
   
   // set same gen count
   newalgo->setGeneration( curralgo->getGeneration() );

   if ( !curralgo->isEmpty() ) {
      // copy pattern in current universe to new universe
      int itop = top.toint();
      int ileft = left.toint();
      int ibottom = bottom.toint();
      int iright = right.toint();
      int ht = ibottom - itop + 1;
      int cx, cy;
   
      // for showing accurate progress we need to add pattern height to pop count
      // in case this is a huge pattern with many blank rows
      double maxcount = curralgo->getPopulation().todouble() + ht;
      double accumcount = 0;
      int currcount = 0;
      bool abort = false;
      BeginProgress(_("Converting pattern"));
   
      for ( cy=itop; cy<=ibottom; cy++ ) {
         currcount++;
         for ( cx=ileft; cx<=iright; cx++ ) {
            int skip = curralgo->nextcell(cx, cy);
            if (skip >= 0) {
               // found next live cell in this row
               cx += skip;
               newalgo->setcell(cx, cy, 1);
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
   delete curralgo;
   curralgo = newalgo;   
   SetGenIncrement();
   UpdateEverything();
}

// -----------------------------------------------------------------------------

void MainFrame::ToggleHyperspeed()
{
   if ( curralgo->hyperCapable() ) {
      hyperspeed = !hyperspeed;
   }
}

// -----------------------------------------------------------------------------

void MainFrame::ToggleHashInfo()
{
   if ( curralgo->hyperCapable() ) {
      hlifealgo::setVerbose(!hlifealgo::getVerbose()) ;
   }
}

// -----------------------------------------------------------------------------

void MainFrame::SetWarp(int newwarp)
{
   warp = newwarp;
   if (warp < minwarp) warp = minwarp;
   SetGenIncrement();
}

// -----------------------------------------------------------------------------

void MainFrame::ShowRuleDialog()
{
   if (generating) return;
   if (ChangeRule()) {
      // show rule in window title (file name doesn't change)
      SetWindowTitle(wxEmptyString);
   }
}
