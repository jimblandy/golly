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
#ifndef _WXPREFS_H_
#define _WXPREFS_H_

// Routines for getting, saving and changing user preferences:

void GetPrefs();
// Read preferences from the GollyPrefs file.

void SavePrefs();
// Write preferences to the GollyPrefs file.

bool ChangePrefs(const wxString& page);
// Open a modal dialog so user can change various preferences.
// Returns true if the user hits OK (so client can call SavePrefs).

// Global preference data:

extern wxString gollydir;        // path of directory containing app
extern int debuglevel;           // for displaying debug info if > 0

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

extern char initrule[];          // initial rule
extern bool inithash;            // initial layer uses hlife algorithm?
extern bool initautofit;         // initial autofit setting
extern bool inithyperspeed;      // initial hyperspeed setting
extern bool initshowhashinfo;    // initial showhashinfo setting
extern bool savexrle;            // save RLE file using XRLE format?
extern bool showtips;            // show button tips?
extern bool showtool;            // show tool bar?
extern bool showlayer;           // show layer bar?
extern bool showstatus;          // show status bar?
extern bool showexact;           // show exact numbers in status bar?
extern bool showgridlines;       // display grid lines?
extern bool swapcolors;          // swap colors used for cell states?
extern bool buffered;            // use wxWdgets buffering to avoid flicker?
extern bool scrollpencil;        // scroll if pencil cursor is dragged outside view?
extern bool scrollcross;         // scroll if cross cursor is dragged outside view?
extern bool scrollhand;          // scroll if hand cursor is dragged outside view?
extern bool allowundo;           // allow undo/redo?
extern int randomfill;           // random fill percentage
extern int opacity;              // percentage opacity of live cells in overlays
extern int tileborder;           // width of tiled window borders
extern int maxhashmem;           // maximum hash memory (in megabytes)
extern int mingridmag;           // minimum mag to draw grid lines
extern int boldspacing;          // spacing of bold grid lines
extern bool showboldlines;       // show bold grid lines?
extern bool mathcoords;          // show Y values increasing upwards?
extern bool syncviews;           // synchronize viewports?
extern bool synccursors;         // synchronize cursors?
extern bool stacklayers;         // stack all layers?
extern bool tilelayers;          // tile all layers?
extern bool askonnew;            // ask to save changes before creating new pattern?
extern bool askonload;           // ask to save changes before loading pattern file?
extern bool askondelete;         // ask to save changes before deleting layer?
extern bool askonquit;           // ask to save changes before quitting app?
extern int newmag;               // mag setting for new pattern
extern bool newremovesel;        // new pattern removes selection?
extern bool openremovesel;       // opening pattern removes selection?
extern wxCursor* newcurs;        // cursor after creating new pattern
extern wxCursor* opencurs;       // cursor after opening pattern
extern int mousewheelmode;       // 0:Ignore, 1:forward=ZoomOut, 2:forward=ZoomIn
extern int thumbrange;           // thumb box scrolling range in terms of view wd/ht
extern int qbasestep;            // qlife's base step
extern int hbasestep;            // hlife's base step (best if power of 2)
extern int mindelay;             // minimum millisec delay (when warp = -1)
extern int maxdelay;             // maximum millisec delay
extern wxString opensavedir;     // directory for Open and Save dialogs
extern wxString rundir;          // directory for Run Script dialog
extern wxString choosedir;       // directory used by Choose File button
extern wxString patterndir;      // directory used by Show Patterns
extern wxString scriptdir;       // directory used by Show Scripts
extern wxString perllib;         // name of Perl library (loaded at runtime)
extern wxString pythonlib;       // name of Python library (loaded at runtime)
extern int dirwinwd;             // width of pattern/script directory window
extern bool showpatterns;        // show pattern directory?
extern bool showscripts;         // show script directory?
extern wxMenu* patternSubMenu;   // submenu of recent pattern files
extern wxMenu* scriptSubMenu;    // submenu of recent script files
extern int numpatterns;          // current number of recent pattern files
extern int numscripts;           // current number of recent script files
extern int maxpatterns;          // maximum number of recent pattern files
extern int maxscripts;           // maximum number of recent script files

