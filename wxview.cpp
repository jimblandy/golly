                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2010 Andrew Trevorrow and Tomas Rokicki.

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

#include "wx/file.h"       // for wxFile
#include "wx/dcbuffer.h"   // for wxBufferedPaintDC
#if wxUSE_TOOLTIPS
   #include "wx/tooltip.h" // for wxToolTip
#endif

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "viewport.h"

#include "wxgolly.h"       // for mainptr, statusptr
#include "wxutils.h"       // for Warning, Fatal, Beep
#include "wxprefs.h"       // for showgridlines, canchangerule, etc
#include "wxhelp.h"        // for ShowHelp
#include "wxmain.h"        // for mainptr->...
#include "wxstatus.h"      // for statusptr->...
#include "wxrender.h"      // for CreatePasteImage, DrawView, DrawSelection, etc
#include "wxscript.h"      // for inscript, PassKeyToScript
#include "wxselect.h"      // for Selection
#include "wxedit.h"        // for UpdateEditBar, ToggleEditBar, etc
#include "wxundo.h"        // for currlayer->undoredo->...
#include "wxalgos.h"       // for algo_type, *_ALGO, CreateNewUniverse, etc
#include "wxlayer.h"       // for currlayer, ResizeLayers, etc
#include "wxtimeline.h"    // for StartStopRecording, DeleteTimeline, etc
#include "wxview.h"

#ifdef __WXMAC__
   #include <Carbon/Carbon.h>       // for Button
#endif

// This module implements a viewport window for editing and viewing patterns.

// -----------------------------------------------------------------------------

const int DRAG_RATE = 20;           // call OnDragTimer 50 times per sec

static bool stopdrawing = false;    // terminate a draw done while generating?
static bool slowdraw = false;       // do slow cell drawing via UpdateView?

static wxString oldrule;            // rule before readclipboard is called
static wxString newrule;            // rule after readclipboard is called
static int newalgo;                 // new algo needed by readclipboard

// remember which translucent button was clicked, and when
static control_id clickedcontrol = NO_CONTROL;
static long clicktime;

// panning buttons are treated differently
#define PANNING_CONTROL (clickedcontrol >= NW_CONTROL && \
                         clickedcontrol <= SE_CONTROL && \
                         clickedcontrol != MIDDLE_CONTROL)

// -----------------------------------------------------------------------------

// event table and handlers:

BEGIN_EVENT_TABLE(PatternView, wxWindow)
   EVT_PAINT            (           PatternView::OnPaint)
   EVT_SIZE             (           PatternView::OnSize)
   EVT_KEY_DOWN         (           PatternView::OnKeyDown)
   EVT_KEY_UP           (           PatternView::OnKeyUp)
   EVT_CHAR             (           PatternView::OnChar)
   EVT_LEFT_DOWN        (           PatternView::OnMouseDown)
   EVT_LEFT_DCLICK      (           PatternView::OnMouseDown)
   EVT_LEFT_UP          (           PatternView::OnMouseUp)
#if wxCHECK_VERSION(2, 8, 0)
   EVT_MOUSE_CAPTURE_LOST (         PatternView::OnMouseCaptureLost)
#endif
   EVT_RIGHT_DOWN       (           PatternView::OnRMouseDown)
   EVT_RIGHT_DCLICK     (           PatternView::OnRMouseDown)
   EVT_MOTION           (           PatternView::OnMouseMotion)
   EVT_ENTER_WINDOW     (           PatternView::OnMouseEnter)
   EVT_LEAVE_WINDOW     (           PatternView::OnMouseExit)
   EVT_MOUSEWHEEL       (           PatternView::OnMouseWheel)
   EVT_TIMER            (wxID_ANY,  PatternView::OnDragTimer)
   EVT_SCROLLWIN        (           PatternView::OnScroll)
   EVT_ERASE_BACKGROUND (           PatternView::OnEraseBackground)
END_EVENT_TABLE()

// -----------------------------------------------------------------------------

// most editing operations are limited to absolute coordinates <= 10^9 because
// getcell and setcell only take int parameters (the limits must be smaller
// than INT_MIN and INT_MAX to avoid boundary conditions)
static bigint min_coord = -1000000000;
static bigint max_coord = +1000000000;

bool PatternView::OutsideLimits(bigint& t, bigint& l, bigint& b, bigint& r)
{
   return ( t < min_coord || l < min_coord ||
            b > max_coord || r > max_coord );
}

// -----------------------------------------------------------------------------

bool PatternView::SelectionExists()
{
   return currlayer->currsel.Exists();
}

// -----------------------------------------------------------------------------

bool PatternView::CopyRect(int itop, int ileft, int ibottom, int iright,
                           lifealgo* srcalgo, lifealgo* destalgo,
                           bool erasesrc, const wxString& progmsg)
{
   int wd = iright - ileft + 1;
   int ht = ibottom - itop + 1;
   int cx, cy;
   double maxcount = (double)wd * (double)ht;
   int cntr = 0;
   int v = 0;
   bool abort = false;
   
   // copy (and erase if requested) live cells from given rect
   // in source universe to same rect in destination universe
   BeginProgress(progmsg);
   for ( cy=itop; cy<=ibottom; cy++ ) {
      for ( cx=ileft; cx<=iright; cx++ ) {
         int skip = srcalgo->nextcell(cx, cy, v);
         if (skip + cx > iright)
            skip = -1;           // pretend we found no more live cells
         if (skip >= 0) {
            // found next live cell
            cx += skip;
            destalgo->setcell(cx, cy, v);
            if (erasesrc) srcalgo->setcell(cx, cy, 0);
         } else {
            cx = iright + 1;     // done this row
         }
         cntr++;
         if ((cntr % 4096) == 0) {
            double prog = ((cy - itop) * (double)(iright - ileft + 1) +
                           (cx - ileft)) / maxcount;
            abort = AbortProgress(prog, wxEmptyString);
            if (abort) break;
         }
      }
      if (abort) break;
   }
   if (erasesrc) srcalgo->endofpattern();
   destalgo->endofpattern();
   EndProgress();
   
   return !abort;
}

// -----------------------------------------------------------------------------

void PatternView::CopyAllRect(int itop, int ileft, int ibottom, int iright,
                              lifealgo* srcalgo, lifealgo* destalgo,
                              const wxString& progmsg)
{
   int wd = iright - ileft + 1;
   int ht = ibottom - itop + 1;
   int cx, cy;
   double maxcount = (double)wd * (double)ht;
   int cntr = 0;
   bool abort = false;
   
   // copy all cells from given rect in srcalgo to same rect in destalgo
   BeginProgress(progmsg);
   for ( cy=itop; cy<=ibottom; cy++ ) {
      for ( cx=ileft; cx<=iright; cx++ ) {
         destalgo->setcell(cx, cy, srcalgo->getcell(cx, cy));
         cntr++;
         if ((cntr % 4096) == 0) {
            abort = AbortProgress((double)cntr / maxcount, wxEmptyString);
            if (abort) break;
         }
      }
      if (abort) break;
   }
   destalgo->endofpattern();
   EndProgress();
}

// -----------------------------------------------------------------------------

void PatternView::ClearSelection()
{
   currlayer->currsel.Clear();
}

// -----------------------------------------------------------------------------

void PatternView::ClearOutsideSelection()
{
   currlayer->currsel.ClearOutside();
}

// -----------------------------------------------------------------------------

void PatternView::CutSelection()
{
   if (!SelectionExists()) return;

   if (mainptr->generating) {
      // terminate generating loop and set command_pending flag
      mainptr->Stop();
      mainptr->command_pending = true;
      mainptr->cmdevent.SetId(ID_CUT);
      return;
   }

   currlayer->currsel.CopyToClipboard(true);
}

// -----------------------------------------------------------------------------

void PatternView::CopySelection()
{
   if (!SelectionExists()) return;

   if (mainptr->generating) {
      // terminate generating loop and set command_pending flag
      mainptr->Stop();
      mainptr->command_pending = true;
      mainptr->cmdevent.SetId(ID_COPY);
      return;
   }

   currlayer->currsel.CopyToClipboard(false);
}

// -----------------------------------------------------------------------------

static bool CellInGrid(const bigint xpos, const bigint ypos)
{
   // return true if cell at xpos,ypos is within bounded grid
   if (currlayer->algo->gridwd > 0 &&
         (xpos < currlayer->algo->gridleft ||
          xpos > currlayer->algo->gridright)) return false;
   
   if (currlayer->algo->gridht > 0 &&
         (ypos < currlayer->algo->gridtop ||
          ypos > currlayer->algo->gridbottom)) return false;
   
   return true;
}

// -----------------------------------------------------------------------------

static bool PointInGrid(int x, int y)
{
   // is given viewport location also in grid?
   if (currlayer->algo->gridwd == 0 && currlayer->algo->gridht == 0) {
      // unbounded grid
      return true;
   }
   pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
   return CellInGrid(cellpos.first, cellpos.second);
}

// -----------------------------------------------------------------------------

void PatternView::SetPasteRect(wxRect& rect, bigint& wd, bigint& ht)
{
   int x, y, pastewd, pasteht;
   int mag = currlayer->view->getmag();
   
   // find cell coord of current paste cursor position
   pair<bigint, bigint> pcell = currlayer->view->at(pastex, pastey);

   // determine bottom right cell
   bigint right = pcell.first;     right += wd;    right -= 1;
   bigint bottom = pcell.second;   bottom += ht;   bottom -= 1;
   
   // best to use same method as in Selection::Visible
   pair<int,int> lt = currlayer->view->screenPosOf(pcell.first, pcell.second, currlayer->algo);
   pair<int,int> rb = currlayer->view->screenPosOf(right, bottom, currlayer->algo);

   if (mag > 0) {
      // move rb to pixel at bottom right corner of cell
      rb.first += (1 << mag) - 1;
      rb.second += (1 << mag) - 1;
      if (mag > 1) {
         // avoid covering gaps at scale 1:4 and above
         rb.first--;
         rb.second--;
      }
   }

   x = lt.first;
   y = lt.second;
   pastewd = rb.first - lt.first + 1;
   pasteht = rb.second - lt.second + 1;

   // this should never happen but play safe
   if (pastewd <= 0) pastewd = 1;
   if (pasteht <= 0) pasteht = 1;
   
   rect = wxRect(x, y, pastewd, pasteht);
   int xoffset, yoffset;
   int cellsize = 1 << mag;      // only used if mag > 0
   int gap = 1;                  // ditto
   if (mag == 1) gap = 0;        // but no gap between cells at scale 1:2
   switch (plocation) {
      case TopLeft:
         break;
      case TopRight:
         xoffset = mag > 0 ? -(pastewd - cellsize + gap) : -pastewd + 1;
         rect.Offset(xoffset, 0);
         break;
      case BottomRight:
         xoffset = mag > 0 ? -(pastewd - cellsize + gap) : -pastewd + 1;
         yoffset = mag > 0 ? -(pasteht - cellsize + gap) : -pasteht + 1;
         rect.Offset(xoffset, yoffset);
         break;
      case BottomLeft:
         yoffset = mag > 0 ? -(pasteht - cellsize + gap) : -pasteht + 1;
         rect.Offset(0, yoffset);
         break;
      case Middle:
         xoffset = mag > 0 ? -(pastewd / cellsize / 2) * cellsize : -pastewd / 2;
         yoffset = mag > 0 ? -(pasteht / cellsize / 2) * cellsize : -pasteht / 2;
         rect.Offset(xoffset, yoffset);
         break;
   }
}

// -----------------------------------------------------------------------------

