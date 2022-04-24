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
  // TGR:  I think we need this; we should never recursively poll
  //       That indicates something bizarre is going on.  Maybe it's
  //       okay but let's see.
  bailIfCalculating() ;
  // TGR:  After the previous statement this should be a no-op
  /*
  if (isCalculating()) {
    // AKT: nicer to simply ignore user event
    // lifefatal("recursive poll called.") ;
    return interrupted ;
  }
  */
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
    abort() ;
    interrupted = 1 ;
  }
}
void lifepoll::updatePop() {}
lifepoll default_poller ;
