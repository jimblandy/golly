// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef _RENDER_H_
#define _RENDER_H_

#include "utils.h"      // for gRect

class Layer;

// Routines for rendering the pattern view:

#ifdef WEB_GUI
bool InitOGLES2();
// Return true if we can create the shaders and program objects
// required by OpenGL ES 2.
#endif

void DrawPattern(int tileindex);
// Draw the current pattern, grid lines, selection, etc.
// The given tile index is only used when drawing tiled layers.

void InitPaste(Layer* pastelayer, gRect& bbox);
// Initialize some globals used to draw the pattern stored in pastelayer.
// The given bounding box is not necessarily the *minimal* bounding box because
// the paste pattern might have blank borders (in fact it could be empty).

#endif
