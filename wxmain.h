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
#ifndef _WXMAIN_H_
#define _WXMAIN_H_

#include "wx/splitter.h"   // for wxSplitterWindow, wxSplitterEvent
#include "wx/dirctrl.h"    // for wxGenericDirCtrl
#include "wx/treectrl.h"   // for wxTreeCtrl, wxTreeEvent
#include "wx/dataobj.h"    // for wxTextDataObject

#include "bigint.h"        // for bigint
#include "writepattern.h"  // for pattern_format
#include "wxprefs.h"       // for MAX_RECENT
#include "wxlayer.h"       // for MAX_LAYERS

// Define the main window:

class MainFrame : public wxFrame
{
public:
   MainFrame();
   ~MainFrame();

   // update functions
   void UpdateEverything();
   void UpdateUserInterface(bool active);
   void UpdateToolBar(bool active);
   void EnableAllMenus(bool enable);
   void UpdateMenuItems(bool active);
   void UpdatePatternAndStatus();
   void UpdateStatus();
   void UpdateMenuAccelerators();

   // clipboard functions
   bool ClipboardHasText();
   bool CopyTextToClipboard(wxString& text);
   bool GetTextFromClipboard(wxTextDataObject* data);
   void OpenClipboard();
   void RunClipboard();

   // file functions
   void OpenFile(const wxString& path, bool remember = true);
   void LoadPattern(const wxString& path, const wxString& newtitle,
                    bool updatestatus = true);
   void NewPattern(const wxString& title = _("untitled"));
   void CreateUniverse();
   void SetWindowTitle(const wxString& filename);
   void OpenPattern();
   void OpenScript();
   void ToggleShowPatterns();
   void ToggleShowScripts();
   void ChangePatternDir();
   void ChangeScriptDir();
   void SavePattern();
   bool SaveCurrentLayer();
   const char* SaveFile(const wxString& path, const wxString& format, bool remember);
   const char* WritePattern(const wxString& path,
                            pattern_format format,
                            int top, int left, int bottom, int right);
   #if wxUSE_DRAG_AND_DROP
      wxDropTarget* NewDropTarget();
   #endif
   
   // edit functions
   void ToggleAllowUndo();
   void RestorePattern(bigint& gen, const wxString& filename,
                       bigint& x, bigint& y, int mag, int warp);

   // prefs functions
   void SetRandomFillPercentage();
   void SetMinimumWarp();
   void UpdateWarp();
   void ShowPrefsDialog(const wxString& page = wxEmptyString);

   // control functions
   void GeneratePattern();
   void GoFaster();
   void GoSlower();
   void Stop();
   void DisplayTimingInfo();
   void NextGeneration(bool useinc);
   void AdvanceOutsideSelection();
   void AdvanceSelection();
   void ToggleAutoFit();
   void ToggleHashing();
   void ToggleHyperspeed();
   void ToggleHashInfo();
   void SetWarp(int newwarp);
   void SetGenIncrement();
   void ResetPattern(bool resetundo = true);
   void SetGeneration();
   const char* ChangeGenCount(const char* genstring, bool inundoredo = false);
   void ShowRuleDialog();

   // view functions
   void ToggleStatusBar();
   void ToggleExactNumbers();
   void ToggleToolBar();
   void ToggleFullScreen();
   void ShowPatternInfo();
   void ResizeSplitWindow(int wd, int ht);
   void ResizeStatusBar(int wd, int ht);
   wxWindow* RightPane();

   // layer functions
   void UpdateLayerItem(int index);
   void AppendLayerItem();
   void RemoveLayerItem();

   // miscellaneous functions
   void QuitApp();

   bool generating;           // currently generating pattern?
   bool fullscreen;           // in full screen mode?
   bool showbanner;           // showing banner message?
   
   bool command_pending;      // user selected a command while generating?
   bool draw_pending;         // user wants to draw while generating?
   bool keepmessage;          // don't clear message created by script?
   wxCommandEvent cmdevent;   // the pending command
   wxMouseEvent mouseevent;   // the pending draw

   // temporary files
   wxString clipfile;         // name of temporary file for storing clipboard data
   wxString perlfile;         // name of temporary Perl script
   wxString pythonfile;       // name of temporary Python script

   // store files passed via command line (processed in first OnIdle)
   wxArrayString pendingfiles;

private:
   // any class wishing to process wxWidgets events must use this macro
   DECLARE_EVENT_TABLE()

