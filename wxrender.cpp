                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2007 Andrew Trevorrow and Tomas Rokicki.

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

/* -------------------- Some notes on Golly's display code ---------------------

The rectangular area used to display patterns is called the viewport.
It's represented by a window of class PatternView defined in wxview.h.
The global viewptr points to a PatternView window which is created near
the bottom of MainFrame::MainFrame() in wxmain.cpp.

Nearly all drawing in the viewport is done in this module.  The only other
places are in wxview.cpp: PatternView::StartDrawingCells() is called when
the pencil cursor is used to click a cell, and PatternView::DrawCells() is
called while the user drags the pencil cursor around to draw many cells.
These exceptions were done for performance reasons -- using update events
to do the drawing was just too slow.

The main rendering routine is DrawView() -- see the end of this module.
DrawView() is called from PatternView::OnPaint(), the update event handler
for the viewport window.  Update events are generated automatically by the
wxWidgets event dispatcher, or they can be generated manually by calling
MainFrame::UpdateEverything() or MainFrame::UpdatePatternAndStatus().

DrawView() does the following tasks:

- Calls currlayer->algo->draw() to draw the current pattern.  It passes
  in renderer, an instance of wx_render (derived from liferender) which
  has 2 methods: killrect() draws a rectangular area of dead cells,
  and blit() draws a monochrome bitmap with at least one live cell.
  
  Note that currlayer->algo->draw() does all the hard work of figuring
  out which parts of the viewport are dead and building all the bitmaps
  for the live parts.  The bitmaps contain suitably shrunken images
  when the scale is < 1:1 (ie. mag < 0).  At scales 1:2 and above,
  DrawStretchedBitmap() is called from wx_render::blit().
  
  Each lifealgo needs to implement its own draw() method.  Currently
  we have hlifealgo::draw() in hlifedraw.cpp and qlifealgo::draw()
  in qlifedraw.cpp.

- Calls DrawGridLines() to overlay grid lines if they are visible.

- Calls DrawSelection() to overlay a translucent selection rectangle
  if a selection exists and any part of it is visible.

- Calls DrawStackedLayers() to overlay multiple layers using the current
  layer's scale and location.

- If the user is doing a paste, CheckPasteImage() creates a temporary
  viewport (tempview) and draws the paste pattern (stored in pastealgo)
  into a masked bitmap which is then used by DrawPasteImage().

Potential optimizations:

- Every time DrawView() is called it draws the entire viewport, so
  there's plenty of room for improvement!  One fairly simple change
  would be to maintain a list of rects seen by wx_render::killrect()
  and only draw new rects when necessary (the list would need to be
  emptied after various UI changes).  Other more complicated schemes
  that only do incremental drawing might also be worth trying.

- The bitmap data passed into wx_render::blit() is an XBM image where
  the bit order in each byte is reversed.  This is required by the
  wxBitmap constructor when creating monochrome bitmaps.  Presumably
  the bit order needs to be reversed back before it can be used with
  the Mac's native blitter (and Windows?) so it would probably be good
  to avoid the bit reversal and call the native blitter directly.

Other points of interest:

- The decision to use buffered drawing is made in PatternView::OnPaint().
  It's never used on Mac OS X or GTK+ 2.0 because windows on those systems
  are automatically buffered.  To avoid flicker on Windows, buffering is
  always used if:
  - the user is doing a paste;
  - the grid lines are visible;
  - the selection is visible;
  - multiple layers are being displayed.

- Change the "#if 0" to "#if 1" in wx_render::killrect() to see randomly
  colored rects.  Gives an insight into how lifealgo::draw() works.

- The viewport window is actually the right-hand pane of a wxSplitWindow.
  The left-hand pane is a directory control (wxGenericDirCtrl) that
  displays the user's preferred pattern or script folder.  This is all
  handled in wxmain.cpp.

----------------------------------------------------------------------------- */

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
   #include "wx/wx.h"      // for all others include the necessary headers
#endif

#ifndef __WXX11__
   #include "wx/rawbmp.h"  // for wxAlphaPixelData
#endif

#include "bigint.h"
#include "lifealgo.h"
#include "viewport.h"

#include "wxgolly.h"       // for viewptr, bigview, statusptr
#include "wxutils.h"       // for Warning, Fatal, FillRect
#include "wxprefs.h"       // for swapcolors, showgridlines, mingridmag, etc
#include "wxstatus.h"      // for statusptr->...
#include "wxview.h"        // for viewptr->...
#include "wxlayer.h"       // currlayer, GetLayer, etc
#include "wxrender.h"

// -----------------------------------------------------------------------------

// bitmap memory for drawing magnified cells (see DrawStretchedBitmap)
const int MAGSIZE = 256;
wxUint16 magarray[MAGSIZE * MAGSIZE / 16];
unsigned char* magbuf = (unsigned char*)magarray;

