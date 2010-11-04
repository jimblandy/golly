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
#include "lifealgo.h"
#include "string.h"
using namespace std ;
lifealgo::~lifealgo() {
   poller = 0 ;
   maxCellStates = 2 ;
}
int lifealgo::verbose ;
/*
 *   Right now, the base/expo should match the current increment.
 *   We do not check this.
 */
int lifealgo::startrecording(int basearg, int expoarg) {
  if (timeline.framecount) {
    // already have a timeline; skip to its end
    gotoframe(timeline.framecount-1) ;
  } else {
    // use the current frame and increment to start a new timeline
    void *now = getcurrentstate() ;
    if (now == 0)
      return 0 ;
    timeline.base = basearg ;
    timeline.expo = expoarg ;
    timeline.frames.push_back(now) ;
    timeline.framecount = 1 ;
    timeline.end = timeline.start = generation ;
    timeline.inc = increment ;
  }
  timeline.next = timeline.end ;
  timeline.next += timeline.inc ;
  timeline.recording = 1 ;
  return timeline.framecount ;
}
pair<int, int> lifealgo::stoprecording() {
  timeline.recording = 0 ;
  timeline.next = 0 ;
  return make_pair(timeline.base, timeline.expo) ;
}
void lifealgo::extendtimeline() {
  if (timeline.recording && generation == timeline.next) {
    void *now = getcurrentstate() ;
    if (now && timeline.framecount < MAX_FRAME_COUNT) {
      timeline.frames.push_back(now) ;
      timeline.framecount++ ;
      timeline.end = timeline.next ;
      timeline.next += timeline.inc ;
    }
  }
}
/*
 *   Note that this *also* changes inc, so don't call unless this is
 *   what you want to do.  It does not update or change the base or
 *   expo if the base != 2, so they can get out of sync.
 *
 *   Currently this is only used by bgolly, and it will only work
 *   properly if the increment argument is a power of two.
 */
void lifealgo::pruneframes() {
   if (timeline.framecount > 1) {
      for (int i=2; i<timeline.framecount; i += 2)
         timeline.frames[i >> 1]  = timeline.frames[i] ;
      timeline.framecount = (timeline.framecount + 1) >> 1 ;
      timeline.frames.resize(timeline.framecount) ;
      timeline.inc += timeline.inc ;
      timeline.end = timeline.inc ;
      timeline.end.mul_smallint(timeline.framecount-1) ;
      timeline.end += timeline.start ;
      timeline.next = timeline.end ;
      timeline.next += timeline.inc ;
      if (timeline.base == 2)
         timeline.expo++ ;
   }
}
int lifealgo::gotoframe(int i) {
  if (i < 0 || i >= timeline.framecount)
    return 0 ;
  setcurrentstate(timeline.frames[i]) ;
  // AKT: avoid mul_smallint(i) crashing with divide-by-zero if i is 0
  if (i > 0) {
    generation = timeline.inc ;
    generation.mul_smallint(i) ;
  } else {
    generation = 0;
  }
  generation += timeline.start ;
  return timeline.framecount ;
}
void lifealgo::destroytimeline() {
  timeline.frames.clear() ;
  timeline.recording = 0 ;
  timeline.framecount = 0 ;
  timeline.end = 0 ;
  timeline.start = 0 ;
  timeline.inc = 0 ;
  timeline.next = 0 ;
}

// AKT: the next 2 routines provide support for a bounded universe

