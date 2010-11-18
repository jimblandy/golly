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
#include "writepattern.h"
#include "lifealgo.h"
#include "util.h"          // for *progress calls
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef __APPLE__
#define BUFFSIZE 4096   // 4K is best for Mac OS X
#else
#define BUFFSIZE 8192   // 8K is best for Windows and other platforms???
#endif

// globals for writing RLE files
static char outbuff[BUFFSIZE];
static size_t outpos;            // current write position in outbuff
static double currsize;          // current file size (for showing in progress dialog)
static bool badwrite;            // fwrite failed?

// using buffered putchar instead of fputc is about 20% faster on Mac OS X
void putchar(char ch, FILE *f) {
   if (badwrite) return;
   if (outpos == BUFFSIZE) {
      if (fwrite(outbuff, 1, outpos, f) != outpos) badwrite = true;
      outpos = 0;
      currsize += BUFFSIZE;
   }
   outbuff[outpos] = ch;
   outpos++;
}

const int WRLE_NONE = -3 ;
const int WRLE_EOP = -2 ;
const int WRLE_NEWLINE = -1 ;

// output of RLE pattern data is channelled thru here to make it easier to
// ensure all lines have <= 70 characters
void AddRun(FILE *f,
            int state,                // in: state of cell to write
            int multistate,           // true if #cell states > 2
            unsigned int &run,        // in and out
            unsigned int &linelen)    // ditto
{
   unsigned int i, numlen;
   char numstr[32];

   if ( run > 1 ) {
      sprintf(numstr, "%u", run);
      numlen = strlen(numstr);
   } else {
      numlen = 0;                      // no run count shown if 1
   }
   if ( linelen + numlen + 1 + multistate > 70 ) {
      putchar('\n', f);
      linelen = 0;
   }
   i = 0;
   while (i < numlen) {
      putchar(numstr[i], f);
      i++;
   }
   if (multistate) {
      if (state <= 0)
         putchar(".$!"[-state], f) ;
      else {
         if (state > 24) {
            int hi = (state - 25) / 24 ;
            putchar((char)(hi + 'p'), f) ;
            linelen++ ;
            state -= (hi + 1) * 24 ;
         }
         putchar((char)('A' + state - 1), f) ;
      }
   } else
      putchar("!$bo"[state+2], f) ;
   linelen += numlen + 1;
   run = 0;                           // reset run count
}

