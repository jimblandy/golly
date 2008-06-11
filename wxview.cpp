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

#include "wx/file.h"       // for wxFile
#include "wx/dcbuffer.h"   // for wxBufferedPaintDC

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "viewport.h"

#include "wxgolly.h"       // for mainptr, statusptr
#include "wxutils.h"       // for Warning, Fatal
#include "wxprefs.h"       // for showgridlines, canchangerule, etc
#include "wxhelp.h"        // for ShowHelp
#include "wxmain.h"        // for mainptr->...
#include "wxstatus.h"      // for statusptr->...
#include "wxrender.h"      // for CreatePasteImage, DrawView, DrawSelection
#include "wxscript.h"      // for inscript, PassKeyToScript
#include "wxedit.h"        // for Selection
#include "wxundo.h"        // for currlayer->undoredo->...
#include "wxalgos.h"       // for algo_type, *_ALGO, CreateNewUniverse
#include "wxlayer.h"       // for currlayer, ResizeLayers, etc
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
   int v = 0 ;
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
      // CheckCursor(true);            // probs on Mac if Paste menu item selected
      #ifdef __WXMAC__
         wxSetCursor(*currlayer->curs);
      #endif
      SetCursor(*currlayer->curs);

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
      
      if ( pastex < 0 || pastex > currlayer->view->getxmax() ||
           pastey < 0 || pastey > currlayer->view->getymax() ) {
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

   // set pastex,pastey to top left cell of paste rectangle
   pastex = left.toint();
   pastey = top.toint();

   // save cell changes if undo/redo is enabled and script isn't constructing a pattern
   bool savecells = allowundo && !currlayer->stayclean;
   if (savecells && inscript) SavePendingChanges();

   // copy pattern from temporary universe to current universe
   int tx, ty, cx, cy;
   double maxcount = wd.todouble() * ht.todouble();
   int cntr = 0;
   bool abort = false;
   bool pattchanged = false;
   BeginProgress(_("Pasting pattern"));
   
   // we can speed up pasting sparse patterns by using nextcell in these cases:
   // - if using Or mode
   // - if current universe is empty
   // - if paste rect is outside current pattern edges
   bool usenextcell;
   if ( pmode == Or || currlayer->algo->isEmpty() ) {
      usenextcell = true;
   } else {
      bigint ctop, cleft, cbottom, cright;
      currlayer->algo->findedges(&ctop, &cleft, &cbottom, &cright);
      usenextcell = top > cbottom || bottom < ctop || left > cright || right < cleft;
   }
   
   lifealgo* curralgo = currlayer->algo;
   if ( usenextcell ) {
      int v = 0 ;
      cy = pastey;
      for ( ty=itop; ty<=ibottom; ty++ ) {
         cx = pastex;
         for ( tx=ileft; tx<=iright; tx++ ) {
            int skip = tempalgo->nextcell(tx, ty, v);
            if (skip + tx > iright)
               skip = -1;           // pretend we found no more live cells
            if (skip >= 0) {
               // found next live cell so paste it into current universe
               tx += skip;
               cx += skip;
               int oldstate = curralgo->getcell(cx, cy);
               if (oldstate != v) {
                  curralgo->setcell(cx, cy, v);
                  pattchanged = true;
                  if (savecells) currlayer->undoredo->SaveCellChange(cx, cy, oldstate, v);
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
      cy = pastey;
      for ( ty=itop; ty<=ibottom; ty++ ) {
         cx = pastex;
         for ( tx=ileft; tx<=iright; tx++ ) {
            tempstate = tempalgo->getcell(tx, ty);
            currstate = curralgo->getcell(cx, cy);
            switch (pmode) {
               case Copy:
                  if (tempstate != currstate) {
                     curralgo->setcell(cx, cy, tempstate);
                     pattchanged = true;
                     if (savecells)
                        currlayer->undoredo->SaveCellChange(cx, cy, currstate, tempstate);
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
                        if (savecells)
                           currlayer->undoredo->SaveCellChange(cx, cy, currstate, 0);
                     }
                  } else {
                     if (currstate != tempstate) {
                        curralgo->setcell(cx, cy, tempstate);
                        pattchanged = true;
                        if (savecells)
                           currlayer->undoredo->SaveCellChange(cx, cy, currstate, tempstate);
                     }
                  }
                  break;
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

   if (pattchanged) currlayer->algo->endofpattern();
   EndProgress();
   
   // tidy up and display result
   statusptr->ClearMessage();
   if (pattchanged) {
      if (savecells) currlayer->undoredo->RememberCellChanges(_("Paste"), currlayer->dirty);
      MarkLayerDirty();    // calls SetWindowTitle
      mainptr->UpdatePatternAndStatus();
   }
   
   // pasting clipboard pattern can also cause a rule change
   if (canchangerule > 0 && oldrule != newrule) {
      const char* err = currlayer->algo->setrule( newrule.mb_str(wxConvLocal) );
      // setrule should succeed (earlier readclipboard didn't return error) but play safe
      if (err) {
         currlayer->algo->setrule( oldrule.mb_str(wxConvLocal) );
         Warning( wxString(err,wxConvLocal) );
      } else {
         // show new rule in title bar
         mainptr->SetWindowTitle(wxEmptyString);
         if (savecells) currlayer->undoredo->RememberRuleChange(oldrule);
      }
   }
}

// -----------------------------------------------------------------------------

bool PatternView::GetClipboardPattern(lifealgo** tempalgo,
                                      bigint* t, bigint* l, bigint* b, bigint* r)
{
   #ifdef __WXX11__
      if ( !wxFileExists(mainptr->clipfile) ) return false;
   #else
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
   #endif         

   // remember rule in case readclipboard changes it
   oldrule = wxString(currlayer->algo->getrule(), wxConvLocal);

   const char* err = readclipboard(mainptr->clipfile.mb_str(wxConvLocal), **tempalgo, t, l, b, r);
   if (err) {
      // cycle thru all other algos until readclipboard succeeds
      for (int i = 0; i < NUM_ALGOS; i++) {
         if (i != currlayer->algtype) {
            delete *tempalgo;
            *tempalgo = CreateNewUniverse((algo_type) i);
            err = readclipboard(mainptr->clipfile.mb_str(wxConvLocal), **tempalgo, t, l, b, r);
            if (!err) break;
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
         newrule = wxString(currlayer->algo->getrule(), wxConvLocal);
      }
   }
   
   // restore rule now in case error occurred
   currlayer->algo->setrule( oldrule.mb_str(wxConvLocal) );

   #ifdef __WXX11__
      // don't delete clipboard file
   #else
      wxRemoveFile(mainptr->clipfile);
   #endif

   if (err) {
      // error could be due to bad rule string in clipboard data
      Warning(wxString(err,wxConvLocal));
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

   // read clipboard pattern into temporary universe;
   // note that tempalgo may be deleted and re-created
   bigint top, left, bottom, right;
   if ( GetClipboardPattern(&tempalgo, &top, &left, &bottom, &right) ) {
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
   if (pmode == Copy) {
      pmode = Or;
      if (!waitingforclick) statusptr->DisplayMessage(_("Paste mode is Or."));
   } else if (pmode == Or) {
      pmode = Xor;
      if (!waitingforclick) statusptr->DisplayMessage(_("Paste mode is Xor."));
   } else {
      pmode = Copy;
      if (!waitingforclick) statusptr->DisplayMessage(_("Paste mode is Copy."));
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
      wxBell();
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
      return true;
   } else {
      // mouse not in viewport
      return false;
   }
}

// -----------------------------------------------------------------------------

bool PatternView::PointInView(int x, int y)
{
   return (x >= 0) && (x <= currlayer->view->getxmax()) &&
          (y >= 0) && (y <= currlayer->view->getymax());
}

// -----------------------------------------------------------------------------

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
         } else {
            #ifdef __WXMAC__
               // wxMac bug??? need this to fix probs after toggling status/tool bar
               wxSetCursor(*currlayer->curs);
            #endif
            SetCursor(*currlayer->curs);
         }
      } else {
         #ifdef __WXMAC__
            wxSetCursor(*wxSTANDARD_CURSOR);
         #endif
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
      // scroll by integral number of cells to avoid rounding probs
      viewwd = currlayer->view->getwidth() >> mag;
      viewht = currlayer->view->getheight() >> mag;
   } else {
      viewwd = currlayer->view->getwidth();
      viewht = currlayer->view->getheight();
   }
   
   // keep thumb boxes in middle of scroll bars
   hthumb = (thumbrange - 1) * viewwd / 2;
   vthumb = (thumbrange - 1) * viewht / 2;
   
   // only big viewport window has scroll bars
   bigview->SetScrollbar(wxHORIZONTAL, hthumb, viewwd, thumbrange * viewwd, true);
   bigview->SetScrollbar(wxVERTICAL, vthumb, viewht, thumbrange * viewht, true);
}

// -----------------------------------------------------------------------------

void PatternView::ProcessKey(int key, int modifiers)
{
   mainptr->showbanner = false;

   // WARNING: ProcessKey can be called while running a script or while
   // waiting for a paste click, so we must avoid doing any actions that
   // could cause havoc at such times.

   action_info action = FindAction(key, modifiers);
   switch (action.id) {
      case DO_NOTHING:
         // any unassigned key turns off full screen mode
         if (mainptr->fullscreen) mainptr->ToggleFullScreen();
         break;

      case DO_OPENFILE:
         {  wxString ext = action.file.AfterLast(wxT('.'));
            if ( ext.IsSameAs(wxT("html"),false) || ext.IsSameAs(wxT("htm"),false) ) {
               // show HTML file in help window
               if (!waitingforclick) ShowHelp(action.file);
            } else {
               // load pattern or run script
               if (!inscript) mainptr->OpenFile(action.file, true);
            }
         }
         break;

      // File menu actions
      case DO_NEWPATT:     if (!inscript) mainptr->NewPattern(); break;
      case DO_OPENPATT:    if (!inscript) mainptr->OpenPattern(); break;
      case DO_OPENCLIP:    if (!inscript) mainptr->OpenClipboard(); break;
      case DO_SAVE:        if (!inscript) mainptr->SavePattern(); break;
      case DO_SAVEXRLE:    if (!inscript) savexrle = !savexrle; break;
      case DO_RUNSCRIPT:   if (!inscript) mainptr->OpenScript(); break;
      case DO_RUNCLIP:     if (!inscript) mainptr->RunClipboard(); break;
      case DO_PREFS:       mainptr->ShowPrefsDialog(); break;
      case DO_PATTERNS:    mainptr->ToggleShowPatterns(); break;
      case DO_PATTDIR:     mainptr->ChangePatternDir(); break;
      case DO_SCRIPTS:     mainptr->ToggleShowScripts(); break;
      case DO_SCRIPTDIR:   mainptr->ChangeScriptDir(); break;
      case DO_QUIT:        mainptr->QuitApp(); break;

      // Edit menu actions
      case DO_UNDO:        if (!inscript) currlayer->undoredo->UndoChange(); break;
      case DO_REDO:        if (!inscript) currlayer->undoredo->RedoChange(); break;
      case DO_DISABLE:     if (!inscript) mainptr->ToggleAllowUndo(); break;
      case DO_CUT:         if (!inscript) CutSelection(); break;
      case DO_COPY:        if (!inscript) CopySelection(); break;
      case DO_CLEAR:       if (!inscript) ClearSelection(); break;
      case DO_CLEAROUT:    if (!inscript) ClearOutsideSelection(); break;
      case DO_PASTE:       if (!inscript) PasteClipboard(false); break;
      case DO_PASTESEL:    if (!inscript) PasteClipboard(true); break;
      case DO_SELALL:      if (!inscript) SelectAll(); break;
      case DO_REMOVESEL:   if (!inscript) RemoveSelection(); break;
      case DO_SHRINK:      if (!inscript) ShrinkSelection(false); break;
      case DO_SHRINKFIT:   if (!inscript) ShrinkSelection(true); break;
      case DO_RANDFILL:    if (!inscript) RandomFill(); break;
      case DO_FLIPTB:      if (!inscript) FlipSelection(true); break;
      case DO_FLIPLR:      if (!inscript) FlipSelection(false); break;
      case DO_ROTATECW:    if (!inscript) RotateSelection(true); break;
      case DO_ROTATEACW:   if (!inscript) RotateSelection(false); break;
      case DO_ADVANCE:     if (!inscript) currlayer->currsel.Advance(); break;
      case DO_ADVANCEOUT:  if (!inscript) currlayer->currsel.AdvanceOutside(); break;
      case DO_CURSDRAW:    SetCursorMode(curs_pencil); break;
      case DO_CURSSEL:     SetCursorMode(curs_cross); break;
      case DO_CURSMOVE:    SetCursorMode(curs_hand); break;
      case DO_CURSIN:      SetCursorMode(curs_zoomin); break;
      case DO_CURSOUT:     SetCursorMode(curs_zoomout); break;
      case DO_CURSCYCLE:   CycleCursorMode(); break;
      case DO_PASTEMODE:   CyclePasteMode(); break;
      case DO_PASTELOC:    CyclePasteLocation(); break;

      // Control menu actions
      case DO_STARTSTOP:
         if (!inscript) {
            if (mainptr->generating) {
               mainptr->Stop();
            } else {
               mainptr->GeneratePattern();
            }
         }
         break;
      case DO_NEXTGEN:
      case DO_NEXTSTEP:
         if (!inscript) {
            if (mainptr->generating) {
               mainptr->Stop();
            } else {
               mainptr->NextGeneration(action.id == DO_NEXTSTEP);
            }
         }
         break;
      case DO_RESET:       if (!inscript) mainptr->ResetPattern(); break;
      case DO_SETGEN:      if (!inscript) mainptr->SetGeneration(); break;
      case DO_FASTER:      mainptr->GoFaster(); break;
      case DO_SLOWER:      mainptr->GoSlower(); break;
      case DO_AUTOFIT:     mainptr->ToggleAutoFit(); break;
      //!!! remove this action??? or change it to DO_CYCLEALGO???
      case DO_HASHING:
         if (!inscript) {
            if (currlayer->algtype != HLIFE_ALGO)
               mainptr->ChangeAlgorithm(HLIFE_ALGO);
            else
               mainptr->ChangeAlgorithm(QLIFE_ALGO);
         }
         break;
      case DO_HYPER:       mainptr->ToggleHyperspeed(); break;
      case DO_HASHINFO:    mainptr->ToggleHashInfo(); break;
      case DO_RULE:        if (!inscript) mainptr->ShowRuleDialog(); break;
      case DO_TIMING:      if (!inscript) mainptr->DisplayTimingInfo(); break;

      // View menu actions
      case DO_LEFT:        PanLeft( SmallScroll(currlayer->view->getwidth()) ); break;
      case DO_RIGHT:       PanRight( SmallScroll(currlayer->view->getwidth()) ); break;
      case DO_UP:          PanUp( SmallScroll(currlayer->view->getheight()) ); break;
      case DO_DOWN:        PanDown( SmallScroll(currlayer->view->getheight()) ); break;
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
      case DO_SHOWSTATUS:  mainptr->ToggleStatusBar(); break;
      case DO_SHOWEXACT:   mainptr->ToggleExactNumbers(); break;
      case DO_SHOWGRID:    ToggleGridLines(); break;
      case DO_SHOWICONS:   ToggleCellIcons(); break;
      case DO_SWAPCOLORS:  ToggleCellColors(); break;
      case DO_BUFFERED:    ToggleBuffering(); break;
      case DO_INFO:        mainptr->ShowPatternInfo(); break;

      // Layer menu actions
      case DO_ADD:         if (!inscript) AddLayer(); break;
      case DO_CLONE:       if (!inscript) CloneLayer(); break;
      case DO_DUPLICATE:   if (!inscript) DuplicateLayer(); break;
      case DO_DELETE:      if (!inscript) DeleteLayer(); break;
      case DO_DELOTHERS:   if (!inscript) DeleteOtherLayers(); break;
      case DO_MOVELAYER:   if (!inscript) MoveLayerDialog(); break;
      case DO_NAMELAYER:   if (!inscript) NameLayerDialog(); break;
      case DO_SYNCVIEWS:   if (!inscript) ToggleSyncViews(); break;
      case DO_SYNCCURS:    if (!inscript) ToggleSyncCursors(); break;
      case DO_STACK:       if (!inscript) ToggleStackLayers(); break;
      case DO_TILE:        if (!inscript) ToggleTileLayers(); break;

      // Help menu actions
      case DO_HELP:
         if (!waitingforclick) {
            // if help window is open then bring it to the front,
            // otherwise open it and display most recent help file
            ShowHelp(wxEmptyString);
         }
         break;
      case DO_ABOUT:       if (!inscript) ShowAboutBox(); break;
      
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

#if MAC_OS_X_VERSION_MIN_REQUIRED == 1030
   // use UpdateView to avoid wxMac bug on Mac OS 10.3.9
   slowdraw = true;
   return;
#else
   if (numlayers > 1 && (stacklayers || (numclones > 0 && tilelayers))) {
      // drawing must be done via UpdateView in ShowDrawing
      slowdraw = true;
      return;
   }
#endif

   int cellsize = 1 << currlayer->view->getmag();

   // convert given cell coords to view coords
   pair<bigint, bigint> lefttop = currlayer->view->at(0, 0);
   wxCoord x = (cx - lefttop.first.toint()) * cellsize;
   wxCoord y = (cy - lefttop.second.toint()) * cellsize;
   
   if (cellsize > 2) cellsize--;    // allow for gap between cells
   
   wxBitmap** iconmaps = NULL;
   if (currlayer->view->getmag() == 3) {
      iconmaps = icons7x7[currlayer->algtype];
   } else if (currlayer->view->getmag() == 4) {
      iconmaps = icons15x15[currlayer->algtype];
   }

   if (showicons && drawstate > 0 && currlayer->view->getmag() > 2 &&
       iconmaps && iconmaps[drawstate]) {
      // draw icon; icon bitmaps are masked so first we have to draw the background
      dc.SetBrush(swapcolors ? *livebrush[currindex] : *deadbrush);
      dc.DrawRectangle(x, y, cellsize, cellsize);
      dc.DrawBitmap(*iconmaps[drawstate], x, y, true);
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
      if (currlayer->algo->NumCellStates() > 2) {
         if (drawstate == 0) {
            cellbrush->SetColour(swapcolors ? *livergb[currindex] : *deadrgb);
         } else {
            cellbrush->SetColour(currlayer->algo->cellred[drawstate],
                                 currlayer->algo->cellgreen[drawstate],
                                 currlayer->algo->cellblue[drawstate]);
         }
         dc.SetBrush(*cellbrush);
      } else {
         dc.SetBrush(drawstate == (int)swapcolors ? *deadbrush : *livebrush[currindex]);
      }
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
   pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
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
      if (currlayer->algo->NumCellStates() > 2) {
         if (drawstate == 0) {
            cellbrush->SetColour(swapcolors ? *livergb[currindex] : *deadrgb);
         } else {
            cellbrush->SetColour(currlayer->algo->cellred[drawstate],
                                 currlayer->algo->cellgreen[drawstate],
                                 currlayer->algo->cellblue[drawstate]);
         }
         dc.SetBrush(*cellbrush);
      } else {
         dc.SetBrush(drawstate == (int)swapcolors ? *deadbrush : *livebrush[currindex]);
      }

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

void PatternView::StartSelectingCells(int x, int y, bool shiftdown)
{
   pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
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
   if ( abs(initselx - x) < 2 && abs(initsely - y) < 2 && !SelectionExists() ) {
      // avoid 1x1 selection if mouse hasn't moved much
      return;
   }

   pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
   if (!forcev) currlayer->currsel.SetLeftRight(cellpos.first, anchorx);
   if (!forceh) currlayer->currsel.SetTopBottom(cellpos.second, anchory);

   if (currlayer->currsel != prevsel) {
      // selection has changed
      DisplaySelectionSize();
      
      // allow mouse interaction if script is running
      bool saveinscript = inscript;
      inscript = false;
      mainptr->UpdatePatternAndStatus();
      inscript = saveinscript;
      
      prevsel = currlayer->currsel;
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
}

// -----------------------------------------------------------------------------

void PatternView::StopDraggingMouse()
{
   if (selectingcells) {
      if (allowundo) RememberNewSelection(_("Selection"));
      selectingcells = false;                // tested by CanUndo
      mainptr->UpdateMenuItems(true);        // enable various Edit menu items
   }
   
   if (drawingcells && allowundo) {
      // MarkLayerDirty (in ShowDrawing) has set dirty flag, so we need to
      // pass in the flag state saved before drawing started
      currlayer->undoredo->RememberCellChanges(_("Drawing"), currlayer->savedirty);
      drawingcells = false;                  // tested by CanUndo
      mainptr->UpdateMenuItems(true);        // enable Undo item
   }
   
   drawingcells = false;
   selectingcells = false;
   movingview = false;
   
   if ( HasCapture() ) ReleaseMouse();
   if ( dragtimer->IsRunning() ) dragtimer->Stop();
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
      wxBell();   // can't zoom in any further
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
   if (tileindex < 0) {
      // use main viewport window's size to reset viewport in each layer
      ResizeLayers(wd, ht);
   }
   
   // only autofit when generating
   if (currlayer->autofit && mainptr && mainptr->generating)
      currlayer->algo->fit(*currlayer->view, 0);
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
   // wd or ht might be < 1 on Win/X11 platforms
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
           (numlayers > 1 && (stacklayers || tilelayers)) ) {
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
   // wd or ht might be < 1 on Win/X11 platforms
   if (wd < 1) wd = 1;
   if (ht < 1) ht = 1;
   
   // resize this viewport
   SetViewSize(wd, ht);
   
   event.Skip();
}

// -----------------------------------------------------------------------------

void PatternView::OnKeyDown(wxKeyEvent& event)
{
   statusptr->ClearMessage();

   realkey = event.GetKeyCode();
   int mods = event.GetModifiers();

   if (realkey == WXK_SHIFT) {
      // pressing shift key temporarily toggles zoom in/out cursor;
      // some platforms (eg. WinXP) send multiple key-down events while
      // a key is pressed so we must be careful to toggle only once
      if (currlayer->curs == curs_zoomin && oldzoom == NULL) {
         oldzoom = curs_zoomin;
         SetCursorMode(curs_zoomout);
         mainptr->UpdateUserInterface(mainptr->IsActive());
      } else if (currlayer->curs == curs_zoomout && oldzoom == NULL) {
         oldzoom = curs_zoomout;
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
      // releasing shift key sets zoom in/out cursor back to original state
      if (oldzoom) {
         SetCursorMode(oldzoom);
         oldzoom = NULL;
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
      #ifdef __WXX11__
         // sigh... pressing shift key by itself causes key = 306, control key = 308
         // and other keys like caps lock and option = -1
         if ( key < 0 || key > 255 ) return;
      #endif
      // let script decide what to do with the key
      PassKeyToScript(key);
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
   
   if ( mainptr->generating && key == WXK_ESCAPE ) {
      mainptr->Stop();
      return;
   }

   ProcessKey(key, mods);
   mainptr->UpdateUserInterface(mainptr->IsActive());
}

// -----------------------------------------------------------------------------

void PatternView::ProcessControlClick(int x, int y)
{
   if (currlayer->curs == curs_zoomin) {
      ZoomOutPos(x, y);
   } else if (currlayer->curs == curs_zoomout) {
      ZoomInPos(x, y);
   }
}

// -----------------------------------------------------------------------------

void PatternView::ProcessClick(int x, int y, bool shiftdown)
{
   // user has clicked somewhere in viewport   
   if (currlayer->curs == curs_pencil) {
      if (inscript) {
         // statusptr->ErrorMessage does nothing if inscript is true
         Warning(_("Drawing is not allowed while a script is running."));
         return;
      }
      if (currlayer->view->getmag() < 0) {
         statusptr->ErrorMessage(_("Drawing is not allowed at scales greater than 1 cell per pixel."));
         return;
      }
      if (mainptr->generating) {
         // we now allow drawing while generating
         // statusptr->ErrorMessage(_("Drawing is not allowed while a pattern is generating."));
         mainptr->Stop();
         mainptr->draw_pending = true;
         mainptr->mouseevent.m_x = x;
         mainptr->mouseevent.m_y = y;
         return;
      }
      StartDrawingCells(x, y);

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
      
      #ifdef __WXX11__
         // control-click is detected here rather than in OnRMouseDown
         if ( event.ControlDown() ) {
            ProcessControlClick(event.GetX(), event.GetY());
            return;
         }
      #endif
      
      ProcessClick(event.GetX(), event.GetY(), event.ShiftDown());
      mainptr->UpdateUserInterface(mainptr->IsActive());
   }
}

// -----------------------------------------------------------------------------

void PatternView::OnMouseUp(wxMouseEvent& WXUNUSED(event))
{
   if (drawingcells || selectingcells || movingview) {
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
   if (drawingcells || selectingcells || movingview) {
      StopDraggingMouse();
   }
}

#endif

// -----------------------------------------------------------------------------

void PatternView::OnRMouseDown(wxMouseEvent& event)
{
   // this is equivalent to control-click in wxMac/wxMSW but not in wxX11 -- sigh
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
   
   ProcessControlClick(event.GetX(), event.GetY());
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
         wxBell();
         break;      // best not to beep lots of times
      }
   }

   // allow mouse interaction if script is running
   bool saveinscript = inscript;
   inscript = false;
   mainptr->UpdateEverything();
   inscript = saveinscript;
}

// -----------------------------------------------------------------------------

void PatternView::OnMouseMotion(wxMouseEvent& WXUNUSED(event))
{
   statusptr->CheckMouseLocation(mainptr->IsActive());
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
   #ifdef __WXX11__
      // make sure viewport keeps keyboard focus
      if ( mainptr->IsActive() ) SetFocus();
   #endif
}

// -----------------------------------------------------------------------------

void PatternView::OnDragTimer(wxTimerEvent& WXUNUSED(event))
{
   // called periodically while drawing/selecting/moving
   wxPoint pt = ScreenToClient( wxGetMousePosition() );
   int x = pt.x;
   int y = pt.y;
   // don't test "!PointInView(x, y)" here -- we want to allow scrolling
   // in full screen mode when mouse is at outer edge of view
   if ( x <= 0 || x >= currlayer->view->getxmax() ||
        y <= 0 || y >= currlayer->view->getymax() ) {
      
      // user can disable scrolling
      if ( drawingcells && !scrollpencil ) {
         if (x < 0) x = 0;
         if (y < 0) y = 0;
         if (x > currlayer->view->getxmax()) x = currlayer->view->getxmax();
         if (y > currlayer->view->getymax()) y = currlayer->view->getymax();
         DrawCells(x, y);
         return;
      }
      if ( selectingcells && !scrollcross ) {
         if (x < 0) x = 0;
         if (y < 0) y = 0;
         if (x > currlayer->view->getxmax()) x = currlayer->view->getxmax();
         if (y > currlayer->view->getymax()) y = currlayer->view->getymax();
         SelectCells(x, y);
         return;
      }
      if ( movingview && !scrollhand ) {
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
         if (forceh || forcev) {
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
   }

   if ( drawingcells ) {
      // only draw cells within view
      if (x < 0) x = 0;
      if (y < 0) y = 0;
      if (x > currlayer->view->getxmax()) x = currlayer->view->getxmax();
      if (y > currlayer->view->getymax()) y = currlayer->view->getymax();
      DrawCells(x, y);

   } else if ( selectingcells ) {
      // only select cells within view
      if (x < 0) x = 0;
      if (y < 0) y = 0;
      if (x > currlayer->view->getxmax()) x = currlayer->view->getxmax();
      if (y > currlayer->view->getymax()) y = currlayer->view->getymax();
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
            // don't Update() immediately -- more responsive, especially on X11
         } else {
            vthumb = newpos;
            currlayer->view->move(0, amount);
            // don't call UpdateEverything here because it calls UpdateScrollBars
            RefreshView();
            // don't Update() immediately -- more responsive, especially on X11
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
   
   // avoid erasing background on GTK+
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
   oldzoom = NULL;            // not shift zooming
}

// -----------------------------------------------------------------------------

// destroy the viewport window
PatternView::~PatternView()
{
   delete dragtimer;
   delete viewbitmap;
   delete cellbrush;
}
