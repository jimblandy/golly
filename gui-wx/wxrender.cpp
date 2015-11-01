/*** /
 
 This file is part of Golly, a Game of Life Simulator.
 Copyright (C) 2013 Andrew Trevorrow and Tomas Rokicki.
 
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
The global viewptr points to a PatternView window which is created in
MainFrame::MainFrame() in wxmain.cpp.

Nearly all drawing in the viewport is done in this module.  The only other
place is in wxview.cpp where PatternView::DrawOneCell() is used to draw
cells with the pencil cursor.  This is done for performance reasons --
using Refresh + Update to do the drawing is too slow.

The main rendering routine is DrawView() -- see the end of this module.
DrawView() is called from PatternView::OnPaint(), the update event handler
for the viewport window.  Update events are created automatically by the
wxWidgets event dispatcher, or they can be created manually by calling
Refresh() and Update().

DrawView() does the following tasks:

- Fills the entire viewport with the state 0 color.
- Calls currlayer->algo->draw() to draw the current pattern.  It passes
  in renderer, an instance of golly_render (derived from liferender) which
  has these methods:
- pixblit() draws a pixmap containing at least one live cell.
- getcolors() provides access to the current layer's color arrays.

Note that currlayer->algo->draw() does all the hard work of figuring
out which parts of the viewport are dead and building all the pixmaps
for the live parts.  The pixmaps contain suitably shrunken images
when the scale is < 1:1 (ie. mag < 0).

Each lifealgo needs to implement its own draw() method; for example,
hlifealgo::draw() in hlifedraw.cpp.

- Calls DrawGridLines() to overlay grid lines if they are visible.

- Calls DrawGridBorder() to draw border around a bounded universe.

- Calls DrawSelection() to overlay a translucent selection rectangle
  if a selection exists and any part of it is visible.

- Calls DrawStackedLayers() to overlay multiple layers using the current
  layer's scale and location.

- If the user is doing a paste, CheckPasteImage() creates a temporary
  viewport (tempview) and draws the paste pattern (stored in pastealgo)
  into a masked pixmap which is then used by DrawPasteImage().

- Calls DrawControls() if the translucent controls need to be drawn.

----------------------------------------------------------------------------- */

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
    #include "wx/wx.h"     // for all others include the necessary headers
#endif

#include "wx/rawbmp.h"     // for wxAlphaPixelData

#include "bigint.h"
#include "lifealgo.h"
#include "viewport.h"

#include "wxgolly.h"       // for viewptr, bigview, statusptr
#include "wxutils.h"       // for Warning, Fatal, FillRect
#include "wxprefs.h"       // for showgridlines, mingridmag, swapcolors, etc
#include "wxstatus.h"      // for statusptr->...
#include "wxview.h"        // for viewptr->...
#include "wxlayer.h"       // currlayer, GetLayer, etc
#include "wxrender.h"

// -----------------------------------------------------------------------------

// globals used in golly_render routines
int currwd, currht;                     // current width and height of viewport, in pixels
int scalefactor;                        // current scale factor (1, 2, 4, 8 or 16)
static unsigned char** icontextures;    // pointers to texture data for each icon
static GLuint texture8 = 0;             // texture for drawing 7x7 icons
static GLuint texture16 = 0;            // texture for drawing 15x15 icons
static GLuint texture32 = 0;            // texture for drawing 31x31 icons
static GLuint rgbtexture = 0;           // texture for drawing RGB bitmaps
static GLuint rgbatexture = 0;          // texture for drawing RGBA bitmaps

// fixed texture coordinates used by glTexCoordPointer
static const GLshort texture_coordinates[] = { 0,0, 1,0, 0,1, 1,1 };

// for drawing stacked layers
bool mask_dead_pixels = false;          // make dead pixels 100% transparent?
unsigned char live_alpha = 255;         // alpha level for live pixels
unsigned char* rgbadata = NULL;         // RGBA data for drawing masked bitmaps
int rgbalen = 0;                        // length of rgbadata

// for drawing paste pattern (initialized in CreatePasteImage)
wxBitmap* pastebitmap;                  // paste bitmap
int pimagewd;                           // width of paste image
int pimageht;                           // height of paste image
int prectwd;                            // must match viewptr->pasterect.width
int prectht;                            // must match viewptr->pasterect.height
int pastemag;                           // must match current viewport's scale
int cvwd, cvht;                         // must match current viewport's width and height
paste_location pasteloc;                // must match plocation
bool pasteicons;                        // must match showicons
bool pastecolors;                       // must match swapcolors
bool pastescale;                        // must match scalepatterns
lifealgo* pastealgo;                    // universe containing paste pattern
wxRect pastebbox;                       // bounding box in cell coords (not necessarily minimal)
static bool drawing_paste = false;      // drawing paste image?

// for drawing translucent controls
wxBitmap* ctrlsbitmap = NULL;           // controls bitmap
wxBitmap* darkctrls = NULL;             // for showing clicked control
int controlswd;                         // width of ctrlsbitmap
int controlsht;                         // height of ctrlsbitmap

// include controls_xpm (XPM data for controls bitmap)
#include "bitmaps/controls.xpm"

// these constants must match image dimensions in bitmaps/controls.xpm
const int buttborder = 6;               // size of outer border
const int buttsize = 22;                // size of each button
const int buttsperrow = 3;              // # of buttons in each row
const int numbutts = 15;                // # of buttons
const int rowgap = 4;                   // vertical gap after first 2 rows

// currently clicked control
control_id currcontrol = NO_CONTROL;

// -----------------------------------------------------------------------------

static void SetColor(int r, int g, int b, int a)
{
    glColor4ub(r, g, b, a);
}

// -----------------------------------------------------------------------------

static void SetPointSize(int ptsize)
{
    glPointSize(ptsize);
}

// -----------------------------------------------------------------------------

