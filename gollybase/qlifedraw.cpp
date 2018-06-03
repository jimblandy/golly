// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "qlifealgo.h"
#include <cstring>
#include <cstdlib>
#include "util.h"

const int logbmsize = 8 ;                   // *must* be 8 in this code
const int bmsize = (1<<logbmsize) ;
const int ibufsize = (bmsize*bmsize/32) ;
static unsigned int ibigbuf[ibufsize] ;     // shared buffer for 256x256 pixels
static unsigned char *bigbuf = (unsigned char *)ibigbuf ;

// AKT: 256x256 pixmap where each pixel is 4 RGBA bytes
static unsigned char pixbuf[bmsize*bmsize*4];

// AKT: RGBA values for cell states (see getcolors call)
static unsigned char deadr, deadg, deadb, deada;
static unsigned char liver, liveg, liveb, livea;

// rowett: RGBA view of cell states
static unsigned int liveRGBA, deadRGBA;

void qlifealgo::renderbm(int x, int y) {
   renderbm(x, y, bmsize, bmsize) ;
}

void qlifealgo::renderbm(int x, int y, int xsize, int ysize) {
   // x,y is lower left corner
   int rx = x ;
   int ry = y ;
   int rw = xsize ;
   int rh = ysize ;
   if (pmag > 1) {
      rx *= pmag ;
      ry *= pmag ;
      rw *= pmag ;
      rh *= pmag ;
   }
   ry = uviewh - ry - rh ;

   unsigned char *bigptr = bigbuf;
   if (renderer->justState() || pmag > 1) {
      // convert each bigbuf byte into 8 bytes of state data
      unsigned char *pixptr = pixbuf;

      for (int i = 0; i < ibufsize * 4; i++) {
         unsigned char byte = *bigptr++;
         *pixptr++ = (byte & 128) ? 1 : 0;
         *pixptr++ = (byte & 64) ? 1 : 0;
         *pixptr++ = (byte & 32) ? 1 : 0;
         *pixptr++ = (byte & 16) ? 1 : 0;
         *pixptr++ = (byte & 8) ? 1 : 0;
         *pixptr++ = (byte & 4) ? 1 : 0;
         *pixptr++ = (byte & 2) ? 1 : 0;
         *pixptr++ = (byte & 1);    // no condition needed
      }
   } else {
      // convert each bigbuf byte into 32 bytes of pixel data (8 * RGBA)
      // get RGBA view of pixel buffer
      unsigned int *pixptr = (unsigned int *)pixbuf;

      for (int i = 0; i < ibufsize * 4; i++) {
         unsigned char byte = *bigptr++;
         *pixptr++ = (byte & 128) ? liveRGBA : deadRGBA;
         *pixptr++ = (byte & 64) ? liveRGBA : deadRGBA;
         *pixptr++ = (byte & 32) ? liveRGBA : deadRGBA;
         *pixptr++ = (byte & 16) ? liveRGBA : deadRGBA;
         *pixptr++ = (byte & 8) ? liveRGBA : deadRGBA;
         *pixptr++ = (byte & 4) ? liveRGBA : deadRGBA;
         *pixptr++ = (byte & 2) ? liveRGBA : deadRGBA;
         *pixptr++ = (byte & 1) ? liveRGBA : deadRGBA;
      }
   }
   if (renderer->justState())
      renderer->stateblit(rx, ry, rw, rh, pixbuf) ;
   else
      renderer->pixblit(rx, ry, rw, rh, pixbuf, pmag);

   memset(bigbuf, 0, sizeof(ibigbuf)) ;
}

static int minlevel;
/*
 *   We cheat for now; we assume we can use 32-bit ints.  We can below
 *   a certain level; we'll deal with higher levels later.
 */
