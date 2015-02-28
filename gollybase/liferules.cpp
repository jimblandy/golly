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
#include "liferules.h"
#include "util.h"
#include <cstdlib>
#include <string.h>
#include <stdlib.h>

liferules::liferules() {
   canonrule[0] = 0 ;
   alternate_rules = 0 ;
   neighbormask = MOORE ;
   rulebits = 0 ;
   wolfram = 0 ;
   memset(rule0, 0, sizeof(rule0)) ;
   memset(rule1, 0, sizeof(rule1)) ;
}

liferules::~liferules() {
}

// returns a count of the number of bits set in given int
static int bitcount(int v) {
   int r = 0 ;
   while (v) {
      r++ ;
      v &= v - 1 ;
   }
   return r ;
}

// initialize current rule table (rptr points to rule0 or rule1)
void liferules::initruletable(char *rptr, int rulebits, int nmask, int wolfram) {
   int i ;
   if (wolfram >= 0) {
      for (i=0; i<0x1000; i = ((i | 0x888) + 1) & 0x1777)
         rptr[i] = (char)(1 & ((i >> 5) | (wolfram >> (i >> 8)))) ;
   } else {
      for (i=0; i<=0x777; i = (((i | 0x888) + 1) & 0x1777))
         if (rulebits & (1 << (((i & 0x20) >> 1) + bitcount(i & nmask))))
            rptr[i] = 1 ;
         else
            rptr[i] = 0 ;
   }
   for (i=0xfff; i>=0; i--)
      rptr[i] = rptr[i & 0x777] + (rptr[(i >> 1) & 0x777] << 1) ;
   for (i=0xffff; i>=0; i--)
      rptr[i] = rptr[i & 0xfff] + (rptr[(i >> 4) & 0xfff] << 4) ;
}

