// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "hperf.h"
/*
 *   Reporting.
 *   The node count listed here wants to be big to reduce the number
 *   of "get times" we do (which can be fairly expensive on some systems),
 *   but it wants to be small in case something is going wrong like swapping
 *   and we want to tell the user that the node rate has dropped.
 *   A value of 65K is a reasonable compromise.
 */
int hperf::reportMask = ((1<<16)-1) ; // node count between checks
/*
 *   How frequently do we update the status bar?  Every two seconds
 *   should be reasonable.  If we set this to zero, then that disables
 *   performance reporting.
 */
double hperf::reportInterval = 2 ; // time between reports
/*
 *   Static buffer for status updates.
 */
char perfstatusline[200] ;
void hperf::report(hperf &mark, int verbose) {
   double ts = gollySecondCount() ;
   double elapsed = ts - mark.timeStamp ;
   if (reportInterval == 0 || elapsed < reportInterval)
      return ;
   timeStamp = ts ;
   nodesCalculated += fastNodeInc ;
   fastNodeInc = 0 ;
   if (verbose) {
      double nodeCount = nodesCalculated - mark.nodesCalculated ;
      double halfFrac = 0 ;
      if (nodeCount > 0)
         halfFrac = (halfNodes - mark.halfNodes) / nodeCount ;
      double depthDelta = depthSum - mark.depthSum ;
      sprintf(perfstatusline, "RATE noderate %g depth %g half %g",
                       nodeCount/elapsed, 1+depthDelta/nodeCount, halfFrac) ;
      lifestatus(perfstatusline) ;
   }
   mark = *this ;
}
void hperf::reportStep(hperf &mark, hperf &ratemark, double newGen, int verbose) {
   nodesCalculated += fastNodeInc ;
   fastNodeInc = 0 ;
   timeStamp = gollySecondCount() ;
   double elapsed = timeStamp - mark.timeStamp ;
   if (reportInterval == 0 || elapsed < reportInterval)
      return ;
   if (verbose) {
      double inc = newGen - mark.genval ;
      if (inc == 0)
         inc = 1e30 ;
      double nodeCount = nodesCalculated - mark.nodesCalculated ;
      double halfFrac = 0 ;
      if (nodeCount > 0)
         halfFrac = (halfNodes - mark.halfNodes) / nodeCount ;
      double depthDelta = depthSum - mark.depthSum ;
      double genspersec = inc / elapsed ;
      double nodespergen = nodeCount / inc ;
      sprintf(perfstatusline,
          "PERF gps %g nps %g depth %g half %g npg %g nodes %g",
          genspersec, nodeCount/elapsed, 1+depthDelta/nodeCount, halfFrac,
          nodespergen, nodeCount) ;
      lifestatus(perfstatusline) ;
   }
   genval = newGen ;
   mark = *this ;
   ratemark = *this ;
}
