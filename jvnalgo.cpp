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
/*
 * jvn 0.99 by Radical Eye Software.
 *
 *   All good ideas here were originated by Gosper or Bell or others, I'm
 *   sure, and all bad ones by yours truly.
 *
 *   The main reason I wrote this program was to attempt to push out the
 *   evaluation of metacatacryst as far as I could.  So this program
 *   really does very little other than compute life as far into the
 *   future as possible, using as little memory as possible (and reusing
 *   it if necessary).  No UI, few options.
 */
#include "jvnalgo.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <iostream>
using namespace std ;
/*
 *   Prime hash sizes tend to work best.
 */
static g_uintptr_t nextprime(g_uintptr_t i) {
   g_uintptr_t j ;
   i |= 1 ;
   for (;; i+=2) {
      for (j=3; j*j<=i; j+=2)
         if (i % j == 0)
            break ;
      if (j*j > i)
         return i ;
   }
}
/*
 *   Note that all the places we represent 4-squares by short, we use
 *   unsigned shorts; this is so we can directly index into these arrays.
 */
static unsigned char shortpop[65536] ;
/*
 *   The cached result of an 8-square is a new 4-square representing
 *   two generations into the future.  This subroutine calculates that
 *   future, assuming ruletable is calculated (see below).  The code
 *   that it uses is similar to code you'll see again, so we explain
 *   what's going on in some detail.
 *
 *   Each time we build a jleaf jnode, we compute the result, because it
 *   is reasonably quick.
 *
 *   The first generation result of an 8-square is a 6-square, which
 *   we represent as nine 2-squares.  The nine 2-squares are called
 *   t00 through t22, and are arranged in a matrix:
 *
 *      t00   t01   t02
 *      t10   t11   t12
 *      t20   t21   t22
 *
 *   To compute each of these, we need to extract the relevant bits
 *   from the four 4-square values n->nw, n->ne, n->sw, and n->ne.
 *   We can use these values to directly index into the ruletable
 *   array.
 *
 *   Then, given the nine values, we can compute a resulting 4-square
 *   by computing four 2-square results, and combining these into a
 *   single 4-square.
 *
 *   It's a bit intricate, but it's not really overwhelming.
 */
#define combine9(t00,t01,t02,t10,t11,t12,t20,t21,t22) \
       ((t00) << 15) | ((t01) << 13) | (((t02) << 11) & 0x1000) | \
       (((t10) << 7) & 0x880) | ((t11) << 5) | (((t12) << 3) & 0x110) | \
       (((t20) >> 1) & 0x8) | ((t21) >> 3) | ((t22) >> 5)
void jvnalgo::jleafres(jleaf *n) {
   unsigned short
   t00 = ruletable[n->nw],
   t01 = ruletable[((n->nw << 2) & 0xcccc) | ((n->ne >> 2) & 0x3333)],
   t02 = ruletable[n->ne],
   t10 = ruletable[((n->nw << 8) & 0xff00) | ((n->sw >> 8) & 0x00ff)],
   t11 = ruletable[((n->nw << 10) & 0xcc00) | ((n->ne << 6) & 0x3300) |
                   ((n->sw >> 6) & 0x00cc) | ((n->se >> 10) & 0x0033)],
   t12 = ruletable[((n->ne << 8) & 0xff00) | ((n->se >> 8) & 0x00ff)],
   t20 = ruletable[n->sw],
   t21 = ruletable[((n->sw << 2) & 0xcccc) | ((n->se >> 2) & 0x3333)],
   t22 = ruletable[n->se] ;
   n->res1 = combine9(t00,t01,t02,t10,t11,t12,t20,t21,t22) ;
   n->res2 =
   (ruletable[(t00 << 10) | (t01 << 8) | (t10 << 2) | t11] << 10) |
   (ruletable[(t01 << 10) | (t02 << 8) | (t11 << 2) | t12] << 8) |
   (ruletable[(t10 << 10) | (t11 << 8) | (t20 << 2) | t21] << 2) |
    ruletable[(t11 << 10) | (t12 << 8) | (t21 << 2) | t22] ;
   n->jleafpop = shortpop[n->nw] + shortpop[n->ne] +
                shortpop[n->sw] + shortpop[n->se] ;
}
/*
 *   We do now support garbage collection, but there are some routines we
 *   call frequently to help us.
 */
#define jnode_hash(a,b,c,d) (((g_uintptr_t)d)+3*(((g_uintptr_t)c)+3*(((g_uintptr_t)b)+3*((g_uintptr_t)a)+3)))
#define jleaf_hash(a,b,c,d) ((d)+9*((c)+9*((b)+9*(a))))
/*
 *   Resize the hash.
 */
void jvnalgo::resize() {
   g_uintptr_t i, nhashprime = nextprime(2 * hashprime) ;
   jnode *p, **nhashtab ;
   if (alloced > maxmem ||
       nhashprime * sizeof(jnode *) > (maxmem - alloced)) {
      hashlimit = G_MAX ;
      return ;
   }
   /*
    *   Don't let the hash table buckets take more than 4% of the
    *   memory.  If we're starting to strain memory, let the buckets
    *   fill up a bit more.
    */
   if (nhashprime > (maxmem/(25*sizeof(int *)))) {
      nhashprime = nextprime(maxmem/(25*sizeof(int *))) ;
      if (nhashprime == hashprime) {
         hashlimit = G_MAX ;
         return ;
      }
   }
   if (verbose) {
     strcpy(statusline, "Resizing hash...") ;
     lifestatus(statusline) ;
   }
   nhashtab = (jnode **)calloc(nhashprime, sizeof(jnode *)) ;
   if (nhashtab == 0) {
     lifewarning("Out of memory; running in a somewhat slower mode; "
                 "try reducing the hash memory limit after restarting.") ;
     hashlimit = G_MAX ;
     return ;
   }
   alloced += sizeof(jnode *) * (nhashprime - hashprime) ;
   for (i=0; i<hashprime; i++) {
      for (p=hashtab[i]; p;) {
         jnode *np = p->next ;
         g_uintptr_t h ;
         if (is_jnode(p)) {
            h = jnode_hash(p->nw, p->ne, p->sw, p->se) ;
         } else {
            jleaf *l = (jleaf *)p ;
            h = jleaf_hash(l->nw, l->ne, l->sw, l->se) ;
         }
         h %= nhashprime ;
         p->next = nhashtab[h] ;
         nhashtab[h] = p ;
         p = np ;
      }
   }
   free(hashtab) ;
   hashtab = nhashtab ;
   hashprime = nhashprime ;
   hashlimit = hashprime ;
   if (verbose) {
     strcpy(statusline+strlen(statusline), " done.") ;
     lifestatus(statusline) ;
   }
}
/*
 *   These next two routines are (nearly) our only hash table access
 *   routines; we simply look up the passed in information.  If we
 *   find it in the hash table, we return it; otherwise, we build a
 *   new jnode and store it in the hash table, and return that.
 */
