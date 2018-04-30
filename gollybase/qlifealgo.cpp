// This file is part of Golly.
// See docs/License.html for the copyright notice.

/*
 *   Inspired by Alan Hensel's Life applet and also by xlife.  Tries to
 *   improve the cache, TLB, and branching behavior for modern CPUs.
 */
#include "qlifealgo.h"
#include "liferules.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <iostream>
using namespace std ;
/*
 *   The ai array is used to figure out the index number of the bit set in
 *   the set [1, 2, 4, 8, 16, 32, 64, 128].  Also, for the value 0, it
 *   returns the result 4, to eliminate a conditional in some obscure piece
 *   of code.
 */
static unsigned char ai[129] ;
/*
 *   This define is the size of memory to ask for at one time.  8K is a good
 *   size; we drop 16 bits because malloc overhead is probably near this.
 *
 *   Values much smaller than this will be impacted by malloc overhead (both
 *   speed and space); values much larger than this will occupy excessive
 *   memory for small universes.
 */
#define MEMCHUNK (8192-16)
/*
 *   When we need a bunch more structures of a particular size, we call this.
 *   This code allocates the memory, adds it to our universe memory allocated
 *   list, tries to maximize the cache alignment of the structures, and then
 *   links all the substructures together into a linked list which is then
 *   returned.
 */
/*
 *   This preprocessor directive is used to work around a bug in
 *   register allocation when using function inlining (-O3 or
 *   better) and gcc 3.4.2, which is very common having shipped with
 *   Fedora Core 3.
 */
#ifdef __GNUC__
__attribute__((noinline))
#endif
linkedmem *qlifealgo::filllist(int size) {
   usedmemory += MEMCHUNK ;
   if (maxmemory != 0 && usedmemory > maxmemory)
      lifefatal("exceeded user-specified memory limit") ;
   linkedmem *p, *safep, *r = (linkedmem *)calloc(MEMCHUNK, 1) ;
   int i = size & - size ;
   if (r == 0)
      lifefatal("No memory.") ;
   r->next = memused ;
   memused = r ;
   safep = p = (linkedmem *)((((g_uintptr_t)(r+1))+i-1)&-i) ;
   while (((g_uintptr_t)p) + 2 * size <= MEMCHUNK+(g_uintptr_t)r) {
      p->next = (linkedmem *)(size + (g_uintptr_t)p) ;
      p = (linkedmem *)(size + (g_uintptr_t)p) ;
   }
   return safep ;
}
#ifdef STATS
static int bricks, tiles, supertiles, rcc, dq, ds, rccs, dqs, dss ;
#define STAT(a) a
#else
#define STAT(a)
#endif
/*
 *   If we need a new empty brick, we call this.  This structure is guaranteed
 *   to be all zeros.
 */
brick *qlifealgo::newbrick() {
   brick *r ;
   if (bricklist == 0)
      bricklist = filllist(sizeof(brick)) ;
   r = (brick *)(bricklist) ;
   bricklist = bricklist->next ;
   memset(r, 0, sizeof(brick)) ;
   STAT(bricks++) ;
   return r ;
}
/*
 *   If we need a new tile, we call this.  The structure is also initialized
 *   appropriately, with all the pointers pointing to the empty brick.
 */
tile *qlifealgo::newtile() {
   tile *r ;
   if (tilelist == 0)
      tilelist = filllist(sizeof(tile)) ;
   r = (tile *)(tilelist) ;
   tilelist = tilelist->next ;
   r->b[0] = r->b[1] = r->b[2] = r->b[3] = emptybrick ;
   r->flags = -1 ;
   STAT(tiles++) ;
   return r ;
}
/*
 *   Finally, a new supertile is provided by this routine.  It initializes
 *   all the subtiles to point to the next level down's empty tile.
 */
supertile *qlifealgo::newsupertile(int lev) {
   supertile *r ;
   if (supertilelist == 0)
      supertilelist = filllist(sizeof(supertile)) ;
   r = (supertile *)supertilelist ;
   supertilelist = supertilelist->next ;
   r->d[0] = r->d[1] = r->d[2] = r->d[3] = r->d[4] = r->d[5] =
                                 r->d[6] = r->d[7] = nullroots[lev-1] ;
   STAT(supertiles++) ;
   return r ;
}
/*
 *   This short little subroutine plays a very important role.  It takes bits
 *   set up according to the c01 or c10 fields of supertiles, and translates
 *   these bits into the impact on the next level up.  Essentially, it swaps
 *   the parallel bits (bits 9 through 16), any of them set, with the edge
 *   bit (bit 8), in a way that requires no conditional branches.  On a
 *   slower older processor without large branch penalties, it might be
 *   faster to use a conditional variation that executes fewer instructions,
 *   but actually this code is not totally performance critical.
 */
static int upchanging(int x) {
   int a = (x & 0x1feff) + 0x1feff ;
   return ((a >> 8) & 1) | ((a >> 16) & 2) | ((x << 1) & 0x200) |
          ((x >> 7) & 0x400) ;
}
/*
 *   If it is determined that the universe is not large enough, this
 *   subroutine adds another level to it, expanding it by a factor of 8
 *   in one dimension, depending on whether the level is even or odd.
 *
 *   It also allocates a new emptytile at the appropriate level.
 *
 *   The old root is always placed at position 4.  This allows expansion
 *   in both positive and negative directions.
 *
 *   The sequence of sizes is as follows:
 *
 *   0 31
 *   -128 127
 *   -1,152 895
 *   -9,344 7,039
 *   -74,880 56,191
 *   -599,168 449,407
 *   -4,793,472 3,595,135
 *   -38,347,904 28,760,959
 *   -306,783,360 230,087,551
 *   INT_MIN 1,840,700,287
 *   INT_MIN INT_MAX
 *
 *   Remember these are only relevant for set() calls and have nothing
 *   to do with rendering or generation.
 */