static void FillRect(int x, int y, int wd, int ht)
{
    GLfloat rect[] = {
        x,    y+ht,     // left, bottom
        x+wd, y+ht,     // right, bottom
        x+wd, y,        // right, top
        x,    y,        // left, top
    };
    glVertexPointer(2, GL_FLOAT, 0, rect);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

// -----------------------------------------------------------------------------

static void DisableTextures()
{
    if (glIsEnabled(GL_TEXTURE_2D)) {
        glDisable(GL_TEXTURE_2D);
    };
}

// -----------------------------------------------------------------------------

void CreateTranslucentControls()
{
    wxImage image(controls_xpm);
    controlswd = image.GetWidth();
    controlsht = image.GetHeight();
    
    // use depth 32 so bitmap has an alpha channel
    ctrlsbitmap = new wxBitmap(controlswd, controlsht, 32);
    if (ctrlsbitmap == NULL) {
        Warning(_("Not enough memory for controls bitmap!"));
    } else {
        // set ctrlsbitmap pixels and their alpha values based on pixels in image
        wxAlphaPixelData data(*ctrlsbitmap, wxPoint(0,0), wxSize(controlswd,controlsht));
        if (data) {
            int alpha = 192;     // 75% opaque
#if !wxCHECK_VERSION(2,9,0)
            data.UseAlpha();
#endif
            wxAlphaPixelData::Iterator p(data);
            for ( int y = 0; y < controlsht; y++ ) {
                wxAlphaPixelData::Iterator rowstart = p;
                for ( int x = 0; x < controlswd; x++ ) {
                    int r = image.GetRed(x,y);
                    int g = image.GetGreen(x,y);
                    int b = image.GetBlue(x,y);
                    if (r == 0 && g == 0 && b == 0) {
                        // make black pixel fully transparent
                        p.Red()   = 0;
                        p.Green() = 0;
                        p.Blue()  = 0;
                        p.Alpha() = 0;
                    } else {
                        // make all non-black pixels translucent
#if defined(__WXMSW__) || (defined(__WXMAC__) && wxCHECK_VERSION(2,8,8))
                        // premultiply the RGB values
                        p.Red()   = r * alpha / 255;
                        p.Green() = g * alpha / 255;
                        p.Blue()  = b * alpha / 255;
#else
                        p.Red()   = r;
                        p.Green() = g;
                        p.Blue()  = b;
#endif
                        p.Alpha() = alpha;
                    }
                    p++;
                }
                p = rowstart;
                p.OffsetY(data, 1);
            }
        }
    }
    
    // create bitmap for showing clicked control
    darkctrls = new wxBitmap(controlswd, controlsht, 32);
    if (darkctrls == NULL) {
        Warning(_("Not enough memory for dark controls bitmap!"));
    } else {
        // set darkctrls pixels and their alpha values based on pixels in image
        wxAlphaPixelData data(*darkctrls, wxPoint(0,0), wxSize(controlswd,controlsht));
        if (data) {
            int alpha = 128;     // 50% opaque
            int gray = 20;       // very dark gray
#if !wxCHECK_VERSION(2,9,0)
            data.UseAlpha();
#endif
            wxAlphaPixelData::Iterator p(data);
            for ( int y = 0; y < controlsht; y++ ) {
                wxAlphaPixelData::Iterator rowstart = p;
                for ( int x = 0; x < controlswd; x++ ) {
                    int r = image.GetRed(x,y);
                    int g = image.GetGreen(x,y);
                    int b = image.GetBlue(x,y);
                    if (r == 0 && g == 0 && b == 0) {
                        // make black pixel fully transparent
                        p.Red()   = 0;
                        p.Green() = 0;
                        p.Blue()  = 0;
                        p.Alpha() = 0;
                    } else {
                        // make all non-black pixels translucent gray
#if defined(__WXMSW__) || (defined(__WXMAC__) && wxCHECK_VERSION(2,8,8))
                        // premultiply the RGB values
                        p.Red()   = gray * alpha / 255;
                        p.Green() = gray * alpha / 255;
                        p.Blue()  = gray * alpha / 255;
#else
                        p.Red()   = gray;
                        p.Green() = gray;
                        p.Blue()  = gray;
#endif
                        p.Alpha() = alpha;
                    }
                    p++;
                }
                p = rowstart;
                p.OffsetY(data, 1);
            }
        }
    }
}

// -----------------------------------------------------------------------------

control_id WhichControl(int x, int y)
{
    // determine which button is at x,y in controls bitmap
    int col, row;
    
    x -= buttborder;
    y -= buttborder;
    if (x < 0 || y < 0) return NO_CONTROL;
    
    // allow for vertical gap after first 2 rows
    if (y < (buttsize + rowgap)) {
        if (y > buttsize) return NO_CONTROL;               // in 1st gap
        row = 1;
    } else if (y < 2*(buttsize + rowgap)) {
        if (y > 2*buttsize + rowgap) return NO_CONTROL;    // in 2nd gap
        row = 2;
    } else {
        row = 3 + (y - 2*(buttsize + rowgap)) / buttsize;
    }
    
    col = 1 + x / buttsize;
    if (col < 1 || col > buttsperrow) return NO_CONTROL;
    if (row < 1 || row > numbutts/buttsperrow) return NO_CONTROL;
    
    int control = (row - 1) * buttsperrow + col;
    return (control_id) control;
}

// -----------------------------------------------------------------------------

void DrawControls(wxDC& dc, wxRect& rect)
{
    if (ctrlsbitmap) {
#ifdef __WXGTK__
        // wxGTK Blit doesn't support alpha channel
        dc.DrawBitmap(*ctrlsbitmap, rect.x, rect.y, true);
#else
        // Blit is about 10% faster than DrawBitmap (on Mac at least)
        wxMemoryDC memdc;
        memdc.SelectObject(*ctrlsbitmap);
        dc.Blit(rect.x, rect.y, rect.width, rect.height, &memdc, 0, 0, wxCOPY, true);
        memdc.SelectObject(wxNullBitmap);
#endif
        
        if (currcontrol > NO_CONTROL && darkctrls) {
            // show clicked control
            int i = (int)currcontrol - 1;
            int x = buttborder + (i % buttsperrow) * buttsize;
            int y = buttborder + (i / buttsperrow) * buttsize;
            
            // allow for vertical gap after first 2 rows
            if (i < buttsperrow) {
                // y is correct
            } else if (i < 2*buttsperrow) {
                y += rowgap;
            } else {
                y += 2*rowgap;
            }
            
#ifdef __WXGTK__
            // wxGTK Blit doesn't support alpha channel
            wxRect r(x, y, buttsize, buttsize);
            dc.DrawBitmap(darkctrls->GetSubBitmap(r), rect.x + x, rect.y + y, true);
#else
            wxMemoryDC memdc;
            memdc.SelectObject(*darkctrls);
            dc.Blit(rect.x + x, rect.y + y, buttsize, buttsize, &memdc, x, y, wxCOPY, true);
            memdc.SelectObject(wxNullBitmap);
#endif
        }
    }
}

// -----------------------------------------------------------------------------

void InitDrawingData()
{
    // no longer needed -- remove???!!!
}

// -----------------------------------------------------------------------------

void DestroyDrawingData()
{
    delete ctrlsbitmap;
    delete darkctrls;
    if (rgbadata) free(rgbadata);
}

// -----------------------------------------------------------------------------

void DrawTexture(unsigned char* rgbdata, int x, int y, int w, int h)
{
    // called from golly_render::pixblit to draw a pattern bitmap at 1:1 scale

	if (rgbtexture == 0) {
        // only need to create texture name once
        glGenTextures(1, &rgbtexture);
    }

    if (!glIsEnabled(GL_TEXTURE_2D)) {
        // restore texture color and enable textures
        SetColor(255, 255, 255, 255);
        glEnable(GL_TEXTURE_2D);
        // bind our texture
        glBindTexture(GL_TEXTURE_2D, rgbtexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);    // avoids edge effects when scaling
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);    // ditto
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexCoordPointer(2, GL_SHORT, 0, texture_coordinates);
    }
    
    // update the texture with the new RGB data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, rgbdata);
    
    GLfloat vertices[] = {
        x,      y,
        x+w,    y,
        x,      y+h,
        x+w,    y+h,
    };
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

// -----------------------------------------------------------------------------

void DrawMaskedTexture(unsigned char* rgbdata, int x, int y, int w, int h)
{
    // called from golly_render::pixblit to draw a masked pattern bitmap at 1:1 scale

    // first we have to convert the given RGB data to RGBA data,
    // where dead pixels have alpha set to 0 and live pixels are set to live_alpha
    // maybe it's time to change pixblit() to return RGBA data???!!!
    // and maybe draw() could pass in dead_alpha and live_alpha???!!!
    if (rgbalen != w*h*4) {
        rgbalen = w*h*4;
        if (rgbadata) free(rgbadata);
        rgbadata = (unsigned char*) malloc(rgbalen);
        if (!rgbadata) return;
    }
    unsigned char deadr = currlayer->cellr[0];
    unsigned char deadg = currlayer->cellg[0];
    unsigned char deadb = currlayer->cellb[0];
    int i = 0;
    int j = 0;
    while (i < w*h*3) {
        unsigned char r = rgbdata[i++];
        unsigned char g = rgbdata[i++];
        unsigned char b = rgbdata[i++];
        if (r == deadr && g == deadg && b == deadb) {
            // dead pixel
            rgbadata[j++] = 0;
            rgbadata[j++] = 0;
            rgbadata[j++] = 0;
            rgbadata[j++] = 0;  // 100% transparent
        } else {
            // live pixel
            rgbadata[j++] = r;
            rgbadata[j++] = g;
            rgbadata[j++] = b;
            rgbadata[j++] = live_alpha;
        }
    }
    
	if (rgbatexture == 0) {
        // only need to create texture name once
        glGenTextures(1, &rgbatexture);
    }

    if (!glIsEnabled(GL_TEXTURE_2D)) {
        // restore texture color and enable textures
        SetColor(255, 255, 255, 255);
        glEnable(GL_TEXTURE_2D);
        // bind our texture
        glBindTexture(GL_TEXTURE_2D, rgbatexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);    // avoids edge effects when scaling
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);    // ditto
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexCoordPointer(2, GL_SHORT, 0, texture_coordinates);
    }
    
    // update the texture with the new RGBA data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbadata);
    
    GLfloat vertices[] = {
        x,      y,
        x+w,    y,
        x,      y+h,
        x+w,    y+h,
    };
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

// -----------------------------------------------------------------------------

void DrawPoints(unsigned char* rgbdata, int x, int y, int w, int h)
{
    // called from golly_render::pixblit to draw pattern at 1:1 scale
    // when we're drawing the paste image

    const int maxcoords = 1024;     // must be multiple of 2
    GLfloat points[maxcoords];
    int numcoords = 0;

    DisableTextures();
    SetPointSize(1);

    unsigned char deadr = currlayer->cellr[0];
    unsigned char deadg = currlayer->cellg[0];
    unsigned char deadb = currlayer->cellb[0];
    unsigned char prevr = deadr;
    unsigned char prevg = deadg;
    unsigned char prevb = deadb;
    SetColor(deadr, deadg, deadb, 255);

    unsigned char r, g, b;
    int i = 0;
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            r = rgbdata[i++];
            g = rgbdata[i++];
            b = rgbdata[i++];
            if (r != deadr || g != deadg || b != deadb) {
                // we've got a live pixel
                bool changecolor = (r != prevr || g != prevg || b != prevb);
                if (changecolor || numcoords == maxcoords) {
                    if (numcoords > 0) {
                        glVertexPointer(2, GL_FLOAT, 0, points);
                        glDrawArrays(GL_POINTS, 0, numcoords/2);
                        numcoords = 0;
                    }
                    if (changecolor) {
                        prevr = r;
                        prevg = g;
                        prevb = b;
                        SetColor(r, g, b, 255);
                    }
                }
                points[numcoords++] = x + col + 0.5;
                points[numcoords++] = y + row + 0.5;
            }
        }
    }

    if (numcoords > 0) {
        glVertexPointer(2, GL_FLOAT, 0, points);
        glDrawArrays(GL_POINTS, 0, numcoords/2);
    }
}

