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

// Implementation code for the "Larger than Life" family of rules.
// See http://psoup.math.wisc.edu/mcell/rullex_lgtl.html for details.

#include "ltlalgo.h"
#include "liferules.h"
#include "util.h"
#include <stdlib.h>     // for malloc, free, etc
#include <limits.h>     // for INT_MIN and INT_MAX
#include <string.h>     // for memset and strchr

// -----------------------------------------------------------------------------

// set default rule to match Life
static const char *DEFAULTRULE = "R1,C0,M0,S2..3,B3..3,NM";

#define MAXRANGE 50
#define DEFAULTSIZE 400     // must be >= 2*MAXRANGE

// maximum number of columns in a cell's neighborhood (used in fast_Moore)
static const int MAXNCOLS = 2 * MAXRANGE + 1;

// maximum number of cells in grid must be < 2^31 so population can't overflow
#define MAXCELLS 100000000.0

// -----------------------------------------------------------------------------

// Create a new empty universe.

ltlalgo::ltlalgo()
{
    // allocate currgrid and nextgrid using the default grid size
    gwd = DEFAULTSIZE;
    ght = DEFAULTSIZE;
    create_grids();
    generation = 0;
    increment = 1;
    show_warning = true;
}

// -----------------------------------------------------------------------------

// Destroy the universe.

ltlalgo::~ltlalgo()
{
    free(currgrid);
    free(nextgrid);
    if (colcounts) free(colcounts);
}

// -----------------------------------------------------------------------------

void ltlalgo::create_grids()
{
    gridbytes = gwd * ght;
    currgrid = (unsigned char*) calloc(gridbytes, sizeof(*currgrid));
    nextgrid = (unsigned char*) calloc(gridbytes, sizeof(*nextgrid));
    if (currgrid == NULL || nextgrid == NULL) lifefatal("Not enough memory for LtL grids!");

    // set grid coordinates of cell at bottom right corner of grid
    gwdm1 = gwd - 1;
    ghtm1 = ght - 1;

    // set cell coordinates of grid edges (middle of grid is 0,0)
    gtop = -int(ght / 2);
    gleft = -int(gwd / 2);
    gbottom = gtop + ghtm1;
    gright = gleft + gwdm1;
    
    // set bigint versions of grid edges (used by GUI code)
    gridtop = gtop;
    gridleft = gleft;
    gridbottom = gbottom;
    gridright = gright;
    
    // the universe is empty
    population = 0;
    
    // init boundaries so next birth will change them
    empty_boundaries();
    
    colcounts = (int*) malloc(gridbytes*4);
    // if NULL then use fast_Moore, otherwise faster_Moore
}

// -----------------------------------------------------------------------------

void ltlalgo::empty_boundaries()
{
    minx = INT_MAX;
    miny = INT_MAX;
    maxx = INT_MIN;
    maxy = INT_MIN;
}

// -----------------------------------------------------------------------------

void ltlalgo::clearall()
{
    lifefatal("clearall is not implemented");
}

// -----------------------------------------------------------------------------

int ltlalgo::NumCellStates()
{
    return maxCellStates;
}

// -----------------------------------------------------------------------------

void ltlalgo::endofpattern()
{
    show_warning = true;
}

// -----------------------------------------------------------------------------

const char* ltlalgo::resize_grids(int up, int down, int left, int right)
{
    // try to resize universe by given amounts (possibly -ve)
    int newwd = gwd + left + right;
    int newht = ght + up + down;
    if ((float)newwd * (float)newht > MAXCELLS) {
        return "Sorry, but the universe can't be expanded that far.";
    }

    // check if new grid edges would be outside editing limits
    int newtop = gtop - up;
    int newleft = gleft - left;
    int newbottom = newtop + newht - 1;
    int newright = newleft + newwd - 1;
    if (newtop <   -1000000000 || newleft < -1000000000 ||
        newbottom > 1000000000 || newright > 1000000000) {
        return "Sorry, but the grid edges can't be outside the editing limits.";
    }
    
    int newbytes = newwd * newht;
    unsigned char* newcurr = (unsigned char*) calloc(newbytes, sizeof(*newcurr));
    unsigned char* newnext = (unsigned char*) calloc(newbytes, sizeof(*newnext));
    if (newcurr == NULL || newnext == NULL) {
        if (newcurr) free(newcurr);
        if (newnext) free(newnext);
        return "Not enough memory to resize universe!";
    }
    
    // resize succeeded so copy pattern from currgrid into newcurr
    if (population > 0) {
        unsigned char* src = currgrid + miny * gwd + minx;
        unsigned char* dest = newcurr + (miny + up) * newwd + minx + left;
        int xbytes = maxx - minx + 1;
        for (int row = miny; row <= maxy; row++) {
            memcpy(dest, src, xbytes);
            src += gwd;
            dest += newwd;
        }
        // shift boundaries
        minx += left;
        maxx += left;
        miny += up;
        maxy += up;
    }
    
    free(currgrid);
    free(nextgrid);
    currgrid = newcurr;
    nextgrid = newnext;

    gwd = newwd;
    ght = newht;
    gridbytes = newbytes;
    
    // set grid coordinates of cell at bottom right corner of grid
    gwdm1 = gwd - 1;
    ghtm1 = ght - 1;

    // adjust cell coordinates of grid edges
    gtop -= up;
    gleft -= left;
    gbottom = gtop + ghtm1;
    gright = gleft + gwdm1;
    
    // set bigint versions of grid edges (used by GUI code)
    gridtop = gtop;
    gridleft = gleft;
    gridbottom = gbottom;
    gridright = gright;

    if (colcounts) free(colcounts);
    colcounts = (int*) malloc(gridbytes*4);
    // if NULL then use fast_Moore, otherwise faster_Moore

    return NULL;    // success
}

