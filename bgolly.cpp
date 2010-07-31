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
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "jvnalgo.h"
#include "ruletable_algo.h"
#include "ruletreealgo.h"
#include "generationsalgo.h"
#include "readpattern.h"
#include "util.h"
#include "viewport.h"
#include "liferender.h"
#include "writepattern.h"
#include <stdlib.h>
#include <iostream>
#include <cstdio>
#include <string.h>
#include <cstdlib>
#ifdef TIMING
#include <sys/time.h>
#endif
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
   virtual void pixblit(int, int, int, int, char*, int) {}
   virtual void getcolors(unsigned char** r, unsigned char** g, unsigned char** b) {
      static unsigned char dummy[256];
      *r = *g = *b = dummy;
   }
} ;

nullrender renderer ;
/*
 *   This is a "lifeerrors" that we can use to test some edge
 *   conditions.  In this case the only real thing we use
 *   it for is to check rendering during a progress dialog.
 */
class nullerrors : public lifeerrors {
public:
   nullerrors() {}
   virtual void fatal(const char *) {}
   virtual void warning(const char *) {}
   virtual void status(const char *) {}
   virtual void beginprogress(const char *s) { abortprogress(0, s) ; }
   virtual bool abortprogress(double, const char *) ;
   virtual void endprogress() { abortprogress(1, "") ; }
   virtual const char* getuserrules() { return "" ; }
   virtual const char* getrulesdir() { return "Rules/" ; }
} ;
nullerrors nullerror ;
#ifdef TIMING
double start ;
double timestamp() {
   struct timeval tv ;
   gettimeofday(&tv, 0) ;
   double now = tv.tv_sec + 0.000001 * tv.tv_usec ;
   double r = now - start ;
   start = now ;
   return r ;
}
#endif
/*
 *   This is a "lifeerrors" that we can use to test some edge
 *   conditions.  In this case the only real thing we use
 *   it for is to check rendering during a progress dialog.
 */
class verbosestatus : public lifeerrors {
public:
   verbosestatus() {}
   virtual void fatal(const char *) {}
   virtual void warning(const char *) {}
   virtual void status(const char *s) {
#ifdef TIMING
      cout << timestamp() << " " << s << endl ;
#endif
   }
   virtual void beginprogress(const char *) {}
   virtual bool abortprogress(double, const char *) { return 0 ; }
   virtual void endprogress() {}
   virtual const char* getuserrules() { return "" ; }
   virtual const char* getrulesdir() { return "Rules/" ; }
} ;
verbosestatus verbosestatus_instance ;
char *filename ;
lifealgo *imp = 0 ;
struct options {
  const char *shortopt ;
  const char *longopt ;
  const char *desc ;
  char opttype ;
  void *data ;
} ;
bigint maxgen = -1, inc = 0 ;
int maxmem = 256 ;
int hyper, render, autofit, quiet, popcount, progress ;
int hashlife ;
char *algoName = 0 ;
int verbose ;
int timeline ;
int stepthresh, stepfactor ;
char *liferule = 0 ;
char *outfilename = 0 ;
char *renderscale = (char *)"1" ;
char *testscript = 0 ;
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
  { "-a", "--algorithm", "Select algorithm by name", 's', &algoName },
  { "-o", "--output", "Output file (*.rle, *.mc, *.rle.gz, *.mc.gz)", 's',
                                                               &outfilename },
  { "-v", "--verbose", "Verbose", 'b', &verbose },
  { "-t", "--timeline", "Use timeline", 'b', &timeline },
  { "",   "--render", "Render (benchmarking)", 'b', &render },
  { "",   "--progress", "Render during progress dialog (debugging)", 'b', &progress },
  { "",   "--popcount", "Popcount (benchmarking)", 'b', &popcount },
  { "",   "--scale", "Rendering scale", 's', &renderscale },