void qlifealgo::uproot() {
   if (min < -100000000)
      min = INT_MIN ;
   else
      min = 8 * min - 128 ;
   if (max > 500000000)
      max = INT_MAX ;
   else
      max = 8 * max - 121 ;
   bmin <<= 3 ;
   bmin -= 128 ;
   bmax <<= 3 ;
   bmax -= 121 ;
   minlow32 = 8 * minlow32 - 4 ;
   if (rootlev >= 38)
     lifefatal("internal:  push too deep for qlifealgo") ;
   for (int i=0; i<2; i++) {
     supertile *oroot = root ;
     rootlev++ ;
     root = newsupertile(rootlev) ;
     if (rootlev > 1)
       root->flags = 0xf0000000 |
         (upchanging(oroot->flags) << (3 + (generation.odd()))) ;
     root->d[4] = oroot ;
     if (oroot != nullroot) {
       nullroots[rootlev] = nullroot = newsupertile(rootlev) ;
     } else {
       nullroots[rootlev] = nullroot = root ;
     }
   }
   // Need to clear this because we don't have valid population values
   // in the new root.
   popValid = 0 ;
}
/*
 *   This subroutine allocates a new empty universe.  The universe starts
 *   out as a 256x256 universe.
 */
qlifealgo::qlifealgo() {
   int test = (INT_MAX != 0x7fffffff) ;
   if (test)
      lifefatal("bad platform for this program") ;
   memused = 0 ;
   maxmemory = 0 ;
   clearall() ;
}
/*
 *   Clear everything.  This one also frees memory.
 */
static int bc[256] ; // popcount
void qlifealgo::clearall() {
   poller->bailIfCalculating() ;
   while (memused) {
      linkedmem *nu = memused->next ;
      free(memused) ;
      memused = nu ;
   }
   generation = 0 ;
   increment = 1 ;
   tilelist = 0 ;
   supertilelist = 0 ;
   bricklist = 0 ;
   rootlev = 0 ;
   cleandowncounter = 63 ;
   usedmemory = 0 ;
   deltaforward = 0 ;
   ai[0] = 4 ; ai[1] = 0 ; ai[2] = 1 ; ai[4] = 2 ; ai[8] = 3 ;
   ai[16] = 4 ; ai[32] = 5 ; ai[64] = 6 ; ai[128] = 7 ;
   minlow32 = min = 0 ;
   max = 31 ;
   bmin = 0 ;
   bmax = 31 ;
   emptybrick = newbrick() ;
   nullroots[0] = nullroot = root = (supertile *)(emptytile = newtile()) ;
   uproot() ;
   popValid = 0 ;
   llxb = 0 ;
   llyb = 0 ;
   llbits = 0 ;
   llsize = 0 ;
   if (bc[255] == 0)
     for (int i=1; i<256; i++)
       bc[i] = bc[i & (i-1)] + 1 ;
}
/*
 *   This subroutine frees a universe.
 */
qlifealgo::~qlifealgo() {
   while (memused) {
      linkedmem *nu = memused->next ;
      free(memused) ;
      memused = nu ;
   }
}
/*
 *   Set the max memory
 */
void qlifealgo::setMaxMemory(int newmemlimit) {
   // AKT: allow setting maxmemory to 0
   if (newmemlimit == 0) {
      maxmemory = 0 ;
      return;
   }
   if (newmemlimit < 10)
      newmemlimit = 10 ;
#ifndef GOLLY64BIT
   else if (newmemlimit > 4000)
      newmemlimit = 4000 ;
#endif
   g_uintptr_t newlimit = ((g_uintptr_t)newmemlimit) << 20 ;
   if (usedmemory > newlimit) {
      lifewarning("Sorry, more memory currently used than allowed.") ;
      return ;
   }
   maxmemory = newlimit ;
}
/*
 *   Finally, our first generation subroutine!  This one handles supertiles
 *   for even to odd generation (0->1).  What is passed in is the universe
 *   itself, the tile (this) to focus on, its three neighbor tiles (the
 *   one `parallel' to it [by the way the subtiles are stacked], the one
 *   past it, and the corner one.
 *
 *   The way we walk down the tree in this subroutine is one of the major
 *   keys to cache and TLB performance.  We walk the universe in a
 *   spatially local way, always doing an entire 32x32 tile before
 *   moving on, always doing a 256x32 supertile before moving on, always
 *   doing a 256x256 supertile, etc.  That is, if we need to access a
 *   particular brick four times in recomputing four bricks, chances are
 *   good that those four times will occur closely in time since we do
 *   spatially adjacent bricks near each other.
 *
 *   Another trick is to do the universe from bottom to top in phase 0->1
 *   and from top to bottom in phase 1->0; this also helps cut down on
 *   those cache misses.
 */