// this lookup table magnifies bits in a given byte by a factor of 2;
// it assumes input and output are in XBM format (bits in each byte are reversed)
// because that's what wxWidgets requires when creating a monochrome bitmap
wxUint16 Magnify2[256];

// globals used in wx_render routines
wxDC* currdc;              // current device context for viewport
int currwd, currht;        // current width and height of viewport
wxBrush* killbrush;        // brush used in killrect

// for drawing multiple layers
wxBitmap* layerbitmap = NULL;    // layer bitmap
int layerwd = -1;                // width of layer bitmap
int layerht = -1;                // height of layer bitmap

// for drawing translucent selection (initialized in InitDrawingData)
#ifdef __WXX11__
   // wxX11 doesn't support alpha channel
#else
   int selwd;                 // width of selection bitmap
   int selht;                 // height of selection bitmap
#endif
wxBitmap* selbitmap = NULL;   // selection bitmap (if NULL then inversion is used)
wxBitmap* graybitmap = NULL;  // for inactive selections when drawing multiple layers

// for drawing paste pattern (initialized in CreatePasteImage)
wxBitmap* pastebitmap;     // paste bitmap
int pimagewd;              // width of paste image
int pimageht;              // height of paste image
int prectwd;               // must match viewptr->pasterect.width
int prectht;               // must match viewptr->pasterect.height
int pastemag;              // must match current viewport's scale
int cvwd, cvht;            // must match current viewport's width and height
paste_location pasteloc;   // must match plocation
lifealgo* pastealgo;       // universe containing paste pattern
wxRect pastebbox;          // bounding box in cell coords (not necessarily minimal)

// for drawing tile borders
const wxColor dkgray(96, 96, 96);
const wxColor ltgray(224, 224, 224);
const wxColor brightgreen(0, 255, 0);

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

#ifndef __WXX11__

void SetSelectionPixels(wxBitmap* bitmap, const wxColor* color)
{
   // set color and alpha of pixels in given bitmap
   wxAlphaPixelData data(*bitmap, wxPoint(0,0), wxSize(selwd,selht));
   if (data) {
      int alpha = 128;     // 50% opaque
      
      #ifdef __WXMSW__
         // premultiply the RGB values on Windows
         int r = color->Red() * alpha / 255;
         int g = color->Green() * alpha / 255;
         int b = color->Blue() * alpha / 255;
      #else
         int r = color->Red();
         int g = color->Green();
         int b = color->Blue();
      #endif
      
      data.UseAlpha();
      wxAlphaPixelData::Iterator p(data);
      for ( int y = 0; y < selht; y++ ) {
         wxAlphaPixelData::Iterator rowStart = p;
         for ( int x = 0; x < selwd; x++ ) {
            p.Red()   = r;
            p.Green() = g;
            p.Blue()  = b;
            p.Alpha() = alpha;
            p++;
         }
         p = rowStart;
         p.OffsetY(data, 1);
      }
   }
}

#endif

// -----------------------------------------------------------------------------

void InitDrawingData()
{
   InitMagnifyTable();
   
   #ifdef __WXX11__
      // wxX11 doesn't support alpha channel
   #else
      // create translucent selection bitmap
      viewptr->GetClientSize(&selwd, &selht);
      // selwd or selht might be < 1 on Win/X11 platforms
      if (selwd < 1) selwd = 1;
      if (selht < 1) selht = 1;

      // use depth 32 so bitmap has an alpha channel
      selbitmap = new wxBitmap(selwd, selht, 32);
      if (selbitmap == NULL) {
         Warning(_("Not enough memory for selection bitmap!"));
      } else {
         SetSelectionPixels(selbitmap, selectrgb);
      }
      
      // create translucent gray bitmap for inactive selections
      graybitmap = new wxBitmap(selwd, selht, 32);
      if (graybitmap == NULL) {
         Warning(_("Not enough memory for gray bitmap!"));
      } else {
         SetSelectionPixels(graybitmap, wxLIGHT_GREY);
      }
   #endif
}

// -----------------------------------------------------------------------------

void DestroyDrawingData()
{
   if (layerbitmap) delete layerbitmap;
   if (selbitmap) delete selbitmap;
   if (graybitmap) delete graybitmap;
}

// -----------------------------------------------------------------------------

