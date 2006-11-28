
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

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "viewport.h"

#include "wxgolly.h"       // for mainptr, statusptr, curralgo
#include "wxutils.h"       // for Warning, Fatal
#include "wxprefs.h"       // for showgridlines, etc
#include "wxhelp.h"        // for ShowHelp
#include "wxmain.h"        // for mainptr->...
#include "wxstatus.h"      // for statusptr->...
#include "wxrender.h"      // for DrawView, DrawSelection
#include "wxscript.h"      // for inscript, PassKeyToScript
#include "wxlayer.h"       // for ResizeLayers, currlayer, etc
#include "wxview.h"

// This module contains most View menu functions (a few like ToggleFullScreen
// and ToggleToolBar are MainFrame methods and so best kept in wxmain.cpp).
// It also contains all the event handlers for the viewport window, such as
// OnPaint, OnKeyDown, OnChar, OnMouseDown, etc.

// -----------------------------------------------------------------------------

// current viewport for displaying patterns
viewport *currview = NULL;

// bitmap for wxBufferedPaintDC is not needed on Mac OS X because
// windows are automatically buffered
#ifndef __WXMAC__
   wxBitmap *viewbitmap = NULL;     // viewport bitmap for OnPaint
   int viewbitmapwd = -1;           // width of viewport bitmap
   int viewbitmapht = -1;           // height of viewport bitmap
#endif

// call OnDragTimer 50 times per sec
const int DRAG_RATE = 20;
const int ID_DRAG_TIMER = 1000;

#ifdef __WXGTK__
   // avoid wxGTK scroll bug
   bool ignorescroll = false;       // ignore next wxEVT_SCROLLWIN_* event?
#endif

// -----------------------------------------------------------------------------

