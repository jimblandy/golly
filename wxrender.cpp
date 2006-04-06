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

#include "wx/image.h"      // for wxImage

#include "bigint.h"
#include "lifealgo.h"
#include "viewport.h"

#include "wxgolly.h"       // for viewptr, statusptr
#include "wxutils.h"       // for Warning, Fatal, FillRect
#include "wxprefs.h"       // for blackcells, showgridlines, mingridmag, etc
#include "wxstatus.h"      // for statusptr->...
#include "wxview.h"        // for viewptr->...
#include "wxrender.h"

// -----------------------------------------------------------------------------

// static data used in wx_render routines
wxDC* currdc;              // current device context for viewport
int currwd, currht;        // current width and height of viewport
wxBrush* deadbrush;        // brush used in killrect

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
#ifdef __WXX11__
   // wxX11's Blit doesn't support alpha channel
#else
   wxImage selimage;       // translucent overlay for drawing selections
   wxBitmap* selbitmap;    // selection bitmap
   int selimagewd;         // width of selection image
   int selimageht;         // height of selection image
#endif

// paste image (initialized in CreatePasteImage)
wxImage pasteimage;        // translucent image for drawing paste pattern
wxBitmap* pastebitmap;     // paste bitmap
int pimagewd;              // width of paste image
int pimageht;              // height of paste image
int prectwd;               // must match viewptr->pasterect.width
int prectht;               // must match viewptr->pasterect.height
int pastemag;              // must match current viewport's scale
int cvwd, cvht;            // must match current viewport's width and height
bool pcolor;               // must match blackcells flag
paste_location pasteloc;   // must match plocation
lifealgo* pastealgo;       // universe containing paste pattern
wxRect pastebbox;          // bounding box in cell coords (not necessarily minimal)

// make this a user preference on Mac/Win???
// PROBLEM: if usemask is false then translucent image on Win is not correct!!!
#ifdef __WXX11__
   bool usemask = true;    // no alpha channel support so must use mask
#else
   bool usemask = true;    // use masked bitmap instead of translucent image?
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
      // create translucent selection image
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
      selimagewd = wd;
      selimageht = ht;
   #endif
}

// -----------------------------------------------------------------------------