// called from wx_render::blit to magnify given bitmap by pmag (2, 4, ... 2^MAX_MAG)
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
      Fatal(_("DrawStretchedBitmap cannot magnify by this amount!"));
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
            
            int rowindex = 0;       // first row in magbuf
            int i, j;
            for ( i = 0; i < blocksize; i++ ) {
               // use lookup table to convert bytes in bmdata to 16-bit ints in magbuf
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
                  // duplicate current magbuf row pmag-2 times
                  for ( j = 2; j < pmag; j++ ) {
                     memcpy(&magarray[rowindex + rowshorts], &magarray[rowindex], rowshorts*2);
                     rowindex += rowshorts;
                  }
                  rowindex += rowshorts;
                  // erase pixel at bottom edge of each cell
                  memset(&magarray[rowindex], 0, rowshorts*2);
               } else {
                  // duplicate current magbuf row pmag-1 times
                  for ( j = 1; j < pmag; j++ ) {
                     memcpy(&magarray[rowindex + rowshorts], &magarray[rowindex], rowshorts*2);
                     rowindex += rowshorts;
                  }
               }
               rowindex += rowshorts;     // start of next row in magbuf
               bptr += rowbytes;          // start of next row in current block
            }
            
            wxBitmap magmap = wxBitmap((const char *)magbuf, magsize, magsize, 1);
            dc.DrawBitmap(magmap, xw, yw, false);
         }
         xw += magsize;     // across to next block
      }
      yw += magsize;     // down to next block
   }
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
   wxRect r(x, y, w, h);
   #if 0
      // use a different pale color each time to see any probs
      wxBrush randbrush(wxColor((rand()&127)+128,
                                (rand()&127)+128,
                                (rand()&127)+128));
      FillRect(*currdc, r, randbrush);
   #else
      FillRect(*currdc, r, *killbrush);
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

void CheckSelectionSize(int viewwd, int viewht)
{
   #ifdef __WXX11__
      // wxX11 doesn't support alpha channel
   #else
      if (viewwd != selwd || viewht != selht) {
         // resize selbitmap and graybitmap
         selwd = viewwd;
         selht = viewht;
         if (selbitmap) delete selbitmap;
         if (graybitmap) delete graybitmap;
         // use depth 32 so bitmaps have an alpha channel
         selbitmap = new wxBitmap(selwd, selht, 32);
         graybitmap = new wxBitmap(selwd, selht, 32);
         if (selbitmap) SetSelectionPixels(selbitmap, selectrgb);
         if (graybitmap) SetSelectionPixels(graybitmap, wxLIGHT_GREY);
      }
   #endif
}

// -----------------------------------------------------------------------------

void SetSelectionColor()
{
   #ifdef __WXX11__
      // wxX11 doesn't support alpha channel
   #else
      // selectrgb has changed
      if (selbitmap) SetSelectionPixels(selbitmap, selectrgb);
   #endif
}

// -----------------------------------------------------------------------------

void DrawSelection(wxDC &dc, wxRect &rect)
{
   if (selbitmap) {
      #ifdef __WXGTK__
         // wxGTK Blit doesn't support alpha channel
         if (selectrgb->Red() == 255 && selectrgb->Green() == 255 && selectrgb->Blue() == 255) {
            // use inversion for speed
            dc.Blit(rect.x, rect.y, rect.width, rect.height, &dc, rect.x, rect.y, wxINVERT);
         } else {
            dc.DrawBitmap(selbitmap->GetSubBitmap(rect), rect.x, rect.y, true);
            // using GetSubBitmap is much faster than clipping:
            // dc.SetClippingRegion(rect);
            // dc.DrawBitmap(*selbitmap, 0, 0, true);
            // dc.DestroyClippingRegion();
         }
      #else
         // Blit seems to be about 10% faster (on Mac at least)
         wxMemoryDC memdc;
         memdc.SelectObject(*selbitmap);
         dc.Blit(rect.x, rect.y, rect.width, rect.height, &memdc, 0, 0, wxCOPY, true);
      #endif
   } else {
      // no alpha channel so just invert rect
      dc.Blit(rect.x, rect.y, rect.width, rect.height, &dc, rect.x, rect.y, wxINVERT);
   }
}

// -----------------------------------------------------------------------------

void DrawInactiveSelection(wxDC &dc, wxRect &rect)
{
   if (graybitmap) {
      #ifdef __WXGTK__
         // wxGTK Blit doesn't support alpha channel
         if (selectrgb->Red() == 255 && selectrgb->Green() == 255 && selectrgb->Blue() == 255) {
            // use inversion for speed
            dc.Blit(rect.x, rect.y, rect.width, rect.height, &dc, rect.x, rect.y, wxINVERT);
         } else {
            dc.DrawBitmap(graybitmap->GetSubBitmap(rect), rect.x, rect.y, true);
         }
      #else
         // Blit seems to be about 10% faster (on Mac at least)
         wxMemoryDC memdc;
         memdc.SelectObject(*graybitmap);
         dc.Blit(rect.x, rect.y, rect.width, rect.height, &memdc, 0, 0, wxCOPY, true);
      #endif
   } else {
      // no alpha channel so just invert rect
      dc.Blit(rect.x, rect.y, rect.width, rect.height, &dc, rect.x, rect.y, wxINVERT);
   }
}

// -----------------------------------------------------------------------------

void CreatePasteImage(lifealgo *palgo, wxRect &bbox)
{
   pastealgo = palgo;      // save for use in CheckPasteImage
   pastebbox = bbox;       // ditto
   pastebitmap = NULL;
   prectwd = -1;           // force CheckPasteImage to update paste image
   prectht = -1;
   pimagewd = -1;          // force CheckPasteImage to rescale paste image
   pimageht = -1;
   pastemag = viewptr->GetMag();
}

