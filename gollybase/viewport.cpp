// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "viewport.h"
#include "lifealgo.h"
#include <cmath>

int MAX_MAG = 4 ;   // default maximum cell size is 2^4

using namespace std ;

void viewport::init() {
   x = 0 ;
   y = 0 ;
   height = width = 8 ;
   mag = 0 ;
   x0 = 0 ;
   y0 = 0 ;
   x0f = 0 ;
   y0f = 0 ;
   xymf = 0 ;
}
void viewport::zoom() {
   if (mag >= MAX_MAG)
      return ;
   mag++ ;
   reposition() ;
}
void viewport::zoom(int xx, int yy) {
   if (mag >= MAX_MAG)
      return ;
   pair<bigint, bigint> oldpos = at(xx, yy);    // save cell pos for use below
   int x2c = xx * 2 - getxmax() ;
   bigint o = x2c ;
   o.mulpow2(-mag-2) ;
   x += o ;
   int y2c = yy * 2 - getymax() ;
   o = y2c ;
   o.mulpow2(-mag-2) ;
   y += o ;
   mag++ ;
   reposition() ;
   // adjust cell position if necessary to avoid any drift
   if (mag >= 0) {
      pair<bigint, bigint> newpos = at(xx, yy);
      bigint xdrift = newpos.first;
      bigint ydrift = newpos.second;
      xdrift -= oldpos.first;
      ydrift -= oldpos.second;
      // drifts will be -1, 0 or 1
      if (xdrift != 0) move(-xdrift.toint() << mag, 0);
      if (ydrift != 0) move(0, -ydrift.toint() << mag);
   }
}
void viewport::unzoom() {
   mag-- ;
   reposition() ;
}
void viewport::unzoom(int xx, int yy) {
   pair<bigint, bigint> oldpos = at(xx, yy);    // save cell pos for use below
   mag-- ;
   int x2c = xx * 2 - getxmax() ;
   bigint o = x2c ;
   o.mulpow2(-mag-2) ;
   x -= o ;
   int y2c = yy * 2 - getymax() ;
   o = y2c ;
   o.mulpow2(-mag-2) ;
   y -= o ;
   reposition() ;
   if (mag >= 0) {
      // adjust cell position if necessary to avoid any drift
      pair<bigint, bigint> newpos = at(xx, yy);
      bigint xdrift = newpos.first;
      bigint ydrift = newpos.second;
      xdrift -= oldpos.first;
      ydrift -= oldpos.second;
      // drifts will be -1, 0 or 1
      if (xdrift != 0) move(-xdrift.toint() << mag, 0);
      if (ydrift != 0) move(0, -ydrift.toint() << mag);
   }
}
pair<bigint, bigint> viewport::at(int x, int y) {
   bigint rx = x ;
   bigint ry = y ;
   rx.mulpow2(-mag) ;
   ry.mulpow2(-mag) ;
   rx += x0 ;
   ry += y0 ;
   return pair<bigint, bigint>(rx, ry) ;
}
pair<double, double> viewport::atf(int x, int y) {
   return pair<double, double>(x0f + x * xymf, y0f + y * xymf) ;
}
/**
 *   Returns the screen position of a particular pixel.  Note that this
 *   is a tiny bit more complicated than you might expect, because it
 *   has to take into account exactly how a life algorithm compresses
 *   multiple pixels into a single pixel (which depends not only on the
 *   lifealgo, but in the case of qlifealgo, *also* depends on the
 *   generation count).  In the case of mag < 0, it always returns
 *   the upper left pixel; it's up to the caller to adjust when
 *   mag<0.
 */
pair<int,int> viewport::screenPosOf(bigint x, bigint y, lifealgo *algo) {
   if (mag < 0) {
      bigint xx0 = x0 ;
      bigint yy0 = y0 ;
      algo->lowerRightPixel(xx0, yy0, mag) ;
      y -= yy0 ;
      x -= xx0 ;
   } else {
      x -= x0 ;
      y -= y0 ;
   }
   x.mulpow2(mag) ;
   y.mulpow2(mag) ;

   int xx = 0 ;
   int yy = 0 ;
   
/* AKT: don't do this clipping as it makes it harder to
        calculate an accurate paste rectangle
   if (x < 0)
     xx = -1 ;
   else if (x > getxmax())
     xx = getxmax() + 1 ;
   else
     xx = x.toint() ;
     
   if (y < 0)
     yy = -1 ;
   else if (y > getymax())
     yy = getymax() + 1 ;
   else
     yy = y.toint() ;
*/
   
   if (x > bigint::maxint)
      xx = INT_MAX ;
   else if (x < bigint::minint)
      xx = INT_MIN ;
   else
      xx = x.toint() ;
   
   if (y > bigint::maxint)
      yy = INT_MAX ;
   else if (y < bigint::minint)
      yy = INT_MIN ;
   else
      yy = y.toint() ;
     
   return pair<int,int>(xx,yy) ;
}
void viewport::move(int dx, int dy) {
   // dx and dy are in pixels
   if (mag > 0) {
      dx /= (1 << mag) ;
      dy /= (1 << mag) ;
   }
   bigint addx = dx ;
   bigint addy = dy ;
   if (mag < 0) {
      addx <<= -mag ;
      addy <<= -mag ;
   }
   x += addx ;
   y += addy ;
   reposition() ;
}
void viewport::resize(int newwidth, int newheight) {
   width = newwidth ;
   height = newheight ;
   reposition() ;
}
void viewport::center() {
   x = 0 ;
   y = 0 ;
   reposition() ;
}
void viewport::reposition() {
   xymf = pow(2.0, -mag) ;
   bigint w = 1 + getxmax() ;
   w.mulpow2(-mag) ;
   w >>= 1 ;
   x0 = x ;
   x0 -= w ;
   w = 1 + getymax() ;
   w.mulpow2(-mag) ;
   w >>= 1 ;
   y0 = y ;
   y0 -= w ;
   y0f = y0.todouble() ;
   x0f = x0.todouble() ;
}
void viewport::setpositionmag(const bigint &xarg, const bigint &yarg,
                              int magarg) {
   x = xarg ;
   y = yarg ;
   mag = magarg ;
   reposition() ;
}
/*
 *   This is only called by fit.  We find an x/y location that
 *   centers things optimally.
 */
void viewport::setpositionmag(const bigint &xmin, const bigint &xmax,
                              const bigint &ymin, const bigint &ymax,
                              int magarg) {
   mag = magarg ;
   x = xmax ;
   x += xmin ;
   x += 1 ;
   x >>= 1 ;
   y = ymax ;
   y += ymin ;
   y += 1 ;
   y >>= 1 ;
   reposition() ;
}
int viewport::contains(const bigint &xarg, const bigint &yarg) {
   if (xarg < x0 || yarg < y0)
      return 0 ;
   bigint t = getxmax() ;
   t += 1 ;
   t.mulpow2(-mag) ;
   t -= 1 ;
   t += x0 ;
   if (xarg > t)
      return 0 ;
   t = getymax() ;
   t += 1 ;
   t.mulpow2(-mag) ;
   t -= 1 ;
   t += y0 ;
   if (yarg > t)
      return 0 ;
   return 1 ;
}
