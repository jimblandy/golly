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

#include "wx/dcbuffer.h"   // for wxBufferedPaintDC
#include "wx/file.h"       // for wxFile

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "viewport.h"

#include "wxgolly.h"       // for wxGetApp, mainptr, statusptr
#include "wxutils.h"       // for Warning, Fatal, FillRect
#include "wxprefs.h"       // for hashing, etc
#include "wxhelp.h"        // for ShowHelp
#include "wxmain.h"        // for mainptr->...
#include "wxstatus.h"      // for statusptr->...
#include "wxrender.h"      // for DrawView, DrawSelection, CreatePasteImage
#include "wxview.h"

#ifdef __WXMAC__
   #include <Carbon/Carbon.h>    // for *AppModalStateForWindow, Button
#endif

// -----------------------------------------------------------------------------

// create a viewport for displaying patterns;
// the initial size is not important -- SetViewSize will change it
viewport currview(10, 10);

// bitmap for wxBufferedPaintDC
#ifndef __WXMAC__                // windows are automatically buffered on OS X
   wxBitmap *viewbitmap = NULL;  // viewport bitmap for OnPaint
   int viewbitmapwd = -1;        // width of viewport bitmap
   int viewbitmapht = -1;        // height of viewport bitmap
#endif

// call OnDragTimer 50 times per sec
const int DRAG_RATE = 20;
const int ID_DRAG_TIMER = 1000;

// -----------------------------------------------------------------------------

// display functions:

void PatternView::NoSelection()
{
   // set seltop > selbottom
   seltop = 1;
   selbottom = 0;
}

bool PatternView::SelectionExists()
{
   return (seltop <= selbottom) == 1;     // avoid VC++ warning
}

bool PatternView::SelectionVisible(wxRect *visrect)
{
   if (!SelectionExists()) return false;

   pair<int,int> lt = currview.screenPosOf(selleft, seltop, curralgo);
   pair<int,int> rb = currview.screenPosOf(selright, selbottom, curralgo);

   if (lt.first > currview.getxmax() || rb.first < 0 ||
       lt.second > currview.getymax() || rb.second < 0) {
      // no part of selection is visible
      return false;
   }

   // all or some of selection is visible in viewport;
   // only set visible rectangle if requested
   if (visrect) {
      if (lt.first < 0) lt.first = 0;
      if (lt.second < 0) lt.second = 0;
      // correct for mag if needed
      if (currview.getmag() > 0) {
         rb.first += (1 << currview.getmag()) - 1;
         rb.second += (1 << currview.getmag()) - 1;
         if (currview.getmag() > 1) {
            // avoid covering gaps
            rb.first--;
            rb.second--;
         }
      }
      if (rb.first > currview.getxmax()) rb.first = currview.getxmax();
      if (rb.second > currview.getymax()) rb.second = currview.getymax();
      visrect->SetX(lt.first);
      visrect->SetY(lt.second);
      visrect->SetWidth(rb.first - lt.first + 1);
      visrect->SetHeight(rb.second - lt.second + 1);
   }
   return true;
}

bool PatternView::GridVisible()
{
   return ( showgridlines && currview.getmag() >= mingridmag );
}

// -----------------------------------------------------------------------------

// most editing and saving operations are limited to abs coords <= 10^9
// because getcell/setcell take int parameters (the limits must be smaller
// than INT_MIN and INT_MAX to avoid boundary conditions)
static bigint min_coord = -1000000000;
static bigint max_coord = +1000000000;

bool PatternView::OutsideLimits(bigint &t, bigint &l, bigint &b, bigint &r)
{
   return ( t < min_coord || l < min_coord || b > max_coord || r > max_coord );
}

// -----------------------------------------------------------------------------

// editing functions:

void PatternView::ShowDrawing()
{
   curralgo->endofpattern();
   mainptr->savestart = true;
   // update status bar
   if (mainptr->StatusVisible()) {
      statusptr->Refresh(false, NULL);
   }
}

void PatternView::DrawOneCell(int cx, int cy, wxDC &dc)
{
   int cellsize = 1 << currview.getmag();

   // convert given cell coords to view coords
   pair<bigint, bigint> lefttop = currview.at(0, 0);
   wxCoord x = (cx - lefttop.first.toint()) * cellsize;
   wxCoord y = (cy - lefttop.second.toint()) * cellsize;
   
   if (cellsize > 2) {
      cellsize--;       // allow for gap between cells
   }
   dc.DrawRectangle(x, y, cellsize, cellsize);
   
   // overlay selection image if cell is within selection
   if ( SelectionExists() &&
        cx >= selleft.toint() && cx <= selright.toint() &&
        cy >= seltop.toint() && cy <= selbottom.toint() ) {
      wxRect r = wxRect(x, y, cellsize, cellsize);
      DrawSelection(dc, r);
   }
}

void PatternView::StartDrawingCells(int x, int y)
{
   pair<bigint, bigint> cellpos = currview.at(x, y);
   // check that cellpos is within getcell/setcell limits
   if ( OutsideLimits(cellpos.second, cellpos.first, cellpos.second, cellpos.first) ) {
      statusptr->ErrorMessage("Drawing is not allowed outside +/- 10^9 boundary.");
      return;
   }

   cellx = cellpos.first.toint();
   celly = cellpos.second.toint();
   drawstate = 1 - curralgo->getcell(cellx, celly);
   curralgo->setcell(cellx, celly, drawstate);

   wxClientDC dc(this);
   dc.BeginDrawing();
   dc.SetPen(*wxTRANSPARENT_PEN);
   dc.SetBrush(drawstate == (int)blackcells ? *wxBLACK_BRUSH : *wxWHITE_BRUSH);
   DrawOneCell(cellx, celly, dc);
   dc.SetBrush(wxNullBrush);        // restore brush
   dc.SetPen(wxNullPen);            // restore pen
   dc.EndDrawing();
   
   ShowDrawing();

   drawingcells = true;
   CaptureMouse();                  // get mouse up event even if outside view
   dragtimer->Start(DRAG_RATE);
}

