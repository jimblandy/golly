// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef GENERALGO_H
#define GENERALGO_H
#include "ghashbase.h"
/**
 *   Our Generations algo class.
 */
class generationsalgo : public ghashbase {
public:
   generationsalgo() ;
   virtual ~generationsalgo() ;
   virtual state slowcalc(state nw, state n, state ne, state w, state c,
                          state e, state sw, state s, state se) ;
   virtual const char* setrule(const char* s) ;
   virtual const char* getrule() ;
   virtual const char* DefaultRule() ;
   virtual int NumCellStates() ;
   static void doInitializeAlgoInfo(staticAlgoInfo &) ;

   enum neighborhood_masks {
      MOORE = 0x1ff,         // all 8 neighbors
      HEXAGONAL = 0x1bb,     // ignore NE and SW neighbors
      VON_NEUMANN = 0x0ba    // 4 orthogonal neighbors
   } ;

   bool isHexagonal() const { return neighbormask == HEXAGONAL ; }
   bool isVonNeumann() const { return neighbormask == VON_NEUMANN ; }

private:
   char canonrule[MAXRULESIZE] ;      // canonical version of valid rule passed into setrule
   neighborhood_masks neighbormask ;  // neighborhood masks in 3x3 table
   bool totalistic ;                  // is rule totalistic?
   bool using_map ;                   // is rule a map?
   int neighbors ;                    // number of neighbors
   int rulebits ;                     // bitmask of neighbor counts used (9 birth, 9 survival)
   int letter_bits[18] ;              // bitmask for non-totalistic letters used
   int negative_bit ;                 // bit in letters bits mask indicating negative
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
   void createRuleMapFromMAP(const char *base64) ;
   void createRuleMap(const char *birth, const char *survival) ;
   void createCanonicalName(const char *base64) ;
   void removeChar(char *string, char skip) ;
   bool lettersValid(const char *part) ;
   int addLetters(int count, int p) ;
} ;

#endif
