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

All drawing in the viewport is done in this module using OpenGL 1.

The main rendering routine is DrawView() -- see the end of this module.
DrawView() is called from PatternView::OnPaint(), the update event handler
for the viewport window.  Update events are created automatically by the
wxWidgets event dispatcher, or they can be created manually by calling
Refresh().

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

- If the user is doing a paste, DrawPasteImage() creates a suitable
  viewport and draws the paste pattern (stored in pastelayer).

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
#include "wxutils.h"       // for Warning
#include "wxprefs.h"       // for showgridlines, mingridmag, swapcolors, etc
#include "wxstatus.h"      // for statusptr->...
#include "wxview.h"        // for viewptr->...
#include "wxlayer.h"       // currlayer, GetLayer, etc
#include "wxrender.h"

// -----------------------------------------------------------------------------

// local data used in golly_render routines

int currwd, currht;                     // current width and height of viewport, in pixels
int scalefactor;                        // current scale factor (1, 2, 4, 8 or 16)
unsigned char dead_alpha = 255;         // alpha value for dead pixels
unsigned char live_alpha = 255;         // alpha value for live pixels
GLuint rgbatexture = 0;                 // texture name for drawing RGBA bitmaps
GLuint icontexture = 0;                 // texture name for drawing icons
GLuint celltexture = 0;                 // texture name for drawing magnified cells
unsigned char* iconatlas = NULL;        // pointer to texture atlas for current set of icons
unsigned char* cellatlas = NULL;        // pointer to texture atlas for current set of magnified cells

// cellatlas needs to be rebuilt if any of these parameters change
int prevnum = 0;                        // previous number of live states
int prevsize = 0;                       // previous cell size
unsigned char prevalpha;                // previous alpha component
unsigned char prevr[256];               // previous red component of each state
unsigned char prevg[256];               // previous green component of each state
unsigned char prevb[256];               // previous blue component of each state

// fixed texture coordinates used by glTexCoordPointer
static const GLshort texture_coordinates[] = { 0,0, 1,0, 0,1, 1,1 };

// for drawing paste pattern
Layer* pastelayer;                      // layer containing the paste pattern
wxRect pastebbox;                       // bounding box in cell coords (not necessarily minimal)
unsigned char* modedata[4] = {NULL};    // RGBA data for drawing current paste mode (And, Copy, Or, Xor)
const int modewd = 32;                  // width of each modedata
const int modeht = 16;                  // height of each modedata

// for drawing translucent controls
unsigned char* ctrlsbitmap = NULL;      // RGBA data for controls bitmap
unsigned char* darkbutt = NULL;         // RGBA data for darkening one button  
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

#ifdef __WXMSW__
    // this constant isn't defined in the OpenGL headers on Windows (XP at least)
    #define GL_CLAMP_TO_EDGE 0x812F
#endif

// -----------------------------------------------------------------------------

