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
#ifndef _WXRENDER_H_
#define _WXRENDER_H_

// Routines and data for rendering the viewport window:

void InitDrawingData();
// Initialize drawing data (such as the image used to draw selections).
// Must be called AFTER the viewport window has been created.

void DestroyDrawingData();
// Call this when the main window is destroyed.

void DrawView(wxDC& dc, int tileindex);
// Draw the current pattern, grid lines, selection, etc.
// The given tile index is only used when drawing tiled layers.

void DrawOneIcon(wxDC& dc, int x, int y, wxBitmap* icon,
                 unsigned char deadr, unsigned char deadg, unsigned char deadb,
                 unsigned char liver, unsigned char liveg, unsigned char liveb);
// Draw the given icon using the given dead cell and live cell colors.

void DrawSelection(wxDC &dc, wxRect& rect);
// Draw the translucent selection image in the given rectangle.
// We need to export this for drawing individual cells.

void SetSelectionColor();
// Update the selection image's color.

class lifealgo;

void CreatePasteImage(lifealgo* pastealgo, wxRect& bbox);
// Create an image used to draw the given paste pattern.
// The given bounding box is not necessarily the *minimal* bounding box because
// the paste pattern might have blank borders (in fact it could be empty).

void DestroyPasteImage();
// Destroy the image created above (call when user clicks to end paste).

void CreateTranslucentControls();
// Create the bitmap for translucent controls and set controlswd and
// controlsht.  Must be called BEFORE the viewport window is created.

extern int controlswd;           // width of translucent controls
extern int controlsht;           // height of translucent controls

// control ids must match button order in controls bitmap
typedef enum {
   NO_CONTROL = 0,               // no current control (must be first)
   STEP1_CONTROL,                // set step exponent to zero (ie. step by 1)
   SLOWER_CONTROL,               // decrease step exponent
   FASTER_CONTROL,               // increase step exponent
   FIT_CONTROL,                  // fit entire pattern in viewport
   ZOOMIN_CONTROL,               // zoom in
   ZOOMOUT_CONTROL,              // zoom out
   NW_CONTROL,                   // pan north west
   UP_CONTROL,                   // pan up
   NE_CONTROL,                   // pan north east
   LEFT_CONTROL,                 // pan left
   MIDDLE_CONTROL,               // pan to 0,0
   RIGHT_CONTROL,                // pan right
   SW_CONTROL,                   // pan south west
   DOWN_CONTROL,                 // pan down
   SE_CONTROL                    // pan south east
} control_id;

extern control_id currcontrol;   // currently clicked control

control_id WhichControl(int x, int y);
// Return the control at the given location in the controls bitmap.

#endif
