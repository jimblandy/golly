// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "ltlalgo.h"
#include "util.h"
#include <string.h>     // for memset and memcpy

// -----------------------------------------------------------------------------

// A 256x256 pixmap is good for OpenGL and matches the size
// used in qlifedraw.cpp and hlifedraw.cpp.
// Note that logpmsize *must* be 8 in iOS 9.x to avoid drawing problems
// in my iPad (probably due to a bug in the OpenGL ES 2 driver).

const int logpmsize = 8;                    // 8=256x256
const int pmsize = (1<<logpmsize);          // pixmap wd and ht, in pixels
const int bpp = 4;                          // bytes per pixel (RGBA)
const int rowoff = (pmsize*bpp);            // row offset, in bytes
const int ibufsize = (pmsize*pmsize*bpp);   // buffer size, in bytes
static unsigned char ipixbuf[ibufsize];     // shared buffer for pixels
static unsigned char *pixbuf = ipixbuf;

// RGBA view of pixbuf
static unsigned int *pixRGBAbuf = (unsigned int *)ipixbuf;

// arrays of RGB colors for each cell state (set by getcolors call)
static unsigned char* cellred;
static unsigned char* cellgreen;
static unsigned char* cellblue;

// alpha values for dead pixels and live pixels (also set by getcolors call)
static unsigned char deada;
static unsigned char livea;

static unsigned int cellRGBA[256];          // cell colors in RGBA format

// -----------------------------------------------------------------------------

// kill all cells in pixbuf

static void killpixels()
{
    // pmag is 1 so pixblit assumes pixbuf contains 4 bytes (RGBA) for each pixel
    if (deada == 0) {
        // dead cells are 100% transparent so we can use fast method
        // (RGB values are irrelevant if alpha is 0)
        memset(pixbuf, 0, sizeof(ipixbuf));
    } else {
        // use slower method
        unsigned int deadRGBA = cellRGBA[0];
        unsigned int *rgbabuf = pixRGBAbuf;

        // fill the first row with the dead pixel state
        for (int i = 0; i < pmsize; i++) {
            *rgbabuf++ = deadRGBA;
        }
        // copy 1st row to remaining rows
        for (int i = rowoff; i < ibufsize; i += rowoff) {
            memcpy(&pixbuf[i], pixbuf, rowoff);
        }
    }
}

// -----------------------------------------------------------------------------

// this is the top-level drawing routine