static void SetColor(int r, int g, int b, int a)
{
    glColor4ub(r, g, b, a);
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

static void EnableTextures()
{
    if (!glIsEnabled(GL_TEXTURE_2D)) {
        // restore texture color and enable textures
        SetColor(255, 255, 255, 255);
        glEnable(GL_TEXTURE_2D);
    }
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
    
    // create ctrlsbitmap and initialize its RGBA data based on pixels in image
    ctrlsbitmap = (unsigned char*) malloc(controlswd * controlsht * 4);
    if (ctrlsbitmap) {
        int p = 0;
        for ( int y = 0; y < controlsht; y++ ) {
            for ( int x = 0; x < controlswd; x++ ) {
                unsigned char r = image.GetRed(x,y);
                unsigned char g = image.GetGreen(x,y);
                unsigned char b = image.GetBlue(x,y);
                if (r == 0 && g == 0 && b == 0) {
                    // make black pixel 100% transparent
                    ctrlsbitmap[p++] = 0;
                    ctrlsbitmap[p++] = 0;
                    ctrlsbitmap[p++] = 0;
                    ctrlsbitmap[p++] = 0;
                } else {
                    // make all non-black pixels translucent
                    ctrlsbitmap[p++] = r;
                    ctrlsbitmap[p++] = g;
                    ctrlsbitmap[p++] = b;
                    ctrlsbitmap[p++] = 192;     // 75% opaque
                }
            }
        }
    }
    
    // allocate bitmap for darkening a clicked button
    darkbutt = (unsigned char*) malloc(buttsize * buttsize * 4);
    
    // create bitmaps for drawing each paste mode
    paste_mode savemode = pmode;
    for (int i = 0; i < 4; i++) {
        pmode = (paste_mode) i;
        wxString pmodestr = wxString(GetPasteMode(),wxConvLocal);   // uses pmode

        wxBitmap modemap(modewd, modeht, 32);
        wxMemoryDC dc;
        dc.SelectObject(modemap);

        wxRect r(0, 0, modewd, modeht);
        wxBrush brush(*wxWHITE);
        FillRect(dc, r, brush);
        
        dc.SetFont(*statusptr->GetStatusFont());
        dc.SetBackgroundMode(wxSOLID);
        dc.SetTextBackground(*wxWHITE);
        dc.SetTextForeground(*wxBLACK);
        dc.SetPen(*wxBLACK);
        
        int textwd, textht;
        dc.GetTextExtent(pmodestr, &textwd, &textht);
        textwd += 4;
        dc.DrawText(pmodestr, 2, modeht - statusptr->GetTextAscent() - 4);
        
        dc.SelectObject(wxNullBitmap);
        
        // now convert modemap data into RGBA data suitable for passing into DrawRGBAData
        modedata[i] = (unsigned char*) malloc(modewd * modeht * 4);
        if (modedata[i]) {
            int j = 0;
            unsigned char* m = modedata[i];
            wxAlphaPixelData data(modemap);
            if (data) {
                wxAlphaPixelData::Iterator p(data);
                for (int y = 0; y < modeht; y++) {
                    wxAlphaPixelData::Iterator rowstart = p;
                    for (int x = 0; x < modewd; x++) {
                        if (x > textwd) {
                            m[j++] = 0;
                            m[j++] = 0;
                            m[j++] = 0;
                            m[j++] = 0;
                        } else {
                            m[j++] = p.Red();
                            m[j++] = p.Green();
                            m[j++] = p.Blue();
                            m[j++] = 255;
                        }
                        p++;
                    }
                    p = rowstart;
                    p.OffsetY(data, 1);
                }
            }
        }
    }
    pmode = savemode;
}

// -----------------------------------------------------------------------------

void DestroyDrawingData()
{
    if (cellatlas) free(cellatlas);
    if (ctrlsbitmap) free(ctrlsbitmap);
    if (darkbutt) free(darkbutt);
    for (int i = 0; i < 4; i++) {
        if (modedata[i]) free(modedata[i]);
    }
}

// -----------------------------------------------------------------------------

void DrawRGBAData(unsigned char* rgbadata, int x, int y, int w, int h)
{
    // only need to create texture name once
	if (rgbatexture == 0) glGenTextures(1, &rgbatexture);

    EnableTextures();
    glBindTexture(GL_TEXTURE_2D, rgbatexture);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);    // avoids edge effects when scaling
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);    // ditto
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexCoordPointer(2, GL_SHORT, 0, texture_coordinates);
    
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

static void LoadIconAtlas(int iconsize, int numicons)
{
    // load the texture atlas containing all icons for later use in DrawIcons
    
    // create icon texture name once
	if (icontexture == 0) glGenTextures(1, &icontexture);
    
    EnableTextures();
    glBindTexture(GL_TEXTURE_2D, icontexture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    int atlaswd = iconsize * numicons;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlaswd, iconsize, 0, GL_RGBA, GL_UNSIGNED_BYTE, iconatlas);
    
#if 0
    // test above code by displaying the entire atlas
    GLfloat wd = atlaswd;
    GLfloat ht = iconsize;
    glTexCoordPointer(2, GL_SHORT, 0, texture_coordinates);
    int x = 0;
    int y = 0;
    GLfloat vertices[] = {
        x,      y,
        x + wd, y,
        x,      y + ht,
        x + wd, y + ht,
    };
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
#endif
}

