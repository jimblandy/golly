                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2008 Andrew Trevorrow and Tomas Rokicki.

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
/**
 *   This file is where we figure out how to draw jvn structures,
 *   no matter what the magnification or renderer.
 */
#include "jvnalgo.h"
#include <vector>
#include <cstring>
#include <iostream>
#include <cstdio>
#include <algorithm>
using namespace std ;

const int logbmsize = 7 ;                 // 6=64x64  7=128x128  8=256x256
const int bmsize = (1<<logbmsize) ;
const int byteoff = (bmsize/8) ;
/* AKT
const int ibufsize = (bmsize*bmsize/32) ;
static unsigned int ibigbuf[ibufsize] ;   // a shared buffer for up to 256x256 pixels
static unsigned char *bigbuf = (unsigned char *)ibigbuf ;
*/
const int rowoff = (bmsize*3) ;           // AKT
const int ibufsize = (bmsize*bmsize*3) ;  // each pixel has 3 r,g,b values
static char ibigbuf[ibufsize] ;           // a shared buffer for up to 256x256 pixels
static char *bigbuf = ibigbuf ;

// AKT: made this a method of jvnalgo so it can use cellred etc
void jvnalgo::drawpixel(int x, int y) {
   /* AKT
   bigbuf[(((bmsize-1)-y) << (logbmsize-3)) + (x >> 3)] |= (1 << (x & 7)) ;
   */
   int i = (bmsize-1-y) * rowoff + x;
   // AKT: for this test we assume all live cells are in state 1
   bigbuf[i]   = cellred[1];
   bigbuf[i+1] = cellgreen[1];
   bigbuf[i+2] = cellblue[1];
}
/*
 *   Draw a 4x4 area yielding 1x1, 2x2, or 4x4 pixels.
 */
static
void draw4x4_1(unsigned short sw, unsigned short se,
               unsigned short nw, unsigned short ne, int llx, int lly) {
   /* AKT
   unsigned char *p = bigbuf + ((bmsize-1+lly) << (logbmsize-3)) + ((-llx) >> 3) ;
   int bit = 1 << ((-llx) & 0x7) ;
   if (sw) *p |= bit ;
   if (se) *p |= (bit << 1) ;
   p -= byteoff ;
   if (nw) *p |= bit ;
   if (ne) *p |= (bit << 1) ;
   */
   // AKT: just draw 1 pixel for this test
   int i = (bmsize-1+lly) * rowoff - llx;
   bigbuf[i]   = 255;//!!!cellred[1];
   bigbuf[i+1] = 255;//!!!cellgreen[1];
   bigbuf[i+2] = 255;//!!!cellblue[1];
}
static
void draw4x4_1(jnode *n, jnode *z, int llx, int lly) {
   /* AKT
   unsigned char *p = bigbuf + ((bmsize-1+lly) << (logbmsize-3)) + ((-llx) >> 3) ;
   int bit = 1 << ((-llx) & 0x7) ;
   if (n->sw != z) *p |= bit ;
   if (n->se != z) *p |= (bit << 1) ;
   p -= byteoff ;
   if (n->nw != z) *p |= bit ;
   if (n->ne != z) *p |= (bit << 1) ;
   */
   // AKT: just draw 1 pixel for this test
   int i = (bmsize-1+lly) * rowoff - llx;
   bigbuf[i]   = 255;//!!!cellred[1];
   bigbuf[i+1] = 255;//!!!cellgreen[1];
   bigbuf[i+2] = 255;//!!!cellblue[1];
}
static unsigned char compress4x4[256] ;
static unsigned char rev8[256] ;
static
void draw4x4_2(unsigned short bits1, unsigned short bits2, int llx, int lly) {
   /* AKT
   unsigned char *p = bigbuf + ((bmsize-1+lly) << (logbmsize-3)) + ((-llx) >> 3) ;
   int mask = (((-llx) & 0x4) ? 0xf0 : 0x0f) ;
   int db = ((bits1 | (bits1 << 4)) & 0xf0f0) +
            ((bits2 | (bits2 >> 4)) & 0x0f0f) ;
   p[0] |= mask & compress4x4[db & 255] ;
   p[-byteoff] |= mask & compress4x4[db >> 8] ;
   */
   // AKT: just draw 1 pixel for this test
   int i = (bmsize-1+lly) * rowoff - llx;
   bigbuf[i]   = 255;//!!!cellred[1];
   bigbuf[i+1] = 255;//!!!cellgreen[1];
   bigbuf[i+2] = 255;//!!!cellblue[1];
}
static
void draw4x4_4(unsigned short bits1, unsigned short bits2, int llx, int lly) {
   /* AKT
   unsigned char *p = bigbuf + ((bmsize-1+lly) << (logbmsize-3)) + ((-llx) >> 3) ;
   p[0] = rev8[((bits1 << 4) & 0xf0) + (bits2 & 0xf)] ;
   p[-byteoff] = rev8[(bits1 & 0xf0) + ((bits2 >> 4) & 0xf)] ;
   p[-2*byteoff] = rev8[((bits1 >> 4) & 0xf0) + ((bits2 >> 8) & 0xf)] ;
   p[-3*byteoff] = rev8[((bits1 >> 8) & 0xf0) + ((bits2 >> 12) & 0xf)] ;
   */
   // AKT: just draw 1 pixel for this test
   int i = (bmsize-1+lly) * rowoff - llx;
   bigbuf[i]   = 255;//!!!cellred[1];
   bigbuf[i+1] = 255;//!!!cellgreen[1];
   bigbuf[i+2] = 255;//!!!cellblue[1];
}

