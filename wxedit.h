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
#ifndef _WXEDIT_H_
#define _WXEDIT_H_

// Edit bar routines:

void CreateEditBar(wxWindow* parent);
// Create edit bar window above given parent window,
// but underneath layer bar if present.

int EditBarHeight();
// Return height of edit bar.

void ResizeEditBar(int wd);
// Change width of edit bar.

void UpdateEditBar(bool active);
// Update state of buttons in edit bar.

void ToggleEditBar();
// Show/hide edit bar.

void ToggleAllStates();
// Show/hide all cell states in expanded edit bar.

void ShiftEditBar(int yamount);
// Shift edit bar up/down by given amount.

void CycleDrawingState(bool higher);
// Cycle current drawing state to next higher/lower state.

#endif
