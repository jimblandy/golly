                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2005 Andrew Trevorrow and Tomas Rokicki.

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

// for compilers that support precompilation
#include "wx/wxprec.h"

// for all others, include the necessary headers
#ifndef WX_PRECOMP
   #include "wx/wx.h"
#endif

#include "wx/image.h"      // for wxImage

#include "bigint.h"
#include "lifealgo.h"
#include "viewport.h"

#include "wxgolly.h"       // for mainptr, viewptr, statusptr
#include "wxutils.h"       // for Warning, Fatal, FillRect
#include "wxprefs.h"       // for blackcells, showgridlines, mingridmag
#include "wxmain.h"        // for mainptr->...
#include "wxstatus.h"      // for statusptr->...
#include "wxview.h"        // for viewptr->...
#include "wxrender.h"

// -----------------------------------------------------------------------------

// current device context for viewport;
// used in wx_render routines so make this static data
wxDC *currdc;

// current viewport width and height (needed in DrawStretchedBitmap)
int currwd, currht;

// bitmap for drawing magnified cells (see DrawStretchedBitmap)
wxBitmap magmap;
const int MAGSIZE = 256;
wxUint16 magarray[MAGSIZE * MAGSIZE / 16];
unsigned char* magbuf = (unsigned char *)magarray;

// this lookup table magnifies bits in a given byte by a factor of 2;
// it assumes input and output are in XBM format (bits in each byte are reversed)
// because that's what wxWidgets requires when creating a monochrome bitmap
wxUint16 Magnify2[256];

// pens for drawing grid lines
wxPen pen_ltgray     (wxColour(0xD0,0xD0,0xD0));
wxPen pen_dkgray     (wxColour(0xA0,0xA0,0xA0));
wxPen pen_verydark   (wxColour(0x40,0x40,0x40));
wxPen pen_notsodark  (wxColour(0x70,0x70,0x70));

// selection image (initialized in InitDrawingData)
#ifndef __WXX11__                // wxX11's Blit doesn't support alpha channel
   wxImage selimage;             // semi-transparent overlay for selections
   wxBitmap *selbitmap;          // selection bitmap
   int selbitmapwd;              // width of selection bitmap
   int selbitmapht;              // height of selection bitmap
#endif

// -----------------------------------------------------------------------------

// initialize Magnify2 table; note that it swaps byte order if running on
// a little-endian processor
void InitMagnifyTable()
{
   int inttest = 1;
   unsigned char *p = (unsigned char *)&inttest;
   int i;
   for (i=0; i<8; i++)
      if (*p)
         Magnify2[1<<i] = 3 << (2 * i);
      else
         Magnify2[1<<i] = 3 << (2 * (i ^ 4));
   for (i=0; i<256; i++)
      if (i & (i-1))
         Magnify2[i] = Magnify2[i & (i-1)] + Magnify2[i & - i];
}

// -----------------------------------------------------------------------------

void InitDrawingData()
{
   InitMagnifyTable();
   
   #ifdef __WXX11__
      // wxX11's Blit doesn't support alpha channel
   #else
      // create semi-transparent selection image
      if ( !selimage.Create(1,1) )
         Fatal("Failed to create selection image!");
      selimage.SetRGB(0, 0, 75, 175, 0);     // darkish green
      selimage.SetAlpha();                   // add alpha channel
      if ( selimage.HasAlpha() ) {
         selimage.SetAlpha(0, 0, 128);       // 50% opaque
      } else {
         Warning("Selection image has no alpha channel!");
      }
      // scale selection image to viewport size and create selbitmap;
      // it's not strictly necessary to do this here because CheckSelectionImage
      // will do it, but it avoids delay when user makes their first selection
      int wd, ht;
      viewptr->GetClientSize(&wd, &ht);
      // wd or ht might be < 1 on Win/X11 platforms
      if (wd < 1) wd = 1;
      if (ht < 1) ht = 1;
      selimage.Rescale(wd, ht);
      selbitmap = new wxBitmap(selimage);
      if (selbitmap == NULL) Warning("Not enough memory for selection image!");
      selbitmapwd = wd;
      selbitmapht = ht;
   #endif
}

// -----------------------------------------------------------------------------