const char *liferules::setrule(const char *rulestring, lifealgo *algo) {
   int slashcount = 0 ;
   int colonpos = 0 ;
   int addend = 17 ;
   int i ;
   
   // AKT: don't allow empty string
   if (rulestring[0] == 0) {
      return "Rule cannot be empty string." ;
   }

   wolfram = -1 ;
   rulebits = 0 ;
   neighbormask = MOORE ;

   // we might need to emulate B0 rule by using two different rules for odd/even gens
   alternate_rules = false;

   if (rulestring[0] == 'w' || rulestring[0] == 'W') {
      // parse Wolfram 1D rule
      wolfram = 0;
      i = 1;
      while (rulestring[i] >= '0' && rulestring[i] <= '9') {
         wolfram = 10 * wolfram + rulestring[i] - '0' ;
         i++ ;
      }
      if ( wolfram < 0 || wolfram > 254 || wolfram & 1 ) {
         return "Wolfram rule must be an even number from 0 to 254." ;
      }
      if (rulestring[i] == ':') {
         colonpos = i ;
      } else if (rulestring[i]) {
         return "Bad character in Wolfram rule." ;
      }
   } else {
      for (i=0; rulestring[i]; i++) {
         if (rulestring[i] == 'h' || rulestring[i] == 'H') {
            neighbormask = HEXAGONAL ;
         } else if (rulestring[i] == 'v' || rulestring[i] == 'V') {
            neighbormask = VON_NEUMANN ;
         } else if (rulestring[i] == 'b' || rulestring[i] == 'B' || rulestring[i] == '/') {
            if (rulestring[i]== '/' && slashcount++ > 0)
               return "Only one slash permitted in life rule." ;
            addend = 0 ;
         } else if (rulestring[i] == 's' || rulestring[i] == 'S') {
            addend = 17 ;
         } else if (rulestring[i] >= '0' && rulestring[i] <= '8') {
            rulebits |= 1 << (addend + rulestring[i] - '0') ;
         } else if (rulestring[i] == ':' && i > 0) {
            colonpos = i ;
            break ;
         } else {
            // nicer to show given rulestring in error msg
            static std::string badrule;
            badrule = "Unknown rule: ";
            badrule += rulestring;
            return badrule.c_str();
         }
      }
   }
   
   // AKT: check for rule suffix like ":T200,100" to specify a bounded universe
   if (colonpos > 0) {
      const char* err = algo->setgridsize(rulestring + colonpos);
      if (err) return err;
   } else {
      // universe is unbounded
      algo->gridwd = 0;
      algo->gridht = 0;
   }
   
   // maximum # of neighbors is the # of 1 bits in neighbormask, minus the central cell
   int maxneighbors = bitcount(neighbormask) - 1;

   // check if rule contains B0
   if (rulebits & 1) {
      /* Use David Eppstein's idea to change the current rule depending on gen parity.
         If original rule has B0 but not S8:
      
         For even generations, whenever the original rule has a Bx or Sx, omit that 
         bit from the modified rule, and whenever the original rule is missing a
         Bx or Sx, add that bit to the modified rule.
         eg. B03/S23 => B1245678/S0145678.
      
         For odd generations, use Bx if and only if the original rule has S(8-x)
         and use Sx if and only if the original rule has B(8-x).
         eg. B03/S23 => B56/S58.
      
         If original rule has B0 and S8:
         
         Such rules don't strobe, so we just want to invert all the cells.
         The trick is to do both changes: invert the bits, and swap Bx for S(8-x).
         eg. B03/S238 => B123478/S0123467 (for ALL gens).
      */
      if (rulebits & (1 << (17+maxneighbors))) {
         // B0-and-Smax rule
         // change rule for all gens; eg. B03/S238 => B123478/S0123467
         int newrulebits = 0;
         for (i=0; i<=maxneighbors; i++) {
            if ( (rulebits & (1 << i     )) == 0 ) newrulebits |= 1 << (17+maxneighbors-i);
            if ( (rulebits & (1 << (17+i))) == 0 ) newrulebits |= 1 << (maxneighbors-i);
         }
         initruletable(rule0, newrulebits, neighbormask, wolfram);
      } else {
         // B0-not-Smax rule
         alternate_rules = true;
         // change rule for even gens; eg. B03/S23 => B1245678/S0145678
         int newrulebits = 0;
         for (i=0; i<=maxneighbors; i++) {
            if ( (rulebits & (1 << i     )) == 0 ) newrulebits |= 1 << i;
            if ( (rulebits & (1 << (17+i))) == 0 ) newrulebits |= 1 << (17+i);
         }
         initruletable(rule0, newrulebits, neighbormask, wolfram);
         // change rule for odd gens; eg. B03/S23 => B56/S58
         newrulebits = 0;
         for (i=0; i<=maxneighbors; i++) {
            if ( rulebits & (1 << (17+maxneighbors-i)) ) newrulebits |= 1 << i;
            if ( rulebits & (1 << (   maxneighbors-i)) ) newrulebits |= 1 << (17+i);
         }
         initruletable(rule1, newrulebits, neighbormask, wolfram);
      }
   } else {
      // not doing B0 emulation so use rule0 for all gens
      initruletable(rule0, rulebits, neighbormask, wolfram);
   }
   
   // AKT: store valid rule in canonical format for getrule()
   int p = 0 ;
   if (wolfram >= 0) {
      sprintf(canonrule, "W%d", wolfram) ;
      while (canonrule[p]) p++ ;
   } else {
      canonrule[p++] = 'B' ;
      for (i=0; i<=maxneighbors; i++) {
         if (rulebits & (1 << i)) canonrule[p++] = '0' + (char)i ;
      }
      canonrule[p++] = '/' ;
      canonrule[p++] = 'S' ;
      for (i=0; i<=maxneighbors; i++) {
         if (rulebits & (1 << (17+i))) canonrule[p++] = '0' + (char)i ;
      }
      if (neighbormask == HEXAGONAL) canonrule[p++] = 'H' ;
      if (neighbormask == VON_NEUMANN) canonrule[p++] = 'V' ;
   }
   if (algo->gridwd > 0 || algo->gridht > 0) {
      // algo->setgridsize() was successfully called above, so append suffix
      const char* bounds = algo->canonicalsuffix() ;
      int i = 0 ;
      while (bounds[i]) canonrule[p++] = bounds[i++] ;
   }
   canonrule[p] = 0 ;

   return 0 ;
}

const char* liferules::getrule() {
   return canonrule ;
}

// B3/S23 -> (1 << 3) + (1 << (17 + 2)) + (1 << (17 + 3)) = 0x180008
bool liferules::isRegularLife() {
  return (neighbormask == MOORE && rulebits == 0x180008 && wolfram < 0) ;
}
