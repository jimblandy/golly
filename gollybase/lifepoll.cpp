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
  bailIfCalculating() ;
  countdown = POLLINTERVAL ;
  calculating++ ;
  if (!interrupted)
    interrupted = checkevents() ;
  calculating-- ;
  return interrupted ;
}
void lifepoll::bailIfCalculating() {
  if (isCalculating()) {
    lifewarning("Illegal operation while calculating.") ;
    abort() ;
    // if abort() call is removed then enable next line
    // interrupted = 1 ;
  }
}
void lifepoll::updatePop() {}
lifepoll default_poller ;