const char* lifealgo::setgridsize(const char* suffix) {
   // parse a rule suffix like ":T100,200" and set the various grid parameters;
   // note that we allow any legal partial suffix -- this lets people type a
   // suffix into the Set Rule dialog without the algorithm changing to UNKNOWN
   const char *p = suffix;
   char topology = 0;
   gridwd = gridht = 0;
   hshift = vshift = 0;
   htwist = vtwist = false;
   boundedplane = false;
   sphere = false;
   
   p++;
   if (*p == 0) return 0;                 // treat ":" like ":T0,0"
   if (*p == 't' || *p == 'T') {
      // torus or infinite tube
      topology = 'T';
   } else if (*p == 'p' || *p == 'P') {
      boundedplane = true;
      topology = 'P';
   } else if (*p == 's' || *p == 'S') {
      sphere = true;
      topology = 'S';
   } else if (*p == 'k' || *p == 'K') {
      // Klein bottle (either htwist or vtwist should become true)
      topology = 'K';
   } else if (*p == 'c' || *p == 'C') {
      // cross-surface
      htwist = vtwist = true;
      topology = 'C';
   } else {
      return "Unknown grid topology.";
   }
   
   p++;
   if (*p == 0) return 0;                 // treat ":<char>" like ":T0,0"
   
   while ('0' <= *p && *p <= '9') {
      if (gridwd >= 200000000) {
         gridwd =   2000000000;           // keep width within editable limits
      } else {
         gridwd = 10 * gridwd + *p - '0';
      }
      p++;
   }
   if (*p == '*') {
      if (topology != 'K') return "Only specify a twist for a Klein bottle.";
      htwist = true;
      p++;
   }
   if (*p == '+' || *p == '-') {
      if (topology == 'P') return "Plane can't have a shift.";
      if (topology == 'S') return "Sphere can't have a shift.";
      if (topology == 'C') return "Cross-surface can't have a shift.";
      if (topology == 'K' && !htwist) return "Shift must be on twisted edges.";
      if (gridwd == 0) return "Can't shift infinite width.";
      int sign = *p == '+' ? 1 : -1;
      p++;
      while ('0' <= *p && *p <= '9') {
         hshift = 10 * hshift + *p - '0';
         p++;
      }
      if (hshift >= (int)gridwd) hshift = hshift % (int)gridwd;
      hshift *= sign;
   }
   if (*p == ',' && topology != 'S') {
      p++;
   } else if (*p) {
      return "Unexpected stuff after grid width.";
   }

   // gridwd has been set
   if ((topology == 'K' || topology == 'C' || topology == 'S') && gridwd == 0) {
      return "Given topology can't have an infinite width.";
   }
   
   if (*p == 0) {
      // grid height is not specified so set it to grid width;
      // ie. treat ":T100" like ":T100,100";
      // this also allows us to have ":S100" rather than ":S100,100"
      gridht = gridwd;
   } else {
      while ('0' <= *p && *p <= '9') {
         if (gridht >= 200000000) {
            gridht =   2000000000;     // keep height within editable limits
         } else {
            gridht = 10 * gridht + *p - '0';
         }
         p++;
      }
      if (*p == '*') {
         if (topology != 'K') return "Only specify a twist for a Klein bottle.";
         if (htwist) return "Klein bottle can't have both horizontal and vertical twists.";
         vtwist = true;
         p++;
      }
      if (*p == '+' || *p == '-') {
         if (topology == 'P') return "Plane can't have a shift.";
         if (topology == 'C') return "Cross-surface can't have a shift.";
         if (topology == 'K' && !vtwist) return "Shift must be on twisted edges.";
         if (hshift != 0) return "Can't have both horizontal and vertical shifts.";
         if (gridht == 0) return "Can't shift infinite height.";
         int sign = *p == '+' ? 1 : -1;
         p++;
         while ('0' <= *p && *p <= '9') {
            vshift = 10 * vshift + *p - '0';
            p++;
         }
         if (vshift >= (int)gridht) vshift = vshift % (int)gridht;
         vshift *= sign;
      }
      if (*p) return "Unexpected stuff after grid height.";
   }

   // gridht has been set
   if ((topology == 'K' || topology == 'C') && gridht == 0) {
      return "Klein bottle or cross-surface can't have an infinite height.";
   }
   
   if (topology == 'K' && !(htwist || vtwist)) {
      // treat ":K10,20" like ":K10,20*"
      vtwist = true;
   }
   
   if ((hshift != 0 || vshift != 0) && (gridwd == 0 || gridht == 0)) {
      return "Shifting is not allowed if either grid dimension is unbounded.";
   }
   
   // now ok to set grid edges
   if (gridwd > 0) {
      gridleft = -int(gridwd) / 2;
      gridright = int(gridwd) - 1;
      gridright += gridleft;
   } else {
      // play safe and set these to something
      gridleft = bigint::zero;
      gridright = bigint::zero;
   }
   if (gridht > 0) {
      gridtop = -int(gridht) / 2;
      gridbottom = int(gridht) - 1;
      gridbottom += gridtop;
   } else {
      // play safe and set these to something
      gridtop = bigint::zero;
      gridbottom = bigint::zero;
   }
   return 0;
}