void qlifealgo::BlitCells(supertile *p,
                          int xoff, int yoff, int wd, int ht, int lev) {
   int i, xinc=0, yinc=0, ypos, x, yy;
   int liveseen = 0 ;
   
   if (xoff >= vieww || xoff + wd < 0 || yoff >= viewh || yoff + ht < 0)
      // no part of this supertile is visible
      return;

   if (p == nullroots[lev]) {
      return;
   }

   // do recursion until we get to level 2 (256x256 supertile)
   if (lev > 2) {
      if (lev & 1) {
         // odd level -- 8 subtiles are stacked horizontally
         xinc = wd = ht;
      } else {
         // even level -- 8 subtiles are stacked vertically
         yinc = ht = (ht >> 3);
      }
      for (i=0; i<8; i++) {
         BlitCells(p->d[i], xoff, yoff, wd, ht, lev-1);
         xoff += xinc;
         yoff += yinc;
      }
      return;
   }

   // walk a (probably) non-empty 256x256 supertile, finding all the 1 bits and
   // setting corresponding bits in the bitmap (bigbuf)
   liveseen = 0 ;
   ypos = yoff;
   // examine the 8 vertically stacked subtiles in this 256x256 supertile (at level 2)
   for (yy=0; yy<8; yy++) {
      if (p->d[yy] != nullroots[1] && ypos < viewh && ypos + 32 >= 0) {
         supertile *psub = p->d[yy];
         x = xoff;
         // examine the 8 tiles in this 256x32 supertile (at level 1)
         for (i=0; i<8; i++) {
            if (psub->d[i] != nullroots[0] && x < vieww && x + 32 >= 0) {
               tile *t = (tile *)psub->d[i];
               int j, k, y = ypos;
               // examine the 4 bricks in this 32x32 tile (at level 0)
               for (j=0; j<4; j++) {
                  if (t->b[j] != emptybrick && y < viewh && y + 8 >= 0) {
                     brick *b = t->b[j];
                     // examine the 8 slices (2 at a time) in the appropriate half-brick
                     for (k=0; k<8; k+=2) {
                        unsigned int v1 = b->d[k+kadd];
                        unsigned int v2 = b->d[k+kadd+1];
                        if (v1|v2) {
                           // do an 8x8 set of bits (2 adjacent slices)
                           int xd = (i<<2)+(k>>1);
                           int yd = (7 - yy) << 10;         // 1024 bytes in 256x32 supertile
                           unsigned char *p = bigbuf + yd + xd + ((3 - j) << 8);
      
                           unsigned int v3 = (((v1 & 0x0f0f0f0f) << 4) |
                                               (v2 & 0x0f0f0f0f));
                           v2 = ((v1 & 0xf0f0f0f0) | ((v2 >> 4) & 0x0f0f0f0f));
                           *p     = (unsigned char)v3;
                           p[32]  = (unsigned char)v2;
                           p[64]  = (unsigned char)(v3 >> 8);
                           p[96]  = (unsigned char)(v2 >> 8);
                           p[128] = (unsigned char)(v3 >> 16);
                           p[160] = (unsigned char)(v2 >> 16);
                           p[192] = (unsigned char)(v3 >> 24);
                           p[224] = (unsigned char)(v2 >> 24);
                           
                           liveseen |= (1 << yy) ;
                        }
                     }
                  }
                  y += 8;   // down to next brick
               }
            }
            x += 32;   // across to next tile
         }
      }
      ypos += 32;   // down to next subtile
   }

   if (liveseen == 0) {
      return;                  // no live cells seen
   }

   // performance:  if we want, liveseen now contains eight bits
   // corresponding to whether those respective 256x32 rectangles
   // contain set pixels or not.  We should trim the bitmap
   // to only render those portions that need to be rendered
   // (using this information).   -tom
   //
   // draw the non-empty bitmap, scaling up if pmag > 1
   renderbm(xoff, yoff) ;
}

// This pattern drawing routine is used when mag > 0.
// We go down to a level where what we're going to draw maps to one
// of 256x256, 128x128, or 64x64.
//
// We no longer rely on popcount having been called; instead we invoke
// the popcount child if needed.

