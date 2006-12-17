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

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
   #include "wx/wx.h"      // for all others include the necessary headers
#endif

#include "wx/stockitem.h"  // for wxGetStockLabel
#include "wx/dnd.h"        // for wxFileDropTarget
#include "wx/filename.h"   // for wxFileName
#include "wx/clipbrd.h"    // for wxTheClipboard

#ifdef __WXMSW__
   // tool bar bitmaps are loaded via .rc file
#else
   // bitmap buttons for the tool bar
   #include "bitmaps/new.xpm"
   #include "bitmaps/open.xpm"
   #include "bitmaps/save.xpm"
   #include "bitmaps/patterns.xpm"
   #include "bitmaps/scripts.xpm"
   #include "bitmaps/play.xpm"
   #include "bitmaps/stop.xpm"
   #include "bitmaps/hash.xpm"
   #include "bitmaps/draw.xpm"
   #include "bitmaps/select.xpm"
   #include "bitmaps/move.xpm"
   #include "bitmaps/zoomin.xpm"
   #include "bitmaps/zoomout.xpm"
   #include "bitmaps/info.xpm"
#endif

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"

#include "wxgolly.h"       // for wxGetApp, statusptr, viewptr, bigview
#include "wxutils.h"       // for Warning, Fatal, BeginProgress, etc
#include "wxprefs.h"       // for gollydir, SavePrefs, SetPasteMode, etc
#include "wxinfo.h"        // for ShowInfo, GetInfoFrame
#include "wxhelp.h"        // for ShowHelp, GetHelpFrame
#include "wxstatus.h"      // for statusptr->...
#include "wxview.h"        // for viewptr->...
#include "wxrender.h"      // for InitDrawingData, DestroyDrawingData
#include "wxscript.h"      // for inscript
#include "wxlayer.h"       // for AddLayer, currlayer, etc
#include "wxmain.h"

#ifdef __WXMAC__
   #include <Carbon/Carbon.h>    // for GetCurrentProcess, etc
#endif

// -----------------------------------------------------------------------------

// IDs for timer and menu commands
enum {
   // one-shot timer
   ID_ONE_TIMER = wxID_HIGHEST,

   // go/stop button (not yet implemented!!!)
   ID_GO_STOP,

   // File menu
   // wxID_NEW,
   // wxID_OPEN,
   ID_OPEN_CLIP,
   ID_OPEN_RECENT,
   // last item in Open Recent submenu
   ID_CLEAR_PATTERNS = ID_OPEN_RECENT + MAX_RECENT + 1,
   ID_SHOW_PATTERNS,
   ID_PATTERN_DIR,
   // wxID_SAVE,
   ID_SAVE_XRLE,
   ID_RUN_SCRIPT,
   ID_RUN_CLIP,
   ID_RUN_RECENT,
   // last item in Run Recent submenu
   ID_CLEAR_SCRIPTS = ID_RUN_RECENT + MAX_RECENT + 1,
   ID_SHOW_SCRIPTS,
   ID_SCRIPT_DIR,
   // wxID_PREFERENCES,
   
   // Edit menu
   ID_CUT,
   ID_COPY,
   ID_CLEAR,
   ID_OUTSIDE,
   ID_PASTE,
   ID_PMODE,
   ID_PLOCATION,
   ID_PASTE_SEL,
   ID_SELALL,
   ID_REMOVE,
   ID_SHRINK,
   ID_RANDOM,
   ID_FLIPUD,
   ID_FLIPLR,
   ID_ROTATEC,
   ID_ROTATEA,
   ID_CMODE,

   // Paste Location submenu
   ID_PL_TL,
   ID_PL_TR,
   ID_PL_BR,
   ID_PL_BL,
   ID_PL_MID,

   // Paste Mode submenu
   ID_PM_COPY,
   ID_PM_OR,
   ID_PM_XOR,

   // Cursor Mode submenu
   ID_DRAW,
   ID_SELECT,
   ID_MOVE,
   ID_ZOOMIN,
   ID_ZOOMOUT,

   // Control menu
   ID_GO,
   ID_STOP,
   ID_NEXT,
   ID_STEP,
   ID_RESET,
   ID_FASTER,
   ID_SLOWER,
   ID_AUTO,
   ID_HASH,
   ID_HYPER,
   ID_HINFO,
   ID_RULE,
   
   // View menu
   ID_FULL,
   ID_FIT,
   ID_FIT_SEL,
   ID_MIDDLE,
   ID_RESTORE00,
   // wxID_ZOOM_IN,
   // wxID_ZOOM_OUT,
   ID_SET_SCALE,
   ID_TOOL_BAR,
   ID_LAYER_BAR,
   ID_STATUS_BAR,
   ID_EXACT,
   ID_GRID,
   ID_COLORS,
   ID_BUFF,
   ID_INFO,

   // Set Scale submenu
   ID_SCALE_1,
   ID_SCALE_2,
   ID_SCALE_4,
   ID_SCALE_8,
   ID_SCALE_16,
   
   // Help menu
   ID_HELP_INDEX,
   ID_HELP_INTRO,
   ID_HELP_TIPS,
   ID_HELP_SHORTCUTS,
   ID_HELP_SCRIPTING,
   ID_HELP_LEXICON,
   ID_HELP_FILE,
   ID_HELP_EDIT,
   ID_HELP_CONTROL,
   ID_HELP_VIEW,
   ID_HELP_LAYER,
   ID_HELP_HELP,
   ID_HELP_REFS,
   ID_HELP_PROBLEMS,
   ID_HELP_CHANGES,
   ID_HELP_CREDITS,

   // Layer menu
   ID_ADD_LAYER,
   ID_CLONE,
   ID_DUPLICATE,
   ID_DEL_LAYER,
   ID_DEL_OTHERS,
   ID_MOVE_LAYER,
   ID_NAME_LAYER,
   ID_SYNC_VIEW,
   ID_SYNC_CURS,
   ID_STACK,
   ID_TILE,
   ID_LAYER0         // keep this last (for OnMenu)
};

// static routines used by GetPrefs() to get IDs for items in Open/Run Recent submenus;
// can't be MainFrame methods because GetPrefs() is called before creating main window
// and I'd rather not expose the IDs in a header file

int GetID_CLEAR_PATTERNS() { return ID_CLEAR_PATTERNS; }
int GetID_OPEN_RECENT()    { return ID_OPEN_RECENT; }
int GetID_CLEAR_SCRIPTS()  { return ID_CLEAR_SCRIPTS; }
int GetID_RUN_RECENT()     { return ID_RUN_RECENT; }

// -----------------------------------------------------------------------------

// one-shot timer to fix problems in wxMac and wxGTK -- see OnOneTimer;
// must be static because it's used in DnDFile::OnDropFiles
wxTimer *onetimer;

#ifdef __WXMSW__
bool callUnselect = false;    // OnIdle needs to call Unselect?
#endif

// -----------------------------------------------------------------------------

// bitmaps for tool bar buttons
const int go_index = 0;
const int stop_index = 1;
const int new_index = 2;
const int open_index = 3;
const int save_index = 4;
const int patterns_index = 5;
const int scripts_index = 6;
const int draw_index = 7;
const int sel_index = 8;
const int move_index = 9;
const int zoomin_index = 10;
const int zoomout_index = 11;
const int info_index = 12;
const int hash_index = 13;
wxBitmap tbBitmaps[14];

// -----------------------------------------------------------------------------

// update tool bar buttons according to the current state
void MainFrame::UpdateToolBar(bool active)
{
   wxToolBar *tbar = GetToolBar();
   if (tbar && tbar->IsShown()) {
      if (viewptr->waitingforclick) active = false;
            
      #ifdef __WXX11__
         // avoid problems by first toggling off all buttons
         tbar->ToggleTool(ID_GO, false);
         tbar->ToggleTool(ID_STOP, false);
         tbar->ToggleTool(ID_HASH, false);
         tbar->ToggleTool(wxID_NEW, false);
         tbar->ToggleTool(wxID_OPEN, false);
         tbar->ToggleTool(wxID_SAVE, false);
         tbar->ToggleTool(ID_SHOW_PATTERNS, false);
         tbar->ToggleTool(ID_SHOW_SCRIPTS, false);
         tbar->ToggleTool(ID_DRAW, false);
         tbar->ToggleTool(ID_SELECT, false);
         tbar->ToggleTool(ID_MOVE, false);
         tbar->ToggleTool(ID_ZOOMIN, false);
         tbar->ToggleTool(ID_ZOOMOUT, false);
         tbar->ToggleTool(ID_INFO, false);
      #endif
      
      bool busy = generating || inscript;
      
      tbar->EnableTool(ID_GO,             active && !busy);
      tbar->EnableTool(ID_STOP,           active && busy);
      tbar->EnableTool(ID_HASH,           active && !busy);
      tbar->EnableTool(wxID_NEW,          active && !busy);
      tbar->EnableTool(wxID_OPEN,         active && !busy);
      tbar->EnableTool(wxID_SAVE,         active && !busy);
      tbar->EnableTool(ID_SHOW_PATTERNS,  active);
      tbar->EnableTool(ID_SHOW_SCRIPTS,   active);
      tbar->EnableTool(ID_DRAW,           active);
      tbar->EnableTool(ID_SELECT,         active);
      tbar->EnableTool(ID_MOVE,           active);
      tbar->EnableTool(ID_ZOOMIN,         active);
      tbar->EnableTool(ID_ZOOMOUT,        active);
      tbar->EnableTool(ID_INFO,           active && !currlayer->currfile.IsEmpty());

      // call ToggleTool for tools added via AddCheckTool or AddRadioTool
      tbar->ToggleTool(ID_HASH,           currlayer->hash);
      tbar->ToggleTool(ID_SHOW_PATTERNS,  showpatterns);
      tbar->ToggleTool(ID_SHOW_SCRIPTS,   showscripts);
      tbar->ToggleTool(ID_DRAW,           currlayer->curs == curs_pencil);
      tbar->ToggleTool(ID_SELECT,         currlayer->curs == curs_cross);
      tbar->ToggleTool(ID_MOVE,           currlayer->curs == curs_hand);
      tbar->ToggleTool(ID_ZOOMIN,         currlayer->curs == curs_zoomin);
      tbar->ToggleTool(ID_ZOOMOUT,        currlayer->curs == curs_zoomout);
   }
}