void PatternView::DrawCells(int x, int y)
{
   pair<bigint, bigint> cellpos = currview.at(x, y);
   if ( currview.getmag() < 0 ||
        OutsideLimits(cellpos.second, cellpos.first, cellpos.second, cellpos.first) ) {
      return;
   }

   int newx = cellpos.first.toint();
   int newy = cellpos.second.toint();
   if ( newx != cellx || newy != celly ) {
      wxClientDC dc(this);
      dc.BeginDrawing();
      dc.SetPen(*wxTRANSPARENT_PEN);
      dc.SetBrush(drawstate == (int)blackcells ? *wxBLACK_BRUSH : *wxWHITE_BRUSH);

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
      
      if (ai > aj) {
         d = aj - (ai >> 1);
         while (ii != newx) {
            if ( curralgo->getcell(ii, jj) != drawstate) {
               curralgo->setcell(ii, jj, drawstate);
               numchanged++;
               DrawOneCell(ii, jj, dc);
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
            if ( curralgo->getcell(ii, jj) != drawstate) {
               curralgo->setcell(ii, jj, drawstate);
               numchanged++;
               DrawOneCell(ii, jj, dc);
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
      
      if ( curralgo->getcell(cellx, celly) != drawstate) {
         curralgo->setcell(cellx, celly, drawstate);
         numchanged++;
         DrawOneCell(cellx, celly, dc);
      }
      
      dc.SetBrush(wxNullBrush);     // restore brush
      dc.SetPen(wxNullPen);         // restore pen
      dc.EndDrawing();
      
      if ( numchanged > 0 ) ShowDrawing();
   }
}

void PatternView::ModifySelection(bigint &xclick, bigint &yclick)
{
   // note that we include "=" in following tests to get sensible
   // results when modifying small selections (ht or wd <= 3)
   if ( yclick <= seltop && xclick <= selleft ) {
      // click is in or outside top left corner
      seltop = yclick;
      selleft = xclick;
      anchory = selbottom;
      anchorx = selright;

   } else if ( yclick <= seltop && xclick >= selright ) {
      // click is in or outside top right corner
      seltop = yclick;
      selright = xclick;
      anchory = selbottom;
      anchorx = selleft;

   } else if ( yclick >= selbottom && xclick >= selright ) {
      // click is in or outside bottom right corner
      selbottom = yclick;
      selright = xclick;
      anchory = seltop;
      anchorx = selleft;

   } else if ( yclick >= selbottom && xclick <= selleft ) {
      // click is in or outside bottom left corner
      selbottom = yclick;
      selleft = xclick;
      anchory = seltop;
      anchorx = selright;
   
   } else if (yclick <= seltop) {
      // click is in or above top edge
      forcev = true;
      seltop = yclick;
      anchory = selbottom;
   
   } else if (yclick >= selbottom) {
      // click is in or below bottom edge
      forcev = true;
      selbottom = yclick;
      anchory = seltop;
   
   } else if (xclick <= selleft) {
      // click is in or left of left edge
      forceh = true;
      selleft = xclick;
      anchorx = selright;
   
   } else if (xclick >= selright) {
      // click is in or right of right edge
      forceh = true;
      selright = xclick;
      anchorx = selleft;
   
   } else {
      // click is somewhere inside selection
      double wd = selright.todouble() - selleft.todouble() + 1.0;
      double ht = selbottom.todouble() - seltop.todouble() + 1.0;
      double onethirdx = selleft.todouble() + wd / 3.0;
      double twothirdx = selleft.todouble() + wd * 2.0 / 3.0;
      double onethirdy = seltop.todouble() + ht / 3.0;
      double twothirdy = seltop.todouble() + ht * 2.0 / 3.0;
      double midy = seltop.todouble() + ht / 2.0;
      double x = xclick.todouble();
      double y = yclick.todouble();
      
      if ( y < onethirdy && x < onethirdx ) {
         // click is near top left corner
         seltop = yclick;
         selleft = xclick;
         anchory = selbottom;
         anchorx = selright;
      
      } else if ( y < onethirdy && x > twothirdx ) {
         // click is near top right corner
         seltop = yclick;
         selright = xclick;
         anchory = selbottom;
         anchorx = selleft;
   
      } else if ( y > twothirdy && x > twothirdx ) {
         // click is near bottom right corner
         selbottom = yclick;
         selright = xclick;
         anchory = seltop;
         anchorx = selleft;
   
      } else if ( y > twothirdy && x < onethirdx ) {
         // click is near bottom left corner
         selbottom = yclick;
         selleft = xclick;
         anchory = seltop;
         anchorx = selright;

      } else if ( x < onethirdx ) {
         // click is near middle of left edge
         forceh = true;
         selleft = xclick;
         anchorx = selright;

      } else if ( x > twothirdx ) {
         // click is near middle of right edge
         forceh = true;
         selright = xclick;
         anchorx = selleft;

      } else if ( y < midy ) {
         // click is below middle section of top edge
         forcev = true;
         seltop = yclick;
         anchory = selbottom;
      
      } else {
         // click is above middle section of bottom edge
         forcev = true;
         selbottom = yclick;
         anchory = seltop;
      }
   }
}

void PatternView::StartSelectingCells(int x, int y, bool shiftdown)
{
   pair<bigint, bigint> cellpos = currview.at(x, y);
   anchorx = cellpos.first;
   anchory = cellpos.second;

   // save original selection so it can be restored if user hits escape
   origtop = seltop;
   origbottom = selbottom;
   origleft = selleft;
   origright = selright;

   // set previous selection to anything impossible
   prevtop = 1;
   prevleft = 1;
   prevbottom = 0;
   prevright = 0;
   
   // for avoiding 1x1 selection if mouse doesn't move much
   initselx = x;
   initsely = y;
   
   // allow changing size in any direction
   forceh = false;
   forcev = false;
   
   if (SelectionExists()) {
      if (shiftdown) {
         // modify current selection
         ModifySelection(cellpos.first, cellpos.second);
         DisplaySelectionSize();
         mainptr->UpdatePatternAndStatus();
      } else {
         // remove current selection
         NoSelection();
         mainptr->UpdatePatternAndStatus();
      }
   }
   
   selectingcells = true;
   CaptureMouse();               // get mouse up event even if outside view
   dragtimer->Start(DRAG_RATE);
}

void PatternView::SelectCells(int x, int y)
{
   if ( abs(initselx - x) < 2 && abs(initsely - y) < 2 && !SelectionExists() ) {
      // avoid 1x1 selection if mouse hasn't moved much
      return;
   }

   pair<bigint, bigint> cellpos = currview.at(x, y);
   if (!forcev) {
      if (cellpos.first <= anchorx) {
         selleft = cellpos.first;
         selright = anchorx;
      } else {
         selleft = anchorx;
         selright = cellpos.first;
      }
   }
   if (!forceh) {
      if (cellpos.second <= anchory) {
         seltop = cellpos.second;
         selbottom = anchory;
      } else {
         seltop = anchory;
         selbottom = cellpos.second;
      }
   }

   if ( seltop != prevtop || selbottom != prevbottom ||
        selleft != prevleft || selright != prevright ) {
      // selection has changed
      DisplaySelectionSize();
      mainptr->UpdatePatternAndStatus();
      prevtop = seltop;
      prevbottom = selbottom;
      prevleft = selleft;
      prevright = selright;
   }
}

void PatternView::StartMovingView(int x, int y)
{
   pair<bigint, bigint> cellpos = currview.at(x, y);
   bigcellx = cellpos.first;
   bigcelly = cellpos.second;
   movingview = true;
   CaptureMouse();               // get mouse up event even if outside view
   dragtimer->Start(DRAG_RATE);
}

void PatternView::MoveView(int x, int y)
{
   pair<bigint, bigint> cellpos = currview.at(x, y);
   bigint newx = cellpos.first;
   bigint newy = cellpos.second;
   bigint xdelta = bigcellx;
   bigint ydelta = bigcelly;
   xdelta -= newx;
   ydelta -= newy;

   int xamount, yamount;
   int mag = currview.getmag();
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
      currview.move(xamount, yamount);
      mainptr->UpdatePatternAndStatus();
      cellpos = currview.at(x, y);
      bigcellx = cellpos.first;
      bigcelly = cellpos.second;
   }
}

void PatternView::StopDraggingMouse()
{
   if (selectingcells)
      mainptr->UpdateMenuItems(true);     // update Edit menu items
   drawingcells = false;
   selectingcells = false;
   movingview = false;
   if ( HasCapture() ) ReleaseMouse();
   if ( dragtimer->IsRunning() ) dragtimer->Stop();
}

void PatternView::RestoreSelection()
{
   seltop = origtop;
   selbottom = origbottom;
   selleft = origleft;
   selright = origright;
   StopDraggingMouse();
   mainptr->UpdatePatternAndStatus();
   statusptr->DisplayMessage("New selection aborted.");
}

void PatternView::TestAutoFit()
{
   if (autofit && mainptr->generating) {
      // assume user no longer wants us to do autofitting
      autofit = false;
   }
}

void PatternView::ZoomInPos(int x, int y)
{
   // zoom in so that clicked cell stays under cursor
   TestAutoFit();
   if (currview.getmag() < MAX_MAG) {
      currview.zoom(x, y);
      mainptr->UpdateEverything();
   } else {
      wxBell();   // can't zoom in any further
   }
}

void PatternView::ZoomOutPos(int x, int y)
{
   // zoom out so that clicked cell stays under cursor
   TestAutoFit();
   currview.unzoom(x, y);
   mainptr->UpdateEverything();
}

void PatternView::ProcessClick(int x, int y, bool shiftdown)
{
   // user has clicked somewhere in viewport
   if (currcurs == curs_pencil) {
      if (mainptr->generating) {
         statusptr->ErrorMessage("Drawing is not allowed while generating.");
         return;
      }
      if (currview.getmag() < 0) {
         statusptr->ErrorMessage("Drawing is not allowed if more than 1 cell per pixel.");
         return;
      }
      StartDrawingCells(x, y);

   } else if (currcurs == curs_cross) {
      TestAutoFit();
      StartSelectingCells(x, y, shiftdown);

   } else if (currcurs == curs_hand) {
      TestAutoFit();
      StartMovingView(x, y);

   } else if (currcurs == curs_zoomin) {
      ZoomInPos(x, y);

   } else if (currcurs == curs_zoomout) {
      ZoomOutPos(x, y);
   }
}

// -----------------------------------------------------------------------------

// more editing functions:

void PatternView::EmptyUniverse()
{
   // kill all live cells in current universe
   int savewarp = mainptr->GetWarp();
   int savemag = currview.getmag();
   bigint savex = currview.x;
   bigint savey = currview.y;
   bigint savegen = curralgo->getGeneration();
   mainptr->CreateUniverse();
   // restore various settings
   mainptr->SetWarp(savewarp);
   mainptr->SetGenIncrement();
   currview.setpositionmag(savex, savey, savemag);
   curralgo->setGeneration(savegen);
   mainptr->UpdatePatternAndStatus();
}

bool PatternView::CopyRect(int itop, int ileft, int ibottom, int iright,
                           lifealgo *srcalgo, lifealgo *destalgo,
                           bool erasesrc, const char *progmsg)
{
   int wd = iright - ileft + 1;
   int ht = ibottom - itop + 1;
   int cx, cy;
   double maxcount = (double)wd * (double)ht;
   int cntr = 0;
   bool abort = false;
   
   // copy (and erase if requested) live cells from given rect
   // in source universe to same rect in destination universe
   BeginProgress(progmsg);
   for ( cy=itop; cy<=ibottom; cy++ ) {
      for ( cx=ileft; cx<=iright; cx++ ) {
         int skip = srcalgo->nextcell(cx, cy);
         if (skip + cx > iright)
            skip = -1;           // pretend we found no more live cells
         if (skip >= 0) {
            // found next live cell
            cx += skip;
            destalgo->setcell(cx, cy, 1);
            if (erasesrc) srcalgo->setcell(cx, cy, 0);
         } else {
            cx = iright + 1;     // done this row
         }
         cntr++;
         if ((cntr % 4096) == 0) {
            double prog = ((cy - itop) * (double)(iright - ileft + 1) +
                           (cx - ileft)) / maxcount;
            abort = AbortProgress(prog, "");
            if (abort) break;
         }
      }
      if (abort) break;
   }
   destalgo->endofpattern();
   EndProgress();
   
   return !abort;
}

void PatternView::CopyAllRect(int itop, int ileft, int ibottom, int iright,
                              lifealgo *srcalgo, lifealgo *destalgo,
                              const char *progmsg)
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
            abort = AbortProgress((double)cntr / maxcount, "");
            if (abort) break;
         }
      }
      if (abort) break;
   }
   destalgo->endofpattern();
   EndProgress();
}

void PatternView::ClearSelection()
{
   if (mainptr->generating || !SelectionExists()) return;
   
   // no need to do anything if there is no pattern
   if (curralgo->isEmpty()) return;
   
   bigint top, left, bottom, right;
   curralgo->findedges(&top, &left, &bottom, &right);
   if ( seltop <= top && selbottom >= bottom &&
        selleft <= left && selright >= right ) {
      // selection encloses entire pattern so just create empty universe
      EmptyUniverse();
      return;
   }

   // no need to do anything if selection is completely outside pattern edges
   if ( seltop > bottom || selbottom < top ||
        selleft > right || selright < left ) {
      return;
   }
   
   // find intersection of selection and pattern to minimize work
   if (seltop > top) top = seltop;
   if (selleft > left) left = selleft;
   if (selbottom < bottom) bottom = selbottom;
   if (selright < right) right = selright;

   // can only use setcell in limited domain
   if ( OutsideLimits(top, left, bottom, right) ) {
      statusptr->ErrorMessage(selection_too_big);
      return;
   }

   // clear all live cells in selection
   int itop = top.toint();
   int ileft = left.toint();
   int ibottom = bottom.toint();
   int iright = right.toint();
   int wd = iright - ileft + 1;
   int ht = ibottom - itop + 1;
   int cx, cy;
   double maxcount = (double)wd * (double)ht;
   int cntr = 0;
   bool abort = false;
   BeginProgress("Clearing selection");
   for ( cy=itop; cy<=ibottom; cy++ ) {
      for ( cx=ileft; cx<=iright; cx++ ) {
         int skip = curralgo->nextcell(cx, cy);
         if (skip + cx > iright)
            skip = -1;           // pretend we found no more live cells
         if (skip >= 0) {
            // found next live cell
            cx += skip;
            curralgo->setcell(cx, cy, 0);
         } else {
            cx = iright + 1;     // done this row
         }
         cntr++;
         if ((cntr % 4096) == 0) {
            double prog = ((cy - itop) * (double)(iright - ileft + 1) +
                           (cx - ileft)) / maxcount;
            abort = AbortProgress(prog, "");
            if (abort) break;
         }
      }
      if (abort) break;
   }
   curralgo->endofpattern();
   mainptr->savestart = true;
   EndProgress();
   
   mainptr->UpdatePatternAndStatus();
}

