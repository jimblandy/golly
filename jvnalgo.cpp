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
#include "jvnalgo.h"
using namespace std ;
const int NORTH = 1 ;
const int SOUTH = 3 ;
const int EAST = 0 ;
const int WEST = 2 ;
const int FLIPDIR = 2 ;
const int DIRMASK = 3 ;
const int SENS = 1 ;
const int CONF = 0x10 ;
const int OTRANS = 0x20 ;
const int STRANS = 0x40 ;
const int TEXC = 0x80 ;
const int CDEXC = 0x80 ;
const int CEXC = 1 ;
const int BIT_OEXC = 1 ;
const int BIT_SEXC = 2 ;
const int BIT_ONEXC = 4 ;
const int BIT_CEXC = 8 ;
static state compress[256] ;
/**
 *   These are the legal *internal* states.
 */
static state uncompress[] = { 0, /* dead */
			      1, 2, 3, 4, 5, 6, 7, 8, /* construction states */
			      16, 17, /* confluent state */
			      32, 33, 34, 35, /* ordinary */
			      64, 65, 66, 67, /* special */
			      144, 145, /* more confluent state */
			      160, 161, 162, 163, /* ordinary active */
			      192, 193, 194, 195 /* special active */
} ;
static int bits(state mcode, state code, state dir) {
   if (code & (TEXC | OTRANS | CONF | CEXC) == 0)
      return 0 ;
   if (code & CONF) {
      if ((mcode & (OTRANS | STRANS)) && ((mcode & DIRMASK) ^ FLIPDIR) == dir)
         return 0 ;
      if (code & 1)
         return BIT_CEXC ;
   } else {
      if ((code & DIRMASK) != dir)
         return 0 ;
      if (code & OTRANS) {
         if (code & TEXC)
            return BIT_OEXC ;
         return BIT_ONEXC ;
      } else if ((code & (STRANS | TEXC)) == (STRANS | TEXC))
         return BIT_SEXC ;
   }
   return 0 ;
}
static state cres[] = {0x22, 0x23, 0x40, 0x41, 0x42, 0x43, 0x10, 0x20, 0x21} ;
jvnalgo::jvnalgo() {
  for (int i=0; i<256; i++)
    compress[i] = 255 ;
  for (unsigned int i=0; i<sizeof(uncompress)/sizeof(uncompress[0]); i++)
    compress[uncompress[i]] = i ;
}
jvnalgo::~jvnalgo() {
}
state jvnalgo::slowcalc(state, state n, state, state w, state c, state e,
			state, state s, state) {
  c = uncompress[c] ;
  int mbits = bits(c, uncompress[n], SOUTH) | bits(c, uncompress[w], EAST) |
               bits(c, uncompress[e], WEST) | bits(c, uncompress[s], NORTH) ;
   if (c < CONF) {
      if (mbits & (BIT_OEXC | BIT_SEXC))
         c = 2 * c + 1 ;
      else
         c = 2 * c ;
      if (c > 8)
         c = cres[c-9] ;
   } else if (c & CONF) {
      if (mbits & BIT_SEXC)
         c = 0 ;
      else if ((mbits & (BIT_OEXC | BIT_ONEXC)) == BIT_OEXC)
         c = ((c & CDEXC) >> 7) + (CDEXC | CONF) ;
      else
         c = ((c & CDEXC) >> 7) + CONF ;
   } else {
      if (((c & OTRANS) && (mbits & BIT_SEXC)) ||
          ((c & STRANS) && (mbits & BIT_OEXC)))
         c = 0 ;
      else if (mbits & (BIT_SEXC | BIT_OEXC | BIT_CEXC))
         c |= 128 ;
      else
         c &= 127 ;
   }
   return compress[c] ;
}