jnode *jvnalgo::find_jnode(jnode *nw, jnode *ne, jnode *sw, jnode *se) {
   jnode *p ;
   g_uintptr_t h = jnode_hash(nw,ne,sw,se) ;
   jnode *pred = 0 ;
   h = h % hashprime ;
   for (p=hashtab[h]; p; p = p->next) { /* make sure to compare nw *first* */
      if (nw == p->nw && ne == p->ne && sw == p->sw && se == p->se) {
         if (pred) { /* move this one to the front */
            pred->next = p->next ;
            p->next = hashtab[h] ;
            hashtab[h] = p ;
         }
         return save(p) ;
      }
      pred = p ;
   }
   p = newjnode() ;
   p->nw = nw ;
   p->ne = ne ;
   p->sw = sw ;
   p->se = se ;
   p->res = 0 ;
   p->next = hashtab[h] ;
   hashtab[h] = p ;
   hashpop++ ;
   if (hashpop > hashlimit)
      resize() ;
   return save(p) ;
}
void jvnalgo::unhash_jnode(jnode *n) {
   jnode *p ;
   g_uintptr_t h = jnode_hash(n->nw,n->ne,n->sw,n->se) ;
   jnode *pred = 0 ;
   h = h % hashprime ;
   for (p=hashtab[h]; p; p = p->next) {
      if (p == n) {
         if (pred)
            pred->next = p->next ;
         else
            hashtab[h] = p->next ;
         return ;
      }
      pred = p ;
   }
   lifefatal("Didn't find jnode to unhash") ;
}
void jvnalgo::rehash_jnode(jnode *n) {
   g_uintptr_t h = jnode_hash(n->nw,n->ne,n->sw,n->se) ;
   h = h % hashprime ;
   n->next = hashtab[h] ;
   hashtab[h] = n ;
}
jleaf *jvnalgo::find_jleaf(unsigned short nw, unsigned short ne,
                                  unsigned short sw, unsigned short se) {
   jleaf *p ;
   jleaf *pred = 0 ;
   g_uintptr_t h = jleaf_hash(nw, ne, sw, se) ;
   h = h % hashprime ;
   for (p=(jleaf *)hashtab[h]; p; p = (jleaf *)p->next) {
      if (nw == p->nw && ne == p->ne && sw == p->sw && se == p->se &&
          !is_jnode(p)) {
         if (pred) {
            pred->next = p->next ;
            p->next = hashtab[h] ;
            hashtab[h] = (jnode *)p ;
         }
         return (jleaf *)save((jnode *)p) ;
      }
      pred = p ;
   }
   p = newjleaf() ;
   p->nw = nw ;
   p->ne = ne ;
   p->sw = sw ;
   p->se = se ;
   jleafres(p) ;
   p->isjnode = 0 ;
   p->next = hashtab[h] ;
   hashtab[h] = (jnode *)p ;
   hashpop++ ;
   if (hashpop > hashlimit)
      resize() ;
   return (jleaf *)save((jnode *)p) ;
}
/*
 *   The following routine does the same, but first it checks to see if
 *   the cached result is any good.  If it is, it directly returns that.
 *   Otherwise, it figures out whether to call the jleaf routine or the
 *   non-jleaf routine by whether two jnodes down is a jleaf jnode or not.
 *   (We'll understand why this is a bit later.)  All the sp stuff is
 *   stack pointer and garbage collection stuff.
 */
jnode *jvnalgo::getres(jnode *n, int depth) {
   if (n->res)
     return n->res ;
   jnode *res = 0 ;
   /**
    *   This routine be the only place we assign to res.  We use
    *   the fact that the poll routine is *sticky* to allow us to
    *   manage unwinding the stack without munging our data
    *   structures.  Note that there may be many find_jnodes
    *   and getres called before we finally actually exit from
    *   here, because the stack is deep and we don't want to
    *   put checks throughout the code.  Instead we need two
    *   calls here, one to prevent us going deeper, and another
    *   to prevent us from destroying the cache field.
    */
   if (poller->poll())
     return zerojnode(depth-1) ;
   int sp = gsp ;
   depth-- ;
   if (ngens >= depth) {
     if (is_jnode(n->nw)) {
       res = dorecurs(n->nw, n->ne, n->sw, n->se, depth) ;
     } else {
       res = (jnode *)dorecurs_jleaf((jleaf *)n->nw, (jleaf *)n->ne,
				   (jleaf *)n->sw, (jleaf *)n->se) ;
     }
   } else {
     if (halvesdone < 1000)
       halvesdone++ ;
     if (is_jnode(n->nw)) {
       res = dorecurs_half(n->nw, n->ne, n->sw, n->se, depth) ;
     } else if (ngens == 0) {
       res = (jnode *)dorecurs_jleaf_quarter((jleaf *)n->nw, (jleaf *)n->ne,
					   (jleaf *)n->sw, (jleaf *)n->se) ;
     } else {
       res = (jnode *)dorecurs_jleaf_half((jleaf *)n->nw, (jleaf *)n->ne,
					(jleaf *)n->sw, (jleaf *)n->se) ;
     }
   }
   pop(sp) ;
   if (poller->isInterrupted()) // don't assign this to the cache field!
     res = zerojnode(depth) ;
   else
     n->res = res ;
   return res ;
}
/*
 *   So let's say the cached way failed.  How do we do it the slow way?
 *   Recursively, of course.  For an n-square (composed of the four
 *   n/2-squares passed in, compute the n/2-square that is n/4
 *   generations ahead.
 *
 *   This routine works exactly the same as the jleafres() routine, only
 *   instead of working on an 8-square, we're working on an n-square,
 *   returning an n/2-square, and we build that n/2-square by first building
 *   9 n/4-squares, use those to calculate 4 more n/4-squares, and
 *   then put these together into a new n/2-square.  Simple, eh?
 */
jnode *jvnalgo::dorecurs(jnode *n, jnode *ne, jnode *t, jnode *e, int depth) {
   int sp = gsp ;
   jnode
   *t00 = getres(n, depth),
   *t01 = getres(find_jnode(n->ne, ne->nw, n->se, ne->sw), depth),
   *t02 = getres(ne, depth),
   *t12 = getres(find_jnode(ne->sw, ne->se, e->nw, e->ne), depth),
   *t11 = getres(find_jnode(n->se, ne->sw, t->ne, e->nw), depth),
   *t10 = getres(find_jnode(n->sw, n->se, t->nw, t->ne), depth),
   *t20 = getres(t, depth),
   *t21 = getres(find_jnode(t->ne, e->nw, t->se, e->sw), depth),
   *t22 = getres(e, depth),
   *t44 = getres(find_jnode(t11, t12, t21, t22), depth),
   *t43 = getres(find_jnode(t10, t11, t20, t21), depth),
   *t33 = getres(find_jnode(t00, t01, t10, t11), depth),
   *t34 = getres(find_jnode(t01, t02, t11, t12), depth) ;
   n = find_jnode(t33, t34, t43, t44) ;
   pop(sp) ;
   return save(n) ;
}
/*
 *   Same as above, but we only do one step instead of 2.
 */
jnode *jvnalgo::dorecurs_half(jnode *n, jnode *ne, jnode *t,
                               jnode *e, int depth) {
   int sp = gsp ;
   jnode
   *t00 = getres(n, depth),
   *t01 = getres(find_jnode(n->ne, ne->nw, n->se, ne->sw), depth),
   *t10 = getres(find_jnode(n->sw, n->se, t->nw, t->ne), depth),
   *t11 = getres(find_jnode(n->se, ne->sw, t->ne, e->nw), depth),
   *t02 = getres(ne, depth),
   *t12 = getres(find_jnode(ne->sw, ne->se, e->nw, e->ne), depth),
   *t20 = getres(t, depth),
   *t21 = getres(find_jnode(t->ne, e->nw, t->se, e->sw), depth),
   *t22 = getres(e, depth) ;
   if (depth > 3) {
      n = find_jnode(find_jnode(t00->se, t01->sw, t10->ne, t11->nw),
                    find_jnode(t01->se, t02->sw, t11->ne, t12->nw),
                    find_jnode(t10->se, t11->sw, t20->ne, t21->nw),
                    find_jnode(t11->se, t12->sw, t21->ne, t22->nw)) ;
   } else {
      n = find_jnode((jnode *)find_jleaf(((jleaf *)t00)->se,
                                             ((jleaf *)t01)->sw,
                                             ((jleaf *)t10)->ne,
                                             ((jleaf *)t11)->nw),
                    (jnode *)find_jleaf(((jleaf *)t01)->se,
                                             ((jleaf *)t02)->sw,
                                             ((jleaf *)t11)->ne,
                                             ((jleaf *)t12)->nw),
                    (jnode *)find_jleaf(((jleaf *)t10)->se,
                                             ((jleaf *)t11)->sw,
                                             ((jleaf *)t20)->ne,
                                             ((jleaf *)t21)->nw),
                    (jnode *)find_jleaf(((jleaf *)t11)->se,
                                             ((jleaf *)t12)->sw,
                                             ((jleaf *)t21)->ne,
                                             ((jleaf *)t22)->nw)) ;
   }
   pop(sp) ;
   return save(n) ;
}
/*
 *   If the jnode is a 16-jnode, then the constituents are leaves, so we
 *   need a very similar but still somewhat different subroutine.  Since
 *   we do not (yet) garbage collect leaves, we don't need all that
 *   save/pop mumbo-jumbo.
 */
