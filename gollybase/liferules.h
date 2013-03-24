                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2013 Andrew Trevorrow and Tomas Rokicki.

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
 *   This class implements the rules supported by QuickLife and HashLife.
 *   A rule lookup table is used for computing a new 2x2 grid from
 *   a provided 4x4 grid (two tables are used to emulate B0-not-Smax rules).
 *   The input is a 16-bit integer, with the most significant bit the
 *   upper left corner, bits going horizontally and then down.  The output
 *   is a 6-bit integer, with the top two bits indicating the top row of
 *   the 2x2 output grid and the least significant two bits indicating the
 *   bottom row.  The middle two bits are always zero.
 */
#ifndef LIFERULES_H
#define LIFERULES_H
#include "lifealgo.h"
const int MAXRULESIZE = 200 ;
class liferules {
public:
   liferules() ;
   ~liferules() ;
   // string returned by setrule is any error
   const char *setrule(const char *s, lifealgo *algo) ;
   const char *getrule() ;
   
   // AKT: we need 2 tables to support B0-not-Smax rule emulation
   // where max is 8, 6 or 4 depending on the neighborhood
   char rule0[65536];      // rule table for even gens if rule has B0 but not Smax,
                           // or for all gens if rule has no B0, or it has B0 *and* Smax
   char rule1[65536];      // rule table for odd gens if rule has B0 but not Smax
   bool alternate_rules;   // set by setrule; true if rule has B0 but not Smax
   
   // AKT: support for various neighborhoods
   enum neighborhood_masks {
      MOORE = 0x777,       // all 8 neighbors
      HEXAGONAL = 0x673,   // ignore NE and SW neighbors
      VON_NEUMANN = 0x272  // 4 orthogonal neighbors
   } ;

   bool isRegularLife() ;  // is this B3/S23?
   bool isHexagonal() const { return neighbormask == HEXAGONAL ; }
   bool isVonNeumann() const { return neighbormask == VON_NEUMANN ; }
   bool isWolfram() const { return wolfram >= 0 ; }

private:
   void initruletable(char *rptr, int rulebits, int nmask, int wolfram) ;
   // canonical version of valid rule passed into setrule
   char canonrule[MAXRULESIZE] ;
   neighborhood_masks neighbormask ;
   int rulebits ;
   int wolfram ;           // >= 0 if Wn rule (n is even and <= 254)
} ;
#endif
