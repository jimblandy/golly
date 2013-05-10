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

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "viewport.h"

#include "utils.h"          // for Warning, Fatal, YesNo, Beep, etc
#include "prefs.h"          // for showgridlines, etc
#include "status.h"         // for DisplayMessage, etc
#include "render.h"         // for CreatePasteImage, DestroyPasteImage
#include "undo.h"           // for currlayer->undoredo->...
#include "select.h"         // for Selection
#include "algos.h"          // for algo_type, *_ALGO, CreateNewUniverse, etc
#include "layer.h"          // for currlayer, ResizeLayers, etc
#include "control.h"        // for generating, ChangeRule
#include "file.h"           // for GetTextFromClipboard
#include "view.h"

#import "PatternViewController.h"   // for UpdatePattern, BeginProgress, etc

// -----------------------------------------------------------------------------

// exported data:

const char* empty_selection     = "There are no live cells in the selection.";
const char* empty_outside       = "There are no live cells outside the selection.";
const char* no_selection        = "There is no selection.";
const char* selection_too_big   = "Selection is outside +/- 10^9 boundary.";
const char* pattern_too_big     = "Pattern is outside +/- 10^9 boundary.";
const char* origin_restored     = "Origin restored.";

bool fullscreen = false;        // in full screen mode?
bool nopattupdate = false;      // disable pattern updates?
bool waitingforpaste = false;   // waiting for user to decide what to do with paste image?
gRect pasterect;                // bounding box of paste image
int pastex, pastey;             // where user wants to paste clipboard pattern

bool draw_pending = false;      // delay drawing?
int pendingx, pendingy;         // start of delayed drawing

// -----------------------------------------------------------------------------

// local data:

static int cellx, celly;                // current cell's 32-bit position
static bigint bigcellx, bigcelly;       // current cell's position
static int initselx, initsely;          // location of initial selection click
static bool forceh;                     // resize selection horizontally?
static bool forcev;                     // resize selection vertically?
static bigint anchorx, anchory;         // anchor cell of current selection
static Selection prevsel;               // previous selection
static int drawstate;                   // new cell state (0..255)

static lifealgo* pastealgo = NULL;      // temporary universe with pattern to be pasted
static gRect pastebox;                  // bounding box for paste pattern
static std::string oldrule;             // rule before readclipboard is called
static std::string newrule;             // rule after readclipboard is called
static int newalgotype;                 // new algo type created by readclipboard

static bool drawingcells = false;       // drawing cells by dragging finger?
static bool pickingcells = false;       // picking cell states by dragging finger?
static bool selectingcells = false;     // selecting cells by dragging finger?
static bool movingview = false;         // moving view by dragging finger?
static bool movingpaste = false;        // moving paste image by dragging finger?

// -----------------------------------------------------------------------------

void UpdatePatternAndStatus()
{
    if (inscript || currlayer->undoredo->doingscriptchanges) return;
    
    UpdatePattern();
    UpdateStatus();
}

// -----------------------------------------------------------------------------

void UpdateEverything()
{
    UpdatePattern();
    UpdateStatus();
    UpdateEditBar();
}

// -----------------------------------------------------------------------------

// most editing operations are limited to absolute coordinates <= 10^9 because
// getcell and setcell only take int parameters (the limits must be smaller
// than INT_MIN and INT_MAX to avoid boundary conditions)
static bigint min_coord = -1000000000;
static bigint max_coord = +1000000000;

bool OutsideLimits(bigint& t, bigint& l, bigint& b, bigint& r)
{
    return ( t < min_coord || l < min_coord ||
             b > max_coord || r > max_coord );
}

// -----------------------------------------------------------------------------

void TestAutoFit()
{
    if (currlayer->autofit && generating) {
        // assume user no longer wants us to do autofitting
        currlayer->autofit = false;
    }
}

// -----------------------------------------------------------------------------

void FitInView(int force)
{
    currlayer->algo->fit(*currlayer->view, force);
}

// -----------------------------------------------------------------------------

bool PointInView(int x, int y)
{
    return ( x >= 0 && x <= currlayer->view->getxmax() &&
             y >= 0 && y <= currlayer->view->getymax() );
}

// -----------------------------------------------------------------------------

bool PointInPasteImage(int x, int y)
{
    return (x >= pasterect.x && x <= pasterect.x + pasterect.width-1 &&
            y >= pasterect.y && y <= pasterect.y + pasterect.height-1);
}

// -----------------------------------------------------------------------------

bool PointInSelection(int x, int y)
{
    pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
    int cx = cellpos.first.toint();
    int cy = cellpos.second.toint();
    return currlayer->currsel.ContainsCell(cx, cy);
}

// -----------------------------------------------------------------------------

bool CellInGrid(const bigint& x, const bigint& y)
{
    // return true if cell at x,y is within bounded grid
    if (currlayer->algo->gridwd > 0 &&
        (x < currlayer->algo->gridleft ||
         x > currlayer->algo->gridright)) return false;
    
    if (currlayer->algo->gridht > 0 &&
        (y < currlayer->algo->gridtop ||
         y > currlayer->algo->gridbottom)) return false;
    
    return true;
}

// -----------------------------------------------------------------------------

bool PointInGrid(int x, int y)
{
    // is given viewport location also in grid?
    if (currlayer->algo->gridwd == 0 && currlayer->algo->gridht == 0) {
        // unbounded grid
        return true;
    }
    pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
    return CellInGrid(cellpos.first, cellpos.second);
}

// -----------------------------------------------------------------------------

