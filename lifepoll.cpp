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
#include "lifepoll.h"
#include "util.h"
lifepoll::lifepoll() {
  interrupted = 0 ;
  calculating = 0 ;
  countdown = POLLINTERVAL ;
}
int lifepoll::checkevents() {
  return 0 ;
}
int lifepoll::inner_poll() {
  // AKT: bailIfCalculating() ;
  if (isCalculating()) {
    // AKT: better to simply ignore user event???
    // lifefatal("recursive poll called.") ;
    return interrupted ;
  }
  countdown = POLLINTERVAL ;
  calculating++ ;
  if (!interrupted)
    interrupted = checkevents() ;
  calculating-- ;
  return interrupted ;
}
void lifepoll::bailIfCalculating() {
  if (isCalculating())
    // AKT: lifefatal("recursive poll called.") ;
    lifefatal("Illegal operation while calculating.") ;
}
void lifepoll::updatePop() {}
lifepoll default_poller ;