// AKT: set all pixels to background color
void jvnalgo::killpixels() {
   if (cellred[0] == cellgreen[0] && cellgreen[0] == cellblue[0]) {
      // use fast method
      memset(bigbuf, cellred[0], sizeof(ibigbuf));
   } else {
      // use slow method
      // or create a single killed row at start of draw() and use memcpy here???
      for (int i = 0; i < ibufsize/3; i += 3) {
         bigbuf[i]   = cellred[0];
         bigbuf[i+1] = cellgreen[0];
         bigbuf[i+2] = cellblue[0];
      }
   }
}

void jvnalgo::clearrect(int minx, int miny, int w, int h) {
   // minx,miny is lower left corner
   if (w <= 0 || h <= 0)
      return ;
   if (pmag > 1) {
      minx *= pmag ;
      miny *= pmag ;
      w *= pmag ;
      h *= pmag ;
   }
   miny = uviewh - miny - h ;
   renderer->killrect(minx, miny, w, h) ;
}

void jvnalgo::renderbm(int x, int y) {
   // x,y is lower left corner
   int rx = x ;
   int ry = y ;
   int rw = bmsize ;
   int rh = bmsize ;
   if (pmag > 1) {
      rx *= pmag ;
      ry *= pmag ;
      rw *= pmag ;
      rh *= pmag ;
   }
   ry = uviewh - ry - rh ;
   /* AKT
   renderer->blit(rx, ry, rw, rh, (int *)ibigbuf, pmag) ;
   memset(bigbuf, 0, sizeof(ibigbuf)) ;
   */
   renderer->pixblit(rx, ry, rw, rh, bigbuf, pmag);
   killpixels();
}
/*
 *   Here, llx and lly are coordinates in screen pixels describing
 *   where the lower left pixel of the screen is.  Draw one jnode.
 *   This is our main recursive routine.
 */
