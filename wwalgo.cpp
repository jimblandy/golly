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
#include "wwalgo.h"

// AKT: for case-insensitive string comparison
#include <string.h>
#ifndef WIN32
   #define stricmp strcasecmp
#endif

using namespace std ;

// AKT: this algo only supports a single rule
const char WW_RULE[] = "WireWorld";

const char* wwalgo::setrule(const char *s) {
   if (stricmp(s, WW_RULE) == 0) return NULL;
   return "This algorithm only supports a single rule (WireWorld).";
}

const char* wwalgo::getrule() {
   return WW_RULE;
}

const char* wwalgo::DefaultRule() {
   return WW_RULE;
}

// colors for each cell state match those used at
// http://www.quinapalus.com/wi-index.html
static unsigned char wwcolors[] = {
     0,   0,   0,    // not used (replaced by user's dead cell color)
     0, 128, 255,    // 1 = lightish blue
   255, 255, 255,    // 2 = white
   255, 128,   0     // 3 = orange
};

unsigned char* wwalgo::GetColorData(int& numcolors)
{
   numcolors = sizeof(wwcolors) / (3 * sizeof(wwcolors[0]));
   return wwcolors;
}

wwalgo::wwalgo() {
}

wwalgo::~wwalgo() {
}

state wwalgo::slowcalc(state nw, state n, state ne, state w, state c, state e,
                       state sw, state s, state se) {
  switch (c) {
  case 0: return 0 ;
  case 1: return 2 ;
  case 2: return 3 ;
  case 3:
    if ((((1+(nw==1)+(n==1)+(ne==1)+(w==1)+(e==1)+(sw==1)+
	     (s==1)+(se==1))) | 1) == 3)
      return 1 ;
    else
      return 3 ;
  default:
    return 0 ; // should throw an error here
  }
}