jleaf *jvnalgo::dorecurs_jleaf(jleaf *n, jleaf *ne, jleaf *t, jleaf *e) {
   unsigned short
   t00 = n->res2,
   t01 = find_jleaf(n->ne, ne->nw, n->se, ne->sw)->res2,
   t02 = ne->res2,
   t10 = find_jleaf(n->sw, n->se, t->nw, t->ne)->res2,
   t11 = find_jleaf(n->se, ne->sw, t->ne, e->nw)->res2,
   t12 = find_jleaf(ne->sw, ne->se, e->nw, e->ne)->res2,
   t20 = t->res2,
   t21 = find_jleaf(t->ne, e->nw, t->se, e->sw)->res2,
   t22 = e->res2 ;
   return find_jleaf(find_jleaf(t00, t01, t10, t11)->res2,
                    find_jleaf(t01, t02, t11, t12)->res2,
                    find_jleaf(t10, t11, t20, t21)->res2,
                    find_jleaf(t11, t12, t21, t22)->res2) ;
}
/*
 *   Same as above but we only do two generations.
 */
#define combine4(t00,t01,t10,t11) (unsigned short)\
((((t00)<<10)&0xcc00)|(((t01)<<6)&0x3300)|(((t10)>>6)&0xcc)|(((t11)>>10)&0x33))
jleaf *jvnalgo::dorecurs_jleaf_half(jleaf *n, jleaf *ne, jleaf *t, jleaf *e) {
   unsigned short
   t00 = n->res2,
   t01 = find_jleaf(n->ne, ne->nw, n->se, ne->sw)->res2,
   t02 = ne->res2,
   t10 = find_jleaf(n->sw, n->se, t->nw, t->ne)->res2,
   t11 = find_jleaf(n->se, ne->sw, t->ne, e->nw)->res2,
   t12 = find_jleaf(ne->sw, ne->se, e->nw, e->ne)->res2,
   t20 = t->res2,
   t21 = find_jleaf(t->ne, e->nw, t->se, e->sw)->res2,
   t22 = e->res2 ;
   return find_jleaf(combine4(t00, t01, t10, t11),
                    combine4(t01, t02, t11, t12),
                    combine4(t10, t11, t20, t21),
                    combine4(t11, t12, t21, t22)) ;
}
/*
 *   Same as above but we only do one generation.
 */
jleaf *jvnalgo::dorecurs_jleaf_quarter(jleaf *n, jleaf *ne,
                                   jleaf *t, jleaf *e) {
   unsigned short
   t00 = n->res1,
   t01 = find_jleaf(n->ne, ne->nw, n->se, ne->sw)->res1,
   t02 = ne->res1,
   t10 = find_jleaf(n->sw, n->se, t->nw, t->ne)->res1,
   t11 = find_jleaf(n->se, ne->sw, t->ne, e->nw)->res1,
   t12 = find_jleaf(ne->sw, ne->se, e->nw, e->ne)->res1,
   t20 = t->res1,
   t21 = find_jleaf(t->ne, e->nw, t->se, e->sw)->res1,
   t22 = e->res1 ;
   return find_jleaf(combine4(t00, t01, t10, t11),
                    combine4(t01, t02, t11, t12),
                    combine4(t10, t11, t20, t21),
                    combine4(t11, t12, t21, t22)) ;
}
/*
 *   We keep free jnodes in a linked list for allocation, and we allocate
 *   them 1000 at a time.
 */
jnode *jvnalgo::newjnode() {
   jnode *r ;
   if (freejnodes == 0) {
      int i ;
      freejnodes = (jnode *)calloc(1001, sizeof(jnode)) ;
      if (freejnodes == 0)
         lifefatal("Out of memory; try reducing the hash memory limit.") ;
      alloced += 1001 * sizeof(jnode) ;
      freejnodes->next = jnodeblocks ;
      jnodeblocks = freejnodes++ ;
      for (i=0; i<999; i++) {
         freejnodes[1].next = freejnodes ;
         freejnodes++ ;
      }
      totalthings += 1000 ;
   }
   if (freejnodes->next == 0 && alloced + 1000 * sizeof(jnode) > maxmem &&
       okaytogc) {
      do_gc(0) ;
   }
   r = freejnodes ;
   freejnodes = freejnodes->next ;
   return r ;
}
/*
 *   Leaves are the same.
 */
jleaf *jvnalgo::newjleaf() {
   return (jleaf *)newjnode() ;
}
/*
 *   Sometimes we want the new jnode or jleaf to be automatically cleared
 *   for us.
 */
jnode *jvnalgo::newclearedjnode() {
   return (jnode *)memset(newjnode(), 0, sizeof(jnode)) ;
}
jleaf *jvnalgo::newclearedjleaf() {
   return (jleaf *)memset(newjleaf(), 0, sizeof(jleaf)) ;
}
jvnalgo::jvnalgo() {
   int i ;
/*
 *   The population of one-bits in an integer is one more than the
 *   population of one-bits in the integer with one fewer bit set,
 *   and we can turn off a bit by anding an integer with the next
 *   lower integer.
 */
   if (shortpop[1] == 0)
      for (i=1; i<65536; i++)
         shortpop[i] = shortpop[i & (i - 1)] + 1 ;
   hashprime = nextprime(1000) ;
   hashlimit = hashprime ;
   hashpop = 0 ;
   hashtab = (jnode **)calloc(hashprime, sizeof(jnode *)) ;
   if (hashtab == 0)
     lifefatal("Out of memory (1).") ;
   alloced += hashprime * sizeof(jnode *) ;
   ngens = 0 ;
   stacksize = 0 ;
   halvesdone = 0 ;
   nzeros = 0 ;
   stack = 0 ;
   gsp = 0 ;
   alloced = 0 ;
   maxmem = 256 * 1024 * 1024 ;
   freejnodes = 0 ;
   okaytogc = 0 ;
   totalthings = 0 ;
   jnodeblocks = 0 ;
   zerojnodea = 0 ;
   ruletable = global_liferules.rule0 ;
/*
 *   We initialize our universe to be a 16-square.  We are in drawing
 *   mode at this point.
 */
   root = (jnode *)newclearedjnode() ;
   population = 0 ;
   generation = 0 ;
   increment = 1 ;
   setincrement = 1 ;
   nonpow2 = 1 ;
   pow2step = 1 ;
   llsize = 0 ;
   depth = 3 ;
   hashed = 0 ;
   popValid = 0 ;
   needPop = 0 ;
   inGC = 0 ;
   cacheinvalid = 0 ;
   gccount = 0 ;
   gcstep = 0 ;

   // AKT: init colors for all cell states here!!!???
   cellred[1] = 255;
   cellgreen[1] = 255;
   cellblue[1] = 255;
}
/**
 *   Destructor frees memory.
 */
jvnalgo::~jvnalgo() {
   free(hashtab) ;
   while (jnodeblocks) {
      jnode *r = jnodeblocks ;
      jnodeblocks = jnodeblocks->next ;
      free(r) ;
   }
   if (zerojnodea)
      free(zerojnodea) ;
   if (stack)
      free(stack) ;
   if (llsize) {
      delete [] llxb ;
      delete [] llyb ;
   }
}
/**
 *   Set increment.
 */
void jvnalgo::setIncrement(bigint inc) {
   increment = inc ;
}
/**
 *   Do a step.
 */
