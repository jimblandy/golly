// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "jvnalgo.h"

// for case-insensitive string comparison
#include <cstring>
#ifndef WIN32
   #define stricmp strcasecmp
#endif
#include <string>

using namespace std ;

// this algorithm supports three rules:
const char* RULE_STRINGS[] = { "JvN29", "Nobili32", "Hutton32" };
const int N_STATES[] = { 29, 32, 32 };

int jvnalgo::NumCellStates() {
   return N_STATES[current_rule];
}

const char* jvnalgo::setrule(const char *s)
{
   const char* colonptr = strchr(s,':');
   string rule_name(s);
   if(colonptr)
      rule_name.assign(s,colonptr);
   
   // check the requested string against the named rules, and deprecated versions
   if (stricmp(rule_name.c_str(), RULE_STRINGS[JvN29]) == 0 || 
        stricmp(rule_name.c_str(), "JvN-29") == 0)
      current_rule = JvN29;
   else if (stricmp(rule_name.c_str(), RULE_STRINGS[Nobili32]) == 0 || 
        stricmp(rule_name.c_str(), "JvN-32") == 0)
      current_rule = Nobili32;
   else if (stricmp(rule_name.c_str(), RULE_STRINGS[Hutton32]) == 0 || 
        stricmp(rule_name.c_str(), "modJvN-32") == 0)
      current_rule = Hutton32;
   else {
      return "This algorithm only supports these rules:\nJvN29, Nobili32, Hutton32.";
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

   maxCellStates = N_STATES[current_rule];
   ghashbase::setrule(RULE_STRINGS[current_rule]);
   return NULL;
}

const char* jvnalgo::getrule() {
   // return canonical rule string
   static char canonrule[MAXRULESIZE];
   sprintf(canonrule, "%s", RULE_STRINGS[current_rule]);
   if (gridwd > 0 || gridht > 0) {
      // setgridsize() was successfully called above, so append suffix
      int len = (int)strlen(canonrule);
      const char* bounds = canonicalsuffix();
      int i = 0;
      while (bounds[i]) canonrule[len++] = bounds[i++];
      canonrule[len] = 0;
   }
   return canonrule;
}

const char* jvnalgo::DefaultRule() {
   return RULE_STRINGS[JvN29];
}

const int NORTH = 1 ;
const int SOUTH = 3 ;
const int EAST = 0 ;
const int WEST = 2 ;
const int FLIPDIR = 2 ;
const int DIRMASK = 3 ;
const int CONF = 0x10 ;
const int OTRANS = 0x20 ;
const int STRANS = 0x40 ;
const int TEXC = 0x80 ;
const int CDEXC = 0x80 ;
const int CROSSEXC = 6 ;
const int CEXC = 1 ;
const int BIT_ONEXC = 1 ;
const int BIT_OEXC_EW = 2 ;
const int BIT_OEXC_NS = 4 ;
const int BIT_OEXC = BIT_OEXC_NS | BIT_OEXC_EW ;
const int BIT_SEXC = 8 ;
const int BIT_CEXC = 16 ;
const int BIT_NS_IN = 32 ;
const int BIT_EW_IN = 64 ;
const int BIT_NS_OUT = 128 ;
const int BIT_EW_OUT = 256 ;
const int BIT_CROSS = (BIT_NS_IN | BIT_EW_IN | BIT_NS_OUT | BIT_EW_OUT) ;
const int BIT_ANY_OUT = (BIT_NS_OUT | BIT_EW_OUT) ;
const int BIT_OEXC_OTHER = 512 ;
const int BIT_SEXC_OTHER = 1024 ;
static state compress[256] ;

/**
 *   These are the legal *internal* states.
 */
static state uncompress[] = {
   0,                      /* dead */
   1, 2, 3, 4, 5, 6, 7, 8, /* construction states */
   32, 33, 34, 35,         /* ordinary */
   160, 161, 162, 163,     /* ordinary active */
   64, 65, 66, 67,         /* special */
   192, 193, 194, 195,     /* special active */
   16, 144,                /* confluent states */
   17, 145,                /* more confluent states */
   146, 148, 150           /* crossing confluent states */
} ;

/**
 *   The behavior of the confluent states under the extended
 *   rules was verified empirically by the wjvn executable,
 *   because I could not interpret the paper sufficiently to
 *   cover some cases I thought were ambiguous, or where the
 *   simulator seemed to contradict the transition rules in the
 *   paper.   -tgr
 */
static int bits(state mcode, state code, state dir) {
   if ((code & (TEXC | OTRANS | STRANS | CONF | CEXC)) == 0)
      return 0 ;
   if (code & CONF) {
      if ((mcode & (OTRANS | STRANS)) && ((mcode & DIRMASK) ^ FLIPDIR) == dir)
         return 0 ;
      if ((code & 2) && !(dir & 1))
         return BIT_CEXC ;
      if ((code & 4) && (dir & 1))
         return BIT_CEXC ;
      if (code & 1)
         return BIT_CEXC ;
      return 0 ;
   }
   if ((code & (OTRANS | STRANS)) == 0)
      return 0 ;
   int r = 0 ;
   if ((code & DIRMASK) == dir) {
      if (code & OTRANS) {
         if (dir & 1) {
           r |= BIT_NS_IN ;
           if (code & TEXC)
             r |= BIT_OEXC_NS ;
           else
             r |= BIT_ONEXC ;
         } else {
           r |= BIT_EW_IN ;
           if (code & TEXC)
             r |= BIT_OEXC_EW ;
           else
             r |= BIT_ONEXC ;
         }
      } else if ((code & (STRANS | TEXC)) == (STRANS | TEXC))
         r |= BIT_SEXC ;
      if ((mcode & (OTRANS | STRANS)) && (dir ^ (mcode & DIRMASK)) == 2) {
         // don't permit these bits to propogate; head to head
      } else {
        if (r & BIT_OEXC)
           r |= BIT_OEXC_OTHER ;
        if (r & BIT_SEXC)
           r |= BIT_SEXC_OTHER ;
      }
   } else {
      if (dir & 1)
         r |= BIT_NS_OUT ;
      else
         r |= BIT_EW_OUT ;
   }
   return r ;
}

static state cres[] = {0x22, 0x23, 0x40, 0x41, 0x42, 0x43, 0x10, 0x20, 0x21} ;

jvnalgo::jvnalgo() {
  for (int i=0; i<256; i++)
    compress[i] = 255 ;
  for (unsigned int i=0; i<sizeof(uncompress)/sizeof(uncompress[0]); i++)
     compress[uncompress[i]] = (state)i ;
  current_rule = JvN29 ;
  maxCellStates = N_STATES[current_rule] ;
}

jvnalgo::~jvnalgo() {
}

state slowcalc_Hutton32(state c,state n,state s,state e,state w);

// --- the update function ---
state jvnalgo::slowcalc(state, state n, state, state w, state c, state e,
                        state, state s, state) {
   if(current_rule == JvN29 || current_rule == Nobili32)
   {
	   c = uncompress[c] ;
	   int mbits = bits(c, uncompress[n], SOUTH) |
	     bits(c, uncompress[w], EAST) |
	     bits(c, uncompress[e], WEST) |
	     bits(c, uncompress[s], NORTH) ;
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
	      else if (current_rule == Nobili32 && (mbits & BIT_CROSS) == BIT_CROSS) {
		 if (mbits & BIT_OEXC)
		    c = (state)((mbits & BIT_OEXC) + CONF + 0x80) ;
		 else
		    c = CONF ;
	      } else {
		 if (c & CROSSEXC) {// was a cross, is no more
		    c = (c & ~(CROSSEXC | CDEXC)) ;
		 }
		 if ((mbits & BIT_OEXC) && !(mbits & BIT_ONEXC))
		    c = ((c & CDEXC) >> 7) + (CDEXC | CONF) ;
		 else if ((mbits & BIT_ANY_OUT) || current_rule == JvN29)
		    c = ((c & CDEXC) >> 7) + CONF ;
		 else
		    /* no change */ ;
	      }
	   } else {
	      if (((c & OTRANS) && (mbits & BIT_SEXC)) ||
		  ((c & STRANS) && (mbits & BIT_OEXC)))
		 c = 0 ;
	      else if (mbits & (BIT_SEXC_OTHER | BIT_OEXC_OTHER | BIT_CEXC))
		 c |= 128 ;
	      else
		 c &= 127 ;
	   }
	   return compress[c] ;
   }
   else // Hutton32
   	return slowcalc_Hutton32(c,n,s,e,w);
}

