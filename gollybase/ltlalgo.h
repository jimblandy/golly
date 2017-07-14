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

// This is the code for the "Larger than Life" family of rules.

#ifndef LTLALGO_H
#define LTLALGO_H

#include "lifealgo.h"
#include "liferules.h"  // for MAXRULESIZE
#include <vector>

class ltlalgo : public lifealgo {
public:
    ltlalgo();
    virtual ~ltlalgo();
    virtual void clearall();
    virtual int setcell(int x, int y, int newstate);
    virtual int getcell(int x, int y);
    virtual int nextcell(int x, int y, int& v);
    virtual void endofpattern();
    virtual void setIncrement(bigint inc) { increment = inc; }
    virtual void setIncrement(int inc) { increment = inc; }
    virtual void setGeneration(bigint gen) { generation = gen; }
    virtual const bigint& getPopulation();
    virtual int isEmpty();
    virtual int hyperCapable() { return 0; }
    virtual void setMaxMemory(int m) {}
    virtual int getMaxMemory() { return 0; }
    virtual const char* setrule(const char* s);
    virtual const char* getrule();
    virtual const char* DefaultRule();
    virtual int NumCellStates();
    virtual void step();
    virtual void* getcurrentstate() { return 0; }
    virtual void setcurrentstate(void*) {}
    virtual void draw(viewport& view, liferender& renderer);
    virtual void fit(viewport& view, int force);
    virtual void lowerRightPixel(bigint& x, bigint& y, int mag);
    virtual void findedges(bigint* t, bigint* l, bigint* b, bigint* r);
    virtual const char* writeNativeFormat(std::ostream&, char*) {
        return "No native format for ltlalgo.";
    }
    static void doInitializeAlgoInfo(staticAlgoInfo&);

private:
    char canonrule[MAXRULESIZE];        // canonical version of valid rule passed into setrule
    int population;                     // number of non-zero cells in current generation
    int gwd, ght;                       // width and height of grid (in cells)
    unsigned char* currgrid;            // contains gwd*ght cells for current generation
    unsigned char* nextgrid;            // contains gwd*ght cells for next generation
    int gridbytes;                      // gwd*ght
    int gwdm1, ghtm1;                   // gwd-1, ght-1 (bottom right corner of grid)
    int minsize;                        // minimum size of gwd and ght is 2*range
    int minx, miny, maxx, maxy;         // boundary of live cells (in grid coordinates)
    int gtop, gleft, gbottom, gright;   // cell coordinates of grid edges
    vector<int> cell_list;              // used by save_cells and restore_cells
    bool show_warning;                  // flag used to avoid multiple warning dialogs
    
    // rule parameters (set by setrule)
    int range;                          // neighborhood radius (1..50)
    int scount;                         // count of states (0..255; values > 2 activate history)
    int totalistic;                     // include middle cell in neighborhood count? (1 or 0)
    int minS, maxS;                     // limits for survival
    int minB, maxB;                     // limits for birth
    char ntype;                         // extended neighborhood type (M = Moore, N = von Neumann)
    char topology;                      // grid topology (T = torus, P = plane)
    
    void create_grids();                // allocate currgrid and nextgrid
    void empty_boundaries();            // set minx, miny, maxx, maxy when population is 0
    void save_cells();                  // save current pattern in cell_list
    void restore_cells();               // restore pattern from cell_list
    void do_bounded_gen();              // calculate the next generation in a bounded universe
    bool do_unbounded_gen();            // calculate the next generation in an unbounded universe

    const char* resize_grids(int up, int down, int left, int right);
    // try to resize universe by given amounts (possibly -ve);
    // if it fails then return a suitable error message
    
    void slowgen(int mincol, int minrow, int maxcol, int maxrow);
    void slow_torus_Moore(int mincol, int minrow, int maxcol, int maxrow);
    void slow_torus_Neumann(int mincol, int minrow, int maxcol, int maxrow);
    void slow_plane_Moore(int mincol, int minrow, int maxcol, int maxrow);
    void slow_plane_Neumann(int mincol, int minrow, int maxcol, int maxrow);
    // these routines are called from do_bounded_gen to process a rectangular region of cells
    // where the extended neighborhood of a live cell might be outside the grid edges
    
    void fast_Moore(int mincol, int minrow, int maxcol, int maxrow);
    void fast_Neumann(int mincol, int minrow, int maxcol, int maxrow);
    // these routines are called from do_*_gen to process a rectangular region of cells
    // where we know the extended neighborhood of every live cell is inside the grid edges
    
    void update_next_grid(int x, int y, int xyoffset, int ncount);
    // called from each of the slow_* and fast_* routines to set x,y cell
    // in nextgrid based on the given neighborhood count
    
    int* colcounts;                     // cumulative column counts of state-1 cells
    void faster_Moore(int mincol, int minrow, int maxcol, int maxrow);
    // faster version of fast_Moore (uses Adam P. Goucher's neighbor counting algorithm)
};

#endif