// -----------------------------------------------------------------------------

void DestroyPasteImage()
{
   if (pastebitmap) {
      delete pastebitmap;
      pastebitmap = NULL;
   }
}

// -----------------------------------------------------------------------------

void MaskDeadPixels(wxBitmap* bitmap, int wd, int ht, int livealpha)
{
   #ifdef __WXX11__
      // wxX11 doesn't support alpha channel
      wxUnusedVar(wd);
      wxUnusedVar(ht);
      wxUnusedVar(livealpha);
      bitmap->SetMask( new wxMask(*bitmap,*deadrgb) );
   #else
      // access pixels in given bitmap and make all dead pixels 100% transparent
      // and use given alpha value for all live pixels
      wxAlphaPixelData data(*bitmap, wxPoint(0,0), wxSize(wd,ht));
      if (data) {
         int deadr = deadrgb->Red();
         int deadg = deadrgb->Green();
         int deadb = deadrgb->Blue();
   
         data.UseAlpha();
         wxAlphaPixelData::Iterator p(data);
         for ( int y = 0; y < ht; y++ ) {
            wxAlphaPixelData::Iterator rowStart = p;
            for ( int x = 0; x < wd; x++ ) {
               // get pixel color
               int r = p.Red();
               int g = p.Green();
               int b = p.Blue();
   
               // set alpha value depending on whether pixel is live or dead
               if (r == deadr && g == deadg && b == deadb) {
                  // make dead pixel 100% transparent
                  p.Red()   = 0;
                  p.Green() = 0;
                  p.Blue()  = 0;
                  p.Alpha() = 0;
               } else {
                  // live pixel
                  #ifdef __WXMSW__
                     // premultiply the RGB values on Windows
                     p.Red()   = r * livealpha / 255;
                     p.Green() = g * livealpha / 255;
                     p.Blue()  = b * livealpha / 255;
                  #else
                     // no change needed
                     // p.Red()   = r;
                     // p.Green() = g;
                     // p.Blue()  = b;
                  #endif
                  p.Alpha() = livealpha;
               }
   
               p++;
            }
            p = rowStart;
            p.OffsetY(data, 1);
         }
      }
   #endif
}

// -----------------------------------------------------------------------------

int PixelsToCells(int pixels) {
   // convert given # of screen pixels to corresponding # of cells
   if (pastemag >= 0) {
      int cellsize = 1 << pastemag;
      return (pixels + cellsize - 1) / cellsize;
   } else {
      // pastemag < 0; no need to worry about overflow
      return pixels << -pastemag;
   }
}

// -----------------------------------------------------------------------------