// -----------------------------------------------------------------------------

static int prevsize = 0;

void DrawIcons(unsigned char* statedata, int x, int y, int w, int h, int pmscale, int stride)
{
    // called from golly_render::pixblit to draw icons for each live cell;
    // assume pmscale > 2 (should be 8, 16 or 32 or 64)
    int iconsize = pmscale;

    // on high density screens the max scale is 1:64, but instead of
    // supporting 63x63 icons we simply scale up the 31x31 icons
    // (leaving a barely noticeable 1px gap at the right and bottom edges)
    if (pmscale == 64) {
        iconsize = 32;
    }

    // create icon textures once
	if (texture8 == 0) glGenTextures(1, &texture8);
	if (texture16 == 0) glGenTextures(1, &texture16);
	if (texture32 == 0) glGenTextures(1, &texture32);

    if (!glIsEnabled(GL_TEXTURE_2D)) {
        // restore texture color and enable textures
        SetColor(255, 255, 255, 255);
        glEnable(GL_TEXTURE_2D);
        prevsize = 0;               // force rebinding
    }
    
    if (iconsize != prevsize) {
        prevsize = iconsize;
        // bind appropriate icon texture
        if (iconsize == 8) glBindTexture(GL_TEXTURE_2D, texture8);
        if (iconsize == 16) glBindTexture(GL_TEXTURE_2D, texture16);
        if (iconsize == 32) glBindTexture(GL_TEXTURE_2D, texture32);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexCoordPointer(2, GL_SHORT, 0, texture_coordinates);
    }

    int prevstate = 0;
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            unsigned char state = statedata[row*stride + col];
            if (state > 0 && icontextures[state]) {
                
                if (state != prevstate) {
                    prevstate = state;
                    // update the texture with the new icon data (in RGBA format)
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, iconsize, iconsize, 0,
                                 GL_RGBA, GL_UNSIGNED_BYTE, icontextures[state]);
                }
                
                int xpos = x + col * pmscale;
                int ypos = y + row * pmscale;
                
                GLfloat vertices[] = {
                    xpos,           ypos,
                    xpos + pmscale, ypos,
                    xpos,           ypos + pmscale,
                    xpos + pmscale, ypos + pmscale,
                };
                glVertexPointer(2, GL_FLOAT, 0, vertices);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }
        }
    }
}

