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

protected:

   std::string LoadRuleTable(std::string filename);
   void PackTransitions(const std::string& symmetries, int n_inputs, 
                        const std::vector< std::pair< std::vector< std::vector<state> >, state> > & transition_table);
   void PackTransition(const std::vector< std::vector<state> > & inputs, state output);
                        
protected:

   string current_rule;
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
