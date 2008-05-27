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
#ifndef JVNALGO_H
#define JVNALGO_H
#include "lifealgo.h"
#include "liferules.h"
/*
 *   Into instances of this jnode structure is where almost all of the
 *   memory allocated by this program goes.  Thus, it is imperative we
 *   keep it as small as possible so we can explore patterns as large
 *   and as deep as possible.
 *
 *   But first, how does this program even work?  Well, there are
 *   two major tricks that are used.
 *
 *   The first trick is to represent the 2D space `symbolically'
 *   (in the sense that a binary decision diagram is a symbolic
 *   representation of a boolean predicate).  This can be thought
 *   of as a sort of compression.  We break up space into a grid of
 *   squares, each containing 8x8 cells.  And we `canonicalize'
 *   each square; that is, all the squares with no cells set are
 *   represented by a single actual instance of an empty square;
 *   all squares with only the upper-left-most cell set are
 *   represented by yet another instance, and so on.  A single pointer
 *   to the single instance of each square takes less space than
 *   representing the actual cell bits themselves.
 *
 *   Where do we store these pointers?  At first, one might envision
 *   a large two-dimensional array of pointers, each one pointing
 *   to one of the square instances.  But instead, we group the
 *   squares (we'll call them 8-squares) into larger squares 16
 *   cells on a side; these are 16-squares.  Each 16-square contains
 *   four 8-squares, so each 16-square is represented by four
 *   pointers, each to an 8-square.  And we canonicalize these as
 *   well, so for a particular set of values for a 16 by 16 array
 *   of cells, we'll only have a single 16-square.
 *
 *   And so on up; we canonicalize 32-squares out of 16-squares, and
 *   on up to some limit.  Now the limit need not be very large;
 *   having just 20 levels of jnodes gives us a universe that is
 *   4 * 2**20 or about 4M cells on a side.  Having 100 levels of
 *   jnodes (easily within the limits of this program) gives us a
 *   universe that is 4 * 2**100 or about 5E30 cells on a side.
 *   I've run universes that expand well beyond 1E50 on a side with
 *   this program.
 *
 *   [A nice thing about this representation is that there are no
 *   coordinate values anywhere, which means that there are no
 *   limits to the coordinate values or complex multidimensional
 *   arithmetic needed.]
 *
 *   [Note that this structure so far is very similar to the octtrees
 *   used in 3D simulation and rendering programs.  It's different,
 *   however, in that we canonicalize the jnodes, and also, of course,
 *   in that it is 2D rather than 3D.]
 *
 *   I mentioned there were two tricks, and that's only the first.
 *   The second trick is to cache the `results' of the LIFE calculation,
 *   but in a way that looks ahead farther in time as you go higher
 *   in the tree, much like the tree jnodes themselves scan larger
 *   distances in space.  This trick is just a little subtle, but it
 *   is where the bulk of the power of the program comes from.
 *
 *   Consider once again the 8-squares.  We want to cache the result
 *   of executing LIFE on that area.  We could cache the result of
 *   looking ahead just one generation; that would yield a 6x6 square.
 *   (Note that we cannot calculate an 8-square, because we are
 *   using the single instance of the 8-square to represent all the
 *   different places that 8x8 arrangement occurs, and those different
 *   places might be surrounded by different border cells.  But we
 *   can say for sure that the central 6-square will evolve in a
 *   unique way in the next generation.)
 *
 *   We could also calculate the 4-square that is two generations
 *   hence, and the 3-square that is three generations hence, and
 *   the 2-square that is four generations hence.  We choose the
 *   4-square that is two generations hence; why will be clear in
 *   a moment.
 *
 *   Now let's consider the 16-square.  We would like to look farther
 *   ahead for this square (if we always only looked two generations
 *   ahead, our runtime would be at *least* linear in the number of
 *   generations, and we want to beat that.)  So let's look 4 generations
 *   ahead, and cache the resulting 8-square.  So we do.
 *
 *   Where do we cache the results?  Well, we cache the results in the
 *   same jnode structure we are using to store the pointers to the
 *   smaller squares themselves.  And since we're hashing them all
 *   together, we want a next pointer for the hash chain.  Put all of
 *   this together, and you get the following structure for the 16-squares
 *   and larger:
 */