void CheckPasteImage()
{
   // paste image needs to be updated if pasterect size changed
   // or viewport size changed or cell colors changed or plocation changed
   if ( prectwd != viewptr->pasterect.width ||
        prectht != viewptr->pasterect.height ||
        cvwd != currlayer->view->getwidth() ||
        cvht != currlayer->view->getheight() ||
        pasteloc != plocation
      ) {
      prectwd = viewptr->pasterect.width;
      prectht = viewptr->pasterect.height;
      pastemag = currlayer->view->getmag();
      cvwd = currlayer->view->getwidth();
      cvht = currlayer->view->getheight();
      pasteloc = plocation;

      // calculate size of paste image; we could just set it to pasterect size
      // but that would be slow and wasteful for large pasterects, so we use
      // the following code (the only tricky bit is when plocation = Middle)
      int pastewd = prectwd;
      int pasteht = prectht;
      
      if (pastewd <= 2 || pasteht <= 2) {
         // no need to draw paste image because border lines will cover it
         if (pastebitmap) delete pastebitmap;
         pastebitmap = NULL;
         if (pimagewd != 1 || pimageht != 1) {
            pimagewd = 1;
            pimageht = 1;
         }
         return;
      }
      
      wxRect cellbox = pastebbox;
      if (pastewd > currlayer->view->getwidth() || pasteht > currlayer->view->getheight()) {
         if (plocation == Middle) {
            // temporary viewport may need to be TWICE size of current viewport
            if (pastewd > 2 * currlayer->view->getwidth())
               pastewd = 2 * currlayer->view->getwidth();
            if (pasteht > 2 * currlayer->view->getheight())
               pasteht = 2 * currlayer->view->getheight();
            if (pastemag > 0) {
               // make sure pastewd/ht don't have partial cells
               int cellsize = 1 << pastemag;
               if ((pastewd + 1) % cellsize > 0)
                  pastewd += cellsize - ((pastewd + 1) % cellsize);
               if ((pasteht + 1) % cellsize != 0)
                  pasteht += cellsize - ((pasteht + 1) % cellsize);
            }
            if (prectwd > pastewd) {
               // make sure prectwd - pastewd is an even number of *cells*
               // to simplify shifting in DrawPasteImage
               if (pastemag > 0) {
                  int cellsize = 1 << pastemag;
                  int celldiff = (prectwd - pastewd) / cellsize;
                  if (celldiff & 1) pastewd += cellsize;
               } else {
                  if ((prectwd - pastewd) & 1) pastewd++;
               }
            }
            if (prectht > pasteht) {
               // make sure prectht - pasteht is an even number of *cells*
               // to simplify shifting in DrawPasteImage
               if (pastemag > 0) {
                  int cellsize = 1 << pastemag;
                  int celldiff = (prectht - pasteht) / cellsize;
                  if (celldiff & 1) pasteht += cellsize;
               } else {
                  if ((prectht - pasteht) & 1) pasteht++;
               }
            }
         } else {
            // plocation is at a corner of pasterect so temporary viewport
            // may need to be size of current viewport
            if (pastewd > currlayer->view->getwidth())
               pastewd = currlayer->view->getwidth();
            if (pasteht > currlayer->view->getheight())
               pasteht = currlayer->view->getheight();
            if (pastemag > 0) {
               // make sure pastewd/ht don't have partial cells
               int cellsize = 1 << pastemag;
               int gap = 1;                        // gap between cells
               if (pastemag == 1) gap = 0;         // no gap at scale 1:2
               if ((pastewd + gap) % cellsize > 0)
                  pastewd += cellsize - ((pastewd + gap) % cellsize);
               if ((pasteht + gap) % cellsize != 0)
                  pasteht += cellsize - ((pasteht + gap) % cellsize);
            }
            cellbox.width = PixelsToCells(pastewd);
            cellbox.height = PixelsToCells(pasteht);
            if (plocation == TopLeft) {
               // show top left corner of pasterect
               cellbox.x = pastebbox.x;
               cellbox.y = pastebbox.y;
            } else if (plocation == TopRight) {
               // show top right corner of pasterect
               cellbox.x = pastebbox.x + pastebbox.width - cellbox.width;
               cellbox.y = pastebbox.y;
            } else if (plocation == BottomRight) {
               // show bottom right corner of pasterect
               cellbox.x = pastebbox.x + pastebbox.width - cellbox.width;
               cellbox.y = pastebbox.y + pastebbox.height - cellbox.height;
            } else { // plocation == BottomLeft
               // show bottom left corner of pasterect
               cellbox.x = pastebbox.x;
               cellbox.y = pastebbox.y + pastebbox.height - cellbox.height;
            }
         }
      }
      
      // delete old bitmap even if size hasn't changed
      if (pastebitmap) delete pastebitmap;
      pimagewd = pastewd;
      pimageht = pasteht;
      #ifdef __WXX11__
         // create a bitmap with screen depth
         pastebitmap = new wxBitmap(pimagewd, pimageht, -1);
      #else
         // create a bitmap with depth 32 so it has an alpha channel
         pastebitmap = new wxBitmap(pimagewd, pimageht, 32);
      #endif
      
      if (pastebitmap) {
         // create temporary viewport and draw pattern into pastebitmap
         // for later use in DrawPasteImage
         viewport tempview(pimagewd, pimageht);
         int midx, midy;
         if (pastemag > 0) {
            midx = cellbox.x + cellbox.width / 2;
            midy = cellbox.y + cellbox.height / 2;
         } else {
            midx = cellbox.x + (cellbox.width - 1) / 2;
            midy = cellbox.y + (cellbox.height - 1) / 2;
         }
         tempview.setpositionmag(midx, midy, pastemag);
         
         wxMemoryDC pattdc;
         pattdc.SelectObject(*pastebitmap);
         
         // set foreground and background colors for DrawBitmap calls
         #if (defined(__WXMAC__) && !wxCHECK_VERSION(2,7,2)) || \
             (defined(__WXMSW__) && !wxCHECK_VERSION(2,8,0))
            // use opposite meaning
            pattdc.SetTextForeground(*deadrgb);
            pattdc.SetTextBackground(*pastergb);
         #else
            pattdc.SetTextForeground(*pastergb);
            pattdc.SetTextBackground(*deadrgb);
         #endif

         // set brush color used in killrect
         killbrush = deadbrush;
         
         // temporarily turn off grid lines for DrawStretchedBitmap
         bool saveshow = showgridlines;
         showgridlines = false;
         
         currdc = &pattdc;
         currwd = tempview.getwidth();
         currht = tempview.getheight();
         pastealgo->draw(tempview, renderer);
         
         showgridlines = saveshow;
   
         // make dead pixels 100% transparent and live pixels 100% opaque
         MaskDeadPixels(pastebitmap, pimagewd, pimageht, 255);
         
      } else {
         // give some indication that pastebitmap could not be created
         wxBell();
      }
   }
}

// -----------------------------------------------------------------------------

