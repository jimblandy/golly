                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2005 Andrew Trevorrow and Tomas Rokicki.

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
#include <string.h>
#include <stdlib.h>

liferules::liferules() {
   rule = 0 ;
   setrule("B3/S23") ;
}

liferules::~liferules() {
   if (rule)
      free((void *)rule) ;
}

// returns a count of the number of bits set in given int
static int bc(int v) {
   int r = 0 ;
   while (v) {
      r++ ;
      v &= v - 1 ;
   }
   return r ;
}

// initialize current rule table (rptr points to rule0 or rule1)
void initruletable(char *rptr, int rulebits, int hexmask, int wolfram) {
   int i;
   if (wolfram >= 0) {
      for (i=0; i<0x1000; i = ((i | 0x888) + 1) & 0x1777)
         rptr[i] = (char)(1 & ((i >> 5) | (wolfram >> (i >> 8)))) ;
   } else {
      for (i=0; i<=0x777; i = (((i | 0x888) + 1) & 0x1777))
         if (rulebits & (1 << (((i & 0x20) >> 1) + bc(i & hexmask))))
            rptr[i] = 1 ;
         else
            rptr[i] = 0 ;
   }
   for (i=0xfff; i>=0; i--)
      rptr[i] = rptr[i & 0x777] + (rptr[(i >> 1) & 0x777] << 1) ;
   for (i=0xffff; i>=0; i--)
      rptr[i] = rptr[i & 0xfff] + (rptr[(i >> 4) & 0xfff] << 4) ;
}

const char *liferules::setrule(const char *rulestring) {
   wolfram = -1 ;
   rulebits = 0 ;
   hexmask = 0x777 ;
   int addend = 17 ;
   int i ;

   // set rule string even if bad character detected
   if (rule)
      free((void *)rule) ;
   rule = strdup(rulestring) ;

   // need to emulate B0-not-S8 rule?
   hasB0notS8 = false;

   for (i=0; rulestring[i]; i++) {
      if (rulestring[i] == 'h' || rulestring[i] == 'H') {
         hexmask = 0x673 ;
      } else if (rulestring[i] == 'b' || rulestring[i] == 'B' || rulestring[i] == '/') {
         addend = 0 ;
      } else if (rulestring[i] == 's' || rulestring[i] == 'S') {
         addend = 17 ;
      } else if (rulestring[i] >= '0' && rulestring[i] <= '8') {
         rulebits |= 1 << (addend + rulestring[i] - '0') ;
      } else if (rulestring[i] == 'w' || rulestring[i] == 'W') {
         wolfram = atol(rulestring+i+1) ;
         if ( wolfram < 0 || wolfram > 254 || wolfram & 1 ) {
            // when we support toroidal universe we can allow all numbers from 0..255!!!
            return "Wolfram rule must be an even number from 0 to 254." ;
         }
         break ;
      } else {
         return "Bad character in rule string." ;
      }
   }

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
      int origrulebits = rulebits;
      if (rulebits & (1 << (17+8))) {
         // B0-and-S8 rule
         // change rule for all gens; eg. B03/S238 => B123478/S0123467
         rulebits = 0;
         for (i=0; i<9; i++) {
            if ( (origrulebits & (1 << i     )) == 0 ) rulebits |= 1 << (17+8-i);
            if ( (origrulebits & (1 << (17+i))) == 0 ) rulebits |= 1 << (8-i);
         }
         initruletable(rule0, rulebits, hexmask, wolfram);
      } else {
         // B0-not-S8 rule
         hasB0notS8 = true;
         // change rule for even gens; eg. B03/S23 => B1245678/S0145678
         rulebits = 0;
         for (i=0; i<9; i++) {
            if ( (origrulebits & (1 << i     )) == 0 ) rulebits |= 1 << i;
            if ( (origrulebits & (1 << (17+i))) == 0 ) rulebits |= 1 << (17+i);
         }
         initruletable(rule0, rulebits, hexmask, wolfram);
         // change rule for odd gens; eg. B03/S23 => B56/S58
         rulebits = 0;
         for (i=0; i<9; i++) {
            if ( origrulebits & (1 << (17+8-i)) ) rulebits |= 1 << i;
            if ( origrulebits & (1 << (   8-i)) ) rulebits |= 1 << (17+i);
         }
         initruletable(rule1, rulebits, hexmask, wolfram);
      }
   } else {
      // rule doesn't have B0 so we'll use rule0 for all gens
      initruletable(rule0, rulebits, hexmask, wolfram);
   }

   return 0 ;
}

// B3S23 -> (1 << 3) + (1 << (17 + 2)) + (1 << (17 + 3)) =
//          0x180008
bool liferules::isRegularLife() {
  return (hexmask == 0x777 && rulebits == 0x180008 && wolfram < 0) ;
}

liferules global_liferules ;