// -----------------------------------------------------------------------------

bool MainFrame::ClipboardHasText()
{
   #ifdef __WXX11__
      return wxFileExists(clipfile);
   #else
      bool hastext = false;
      if ( wxTheClipboard->Open() ) {
         hastext = wxTheClipboard->IsSupported( wxDF_TEXT );
         if (!hastext) {
            // we'll try to convert bitmap data to text pattern
            hastext = wxTheClipboard->IsSupported( wxDF_BITMAP );
         }
         wxTheClipboard->Close();
      }
      return hastext;
   #endif
}

// -----------------------------------------------------------------------------

bool MainFrame::StatusVisible()
{
   return (statusptr && statusptr->statusht > 0);
}

// -----------------------------------------------------------------------------

void MainFrame::EnableAllMenus(bool enable)
{
   #ifdef __WXMAC__
      // enable/disable all menus, including Help menu and items in app menu
      if (enable)
         EndAppModalStateForWindow( (OpaqueWindowPtr*)this->MacGetWindowRef() );
      else
         BeginAppModalStateForWindow( (OpaqueWindowPtr*)this->MacGetWindowRef() );
   #else
      wxMenuBar *mbar = GetMenuBar();
      if (mbar) {
         int count = mbar->GetMenuCount();
         int i;
         for (i = 0; i<count; i++) {
            mbar->EnableTop(i, enable);
         }
      }
   #endif
}

// -----------------------------------------------------------------------------

// update menu bar items according to the given state
void MainFrame::UpdateMenuItems(bool active)
{
   wxMenuBar *mbar = GetMenuBar();
   wxToolBar *tbar = GetToolBar();
   if (mbar) {
      bool textinclip = ClipboardHasText();
      bool selexists = viewptr->SelectionExists();
      bool busy = generating || inscript;

      if (viewptr->waitingforclick) active = false;
      
      mbar->Enable(wxID_NEW,           active && !busy);
      mbar->Enable(wxID_OPEN,          active && !busy);
      mbar->Enable(ID_OPEN_CLIP,       active && !busy && textinclip);
      mbar->Enable(ID_OPEN_RECENT,     active && !busy && numpatterns > 0);
      mbar->Enable(ID_SHOW_PATTERNS,   active);
      mbar->Enable(ID_PATTERN_DIR,     active);
      mbar->Enable(wxID_SAVE,          active && !busy);
      mbar->Enable(ID_SAVE_XRLE,       active);
      mbar->Enable(ID_RUN_SCRIPT,      active && !busy);
      mbar->Enable(ID_RUN_CLIP,        active && !busy && textinclip);
      mbar->Enable(ID_RUN_RECENT,      active && !busy && numscripts > 0);
      mbar->Enable(ID_SHOW_SCRIPTS,    active);
      mbar->Enable(ID_SCRIPT_DIR,      active);
      mbar->Enable(wxID_PREFERENCES,   !busy);

      mbar->Enable(ID_CUT,       active && !busy && selexists);
      mbar->Enable(ID_COPY,      active && !busy && selexists);
      mbar->Enable(ID_CLEAR,     active && !busy && selexists);
      mbar->Enable(ID_OUTSIDE,   active && !busy && selexists);
      mbar->Enable(ID_PASTE,     active && !busy && textinclip);
      mbar->Enable(ID_PASTE_SEL, active && !busy && textinclip && selexists);
      mbar->Enable(ID_PLOCATION, active);
      mbar->Enable(ID_PMODE,     active);
      mbar->Enable(ID_SELALL,    active);
      mbar->Enable(ID_REMOVE,    active && selexists);
      mbar->Enable(ID_SHRINK,    active && selexists);
      mbar->Enable(ID_RANDOM,    active && !busy && selexists);
      mbar->Enable(ID_FLIPUD,    active && !busy && selexists);
      mbar->Enable(ID_FLIPLR,    active && !busy && selexists);
      mbar->Enable(ID_ROTATEC,   active && !busy && selexists);
      mbar->Enable(ID_ROTATEA,   active && !busy && selexists);
      mbar->Enable(ID_CMODE,     active);

      mbar->Enable(ID_GO,        active && !busy);
      mbar->Enable(ID_STOP,      active && busy);
      mbar->Enable(ID_NEXT,      active && !busy);
      mbar->Enable(ID_STEP,      active && !busy);
      mbar->Enable(ID_RESET,     active && !busy &&
                                 currlayer->algo->getGeneration() > currlayer->startgen);
      mbar->Enable(ID_FASTER,    active);
      mbar->Enable(ID_SLOWER,    active && currlayer->warp > minwarp);
      mbar->Enable(ID_AUTO,      active);
      mbar->Enable(ID_HASH,      active && !busy);
      mbar->Enable(ID_HYPER,     active);
      mbar->Enable(ID_HINFO,     active);
      mbar->Enable(ID_RULE,      active && !busy);

      mbar->Enable(ID_FULL,      active);
      mbar->Enable(ID_FIT,       active);
      mbar->Enable(ID_FIT_SEL,   active && selexists);
      mbar->Enable(ID_MIDDLE,    active);
      mbar->Enable(ID_RESTORE00, active && (currlayer->originx != bigint::zero ||
                                            currlayer->originy != bigint::zero));
      mbar->Enable(wxID_ZOOM_IN, active && viewptr->GetMag() < MAX_MAG);
      mbar->Enable(wxID_ZOOM_OUT, active);
      mbar->Enable(ID_SET_SCALE, active);
      mbar->Enable(ID_TOOL_BAR,  active);
      mbar->Enable(ID_LAYER_BAR, active);
      mbar->Enable(ID_STATUS_BAR,active);
      mbar->Enable(ID_EXACT,     active);
      mbar->Enable(ID_GRID,      active);
      mbar->Enable(ID_COLORS,    active);
      #ifdef __WXMAC__
         // windows on Mac OS X are automatically buffered
         mbar->Enable(ID_BUFF,   false);
         mbar->Check(ID_BUFF,    true);
      #else
         mbar->Enable(ID_BUFF,   active);
         mbar->Check(ID_BUFF,    buffered);
      #endif
      mbar->Enable(ID_INFO,      !currlayer->currfile.IsEmpty());

      mbar->Enable(ID_ADD_LAYER,    active && !busy && numlayers < maxlayers);
      mbar->Enable(ID_CLONE,        active && !busy && numlayers < maxlayers);
      mbar->Enable(ID_DUPLICATE,    active && !busy && numlayers < maxlayers);
      mbar->Enable(ID_DEL_LAYER,    active && !busy && numlayers > 1);
      mbar->Enable(ID_DEL_OTHERS,   active && !inscript && numlayers > 1);
      mbar->Enable(ID_MOVE_LAYER,   active && !busy && numlayers > 1);
      mbar->Enable(ID_NAME_LAYER,   active && !inscript);
      mbar->Enable(ID_SYNC_VIEW,    active);
      mbar->Enable(ID_SYNC_CURS,    active);
      mbar->Enable(ID_STACK,        active);
      mbar->Enable(ID_TILE,         active);
      for (int id = ID_LAYER0; id < ID_LAYER0 + numlayers; id++) {
         if (generating) {
            // allow switching to clone of current universe
            mbar->Enable(id, active && !inscript &&
                             currlayer->cloneid > 0 &&
                             currlayer->cloneid == GetLayer(id - ID_LAYER0)->cloneid);
         } else {
            mbar->Enable(id, active && !inscript);
         }
      }

      // tick/untick menu items created using AppendCheckItem
      mbar->Check(ID_SAVE_XRLE,     savexrle);
      mbar->Check(ID_SHOW_PATTERNS, showpatterns);
      mbar->Check(ID_SHOW_SCRIPTS,  showscripts);
      mbar->Check(ID_AUTO,       autofit);
      mbar->Check(ID_HASH,       currlayer->hash);
      mbar->Check(ID_HYPER,      hyperspeed);
      mbar->Check(ID_HINFO,      hlifealgo::getVerbose() != 0);
      mbar->Check(ID_TOOL_BAR,   tbar && tbar->IsShown());
      mbar->Check(ID_LAYER_BAR,  showlayer);
      mbar->Check(ID_STATUS_BAR, StatusVisible());
      mbar->Check(ID_EXACT,      showexact);
      mbar->Check(ID_GRID,       showgridlines);
      mbar->Check(ID_COLORS,     swapcolors);
      mbar->Check(ID_PL_TL,      plocation == TopLeft);
      mbar->Check(ID_PL_TR,      plocation == TopRight);
      mbar->Check(ID_PL_BR,      plocation == BottomRight);
      mbar->Check(ID_PL_BL,      plocation == BottomLeft);
      mbar->Check(ID_PL_MID,     plocation == Middle);
      mbar->Check(ID_PM_COPY,    pmode == Copy);
      mbar->Check(ID_PM_OR,      pmode == Or);
      mbar->Check(ID_PM_XOR,     pmode == Xor);
      mbar->Check(ID_DRAW,       currlayer->curs == curs_pencil);
      mbar->Check(ID_SELECT,     currlayer->curs == curs_cross);
      mbar->Check(ID_MOVE,       currlayer->curs == curs_hand);
      mbar->Check(ID_ZOOMIN,     currlayer->curs == curs_zoomin);
      mbar->Check(ID_ZOOMOUT,    currlayer->curs == curs_zoomout);
      mbar->Check(ID_SCALE_1,    viewptr->GetMag() == 0);
      mbar->Check(ID_SCALE_2,    viewptr->GetMag() == 1);
      mbar->Check(ID_SCALE_4,    viewptr->GetMag() == 2);
      mbar->Check(ID_SCALE_8,    viewptr->GetMag() == 3);
      mbar->Check(ID_SCALE_16,   viewptr->GetMag() == 4);
      mbar->Check(ID_SYNC_VIEW,  syncviews);
      mbar->Check(ID_SYNC_CURS,  synccursors);
      mbar->Check(ID_STACK,      stacklayers);
      mbar->Check(ID_TILE,       tilelayers);
      for (int id = ID_LAYER0; id < ID_LAYER0 + numlayers; id++)
         mbar->Check(id, currindex == (id - ID_LAYER0));
   }
}