// zoom out so that central cell stays central
void PatternView::ZoomOut()
{
   TestAutoFit();
   currview->unzoom();
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

// zoom in so that central cell stays central
void PatternView::ZoomIn()
{
   TestAutoFit();
   if (currview->getmag() < MAX_MAG) {
      currview->zoom();
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
   if (mag == currview->getmag()) return;
   TestAutoFit();
   currview->setmag(mag);
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::FitPattern()
{
   curralgo->fit(*currview, 1);
   // best not to call TestAutoFit
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

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
      currview->setpositionmag(newx, newy, mag);
      if ( currview->contains(selleft, seltop) &&
           currview->contains(selright, selbottom) )
         break;
      mag--;
   }
   
   TestAutoFit();
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::ViewOrigin()
{
   // put 0,0 cell in middle of view
   if ( originx == bigint::zero && originy == bigint::zero ) {
      currview->center();
   } else {
      // put cell saved by ChangeOrigin in middle
      currview->setpositionmag(originx, originy, currview->getmag());
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
   if ( pt.x < 0 || pt.x > currview->getxmax() ||
        pt.y < 0 || pt.y > currview->getymax() ) {
      statusptr->ErrorMessage(_("Origin not changed."));
   } else {
      pair<bigint, bigint> cellpos = currview->at(pt.x, pt.y);
      originy = cellpos.second;
      originx = cellpos.first;
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

// -----------------------------------------------------------------------------

void PatternView::SetViewSize()
{
   int wd, ht;
   GetClientSize(&wd, &ht);
   // wd or ht might be < 1 on Win/X11 platforms
   if (wd < 1) wd = 1;
   if (ht < 1) ht = 1;
   ResizeLayers(wd, ht);
   // only autofit when generating
   if (autofit && mainptr->generating)
      curralgo->fit(*currview, 0);
}

// -----------------------------------------------------------------------------

void PatternView::ToggleGridLines()
{
   showgridlines = !showgridlines;
   if (currview->getmag() >= mingridmag)
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

bool PatternView::GetCellPos(bigint &xpos, bigint &ypos)
{
   wxPoint pt = ScreenToClient( wxGetMousePosition() );
   if (PointInView(pt.x, pt.y)) {
      // get mouse location in cell coords
      pair<bigint, bigint> cellpos = currview->at(pt.x, pt.y);
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
   return (x >= 0) && (x <= currview->getxmax()) &&
          (y >= 0) && (y <= currview->getymax());
}

// -----------------------------------------------------------------------------

void PatternView::CheckCursor(bool active)
{
   if (active) {
      // make sure cursor is up to date
      wxPoint pt = ScreenToClient( wxGetMousePosition() );
      if (PointInView(pt.x, pt.y)) {
         #ifdef __WXMAC__
            // wxMac bug??? need this to fix probs after toggling status/tool bar
            wxSetCursor(*currcurs);
         #endif
         SetCursor(*currcurs);
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
   return currview->getmag();
}

// -----------------------------------------------------------------------------

void PatternView::SetMag(int mag)
{
   TestAutoFit();
   if (mag > MAX_MAG) mag = MAX_MAG;
   currview->setmag(mag);
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::SetPosMag(const bigint &x, const bigint &y, int mag)
{
   currview->setpositionmag(x, y, mag);
}

// -----------------------------------------------------------------------------

void PatternView::GetPos(bigint &x, bigint &y)
{
   x = currview->x;
   y = currview->y;
}

// -----------------------------------------------------------------------------

void PatternView::FitInView(int force)
{
   curralgo->fit(*currview, force);
}

// -----------------------------------------------------------------------------

int PatternView::CellVisible(const bigint &x, const bigint &y)
{
   return currview->contains(x, y);
}

// -----------------------------------------------------------------------------

// scrolling functions:

void PatternView::PanUp(int amount)
{
   TestAutoFit();
   currview->move(0, -amount);
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::PanDown(int amount)
{
   TestAutoFit();
   currview->move(0, amount);
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::PanLeft(int amount)
{
   TestAutoFit();
   currview->move(-amount, 0);
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void PatternView::PanRight(int amount)
{
   TestAutoFit();
   currview->move(amount, 0);
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

int PatternView::SmallScroll(int xysize)
{
   int amount;
   int mag = currview->getmag();
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
   int mag = currview->getmag();
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
   if (currview->getmag() > 0) {
      // scroll by integral number of cells to avoid rounding probs
      viewwd = currview->getwidth() >> currview->getmag();
      viewht = currview->getheight() >> currview->getmag();
   } else {
      viewwd = currview->getwidth();
      viewht = currview->getheight();
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
      case WXK_LEFT:    PanLeft( SmallScroll(currview->getwidth()) ); break;
      case WXK_RIGHT:   PanRight( SmallScroll(currview->getwidth()) ); break;
      case WXK_UP:      PanUp( SmallScroll(currview->getheight()) ); break;
      case WXK_DOWN:    PanDown( SmallScroll(currview->getheight()) ); break;

      // note that ProcessKey can be called from the golly_dokey command
      // so best to avoid changing pattern while running a script

      case WXK_BACK:       // delete key generates backspace code
      case WXK_DELETE:     // probably never happens but play safe
         if (shiftdown) {
            if (!inscript) ClearOutsideSelection();
         } else {
            if (!inscript) ClearSelection();
         }
         break;
      
      case 'a':   SelectAll(); break;
      case 'k':   RemoveSelection(); break;
      case 's':   ShrinkSelection(true); break;
      case 'v':   if (!inscript) PasteClipboard(false); break;
      
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
         if (!inscript) mainptr->GeneratePattern();
         break;

      case ' ':
         // not generating -- see PatternView::OnChar
         if (!inscript) mainptr->NextGeneration(false);  // do only 1 gen
         break;

      case WXK_TAB:
         if (!inscript) mainptr->NextGeneration(true);   // use current increment
         break;

      case 't':   mainptr->ToggleAutoFit(); break;
      
      // timing info is only for GeneratePattern calls
      case 'T':   if (!inscript) mainptr->DisplayTimingInfo(); break;

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

      case '\'':  mainptr->ToggleToolBar(); break;
      case ';':   mainptr->ToggleStatusBar(); break;
      case 'e':   mainptr->ToggleExactNumbers(); break;
      case 'l':   ToggleGridLines(); break;
      case 'b':   ToggleCellColors(); break;
      case 'i':   mainptr->ShowPatternInfo(); break;
      case ',':   mainptr->ShowPrefsDialog(); break;
      case 'p':   mainptr->ToggleShowPatterns(); break;
      case 'P':   mainptr->ToggleShowScripts(); break;
   
      case 'h':
      case WXK_HELP:
         if (!waitingforclick) {
            // if help window is open then bring it to the front,
            // otherwise open it and display last help file
            ShowHelp(wxEmptyString);
         }
         break;

      default:
         // any other key turns off full screen mode
         if (mainptr->fullscreen) mainptr->ToggleFullScreen();
   }
}

// -----------------------------------------------------------------------------

void PatternView::ShowDrawing()
{
   curralgo->endofpattern();
   currlayer->savestart = true;

   // update status bar
   if (mainptr->StatusVisible()) {
      statusptr->Refresh(false, NULL);
   }

   if (numlayers > 1 && drawlayers) {
      // update all layers; this is rather slow but most people won't be
      // drawing cells when all layers are displayed (too confusing)
      Refresh(false, NULL);
      Update();
   }
}

// -----------------------------------------------------------------------------

void PatternView::DrawOneCell(int cx, int cy, wxDC &dc)
{
   if (numlayers > 1 && drawlayers) {
      // drawing must be done via Update in ShowDrawing
      return;
   }

   int cellsize = 1 << currview->getmag();

   // convert given cell coords to view coords
   pair<bigint, bigint> lefttop = currview->at(0, 0);
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

// -----------------------------------------------------------------------------

void PatternView::StartDrawingCells(int x, int y)
{
   pair<bigint, bigint> cellpos = currview->at(x, y);
   // check that cellpos is within getcell/setcell limits
   if ( OutsideLimits(cellpos.second, cellpos.first, cellpos.second, cellpos.first) ) {
      statusptr->ErrorMessage(_("Drawing is not allowed outside +/- 10^9 boundary."));
      return;
   }

   cellx = cellpos.first.toint();
   celly = cellpos.second.toint();
   drawstate = 1 - curralgo->getcell(cellx, celly);
   curralgo->setcell(cellx, celly, drawstate);

   wxClientDC dc(this);
   dc.SetPen(*wxTRANSPARENT_PEN);
   dc.SetBrush(drawstate == (int)swapcolors ? *deadbrush : *livebrush[currindex]);
   DrawOneCell(cellx, celly, dc);
   dc.SetBrush(wxNullBrush);        // restore brush
   dc.SetPen(wxNullPen);            // restore pen
   
   ShowDrawing();

   drawingcells = true;
   CaptureMouse();                  // get mouse up event even if outside view
   dragtimer->Start(DRAG_RATE);     // see OnDragTimer
}

// -----------------------------------------------------------------------------

void PatternView::DrawCells(int x, int y)
{
   pair<bigint, bigint> cellpos = currview->at(x, y);
   if ( currview->getmag() < 0 ||
        OutsideLimits(cellpos.second, cellpos.first, cellpos.second, cellpos.first) ) {
      return;
   }

   int newx = cellpos.first.toint();
   int newy = cellpos.second.toint();
   if ( newx != cellx || newy != celly ) {
      wxClientDC dc(this);
      dc.SetPen(*wxTRANSPARENT_PEN);
      dc.SetBrush(drawstate == (int)swapcolors ? *deadbrush : *livebrush[currindex]);

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
      
      if ( numchanged > 0 ) ShowDrawing();
   }
}

// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------

void PatternView::StartSelectingCells(int x, int y, bool shiftdown)
{
   pair<bigint, bigint> cellpos = currview->at(x, y);
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
      } else {
         // remove current selection
         NoSelection();
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

   pair<bigint, bigint> cellpos = currview->at(x, y);
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
      
      // allow mouse interaction if script is running
      bool saveinscript = inscript;
      inscript = false;
      mainptr->UpdatePatternAndStatus();
      inscript = saveinscript;
      
      prevtop = seltop;
      prevbottom = selbottom;
      prevleft = selleft;
      prevright = selright;
   }
}

// -----------------------------------------------------------------------------

void PatternView::StartMovingView(int x, int y)
{
   pair<bigint, bigint> cellpos = currview->at(x, y);
   bigcellx = cellpos.first;
   bigcelly = cellpos.second;
   movingview = true;
   CaptureMouse();                  // get mouse up event even if outside view
   dragtimer->Start(DRAG_RATE);     // see OnDragTimer
}

// -----------------------------------------------------------------------------

void PatternView::MoveView(int x, int y)
{
   pair<bigint, bigint> cellpos = currview->at(x, y);
   bigint newx = cellpos.first;
   bigint newy = cellpos.second;
   bigint xdelta = bigcellx;
   bigint ydelta = bigcelly;
   xdelta -= newx;
   ydelta -= newy;

   int xamount, yamount;
   int mag = currview->getmag();
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
      currview->move(xamount, yamount);
      
      // allow mouse interaction if script is running
      bool saveinscript = inscript;
      inscript = false;
      mainptr->UpdatePatternAndStatus();
      inscript = saveinscript;
      
      cellpos = currview->at(x, y);
      bigcellx = cellpos.first;
      bigcelly = cellpos.second;
   }
}

// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------

void PatternView::TestAutoFit()
{
   if (autofit && mainptr->generating) {
      // assume user no longer wants us to do autofitting
      autofit = false;
   }
}

// -----------------------------------------------------------------------------

void PatternView::ZoomInPos(int x, int y)
{
   // zoom in so that clicked cell stays under cursor
   TestAutoFit();
   if (currview->getmag() < MAX_MAG) {
      currview->zoom(x, y);
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
   currview->unzoom(x, y);
   // allow mouse interaction if script is running
   bool saveinscript = inscript;
   inscript = false;
   mainptr->UpdateEverything();
   inscript = saveinscript;
}

// -----------------------------------------------------------------------------

void PatternView::ProcessClick(int x, int y, bool shiftdown)
{
   // user has clicked somewhere in viewport   
   if (currcurs == curs_pencil) {
      if (inscript) {
         // statusptr->ErrorMessage does nothing if inscript is true
         Warning(_("Drawing is not allowed while a script is running."));
         return;
      }
      if (mainptr->generating) {
         statusptr->ErrorMessage(_("Drawing is not allowed while a pattern is generating."));
         return;
      }
      if (currview->getmag() < 0) {
         statusptr->ErrorMessage(_("Drawing is not allowed at scales greater than 1 cell per pixel."));
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

// -----------------------------------------------------------------------------

void PatternView::OnPaint(wxPaintEvent& WXUNUSED(event))
{
   int wd, ht;
   GetClientSize(&wd, &ht);
   // wd or ht might be < 1 on Win/X11 platforms
   if (wd < 1) wd = 1;
   if (ht < 1) ht = 1;
   if ( wd != currview->getwidth() || ht != currview->getheight() ) {
      // need to change viewport size;
      // can happen on Windows when resizing/maximizing
      SetViewSize();
   }

   #ifdef __WXMAC__
      // windows on Mac OS X are automatically buffered
      wxPaintDC dc(this);
      DrawView(dc);
   #else
      if ( buffered || waitingforclick || GridVisible() || SelectionVisible(NULL) ||
           (numlayers > 1 && drawlayers) ) {
         // use wxWidgets buffering to avoid flicker
         if (wd != viewbitmapwd || ht != viewbitmapht) {
            // need to create a new bitmap for viewport
            if (viewbitmap) delete viewbitmap;
            viewbitmap = new wxBitmap(wd, ht);
            if (viewbitmap == NULL) Fatal(_("Not enough memory to do buffering!"));
            viewbitmapwd = wd;
            viewbitmapht = ht;
         }
         wxBufferedPaintDC dc(this, *viewbitmap);
         DrawView(dc);
      } else {
         wxPaintDC dc(this);
         DrawView(dc);
      }
   #endif
}

// -----------------------------------------------------------------------------

void PatternView::OnKeyDown(wxKeyEvent& event)
{
   statusptr->ClearMessage();
   int key = event.GetKeyCode();
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

void PatternView::RestoreSelection()
{
   seltop = origtop;
   selbottom = origbottom;
   selleft = origleft;
   selright = origright;
   StopDraggingMouse();
   
   // allow mouse interaction if script is running
   bool saveinscript = inscript;
   inscript = false;
   mainptr->UpdatePatternAndStatus();
   inscript = saveinscript;
   
   statusptr->DisplayMessage(_("New selection aborted."));
}

// -----------------------------------------------------------------------------

void PatternView::OnChar(wxKeyEvent& event)
{
   // get translated keyboard event
   int key = event.GetKeyCode();

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
         /*
         char msg[64];
         char ch = key;
         sprintf(msg, "key=%d char=%c shifted=%d", key, ch, event.ShiftDown() );
         Warning(msg);
         */
         if ( key < 0 || key > 255 ) return;
      #endif
      // let script decide what to do with the key
      PassKeyToScript(key);
      return;
   }

   if ( waitingforclick && key == WXK_ESCAPE ) {
      // cancel paste
      pastex = -1;
      pastey = -1;
      waitingforclick = false;
      return;
   }
   
   if ( mainptr->generating && (key == WXK_ESCAPE || key == WXK_RETURN || key == ' ' || key == '.') ) {
      mainptr->StopGenerating();
      return;
   }

   if ( key == ' ' && event.ShiftDown() ) {
      mainptr->AdvanceOutsideSelection();
      return;
   } else if ( key == ' ' && event.ControlDown() ) {
      mainptr->AdvanceSelection();
      return;
   }

   // this was added to test Fatal, but could be useful to quit without saving prefs
   if ( key == 17 && event.ShiftDown() ) {
      // ^Q == 17
      Fatal(_("Quitting without saving preferences."));
   }

   if ( event.CmdDown() || event.AltDown() ) {
      event.Skip();
   } else {
      ProcessKey(key, event.ShiftDown());
      mainptr->UpdateUserInterface(mainptr->IsActive());
   }
}

// -----------------------------------------------------------------------------

void PatternView::ProcessControlClick(int x, int y)
{
   if (currcurs == curs_zoomin) {
      ZoomOutPos(x, y);
   } else if (currcurs == curs_zoomout) {
      ZoomInPos(x, y);
   } else {
      /* let all other cursor modes advance clicked region -- probably unwise
      pair<bigint, bigint> cellpos = currview->at(x, y);
      if ( cellpos.first < selleft || cellpos.first > selright ||
           cellpos.second < seltop || cellpos.second > selbottom ) {
         mainptr->AdvanceOutsideSelection();
      } else {
         mainptr->AdvanceSelection();
      }
      */
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
   }
}

// -----------------------------------------------------------------------------

void PatternView::OnRMouseDown(wxMouseEvent& event)
{
   // this is equivalent to control-click in wxMac/wxMSW but not in wxX11 -- sigh
   statusptr->ClearMessage();
   mainptr->showbanner = false;
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
      currview->unzoom();
   }

   while (wheelpos <= -delta) {
      wheelpos += delta;
      TestAutoFit();
      if (currview->getmag() < MAX_MAG) {
         currview->zoom();
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
   if ( x <= 0 || x >= currview->getxmax() ||
        y <= 0 || y >= currview->getymax() ) {
      // scroll view
      int xamount = 0;
      int yamount = 0;
      if (x <= 0) xamount = -SmallScroll(currview->getwidth());
      if (y <= 0) yamount = -SmallScroll(currview->getheight());
      if (x >= currview->getxmax()) xamount = SmallScroll(currview->getwidth());
      if (y >= currview->getymax()) yamount = SmallScroll(currview->getheight());

      if ( drawingcells ) {
         currview->move(xamount, yamount);
         mainptr->UpdatePatternAndStatus();

      } else if ( selectingcells ) {
         currview->move(xamount, yamount);
         // no need to call UpdatePatternAndStatus() here because
         // it will be called soon in SelectCells, except in this case:
         if (forceh || forcev) {
            // selection might not change so must update pattern
            Refresh(false, NULL);
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
         currview->move(-xamount, -yamount);
         
         // allow mouse interaction if script is running
         bool saveinscript = inscript;
         inscript = false;
         mainptr->UpdatePatternAndStatus();
         inscript = saveinscript;
         
         // adjust x,y and bigcellx,bigcelly for MoveView call below
         x += xamount;
         y += yamount;
         pair<bigint, bigint> cellpos = currview->at(x, y);
         bigcellx = cellpos.first;
         bigcelly = cellpos.second;
      }
   }

   if ( drawingcells ) {
      // only draw cells within view
      if (x < 0) x = 0;
      if (y < 0) y = 0;
      if (x > currview->getxmax()) x = currview->getxmax();
      if (y > currview->getymax()) y = currview->getymax();
      DrawCells(x, y);

   } else if ( selectingcells ) {
      // only select cells within view
      if (x < 0) x = 0;
      if (y < 0) y = 0;
      if (x > currview->getxmax()) x = currview->getxmax();
      if (y > currview->getymax()) y = currview->getymax();
      SelectCells(x, y);

   } else if ( movingview ) {
      MoveView(x, y);
   }
}

// -----------------------------------------------------------------------------

void PatternView::OnScroll(wxScrollWinEvent& event)
{
   #ifdef __WXGTK__
      // avoid unwanted scroll event
      if (ignorescroll) {
         ignorescroll = false;
         UpdateScrollBars();
         return;
      }
   #endif
   
   WXTYPE type = event.GetEventType();
   int orient = event.GetOrientation();

   if (type == wxEVT_SCROLLWIN_LINEUP) {
      if (orient == wxHORIZONTAL)
         PanLeft( SmallScroll(currview->getwidth()) );
      else
         PanUp( SmallScroll(currview->getheight()) );

   } else if (type == wxEVT_SCROLLWIN_LINEDOWN) {
      if (orient == wxHORIZONTAL)
         PanRight( SmallScroll(currview->getwidth()) );
      else
         PanDown( SmallScroll(currview->getheight()) );

   } else if (type == wxEVT_SCROLLWIN_PAGEUP) {
      if (orient == wxHORIZONTAL)
         PanLeft( BigScroll(currview->getwidth()) );
      else
         PanUp( BigScroll(currview->getheight()) );

   } else if (type == wxEVT_SCROLLWIN_PAGEDOWN) {
      if (orient == wxHORIZONTAL)
         PanRight( BigScroll(currview->getwidth()) );
      else
         PanDown( BigScroll(currview->getheight()) );

   } else if (type == wxEVT_SCROLLWIN_THUMBTRACK) {
      int newpos = event.GetPosition();
      int amount = newpos - (orient == wxHORIZONTAL ? hthumb : vthumb);
      if (amount != 0) {
         TestAutoFit();
         if (currview->getmag() > 0) {
            // amount is in cells so convert to pixels
            amount = amount << currview->getmag();
         }
         if (orient == wxHORIZONTAL) {
            hthumb = newpos;
            currview->move(amount, 0);
            // don't call UpdateEverything here because it calls UpdateScrollBars
            Refresh(false, NULL);
            // don't Update() immediately -- more responsive, especially on X11
         } else {
            vthumb = newpos;
            currview->move(0, amount);
            // don't call UpdateEverything here because it calls UpdateScrollBars
            Refresh(false, NULL);
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
      UpdateScrollBars();
      inscript = true;
   }

   #ifdef __WXGTK__
      if (type != wxEVT_SCROLLWIN_THUMBTRACK) {
         // avoid next scroll event
         ignorescroll = true;
      }
   #endif
}

// -----------------------------------------------------------------------------

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
   // create viewport for displaying patterns;
   // the initial size is not important because SetViewSize will change it
   currview = new viewport(10, 10);
   if (currview == NULL) Fatal(_("Failed to create viewport!"));

   dragtimer = new wxTimer(this, ID_DRAG_TIMER);
   if (dragtimer == NULL) Fatal(_("Failed to create drag timer!"));
   
   // avoid erasing background on GTK+
   SetBackgroundStyle(wxBG_STYLE_CUSTOM);

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

// -----------------------------------------------------------------------------

// destroy the viewport window
PatternView::~PatternView()
{
   if (currview) delete currview;
   if (dragtimer) delete dragtimer;

   #ifndef __WXMAC__
      if (viewbitmap) delete viewbitmap;
   #endif
}