void qlifealgo::ShrinkCells(supertile *p,
                            int xoff, int yoff, int wd, int ht, int lev) {
   int i ;
   if (lev >= bmlev) {
      if (xoff >= vieww || xoff + wd < 0 || yoff >= viewh || yoff + ht < 0)
         // no part of this supertile/tile is visible
         return ;
      if (p == nullroots[lev]) {
         return ;
      }
      if (lev == bmlev) {
         bmleft = xoff ;
         bmtop = yoff ;
      }
   } else {
      if (p == nullroots[lev])
         return ;
   }
   int bminc = -1 << (logshbmsize-3) ;
   unsigned char *bm = bigbuf + (((shbmsize-1)-yoff+bmtop) << (logshbmsize-3)) +
                                  ((xoff-bmleft) >> 3) ;
   int bit = 128 >> ((xoff-bmleft) & 7) ;
   // do recursion until we get to minimum level (which depends on mag)
   if (lev > minlevel) {
      int xinc = 0, yinc = 0 ;
      if (lev & 1) {
         // odd level -- 8 subtiles are stacked horizontally
         xinc = wd ;
         wd = ht ;
      } else {
         // even level -- 8 subtiles are stacked vertically
         yinc = ht ;
         ht = (ht >> 3);
      }
      int xxinc = 0 ;
      int yyinc = 0 ;
      if (yinc <= 8 && xinc == 0) {
//   This is a case where we need to traverse multiple nodes for a
//   single pixel.  This can be really slow, especially if we have
//   to examine 4x4=16 nodes just for a single pixel!  To help
//   mitigate that, we special-case this and do the entire square,
//   tracking which pixels are set and not traversing when we
//   would set a pixel again.
         xinc = yinc ;
         int sh = 0 ;
         if (xinc == 8)
            sh = 0 ;
         else if (xinc == 4)
            sh = 1 ;
         else if (xinc == 2)
            sh = 2 ;
         for (i=0; i<8; i++) {
            if (p->d[i] != nullroots[lev-1]) {
               supertile *pp = p->d[i] ;
               int bbit = bit ;
               for (int j=0; j<8; j++) {
                  if (pp->d[j] != nullroots[lev-2]) {
                     if (0 == (*bm & bbit)) {
                        supertile *ppp = pp->d[j] ;
                        if (lev > 2) {
                           if ( ppp->pop[oddgen] != 0 )
                              *bm |= bbit ;
                        } else {
                           tile *t = (tile *)ppp ;
                           if (t->flags & quickb)
                              *bm |= bbit ;
                        }
                     }
                  }
                  if ((j ^ (j + 1)) >> sh)
                     bbit >>= 1 ;
               }
            }
            if ((i ^ (i + 1)) >> sh)
               bm += bminc ;
         }
         return ;
      } else {
         for (i=0; i<8; i++) {
            ShrinkCells(p->d[i], xoff + (xxinc >> 3), yoff + (yyinc >> 3),
                        wd, ht, lev-1);
            xxinc += xinc ;
            yyinc += yinc ;
         }
         if (lev == bmlev)
            renderbm(bmleft, bmtop, shbmsize, shbmsize) ;
      }
   } else if (mag > 4) {
      if (lev > 0) {
         // mag >= 8
         if ( p->pop[oddgen] != 0 )
            *bm |= bit ;
      } else {
         // mag = 5..7
         tile *t = (tile *)p;
         if (t->flags & quickb)
            *bm |= bit ;
      }
   } else {
     switch (mag) {
      case 4: {
         // shrink 32x32 tile to 2x2 pixels
         tile *t = (tile *)p;
         if ((t->b[0] != emptybrick || t->b[1] != emptybrick) ) {
            brick *bt = t->b[0];
            brick *bb = t->b[1];
            // examine the top left 16x16 cells
            if ( bt->d[kadd+0] | bt->d[kadd+1] | bt->d[kadd+2] | bt->d[kadd+3] |
                 bb->d[kadd+0] | bb->d[kadd+1] | bb->d[kadd+2] | bb->d[kadd+3] )
                  // shrink 16x16 cells to 1 pixel
               *bm |= bit ;
            // examine the top right 16x16 cells
            if ( bt->d[kadd+4] | bt->d[kadd+5] | bt->d[kadd+6] | bt->d[kadd+7] |
                 bb->d[kadd+4] | bb->d[kadd+5] | bb->d[kadd+6] | bb->d[kadd+7] )
               // shrink 16x16 cells to 1 pixel
               *bm |= bit >> 1 ;
         }
         bm += bminc ;
         if (t->b[2] != emptybrick || t->b[3] != emptybrick) {
            brick *bt = t->b[2];
            brick *bb = t->b[3];
            // examine the bottom left 16x16 cells
            if ( bt->d[kadd+0] | bt->d[kadd+1] | bt->d[kadd+2] | bt->d[kadd+3] |
                 bb->d[kadd+0] | bb->d[kadd+1] | bb->d[kadd+2] | bb->d[kadd+3] )
               // shrink 16x16 cells to 1 pixel
               *bm |= bit ;
            // examine the bottom right 16x16 cells
            if ( bt->d[kadd+4] | bt->d[kadd+5] | bt->d[kadd+6] | bt->d[kadd+7] |
                 bb->d[kadd+4] | bb->d[kadd+5] | bb->d[kadd+6] | bb->d[kadd+7] )
               // shrink 16x16 cells to 1 pixel
               *bm |= bit >> 1 ;
         }
      }
      break;
      
      case 3: {
         // mag = 3 so shrink 32x32 tile to 4x4 pixels
         tile *t = (tile *)p;
         int j ;
         // examine the 4 bricks in this 32x32 tile
         for (j=0; j<4; j++) {
            if (t->b[j] != emptybrick) {
               brick *b = t->b[j];
               int k ;
               // examine the 8 slices (2 at a time) in the appropriate half-brick
               int bbit = bit;
               for (k=0; k<8; k += 2) {
                  if ( (b->d[k+kadd] | b->d[k+kadd+1]) ) *bm |= bbit ;
                  bbit >>= 1 ;
               }
            }
            bm += bminc ;
         }
      }
      break;

      case 2: {
         // mag = 2 so shrink 32x32 tile to 8x8 pixels
         tile *t = (tile *)p;
         int j ;
         // examine the 4 bricks in this 32x32 tile
         for (j=0; j<4; j++) {
            if (t->b[j] != emptybrick) {
               brick *b = t->b[j];
               int k ;
               bit = 128 ;
               // examine the 8 slices in the appropriate half-brick
               for (k=0; k<8; k++) {
                  unsigned int s = b->d[k+kadd];
                  // s represents a 4x8 slice so examine top and bottom halves
                  if (s) {
                     if (s & 0xFFFF0000) *bm |= bit ;
                     if (s & 0x0000FFFF) bm[bminc] |= bit ;
                  }
                  bit >>= 1 ;
               }
            }
            bm += 2 * bminc ;
         }
      }
      break;

      case 1: {
         // mag = 1 so shrink 32x32 tile to 16x16 pixels
         tile *t = (tile *)p;
         int j ;
         // examine the 4 bricks in this 32x32 tile
         unsigned char *bmm = bm ;
         for (j=0; j<4; j++) {
            if (t->b[j] != emptybrick) {
               brick *b = t->b[j];
               int k ;
               bit = 128 ;
               // examine the 8 slices in the appropriate half-brick
               for (k=0; k<8; k++) {
                  unsigned int s = b->d[k+kadd];
                  if (s) {
                     // s is a non-empty 4x8 slice so shrink each 2x2 section to 1 pixel
                     if (s & 0xCC000000) *bmm |= bit ;
                     if (s & 0x00CC0000) bmm[bminc] |= bit ;
                     if (s & 0x0000CC00) bmm[bminc+bminc] |= bit ;
                     if (s & 0x000000CC) bmm[bminc+bminc+bminc] |= bit ;
                     bit >>= 1 ;
                     if (s & 0x33000000) *bmm |= bit ;
                     if (s & 0x00330000) bmm[bminc] |= bit ;
                     if (s & 0x00003300) bmm[bminc+bminc] |= bit ;
                     if (s & 0x00000033) bmm[bminc+bminc+bminc] |= bit ;
                     bit >>= 1 ;
                  } else {
                     bit >>= 2 ;
                  }
                  if (bit < 1) {
                     bmm++ ;
                     bit = 128 ;
                  }
               }
               bmm -= 2 ;
            }
            bmm += 4 * bminc ;
         }
      }
      break;
    }
   }
}
/*
 *   Fill in the llxb and llyb bits from the viewport information.
 *   Allocate if necessary.  This arithmetic should be done carefully.
 */