int qlifealgo::doquad01(supertile *zis, supertile *edge,
                        supertile *par, supertile *cor, int lev) {
/*
 *   First we figure out which subtiles we need to recalculate.  There will
 *   always be at least one if we got into this subroutine (except for the
 *   case of a static universe and at the root level).  To do this, we
 *   use the edge bits from the parallel supertile, and one bit each from
 *   the other two neighbor tiles, blending them into a single 8-bit
 *   recalculate int.
 *
 *   Note that the parallel and corner have already been recomputed so
 *   their changing bits are shifted up 10 positions in c.
 */
   poller->poll() ;
   int changing = (zis->flags | (par->flags >> 19) |
                   (((edge->flags >> 18) | (cor->flags >> 27)) & 1)) & 0xff ;
   int x, b, nchanging = (zis->flags & 0x3ff00) << 10 ;
   supertile *p, *pf, *pu, *pfu ;
   STAT(ds++) ;
/*
 *   Only if the first subtile needs to be recomputed do we actually need to
 *   `visit' the edge and corner neighbors.  We always keep track of the
 *   subtiles one level down.
 */
   if (changing & 1) {
      x = 7 ;
      b = 1 ;
      pf = edge->d[0] ;
      pfu = cor->d[0] ;
   } else {
/*
 *   Otherwise, we compute which tile we need to examine first with the help
 *   of the ai array.
 */
      b = (changing & - changing) ;
      x = 7 - ai[b] ;
      pf = zis->d[x + 1] ;
      pfu = par->d[x + 1] ;
   }
   for (;;) {
      p = zis->d[x] ;
      pu = par->d[x] ;
/*
 *   Do we need to recompute this subtile?
 */
      if (changing & b) {
/*
 *   If so, is it the canonical empty supertile for this level?  If so,
 *   allocate a new empty supertile and set the void bits appropriately.
 */
         if (zis->d[x] == nullroots[lev-1])
            p = zis->d[x] = (lev == 1 ? (supertile *)newtile() :
                                                      newsupertile(lev-1)) ;
/*
 *   If it's level 1, call the tile handler, else call the next level down of
 *   the supertile handler.  The return value is the changing indicators that
 *   should be propogated up.
 */
         nchanging |= ((lev == 1) ? p01((tile *)p, (tile *)pf,
                                        (tile *)pu, (tile *)pfu) :
                                    doquad01(p, pu, pf, pfu, lev-1)) << x ;
         changing -= b ;
      } else if (changing == 0)
         break ;
      b <<= 1 ;
      x-- ;
      pfu = pu ;
      pf = p ;
   }
   zis->flags = nchanging | 0xf0000000 ;
   return upchanging(nchanging) ;
}
/*
 *   This is for odd to even generations, and the documentation is pretty
 *   much the same as for the previous subroutine.
 */
int qlifealgo::doquad10(supertile *zis, supertile *edge,
                        supertile *par, supertile *cor, int lev) {
   poller->poll() ;
   int changing = (zis->flags | (par->flags >> 19) |
                   (((edge->flags >> 18) | (cor->flags >> 27)) & 1)) & 0xff ;
   int x, b, nchanging = (zis->flags & 0x3ff00) << 10 ;
   supertile *p, *pf, *pu, *pfu ;
   STAT(ds++) ;
   if (changing & 1) {
      x = 0 ;
      b = 1 ;
      pf = edge->d[7] ;
      pfu = cor->d[7] ;
   } else {
      b = (changing & - changing) ;
      x = ai[b] ;
      pf = zis->d[x - 1] ;
      pfu = par->d[x - 1] ;
   }
   for (;;) {
      p = zis->d[x] ;
      pu = par->d[x] ;
      if (changing & b) {
         if (zis->d[x] == nullroots[lev-1])
            p = zis->d[x] = (lev == 1 ? (supertile *)newtile() :
                                                     newsupertile(lev-1)) ;
         nchanging |= ((lev == 1) ? p10((tile *)pfu, (tile *)pu,
                                       (tile *)pf, (tile *)p) :
                                  doquad10(p, pu, pf, pfu, lev-1)) << (7-x) ;
         changing -= b ;
      } else if (changing == 0)
         break ;
      b <<= 1 ;
      x++ ;
      pfu = pu ;
      pf = p ;
   }
   zis->flags = nchanging | 0xf0000000 ;
   return upchanging(nchanging) ;
}
/*
 *   This is our monster subroutine that, with its mirror below, accounts for
 *   about 90% of the runtime.  It handles recomputation for a 32x32 tile.
 *   Passed in are the neighbor tiles:  pr (to the right), pd (down), and
 *   prd (down and to the right).
 */