void RememberOneCellChange(int cx, int cy, int oldstate, int newstate)
{
    if (allowundo) {
        // remember this cell change for later undo/redo
        currlayer->undoredo->SaveCellChange(cx, cy, oldstate, newstate);
    }
}

// -----------------------------------------------------------------------------

void DrawCells(int x, int y)
{
    // make sure x,y is within viewport
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > currlayer->view->getxmax()) x = currlayer->view->getxmax();
    if (y > currlayer->view->getymax()) y = currlayer->view->getymax();
    
    // make sure x,y is within bounded grid
    pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
    if (currlayer->algo->gridwd > 0) {
        if (cellpos.first < currlayer->algo->gridleft) cellpos.first = currlayer->algo->gridleft;
        if (cellpos.first > currlayer->algo->gridright) cellpos.first = currlayer->algo->gridright;
    }
    if (currlayer->algo->gridht > 0) {
        if (cellpos.second < currlayer->algo->gridtop) cellpos.second = currlayer->algo->gridtop;
        if (cellpos.second > currlayer->algo->gridbottom) cellpos.second = currlayer->algo->gridbottom;
    }
    
    if ( currlayer->view->getmag() < 0 ||
        OutsideLimits(cellpos.second, cellpos.first, cellpos.second, cellpos.first) ) {
        return;
    }
    
    int currstate;        
    int numchanged = 0;
    int newx = cellpos.first.toint();
    int newy = cellpos.second.toint();
    if ( newx != cellx || newy != celly ) {
        
        // draw a line of cells using Bresenham's algorithm
        int d, ii, jj, di, ai, si, dj, aj, sj;
        di = newx - cellx;
        ai = abs(di) << 1;
        si = (di < 0)? -1 : 1;
        dj = newy - celly;
        aj = abs(dj) << 1;
        sj = (dj < 0)? -1 : 1;
        
        ii = cellx;
        jj = celly;
        
        lifealgo* curralgo = currlayer->algo;
        if (ai > aj) {
            d = aj - (ai >> 1);
            while (ii != newx) {
                currstate = curralgo->getcell(ii, jj);
                if (currstate != drawstate) {
                    curralgo->setcell(ii, jj, drawstate);
                    RememberOneCellChange(ii, jj, currstate, drawstate);
                    numchanged++;
                }
                if (d >= 0) {
                    jj += sj;
                    d  -= ai;
                }
                ii += si;
                d  += aj;
            }
        } else {
            d = ai - (aj >> 1);
            while (jj != newy) {
                currstate = curralgo->getcell(ii, jj);
                if (currstate != drawstate) {
                    curralgo->setcell(ii, jj, drawstate);
                    RememberOneCellChange(ii, jj, currstate, drawstate);
                    numchanged++;
                }
                if (d >= 0) {
                    ii += si;
                    d  -= aj;
                }
                jj += sj;
                d  += ai;
            }
        }
        
        cellx = newx;
        celly = newy;
        
        currstate = curralgo->getcell(cellx, celly);
        if (currstate != drawstate) {
            curralgo->setcell(cellx, celly, drawstate);
            RememberOneCellChange(cellx, celly, currstate, drawstate);
            numchanged++;
        }
        
    }
    
    if (numchanged > 0) {
        currlayer->algo->endofpattern();
        MarkLayerDirty();
        UpdatePattern();
        UpdateStatus();
    }
}

// -----------------------------------------------------------------------------

void StartDrawingCells(int x, int y)
{
    if (generating) {
        // temporarily stop generating when drawing cells (necessary for undo/redo)
        PauseGenerating();
        if (event_checker > 0) {
            // delay drawing until after step() finishes in NextGeneration()
            draw_pending = true;
            pendingx = x;
            pendingy = y;
            return;
        }
        // NOTE: ResumeGenerating() is called in TouchEnded()
    }

    if (!PointInGrid(x, y)) {
        ErrorMessage("Drawing is not allowed outside grid.");
        return;
    }
    
    if (currlayer->view->getmag() < 0) {
        ErrorMessage("Drawing is not allowed at scales greater than 1 cell per pixel.");
        return;
    }
    
    // check that x,y is within getcell/setcell limits
    pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
    if ( OutsideLimits(cellpos.second, cellpos.first, cellpos.second, cellpos.first) ) {
        ErrorMessage("Drawing is not allowed outside +/- 10^9 boundary.");
        return;
    }
    
    drawingcells = true;
    
    // save dirty state now for later use by RememberCellChanges
    if (allowundo) currlayer->savedirty = currlayer->dirty;
    
    cellx = cellpos.first.toint();
    celly = cellpos.second.toint();
    int currstate = currlayer->algo->getcell(cellx, celly);
    
    // reset drawing state in case it's no longer valid (due to algo/rule change)
    if (currlayer->drawingstate >= currlayer->algo->NumCellStates()) {
        currlayer->drawingstate = 1;
    }
    
    if (currstate == currlayer->drawingstate) {
        drawstate = 0;
    } else {
        drawstate = currlayer->drawingstate;
    }
    if (currstate != drawstate) {
        currlayer->algo->setcell(cellx, celly, drawstate);
        currlayer->algo->endofpattern();
        
        // remember this cell change for later undo/redo
        RememberOneCellChange(cellx, celly, currstate, drawstate);
        MarkLayerDirty();
        
        UpdatePattern();
        UpdateStatus();     // update population count
    }
}

// -----------------------------------------------------------------------------