// -----------------------------------------------------------------------------

void DrawOneIcon(wxDC& dc, int x, int y, wxBitmap* icon,
                 unsigned char deadr, unsigned char deadg, unsigned char deadb,
                 unsigned char liver, unsigned char liveg, unsigned char liveb,
                 bool multicolor)
{
    // draw a single icon (either multi-color or grayscale) outside the viewport
    int wd = icon->GetWidth();
    int ht = icon->GetHeight();
    wxBitmap pixmap(wd, ht, 32);
    
    wxAlphaPixelData pxldata(pixmap);
    if (pxldata) {
#if defined(__WXGTK__) && !wxCHECK_VERSION(2,9,0)
        pxldata.UseAlpha();
#endif
        wxAlphaPixelData::Iterator p(pxldata);
        wxAlphaPixelData icondata(*icon);
        if (icondata) {
            wxAlphaPixelData::Iterator iconpxl(icondata);
            for (int i = 0; i < ht; i++) {
                wxAlphaPixelData::Iterator pixmaprow = p;
                wxAlphaPixelData::Iterator iconrow = iconpxl;
                for (int j = 0; j < wd; j++) {
                    if (iconpxl.Red() || iconpxl.Green() || iconpxl.Blue()) {
                        if (multicolor) {
                            // use non-black pixel in multi-colored icon
                            if (swapcolors) {
                                p.Red()   = 255 - iconpxl.Red();
                                p.Green() = 255 - iconpxl.Green();
                                p.Blue()  = 255 - iconpxl.Blue();
                            } else {
                                p.Red()   = iconpxl.Red();
                                p.Green() = iconpxl.Green();
                                p.Blue()  = iconpxl.Blue();
                            }
                        } else {
                            // grayscale icon
                            if (iconpxl.Red() == 255) {
                                // replace white pixel with live cell color
                                p.Red()   = liver;
                                p.Green() = liveg;
                                p.Blue()  = liveb;
                            } else {
                                // replace gray pixel with appropriate shade between 
                                // live and dead cell colors
                                float frac = (float)(iconpxl.Red()) / 255.0;
                                p.Red()   = (int)(deadr + frac * (liver - deadr) + 0.5);
                                p.Green() = (int)(deadg + frac * (liveg - deadg) + 0.5);
                                p.Blue()  = (int)(deadb + frac * (liveb - deadb) + 0.5);
                            }
                        }
                    } else {
                        // replace black pixel with dead cell color
                        p.Red()   = deadr;
                        p.Green() = deadg;
                        p.Blue()  = deadb;
                    }
#if defined(__WXGTK__) || wxCHECK_VERSION(2,9,0)
                    p.Alpha() = 255;
#endif
                    p++;
                    iconpxl++;
                }
                // move to next row of pixmap
                p = pixmaprow;
                p.OffsetY(pxldata, 1);
                // move to next row of icon bitmap
                iconpxl = iconrow;
                iconpxl.OffsetY(icondata, 1);
            }
        }
    }
    dc.DrawBitmap(pixmap, x, y);
}

// -----------------------------------------------------------------------------

void DrawMagnifiedTwoStateCells(unsigned char* statedata, int x, int y, int w, int h, int pmscale, int stride)
{
    // called from golly_render::pixblit to draw cells magnified by pmscale (2, 4, ... 2^MAX_MAG)
    // when number of states is 2
    int cellsize = pmscale > 2 ? pmscale - 1 : pmscale;

    const int maxcoords = 1024;     // must be multiple of 2
    GLfloat points[maxcoords];
    int numcoords = 0;

    DisableTextures();
    SetPointSize(cellsize);

    unsigned char alpha = mask_dead_pixels ? live_alpha : 255;

    // all live cells are in state 1 so only need to set color once
    SetColor(currlayer->cellr[1],
             currlayer->cellg[1],
             currlayer->cellb[1], alpha);
    
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            unsigned char state = statedata[row*stride + col];
            if (state > 0) {
                if (numcoords == maxcoords) {
                    glVertexPointer(2, GL_FLOAT, 0, points);
                    glDrawArrays(GL_POINTS, 0, numcoords/2);
                    numcoords = 0;
                }
                // get mid point of cell
                GLfloat midx = x + col*pmscale + cellsize/2.0;
                GLfloat midy = y + row*pmscale + cellsize/2.0;
                if (midx > float(currwd) || midy > float(currht)) {
                    // midx,midy is outside viewport so we need to use FillRect to see partially
                    // visible cell at right/bottom edge
                    FillRect(x + col*pmscale, y + row*pmscale, cellsize, cellsize);
                } else {
                    points[numcoords++] = midx;
                    points[numcoords++] = midy;
                }
            }
        }
    }

    if (numcoords > 0) {
        glVertexPointer(2, GL_FLOAT, 0, points);
        glDrawArrays(GL_POINTS, 0, numcoords/2);
    }
}

// -----------------------------------------------------------------------------

