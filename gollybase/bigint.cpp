// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "bigint.h"
#include <cstdlib>
#include <string.h>
#include <iostream>
#include <cmath>
#include <limits.h>
#include "util.h"
#undef SLOWCHECK
using namespace std ;
/**
 *   Static data.
 */
static const int MAX_SIMPLE = 0x3fffffff ;
static const int MIN_SIMPLE = -0x40000000 ;
char *bigint::printbuf ;
int *bigint::work ;
int bigint::printbuflen ;
int bigint::workarrlen ;
char bigint::sepchar = ',' ;
int bigint::sepcount = 3 ;
/**
 *   Routines.
 */
bigint::bigint(G_INT64 i) {
   if (i <= INT_MAX && i >= INT_MIN)
      fromint((int)i) ;
   else {
      v.i = 0 ;
      vectorize(0) ;
      v.p[0] = 3 ;
      v.p[1] = (int)(i & 0x7fffffff) ;
      v.p[2] = (int)((i >> 31) & 0x7fffffff) ;
      v.p[3] = 0 ;
      ripple((int)(i >> 62), 3) ;
   }
}
// we can parse ####, 2^###, -#####
// AKT: we ignore all non-digits (except for leading '-')
// so we can parse strings like "1,234" or "+1.234";
// it is up to caller to impose smarter restrictions
bigint::bigint(const char *s) {
   if (*s == '2' && s[1] == '^') {
      long x = atol(s+2) ;
      if (x < 31)
         fromint(1 << x) ;
      else {
         int sz = 2 + int((x + 1) / 31) ;
         int asz = sz ;
         while (asz & (asz - 1))
            asz &= asz - 1 ;
         asz *= 2 ;
         v.p = new int[asz] ;
         v.p[0] = sz ;
         for (int i=1; i<=sz; i++)
            v.p[i] = 0 ;
         v.p[sz-1] = 1 << (x % 31) ;
      }
   } else {
      int neg = 0 ;
      if (*s == '-') {
         neg = 1 ;
         s++ ;
      }
      fromint(0) ;
      while (*s) {
         // AKT: was *s != sepchar
         if (*s >= '0' && *s <= '9') {
            mul_smallint(10) ;
            if (neg)
               add_smallint('0'-*s) ;
            else
               add_smallint(*s-'0') ;
         }
         s++ ;
      }
   }
}
static int *copyarr(int *p) {
   int sz = *p ;
   while (sz & (sz - 1))
      sz &= sz - 1 ;
   sz *= 2 ;
   int *r = new int[sz] ;
   memcpy(r, p, sizeof(int) * (p[0] + 1)) ;
#ifdef SLOWCHECK
   for (int i=p[0]+1; i<sz; i++)
      r[i] = 0xdeadbeef ;
#endif
   return r ;
}
bigint::bigint(const bigint &a) {
   if (a.v.i & 1)
      v.i = a.v.i ;
   else
      v.p = copyarr(a.v.p) ;
}
/**
 *   We special-case the p=0 case so we can "initialize" bigint memory
 *   with zero.
 */
