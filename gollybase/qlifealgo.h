// This file is part of Golly.
// See docs/License.html for the copyright notice.

/**
 *   This is the fast "conventional" life algorithm code.
 */
#ifndef QLIFEALGO_H
#define QLIFEALGO_H
#include "lifealgo.h"
#include "liferules.h"
#include <vector>
/*
 *   The smallest unit of the universe is the `slice', which is a
 *   4 (horizontal) by 8 (vertical) chunk of the world.  Each slice
 *   is stored in a 32-bit word.  The most significant bit of the
 *   word is the upper left bit; the remaining bits go across
 *   horizontally then down vertically.
 *
 *   We always recompute in units of a slice.  This is smaller than the
 *   8x8 in some programs, and in this particular case adds to the
 *   efficiency.
 *
 *   This arrangment of bits was selected to minimize the bit
 *   twiddling needed for lookup; in general, we use a lookup table
 *   that takes a 4x4 current-generation and figures out the values
 *   of the inner 2x2 cells.
 *
 *   A `brick' is composed of eight slices for the even generation and
 *   eight slices for the odd generation.  This is a total of 16
 *   32-bit words.  The slices are concatenated horizontally so each
 *   brick contains two generations of a 32x8 section of the universe.
 *
 *   No other data is stored directly into the bricks; various flags are
 *   stored in the next level up.  We try and minimize the size of the
 *   data for a given universe so that we can minimize the number of
 *   data cache misses and TLB misses.  For almost all universes, the
 *   bulk of the allocated data is in these 64-byte bricks.
 *
 *   We use a stagger-step type algorithm to minimize the number of
 *   neighbors that need to be examined (thanks, Alan!) for each slice.
 *   Thus, a brick that holds (0,0)-(31,7) for the even generation would
 *   hold (1,1)-(32,8) in the odd generations.
 */
struct brick { /* 64 bytes */
   unsigned int d[16] ;
} ;
/*
 *   A tile contains the flags for four bricks, and pointers to the bricks
 *   themselves.  These pointers are never null; if the portion of the
 *   universe is completely empty, they point to a unique allocated
 *   `emptybrick'.  This means that we do not need to check if the pointers
 *   are null; we can simply always dereference and get valid data.  Thus,
 *   each tile corresponds to a 32x32 section of the universe.
 *
 *   The c flags indicate that, in the previous computation,
 *   the data was found to change in some slice or
 *   subslice.  The least significant bit in each c01 flag corresponds
 *   to the last (8th) slice for phase 0->1, and to the first slice for
 *   phase 1->0.
 *
 *   There are ten valid bits in c flags 1-4, one for each slice,
 *   one most significant one that indicates that the rightmost (leftmost)
 *   two columns of the first (eighth) slice changed in phase 0->1 (1->0)
 *   (and thus, the next brick to the left (right) needs to be recomputed).
 *   The tenth bit is just the ninth bit from the previous generation.
 *
 *   For c flags 0 and 5, we only have the first nine bits as above.
 *
 *   We need six flags; four for the slices themselves, and one indicating
 *   that the top two rows of a particular slice has changed, and thus
 *   the next brick up needs to be recomputed, and one indicating that the
 *   bottom two rows of a particular slice has changed.
 *
 *   In all cases, tiles only contain data about what has happened completely
 *   within their borders; indicating that the next slice over needs to be
 *   recomputed is handled by always inspecting the neighboring slices
 *   *before* a recomputation.
 *
 *   The flags int is divided into two 12-bit fields, each holding a population
 *   count, and one eight-bit field, holding dirty flags.
 *
 *   Note that tiles only point `down' to bricks, never to each other or to
 *   higher-level supertiles.
 *
 *   Tiles are numbered as level `0' of the universe tree.
 *
 *   The tiles are 32 bytes each; they can hold up to four bricks, so the
 *   memory consumption of the tiles tends to be small.
 */