void DrawMagnifiedCells(unsigned char* statedata, int x, int y, int w, int h, int pmscale, int stride, int numstates)
{
    // called from golly_render::pixblit to draw cells magnified by pmscale (2, 4, ... 2^MAX_MAG)
    // when numstates is > 2
    int cellsize = pmscale > 2 ? pmscale - 1 : pmscale;

    const int maxcoords = 256;      // must be multiple of 2
    GLfloat points[256][maxcoords];
    int numcoords[256] = {0};

    unsigned char alpha = mask_dead_pixels ? live_alpha : 255;

    DisableTextures();
    SetPointSize(cellsize);

    // following code minimizes color changes due to state changes
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            unsigned char state = statedata[row*stride + col];
            if (state > 0) {
                if (numcoords[state] == maxcoords) {
                    // this shouldn't happen too often
                    SetColor(currlayer->cellr[state],
                             currlayer->cellg[state],
                             currlayer->cellb[state], alpha);
                    glVertexPointer(2, GL_FLOAT, 0, points[state]);
                    glDrawArrays(GL_POINTS, 0, numcoords[state]/2);
                    numcoords[state] = 0;
                }
                // get mid point of cell
                GLfloat midx = x + col*pmscale + cellsize/2.0;
                GLfloat midy = y + row*pmscale + cellsize/2.0;
                if (midx > float(currwd) || midy > float(currht)) {
                    // midx,midy is outside viewport so we need to use FillRect to see partially
                    // visible cell at right/bottom edge
                    SetColor(currlayer->cellr[state],
                             currlayer->cellg[state],
                             currlayer->cellb[state], alpha);
                    FillRect(x + col*pmscale, y + row*pmscale, cellsize, cellsize);
                } else {
                    points[state][numcoords[state]++] = midx;
                    points[state][numcoords[state]++] = midy;
                }
            }
        }
    }

    for (int state = 1; state < numstates; state++) {
        if (numcoords[state] > 0) {
            SetColor(currlayer->cellr[state],
                     currlayer->cellg[state],
                     currlayer->cellb[state], alpha);
            glVertexPointer(2, GL_FLOAT, 0, points[state]);
            glDrawArrays(GL_POINTS, 0, numcoords[state]/2);
        }
    }
}

// -----------------------------------------------------------------------------

class golly_render : public liferender
{
public:
    golly_render() {}
    virtual ~golly_render() {}
    virtual void killrect(int x, int y, int w, int h);
    virtual void pixblit(int x, int y, int w, int h, char* pm, int pmscale);
    virtual void getcolors(unsigned char** r, unsigned char** g, unsigned char** b);
};

golly_render renderer;     // create instance

// -----------------------------------------------------------------------------

void golly_render::killrect(int x, int y, int w, int h)
{
    // no need to do anything because background has already been filled by glClear in DrawView
}

// -----------------------------------------------------------------------------

void golly_render::pixblit(int x, int y, int w, int h, char* pmdata, int pmscale)
{
    if (x >= currwd || y >= currht) return;
    if (x + w <= 0 || y + h <= 0) return;
    
    // stride is the horizontal pixel width of the image data
    int stride = w/pmscale;
    
    // clip data outside viewport
    if (pmscale > 1) {
        // pmdata contains 1 byte per `pmscale' pixels, so we must be careful
        // and adjust x, y, w and h by multiples of `pmscale' only.
        if (x < 0) {
            int dx = -x/pmscale*pmscale;
            pmdata += dx/pmscale;
            w -= dx;
            x += dx;
        }
        if (y < 0) {
            int dy = -y/pmscale*pmscale;
            pmdata += dy/pmscale*stride;
            h -= dy;
            y += dy;
        }
        if (x + w >= currwd + pmscale) w = (currwd - x + pmscale - 1)/pmscale*pmscale;
        if (y + h >= currht + pmscale) h = (currht - y + pmscale - 1)/pmscale*pmscale;
    }
    
    int numstates = currlayer->algo->NumCellStates();
    
    if (pmscale == 1) {
        // draw RGB pixel data at scale 1:1
        if (drawing_paste) {
            // we can't use DrawTexture to draw paste image because glTexImage2D clobbers
            // any background pattern, so we use DrawPoints
            DrawPoints((unsigned char*) pmdata, x, y, w, h);
        } else if (mask_dead_pixels) {
            // convert RGB data to RGBA data where dead pixels are 100% transparent
            // and the transparency of live pixels depends on live_alpha
            DrawMaskedTexture((unsigned char*) pmdata, x, y, w, h);
        } else {
            DrawTexture((unsigned char*) pmdata, x, y, w, h);
        }
        
    } else if (showicons && pmscale > 4 && icontextures) {
        // draw icons at scales 1:8 and above
        DrawIcons((unsigned char*) pmdata, x, y, w/pmscale, h/pmscale, pmscale, stride);
        
    } else {
        // draw magnified cells, assuming pmdata contains (w/pmscale)*(h/pmscale) bytes
        // where each byte contains a cell state
        if (numstates == 2) {
            DrawMagnifiedTwoStateCells((unsigned char*) pmdata, x, y, w/pmscale, h/pmscale, pmscale, stride);
        } else {
            DrawMagnifiedCells((unsigned char*) pmdata, x, y, w/pmscale, h/pmscale, pmscale, stride, numstates);
        }
    }
}

// -----------------------------------------------------------------------------

void golly_render::getcolors(unsigned char** r, unsigned char** g, unsigned char** b)
{
    *r = currlayer->cellr;
    *g = currlayer->cellg;
    *b = currlayer->cellb;
}

// -----------------------------------------------------------------------------

void DrawSelection(wxRect& rect, bool active)
{
    // draw semi-transparent rectangle
    DisableTextures();
    if (active) {
        SetColor(selectrgb->Red(), selectrgb->Green(), selectrgb->Blue(), 128);
    } else {
        // use light gray to indicate an inactive selection
        SetColor(160, 160, 160, 128);
    }
    FillRect(rect.x, rect.y, rect.width, rect.height);
}

// -----------------------------------------------------------------------------