   // event handlers
   void OnMenu(wxCommandEvent& event);
   void OnSetFocus(wxFocusEvent& event);
   void OnActivate(wxActivateEvent& event);
   void OnSize(wxSizeEvent& event);
   void OnIdle(wxIdleEvent& event);
   void OnDirTreeExpand(wxTreeEvent& event);
   void OnDirTreeCollapse(wxTreeEvent& event);
   void OnDirTreeSelection(wxTreeEvent& event);
   void OnSashDblClick(wxSplitterEvent& event);
   void OnOneTimer(wxTimerEvent& event);
   void OnClose(wxCloseEvent& event);

   // file functions
   bool LoadImage(const wxString& path);
   void MySetTitle(const wxString& title);
   void SetCurrentFile(const wxString& path);
   wxString GetBaseName(const wxString& path);
   void SaveSucceeded(const wxString& path);
   void AddRecentPattern(const wxString& path);
   void OpenRecentPattern(int id);
   void ClearMissingPatterns();
   void ClearAllPatterns();
   void AddRecentScript(const wxString& path);
   void OpenRecentScript(int id);
   void ClearMissingScripts();
   void ClearAllScripts();
   wxString GetScriptFileName(const wxString& text);

   // control functions
   bool SaveStartingPattern();
   void DisplayPattern();

   // miscellaneous functions
   void CreateMenus();
   void CreateToolbar();
   void CreateDirControls();
   void SimplifyTree(wxString& dir, wxTreeCtrl* treectrl, wxTreeItemId root);
   void DeselectTree(wxTreeCtrl* treectrl, wxTreeItemId root);
   void DoPendingAction(bool restart);

   // splittable window contains pattern/script directory in left pane
   // and layer bar plus viewport window in right pane
   wxSplitterWindow* splitwin;
   wxGenericDirCtrl* patternctrl;
   wxGenericDirCtrl* scriptctrl;
   
   int minwarp;                  // warp value at maximum delay (must be <= 0)
   long whentosee;               // when to do next gen (if warp < 0)
   long begintime, endtime;      // for timing info
   double begingen, endgen;      // ditto
};

// ids for menu commands
enum {
   // File menu
   // wxID_NEW,
   // wxID_OPEN,
   ID_OPEN_CLIP = wxID_HIGHEST + 1,
   ID_OPEN_RECENT,
   // last 2 items in Open Recent submenu
   ID_CLEAR_MISSING_PATTERNS = ID_OPEN_RECENT + MAX_RECENT + 1,
   ID_CLEAR_ALL_PATTERNS,
   ID_SHOW_PATTERNS,
   ID_PATTERN_DIR,
   // wxID_SAVE,
   ID_SAVE_XRLE,
   ID_RUN_SCRIPT,
   ID_RUN_CLIP,
   ID_RUN_RECENT,
   // last 2 items in Run Recent submenu
   ID_CLEAR_MISSING_SCRIPTS = ID_RUN_RECENT + MAX_RECENT + 1,
   ID_CLEAR_ALL_SCRIPTS,
   ID_SHOW_SCRIPTS,
   ID_SCRIPT_DIR,
   // wxID_PREFERENCES,
   // wxID_EXIT,
   
   // Edit menu
   // wxID_UNDO,
   // wxID_REDO,
   // wxID_CUT,
   // wxID_COPY,
   ID_NO_UNDO,
   // wxID_CLEAR,
   ID_OUTSIDE,
   // wxID_PASTE,
   ID_PMODE,
   ID_PLOCATION,
   ID_PASTE_SEL,
   // wxID_SELECTALL,
   ID_REMOVE,
   ID_SHRINK,
   ID_RANDOM,
   ID_FLIPTB,
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
   ID_START,
   ID_NEXT,
   ID_STEP,
   ID_RESET,
   ID_SETGEN,
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
   ID_LAYER0,
   ID_LAYERMAX = ID_LAYER0 + MAX_LAYERS - 1,
   
   // Help menu
   ID_HELP_INDEX,
   ID_HELP_INTRO,
   ID_HELP_TIPS,
   ID_HELP_KEYBOARD,
   ID_HELP_MOUSE,
   ID_HELP_PERL,
   ID_HELP_PYTHON,
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
   
   // these ids don't have a menu item
   ID_LOAD_LEXICON,     // load lexicon pattern
   ID_SHOW_HELP         // for help button in tool bar
};

#endif
