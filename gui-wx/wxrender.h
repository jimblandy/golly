// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef _WXRENDER_H_
#define _WXRENDER_H_

// Routines and data for rendering the viewport window:

void DestroyDrawingData();
// Call this when the main window is destroyed.

void DrawView(int tileindex);
// Draw the current pattern, grid lines, selection, etc.
// The given tile index is only used when drawing tiled layers.

void DrawOneIcon(wxDC& dc, int x, int y, wxBitmap* icon,
                 unsigned char deadr, unsigned char deadg, unsigned char deadb,
                 unsigned char liver, unsigned char liveg, unsigned char liveb,
                 bool multicolor);
// Draw the given icon using the given dead cell and live cell colors.
// This routine does not use OpenGL -- it's for drawing icons outside the
// viewport (eg. in the edit bar).

class Layer;

void InitPaste(Layer* pastelayer, wxRect& bbox);
// Initialize some globals used to draw the pattern stored in pastelayer.
// The given bounding box is not necessarily the *minimal* bounding box because
// the paste pattern might have blank borders (in fact it could be empty).

void CreateTranslucentControls();
// Create the bitmap for translucent controls and set controlswd and
// controlsht.  Must be called BEFORE the viewport window is created.

extern int controlswd;      // width of translucent controls
extern int controlsht;      // height of translucent controls

// control ids must match button order in controls bitmap
typedef enum {
    NO_CONTROL = 0,         // no current control (must be first)
    STEP1_CONTROL,          // set step exponent to zero (ie. step by 1)
    SLOWER_CONTROL,         // decrease step exponent
    FASTER_CONTROL,         // increase step exponent
    FIT_CONTROL,            // fit entire pattern in viewport
    ZOOMIN_CONTROL,         // zoom in
    ZOOMOUT_CONTROL,        // zoom out
    NW_CONTROL,             // pan north west
    UP_CONTROL,             // pan up
    NE_CONTROL,             // pan north east
    LEFT_CONTROL,           // pan left
    MIDDLE_CONTROL,         // pan to 0,0
    RIGHT_CONTROL,          // pan right
    SW_CONTROL,             // pan south west
    DOWN_CONTROL,           // pan down
    SE_CONTROL              // pan south east
} control_id;

extern control_id currcontrol;   // currently clicked control

control_id WhichControl(int x, int y);
// Return the control at the given location in the controls bitmap.

#endif
