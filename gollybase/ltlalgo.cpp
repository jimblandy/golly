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
#include <stdlib.h>
#include <limits.h>     // for INT_MIN and INT_MAX
#include <string.h>     // for memset and strchr

// -----------------------------------------------------------------------------

// set default rule to match Life
static const char *DEFAULTRULE = "R1,C0,M0,S2..3,B3..3,NM";

#define MAXRANGE 50             // too slow???!!!
#define DEFAULTSIZE 400         // must be >= 2*MAXRANGE

// maximum number of cells in grid must be < 2^31 so population can't overflow
#define MAXCELLS 100000000.0    // too big???!!!

// -----------------------------------------------------------------------------

// Create a new empty universe.

ltlalgo::ltlalgo()
{
    // allocate currgrid and nextgrid using the default grid size
    gridwd = DEFAULTSIZE;
    gridht = DEFAULTSIZE;
    create_grids();
    
    generation = 0;
    increment = 1;
    
    // this algo uses a true bounded grid with no border so tell GUI code
    // not to call CreateBorderCells and DeleteBorderCells
    unbounded = false;
}

// -----------------------------------------------------------------------------

// Destroy the universe.

ltlalgo::~ltlalgo()
{
    free(currgrid);
    free(nextgrid);
}

// -----------------------------------------------------------------------------

