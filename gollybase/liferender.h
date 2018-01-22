// This file is part of Golly.
// See docs/License.html for the copyright notice.

/**
 *   Encapsulate a class capable of rendering a life universe.
 *   Note that we only use blitbit calls (no setpixel).
 *   Coordinates are in the same coordinate system as the
 *   viewport min/max values.
 *
 *   Also note that the render is responsible for deciding how
 *   to scale bits up as necessary, whether using the graphics
 *   hardware or the CPU.  Blits will only be called with
 *   reasonable bitmaps (32x32 or larger, probably) so the
 *   overhead should not be horrible.  Also, the bitmap must
 *   have zeros for all pixels to the left and right of those
 *   requested to be rendered (just for simplicity).
 *
 *   If clipping is needed, it's the responsibility of these
 *   routines, *not* the caller (although the caller should make
 *   every effort to not call these routines with out of bound
 *   values).
 */
#ifndef LIFERENDER_H
#define LIFERENDER_H
class liferender {
public:
   liferender() : juststate(0) {}
   liferender(int state) : juststate(state) {}
   int justState() { return juststate ; }
   virtual ~liferender() ;

   // First two methods (pixblit/getcolors) only called for normal
   // display renderers.  For "getstate" renderers, these will never
   // be called.
   // pixblit is used to draw a pixel map by passing data in two formats:
   // If pmscale == 1 then pm data contains 4*w*h bytes where each
   // byte quadruplet contains the RGBA values for the corresponding pixel.
   // If pmscale > 1 then pm data contains (w/pmscale)*(h/pmscale) bytes
   // where each byte is a cell state (0..255).  This allows the rendering
   // code to display either icons or colors.
   virtual void pixblit(int x, int y, int w, int h, unsigned char* pm, int pmscale) ;

   // the drawing code needs access to the current layer's colors,
   // and to the transparency values for dead pixels and live pixels
   virtual void getcolors(unsigned char** r, unsigned char** g, unsigned char** b,
                          unsigned char* dead_alpha, unsigned char* live_alpha) ;
   // for state renderers, this just copies the cell state; no scaling is
   // supported.  Only called for juststate renderers.
   virtual void stateblit(int x, int y, int w, int h, unsigned char* pm) ;
private:
   int juststate ;
} ;
class staterender : public liferender {
public:
   staterender(unsigned char *_buf, int _vw, int _vh) :
               liferender(1), buf(_buf), vw(_vw), vh(_vh) {}
   virtual void stateblit(int x, int y, int w, int h, unsigned char* pm) ;
private:
   unsigned char *buf ;
   int vw, vh ;
} ;
#endif