// XPM data for the 31 7x7 icons used in JvN algo
static const char* jvn7x7[] = {
// width height ncolors chars_per_pixel
"7 217 4 1",
// colors
". c #000000",    // black will be transparent
"D c #404040",
"E c #E0E0E0",
"W c #FFFFFF",    // white
// pixels
".DEWED.",
"DWWWWWD",
"EWWWWWE",
"WWWWWWW",
"EWWWWWE",
"DWWWWWD",
".DEWED.",

"..WWW..",
".WWWWW.",
"WWWWWWW",
".......",
"WWW.WWW",
".WW.WW.",
"..W.W..",

"..W.W..",
".WW.WW.",
"WWW.WWW",
".......",
"WWWWWWW",
".WWWWW.",
"..WWW..",

"..W.W..",
".WW.WW.",
"WWW.WWW",
".......",
"WWW.WWW",
"WWW.WWW",
"WWW.WWW",

"..W.WWW",
".WW.WWW",
"WWW.WWW",
".......",
"WWW.WWW",
"WWW.WW.",
"WWW.W..",

"WWW.W..",
"WWW.WW.",
"WWW.WWW",
".......",
"WWW.WWW",
".WW.WWW",
"..W.WWW",

"WWW.WWW",
"WWW.WWW",
"WWW.WWW",
".......",
"WWW.WWW",
".WW.WW.",
"..W.W..",

"..W.W..",
".WW.WW.",
"WWW.WWW",
".......",
"WWW.WWW",
".WW.WW.",
"..W.W..",

".......",
"....W..",
"....WW.",
"WWWWWWW",
"....WW.",
"....W..",
".......",

"...W...",
"..WWW..",
".WWWWW.",
"...W...",
"...W...",
"...W...",
"...W...",

".......",
"..W....",
".WW....",
"WWWWWWW",
".WW....",
"..W....",
".......",

"...W...",
"...W...",
"...W...",
"...W...",
".WWWWW.",
"..WWW..",
"...W...",

".......",
"....W..",
"....WW.",
"WWWWWWW",
"....WW.",
"....W..",
".......",

"...W...",
"..WWW..",
".WWWWW.",
"...W...",
"...W...",
"...W...",
"...W...",

".......",
"..W....",
".WW....",
"WWWWWWW",
".WW....",
"..W....",
".......",

"...W...",
"...W...",
"...W...",
"...W...",
".WWWWW.",
"..WWW..",
"...W...",

".......",
"....W..",
"....WW.",
"WWWWWWW",
"....WW.",
"....W..",
".......",

"...W...",
"..WWW..",
".WWWWW.",
"...W...",
"...W...",
"...W...",
"...W...",

".......",
"..W....",
".WW....",
"WWWWWWW",
".WW....",
"..W....",
".......",

"...W...",
"...W...",
"...W...",
"...W...",
".WWWWW.",
"..WWW..",
"...W...",

".......",
"....W..",
"....WW.",
"WWWWWWW",
"....WW.",
"....W..",
".......",

"...W...",
"..WWW..",
".WWWWW.",
"...W...",
"...W...",
"...W...",
"...W...",

".......",
"..W....",
".WW....",
"WWWWWWW",
".WW....",
"..W....",
".......",

"...W...",
"...W...",
"...W...",
"...W...",
".WWWWW.",
"..WWW..",
"...W...",

"...W...",
"..WWW..",
".WW.WW.",
"WW...WW",
".WW.WW.",
"..WWW..",
"...W...",

"...W...",
"..WWW..",
".WW.WW.",
"WW...WW",
".WW.WW.",
"..WWW..",
"...W...",

"...W...",
"..WWW..",
".WWWWW.",
"WWW.WWW",
".WWWWW.",
"..WWW..",
"...W...",

"...W...",
"..WWW..",
".WWWWW.",
"WWWWWWW",
".WWWWW.",
"..WWW..",
"...W...",

"...W...",
"..W.W..",
".WW.WW.",
"WWW.WWW",
".WW.WW.",
"..W.W..",
"...W...",

"...W...",
"..WWW..",
".WWWWW.",
"W.....W",
".WWWWW.",
"..WWW..",
"...W...",

"...W...",
"..WWW..",
".W.W.W.",
"WWW.WWW",
".W.W.W.",
"..WWW..",
"...W..."};

// XPM data for the 31 15x15 icons used in JvN algo
static const char* jvn15x15[] = {
// width height ncolors chars_per_pixel
"15 465 5 1",
// colors
". c #000000",    // black will be transparent
"D c #404040",
"C c #808080",
"B c #C0C0C0",
"W c #FFFFFF",    // white
// pixels
"...............",
"....DBWWWBD....",
"...BWWWWWWWB...",
"..BWWWWWWWWWB..",
".DWWWWWWWWWWWD.",
".BWWWWWWWWWWWB.",
".WWWWWWWWWWWWW.",
".WWWWWWWWWWWWW.",
".WWWWWWWWWWWWW.",
".BWWWWWWWWWWWB.",
".DWWWWWWWWWWWD.",
"..BWWWWWWWWWB..",
"...BWWWWWWWB...",
"....DBWWWBD....",
"...............",

"...............",
"......WWW......",
".....WWWWW.....",
"....WWWWWWW....",
"...WWWWWWWWW...",
"..WWWWWWWWWWW..",
".WWWWWWWWWWWWW.",
"...............",
".WWWWWW.WWWWWW.",
"..WWWWW.WWWWW..",
"...WWWW.WWWW...",
"....WWW.WWW....",
".....WW.WW.....",
"......W.W......",
"...............",

"...............",
"......W.W......",
".....WW.WW.....",
"....WWW.WWW....",
"...WWWW.WWWW...",
"..WWWWW.WWWWW..",
".WWWWWW.WWWWWW.",
"...............",
".WWWWWWWWWWWWW.",
"..WWWWWWWWWWW..",
"...WWWWWWWWW...",
"....WWWWWWW....",
".....WWWWW.....",
"......WWW......",
"...............",

"...............",
"......W.W......",
".....WW.WW.....",
"....WWW.WWW....",
"...WWWW.WWWW...",
"..WWWWW.WWWWW..",
".WWWWWW.WWWWWW.",
"...............",
".WWWWWW.WWWWWW.",
".WWWWWW.WWWWWW.",
".WWWWWW.WWWWWW.",
".WWWWWW.WWWWWW.",
".WWWWWW.WWWWWW.",
".WWWWWW.WWWWWW.",
"...............",

"...............",
"......W.WWWWWW.",
".....WW.WWWWWW.",
"....WWW.WWWWWW.",
"...WWWW.WWWWWW.",
"..WWWWW.WWWWWW.",
".WWWWWW.WWWWWW.",
"...............",
".WWWWWW.WWWWWW.",
".WWWWWW.WWWWW..",
".WWWWWW.WWWW...",
".WWWWWW.WWW....",
".WWWWWW.WW.....",
".WWWWWW.W......",
"...............",

"...............",
".WWWWWW.W......",
".WWWWWW.WW.....",
".WWWWWW.WWW....",
".WWWWWW.WWWW...",
".WWWWWW.WWWWW..",
".WWWWWW.WWWWWW.",
"...............",
".WWWWWW.WWWWWW.",
"..WWWWW.WWWWWW.",
"...WWWW.WWWWWW.",
"....WWW.WWWWWW.",
".....WW.WWWWWW.",
"......W.WWWWWW.",
"...............",

"...............",
".WWWWWW.WWWWWW.",
".WWWWWW.WWWWWW.",
".WWWWWW.WWWWWW.",
".WWWWWW.WWWWWW.",
".WWWWWW.WWWWWW.",
".WWWWWW.WWWWWW.",
"...............",
".WWWWWW.WWWWWW.",
"..WWWWW.WWWWW..",
"...WWWW.WWWW...",
"....WWW.WWW....",
".....WW.WW.....",
"......W.W......",
"...............",

"...............",
"......W.W......",
".....WW.WW.....",
"....WWW.WWW....",
"...WWWW.WWWW...",
"..WWWWW.WWWWW..",
".WWWWWW.WWWWWW.",
"...............",
".WWWWWW.WWWWWW.",
"..WWWWW.WWWWW..",
"...WWWW.WWWW...",
"....WWW.WWW....",
".....WW.WW.....",
"......W.W......",
"...............",

"...............",
".......W.......",
".......WW......",
".......WWW.....",
".......WWWW....",
".......WWWWW...",
".WWWWWWWWWWWW..",
".WWWWWWWWWWWWW.",
".WWWWWWWWWWWW..",
".......WWWWW...",
".......WWWW....",
".......WWW.....",
".......WW......",
".......W.......",
"...............",

"...............",
".......W.......",
"......WWW......",
".....WWWWW.....",
"....WWWWWWW....",
"...WWWWWWWWW...",
"..WWWWWWWWWWW..",
".WWWWWWWWWWWWW.",
"......WWW......",
"......WWW......",
"......WWW......",
"......WWW......",
"......WWW......",
"......WWW......",

"...............",
"...............",
".......W.......",
"......WW.......",
".....WWW.......",
"....WWWW.......",
"...WWWWW.......",
"..WWWWWWWWWWWW.",
".WWWWWWWWWWWWW.",
"..WWWWWWWWWWWW.",
"...WWWWW.......",
"....WWWW.......",
".....WWW.......",
"......WW.......",
".......W.......",
"...............",

"...............",
"......WWW......",
"......WWW......",
"......WWW......",
"......WWW......",
"......WWW......",
"......WWW......",
".WWWWWWWWWWWWW.",
"..WWWWWWWWWWW..",
"...WWWWWWWWW...",
"....WWWWWWW....",
".....WWWWW.....",
"......WWW......",
".......W.......",
"...............",

"...............",
".......W.......",
".......WW......",
".......WWW.....",
".......WWWW....",
".......WWWWW...",
".WWWWWWWWWWWW..",
".WWWWWWWWWWWWW.",
".WWWWWWWWWWWW..",
".......WWWWW...",
".......WWWW....",
".......WWW.....",
".......WW......",
".......W.......",
"...............",

"...............",
".......W.......",
"......WWW......",
".....WWWWW.....",
"....WWWWWWW....",
"...WWWWWWWWW...",
"..WWWWWWWWWWW..",
".WWWWWWWWWWWWW.",
"......WWW......",
"......WWW......",
"......WWW......",
"......WWW......",
"......WWW......",
"......WWW......",
"...............",

"...............",
".......W.......",
"......WW.......",
".....WWW.......",
"....WWWW.......",
"...WWWWW.......",
"..WWWWWWWWWWWW.",
".WWWWWWWWWWWWW.",
"..WWWWWWWWWWWW.",
"...WWWWW.......",
"....WWWW.......",
".....WWW.......",
"......WW.......",
".......W.......",
"...............",

"...............",
"......WWW......",
"......WWW......",
"......WWW......",
"......WWW......",
"......WWW......",
"......WWW......",
".WWWWWWWWWWWWW.",
"..WWWWWWWWWWW..",
"...WWWWWWWWW...",
"....WWWWWWW....",
".....WWWWW.....",
"......WWW......",
".......W.......",
"...............",

"...............",
".......W.......",
".......WW......",
".......WWW.....",
".......WWWW....",
".......WWWWW...",
".WWWWWWWWWWWW..",
".WWWWWWWWWWWWW.",
".WWWWWWWWWWWW..",
".......WWWWW...",
".......WWWW....",
".......WWW.....",
".......WW......",
".......W.......",
"...............",

"...............",
".......W.......",
"......WWW......",
".....WWWWW.....",
"....WWWWWWW....",
"...WWWWWWWWW...",
"..WWWWWWWWWWW..",
".WWWWWWWWWWWWW.",
"......WWW......",
"......WWW......",
"......WWW......",
"......WWW......",
"......WWW......",
"......WWW......",
"...............",

"...............",
".......W.......",
"......WW.......",
".....WWW.......",
"....WWWW.......",
"...WWWWW.......",
"..WWWWWWWWWWWW.",
".WWWWWWWWWWWWW.",
"..WWWWWWWWWWWW.",
"...WWWWW.......",
"....WWWW.......",
".....WWW.......",
"......WW.......",
".......W.......",
"...............",

"...............",
"......WWW......",
"......WWW......",
"......WWW......",
"......WWW......",
"......WWW......",
"......WWW......",
".WWWWWWWWWWWWW.",
"..WWWWWWWWWWW..",
"...WWWWWWWWW...",
"....WWWWWWW....",
".....WWWWW.....",
"......WWW......",
".......W.......",
"...............",

"...............",
".......W.......",
".......WW......",
".......WWW.....",
".......WWWW....",
".......WWWWW...",
".WWWWWWWWWWWW..",
".WWWWWWWWWWWWW.",
".WWWWWWWWWWWW..",
".......WWWWW...",
".......WWWW....",
".......WWW.....",
".......WW......",
".......W.......",
"...............",

"...............",
".......W.......",
"......WWW......",
".....WWWWW.....",
"....WWWWWWW....",
"...WWWWWWWWW...",
"..WWWWWWWWWWW..",
".WWWWWWWWWWWWW.",
"......WWW......",
"......WWW......",
"......WWW......",
"......WWW......",
"......WWW......",
"......WWW......",
"...............",

"...............",
".......W.......",
"......WW.......",
".....WWW.......",
"....WWWW.......",
"...WWWWW.......",
"..WWWWWWWWWWWW.",
".WWWWWWWWWWWWW.",
"..WWWWWWWWWWWW.",
"...WWWWW.......",
"....WWWW.......",
".....WWW.......",
"......WW.......",
".......W.......",
"...............",

"...............",
"......WWW......",
"......WWW......",
"......WWW......",
"......WWW......",
"......WWW......",
"......WWW......",
".WWWWWWWWWWWWW.",
"..WWWWWWWWWWW..",
"...WWWWWWWWW...",
"....WWWWWWW....",
".....WWWWW.....",
"......WWW......",
".......W.......",
"...............",

"...............",
".......W.......",
"......WWW......",
".....WWWWW.....",
"....WWWWWWW....",
"...WWWW.WWWW...",
"..WWWW...WWWW..",
".WWWW.....WWWW.",
"..WWWW...WWWW..",
"...WWWW.WWWW...",
"....WWWWWWW....",
".....WWWWW.....",
"......WWW......",
".......W.......",
"...............",

"...............",
".......W.......",
"......WWW......",
".....WWWWW.....",
"....WWWWWWW....",
"...WWWW.WWWW...",
"..WWWW...WWWW..",
".WWWW.....WWWW.",
"..WWWW...WWWW..",
"...WWWW.WWWW...",
"....WWWWWWW....",
".....WWWWW.....",
"......WWW......",
".......W.......",
"...............",

"...............",
".......W.......",
"......WWW......",
".....WWWWW.....",
"....WWWWWWW....",
"...WWWWWWWWW...",
"..WWWW...WWWW..",
".WWWWW...WWWWW.",
"..WWWW...WWWW..",
"...WWWWWWWWW...",
"....WWWWWWW....",
".....WWWWW.....",
"......WWW......",
".......W.......",
"...............",

"...............",
".......W.......",
"......WWW......",
".....WWWWW.....",
"....WWWWWWW....",
"...WWWWWWWWW...",
"..WWWWWWWWWWW..",
".WWWWWWWWWWWWW.",
"..WWWWWWWWWWW..",
"...WWWWWWWWW...",
"....WWWWWWW....",
".....WWWWW.....",
"......WWW......",
".......W.......",
"...............",

"...............",
".......W.......",
"......WWW......",
".....W...W.....",
"....WW...WW....",
"...WWW...WWW...",
"..WWWW...WWWW..",
".WWWWW...WWWWW.",
"..WWWW...WWWW..",
"...WWW...WWW...",
"....WW...WW....",
".....W...W.....",
"......WWW......",
".......W.......",
"...............",

"...............",
".......W.......",
"......WWW......",
".....WWWWW.....",
"....WWWWWWW....",
"...WWWWWWWWW...",
"..W.........W..",
".WW.........WW.",
"..W.........W..",
"...WWWWWWWWW...",
"....WWWWWWW....",
".....WWWWW.....",
"......WWW......",
".......W.......",
"...............",

"...............",
".......W.......",
"......WWW......",
".....WWWWW.....",
"....W.WWW.W....",
"...W...W...W...",
"..WWW.....WWW..",
".WWWWW...WWWWW.",
"..WWW.....WWW..",
"...W...W...W...",
"....W.WWW.W....",
".....WWWWW.....",
"......WWW......",
".......W.......",
"..............."};