void DestroyDrawingData()
{
   #ifndef __WXX11__
      selimage.Destroy();
      if (selbitmap) delete selbitmap;
   #endif

   pen_ltgray.~wxPen();
   pen_dkgray.~wxPen();
   pen_verydark.~wxPen();
   pen_notsodark.~wxPen();
}

// -----------------------------------------------------------------------------

void CheckSelectionImage(int viewwd, int viewht)
{
   #ifdef __WXX11__
      // avoid compiler warnings
      if ( viewwd == viewht ) {}
   #else
      // wxX11's Blit doesn't support alpha channel
      if ( viewwd != selbitmapwd || viewht != selbitmapht ) {
         // rescale selection image and create new bitmap
         selimage.Rescale(viewwd, viewht);
         if (selbitmap) delete selbitmap;
         selbitmap = new wxBitmap(selimage);
         // don't check if selbitmap is NULL here -- done in DrawSelection
         selbitmapwd = viewwd;
         selbitmapht = viewht;
      }
   #endif
}

// -----------------------------------------------------------------------------

void DrawSelection(wxDC &dc, wxRect &rect)
{
   #ifdef __WXX11__
      // wxX11's Blit doesn't support alpha channel so we just invert rect
      dc.Blit(rect.x, rect.y, rect.width, rect.height, &dc, rect.x, rect.y, wxINVERT);
   #else
      if (selbitmap) {
         // draw semi-transparent green rect
         wxMemoryDC memDC;
         memDC.SelectObject(*selbitmap);
         dc.Blit(rect.x, rect.y, rect.width, rect.height, &memDC, 0, 0, wxCOPY, true);
      } else {
         // probably not enough memory
         wxBell();
      }
   #endif
}

// -----------------------------------------------------------------------------

void DrawPasteRect(wxDC &dc)
{
   dc.SetPen(*wxRED_PEN);
   dc.SetBrush(*wxTRANSPARENT_BRUSH);

   dc.DrawRectangle(viewptr->pasterect);
   
   dc.SetFont(*statusptr->GetStatusFont());
   dc.SetBackgroundMode(wxSOLID);
   dc.SetTextForeground(*wxRED);
   dc.SetTextBackground(*wxWHITE);

   const char *pmodestr = GetPasteMode();
   int pmodex = viewptr->pasterect.x + 2;
   int pmodey = viewptr->pasterect.y - 4;
   dc.DrawText(pmodestr, pmodex, pmodey - statusptr->GetTextAscent());
   
   dc.SetBrush(wxNullBrush);
   dc.SetPen(wxNullPen);
}

// -----------------------------------------------------------------------------