// -----------------------------------------------------------------------------

void DrawIcons(unsigned char* statedata, int x, int y, int w, int h, int pmscale, int stride, int numicons)
{
    // called from golly_render::pixblit to draw icons for each live cell;
    // assume pmscale > 2 (should be 8, 16 or 32 -- if higher then the 31x31 icons
    // will be scaled up)

    // one icon = 2 triangles = 4 vertices = 8 coordinates for GL_TRIANGLE_STRIP:
    //
    //   0,1 *---* 2,3
    //       | / |
    //   4,5 *---* 6,7
    //
    GLfloat vertices[8];
    GLfloat texcoords[8];

    EnableTextures();
    glBindTexture(GL_TEXTURE_2D, icontexture);

    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            unsigned char state = statedata[row*stride + col];
            if (state > 0) {
                int xpos = x + col * pmscale;
                int ypos = y + row * pmscale;

                vertices[0] = xpos;
                vertices[1] = ypos;
                vertices[2] = xpos + pmscale;
                vertices[3] = ypos;
                vertices[4] = xpos;
                vertices[5] = ypos + pmscale;
                vertices[6] = xpos + pmscale;
                vertices[7] = ypos + pmscale;
                
                texcoords[0] = (state-1) * 1.0/numicons;
                texcoords[1] = 0.0;
                texcoords[2] = state * 1.0/numicons;
                texcoords[3] = 0.0;
                texcoords[4] = texcoords[0];
                texcoords[5] = 1.0;
                texcoords[6] = texcoords[2];
                texcoords[7] = 1.0;

                glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
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
    // draw a single icon (either multi-color or grayscale) outside the viewport,
    // so we don't use OpenGL calls in here
    
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

static bool ChangeCellAtlas(int cellsize, int numcells, unsigned char alpha)
{
    if (numcells != prevnum) return true;
    if (cellsize != prevsize) return true;
    if (alpha != prevalpha) return true;
    
    for (int state = 1; state <= numcells; state++) {
        if (currlayer->cellr[state] != prevr[state]) return true;
        if (currlayer->cellg[state] != prevg[state]) return true;
        if (currlayer->cellb[state] != prevb[state]) return true;
    }
    
    return false;   // no need to change cellatlas
}

// -----------------------------------------------------------------------------

static void LoadCellAtlas(int cellsize, int numcells, unsigned char alpha)
{
    // cellatlas might need to be (re)created
    if (ChangeCellAtlas(cellsize, numcells, alpha)) {
        prevnum = numcells;
        prevsize = cellsize;
        prevalpha = alpha;
        for (int state = 1; state <= numcells; state++) {
            prevr[state] = currlayer->cellr[state];
            prevg[state] = currlayer->cellg[state];
            prevb[state] = currlayer->cellb[state];
        }
        
        if (cellatlas) free(cellatlas);
        
        // allocate enough memory for texture atlas to store RGBA pixels for a row of cells
        // (note that we use calloc so all alpha bytes are initially 0)
        int rowbytes = numcells * cellsize * 4;
        cellatlas = (unsigned char*) calloc(rowbytes * cellsize, 1);
        
        if (cellatlas) {
            // set pixels in top row
            int tpos = 0;
            for (int state = 1; state <= numcells; state++) {
                unsigned char r = currlayer->cellr[state];
                unsigned char g = currlayer->cellg[state];
                unsigned char b = currlayer->cellb[state];
                
                // if the cell size is > 2 then there is a 1 pixel gap at right and bottom edge of each cell
                int cellwd = cellsize > 2 ? cellsize-1 : 2;
                for (int i = 0; i < cellwd; i++) {
                    cellatlas[tpos++] = r;
                    cellatlas[tpos++] = g;
                    cellatlas[tpos++] = b;
                    cellatlas[tpos++] = alpha;
                }
                if (cellsize > 2) tpos += 4;    // skip transparent pixel at right edge of cell
            }
            // copy top row to remaining rows
            int remrows = cellsize > 2 ? cellsize - 2 : 1;
            for (int i = 1; i <= remrows; i++) {
                memcpy(&cellatlas[i * rowbytes], cellatlas, rowbytes);
            }
        }
    }
    
    // create cell texture name once
	if (celltexture == 0) glGenTextures(1, &celltexture);
    
    EnableTextures();
    glBindTexture(GL_TEXTURE_2D, celltexture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // load the texture atlas containing all magnified cells for later use in DrawMagnifiedCells
    int atlaswd = cellsize * numcells;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlaswd, cellsize, 0, GL_RGBA, GL_UNSIGNED_BYTE, cellatlas);
    
#if 0
    // test above code by displaying the entire atlas
    GLfloat wd = atlaswd;
    GLfloat ht = cellsize;
    glTexCoordPointer(2, GL_SHORT, 0, texture_coordinates);
    int x = 0;
    int y = 0;
    GLfloat vertices[] = {
        x,      y,
        x + wd, y,
        x,      y + ht,
        x + wd, y + ht,
    };
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
#endif
}

// -----------------------------------------------------------------------------

void DrawMagnifiedCells(unsigned char* statedata, int x, int y, int w, int h, int pmscale, int stride, int numcells)
{
    // called from golly_render::pixblit to draw cells magnified by pmscale (2, 4, ... 2^MAX_MAG)

    // one cell = 2 triangles = 4 vertices = 8 coordinates for GL_TRIANGLE_STRIP:
    //
    //   0,1 *---* 2,3
    //       | / |
    //   4,5 *---* 6,7
    //
    GLfloat vertices[8];
    GLfloat texcoords[8];

    EnableTextures();
    glBindTexture(GL_TEXTURE_2D, celltexture);

    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            unsigned char state = statedata[row*stride + col];
            if (state > 0) {
                int xpos = x + col * pmscale;
                int ypos = y + row * pmscale;

                vertices[0] = xpos;
                vertices[1] = ypos;
                vertices[2] = xpos + pmscale;
                vertices[3] = ypos;
                vertices[4] = xpos;
                vertices[5] = ypos + pmscale;
                vertices[6] = xpos + pmscale;
                vertices[7] = ypos + pmscale;
                
                texcoords[0] = (state-1) * 1.0/numcells;
                texcoords[1] = 0.0;
                texcoords[2] = state * 1.0/numcells;
                texcoords[3] = 0.0;
                texcoords[4] = texcoords[0];
                texcoords[5] = 1.0;
                texcoords[6] = texcoords[2];
                texcoords[7] = 1.0;

                glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
                glVertexPointer(2, GL_FLOAT, 0, vertices);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }
        }
    }
}