void PickCell(int x, int y)
{
    if (!PointInGrid(x, y)) return;
    
    pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
    if ( currlayer->view->getmag() < 0 ||
        OutsideLimits(cellpos.second, cellpos.first, cellpos.second, cellpos.first) ) {
        return;
    }
    
    int newx = cellpos.first.toint();
    int newy = cellpos.second.toint();
    if ( newx != cellx || newy != celly ) {
        cellx = newx;
        celly = newy;
        currlayer->drawingstate = currlayer->algo->getcell(cellx, celly);
        UpdateEditBar();
    }
}

// -----------------------------------------------------------------------------

void StartPickingCells(int x, int y)
{
    if (!PointInGrid(x, y)) {
        ErrorMessage("Picking is not allowed outside grid.");
        return;
    }
    
    if (currlayer->view->getmag() < 0) {
        ErrorMessage("Picking is not allowed at scales greater than 1 cell per pixel.");
        return;
    }
    
    cellx = INT_MAX;
    celly = INT_MAX;
    
    PickCell(x, y);
    pickingcells = true;
}

// -----------------------------------------------------------------------------

void SelectCells(int x, int y)
{
    // only select cells within view
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > currlayer->view->getxmax()) x = currlayer->view->getxmax();
    if (y > currlayer->view->getymax()) y = currlayer->view->getymax();
    
    if ( abs(initselx - x) < 2 && abs(initsely - y) < 2 && !SelectionExists() ) {
        // avoid 1x1 selection if finger hasn't moved much
        return;
    }
    
    // make sure x,y is within bounded grid
    pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
    if (currlayer->algo->gridwd > 0) {
        if (cellpos.first < currlayer->algo->gridleft) cellpos.first = currlayer->algo->gridleft;
        if (cellpos.first > currlayer->algo->gridright) cellpos.first = currlayer->algo->gridright;
    }
    if (currlayer->algo->gridht > 0) {
        if (cellpos.second < currlayer->algo->gridtop) cellpos.second = currlayer->algo->gridtop;
        if (cellpos.second > currlayer->algo->gridbottom) cellpos.second = currlayer->algo->gridbottom;
    }
    
    if (forceh && forcev) {
        // move selection
        bigint xdelta = cellpos.first;
        bigint ydelta = cellpos.second;
        xdelta -= bigcellx;
        ydelta -= bigcelly;
        if ( xdelta != bigint::zero || ydelta != bigint::zero ) {
            currlayer->currsel.Move(xdelta, ydelta);
            bigcellx = cellpos.first;
            bigcelly = cellpos.second;
        }
    } else {
        // change selection size
        if (!forcev) currlayer->currsel.SetLeftRight(cellpos.first, anchorx);
        if (!forceh) currlayer->currsel.SetTopBottom(cellpos.second, anchory);
    }
    
    if (currlayer->currsel != prevsel) {
        // selection has changed
        DisplaySelectionSize();
        prevsel = currlayer->currsel;
        UpdatePatternAndStatus();
    }
}

// -----------------------------------------------------------------------------

void StartSelectingCells(int x, int y)
{
    TestAutoFit();

    // make sure anchor cell is within bounded grid (x,y can be outside grid)
    pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
    if (currlayer->algo->gridwd > 0) {
        if (cellpos.first < currlayer->algo->gridleft) cellpos.first = currlayer->algo->gridleft;
        if (cellpos.first > currlayer->algo->gridright) cellpos.first = currlayer->algo->gridright;
    }
    if (currlayer->algo->gridht > 0) {
        if (cellpos.second < currlayer->algo->gridtop) cellpos.second = currlayer->algo->gridtop;
        if (cellpos.second > currlayer->algo->gridbottom) cellpos.second = currlayer->algo->gridbottom;
    }
    anchorx = cellpos.first;
    anchory = cellpos.second;
    bigcellx = anchorx;
    bigcelly = anchory;
    
    // save original selection for RememberNewSelection
    currlayer->savesel = currlayer->currsel;
    
    // reset previous selection
    prevsel.Deselect();
    
    // for avoiding 1x1 selection if finger doesn't move much
    initselx = x;
    initsely = y;
    
    // allow changing size in any direction
    forceh = false;
    forcev = false;
    
    if (SelectionExists()) {
        if (PointInSelection(x, y)) {
            // modify current selection
            currlayer->currsel.Modify(x, y, anchorx, anchory, &forceh, &forcev);
            DisplaySelectionSize();
        } else {
            // remove current selection
            currlayer->currsel.Deselect();
        }
        UpdatePatternAndStatus();
    }
    
    selectingcells = true;
}

// -----------------------------------------------------------------------------

void MoveView(int x, int y)
{
    pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
    bigint xdelta = bigcellx;
    bigint ydelta = bigcelly;
    xdelta -= cellpos.first;
    ydelta -= cellpos.second;
    
    int xamount, yamount;
    int mag = currlayer->view->getmag();
    if (mag >= 0) {
        // move an integral number of cells
        xamount = xdelta.toint() << mag;
        yamount = ydelta.toint() << mag;
    } else {
        // convert cell deltas to screen pixels
        xdelta >>= -mag;
        ydelta >>= -mag;
        xamount = xdelta.toint();
        yamount = ydelta.toint();
    }
    
    if ( xamount != 0 || yamount != 0 ) {
        currlayer->view->move(xamount, yamount);
        cellpos = currlayer->view->at(x, y);
        bigcellx = cellpos.first;
        bigcelly = cellpos.second;
        UpdatePattern();
        UpdateStatus();
    }
}

