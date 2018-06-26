// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "wx/wxprec.h"      // for compilers that support precompilation
#ifndef WX_PRECOMP
    #include "wx/wx.h"      // for all others include the necessary headers
#endif

#include "wx/rawbmp.h"      // for wxAlphaPixelData

#include "wxgolly.h"        // for mainptr, viewptr
#include "wxmain.h"         // for mainptr->...
#include "wxview.h"         // for viewptr->...
#include "wxlayer.h"        // for currlayer->...
#include "wxprefs.h"        // for showoverlay
#include "wxutils.h"        // for Warning

#include "wxoverlay.h"

#include <vector>           // for std::vector
#include <cstdio>           // for FILE*, etc
#include <math.h>           // for sin, cos, log, sqrt and atn2
#include <stdlib.h>         // for malloc, free
#include <string.h>         // for strchr, strtok
#include <ctype.h>          // for isspace, isdigit

// -----------------------------------------------------------------------------

class Clip {
public:
    Clip(int w, int h, bool use_calloc = false) {
        cwd = w;
        cht = h;
        if (use_calloc) {
            cdata = (unsigned char*) calloc(cwd * cht * 4, sizeof(*cdata));
        } else {
            cdata = (unsigned char*) malloc(cwd * cht * 4);
        }
        rowindex = NULL;
        hasindex = false;
    }

    ~Clip() {
        if (cdata) free(cdata);
        if (rowindex) free(rowindex);
    }

    // add row index to the clip
    int addIndex() {
        if (!rowindex) {
            // allocate the index
            rowindex = (unsigned char*) malloc(cht);
        }
        unsigned char* p = cdata;
        unsigned char alpha;
        unsigned char first;
        int j;
 
        // check each row
        int numtrans = 0;
        int numopaque = 0;
        for (int i = 0; i < cht; i++) {
            // check if row contains all transparent or all opaque pixels
            first = p[3];
            alpha = first;
            p += 4;
            j = 1;
            if (first == 0 || first == 255) {
                while (j < cwd && alpha == first) {
                    alpha = p[3];
                    p += 4;
                    j++;
                }
            }
            // set this row's flag
            if (alpha == 0 && first == 0) {
                numtrans++;
                rowindex[i] = 0;
            } else if (alpha == 255 && first == 255) {
                numopaque++;
                rowindex[i] = 255;
            } else {
                rowindex[i] = 128;  // anything but 0 or 255
            }
            p += (cwd - j) * 4;
        }
        // only enable the index if there were any transparent or opaque rows
        hasindex = (numtrans || numopaque) ? true : false;
        return numtrans;
    }

    // remove row index from the clip
    void removeIndex() {
        hasindex = false;
    }

    unsigned char* cdata;       // RGBA data (cwd * cht * 4 bytes)
    int cwd, cht;               // width and height of the clip in pixels
    unsigned char* rowindex;    // flag per row (0 if transparent, 255 if opaque)
    bool hasindex;              // whether the clip currently has a row index
};

// -----------------------------------------------------------------------------

// some useful macros:

#if !wxCHECK_VERSION(2,9,0)
    #define wxImageResizeQuality int
#endif

#ifdef __WXMSW__
    #define round(x) int( (x) < 0 ? (x)-0.5 : (x)+0.5 )
    #define remainder(n,d) ( (n) - round((n)/(d)) * (d) )
#endif

#define PixelInOverlay(x,y) \
    (x >= 0 && x < ovwd && \
     y >= 0 && y < ovht)

#define PixelInTarget(x,y) \
    (x >= 0 && x < wd && \
     y >= 0 && y < ht)

#define RectOutsideTarget(x,y,w,h) \
    (x >= wd || x + w <= 0 || \
     y >= ht || y + h <= 0)

#define RectInsideTarget(x,y,w,h) \
    (x >= 0 && x + w <= wd && \
     y >= 0 && y + h <= ht)

#define PixelsMatch(newpxl,oldr,oldg,oldb,olda) \
    (newpxl[0] == oldr && \
     newpxl[1] == oldg && \
     newpxl[2] == oldb && \
     newpxl[3] == olda)

// -----------------------------------------------------------------------------

Overlay* curroverlay = NULL;        // pointer to current overlay

const char* no_overlay = "overlay has not been created";
const char* no_cellview = "overlay does not have a cell view";

const int cellviewmaxsize = 4096;   // maximum dimension for cell view
const int cellviewmultiple = 16;    // cellview dimensions must be a multiple of this value

// for camera
const double camminzoom = 0.0625;   // minimum zoom
const double cammaxzoom = 32.0;     // maximum zoom

// for theme
const int aliveStart = 64;          // new cell color index
const int aliveEnd = 127;           // cell alive longest color index
const int deadStart = 63;           // cell just died color index
const int deadEnd = 1;              // cell dead longest color index

// for stars
const int numStars = 10000;         // number of stars in starfield
const int starMaxX = 8192;
const int starMaxY = 8192;
const int starMaxZ = 1024;
const double degToRad = M_PI / 180;
const double radToDeg = 180 / M_PI;

// for replace
const int matchany = -1;            // match any component value

#ifdef __WXMAC__
    // on Mac we'll need to increase the line height of text by 1 or 2 pixels to avoid
    // a GetTextExtent bug that clips the bottom pixels of descenders like "gjpqy"
    int extraht;
#endif

// -----------------------------------------------------------------------------

Overlay::Overlay()
{
    pixmap = NULL;
    ovpixmap = NULL;
    cellview = NULL;
    cellview1 = NULL;
    zoomview = NULL;
    starx = NULL;
    stary = NULL;
    starz = NULL;
    renderclip = NULL;
    #ifdef ENABLE_SOUND
    engine = NULL;
    #endif
}

// -----------------------------------------------------------------------------

Overlay::~Overlay()
{
    DeleteOverlay();
}

// -----------------------------------------------------------------------------

void Overlay::DeleteOverlay()
{
    if (ovpixmap) {
        free(ovpixmap);
        ovpixmap = NULL;
    }
    pixmap = NULL;

    // delete clips
    std::map<std::string,Clip*>::iterator it;
    for (it = clips.begin(); it != clips.end(); ++it) {
        delete it->second;
    }
    clips.clear();

    #ifdef ENABLE_SOUND
    // stop any sound playback and delete cached sounds
    if (engine) {
        // delete sounds
        std::map<std::string,ISound*>::iterator itsnd;
        for (itsnd = sounds.begin(); itsnd != sounds.end(); ++itsnd) {
            itsnd->second->drop();
        }
        sounds.clear();

        // delete engine
        engine->drop();
        engine = NULL;
    }
    #endif

    // delete cellview
    DeleteCellView();
}

// -----------------------------------------------------------------------------

void Overlay::DeleteStars()
{
    if (starx) {
        free(starx);
        starx = NULL;
    }
    if (stary) {
        free(stary);
        stary = NULL;
    }
    if (starz) {
        free(starz);
        starz = NULL;
    }
}

// -----------------------------------------------------------------------------

void Overlay::DeleteCellView()
{
    if (cellview) {
        free(cellview);
        cellview = NULL;
    }
    if (cellview1) {
        free(cellview1);
        cellview1 = NULL;
    }
    if (zoomview) {
        free(zoomview);
        zoomview = NULL;
    }
    DeleteStars();
}

// -----------------------------------------------------------------------------

void Overlay::SetRGBA(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha, unsigned int *rgba)
{
    unsigned char *rgbaptr = (unsigned char *)rgba;
    *rgbaptr++ = red;
    *rgbaptr++ = green;
    *rgbaptr++ = blue;
    *rgbaptr++ = alpha;
}

// -----------------------------------------------------------------------------

void Overlay::GetRGBA(unsigned char *red, unsigned char *green, unsigned char *blue, unsigned char *alpha, unsigned int rgba)
{
    unsigned char *rgbaptr = (unsigned char *)&rgba;
    *red   = *rgbaptr++;
    *green = *rgbaptr++;
    *blue  = *rgbaptr++;
    *alpha = *rgbaptr++;
}

// -----------------------------------------------------------------------------

void Overlay::RefreshCellViewWithTheme()
{
    // refresh the cellview for a 2 state pattern using LifeViewer theme
    unsigned char *cellviewptr = cellview;
    unsigned char state;
    unsigned char* cellviewptr1 = cellview1;
    unsigned char *end = cellview + (cellwd * cellht);

    // get the cells in the cell view
    lifealgo *algo = currlayer->algo;
    algo->getcells(cellviewptr1, cellx, celly, cellwd, cellht);

    // update based on the theme
    while (cellviewptr < end) {
        state = *cellviewptr;
        if (*cellviewptr1++) {
            // new cell is alive
            if (state >= aliveStart) {
                // cell was already alive
                if (state < aliveEnd) *cellviewptr = state + 1;
            } else {
                // cell just born
                *cellviewptr = aliveStart;
            }
        } else {
            // new cell is dead
            if (state >= aliveStart) {
                // cell just died
                *cellviewptr = deadStart;
            } else {
                // cell is decaying
                if (state > deadEnd) *cellviewptr = state - 1;
            }
        }
        cellviewptr++;
    }
}

// -----------------------------------------------------------------------------

void Overlay::RefreshCellView()
{
    lifealgo *algo = currlayer->algo;

    // read cells into the buffer
    algo->getcells(cellview, cellx, celly, cellwd, cellht);
}

// -----------------------------------------------------------------------------

void Overlay::GetPatternColors()
{
    unsigned char *rgb = (unsigned char *)cellRGBA;

    // read pattern colors
    for (int i = 0; i <= currlayer->numicons; i++) {
        *rgb++ = currlayer->cellr[i];
        *rgb++ = currlayer->cellg[i];
        *rgb++ = currlayer->cellb[i];
        *rgb++ = 255; // opaque
    }

    // read border color from View Settings
    unsigned char borderr = borderrgb->Red();
    unsigned char borderg = borderrgb->Green();
    unsigned char borderb = borderrgb->Blue();
    unsigned char alpha = 255; // opaque
    SetRGBA(borderr, borderg, borderb, alpha, &borderRGBA);
}

// -----------------------------------------------------------------------------

void Overlay::GetThemeColors(double brightness)
{
    unsigned char *rgb = (unsigned char *)cellRGBA;
    double weight;

    // cell born color
    unsigned char aliveStartR, aliveStartG, aliveStartB, aliveStartA;

    // cell alive long time color
    unsigned char aliveEndR, aliveEndG, aliveEndB;

    // cell just died color
    unsigned char deadStartR, deadStartG, deadStartB, deadStartA;

    // cell dead long time color
    unsigned char deadEndR, deadEndG, deadEndB;

    // cell never occupied color
    unsigned char unoccupiedR, unoccupiedG, unoccupiedB, unoccupiedA;

    // get the color rgb components
    GetRGBA(&aliveStartR, &aliveStartG, &aliveStartB, &aliveStartA, aliveStartRGBA);
    GetRGBA(&aliveEndR, &aliveEndG, &aliveEndB, &aliveStartA, aliveEndRGBA);
    GetRGBA(&deadStartR, &deadStartG, &deadStartB, &deadStartA, deadStartRGBA);
    GetRGBA(&deadEndR, &deadEndG, &deadEndB, &deadStartA, deadEndRGBA);
    GetRGBA(&unoccupiedR, &unoccupiedG, &unoccupiedB, &unoccupiedA, unoccupiedRGBA);

    // set never occupied cell color
    *rgb++ = unoccupiedR;
    *rgb++ = unoccupiedG;
    *rgb++ = unoccupiedB;
    *rgb++ = unoccupiedA;

    // set decaying colors
    for (int i = deadEnd; i <= deadStart; i++) {
        weight = 1 - ((double)(i - deadEnd) / (deadStart - deadEnd));
        *rgb++ = deadStartR * (1 - weight) + deadEndR * weight;
        *rgb++ = deadStartG * (1 - weight) + deadEndG * weight;
        *rgb++ = deadStartB * (1 - weight) + deadEndB * weight;
        *rgb++ = deadStartA;
    }

    // set living colors
    for (int i = aliveStart; i <= aliveEnd; i++) {
        weight = 1 - ((double)(i - aliveStart) / (aliveEnd - aliveStart));
        *rgb++ = (aliveStartR * weight + aliveEndR * (1 - weight)) * brightness;
        *rgb++ = (aliveStartG * weight + aliveEndG * (1 - weight)) * brightness;
        *rgb++ = (aliveStartB * weight + aliveEndB * (1 - weight)) * brightness;
        *rgb++ = aliveStartA;
    }

    // read border color from View Settings
    unsigned char borderr = borderrgb->Red();
    unsigned char borderg = borderrgb->Green();
    unsigned char borderb = borderrgb->Blue();
    SetRGBA(borderr, borderg, borderb, bordera, &borderRGBA);
}

// -----------------------------------------------------------------------------

void Overlay::UpdateZoomView(unsigned char* source, unsigned char *dest, int step)
{
    int h, w;
    unsigned char state;
    unsigned char max;
    int halfstep = step >> 1;
    int ystep = step * cellwd;
    unsigned char* row1 = source;
    unsigned char* row2 = source + halfstep * cellwd;

    for (h = 0; h < cellht; h += step) {
        for (w = 0; w < cellwd; w += step) {
            // find the maximum state value in each 2x2 block
            max = row1[w];
            state = row1[w + halfstep];
            if (state > max) max = state;
            state = row2[w];
            if (state > max) max = state;
            state = row2[w + halfstep];
            if (state > max) max = state;
            dest[w] = max;
        }

        // update row pointers
        row1 += ystep;
        row2 += ystep;
        dest += ystep;
    }
}

// -----------------------------------------------------------------------------

const char* Overlay::DoDrawCells()
{
    if (cellview == NULL) return OverlayError(no_cellview);

    int mask = 0;
    unsigned char *cells = cellview;
    unsigned char *source = cellview;
    unsigned char *dest = zoomview;

    // check for zoom < 1
    if (camzoom < 1) {
        int negzoom = (1 / camzoom) - 0.001;
        int step = 2;
        do {
            UpdateZoomView(source, dest, step);

            // next zoom level
            step <<= 1;
            negzoom >>= 1;
            mask = (mask << 1) | 1;

            // update source and destination
            source = dest;
            dest = zoomview + (step >> 1) - 1;
        } while (negzoom >= 1);

        // source is the zoom view
        cells = source;
    }

    // check for hex
    double angle = camangle;
    if (ishex) {
        angle = 0;
    }

    // pick renderer based on whether camera is rotated
    if (angle == 0) {
        DrawCellsNoRotate(cells, ~mask);
    }
    else {
        DrawCellsRotate(cells, ~mask, angle);
    }

    // draw stars if enabled
    if (stars) {
        DrawStars(angle);
    }

    // mark target clip as changed
    DisableTargetClipIndex();

    return NULL;
}

// -----------------------------------------------------------------------------

void Overlay::DrawCellsRotate(unsigned char *cells, int mask, double angle)
{
    // convert depth to actual depth
    double depth = camlayerdepth / 2 + 1;

    // check pixel brightness depending on layers
    double brightness = 1;
    double brightinc = 0;

    // check whether to draw layers
    int layertarget = 0;
    if (theme && camlayers > 1 && depth > 1) {
        brightness = 0.6;
        brightinc = 0.4 / (camlayers - 1);
        layertarget = camlayers;
    }

    // refresh the cell view
    if (theme) {
        // using theme colors
        GetThemeColors(brightness);
    }
    else {
        // using standard pattern colors
        GetPatternColors();
    }

    // compute deltas in horizontal and vertical direction based on rotation
    double dxy = sin(angle / 180 * M_PI) / camzoom;
    double dyy = cos(angle / 180 * M_PI) / camzoom;

    double sy = -((wd / 2) * (-dxy) + (ht / 2) * dyy) + camy;
    double sx = -((wd / 2) * dyy + (ht / 2) * dxy) + camx;

    unsigned char state;
    unsigned int rgba;
    unsigned int *overlayptr = (unsigned int *)pixmap;
    double x, y;
    int ix, iy;
    int h, w;

    // draw each pixel
    y = sy;
    for (h = 0; h < ht; h++) {
        x = sx;

        // offset if hex rule
        if (ishex) {
            x += 0.5 * (int)y;
        }

        for (w = 0; w < wd; w++) {
            ix = (((int)x) & mask);
            iy = (((int)y) & mask);

            // check if pixel is in the cell view
            if (ix >= 0 && ix < cellwd && iy >= 0 && iy < cellht) {
                state = cells[cellwd * iy + ix];
                rgba = cellRGBA[state];
            }
            else {
                rgba = borderRGBA;
            }

            // set the pixel
            *overlayptr++ = rgba;

            // update row position
            x += dyy;
            y -= dxy;
        }

        // update column position
        sx += dxy;
        sy += dyy;
        y = sy;
    }

    // draw grid lines if enabled
    if (grid && angle == 0 && camzoom >= 4) {
        DrawGridLines();
    }

    // draw any layers
    if (theme) {
        double layerzoom = camzoom;
        int zoomlevel;

        for (int i = 1; i < layertarget; i++) {
            unsigned char transparenttarget = (i * ((aliveEnd + 1) / camlayers));

            // update brightness
            brightness += brightinc;
            GetThemeColors(brightness);

            // adjust zoom for next level
            dxy /= depth;
            dyy /= depth;
            layerzoom *= depth;
            cells = cellview;
            zoomlevel = 0;
            mask = ~0;

            // compute which zoomview level to use for this layer
            if (layerzoom < 0.125) {
                zoomlevel = 8;
            }
            else {
                if (layerzoom < 0.25) {
                    zoomlevel = 4;
                }
                else {
                    if (layerzoom < 0.5) {
                        zoomlevel = 2;
                    }
                    else {
                        if (layerzoom < 1) {
                            zoomlevel = 1;
                        }
                    }
                }
            }

            // setup the mask for the zoom level
            if (zoomlevel > 0) {
                mask = ~((zoomlevel << 1) - 1);
                cells = zoomview + zoomlevel - 1;
            }

            sy = -((wd / 2) * (-dxy) + (ht / 2) * dyy) + camy;
            sx = -((wd / 2) * dyy + (ht / 2) * dxy) + camx;

            overlayptr = (unsigned int *)pixmap;

            // draw each pixel
            y = sy;
            for (h = 0; h < ht; h++) {
                x = sx;

                // offset if hex rule
                if (ishex) {
                    x += 0.5 * (int)y;
                }

                for (w = 0; w < wd; w++) {
                    ix = (((int)x) & mask);
                    iy = (((int)y) & mask);

                    // check if pixel is on the grid
                    if (ix >= 0 && ix < cellwd && iy >= 0 && iy < cellht) {
                        state = cells[cellwd * iy + ix];

                        // check if it is transparent
                        if (state >= transparenttarget) {
                            // draw the pixel
                            *overlayptr = cellRGBA[state];
                        }
                    }
                    overlayptr++;

                    // update row position
                    x += dyy;
                    y -= dxy;
                }

                // update column position
                sx += dxy;
                sy += dyy;
                y = sy;
            }
        }
    }
}

// -----------------------------------------------------------------------------