// XPM data for the 31 31x31 icons used in JvN algo
static const char* jvn31x31[] = {
// width height ncolors chars_per_pixel
"31 961 5 1",
// colors
". c #000000",    // black will be transparent
"D c #404040",
"C c #808080",
"B c #C0C0C0",
"W c #FFFFFF",    // white
// pixels
"...............................",
"...............................",
"..........DCBWWWWWBCD..........",
".........CWWWWWWWWWWWC.........",
".......DWWWWWWWWWWWWWWWD.......",
"......BWWWWWWWWWWWWWWWWWB......",
".....BWWWWWWWWWWWWWWWWWWWB.....",
"....DWWWWWWWWWWWWWWWWWWWWWD....",
"....WWWWWWWWWWWWWWWWWWWWWWW....",
"...CWWWWWWWWWWWWWWWWWWWWWWWC...",
"..DWWWWWWWWWWWWWWWWWWWWWWWWWD..",
"..CWWWWWWWWWWWWWWWWWWWWWWWWWC..",
"..BWWWWWWWWWWWWWWWWWWWWWWWWWB..",
"..WWWWWWWWWWWWWWWWWWWWWWWWWWW..",
"..WWWWWWWWWWWWWWWWWWWWWWWWWWW..",
"..WWWWWWWWWWWWWWWWWWWWWWWWWWW..",
"..WWWWWWWWWWWWWWWWWWWWWWWWWWW..",
"..WWWWWWWWWWWWWWWWWWWWWWWWWWW..",
"..BWWWWWWWWWWWWWWWWWWWWWWWWWB..",
"..CWWWWWWWWWWWWWWWWWWWWWWWWWC..",
"..DWWWWWWWWWWWWWWWWWWWWWWWWWD..",
"...CWWWWWWWWWWWWWWWWWWWWWWWC...",
"....WWWWWWWWWWWWWWWWWWWWWWW....",
"....DWWWWWWWWWWWWWWWWWWWWWD....",
".....BWWWWWWWWWWWWWWWWWWWB.....",
"......BWWWWWWWWWWWWWWWWWB......",
".......DWWWWWWWWWWWWWWWD.......",
".........CWWWWWWWWWWWC.........",
"..........DCBWWWWWBCD..........",
"...............................",
"...............................",

"...............................",
"...............W...............",
"..............WWW..............",
".............WWWWW.............",
"............WWWWWWW............",
"...........WWWWWWWWW...........",
"..........WWWWWWWWWWW..........",
".........WWWWWWWWWWWWW.........",
"........WWWWWWWWWWWWWWW........",
".......WWWWWWWWWWWWWWWWW.......",
"......WWWWWWWWWWWWWWWWWWW......",
".....WWWWWWWWWWWWWWWWWWWWW.....",
"....WWWWWWWWWWWWWWWWWWWWWWW....",
"...WWWWWWWWWWWWWWWWWWWWWWWWW...",
"..WWWWWWWWWWWWWWWWWWWWWWWWWWW..",
"...............................",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"...WWWWWWWWWWWW.WWWWWWWWWWWW...",
"....WWWWWWWWWWW.WWWWWWWWWWW....",
".....WWWWWWWWWW.WWWWWWWWWW.....",
"......WWWWWWWWW.WWWWWWWWW......",
".......WWWWWWWW.WWWWWWWW.......",
"........WWWWWWW.WWWWWWW........",
".........WWWWWW.WWWWWW.........",
"..........WWWWW.WWWWW..........",
"...........WWWW.WWWW...........",
"............WWW.WWW............",
".............WW.WW.............",
"..............W.W..............",
"...............................",
"...............................",

"...............................",
"...............................",
"..............W.W..............",
".............WW.WW.............",
"............WWW.WWW............",
"...........WWWW.WWWW...........",
"..........WWWWW.WWWWW..........",
".........WWWWWW.WWWWWW.........",
"........WWWWWWW.WWWWWWW........",
".......WWWWWWWW.WWWWWWWW.......",
"......WWWWWWWWW.WWWWWWWWW......",
".....WWWWWWWWWW.WWWWWWWWWW.....",
"....WWWWWWWWWWW.WWWWWWWWWWW....",
"...WWWWWWWWWWWW.WWWWWWWWWWWW...",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"...............................",
"..WWWWWWWWWWWWWWWWWWWWWWWWWWW..",
"...WWWWWWWWWWWWWWWWWWWWWWWWW...",
"....WWWWWWWWWWWWWWWWWWWWWWW....",
".....WWWWWWWWWWWWWWWWWWWWW.....",
"......WWWWWWWWWWWWWWWWWWW......",
".......WWWWWWWWWWWWWWWWW.......",
"........WWWWWWWWWWWWWWW........",
".........WWWWWWWWWWWWW.........",
"..........WWWWWWWWWWW..........",
"...........WWWWWWWWW...........",
"............WWWWWWW............",
".............WWWWW.............",
"..............WWW..............",
"...............W...............",
"...............................",

"...............................",
"...............................",
"..............W.W..............",
".............WW.WW.............",
"............WWW.WWW............",
"...........WWWW.WWWW...........",
"..........WWWWW.WWWWW..........",
".........WWWWWW.WWWWWW.........",
"........WWWWWWW.WWWWWWW........",
".......WWWWWWWW.WWWWWWWW.......",
"......WWWWWWWWW.WWWWWWWWW......",
".....WWWWWWWWWW.WWWWWWWWWW.....",
"....WWWWWWWWWWW.WWWWWWWWWWW....",
"...WWWWWWWWWWWW.WWWWWWWWWWWW...",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"...............................",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"...............................",
"...............................",

"...............................",
"...............................",
"..............W.WWWWWWWWWWWWW..",
".............WW.WWWWWWWWWWWWW..",
"............WWW.WWWWWWWWWWWWW..",
"...........WWWW.WWWWWWWWWWWWW..",
"..........WWWWW.WWWWWWWWWWWWW..",
".........WWWWWW.WWWWWWWWWWWWW..",
"........WWWWWWW.WWWWWWWWWWWWW..",
".......WWWWWWWW.WWWWWWWWWWWWW..",
"......WWWWWWWWW.WWWWWWWWWWWWW..",
".....WWWWWWWWWW.WWWWWWWWWWWWW..",
"....WWWWWWWWWWW.WWWWWWWWWWWWW..",
"...WWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"...............................",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWW...",
"..WWWWWWWWWWWWW.WWWWWWWWWWW....",
"..WWWWWWWWWWWWW.WWWWWWWWWW.....",
"..WWWWWWWWWWWWW.WWWWWWWWW......",
"..WWWWWWWWWWWWW.WWWWWWWW.......",
"..WWWWWWWWWWWWW.WWWWWWW........",
"..WWWWWWWWWWWWW.WWWWWW.........",
"..WWWWWWWWWWWWW.WWWWW..........",
"..WWWWWWWWWWWWW.WWWW...........",
"..WWWWWWWWWWWWW.WWW............",
"..WWWWWWWWWWWWW.WW.............",
"..WWWWWWWWWWWWW.W..............",
"...............................",
"...............................",

"...............................",
"...............................",
"..WWWWWWWWWWWWW.W..............",
"..WWWWWWWWWWWWW.WW.............",
"..WWWWWWWWWWWWW.WWW............",
"..WWWWWWWWWWWWW.WWWW...........",
"..WWWWWWWWWWWWW.WWWWW..........",
"..WWWWWWWWWWWWW.WWWWWW.........",
"..WWWWWWWWWWWWW.WWWWWWW........",
"..WWWWWWWWWWWWW.WWWWWWWW.......",
"..WWWWWWWWWWWWW.WWWWWWWWW......",
"..WWWWWWWWWWWWW.WWWWWWWWWW.....",
"..WWWWWWWWWWWWW.WWWWWWWWWWW....",
"..WWWWWWWWWWWWW.WWWWWWWWWWWW...",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"...............................",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"...WWWWWWWWWWWW.WWWWWWWWWWWWW..",
"....WWWWWWWWWWW.WWWWWWWWWWWWW..",
".....WWWWWWWWWW.WWWWWWWWWWWWW..",
"......WWWWWWWWW.WWWWWWWWWWWWW..",
".......WWWWWWWW.WWWWWWWWWWWWW..",
"........WWWWWWW.WWWWWWWWWWWWW..",
".........WWWWWW.WWWWWWWWWWWWW..",
"..........WWWWW.WWWWWWWWWWWWW..",
"...........WWWW.WWWWWWWWWWWWW..",
"............WWW.WWWWWWWWWWWWW..",
".............WW.WWWWWWWWWWWWW..",
"..............W.WWWWWWWWWWWWW..",
"...............................",
"...............................",

"...............................",
"...............................",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"...............................",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"...WWWWWWWWWWWW.WWWWWWWWWWWW...",
"....WWWWWWWWWWW.WWWWWWWWWWW....",
".....WWWWWWWWWW.WWWWWWWWWW.....",
"......WWWWWWWWW.WWWWWWWWW......",
".......WWWWWWWW.WWWWWWWW.......",
"........WWWWWWW.WWWWWWW........",
".........WWWWWW.WWWWWW.........",
"..........WWWWW.WWWWW..........",
"...........WWWW.WWWW...........",
"............WWW.WWW............",
".............WW.WW.............",
"..............W.W..............",
"...............................",
"...............................",

"...............................",
"...............................",
"..............W.W..............",
".............WW.WW.............",
"............WWW.WWW............",
"...........WWWW.WWWW...........",
"..........WWWWW.WWWWW..........",
".........WWWWWW.WWWWWW.........",
"........WWWWWWW.WWWWWWW........",
".......WWWWWWWW.WWWWWWWW.......",
"......WWWWWWWWW.WWWWWWWWW......",
".....WWWWWWWWWW.WWWWWWWWWW.....",
"....WWWWWWWWWWW.WWWWWWWWWWW....",
"...WWWWWWWWWWWW.WWWWWWWWWWWW...",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"...............................",
"..WWWWWWWWWWWWW.WWWWWWWWWWWWW..",
"...WWWWWWWWWWWW.WWWWWWWWWWWW...",
"....WWWWWWWWWWW.WWWWWWWWWWW....",
".....WWWWWWWWWW.WWWWWWWWWW.....",
"......WWWWWWWWW.WWWWWWWWW......",
".......WWWWWWWW.WWWWWWWW.......",
"........WWWWWWW.WWWWWWW........",
".........WWWWWW.WWWWWW.........",
"..........WWWWW.WWWWW..........",
"...........WWWW.WWWW...........",
"............WWW.WWW............",
".............WW.WW.............",
"..............W.W..............",
"...............................",
"...............................",

"...............................",
"...............................",
"...............W...............",
"...............WW..............",
"...............WWW.............",
"...............WWWW............",
"...............WWWWW...........",
"...............WWWWWW..........",
"...............WWWWWWW.........",
"...............WWWWWWWW........",
"...............WWWWWWWWW.......",
"...............WWWWWWWWWW......",
"...............WWWWWWWWWWW.....",
"..WWWWWWWWWWWWWWWWWWWWWWWWW....",
"..WWWWWWWWWWWWWWWWWWWWWWWWWW...",
"..WWWWWWWWWWWWWWWWWWWWWWWWWWW..",
"..WWWWWWWWWWWWWWWWWWWWWWWWWW...",
"..WWWWWWWWWWWWWWWWWWWWWWWWW....",
"...............WWWWWWWWWWW.....",
"...............WWWWWWWWWW......",
"...............WWWWWWWWW.......",
"...............WWWWWWWW........",
"...............WWWWWWW.........",
"...............WWWWWW..........",
"...............WWWWW...........",
"...............WWWW............",
"...............WWW.............",
"...............WW..............",
"...............W...............",
"...............................",
"...............................",

"...............................",
"...............................",
"...............W...............",
"..............WWW..............",
".............WWWWW.............",
"............WWWWWWW............",
"...........WWWWWWWWW...........",
"..........WWWWWWWWWWW..........",
".........WWWWWWWWWWWWW.........",
"........WWWWWWWWWWWWWWW........",
".......WWWWWWWWWWWWWWWWW.......",
"......WWWWWWWWWWWWWWWWWWW......",
".....WWWWWWWWWWWWWWWWWWWWW.....",
"....WWWWWWWWWWWWWWWWWWWWWWW....",
"...WWWWWWWWWWWWWWWWWWWWWWWWW...",
"..WWWWWWWWWWWWWWWWWWWWWWWWWWW..",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
"...............................",
"...............................",

"...............................",
"...............................",
"...............W...............",
"..............WW...............",
".............WWW...............",
"............WWWW...............",
"...........WWWWW...............",
"..........WWWWWW...............",
".........WWWWWWW...............",
"........WWWWWWWW...............",
".......WWWWWWWWW...............",
"......WWWWWWWWWW...............",
".....WWWWWWWWWWW...............",
"....WWWWWWWWWWWWWWWWWWWWWWWWW..",
"...WWWWWWWWWWWWWWWWWWWWWWWWWW..",
"..WWWWWWWWWWWWWWWWWWWWWWWWWWW..",
"...WWWWWWWWWWWWWWWWWWWWWWWWWW..",
"....WWWWWWWWWWWWWWWWWWWWWWWWW..",
".....WWWWWWWWWWW...............",
"......WWWWWWWWWW...............",
".......WWWWWWWWW...............",
"........WWWWWWWW...............",
".........WWWWWWW...............",
"..........WWWWWW...............",
"...........WWWWW...............",
"............WWWW...............",
".............WWW...............",
"..............WW...............",
"...............W...............",
"...............................",
"...............................",

"...............................",
"...............................",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
"..WWWWWWWWWWWWWWWWWWWWWWWWWWW..",
"...WWWWWWWWWWWWWWWWWWWWWWWWW...",
"....WWWWWWWWWWWWWWWWWWWWWWW....",
".....WWWWWWWWWWWWWWWWWWWWW.....",
"......WWWWWWWWWWWWWWWWWWW......",
".......WWWWWWWWWWWWWWWWW.......",
"........WWWWWWWWWWWWWWW........",
".........WWWWWWWWWWWWW.........",
"..........WWWWWWWWWWW..........",
"...........WWWWWWWWW...........",
"............WWWWWWW............",
".............WWWWW.............",
"..............WWW..............",
"...............W...............",
"...............................",
"...............................",

"...............................",
"...............................",
"...............W...............",
"...............WW..............",
"...............WWW.............",
"...............WWWW............",
"...............WWWWW...........",
"...............WWWWWW..........",
"...............WWWWWWW.........",
"...............WWWWWWWW........",
"...............WWWWWWWWW.......",
"...............WWWWWWWWWW......",
"...............WWWWWWWWWWW.....",
"..WWWWWWWWWWWWWWWWWWWWWWWWW....",
"..WWWWWWWWWWWWWWWWWWWWWWWWWW...",
"..WWWWWWWWWWWWWWWWWWWWWWWWWWW..",
"..WWWWWWWWWWWWWWWWWWWWWWWWWW...",
"..WWWWWWWWWWWWWWWWWWWWWWWWW....",
"...............WWWWWWWWWWW.....",
"...............WWWWWWWWWW......",
"...............WWWWWWWWW.......",
"...............WWWWWWWW........",
"...............WWWWWWW.........",
"...............WWWWWW..........",
"...............WWWWW...........",
"...............WWWW............",
"...............WWW.............",
"...............WW..............",
"...............W...............",
"...............................",
"...............................",

"...............................",
"...............................",
"...............W...............",
"..............WWW..............",
".............WWWWW.............",
"............WWWWWWW............",
"...........WWWWWWWWW...........",
"..........WWWWWWWWWWW..........",
".........WWWWWWWWWWWWW.........",
"........WWWWWWWWWWWWWWW........",
".......WWWWWWWWWWWWWWWWW.......",
"......WWWWWWWWWWWWWWWWWWW......",
".....WWWWWWWWWWWWWWWWWWWWW.....",
"....WWWWWWWWWWWWWWWWWWWWWWW....",
"...WWWWWWWWWWWWWWWWWWWWWWWWW...",
"..WWWWWWWWWWWWWWWWWWWWWWWWWWW..",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
"...............................",
"...............................",

"...............................",
"...............................",
"...............W...............",
"..............WW...............",
".............WWW...............",
"............WWWW...............",
"...........WWWWW...............",
"..........WWWWWW...............",
".........WWWWWWW...............",
"........WWWWWWWW...............",
".......WWWWWWWWW...............",
"......WWWWWWWWWW...............",
".....WWWWWWWWWWW...............",
"....WWWWWWWWWWWWWWWWWWWWWWWWW..",
"...WWWWWWWWWWWWWWWWWWWWWWWWWW..",
"..WWWWWWWWWWWWWWWWWWWWWWWWWWW..",
"...WWWWWWWWWWWWWWWWWWWWWWWWWW..",
"....WWWWWWWWWWWWWWWWWWWWWWWWW..",
".....WWWWWWWWWWW...............",
"......WWWWWWWWWW...............",
".......WWWWWWWWW...............",
"........WWWWWWWW...............",
".........WWWWWWW...............",
"..........WWWWWW...............",
"...........WWWWW...............",
"............WWWW...............",
".............WWW...............",
"..............WW...............",
"...............W...............",
"...............................",
"...............................",

"...............................",
"...............................",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
"..WWWWWWWWWWWWWWWWWWWWWWWWWWW..",
"...WWWWWWWWWWWWWWWWWWWWWWWWW...",
"....WWWWWWWWWWWWWWWWWWWWWWW....",
".....WWWWWWWWWWWWWWWWWWWWW.....",
"......WWWWWWWWWWWWWWWWWWW......",
".......WWWWWWWWWWWWWWWWW.......",
"........WWWWWWWWWWWWWWW........",
".........WWWWWWWWWWWWW.........",
"..........WWWWWWWWWWW..........",
"...........WWWWWWWWW...........",
"............WWWWWWW............",
".............WWWWW.............",
"..............WWW..............",
"...............W...............",
"...............................",
"...............................",

"...............................",
"...............................",
"...............W...............",
"...............WW..............",
"...............WWW.............",
"...............WWWW............",
"...............WWWWW...........",
"...............WWWWWW..........",
"...............WWWWWWW.........",
"...............WWWWWWWW........",
"...............WWWWWWWWW.......",
"...............WWWWWWWWWW......",
"...............WWWWWWWWWWW.....",
"..WWWWWWWWWWWWWWWWWWWWWWWWW....",
"..WWWWWWWWWWWWWWWWWWWWWWWWWW...",
"..WWWWWWWWWWWWWWWWWWWWWWWWWWW..",
"..WWWWWWWWWWWWWWWWWWWWWWWWWW...",
"..WWWWWWWWWWWWWWWWWWWWWWWWW....",
"...............WWWWWWWWWWW.....",
"...............WWWWWWWWWW......",
"...............WWWWWWWWW.......",
"...............WWWWWWWW........",
"...............WWWWWWW.........",
"...............WWWWWW..........",
"...............WWWWW...........",
"...............WWWW............",
"...............WWW.............",
"...............WW..............",
"...............W...............",
"...............................",
"...............................",

"...............................",
"...............................",
"...............W...............",
"..............WWW..............",
".............WWWWW.............",
"............WWWWWWW............",
"...........WWWWWWWWW...........",
"..........WWWWWWWWWWW..........",
".........WWWWWWWWWWWWW.........",
"........WWWWWWWWWWWWWWW........",
".......WWWWWWWWWWWWWWWWW.......",
"......WWWWWWWWWWWWWWWWWWW......",
".....WWWWWWWWWWWWWWWWWWWWW.....",
"....WWWWWWWWWWWWWWWWWWWWWWW....",
"...WWWWWWWWWWWWWWWWWWWWWWWWW...",
"..WWWWWWWWWWWWWWWWWWWWWWWWWWW..",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
"...............................",
"...............................",

"...............................",
"...............................",
"...............W...............",
"..............WW...............",
".............WWW...............",
"............WWWW...............",
"...........WWWWW...............",
"..........WWWWWW...............",
".........WWWWWWW...............",
"........WWWWWWWW...............",
".......WWWWWWWWW...............",
"......WWWWWWWWWW...............",
".....WWWWWWWWWWW...............",
"....WWWWWWWWWWWWWWWWWWWWWWWWW..",
"...WWWWWWWWWWWWWWWWWWWWWWWWWW..",
"..WWWWWWWWWWWWWWWWWWWWWWWWWWW..",
"...WWWWWWWWWWWWWWWWWWWWWWWWWW..",
"....WWWWWWWWWWWWWWWWWWWWWWWWW..",
".....WWWWWWWWWWW...............",
"......WWWWWWWWWW...............",
".......WWWWWWWWW...............",
"........WWWWWWWW...............",
".........WWWWWWW...............",
"..........WWWWWW...............",
"...........WWWWW...............",
"............WWWW...............",
".............WWW...............",
"..............WW...............",
"...............W...............",
"...............................",
"...............................",

"...............................",
"...............................",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
"..WWWWWWWWWWWWWWWWWWWWWWWWWWW..",
"...WWWWWWWWWWWWWWWWWWWWWWWWW...",
"....WWWWWWWWWWWWWWWWWWWWWWW....",
".....WWWWWWWWWWWWWWWWWWWWW.....",
"......WWWWWWWWWWWWWWWWWWW......",
".......WWWWWWWWWWWWWWWWW.......",
"........WWWWWWWWWWWWWWW........",
".........WWWWWWWWWWWWW.........",
"..........WWWWWWWWWWW..........",
"...........WWWWWWWWW...........",
"............WWWWWWW............",
".............WWWWW.............",
"..............WWW..............",
"...............W...............",
"...............................",
"...............................",

"...............................",
"...............................",
"...............W...............",
"...............WW..............",
"...............WWW.............",
"...............WWWW............",
"...............WWWWW...........",
"...............WWWWWW..........",
"...............WWWWWWW.........",
"...............WWWWWWWW........",
"...............WWWWWWWWW.......",
"...............WWWWWWWWWW......",
"...............WWWWWWWWWWW.....",
"..WWWWWWWWWWWWWWWWWWWWWWWWW....",
"..WWWWWWWWWWWWWWWWWWWWWWWWWW...",
"..WWWWWWWWWWWWWWWWWWWWWWWWWWW..",
"..WWWWWWWWWWWWWWWWWWWWWWWWWW...",
"..WWWWWWWWWWWWWWWWWWWWWWWWW....",
"...............WWWWWWWWWWW.....",
"...............WWWWWWWWWW......",
"...............WWWWWWWWW.......",
"...............WWWWWWWW........",
"...............WWWWWWW.........",
"...............WWWWWW..........",
"...............WWWWW...........",
"...............WWWW............",
"...............WWW.............",
"...............WW..............",
"...............W...............",
"...............................",
"...............................",

"...............................",
"...............................",
"...............W...............",
"..............WWW..............",
".............WWWWW.............",
"............WWWWWWW............",
"...........WWWWWWWWW...........",
"..........WWWWWWWWWWW..........",
".........WWWWWWWWWWWWW.........",
"........WWWWWWWWWWWWWWW........",
".......WWWWWWWWWWWWWWWWW.......",
"......WWWWWWWWWWWWWWWWWWW......",
".....WWWWWWWWWWWWWWWWWWWWW.....",
"....WWWWWWWWWWWWWWWWWWWWWWW....",
"...WWWWWWWWWWWWWWWWWWWWWWWWW...",
"..WWWWWWWWWWWWWWWWWWWWWWWWWWW..",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
"...............................",
"...............................",

"...............................",
"...............................",
"...............W...............",
"..............WW...............",
".............WWW...............",
"............WWWW...............",
"...........WWWWW...............",
"..........WWWWWW...............",
".........WWWWWWW...............",
"........WWWWWWWW...............",
".......WWWWWWWWW...............",
"......WWWWWWWWWW...............",
".....WWWWWWWWWWW...............",
"....WWWWWWWWWWWWWWWWWWWWWWWWW..",
"...WWWWWWWWWWWWWWWWWWWWWWWWWW..",
"..WWWWWWWWWWWWWWWWWWWWWWWWWWW..",
"...WWWWWWWWWWWWWWWWWWWWWWWWWW..",
"....WWWWWWWWWWWWWWWWWWWWWWWWW..",
".....WWWWWWWWWWW...............",
"......WWWWWWWWWW...............",
".......WWWWWWWWW...............",
"........WWWWWWWW...............",
".........WWWWWWW...............",
"..........WWWWWW...............",
"...........WWWWW...............",
"............WWWW...............",
".............WWW...............",
"..............WW...............",
"...............W...............",
"...............................",
"...............................",

"...............................",
"...............................",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
".............WWWWW.............",
"..WWWWWWWWWWWWWWWWWWWWWWWWWWW..",
"...WWWWWWWWWWWWWWWWWWWWWWWWW...",
"....WWWWWWWWWWWWWWWWWWWWWWW....",
".....WWWWWWWWWWWWWWWWWWWWW.....",
"......WWWWWWWWWWWWWWWWWWW......",
".......WWWWWWWWWWWWWWWWW.......",
"........WWWWWWWWWWWWWWW........",
".........WWWWWWWWWWWWW.........",
"..........WWWWWWWWWWW..........",
"...........WWWWWWWWW...........",
"............WWWWWWW............",
".............WWWWW.............",
"..............WWW..............",
"...............W...............",
"...............................",
"...............................",

"...............................",
"...............W...............",
"..............WWW..............",
".............WWWWW.............",
"............WWWWWWW............",
"...........WWWWWWWWW...........",
"..........WWWWWWWWWWW..........",
".........WWWWWWWWWWWWW.........",
"........WWWWWWWWWWWWWWW........",
".......WWWWWWWW.WWWWWWWW.......",
"......WWWWWWWW...WWWWWWWW......",
".....WWWWWWWW.....WWWWWWWW.....",
"....WWWWWWWW.......WWWWWWWW....",
"...WWWWWWWW.........WWWWWWWW...",
"..WWWWWWWW...........WWWWWWWW..",
".WWWWWWWW.............WWWWWWWW.",
"..WWWWWWWW...........WWWWWWWW..",
"...WWWWWWWW.........WWWWWWWW...",
"....WWWWWWWW.......WWWWWWWW....",
".....WWWWWWWW.....WWWWWWWW.....",
"......WWWWWWWW...WWWWWWWW......",
".......WWWWWWWW.WWWWWWWW.......",
"........WWWWWWWWWWWWWWW........",
".........WWWWWWWWWWWWW.........",
"..........WWWWWWWWWWW..........",
"...........WWWWWWWWW...........",
"............WWWWWWW............",
".............WWWWW.............",
"..............WWW..............",
"...............W...............",
"...............................",

"...............................",
"...............W...............",
"..............WWW..............",
".............WWWWW.............",
"............WWWWWWW............",
"...........WWWWWWWWW...........",
"..........WWWWWWWWWWW..........",
".........WWWWWWWWWWWWW.........",
"........WWWWWWWWWWWWWWW........",
".......WWWWWWWW.WWWWWWWW.......",
"......WWWWWWWW...WWWWWWWW......",
".....WWWWWWWW.....WWWWWWWW.....",
"....WWWWWWWW.......WWWWWWWW....",
"...WWWWWWWW.........WWWWWWWW...",
"..WWWWWWWW...........WWWWWWWW..",
".WWWWWWWW.............WWWWWWWW.",
"..WWWWWWWW...........WWWWWWWW..",
"...WWWWWWWW.........WWWWWWWW...",
"....WWWWWWWW.......WWWWWWWW....",
".....WWWWWWWW.....WWWWWWWW.....",
"......WWWWWWWW...WWWWWWWW......",
".......WWWWWWWW.WWWWWWWW.......",
"........WWWWWWWWWWWWWWW........",
".........WWWWWWWWWWWWW.........",
"..........WWWWWWWWWWW..........",
"...........WWWWWWWWW...........",
"............WWWWWWW............",
".............WWWWW.............",
"..............WWW..............",
"...............W...............",
"...............................",

"...............................",
"...............W...............",
"..............WWW..............",
".............WWWWW.............",
"............WWWWWWW............",
"...........WWWWWWWWW...........",
"..........WWWWWWWWWWW..........",
".........WWWWWWWWWWWWW.........",
"........WWWWWWWWWWWWWWW........",
".......WWWWWWWWWWWWWWWWW.......",
"......WWWWWWWWWWWWWWWWWWW......",
".....WWWWWWWWWWWWWWWWWWWWW.....",
"....WWWWWWWW.......WWWWWWWW....",
"...WWWWWWWWW.......WWWWWWWWW...",
"..WWWWWWWWWW.......WWWWWWWWWW..",
".WWWWWWWWWWW.......WWWWWWWWWWW.",
"..WWWWWWWWWW.......WWWWWWWWWW..",
"...WWWWWWWWW.......WWWWWWWWW...",
"....WWWWWWWW.......WWWWWWWW....",
".....WWWWWWWWWWWWWWWWWWWWW.....",
"......WWWWWWWWWWWWWWWWWWW......",
".......WWWWWWWWWWWWWWWWW.......",
"........WWWWWWWWWWWWWWW........",
".........WWWWWWWWWWWWW.........",
"..........WWWWWWWWWWW..........",
"...........WWWWWWWWW...........",
"............WWWWWWW............",
".............WWWWW.............",
"..............WWW..............",
"...............W...............",
"...............................",

"...............................",
"...............W...............",
"..............WWW..............",
".............WWWWW.............",
"............WWWWWWW............",
"...........WWWWWWWWW...........",
"..........WWWWWWWWWWW..........",
".........WWWWWWWWWWWWW.........",
"........WWWWWWWWWWWWWWW........",
".......WWWWWWWWWWWWWWWWW.......",
"......WWWWWWWWWWWWWWWWWWW......",
".....WWWWWWWWWWWWWWWWWWWWW.....",
"....WWWWWWWWWWWWWWWWWWWWWWW....",
"...WWWWWWWWWWWWWWWWWWWWWWWWW...",
"..WWWWWWWWWWWWWWWWWWWWWWWWWWW..",
".WWWWWWWWWWWWWWWWWWWWWWWWWWWWW.",
"..WWWWWWWWWWWWWWWWWWWWWWWWWWW..",
"...WWWWWWWWWWWWWWWWWWWWWWWWW...",
"....WWWWWWWWWWWWWWWWWWWWWWW....",
".....WWWWWWWWWWWWWWWWWWWWW.....",
"......WWWWWWWWWWWWWWWWWWW......",
".......WWWWWWWWWWWWWWWWW.......",
"........WWWWWWWWWWWWWWW........",
".........WWWWWWWWWWWWW.........",
"..........WWWWWWWWWWW..........",
"...........WWWWWWWWW...........",
"............WWWWWWW............",
".............WWWWW.............",
"..............WWW..............",
"...............W...............",
"...............................",


"...............................",
"...............W...............",
"..............WWW..............",
".............WWWWW.............",
"............WWWWWWW............",
"...........WWWWWWWWW...........",
"..........WWWWWWWWWWW..........",
".........WWWWWWWWWWWWW.........",
"........WWWW.......WWWW........",
".......WWWWW.......WWWWW.......",
"......WWWWWW.......WWWWWW......",
".....WWWWWWW.......WWWWWWW.....",
"....WWWWWWWW.......WWWWWWWW....",
"...WWWWWWWWW.......WWWWWWWWW...",
"..WWWWWWWWWW.......WWWWWWWWWW..",
".WWWWWWWWWWW.......WWWWWWWWWWW.",
"..WWWWWWWWWW.......WWWWWWWWWW..",
"...WWWWWWWWW.......WWWWWWWWW...",
"....WWWWWWWW.......WWWWWWWW....",
".....WWWWWWW.......WWWWWWW.....",
"......WWWWWW.......WWWWWW......",
".......WWWWW.......WWWWW.......",
"........WWWW.......WWWW........",
".........WWWWWWWWWWWWW.........",
"..........WWWWWWWWWWW..........",
"...........WWWWWWWWW...........",
"............WWWWWWW............",
".............WWWWW.............",
"..............WWW..............",
"...............W...............",
"...............................",

"...............................",
"...............W...............",
"..............WWW..............",
".............WWWWW.............",
"............WWWWWWW............",
"...........WWWWWWWWW...........",
"..........WWWWWWWWWWW..........",
".........WWWWWWWWWWWWW.........",
"........WWWWWWWWWWWWWWW........",
".......WWWWWWWWWWWWWWWWW.......",
"......WWWWWWWWWWWWWWWWWWW......",
".....WWWWWWWWWWWWWWWWWWWWW.....",
"....WWWW...............WWWW....",
"...WWWWW...............WWWWW...",
"..WWWWWW...............WWWWWW..",
".WWWWWWW...............WWWWWWW.",
"..WWWWWW...............WWWWWW..",
"...WWWWW...............WWWWW...",
"....WWWW...............WWWW....",
".....WWWWWWWWWWWWWWWWWWWWW.....",
"......WWWWWWWWWWWWWWWWWWW......",
".......WWWWWWWWWWWWWWWWW.......",
"........WWWWWWWWWWWWWWW........",
".........WWWWWWWWWWWWW.........",
"..........WWWWWWWWWWW..........",
"...........WWWWWWWWW...........",
"............WWWWWWW............",
".............WWWWW.............",
"..............WWW..............",
"...............W...............",
"...............................",

"...............................",
"...............W...............",
"..............WWW..............",
".............WWWWW.............",
"............WWWWWWW............",
"...........WWWWWWWWW...........",
"..........WWWWWWWWWWW..........",
".........WWWWWWWWWWWWW.........",
"........WWW.WWWWWWW.WWW........",
".......WWW...WWWWW...WWW.......",
"......WWW.....WWW.....WWW......",
".....WWW.......W.......WWW.....",
"....WWWWW.............WWWWW....",
"...WWWWWWW...........WWWWWWW...",
"..WWWWWWWWW.........WWWWWWWWW..",
".WWWWWWWWWWW.......WWWWWWWWWWW.",
"..WWWWWWWWW.........WWWWWWWWW..",
"...WWWWWWW...........WWWWWWW...",
"....WWWWW.............WWWWW....",
".....WWW.......W.......WWW.....",
"......WWW.....WWW.....WWW......",
".......WWW...WWWWW...WWW.......",
"........WWW.WWWWWWW.WWW........",
".........WWWWWWWWWWWWW.........",
"..........WWWWWWWWWWW..........",
"...........WWWWWWWWW...........",
"............WWWWWWW............",
".............WWWWW.............",
"..............WWW..............",
"...............W...............",
"..............................."};

