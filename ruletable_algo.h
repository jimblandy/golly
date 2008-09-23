                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2008 Andrew Trevorrow and Tomas Rokicki.

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
#include <map>
#include <string>
#include <vector>
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

   string LoadRuleTable(string filename);

protected:

   std::string current_rule;
   unsigned int n_states;
   unsigned int neighbourhood_size; // 5=von Neumann; 9=Moore; others currently unsupported

   std::map< std::vector< std::vector<state> > , state> transition_table;
   // e.g. "1,[2,3,4],5,[2,6],7 -> 8" captures 6 different transitions, plus all their symmetries

   enum { none, withRotations } symmetries;
};
#endif
