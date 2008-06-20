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
#include "generationsalgo.h"
#include <string.h>
using namespace std ;

int generationsalgo::NumCellStates() {
   return maxCellStates ;
}

const char* generationsalgo::setrule(const char *s) {
   if ((int)strlen(s) + 10 > MAXRULESIZE)
      return "Rule too long for Generations" ;
   // a legal rule goes:
   // [0-8]+/[1-8]+/[1-9][0-9]*
   const char *p = s ;
   int tbornbits = 0, tstaybits = 0, tnumstates = 0 ;
   while ('0' <= *p && *p <= '8') {
      tstaybits |= 1<<(*p-'0') ;
      p++ ;
   }
   if (*p != '/')
      return "Missing expected slash in Generations rule" ;
   p++ ;
   while ('1' <= *p && *p <= '8') {
      tbornbits |= 1<<(*p-'0') ;
      p++ ;
   }
   if (*p != '/')
      return "Missing expected slash in Generations rule" ;
   p++ ;
   while ('0' <= *p && *p <= '9') {
      tnumstates = 10 * tnumstates + *p - '0' ;
      p++ ;
      if (tnumstates > 256)
	 return "Number of states too high in Generations rule" ;
   }
   if (tnumstates < 2)
      return "Number of states too low in Generations rule" ;
   if (*p)
      return "Extra stuff at end of Generations rule" ;
   bornbits = tbornbits ;
   staybits = tstaybits ;
   maxCellStates = tnumstates ;
   strcpy(rulecopy, s) ;
   ghashbase::setrule(rulecopy) ;
   return 0 ;
}

char generationsalgo::rulecopy[MAXRULESIZE] ;

const char* generationsalgo::getrule() {
   return rulecopy ;
}

static const char *DEFAULTRULE = "12/34/3" ;
const char* generationsalgo::DefaultRule() {
   return DEFAULTRULE ;
}

/*
 *   These colors are not presently used; we'll go back to them
 *   when we permit colors to change with rules.
 */
static unsigned char defcolors[256*3] ;
static int lastcolorset = -1 ;
unsigned char *generationsalgo::GetColorData(int &numcolors) {
   numcolors = maxCellStates ;
   if (lastcolorset != numcolors) {
      /* use Yellow (255,255,0) -> Red (255,0,0) */
      if (numcolors <= 2) {
	 defcolors[3] = 255 ;
	 defcolors[4] = 255 ;
	 defcolors[5] = 0 ;
      } else {
	 for (int i=1; i<numcolors; i++) {
	    defcolors[i*3] = 255 ;
	    defcolors[i*3+1] = (unsigned char)
                                 (255 * (numcolors-i-1) / (numcolors-2)) ;
            defcolors[i*3+2] = 0 ;
	 }
      }
   }
   return defcolors ;
}

generationsalgo::generationsalgo() {
   // we may need this to be >2 here so it's recognized as multistate
   maxCellStates = 3 ;
}

generationsalgo::~generationsalgo() {}

state generationsalgo::slowcalc(state nw, state n, state ne, state w, state c,
				state e, state sw, state s, state se) {
   int nn = (nw==1)+(n==1)+(ne==1)+(w==1)+(e==1)+(sw==1)+(s==1)+(se==1) ;
   if (c==1 && (staybits & (1 << nn)))
      return 1 ;
   if (c==0 && (bornbits & (1 << nn)))
      return 1 ;
   if (c > 0 && c+1 < maxCellStates)
      return c + 1 ;
   return 0 ;
}
static lifealgo *creator() { return new generationsalgo() ; }
void generationsalgo::doInitializeAlgoInfo(staticAlgoInfo &ai) {
   ghashbase::doInitializeAlgoInfo(ai) ;
   ai.setAlgorithmName("Generations") ;
   ai.setAlgorithmCreator(&creator) ;
   ai.setStatusRGB(255, 226, 226) ;    // pale pink
}