// colors for each cell state (we try to match colors used in icons)
static unsigned char jvncolors[] = {
    48,  48,  48,    // 0  dark gray
   255,   0,   0,    // 1  red
   255, 125,   0,    // 2  orange (to match red and yellow)
   255, 150,  25,    // 3   lighter
   255, 175,  50,    // 4    lighter
   255, 200,  75,    // 5     lighter
   255, 225, 100,    // 6      lighter
   255, 250, 125,    // 7       lighter
   251, 255,   0,    // 8  yellow
    89,  89, 255,    // 9  blue
   106, 106, 255,    // 10  lighter
   122, 122, 255,    // 11   lighter
   139, 139, 255,    // 12    lighter
    27, 176,  27,    // 13 green
    36, 200,  36,    // 14  lighter
    73, 255,  73,    // 15   lighter
   106, 255, 106,    // 16    lighter
   235,  36,  36,    // 17 red
   255,  56,  56,    // 18  lighter
   255,  73,  73,    // 19   lighter
   255,  89,  89,    // 20    lighter
   185,  56, 255,    // 21 purple
   191,  73, 255,    // 22  lighter
   197,  89, 255,    // 23   lighter
   203, 106, 255,    // 24    lighter
     0, 255, 128,    // 25 light green
   255, 128,  64,    // 26 light orange
   255, 255, 128,    // 27 light yellow
    33, 215, 215,    // 28 cyan
    27, 176, 176,    // 29  darker
    24, 156, 156,    // 30   darker
    21, 137, 137     // 31    darker
};