void jvnalgo::drawjnode(jnode *n, int llx, int lly, int depth, jnode *z) {
   int sw = 1 << (depth - mag + 1) ;
   if (sw >= bmsize &&
       (llx + vieww <= 0 || lly + viewh <= 0 || llx >= sw || lly >= sw))
      return ;
   if (n == z) {
      if (sw >= bmsize)
         clearrect(-llx, -lly, sw, sw) ;
   } else if (depth > 2 && sw > 2) {
      z = z->nw ;
      sw >>= 1 ;
      depth-- ;
      if (sw == (bmsize >> 1)) {
         drawjnode(n->sw, 0, 0, depth, z) ;
         drawjnode(n->se, -(bmsize/2), 0, depth, z) ;
         drawjnode(n->nw, 0, -(bmsize/2), depth, z) ;
         drawjnode(n->ne, -(bmsize/2), -(bmsize/2), depth, z) ;
         renderbm(-llx, -lly) ;
      } else {
         drawjnode(n->sw, llx, lly, depth, z) ;
         drawjnode(n->se, llx-sw, lly, depth, z) ;
         drawjnode(n->nw, llx, lly-sw, depth, z) ;
         drawjnode(n->ne, llx-sw, lly-sw, depth, z) ;
      }
   } else if (depth > 2 && sw == 2) {
      draw4x4_1(n, z->nw, llx, lly) ;
   } else if (sw == 1) {
      drawpixel(-llx, -lly) ;
   } else {
      struct jleaf *l = (struct jleaf *)n ;
      sw >>= 1 ;
      if (sw == 1) {
         draw4x4_1(l->sw, l->se, l->nw, l->ne, llx, lly) ;
      } else if (sw == 2) {
         draw4x4_2(l->sw, l->se, llx, lly) ;
         draw4x4_2(l->nw, l->ne, llx, lly-sw) ;
      } else {
         draw4x4_4(l->sw, l->se, llx, lly) ;
         draw4x4_4(l->nw, l->ne, llx, lly-sw) ;
      }
   }
}
/*
 *   Fill in the llxb and llyb bits from the viewport information.
 *   Allocate if necessary.  This arithmetic should be done carefully.
 */
void jvnalgo::fill_ll(int d) {
   pair<bigint, bigint> coor = view->at(0, view->getymax()) ;
   coor.second.mul_smallint(-1) ;
   bigint s = 1 ;
   s <<= d ;
   coor.first += s ;
   coor.second += s ;
   int bitsreq = coor.first.bitsreq() ;
   int bitsreq2 = coor.second.bitsreq() ;
   if (bitsreq2 > bitsreq)
      bitsreq = bitsreq2 ;
   if (bitsreq <= d)
     bitsreq = d + 1 ; // need to access llxyb[d]
   if (bitsreq > llsize) {
      if (llsize) {
         delete [] llxb ;
         delete [] llyb ;
      }
      llxb = new char[bitsreq] ;
      llyb = new char[bitsreq] ;
      llsize = bitsreq ;
   }
   llbits = bitsreq ;
   coor.first.tochararr(llxb, llbits) ;
   coor.second.tochararr(llyb, llbits) ;
}
static void init_rev8() {
   int i;
   for (i=0; i<8; i++) {
      rev8[1<<i] = (unsigned char)(1<<(7-i)) ;
      compress4x4[1<<i] = (unsigned char)(0x11 << ((7 - i) >> 1)) ;
   }
   for (i=0; i<256; i++)
      if (i & (i-1)) {
         rev8[i] = rev8[i & (i-1)] + rev8[i & -i] ;
         compress4x4[i] = compress4x4[i & (i-1)] | compress4x4[i & -i] ;
      }
}
/*
 *   This is the top-level draw routine that takes the root jnode.
 *   It maintains four jnodes onto which the screen fits and uses the
 *   high bits of llx/lly to project those four jnodes as far down
 *   the tree as possible, so we know we can get away with just
 *   32-bit arithmetic in the above recursive routine.  This way
 *   we don't need any high-precision addition or subtraction to
 *   display an image.
 */
