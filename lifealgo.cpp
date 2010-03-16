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
  if (timeline.framecount)
    destroytimeline() ;
  void *now = getcurrentstate() ;
  if (now == 0)
    return 0 ;
  timeline.base = basearg ;
  timeline.expo = expoarg ;
  timeline.frames.push_back(now) ;
  timeline.recording = 1 ;
  timeline.framecount = 1 ;
  timeline.next = timeline.end = timeline.start = generation ;
  timeline.inc = increment ;
  timeline.next += increment ;
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
