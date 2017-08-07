// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef RULETREEALGO_H
#define RULETREEALGO_H
#include "ghashbase.h"
/**
 *   An algorithm that uses an n-dary decision diagram.
 */
class ruletreealgo : public ghashbase {
public:
   ruletreealgo() ;
   virtual ~ruletreealgo() ;
   virtual state slowcalc(state nw, state n, state ne, state w, state c,
                          state e, state sw, state s, state se) ;
   virtual const char* setrule(const char* s) ;
   virtual const char* getrule() ;
   virtual const char* DefaultRule() ;
   virtual int NumCellStates() ;
   static void doInitializeAlgoInfo(staticAlgoInfo &) ;

   // these two methods are needed for RuleLoader algo
   bool IsDefaultRule(const char* rulename);
   const char* LoadTree(FILE* rulefile, int lineno, char endchar, const char* s);

private:
   int *a, base ;
   state *b ;
   int num_neighbors, num_states, num_nodes ;
   char rule[MAXRULESIZE] ;
};
#endif