const char* lifealgo::canonicalsuffix() {
   if (gridwd > 0 || gridht > 0) {
      static char bounds[64];
      if (boundedplane) {
         sprintf(bounds, ":P%u,%u", gridwd, gridht);
      } else if (sphere) {
         // sphere requires a square grid (gridwd == gridht)
         sprintf(bounds, ":S%u", gridwd);
      } else if (htwist && vtwist) {
         // cross-surface if both horizontal and vertical edges are twisted
         sprintf(bounds, ":C%u,%u", gridwd, gridht);
      } else if (htwist) {
         // Klein bottle if only horizontal edges are twisted
         if (hshift != 0 && (gridwd & 1) == 0) {
            // twist and shift is only possible if gridwd is even and hshift is 1
            sprintf(bounds, ":K%u*+1,%u", gridwd, gridht);
         } else {
            sprintf(bounds, ":K%u*,%u", gridwd, gridht);
         }
      } else if (vtwist) {
         // Klein bottle if only vertical edges are twisted
         if (vshift != 0 && (gridht & 1) == 0) {
            // twist and shift is only possible if gridht is even and vshift is 1
            sprintf(bounds, ":K%u,%u*+1", gridwd, gridht);
         } else {
            sprintf(bounds, ":K%u,%u*", gridwd, gridht);
         }
      } else if (hshift < 0) {
         // torus with -ve horizontal shift
         sprintf(bounds, ":T%u%d,%u", gridwd, hshift, gridht);
      } else if (hshift > 0) {
         // torus with +ve horizontal shift
         sprintf(bounds, ":T%u+%d,%u", gridwd, hshift, gridht);
      } else if (vshift < 0) {
         // torus with -ve vertical shift
         sprintf(bounds, ":T%u,%u%d", gridwd, gridht, vshift);
      } else if (vshift > 0) {
         // torus with +ve vertical shift
         sprintf(bounds, ":T%u,%u+%d", gridwd, gridht, vshift);
      } else {
         // unshifted torus, or an infinite tube
         sprintf(bounds, ":T%u,%u", gridwd, gridht);
      }
      return bounds;
   } else {
      // unbounded universe
      return 0;
   }
}

int staticAlgoInfo::nextAlgoId = 0 ;
staticAlgoInfo *staticAlgoInfo::head = 0 ;
staticAlgoInfo::staticAlgoInfo() {
   id = nextAlgoId++ ;
   next = head ;
   head = this ;
   // init default icon data
   defxpm7x7 = NULL;
   defxpm15x15 = NULL;
}
staticAlgoInfo *staticAlgoInfo::byName(const char *s) {
   for (staticAlgoInfo *i=head; i; i=i->next)
      if (strcmp(i->algoName, s) == 0)
         return i ;
   return 0 ;
}
int staticAlgoInfo::nameToIndex(const char *s) {
   staticAlgoInfo *r = byName(s) ;
   if (r == 0)
      return -1 ;
   return r->id ;
}
staticAlgoInfo &staticAlgoInfo::tick() {
   return *(new staticAlgoInfo()) ;
}