//{ "",   "--stepthreshold", "Stepsize >= gencount/this (default 1)",
//                                                          'i', &stepthresh },
//{ "",   "--stepfactor", "How much to scale step by (default 2)",
//                                                        'i', &stepfactor },
  { "",   "--autofit", "Autofit before each render", 'b', &autofit },
  { "",   "--exec", "Run testing script", 's', &testscript },
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
      err = imp->writeNativeFormat(f, NULL) ;   // NULL means no comments
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
const int MAXCMDLENGTH = 2048 ;
struct cmdbase {
   cmdbase(const char *cmdarg, const char *argsarg) {
      verb = cmdarg ;
      args = argsarg ;
      next = list ;
      list = this ;
   }
   const char *verb ;
   const char *args ;
   int iargs[4] ;
   char *sarg ;
   bigint barg ;
   virtual void doit() {}
   // for convenience, we put the generic loop here that takes a
   // 4x bounding box and runs getnext on all y values until
   // they are done.  Input is assumed to be a bounding box in the
   // form minx miny maxx maxy
   void runnextloop() {
      int minx = iargs[0] ;
      int miny = iargs[1] ;
      int maxx = iargs[2] ;
      int maxy = iargs[3] ;
      int v ;
      for (int y=miny; y<=maxy; y++) {
         for (int x=minx; x<=maxx; x++) {
            int dx = imp->nextcell(x, y, v) ;
            if (dx < 0)
               break ;
            if (x > 0 && (x + dx) < 0)
               break ;
            x += dx ;
            if (x > maxx)
               break ;
            nextloopinner(x, y) ;
         }
      }
   }
   virtual void nextloopinner(int, int) {}
   int parseargs(const char *cmdargs) {
      int iargn = 0 ;
      char sbuf[MAXCMDLENGTH+2] ;
      for (const char *rargs = args; *rargs; rargs++) {
         while (*cmdargs && *cmdargs <= ' ')
            cmdargs++ ;
         if (*cmdargs == 0) {
            lifewarning("Missing needed argument") ;
            return 0 ;
         }
         switch (*rargs) {
         case 'i':
           if (sscanf(cmdargs, "%d", iargs+iargn) != 1) {
             lifewarning("Missing needed integer argument") ;
             return 0 ;
           }
           iargn++ ;
           break ;
         case 'b':
           {
              int i = 0 ;
              for (i=0; cmdargs[i] > ' '; i++)
                 sbuf[i] = cmdargs[i] ;
              sbuf[i] = 0 ;
              barg = bigint(sbuf) ;
           }
           break ;
         case 's':
           if (sscanf(cmdargs, "%s", sbuf) != 1) {
             lifewarning("Missing needed string argument") ;
             return 0 ;
           }
           sarg = strdup(sbuf) ;
           break ;
         default:
           lifefatal("Internal error in parseargs") ;
         }
         while (*cmdargs && *cmdargs > ' ')
           cmdargs++ ;
      }
      return 1 ;
   }
   static void docmd(const char *cmdline) {
      for (cmdbase *cmd=list; cmd; cmd = cmd->next)
         if (strncmp(cmdline, cmd->verb, strlen(cmd->verb)) == 0 &&
             cmdline[strlen(cmd->verb)] <= ' ') {
            if (cmd->parseargs(cmdline+strlen(cmd->verb))) {
               cmd->doit() ;
            }
            return ;
         }
      lifewarning("Didn't understand command") ;
   }
   cmdbase *next ;
   virtual ~cmdbase() {}
   static cmdbase *list ;
} ;
cmdbase *cmdbase::list = 0 ;
struct loadcmd : public cmdbase {
   loadcmd() : cmdbase("load", "s") {}
   virtual void doit() {
     const char *err = readpattern(sarg, *imp) ;
     if (err != 0)
       lifewarning(err) ;
   }
} load_inst ;
struct stepcmd : public cmdbase {
   stepcmd() : cmdbase("step", "b") {}
   virtual void doit() {
      imp->setIncrement(barg) ;
      imp->step() ;
      cout << imp->getGeneration().tostring() << ": " ;
      cout << imp->getPopulation().tostring() << endl ;
   }
} step_inst ;
struct showcmd : public cmdbase {
   showcmd() : cmdbase("show", "") {}
   virtual void doit() {
      cout << imp->getGeneration().tostring() << ": " ;
      cout << imp->getPopulation().tostring() << endl ;
   }
} show_inst ;
struct quitcmd : public cmdbase {
   quitcmd() : cmdbase("quit", "") {}
   virtual void doit() {
      cout << "Buh-bye!" << endl ;
      exit(10) ;
   }
} quit_inst ;
struct setcmd : public cmdbase {
   setcmd() : cmdbase("set", "ii") {}
   virtual void doit() {
      imp->setcell(iargs[0], iargs[1], 1) ;
   }
} set_inst ;
struct unsetcmd : public cmdbase {
   unsetcmd() : cmdbase("unset", "ii") {}
   virtual void doit() {
      imp->setcell(iargs[0], iargs[1], 0) ;
   }
} unset_inst ;
struct helpcmd : public cmdbase {
   helpcmd() : cmdbase("help", "") {}
   virtual void doit() {
      for (cmdbase *cmd=list; cmd; cmd = cmd->next)
         cout << cmd->verb << " " << cmd->args << endl ;
   }
} help_inst ;
struct getcmd : public cmdbase {
   getcmd() : cmdbase("get", "ii") {}
   virtual void doit() {
     cout << "At " << iargs[0] << "," << iargs[1] << " -> " <<
        imp->getcell(iargs[0], iargs[1]) << endl ;
   }
} get_inst ;
struct getnextcmd : public cmdbase {
   getnextcmd() : cmdbase("getnext", "ii") {}
   virtual void doit() {
     int v ;
     cout << "At " << iargs[0] << "," << iargs[1] << " next is " <<
        imp->nextcell(iargs[0], iargs[1], v) << endl ;
   }
} getnext_inst ;
vector<pair<int, int> > cutbuf ;
struct copycmd : public cmdbase {
   copycmd() : cmdbase("copy", "iiii") {}
   virtual void nextloopinner(int x, int y) {
      cutbuf.push_back(make_pair(x-iargs[0], y-iargs[1])) ;
   }
   virtual void doit() {
      cutbuf.clear() ;
      runnextloop() ;
      cout << cutbuf.size() << " pixels copied." << endl ;
   }
} copy_inst ;
struct cutcmd : public cmdbase {
   cutcmd() : cmdbase("cut", "iiii") {}
   virtual void nextloopinner(int x, int y) {
      cutbuf.push_back(make_pair(x-iargs[0], y-iargs[1])) ;
      imp->setcell(x, y, 0) ;
   }
   virtual void doit() {
      cutbuf.clear() ;
      runnextloop() ;
      cout << cutbuf.size() << " pixels cut." << endl ;
   }
} cut_inst ;
// this paste only sets cells, never clears cells
struct pastecmd : public cmdbase {
   pastecmd() : cmdbase("paste", "ii") {}
   virtual void doit() {
      for (unsigned int i=0; i<cutbuf.size(); i++)
         imp->setcell(cutbuf[i].first, cutbuf[i].second, 1) ;
      cout << cutbuf.size() << " pixels pasted." << endl ;
   }
} paste_inst ;
struct showcutcmd : public cmdbase {
   showcutcmd() : cmdbase("showcut", "") {}
   virtual void doit() {
      for (unsigned int i=0; i<cutbuf.size(); i++)
         cout << cutbuf[i].first << " " << cutbuf[i].second << endl ;
   }
} showcut_inst ;
lifealgo *createUniverse() {
   if (algoName == 0) {
     if (hashlife)
       algoName = (char *)"HashLife" ;
     else
       algoName = (char *)"QuickLife" ;
   }
   staticAlgoInfo *ai = staticAlgoInfo::byName(algoName) ;
   if (ai == 0)
      lifefatal("No such algorithm") ;
   lifealgo *imp = (ai->creator)() ;
   if (imp == 0)
      lifefatal("Could not create universe") ;
   imp->setMaxMemory(maxmem) ;
   return imp ;
}
struct newcmd : public cmdbase {
   newcmd() : cmdbase("new", "") {}
   virtual void doit() {
     if (imp != 0)
        delete imp ;
     imp = createUniverse() ;
   }
} new_inst ;
struct sethashingcmd : public cmdbase {
   sethashingcmd() : cmdbase("sethashing", "i") {}
   virtual void doit() {
      hashlife = iargs[0] ;
   }
} sethashing_inst ;
struct setmaxmemcmd : public cmdbase {
   setmaxmemcmd() : cmdbase("setmaxmem", "i") {}
   virtual void doit() {
      maxmem = iargs[0] ;
   }
} setmaxmem_inst ;
struct setalgocmd : public cmdbase {
   setalgocmd() : cmdbase("setalgo", "s") {}
   virtual void doit() {
      algoName = sarg ;
   }
} setalgocmd_inst ;
struct edgescmd : public cmdbase {
   edgescmd() : cmdbase("edges", "") {}
   virtual void doit() {
      bigint t, l, b, r ;
      imp->findedges(&t, &l, &b, &r) ;
      cout << "Bounding box " << l.tostring() ;
      cout << " " << t.tostring() ;
      cout << " .. " << r.tostring() ;
      cout << " " << b.tostring() << endl ;
   }
} edges_inst ;
bool nullerrors::abortprogress(double, const char *) {
   imp->draw(viewport, renderer) ;
   return 0 ;
}
void runtestscript(const char *testscript) {
   FILE *cmdfile = 0 ;
   if (strcmp(testscript, "-") != 0)
      cmdfile = fopen(testscript, "r") ;
   else
      cmdfile = stdin ;
   char cmdline[MAXCMDLENGTH + 10] ;
   if (cmdfile == 0)
      lifefatal("Cannot open testscript") ;
   for (;;) {
     cerr << flush ;
     if (cmdfile == stdin)
       cout << "bgolly> " << flush ;
     else
       cout << flush ;
     if (fgets(cmdline, MAXCMDLENGTH, cmdfile) == 0)
        break ;
     cmdbase::docmd(cmdline) ;
   }
   exit(0) ;
}
int main(int argc, char *argv[]) {
   cout << 
    "This is bgolly " STRINGIFY(VERSION) " Copyright 2010 The Golly Gang."
                                                            << endl << flush ;
   qlifealgo::doInitializeAlgoInfo(staticAlgoInfo::tick()) ;
   hlifealgo::doInitializeAlgoInfo(staticAlgoInfo::tick()) ;
   jvnalgo::doInitializeAlgoInfo(staticAlgoInfo::tick()) ;
   generationsalgo::doInitializeAlgoInfo(staticAlgoInfo::tick()) ;
   ruletable_algo::doInitializeAlgoInfo(staticAlgoInfo::tick()) ;
   ruletreealgo::doInitializeAlgoInfo(staticAlgoInfo::tick()) ;
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
   if (argc < 2 && !testscript)
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
      if (strlen(outfilename) > 200)
         lifefatal("Output filename too long") ;
   }
   if (timeline && hyper)
      lifefatal("Cannot use both timeline and hyperthreading") ;
   imp = createUniverse() ;
   if (progress)
     lifeerrors::seterrorhandler(&nullerror) ;
   else if (verbose) {
     lifeerrors::seterrorhandler(&verbosestatus_instance) ;
     hlifealgo::setVerbose(1) ;
   }
   imp->setMaxMemory(maxmem) ;
#ifdef TIMING
   timestamp() ;
#endif
   if (testscript) {
     if (argc > 1) {
       filename = argv[1] ;
       const char *err = readpattern(argv[1], *imp) ;
       if (err)
         lifefatal(err) ;
     }
     runtestscript(testscript) ;
   }
   filename = argv[1] ;
   const char *err = readpattern(argv[1], *imp) ;
   if (err)
      lifefatal(err) ;
   if (liferule)
      imp->setrule(liferule) ;
   if (inc != 0)
      imp->setIncrement(inc) ;
   if (timeline) {
      int lowbit = inc.lowbitset() ;
      bigint t = 1 ;
      for (int i=0; i<lowbit; i++)
         t.mul_smallint(2) ;
      if (t != inc)
         lifefatal("Bad increment for timeline") ;
      imp->startrecording(2, lowbit) ;
   }
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
      if (timeline && imp->getframecount() + 2 > MAX_FRAME_COUNT)
         imp->pruneframes() ;
      if (hyper)
         imp->setIncrement(imp->getGeneration()) ;
   }
   if (maxgen >= 0 && outfilename != 0)
      writepat(-1) ;
   exit(0) ;
}
