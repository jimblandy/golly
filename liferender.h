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
/**
 *   Encapsulate a class capable of rendering a life universe.
 *   Note that we only use fill rectangle calls and blitbit calls
 *   (no setpixel).  Coordinates are in the same coordinate
 *   system as the viewport min/max values.
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
   liferender() {}
   virtual ~liferender() ;

   // killrect is used to draw background (ie. dead cells)
   virtual void killrect(int x, int y, int w, int h) = 0 ;

   // pixblit is used to draw a pixel map by passing data in two formats:
   // If pmscale == 1 then pm data contains 3*w*h bytes where each
   // byte triplet contains the rgb values for the corresponding pixel.
   // If pmscale > 1 then pm data contains (w/pmscale)*(h/pmscale) bytes
   // where each byte is a cell state (0..255).  This allows the rendering
   // code to display either icons or colors.
   virtual void pixblit(int x, int y, int w, int h, char* pm, int pmscale) = 0 ;

   // drawing code needs access to current layer's colors
   virtual void getcolors(unsigned char** r, unsigned char** g, unsigned char** b) = 0;
} ;
#endif