void PatternView::ClearOutsideSelection()
{
   if (mainptr->generating || !SelectionExists()) return;
   
   // no need to do anything if there is no pattern
   if (curralgo->isEmpty()) return;
   
   // no need to do anything if selection encloses entire pattern
   bigint top, left, bottom, right;
   curralgo->findedges(&top, &left, &bottom, &right);
   if ( seltop <= top && selbottom >= bottom &&
        selleft <= left && selright >= right ) {
      return;
   }

   // create empty universe if selection is completely outside pattern edges
   if ( seltop > bottom || selbottom < top ||
        selleft > right || selright < left ) {
      EmptyUniverse();
      return;
   }
   
   // find intersection of selection and pattern to minimize work
   if (seltop > top) top = seltop;
   if (selleft > left) left = selleft;
   if (selbottom < bottom) bottom = selbottom;
   if (selright < right) right = selright;

   // check that selection is small enough to save
   if ( OutsideLimits(top, left, bottom, right) ) {
      statusptr->ErrorMessage(selection_too_big);
      return;
   }
   
   // create a new universe
   lifealgo *newalgo;
   if ( hashing ) {
      newalgo = new hlifealgo();
      newalgo->setMaxMemory(maxhashmem);
   } else {
      newalgo = new qlifealgo();
   }
   newalgo->setpoll(wxGetApp().Poller());

   // set same gen count
   newalgo->setGeneration( curralgo->getGeneration() );
   
   // copy live cells in selection to new universe
   if ( CopyRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                 curralgo, newalgo, false, "Saving selection") ) {
      // delete old universe and point curralgo at new universe
      mainptr->savestart = true;
      delete curralgo;
      curralgo = newalgo;
      mainptr->SetGenIncrement();
      mainptr->UpdatePatternAndStatus();
   } else {
      // aborted, so don't change current universe
      delete newalgo;
   }
}

const unsigned int maxrleline = 70;    // max line length for RLE data

void AppendEOL(char **chptr)
{
   #ifdef __WXMAC__
      **chptr = '\r';      // nicer for stupid apps like LifeLab :)
      *chptr += 1;
   #elif defined(__WXMSW__)
      **chptr = '\r';
      *chptr += 1;
      **chptr = '\n';
      *chptr += 1;
   #else // assume Unix
      **chptr = '\n';
      *chptr += 1;
   #endif
}

void AddRun (char ch,
             unsigned int *run,        // in and out
             unsigned int *linelen,    // ditto
             char **chptr)             // ditto
{
   // output of RLE pattern data is channelled thru here to make it easier to
   // ensure all lines have <= maxrleline characters
   unsigned int i, numlen;
   char numstr[32];
   
   if ( *run > 1 ) {
      sprintf(numstr, "%u", *run);
      numlen = strlen(numstr);
   } else {
      numlen = 0;                      // no run count shown if 1
   }
   // keep linelen <= maxrleline
   if ( *linelen + numlen + 1 > maxrleline ) {
      AppendEOL(chptr);
      *linelen = 0;
   }
   i = 0;
   while (i < numlen) {
      **chptr = numstr[i];
      *chptr += 1;
      i++;
   }
   **chptr = ch;
   *chptr += 1;
   *linelen += numlen + 1;
   *run = 0;                           // reset run count
}

void PatternView::CopySelectionToClipboard(bool cut)
{
   // can only use getcell/setcell in limited domain
   if ( OutsideLimits(seltop, selbottom, selleft, selright) ) {
      statusptr->ErrorMessage(selection_too_big);
      return;
   }
   
   int itop = seltop.toint();
   int ileft = selleft.toint();
   int ibottom = selbottom.toint();
   int iright = selright.toint();
   unsigned int wd = iright - ileft + 1;
   unsigned int ht = ibottom - itop + 1;

   // convert cells in selection to RLE data in textptr
   char *textptr;
   char *etextptr;
   int cursize = 4096;
   
   textptr = (char *)malloc(cursize);
   if (textptr == NULL) {
      statusptr->ErrorMessage("Not enough memory for clipboard data!");
      return;
   }
   etextptr = textptr + cursize;

   // add RLE header line
   sprintf(textptr, "x = %u, y = %u, rule = %s", wd, ht, curralgo->getrule());
   char *chptr = textptr;
   chptr += strlen(textptr);
   AppendEOL(&chptr);
   // save start of data in case livecount is zero
   int datastart = chptr - textptr;
   
   // add RLE pattern data
   unsigned int livecount = 0;
   unsigned int linelen = 0;
   unsigned int brun = 0;
   unsigned int orun = 0;
   unsigned int dollrun = 0;
   char lastchar;
   int cx, cy;
   
   double maxcount = (double)wd * (double)ht;
   int cntr = 0;
   bool abort = false;
   if (cut)
      BeginProgress("Cutting selection");
   else
      BeginProgress("Copying selection");

   for ( cy=itop; cy<=ibottom; cy++ ) {
      // set lastchar to anything except 'o' or 'b'
      lastchar = 0;
      for ( cx=ileft; cx<=iright; cx++ ) {
         int skip = curralgo->nextcell(cx, cy);
         if (skip + cx > iright)
            skip = -1;           // pretend we found no more live cells
         if (skip > 0) {
            // have exactly "skip" empty cells here
            if (lastchar == 'b') {
               brun += skip;
            } else {
               if (orun > 0) {
                  // output current run of live cells
                  AddRun('o', &orun, &linelen, &chptr);
               }
               lastchar = 'b';
               brun = skip;
            }
         }
         if (skip >= 0) {
            // found next live cell
            cx += skip;
            livecount++;
            if (cut) curralgo->setcell(cx, cy, 0);
            if (lastchar == 'o') {
               orun++;
            } else {
               if (dollrun > 0) {
                  // output current run of $ chars
                  AddRun('$', &dollrun, &linelen, &chptr);
               }
               if (brun > 0) {
                  // output current run of dead cells
                  AddRun('b', &brun, &linelen, &chptr);
               }
               lastchar = 'o';
               orun = 1;
            }
         } else {
            cx = iright + 1;     // done this row
         }
         cntr++;
         if ((cntr % 4096) == 0) {
            double prog = ((cy - itop) * (double)(iright - ileft + 1) +
                           (cx - ileft)) / maxcount;
            abort = AbortProgress(prog, "");
            if (abort) break;
         }
         if (chptr + 60 >= etextptr) {
            // nearly out of space; try to increase allocation
            char *ntxtptr = (char *)realloc(textptr, 2*cursize);
            if (ntxtptr == 0) {
               statusptr->ErrorMessage("No more memory for clipboard data!");
               // don't return here -- best to set abort flag and break so that
               // partially cut/copied portion gets saved to clipboard
               abort = true;
               break;
            }
            chptr = ntxtptr + (chptr - textptr);
            cursize *= 2;
            etextptr = ntxtptr + cursize;
            textptr = ntxtptr;
         }
      }
      if (abort) break;
      // end of current row
      if (lastchar == 'b') {
         // forget dead cells at end of row
         brun = 0;
      } else if (lastchar == 'o') {
         // output current run of live cells
         AddRun('o', &orun, &linelen, &chptr);
      }
      dollrun++;
   }
   
   if (livecount == 0) {
      // no live cells in selection so simplify RLE data to "!"
      chptr = textptr + datastart;
      *chptr = '!';
      chptr++;
   } else {
      // terminate RLE data
      dollrun = 1;
      AddRun('!', &dollrun, &linelen, &chptr);
      if (cut) {
         curralgo->endofpattern();
         mainptr->savestart = true;
      }
   }
   AppendEOL(&chptr);
   *chptr = 0;
   
   EndProgress();
   
   if (cut && livecount > 0)
      mainptr->UpdatePatternAndStatus();
   
   wxString text(textptr);
   mainptr->CopyTextToClipboard(text);
   free(textptr);
}

void PatternView::CutSelection()
{
   if (mainptr->generating || !SelectionExists()) return;
   CopySelectionToClipboard(true);
}

void PatternView::CopySelection()
{
   if (mainptr->generating || !SelectionExists()) return;
   CopySelectionToClipboard(false);
}

void PatternView::EnableAllMenus(bool enable)
{
   #ifdef __WXMAC__
      // enable/disable all menus, including Help menu and items in app menu
      if (enable)
         EndAppModalStateForWindow( (OpaqueWindowPtr*)mainptr->MacGetWindowRef() );
      else
         BeginAppModalStateForWindow( (OpaqueWindowPtr*)mainptr->MacGetWindowRef() );
   #else
      wxMenuBar *mbar = mainptr->GetMenuBar();
      if (mbar) {
         int count = mbar->GetMenuCount();
         int i;
         for (i = 0; i<count; i++) {
            mbar->EnableTop(i, enable);
         }
      }
   #endif
}

