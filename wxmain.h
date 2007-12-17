                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2007 Andrew Trevorrow and Tomas Rokicki.

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
   void ShowPrefsDialog();

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

   // flags
   bool generating;           // currently generating pattern?
   bool fullscreen;           // in full screen mode?
   bool showbanner;           // showing banner message?

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

// static routines needed by GetPrefs() to get IDs for items in Open Recent and
// Run Recent submenus; they can't be MainFrame methods because GetPrefs() is
// called before the main window is created
int GetID_CLEAR_MISSING_PATTERNS();
int GetID_CLEAR_ALL_PATTERNS();
int GetID_OPEN_RECENT();
int GetID_CLEAR_MISSING_SCRIPTS();
int GetID_CLEAR_ALL_SCRIPTS();
int GetID_RUN_RECENT();

// static routines used to post commands to the event queue
int GetID_START();
int GetID_RESET();
int GetID_HASH();

#endif
