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
    virtual int nextcell(int x, int y, int &v);
    virtual void endofpattern() {}
    virtual void setIncrement(bigint inc) { increment = inc; }
    virtual void setIncrement(int inc) { increment = inc; }
    virtual void setGeneration(bigint gen) { generation = gen; }
    virtual const bigint &getPopulation();
    virtual int isEmpty();
    virtual int hyperCapable() { return 0; }
    virtual void setMaxMemory(int m) {}
    virtual int getMaxMemory() { return 0; }
    virtual const char *setrule(const char *s);
    virtual const char *getrule();
    virtual const char* DefaultRule();
    virtual int NumCellStates();
    virtual void step();
    virtual void* getcurrentstate() { return 0; }
    virtual void setcurrentstate(void *) {}
    virtual void draw(viewport &view, liferender &renderer);
    virtual void fit(viewport &view, int force);
    virtual void lowerRightPixel(bigint &x, bigint &y, int mag);
    virtual void findedges(bigint *t, bigint *l, bigint *b, bigint *r);
    virtual const char *writeNativeFormat(std::ostream &, char *) {
        return "No native format for ltlalgo.";
    }
    static void doInitializeAlgoInfo(staticAlgoInfo &);

private:
    char canonrule[MAXRULESIZE];        // canonical version of valid rule passed into setrule
    int population;                     // number of non-zero cells in current generation
    unsigned char* currgrid;            // contains gridwd*gridht cells for current generation
    unsigned char* nextgrid;            // contains gridwd*gridht cells for next generation
    int gridbytes;                      // gridwd*gridht
    int gridwdm1, gridhtm1;             // gridwd-1, gridht-1 (bottom right corner of grid)
    int gtop, gleft, gbottom, gright;   // cell coordinates of grid edges (middle cell is 0,0)
    int minx, miny, maxx, maxy;         // boundary of live cells (in grid coordinates)
    vector<int> cell_list;              // used by save_cells and restore_cells
    
    // rule parameters (set by setrule)
    int range;                          // neighborhood radius (1..10)
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
    void dogen();                       // called from step() to calculate the next generation
    
    void slowgen(int mincol, int minrow, int maxcol, int maxrow);
    void slow_torus_Moore(int mincol, int minrow, int maxcol, int maxrow);
    void slow_torus_Neumann(int mincol, int minrow, int maxcol, int maxrow);
    void slow_plane_Moore(int mincol, int minrow, int maxcol, int maxrow);
    void slow_plane_Neumann(int mincol, int minrow, int maxcol, int maxrow);
    // called from dogen() to process a rectangular region of cells where
    // the extended neighborhood of a cell might be outside the grid edges
    
    void fast_Moore(int mincol, int minrow, int maxcol, int maxrow);
    void fast_Neumann(int mincol, int minrow, int maxcol, int maxrow);
    // called from dogen() to process a rectangular region of cells where
    // we know the extended neighborhood of every cell is inside the grid edges
    
    void update_next_grid(int x, int y, int yoffset, int ncount);
    // called from each of the slow_* and fast_* routines to set x,y cell
    // in nextgrid based on given neighborhood count
};

#endif