// -----------------------------------------------------------------------------

// Set the cell at the given location to the given state.

int ltlalgo::setcell(int x, int y, int newstate)
{
    if (newstate < 0 || newstate >= maxCellStates) return -1;
    
    if (unbounded) {
        // check if universe needs to be expanded
        if (x < gleft || x > gright || y < gtop || y > gbottom) {
            if (population == 0) {
                // no need to resize empty grids;
                // just adjust grid edges so that x,y is in middle of grid
                gtop = y - int(ght / 2);
                gleft = x - int(gwd / 2);
                gbottom = gtop + ghtm1;
                gright = gleft + gwdm1;
                // set bigint versions of grid edges (used by GUI code)
                gridtop = gtop;
                gridleft = gleft;
                gridbottom = gbottom;
                gridright = gright;
            } else {
                int up = y < gtop ? gtop - y : 0;
                int down = y > gbottom ? y - gbottom : 0;
                int left = x < gleft ? gleft - x : 0;
                int right = x > gright ? x - gright : 0;
                const char* errmsg = resize_grids(up, down, left, right);
                if (errmsg) {
                    if (show_warning) lifewarning(errmsg);
                    // prevent further warning messages until endofpattern is called
                    // (this avoids user having to close thousands of dialog boxes
                    // if they attempted to paste a large pattern)
                    show_warning = false;
                    return -1;
                }
            }
        }
    } else {
        // check if x,y is outside bounded universe
        if (x < gleft || x > gright) return -1;
        if (y < gtop || y > gbottom) return -1;
    }

    // set x,y cell in currgrid
    int gx = x - gleft;
    int gy = y - gtop;
    unsigned char* cellptr = currgrid + gy * gwd + gx;
    int oldstate = *cellptr;
    if (newstate != oldstate) {
        *cellptr = (unsigned char)newstate;
        // population might change
        if (oldstate == 0 && newstate > 0) {
            population++;
            if (gx < minx) minx = gx;
            if (gx > maxx) maxx = gx;
            if (gy < miny) miny = gy;
            if (gy > maxy) maxy = gy;
        } else if (oldstate > 0 && newstate == 0) {
            population--;
            if (population == 0) empty_boundaries();
        }
    }
    
    return 0;
}

// -----------------------------------------------------------------------------

// Get the state of the cell at the given location.

int ltlalgo::getcell(int x, int y)
{
    if (unbounded) {
        // cell outside grid is dead
        if (x < gleft || x > gright) return 0;
        if (y < gtop || y > gbottom) return 0;
    } else {
        // error if x,y is outside bounded universe
        if (x < gleft || x > gright) return -1;
        if (y < gtop || y > gbottom) return -1;
    }

    // get x,y cell in currgrid
    unsigned char* cellptr = currgrid + (y - gtop) * gwd + (x - gleft);
    return *cellptr;
}

// -----------------------------------------------------------------------------

// Return the distance to the next non-zero cell in the given row,
// or -1 if there is none.

int ltlalgo::nextcell(int x, int y, int& v)
{
    // check if x,y is outside grid
    if (x < gleft || x > gright) return -1;
    if (y < gtop || y > gbottom) return -1;
    
    // get x,y cell in currgrid
    unsigned char* cellptr = currgrid + (y - gtop) * gwd + (x - gleft);
    
    // init distance
    int d = 0;
    do {
        v = *cellptr;
        if (v > 0) return d;    // found a non-zero cell
        d++;
        cellptr++;
        x++;
    } while (x <= gright);
    
    return -1;
}

// -----------------------------------------------------------------------------

static bigint bigpop;

const bigint& ltlalgo::getPopulation()
{
    bigpop = population;
    return bigpop;
}

// -----------------------------------------------------------------------------

int ltlalgo::isEmpty()
{
    return population == 0 ? 1 : 0;
}

// -----------------------------------------------------------------------------

