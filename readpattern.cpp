                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2008 Andrew Trevorrow and Tomas Rokicki.

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
#include "readpattern.h"
#include "lifealgo.h"
#include "util.h"          // for *progress calls
#include <cstdio>
#ifdef ZLIB
#include <zlib.h>
#endif
#include <cstdlib>
#include <cstring>

#define LINESIZE 20000
#define CR 13
#define LF 10

bool getedges = false;              // find pattern edges?
bigint top, left, bottom, right;    // the pattern edges

#ifdef __WXMAC__
#define BUFFSIZE 4096      // 4K is best for Mac OS X
#else
#define BUFFSIZE 8192      // 8K is best for Windows and other platforms???
#endif

#ifdef ZLIB
gzFile zinstream ;
#else
FILE *pattfile ;
#endif

char filebuff[BUFFSIZE];
int buffpos, bytesread, prevchar;

long filesize;             // length of file in bytes
double maxbuffs;           // length of file in buffers
unsigned int buffcount;    // buffers read so far (for showing in progress dialog)

// use buffered getchar instead of slow fgetc
// don't override the "getchar" name which is likely to be a macro
int mgetchar() {
   if (buffpos == BUFFSIZE) {
#ifdef ZLIB
      bytesread = gzread(zinstream, filebuff, BUFFSIZE);
#else
      bytesread = fread(filebuff, 1, BUFFSIZE, pattfile);
#endif
      buffpos = 0;
      buffcount++;
      lifeabortprogress((double)buffcount / maxbuffs, "");
   }
   if (buffpos >= bytesread) return EOF;
   return filebuff[buffpos++];
}

// use getline instead of fgets so we can handle DOS/Mac/Unix line endings
char *getline(char *line, int maxlinelen) {
   int i = 0;
   while (i < maxlinelen) {
      int ch = mgetchar();
      if (isaborted()) return NULL;
      switch (ch) {
         case CR:
            prevchar = CR;
            line[i] = 0;
            return line;
         case LF:
            if (prevchar != CR) {
               prevchar = LF;
               line[i] = 0;
               return line;
            }
            // if CR+LF (DOS) then ignore the LF
            break;
         case EOF:
            if (i == 0) return NULL;
            line[i] = 0;
            return line;
         default:
            prevchar = ch;
            line[i++] = (char) ch;
            break;
      }
   }
   line[i] = 0;      // silently truncate long line
   return line;
}

// Read a text pattern like "...ooo$$$ooo" where '.', ',' and chars <= ' '
// represent dead cells, '$' represents 10 dead cells, and all other chars
// represent live cells.
void readtextpattern(lifealgo &imp, char *line) {
   int x=0, y=0;
   char *p;

   do {
      for (p = line; *p; p++) {
         if (*p == '.' || *p == ',' || *p <= ' ') {
            x++;
         } else if (*p == '$') {
            x += 10;
         } else {
            imp.setcell(x, y, 1);
            x++;
         }
      }
      y++ ;
      if (getedges && right.toint() < x - 1) right = x - 1;
      x = 0;
   } while (getline(line, LINESIZE));

   if (getedges) bottom = y - 1;
}

/*
 *   Parse "#CXRLE key=value key=value ..." line and extract values.
 */
void ParseXRLELine(char *line, int *xoff, int *yoff, bigint &gen) {
   char *key = line;
   while (key) {
      // set key to start of next key word
      while (*key && *key != ' ') key++;
      while (*key == ' ') key++;
      if (*key == 0) return;
      
      // set value to pos of char after next '='
      char *value = key;
      while (*value && *value != '=') value++;
      if (*value == 0) return;
      value++;
   
      if (strncmp(key, "Pos", 3) == 0) {
         // extract Pos=int,int
         sscanf(value, "%d,%d", xoff, yoff);

      } else if (strncmp(key, "Gen", 3) == 0) {
         // extract Gen=bigint
         char *p = value;
         while (*p >= '0' && *p <= '9') p++;
         char savech = *p;
         *p = 0;
         gen = bigint(value);
         *p = savech;
         value = p;
      }

      key = value;
   }
}

/*
 *   Read an RLE pattern into given life algorithm implementation.
 */