void qlifealgo::fill_ll(int d) {
   pair<bigint, bigint> coor = view->at(0, view->getymax()) ;
   coor.second.mul_smallint(-1) ;
   coor.first -= bmin ;
   coor.second -= bmin ;
   if (oddgen) {
      coor.first -= 1 ;
      coor.second -= 1 ;
   }
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
void qlifealgo::draw(viewport &viewarg, liferender &renderarg) {
   memset(bigbuf, 0, sizeof(ibigbuf)) ;
   renderer = &renderarg ;

   if (!renderer->justState()) {
      // AKT: get cell colors and alpha values for dead and live pixels
      unsigned char *r, *g, *b;
      renderer->getcolors(&r, &g, &b, &deada, &livea);
      deadr = r[0];
      deadg = g[0];
      deadb = b[0];
      liver = r[1];
      liveg = g[1];
      liveb = b[1];

      // create RGBA live color
      unsigned char *colptr = (unsigned char *)&liveRGBA;
      *colptr++ = liver;
      *colptr++ = liveg;
      *colptr++ = liveb;
      *colptr++ = livea;

      // create RGBA dead color
      colptr = (unsigned char *)&deadRGBA;
      *colptr++ = deadr;
      *colptr++ = deadg;
      *colptr++ = deadb;
      *colptr++ = deada;
   }
      
   view = &viewarg ;
   uvieww = view->getwidth() ;
   uviewh = view->getheight() ;
   oddgen = getGeneration().odd() ;
   kadd = oddgen ? 8 : 0 ;
   int xoff, yoff ;
   if (view->getmag() > 0) {
      pmag = 1 << (view->getmag()) ;
      mag = 0 ;
      viewh = ((uviewh - 1) >> view->getmag()) + 1 ;
      vieww = ((uvieww - 1) >> view->getmag()) + 1 ;
      uviewh += (-uviewh) & (pmag-1) ;
   } else {
      mag = (-view->getmag()) ;
      // cheat for now since unzoom is broken
      pmag = 1 ;
      viewh = uviewh ;
      vieww = uvieww ;
   }
   if (root == nullroots[rootlev]) {
      renderer = 0 ;
      view = 0 ;
      return ;
   }
   int d = 5 + (rootlev + 1) / 2 * 3 ;
   fill_ll(d) ;
   int maxd = vieww ;
   supertile *sw = root, *nw = nullroots[rootlev], *ne = nullroots[rootlev],
     *se = nullroots[rootlev] ;
   if (viewh > maxd)
     maxd = viewh ;
   int llx=-llxb[llbits-1], lly=-llyb[llbits-1] ;
/*   Skip down to top of tree. */
   int i;
   for (i=llbits-1; i>=d && i>=mag; i--) { /* go down to d, but not further than mag */
      llx = (llx << 1) + llxb[i] ;
      lly = (lly << 1) + llyb[i] ;
      if (llx > 2*maxd || lly > 2*maxd || llx < -2*maxd || lly < -2*maxd) {
         renderer = 0 ;
         view = 0 ;
         return ;
      }
   }
   /*  Find the lowest four we need to examine */
   int curlev = rootlev ;
   while (d > 8 && d - mag > 2 &&
          (d - mag > 28 || (1 << (d - mag)) > 32 * maxd)) {
      // d is 5 + 3 * i for some positive i
      llx = (llx << 3) + (llxb[d-1] << 2) + (llxb[d-2] << 1) + llxb[d-3] ;
      lly = (lly << 3) + (llyb[d-1] << 2) + (llyb[d-2] << 1) + llyb[d-3] ;
      int xp = llx ;
      if (xp < 0)
         xp = 0 ;
      else if (xp > 7)
         xp = 7 ;
      int yp = lly ;
      if (yp < 0)
         yp = 0 ;
      else if (yp > 7)
         yp = 7 ;
      if (xp == 7) {
         if (yp == 7) {
            ne = ne->d[0]->d[0] ;
            se = se->d[7]->d[0] ;
            nw = nw->d[0]->d[7] ;
            sw = sw->d[7]->d[7] ;
         } else {
            ne = se->d[yp+1]->d[0] ;
            se = se->d[yp]->d[0] ;
            nw = sw->d[yp+1]->d[7] ;
            sw = sw->d[yp]->d[7] ;
         }
      } else {
         if (yp == 7) {
            ne = nw->d[0]->d[xp+1] ;
            se = sw->d[7]->d[xp+1] ;
            nw = nw->d[0]->d[xp] ;
            sw = sw->d[7]->d[xp] ;
         } else {
            ne = sw->d[yp+1]->d[xp+1] ;
            se = sw->d[yp]->d[xp+1] ;
            nw = sw->d[yp+1]->d[xp] ;
            sw = sw->d[yp]->d[xp] ;
         }
      }
      llx -= xp ;
      lly -= yp ;
      if (llx > 2*maxd || lly > 2*maxd || llx < -2*maxd || lly < -2*maxd) {
         renderer = 0 ;
         view = 0 ;
         return ;
      }
      d -= 3 ;
      curlev -= 2 ;
   }
   find_set_bits(nw, curlev, oddgen) ;
   find_set_bits(ne, curlev, oddgen) ;
   find_set_bits(sw, curlev, oddgen) ;
   find_set_bits(se, curlev, oddgen) ;
// getPopulation() ;
   /*  At this point we know we can use 32-bit arithmetic. */
   for (i=d-1; i>=mag; i--) {
      llx = (llx << 1) + llxb[i] ;
      lly = (lly << 1) + llyb[i] ;
   }
   /* now, we have four nodes to draw.  the ll point in
      screen coordinates is given by llx/lly.  the ur point
      in screen coordinates is given by that plus 2 << (d-mag).
   */
   xoff = -llx ;
   yoff = -lly ;
   int wd = 2 ;
   if (d >= mag)
      wd = 2 << (d-mag) ;
   int yoffuht = yoff + wd ;
   int xoffuwd = xoff + wd ;
   if (yoff >= viewh || xoff >= vieww || yoffuht < 0 || xoffuwd < 0) {
      renderer = 0 ;
      view = 0 ;
      return ;
   }
   int levsize = wd / 2 ;
   // do recursive drawing
   quickb = 0xfff << (8 + oddgen * 12) ;
   if (mag > 0) {
      bmlev = (1 + mag / 3) * 2 ;
      logshbmsize = 8 - (mag % 3) ;
      shbmsize = 1 << logshbmsize ;
      if (mag < 5) {
         // recurse down to 32x32 tiles
         minlevel = 0;
      } else {
         // if mag = 5..7 minlevel = 0 (32x32 tiles)
         // if mag = 8..10 minlevel = 2 (256x256 supertiles)
         // if mag = 11..13 minlevel = 4 (2048x2048 supertiles) etc...
         minlevel = ((mag - 5) / 3) * 2;
      }
      bmleft = xoff ;
      bmtop = yoff ;
      ShrinkCells(sw, xoff, yoff, levsize, levsize, curlev);
      ShrinkCells(se, xoff+levsize, yoff, levsize, levsize, curlev);
      ShrinkCells(nw, xoff, yoff+levsize, levsize, levsize, curlev);
      ShrinkCells(ne, xoff+levsize, yoff+levsize, levsize, levsize, curlev);
      if (bmlev > curlev)
         renderbm(bmleft, bmtop, shbmsize, shbmsize) ;
   } else {
      // recurse down to 256x256 supertiles and use bitmap blitting
      BlitCells(sw, xoff, yoff, levsize, levsize, curlev);
      BlitCells(se, xoff+levsize, yoff, levsize, levsize, curlev);
      BlitCells(nw, xoff, yoff+levsize, levsize, levsize, curlev);
      BlitCells(ne, xoff+levsize, yoff+levsize, levsize, levsize, curlev);
   }
   renderer = 0 ;
   view = 0 ;
}
/**
 *   Find the subsupertiles with the smallest indices.
 */
int qlifealgo::lowsub(vector<supertile*> &src, vector<supertile*> &dst,
                      int lev) {
  int lowlev = 7 ;
  dst.clear() ;
  supertile *z = nullroots[lev-1] ;
  if (lev > 1) {
    for (int i=0; i<(int)src.size(); i++) {
      supertile *p = src[i] ;
      for (int j=0; j<lowlev; j++)
        if (p->d[j] != z && (p->d[j]->pop[oddgen])) {
          lowlev = j ;
          dst.clear() ;
        }
      if (p->d[lowlev] != z && (p->d[lowlev]->pop[oddgen]))
        dst.push_back(p->d[lowlev]) ;
    }
  } else {
    for (int i=0; i<(int)src.size(); i++) {
      supertile *p = src[i] ;
      for (int j=0; j<lowlev; j++)
        if (p->d[j] != z && (((tile *)(p->d[j]))->flags & quickb)) {
          lowlev = j ;
          dst.clear() ;
        }
      if (p->d[lowlev] != z && (((tile *)(p->d[lowlev]))->flags & quickb))
        dst.push_back(p->d[lowlev]) ;
    }
  }
  return lowlev ;
}
/**
 *   Find the subsupertiles with the highest indices.
 */
int qlifealgo::highsub(vector<supertile*> &src, vector<supertile*> &dst,
                       int lev) {
  int highlev = 0 ;
  dst.clear() ;
  supertile *z = nullroots[lev-1] ;
  if (lev > 1) {
    for (int i=0; i<(int)src.size(); i++) {
      supertile *p = src[i] ;
      for (int j=7; j>highlev; j--)
        if (p->d[j] != z && (p->d[j]->pop[oddgen])) {
          highlev = j ;
          dst.clear() ;
        }
      if (p->d[highlev] != z && (p->d[highlev]->pop[oddgen]))
        dst.push_back(p->d[highlev]) ;
    }
  } else {
    for (int i=0; i<(int)src.size(); i++) {
      supertile *p = src[i] ;
      for (int j=7; j>highlev; j--)
        if (p->d[j] != z && (((tile *)(p->d[j]))->flags & quickb)) {
          highlev = j ;
          dst.clear() ;
        }
      if (p->d[highlev] != z && (((tile *)(p->d[highlev]))->flags & quickb))
        dst.push_back(p->d[highlev]) ;
    }
  }
  return highlev ;
}
/**
 *   Find all nonzero sub-supertiles.
 */
void qlifealgo::allsub(vector<supertile*> &src, vector<supertile*> &dst,
                       int lev) {
  dst.clear() ;
  supertile *z = nullroots[lev-1] ;
  if (lev > 1) {
    for (int i=0; i<(int)src.size(); i++) {
      supertile *p = src[i] ;
      for (int j=0; j<8; j++)
        if (p->d[j] != z && (p->d[j]->pop[oddgen]))
          dst.push_back(p->d[j]) ;
    }
  } else {
    for (int i=0; i<(int)src.size(); i++) {
      supertile *p = src[i] ;
      for (int j=0; j<8; j++)
        if (p->d[j] != z && (((tile *)(p->d[j]))->flags & quickb))
          dst.push_back(p->d[j]) ;
    }
  }
}
int qlifealgo::gethbitsfromleaves(vector<supertile *> v) {
  int h[8] ;
  int i;
  for (i=0; i<8; i++)
    h[i] = 0 ;
  for (i=0; i<(int)v.size(); i++) {
    tile *p = (tile *)v[i] ;
    for (int j=0; j<4; j++)
      if (p->b[j] != emptybrick)
        for (int k=0; k<8; k++)
          h[k] |= p->b[j]->d[k+kadd] ;
  }
  int r = 0 ;
  for (i=0; i<8; i++) {
    int v = h[i] ;
    v |= (v >> 16) ;
    v |= (v >> 8) ;
    v |= (v >> 4) ;
    r = (r << 4) | (v & 15) ;
  }
  return r ;
}
int qlifealgo::getvbitsfromleaves(vector<supertile *> vec) {
  int v[4] ;
  int i;
  for (i=0; i<4; i++)
    v[i] = 0 ;
  for (i=0; i<(int)vec.size(); i++) {
    tile *p = (tile *)vec[i] ;
    for (int j=0; j<4; j++)
      if (p->b[j] != emptybrick)
        for (int k=0; k<8; k++)
          v[j] |= p->b[j]->d[k+kadd] ;
  }
  int r = 0 ;
  for (i=3; i>=0; i--) {
    int vv = v[i] ;
    for (int j=0; j<8; j++) {
      r += r ;
      if (vv & (0xf << (4 * j)))
        r++ ;
    }
  }
  return r ;
}

void qlifealgo::findedges(bigint *ptop, bigint *pleft, bigint *pbottom, bigint *pright) {
   // AKT: following code is from fit() but all goal/size stuff
   // has been removed so it finds the exact pattern edges
   bigint xmin = 0 ;
   bigint xmax = 1 ;
   bigint ymin = 0 ;
   bigint ymax = 1 ;
   getPopulation() ; // make sure pop values are valid
   oddgen = getGeneration().odd() ;
   kadd = oddgen ? 8 : 0 ;
   quickb = 0xfff << (8 + oddgen * 12) ;
   int currdepth = rootlev ;
   if (root == nullroots[currdepth] || root->pop[oddgen] == 0) {
      // AKT: return impossible edges to indicate empty pattern;
      // not really a problem because caller should check first
      *ptop = 1 ;
      *pleft = 1 ;
      *pbottom = 0 ;
      *pright = 0 ;
      return ;
   }
   vector<supertile *> top, left, bottom, right ;
   top.push_back(root) ;
   left.push_back(root) ;
   bottom.push_back(root) ;
   right.push_back(root) ;
   int topbm = 0, bottombm = 0, rightbm = 0, leftbm = 0 ;
   int bitval = (currdepth + 1) / 2 * 3 + 5 ;
   while (bitval > 0) {
      if (bitval == 5) { // we have leaf nodes; turn them into bitmasks
         topbm = getvbitsfromleaves(top) ;
         bottombm = getvbitsfromleaves(bottom) ;
         leftbm = gethbitsfromleaves(left) ;
         rightbm = gethbitsfromleaves(right) ;
      }
      if (bitval <= 5) {
         int sz = 1 << bitval ;
         int masklo = (1 << (sz >> 1)) - 1 ;
         int maskhi = ~masklo ;
         ymax += ymax ;
         xmax += xmax ;
         ymin += ymin ;
         xmin += xmin ;
         if ((topbm & maskhi) == 0) {
            ymax.add_smallint(-1) ;
         } else {
            topbm = (topbm >> (sz >> 1)) & masklo ;
         }
         if ((bottombm & masklo) == 0) {
            ymin.add_smallint(1) ;
            bottombm = (bottombm >> (sz >> 1)) & masklo ;
         }
         if ((rightbm & masklo) == 0) {
            xmax.add_smallint(-1) ;
            rightbm = (rightbm >> (sz >> 1)) & masklo ;
         }
         if ((leftbm & maskhi) == 0) {
            xmin.add_smallint(1) ;
         } else {
            leftbm = (leftbm >> (sz >> 1)) & masklo ;
         }
         bitval-- ;
      } else {
         vector<supertile *> newv ;
         int outer = highsub(top, newv, currdepth) ;
         allsub(newv, top, currdepth-1) ;
         ymax <<= 3 ;
         ymax -= (7 - outer) ;
         outer = lowsub(bottom, newv, currdepth) ;
         allsub(newv, bottom, currdepth-1) ;
         ymin <<= 3 ;
         ymin += outer ;
         allsub(left, newv, currdepth) ;
         outer = lowsub(newv, left, currdepth-1) ;
         xmin <<= 3 ;
         xmin += outer ;
         allsub(right, newv, currdepth) ;
         outer = highsub(newv, right, currdepth-1) ;
         xmax <<= 3 ;
         xmax -= (7-outer) ;
         currdepth -= 2 ;
         bitval -= 3 ;
      }
   }
   if (bitval > 0) {
      xmin <<= bitval ;
      ymin <<= bitval ;
      xmax <<= bitval ;
      ymax <<= bitval ;
   }
   if (oddgen) {
      xmin += 1 ;
      ymin += 1 ;
      xmax += 1 ;
      ymax += 1 ;
   }
   xmin += bmin ;
   ymin += bmin ;
   xmax += bmin ;
   ymax += bmin ;
   ymax -= 1 ;
   xmax -= 1 ;
   ymin.mul_smallint(-1) ;
   ymax.mul_smallint(-1) ;
   // set pattern edges
   *ptop = ymax ;          // due to y flip
   *pbottom = ymin ;       // due to y flip
   *pleft = xmin ;
   *pright = xmax ;
}

void qlifealgo::fit(viewport &view, int force) {
   bigint xmin = 0 ;
   bigint xmax = 1 ;
   bigint ymin = 0 ;
   bigint ymax = 1 ;
   getPopulation() ; // make sure pop values are valid
   oddgen = getGeneration().odd() ;
   kadd = oddgen ? 8 : 0 ;
   quickb = 0xfff << (8 + oddgen * 12) ;
   int xgoal = view.getwidth() ;
   int ygoal = view.getheight() ;
   if (xgoal < 8)
      xgoal = 8 ;
   if (ygoal < 8)
      ygoal = 8 ;
   int xsize = 1 ;
   int ysize = 1 ;
   int currdepth = rootlev ;
   if (root == nullroots[currdepth] || root->pop[oddgen] == 0) {
      view.center() ;
      view.setmag(MAX_MAG) ;
      return ;
   }
   vector<supertile *> top, left, bottom, right ;
   top.push_back(root) ;
   left.push_back(root) ;
   bottom.push_back(root) ;
   right.push_back(root) ;
   int topbm = 0, bottombm = 0, rightbm = 0, leftbm = 0 ;
   int bitval = (currdepth + 1) / 2 * 3 + 5 ;
   while (bitval > 0) {
      if (bitval == 5) { // we have leaf nodes; turn them into bitmasks
         topbm = getvbitsfromleaves(top) ;
         bottombm = getvbitsfromleaves(bottom) ;
         leftbm = gethbitsfromleaves(left) ;
         rightbm = gethbitsfromleaves(right) ;
      }
      if (bitval <= 5) {
         int sz = 1 << bitval ;
         int masklo = (1 << (sz >> 1)) - 1 ;
         int maskhi = ~masklo ;
         ymax += ymax ;
         xmax += xmax ;
         ymin += ymin ;
         xmin += xmin ;
         xsize <<= 1 ;
         ysize <<= 1 ;
         if ((topbm & maskhi) == 0) {
            ymax.add_smallint(-1) ;
            ysize-- ;
         } else {
            topbm = (topbm >> (sz >> 1)) & masklo ;
         }
         if ((bottombm & masklo) == 0) {
            ymin.add_smallint(1) ;
            ysize-- ;
            bottombm = (bottombm >> (sz >> 1)) & masklo ;
         }
         if ((rightbm & masklo) == 0) {
            xmax.add_smallint(-1) ;
            xsize-- ;
            rightbm = (rightbm >> (sz >> 1)) & masklo ;
         }
         if ((leftbm & maskhi) == 0) {
            xmin.add_smallint(1) ;
            xsize-- ;
         } else {
            leftbm = (leftbm >> (sz >> 1)) & masklo ;
         }
         bitval-- ;
      } else {
         vector<supertile *> newv ;
         ysize <<= 3 ;
         int outer = highsub(top, newv, currdepth) ;
         allsub(newv, top, currdepth-1) ;
         ymax <<= 3 ;
         ymax -= (7 - outer) ;
         ysize -= (7 - outer) ;
         outer = lowsub(bottom, newv, currdepth) ;
         allsub(newv, bottom, currdepth-1) ;
         ymin <<= 3 ;
         ymin += outer ;
         ysize -= outer ;
         xsize <<= 3 ;
         allsub(left, newv, currdepth) ;
         outer = lowsub(newv, left, currdepth-1) ;
         xmin <<= 3 ;
         xmin += outer ;
         xsize -= outer ;
         allsub(right, newv, currdepth) ;
         outer = highsub(newv, right, currdepth-1) ;
         xmax <<= 3 ;
         xmax -= (7-outer) ;
         xsize -= (7-outer) ;
         currdepth -= 2 ;
         bitval -= 3 ;
      }
      if (xsize > xgoal || ysize > ygoal)
         break ;
   }
   if (bitval > 0) {
      xmin <<= bitval ;
      ymin <<= bitval ;
      xmax <<= bitval ;
      ymax <<= bitval ;
   }
   if (oddgen) {
      xmin += 1 ;
      ymin += 1 ;
      xmax += 1 ;
      ymax += 1 ;
   }
   xmin += bmin ;
   ymin += bmin ;
   xmax += bmin ;
   ymax += bmin ;
   ymax -= 1 ;
   xmax -= 1 ;
   ymin.mul_smallint(-1) ;
   ymax.mul_smallint(-1) ;
   if (!force) {
      // if all four of the above dimensions are in the viewport, don't change
      if (view.contains(xmin, ymin) && view.contains(xmax, ymax))
         return ;
   }
   int mag = - bitval ;
   while (2 * xsize <= xgoal && 2 * ysize <= ygoal && mag < MAX_MAG) {
      mag++ ;
      xsize *= 2 ;
      ysize *= 2 ;
   }
   while (xsize > xgoal || ysize > ygoal) {
      mag-- ;
      xsize /= 2 ;
      ysize /= 2 ;
   }
   view.setpositionmag(xmin, xmax, ymin, ymax, mag) ;
}
/**
 *   Fixed for qlife.
 */
void qlifealgo::lowerRightPixel(bigint &x, bigint &y, int mag) {
   if (mag >= 0)
     return ;
   x -= oddgen ;
   x -= bmin ;
   x >>= -mag ;
   x <<= -mag ;
   x += bmin ;   
   x += oddgen ;
   y -= 1 ;
   y += bmin ;
   y += oddgen ;
   y >>= -mag ;
   y <<= -mag ;
   y -= bmin ;
   y += 1 ;
   y -= oddgen ;
}