// magnify given bitmap by pmag (2, 4, ... 2^MAX_MAG);
// called from wx_render::blit so best if this is NOT a PatternView method
void DrawStretchedBitmap(wxDC &dc, int xoff, int yoff, int *bmdata, int bmsize, int pmag)
{
   int blocksize, magsize, rowshorts, numblocks, numshorts, numbytes;
   int rowbytes = bmsize / 8;
   int row, col;                    // current block
   int xw, yw;                      // window pos of scaled block's top left corner

   // try to process bmdata in square blocks of size MAGSIZE/pmag so each
   // magnified block is MAGSIZE x MAGSIZE
   blocksize = MAGSIZE / pmag;
   magsize = MAGSIZE;
   if (blocksize > bmsize) {
      blocksize = bmsize;
      magsize = bmsize * pmag;      // only use portion of magarray
   }
   rowshorts = magsize / 16;
   numbytes = rowshorts * 2;

   // pmag must be <= numbytes so numshorts (see below) will be > 0
   if (pmag > numbytes) {
      // this should never happen if max pmag is 16 (MAX_MAG = 4) and min bmsize is 64
      Fatal("DrawStretchedBitmap cannot magnify by this amount!");
   }
   
   // nicer to have gaps between cells at scales > 1:2
   wxUint16 gapmask = 0;
   if ( (pmag > 2 && pmag < (1 << mingridmag)) ||
        (pmag >= (1 << mingridmag) && !showgridlines) ) {
      // we use 7/7F rather than E/FE because of XBM bit reversal
      if (pmag == 4) {
         gapmask = 0x7777;
      } else if (pmag == 8) {
         gapmask = 0x7F7F;
      } else if (pmag == 16) {
         unsigned char *p = (unsigned char *)&gapmask;
         gapmask = 0xFF7F;
         // swap byte order if little-endian processor
         if (*p != 0xFF) gapmask = 0x7FFF;
      }
   }

   yw = yoff;
   numblocks = bmsize / blocksize;
   for ( row = 0; row < numblocks; row++ ) {
      xw = xoff;
      for ( col = 0; col < numblocks; col++ ) {
         if ( xw < currwd && xw + magsize >= 0 &&
              yw < currht && yw + magsize >= 0 ) {
            // some part of magnified block will be visible;
            // set bptr to address of byte at top left corner of current block
            unsigned char *bptr = (unsigned char *)bmdata + (row * blocksize * rowbytes) +
                                                            (col * blocksize / 8);
            
            int rowindex = 0;       // first row in magmap
            int i, j;
            for ( i = 0; i < blocksize; i++ ) {
               // use lookup table to convert bytes in bmdata to 16-bit ints in magmap
               numshorts = numbytes / pmag;
               for ( j = 0; j < numshorts; j++ ) {
                  magarray[rowindex + j] = Magnify2[ bptr[j] ];
               }
               while (numshorts < rowshorts) {
                  // stretch completed bytes in current row starting from right end
                  unsigned char *p = (unsigned char *)&magarray[rowindex + numshorts];
                  for ( j = numshorts * 2 - 1; j >= 0; j-- ) {
                     p--;
                     magarray[rowindex + j] = Magnify2[ *p ];
                  }
                  numshorts *= 2;
               }
               if (gapmask > 0) {
                  // erase pixel at right edge of each cell
                  for ( j = 0; j < rowshorts; j++ )
                     magarray[rowindex + j] &= gapmask;
                  // duplicate current magmap row pmag-2 times
                  for ( j = 2; j < pmag; j++ ) {
                     memcpy(&magarray[rowindex + rowshorts], &magarray[rowindex], rowshorts*2);
                     rowindex += rowshorts;
                  }
                  rowindex += rowshorts;
                  // erase pixel at bottom edge of each cell
                  memset(&magarray[rowindex], 0, rowshorts*2);
               } else {
                  // duplicate current magmap row pmag-1 times
                  for ( j = 1; j < pmag; j++ ) {
                     memcpy(&magarray[rowindex + rowshorts], &magarray[rowindex], rowshorts*2);
                     rowindex += rowshorts;
                  }
               }
               rowindex += rowshorts;     // start of next row in magmap
               bptr += rowbytes;          // start of next row in current block
            }
            
            magmap = wxBitmap((const char *)magbuf, magsize, magsize, 1);
            dc.DrawBitmap(magmap, xw, yw, false);
         }
         xw += magsize;     // across to next block
      }
      yw += magsize;     // down to next block
   }
}

// -----------------------------------------------------------------------------

void DrawGridLines(wxDC &dc, wxRect &r, viewport &currview)
{
   int pmag = 1 << currview.getmag();
   int h, v, i, topbold, leftbold;

   if (showboldlines) {
      // ensure that origin cell stays next to bold lines;
      // ie. bold lines scroll when pattern is scrolled
      pair<bigint, bigint> lefttop = currview.at(0, 0);
      leftbold = lefttop.first.mod_smallint(boldspacing);
      topbold = lefttop.second.mod_smallint(boldspacing);
      if (viewptr->originx != bigint::zero) {
         leftbold -= viewptr->originx.mod_smallint(boldspacing);
      }
      if (viewptr->originy != bigint::zero) {
         topbold -= viewptr->originy.mod_smallint(boldspacing);
      }
      if (mathcoords) topbold--;   // show origin cell above bold line
   } else {
      // avoid spurious gcc warning
      topbold = leftbold = 0;
   }

   // draw all plain lines first
   if (blackcells) {
      dc.SetPen(pen_ltgray);
   } else {
      dc.SetPen(pen_verydark);
   }   
   i = showboldlines ? topbold : 1;
   v = -1;
   while (true) {
      v += pmag;
      if (v >= r.height) break;
      if (showboldlines) i++;
      if (i % boldspacing != 0 && v >= r.y && v < r.y + r.height)
         dc.DrawLine(r.x, v, r.GetRight() + 1, v);
   }
   i = showboldlines ? leftbold : 1;
   h = -1;
   while (true) {
      h += pmag;
      if (h >= r.width) break;
      if (showboldlines) i++;
      if (i % boldspacing != 0 && h >= r.x && h < r.x + r.width)
         dc.DrawLine(h, r.y, h, r.GetBottom() + 1);
   }

   if (showboldlines) {
      // overlay bold lines
      if (blackcells) {
         dc.SetPen(pen_dkgray);
      } else {
         dc.SetPen(pen_notsodark);
      }
      i = topbold;
      v = -1;
      while (true) {
         v += pmag;
         if (v >= r.height) break;
         i++;
         if (i % boldspacing == 0 && v >= r.y && v < r.y + r.height)
            dc.DrawLine(r.x, v, r.GetRight() + 1, v);
      }
      i = leftbold;
      h = -1;
      while (true) {
         h += pmag;
         if (h >= r.width) break;
         i++;
         if (i % boldspacing == 0 && h >= r.x && h < r.x + r.width)
            dc.DrawLine(h, r.y, h, r.GetBottom() + 1);
      }
   }
   
   dc.SetPen(*wxBLACK_PEN);
}