extern wxArrayString namedrules;
// We maintain an array of named rules, where each string is of the form
// "name of rule|B.../S...".  The first string is always "Life|B3/S23".

// Keyboard shortcuts:

// define the actions that can be invoked by various key combinations
typedef enum {
   DO_NOTHING = 0,               // null action must be zero
   DO_OPENFILE,                  // open a chosen pattern/script/html file
   // rest are in alphabetical order
   DO_ABOUT,                     // about Golly
   DO_ADD,                       // add layer
   DO_ADVANCEOUT,                // advance outside
   DO_ADVANCE,                   // advance selection
   DO_AUTOFIT,                   // auto fit
   DO_BUFFERED,                  // buffered
   DO_CHANGE00,                  // change origin
   DO_CLEAROUT,                  // clear outside
   DO_CLEAR,                     // clear selection
   DO_CLONE,                     // clone layer
   DO_COPY,                      // copy selection
   DO_CURSDRAW,                  // cursor mode: draw
   DO_CURSMOVE,                  // cursor mode: move
   DO_CURSSEL,                   // cursor mode: select
   DO_CURSIN,                    // cursor mode: zoom in
   DO_CURSOUT,                   // cursor mode: zoom out
   DO_CUT,                       // cut selection
   DO_CURSCYCLE,                 // cycle cursor mode
   DO_PASTELOC,                  // cycle paste location
   DO_PASTEMODE,                 // cycle paste mode
   DO_DELETE,                    // delete layer
   DO_DELOTHERS,                 // delete other layers
   DO_DISABLE,                   // disable undo/redo
   DO_DUPLICATE,                 // duplicate layer
   DO_FASTER,                    // faster
   DO_FIT,                       // fit pattern
   DO_FITSEL,                    // fit selection
   DO_FLIPLR,                    // flip left-right
   DO_FLIPTB,                    // flip top-bottom
   DO_FULLSCREEN,                // full screen
   DO_HYPER,                     // hyperspeed
   DO_MIDDLE,                    // middle
   DO_MOVELAYER,                 // move layer...
   DO_NAMELAYER,                 // name layer...
   DO_NEWPATT,                   // new pattern
   DO_NEXTGEN,                   // next generation
   DO_NEXTSTEP,                  // next step
   DO_OPENCLIP,                  // open clipboard
   DO_OPENPATT,                  // open pattern...
   DO_PASTE,                     // paste
   DO_PASTESEL,                  // paste to selection
   DO_INFO,                      // pattern info
   DO_PREFS,                     // preferences...
   DO_QUIT,                      // quit Golly
   DO_RANDFILL,                  // random fill
   DO_REDO,                      // redo
   DO_REMOVESEL,                 // remove selection
   DO_RESET,                     // reset
   DO_RESTORE00,                 // restore origin
   DO_ROTATEACW,                 // rotate anticlockwise
   DO_ROTATECW,                  // rotate clockwise
   DO_RUNCLIP,                   // run clipboard
   DO_RUNSCRIPT,                 // run script...
   DO_SAVEXRLE,                  // save extended rle
   DO_SAVE,                      // save pattern...
   DO_DOWN,                      // scroll down
   DO_LEFT,                      // scroll left
   DO_RIGHT,                     // scroll right
   DO_UP,                        // scroll up
   DO_SELALL,                    // select all
   DO_SETGEN,                    // set generation...
   DO_PATTDIR,                   // set pattern folder...
   DO_RULE,                      // set rule...
   DO_SCALE1,                    // set scale 1:1
   DO_SCALE2,                    // set scale 1:2
   DO_SCALE4,                    // set scale 1:4
   DO_SCALE8,                    // set scale 1:8
   DO_SCALE16,                   // set scale 1:16
   DO_SCRIPTDIR,                 // set script folder...
   DO_SHOWEXACT,                 // show exact numbers
   DO_SHOWGRID,                  // show grid lines
   DO_HASHINFO,                  // show hash info
   DO_HELP,                      // show help
   DO_SHOWLAYER,                 // show layer bar
   DO_PATTERNS,                  // show patterns
   DO_SCRIPTS,                   // show scripts
   DO_SHOWSTATUS,                // show status bar
   DO_TIMING,                    // show timing
   DO_SHOWTOOL,                  // show tool bar
   DO_SHRINKFIT,                 // shrink and fit
   DO_SHRINK,                    // shrink selection
   DO_SLOWER,                    // slower
   DO_STACK,                     // stack layers
   DO_STARTSTOP,                 // start/stop generating
   DO_SWAPCOLORS,                // swap cell colors
   DO_SYNCCURS,                  // synchronize cursors
   DO_SYNCVIEWS,                 // synchronize views
   DO_TILE,                      // tile layers
   DO_UNDO,                      // undo
   DO_HASHING,                   // use hashing
   DO_ZOOMIN,                    // zoom in
   DO_ZOOMOUT,                   // zoom out
   MAX_ACTIONS
} action_id;

