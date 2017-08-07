// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef JVNALGO_H
#define JVNALGO_H
#include "ghashbase.h"
/**
 *   Our JvN algo class.
 */
class jvnalgo : public ghashbase {
public:
   jvnalgo() ;
   virtual ~jvnalgo() ;
   virtual state slowcalc(state nw, state n, state ne, state w, state c,
                          state e, state sw, state s, state se) ;
   virtual const char* setrule(const char* s) ;
   virtual const char* getrule() ;
   virtual const char* DefaultRule() ;
   virtual int NumCellStates() ;
   static void doInitializeAlgoInfo(staticAlgoInfo &) ;
private:
   enum { JvN29, Nobili32, Hutton32 } current_rule ;
};
#endif