struct tile { /* 32 bytes */
   struct brick *b[4] ;
   short c[6] ;
   long flags ;
} ;
/*
 *   Supertiles hold pointers to eight subtiles, which can either be 
 *   tiles or supertiles themselves.  Supertiles are numbered as levels `1' on
 *   up of the universe tree.
 *
 *   Odd levels stack their subtiles horizontally; even levels stack their
 *   subtiles vertically.  Thus, each supertile at level 1 corresponds to
 *   a 256x32 section of the universe.  Each supertile at level 2 corresponds
 *   to a 256x256 section of the universe.  And so on.
 *
 *   Levels are never skipped (that is, each pointer in a supertile at
 *   level `n' points to a valid supertile or tile at level `n-1').
 *
 *   The c flag indicates that changes have happened in the subtiles.
 *   The least significant eight bits indicate that changes have occurred
 *   anywhere in the subtile; the least significant bit (bit 0) is associated
 *   with the first subtile for phase 0->1 and the eighth subtile for phase
 *   1->0, and bit 7 is associated with other.  Bit number 8 indicates that
 *   some bits on the right (left) edge of the eighth (first) subtile have
 *   changed for phase 0->1 (1->0).  Bits numbered 9 through 16 indicate that
 *   bits on the bottom (top) of the corresponding subtile have changed, and
 *   bit number 17 indicates that a bit in the lower right (upper left) corner
 *   of the eighth (first) subtile has changed.
 *
 *   Bit 18 through 27 correspond to the previous generation's bits 8 through
 *   17.  Bits 28 through 31 are the dirty bits; we currently use three of
 *   them.
 *
 *   The above description corresponds to odd levels.  For even levels,
 *   since tiles are stacked vertically instead of horizontally, change
 *   `lower' to `right' and `right' to `lower'.
 *
 *   The dirty flags again are used to indicate that this tile needed to
 *   be recomputed since the last time the dirty bit was cleared.
 *
 *   The two pop values hold a population count, if the appropriate dirty
 *   bit is clear.  These counts will never hold values greater than
 *   500M, so that a sum of the eight in unsigned arithmetic is guaranteed
 *   to never overflow.
 *
 *   This completes the universe data structures.  Note that there is no
 *   limit (other than the size of a pointer and memory constraints) on
 *   the size of the universe.  For instance, using these data structures
 *   we can easily build a universe with elements separated by 2^200
 *   pixels.
 *
 *   The supertiles are 44 bytes each; they correspond to at least a
 *   256x32 chunk of the universe, so the total memory consumption due to
 *   supertiles tends to be small.
 */
struct supertile { /* 44 bytes */
   struct supertile *d[8] ;
   int flags ;
   int pop[2] ;
} ;
/*
 *   This is a common header for chunks of memory linked together.
 */
struct linkedmem {
   struct linkedmem *next ;
} ;
/*
 *   This structure contains all of our variables that pertain to a
 *   particular universe.  (Thus, we support multiple universes.)
 *
 *   The minx, miny, maxx, and maxy values describe the borders of
 *   the universe dictated by the level of the root supertile.  If
 *   ever during computation these bounds are found to be too tight,
 *   another supertile is added on top of the root, expanding these
 *   bounds.
 *
 *   These bounds currently limit the size of the universe to 2^32 in
 *   each direction; however, they are only used during the set call
 *   (when initially setting up the universe).  Changing them to
 *   doubles or 64-bit ints will relax this limitation.  For now we
 *   leave them as is.
 *
 *   The rootlev variable contains the supertile level of the root node.
 *
 *   The tilelist, supertilelist, and bricklist are freelists for the
 *   appropriate type of structure.
 *
 *   The memused is a linked list of all memory blocks allocated; this
 *   enables us to free the universe and all of its memory without actually
 *   walking the entire life tree.
 *
 *   The emptybrick pointer points to the unique brick that is guaranteed
 *   to always be empty.  The emptytile pointer is similar.
 *
 *   Finally, root is the top of the current life tree.  Nullroot is the
 *   topmost empty supertile allocated, and nullroots[] holds the empty
 *   supertiles at each level.  Setting this to 40 limits the number of
 *   levels to 40, which is sufficient for a 2^65x2^62 universe.
 */
