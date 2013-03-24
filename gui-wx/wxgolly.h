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

#ifndef _WXGOLLY_H_
#define _WXGOLLY_H_

// need some forward declarations
class lifepoll;
class MainFrame;
class PatternView;
class StatusBar;

// Define our application class:

class GollyApp : public wxApp
{
public:
    // called on application startup
    virtual bool OnInit();
    
#ifdef __WXMAC__
    // called in response to an open-document event;
    // eg. when a file is dropped onto the app icon
    virtual void MacOpenFile(const wxString& fullPath);
#endif
    
    // put app icon in given frame
    void SetFrameIcon(wxFrame* frame);
    
    // event poller is used by non-wx modules to process events
    lifepoll* Poller();
    void PollerReset();
    void PollerInterrupt();
};

DECLARE_APP(GollyApp)            // so other files can use wxGetApp

// At the risk of offending C++ purists, we use some globals to simplify code:
// "mainptr->GetRect()" is more readable than "wxGetApp().GetMainFrame()->GetRect()".

extern MainFrame* mainptr;       // main window
extern PatternView* viewptr;     // current viewport window (possibly a tile)
extern PatternView* bigview;     // big viewport window (encloses all tiles)
extern StatusBar* statusptr;     // status bar window
extern wxStopWatch* stopwatch;   // global stopwatch (started in OnInit)
extern bool insideYield;         // processing an event via Yield()?

#endif