void jvnalgo::draw(viewport &viewarg, liferender &rendererarg) {
   if (rev8[255] == 0)
      init_rev8() ;
   /* AKT
   memset(bigbuf, 0, sizeof(ibigbuf)) ;
   */
   killpixels();
   
   ensure_hashed() ;
   renderer = &rendererarg ;
   view = &viewarg ;
   uvieww = view->getwidth() ;
   uviewh = view->getheight() ;
   if (view->getmag() > 0) {
      pmag = 1 << (view->getmag()) ;
      mag = 0 ;
      viewh = ((uviewh - 1) >> view->getmag()) + 1 ;
      vieww = ((uvieww - 1) >> view->getmag()) + 1 ;
      uviewh += (-uviewh) & (pmag - 1) ;
   } else {
      mag = (-view->getmag()) ;
      pmag = 1 ;
      viewh = uviewh ;
      vieww = uvieww ;
   }
   int d = depth ;
   fill_ll(d) ;
   int maxd = vieww ;
   int i ;
   jnode *z = zerojnode(d) ;
   jnode *sw = root, *nw = z, *ne = z, *se = z ;
   if (viewh > maxd)
      maxd = viewh ;
   int llx=-llxb[llbits-1], lly=-llyb[llbits-1] ;
/*   Skip down to top of tree. */
   for (i=llbits-1; i>d && i>=mag; i--) { /* go down to d, but not further than mag */
      llx = (llx << 1) + llxb[i] ;
      lly = (lly << 1) + llyb[i] ;
      if (llx > 2*maxd || lly > 2*maxd || llx < -2*maxd || lly < -2*maxd) {
         clearrect(0, 0, vieww, viewh) ;
	 goto bail ;
      }
   }
   /*  Find the lowest four we need to examine */
   while (d > 2 && d - mag >= 0 &&
	  (d - mag > 28 || (1 << (d - mag)) > 2 * maxd)) {
      llx = (llx << 1) + llxb[d] ;
      lly = (lly << 1) + llyb[d] ;
      if (llx >= 1) {
	 if (lly >= 1) {
            ne = ne->sw ;
            nw = nw->se ;
            se = se->nw ;
            sw = sw->ne ;
	    lly-- ;
         } else {
            ne = se->nw ;
            nw = sw->ne ;
            se = se->sw ;
            sw = sw->se ;
         }
	 llx-- ;
      } else {
	 if (lly >= 1) {
            ne = nw->se ;
            nw = nw->sw ;
            se = sw->ne ;
            sw = sw->nw ;
	    lly-- ;
         } else {
            ne = sw->ne ;
            nw = sw->nw ;
            se = sw->se ;
            sw = sw->sw ;
         }
      }
      if (llx > 2*maxd || lly > 2*maxd || llx < -2*maxd || lly < -2*maxd) {
         clearrect(0, 0, vieww, viewh) ;
         goto bail ;
      }
      d-- ;
   }
   /*  At this point we know we can use 32-bit arithmetic. */
   for (i=d; i>=mag; i--) {
      llx = (llx << 1) + llxb[i] ;
      lly = (lly << 1) + llyb[i] ;
   }
   /* clear the border *around* the universe if necessary */
   if (d + 1 <= mag) {
      jnode *z = zerojnode(d) ;
      if (llx > 0 || lly > 0 || llx + vieww <= 0 || lly + viewh <= 0 ||
          (sw == z && se == z && nw == z && ne == z)) {
         clearrect(0, 0, vieww, viewh) ;
      } else {
         clearrect(0, 1-lly, vieww, viewh-1+lly) ;
         clearrect(0, 0, vieww, -lly) ;
         clearrect(0, -lly, -llx, 1) ;
         clearrect(1-llx, -lly, vieww-1+llx, 1) ;
         drawpixel(0, 0) ;
         renderbm(-llx, -lly) ;
      }
   } else {
      z = zerojnode(d) ;
      maxd = 1 << (d - mag + 2) ;
      clearrect(0, maxd-lly, vieww, viewh-maxd+lly) ;
      clearrect(0, 0, vieww, -lly) ;
      clearrect(0, -lly, -llx, maxd) ;
      clearrect(maxd-llx, -lly, vieww-maxd+llx, maxd) ;
      if (maxd <= bmsize) {
         maxd >>= 1 ;
         drawjnode(sw, 0, 0, d, z) ;
         drawjnode(se, -maxd, 0, d, z) ;
         drawjnode(nw, 0, -maxd, d, z) ;
         drawjnode(ne, -maxd, -maxd, d, z) ;
         renderbm(-llx, -lly) ;
      } else {
         maxd >>= 1 ;
         drawjnode(sw, llx, lly, d, z) ;
         drawjnode(se, llx-maxd, lly, d, z) ;
         drawjnode(nw, llx, lly-maxd, d, z) ;
         drawjnode(ne, llx-maxd, lly-maxd, d, z) ;
      }
   }
bail:
   renderer = 0 ;
   view = 0 ;
}
static
int getbitsfromleaves(const vector<jnode *> &v) {
  unsigned short nw=0, ne=0, sw=0, se=0 ;
  int i;
  for (i=0; i<(int)v.size(); i++) {
    jleaf *p = (jleaf *)v[i] ;
    nw |= p->nw ;
    ne |= p->ne ;
    sw |= p->sw ;
    se |= p->se ;
  }
  int r = 0 ;
  // horizontal bits are least significant ones
  unsigned short w = nw | sw ;
  unsigned short e = ne | se ;
  // vertical bits are next 8
  unsigned short n = nw | ne ;
  unsigned short s = sw | se ;
  for (i=0; i<4; i++) {
    if (w & (0x1111 << i))
      r |= 0x1000 << i ;
    if (e & (0x1111 << i))
      r |= 0x100 << i ;
    if (n & (0xf << (4 * i)))
      r |= 0x10 << i ;
    if (s & (0xf << (4 * i)))
      r |= 0x1 << i ;
  }
  return r ;
}

