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

#include "wx/dataobj.h"    // for wxTextDataObject
#include "wx/treectrl.h"   // for wxTreeEvent
#include "wx/splitter.h"   // for wxSplitterEvent
#include "bigint.h"        // for bigint

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
   int GetWarp();
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

   // flags
   bool generating;              // currently generating pattern?
   bool fullscreen;              // in full screen mode?
   bool showbanner;              // showing banner message?
   bool savestart;               // need to save starting pattern?

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

   // control functions
   void ChangeGoToStop();
   void ChangeStopToGo();
   bool SaveStartingPattern();
   void ShowRuleDialog();

   wxString currfile;            // full path of current pattern file
   wxString currname;            // file name displayed in main window title
   
   int warp;                     // current speed setting
   int minwarp;                  // warp value at maximum delay (must be <= 0)
   long whentosee;               // when to do next gen (if warp < 0)
   long begintime, endtime;      // for timing info
   double begingen, endgen;      // ditto

   wxString startfile;           // file for saving starting pattern
   wxString startrule;           // starting rule
   bigint startgen;              // starting generation (>= 0)
   bigint startx, starty;        // starting location
   int startwarp;                // starting speed
   int startmag;                 // starting scale
   bool starthash;               // hashing was on at start?
};

// name of temporary file for storing clipboard data
extern wxString clipfile;

// static routines needed by GetPrefs() to get IDs for items in Open Recent and
// Run Recent submenus; they can't be MainFrame methods because GetPrefs() is
// called before the main window is created -- what a kludge!
int GetID_CLEAR_PATTERNS();
int GetID_OPEN_RECENT();
int GetID_CLEAR_SCRIPTS();
int GetID_RUN_RECENT();

#endif