// -----------------------------------------------------------------------------

void MainFrame::UpdateUserInterface(bool active)
{
   UpdateToolBar(active);
   UpdateLayerBar(active);
   UpdateMenuItems(active);
   viewptr->CheckCursor(active);
   statusptr->CheckMouseLocation(active);

   #ifdef __WXMSW__
      // ensure viewport window has keyboard focus if main window is active
      if (active) viewptr->SetFocus();
   #endif
}

// -----------------------------------------------------------------------------

// update everything in main window, and menu bar and cursor
void MainFrame::UpdateEverything()
{
   if (IsIconized()) {
      // main window has been minimized, so only update menu bar items
      UpdateMenuItems(false);
      return;
   }

   // update tool bar, layer bar, menu bar and cursor
   UpdateUserInterface(IsActive());

   if (inscript) {
      // make sure scroll bars are accurate while running script
      bigview->UpdateScrollBars();
      return;
   }

   int wd, ht;
   GetClientSize(&wd, &ht);      // includes status bar and viewport

   if (wd > 0 && ht > statusptr->statusht) {
      UpdateView();
      bigview->UpdateScrollBars();
   }
   
   if (wd > 0 && ht > 0 && StatusVisible()) {
      statusptr->Refresh(false);
      statusptr->Update();
   }
}

// -----------------------------------------------------------------------------

// only update viewport and status bar
void MainFrame::UpdatePatternAndStatus()
{
   if (inscript) return;

   if (!IsIconized()) {
      UpdateView();
      if (StatusVisible()) {
         statusptr->CheckMouseLocation(IsActive());
         statusptr->Refresh(false);
         statusptr->Update();
      }
   }
}

// -----------------------------------------------------------------------------

// only update status bar
void MainFrame::UpdateStatus()
{
   if (!IsIconized()) {
      if (StatusVisible()) {
         statusptr->CheckMouseLocation(IsActive());
         statusptr->Refresh(false);
         statusptr->Update();
      }
   }
}

// -----------------------------------------------------------------------------

void MainFrame::SimplifyTree(wxString &dir, wxTreeCtrl* treectrl, wxTreeItemId root)
{
   // delete old tree (except root)
   treectrl->DeleteChildren(root);

   // append dir as only child
   wxDirItemData* diritem = new wxDirItemData(dir, dir, true);
   wxTreeItemId id;
   id = treectrl->AppendItem(root, dir.AfterLast(wxFILE_SEP_PATH), 0, 0, diritem);
   if ( diritem->HasFiles() || diritem->HasSubDirs() ) {
      treectrl->SetItemHasChildren(id);
      treectrl->Expand(id);
      #ifndef __WXMSW__
         // causes crash on Windows!!!
         treectrl->ScrollTo(root);
      #endif
   }
}

// -----------------------------------------------------------------------------

void MainFrame::DeselectTree(wxTreeCtrl* treectrl, wxTreeItemId root)
{
   // recursively traverse tree and reset each file item background to white
   wxTreeItemIdValue cookie;
   wxTreeItemId id = treectrl->GetFirstChild(root, cookie);
   while ( id.IsOk() ) {
      if ( treectrl->ItemHasChildren(id) ) {
         DeselectTree(treectrl, id);
      } else {
         wxColor currcolor = treectrl->GetItemBackgroundColour(id);
         if ( currcolor != *wxWHITE ) {
            treectrl->SetItemBackgroundColour(id, *wxWHITE);
         }
      }
      id = treectrl->GetNextChild(root, cookie);
   }
}

// -----------------------------------------------------------------------------

// Define a window for right pane of split window:

class RightWindow : public wxWindow
{
public:
   RightWindow(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht)
      : wxWindow(parent, wxID_ANY, wxPoint(xorg,yorg), wxSize(wd,ht),
                 wxNO_BORDER |
                 // need this to avoid layer bar buttons flashing on Windows
                 wxNO_FULL_REPAINT_ON_RESIZE)
   {
      // avoid erasing background on GTK+
      SetBackgroundStyle(wxBG_STYLE_CUSTOM);
   }
   ~RightWindow() {}

   // event handlers
   void OnSize(wxSizeEvent& event);
   void OnEraseBackground(wxEraseEvent& event);

   DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(RightWindow, wxWindow)
   EVT_SIZE             (RightWindow::OnSize)
   EVT_ERASE_BACKGROUND (RightWindow::OnEraseBackground)
END_EVENT_TABLE()

// -----------------------------------------------------------------------------

void RightWindow::OnSize(wxSizeEvent& event)
{
   int wd, ht;
   GetClientSize(&wd, &ht);
   if (wd > 0 && ht > 0 && bigview) {
      // resize layer bar and main viewport window
      ResizeLayerBar(wd);
      bigview->SetSize(0, showlayer ? layerbarht : 0,
                       wd, showlayer ? ht - layerbarht : ht);
   }
   event.Skip();
}

// -----------------------------------------------------------------------------

void RightWindow::OnEraseBackground(wxEraseEvent& WXUNUSED(event))
{
   // do nothing because layer bar and viewport cover all of right pane
}

// -----------------------------------------------------------------------------

RightWindow* rightpane;

wxWindow* MainFrame::RightPane()
{
   return rightpane;
}

// -----------------------------------------------------------------------------

void MainFrame::ResizeSplitWindow()
{
   int wd, ht;
   GetClientSize(&wd, &ht);

   splitwin->SetSize(0, statusptr->statusht, wd,
                     ht > statusptr->statusht ? ht - statusptr->statusht : 0);

   // wxSplitterWindow automatically resizes left and right panes;
   // note that RightWindow::OnSize has been called
}

// -----------------------------------------------------------------------------

void MainFrame::ToggleStatusBar()
{
   int wd, ht;
   GetClientSize(&wd, &ht);
   if (StatusVisible()) {
      statusptr->statusht = 0;
      statusptr->SetSize(0, 0, 0, 0);
      #ifdef __WXX11__
         // move so we don't see small portion
         statusptr->Move(-100, -100);
      #endif
   } else {
      statusptr->statusht = showexact ? STATUS_EXHT : STATUS_HT;
      statusptr->SetSize(0, 0, wd, statusptr->statusht);
   }
   ResizeSplitWindow();
   UpdateEverything();
}

// -----------------------------------------------------------------------------

void MainFrame::ToggleExactNumbers()
{
   int wd, ht;
   GetClientSize(&wd, &ht);
   showexact = !showexact;
   if (StatusVisible()) {
      statusptr->statusht = showexact ? STATUS_EXHT : STATUS_HT;
      statusptr->SetSize(0, 0, wd, statusptr->statusht);
      ResizeSplitWindow();
      UpdateEverything();
   } else {
      // show the status bar using new size
      ToggleStatusBar();
   }
}

// -----------------------------------------------------------------------------

void MainFrame::ToggleToolBar()
{
   #ifdef __WXX11__
      // Show(false) does not hide tool bar!!!
      statusptr->ErrorMessage(_("Sorry, tool bar hiding is not implemented for X11."));
   #else
      wxToolBar *tbar = GetToolBar();
      if (tbar) {
         if (tbar->IsShown()) {
            tbar->Show(false);
         } else {
            tbar->Show(true);
         }
         int wd, ht;
         #ifdef __WXGTK__
            // wxGTK bug??? do a temporary size change to force origin to change -- sheesh
            GetSize(&wd, &ht);
            SetSize(wd-1, ht-1);
            SetSize(wd, ht);
         #endif
         GetClientSize(&wd, &ht);
         if (StatusVisible()) {
            // adjust size of status bar
            statusptr->SetSize(0, 0, wd, statusptr->statusht);
         }
         ResizeSplitWindow();
         UpdateEverything();
      }
   #endif
}

// -----------------------------------------------------------------------------

void MainFrame::ToggleFullScreen()
{
   #ifdef __WXX11__
      // ShowFullScreen(true) does nothing!!!
      statusptr->ErrorMessage(_("Sorry, full screen mode is not implemented for X11."));
   #else
      static bool restorestatusbar; // restore status bar at end of full screen mode?
      static bool restorelayerbar;  // restore layer bar?
      static bool restoretoolbar;   // restore tool bar?
      static bool restorepattdir;   // restore pattern directory?
      static bool restorescrdir;    // restore script directory?

      if (!fullscreen) {
         // save current location and size for use in SavePrefs
         wxRect r = GetRect();
         mainx = r.x;
         mainy = r.y;
         mainwd = r.width;
         mainht = r.height;
      }

      fullscreen = !fullscreen;
      ShowFullScreen(fullscreen,
         // don't use wxFULLSCREEN_ALL because that prevents tool bar being
         // toggled in full screen mode on Windows
         wxFULLSCREEN_NOMENUBAR | wxFULLSCREEN_NOBORDER | wxFULLSCREEN_NOCAPTION);

      wxToolBar *tbar = GetToolBar();
      if (fullscreen) {
         // hide scroll bars
         bigview->SetScrollbar(wxHORIZONTAL, 0, 0, 0, true);
         bigview->SetScrollbar(wxVERTICAL, 0, 0, 0, true);
         
         // hide status bar if necessary
         restorestatusbar = StatusVisible();
         if (restorestatusbar) {
            statusptr->statusht = 0;
            statusptr->SetSize(0, 0, 0, 0);
         }
         
         // hide layer bar if necessary
         restorelayerbar = showlayer;
         if (restorelayerbar) {
            ToggleLayerBar();
         }
         
         // hide tool bar if necessary
         restoretoolbar = tbar && tbar->IsShown();
         if (restoretoolbar) {
            tbar->Show(false);
         }
         
         // hide pattern/script directory if necessary
         restorepattdir = showpatterns;
         restorescrdir = showscripts;
         if (restorepattdir) {
            dirwinwd = splitwin->GetSashPosition();
            splitwin->Unsplit(patternctrl);
            showpatterns = false;
         } else if (restorescrdir) {
            dirwinwd = splitwin->GetSashPosition();
            splitwin->Unsplit(scriptctrl);
            showscripts = false;
         }

      } else {
         // first show tool bar if necessary
         if (restoretoolbar && tbar && !tbar->IsShown()) {
            tbar->Show(true);
            if (StatusVisible()) {
               // reduce width of status bar below
               restorestatusbar = true;
            }
         }
         
         // show status bar if necessary;
         // note that even if it's visible we may have to resize width
         if (restorestatusbar) {
            statusptr->statusht = showexact ? STATUS_EXHT : STATUS_HT;
            int wd, ht;
            GetClientSize(&wd, &ht);
            statusptr->SetSize(0, 0, wd, statusptr->statusht);
         }

         // show layer bar if necessary
         if (restorelayerbar && !showlayer) {
            ToggleLayerBar();
         }

         // restore pattern/script directory if necessary
         if ( restorepattdir && !splitwin->IsSplit() ) {
            splitwin->SplitVertically(patternctrl, RightPane(), dirwinwd);
            showpatterns = true;
         } else if ( restorescrdir && !splitwin->IsSplit() ) {
            splitwin->SplitVertically(scriptctrl, RightPane(), dirwinwd);
            showscripts = true;
         }
      }

      if (!fullscreen) {
         // restore scroll bars BEFORE setting viewport size
         bigview->UpdateScrollBars();
      }
      // adjust size of viewport (and pattern/script directory if visible)
      ResizeSplitWindow();
      UpdateEverything();
   #endif
}

// -----------------------------------------------------------------------------

void MainFrame::ShowPatternInfo()
{
   if (viewptr->waitingforclick || currlayer->currfile.IsEmpty()) return;
   ShowInfo(currlayer->currfile);
}

// -----------------------------------------------------------------------------

// event table and handlers for main window:

BEGIN_EVENT_TABLE(MainFrame, wxFrame)
   EVT_MENU                (wxID_ANY,        MainFrame::OnMenu)
   EVT_BUTTON              (wxID_ANY,        MainFrame::OnButton)
   EVT_SET_FOCUS           (                 MainFrame::OnSetFocus)
   EVT_ACTIVATE            (                 MainFrame::OnActivate)
   EVT_SIZE                (                 MainFrame::OnSize)
   EVT_IDLE                (                 MainFrame::OnIdle)
#ifdef __WXMAC__
   EVT_TREE_ITEM_EXPANDED  (wxID_TREECTRL,   MainFrame::OnDirTreeExpand)
   // wxMac bug??? EVT_TREE_ITEM_COLLAPSED doesn't get called
   EVT_TREE_ITEM_COLLAPSING (wxID_TREECTRL,  MainFrame::OnDirTreeCollapse)
#endif
   EVT_TREE_SEL_CHANGED    (wxID_TREECTRL,   MainFrame::OnDirTreeSelection)
   EVT_SPLITTER_DCLICK     (wxID_ANY,        MainFrame::OnSashDblClick)
   EVT_TIMER               (ID_ONE_TIMER,    MainFrame::OnOneTimer)
   EVT_CLOSE               (                 MainFrame::OnClose)
END_EVENT_TABLE()

// -----------------------------------------------------------------------------

void MainFrame::OnMenu(wxCommandEvent& event)
{
   showbanner = false;
   statusptr->ClearMessage();
   int id = event.GetId();
   switch (id)
   {
      // File menu
      case wxID_NEW:          NewPattern(); break;
      case wxID_OPEN:         OpenPattern(); break;
      case ID_OPEN_CLIP:      OpenClipboard(); break;
      case ID_CLEAR_PATTERNS: ClearRecentPatterns(); break;
      case ID_SHOW_PATTERNS:  ToggleShowPatterns(); break;
      case ID_PATTERN_DIR:    ChangePatternDir(); break;
      case wxID_SAVE:         SavePattern(); break;
      case ID_SAVE_XRLE:      savexrle = !savexrle; break;
      case ID_RUN_SCRIPT:     OpenScript(); break;
      case ID_RUN_CLIP:       RunClipboard(); break;
      case ID_CLEAR_SCRIPTS:  ClearRecentScripts(); break;
      case ID_SHOW_SCRIPTS:   ToggleShowScripts(); break;
      case ID_SCRIPT_DIR:     ChangeScriptDir(); break;
      case wxID_PREFERENCES:  ShowPrefsDialog(); break;
      case wxID_EXIT:         Close(true); break;        // true forces frame to close
      // Edit menu
      case ID_CUT:            viewptr->CutSelection(); break;
      case ID_COPY:           viewptr->CopySelection(); break;
      case ID_CLEAR:          viewptr->ClearSelection(); break;
      case ID_OUTSIDE:        viewptr->ClearOutsideSelection(); break;
      case ID_PASTE:          viewptr->PasteClipboard(false); break;
      case ID_PASTE_SEL:      viewptr->PasteClipboard(true); break;
      case ID_PL_TL:          SetPasteLocation("TopLeft"); break;
      case ID_PL_TR:          SetPasteLocation("TopRight"); break;
      case ID_PL_BR:          SetPasteLocation("BottomRight"); break;
      case ID_PL_BL:          SetPasteLocation("BottomLeft"); break;
      case ID_PL_MID:         SetPasteLocation("Middle"); break;
      case ID_PM_COPY:        SetPasteMode("Copy"); break;
      case ID_PM_OR:          SetPasteMode("Or"); break;
      case ID_PM_XOR:         SetPasteMode("Xor"); break;
      case ID_SELALL:         viewptr->SelectAll(); break;
      case ID_REMOVE:         viewptr->RemoveSelection(); break;
      case ID_SHRINK:         viewptr->ShrinkSelection(false); break;
      case ID_RANDOM:         viewptr->RandomFill(); break;
      case ID_FLIPUD:         viewptr->FlipUpDown(); break;
      case ID_FLIPLR:         viewptr->FlipLeftRight(); break;
      case ID_ROTATEC:        viewptr->RotateSelection(true); break;
      case ID_ROTATEA:        viewptr->RotateSelection(false); break;
      case ID_DRAW:           viewptr->SetCursorMode(curs_pencil); break;
      case ID_SELECT:         viewptr->SetCursorMode(curs_cross); break;
      case ID_MOVE:           viewptr->SetCursorMode(curs_hand); break;
      case ID_ZOOMIN:         viewptr->SetCursorMode(curs_zoomin); break;
      case ID_ZOOMOUT:        viewptr->SetCursorMode(curs_zoomout); break;
      // Control menu
      case ID_GO:             GeneratePattern(); break;
      case ID_STOP:           StopGenerating(); break;
      case ID_NEXT:           NextGeneration(false); break;
      case ID_STEP:           NextGeneration(true); break;
      case ID_RESET:          ResetPattern(); break;
      case ID_FASTER:         GoFaster(); break;
      case ID_SLOWER:         GoSlower(); break;
      case ID_AUTO:           ToggleAutoFit(); break;
      case ID_HASH:           ToggleHashing(); break;
      case ID_HYPER:          ToggleHyperspeed(); break;
      case ID_HINFO:          ToggleHashInfo(); break;
      case ID_RULE:           ShowRuleDialog(); break;
      // View menu
      case ID_FULL:           ToggleFullScreen(); break;
      case ID_TOOL_BAR:       ToggleToolBar(); break;
      case ID_LAYER_BAR:      ToggleLayerBar(); break;
      case ID_STATUS_BAR:     ToggleStatusBar(); break;
      case ID_EXACT:          ToggleExactNumbers(); break;
      case ID_INFO:           ShowPatternInfo(); break;
      case ID_FIT:            viewptr->FitPattern(); break;
      case ID_FIT_SEL:        viewptr->FitSelection(); break;
      case ID_MIDDLE:         viewptr->ViewOrigin(); break;
      case ID_RESTORE00:      viewptr->RestoreOrigin(); break;
      case wxID_ZOOM_IN:      viewptr->ZoomIn(); break;
      case wxID_ZOOM_OUT:     viewptr->ZoomOut(); break;
      case ID_SCALE_1:        viewptr->SetPixelsPerCell(1); break;
      case ID_SCALE_2:        viewptr->SetPixelsPerCell(2); break;
      case ID_SCALE_4:        viewptr->SetPixelsPerCell(4); break;
      case ID_SCALE_8:        viewptr->SetPixelsPerCell(8); break;
      case ID_SCALE_16:       viewptr->SetPixelsPerCell(16); break;
      case ID_GRID:           viewptr->ToggleGridLines(); break;
      case ID_COLORS:         viewptr->ToggleCellColors(); break;
      case ID_BUFF:           viewptr->ToggleBuffering(); break;
      // Layer menu
      case ID_ADD_LAYER:      AddLayer(); break;
      case ID_CLONE:          CloneLayer(); break;
      case ID_DUPLICATE:      DuplicateLayer(); break;
      case ID_DEL_LAYER:      DeleteLayer(); break;
      case ID_DEL_OTHERS:     DeleteOtherLayers(); break;
      case ID_MOVE_LAYER:     MoveLayerDialog(); break;
      case ID_NAME_LAYER:     NameLayerDialog(); break;
      case ID_SYNC_VIEW:      ToggleSyncViews(); break;
      case ID_SYNC_CURS:      ToggleSyncCursors(); break;
      case ID_STACK:          ToggleStackLayers(); break;
      case ID_TILE:           ToggleTileLayers(); break;
      // Help menu
      case ID_HELP_INDEX:     ShowHelp(_("Help/index.html")); break;
      case ID_HELP_INTRO:     ShowHelp(_("Help/intro.html")); break;
      case ID_HELP_TIPS:      ShowHelp(_("Help/tips.html")); break;
      case ID_HELP_SHORTCUTS: ShowHelp(_("Help/shortcuts.html")); break;
      case ID_HELP_SCRIPTING: ShowHelp(_("Help/scripting.html")); break;
      case ID_HELP_LEXICON:   ShowHelp(_("Help/Lexicon/lex.htm")); break;
      case ID_HELP_FILE:      ShowHelp(_("Help/file.html")); break;
      case ID_HELP_EDIT:      ShowHelp(_("Help/edit.html")); break;
      case ID_HELP_CONTROL:   ShowHelp(_("Help/control.html")); break;
      case ID_HELP_VIEW:      ShowHelp(_("Help/view.html")); break;
      case ID_HELP_LAYER:     ShowHelp(_("Help/layer.html")); break;
      case ID_HELP_HELP:      ShowHelp(_("Help/help.html")); break;
      case ID_HELP_REFS:      ShowHelp(_("Help/refs.html")); break;
      case ID_HELP_PROBLEMS:  ShowHelp(_("Help/problems.html")); break;
      case ID_HELP_CHANGES:   ShowHelp(_("Help/changes.html")); break;
      case ID_HELP_CREDITS:   ShowHelp(_("Help/credits.html")); break;
      case wxID_ABOUT:        ShowAboutBox(); break;
      default:
         if ( id > ID_OPEN_RECENT && id <= ID_OPEN_RECENT + numpatterns ) {
            OpenRecentPattern(id);
         } else
         if ( id > ID_RUN_RECENT && id <= ID_RUN_RECENT + numscripts ) {
            OpenRecentScript(id);
         } else
         if ( id >= ID_LAYER0 ) {
            SetLayer(id - ID_LAYER0);
         }
   }
   UpdateUserInterface(IsActive());
   
   // allow user interaction while running script
   if (inscript) {
      inscript = false;
      UpdatePatternAndStatus();
      inscript = true;
   }
}

// -----------------------------------------------------------------------------

void MainFrame::OnButton(wxCommandEvent& WXUNUSED(event))
{
   /* when we have a working go/stop button we may need code like this!!!
   showbanner = false;
   statusptr->ClearMessage();
   viewptr->SetFocus();
   if ( event.GetId() == ID_GO_STOP ) {
      if (generating) {
         StopGenerating();
      } else {
         GeneratePattern();
      }
   }
   */
}

// -----------------------------------------------------------------------------

void MainFrame::OnSetFocus(wxFocusEvent& WXUNUSED(event))
{
   // this is never called in Mac app, presumably because it doesn't
   // make any sense for a wxFrame to get the keyboard focus

   #ifdef __WXMSW__
      // fix wxMSW problem: don't let main window get focus after being minimized
      if (viewptr) viewptr->SetFocus();
   #endif

   #ifdef __WXX11__
      // make sure viewport keeps keyboard focus whenever main window is active
      if (viewptr && IsActive()) viewptr->SetFocus();
      // fix problems after modal dialog or help window is closed
      UpdateUserInterface(IsActive());
   #endif
}

// -----------------------------------------------------------------------------

void MainFrame::OnActivate(wxActivateEvent& event)
{
   // this is never called in X11 app!!!
   // note that IsActive() doesn't always match event.GetActive()

   #ifdef __WXMAC__
      if (!event.GetActive()) wxSetCursor(*wxSTANDARD_CURSOR);
      // to avoid disabled menu items after a modal dialog closes
      // don't call UpdateMenuItems on deactivation
      if (event.GetActive()) UpdateUserInterface(true);
   #else
      UpdateUserInterface(event.GetActive());
   #endif
   
   #ifdef __WXGTK__
      if (event.GetActive()) onetimer->Start(20, wxTIMER_ONE_SHOT);
      // OnOneTimer will be called after delay of 0.02 secs
   #endif
   
   event.Skip();
}

// -----------------------------------------------------------------------------

void MainFrame::OnSize(wxSizeEvent& event)
{
   #ifdef __WXMSW__
      // save current location and size for use in SavePrefs if app
      // is closed when window is minimized
      wxRect r = GetRect();
      mainx = r.x;
      mainy = r.y;
      mainwd = r.width;
      mainht = r.height;
   #endif

   int wd, ht;
   GetClientSize(&wd, &ht);
   if (wd > 0 && ht > 0) {
      // note that statusptr and viewptr might be NULL if OnSize gets called
      // from MainFrame::MainFrame (true if X11)
      if (StatusVisible()) {
         // adjust size of status bar
         statusptr->SetSize(0, 0, wd, statusptr->statusht);
      }
      if (viewptr && statusptr && ht > statusptr->statusht) {
         // adjust size of viewport (and pattern/script directory if visible)
         ResizeSplitWindow();
      }
   }
   
   #if defined(__WXX11__) || defined(__WXGTK__)
      // need to do default processing for menu bar and tool bar
      event.Skip();
   #else
      wxUnusedVar(event);
   #endif
}

// -----------------------------------------------------------------------------

void MainFrame::OnIdle(wxIdleEvent& WXUNUSED(event))
{
   #ifdef __WXX11__
      // don't change focus here because it prevents menus staying open
      return;
   #endif
   
   #ifdef __WXMSW__
      if ( callUnselect ) {
         // deselect file/folder so user can click the same item
         if (showpatterns) patternctrl->GetTreeCtrl()->Unselect();
         if (showscripts) scriptctrl->GetTreeCtrl()->Unselect();
         callUnselect = false;
         
         // calling SetFocus once doesn't stuff up layer bar buttons
         if ( IsActive() && viewptr ) viewptr->SetFocus();
      }
   #else
      // ensure viewport window has keyboard focus if main window is active;
      // note that we can't do this on Windows because it stuffs up clicks
      // in layer bar buttons
      if ( IsActive() && viewptr ) viewptr->SetFocus();
   #endif
}

// -----------------------------------------------------------------------------

void MainFrame::OnDirTreeExpand(wxTreeEvent& WXUNUSED(event))
{
   if ((generating || inscript) && (showpatterns || showscripts)) {
      // send idle event so directory tree gets updated
      wxIdleEvent idleevent;
      wxGetApp().SendIdleEvents(this, idleevent);
   }
}

// -----------------------------------------------------------------------------

void MainFrame::OnDirTreeCollapse(wxTreeEvent& WXUNUSED(event))
{
   if ((generating || inscript) && (showpatterns || showscripts)) {
      // send idle event so directory tree gets updated
      wxIdleEvent idleevent;
      wxGetApp().SendIdleEvents(this, idleevent);
   }
}

// -----------------------------------------------------------------------------

void MainFrame::OnDirTreeSelection(wxTreeEvent& event)
{
   // note that viewptr will be NULL if called from MainFrame::MainFrame
   if ( viewptr ) {
      wxTreeItemId id = event.GetItem();
      if ( !id.IsOk() ) return;

      wxGenericDirCtrl* dirctrl = NULL;
      if (showpatterns) dirctrl = patternctrl;
      if (showscripts) dirctrl = scriptctrl;
      if (dirctrl == NULL) return;
      
      wxString filepath = dirctrl->GetFilePath();

      // deselect file/folder so this handler will be called if user clicks same item
      wxTreeCtrl* treectrl = dirctrl->GetTreeCtrl();
      #ifdef __WXMSW__
         // can't call UnselectAll() or Unselect() here
      #else
         treectrl->UnselectAll();
      #endif

      if ( filepath.IsEmpty() ) {
         // user clicked on a folder name so expand or collapse it???
         // unfortunately, using Collapse/Expand causes this handler to be
         // called again and there's no easy way to distinguish between
         // a click in the folder name or a dbl-click (or a click in the
         // +/-/arrow image)
         /*
         if ( treectrl->IsExpanded(id) ) {
            treectrl->Collapse(id);
         } else {
            treectrl->Expand(id);
         }
         */
      } else {
         // user clicked on a file name
         if ( inscript ) {
            // use Warning because statusptr->ErrorMessage does nothing if inscript
            if ( showpatterns )
               Warning(_("Cannot load pattern while a script is running."));
            else
               Warning(_("Cannot run script while another one is running."));
         } else if ( generating ) {
            if ( showpatterns )
               statusptr->ErrorMessage(_("Cannot load pattern while generating."));
            else
               statusptr->ErrorMessage(_("Cannot run script while generating."));
         } else {
            // reset background of previously selected file by traversing entire tree;
            // we can't just remember previously selected id because ids don't persist
            // after a folder has been collapsed and expanded
            DeselectTree(treectrl, treectrl->GetRootItem());

            // indicate the selected file
            treectrl->SetItemBackgroundColour(id, *wxLIGHT_GREY);
            
            #ifdef __WXX11__
               // needed for scripts like goto.py which prompt user to enter string
               viewptr->SetFocus();
            #endif
            
            // load pattern or run script
            OpenFile(filepath);
         }
      }

      #ifdef __WXMSW__
         // calling Unselect() here causes a crash so do later in OnIdle
         callUnselect = true;
      #endif

      // changing focus here works on X11 but not on Mac or Windows
      // (presumably because they set focus to treectrl after this call)
      viewptr->SetFocus();
   }
}

// -----------------------------------------------------------------------------

void MainFrame::OnSashDblClick(wxSplitterEvent& WXUNUSED(event))
{
   // splitwin's sash was double-clicked
   if (showpatterns) ToggleShowPatterns();
   if (showscripts) ToggleShowScripts();
   UpdateMenuItems(IsActive());
   UpdateToolBar(IsActive());
}

// -----------------------------------------------------------------------------

void MainFrame::OnOneTimer(wxTimerEvent& WXUNUSED(event))
{
   // fix drag and drop problem on Mac -- see DnDFile::OnDropFiles
   #ifdef __WXMAC__
      // remove colored frame
      if (viewptr) RefreshView();
   #endif
   
   // fix menu item problem on Linux after modal dialog has closed
   #ifdef __WXGTK__
      UpdateMenuItems(true);
   #endif
}

// -----------------------------------------------------------------------------

void MainFrame::OnClose(wxCloseEvent& WXUNUSED(event))
{
   if (GetHelpFrame()) GetHelpFrame()->Close(true);
   if (GetInfoFrame()) GetInfoFrame()->Close(true);
   
   if (splitwin->IsSplit()) dirwinwd = splitwin->GetSashPosition();
   
   #ifndef __WXMAC__
      // if script is running we need to call exit below
      bool wasinscript = inscript;
   #endif

   // abort any running script and tidy up; also restores current directory
   // to location of Golly app so prefs file will be saved in correct place
   FinishScripting();

   // save main window location and other user preferences
   SavePrefs();
   
   // delete any temporary files
   if (wxFileExists(scriptfile)) wxRemoveFile(scriptfile);
   for (int i = 0; i < numlayers; i++) {
      Layer* layer = GetLayer(i);
      if (wxFileExists(layer->tempstart)) wxRemoveFile(layer->tempstart);
   }
   
   #ifndef __WXMAC__
      // avoid error message on Windows or seg fault on Linux
      if (wasinscript) exit(0);
   #endif

   #if defined(__WXX11__) || defined(__WXGTK__)
      // avoid seg fault on Linux
      if (generating) exit(0);
   #else
      if (generating) StopGenerating();
   #endif
   
   Destroy();
}

// -----------------------------------------------------------------------------

// note that drag and drop is not supported by wxX11

#if wxUSE_DRAG_AND_DROP

// derive a simple class for handling dropped files
class DnDFile : public wxFileDropTarget
{
public:
   DnDFile() {}
   virtual bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames);
};