int qlifealgo::p01(tile *p, tile *pr, tile *pd, tile *prd) {
   brick *db = pd->b[0], *rdb = prd->b[0] ;
/*
 *   Do we need to recompute the fourth brick?  This happens here because its
 *   the only place we need to pull in changing from the down and corner
 *   neighbor.
 */
   int i, recomp = (p->c[4] | pd->c[0] | (pr->c[4] >> 9) | (prd->c[0] >> 8)) & 0xff ;
   STAT(dq++) ;
   p->c[5] = 0 ;
   p->flags |= 0xfff00000 ;
/*
 *   For each brick . . .
 */
   for (i=3; i>=0; i--) {
      brick *b = p->b[i], *rb = pr->b[i] ;
/*
 *   Do we need to recompute?
 */
      if (recomp) {
         unsigned int traildata, trailunderdata ;
         int j, cdelta = 0, maska, maskb, maskprev = 0 ;
/*
 *   If so, set the dirty bit.  Also, if this brick is the canonical empty
 *   brick, get a new one.
 */
         p->flags |= 1 << i ;
         if (b == emptybrick)
            p->b[i] = b = newbrick() ;
/*
 *   If we need to recompute the end slice, now is a good time to get the
 *   right neighbor's data.
 */
         if (recomp & 1) {
            j = 7 ;
            traildata = rb->d[0] ;
            trailunderdata = rdb->d[0] ;
         } else {
/*
 *   Otherwise we use the ai[] array to figure out where to begin in this
 *   brick.
 */
            j = ai[recomp & - recomp] ;
            recomp >>= j ;
            j = 7 - j ;
            traildata = b->d[j+1] ;
            trailunderdata = db->d[j+1] ;
         }
         trailunderdata = (traildata << 8) + (trailunderdata >> 24) ;
         for (;;) {
/*
 *   At all times here, we have traildata (the data from the slice to the
 *   right) and trailunderdata (24 bits of traildata and eight bits from
 *   the slice under and to the right).
 *
 *   Do we need  to recompute this slice?
 */
            if (recomp & 1) {
/*
 *   Our main recompute chunk recomputes a single slice.
 */
               unsigned int zisdata = b->d[j] ;
               unsigned int underdata = (zisdata << 8) + (db->d[j] >> 24) ;
               unsigned int otherdata = ((zisdata << 2) & 0xcccccccc) +
                                        ((traildata >> 2) & 0x33333333) ;
               unsigned int otherunderdata = ((underdata << 2) & 0xcccccccc) +
                                    ((trailunderdata >> 2) & 0x33333333) ;
               int newv = (ruletable[zisdata >> 16] << 26) +
                          (ruletable[underdata >> 16] << 18) +
                          (ruletable[zisdata & 0xffff] << 10) +
                          (ruletable[underdata & 0xffff] << 2) +
                          (ruletable[otherdata >> 16] << 24) +
                          (ruletable[otherunderdata >> 16] << 16) +
                          (ruletable[otherdata & 0xffff] << 8) +
                           ruletable[otherunderdata & 0xffff] ;
/*
 *   Has anything changed?
 *   Keep track of what has changed in the entire cell, the rightmost
 *   two columns, the lowest two rows, and the lowest rightmost 2x2 cell, into
 *   the maskprev int.  Do all of this without conditionals.
 */
               int delta = (b->d[j + 8] ^ newv) | deltaforward ;
               STAT(rcc++) ;
               b->d[j + 8] = newv ;
               maska = cdelta | (delta & 0x33333333) ;
               maskb = maska | -maska ;
               maskprev = (maskprev << 1) |
                          ((maskb >> 9) & 0x400000) | (maskb & 0x80) ;
               cdelta = delta ;
               traildata = zisdata ;
               trailunderdata = underdata ;
            } else {
/*
 *   No need to recompute?  Well, maintain our necessary invariants and bail
 *   if we're done.
 */
               maskb = cdelta | -cdelta ;
               maskprev = (maskprev << 1) |
                          ((maskb >> 9) & 0x400000) | (maskb & 0x80) ;
               if (recomp == 0)
                  break ; ;
               cdelta = 0 ;
               traildata = b->d[j] ;
               trailunderdata = (traildata << 8) + (db->d[j] >> 24) ;
            }
            recomp >>= 1 ;
            j-- ;
         }
/*
 *   Finally done with that brick!  Update our changing for the next
 *   call to p10, and or-in any changes to the lower two rows that we saw
 *   into the next brick down's changing variable.
 */
         p->c[i+2] |= (maskprev >> (6 - j)) & 0x1ff ;
         p->c[i+1] =
           (short)(((p->c[i+1] & 0x100) << 1) | (maskprev >> (21 - j))) ;
      } else
         p->c[i+1] = 0 ;
/*
 *   Calculate recomp for the next row down.
 */
      recomp = (p->c[i] | (pr->c[i] >> 9)) & 0xff ;
      db = b ;
      rdb = rb ;
   }
/*
 *   Propogate the changing information for this tile to the supertile on
 *   the next level up.
 */
   recomp = p->c[5] ;
   i = recomp | p->c[0] | p->c[1] | p->c[2] | p->c[3] | p->c[4] ;
   if (recomp)
      return 0x201 | ((recomp & 0x100) << 2) | ((i & 0x100) >> 7) ;
   else
      return i ? ((i & 0x100) >> 7) | 1 : 0 ;
}
/*
 *   This subroutine is the mirror of the one above, used for odd to even
 *   generations.
 */
int qlifealgo::p10(tile *plu, tile *pu, tile *pl, tile *p) {
   brick *ub = pu->b[3], *lub = plu->b[3] ;
   int i, recomp = (p->c[1] | pu->c[5] | (pl->c[1] >> 9) | (plu->c[5] >> 8)) & 0xff ;
   STAT(dq++) ;
   p->c[0] = 0 ;
   p->flags |= 0x000fff00 ;
   for (i=0; i<=3; i++) {
      brick *b = p->b[i], *lb = pl->b[i] ;
      if (recomp) {
         int maska, maskprev = 0, j, cdelta = 0 ;
         unsigned int traildata, trailoverdata ;
         p->flags |= 1 << i ;
         if (b == emptybrick)
            p->b[i] = b = newbrick() ;
         if (recomp & 1) {
            j = 0 ;
            traildata = lb->d[15] ;
            trailoverdata = lub->d[15] ;
         } else {
            j = ai[recomp & - recomp] ;
            traildata = b->d[j+7] ;
            trailoverdata = ub->d[j+7] ;
            recomp >>= j ;
         }
         trailoverdata = (traildata >> 8) + (trailoverdata << 24) ;
         for (;;) {
            if (recomp & 1) {
               unsigned int zisdata = b->d[j + 8] ;
               unsigned int overdata = (zisdata >> 8) + (ub->d[j + 8] << 24) ;
               unsigned int otherdata = ((zisdata >> 2) & 0x33333333) +
                                        ((traildata << 2) & 0xcccccccc) ;
               unsigned int otheroverdata = ((overdata >> 2) & 0x33333333) +
                                    ((trailoverdata << 2) & 0xcccccccc) ;
               int newv = (ruletable[otheroverdata >> 16] << 26) +
                          (ruletable[otherdata >> 16] << 18) +
                          (ruletable[otheroverdata & 0xffff] << 10) +
                          (ruletable[otherdata & 0xffff] << 2) +
                          (ruletable[overdata >> 16] << 24) +
                          (ruletable[zisdata >> 16] << 16) +
                          (ruletable[overdata & 0xffff] << 8) +
                           ruletable[zisdata & 0xffff] ;
               int delta = (b->d[j] ^ newv) | deltaforward ;
               STAT(rcc++) ;
               maska = cdelta | (delta & 0xcccccccc) ;
               maskprev = (maskprev << 1) |
                          (((maska | - maska) >> 9) & 0x400000) |
                          ((((maska >> 24) | 0x100) - 1) & 0x100) ;
               b->d[j] = newv ;
               cdelta = delta ;
               traildata = zisdata ;
               trailoverdata = overdata ;
            } else {
               maskprev = (maskprev << 1) |
                          (((cdelta | - cdelta) >> 9) & 0x400000) |
                          ((((cdelta >> 24) | 0x100) - 1) & 0x100) ;
               if (recomp == 0)
                  break ;
               cdelta = 0 ;
               traildata = b->d[j + 8] ;
               trailoverdata = (traildata >> 8) + (ub->d[j + 8] << 24) ;
            }
            recomp >>= 1 ;
            j++ ;
         }
         p->c[i+1] =
           (short)(((p->c[i+1] & 0x100) << 1) | (maskprev >> (14 + j))) ;
         p->c[i] |= (maskprev >> j) & 0x1ff ;
      } else
         p->c[i+1] = 0 ;
      recomp = (p->c[i+2] | (pl->c[i+2] >> 9)) & 0xff ;
      ub = b ;
      lub = lb ;
   }
   recomp = p->c[0] ;
   i = recomp | p->c[1] | p->c[2] | p->c[3] | p->c[4] | p->c[5] ;
   if (recomp)
      return 0x201 | ((recomp & 0x100) << 2) | ((i & 0x100) >> 7) ;
   else
      return i ? ((i & 0x100) >> 7) | 1 : 0 ;
}
/**
 *   Mark a node and its subnodes as changed.  We really
 *   only mark those nodes that have any cells set at all.
 */