typedef struct {
   action_id id;                 // one of the above
   wxString file;                // non-empty if id is DO_OPENFILE
} action_info;

action_info FindAction(int key, int modifiers);
// return the action info for the given key and modifier set

wxString GetAccelerator(action_id action);
// return a string, possibly empty, containing the menu item
// accelerator(s) for the given action

void SetAccelerator(wxMenuBar* mbar, int item, action_id action);
// update accelerator for given menu item using given action

wxString GetShortcutTable();
// return HTML data to display current keyboard shortcuts

// Colors:

extern wxColor* livergb[10];     // color for live cells in each layer
extern wxColor* deadrgb;         // color for dead cells
extern wxColor* pastergb;        // color for pasted pattern
extern wxColor* selectrgb;       // color for selected cells
extern wxColor* qlifergb;        // status bar background if using qlife
extern wxColor* hlifergb;        // status bar background if using hlife

// colored brushes and pens
extern wxBrush* livebrush[10];   // for drawing live cells in each layer
extern wxBrush* deadbrush;       // for drawing dead cells
extern wxBrush* qlifebrush;      // for status bar background if using qlife
extern wxBrush* hlifebrush;      // for status bar background if using hlife
extern wxPen* pastepen;          // for drawing paste rect
extern wxPen* gridpen;           // for drawing plain grid
extern wxPen* boldpen;           // for drawing bold grid
extern wxPen* sgridpen[10];      // for drawing plain grid if swapcolors is true
extern wxPen* sboldpen[10];      // for drawing bold grid if swapcolors is true

void SetBrushesAndPens();        // update colors for brushes and pens

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
const int MAX_HASHMB =           // maximum value of maxhashmem
            sizeof(char*) <= 4 ? 4000 : 100000000;
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
void SetPasteLocation(const char* s);

// get/set pmode
const char* GetPasteMode();
void SetPasteMode(const char* s);

// Cursor modes:

extern wxCursor* curs_pencil;    // for drawing cells
extern wxCursor* curs_cross;     // for selecting cells
extern wxCursor* curs_hand;      // for moving view by dragging
extern wxCursor* curs_zoomin;    // for zooming in to a clicked cell
extern wxCursor* curs_zoomout;   // for zooming out from a clicked cell

int CursorToIndex(wxCursor* curs);
// convert given cursor to an index: 0 for curs_pencil, 1 for curs_cross, etc

wxCursor* IndexToCursor(int i);
// convert given index to a cursor (NULL if i is not in 0..4)

#endif