bool DnDFile::OnDropFiles(wxCoord, wxCoord, const wxArrayString& filenames)
{
   if (mainptr->generating) return false;

   // is there a wx call to bring app to front???
   #ifdef __WXMAC__
      ProcessSerialNumber process;
      if ( GetCurrentProcess(&process) == noErr )
         SetFrontProcess(&process);
   #endif
   #ifdef __WXMSW__
      SetForegroundWindow( (HWND)mainptr->GetHandle() );
   #endif
   mainptr->Raise();
   
   size_t numfiles = filenames.GetCount();
   for ( size_t n = 0; n < numfiles; n++ ) {
      mainptr->OpenFile(filenames[n]);
   }

   #ifdef __WXMAC__
      // need to call Refresh a bit later to remove colored frame on Mac
      onetimer->Start(10, wxTIMER_ONE_SHOT);
      // OnOneTimer will be called once after a delay of 0.01 sec
   #endif
   
   return true;
}

#endif // wxUSE_DRAG_AND_DROP

// -----------------------------------------------------------------------------

void MainFrame::SetRandomFillPercentage()
{
   // update Random Fill menu item to show randomfill value
   wxMenuBar *mbar = GetMenuBar();
   if (mbar) {
      wxString randlabel;
      randlabel.Printf(_("Random Fill (%d%c)\tCtrl+5"), randomfill, '%');
      mbar->SetLabel(ID_RANDOM, randlabel);
   }
}

