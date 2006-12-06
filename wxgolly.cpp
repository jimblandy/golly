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

/* A GUI for Golly, implemented in wxWidgets (www.wxwidgets.org).
   Unfinished code is flagged by "!!!".
   Uncertain code is flagged by "???".
*/

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
   #include "wx/wx.h"      // for all others include the necessary headers
#endif

#include "wx/image.h"      // for wxImage
#include "wx/stdpaths.h"   // for wxStandardPaths
#include "wx/sysopt.h"     // for wxSystemOptions
#include "wx/filename.h"   // for wxFileName

#include "lifepoll.h"
#include "util.h"          // for lifeerrors

#include "wxgolly.h"       // defines GollyApp class
#include "wxmain.h"        // defines MainFrame class
#include "wxstatus.h"      // defines StatusBar class
#include "wxview.h"        // defines PatternView class
#include "wxutils.h"       // for Warning, Fatal, BeginProgress, etc
#include "wxprefs.h"       // for GetPrefs, gollydir
#include "wxscript.h"      // for IsScript

#ifdef __WXMSW__
   // app icons are loaded via .rc file
#else
   #include "appicon.xpm"
#endif

// -----------------------------------------------------------------------------

// Create a new application object: this macro will allow wxWidgets to create
// the application object during program execution and also implements the
// accessor function wxGetApp() which will return the reference of the correct
// type (i.e. GollyApp and not wxApp).

IMPLEMENT_APP(GollyApp)

// -----------------------------------------------------------------------------

#define STRINGIFY(arg) STR2(arg)
#define STR2(arg) #arg

MainFrame *mainptr = NULL;       // main window
PatternView *viewptr = NULL;     // viewport child window (in main window)
StatusBar *statusptr = NULL;     // status bar child window (in main window)
wxStopWatch *stopwatch;          // global stopwatch

// -----------------------------------------------------------------------------

// let non-wx modules call Fatal, Warning, BeginProgress, etc

class wx_errors : public lifeerrors
{
public:
   virtual void fatal(const char *s) {
      Fatal(wxString(s,wxConvLocal));
   }
   virtual void warning(const char *s) {
      Warning(wxString(s,wxConvLocal));
   }
   virtual void status(const char *s) {
      statusptr->DisplayMessage(wxString(s,wxConvLocal));
   }
   virtual void beginprogress(const char *s) {
      BeginProgress(wxString(s,wxConvLocal));
      // init flag for isaborted() calls in non-wx modules
      aborted = false;
   }
   virtual bool abortprogress(double f, const char *s) {
      return AbortProgress(f, wxString(s,wxConvLocal));
   }
   virtual void endprogress() {
      EndProgress();
   }
};

wx_errors wxerrhandler;    // create instance

// -----------------------------------------------------------------------------

// let non-wx modules process events

class wx_poll : public lifepoll
{
public:
   virtual int checkevents();
   virtual void updatePop();
   long nextcheck;
};

void CallYield()
{
   if (mainptr->IsActive()) {
      // make sure viewport keeps keyboard focus
      viewptr->SetFocus();
   }
   wxGetApp().Yield(true);
}

int wx_poll::checkevents()
{
   #ifdef __WXMSW__
      // on Windows it seems that Time has a higher overhead than Yield
      CallYield();
   #else
      // on Mac/Linux it is much faster to avoid calling Yield too often
      long t = stopwatch->Time();
      if (t > nextcheck) {
         nextcheck = t + 100;        // 10 times per sec
         #ifdef __WXX11__
            wxGetApp().Yield(true);            
         #else
            CallYield();
         #endif
      }
   #endif
   return isInterrupted();
}

void wx_poll::updatePop()
{
   if (mainptr->StatusVisible()) {
      statusptr->Refresh(false, NULL);
      statusptr->Update();
   }
}

wx_poll wxpoller;    // create instance

lifepoll* GollyApp::Poller()
{
   return &wxpoller;
}

void GollyApp::PollerReset()
{
   wxpoller.resetInterrupted();
   wxpoller.nextcheck = 0;
}

void GollyApp::PollerInterrupt()
{
   wxpoller.setInterrupted();
   wxpoller.nextcheck = 0;
}

// -----------------------------------------------------------------------------

void SetAppDirectory(const char *argv0)
{
   #ifdef __WXMSW__
      // on Windows we need to reset current directory to app directory if user
      // dropped file from somewhere else onto app to start it up (otherwise we
      // can't find Help files and prefs file gets saved to wrong location)
      wxStandardPaths wxstdpaths;
      wxString appdir = wxstdpaths.GetDataDir();
      wxString currdir = wxGetCwd();
      if ( currdir.CmpNoCase(appdir) != 0 )
         wxSetWorkingDirectory(appdir);
      // avoid VC++ warning
      wxUnusedVar(argv0);
   #elif defined(__WXMAC__)
      // wxMac has set current directory to location of .app bundle so no need
      // to do anything
   #elif defined(__UNIX__)
      // user might have started app from a different directory so find
      // last "/" in argv0 and change cwd if "/" isn't part of "./" prefix
      unsigned int pos = strlen(argv0);
      while (pos > 0) {
         pos--;
         if (argv0[pos] == '/') break;
      }
      if ( pos > 0 && !(pos == 1 && argv0[0] == '.') ) {
         char appdir[2048];
         if (pos < sizeof(appdir)) {
            strncpy(appdir, argv0, pos);
            appdir[pos] = 0;
            wxSetWorkingDirectory(wxString(appdir,wxConvLocal));
         }
      }
   #endif
}

