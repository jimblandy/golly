// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "liferender.h"
#include "util.h"
liferender::~liferender() {}
void liferender::pixblit(int x, int y, int w, int h, unsigned char* pm, int pmscale) {
   lifefatal("pixblit not implemented") ;
}
void liferender::getcolors(unsigned char** r, unsigned char** g, unsigned char** b,
                                   unsigned char* dead_alpha, unsigned char* live_alpha) {
   lifefatal("getcolors not implemented") ;
}
void liferender::stateblit(int x, int y, int w, int h, unsigned char* pm) {
   lifefatal("stateblit not implemented") ;
}
void staterender::stateblit(int x, int y, int w, int h, unsigned char* pm) {
   int ymin = y < 0 ? 0 : y ;
   int ymax = vh < y+h ? vh-1 : y+h-1 ;
   int xmin = x < 0 ? 0 : x ;
   int xmax = vw < x+w ? vw-1 : x+w-1 ;
   if (ymax < ymin || xmax < xmin)
      return ;
   int nb = xmax - xmin + 1 ;
   for (int yy=ymin; yy<=ymax; yy++) {
      unsigned char *rp = pm + (yy - y) * w + (xmin - x) ;
      unsigned char *wp = buf + yy * vw + xmin ;
      for (int i=0; i<nb; i++)
         *wp++ = *rp++ ;
   }
}