bigint &bigint::operator=(const bigint &b) {
   if (&b != this) {
      if (0 == (v.i & 1))
         if (v.p)
            delete [] v.p ;
      if (b.v.i & 1)
         v.i = b.v.i ;
      else
         v.p = copyarr(b.v.p) ;
   }
   return *this ;
}
bigint::~bigint() {
   if (0 == (v.i & 1))
      delete [] v.p ;
}
bigint::bigint(const bigint &a, const bigint &b, const bigint &c, const bigint &d) {
   const int checkmask = 0xf0000001 ;
   if ((a.v.i & checkmask) == 1 && (b.v.i & checkmask) == 1 &&
       (c.v.i & checkmask) == 1 && (d.v.i & checkmask) == 1) {
      // hot path
      v.i = a.v.i + b.v.i + c.v.i + d.v.i - 3 ;
      return ;
   }
   v.i = 1 ;
   *this = a ;
   *this += b ;
   *this += c ;
   *this += d ;
}
const char *bigint::tostring(char sep) const {
   int lenreq = 32 ;
   if ((v.i & 1) == 0)
      lenreq = size() * 32 ;
   if (lenreq > printbuflen) {
     if (printbuf)
         delete [] printbuf ;
      printbuf = new char[2 * lenreq] ;
      printbuflen = 2 * lenreq ;
   }
   int sz = 1 ;
   if (0 == (v.i & 1))
      sz = size() ;
   ensurework(sz) ;
   int neg = sign() < 0 ;
   if (v.i & 1) {
      if (neg)
         work[0] = -(v.i >> 1) ;
      else
         work[0] = v.i >> 1 ;
   } else {
      if (neg) {
         int carry = 1 ;
         for (int i=0; i+1<sz; i++) {
            int c = carry + (v.p[i+1] ^ 0x7fffffff) ;
            work[i] = c & 0x7fffffff ;
            carry = (c >> 31) & 1 ;
         }
         work[sz-1] = carry + ~v.p[sz] ;
      } else {
         for (int i=0; i<sz; i++)
            work[i] = v.p[i+1] ;
      }
   }
   char *p = printbuf ;
   const int bigradix = 1000000000 ; // 9 digits at a time
   for (;;) {
      int allbits = 0 ;
      int carry = 0 ;
      int i;
      for (i=sz-1; i>=0; i--) {
         G_INT64 c = carry * G_MAKEINT64(0x80000000) + work[i] ;
         carry = (int)(c % bigradix) ;
         work[i] = (int)(c / bigradix) ;
         allbits |= work[i] ;
      }
      for (i=0; i<9; i++) { // put the nine digits in
         *p++ = (char)(carry % 10 + '0') ;
         carry /= 10 ;
      }
      if (allbits == 0)
         break ;
   }
   while (p > printbuf + 1 && *(p-1) == '0')
      p-- ;
   char *r = p ;
   if (neg)
      *r++ = '-' ;
   for (int i=(int)(p-printbuf-1); i>=0; i--) {
      *r++ = printbuf[i] ;
      if (i && sep && (i % sepcount == 0))
         *r++ = sep ;
   }
   *r++ = 0 ;
   return p ;
}
void bigint::grow(int osz, int nsz) {
   int bdiffs = osz ^ nsz ;
   if (bdiffs > osz) {
      while (bdiffs & (bdiffs - 1))
         bdiffs &= bdiffs - 1 ;
      int *nv = new int[2*bdiffs] ;
      for (int i=0; i<=osz; i++)
         nv[i] = v.p[i] ;
#ifdef SLOWCHECK
      for (int i=osz+1; i<2*bdiffs; i++)
         nv[i] = 0xdeadbeef ;
#endif
      delete [] v.p ;
      v.p = nv ;
   }
   int av = v.p[osz] ;
   while (osz < nsz) {
      v.p[osz] = av & 0x7fffffff ;
      osz++ ;
   }
   v.p[nsz] = av ;
   v.p[0] = nsz ;
}
bigint& bigint::operator+=(const bigint &a) {
   if (a.v.i & 1)
      add_smallint(a.v.i >> 1) ;
   else {
      if (v.i & 1)
         vectorize(v.i >> 1) ;
      ripple(a, 0) ;
   }
   return *this ;
}
bigint& bigint::operator-=(const bigint &a) {
   if (a.v.i & 1)
      add_smallint(-(a.v.i >> 1)) ;
   else {
      if (v.i & 1)
         vectorize(v.i >> 1) ;
      ripplesub(a, 1) ;
   }
   return *this ;
}
int bigint::sign() const {
   int si = v.i ;
   if (0 == (si & 1))
      si = v.p[size()] ;
   if (si > 0)
      return 1 ;
   if (si < 0)
      return -1 ;
   return 0 ;
}
int bigint::size() const {
   return v.p[0] ;
}
void bigint::add_smallint(int a) {
   if (v.i & 1)
      fromint((v.i >> 1) + a) ;
   else
      ripple(a, 1) ;
}
// do we need to shrink it to keep it canonical?
void bigint::shrink(int pos) {
   while (pos > 1 && ((v.p[pos] - v.p[pos-1]) & 0x7fffffff) == 0) {
      pos-- ;
      v.p[pos] = v.p[pos+1] ;
      v.p[0] = pos ;
   }
   if (pos == 1) {
      int c = v.p[1] ;
      delete [] v.p ;
      v.i = 1 | (c << 1) ;
   } else if (pos == 2 && ((v.p[2] ^ v.p[1]) & 0x40000000) == 0) {
      int c = v.p[1] + (v.p[2] << 31) ;
      delete [] v.p ;
      v.i = 1 | (c << 1) ;
   }
}
void grow(int osz, int nsz) ;
// note:  carry may be any legal 31-bit int, *or* positive 2^30
// note:  may only be called on arrayed bigints
void bigint::ripple(int carry, int pos) {
   int c ;
   int sz = size() ;
   while (pos < sz) {
      c = v.p[pos] + (carry & 0x7fffffff) ;
      carry = ((c >> 31) & 1) + (carry >> 31) ;
      v.p[pos++] = c & 0x7fffffff;
   }
   c = v.p[pos] + carry ;
   if (c == 0 || c == -1) { // see if we can make it smaller
      v.p[pos] = c ;
      shrink(pos) ;
   } else { // need to extend
      if (0 == (sz & (sz + 1))) { // grow array (oops)
         grow(sz, sz+1) ;
      } else
         v.p[0] = sz + 1 ;
      v.p[pos] = c & 0x7fffffff ;
      v.p[pos+1] = -((c >> 31) & 1) ;
   }
}
void bigint::ripple(const bigint &a, int carry) {
   int asz = a.size() ;
   int tsz = size() ;
   int pos = 1 ;
   if (tsz < asz) { // gotta resize
      grow(tsz, asz) ;
      tsz = asz ;
   }
   while (pos < asz) {
      int c = v.p[pos] + a.v.p[pos] + carry ;
      carry = (c >> 31) & 1 ;
      v.p[pos++] = c & 0x7fffffff;
   }
   ripple(carry + a.v.p[pos], pos) ;
}
void bigint::ripplesub(const bigint &a, int carry) {
   int asz = a.size() ;
   int tsz = size() ;
   int pos = 1 ;
   if (tsz < asz) { // gotta resize
      grow(tsz, asz) ;
      tsz = asz ;
   }
   while (pos < asz) {
      int c = v.p[pos] + (0x7fffffff ^ a.v.p[pos]) + carry ;
      carry = (c >> 31) & 1 ;
      v.p[pos++] = c & 0x7fffffff;
   }
   ripple(carry + ~a.v.p[pos], pos) ;
}
// make sure it's in vector form; may leave it not canonical!
void bigint::vectorize(int i) {
   v.p = new int[4] ;
   v.p[0] = 2 ;
   v.p[1] = i & 0x7fffffff ;
   if (i < 0)
      v.p[2] = -1 ;
   else
      v.p[2] = 0 ;
#ifdef SLOWCHECK
   v.p[3] = 0xdeadbeef ;
#endif
}
void bigint::fromint(int i) {
   if (i <= MAX_SIMPLE && i >= MIN_SIMPLE)
      v.i = (i << 1) | 1 ;
   else
      vectorize(i) ;
}
void bigint::ensurework(int sz) const {
   sz += 3 ;
   if (sz > workarrlen) {
      if (work)
         delete [] work ;
      work = new int[sz * 2] ;
      workarrlen = sz * 2 ;
   }
}
void bigint::mul_smallint(int a) {
   int c ;
   if (a == 0) {
      *this = 0 ;
      return ;
   }
   if (v.i & 1) {
      if ((v.i >> 1) <= MAX_SIMPLE / a) {
         c = (v.i >> 1) * a ;
         fromint((v.i >> 1) * a) ;
         return ;
      }
      vectorize(v.i >> 1) ;
   }
   int sz = size() ;
   int carry = 0 ;
   int pos = 1 ;
   while (pos < sz) {
      int clo = (v.p[pos] & 0xffff) * a + carry ;
      int chi = (v.p[pos] >> 16) * a + (clo >> 16) ;
      carry = chi >> 15 ;
      v.p[pos++] = (clo & 0xffff) + ((chi & 0x7fff) << 16) ;
   }
   c = v.p[pos] * a + carry ;
   if (c == 0 || c == -1) { // see if we can make it smaller
      v.p[pos] = c ;
      shrink(pos) ;
   } else { // need to extend
      if (0 == (sz & (sz + 1))) { // grow array (oops)
         grow(sz, sz+1) ;
      } else
         v.p[0] = sz + 1 ;
      v.p[pos] = c & 0x7fffffff ;
      v.p[pos+1] = -((c >> 31) & 1) ;
   }
}
void bigint::div_smallint(int a) {
   if (v.i & 1) {
      int r = (v.i >> 1) / a ;
      fromint(r) ;
      return ;
   }
   if (v.p[v.p[0]] < 0)
      lifefatal("we don't support divsmallint when negative yet") ;
   int carry = 0 ;
   int pos = v.p[0] ;
   while (pos > 0) {
      G_INT64 t = ((G_INT64)carry << G_MAKEINT64(31)) + v.p[pos] ;
      carry = (int)(t % a) ;
      v.p[pos] = (int)(t / a) ;
      pos-- ;
   }
   shrink(v.p[0]) ;
}
int bigint::mod_smallint(int a) {
   if (v.i & 1)
      return (((v.i >> 1) % a) + a) % a ;
   int pos = v.p[0] ;
   int mm = (2 * ((1 << 30) % a) % a) ;
   int r = 0 ;
   while (pos > 0) {
      r = (mm * r + v.p[pos]) % a ;
      pos-- ;
   }
   return (r + a) % a ;
}
void bigint::div2() {
   if (v.i & 1) {
      v.i = ((v.i >> 1) | 1) ;
      return ;
   }
   int sz = v.p[0] ;
   int carry = -v.p[sz] ;
   for (int i=sz-1; i>0; i--) {
      int c = (v.p[i] >> 1) + (carry << 30) ;
      carry = v.p[i] & 1 ;
      v.p[i] = c ;
   }
   shrink(v.p[0]) ;
}
bigint& bigint::operator>>=(int i) {
   if (v.i & 1) {
      if (i > 31)
         v.i = ((v.i >> 31) | 1) ;
      else
         v.i = ((v.i >> i) | 1) ;
      return *this ;
   }
   int bigsh = i / 31 ;
   if (bigsh) {
      if (bigsh >= v.p[0]) {
         v.p[1] = v.p[v.p[0]] ;
         v.p[0] = 1 ;
      } else {
         for (int j=1; j+bigsh<=v.p[0]; j++)
            v.p[j] = v.p[j+bigsh] ;
         v.p[0] -= bigsh ;
      }
      i -= bigsh * 31 ;
   }
   int carry = v.p[v.p[0]] ;
   if (i) {
      for (int j=v.p[0]-1; j>0; j--) {
         int c = ((v.p[j] >> i) | (carry << (31-i))) & 0x7fffffff ;
         carry = v.p[j] ;
         v.p[j] = c ;
      }
   }
   shrink(v.p[0]) ;
   return *this ;
}
bigint& bigint::operator<<=(int i) {
   if (v.i & 1) {
      if (v.i == 1)
         return *this ;
      if (i < 30 && (v.i >> 31) == (v.i >> (31 - i))) {
         v.i = ((v.i & ~1) << i) | 1 ;
         return *this ;
      }
      vectorize(v.i >> 1) ;
   }
   int bigsh = i / 31 ;
   int nsize = v.p[0] + bigsh + 1 ; // how big we need it to be, worst case
   grow(v.p[0], nsize) ;
   if (bigsh) {
      int j ;
      for (j=v.p[0]-1; j>bigsh; j--)
         v.p[j] = v.p[j-bigsh] ;
      for (j=bigsh; j>0; j--)
         v.p[j] = 0 ;
      i -= bigsh * 31 ;
   }
   int carry = 0 ;
   if (i) {
      for (int j=1; j<v.p[0]; j++) {
         int c = ((v.p[j] << i) | (carry >> (31-i))) & 0x7fffffff ;
         carry = v.p[j] ;
         v.p[j] = c ;
      }
   }
   shrink(v.p[0]) ;
   return *this ;
}
void bigint::mulpow2(int p) {
   if (p > 0)
      *this <<= p ;
   else if (p < 0)
      *this >>= -p ;
}
int bigint::even() const {
   if (v.i & 1)
      return 1-((v.i >> 1) & 1) ;
   else
      return 1-(v.p[1] & 1) ;
}
int bigint::odd() const {
   if (v.i & 1)
      return ((v.i >> 1) & 1) ;
   else
      return (v.p[1] & 1) ;
}
int bigint::low31() const {
   if (v.i & 1)
      return ((v.i >> 1) & 0x7fffffff) ;
   else
      return v.p[1] ;
}
int bigint::operator==(const bigint &b) const {
   if ((b.v.i - v.i) & 1)
      return 0 ;
   if (v.i & 1)
      return v.i == b.v.i ;
   if (b.v.p[0] != v.p[0])
      return 0 ;
   return memcmp(v.p, b.v.p, sizeof(int) * (v.p[0] + 1)) == 0 ;
}
int bigint::operator!=(const bigint &b) const {
   return !(*this == b) ;
}
int bigint::operator<(const bigint &b) const {
   if (b.v.i & 1)
      if (v.i & 1)
         return v.i < b.v.i ;
      else
         return v.p[v.p[0]] < 0 ;
   else
      if (v.i & 1)
         return b.v.p[b.v.p[0]] >= 0 ;
   int d = v.p[v.p[0]] - b.v.p[b.v.p[0]] ;
   if (d < 0)
      return 1 ;
   if (d > 0)
      return 0 ;
   if (v.p[0] > b.v.p[0])
      return v.p[v.p[0]] < 0 ;
   else if (v.p[0] < b.v.p[0])
      return v.p[v.p[0]] >= 0 ;
   for (int i=v.p[0]; i>0; i--)
      if (v.p[i] < b.v.p[i])
         return 1 ;
      else if (v.p[i] > b.v.p[i])
         return 0 ;
   return 0 ;
}
int bigint::operator<=(const bigint &b) const {
   if (b.v.i & 1)
      if (v.i & 1)
         return v.i <= b.v.i ;
      else
         return v.p[v.p[0]] < 0 ;
   else
      if (v.i & 1)
         return b.v.p[b.v.p[0]] >= 0 ;
   int d = v.p[v.p[0]] - b.v.p[b.v.p[0]] ;
   if (d < 0)
      return 1 ;
   if (d > 0)
      return 0 ;
   if (v.p[0] > b.v.p[0])
      return v.p[v.p[0]] < 0 ;
   else if (v.p[0] < b.v.p[0])
      return v.p[v.p[0]] >= 0 ;
   for (int i=v.p[0]; i>0; i--)
      if (v.p[i] < b.v.p[i])
         return 1 ;
      else if (v.p[i] > b.v.p[i])
         return 0 ;
   return 1 ;
}
int bigint::operator>(const bigint &b) const {
   return !(*this <= b) ;
}
int bigint::operator>=(const bigint &b) const {
   return !(*this < b) ;
}
static double mybpow(int n) {
   double r = 1 ;
   double s = 65536.0 * 32768.0 ;
   while (n) {
      if (n & 1)
         r *= s ;
      s *= s ;
      n >>= 1 ;
   }
   return r ;
}
/**
 *   Turn this bigint into a double.
 */
