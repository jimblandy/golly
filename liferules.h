                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2006 Andrew Trevorrow and Tomas Rokicki.

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
 *   This class implements the life rules; parses them, validates them,
 *   builds the 65K entry table for them.  Normally this is statically
 *   instantiated and shared by all algorithms, but there is no absolute
 *   requirement this be the case.
 *
 *   A rule lookup table is used for computing a new 2x2 grid from
 *   a provided 4x4 grid (two tables are used to emulate B0-not-S8 rules).
 *   The input is a 16-bit integer, with the most significant bit the
 *   upper left corner, bits going horizontally and then down.  The output
 *   is a 6-bit integer, with the top two bits indicating the top row of
 *   the 2x2 output grid and the least significant two bits indicating the
 *   bottom row.  The middle two bits are always zero.
 */
#ifndef LIFERULES_H
#define LIFERULES_H
class liferules {
public:
   liferules() ;
   ~liferules() ;
   // string returned by setrule is any error
   const char *setrule(const char *s) ;
   const char *getrule() const { return rule ; }
   // AKT: we need 2 tables to support B0-not-S8 rule emulation
   char rule0[65536];      // rule table for even gens if rule has B0 but not S8,
                           // or for all gens if rule has no B0, or it has B0 *and* S8
   char rule1[65536];      // rule table for odd gens if rule has B0 but not S8
   bool hasB0notS8;        // set by setrule; true if rule has B0 but not S8
   bool isRegularLife() ;  // is this s23b3?
private:
   const char *rule ;      // copy of string passed into setrule
   int hexmask ;           // really neighbormask
   int rulebits ;
   int wolfram ;
} ;
extern liferules global_liferules ;
#endif