const char *readrle(lifealgo &imp, char *line) {
   int n=0, x=0, y=0 ;
   char *p ;
   char *ruleptr;
   const char *errmsg = 0;
   int wd=0, ht=0, xoff=0, yoff=0;
   bigint gen = bigint::zero;
   bool xrle = false;               // extended RLE format?

   // parse any #CXRLE line(s) at start
   while (strncmp(line, "#CXRLE", 6) == 0) {
      ParseXRLELine(line, &xoff, &yoff, gen);
      imp.setGeneration(gen);
      xrle = true;
      if (getline(line, LINESIZE) == NULL) return errmsg;
   }
   
   do {
      if (line[0] == '#') {
         if (line[1] == 'r') {
            ruleptr = line;
            ruleptr += 2;
            while (*ruleptr && *ruleptr <= ' ') ruleptr++;
            p = ruleptr;
            while (*p > ' ') p++;
            *p = 0;
            errmsg = imp.setrule(ruleptr) ;
         }
      } else if (line[0] == 'x') {
         // extract wd and ht
         p = line;
         while (*p && *p != '=') p++; p++;
         sscanf(p, "%d", &wd);
         while (*p && *p != '=') p++; p++;
         sscanf(p, "%d", &ht);
         
         /* we no longer center RLE pattern around 0,0
         if (!xrle) {
            xoff = -(wd / 2);
            yoff = -(ht / 2);
         }
         */

         if (getedges) {
            top = yoff;
            left = xoff;
            bottom = yoff + ht - 1;
            right = xoff + wd - 1;
         }
         
         while (*p && *p != 'r') p++;
         if (strncmp(p, "rule", 4) == 0) {
            p += 4;
            while (*p && (*p <= ' ' || *p == '=')) p++;
            ruleptr = p;
            while (*p > ' ') p++;
            // remove any comma at end of rule
            if (p[-1] == ',') p--;
            *p = 0;
            errmsg = imp.setrule(ruleptr) ;
         }
      } else {
         n = 0 ;
         for (p=line; *p; p++) {
            if ('0' <= *p && *p <= '9') {
               n = n * 10 + *p - '0' ;
            } else {
               if (n == 0)
                  n = 1 ;
               if (*p == 'b') {
                  x += n ;
               } else if (*p == 'o') {
                  while (n-- > 0)
                     imp.setcell(xoff + x++, yoff + y, 1) ;
               } else if (*p == '$') {
                  x = 0 ;
                  y += n ;
               } else if (*p == '!') {
                  return errmsg;
               }
               n = 0 ;
            }
         }
      }
   } while (getline(line, LINESIZE));
   
   return errmsg;
}

/*
 *   This ugly bit of code will go undocumented.  It reads Alan Hensel's
 *   PC Life format, either 1.05 or 1.06.
 */
const char *readpclife(lifealgo &imp, char *line) {
   int x=0, y=0 ;
   int leftx = x ;
   char *p ;
   char *ruleptr;
   const char *errmsg = 0;

   do {
      if (line[0] == '#') {
         if (line[1] == 'P') {
            sscanf(line + 2, " %d %d", &x, &y) ;
            leftx = x ;
         } else if (line[1] == 'N') {
            imp.setrule("B3/S23") ;
         } else if (line[1] == 'R') {
            ruleptr = line;
            ruleptr += 2;
            while (*ruleptr && *ruleptr <= ' ') ruleptr++;
            p = ruleptr;
            while (*p > ' ') p++;
            *p = 0;
            errmsg = imp.setrule(ruleptr) ;
         }
      } else if (line[0] == '-' || ('0' <= line[0] && line[0] <= '9')) {
         sscanf(line, "%d %d", &x, &y) ;
         imp.setcell(x, y, 1) ;
      } else if (line[0] == '.' || line[0] == '*') {
         for (p = line; *p; p++) {
            if (*p == '*')
               imp.setcell(x, y, 1) ;
            x++ ;
         }
         x = leftx ;
         y++ ;
      }
   } while (getline(line, LINESIZE));
   
   return errmsg;
}

/*
 *   This routine reads David Bell's dblife format.
 */
void readdblife(lifealgo &imp, char *line) {
   int n=0, x=0, y=0;
   char *p;

   while (getline(line, LINESIZE)) {
      if (line[0] != '!') {
         // parse line like "23.O15.3O15.3O15.O4.4O"
         n = x = 0;
         for (p=line; *p; p++) {
            if ('0' <= *p && *p <= '9') {
               n = n * 10 + *p - '0';
            } else {
               if (n == 0) n = 1;
               if (*p == '.') {
                  x += n;
               } else if (*p == 'O') {
                  while (n-- > 0) imp.setcell(x++, y, 1);
               } else {
                  // ignore dblife commands like "5k10h@"
               }
               n = 0;
            }
         }
         y++;
      }
   }
}

long getfilesize(const char *filename) {
   // following method is only accurate for uncompressed file
   long flen = 0;
   FILE *f = fopen(filename, "r");
   if (f != 0) {
      fseek(f, 0L, SEEK_END);
      flen = ftell(f);
      fclose(f);
   }
   const char *p = strrchr(filename, '.');
   if ( p != NULL && p[1] == 'g' && p[2] == 'z' ) {
      // is there a fast way to get uncompressed size of a .gz file???!!!
      // for now we just multiply flen by a fudge factor -- yuk!
      flen = long( (double)flen * 4.0 );
   }
   return flen;
}

