// This file is part of Golly.
// See docs/License.html for the copyright notice.

/**
 *   Basic utility classes for things like fatal errors.
 */
#ifndef UTIL_H
#define UTIL_H
#include <cstdio> // for FILE *

void lifefatal(const char *s) ;
void lifewarning(const char *s) ;
void lifestatus(const char *s) ;
void lifebeginprogress(const char *dlgtitle) ;
bool lifeabortprogress(double fracdone, const char *newmsg) ;
void lifeendprogress() ;
const char *lifegetuserrules() ;
const char *lifegetrulesdir() ;
bool isaborted() ;
FILE *getdebugfile() ;
/**
 *   Sick of line ending woes.  This class takes care of this for us.
 */
class linereader {
public:
   linereader(FILE *f) ;
   char *fgets(char *buf, int maxlen) ;
   void setfile(FILE *f) ;
   void setcloseonfree() ;
   int close() ;
   ~linereader() ;
private:
   int lastchar ;
   int closeonfree ;
   FILE *fp ;
} ;
/**
 *   To substitute your own routines, use the following class.
 */
class lifeerrors {
public:
   virtual void fatal(const char *s) = 0 ;
   virtual void warning(const char *s) = 0 ;
   virtual void status(const char *s) = 0 ;
   virtual void beginprogress(const char *dlgtitle) = 0 ;
   virtual bool abortprogress(double fracdone, const char *newmsg) = 0 ;
   virtual void endprogress() = 0 ;
   virtual const char *getuserrules() = 0 ;
   virtual const char *getrulesdir() = 0 ;
   static void seterrorhandler(lifeerrors *obj) ;
   bool aborted ;
} ;
#endif