// write current pattern to file using extended RLE format
const char *writerle(FILE *f, char *comments, lifealgo &imp,
                     int top, int left, int bottom, int right,
                     bool xrle)
{
   badwrite = false;
   if (xrle) {
      // write out #CXRLE line; note that the XRLE indicator is prefixed
      // with #C so apps like Life32 and MCell will ignore the line
      fprintf(f, "#CXRLE Pos=%d,%d", left, top);
      if (imp.getGeneration() > bigint::zero)
         fprintf(f, " Gen=%s", imp.getGeneration().tostring('\0'));
      fputs("\n", f);
   }

   char *endcomms = NULL;
   if (comments && comments[0]) {
      // write given comment line(s) -- can't just do fputs(comments,f)
      // because comments might include arbitrary text after the "!"
      char *p = comments;
      while (*p == '#') {
         while (*p != '\n') p++;
         p++;
      }
      if (p != comments) {
         char savech = *p;
         *p = '\0';
         fputs(comments, f);
         *p = savech;
      }
      // any comment lines not starting with # will be written after "!"
      if (*p != '\0') endcomms = p;
   }

   if ( imp.isEmpty() || top > bottom || left > right ) {
      // empty pattern
      fprintf(f, "x = 0, y = 0, rule = %s\n!\n", imp.getrule());
   } else {
      // do header line
      unsigned int wd = right - left + 1;
      unsigned int ht = bottom - top + 1;
      sprintf(outbuff, "x = %u, y = %u, rule = %s\n", wd, ht, imp.getrule());
      outpos = strlen(outbuff);

      // do RLE data
      unsigned int linelen = 0;
      unsigned int brun = 0;
      unsigned int orun = 0;
      unsigned int dollrun = 0;
      int laststate = WRLE_NONE ;
      int multistate = imp.NumCellStates() > 2 ;
      int cx, cy;
      
      // for showing accurate progress we need to add pattern height to pop count
      // in case this is a huge pattern with many blank rows
      double maxcount = imp.getPopulation().todouble() + ht;
      double accumcount = 0;
      int currcount = 0;
      int v = 0 ;
      for ( cy=top; cy<=bottom; cy++ ) {
         // set lastchar to anything except 'o' or 'b'
         laststate = WRLE_NONE ;
         currcount++;
         for ( cx=left; cx<=right; cx++ ) {
            int skip = imp.nextcell(cx, cy, v);
            if (skip + cx > right)
               skip = -1;           // pretend we found no more live cells
            if (skip > 0) {
               // have exactly "skip" dead cells here
               if (laststate == 0) {
                  brun += skip;
               } else {
                  if (orun > 0) {
                     // output current run of live cells
                     AddRun(f, laststate, multistate, orun, linelen);
                  }
                  laststate = 0 ;
                  brun = skip;
               }
            }
            if (skip >= 0) {
               // found next live cell in this row
               cx += skip;
               if (laststate == v) {
                  orun++;
               } else {
                  if (dollrun > 0)
                     // output current run of $ chars
                     AddRun(f, WRLE_NEWLINE, multistate, dollrun, linelen);
                  if (brun > 0)
                     // output current run of dead cells
                     AddRun(f, 0, multistate, brun, linelen);
                  if (orun > 0)
                     AddRun(f, laststate, multistate, orun, linelen) ;
                  laststate = v ;
                  orun = 1;
               }
               currcount++;
            } else {
               cx = right + 1;  // done this row
            }
            if (currcount > 1024) {
               char msg[128];
               accumcount += currcount;
               currcount = 0;
               sprintf(msg, "File size: %.2f MB", currsize / 1048576.0);
               if (lifeabortprogress(accumcount / maxcount, msg)) break;
            }
         }
         // end of current row
         if (isaborted()) break;
         if (laststate == 0)
            // forget dead cells at end of row
            brun = 0;
         else if (laststate >= 0)
            // output current run of live cells
            AddRun(f, laststate, multistate, orun, linelen);
         dollrun++;
      }
      
      // terminate RLE data
      dollrun = 1;
      AddRun(f, WRLE_EOP, multistate, dollrun, linelen);
      putchar('\n', f);
      
      // flush outbuff
      if (outpos > 0 && !badwrite && fwrite(outbuff, 1, outpos, f) != outpos)
         badwrite = true;
   }
   
   if (endcomms) fputs(endcomms, f);
   
   if (badwrite)
      return "Failed to write output buffer!";
   else
      return 0;
}

const char *writelife105(FILE *f, char *comments, lifealgo &imp)
{
   fputs("#Life 1.05\n", f);
   fprintf(f, "#R %s\n", imp.getrule());
   if (comments && comments[0]) {
      // write given comment line(s)
      fputs(comments, f);
   }
   return "Not yet implemented.";
}

const char *writemacrocell(FILE *f, char *comments, lifealgo &imp)
{
   if (imp.hyperCapable())
      return imp.writeNativeFormat(f, comments);
   else
      return "Not yet implemented.";
}

const char *writepattern(const char *filename, lifealgo &imp, pattern_format format,
                         int top, int left, int bottom, int right)
{
   FILE *f;
   
   // extract any comments if file exists so we can copy them to new file
   char *commptr = NULL;
   f = fopen(filename, "r");
   if (f) {
      fclose(f);
      const char *err = readcomments(filename, &commptr);
      if (err) {
         if (commptr) free(commptr);
         return err;
      }
   }
   
   f = fopen(filename, "w");
   if (f == 0) {
      if (commptr) free(commptr);
      return "Can't create pattern file!";
   }

   // skip past any old #CXRLE lines at start of existing XRLE file
   char *comments = commptr;
   if (comments) {
      while (strncmp(comments, "#CXRLE", 6) == 0) {
         while (*comments != '\n') comments++;
         comments++;
      }
   }

   currsize = 0.0;
   lifebeginprogress("Writing pattern file");

   const char *errmsg = NULL;
   switch (format) {
      case RLE_format:
         errmsg = writerle(f, comments, imp, top, left, bottom, right, false);
         break;

      case XRLE_format:
         errmsg = writerle(f, comments, imp, top, left, bottom, right, true);
         break;

      case L105_format:
         // Life 1.05 format ignores given edges
         errmsg = writelife105(f, comments, imp);
         break;

      case MC_format:
         // macrocell format ignores given edges
         errmsg = writemacrocell(f, comments, imp);
         break;

      default:
         errmsg = "Unsupported pattern format!";
   }
   
   if (errmsg == NULL && ferror(f))
      errmsg = "Error occurred writing file; maybe disk is full?";
   
   lifeendprogress();
   fclose(f);
   
   if (commptr) free(commptr);
   
   if (isaborted())
      return "File contains truncated pattern.";
   else
      return errmsg;
}