void CreatePasteImage(lifealgo* palgo, wxRect& bbox)
{
    pastealgo = palgo;      // save for use in CheckPasteImage
    pastebbox = bbox;       // ditto
    pastebitmap = NULL;
    prectwd = -1;           // force CheckPasteImage to update paste image
    prectht = -1;
    pimagewd = -1;          // force CheckPasteImage to rescale paste image
    pimageht = -1;
    pastemag = currlayer->view->getmag();
    pasteicons = showicons;
    pastecolors = swapcolors;
    pastescale = scalepatterns;
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

// remove eventually!!!

void MaskDeadPixels(wxBitmap* bitmap, int wd, int ht, int livealpha)
{
    // access pixels in given bitmap and make all dead pixels 100% transparent
    // and use given alpha value for all live pixels
    wxAlphaPixelData data(*bitmap, wxPoint(0,0), wxSize(wd,ht));
    if (data) {
        int deadr = currlayer->cellr[0];
        int deadg = currlayer->cellg[0];
        int deadb = currlayer->cellb[0];
        
#if !wxCHECK_VERSION(2,9,0)
        data.UseAlpha();
#endif
        wxAlphaPixelData::Iterator p(data);
        for ( int y = 0; y < ht; y++ ) {
            wxAlphaPixelData::Iterator rowstart = p;
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
#if defined(__WXMSW__) || (defined(__WXMAC__) && wxCHECK_VERSION(2,8,8))
                    // premultiply the RGB values
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
            p = rowstart;
            p.OffsetY(data, 1);
        }
    }
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
    // paste image needs to be updated if any of these changed:
    // pasterect size, viewport size, plocation, showicons, swapcolors, scalepatterns
    if ( prectwd != viewptr->pasterect.width ||
         prectht != viewptr->pasterect.height ||
         cvwd != currlayer->view->getwidth() ||
         cvht != currlayer->view->getheight() ||
         pasteloc != plocation ||
         pasteicons != showicons ||
         pastecolors != swapcolors ||
         pastescale != scalepatterns ) {
        
        prectwd = viewptr->pasterect.width;
        prectht = viewptr->pasterect.height;
        pastemag = currlayer->view->getmag();
        cvwd = currlayer->view->getwidth();
        cvht = currlayer->view->getheight();
        pasteloc = plocation;
        pasteicons = showicons;
        pastecolors = swapcolors;
        pastescale = scalepatterns;
        
        // calculate size of paste image; we could just set it to pasterect size
        // but that would be slow and wasteful for large pasterects, so we use
        // the following code (the only tricky bit is when plocation = Middle)
        int pastewd = prectwd;
        int pasteht = prectht;
        
        if (pastewd <= 2 || pasteht <= 2) {
            // no need to draw paste image because border lines will cover it
            delete pastebitmap;
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
        delete pastebitmap;
        pimagewd = pastewd;
        pimageht = pasteht;
        // create a bitmap with depth 32 so it has an alpha channel
        pastebitmap = new wxBitmap(pimagewd, pimageht, 32);
        
        if (pastebitmap) {
            // create temporary viewport and draw pattern into pastebitmap
            // for later use in DrawPasteImage
            viewport tempview(pimagewd, pimageht);
            int midx, midy;
            if (pastemag > 1) {
                // allow for gap between cells
                midx = cellbox.x + (cellbox.width - 1) / 2;
                midy = cellbox.y + (cellbox.height - 1) / 2;
            } else {
                midx = cellbox.x + cellbox.width / 2;
                midy = cellbox.y + cellbox.height / 2;
            }
            tempview.setpositionmag(midx, midy, pastemag);
            
            // temporarily turn off grid lines
            bool saveshow = showgridlines;
            showgridlines = false;
            drawing_paste = true;
            
            wxMemoryDC pattdc;
            pattdc.SelectObject(*pastebitmap);
            // remove!!! currdc = &pattdc;
            currwd = tempview.getwidth();
            currht = tempview.getheight();
            
            if (scalefactor > 1) {
                // change tempview scale to 1:1 and increase its size by scalefactor
                tempview.setmag(0);
                currwd = currwd * scalefactor;
                currht = currht * scalefactor;
                tempview.resize(currwd, currht);
                pastealgo->draw(tempview, renderer);
                // no need to restore tempview settings
            } else {
                // no scaling
                pastealgo->draw(tempview, renderer);
            }
            
            pattdc.SelectObject(wxNullBitmap);
            
            drawing_paste = false;
            showgridlines = saveshow;
            
            // make dead pixels 100% transparent and live pixels 100% opaque
            MaskDeadPixels(pastebitmap, pimagewd, pimageht, 255);
        }
    }
}

// -----------------------------------------------------------------------------

void DrawPasteImage(wxDC& dc)
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
        memdc.SelectObject(wxNullBitmap);
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

void DrawGridLines(int wd, int ht)
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
        // avoid gcc warning
        topbold = leftbold = 0;
    }

    DisableTextures();
    glLineWidth(1.0);

    // set the stroke color depending on current bg color
    int r = currlayer->cellr[0];
    int g = currlayer->cellg[0];
    int b = currlayer->cellb[0];
    int gray = (int) ((r + g + b) / 3.0);
    if (gray > 127) {
        // darker lines
        SetColor(r > 32 ? r - 32 : 0,
                 g > 32 ? g - 32 : 0,
                 b > 32 ? b - 32 : 0, 255);
    } else {
        // lighter lines
        SetColor(r + 32 < 256 ? r + 32 : 255,
                 g + 32 < 256 ? g + 32 : 255,
                 b + 32 < 256 ? b + 32 : 255, 255);
    }

    // draw all plain lines first;
    // note that we need to subtract 0.5 from each coordinate to avoid uneven spacing

    i = showboldlines ? topbold : 1;
    v = 0;
    while (true) {
        v += cellsize;
        if (v >= ht) break;
        if (showboldlines) i++;
        if (i % boldspacing != 0 && v >= 0 && v < ht) {
            GLfloat points[] = {   -0.5, v-0.5,
                                 wd-0.5, v-0.5 };
            glVertexPointer(2, GL_FLOAT, 0, points);
            glDrawArrays(GL_LINES, 0, 2);
        }
    }
    i = showboldlines ? leftbold : 1;
    h = 0;
    while (true) {
        h += cellsize;
        if (h >= wd) break;
        if (showboldlines) i++;
        if (i % boldspacing != 0 && h >= 0 && h < wd) {
            GLfloat points[] = { h-0.5,   -0.5,
                                 h-0.5, ht-0.5 };
            glVertexPointer(2, GL_FLOAT, 0, points);
            glDrawArrays(GL_LINES, 0, 2);
        }
    }

    if (showboldlines) {
        // draw bold lines in slightly darker/lighter color
        if (gray > 127) {
            // darker lines
            SetColor(r > 64 ? r - 64 : 0,
                     g > 64 ? g - 64 : 0,
                     b > 64 ? b - 64 : 0, 255);
        } else {
            // lighter lines
            SetColor(r + 64 < 256 ? r + 64 : 255,
                     g + 64 < 256 ? g + 64 : 255,
                     b + 64 < 256 ? b + 64 : 255, 255);
        }
        
        i = topbold;
        v = 0;
        while (true) {
            v += cellsize;
            if (v >= ht) break;
            i++;
            if (i % boldspacing == 0 && v >= 0 && v < ht) {
                GLfloat points[] = {   -0.5, v-0.5,
                                     wd-0.5, v-0.5 };
                glVertexPointer(2, GL_FLOAT, 0, points);
                glDrawArrays(GL_LINES, 0, 2);
            }
        }
        i = leftbold;
        h = 0;
        while (true) {
            h += cellsize;
            if (h >= wd) break;
            i++;
            if (i % boldspacing == 0 && h >= 0 && h < wd) {
                GLfloat points[] = { h-0.5,   -0.5,
                                     h-0.5, ht-0.5 };
                glVertexPointer(2, GL_FLOAT, 0, points);
                glDrawArrays(GL_LINES, 0, 2);
            }
        }
    }
}

// -----------------------------------------------------------------------------