/**
 *   Copy the vector, but sort it and uniquify it so we don't have a ton
 *   of duplicate jnodes.
 */
static
void sortunique(vector<jnode *> &dest, vector<jnode *> &src) {
  swap(src, dest) ; // note:  this is superfast
  sort(dest.begin(), dest.end()) ;
  vector<jnode *>::iterator new_end = unique(dest.begin(), dest.end()) ;
  dest.erase(new_end, dest.end()) ;
  src.clear() ;
}

void jvnalgo::findedges(bigint *ptop, bigint *pleft, bigint *pbottom, bigint *pright) {
   // following code is from fit() but all goal/size stuff
   // has been removed so it finds the exact pattern edges
   ensure_hashed() ;
   bigint xmin = -1 ;
   bigint xmax = 1 ;
   bigint ymin = -1 ;
   bigint ymax = 1 ;
   int currdepth = depth ;
   int i;
   if (root == zerojnode(currdepth)) {
      // return impossible edges to indicate empty pattern;
      // not really a problem because caller should check first
      *ptop = 1 ;
      *pleft = 1 ;
      *pbottom = 0 ;
      *pright = 0 ;
      return ;
   }
   vector<jnode *> top, left, bottom, right ;
   top.push_back(root) ;
   left.push_back(root) ;
   bottom.push_back(root) ;
   right.push_back(root) ;
   int topbm = 0, bottombm = 0, rightbm = 0, leftbm = 0 ;
   while (currdepth >= 0) {
      currdepth-- ;
      if (currdepth == 1) { // we have jleaf jnodes; turn them into bitmasks
         topbm = getbitsfromleaves(top) & 0xff ;
         bottombm = getbitsfromleaves(bottom) & 0xff ;
         leftbm = getbitsfromleaves(left) >> 8 ;
         rightbm = getbitsfromleaves(right) >> 8 ;
      }
      if (currdepth <= 1) {
          int sz = 1 << (currdepth + 2) ;
          int maskhi = (1 << sz) - (1 << (sz >> 1)) ;
          int masklo = (1 << (sz >> 1)) - 1 ;
          ymax += ymax ;
          if ((topbm & maskhi) == 0) {
             ymax.add_smallint(-2) ;
          } else {
             topbm >>= (sz >> 1) ;
          }
          ymin += ymin ;
          if ((bottombm & masklo) == 0) {
            ymin.add_smallint(2) ;
            bottombm >>= (sz >> 1) ;
          }
          xmax += xmax ;
          if ((rightbm & masklo) == 0) {
             xmax.add_smallint(-2) ;
             rightbm >>= (sz >> 1) ;
          }
          xmin += xmin ;
          if ((leftbm & maskhi) == 0) {
            xmin.add_smallint(2) ;
          } else {
            leftbm >>= (sz >> 1) ;
          }
      } else {
         jnode *z = 0 ;
         if (hashed)
            z = zerojnode(currdepth) ;
         vector<jnode *> newv ;
         int outer = 0 ;
         for (i=0; i<(int)top.size(); i++) {
            jnode *t = top[i] ;
            if (!outer && (t->nw != z || t->ne != z)) {
               newv.clear() ;
               outer = 1 ;
            }
            if (outer) {
               if (t->nw != z)
                  newv.push_back(t->nw) ;
               if (t->ne != z)
                  newv.push_back(t->ne) ;
            } else {
               if (t->sw != z)
                  newv.push_back(t->sw) ;
               if (t->se != z)
                  newv.push_back(t->se) ;
            }
         }
	 sortunique(top, newv) ;
         ymax += ymax ;
         if (!outer) {
            ymax.add_smallint(-2) ;
         }
         outer = 0 ;
         for (i=0; i<(int)bottom.size(); i++) {
            jnode *t = bottom[i] ;
            if (!outer && (t->sw != z || t->se != z)) {
               newv.clear() ;
               outer = 1 ;
            }
            if (outer) {
               if (t->sw != z)
                  newv.push_back(t->sw) ;
               if (t->se != z)
                  newv.push_back(t->se) ;
            } else {
               if (t->nw != z)
                  newv.push_back(t->nw) ;
               if (t->ne != z)
                  newv.push_back(t->ne) ;
            }
         }
	 sortunique(bottom, newv) ;
         ymin += ymin ;
         if (!outer) {
            ymin.add_smallint(2) ;
         }
         outer = 0 ;
         for (i=0; i<(int)right.size(); i++) {
            jnode *t = right[i] ;
            if (!outer && (t->ne != z || t->se != z)) {
               newv.clear() ;
               outer = 1 ;
            }
            if (outer) {
               if (t->ne != z)
                  newv.push_back(t->ne) ;
               if (t->se != z)
                  newv.push_back(t->se) ;
            } else {
               if (t->nw != z)
                  newv.push_back(t->nw) ;
               if (t->sw != z)
                  newv.push_back(t->sw) ;
            }
         }
	 sortunique(right, newv) ;
         xmax += xmax ;
         if (!outer) {
            xmax.add_smallint(-2) ;
         }
         outer = 0 ;
         for (i=0; i<(int)left.size(); i++) {
            jnode *t = left[i] ;
            if (!outer && (t->nw != z || t->sw != z)) {
               newv.clear() ;
               outer = 1 ;
            }
            if (outer) {
               if (t->nw != z)
                  newv.push_back(t->nw) ;
               if (t->sw != z)
                  newv.push_back(t->sw) ;
            } else {
               if (t->ne != z)
                  newv.push_back(t->ne) ;
               if (t->se != z)
                  newv.push_back(t->se) ;
            }
         }
	 sortunique(left, newv) ;
         xmin += xmin ;
         if (!outer) {
            xmin.add_smallint(2) ;
         }
      }
   }
   xmin >>= 1 ;
   xmax >>= 1 ;
   ymin >>= 1 ;
   ymax >>= 1 ;
   xmin <<= (currdepth + 1) ;
   ymin <<= (currdepth + 1) ;
   xmax <<= (currdepth + 1) ;
   ymax <<= (currdepth + 1) ;
   xmax -= 1 ;
   ymax -= 1 ;
   ymin.mul_smallint(-1) ;
   ymax.mul_smallint(-1) ;
   // set pattern edges
   *ptop = ymax ;          // due to y flip
   *pbottom = ymin ;       // due to y flip
   *pleft = xmin ;
   *pright = xmax ;
}