static lifealgo *creator() { return new jvnalgo() ; }

void jvnalgo::doInitializeAlgoInfo(staticAlgoInfo &ai) {
   ghashbase::doInitializeAlgoInfo(ai) ;
   ai.setAlgorithmName("JvN") ;
   ai.setAlgorithmCreator(&creator) ;
   ai.minstates = 29 ;
   ai.maxstates = 32 ;
   // init default color scheme
   ai.defgradient = false;
   ai.defr1 = ai.defg1 = ai.defb1 = 255;     // start color = white
   ai.defr2 = ai.defg2 = ai.defb2 = 128;     // end color = gray
   int numcolors = sizeof(jvncolors) / (sizeof(jvncolors[0])*3);
   unsigned char* rgbptr = jvncolors;
   for (int i = 0; i < numcolors; i++) {
      ai.defr[i] = *rgbptr++;
      ai.defg[i] = *rgbptr++;
      ai.defb[i] = *rgbptr++;
   }
   // init default icon data
   ai.defxpm7x7 = jvn7x7;
   ai.defxpm15x15 = jvn15x15;
   ai.defxpm31x31 = jvn31x31;
}

// ------------------- beginning of Hutton32 section -----------------------

	/***

Motivation: In the original von Neumann transition rules, lines of transmission states can
extend themselves by writing out binary signal trains, e.g. 10000 for extend with a right-directed
ordinary transmission state (OTS). But for construction, a dual-stranded construction arm (c-arm)
is needed, simply because the arm must be retracted after each write. I noticed that there was room
to add the needed write-and-retract operation by modifying the transition rules slightly. This
allows the machine to be greatly reduced in size and speed of replication.

Another modification was made when it was noticed that the construction could be made rotationally
invariant simply by basing the orientation of the written cell on the orientation of the one writing
it. Instead of "write an up arrow" we have "turn left". This allows us to spawn offspring in 
different directions and to fill up the space with more and more copies in a manner inspired by
Langton's Loops. 

A single OTS line can now act as a c-arm in any direction. Below are the signal trains:

100000 : move forward (write an OTS arrow in the same direction)
100010 : turn left
10100  : turn right
100001 : write a forward-directed OTS and retract
100011 : write a left-directed OTS and retract
10011  : write a reverse-directed OTS and retract
10101  : write a right-directed OTS and retract
101101 : write a forward-directed special transmission state (STS) and retract
110001 : write a left-directed STS and retract
110101 : write a reverse-directed STS and retract
111001 : write a right-directed STS and retract
1111   : write a confluent state and retract
101111 : retract

Achieving these features without adding new states required making some slight changes elsewhere,
though hopefully these don't affect the computation- or construction-universality of the CA. The 
most important effects are listed here:

1) OTS's cannot destroy STS's. This functionality was used in von Neumann's construction and 
read-write arms but isn't needed for the logic organs, as far as I know. The opposite operation
is still enabled.
2) STS lines can only construct one cell type: an OTS in the forward direction. Some logic organs
will need to be redesigned.

Under this modified JvN rule, a self-replicator can be much smaller, consisting only of a tape
contained within a repeater-emitter loop. One early example consisted of 5521 cells in total, and 
replicates in 44,201 timesteps, compared with 8 billion timesteps for the smallest known JvN-32
replicator. This became possible because the construction process runs at the same speed as a moving
signal, allowing the tape to be simply stored in a repeater-emitter loop. The machine simply creates
a loop of the right size (by counting tape circuits) before allowing the tape contents to fill up
their new home.

The rotational invariance allows the machine to make multiple copies oriented in different directions.
The population growth starts off as exponential but soons slows down as the long tapes obstruct the 
new copies.

Some context for these modifications to von Neumann's rule table:
Codd simplified vN's CA to a rotationally-invariant 8 states. Langton modified this to make a 
self-replicating repeater-emitter, his 'loops'. Other loops were made by Sayama, Perrier, Tempesti, 
Byl, Chou-Reggia, and others. So there are other CA derived from vN's that support faster replication
than that achieveable here, and some of them retain the computation- and construction-universality
that von Neumann was considering. Our modifications are mostly a historical exploration of the 
possibility space around vN's CA, to explore the questions of why he made the design decisions he did.
In particular, why didn't von Neumann design for a tape loop stored within a repeater-emitter? It would
have made his machine much simpler from the beginning. Why didn't he consider write-and-retraction
instead of designing a complicated c-arm procedure? Of course this is far from criticism of vN - his 
untimely death interrupted his work in this area. 

Some explanation of the details of the modifications is given below:

The transition rules are as in Nobili32 (or JvN29), except the following:
1) The end of an OTS wire, when writing a new cell, adopts one of two states: excited OTS and excited
STS, standing for bits 1 and 0 respectively. After writing the cell reverts to being an OTS.
2) A sensitized cell that is about to revert to an arrow bases its direction upon that of the excited
arrow that is pointing to it. 
3) A TS 'c', with a sensitized state 's' on its output that will become an OTS next (based on the 
state of 'c'), reverts to the ground state if any of 'c's input is 1, else it quiesces. 
4) A TS 'c', with a sensitized state 's' on its output that will become a confluent state next
(based on the state of 'c'), reverts to the first sensitized state S is any of 'c's input is one, 
else it reverts to the ground state.
5) A TS 'c', with an STS on its output, reverts to the ground state if any of 'c's input is 1.

Tim Hutton <tim.hutton@gmail.com>, 2008

	***/
	