// -----------------------------------------------------------------------------

void StartMovingView(int x, int y)
{
    TestAutoFit();
    pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
    bigcellx = cellpos.first;
    bigcelly = cellpos.second;
    movingview = true;
}

// -----------------------------------------------------------------------------

void MovePaste(int x, int y)
{
    pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
    bigint xdelta = bigcellx;
    bigint ydelta = bigcelly;
    xdelta -= cellpos.first;
    ydelta -= cellpos.second;
    
    int xamount, yamount;
    int mag = currlayer->view->getmag();
    if (mag >= 0) {
        // move an integral number of cells
        xamount = xdelta.toint() << mag;
        yamount = ydelta.toint() << mag;
    } else {
        // convert cell deltas to screen pixels
        xdelta >>= -mag;
        ydelta >>= -mag;
        xamount = xdelta.toint();
        yamount = ydelta.toint();
    }
    
    if ( xamount != 0 || yamount != 0 ) {
        // shift location of pasterect
        pastex -= xamount;
        pastey -= yamount;
        cellpos = currlayer->view->at(x, y);
        bigcellx = cellpos.first;
        bigcelly = cellpos.second;
        UpdatePattern();
    }
}

// -----------------------------------------------------------------------------

void StartMovingPaste(int x, int y)
{
    pair<bigint, bigint> cellpos = currlayer->view->at(x, y);
    bigcellx = cellpos.first;
    bigcelly = cellpos.second;
    movingpaste = true;
}

// -----------------------------------------------------------------------------

void TouchBegan(int x, int y)
{
    if (waitingforpaste && PointInPasteImage(x, y)) {
        StartMovingPaste(x, y);

    } else if (currlayer->touchmode == drawmode) {
        StartDrawingCells(x, y);
        
    } else if (currlayer->touchmode == pickmode) {
        StartPickingCells(x, y);
        
    } else if (currlayer->touchmode == selectmode) {
        StartSelectingCells(x, y);
        
    } else if (currlayer->touchmode == movemode) {
        StartMovingView(x, y);
        
    }
}

// -----------------------------------------------------------------------------

void TouchMoved(int x, int y)
{
    // make sure x,y is within viewport
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > currlayer->view->getxmax()) x = currlayer->view->getxmax();
    if (y > currlayer->view->getymax()) y = currlayer->view->getymax();
    
    if ( drawingcells ) {
        DrawCells(x, y);
    
    } else if ( pickingcells ) {
        PickCell(x, y);
    
    } else if ( selectingcells ) {
        SelectCells(x, y);
    
    } else if ( movingview ) {
        MoveView(x, y);
    
    } else if ( movingpaste ) {
        MovePaste(x, y);
    }
}

// -----------------------------------------------------------------------------

void TouchEnded()
{
    if (drawingcells && allowundo) {
        // MarkLayerDirty has set dirty flag, so we need to
        // pass in the flag state saved before drawing started
        currlayer->undoredo->RememberCellChanges("Drawing", currlayer->savedirty);
        UpdateEditBar();    // update various buttons
    }
    
    if (selectingcells) {
        if (allowundo) RememberNewSelection("Selection");
        UpdateEditBar();    // update various buttons
    }
    
    drawingcells = false;
    pickingcells = false;
    selectingcells = false;
    movingview = false;
    movingpaste = false;

    ResumeGenerating();
}

// -----------------------------------------------------------------------------

int SmallScroll(int xysize)
{
    int amount;
    int mag = currlayer->view->getmag();
    if (mag > 0) {
        // scroll an integral number of cells (1 cell = 2^mag pixels)
        if (mag < 3) {
            amount = ((xysize >> mag) / 20) << mag;
            if (amount == 0) amount = 1 << mag;
            return amount;
        } else {
            // grid lines are visible so scroll by only 1 cell
            return 1 << mag;
        }
    } else {
        // scroll by approx 5% of current wd/ht
        amount = xysize / 20;
        if (amount == 0) amount = 1;
        return amount;
    }
}

// -----------------------------------------------------------------------------

bool CopyRect(int itop, int ileft, int ibottom, int iright,
              lifealgo* srcalgo, lifealgo* destalgo,
              bool erasesrc, const char* progmsg)
{
    int wd = iright - ileft + 1;
    int ht = ibottom - itop + 1;
    int cx, cy;
    double maxcount = (double)wd * (double)ht;
    int cntr = 0;
    int v = 0;
    bool abort = false;
    
    // copy (and erase if requested) live cells from given rect
    // in source universe to same rect in destination universe
    BeginProgress(progmsg);
    for ( cy=itop; cy<=ibottom; cy++ ) {
        for ( cx=ileft; cx<=iright; cx++ ) {
            int skip = srcalgo->nextcell(cx, cy, v);
            if (skip + cx > iright)
                skip = -1;           // pretend we found no more live cells
            if (skip >= 0) {
                // found next live cell
                cx += skip;
                destalgo->setcell(cx, cy, v);
                if (erasesrc) srcalgo->setcell(cx, cy, 0);
            } else {
                cx = iright + 1;     // done this row
            }
            cntr++;
            if ((cntr % 4096) == 0) {
                double prog = ((cy - itop) * (double)(iright - ileft + 1) +
                               (cx - ileft)) / maxcount;
                abort = AbortProgress(prog, "");
                if (abort) break;
            }
        }
        if (abort) break;
    }
    if (erasesrc) srcalgo->endofpattern();
    destalgo->endofpattern();
    EndProgress();
    
    return !abort;
}

