/*** /

 This file is part of Golly, a Game of Life Simulator.
 Copyright (C) 2013 Andrew Trevorrow and Tomas Rokicki.

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

#ifndef _VIEW_H_
#define _VIEW_H_

#include "bigint.h"     // for bigint
#include "lifealgo.h"   // for lifealgo

#include "utils.h"      // for gRect

// Data and routines for viewing and editing patterns:

extern const char* empty_selection;
extern const char* empty_outside;
extern const char* no_selection;
extern const char* selection_too_big;
extern const char* pattern_too_big;
extern const char* origin_restored;

extern bool widescreen;         // is screen wide enough to show all info?
extern bool fullscreen;         // in full screen mode?
extern bool nopattupdate;       // disable pattern updates?
extern bool waitingforpaste;    // waiting for user to decide what to do with paste image?
extern gRect pasterect;         // bounding box of paste image
extern int pastex, pastey;      // where user wants to paste clipboard pattern
extern bool draw_pending;       // delay drawing?
extern int pendingx, pendingy;  // start of delayed drawing

void UpdateEverything();
void UpdatePatternAndStatus();
bool OutsideLimits(bigint& t, bigint& l, bigint& b, bigint& r);
void TestAutoFit();
void FitInView(int force);
void TouchBegan(int x, int y);
void TouchMoved(int x, int y);
void TouchEnded();
bool CopyRect(int top, int left, int bottom, int right,
              lifealgo* srcalgo, lifealgo* destalgo,
              bool erasesrc, const char* progmsg);
void CopyAllRect(int top, int left, int bottom, int right,
                 lifealgo* srcalgo, lifealgo* destalgo,
                 const char* progmsg);
bool SelectionExists();
void SelectAll();
void RemoveSelection();
void FitSelection();
void DisplaySelectionSize();
void SaveCurrentSelection();
void RememberNewSelection(const char* action);
void CutSelection();
void CopySelection();
void ClearSelection();
void ClearOutsideSelection();
void ShrinkSelection(bool fit);
void RandomFill();
bool FlipSelection(bool topbottom, bool inundoredo = false);
bool RotateSelection(bool clockwise, bool inundoredo = false);
void PasteClipboard();
bool FlipPastePattern(bool topbottom);
bool RotatePastePattern(bool clockwise);
void DoPaste(bool toselection);
void AbortPaste();
void ZoomInPos(int x, int y);
void ZoomOutPos(int x, int y);
bool PointInView(int x, int y);
bool PointInPasteImage(int x, int y);
bool PointInSelection(int x, int y);
bool PointInGrid(int x, int y);
bool CellInGrid(const bigint& x, const bigint& y);
void PanUp(int amount);
void PanDown(int amount);
void PanLeft(int amount);
void PanRight(int amount);
void PanNE();
void PanNW();
void PanSE();
void PanSW();
int SmallScroll(int xysize);
int BigScroll(int xysize);

#endif