// -----------------------------------------------------------------------------

void MainFrame::UpdateLayerItem(int index)
{
   // update name in given layer's menu item
   wxMenuBar *mbar = GetMenuBar();
   if (mbar) {
      wxString label;
      label.Printf(_("%d: "), index);
      
      int cid = GetLayer(index)->cloneid;
      while (cid > 0) {
         // display one or more "=" chars to indicate this is a cloned layer
         label += wxT('=');
         cid--;
      }
      
      label += GetLayer(index)->currname;
      mbar->SetLabel(ID_LAYER0 + index, label);
   }
}

// -----------------------------------------------------------------------------

void MainFrame::AppendLayerItem()
{
   wxMenuBar *mbar = GetMenuBar();
   if (mbar) {
      wxMenu* layermenu = mbar->GetMenu( mbar->FindMenu(_("Layer")) );
      if (layermenu) {
         // no point setting the item name here because UpdateLayerItem
         // will be called very soon
         layermenu->AppendCheckItem(ID_LAYER0 + numlayers - 1, _("foo"));
      } else {
         Warning(_("Could not find Layer menu!"));
      }
   }
}

// -----------------------------------------------------------------------------

void MainFrame::RemoveLayerItem()
{
   wxMenuBar *mbar = GetMenuBar();
   if (mbar) {
      wxMenu* layermenu = mbar->GetMenu( mbar->FindMenu(_("Layer")) );
      if (layermenu) {
         layermenu->Delete(ID_LAYER0 + numlayers);
      } else {
         Warning(_("Could not find Layer menu!"));
      }
   }
}

// -----------------------------------------------------------------------------