void DrawPasteImage(wxDC &dc)
{
   if (pastebitmap) {
      // draw paste image
      wxRect r = viewptr->pasterect;
      if (r.width > pimagewd || r.height > pimageht) {
         // paste image is smaller than pasterect (which can't fit in viewport)
         // so shift image depending on plocation
         switch (plocation) {
            case TopLeft:
               // no need to do any shifting
               break;
            case TopRight:
               // shift image to top right corner of pasterect
               r.x += r.width - pimagewd;
               break;
            case BottomRight:
               // shift image to bottom right corner of pasterect
               r.x += r.width - pimagewd;
               r.y += r.height - pimageht;
               break;
            case BottomLeft:
               // shift image to bottom left corner of pasterect
               r.y += r.height - pimageht;
               break;
            case Middle:
               // shift image to middle of pasterect; note that CheckPasteImage
               // has ensured (r.width - pimagewd) and (r.height - pimageht)
               // are an even number of *cells* if pastemag > 0
               r.x += (r.width - pimagewd) / 2;
               r.y += (r.height - pimageht) / 2;
               break;
         }
      }
      #ifdef __WXGTK__
         // wxGTK Blit doesn't support alpha channel
         dc.DrawBitmap(*pastebitmap, r.x, r.y, true);
      #else
         wxMemoryDC memdc;
         memdc.SelectObject(*pastebitmap);
         dc.Blit(r.x, r.y, pimagewd, pimageht, &memdc, 0, 0, wxCOPY, true);
      #endif
   }

   // now overlay border rectangle
   dc.SetPen(*pastepen);
   dc.SetBrush(*wxTRANSPARENT_BRUSH);

   // if large rect then we need to avoid overflow because DrawRectangle
   // has problems on Mac if given a size that exceeds 32K
   wxRect r = viewptr->pasterect;
   if (r.x < 0) { int diff = -1 - r.x;  r.x = -1;  r.width -= diff; }
   if (r.y < 0) { int diff = -1 - r.y;  r.y = -1;  r.height -= diff; }
   if (r.width > currlayer->view->getwidth())
      r.width = currlayer->view->getwidth() + 2;
   if (r.height > currlayer->view->getheight())
      r.height = currlayer->view->getheight() + 2;
   dc.DrawRectangle(r);
   
   if (r.y > 0) {
      dc.SetFont(*statusptr->GetStatusFont());
      //dc.SetBackgroundMode(wxSOLID);
      //dc.SetTextBackground(*wxWHITE);
      dc.SetBackgroundMode(wxTRANSPARENT);   // better in case pastergb is white
      dc.SetTextForeground(*pastergb);
      wxString pmodestr = wxString(GetPasteMode(),wxConvLocal);
      int pmodex = r.x + 2;
      int pmodey = r.y - 4;
      dc.DrawText(pmodestr, pmodex, pmodey - statusptr->GetTextAscent());
   }
   
   dc.SetBrush(wxNullBrush);
   dc.SetPen(wxNullPen);
}

// -----------------------------------------------------------------------------

void DrawGridLines(wxDC &dc, wxRect &r, int layerindex)
{
   int cellsize = 1 << currlayer->view->getmag();
   int h, v, i, topbold, leftbold;

   if (showboldlines) {
      // ensure that origin cell stays next to bold lines;
      // ie. bold lines scroll when pattern is scrolled
      pair<bigint, bigint> lefttop = currlayer->view->at(0, 0);
      leftbold = lefttop.first.mod_smallint(boldspacing);
      topbold = lefttop.second.mod_smallint(boldspacing);
      if (currlayer->originx != bigint::zero) {
         leftbold -= currlayer->originx.mod_smallint(boldspacing);
      }
      if (currlayer->originy != bigint::zero) {
         topbold -= currlayer->originy.mod_smallint(boldspacing);
      }
      if (mathcoords) topbold--;   // show origin cell above bold line
   } else {
      // avoid spurious gcc warning
      topbold = leftbold = 0;
   }

   // draw all plain lines first
   dc.SetPen(swapcolors ? *sgridpen[layerindex] : *gridpen);
   
   i = showboldlines ? topbold : 1;
   v = -1;
   while (true) {
      v += cellsize;
      if (v >= r.height) break;
      if (showboldlines) i++;
      if (i % boldspacing != 0 && v >= r.y && v < r.y + r.height)
         dc.DrawLine(r.x, v, r.GetRight() + 1, v);
   }
   i = showboldlines ? leftbold : 1;
   h = -1;
   while (true) {
      h += cellsize;
      if (h >= r.width) break;
      if (showboldlines) i++;
      if (i % boldspacing != 0 && h >= r.x && h < r.x + r.width)
         dc.DrawLine(h, r.y, h, r.GetBottom() + 1);
   }

   if (showboldlines) {
      // overlay bold lines
      dc.SetPen(swapcolors ? *sboldpen[layerindex] : *boldpen);
      i = topbold;
      v = -1;
      while (true) {
         v += cellsize;
         if (v >= r.height) break;
         i++;
         if (i % boldspacing == 0 && v >= r.y && v < r.y + r.height)
            dc.DrawLine(r.x, v, r.GetRight() + 1, v);
      }
      i = leftbold;
      h = -1;
      while (true) {
         h += cellsize;
         if (h >= r.width) break;
         i++;
         if (i % boldspacing == 0 && h >= r.x && h < r.x + r.width)
            dc.DrawLine(h, r.y, h, r.GetBottom() + 1);
      }
   }
   
   dc.SetPen(wxNullPen);
}