void ltlalgo::draw(viewport &view, liferender &renderer)
{
    if (population == 0) return;

    if (!renderer.justState()) {
       // get cell colors and alpha values for dead and live pixels
       renderer.getcolors(&cellred, &cellgreen, &cellblue, &deada, &livea);
    
       // create RGBA view
       unsigned char *rgbaptr = (unsigned char *)cellRGBA;
    
       // create dead color
       *rgbaptr++ = cellred[0];
       *rgbaptr++ = cellgreen[0];
       *rgbaptr++ = cellblue[0];
       *rgbaptr++ = deada;
    
       // create live colors
       unsigned int livestates = NumCellStates() - 1;
       for (unsigned int ui = 1; ui <= livestates; ui++) {
           *rgbaptr++ = cellred[ui];
           *rgbaptr++ = cellgreen[ui];
           *rgbaptr++ = cellblue[ui];
           *rgbaptr++ = livea;
       }
    }
    
    int mag, pmag;
    int vieww = view.getwidth();
    int viewh = view.getheight();
    if (view.getmag() > 0) {
        pmag = 1 << view.getmag();
        mag = 0;
    } else {
        pmag = 1;
        mag = -view.getmag();
    }
    
    // get pixel position in view of grid's top left cell
    pair<int,int> ltpxl = view.screenPosOf(gridleft, gridtop, this);
    
    if (renderer.justState() || pmag > 1) {
        if (unbounded) {
            // simply display the entire grid -- ie. no need to use pixbuf
            int x = ltpxl.first;        // already multiplied by pmag
            int y = ltpxl.second;       // ditto
            int wd = gwd * pmag;
            int ht = ght * pmag;
            if (renderer.justState())
               renderer.stateblit(x, y, wd, ht, currgrid) ;
            else
               renderer.pixblit(x, y, wd, ht, currgrid, pmag);
        } else {
            // the universe is bounded so we need to include the outer border
            bigint outerleft = gridleft;
            bigint outertop = gridtop;
            outerleft -= border;
            outertop -= border;
            ltpxl = view.screenPosOf(outerleft, outertop, this);
            int x = ltpxl.first;
            int y = ltpxl.second;
            int wd = outerwd * pmag;
            int ht = outerht * pmag;
            if (renderer.justState())
               renderer.stateblit(x, y, wd, ht, outergrid1) ;
            else
               renderer.pixblit(x, y, wd, ht, outergrid1, pmag);
        }
    } else {
        // pmag is 1 so first fill pixbuf with dead cells
        killpixels();
        if (mag == 0) {
            // divide grid into pmsize * pmsize blocks and draw them at scale 1:1
            for (int row = 0; row < ght; row += pmsize) {
                for (int col = 0; col < gwd; col += pmsize) {
                    
                    // don't go beyond bottom/right edges of grid
                    int jmax = row + pmsize <= ght ? pmsize : pmsize - (row + pmsize - ght);
                    int imax = col + pmsize <= gwd ? pmsize : pmsize - (col + pmsize - gwd);
                
                    // check if this block is visible in view
                    int x = ltpxl.first + col;
                    int y = ltpxl.second + row;
                    if (x >= vieww || y >= viewh || x+imax <= 0 || y+jmax <= 0) {
                        // not visible
                    } else {
                        // get cell at top left corner of this block
                        unsigned char* cellptr = currgrid + row * outerwd + col;
                        
                        // find live cells in this block and store their RGBA data in pixbuf
                        for (int j = 0; j < jmax; j++) {
                            unsigned char* p = cellptr;
                            int pixrow = j * pmsize;
                            for (int i = 0; i < imax; i++) {
                                if (*p > 0) pixRGBAbuf[pixrow + i] = cellRGBA[*p];
                                p++;
                            }
                            cellptr += outerwd;
                        }
                        
                        // draw this block
                        renderer.pixblit(x, y, pmsize, pmsize, pixbuf, 1);
                        killpixels();
                    }
                }
            }
        } else {
            // mag > 0 (actually zoomed out);
            // divide grid into pmsize*(2^mag) * pmsize*(2^mag) blocks, shrinking them down
            // to pmsize * pmsize, and draw all non-zero cells using the state 1 color
            unsigned int state1RGBA = cellRGBA[1];
            
            // check if grid shrinks to 1 pixel
            if ((gwd >> mag) == 0 && (ght >> mag) == 0) {
                pixRGBAbuf[0] = state1RGBA;     // there is at least 1 live cell in grid
                int x = ltpxl.first;
                int y = ltpxl.second;
                renderer.pixblit(x, y, pmsize, pmsize, pixbuf, 1);
                pixRGBAbuf[0] = cellRGBA[0];
                return;
            }
            
            // above check should avoid overflow in blocksize calc, but play safe
            if (mag > 20) mag = 20;
            pmag = 1 << mag;
            int blocksize = pmsize * pmag;
            for (int row = 0; row < ght; row += blocksize) {
                for (int col = 0; col < gwd; col += blocksize) {
                
                    // check if shrunken block is visible in view
                    int x = ltpxl.first + (col >> mag);
                    int y = ltpxl.second + (row >> mag);
                    if (x >= vieww || y >= viewh || x+pmsize <= 0 || y+pmsize <= 0) {
                        // not visible
                    } else {
                        // get cell at top left corner of this big block
                        unsigned char* cellptr = currgrid + row * outerwd + col;
                        
                        // avoid going way beyond bottom/right edges of grid
                        int jmax = row + blocksize <= ght ? blocksize : blocksize - (row + blocksize - ght);
                        int imax = col + blocksize <= gwd ? blocksize : blocksize - (col + blocksize - gwd);
                        
                        for (int j = 0; j < jmax; j += pmag) {
                            unsigned char* p = cellptr;
                            int sqtop = row + j;
                            for (int i = 0; i < imax; i += pmag) {
                            
                                // shrink pmag*pmag cells in grid down to 1 pixel in pixbuf
                                int sqleft = col + i;
                                for (int r = 0; r < pmag; r++) {
                                    if (sqtop + r < ght) {
                                        unsigned char* topleft = p + r * outerwd;
                                        for (int c = 0; c < pmag; c++) {
                                            if (sqleft + c < gwd) {
                                                unsigned char* q = topleft + c;
                                                if (*q > 0) {
                                                    pixRGBAbuf[(j >> mag) * pmsize + (i >> mag)] = state1RGBA;
                                                    // no need to keep looking in this square
                                                    goto found1;
                                                }
                                            }
                                        }
                                    }
                                }
                                found1:
                                p += pmag;
                                
                            }
                            cellptr += outerwd * pmag;
                        }
                        
                        // draw the shrunken block
                        renderer.pixblit(x, y, pmsize, pmsize, pixbuf, 1);
                        killpixels();
                    }
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::findedges(bigint *ptop, bigint *pleft, bigint *pbottom, bigint *pright)
{
    if (population == 0) {
        // return impossible edges to indicate an empty pattern;
        // not really a problem because caller should check first
        *ptop = 1;
        *pleft = 1;
        *pbottom = 0;
        *pright = 0;
        return;
    }
    
    // the code in ltlalgo.cpp maintains a boundary of live cells in
    // minx,miny,maxx,maxy but it might not be the minimal boundary
    // (eg. if user deleted some live cells)

    // find the top edge (miny)
    for (int row = miny; row <= maxy; row++) {
        unsigned char* cellptr = currgrid + row * outerwd + minx;
        for (int col = minx; col <= maxx; col++) {
            if (*cellptr > 0) {
                miny = row;
                goto found_top;
            }
            cellptr++;
        }
    }
    // should never get here if population > 0
    lifefatal("Bug detected in ltlalgo::findedges!");
    
    found_top:
    
    // find the bottom edge (maxy)
    for (int row = maxy; row >= miny; row--) {
        unsigned char* cellptr = currgrid + row * outerwd + minx;
        for (int col = minx; col <= maxx; col++) {
            if (*cellptr > 0) {
                maxy = row;
                goto found_bottom;
            }
            cellptr++;
        }
    }
    
    found_bottom:
    
    // find the left edge (minx)
    for (int col = minx; col <= maxx; col++) {
        unsigned char* cellptr = currgrid + miny * outerwd + col;
        for (int row = miny; row <= maxy; row++) {
            if (*cellptr > 0) {
                minx = col;
                goto found_left;
            }
            cellptr += outerwd;
        }
    }
    
    found_left:
    
    // find the right edge (maxx)
    for (int col = maxx; col >= minx; col--) {
        unsigned char* cellptr = currgrid + miny * outerwd + col;
        for (int row = miny; row <= maxy; row++) {
            if (*cellptr > 0) {
                maxx = col;
                goto found_right;
            }
            cellptr += outerwd;
        }
    }
    
    found_right:

    // set pattern edges (in cell coordinates)
    *ptop = int(miny + gtop);
    *pleft = int(minx + gleft);
    *pbottom = int(maxy + gtop);
    *pright = int(maxx + gleft);
}

// -----------------------------------------------------------------------------

void ltlalgo::fit(viewport &view, int force)
{
    if (population == 0) {
        view.center();
        view.setmag(MAX_MAG);
        return;
    }
    
    bigint top, left, bottom, right;
    findedges(&top, &left, &bottom, &right);

    if (!force) {
        // if all four of the above dimensions are in the viewport, don't change
        if (view.contains(left, top) && view.contains(right, bottom))
            return;
    }

    bigint midx = right;
    midx -= left;
    midx += bigint::one;
    midx.div2();
    midx += left;
    
    bigint midy = bottom;
    midy -= top;
    midy += bigint::one;
    midy.div2();
    midy += top;
    
    int mag = MAX_MAG;
    for (;;) {
        view.setpositionmag(midx, midy, mag);
        if (view.contains(left, top) && view.contains(right, bottom))
            break;
        mag--;
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::lowerRightPixel(bigint &x, bigint &y, int mag)
{
    if (mag >= 0) return;
    x >>= -mag;
    x <<= -mag;
    y -= 1;
    y >>= -mag;
    y <<= -mag;
    y += 1;
}