void MainFrame::CreateMenus()
{
   wxMenu *fileMenu = new wxMenu();
   wxMenu *editMenu = new wxMenu();
   wxMenu *controlMenu = new wxMenu();
   wxMenu *viewMenu = new wxMenu();
   wxMenu *layerMenu = new wxMenu();
   wxMenu *helpMenu = new wxMenu();

   // create submenus
   wxMenu *plocSubMenu = new wxMenu();
   wxMenu *pmodeSubMenu = new wxMenu();
   wxMenu *cmodeSubMenu = new wxMenu();
   wxMenu *scaleSubMenu = new wxMenu();

   plocSubMenu->AppendCheckItem(ID_PL_TL, _("Top Left"));
   plocSubMenu->AppendCheckItem(ID_PL_TR, _("Top Right"));
   plocSubMenu->AppendCheckItem(ID_PL_BR, _("Bottom Right"));
   plocSubMenu->AppendCheckItem(ID_PL_BL, _("Bottom Left"));
   plocSubMenu->AppendCheckItem(ID_PL_MID, _("Middle"));

   pmodeSubMenu->AppendCheckItem(ID_PM_COPY, _("Copy"));
   pmodeSubMenu->AppendCheckItem(ID_PM_OR, _("Or"));
   pmodeSubMenu->AppendCheckItem(ID_PM_XOR, _("Xor"));

   cmodeSubMenu->AppendCheckItem(ID_DRAW, _("Draw\tF5"));
   cmodeSubMenu->AppendCheckItem(ID_SELECT, _("Select\tF6"));
   cmodeSubMenu->AppendCheckItem(ID_MOVE, _("Move\tF7"));
   cmodeSubMenu->AppendCheckItem(ID_ZOOMIN, _("Zoom In\tF8"));
   cmodeSubMenu->AppendCheckItem(ID_ZOOMOUT, _("Zoom Out\tF9"));

   scaleSubMenu->AppendCheckItem(ID_SCALE_1, _("1:1\tCtrl+1"));
   scaleSubMenu->AppendCheckItem(ID_SCALE_2, _("1:2\tCtrl+2"));
   scaleSubMenu->AppendCheckItem(ID_SCALE_4, _("1:4\tCtrl+4"));
   scaleSubMenu->AppendCheckItem(ID_SCALE_8, _("1:8\tCtrl+8"));
   scaleSubMenu->AppendCheckItem(ID_SCALE_16, _("1:16\tCtrl+6"));

   fileMenu->Append(wxID_NEW, _("New Pattern\tCtrl+N"));
   fileMenu->AppendSeparator();
   fileMenu->Append(wxID_OPEN, _("Open Pattern...\tCtrl+O"));
   fileMenu->Append(ID_OPEN_CLIP, _("Open Clipboard\tShift+Ctrl+O"));
   fileMenu->Append(ID_OPEN_RECENT, _("Open Recent"), patternSubMenu);
   fileMenu->AppendSeparator();
   fileMenu->AppendCheckItem(ID_SHOW_PATTERNS, _("Show Patterns\tCtrl+P"));
   fileMenu->Append(ID_PATTERN_DIR, _("Set Pattern Folder..."));
   fileMenu->AppendSeparator();
   fileMenu->Append(wxID_SAVE, _("Save Pattern...\tCtrl+S"));
   fileMenu->AppendCheckItem(ID_SAVE_XRLE, _("Save Extended RLE"));
   fileMenu->AppendSeparator();
   fileMenu->Append(ID_RUN_SCRIPT, _("Run Script..."));
   fileMenu->Append(ID_RUN_CLIP, _("Run Clipboard"));
   fileMenu->Append(ID_RUN_RECENT, _("Run Recent"), scriptSubMenu);
   fileMenu->AppendSeparator();
   fileMenu->AppendCheckItem(ID_SHOW_SCRIPTS, _("Show Scripts\tShift+Ctrl+P"));
   fileMenu->Append(ID_SCRIPT_DIR, _("Set Script Folder..."));
   fileMenu->AppendSeparator();
   #ifdef __WXMSW__
      // Windows doesn't support Ctrl+<non-alpha> menu shortcut, and best not to
      // use non-Ctrl shortcut because it can't be used when menu is disabled
      fileMenu->Append(wxID_PREFERENCES, _("Preferences..."));
   #else
      // on the Mac the Preferences item gets moved to the app menu
      fileMenu->Append(wxID_PREFERENCES, _("Preferences...\tCtrl+,"));
   #endif
   fileMenu->AppendSeparator();
   // on the Mac the Ctrl+Q is changed to Cmd-Q and the item is moved to the app menu
   #if wxCHECK_VERSION(2, 7, 1)
      fileMenu->Append(wxID_EXIT, wxGetStockLabel(wxID_EXIT));
   #else
      fileMenu->Append(wxID_EXIT, wxGetStockLabel(wxID_EXIT,true,_("Ctrl+Q")));
   #endif

   editMenu->Append(ID_CUT, _("Cut\tCtrl+X"));
   editMenu->Append(ID_COPY, _("Copy\tCtrl+C"));
   #ifdef __WXMSW__
      // avoid non-Ctrl shortcut because it can't be used when menu is disabled
      editMenu->Append(ID_CLEAR, _("Clear"));
      editMenu->Append(ID_OUTSIDE, _("Clear Outside"));
   #else
      editMenu->Append(ID_CLEAR, _("Clear\tDelete"));
      editMenu->Append(ID_OUTSIDE, _("Clear Outside\tShift+Delete"));
   #endif
   editMenu->AppendSeparator();
   editMenu->Append(ID_PASTE, _("Paste\tCtrl+V"));
   editMenu->Append(ID_PMODE, _("Paste Mode"), pmodeSubMenu);
   editMenu->Append(ID_PLOCATION, _("Paste Location"), plocSubMenu);
   editMenu->Append(ID_PASTE_SEL, _("Paste to Selection"));
   editMenu->AppendSeparator();
   editMenu->Append(ID_SELALL, _("Select All\tCtrl+A"));
   editMenu->Append(ID_REMOVE, _("Remove Selection\tCtrl+K"));
   editMenu->Append(ID_SHRINK, _("Shrink Selection"));
   // full label will be set later by SetRandomFillPercentage
   editMenu->Append(ID_RANDOM, _("Random Fill\tCtrl+5"));
   editMenu->Append(ID_FLIPUD, _("Flip Up-Down"));
   editMenu->Append(ID_FLIPLR, _("Flip Left-Right"));
   editMenu->Append(ID_ROTATEC, _("Rotate Clockwise"));
   editMenu->Append(ID_ROTATEA, _("Rotate Anticlockwise"));
   editMenu->AppendSeparator();
   editMenu->Append(ID_CMODE, _("Cursor Mode"), cmodeSubMenu);

   controlMenu->Append(ID_GO, _("Go\tCtrl+G"));
   #ifdef __WXMSW__
      // Windows doesn't support Ctrl+<non-alpha> menu shortcut, and best not to
      // use non-Ctrl shortcut because it can't be used when menu is disabled
      controlMenu->Append(ID_STOP, _("Stop"));
      controlMenu->Append(ID_NEXT, _("Next"));
      controlMenu->Append(ID_STEP, _("Next Step"));
   #else
      controlMenu->Append(ID_STOP, _("Stop\tCtrl+."));
      controlMenu->Append(ID_NEXT, _("Next\tSpace"));
      controlMenu->Append(ID_STEP, _("Next Step\tTab"));
   #endif
   controlMenu->Append(ID_RESET, _("Reset\tCtrl+R"));
   controlMenu->AppendSeparator();
   #ifdef __WXMSW__
      // Windows doesn't support Ctrl+<non-alpha> menu shortcut, and best not to
      // use non-Ctrl shortcut because it can't be used when menu is disabled
      controlMenu->Append(ID_FASTER, _("Faster"));
      controlMenu->Append(ID_SLOWER, _("Slower"));
   #else
      controlMenu->Append(ID_FASTER, _("Faster\tCtrl++"));
      controlMenu->Append(ID_SLOWER, _("Slower\tCtrl+-"));
   #endif
   controlMenu->AppendSeparator();
   controlMenu->AppendCheckItem(ID_AUTO, _("Auto Fit\tCtrl+T"));
   controlMenu->AppendCheckItem(ID_HASH, _("Use Hashing\tCtrl+U"));
   controlMenu->AppendCheckItem(ID_HYPER, _("Hyperspeed"));
   controlMenu->AppendCheckItem(ID_HINFO, _("Show Hash Info"));
   controlMenu->AppendSeparator();
   controlMenu->Append(ID_RULE, _("Rule..."));

   #ifdef __WXMAC__
      // F11 is a default activation key for Expose so use F1 instead
      viewMenu->Append(ID_FULL, _("Full Screen\tF1"));
   #else
      viewMenu->Append(ID_FULL, _("Full Screen\tF11"));
   #endif
   viewMenu->AppendSeparator();
   viewMenu->Append(ID_FIT, _("Fit Pattern\tCtrl+F"));
   viewMenu->Append(ID_FIT_SEL, _("Fit Selection\tShift+Ctrl+F"));
   viewMenu->Append(ID_MIDDLE, _("Middle\tCtrl+M"));
   viewMenu->Append(ID_RESTORE00, _("Restore Origin\tCtrl+9"));
   viewMenu->AppendSeparator();
   #ifdef __WXMSW__
      // Windows doesn't support Ctrl+<non-alpha> menu shortcut, and best not to
      // use non-Ctrl shortcut because it can't be used when menu is disabled
      viewMenu->Append(wxID_ZOOM_IN, _("Zoom In"));
      viewMenu->Append(wxID_ZOOM_OUT, _("Zoom Out"));
   #else
      viewMenu->Append(wxID_ZOOM_IN, _("Zoom In\tCtrl+]"));
      viewMenu->Append(wxID_ZOOM_OUT, _("Zoom Out\tCtrl+["));
   #endif
   viewMenu->Append(ID_SET_SCALE, _("Set Scale"), scaleSubMenu);
   viewMenu->AppendSeparator();
   #ifdef __WXMSW__
      // Windows doesn't support Ctrl+<non-alpha> menu shortcut, and best not to
      // use non-Ctrl shortcut because it can't be used when menu is disabled
      viewMenu->AppendCheckItem(ID_TOOL_BAR, _("Show Tool Bar"));
      viewMenu->AppendCheckItem(ID_LAYER_BAR, _("Show Layer Bar"));
      viewMenu->AppendCheckItem(ID_STATUS_BAR, _("Show Status Bar"));
   #else
      viewMenu->AppendCheckItem(ID_TOOL_BAR, _("Show Tool Bar\tCtrl+'"));
      viewMenu->AppendCheckItem(ID_LAYER_BAR, _("Show Layer Bar\tCtrl+\\"));
      viewMenu->AppendCheckItem(ID_STATUS_BAR, _("Show Status Bar\tCtrl+;"));
   #endif
   viewMenu->AppendCheckItem(ID_EXACT, _("Show Exact Numbers\tCtrl+E"));
   viewMenu->AppendCheckItem(ID_GRID, _("Show Grid Lines\tCtrl+L"));
   viewMenu->AppendCheckItem(ID_COLORS, _("Swap Cell Colors\tCtrl+B"));
   viewMenu->AppendCheckItem(ID_BUFF, _("Buffered"));
   viewMenu->AppendSeparator();
   viewMenu->Append(ID_INFO, _("Pattern Info\tCtrl+I"));

   layerMenu->Append(ID_ADD_LAYER, _("Add Layer"));
   layerMenu->Append(ID_CLONE, _("Clone Layer"));
   layerMenu->Append(ID_DUPLICATE, _("Duplicate Layer"));
   layerMenu->AppendSeparator();
   layerMenu->Append(ID_DEL_LAYER, _("Delete Layer"));
   layerMenu->Append(ID_DEL_OTHERS, _("Delete Other Layers"));
   layerMenu->AppendSeparator();
   layerMenu->Append(ID_MOVE_LAYER, _("Move Layer..."));
   layerMenu->Append(ID_NAME_LAYER, _("Name Layer..."));
   layerMenu->AppendSeparator();
   layerMenu->AppendCheckItem(ID_SYNC_VIEW, _("Synchronize Views"));
   layerMenu->AppendCheckItem(ID_SYNC_CURS, _("Synchronize Cursors"));
   layerMenu->AppendSeparator();
   layerMenu->AppendCheckItem(ID_STACK, _("Stack Layers"));
   layerMenu->AppendCheckItem(ID_TILE, _("Tile Layers"));
   layerMenu->AppendSeparator();
   layerMenu->AppendCheckItem(ID_LAYER0, _("0"));
   // UpdateLayerItem will soon change the above item name

   helpMenu->Append(ID_HELP_INDEX, _("Contents"));
   helpMenu->Append(ID_HELP_INTRO, _("Introduction"));
   helpMenu->Append(ID_HELP_TIPS, _("Hints and Tips"));
   helpMenu->Append(ID_HELP_SHORTCUTS, _("Shortcuts"));
   helpMenu->Append(ID_HELP_SCRIPTING, _("Scripting"));
   helpMenu->Append(ID_HELP_LEXICON, _("Life Lexicon"));
   helpMenu->AppendSeparator();
   helpMenu->Append(ID_HELP_FILE, _("File Menu"));
   helpMenu->Append(ID_HELP_EDIT, _("Edit Menu"));
   helpMenu->Append(ID_HELP_CONTROL, _("Control Menu"));
   helpMenu->Append(ID_HELP_VIEW, _("View Menu"));
   helpMenu->Append(ID_HELP_LAYER, _("Layer Menu"));
   helpMenu->Append(ID_HELP_HELP, _("Help Menu"));
   helpMenu->AppendSeparator();
   helpMenu->Append(ID_HELP_REFS, _("References"));
   helpMenu->Append(ID_HELP_PROBLEMS, _("Known Problems"));
   helpMenu->Append(ID_HELP_CHANGES, _("Changes"));
   helpMenu->Append(ID_HELP_CREDITS, _("Credits"));
   #ifndef __WXMAC__
      helpMenu->AppendSeparator();
   #endif
   // on the Mac the wxID_ABOUT item gets moved to the app menu
   helpMenu->Append(wxID_ABOUT, _("About Golly"));

   // create the menu bar and append menus
   wxMenuBar *menuBar = new wxMenuBar();
   if (menuBar == NULL) Fatal(_("Failed to create menu bar!"));
   menuBar->Append(fileMenu, _("&File"));
   menuBar->Append(editMenu, _("&Edit"));
   menuBar->Append(controlMenu, _("&Control"));
   menuBar->Append(viewMenu, _("&View"));
   menuBar->Append(layerMenu, _("&Layer"));
   menuBar->Append(helpMenu, _("&Help"));
   
   #ifdef __WXMAC__
      // prevent Window menu being added automatically by wxMac 2.6.1+
      menuBar->SetAutoWindowMenu(false);
   #endif

   // attach menu bar to the frame
   SetMenuBar(menuBar);
}

// -----------------------------------------------------------------------------