void PatternView::PasteTemporaryToCurrent(lifealgo* tempalgo, bool toselection,
                                          bigint top, bigint left, bigint bottom, bigint right)
{
   // make sure given edges are within getcell/setcell limits
   if ( OutsideLimits(top, left, bottom, right) ) {
      statusptr->ErrorMessage(_("Clipboard pattern is too big."));
      return;
   }
   int itop = top.toint();
   int ileft = left.toint();
   int ibottom = bottom.toint();
   int iright = right.toint();
   bigint wd = iright - ileft + 1;
   bigint ht = ibottom - itop + 1;
   
   if ( toselection ) {
      if ( !currlayer->currsel.CanPaste(wd, ht, top, left) ) {
         statusptr->ErrorMessage(_("Clipboard pattern is bigger than selection."));
         return;
      }
      // top and left have been set to the selection's top left corner

   } else {
      // ask user where to paste the clipboard pattern
      statusptr->DisplayMessage(_("Click where you want to paste..."));

      // temporarily change cursor to cross
      wxCursor* savecurs = currlayer->curs;
      currlayer->curs = curs_cross;
      CheckCursor(true);

      // create image for drawing pattern to be pasted; note that given box
      // is not necessarily the minimal bounding box because clipboard pattern
      // might have blank borders (in fact it could be empty)
      wxRect bbox = wxRect(ileft, itop, wd.toint(), ht.toint());
      CreatePasteImage(tempalgo, bbox);

      waitingforclick = true;
      mainptr->EnableAllMenus(false);  // disable all menu items
      mainptr->UpdateToolBar(false);   // disable all tool bar buttons
      UpdateLayerBar(false);           // disable all layer bar buttons
      UpdateEditBar(false);            // disable all edit bar buttons
      CaptureMouse();                  // get mouse down event even if outside view
      pasterect = wxRect(-1,-1,0,0);

      while (waitingforclick) {
         wxPoint pt = ScreenToClient( wxGetMousePosition() );
         pastex = pt.x;
         pastey = pt.y;
         if (PointInView(pt.x, pt.y)) {
            // determine new paste rectangle
            wxRect newrect;
            SetPasteRect(newrect, wd, ht);
            if (newrect != pasterect) {
               // draw new pasterect
               pasterect = newrect;
               Refresh(false);
               // don't update immediately
            }
         } else {
            // mouse outside viewport so erase old pasterect if necessary
            if ( pasterect.width > 0 ) {
               pasterect = wxRect(-1,-1,0,0);
               Refresh(false);
               // don't update immediately
            }
         }
         wxMilliSleep(10);             // don't hog CPU
         wxGetApp().Yield(true);
         // make sure viewport retains focus so we can use keyboard shortcuts
         SetFocus();
         // waitingforclick becomes false if OnMouseDown is called
         #ifdef __WXMAC__
            // need to check for click here because OnMouseDown does not
            // get called if click is in menu bar or in another window;
            // is this a CaptureMouse bug in wxMac???
            if ( waitingforclick && Button() ) {
               pt = ScreenToClient( wxGetMousePosition() );
               pastex = pt.x;
               pastey = pt.y;
               waitingforclick = false;
               FlushEvents(mDownMask + mUpMask, 0);   // avoid wx seeing click
            }
         #endif
      }

      if ( HasCapture() ) ReleaseMouse();
      mainptr->EnableAllMenus(true);
      DestroyPasteImage();
   
      // restore cursor
      currlayer->curs = savecurs;
      CheckCursor(mainptr->IsActive());
      
      if ( pasterect.width > 0 ) {
         // erase old pasterect
         Refresh(false);
         // no need to update immediately
         // Update();
      }
      
      if ( !PointInView(pastex, pastey) ||
           // allow paste if any corner of pasterect is within grid
           !( PointInGrid(pastex, pastey) ||
              PointInGrid(pastex+pasterect.width-1, pastey) ||
              PointInGrid(pastex, pastey+pasterect.height-1) ||
              PointInGrid(pastex+pasterect.width-1, pastey+pasterect.height-1) ) ) {
         statusptr->DisplayMessage(_("Paste aborted."));
         return;
      }
      
      // set paste rectangle's top left cell coord
      pair<bigint, bigint> clickpos = currlayer->view->at(pastex, pastey);
      top = clickpos.second;
      left = clickpos.first;
      bigint halfht = ht;
      bigint halfwd = wd;
      halfht.div2();
      halfwd.div2();
      if (currlayer->view->getmag() > 1) {
         if (ht.even()) halfht -= 1;
         if (wd.even()) halfwd -= 1;
      }
      switch (plocation) {
         case TopLeft:     /* no change*/ break;
         case TopRight:    left -= wd; left += 1; break;
         case BottomRight: left -= wd; left += 1; top -= ht; top += 1; break;
         case BottomLeft:  top -= ht; top += 1; break;
         case Middle:      left -= halfwd; top -= halfht; break;
      }
   }

   // check that paste rectangle is within edit limits
   bottom = top;   bottom += ht;   bottom -= 1;
   right = left;   right += wd;    right -= 1;
   if ( OutsideLimits(top, left, bottom, right) ) {
      statusptr->ErrorMessage(_("Pasting is not allowed outside +/- 10^9 boundary."));
      return;
   }

   // selection might change if grid becomes smaller,
   // so save current selection for RememberRuleChange/RememberAlgoChange
   SaveCurrentSelection();
   
   // pasting clipboard pattern can cause a rule change
   int oldmaxstate = currlayer->algo->NumCellStates() - 1;
   if (canchangerule > 0 && oldrule != newrule) {
      const char* err = currlayer->algo->setrule( newrule.mb_str(wxConvLocal) );
      // setrule can fail if readclipboard loaded clipboard pattern into
      // a different type of algo (newalgo)
      if (err) {
         // allow rule change to cause algo change
         mainptr->ChangeAlgorithm(newalgo, newrule);
      } else {
         // show new rule in title bar
         mainptr->SetWindowTitle(wxEmptyString);
         // if grid is bounded then remove any live cells outside grid edges
         if (currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0) {
            mainptr->ClearOutsideGrid();
         }
         // rule change might have changed the number of cell states;
         // if there are fewer states then pattern might change
         int newmaxstate = currlayer->algo->NumCellStates() - 1;
         if (newmaxstate < oldmaxstate && !currlayer->algo->isEmpty()) {
            mainptr->ReduceCellStates(newmaxstate);
         }
         // switch to default colors for new rule
         UpdateLayerColors();
         if (allowundo && !currlayer->stayclean)
            currlayer->undoredo->RememberRuleChange(oldrule);
      }
   }

   // set pastex,pastey to top left cell of paste rectangle
   pastex = left.toint();
   pastey = top.toint();

   // save cell changes if undo/redo is enabled and script isn't constructing a pattern
   bool savecells = allowundo && !currlayer->stayclean;
   if (savecells && inscript) SavePendingChanges();

   // don't paste cells outside bounded grid
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

   // copy pattern from temporary universe to current universe
   int tx, ty, cx, cy;
   double maxcount = wd.todouble() * ht.todouble();
   int cntr = 0;
   bool abort = false;
   bool pattchanged = false;
   bool reduced = false;
   lifealgo* curralgo = currlayer->algo;
   int maxstate = curralgo->NumCellStates() - 1;

   BeginProgress(_("Pasting pattern"));
   
   // we can speed up pasting sparse patterns by using nextcell in these cases:
   // - if using Or mode
   // - if current universe is empty
   // - if paste rect is outside current pattern edges
   bool usenextcell;
   if ( pmode == Or || curralgo->isEmpty() ) {
      usenextcell = true;
   } else {
      bigint ctop, cleft, cbottom, cright;
      curralgo->findedges(&ctop, &cleft, &cbottom, &cright);
      usenextcell = top > cbottom || bottom < ctop || left > cright || right < cleft;
   }
   
   if ( usenextcell && pmode == And ) {
      // current universe is empty or paste rect is outside current pattern edges
      // so don't change any cells
   } else if ( usenextcell ) {
      int newstate = 0;
      cy = pastey;
      for ( ty=itop; ty<=ibottom; ty++ ) {
         cx = pastex;
         for ( tx=ileft; tx<=iright; tx++ ) {
            int skip = tempalgo->nextcell(tx, ty, newstate);
            if (skip + tx > iright)
               skip = -1;           // pretend we found no more live cells
            if (skip >= 0) {
               // found next live cell so paste it into current universe
               tx += skip;
               cx += skip;
               if (cx >= gleft && cx <= gright && cy >= gtop && cy <= gbottom) {
                  int currstate = curralgo->getcell(cx, cy);
                  if (currstate != newstate) {
                     if (newstate > maxstate) {
                        newstate = maxstate;
                        reduced = true;
                     }
                     curralgo->setcell(cx, cy, newstate);
                     pattchanged = true;
                     if (savecells) currlayer->undoredo->SaveCellChange(cx, cy, currstate, newstate);
                  }
               }
               cx++;
            } else {
               tx = iright + 1;     // done this row
            }
            cntr++;
            if ((cntr % 4096) == 0) {
               double prog = ((ty - itop) * (double)(iright - ileft + 1) +
                              (tx - ileft)) / maxcount;
               abort = AbortProgress(prog, wxEmptyString);
               if (abort) break;
            }
         }
         if (abort) break;
         cy++;
      }
   } else {
      // have to use slower getcell/setcell calls
      int tempstate, currstate;
      int numstates = curralgo->NumCellStates();
      cy = pastey;
      for ( ty=itop; ty<=ibottom; ty++ ) {
         cx = pastex;
         for ( tx=ileft; tx<=iright; tx++ ) {
            tempstate = tempalgo->getcell(tx, ty);
            currstate = curralgo->getcell(cx, cy);
            if (cx >= gleft && cx <= gright && cy >= gtop && cy <= gbottom) {
               switch (pmode) {
                  case And:
                     if (tempstate != currstate && currstate > 0) {
                        curralgo->setcell(cx, cy, 0);
                        pattchanged = true;
                        if (savecells) currlayer->undoredo->SaveCellChange(cx, cy, currstate, 0);
                     }
                     break;
                  case Copy:
                     if (tempstate != currstate) {
                        if (tempstate > maxstate) {
                           tempstate = maxstate;
                           reduced = true;
                        }
                        curralgo->setcell(cx, cy, tempstate);
                        pattchanged = true;
                        if (savecells) currlayer->undoredo->SaveCellChange(cx, cy, currstate, tempstate);
                     }
                     break;
                  case Or:
                     // Or mode is done using above nextcell loop;
                     // we only include this case to avoid compiler warning
                     break;
                  case Xor:
                     if (tempstate == currstate) {
                        if (currstate != 0) {
                           curralgo->setcell(cx, cy, 0);
                           pattchanged = true;
                           if (savecells) currlayer->undoredo->SaveCellChange(cx, cy, currstate, 0);
                        }
                     } else {
                        // tempstate != currstate
                        int newstate = tempstate ^ currstate;
                        // if xor overflows then don't change current state
                        if (newstate >= numstates) newstate = currstate;
                        if (currstate != newstate) {
                           curralgo->setcell(cx, cy, newstate);
                           pattchanged = true;
                           if (savecells) currlayer->undoredo->SaveCellChange(cx, cy, currstate, newstate);
                        }
                     }
                     break;
               }
            }
            cx++;
            cntr++;
            if ( (cntr % 4096) == 0 ) {
               abort = AbortProgress((double)cntr / maxcount, wxEmptyString);
               if (abort) break;
            }
         }
         if (abort) break;
         cy++;
      }
   }

   if (pattchanged) curralgo->endofpattern();
   EndProgress();
   
   // tidy up and display result
   statusptr->ClearMessage();
   if (pattchanged) {
      if (savecells) currlayer->undoredo->RememberCellChanges(_("Paste"), currlayer->dirty);
      MarkLayerDirty();    // calls SetWindowTitle
      mainptr->UpdatePatternAndStatus();
   }
   
   if (reduced) statusptr->ErrorMessage(_("Some cell states were reduced."));
}

// -----------------------------------------------------------------------------

