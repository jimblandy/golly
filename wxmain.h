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
#ifndef _WXMAIN_H_
#define _WXMAIN_H_

#include "wx/splitter.h"   // for wxSplitterWindow, wxSplitterEvent
#include "wx/dirctrl.h"    // for wxGenericDirCtrl
#include "wx/treectrl.h"   // for wxTreeCtrl, wxTreeEvent
#include "wx/dataobj.h"    // for wxTextDataObject
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

   // clipboard functions
   bool ClipboardHasText();
   bool CopyTextToClipboard(wxString& text);
   bool GetTextFromClipboard(wxTextDataObject* data);
   void OpenClipboard();
   void RunClipboard();

   // file functions
   void OpenFile(const wxString& path, bool remember = true);
   void NewPattern(const wxString& title = _("untitled"));
   void CreateUniverse();
   void SetWindowTitle(const wxString& filename);
   void OpenPattern();
   void OpenScript();
   void ToggleShowPatterns();
   void ToggleShowScripts();
   void SavePattern();
   wxString SaveFile(const wxString& path, const wxString& format, bool remember);

   // prefs functions
   void SetRandomFillPercentage();
   void SetMinimumWarp();
   void UpdateWarp();
   void ShowPrefsDialog();

   // control functions
   void GeneratePattern();
   void GoFaster();
   void GoSlower();
   void StopGenerating();
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
   void ResetPattern();

   // view functions
   bool StatusVisible();
   void ToggleStatusBar();
   void ToggleExactNumbers();
   void ToggleToolBar();
   void ToggleFullScreen();
   void ShowPatternInfo();
   void ResizeSplitWindow();

   // layer functions
   void UpdateLayerItem(int index);
   void AppendLayerItem();
   void RemoveLayerItem();

   // flags
   bool generating;           // currently generating pattern?
   bool fullscreen;           // in full screen mode?
   bool showbanner;           // showing banner message?

   // temporary files
   wxString scriptfile;       // name of file created by RunClipboard
   wxString clipfile;         // name of file for storing clipboard data

private:
   // any class wishing to process wxWidgets events must use this macro
   DECLARE_EVENT_TABLE()

   // event handlers
   void OnMenu(wxCommandEvent& event);
   void OnButton(wxCommandEvent& event);
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
   bool LoadImage();
   void LoadPattern(const wxString& newtitle);
   void MySetTitle(const wxString& title);
   void SetCurrentFile(const wxString& path);
   wxString GetBaseName(const wxString& fullpath);
   void AddRecentPattern(const wxString& path);
   void OpenRecentPattern(int id);
   void ClearRecentPatterns();
   void ChangePatternDir();
   void AddRecentScript(const wxString& path);
   void OpenRecentScript(int id);
   void ClearRecentScripts();
   void ChangeScriptDir();
   const char *WritePattern(const wxString& path,
                            pattern_format format,
                            int top, int left, int bottom, int right);

   // control functions
   void ChangeGoToStop();
   void ChangeStopToGo();
   bool SaveStartingPattern();
   void ShowRuleDialog();

   // miscellaneous
   void CreateMenus();
   void CreateToolbar();
   void CreateDirControls();
   void SimplifyTree(wxString &dir, wxTreeCtrl* treectrl, wxTreeItemId root);
   void DeselectTree(wxTreeCtrl* treectrl, wxTreeItemId root);
   
   // splittable window contains pattern/script directory and viewport
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
int GetID_CLEAR_PATTERNS();
int GetID_OPEN_RECENT();
int GetID_CLEAR_SCRIPTS();
int GetID_RUN_RECENT();

#endif
