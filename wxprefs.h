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
#ifndef _WXPREFS_H_
#define _WXPREFS_H_

// Routines for getting, saving and changing user preferences:

// Read preferences from the GollyPrefs file.
void GetPrefs();

// Write preferences to the GollyPrefs file.
void SavePrefs();

// Open a modal dialog so user can change various preferences.
// Returns true if the user hits OK (so client can call SavePrefs).
bool ChangePrefs();

// Global preferences:

extern wxString gollydir;        // path of directory containing app

extern int mainx;                // main window's location
extern int mainy;
extern int mainwd;               // main window's size
extern int mainht;
extern bool maximize;            // maximize main window?

extern int helpx;                // help window's location
extern int helpy;
extern int helpwd;               // help window's size
extern int helpht;
extern int helpfontsize;         // font size in help window

extern int infox;                // info window's location
extern int infoy;
extern int infowd;               // info window's size
extern int infoht;

extern bool savexrle;            // save RLE file using XRLE format?
extern bool autofit;             // auto fit pattern while generating?
extern bool hashing;             // use hlife algorithm?
extern bool hyperspeed;          // use hyperspeed if supported by current algo?
extern bool showtool;            // show tool bar?
extern bool showstatus;          // show status bar?
extern bool showexact;           // show exact numbers in status bar?
extern bool showgridlines;       // display grid lines?
extern bool swapcolors;          // swap colors used for cell states?
extern bool buffered;            // use wxWdgets buffering to avoid flicker?
extern int randomfill;           // random fill percentage (1..100)
extern int maxhashmem;           // maximum hash memory (in megabytes)
extern int mingridmag;           // minimum mag to draw grid lines
extern int boldspacing;          // spacing of bold grid lines
extern bool showboldlines;       // show bold grid lines?
extern bool mathcoords;          // show Y values increasing upwards?
extern bool drawlayers;          // draw all layers?
extern bool genlayers;           // generate all layers?
extern int newmag;               // mag setting for new pattern
extern bool newremovesel;        // new pattern removes selection?
extern bool openremovesel;       // opening pattern removes selection?
extern wxCursor *newcurs;        // cursor after creating new pattern
extern wxCursor *opencurs;       // cursor after opening pattern
extern char initrule[];          // for first NewPattern before prefs saved
extern int mousewheelmode;       // 0:Ignore, 1:forward=ZoomOut, 2:forward=ZoomIn
extern int thumbrange;           // thumb box scrolling range in terms of view wd/ht
extern int qbasestep;            // qlife's base step
extern int hbasestep;            // hlife's base step (best if power of 2)
extern int mindelay;             // minimum millisec delay (when warp = -1)
extern int maxdelay;             // maximum millisec delay
extern wxString opensavedir;     // directory for Open and Save dialogs
extern wxString rundir;          // directory for Run Script dialog
extern wxString patterndir;      // directory used by Show Patterns
extern wxString scriptdir;       // directory used by Show Scripts
extern wxString pythonlib;       // name of Python library (loaded at runtime)
extern int dirwinwd;             // width of pattern/script directory window
extern bool showpatterns;        // show pattern directory?
extern bool showscripts;         // show script directory?
extern wxMenu *patternSubMenu;   // submenu of recent pattern files
extern wxMenu *scriptSubMenu;    // submenu of recent script files
extern int numpatterns;          // current number of recent pattern files
extern int numscripts;           // current number of recent script files
extern int maxpatterns;          // maximum number of recent pattern files
extern int maxscripts;           // maximum number of recent script files

// We maintain an array of named rules, where each string is of the form
// "name of rule|B.../S...".  The first string is always "Life|B3/S23".
extern wxArrayString namedrules;

// Colors:

extern wxColor *livergb;         // color for live cells
extern wxColor *deadrgb;         // color for dead cells
extern wxColor *pastergb;        // color for pasted pattern
extern wxColor *selectrgb;       // color for selected cells
extern wxColor *qlifergb;        // status bar background if not hashing
extern wxColor *hlifergb;        // status bar background if hashing

// colored brushes and pens
extern wxBrush *livebrush;       // for drawing live cells
extern wxBrush *deadbrush;       // for drawing dead cells
extern wxBrush *qlifebrush;      // for status bar background if not hashing
extern wxBrush *hlifebrush;      // for status bar background if hashing
extern wxPen *pastepen;          // for drawing paste rect
extern wxPen *gridpen;           // for drawing plain grid
extern wxPen *boldpen;           // for drawing bold grid
extern wxPen *sgridpen;          // for drawing plain grid if swapcolors is true
extern wxPen *sboldpen;          // for drawing bold grid if swapcolors is true

// update colors for brushes and pens
void SetBrushesAndPens();

// Various constants:

const int minmainwd = 200;       // main window's minimum width
const int minmainht = 100;       // main window's minimum height

const int minhelpwd = 400;       // help window's minimum width
const int minhelpht = 100;       // help window's minimum height

const int minfontsize = 6;       // minimum value of helpfontsize
const int maxfontsize = 30;      // maximum value of helpfontsize

const int mininfowd = 400;       // info window's minimum width
const int mininfoht = 100;       // info window's minimum height

const int MAX_RECENT = 100;      // maximum value of maxpatterns and maxscripts
const int MAX_SPACING = 1000;    // maximum value of boldspacing
const int MIN_HASHMB = 10;       // minimum value of maxhashmem
const int MAX_HASHMB = 4000;     // make bigger when hlifealgo is 64-bit clean
const int MAX_BASESTEP = 10000;  // maximum qbasestep or hbasestep
const int MAX_DELAY = 5000;      // maximum mindelay or maxdelay
const int MAX_THUMBRANGE = 500;  // maximum thumbrange
const int MIN_DIRWD = 50;        // minimum dirwinwd

// Following are used by GetPrefs() before the view window is created:

typedef enum {
   TopLeft, TopRight, BottomRight, BottomLeft, Middle
} paste_location;

typedef enum {
   Copy, Or, Xor
} paste_mode;

extern paste_location plocation; // location of cursor in paste rectangle
extern paste_mode pmode;         // logical paste mode

// get/set plocation
const char* GetPasteLocation();
void SetPasteLocation(const char *s);

// get/set pmode
const char* GetPasteMode();
void SetPasteMode(const char *s);

// Cursor modes:

extern wxCursor *curs_pencil;    // for drawing cells
extern wxCursor *curs_cross;     // for selecting cells
extern wxCursor *curs_hand;      // for moving view by dragging
extern wxCursor *curs_zoomin;    // for zooming in to a clicked cell
extern wxCursor *curs_zoomout;   // for zooming out from a clicked cell
extern wxCursor *currcurs;       // set to one of the above cursors

// convert given cursor to an index: 0 for curs_pencil, 1 for curs_cross, etc
int CursorToIndex(wxCursor *curs);

// convert given index to a cursor (NULL if i is not in 0..4)
wxCursor* IndexToCursor(int i);

#endif