struct jnode {
   jnode *next ;              /* hash link */
   jnode *nw, *ne, *sw, *se ; /* constant; nw != 0 means nonjleaf */
   jnode *res ;               /* cache */
} ;
/*
 *   For the 8-squares, we do not have `children', we have actual data
 *   values.  We still break up the 8-square into 4-squares, but the
 *   4-squares only have 16 cells in them, so we represent them directly
 *   by an unsigned short (in this case, the direct value itself takes
 *   less memory than the pointer we might replace it with).
 *
 *   One minor trick about the following structure.  We did lie above
 *   somewhat; sometimes the struct jnode * points to an actual struct
 *   jnode, and sometimes it points to a struct jleaf.  So we need a way
 *   to tell if the thing we are pointing at is a jnode or a jleaf.  We
 *   could add another bit to the jnode structure, but this would grow
 *   it, and we want it to stay as small as possible.  Now, notice
 *   that, in all valid struct jnodes, all four pointers (nw, ne, sw,
 *   and se) must contain a live non-zero value.  We simply ensure
 *   that the struct jleaf contains a zero where the first (nw) pointer
 *   field would be in a struct jnode.
 *
 *   Each short represents a 4-square in normal, left-to-right then top-down
 *   order from the most significant bit.  So bit 0x8000 is the upper
 *   left (or northwest) bit, and bit 0x1000 is the upper right bit, and
 *   so on.
 */
struct jleaf {
   jnode *next ;              /* hash link */
   jnode *isjnode ;            /* must always be zero for leaves */
   unsigned short nw, ne, sw, se ;  /* constant */
   unsigned short res1, res2 ;      /* constant */
   unsigned short jleafpop ;         /* how many set bits */
} ;
/*
 *   If it is a struct jnode, this returns a non-zero value, otherwise it
 *   returns a zero value.
 */
#define is_jnode(n) (((jnode *)(n))->nw)
/**
 *   Our jvnalgo class.
 */
class jvnalgo : public lifealgo {
public:
   jvnalgo() ;
   virtual ~jvnalgo() ;
   // note that for jvnalgo, clearall() releases no memory; it retains
   // the full cache information but just sets the current pattern to
   // the empty pattern.
   virtual void clearall() ; // not implemented
   virtual void setcell(int x, int y, int newstate) ;
   virtual int getcell(int x, int y) ;
   virtual int nextcell(int x, int y) ;
   virtual void endofpattern() ;
   virtual void setIncrement(bigint inc) ;
   virtual void setIncrement(int inc) { setIncrement(bigint(inc)) ; }
   virtual void setGeneration(bigint gen) { generation = gen ; }
   virtual const bigint &getIncrement() { return increment ; }
   virtual const bigint &getPopulation() ;
   virtual const bigint &getGeneration() { return generation ; }
   virtual int isEmpty() ;
   virtual int hyperCapable() { return 1 ; }
   virtual void setMaxMemory(int m) ;
   virtual int getMaxMemory() { return maxmem >> 20 ; }
   virtual const char *setrule(const char *s) ;
   virtual const char *getrule() { return global_liferules.getrule() ; }
   virtual void step() ;
   static void setVerbose(int v) { verbose = v ; }
   static int getVerbose() { return verbose ; }
   /*
    *   The contract of draw() is that it render every pixel in the
    *   viewport precisely once.  This allows us to eliminate all
    *   flashing.  Later we'll make this be damage-specific.
    */
   virtual void draw(viewport &view, liferender &renderer) ;
   virtual void fit(viewport &view, int force) ;
   virtual void lowerRightPixel(bigint &x, bigint &y, int mag) ;
   virtual void findedges(bigint *t, bigint *l, bigint *b, bigint *r) ;
   virtual const char *readmacrocell(char *line) ;
   virtual const char *writeNativeFormat(FILE *f, char *comments) ;