bool PatternView::GetClipboardPattern(lifealgo** tempalgo,
                                      bigint* t, bigint* l, bigint* b, bigint* r)
{
   wxTextDataObject data;
   if ( !mainptr->GetTextFromClipboard(&data) ) return false;

   // copy clipboard data to temporary file so we can handle all formats
   // supported by readclipboard
   wxFile tmpfile(mainptr->clipfile, wxFile::write);
   if ( !tmpfile.IsOpened() ) {
      Warning(_("Could not create temporary file for clipboard data!"));
      return false;
   }
   if ( !tmpfile.Write(data.GetText()) ) {
      Warning(_("Could not write clipboard data to temporary file!  Maybe disk is full?"));
      tmpfile.Close();
      return false;
   }
   tmpfile.Close();

   // remember current rule
   oldrule = wxString(currlayer->algo->getrule(), wxConvLocal);

   const char* err = readclipboard(mainptr->clipfile.mb_str(wxConvLocal), **tempalgo, t, l, b, r);
   if (err) {
      // cycle thru all other algos until readclipboard succeeds
      for (int i = 0; i < NumAlgos(); i++) {
         if (i != currlayer->algtype) {
            delete *tempalgo;
            *tempalgo = CreateNewUniverse(i);
            err = readclipboard(mainptr->clipfile.mb_str(wxConvLocal), **tempalgo, t, l, b, r);
            if (!err) {
               newalgo = i;   // remember new algo for later use in PasteTemporaryToCurrent
               break;
            }
         }
      }
   }

   if (canchangerule > 0) {
      // set newrule for later use in PasteTemporaryToCurrent
      if (canchangerule == 1 && !currlayer->algo->isEmpty()) {
         // don't change rule if universe isn't empty
         newrule = oldrule;
      } else {
         // remember rule set by readclipboard
         newrule = wxString((*tempalgo)->getrule(), wxConvLocal);
      }
   }
   
   // restore rule now in case error occurred
   // (only needed because qlife and hlife share a global rule table)
   currlayer->algo->setrule( oldrule.mb_str(wxConvLocal) );

   wxRemoveFile(mainptr->clipfile);

   if (err) {
      // error probably due to bad rule string in clipboard data
      Warning(_("Could not load clipboard pattern\n(probably due to unknown rule)."));
      return false;
   }

   return true;
}

// -----------------------------------------------------------------------------

void PatternView::PasteClipboard(bool toselection)
{
   if (waitingforclick || !mainptr->ClipboardHasText()) return;
   if (toselection && !SelectionExists()) return;

   if (mainptr->generating) {
      // terminate generating loop and set command_pending flag
      mainptr->Stop();
      mainptr->command_pending = true;
      mainptr->cmdevent.SetId(toselection ? ID_PASTE_SEL : ID_PASTE);
      return;
   }

   // create a temporary universe for storing clipboard pattern;
   // GetClipboardPattern assumes it is same type as current universe
   lifealgo* tempalgo = CreateNewUniverse(currlayer->algtype);
   // no need to call setrule here -- readclipboard will do it

   // read clipboard pattern into temporary universe
   bigint top, left, bottom, right;
   if ( GetClipboardPattern(&tempalgo, &top, &left, &bottom, &right) ) {
      // tempalgo might have been deleted and re-created as a different type of universe
      PasteTemporaryToCurrent(tempalgo, toselection, top, left, bottom, right);
   }

   delete tempalgo;
}

// -----------------------------------------------------------------------------

void PatternView::CyclePasteLocation()
{
   if (plocation == TopLeft) {
      plocation = TopRight;
      if (!waitingforclick) statusptr->DisplayMessage(_("Paste location is Top Right."));
   } else if (plocation == TopRight) {
      plocation = BottomRight;
      if (!waitingforclick) statusptr->DisplayMessage(_("Paste location is Bottom Right."));
   } else if (plocation == BottomRight) {
      plocation = BottomLeft;
      if (!waitingforclick) statusptr->DisplayMessage(_("Paste location is Bottom Left."));
   } else if (plocation == BottomLeft) {
      plocation = Middle;
      if (!waitingforclick) statusptr->DisplayMessage(_("Paste location is Middle."));
   } else {
      plocation = TopLeft;
      if (!waitingforclick) statusptr->DisplayMessage(_("Paste location is Top Left."));
   }
   if (waitingforclick) {
      // force redraw of paste rectangle if mouse is inside viewport
      pasterect = wxRect(-1,-1,0,0);
   }
}

// -----------------------------------------------------------------------------

void PatternView::CyclePasteMode()
{
   if (pmode == And) {
      pmode = Copy;
      if (!waitingforclick) statusptr->DisplayMessage(_("Paste mode is Copy."));
   } else if (pmode == Copy) {
      pmode = Or;
      if (!waitingforclick) statusptr->DisplayMessage(_("Paste mode is Or."));
   } else if (pmode == Or) {
      pmode = Xor;
      if (!waitingforclick) statusptr->DisplayMessage(_("Paste mode is Xor."));
   } else {
      pmode = And;
      if (!waitingforclick) statusptr->DisplayMessage(_("Paste mode is And."));
   }
   if (waitingforclick) {
      // force redraw of paste rectangle if mouse is inside viewport
      pasterect = wxRect(-1,-1,0,0);
   }
}

// -----------------------------------------------------------------------------

void PatternView::DisplaySelectionSize()
{
   if (waitingforclick || inscript || currlayer->undoredo->doingscriptchanges)
      return;
   
   currlayer->currsel.DisplaySize();
}

// -----------------------------------------------------------------------------

void PatternView::SaveCurrentSelection()
{
   if (allowundo && !currlayer->stayclean) {
      currlayer->savesel = currlayer->currsel;
   }
}

// -----------------------------------------------------------------------------

void PatternView::RememberNewSelection(const wxString& action)
{
   if (TimelineExists()) {
      // we allow selections while a timeline exists but we can't
      // remember them in the undo/redo history
      return;
   }
   if (allowundo && !currlayer->stayclean) {
      if (inscript) SavePendingChanges();
      currlayer->undoredo->RememberSelection(action);
   }
}

// -----------------------------------------------------------------------------

