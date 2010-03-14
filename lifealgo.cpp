                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2009 Andrew Trevorrow and Tomas Rokicki.

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
int lifealgo::startrecording(int base, int expo) {
  if (timeline.framecount)
    destroytimeline() ;
  void *now = getcurrentstate() ;
  if (now == 0)
    return 0 ;
  timeline.savedbase = base ;
  timeline.savedexpo = expo ;
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
  return make_pair(timeline.savedbase, timeline.savedexpo) ;
}
void lifealgo::extendTimeline() {
  if (timeline.recording && generation == timeline.next) {
    void *now = getcurrentstate() ;
    if (now) {
      if ((timeline.framecount & 1) == 0
	  && timeline.framecount + 1 >= MAX_FRAME_COUNT) {
	 for (int i=2; i<timeline.framecount; i += 2)
	   timeline.frames[i >> 1]  = timeline.frames[i] ;
         timeline.framecount >>= 1 ;
	 timeline.frames.resize(timeline.framecount) ;
	 timeline.inc += timeline.inc ;
      }
      timeline.frames.push_back(now) ;
      timeline.framecount++ ;
      timeline.end = timeline.next ;
      timeline.next += timeline.inc ;
    }
  }
}
int lifealgo::gotoframe(int i) {
  if (i < 0 || i >= timeline.framecount)
    return 0 ;
  setcurrentstate(timeline.frames[i]) ;
  generation = timeline.inc ;
  generation.mul_smallint(i) ;
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