void ltlalgo::update_next_grid(int x, int y, int xyoffset, int ncount)
{
    // x,y cell in nextgrid might change based on the given neighborhood count
    unsigned char state = *(currgrid + xyoffset);
    if (state == 0) {
        // this cell is dead
        if (ncount >= minB && ncount <= maxB) {
            // new cell is born in nextgrid
            unsigned char* nextcell = nextgrid + xyoffset;
            *nextcell = 1;
            population++;
            if (x < minx) minx = x;
            if (x > maxx) maxx = x;
            if (y < miny) miny = y;
            if (y > maxy) maxy = y;
        }
    } else if (state == 1) {
        // this cell is alive
        if (totalistic == 0) ncount--;              // don't count this cell
        if (ncount >= minS && ncount <= maxS) {
            // cell survives so copy into nextgrid
            unsigned char* nextcell = nextgrid + xyoffset;
            *nextcell = 1;
            // population doesn't change but boundary in nextgrid might
            if (x < minx) minx = x;
            if (x > maxx) maxx = x;
            if (y < miny) miny = y;
            if (y > maxy) maxy = y;
        } else if (maxCellStates > 2) {
            // cell decays to state 2
            unsigned char* nextcell = nextgrid + xyoffset;
            *nextcell = 2;
            // population doesn't change but boundary in nextgrid might
            if (x < minx) minx = x;
            if (x > maxx) maxx = x;
            if (y < miny) miny = y;
            if (y > maxy) maxy = y;
        } else {
            // cell dies
            population--;
            if (population == 0) empty_boundaries();
        }
    } else {
        // state is > 1 so this cell will eventually die
        if (state + 1 < maxCellStates) {
            unsigned char* nextcell = nextgrid + xyoffset;
            *nextcell = state + 1;
            // population doesn't change but boundary in nextgrid might
            if (x < minx) minx = x;
            if (x > maxx) maxx = x;
            if (y < miny) miny = y;
            if (y > maxy) maxy = y;
        } else {
            // cell dies
            population--;
            if (population == 0) empty_boundaries();
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::faster_Moore(int mincol, int minrow, int maxcol, int maxrow)
{
    // use Adam P. Goucher's algorithm to calculate Moore neighborhood counts
    minrow -= range;
    mincol -= range;
    maxrow += range;
    maxcol += range;
    
    if (unbounded) {
        // we can safely assume there is at least a 2*range boundary of
        // dead cells surrounding the pattern
        int r2 = range * 2;
        int minrowpr2 = minrow+r2;
        int mincolpr2 = mincol+r2;
        
        // put zeros in top 2*range rows of colcounts
        for (int i = minrow; i < minrowpr2; i++) {
            int* ccptr = colcounts + i * gwd + mincol;
            for (int j = mincol; j <= maxcol; j++) {
                *ccptr++ = 0;
            }
        }
    
        // put zeros in left 2*range columns of colcounts
        for (int j = mincol; j < mincolpr2; j++) {
            int* ccptr = colcounts + minrowpr2 * gwd + j;
            for (int i = minrowpr2; i <= maxrow; i++) {
                *ccptr = 0;
                ccptr += gwd;
            }
        }
    
        // calculate cumulative counts for remaining columns and store in colcounts
        for (int i = minrowpr2; i <= maxrow; i++) {
            int rowcount = 0;
            for (int j = mincolpr2; j <= maxcol; j++) {
                int offset = i * gwd + j;
                unsigned char* cellptr = currgrid + offset;
                if (*cellptr == 1) rowcount++;
                int* ccptr = colcounts + offset;
                *ccptr = *(ccptr - gwd) + rowcount;
            }
        }
    } else {
        // bounded grid, so the expanded edges might contain state-1 cells;
        // calculate cumulative counts for each column and store in colcounts
        for (int i = minrow; i <= maxrow; i++) {
            int rowcount = 0;
            for (int j = mincol; j <= maxcol; j++) {
                int offset = i * gwd + j;
                unsigned char* cellptr = currgrid + offset;
                if (*cellptr == 1) rowcount++;
                int* ccptr = colcounts + offset;
                if (i > minrow) {
                    *ccptr = *(ccptr - gwd) + rowcount;
                } else {
                    *ccptr = rowcount;
                }
            }
        }
    }
    
    minrow += range;
    mincol += range;
    maxrow -= range;
    maxcol -= range;

    // calculate final neighborhood counts using values in colcounts
    // and update the corresponding cells in nextgrid
    int minrowplus = minrow + range;
    int mincolplus = mincol + range;
    int* colptr = colcounts + minrowplus * gwd;
    int* ccptr = colptr + mincolplus;
    update_next_grid(mincol, minrow, minrow*gwd+mincol, *ccptr);
    
    int rangep1 = range + 1;
    for (int j = mincol+1; j <= maxcol; j++) {
        // do i == minrow
        int* ccptr1 = colptr + (j+range);
        int* ccptr2 = colptr + (j-rangep1);
        update_next_grid(j, minrow, minrow*gwd+j, *ccptr1 - *ccptr2);
    }
    
    colptr = colcounts + mincolplus;
    for (int i = minrow+1; i <= maxrow; i++) {
        // do j == mincol
        int* ccptr1 = colptr + (i+range) * gwd;
        int* ccptr2 = colptr + (i-rangep1) * gwd;
        update_next_grid(mincol, i, i*gwd+mincol, *ccptr1 - *ccptr2);
    }
    
    for (int i = minrow+1; i <= maxrow; i++) {
        int* ipr = colcounts + (i+range) * gwd;
        int* imrm1 = colcounts + (i-rangep1) * gwd;
        for (int j = mincol+1; j <= maxcol; j++) {
            int jpr = j+range;
            int jmrm1 = j-rangep1;
            int* ccptr1 = ipr + jpr;
            int* ccptr2 = imrm1 + jmrm1;
            int* ccptr3 = ipr + jmrm1;
            int* ccptr4 = imrm1 + jpr;
            update_next_grid(j, i, i*gwd+j, *ccptr1 + *ccptr2 - *ccptr3 - *ccptr4);
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::fast_Moore(int mincol, int minrow, int maxcol, int maxrow)
{
    if (range == 1) {
        for (int y = minrow; y <= maxrow; y++) {
            int yoffset = y * gwd;
            unsigned char* topy = currgrid + (y - 1) * gwd;
            for (int x = mincol; x <= maxcol; x++) {
                // count the state-1 neighbors within the current range
                // using the extended Moore neighborhood with no edge checks
                int ncount = 0;
                unsigned char* cellptr = topy + (x - 1);
                if (*cellptr++ == 1) ncount++;
                if (*cellptr++ == 1) ncount++;
                if (*cellptr   == 1) ncount++;
                cellptr += gwd;
                if (*cellptr   == 1) ncount++;
                if (*--cellptr == 1) ncount++;
                if (*--cellptr == 1) ncount++;
                cellptr += gwd;
                if (*cellptr++ == 1) ncount++;
                if (*cellptr++ == 1) ncount++;
                if (*cellptr   == 1) ncount++;
                update_next_grid(x, y, yoffset+x, ncount);
            }
        }
    } else {
        // range > 1
        int rightcol = 2 * range;
        for (int y = minrow; y <= maxrow; y++) {
            int yoffset = y * gwd;
            int ymrange = y - range;
            int yprange = y + range;
            unsigned char* topy = currgrid + ymrange * gwd;
            
            // for the 1st cell in this row we count the state-1 cells in the
            // extended Moore neighborhood and remember the column counts
            int colcount[MAXNCOLS];
            int xmrange = mincol - range;
            int xprange = mincol + range;
            int ncount = 0;
            for (int i = xmrange; i <= xprange; i++) {
                unsigned char* cellptr = topy + i;
                int col = i - xmrange;
                colcount[col] = 0;
                for (int j = ymrange; j <= yprange; j++) {
                    if (*cellptr == 1) colcount[col]++;
                    cellptr += gwd;
                }
                ncount += colcount[col];
            }
            
            // we now have the neighborhood counts in each column;
            // eg. 7 columns if range == 3:
            //   ---------------
            //   | | | | | | | |
            //   | | | | | | | |
            //   |0|1|2|3|4|5|6|
            //   | | | | | | | |
            //   | | | | | | | |
            //   ---------------
            
            update_next_grid(mincol, y, yoffset+mincol, ncount);
            
            // for the remaining cells in this row we only need to update
            // the count in the right column of the new neighborhood
            // and shift the other column counts to the left
            topy += range;
            for (int x = mincol+1; x <= maxcol; x++) {
                // get count in right column
                int rcount = 0;
                unsigned char* cellptr = topy + x;
                for (int j = ymrange; j <= yprange; j++) {
                    if (*cellptr == 1) rcount++;
                    cellptr += gwd;
                }
                
                ncount = rcount;
                for (int i = 1; i <= rightcol; i++) {
                    ncount += colcount[i];
                    colcount[i-1] = colcount[i];
                }
                colcount[rightcol] = rcount;
                
                update_next_grid(x, y, yoffset+x, ncount);
            }
        }
    
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::fast_Neumann(int mincol, int minrow, int maxcol, int maxrow)
{
    if (range == 1) {
        int gwd2 = gwd * 2;
        for (int y = minrow; y <= maxrow; y++) {
            int yoffset = y * gwd;
            unsigned char* topy = currgrid + yoffset;
            for (int x = mincol; x <= maxcol; x++) {
                // count the state-1 neighbors within the current range
                // using the extended von Neumann neighborhood with no edge checks
                // (at range 1 a diamond is a cross: +)
                int ncount = 0;
                unsigned char* cellptr = topy + (x - 1);
                if (*cellptr++ == 1) ncount++;
                if (*cellptr++ == 1) ncount++;
                if (*cellptr   == 1) ncount++;
                cellptr -= gwd;
                if (*--cellptr == 1) ncount++;
                cellptr += gwd2;
                if (*cellptr   == 1) ncount++;
                update_next_grid(x, y, yoffset+x, ncount);
            }
        }
    } else {
        // range > 1
        for (int y = minrow; y <= maxrow; y++) {
            int yoffset = y * gwd;
            int ymrange = y - range;
            int yprange = y + range;
            unsigned char* topy = currgrid + ymrange * gwd;
            for (int x = mincol; x <= maxcol; x++) {
                // count the state-1 neighbors within the current range
                // using the extended von Neumann neighborhood (diamond) with no edge checks
                int ncount = 0;
                int xoffset = 0;
                unsigned char* rowptr = topy;
                for (int j = ymrange; j < y; j++) {
                    unsigned char* cellptr = rowptr + (x - xoffset);
                    int len = 2 * xoffset + 1;
                    for (int i = 0; i < len; i++) {
                        if (*cellptr++ == 1) ncount++;
                    }
                    xoffset++;          // 0, 1, 2, ..., range
                    rowptr += gwd;
                }
                // xoffset == range
                for (int j = y; j <= yprange; j++) {
                    unsigned char* cellptr = rowptr + (x - xoffset);
                    int len = 2 * xoffset + 1;
                    for (int i = 0; i < len; i++) {
                        if (*cellptr++ == 1) ncount++;
                    }
                    xoffset--;          // range-1, ..., 2, 1, 0
                    rowptr += gwd;
                }
                update_next_grid(x, y, yoffset+x, ncount);
            }
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::slow_torus_Moore(int mincol, int minrow, int maxcol, int maxrow)
{
    for (int y = minrow; y <= maxrow; y++) {
        int yoffset = y * gwd;
        for (int x = mincol; x <= maxcol; x++) {
            // count the state-1 neighbors within the current range
            // using the extended Moore neighborhood and wrapping at grid edges
            int ncount = 0;
            for (int j = -range; j <= range; j++) {
                int newy = y + j;
                if (newy >= ght) {
                    newy -= ght;
                } else if (newy < 0) {
                    newy += ght;
                }
                unsigned char* rowptr = currgrid + newy * gwd;
                for (int i = -range; i <= range; i++) {
                    int newx = x + i;
                    if (newx >= gwd) {
                        newx -= gwd;
                    } else if (newx < 0) {
                        newx += gwd;
                    }
                    unsigned char* cellptr = rowptr + newx;
                    if (*cellptr == 1) ncount++;
                }
            }
            update_next_grid(x, y, yoffset+x, ncount);
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::slow_torus_Neumann(int mincol, int minrow, int maxcol, int maxrow)
{
    for (int y = minrow; y <= maxrow; y++) {
        int yoffset = y * gwd;
        int ymrange = y - range;
        int yprange = y + range;
        for (int x = mincol; x <= maxcol; x++) {
            // count the state-1 neighbors within the current range
            // using the extended von Neumann neighborhood and wrapping at grid edges
            int ncount = 0;
            int xoffset = 0;
            for (int j = ymrange; j < y; j++) {
                int newy = j;
                if (newy >= ght) {
                    newy -= ght;
                } else if (newy < 0) {
                    newy += ght;
                }
                unsigned char* rowptr = currgrid + newy * gwd;
                int xleft = x - xoffset;
                int len = 2 * xoffset + 1;
                for (int i = 0; i < len; i++) {
                    int newx = xleft + i;
                    if (newx >= gwd) {
                        newx -= gwd;
                    } else if (newx < 0) {
                        newx += gwd;
                    }
                    unsigned char* cellptr = rowptr + newx;
                    if (*cellptr == 1) ncount++;
                }
                xoffset++;          // 0, 1, 2, ..., range
            }
            // xoffset == range
            for (int j = y; j <= yprange; j++) {
                int newy = j;
                if (newy >= ght) {
                    newy -= ght;
                } else if (newy < 0) {
                    newy += ght;
                }
                unsigned char* rowptr = currgrid + newy * gwd;
                int xleft = x - xoffset;
                int len = 2 * xoffset + 1;
                for (int i = 0; i < len; i++) {
                    int newx = xleft + i;
                    if (newx >= gwd) {
                        newx -= gwd;
                    } else if (newx < 0) {
                        newx += gwd;
                    }
                    unsigned char* cellptr = rowptr + newx;
                    if (*cellptr == 1) ncount++;
                }
                xoffset--;          // range-1, ..., 2, 1, 0
            }
            update_next_grid(x, y, yoffset+x, ncount);
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::slow_plane_Moore(int mincol, int minrow, int maxcol, int maxrow)
{
    for (int y = minrow; y <= maxrow; y++) {
        int yoffset = y * gwd;
        int ymrange = y - range;
        int yprange = y + range;
        if (ymrange < 0) ymrange = 0;
        if (yprange >= ght) yprange = ghtm1;
        
        for (int x = mincol; x <= maxcol; x++) {
            int xmrange = x - range;
            int xprange = x + range;
            if (xmrange < 0) xmrange = 0;
            if (xprange >= gwd) xprange = gwdm1;
            
            // count the state-1 neighbors within the current range
            // using the extended Moore neighborhood clipped to plane
            int ncount = 0;
            for (int j = ymrange; j <= yprange; j++) {
                unsigned char* cellptr = currgrid + j * gwd + xmrange;
                for (int i = xmrange; i <= xprange; i++) {
                    if (*cellptr++ == 1) ncount++;
                }
            }
            update_next_grid(x, y, yoffset+x, ncount);
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::slow_plane_Neumann(int mincol, int minrow, int maxcol, int maxrow)
{
    for (int y = minrow; y <= maxrow; y++) {
        int yoffset = y * gwd;
        int ymrange = y - range;
        int yprange = y + range;
        for (int x = mincol; x <= maxcol; x++) {
            // count the state-1 neighbors within the current range
            // using the extended von Neumann neighborhood clipped to plane
            int ncount = 0;
            int xoffset = 0;
            for (int j = ymrange; j < y; j++) {
                if (j >= 0 && j < ght) {
                    unsigned char* rowptr = currgrid + j * gwd;
                    int xleft = x - xoffset;
                    int len = 2 * xoffset + 1;
                    for (int i = 0; i < len; i++) {
                        int newx = xleft + i;
                        if (newx >= 0 && newx < gwd) {
                            unsigned char* cellptr = rowptr + newx;
                            if (*cellptr == 1) ncount++;
                        }
                    }
                }
                xoffset++;          // 0, 1, 2, ..., range
            }
            // xoffset == range
            for (int j = y; j <= yprange; j++) {
                if (j >= 0 && j < ght) {
                    unsigned char* rowptr = currgrid + j * gwd;
                    int xleft = x - xoffset;
                    int len = 2 * xoffset + 1;
                    for (int i = 0; i < len; i++) {
                        int newx = xleft + i;
                        if (newx >= 0 && newx < gwd) {
                            unsigned char* cellptr = rowptr + newx;
                            if (*cellptr == 1) ncount++;
                        }
                    }
                }
                xoffset--;          // range-1, ..., 2, 1, 0
            }
            update_next_grid(x, y, yoffset+x, ncount);
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::slowgen(int mincol, int minrow, int maxcol, int maxrow)
{
    if (minrow <= maxrow && mincol <= maxcol) {
        // process all cells in the given rectangle, taking into account the topology
        // and checking if the extended neighborhood is outside the grid edges
        if (topology == 'T') {
            // torus
            if (ntype == 'M') {
                slow_torus_Moore(mincol, minrow, maxcol, maxrow);
            } else {
                slow_torus_Neumann(mincol, minrow, maxcol, maxrow);
            }
        } else {
            // plane
            if (ntype == 'M') {
                slow_plane_Moore(mincol, minrow, maxcol, maxrow);
            } else {
                slow_plane_Neumann(mincol, minrow, maxcol, maxrow);
            }
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::do_bounded_gen()
{
    if (gwd == minsize && ght == minsize) {
        // reset minx,miny,maxx,maxy for first birth or survivor in nextgrid
        empty_boundaries();
    
        // process every cell in the grid, with edge checks
        slowgen(0, 0, gwdm1, ghtm1);
    
    } else {
        // limit processing to rectangle where births/deaths can occur
        int mincol, minrow, maxcol, maxrow;
        if (minB == 0) {
            // birth in every dead cell so process entire grid
            mincol = 0;
            minrow = 0;
            maxcol = gwdm1;
            maxrow = ghtm1;
        } else {
            mincol = minx - range;
            minrow = miny - range;
            maxcol = maxx + range;
            maxrow = maxy + range;
            
            // check if the limits are outside the grid edges
            if (mincol < 0) {
                mincol = 0;
                if (topology == 'T') maxcol = gwdm1;
            }
            if (maxcol > gwdm1) {
                maxcol = gwdm1;
                if (topology == 'T') mincol = 0;
            }
            if (minrow < 0) {
                minrow = 0;
                if (topology == 'T') maxrow = ghtm1;
            }
            if (maxrow > ghtm1) {
                maxrow = ghtm1;
                if (topology == 'T') minrow = 0;
            }
        }
        
        // reset minx,miny,maxx,maxy for first birth or survivor in nextgrid
        empty_boundaries();
    
        // if the limits are outside the inner rectangle (ie. grid inset by range)
        // then we need to call slowgen up to 4 times:
        //
        //                    grid
        //   o--------------------------------------   o = (0,0)
        //   |  m--------------------------------  |   m = (mincol,minrow)
        //   |  |*******************************|  |
        //   |  |**r--------------------------**|  |   r = (range,range)
        //   |  |**|                         |**|  |
        //   |  |**|     inner rectangle     |**|  |
        //   |  |**|                         |**|  |
        //   |  |**--------------------------s**|  |   s = (gwdm1-range,ghtm1-range)
        //   |  |*******************************|  |
        //   |  --------------------------------n  |   n = (maxcol,maxrow)
        //   --------------------------------------p   p = (gwdm1,ghtm1)
        //
        if (minrow < range) {
            // process rectangle above inner rectangle
            slowgen(mincol, minrow, maxcol, range - 1);
        }
        if (mincol < range) {
            // process rectangle to the left of inner rectangle
            slowgen(mincol, range, range - 1, ghtm1 - range);
        }
        if (maxrow > ghtm1 - range) {
            // process rectangle below inner rectangle
            slowgen(mincol, ghtm1 - range + 1, maxcol, maxrow);
        }
        if (maxcol > gwdm1 - range) {
            // process rectangle to the right of inner rectangle
            slowgen(gwdm1 - range + 1, range, maxcol, ghtm1 - range);
        }
        
        // find the intersection of the limits with the inner rectangle
        // and process that intersection rapidly (there's no need to check
        // if a cell's extended neighborhood is outside the grid edges)
        if (minrow < range) minrow = range;
        if (mincol < range) mincol = range;
        if (maxrow > ghtm1 - range) maxrow = ghtm1 - range;
        if (maxcol > gwdm1 - range) maxcol = gwdm1 - range;
        if (minrow <= maxrow && mincol <= maxcol) {
            if (ntype == 'M') {
                if (colcounts) {
                    faster_Moore(mincol, minrow, maxcol, maxrow);
                } else {
                    fast_Moore(mincol, minrow, maxcol, maxrow);
                }
            } else {
                fast_Neumann(mincol, minrow, maxcol, maxrow);
            }
        }
    }
}

// -----------------------------------------------------------------------------

bool ltlalgo::do_unbounded_gen()
{
    int mincol = minx - range;
    int minrow = miny - range;
    int maxcol = maxx + range;
    int maxrow = maxy + range;

    if (mincol < range || maxcol > gwdm1-range || minrow < range || maxrow > ghtm1-range) {
        // pattern boundary is too close to a grid edge so expand the universe in that
        // direction, and possibly shrink the universe in the opposite direction        
        int inc = MAXRANGE * 2;
        int up    = minrow < range       ? inc : 0;
        int down  = maxrow > ghtm1-range ? inc : 0;
        int left  = mincol < range       ? inc : 0;
        int right = maxcol > gwdm1-range ? inc : 0;
        
        // check for possible shrinkage (pattern might be a spaceship)
        if (up > 0    && down == 0  && maxrow < ghtm1-range) down  = -(ghtm1-maxrow-range);
        if (down > 0  && up == 0    && minrow > range)       up    = -(minrow-range);
        if (left > 0  && right == 0 && maxcol < gwdm1-range) right = -(gwdm1-maxcol-range);
        if (right > 0 && left == 0  && mincol > range)       left  = -(mincol-range);
        
        const char* errmsg = resize_grids(up, down, left, right);
        if (errmsg) {
            lifewarning(errmsg);    // no need to check show_warning here
            return false;           // stop generating
        }
        
        mincol = minx - range;
        minrow = miny - range;
        maxcol = maxx + range;
        maxrow = maxy + range;
        
        /* DEBUG: remove eventually!!! */
        if (mincol < range || maxcol > gwdm1-range || minrow < range || maxrow > ghtm1-range) {
            char msg[256];
            sprintf(msg, "BUG: new grid wd,ht = %d,%d\nmincol,minrow,maxcol,maxrow = %d,%d,%d,%d",
                         gwd,ght,mincol,minrow,maxcol,maxrow);
            lifewarning(msg);
        }
    }
        
    // reset minx,miny,maxx,maxy for first birth or survivor in nextgrid
    empty_boundaries();

    if (ntype == 'M') {
        if (colcounts) {
            faster_Moore(mincol, minrow, maxcol, maxrow);
        } else {
            fast_Moore(mincol, minrow, maxcol, maxrow);
        }
    } else {
        fast_Neumann(mincol, minrow, maxcol, maxrow);
    }
    return true;
}

// -----------------------------------------------------------------------------

// Do increment generations.

void ltlalgo::step()
{
    bigint t = increment;
    while (t != 0) {
        if (population > 0 || minB == 0) {
            int prevpop = population;
            
            // calculate the next generation in nextgrid
            if (unbounded) {
                if (!do_unbounded_gen()) {
                    // failed to resize universe so stop generating
                    poller->setInterrupted();
                    return;
                }
            } else {
                do_bounded_gen();
            }
        
            // swap currgrid and nextgrid
            unsigned char* temp = currgrid;
            currgrid = nextgrid;
            nextgrid = temp;
            
            // kill all cells in nextgrid
            if (prevpop > 0) memset(nextgrid, 0, gridbytes);
        }
    
        generation += bigint::one;
        
        // this is a safe place to check for user events
        if (poller->inner_poll()) return;
        
        t -= 1;
        // user might have changed increment 
        if (t > increment) t = increment;
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::save_cells()
{
    for (int y = miny; y <= maxy; y++) {
        int yoffset = y * gwd;
        for (int x = minx; x <= maxx; x++) {
            unsigned char* currcell = currgrid + yoffset + x;
            if (*currcell) {
                cell_list.push_back(x + gleft);
                cell_list.push_back(y + gtop);
                cell_list.push_back(*currcell);
            }
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::restore_cells()
{
    clipped_cells.clear();
    for (size_t i = 0; i < cell_list.size(); i += 3) {
        int x = cell_list[i];
        int y = cell_list[i+1];
        int s = cell_list[i+2];
        // check if x,y is outside grid
        if (x < gleft || x > gright || y < gtop || y > gbottom) {
            // store clipped cells so that GUI code (eg. ClearOutsideGrid)
            // can remember them in case this rule change is undone
            clipped_cells.push_back(x);
            clipped_cells.push_back(y);
            clipped_cells.push_back(s);
        } else {
            setcell(x, y, s);
        }
    }
    cell_list.clear();
}

// -----------------------------------------------------------------------------

// Switch to the given rule if it is valid.

const char *ltlalgo::setrule(const char *s)
{
    int r, c, m, s1, s2, b1, b2, endpos;
    char n;
    if (sscanf(s, "R%d,C%d,M%d,S%d..%d,B%d..%d,N%c%n",
                  &r, &c, &m, &s1, &s2, &b1, &b2, &n, &endpos) != 8) {
        // try alternate LtL syntax as defined by Kellie Evans;
        // eg: 5,34,45,34,58 is equivalent to R5,C0,M1,S34..58,B34..45,NM
        if (sscanf(s, "%d,%d,%d,%d,%d%n",
                      &r, &b1, &b2, &s1, &s2, &endpos) == 5) {
            c = 0;
            m = 1;
            n = 'M';
        } else {
            return "bad syntax in Larger than Life rule";
        }
    }
    
    if (r < 1) return "R value is too small";
    if (r > MAXRANGE) return "R value is too big";
    if (c < 0 || c > 255) return "C value must be from 0 to 255";
    if (m < 0 || m > 1) return "M value must be 0 or 1";
    if (s1 > s2) return "S minimum must be <= S maximum";
    if (b1 > b2) return "B minimum must be <= B maximum";
    if (n != 'M' && n != 'N') return "N must be followed by M or N";
    if (s[endpos] != 0 && s[endpos] != ':') return "bad suffix";

    char t = 'T';
    int newwd = DEFAULTSIZE;
    int newht = DEFAULTSIZE;

    // check for explicit suffix like ":T200,100"
    const char *suffix = strchr(s, ':');
    if (suffix && suffix[1] != 0) {
        if (suffix[1] == 'T' || suffix[1] == 't') {
            t = 'T';
        } else if (suffix[1] == 'P' || suffix[1] == 'p') {
            t = 'P';
        } else {
            return "bad topology in suffix (must be torus or plane)";
        }
        if (suffix[2] != 0) {
            if (sscanf(suffix+2, "%d,%d", &newwd, &newht) != 2) {
                if (sscanf(suffix+2, "%d", &newwd) != 1) {
                    return "bad grid size";
                } else {
                    newht = newwd;
                }
            }
        }
        if ((float)newwd * (float)newht > MAXCELLS) return "grid size is too big";
    }
    
    if (!suffix) {
        // no suffix given so universe is unbounded
        if (b1 == 0) return "B0 is not allowed if universe is unbounded";
    }

    // the given rule is valid
    range = r;
    scount = c;
    totalistic = m;
    minS = s1;
    maxS = s2;
    minB = b1;
    maxB = b2;
    ntype = n;
    topology = t;
    
    // set the grid_type so the GUI code can display circles or diamonds in icon mode
    grid_type = ntype == 'M' ? SQUARE_GRID : VN_GRID;
    
    if (suffix) {
        // use a bounded universe
        minsize = 2 * range;
        if (newwd < minsize) newwd = minsize;
        if (newht < minsize) newht = minsize;
        
        // note that if the old universe is unbounded we need to create new grids
        // in case the middle cell in the old grids is not at 0,0
        if (gwd != newwd || ght != newht || unbounded) {
            if (population > 0) {
                save_cells();       // store the current pattern in cell_list
            }
            // update gwd and ght, free the current grids and allocate new ones
            gwd = newwd;
            ght = newht;
            free(currgrid);
            free(nextgrid);
            if (colcounts) free(colcounts);
            create_grids();
            if (cell_list.size() > 0) {
                // restore the pattern (if the new grid is smaller then any live cells
                // outside the grid will be saved in clipped_cells)
                restore_cells();
            }
        }

        // need to set a lifealgo flag if minB is 0 so the GUI code can allow
        // pattern generation when the population is 0 ???
        // hasB0 = minB == 0;
        
        // this algo uses a true bounded grid with no border so tell GUI code
        // not to call CreateBorderCells and DeleteBorderCells
        unbounded = false;
        
        // set bounded grid dimensions used by GUI code
        gridwd = gwd;
        gridht = ght;

    } else {
        // no suffix given so use an unbounded universe
        unbounded = true;
        
        // set unbounded grid dimensions used by GUI code
        gridwd = 0;
        gridht = 0;
    
        // don't change size or contents of currgrid and nextgrid
    }

    // set the number of cell states
    if (scount > 2) {
        // show history
        maxCellStates = scount;
    } else {
        maxCellStates = 2;
        scount = 0;         // show C0 in canonical rule
    }

    // set the canonical rule
    if (unbounded) {
        sprintf(canonrule, "R%d,C%d,M%d,S%d..%d,B%d..%d,N%c",
                           range, scount, totalistic, minS, maxS, minB, maxB, ntype);
    } else {
        sprintf(canonrule, "R%d,C%d,M%d,S%d..%d,B%d..%d,N%c:%c%d,%d",
                           range, scount, totalistic, minS, maxS, minB, maxB, ntype,
                           topology, gwd, ght);
    }
    
    return 0;
}

// -----------------------------------------------------------------------------

const char* ltlalgo::getrule()
{
   return canonrule;
}

// -----------------------------------------------------------------------------

const char* ltlalgo::DefaultRule()
{
    return DEFAULTRULE;
}

// -----------------------------------------------------------------------------

static lifealgo *creator() { return new ltlalgo(); }

void ltlalgo::doInitializeAlgoInfo(staticAlgoInfo& ai)
{
    ai.setAlgorithmName("Larger than Life");
    ai.setAlgorithmCreator(&creator);
    ai.setDefaultBaseStep(10);
    ai.setDefaultMaxMem(0);
    ai.minstates = 2;
    ai.maxstates = 256;
    // init default color scheme
    ai.defgradient = true;              // use gradient
    ai.defr1 = 255;                     // start color = yellow
    ai.defg1 = 255;
    ai.defb1 = 0;
    ai.defr2 = 255;                     // end color = red
    ai.defg2 = 0;
    ai.defb2 = 0;
    // if not using gradient then set all states to white
    for (int i=0; i<256; i++) {
        ai.defr[i] = ai.defg[i] = ai.defb[i] = 255;
    }
}
