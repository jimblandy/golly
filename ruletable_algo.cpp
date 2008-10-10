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
#include "ruletable_algo.h"

#include "util.h"      // AKT: for lifegetrulesdir()

// for case-insensitive string comparison
#include <string.h>
#ifndef WIN32
   #define stricmp strcasecmp
   #define strnicmp strncasecmp
#endif

#include <algorithm>
#include <iterator>
#include <functional>
#include <fstream>
#include <sstream>
using namespace std ;

int ruletable_algo::NumCellStates()
{
   return this->n_states;
}

const char* ruletable_algo::setrule(const char* s) 
{
   string ret = LoadRuleTable(s) ;
   if(!ret.empty())
      return "error";

   this->current_rule = s;
   maxCellStates = this->n_states;
   ghashbase::setrule(s);
   return NULL;
}

vector<string> tokenize(const string& str,const string& delimiters)
{
   vector<string> tokens;

   // skip delimiters at beginning.
   string::size_type lastPos = str.find_first_not_of(delimiters, 0);

   // find first "non-delimiter".
   string::size_type pos = str.find_first_of(delimiters, lastPos);

   while (string::npos != pos || string::npos != lastPos)
   {
      // found a token, add it to the vector.
      tokens.push_back(str.substr(lastPos, pos - lastPos));

      // skip delimiters.  Note the "not_of"
      lastPos = str.find_first_not_of(delimiters, pos);

      // find next "non-delimiter"
      pos = str.find_first_of(delimiters, lastPos);
   }

   return tokens;
}

bool starts_with(const string& line,const string& keyword)
{
   return strnicmp(line.c_str(),keyword.c_str(),keyword.length())==0;
}

const char *defaultRuleData[] = {
   "n_states:8", "neighborhood_size:5", "symmetries:withRotations",
   "000000", "000012", "000020", "000030", "000050", "000063", "000071",
   "000112", "000122", "000132", "000212", "000220", "000230", "000262",
   "000272", "000320", "000525", "000622", "000722", "001022", "001120",
   "002020", "002030", "002050", "002125", "002220", "002322", "005222",
   "012321", "012421", "012525", "012621", "012721", "012751", "014221",
   "014321", "014421", "014721", "016251", "017221", "017255", "017521",
   "017621", "017721", "025271", "100011", "100061", "100077", "100111",
   "100121", "100211", "100244", "100277", "100511", "101011", "101111",
   "101244", "101277", "102026", "102121", "102211", "102244", "102263",
   "102277", "102327", "102424", "102626", "102644", "102677", "102710",
   "102727", "105427", "111121", "111221", "111244", "111251", "111261",
   "111277", "111522", "112121", "112221", "112244", "112251", "112277",
   "112321", "112424", "112621", "112727", "113221", "122244", "122277",
   "122434", "122547", "123244", "123277", "124255", "124267", "125275",
   "200012", "200022", "200042", "200071", "200122", "200152", "200212",
   "200222", "200232", "200242", "200250", "200262", "200272", "200326",
   "200423", "200517", "200522", "200575", "200722", "201022", "201122",
   "201222", "201422", "201722", "202022", "202032", "202052", "202073",
   "202122", "202152", "202212", "202222", "202272", "202321", "202422",
   "202452", "202520", "202552", "202622", "202722", "203122", "203216",
   "203226", "203422", "204222", "205122", "205212", "205222", "205521",
   "205725", "206222", "206722", "207122", "207222", "207422", "207722",
   "211222", "211261", "212222", "212242", "212262", "212272", "214222",
   "215222", "216222", "217222", "222272", "222442", "222462", "222762",
   "222772", "300013", "300022", "300041", "300076", "300123", "300421",
   "300622", "301021", "301220", "302511", "401120", "401220", "401250",
   "402120", "402221", "402326", "402520", "403221", "500022", "500215",
   "500225", "500232", "500272", "500520", "502022", "502122", "502152",
   "502220", "502244", "502722", "512122", "512220", "512422", "512722",
   "600011", "600021", "602120", "612125", "612131", "612225", "700077",
   "701120", "701220", "701250", "702120", "702221", "702251", "702321",
   "702525", "702720", 0 } ;

/*
 *   Make sure ifstream goes away even if we return, despite using a
 *   pointer variable.  Probably a cleaner way to do this.
 */
struct freeme {
   freeme(ifstream *pp) : p(pp) {}
   ~freeme() { if (p) delete p ; }   // AKT: why was this commented out?
   ifstream *p ;
} ;