void jvnalgo::step() {
   poller->bailIfCalculating() ;
   // we use while here because the increment may be changed while we are
   // doing the hashtable sweep; if that happens, we may need to sweep
   // again.
   int cleareddownto = 1000000000 ;
   while (increment != setincrement) {
      bigint pendingincrement = increment ;
      int newpow2 = 0 ;
      bigint t = pendingincrement ;
      while (t > 0 && t.even()) {
         newpow2++ ;
         t.div2() ;
      }
      nonpow2 = t.low31() ;
      if (t != nonpow2)
         lifefatal("bad increment") ;
      int downto = newpow2 ;
      if (ngens < newpow2)
	 downto = ngens ;
      if (newpow2 != ngens && cleareddownto > downto) {
         new_ngens(newpow2) ;
	 cleareddownto = downto ;
      } else {
         ngens = newpow2 ;
      }
      setincrement = pendingincrement ;
      pow2step = 1 ;
      while (newpow2--)
         pow2step += pow2step ;
   }
   gcstep = 0 ;
   for (int i=0; i<nonpow2; i++) {
      jnode *newroot = runpattern() ;
      if (newroot == 0 || poller->isInterrupted()) // we *were* interrupted
         break ;
      popValid = 0 ;
      root = newroot ;
   }
   depth = jnode_depth(root) ;
}
/*
 *   Set the max memory
 */
void jvnalgo::setMaxMemory(int newmemlimit) {
   if (newmemlimit < 10)
     newmemlimit = 10 ;
   else if (sizeof(maxmem) <= 4 && newmemlimit > 4000)
     newmemlimit = 4000 ;
   g_uintptr_t newlimit = ((g_uintptr_t)newmemlimit) << 20 ;
   if (alloced > newlimit) {
      lifewarning("Sorry, more memory currently used than allowed.") ;
      return ;
   }
   maxmem = newlimit ;
   hashlimit = hashprime ;
}
/**
 *   Clear everything.
 */
void jvnalgo::clearall() {
   lifefatal("clearall not implemented yet") ;
}
/*
 *   This routine expands our universe by a factor of two, maintaining
 *   centering.  We use four new jnodes, and *reuse* the root so this cannot
 *   be called after we've started hashing.
 */
void jvnalgo::pushroot_1() {
   jnode *t ;
   t = newclearedjnode() ;
   t->se = root->nw ;
   root->nw = t ;
   t = newclearedjnode() ;
   t->sw = root->ne ;
   root->ne = t ;
   t = newclearedjnode() ;
   t->ne = root->sw ;
   root->sw = t ;
   t = newclearedjnode() ;
   t->nw = root->se ;
   root->se = t ;
   depth++ ;
}
/*
 *   Return the depth of this jnode (2 is 8x8).
 */
int jvnalgo::jnode_depth(jnode *n) {
   int depth = 2 ;
   while (is_jnode(n)) {
      depth++ ;
      n = n->nw ;
   }
   return depth ;
}
/*
 *   This routine returns the canonical clear space jnode at a particular
 *   depth.
 */
jnode *jvnalgo::zerojnode(int depth) {
   while (depth >= nzeros) {
      int nnzeros = 2 * nzeros + 10 ;
      zerojnodea = (jnode **)realloc(zerojnodea,
                                          nnzeros * sizeof(jnode *)) ;
      if (zerojnodea == 0)
	lifefatal("Out of memory (2).") ;
      alloced += (nnzeros - nzeros) * sizeof(jnode *) ;
      while (nzeros < nnzeros)
         zerojnodea[nzeros++] = 0 ;
   }
   if (zerojnodea[depth] == 0) {
      if (depth == 2) {
         zerojnodea[depth] = (jnode *)find_jleaf(0, 0, 0, 0) ;
      } else {
         jnode *z = zerojnode(depth-1) ;
         zerojnodea[depth] = find_jnode(z, z, z, z) ;
      }
   }
   return zerojnodea[depth] ;
}
/*
 *   Same, but with hashed jnodes.
 */
jnode *jvnalgo::pushroot(jnode *n) {
   int depth = jnode_depth(n) ;
   jnode *z = zerojnode(depth-1) ;
   return find_jnode(find_jnode(z, z, z, n->nw),
                    find_jnode(z, z, n->ne, z),
                    find_jnode(z, n->sw, z, z),
                    find_jnode(n->se, z, z, z)) ;
}
/*
 *   Here is our recursive routine to set a bit in our universe.  We
 *   pass in a depth, and walk the space.  Again, a lot of bit twiddling,
 *   but really not all that complicated.  We allocate new jnodes and
 *   leaves on our way down.
 *
 *   Note that at this point our universe lives outside the hash table
 *   and has not been canonicalized, and that many of the pointers in
 *   the jnodes can be null.  We'll patch this up in due course.
 */
jnode *jvnalgo::setbit(jnode *n, int x, int y, int newstate, int depth) {
   if (depth == 2) {
      jleaf *l = (jleaf *)n ;
      if (hashed) {
         unsigned short nw = l->nw ;
         unsigned short sw = l->sw ;
         unsigned short ne = l->ne ;
         unsigned short se = l->se ;
         if (newstate) {
            if (x < 0)
               if (y < 0)
                  sw |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
               else
                  nw |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
            else
               if (y < 0)
                  se |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
               else
                  ne |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
         } else {
            if (x < 0)
               if (y < 0)
                  sw &= ~(1 << (3 - (x & 3) + 4 * (y & 3))) ;
               else
                  nw &= ~(1 << (3 - (x & 3) + 4 * (y & 3))) ;
            else
               if (y < 0)
                  se &= ~(1 << (3 - (x & 3) + 4 * (y & 3))) ;
               else
                  ne &= ~(1 << (3 - (x & 3) + 4 * (y & 3))) ;
         }
         return save((jnode *)find_jleaf(nw, ne, sw, se)) ;
      }
      if (newstate) {
         if (x < 0)
            if (y < 0)
               l->sw |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
            else
               l->nw |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
         else
            if (y < 0)
               l->se |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
            else
               l->ne |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
      } else {
         if (x < 0)
            if (y < 0)
               l->sw &= ~(1 << (3 - (x & 3) + 4 * (y & 3))) ;
            else
               l->nw &= ~(1 << (3 - (x & 3) + 4 * (y & 3))) ;
         else
            if (y < 0)
               l->se &= ~(1 << (3 - (x & 3) + 4 * (y & 3))) ;
            else
               l->ne &= ~(1 << (3 - (x & 3) + 4 * (y & 3))) ;
      }
      return (jnode *)l ;
   } else {
      int w = 1 << depth ;
      if (depth > 31)
	 w = 0 ;
      depth-- ;
      jnode **nptr ;
      if (x < 0) {
         if (y < 0)
            nptr = &(n->sw) ;
         else
            nptr = &(n->nw) ;
      } else {
         if (y < 0)
            nptr = &(n->se) ;
         else
            nptr = &(n->ne) ;
      }
      if (*nptr == 0) {
         if (depth == 2)
            *nptr = (jnode *)newclearedjleaf() ;
         else
            *nptr = newclearedjnode() ;
      }
      jnode *s = setbit(*nptr, (x & (w - 1)) - (w >> 1),
                              (y & (w - 1)) - (w >> 1), newstate, depth) ;
      if (hashed) {
         jnode *nw = n->nw ;
         jnode *sw = n->sw ;
         jnode *ne = n->ne ;
         jnode *se = n->se ;
         if (x < 0) {
            if (y < 0)
               sw = s ;
            else
               nw = s ;
         } else {
            if (y < 0)
               se = s ;
            else
               ne = s ;
         }
         n = save(find_jnode(nw, ne, sw, se)) ;
      } else {
         *nptr = s ;
      }
      return n ;
   }
}
/*
 *   Here is our recursive routine to get a bit in our universe.  We
 *   pass in a depth, and walk the space.  Again, a lot of bit twiddling,
 *   but really not all that complicated.
 */