int qlifealgo::markglobalchange(supertile *p, int lev) {
   int i ;
   if (lev == 0) {
      tile *pp = (tile *)p ;
      if (pp != emptytile) {
        int s = 0 ;
        for (int i=0; i<4; i++)
          for (int j=0; j<16; j++)
            s |= pp->b[i]->d[j] ;
        if (s) {
          pp->c[0] = pp->c[5] = 0x1ff ;
          pp->c[1] = pp->c[2] = pp->c[3] = pp->c[4] = 0x3ff ;
          return 0x603 ;
        }
      }
      return 0 ;
   } else {
      if (p != nullroots[lev]) {
         int nchanging = 0 ;
         if (generation.odd())
           for (i=0; i<8; i++)
              nchanging |= (markglobalchange(p->d[i], lev-1)) << i ;
         else
           for (i=0; i<8; i++)
             nchanging |= (markglobalchange(p->d[i], lev-1)) << (7 - i) ;
         p->flags |= nchanging | 0xf0000000 ;
         return upchanging(nchanging) ;
      }
      return 0 ;
   }
}
void qlifealgo::markglobalchange() {
   markglobalchange(root, rootlev) ;
   deltaforward = 0xffffffff ;
}
/*
 *   This subroutine sets a bit at a particular location.
 *
 *   We walk down the tree to the particular bit, setting changing flags as
 *   we go.
 */
int qlifealgo::setcell(int x, int y, int newstate) {
   if (newstate & ~1)
      return -1 ;
   y = - y ;
   supertile *b ;
   tile *p ;
   int lev ;
   int odd = generation.odd() ;
   if (odd) {
      x-- ;
      y-- ;
   }
   while (x < min || x > max || y < min || y > max)
      uproot() ;
   int xdel = (x >> 5) - minlow32 ;
   int ydel = (y >> 5) - minlow32 ;
   int xc = x - (minlow32 << 5) ;
   int yc = y - (minlow32 << 5) ;
   if (root == nullroot)
      root = newsupertile(rootlev) ;
   b = root ;
   lev = rootlev ;
   while (lev > 0) {
      int i, d = 1 ;
      if (lev & 1) {
         int s = (lev >> 1) + lev - 1 ;
         i = (xdel >> s) & 7 ;
         s = (1 << (s + 5)) - 2 ;
         if ((xc & s) == ((odd) ? s : 0))
            d += 2 ;
         if ((yc & s) == ((odd) ? s : 0))
            d += d << 9 ;
      } else {
         int s = (lev >> 1) + lev - 3 ;
         i = (ydel >> s) & 7 ;
         s = (1 << (s + 5)) - 2 ;
         if ((yc & s) == ((odd) ? s : 0))
            d += 2 ;
         s |= s << 3 ;
         if ((xc & s) == ((odd) ? s : 0))
            d += d << 9 ;
      }
      if (odd)
         b->flags |= (d << i) | 0xf0000000 ;
      else
         b->flags |= (d << (7 - i)) | 0xf0000000 ;
      if (b->d[i] == nullroots[lev-1])
         b->d[i] = (lev==1 ? (supertile *)newtile() :
                                                      newsupertile(lev-1)) ;
      lev -= 1 ;
      b = b->d[i] ;
   }
   x &= 31 ;
   y &= 31 ;
   p = (tile *)b ;
   if (p->b[(y >> 3) & 0x3] == emptybrick)
      p->b[(y >> 3) & 0x3] = newbrick() ;
   if (odd) {
      int mor = ((x & 2) ? 3 : 1) << ((x >> 2) & 0x7) ;
      p->c[((y >> 3) & 0x3) + 1] |= mor ;
      p->flags = -1 ;
      if ((y & 6) == 6)
         p->c[((y >> 3) & 0x3) + 2] |= mor ;
      if (newstate)
         p->b[(y >> 3) & 0x3]->d[8 + ((x >> 2) & 0x7)]
                                   |= (1 << (31 - (y & 7) * 4 - (x & 3))) ;
      else
         p->b[(y >> 3) & 0x3]->d[8 + ((x >> 2) & 0x7)]
                                  &= ~(1 << (31 - (y & 7) * 4 - (x & 3))) ;
   } else {
      int mor = ((x & 2) ? 1 : 3) << (7 - ((x >> 2) & 0x7)) ;
      p->c[((y >> 3) & 0x3) + 1] |= mor ;
      p->flags = -1 ;
      if ((y & 6) == 0)
         p->c[((y >> 3) & 0x3)] |= mor ;
      if (newstate)
         p->b[(y >> 3) & 0x3]->d[(x >> 2) & 0x7]
                                   |= (1 << (31 - (y & 7) * 4 - (x & 3))) ;
      else
         p->b[(y >> 3) & 0x3]->d[(x >> 2) & 0x7]
                                  &= ~(1 << (31 - (y & 7) * 4 - (x & 3))) ;
   }
   deltaforward = 0xffffffff ;
   return 0 ;
}
/*
 *   This subroutine gets a bit at a particular location.
 */