void ltlalgo::create_grids()
{
    gridbytes = gridwd * gridht;
    currgrid = (unsigned char*) calloc(gridbytes, sizeof(*currgrid));
    nextgrid = (unsigned char*) calloc(gridbytes, sizeof(*nextgrid));
    if (currgrid == NULL || nextgrid == NULL) lifefatal("not enough memory for LtL grids!");

    // cell at bottom right corner of grid
    gridwdm1 = gridwd - 1;
    gridhtm1 = gridht - 1;

    // set cell coordinates of grid edges
    gtop = -int(gridht / 2);
    gleft = -int(gridwd / 2);
    gbottom = gtop + gridhtm1;
    gright = gleft + gridwdm1;
    
    // also need to set bigint versions of grid edges (used by GUI code)
    gridtop = gtop;
    gridleft = gleft;
    gridbottom = gbottom;
    gridright = gright;
    
    // the universe is empty
    population = 0;
    
    // init boundaries so next birth will change them
    empty_boundaries();
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

// Set the cell at the given location to the given state.

int ltlalgo::setcell(int x, int y, int newstate)
{
    if (newstate < 0 || newstate >= maxCellStates) return -1;
    
    // check if x,y is outside grid
    if (x < gleft || x > gright) return -1;
    if (y < gtop || y > gbottom) return -1;

    // set x,y cell in currgrid
    int gx = x - gleft;
    int gy = y - gtop;
    unsigned char* cellptr = currgrid + gy * gridwd + gx;
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
    // check if x,y is outside grid
    if (x < gleft || x > gright) return -1;
    if (y < gtop || y > gbottom) return -1;

    // get x,y cell in currgrid
    unsigned char* cellptr = currgrid + (y - gtop) * gridwd + (x - gleft);
    return *cellptr;
}

// -----------------------------------------------------------------------------

// Return the distance to the next non-zero cell in the given row,
// or -1 if there is none.

int ltlalgo::nextcell(int x, int y, int &v)
{
    // check if x,y is outside grid
    if (x < gleft || x > gright) return -1;
    if (y < gtop || y > gbottom) return -1;
    
    // get x,y cell in currgrid
    unsigned char* cellptr = currgrid + (y - gtop) * gridwd + (x - gleft);
    
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

const bigint &ltlalgo::getPopulation()
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

void ltlalgo::update_next_grid(int x, int y, int yoffset, int ncount)
{
    // x,y cell in nextgrid might change based on the given neighborhood count
    unsigned char* currcell = currgrid + yoffset + x;
    if (*currcell == 0) {
        // this cell is dead
        if (ncount >= minB && ncount <= maxB) {
            // new cell is born in nextgrid
            unsigned char* nextcell = nextgrid + yoffset + x;
            *nextcell = 1;
            population++;
            if (x < minx) minx = x;
            if (x > maxx) maxx = x;
            if (y < miny) miny = y;
            if (y > maxy) maxy = y;
        }
    } else if (*currcell == 1) {
        // this cell is alive
        if (totalistic == 0) ncount--;              // don't count this cell
        if (ncount >= minS && ncount <= maxS) {
            // cell survives so copy into nextgrid
            unsigned char* nextcell = nextgrid + yoffset + x;
            *nextcell = 1;
            // population doesn't change but boundary in nextgrid might
            if (x < minx) minx = x;
            if (x > maxx) maxx = x;
            if (y < miny) miny = y;
            if (y > maxy) maxy = y;
        } else if (maxCellStates > 2) {
            // cell decays to state 2
            unsigned char* nextcell = nextgrid + yoffset + x;
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
        if (*currcell + 1 < maxCellStates) {
            unsigned char* nextcell = nextgrid + yoffset + x;
            *nextcell = *currcell + 1;
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

void ltlalgo::fast_Moore(int mincol, int minrow, int maxcol, int maxrow)
{
    if (range == 1) {
        for (int y = minrow; y <= maxrow; y++) {
            int yoffset = y * gridwd;
            unsigned char* topy = currgrid + (y - 1) * gridwd;
            for (int x = mincol; x <= maxcol; x++) {
                // count the state-1 neighbors within the current range
                // using the extended Moore neighborhood with no edge checks
                int ncount = 0;
                unsigned char* cellptr = topy + (x - 1);
                if (*cellptr++ == 1) ncount++;
                if (*cellptr++ == 1) ncount++;
                if (*cellptr   == 1) ncount++;
                cellptr += gridwd;
                if (*cellptr   == 1) ncount++;
                if (*--cellptr == 1) ncount++;
                if (*--cellptr == 1) ncount++;
                cellptr += gridwd;
                if (*cellptr++ == 1) ncount++;
                if (*cellptr++ == 1) ncount++;
                if (*cellptr   == 1) ncount++;
                update_next_grid(x, y, yoffset, ncount);
            }
        }
    } else {
        // range > 1
        for (int y = minrow; y <= maxrow; y++) {
            int yoffset = y * gridwd;
            int ymrange = y - range;
            int yprange = y + range;
            unsigned char* topy = currgrid + ymrange * gridwd;
            
            // for the 1st cell in this row we count the state-1 cells in the
            // Moore neighborhood's left column, middle columns, and right column
            int xmrange = mincol - range;
            int xprange = mincol + range;
            
            // get count in left column
            int lcount = 0;
            unsigned char* cellptr = topy + xmrange;
            for (int j = ymrange; j <= yprange; j++) {
                if (*cellptr == 1) lcount++;
                cellptr += gridwd;
            }
            
            // get count in right column
            int rcount = 0;
            cellptr = topy + xprange;
            for (int j = ymrange; j <= yprange; j++) {
                if (*cellptr == 1) rcount++;
                cellptr += gridwd;
            }
            
            // get count in middle columns
            xmrange++;
            xprange--;
            int midcount = 0;
            for (int j = ymrange; j <= yprange; j++) {
                cellptr = currgrid + j * gridwd + xmrange;
                for (int i = xmrange; i <= xprange; i++) {
                    if (*cellptr++ == 1) midcount++;
                }
            }
            
            // we now have the neighborhood counts in 3 parts:
            //      2*range+1
            //   ---------------
            //   | |         | |
            //   | |         | |
            //   |l|    m    |r| 2*range+1
            //   | |         | |
            //   | |         | |
            //   ---------------
            
            update_next_grid(mincol, y, yoffset, lcount+midcount+rcount);
            
            // for the remaining cells in this row we only need to count
            // the left and right columns in each cell's neighborhood
            for (int x = mincol+1; x <= maxcol; x++) {
                // get new left count
                lcount = 0;
                cellptr = topy + x - range;
                for (int j = ymrange; j <= yprange; j++) {
                    if (*cellptr == 1) lcount++;
                    cellptr += gridwd;
                }
                
                // new midcount = old midcount + OLD rcount - NEW lcount
                midcount = midcount + rcount - lcount;

                // get new right count
                rcount = 0;
                cellptr = topy + x + range;
                for (int j = ymrange; j <= yprange; j++) {
                    if (*cellptr == 1) rcount++;
                    cellptr += gridwd;
                }
                
                update_next_grid(x, y, yoffset, lcount+midcount+rcount);
            }
        }
    
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::fast_Neumann(int mincol, int minrow, int maxcol, int maxrow)
{
    if (range == 1) {
        int gridwd2 = gridwd * 2;
        for (int y = minrow; y <= maxrow; y++) {
            int yoffset = y * gridwd;
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
                cellptr -= gridwd;
                if (*--cellptr == 1) ncount++;
                cellptr += gridwd2;
                if (*cellptr   == 1) ncount++;
                update_next_grid(x, y, yoffset, ncount);
            }
        }
    } else {
        // range > 1
        for (int y = minrow; y <= maxrow; y++) {
            int yoffset = y * gridwd;
            int ymrange = y - range;
            int yprange = y + range;
            unsigned char* topy = currgrid + ymrange * gridwd;
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
                    rowptr += gridwd;
                }
                // xoffset == range
                for (int j = y; j <= yprange; j++) {
                    unsigned char* cellptr = rowptr + (x - xoffset);
                    int len = 2 * xoffset + 1;
                    for (int i = 0; i < len; i++) {
                        if (*cellptr++ == 1) ncount++;
                    }
                    xoffset--;          // range-1, ..., 2, 1, 0
                    rowptr += gridwd;
                }
                update_next_grid(x, y, yoffset, ncount);
            }
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::slow_torus_Moore(int mincol, int minrow, int maxcol, int maxrow)
{
    for (int y = minrow; y <= maxrow; y++) {
        int yoffset = y * gridwd;
        for (int x = mincol; x <= maxcol; x++) {
            // count the state-1 neighbors within the current range
            // using the extended Moore neighborhood and wrapping at grid edges
            int ncount = 0;
            for (int j = -range; j <= range; j++) {
                int newy = y + j;
                if (newy >= (int)gridht) {
                    newy = newy % gridht;
                } else if (newy < 0) {
                    newy += gridht;
                }
                unsigned char* rowptr = currgrid + newy * gridwd;
                for (int i = -range; i <= range; i++) {
                    int newx = x + i;
                    if (newx >= (int)gridwd) {
                        newx = newx % gridwd;
                    } else if (newx < 0) {
                        newx += gridwd;
                    }
                    unsigned char* cellptr = rowptr + newx;
                    if (*cellptr == 1) ncount++;
                }
            }
            update_next_grid(x, y, yoffset, ncount);
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::slow_torus_Neumann(int mincol, int minrow, int maxcol, int maxrow)
{
    for (int y = minrow; y <= maxrow; y++) {
        int yoffset = y * gridwd;
        int ymrange = y - range;
        int yprange = y + range;
        for (int x = mincol; x <= maxcol; x++) {
            // count the state-1 neighbors within the current range
            // using the extended von Neumann neighborhood and wrapping at grid edges
            int ncount = 0;
            int xoffset = 0;
            for (int j = ymrange; j < y; j++) {
                int newy = j;
                if (newy >= (int)gridht) {
                    newy = newy % gridht;
                } else if (newy < 0) {
                    newy += gridht;
                }
                unsigned char* rowptr = currgrid + newy * gridwd;
                int xleft = x - xoffset;
                int len = 2 * xoffset + 1;
                for (int i = 0; i < len; i++) {
                    int newx = xleft + i;
                    if (newx >= (int)gridwd) {
                        newx = newx % gridwd;
                    } else if (newx < 0) {
                        newx += gridwd;
                    }
                    unsigned char* cellptr = rowptr + newx;
                    if (*cellptr == 1) ncount++;
                }
                xoffset++;          // 0, 1, 2, ..., range
            }
            // xoffset == range
            for (int j = y; j <= yprange; j++) {
                int newy = j;
                if (newy >= (int)gridht) {
                    newy = newy % gridht;
                } else if (newy < 0) {
                    newy += gridht;
                }
                unsigned char* rowptr = currgrid + newy * gridwd;
                int xleft = x - xoffset;
                int len = 2 * xoffset + 1;
                for (int i = 0; i < len; i++) {
                    int newx = xleft + i;
                    if (newx >= (int)gridwd) {
                        newx = newx % gridwd;
                    } else if (newx < 0) {
                        newx += gridwd;
                    }
                    unsigned char* cellptr = rowptr + newx;
                    if (*cellptr == 1) ncount++;
                }
                xoffset--;          // range-1, ..., 2, 1, 0
            }
            update_next_grid(x, y, yoffset, ncount);
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::slow_plane_Moore(int mincol, int minrow, int maxcol, int maxrow)
{
    for (int y = minrow; y <= maxrow; y++) {
        int yoffset = y * gridwd;
        int ymrange = y - range;
        int yprange = y + range;
        if (ymrange < 0) ymrange = 0;
        if (yprange >= (int)gridht) yprange = gridhtm1;
        
        for (int x = mincol; x <= maxcol; x++) {
            int xmrange = x - range;
            int xprange = x + range;
            if (xmrange < 0) xmrange = 0;
            if (xprange >= (int)gridwd) xprange = gridwdm1;
            
            // count the state-1 neighbors within the current range
            // using the extended Moore neighborhood clipped to plane
            int ncount = 0;
            for (int j = ymrange; j <= yprange; j++) {
                unsigned char* cellptr = currgrid + j * gridwd + xmrange;
                for (int i = xmrange; i <= xprange; i++) {
                    if (*cellptr++ == 1) ncount++;
                }
            }
            update_next_grid(x, y, yoffset, ncount);
        }
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::slow_plane_Neumann(int mincol, int minrow, int maxcol, int maxrow)
{
    for (int y = minrow; y <= maxrow; y++) {
        int yoffset = y * gridwd;
        int ymrange = y - range;
        int yprange = y + range;
        for (int x = mincol; x <= maxcol; x++) {
            // count the state-1 neighbors within the current range
            // using the extended von Neumann neighborhood clipped to plane
            int ncount = 0;
            int xoffset = 0;
            for (int j = ymrange; j < y; j++) {
                if (j >= 0 && j < (int)gridht) {
                    unsigned char* rowptr = currgrid + j * gridwd;
                    int xleft = x - xoffset;
                    int len = 2 * xoffset + 1;
                    for (int i = 0; i < len; i++) {
                        int newx = xleft + i;
                        if (newx >= 0 && newx < (int)gridwd) {
                            unsigned char* cellptr = rowptr + newx;
                            if (*cellptr == 1) ncount++;
                        }
                    }
                }
                xoffset++;          // 0, 1, 2, ..., range
            }
            // xoffset == range
            for (int j = y; j <= yprange; j++) {
                if (j >= 0 && j < (int)gridht) {
                    unsigned char* rowptr = currgrid + j * gridwd;
                    int xleft = x - xoffset;
                    int len = 2 * xoffset + 1;
                    for (int i = 0; i < len; i++) {
                        int newx = xleft + i;
                        if (newx >= 0 && newx < (int)gridwd) {
                            unsigned char* cellptr = rowptr + newx;
                            if (*cellptr == 1) ncount++;
                        }
                    }
                }
                xoffset--;          // range-1, ..., 2, 1, 0
            }
            update_next_grid(x, y, yoffset, ncount);
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

void ltlalgo::dogen()
{
    if ((int)gridwd == minsize && (int)gridht == minsize) {
        // reset minx,miny,maxx,maxy for first birth or survivor in nextgrid
        empty_boundaries();
    
        // process every cell in the grid, with edge checks
        slowgen(0, 0, gridwdm1, gridhtm1);
    
    } else {
        // limit processing to rectangle where births/deaths can occur
        int mincol, minrow, maxcol, maxrow;
        if (minB == 0) {
            // birth in every dead cell so process entire grid
            mincol = 0;
            minrow = 0;
            maxcol = gridwdm1;
            maxrow = gridhtm1;
        } else {
            mincol = minx - range;
            minrow = miny - range;
            maxcol = maxx + range;
            maxrow = maxy + range;
            
            // check if the limits are outside the grid edges
            if (mincol < 0 || maxcol >= (int)gridwd) {
                mincol = 0;
                maxcol = gridwdm1;
            }
            if (minrow < 0 || maxrow >= (int)gridht) {
                minrow = 0;
                maxrow = gridhtm1;
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
        //   |  |**--------------------------s**|  |   s = (gridwdm1-range,gridhtm1-range)
        //   |  |*******************************|  |
        //   |  --------------------------------n  |   n = (maxcol,maxrow)
        //   --------------------------------------p   p = (gridwdm1,gridhtm1)
        //
        if (minrow < range) {
            // process rectangle above inner rectangle
            slowgen(mincol, minrow, maxcol, range - 1);
        }
        if (mincol < range) {
            // process rectangle to the left of inner rectangle
            slowgen(mincol, range, range - 1, gridhtm1 - range);
        }
        if (maxrow > gridhtm1 - range) {
            // process rectangle below inner rectangle
            slowgen(mincol, gridhtm1 - range + 1, maxcol, maxrow);
        }
        if (maxcol > gridwdm1 - range) {
            // process rectangle to the right of inner rectangle
            slowgen(gridwdm1 - range + 1, range, maxcol, gridhtm1 - range);
        }
        
        // find the intersection of the limits with the inner rectangle
        // and process that intersection rapidly (there's no need to check
        // if a cell's extended neighborhood is outside the grid edges)
        if (minrow < range) minrow = range;
        if (mincol < range) mincol = range;
        if (maxrow > gridhtm1 - range) maxrow = gridhtm1 - range;
        if (maxcol > gridwdm1 - range) maxcol = gridwdm1 - range;
        if (minrow <= maxrow && mincol <= maxcol) {
            if (ntype == 'M') {
                fast_Moore(mincol, minrow, maxcol, maxrow);
            } else {
                fast_Neumann(mincol, minrow, maxcol, maxrow);
            }
        }
    }
}

// -----------------------------------------------------------------------------

// Do increment generations.

void ltlalgo::step()
{
    bigint t = increment;
    while (t != 0) {
        if (population > 0 || minB == 0) {
            dogen();
        
            // swap currgrid and nextgrid
            unsigned char* temp = currgrid;
            currgrid = nextgrid;
            nextgrid = temp;
            
            // clear nextgrid for next dogen call
            memset(nextgrid, 0, gridbytes);
        }
    
        generation += bigint::one;
        
        // this is a safe place to check for user events
        if (poller->inner_poll()) break;
        
        t -= 1;
        // user might have changed increment 
        if (t > increment) t = increment;
    }
}

// -----------------------------------------------------------------------------

void ltlalgo::save_cells()
{
    for (int y = miny; y <= maxy; y++) {
        int yoffset = y * gridwd;
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
    }

    if ((float)newwd * (float)newht > MAXCELLS) return "grid size is too big";

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

    minsize = 2 * range;
    if (newwd < minsize) newwd = minsize;
    if (newht < minsize) newht = minsize;
    
    // set the grid_type so the GUI can display circles or diamonds in icon mode
    grid_type = ntype == 'M' ? SQUARE_GRID : VN_GRID;
    
    // need to set a lifealgo flag if minB is 0 so GUI code can allow
    // pattern generation when the population is 0 ???!!!
    // hasB0 = minB == 0;

    if ((int)gridwd != newwd || (int)gridht != newht) {
        if (population > 0) {
            save_cells();       // store the current pattern in cell_list
        }
        // update gridwd and gridht, free the current grids and allocate new ones
        gridwd = newwd;
        gridht = newht;
        free(currgrid);
        free(nextgrid);
        create_grids();
        if (cell_list.size() > 0) {
            // restore the pattern (if the new grid is smaller then any live cells
            // outside the grid will be saved in clipped_cells)
            restore_cells();
        }
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
    sprintf(canonrule, "R%d,C%d,M%d,S%d..%d,B%d..%d,N%c:%c%d,%d",
                       range, scount, totalistic, minS, maxS, minB, maxB, ntype,
                       topology, gridwd, gridht);

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

void ltlalgo::doInitializeAlgoInfo(staticAlgoInfo &ai)
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
    
    // or use MCell's default colors!!!???
    
    for (int i=0; i<256; i++) {
        ai.defr[i] = ai.defg[i] = ai.defb[i] = 255;
    }
}