void PatternView::SelectAll()
{
   SaveCurrentSelection();
   if (SelectionExists()) {
      currlayer->currsel.Deselect();
      mainptr->UpdatePatternAndStatus();
   }

   if (currlayer->algo->isEmpty()) {
      statusptr->ErrorMessage(empty_pattern);
      RememberNewSelection(_("Deselection"));
      return;
   }
   
   bigint top, left, bottom, right;
   currlayer->algo->findedges(&top, &left, &bottom, &right);
   currlayer->currsel.SetEdges(top, left, bottom, right);

   RememberNewSelection(_("Select All"));
   DisplaySelectionSize();
   mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void PatternView::RemoveSelection()
{
   if (SelectionExists()) {
      SaveCurrentSelection();
      currlayer->currsel.Deselect();
      RememberNewSelection(_("Deselection"));
      mainptr->UpdatePatternAndStatus();
   }
}

// -----------------------------------------------------------------------------

void PatternView::ShrinkSelection(bool fit)
{
   currlayer->currsel.Shrink(fit);
}

// -----------------------------------------------------------------------------

void PatternView::RandomFill()
{
   currlayer->currsel.RandomFill();
}

// -----------------------------------------------------------------------------

bool PatternView::FlipSelection(bool topbottom, bool inundoredo)
{
   return currlayer->currsel.Flip(topbottom, inundoredo);
}

// -----------------------------------------------------------------------------

bool PatternView::RotateSelection(bool clockwise, bool inundoredo)
{
   return currlayer->currsel.Rotate(clockwise, inundoredo);
}

// -----------------------------------------------------------------------------

void PatternView::SetCursorMode(wxCursor* cursor)
{
   currlayer->curs = cursor;
}

// -----------------------------------------------------------------------------

void PatternView::CycleCursorMode()
{
   if (drawingcells || selectingcells || movingview || waitingforclick)
      return;

   if (currlayer->curs == curs_pencil)
      currlayer->curs = curs_pick;
   else if (currlayer->curs == curs_pick)
      currlayer->curs = curs_cross;
   else if (currlayer->curs == curs_cross)
      currlayer->curs = curs_hand;
   else if (currlayer->curs == curs_hand)
      currlayer->curs = curs_zoomin;
   else if (currlayer->curs == curs_zoomin)
      currlayer->curs = curs_zoomout;
   else
      currlayer->curs = curs_pencil;
}

// -----------------------------------------------------------------------------

void PatternView::ZoomOut()
{
   TestAutoFit();
   currlayer->view->unzoom();
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::ZoomIn()
{
   TestAutoFit();
   if (currlayer->view->getmag() < MAX_MAG) {
      currlayer->view->zoom();
      mainptr->UpdateEverything();
   } else {
      Beep();
   }
}

// -----------------------------------------------------------------------------

void PatternView::SetPixelsPerCell(int pxlspercell)
{
   int mag = 0;
   while (pxlspercell > 1) {
      mag++;
      pxlspercell >>= 1;
   }
   if (mag == currlayer->view->getmag()) return;
   TestAutoFit();
   currlayer->view->setmag(mag);
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::FitPattern()
{
   currlayer->algo->fit(*currlayer->view, 1);
   // best not to call TestAutoFit
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::FitSelection()
{
   if (!SelectionExists()) return;

   currlayer->currsel.Fit();
   
   TestAutoFit();
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::ViewOrigin()
{
   // put 0,0 cell in middle of view
   if ( currlayer->originx == bigint::zero && currlayer->originy == bigint::zero ) {
      currlayer->view->center();
   } else {
      // put cell saved by ChangeOrigin in middle
      currlayer->view->setpositionmag(currlayer->originx, currlayer->originy,
                                      currlayer->view->getmag());
   }
   TestAutoFit();
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::ChangeOrigin()
{
   if (waitingforclick) return;
   // change cell under cursor to 0,0
   wxPoint pt = ScreenToClient( wxGetMousePosition() );
   if ( pt.x < 0 || pt.x > currlayer->view->getxmax() ||
        pt.y < 0 || pt.y > currlayer->view->getymax() ) {
      statusptr->ErrorMessage(_("Origin not changed."));
   } else {
      pair<bigint, bigint> cellpos = currlayer->view->at(pt.x, pt.y);
      currlayer->originx = cellpos.first;
      currlayer->originy = cellpos.second;
      statusptr->DisplayMessage(_("Origin changed."));
      if ( GridVisible() )
         mainptr->UpdatePatternAndStatus();
      else
         statusptr->UpdateXYLocation();
   }
}

// -----------------------------------------------------------------------------

void PatternView::RestoreOrigin()
{
   if (waitingforclick) return;
   if (currlayer->originx != bigint::zero || currlayer->originy != bigint::zero) {
      currlayer->originx = 0;
      currlayer->originy = 0;
      statusptr->DisplayMessage(origin_restored);
      if ( GridVisible() )
         mainptr->UpdatePatternAndStatus();
      else
         statusptr->UpdateXYLocation();
   }
}

// -----------------------------------------------------------------------------

bool PatternView::GridVisible()
{
   return ( showgridlines && currlayer->view->getmag() >= mingridmag );
}

// -----------------------------------------------------------------------------

void PatternView::ToggleGridLines()
{
   showgridlines = !showgridlines;
   if ( (currlayer->view->getmag() >= mingridmag) ||
        // also update everything if drawing all layers
        (numlayers > 1 && (stacklayers || tilelayers))
      )
      mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::ToggleCellIcons()
{
   showicons = !showicons;
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::ToggleCellColors()
{
   swapcolors = !swapcolors;
   InvertCellColors();
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::ToggleBuffering()
{
   buffered = !buffered;
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

bool PatternView::GetCellPos(bigint& xpos, bigint& ypos)
{
   wxPoint pt = ScreenToClient( wxGetMousePosition() );
   if (PointInView(pt.x, pt.y)) {
      // get mouse location in cell coords
      pair<bigint, bigint> cellpos = currlayer->view->at(pt.x, pt.y);
      xpos = cellpos.first;
      ypos = cellpos.second;
      
      // check if xpos,ypos is outside bounded grid
      if (!CellInGrid(xpos, ypos)) return false;
      
      return true;
   } else {
      // mouse not in viewport
      return false;
   }
}

// -----------------------------------------------------------------------------

bool PatternView::PointInView(int x, int y)
{
   return ( x >= 0 && x <= currlayer->view->getxmax() &&
            y >= 0 && y <= currlayer->view->getymax() );
}

// -----------------------------------------------------------------------------

#ifdef __WXGTK__
   // nicer to redraw entire viewport on Linux
   // otherwise we see partial drawing in some cases
   #define RefreshControls() Refresh(false)
#else
   #define RefreshControls() RefreshRect(controlsrect,false)
#endif

void PatternView::CheckCursor(bool active)
{
   if (active) {
      // make sure cursor is up to date
      wxPoint pt = ScreenToClient( wxGetMousePosition() );
      if (PointInView(pt.x, pt.y)) {         
         if (numlayers > 1 && tilelayers && tileindex != currindex) {
            // show arrow cursor if over tile border (ie. bigview) or non-current tile
            #ifdef __WXMAC__
               // wxMac bug??? need this to fix probs after toggling status/tool bar
               wxSetCursor(*wxSTANDARD_CURSOR);
            #endif
            SetCursor(*wxSTANDARD_CURSOR);
            if (showcontrols) {
               showcontrols = false;
               RefreshControls();
            }
         
         } else if ( (controlsrect.Contains(pt) || clickedcontrol > NO_CONTROL) &&
                     !(drawingcells || selectingcells || movingview || waitingforclick) ) {
            // cursor is over translucent controls, or user clicked in a control
            // and hasn't released mouse button yet
            #ifdef __WXMAC__
               wxSetCursor(*wxSTANDARD_CURSOR);
            #endif
            SetCursor(*wxSTANDARD_CURSOR);
            if (!showcontrols) {
               showcontrols = true;
               RefreshControls();
            }
         
         } else {
            // show current cursor mode
            #ifdef __WXMAC__
               wxSetCursor(*currlayer->curs);
            #endif
            SetCursor(*currlayer->curs);
            if (showcontrols) {
               showcontrols = false;
               RefreshControls();
            }
         }
      
      } else {
         // cursor is not in viewport
         #ifdef __WXMAC__
            wxSetCursor(*wxSTANDARD_CURSOR);
         #endif
         SetCursor(*wxSTANDARD_CURSOR);
         if (showcontrols) {
            showcontrols = false;
            RefreshControls();
         }
      }
   
   } else {
      // main window is not active so don't change cursor
   }
}

// -----------------------------------------------------------------------------

int PatternView::GetMag()
{
   return currlayer->view->getmag();
}

// -----------------------------------------------------------------------------

void PatternView::SetMag(int mag)
{
   TestAutoFit();
   if (mag > MAX_MAG) mag = MAX_MAG;
   currlayer->view->setmag(mag);
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::SetPosMag(const bigint& x, const bigint& y, int mag)
{
   currlayer->view->setpositionmag(x, y, mag);
}

// -----------------------------------------------------------------------------

void PatternView::GetPos(bigint& x, bigint& y)
{
   x = currlayer->view->x;
   y = currlayer->view->y;
}

// -----------------------------------------------------------------------------

void PatternView::FitInView(int force)
{
   currlayer->algo->fit(*currlayer->view, force);
}

// -----------------------------------------------------------------------------

int PatternView::CellVisible(const bigint& x, const bigint& y)
{
   return currlayer->view->contains(x, y);
}

// -----------------------------------------------------------------------------

// scrolling functions:

void PatternView::PanUp(int amount)
{
   TestAutoFit();
   currlayer->view->move(0, -amount);
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::PanDown(int amount)
{
   TestAutoFit();
   currlayer->view->move(0, amount);
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::PanLeft(int amount)
{
   TestAutoFit();
   currlayer->view->move(-amount, 0);
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::PanRight(int amount)
{
   TestAutoFit();
   currlayer->view->move(amount, 0);
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::PanNE()
{
   TestAutoFit();
   int xamount = SmallScroll(currlayer->view->getwidth());
   int yamount = SmallScroll(currlayer->view->getheight());
   int amount = (xamount < yamount) ? xamount : yamount;
   currlayer->view->move(amount, -amount);
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::PanNW()
{
   TestAutoFit();
   int xamount = SmallScroll(currlayer->view->getwidth());
   int yamount = SmallScroll(currlayer->view->getheight());
   int amount = (xamount < yamount) ? xamount : yamount;
   currlayer->view->move(-amount, -amount);
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::PanSE()
{
   TestAutoFit();
   int xamount = SmallScroll(currlayer->view->getwidth());
   int yamount = SmallScroll(currlayer->view->getheight());
   int amount = (xamount < yamount) ? xamount : yamount;
   currlayer->view->move(amount, amount);
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::PanSW()
{
   TestAutoFit();
   int xamount = SmallScroll(currlayer->view->getwidth());
   int yamount = SmallScroll(currlayer->view->getheight());
   int amount = (xamount < yamount) ? xamount : yamount;
   currlayer->view->move(-amount, amount);
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

int PatternView::SmallScroll(int xysize)
{
   int amount;
   int mag = currlayer->view->getmag();
   if (mag > 0) {
      // scroll an integral number of cells (1 cell = 2^mag pixels)
      if (mag < 3) {
         amount = ((xysize >> mag) / 20) << mag;
         if (amount == 0) amount = 1 << mag;
         return amount;
      } else {
         // grid lines are visible so scroll by only 1 cell
         return 1 << mag;
      }
   } else {
      // scroll by approx 5% of current wd/ht
      amount = xysize / 20;
      if (amount == 0) amount = 1;
      return amount;
   }
}

// -----------------------------------------------------------------------------

int PatternView::BigScroll(int xysize)
{
   int amount;
   int mag = currlayer->view->getmag();
   if (mag > 0) {
      // scroll an integral number of cells (1 cell = 2^mag pixels)
      amount = ((xysize >> mag) * 9 / 10) << mag;
      if (amount == 0) amount = 1 << mag;
      return amount;
   } else {
      // scroll by approx 90% of current wd/ht
      amount = xysize * 9 / 10;
      if (amount == 0) amount = 1;
      return amount;
   }
}

// -----------------------------------------------------------------------------

void PatternView::UpdateScrollBars()
{
   if (mainptr->fullscreen) return;

   int viewwd, viewht;
   int mag = currlayer->view->getmag();
   if (mag > 0) {
      // scroll by cells, so determine number of cells visible in viewport
      viewwd = currlayer->view->getwidth() >> mag;
      viewht = currlayer->view->getheight() >> mag;
   } else {
      // scroll by pixels, so get pixel dimensions of viewport
      viewwd = currlayer->view->getwidth();
      viewht = currlayer->view->getheight();
   }
   if (viewwd < 1) viewwd = 1;
   if (viewht < 1) viewht = 1;

   if (currlayer->algo->gridwd > 0) {
      // restrict scrolling to left/right edges of grid if its width is finite
      int range = currlayer->algo->gridwd;
      // avoid scroll bar disappearing
      if (range < 3) range = 3;
      hthumb = currlayer->view->x.toint() + range / 2;
      bigview->SetScrollbar(wxHORIZONTAL, hthumb, 1, range, true);
   } else {
      // keep thumb box in middle of scroll bar if grid width is infinite
      hthumb = (thumbrange - 1) * viewwd / 2;
      bigview->SetScrollbar(wxHORIZONTAL, hthumb, viewwd, thumbrange * viewwd, true);
   }

   if (currlayer->algo->gridht > 0) {
      // restrict scrolling to top/bottom edges of grid if its height is finite
      int range = currlayer->algo->gridht;
      // avoid scroll bar disappearing
      if (range < 3) range = 3;
      vthumb = currlayer->view->y.toint() + range / 2;
      bigview->SetScrollbar(wxVERTICAL, vthumb, 1, range, true);
   } else {
      // keep thumb box in middle of scroll bar if grid height is infinite
      vthumb = (thumbrange - 1) * viewht / 2;
      bigview->SetScrollbar(wxVERTICAL, vthumb, viewht, thumbrange * viewht, true);
   }
}

// -----------------------------------------------------------------------------

void PatternView::ProcessKey(int key, int modifiers)
{
   mainptr->showbanner = false;

   // WARNING: ProcessKey can be called while running a script, or reading
   // a large pattern file, or waiting for a paste click etc, so we must avoid
   // doing any actions that could cause havoc at such times.
   bool busy = nopattupdate || waitingforclick || dragtimer->IsRunning();
   bool timeline = TimelineExists();

   action_info action = FindAction(key, modifiers);
   switch (action.id) {
      case DO_NOTHING:     // any unassigned key turns off full screen mode
                           if (mainptr->fullscreen) mainptr->ToggleFullScreen();
                           break;

      case DO_OPENFILE:    if (IsHTMLFile(action.file)) {
                              // show HTML file in help window
                              if (!busy) ShowHelp(action.file);
                           } else {
                              // load pattern or run script
                              if (!inscript && !busy) mainptr->OpenFile(action.file, true);
                           }
                           break;

      // File menu actions
      case DO_NEWPATT:     if (!inscript && !busy) mainptr->NewPattern(); break;
      case DO_OPENPATT:    if (!inscript && !busy) mainptr->OpenPattern(); break;
      case DO_OPENCLIP:    if (!inscript && !busy) mainptr->OpenClipboard(); break;
      case DO_SAVE:        if (!inscript && !busy) mainptr->SavePattern(); break;
      case DO_SAVEXRLE:    if (!inscript) savexrle = !savexrle; break;
      case DO_RUNSCRIPT:   if (!inscript && !timeline && !busy) mainptr->OpenScript(); break;
      case DO_RUNCLIP:     if (!inscript && !timeline && !busy) mainptr->RunClipboard(); break;
      case DO_PREFS:       if (!busy) mainptr->ShowPrefsDialog(); break;
      case DO_PATTDIR:     if (!busy) mainptr->ChangePatternDir(); break;
      case DO_SCRIPTDIR:   if (!busy) mainptr->ChangeScriptDir(); break;
      case DO_PATTERNS:    mainptr->ToggleShowPatterns(); break;
      case DO_SCRIPTS:     mainptr->ToggleShowScripts(); break;
      case DO_QUIT:        mainptr->QuitApp(); break;

      // Edit menu actions
      case DO_UNDO:        if (!inscript && !timeline && !busy) currlayer->undoredo->UndoChange(); break;
      case DO_REDO:        if (!inscript && !timeline && !busy) currlayer->undoredo->RedoChange(); break;
      case DO_DISABLE:     if (!inscript) mainptr->ToggleAllowUndo(); break;
      case DO_CUT:         if (!inscript && !timeline) CutSelection(); break;
      case DO_COPY:        if (!inscript) CopySelection(); break;
      case DO_CLEAR:       if (!inscript && !timeline) ClearSelection(); break;
      case DO_CLEAROUT:    if (!inscript && !timeline) ClearOutsideSelection(); break;
      case DO_PASTE:       if (!inscript && !timeline && !busy) PasteClipboard(false); break;
      case DO_PASTESEL:    if (!inscript && !timeline && !busy) PasteClipboard(true); break;
      case DO_SELALL:      if (!inscript) SelectAll(); break;
      case DO_REMOVESEL:   if (!inscript) RemoveSelection(); break;
      case DO_SHRINK:      if (!inscript) ShrinkSelection(false); break;
      case DO_SHRINKFIT:   if (!inscript) ShrinkSelection(true); break;
      case DO_RANDFILL:    if (!inscript && !timeline) RandomFill(); break;
      case DO_FLIPTB:      if (!inscript && !timeline) FlipSelection(true); break;
      case DO_FLIPLR:      if (!inscript && !timeline) FlipSelection(false); break;
      case DO_ROTATECW:    if (!inscript && !timeline) RotateSelection(true); break;
      case DO_ROTATEACW:   if (!inscript && !timeline) RotateSelection(false); break;
      case DO_ADVANCE:     if (!inscript && !timeline) currlayer->currsel.Advance(); break;
      case DO_ADVANCEOUT:  if (!inscript && !timeline) currlayer->currsel.AdvanceOutside(); break;
      case DO_CURSDRAW:    SetCursorMode(curs_pencil); break;
      case DO_CURSPICK:    SetCursorMode(curs_pick); break;
      case DO_CURSSEL:     SetCursorMode(curs_cross); break;
      case DO_CURSMOVE:    SetCursorMode(curs_hand); break;
      case DO_CURSIN:      SetCursorMode(curs_zoomin); break;
      case DO_CURSOUT:     SetCursorMode(curs_zoomout); break;
      case DO_CURSCYCLE:   CycleCursorMode(); break;
      case DO_PASTEMODE:   CyclePasteMode(); break;
      case DO_PASTELOC:    CyclePasteLocation(); break;
      case DO_NEXTHIGHER:  CycleDrawingState(true); break;
      case DO_NEXTLOWER:   CycleDrawingState(false); break;

      // Control menu actions
      case DO_STARTSTOP:   if (!inscript) {
                              if (timeline) {
                                 if (currlayer->algo->isrecording()) {
                                    StartStopRecording();   // stop recording
                                 } else {
                                    PlayTimeline(1);        // play forwards or stop if already playing
                                 }
                              } else if (mainptr->generating) {
                                 mainptr->Stop();
                              } else {
                                 mainptr->GeneratePattern();
                              }
                           }
                           break;
      case DO_NEXTGEN:     if (!inscript && !timeline) mainptr->NextGeneration(false); break;
      case DO_NEXTSTEP:    if (!inscript && !timeline) mainptr->NextGeneration(true); break;
      case DO_RESET:       if (!inscript && !timeline && !busy) mainptr->ResetPattern(); break;
      case DO_SETGEN:      if (!inscript && !timeline && !busy) mainptr->SetGeneration(); break;
      case DO_SETBASE:     if (!inscript && !timeline && !busy) mainptr->SetBaseStep(); break;
      case DO_FASTER:      mainptr->GoFaster(); break;
      case DO_SLOWER:      mainptr->GoSlower(); break;
      case DO_AUTOFIT:     mainptr->ToggleAutoFit(); break;
      case DO_HYPER:       if (!timeline) mainptr->ToggleHyperspeed(); break;
      case DO_HASHINFO:    mainptr->ToggleHashInfo(); break;
      case DO_RECORD:      StartStopRecording(); break;
      case DO_DELTIME:     DeleteTimeline(); break;
      case DO_PLAYBACK:    if (!inscript && timeline) PlayTimeline(-1); break;
      case DO_SETRULE:     if (!inscript && !timeline && !busy) mainptr->ShowRuleDialog(); break;
      case DO_TIMING:      if (!inscript && !timeline) mainptr->DisplayTimingInfo(); break;
      case DO_HASHING:     if (!inscript && !timeline && !busy) {
                              if (currlayer->algtype != HLIFE_ALGO)
                                 mainptr->ChangeAlgorithm(HLIFE_ALGO);
                              else
                                 mainptr->ChangeAlgorithm(QLIFE_ALGO);
                           }
                           break;

      // View menu actions
      case DO_LEFT:        PanLeft( SmallScroll(currlayer->view->getwidth()) ); break;
      case DO_RIGHT:       PanRight( SmallScroll(currlayer->view->getwidth()) ); break;
      case DO_UP:          PanUp( SmallScroll(currlayer->view->getheight()) ); break;
      case DO_DOWN:        PanDown( SmallScroll(currlayer->view->getheight()) ); break;
      case DO_NE:          PanNE(); break;
      case DO_NW:          PanNW(); break;
      case DO_SE:          PanSE(); break;
      case DO_SW:          PanSW(); break;
      case DO_FULLSCREEN:  mainptr->ToggleFullScreen(); break;
      case DO_FIT:         FitPattern(); break;
      case DO_FITSEL:      FitSelection(); break;
      case DO_MIDDLE:      ViewOrigin(); break;
      case DO_CHANGE00:    ChangeOrigin(); break;
      case DO_RESTORE00:   RestoreOrigin(); break;
      case DO_ZOOMIN:      ZoomIn(); break;
      case DO_ZOOMOUT:     ZoomOut(); break;
      case DO_SCALE1:      SetPixelsPerCell(1); break;
      case DO_SCALE2:      SetPixelsPerCell(2); break;
      case DO_SCALE4:      SetPixelsPerCell(4); break;
      case DO_SCALE8:      SetPixelsPerCell(8); break;
      case DO_SCALE16:     SetPixelsPerCell(16); break;
      case DO_SHOWTOOL:    mainptr->ToggleToolBar(); break;
      case DO_SHOWLAYER:   ToggleLayerBar(); break;
      case DO_SHOWEDIT:    ToggleEditBar(); break;
      case DO_SHOWSTATES:  ToggleAllStates(); break;
      case DO_SHOWSTATUS:  mainptr->ToggleStatusBar(); break;
      case DO_SHOWEXACT:   mainptr->ToggleExactNumbers(); break;
      case DO_SHOWICONS:   ToggleCellIcons(); break;
      case DO_INVERT:      ToggleCellColors(); break;
      case DO_SHOWGRID:    ToggleGridLines(); break;
      case DO_BUFFERED:    ToggleBuffering(); break;
      case DO_SHOWTIME:    ToggleTimelineBar(); break;
      case DO_INFO:        if (!busy) mainptr->ShowPatternInfo(); break;

      // Layer menu actions
      case DO_ADD:         if (!inscript) AddLayer(); break;
      case DO_CLONE:       if (!inscript) CloneLayer(); break;
      case DO_DUPLICATE:   if (!inscript) DuplicateLayer(); break;
      case DO_DELETE:      if (!inscript) DeleteLayer(); break;
      case DO_DELOTHERS:   if (!inscript) DeleteOtherLayers(); break;
      case DO_MOVELAYER:   if (!inscript && !busy) MoveLayerDialog(); break;
      case DO_NAMELAYER:   if (!inscript && !busy) NameLayerDialog(); break;
      case DO_SETCOLORS:   if (!inscript && !busy) SetLayerColors(); break;
      case DO_SYNCVIEWS:   if (!inscript) ToggleSyncViews(); break;
      case DO_SYNCCURS:    if (!inscript) ToggleSyncCursors(); break;
      case DO_STACK:       if (!inscript) ToggleStackLayers(); break;
      case DO_TILE:        if (!inscript) ToggleTileLayers(); break;

      // Help menu actions
      case DO_HELP:        if (!busy) {
                              // if help window is open then bring it to the front,
                              // otherwise open it and display most recent help file
                              ShowHelp(wxEmptyString);
                           }
                           break;
      case DO_ABOUT:       if (!inscript && !busy) ShowAboutBox(); break;
      
      default:             Warning(_("Bug detected in ProcessKey!"));
   }
}

// -----------------------------------------------------------------------------

void PatternView::ShowDrawing()
{
   currlayer->algo->endofpattern();

   // update status bar
   if (showstatus) statusptr->Refresh(false);

   if (slowdraw) {
      // we have to draw by updating entire view
      slowdraw = false;
      UpdateView();
   }

   MarkLayerDirty();
}

// -----------------------------------------------------------------------------

void PatternView::DrawOneCell(wxDC& dc, int cx, int cy, int oldstate, int newstate)
{
   // remember this cell change for later undo/redo
   if (allowundo) currlayer->undoredo->SaveCellChange(cx, cy, oldstate, newstate);

   if (numlayers > 1 && (stacklayers || (numclones > 0 && tilelayers))) {
      // drawing must be done via UpdateView in ShowDrawing
      slowdraw = true;
      return;
   }

   int cellsize = 1 << currlayer->view->getmag();

   // convert given cell coords to view coords
   pair<bigint, bigint> lefttop = currlayer->view->at(0, 0);
   wxCoord x = (cx - lefttop.first.toint()) * cellsize;
   wxCoord y = (cy - lefttop.second.toint()) * cellsize;
   
   if (cellsize > 2) cellsize--;    // allow for gap between cells
   
   wxBitmap** iconmaps = NULL;
   if (currlayer->view->getmag() == 3) {
      iconmaps = currlayer->icons7x7;
   } else if (currlayer->view->getmag() == 4) {
      iconmaps = currlayer->icons15x15;
   }

   if (showicons && drawstate > 0 && currlayer->view->getmag() > 2 &&
       iconmaps && iconmaps[drawstate]) {
      DrawOneIcon(dc, x, y, iconmaps[drawstate],
                  currlayer->cellr[0],
                  currlayer->cellg[0],
                  currlayer->cellb[0],
                  currlayer->cellr[drawstate],
                  currlayer->cellg[drawstate],
                  currlayer->cellb[drawstate]);
   } else {
      dc.DrawRectangle(x, y, cellsize, cellsize);
   }
   
   // overlay selection image if cell is within selection
   if (SelectionExists() && currlayer->currsel.ContainsCell(cx, cy)) {
      wxRect r = wxRect(x, y, cellsize, cellsize);
      DrawSelection(dc, r);
   }
}

// -----------------------------------------------------------------------------

void PatternView::StartDrawingCells(int x, int y)
{
   pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
   // check that cellpos is within getcell/setcell limits
   if ( OutsideLimits(cellpos.second, cellpos.first, cellpos.second, cellpos.first) ) {
      statusptr->ErrorMessage(_("Drawing is not allowed outside +/- 10^9 boundary."));
      return;
   }
   
   // ShowDrawing will call MarkLayerDirty so we need to save dirty state now
   // for later use by RememberCellChanges
   if (allowundo) currlayer->savedirty = currlayer->dirty;

   cellx = cellpos.first.toint();
   celly = cellpos.second.toint();
   int currstate = currlayer->algo->getcell(cellx, celly);
   
   // reset drawing state in case it's no longer valid (due to algo/rule change)
   if (currlayer->drawingstate >= currlayer->algo->NumCellStates()) {
      currlayer->drawingstate = 1;
   }

   if (currstate == currlayer->drawingstate) {
      drawstate = 0;
   } else {
      drawstate = currlayer->drawingstate;
   }
   if (currstate != drawstate) {
      currlayer->algo->setcell(cellx, celly, drawstate);
   
      wxClientDC dc(this);
      dc.SetPen(*wxTRANSPARENT_PEN);

      cellbrush->SetColour(currlayer->cellr[drawstate],
                           currlayer->cellg[drawstate],
                           currlayer->cellb[drawstate]);
      dc.SetBrush(*cellbrush);

      DrawOneCell(dc, cellx, celly, currstate, drawstate);
      dc.SetBrush(wxNullBrush);
      dc.SetPen(wxNullPen);
      
      ShowDrawing();
   }
   
   drawingcells = true;
   CaptureMouse();                     // get mouse up event even if outside view
   dragtimer->Start(DRAG_RATE);        // see OnDragTimer
   
   if (stopdrawing) {
      // mouse up event has already been seen so terminate drawing immediately
      stopdrawing = false;
      StopDraggingMouse();
   }
}

// -----------------------------------------------------------------------------

void PatternView::DrawCells(int x, int y)
{
   // make sure x,y is within viewport
   if (x < 0) x = 0;
   if (y < 0) y = 0;
   if (x > currlayer->view->getxmax()) x = currlayer->view->getxmax();
   if (y > currlayer->view->getymax()) y = currlayer->view->getymax();
   
   // make sure x,y is within bounded grid
   pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
   if (currlayer->algo->gridwd > 0) {
      if (cellpos.first < currlayer->algo->gridleft) cellpos.first = currlayer->algo->gridleft;
      if (cellpos.first > currlayer->algo->gridright) cellpos.first = currlayer->algo->gridright;
   }
   if (currlayer->algo->gridht > 0) {
      if (cellpos.second < currlayer->algo->gridtop) cellpos.second = currlayer->algo->gridtop;
      if (cellpos.second > currlayer->algo->gridbottom) cellpos.second = currlayer->algo->gridbottom;
   }
   
   if ( currlayer->view->getmag() < 0 ||
        OutsideLimits(cellpos.second, cellpos.first, cellpos.second, cellpos.first) ) {
      return;
   }

   int newx = cellpos.first.toint();
   int newy = cellpos.second.toint();
   if ( newx != cellx || newy != celly ) {
      int currstate;
      wxClientDC dc(this);
      dc.SetPen(*wxTRANSPARENT_PEN);

      cellbrush->SetColour(currlayer->cellr[drawstate],
                           currlayer->cellg[drawstate],
                           currlayer->cellb[drawstate]);
      dc.SetBrush(*cellbrush);

      int numchanged = 0;
      
      // draw a line of cells using Bresenham's algorithm;
      // this code comes from Guillermo Garcia's Life demo supplied with wx
      int d, ii, jj, di, ai, si, dj, aj, sj;
      di = newx - cellx;
      ai = abs(di) << 1;
      si = (di < 0)? -1 : 1;
      dj = newy - celly;
      aj = abs(dj) << 1;
      sj = (dj < 0)? -1 : 1;
      
      ii = cellx;
      jj = celly;
      
      lifealgo* curralgo = currlayer->algo;
      if (ai > aj) {
         d = aj - (ai >> 1);
         while (ii != newx) {
            currstate = curralgo->getcell(ii, jj);
            if (currstate != drawstate) {
               curralgo->setcell(ii, jj, drawstate);
               DrawOneCell(dc, ii, jj, currstate, drawstate);
               numchanged++;
            }
            if (d >= 0) {
               jj += sj;
               d  -= ai;
            }
            ii += si;
            d  += aj;
         }
      } else {
         d = ai - (aj >> 1);
         while (jj != newy) {
            currstate = curralgo->getcell(ii, jj);
            if (currstate != drawstate) {
               curralgo->setcell(ii, jj, drawstate);
               DrawOneCell(dc, ii, jj, currstate, drawstate);
               numchanged++;
            }
            if (d >= 0) {
               ii += si;
               d  -= aj;
            }
            jj += sj;
            d  += ai;
         }
      }
      
      cellx = newx;
      celly = newy;
      
      currstate = curralgo->getcell(cellx, celly);
      if (currstate != drawstate) {
         curralgo->setcell(cellx, celly, drawstate);
         DrawOneCell(dc, cellx, celly, currstate, drawstate);
         numchanged++;
      }
      
      dc.SetBrush(wxNullBrush);     // restore brush
      dc.SetPen(wxNullPen);         // restore pen
      
      if (numchanged > 0) ShowDrawing();
   }
}

// -----------------------------------------------------------------------------

void PatternView::PickCell(int x, int y)
{
   pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
   if ( currlayer->view->getmag() < 0 ||
        OutsideLimits(cellpos.second, cellpos.first, cellpos.second, cellpos.first) ) {
      return;
   }

   int cellx = cellpos.first.toint();
   int celly = cellpos.second.toint();
   currlayer->drawingstate = currlayer->algo->getcell(cellx, celly);
   UpdateEditBar(true);
}

// -----------------------------------------------------------------------------

void PatternView::StartSelectingCells(int x, int y, bool shiftdown)
{
   // make sure anchor cell is within bounded grid (x,y can be outside grid)
   pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
   if (currlayer->algo->gridwd > 0) {
      if (cellpos.first < currlayer->algo->gridleft) cellpos.first = currlayer->algo->gridleft;
      if (cellpos.first > currlayer->algo->gridright) cellpos.first = currlayer->algo->gridright;
   }
   if (currlayer->algo->gridht > 0) {
      if (cellpos.second < currlayer->algo->gridtop) cellpos.second = currlayer->algo->gridtop;
      if (cellpos.second > currlayer->algo->gridbottom) cellpos.second = currlayer->algo->gridbottom;
   }
   anchorx = cellpos.first;
   anchory = cellpos.second;

   // save original selection so it can be restored if user hits escape;
   // also used by RememberNewSelection
   currlayer->savesel = currlayer->currsel;

   // reset previous selection
   prevsel.Deselect();
   
   // for avoiding 1x1 selection if mouse doesn't move much
   initselx = x;
   initsely = y;
   
   // allow changing size in any direction
   forceh = false;
   forcev = false;
   
   if (SelectionExists()) {
      if (shiftdown) {
         // modify current selection
         currlayer->currsel.Modify(cellpos.first, cellpos.second,
                                   anchorx, anchory, &forceh, &forcev);
         DisplaySelectionSize();
      } else {
         // remove current selection
         currlayer->currsel.Deselect();
      }
      // allow mouse interaction if script is running
      bool saveinscript = inscript;
      inscript = false;
      mainptr->UpdatePatternAndStatus();
      inscript = saveinscript;
   }
   
   selectingcells = true;
   CaptureMouse();                  // get mouse up event even if outside view
   dragtimer->Start(DRAG_RATE);     // see OnDragTimer
}

// -----------------------------------------------------------------------------

void PatternView::SelectCells(int x, int y)
{
   // only select cells within view
   if (x < 0) x = 0;
   if (y < 0) y = 0;
   if (x > currlayer->view->getxmax()) x = currlayer->view->getxmax();
   if (y > currlayer->view->getymax()) y = currlayer->view->getymax();

   if ( abs(initselx - x) < 2 && abs(initsely - y) < 2 && !SelectionExists() ) {
      // avoid 1x1 selection if mouse hasn't moved much
      return;
   }

   // make sure x,y is within bounded grid
   pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
   if (currlayer->algo->gridwd > 0) {
      if (cellpos.first < currlayer->algo->gridleft) cellpos.first = currlayer->algo->gridleft;
      if (cellpos.first > currlayer->algo->gridright) cellpos.first = currlayer->algo->gridright;
   }
   if (currlayer->algo->gridht > 0) {
      if (cellpos.second < currlayer->algo->gridtop) cellpos.second = currlayer->algo->gridtop;
      if (cellpos.second > currlayer->algo->gridbottom) cellpos.second = currlayer->algo->gridbottom;
   }

   if (!forcev) currlayer->currsel.SetLeftRight(cellpos.first, anchorx);
   if (!forceh) currlayer->currsel.SetTopBottom(cellpos.second, anchory);

   if (currlayer->currsel != prevsel) {
      // selection has changed
      DisplaySelectionSize();
      prevsel = currlayer->currsel;
      
      // allow mouse interaction if script is running
      bool saveinscript = inscript;
      inscript = false;
      mainptr->UpdatePatternAndStatus();
      inscript = saveinscript;
   }
}

// -----------------------------------------------------------------------------

void PatternView::StartMovingView(int x, int y)
{
   pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
   bigcellx = cellpos.first;
   bigcelly = cellpos.second;
   movingview = true;
   CaptureMouse();                  // get mouse up event even if outside view
   dragtimer->Start(DRAG_RATE);     // see OnDragTimer
}

// -----------------------------------------------------------------------------

void PatternView::MoveView(int x, int y)
{
   pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
   bigint newx = cellpos.first;
   bigint newy = cellpos.second;
   bigint xdelta = bigcellx;
   bigint ydelta = bigcelly;
   xdelta -= newx;
   ydelta -= newy;

   int xamount, yamount;
   int mag = currlayer->view->getmag();
   if (mag >= 0) {
      // move an integral number of cells
      xamount = xdelta.toint() << mag;
      yamount = ydelta.toint() << mag;
   } else {
      // convert cell deltas to screen pixels
      xdelta >>= -mag;
      ydelta >>= -mag;
      xamount = xdelta.toint();
      yamount = ydelta.toint();
   }

   if ( xamount != 0 || yamount != 0 ) {
      currlayer->view->move(xamount, yamount);
      
      // allow mouse interaction if script is running
      bool saveinscript = inscript;
      inscript = false;
      mainptr->UpdatePatternAndStatus();
      inscript = saveinscript;
      
      cellpos = currlayer->view->at(x, y);
      bigcellx = cellpos.first;
      bigcelly = cellpos.second;
   }
   
   // need to update scroll bars if grid is bounded
   if (currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0) {
      UpdateScrollBars();
   }
}

// -----------------------------------------------------------------------------

void PatternView::StopDraggingMouse()
{
   if ( HasCapture() ) ReleaseMouse();
   if ( dragtimer->IsRunning() ) dragtimer->Stop();

   if (selectingcells) {
      if (allowundo) RememberNewSelection(_("Selection"));
      selectingcells = false;                // tested by CanUndo
      mainptr->UpdateMenuItems(true);        // enable various Edit menu items
      if (allowundo) UpdateEditBar(true);    // update Undo/Redo buttons
   }
   
   if (drawingcells && allowundo) {
      // MarkLayerDirty (in ShowDrawing) has set dirty flag, so we need to
      // pass in the flag state saved before drawing started
      currlayer->undoredo->RememberCellChanges(_("Drawing"), currlayer->savedirty);
      drawingcells = false;                  // tested by CanUndo
      mainptr->UpdateMenuItems(true);        // enable Undo item
      UpdateEditBar(true);                   // update Undo/Redo buttons
   }
   
   if (clickedcontrol > NO_CONTROL) {
      if (currcontrol == clickedcontrol && !PANNING_CONTROL) {
         // only do non-panning function when button is released
         ProcessClickedControl();
      }
      clickedcontrol = NO_CONTROL;
      currcontrol = NO_CONTROL;
      RefreshRect(controlsrect, false);
      Update();
   }
   
   drawingcells = false;
   selectingcells = false;
   movingview = false;

   CheckCursor(true);
}

// -----------------------------------------------------------------------------

void PatternView::RestoreSelection()
{
   currlayer->currsel = currlayer->savesel;
   StopDraggingMouse();
   
   // allow mouse interaction if script is running
   bool saveinscript = inscript;
   inscript = false;
   mainptr->UpdatePatternAndStatus();
   inscript = saveinscript;
   
   statusptr->DisplayMessage(_("New selection aborted."));
}

// -----------------------------------------------------------------------------

void PatternView::TestAutoFit()
{
   if (currlayer->autofit && mainptr->generating) {
      // assume user no longer wants us to do autofitting
      currlayer->autofit = false;
   }
}

// -----------------------------------------------------------------------------

void PatternView::ZoomInPos(int x, int y)
{
   // zoom in so that clicked cell stays under cursor
   TestAutoFit();
   if (currlayer->view->getmag() < MAX_MAG) {
      currlayer->view->zoom(x, y);
      // allow mouse interaction if script is running
      bool saveinscript = inscript;
      inscript = false;
      mainptr->UpdateEverything();
      inscript = saveinscript;
   } else {
      Beep();   // can't zoom in any further
   }
}

// -----------------------------------------------------------------------------

void PatternView::ZoomOutPos(int x, int y)
{
   // zoom out so that clicked cell stays under cursor
   TestAutoFit();
   currlayer->view->unzoom(x, y);
   // allow mouse interaction if script is running
   bool saveinscript = inscript;
   inscript = false;
   mainptr->UpdateEverything();
   inscript = saveinscript;
}

// -----------------------------------------------------------------------------

void PatternView::SetViewSize(int wd, int ht)
{
   // wd or ht might be < 1 on Windows
   if (wd < 1) wd = 1;
   if (ht < 1) ht = 1;

   if (tileindex < 0) {
      // use main viewport window's size to reset viewport in each layer
      ResizeLayers(wd, ht);
   }
   
   // only autofit when generating
   if (currlayer->autofit && mainptr && mainptr->generating)
      currlayer->algo->fit(*currlayer->view, 0);
   
   // update position of translucent controls
   switch (controlspos) {
      case 1:
         // top left corner
         controlsrect = wxRect(0, 0, controlswd, controlsht);
         break;
      case 2:
         // top right corner
         controlsrect = wxRect(wd - controlswd, 0, controlswd, controlsht);
         break;
      case 3:
         // bottom right corner
         controlsrect = wxRect(wd - controlswd, ht - controlsht, controlswd, controlsht);
         break;
      case 4:
         // bottom left corner
         controlsrect = wxRect(0, ht - controlsht, controlswd, controlsht);
         break;
      default:
         // controlspos should be 0 (controls are disabled)
         controlsrect = wxRect(0, 0, 0, 0);
   }
}

// -----------------------------------------------------------------------------

void PatternView::OnPaint(wxPaintEvent& WXUNUSED(event))
{
   /* avoid unwanted drawing in certain situations???
   if (ignorepaint) {
      ignorepaint = false;
      wxPaintDC dc(this);
      return;
   }
   */

   int wd, ht;
   GetClientSize(&wd, &ht);
   // wd or ht might be < 1 on Windows
   if (wd < 1) wd = 1;
   if (ht < 1) ht = 1;

   if ( numclones > 0 && numlayers > 1 && (stacklayers || tilelayers) )
      SyncClones();

   if ( numlayers > 1 && tilelayers ) {
      if ( tileindex >= 0 && ( wd != GetLayer(tileindex)->view->getwidth() ||
                               ht != GetLayer(tileindex)->view->getheight() ) ) {
         // might happen on Win/GTK???
         GetLayer(tileindex)->view->resize(wd, ht);
      }
   } else if ( wd != currlayer->view->getwidth() || ht != currlayer->view->getheight() ) {
      // need to change viewport size;
      // can happen on Windows when resizing/maximizing main window
      SetViewSize(wd, ht);
   }

   #if defined(__WXMAC__) || defined(__WXGTK__)
      // windows on Mac OS X and GTK+ 2.0 are automatically buffered
      wxPaintDC dc(this);
      DrawView(dc, tileindex);
   #else
      if ( buffered || waitingforclick || GridVisible() || currlayer->currsel.Visible(NULL) ||
           showcontrols || (numlayers > 1 && (stacklayers || tilelayers)) ) {
         // use wxWidgets buffering to avoid flicker
         if (wd != viewbitmapwd || ht != viewbitmapht) {
            // need to create a new bitmap for current viewport
            delete viewbitmap;
            viewbitmap = new wxBitmap(wd, ht);
            if (viewbitmap == NULL) Fatal(_("Not enough memory to do buffering!"));
            viewbitmapwd = wd;
            viewbitmapht = ht;
         }
         wxBufferedPaintDC dc(this, *viewbitmap);
         DrawView(dc, tileindex);
      } else {
         wxPaintDC dc(this);
         DrawView(dc, tileindex);
      }
   #endif
}

// -----------------------------------------------------------------------------

void PatternView::OnSize(wxSizeEvent& event)
{
   int wd, ht;
   GetClientSize(&wd, &ht);
   
   // resize this viewport
   SetViewSize(wd, ht);
   
   event.Skip();
}

// -----------------------------------------------------------------------------

void PatternView::OnKeyDown(wxKeyEvent& event)
{
   #ifdef __WXMAC__
      // close any open tool tip window (fixes wxMac bug?)
      wxToolTip::RemoveToolTips();
   #endif
   
   statusptr->ClearMessage();

   realkey = event.GetKeyCode();
   int mods = event.GetModifiers();

   if (realkey == WXK_SHIFT) {
      // pressing shift key temporarily toggles the draw/pick cursors or the
      // zoom in/out cursors; note that Windows sends multiple key-down events
      // while shift key is pressed so we must be careful to toggle only once
      if (currlayer->curs == curs_pencil && oldcursor == NULL) {
         oldcursor = curs_pencil;
         SetCursorMode(curs_pick);
         mainptr->UpdateUserInterface(mainptr->IsActive());
      } else if (currlayer->curs == curs_pick && oldcursor == NULL) {
         oldcursor = curs_pick;
         SetCursorMode(curs_pencil);
         mainptr->UpdateUserInterface(mainptr->IsActive());
      } else if (currlayer->curs == curs_zoomin && oldcursor == NULL) {
         oldcursor = curs_zoomin;
         SetCursorMode(curs_zoomout);
         mainptr->UpdateUserInterface(mainptr->IsActive());
      } else if (currlayer->curs == curs_zoomout && oldcursor == NULL) {
         oldcursor = curs_zoomout;
         SetCursorMode(curs_zoomin);
         mainptr->UpdateUserInterface(mainptr->IsActive());
      }
   }
   
   if (debuglevel == 1) {
      // set debugkey now but don't show it until OnChar
      debugkey = wxString::Format(_("OnKeyDown: key=%d (%c) mods=%d"),
                                  realkey, realkey < 128 ? wxChar(realkey) : wxChar('?'), mods);
   }

   // WARNING: logic must match that in KeyComboCtrl::OnKeyDown in wxprefs.cpp
   if (mods == wxMOD_NONE || realkey == WXK_ESCAPE || realkey > 127) {
      // tell OnChar handler to ignore realkey
      realkey = 0;
   }
   
   #ifdef __WXMSW__
      // on Windows, OnChar is NOT called for some ctrl-key combos like
      // ctrl-0..9 or ctrl-alt-key, so we call OnChar ourselves
      if (realkey > 0 && (mods & wxMOD_CONTROL)) {
         OnChar(event);
         return;
      }
   #endif

   #ifdef __WXMAC__
      // allow option-E/I/N/U/` (OnChar is not called for those key combos
      // although the prefs dialog KeyComboCtrl::OnChar *is* called)
      if (mods == wxMOD_ALT && (realkey == 'E' || realkey == 'I' || realkey == 'N' ||
                                realkey == 'U' || realkey == '`')) {
         OnChar(event);
         return;
      }
   #endif

   #ifdef __WXGTK__
      if (realkey == ' ' && mods == wxMOD_SHIFT) {
         // fix wxGTK bug (curiously, the bug isn't seen in the prefs dialog);
         // OnChar won't see the shift modifier, so set realkey to a special
         // value to tell OnChar that shift-space was pressed
         realkey = -666;
      }
   #endif

   event.Skip();
}

// -----------------------------------------------------------------------------

void PatternView::OnKeyUp(wxKeyEvent& event)
{
   int key = event.GetKeyCode();

   if (key == WXK_SHIFT) {
      // releasing shift key sets cursor back to original state
      if (oldcursor) {
         SetCursorMode(oldcursor);
         oldcursor = NULL;
         mainptr->UpdateUserInterface(mainptr->IsActive());
      }
   }

   event.Skip();
}

// -----------------------------------------------------------------------------

void PatternView::OnChar(wxKeyEvent& event)
{
   // get translated keyboard event
   int key = event.GetKeyCode();
   int mods = event.GetModifiers();
   
   if (debuglevel == 1) {
      debugkey += wxString::Format(_("\nOnChar: key=%d (%c) mods=%d"),
                                   key, key < 128 ? wxChar(key) : wxChar('?'), mods);
      Warning(debugkey);
   }

   // WARNING: logic must match that in KeyComboCtrl::OnChar in wxprefs.cpp
   if (realkey > 0 && mods != wxMOD_NONE) {
      #ifdef __WXGTK__
         // sigh... wxGTK returns inconsistent results for shift-comma combos
         // so we assume that '<' is produced by pressing shift-comma
         // (which might only be true for US keyboards)
         if (key == '<' && (mods & wxMOD_SHIFT)) realkey = ',';
      #endif
      #ifdef __WXMSW__
         // sigh... wxMSW returns inconsistent results for some shift-key combos
         // so again we assume we're using a US keyboard
         if (key == '~' && (mods & wxMOD_SHIFT)) realkey = '`';
         if (key == '+' && (mods & wxMOD_SHIFT)) realkey = '=';
      #endif
      if (mods == wxMOD_SHIFT && key != realkey) {
         // use translated key code but remove shift key;
         // eg. we want shift-'/' to be seen as '?'
         mods = wxMOD_NONE;
      } else {
         // use key code seen by OnKeyDown
         key = realkey;
         if (key >= 'A' && key <= 'Z') key += 32;  // convert A..Z to a..z
      }
   }

   #ifdef __WXGTK__
      if (realkey == -666) {
         // OnKeyDown saw that shift-space was pressed but for some reason
         // OnChar doesn't see the modifier (ie. mods is wxMOD_NONE)
         key = ' ';
         mods = wxMOD_SHIFT;
      }
   #endif

   // do this check first because we allow user to make a selection while
   // generating a pattern or running a script
   if ( selectingcells && key == WXK_ESCAPE ) {
      RestoreSelection();
      return;
   }

   if (inscript) {
      // let script decide what to do with the key
      if (mods == wxMOD_SHIFT && key >= 'a' && key <= 'z') {
         // let script see A..Z
         PassKeyToScript(key - 32);
      } else {
         PassKeyToScript(key);
      }
      return;
   }

   // test waitingforclick before mainptr->generating so user can cancel
   // a paste operation while generating
   if ( waitingforclick && key == WXK_ESCAPE ) {
      // cancel paste
      pastex = -1;
      pastey = -1;
      waitingforclick = false;
      return;
   }
   
   if ( TimelineExists() && key == WXK_ESCAPE ) {
      if (currlayer->algo->isrecording()) {
         StartStopRecording();   // stop recording
      } else {
         PlayTimeline(0);        // stop autoplay
      }
      return;
   }
   
   if ( mainptr->generating && key == WXK_ESCAPE ) {
      mainptr->Stop();
      return;
   }

   ProcessKey(key, mods);
   mainptr->UpdateUserInterface(mainptr->IsActive());
}

// -----------------------------------------------------------------------------

void PatternView::ProcessClickedControl()
{
   switch (clickedcontrol) {
      case STEP1_CONTROL:
         if (TimelineExists()) {
            // reset autoplay speed to 0 (no delay, no frame skipping)
            ResetTimelineSpeed();
         } else if (currlayer->currexpo != 0) {
            mainptr->SetStepExponent(0);
            statusptr->Refresh(false);
            statusptr->Update();
         }
         break;
      
      case SLOWER_CONTROL:
         mainptr->GoSlower();
         break;
         
      case FASTER_CONTROL:
         mainptr->GoFaster();
         break;
         
      case FIT_CONTROL:
         FitPattern();
         break;
         
      case ZOOMIN_CONTROL:
         ZoomIn();
         break;
         
      case ZOOMOUT_CONTROL:
         ZoomOut();
         break;
         
      case NW_CONTROL:
         PanNW();
         break;
         
      case UP_CONTROL:
         PanUp( SmallScroll(currlayer->view->getheight()) );
         break;
         
      case NE_CONTROL:
         PanNE();
         break;
         
      case LEFT_CONTROL:
         PanLeft( SmallScroll(currlayer->view->getwidth()) );
         break;
         
      case MIDDLE_CONTROL:
         ViewOrigin();
         break;
         
      case RIGHT_CONTROL:
         PanRight( SmallScroll(currlayer->view->getwidth()) );
         break;
         
      case SW_CONTROL:
         PanSW();
         break;
         
      case DOWN_CONTROL:
         PanDown( SmallScroll(currlayer->view->getheight()) );
         break;
         
      case SE_CONTROL:
         PanSE();
         break;
      
      default:    // should never happen
         break;
   }
}

// -----------------------------------------------------------------------------

void PatternView::ProcessClick(int x, int y, bool shiftdown)
{
   // user has clicked x,y pixel in viewport
   if (showcontrols) {
      currcontrol = WhichControl(x - controlsrect.x, y - controlsrect.y);
      if (currcontrol > NO_CONTROL) {
         clickedcontrol = currcontrol;       // remember which control was clicked
         clicktime = stopwatch->Time();      // remember when clicked (in millisecs)
         CaptureMouse();                     // get mouse up event even if outside view
         dragtimer->Start(DRAG_RATE);        // see OnDragTimer
         RefreshRect(controlsrect, false);   // redraw clicked button
         #ifdef __WXGTK__
            // nicer to see change immediately on Linux
            Update();
         #endif
         if (PANNING_CONTROL) {
            // scroll immediately
            ProcessClickedControl();
         }
      }
   
   } else if (currlayer->curs == curs_pencil) {
      if (inscript) {
         // best not to clobber any status bar message displayed by script
         Warning(_("Drawing is not allowed while a script is running."));
         return;
      }
      if (!PointInGrid(x, y)) {
         statusptr->ErrorMessage(_("Drawing is not allowed outside grid."));
         return;
      }
      if (TimelineExists()) {
         statusptr->ErrorMessage(_("Drawing is not allowed if there is a timeline."));
         return;
      }
      if (currlayer->view->getmag() < 0) {
         statusptr->ErrorMessage(_("Drawing is not allowed at scales greater than 1 cell per pixel."));
         return;
      }
      if (mainptr->generating) {
         // we allow drawing while generating
         mainptr->Stop();
         mainptr->draw_pending = true;
         mainptr->mouseevent.m_x = x;
         mainptr->mouseevent.m_y = y;
         return;
      }
      StartDrawingCells(x, y);

   } else if (currlayer->curs == curs_pick) {
      if (inscript) {
         // best not to clobber any status bar message displayed by script
         Warning(_("Picking is not allowed while a script is running."));
         return;
      }
      if (!PointInGrid(x, y)) {
         statusptr->ErrorMessage(_("Picking is not allowed outside grid."));
         return;
      }
      if (currlayer->view->getmag() < 0) {
         statusptr->ErrorMessage(_("Picking is not allowed at scales greater than 1 cell per pixel."));
         return;
      }
      PickCell(x, y);

   } else if (currlayer->curs == curs_cross) {
      TestAutoFit();
      StartSelectingCells(x, y, shiftdown);

   } else if (currlayer->curs == curs_hand) {
      TestAutoFit();
      StartMovingView(x, y);

   } else if (currlayer->curs == curs_zoomin) {
      ZoomInPos(x, y);

   } else if (currlayer->curs == curs_zoomout) {
      ZoomOutPos(x, y);
   }
}

// -----------------------------------------------------------------------------

void PatternView::OnMouseDown(wxMouseEvent& event)
{
   if (waitingforclick) {
      // save paste location
      pastex = event.GetX();
      pastey = event.GetY();
      waitingforclick = false;
   } else {
      statusptr->ClearMessage();
      mainptr->showbanner = false;

      if (numlayers > 1 && tilelayers && tileindex < 0) {
         // ignore click in tile border
         return;
      }
   
      if (tileindex >= 0 && tileindex != currindex) {
         // switch current layer to clicked tile
         SwitchToClickedTile(tileindex);
         return;
      }
      
      ProcessClick(event.GetX(), event.GetY(), event.ShiftDown());
      mainptr->UpdateUserInterface(mainptr->IsActive());
   }
}

// -----------------------------------------------------------------------------

void PatternView::OnMouseUp(wxMouseEvent& WXUNUSED(event))
{
   if (drawingcells || selectingcells || movingview || clickedcontrol > NO_CONTROL) {
      StopDraggingMouse();
   } else if (mainptr->draw_pending) {
      // this can happen if user does a quick click while pattern is generating,
      // so set a special flag to force drawing to terminate
      stopdrawing = true;
   }
}

// -----------------------------------------------------------------------------

#if wxCHECK_VERSION(2, 8, 0)

// mouse capture can be lost on Windows before mouse-up event
void PatternView::OnMouseCaptureLost(wxMouseCaptureLostEvent& WXUNUSED(event))
{
   if (drawingcells || selectingcells || movingview || clickedcontrol > NO_CONTROL) {
      StopDraggingMouse();
   }
}

#endif

// -----------------------------------------------------------------------------

void PatternView::OnRMouseDown(wxMouseEvent& event)
{
   // this is equivalent to control-click
   statusptr->ClearMessage();
   mainptr->showbanner = false;

   if (numlayers > 1 && tilelayers && tileindex < 0) {
      // ignore click in tile border
      return;
   }
   
   if (tileindex >= 0 && tileindex != currindex) {
      // switch current layer to clicked tile
      SwitchToClickedTile(tileindex);
      return;
   }
   
   if (currlayer->curs == curs_zoomin) {
      ZoomOutPos(event.GetX(), event.GetY());
   } else if (currlayer->curs == curs_zoomout) {
      ZoomInPos(event.GetX(), event.GetY());
   }
}

// -----------------------------------------------------------------------------

void PatternView::OnMouseWheel(wxMouseEvent& event)
{
   // wheelpos should be persistent, because in theory we should keep track of
   // the remainder if the amount scrolled was not an even number of deltas.
   static int wheelpos = 0;
   int delta;

   if (mousewheelmode == 0) {
      // ignore wheel, according to user preference
      event.Skip();
      return;
   }

   // delta is the amount that represents one "step" of rotation. Normally 120.
   delta = event.GetWheelDelta();

   if (mousewheelmode == 2)
      wheelpos -= event.GetWheelRotation();
   else
      wheelpos += event.GetWheelRotation();

   while (wheelpos >= delta) {
      wheelpos -= delta;
      TestAutoFit();
      currlayer->view->unzoom();
   }

   while (wheelpos <= -delta) {
      wheelpos += delta;
      TestAutoFit();
      if (currlayer->view->getmag() < MAX_MAG) {
         currlayer->view->zoom();
      } else {
         Beep();
         wheelpos = 0;
         break;         // best not to beep lots of times
      }
   }

   // allow mouse interaction if script is running
   bool saveinscript = inscript;
   inscript = false;
   mainptr->UpdateEverything();
   inscript = saveinscript;
}

// -----------------------------------------------------------------------------

void PatternView::OnMouseMotion(wxMouseEvent& event)
{
   statusptr->CheckMouseLocation(mainptr->IsActive());
   
   // check if translucent controls need to be shown/hidden
   if (mainptr->IsActive()) {
      wxPoint pt( event.GetX(), event.GetY() );
      bool show = (controlsrect.Contains(pt) || clickedcontrol > NO_CONTROL) &&
                  !(drawingcells || selectingcells || movingview || waitingforclick) &&
                  !(numlayers > 1 && tilelayers && tileindex != currindex);
      if (showcontrols != show) {
         // let CheckCursor set showcontrols and call RefreshRect
         CheckCursor(true);
      }
   }
}

// -----------------------------------------------------------------------------

void PatternView::OnMouseEnter(wxMouseEvent& WXUNUSED(event))
{
   // Win bug??? we don't get this event if CaptureMouse has been called
   CheckCursor(mainptr->IsActive());
   // no need to call CheckMouseLocation here (OnMouseMotion will be called)
}

// -----------------------------------------------------------------------------

void PatternView::OnMouseExit(wxMouseEvent& WXUNUSED(event))
{
   // Win bug??? we don't get this event if CaptureMouse has been called
   CheckCursor(mainptr->IsActive());
   statusptr->CheckMouseLocation(mainptr->IsActive());
}

// -----------------------------------------------------------------------------

void PatternView::OnDragTimer(wxTimerEvent& WXUNUSED(event))
{
   // called periodically while drawing/selecting/moving
   // or if user has clicked a translucent control and button is still down
   wxPoint pt = ScreenToClient( wxGetMousePosition() );
   int x = pt.x;
   int y = pt.y;
   
   if (clickedcontrol > NO_CONTROL) {
      control_id oldcontrol = currcontrol;
      currcontrol = WhichControl(x - controlsrect.x, y - controlsrect.y);
      if (currcontrol == clickedcontrol) {
         if (PANNING_CONTROL && stopwatch->Time() - clicktime > 300) {
            // panning can be repeated while button is pressed, but only after
            // a short pause (0.3 secs) from the time the button was clicked
            // (this matches the way scroll buttons work on Mac/Windows)
            ProcessClickedControl();
         }
      } else {
         currcontrol = NO_CONTROL;
      }
      if (currcontrol != oldcontrol) RefreshRect(controlsrect, false);
      return;
   }

   // don't test "!PointInView(x, y)" here -- we want to allow scrolling
   // in full screen mode when mouse is at outer edge of view
   if ( x <= 0 || x >= currlayer->view->getxmax() ||
        y <= 0 || y >= currlayer->view->getymax() ) {
      
      // user can disable scrolling
      if ( drawingcells && !scrollpencil ) {
         DrawCells(x, y);
         return;
      }
      if ( selectingcells && !scrollcross ) {
         SelectCells(x, y);
         return;
      }
      if ( movingview && !scrollhand ) {
         // make sure x,y is within viewport
         if (x < 0) x = 0;
         if (y < 0) y = 0;
         if (x > currlayer->view->getxmax()) x = currlayer->view->getxmax();
         if (y > currlayer->view->getymax()) y = currlayer->view->getymax();
         MoveView(x, y);
         return;
      }
      
      // scroll view
      int xamount = 0;
      int yamount = 0;
      if (x <= 0) xamount = -SmallScroll( currlayer->view->getwidth() );
      if (y <= 0) yamount = -SmallScroll( currlayer->view->getheight() );
      if (x >= currlayer->view->getxmax())
         xamount = SmallScroll( currlayer->view->getwidth() );
      if (y >= currlayer->view->getymax())
         yamount = SmallScroll( currlayer->view->getheight() );

      if ( drawingcells ) {
         currlayer->view->move(xamount, yamount);
         mainptr->UpdatePatternAndStatus();

      } else if ( selectingcells ) {
         currlayer->view->move(xamount, yamount);
         // no need to call UpdatePatternAndStatus() here because
         // it will be called soon in SelectCells, except in this case:
         if (forceh || forcev || currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0) {
            // selection might not change so must update pattern
            RefreshView();
            // need to update now if script is running
            if (inscript) {
               inscript = false;
               mainptr->UpdatePatternAndStatus();
               inscript = true;
            }
         }

      } else if ( movingview ) {
         // scroll in opposite direction, and if both amounts are non-zero then
         // set both to same (larger) absolute value so user can scroll at 45 degrees
         if ( xamount != 0 && yamount != 0 ) {
            if ( abs(xamount) > abs(yamount) ) {
               yamount = yamount < 0 ? -abs(xamount) : abs(xamount);
            } else {
               xamount = xamount < 0 ? -abs(yamount) : abs(yamount);
            }
         }
         currlayer->view->move(-xamount, -yamount);
         
         // allow mouse interaction if script is running
         bool saveinscript = inscript;
         inscript = false;
         mainptr->UpdatePatternAndStatus();
         inscript = saveinscript;
         
         // adjust x,y and bigcellx,bigcelly for MoveView call below
         x += xamount;
         y += yamount;
         pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
         bigcellx = cellpos.first;
         bigcelly = cellpos.second;
      }

      // need to update scroll bars if grid is bounded
      if (currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0) {
         UpdateScrollBars();
      }
   }

   if ( drawingcells ) {
      DrawCells(x, y);

   } else if ( selectingcells ) {
      SelectCells(x, y);

   } else if ( movingview ) {
      MoveView(x, y);
   }
}

// -----------------------------------------------------------------------------

void PatternView::OnScroll(wxScrollWinEvent& event)
{   
   WXTYPE type = event.GetEventType();
   int orient = event.GetOrientation();

   if (type == wxEVT_SCROLLWIN_LINEUP) {
      if (orient == wxHORIZONTAL)
         PanLeft( SmallScroll(currlayer->view->getwidth()) );
      else
         PanUp( SmallScroll(currlayer->view->getheight()) );

   } else if (type == wxEVT_SCROLLWIN_LINEDOWN) {
      if (orient == wxHORIZONTAL)
         PanRight( SmallScroll(currlayer->view->getwidth()) );
      else
         PanDown( SmallScroll(currlayer->view->getheight()) );

   } else if (type == wxEVT_SCROLLWIN_PAGEUP) {
      if (orient == wxHORIZONTAL)
         PanLeft( BigScroll(currlayer->view->getwidth()) );
      else
         PanUp( BigScroll(currlayer->view->getheight()) );

   } else if (type == wxEVT_SCROLLWIN_PAGEDOWN) {
      if (orient == wxHORIZONTAL)
         PanRight( BigScroll(currlayer->view->getwidth()) );
      else
         PanDown( BigScroll(currlayer->view->getheight()) );

   } else if (type == wxEVT_SCROLLWIN_THUMBTRACK) {
      int newpos = event.GetPosition();
      int amount = newpos - (orient == wxHORIZONTAL ? hthumb : vthumb);
      if (amount != 0) {
         TestAutoFit();
         if (currlayer->view->getmag() > 0) {
            // amount is in cells so convert to pixels
            amount = amount << currlayer->view->getmag();
         }
         if (orient == wxHORIZONTAL) {
            hthumb = newpos;
            currlayer->view->move(amount, 0);
            // don't call UpdateEverything here because it calls UpdateScrollBars
            RefreshView();
            // don't Update() immediately -- more responsive
         } else {
            vthumb = newpos;
            currlayer->view->move(0, amount);
            // don't call UpdateEverything here because it calls UpdateScrollBars
            RefreshView();
            // don't Update() immediately -- more responsive
         }
      }

   } else if (type == wxEVT_SCROLLWIN_THUMBRELEASE) {
      // now we can call UpdateScrollBars
      mainptr->UpdateEverything();
   }
   
   // need an update if script is running
   if (inscript && type != wxEVT_SCROLLWIN_THUMBTRACK) {
      inscript = false;
      mainptr->UpdatePatternAndStatus();
      bigview->UpdateScrollBars();
      inscript = true;
   }
}

// -----------------------------------------------------------------------------

void PatternView::OnEraseBackground(wxEraseEvent& WXUNUSED(event))
{
   // do nothing because we'll be painting the entire viewport
   
   //!!! why does this get called even though we always call Refresh(false)???
   // and why does bg still get erased (on Mac and GTK, but not Windows)???
   // note that eraseBack parameter in wxWindowMac::Refresh in window.cpp is never used!
}

// -----------------------------------------------------------------------------

// create the viewport window
PatternView::PatternView(wxWindow* parent, wxCoord x, wxCoord y, int wd, int ht, long style)
   : wxWindow(parent, wxID_ANY, wxPoint(x,y), wxSize(wd,ht), style)
{
   dragtimer = new wxTimer(this, wxID_ANY);
   if (dragtimer == NULL) Fatal(_("Failed to create drag timer!"));
   
   cellbrush = new wxBrush(*wxBLACK_BRUSH);
   if (cellbrush == NULL) Fatal(_("Failed to create cell brush!"));
   
   // avoid erasing background on GTK+ -- doesn't work!!!
   SetBackgroundStyle(wxBG_STYLE_CUSTOM);

   // force viewbitmap to be created in first OnPaint call
   viewbitmap = NULL;
   viewbitmapwd = -1;
   viewbitmapht = -1;

   drawingcells = false;      // not drawing cells
   selectingcells = false;    // not selecting cells
   movingview = false;        // not moving view
   waitingforclick = false;   // not waiting for user to click
   nopattupdate = false;      // enable pattern updates
   showcontrols = false;      // not showing translucent controls
   oldcursor = NULL;          // for toggling cursor via shift key 
}

// -----------------------------------------------------------------------------

// destroy the viewport window
PatternView::~PatternView()
{
   delete dragtimer;
   delete viewbitmap;
   delete cellbrush;
}