// -----------------------------------------------------------------------------

class wx_render : public liferender
{
public:
   wx_render() {}
   virtual ~wx_render() {}
   virtual void killrect(int x, int y, int w, int h);
   virtual void blit(int x, int y, int w, int h, int *bm, int bmscale=1);
};

void wx_render::killrect(int x, int y, int w, int h)
{
   if (w <= 0 || h <= 0) return;
   wxRect r = wxRect(x, y, w, h);
   #if 0
      // use a different pale color each time to see any probs
      wxBrush *randbrush = new wxBrush(wxColour((rand()&127)+128,
                                                (rand()&127)+128,
                                                (rand()&127)+128));
      FillRect(*currdc, r, *randbrush);
      delete randbrush;
   #else
      FillRect(*currdc, r, blackcells ? *wxWHITE_BRUSH : *wxBLACK_BRUSH);
   #endif
}

void wx_render::blit(int x, int y, int w, int h, int *bmdata, int bmscale)
{
   if (bmscale == 1) {
      wxBitmap bmap = wxBitmap((const char *)bmdata, w, h, 1);
      currdc->DrawBitmap(bmap, x, y);
   } else {
      // stretch bitmap by bmscale
      DrawStretchedBitmap(*currdc, x, y, bmdata, w / bmscale, bmscale);
   }
}

wx_render renderer;     // create instance

// -----------------------------------------------------------------------------

void DrawView(wxDC &dc, viewport &currview)
{
   currdc = &dc;     // for use in blit and killrect

   // set foreground and background colors for DrawBitmap calls
   #ifdef __WXMSW__
   // use opposite bit meaning on Windows -- sigh
   if ( !blackcells ) {
   #else
   if ( blackcells ) {
   #endif
      dc.SetTextForeground(*wxBLACK);
      dc.SetTextBackground(*wxWHITE);
   } else {
      dc.SetTextForeground(*wxWHITE);
      dc.SetTextBackground(*wxBLACK);
   }

   wxRect r;

   if (mainptr->nopattupdate) {
      // don't update pattern, just fill background
      r = wxRect(0, 0, currview.getwidth(), currview.getheight());
      FillRect(dc, r, blackcells ? *wxWHITE_BRUSH : *wxBLACK_BRUSH);
   } else {
      // update the pattern via a sequence of blit and killrect calls;
      // set currwd and currht for use in DrawStretchedBitmap
      currwd = currview.getwidth();
      currht = currview.getheight();
      curralgo->draw(currview, renderer);
   }

   if ( viewptr->GridVisible() ) {
      r = wxRect(0, 0, currview.getwidth(), currview.getheight());
      DrawGridLines(dc, r, currview);
   }
   
   if ( viewptr->SelectionVisible(&r) ) {
      CheckSelectionImage(currview.getwidth(), currview.getheight());
      DrawSelection(dc, r);
   }
   
   if ( viewptr->waitingforclick && viewptr->pasterect.width > 0 ) {
      DrawPasteRect(dc);
   }
}
