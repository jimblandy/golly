                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2006 Andrew Trevorrow and Tomas Rokicki.

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
#ifndef _WXVIEW_H_
#define _WXVIEW_H_

#include "bigint.h"

// Define a child window for viewing and editing patterns:

class PatternView : public wxWindow
{
public:
    PatternView(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht);
    ~PatternView();

   // edit functions
   void CutSelection();
   void CopySelection();
   void ClearSelection();
   void ClearOutsideSelection();
   void CopySelectionToClipboard(bool cut);
   bool GetClipboardPattern(lifealgo *tempalgo, bigint *t, bigint *l, bigint *b, bigint *r);
   void PasteClipboard(bool toselection);
   void CyclePasteLocation();
   void CyclePasteMode();
   void DisplaySelectionSize();
   void NoSelection();
   bool SelectionExists();
   void SelectAll();
   void RemoveSelection();
   void ShrinkSelection(bool fit);
   void RandomFill();
   void FlipLeftRight();
   void FlipUpDown();
   void RotateSelection(bool clockwise);
   void SetCursorMode(wxCursor *curs);
   void CycleCursorMode();
   bool CopyRect(int itop, int ileft, int ibottom, int iright,
                 lifealgo *srcalgo, lifealgo *destalgo,
                 bool erasesrc, const wxString &progmsg);
   void CopyAllRect(int itop, int ileft, int ibottom, int iright,
                    lifealgo *srcalgo, lifealgo *destalgo,
                    const wxString &progmsg);

   // return true if given rect is outside getcell/setcell limits
   bool OutsideLimits(bigint &t, bigint &l, bigint &b, bigint &r);

   // return true and get mouse location's cell coords if over viewport
   bool GetCellPos(bigint &xpos, bigint &ypos);

   // return true if given screen position is in viewport
   bool PointInView(int x, int y);

   // display functions
   bool GridVisible();
   bool SelectionVisible(wxRect *visrect);

   // view functions
   void ZoomOut();
   void ZoomIn();
   void SetPixelsPerCell(int pxlspercell);
   void FitPattern();
   void FitSelection();
   void ViewOrigin();
   void ChangeOrigin();
   void RestoreOrigin();
   void SetViewSize();
   void ToggleGridLines();
   void ToggleCellColors();
   void ToggleBuffering();
   void UpdateScrollBars();         // update thumb positions
   void CheckCursor(bool active);   // make sure cursor is up to date
   int GetMag();                    // get magnification (..., -1=2:1, 0=1:1, 1=1:2, ...)
   void SetMag(int newmag);
   void SetPosMag(const bigint &x, const bigint &y, int mag);
   void GetPos(bigint &x, bigint &y);
   void FitInView(int force);
   int CellVisible(const bigint &x, const bigint &y);

   // process keyboard and mouse events
   void ProcessKey(int key, bool shiftdown);
   void ProcessControlClick(int x, int y);
   void ProcessClick(int x, int y, bool shiftdown);

   // data
   bool waitingforclick;         // waiting for paste click?
   bool drawingcells;            // drawing cells due to dragging mouse?
   bool selectingcells;          // selecting cells due to dragging mouse?
   bool movingview;              // moving view due to dragging mouse?
   bool nopattupdate;            // disable pattern updates?
   bigint seltop;                // edges of current selection
   bigint selbottom;
   bigint selleft;
   bigint selright;
   bigint originy;               // new X coord set by ChangeOrigin
   bigint originx;               // new Y coord set by ChangeOrigin
   wxRect pasterect;             // area to be pasted

private:
   // any class wishing to process wxWidgets events must use this macro
   DECLARE_EVENT_TABLE()

   // event handlers
   void OnPaint(wxPaintEvent& event);
   void OnKeyDown(wxKeyEvent& event);
   void OnKeyUp(wxKeyEvent& event);
   void OnChar(wxKeyEvent& event);
   void OnMouseDown(wxMouseEvent& event);
   void OnMouseUp(wxMouseEvent& event);
   void OnRMouseDown(wxMouseEvent& event);
   void OnMouseWheel(wxMouseEvent& event);
   void OnMouseMotion(wxMouseEvent& event);
   void OnMouseEnter(wxMouseEvent& event);
   void OnMouseExit(wxMouseEvent& event);
   void OnDragTimer(wxTimerEvent& event);
   void OnScroll(wxScrollWinEvent& event);
   void OnEraseBackground(wxEraseEvent& event);
   
   // edit functions
   void ShowDrawing();
   void DrawOneCell(int cx, int cy, wxDC &dc);
   void StartDrawingCells(int x, int y);
   void DrawCells(int x, int y);
   void ModifySelection(bigint &xclick, bigint &yclick);
   void StartSelectingCells(int x, int y, bool shiftdown);
   void SelectCells(int x, int y);
   void StartMovingView(int x, int y);
   void MoveView(int x, int y);
   void StopDraggingMouse();
   void RestoreSelection();
   void TestAutoFit();
   void ZoomInPos(int x, int y);
   void ZoomOutPos(int x, int y);
   void EmptyUniverse();
   void SetPasteRect(wxRect &rect, bigint &wd, bigint &ht);
   void PasteTemporaryToCurrent(lifealgo *tempalgo, bool toselection,
                                bigint top, bigint left, bigint bottom, bigint right);
   void RotatePattern(bool clockwise, bigint &newtop, bigint &newbottom,
                                      bigint &newleft, bigint &newright);

   // scroll functions
   void PanUp(int amount);
   void PanDown(int amount);
   void PanLeft(int amount);
   void PanRight(int amount);
   int SmallScroll(int xysize);
   int BigScroll(int xysize);

   // data
   wxTimer *dragtimer;           // timer used while dragging mouse
   int cellx, celly;             // current cell's 32-bit position
   bigint bigcellx, bigcelly;    // current cell's position
   int initselx, initsely;       // location of initial selection click
   bool forceh;                  // resize selection horizontally?
   bool forcev;                  // resize selection vertically?
   bigint anchorx, anchory;      // anchor cell of current selection
   bigint origtop;               // edges of original selection
   bigint origbottom;            // (used to restore selection if user hits escape)
   bigint origleft;
   bigint origright;
   bigint prevtop;               // previous edges of new selection
   bigint prevbottom;            // (used to decide if new selection has to be drawn)
   bigint prevleft;
   bigint prevright;
   int drawstate;                // new cell state (0 or 1)
   int pastex, pastey;           // where user wants to paste clipboard pattern
   wxCursor *oldzoom;            // non-NULL if shift key has toggled zoom in/out cursor
   int hthumb, vthumb;           // current thumb box positions
};

const wxString empty_pattern       = _("All cells are dead.");
const wxString empty_selection     = _("There are no live cells in the selection.");
const wxString empty_outside       = _("There are no live cells outside the selection.");
const wxString no_selection        = _("There is no selection.");
const wxString selection_too_big   = _("Selection is outside +/- 10^9 boundary.");
const wxString origin_restored     = _("Origin restored.");

#endif