string ruletable_algo::LoadRuleTable(string rule)
{
   const string comment_keyword = "#";
   const string symmetries_keyword = "symmetries:";
   const string neighbourhood_size_keyword = "neighborhood_size:";
   const string n_states_keyword = "n_states:";
   const string variable_keyword = "var ";
   const string withRotations_symmetry_keyword = "withRotations";
   const string none_symmetry_keyword = "none";

   int isDefaultRule = (strcmp(rule.c_str(), DefaultRule()) == 0) ;
   string line ;
   ifstream *in = 0 ;
   freeme freeme(0) ;
   int lineno = 0 ;
   if (!isDefaultRule) 
   {
      // AKT: we need to prepend the full path to the rules dir because when Golly
      // runs a script it temporarily changes the cwd to the location of the script
      string full_filename = lifegetrulesdir() ;
      int istart = full_filename.size() ;
      full_filename += rule + ".table" ;
      for (unsigned int i=istart; i<full_filename.size(); i++)
         if (full_filename[i] == '/' || full_filename[i] == '\\' ||
            full_filename[i] == ':')
         full_filename[i] = '-' ;
      in = new ifstream(full_filename.c_str());
      freeme.p = in ; // make sure it goes away if we return with an error
      if (in == 0 || !in->good()) 
         return "Failed to open file: "+full_filename;
   }
   else {   }

   this->neighbourhood_size = 5; // default
   this->symmetries = withRotations; // default
   this->n_states = 8;  // default

   map< string, vector<state> > variables;
   map<vector<vector<state> >, state> transition_table;

   for (;;) 
   {
      if (isDefaultRule) {
         if (defaultRuleData[lineno] == 0)
            break ;
         line = defaultRuleData[lineno] ;
      } else {
         if (!in->good())
            break ;
         getline(*in, line) ;
      }
      lineno++ ;
      int allws = 1 ;
      for (unsigned int i=0; i<line.size(); i++)
         if (line[i] > ' ') {
            allws = 0 ;
            break ;
         }
      if(starts_with(line,comment_keyword) || allws)
         continue; // comment line
      else if(starts_with(line,n_states_keyword))
      {
         // parse the rest of the line
         if(sscanf(line.c_str()+n_states_keyword.length(),"%d",&this->n_states)!=1)
            return "Error reading file: "+line;
      }
      else if(starts_with(line,symmetries_keyword))
      {
         string remaining(line.begin()+symmetries_keyword.length(),line.end());
         if(starts_with(remaining,withRotations_symmetry_keyword))
            this->symmetries = withRotations;
         else if(starts_with(remaining,none_symmetry_keyword))
            this->symmetries = none;
         else
            return "Error reading file: "+line;
      }
      else if(starts_with(line,neighbourhood_size_keyword))
      {
         // parse the rest of the line
         if(sscanf(line.c_str()+neighbourhood_size_keyword.length(),"%d",&this->neighbourhood_size)!=1)
            return "Error reading file: " + line;
         if(this->neighbourhood_size!=5 && this->neighbourhood_size!=9)
            return "Error reading file, unsupported neighbourhood_size: " + line;
      }
      else if(starts_with(line,variable_keyword))
      {
         // parse the rest of the line for the variable
         vector<string> tokens = tokenize(line,"= {,}");
         string variable_name = tokens[1];
         vector<state> states;
         if(tokens.size()<4)
            return "Error reading file: "+line;
         for(unsigned int i=2;i<tokens.size();i++)
         {
            istringstream iss(tokens[i]);
            int s;
            iss >> s;
            states.push_back((state)s);
         }
         variables[variable_name] = states;
      }
      else
      {
         // must be a transitions line
         vector<vector<state> > inputs;
         state output;
         if(this->n_states<=10 && variables.empty())
         {
            // if there are single-digit states and no variables then use compressed form
            // e.g. 012345 for 0,1,2,3,4 -> 5
            if(line.length() < this->neighbourhood_size+1) // we allow for comments after the rule
               return "Error reading line: "+line;
            for(unsigned int i=0;i<this->neighbourhood_size;i++)
            {
               char c = line[i];
               if(c<'0' || c>'9')
                  return "Error reading line: "+line;
               inputs.push_back(vector<state>(1,c-'0'));
            }
            unsigned char c = line[this->neighbourhood_size];
            if(c<'0' || c>'9')
               return "Error reading line: "+line;
            output = c-'0';
         }
         else 
         {
            vector<string> tokens = tokenize(line,", #\t");
            if(tokens.size() < this->neighbourhood_size+1)
            {
               ostringstream oss;
               oss << "Error reading transition line, too few entries (" << tokens.size() << ", expected " <<
                  (this->neighbourhood_size+1) << ") on line: " << line;
               return oss.str();
            }
            // parse the inputs
            for(unsigned int i=0;i<this->neighbourhood_size;i++)
            {
               map<string,vector<state> >::const_iterator found;
               found = variables.find(tokens[i]);
               if(found!=variables.end())
               {
                  // found a variable name, replace it with the variable's contents
                  inputs.push_back(found->second);
                  // (variable must have been declared before this point in the file or it won't be found)
               }
               else
               {
                  // found a single state value
                  int s;
                  if(sscanf(tokens[i].c_str(),"%d",&s)!=1)
                     return "Error reading transition line, expecting a value: "+line;
                  inputs.push_back(vector<state>(1,s));
               }
            }
            // parse the output
            int outp;
            if(sscanf(tokens[this->neighbourhood_size].c_str(),"%d",&outp)!=1)
               return "Error reading transition line, failed to parse output state: "+line;
            output = (state)outp;
         }
         transition_table[inputs]=output;
      }
   }
   // now convert transition table to bitmask lookup
   {
      unsigned int n_bits = sizeof(TBits)*8;
      int n_rotations,rotation_skip;
      if(this->symmetries == withRotations)
      {
         n_rotations=4;
         if(this->neighbourhood_size==5)
            rotation_skip=1;
         else // neighbourhood_size==9
            rotation_skip=2;
      }
      else
      {
         n_rotations=1;
         rotation_skip=1;
      }
      unsigned int M = transition_table.size() * n_rotations; // (we need to expand out symmetry)
      unsigned int MC = (M+n_bits-1) / n_bits; // the rule table is compressed down to 1 bit each
      // initialize lookup table to all bits turned off 
      this->lut.assign(neighbourhood_size,vector< vector<TBits> >(this->n_states,vector<TBits>(MC,0))); 
      this->output.resize(M);
      // work through the rules, filling the bit masks
      unsigned int iRule=0,iRuleC,iBit,iNbor;
      TBits mask;
      // (each transition rule looks like, e.g. 1,[2,3,4],[2,4],0,3 -> 0 )
      for(map<vector<vector<state> >,state >::const_iterator rule_it = transition_table.begin();rule_it!=transition_table.end();rule_it++)
      {
         const vector<vector<state> >& rule = rule_it->first;
         for(int iRot=0;iRot<n_rotations;iRot++)
         {
            this->output[iRule] = rule_it->second;
            iBit = iRule % n_bits;
            iRuleC = (iRule-iBit)/n_bits; // the compressed index of the rule
            mask = (TBits)1 << iBit; // (we need to ensure this is a 64-bit shift, not a 32-bit shift)
            for(iNbor=0;iNbor<this->neighbourhood_size;iNbor++)
            {
               const vector<state>& possibles = rule[iNbor];
               for(vector<state>::const_iterator poss_it=possibles.begin();poss_it!=possibles.end();poss_it++)
               {
                  if(iNbor>0)
                     this->lut[1+((iNbor-1+iRot*rotation_skip)%(this->neighbourhood_size-1))][*poss_it][iRuleC] |= mask;
                  else
                     this->lut[iNbor][*poss_it][iRuleC] |= mask;
               }
            }
            iRule++; // this is the index of the rule after expansion for symmetry
         }
      }
   }
   return string(""); // success
}

