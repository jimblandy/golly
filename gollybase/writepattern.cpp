// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "writepattern.h"
#include "lifealgo.h"
#include "util.h"          // for *progress calls
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#ifdef ZLIB
#include <zlib.h>
#include <streambuf>
#endif

#ifdef __APPLE__
#define BUFFSIZE 4096   // 4K is best for Mac OS X
#else
#define BUFFSIZE 8192   // 8K is best for Windows and other platforms???
#endif

// globals for writing RLE files
static char outbuff[BUFFSIZE];
static size_t outpos;            // current write position in outbuff
static bool badwrite;            // fwrite failed?

// using buffered putchar instead of fputc is about 20% faster on Mac OS X
static void putchar(char ch, std::ostream &os) {
   if (badwrite) return;
   if (outpos == BUFFSIZE) {
      if (!os.write(outbuff, outpos)) badwrite = true;
      outpos = 0;
   }
   outbuff[outpos] = ch;
   outpos++;
}

const int WRLE_NONE = -3 ;
const int WRLE_EOP = -2 ;
const int WRLE_NEWLINE = -1 ;

// output of RLE pattern data is channelled thru here to make it easier to
// ensure all lines have <= 70 characters
void AddRun(std::ostream &f,
            int state,                // in: state of cell to write
            int multistate,           // true if #cell states > 2
            unsigned int &run,        // in and out
            unsigned int &linelen)    // ditto
{
   unsigned int i, numlen;
   char numstr[32];

   if ( run > 1 ) {
      sprintf(numstr, "%u", run);
      numlen = (int)strlen(numstr);
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
const char *writerle(std::ostream &os, char *comments, lifealgo &imp,
                     int top, int left, int bottom, int right,
                     bool xrle)
{
   badwrite = false;
   if (xrle) {
      // write out #CXRLE line; note that the XRLE indicator is prefixed
      // with #C so apps like Life32 and MCell will ignore the line
      os << "#CXRLE Pos=" << left << ',' << top;
      if (imp.getGeneration() > bigint::zero)
         os << " Gen=" << imp.getGeneration().tostring('\0');
      os << '\n';
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
         os << comments;
         *p = savech;
      }
      // any comment lines not starting with # will be written after "!"
      if (*p != '\0') endcomms = p;
   }

   if ( imp.isEmpty() || top > bottom || left > right ) {
      // empty pattern
      os << "x = 0, y = 0, rule = " << imp.getrule() << "\n!\n";
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
                     AddRun(os, laststate, multistate, orun, linelen);
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
                     AddRun(os, WRLE_NEWLINE, multistate, dollrun, linelen);
                  if (brun > 0)
                     // output current run of dead cells
                     AddRun(os, 0, multistate, brun, linelen);
                  if (orun > 0)
                     AddRun(os, laststate, multistate, orun, linelen) ;
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
               sprintf(msg, "File size: %.2f MB", os.tellp() / 1048576.0);
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
            AddRun(os, laststate, multistate, orun, linelen);
         dollrun++;
      }
      
      // terminate RLE data
      dollrun = 1;
      AddRun(os, WRLE_EOP, multistate, dollrun, linelen);
      putchar('\n', os);

      // flush outbuff
      if (outpos > 0 && !badwrite && !os.write(outbuff, outpos))
         badwrite = true;
   }

   if (endcomms) os << endcomms;

   if (badwrite)
      return "Failed to write output buffer!";
   else
      return 0;
}

const char *writemacrocell(std::ostream &os, char *comments, lifealgo &imp)
{
   if (imp.hyperCapable())
      return imp.writeNativeFormat(os, comments);
   else
      return "Not yet implemented.";
}

#ifdef ZLIB
class gzbuf : public std::streambuf
{
public:
   gzbuf() : file(NULL) { }
   gzbuf(const char *path) : file(NULL) { open(path); }
   ~gzbuf() { close(); }

   gzbuf *open(const char *path)
   {
      if (file) return NULL;
      file = gzopen(path, "wb");
      return file ? this : NULL;
   }

   gzbuf *close()
   {
      if (!file) return NULL;
      int res = gzclose(file);
      file = NULL;
      return res == Z_OK ? this : NULL;
   }

   bool is_open() const { return file!=NULL; }

   int overflow(int c=EOF)
   {
      if (c == EOF)
         return c ;
      return gzputc(file, c) ;
   }

   std::streamsize xsputn(const char_type *s, std::streamsize n)
   {
      return gzwrite(file, s, (unsigned int)n);
   }

   int sync()
   {
      return gzflush(file, Z_SYNC_FLUSH) == Z_OK ? 0 : -1;
   }

   pos_type seekoff(off_type off, std::ios_base::seekdir way, std::ios_base::openmode which)
   {
      if (file && off == 0 && way == std::ios_base::cur && which == std::ios_base::out)
      {
         #if ZLIB_VERNUM >= 0x1240
            // gzoffset is only available in zlib 1.2.4 or later
            return pos_type(gzoffset(file));
         #else
            // return an approximation of file size (only used in progress dialog)
            z_off_t offset = gztell(file);
            if (offset > 0) offset /= 4;
            return pos_type(offset);
         #endif
      }
      return pos_type(off_type(-1));
   }
private:
   gzFile file;
};
#endif

const char *writepattern(const char *filename, lifealgo &imp,
                         pattern_format format, output_compression compression,
                         int top, int left, int bottom, int right)
{
   // extract any comments if file exists so we can copy them to new file
   char *commptr = NULL;
   FILE *f = fopen(filename, "r");
   if (f) {
      fclose(f);
      const char *err = readcomments(filename, &commptr);
      if (err) {
         if (commptr) free(commptr);
         return err;
      }
   }

   // skip past any old #CXRLE lines at start of existing XRLE file
   char *comments = commptr;
   if (comments) {
      while (strncmp(comments, "#CXRLE", 6) == 0) {
         while (*comments != '\n') comments++;
         comments++;
      }
   }

   // open output stream
   std::streambuf *streambuf = NULL;
   std::filebuf filebuf;
#ifdef ZLIB
   gzbuf gzbuf;
#endif

   switch (compression)
   {
   default:  /* no output compression */
      streambuf = filebuf.open(filename, std::ios_base::out);
      break;

   case gzip_compression:
#ifdef ZLIB
      streambuf = gzbuf.open(filename);
      break;
#else
      if (commptr) free(commptr);
      return "GZIP compression not supported";
#endif
   }
   if (!streambuf) {
      if (commptr) free(commptr);
      return "Can't create pattern file!";
   }
   std::ostream os(streambuf);

   lifebeginprogress("Writing pattern file");

   const char *errmsg = NULL;
   switch (format) {
      case RLE_format:
         errmsg = writerle(os, comments, imp, top, left, bottom, right, false);
         break;

      case XRLE_format:
         errmsg = writerle(os, comments, imp, top, left, bottom, right, true);
         break;

      case MC_format:
         // macrocell format ignores given edges
         errmsg = writemacrocell(os, comments, imp);
         break;

      default:
         errmsg = "Unsupported pattern format!";
   }

   if (errmsg == NULL && !os.flush())
      errmsg = "Error occurred writing file; maybe disk is full?";

   lifeendprogress();

   if (commptr) free(commptr);
   
   if (isaborted())
      return "File contains truncated pattern.";
   else
      return errmsg;
}