class qlifealgo : public lifealgo {
public:
   qlifealgo() ;
   virtual ~qlifealgo() ;
   virtual void clearall() ;
   virtual int setcell(int x, int y, int newstate) ;
   virtual int getcell(int x, int y) ;
   virtual int nextcell(int x, int y, int &v) ;
   // call after setcell/clearcell calls
   virtual void endofpattern() {
     // AKT: unnecessary (and prevents shrinking selection while generating)
     // poller->bailIfCalculating() ;
     popValid = 0 ;
   }
   virtual void setIncrement(bigint inc) { increment = inc ; }
   virtual void setIncrement(int inc) { increment = inc ; }
   virtual void setGeneration(bigint gen) { generation = gen ; }
   virtual const bigint &getPopulation() ;
   virtual int isEmpty() ;
   // can we do the gen count doubling? only hashlife
   virtual int hyperCapable() { return 0 ; }
   virtual void setMaxMemory(int m) ;
   virtual int getMaxMemory() { return (int)(maxmemory >> 20) ; }
   virtual const char *setrule(const char *s) ;
   virtual const char *getrule() { return qliferules.getrule() ; }
   virtual void step() ;
   virtual void* getcurrentstate() { return 0 ; }
   virtual void setcurrentstate(void *) {}
   virtual void draw(viewport &view, liferender &renderer) ;
   virtual void fit(viewport &view, int force) ;
   virtual void lowerRightPixel(bigint &x, bigint &y, int mag) ;
   virtual void findedges(bigint *t, bigint *l, bigint *b, bigint *r) ;
   virtual const char *writeNativeFormat(std::ostream &, char *) {
      return "No native format for qlifealgo yet." ;
   }
   static void doInitializeAlgoInfo(staticAlgoInfo &) ;
private:
   linkedmem *filllist(int size) ;
   brick *newbrick() ;
   tile *newtile() ;
   supertile *newsupertile(int lev) ;
   void uproot() ;
   int doquad01(supertile *zis, supertile *edge,
                supertile *par, supertile *cor, int lev) ;
   int doquad10(supertile *zis, supertile *edge,
                supertile *par, supertile *cor, int lev) ;
   int p01(tile *p, tile *pr, tile *pd, tile *prd) ;
   int p10(tile *plu, tile *pu, tile *pl, tile *p) ;
   G_INT64 find_set_bits(supertile *p, int lev, int gm1) ;
   int isEmpty(supertile *p, int lev, int gm1) ;
   supertile *mdelete(supertile *p, int lev) ;
   G_INT64 popcount() ;
   int uproot_needed() ;
   void dogen() ;
   void renderbm(int x, int y) ;
   void renderbm(int x, int y, int xsize, int ysize) ;
   void BlitCells(supertile *p, int xoff, int yoff, int wd, int ht, int lev) ;
   void ShrinkCells(supertile *p, int xoff, int yoff, int wd, int ht, int lev) ;
   int nextcell(int x, int y, supertile *n, int lev) ;
   void fill_ll(int d) ;
   int lowsub(vector<supertile*> &src, vector<supertile*> &dst, int lev) ;
   int highsub(vector<supertile*> &src, vector<supertile*> &dst, int lev) ;
   void allsub(vector<supertile*> &src, vector<supertile*> &dst, int lev) ;
   int gethbitsfromleaves(vector<supertile *> v) ;
   int getvbitsfromleaves(vector<supertile *> v) ;
   int markglobalchange(supertile *, int) ;
   void markglobalchange() ; // call if the rule changes
   /* data elements */
   int min, max, rootlev ;
   int minlow32 ;
   bigint bmin, bmax ;
   bigint population ;
   int popValid ;
   linkedmem *tilelist, *supertilelist, *bricklist ;
   linkedmem *memused ;
   brick *emptybrick ;
   tile *emptytile ;
   supertile *root, *nullroot, *nullroots[40] ;
   int cleandowncounter ;
   g_uintptr_t maxmemory, usedmemory ;
   char *ruletable ;
   // when drawing, these are used
   liferender *renderer ;
   viewport *view ;
   int uviewh, uvieww, viewh, vieww, mag, pmag, kadd ;
   int oddgen ;
   int bmleft, bmtop, bmlev, shbmsize, logshbmsize ;
   int quickb, deltaforward ;
   int llbits, llsize ;
   char *llxb, *llyb ;
   liferules qliferules ;
} ;
#endif