// -----------------------------------------------------------------------------

void CopyAllRect(int itop, int ileft, int ibottom, int iright,
                 lifealgo* srcalgo, lifealgo* destalgo,
                 const char* progmsg)
{
    int wd = iright - ileft + 1;
    int ht = ibottom - itop + 1;
    int cx, cy;
    double maxcount = (double)wd * (double)ht;
    int cntr = 0;
    bool abort = false;
    
    // copy all cells from given rect in srcalgo to same rect in destalgo
    BeginProgress(progmsg);
    for ( cy=itop; cy<=ibottom; cy++ ) {
        for ( cx=ileft; cx<=iright; cx++ ) {
            destalgo->setcell(cx, cy, srcalgo->getcell(cx, cy));
            cntr++;
            if ((cntr % 4096) == 0) {
                abort = AbortProgress((double)cntr / maxcount, "");
                if (abort) break;
            }
        }
        if (abort) break;
    }
    destalgo->endofpattern();
    EndProgress();
}

// -----------------------------------------------------------------------------

bool SelectionExists()
{
    return currlayer->currsel.Exists();
}

// -----------------------------------------------------------------------------

void SelectAll()
{
    SaveCurrentSelection();
    if (SelectionExists()) {
        currlayer->currsel.Deselect();
        UpdatePatternAndStatus();
    }
    
    if (currlayer->algo->isEmpty()) {
        ErrorMessage("All cells are dead.");
        RememberNewSelection("Deselection");
        return;
    }
    
    bigint top, left, bottom, right;
    currlayer->algo->findedges(&top, &left, &bottom, &right);
    currlayer->currsel.SetEdges(top, left, bottom, right);
    
    RememberNewSelection("Select All");
    DisplaySelectionSize();
    UpdateEverything();
}

// -----------------------------------------------------------------------------

void RemoveSelection()
{
    if (SelectionExists()) {
        SaveCurrentSelection();
        currlayer->currsel.Deselect();
        RememberNewSelection("Deselection");
        UpdateEverything();
    }
}

// -----------------------------------------------------------------------------

void FitSelection()
{
    if (!SelectionExists()) return;
    
    currlayer->currsel.Fit();
    
    TestAutoFit();
    UpdateEverything();
}

// -----------------------------------------------------------------------------

void DisplaySelectionSize()
{
    currlayer->currsel.DisplaySize();
}

// -----------------------------------------------------------------------------

void SaveCurrentSelection()
{
    if (allowundo && !currlayer->stayclean) {
        currlayer->savesel = currlayer->currsel;
    }
}

// -----------------------------------------------------------------------------

void RememberNewSelection(const char* action)
{
    if (allowundo && !currlayer->stayclean) {
        currlayer->undoredo->RememberSelection(action);
    }
}

// -----------------------------------------------------------------------------

void ClearSelection()
{
    currlayer->currsel.Clear();
}

// -----------------------------------------------------------------------------

void ClearOutsideSelection()
{
    currlayer->currsel.ClearOutside();
}

// -----------------------------------------------------------------------------

void CutSelection()
{
    currlayer->currsel.CopyToClipboard(true);
}

// -----------------------------------------------------------------------------

void CopySelection()
{
    currlayer->currsel.CopyToClipboard(false);
}

// -----------------------------------------------------------------------------

void ShrinkSelection(bool fit)
{
    currlayer->currsel.Shrink(fit);
}

// -----------------------------------------------------------------------------

void RandomFill()
{
    currlayer->currsel.RandomFill();
}

// -----------------------------------------------------------------------------

bool FlipSelection(bool topbottom, bool inundoredo)
{
    return currlayer->currsel.Flip(topbottom, inundoredo);
}

// -----------------------------------------------------------------------------

bool RotateSelection(bool clockwise, bool inundoredo)
{
    return currlayer->currsel.Rotate(clockwise, inundoredo);
}

// -----------------------------------------------------------------------------

bool GetClipboardPattern(bigint* t, bigint* l, bigint* b, bigint* r)
{
    std::string data;
    if ( !GetTextFromClipboard(data) ) return false;
    
    // copy clipboard data to temporary file so we can handle all formats supported by readclipboard
    FILE* tmpfile = fopen(clipfile.c_str(), "w");
    if (tmpfile) {
        if (fputs(data.c_str(), tmpfile) == EOF) {
            fclose(tmpfile);
            Warning("Could not write pasteboard text to temporary file!");
            return false;
        }
    } else {
        Warning("Could not create temporary file for pasteboard data!");
        return false;
    }
    fclose(tmpfile);
    
    // remember current rule
    oldrule = currlayer->algo->getrule();
    
    //!!! avoid crash in hlifealgo::endpattern if pattern copied in an algo after hlife (eg. JvN)
    // is pasted into qlife (but only with bounded grid for some unknown reason);
    // discuss better solutions with Tom:
    // 1. in loadpattern() only call imp.endofpattern() if !errmsg
    // 2. in hlifealgo::setrule() call clearcache() 1st thing
    pastealgo->setrule(pastealgo->DefaultRule());
    
    const char* err = readclipboard(clipfile.c_str(), *pastealgo, t, l, b, r);
    if (err) {
        // cycle thru all other algos until readclipboard succeeds
        for (int i = 0; i < NumAlgos(); i++) {
            if (i != currlayer->algtype) {
                delete pastealgo;
                pastealgo = CreateNewUniverse(i);
                
                //!!! avoid possible crash (see above)
                pastealgo->setrule(pastealgo->DefaultRule());
                
                err = readclipboard(clipfile.c_str(), *pastealgo, t, l, b, r);
                if (!err) {
                    newalgotype = i;   // remember new algo for later use in PasteTemporaryToCurrent
                    break;
                }
            }
        }
    }
    
    if (!err && canchangerule > 0) {
        // set newrule for later use in PasteTemporaryToCurrent
        if (canchangerule == 1 && !currlayer->algo->isEmpty()) {
            // don't change rule if universe isn't empty
            newrule = oldrule;
        } else {
            // remember rule set by readclipboard
            newrule = pastealgo->getrule();
        }
    }
    
    RemoveFile(clipfile);
    
    if (err) {
        // error probably due to bad rule string in clipboard data
        Warning("Could not load pasteboard pattern\n(probably due to unknown rule).");
        return false;
    }
    
    return true;
}

