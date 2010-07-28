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
#include "viewport.h"
#include "lifealgo.h"
#include <cmath>
#include <iostream>
#include <cstdio>
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
   
   // AKT: better to do this in bigint::toint() and make these static consts???
   bigint bigmaxint(INT_MAX) ;
   bigint bigminint(INT_MIN) ;

   if (x > bigmaxint)
      xx = INT_MAX ;
   else if (x < bigminint)
      xx = INT_MIN ;
   else
      xx = x.toint() ;
   
   if (y > bigmaxint)
      yy = INT_MAX ;
   else if (y < bigminint)
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
   x0 = - getxmax() ;
   y0 = - getymax() ;
   x0.mulpow2(-mag) ;
   y0.mulpow2(-mag) ;
   x0 += 1 ;
   y0 += 1 ;
   x0 >>= 1 ;
   y0 >>= 1 ;
   x0 += x ;
   y0 += y ;
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
int viewport::contains(const bigint &xarg, const bigint &yarg) {
   bigint t = x ;
   t -= xarg ;
   t.mulpow2(mag+1) ;
   if (t <= -getxmax() || t >= getxmax()) {
     return 0 ;
   }
   t = y ;
   t -= yarg ;
   t.mulpow2(mag+1) ;
   if (t <= -getymax() || t >= getymax()) {
     return 0 ;
   }
   return 1 ;
}