void DestroyDrawingData()
{
   #ifdef __WXX11__
      // wxX11 doesn't support alpha channel
   #else
      selimage.Destroy();
      if (selbitmap) delete selbitmap;
   #endif

   pen_ltgray.~wxPen();
   pen_dkgray.~wxPen();
   pen_verydark.~wxPen();
   pen_notsodark.~wxPen();
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
      FillRect(*currdc, r, *deadbrush);
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

void CheckSelectionImage(int viewwd, int viewht)
{
   #ifdef __WXX11__
      // wxX11 doesn't support alpha channel;
      // avoid compiler warnings
      if ( viewwd == viewht ) {}
   #else
      if ( viewwd != selimagewd || viewht != selimageht ) {
         // rescale selection image and create new bitmap
         selimage.Rescale(viewwd, viewht);
         if (selbitmap) delete selbitmap;
         selbitmap = new wxBitmap(selimage);
         // don't check if selbitmap is NULL here -- done in DrawSelection
         selimagewd = viewwd;
         selimageht = viewht;
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
         // draw translucent green rect
         wxMemoryDC memdc;
         memdc.SelectObject(*selbitmap);
         dc.Blit(rect.x, rect.y, rect.width, rect.height, &memdc, 0, 0, wxCOPY, true);
      }
   #endif
}

// -----------------------------------------------------------------------------

void CreatePasteImage(lifealgo *palgo, wxRect &bbox)
{
   pastealgo = palgo;      // save for use in CheckPasteImage
   pastebbox = bbox;       // ditto
   
   if (!usemask) {
      // create translucent image for drawing given pattern
      if ( !pasteimage.Create(1,1) )
         Fatal("Failed to create paste image!");
      // color here doesn't matter coz we paint entire image
      // pasteimage.SetRGB(0, 0, 128, 128, 128);
      pasteimage.SetAlpha();                       // add alpha channel
      if ( pasteimage.HasAlpha() ) {
         pasteimage.SetAlpha(0, 0, 128);           // 50% opaque
      } else {
         Warning("Paste image has no alpha channel!");
      }
   }
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
   if (!usemask) pasteimage.Destroy();
   if (pastebitmap) {
      delete pastebitmap;
      pastebitmap = NULL;
   }
}

// -----------------------------------------------------------------------------

int PixelsToCells(int pixels) {
   // convert given # of pixels to corresponding # of cells
   int mag = viewptr->GetMag();
   if (mag >= 0) {
      int cellsize = 1 << mag;
      return (pixels + cellsize - 1) / cellsize;
   } else {
      // mag < 0; no need to worry about overflow
      return pixels << -mag;
   }
}

// -----------------------------------------------------------------------------

void CheckPasteImage(viewport &currview)
{
   // paste image needs to be updated if pasterect size changed
   // or viewport size changed or cell colors changed or plocation changed
   if ( prectwd != viewptr->pasterect.width ||
        prectht != viewptr->pasterect.height ||
        cvwd != currview.getwidth() ||
        cvht != currview.getheight() ||
        pcolor != blackcells ||
        pasteloc != plocation
      ) {
      prectwd = viewptr->pasterect.width;
      prectht = viewptr->pasterect.height;
      pastemag = currview.getmag();
      cvwd = currview.getwidth();
      cvht = currview.getheight();
      pcolor = blackcells;
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
            if (!usemask) {
               // wxMac bug??? Rescale values must change to avoid loss of alpha channel
               pasteimage.Rescale(pimagewd, pimageht);
            }
         }
         return;
      }
      
      wxRect cellbox = pastebbox;
      if (pastewd > currview.getwidth() || pasteht > currview.getheight()) {
         if (plocation == Middle) {
            // temporary viewport may need to be TWICE size of current viewport
            if (pastewd > 2 * currview.getwidth()) pastewd = 2 * currview.getwidth();
            if (pasteht > 2 * currview.getheight()) pasteht = 2 * currview.getheight();
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
            if (pastewd > currview.getwidth()) pastewd = currview.getwidth();
            if (pasteht > currview.getheight()) pasteht = currview.getheight();
            if (pastemag > 0) {
               // make sure pastewd/ht don't have partial cells
               int cellsize = 1 << pastemag;
               if ((pastewd + 1) % cellsize > 0)
                  pastewd += cellsize - ((pastewd + 1) % cellsize);
               if ((pasteht + 1) % cellsize != 0)
                  pasteht += cellsize - ((pasteht + 1) % cellsize);
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
      
      if (usemask) {
         // delete old bitmap and corresponding mask, even if size hasn't changed
         if (pastebitmap) delete pastebitmap;
         pimagewd = pastewd;
         pimageht = pasteht;
         // create new bitmap (-1 means screen depth)
         pastebitmap = new wxBitmap(pimagewd, pimageht, -1);
         // mask will be created after drawing pattern into bitmap
      } else if ( pimagewd != pastewd || pimageht != pasteht ) {
         // rescale paste image and create new bitmap
         pimagewd = pastewd;
         pimageht = pasteht;
         pasteimage.Rescale(pimagewd, pimageht);
         if (pastebitmap) delete pastebitmap;
         pastebitmap = new wxBitmap(pasteimage);
      }
      
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
         #if defined(__WXMAC__) || defined(__WXMSW__)
            // use opposite meaning on Mac/Windows -- sheesh
            if (blackcells || usemask) {
               pattdc.SetTextForeground(*wxWHITE);
               pattdc.SetTextBackground(*wxRED);
            } else {
               pattdc.SetTextForeground(*wxBLACK);
               pattdc.SetTextBackground(*wxRED);
            }
         #else
            if (blackcells || usemask) {
               pattdc.SetTextForeground(*wxRED);
               pattdc.SetTextBackground(*wxWHITE);
            } else {
               pattdc.SetTextForeground(*wxRED);
               pattdc.SetTextBackground(*wxBLACK);
            }
         #endif

         // set brush color used in killrect
         deadbrush = blackcells || usemask ? wxWHITE_BRUSH : wxBLACK_BRUSH;
         
         // temporarily turn off grid lines for DrawStretchedBitmap
         bool saveshow = showgridlines;
         showgridlines = false;
         
         currdc = &pattdc;
         currwd = tempview.getwidth();
         currht = tempview.getheight();
         pastealgo->draw(tempview, renderer);
         
         showgridlines = saveshow;

         if (usemask) {
            // add new mask to pastebitmap
            #ifdef __WXMSW__
               // temporarily change depth to avoid bug in wxMSW 2.6.0
               int d = pastebitmap->GetDepth();
               pastebitmap->SetDepth(1);
               pastebitmap->SetMask( new wxMask(*pastebitmap,*wxWHITE) );
               pastebitmap->SetDepth(d);
            #else
               // white background will be transparent
               pastebitmap->SetMask( new wxMask(*pastebitmap,*wxWHITE) );
            #endif
         }
      } else {
         // give some indication that pastebitmap could not be created
         wxBell();
      }
   }
}

// -----------------------------------------------------------------------------

void DrawPasteImage(wxDC &dc, viewport &currview)
{
   if (pastebitmap) {
      // draw paste image
      wxMemoryDC memdc;
      memdc.SelectObject(*pastebitmap);
      wxRect r = viewptr->pasterect;
      if (r.width > pimagewd || r.height > pimageht) {
         // paste image is smaller than pasterect (which can't fit in currview)
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
      dc.Blit(r.x, r.y, pimagewd, pimageht, &memdc, 0, 0, wxCOPY, true);
   }

   // now overlay border rectangle
   dc.SetPen(*wxRED_PEN);
   dc.SetBrush(*wxTRANSPARENT_BRUSH);

   // if large rect then we need to avoid overflow because DrawRectangle
   // has problems on Mac if given a size that exceeds 32K
   wxRect r = viewptr->pasterect;
   if (r.x < 0) { int diff = -1 - r.x;  r.x = -1;  r.width -= diff; }
   if (r.y < 0) { int diff = -1 - r.y;  r.y = -1;  r.height -= diff; }
   if (r.width > currview.getwidth()) r.width = currview.getwidth() + 2;
   if (r.height > currview.getheight()) r.height = currview.getheight() + 2;
   dc.DrawRectangle(r);
   
   if (r.y > 0) {
      dc.SetFont(*statusptr->GetStatusFont());
      dc.SetBackgroundMode(wxSOLID);
      dc.SetTextForeground(*wxRED);
      dc.SetTextBackground(*wxWHITE);
      const char *pmodestr = GetPasteMode();
      int pmodex = r.x + 2;
      int pmodey = r.y - 4;
      dc.DrawText(pmodestr, pmodex, pmodey - statusptr->GetTextAscent());
   }
   
   dc.SetBrush(wxNullBrush);
   dc.SetPen(wxNullPen);
}

// -----------------------------------------------------------------------------

void DrawGridLines(wxDC &dc, wxRect &r, viewport &currview)
{
   int cellsize = 1 << currview.getmag();
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
      if (blackcells) {
         dc.SetPen(pen_dkgray);
      } else {
         dc.SetPen(pen_notsodark);
      }
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
   
   dc.SetPen(*wxBLACK_PEN);
}

// -----------------------------------------------------------------------------

void DrawView(wxDC &dc, viewport &currview)
{
   wxRect r;

   if ( viewptr->nopattupdate ) {
      // don't draw pattern, just fill background
      r = wxRect(0, 0, currview.getwidth(), currview.getheight());
      FillRect(dc, r, blackcells ? *wxWHITE_BRUSH : *wxBLACK_BRUSH);
   } else {
      // set foreground and background colors for DrawBitmap calls
      #if defined(__WXMAC__) || defined(__WXMSW__)
      // use opposite meaning on Mac/Windows -- sheesh
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
      // set brush color used in killrect
      deadbrush = blackcells ? wxWHITE_BRUSH : wxBLACK_BRUSH;
      // draw pattern using a sequence of blit and killrect calls
      currdc = &dc;
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
      // this test is not really necessary, but it avoids unnecessary
      // drawing of the paste image when user changes scale
      if ( pastemag != viewptr->GetMag() &&
           prectwd == viewptr->pasterect.width && prectwd > 1 &&
           prectht == viewptr->pasterect.height && prectht > 1 ) {
         // don't draw old paste image, a new one is coming very soon
      } else {
         CheckPasteImage(currview);
         DrawPasteImage(dc, currview);
      }
   }
}