// -----------------------------------------------------------------------------

bool ClipboardContainsRule()
{
    std::string data;
    if (!GetTextFromClipboard(data)) return false;
    if (strncmp(data.c_str(), "@RULE ", 6) != 0) return false;
    
    // extract rule name
    std::string rulename;
    int i = 6;
    while (data[i] > ' ') {
        rulename += data[i];
        i++;
    }
    
    // check if Documents/Rules/rulename.rule already exists
    std::string rulepath = userrules + rulename;
    rulepath += ".rule";
    if (FileExists(rulepath)) {
        std::string question = "Do you want to replace the existing " + rulename;
        question += ".rule with the version in the pasteboard?";
        if (!YesNo(question.c_str())) {
            // don't overwrite existing .rule file
            return true;
        }
    }
    
    // create Documents/Rules/rulename.rule
    FILE* rulefile = fopen(rulepath.c_str(), "w");
    if (rulefile) {
        if (fputs(data.c_str(), rulefile) == EOF) {
            fclose(rulefile);
            Warning("Could not write pasteboard text to .rule file!");
            return true;
        }
    } else {
        Warning("Could not open .rule file for writing!");
        return true;
    }
    fclose(rulefile);
    
    // now switch to the newly created rule
    ChangeRule(rulename);

    std::string msg = "Created Documents/Rules/" + rulename + ".rule";
    DisplayMessage(msg.c_str());

    return true;
}

// -----------------------------------------------------------------------------

void PasteClipboard()
{
    // if clipboard text starts with "@RULE rulename" then install rulename.rule
    // and switch to that rule
    if (ClipboardContainsRule()) return;
    
    // create a temporary universe for storing the clipboard pattern
    if (pastealgo) Warning("Bug detected in PasteClipboard!");
    pastealgo = CreateNewUniverse(currlayer->algtype);
    
    // read clipboard pattern into temporary universe
    bigint top, left, bottom, right;
    if ( GetClipboardPattern(&top, &left, &bottom, &right) ) {
        // make sure given edges are within getcell/setcell limits
        if ( OutsideLimits(top, left, bottom, right) ) {
            ErrorMessage("Pasteboard pattern is too big.");
        } else {
            DisplayMessage("Drag paste image to desired location then double-tap image to paste/flip/rotate/etc...");
            waitingforpaste = true;

            // set initial position of pasterect's top left corner to near top left corner
            // of viewport so all of paste image is likely to be visble and it isn't far
            // to move finger from Paste button
            pastex = 128;
            pastey = 64;
            
            // create image for drawing the pattern to be pasted; note that pastebox
            // is not necessarily the minimal bounding box because clipboard pattern
            // might have blank borders (in fact it could be empty)
            int itop = top.toint();
            int ileft = left.toint();
            int ibottom = bottom.toint();
            int iright = right.toint();
            int wd = iright - ileft + 1;
            int ht = ibottom - itop + 1;
            SetRect(pastebox, ileft, itop, wd, ht);
            CreatePasteImage(pastealgo, pastebox);
        }
    }
    
    // waitingforpaste will only be false if an error occurred
    if (!waitingforpaste) {
        delete pastealgo;
        pastealgo = NULL;
    }
}

// -----------------------------------------------------------------------------

