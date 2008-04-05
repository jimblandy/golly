                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2008 Andrew Trevorrow and Tomas Rokicki.

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

class lifealgo;

// Routines for rendering the viewport window:

void InitDrawingData();
// Initialize drawing data (such as the image used to draw selections).
// Must be done after the viewport window has been created.

void DrawView(wxDC &dc, int tileindex);
// Draw the current pattern, grid lines, selection, etc.
// The given tile index is only used when drawing tiled layers.

void DrawSelection(wxDC &dc, wxRect &rect);
// Draw the translucent selection image in the given rectangle.
// We need to export this for drawing individual cells.

void SetSelectionColor();
// Update the selection image's color.

void CreatePasteImage(lifealgo *pastealgo, wxRect &bbox);
// Create an image used to draw the given paste pattern.
// The given bounding box is not necessarily the *minimal* bounding box because
// the paste pattern might have blank borders (in fact it could be empty).

void DestroyPasteImage();
// Destroy the image created above (call when user clicks to end paste).

void DestroyDrawingData();
// Call this when the main window is destroyed.

#endif
