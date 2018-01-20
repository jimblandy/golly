// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef HPERF_H
#define HPERF_H
#include "util.h"
/*
 *   Performance data.  We keep running values here.  We can copy this
 *   to "mark" variables, and then report performance for deltas.
 */
struct hperf {
   void clear() {
      fastNodeInc = 0 ;
      nodesCalculated = 0 ;
      depthSum = 0 ;
      timeStamp = gollySecondCount() ;
      genval = 0 ;
   }
   void report(hperf&, int verbose) ;
   void reportStep(hperf&, hperf&, double genval, int verbose) ;
   int fastinc(int depth, int half) {
      depthSum += depth ;
      if (half)
         halfNodes++ ;
      if ((++fastNodeInc & reportMask) == 0)
         return 1 ;
      else
         return 0 ;
   }
   double getReportInterval() {
      return reportInterval ;
   }
   void setReportInterval(double v) {
      reportInterval = v ;
   }
   int fastNodeInc ;
   double nodesCalculated ;
   double halfNodes ;
   double depthSum ;
   double timeStamp ;
   double genval ;
   static int reportMask ;
   static double reportInterval ;
} ;
#endif
