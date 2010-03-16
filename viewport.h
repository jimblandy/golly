                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2010 Andrew Trevorrow and Tomas Rokicki.

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
#ifndef VIEWPORT_H
#define VIEWPORT_H
#include "bigint.h"
#include <utility>
using namespace std ;
class lifealgo ;
/**
 *   This class holds information on where in space the user's window is.
 *   It provides the functions to zoom, unzoom, move, etc.
 *
 *   The reason we need a whole class is because doing this in a universe
 *   that may be billions of billions of cells on a side, but we still
 *   want single cell precision, is a bit tricky.
 *
 *   This class is not finished yet; I still need to be more precise about
 *   cases where mag>0 and precisely what will be displayed in this case
 *   and precisely how the coordinate transformations will occur.
 */
const int MAX_MAG = 4 ;
class viewport {
public:
   viewport(int width, int height) {
      init() ;
      resize(width, height) ;
   }
   void zoom() ;
   void zoom(int x, int y) ;
   void unzoom() ;
   void unzoom(int x, int y) ;
   void center() ;
   pair<bigint, bigint> at(int x, int y) ;
   pair<double, double> atf(int x, int y) ;
   pair<int,int> screenPosOf(bigint x, bigint y, lifealgo *algo) ;
   void resize(int newwidth, int newheight) ;
   void move(int dx, int dy) ;   // dx and dy are given in pixels
   int getmag() const { return mag ; }
   void setmag(int magarg) { mag = magarg ; reposition() ; }
   void setpositionmag(const bigint &xarg, const bigint &yarg, int magarg) ;
   int getwidth() const { return width ; }
   int getheight() const { return height ; }
   int getxmax() const { return width-1 ; }
   int getymax() const { return height-1 ; }
   int contains(const bigint &x, const bigint &y) ;
   bigint x, y ;           // cell at center of viewport
private:
   void init() ;
   void reposition() ;     // recalculate *0* and *m* values
   int width, height ;
   int mag ;               // plus is zoom in; neg is zoom out
   bigint x0, y0 ;
   double x0f, y0f ;
   double xymf ;           // always = 2**-mag
} ;
#endif