const char *loadpattern(lifealgo &imp) {
   char line[LINESIZE + 1] ;
   const char *errmsg = 0;

   if (getedges) {
      // called from readclipboard so don't reset rule;
      // ie. only change rule if an explicit rule is supplied
   } else {
      // reset rule to Conway's Life (default if explicit rule isn't supplied)
      imp.setrule("B3/S23") ;
   }

   buffcount = 0;
   maxbuffs = (double)filesize / (double)BUFFSIZE;
   if (maxbuffs < 1.0) maxbuffs = 1.0;
   if (getedges)
      lifebeginprogress("Reading from clipboard");
   else
      lifebeginprogress("Reading pattern file");

   // skip any blank lines at start to avoid problem when copying pattern
   // from Internet Explorer
   while (getline(line, LINESIZE) && line[0] == 0) ;

   // test for 'i' to cater for #LLAB comment in LifeLab file
   if (line[0] == '#' && line[1] == 'L' && line[2] == 'i') {
      errmsg = readpclife(imp, line) ;
      imp.endofpattern() ;
      if (getedges && !imp.isEmpty()) {
         imp.findedges(&top, &left, &bottom, &right) ;
      }

   } else if (line[0] == '#' && line[1] == 'P' && line[2] == ' ') {
      // WinLifeSearch creates clipboard patterns similar to
      // Life 1.05 format but without the header line
      errmsg = readpclife(imp, line) ;
      imp.endofpattern() ;
      if (getedges && !imp.isEmpty()) {
         imp.findedges(&top, &left, &bottom, &right) ;
      }

   } else if (line[0] == '#' || line[0] == 'x') {
      errmsg = readrle(imp, line) ;
      imp.endofpattern() ;
      // if getedges is true then readrle has set top,left,bottom,right

   } else if (line[0] == '!') {
      readdblife(imp, line) ;
      imp.endofpattern() ;
      if (getedges && !imp.isEmpty()) {
         imp.findedges(&top, &left, &bottom, &right) ;
      }

   } else if (line[0] == '[') {
      errmsg = imp.readmacrocell(line) ;
      imp.endofpattern() ;
      if (getedges && !imp.isEmpty()) {
         imp.findedges(&top, &left, &bottom, &right) ;
      }

   } else {
      // read a text pattern like "...ooo$$$ooo"
      readtextpattern(imp, line) ;
      imp.endofpattern() ;
      // if getedges is true then readtextpattern has set top,left,bottom,right
   }
   
   lifeendprogress();
   return errmsg ;
}

const char *build_err_str(const char *filename) {
   static char file_err_str[2048];
   sprintf(file_err_str, "Can't open pattern file:\n%s", filename);
   return file_err_str;
}

const char *readpattern(const char *filename, lifealgo &imp) {
   filesize = getfilesize(filename);
#ifdef ZLIB
   zinstream = gzopen(filename, "rb") ;      // rb needed on Windows
   if (zinstream == 0)
      return build_err_str(filename) ;
#else
   pattfile = fopen(filename, "r") ;
   if (pattfile == 0)
      return build_err_str(filename) ;
#endif
   buffpos = BUFFSIZE;                       // for 1st getchar call
   prevchar = 0;                             // for 1st getline call
   const char *errmsg = loadpattern(imp) ;
#ifdef ZLIB
   gzclose(zinstream) ;
#else
   fclose(pattfile) ;
#endif
   return errmsg ;
}

const char *readclipboard(const char *filename, lifealgo &imp,
                          bigint *t, bigint *l, bigint *b, bigint *r) {
   filesize = getfilesize(filename);
#ifdef ZLIB
   zinstream = gzopen(filename, "rb") ;      // rb needed on Windows
   if (zinstream == 0)
      return "Can't open clipboard file!" ;
#else
   pattfile = fopen(filename, "r") ;
   if (pattfile == 0)
      return "Can't open clipboard file!" ;
#endif
   buffpos = BUFFSIZE;                       // for 1st getchar call
   prevchar = 0;                             // for 1st getline call

   top = 0;
   left = 0;
   bottom = 0;
   right = 0;
   getedges = true;
   const char *errmsg = loadpattern(imp);
   getedges = false;
   *t = top;
   *l = left;
   *b = bottom;
   *r = right;
   // make sure we return a valid rect
   if (bottom < top) *b = top;
   if (right < left) *r = left;

#ifdef ZLIB
   gzclose(zinstream) ;
#else
   fclose(pattfile) ;
#endif
   return errmsg ;
}

