                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2007 Andrew Trevorrow and Tomas Rokicki.

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

static FILE *f ;
FILE *getdebugfile() {
  if (f == 0)
    f = fopen("trace.txt", "w") ;
  return f ;
}
