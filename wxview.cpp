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

#include "wxgolly.h"       // for mainptr, statusptr
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
   currlayer->view->unzoom();
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

// zoom in so that central cell stays central
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

   bigint newx = currlayer->selright;
   newx -= currlayer->selleft;
   newx += bigint::one;
   newx.div2();
   newx += currlayer->selleft;

   bigint newy = currlayer->selbottom;
   newy -= currlayer->seltop;
   newy += bigint::one;
   newy.div2();
   newy += currlayer->seltop;

   int mag = MAX_MAG;
   while (true) {
      currlayer->view->setpositionmag(newx, newy, mag);
      if ( currlayer->view->contains(currlayer->selleft, currlayer->seltop) &&
           currlayer->view->contains(currlayer->selright, currlayer->selbottom) )
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

void PatternView::ToggleGridLines()
{
   showgridlines = !showgridlines;
   if (currlayer->view->getmag() >= mingridmag)
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
         #ifdef __WXMAC__
            // wxMac bug??? need this to fix probs after toggling status/tool bar
            wxSetCursor(*currlayer->curs);
         #endif
         SetCursor(*currlayer->curs);
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

void PatternView::SetPosMag(const bigint &x, const bigint &y, int mag)
{
   currlayer->view->setpositionmag(x, y, mag);
}

// -----------------------------------------------------------------------------

void PatternView::GetPos(bigint &x, bigint &y)
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

int PatternView::CellVisible(const bigint &x, const bigint &y)
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
   SetScrollbar(wxHORIZONTAL, hthumb, viewwd, thumbrange * viewwd, true);
   SetScrollbar(wxVERTICAL, vthumb, viewht, thumbrange * viewht, true);
}

// -----------------------------------------------------------------------------

void PatternView::ProcessKey(int key, bool shiftdown)
{
   mainptr->showbanner = false;
   switch (key) {
      case WXK_LEFT:    PanLeft( SmallScroll(currlayer->view->getwidth()) ); break;
      case WXK_RIGHT:   PanRight( SmallScroll(currlayer->view->getwidth()) ); break;
      case WXK_UP:      PanUp( SmallScroll(currlayer->view->getheight()) ); break;
      case WXK_DOWN:    PanDown( SmallScroll(currlayer->view->getheight()) ); break;

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

      case '\\':  ToggleLayerBar(); break;
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
   currlayer->algo->endofpattern();
   currlayer->savestart = true;

   // update status bar
   if (mainptr->StatusVisible()) {
      statusptr->Refresh(false);
   }

   if (numlayers > 1 && stacklayers) {
      // update all layers; this is rather slow but most people won't be
      // drawing cells when all layers are displayed (too confusing)
      Refresh(false);
      Update();
   }
}

// -----------------------------------------------------------------------------

void PatternView::DrawOneCell(int cx, int cy, wxDC &dc)
{
   if (numlayers > 1 && stacklayers) {
      // drawing must be done via Update in ShowDrawing
      return;
   }

   int cellsize = 1 << currlayer->view->getmag();

   // convert given cell coords to view coords
   pair<bigint, bigint> lefttop = currlayer->view->at(0, 0);
   wxCoord x = (cx - lefttop.first.toint()) * cellsize;
   wxCoord y = (cy - lefttop.second.toint()) * cellsize;
   
   if (cellsize > 2) {
      cellsize--;       // allow for gap between cells
   }
   dc.DrawRectangle(x, y, cellsize, cellsize);
   
   // overlay selection image if cell is within selection
   if ( SelectionExists() &&
        cx >= currlayer->selleft.toint() && cx <= currlayer->selright.toint() &&
        cy >= currlayer->seltop.toint() && cy <= currlayer->selbottom.toint() ) {
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

   cellx = cellpos.first.toint();
   celly = cellpos.second.toint();
   drawstate = 1 - currlayer->algo->getcell(cellx, celly);
   currlayer->algo->setcell(cellx, celly, drawstate);

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
   pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
   if ( currlayer->view->getmag() < 0 ||
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
      
      lifealgo *curralgo = currlayer->algo;
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
   if ( yclick <= currlayer->seltop && xclick <= currlayer->selleft ) {
      // click is in or outside top left corner
      currlayer->seltop = yclick;
      currlayer->selleft = xclick;
      anchory = currlayer->selbottom;
      anchorx = currlayer->selright;

   } else if ( yclick <= currlayer->seltop && xclick >= currlayer->selright ) {
      // click is in or outside top right corner
      currlayer->seltop = yclick;
      currlayer->selright = xclick;
      anchory = currlayer->selbottom;
      anchorx = currlayer->selleft;

   } else if ( yclick >= currlayer->selbottom && xclick >= currlayer->selright ) {
      // click is in or outside bottom right corner
      currlayer->selbottom = yclick;
      currlayer->selright = xclick;
      anchory = currlayer->seltop;
      anchorx = currlayer->selleft;

   } else if ( yclick >= currlayer->selbottom && xclick <= currlayer->selleft ) {
      // click is in or outside bottom left corner
      currlayer->selbottom = yclick;
      currlayer->selleft = xclick;
      anchory = currlayer->seltop;
      anchorx = currlayer->selright;
   
   } else if (yclick <= currlayer->seltop) {
      // click is in or above top edge
      forcev = true;
      currlayer->seltop = yclick;
      anchory = currlayer->selbottom;
   
   } else if (yclick >= currlayer->selbottom) {
      // click is in or below bottom edge
      forcev = true;
      currlayer->selbottom = yclick;
      anchory = currlayer->seltop;
   
   } else if (xclick <= currlayer->selleft) {
      // click is in or left of left edge
      forceh = true;
      currlayer->selleft = xclick;
      anchorx = currlayer->selright;
   
   } else if (xclick >= currlayer->selright) {
      // click is in or right of right edge
      forceh = true;
      currlayer->selright = xclick;
      anchorx = currlayer->selleft;
   
   } else {
      // click is somewhere inside selection
      double wd = currlayer->selright.todouble() - currlayer->selleft.todouble() + 1.0;
      double ht = currlayer->selbottom.todouble() - currlayer->seltop.todouble() + 1.0;
      double onethirdx = currlayer->selleft.todouble() + wd / 3.0;
      double twothirdx = currlayer->selleft.todouble() + wd * 2.0 / 3.0;
      double onethirdy = currlayer->seltop.todouble() + ht / 3.0;
      double twothirdy = currlayer->seltop.todouble() + ht * 2.0 / 3.0;
      double midy = currlayer->seltop.todouble() + ht / 2.0;
      double x = xclick.todouble();
      double y = yclick.todouble();
      
      if ( y < onethirdy && x < onethirdx ) {
         // click is near top left corner
         currlayer->seltop = yclick;
         currlayer->selleft = xclick;
         anchory = currlayer->selbottom;
         anchorx = currlayer->selright;
      
      } else if ( y < onethirdy && x > twothirdx ) {
         // click is near top right corner
         currlayer->seltop = yclick;
         currlayer->selright = xclick;
         anchory = currlayer->selbottom;
         anchorx = currlayer->selleft;
   
      } else if ( y > twothirdy && x > twothirdx ) {
         // click is near bottom right corner
         currlayer->selbottom = yclick;
         currlayer->selright = xclick;
         anchory = currlayer->seltop;
         anchorx = currlayer->selleft;
   
      } else if ( y > twothirdy && x < onethirdx ) {
         // click is near bottom left corner
         currlayer->selbottom = yclick;
         currlayer->selleft = xclick;
         anchory = currlayer->seltop;
         anchorx = currlayer->selright;

      } else if ( x < onethirdx ) {
         // click is near middle of left edge
         forceh = true;
         currlayer->selleft = xclick;
         anchorx = currlayer->selright;

      } else if ( x > twothirdx ) {
         // click is near middle of right edge
         forceh = true;
         currlayer->selright = xclick;
         anchorx = currlayer->selleft;

      } else if ( y < midy ) {
         // click is below middle section of top edge
         forcev = true;
         currlayer->seltop = yclick;
         anchory = currlayer->selbottom;
      
      } else {
         // click is above middle section of bottom edge
         forcev = true;
         currlayer->selbottom = yclick;
         anchory = currlayer->seltop;
      }
   }
}

// -----------------------------------------------------------------------------

void PatternView::StartSelectingCells(int x, int y, bool shiftdown)
{
   pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
   anchorx = cellpos.first;
   anchory = cellpos.second;

   // save original selection so it can be restored if user hits escape
   origtop = currlayer->seltop;
   origbottom = currlayer->selbottom;
   origleft = currlayer->selleft;
   origright = currlayer->selright;

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

   pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
   if (!forcev) {
      if (cellpos.first <= anchorx) {
         currlayer->selleft = cellpos.first;
         currlayer->selright = anchorx;
      } else {
         currlayer->selleft = anchorx;
         currlayer->selright = cellpos.first;
      }
   }
   if (!forceh) {
      if (cellpos.second <= anchory) {
         currlayer->seltop = cellpos.second;
         currlayer->selbottom = anchory;
      } else {
         currlayer->seltop = anchory;
         currlayer->selbottom = cellpos.second;
      }
   }

   if ( currlayer->seltop != prevtop || currlayer->selbottom != prevbottom ||
        currlayer->selleft != prevleft || currlayer->selright != prevright ) {
      // selection has changed
      DisplaySelectionSize();
      
      // allow mouse interaction if script is running
      bool saveinscript = inscript;
      inscript = false;
      mainptr->UpdatePatternAndStatus();
      inscript = saveinscript;
      
      prevtop = currlayer->seltop;
      prevbottom = currlayer->selbottom;
      prevleft = currlayer->selleft;
      prevright = currlayer->selright;
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

void PatternView::ProcessClick(int x, int y, bool shiftdown)
{
   // user has clicked somewhere in viewport   
   if (currlayer->curs == curs_pencil) {
      if (inscript) {
         // statusptr->ErrorMessage does nothing if inscript is true
         Warning(_("Drawing is not allowed while a script is running."));
         return;
      }
      if (mainptr->generating) {
         statusptr->ErrorMessage(_("Drawing is not allowed while a pattern is generating."));
         return;
      }
      if (currlayer->view->getmag() < 0) {
         statusptr->ErrorMessage(_("Drawing is not allowed at scales greater than 1 cell per pixel."));
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

// event table and handlers:

BEGIN_EVENT_TABLE(PatternView, wxWindow)
   EVT_PAINT            (                 PatternView::OnPaint)
   EVT_SIZE             (                 PatternView::OnSize)
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
   if ( wd != currlayer->view->getwidth() || ht != currlayer->view->getheight() ) {
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
           (numlayers > 1 && stacklayers) ) {
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

void PatternView::SetViewSize()
{
   int wd, ht;
   GetClientSize(&wd, &ht);
   // wd or ht might be < 1 on Win/X11 platforms
   if (wd < 1) wd = 1;
   if (ht < 1) ht = 1;
   
   ResizeLayers(wd, ht);
   
   // only autofit when generating
   if (autofit && mainptr && mainptr->generating)
      currlayer->algo->fit(*currlayer->view, 0);
}

// -----------------------------------------------------------------------------

void PatternView::OnSize(wxSizeEvent& event)
{
   // resize viewports in all layers
   SetViewSize();   
   event.Skip();
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
   currlayer->seltop = origtop;
   currlayer->selbottom = origbottom;
   currlayer->selleft = origleft;
   currlayer->selright = origright;
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
   if (currlayer->curs == curs_zoomin) {
      ZoomOutPos(x, y);
   } else if (currlayer->curs == curs_zoomout) {
      ZoomInPos(x, y);
   } else {
      /* let all other cursor modes advance clicked region -- probably unwise
      pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
      if ( cellpos.first < currlayer->selleft || cellpos.first > currlayer->selright ||
           cellpos.second < currlayer->seltop || cellpos.second > currlayer->selbottom ) {
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
            Refresh(false);
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
            Refresh(false);
            // don't Update() immediately -- more responsive, especially on X11
         } else {
            vthumb = newpos;
            currlayer->view->move(0, amount);
            // don't call UpdateEverything here because it calls UpdateScrollBars
            Refresh(false);
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
}

// -----------------------------------------------------------------------------

// destroy the viewport window
PatternView::~PatternView()
{
   if (dragtimer) delete dragtimer;

   #ifndef __WXMAC__
      if (viewbitmap) delete viewbitmap;
   #endif
}