int jvnalgo::getbit(jnode *n, int x, int y, int depth) {
   if (depth == 2) {
      jleaf *l = (jleaf *)n ;
      int test = 0 ;
      if (x < 0)
         if (y < 0)
            test = (l->sw & (1 << (3 - (x & 3) + 4 * (y & 3)))) ;
         else
            test = (l->nw & (1 << (3 - (x & 3) + 4 * (y & 3)))) ;
      else
         if (y < 0)
            test = (l->se & (1 << (3 - (x & 3) + 4 * (y & 3)))) ;
         else
            test = (l->ne & (1 << (3 - (x & 3) + 4 * (y & 3)))) ;
      if (test)
         return 1 ;
      return 0 ;
   } else {
      int w = 1 << depth ;
      if (depth > 31)
	 w = 0 ;
      jnode *nptr ;
      depth-- ;
      if (x < 0) {
         if (y < 0)
            nptr = n->sw ;
	 else
            nptr = n->nw ;
      } else {
         if (y < 0)
            nptr = n->se ;
         else
            nptr = n->ne ;
      }
      if (nptr == 0 || nptr == zerojnode(depth))
         return 0 ;
      return getbit(nptr, (x & (w - 1)) - (w >> 1), (y & (w - 1)) - (w >> 1),
                    depth) ;
   }
}
/*
 *   Here is our recursive routine to get the next bit in our universe.  We
 *   pass in a depth, and walk the space.  Again, a lot of bit twiddling,
 *   but really not all that complicated.
 */
int jvnalgo::nextbit(jnode *n, int x, int y, int depth) {
   if (n == 0 || n == zerojnode(depth))
      return -1 ;
   if (depth == 2) {
      jleaf *l = (jleaf *)n ;
      int test = 0 ;
      if (y < 0)
	test = (((l->sw >> (4 * (y & 3))) & 15) << 4) |
	        ((l->se >> (4 * (y & 3))) & 15) ;
      else
	test = (((l->nw >> (4 * (y & 3))) & 15) << 4) |
	        ((l->ne >> (4 * (y & 3))) & 15) ;
      test &= (1 << (4 - x)) - 1 ;
      if (test) {
	int r = 0 ;
	int b = 1 << (3 - x) ;
	while ((test & b) == 0) {
	  r++ ;
	  b >>= 1 ;
	}
	return r ;
      }
      return -1 ; // none found
   } else {
      int w = 1 << depth ;
      int wh = 1 << (depth - 1) ;
      jnode *lft, *rght ;
      depth-- ;
      if (y < 0) {
	lft = n->sw ;
	rght = n->se ;
      } else {
	lft = n->nw ;
	rght = n->ne ;
      }
      int r = 0 ;
      if (x < 0) {
	int t = nextbit(lft, (x & (w-1)) - wh,
			(y & (w - 1)) - wh, depth) ;
	if (t >= 0)
	  return t ;
	r = -x ;
	x = 0 ;
      }
      int t = nextbit(rght, (x & (w-1)) - wh,
		      (y & (w - 1)) - wh, depth) ;
      if (t >= 0)
	return r + t ;
      return -1 ;
   }
}
/*
 *   Our nonrecurse top-level bit setting routine simply expands the
 *   universe as necessary to encompass the passed-in coordinates, and
 *   then invokes the recursive setbit.  Right now it works hashed or
 *   unhashed (but it's faster when unhashed).  We also turn on the inGC
 *   flag to inhibit popcount.
 */
void jvnalgo::setcell(int x, int y, int newstate) {
   if (hashed) {
      clearstack() ;
      save(root) ;
      okaytogc = 1 ;
   }
   inGC = 1 ;
   y = - y ;
   int sx = x ;
   int sy = y ;
   if (depth <= 31) {
     sx >>= depth ;
     sy >>= depth ;
   } else {
     sx >>= 31 ;
     sy >>= 31 ;
   }
   while (sx > 0 || sx < -1 || sy > 0 || sy < -1) {
      if (hashed) {
         root = save(pushroot(root)) ;
         depth++ ;
      } else {
         pushroot_1() ;
      }
      sx >>= 1 ;
      sy >>= 1 ;
   }
   root = setbit(root, x, y, newstate, depth) ;
   if (hashed) {
      okaytogc = 0 ;
   }
}
/*
 *   Our nonrecurse top-level bit getting routine.
 */
int jvnalgo::getcell(int x, int y) {
   y = - y ;
   int sx = x ;
   int sy = y ;
   if (depth <= 31) {
     sx >>= depth ;
     sy >>= depth ;
   } else {
     sx >>= 31 ;
     sy >>= 31 ;
   }
   if (sx > 0 || sx < -1 || sy > 0 || sy < -1)
      return 0 ;
   return getbit(root, x, y, depth) ;
}
/*
 *   A recursive bit getting routine, but this one returns the
 *   number of pixels to the right to the next set cell in the
 *   current universe, or -1 if none set to the right, or if
 *   the next set pixel is out of range.
 */
int jvnalgo::nextcell(int x, int y) {
   y = - y ;
   int sx = x ;
   int sy = y ;
   if (depth <= 31) {
     sx >>= depth ;
     sy >>= depth ;
   } else {
     sx >>= 31 ;
     sy >>= 31 ;
   }
   while (sx > 0 || sx < -1 || sy > 0 || sy < -1) {
      if (hashed) {
         root = save(pushroot(root)) ;
         depth++ ;
      } else {
         pushroot_1() ;
      }
      sx >>= 1 ;
      sy >>= 1 ;
   }
   if (depth > 30) {
      struct jnode tjnode = *root ;
      int mdepth = depth ;
      while (mdepth > 30) {
	 tjnode.nw = tjnode.nw->se ;
	 tjnode.ne = tjnode.ne->sw ;
	 tjnode.sw = tjnode.sw->ne ;
	 tjnode.se = tjnode.se->nw ;
	 mdepth-- ;
      }
      return nextbit(&tjnode, x, y, mdepth) ;
   }
   return nextbit(root, x, y, depth) ;
}
/*
 *   Canonicalize a universe by filling in the null pointers and then
 *   invoking find_jnode on each jnode.  Drops the original universe on
 *   the floor [big deal, it's probably small anyway].
 */
jnode *jvnalgo::hashpattern(jnode *root, int depth) {
   jnode *r ;
   if (root == 0) {
      r = zerojnode(depth) ;
   } else if (depth == 2) {
      jleaf *n = (jleaf *)root ;
      r = (jnode *)find_jleaf(n->nw, n->ne, n->sw, n->se) ;
      n->next = freejnodes ;
      freejnodes = root ;
   } else {
      depth-- ;
      r = find_jnode(hashpattern(root->nw, depth),
                    hashpattern(root->ne, depth),
                    hashpattern(root->sw, depth),
                    hashpattern(root->se, depth)) ;
      root->next = freejnodes ;
      freejnodes = root ;
   }
   return r ;
}
void jvnalgo::endofpattern() {
   poller->bailIfCalculating() ;
   if (!hashed) {
      root = hashpattern(root, depth) ;
      hashed = 1 ;
   }
   popValid = 0 ;
   needPop = 0 ;
   inGC = 0 ;
}
void jvnalgo::ensure_hashed() {
   if (!hashed)
      endofpattern() ;
}
/*
 *   Pop off any levels we don't need.
 */
jnode *jvnalgo::popzeros(jnode *n) {
   int depth = jnode_depth(n) ;
   while (depth > 3) {
      jnode *z = zerojnode(depth-2) ;
      if (n->nw->nw == z && n->nw->ne == z && n->nw->sw == z &&
          n->ne->nw == z && n->ne->ne == z && n->ne->se == z &&
          n->sw->nw == z && n->sw->sw == z && n->sw->se == z &&
          n->se->ne == z && n->se->sw == z && n->se->se == z) {
         depth-- ;
         n = find_jnode(n->nw->se, n->ne->sw, n->sw->ne, n->se->nw) ;
      } else {
         break ;
      }
   }
   return n ;
}
/*
 *   A lot of the routines from here on down traverse the universe, hanging
 *   information off the jnodes.  The way they generally do so is by using
 *   (or abusing) the cache (res) field, and the least significant bit of
 *   the hash next field (as a visited bit).
 */