void PatternView::SetPasteRect(wxRect &rect, bigint &wd, bigint &ht)
{
   int x, y, pastewd, pasteht;
   int mag = currview.getmag();
   
   // find cell coord of current paste cursor position
   pair<bigint, bigint> pcell = currview.at(pastex, pastey);

   // determine bottom right cell
   bigint right = pcell.first;     right += wd;    right -= 1;
   bigint bottom = pcell.second;   bottom += ht;   bottom -= 1;
   
   // best to use same method as in SelectionVisible
   pair<int,int> lt = currview.screenPosOf(pcell.first, pcell.second, curralgo);
   pair<int,int> rb = currview.screenPosOf(right, bottom, curralgo);

   // correct for mag if needed
   if (mag > 0) {
      rb.first += (1 << mag) - 1;
      rb.second += (1 << mag) - 1;
      if (mag > 1) {
         // avoid covering gaps
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
   int cellsize = 1 << mag;
   int xoffset, yoffset;
   switch (plocation) {
      case TopLeft:
         break;
      case TopRight:
         xoffset = mag > 0 ? -(pastewd - cellsize + 1) : -pastewd + 1;
         rect.Offset(xoffset, 0);
         break;
      case BottomRight:
         xoffset = mag > 0 ? -(pastewd - cellsize + 1) : -pastewd + 1;
         yoffset = mag > 0 ? -(pasteht - cellsize + 1) : -pasteht + 1;
         rect.Offset(xoffset, yoffset);
         break;
      case BottomLeft:
         yoffset = mag > 0 ? -(pasteht - cellsize + 1) : -pasteht + 1;
         rect.Offset(0, yoffset);
         break;
      case Middle:
         xoffset = mag > 0 ? -(pastewd / cellsize / 2) * cellsize : -pastewd / 2;
         yoffset = mag > 0 ? -(pasteht / cellsize / 2) * cellsize : -pasteht / 2;
         rect.Offset(xoffset, yoffset);
         break;
   }
}

void PatternView::PasteTemporaryToCurrent(lifealgo *tempalgo, bool toselection,
                             bigint top, bigint left, bigint bottom, bigint right)
{
   // make sure given edges are within getcell/setcell limits
   if ( OutsideLimits(top, left, bottom, right) ) {
      statusptr->ErrorMessage("Clipboard pattern is too big.");
      return;
   }
   int itop = top.toint();
   int ileft = left.toint();
   int ibottom = bottom.toint();
   int iright = right.toint();
   bigint ht = ibottom - itop + 1;
   bigint wd = iright - ileft + 1;
   
   if ( toselection ) {
      bigint selht = selbottom;  selht -= seltop;   selht += 1;
      bigint selwd = selright;   selwd -= selleft;  selwd += 1;
      if ( ht > selht || wd > selwd ) {
         statusptr->ErrorMessage("Clipboard pattern is bigger than selection.");
         return;
      }

      // set paste rectangle's top left cell coord
      top = seltop;
      left = selleft;

   } else {

      // ask user where to paste the clipboard pattern
      statusptr->DisplayMessage("Click where you want to paste...");

      // temporarily change cursor to cross
      wxCursor *savecurs = currcurs;
      currcurs = curs_cross;
      // CheckCursor(true);            // probs on Mac if Paste menu item selected
      wxSetCursor(*currcurs);
      SetCursor(*currcurs);

      // create image for drawing pattern to be pasted; note that given box
      // is not necessarily the minimal bounding box because clipboard pattern
      // might have blank borders (in fact it could be empty)
      wxRect bbox = wxRect(ileft, itop, wd.toint(), ht.toint());
      CreatePasteImage(tempalgo, bbox);

      waitingforclick = true;
      EnableAllMenus(false);           // disable all menu items
      mainptr->UpdateToolBar(false);   // disable all tool bar buttons
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
               Refresh(false, NULL);
               // don't update immediately
               // Update();
            }
         } else {
            // mouse outside viewport so erase old pasterect if necessary
            if ( pasterect.width > 0 ) {
               pasterect = wxRect(-1,-1,0,0);
               Refresh(false, NULL);
               // don't update immediately
               // Update();
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

      ReleaseMouse();
      EnableAllMenus(true);
      DestroyPasteImage();
   
      // restore cursor
      currcurs = savecurs;
      CheckCursor(mainptr->IsActive());
      
      if ( pasterect.width > 0 ) {
         // erase old pasterect
         Refresh(false, NULL);
         // no need to update immediately
         // Update();
      }
      
      if ( pastex < 0 || pastex > currview.getxmax() ||
           pastey < 0 || pastey > currview.getymax() ) {
         statusptr->DisplayMessage("Paste aborted.");
         return;
      }
      
      // set paste rectangle's top left cell coord
      pair<bigint, bigint> clickpos = currview.at(pastex, pastey);
      top = clickpos.second;
      left = clickpos.first;
      bigint halfht = ht;
      bigint halfwd = wd;
      halfht.div2();
      halfwd.div2();
      if (currview.getmag() > 1) {
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
      statusptr->ErrorMessage("Pasting is not allowed outside +/- 10^9 boundary.");
      return;
   }

   // set pastex,pastey to top left cell of paste rectangle
   pastex = left.toint();
   pastey = top.toint();

   // copy pattern from temporary universe to current universe
   int tx, ty, cx, cy;
   double maxcount = wd.todouble() * ht.todouble();
   int cntr = 0;
   bool abort = false;
   BeginProgress("Pasting pattern");
   
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
   
   if ( usenextcell ) {
      cy = pastey;
      for ( ty=itop; ty<=ibottom; ty++ ) {
         cx = pastex;
         for ( tx=ileft; tx<=iright; tx++ ) {
            int skip = tempalgo->nextcell(tx, ty);
            if (skip + tx > iright)
               skip = -1;           // pretend we found no more live cells
            if (skip >= 0) {
               // found next live cell so paste it into current universe
               tx += skip;
               cx += skip;
               curralgo->setcell(cx, cy, 1);
               cx++;
            } else {
               tx = iright + 1;     // done this row
            }
            cntr++;
            if ((cntr % 4096) == 0) {
               double prog = ((ty - itop) * (double)(iright - ileft + 1) +
                              (tx - ileft)) / maxcount;
               abort = AbortProgress(prog, "");
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
            switch (pmode) {
               case Copy:
                  curralgo->setcell(cx, cy, tempstate);
                  break;
               case Or:
                  // Or mode is done using above nextcell loop;
                  // we only include this case to avoid compiler warning
                  break;
               case Xor:
                  currstate = curralgo->getcell(cx, cy);
                  if (tempstate == currstate) {
                     if (currstate != 0) curralgo->setcell(cx, cy, 0);
                  } else {
                     if (currstate != 1) curralgo->setcell(cx, cy, 1);
                  }
                  break;
            }
            cx++;
            cntr++;
            if ( (cntr % 4096) == 0 ) {
               abort = AbortProgress((double)cntr / maxcount, "");
               if (abort) break;
            }
         }
         if (abort) break;
         cy++;
      }
   }

   curralgo->endofpattern();
   mainptr->savestart = true;
   EndProgress();
   
   // tidy up and display result
   statusptr->ClearMessage();
   mainptr->UpdatePatternAndStatus();
}

void PatternView::PasteClipboard(bool toselection)
{
   if (mainptr->generating || waitingforclick || !mainptr->ClipboardHasText()) return;
   if (toselection && !SelectionExists()) return;

#ifdef __WXX11__
   if ( wxFileExists(clipfile) ) {
#else
   wxTextDataObject data;
   if ( mainptr->GetTextFromClipboard(&data) ) {
      // copy clipboard data to temporary file so we can handle all formats
      // supported by readclipboard
      wxFile tmpfile(clipfile, wxFile::write);
      if ( !tmpfile.IsOpened() ) {
         statusptr->ErrorMessage("Could not create temporary file!");
         return;
      }
      tmpfile.Write( data.GetText() );
      tmpfile.Close();
#endif         
      // create a temporary universe for storing clipboard pattern
      lifealgo *tempalgo;
      tempalgo = new qlifealgo();      // qlife's setcell/getcell are faster
      tempalgo->setpoll(wxGetApp().Poller());

      // read clipboard pattern into temporary universe
      bigint top, left, bottom, right;
      const char *err = readclipboard(clipfile, *tempalgo, &top, &left, &bottom, &right);
      if (err && strcmp(err,cannotreadhash) == 0) {
         // clipboard contains macrocell data so we have to use hlife
         delete tempalgo;
         tempalgo = new hlifealgo();
         tempalgo->setpoll(wxGetApp().Poller());
         err = readclipboard(clipfile, *tempalgo, &top, &left, &bottom, &right);
      }
      if (err) {
         Warning(err);
      } else {
         PasteTemporaryToCurrent(tempalgo, toselection, top, left, bottom, right);
      }
      
      // delete temporary universe and clipboard file
      delete tempalgo;
      #ifdef __WXX11__
         // don't delete clipboard file
      #else
         wxRemoveFile(clipfile);
      #endif
   }
}

void PatternView::CyclePasteLocation()
{
   if (plocation == TopLeft) {
      plocation = TopRight;
      if (!waitingforclick) statusptr->DisplayMessage("Paste location is Top Right.");
   } else if (plocation == TopRight) {
      plocation = BottomRight;
      if (!waitingforclick) statusptr->DisplayMessage("Paste location is Bottom Right.");
   } else if (plocation == BottomRight) {
      plocation = BottomLeft;
      if (!waitingforclick) statusptr->DisplayMessage("Paste location is Bottom Left.");
   } else if (plocation == BottomLeft) {
      plocation = Middle;
      if (!waitingforclick) statusptr->DisplayMessage("Paste location is Middle.");
   } else {
      plocation = TopLeft;
      if (!waitingforclick) statusptr->DisplayMessage("Paste location is Top Left.");
   }
   if (waitingforclick) {
      // force redraw of paste rectangle if mouse is inside viewport
      pasterect = wxRect(-1,-1,0,0);
   }
}

void PatternView::CyclePasteMode()
{
   if (pmode == Copy) {
      pmode = Or;
      if (!waitingforclick) statusptr->DisplayMessage("Paste mode is Or.");
   } else if (pmode == Or) {
      pmode = Xor;
      if (!waitingforclick) statusptr->DisplayMessage("Paste mode is Xor.");
   } else {
      pmode = Copy;
      if (!waitingforclick) statusptr->DisplayMessage("Paste mode is Copy.");
   }
   if (waitingforclick) {
      // force redraw of paste rectangle if mouse is inside viewport
      pasterect = wxRect(-1,-1,0,0);
   }
}

void PatternView::DisplaySelectionSize()
{
   if (waitingforclick) return;
   bigint wd = selright;    wd -= selleft;   wd += bigint::one;
   bigint ht = selbottom;   ht -= seltop;    ht += bigint::one;
   char s[128];
   sprintf(s, "Selection wd x ht = %s x %s",
              statusptr->Stringify(wd), statusptr->Stringify(ht));
   statusptr->SetMessage(s);
}

void PatternView::SelectAll()
{
   if (SelectionExists()) {
      NoSelection();
      mainptr->UpdatePatternAndStatus();
   }

   if (curralgo->isEmpty()) {
      statusptr->ErrorMessage(empty_pattern);
      return;
   }
   
   curralgo->findedges(&seltop, &selleft, &selbottom, &selright);
   DisplaySelectionSize();
   mainptr->UpdatePatternAndStatus();
}

void PatternView::RemoveSelection()
{
   if (SelectionExists()) {
      NoSelection();
      mainptr->UpdatePatternAndStatus();
   }
}

void PatternView::FitSelection()
{
   if (!SelectionExists()) return;

   bigint newx = selright;
   newx -= selleft;
   newx += bigint::one;
   newx.div2();
   newx += selleft;

   bigint newy = selbottom;
   newy -= seltop;
   newy += bigint::one;
   newy.div2();
   newy += seltop;

   int mag = MAX_MAG;
   while (true) {
      currview.setpositionmag(newx, newy, mag);
      if ( currview.contains(selleft, seltop) &&
           currview.contains(selright, selbottom) )
         break;
      mag--;
   }
   
   TestAutoFit();
   mainptr->UpdateEverything();
}

void PatternView::ShrinkSelection(bool fit)
{
   if (!SelectionExists()) return;
   
   // check if there is no pattern
   if (curralgo->isEmpty()) {
      statusptr->ErrorMessage(empty_selection);
      if (fit) FitSelection();
      return;
   }
   
   // check if selection encloses entire pattern
   bigint top, left, bottom, right;
   curralgo->findedges(&top, &left, &bottom, &right);
   if ( seltop <= top && selbottom >= bottom &&
        selleft <= left && selright >= right ) {
      // shrink edges
      seltop = top;
      selbottom = bottom;
      selleft = left;
      selright = right;
      DisplaySelectionSize();
      if (fit)
         FitSelection();   // calls UpdateEverything
      else
         mainptr->UpdatePatternAndStatus();
      return;
   }

   // check if selection is completely outside pattern edges
   if ( seltop > bottom || selbottom < top ||
        selleft > right || selright < left ) {
      statusptr->ErrorMessage(empty_selection);
      if (fit) FitSelection();
      return;
   }
   
   // find intersection of selection and pattern to minimize work
   if (seltop > top) top = seltop;
   if (selleft > left) left = selleft;
   if (selbottom < bottom) bottom = selbottom;
   if (selright < right) right = selright;

   // check that selection is small enough to save
   if ( OutsideLimits(top, left, bottom, right) ) {
      statusptr->ErrorMessage(selection_too_big);
      if (fit) FitSelection();
      return;
   }
   
   // the easy way to shrink selection is to create a new temporary universe,
   // copy selection into new universe and then call findedges;
   // a faster way would be to scan selection from top to bottom until first
   // live cell found, then from bottom to top, left to right and right to left!!!
   lifealgo *tempalgo;
   tempalgo = new qlifealgo();         // qlife's findedges is faster
   tempalgo->setpoll(wxGetApp().Poller());
   
   // copy live cells in selection to temporary universe
   if ( CopyRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                 curralgo, tempalgo, false, "Saving selection") ) {
      if ( tempalgo->isEmpty() ) {
         statusptr->ErrorMessage(empty_selection);
      } else {
         tempalgo->findedges(&seltop, &selleft, &selbottom, &selright);
         DisplaySelectionSize();
         if (!fit) mainptr->UpdatePatternAndStatus();
      }
   }
   
   delete tempalgo;
   if (fit) FitSelection();
}

void PatternView::RandomFill()
{
   if (mainptr->generating || !SelectionExists()) return;

   // can only use getcell/setcell in limited domain
   if ( OutsideLimits(seltop, selbottom, selleft, selright) ) {
      statusptr->ErrorMessage(selection_too_big);
      return;
   }
   
   int itop = seltop.toint();
   int ileft = selleft.toint();
   int ibottom = selbottom.toint();
   int iright = selright.toint();
   int wd = iright - ileft + 1;
   int ht = ibottom - itop + 1;
   double maxcount = (double)wd * (double)ht;
   int cntr = 0;
   bool abort = false;
   BeginProgress("Randomly filling selection");
   int cx, cy;
   for ( cy=itop; cy<=ibottom; cy++ ) {
      for ( cx=ileft; cx<=iright; cx++ ) {
         // randomfill is from 1..100
         curralgo->setcell(cx, cy, (rand() % 100) < randomfill);
         cntr++;
         if ((cntr % 4096) == 0) {
            abort = AbortProgress((double)cntr / maxcount, "");
            if (abort) break;
         }
      }
      if (abort) break;
   }
   curralgo->endofpattern();
   mainptr->savestart = true;
   EndProgress();
   mainptr->UpdatePatternAndStatus();
}

void PatternView::FlipVertically()
{
   if (mainptr->generating || !SelectionExists()) return;

   // can only use getcell/setcell in limited domain
   if ( OutsideLimits(seltop, selbottom, selleft, selright) ) {
      statusptr->ErrorMessage(selection_too_big);
      return;
   }
   
   int itop = seltop.toint();
   int ileft = selleft.toint();
   int ibottom = selbottom.toint();
   int iright = selright.toint();
   int wd = iright - ileft + 1;
   int ht = ibottom - itop + 1;
   
   if (wd == 1) return;
   
   // following code can be optimized by using faster nextcell calls!!!
   // ie. if entire pattern is selected then flip into new universe and
   // switch to that universe
   
   double maxcount = (double)wd * (double)ht / 2.0;
   int cntr = 0;
   bool abort = false;
   BeginProgress("Flipping selection vertically");
   int cx, cy;
   int mirrorx = iright;
   iright = (ileft - 1) + wd / 2;
   for ( cx=ileft; cx<=iright; cx++ ) {
      for ( cy=itop; cy<=ibottom; cy++ ) {
         int currstate = curralgo->getcell(cx, cy);
         int mirrstate = curralgo->getcell(mirrorx, cy);
         if ( currstate != mirrstate ) {
            curralgo->setcell(cx, cy, mirrstate);
            curralgo->setcell(mirrorx, cy, currstate);
         }
         cntr++;
         if ((cntr % 4096) == 0) {
            abort = AbortProgress((double)cntr / maxcount, "");
            if (abort) break;
         }
      }
      if (abort) break;
      mirrorx--;
   }
   curralgo->endofpattern();
   mainptr->savestart = true;
   EndProgress();
   mainptr->UpdatePatternAndStatus();
}

void PatternView::FlipHorizontally()
{
   if (mainptr->generating || !SelectionExists()) return;

   // can only use getcell/setcell in limited domain
   if ( OutsideLimits(seltop, selbottom, selleft, selright) ) {
      statusptr->ErrorMessage(selection_too_big);
      return;
   }
   
   int itop = seltop.toint();
   int ileft = selleft.toint();
   int ibottom = selbottom.toint();
   int iright = selright.toint();
   int wd = iright - ileft + 1;
   int ht = ibottom - itop + 1;
   
   if (ht == 1) return;
   
   // following code can be optimized by using faster nextcell calls!!!
   // ie. if entire pattern is selected then flip into new universe and
   // switch to that universe
   
   double maxcount = (double)wd * (double)ht / 2.0;
   int cntr = 0;
   bool abort = false;
   BeginProgress("Flipping selection horizontally");
   int cx, cy;
   int mirrory = ibottom;
   ibottom = (itop - 1) + ht / 2;
   for ( cy=itop; cy<=ibottom; cy++ ) {
      for ( cx=ileft; cx<=iright; cx++ ) {
         int currstate = curralgo->getcell(cx, cy);
         int mirrstate = curralgo->getcell(cx, mirrory);
         if ( currstate != mirrstate ) {
            curralgo->setcell(cx, cy, mirrstate);
            curralgo->setcell(cx, mirrory, currstate);
         }
         cntr++;
         if ((cntr % 4096) == 0) {
            abort = AbortProgress((double)cntr / maxcount, "");
            if (abort) break;
         }
      }
      if (abort) break;
      mirrory--;
   }
   curralgo->endofpattern();
   mainptr->savestart = true;
   EndProgress();
   mainptr->UpdatePatternAndStatus();
}

const char rotate_clockwise[]       = "Rotating selection +90 degrees";
const char rotate_anticlockwise[]   = "Rotating selection -90 degrees";

void PatternView::RotatePattern(bool clockwise,
                                bigint &newtop, bigint &newbottom,
                                bigint &newleft, bigint &newright)
{
   // create new universe of same type as current universe
   lifealgo *newalgo;
   if ( hashing ) {
      newalgo = new hlifealgo();
      newalgo->setMaxMemory(maxhashmem);
   } else {
      newalgo = new qlifealgo();
   }
   newalgo->setpoll(wxGetApp().Poller());

   // set same gen count
   newalgo->setGeneration( curralgo->getGeneration() );
   
   // copy all live cells to new universe, rotating the coords by +/- 90 degrees
   int itop    = seltop.toint();
   int ileft   = selleft.toint();
   int ibottom = selbottom.toint();
   int iright  = selright.toint();
   int wd = iright - ileft + 1;
   int ht = ibottom - itop + 1;
   double maxcount = (double)wd * (double)ht;
   int cntr = 0;
   bool abort = false;
   int cx, cy, newx, newy, newxinc, newyinc, firstnewy;
   if (clockwise) {
      BeginProgress(rotate_clockwise);
      firstnewy = newtop.toint();
      newx = newright.toint();
      newyinc = 1;
      newxinc = -1;
   } else {
      BeginProgress(rotate_anticlockwise);
      firstnewy = newbottom.toint();
      newx = newleft.toint();
      newyinc = -1;
      newxinc = 1;
   }

   for ( cy=itop; cy<=ibottom; cy++ ) {
      newy = firstnewy;
      for ( cx=ileft; cx<=iright; cx++ ) {
         int skip = curralgo->nextcell(cx, cy);
         if (skip + cx > iright)
            skip = -1;           // pretend we found no more live cells
         if (skip >= 0) {
            // found next live cell
            cx += skip;
            newy += newyinc * skip;
            newalgo->setcell(newx, newy, 1);
         } else {
            cx = iright + 1;     // done this row
         }
         cntr++;
         if ((cntr % 4096) == 0) {
            double prog = ((cy - itop) * (double)(iright - ileft + 1) +
                           (cx - ileft)) / maxcount;
            abort = AbortProgress(prog, "");
            if (abort) break;
         }
         newy += newyinc;
      }
      if (abort) break;
      newx += newxinc;
   }

   newalgo->endofpattern();
   EndProgress();
   
   if (abort) {
      delete newalgo;
   } else {
      // rotate the selection edges
      seltop    = newtop;
      selbottom = newbottom;
      selleft   = newleft;
      selright  = newright;
      // switch to new universe and display results
      mainptr->savestart = true;
      delete curralgo;
      curralgo = newalgo;
      mainptr->SetGenIncrement();
      DisplaySelectionSize();
      mainptr->UpdatePatternAndStatus();
   }
}

void PatternView::RotateSelection(bool clockwise)
{
   if (mainptr->generating || !SelectionExists()) return;
   
   // determine rotated selection edges
   bigint halfht = selbottom;   halfht -= seltop;    halfht.div2();
   bigint halfwd = selright;    halfwd -= selleft;   halfwd.div2();
   bigint midy = seltop;    midy += halfht;
   bigint midx = selleft;   midx += halfwd;
   bigint newtop    = midy;   newtop    += selleft;     newtop    -= midx;
   bigint newbottom = midy;   newbottom += selright;    newbottom -= midx;
   bigint newleft   = midx;   newleft   += seltop;      newleft   -= midy;
   bigint newright  = midx;   newright  += selbottom;   newright  -= midy;
   
   // things are simple if there is no pattern
   if (curralgo->isEmpty()) {
      seltop    = newtop;
      selbottom = newbottom;
      selleft   = newleft;
      selright  = newright;
      DisplaySelectionSize();
      mainptr->UpdatePatternAndStatus();
      return;
   }
   
   // things are also simple if the selection and rotated selection are both
   // outside the pattern edges (ie. both are empty)
   bigint top, left, bottom, right;
   curralgo->findedges(&top, &left, &bottom, &right);
   if ( (seltop > bottom || selbottom < top || selleft > right || selright < left) &&
        (newtop > bottom || newbottom < top || newleft > right || newright < left) ) {
      seltop    = newtop;
      selbottom = newbottom;
      selleft   = newleft;
      selright  = newright;
      DisplaySelectionSize();
      mainptr->UpdatePatternAndStatus();
      return;
   }

   // can only use nextcell/getcell/setcell in limited domain
   if ( OutsideLimits(seltop, selbottom, selleft, selright) ) {
      statusptr->ErrorMessage(selection_too_big);
      return;
   }

   // make sure rotated selection edges are also within limits
   if ( OutsideLimits(newtop, newbottom, newleft, newright) ) {
      statusptr->ErrorMessage("New selection would be outside +/- 10^9 boundary.");
      return;
   }
   
   // use faster method if selection encloses entire pattern
   if ( seltop <= top && selbottom >= bottom &&
        selleft <= left && selright >= right ) {
      RotatePattern(clockwise, newtop, newbottom, newleft, newright);
      return;
   }

   // create temporary universe; doesn't need to match current universe so
   // use qlife because its setcell/getcell calls are faster
   lifealgo *tempalgo;
   tempalgo = new qlifealgo();
   tempalgo->setpoll(wxGetApp().Poller());
   
   // copy (and kill) live cells in selection to temporary universe,
   // rotating the new coords by +/- 90 degrees
   int itop    = seltop.toint();
   int ileft   = selleft.toint();
   int ibottom = selbottom.toint();
   int iright  = selright.toint();
   int wd = iright - ileft + 1;
   int ht = ibottom - itop + 1;
   double maxcount = (double)wd * (double)ht;
   int cntr = 0;
   bool abort = false;
   int cx, cy, newx, newy, newxinc, newyinc, firstnewy;
   if (clockwise) {
      BeginProgress(rotate_clockwise);
      firstnewy = newtop.toint();
      newx = newright.toint();
      newyinc = 1;
      newxinc = -1;
   } else {
      BeginProgress(rotate_anticlockwise);
      firstnewy = newbottom.toint();
      newx = newleft.toint();
      newyinc = -1;
      newxinc = 1;
   }

   for ( cy=itop; cy<=ibottom; cy++ ) {
      newy = firstnewy;
      for ( cx=ileft; cx<=iright; cx++ ) {
         int skip = curralgo->nextcell(cx, cy);
         if (skip + cx > iright)
            skip = -1;           // pretend we found no more live cells
         if (skip >= 0) {
            // found next live cell
            cx += skip;
            curralgo->setcell(cx, cy, 0);
            newy += newyinc * skip;
            tempalgo->setcell(newx, newy, 1);
         } else {
            cx = iright + 1;     // done this row
         }
         cntr++;
         if ((cntr % 4096) == 0) {
            double prog = ((cy - itop) * (double)(iright - ileft + 1) +
                           (cx - ileft)) / maxcount;
            abort = AbortProgress(prog, "");
            if (abort) break;
         }
         newy += newyinc;
      }
      if (abort) break;
      newx += newxinc;
   }
   
   tempalgo->endofpattern();
   curralgo->endofpattern();
   EndProgress();
   
   if (abort) {
      // perhaps we should restore original selection???
   } else {
      // copy rotated selection from temporary universe to current universe
      itop    = newtop.toint();
      ileft   = newleft.toint();
      ibottom = newbottom.toint();
      iright  = newright.toint();
      // check if new selection rect is outside modified pattern edges
      curralgo->findedges(&top, &left, &bottom, &right);
      if ( newtop > bottom || newbottom < top || newleft > right || newright < left ) {
         // safe to use fast nextcell calls
         CopyRect(itop, ileft, ibottom, iright,
                  tempalgo, curralgo, false, "Merging rotated selection");
      } else {
         // have to use slow getcell calls
         CopyAllRect(itop, ileft, ibottom, iright,
                     tempalgo, curralgo, "Pasting rotated selection");
      }      
      // rotate the selection edges
      seltop    = newtop;
      selbottom = newbottom;
      selleft   = newleft;
      selright  = newright;
   }
   
   // delete temporary universe and display results
   mainptr->savestart = true;
   delete tempalgo;
   DisplaySelectionSize();
   mainptr->UpdatePatternAndStatus();
}

void PatternView::SetCursorMode(wxCursor *curs)
{
   currcurs = curs;
}

void PatternView::CycleCursorMode()
{
   if (drawingcells || selectingcells || movingview || waitingforclick)
      return;

   if (currcurs == curs_pencil)
      currcurs = curs_cross;
   else if (currcurs == curs_cross)
      currcurs = curs_hand;
   else if (currcurs == curs_hand)
      currcurs = curs_zoomin;
   else if (currcurs == curs_zoomin)
      currcurs = curs_zoomout;
   else
      currcurs = curs_pencil;
}

// -----------------------------------------------------------------------------

// viewing functions:

// zoom out so that central cell stays central
void PatternView::ZoomOut()
{
   TestAutoFit();
   currview.unzoom();
   mainptr->UpdateEverything();
}

// zoom in so that central cell stays central
void PatternView::ZoomIn()
{
   TestAutoFit();
   if (currview.getmag() < MAX_MAG) {
      currview.zoom();
      mainptr->UpdateEverything();
   } else {
      wxBell();
   }
}

void PatternView::SetPixelsPerCell(int pxlspercell)
{
   int mag = 0;
   while (pxlspercell > 1) {
      mag++;
      pxlspercell >>= 1;
   }
   if (mag == currview.getmag()) return;
   TestAutoFit();
   currview.setmag(mag);
   mainptr->UpdateEverything();
}

void PatternView::FitPattern()
{
   curralgo->fit(currview, 1);
   // best not to call TestAutoFit
   mainptr->UpdateEverything();
}

void PatternView::ViewOrigin()
{
   // put 0,0 cell in middle of view
   if ( originx == bigint::zero && originy == bigint::zero ) {
      currview.center();
   } else {
      // put cell saved by ChangeOrigin in middle
      currview.setpositionmag(originx, originy, currview.getmag());
   }
   TestAutoFit();
   mainptr->UpdateEverything();
}

void PatternView::ChangeOrigin()
{
   if (waitingforclick) return;
   // change cell under cursor to 0,0
   wxPoint pt = ScreenToClient( wxGetMousePosition() );
   if ( pt.x < 0 || pt.x > currview.getxmax() ||
        pt.y < 0 || pt.y > currview.getymax() ) {
      statusptr->ErrorMessage("Origin not changed.");
   } else {
      pair<bigint, bigint> cellpos = currview.at(pt.x, pt.y);
      originy = cellpos.second;
      originx = cellpos.first;
      statusptr->DisplayMessage("Origin changed.");
      if ( GridVisible() )
         mainptr->UpdatePatternAndStatus();
      else
         statusptr->UpdateXYLocation();
   }
}

void PatternView::RestoreOrigin()
{
   if (waitingforclick) return;
   if (originx != bigint::zero || originy != bigint::zero) {
      originy = 0;
      originx = 0;
      statusptr->DisplayMessage(origin_restored);
      if ( GridVisible() )
         mainptr->UpdatePatternAndStatus();
      else
         statusptr->UpdateXYLocation();
   }
}

void PatternView::SetViewSize()
{
   int wd, ht;
   GetClientSize(&wd, &ht);
   // wd or ht might be < 1 on Win/X11 platforms
   if (wd < 1) wd = 1;
   if (ht < 1) ht = 1;
   currview.resize(wd, ht);
   // only autofit when generating
   if (autofit && mainptr->generating)
      curralgo->fit(currview, 0);
}

void PatternView::ToggleGridLines()
{
   showgridlines = !showgridlines;
   if (currview.getmag() >= mingridmag)
      mainptr->UpdateEverything();
}

void PatternView::ToggleVideo()
{
   blackcells = !blackcells;
   mainptr->UpdateEverything();
}

void PatternView::ToggleBuffering()
{
   buffered = !buffered;
   mainptr->UpdateEverything();
}

bool PatternView::GetCellPos(bigint &xpos, bigint &ypos)
{
   wxPoint pt = ScreenToClient( wxGetMousePosition() );
   if (PointInView(pt.x, pt.y)) {
      // get mouse location in cell coords
      pair<bigint, bigint> cellpos = currview.at(pt.x, pt.y);
      xpos = cellpos.first;
      ypos = cellpos.second;
      return true;
   } else {
      // mouse not in viewport
      return false;
   }
}

bool PatternView::PointInView(int x, int y)
{
   return (x >= 0) && (x <= currview.getxmax()) &&
          (y >= 0) && (y <= currview.getymax());
}

void PatternView::CheckCursor(bool active)
{
   if (active) {
      // make sure cursor is up to date
      wxPoint pt = ScreenToClient( wxGetMousePosition() );
      if (PointInView(pt.x, pt.y)) {
         // need both calls to fix Mac probs after toggling status/tool bar;
         // test if bug still exists in 2.6.2!!!
         wxSetCursor(*currcurs);
         SetCursor(*currcurs);
      } else {
         wxSetCursor(*wxSTANDARD_CURSOR);
      }
   } else {
      // main window is not active so don't change cursor
   }
}

// -----------------------------------------------------------------------------

// these functions allow currview to be hidden from other modules:

int PatternView::GetMag()
{
   return currview.getmag();
}

void PatternView::SetMag(int mag)
{
   TestAutoFit();
   if (mag > MAX_MAG) mag = MAX_MAG;
   currview.setmag(mag);
   mainptr->UpdateEverything();
}

void PatternView::SetPosMag(const bigint &x, const bigint &y, int mag)
{
   currview.setpositionmag(x, y, mag);
}

void PatternView::GetPos(bigint &x, bigint &y)
{
   x = currview.x;
   y = currview.y;
}

void PatternView::FitInView(int force)
{
   curralgo->fit(currview, force);
}

// -----------------------------------------------------------------------------

// scrolling functions:

void PatternView::PanUp(int amount)
{
   TestAutoFit();
   currview.move(0, -amount);
   mainptr->UpdateEverything();
}

void PatternView::PanDown(int amount)
{
   TestAutoFit();
   currview.move(0, amount);
   mainptr->UpdateEverything();
}

void PatternView::PanLeft(int amount)
{
   TestAutoFit();
   currview.move(-amount, 0);
   mainptr->UpdateEverything();
}

void PatternView::PanRight(int amount)
{
   TestAutoFit();
   currview.move(amount, 0);
   mainptr->UpdateEverything();
}

int PatternView::SmallScroll(int xysize)
{
   int amount;
   int mag = currview.getmag();
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

int PatternView::BigScroll(int xysize)
{
   int amount;
   int mag = currview.getmag();
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

void PatternView::UpdateScrollBars()
{
   if (mainptr->fullscreen) return;

   int viewwd, viewht;
   if (currview.getmag() > 0) {
      // scroll by integral number of cells to avoid rounding probs
      viewwd = currview.getwidth() >> currview.getmag();
      viewht = currview.getheight() >> currview.getmag();
   } else {
      viewwd = currview.getwidth();
      viewht = currview.getheight();
   }
   // keep thumb boxes in middle of scroll bars
   hthumb = (thumbrange - 1) * viewwd / 2;
   vthumb = (thumbrange - 1) * viewht / 2;
   SetScrollbar(wxHORIZONTAL, hthumb, viewwd, thumbrange * viewwd, true);
   SetScrollbar(wxVERTICAL, vthumb, viewht, thumbrange * viewht, true);
}

// -----------------------------------------------------------------------------

void PatternView::ProcessKey(int key, bool shiftdown)
{
   mainptr->showbanner = false;
   switch (key) {
      case WXK_LEFT:    PanLeft( SmallScroll(currview.getwidth()) ); break;
      case WXK_RIGHT:   PanRight( SmallScroll(currview.getwidth()) ); break;
      case WXK_UP:      PanUp( SmallScroll(currview.getheight()) ); break;
      case WXK_DOWN:    PanDown( SmallScroll(currview.getheight()) ); break;

      case WXK_BACK:    // delete key generates backspace code
      case WXK_DELETE:  // probably never happens but play safe
         if (shiftdown)
            ClearOutsideSelection();
         else
            ClearSelection();
         break;
      
      case 'a':   SelectAll(); break;
      case 'k':   RemoveSelection(); break;
      case 's':   ShrinkSelection(true); break;
      case 'v':   PasteClipboard(false); break;
      
      case 'L':   CyclePasteLocation(); break;
      case 'M':   CyclePasteMode(); break;
      case 'c':   CycleCursorMode(); break;

      #ifdef __WXX11__
         // need this so we can use function keys immediately without having
         // to open Edit menu -- sheesh
         case WXK_F5:   SetCursorMode(curs_pencil); break;
         case WXK_F6:   SetCursorMode(curs_cross); break;
         case WXK_F7:   SetCursorMode(curs_hand); break;
         case WXK_F8:   SetCursorMode(curs_zoomin); break;
         case WXK_F9:   SetCursorMode(curs_zoomout); break;
      #endif

      case 'g':
      case WXK_RETURN:
         // not generating -- see PatternView::OnChar
         mainptr->GeneratePattern();
         break;

      case ' ':
         // not generating -- see PatternView::OnChar
         mainptr->NextGeneration(false);  // do only 1 gen
         break;

      case WXK_TAB:
         mainptr->NextGeneration(true);   // use current increment
         break;

      case 't':   mainptr->ToggleAutoFit(); break;
      case 'T':   mainptr->DisplayTimingInfo(); break;

      case WXK_ADD:        // for X11
      case '+':
      case '=':   mainptr->GoFaster(); break;

      case WXK_SUBTRACT:   // for X11
      case '-':
      case '_':   mainptr->GoSlower(); break;

      // F11 is also used on non-Mac platforms (handled by MainFrame::OnMenu)
      case WXK_F1: mainptr->ToggleFullScreen(); break;

      case 'f':   FitPattern(); break;
      case 'F':   FitSelection(); break;

      case WXK_HOME:
      case 'm':   ViewOrigin(); break;
      case '0':   ChangeOrigin(); break;
      case '9':   RestoreOrigin(); break;

      case WXK_DIVIDE:     // for X11
      case '[':
      case '/':   ZoomOut(); break;

      case WXK_MULTIPLY:   // for X11
      case ']':
      case '*':   ZoomIn(); break;

      case '1':   SetPixelsPerCell(1); break;
      case '2':   SetPixelsPerCell(2); break;
      case '4':   SetPixelsPerCell(4); break;
      case '8':   SetPixelsPerCell(8); break;
      case '6':   SetPixelsPerCell(16); break;

      case 'l':   ToggleGridLines(); break;
      case 'b':   ToggleVideo(); break;
      case ';':   mainptr->ToggleStatusBar(); break;
      case '\'':  mainptr->ToggleToolBar(); break;
      case 'i':   mainptr->ShowPatternInfo(); break;
      case ',':   mainptr->ShowPrefsDialog(); break;
      case 'p':   mainptr->ToggleShowPatterns(); break;
      case 'P':   mainptr->ToggleShowScripts(); break;
   
      case 'h':
      case WXK_HELP:
         if (!waitingforclick) {
            // if help window is open then bring it to the front,
            // otherwise open it and display last help file
            ShowHelp("");
         }
         break;

      default:
         // any other key turns off full screen mode
         if (mainptr->fullscreen) mainptr->ToggleFullScreen();
   }
}

// -----------------------------------------------------------------------------

// event table and handlers:

BEGIN_EVENT_TABLE(PatternView, wxWindow)
   EVT_PAINT            (                 PatternView::OnPaint)
   EVT_KEY_DOWN         (                 PatternView::OnKeyDown)
   EVT_KEY_UP           (                 PatternView::OnKeyUp)
   EVT_CHAR             (                 PatternView::OnChar)
   EVT_LEFT_DOWN        (                 PatternView::OnMouseDown)
   EVT_LEFT_DCLICK      (                 PatternView::OnMouseDown)
   EVT_LEFT_UP          (                 PatternView::OnMouseUp)
   EVT_RIGHT_DOWN       (                 PatternView::OnRMouseDown)
   EVT_RIGHT_DCLICK     (                 PatternView::OnRMouseDown)
   EVT_MOTION           (                 PatternView::OnMouseMotion)
   EVT_ENTER_WINDOW     (                 PatternView::OnMouseEnter)
   EVT_LEAVE_WINDOW     (                 PatternView::OnMouseExit)
   EVT_MOUSEWHEEL       (                 PatternView::OnMouseWheel)
   EVT_TIMER            (ID_DRAG_TIMER,   PatternView::OnDragTimer)
   EVT_SCROLLWIN        (                 PatternView::OnScroll)
   EVT_ERASE_BACKGROUND (                 PatternView::OnEraseBackground)
END_EVENT_TABLE()

void PatternView::OnPaint(wxPaintEvent& WXUNUSED(event))
{
   int wd, ht;
   GetClientSize(&wd, &ht);
   // wd or ht might be < 1 on Win/X11 platforms
   if (wd < 1) wd = 1;
   if (ht < 1) ht = 1;
   if ( wd != currview.getwidth() || ht != currview.getheight() ) {
      // need to change viewport size;
      // can happen on Windows when resizing/maximizing
      SetViewSize();
   }

   #ifdef __WXMAC__
      // windows on Mac OS X are automatically buffered
      wxPaintDC dc(this);
      dc.BeginDrawing();
      DrawView(dc, currview);
      dc.EndDrawing();
   #else
      if ( buffered || waitingforclick || GridVisible() || SelectionVisible(NULL) ) {
         // use wxWidgets buffering to avoid flicker
         if (wd != viewbitmapwd || ht != viewbitmapht) {
            // need to create a new bitmap for viewport
            if (viewbitmap) delete viewbitmap;
            viewbitmap = new wxBitmap(wd, ht);
            if (viewbitmap == NULL) Fatal("Not enough memory to do buffering!");
            viewbitmapwd = wd;
            viewbitmapht = ht;
         }
         wxBufferedPaintDC dc(this, *viewbitmap);
         dc.BeginDrawing();
         DrawView(dc, currview);
         dc.EndDrawing();
      } else {
         wxPaintDC dc(this);
         dc.BeginDrawing();
         DrawView(dc, currview);
         dc.EndDrawing();
      }
   #endif
}

void PatternView::OnKeyDown(wxKeyEvent& event)
{
   int key = event.GetKeyCode();
   statusptr->ClearMessage();
   
   #ifdef __WXMSW__
      // space is an accelerator for Next menu item which is disabled
      // when generating so space won't be seen by PatternView::OnChar
      // (for some reason wxMac has no such problem)
      if ( mainptr->generating && key == ' ' ) {
         mainptr->StopGenerating();
         return;
      }
   #endif
   
   if (key == WXK_SHIFT) {
      // pressing shift key temporarily toggles zoom in/out cursor;
      // some platforms (eg. WinXP) send multiple key-down events while
      // a key is pressed so we must be careful to toggle only once
      if (currcurs == curs_zoomin && oldzoom == NULL) {
         oldzoom = curs_zoomin;
         SetCursorMode(curs_zoomout);
         mainptr->UpdateUserInterface(mainptr->IsActive());
      } else if (currcurs == curs_zoomout && oldzoom == NULL) {
         oldzoom = curs_zoomout;
         SetCursorMode(curs_zoomin);
         mainptr->UpdateUserInterface(mainptr->IsActive());
      }
   }
   
   event.Skip();
}

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

void PatternView::OnChar(wxKeyEvent& event)
{
   // get translated keyboard event
   int key = event.GetKeyCode();

   if ( mainptr->generating && (key == '.' || key == WXK_RETURN || key == ' ') ) {
      mainptr->StopGenerating();
      return;
   }

   if ( waitingforclick && key == WXK_ESCAPE ) {
      // cancel paste
      pastex = -1;
      pastey = -1;
      waitingforclick = false;
      return;
   }

   if ( selectingcells && key == WXK_ESCAPE ) {
      RestoreSelection();
      return;
   }

   #ifndef __WXMSW__
      // need these tests here because wxX11 doesn't seem to process accelerators
      // before EVT_CHAR handler and so NextGeneration is not called from OnMenu;
      // and 1st call of wxGetKeyState on wxMac causes a long delay -- sheesh
      if ( key == ' ' && event.ShiftDown() ) {
         mainptr->AdvanceOutsideSelection();
         return;
      } else if ( key == ' ' && event.ControlDown() ) {
         mainptr->AdvanceSelection();
         return;
      }
   #endif

   // this was added to test Fatal, but could be useful to quit without saving prefs
   if ( key == 17 || (key == 'q' && event.ControlDown()) ) {
      Fatal("Quitting without saving preferences.");
   }

   if ( event.CmdDown() || event.AltDown() ) {
      event.Skip();
   } else {
      ProcessKey(key, event.ShiftDown());
      mainptr->UpdateUserInterface(mainptr->IsActive());
   }
}

void PatternView::ProcessControlClick(int x, int y)
{
   if (currcurs == curs_zoomin) {
      ZoomOutPos(x, y);
   } else if (currcurs == curs_zoomout) {
      ZoomInPos(x, y);
   } else {
      /* let all other cursor modes advance clicked region -- probably unwise
      pair<bigint, bigint> cellpos = currview.at(x, y);
      if ( cellpos.first < selleft || cellpos.first > selright ||
           cellpos.second < seltop || cellpos.second > selbottom ) {
         mainptr->AdvanceOutsideSelection();
      } else {
         mainptr->AdvanceSelection();
      }
      */
   }
}

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

void PatternView::OnMouseUp(wxMouseEvent& WXUNUSED(event))
{
   if (drawingcells || selectingcells || movingview) {
      StopDraggingMouse();
   }
}

void PatternView::OnRMouseDown(wxMouseEvent& event)
{
   // this is equivalent to control-click in wxMac/wxMSW but not in wxX11 -- sigh
   statusptr->ClearMessage();
   mainptr->showbanner = false;
   ProcessControlClick(event.GetX(), event.GetY());
}

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
      currview.unzoom();
   }

   while (wheelpos <= -delta) {
      wheelpos += delta;
      TestAutoFit();
      if (currview.getmag() < MAX_MAG) {
         currview.zoom();
      } else {
         wxBell();
         break;      // best not to beep lots of times
      }
   }

   mainptr->UpdateEverything();
}

void PatternView::OnMouseMotion(wxMouseEvent& WXUNUSED(event))
{
   statusptr->CheckMouseLocation(mainptr->IsActive());
}

void PatternView::OnMouseEnter(wxMouseEvent& WXUNUSED(event))
{
   // Win bug??? we don't get this event if CaptureMouse has been called
   CheckCursor(mainptr->IsActive());
   // no need to call CheckMouseLocation here (OnMouseMotion will be called)
}

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

void PatternView::OnDragTimer(wxTimerEvent& WXUNUSED(event))
{
   // called periodically while drawing/selecting/moving
   wxPoint pt = ScreenToClient( wxGetMousePosition() );
   int x = pt.x;
   int y = pt.y;
   // don't test "!PointInView(x, y)" here -- we want to allow scrolling
   // in full screen mode when mouse is at outer edge of view
   if ( x <= 0 || x >= currview.getxmax() ||
        y <= 0 || y >= currview.getymax() ) {
      // scroll view
      int xamount = 0;
      int yamount = 0;
      if (x <= 0) xamount = -SmallScroll(currview.getwidth());
      if (y <= 0) yamount = -SmallScroll(currview.getheight());
      if (x >= currview.getxmax()) xamount = SmallScroll(currview.getwidth());
      if (y >= currview.getymax()) yamount = SmallScroll(currview.getheight());

      if ( drawingcells ) {
         currview.move(xamount, yamount);
         mainptr->UpdatePatternAndStatus();

      } else if ( selectingcells ) {
         currview.move(xamount, yamount);
         // no need to call UpdatePatternAndStatus() here because
         // it will be called soon in SelectCells, except in this case:
         if (forceh || forcev) {
            // selection might not change so must update pattern
            Refresh(false, NULL);
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
         currview.move(-xamount, -yamount);
         mainptr->UpdatePatternAndStatus();
         // adjust x,y and bigcellx,bigcelly for MoveView call below
         x += xamount;
         y += yamount;
         pair<bigint, bigint> cellpos = currview.at(x, y);
         bigcellx = cellpos.first;
         bigcelly = cellpos.second;
      }
   }

   if ( drawingcells ) {
      // only draw cells within view
      if (x < 0) x = 0;
      if (y < 0) y = 0;
      if (x > currview.getxmax()) x = currview.getxmax();
      if (y > currview.getymax()) y = currview.getymax();
      DrawCells(x, y);

   } else if ( selectingcells ) {
      // only select cells within view
      if (x < 0) x = 0;
      if (y < 0) y = 0;
      if (x > currview.getxmax()) x = currview.getxmax();
      if (y > currview.getymax()) y = currview.getymax();
      SelectCells(x, y);

   } else if ( movingview ) {
      MoveView(x, y);
   }
}

void PatternView::OnScroll(wxScrollWinEvent& event)
{
   WXTYPE type = (WXTYPE)event.GetEventType();
   int orient = event.GetOrientation();

   if (type == wxEVT_SCROLLWIN_LINEUP)
   {
      if (orient == wxHORIZONTAL)
         PanLeft( SmallScroll(currview.getwidth()) );
      else
         PanUp( SmallScroll(currview.getheight()) );
   }
   else if (type == wxEVT_SCROLLWIN_LINEDOWN)
   {
      if (orient == wxHORIZONTAL)
         PanRight( SmallScroll(currview.getwidth()) );
      else
         PanDown( SmallScroll(currview.getheight()) );
   }
   else if (type == wxEVT_SCROLLWIN_PAGEUP)
   {
      if (orient == wxHORIZONTAL)
         PanLeft( BigScroll(currview.getwidth()) );
      else
         PanUp( BigScroll(currview.getheight()) );
   }
   else if (type == wxEVT_SCROLLWIN_PAGEDOWN)
   {
      if (orient == wxHORIZONTAL)
         PanRight( BigScroll(currview.getwidth()) );
      else
         PanDown( BigScroll(currview.getheight()) );
   }
   else if (type == wxEVT_SCROLLWIN_THUMBTRACK)
   {
      int newpos = event.GetPosition();
      int amount = newpos - (orient == wxHORIZONTAL ? hthumb : vthumb);
      if (amount != 0) {
         TestAutoFit();
         if (currview.getmag() > 0) {
            // amount is in cells so convert to pixels
            amount = amount << currview.getmag();
         }
         if (orient == wxHORIZONTAL) {
            hthumb = newpos;
            currview.move(amount, 0);
            // don't call UpdateEverything here because it calls UpdateScrollBars
            Refresh(false, NULL);
            // don't Update() immediately -- more responsive, especially on X11
         } else {
            vthumb = newpos;
            currview.move(0, amount);
            // don't call UpdateEverything here because it calls UpdateScrollBars
            Refresh(false, NULL);
            // don't Update() immediately -- more responsive, especially on X11
         }
      }
      #ifdef __WXX11__
         // need to change the thumb position manually
         SetScrollPos(orient, newpos, true);
      #endif
   }
   else if (type == wxEVT_SCROLLWIN_THUMBRELEASE)
   {
      // now we can call UpdateScrollBars
      mainptr->UpdateEverything();
   }
}

void PatternView::OnEraseBackground(wxEraseEvent& WXUNUSED(event))
{
   // do nothing because we'll be painting the entire viewport
}

// -----------------------------------------------------------------------------

// create the viewport window
PatternView::PatternView(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht)
   : wxWindow(parent, wxID_ANY, wxPoint(xorg,yorg), wxSize(wd,ht),
                  wxNO_BORDER |
                  wxWANTS_CHARS |              // receive all keyboard events
                  wxFULL_REPAINT_ON_RESIZE |
                  wxVSCROLL | wxHSCROLL
             )
{
   // avoid erasing background on GTK+
   SetBackgroundStyle(wxBG_STYLE_CUSTOM);

   dragtimer = new wxTimer(this, ID_DRAG_TIMER);
   if (dragtimer == NULL) Fatal("Failed to create drag timer!");

   drawingcells = false;      // not drawing cells
   selectingcells = false;    // not selecting cells
   movingview = false;        // not moving view
   waitingforclick = false;   // not waiting for user to click
   nopattupdate = false;      // enable pattern updates
   oldzoom = NULL;            // not shift zooming
   originy = 0;               // no origin offset
   originx = 0;
   NoSelection();             // initially no selection
}

// destroy the viewport window
PatternView::~PatternView()
{
   if (dragtimer) delete dragtimer;

   #ifndef __WXMAC__
      if (viewbitmap) delete viewbitmap;
   #endif
}