// -----------------------------------------------------------------------------

void GollyApp::SetFrameIcon(wxFrame *frame)
{
   // set frame icon
   #ifdef __WXMSW__
      // create a bundle with 32x32 and 16x16 icons
      wxIconBundle icb(wxICON(appicon0));
      icb.AddIcon(wxICON(appicon1));
      frame->SetIcons(icb);
   #else
      // use appicon.xpm on other platforms (ignored on Mac)
      frame->SetIcon(wxICON(appicon));
   #endif
}

// -----------------------------------------------------------------------------

#ifdef __WXMAC__
// handle odoc event
void GollyApp::MacOpenFile(const wxString &fullPath)
{
   if (mainptr->generating) return;
   mainptr->Raise();
   mainptr->OpenFile(fullPath);
}
#endif

// -----------------------------------------------------------------------------

// app execution starts here
bool GollyApp::OnInit()
{
   SetAppName(_("Golly"));    // for use in Warning/Fatal dialogs

   // create a stopwatch so we can use Time() to get elapsed millisecs
   stopwatch = new wxStopWatch();

   #if defined(__WXMAC__) && !wxCHECK_VERSION(2,7,2)
      // prevent rectangle animation when windows open/close
      wxSystemOptions::SetOption(wxMAC_WINDOW_PLAIN_TRANSITION, 1);
      // prevent position problem in wxTextCtrl with wxTE_DONTWRAP style
      // (but doesn't fix problem with I-beam cursor over scroll bars)
      wxSystemOptions::SetOption(wxMAC_TEXTCONTROL_USE_MLTE, 1);
   #endif

   // get current working directory before calling SetAppDirectory
   wxString initdir = wxFileName::GetCwd();
   if (initdir.Last() != wxFILE_SEP_PATH) initdir += wxFILE_SEP_PATH;

   // make sure current working directory contains application otherwise
   // we can't open Help files and prefs file gets saved in wrong location
   SetAppDirectory( wxString(argv[0]).mb_str(wxConvLocal) );

   // now set global gollydir for use in GetPrefs and elsewhere
   gollydir = wxFileName::GetCwd();
   if (gollydir.Last() != wxFILE_SEP_PATH) gollydir += wxFILE_SEP_PATH;
   
   // let non-wx modules call Fatal, Warning, BeginProgress, etc
   lifeerrors::seterrorhandler(&wxerrhandler);

   // allow our .html files to include common graphic formats;
   // note that wxBMPHandler is always installed
   wxImage::AddHandler(new wxGIFHandler);
   wxImage::AddHandler(new wxPNGHandler);
   wxImage::AddHandler(new wxTIFFHandler);
   
   // get main window location and other user preferences
   GetPrefs();
   
   // create main window (also initializes viewptr and statusptr)
   mainptr = new MainFrame();
   if (mainptr == NULL) Fatal(_("Failed to create main window!"));
   
   // initialize some stuff before showing main window
   mainptr->SetRandomFillPercentage();
   mainptr->SetMinimumWarp();

   wxString banner = _("This is Golly version ");
   banner +=         _(STRINGIFY(VERSION));
   banner +=         _(".  Copyright 2006 The Golly Gang.");
   statusptr->SetMessage(banner);
   
   wxFileName filename;
   if (argc > 1) {
      // convert argv[1] to a full path if not one already; this allows users
      // to do things like "../golly bricklayer.py" from within Scripts folder
      filename = argv[1];
      if (!filename.IsAbsolute()) filename = initdir + argv[1];
   }
   
   // load pattern file if supplied on Win/Unix command line;
   // do before showing window to avoid an irritating flash
   if (argc > 1 && !IsScript(filename.GetFullPath())) {
      mainptr->OpenFile(filename.GetFullPath());
   } else {
      mainptr->NewPattern();
   }   

   if (maximize) mainptr->Maximize(true);
   if (!showstatus) mainptr->ToggleStatusBar();
   if (!showtool) mainptr->ToggleToolBar();

   // now show main window
   mainptr->Show(true);
   SetTopWindow(mainptr);

   // load script file AFTER showing main window to avoid crash in Win/X11 app
   if (argc > 1 && IsScript(filename.GetFullPath())) {
      mainptr->OpenFile(filename.GetFullPath());
   }

   #ifdef __WXX11__
      // prevent main window being resized very small to avoid nasty errors
      // mainptr->SetMinSize(wxSize(minmainwd, minmainht));
      // above works but moves window to default pos!!!
      // and calling Move clobbers effect of SetMinSize!!! sigh
      // Yield(true);
      // mainptr->Move(mainx, mainy);
   #endif

   // true means call wxApp::OnRun() which will enter the main event loop;
   // false means exit immediately
   return true;
}