#define marked(n) (1 & (g_uintptr_t)(n)->next)
#define mark(n) ((n)->next = (jnode *)(1 | (g_uintptr_t)(n)->next))
#define clearmark(n) ((n)->next = (jnode *)(~1 & (g_uintptr_t)(n)->next))
#define clearmarkbit(p) ((jnode *)(~1 & (g_uintptr_t)(p)))
/*
 *   Sometimes we want to use *res* instead of next to mark.  You cannot
 *   do this to leaves, though.
 */
#define marked2(n) (1 & (g_uintptr_t)(n)->res)
#define mark2(n) ((n)->res = (jnode *)(1 | (g_uintptr_t)(n)->res))
#define clearmark2(n) ((n)->res = (jnode *)(~1 & (g_uintptr_t)(n)->res))
static void sum4(bigint &dest, const bigint &a, const bigint &b,
                 const bigint &c, const bigint &d) {
   dest = a ;
   dest += b ;
   dest += c ;
   dest += d ;
}
/*
 *   This recursive routine calculates the population by hanging the
 *   population on marked jnodes.
 */
const bigint &jvnalgo::calcpop(jnode *root, int depth) {
   if (root == zerojnode(depth))
      return bigint::zero ;
   if (depth == 2) {
      root->nw = 0 ;
      bigint &r = *(bigint *)&(root->nw) ;
      jleaf *n = (jleaf *)root ;
      r = n->jleafpop ;
      return r ;
   } else if (marked2(root)) {
      return *(bigint*)&(root->next) ;
   } else {
      depth-- ;
      unhash_jnode(root) ;
/**
 *   We use the memory in root->next as a value bigint.  But we want to
 *   make sure the copy constructor doesn't "clean up" something that
 *   doesn't exist.  So we clear it to zero here.
 */
      root->next = (jnode *)0 ; // I wish I could come up with a cleaner way
      sum4(*(bigint *)&(root->next),
           calcpop(root->nw, depth), calcpop(root->ne, depth),
           calcpop(root->sw, depth), calcpop(root->se, depth)) ;
      mark2(root) ;
      return *(bigint *)&(root->next) ;
   }
}
/*
 *   Call this after doing something that unhashes jnodes in order to
 *   use the next field as a temp pointer.
 */
void jvnalgo::aftercalcpop2(jnode *root, int depth, int cleanbigints) {
   if (root == zerojnode(depth))
      return ;
   if (depth == 2) {
      root->nw = 0 ; // all these bigints are guaranteed to be small
      return ;
   }
   if (marked2(root)) {
      clearmark2(root) ;
      depth-- ;
      aftercalcpop2(root->nw, depth, cleanbigints) ;
      aftercalcpop2(root->ne, depth, cleanbigints) ;
      aftercalcpop2(root->sw, depth, cleanbigints) ;
      aftercalcpop2(root->se, depth, cleanbigints) ;
      if (cleanbigints)
         *(bigint *)&(root->next) = bigint::zero ; // clean up; yuck!
      rehash_jnode(root) ;
   }
}
/*
 *   This top level routine calculates the population of a universe.
 */
void jvnalgo::calcPopulation(jnode *root) {
   int depth ;
   ensure_hashed() ;
   depth = jnode_depth(root) ;
   population = calcpop(root, depth) ;
   aftercalcpop2(root, depth, 1) ;
}
/*
 *   Is the universe empty?
 */
int jvnalgo::isEmpty() {
   ensure_hashed() ;
   return root == zerojnode(depth) ;
}
/*
 *   This routine marks a jnode as needed to be saved.
 */
jnode *jvnalgo::save(jnode *n) {
   if (gsp >= stacksize) {
      int nstacksize = stacksize * 2 + 100 ;
      alloced += sizeof(jnode *)*(nstacksize-stacksize) ;
      stack = (jnode **)realloc(stack, nstacksize * sizeof(jnode *)) ;
      if (stack == 0)
	lifefatal("Out of memory (3).") ;
      stacksize = nstacksize ;
   }
   stack[gsp++] = n ;
   return n ;
}
/*
 *   This routine pops the stack back to a previous depth.
 */
void jvnalgo::pop(int n) {
   gsp = n ;
}
/*
 *   This routine clears the stack altogether.
 */
void jvnalgo::clearstack() {
   gsp = 0 ;
}
/*
 *   Do a gc.  Walk down from all jnodes reachable on the stack, saveing
 *   them by setting the odd bit on the next link.  Then, walk the hash,
 *   eliminating the res from everything that's not saveed, and moving
 *   the jnodes from the hash to the freelist as appropriate.  Finally,
 *   walk the hash again, clearing the low order bits in the next pointers.
 */
void jvnalgo::gc_mark(jnode *root, int invalidate) {
   if (!marked(root)) {
      mark(root) ;
      if (is_jnode(root)) {
         gc_mark(root->nw, invalidate) ;
         gc_mark(root->ne, invalidate) ;
         gc_mark(root->sw, invalidate) ;
         gc_mark(root->se, invalidate) ;
         if (root->res)
	    if (invalidate)
	      root->res = 0 ;
	    else
	      gc_mark(root->res, invalidate) ;
      }
   }
}
/**
 *   If the invalidate flag is set, we want to kill *all* cache entries
 *   and recalculate all leaves.
 */
void jvnalgo::do_gc(int invalidate) {
   int i ;
   g_uintptr_t freed_jnodes=0 ;
   jnode *p, *pp ;
   inGC = 1 ;
   gccount++ ;
   gcstep++ ;
   if (verbose) {
     if (gcstep > 1)
       sprintf(statusline, "GC #%d(%d) ", gccount, gcstep) ;
     else
       sprintf(statusline, "GC #%d ", gccount) ;
     lifestatus(statusline) ;
   }
   for (i=nzeros-1; i>=0; i--)
      if (zerojnodea[i] != 0)
         break ;
   if (i >= 0)
      gc_mark(zerojnodea[i], 0) ; // never invalidate zerojnode
   for (i=0; i<gsp; i++) {
      poller->poll() ;
      gc_mark(stack[i], invalidate) ;
   }
   hashpop = 0 ;
   memset(hashtab, 0, sizeof(jnode *) * hashprime) ;
   freejnodes = 0 ;
   for (p=jnodeblocks; p; p=p->next) {
      poller->poll() ;
      for (pp=p+1, i=1; i<1001; i++, pp++) {
         if (marked(pp)) {
            int h = 0 ;
            if (pp->nw) { /* yes, it's a jnode */
               h = jnode_hash(pp->nw, pp->ne, pp->sw, pp->se) % hashprime ;
            } else {
               jleaf *lp = (jleaf *)pp ;
               if (invalidate)
                  jleafres(lp) ;
               h = jleaf_hash(lp->nw, lp->ne, lp->sw, lp->se) % hashprime ;
            }
            pp->next = hashtab[h] ;
            hashtab[h] = pp ;
            hashpop++ ;
         } else {
            pp->next = freejnodes ;
            freejnodes = pp ;
            freed_jnodes++ ;
         }
      }
   }
   inGC = 0 ;
   if (verbose) {
     int perc = freed_jnodes / (totalthings / 100) ;
     sprintf(statusline+strlen(statusline), " freed %d percent.", perc) ;
     lifestatus(statusline) ;
   }
   if (needPop) {
      calcPopulation(root) ;
      popValid = 1 ;
      needPop = 0 ;
      poller->updatePop() ;
   }
}
/*
 *   Clear the cache bits down to the appropriate level, marking the
 *   jnodes we've handled.
 */
void jvnalgo::clearcache(jnode *n, int depth, int clearto) {
   if (!marked(n)) {
      mark(n) ;
      if (depth > 3) {
         depth-- ;
	 poller->poll() ;
         clearcache(n->nw, depth, clearto) ;
         clearcache(n->ne, depth, clearto) ;
         clearcache(n->sw, depth, clearto) ;
         clearcache(n->se, depth, clearto) ;
         if (n->res)
            clearcache(n->res, depth, clearto) ;
      }
      if (depth >= clearto)
         n->res = 0 ;
   }
}
/*
 *   Clear the entire cache of everything, and recalculate all leaves.
 *   This can be very expensive.
 */