// -----------------------------------------------------------------------------

void DrawOneLayer(wxDC &dc, int index)
{
   wxMemoryDC layerdc;
   layerdc.SelectObject(*layerbitmap);
   
   // set foreground and background colors for DrawBitmap calls
   #if (defined(__WXMAC__) && !wxCHECK_VERSION(2,7,2)) || \
       (defined(__WXMSW__) && !wxCHECK_VERSION(2,8,0))
      // use opposite meaning
      layerdc.SetTextForeground(*deadrgb);
      layerdc.SetTextBackground(*livergb[index]);
   #else
      layerdc.SetTextForeground(*livergb[index]);
      layerdc.SetTextBackground(*deadrgb);
   #endif
   
   currdc = &layerdc;
   currlayer->algo->draw(*currlayer->view, renderer);
   
   // make dead pixels 100% transparent; live pixels use opacity setting
   MaskDeadPixels(layerbitmap, layerwd, layerht, int(2.55 * opacity));
   
   // draw result
   #ifdef __WXX11__
      wxMemoryDC memdc;
      memdc.SelectObject(*layerbitmap);
      dc.Blit(0, 0, layerwd, layerht, &memdc, 0, 0, wxCOPY, true);
      // need to delete mask
      layerbitmap->SetMask(NULL);
   #else
      dc.DrawBitmap(*layerbitmap, 0, 0, true);
   #endif
}

// -----------------------------------------------------------------------------

void DrawStackedLayers(wxDC &dc)
{
   // check if layerbitmap needs to be created or resized
   if ( layerwd != currlayer->view->getwidth() ||
        layerht != currlayer->view->getheight() ) {
      layerwd = currlayer->view->getwidth();
      layerht = currlayer->view->getheight();
      if (layerbitmap) delete layerbitmap;
      #ifdef __WXX11__
         // create a bitmap with screen depth
         layerbitmap = new wxBitmap(layerwd, layerht, -1);
      #else
         // create a bitmap with depth 32 so it has an alpha channel
         layerbitmap = new wxBitmap(layerwd, layerht, 32);
      #endif
      if (!layerbitmap) {
         Fatal(_("Not enough memory for layer bitmap!"));
         return;
      }
   }

   // temporarily turn off grid lines for DrawStretchedBitmap
   bool saveshow = showgridlines;
   showgridlines = false;

   // set brush color used in killrect
   killbrush = deadbrush;
   
   // draw patterns in layers 1..numlayers-1
   for ( int i = 1; i < numlayers; i++ ) {
      Layer *savelayer = currlayer;
      currlayer = GetLayer(i);
      
      // use real current layer's viewport
      viewport *saveview = currlayer->view;
      currlayer->view = savelayer->view;
      
      // avoid drawing a cloned layer more than once??? draw first or last clone???

      if ( !currlayer->algo->isEmpty() ) {
         DrawOneLayer(dc, i);
      }
      
      // draw this layer's selection if necessary
      wxRect r;
      if ( viewptr->SelectionVisible(&r) ) {
         CheckSelectionSize(layerwd, layerht);
         if (i == currindex)
            DrawSelection(dc, r);
         else
            DrawInactiveSelection(dc, r);
      }
      
      // restore viewport and currlayer
      currlayer->view = saveview;
      currlayer = savelayer;
   }
   
   showgridlines = saveshow;
}

// -----------------------------------------------------------------------------

void DrawTileFrame(wxDC& dc, wxRect& trect, wxBrush& brush, int wd)
{
   trect.Inflate(wd);
   wxRect r = trect;

   r.height = wd;
   FillRect(dc, r, brush);       // top edge
   
   r.y += trect.height - wd;
   FillRect(dc, r, brush);       // bottom edge
   
   r = trect;
   r.width = wd;
   FillRect(dc, r, brush);       // left edge
   
   r.x += trect.width - wd;
   FillRect(dc, r, brush);       // right edge
}

// -----------------------------------------------------------------------------

