// This file is part of Golly.
// See docs/License.html for the copyright notice.

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
const int MAXRULESIZE = 500 ;  // maximum number of characters in a rule
const int ALL3X3 = 512 ;       // all possible 3x3 cell combinations
const int ALL4X4 = 65536 ;     // all possible 4x4 cell combinations
const int MAP512LENGTH = 86 ;  // number of base64 characters to encode 512bit map for Moore neighborhood
const int MAP128LENGTH = 22 ;  // number of base64 characters to encode 128bit map for Hex neighborhood
const int MAP32LENGTH  = 6 ;   // number of base64 characters to encode 32bit map for von Neumann neighborhood

class liferules {
public:
   liferules() ;
   ~liferules() ;
   // string returned by setrule is any error
   const char *setrule(const char *s, lifealgo *algo) ;
   const char *getrule() ;
   
   // AKT: we need 2 tables to support B0-not-Smax rule emulation
   // where max is 8, 6 or 4 depending on the neighborhood
   char rule0[ALL4X4] ;      // rule table for even gens if rule has B0 but not Smax,
                             // or for all gens if rule has no B0, or it has B0 *and* Smax
   char rule1[ALL4X4] ;      // rule table for odd gens if rule has B0 but not Smax
   bool alternate_rules ;    // set by setrule; true if rule has B0 but not Smax
   
   // AKT: support for various neighborhoods
   // rowett: support for non-totalistic isotropic rules
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
   char canonrule[MAXRULESIZE] ;      // canonical version of valid rule passed into setrule
   neighborhood_masks neighbormask ;  // neighborhood masks in 3x3 table
   bool totalistic ;                  // is rule totalistic?
   bool using_map ;                   // is rule a map?
   int neighbors ;                    // number of neighbors
   int rulebits ;                     // bitmask of neighbor counts used (9 birth, 9 survival)
   int letter_bits[18] ;              // bitmask for non-totalistic letters used
   int negative_bit ;                 // bit in letters bits mask indicating negative
   int wolfram ;                      // >= 0 if Wn rule (n is even and <= 254)
   int survival_offset ;              // bit offset in rulebits for survival
   int max_letters[18] ;              // maximum number of letters per neighbor count
   const int *order_letters[18] ;     // canonical letter order per neighbor count
   const char *valid_rule_letters ;   // all valid letters
   const char *rule_letters[4] ;      // valid rule letters per neighbor count
   const int *rule_neighborhoods[4] ; // isotropic neighborhoods per neighbor count
   char rule3x3[ALL3X3] ;             // all 3x3 cell mappings 012345678->4'
   const char *base64_characters ;    // base 64 encoding characters

   void initRule() ;
   void setTotalistic(int value, bool survival) ;
   int flipBits(int x) ;
   int rotateBits90Clockwise(int x) ;
   void setSymmetrical512(int x, int b) ;
   void setSymmetrical(int value, bool survival, int lindex, int normal) ;
   void setTotalisticRuleFromString(const char *rule, bool survival) ;
   void setRuleFromString(const char *rule, bool survival) ;
   void createWolframMap() ;
   void createRuleMapFromMAP(const char *base64) ;
   void createRuleMap(const char *birth, const char *survival) ;
   void convertTo4x4Map(char *which) ;
   void saveRule() ;
   void createCanonicalName(lifealgo *algo, const char *base64) ;
   void removeChar(char *string, char skip) ;
   bool lettersValid(const char *part) ;
   int addLetters(int count, int p) ;
} ;
#endif
