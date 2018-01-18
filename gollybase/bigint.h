// This file is part of Golly.
// See docs/License.html for the copyright notice.

/**
 *   Class bigint manages signed bigints using a very Lisp-ish approach.
 *   Integers from -0x40000000 through 0x3fffffff are represented
 *   by a direct instance of this four-byte class, with the lowest
 *   bit set.  Integers outside that range use a pointer to an
 *   integer array; the first element is how many elements of
 *   that array are used.  The array itself is always a power of
 *   two in size, the smallest power of two greater than
 *   the number of used elements.
 *
 *   The value of the bigint, when represented as a vector, is
 *   always sum 1<=i<=v.p[0] 2^(31*(i-1))*v.p[i]
 *
 *   You can use this as a value class, in which case it may do a
 *   lot of allocation/deallocation during copy and assignment, or
 *   (preferably, for efficiency) you can use it as a mutating
 *   class, with +=, -=, and the like operators that will not
 *   allocate/free unnecessarily.
 *
 *   We'll add mempool support soon.
 *
 *   If we are using an int array, each holds 31 bits of the number.
 *   All elements except the last are in the range 0..2^31-1; the
 *   last is always either 0 or -1.
 *   (This is an invariant; note that we have only canonical forms.)
 *   If it is a positive number, the last element is 0; if it is
 *   a negative number, the last element is -1.  You can consider
 *   the last element as
 *   the sign except that the other components are in two's
 *   complement form.
 *
 *   We use a binary form (the radix is 2^31) because that makes
 *   bit extraction faster, at the expense of decimal conversion.
 *   But decimal conversion is not *that* onerous, so this is
 *   okay.
 *
 *   This code is *very* carefully written so that things like
 *   the comparisons can be done simply; specifically, we always
 *   have a single canonical representation of each number.
 *
 *   We never use an array size smaller than 4.
 *
 *   Nonnegative numbers are represented as follows:
 *
 *   0..2^30-1     Directly, shifted left one with the low bit set
 *   2^30..2^31-1  siz=2, low 31 bits then 0; array size is 4
 *   2^31..2^62-1  size=3, two 31-bit words then 0; array size is 4
 *   2^62..2^93-1  size=4, three 31-bit words then 0; array size is 8
 *   ...
 *   2^155..2^186-1  size=7, six 31-bit words than 0; array size is 8
 *   2^186..2^217-1  size=8, seven 31-bit words then 0; array size is 16
 *
 *   Negative numbers are analogous:
 *
 *   -1..-2^30     Directly, shifted left one with the low bit set
 *   -2^30-1..-2^31  siz=2, low 31 bits then -1; array size is 4
 *   -2^31-1..-2^62  size=3, two 31-bit words then -1; array size is 4
 *   -2^62-1..-2^93  size=4, three 31-bit words then -1; array size is 8
 *   ...
 *   -2^155-1..-2^186  size=7, six 31-bit words than -1; array size is 8
 *   -2^186-1..-2^217  size=8, seven 31-bit words then -1; array size is 16
 *
 *   The only upper bound on the size of these numbers is memory.
 *
 *   We only provide a limited number of arithmetic operations for the
 *   moment.  (Addition, subtraction, comparison, bit extraction,
 *   radix conversion, parsing, copying, assignment, to float, to double,
 *   to int, to long long, minbits).
 */
#ifndef BIGINT_H
#define BIGINT_H

#ifdef _MSC_VER
#if _MSC_VER < 1300  // 12xx = VC6; 13xx = VC7
#define G_INT64          __int64
#define G_MAKEINT64(x)   x ## i64
#define G_INT64_FMT      "I64d"
#endif
#endif

#ifndef G_INT64
#define G_INT64          long long
#define G_MAKEINT64(x)   x ## LL
#define G_INT64_FMT      "lld"
#endif

class bigint {
public:
   bigint() { v.i = 1 ; }
   bigint(short i) { v.i = ((int)i << 1) + 1 ; }
   bigint(int i) { fromint(i) ; }
   bigint(G_INT64 i) ;
   bigint(const char *s) ;
   bigint(const bigint &a) ;
   // create a new bigint by adding four other bigints; fastpath for popcount
   bigint(const bigint &a, const bigint &b, const bigint &c, const bigint &d) ;
   ~bigint() ;
   bigint& operator=(const bigint &a) ;
   bigint& operator+=(const bigint &a) ;
   bigint& operator-=(const bigint &a) ;
   bigint& operator>>=(int i) ;
   bigint& operator<<=(int i) ;
   void mulpow2(int p) ;
   int operator==(const bigint &b) const ;
   int operator!=(const bigint &b) const ;
   int operator<=(const bigint &b) const ;
   int operator>=(const bigint &b) const ;
   int operator<(const bigint &b) const ;
   int operator>(const bigint &b) const ;
   int even() const ;
   int odd() const ;
   int low31() const ; // return the low 31 bits quickly
   int lowbitset() const ; // return the index of the lowest set bit
   const char *tostring(char sep=sepchar) const ;
   int sign() const ;
   // note: a should be a small positive int, say 1..10,000
   void mul_smallint(int a) ;
   // note: a should be a small positive int, say 1..10,000
   void div_smallint(int a) ;
   // note: a should be a small positive int, say 1..10,000
   int mod_smallint(int a) ;
   // note: a should be a small positive int, say 1..10,000
   void div2() ;
   // note:  a may only be a *31* bit int, not just 0 or 1
   // note:  may only be called on arrayed bigints
   void add_smallint(int a) ;
   double todouble() const ;
   double toscinot() const ;
   int toint() const ;
   // static values predefined
   static const bigint zero, one, two, three, minint, maxint ;
   // editing limits
   static const bigint min_coord, max_coord ;
   // how many bits required to represent this (approximately)?
   int bitsreq() const ;
   // fill in one bit per char, up to n.
   void tochararr(char *ar, int siz) const ;
private:
   // note:  may only be called on arrayed bigints
   int size() const ;
   // do we need to shrink it to keep it canonical?
   void shrink(int pos) ;
   void grow(int osz, int nsz) ;
   // note:  carry may be any legal 31-bit int
   // note:  may only be called on arrayed bigints
   void ripple(int carry, int pos) ;
   void ripple(const bigint &a, int carry) ;
   void ripplesub(const bigint &a, int carry) ;
   // make sure it's in vector form; may leave it not canonical!
   void vectorize(int i) ;
   void fromint(int i) ;
   void ensurework(int sz) const ;
   union {
      int i ;
      int *p ;
   } v ;
   static char *printbuf ;
   static int *work ;
   static int printbuflen ;
   static int workarrlen ;
   static char sepchar ;
   static int sepcount ;
} ;
#endif
