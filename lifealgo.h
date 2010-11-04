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
/**
 *   This is the pure abstract class any life calculation algorithm
 *   must support.  As long as a life algorithm implements this
 *   interface, it can be invoked by our driver code.
 */
#ifndef LIFEALGO_H
#define LIFEALGO_H
#include <cstdio>
#include "bigint.h"
#include "viewport.h"
#include "liferender.h"
#include "lifepoll.h"
#include "readpattern.h"
#include "platform.h"
// moving the include vector *before* platform.h breaks compilation
#include <vector>
// this must not be increased beyond 32767, because we use a bigint
// multiply that only supports multiplicands up to that size.
const int MAX_FRAME_COUNT = 32000 ;

/**
 *   Timeline support is pretty generic.
 */
class timeline_t {
public:
   timeline_t() : recording(0), framecount(0), savetimeline(1),
                  start(0), inc(0), next(0), end(0), frames() {}
   int recording, framecount, base, expo, savetimeline ;
   bigint start, inc, next, end ;
   vector<void *> frames ;
} ;

class lifealgo {
public:
   lifealgo() : generation(0), increment(0), timeline(), grid_type(SQUARE_GRID)
      {  poller = &default_poller ;
         gridwd = gridht = 0 ;         // default is an unbounded universe
      }
   virtual void clearall() = 0 ;
   // returns <0 if error
   virtual int setcell(int x, int y, int newstate) = 0 ;
   virtual int getcell(int x, int y) = 0 ;
   virtual int nextcell(int x, int y, int &v) = 0 ;
   // call after setcell/clearcell calls
   virtual void endofpattern() = 0 ;
   virtual void setIncrement(bigint inc) = 0 ;
   virtual void setIncrement(int inc) = 0 ;
   virtual void setGeneration(bigint gen) = 0 ;
   const bigint &getIncrement() { return increment ; }
   const bigint &getGeneration() { return generation ; }
   virtual const bigint &getPopulation() = 0 ;
   virtual int isEmpty() = 0 ;
   // can we do the gen count doubling? only hashlife
   virtual int hyperCapable() = 0 ;
   virtual void setMaxMemory(int m) = 0 ;          // never alloc more than this
   virtual int getMaxMemory() = 0 ;
   virtual const char *setrule(const char *) = 0 ; // new rules; returns err msg
   virtual const char *getrule() = 0 ;             // get current rule set
   virtual void step() = 0 ;                       // do inc gens
   virtual ~lifealgo() = 0 ;
   virtual void draw(viewport &view, liferender &renderer) = 0 ;
   virtual void fit(viewport &view, int force) = 0 ;
   virtual void findedges(bigint *t, bigint *l, bigint *b, bigint *r) = 0 ;
   virtual void lowerRightPixel(bigint &x, bigint &y, int mag) = 0 ;
   virtual const char *writeNativeFormat(FILE *f, char *comments) = 0 ;
   void setpoll(lifepoll *pollerarg) { poller = pollerarg ; }
   virtual const char *readmacrocell(char *) { return "cannot read hash" ; }
   
   // Verbosity crosses algorithms.  We need to embed this sort of option
   // into some global shared thing or something rather than use static.
   static void setVerbose(int v) { verbose = v ; }
   static int getVerbose() { return verbose ; }

   virtual const char* DefaultRule() { return "B3/S23"; }
   // return number of cell states in this universe (2..256)
   virtual int NumCellStates() { return 2; }

   // timeline support
   virtual void* getcurrentstate() = 0 ;
   virtual void setcurrentstate(void *) = 0 ;
   int startrecording(int base, int expo) ;
   pair<int, int> stoprecording() ;
   pair<int, int> getbaseexpo()
                       { return make_pair(timeline.base, timeline.expo) ; }
   void extendtimeline() ;
   void pruneframes() ;
   const bigint &gettimelinestart() { return timeline.start ; }
   const bigint &gettimelineend() { return timeline.end ; }
   const bigint &gettimelineinc() { return timeline.inc ; }
   const int getframecount() { return timeline.framecount ; }
   const int isrecording() { return timeline.recording ; }
   int gotoframe(int i) ;
   void destroytimeline() ;
   void savetimelinewithframe(int yesno) { timeline.savetimeline = yesno ; }

   // support for a bounded universe with various topologies:
   // plane, cylinder, torus, Klein bottle, cross-surface, sphere
   unsigned int gridwd, gridht ;    // bounded universe if either is > 0
   bigint gridleft, gridright ;     // undefined if gridwd is 0
   bigint gridtop, gridbottom ;     // undefined if gridht is 0
   bool boundedplane ;              // topology is a bounded plane?
   bool sphere ;                    // topology is a sphere?
   bool htwist, vtwist ;            // Klein bottle if either is true,
                                    // or cross-surface if both are true
   int hshift, vshift ;             // torus with horizontal or vertical shift
   const char* setgridsize(const char* suffix) ;
   // use in setrule() to parse a suffix like ":T100,200" and set
   // the above parameters
   const char* canonicalsuffix() ;
   // use in setrule() to return the canonical version of suffix;
   // eg. ":t0020" would be converted to ":T20,0"
   
   enum TGridType { SQUARE_GRID, TRI_GRID, HEX_GRID } ;
   TGridType getgridtype() const { return grid_type ; }

protected:
   lifepoll *poller ;
   static int verbose ;
   int maxCellStates ; // keep up to date; setcell depends on it
   bigint generation ;
   bigint increment ;
   timeline_t timeline ;
   TGridType grid_type ;
} ;

/**
 *   If you need any static information from a lifealgo, this class can be
 *   called (or overridden) to set up all that data.  Right now the
 *   functions do nothing; override if you need that info.  These are
 *   called one by one by a static method in the algorithm itself,
 *   if that information is available.  The ones marked optional need
 *   not be called.
 */
class staticAlgoInfo {
public:
   staticAlgoInfo() ;

   // mandatory
   void setAlgorithmName(const char *s) { algoName = s ; }
   void setAlgorithmCreator(lifealgo *(*f)()) { creator = f ; }
   
   // optional; override if you want to retain this data
   virtual void setDefaultBaseStep(int) {}
   virtual void setDefaultMaxMem(int) {}
   
   // minimum and maximum number of cell states supported by this algorithm;
   // both must be within 2..256
   int minstates;
   int maxstates;
   
   // default color scheme
   bool defgradient;                      // use color gradient?
   unsigned char defr1, defg1, defb1;     // color at start of gradient
   unsigned char defr2, defg2, defb2;     // color at end of gradient
   // if defgradient is false then use these colors for each cell state
   unsigned char defr[256], defg[256], defb[256];
   
   // default icon data (in XPM format)
   const char **defxpm7x7;                // 7x7 icons
   const char **defxpm15x15;              // 15x15 icons
   
   // basic data
   const char *algoName ;
   lifealgo *(*creator)() ;
   int id ; // my index
   staticAlgoInfo *next ;
   
   // support:  give me sequential algorithm IDs
   static int getNumAlgos() { return nextAlgoId ; }
   static int nextAlgoId ;
   static staticAlgoInfo &tick() ;
   static staticAlgoInfo *head ;
   static staticAlgoInfo *byName(const char *s) ;
   static int nameToIndex(const char *s) ;
} ;
#endif