double bigint::todouble() const {
   if (v.i & 1)
      return (double)(v.i >> 1) ;
   double r = 0 ;
   double m = 1 ;
   int lim = 1 ;
   if (v.p[0] > 4) {
      lim = v.p[0] - 3 ;
      m = mybpow(lim-1) ;
   }
   for (int i=lim; i<=v.p[0]; i++) {
      r = r + m * v.p[i] ;
      m *= 65536 * 32768.0 ;
   }
   return r ;
}
/**
 * Turn this bigint into a double in a way that preserves huge exponents.
 * Here are some examples:
 *
 * To represent
 * the quantity     We return the value
 *  27               1.27
 * -6.02e23         -23.602
 *  6.02e23          23.602
 *  9.99e299         299.999
 *  1.0e300          300.1
 *  1.0e1000         1000.1       This would normally overflow
 *  6.7e12345        12345.67
 */
double bigint::toscinot() const
{
  double mant, exponent;
  double k_1_10 = 0.1;
  double k_1_10000 = 0.0001;
  double k_base = 65536.0 * 32768.0;

  exponent = 0;
  if (v.i & 1) {
    /* small integer */
    mant = (double)(v.i >> 1) ;
  } else {
    /* big integer: a string of 31-bit chunks */
    mant = 0 ;
    double m = 1 ;
    for (int i=1; i<=v.p[0]; i++) {
      mant = mant + m * v.p[i] ;
      m *= k_base ;
      while (m >= 100000.0) {
        m *= k_1_10000;
        mant *= k_1_10000;
        exponent += 4.0;
      }
    }
  }

  /* Add the last few powers of 10 back into the mantissa */
  while(((mant < 0.5) && (mant > -0.5)) && (exponent > 0.0)) {
    mant *= 10.0;
    exponent -= 1.0;
  }

  /* Mantissa might be greater than 1 at this point */
  while((mant >= 1.0) || (mant <= -1.0)) {
    mant *= k_1_10;
    exponent += 1.0;
  }

  if (mant >= 0) {
    // Normal case: 6.02e23 -> 23.602
    return(exponent + mant);
  }
  // Negative case: -6.02e23 -> -23.602
  // In this example mant will be -0.602 and exponent will be 23.0
  return(mant - exponent);
}
/**
 *   Return an int.
 */
