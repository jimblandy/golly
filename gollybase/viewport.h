// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef VIEWPORT_H
#define VIEWPORT_H
#include "bigint.h"
#include <utility>
using std::pair;
using std::make_pair;

class lifealgo ;
/**
 *   This class holds information on where in space the user's window is.
 *   It provides the functions to zoom, unzoom, move, etc.
 *
 *   The reason we need a whole class is because doing this in a universe
 *   that may be billions of billions of cells on a side, but we still
 *   want single cell precision, is a bit tricky.
 */
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
   void setpositionmag(const bigint &xlo, const bigint &xhi,
                       const bigint &ylo, const bigint &yhi, int magarg) ;
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

extern int MAX_MAG;
// maximum cell size is 2^MAX_MAG (default is 2^4, but devices with
// high-resolution screens will probably want a bigger cell size)

#endif
