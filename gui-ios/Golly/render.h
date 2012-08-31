/*** /
 
 This file is part of Golly, a Game of Life Simulator.
 Copyright (C) 2012 Andrew Trevorrow and Tomas Rokicki.
 
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

#ifndef _RENDER_H_
#define _RENDER_H_

#include "utils.h"      // for gRect

class lifealgo;

// Routines for rendering the pattern view:

void DrawPattern(EAGLContext* context, int tileindex);
// Draw the current pattern, grid lines, selection, etc.
// The given tile index is only used when drawing tiled layers.

void CreatePasteImage(lifealgo* pastealgo, gRect& bbox);
// Create an image used to draw the given paste pattern.
// The given bounding box is not necessarily the *minimal* bounding box because
// the paste pattern might have blank borders (in fact it could be empty).

void DestroyPasteImage();
// Destroy the image created above (call when user ends paste).

/*!!!???
void DrawOneIcon(wxDC& dc, int x, int y, wxBitmap* icon,
                 unsigned char deadr, unsigned char deadg, unsigned char deadb,
                 unsigned char liver, unsigned char liveg, unsigned char liveb);
// Draw the given icon using the given dead cell and live cell colors.
*/

#endif
