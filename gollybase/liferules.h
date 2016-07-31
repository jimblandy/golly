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
const int ALL3X3 = 512 ;
class liferules {
public:
   liferules() ;
   ~liferules() ;
   // string returned by setrule is any error
   const char *setrule(const char *s, lifealgo *algo) ;
   const char *getrule() ;
   
   // AKT: we need 2 tables to support B0-not-Smax rule emulation
   // where max is 8, 6 or 4 depending on the neighborhood
   char rule0[65536];        // rule table for even gens if rule has B0 but not Smax,
                             // or for all gens if rule has no B0, or it has B0 *and* Smax
   char rule1[65536];        // rule table for odd gens if rule has B0 but not Smax
   bool alternate_rules;     // set by setrule; true if rule has B0 but not Smax
   
   // AKT: support for various neighborhoods
   // rowett: support for hensel2
   enum neighborhood_masks {
      MOORE = 0x1ff,         // all 8 neighbors
      HEXAGONAL = 0x0fe,     // ignore NE and SW neighbors
      VON_NEUMANN = 0x0ba    // 4 orthogonal neighbors
   } ;

   bool isRegularLife() ;    // is this B3/S23?
   bool isHexagonal() const { return neighbormask == HEXAGONAL ; }
   bool isVonNeumann() const { return neighbormask == VON_NEUMANN ; }
   bool isWolfram() const { return wolfram >= 0 ; }

private:
   // canonical version of valid rule passed into setrule
   char canonrule[MAXRULESIZE] ;
   neighborhood_masks neighbormask ;
   bool totalistic ;         // is rule totalistic?
   int neighbors ;           // number  of neighbors
   int rulebits ;            // bitmask of neighbor counts used (9 birth, 9 survival)
   int letter_bits[18] ;     // bitmask for non-totalistic letters used
   int neg_letter_bits[18] ; // bitmask for non-totalistic negative letters used
   int wolfram ;             // >= 0 if Wn rule (n is even and <= 254)
   int survival_offset ;     // bit offset in rulebits for survival
   int max_letters[18] ;     // maximum number of letters per neighbor count
   void initRule() ;
   void setTotalistic(int value, bool survival) ;
   int flipBits(int x) ;
   int rotateBits90Clockwise(int x) ;
   void setSymmetrical512(int x, int b) ;
   void setSymmetrical(int value, bool survival, int lindex, int normal) ;
   void setTotalisticRuleFromString(const char *rule, bool survival) ;
   void setRuleFromString(const char *rule, bool survival) ;
   void createWolframMap() ;
   void createRuleMap(const char *birth, const char *survival) ;
   void createB0SmaxRuleMap(const char *birth, const char *survival) ;
   void createB0EvenRuleMap(const char *birth, const char *survival) ;
   void createB0OddRuleMap(const char *birth, const char *survival) ;
   void convertTo4x4Map(char *which) ;
   void createCanonicalName(lifealgo *algo) ;
   void removeChar(char *string, char skip) ;
   bool lettersValid(const char *part) ;
   int addLetters(int count, int p) ;
   const char *valid_rule_letters ;
   const char *rule_letters[4] ;
   const int *rule_neighborhoods[4] ;
   char rule3x3[ALL3X3] ;  // all 3x3 cell mappings 012345678->4'
} ;
#endif