int bigint::toint() const {
   if (v.i & 1)
      return (v.i >> 1) ;
   return (v.p[v.p[0]] << 31) | v.p[1] ;
}
/**
 *   How many bits required to represent this, approximately?
 *   Should overestimate but not by too much.
 */
int bigint::bitsreq() const {
   if (v.i & 1)
      return 31 ;
   return v.p[0] * 31 ;
}
/**
 *   Find the lowest bit set.
 */
int bigint::lowbitset() const {
   int r = 0 ;
   if (v.i & 1) {
      if (v.i == 1)
         return -1 ;
      for (int i=1; i<32; i++)
         if ((v.i >> i) & 1)
            return i-1 ;
   }
   int o = 1 ;
   while (v.p[o] == 0) {
      o++ ;
      r += 31 ;
   }
   for (int i=0; i<32; i++)
      if ((v.p[o] >> i) & 1)
         return i + r ;
   return -1 ;
}
/**
 *   Fill a character array with the bits.  Top one is always
 *   the sign bit.
 */
void bigint::tochararr(char *fillme, int n) const {
   int at = 0 ;
   while (n > 0) {
      int w = 0 ;
      if (v.i & 1) {
         if (at == 0)
            w = v.i >> 1 ;
         else
            w = v.i >> 31 ;
      } else {
         if (at < v.p[0])
            w = v.p[at+1] ;
         else
            w = v.p[v.p[0]] ;
      }
      int lim = 31 ;
      if (n < 31)
         lim = n ;
      for (int i=0; i<lim; i++) {
         *fillme++ = (char)(w & 1) ;
         w >>= 1 ;
      }
      n -= 31 ;
      at++ ;
   }
}
/**
 *   Manifests.
 */
const bigint bigint::zero(0) ;
const bigint bigint::one(1) ;
const bigint bigint::two(2) ;
const bigint bigint::three(3) ;
const bigint bigint::maxint(INT_MAX) ;
const bigint bigint::minint(INT_MIN) ;

// most editing operations are limited to absolute coordinates <= 10^9,
// partly because getcell and setcell only take int parameters, but mostly
// to avoid ridiculously long cut/copy/paste/rotate/etc operations
const bigint bigint::min_coord(-1000000000) ;
const bigint bigint::max_coord(+1000000000) ;
