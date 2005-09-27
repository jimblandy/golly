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
   } while (getline(line, LINESIZE) != 0);

   if (getedges) bottom = y - 1;
}

/*
 *   Read an RLE pattern into given life algorithm implementation.
 */
void readrle(lifealgo &imp, char *line) {
   int n=0, x=0, y=0 ;
   char *p ;
   char *cliferules;
   int wd=0, ht=0, xoff=0, yoff=0;

   // getline at end of while loop so we see 1st line
   do {
      if (line[0] == '#') {
         if (line[1] == 'r') {
            cliferules = line;
            cliferules += 2;
            while (*cliferules && *cliferules <= ' ')
               cliferules++;
            p = cliferules;
            while (*p > ' ')
               p++;
            *p = 0;
            imp.setrule(cliferules) ;
         }
      } else if (line[0] == 'x') {
         // extract wd and ht and use to center pattern around 0,0
         p = line;
         while (*p && *p != '=') p++; p++;
         sscanf(p, "%d", &wd);
         while (*p && *p != '=') p++; p++;
         sscanf(p, "%d", &ht);
         xoff = -(wd / 2);
         yoff = -(ht / 2);

         if (getedges) {
            bigint bigwd = wd - 1;
            bigint bight = ht - 1;
            top = yoff;
            left = xoff;
            bottom = top;   bottom += bight;
            right = left;   right += bigwd;
         }
         
         for (p=line; *p && *p != 'r'; p++)
            ;
         if (strncmp(p, "rule", 4) == 0) {
            p += 4;
            while (*p && (*p <= ' ' || *p == '='))
               p++;
            cliferules = p;
            while (*p > ' ')
               p++;
            *p = 0;
            imp.setrule(cliferules) ;
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
                  return ;
               } else if (*p > ' ') {
                  // ignore illegal chars
               }
               n = 0 ;
            }
         }
      }
   } while (getline(line, LINESIZE) != 0);
}

/*
 *   This ugly bit of code will go undocumented.  It reads Alan Hensel's
 *   PC Life format, either 1.05 or 1.06.
 */