void jvnalgo::fit(viewport &view, int force) {
   ensure_hashed() ;
   bigint xmin = -1 ;
   bigint xmax = 1 ;
   bigint ymin = -1 ;
   bigint ymax = 1 ;
   int xgoal = view.getwidth() - 2 ;
   int ygoal = view.getheight() - 2 ;
   if (xgoal < 8)
      xgoal = 8 ;
   if (ygoal < 8)
      ygoal = 8 ;
   int xsize = 2 ;
   int ysize = 2 ;
   int currdepth = depth ;
   int i;
   if (root == zerojnode(currdepth)) {
      view.center() ;
      view.setmag(MAX_MAG) ;
      return ;
   }
   vector<jnode *> top, left, bottom, right ;
   top.push_back(root) ;
   left.push_back(root) ;
   bottom.push_back(root) ;
   right.push_back(root) ;
   int topbm = 0, bottombm = 0, rightbm = 0, leftbm = 0 ;
   while (currdepth >= 0) {
      currdepth-- ;
      if (currdepth == 1) { // we have jleaf jnodes; turn them into bitmasks
         topbm = getbitsfromleaves(top) & 0xff ;
	 bottombm = getbitsfromleaves(bottom) & 0xff ;
	 leftbm = getbitsfromleaves(left) >> 8 ;
	 rightbm = getbitsfromleaves(right) >> 8 ;
      }
      if (currdepth <= 1) {
	 int sz = 1 << (currdepth + 2) ;
	 int maskhi = (1 << sz) - (1 << (sz >> 1)) ;
	 int masklo = (1 << (sz >> 1)) - 1 ;
         ymax += ymax ;
	 if ((topbm & maskhi) == 0) {
	    ymax.add_smallint(-2) ;
	    ysize-- ;
	 } else {
	    topbm >>= (sz >> 1) ;
	 }
         ymin += ymin ;
	 if ((bottombm & masklo) == 0) {
	   ymin.add_smallint(2) ;
	   ysize-- ;
	   bottombm >>= (sz >> 1) ;
	 }
         xmax += xmax ;
	 if ((rightbm & masklo) == 0) {
	    xmax.add_smallint(-2) ;
	    xsize-- ;
	    rightbm >>= (sz >> 1) ;
	 }
         xmin += xmin ;
	 if ((leftbm & maskhi) == 0) {
	   xmin.add_smallint(2) ;
	   xsize-- ;
	 } else {
	   leftbm >>= (sz >> 1) ;
	 }
         xsize <<= 1 ;
         ysize <<= 1 ;
      } else {
         jnode *z = 0 ;
         if (hashed)
            z = zerojnode(currdepth) ;
         vector<jnode *> newv ;
         int outer = 0 ;
         for (i=0; i<(int)top.size(); i++) {
            jnode *t = top[i] ;
            if (!outer && (t->nw != z || t->ne != z)) {
               newv.clear() ;
               outer = 1 ;
            }
            if (outer) {
               if (t->nw != z)
                  newv.push_back(t->nw) ;
               if (t->ne != z)
                  newv.push_back(t->ne) ;
            } else {
               if (t->sw != z)
                  newv.push_back(t->sw) ;
               if (t->se != z)
                  newv.push_back(t->se) ;
            }
         }
         top = newv ;
         newv.clear() ;
         ymax += ymax ;
         if (!outer) {
            ymax.add_smallint(-2) ;
            ysize-- ;
         }
         outer = 0 ;
         for (i=0; i<(int)bottom.size(); i++) {
            jnode *t = bottom[i] ;
            if (!outer && (t->sw != z || t->se != z)) {
               newv.clear() ;
               outer = 1 ;
            }
            if (outer) {
               if (t->sw != z)
                  newv.push_back(t->sw) ;
               if (t->se != z)
                  newv.push_back(t->se) ;
            } else {
               if (t->nw != z)
                  newv.push_back(t->nw) ;
               if (t->ne != z)
                  newv.push_back(t->ne) ;
            }
         }
         bottom = newv ;
         newv.clear() ;
         ymin += ymin ;
         if (!outer) {
            ymin.add_smallint(2) ;
            ysize-- ;
         }
         ysize *= 2 ;
         outer = 0 ;
         for (i=0; i<(int)right.size(); i++) {
            jnode *t = right[i] ;
            if (!outer && (t->ne != z || t->se != z)) {
               newv.clear() ;
               outer = 1 ;
            }
            if (outer) {
               if (t->ne != z)
                  newv.push_back(t->ne) ;
               if (t->se != z)
                  newv.push_back(t->se) ;
            } else {
               if (t->nw != z)
                  newv.push_back(t->nw) ;
               if (t->sw != z)
                  newv.push_back(t->sw) ;
            }
         }
         right = newv ;
         newv.clear() ;
         xmax += xmax ;
         if (!outer) {
            xmax.add_smallint(-2) ;
            xsize-- ;
         }
         outer = 0 ;
         for (i=0; i<(int)left.size(); i++) {
            jnode *t = left[i] ;
            if (!outer && (t->nw != z || t->sw != z)) {
               newv.clear() ;
               outer = 1 ;
            }
            if (outer) {
               if (t->nw != z)
                  newv.push_back(t->nw) ;
               if (t->sw != z)
                  newv.push_back(t->sw) ;
            } else {
               if (t->ne != z)
                  newv.push_back(t->ne) ;
               if (t->se != z)
                  newv.push_back(t->se) ;
            }
         }
         left = newv ;
         newv.clear() ;
         xmin += xmin ;
         if (!outer) {
            xmin.add_smallint(2) ;
            xsize-- ;
         }
         xsize *= 2 ;
      }
      if (xsize > xgoal || ysize > ygoal)
         break ;
   }
   xmin >>= 1 ;
   xmax >>= 1 ;
   ymin >>= 1 ;
   ymax >>= 1 ;
   xmin <<= (currdepth + 1) ;
   ymin <<= (currdepth + 1) ;
   xmax <<= (currdepth + 1) ;
   ymax <<= (currdepth + 1) ;
   xmax -= 1 ;
   ymax -= 1 ;
   ymin.mul_smallint(-1) ;
   ymax.mul_smallint(-1) ;
   if (!force) {
      // if all four of the above dimensions are in the viewport, don't change
      if (view.contains(xmin, ymin) && view.contains(xmax, ymax))
         return ;
   }
   xmin += xmax ;
   xmin >>= 1 ;
   ymin += ymax ;
   ymin >>= 1 ;
   int mag = - currdepth - 1 ;
   while (xsize <= xgoal && ysize <= ygoal && mag < MAX_MAG) {
      mag++ ;
      xsize *= 2 ;
      ysize *= 2 ;
   }
   view.setpositionmag(xmin, ymin, mag) ;
}
void jvnalgo::lowerRightPixel(bigint &x, bigint &y, int mag) {
   if (mag >= 0)
     return ;
   x >>= -mag ;
   x <<= -mag ;
   y -= 1 ;
   y >>= -mag ;
   y <<= -mag ;
   y += 1 ;
}
