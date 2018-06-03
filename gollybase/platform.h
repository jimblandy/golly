// This file is part of Golly.
// See docs/License.html for the copyright notice.

/**
 *   This file holds things that are platform-dependent such as
 *   dependencies on 64-bitness, help for when the platform does not
 *   have inttypes.h, and things like that.
 *
 *   We need to convert pointers to integers and back for two
 *   reasons.  First, hlife structures use the address of a node
 *   as its label, and we need to hash these labels.  Secondly,
 *   we use bit tricks for garbage collection and need to set and
 *   clear low-order bits in a pointer.  Normally the typedef
 *   below is all we need, but if your platform doesn't have
 *   uintptr_t you can change that here.  We also need this type
 *   for anything that might hold the *count* of nodes, since
 *   this might be larger than an int.  If inttypes does not
 *   exist, and you're compiling for a 64-bit platform, you may
 *   need to make some changes here.
 */
#include <limits.h>
#if defined(_WIN64)
   #define PRIuPTR "I64u"
   typedef uintptr_t g_uintptr_t ;
   #define G_MAX SIZE_MAX
   #define GOLLY64BIT (1)
#elif defined(__LP64__) || defined(__amd64__)
   #define __STDC_FORMAT_MACROS
   #define __STDC_LIMIT_MACROS
   #include <inttypes.h>
   #include <stdint.h>
   typedef uintptr_t g_uintptr_t ;
   #define G_MAX SIZE_MAX
   #define GOLLY64BIT (1)
#else
   #define PRIuPTR "u"
   typedef unsigned int g_uintptr_t ;
   #define G_MAX UINT_MAX
   #undef GOLLY64BIT
#endif
#define USEPREFETCH (1)
// note that _WIN32 is also defined when compiling for 64-bit Windows
#ifdef _WIN32
#include <mmintrin.h>
#include <xmmintrin.h>
#define PREFETCH(a) _mm_prefetch((const char *)a, _MM_HINT_T0)
#else
#define PREFETCH(a) __builtin_prefetch(a)
#endif