void readpclife(lifealgo &imp, char *line) {
   int x=0, y=0 ;
   int leftx = x ;
   char *p ;
   char *cliferules;

   for (;getline(line, LINESIZE);) {
      if (line[0] == '#') {
         if (line[1] == 'P') {
            sscanf(line + 2, " %d %d", &x, &y) ;
            leftx = x ;
         } else if (line[1] == 'N') {
            // already done so no need for this
         } else if (line[1] == 'R') {
            cliferules = line;
            cliferules += 2;
            while (*cliferules && *cliferules <= ' ')
               cliferules++;
            p = cliferules;
            while (*p > ' ')
               p++;
            *p = 0;
            imp.setrule(cliferules) ;
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
   char *p = strrchr(filename, '.');
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

   // always reset rules to Conway's Life
   imp.setrule("B3/S23") ;

   buffcount = 0;
   maxbuffs = (double)filesize / (double)BUFFSIZE;
   if (maxbuffs < 1.0) maxbuffs = 1.0;
   if (getedges)
      lifebeginprogress("Reading from clipboard");
   else
      lifebeginprogress("Reading pattern file");

   if (getline(line, LINESIZE) == 0) {
      // file is probably empty, so better just to continue
      // return "Can't read pattern file!" ;
   } 

   // test for 'i' to cater for #LLAB comment in LifeLab file
   if (line[0] == '#' && line[1] == 'L' && line[2] == 'i') {
      readpclife(imp, line) ;
      imp.endofpattern() ;
      if (getedges) {
         imp.findedges(&top, &left, &bottom, &right) ;
      }

   } else if (line[0] == '#' || line[0] == 'x') {
      readrle(imp, line) ;
      imp.endofpattern() ;
      // if getedges is true then readrle has set top,left,bottom,right

   } else if (line[0] == '[') {
      errmsg = imp.readmacrocell(line) ;
      imp.endofpattern() ;
      if (errmsg == 0 && getedges) {
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

const char *readpattern(const char *filename, lifealgo &imp) {
   filesize = getfilesize(filename);
#ifdef ZLIB
   zinstream = gzopen(filename, "r") ;
   if (zinstream == 0)
      return "Can't open pattern file!" ;
#else
   pattfile = fopen(filename, "r") ;
   if (pattfile == 0)
      return "Can't open pattern file!" ;
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
   zinstream = gzopen(filename, "r") ;
   if (zinstream == 0)
      return "Can't open clipboard file!" ;
#else
   pattfile = fopen(filename, "r") ;
   if (pattfile == 0)
      return "Can't open clipboard file!" ;
#endif
   buffpos = BUFFSIZE;                       // for 1st getchar call
   prevchar = 0;                             // for 1st getline call

   // save current rule changed by loadpattern
   char saverule[128];
   strncpy(saverule, imp.getrule(), sizeof(saverule));

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
         
   // restore rule
   imp.setrule(saverule);

#ifdef ZLIB
   gzclose(zinstream) ;
#else
   fclose(pattfile) ;
#endif
   return errmsg ;
}

const char *readcomments(const char *filename, char *commptr, int maxcommlen) {
   filesize = getfilesize(filename);
#ifdef ZLIB
   zinstream = gzopen(filename, "r") ;
   if (zinstream == 0)
      return "Can't open pattern file!" ;
#else
   pattfile = fopen(filename, "r") ;
   if (pattfile == 0)
      return "Can't open pattern file!" ;
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

   if (getline(line, LINESIZE) == 0) {
      // file is probably empty, so better just to continue
      // return "Can't read pattern file!" ;
   } 

   // test for 'i' to cater for #LLAB comment in LifeLab file
   if (line[0] == '#' && line[1] == 'L' && line[2] == 'i') {
      // extract "#D..." lines from Life 1.05/1.06 file
      while (line[0] == '#') {
         if (line[1] == 'D') {
            int linelen = strlen(line);
            if (commlen + linelen + 1 > maxcommlen) break;
            strncpy(commptr + commlen, line, linelen);
            commlen += linelen;
            commptr[commlen] = '\n';      // getline strips off eol char(s)
            commlen++;
         }
         if (getline(line, LINESIZE) == 0) break;
      }

   } else if (line[0] == '#' || line[0] == 'x') {
      // extract "#..." lines from RLE file
      while (line[0] == '#') {
         int linelen = strlen(line);
         if (commlen + linelen + 1 > maxcommlen) break;
         strncpy(commptr + commlen, line, linelen);
         commlen += linelen;
         commptr[commlen] = '\n';         // getline strips off eol char(s)
         commlen++;
         if (getline(line, LINESIZE) == 0) break;
      }
      // also look for any lines after "!" but only if file is < 1MB
      // (ZLIB doesn't seem to provide a fast way to go to eof)
      if (filesize < 1024*1024) {
         bool foundexcl = false;
         while ( getline(line, LINESIZE) != 0 ) {
            if ( strrchr(line, '!') ) { foundexcl = true; break; }
         }
         if (foundexcl) {
            while ( getline(line, LINESIZE) != 0 ) {
               int linelen = strlen(line);
               if (commlen + linelen + 1 > maxcommlen) break;
               strncpy(commptr + commlen, line, linelen);
               commlen += linelen;
               commptr[commlen] = '\n';      // getline strips off eol char(s)
               commlen++;
            }
         }
      }

   } else if (line[0] == '[') {
      // no comments in a macrocell file???

   } else {
      // no comments in a text pattern file???
   }
   
   lifeendprogress();
   if (commlen == maxcommlen) commlen--;
   commptr[commlen] = 0;

#ifdef ZLIB
   gzclose(zinstream) ;
#else
   fclose(pattfile) ;
#endif
   return 0 ;
}