void jvnalgo::clearcache() {
   cacheinvalid = 1 ;
}
/*
 *   Change the ngens value.  Requires us to walk the hash, clearing
 *   the cache fields of any jnodes that do not have the appropriate
 *   values.
 */
void jvnalgo::new_ngens(int newval) {
   g_uintptr_t i ;
   jnode *p, *pp ;
   int clearto = ngens ;
   if (newval > ngens && halvesdone == 0) {
      ngens = newval ;
      return ;
   }
   if (verbose) {
     strcpy(statusline, "Changing increment...") ;
     lifestatus(statusline) ;
   }
   if (newval < clearto)
      clearto = newval ;
   clearto++ ; /* clear this depth and above */
   if (clearto < 3)
      clearto = 3 ;
   ngens = newval ;
   inGC = 1 ;
   for (i=0; i<hashprime; i++)
      for (p=hashtab[i]; p; p=clearmarkbit(p->next))
	 if (is_jnode(p) && !marked(p))
            clearcache(p, jnode_depth(p), clearto) ;
   for (p=jnodeblocks; p; p=p->next) {
      poller->poll() ;
      for (pp=p+1, i=1; i<1001; i++, pp++)
         clearmark(pp) ;
   }
   halvesdone = 0 ;
   inGC = 0 ;
   if (needPop) {
      calcPopulation(root) ;
      popValid = 1 ;
      needPop = 0 ;
      poller->updatePop() ;
   }
   if (verbose) {
     strcpy(statusline+strlen(statusline), " done.") ;
     lifestatus(statusline) ;
   }
}
/*
 *   Return log2.
 */
int jvnalgo::log2(unsigned int n) {
   int r = 0 ;
   while ((n & 1) == 0) {
      n >>= 1 ;
      r++ ;
   }
   if (n != 1) {
      lifefatal("Expected power of two!") ;
   }
   return r ;
}
static bigint negone = -1 ;
const bigint &jvnalgo::getPopulation() {
   // note:  if called during gc, then we cannot call calcPopulation
   // since that will mess up the gc.
   if (!popValid) {
      if (inGC) {
	needPop = 1 ;
	return negone ;
      } else {
	calcPopulation(root) ;
	popValid = 1 ;
	needPop = 0 ;
      }
   }
   return population ;
}
/*
 *   Finally, we get to run the pattern.  We first ensure that all
 *   clearspace jnodes and the input pattern is never garbage
 *   collected; we turn on garbage collection, and then we invoke our
 *   magic top-level routine passing in clearspace borders that are
 *   guaranteed large enough.
 */
jnode *jvnalgo::runpattern() {
   jnode *n = root ;
   save(root) ; // do this in case we interrupt generation
   ensure_hashed() ;
   okaytogc = 1 ;
   if (cacheinvalid) {
      do_gc(1) ; // invalidate the entire cache and recalc leaves
      cacheinvalid = 0 ;
   }
   int depth = jnode_depth(n) ;
   jnode *n2 ;
   n = pushroot(n) ;
   depth++ ;
   n = pushroot(n) ;
   depth++ ;
   while (ngens + 2 > depth) {
      n = pushroot(n) ;
      depth++ ;
   }
   save(zerojnode(nzeros-1)) ;
   save(n) ;
   n2 = getres(n, depth) ;
   okaytogc = 0 ;
   clearstack() ;
   if (halvesdone == 1) {
      n->res = 0 ;
      halvesdone = 0 ;
   }
   if (poller->isInterrupted())
      return 0 ; // indicate it was interrupted
   n = popzeros(n2) ;
   generation += pow2step ;
   return n ;
}
const char *jvnalgo::readmacrocell(char *line) {
   int n=0 ;
   g_uintptr_t i=1, nw, ne, sw, se, indlen=0 ;
   int r, d ;
   jnode **ind = 0 ;
   root = 0 ;
   while (getline(line, 10000)) {
      if (i >= indlen) {
         g_uintptr_t nlen = i + indlen + 10 ;
         ind = (jnode **)realloc(ind, sizeof(jnode*) * nlen) ;
	 if (ind == 0)
	   lifefatal("Out of memory (4).") ;
         while (indlen < nlen)
            ind[indlen++] = 0 ;
      }
      if (line[0] == '.' || line[0] == '*' || line[0] == '$') {
         int x=0, y=7 ;
         unsigned short lnw=0, lne=0, lsw=0, lse=0 ;
         char *p = 0 ;
         for (p=line; *p > ' '; p++) {
            switch(*p) {
case '*':      if (x > 7 || y < 0)
                  return "Illegal coordinates in readmacrocell." ;
               if (x < 4)
                  if (y < 4)
                     lsw |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
                  else
                     lnw |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
               else
                  if (y < 4)
                     lse |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
                  else
                     lne |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
               /* note: fall through here */
case '.':      x++ ;
               break ;
case '$':      x = 0 ;
               y-- ;
               break ;
default:       return "Illegal character in readmacrocell." ;
            }
         }
	 clearstack() ;
         ind[i++] = (jnode *)find_jleaf(lnw, lne, lsw, lse) ;
      } else if (line[0] == '#') {
         switch (line[1]) {
         char *p, *pp ;
         case 'R':
            p = line + 2 ;
            while (*p && *p <= ' ') p++ ;
            pp = p ;
            while (*pp > ' ') pp++ ;
            *pp = 0 ;
            global_liferules.setrule(p) ;
            break ;
         case 'G':
            p = line + 2 ;
            while (*p && *p <= ' ') p++ ;
            pp = p ;
            while (*pp >= '0' && *pp <= '9') pp++ ;
            *pp = 0 ;
            generation = bigint(p) ;
            break ;
         }
      } else {
         n = sscanf(line, "%d %" PRIuPTR " %" PRIuPTR " %" PRIuPTR " %" PRIuPTR " %d", &d, &nw, &ne, &sw, &se, &r) ;
         if (n < 0) // blank line; permit
            continue ;
	 if (n == 0) {
	    // conversion error in first argument; we allow only if the only
	    // content on the line is whitespace.
	    char *ws = line ;
	    while (*ws && *ws <= ' ')
	       ws++ ;
	    if (*ws > 0)
	       return "Parse error in macrocell format." ;
	    continue ;
	 }
         if (n < 5)
            // AKT: best not to use lifefatal here because user won't see any
            // error message when reading clipboard data starting with "[..."
            return "Parse error in readmacrocell." ;
         if (d < 4)
            return "Oops; bad depth in readmacrocell." ;
         ind[0] = zerojnode(d-2) ; /* allow zeros to work right */
         if (nw < 0 || nw >= i || ind[nw] == 0 ||
             ne < 0 || ne >= i || ind[ne] == 0 ||
             sw < 0 || sw >= i || ind[sw] == 0 ||
             se < 0 || se >= i || ind[se] == 0) {
            return "Node out of range in readmacrocell." ;
         }
	 clearstack() ;
         root = ind[i++] = find_jnode(ind[nw], ind[ne], ind[sw], ind[se]) ;
         depth = d - 1 ;
      }
   }
   if (ind)
      free(ind) ;
   if (root == 0) {
      // AKT: allow empty macrocell pattern; note that endofpattern()
      // will be called soon so don't set hashed here
      // return "Invalid macrocell file: no jnodes." ;
      return 0 ;
   }
   hashed = 1 ;
   return 0 ;
}
const char *jvnalgo::setrule(const char *s) {
   poller->bailIfCalculating() ;
   clearcache() ;
   return global_liferules.setrule(s) ;
}
void jvnalgo::unpack8x8(unsigned short nw, unsigned short ne,
			  unsigned short sw, unsigned short se,
			  unsigned int *top, unsigned int *bot) {
   *top = ((nw & 0xf000) << 16) | (((ne & 0xf000) | (nw & 0xf00)) << 12) |
          (((ne & 0xf00) | (nw & 0xf0)) << 8) |
          (((ne & 0xf0) | (nw & 0xf)) << 4) | (ne & 0xf) ;
   *bot = ((sw & 0xf000) << 16) | (((se & 0xf000) | (sw & 0xf00)) << 12) |
          (((se & 0xf00) | (sw & 0xf0)) << 8) |
          (((se & 0xf0) | (sw & 0xf)) << 4) | (se & 0xf) ;
}
/**
 *   Write out the native macrocell format.  This is the one we use when
 *   we're not interactive and displaying a progress dialog.
 */
