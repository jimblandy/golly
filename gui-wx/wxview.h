// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef _WXVIEW_H_
#define _WXVIEW_H_

#include "wx/glcanvas.h"    // for wxGLCanvas, wxGLContext

#include "bigint.h"         // for bigint
#include "lifealgo.h"       // for lifealgo
#include "wxselect.h"       // for Selection
#include "wxlayer.h"        // for Layer

// OpenGL version and relevant features
extern int glMajor;
extern int glMinor;
extern int glMaxTextureSize;

// OpenGL is used for viewing and editing patterns:

class PatternView : public wxGLCanvas
{
public:
    PatternView(wxWindow* parent, wxCoord x, wxCoord y, int wd, int ht, long style);
    ~PatternView();
    
    // edit functions
    void CutSelection();
    void CopySelection();
    void ClearSelection();
    void ClearOutsideSelection();
    bool GetClipboardPattern(Layer* templayer, bigint* t, bigint* l, bigint* b, bigint* r);
    void PasteClipboard(bool toselection);
    void AbortPaste();
    void CyclePasteLocation();
    void CyclePasteMode();
    void DisplaySelectionSize();
    bool SelectionExists();
    void SelectAll();
    void RemoveSelection();
    void ShrinkSelection(bool fit);
    void RandomFill();
    bool FlipSelection(bool topbottom, bool inundoredo = false);
    bool RotateSelection(bool clockwise, bool inundoredo = false);
    void SetCursorMode(wxCursor* curs);
    void CycleCursorMode();
    bool CopyRect(int top, int left, int bottom, int right,
                  lifealgo* srcalgo, lifealgo* destalgo,
                  bool erasesrc, const wxString& progmsg);
    void CopyAllRect(int top, int left, int bottom, int right,
                     lifealgo* srcalgo, lifealgo* destalgo,
                     const wxString& progmsg);
    void SaveCurrentSelection();
    void RememberNewSelection(const wxString& action);
    
    bool OutsideLimits(bigint& t, bigint& l, bigint& b, bigint& r);
    // return true if given rect is outside getcell/setcell limits
    
    bool GetCellPos(bigint& xpos, bigint& ypos);
    // return true and get mouse location's cell coords if over viewport
    
    bool PointInView(int x, int y);
    // return true if given screen position is in viewport
    
    // view functions
    void ZoomOut();
    void ZoomIn();
    void SetPixelsPerCell(int pxlspercell);
    void FitPattern();
    void FitSelection();
    void ViewOrigin();
    void ChangeOrigin();
    void RestoreOrigin();
    void SetViewSize(int wd, int ht);
    bool GridVisible();
    void ToggleGridLines();
    void ToggleCellIcons();
    void ToggleCellColors();
    void ToggleSmarterScaling();
    void UpdateScrollBars();         // update thumb positions
    void CheckCursor(bool active);   // make sure cursor is up to date
    int GetMag();                    // get magnification (..., -1=2:1, 0=1:1, 1=1:2, ...)
    void SetMag(int newmag);
    void SetPosMag(const bigint& x, const bigint& y, int mag);
    void GetPos(bigint& x, bigint& y);
    void FitInView(int force);
    bool CellVisible(const bigint& x, const bigint& y);
    bool CellInGrid(const bigint& x, const bigint& y);
    bool PointInGrid(int x, int y);
    bool RectOutsideGrid(const wxRect& rect);
    void TestAutoFit();
    
    // process keyboard and mouse events
    void ProcessKey(int key, int modifiers);
    void ProcessClick(int x, int y, int button, int modifiers);
    void ProcessClickedControl();
    
    // data
    bool waitingforclick;         // waiting for paste click?
    bool drawingcells;            // drawing cells due to dragging mouse?
    bool selectingcells;          // selecting cells due to dragging mouse?
    bool movingview;              // moving view due to dragging mouse?
    bool nopattupdate;            // disable pattern updates?
    bool showcontrols;            // draw translucent controls?
    wxRect controlsrect;          // location of translucent controls
    wxRect pasterect;             // area to be pasted
    wxCursor* oldcursor;          // non-NULL if shift key has toggled cursor
    wxCursor* restorecursor;      // non-NULL if cursor changed due to middle button click
    
    int tileindex;
    // if the tileindex is >= 0 then this is a tiled window (such windows
    // lie on top of the main viewport window when tilelayers is true);
    // the tileindex matches the layer position
    
private:
    // any class wishing to process wxWidgets events must use this macro
    DECLARE_EVENT_TABLE()
    
    // event handlers
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnKeyUp(wxKeyEvent& event);
    void OnChar(wxKeyEvent& event);
    void OnMouseDown(wxMouseEvent& event);
    void OnMouseUp(wxMouseEvent& event);
#if wxCHECK_VERSION(2, 8, 0)
    void OnMouseCaptureLost(wxMouseCaptureLostEvent& event);
#endif
    void OnMouseWheel(wxMouseEvent& event);
    void OnMouseMotion(wxMouseEvent& event);
    void OnMouseEnter(wxMouseEvent& event);
    void OnMouseExit(wxMouseEvent& event);
    void OnDragTimer(wxTimerEvent& event);
    void OnScroll(wxScrollWinEvent& event);
    void OnEraseBackground(wxEraseEvent& event);
    
    // edit functions
    void RememberOneCellChange(int cx, int cy, int oldstate, int newstate);
    void StartDrawingCells(int x, int y);
    void DrawCells(int x, int y);
    void PickCell(int x, int y);
    void StartSelectingCells(int x, int y, bool shiftdown);
    void SelectCells(int x, int y);
    void StartMovingView(int x, int y);
    void MoveView(int x, int y);
    void StopDraggingMouse();
    void RestoreSelection();
    void ZoomInPos(int x, int y);
    void ZoomOutPos(int x, int y);
    void SetPasteRect(wxRect& rect, bigint& wd, bigint& ht);
    void PasteTemporaryToCurrent(bool toselection,
                                 bigint top, bigint left, bigint bottom, bigint right);
    bool FlipPastePattern(bool topbottom);
    bool RotatePastePattern(bool clockwise);
    
    // scroll functions
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
    
    // data
    wxGLContext* glcontext;       // OpenGL context for this canvas
    bool initgl;                  // need to initialize GL state?
    wxTimer* dragtimer;           // timer used while dragging mouse
    int cellx, celly;             // current cell's 32-bit position
    bigint bigcellx, bigcelly;    // current cell's position
    int initselx, initsely;       // location of initial selection click
    bool forceh;                  // resize selection horizontally?
    bool forcev;                  // resize selection vertically?
    bigint anchorx, anchory;      // anchor cell of current selection
    Selection prevsel;            // previous selection
    int drawstate;                // new cell state (0..255)
    int pastex, pastey;           // where user wants to paste clipboard pattern
    int hthumb, vthumb;           // current thumb box positions
    int realkey;                  // key code set by OnKeyDown
    wxString debugkey;            // display debug info for OnKeyDown and OnChar
};

const wxString empty_pattern       = _("All cells are dead.");
const wxString empty_selection     = _("There are no live cells in the selection.");
const wxString empty_outside       = _("There are no live cells outside the selection.");
const wxString no_selection        = _("There is no selection.");
const wxString selection_too_big   = _("Selection is outside +/- 10^9 boundary.");
const wxString pattern_too_big     = _("Pattern is outside +/- 10^9 boundary.");
const wxString origin_restored     = _("Origin restored.");

#endif