void DrawGridBorder(int wd, int ht)
{
    // universe is bounded so draw any visible border regions
    pair<int,int> ltpxl = currlayer->view->screenPosOf(currlayer->algo->gridleft,
                                                       currlayer->algo->gridtop,
                                                       currlayer->algo);
    pair<int,int> rbpxl = currlayer->view->screenPosOf(currlayer->algo->gridright,
                                                       currlayer->algo->gridbottom,
                                                       currlayer->algo);
    int left = ltpxl.first;
    int top = ltpxl.second;
    int right = rbpxl.first;
    int bottom = rbpxl.second;
    if (currlayer->algo->gridwd == 0) {
        left = 0;
        right = wd-1;
    }
    if (currlayer->algo->gridht == 0) {
        top = 0;
        bottom = ht-1;
    }

    // note that right and/or bottom might be INT_MAX so avoid adding to cause overflow
    if (currlayer->view->getmag() > 0) {
        // move to bottom right pixel of cell at gridright,gridbottom
        if (right < wd) right += (1 << currlayer->view->getmag()) - 1;
        if (bottom < ht) bottom += (1 << currlayer->view->getmag()) - 1;
        if (currlayer->view->getmag() == 1) {
            // there are no gaps at scale 1:2
            if (right < wd) right++;
            if (bottom < ht) bottom++;
        }
    } else {
        if (right < wd) right++;
        if (bottom < ht) bottom++;
    }

    if (left < 0 && right >= wd && top < 0 && bottom >= ht) {
        // border isn't visible (ie. grid fills viewport)
        return;
    }

    DisableTextures();
    SetColor(borderrgb->Red(), borderrgb->Green(), borderrgb->Blue(), 255);

    if (left >= wd || right < 0 || top >= ht || bottom < 0) {
        // no part of grid is visible so fill viewport with border
        FillRect(0, 0, wd, ht);
        return;
    }

    // avoid drawing overlapping rects below
    int rtop = 0;
    int rheight = ht;

    if (currlayer->algo->gridht > 0) {
        if (top > 0) {
            // top border is visible
            FillRect(0, 0, wd, top);
            // reduce size of rect below
            rtop = top;
            rheight -= top;
        }
        if (bottom < ht) {
            // bottom border is visible
            FillRect(0, bottom, wd, ht - bottom);
            // reduce size of rect below
            rheight -= ht - bottom;
        }
    }

    if (currlayer->algo->gridwd > 0) {
        if (left > 0) {
            // left border is visible
            FillRect(0, rtop, left, rheight);
        }
        if (right < wd) {
            // right border is visible
            FillRect(right, rtop, wd - right, rheight);
        }
    }
}

// -----------------------------------------------------------------------------

