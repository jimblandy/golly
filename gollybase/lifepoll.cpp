// This file is part of Golly.
// See docs/License.html for the copyright notice.

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
    // AKT: nicer to simply ignore user event
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
  if (isCalculating()) {
    // AKT: nicer not to call lifefatal
    // lifefatal("recursive poll called.") ;
    lifewarning("Illegal operation while calculating.") ;
    interrupted = 1 ;
  }
}
void lifepoll::updatePop() {}
lifepoll default_poller ;
