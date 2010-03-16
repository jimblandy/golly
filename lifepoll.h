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
 *   This interface is called every so often by lifealgo routines to
 *   make sure events are processed and the screen is redrawn in a
 *   timely manner.  The default class does nothing (no actual
 *   callbacks); the user of this class should override checkevents()
 *   to do the right thing.
 */
#ifndef LIFEPOLL_H
#define LIFEPOLL_H
/**
 *   How frequently to invoke the heavyweight event checker, as a
 *   count of inner-loop polls.
 */
const int POLLINTERVAL = 1000 ;
class lifepoll {
public:
   lifepoll() ;
   /**
    *   This is what should be overridden; it should check events,
    *   and return 0 if all is okay or 1 if the existing calculation
    *   should be interrupted.
    */
   virtual int checkevents() ;
   /**
    *   Was an interrupt requested?
    */
   int isInterrupted() { return interrupted ; }
   /**
    *   Before a calculation begins, call this to reset the
    *   interrupted flag.
    */
   void resetInterrupted() { interrupted = 0 ; }
   /**
    *   Call this to stop the current calculation.
    */
   void setInterrupted() { interrupted = 1 ; }
   /**
    *   This is the routine called by the life algorithms at various
    *   points.  It calls checkevents() and stashes the result.
    *
    *   This routine may happen to go in the inner loop where it
    *   could be called a million times a second or more.  Checking
    *   for events might be a moderately heavyweight operation that
    *   could take microseconds, significantly slowing down the
    *   calculation.  To solve this, we use a countdown variable
    *   and only actually check when the countdown gets low
    *   enough (and we put it inline).  We assume a derating of
    *   1000 is sufficient to alleviate the slowdown without
    *   significantly impacting event response time.  Even so, the
    *   poll positions should be carefully selected to be *not*
    *   millions of times a second.
    */
   inline int poll() {
      return (countdown-- > 0) ? interrupted : inner_poll() ;
   }
   int inner_poll() ;
   /**
    *   Sometimes we do a lengthy operation that we *don't* poll
    *   during.  After such operations, this function resets the
    *   poll countdown back to zero so we get very quick response.
    */
   void reset_countdown() { countdown = 0 ; }
   /**
    *   Some routines should not be called during a poll() such as ones
    *   that would modify the state of the algorithm process.  Some
    *   can be safely called but should be deferred.  This routine lets
    *   us know if we are called from the callback or not.
    */
   int isCalculating() { return calculating ; }
   void bailIfCalculating() ;
   /**
    *   Sometimes getPopulation is called when hashlife is in a state
    *   where we just can't calculate it at that point in time.  So
    *   hashlife remembers that the population was requested, and
    *   when the GC is finished, calcs the pop and executes this
    *   callback to update the status window.
    */
   virtual void updatePop() ;
private:
   int interrupted ;
   int calculating ;
   int countdown ;
} ;
extern lifepoll default_poller ;
#endif