void DrawOneLayer()
{
    // dead pixels will be 100% transparent, and live pixels will use opacity setting
    mask_dead_pixels = true;
    live_alpha = int(2.55 * opacity);
    int texbytes;
    int numstates = currlayer->algo->NumCellStates();
    
    if (showicons && currlayer->view->getmag() > 2) {
        // only show icons at scales 1:8 and above
        if (currlayer->view->getmag() == 3) {
            icontextures = currlayer->textures7x7;
            texbytes = 8*8*4;
        } else if (currlayer->view->getmag() == 4) {
            icontextures = currlayer->textures15x15;
            texbytes = 16*16*4;
        } else {
            icontextures = currlayer->textures31x31;
            texbytes = 32*32*4;
        }
        
        // DANGER: we're making assumptions about what CreateIconTextures does
        // (see wxlayer.cpp)
        
        if (live_alpha < 255) {
            // this is ugly, but we need to replace the alpha 255 values in
            // icontextures[1..numstates-1] with live_alpha (2..249)
            // so that DrawIcons displays translucent icons
            for (int state = 1; state < numstates; state++) {
                if (icontextures[state]) {
                    unsigned char* b = icontextures[state];
                    int i = 3;
                    while (i < texbytes) {
                        if (b[i] == 255) b[i] = live_alpha;
                        i += 4;
                    }
                }
            }
        }
    }
    
    if (scalefactor > 1) {
        // temporarily change viewport scale to 1:1 and increase its size by scalefactor
        int currmag = currlayer->view->getmag();
        currlayer->view->setmag(0);
        currwd = currwd * scalefactor;
        currht = currht * scalefactor;
        currlayer->view->resize(currwd, currht);
        
        glPushMatrix();
        glScalef(1.0/scalefactor, 1.0/scalefactor, 1.0);
        
        currlayer->algo->draw(*currlayer->view, renderer);
        
        // restore viewport settings
        currwd = currwd / scalefactor;
        currht = currht / scalefactor;
        currlayer->view->resize(currwd, currht);
        currlayer->view->setmag(currmag);
        
        // restore OpenGL scale
        glPopMatrix();
        
    } else {
        currlayer->algo->draw(*currlayer->view, renderer);
    }
    
    mask_dead_pixels = false;

    if (showicons && currlayer->view->getmag() > 2 && live_alpha < 255) {
        // restore alpha values that were changed above
        for (int state = 1; state < numstates; state++) {
            if (icontextures[state]) {
                unsigned char* b = icontextures[state];
                int i = 3;
                while (i < texbytes) {
                    if (b[i] == live_alpha) b[i] = 255;
                    i += 4;
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------

void DrawStackedLayers()
{
    // temporarily turn off grid lines
    bool saveshow = showgridlines;
    showgridlines = false;
    
    // overlay patterns in layers 1..numlayers-1
    for ( int i = 1; i < numlayers; i++ ) {
        Layer* savelayer = currlayer;
        currlayer = GetLayer(i);
        
        // use real current layer's viewport
        viewport* saveview = currlayer->view;
        currlayer->view = savelayer->view;
        
        if ( !currlayer->algo->isEmpty() ) {
            DrawOneLayer();
        }
        
        // draw this layer's selection if necessary
        wxRect r;
        if ( currlayer->currsel.Visible(&r) ) {
            DrawSelection(r, i == currindex);
        }
        
        // restore viewport and currlayer
        currlayer->view = saveview;
        currlayer = savelayer;
    }
    
    showgridlines = saveshow;
}

// -----------------------------------------------------------------------------

void DrawTileFrame(wxRect& trect, int wd)
{
    trect.Inflate(wd);
    wxRect r = trect;
    
    r.height = wd;
    FillRect(r.x, r.y, r.width, r.height);       // top edge
    
    r.y += trect.height - wd;
    FillRect(r.x, r.y, r.width, r.height);       // bottom edge
    
    r = trect;
    r.width = wd;
    FillRect(r.x, r.y, r.width, r.height);       // left edge
    
    r.x += trect.width - wd;
    FillRect(r.x, r.y, r.width, r.height);       // right edge
}

// -----------------------------------------------------------------------------

void DrawTileBorders()
{
    if (tileborder <= 0) return;    // no borders
    
    // draw tile borders in bigview window
    int wd, ht;
    bigview->GetClientSize(&wd, &ht);
    if (wd < 1 || ht < 1) return;
    
    // most people will choose either a very light or very dark color for dead cells,
    // so draw mid gray border around non-current tiles
    DisableTextures();
    SetColor(144, 144, 144, 255);
    wxRect trect;
    for ( int i = 0; i < numlayers; i++ ) {
        if (i != currindex) {
            trect = GetLayer(i)->tilerect;
            DrawTileFrame(trect, tileborder);
        }
    }
    
    // draw green border around current tile
    trect = GetLayer(currindex)->tilerect;
    SetColor(0, 255, 0, 255);
    DrawTileFrame(trect, tileborder);
}

// -----------------------------------------------------------------------------

void DrawView(int tileindex)
{
    wxRect r;
    Layer* savelayer = NULL;
    viewport* saveview0 = NULL;
    int colorindex;
    int currmag = currlayer->view->getmag();

    // fill the background with state 0 color
    glClearColor(currlayer->cellr[0]/255.0,
                 currlayer->cellg[0]/255.0,
                 currlayer->cellb[0]/255.0,
                 1.0);
    glClear(GL_COLOR_BUFFER_BIT /* | GL_DEPTH_BUFFER_BIT  no need???!!! */);
    
    // if grid is bounded then ensure viewport's central cell is not outside grid edges
    if ( currlayer->algo->gridwd > 0) {
        if ( currlayer->view->x < currlayer->algo->gridleft )
            currlayer->view->setpositionmag(currlayer->algo->gridleft,
                                            currlayer->view->y,
                                            currmag);
        else if ( currlayer->view->x > currlayer->algo->gridright )
            currlayer->view->setpositionmag(currlayer->algo->gridright,
                                            currlayer->view->y,
                                            currmag);
    }
    if ( currlayer->algo->gridht > 0) {
        if ( currlayer->view->y < currlayer->algo->gridtop )
            currlayer->view->setpositionmag(currlayer->view->x,
                                            currlayer->algo->gridtop,
                                            currmag);
        else if ( currlayer->view->y > currlayer->algo->gridbottom )
            currlayer->view->setpositionmag(currlayer->view->x,
                                            currlayer->algo->gridbottom,
                                            currmag);
    }
    
    if ( viewptr->nopattupdate ) {
        // don't draw incomplete pattern, just draw grid lines and border
        currwd = currlayer->view->getwidth();
        currht = currlayer->view->getheight();
        // might as well draw grid lines and border
        if ( viewptr->GridVisible() ) {
            DrawGridLines(currwd, currht);
        }
        if ( currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0 ) {
            DrawGridBorder(currwd, currht);
        }
        return;
    }
    
    if ( numlayers > 1 && tilelayers ) {
        if ( tileindex < 0 ) {
            DrawTileBorders();
            return;
        }
        // tileindex >= 0 so temporarily change some globals to draw this tile
        if ( syncviews && tileindex != currindex ) {
            // make sure this layer uses same location and scale as current layer
            GetLayer(tileindex)->view->setpositionmag(currlayer->view->x,
                                                      currlayer->view->y,
                                                      currmag);
        }
        savelayer = currlayer;
        currlayer = GetLayer(tileindex);
        currmag = currlayer->view->getmag();    // possibly changed if not syncviews
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
    
    if (showicons && currmag > 2) {
        // only show icons at scales 1:8 and above
        if (currmag == 3) {
            icontextures = currlayer->textures7x7;
        } else if (currmag == 4) {
            icontextures = currlayer->textures15x15;
        } else {
            icontextures = currlayer->textures31x31;
        }
    }
    
    // draw pattern using a sequence of pixblit calls
    currwd = currlayer->view->getwidth();
    currht = currlayer->view->getheight();
    if (scalepatterns && currmag <= -1 && currmag >= -4) {
        // current scale is from 2^1:1 to 2^4:1
        scalefactor = 1 << (-currmag);
        
        // temporarily change viewport scale to 1:1 and increase its size by scalefactor
        currlayer->view->setmag(0);
        currwd = currwd * scalefactor;
        currht = currht * scalefactor;
        currlayer->view->resize(currwd, currht);
        
        glPushMatrix();
        glScalef(1.0/scalefactor, 1.0/scalefactor, 1.0);
        
        currlayer->algo->draw(*currlayer->view, renderer);
        
        // restore viewport settings
        currwd = currwd / scalefactor;
        currht = currht / scalefactor;
        currlayer->view->resize(currwd, currht);
        currlayer->view->setmag(currmag);
        
        // restore OpenGL scale
        glPopMatrix();
        
    } else {
        // no scaling
        scalefactor = 1;
        currlayer->algo->draw(*currlayer->view, renderer);
    }
    
    if ( viewptr->GridVisible() ) {
        DrawGridLines(currwd, currht);
    }
    
    // if universe is bounded then draw border regions (if visible)
    if ( currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0 ) {
        DrawGridBorder(currwd, currht);
    }
    
    if ( currlayer->currsel.Visible(&r) ) {
        DrawSelection(r, colorindex == currindex);
    }
    
    if ( numlayers > 1 && stacklayers ) {
        // must restore currlayer before we call DrawStackedLayers
        currlayer = savelayer;
        if ( saveview0 ) {
            // restore layer 0's viewport
            GetLayer(0)->view = saveview0;
        }
        // draw layers 1, 2, ... numlayers-1
        DrawStackedLayers();
    }
    
    if ( viewptr->waitingforclick && viewptr->pasterect.width > 0 ) {
        // this test is not really necessary, but it avoids unnecessary
        // drawing of the paste image when user changes scale
        if (pastemag != currmag &&
            prectwd == viewptr->pasterect.width && prectwd > 1 &&
            prectht == viewptr->pasterect.height && prectht > 1) {
            // don't draw old paste image, a new one is coming very soon
        } else {
            CheckPasteImage();
            //!!! DrawPasteImage();
        }
    }
    
    if (viewptr->showcontrols) {
        //!!! DrawControls(dc, viewptr->controlsrect);
    }
    
    if ( numlayers > 1 && tilelayers ) {
        // restore globals changed above
        currlayer = savelayer;
        viewptr = currlayer->tilewin;
    }
}