// -----------------------------------------------------------------------------

class golly_render : public liferender
{
public:
    golly_render() {}
    virtual ~golly_render() {}
    virtual void pixblit(int x, int y, int w, int h, unsigned char* pm, int pmscale);
    virtual void getcolors(unsigned char** r, unsigned char** g, unsigned char** b,
                           unsigned char* deada, unsigned char* livea);
};

golly_render renderer;     // create instance

// -----------------------------------------------------------------------------

void golly_render::pixblit(int x, int y, int w, int h, unsigned char* pmdata, int pmscale)
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
    
    if (pmscale == 1) {
        // draw RGBA pixel data at scale 1:1
        DrawRGBAData(pmdata, x, y, w, h);
        
    } else if (showicons && pmscale > 4 && iconatlas) {
        // draw icons at scales 1:8 and above
        DrawIcons(pmdata, x, y, w/pmscale, h/pmscale, pmscale, stride, currlayer->numicons);
        
    } else {
        // draw magnified cells, assuming pmdata contains (w/pmscale)*(h/pmscale) bytes
        // where each byte contains a cell state
        DrawMagnifiedCells(pmdata, x, y, w/pmscale, h/pmscale, pmscale, stride, currlayer->numicons);
    }
}

// -----------------------------------------------------------------------------

