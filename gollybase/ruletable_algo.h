// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef RULETABLE_ALGO_H
#define RULETABLE_ALGO_H
#include "ghashbase.h"
#include <string>
#include <vector>
#include <utility>
/**
 *   An algo that takes a rule table.
 */
class ruletable_algo : public ghashbase {

public:

   ruletable_algo() ;
   virtual ~ruletable_algo() ;
   virtual state slowcalc(state nw, state n, state ne, state w, state c,
                          state e, state sw, state s, state se) ;
   virtual const char* setrule(const char* s) ;
   virtual const char* getrule() ;
   virtual const char* DefaultRule() ;
   virtual int NumCellStates() ;
   static void doInitializeAlgoInfo(staticAlgoInfo &) ;

   // these two methods are needed for RuleLoader algo
   bool IsDefaultRule(const char* rulename);
   const char* LoadTable(FILE* rulefile, int lineno, char endchar, const char* s);

protected:

   std::string LoadRuleTable(std::string filename);
   void PackTransitions(const std::string& symmetries, int n_inputs, 
                        const std::vector< std::pair< std::vector< std::vector<state> >, state> > & transition_table);
   void PackTransition(const std::vector< std::vector<state> > & inputs, state output);
                        
protected:

   std::string current_rule;
   unsigned int n_states;
   enum TNeighborhood { vonNeumann, Moore, hexagonal, oneDimensional } neighborhood; 
   static const int N_SUPPORTED_NEIGHBORHOODS = 4;
   static const std::string neighborhood_value_keywords[N_SUPPORTED_NEIGHBORHOODS];

   // we use a lookup table to match inputs to outputs:
   typedef unsigned long long int TBits; // we can use unsigned int if we hit portability issues (not much slower)
   std::vector< std::vector< std::vector<TBits> > > lut; // TBits lut[neighbourhood_size][n_states][n_compressed_rules];
   unsigned int n_compressed_rules;
   std::vector<state> output; // state output[n_rules];

};
#endif