void Overlay::DrawCellsNoRotate(unsigned char *cells, int mask)
{
    // convert depth to actual depth
    double depth = camlayerdepth / 2 + 1;

    // check pixel brightness depending on layers
    double brightness = 1;
    double brightinc = 0;

    // check whether to draw layers
    int layertarget = 0;
    if (theme && camlayers > 1 && depth > 1) {
        brightness = 0.6;
        brightinc = 0.4 / (camlayers - 1);
        layertarget = camlayers;
    }

    // refresh the cell view
    if (theme) {
        // using theme colors
        GetThemeColors(brightness);
    }
    else {
        // using standard pattern colors
        GetPatternColors();
    }

    // compute deltas in horizontal and vertical direction based on rotation
    double dyy = 1 / camzoom;

    double sy = -((ht / 2) * dyy) + camy;
    double sx = -((wd / 2) * dyy) + camx;

    unsigned char state;
    unsigned int rgba;
    unsigned int *overlayptr = (unsigned int *)pixmap;
    unsigned char *rowptr;
    double x, y;
    int ix, iy;
    int h, w;
    int sectionsize = 4;   // size of unrolled loop
    int endrow = wd & ~(sectionsize - 1);

    // draw each pixel
    y = sy;
    for (h = 0; h < ht; h++) {
        iy = (((int)y) & mask);

        // clip to the grid
        if (iy >= 0 && iy < cellht) {
            // get the row
            rowptr = cells + cellwd * iy;
            x = sx;

            // offset if hex rule
            if (ishex) {
                x += 0.5 * (int)y;
            }

            w = 0;
            while (w < endrow) {
                // check if pixel is in the cell view
                ix = (((int)x) & mask);
                if (ix >= 0 && ix < cellwd) {
                    state = rowptr[ix];
                    rgba = cellRGBA[state];
                }
                else {
                    rgba = borderRGBA;
                }

                // set the pixel
                *overlayptr++ = rgba;

                // update row position
                x += dyy;

                // loop unroll
                ix = (((int)x) & mask);
                if (ix >= 0 && ix < cellwd) {
                    state = rowptr[ix];
                    rgba = cellRGBA[state];
                }
                else {
                    rgba = borderRGBA;
                }
                *overlayptr++ = rgba;
                x += dyy;

                // loop unroll
                ix = (((int)x) & mask);
                if (ix >= 0 && ix < cellwd) {
                    state = rowptr[ix];
                    rgba = cellRGBA[state];
                }
                else {
                    rgba = borderRGBA;
                }
                *overlayptr++ = rgba;
                x += dyy;

                // loop unroll
                ix = (((int)x) & mask);
                if (ix >= 0 && ix < cellwd) {
                    state = rowptr[ix];
                    rgba = cellRGBA[state];
                }
                else {
                    rgba = borderRGBA;
                }
                *overlayptr++ = rgba;
                x += dyy;

                // next section
                w += sectionsize;
            }

            // remaining pixels
            while (w < wd) {
                // render pixel
                ix = (((int)x) & mask);
                if (ix >= 0 && ix < cellwd) {
                    state = rowptr[ix];
                    rgba = cellRGBA[state];
                }
                else {
                    rgba = borderRGBA;
                }
                *overlayptr++ = rgba;
                x += dyy;

                // next pixel
                w++;
            }
        }
        else {
            // draw off grid row
            w = 0;
            while (w < endrow) {
                // draw section
                *overlayptr++ = borderRGBA;
                *overlayptr++ = borderRGBA;
                *overlayptr++ = borderRGBA;
                *overlayptr++ = borderRGBA;

                // next section
                w += sectionsize;
            }

            // remaining pixels
            while (w < wd) {
                // draw pixel
                *overlayptr++ = borderRGBA;

                // next pixel
                w++;
            }
        }

        // update column position
        sy += dyy;
        y = sy;
    }

    // draw grid lines if enabled
    if (grid && camzoom >= 4) {
        DrawGridLines();
    }

    // draw any layers
    if (theme) {
        double layerzoom = camzoom;
        int zoomlevel;

        for (int i = 1; i < layertarget; i++) {
            unsigned char transparenttarget = (i * ((aliveEnd + 1) / camlayers));

            // update brightness
            brightness += brightinc;
            GetThemeColors(brightness);

            // adjust zoom for next level
            dyy /= depth;
            layerzoom *= depth;
            cells = cellview;
            zoomlevel = 0;
            mask = ~0;

            // compute which zoomview level to use for this layer
            if (layerzoom < 0.125) {
                zoomlevel = 8;
            }
            else {
                if (layerzoom < 0.25) {
                    zoomlevel = 4;
                }
                else {
                    if (layerzoom < 0.5) {
                        zoomlevel = 2;
                    }
                    else {
                        if (layerzoom < 1) {
                            zoomlevel = 1;
                        }
                    }
                }
            }

            // setup the mask for the zoom level
            if (zoomlevel > 0) {
                mask = ~((zoomlevel << 1) - 1);
                cells = zoomview + zoomlevel - 1;
            }

            sy = -((ht / 2) * dyy) + camy;
            sx = -((wd / 2) * dyy) + camx;

            overlayptr = (unsigned int *)pixmap;

            // draw each pixel
            y = sy;
            for (h = 0; h < ht; h++) {
                iy = (((int)y) & mask);

                // clip to the grid
                if (iy >= 0 && iy < cellht) {
                    // get the row
                    rowptr = cells + cellwd * iy;
                    x = sx;

                    // offset if hex rule
                    if (ishex) {
                        x += 0.5 * (int)y;
                    }

                    w = 0;
                    while (w < endrow) {
                        // check if pixel is on the grid
                        ix = (((int)x) & mask);
                        if (ix >= 0 && ix < cellwd) {
                            state = rowptr[ix];

                            // check if it is transparent
                            if (state >= transparenttarget) {
                                // draw the pixel
                                *overlayptr = cellRGBA[state];
                            }
                        }
                        overlayptr++;

                        // update row position
                        x += dyy;

                        // loop unroll
                        ix = (((int)x) & mask);
                        if (ix >= 0 && ix < cellwd) {
                            state = rowptr[ix];
                            if (state >= transparenttarget) {
                                *overlayptr = cellRGBA[state];
                            }
                        }
                        overlayptr++;
                        x += dyy;

                        // loop unroll
                        ix = (((int)x) & mask);
                        if (ix >= 0 && ix < cellwd) {
                            state = rowptr[ix];
                            if (state >= transparenttarget) {
                                *overlayptr = cellRGBA[state];
                            }
                        }
                        overlayptr++;
                        x += dyy;

                        // loop unroll
                        ix = (((int)x) & mask);
                        if (ix >= 0 && ix < cellwd) {
                            state = rowptr[ix];
                            if (state >= transparenttarget) {
                                *overlayptr = cellRGBA[state];
                            }
                        }
                        overlayptr++;
                        x += dyy;

                        // next section
                        w += sectionsize;
                    }

                    // remaining pixels
                    while (w < wd) {
                        // render pixel
                        ix = (((int)x) & mask);
                        if (ix >= 0 && ix < cellwd) {
                            state = rowptr[ix];
                            if (state >= transparenttarget) {
                                *overlayptr = cellRGBA[state];
                            }
                        }
                        overlayptr++;
                        x += dyy;

                        // next pixel
                        w++;
                   }
                }
                else {
                    // draw off grid row
                    w = 0;
                    while (w < endrow) {
                        // draw section
                        *overlayptr++ = borderRGBA;
                        *overlayptr++ = borderRGBA;
                        *overlayptr++ = borderRGBA;
                        *overlayptr++ = borderRGBA;

                        // next section
                        w += sectionsize;
                    }

                    // remaining pixels
                    while (w < wd) {
                        // draw pixel
                        *overlayptr++ = borderRGBA;

                        // next pixel
                        w++;
                    }
                }

                // update column position
                sy += dyy;
                y = sy;
            }
        }
    }
}

// -----------------------------------------------------------------------------

const char* Overlay::DoUpdateCells()
{
    if (cellview == NULL) return OverlayError(no_cellview);

    // check if themes are used
    if (theme) {
        RefreshCellViewWithTheme();
    }
    else {
        RefreshCellView();
    }

    return NULL;
}

// -----------------------------------------------------------------------------

void Overlay::DrawVLine(int x, int y1, int y2, unsigned int color)
{
    // check the line is on the display
    if (x < 0 || x >= wd) {
        return;
    }

    // clip the line to the display
    if (y1 < 0) {
        y1 = 0;
    }
    else {
        if (y1 >= ht) {
            y1 = ht - 1;
        }
    }
    if (y2 < 0) {
        y2 = 0;
    }
    else {
        if (y2 >= ht) {
            y2 = ht - 1;
        }
    }

    int offset = y1 * wd + x;
    unsigned int *pix = (unsigned int*)pixmap;
    pix += offset;

    y2 -= y1;
    y1 = 0;

    int sectionsize = 4;   // size of unrolled loop
    int endcol = y2 & ~(sectionsize - 1);

    while (y1 < endcol) {
        // draw section
        *pix = color;
        pix += wd;
        *pix = color;
        pix += wd;
        *pix = color;
        pix += wd;
        *pix = color;
        pix += wd;
        y1 += sectionsize;
    }

    // draw remaining pixels
    while (y1 < y2) {
        *pix = color;
        pix += wd;
        y1++;
    }
}

// -----------------------------------------------------------------------------

void Overlay::DrawHLine(int x1, int x2, int y, unsigned int color)
{
    // check the line is on the display
    if (y < 0 || y >= ht) {
        return;
    }

    int offset = y * wd + x1;
    unsigned int *pix = (unsigned int*)pixmap;
    pix += offset;

    x2 -= x1;
    x1 = 0;

    int sectionsize = 4;   // size of unrolled loop
    int endrow = x2 & ~(sectionsize - 1);

    // check the line is on the display
    while (x1 < endrow) {
        // draw section
        *pix++ = color;
        *pix++ = color;
        *pix++ = color;
        *pix++ = color;
        x1 += sectionsize;
    }

    // remaining pixels
    while (x1 < x2) {
        *pix++ = color;
        x1++;
    }
}

// -----------------------------------------------------------------------------

void Overlay::DrawGridLines()
{
    double x, y;
    unsigned char shade;
    bool light = false;

    // check if background is light or dark
    unsigned char red, green, blue, alpha;
    GetRGBA(&red, &green, &blue, &alpha, cellRGBA[0]);
    if ((red + green + blue) / 3 >= 128) {
        light = true;
    }

    // check if custom grid line color is defined
    if (!customgridcolor) {
        // no custom grid color defined to base it on background color
        shade = light ? 229 : 80;
        SetRGBA(shade, shade, shade, 255, &gridRGBA);
    }

    // check if custom major grid line color is defined
    if (!customgridmajorcolor) {
        // no custom grid color defined to base it on background color
        shade = light ? 209 : 112;
        SetRGBA(shade, shade, shade, 255, &gridmajorRGBA);
    }

    // compute single cell offset
    double xoff = remainder(((cellwd / 2 - camx + 0.5) * camzoom) + (wd / 2), camzoom);
    double yoff = remainder(((cellht / 2 - camy + 0.5) * camzoom) + (ht / 2), camzoom);

    // draw twice if major grid lines enabled
    int loop = 1;
    if (gridmajor > 0) {
        loop = 2;
    }

    // start drawing the grid lines
    unsigned int targetRGBA = gridRGBA;
    unsigned int drawRGBA = gridRGBA;
    int gridlineNum;
    int vlineNum;

    while (loop) {
        // compute grid line vertical offset
        gridlineNum = floor(-(wd / 2 / camzoom) - (cellwd / 2 - camx));

        // draw vertical lines
        for (x = 0; x <= wd * camzoom; x += camzoom) {
            if (gridmajor > 0) {
                // choose whether to use major or minor color
                if (gridlineNum % gridmajor == 0) {
                    drawRGBA = gridmajorRGBA;
                }
                else {
                    drawRGBA = gridRGBA;
                }
            }
            gridlineNum++;

            // check whether to draw the line
            if (drawRGBA == targetRGBA) {
                // check for hex display
                if (ishex) {
                    vlineNum = (int)(-(ht / 2 / camzoom) - (cellht / 2 - camy));

                    // draw staggered vertical line
                    for (y = yoff - camzoom; y <= ht + camzoom; y += camzoom) {
                        if ((vlineNum & 1) != 0) {
                            DrawVLine(round(x + xoff + camzoom / 2), round(y + camzoom / 2), round(y + camzoom / 2 +  camzoom - 1), drawRGBA);
                        }
                        else {
                            DrawVLine(round(x + xoff + camzoom), round(y + camzoom / 2), round(y + camzoom / 2 + camzoom - 1), drawRGBA);
                        }
                        vlineNum++;
                    }
                }
                else {
                    DrawVLine(round(x + xoff + camzoom / 2), 0, ht - 1, drawRGBA);
                }
            }
        }

        // compute grid line horizontal offset
        gridlineNum = (int)(-(ht / 2 / camzoom) - (cellht / 2 - camy));

        // draw horizontal lines
        for (y = 0; y <= ht + camzoom; y += camzoom) {
            if (gridmajor > 0) {
                // choose whether to use major or minor color
                if (gridlineNum % gridmajor == 0) {
                    drawRGBA = gridmajorRGBA;
                }
                else {
                    drawRGBA = gridRGBA;
                }
            }
            gridlineNum++;

            // check whether to draw the line
            if (drawRGBA == targetRGBA) {
                DrawHLine(0, wd - 1, round(y + yoff + camzoom / 2), drawRGBA);
            }
        }

        // next iteration so switch to major color
        loop--;
        targetRGBA = gridmajorRGBA;
    }
}

// -----------------------------------------------------------------------------

void Overlay::CreateStars()
{
    int i;
    double curx, cury, curz;

    // allocate the stars
    if (starx == NULL) {
        starx = (double *)malloc(numStars * sizeof(*starx));
    }
    if (stary == NULL) {
        stary = (double *)malloc(numStars * sizeof(*stary));
    }
    if (starz == NULL) {
        starz = (double *)malloc(numStars * sizeof(*starz));
    }

    // compute radius^2 of the starfield
    int radius2 = (starMaxX * starMaxX) + (starMaxY * starMaxY);
    double id;

    // create random stars using fixed seed
    srand(52315);

    for (i = 0; i < numStars; i++) {
        // get the next z coordinate based on the star number
        // (more stars nearer the camera)
        id = (double)i;
        curz = ((id / numStars) * (id / numStars) * (id / numStars) * (id / numStars) * starMaxZ) + 1;

        // pick a random 2d position and ensure it is within the radius
        do {
            curx = 3 * ((((double)rand()) / RAND_MAX * starMaxX) - (starMaxX / 2));
            cury = 3 * ((((double)rand()) / RAND_MAX * starMaxY) - (starMaxY / 2));
        } while (((curx * curx) + (cury * cury)) > radius2);

        // save the star position
        starx[i] = curx;
        stary[i] = cury;
        starz[i] = curz;
    }

    // create random seed
    srand(time(0));
}

// -----------------------------------------------------------------------------

void Overlay::DrawStars(double angle)
{
    int i;
    double x, y, z;
    int ix, iy;
    double radius;
    double theta;
    int offset;
    unsigned int* pixmapRGBA = (unsigned int*)pixmap;

    // get the unoccupied cell pixel color
    unsigned int blankRGBA = cellRGBA[0];
    unsigned char* blankCol = (unsigned char*)&blankRGBA;
    unsigned char blankR = *blankCol++;
    unsigned char blankG = *blankCol++;
    unsigned char blankB = *blankCol++;

    // get the star color components
    unsigned char* starCol = (unsigned char*)&starRGBA;
    unsigned char starR = *starCol++;
    unsigned char starG = *starCol++;
    unsigned char starB = *starCol++;

    unsigned int pixelRGBA;
    unsigned char* pixelCol = (unsigned char*)&pixelRGBA;
    pixelCol[3] = 255;
    unsigned char red, green, blue;

    // check if stars have been allocated
    if (starx == NULL) {
        CreateStars();
    }

    // update each star
    for (i = 0; i < numStars; i++) {
        // get the 2d part of 3d position
        x = starx[i] - camx;
        y = stary[i] - camy;

        // check if angle is non zero
        if (angle != 0) {
            // compute radius
            radius = sqrt((x * x) + (y * y));

            // get angle
            theta = atan2(y, x) * radToDeg;

            // add current rotation
            theta += angle;
            if (theta < 0) {
                theta += 360;
            }
            else {
                if (theta >= 360) {
                    theta -= 360;
                }
            }

            // compute rotated position
            x = radius * cos(theta * degToRad);
            y = radius * sin(theta * degToRad);
        }

        // create the 2d position
        z = (starz[i] / camzoom) * 2;
        ix = (int)(x / z) + wd / 2;
        iy = (int)(y / z) + ht / 2;

        // check the star and halo are on the display
        if (ix > 0 && ix < (wd - 1) && iy > 0 && iy < (ht - 1)) {
            // compute the star brightness
            z = 1536 / z;
            if (z > 255) {
                z = 255;
            }
            z = z / 255;

            // check if pixel is blank
            offset = ix + iy * wd;
            if (pixmapRGBA[offset] == blankRGBA) {
                // compute the color components
                red = blankR + (starR - blankR) * z;
                green = blankG + (starG - blankG) * z;
                blue = blankB + (starB - blankB) * z;
                pixelCol[0] = red;
                pixelCol[1] = green;
                pixelCol[2] = blue;
                pixmapRGBA[offset] = pixelRGBA;
            }

            // use a dimmer color for the halo
            red = blankR + (starR - blankR) * z;
            green = blankG + (starG - blankG) * z;
            blue = blankB + (starB - blankB) * z;
            pixelCol[0] = red;
            pixelCol[1] = green;
            pixelCol[2] = blue;

            // left halo
            offset -= 1;
            if (pixmapRGBA[offset] == blankRGBA) {
                pixmapRGBA[offset] = pixelRGBA;
            }

            // right halo
            offset += 2;
            if (pixmapRGBA[offset] == blankRGBA) {
                pixmapRGBA[offset] = pixelRGBA;
            }

            // top halo
            offset -= (1 + wd);
            if (pixmapRGBA[offset] == blankRGBA) {
                pixmapRGBA[offset] = pixelRGBA;
            }

            // bottom halo
            offset += (wd + wd);
            if (pixmapRGBA[offset] == blankRGBA) {
                pixmapRGBA[offset] = pixelRGBA;
            }
        }
    }
}

// -----------------------------------------------------------------------------