int qlifealgo::getcell(int x, int y) {
   y = - y ;
   supertile *b ;
   tile *p ;
   int lev ;
   int odd = generation.odd() ;
   if (odd) {
      x-- ;
      y-- ;
   }
   while (x < min || x > max || y < min || y > max)
      uproot() ;
   if (x < min || x > max || y < min || y > max)
      return 0 ;
   int xdel = (x >> 5) - minlow32 ;
   int ydel = (y >> 5) - minlow32 ;
   if (root == nullroot)
      return 0 ;
   b = root ;
   lev = rootlev ;
   while (lev > 0) {
      int i ;
      if (lev & 1) {
         int s = (lev >> 1) + lev - 1 ;
         i = (xdel >> s) & 7 ;
      } else {
         int s = (lev >> 1) + lev - 3 ;
         i = (ydel >> s) & 7 ;
      }
      if (b->d[i] == nullroots[lev-1])
         return 0 ;
      lev -= 1 ;
      b = b->d[i] ;
   }
   x &= 31 ;
   y &= 31 ;
   p = (tile *)b ;
   if (p->b[(y >> 3) & 0x3] == emptybrick)
      return 0 ;
   if (odd) {
      if (p->b[(y >> 3) & 0x3]->d[8 + ((x >> 2) & 0x7)] &
          (1 << (31 - (y & 7) * 4 - (x & 3))))
         return 1 ;
      else
         return 0 ;
   } else {
      if (p->b[(y >> 3) & 0x3]->d[(x >> 2) & 0x7] &
          (1 << (31 - (y & 7) * 4 - (x & 3))))
         return 1 ;
      else
         return 0 ;
   }
}
/**
 *   Similar but returns the distance to the next set cell horizontally.
 */
int qlifealgo::nextcell(int x, int y, int &v) {
   v = 1 ;
   y = - y ;
   int odd = generation.odd() ;
   if (odd) {
      x-- ;
      y-- ;
   }
   while (x < min || x > max || y < min || y > max)
      uproot() ;
   if (x > max || x < min || y < min || y > max)
      return -1 ;
   return nextcell(x, y, root, rootlev) ;
}
int qlifealgo::nextcell(int x, int y, supertile *n, int lev) {
   if (lev > 0) {
      if (n == nullroots[lev])
         return -1 ;
      int xdel = (x >> 5) - minlow32 ;
      int ydel = (y >> 5) - minlow32 ;
      int i ;
      if (lev & 1) {
         int s = (lev >> 1) + lev - 1 ;
         i = (xdel >> s) & 7 ;
         int r = 0 ;
         int off = (x & 31) + ((xdel & ((1 << s) - 1)) << 5) ;
         while (i < 8) {
            int t = nextcell(x, y, n->d[i], lev-1) ;
            if (t < 0) {
              r += (32 << s) - off ;
              x += (32 << s) - off ;
              off = 0 ;
            } else {
              return r + t ;
            }
            i++ ;
         }
         return -1 ;
      } else {
         int s = (lev >> 1) + lev - 3 ;
         i = (ydel >> s) & 7 ;
         return nextcell(x, y, n->d[i], lev-1) ;
      }
   }
   x &= 31 ;
   y &= 31 ;
   tile *p = (tile *)n ;
   brick *br = (brick *)(p->b[(y>>3) & 3]) ;
   if (br == emptybrick)
      return -1 ;
   int i = ((x >> 2) & 7) ;
   int add = (generation.odd() ? 8 : 0) ;
   int sh = (7 - (y & 7)) * 4 ;
   int r = 0 ;
   x &= 3 ;
   int m = 15 >> x ;
   while (i < 8) {
     int t = (br->d[i+add] >> sh) & m ;
     if (t) {
       if (t & 8) return r - x ;
       if (t & 4) return r + 1 - x ;
       if (t & 2) return r + 2 - x ;
       return r + 3 - x ;
     }
     r += (4 - x) ;
     x = 0 ;
     m = 15 ;
     i++ ;
   }
   return -1 ;
}
/*
 *   This subroutine calculates the population count of the universe.  It
 *   uses dirty bits number 1 and 2 of supertiles.
 */