   // AKT: maximum number of cell states in this universe
   virtual int MaxCellStates() { return 256; }   // or 29???

private:
/*
 *   Some globals representing our universe.  The root is the
 *   real root of the universe, and the depth is the depth of the
 *   tree where 2 means that root is a jleaf, and 3 means that the
 *   children of root are leaves, and so on.  The center of the
 *   root is always coordinate position (0,0), so at startup the
 *   x and y coordinates range from -4..3; in general,
 *   -(2**depth)..(2**depth)-1.  The zerojnodea is an
 *   array of canonical `empty-space' jnodes at various depths.
 *   The ngens is an input parameter which is the second power of
 *   the number of generations to run.
 */
   jnode *root ;
   int depth ;
   jnode **zerojnodea ;
   int nzeros ;
/*
 *   Finally, our gc routine.  We keep a `stack' of all the `roots'
 *   we want to preserve.  Nodes not reachable from here, we allow to
 *   be freed.  Same with leaves.
 */
   jnode **stack ;
   int stacksize ;
   g_uintptr_t hashpop, hashlimit, hashprime ;
   jnode **hashtab ;
   int halvesdone ;
   int gsp ;
   g_uintptr_t alloced, maxmem ;
   jnode *freejnodes ;
   int okaytogc ;
   g_uintptr_t totalthings ;
   jnode *jnodeblocks ;
   char *ruletable ;
   bigint generation ;
   bigint population ;
   bigint increment ;
   bigint setincrement ;
   bigint pow2step ; // greatest power of two in increment
   int nonpow2 ; // increment / pow2step
   int ngens ; // log2(pow2step)
   int popValid, needPop, inGC ;
   /*
    *   When rendering we store the relevant bits here rather than
    *   passing them deep into recursive subroutines.
    */
   liferender *renderer ;
   viewport *view ;
   int uviewh, uvieww, viewh, vieww, mag, pmag ;
   int llbits, llsize ;
   char *llxb, *llyb ;
   int hashed ;
   int cacheinvalid ;
   g_uintptr_t cellcounter ; // used when writing
   g_uintptr_t writecells ; // how many to write
   int gccount ; // how many gcs total this pattern
   int gcstep ; // how many gcs this step
   static int verbose ;
   static char statusline[] ;
//
   void jleafres(jleaf *n) ;
   void resize() ;
   jnode *find_jnode(jnode *nw, jnode *ne, jnode *sw, jnode *se) ;
   void unhash_jnode(jnode *n) ;
   void rehash_jnode(jnode *n) ;
   jleaf *find_jleaf(unsigned short nw, unsigned short ne,
                   unsigned short sw, unsigned short se) ;
   jnode *getres(jnode *n, int depth) ;
   jnode *dorecurs(jnode *n, jnode *ne, jnode *t, jnode *e, int depth) ;
   jnode *dorecurs_half(jnode *n, jnode *ne, jnode *t, jnode *e, int depth) ;
   jleaf *dorecurs_jleaf(jleaf *n, jleaf *ne, jleaf *t, jleaf *e) ;
   jleaf *dorecurs_jleaf_half(jleaf *n, jleaf *ne, jleaf *t, jleaf *e) ;
   jleaf *dorecurs_jleaf_quarter(jleaf *n, jleaf *ne, jleaf *t, jleaf *e) ;
   jnode *newjnode() ;
   jleaf *newjleaf() ;
   jnode *newclearedjnode() ;
   jleaf *newclearedjleaf() ;
   void pushroot_1() ;
   int jnode_depth(jnode *n) ;
   jnode *zerojnode(int depth) ;
   jnode *pushroot(jnode *n) ;
   jnode *setbit(jnode *n, int x, int y, int newstate, int depth) ;
   int getbit(jnode *n, int x, int y, int depth) ;
   int nextbit(jnode *n, int x, int y, int depth) ;
   jnode *hashpattern(jnode *root, int depth) ;
   jnode *popzeros(jnode *n) ;
   const bigint &calcpop(jnode *root, int depth) ;
   void aftercalcpop2(jnode *root, int depth, int cleanbigints) ;
   void calcPopulation(jnode *root) ;
   jnode *save(jnode *n) ;
   void pop(int n) ;
   void clearstack() ;
   void clearcache() ;
   void gc_mark(jnode *root, int invalidate) ;
   void do_gc(int invalidate) ;
   void clearcache(jnode *n, int depth, int clearto) ;
   void new_ngens(int newval) ;
   int log2(unsigned int n) ;
   jnode *runpattern() ;
   void clearrect(int x, int y, int w, int h) ;
   void renderbm(int x, int y) ;
   void fill_ll(int d) ;
   void drawjnode(jnode *n, int llx, int lly, int depth, jnode *z) ;
   void ensure_hashed() ;
   g_uintptr_t writecell(FILE *f, jnode *root, int depth) ;
   g_uintptr_t writecell_2p1(jnode *root, int depth) ;
   g_uintptr_t writecell_2p2(FILE *f, jnode *root, int depth) ;
   void unpack8x8(unsigned short nw, unsigned short ne,
		  unsigned short sw, unsigned short se,
		  unsigned int *top, unsigned int *bot) ;

   // AKT: set all pixels to background color
   void killpixels();
   // AKT: this was a static routine in jvndraw.cpp
   void drawpixel(int x, int y);
} ;
#endif