g_uintptr_t jvnalgo::writecell(FILE *f, jnode *root, int depth) {
   g_uintptr_t thiscell = 0 ;
   if (root == zerojnode(depth))
      return 0 ;
   if (depth == 2) {
      if (root->nw != 0)
         return (g_uintptr_t)(root->nw) ;
   } else {
      if (marked2(root))
         return (g_uintptr_t)(root->next) ;
      unhash_jnode(root) ;
      mark2(root) ;
   }
   if (depth == 2) {
      int i, j ;
      unsigned int top, bot ;
      jleaf *n = (jleaf *)root ;
      thiscell = ++cellcounter ;
      root->nw = (jnode *)thiscell ;
      unpack8x8(n->nw, n->ne, n->sw, n->se, &top, &bot) ;
      for (j=7; (top | bot) && j>=0; j--) {
         int bits = (top >> 24) ;
         top = (top << 8) | (bot >> 24) ;
         bot = (bot << 8) ;
         for (i=0; bits && i<8; i++, bits = (bits << 1) & 255)
            if (bits & 128)
               fputs("*", f) ;
            else
               fputs(".", f) ;
         fputs("$", f) ;
      }
      fputs("\n", f) ;
   } else {
      g_uintptr_t nw = writecell(f, root->nw, depth-1) ;
      g_uintptr_t ne = writecell(f, root->ne, depth-1) ;
      g_uintptr_t sw = writecell(f, root->sw, depth-1) ;
      g_uintptr_t se = writecell(f, root->se, depth-1) ;
      thiscell = ++cellcounter ;
      root->next = (jnode *)thiscell ;
      fprintf(f, "%d %" PRIuPTR " %" PRIuPTR " %" PRIuPTR " %" PRIuPTR "\n", depth+1, nw, ne, sw, se) ;
   }
   return thiscell ;
}
/**
 *   This new two-pass method works by doing a prepass that numbers the
 *   jnodes and counts the number of jnodes that should be sent, so we can
 *   display an accurate progress dialog.
 */
g_uintptr_t jvnalgo::writecell_2p1(jnode *root, int depth) {
   g_uintptr_t thiscell = 0 ;
   if (root == zerojnode(depth))
      return 0 ;
   if (depth == 2) {
      if (root->nw != 0)
         return (g_uintptr_t)(root->nw) ;
   } else {
      if (marked2(root))
         return (g_uintptr_t)(root->next) ;
      unhash_jnode(root) ;
      mark2(root) ;
   }
   if (depth == 2) {
      thiscell = ++cellcounter ;
      // note:  we *must* not abort this prescan
      if ((cellcounter & 4095) == 0)
	 lifeabortprogress(0, "Scanning tree") ;
      root->nw = (jnode *)thiscell ;
   } else {
      writecell_2p1(root->nw, depth-1) ;
      writecell_2p1(root->ne, depth-1) ;
      writecell_2p1(root->sw, depth-1) ;
      writecell_2p1(root->se, depth-1) ;
      thiscell = ++cellcounter ;
      // note:  we *must* not abort this prescan
      if ((cellcounter & 4095) == 0)
	 lifeabortprogress(0, "Scanning tree") ;
      root->next = (jnode *)thiscell ;
   }
   return thiscell ;
}
/**
 *   This one writes the cells, but assuming they've already been
 *   numbered, and displaying a progress dialog.
 */
static char progressmsg[80] ;
g_uintptr_t jvnalgo::writecell_2p2(FILE *f, jnode *root, int depth) {
   g_uintptr_t thiscell = 0 ;
   if (root == zerojnode(depth))
      return 0 ;
   if (depth == 2) {
      if (cellcounter + 1 != (g_uintptr_t)(root->nw))
	 return (g_uintptr_t)(root->nw) ;
      thiscell = ++cellcounter ;
      if ((cellcounter & 4095) == 0) {
	 unsigned long siz = ftell(f) ;
         sprintf(progressmsg, "File size: %.2g MB", double((siz >> 20))) ;
	 lifeabortprogress(thiscell/(double)writecells, progressmsg) ;
      }
      int i, j ;
      unsigned int top, bot ;
      jleaf *n = (jleaf *)root ;
      root->nw = (jnode *)thiscell ;
      unpack8x8(n->nw, n->ne, n->sw, n->se, &top, &bot) ;
      for (j=7; (top | bot) && j>=0; j--) {
         int bits = (top >> 24) ;
         top = (top << 8) | (bot >> 24) ;
         bot = (bot << 8) ;
         for (i=0; bits && i<8; i++, bits = (bits << 1) & 255)
            if (bits & 128)
               fputs("*", f) ;
            else
               fputs(".", f) ;
         fputs("$", f) ;
      }
      fputs("\n", f) ;
   } else {
      if (cellcounter + 1 > (g_uintptr_t)(root->next) || isaborted())
	 return (g_uintptr_t)(root->next) ;
      g_uintptr_t nw = writecell_2p2(f, root->nw, depth-1) ;
      g_uintptr_t ne = writecell_2p2(f, root->ne, depth-1) ;
      g_uintptr_t sw = writecell_2p2(f, root->sw, depth-1) ;
      g_uintptr_t se = writecell_2p2(f, root->se, depth-1) ;
      if (!isaborted() &&
	  cellcounter + 1 != (g_uintptr_t)(root->next)) { // this should never happen
	 lifefatal("Internal in writecell_2p2") ;
	 return (g_uintptr_t)(root->next) ;
      }
      thiscell = ++cellcounter ;
      if ((cellcounter & 4095) == 0) {
	 unsigned long siz = ftell(f) ;
         sprintf(progressmsg, "File size: %.2g MB", double((siz >> 20))) ;
	 lifeabortprogress(thiscell/(double)writecells, progressmsg) ;
      }
      root->next = (jnode *)thiscell ;
      fprintf(f, "%d %" PRIuPTR " %" PRIuPTR " %" PRIuPTR " %" PRIuPTR "\n", depth+1, nw, ne, sw, se) ;
   }
   return thiscell ;
}
#define STRINGIFY(arg) STR2(arg)
#define STR2(arg) #arg
const char *jvnalgo::writeNativeFormat(FILE *f, char *comments) {
   int depth = jnode_depth(root) ;
   fputs("[M2] (golly " STRINGIFY(VERSION) ")", f) ;
   fputs("\n", f) ;
   if (!global_liferules.isRegularLife()) {
      // write non-Life rule
      fprintf(f, "#R %s\n", global_liferules.getrule()) ;
   }
   if (generation > bigint::zero) {
      // write non-zero gen count
      fprintf(f, "#G %s\n", generation.tostring('\0')) ;
   }
   if (comments && comments[0]) {
      // write given comment line(s)
      fputs(comments, f) ;
   }
   /* this is the old way:
   cellcounter = 0 ;
   writecell(f, root, depth) ;
   */
   /* this is the new two-pass way */
   cellcounter = 0 ;
   writecell_2p1(root, depth) ;
   writecells = cellcounter ;
   cellcounter = 0 ;
   writecell_2p2(f, root, depth) ;
   /* end new two-pass way */
   aftercalcpop2(root, depth, 0) ;
   return 0 ;
}
int jvnalgo::verbose = 0 ;
char jvnalgo::statusline[120] ;