G_INT64 qlifealgo::find_set_bits(supertile *p, int lev, int gm1) {
   G_INT64 pop = 0 ;
   int i, j, b ;
   if (lev == 0) {
      tile *pp = (tile *)p ;
      b = 8 + gm1 * 12 ;
      pop = (pp->flags >> b) & 0xfff ;
      if (pop > 0x800) {
         pop = 0 ;
         for (i=0; i<4; i++) {
            if (pp->b[i] != emptybrick) {
               for (j=0; j<8; j++) {
#ifdef FASTPOPCOUNT
                  pop += FASTPOPCOUNT(pp->b[i]->d[j+gm1*8]) ;
#else
                  int k = pp->b[i]->d[j+gm1*8] ;
                  if (k)
                    pop += bc[k & 255] + bc[(k >> 8) & 255] +
                      bc[(k >> 16) & 255] + bc[(k >> 24) & 255] ;
#endif
               }
            }
         }
         pp->flags = (long)((pp->flags & ~(0xfff << b)) | (pop << b)) ;
      }
   } else {
      if (p->flags & (0x20000000 << gm1)) {
         for (i=0; i<8; i++)
            if (p->d[i] != nullroots[lev-1])
               pop += find_set_bits(p->d[i], lev-1, gm1) ;
         if (pop < 500000000) {
            p->pop[gm1] = (int)pop ;
            p->flags &= ~(0x20000000 << gm1) ;
         } else {
            p->pop[gm1] = 0xfffffff ; // placeholder; *some* bits are set
         }
      } else {
         pop = p->pop[gm1] ;
      }
   }
   return pop ;
}
/**
 *   A variation that tries to quickly answer:  *any* bits set?
 */
int qlifealgo::isEmpty(supertile *p, int lev, int gm1) {
   int i, j, k, b ;
   if (lev == 0) {
      tile *pp = (tile *)p ;
      b = 8 + gm1 * 12 ;
      int pop = (pp->flags >> b) & 0xfff ;
      if (pop > 0x800) {
         pop = 0 ;
         for (i=0; i<4; i++) {
            if (pp->b[i] != emptybrick) {
               for (j=0; j<8; j++) {
                  k = pp->b[i]->d[j+gm1*8] ;
                  if (k)
                     return 0 ;
               }
            }
         }
      }
      return pop ? 0 : 1 ;
   } else {
      if (p->flags & (0x20000000 << gm1)) {
         for (i=0; i<8; i++)
            if (p->d[i] != nullroots[lev-1])
               if (!isEmpty(p->d[i], lev-1, gm1))
                  return 0 ;
         return 1 ;
      } else {
         return p->pop[gm1] ? 0 : 1 ;
      }
   }
}
/*
 *   Another critical subroutine, this one cleans up the empty bricks,
 *   tiles, and supertiles as the generations go by.  This speeds things
 *   up by not using too much memory (minimizing cache misses and TLB
 *   misses).  We only try to delete bricks, tiles, and supertiles from
 *   regions of the universe that have been active since we last attempted
 *   to delete tiles.  We delete all possible tiles, even those near active
 *   regions; if necessary, 
 *
 *   We use dirty bit number 0 of supertiles, and dirty bits 0..3 of
 *   tiles.
 */
supertile *qlifealgo::mdelete(supertile *p, int lev) {
   int i ;
   if (lev == 0) {
      tile *pp = (tile *)p ;
      if (pp->flags & 0xf) {
         int seen = 0 ;
         for (i=0; i<4; i++) {
            brick *b = pp->b[i] ;
            if (b != emptybrick) {
               if ((pp->flags & (1 << i))) {
                  if (b->d[0] | b->d[1] | b->d[2] | b->d[3] | b->d[4] |
                      b->d[5] | b->d[6] | b->d[7] | b->d[8] | b->d[9] |
                      b->d[10] | b->d[11] | b->d[12] | b->d[13] | b->d[14] |
                      b->d[15]) {
                     seen++ ;
                  } else {
                     STAT(bricks--) ;
                     ((linkedmem *)b)->next = bricklist ;
                     bricklist = (linkedmem *)b ;
                     pp->b[i] = emptybrick ;
                  }
               } else
                  seen++ ;
            }
         }
         if (seen || ((pp->c[1] | pp->c[2] | pp->c[3] | pp->c[4]) & 0xff) ||
                                 ((generation.odd()) ? pp->c[5] : pp->c[0]))
            pp->flags &= 0xfffffff0 ;
         else {
            STAT(tiles--) ;
            memset(pp, 0, sizeof(tile)) ;
            ((linkedmem *)pp)->next = tilelist ;
            tilelist = (linkedmem *)pp ;
            return nullroots[lev] ;
         }
      }
   } else {
      if (p->flags & 0x10000000) {
         int keep = 0 ;
         for (i=0; i<8; i++)
            if (p->d[i] != nullroots[lev-1])
               if ((p->d[i] = mdelete(p->d[i], lev-1)) !=
                                                       nullroots[lev-1])
                  keep++ ;
         if (keep || p == root || (p->flags & 0x3ffff))
            p->flags &= 0xefffffff ;
         else {
            STAT(supertiles--) ;
            memset(p, 0, sizeof(supertile)) ;
            ((linkedmem *)p)->next = supertilelist ;
            supertilelist = (linkedmem *)p ;
            return nullroots[lev] ;
         }
      }
   }
   return p ;
}
G_INT64 qlifealgo::popcount() {
   return find_set_bits(root, rootlev, generation.odd()) ;
}
const bigint &qlifealgo::getPopulation() {
   if (!popValid) {
      population = bigint(popcount()) ;
      popValid = 1 ;
      poller->reset_countdown() ;
   }
   return population ;
}
int qlifealgo::isEmpty() {
   return isEmpty(root, rootlev, generation.odd()) ;
}
/*
 *   Here we look at the root node and see if activity is getting
 *   uncomfortably close to the current edges.  If so, we add another
 *   level onto the top.
 */
