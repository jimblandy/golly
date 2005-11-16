                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2005 Andrew Trevorrow and Tomas Rokicki.

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
#include "bigint.h"        // for bigint
#include "lifealgo.h"      // for lifealgo

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
   void UpdateMenuItems(bool active);
   void UpdatePatternAndStatus();
   void UpdateStatus();

   // clipboard functions
   bool ClipboardHasText();
   bool CopyTextToClipboard(wxString &text);
   bool GetTextFromClipboard(wxTextDataObject *data);
   void OpenClipboard();

   // file functions
   void NewPattern();
   void CreateUniverse();
   void ConvertPathAndOpen(const char *path, bool convertUTF8);
   void OpenPattern();
   void SavePattern();
   
   // prefs functions
   void SetRandomFillPercentage();
   void SetMinimumWarp();
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
   int GetWarp();
   void SetWarp(int newwarp);
   void SetGenIncrement();

   // view functions
   bool StatusVisible();
   void ToggleStatusBar();
   void ToggleToolBar();
   void ToggleFullScreen();
   void ShowPatternInfo();

   // flags
   bool generating;              // currently generating pattern?
   bool fullscreen;              // in full screen mode?
   bool showbanner;              // showing banner message?
   bool nopattupdate;            // disable pattern updates?
   bool savestart;               // need to save starting pattern?

private:
   // any class wishing to process wxWidgets events must use this macro
   DECLARE_EVENT_TABLE()

   // event handlers
   void OnMenu(wxCommandEvent& event);
   void OnSetFocus(wxFocusEvent& event);
   void OnActivate(wxActivateEvent& event);
   void OnSize(wxSizeEvent& event);
   void OnOneTimer(wxTimerEvent& event);
   void OnClose(wxCloseEvent& event);

   // file functions
   void MySetTitle(const char *title);
   void SetWindowTitle(const char *filename);
   void LoadPattern(const char *newtitle);
   void SetCurrentFile(const char *path);
   const char* GetBaseName(const char *fullpath);
   void AddRecentFile(const char *path);
   void OpenRecentFile(int id);
   void ClearRecentFiles();

   // control functions
   void ChangeGoToStop();
   void ChangeStopToGo();
   bool SaveStartingPattern();
   void ResetPattern();
   void ShowRuleDialog();

   char currfile[4096];          // full path of current pattern file
   char currname[256];           // file name displayed in main window title
   
   int warp;                     // current speed setting
   int minwarp;                  // warp value at maximum delay (must be <= 0)
   long whentosee;               // when to do next gen (if warp < 0)
   long starttime, endtime;      // for timing info
   double startgen, endgen;      // ditto

   lifealgo *gen0algo;           // starting pattern
   char gen0rule[128];           // starting rule
   int gen0warp;                 // starting speed
   int gen0mag;                  // starting scale
   bigint gen0x, gen0y;          // starting location
   bool gen0hash;                // hashing was on at start?

   bool restorestatus;           // restore status bar at end of full screen mode?
   bool restoretoolbar;          // restore tool bar at end of full screen mode?

   wxToolBarToolBase *gotool;    // go button in tool bar
   wxToolBarToolBase *stoptool;  // stop button in tool bar
};

// temporary file for storing clipboard data
const char clipfile[] = ".golly_clipboard";

// static routines needed by GetPrefs() to get IDs for items in Open Recent submenu;
// can't be MainFrame methods because GetPrefs() is called before the main window
// is created -- what a kludge!
int GetID_RECENT_CLEAR();
int GetID_RECENT();

#endif