void PasteTemporaryToCurrent(bigint top, bigint left, bigint wd, bigint ht)
{
    // reset waitingforpaste now to avoid paste image being displayed prematurely
    waitingforpaste = false;
    
    bigint bottom = top;   bottom += ht;   bottom -= 1;
    bigint right = left;   right += wd;    right -= 1;
    
    // check that paste rectangle is within edit limits
    if ( OutsideLimits(top, left, bottom, right) ) {
        ErrorMessage("Pasting is not allowed outside +/- 10^9 boundary.");
        return;
    }

    // set edges of pattern in pastealgo
    int itop = pastebox.y;
    int ileft = pastebox.x;
    int ibottom = pastebox.y + pastebox.height - 1;
    int iright = pastebox.x + pastebox.width - 1;
    
    // set pastex,pastey to top left cell of paste rectangle
    pastex = left.toint();
    pastey = top.toint();
    
    // selection might change if grid becomes smaller,
    // so save current selection for RememberRuleChange/RememberAlgoChange
    SaveCurrentSelection();
    
    // pasting clipboard pattern can cause a rule change
    int oldmaxstate = currlayer->algo->NumCellStates() - 1;
    if (canchangerule > 0 && oldrule != newrule) {
        const char* err = currlayer->algo->setrule( newrule.c_str() );
        // setrule can fail if readclipboard loaded clipboard pattern into
        // a different type of algo (newalgotype)
        if (err) {
            // allow rule change to cause algo change
            ChangeAlgorithm(newalgotype, newrule.c_str());
        } else {
            // if grid is bounded then remove any live cells outside grid edges
            if (currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0) {
                ClearOutsideGrid();
            }
            // rule change might have changed the number of cell states;
            // if there are fewer states then pattern might change
            int newmaxstate = currlayer->algo->NumCellStates() - 1;
            if (newmaxstate < oldmaxstate && !currlayer->algo->isEmpty()) {
                ReduceCellStates(newmaxstate);
            }
            // switch to default colors for new rule
            UpdateLayerColors();
            if (allowundo && !currlayer->stayclean)
                currlayer->undoredo->RememberRuleChange(oldrule.c_str());
        }
    }
    
    // save cell changes if undo/redo is enabled and script isn't constructing a pattern
    bool savecells = allowundo && !currlayer->stayclean;
    //!!! if (savecells && inscript) SavePendingChanges();
    
    // don't paste cells outside bounded grid
    int gtop = currlayer->algo->gridtop.toint();
    int gleft = currlayer->algo->gridleft.toint();
    int gbottom = currlayer->algo->gridbottom.toint();
    int gright = currlayer->algo->gridright.toint();
    if (currlayer->algo->gridwd == 0) {
        // grid has infinite width
        gleft = INT_MIN;
        gright = INT_MAX;
    }
    if (currlayer->algo->gridht == 0) {
        // grid has infinite height
        gtop = INT_MIN;
        gbottom = INT_MAX;
    }
    
    // copy pattern from temporary universe to current universe
    int tx, ty, cx, cy;
    double maxcount = wd.todouble() * ht.todouble();
    int cntr = 0;
    bool abort = false;
    bool pattchanged = false;
    bool reduced = false;
    lifealgo* curralgo = currlayer->algo;
    int maxstate = curralgo->NumCellStates() - 1;
    
    BeginProgress("Pasting pattern");
    
    // we can speed up pasting sparse patterns by using nextcell in these cases:
    // - if using Or mode
    // - if current universe is empty
    // - if paste rect is outside current pattern edges
    bool usenextcell;
    if ( pmode == Or || curralgo->isEmpty() ) {
        usenextcell = true;
    } else {
        bigint ctop, cleft, cbottom, cright;
        curralgo->findedges(&ctop, &cleft, &cbottom, &cright);
        usenextcell = top > cbottom || bottom < ctop || left > cright || right < cleft;
    }
    
    if ( usenextcell && pmode == And ) {
        // current universe is empty or paste rect is outside current pattern edges
        // so don't change any cells
    } else if ( usenextcell ) {
        int newstate = 0;
        cy = pastey;
        for ( ty=itop; ty<=ibottom; ty++ ) {
            cx = pastex;
            for ( tx=ileft; tx<=iright; tx++ ) {
                int skip = pastealgo->nextcell(tx, ty, newstate);
                if (skip + tx > iright)
                    skip = -1;           // pretend we found no more live cells
                if (skip >= 0) {
                    // found next live cell so paste it into current universe
                    tx += skip;
                    cx += skip;
                    if (cx >= gleft && cx <= gright && cy >= gtop && cy <= gbottom) {
                        int currstate = curralgo->getcell(cx, cy);
                        if (currstate != newstate) {
                            if (newstate > maxstate) {
                                newstate = maxstate;
                                reduced = true;
                            }
                            curralgo->setcell(cx, cy, newstate);
                            pattchanged = true;
                            if (savecells) currlayer->undoredo->SaveCellChange(cx, cy, currstate, newstate);
                        }
                    }
                    cx++;
                } else {
                    tx = iright + 1;     // done this row
                }
                cntr++;
                if ((cntr % 4096) == 0) {
                    double prog = ((ty - itop) * (double)(iright - ileft + 1) +
                                   (tx - ileft)) / maxcount;
                    abort = AbortProgress(prog, "");
                    if (abort) break;
                }
            }
            if (abort) break;
            cy++;
        }
    } else {
        // have to use slower getcell/setcell calls
        int tempstate, currstate;
        int numstates = curralgo->NumCellStates();
        cy = pastey;
        for ( ty=itop; ty<=ibottom; ty++ ) {
            cx = pastex;
            for ( tx=ileft; tx<=iright; tx++ ) {
                tempstate = pastealgo->getcell(tx, ty);
                currstate = curralgo->getcell(cx, cy);
                if (cx >= gleft && cx <= gright && cy >= gtop && cy <= gbottom) {
                    switch (pmode) {
                        case And:
                            if (tempstate != currstate && currstate > 0) {
                                curralgo->setcell(cx, cy, 0);
                                pattchanged = true;
                                if (savecells) currlayer->undoredo->SaveCellChange(cx, cy, currstate, 0);
                            }
                            break;
                        case Copy:
                            if (tempstate != currstate) {
                                if (tempstate > maxstate) {
                                    tempstate = maxstate;
                                    reduced = true;
                                }
                                curralgo->setcell(cx, cy, tempstate);
                                pattchanged = true;
                                if (savecells) currlayer->undoredo->SaveCellChange(cx, cy, currstate, tempstate);
                            }
                            break;
                        case Or:
                            // Or mode is done using above nextcell loop;
                            // we only include this case to avoid compiler warning
                            break;
                        case Xor:
                            if (tempstate == currstate) {
                                if (currstate != 0) {
                                    curralgo->setcell(cx, cy, 0);
                                    pattchanged = true;
                                    if (savecells) currlayer->undoredo->SaveCellChange(cx, cy, currstate, 0);
                                }
                            } else {
                                // tempstate != currstate
                                int newstate = tempstate ^ currstate;
                                // if xor overflows then don't change current state
                                if (newstate >= numstates) newstate = currstate;
                                if (currstate != newstate) {
                                    curralgo->setcell(cx, cy, newstate);
                                    pattchanged = true;
                                    if (savecells) currlayer->undoredo->SaveCellChange(cx, cy, currstate, newstate);
                                }
                            }
                            break;
                    }
                }
                cx++;
                cntr++;
                if ( (cntr % 4096) == 0 ) {
                    abort = AbortProgress((double)cntr / maxcount, "");
                    if (abort) break;
                }
            }
            if (abort) break;
            cy++;
        }
    }
    
    if (pattchanged) curralgo->endofpattern();
    EndProgress();
    
    // tidy up and display result
    ClearMessage();
    if (pattchanged) {
        if (savecells) currlayer->undoredo->RememberCellChanges("Paste", currlayer->dirty);
        MarkLayerDirty();
        UpdatePatternAndStatus();
    }
    
    if (reduced) ErrorMessage("Some cell states were reduced.");
}