int qlifealgo::uproot_needed() {
   int i ;
   if (root->d[0] != nullroots[rootlev-1] ||
       root->d[7] != nullroots[rootlev-1])
      return 1 ;
   for (i=1; i<7; i++)
      if (root->d[i]->d[0] != nullroots[rootlev-2] ||
          root->d[i]->d[7] != nullroots[rootlev-2])
         return 1 ;
   return 0 ;
}
/*
 *   The new generation code is simple.  We uproot if needed.  Then, we call
 *   the appropriate top-level slice code depending on the generation number.
 *   Finally, if we are generations 64, 128, 192, and so on, we clean up
 *   the tree.
 *
 *   Note that this 64 was carefully chosen to balance extraneous bricks left
 *   behind by gliders against the computational cost of deletion.
 */
void qlifealgo::dogen() {
   poller->reset_countdown() ;
#ifdef STATS
   ds = 0 ; dq = 0 ; rcc = 0 ;
#endif
   // AKT: if grid is bounded then we should never need to call uproot() here
   // because setrule() has already expanded the universe to enclose the grid
   if (gridwd == 0 || gridht == 0) {
      while (uproot_needed())
         uproot() ;
   }
   if (generation.odd())
      doquad10(root, nullroot, nullroot, nullroot, rootlev) ;
   else
      doquad01(root, nullroot, nullroot, nullroot, rootlev) ;
   deltaforward = 0 ;
   generation += bigint::one ;
   popValid = 0 ;
   if (--cleandowncounter == 0) {
      cleandowncounter = 63 ;
      mdelete(root, rootlev) ;
   }
#ifdef STATS
   dss += ds ; dqs += dq ; rccs += rcc ;
#endif
}
/**
 *   Step.  Do increment generations.
 */
void qlifealgo::step() {
   poller->bailIfCalculating() ;
   bigint t = increment ;
   while (t != 0) {
      if (qliferules.alternate_rules) {
         // emulate B0-not-Smax rule by changing rule table depending on gen parity
         if (generation.odd())
            ruletable = qliferules.rule1 ;
         else
            ruletable = qliferules.rule0 ;
      } else {
         ruletable = qliferules.rule0 ;
      }
      dogen() ;
      if (poller->isInterrupted())
         break ;
      t -= 1 ;
      if (t > increment) // might change; make it happen now
         t = increment ;
   }
}

// Flip bits in given rule table.
// This is a tad tricky because we want to turn both the input
// and the output of this table upside down.
static void fliprule(char *rptr) {
   for (int i=0; i<65536; i++) {
      int j = ((i & 0xf) << 12) +
               ((i & 0xf0) << 4) + ((i & 0xf00) >> 4) + ((i & 0xf000) >> 12) ;
      if (i <= j) {
         char fi = rptr[i] ;
         char fj = rptr[j] ;
         fi = ((fi & 0x30) >> 4) + ((fi & 0x3) << 4) ;
         fj = ((fj & 0x30) >> 4) + ((fj & 0x3) << 4) ;
         rptr[i] = fj ;
         rptr[j] = fi ;
      }
   }
}

/**
 *   If we change the rule we need to mark everything dirty.
 */
const char *qlifealgo::setrule(const char *s) {
   const char* err = qliferules.setrule(s, this);
   if (err) return err;

   markglobalchange() ;
   
   // AKT: qlifealgo has an opposite interpretation of the orientation of
   // a rule table assumed by qliferules.setrule.  For vertically symmetrical
   // rules such as the Moore or von Neumann neighborhoods this doesn't matter,
   // but for hexagonal rules and Wolfram rules we need to flip the rule table(s)
   // upside down.
   if ( qliferules.isHexagonal() || qliferules.isWolfram() ) {
      if (qliferules.alternate_rules) {
         // hex rule has B0 but not S6 so we'll be using rule1 for odd gens
         fliprule(qliferules.rule1);
      }
      fliprule(qliferules.rule0);
   }
   
   // ruletable is set in step(), but play safe
   ruletable = qliferules.rule0 ;
   
   if (qliferules.isHexagonal())
      grid_type = HEX_GRID;
   else if (qliferules.isVonNeumann())
      grid_type = VN_GRID;
   else
      grid_type = SQUARE_GRID;

   // AKT: if the grid is bounded then call uproot() if necessary so that
   // dogen() never needs to call it
   if (gridwd > 0 && gridht > 0) {
      // use the top left and bottom right corners of the grid, but expanded by 2
      // to allow for growth in the borders when the grid edges are joined
      int xmin = -int(gridwd/2) - 2;
      int ymin = -int(gridht/2) - 2;
      int xmax = xmin + gridwd + 3;
      int ymax = ymin + gridht + 3;
      // duplicate the expansion code in setcell()
      ymin = -ymin;
      ymax = -ymax;
      if (generation.odd()) {
         xmin--;
         ymin--;
         xmax--;
         ymax--;
      }
      // min is -ve, max is +ve, xmin is -ve, xmax is +ve, ymin is +ve, ymax is -ve
      while (xmin < min || xmax > max || ymin > max || ymax < min)
         uproot();
   }
   
   return 0;
}

static lifealgo *creator() { return new qlifealgo() ; }

void qlifealgo::doInitializeAlgoInfo(staticAlgoInfo &ai) {
   ai.setAlgorithmName("QuickLife") ;
   ai.setAlgorithmCreator(&creator) ;
   ai.setDefaultBaseStep(10) ;
   ai.setDefaultMaxMem(0) ;
   ai.minstates = 2 ;
   ai.maxstates = 2 ;
   // init default color scheme
   ai.defgradient = false;
   ai.defr1 = ai.defg1 = ai.defb1 = 255;        // start color = white
   ai.defr2 = ai.defg2 = ai.defb2 = 255;        // end color = white
   ai.defr[0] = ai.defg[0] = ai.defb[0] = 48;   // 0 state = dark gray
   ai.defr[1] = ai.defg[1] = ai.defb[1] = 255;  // 1 state = white
}