const char* Overlay::DoCellView(const char* args)
{
    // check the arguments are valid
    int x, y, w, h;
    if (sscanf(args, " %d %d %d %d", &x, &y, &w, &h) != 4) {
        return OverlayError("cellview command requires 4 arguments");
    }

    if (w < cellviewmultiple) return OverlayError("width of cellview must be >= 16");
    if (h < cellviewmultiple) return OverlayError("height of cellview must be >= 16");

    if (w > cellviewmaxsize) return OverlayError("width of cellview too big");
    if (h > cellviewmaxsize) return OverlayError("height of cellview too big");

    if ((w & (cellviewmultiple - 1)) != 0) return OverlayError("width of cellview must be a multiple of 16");
    if ((h & (cellviewmultiple - 1)) != 0) return OverlayError("height of cellview  must be a multiple of 16");

    // delete any existing cellview
    DeleteCellView();

    // use calloc so all cells will be in state 0
    cellview = (unsigned char*) calloc(w * h, sizeof(*cellview));
    if (cellview == NULL) return OverlayError("not enough memory to create cellview");
    cellview1 = (unsigned char*) calloc(w * h, sizeof(*cellview1));
    if (cellview1 == NULL) return OverlayError("not enough memory to create cellview");

    // allocate the zoom view
    zoomview = (unsigned char*) calloc(w * h, sizeof(*zoomview));
    if (zoomview == NULL) return OverlayError("not enough memory to create cellview");

    // save the arguments
    cellwd = w;
    cellht = h;
    cellx = x;
    celly = y;

    // set the default camera position to the center
    camx = w / 2;
    camy = h / 2;

    // set default angle
    camangle = 0;

    // set default zoom
    camzoom = 1;

    // set default layers
    camlayers = 1;
    camlayerdepth = 0.05;

    // initialize ishex to false and let scripts use the hexrule() function
    // from the oplus package to determine if the current rule uses a
    // hexagonal neighborhood
    ishex = false;

    // use standard pattern colors
    theme = false;

    // disable grid and set default grid major interval
    grid = false;
    gridmajor = 10;
    customgridcolor = false;
    customgridmajorcolor = false;

    // disable stars and set star color to opaque whie
    stars = false;
    SetRGBA(255, 255, 255, 255, &starRGBA);

    // populate cellview
    DoUpdateCells();

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::CamZoom(const char* args)
{
    // check the argument is valid
    double zoom;
    if (sscanf(args, " %lf", &zoom) != 1) {
        return OverlayError("camera zoom command requires 1 argument");
    }

    if (zoom < camminzoom) return OverlayError("camera zoom too small");
    if (zoom > cammaxzoom) return OverlayError("camera zoom too big");

    // save the new value
    camzoom = zoom;

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::CamAngle(const char* args)
{
    // check the argument is valid
    double angle;
    if (sscanf(args, " %lf", &angle) != 1) {
        return OverlayError("camera angle command requires 1 argument");
    }

    if (angle < 0) return OverlayError("camera angle too small");
    if (angle > 360) return OverlayError("camera angle too big");

    // save the new value
    camangle = angle;

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::CamXY(const char* args)
{
    // check the arguments are valid
    double x;
    double y;
    if (sscanf(args, " %lf %lf", &x, &y) != 2) {
        return OverlayError("camera xy command requires 2 arguments");
    }

    // save the new values
    camx = x;
    camy = y;

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoCamera(const char* args)
{
    if (cellview == NULL) return OverlayError(no_cellview);

    if (strncmp(args, "xy ", 3) == 0)    return CamXY(args+3);
    if (strncmp(args, "angle ", 6) == 0) return CamAngle(args+6);
    if (strncmp(args, "zoom ", 5) == 0)  return CamZoom(args+5);

    return OverlayError("unknown camera command");
}

// -----------------------------------------------------------------------------

const char* Overlay::CellOptionLayers(const char* args)
{
    // check the argument is valid
    int howmany;

    if (sscanf(args, " %d", &howmany) != 1) {
        return OverlayError("celloption layers command requires 1 argument");
    }

    if (howmany < 1) return OverlayError("celloption layers must be at least 1");
    if (howmany > 10) return OverlayError("celloption layers is too big");

    // save the new values
    camlayers = howmany;

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::CellOptionDepth(const char* args)
{
    // check the argument is valid
    double depth;
    if (sscanf(args, " %lf", &depth) != 1) {
        return OverlayError("celloption depth command requires 1 argument");
    }

    if (depth < 0 || depth > 1) return OverlayError("celloption depth is out of range");

    // save the new values
    camlayerdepth = depth;

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::CellOptionHex(const char* args)
{
    int mode;
    if (sscanf(args, "%d", &mode) != 1) {
        return OverlayError("celloption hex command requires 1 argument");
    }

    ishex = mode == 1;

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::CellOptionGrid(const char* args)
{
    int mode;
    if (sscanf(args, "%d", &mode) != 1) {
        return OverlayError("celloption grid command requires 1 argument");
    }

    grid = mode == 1;

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::CellOptionGridMajor(const char* args)
{
    int major;
    if (sscanf(args, "%d", &major) != 1) {
        return OverlayError("celloption grid command requires 1 argument");
    }

    if (major < 0 || major > 16) return OverlayError("celloption major is out of range");

    gridmajor = major;

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::CellOptionStars(const char* args)
{
    int mode;
    if (sscanf(args, "%d", &mode) != 1) {
        return OverlayError("celloption stars command requires 1 argument");
    }

    stars = mode == 1;

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoCellOption(const char* args)
{
    if (cellview == NULL) return OverlayError(no_cellview);

    if (strncmp(args, "hex ", 3) == 0)        return CellOptionHex(args+3);
    if (strncmp(args, "depth ", 6) == 0)      return CellOptionDepth(args+6);
    if (strncmp(args, "layers ", 7) == 0)     return CellOptionLayers(args+7);
    if (strncmp(args, "grid ", 5) == 0)       return CellOptionGrid(args+5);
    if (strncmp(args, "gridmajor ", 10) == 0) return CellOptionGridMajor(args+10);
    if (strncmp(args, "stars ", 6) == 0)      return CellOptionStars(args+6);

    return OverlayError("unknown celloption command");
}

// -----------------------------------------------------------------------------

const char* Overlay::DoTheme(const char* args)
{
    if (cellview == NULL) return OverlayError(no_cellview);

    // check the arguments are valid
    int asr, asg, asb, aer, aeg, aeb, dsr, dsg, dsb, der, deg, deb, ur, ug, ub;

    // default alpha values to opaque
    int aa = 255;
    int da = 255;
    int ua = 255;
    int ba = 255;

    // whether theme is disabled
    int disable = 0;

    // argument count
    int count = 0;

    // check for 19 argument version
    count = sscanf(args, " %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d", 
        &asr, &asg, &asb, &aer, &aeg, &aeb, &dsr, &dsg, &dsb, &der, &deg, &deb, &ur, &ug, &ub, &aa, &da, &ua, &ba);
    if (count != 19) {
        // check for 15 argument version
        if (count != 15) {        
            // check for single argument version
            if (count == 1) {
                disable = asr;
                if (disable != -1) {
                    return OverlayError("theme command single argument must be -1");
                }
            }
            else {
                return OverlayError("theme command requires single argument -1, or 15 or 19 rgb components");
            }
        }
    }

    if (disable != -1) {
        if (asr < 0 || asr > 255 ||
            asg < 0 || asg > 255 ||
            asb < 0 || asb > 255 ) {
            return OverlayError("theme alivestart values must be from 0 to 255");
        }
        if (aer < 0 || aer > 255 ||
            aeg < 0 || aeg > 255 ||
            aeb < 0 || aeb > 255 ) {
            return OverlayError("theme aliveend values must be from 0 to 255");
        }
        if (dsr < 0 || dsr > 255 ||
            dsg < 0 || dsg > 255 ||
            dsb < 0 || dsb > 255 ) {
            return OverlayError("theme deadstart values must be from 0 to 255");
        }
        if (der < 0 || der > 255 ||
            deg < 0 || deg > 255 ||
            deb < 0 || deb > 255 ) {
            return OverlayError("theme deadend values must be from 0 to 255");
        }
        if (ur < 0 || ur > 255 ||
            ug < 0 || ug > 255 ||
            ub < 0 || ub > 255 ) {
            return OverlayError("theme unnocupied values must be from 0 to 255");
        }
        if (aa < 0 || aa > 255) {
            return OverlayError("theme alive alpha must be from 0 to 255");
        }
        if (da < 0 || da > 255) {
            return OverlayError("theme dead alpha must be from 0 to 255");
        }
        if (ua < 0 || ua > 255) {
            return OverlayError("theme unoccupied alpha must be from 0 to 255");
        }
        if (ba < 0 || ba > 255) {
            return OverlayError("theme border alpha must be from 0 to 255");
        }
    }

    // save the new values
    if (disable == -1) {
        theme = false;
    }
    else {
        theme = true;
        SetRGBA(asr, asg, asb, aa, &aliveStartRGBA);
        SetRGBA(aer, aeg, aeb, aa, &aliveEndRGBA);
        SetRGBA(dsr, dsg, dsb, da, &deadStartRGBA);
        SetRGBA(der, deg, deb, da, &deadEndRGBA);
        SetRGBA(ur, ug, ub, ua, &unoccupiedRGBA);
        bordera = ba;
    }

    return NULL;
}

// -----------------------------------------------------------------------------

void Overlay::SetRenderTarget(unsigned char* pix, int pwd, int pht, Clip* clip)
{
    pixmap = pix;
    wd = pwd;
    ht = pht;
    renderclip = clip;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoResize(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    // don't set wd and ht until we've checked the args are valid
    int w, h, oldw, oldh;
    bool isclip = false;
    int namepos;
    char dummy;
    if (sscanf(args, " %d %d %n%c", &w, &h, &namepos, &dummy) != 3) {
        if (sscanf(args, " %d %d", &w, &h) != 2) {
            return OverlayError("create command requires 2 or 3 arguments");
        }
    } else {
        isclip = true;
    }

    // check whether resizing clip or overlay
    if (isclip) {
        // resize clip
        if (w <= 0) return OverlayError("width of clip must be > 0");
        if (h <= 0) return OverlayError("height of clip must be > 0");

        // get clip name
        std::string name = args + namepos;

        // check if the clip exists
        std::map<std::string,Clip*>::iterator it;
        it = clips.find(name);
        if (it == clips.end()) {
            static std::string msg;
            msg = "unknown resize clip (";
            msg += name;
            msg += ")";
            return OverlayError(msg.c_str());
        }

        // get the clip dimensions
        oldw = it->second->cwd;
        oldh = it->second->cht;

        // delete the clip
        delete it->second;
        clips.erase(it);

        // allocate the resized clip with calloc
        Clip* newclip = new Clip(w, h, true);
        if (newclip == NULL || newclip->cdata == NULL) {
            delete newclip;
            return OverlayError("not enough memory to resize clip");
        }

        // save named clip
        clips[name] = newclip;

        // check if the clip is the render target
        if (targetname == name) {
            SetRenderTarget(newclip->cdata, newclip->cwd, newclip->cht, newclip);
        }   
    } else {
        // resize overlay
        if (w <= 0) return OverlayError("width of overlay must be > 0");
        if (h <= 0) return OverlayError("height of overlay must be > 0");

        // given width and height are ok
        oldw = ovwd;
        oldh = ovht;
        ovwd = w;
        ovht = h;

        // free the previous pixmap
        free(ovpixmap);

        // create the new pixmap
        ovpixmap = (unsigned char*) calloc(ovwd * ovht * 4, sizeof(*ovpixmap));
        if (ovpixmap == NULL) return OverlayError("not enough memory to resize overlay");

        // check if overlay is the render target
        if (targetname == "") {
            SetRenderTarget(ovpixmap, ovwd, ovht, NULL);
        }
    }

    // return old dimensions
    static char result[32];
    sprintf(result, "%d %d", oldw, oldh);
    return result;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoCreate(const char* args)
{
    // don't set wd and ht until we've checked the args are valid
    int w, h;
    bool isclip = false;
    int namepos;
    char dummy;
    if (sscanf(args, " %d %d %n%c", &w, &h, &namepos, &dummy) != 3) {
        if (sscanf(args, " %d %d", &w, &h) != 2) {
            return OverlayError("create command requires 2 or 3 arguments");
        }
    } else {
        isclip = true;
    }

    // check whether creating clip or overlay
    if (isclip) {
        // create clip
        if (w <= 0) return OverlayError("width of clip must be > 0");
        if (h <= 0) return OverlayError("height of clip must be > 0");

        std::string name = args + namepos;

        // delete any existing clip data with the given name
        std::map<std::string,Clip*>::iterator it;
        it = clips.find(name);
        if (it != clips.end()) {
            delete it->second;
            clips.erase(it);
        }

        // allocate the clip with calloc
        Clip* newclip = new Clip(w, h, true);
        if (newclip == NULL || newclip->cdata == NULL) {
            delete newclip;
            return OverlayError("not enough memory to create clip");
        }

        // create named clip
        clips[name] = newclip;
    } else {
        // creating overlay
        if (w <= 0) return OverlayError("width of overlay must be > 0");
        if (h <= 0) return OverlayError("height of overlay must be > 0");

        // given width and height are ok
        ovwd = w;
        ovht = h;

        // delete any existing pixmap
        DeleteOverlay();

        // use calloc so all pixels will be 100% transparent (alpha = 0)
        ovpixmap = (unsigned char*) calloc(ovwd * ovht * 4, sizeof(*ovpixmap));
        if (ovpixmap == NULL) return OverlayError("not enough memory to create overlay");

        // initialize RGBA values to opaque white
        r = g = b = a = 255;

        // don't do alpha blending initially
        alphablend = false;

        only_draw_overlay = false;

        // initial position of overlay is in top left corner of current layer
        pos = topleft;

        ovcursor = wxSTANDARD_CURSOR;
        cursname = "arrow";

        // identity transform
        axx = 1;
        axy = 0;
        ayx = 0;
        ayy = 1;
        identity = true;

        // initialize current font used by text command
        currfont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
        fontname = "default";
        fontsize = 10;
        #ifdef __WXMAC__
            // need to increase Mac font size by 25% to match text size on Win/Linux
            currfont.SetPointSize(int(fontsize * 1.25 + 0.5));
            extraht = 1;
        #else
            currfont.SetPointSize(fontsize);
        #endif

        // default text alignment
        align = left;

        // default text background
        textbgRGBA = 0;

        // default width for lines and ellipses
        linewidth = 1;

        // make sure the Show Overlay option is ticked
        if (!showoverlay) {
            mainptr->ToggleOverlay();
        } else {
            // enable Save Overlay
            mainptr->UpdateMenuItems();
        }

        // set overlay as render target
        SetRenderTarget(ovpixmap, ovwd, ovht, NULL);
        targetname = "";

        #ifdef ENABLE_SOUND
        // initialise sound
        engine = createIrrKlangDevice(ESOD_AUTO_DETECT, ESEO_MULTI_THREADED | ESEO_LOAD_PLUGINS | ESEO_USE_3D_BUFFERS);
        if (!engine) {
            Warning(_("Unable to initialize sound"));
        }
        #endif
    }

    return NULL;
}

// -----------------------------------------------------------------------------

bool Overlay::PointInOverlay(int vx, int vy, int* ox, int* oy)
{
    if (ovpixmap == NULL) return false;

    int viewwd, viewht;
    viewptr->GetClientSize(&viewwd, &viewht);
    if (viewwd <= 0 || viewht <= 0) return false;

    int x = 0;
    int y = 0;
    switch (pos) {
        case topleft:
            break;
        case topright:
            x = viewwd - ovwd;
            break;
        case bottomright:
            x = viewwd - ovwd;
            y = viewht - ovht;
            break;
        case bottomleft:
            y = viewht - ovht;
            break;
        case middle:
            x = (viewwd - ovwd) / 2;
            y = (viewht - ovht) / 2;
            break;
    }

    if (vx < x) return false;
    if (vy < y) return false;
    if (vx >= x + ovwd) return false;
    if (vy >= y + ovht) return false;

    *ox = vx - x;
    *oy = vy - y;

    return true;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoPosition(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    if (strncmp(args+1, "topleft", 7) == 0) {
        pos = topleft;

    } else if (strncmp(args+1, "topright", 8) == 0) {
        pos = topright;

    } else if (strncmp(args+1, "bottomright", 11) == 0) {
        pos = bottomright;

    } else if (strncmp(args+1, "bottomleft", 10) == 0) {
        pos = bottomleft;

    } else if (strncmp(args+1, "middle", 6) == 0) {
        pos = middle;

    } else {
        return OverlayError("unknown position");
    }

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DecodeReplaceArg(const char* arg, int* find, bool* negfind, int* replace, int* invreplace, int* delta, int component) {
    // argument is a string defining the find component value and optional replacement
    // find part is one of:
    //     *       (match any value)
    //     0..255  (match specific value)
    //     !0..255 (match values other than this value)
    // optional replacement part is one of:
    //     r       (replace with red component)
    //     g       (replace with green component)
    //     b       (replace with blue component)
    //     a       (replace with alpha component)
    //     #       (leave this component unchanged)
    // replacement part can be followed by one of:
    //     -       (component value should be inverted: v -> 255-v)
    //     --      (component value should be decremented)
    //     ++      (component value should be incremented)
    //     -0..255 (component value should have constant subtracted from it)
    //     +0..255 (component value should have constant added to it)
    char *p     = (char*)arg;
    *find       = 0;
    *negfind    = false;
    *replace    = 0;
    *invreplace = 0;

    // decode find
    if (*p == '*') {
        // match any
        *find = matchany;
        p++;
    }
    else {
        if (*p == '!') {
            // invert match
            *negfind = true;
            p++;
        }
        // read value
        while (*p >= '0' && *p <= '9') {
            *find = 10 * (*find) + *p - '0';
            p++;
        }
        if (*find < 0 || *find > 255) {
            return "replace argument is out of range";
        }
    }

    // decode optional replacement
    if (*p != '\0') {
        const char* valid = "rgba#";
        char* match = strchr((char*)valid, *p);
        if (match == NULL) {
            return "replace argument postfix is invalid";
        }
        *replace = match - valid + 1;
        if (*replace == 5) *replace = component;
        p++;
        // check for invert, increment or decrement
        if (*p == '-') {
            p++;
            if (*p == '-') {
                *delta = -1;
                p++;
            } else {
                if (*p >= '0' && *p <= '9') {
                    while (*p >= '0' && *p <= '9') {
                        *delta = 10 * (*delta) + *p - '0';
                        p++;
                    }
                    if (*delta < 0 || *delta > 255) {
                        return "replace delta is out of range";
                    }
                    *delta = -*delta;
                } else {
                    *invreplace = 255;
                }
            }
        } else {
            if (*p == '+') {
                p++;
                if (*p == '+') {
                    *delta = 1;
                    p++;
                } else {
                    if (*p >= '0' && *p <= '9') {
                        while (*p >= '0' && *p <= '9') {
                            *delta = 10 * (*delta) + *p - '0';
                            p++;
                        }
                        if (*delta < 0 || *delta > 255) {
                            return "replace delta is out of range";
                        }
                    } else {
                        p--;
                    }
                }
            }
        }
    }

    // any trailing characters are invalid
    if (*p != '\0') {
        return "replace argument postix is invalid";
    }
    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoReplace(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    // allocate memory for the arguments
    char* buffer = (char*)malloc(strlen(args) + 1);
    strcpy(buffer, args);

    // check the arguments exist
    const char* delim = " ";
    const char *arg1 = strtok(buffer, delim);
    const char *arg2 = strtok(NULL, delim);
    const char *arg3 = strtok(NULL, delim);
    const char *arg4 = strtok(NULL, delim);
    if (arg1 == NULL || arg2 == NULL || arg3 == NULL || arg4 == NULL) {
        free(buffer);
        return OverlayError("replace command requires 4 arguments");
    }

    // decode the r g b a arguments
    int findr = 0;
    int findg = 0;
    int findb = 0;
    int finda = 0;
    int replacer = 0;
    int replaceg = 0;
    int replaceb = 0;
    int replacea = 0;
    bool negr = false;
    bool negg = false;
    bool negb = false;
    bool nega = false;
    int invr = 0;
    int invg = 0;
    int invb = 0;
    int inva = 0;
    int deltar = 0;
    int deltag = 0;
    int deltab = 0;
    int deltaa = 0;
    const char *error = DecodeReplaceArg(arg1, &findr, &negr, &replacer, &invr, &deltar, 1);
    if (error) { free(buffer); return OverlayError(error); }
    error = DecodeReplaceArg(arg2, &findg, &negg, &replaceg, &invg, &deltag, 2);
    if (error) { free(buffer); return OverlayError(error); }
    error = DecodeReplaceArg(arg3, &findb, &negb, &replaceb, &invb, &deltab, 3);
    if (error) { free(buffer); return OverlayError(error); }
    error = DecodeReplaceArg(arg4, &finda, &nega, &replacea, &inva, &deltaa, 4);
    if (error) { free(buffer); return OverlayError(error); }
    free(buffer);

    // get the current render target
    unsigned char* clipdata = pixmap;
    int w = wd;
    int h = ht;

    // check that negation is correctly used
    if (negg || negb || (nega && negr)) {
        return OverlayError("replace ! may only be at start or before alpha");
    }

    // mark target clip as changed
    DisableTargetClipIndex();

    unsigned char clipr, clipg, clipb, clipa;
    const int bytebits = 8;
    const int remainbits = 32 - bytebits;

    // count how many pixels are replaced
    int numchanged = 0;
    static char result[16];

    bool allwild = (findr == matchany && findg == matchany && findb == matchany && finda == matchany);
    bool zerodelta = (deltar == 0 && deltag == 0 && deltab == 0 && deltaa == 0);
    bool zeroinv = (invr == 0 && invg == 0 && invb == 0 && inva == 0);
    bool fixedreplace = (replacer == 0 && replaceg == 0 && replaceb == 0 && replacea == 0);
    bool destreplace = (replacer == 1 && replaceg == 2 && replaceb == 3 && replacea == 4);

    // some specific common use cases are optimized for performance followed by the general purpose routine
    // the optimized versions are typically an order of magnitude faster

    // optimization case 1: fixed find and replace
    if ((findr != matchany && findg != matchany && findb != matchany && finda != matchany) &&
        fixedreplace && !nega) {
        // use 32 bit colors
        unsigned int* cdata = (unsigned int*)clipdata;
        unsigned int findcol = 0;
        unsigned int replacecol = 0;
        SetRGBA(findr, findg, findb, finda, &findcol);
        SetRGBA(r, g, b, a, &replacecol);

        // check for not equals case
        if (negr) {
            for (int i = 0; i < w * h; i++) {
                if (*cdata != findcol) {
                    *cdata = replacecol;
                    numchanged++;
                }
                cdata++;
            }
        } else {
            for (int i = 0; i < w * h; i++) {
                if (*cdata == findcol) {
                    *cdata = replacecol;
                    numchanged++;
                }
                cdata++;
            }
        }

        // return number of pixels replaced
        sprintf(result, "%d", numchanged);
        return result;
    }

    // optimization case 2: match pixels with different alpha and fixed replace
    if (zerodelta && fixedreplace && zeroinv && nega &&
        ((findr != matchany && findg != matchany && findb != matchany) || (findr == matchany && findg == matchany && findb == matchany))) {
        unsigned int* cdata = (unsigned int*)clipdata;
        unsigned int replacecol = 0;
        SetRGBA(r, g, b, a, &replacecol);
        if (findr != matchany) {
            // fixed match
            for (int i = 0; i < w * h; i++) {
                if (clipdata[0] == findr && clipdata[1] == findg && clipdata[2] == findb && clipdata[3] != finda) {
                    *cdata = replacecol;
                    numchanged++;
                }
                cdata++;
                clipdata += 4;
            }
        } else {
            // r g b wildcard match
            for (int i = 0; i < w * h; i++) {
                if (clipdata[3] != finda) {
                    *cdata = replacecol;
                    numchanged++;
                }
                cdata++;
                clipdata += 4;
            }
        }

        // return number of pixels replaced
        sprintf(result, "%d", numchanged);
        return result;
    }

    // optimization case 3: fill
    if (allwild && zerodelta && fixedreplace) {
        // fill clip with current RGBA
        unsigned int* cdata = (unsigned int*)clipdata;
        unsigned int replacecol = 0;
        SetRGBA(r, g, b, a, &replacecol);
        for (int i = 0; i < w * h; i++) {
            *cdata++ = replacecol;
        }

        // return number of pixels replaced
        numchanged = w * h;
        sprintf(result, "%d", numchanged);
        return result;
    }

    // optimization case 4: no-op
    if (allwild && zerodelta && zeroinv && destreplace) {
        // return number of pixels replaced
        sprintf(result, "%d", numchanged);
        return result;
    }

    // optimization case 5: set constant alpha value on every pixel
    if (allwild && zerodelta && zeroinv &&
        (replacer == 1 && replaceg == 2 && replaceb == 3 && replacea == 0)) {
        // set alpha
        for (int i = 0; i < w * h; i++) {
            if (clipdata[3] != a) {
                clipdata[3] = a;
                numchanged++;
            }
            clipdata += 4;
        }

        // return number of pixels replaced
        sprintf(result, "%d", numchanged);
        return result;
    }

    // optimization case 6: invert colors
    if (allwild && zerodelta && destreplace &&
        (invr != 0 && invg != 0 && invb != 0 && inva == 0)) {
        // invert every pixel
        unsigned int* cdata = (unsigned int*)clipdata;
        unsigned int invmask = 0;
        SetRGBA(255, 255, 255, 0, &invmask);

        for (int i = 0; i < w * h; i++) {
            *cdata = *cdata ^ invmask;
            cdata++;
        }

        // return number of pixels replaced
        numchanged = w * h;
        sprintf(result, "%d", numchanged);
        return result;
    }

    // optimization case 7: offset r g b a values
    if (allwild && zeroinv && destreplace && !zerodelta) {
        // offset rgba values of every pixel
        bool changed;
        int value, orig;
        unsigned int clamp;

        for (int i = 0; i < w * h; i++) {
            changed = false;

            // change r if required
            if (deltar) {
                orig = clipdata[0];
                value = orig + deltar;
                clamp = value >> bytebits;
                if (clamp) { value = ~clamp >> remainbits; }
                changed = value != orig;
                if (changed) {
                    clipdata[0] = value;
                }
            }
            // change g if required
            if (deltag) {
                orig = clipdata[1];
                value = orig + deltag;
                clamp = value >> bytebits;
                if (clamp) { value = ~clamp >> remainbits; }
                changed = value != orig;
                if (changed) {
                    clipdata[1] = value;
                }
            }
            // change b if required
            if (deltab) {
                orig = clipdata[2];
                value = orig + deltab;
                clamp = value >> bytebits;
                if (clamp) { value = ~clamp >> remainbits; }
                changed = value != orig;
                if (changed) {
                    clipdata[2] = value;
                }
            }
            // change a if required
            if (deltaa) {
                orig = clipdata[3];
                value = orig + deltaa;
                clamp = value >> bytebits;
                if (clamp) { value = ~clamp >> remainbits; }
                changed = value != orig;
                if (changed) {
                    clipdata[3] = value;
                }
            }
            // count number changed
            if (changed) {
                numchanged++;
            }
            clipdata += 4;
        }

        // return number of pixels replaced
        sprintf(result, "%d", numchanged);
        return result;
    }

    // general case
    bool matchr, matchg, matchb, matcha, matchpixel;
    int value = 0;
    bool changed = false;
    unsigned int clamp;
    for (int i = 0; i < w * h; i++) {
        // read the clip pixel
        clipr = clipdata[0];
        clipg = clipdata[1];
        clipb = clipdata[2];
        clipa = clipdata[3];

        // check if the pixel components match
        matchr = (findr == matchany) || (findr == clipr);
        matchg = (findg == matchany) || (findg == clipg);
        matchb = (findb == matchany) || (findb == clipb);
        matcha = (finda == matchany) || (finda == clipa);

        // check for negative components
        if (negr) {
            matchpixel = !(matchr && matchg && matchb && matcha);
        } else {
            if (nega) {
                matchpixel = (matchr && matchg && matchb && !matcha);
            } else {
                matchpixel = (matchr && matchg && matchb && matcha);
            }
        }

        // did pixel match
        changed = false;
        if (matchpixel) {
            // match made so process r component
            switch (replacer) {
                case 0:
                    // use current RGBA r component
                    value = r;
                    break;
                case 1:
                    // use clip r component
                    value = clipr ^ invr;
                    break;
                case 2:
                    // use clip g component
                    value = clipg ^ invr;
                    break;
                case 3:
                    // use clip b component
                    value = clipb ^ invr;
                    break;
                case 4:
                    // use clip a component
                    value = clipa ^ invr;
                    break;
            }
            if (deltar) {
                value += deltar;
                clamp = value >> bytebits;
                if (clamp) { value = ~clamp >> remainbits; }
            }
            if (value != clipr) {
                *clipdata = value;
                changed = true;
            }
            clipdata++;

            // g component
            switch (replaceg) {
                case 0:
                    // use current RGBA g component
                    value = g;
                    break;
                case 1:
                    // use clip r component
                    value = clipr ^ invg;
                    break;
                case 2:
                    // use clip g component
                    value = clipg ^ invg;
                    break;
                case 3:
                    // use clip b component
                    value = clipb ^ invg;
                    break;
                case 4:
                    // use clip a component
                    value = clipa ^ invg;
                    break;
            }
            if (deltag) {
                value += deltag;
                clamp = value >> bytebits;
                if (clamp) { value = ~clamp >> remainbits; }
            }
            if (value != clipg) {
                *clipdata = value;
                changed = true;
            }
            clipdata++;

            // b component
            switch (replaceb) {
                case 0:
                    // use current RGBA b component
                    value = b;
                    break;
                case 1:
                    // use clip r component
                    value = clipr ^ invb;
                    break;
                case 2:
                    // use clip g component
                    value = clipr ^ invb;
                    break;
                case 3:
                    // use clip b component
                    value = clipb ^ invb;
                    break;
                case 4:
                    // use clip a component
                    value = clipa ^ invb;
                    break;
            }
            if (deltab) {
                value += deltab;
                clamp = value >> bytebits;
                if (clamp) { value = ~clamp >> remainbits; }
            }
            if (value != clipb) {
                *clipdata = value;
                changed = true;
            }
            clipdata++;

            // a component
            switch (replacea) {
                case 0:
                    // use current RGBA a component
                    value = a;
                    break;
                case 1:
                    // use clip r component
                    value = clipr ^ inva;
                    break;
                case 2:
                    // use clip g component
                    value = clipg ^ inva;
                    break;
                case 3:
                    // use clip b component
                    value = clipb ^ inva;
                    break;
                case 4:
                    // use clip a component
                    value = clipa ^ inva;
                    break;
            }
            if (deltaa) {
                value += deltaa;
            }
            if (value != clipa) {
                *clipdata = value;
                changed = true;
            }
            clipdata++;

            // check if pixel changed
            if (changed) {
                clamp = value >> bytebits;
                if (clamp) { value = ~clamp >> remainbits; }
                numchanged++;
            }
        } else {
            // no match so skip pixel
            clipdata += 4;
        }
    }

    // return number of pixels replaced
    sprintf(result, "%d", numchanged);
    return result;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoSetRGBATable(const char* cmd, lua_State* L, int n, int* nresults)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    // check if there are arguments
    int valid = false;
    int i = 2;

    if (n > 1) {
        // attempt to read the arguments
        lua_rawgeti(L, 1, i++);
        int a1 = (int)lua_tonumberx(L, -1, &valid);
        lua_pop(L, 1);
        if (!valid) return OverlayError("rgba command has illegal red argument");
        lua_rawgeti(L, 1, i++);
        int a2 = (int)lua_tonumberx(L, -1, &valid);
        lua_pop(L, 1);
        if (!valid) return OverlayError("rgba command has illegal green argument");
        lua_rawgeti(L, 1, i++);
        int a3 = (int)lua_tonumberx(L, -1, &valid);
        lua_pop(L, 1);
        if (!valid) return OverlayError("rgba command has illegal blue argument");
        lua_rawgeti(L, 1, i++);
        int a4 = (int)lua_tonumberx(L, -1, &valid);
        lua_pop(L, 1);
        if (!valid) return OverlayError("rgba command has illegal alpha argument");

        // validate argument range
        if (a1 < 0 || a1 > 255 ||
            a2 < 0 || a2 > 255 ||
            a3 < 0 || a3 > 255 ||
            a4 < 0 || a4 > 255) {
            return OverlayError("rgba values must be from 0 to 255");
        }

        // return the current rgba values as a table on the Lua stack
        // {"rgba", r, g, b, a}
        lua_newtable(L);
        i = 1;
        lua_pushstring(L, cmd);
        lua_rawseti(L, -2, i++);
        lua_pushinteger(L, r);
        lua_rawseti(L, -2, i++);
        lua_pushinteger(L, g);
        lua_rawseti(L, -2, i++);
        lua_pushinteger(L, b);
        lua_rawseti(L, -2, i++);
        lua_pushinteger(L, a);
        lua_rawseti(L, -2, i++);
        *nresults = 1;  // 1 result: the table

        // set the new values
        r = (unsigned char)a1;
        g = (unsigned char)a2;
        b = (unsigned char)a3;
        a = (unsigned char)a4;
    } else {
        return OverlayError("rgba command requires 4 arguments");
    }

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoSetRGBA(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int a1, a2, a3, a4;
    if (sscanf(args, " %d %d %d %d", &a1, &a2, &a3, &a4) != 4) {
        return OverlayError("rgba command requires 4 arguments");
    }

    if (a1 < 0 || a1 > 255 ||
        a2 < 0 || a2 > 255 ||
        a3 < 0 || a3 > 255 ||
        a4 < 0 || a4 > 255) {
        return OverlayError("rgba values must be from 0 to 255");
    }

    unsigned char oldr = r;
    unsigned char oldg = g;
    unsigned char oldb = b;
    unsigned char olda = a;

    r = (unsigned char) a1;
    g = (unsigned char) a2;
    b = (unsigned char) a3;
    a = (unsigned char) a4;

    // return old values
    static char result[16];
    sprintf(result, "%hhu %hhu %hhu %hhu", oldr, oldg, oldb, olda);
    return result;
}

// -----------------------------------------------------------------------------

void Overlay::DrawPixel(int x, int y)
{
    // caller must guarantee that pixel is within pixmap
    unsigned char* p = pixmap + y*wd*4 + x*4;
    if (alphablend && a < 255) {
        // do nothing if source pixel is transparent
        if (a > 0) {
            // source pixel is translucent so blend with destination pixel;
            // see https://en.wikipedia.org/wiki/Alpha_compositing#Alpha_blending
            unsigned char destr = p[0];
            unsigned char destg = p[1];
            unsigned char destb = p[2];
            unsigned char desta = p[3];
            if (desta == 255) {
                // destination pixel is opaque
                unsigned int alpha = a + 1;
                unsigned int invalpha = 256 - a;
                p[0] = (alpha * r + invalpha * destr) >> 8;
                p[1] = (alpha * g + invalpha * destg) >> 8;
                p[2] = (alpha * b + invalpha * destb) >> 8;
                // no need to change p[3] (alpha stays at 255)
            } else {
                // destination pixel is translucent
                float alpha = a / 255.0;
                float inva = 1.0 - alpha;
                float destalpha = desta / 255.0;
                float outa = alpha + destalpha * inva;
                p[3] = int(outa * 255);
                if (p[3] > 0) {
                    p[0] = int((r * alpha + destr * destalpha * inva) / outa);
                    p[1] = int((g * alpha + destg * destalpha * inva) / outa);
                    p[2] = int((b * alpha + destb * destalpha * inva) / outa);
                }
            }
        }
    } else {
        p[0] = r;
        p[1] = g;
        p[2] = b;
        p[3] = a;
    }
}

// -----------------------------------------------------------------------------

const char* Overlay::GetCoordinatePair(char* args, int* x, int* y)
{
    // attempt to decode integers
    char c = *args++;
    bool sign = false;
    int newx = 0;
    int newy = 0;

    // skip whitespace
    while (c == ' ') {
        c = *args++;
    }
    if (!c) return NULL;

    // check for sign
    if (c == '-') {
        sign = true;
        c = *args++;
    }
    if (!c) return NULL;

    // read digits
    while (c >= '0' && c <= '9') {
        newx = 10 * newx + (c - '0');
        c = *args++;
    }
    if (sign) newx = -newx;

    // check for end of word
    if (c && c != ' ') return NULL;

    // skip whitespace
    while (c == ' ') {
        c = *args++;
    }
    if (!c) return NULL;

    // check for sign
    sign = false;
    if (c == '-') {
        sign = true;
        c = *args++;
    }

    // read digits
    while (c >= '0' && c <= '9') {
        newy = 10 * newy + (c - '0');
        c = *args++;
    }
    if (sign) newy = -newy;

    // check for end of word
    if (c && c != ' ') return NULL;
    while (c == ' ') {
        c = *args++;
    }
    args--;

    // return coordinates
    *x = newx;
    *y = newy;
    return args;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoSetTable(lua_State* L, int n, int* nresults)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    // check if there are arguments
    // note: it is possible that n > 1 and arguments have nil values
    int valid = false;
    int i = 2;
    int type = -1;

    // mark target clip as changed
    DisableTargetClipIndex();

    // check for alpha blending
    if (alphablend && a < 255) {
        // use alpha blending
        // do nothing if source pixel is transparent
        if (a > 0) {
            do {
                // get next pixel coordinate
                lua_rawgeti(L, 1, i++);
                int x = (int)lua_tonumberx(L, -1, &valid);
                if (!valid) break;
                lua_pop(L, 1);
                lua_rawgeti(L, 1, i++);
                int y = (int)lua_tonumberx(L, -1, &valid);
                if (!valid) break;
                lua_pop(L, 1);

                // ignore pixel if outside pixmap edges
                if (PixelInTarget(x, y)) {
                    unsigned char* p = pixmap + y*wd*4 + x*4;
                    // source pixel is translucent so blend with destination pixel;
                    // see https://en.wikipedia.org/wiki/Alpha_compositing#Alpha_blending
                    unsigned char destr = p[0];
                    unsigned char destg = p[1];
                    unsigned char destb = p[2];
                    unsigned char desta = p[3];
                    if (desta == 255) {
                        // destination pixel is opaque
                        unsigned int alpha = a + 1;
                        unsigned int invalpha = 256 - a;
                        p[0] = (alpha * r + invalpha * destr) >> 8;
                        p[1] = (alpha * g + invalpha * destg) >> 8;
                        p[2] = (alpha * b + invalpha * destb) >> 8;
                        // no need to change p[3] (alpha stays at 255)
                    } else {
                        // destination pixel is translucent
                        float alpha = a / 255.0;
                        float inva = 1.0 - alpha;
                        float destalpha = desta / 255.0;
                        float outa = alpha + destalpha * inva;
                        p[3] = int(outa * 255);
                        if (p[3] > 0) {
                            p[0] = int((r * alpha + destr * destalpha * inva) / outa);
                            p[1] = int((g * alpha + destg * destalpha * inva) / outa);
                            p[2] = int((b * alpha + destb * destalpha * inva) / outa);
                        }
                    }
                }
            } while (i <= n);

            // check if loop terminated because of failed number conversion
            if (!valid) {
                // get the type of the argument
                type = lua_type(L, -1);
                lua_pop(L, 1);
            }
        }
    }
    else {
        // use fast copy
        unsigned int rgba = 0;
        SetRGBA(r, g, b, a, &rgba);
        unsigned int* lpixmap = (unsigned int*)pixmap;
        do {
            // get next pixel coordinate
            lua_rawgeti(L, 1, i++);
            int x = (int)lua_tonumberx(L, -1, &valid);
            if (!valid) break;
            lua_pop(L, 1);
            lua_rawgeti(L, 1, i++);
            int y = (int)lua_tonumberx(L, -1, &valid);
            if (!valid) break;
            lua_pop(L, 1);

            // ignore pixel if outside pixmap edges
            if (PixelInTarget(x, y)) {
                *(lpixmap + y*wd + x) = rgba;
            }
        } while (i <= n);

        // check if loop terminated because of failed number conversion
        if (!valid) {
            // get the type of the argument
            type = lua_type(L, -1);
            lua_pop(L, 1);
        }
    }

    // check if there were errors
    if (!valid) {
        // check if the argument number is a multiple of 2 and the argument is nil
        if ((((i - 3) & 1) == 0) && (type == LUA_TNIL)) {
            // command was valid
            valid = true;
        }
    }

    if (!valid) {
        // return appropriate error message
        switch ((i - 3) & 1) {
            case 0:
                return OverlayError("set command has illegal x");
                break;
            case 1:
                return OverlayError("set command has illegal y");
                break;
        }
    }
    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoSetPixel(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int x = 0;
    int y = 0;
    // get the first pixel coordinates (mandatory)
    args = GetCoordinatePair((char*)args, &x, &y);
    if (!args) return OverlayError("set command requires coordinate pairs");

    // mark target clip as changed
    DisableTargetClipIndex();

    // ignore pixel if outside pixmap edges
    if (PixelInTarget(x, y)) DrawPixel(x, y);

    // read any further coordinates
    while (*args) {
        args = GetCoordinatePair((char*)args, &x, &y);
        if (!args) return OverlayError("set command has illegal coordinates");
        if (PixelInTarget(x, y)) DrawPixel(x, y);
    }

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoGetTable(lua_State* L, int n, int* nresults)
{
    if (pixmap == NULL) return "";

    // check if there are arguments
    int valid = false;
    int i = 2;

    if (n > 1) {
        // attempt to read the arguments
        lua_rawgeti(L, 1, i++);
        int x = (int)lua_tonumberx(L, -1, &valid);
        lua_pop(L,1);
        if (!valid) return OverlayError("get command has illegal x argument");
        lua_rawgeti(L, 1, i++);
        int y = (int)lua_tonumberx(L, -1, &valid);
        lua_pop(L,1);
        if (!valid) return OverlayError("get command has illegal y argument");

        // check if x,y is outside pixmap
        if (!PixelInTarget(x, y)) {
            // return -1 for all components to indicate outside pixmap
            lua_pushinteger(L, -1);
            lua_pushinteger(L, -1);
            lua_pushinteger(L, -1);
            lua_pushinteger(L, -1);
        } else {
            // get and return the pixel rgba values
            unsigned char* p = pixmap + y*wd*4 + x*4;
            lua_pushinteger(L, p[0]);
            lua_pushinteger(L, p[1]);
            lua_pushinteger(L, p[2]);
            lua_pushinteger(L, p[3]);
        }
        *nresults = 4;
    } else {
        return OverlayError("get command requires 2 arguments");
    }
    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoGetPixel(const char* args)
{
    if (pixmap == NULL) return "";

    int x, y;
    if (sscanf(args, "%d %d", &x, &y) != 2) {
        return OverlayError("get command requires 2 arguments");
    }

    // check if x,y is outside pixmap
    if (!PixelInTarget(x, y)) return "";

    unsigned char* p = pixmap + y*wd*4 + x*4;
    static char result[16];
    sprintf(result, "%hhu %hhu %hhu %hhu", p[0], p[1], p[2], p[3]);
    return result;
}

// -----------------------------------------------------------------------------

bool Overlay::TransparentPixel(int x, int y)
{
    if (ovpixmap == NULL) return false;

    // check if x,y is outside pixmap
    if (!PixelInOverlay(x, y)) return false;

    unsigned char* p = ovpixmap + y*ovwd*4 + x*4;

    // return true if alpha value is 0
    return p[3] == 0;
}

// -----------------------------------------------------------------------------

void Overlay::SetOverlayCursor()
{
    if (cursname == "current") {
        // currlayer->curs might have changed
        ovcursor = currlayer->curs;
    }
    #ifdef __WXMAC__
        wxSetCursor(*ovcursor);
    #endif
    viewptr->SetCursor(*ovcursor);
}

// -----------------------------------------------------------------------------

const char* Overlay::DoCursor(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    if (strncmp(args+1, "arrow", 5) == 0) {
        ovcursor = wxSTANDARD_CURSOR;

    } else if (strncmp(args+1, "current", 7) == 0) {
        ovcursor = currlayer->curs;

    } else if (strncmp(args+1, "pencil", 6) == 0) {
        ovcursor = curs_pencil;

    } else if (strncmp(args+1, "pick", 4) == 0) {
        ovcursor = curs_pick;

    } else if (strncmp(args+1, "cross", 5) == 0) {
        ovcursor = curs_cross;

    } else if (strncmp(args+1, "hand", 4) == 0) {
        ovcursor = curs_hand;

    } else if (strncmp(args+1, "zoomin", 6) == 0) {
        ovcursor = curs_zoomin;

    } else if (strncmp(args+1, "zoomout", 7) == 0) {
        ovcursor = curs_zoomout;

    } else if (strncmp(args+1, "wait", 4) == 0) {
        ovcursor = curs_wait;

    } else if (strncmp(args+1, "hidden", 6) == 0) {
        ovcursor = curs_hidden;

    } else {
        return OverlayError("unknown cursor");
    }

    std::string oldcursor = cursname;
    cursname = args+1;

    viewptr->CheckCursor(mainptr->infront);

    // return old cursor name
    static std::string result;
    result = oldcursor;
    return result.c_str();
}

// -----------------------------------------------------------------------------

void Overlay::CheckCursor()
{
    // the cursor needs to be checked if the pixmap data has changed, but that's
    // highly likely if we call this routine at the end of DrawOverlay
    viewptr->CheckCursor(mainptr->infront);
}

// -----------------------------------------------------------------------------

const char* Overlay::DoGetXY()
{
    if (pixmap == NULL) return "";
    if (!mainptr->infront) return "";

    wxPoint pt = viewptr->ScreenToClient( wxGetMousePosition() );

    int ox, oy;
    if (PointInOverlay(pt.x, pt.y, &ox, &oy)) {
        static char result[32];
        sprintf(result, "%d %d", ox, oy);
        return result;
    } else {
        return "";
    }
}

// -----------------------------------------------------------------------------

const char* Overlay::LineOptionWidth(const char* args)
{
    int w, oldwidth;
    if (sscanf(args, " %d", &w) != 1) {
        return OverlayError("lineoption width command requires 1 argument");
    }

    if (w < 1) return OverlayError("line width must be > 0");
    if (w > 10000) return OverlayError("line width must be <= 10000");

    oldwidth = linewidth;
    linewidth = w;

    static char result[32];
    sprintf(result, "%d", oldwidth);
    return result;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoLineOption(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    if (strncmp(args, "width ", 6) == 0) return LineOptionWidth(args+6);

    return OverlayError("unknown lineoption command");
}

// -----------------------------------------------------------------------------

void Overlay::DrawAAPixel(int x, int y, double opac)
{
    if (PixelInTarget(x, y)) {
        unsigned char newalpha = 255-int(opac);
        if (newalpha == 0) return;

        if (!alphablend) {
            // only true in DrawThickEllipse
            if (newalpha > 127) {
                // don't adjust current alpha value
                DrawPixel(x, y);
            }
            return;
        }

        // temporarily adjust current alpha value
        unsigned char olda = a;
        if (a < 255) newalpha = int(newalpha * a / 255);
        a = newalpha;

        DrawPixel(x, y);

        // restore alpha
        a = olda;
    }
}

// -----------------------------------------------------------------------------

// need an adjustment for thick lines when linewidth is an even number
static double even_w;

void Overlay::PerpendicularX(int x0, int y0, int dx, int dy, int xstep, int ystep,
                             int einit, int winit, double w, double D2)
{
    // dx > dy
    int threshold = dx - 2*dy;
    int E_diag = -2*dx;
    int E_square = 2*dy;
    int x = x0;
    int y = y0;
    int err = einit;
    int tk = dx+dy-winit;

    // draw top/bottom half of line
    int q = 0;
    while (tk <= even_w) {
        if (alphablend) {
            double alfa = 255 * (w - tk) / D2;
            if (alfa < 255) {
                if (even_w != w) alfa = 128;
                DrawAAPixel(x, y, 255-alfa);
            } else {
                if (PixelInTarget(x, y)) DrawPixel(x, y);
            }
        } else {
            if (PixelInTarget(x, y)) DrawPixel(x, y);
        }
        if (err >= threshold) {
            x += xstep;
            err += E_diag;
            tk += 2*dy;
        }
        err += E_square;
        y += ystep;
        tk += 2*dx;
        q++;
    }

    y = y0;
    x = x0;
    err = -einit;
    tk = dx+dy+winit;

    // draw other half of line
    int p = 0;
    while (tk <= w) {
        if (p > 0) {
            if (alphablend) {
                double alfa = 255 * (w - tk) / D2;
                if (alfa < 255) {
                    if (even_w != w) alfa = 128;
                    DrawAAPixel(x, y, 255-alfa);
                } else {
                    if (PixelInTarget(x, y)) DrawPixel(x, y);
                }
            } else {
                if (PixelInTarget(x, y)) DrawPixel(x, y);
            }
        }
        if (err > threshold) {
            x -= xstep;
            err += E_diag;
            tk += 2*dy;
        }
        err += E_square;
        y -= ystep;
        tk += 2*dx;
        p++;
    }

    if (q == 0 && p < 2) {
        // needed for very thin lines
        if (PixelInTarget(x0, y0)) DrawPixel(x0, y0);
    }
}

// -----------------------------------------------------------------------------

void Overlay::PerpendicularY(int x0, int y0, int dx, int dy, int xstep, int ystep,
                             int einit, int winit, double w, double D2)
{
    // dx <= dy
    int threshold = dy - 2*dx;
    int E_diag = -2*dy;
    int E_square = 2*dx;
    int x = x0;
    int y = y0;
    int err = -einit;
    int tk = dx+dy+winit;

    // draw left/right half of line
    int q = 0;
    while (tk <= w) {
        if (alphablend) {
            double alfa = 255 * (w - tk) / D2;
            if (alfa < 255) {
                if (even_w != w) alfa = 128;
                DrawAAPixel(x, y, 255-alfa);
            } else {
                if (PixelInTarget(x, y)) DrawPixel(x, y);
            }
        } else {
            if (PixelInTarget(x, y)) DrawPixel(x, y);
        }
        if (err > threshold) {
            y += ystep;
            err += E_diag;
            tk += 2*dx;
        }
        err += E_square;
        x += xstep;
        tk += 2*dy;
        q++;
    }

    y = y0;
    x = x0;
    err = einit;
    tk = dx+dy-winit;

    // draw other half of line
    int p = 0;
    while (tk <= even_w) {
        if (p > 0) {
            if (alphablend) {
                double alfa = 255 * (w - tk) / D2;
                if (alfa < 255) {
                    if (even_w != w) alfa = 128;
                    DrawAAPixel(x, y, 255-alfa);
                } else {
                    if (PixelInTarget(x, y)) DrawPixel(x, y);
                }
            } else {
                if (PixelInTarget(x, y)) DrawPixel(x, y);
            }
        }
        if (err >= threshold) {
            y -= ystep;
            err += E_diag;
            tk += 2*dx;
        }
        err += E_square;
        x -= xstep;
        tk += 2*dy;
        p++;
    }

    if (q == 0 && p < 2) {
        // needed for very thin lines
        if (PixelInTarget(x0, y0)) DrawPixel(x0, y0);
    }
}

// -----------------------------------------------------------------------------

void Overlay::DrawThickLine(int x0, int y0, int x1, int y1)
{
    // based on code from http://kt8216.unixcab.org/murphy/index.html
    
    // following code fixes alignment problems when linewidth is an even number
    if (x0 > x1) {
        // swap starting and end points so we always draw lines from left to right
        int tempx = x0; x0 = x1; x1 = tempx;
        int tempy = y0; y0 = y1; y1 = tempy;
    } else if (x0 == x1 && y0 > y1) {
        // swap y coords so vertical lines are always drawn from top to bottom
        int tempy = y0; y0 = y1; y1 = tempy;
    }

    int dx = x1 - x0;
    int dy = y1 - y0;
    int xstep = 1;
    int ystep = 1;
    int pxstep = 0;
    int pystep = 0;

    if (dx < 0) { dx = -dx; xstep = -1; }
    if (dy < 0) { dy = -dy; ystep = -1; }

    if (dx == 0 && dy == 0) {
        if (PixelInTarget(x0, y0)) DrawPixel(x0, y0);
        return;
    }

    if (dx == 0) xstep = 0;
    if (dy == 0) ystep = 0;

    switch (xstep + ystep*4) {
        case -1 + -1*4 : pystep = -1; pxstep =  1; break;   // -5
        case -1 +  0*4 : pystep = -1; pxstep =  0; break;   // -1
        case -1 +  1*4 : pystep =  1; pxstep =  1; break;   //  3
        case  0 + -1*4 : pystep =  0; pxstep = -1; break;   // -4
        case  0 +  0*4 : pystep =  0; pxstep =  0; break;   //  0
        case  0 +  1*4 : pystep =  0; pxstep =  1; break;   //  4
        case  1 + -1*4 : pystep = -1; pxstep = -1; break;   // -3
        case  1 +  0*4 : pystep = -1; pxstep =  0; break;   //  1
        case  1 +  1*4 : pystep =  1; pxstep = -1; break;   //  5
    }
    
    double D = sqrt(double(dx*dx + dy*dy));
    double D2 = 2*D;
    double w = (linewidth + 1) * D;
    
    // need to reduce thickness of line if linewidth is an even number
    // and line is vertical or horizontal
    if (linewidth % 2 == 0 && (dx == 0 || dy == 0)) {
        even_w = linewidth * D;
    } else {
        even_w = w;
    }
    
    // this hack is needed to improve antialiased sloped lines of width 2
    if (alphablend && linewidth == 2 && dx != 0 && dy != 0) {
        even_w = (linewidth + 1.75) * D;
        w = even_w;
    }

    int p_error = 0;
    int err = 0;
    int x = x0;
    int y = y0;

    if (dx > dy) {
        int threshold = dx - 2*dy;
        int E_diag = -2*dx;
        int E_square = 2*dy;
        int length = dx + 1;
        for (int p = 0; p < length; p++) {
            PerpendicularX(x, y, dx, dy, pxstep, pystep, p_error, err, w, D2);
            if (err >= threshold) {
                y += ystep;
                err += E_diag;
                if (p_error >= threshold) {
                    p_error += E_diag;
                    PerpendicularX(x, y, dx, dy, pxstep, pystep, p_error+E_square, err, w, D2);
                }
                p_error += E_square;
            }
            err += E_square;
            x += xstep;
        }
    } else {
        int threshold = dy - 2*dx;
        int E_diag = -2*dy;
        int E_square = 2*dx;
        int length = dy + 1;
        for (int p = 0; p < length; p++) {
            PerpendicularY(x, y, dx, dy, pxstep, pystep, p_error, err, w, D2);
            if (err >= threshold) {
                x += xstep;
                err += E_diag;
                if (p_error >= threshold) {
                    p_error += E_diag;
                    PerpendicularY(x, y, dx, dy, pxstep, pystep, p_error+E_square, err, w, D2);
                }
                p_error += E_square;
            }
            err += E_square;
            y += ystep;
        }
    }
}

// -----------------------------------------------------------------------------

void Overlay::DrawAntialiasedLine(int x0, int y0, int x1, int y1)
{
    // based on code from http://members.chello.at/~easyfilter/bresenham.html

    long dx = abs(x1-x0);
    long dy = abs(y1-y0);
    long err = dx-dy;
    long e2, x2;
    double ed = dx+dy == 0 ? 1 : sqrt(double(dx*dx+dy*dy));
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;

    while (true) {
        DrawAAPixel(x0, y0, 255*abs(err-dx+dy)/ed);
        e2 = err;
        x2 = x0;
        if (2*e2 >= -dx) {
            if (x0 == x1) break;
            if (e2+dy < ed) DrawAAPixel(x0, y0+sy, 255*(e2+dy)/ed);
            err -= dy;
            x0 += sx;
        }
        if (2*e2 <= dy) {
            if (y0 == y1) break;
            if (dx-e2 < ed) DrawAAPixel(x2+sx, y0, 255*(dx-e2)/ed);
            err += dx;
            y0 += sy;
        }
    }
}

// -----------------------------------------------------------------------------

const char* Overlay::DoLineTable(lua_State* L, int n, bool connected, int* nresults)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    // check if there are arguments
    // note: it is possible that n > 1 and arguments have nil values
    int valid = false;
    int i = 2;
    int type = -1;

    if (n > 1) {
        // get line start coordinate pair
        lua_rawgeti(L, 1, i++);
        int x1 = (int)lua_tonumberx(L, -1, &valid);
        lua_pop(L, 1);
        if (!valid) {
            if (connected) {
                return OverlayError("line command has illegal start x");
            } else {
                return OverlayError("lines command has illegal start x");
            }
        }
        lua_rawgeti(L, 1, i++);
        int y1 = (int)lua_tonumberx(L, -1, &valid);
        lua_pop(L, 1);
        if (!valid) {
            if (connected) {
                return OverlayError("line command has illegal start y");
            } else {
                return OverlayError("lines command has illegal start y");
            }
        }
        // get line end coordinate pair
        lua_rawgeti(L, 1, i++);
        int x2 = (int)lua_tonumberx(L, -1, &valid);
        lua_pop(L, 1);
        if (!valid) {
            if (connected) {
                return OverlayError("line command has illegal end x");
            } else {
                return OverlayError("lines command has illegal end x");
            }
        }
        lua_rawgeti(L, 1, i++);
        int y2 = (int)lua_tonumberx(L, -1, &valid);
        lua_pop(L, 1);
        if (!valid) {
            if (connected) {
                return OverlayError("line command has illegal end y");
            } else {
                return OverlayError("lines command has illegal end y");
            }
        }

        // mark target clip as changed
        DisableTargetClipIndex();

        // draw the first line
        RenderLine(x1, y1, x2, y2);

        // draw any follow on lines
        while (i <= n) {
            if (connected) {
                // start point is previous line's end point
                x1 = x2;
                y1 = y2;
            } else {
                // read the next start point
                lua_rawgeti(L, 1, i++);
                x1 = (int)lua_tonumberx(L, -1, &valid);
                if (!valid) break;
                lua_pop(L, 1);
                lua_rawgeti(L, 1, i++);
                y1 = (int)lua_tonumberx(L, -1, &valid);
                if (!valid) break;
                lua_pop(L, 1);
            }
            // read the next end point
            lua_rawgeti(L, 1, i++);
            x2 = (int)lua_tonumberx(L, -1, &valid);
            if (!valid) break;
            lua_pop(L, 1);
            lua_rawgeti(L, 1, i++);
            y2 = (int)lua_tonumberx(L, -1, &valid);
            if (!valid) break;
            lua_pop(L, 1);

            // draw the line
            RenderLine(x1, y1, x2, y2);
        }

        // check if loop terminated because of failed number conversion
        if (!valid) {
            // get the type of the argument
            type = lua_type(L, -1);
            lua_pop(L, 1);
        }
    }
    else {
        // no arguments supplied
        if (connected) {
            return OverlayError("line command requires at least two coordinate pairs");
        } else {
            return OverlayError("lines command requires at least two coordinate pairs");
        }
    }

    // check if there were errors
    if (!valid) {
        if (connected) {
            // check if the argument number is a multiple of 2 and the argument is nil
            if (!((((i - 3) & 1) == 0) && (type == LUA_TNIL))) {
                switch ((i - 3) & 1) {
                    case 0:
                        return OverlayError("line command has illegal end x");
                        break;
                    case 1:
                        return OverlayError("line command has illegal end y");
                        break;
                }
            }
        } else{
            // check if the argument number is a multiple of 4 and the argument is nil
            if (!((((i - 3) & 3) == 0) && (type == LUA_TNIL))) {
                switch ((i - 3) & 1) {
                    case 0:
                        return OverlayError("lines command has illegal start x");
                        break;
                    case 1:
                        return OverlayError("lines command has illegal start y");
                        break;
                    case 2:
                        return OverlayError("lines command has illegal end x");
                        break;
                    case 3:
                        return OverlayError("lines command has illegal end y");
                        break;
                }
            }
        }
    }

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoLine(const char* args, bool connected)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    args = GetCoordinatePair((char*)args, &x1, &y1);
    if (!args) {
        if (connected) {
            return OverlayError("line command requires at least two coordinate pairs");
        } else {
            return OverlayError("lines command requires at least two coordinate pairs");
        }
    }
    args = GetCoordinatePair((char*)args, &x2, &y2);
    if (!args) {
        if (connected) {
            return OverlayError("line command requires at least two coordinate pairs");
        } else {
            return OverlayError("lines command requires at least two coordinate pairs");
        }
    }

    // mark target clip as changed
    DisableTargetClipIndex();

    // draw the line
    RenderLine(x1, y1, x2, y2);

    // read any further coordinates
    while (*args) {
        if (connected) {
            x1 = x2;
            y1 = y2;
            args = GetCoordinatePair((char*)args, &x2, &y2);
            if (!args) return OverlayError("line command has illegal coordinates");
        } else {
            args = GetCoordinatePair((char*)args, &x1, &y1);
            if (!args) return OverlayError("lines command has illegal coordinates");
            args = GetCoordinatePair((char*)args, &x2, &y2);
            if (!args) return OverlayError("lines command has illegal coordinates");
        }
        RenderLine(x1, y1, x2, y2);
    }
    return NULL;
}


// -----------------------------------------------------------------------------

void Overlay::RenderLine(int x0, int y0, int x1, int y1) {
    if (linewidth > 1) {
        DrawThickLine(x0, y0, x1, y1);
        return;
    }

    if (x0 == x1 && y0 == y1) {
        if (PixelInTarget(x0, y0)) DrawPixel(x0, y0);
        return;
    }

    if (alphablend) {
        DrawAntialiasedLine(x0, y0, x1, y1);
        return;
    }

    // no alpha blending so use fast copy
    unsigned int rgba = 0;
    SetRGBA(r, g, b, a, &rgba);
    unsigned int* lpixmap = (unsigned int*)pixmap;

    // draw a line of pixels from x0,y0 to x1,y1 using Bresenham's algorithm
    int dx = x1 - x0;
    int ax = abs(dx) * 2;
    int sx = dx < 0 ? -1 : 1;

    int dy = y1 - y0;
    int ay = abs(dy) * 2;
    int sy = dy < 0 ? -1 : 1;

    if (ax > ay) {
        int d = ay - (ax / 2);
        while (x0 != x1) {
        if (PixelInTarget(x0, y0)) *(lpixmap + y0*wd + x0) = rgba;
            if (d >= 0) {
                y0 = y0 + sy;
                d = d - ax;
            }
            x0 = x0 + sx;
            d = d + ay;
        }
    } else {
        int d = ax - (ay / 2);
        while (y0 != y1) {
            if (PixelInTarget(x0, y0)) *(lpixmap + y0*wd + x0) = rgba;
            if (d >= 0) {
                x0 = x0 + sx;
                d = d - ay;
            }
            y0 = y0 + sy;
            d = d + ax;
        }
    }
    if (PixelInTarget(x1, y1)) *(lpixmap + y1*wd + x1) = rgba;
}

// -----------------------------------------------------------------------------

void Overlay::DrawThickEllipse(int x0, int y0, int x1, int y1)
{
    // based on code from http://members.chello.at/~easyfilter/bresenham.html

    if (linewidth == 1) {
        if (alphablend) {
            DrawAntialiasedEllipse(x0, y0, x1, y1);
        } else {
            DrawEllipse(x0, y0, x1, y1);
        }
        return;
    }

    if (x1 == x0 || y1 == y0) {
        DrawThickLine(x0, y0, x1, y1);
        return;
    }

    double th = linewidth;
    long a = abs(x1-x0);
    long b = abs(y1-y0);
    long b1 = b&1;
    double a2 = a-2*th;
    double b2 = b-2*th;
    double dx = 4*(a-1)*b*b;
    double dy = 4*(b1-1)*a*a;
    double i = a+b2;
    double err = b1*a*a;
    double dx2, dy2, e2, ed; 

    if ((th-1)*(2*b-th) > a*a) {
        b2 = sqrt(a*(b-a)*i*a2)/(a-th);
    }
    if ((th-1)*(2*a-th) > b*b) {
        a2 = sqrt(b*(a-b)*i*b2)/(b-th);
        th = (a-a2)/2;
    }

    if (b2 <= 0) th = a;    // filled ellipse

    e2 = th-floor(th);
    th = x0+th-e2;
    dx2 = 4*(a2+2*e2-1)*b2*b2;
    dy2 = 4*(b1-1)*a2*a2;
    e2 = dx2*e2;
    y0 += (b+1)>>1;
    y1 = y0-b1;
    a = 8*a*a;
    b1 = 8*b*b;
    a2 = 8*a2*a2;
    b2 = 8*b2*b2;

    do {          
        while (true) {                           
            if (err < 0 || x0 > x1) { i = x0; break; }
            // do outside antialiasing
            i = dx < dy ? dx : dy;
            ed = dx > dy ? dx : dy;
            if (y0 == y1+1 && 2*err > dx && a > b1) {
                ed = a/4;
            } else {
                ed += 2*ed*i*i/(4*ed*ed+i*i+1)+1;
            }
            i = 255*err/ed;
            // i can be > 255
            if (i <= 255) {
                // extra tests avoid some pixels being drawn twice
                if (x0 == x1) {
                    DrawAAPixel(x0, y0, i);
                    DrawAAPixel(x0, y1, i);
                } else if (y0 == y1) {
                    DrawAAPixel(x0, y0, i);
                    DrawAAPixel(x1, y0, i);
                } else {
                    // x0 != x1 and y0 != y1
                    DrawAAPixel(x0, y0, i);
                    DrawAAPixel(x0, y1, i);
                    DrawAAPixel(x1, y0, i);
                    DrawAAPixel(x1, y1, i);
                }
            }
            if (err+dy+a < dx) { i = x0+1; break; }
            x0++;
            x1--;
            err -= dx;
            dx -= b1;
        }
        while (i < th && 2*i <= x0+x1) {
            // set pixel within line
            int x = x0+x1-i;
            // extra tests avoid some pixels being drawn twice
            if (x == i && y0 == y1) {
                if (PixelInTarget(i, y0)) DrawPixel(i, y0);
            } else if (x == i) {
                if (PixelInTarget(i, y0)) DrawPixel(i, y0);
                if (PixelInTarget(i, y1)) DrawPixel(i, y1);
            } else if (y0 == y1) {
                if (PixelInTarget(i, y0)) DrawPixel(i, y0);
                if (PixelInTarget(x, y0)) DrawPixel(x, y0);
            } else {
                // x != i and y0 != y1
                if (PixelInTarget(i, y0)) DrawPixel(i, y0);
                if (PixelInTarget(x, y0)) DrawPixel(x, y0); 
                if (PixelInTarget(i, y1)) DrawPixel(i, y1);
                if (PixelInTarget(x, y1)) DrawPixel(x, y1);
            }
            i++;
        }    
        while (e2 > 0 && x0+x1 >= 2*th) {
            // do inside antialiasing
            i = dx2 < dy2 ? dx2 : dy2;
            ed = dx2 > dy2 ? dx2 : dy2;
            if (y0 == y1+1 && 2*e2 > dx2 && a2 > b2) {
                ed = a2/4;
            } else {
                ed += 2*ed*i*i/(4*ed*ed+i*i);
            }
            i = 255-255*e2/ed;
            // i can be -ve
            if (i < 0) i = 0;
            int x = x0+x1-th;
            // extra test avoids some pixels being drawn twice
            if (x == th) {
                DrawAAPixel(x, y0, i);
                DrawAAPixel(x, y1, i);
            } else {
                DrawAAPixel(th, y0, i);
                DrawAAPixel(x,  y0, i);
                DrawAAPixel(th, y1, i);
                DrawAAPixel(x,  y1, i);
            }
            if (e2+dy2+a2 < dx2) break;
            th++;
            e2 -= dx2;
            dx2 -= b2;
        }
        e2 += dy2 += a2;
        y0++;
        y1--;
        err += dy += a;
    } while (x0 < x1);

    if (y0-y1 <= b) {
        if (err > dy+a) {
            y0--;
            y1++;
            err -= dy -= a;
        }
        while (y0-y1 <= b) {
            i = 255*4*err/b1;
            DrawAAPixel(x0, y0,   i);
            DrawAAPixel(x1, y0++, i);
            DrawAAPixel(x0, y1,   i);
            DrawAAPixel(x1, y1--, i);
            err += dy += a;
        }
    }
}

// -----------------------------------------------------------------------------

void Overlay::DrawAntialiasedEllipse(int x0, int y0, int x1, int y1)
{
    // based on code from http://members.chello.at/~easyfilter/bresenham.html

    long a = abs(x1-x0);
    long b = abs(y1-y0);
    long b1 = b&1;
    double dx = 4*(a-1.0)*b*b;
    double dy = 4*(b1+1.0)*a*a;
    double err = b1*a*a-dx+dy;
    double ed, i;
    bool f;

    if (a == 0 || b == 0) {
        DrawAntialiasedLine(x0, y0, x1, y1);
        return;
    }

    y0 += (b+1)/2;
    y1 = y0-b1;
    a = 8*a*a;
    b1 = 8*b*b;

    while (true) {
        i = dx < dy ? dx : dy;
        ed = dx > dy ? dx : dy;
        if (y0 == y1+1 && err > dy && a > b1) {
            ed = 255*4.0/a;
        } else {
            ed = 255/(ed+2*ed*i*i/(4*ed*ed+i*i));
        }
        i = ed*fabs(err+dx-dy);     // intensity depends on pixel error

        // extra tests avoid pixels at extremities being drawn twice
        if (x0 == x1) {
            DrawAAPixel(x0, y0, i);
            DrawAAPixel(x0, y1, i);
        } else if (y0 == y1) {
            DrawAAPixel(x0, y0, i);
            DrawAAPixel(x1, y0, i);
        } else {
            // x0 != x1 and y0 != y1
            DrawAAPixel(x0, y0, i);
            DrawAAPixel(x0, y1, i);
            DrawAAPixel(x1, y0, i);
            DrawAAPixel(x1, y1, i);
        }

        f = 2*err+dy >= 0;
        if (f) {
            if (x0 >= x1) break;
            i = ed*(err+dx);
            if (i < 255) {
                DrawAAPixel(x0, y0+1, i);
                DrawAAPixel(x0, y1-1, i);
                DrawAAPixel(x1, y0+1, i);
                DrawAAPixel(x1, y1-1, i);
            }
        }
        if (2*err <= dx) {
            i = ed*(dy-err);
            if (i < 255) {
                DrawAAPixel(x0+1, y0, i);
                DrawAAPixel(x1-1, y0, i);
                DrawAAPixel(x0+1, y1, i);
                DrawAAPixel(x1-1, y1, i);
            }
            y0++;
            y1--;
            err += dy += a;
        }
        if (f) {
            x0++;
            x1--;
            err -= dx -= b1;
        }
    }

    if (--x0 == x1++) {
        while (y0-y1 < b) {
            i = 255*4*fabs(err+dx)/b1;
            DrawAAPixel(x0, ++y0, i);
            DrawAAPixel(x1,   y0, i);
            DrawAAPixel(x0, --y1, i);
            DrawAAPixel(x1,   y1, i);
            err += dy += a;
        }
    }
}

// -----------------------------------------------------------------------------

void Overlay::DrawEllipse(int x0, int y0, int x1, int y1)
{
    // based on code from http://members.chello.at/~easyfilter/bresenham.html

    long a = abs(x1-x0);
    long b = abs(y1-y0);
    long b1 = b&1;
    double dx = 4*(1.0-a)*b*b;
    double dy = 4*(b1+1.0)*a*a;
    double err = dx+dy+b1*a*a;
    double e2;

    y0 += (b+1)/2;
    y1 = y0-b1;
    a *= 8*a;
    b1 = 8*b*b;

    do {
        if (PixelInTarget(x1, y0)) DrawPixel(x1, y0);
        if (PixelInTarget(x0, y0)) DrawPixel(x0, y0);
        if (PixelInTarget(x0, y1)) DrawPixel(x0, y1);
        if (PixelInTarget(x1, y1)) DrawPixel(x1, y1);
        e2 = 2*err;
        if (e2 <= dy) {
            y0++;
            y1--;
            err += dy += a;
        }
        if (e2 >= dx || 2*err > dy) {
            x0++;
            x1--;
            err += dx += b1;
        }
    } while (x0 <= x1);

    // note that next test must be <= b
    while (y0-y1 <= b) {
        // finish tip of ellipse
        if (PixelInTarget(x0-1, y0)) DrawPixel(x0-1, y0);
        if (PixelInTarget(x1+1, y0)) DrawPixel(x1+1, y0);
        if (PixelInTarget(x0-1, y1)) DrawPixel(x0-1, y1);
        if (PixelInTarget(x1+1, y1)) DrawPixel(x1+1, y1);
        y0++;
        y1--;
    }
}

// -----------------------------------------------------------------------------

const char* Overlay::DoEllipse(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int x, y, w, h;
    if (sscanf(args, " %d %d %d %d", &x, &y, &w, &h) != 4) {
        return OverlayError("ellipse command requires 4 arguments");
    }

    // treat non-positive w/h as inset from overlay's width/height
    if (w <= 0) w = wd + w;
    if (h <= 0) h = ht + h;
    if (w <= 0) return OverlayError("ellipse width must be > 0");
    if (h <= 0) return OverlayError("ellipse height must be > 0");

    // mark target clip as changed
    DisableTargetClipIndex();

    if (linewidth > 1) {
        DrawThickEllipse(x, y, x+w-1, y+h-1);
        return NULL;
    }

    if (alphablend) {
        DrawAntialiasedEllipse(x, y, x+w-1, y+h-1);
        return NULL;
    }

    // draw a non-antialiased ellipse where linewidth is 1
    DrawEllipse(x, y, x+w-1, y+h-1);
    return NULL;
}

// -----------------------------------------------------------------------------

void Overlay::FillRect(int x, int y, int w, int h)
{
    if (alphablend && a < 255) {
        unsigned char* p = pixmap + y * wd * 4 + x * 4;
        for (int j=y; j<y+h; j++) {
            for (int i=x; i<x+w; i++) {
                // draw the pixel
                if (a > 0) {
                    // source pixel is translucent so blend with destination pixel;
                    // see https://en.wikipedia.org/wiki/Alpha_compositing#Alpha_blending
                    unsigned char destr = p[0];
                    unsigned char destg = p[1];
                    unsigned char destb = p[2];
                    unsigned char desta = p[3];
                    if (desta == 255) {
                        // destination pixel is opaque
                        unsigned int alpha = a + 1;
                        unsigned int invalpha = 256 - a;
                        *p++ = (alpha * r + invalpha * destr) >> 8;
                        *p++ = (alpha * g + invalpha * destg) >> 8;
                        *p++ = (alpha * b + invalpha * destb) >> 8;
                        p++;   // no need to change p[3] (alpha stays at 255)
                    } else {
                        // destination pixel is translucent
                        float alpha = a / 255.0;
                        float inva = 1.0 - alpha;
                        float destalpha = desta / 255.0;
                        float outa = alpha + destalpha * inva;
                        p[3] = int(outa * 255);
                        if (p[3] > 0) {
                            *p++ = int((r * alpha + destr * destalpha * inva) / outa);
                            *p++ = int((g * alpha + destg * destalpha * inva) / outa);
                            *p++ = int((b * alpha + destb * destalpha * inva) / outa);
                            p++;   // already set above
                        } else {
                            p += 4;
                        }
                    }
                } else {
                    // skip transparent pixel
                    p += 4;
                }
            }
            p += (wd - w) * 4;
        }
    } else {
        int rowbytes = wd * 4;
        unsigned char* p = pixmap + y*rowbytes + x*4;
        p[0] = r;
        p[1] = g;
        p[2] = b;
        p[3] = a;

        // copy above pixel to remaining pixels in first row
        unsigned int *dest = (unsigned int*)p;
        unsigned int color = *dest;
        for (int i=1; i<w; i++) {
            *++dest = color;
        }

        // copy first row to remaining rows
        unsigned char* d = p;
        int wbytes = w * 4;
        for (int i=1; i<h; i++) {
            d += rowbytes;
            memcpy(d, p, wbytes);
        }
    }
}

// -----------------------------------------------------------------------------

const char* Overlay::DoFillTable(lua_State* L, int n, int* nresults)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    // check if there are arguments
    // note: it is possible that n > 1 and arguments have nil values
    int valid = false;
    int i = 2;
    int type = -1;

    if (n > 1) {
        // mark target clip as changed
        DisableTargetClipIndex();

        // draw each rectangle
        do {
            // get the coordinates
            lua_rawgeti(L, 1, i++);
            int x = (int)lua_tonumberx(L, -1, &valid);
            if (!valid) break;
            lua_pop(L, 1);
            lua_rawgeti(L, 1, i++);
            int y = (int)lua_tonumberx(L, -1, &valid);
            if (!valid) break;
            lua_pop(L, 1);
            lua_rawgeti(L, 1, i++);
            int w = (int)lua_tonumberx(L, -1, &valid);
            if (!valid) break;
            lua_pop(L, 1);
            lua_rawgeti(L, 1, i++);
            int h = (int)lua_tonumberx(L, -1, &valid);
            if (!valid) break;
            lua_pop(L, 1);

            // treat non-positive w/h as inset from overlay's width/height
            if (w <= 0) w = wd + w;
            if (h <= 0) h = ht + h;
            if (w <= 0) return OverlayError("fill width must be > 0");
            if (h <= 0) return OverlayError("fill height must be > 0");

            // ignore rect if completely outside target edges
            if (!RectOutsideTarget(x, y, w, h)) {
                // clip any part of rect outside target edges
                int xmax = x + w - 1;
                int ymax = y + h - 1;
                if (x < 0) x = 0;
                if (y < 0) y = 0;
                if (xmax >= wd) xmax = wd - 1;
                if (ymax >= ht) ymax = ht - 1;
                w = xmax - x + 1;
                h = ymax - y + 1;
    
                // fill visible rect with current RGBA values
                FillRect(x, y, w, h);
            }
        } while (i <= n);

        // check if loop terminated because of failed number conversion
        if (!valid) {
            // get the type of the argument
            type = lua_type(L, -1);
            lua_pop(L, 1);
        }
    }

    // check if there were no arguments
    // either none supplied, or the first argument value was nil
    if (n == 1 || (i == 3 && type == LUA_TNIL)) {
        // fill entire target with current RGBA values
        FillRect(0, 0, wd, ht);
        valid = true;
    }

    // check if there were errors
    if (!valid) {
        // check if the argument number is a multiple of 4 and the argument is nil
        if ((((i - 3) & 3) == 0) && (type == LUA_TNIL)) {
            // command was valid
            valid = true;
        }
    }

    if (!valid) {
        // return appropriate error message
        switch ((i - 3) & 3) {
            case 0:
                return OverlayError("fill command has illegal x");
                break;
            case 1:
                return OverlayError("fill command has illegal y");
                break;
            case 2:
                return OverlayError("fill command has illegal width");
                break;
            case 3:
                return OverlayError("fill command has illegal height");
                break;
        }
    }

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoFill(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    if (*args == ' ') {
        int x = 0, y = 0, w = 0, h = 0;
        args = GetCoordinatePair((char*)args, &x, &y);
        if (!args) return OverlayError("fill command requires 0 or at least 4 arguments");
        args = GetCoordinatePair((char*)args, &w, &h);
        if (!args) return OverlayError("fill command requires 0 or at least 4 arguments");

        // treat non-positive w/h as inset from overlay's width/height
        if (w <= 0) w = wd + w;
        if (h <= 0) h = ht + h;
        if (w <= 0) return OverlayError("fill width must be > 0");
        if (h <= 0) return OverlayError("fill height must be > 0");

        // mark target clip as changed
        DisableTargetClipIndex();

        // ignore rect if completely outside target edges
        if (!RectOutsideTarget(x, y, w, h)) {
            // clip any part of rect outside target edges
            int xmax = x + w - 1;
            int ymax = y + h - 1;
            if (x < 0) x = 0;
            if (y < 0) y = 0;
            if (xmax >= wd) xmax = wd - 1;
            if (ymax >= ht) ymax = ht - 1;
            w = xmax - x + 1;
            h = ymax - y + 1;

            // fill visible rect with current RGBA values
            FillRect(x, y, w, h);
        }
        
        while (*args) {
            args = GetCoordinatePair((char*)args, &x, &y);
            if (!args) return OverlayError("fill command invalid arguments");
            args = GetCoordinatePair((char*)args, &w, &h);
            if (!args) return OverlayError("fill command invalid arguments");

            // treat non-positive w/h as inset from overlay's width/height
            if (w <= 0) w = wd + w;
            if (h <= 0) h = ht + h;
            if (w <= 0) return OverlayError("fill width must be > 0");
            if (h <= 0) return OverlayError("fill height must be > 0");
    
            // ignore rect if completely outside target edges
            if (!RectOutsideTarget(x, y, w, h)) {
                // clip any part of rect outside target edges
                int xmax = x + w - 1;
                int ymax = y + h - 1;
                if (x < 0) x = 0;
                if (y < 0) y = 0;
                if (xmax >= wd) xmax = wd - 1;
                if (ymax >= ht) ymax = ht - 1;
                w = xmax - x + 1;
                h = ymax - y + 1;
    
                // fill visible rect with current RGBA values
                FillRect(x, y, w, h);
            }
        }
    } else {
        // mark target clip as changed
        DisableTargetClipIndex();

        // fill entire target with current RGBA values
        FillRect(0, 0, wd, ht);
    }

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoCopy(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int x, y, w, h;
    int namepos;
    char dummy;
    if (sscanf(args, " %d %d %d %d %n%c", &x, &y, &w, &h, &namepos, &dummy) != 5) {
        // note that %n is not included in the count
        return OverlayError("copy command requires 5 arguments");
    }

    // treat non-positive w/h as inset from overlay's width/height
    // (makes it easy to copy entire overlay via "copy 0 0 0 0 all")
    if (w <= 0) w = wd + w;
    if (h <= 0) h = ht + h;
    if (w <= 0) return OverlayError("copy width must be > 0");
    if (h <= 0) return OverlayError("copy height must be > 0");

    std::string name = args + namepos;

    // delete any existing clip data with the given name
    std::map<std::string,Clip*>::iterator it;
    it = clips.find(name);
    if (it != clips.end()) {
        delete it->second;
        clips.erase(it);
    }

    bool use_calloc;
    if (RectInsideTarget(x, y, w, h)) {
        // use malloc to allocate clip memory
        use_calloc = false;
    } else {
        // use calloc so parts outside target will be transparent
        use_calloc = true;
    }

    Clip* newclip = new Clip(w, h, use_calloc);
    if (newclip == NULL || newclip->cdata == NULL) {
        delete newclip;
        return OverlayError("not enough memory to copy pixels");
    }

    if (use_calloc) {
        if (RectOutsideTarget(x, y, w, h)) {
            // clip rect is completely outside target so no need to copy
            // target pixels (clip pixels are all transparent)
        } else {
            // calculate offsets in clip data and bytes per row
            int clipx = x >= 0 ? 0 : -x;
            int clipy = y >= 0 ? 0 : -y;
            int cliprowbytes = w * 4;

            // set x,y,w,h to intersection with target
            int xmax = x + w - 1;
            int ymax = y + h - 1;
            if (x < 0) x = 0;
            if (y < 0) y = 0;
            if (xmax >= wd) xmax = wd - 1;
            if (ymax >= ht) ymax = ht - 1;
            w = xmax - x + 1;
            h = ymax - y + 1;

            // copy intersection rect from target into corresponding area of clip data
            unsigned char* dest = newclip->cdata + clipy*cliprowbytes + clipx*4;
            int rowbytes = wd * 4;
            int wbytes = w * 4;
            unsigned char* src = pixmap + y*rowbytes + x*4;
            for (int i = 0; i < h; i++) {
                memcpy(dest, src, wbytes);
                src += rowbytes;
                dest += cliprowbytes;
            }
        }
    } else {
        // given rectangle is within target so fill newclip->cdata with
        // pixel data from that rectangle in pixmap
        unsigned char* dest = newclip->cdata;

        if (x == 0 && y == 0 && w == wd && h == ht) {
            // clip and overlay are the same size so do a fast copy
            memcpy(dest, pixmap, w * h * 4);

        } else {
            // use memcpy to copy each row
            int rowbytes = wd * 4;
            int wbytes = w * 4;
            unsigned char* src = pixmap + y*rowbytes + x*4;
            for (int i = 0; i < h; i++) {
                memcpy(dest, src, wbytes);
                src += rowbytes;
                dest += wbytes;
            }
        }
    }

    clips[name] = newclip;      // create named clip for later use by paste, scale, etc

    return NULL;
}

// -----------------------------------------------------------------------------

void Overlay::DisableTargetClipIndex()
{
    // if the current target is a clip then it may have been modified so disable index
    if (renderclip) {
        renderclip->removeIndex();
    }
}

// -----------------------------------------------------------------------------

const char* Overlay::DoOptimize(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int namepos;
    char dummy;
    if (sscanf(args, " %n%c", &namepos, &dummy) != 1) {
        // note that %n is not included in the count
        return OverlayError("optimize command requires an argument");
    }

    std::string name = args + namepos;
    std::map<std::string,Clip*>::iterator it;
    it = clips.find(name);
    if (it == clips.end()) {
        static std::string msg;
        msg = "unknown optimize clip (";
        msg += name;
        msg += ")";
        return OverlayError(msg.c_str());
    }

    Clip* clipptr = it->second;

    // add index to the clip
    int numtrans = clipptr->addIndex();

    static char result[12];
    sprintf(result, "%d", numtrans);
    return result;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoPasteTable(lua_State* L, int n, int* nresults)
{
    const char* result = NULL;

    // clip name
    const char* clipname = NULL;
    int clipi = 0;

    // allocate space for coordinate values
    if (n > 1) {
        int* coords = (int*)malloc((n - 1) * sizeof(int));
        int j = 0;

        // get the array of coordinates
        int valid = true;
        int i = 2;
        while (i <= n && valid) {
            // read the element at the next index
            lua_rawgeti(L, 1, i);
            // attempt to decode as a number
            lua_Number value = lua_tonumberx(L, -1, &valid);
            if (valid) {
                // store the number
                coords[j++] = (int)value;
            }
            else {
                // was not a number so check the type
                int type = lua_type(L, -1);
                if (type == LUA_TSTRING) {
                    // first time decode as a string after that it's an error
                    if (clipname == NULL) {
                        clipname = lua_tostring(L, -1);
                        clipi = i;
                        valid = true;
                    }
                }
                else {
                    if (type == LUA_TNIL) {
                        // if it's nil then stop
                        n = i - 1;
                        valid = true;
                    }
                }
            }
            lua_pop(L, 1);
            i++;
        }

        // clip name must be last argument
        if (clipname && (clipi != n)) {
            valid = false;
        }

        // check if the coordinates were all numbers
        if (valid) {
            // call the required function
            result = DoPasteTableInternal(coords, j, clipname);
        }

        // free argument list
        free(coords);

        if (!valid) {
            result = OverlayError("paste command has invalid arguments");
        }
    }

    return result;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoPasteTableInternal(const int* coords, int n, const char* clip)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    // check that coordinates are supplied and that there are an even number
    if (clip == NULL) return OverlayError("paste command requires a clip name");
    if (n < 2) return OverlayError("paste command requires coordinate pairs");
    if ((n & 1) != 0) return OverlayError("paste command has illegal coordinates");

    // lookup the named clip
    std::string name = clip;
    std::map<std::string,Clip*>::iterator it;
    it = clips.find(name);
    if (it == clips.end()) {
        static std::string msg;
        msg = "unknown paste clip (";
        msg += name;
        msg += ")";
        return OverlayError(msg.c_str());
    }

    Clip* clipptr = it->second;
    int w = clipptr->cwd;
    int h = clipptr->cht;

    // mark target clip as changed
    DisableTargetClipIndex();
    
    // paste at each coordinate pair
    const int ow = w;
    const int oh = h;

    // get the first coordinate pair
    int ci = 0;
    do {
        int x = coords[ci++];
        int y = coords[ci++];

        // set original width and height since these can be changed below if clipping required
        w = ow;
        h = oh;

        // discard if location is completely outside target
        if (!RectOutsideTarget(x, y, w, h)) {
            // check for transformation
            if (identity) {
                // no transformation, check for clip and target the same size without alpha blending
                if (!alphablend && x == 0 && y == 0 && w == wd && h == ht) {
                    // fast paste with single memcpy call
                    memcpy(pixmap, clipptr->cdata, w * h * 4);
                }
                else {
                    // get the clip data
                    unsigned int *ldata = (unsigned int*)clipptr->cdata;
                    int cliprowpixels = w;
                    int rowoffset = 0;
            
                    // check for clipping
                    int xmax = x + w - 1;
                    int ymax = y + h - 1;
                    if (x < 0) {
                        // skip pixels off left edge
                        ldata += -x;
                        x = 0;
                    }
                    if (y < 0) {
                        // skip pixels off top edge
                        ldata += -y * cliprowpixels;
                        rowoffset = -y;
                        y = 0;
                    }
                    if (xmax >= wd) xmax = wd - 1;
                    if (ymax >= ht) ymax = ht - 1;
                    w = xmax - x + 1;
                    h = ymax - y + 1;
            
                    // get the paste target data
                    int targetrowpixels = wd;
                    unsigned int* lp = (unsigned int*)pixmap;
                    lp += y * targetrowpixels + x;
                    
                    // check for alpha blending
                    if (alphablend) {
                        // alpha blending
                        unsigned char* p = (unsigned char*)lp;
                        bool hasindex = clipptr->hasindex;
                        unsigned char* rowindex = clipptr->rowindex;
            
                        for (int j = 0; j < h; j++) {
                            // if row index exists then skip row if blank (all pixels have alpha 0)
                            if (hasindex && !rowindex[rowoffset + j]) {
                                ldata += w;
                                lp += w;
                            } else {
                                // if row index exists and all pixels opaque then use memcpy
                                if (hasindex && rowindex[rowoffset + j] == 255) {
                                    memcpy(lp, ldata, w << 2);
                                    ldata += w;
                                    lp += w;
                                } else {
                                    for (int i = 0; i < w; i++) {
                                        // get the source pixel
                                        unsigned char* srcp = (unsigned char*)ldata;
                                        unsigned int rgba = *ldata++;
                                        unsigned char pa = srcp[3];
                    
                                        // draw the pixel
                                        if (pa < 255) {
                                            if (pa > 0) {
                                                // source pixel is translucent so blend with destination pixel;
                                                // see https://en.wikipedia.org/wiki/Alpha_compositing#Alpha_blending
                                                unsigned char pr = srcp[0];
                                                unsigned char pg = srcp[1];
                                                unsigned char pb = srcp[2];
                                                unsigned char destr = p[0];
                                                unsigned char destg = p[1];
                                                unsigned char destb = p[2];
                                                unsigned char desta = p[3];
                                                if (desta == 255) {
                                                    // destination pixel is opaque
                                                    unsigned int alpha = pa + 1;
                                                    unsigned int invalpha = 256 - pa;
                                                    *p++ = (alpha * pr + invalpha * destr) >> 8;
                                                    *p++ = (alpha * pg + invalpha * destg) >> 8;
                                                    *p++ = (alpha * pb + invalpha * destb) >> 8;
                                                    p++;   // no need to change p[3] (alpha stays at 255)
                                                } else {
                                                    // destination pixel is translucent
                                                    float alpha = pa / 255.0;
                                                    float inva = 1.0 - alpha;
                                                    float destalpha = desta / 255.0;
                                                    float outa = alpha + destalpha * inva;
                                                    p[3] = int(outa * 255);
                                                    if (p[3] > 0) {
                                                        *p++ = int((pr * alpha + destr * destalpha * inva) / outa);
                                                        *p++ = int((pg * alpha + destg * destalpha * inva) / outa);
                                                        *p++ = int((pb * alpha + destb * destalpha * inva) / outa);
                                                        p++;   // already set above
                                                    } else {
                                                        p += 4;
                                                    }
                                                }
                                            } else {
                                                // skip transparent pixel
                                                p += 4;
                                            }
                                        } else {
                                            // pixel is opaque so copy it
                                            *lp = rgba;
                                            p += 4;
                                        }
                                        lp++;
                                    }
                                }
                            }
                            // next clip and target row
                            lp += targetrowpixels - w;
                            p = (unsigned char*)lp;
                            ldata += cliprowpixels - w;
                        }
                    } else {
                        // no alpha blending
                        for (int j = 0; j < h; j++) {
                            // copy each row with memcpy
                            memcpy(lp, ldata, w << 2);
                            lp += targetrowpixels;
                            ldata += cliprowpixels;
                        }
                    }
                }
            } else {
                // do an affine transformation
                unsigned char* data = clipptr->cdata;
                int x0 = x - (x * axx + y * axy);
                int y0 = y - (x * ayx + y * ayy);
        
                // check for alpha blend
                if (alphablend) {
                    // save RGBA values
                    unsigned char saver = r;
                    unsigned char saveg = g;
                    unsigned char saveb = b;
                    unsigned char savea = a;
        
                    for (int j = 0; j < h; j++) {
                        for (int i = 0; i < w; i++) {
                            r = *data++;
                            g = *data++;
                            b = *data++;
                            a = *data++;
                            int newx = x0 + x * axx + y * axy;
                            int newy = y0 + x * ayx + y * ayy;
                            if (PixelInTarget(newx, newy)) DrawPixel(newx, newy);
                            x++;
                        }
                        y++;
                        x -= w;
                    }
        
                    // restore saved RGBA values
                    r = saver;
                    g = saveg;
                    b = saveb;
                    a = savea;
                } else {
                    // no alpha blend
                    unsigned int* ldata = (unsigned int*)data;
                    unsigned int* lp = (unsigned int*)pixmap;
                    for (int j = 0; j < h; j++) {
                        for (int i = 0; i < w; i++) {
                            int newx = x0 + x * axx + y * axy;
                            int newy = y0 + x * ayx + y * ayy;
                            if (PixelInTarget(newx, newy)) *(lp + newy * wd + newx) = *ldata;
                            ldata++;
                            x++;
                        }
                        y++;
                        x -= w;
                    }
                }
            }
        }
    }
    while (ci < n - 1);

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoPaste(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int x, y;

    // find out the length of the argument string
    int arglen = strlen(args);
    if (arglen == 0) {
        return OverlayError("paste command requires at least 3 arguments");
    }

    // make a copy of the arguments so we can change them
    char *buffer = (char*)malloc(arglen + 1);   // add 1 for the terminating nul
    if (buffer == NULL) return OverlayError("not enough memory for paste");
    char *copy = buffer;
    strcpy(copy, args);

    // find the last argument which should be the clip name
    char* lastarg = copy + arglen - 1;

    // skip trailing whitespace
    while (lastarg >= copy && *lastarg == ' ') {
        lastarg--;
    }

    // skip until whitespace
    while (lastarg >= copy && *lastarg != ' ') {
        lastarg--;
    }

    // check if clip name was found
    if (lastarg < copy) {
        free(buffer);
        return OverlayError("paste command requires at least 3 arguments");
    }

    // null terminate the arguments before the clip name
    *lastarg++ = 0;

    // lookup the named clip
    std::string name = lastarg;
    std::map<std::string,Clip*>::iterator it;
    it = clips.find(name);
    if (it == clips.end()) {
        static std::string msg;
        msg = "unknown paste clip (";
        msg += name;
        msg += ")";
        free(buffer);
        return OverlayError(msg.c_str());
    }

    Clip* clipptr = it->second;
    int w = clipptr->cwd;
    int h = clipptr->cht;

    // read the first coordinate pair
    copy = (char*)GetCoordinatePair(copy, &x, &y);
    if (!copy) {
        free(buffer);
        return OverlayError("paste command requires a least one coordinate pair");
    }

    // mark target clip as changed
    DisableTargetClipIndex();
    
    // paste at each coordinate pair
    const int ow = w;
    const int oh = h;
    do {
        // set original width and height since these can be changed below if clipping required
        w = ow;
        h = oh;

        // discard if location is completely outside target
        if (!RectOutsideTarget(x, y, w, h)) {
            // check for transformation
            if (identity) {
                // no transformation, check for clip and target the same size without alpha blending
                if (!alphablend && x == 0 && y == 0 && w == wd && h == ht) {
                    // fast paste with single memcpy call
                    memcpy(pixmap, clipptr->cdata, w * h * 4);
                }
                else {
                    // get the clip data
                    unsigned int *ldata = (unsigned int*)clipptr->cdata;
                    int cliprowpixels = w;
                    int rowoffset = 0;
            
                    // check for clipping
                    int xmax = x + w - 1;
                    int ymax = y + h - 1;
                    if (x < 0) {
                        // skip pixels off left edge
                        ldata += -x;
                        x = 0;
                    }
                    if (y < 0) {
                        // skip pixels off top edge
                        ldata += -y * cliprowpixels;
                        rowoffset = -y;
                        y = 0;
                    }
                    if (xmax >= wd) xmax = wd - 1;
                    if (ymax >= ht) ymax = ht - 1;
                    w = xmax - x + 1;
                    h = ymax - y + 1;
            
                    // get the paste target data
                    int targetrowpixels = wd;
                    unsigned int* lp = (unsigned int*)pixmap;
                    lp += y * targetrowpixels + x;
                    
                    // check for alpha blending
                    if (alphablend) {
                        // alpha blending
                        unsigned char* p = (unsigned char*)lp;
                        bool hasindex = clipptr->hasindex;
                        unsigned char* rowindex = clipptr->rowindex;
            
                        for (int j = 0; j < h; j++) {
                            // if row index exists then skip row if blank (all pixels have alpha 0)
                            if (hasindex && !rowindex[rowoffset + j]) {
                                ldata += w;
                                lp += w;
                            } else {
                                // if row index exists and all pixels opaque then use memcpy
                                if (hasindex && rowindex[rowoffset + j] == 255) {
                                    memcpy(lp, ldata, w << 2);
                                    ldata += w;
                                    lp += w;
                                } else {
                                    for (int i = 0; i < w; i++) {
                                        // get the source pixel
                                        unsigned char* srcp = (unsigned char*)ldata;
                                        unsigned int rgba = *ldata++;
                                        unsigned char pa = srcp[3];
                    
                                        // draw the pixel
                                        if (pa < 255) {
                                            if (pa > 0) {
                                                // source pixel is translucent so blend with destination pixel;
                                                // see https://en.wikipedia.org/wiki/Alpha_compositing#Alpha_blending
                                                unsigned char pr = srcp[0];
                                                unsigned char pg = srcp[1];
                                                unsigned char pb = srcp[2];
                                                unsigned char destr = p[0];
                                                unsigned char destg = p[1];
                                                unsigned char destb = p[2];
                                                unsigned char desta = p[3];
                                                if (desta == 255) {
                                                    // destination pixel is opaque
                                                    unsigned int alpha = pa + 1;
                                                    unsigned int invalpha = 256 - pa;
                                                    *p++ = (alpha * pr + invalpha * destr) >> 8;
                                                    *p++ = (alpha * pg + invalpha * destg) >> 8;
                                                    *p++ = (alpha * pb + invalpha * destb) >> 8;
                                                    p++;   // no need to change p[3] (alpha stays at 255)
                                                } else {
                                                    // destination pixel is translucent
                                                    float alpha = pa / 255.0;
                                                    float inva = 1.0 - alpha;
                                                    float destalpha = desta / 255.0;
                                                    float outa = alpha + destalpha * inva;
                                                    p[3] = int(outa * 255);
                                                    if (p[3] > 0) {
                                                        *p++ = int((pr * alpha + destr * destalpha * inva) / outa);
                                                        *p++ = int((pg * alpha + destg * destalpha * inva) / outa);
                                                        *p++ = int((pb * alpha + destb * destalpha * inva) / outa);
                                                        p++;   // already set above
                                                    } else {
                                                        p += 4;
                                                    }
                                                }
                                            } else {
                                                // skip transparent pixel
                                                p += 4;
                                            }
                                        } else {
                                            // pixel is opaque so copy it
                                            *lp = rgba;
                                            p += 4;
                                        }
                                        lp++;
                                    }
                                }
                            }
                            // next clip and target row
                            lp += targetrowpixels - w;
                            p = (unsigned char*)lp;
                            ldata += cliprowpixels - w;
                        }
                    } else {
                        // no alpha blending
                        for (int j = 0; j < h; j++) {
                            // copy each row with memcpy
                            memcpy(lp, ldata, w << 2);
                            lp += targetrowpixels;
                            ldata += cliprowpixels;
                        }
                    }
                }
            } else {
                // do an affine transformation
                unsigned char* data = clipptr->cdata;
                int x0 = x - (x * axx + y * axy);
                int y0 = y - (x * ayx + y * ayy);
        
                // check for alpha blend
                if (alphablend) {
                    // save RGBA values
                    unsigned char saver = r;
                    unsigned char saveg = g;
                    unsigned char saveb = b;
                    unsigned char savea = a;
        
                    for (int j = 0; j < h; j++) {
                        for (int i = 0; i < w; i++) {
                            r = *data++;
                            g = *data++;
                            b = *data++;
                            a = *data++;
                            int newx = x0 + x * axx + y * axy;
                            int newy = y0 + x * ayx + y * ayy;
                            if (PixelInTarget(newx, newy)) DrawPixel(newx, newy);
                            x++;
                        }
                        y++;
                        x -= w;
                    }
        
                    // restore saved RGBA values
                    r = saver;
                    g = saveg;
                    b = saveb;
                    a = savea;
                } else {
                    // no alpha blend
                    unsigned int* ldata = (unsigned int*)data;
                    unsigned int* lp = (unsigned int*)pixmap;
                    for (int j = 0; j < h; j++) {
                        for (int i = 0; i < w; i++) {
                            int newx = x0 + x * axx + y * axy;
                            int newy = y0 + x * ayx + y * ayy;
                            if (PixelInTarget(newx, newy)) *(lp + newy * wd + newx) = *ldata;
                            ldata++;
                            x++;
                        }
                        y++;
                        x -= w;
                    }
                }
            }
        }
    }
    while ((copy = (char*)GetCoordinatePair(copy, &x, &y)) != 0);

    // free the buffer
    free(buffer);

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoScale(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    wxImageResizeQuality quality;
    if (strncmp(args, " best ", 6) == 0) {
        quality = wxIMAGE_QUALITY_HIGH;
        args += 6;
    } else if (strncmp(args, " fast ", 6) == 0) {
        quality = wxIMAGE_QUALITY_NORMAL;
        args += 6;
    } else {
        return OverlayError("scale quality must be best or fast");
    }
    
    int x, y, w, h;
    int namepos;
    char dummy;
    if (sscanf(args, "%d %d %d %d %n%c", &x, &y, &w, &h, &namepos, &dummy) != 5) {
        // note that %n is not included in the count
        return OverlayError("scale command requires 5 arguments");
    }

    // treat non-positive w/h as inset from target's width/height
    if (w <= 0) w = wd + w;
    if (h <= 0) h = ht + h;
    if (w <= 0) return OverlayError("scale width must be > 0");
    if (h <= 0) return OverlayError("scale height must be > 0");

    std::string name = args + namepos;
    std::map<std::string,Clip*>::iterator it;
    it = clips.find(name);
    if (it == clips.end()) {
        static std::string msg;
        msg = "unknown scale clip (";
        msg += name;
        msg += ")";
        return OverlayError(msg.c_str());
    }

    // do nothing if scaled rect is completely outside target
    if (RectOutsideTarget(x, y, w, h)) return NULL;

    Clip* clipptr = it->second;
    int clipw = clipptr->cwd;
    int cliph = clipptr->cht;
    
    if (w > clipw && w % clipw == 0 &&
        h > cliph && h % cliph == 0 && quality == wxIMAGE_QUALITY_NORMAL) {
        // no need to create a wxImage to expand pixels by integer multiples
        DisableTargetClipIndex();
        int xscale = w / clipw;
        int yscale = h / cliph;
        unsigned char* p = clipptr->cdata;

        // save current RGBA values
        unsigned char saver = r;
        unsigned char saveg = g;
        unsigned char saveb = b;
        unsigned char savea = a;
        
        if (RectInsideTarget(x, y, w, h)) {
            for (int j = 0; j < cliph; j++) {
                for (int i = 0; i < clipw; i++) {
                    r = *p++;
                    g = *p++;
                    b = *p++;
                    a = *p++;
                    FillRect(x, y, xscale, yscale);
                    x += xscale;
                }
                y += yscale;
                x -= clipw * xscale;
            }
        } else {
            for (int j = 0; j < cliph; j++) {
                for (int i = 0; i < clipw; i++) {
                    r = *p++;
                    g = *p++;
                    b = *p++;
                    a = *p++;
                    if (RectOutsideTarget(x, y, xscale, yscale)) {
                        // expanded pixel is outside target
                    } else {
                        for (int row = 0; row < yscale; row++) {
                            for (int col = 0; col < xscale; col++) {
                                if (PixelInTarget(x+col, y+row)) DrawPixel(x+col, y+row);
                            }
                        }
                    }
                    x += xscale;
                }
                y += yscale;
                x -= clipw * xscale;
            }
        }

        // restore saved RGBA values
        r = saver;
        g = saveg;
        b = saveb;
        a = savea;

        return NULL;
    }

    // get the clip's RGB and alpha data so we can create a wxImage
    unsigned char* rgbdata = (unsigned char*) malloc(clipw * cliph * 3);
    if (rgbdata== NULL) {
        return OverlayError("not enough memory to scale rgb data");
    }
    unsigned char* alphadata = (unsigned char*) malloc(clipw * cliph);
    if (alphadata == NULL) {
        free(rgbdata);
        return OverlayError("not enough memory to scale alpha data");
    }

    unsigned char* p = clipptr->cdata;
    int rgbpos = 0;
    int alphapos = 0;
    for (int j = 0; j < cliph; j++) {
        for (int i = 0; i < clipw; i++) {
            rgbdata[rgbpos++] = *p++;
            rgbdata[rgbpos++] = *p++;
            rgbdata[rgbpos++] = *p++;
            alphadata[alphapos++] = *p++;
        }
    }

    // create wxImage with the given clip's size and using its RGB and alpha data;
    // static_data flag is false so wxImage dtor will free rgbdata and alphadata
    wxImage image(clipw, cliph, rgbdata, alphadata, false);
    
    // scale the wxImage to the requested width and height
    image.Rescale(w, h, quality);

    // mark target clip as changed
    DisableTargetClipIndex();

    // save current RGBA values
    unsigned char saver = r;
    unsigned char saveg = g;
    unsigned char saveb = b;
    unsigned char savea = a;
    
    // copy the pixels from the scaled wxImage into the current target
    unsigned char* rdata = image.GetData();
    unsigned char* adata = image.GetAlpha();
    rgbpos = 0;
    alphapos = 0;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            r = rdata[rgbpos++];
            g = rdata[rgbpos++];
            b = rdata[rgbpos++];
            a = adata[alphapos++];
            if (PixelInTarget(x, y)) DrawPixel(x, y);
            x++;
        }
        y++;
        x -= w;
    }

    // restore saved RGBA values
    r = saver;
    g = saveg;
    b = saveb;
    a = savea;

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoTarget(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int namepos;
    char dummy;
    int numargs = sscanf(args, " %n%c", &namepos, &dummy);
    if (numargs != 1) {
        if (*args == 0 || *args == ' ') {
            numargs = 0;
        } else {
            return OverlayError("target command requires 0 or 1 arguments");
        }
    }

    // previous target name
    static std::string result;
    result = targetname;

    // no arguments means overlay is the target
    if (numargs == 0) {
        SetRenderTarget(ovpixmap, ovwd, ovht, NULL);
        targetname = "";
    } else {
        // one argument means clip is the target
        std::string name = args + namepos;
        std::map<std::string,Clip*>::iterator it;
        it = clips.find(name);
        if (it == clips.end()) {
            static std::string msg;
            msg = "unknown target name (";
            msg += name;
            msg += ")";
            return OverlayError(msg.c_str());
        } else {
            // set clip as the target
            Clip* clipptr = it->second;
            SetRenderTarget(clipptr->cdata, clipptr->cwd, clipptr->cht, clipptr);
            targetname = name;
        }
    }

    // return previous target
    return result.c_str();
}

// -----------------------------------------------------------------------------

const char* Overlay::DoDelete(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    // check for optional clip name
    int namepos;
    char dummy;
    int numargs = sscanf(args, " %n%c", &namepos, &dummy);
    if (numargs != 1) {
        if (*args == 0 || *args == ' ') {
            numargs = 0;
        } else {
            return OverlayError("delete command requires 0 or 1 arguments");
        }
    }

    // was optional clip name specified
    if (numargs == 0) {
        // no so delete overlay
        DeleteOverlay();
    } else {
        // yes so look up clip by name
        std::string name = args + namepos;
        std::map<std::string,Clip*>::iterator it;
        it = clips.find(name);
        if (it == clips.end()) {
            static std::string msg;
            msg = "unknown delete clip (";
            msg += name;
            msg += ")";
            return OverlayError(msg.c_str());
        }
        // check if the clip is the current render target
        if (name == targetname) {
            return OverlayError("delete clip is current render target");
        } else {
            // delete the clip
            delete it->second;
            clips.erase(it);
        }
    }

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoLoad(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int x, y;
    int filepos;
    char dummy;
    if (sscanf(args, " %d %d %n%c", &x, &y, &filepos, &dummy) != 3) {
        // note that %n is not included in the count
        return OverlayError("load command requires 3 arguments");
    }

    wxString filepath = wxString(args + filepos, wxConvLocal);
    if (!wxFileExists(filepath)) {
        return OverlayError("given file does not exist");
    }

    wxImage image;
    if (!image.LoadFile(filepath)) {
        return OverlayError("failed to load image from given file");
    }

    int imgwd = image.GetWidth();
    int imght = image.GetHeight();
    if (RectOutsideTarget(x, y, imgwd, imght)) {
        // do nothing if image rect is completely outside target,
        // but we still return the image dimensions so users can do things
        // like center the image within the target
    } else {
        // mark target clip as changed
        DisableTargetClipIndex();

        // use alpha data if it exists otherwise try looking for mask
        unsigned char* alphadata = NULL;
        if (image.HasAlpha()) {
            alphadata = image.GetAlpha();
        }
        unsigned char maskr = 0;
        unsigned char maskg = 0;
        unsigned char maskb = 0;
        bool hasmask = false;
        if (alphadata == NULL) {
            hasmask = image.GetOrFindMaskColour(&maskr, &maskg, &maskb);
        }

        // save current RGBA values
        unsigned char saver = r;
        unsigned char saveg = g;
        unsigned char saveb = b;
        unsigned char savea = a;

        unsigned char* rgbdata = image.GetData();
        int rgbpos = 0;
        int alphapos = 0;
        for (int j = 0; j < imght; j++) {
            for (int i = 0; i < imgwd; i++) {
                r = rgbdata[rgbpos++];
                g = rgbdata[rgbpos++];
                b = rgbdata[rgbpos++];
                if (alphadata) {
                    a = alphadata[alphapos++];
                } else if (hasmask && r == maskr && g == maskg && b == maskb) {
                    // transparent pixel
                    a = 0;
                } else {
                    a = 255;
                }
                if (PixelInTarget(x, y)) DrawPixel(x, y);
                x++;
            }
            y++;
            x -= imgwd;
        }

        // restore saved RGBA values
        r = saver;
        g = saveg;
        b = saveb;
        a = savea;
    }

    // return image dimensions
    static char result[32];
    sprintf(result, "%d %d", imgwd, imght);
    return result;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoSave(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int x, y, w, h;
    int filepos;
    char dummy;
    if (sscanf(args, " %d %d %d %d %n%c", &x, &y, &w, &h, &filepos, &dummy) != 5) {
        // note that %n is not included in the count
        return OverlayError("save command requires 5 arguments");
    }

    // treat non-positive w/h as inset from overlay's width/height
    // (makes it easy to save entire overlay via "save 0 0 0 0 foo.png")
    if (w <= 0) w = wd + w;
    if (h <= 0) h = ht + h;
    if (w <= 0) return OverlayError("save width must be > 0");
    if (h <= 0) return OverlayError("save height must be > 0");

    if (x < 0 || x+w > wd || y < 0 || y+h > ht) {
        return OverlayError("save rectangle must be within overlay");
    }

    wxString filepath = wxString(args + filepos, wxConvLocal);
    wxString ext = filepath.AfterLast('.');
    if (!ext.IsSameAs(wxT("png"),false)) {
        return OverlayError("save file must have a .png extension");
    }

    unsigned char* rgbdata = (unsigned char*) malloc(w * h * 3);
    if (rgbdata== NULL) {
        return OverlayError("not enough memory to save RGB data");
    }
    unsigned char* alphadata = (unsigned char*) malloc(w * h);
    if (alphadata == NULL) {
        free(rgbdata);
        return OverlayError("not enough memory to save alpha data");
    }

    int rgbpos = 0;
    int alphapos = 0;
    int rowbytes = wd * 4;
    for (int j=y; j<y+h; j++) {
        for (int i=x; i<x+w; i++) {
            // get pixel at i,j
            unsigned char* p = pixmap + j*rowbytes + i*4;
            rgbdata[rgbpos++] = p[0];
            rgbdata[rgbpos++] = p[1];
            rgbdata[rgbpos++] = p[2];
            alphadata[alphapos++] = p[3];
        }
    }

    // create image of requested size using the given RGB and alpha data;
    // static_data flag is false so wxImage dtor will free rgbdata and alphadata
    wxImage image(w, h, rgbdata, alphadata, false);

    if (!image.SaveFile(filepath)) {
        return OverlayError("failed to save image in given file");
    }

    return NULL;
}

// -----------------------------------------------------------------------------

void Overlay::SaveOverlay(const wxString& pngpath)
{
    if (ovpixmap == NULL) {
        Warning(_("There is no overlay data to save!"));
        return;
    }

    unsigned char* rgbdata = (unsigned char*) malloc(ovwd * ovht * 3);
    if (rgbdata== NULL) {
        Warning(_("Not enough memory to copy RGB data."));
        return;
    }
    unsigned char* alphadata = (unsigned char*) malloc(ovwd * ovht);
    if (alphadata == NULL) {
        free(rgbdata);
        Warning(_("Not enough memory to copy alpha data."));
        return;
    }

    unsigned char* p = ovpixmap;
    int rgbpos = 0;
    int alphapos = 0;
    for (int j=0; j<ht; j++) {
        for (int i=0; i<wd; i++) {
            rgbdata[rgbpos++] = p[0];
            rgbdata[rgbpos++] = p[1];
            rgbdata[rgbpos++] = p[2];
            alphadata[alphapos++] = p[3];
            p += 4;
        }
    }

    // create image using the given RGB and alpha data;
    // static_data flag is false so wxImage dtor will free rgbdata and alphadata
    wxImage image(wd, ht, rgbdata, alphadata, false);

    if (!image.SaveFile(pngpath)) {
        Warning(_("Failed to save overlay in given file."));
    }
}

// -----------------------------------------------------------------------------

const char* Overlay::DoFlood(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int x, y;
    if (sscanf(args, " %d %d", &x, &y) != 2) {
        return OverlayError("flood command requires 2 arguments");
    }

    // // check if x,y is outside pixmap
    if (!PixelInTarget(x, y)) return NULL;

    int rowbytes = wd * 4;
    unsigned char* oldpxl = pixmap + y*rowbytes + x*4;
    unsigned char oldr = oldpxl[0];
    unsigned char oldg = oldpxl[1];
    unsigned char oldb = oldpxl[2];
    unsigned char olda = oldpxl[3];

    // do nothing if color of given pixel matches current RGBA values
    if (oldr == r && oldg == g && oldb == b && olda == a) return NULL;

    // mark target clip as changed
    DisableTargetClipIndex();

    // do flood fill using fast scanline algorithm
    // (based on code at http://lodev.org/cgtutor/floodfill.html)
    bool slowdraw = alphablend && a < 255;
    int maxy = ht - 1;
    std::vector<int> xcoord;
    std::vector<int> ycoord;
    xcoord.push_back(x);
    ycoord.push_back(y);
    while (!xcoord.empty()) {
        // get pixel coords from end of vectors
        x = xcoord.back();
        y = ycoord.back();
        xcoord.pop_back();
        ycoord.pop_back();

        bool above = false;
        bool below = false;

        unsigned char* newpxl = pixmap + y*rowbytes + x*4;
        while (x >= 0 && PixelsMatch(newpxl,oldr,oldg,oldb,olda)) {
            x--;
            newpxl -= 4;
        }
        x++;
        newpxl += 4;

        while (x < wd && PixelsMatch(newpxl,oldr,oldg,oldb,olda)) {
            if (slowdraw) {
                // pixel is within pixmap
                DrawPixel(x,y);
            } else {
                newpxl[0] = r;
                newpxl[1] = g;
                newpxl[2] = b;
                newpxl[3] = a;
            }

            if (y > 0) {
                unsigned char* apxl = newpxl - rowbytes;    // pixel at x, y-1

                if (!above && PixelsMatch(apxl,oldr,oldg,oldb,olda)) {
                    xcoord.push_back(x);
                    ycoord.push_back(y-1);
                    above = true;
                } else if (above && !PixelsMatch(apxl,oldr,oldg,oldb,olda)) {
                    above = false;
                }
            }

            if (y < maxy) {
                unsigned char* bpxl = newpxl + rowbytes;    // pixel at x, y+1

                if (!below && PixelsMatch(bpxl,oldr,oldg,oldb,olda)) {
                    xcoord.push_back(x);
                    ycoord.push_back(y+1);
                    below = true;
                } else if (below && !PixelsMatch(bpxl,oldr,oldg,oldb,olda)) {
                    below = false;
                }
            }

            x++;
            newpxl += 4;
        }
    }

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoBlend(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int i;
    if (sscanf(args, " %d", &i) != 1) {
        return OverlayError("blend command requires 1 argument");
    }

    if (i < 0 || i > 1) {
        return OverlayError("blend value must be 0 or 1");
    }

    int oldblend = alphablend ? 1 : 0;
    alphablend = i > 0;

    // return old value
    static char result[2];
    sprintf(result, "%d", oldblend);
    return result;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoFont(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    bool samename = false;      // only change font size?
    const char* newname = NULL;
    int newsize;
    int namepos;
    char dummy;
    int numargs = sscanf(args, " %d %n%c", &newsize, &namepos, &dummy);
    if (numargs == 1) {
        samename = true;
    } else if (numargs != 2) {
        // note that %n is not included in the count
        return OverlayError("font command requires 1 or 2 arguments");
    }

    if (newsize <= 0 || newsize >= 1000) {
        return OverlayError("font size must be > 0 and < 1000");
    }

    #ifdef __WXMAC__
        // need to increase Mac font size by 25% to match text size on Win/Linux
        int ptsize = int(newsize * 1.25 + 0.5);

        // set extraht to avoid GetTextExtent bug that clips descenders
        extraht = 1;
        if (strncmp(args+namepos, "default", 7) == 0 &&
            (newsize == 20 || newsize == 24 || newsize == 47)) {
            // strange but true
            extraht = 2;
        }
    #else
        int ptsize = newsize;
    #endif

    if (samename) {
        // just change the current font's size
        currfont.SetPointSize(ptsize);

    } else {
        newname = args + namepos;

        // check if given font name is valid
        if (strcmp(newname, "default") == 0) {
            currfont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);

        } else if (strcmp(newname, "default-bold") == 0) {
            currfont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
            currfont.SetWeight(wxFONTWEIGHT_BOLD);

        } else if (strcmp(newname, "default-italic") == 0) {
            currfont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
            currfont.SetStyle(wxFONTSTYLE_ITALIC);

        } else if (strcmp(newname, "mono") == 0) {
            currfont = wxFont(ptsize, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

        } else if (strcmp(newname, "mono-bold") == 0) {
            currfont = wxFont(ptsize, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);

        } else if (strcmp(newname, "mono-italic") == 0) {
            currfont = wxFont(ptsize, wxFONTFAMILY_MODERN, wxFONTSTYLE_ITALIC, wxFONTWEIGHT_NORMAL);

        } else if (strcmp(newname, "roman") == 0) {
            currfont = wxFont(ptsize, wxFONTFAMILY_ROMAN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

        } else if (strcmp(newname, "roman-bold") == 0) {
            currfont = wxFont(ptsize, wxFONTFAMILY_ROMAN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);

        } else if (strcmp(newname, "roman-italic") == 0) {
            currfont = wxFont(ptsize, wxFONTFAMILY_ROMAN, wxFONTSTYLE_ITALIC, wxFONTWEIGHT_NORMAL);

        } else {
            return OverlayError("unknown font name");
        }

        // note that calling SetPointSize here avoids a bug in wxFont
        // that causes a 70pt font to end up as 8pt
        currfont.SetPointSize(ptsize);
    }

    int oldfontsize = fontsize;
    std::string oldfontname = fontname;

    fontsize = newsize;
    if (!samename) fontname = newname;

    // return old fontsize and fontname
    char ibuff[16];
    sprintf(ibuff, "%d", oldfontsize);
    static std::string result;
    result = ibuff;
    result += " ";
    result += oldfontname;
    return result.c_str();
}

// -----------------------------------------------------------------------------

const char* Overlay::TextOptionAlign(const char* args)
{
    text_alignment newalign;

    // check the specified alignment
    if (strcmp(args, "left") == 0) {
        newalign = left;
    } else if (strcmp(args, "right") == 0) {
        newalign = right;
    } else if (strcmp(args, "center") == 0) {
        newalign = center;
    } else {
        return OverlayError("unknown text alignment");
    }

    // get old value as string
    static char result[8];

    if (align == left) {
        sprintf(result, "left");
    } else if (align == right) {
        sprintf(result, "right");
    } else {
        sprintf(result, "center");
    }

    // save alignment settings
    align = newalign;

    return result;
}

// -----------------------------------------------------------------------------

const char* Overlay::TextOptionBackground(const char* args)
{
    int a1, a2, a3, a4;
    if (sscanf(args, " %d %d %d %d", &a1, &a2, &a3, &a4) != 4) {
        return OverlayError("textoption background command requires 4 arguments");
    }

    if (a1 < 0 || a1 > 255 ||
        a2 < 0 || a2 > 255 ||
        a3 < 0 || a3 > 255 ||
        a4 < 0 || a4 > 255) {
        return OverlayError("background rgba values must be from 0 to 255");
    }

    unsigned char oldr;
    unsigned char oldg;
    unsigned char oldb;
    unsigned char olda;
    GetRGBA(&oldr, &oldg, &oldb, &olda, textbgRGBA);

    SetRGBA(a1, a2, a3, a4, &textbgRGBA);

    // return old values
    static char result[16];
    sprintf(result, "%hhu %hhu %hhu %hhu", oldr, oldg, oldb, olda);
    return result;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoTextOption(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    if (strncmp(args, "align ", 6) == 0)       return TextOptionAlign(args+6);
    if (strncmp(args, "background ", 11) == 0) return TextOptionBackground(args+11);

    return OverlayError("unknown textoption command");
}

// -----------------------------------------------------------------------------

const char* Overlay::DoText(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    // we don't use sscanf to parse the args because we want to allow the
    // text to start with a space
    int namepos = 0;
    int textpos = 0;
    const char* p = args;
    while (*p && *p == ' ') {
        namepos++;
        p++;
    }
    if (namepos > 0 && *p) {
        textpos = namepos;
        while (*p && *p != ' ') {
            textpos++;
            p++;
        }
        if (*p) p++;        // skip past space after clip name
        if (*p) {
            textpos++;
        } else {
            textpos = 0;    // no text supplied
        }
    }
    if (namepos == 0 || textpos == 0) {
        return OverlayError("text command requires 2 arguments");
    }

    std::string name = args + namepos;
    name = name.substr(0, name.find(" "));
    // check if the clip is the current render target
    if (name == targetname) {
        return OverlayError("text clip is current render target");
    }

    // create memory drawing context
    wxMemoryDC dc;
    dc.SetFont(currfont);

    // get the line height and descent (leading is ignored)
    wxString textstr = _("M");
    int textwd, descent, ignored;
    int lineht = 0;
    dc.GetTextExtent(textstr, &textwd, &lineht, &descent, &ignored);

    #ifdef __WXMAC__
        // increase lineht and descent on Mac to avoid clipping descenders
        lineht += extraht;
        descent += extraht;
    #endif

    // count lines
    char *textarg = (char*)args + textpos;
    char *index = textarg;
    int lines = 1;
    while (*index) {
        if (*index == '\n') lines++;
        index++;
    }

    // allocate buffers for line width and start position
    int* width = (int*) malloc(lines * sizeof(int));
    char** line = (char**) malloc(lines * sizeof(char*));

    // find first line
    char *textlines = textarg;
    index = strchr(textlines, '\n');

    // process each line of text to size the bitmap
    int bitmapwd = 0;
    int bitmapht = 0;
    int i = 0;
    do {
        // save pointer to the line start
        line[i] = textlines;
        if (index) {
            // null terminate line
            *index = 0;
        }

        // get the drawn string width
        textstr = wxString(textlines, wxConvUTF8);
        dc.GetTextExtent(textstr, &textwd, &ignored, &ignored, &ignored);

        // save the line width
        width[i] = textwd;

        // update bitmap width
        if (bitmapwd < textwd) bitmapwd = textwd;

        // update the bitmap height
        bitmapht += lineht;

        // next line
        if (index) {
            textlines = index + 1;
            index = strchr(textlines, '\n');
        }
        i++;
    } while (i < lines);

    // delete any existing clip data with the given name
    std::map<std::string,Clip*>::iterator it;
    it = clips.find(name);
    if (it != clips.end()) {
        delete it->second;
        clips.erase(it);
    }

    // create clip data with given name and big enough to enclose text
    Clip* textclip = new Clip(bitmapwd, bitmapht);
    if (textclip == NULL || textclip->cdata == NULL) {
        delete textclip;
        free(width);
        free(line);
        return OverlayError("not enough memory for text clip");
    }

    // get background color
    unsigned char bgr, bgg, bgb, bga;
    GetRGBA(&bgr, &bgg, &bgb, &bga, textbgRGBA);
    wxColour textbgcol(bgr, bgg, bgb, bga);
    wxColour transbgcol(255, 255, 255, 255);

    // get text foreground color
    wxColour textfgcol(r, g, b, a);
    wxColour transfgcol(255 - a, 255 - a, 255 - a, 255);

    // create the bitmap
    wxBitmap bitmap(bitmapwd, bitmapht, 32);

    // select the bitmap
    dc.SelectObject(bitmap);

    // create a rectangle to fill the bitmap
    wxRect rect(0, 0, bitmapwd, bitmapht);
    dc.SetPen(*wxTRANSPARENT_PEN);
    wxBrush brush(textbgcol);

    // if blending use transparent and replace later
    if (bga < 255) brush.SetColour(transbgcol);

    // fill the bitmap
    dc.SetBrush(brush);
    dc.DrawRectangle(rect);
    dc.SetBrush(wxNullBrush);
    dc.SetPen(wxNullPen);

    // set text background color to transparent
    dc.SetBackgroundMode(wxTRANSPARENT);

    // set text foreground color
    if (bga < 255) {
        dc.SetTextForeground(transfgcol);
    } else {
        dc.SetTextForeground(textfgcol);
    }

    // draw each text line
    int xpos = 0;
    int textrow = 0;
    for (i = 0; i < lines; i++) {
        // check if the line is empty
        if (*line[i]) {
            textstr = wxString(line[i], wxConvUTF8);

            // check text alignment
            xpos = 0;
            if (align != left) {
                if (align == right) {
                    xpos = bitmapwd - width[i];
                } else {
                    xpos = (bitmapwd - width[i]) / 2;
                }
            }

            // draw text
            dc.DrawText(textstr, xpos, textrow);
        }

        // next line
        textrow += lineht;
    }

    // deallocate buffers
    free(width);
    free(line);

    // deselect the bitmap
    dc.SelectObject(wxNullBitmap);

    // copy text from top left corner of offscreen image into clip data
    unsigned char* m = textclip->cdata;
    unsigned int bitmapr, bitmapg, bitmapb;

    // get iterator over bitmap data
    wxAlphaPixelData data(bitmap);
    wxAlphaPixelData::Iterator iter(data);

    // check for transparent background
    if (bga < 255) {
        // transparent so look for background pixels to swap
        for (int y = 0; y < bitmapht; y++) {
            wxAlphaPixelData::Iterator rowstart = iter;
            for (int x = 0; x < bitmapwd; x++) {
                // get pixel RGB components
                bitmapr = iter.Red();
                bitmapg = iter.Green();
                bitmapb = iter.Blue();

                if (bitmapr == 255 && bitmapg == 255 && bitmapb == 255) {
                    // background found so replace with transparent pixel
                    *m++ = 0;
                    *m++ = 0;
                    *m++ = 0;
                    *m++ = 0;
                } else {
                    // foreground found so replace with foreground color
                    *m++ = r;
                    *m++ = g;
                    *m++ = b;

                    // set alpha based on grayness
                    *m++ = 255 - bitmapr;
                }

                // pre-increment is faster
                ++iter;
            }
            iter = rowstart;
            iter.OffsetY(data, 1);
        }
    } else {
        // opaque background so just copy pixels
        for (int y = 0; y < bitmapht; y++) {
            wxAlphaPixelData::Iterator rowstart = iter;
            for (int x = 0; x < bitmapwd; x++) {
                *m++ = iter.Red();
                *m++ = iter.Green();
                *m++ = iter.Blue();
                *m++ = 255;

                // pre-increment is faster
                ++iter;
            }
            iter = rowstart;
            iter.OffsetY(data, 1);
        }
    }

    // create named clip for later use by paste, scale, etc
    clips[name] = textclip;

    // return text info
    static char result[48];
    sprintf(result, "%d %d %d", bitmapwd, bitmapht, descent);
    return result;
}

// -----------------------------------------------------------------------------

#ifdef ENABLE_SOUND
const char* Overlay::SoundPlay(const char* args, bool loop)
{
    // check for engine
    if (engine) {
        if (*args == 0) {
            if (loop) {
                return OverlayError("sound loop requires an argument");
            }
            else {
                return OverlayError("sound play requires an argument");
            }
        }

        // check for the optional volume argument
        float v = 1;
        const char* name = args;

        // skip name
        char *scan = (char*)args;
        while (*scan && *scan != ' ') {
            scan++;
        }

        // check if there is a volume argument
        if (*scan) {
            if (sscanf(scan, " %f", &v) == 1) {
                if (v < 0.0 || v > 1.0) {
                    if (loop) {
                        return OverlayError("sound loop volume must be in the range 0 to 1");
                    }
                    else {
                        return OverlayError("sound play volume must be in the range 0 to 1");
                    }
                }
            }

            // null terminate name
            *scan = 0;
        }
    
        // lookup the sound source
        ISoundSource* source = engine->getSoundSource(name, false);
        if (!source) {
            // create and preload the sound source
            source = engine->addSoundSourceFromFile(name, ESM_AUTO_DETECT, true);
            if (!source) {
                // don't throw error just return error message
                return "could not find sound";
            }
        }
    
        // check if the sound exists
        ISound* sound = NULL;
        std::map<std::string,ISound*>::iterator it;
        it = sounds.find(name);
        if (it != sounds.end()) {
            // sound exists so drop it
            sound = it->second;
            if (!sound->isFinished()) {
                sound->stop();
            }
            sound->drop();
            sounds.erase(it);
        }

        // prepare to play the sound (but pause it so we can adjust volume first)
        sound = engine->play2D(source, loop, true);
        if (!sound) {
            // don't throw error just return error message
            return "could not play sound";
        }

        // set the volume and then play
        sound->setVolume(v);
        sound->setIsPaused(false);
        
        // cache the sound
        sounds[name] = sound;
    }

    return NULL;
}
#endif

// -----------------------------------------------------------------------------

#ifdef ENABLE_SOUND
const char* Overlay::SoundStop(const char* args)
{
    // check for engine
    if (engine) {
        // check for argument
        if (*args == 0) {
            // stop all sounds
            engine->stopAllSounds();
        }
        else {
            // skip whitespace
            while (*args == ' ') {
                args++;
            }
    
            // stop named sound
            ISoundSource* source = engine->getSoundSource(args, false);
            if (source) {
                // find the sound
                std::map<std::string,ISound*>::iterator it;
                it = sounds.find(args);
                if (it != sounds.end()) {
                   // stop the sound
                   ISound* sound = it->second;
                   if (!sound->isFinished()) {
                       sound->stop();
                   }
                }
            }
        }
    }

    return NULL;
}
#endif

// -----------------------------------------------------------------------------

#ifdef ENABLE_SOUND
const char* Overlay::SoundState(const char* args)
{
    bool playing = false;
    bool paused = false;

    // check for engine
    if (engine) {
        // check for argument
        if (*args == 0) {
            // see if any sounds are playing
            for (int i = 0; i < engine->getSoundSourceCount(); i++) {
                if (engine->isCurrentlyPlaying(engine->getSoundSource(i))) {
                    playing = true;
                }
            }
        }
        else {
            // skip whitespace
            while (*args == ' ') {
                args++;
            }
    
            // see if named sound is playing
            ISoundSource* source = engine->getSoundSource(args, false);
            if (!source) {
                return "unknown";
            }
            else {
                // find the sound
                std::map<std::string,ISound*>::iterator it;
                it = sounds.find(args);
                if (it != sounds.end()) {
                    ISound* sound = it->second;
                    if (sound->getIsPaused()) {
                        paused = true;
                    }
                    if (engine->isCurrentlyPlaying(source)) {
                        playing = true;
                    }
                }       
            }
        }
    }

    // return status as string
    if (paused && playing) {
        return "paused";
    } else {
        if (playing) {
            return "playing";
        }
        else {
            return "stopped";
        }
    }
}
#endif

// -----------------------------------------------------------------------------

#ifdef ENABLE_SOUND
const char* Overlay::SoundVolume(const char* args)
{
    // check for engine
    if (engine) {
        float v = 1;
        const char* name = args;

        // skip name
        char *scan = (char*)args;
        while (*scan && *scan != ' ') {
            scan++;
        }

        // check if there is a volume argument
        if (*scan) {
            if (sscanf(scan, " %f", &v) == 1) {
                if (v < 0.0 || v > 1.0) {
                    return OverlayError("sound volume must be in the range 0 to 1");
                }
            } else {
                return OverlayError("sound volume command requires two arguments");
            }

            // null terminate name
            *scan = 0;
        }

        // lookup the sound
        ISoundSource* source = engine->getSoundSource(name, false);
        if (source) {
            // set the default volume for the source
            source->setDefaultVolume(v);

            // check if the sound is playing
            std::map<std::string,ISound*>::iterator it;
            it = sounds.find(name);
            if (it != sounds.end()) {
               // set the sound volume
               ISound* sound = it->second;
               if (!sound->isFinished()) {
                   sound->setVolume(v);
               }
            }
        }
    }
    
    return NULL;
}
#endif

// -----------------------------------------------------------------------------

#ifdef ENABLE_SOUND
const char* Overlay::SoundPause(const char* args)
{
    // check for engine
    if (engine) {
        // check for argument
        if (*args == 0) {
            // pause all sounds
            engine->setAllSoundsPaused();
        }
        else {
            // skip whitespace
            while (*args == ' ') {
                args++;
            }
    
            // pause named sound
            ISoundSource* source = engine->getSoundSource(args, false);
            if (source) {
                // find the sound
                std::map<std::string,ISound*>::iterator it;
                it = sounds.find(args);
                if (it != sounds.end()) {
                   // pause the sound
                   ISound* sound = it->second;
                   if (!sound->isFinished()) {
                       sound->setIsPaused();
                   }
                }
            }
        }
    }

    return NULL;
}
#endif

// -----------------------------------------------------------------------------

#ifdef ENABLE_SOUND
const char* Overlay::SoundResume(const char* args)
{
    // check for engine
    if (engine) {
        // check for argument
        if (*args == 0) {
            // resume all paused sounds
            engine->setAllSoundsPaused(false);
        }
        else {
            // skip whitespace
            while (*args == ' ') {
                args++;
            }
    
            // resume named sound
            ISoundSource* source = engine->getSoundSource(args, false);
            if (source) {
                // find the sound
                std::map<std::string,ISound*>::iterator it;
                it = sounds.find(args);
                if (it != sounds.end()) {
                   // resume the sound
                   ISound* sound = it->second;
                   if (!sound->isFinished()) {
                       sound->setIsPaused(false);
                   }
                }
            }
        }
    }

    return NULL;
}
#endif

// -----------------------------------------------------------------------------

const char* Overlay::DoSound(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    #ifdef ENABLE_SOUND
    // check for sound engine query
    if (!*args) {
        if (engine) {
            // sound engine enabled
            return "2";
        }
        else {
            // sound engine failed to start
            return "1";
        }
    }

    // skip whitespace
    while (*args == ' ') {
        args++;
    }

    // check which sound command is specified
    if (strncmp(args, "play ", 5) == 0)    return SoundPlay(args+5, false);
    if (strncmp(args, "loop ", 5) == 0)    return SoundPlay(args+5, true);
    if (strncmp(args, "stop", 4) == 0)     return SoundStop(args+4);
    if (strncmp(args, "state", 5) == 0)    return SoundState(args+5);
    if (strncmp(args, "volume ", 7) == 0)  return SoundVolume(args+7);
    if (strncmp(args, "pause", 5) == 0)    return SoundPause(args+5);
    if (strncmp(args, "resume", 6) == 0)   return SoundResume(args+6);

    return OverlayError("unknown sound command");
    #else
    // if sound support not enabled then just return
    return "0";
    #endif
}

// -----------------------------------------------------------------------------

const char* Overlay::DoTransform(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int a1, a2, a3, a4;
    if (sscanf(args, " %d %d %d %d", &a1, &a2, &a3, &a4) != 4) {
        return OverlayError("transform command requires 4 arguments");
    }

    if (a1 < -1 || a1 > 1 ||
        a2 < -1 || a2 > 1 ||
        a3 < -1 || a3 > 1 ||
        a4 < -1 || a4 > 1) {
        return OverlayError("transform values must be 0, 1 or -1");
    }

    int oldaxx = axx;
    int oldaxy = axy;
    int oldayx = ayx;
    int oldayy = ayy;

    axx = a1;
    axy = a2;
    ayx = a3;
    ayy = a4;
    identity = (axx == 1) && (axy == 0) && (ayx == 0) && (ayy == 1);

    // return old values
    static char result[16];
    sprintf(result, "%d %d %d %d", oldaxx, oldaxy, oldayx, oldayy);
    return result;
}

// -----------------------------------------------------------------------------

bool Overlay::OnlyDrawOverlay()
{
    // only use the overlay
    if (ovpixmap == NULL) return false;

    if (only_draw_overlay) {
        // this flag must only be used for one refresh so reset it immediately
        only_draw_overlay = false;
        return showoverlay && !(numlayers > 1 && tilelayers);
    } else {
        return false;
    }
}

// -----------------------------------------------------------------------------

const char* Overlay::DoUpdate()
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    only_draw_overlay = true;
    viewptr->Refresh(false);
    viewptr->Update();
    // DrawView in wxrender.cpp will call OnlyDrawOverlay (see above)

    #ifdef __WXGTK__
        // needed on Linux to see update immediately
        insideYield = true;
        wxGetApp().Yield(true);
        insideYield = false;
    #endif

    #ifdef ENABLE_SOUND
    // update sound engine (in case threading not supported)
    if (engine) {
        engine->update();
    }
    #endif

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::OverlayError(const char* msg)
{
    static std::string err;
    err = "ERR:";
    err += msg;
    return err.c_str();
}

// -----------------------------------------------------------------------------

const char* Overlay::DoOverlayCommand(const char* cmd)
{
    // determine which command to run
    if (strncmp(cmd, "set ", 4) == 0)          return DoSetPixel(cmd+4);
    if (strncmp(cmd, "get ", 4) == 0)          return DoGetPixel(cmd+4);
    if (strcmp(cmd,  "xy") == 0)               return DoGetXY();
    if (strncmp(cmd, "paste", 5) == 0)         return DoPaste(cmd+5);
    if (strncmp(cmd, "rgba", 4) == 0)          return DoSetRGBA(cmd+4);
    if (strncmp(cmd, "blend", 5) == 0)         return DoBlend(cmd+5);
    if (strncmp(cmd, "fill", 4) == 0)          return DoFill(cmd+4);
    if (strncmp(cmd, "copy", 4) == 0)          return DoCopy(cmd+4);
    if (strncmp(cmd, "optimize", 8) == 0)      return DoOptimize(cmd+8);
    if (strncmp(cmd, "lineoption ", 11) == 0)  return DoLineOption(cmd+11);
    if (strncmp(cmd, "lines", 5) == 0)         return DoLine(cmd+5, false);
    if (strncmp(cmd, "line", 4) == 0)          return DoLine(cmd+4, true);
    if (strncmp(cmd, "ellipse", 7) == 0)       return DoEllipse(cmd+7);
    if (strncmp(cmd, "flood", 5) == 0)         return DoFlood(cmd+5);
    if (strncmp(cmd, "textoption ", 11) == 0)  return DoTextOption(cmd+11);
    if (strncmp(cmd, "text", 4) == 0)          return DoText(cmd+4);
    if (strncmp(cmd, "font", 4) == 0)          return DoFont(cmd+4);
    if (strncmp(cmd, "transform", 9) == 0)     return DoTransform(cmd+9);
    if (strncmp(cmd, "position", 8) == 0)      return DoPosition(cmd+8);
    if (strncmp(cmd, "load", 4) == 0)          return DoLoad(cmd+4);
    if (strncmp(cmd, "save", 4) == 0)          return DoSave(cmd+4);
    if (strncmp(cmd, "scale", 5) == 0)         return DoScale(cmd+5);
    if (strncmp(cmd, "cursor", 6) == 0)        return DoCursor(cmd+6);
    if (strcmp(cmd,  "update") == 0)           return DoUpdate();
    if (strncmp(cmd, "create", 6) == 0)        return DoCreate(cmd+6);
    if (strncmp(cmd, "resize", 6) == 0)        return DoResize(cmd+6);
    if (strncmp(cmd, "cellview ", 9) == 0)     return DoCellView(cmd+9);
    if (strncmp(cmd, "celloption ", 11) == 0)  return DoCellOption(cmd+11);
    if (strncmp(cmd, "camera ", 7) == 0)       return DoCamera(cmd+7);
    if (strncmp(cmd, "theme ", 6) == 0)        return DoTheme(cmd+6);
    if (strncmp(cmd, "target", 6) == 0)        return DoTarget(cmd+6);
    if (strncmp(cmd, "replace ", 8) == 0)      return DoReplace(cmd+8);
    if (strncmp(cmd, "sound", 5) == 0)         return DoSound(cmd+5);
    if (strcmp(cmd,  "updatecells") == 0)      return DoUpdateCells();
    if (strcmp(cmd,  "drawcells") == 0)        return DoDrawCells();
    if (strncmp(cmd, "delete", 6) == 0)        return DoDelete(cmd+6);
    return OverlayError("unknown command");
}

// -----------------------------------------------------------------------------

const char* Overlay::DoOverlayTable(const char* cmd, lua_State* L, int n, int* nresults)
{
    // determine which command to run
    if ((strcmp(cmd, "set")) == 0)      return DoSetTable(L, n, nresults);
    if ((strcmp(cmd, "get")) == 0)      return DoGetTable(L, n, nresults);
    if ((strcmp(cmd, "paste")) == 0)    return DoPasteTable(L, n, nresults);
    if ((strcmp(cmd, "rgba")) == 0)     return DoSetRGBATable(cmd, L, n, nresults);
    if ((strcmp(cmd, "line")) == 0)     return DoLineTable(L, n, true, nresults);
    if ((strcmp(cmd, "lines")) == 0)    return DoLineTable(L, n, false, nresults);
    if ((strcmp(cmd, "fill")) == 0)     return DoFillTable(L, n, nresults);
    return OverlayError("unknown command");
}