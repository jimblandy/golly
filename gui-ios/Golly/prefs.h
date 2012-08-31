/*** /
 
 This file is part of Golly, a Game of Life Simulator.
 Copyright (C) 2012 Andrew Trevorrow and Tomas Rokicki.
 
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

#ifndef _PREFS_H_
#define _PREFS_H_

#include <string>       // for std::string
#include <list>         // for std::list

#include "utils.h"      // for gColor

// Routines for loading and saving user preferences:

void GetPrefs();
// Read preferences from the GollyPrefs file.

void SavePrefs();
// Write preferences to the GollyPrefs file.

// Various constants:

extern const char* GOLLY_VERSION;       // app version

const int minfontsize = 6;              // minimum value of helpfontsize
const int maxfontsize = 30;             // maximum value of helpfontsize

const int MAX_SPACING = 1000;           // maximum value of boldspacing
const int MAX_BASESTEP = 2000000000;    // maximum base step
const int MAX_DELAY = 5000;             // maximum mindelay or maxdelay
const int MAX_RECENT = 100;             // maximum value of maxpatterns
const int MIN_MEM_MB = 10;              // minimum value of maxhashmem
const int MAX_MEM_MB = 300;             // maximum value of maxhashmem???!!!

// Global paths:

extern std::string gollydir;        // path of directory containing app
extern std::string datadir;         // path of directory for user-specific data
extern std::string tempdir;         // path of directory for temporary data
extern std::string helpdir;         // path of directory for app's supplied help info
extern std::string patternsdir;     // path of directory for app's supplied patterns
extern std::string rulesdir;        // path of directory for app's supplied rule data
extern std::string userrules;       // path of directory for user's rule data
extern std::string downloaddir;     // path of directory for downloaded data
extern std::string clipfile;        // path of temporary file for storing clipboard data

// Global preference data:

extern int debuglevel;              // for displaying debug info if > 0
extern int helpfontsize;            // font size in help window
extern char initrule[];             // initial rule
extern bool initautofit;            // initial autofit setting
extern bool inithyperspeed;         // initial hyperspeed setting
extern bool initshowhashinfo;       // initial showhashinfo setting
extern bool savexrle;               // save RLE file using XRLE format?
extern bool showtool;               // show tool bar?
extern bool showlayer;              // show layer bar?
extern bool showedit;               // show edit bar?
extern bool showallstates;          // show all cell states in edit bar?
extern bool showstatus;             // show status bar?
extern bool showexact;              // show exact numbers in status bar?
extern bool showtimeline;           // show timeline bar?
extern bool showtiming;             // show timing messages?
extern bool showgridlines;          // display grid lines?
extern bool showicons;              // display icons for cell states?
extern bool swapcolors;             // swap colors used for cell states?
extern bool allowundo;              // allow undo/redo?
extern bool allowbeep;              // okay to play beep sound?
extern bool restoreview;            // should reset/undo restore view?
extern int canchangerule;           // if > 0 then paste can change rule
extern int randomfill;              // random fill percentage
extern int opacity;                 // percentage opacity of live cells in overlays
extern int tileborder;              // width of tiled window borders
extern int mingridmag;              // minimum mag to draw grid lines
extern int boldspacing;             // spacing of bold grid lines
extern bool showboldlines;          // show bold grid lines?
extern bool mathcoords;             // show Y values increasing upwards?
extern bool syncviews;              // synchronize viewports?
extern bool syncmodes;              // synchronize touch modes?
extern bool stacklayers;            // stack all layers?
extern bool tilelayers;             // tile all layers?
extern bool askonnew;               // ask to save changes before creating new pattern?
extern bool askonload;              // ask to save changes before loading pattern file?
extern bool askondelete;            // ask to save changes before deleting layer?
extern bool askonquit;              // ask to save changes before quitting app?
extern int newmag;                  // mag setting for new pattern
extern bool newremovesel;           // new pattern removes selection?
extern bool openremovesel;          // opening pattern removes selection?
extern int mindelay;                // minimum millisec delay
extern int maxdelay;                // maximum millisec delay
extern int maxhashmem;              // maximum memory (in MB) for hashlife-based algos

// Recent patterns:

extern int numpatterns;             // current number of recent pattern files
extern int maxpatterns;             // maximum number of recent pattern files
extern std::list<std::string> recentpatterns; // list of recent pattern files

// Colors:

extern gColor borderrgb;            // color for border around bounded grid
extern gColor selectrgb;            // color for selected cells
extern gColor pastergb;             // color for pattern to be pasted

// Paste info:

typedef enum {
    And, Copy, Or, Xor              // logical paste modes
} paste_mode;

extern paste_mode pmode;            // current paste mode

const char* GetPasteMode();         // get pmode
void SetPasteMode(const char* s);   // set pmode

#endif
