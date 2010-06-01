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
#if defined(__LP64__) || defined(__amd64__)
   #define __STDC_FORMAT_MACROS
   #define __STDC_LIMIT_MACROS
   #include <inttypes.h>
   #include <stdint.h>
   typedef uintptr_t g_uintptr_t ;
   #define G_MAX SIZE_MAX
#else
   #define PRIuPTR "u"
   typedef unsigned int g_uintptr_t ;
   #define G_MAX UINT_MAX
#endif