bool is_OTS(state c) {
	return c>=9 && c<=16;
}
bool is_STS(state c) {
	return c>=17 && c<=24;
}
bool is_TS(state c) {
	return is_OTS(c) || is_STS(c);
}
bool is_sensitized(state c) {
	return c>=1 && c<=8;
}
bool is_east(state c) {
	return c==9 || c==13 || c==17 || c==21;
}
bool is_north(state c) {
	return c==10 || c==14 || c==18 || c==22;
}
bool is_west(state c) {
	return c==11 || c==15 || c==19 || c==23;
}
bool is_south(state c) {
	return c==12 || c==16 || c==20 || c==24;
}
bool is_excited(state c) {
	return (c>=13 && c<=16) || (c>=21 && c<=24);
}
state dir(state c) {	// return 0,1,2,3 encoding the direction of 'c': right,up,left,down
	return (c-9)%4;
}
state output(state c,state n,state s,state e,state w) // what is the state of the cell we are pointing to?
{
	if(is_east(c)) return e;
	else if(is_north(c)) return n;
	else if(is_west(c)) return w;
	else if(is_south(c)) return s;
	else return 0; // error
}
state input(state n,state s,state e,state w)	// what is the state of the excited cell pointing at us?
{
	if(is_east(w) && is_excited(w)) return w;
	else if(is_north(s) && is_excited(s)) return s;
	else if(is_west(e) && is_excited(e)) return e;
	else if(is_south(n) && is_excited(n)) return n;
	else return 0; // error
}
bool output_will_become_OTS(state c,state n,state s,state e,state w)
{
	return output(c,n,s,e,w)==8
		|| (output(c,n,s,e,w)==4 && is_excited(c))
		|| (output(c,n,s,e,w)==5 && !is_excited(c));
}
bool output_will_become_confluent(state c,state n,state s,state e,state w)
{
	return output(c,n,s,e,w)==7 && is_excited(c);
}
bool output_will_become_sensitized(state c,state n,state s,state e,state w)
{
	int out=output(c,n,s,e,w);
	return ((out==0 && is_excited(c)) || out==1 || out==2 || out==3 || (out==4 && !is_OTS(c)));
}
bool excited_arrow_to_us(state n,state s,state e,state w)
{
	return n==16 || n==24 || s==14 || s==22 || e==15 || e==23 || w==13 || w==21;
}
bool excited_OTS_to_us(state c,state n,state s,state e,state w) { // is there an excited OTS state that will hit us next?
	return ((n==16 || n==27 || n==28 || n==30 || n==31) && !(c==14 || c==10)) 
		|| ((s==14 || s==27 || s==28 || s==30 || s==31) && !(c==16 || c==12))
		|| ((e==15 || e==27 || e==28 || e==29 || e==31) && !(c==13 || c==9))
		|| ((w==13 || w==27 || w==28 || w==29 || w==31) && !(c==15 || c==11));
}
bool excited_OTS_arrow_to_us(state c,state n,state s,state e,state w) { // is there an excited OTS arrow pointing at us?
	return (n==16 && !(c==14 || c==10)) 
		|| (s==14 && !(c==16 || c==12))
		|| (e==15 && !(c==13 || c==9))
		|| (w==13 && !(c==15 || c==11));
}
bool OTS_arrow_to_us(state n,state s,state e,state w) {	// is there an OTS arrow pointing at us?
	return (is_OTS(n) && is_south(n)) || (is_OTS(s) && is_north(s)) 
		|| (is_OTS(e) && is_west(e)) || (is_OTS(w) && is_east(w));
}
bool excited_STS_to_us(state c,state n,state s,state e,state w) { // is there an excited STS state that will hit us next?
	return ((n==24 || n==27 || n==28 || n==30 || n==31) && !(c==22 || c==18)) 
		|| ((s==22 || s==27 || s==28 || s==30 || s==31) && !(c==24 || c==20))
		|| ((e==23 || e==27 || e==28 || e==29 || e==31) && !(c==21 || c==17))
		|| ((w==21 || w==27 || w==28 || w==29 || w==31) && !(c==23 || c==19));
}
bool excited_STS_arrow_to_us(state c,state n,state s,state e,state w) { // is there an excited STS arrow pointing at us?
	return (n==24 && !(c==22 || c==18)) 
		|| (s==22 && !(c==24 || c==20))
		|| (e==23 && !(c==21 || c==17))
		|| (w==21 && !(c==23 || c==19));
}
bool all_inputs_on(state n,state s,state e,state w) {
	return (!(n==12 || s==10 || e==11 || w==9)) && (n==16 || s==14 || e==15 || w==13);
}
bool is_crossing(state n,state s,state e,state w) 
{
	int n_inputs=0;
	if(is_south(n)) n_inputs++;
	if(is_east(w)) n_inputs++;
	if(is_west(e)) n_inputs++;
	if(is_north(s)) n_inputs++;
	int n_outputs=0;
	if(is_TS(n) && !is_south(n)) n_outputs++;
	if(is_TS(w) && !is_east(w)) n_outputs++;
	if(is_TS(e) && !is_west(e)) n_outputs++;
	if(is_TS(s) && !is_north(s)) n_outputs++;
	return n_inputs==2 && n_outputs==2;
}
state quiesce(state c)
{
	if(((c>=13 && c<=16) || (c>=21 && c<=24)))
		return c-4;
	else if(c>=26 && c<=31)
		return 25;
	else
		return c;
}
// the update function itself
state slowcalc_Hutton32(state c,state n,state s,state e,state w)
{
	if(is_OTS(c))
	{
		if(excited_STS_arrow_to_us(c,n,s,e,w))
			return 0;		// we get destroyed by the incoming excited STS
		else if(excited_OTS_to_us(c,n,s,e,w))
		{
			if(output_will_become_OTS(c,n,s,e,w) || (is_STS(output(c,n,s,e,w)) && !is_excited(output(c,n,s,e,w))))
				return 0;	// we become the ground state (retraction)
			else if(output_will_become_confluent(c,n,s,e,w))
				return 1;	// we become sensitized by the next input (after retraction)
			else
				return quiesce(c)+4;	// we become excited (usual OTS transmission)
		}
		else if(output_will_become_confluent(c,n,s,e,w))
			return 0;		// we become the ground state (retraction)
		else if(is_excited(c) && output_will_become_sensitized(c,n,s,e,w))
			return quiesce(c)+12;	// we become excited STS (special for end-of-wire: 
					// means quiescent OTS, used to mark which cell is the sensitized cell's input)
		else
			return quiesce(c);
	}
	else if(is_STS(c))
	{
		if(is_excited(c) && is_sensitized(output(c,n,s,e,w)) && OTS_arrow_to_us(n,s,e,w))
		{
			// this cell is the special mark at the end of an OTS wire, so it behaves differently
			// if output is about to finalize, we revert to ground or quiescent OTS, depending on next signal
			// if output will remain sensitized, we change to excited OTS if next signal is 1
			if(output_will_become_sensitized(c,n,s,e,w))
			{
				if(excited_OTS_arrow_to_us(c,n,s,e,w))
					return c-8;
				else
					return c;
			}
			else {
				if(excited_OTS_arrow_to_us(c,n,s,e,w))
					return 0;	// write-and-retract
				else
					return quiesce(c)-8;	// revert to quiescent OTS
			}
		}
		else if(is_excited(c) && output(c,n,s,e,w)==0)
			if(excited_STS_arrow_to_us(c,n,s,e,w))
				return c;	// we remain excited
			else
				return quiesce(c);	// we quiesce
		else if(excited_OTS_arrow_to_us(c,n,s,e,w))
			return 0;	// we get destroyed by the incoming excited OTS
		else if(excited_STS_to_us(c,n,s,e,w))
			return quiesce(c)+4;	// we become excited (usual STS transmission)
		else
			 return quiesce(c);	// we quiesce (usual STS transmission)
	}
	else if(c==0)
	{
		if(excited_OTS_arrow_to_us(c,n,s,e,w)) // (excludes e.g. excited confluent states)
			return 1;	// we become sensitized
		else if(excited_STS_arrow_to_us(c,n,s,e,w))
			return quiesce(input(n,s,e,w))-8;	// directly become 'forward' OTS
		else return c;
	}
	else if(c==1)
	{
		if(!excited_OTS_arrow_to_us(c,n,s,e,w)) 
			return 2; 	// 10
		else
			return 3;	// 11
	}
	else if(c==2)
	{
		if(!excited_OTS_arrow_to_us(c,n,s,e,w))
			return 4;	// 100
		else
			return 5;	// 101
	}
	else if(c==3)
	{
		if(!excited_OTS_arrow_to_us(c,n,s,e,w))
			return 6; 	// 110
		else 
			return 7;	// 111
	}
	else if(c==4)
	{
		if(!excited_OTS_arrow_to_us(c,n,s,e,w))
			return 8; 	// 1000
		else
			return ( (quiesce(input(n,s,e,w))-9+2) % 4 )+9;	// 1001: reverse
	}
	else if(c==5)
	{
		if(!excited_OTS_arrow_to_us(c,n,s,e,w))
			return ( (quiesce(input(n,s,e,w))-9+3) % 4 )+9; 	// 1010: turn right
		else
			return quiesce(input(n,s,e,w))+8;	// 1011: STS forward
	}
	else if(c==6)
	{
		if(!excited_OTS_arrow_to_us(c,n,s,e,w))
			return ( (quiesce(input(n,s,e,w))-9+1) % 4 )+17; 	// 1100: STS turn left
		else	
			return ( (quiesce(input(n,s,e,w))-9+2) % 4 )+17;	// 1101: STS reverse
	}
	else if(c==7)
	{
		if(!excited_OTS_arrow_to_us(c,n,s,e,w))
			return ( (quiesce(input(n,s,e,w))-9+3) % 4 )+17; 	// 1110: STS turn left
		else	
			return 25;	// 1111
	}
	else if(c==8)
	{
		if(!excited_OTS_arrow_to_us(c,n,s,e,w))
			return 9+dir(input(n,s,e,w)); 	// 10000: move forward
		else
			return 9+dir(input(n,s,e,w)+1);	// 10001: turn left
	}
	else if(c==25) 	// quiescent confluent state
	{
		if(excited_STS_arrow_to_us(c,n,s,e,w))
			return 0;	// we get destroyed by the incoming excited STS
		else if(is_crossing(n,s,e,w)) // for JvN-32 crossings
		{
			if((n==16||s==14)&&(e==15||w==13))
				return 31;	// double crossing
			else if(n==16||s==14)
				return 30;	// vertical crossing
			else if(e==15||w==13)
				return 29;	// horizontal crossing
			else
				return 25;	// nothing happening
		}
		else if(all_inputs_on(n,s,e,w))
			return 26;
		else
			return 25;
	}
	else if(c==26)
	{
		if(excited_STS_arrow_to_us(c,n,s,e,w))
			return 0;	// we get destroyed by the incoming excited STS
		else if(all_inputs_on(n,s,e,w))
			return 28;
		else
			return 27;
	}
	else if(c==27)
	{
		if(excited_STS_arrow_to_us(c,n,s,e,w))
			return 0;	// we get destroyed by the incoming excited STS
		else if(all_inputs_on(n,s,e,w))
			return 26;
		else
			return 25;
	}
	else if(c==28)
	{
		if(excited_STS_arrow_to_us(c,n,s,e,w))
			return 0;	// we get destroyed by the incoming excited STS
		else if(all_inputs_on(n,s,e,w))
			return 28;
		else
			return 27;
	}
	else if(c==29 || c==30 || c==31)
	{
		if(excited_STS_arrow_to_us(c,n,s,e,w))
			return 0;	// we get destroyed by the incoming excited STS
		else if((n==16||s==14)&&(e==15||w==13))
			return 31;	// double crossing
		else if(n==16||s==14)
			return 30;	// vertical crossing
		else if(e==15||w==13)
			return 29;	// horizontal crossing
		else
			return 25;	// revert to quiescent confluent state
	}
	else
		return c;	// error - should be no more states
}
// ------------------ end of Hutton32 section -------------------------