const char *readcomments(const char *filename, char **commptr)
{
   // allocate a 128K buffer for storing comment data (big enough
   // for the comments in Dean Hickerson's stamp collection)
   const int maxcommlen = 128 * 1024;
   *commptr = (char *)malloc(maxcommlen);
   if (*commptr == NULL) {
      return "Not enough memory for comments!";
   }
   char *cptr = *commptr;

   filesize = getfilesize(filename);
#ifdef ZLIB
   zinstream = gzopen(filename, "rb") ;      // rb needed on Windows
   if (zinstream == 0)
      return build_err_str(filename) ;
#else
   pattfile = fopen(filename, "r") ;
   if (pattfile == 0)
      return build_err_str(filename) ;
#endif
   char line[LINESIZE + 1] ;
   buffpos = BUFFSIZE;                       // for 1st getchar call
   prevchar = 0;                             // for 1st getline call
   int commlen = 0;

   // loading comments is likely to be quite fast so no real need to
   // display the progress dialog, but getchar calls lifeabortprogress
   // so safer just to assume the progress dialog might appear
   buffcount = 0;
   maxbuffs = (double)filesize / (double)BUFFSIZE;
   if (maxbuffs < 1.0) maxbuffs = 1.0;
   lifebeginprogress("Loading comments");

   // skip any blank lines at start to avoid problem when copying pattern
   // from Internet Explorer
   while (getline(line, LINESIZE) && line[0] == 0) ;

   // test for 'i' to cater for #LLAB comment in LifeLab file
   if (line[0] == '#' && line[1] == 'L' && line[2] == 'i') {
      // extract comment lines from Life 1.05/1.06 file
      int linecount = 0;
      while (linecount < 10000) {
         linecount++;
         if (line[0] == '#' &&
               !(line[1] == 'P' && line[2] == ' ') &&
               !(line[1] == 'N' && line[2] == 0)) {
            int linelen = strlen(line);
            if (commlen + linelen + 1 > maxcommlen) break;
            strncpy(cptr + commlen, line, linelen);
            commlen += linelen;
            cptr[commlen] = '\n';      // getline strips off eol char(s)
            commlen++;
         }
         if (getline(line, LINESIZE) == NULL) break;
      }

   } else if (line[0] == '#' || line[0] == 'x') {
      // extract comment lines from RLE file
      while (line[0] == '#') {
         int linelen = strlen(line);
         if (commlen + linelen + 1 > maxcommlen) break;
         strncpy(cptr + commlen, line, linelen);
         commlen += linelen;
         cptr[commlen] = '\n';         // getline strips off eol char(s)
         commlen++;
         if (getline(line, LINESIZE) == NULL) break;
      }
      // also look for any lines after "!" but only if file is < 1MB
      // (ZLIB doesn't seem to provide a fast way to go to eof)
      if (filesize < 1024*1024) {
         bool foundexcl = false;
         while (getline(line, LINESIZE)) {
            if (strrchr(line, '!')) { foundexcl = true; break; }
         }
         if (foundexcl) {
            while (getline(line, LINESIZE)) {
               int linelen = strlen(line);
               if (commlen + linelen + 1 > maxcommlen) break;
               strncpy(cptr + commlen, line, linelen);
               commlen += linelen;
               cptr[commlen] = '\n';      // getline strips off eol char(s)
               commlen++;
            }
         }
      }

   } else if (line[0] == '!') {
      // extract "!..." lines from dblife file
      while (line[0] == '!') {
         int linelen = strlen(line);
         if (commlen + linelen + 1 > maxcommlen) break;
         strncpy(cptr + commlen, line, linelen);
         commlen += linelen;
         cptr[commlen] = '\n';            // getline strips off eol char(s)
         commlen++;
         if (getline(line, LINESIZE) == NULL) break;
      }

   } else if (line[0] == '[') {
      // extract "#C..." lines from macrocell file
      while (getline(line, LINESIZE)) {
         if (line[0] != '#') break;
         if (line[1] == 'C') {
            int linelen = strlen(line);
            if (commlen + linelen + 1 > maxcommlen) break;
            strncpy(cptr + commlen, line, linelen);
            commlen += linelen;
            cptr[commlen] = '\n';         // getline strips off eol char(s)
            commlen++;
         }
      }

   } else {
      // no comments in text pattern file???
   }
   
   lifeendprogress();
   if (commlen == maxcommlen) commlen--;
   cptr[commlen] = 0;

#ifdef ZLIB
   gzclose(zinstream) ;
#else
   fclose(pattfile) ;
#endif
   return 0 ;
}