void MainFrame::CreateToolbar()
{
   #ifdef __WXX11__
      // creating vertical tool bar stuffs up X11 menu bar!!!
      wxToolBar *toolBar = CreateToolBar(wxTB_FLAT | wxNO_BORDER | wxTB_HORIZONTAL);
   #elif defined(__WXGTK__)
      // create vertical tool bar at left edge of frame
      wxToolBar *toolBar = CreateToolBar(wxTB_VERTICAL);
   #else
      // create vertical tool bar at left edge of frame
      wxToolBar *toolBar = CreateToolBar(wxTB_FLAT | wxNO_BORDER | wxTB_VERTICAL);
   #endif
   
   #ifdef __WXMAC__
      // this results in a tool bar that is 32 pixels wide (matches STATUS_HT)
      toolBar->SetMargins(4, 8);
   #elif defined(__WXMSW__)
      // Windows seems to ignore *any* margins
      toolBar->SetMargins(0, 0);
   #else
      // X11/GTK tool bar looks better with these margins
      toolBar->SetMargins(2, 2);
   #endif

   toolBar->SetToolBitmapSize(wxSize(16, 16));

   tbBitmaps[go_index] = wxBITMAP(play);
   tbBitmaps[stop_index] = wxBITMAP(stop);
   tbBitmaps[new_index] = wxBITMAP(new);
   tbBitmaps[open_index] = wxBITMAP(open);
   tbBitmaps[save_index] = wxBITMAP(save);
   tbBitmaps[patterns_index] = wxBITMAP(patterns);
   tbBitmaps[scripts_index] = wxBITMAP(scripts);
   tbBitmaps[draw_index] = wxBITMAP(draw);
   tbBitmaps[sel_index] = wxBITMAP(select);
   tbBitmaps[move_index] = wxBITMAP(move);
   tbBitmaps[zoomin_index] = wxBITMAP(zoomin);
   tbBitmaps[zoomout_index] = wxBITMAP(zoomout);
   tbBitmaps[info_index] = wxBITMAP(info);
   tbBitmaps[hash_index] = wxBITMAP(hash);

   #ifdef __WXX11__
      // reduce update probs by using toggle buttons
      #define ADD_TOOL(id, bmp, tooltip) \
         toolBar->AddCheckTool(id, wxEmptyString, bmp, wxNullBitmap, tooltip)
   #else
      #define ADD_TOOL(id, bmp, tooltip) \
         toolBar->AddTool(id, wxEmptyString, bmp, tooltip)
   #endif

   #define ADD_RADIO(id, bmp, tooltip) \
      toolBar->AddRadioTool(id, wxEmptyString, bmp, wxNullBitmap, tooltip)
   
   #define ADD_CHECK(id, bmp, tooltip) \
      toolBar->AddCheckTool(id, wxEmptyString, bmp, wxNullBitmap, tooltip)

   ADD_TOOL(ID_GO, tbBitmaps[go_index], _("Start generating"));
   ADD_TOOL(ID_STOP, tbBitmaps[stop_index], _("Stop generating"));
   ADD_CHECK(ID_HASH, tbBitmaps[hash_index], _("Toggle hashing"));
   toolBar->AddSeparator();
   ADD_TOOL(wxID_NEW, tbBitmaps[new_index], _("New pattern"));
   ADD_TOOL(wxID_OPEN, tbBitmaps[open_index], _("Open pattern"));
   ADD_TOOL(wxID_SAVE, tbBitmaps[save_index], _("Save pattern"));
   toolBar->AddSeparator();
   ADD_CHECK(ID_SHOW_PATTERNS, tbBitmaps[patterns_index], _("Show/hide patterns"));
   ADD_CHECK(ID_SHOW_SCRIPTS, tbBitmaps[scripts_index], _("Show/hide scripts"));
   toolBar->AddSeparator();
   ADD_RADIO(ID_DRAW, tbBitmaps[draw_index], _("Draw"));
   ADD_RADIO(ID_SELECT, tbBitmaps[sel_index], _("Select"));
   ADD_RADIO(ID_MOVE, tbBitmaps[move_index], _("Move"));
   ADD_RADIO(ID_ZOOMIN, tbBitmaps[zoomin_index], _("Zoom in"));
   ADD_RADIO(ID_ZOOMOUT, tbBitmaps[zoomout_index], _("Zoom out"));
   toolBar->AddSeparator();
   ADD_TOOL(ID_INFO, tbBitmaps[info_index], _("Pattern information"));
   
   toolBar->Realize();
}

// -----------------------------------------------------------------------------

void MainFrame::CreateDirControls()
{
   patternctrl = new wxGenericDirCtrl(splitwin, wxID_ANY, wxEmptyString,
                                      wxDefaultPosition, wxDefaultSize,
                                      #ifdef __WXMSW__
                                         // speed up a bit
                                         wxDIRCTRL_DIR_ONLY | wxNO_BORDER,
                                      #else
                                         wxNO_BORDER,
                                      #endif
                                      wxEmptyString   // see all file types
                                     );
   if (patternctrl == NULL) Fatal(_("Failed to create pattern directory control!"));

   scriptctrl = new wxGenericDirCtrl(splitwin, wxID_ANY, wxEmptyString,
                                     wxDefaultPosition, wxDefaultSize,
                                     #ifdef __WXMSW__
                                        // speed up a bit
                                        wxDIRCTRL_DIR_ONLY | wxNO_BORDER,
                                     #else
                                        wxNO_BORDER,
                                     #endif
                                     // using *.py prevents seeing a folder alias or
                                     // sym link so best to show all files???
                                     _T("Python scripts|*.py")
                                    );
   if (scriptctrl == NULL) Fatal(_("Failed to create script directory control!"));
   
   #ifdef __WXMSW__
      // now remove wxDIRCTRL_DIR_ONLY so we see files
      patternctrl->SetWindowStyle(wxNO_BORDER);
      scriptctrl->SetWindowStyle(wxNO_BORDER);
   #endif

   #ifdef __WXGTK__
      // make sure background is white when using KDE's GTK theme
      patternctrl->GetTreeCtrl()->SetBackgroundStyle(wxBG_STYLE_COLOUR);
      scriptctrl->GetTreeCtrl()->SetBackgroundStyle(wxBG_STYLE_COLOUR);
      patternctrl->GetTreeCtrl()->SetBackgroundColour(*wxWHITE);
      scriptctrl->GetTreeCtrl()->SetBackgroundColour(*wxWHITE);
      // reduce indent a bit
      patternctrl->GetTreeCtrl()->SetIndent(8);
      scriptctrl->GetTreeCtrl()->SetIndent(8);
   #else
      // reduce indent a lot
      patternctrl->GetTreeCtrl()->SetIndent(4);
      scriptctrl->GetTreeCtrl()->SetIndent(4);
   #endif
   
   // reduce font size -- doesn't seem to reduce line height
   // wxFont font = *(statusptr->GetStatusFont());
   // patternctrl->GetTreeCtrl()->SetFont(font);
   // scriptctrl->GetTreeCtrl()->SetFont(font);
   
   if ( wxFileName::DirExists(patterndir) ) {
      // only show patterndir and its contents
      SimplifyTree(patterndir, patternctrl->GetTreeCtrl(), patternctrl->GetRootId());
   }
   if ( wxFileName::DirExists(scriptdir) ) {
      // only show scriptdir and its contents
      SimplifyTree(scriptdir, scriptctrl->GetTreeCtrl(), scriptctrl->GetRootId());
   }
}

// -----------------------------------------------------------------------------

// create the main window
MainFrame::MainFrame()
   : wxFrame(NULL, wxID_ANY, wxEmptyString, wxPoint(mainx,mainy), wxSize(mainwd,mainht))
{
   wxGetApp().SetFrameIcon(this);

   // initialize hidden files to be in same folder as Golly app;
   // they must be absolute paths in case they are used from a script command when
   // the current directory has been changed to the location of the script file
   scriptfile = gollydir + wxT(".golly_clip.py");
   clipfile = gollydir + wxT(".golly_clipboard");

   // create one-shot timer (see OnOneTimer)
   onetimer = new wxTimer(this, ID_ONE_TIMER);

   CreateMenus();
   CreateToolbar();

   int wd, ht;
   GetClientSize(&wd, &ht);
   // wd or ht might be < 1 on Win/X11 platforms
   if (wd < 1) wd = 1;
   if (ht < 1) ht = 1;

   // wxStatusBar can only appear at bottom of frame so we use our own
   // status bar class which creates a child window at top of frame
   int statht = showexact ? STATUS_EXHT : STATUS_HT;
   statusptr = new StatusBar(this, 0, 0, wd, statht);
   if (statusptr == NULL) Fatal(_("Failed to create status bar!"));
   
   // create a split window with pattern/script directory in left pane
   // and layer bar plus pattern viewport in right pane
   splitwin = new wxSplitterWindow(this, wxID_ANY,
                                   wxPoint(0, statht),
                                   wxSize(wd, ht - statht),
                                   #ifdef __WXMSW__
                                      wxSP_BORDER |
                                   #endif
                                   wxSP_3DSASH | wxSP_NO_XP_THEME | wxSP_LIVE_UPDATE);
   if (splitwin == NULL) Fatal(_("Failed to create split window!"));

   // create patternctrl and scriptctrl in left pane
   CreateDirControls();
   
   // create a window for right pane which contains layer bar and pattern viewport
   rightpane = new RightWindow(splitwin, 0, 0, wd, ht - statht);
   if (rightpane == NULL) Fatal(_("Failed to create right pane!"));
   
   // create layer bar and initial layer
   CreateLayerBar(rightpane);
   AddLayer();
   
   // create viewport at minimum size to avoid scroll bars being clipped on Mac
   viewptr = new PatternView(rightpane, 0, showlayer ? layerbarht : 0, 40, 40,
                             wxNO_BORDER |
                             wxWANTS_CHARS |              // receive all keyboard events
                             wxFULL_REPAINT_ON_RESIZE |
                             wxVSCROLL | wxHSCROLL);
   if (viewptr == NULL) Fatal(_("Failed to create viewport window!"));
   
   // this is the main viewport window (tile windows have a tileindex >= 0)
   viewptr->tileindex = -1;
   bigview = viewptr;
   
   #if wxUSE_DRAG_AND_DROP
      // let users drop files onto viewport
      viewptr->SetDropTarget(new DnDFile());
      //!!! what about tile windows???
   #endif
   
   // these seemingly redundant steps are needed to avoid problems on Windows
   splitwin->SplitVertically(patternctrl, rightpane, dirwinwd);
   splitwin->SetSashPosition(dirwinwd);
   splitwin->SetMinimumPaneSize(50);
   splitwin->Unsplit(patternctrl);
   splitwin->UpdateSize();

   splitwin->SplitVertically(scriptctrl, rightpane, dirwinwd);
   splitwin->SetSashPosition(dirwinwd);
   splitwin->SetMinimumPaneSize(50);
   splitwin->Unsplit(scriptctrl);
   splitwin->UpdateSize();

   if (showpatterns) splitwin->SplitVertically(patternctrl, rightpane, dirwinwd);
   if (showscripts) splitwin->SplitVertically(scriptctrl, rightpane, dirwinwd);

   InitDrawingData();      // do this after viewport size has been set

   generating = false;     // not generating pattern
   fullscreen = false;     // not in full screen mode
   showbanner = true;      // avoid first file clearing banner message
}

// -----------------------------------------------------------------------------

MainFrame::~MainFrame()
{
   delete onetimer;
   DestroyDrawingData();
}