// -----------------------------------------------------------------------------

void DoPaste(bool toselection)
{
    bigint top, left;
    bigint wd = pastebox.width;
    bigint ht = pastebox.height;

    if (toselection) {
        // paste pattern into selection rectangle, if possible
        if (!SelectionExists()) {
            ErrorMessage("There is no selection.");
            return;
        }
        if (!currlayer->currsel.CanPaste(wd, ht, top, left)) {
            ErrorMessage("Pasteboard pattern is bigger than selection.");
            return;
        }
        // top and left have been set to the selection's top left corner
        PasteTemporaryToCurrent(top, left, wd, ht);
    
    } else {
        // paste pattern into pasterect, if possible
        if ( // allow paste if any corner of pasterect is within grid
             !( PointInGrid(pastex, pastey) ||
                PointInGrid(pastex+pasterect.width-1, pastey) ||
                PointInGrid(pastex, pastey+pasterect.height-1) ||
                PointInGrid(pastex+pasterect.width-1, pastey+pasterect.height-1) ) ) {
            ErrorMessage("Paste must be at least partially within grid.");
            return;
        }
        // get paste rectangle's top left cell coord
        pair<bigint, bigint> cellpos = currlayer->view->at(pastex, pastey);
        top = cellpos.second;
        left = cellpos.first;
        PasteTemporaryToCurrent(top, left, wd, ht);
    }
    
    AbortPaste();
}

// -----------------------------------------------------------------------------

void AbortPaste()
{
    waitingforpaste = false;
    pastex = -1;
    pastey = -1;
    if (pastealgo) {
        delete pastealgo;
        pastealgo = NULL;
    }
    DestroyPasteImage();
}

// -----------------------------------------------------------------------------

bool FlipPastePattern(bool topbottom)
{
    bool result;
    Selection pastesel(pastebox.y, pastebox.x, pastebox.y + pastebox.height - 1, pastebox.x + pastebox.width - 1);
    
    // flip the pattern in pastealgo
    lifealgo* savealgo = currlayer->algo;
    currlayer->algo = pastealgo;
    // pass in true for inundoredo parameter so flip won't be remembered
    // and layer won't be marked as dirty; also set inscript temporarily
    // so that viewport won't be updated
    inscript = true;
    result = pastesel.Flip(topbottom, true);
    // currlayer->algo might point to a *different* universe
    pastealgo = currlayer->algo;
    currlayer->algo = savealgo;
    inscript = false;
    
    if (result) {
        DestroyPasteImage();
        CreatePasteImage(pastealgo, pastebox);
    }
    
    return result;
}

// -----------------------------------------------------------------------------

bool RotatePastePattern(bool clockwise)
{
    bool result;
    Selection pastesel(pastebox.y, pastebox.x, pastebox.y + pastebox.height - 1, pastebox.x + pastebox.width - 1);
    
    // rotate the pattern in pastealgo
    lifealgo* savealgo = currlayer->algo;
    currlayer->algo = pastealgo;
    // pass in true for inundoredo parameter so rotate won't be remembered
    // and layer won't be marked as dirty; also set inscript temporarily
    // so that viewport won't be updated
    inscript = true;
    result = pastesel.Rotate(clockwise, true);
    // currlayer->algo might point to a *different* universe
    pastealgo = currlayer->algo;
    currlayer->algo = savealgo;
    inscript = false;
    
    if (result) {
        // get rotated selection and update pastebox
        int x, y, wd, ht;
        pastesel.GetRect(&x, &y, &wd, &ht);
        SetRect(pastebox, x, y, wd, ht);
        DestroyPasteImage();
        CreatePasteImage(pastealgo, pastebox);
    }
    
    return result;
}

// -----------------------------------------------------------------------------

void ZoomInPos(int x, int y)
{
    // zoom in to given point
    TestAutoFit();
    if (currlayer->view->getmag() < MAX_MAG) {
        currlayer->view->zoom(x, y);
        UpdatePatternAndStatus();
    } else {
        Beep();   // can't zoom in any further
    }
}

// -----------------------------------------------------------------------------

void ZoomOutPos(int x, int y)
{
    // zoom out from given point
    TestAutoFit();
    currlayer->view->unzoom(x, y);
    UpdatePatternAndStatus();
}
