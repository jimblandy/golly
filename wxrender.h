                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2005 Andrew Trevorrow and Tomas Rokicki.

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

// Routines for rendering the viewport window:

// Initialize drawing data (such as the image used to draw selections).
// Must be done after the viewport window has been created.
void InitDrawingData();

// Draw the current pattern, grid lines, selection, etc.
void DrawView(wxDC &dc, viewport &currview);

// Draw the translucent selection image in the given rectangle.
// We need to export this for drawing individual cells.
void DrawSelection(wxDC &dc, wxRect &rect);

// Call this when the main window is destroyed.
void DestroyDrawingData();

#endif