void DrawTileBorders(wxDC& dc)
{
   if (tileborder <= 0) return;    // no borders
   
   // draw tile borders in bigview window
   int wd, ht;
   bigview->GetClientSize(&wd, &ht);
   if (wd < 1 || ht < 1) return;
   
   wxBrush brush;
   int gray = (int) ((deadrgb->Red() + deadrgb->Green() + deadrgb->Blue()) / 3.0);
   if (gray > 127) {
      // deadrgb is light
      brush.SetColour(swapcolors ? ltgray : dkgray);
   } else {
      // deadrgb is dark
      brush.SetColour(swapcolors ? dkgray : ltgray);
   }
   wxRect trect;
   for ( int i = 0; i < numlayers; i++ ) {
      trect = GetLayer(i)->tilerect;
      DrawTileFrame(dc, trect, brush, tileborder);
   }

   // draw different colored border to indicate tile for current layer
   trect = GetLayer(currindex)->tilerect;
   brush.SetColour(brightgreen);                   //??? or *selectrgb
   DrawTileFrame(dc, trect, brush, tileborder);    //??? or thinner: (tileborder + 1) / 2);
}

// -----------------------------------------------------------------------------

void DrawView(wxDC& dc, int tileindex)
{
   wxRect r;
   Layer *savelayer = NULL;
   viewport *saveview0 = NULL;
   int colorindex;
   
   if ( viewptr->nopattupdate ) {
      // don't draw incomplete pattern, just fill background
      r = wxRect(0, 0, currlayer->view->getwidth(), currlayer->view->getheight());
      FillRect(dc, r, swapcolors ? *livebrush[0] : *deadbrush);

      // might as well draw grid lines
      if ( viewptr->GridVisible() ) DrawGridLines(dc, r, 0);
      return;
   }

   if ( numlayers > 1 && tilelayers ) {
      if ( tileindex < 0 ) {
         DrawTileBorders(dc);
         return;
      }
      // tileindex >= 0 so temporarily change some globals to draw this tile
      if ( syncviews && tileindex != currindex ) {
         // make sure this layer uses same location and scale as current layer
         GetLayer(tileindex)->view->setpositionmag(currlayer->view->x,
                                                   currlayer->view->y,
                                                   currlayer->view->getmag());
      }
      savelayer = currlayer;
      currlayer = GetLayer(tileindex);
      viewptr = currlayer->tilewin;
      colorindex = tileindex;
   } else if ( numlayers > 1 && stacklayers ) {
      // draw all layers starting with layer 0 but using current layer's viewport
      savelayer = currlayer;
      if ( currindex != 0 ) {
         // change currlayer to layer 0
         currlayer = GetLayer(0);
         saveview0 = currlayer->view;
         currlayer->view = savelayer->view;
      }
      colorindex = 0;
   } else {
      // just draw the current layer
      colorindex = currindex;
   }

// set foreground and background colors for DrawBitmap calls
#if (defined(__WXMAC__) && !wxCHECK_VERSION(2,7,2)) || \
    (defined(__WXMSW__) && !wxCHECK_VERSION(2,8,0))
   // use opposite meaning
   if ( swapcolors ) {
#else
   if ( !swapcolors ) {
#endif
      dc.SetTextForeground(*livergb[colorindex]);
      dc.SetTextBackground(*deadrgb);
   } else {
      dc.SetTextForeground(*deadrgb);
      dc.SetTextBackground(*livergb[colorindex]);
   }

   // set brush color used in killrect
   killbrush = swapcolors ? livebrush[colorindex] : deadbrush;

   // draw pattern using a sequence of blit and killrect calls
   currdc = &dc;
   currwd = currlayer->view->getwidth();
   currht = currlayer->view->getheight();
   currlayer->algo->draw(*currlayer->view, renderer);

   if ( viewptr->GridVisible() ) {
      r = wxRect(0, 0, currlayer->view->getwidth(), currlayer->view->getheight());
      DrawGridLines(dc, r, colorindex);
   }
   
   if ( viewptr->SelectionVisible(&r) ) {
      CheckSelectionSize(currlayer->view->getwidth(), currlayer->view->getheight());
      if (colorindex == currindex)
         DrawSelection(dc, r);
      else
         DrawInactiveSelection(dc, r);
   }
   
   if ( numlayers > 1 && stacklayers ) {
      // must restore currlayer before we call DrawStackedLayers
      currlayer = savelayer;
      if ( saveview0 ) {
         // restore layer 0's viewport
         GetLayer(0)->view = saveview0;
      }
      // draw layers 1, 2, ... numlayers-1
      DrawStackedLayers(dc);
   }
   
   if ( viewptr->waitingforclick && viewptr->pasterect.width > 0 ) {
      // this test is not really necessary, but it avoids unnecessary
      // drawing of the paste image when user changes scale
      if ( pastemag != viewptr->GetMag() &&
           prectwd == viewptr->pasterect.width && prectwd > 1 &&
           prectht == viewptr->pasterect.height && prectht > 1 ) {
         // don't draw old paste image, a new one is coming very soon
      } else {
         CheckPasteImage();
         DrawPasteImage(dc);
      }
   }

   if ( numlayers > 1 && tilelayers ) {
      // restore globals changed above
      currlayer = savelayer;
      viewptr = currlayer->tilewin;
   }
}
