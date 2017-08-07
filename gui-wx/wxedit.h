// This file is part of Golly.
// See docs/License.html for the copyright notice.

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

void UpdateEditBar();
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