const char* ruletable_algo::getrule() {
   return this->current_rule.c_str();
}

const char* ruletable_algo::DefaultRule() {
   return "Langtons-Loops";
}

ruletable_algo::ruletable_algo()
{
}

ruletable_algo::~ruletable_algo()
{
}

// --- the update function ---
state ruletable_algo::slowcalc(state nw, state n, state ne, state w, state c, state e,
                        state sw, state s, state se) 
{
   const unsigned int MC = this->lut[0][0].size();
   unsigned int iRule;
   TBits is_match;

   for(iRule=0;iRule<MC;iRule++)
   {
      // is there a match for any of the sizeof(TBits)*8 rules within iRule?
      if(this->neighbourhood_size==5)
         is_match = (TBits)-1 & this->lut[0][c][iRule] & this->lut[1][n][iRule] & this->lut[2][e][iRule] & 
            this->lut[3][s][iRule] & this->lut[4][w][iRule];
      else // this->neighbourhood_size==9
         is_match = (TBits)-1 & this->lut[0][c][iRule] & this->lut[1][n][iRule] & this->lut[2][ne][iRule] & 
         this->lut[3][e][iRule] & this->lut[4][se][iRule] & this->lut[5][s][iRule] & this->lut[6][sw][iRule] & 
            this->lut[7][w][iRule] & this->lut[8][nw][iRule];
      // if one of them matched, return the output of the first
      if(is_match)
      {
         // find the least significant bit of is_match
         unsigned int iBit=0;
         while(!(is_match&((TBits)1<<iBit)))
            ++iBit;
         return this->output[ iRule*sizeof(TBits)*8 + iBit ];
      }
   }
   return c; // default: no change
}

static lifealgo *creator() { return new ruletable_algo() ; }

void ruletable_algo::doInitializeAlgoInfo(staticAlgoInfo &ai) 
{
   ghashbase::doInitializeAlgoInfo(ai) ;
   ai.setAlgorithmName("RuleTable") ;
   ai.setAlgorithmCreator(&creator) ;
   ai.minstates = 2 ;
   ai.maxstates = 256 ;
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
