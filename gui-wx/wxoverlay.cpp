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
#include "wxoverlay.h"

#include <vector>           // for std::vector
#include <cstdio>           // for FILE*, etc
#include <math.h>           // for sin, cos, log, sqrt and atn2
#include <stdlib.h>         // for malloc, free

// -----------------------------------------------------------------------------

// some useful macros:

#ifdef __WXMSW__
    #define round(x) int( (x) < 0 ? (x)-0.5 : (x)+0.5 )
    #define remainder(n,d) ( (n) - round((n)/(d)) * (d) )
#endif

#define PixelInOverlay(x,y) \
    (x >= 0 && x < wd && \
     y >= 0 && y < ht)

#define RectOutsideOverlay(x,y,w,h) \
    (x >= wd || x + w <= 0 || \
     y >= ht || y + h <= 0)

#define RectInsideOverlay(x,y,w,h) \
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

// -----------------------------------------------------------------------------

Overlay::Overlay()
{
    pixmap = NULL;
    cellview = NULL;
    zoomview = NULL;
    starx = NULL;
    stary = NULL;
    starz = NULL;
}

// -----------------------------------------------------------------------------

Overlay::~Overlay()
{
    DeleteOverlay();
}

// -----------------------------------------------------------------------------

void Overlay::DeleteOverlay()
{
    if (pixmap) {
        free(pixmap);
        pixmap = NULL;
    }

    std::map<std::string,Clip*>::iterator it;
    for (it = clips.begin(); it != clips.end(); ++it) {
        delete it->second;
    }
    clips.clear();

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
    int h, w, v = 0;
    int skip;
    lifealgo *algo = currlayer->algo;

    int rightx = cellx + cellwd;
    int bottomy = celly + cellht;

    for (h = celly; h < bottomy; h++) {
        w = cellx;
        while (w < rightx) {
            skip = algo->nextcell(w, h, v);
            if (skip >= 0) {
                skip += w;
                if (skip >= rightx) skip = rightx;
                while (w < skip) {
                    // new cells are dead
                    state = *cellviewptr;
                    if (state) {
                        if (state >= aliveStart) {
                            // cell just died
                            *cellviewptr = deadStart;
                        }
                        else {
                            if (state > deadEnd) {
                                // cell decaying
                                *cellviewptr = state - 1;
                            }
                        }
                    }
                    cellviewptr++;

                    // next dead cell
                    w++;
                }

                // cell is alive
                if (w < rightx) {
                    state = *cellviewptr;
                    if (state >= aliveStart) {
                        // check for max length
                        if (state < aliveEnd) {
                            // cell living
                            *cellviewptr = state + 1;
                        }
                    }
                    else {
                        // cell just born
                        *cellviewptr = aliveStart;
                    }
                    cellviewptr++;

                    // next cell
                    w++;
                }
            }
            else {
                // dead to end of row
                while (w < rightx) {
                    // new cells are dead
                    state = *cellviewptr;
                    if (state) {
                        if (state >= aliveStart) {
                            // cell just died
                            *cellviewptr = deadStart;
                        }
                        else {
                            if (state > deadEnd) {
                                // cell decaying
                                *cellviewptr = state - 1;
                            }
                        }
                    }
                    cellviewptr++;

                    // next dead cell
                    w++;
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------

void Overlay::RefreshCellView()
{
    bigint top, left, bottom, right;
    int leftx, rightx, topy, bottomy;
    int h, w, v = 0;
    int skip;
    lifealgo *algo = currlayer->algo;

    // find pattern bounding box
    algo->findedges(&top, &left, &bottom, &right);
    leftx = left.toint();
    rightx = right.toint();
    topy = top.toint();
    bottomy = bottom.toint();

    // clip to cell view
    if (leftx < cellx) {
        leftx = cellx;
    }
    if (rightx >= cellx + cellwd) {
        rightx = cellx + cellwd - 1;
    }
    if (bottomy >= celly + cellht) {
        bottomy = celly + cellht - 1;
    }
    if (topy < celly) {
        topy = celly;
    }

    // clear the cell view
    memset(cellview, 0, cellwd * cellht * sizeof(*cellview));

    // copy live cells into the cell view
    for (h = topy; h <= bottomy; h++) {
        for (w = leftx; w <= rightx; w++) {
            skip = algo->nextcell(w, h, v);
            if (skip >= 0) {
                // live cell found
                w += skip;
                if (w <= rightx) {
                    cellview[(h - celly) * cellwd + w - cellx] = v;
                }
            }
            else {
                // end of row
                w = rightx;
            }
        }
    }
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
}

// -----------------------------------------------------------------------------

void Overlay::GetThemeColors(double brightness)
{
    unsigned char *rgb = (unsigned char *)cellRGBA;
    double weight;
    unsigned char alpha;

    // cell born color
    unsigned char aliveStartR, aliveStartG, aliveStartB;

    // cell alive long time color
    unsigned char aliveEndR, aliveEndG, aliveEndB;

    // cell just died color
    unsigned char deadStartR, deadStartG, deadStartB;

    // cell dead long time color
    unsigned char deadEndR, deadEndG, deadEndB;

    // cell never occupied color
    unsigned char unoccupiedR, unoccupiedG, unoccupiedB;

    // get the color rgb components
    GetRGBA(&aliveStartR, &aliveStartG, &aliveStartB, &alpha, aliveStartRGBA);
    GetRGBA(&aliveEndR, &aliveEndG, &aliveEndB, &alpha, aliveEndRGBA);
    GetRGBA(&deadStartR, &deadStartG, &deadStartB, &alpha, deadStartRGBA);
    GetRGBA(&deadEndR, &deadEndG, &deadEndB, &alpha, deadEndRGBA);
    GetRGBA(&unoccupiedR, &unoccupiedG, &unoccupiedB, &alpha, unoccupiedRGBA);

    // set never occupied cell color
    *rgb++ = unoccupiedR;
    *rgb++ = unoccupiedG;
    *rgb++ = unoccupiedB;
    *rgb++ = 255; // opaque

    // set decaying colors
    for (int i = deadEnd; i <= deadStart; i++) {
        weight = 1 - ((double)(i - deadEnd) / (deadStart - deadEnd));
        *rgb++ = deadStartR * (1 - weight) + deadEndR * weight;
        *rgb++ = deadStartG * (1 - weight) + deadEndG * weight;
        *rgb++ = deadStartB * (1 - weight) + deadEndB * weight;
        *rgb++ = 255; // opaque
    }

    // set living colors
    for (int i = aliveStart; i <= aliveEnd; i++) {
        weight = 1 - ((double)(i - aliveStart) / (aliveEnd - aliveStart));
        *rgb++ = (aliveStartR * weight + aliveEndR * (1 - weight)) * brightness;
        *rgb++ = (aliveStartG * weight + aliveEndG * (1 - weight)) * brightness;
        *rgb++ = (aliveStartB * weight + aliveEndB * (1 - weight)) * brightness;
        *rgb++ = 255; // opaque
    }
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
            state = row2[w + halfstep];
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

    // get the border color
    unsigned char borderr = borderrgb->Red();
    unsigned char borderg = borderrgb->Green();
    unsigned char borderb = borderrgb->Blue();
    unsigned char bordera = 255;    // opaque
    unsigned int borderRGBA;
    SetRGBA(borderr, borderg, borderb, bordera, &borderRGBA);

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

    // get the border color
    unsigned char borderr = borderrgb->Red();
    unsigned char borderg = borderrgb->Green();
    unsigned char borderb = borderrgb->Blue();
    unsigned char bordera = 255;    // opaque
    unsigned int borderRGBA;
    SetRGBA(borderr, borderg, borderb, bordera, &borderRGBA);

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
    int disable = 0;
    unsigned char alpha = 255;  // opaque

    if (sscanf(args, " %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
        &asr, &asg, &asb, &aer, &aeg, &aeb, &dsr, &dsg, &dsb, &der, &deg, &deb, &ur, &ug, &ub) != 15) {
        // check for single argument version
        if (sscanf(args, " %d", &disable) != 1) {
            return OverlayError("theme command requires single argument -1 or 15 rgb components");
        }
        else {
            if (disable != -1) {
                return OverlayError("theme command single argument must be -1");
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
    }

    // save the new values
    if (disable == -1) {
        theme = false;
    }
    else {
        theme = true;
        SetRGBA(asr, asg, asb, alpha, &aliveStartRGBA);
        SetRGBA(aer, aeg, aeb, alpha, &aliveEndRGBA);
        SetRGBA(dsr, dsg, dsb, alpha, &deadStartRGBA);
        SetRGBA(der, deg, deb, alpha, &deadEndRGBA);
        SetRGBA(ur, ug, ub, alpha, &unoccupiedRGBA);
    }

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoResize(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    // don't set wd and ht until we've checked the args are valid
    int w, h;
    if (sscanf(args, " %d %d", &w, &h) != 2) {
        return OverlayError("resize command requires 2 arguments");
    }

    if (w <= 0) return OverlayError("width of overlay must be > 0");
    if (h <= 0) return OverlayError("height of overlay must be > 0");

    // given width and height are ok
    wd = w;
    ht = h;

    // free the previous pixmap
    free(pixmap);

    // create the new pixmap
    pixmap = (unsigned char*) calloc(wd * ht * 4, sizeof(*pixmap));
    if (pixmap == NULL) return OverlayError("not enough memory to resize overlay");

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoCreate(const char* args)
{
    // don't set wd and ht until we've checked the args are valid
    int w, h;
    if (sscanf(args, " %d %d", &w, &h) != 2) {
        return OverlayError("create command requires 2 arguments");
    }

    if (w <= 0) return OverlayError("width of overlay must be > 0");
    if (h <= 0) return OverlayError("height of overlay must be > 0");

    // given width and height are ok
    wd = w;
    ht = h;

    // delete any existing pixmap
    DeleteOverlay();

    // use calloc so all pixels will be 100% transparent (alpha = 0)
    pixmap = (unsigned char*) calloc(wd * ht * 4, sizeof(*pixmap));
    if (pixmap == NULL) return OverlayError("not enough memory to create overlay");

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
    #else
        currfont.SetPointSize(fontsize);
    #endif

    // default text alignment
    align = left;

    // default text background
    textbgRGBA = 0;

    // make sure the Show Overlay option is ticked
    if (!showoverlay) mainptr->ToggleOverlay();

    return NULL;
}

// -----------------------------------------------------------------------------

bool Overlay::PointInOverlay(int vx, int vy, int* ox, int* oy)
{
    if (pixmap == NULL) return false;

    int viewwd, viewht;
    viewptr->GetClientSize(&viewwd, &viewht);
    if (viewwd <= 0 || viewht <= 0) return false;

    int x = 0;
    int y = 0;
    switch (pos) {
        case topleft:
            break;
        case topright:
            x = viewwd - wd;
            break;
        case bottomright:
            x = viewwd - wd;
            y = viewht - ht;
            break;
        case bottomleft:
            y = viewht - ht;
            break;
        case middle:
            x = (viewwd - wd) / 2;
            y = (viewht - ht) / 2;
            break;
    }

    if (vx < x) return false;
    if (vy < y) return false;
    if (vx >= x + wd) return false;
    if (vy >= y + ht) return false;

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

const char* Overlay::DoSetPixel(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int x, y;
    if (sscanf(args, "%d %d", &x, &y) != 2) {
        return OverlayError("set command requires 2 arguments");
    }

    // ignore pixel if outside pixmap edges
    if (PixelInOverlay(x, y)) DrawPixel(x, y);

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
    if (!PixelInOverlay(x, y)) return "";

    unsigned char* p = pixmap + y*wd*4 + x*4;
    static char result[16];
    sprintf(result, "%hhu %hhu %hhu %hhu", p[0], p[1], p[2], p[3]);
    return result;
}

// -----------------------------------------------------------------------------

bool Overlay::TransparentPixel(int x, int y)
{
    if (pixmap == NULL) return false;

    // check if x,y is outside pixmap
    if (!PixelInOverlay(x, y)) return false;

    unsigned char* p = pixmap + y*wd*4 + x*4;

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

const char* Overlay::DoLine(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int x1, y1, x2, y2;
    if (sscanf(args, " %d %d %d %d", &x1, &y1, &x2, &y2) != 4) {
        return OverlayError("line command requires 4 arguments");
    }

    if (x1 == x2 && y1 == y2) {
        if (PixelInOverlay(x1, y1)) DrawPixel(x1, y1);
        return NULL;
    }

    // draw a line of pixels from x1,y1 to x2,y2 using Bresenham's algorithm
    int dx = x2 - x1;
    int ax = abs(dx) * 2;
    int sx = dx < 0 ? -1 : 1;

    int dy = y2 - y1;
    int ay = abs(dy) * 2;
    int sy = dy < 0 ? -1 : 1;

    if (ax > ay) {
        int d = ay - (ax / 2);
        while (x1 != x2) {
            if (PixelInOverlay(x1, y1)) DrawPixel(x1, y1);
            if (d >= 0) {
                y1 = y1 + sy;
                d = d - ax;
            }
            x1 = x1 + sx;
            d = d + ay;
        }
    } else {
        int d = ax - (ay / 2);
        while (y1 != y2) {
            if (PixelInOverlay(x1, y1)) DrawPixel(x1, y1);
            if (d >= 0) {
                x1 = x1 + sx;
                d = d - ay;
            }
            y1 = y1 + sy;
            d = d + ax;
        }
    }
    if (PixelInOverlay(x2, y2)) DrawPixel(x2, y2);

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

const char* Overlay::DoFill(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    if (args[0] == ' ') {
        int x, y, w, h;
        if (sscanf(args, " %d %d %d %d", &x, &y, &w, &h) != 4) {
            return OverlayError("fill command requires 0 or 4 arguments");
        }

        // treat non-positive w/h as inset from overlay's width/height
        if (w <= 0) w = wd + w;
        if (h <= 0) h = ht + h;
        if (w <= 0) return OverlayError("fill width must be > 0");
        if (h <= 0) return OverlayError("fill height must be > 0");

        // ignore rect if completely outside pixmap edges
        if (RectOutsideOverlay(x, y, w, h)) return NULL;

        // clip any part of rect outside pixmap edges
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

    } else {
        // fill entire pixmap with current RGBA values
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
    if (RectInsideOverlay(x, y, w, h)) {
        // use malloc to allocate clip memory
        use_calloc = false;
    } else {
        // use calloc so parts outside overlay will be transparent
        use_calloc = true;
    }

    Clip* newclip = new Clip(w, h, use_calloc);
    if (newclip == NULL || newclip->cdata == NULL) {
        delete newclip;
        return OverlayError("not enough memory to copy pixels");
    }

    if (use_calloc) {
        if (RectOutsideOverlay(x, y, w, h)) {
            // clip rect is completely outside overlay so no need to copy
            // overlay pixels (clip pixels are all transparent)
        } else {
            // calculate offsets in clip data and bytes per row
            int clipx = x >= 0 ? 0 : -x;
            int clipy = y >= 0 ? 0 : -y;
            int cliprowbytes = w * 4;
            
            // set x,y,w,h to intersection with overlay
            int xmax = x + w - 1;
            int ymax = y + h - 1;
            if (x < 0) x = 0;
            if (y < 0) y = 0;
            if (xmax >= wd) xmax = wd - 1;
            if (ymax >= ht) ymax = ht - 1;
            w = xmax - x + 1;
            h = ymax - y + 1;
            
            // copy intersection rect from overlay into corresponding area of clip data
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
        // given rectangle is within overlay so fill newclip->cdata with
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

    clips[name] = newclip;      // create named clip for later use by DoPaste

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoPaste(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int x, y;
    int namepos;
    char dummy;
    if (sscanf(args, " %d %d %n%c", &x, &y, &namepos, &dummy) != 3) {
        // note that %n is not included in the count
        return OverlayError("paste command requires 3 arguments");
    }

    std::string name = args + namepos;
    std::map<std::string,Clip*>::iterator it;
    it = clips.find(name);
    if (it == clips.end()) {
        static std::string msg;
        msg = "unknown paste name (";
        msg += name;
        msg += ")";
        return OverlayError(msg.c_str());
    }

    Clip* clipptr = it->second;
    int w = clipptr->cwd;
    int h = clipptr->cht;

    // do nothing if rect is completely outside overlay
    if (RectOutsideOverlay(x, y, w, h)) return NULL;

    if (x == 0 && y == 0 && w == wd && h == ht && !alphablend && identity) {
        // clip and overlay are the same size and there's no alpha blending
        // or transforming so do a fast paste using a single memcpy call
        memcpy(pixmap, clipptr->cdata, w * h * 4);

    } else if (RectInsideOverlay(x, y, w, h) && !alphablend && identity) {
        // rect is within overlay and there's no alpha blending or transforming
        // so use memcpy to paste rows of pixels from clip data into pixmap
        unsigned char* data = clipptr->cdata;
        int rowbytes = wd * 4;
        int wbytes = w * 4;
        unsigned char* p = pixmap + y*rowbytes + x*4;
        for (int j = 0; j < h; j++) {
            memcpy(p, data, wbytes);
            p += rowbytes;
            data += wbytes;
        }

    } else {
        // save current RGBA values and paste pixel by pixel, clipping any
        // outside the overlay, and possibly doing alpha blending and/or an
        // affine transformation
        unsigned char saver = r;
        unsigned char saveg = g;
        unsigned char saveb = b;
        unsigned char savea = a;

        unsigned char* data = clipptr->cdata;

        if (identity) {
            if (RectInsideOverlay(x, y, w, h)) {
                // clip is inside overlay so no need for bounds checking
                unsigned char* p = pixmap + y * wd * 4 + x * 4;
                for (int j = 0; j < h; j++) {
                    for (int i = 0; i < w; i++) {
                        r = *data++;
                        g = *data++;
                        b = *data++;
                        a = *data++;

                        // draw the pixel
                        if (a < 255) {
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
                        } else {
                            // pixel is opaque so copy it
                            *p++ = r;
                            *p++ = g;
                            *p++ = b;
                            *p++ = a;
                        }
                    }
                    p += (wd - w) * 4;
                }
            }
            else {
                unsigned char* p;
                for (int j = 0; j < h; j++) {
                    // check if row is in overlay
                    if (PixelInOverlay(0, y)) {
                        p = pixmap + y * wd * 4 + x * 4;
                        for (int i = 0; i < w; i++) {
                            if (PixelInOverlay(x, y)) {
                                r = *data++;
                                g = *data++;
                                b = *data++;
                                a = *data++;

                                // draw the pixel
                                if (a < 255) {
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
                                } else {
                                    // pixel is opaque so copy it
                                    *p++ = r;
                                    *p++ = g;
                                    *p++ = b;
                                    *p++ = a;
                                }
                            } else {
                                // pixel is outside overlay
                                data += 4;
                                p += 4;
                            }
                            x++;
                        }
                        x -= w;
                    } else {
                        // row is outside overlay
                        data += 4 * w;
                    }
                    y++;
                }
            }
        } else {
            // do an affine transformation
            int x0 = x - (x * axx + y * axy);
            int y0 = y - (x * ayx + y * ayy);
            for (int j = 0; j < h; j++) {
                for (int i = 0; i < w; i++) {
                    r = *data++;
                    g = *data++;
                    b = *data++;
                    a = *data++;
                    int newx = x0 + x * axx + y * axy;
                    int newy = y0 + x * ayx + y * ayy;
                    if (PixelInOverlay(newx, newy)) DrawPixel(newx, newy);
                    x++;
                }
                y++;
                x -= w;
            }
        }

        // restore saved RGBA values
        r = saver;
        g = saveg;
        b = saveb;
        a = savea;
    }

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoFreeClip(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int namepos;
    char dummy;
    if (sscanf(args, " %n%c", &namepos, &dummy) != 1) {
        // note that %n is not included in the count
        return OverlayError("freeclip command requires 1 argument");
    }

    std::string name = args + namepos;
    std::map<std::string,Clip*>::iterator it;
    it = clips.find(name);
    if (it == clips.end()) {
        static std::string msg;
        msg = "unknown freeclip name (";
        msg += name;
        msg += ")";
        return OverlayError(msg.c_str());
    } else {
        // delete the clip
        delete it->second;
        clips.erase(it);
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
    if (RectOutsideOverlay(x, y, imgwd, imght)) {
        // do nothing if image rect is completely outside overlay,
        // but we still return the image dimensions so users can do things
        // like center the image within the overlay
    } else {
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
                if (PixelInOverlay(x, y)) DrawPixel(x, y);
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

const char* Overlay::DoFlood(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int x, y;
    if (sscanf(args, " %d %d", &x, &y) != 2) {
        return OverlayError("flood command requires 2 arguments");
    }

    // // check if x,y is outside pixmap
    if (!PixelInOverlay(x, y)) return NULL;

    int rowbytes = wd * 4;
    unsigned char* oldpxl = pixmap + y*rowbytes + x*4;
    unsigned char oldr = oldpxl[0];
    unsigned char oldg = oldpxl[1];
    unsigned char oldb = oldpxl[2];
    unsigned char olda = oldpxl[3];

    // do nothing if color of given pixel matches current RGBA values
    if (oldr == r && oldg == g && oldb == b && olda == a) return NULL;

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
            currfont.SetPointSize(ptsize);

        } else if (strcmp(newname, "default-bold") == 0) {
            currfont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
            currfont.SetPointSize(ptsize);
            currfont.SetWeight(wxFONTWEIGHT_BOLD);

        } else if (strcmp(newname, "default-italic") == 0) {
            currfont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
            currfont.SetPointSize(ptsize);
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
    if (pixmap == NULL) return OverlayError(no_overlay);

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

    // create memory drawing context
    wxMemoryDC dc;
    dc.SetFont(currfont);

    // get the line height
    wxString textstr = _("M");
    int textwd, textht, descent, leading;
    int lineht = 0;
    dc.GetTextExtent(textstr, &textwd, &lineht, &descent, &leading);

    // count lines
    char *textarg = (char*)args + textpos;
    char *index = textarg;
    int lines = 1;
    while (*index) {
        if (*index == '\n') lines++;
        index++;
    }

    // allocate line width and start position buffers
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
        dc.GetTextExtent(textstr, &textwd, &textht, &descent, &leading);

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

    // create named clip for later use by DoPaste
    clips[name] = textclip;

    // return text info
    static char result[48];
    sprintf(result, "%d %d %d", bitmapwd, bitmapht, descent);
    return result;
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
    if (pixmap == NULL) return false;

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
    if (strncmp(cmd, "set ", 4) == 0)         return DoSetPixel(cmd+4);
    if (strncmp(cmd, "get ", 4) == 0)         return DoGetPixel(cmd+4);
    if (strcmp(cmd,  "xy") == 0)              return DoGetXY();
    if (strncmp(cmd, "line", 4) == 0)         return DoLine(cmd+4);
    if (strncmp(cmd, "rgba", 4) == 0)         return DoSetRGBA(cmd+4);
    if (strncmp(cmd, "fill", 4) == 0)         return DoFill(cmd+4);
    if (strncmp(cmd, "copy", 4) == 0)         return DoCopy(cmd+4);
    if (strncmp(cmd, "paste", 5) == 0)        return DoPaste(cmd+5);
    if (strncmp(cmd, "load", 4) == 0)         return DoLoad(cmd+4);
    if (strncmp(cmd, "save", 4) == 0)         return DoSave(cmd+4);
    if (strncmp(cmd, "flood", 5) == 0)        return DoFlood(cmd+5);
    if (strncmp(cmd, "blend", 5) == 0)        return DoBlend(cmd+5);
    if (strncmp(cmd, "textoption ", 11) == 0) return DoTextOption(cmd+11);
    if (strncmp(cmd, "text", 4) == 0)         return DoText(cmd+4);
    if (strncmp(cmd, "font", 4) == 0)         return DoFont(cmd+4);
    if (strncmp(cmd, "freeclip", 8) == 0)     return DoFreeClip(cmd+8);
    if (strncmp(cmd, "transform", 9) == 0)    return DoTransform(cmd+9);
    if (strncmp(cmd, "position", 8) == 0)     return DoPosition(cmd+8);
    if (strncmp(cmd, "cursor", 6) == 0)       return DoCursor(cmd+6);
    if (strcmp(cmd,  "update") == 0)          return DoUpdate();
    if (strncmp(cmd, "create", 6) == 0)       return DoCreate(cmd+6);
    if (strncmp(cmd, "resize", 6) == 0)       return DoResize(cmd+6);
    if (strncmp(cmd, "cellview ", 9) == 0)    return DoCellView(cmd+9);
    if (strncmp(cmd, "camera ", 7) == 0)      return DoCamera(cmd+7);
    if (strncmp(cmd, "celloption ", 11) == 0) return DoCellOption(cmd+11);
    if (strncmp(cmd, "theme ", 6) == 0)       return DoTheme(cmd+6);
    if (strcmp(cmd,  "updatecells") == 0)     return DoUpdateCells();
    if (strcmp(cmd,  "drawcells") == 0)       return DoDrawCells();
    if (strcmp(cmd,  "delete") == 0) {
        DeleteOverlay();
        return NULL;
    }
    return OverlayError("unknown command");
}
