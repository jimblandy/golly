                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2005 Andrew Trevorrow and Tomas Rokicki.

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
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "readpattern.h"
#include "util.h"
#include "viewport.h"
#include "liferender.h"
#include "writepattern.h"
#include <stdlib.h>
#include <iostream>
#include <cstdio>
using namespace std ;
viewport viewport(1000, 1000) ;
/*
 *   This is a "renderer" that is just stubs, for performance testing.
 */
class nullrender : public liferender {
public:
   nullrender() {}
   virtual ~nullrender() {}
   virtual void killrect(int, int, int, int) {}
   virtual void blit(int, int, int, int, int*, int=1) {}
} ;
nullrender renderer ;
char *filename ;
lifealgo *imp = 0 ;
struct options {
  char *shortopt ;
  char *longopt ;
  char *desc ;
  char opttype ;
  void *data ;
} ;
bigint maxgen = -1, inc = 0 ;
int maxmem = 256 ;
int hyper, hashlife, render, autofit, quiet, popcount ;
int stepthresh, stepfactor ;
char *liferule = 0 ;
char *outfilename = 0 ;
char *renderscale = "1" ;
int outputgzip, outputismc ;
int numberoffset ; // where to insert file name numbers
options options[] = {
  { "-m", "--generation", "How far to run", 'I', &maxgen },
  { "-i", "--stepsize", "Step size", 'I', &inc },
  { "-M", "--maxmemory", "Max memory to use in megabytes", 'i', &maxmem },
  { "-2", "--exponential", "Use exponentially increasing steps", 'b', &hyper },
  { "-q", "--quiet", "Don't show population; twice, don't show anything", 'b', &quiet },
  { "-r", "--rule", "Life rule to use", 's', &liferule },
  { "-h", "--hashlife", "Use Hashlife algorithm", 'b', &hashlife },
  { "-o", "--output", "Output file (*.rle, *.mc, *.rle.gz, *.mc.gz)", 's',
                                                               &outfilename },
  { "",   "--render", "Render (benchmarking)", 'b', &render },
  { "",   "--popcount", "Popcount (benchmarking)", 'b', &popcount },
  { "",   "--scale", "Rendering scale", 's', &renderscale },
//{ "",   "--stepthreshold", "Stepsize >= gencount/this (default 1)",
//                                                          'i', &stepthresh },
//{ "",   "--stepfactor", "How much to scale step by (default 2)",
//                                                        'i', &stepfactor },
  { "",   "--autofit", "Autofit before each render", 'b', &autofit },
  { 0, 0, 0, 0, 0 }
} ;
int endswith(const char *s, const char *suff) {
   int off = strlen(s) - strlen(suff) ;
   if (off <= 0)
      return 0 ;
   s += off ;
   while (*s)
      if (tolower(*s++) != tolower(*suff++))
         return 0 ;
   numberoffset = off ;
   return 1 ;
}
void usage(const char *s) {
  fprintf(stderr, "Usage:  bgolly [options] patternfile\n") ;
  for (int i=0; options[i].shortopt; i++)
    fprintf(stderr, "%3s %-15s %s\n", options[i].shortopt, options[i].longopt,
	    options[i].desc) ;
  if (s)
    lifefatal(s) ;
  exit(0) ;
}
#define STRINGIFY(ARG) STR2(ARG)
#define STR2(ARG) #ARG
#define MAXRLE 1000000000
void writepat(int fc) {
   char *thisfilename = outfilename ;
   char tmpfilename[256] ;
   if (fc >= 0) {
      strcpy(tmpfilename, outfilename) ;
      char *p = tmpfilename + numberoffset ;
      *p++ = '-' ;
      sprintf(p, "%d", fc) ;
      p += strlen(p) ;
      strcpy(p, outfilename + numberoffset) ;
      thisfilename = tmpfilename ;
   }
   cerr << "(->" << thisfilename << flush ;
   const char *err ;
   if (outputismc) {
      FILE *f = fopen(thisfilename, "w") ;
      if (f == 0)
         lifefatal("Cannot open file for writing") ;
      err = imp->writeNativeFormat(f) ;
      fclose(f) ;
   } else {
      bigint t, l, b, r ;
      imp->findedges(&t, &l, &b, &r) ;
      if (t < -MAXRLE || l < -MAXRLE || b > MAXRLE || r > MAXRLE)
         lifefatal("Pattern too large to write in RLE format") ;
      err = writepattern(thisfilename, *imp, RLE_format,
                         t.toint(), l.toint(), b.toint(), r.toint()) ;
   }
   if (err != 0)
      lifewarning(err) ;
   cerr << ")" << flush ;
}
int main(int argc, char *argv[]) {
   cout << 
    "This is bgolly " STRINGIFY(VERSION) " Copyright 2005 The Golly Gang."
                                                            << endl << flush ;
   while (argc > 1 && argv[1][0] == '-') {
      argc-- ;
      argv++ ;
      char *opt = argv[0] ;
      int hit = 0 ;
      for (int i=0; options[i].shortopt; i++) {
	if (strcmp(opt, options[i].shortopt) == 0 ||
	    strcmp(opt, options[i].longopt) == 0) {
	  switch (options[i].opttype) {
case 'i':
             if (argc < 2)
	        lifefatal("Bad option argument") ;
             *(int *)options[i].data = atol(argv[1]) ;
	     argc-- ;
	     argv++ ;
             break ;
case 'I':
             if (argc < 2)
	        lifefatal("Bad option argument") ;
             *(bigint *)options[i].data = bigint(argv[1]) ;
	     argc-- ;
	     argv++ ;
             break ;
case 'b':
             (*(int *)options[i].data) += 1 ;
             break ;
case 's':
             if (argc < 2)
	        lifefatal("Bad option argument") ;
             *(char **)options[i].data = argv[1] ;
	     argc-- ;
	     argv++ ;
             break ;
	  }
	  hit++ ;
	  break ;
	}
      }
      if (!hit)
	usage("Bad option given") ;
   }
   if (argc < 2)
     usage("No pattern argument given") ;
   if (argc > 2)
     usage("Extra stuff after pattern argument") ;
   if (outfilename) {
      if (endswith(outfilename, ".rle")) {
      } else if (endswith(outfilename, ".mc")) {
         outputismc = 1 ;
#ifdef ZLIB
      } else if (endswith(outfilename, ".rle.gz")) {
         outputgzip = 1 ;
      } else if (endswith(outfilename, ".mc.gz")) {
         outputismc = 1 ;
         outputgzip = 1 ;
#endif
      } else {
         lifefatal("Output filename must end with .rle or .mc.") ;
      }
      if (outputgzip)
         lifefatal("Gzipped output files not supported yet") ;
      if (outputismc && !hashlife)
         lifefatal("Cannot write MC files if not using hash") ;
      if (strlen(outfilename) > 200)
         lifefatal("Output filename too long") ;
   }
   if (hashlife)
     imp = new hlifealgo() ;
   else
     imp = new qlifealgo() ;
   imp->setMaxMemory(maxmem) ;
   filename = argv[1] ;
   const char *err = readpattern(argv[1], *imp) ;
   if (err)
      lifefatal(err) ;
   if (liferule)
      imp->setrule(liferule) ;
   if (inc != 0)
      imp->setIncrement(inc) ;
   int fc = 0 ;
   for (;;) {
      if (quiet < 2) {
         cout << imp->getGeneration().tostring() ;
         if (!quiet)
	   cout << ": " << imp->getPopulation().tostring() << endl ;
         else
	   cout << endl ;
      }
      if (popcount)
         imp->getPopulation() ;
      if (autofit)
	imp->fit(viewport, 1) ;
      if (render)
	imp->draw(viewport, renderer) ;
      if (maxgen >= 0 && imp->getGeneration() >= maxgen)
         break ;
      if (!hyper && maxgen > 0 && inc == 0) {
         bigint diff = maxgen ;
         diff -= imp->getGeneration() ;
         int bs = diff.lowbitset() ;
         diff = 1 ;
         diff <<= bs ;
         imp->setIncrement(diff) ;
      }
      imp->step() ;
      if (maxgen < 0 && outfilename != 0)
         writepat(fc++) ;
      if (hyper)
         imp->setIncrement(imp->getGeneration()) ;
   }
   if (maxgen >= 0 && outfilename != 0)
      writepat(-1) ;
   exit(0) ;
}