void golly_render::getcolors(unsigned char** r, unsigned char** g, unsigned char** b,
                             unsigned char* deada, unsigned char* livea)
{
    *r = currlayer->cellr;
    *g = currlayer->cellg;
    *b = currlayer->cellb;
    *deada = dead_alpha;
    *livea = live_alpha;
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

void InitPaste(Layer* player, wxRect& bbox)
{
    // set globals used in DrawPasteImage
    pastelayer = player;
    pastebbox = bbox;
}

// -----------------------------------------------------------------------------

int PixelsToCells(int pixels, int mag) {
    // convert given # of screen pixels to corresponding # of cells
    if (mag >= 0) {
        int cellsize = 1 << mag;
        return (pixels + cellsize - 1) / cellsize;
    } else {
        // mag < 0; no need to worry about overflow
        return pixels << -mag;
    }
}

// -----------------------------------------------------------------------------

void DrawPasteImage()
{
    int pastemag = currlayer->view->getmag();
    
    // note that viewptr->pasterect.width > 0
    int prectwd = viewptr->pasterect.width;
    int prectht = viewptr->pasterect.height;
    
    // calculate size of paste image; we could just set it to pasterect size
    // but that would be slow and wasteful for large pasterects, so we use
    // the following code (the only tricky bit is when plocation = Middle)
    int pastewd = prectwd;
    int pasteht = prectht;
    
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
            cellbox.width = PixelsToCells(pastewd, pastemag);
            cellbox.height = PixelsToCells(pasteht, pastemag);
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
    
    wxRect r = viewptr->pasterect;
    if (r.width > pastewd || r.height > pasteht) {
        // paste image is smaller than pasterect (which can't fit in viewport)
        // so shift image depending on plocation
        switch (plocation) {
            case TopLeft:
                // no need to do any shifting
                break;
            case TopRight:
                // shift image to top right corner of pasterect
                r.x += r.width - pastewd;
                break;
            case BottomRight:
                // shift image to bottom right corner of pasterect
                r.x += r.width - pastewd;
                r.y += r.height - pasteht;
                break;
            case BottomLeft:
                // shift image to bottom left corner of pasterect
                r.y += r.height - pasteht;
                break;
            case Middle:
                // shift image to middle of pasterect; note that above code
                // has ensured (r.width - pastewd) and (r.height - pasteht)
                // are an even number of *cells* if pastemag > 0
                r.x += (r.width - pastewd) / 2;
                r.y += (r.height - pasteht) / 2;
                break;
        }
    }
    
    // set up viewport for drawing paste pattern
    pastelayer->view->resize(pastewd, pasteht);
    int midx, midy;
    if (pastemag > 1) {
        // allow for gap between cells
        midx = cellbox.x + (cellbox.width - 1) / 2;
        midy = cellbox.y + (cellbox.height - 1) / 2;
    } else {
        midx = cellbox.x + cellbox.width / 2;
        midy = cellbox.y + cellbox.height / 2;
    }
    pastelayer->view->setpositionmag(midx, midy, pastemag);
    
    // temporarily turn off grid lines
    bool saveshow = showgridlines;
    showgridlines = false;
    
    // dead pixels will be 100% transparent and live pixels 100% opaque
    dead_alpha = 0;
    live_alpha = 255;

    currwd = pastelayer->view->getwidth();
    currht = pastelayer->view->getheight();
    
    glTranslatef(r.x, r.y, 0);

    // temporarily set currlayer to pastelayer so golly_render routines
    // will use the paste pattern's color and icons
    Layer* savelayer = currlayer;
    currlayer = pastelayer;

    if (showicons && pastemag > 2) {
        // only show icons at scales 1:8 and above
        if (pastemag == 3) {
            iconatlas = currlayer->atlas7x7;
            LoadIconAtlas(8, currlayer->numicons);
        } else if (pastemag == 4) {
            iconatlas = currlayer->atlas15x15;
            LoadIconAtlas(16, currlayer->numicons);
        } else {
            iconatlas = currlayer->atlas31x31;
            LoadIconAtlas(32, currlayer->numicons);
        }
    } else if (pastemag > 0) {
        LoadCellAtlas(1 << pastemag, currlayer->numicons, 255);
    }
    
    if (scalefactor > 1) {
        // change scale to 1:1 and increase its size by scalefactor
        pastelayer->view->setmag(0);
        currwd = currwd * scalefactor;
        currht = currht * scalefactor;
        pastelayer->view->resize(currwd, currht);
        
        glPushMatrix();
        glScalef(1.0/scalefactor, 1.0/scalefactor, 1.0);
        
        pastelayer->algo->draw(*pastelayer->view, renderer);
        
        // restore viewport settings
        currwd = currwd / scalefactor;
        currht = currht / scalefactor;
                
        // restore OpenGL scale
        glPopMatrix();
        
    } else {
        // no scaling
        pastelayer->algo->draw(*pastelayer->view, renderer);
    }
    
    currlayer = savelayer;
    showgridlines = saveshow;
    
    glTranslatef(-r.x, -r.y, 0);
    
    // overlay translucent rect to show paste area
    DisableTextures();
    SetColor(pastergb->Red(), pastergb->Green(), pastergb->Blue(), 64);
    r = viewptr->pasterect;
    FillRect(r.x, r.y, r.width, r.height);
    
    // show current paste mode
    if (r.y > 0 && modedata[(int)pmode]) {
        DrawRGBAData(modedata[(int)pmode], r.x, r.y - modeht - 1, modewd, modeht);
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

void DrawControls(wxRect& rect)
{
    if (ctrlsbitmap) {
        DrawRGBAData(ctrlsbitmap, rect.x, rect.y, controlswd, controlsht);
        
        if (currcontrol > NO_CONTROL && darkbutt) {
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
            
            // draw one darkened button
            int p = 0;
            for ( int row = 0; row < buttsize; row++ ) {
                for ( int col = 0; col < buttsize; col++ ) {
                    unsigned char alpha = ctrlsbitmap[((row + y) * controlswd + col + x) * 4 + 3];
                    if (alpha == 0) {
                        // pixel is transparent
                        darkbutt[p++] = 0;
                        darkbutt[p++] = 0;
                        darkbutt[p++] = 0;
                        darkbutt[p++] = 0;
                    } else {
                        // pixel is part of button so use a very dark gray
                        darkbutt[p++] = 20;
                        darkbutt[p++] = 20;
                        darkbutt[p++] = 20;
                        darkbutt[p++] = 128;    // 50% opaque
                    }
                }
            }
            DrawRGBAData(darkbutt, rect.x + x, rect.y + y, buttsize, buttsize);
        }
    }
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
    // note that we need to add/subtract 0.5 from coordinates to avoid uneven spacing

    i = showboldlines ? topbold : 1;
    v = 0;
    while (true) {
        v += cellsize;
        if (v > ht) break;
        if (showboldlines) i++;
        if (i % boldspacing != 0) {
            GLfloat points[] = {   -0.5, v-0.5,
                                 wd+0.5, v-0.5 };
            glVertexPointer(2, GL_FLOAT, 0, points);
            glDrawArrays(GL_LINES, 0, 2);
        }
    }
    i = showboldlines ? leftbold : 1;
    h = 0;
    while (true) {
        h += cellsize;
        if (h > wd) break;
        if (showboldlines) i++;
        if (i % boldspacing != 0) {
            GLfloat points[] = { h-0.5,   -0.5,
                                 h-0.5, ht+0.5 };
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
            if (v > ht) break;
            i++;
            if (i % boldspacing == 0) {
                GLfloat points[] = {   -0.5, v-0.5,
                                     wd+0.5, v-0.5 };
                glVertexPointer(2, GL_FLOAT, 0, points);
                glDrawArrays(GL_LINES, 0, 2);
            }
        }
        i = leftbold;
        h = 0;
        while (true) {
            h += cellsize;
            if (h > wd) break;
            i++;
            if (i % boldspacing == 0) {
                GLfloat points[] = { h-0.5,   -0.5,
                                     h-0.5, ht+0.5 };
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

void ReplaceAlpha(int iconsize, int numicons, unsigned char oldalpha, unsigned char newalpha)
{
    if (iconatlas) {
        int numbytes = numicons * iconsize * iconsize * 4;
        int i = 3;
        while (i < numbytes) {
            if (iconatlas[i] == oldalpha) iconatlas[i] = newalpha;
            i += 4;
        }
    }
}

// -----------------------------------------------------------------------------

void DrawOneLayer()
{
    // dead pixels will be 100% transparent, and live pixels will use opacity setting
    dead_alpha = 0;
    live_alpha = int(2.55 * opacity);
    
    int iconsize = 0;
    int currmag = currlayer->view->getmag();
    
    if (showicons && currmag > 2) {
        // only show icons at scales 1:8 and above
        if (currmag == 3) {
            iconatlas = currlayer->atlas7x7;
            iconsize = 8;
        } else if (currmag == 4) {
            iconatlas = currlayer->atlas15x15;
            iconsize = 16;
        } else {
            iconatlas = currlayer->atlas31x31;
            iconsize = 32;
        }
        
        // DANGER: we're making assumptions about what CreateIconAtlas does in wxlayer.cpp
        
        if (live_alpha < 255) {
            // this is ugly, but we need to replace the alpha 255 values in iconatlas
            // with live_alpha so that LoadIconAtlas will load translucent icons
            ReplaceAlpha(iconsize, currlayer->numicons, 255, live_alpha);
        }
        
        // load iconatlas for use by DrawIcons
        LoadIconAtlas(iconsize, currlayer->numicons);
    } else if (currmag > 0) {
        LoadCellAtlas(1 << currmag, currlayer->numicons, live_alpha);
    }
    
    if (scalefactor > 1) {
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
        currlayer->algo->draw(*currlayer->view, renderer);
    }

    if (showicons && currmag > 2 && live_alpha < 255) {
        // restore alpha values in iconatlas
        ReplaceAlpha(iconsize, currlayer->numicons, live_alpha, 255);
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
        // don't draw incomplete pattern, just fill background
        glClearColor(currlayer->cellr[0]/255.0,
                     currlayer->cellg[0]/255.0,
                     currlayer->cellb[0]/255.0,
                     1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        // might as well draw grid lines and border
        currwd = currlayer->view->getwidth();
        currht = currlayer->view->getheight();
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
            // there's no need to fill bigview's background
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

    // fill the background with the current state 0 color
    // (note that currlayer might have changed)
    glClearColor(currlayer->cellr[0]/255.0,
                 currlayer->cellg[0]/255.0,
                 currlayer->cellb[0]/255.0,
                 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    
    if (showicons && currmag > 2) {
        // only show icons at scales 1:8 and above
        if (currmag == 3) {
            iconatlas = currlayer->atlas7x7;
            LoadIconAtlas(8, currlayer->numicons);
        } else if (currmag == 4) {
            iconatlas = currlayer->atlas15x15;
            LoadIconAtlas(16, currlayer->numicons);
        } else {
            iconatlas = currlayer->atlas31x31;
            LoadIconAtlas(32, currlayer->numicons);
        }
    } else if (currmag > 0) {
        LoadCellAtlas(1 << currmag, currlayer->numicons, 255);
    }
    
    currwd = currlayer->view->getwidth();
    currht = currlayer->view->getheight();
    
    // all pixels are initially opaque
    dead_alpha = 255;
    live_alpha = 255;
    
    // draw pattern using a sequence of pixblit calls
    if (smartscale && currmag <= -1 && currmag >= -4) {
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
        DrawPasteImage();
    }
    
    if (viewptr->showcontrols) {
        DrawControls(viewptr->controlsrect);
    }
    
    if ( numlayers > 1 && tilelayers ) {
        // restore globals changed above
        currlayer = savelayer;
        viewptr = currlayer->tilewin;
    }
}
