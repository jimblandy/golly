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
#include "rd_algo.h"

#include "util.h"      // for lifegetuserrules, lifegetrulesdir, lifewarning

#include <algorithm>
#include <map>
#include <sstream>
using namespace std;

#include <string.h> // for strchr

int rd_algo::NumCellStates()
{
   return 256;
}

const char* rd_algo::setrule(const char* s)
{
   const char *colonptr = strchr(s, ':');
   string rule_name(s);
   if (colonptr) 
      rule_name.assign(s,colonptr);
    // apply the change

   if(rule_name=="schlogl")
   {

   }
   else
   {
      return "error";
   }
   
   // check for rule suffix like ":T200,100" to specify a bounded universe
   if (colonptr) {
      const char* err = setgridsize(colonptr);
      if (err) return err;
   } else {
      // universe is unbounded
      gridwd = 0;
      gridht = 0;
   }

   // set canonical rule string returned by getrule()
   this->current_rule = rule_name.c_str();
   if (gridwd > 0 || gridht > 0) {
      // setgridsize() was successfully called above, so append suffix
      string bounds = canonicalsuffix();
      this->current_rule += bounds;
   }
   
   maxCellStates = 256;
   ghashbase::setrule(rule_name.c_str());
   return NULL;
}

const char* rd_algo::getrule() {
   return this->current_rule.c_str();
}

const char* rd_algo::DefaultRule() {
   return "schlogl";
}

float frand(float lower,float upper) {
    return lower + rand()*(upper-lower)/RAND_MAX;
}

rd_algo::rd_algo()
{
}

rd_algo::~rd_algo()
{
}

// --- the update function ---
state rd_algo::slowcalc(state nw, state n, state ne, state w, state c, state e,
                        state sw, state s, state se) 
{
    const float speed=0.1;
    const float min_a=-1.0,max_a=1.0;
    // first we need to extract the values of chemicals a and b in the neighboring cells
    state vals[9] = {c,n,e,s,w,nw,ne,se,sw};
    float a[9];
    int max_int = 255;
    for(int i=0;i<9;i++)
        a[i] = min_a + vals[i] * (max_a-min_a) / max_int;// + frand(-0.1,0.1);
    // compute the Laplacian
    float dda = a[1]+a[2]+a[3]+a[4]+a[5]+a[6]+a[7]+a[8] - 8*a[0];
    // compute the new rate of change
    float da = dda + a[0] - a[0]*a[0]*a[0];
    a[0] += speed * da;
    // encode a and b into the output char
    int out = int((a[0] - min_a)/(max_a-min_a) * max_int + 0.5); // round
    return min(max_int,max(0,out));
}

static lifealgo *creator() { return new rd_algo(); }

void rd_algo::doInitializeAlgoInfo(staticAlgoInfo &ai) 
{
   ghashbase::doInitializeAlgoInfo(ai);
   ai.setAlgorithmName("ReactionDiffusion");
   ai.setAlgorithmCreator(&creator);
   ai.minstates = 2;
   ai.maxstates = 256;
   // init default color scheme
   ai.defgradient = true;              // use gradient
   ai.defr1 = 255;                     // start color = red
   ai.defg1 = 0;
   ai.defb1 = 0;
   ai.defr2 = 255;                     // end color = yellow
   ai.defg2 = 255;
   ai.defb2 = 0;
   // if not using gradient then set all states to white
   for (int i=0; i<256; i++) {
      ai.defr[i] = ai.defg[i] = ai.defb[i] = 255;
   }
}
