// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "util.h"
#include <stdio.h>
#include <stdlib.h>

/**
 *   For now error just uses stderr.
 */
class baselifeerrors : public lifeerrors {
public:
   virtual void fatal(const char *s) {
      fprintf(stderr, "%s\n", s) ;
      exit(10) ;
   }
   virtual void warning(const char *s) {
      fprintf(stderr, "%s\n", s) ;
   }
   virtual void status(const char *s) {
      fprintf(stderr, "%s\n", s) ;
   }
   virtual void beginprogress(const char *) {
      aborted = false ;
   }
   virtual bool abortprogress(double, const char *) {
      return false ;
   }
   virtual void endprogress() {
      // do nothing
   }
   virtual const char *getuserrules() {
      return "" ;
   }
   virtual const char *getrulesdir() {
      return "" ;
   }
} ;

baselifeerrors baselifeerrors ;
lifeerrors *errorhandler = &baselifeerrors ;

void lifeerrors::seterrorhandler(lifeerrors *o) {
  if (o == 0)
    errorhandler = &baselifeerrors ;
  else
    errorhandler = o ;
}

void lifefatal(const char *s) {
   errorhandler->fatal(s) ;
}

void lifewarning(const char *s) {
   errorhandler->warning(s) ;
}

void lifestatus(const char *s) {
   errorhandler->status(s) ;
}

void lifebeginprogress(const char *dlgtitle) {
   errorhandler->beginprogress(dlgtitle) ;
}

bool lifeabortprogress(double fracdone, const char *newmsg) {
   return errorhandler->aborted |=
      errorhandler->abortprogress(fracdone, newmsg) ;
}

bool isaborted() {
   return errorhandler->aborted ;
}

void lifeendprogress() {
   errorhandler->endprogress() ;
}

const char *lifegetuserrules() {
   return errorhandler->getuserrules() ;
}

const char *lifegetrulesdir() {
   return errorhandler->getrulesdir() ;
}

static FILE *f ;
FILE *getdebugfile() {
  if (f == 0)
    f = fopen("trace.txt", "w") ;
  return f ;
}
/**
 *   Manage reading lines from a FILE* without worrying about
 *   line terminates.  Note that the fgets() routine does not
 *   insert any line termination characters at all.
 */
linereader::linereader(FILE *f) {
   setfile(f) ;
}
void linereader::setfile(FILE *f) {
   fp = f ;
   lastchar = 0 ;
   closeonfree = 0 ;    // AKT: avoid crash on Linux
}
void linereader::setcloseonfree() {
   closeonfree = 1 ;
}
int linereader::close() {
   if (fp) {
      int r = fclose(fp) ;
      fp = 0 ;
      return r ;
   }
   return 0 ;
}
linereader::~linereader() {
   if (closeonfree)
      close() ;
}
const int LF = 10 ;
const int CR = 13 ;
char *linereader::fgets(char *buf, int maxlen) {
   int i = 0 ;
   for (;;) {
      if (i+1 >= maxlen) {
         buf[i] = 0 ;
         return buf ;
      }
      int c = getc(fp) ;
      switch (c) {
      case EOF:
         if (i == 0)
            return 0 ;
         buf[i] = 0 ;
         return buf ;
      case LF:
         if (lastchar != CR) {
            lastchar = LF ;
            buf[i] = 0 ;
            return buf ;
         }
         break ;
      case CR:
         lastchar = CR ;
         buf[i] = 0 ;
         return buf ;
      default:
         lastchar = c ;
         buf[i++] = (char)c ;
         break ;
      }
   }
}
