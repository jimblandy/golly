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

/* This module implements a GUI for Golly using wxWidgets (www.wxwidgets.org).
   Unfinished code is flagged by "!!!".
   Uncertain code is flagged by "???".

   Some key routines:
   MyApp::OnInit           - app execution starts here
   MainFrame::MainFrame    - creates main window
   MainFrame::OnMenu       - handles menu commands
   ProcessKey              - handles key presses
   ProcessClick            - handles mouse clicks
   RefreshWindow           - updates main window
   PatternView::OnPaint    - wxPaintEvent handler for viewport
   StatusBar::OnPaint      - wxPaintEvent handler for status bar
   GeneratePattern         - does pattern generation
   ShowHelp                - displays html files stored in Help folder
   SavePrefs               - saves user preferences
*/

// for compilers that support precompilation
#include "wx/wxprec.h"

// for all others, include the necessary headers
#ifndef WX_PRECOMP
   #include "wx/wx.h"
#endif

#include "wx/stockitem.h"     // for wxGetStockLabel
#include "wx/dcbuffer.h"      // for wxBufferedPaintDC
#include "wx/clipbrd.h"       // for wxTheClipboard
#include "wx/dataobj.h"       // for wxTextDataObject
#include "wx/file.h"          // for wxFile
#include "wx/dnd.h"           // for wxFileDropTarget
#include "wx/wxhtml.h"        // for wxHtmlWindow
#include "wx/image.h"         // for wxImage
#include "wx/stdpaths.h"      // for wxStandardPaths
#include "wx/progdlg.h"       // for wxProgressDialog
#include "wx/sysopt.h"        // for wxSystemOptions
#include "wx/ffile.h"         // for wxFFile

#ifdef __WXMSW__
   // app icons and tool bar bitmaps are loaded via .rc file
#else
   // application icon
   #include "appicon.xpm"
   // bitmap buttons for the tool bar
   #include "bitmaps/new.xpm"
   #include "bitmaps/open.xpm"
   #include "bitmaps/save.xpm"
   #include "bitmaps/play.xpm"
   #include "bitmaps/stop.xpm"
   #include "bitmaps/hash.xpm"
   #include "bitmaps/draw.xpm"
   #include "bitmaps/select.xpm"
   #include "bitmaps/move.xpm"
   #include "bitmaps/zoomin.xpm"
   #include "bitmaps/zoomout.xpm"
   #include "bitmaps/info.xpm"
   // cursors
   #ifdef __WXX11__
      // wxX11 doesn't support creating cursors from a bitmap file -- darn it!
   #else
      #include "bitmaps/zoomin_curs.xpm"
      #include "bitmaps/zoomout_curs.xpm"
   #endif
#endif

#include "lifealgo.h"
#include "hlifealgo.h"
#include "qlifealgo.h"
#include "liferender.h"
#include "viewport.h"
#include "readpattern.h"
#include "writepattern.h"
#include "lifepoll.h"
#include "util.h"       // for lifeerrors

#undef MYDEBUG          // can't use DEBUG on Mac; stuffs up #include <Carbon/Carbon.h>

#ifdef MYDEBUG
   #include <iostream>
#endif
using namespace std;

#ifdef __WXMAC__
   #include <Carbon/Carbon.h>
#endif

// -----------------------------------------------------------------------------

// define an application class derived from wxApp
class MyApp : public wxApp {
public:
   // called on application startup
   virtual bool OnInit();

   #ifdef __WXMAC__
      // called in response to an open-document event
      virtual void MacOpenFile(const wxString &fullPath);
   #endif
};

// Create a new application object: this macro will allow wxWidgets to create
// the application object during program execution and also implements the
// accessor function wxGetApp() which will return the reference of the correct
// type (i.e. MyApp and not wxApp).
IMPLEMENT_APP(MyApp)

// -----------------------------------------------------------------------------

// define the main window
class MainFrame : public wxFrame {
public:
   MainFrame();
   ~MainFrame();

private:
   // event handlers
   void OnSetFocus(wxFocusEvent& event);
   void OnMenu(wxCommandEvent& event);
   void OnActivate(wxActivateEvent& event);
   void OnSize(wxSizeEvent& event);
   void OnOneTimer(wxTimerEvent& event);
   void OnClose(wxCloseEvent& event);

   // any class wishing to process wxWidgets events must use this macro
   DECLARE_EVENT_TABLE()
};

// -----------------------------------------------------------------------------

// define a child window for viewing patterns
class PatternView : public wxWindow {
public:
    PatternView(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht);
    ~PatternView();

private:
   // event handlers
   void OnPaint(wxPaintEvent& event);
   void OnKeyDown(wxKeyEvent& event);
   void OnKeyUp(wxKeyEvent& event);
   void OnChar(wxKeyEvent& event);
   void OnMouseDown(wxMouseEvent& event);
   void OnMouseUp(wxMouseEvent& event);
   void OnMouseMotion(wxMouseEvent& event);
   void OnMouseEnter(wxMouseEvent& event);
   void OnMouseExit(wxMouseEvent& event);
   void OnDragTimer(wxTimerEvent& event);
   void OnScroll(wxScrollWinEvent& event);
   void OnEraseBackground(wxEraseEvent& event);

   // any class wishing to process wxWidgets events must use this macro
   DECLARE_EVENT_TABLE()
};

// -----------------------------------------------------------------------------

// define a child window for status bar (at top of frame)
class StatusBar : public wxWindow {
public:
    StatusBar(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht);
    ~StatusBar();

private:
   // event handlers
   void OnPaint(wxPaintEvent& event);
   void OnMouseDown(wxMouseEvent& event);
   void OnEraseBackground(wxEraseEvent& event);

   // any class wishing to process wxWidgets events must use this macro
   DECLARE_EVENT_TABLE()
};

// -----------------------------------------------------------------------------

// define a modeless help window
class HelpFrame : public wxFrame {
public:
   HelpFrame();
   ~HelpFrame() {}

private:
   // event handlers
   void OnBackButton(wxCommandEvent& event);
   void OnForwardButton(wxCommandEvent& event);
   void OnContentsButton(wxCommandEvent& event);
   void OnCloseButton(wxCommandEvent& event);
   void OnClose(wxCloseEvent& event);

   // any class wishing to process wxWidgets events must use this macro
   DECLARE_EVENT_TABLE()
};

// define a child window for displaying html info
class HtmlView : public wxHtmlWindow {
public:
   HtmlView(wxWindow *parent, wxWindowID id, const wxPoint& pos,
            const wxSize& size, long style)
      : wxHtmlWindow(parent, id, pos, size, style) { }
   virtual void OnLinkClicked(const wxHtmlLinkInfo& link);

private:
   void OnChar(wxKeyEvent& event);

   // any class wishing to process wxWidgets events must use this macro
   DECLARE_EVENT_TABLE()
};

// -----------------------------------------------------------------------------

// define a modeless window to display pattern info
class InfoFrame : public wxFrame {
public:
   InfoFrame(char *comments);
   ~InfoFrame() {}

private:
   // event handlers
   void OnCloseButton(wxCommandEvent& event);
   void OnClose(wxCloseEvent& event);

   // any class wishing to process wxWidgets events must use this macro
   DECLARE_EVENT_TABLE()
};

// define a child window for viewing comments
class TextView : public wxTextCtrl {
public:
   TextView(wxWindow *parent, wxWindowID id, const wxString& value,
            const wxPoint& pos, const wxSize& size, long style)
      : wxTextCtrl(parent, id, value, pos, size, style) { }

private:
   void OnChar(wxKeyEvent& event);
   void OnSetFocus(wxFocusEvent& event);

   // any class wishing to process wxWidgets events must use this macro
   DECLARE_EVENT_TABLE()
};

// -----------------------------------------------------------------------------

// IDs for controls and menu commands (other than standard wxID_* commands)
enum {
   // timers
   ID_DRAG_TIMER = wxID_HIGHEST,
   ID_ONE_TIMER,

   // buttons in help window
   ID_BACK_BUTT,
   ID_FORWARD_BUTT,
   ID_CONTENTS_BUTT,
   // wxID_CLOSE,

   // File menu (see also wxID_NEW, wxID_OPEN, wxID_SAVE)
   ID_OPENCLIP,
   
   // Edit menu
   ID_CUT,
   ID_COPY,
   ID_CLEAR,
   ID_PASTE,
   ID_PMODE,
   ID_PLOCATION,
   ID_PASTE_SEL,
   ID_SELALL,
   ID_REMOVE,
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
   ID_GO,
   ID_STOP,
   ID_NEXT,
   ID_STEP,
   ID_RESET,
   ID_FASTER,
   ID_SLOWER,
   ID_AUTO,
   ID_HASH,
   ID_HYPER,
   ID_MAXMEM,
   ID_RULE,
   
   // View menu (see also wxID_ZOOM_IN, wxID_ZOOM_OUT)
   ID_FIT,
   ID_MIDDLE,
   ID_FULL,
   ID_STATUS,
   ID_TOOL,
   ID_GRID,
   ID_VIDEO,
   ID_BUFF,
   ID_INFO,
   
   // Help menu
   ID_HELP_INDEX,
   ID_HELP_INTRO,
   ID_HELP_TIPS,
   ID_HELP_SHORTCUTS,
   ID_HELP_FILE,
   ID_HELP_EDIT,
   ID_HELP_CONTROL,
   ID_HELP_VIEW,
   ID_HELP_HELP,
   ID_HELP_REFS,
   ID_HELP_PROBLEMS,
   ID_HELP_CHANGES,
   ID_HELP_CREDITS
};

// -----------------------------------------------------------------------------

#define STRINGIFY(arg) STR2(arg)
#define STR2(arg) #arg
const char *BANNER = "This is Golly version " STRINGIFY(VERSION)
                     ".  Copyright 2005 The Golly Gang.";

lifealgo *curralgo = NULL;    // current life algorithm (qlife or hlife)
viewport currview(10, 10);    // current viewport for displaying patterns

MainFrame *frameptr;          // main window
PatternView *viewptr;         // viewport child window (in main window)
StatusBar *statusptr;         // status bar child window (in main window)
wxDC *currdc;                 // current device context
wxFont *statusfont;           // status bar font
wxBitmap *statbitmap;         // status bar bitmap
int statbitmapwd = -1;        // width of status bar bitmap
int statbitmapht = -1;        // height of status bar bitmap
HelpFrame *helpptr;           // help window
HtmlView *htmlwin;            // html child window (in help window and about box)
InfoFrame *infoptr;           // pattern info window
wxTimer *onetimer;            // one-shot timer
wxToolBarToolBase *gotool;    // go button in tool bar
wxToolBarToolBase *stoptool;  // stop button in tool bar

bool generating = false;      // currently generating?
bool autofit = false;         // auto fit pattern while generating?
bool hashing = false;         // use hlife algorithm?
bool hyperspeed = false;      // use hyperspeed if supported by current algo?
bool fullscreen = false;      // in full screen mode?
bool blackcells = true;       // live cells are black?
bool showgridlines = true;    // display grid lines?
bool buffered = true;         // use wxWdgets buffering to avoid flicker?
bool showbanner = true;       // avoid first file clearing BANNER message
bool nopattupdate = false;    // disable pattern updates?
bool restorestatus;           // restore status bar at end of full screen mode?
bool restoretoolbar;          // restore tool bar at end of full screen mode?

// status bar stuff
#define STATUS_HT (32)        // status bar height (enough for 2 lines)
#define BASELINE1 (12)        // baseline of 1st line
#define BASELINE2 (26)        // baseline of 2nd line
int h_gen;                    // horizontal position of "Generation"
int h_pop;                    // horizontal position of "Population"
int h_scale;                  // horizontal position of "Scale"
int h_step;                   // horizontal position of "Step"
int h_xy;                     // horizontal position of "X,Y"
int textascent;               // vertical adjustment used in DrawText calls
int statusht = STATUS_HT;     // status bar is initially visible
char statusmsg[256];          // for messages on 2nd line
double currx, curry;          // cursor location in cell coords
bool showxy = false;          // show cursor location?

// timing stuff
long starttime, endtime;
double startgen, endgen;
long whentosee;               // when to do next gen (if warp < 0)
long gendelay;                // delay in millisecs between each gen (if warp < 0)
int warp;                     // current speed setting
#define MIN_WARP (-4)         // determines maximum delay
#define MIN_DELAY (250)       // minimum millisec delay (when warp = -1)

// various colors
wxColour paleyellow = wxColour(0xFF,0xFF,0xCE);    // for status bar if not hashing
wxColour paleblue =   wxColour(0xE2,0xFA,0xF8);    // for status bar if hashing
wxColour ltgray =     wxColour(0xD0,0xD0,0xD0);    // for grid lines (blackcells true)
wxColour dkgray =     wxColour(0xA0,0xA0,0xA0);    // ditto
wxColour verydark =   wxColour(0x40,0x40,0x40);    // for grid lines (blackcells false)
wxColour notsodark =  wxColour(0x70,0x70,0x70);    // ditto
/* use *wxWHITE and *wxBLACK
wxColour white =      wxColour(0xFF,0xFF,0xFF);
wxColour black =      wxColour(0,0,0);
*/

// some pens for SetPen calls
wxPen *pen_ltgray;
wxPen *pen_dkgray;
wxPen *pen_verydark;
wxPen *pen_notsodark;

// some brushes for FillRect calls
wxBrush *brush_yellow;
wxBrush *brush_blue;
wxBrush *brush_dkgray;

// some cursors
wxCursor *currcurs;           // set to one of the following cursors
wxCursor *curs_pencil;
wxCursor *curs_cross;
wxCursor *curs_hand;
wxCursor *curs_zoomin;
wxCursor *curs_zoomout;

wxCursor *oldzoom = NULL;     // non-NULL if shift key has toggled zoom in/out cursor

// most editing and saving operations are limited to abs coords <= 10^9
// because getcell/setcell take int parameters (the limits must be smaller
// than INT_MIN and INT_MAX to avoid boundary conditions)
static bigint min_coord = -1000000000;
static bigint max_coord = +1000000000;
#define OutsideLimits(t, l, b, r) \
        ( t < min_coord || l < min_coord || b > max_coord || r > max_coord )

// editing stuff
int cellx, celly;             // current cell's 32-bit position
bigint bigcellx, bigcelly;    // current cell's position
int initselx, initsely;       // location of initial selection click
bool forceh;                  // resize selection horizontally?
bool forcev;                  // resize selection vertically?
bigint anchorx, anchory;      // anchor cell of current selection
bigint seltop;                // current edges of selection
bigint selbottom;
bigint selleft;
bigint selright;
bigint prevtop;               // previous edges of selection
bigint prevbottom;
bigint prevleft;
bigint prevright;
int drawstate;                // new cell state (0 or 1)
bool drawingcells = false;    // drawing cells due to dragging mouse?
bool selectingcells = false;  // selecting cells due to dragging mouse?
bool movingview = false;      // moving view due to dragging mouse?
wxTimer *dragtimer;           // timer used while dragging mouse
const int dragrate = 20;      // call OnDragTimer 50 times per sec
bool waitingforclick = false; // waiting for user to paste clipboard pattern?
int pastex, pastey;           // where user wants to paste clipboard pattern
wxRect pasterect;             // shows area to be pasted

// current paste location (ie. location of cursor in paste rectangle)
typedef enum {
   TopLeft, TopRight, BottomRight, BottomLeft, Middle
} paste_location;
paste_location plocation = TopLeft;

// current paste mode
typedef enum {
   Copy, Or, Xor
} paste_mode;
paste_mode pmode = Copy;

// wxX11's Blit doesn't support alpha channel
#ifndef __WXX11__
   wxImage selimage;          // semi-transparent overlay for selections
   wxBitmap *selbitmap;       // selection bitmap
   int selbitmapwd = -1;      // width of selection bitmap
   int selbitmapht = -1;      // height of selection bitmap
#endif

// file stuff
char currfile[4096];          // full path of current pattern file
char currname[256];           // file name displayed in main window title
wxString opensavedir;         // directory for open and save dialogs
wxString appdir;              // location of application

// temporary file for storing clipboard data
const char clipfile[] = ".golly_clipboard";

// also need a more permanent file created by OpenClipboard;
// it can be used to reset pattern or to show comments
const char gen0file[] = ".golly_gen0";

// globals for saving starting pattern
lifealgo *gen0algo = NULL;
char gen0rule[128];
bool gen0hash;
bool savestart = false;

// globals for showing progress
wxProgressDialog *progdlg = NULL;         // progress dialog
#ifdef __WXX11__
   const int maxprogrange = 10000;        // maximum range must be < 32K on X11?
#else
   const int maxprogrange = 1000000000;   // maximum range (best if very large)
#endif
long progstart;                           // starting time (in millisecs)
long prognext;                            // when to update progress dialog
char progtitle[128];                      // title for progress dialog

// -----------------------------------------------------------------------------

// Golly's preferences file is a simple text file created in the same directory
// as the application.  This makes backing up and uninstalling easy.

char prefsname[] = "GollyPrefs";
const int prefsversion = 1;
const int preflinesize = 5000;   // must be quite long for storing file paths
int mainx = 30;                  // main window's default location
int mainy = 30;
int mainwd = 640;                // main window's default size
int mainht = 480;
const int minmainwd = 200;       // main window's minimum size
const int minmainht = 100;
bool maximize = true;            // maximize main window?
int helpx = 60;                  // help window's default location
int helpy = 60;
int helpwd = 600;                // help window's default size
int helpht = 400;
const int minhelpwd = 400;       // help window's minimum size
const int minhelpht = 100;
int infox = 100;                 // info window's default location
int infoy = 100;
int infowd = 600;                // info window's default size
int infoht = 400;
const int mininfowd = 400;       // info window's minimum size
const int mininfoht = 100;
bool showstatus = true;          // show status bar?
bool showtool = true;            // show tool bar?
int maxhmem = 300;               // maximum hash memory (in megabytes)
const int minhashmb = 10;        // minimum value of maxhmem
const int maxhashmb = 4000;      // make bigger when hlifealgo is 64-bit clean???
char initrule[128] = "B3/S23";   // for first NewPattern before prefs saved

void Warning(const char *s);
void ToggleFullScreen();

const char *GetPasteLocation() {
   switch (plocation) {
      case TopLeft:     return "TopLeft";
      case TopRight:    return "TopRight";
      case BottomRight: return "BottomRight";
      case BottomLeft:  return "BottomLeft";
      case Middle:      return "Middle";
      default:          return "unknown";
   }
}

const char *GetPasteMode() {
   switch (pmode) {
      case Copy:  return "Copy";
      case Or:    return "Or";
      case Xor:   return "Xor";
      default:    return "unknown";
   }
}

void SavePrefs() {
   if (frameptr == NULL) {
      // probably called very early from Fatal, so best not to write prefs
      return;
   }
   FILE *f = fopen(prefsname, "w");
   if (f == 0) {
      Warning("Could not save preferences file!");
      return;
   }
   fprintf(f, "# NOTE: If you edit this file then do so when Golly isn't running\n");
   fprintf(f, "# otherwise all your changes will be clobbered when Golly quits.\n");
   fprintf(f, "version=%d\n", prefsversion);
   // save main window's location and size
   if (fullscreen) ToggleFullScreen();
   wxRect r = frameptr->GetRect();
   mainx = r.x;
   mainy = r.y;
   mainwd = r.width;
   mainht = r.height;
   fprintf(f, "main_window=%d,%d,%d,%d\n", mainx, mainy, mainwd, mainht);
   #ifdef __WXX11__
      // frameptr->IsMaximized() is always true on X11 so avoid it!!!
      fprintf(f, "maximize=%d\n", 0);
   #else
      fprintf(f, "maximize=%d\n", frameptr->IsMaximized() ? 1 : 0);
   #endif
   if (helpptr) {
      wxRect r = helpptr->GetRect();
      helpx = r.x;
      helpy = r.y;
      helpwd = r.width;
      helpht = r.height;
   }
   fprintf(f, "help_window=%d,%d,%d,%d\n", helpx, helpy, helpwd, helpht);
   if (infoptr) {
      wxRect r = infoptr->GetRect();
      infox = r.x;
      infoy = r.y;
      infowd = r.width;
      infoht = r.height;
   }
   fprintf(f, "info_window=%d,%d,%d,%d\n", infox, infoy, infowd, infoht);
   fprintf(f, "paste_location=%s\n", GetPasteLocation());
   fprintf(f, "paste_mode=%s\n", GetPasteMode());
   fprintf(f, "auto_fit=%d\n", autofit ? 1 : 0);
   fprintf(f, "hashing=%d\n", hashing ? 1 : 0);
   fprintf(f, "hyperspeed=%d\n", hyperspeed ? 1 : 0);
   fprintf(f, "max_hash_mem=%d\n", maxhmem);
   if (curralgo) fprintf(f, "rule=%s\n", curralgo->getrule());
   fprintf(f, "show_status=%d\n", statusht > 0 ? 1 : 0);
   fprintf(f, "show_tool=%d\n", frameptr->GetToolBar()->IsShown() ? 1 : 0);
   fprintf(f, "grid_lines=%d\n", showgridlines ? 1 : 0);
   fprintf(f, "black_on_white=%d\n", blackcells ? 1 : 0);
   fprintf(f, "buffered=%d\n", buffered ? 1 : 0);
   fprintf(f, "open_save_dir=%s\n", opensavedir.c_str());
   fclose(f);
}

bool GetKeyVal(FILE *f, char *line, char **keyword, char **value) {
   while ( fgets(line, preflinesize, f) != 0 ) {
      if ( line[0] == '#' || line[0] == '\n' ) {
         // skip comment line or empty line
      } else {
         // line should have format keyword=value
         *keyword = line;
         *value = line;
         while ( **value != '=' && **value != '\n' ) *value += 1;
         **value = 0;   // terminate keyword
         *value += 1;
         return true;
      }
   }
   return false;
}

void CheckVisibility(int *x, int *y, int *wd, int *ht) {
   wxRect maxrect = wxGetClientDisplayRect();
   // reset x,y if title bar isn't clearly visible
   if ( *y + 10 < maxrect.y || *y + 10 > maxrect.GetBottom() ||
        *x + 10 > maxrect.GetRight() || *x + *wd - 10 < maxrect.x ) {
      *x = wxDefaultCoord;
      *y = wxDefaultCoord;
   }
   // reduce wd,ht if too big for screen
   if (*wd > maxrect.width) *wd = maxrect.width;
   if (*ht > maxrect.height) *ht = maxrect.height;
}

void GetPrefs() {
   if ( wxFileExists(prefsname) ) {
      FILE *f = fopen(prefsname, "r");
      if (f == 0) {
         Warning("Could not read preferences file!");
         return;
      }
      char line[preflinesize];
      char *keyword;
      char *value;
      while ( GetKeyVal(f, line, &keyword, &value) ) {
         if (strcmp(keyword, "version") == 0) {
            int currversion;
            if (sscanf(value, "%d", &currversion) == 1 && currversion < prefsversion) {
               // may need to do something in the future if syntax changes
            }
         } else if (strcmp(keyword, "main_window") == 0) {
            sscanf(value, "%d,%d,%d,%d", &mainx, &mainy, &mainwd, &mainht);
            // avoid very small window -- can cause nasty probs on X11
            if (mainwd < minmainwd) mainwd = minmainwd;
            if (mainht < minmainht) mainht = minmainht;
            CheckVisibility(&mainx, &mainy, &mainwd, &mainht);
         } else if (strcmp(keyword, "maximize") == 0) {
            maximize = value[0] == '1';
         } else if (strcmp(keyword, "help_window") == 0) {
            sscanf(value, "%d,%d,%d,%d", &helpx, &helpy, &helpwd, &helpht);
            if (helpwd < minhelpwd) helpwd = minhelpwd;
            if (helpht < minhelpht) helpht = minhelpht;
            CheckVisibility(&helpx, &helpy, &helpwd, &helpht);
         } else if (strcmp(keyword, "info_window") == 0) {
            sscanf(value, "%d,%d,%d,%d", &infox, &infoy, &infowd, &infoht);
            if (infowd < mininfowd) infowd = mininfowd;
            if (infoht < mininfoht) infoht = mininfoht;
            CheckVisibility(&infox, &infoy, &infowd, &infoht);
         } else if (strcmp(keyword, "paste_location") == 0) {
            char val[16];
            sscanf(value, "%s\n", val);
            if (strcmp(val, "TopLeft") == 0) {
               plocation = TopLeft;
            } else if (strcmp(val, "TopRight") == 0) {
               plocation = TopRight;
            } else if (strcmp(val, "BottomRight") == 0) {
               plocation = BottomRight;
            } else if (strcmp(val, "BottomLeft") == 0) {
               plocation = BottomLeft;
            } else if (strcmp(val, "Middle") == 0) {
               plocation = Middle;
            }
         } else if (strcmp(keyword, "paste_mode") == 0) {
            char val[16];
            sscanf(value, "%s\n", val);
            if (strcmp(val, "Copy") == 0) {
               pmode = Copy;
            } else if (strcmp(val, "Or") == 0) {
               pmode = Or;
            } else if (strcmp(val, "Xor") == 0) {
               pmode = Xor;
            }
         } else if (strcmp(keyword, "auto_fit") == 0) {
            autofit = value[0] == '1';
         } else if (strcmp(keyword, "hashing") == 0) {
            hashing = value[0] == '1';
         } else if (strcmp(keyword, "hyperspeed") == 0) {
            hyperspeed = value[0] == '1';
         } else if (strcmp(keyword, "max_hash_mem") == 0) {
            sscanf(value, "%d", &maxhmem);
            if (maxhmem < minhashmb) maxhmem = minhashmb;
            if (maxhmem > maxhashmb) maxhmem = maxhashmb;
         } else if (strcmp(keyword, "rule") == 0) {
            sscanf(value, "%s\n", initrule);
         } else if (strcmp(keyword, "show_status") == 0) {
            showstatus = value[0] == '1';
         } else if (strcmp(keyword, "show_tool") == 0) {
            showtool = value[0] == '1';
         } else if (strcmp(keyword, "grid_lines") == 0) {
            showgridlines = value[0] == '1';
         } else if (strcmp(keyword, "black_on_white") == 0) {
            blackcells = value[0] == '1';
         } else if (strcmp(keyword, "buffered") == 0) {
            buffered = value[0] == '1';
         } else if (strcmp(keyword, "open_save_dir") == 0) {
            opensavedir = value;
            opensavedir.RemoveLast();  // remove \n
            if ( !wxFileName::DirExists(opensavedir) ) {
               // reset to application directory
               opensavedir = appdir;
            }
         }
      }
      fclose(f);
   } else {
      // prefs file doesn't exist yet
      opensavedir = appdir;
   }
}

// -----------------------------------------------------------------------------

void FinishApp() {
   // WARNING: infinite recursion will occur if Fatal is called in here
   
   // save main window location and other user preferences
   SavePrefs();
   
   // delete gen0file if it exists
   if (wxFileExists(gen0file)) wxRemoveFile(gen0file);
}

#ifdef __WXMAC__

// do our own Mac-specific warning and fatal dialogs using StandardAlert
// so we can center dialogs on parent window and also see modified app icon

bool AppInBackground() {
   ProcessSerialNumber frontPSN, currentPSN;
   bool sameProcess = false;

   GetCurrentProcess(&currentPSN);
   GetFrontProcess(&frontPSN);
   SameProcess(&currentPSN, &frontPSN, (Boolean *)&sameProcess);

   return sameProcess == false;
}

void NotifyUser() {
   NMRec myNMRec;
   if ( AppInBackground() ) {
      myNMRec.qType = nmType;
      myNMRec.nmMark = 1;
      myNMRec.nmIcon = NULL;
      myNMRec.nmSound = NULL;
      myNMRec.nmStr = NULL;
      myNMRec.nmResp = NULL;
      myNMRec.nmRefCon = 0;
      if ( NMInstall(&myNMRec) == noErr ) {
         // wait for resume event to bring us to foreground
         do {
            EventRef event;
            EventTargetRef target;
            if ( ReceiveNextEvent(0, NULL, kEventDurationNoWait, true, &event) == noErr ) {
               target = GetEventDispatcherTarget();
               SendEventToEventTarget(event, target);
               ReleaseEvent(event);
            }
            Delay(6,NULL);                // don't hog CPU
         } while ( AppInBackground() );
         NMRemove(&myNMRec);
      }
   }
}

void MacWarning(const char *s) {
   short itemHit;
   AlertStdAlertParamRec alertParam;
   Str255 ptitle, pmsg;

   CopyCStringToPascal("Golly warning:", ptitle);
   CopyCStringToPascal(s, pmsg);

   NotifyUser();
   alertParam.movable = true;
   alertParam.helpButton = false;
   alertParam.filterProc = NULL;
   alertParam.defaultText = NULL;
   alertParam.cancelText = NULL;
   alertParam.otherText = NULL;
   alertParam.defaultButton = kAlertStdAlertOKButton;
   alertParam.cancelButton = 0;
   alertParam.position = kWindowAlertPositionParentWindow;
   StandardAlert(kAlertCautionAlert, ptitle, pmsg, &alertParam, &itemHit);
}

void MacFatal(const char *s) {
   short itemHit;
   AlertStdAlertParamRec alertParam;
   Str255 ptitle, pmsg, pquit;

   CopyCStringToPascal("Golly error:", ptitle);
   CopyCStringToPascal(s, pmsg);
   CopyCStringToPascal("Quit", pquit);

   NotifyUser();
   alertParam.movable = true;
   alertParam.helpButton = false;
   alertParam.filterProc = NULL;
   alertParam.defaultText = pquit;
   alertParam.cancelText = NULL;
   alertParam.otherText = NULL;
   alertParam.defaultButton = kAlertStdAlertOKButton;
   alertParam.cancelButton = 0;
   alertParam.position = kWindowAlertPositionParentWindow;
   StandardAlert(kAlertStopAlert, ptitle, pmsg, &alertParam, &itemHit);
}

#endif // __WXMAC__

void Warning(const char *s) {
   wxBell();
   wxSetCursor(*wxSTANDARD_CURSOR);
   #ifdef __WXMAC__
      MacWarning(s);
   #else
      wxMessageBox(_(s), _("Golly warning:"), wxOK | wxICON_EXCLAMATION, frameptr);
   #endif
}

void Fatal(const char *s) {
   FinishApp();
   wxBell();
   wxSetCursor(*wxSTANDARD_CURSOR);
   #ifdef __WXMAC__
      MacFatal(s);
   #else
      wxMessageBox(_(s), _("Golly error:"), wxOK | wxICON_ERROR, frameptr);
   #endif
   // calling wxExit() results in a bus error on X11
   exit(1);
}

void BeginProgress(const char *dlgtitle) {
   if (progdlg) {
      // better do this in case of nested call
      delete progdlg;
      progdlg = NULL;
   }
   strncpy(progtitle, dlgtitle, sizeof(progtitle));
   progstart = wxGetElapsedTime(false);
   // let user know they'll have to wait
   wxSetCursor(*wxHOURGLASS_CURSOR);
   viewptr->SetCursor(*wxHOURGLASS_CURSOR);
}

bool AbortProgress(double fraction_done, const char *newmsg) {
   long t = wxGetElapsedTime(false);
   if (progdlg) {
      if (t < prognext) return false;
      #ifdef __WXX11__
         prognext = t + 1000;    // call Update about once per sec on X11
      #else
         prognext = t + 100;     // call Update about 10 times per sec
      #endif
      // Update returns false if user hits Cancel button;
      // too bad wxMac and wxX11 don't let user hit escape key!!!
      return !progdlg->Update(int((double)maxprogrange * fraction_done), newmsg);
   } else {
      // note that fraction_done is not a very accurate estimator for how long
      // the task will take, especially now that we use nextcell for cut/copy
      long msecs = t - progstart;
      if ( (msecs > 1000 && fraction_done < 0.3) || msecs > 2500 ) {
         // task is probably going to take a while so create progress dialog
         progdlg = new wxProgressDialog(_T(progtitle), _T(""),
                                        maxprogrange, frameptr,
                                        wxPD_CAN_ABORT | wxPD_APP_MODAL | wxPD_SMOOTH |
                                        wxPD_ESTIMATED_TIME | wxPD_REMAINING_TIME);
         #ifdef __WXMAC__
            // avoid user selecting Quit or bringing another window to front
            if (progdlg) BeginAppModalStateForWindow(FrontWindow());
         #endif
      }
      prognext = t + 10;      // short delay until 1st Update
      return false;           // don't abort
   }
}

void EndProgress() {
   if (progdlg) {
      #ifdef __WXMAC__
         EndAppModalStateForWindow(FrontWindow());
      #endif
      delete progdlg;
      progdlg = NULL;
      #ifdef __WXX11__
         // fix activate problem on X11 if user hit Cancel button
         frameptr->SetFocus();
      #endif
   }
}

void SetFrameIcon(wxFrame *frame) {
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

void DisplayMessage(const char *s);

class wxlifeerrors : public lifeerrors {
public:
   virtual void fatal(const char *s) { Fatal(s); }
   virtual void warning(const char *s) { Warning(s); }
   virtual void status(const char *s) { DisplayMessage(s); }
   virtual void beginprogress(const char *s) { BeginProgress(s); }
   virtual bool abortprogress(double f, const char *s) { return AbortProgress(f, s); }
   virtual void endprogress() { EndProgress(); }
};

wxlifeerrors wxerrhandler;

// -----------------------------------------------------------------------------

void NoSelection() {
   // set seltop > selbottom
   seltop = 1;
   selbottom = 0;
}

bool SelectionExists() {
   return (seltop <= selbottom) == 1;     // avoid VC++ warning
}

void InitSelection() {
   NoSelection();

   #ifdef __WXX11__
      // wxX11's Blit doesn't support alpha channel
   #else
      // create semi-transparent selection image
      if ( !selimage.Create(1,1) )
         Fatal("Failed to create selection image!");
      selimage.SetRGB(0, 0, 75, 175, 0);     // darkish green
      selimage.SetAlpha();                   // add alpha channel
      if ( selimage.HasAlpha() ) {
         selimage.SetAlpha(0, 0, 128);       // 50% opaque
      } else {
         Warning("Selection image has no alpha channel!");
      }
      // scale selection image to viewport size and create selbitmap;
      // it's not strictly necessary to do this here (because PatternView::OnPaint
      // will do it) but it avoids any delay when user makes their first selection
      int wd, ht;
      viewptr->GetClientSize(&wd, &ht);
      // wd or ht might be < 1 on Win/X11 platforms
      if (wd < 1) wd = 1;
      if (ht < 1) ht = 1;
      selimage.Rescale(wd, ht);
      if (selbitmap) delete selbitmap;
      selbitmap = new wxBitmap(selimage);
      selbitmapwd = wd;
      selbitmapht = ht;
   #endif
}

bool SelectionVisible(wxRect *visrect) {
   if (!SelectionExists())
      return false;

   pair<int,int> lt = currview.screenPosOf(selleft, seltop, curralgo);
   pair<int,int> rb = currview.screenPosOf(selright, selbottom, curralgo);

   if (lt.first > currview.getxmax() ||
       lt.second > currview.getymax() ||
       rb.first < 0 ||
       rb.second < 0)
      // no part of selection is visible
      return false;

   // all or some of selection is visible in viewport;
   // only set visible rectangle if requested
   if (visrect) {
      if (lt.first < 0) lt.first = 0;
      if (lt.second < 0) lt.second = 0;
      // correct for mag if needed
      if (currview.getmag() > 0) {
         rb.first += (1 << currview.getmag()) - 1;
         rb.second += (1 << currview.getmag()) - 1;
         if (currview.getmag() > 1) {
            // avoid covering gaps
            rb.first--;
            rb.second--;
         }
      }
      if (rb.first > currview.getxmax()) rb.first = currview.getxmax();
      if (rb.second > currview.getymax()) rb.second = currview.getymax();
      visrect->SetX(lt.first);
      visrect->SetY(lt.second);
      visrect->SetWidth(rb.first - lt.first + 1);
      visrect->SetHeight(rb.second - lt.second + 1);
   }
   return true;
}

#ifdef __WXX11__
void DrawX11Selection(wxDC &dc, wxRect &rect) {
   // wxX11's Blit doesn't support alpha channel so we just invert rect
   dc.Blit(rect.x, rect.y, rect.width, rect.height, &dc,
           rect.x, rect.y, wxINVERT);
}
#endif

void DrawSelection(wxDC &dc) {
   wxRect rect;
   if ( SelectionVisible(&rect) ) {
      #ifdef __WXX11__
         DrawX11Selection(dc, rect);
      #else
         if (selbitmap) {
            // draw semi-transparent green rect
            wxMemoryDC memDC;
            memDC.SelectObject(*selbitmap);
            dc.Blit(rect.x, rect.y, rect.width, rect.height, &memDC, 0, 0, wxCOPY, true);
         } else {
            // probably not enough memory
            wxBell();
         }
      #endif
   }
}

void DrawPasteRect(wxDC &dc) {
   dc.SetPen(*wxRED_PEN);
   dc.SetBrush(*wxTRANSPARENT_BRUSH);

   dc.DrawRectangle(pasterect);
   
   dc.SetFont(*statusfont);
   dc.SetBackgroundMode(wxSOLID);
   dc.SetTextForeground(*wxRED);
   dc.SetTextBackground(*wxWHITE);

   const char *pmodestr = GetPasteMode();
   int pmodex = pasterect.x + 2;
   int pmodey = pasterect.y - 4;
   dc.DrawText(pmodestr, pmodex, pmodey - textascent);
   
   dc.SetBrush(wxNullBrush);
   dc.SetPen(wxNullPen);
}

void FillRect(wxDC &dc, wxRect &rect, wxBrush &brush) {
   // set pen transparent so brush fills rect
   dc.SetPen(*wxTRANSPARENT_PEN);
   dc.SetBrush(brush);
   
   dc.DrawRectangle(rect);
   
   dc.SetBrush(wxNullBrush);     // restore brush
   dc.SetPen(wxNullPen);         // restore pen
}

// this lookup table magnifies bits in a given byte by a factor of 2;
// it assumes input and output are in XBM format (bits in each byte are reversed)
// because that's what wxWidgets requires when creating a monochrome bitmap
wxUint16 Magnify2[256];

// initialize Magnify2 table; note that it swaps byte order if running on Windows
// or some other little-endian processor
void InitMagnifyTable() {
   int inttest = 1;
   unsigned char *p = (unsigned char *)&inttest;
   for (int i=0; i<8; i++)
      if (*p)
         Magnify2[1<<i] = 3 << (2 * i);
      else
         Magnify2[1<<i] = 3 << (2 * (i ^ 4));
   for (int i=0; i<256; i++)
      if (i & (i-1))
         Magnify2[i] = Magnify2[i & (i-1)] + Magnify2[i & - i];
}

// this bitmap is for drawing magnified cells
wxBitmap magmap;
#define MAGSIZE (256)
wxUint16 magarray[MAGSIZE * MAGSIZE / 16];
unsigned char* magbuf = (unsigned char *)magarray;

#define MIN_GRID_MAG (3)      // minimum mag at which to draw grid lines
#define MIN_GRID_SCALE (8)    // minimum scale at which to draw grid lines (2^mag)

bool GridVisible() {
   return ( showgridlines && currview.getmag() >= MIN_GRID_MAG );
}

// magnify given bitmap by pmag (2, 4, ... 2^MAX_MAG)
void DrawStretchedBitmap(int xoff, int yoff, int *bmdata, int bmsize, int pmag) {

   int blocksize, magsize, rowshorts, numblocks, numshorts, numbytes;
   int rowbytes = bmsize / 8;
   int row, col;                    // current block
   int xw, yw;                      // window pos of scaled block's top left corner

   // try to process bmdata in square blocks of size MAGSIZE/pmag so each
   // magnified block is MAGSIZE x MAGSIZE
   blocksize = MAGSIZE / pmag;
   magsize = MAGSIZE;
   if (blocksize > bmsize) {
      blocksize = bmsize;
      magsize = bmsize * pmag;      // only use portion of magarray
   }
   rowshorts = magsize / 16;
   numbytes = rowshorts * 2;

   // pmag must be <= numbytes so numshorts (see below) will be > 0
   if (pmag > numbytes) {
      // this should never happen if max pmag is 16 (MAX_MAG = 4) and min bmsize is 64
      Fatal("DrawStretchedBitmap cannot magnify by this amount!");
   }
   
   // nicer to have gaps between cells at scales > 1:2
   wxUint16 gapmask = 0;
   if ( (pmag > 2 && pmag < MIN_GRID_SCALE) ||
        (pmag >= MIN_GRID_SCALE && !showgridlines) ) {
      // we use 7/7F rather than E/FE because of XBM bit reversal
      if (pmag == 4) {
         gapmask = 0x7777;
      } else if (pmag == 8) {
         gapmask = 0x7F7F;
      } else if (pmag == 16) {
         unsigned char *p = (unsigned char *)&gapmask;
         gapmask = 0xFF7F;
         // swap byte order if little-endian processor
         if (*p != 0xFF) gapmask = 0x7FFF;
      }
   }

   yw = yoff;
   numblocks = bmsize / blocksize;
   for ( row = 0; row < numblocks; row++ ) {
      xw = xoff;
      for ( col = 0; col < numblocks; col++ ) {
         if ( xw < currview.getwidth() && xw + magsize >= 0 &&
              yw < currview.getheight() && yw + magsize >= 0 ) {
            // some part of magnified block will be visible;
            // set bptr to address of byte at top left corner of current block
            unsigned char *bptr = (unsigned char *)bmdata + (row * blocksize * rowbytes) +
                                                            (col * blocksize / 8);
            
            int rowindex = 0;       // first row in magmap
            int i, j;
            for ( i = 0; i < blocksize; i++ ) {
               // use lookup table to convert bytes in bmdata to 16-bit ints in magmap
               numshorts = numbytes / pmag;
               for ( j = 0; j < numshorts; j++ ) {
                  magarray[rowindex + j] = Magnify2[ bptr[j] ];
               }
               while (numshorts < rowshorts) {
                  // stretch completed bytes in current row starting from right end
                  unsigned char *p = (unsigned char *)&magarray[rowindex + numshorts];
                  for ( j = numshorts * 2 - 1; j >= 0; j-- ) {
                     p--;
                     magarray[rowindex + j] = Magnify2[ *p ];
                  }
                  numshorts *= 2;
               }
               if (gapmask > 0) {
                  // erase pixel at right edge of each cell
                  for ( j = 0; j < rowshorts; j++ )
                     magarray[rowindex + j] &= gapmask;
                  // duplicate current magmap row pmag-2 times
                  for ( j = 2; j < pmag; j++ ) {
                     memcpy(&magarray[rowindex + rowshorts], &magarray[rowindex], rowshorts*2);
                     rowindex += rowshorts;
                  }
                  rowindex += rowshorts;
                  // erase pixel at bottom edge of each cell
                  memset(&magarray[rowindex], 0, rowshorts*2);
               } else {
                  // duplicate current magmap row pmag-1 times
                  for ( j = 1; j < pmag; j++ ) {
                     memcpy(&magarray[rowindex + rowshorts], &magarray[rowindex], rowshorts*2);
                     rowindex += rowshorts;
                  }
               }
               rowindex += rowshorts;     // start of next row in magmap
               bptr += rowbytes;          // start of next row in current block
            }
            
            magmap = wxBitmap((const char *)magbuf, magsize, magsize, 1);
            currdc->DrawBitmap(magmap, xw, yw, false);
         }
         xw += magsize;     // across to next block
      }
      yw += magsize;     // down to next block
   }
}

void DrawGridLines(wxDC &dc, wxRect &r, int pmag) {
   int h, v, i, topmod10, leftmod10;

   // ensure that 0,0 cell is next to mod-10 lines;
   // ie. mod-10 lines will scroll when pattern is scrolled
   pair<bigint, bigint> lefttop = currview.at(0, 0);
   leftmod10 = lefttop.first.mod_smallint(10);
   topmod10 = lefttop.second.mod_smallint(10);

   // draw all non mod-10 lines first
   if (blackcells) {
      dc.SetPen(*pen_ltgray);
   } else {
      dc.SetPen(*pen_verydark);
   }   
   i = topmod10;
   v = - 1;
   while (true) {
      v += pmag;
      if (v >= currview.getheight()) break;
      i++;
      if (i % 10 != 0 && v >= r.y && v < r.y + r.height)
         dc.DrawLine(r.x, v, r.GetRight() + 1, v);
   }
   i = leftmod10;
   h = - 1;
   while (true) {
      h += pmag;
      if (h >= currview.getwidth()) break;
      i++;
      if (i % 10 != 0 && h >= r.x && h < r.x + r.width)
         dc.DrawLine(h, r.y, h, r.GetBottom() + 1);
   }

   // now overlay mod-10 lines
   if (blackcells) {
      dc.SetPen(*pen_dkgray);
   } else {
      dc.SetPen(*pen_notsodark);
   }
   i = topmod10;
   v = - 1;
   while (true) {
      v += pmag;
      if (v >= currview.getheight()) break;
      i++;
      if (i % 10 == 0 && v >= r.y && v < r.y + r.height)
         dc.DrawLine(r.x, v, r.GetRight() + 1, v);
   }
   i = leftmod10;
   h = - 1;
   while (true) {
      h += pmag;
      if (h >= currview.getwidth()) break;
      i++;
      if (i % 10 == 0 && h >= r.x && h < r.x + r.width)
         dc.DrawLine(h, r.y, h, r.GetBottom() + 1);
   }
   
   dc.SetPen(*wxBLACK_PEN);
}

class wx_render : public liferender {
public:
   wx_render() {}
   virtual ~wx_render() {}
   virtual void killrect(int x, int y, int w, int h);
   virtual void blit(int x, int y, int w, int h, int *bm, int bmscale=1);
};

void wx_render::killrect(int x, int y, int w, int h) {
   if (w <= 0 || h <= 0)
      return;
   wxRect r = wxRect(x, y, w, h);
   #ifdef MYDEBUG
      // use a different pale color each time to see any probs
      wxBrush *randbrush = new wxBrush(wxColour((rand()&127)+128,
                                                (rand()&127)+128,
                                                (rand()&127)+128));
      FillRect(*currdc, r, *randbrush);
      delete randbrush;
   #else
      FillRect(*currdc, r, blackcells ? *wxWHITE_BRUSH : *wxBLACK_BRUSH);
   #endif
}

void wx_render::blit(int x, int y, int w, int h, int *bmdata, int bmscale) {
   if (bmscale == 1) {
      wxBitmap bmap = wxBitmap((const char *)bmdata, w, h, 1);
      currdc->DrawBitmap(bmap, x, y);
   } else {
      // stretch bitmap by bmscale
      DrawStretchedBitmap(x, y, bmdata, w / bmscale, bmscale);
   }
}

wx_render renderer;

// display pattern visible in viewport
void DisplayPattern() {
   // set foreground and background colors for DrawBitmap calls
   #ifdef __WXMSW__
   // use opposite bit meaning for Windows -- sigh
   if ( !blackcells ) {
   #else
   if ( blackcells ) {
   #endif
      currdc->SetTextForeground(*wxBLACK);
      currdc->SetTextBackground(*wxWHITE);
   } else {
      currdc->SetTextForeground(*wxWHITE);
      currdc->SetTextBackground(*wxBLACK);
   }

   if (nopattupdate) {
      // don't update pattern, just fill background
      wxRect r = wxRect(0, 0, currview.getwidth(), currview.getheight());
      FillRect(*currdc, r, blackcells ? *wxWHITE_BRUSH : *wxBLACK_BRUSH);
   } else {
      curralgo->draw(currview, renderer);    // calls blit and killrect
   }

   if ( GridVisible() ) {
      wxRect r = wxRect(0, 0, currview.getwidth(), currview.getheight());
      DrawGridLines(*currdc, r, 1 << currview.getmag());
   }
   
   DrawSelection(*currdc);
   
   if ( waitingforclick && pasterect.width > 0 ) {
      DrawPasteRect(*currdc);
   }
}

// empty statusmsg and erase 2nd line (ie. bottom half) of status bar
void ClearMessage() {
   if (waitingforclick) return;     // don't clobber paste msg
   statusmsg[0] = 0;
   if (statusht > 0) {
      int wd, ht;
      statusptr->GetClientSize(&wd, &ht);
      if (wd > 0 && ht > 0) {
         wxRect r = wxRect( wxPoint(0,statusht/2), wxPoint(wd-1,ht-2) );
         statusptr->Refresh(false, &r);
         // don't call Update() otherwise Win/X11 users see blue & yellow bands
         // when toggling hashing option
      }
   }
}

void DisplayMessage(const char *s) {
   strncpy(statusmsg, s, sizeof(statusmsg));
   if (statusht > 0) {
      int wd, ht;
      statusptr->GetClientSize(&wd, &ht);
      if (wd > 0 && ht > 0) {
         wxRect r = wxRect( wxPoint(0,statusht/2), wxPoint(wd-1,ht-2) );
         statusptr->Refresh(false, &r);
         // show message immediately
         statusptr->Update();
      }
   }
}

void ErrorMessage(const char *s) {
   wxBell();
   DisplayMessage(s);
}

void SetMessage(const char *s) {
   // set message string without displaying it
   strncpy(statusmsg, s, sizeof(statusmsg));
}

void SetStatusFont(wxDC &dc) {
   dc.SetFont(*statusfont);
   dc.SetTextForeground(*wxBLACK);
   dc.SetBrush(*wxBLACK_BRUSH);           // avoids problem on Linux/X11
   dc.SetBackgroundMode(wxTRANSPARENT);
}

void DisplayText(wxDC &dc, const char *s, wxCoord x, wxCoord y) {
   // DrawText's y parameter is top of text box but we pass in baseline
   // so adjust by textascent which depends on platform and OS version -- yuk!
   dc.DrawText(_(s), x, y - textascent);
}

// Ping-pong in the buffer so we can use multiple at a time.
const int STRINGIFYSIZE = 11;
const char *stringify(double d) {
   static char buf[120];
   static char *p = buf;
   if (p + STRINGIFYSIZE + 1 >= buf + sizeof(buf))
      p = buf;
   if (d <= 99999999999.0 && d >= -9999999999.0)
      sprintf(p, "%.f", d);
   else
      sprintf(p, "%g", d);
   char *r = p;
   p += strlen(p)+1;
   return r;
}
const char *stringify(const bigint &b) {
   return stringify(b.todouble());
}

void DrawStatusBar(wxDC &dc, wxRect &updaterect) {
   int wd, ht;
   statusptr->GetClientSize(&wd, &ht);
   if (wd < 1 || ht < 1) return;

   wxRect r = wxRect(0, 0, wd, ht);
   FillRect(dc, r, hashing ? *brush_blue : *brush_yellow);

   #ifdef __WXMSW__
      // draw gray lines at top, left and right edges
      dc.SetPen(*wxGREY_PEN);
      dc.DrawLine(0, 0, r.width, 0);
      dc.DrawLine(0, 0, 0, r.height);
      dc.DrawLine(r.GetRight(), 0, r.GetRight(), r.height);
   #else
      // draw gray line at bottom edge
      dc.SetPen(*wxLIGHT_GREY_PEN);
      dc.DrawLine(0, r.GetBottom(), r.width, r.GetBottom());
   #endif
   dc.SetPen(wxNullPen);

   // must be here rather than in StatusBar::OnPaint; it looks like
   // some call resets the font
   SetStatusFont(dc);

   char strbuf[256];
   
   if (updaterect.y < statusht/2) {
      // show info in top line
      if (updaterect.x < h_xy) {
         // show all info
         sprintf(strbuf, "Generation=%s",
                  curralgo == NULL ? "0" : stringify(curralgo->getGeneration()));
         DisplayText(dc, strbuf, h_gen, BASELINE1);
      
         double pop;
         if (progdlg) {
            // avoid calling getPopulation() if progress dialog is open
            pop = -1.0;
         } else {
            pop = curralgo == NULL ? 0.0 : curralgo->getPopulation().todouble();
         }
         if (pop >= 0) {
            sprintf(strbuf, "Population=%s", stringify(pop));
         } else {
            sprintf(strbuf, "Population=(pending)");
         }
         DisplayText(dc, strbuf, h_pop, BASELINE1);
      
         if (currview.getmag() < 0) {
            sprintf(strbuf, "Scale=2^%d:1", -currview.getmag());
         } else {
            sprintf(strbuf, "Scale=1:%d", 1 << currview.getmag());
         }
         DisplayText(dc, strbuf, h_scale, BASELINE1);
         
         if (warp < 0) {
            // show delay in secs
            sprintf(strbuf, "Delay=%gs", (double)(MIN_DELAY * (1 << (-warp - 1))) / 1000.0);
         } else {
            // show gen increment that matches code in SetGenIncrement
            if (hashing) {
               sprintf(strbuf, "Step=8^%d", warp);
            } else {
               sprintf(strbuf, "Step=10^%d", warp);
            }
         }
         DisplayText(dc, strbuf, h_step, BASELINE1);
      }
      if (showxy) {
         // if we ever provide an option to display standard math coords
         // (ie. y increasing upwards) then use -curry - 1
         sprintf(strbuf, "X,Y=%s,%s", stringify(currx), stringify(curry));
      } else {
         sprintf(strbuf, "X,Y=");
      }
      DisplayText(dc, strbuf, h_xy, BASELINE1);
   }

   if (statusmsg[0]) {
      // display status message on 2nd line
      DisplayText(dc, statusmsg, h_gen, BASELINE2);
   }
}

int SmallScroll(int xysize) {
   int amount;
   int mag = currview.getmag();
   if (mag > 0) {
      // scroll an integral number of cells (1 cell = 2^mag pixels)
      if (mag < 3) {
         amount = ((xysize >> mag) / 20) << mag;
         if (amount == 0) amount = 1 << mag;
         return amount;
      } else {
         // grid lines are visible so scroll by only 1 cell
         return 1 << mag;
      }
   } else {
      // scroll by approx 5% of current wd/ht
      amount = xysize / 20;
      if (amount == 0) amount = 1;
      return amount;
   }
}

int BigScroll(int xysize) {
   int amount;
   int mag = currview.getmag();
   if (mag > 0) {
      // scroll an integral number of cells (1 cell = 2^mag pixels)
      amount = ((xysize >> mag) * 9 / 10) << mag;
      if (amount == 0) amount = 1 << mag;
      return amount;
   } else {
      // scroll by approx 90% of current wd/ht
      amount = xysize * 9 / 10;
      if (amount == 0) amount = 1;
      return amount;
   }
}

int hthumb, vthumb;     // current thumb box positions

void UpdateScrollBars() {
   if (fullscreen)
      return;
   if (currview.getmag() > 0) {
      // scroll by integral number of cells to avoid rounding probs
      hthumb = currview.getwidth() >> currview.getmag();
      vthumb = currview.getheight() >> currview.getmag();
   } else {
      hthumb = currview.getwidth();
      vthumb = currview.getheight();
   }
   // keep thumb boxes in middle of scroll bars
   viewptr->SetScrollbar(wxHORIZONTAL, hthumb, hthumb, 3 * hthumb, true);
   viewptr->SetScrollbar(wxVERTICAL, vthumb, vthumb, 3 * vthumb, true);
}

// update tool bar buttons according to the current state
void UpdateToolBar(bool active) {
   wxToolBar *tbar = frameptr->GetToolBar();
   if (tbar && tbar->IsShown()) {
      #ifdef __WXX11__
         // reduce probs by first toggling all buttons off
         tbar->ToggleTool(wxID_NEW,       false);
         tbar->ToggleTool(wxID_OPEN,      false);
         tbar->ToggleTool(wxID_SAVE,      false);
         tbar->ToggleTool(ID_DRAW,        false);
         tbar->ToggleTool(ID_SELECT,      false);
         tbar->ToggleTool(ID_MOVE,        false);
         tbar->ToggleTool(ID_ZOOMIN,      false);
         tbar->ToggleTool(ID_ZOOMOUT,     false);
         tbar->ToggleTool(ID_GO,          false);
         tbar->ToggleTool(ID_STOP,        false);
         tbar->ToggleTool(ID_HASH,        false);
         tbar->ToggleTool(ID_INFO,        false);
      #endif
      if (waitingforclick)
         active = false;
      tbar->EnableTool(wxID_NEW,       active && !generating);
      tbar->EnableTool(wxID_OPEN,      active && !generating);
      tbar->EnableTool(wxID_SAVE,      active && !generating);
      tbar->EnableTool(ID_DRAW,        active);
      tbar->EnableTool(ID_SELECT,      active);
      tbar->EnableTool(ID_MOVE,        active);
      tbar->EnableTool(ID_ZOOMIN,      active);
      tbar->EnableTool(ID_ZOOMOUT,     active);
      tbar->EnableTool(ID_GO,          active && !generating);
      tbar->EnableTool(ID_STOP,        active && generating);
      tbar->EnableTool(ID_HASH,        active && !generating);
      tbar->EnableTool(ID_INFO,        active && currfile[0] != 0);
      // call ToggleTool for tools added via AddCheckTool or AddRadioTool
      tbar->ToggleTool(ID_HASH,        hashing);
      if (currcurs == curs_pencil)
         tbar->ToggleTool(ID_DRAW, true);
      else if (currcurs == curs_cross)
         tbar->ToggleTool(ID_SELECT, true);
      else if (currcurs == curs_hand)
         tbar->ToggleTool(ID_MOVE, true);
      else if (currcurs == curs_zoomin)
         tbar->ToggleTool(ID_ZOOMIN, true);
      else if (currcurs == curs_zoomout)
         tbar->ToggleTool(ID_ZOOMOUT, true);
   }
}

bool ClipboardHasText() {
   #ifdef __WXX11__
      return wxFileExists(clipfile);
   #else
      bool hastext = false;
      if ( wxTheClipboard->Open() ) {
         hastext = wxTheClipboard->IsSupported( wxDF_TEXT );
         wxTheClipboard->Close();
      }
      return hastext;
   #endif
}

// update menu bar items according to the current state
void UpdateMenuItems(bool active) {
   wxMenuBar *mbar = frameptr->GetMenuBar();
   wxToolBar *tbar = frameptr->GetToolBar();
   bool textinclip = ClipboardHasText();
   if (mbar) {
      // disable most items if main window is inactive
      if (waitingforclick)
         active = false;
      mbar->Enable(wxID_NEW,     active && !generating);
      mbar->Enable(wxID_OPEN,    active && !generating);
      mbar->Enable(ID_OPENCLIP,  active && (!generating) && textinclip);
      mbar->Enable(wxID_SAVE,    active && !generating);
      mbar->Enable(ID_CUT,       active && (!generating) && SelectionExists());
      mbar->Enable(ID_COPY,      active && (!generating) && SelectionExists());
      mbar->Enable(ID_CLEAR,     active && (!generating) && SelectionExists());
      mbar->Enable(ID_PASTE,     active && (!generating) && textinclip);
      mbar->Enable(ID_PASTE_SEL, active && (!generating) &&
                                 SelectionExists() && textinclip);
      mbar->Enable(ID_PLOCATION, active);
      mbar->Enable(ID_PMODE,     active);
      mbar->Enable(ID_SELALL,    active);
      mbar->Enable(ID_REMOVE,    active && SelectionExists());
      mbar->Enable(ID_CMODE,     active);
      mbar->Enable(ID_GO,        active && !generating);
      mbar->Enable(ID_STOP,      active && generating);
      mbar->Enable(ID_NEXT,      active && !generating);
      mbar->Enable(ID_STEP,      active && !generating);
      mbar->Enable(ID_RESET,     active && !generating &&
                                 curralgo->getGeneration() > bigint::zero);
      mbar->Enable(ID_FASTER,    active);
      mbar->Enable(ID_SLOWER,    active && warp > MIN_WARP);
      mbar->Enable(ID_AUTO,      active);
      mbar->Enable(ID_HASH,      active && !generating);
      mbar->Enable(ID_HYPER,     active && curralgo->hyperCapable() != 0);
      mbar->Enable(ID_MAXMEM,    active && hashing && !generating);
      mbar->Enable(ID_RULE,      active && !generating);
      mbar->Enable(ID_FIT,       active);
      mbar->Enable(ID_MIDDLE,    active);
      mbar->Enable(ID_FULL,      active);
      mbar->Enable(ID_STATUS,    active);
      mbar->Enable(ID_TOOL,      active);
      mbar->Enable(ID_GRID,      active);
      mbar->Enable(ID_VIDEO,     active);
      #ifdef __WXMAC__
         // windows on Mac OS X are automatically buffered
         mbar->Enable(ID_BUFF, false);
      #else
         mbar->Enable(ID_BUFF, active);
      #endif
      mbar->Enable(wxID_ZOOM_IN, active && currview.getmag() < MAX_MAG);
      mbar->Enable(wxID_ZOOM_OUT, active);
      mbar->Enable(ID_INFO,      currfile[0] != 0);
      // tick/untick menu items created using AppendCheckItem
      mbar->Check(ID_PL_TL,      plocation == TopLeft);
      mbar->Check(ID_PL_TR,      plocation == TopRight);
      mbar->Check(ID_PL_BR,      plocation == BottomRight);
      mbar->Check(ID_PL_BL,      plocation == BottomLeft);
      mbar->Check(ID_PL_MID,     plocation == Middle);
      mbar->Check(ID_PM_COPY,    pmode == Copy);
      mbar->Check(ID_PM_OR,      pmode == Or);
      mbar->Check(ID_PM_XOR,     pmode == Xor);
      mbar->Check(ID_DRAW,       currcurs == curs_pencil);
      mbar->Check(ID_SELECT,     currcurs == curs_cross);
      mbar->Check(ID_MOVE,       currcurs == curs_hand);
      mbar->Check(ID_ZOOMIN,     currcurs == curs_zoomin);
      mbar->Check(ID_ZOOMOUT,    currcurs == curs_zoomout);
      mbar->Check(ID_AUTO,       autofit);
      mbar->Check(ID_HASH,       hashing);
      mbar->Check(ID_HYPER,      hyperspeed);
      mbar->Check(ID_STATUS,     statusht > 0);
      mbar->Check(ID_TOOL,       tbar && tbar->IsShown());
      mbar->Check(ID_GRID,       showgridlines);
      mbar->Check(ID_VIDEO,      blackcells);
      #ifdef __WXMAC__
         // windows on Mac OS X are automatically buffered
         mbar->Check(ID_BUFF, true);
      #else
         mbar->Check(ID_BUFF, buffered);
      #endif
   }
}

bool PointInView(int x, int y) {
   return (x >= 0) && (x <= currview.getxmax()) &&
          (y >= 0) && (y <= currview.getymax());
}

void CheckCursor(bool active) {
   if ( active ) {
      // make sure cursor is up to date
      wxPoint pt = viewptr->ScreenToClient( wxGetMousePosition() );
      if (PointInView(pt.x, pt.y)) {
         // need both calls to fix Mac probs after toggling status/tool bar
         wxSetCursor(*currcurs);
         viewptr->SetCursor(*currcurs);
      } else {
         wxSetCursor(*wxSTANDARD_CURSOR);
      }
   } else {
      // main window is not active so don't change cursor
   }
}

void UpdateXYLocation() {
   int wd, ht;
   statusptr->GetClientSize(&wd, &ht);
   if (wd > h_xy && ht > 0) {
      wxRect r = wxRect( wxPoint(h_xy, 0), wxPoint(wd-1, statusht/2) );
      statusptr->Refresh(false, &r);
      // no need to Update() immediately
   }
}

void CheckMouseLocation(bool active) {
   if (statusht == 0)
      return;

   if ( !active ) {
      // main window is not in front so clear X,Y location
      showxy = false;
      UpdateXYLocation();
      return;
   }

   // may need to update X,Y location in status bar
   wxPoint pt = viewptr->ScreenToClient( wxGetMousePosition() );
   if (PointInView(pt.x, pt.y)) {
      // get location in cell coords
      pair<double, double> coor = currview.atf(pt.x, pt.y);
      if (currview.getmag() > 0) {
         coor.first = floor(coor.first);
         coor.second = floor(coor.second);
      }
      // need next 2 lines to avoid seeing "-0"
      if (fabs(coor.first) < 1) coor.first = 0;
      if (fabs(coor.second) < 1) coor.second = 0;
      if ( coor.first != currx || coor.second != curry ) {
         // show new X,Y location
         currx = coor.first;
         curry = coor.second;
         showxy = true;
         UpdateXYLocation();
      } else if (!showxy) {
         showxy = true;
         UpdateXYLocation();
      }
   } else {
      // outside viewport so clear X,Y location
      showxy = false;
      UpdateXYLocation();
   }
}

void UpdateUserInterface(bool active) {
   UpdateToolBar(active);
   UpdateMenuItems(active);
   CheckCursor(active);
   CheckMouseLocation(active);
}

// update everything in main window
void RefreshWindow() {
   if (frameptr->IsIconized()) return;    // do nothing if we've been minimized

   int wd, ht;
   frameptr->GetClientSize(&wd, &ht);     // includes status bar and viewport

   if (wd > 0 && ht > statusht) {
      viewptr->Refresh(false, NULL);
      viewptr->Update();                  // call PatternView::OnPaint
      UpdateScrollBars();
   }
   
   if (wd > 0 && ht > 0 && statusht > 0) {
      statusptr->Refresh(false, NULL);
      statusptr->Update();                // call StatusBar::OnPaint
   }
   
   UpdateUserInterface(frameptr->IsActive());
}

// only update pattern and status bar
void RefreshPatternAndStatus() {
   if (!frameptr->IsIconized()) {
      viewptr->Refresh(false, NULL);
      viewptr->Update();
      if (statusht > 0) {
         CheckMouseLocation(frameptr->IsActive());
         statusptr->Refresh(false, NULL);
         statusptr->Update();
      }
   }
}

// only update status bar
void RefreshStatus() {
   if (!frameptr->IsIconized()) {
      if (statusht > 0) {
         CheckMouseLocation(frameptr->IsActive());
         statusptr->Refresh(false, NULL);
         statusptr->Update();
      }
   }
}

// -----------------------------------------------------------------------------

class wx_poll : public lifepoll {
public:
   virtual int checkevents();
   virtual void updatePop();
   long nextcheck;
};

int wx_poll::checkevents() {
   #ifdef __WXMSW__
      // on Windows wxGetElapsedTime has a higher overhead than Yield
      wxGetApp().Yield(true);
      if (helpptr && helpptr->IsActive()) {
         // send idle events to htmlwin so cursor gets updated
         wxIdleEvent event;
         wxGetApp().SendIdleEvents(htmlwin, event);
      }
   #else
      // on Mac and X11 it is much faster to avoid calling Yield too often
      long t = wxGetElapsedTime(false);
      if (t > nextcheck) {
         nextcheck = t + 50;        // 20th of a sec
         wxGetApp().Yield(true);
         #ifdef __WXMAC__
            if (helpptr && helpptr->IsActive()) {
               // send idle events to htmlwin so cursor gets updated
               wxIdleEvent event;
               wxGetApp().SendIdleEvents(htmlwin, event);
            }
         #endif
      }
   #endif
   return isInterrupted();
}

void wx_poll::updatePop() {
   if (statusht > 0) {
      statusptr->Refresh(false, NULL);
      statusptr->Update();                // calls StatusBar::OnPaint
   }
}

wx_poll wx_poller;

// -----------------------------------------------------------------------------

// filing functions

const char B0message[] = "Hashing has been turned off due to B0-not-S8 rule.";

void SetAppDirectory() {
   #ifdef __WXMSW__
      // on Windows we need to reset current directory to app directory if user
      // dropped file from somewhere else onto app to start it up (otherwise we
      // can't find Help files and prefs file gets saved to wrong location)
      wxStandardPaths wxstdpaths;
      appdir = wxstdpaths.GetDataDir();
      wxString currdir = wxGetCwd();
      if ( currdir.CmpNoCase(appdir) != 0 )
         wxSetWorkingDirectory(appdir);
   #else
      // need to fix this on Mac!!! use wx book's example???
      appdir = wxGetCwd();
   #endif
}

void MySetTitle(const char *title) {
   #ifdef __WXMAC__
      if (FrontWindow() != NULL) {
         // avoid wxMac's SetTitle call -- it causes an undesirable window refresh
         Str255 ptitle;
         CopyCStringToPascal(title, ptitle);
         SetWTitle(FrontWindow(), (unsigned char const *)ptitle);
      } else {
         // this can happen before main window is shown
         frameptr->SetTitle(title);
      }
   #else
      frameptr->SetTitle(title);
   #endif
}

void SetWindowTitle(const char *filename) {
   char wtitle[128];
   // save filename for use when changing rule
   strncpy(currname, filename, sizeof(currname));
   sprintf(wtitle, "Golly: %s [%s]", filename, curralgo->getrule());
   MySetTitle(wtitle);
}

void SetGenIncrement() {
   if (warp > 0) {
      bigint inc = 1;
      // WARNING: if this code changes then we'll need changes to DrawStatusBar
      if (hashing) {
         // set inc to 8^warp
         inc.mulpow2(warp * 3);
      } else {
         // set inc to 10^warp
         int i = warp;
         while (i > 0) { inc.mul_smallint(10); i--; }
      }
      curralgo->setIncrement(inc);
   } else {
      curralgo->setIncrement(1);
   }
}

void CreateUniverse() {
   // first delete old universe if it exists
   if (curralgo) delete curralgo;

   if (hashing) {
      curralgo = new hlifealgo();
      curralgo->setMaxMemory(maxhmem);
   } else {
      curralgo = new qlifealgo();
   }

   // curralgo->step() will call wx_poll::checkevents()
   curralgo->setpoll(&wx_poller);

   // increment has been reset to 1 but that's probably not always desirable
   // so set increment using current warp value
   SetGenIncrement();
}

void FitInView() {
   curralgo->fit(currview, 1);
}

void NewPattern() {
   if (generating) return;
   savestart = false;
   currcurs = curs_pencil;
   currfile[0] = 0;
   warp = 0;
   CreateUniverse();
   if (initrule[0]) {
      // this is the first call of NewPattern when app starts
      const char *err = curralgo->setrule(initrule);
      if (err != 0)
         Warning(err);
      if (global_liferules.hasB0notS8 && hashing) {
         hashing = false;
         SetMessage(B0message);
         CreateUniverse();
      }
      initrule[0] = 0;        // don't use it again
   }
   // window title will also show curralgo->getrule()
   SetWindowTitle("untitled");
   FitInView();
   RefreshWindow();
}

void LoadPattern(const char *newtitle) {
   // don't use initrule in future NewPattern calls
   initrule[0] = 0;
   if (newtitle) {
      savestart = false;
      currcurs = curs_zoomin;
      if (infoptr) {
         // comments will no longer be relevant so close info window
         infoptr->Close(true);
      }
   }
   if (!showbanner) ClearMessage();
   warp = 0;

   if (curralgo) {
      // delete old universe and set NULL so status bar shows gen=0 and pop=0
      delete curralgo;
      curralgo = NULL;
   }
   // update all of status bar so we don't see different colored lines
   RefreshStatus();
   // set curralgo after drawing status bar otherwise getPopulation would
   // get called and slow down hlife pattern loading
   CreateUniverse();

   if (newtitle) {
      // show new file name in window title but no rule (which readpattern can change);
      // nicer if user can see file name while loading a very large pattern
      char wtitle[128];
      sprintf(wtitle, "Golly: Loading %s", newtitle);
      MySetTitle(wtitle);
   }

   nopattupdate = true;
   const char *err = readpattern(currfile, *curralgo);
   if (err && strcmp(err,cannotreadhash) == 0 && !hashing) {
      hashing = true;
      SetMessage("Hashing has been turned on for macrocell format.");
      // update all of status bar so we don't see different colored lines
      RefreshStatus();
      CreateUniverse();
      err = readpattern(currfile, *curralgo);
   } else if (global_liferules.hasB0notS8 && hashing && newtitle) {
      hashing = false;
      SetMessage(B0message);
      // update all of status bar so we don't see different colored lines
      RefreshStatus();
      CreateUniverse();
      err = readpattern(currfile, *curralgo);
   }
   nopattupdate = false;
   if (err != 0) Warning(err);

   // show full window title after readpattern has set rule
   if (newtitle) SetWindowTitle(newtitle);
   FitInView();
   RefreshWindow();
   showbanner = false;
}

void ResetPattern() {
   if ( !generating && curralgo->getGeneration() > bigint::zero ) {
      if ( gen0algo ) {
         // restore starting pattern saved in gen0algo
         delete curralgo;
         curralgo = gen0algo;
         gen0algo = NULL;
         savestart = true;
         hashing = gen0hash;
         warp = 0;
         SetGenIncrement();
         curralgo->setMaxMemory(maxhmem);
         curralgo->setGeneration(bigint::zero);
         curralgo->setrule(gen0rule);
         SetWindowTitle(currname);
         FitInView();
         RefreshWindow();
      } else {
         // restore starting pattern from currfile
         if ( currfile[0] == 0 ) {
            // if this happens then savestart logic is probably wrong
            Warning("There is no pattern file to reload!");
         } else {
            // save rule in case user changed it after loading pattern
            char saverule[128];
            strncpy(saverule, curralgo->getrule(), sizeof(saverule));
      
            // pass in NULL so window title, savestart and currcurs won't change
            LoadPattern(NULL);
            // warp and gen count have been reset to 0
            
            // restore saved rule
            curralgo->setrule(saverule);
         }
      }
   }
}

const char *GetBaseName(const char *fullpath) {
   // there's probably a better/safer way to do this using wxFileName::GetFullName???!!!
   #ifdef __WXMSW__
      char separator = '\\';
   #else
      char separator = '/';
   #endif
   int len = strlen(fullpath);
   while (len > 0) {
      len--;
      if (fullpath[len] == separator) {
         len++;
         break;
      }
   }
   return (const char *)&fullpath[len];
}

void SetCurrentFile(const char *inpath) {
   #ifdef __WXMAC__
      // copy given path to currfile but with UTF8 encoding so fopen will work
      CFURLRef url = CFURLCreateWithBytes(NULL,
                                          (const UInt8*)inpath,
                                          strlen(inpath),
                                          kCFStringEncodingMacRoman,
                                          NULL);
      CFStringRef str = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
      CFRelease(url);
      CFStringGetCString(str, currfile, sizeof(currfile), kCFStringEncodingUTF8);
      CFRelease(str);
   #else
      strncpy(currfile, inpath, sizeof(currfile));
   #endif
}

void OpenPattern() {
   if (generating) return;

   wxFileDialog opendlg(frameptr, _("Choose a pattern file"),
                        opensavedir, wxEmptyString,
   _("All files (*)|*|RLE (*.rle)|*.rle|Life 1.05/1.06 (*.lif)|*.lif|Macrocell (*.mc)|*.mc"),
                        wxOPEN | wxFILE_MUST_EXIST);

   if ( opendlg.ShowModal() == wxID_OK ) {
      wxFileName fullpath = wxFileName( opendlg.GetPath() );
      opensavedir = fullpath.GetPath();
      SetCurrentFile( opendlg.GetPath() );
      LoadPattern( opendlg.GetFilename() );
   }
}

bool GetTextFromClipboard(wxTextDataObject *data) {
   bool gotdata = false;
   if ( wxTheClipboard->Open() ) {
      if ( wxTheClipboard->IsSupported( wxDF_TEXT ) ) {
         gotdata = wxTheClipboard->GetData( *data );
         if (!gotdata) {
            ErrorMessage("Could not get clipboard data!");
         }
      } else {
         #ifdef __WXX11__
            ErrorMessage("Sorry, but there is no clipboard support for X11.");
            // do X11 apps like xlife or fontforge have clipboard support???!!!
         #else
            ErrorMessage("No text in clipboard.");
         #endif
      }
      wxTheClipboard->Close();
   } else {
      ErrorMessage("Could not open clipboard!");
   }
   return gotdata;
}

void OpenClipboard() {
   if (generating) return;
   // load and view pattern data stored in clipboard
   #ifdef __WXX11__
      // on X11 the clipboard data is in non-temporary clipfile, so copy
      // clipfile to gen0file (for use by ResetPattern and ShowPatternInfo)
      wxFFile infile(clipfile, "r");
      if ( infile.IsOpened() ) {
         wxString data;
         if ( infile.ReadAll(&data) ) {
            wxFile outfile(gen0file, wxFile::write);
            if ( outfile.IsOpened() ) {
               outfile.Write(data);
               outfile.Close();
               strncpy(currfile, gen0file, sizeof(currfile));
               LoadPattern("clipboard");
            } else {
               ErrorMessage("Could not create gen0file!");
            }
         } else {
            ErrorMessage("Could not read clipfile data!");
         }
         infile.Close();
      } else {
         ErrorMessage("Could not open clipfile!");
      }
   #else
      wxTextDataObject data;
      if (GetTextFromClipboard(&data)) {
         // copy clipboard data to gen0file so we can handle all formats
         // supported by readpattern
         wxFile outfile(gen0file, wxFile::write);
         if ( outfile.IsOpened() ) {
            outfile.Write( data.GetText() );
            outfile.Close();
            strncpy(currfile, gen0file, sizeof(currfile));
            LoadPattern("clipboard");
            // do NOT delete gen0file -- it can be reloaded by ResetPattern
            // or used by ShowPatternInfo
         } else {
            ErrorMessage("Could not create gen0file!");
         }
      }
   #endif
}

void SavePattern() {
   if (generating) return;
   
   wxString filetypes;
   int RLEindex, L105index, MCindex;
   RLEindex = L105index = MCindex = -1;   // format not allowed (any -ve number)
   
   bigint top, left, bottom, right;
   int itop, ileft, ibottom, iright;
   curralgo->findedges(&top, &left, &bottom, &right);
   if (hashing) {
      if ( OutsideLimits(top, left, bottom, right) ) {
         // too big for RLE so only allow saving as MC file
         itop = ileft = ibottom = iright = 0;
         filetypes = _("Macrocell (*.mc)|*.mc");
         MCindex = 0;
      } else {
         // allow saving as MC or RLE file
         itop = top.toint();
         ileft = left.toint();
         ibottom = bottom.toint();
         iright = right.toint();
         filetypes = _("RLE (*.rle)|*.rle|Macrocell (*.mc)|*.mc");
         RLEindex = 0;
         MCindex = 1;
      }
   } else {
      // allow saving as RLE or Life 1.05 file if pattern is small enough
      if ( OutsideLimits(top, left, bottom, right) ) {
         ErrorMessage("Pattern is outside +/- 10^9 boundary.");
         return;
      }   
      itop = top.toint();
      ileft = left.toint();
      ibottom = bottom.toint();
      iright = right.toint();
      filetypes = _("RLE (*.rle)|*.rle|Life 1.05 (*.lif)|*.lif");
      RLEindex = 0;
      L105index = 1;
   }

   wxFileDialog savedlg( frameptr, _("Save pattern"),
                         opensavedir, wxEmptyString, filetypes,
                         wxSAVE | wxOVERWRITE_PROMPT );

   if ( savedlg.ShowModal() == wxID_OK ) {
      wxFileName fullpath = wxFileName( savedlg.GetPath() );
      opensavedir = fullpath.GetPath();
      wxString ext = fullpath.GetExt();
      pattern_format format;
      // if user supplied a known extension (rle/lif/mc) then use that format if
      // it is allowed, otherwise use current format specified in filter menu
      if ( ext.IsSameAs("rle",false) && RLEindex >= 0 ) {
         format = RLE_format;
      } else if ( ext.IsSameAs("lif",false) && L105index >= 0 ) {
         format = L105_format;
      } else if ( ext.IsSameAs("mc",false) && MCindex >= 0 ) {
         format = MC_format;
      } else if ( savedlg.GetFilterIndex() == RLEindex ) {
         format = RLE_format;
      } else if ( savedlg.GetFilterIndex() == L105index ) {
         format = L105_format;
      } else if ( savedlg.GetFilterIndex() == MCindex ) {
         format = MC_format;
      } else {
         ErrorMessage("Bug in SavePattern!");
         return;
      }
      SetCurrentFile( savedlg.GetPath() );
      SetWindowTitle( savedlg.GetFilename() );
      const char *err = writepattern(savedlg.GetPath(), *curralgo, format,
                                     itop, ileft, ibottom, iright);
      if ( err != 0 ) {
         ErrorMessage(err);
      } else {
         DisplayMessage("Pattern saved in file.");
         if ( curralgo->getGeneration() == bigint::zero ) {
            // no need to save starting pattern (ResetPattern can load file)
            savestart = false;
         }
      }
   }
}

// -----------------------------------------------------------------------------

// editing functions

const char empty_pattern[] = "All cells are dead.";
const char selection_too_big[] = "Selection is outside +/- 10^9 boundary.";

void ClearSelection() {
   if (generating || !SelectionExists()) return;
   
   // no need to do anything if there is no pattern
   if (curralgo->isEmpty()) return;
   
   bigint top, left, bottom, right;
   curralgo->findedges(&top, &left, &bottom, &right);
   if ( seltop <= top && selbottom >= bottom &&
        selleft <= left && selright >= right ) {
      // selection encloses entire pattern so just create new universe
      int savewarp = warp;
      int savemag = currview.getmag();
      bigint savex = currview.x;
      bigint savey = currview.y;
      bigint savegen = curralgo->getGeneration();
      CreateUniverse();
      // restore various settings
      warp = savewarp;
      SetGenIncrement();
      currview.setpositionmag(savex, savey, savemag);
      curralgo->setGeneration(savegen);
      RefreshPatternAndStatus();
      return;
   }

   // no need to do anything if selection is completely outside pattern edges
   if ( seltop > bottom || selbottom < top ||
        selleft > right || selright < left ) {
      return;
   }
   
   // find intersection of selection and pattern to minimize work
   if (seltop > top) top = seltop;
   if (selleft > left) left = selleft;
   if (selbottom < bottom) bottom = selbottom;
   if (selright < right) right = selright;

   // can only use setcell in limited domain
   if ( OutsideLimits(top, left, bottom, right) ) {
      ErrorMessage(selection_too_big);
      return;
   }

   int itop = top.toint();
   int ileft = left.toint();
   int ibottom = bottom.toint();
   int iright = right.toint();
   int wd = iright - ileft + 1;
   int ht = ibottom - itop + 1;
   int cx, cy;

   double maxcount = (double)wd * (double)ht;
   int currcount = 0;
   bool abort = false;
   BeginProgress("Clearing selection");

   // this is likely to be very slow for large selections;
   // need to implement a fast setrect routine for each algo???!!!
   for ( cy=itop; cy<=ibottom; cy++ ) {
      for ( cx=ileft; cx<=iright; cx++ ) {
         curralgo->setcell(cx, cy, 0);
         currcount++;
         if ( (currcount % 1000) == 0 ) {
            abort = AbortProgress((double)currcount / maxcount, "");
            if (abort) break;
         }
      }
      if (abort) break;
   }
   curralgo->endofpattern();
   savestart = true;
   
   EndProgress();
   RefreshPatternAndStatus();
}

#ifdef __WXX11__
// no global clipboard support on X11 so we save data in a file
void CreateX11Clipboard(char *textptr, size_t textlen) {
   wxFile tmpfile(clipfile, wxFile::write);
   if ( tmpfile.IsOpened() ) {
      if ( tmpfile.Write( textptr, textlen ) < textlen ) {
         Warning("Could not write all data to clipboard file!");
      }
      tmpfile.Close();
   } else {
      Warning("Could not create clipboard file!");
   }
}
#endif

const unsigned int maxrleline = 70;    // max line length for RLE data

#ifdef __WXMAC__
   const char EOL = '\r';              // nicer for stupid apps like LifeLab :)
#else
   const char EOL = '\n';
#endif

void AddRun (char ch,
             unsigned int *run,        // in and out
             unsigned int *linelen,    // ditto
             char **chptr)             // ditto
{
   // output of RLE pattern data is channelled thru here to make it easier to
   // ensure all lines have <= maxrleline characters
   unsigned int i, numlen;
   char numstr[32];
   
   if ( *run > 1 ) {
      sprintf(numstr, "%u", *run);
      numlen = strlen(numstr);
   } else {
      numlen = 0;                      // no run count shown if 1
   }
   // keep linelen <= maxrleline
   if ( *linelen + numlen + 1 > maxrleline ) {
      **chptr = EOL;
      *chptr += 1;
      *linelen = 0;
   }
   i = 0;
   while (i < numlen) {
      **chptr = numstr[i];
      *chptr += 1;
      i++;
   }
   **chptr = ch;
   *chptr += 1;
   *linelen += numlen + 1;
   *run = 0;                           // reset run count
}

void CopyToClipboard(bool cut) {
   // can only use getcell/setcell in limited domain
   if ( OutsideLimits(seltop, selbottom, selleft, selright) ) {
      ErrorMessage(selection_too_big);
      return;
   }
   
   int itop = seltop.toint();
   int ileft = selleft.toint();
   int ibottom = selbottom.toint();
   int iright = selright.toint();
   unsigned int wd = iright - ileft + 1;
   unsigned int ht = ibottom - itop + 1;

   // convert cells in selection to RLE data in textptr
   char *textptr;
   char *etextptr;
   int cursize = 4096;
   
   textptr = (char *)malloc(cursize);
   if (textptr == NULL) {
      ErrorMessage("Not enough memory for clipboard data!");
      return;
   }
   etextptr = textptr + cursize;

   // add RLE header line
   sprintf(textptr, "x = %u, y = %u, rule = %s%c", wd, ht, curralgo->getrule(), EOL);
   char *chptr = textptr;
   chptr += strlen(textptr);
   // save start of data in case livecount is zero
   int datastart = chptr - textptr;
   
   // add RLE pattern data
   unsigned int livecount = 0;
   unsigned int linelen = 0;
   unsigned int brun = 0;
   unsigned int orun = 0;
   unsigned int dollrun = 0;
   char lastchar;
   int cx, cy;
   
   double maxcount = (double)wd * (double)ht;
   int cntr = 0;
   bool abort = false;
   if (cut)
      BeginProgress("Cutting selection");
   else
      BeginProgress("Copying selection");

   for ( cy=itop; cy<=ibottom; cy++ ) {
      // set lastchar to anything except 'o' or 'b'
      lastchar = 0;
      for ( cx=ileft; cx<=iright; cx++ ) {
         int skip = curralgo->nextcell(cx, cy);
         if (skip + cx > iright)
            skip = -1;           // pretend we found no more live cells
         if (skip > 0) {
            // have exactly "skip" empty cells here
            if (lastchar == 'b') {
               brun += skip;
            } else {
               if (orun > 0) {
                  // output current run of live cells
                  AddRun('o', &orun, &linelen, &chptr);
               }
               lastchar = 'b';
               brun = skip;
            }
         }
         if (skip >= 0) {
            // found next live cell
            cx += skip;
            livecount++;
            if (cut) curralgo->setcell(cx, cy, 0);
            if (lastchar == 'o') {
               orun++;
            } else {
               if (dollrun > 0) {
                  // output current run of $ chars
                  AddRun('$', &dollrun, &linelen, &chptr);
               }
               if (brun > 0) {
                  // output current run of dead cells
                  AddRun('b', &brun, &linelen, &chptr);
               }
               lastchar = 'o';
               orun = 1;
            }
         } else {
            cx = iright + 1;  // done
         }
         cntr++;
         if ((cntr & 4096) == 0) {
            double prog = ((cy - itop) * (double)(iright - ileft + 1) +
                           (cx - ileft)) / maxcount;
            abort = AbortProgress(prog, "");
            if (abort) break;
         }
         if (chptr + 60 >= etextptr) {
            // nearly out of space; try to increase allocation
            char *ntxtptr = (char *)realloc(textptr, 2*cursize);
            if (ntxtptr == 0) {
               ErrorMessage("No more memory for clipboard data!");
               // don't return here -- best to set abort flag and break so that
               // partially cut/copied portion gets saved to clipboard
               abort = true;
               break;
            }
            chptr = ntxtptr + (chptr - textptr);
            cursize *= 2;
            etextptr = ntxtptr + cursize;
            textptr = ntxtptr;
         }
      }
      if (abort) break;
      // end of current row
      if (lastchar == 'b') {
         // forget dead cells at end of row
         brun = 0;
      } else if (lastchar == 'o') {
         // output current run of live cells
         AddRun('o', &orun, &linelen, &chptr);
      }
      dollrun++;
   }
   
   if (livecount == 0) {
      // no live cells in selection so simplify RLE data to "!"
      chptr = textptr + datastart;
      *chptr = '!';
      chptr++;
   } else {
      // terminate RLE data
      dollrun = 1;
      AddRun('!', &dollrun, &linelen, &chptr);
      if (cut) {
         curralgo->endofpattern();
         savestart = true;
      }
   }
   *chptr = EOL;
   chptr++;
   *chptr = 0;
   
   EndProgress();
   
   if (cut && livecount > 0)
      RefreshPatternAndStatus();
   
   // copy text to clipboard
   #ifdef __WXX11__
      CreateX11Clipboard(textptr, strlen(textptr));
   #else
      if (wxTheClipboard->Open()) {
         if ( !wxTheClipboard->SetData(new wxTextDataObject(textptr)) ) {
            ErrorMessage("Could not copy selection to clipboard!");
         }
         wxTheClipboard->Close();
      } else {
         ErrorMessage("Could not open clipboard!");
      }
   #endif
   
   // tidy up
   free(textptr);
}

void CutSelection() {
   if (generating || !SelectionExists()) return;
   CopyToClipboard(true);
}

void CopySelection() {
   if (generating || !SelectionExists()) return;
   CopyToClipboard(false);
}

void EnableAllMenus(bool enable) {
   #ifdef __WXMAC__
      // enable/disable all menus, including Help menu and About/Quit items in app menu
      if (enable)
         EndAppModalStateForWindow(FrontWindow());
      else
         BeginAppModalStateForWindow(FrontWindow());
   #else
      wxMenuBar *mbar = frameptr->GetMenuBar();
      if (mbar) {
         int count = mbar->GetMenuCount();
         int i;
         for (i = 0; i<count; i++) {
            mbar->EnableTop(i, enable);
         }
      }
   #endif
}

void SetPasteRect(wxRect &rect, bigint &wd, bigint &ht) {
   int x, y, pastewd, pasteht;
   int mag = currview.getmag();
   int cellsize = 1 << mag;
   if (mag >= 0) {
      x = pastex - (pastex % cellsize);
      y = pastey - (pastey % cellsize);
      // if wd or ht are large then we need to avoid overflow but still
      // ensure that rect edges won't be seen
      bigint viswd = (currview.getwidth() + 1) >> mag;
      bigint visht = (currview.getheight() + 1) >> mag;
      // we use twice viewport wd/ht in case cursor is in middle of pasterect
      viswd.mul_smallint(2);
      visht.mul_smallint(2);
      pastewd = (wd <= viswd) ? wd.toint() << mag : 2*currview.getwidth() + 2;
      pasteht = (ht <= visht) ? ht.toint() << mag : 2*currview.getheight() + 2;
      if (mag > 1) {
         pastewd--;
         pasteht--;
      }
   } else {
      // mag < 0
      x = pastex;
      y = pastey;
      // following results in too small a rect???!!!
      pastewd = wd.toint() >> -mag;
      pasteht = ht.toint() >> -mag;      
      if (pastewd <= 0) {
         pastewd = 1;
      } else if (pastewd > 2*currview.getwidth()) {
         // avoid DrawRectangle problem on Mac (QD rect wd should not exceed 32K)
         pastewd = 2*currview.getwidth() + 1;
      }
      if (pasteht <= 0) {
         pasteht = 1;
      } else if (pasteht > 2*currview.getheight()) {
         // avoid DrawRectangle problem on Mac (QD rect ht should not exceed 32K)
         pasteht = 2*currview.getheight() + 1;
      }
   }
   rect = wxRect(x, y, pastewd, pasteht);
   int xoffset, yoffset;
   switch (plocation) {
      case TopLeft:
         break;
      case TopRight:
         xoffset = mag > 0 ? -(pastewd - cellsize + 1) : -pastewd + 1;
         rect.Offset(xoffset, 0);
         break;
      case BottomRight:
         xoffset = mag > 0 ? -(pastewd - cellsize + 1) : -pastewd + 1;
         yoffset = mag > 0 ? -(pasteht - cellsize + 1) : -pasteht + 1;
         rect.Offset(xoffset, yoffset);
         break;
      case BottomLeft:
         yoffset = mag > 0 ? -(pasteht - cellsize + 1) : -pasteht + 1;
         rect.Offset(0, yoffset);
         break;
      case Middle:
         xoffset = mag > 0 ? -(pastewd / cellsize / 2) * cellsize : -pastewd / 2;
         yoffset = mag > 0 ? -(pasteht / cellsize / 2) * cellsize : -pasteht / 2;
         rect.Offset(xoffset, yoffset);
         break;
   }
}

void PasteTemporaryToCurrent(lifealgo *tempalgo, bool toselection,
                             bigint top, bigint left, bigint bottom, bigint right) {
   // make sure given edges are within getcell/setcell limits
   if ( OutsideLimits(top, left, bottom, right) ) {
      ErrorMessage("Clipboard pattern is too big.");
      return;
   }   
   int itop = top.toint();
   int ileft = left.toint();
   int ibottom = bottom.toint();
   int iright = right.toint();
   bigint ht = ibottom - itop + 1;
   bigint wd = iright - ileft + 1;
   
   if ( toselection ) {
      bigint selht = selbottom;  selht -= seltop;   selht += 1;
      bigint selwd = selright;   selwd -= selleft;  selwd += 1;
      if ( ht > selht || wd > selwd ) {
         ErrorMessage("Clipboard pattern is bigger than selection.");
         return;
      }

      // set paste rectangle's top left cell coord
      top = seltop;
      left = selleft;

   } else {

      // ask user where to paste the clipboard pattern
      DisplayMessage("Click where you want to paste...");

      // temporarily change cursor to cross
      wxCursor *savecurs = currcurs;
      currcurs = curs_cross;
      // CheckCursor(true);            // probs on Mac if Paste menu item selected
      wxSetCursor(*currcurs);
      viewptr->SetCursor(*currcurs);

      waitingforclick = true;
      EnableAllMenus(false);           // disable all menu items
      UpdateToolBar(false);            // disable all tool bar buttons
      viewptr->CaptureMouse();         // get mouse down event even if outside view
      pasterect = wxRect(-1,-1,0,0);

      while (waitingforclick) {
         wxPoint pt = viewptr->ScreenToClient( wxGetMousePosition() );
         pastex = pt.x;
         pastey = pt.y;
         if (PointInView(pt.x, pt.y)) {
            // determine new paste rectangle
            wxRect newrect;
            SetPasteRect(newrect, wd, ht);
            if (newrect != pasterect) {
               // draw new pasterect
               pasterect = newrect;
               viewptr->Refresh(false, NULL);
               // don't update immediately
               // viewptr->Update();
            }
         } else {
            // mouse outside viewport so erase old pasterect if necessary
            if ( pasterect.width > 0 ) {
               pasterect = wxRect(-1,-1,0,0);
               viewptr->Refresh(false, NULL);
               // don't update immediately
               // viewptr->Update();
            }
         }
         wxMilliSleep(10);             // sleep for a bit
         wxGetApp().Yield(true);       // process events
         #ifdef __WXMAC__
            // need to check if button down due to CaptureMouse bug in wxMac!!!
            if ( Button() ) {
               waitingforclick = false;
               FlushEvents(mDownMask + mUpMask, 0);   // avoid wx seeing click
            }
         #endif
      }

      viewptr->ReleaseMouse();
      EnableAllMenus(true);
   
      // restore cursor
      currcurs = savecurs;
      CheckCursor(frameptr->IsActive());
      
      if ( pasterect.width > 0 ) {
         // erase old pasterect
         viewptr->Refresh(false, NULL);
         // no need to update immediately
         // viewptr->Update();
      }
      
      if ( pastex < 0 || pastex > currview.getxmax() ||
           pastey < 0 || pastey > currview.getymax() ) {
         DisplayMessage("Paste aborted.");
         return;
      }
      
      // set paste rectangle's top left cell coord
      pair<bigint, bigint> clickpos = currview.at(pastex, pastey);
      top = clickpos.second;
      left = clickpos.first;
      bigint halfht = ht;
      bigint halfwd = wd;
      halfht.div2();
      halfwd.div2();
      if (currview.getmag() > 1) {
         if (ht.even()) halfht -= 1;
         if (wd.even()) halfwd -= 1;
      }
      switch (plocation) {
         case TopLeft:     /* no change*/ break;
         case TopRight:    left -= wd; left += 1; break;
         case BottomRight: left -= wd; left += 1; top -= ht; top += 1; break;
         case BottomLeft:  top -= ht; top += 1; break;
         case Middle:      left -= halfwd; top -= halfht; break;
      }
   }

   // check that paste rectangle is within edit limits
   bottom = top;   bottom += ht;   bottom -= 1;
   right = left;   right += wd;    right -= 1;
   if ( OutsideLimits(top, left, bottom, right) ) {
      ErrorMessage("Pasting is not allowed outside +/- 10^9 boundary.");
      return;
   }

   // set pastex,pastey to top left cell of paste rectangle
   pastex = left.toint();
   pastey = top.toint();

   double maxcount = wd.todouble() * ht.todouble();
   int currcount = 0;
   bool abort = false;
   BeginProgress("Pasting pattern");
   
   // copy pattern from temporary universe to current universe
   int tempstate, currstate, tx, ty, cx, cy;
   cy = pastey;
   for ( ty=itop; ty<=ibottom; ty++ ) {
      cx = pastex;
      for ( tx=ileft; tx<=iright; tx++ ) {
         tempstate = tempalgo->getcell(tx, ty);
         switch (pmode) {
            case Copy:
               curralgo->setcell(cx, cy, tempstate);
               break;
            case Or:
               if (tempstate == 1) curralgo->setcell(cx, cy, 1);
               break;
            case Xor:
               currstate = curralgo->getcell(cx, cy);
               if (tempstate == currstate) {
                  if (currstate != 0) curralgo->setcell(cx, cy, 0);
               } else {
                  if (currstate != 1) curralgo->setcell(cx, cy, 1);
               }
               break;
         }
         cx++;
         currcount++;
         if ( (currcount % 1000) == 0 ) {
            abort = AbortProgress((double)currcount / maxcount, "");
            if (abort) break;
         }
      }
      if (abort) break;
      cy++;
   }
   curralgo->endofpattern();
   savestart = true;
   
   EndProgress();
   
   // tidy up and display result
   ClearMessage();
   RefreshPatternAndStatus();
}

void PasteClipboard(bool toselection) {
   if (generating || waitingforclick || !ClipboardHasText()) return;
   if (toselection && !SelectionExists()) return;

#ifdef __WXX11__
   if ( wxFileExists(clipfile) ) {
#else
   wxTextDataObject data;
   if ( GetTextFromClipboard(&data) ) {
      // copy clipboard data to temporary file so we can handle all formats
      // supported by readclipboard
      wxFile tmpfile(clipfile, wxFile::write);
      if ( !tmpfile.IsOpened() ) {
         ErrorMessage("Could not create temporary file!");
         return;
      }
      tmpfile.Write( data.GetText() );
      tmpfile.Close();
#endif         
      // create a temporary universe for storing clipboard pattern
      lifealgo *tempalgo;
      if (hashing)
         tempalgo = new hlifealgo();
      else
         tempalgo = new qlifealgo();
      tempalgo->setpoll(&wx_poller);

      // read clipboard pattern into temporary universe
      bigint top, left, bottom, right;
      const char *err = readclipboard(clipfile, *tempalgo, &top, &left, &bottom, &right);
      if (err != 0) {
         // try toggling temporary universe's type
         delete tempalgo;
         if (hashing)
            tempalgo = new qlifealgo();
         else
            tempalgo = new hlifealgo();
         err = readclipboard(clipfile, *tempalgo, &top, &left, &bottom, &right);
         if (err != 0)
            Warning(err);   // give up
      }
      
      // if we got a pattern then paste it into current universe
      if (err == 0) {
         PasteTemporaryToCurrent(tempalgo, toselection, top, left, bottom, right);
      }
      
      // delete temporary universe and clipboard file
      delete tempalgo;
      #ifdef __WXX11__
         // don't delete clipboard file
      #else
         wxRemoveFile(clipfile);
      #endif
   }
}

void SetPasteLocation(paste_location newloc) {
   plocation = newloc;
}

void CyclePasteLocation() {
   if (plocation == TopLeft) {
      plocation = TopRight;
      if (!waitingforclick) DisplayMessage("Paste location is Top Right.");
   } else if (plocation == TopRight) {
      plocation = BottomRight;
      if (!waitingforclick) DisplayMessage("Paste location is Bottom Right.");
   } else if (plocation == BottomRight) {
      plocation = BottomLeft;
      if (!waitingforclick) DisplayMessage("Paste location is Bottom Left.");
   } else if (plocation == BottomLeft) {
      plocation = Middle;
      if (!waitingforclick) DisplayMessage("Paste location is Middle.");
   } else {
      plocation = TopLeft;
      if (!waitingforclick) DisplayMessage("Paste location is Top Left.");
   }
   if (waitingforclick) {
      // force redraw of paste rectangle if mouse is inside viewport
      pasterect = wxRect(-1,-1,0,0);
   }
}

void SetPasteMode(paste_mode newmode) {
   pmode = newmode;
}

void CyclePasteMode() {
   if (pmode == Copy) {
      pmode = Or;
      if (!waitingforclick) DisplayMessage("Paste mode is Or.");
   } else if (pmode == Or) {
      pmode = Xor;
      if (!waitingforclick) DisplayMessage("Paste mode is Xor.");
   } else {
      pmode = Copy;
      if (!waitingforclick) DisplayMessage("Paste mode is Copy.");
   }
   if (waitingforclick) {
      // force redraw of paste rectangle if mouse is inside viewport
      pasterect = wxRect(-1,-1,0,0);
   }
}

void DisplaySelectionSize() {
   if (waitingforclick) return;
   bigint wd = selright;    wd -= selleft;   wd += bigint::one;
   bigint ht = selbottom;   ht -= seltop;    ht += bigint::one;
   char s[128];
   sprintf(s, "Selection wd x ht = %g x %g", wd.todouble(), ht.todouble() );
   SetMessage(s);
}

void SelectAll() {
   if (SelectionExists()) {
      NoSelection();
      RefreshPatternAndStatus();
   }

   if (curralgo->isEmpty()) {
      DisplayMessage(empty_pattern);
      return;
   }
   
   curralgo->findedges(&seltop, &selleft, &selbottom, &selright);
   DisplaySelectionSize();
   RefreshPatternAndStatus();
}

void RemoveSelection() {
   if (SelectionExists()) {
      NoSelection();
      RefreshPatternAndStatus();
   }
}

void SetCursorMode(wxCursor *newcurs) {
   currcurs = newcurs;
}

void CycleCursorMode() {
   if (drawingcells || selectingcells || movingview || waitingforclick)
      return;
   if (currcurs == curs_pencil)
      currcurs = curs_cross;
   else if (currcurs == curs_cross)
      currcurs = curs_hand;
   else if (currcurs == curs_hand)
      currcurs = curs_zoomin;
   else if (currcurs == curs_zoomin)
      currcurs = curs_zoomout;
   else
      currcurs = curs_pencil;
}

void ShowDrawing() {
   curralgo->endofpattern();
   savestart = true;
   // update status bar
   if (statusht > 0) {
      statusptr->Refresh(false, NULL);
   }
}

void DrawOneCell(int cx, int cy, wxDC &dc) {
   int cellsize = 1 << currview.getmag();

   // convert given cell coords to view coords
   pair<bigint, bigint> lefttop = currview.at(0, 0);
   wxCoord x = (cx - lefttop.first.toint()) * cellsize;
   wxCoord y = (cy - lefttop.second.toint()) * cellsize;
   
   if (cellsize > 2) {
      cellsize--;       // allow for gap between cells
   }
   dc.DrawRectangle(x, y, cellsize, cellsize);
   
   // overlay selection image if cell is within selection
   #ifdef __WXX11__
      if ( SelectionExists() &&
           cx >= selleft.toint() && cx <= selright.toint() &&
           cy >= seltop.toint() && cy <= selbottom.toint() ) {
         wxRect r = wxRect(x, y, cellsize, cellsize);
         DrawX11Selection(dc, r);
      }
   #else
      if ( SelectionExists() && selbitmap &&
           cx >= selleft.toint() && cx <= selright.toint() &&
           cy >= seltop.toint() && cy <= selbottom.toint() ) {
         wxMemoryDC memDC;
         memDC.SelectObject(*selbitmap);
         dc.Blit(x, y, cellsize, cellsize, &memDC, 0, 0, wxCOPY, true);
      }
   #endif
}

void StartDrawingCells(int x, int y) {
   pair<bigint, bigint> cellpos = currview.at(x, y);
   // check that cellpos is within getcell/setcell limits
   if ( OutsideLimits(cellpos.second, cellpos.first, cellpos.second, cellpos.first) ) {
      ErrorMessage("Drawing is not allowed outside +/- 10^9 boundary.");
      return;
   }

   cellx = cellpos.first.toint();
   celly = cellpos.second.toint();
   drawstate = 1 - curralgo->getcell(cellx, celly);
   curralgo->setcell(cellx, celly, drawstate);

   wxClientDC dc(viewptr);
   dc.BeginDrawing();
   dc.SetPen(*wxTRANSPARENT_PEN);
   dc.SetBrush(drawstate == (int)blackcells ? *wxBLACK_BRUSH : *wxWHITE_BRUSH);
   DrawOneCell(cellx, celly, dc);
   dc.SetBrush(wxNullBrush);        // restore brush
   dc.SetPen(wxNullPen);            // restore pen
   dc.EndDrawing();
   
   ShowDrawing();

   drawingcells = true;
   viewptr->CaptureMouse();         // get mouse up event even if outside view
   dragtimer->Start(dragrate);
}

void DrawCells(int x, int y) {
   pair<bigint, bigint> cellpos = currview.at(x, y);
   if ( currview.getmag() < 0 ||
        OutsideLimits(cellpos.second, cellpos.first, cellpos.second, cellpos.first) ) {
      return;
   }

   int newx = cellpos.first.toint();
   int newy = cellpos.second.toint();
   if ( newx != cellx || newy != celly ) {
      wxClientDC dc(viewptr);
      dc.BeginDrawing();
      dc.SetPen(*wxTRANSPARENT_PEN);
      dc.SetBrush(drawstate == (int)blackcells ? *wxBLACK_BRUSH : *wxWHITE_BRUSH);

      int numchanged = 0;
      
      // draw a line of cells using Bresenham's algorithm;
      // this code comes from Guillermo Garcia's Life demo supplied with wx
      int d, ii, jj, di, ai, si, dj, aj, sj;
      di = newx - cellx;
      ai = abs(di) << 1;
      si = (di < 0)? -1 : 1;
      dj = newy - celly;
      aj = abs(dj) << 1;
      sj = (dj < 0)? -1 : 1;
      
      ii = cellx;
      jj = celly;
      
      if (ai > aj) {
         d = aj - (ai >> 1);
         while (ii != newx) {
            if ( curralgo->getcell(ii, jj) != drawstate) {
               curralgo->setcell(ii, jj, drawstate);
               numchanged++;
               DrawOneCell(ii, jj, dc);
            }
            if (d >= 0) {
               jj += sj;
               d  -= ai;
            }
            ii += si;
            d  += aj;
         }
      } else {
         d = ai - (aj >> 1);
         while (jj != newy) {
            if ( curralgo->getcell(ii, jj) != drawstate) {
               curralgo->setcell(ii, jj, drawstate);
               numchanged++;
               DrawOneCell(ii, jj, dc);
            }
            if (d >= 0) {
               ii += si;
               d  -= aj;
            }
            jj += sj;
            d  += ai;
         }
      }
      
      cellx = newx;
      celly = newy;
      
      if ( curralgo->getcell(cellx, celly) != drawstate) {
         curralgo->setcell(cellx, celly, drawstate);
         numchanged++;
         DrawOneCell(cellx, celly, dc);
      }
      
      dc.SetBrush(wxNullBrush);     // restore brush
      dc.SetPen(wxNullPen);         // restore pen
      dc.EndDrawing();
      
      if ( numchanged > 0 ) ShowDrawing();
   }
}

void ModifySelection(bigint &xclick, bigint &yclick) {
   // note that we include "=" in following tests to get sensible
   // results when modifying small selections (ht or wd <= 3)
   if ( yclick <= seltop && xclick <= selleft ) {
      // click is in or outside top left corner
      seltop = yclick;
      selleft = xclick;
      anchory = selbottom;
      anchorx = selright;

   } else if ( yclick <= seltop && xclick >= selright ) {
      // click is in or outside top right corner
      seltop = yclick;
      selright = xclick;
      anchory = selbottom;
      anchorx = selleft;

   } else if ( yclick >= selbottom && xclick >= selright ) {
      // click is in or outside bottom right corner
      selbottom = yclick;
      selright = xclick;
      anchory = seltop;
      anchorx = selleft;

   } else if ( yclick >= selbottom && xclick <= selleft ) {
      // click is in or outside bottom left corner
      selbottom = yclick;
      selleft = xclick;
      anchory = seltop;
      anchorx = selright;
   
   } else if (yclick <= seltop) {
      // click is in or above top edge
      forcev = true;
      seltop = yclick;
      anchory = selbottom;
   
   } else if (yclick >= selbottom) {
      // click is in or below bottom edge
      forcev = true;
      selbottom = yclick;
      anchory = seltop;
   
   } else if (xclick <= selleft) {
      // click is in or left of left edge
      forceh = true;
      selleft = xclick;
      anchorx = selright;
   
   } else if (xclick >= selright) {
      // click is in or right of right edge
      forceh = true;
      selright = xclick;
      anchorx = selleft;
   
   } else {
      // click is somewhere inside selection
      double wd = selright.todouble() - selleft.todouble() + 1.0;
      double ht = selbottom.todouble() - seltop.todouble() + 1.0;
      double onethirdx = selleft.todouble() + wd / 3.0;
      double twothirdx = selleft.todouble() + wd * 2.0 / 3.0;
      double onethirdy = seltop.todouble() + ht / 3.0;
      double twothirdy = seltop.todouble() + ht * 2.0 / 3.0;
      double midy = seltop.todouble() + ht / 2.0;
      double x = xclick.todouble();
      double y = yclick.todouble();
      
      if ( y < onethirdy && x < onethirdx ) {
         // click is near top left corner
         seltop = yclick;
         selleft = xclick;
         anchory = selbottom;
         anchorx = selright;
      
      } else if ( y < onethirdy && x > twothirdx ) {
         // click is near top right corner
         seltop = yclick;
         selright = xclick;
         anchory = selbottom;
         anchorx = selleft;
   
      } else if ( y > twothirdy && x > twothirdx ) {
         // click is near bottom right corner
         selbottom = yclick;
         selright = xclick;
         anchory = seltop;
         anchorx = selleft;
   
      } else if ( y > twothirdy && x < onethirdx ) {
         // click is near bottom left corner
         selbottom = yclick;
         selleft = xclick;
         anchory = seltop;
         anchorx = selright;

      } else if ( x < onethirdx ) {
         // click is near middle of left edge
         forceh = true;
         selleft = xclick;
         anchorx = selright;

      } else if ( x > twothirdx ) {
         // click is near middle of right edge
         forceh = true;
         selright = xclick;
         anchorx = selleft;

      } else if ( y < midy ) {
         // click is below middle section of top edge
         forcev = true;
         seltop = yclick;
         anchory = selbottom;
      
      } else {
         // click is above middle section of bottom edge
         forcev = true;
         selbottom = yclick;
         anchory = seltop;
      }
   }
}

void StartSelectingCells(int x, int y, bool shiftkey) {
   pair<bigint, bigint> cellpos = currview.at(x, y);
   anchorx = cellpos.first;
   anchory = cellpos.second;

   // set previous selection to anything impossible
   prevtop = 1;
   prevleft = 1;
   prevbottom = 0;
   prevright = 0;
   
   // for avoiding 1x1 selection if mouse doesn't move much
   initselx = x;
   initsely = y;
   
   // allow changing size in any direction
   forceh = false;
   forcev = false;
   
   if (SelectionExists()) {
      if (shiftkey) {
         // modify current selection
         ModifySelection(cellpos.first, cellpos.second);
         DisplaySelectionSize();
         RefreshPatternAndStatus();
      } else {
         // remove current selection
         NoSelection();
         RefreshPatternAndStatus();
      }
   }
   
   selectingcells = true;
   viewptr->CaptureMouse();         // get mouse up event even if outside view
   dragtimer->Start(dragrate);
}

void SelectCells(int x, int y) {
   if ( abs(initselx - x) < 2 && abs(initsely - y) < 2 && !SelectionExists() ) {
      // avoid 1x1 selection if mouse hasn't moved much
      return;
   }

   pair<bigint, bigint> cellpos = currview.at(x, y);
   if (!forcev) {
      if (cellpos.first <= anchorx) {
         selleft = cellpos.first;
         selright = anchorx;
      } else {
         selleft = anchorx;
         selright = cellpos.first;
      }
   }
   if (!forceh) {
      if (cellpos.second <= anchory) {
         seltop = cellpos.second;
         selbottom = anchory;
      } else {
         seltop = anchory;
         selbottom = cellpos.second;
      }
   }

   if ( seltop != prevtop || selbottom != prevbottom ||
        selleft != prevleft || selright != prevright ) {
      // selection has changed
      DisplaySelectionSize();
      RefreshPatternAndStatus();
      prevtop = seltop;
      prevbottom = selbottom;
      prevleft = selleft;
      prevright = selright;
   }
}

void StartMovingView(int x, int y) {
   pair<bigint, bigint> cellpos = currview.at(x, y);
   bigcellx = cellpos.first;
   bigcelly = cellpos.second;
   movingview = true;
   viewptr->CaptureMouse();         // get mouse up event even if outside view
   dragtimer->Start(dragrate);
}

void MoveView(int x, int y) {
   pair<bigint, bigint> cellpos = currview.at(x, y);
   bigint newx = cellpos.first;
   bigint newy = cellpos.second;
   bigint xdelta = bigcellx;
   bigint ydelta = bigcelly;
   xdelta -= newx;
   ydelta -= newy;

   int xamount, yamount;
   int mag = currview.getmag();
   if (mag >= 0) {
      // move an integral number of cells
      xamount = xdelta.toint() << mag;
      yamount = ydelta.toint() << mag;
   } else {
      // convert cell deltas to screen pixels
      xdelta >>= -mag;
      ydelta >>= -mag;
      xamount = xdelta.toint();
      yamount = ydelta.toint();
   }

   if ( xamount != 0 || yamount != 0 ) {
      currview.move(xamount, yamount);
      RefreshPatternAndStatus();
      cellpos = currview.at(x, y);
      bigcellx = cellpos.first;
      bigcelly = cellpos.second;
   }
}

void StopDraggingMouse() {
   if (selectingcells)
      UpdateMenuItems(true);        // update Edit menu items
   drawingcells = false;
   selectingcells = false;
   movingview = false;
   if ( viewptr->HasCapture() ) viewptr->ReleaseMouse();
   if ( dragtimer->IsRunning() ) dragtimer->Stop();
}

void TestAutoFit() {
   if (autofit && generating) {
      // assume user no longer wants us to do autofitting
      autofit = false;
   }
}

// user has clicked somewhere in viewport
void ProcessClick(int x, int y, bool shiftkey) {
   showbanner = false;

   if (currcurs == curs_pencil) {
      if (generating) {
         ErrorMessage("Drawing is not allowed while generating.");
         return;
      }
      if (currview.getmag() < 0) {
         ErrorMessage("Drawing is not allowed if more than 1 cell per pixel.");
         return;
      }
      StartDrawingCells(x, y);

   } else if (currcurs == curs_cross) {
      TestAutoFit();
      StartSelectingCells(x, y, shiftkey);

   } else if (currcurs == curs_hand) {
      TestAutoFit();
      StartMovingView(x, y);

   } else if (currcurs == curs_zoomin) {
      TestAutoFit();
      // zoom in so that clicked cell stays under cursor
      if (currview.getmag() < MAX_MAG) {
         currview.zoom(x, y);
         RefreshWindow();
      } else {
         wxBell();   // can't zoom in any further
      }

   } else if (currcurs == curs_zoomout) {
      TestAutoFit();
      // zoom out so that clicked cell stays under cursor
      currview.unzoom(x, y);
      RefreshWindow();
   }
}

// -----------------------------------------------------------------------------

// control functions

void ChangeGoToStop() {
   /* doesn't work on Windows -- all the other tools go missing!!!
   // replace tool bar's go button with stop button
   wxToolBar *tbar = frameptr->GetToolBar();
   if (tbar) {
      tbar->RemoveTool(ID_GO);
      tbar->InsertTool(0, stoptool);
      tbar->Realize();
   }
   */
}

void ChangeStopToGo() {
   /* doesn't work on Windows!!!
   // replace tool bar's stop button with go button
   wxToolBar *tbar = frameptr->GetToolBar();
   if (tbar) {
      tbar->RemoveTool(ID_STOP);
      tbar->InsertTool(0, gotool);
      tbar->Realize();
   }
   */
}

bool SaveStartingPattern() {
   if ( curralgo->getGeneration() > bigint::zero ) {
      // don't save pattern if gen count > 0
      return true;
   }

   if ( gen0algo ) {
      // delete old starting pattern
      delete gen0algo;
      gen0algo = NULL;
   }
   
   if ( !savestart ) {
      // no need to save pattern stored in currfile
      return true;
   }

   // only save pattern if its edges are within getcell/setcell limits
   bigint top, left, bottom, right;
   curralgo->findedges(&top, &left, &bottom, &right);
   if ( OutsideLimits(top, left, bottom, right) ) {
      ErrorMessage("Starting pattern is outside +/- 10^9 boundary.");
      // ask user if they want to continue generating???
      return false;
   }   
   
   // save current rule
   strncpy(gen0rule, curralgo->getrule(), sizeof(gen0rule));
   
   // save type of universe
   gen0hash = hashing;
   
   // create gen0algo and duplicate current pattern
   if ( hashing ) {
      gen0algo = new hlifealgo();
   } else {
      gen0algo = new qlifealgo();
   }
   gen0algo->setpoll(&wx_poller);

   // copy (non-empty) pattern in current universe to gen0algo;
   // slow for large patterns so ask Tom if it's possible to
   // write a fast universe duplicator???
   int itop = top.toint();
   int ileft = left.toint();
   int ibottom = bottom.toint();
   int iright = right.toint();
   int wd = iright - ileft + 1;
   int ht = ibottom - itop + 1;
   int cx, cy;
   double maxcount = (double)wd * (double)ht;
   int currcount = 0;
   bool abort = false;
   BeginProgress("Saving starting pattern");
   for ( cy=itop; cy<=ibottom; cy++ ) {
      for ( cx=ileft; cx<=iright; cx++ ) {
         if ( curralgo->getcell(cx, cy) == 1 ) {
            gen0algo->setcell(cx, cy, 1);
         }
         currcount++;
         if ( (currcount % 1000) == 0 ) {
            abort = AbortProgress((double)currcount / maxcount, "");
            if (abort) break;
         }
      }
      if (abort) break;
   }
   gen0algo->endofpattern();
   EndProgress();

   // if (abort) return false; ???
   // or put following in a modal dlg with Cancel (default) and Continue buttons:
   // "The starting pattern has not been saved and cannot be restored if you continue."
   
   return true;
}

void GoFaster() {
   warp++;
   SetGenIncrement();
   // only need to refresh status bar
   RefreshStatus();
   if (generating && warp < 0) {
      gendelay = MIN_DELAY * (1 << (-warp - 1));
      whentosee -= gendelay;
   }
}

void GoSlower() {
   if (warp > MIN_WARP) {
      warp--;
      SetGenIncrement();
      // only need to refresh status bar
      RefreshStatus();
      if (generating && warp < 0) {
         gendelay = MIN_DELAY * (1 << (-warp - 1));
         whentosee += gendelay;
      }
   } else {
      wxBell();
   }
}

void GeneratePattern() {
   if (generating || drawingcells || waitingforclick) {
      wxBell();
      return;
   }
   
   if (curralgo->isEmpty()) {
      DisplayMessage(empty_pattern);
      return;
   }
   
   if (!SaveStartingPattern()) {
      return;
   }

   // for DisplayTimingInfo
   starttime = wxGetElapsedTime(false);
   startgen = curralgo->getGeneration().todouble();
   
   generating = true;               // avoid recursion
   ChangeGoToStop();
   wx_poller.resetInterrupted();
   wx_poller.nextcheck = 0;
   UpdateUserInterface(frameptr->IsActive());
   
   if (warp < 0) {
      gendelay = MIN_DELAY * (1 << (-warp - 1));
      whentosee = wxGetElapsedTime(false) + gendelay;
   }
   int hypdown = 64;

   while (true) {
      if (warp < 0) {
         // slow down by only doing one gen every gendelay millisecs
         long currmsec = wxGetElapsedTime(false);
         if (currmsec >= whentosee) {
            curralgo->step();
            if (autofit) curralgo->fit(currview, 0);
            // don't call RefreshWindow() -- no need to update menu/tool/scroll bars
            RefreshPatternAndStatus();
            if (wx_poller.checkevents()) break;
            whentosee = currmsec + gendelay;
         } else {
            // process events while we wait
            if (wx_poller.checkevents()) break;
         }
      } else {
         // warp >= 0 so only show results every curralgo->getIncrement() gens
         curralgo->step();
         if (autofit) curralgo->fit(currview, 0);
         // don't call RefreshWindow() -- no need to update menu/tool/scroll bars
         RefreshPatternAndStatus();
         if (wx_poller.checkevents()) break;
         if (hyperspeed && curralgo->hyperCapable()) {
            hypdown--;
            if (hypdown == 0) {
               hypdown = 64;
               GoFaster();
            }
         }
      }
   }

   generating = false;

   // for DisplayTimingInfo
   endtime = wxGetElapsedTime(false);
   endgen = curralgo->getGeneration().todouble();
   
   ChangeStopToGo();
   
   // display the final pattern
   if (autofit) curralgo->fit(currview, 0);
   RefreshWindow();
   // UpdateUserInterface has been called
}

void StopGenerating() {
   if (generating) {
      wx_poller.setInterrupted();
      wx_poller.nextcheck = 0;
   }
}

void DisplayTimingInfo() {
   if (waitingforclick) return;
   if (generating) {
      endtime = wxGetElapsedTime(false);
      endgen = curralgo->getGeneration().todouble();
   }
   if (endtime > starttime) {
      char s[128];
      sprintf(s,"%g gens in %g secs (%g gens/sec)",
                endgen - startgen, (double)(endtime - starttime) / 1000.0,
                (double)(endgen - startgen) / ((double)(endtime - starttime) / 1000.0));
      DisplayMessage(s);
   }
}

void NextGeneration(bool useinc) {
   if (generating || drawingcells || waitingforclick) {
      // don't play sound here because it'll be heard if user holds down space/tab key
      // wxBell();
      return;
   }

   if (curralgo->isEmpty()) {
      DisplayMessage(empty_pattern);
      return;
   }
   
   if (!SaveStartingPattern()) {
      return;
   }
      
   // curralgo->step() calls checkevents so set generating flag to avoid recursion
   generating = true;
   ChangeGoToStop();
   wx_poller.resetInterrupted();
   wx_poller.nextcheck = 0;
   CheckCursor(frameptr->IsActive());

   if (useinc) {
      // step by current increment
      if (curralgo->getIncrement() > bigint::one) {
         UpdateToolBar(frameptr->IsActive());
         UpdateMenuItems(frameptr->IsActive());
      }
      curralgo->step();
   } else {
      // make sure we only step by one gen
      bigint saveinc = curralgo->getIncrement();
      curralgo->setIncrement(1);
      curralgo->step();
      curralgo->setIncrement(saveinc);
   }

   generating = false;
   ChangeStopToGo();
   
   // autofit is only used when doing many gens
   if (autofit && useinc && curralgo->getIncrement() > bigint::one)
      curralgo->fit(currview, 0);
   RefreshWindow();
}

void ToggleAutoFit() {
   autofit = !autofit;
   // we only use autofit when generating; that's why the Auto Fit item
   // is in the Control menu and not in the View menu
   if (autofit && generating) {
      curralgo->fit(currview, 0);
      RefreshWindow();
   }
}

void ToggleHashing() {
   if ( generating ) {
      wxBell();
      return;
   }

   if ( global_liferules.hasB0notS8 && !hashing ) {
      ErrorMessage("Hashing cannot be used with a B0-not-S8 rule.");
      return;
   }

   // check if current pattern is too big to use getcell/setcell
   bigint top, left, bottom, right;
   if ( !curralgo->isEmpty() ) {
      curralgo->findedges(&top, &left, &bottom, &right);
      if ( OutsideLimits(top, left, bottom, right) ) {
         ErrorMessage("Pattern cannot be converted (outside +/- 10^9 boundary).");
         // ask user if they want to continue anyway???
         return;
      }
   }

   // toggle hashing option and update status bar immediately
   hashing = !hashing;
   warp = 0;
   RefreshStatus();

   // create a new universe of the right flavor
   lifealgo *newalgo;
   if ( hashing ) {
      newalgo = new hlifealgo();
      newalgo->setMaxMemory(maxhmem);
   } else {
      newalgo = new qlifealgo();
   }
   newalgo->setpoll(&wx_poller);

   // set same gen count
   newalgo->setGeneration( curralgo->getGeneration() );

   if ( !curralgo->isEmpty() ) {
      // copy pattern in current universe to new universe
      int itop = top.toint();
      int ileft = left.toint();
      int ibottom = bottom.toint();
      int iright = right.toint();
      int wd = iright - ileft + 1;
      int ht = ibottom - itop + 1;
      int cx, cy;
   
      double maxcount = (double)wd * (double)ht;
      int currcount = 0;
      bool abort = false;
      BeginProgress("Converting pattern");
   
      // very slow for large patterns -- ask Tom if it's possible to
      // write fast qlife<->hlife conversion routines???
      for ( cy=itop; cy<=ibottom; cy++ ) {
         for ( cx=ileft; cx<=iright; cx++ ) {
            if ( curralgo->getcell(cx, cy) == 1 ) {
               newalgo->setcell(cx, cy, 1);
            }
            currcount++;
            if ( (currcount % 1000) == 0 ) {
               abort = AbortProgress((double)currcount / maxcount, "");
               if (abort) break;
            }
         }
         if (abort) break;
      }
      newalgo->endofpattern();
      
      EndProgress();
   }
   
   // delete old universe and point current universe to new universe
   delete curralgo;
   curralgo = newalgo;
   SetGenIncrement();
   RefreshWindow();
}

void ToggleHyperspeed() {
   if ( curralgo->hyperCapable() ) {
      hyperspeed = !hyperspeed;
   }
}

// let user change current rule
void ChangeRule() {
   if (generating) {
      wxBell();
   } else {
      // eventually use a more sophisticated dialog with a pop-up menu
      // containing named rules like Conway's Life, HighLife, etc!!!
      wxString oldrule = wxT( curralgo->getrule() );
      wxString newrule = wxGetTextFromUser( _T("Enter rule using B0..8/S0..8 notation:"),
                                            _T("Change rule"),
                                            oldrule,
                                            frameptr );
      if ( !newrule.empty() ) {
         const char *err = curralgo->setrule( (char *)newrule.c_str() );
         // restore old rule if an error occurred
         if ( err != 0 ) {
            Warning(err);
            curralgo->setrule( (char *)oldrule.c_str() );
         } else if ( global_liferules.hasB0notS8 && hashing ) {
            Warning("B0-not-S8 rules are not allowed when hashing.");
            curralgo->setrule( (char *)oldrule.c_str() );
         } else {
            // show new rule in window title
            SetWindowTitle(currname);
         }
      }
   }
}

void ChangeMaxMemory() {
   if (generating || !hashing) {
      wxBell();
   } else {
      long res = wxGetNumberFromUser( _T("Specify the maximum amount of memory\n")
                                      _T("to be used when hashing patterns."),
                                      _T("In megabytes:"), _T("Maximum hash memory"),
                                      curralgo->getMaxMemory(), minhashmb, maxhashmb,
                                      frameptr );
      if (res != -1) {
         if (res < minhashmb) res = minhashmb;
         if (res > maxhashmb) res = maxhashmb;
         maxhmem = res;
         curralgo->setMaxMemory(maxhmem);
      }
   }
}

// -----------------------------------------------------------------------------

// viewing functions

void PanUp(int amount) {
   TestAutoFit();
   currview.move(0, -amount);
   RefreshWindow();
}

void PanDown(int amount) {
   TestAutoFit();
   currview.move(0, amount);
   RefreshWindow();
}

void PanLeft(int amount) {
   TestAutoFit();
   currview.move(-amount, 0);
   RefreshWindow();
}

void PanRight(int amount) {
   TestAutoFit();
   currview.move(amount, 0);
   RefreshWindow();
}

// zoom out so that central cell stays central
void ZoomOut() {
   TestAutoFit();
   currview.unzoom();
   RefreshWindow();
}

// zoom in so that central cell stays central
void ZoomIn() {
   TestAutoFit();
   if (currview.getmag() < MAX_MAG) {
      currview.zoom();
      RefreshWindow();
   } else {
      wxBell();
   }
}

void SetPixelsPerCell(int pxlspercell) {
   int newmag = 0;
   while (pxlspercell > 1) {
      newmag++;
      pxlspercell >>= 1;
   }
   if (newmag == currview.getmag()) return;
   TestAutoFit();
   currview.setmag(newmag);
   RefreshWindow();
}

void FitPattern() {
   FitInView();
   RefreshWindow();
}

void ViewMiddle() {
   // put 0,0 in middle of view
   currview.center();
   RefreshWindow();
}

// set viewport size
void SetViewSize() {
   int wd, ht;
   viewptr->GetClientSize(&wd, &ht);
   // wd or ht might be < 1 on Win/X11 platforms
   if (wd < 1) wd = 1;
   if (ht < 1) ht = 1;
   currview.resize(wd, ht);
   // only autofit when generating
   if (autofit && generating)
      curralgo->fit(currview, 0);
}

void ToggleStatusBar() {
   int wd, ht;
   frameptr->GetClientSize(&wd, &ht);
   if (statusht > 0) {
      statusht = 0;
      statusptr->SetSize(0, 0, 0, 0);
      #ifdef __WXX11__
         // move so we don't see small portion
         statusptr->Move(-100, -100);
      #endif
   } else {
      statusht = STATUS_HT;
      statusptr->SetSize(0, 0, wd, statusht);
   }
   viewptr->SetSize(0, statusht, wd, ht > statusht ? ht - statusht : 0);
   SetViewSize();
   RefreshWindow();
}

void ToggleToolBar() {
   #ifdef __WXX11__
      // Show(false) does not hide tool bar!!!
      ErrorMessage("Sorry, tool bar hiding is not implemented for X11.");
   #else
      wxToolBar *tbar = frameptr->GetToolBar();
      if (tbar) {
         if (tbar->IsShown()) {
            tbar->Show(false);
         } else {
            tbar->Show(true);
         }
         int wd, ht;
         frameptr->GetClientSize(&wd, &ht);
         if (statusht > 0) {
            // adjust size of status bar
            statusptr->SetSize(0, 0, wd, statusht);
         }
         // adjust size of viewport
         viewptr->SetSize(0, statusht, wd, ht > statusht ? ht - statusht : 0);
         SetViewSize();
         RefreshWindow();
      }
   #endif
}

void ToggleFullScreen() {
   #ifdef __WXX11__
      // ShowFullScreen(true) does nothing!!!
      ErrorMessage("Sorry, full screen mode is not implemented for X11.");
   #else
      fullscreen = !fullscreen;
      frameptr->ShowFullScreen(fullscreen,
                   // don't use wxFULLSCREEN_ALL because that prevents tool bar being
                   // toggled in full screen mode on Windows
                   wxFULLSCREEN_NOMENUBAR | wxFULLSCREEN_NOBORDER | wxFULLSCREEN_NOCAPTION);
      wxToolBar *tbar = frameptr->GetToolBar();
      if (fullscreen) {
         // hide scroll bars
         viewptr->SetScrollbar(wxHORIZONTAL, 0, 0, 0, true);
         viewptr->SetScrollbar(wxVERTICAL, 0, 0, 0, true);
         // hide status bar if necessary
         restorestatus = statusht > 0;
         if (statusht > 0) {
            statusht = 0;
            statusptr->SetSize(0, 0, 0, 0);
         }
         // hide tool bar if necessary
         restoretoolbar = tbar && tbar->IsShown();
         if (tbar && tbar->IsShown()) {
            tbar->Show(false);
         }
      } else {
         // first show tool bar if necessary
         if (restoretoolbar && tbar && !tbar->IsShown()) {
            tbar->Show(true);
         }
         // now show status bar if necessary
         // note that even if statusht > 0 we may have to resize width
         if (restorestatus) {
            statusht = STATUS_HT;
            int wd, ht;
            frameptr->GetClientSize(&wd, &ht);
            statusptr->SetSize(0, 0, wd, statusht);
         }
      }
      // adjust size of viewport
      int wd, ht;
      frameptr->GetClientSize(&wd, &ht);
      viewptr->SetSize(0, statusht, wd, ht > statusht ? ht - statusht : 0);
      SetViewSize();
      RefreshWindow();     // calls UpdateScrollBars
   #endif
}

void ToggleGridLines() {
   showgridlines = !showgridlines;
   if (currview.getmag() >= MIN_GRID_MAG)
      RefreshWindow();
}

void ToggleVideo() {
   blackcells = !blackcells;
   RefreshWindow();
}

void ToggleBuffering() {
   buffered = !buffered;
   RefreshWindow();
}

// -----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(InfoFrame, wxFrame)
   EVT_BUTTON     (wxID_CLOSE,   InfoFrame::OnCloseButton)
   EVT_CLOSE      (              InfoFrame::OnClose)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(TextView, wxTextCtrl)
   EVT_CHAR       (TextView::OnChar)
   EVT_SET_FOCUS  (TextView::OnSetFocus)
END_EVENT_TABLE()

void TextView::OnChar(wxKeyEvent& event) {
   int key = event.GetKeyCode();
   if ( event.CmdDown() || event.AltDown() ) {
      // let default handler see things like cmd-C 
      event.Skip();
   } else {
      // let escape/return/enter key close info window
      if ( key == WXK_ESCAPE || key == WXK_RETURN || key == WXK_NUMPAD_ENTER) {
         infoptr->Close(true);
      } else {
         event.Skip();
      }
   }
}

void TextView::OnSetFocus(wxFocusEvent& WXUNUSED(event)) {
   #ifdef __WXMAC__
      // wxMac prob: remove focus ring around read-only textctrl???!!!
      //!!! infopanel->SetFocus();
   #endif
}

// create the pattern info window
InfoFrame::InfoFrame(char *comments)
   : wxFrame(NULL, wxID_ANY, _("Pattern Info"),
             wxPoint(infox,infoy), wxSize(infowd,infoht))
{
   SetFrameIcon(this);

   #ifdef __WXMSW__
      // avoid default background colour (dark grey)
      SetBackgroundColour(*wxLIGHT_GREY);
   #endif

   TextView* textctrl = new TextView(this, wxID_ANY, wxEmptyString,
                                 wxDefaultPosition, wxDefaultSize,
                                 wxTE_RICH | // needed for font changing on Windows
                                 wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);

   // use a fixed-width font
   wxTextAttr textattr = wxTextAttr(wxNullColour, wxNullColour,
                                 #ifdef __WXMAC__
                                    wxFont(11, wxMODERN, wxNORMAL, wxNORMAL));
                                 #else
                                    wxFont(10, wxMODERN, wxNORMAL, wxNORMAL));
                                 #endif
   textctrl->SetDefaultStyle(textattr);   // doesn't change font on X11!!!
   textctrl->WriteText(comments[0] == 0 ? "No comments found." : comments);
   textctrl->ShowPosition(0);
   textctrl->SetInsertionPoint(0);        // needed to change pos on X11

   wxButton *closebutt = new wxButton(this, wxID_CLOSE, "Close");
   closebutt->SetDefault();
   
   wxBoxSizer *vbox = new wxBoxSizer(wxVERTICAL);
   vbox->Add(textctrl, 1, wxLEFT | wxRIGHT | wxTOP | wxEXPAND | wxALIGN_TOP, 10);
   vbox->Add(closebutt, 0, wxALL | wxALIGN_CENTER, 10);

   SetMinSize(wxSize(mininfowd, mininfoht));
   SetSizer(vbox);

   #ifdef __WXMAC__
      // expand sizer now to avoid seeing small htmlwin and buttons in top left corner
      vbox->SetDimension(0, 0, infowd, infoht);
   #endif
}

void InfoFrame::OnCloseButton(wxCommandEvent& WXUNUSED(event)) {
   Close(true);
}

void InfoFrame::OnClose(wxCloseEvent& WXUNUSED(event)) {
   // save current location and size for later use in SavePrefs
   wxRect r = infoptr->GetRect();
   infox = r.x;
   infoy = r.y;
   infowd = r.width;
   infoht = r.height;
   
   Destroy();        // also deletes all child windows (buttons, etc)
   infoptr = NULL;
}

void ShowPatternInfo() {
   if (waitingforclick || currfile[0] == 0) return;

   if (infoptr) {
      // info window exists so just bring it to front
      infoptr->Raise();
      #ifdef __WXX11__
         infoptr->SetFocus();    // activate window
      #endif
      return;
   }

   // create a 32K buffer for receiving comment data
   char *commptr;
   const int maxcommsize = 32 * 1024;
   commptr = (char *)malloc(maxcommsize);
   if (commptr == NULL) {
      ErrorMessage("Not enough memory for comments!");
      return;
   }

   // read and display comments in current pattern file
   const char *err;
   err = readcomments(currfile, commptr, maxcommsize);
   if (err != 0) {
      Warning(err);
   } else {
      infoptr = new InfoFrame(commptr);
      if (infoptr == NULL) {
         Warning("Could not create info window!");
      } else {
         infoptr->Show(true);
         #ifdef __WXX11__
            // avoid wxX11 bug (probably caused by earlier SetMinSize call);
            // info window needs to be moved to infox,infoy
            infoptr->Lower();
            // don't call Yield -- doesn't work if we're generating
            while (wxGetApp().Pending()) wxGetApp().Dispatch();
            infoptr->Move(infox, infoy);
            // note that Move clobbers effect of SetMinSize!!!
            infoptr->Raise();
         #endif
      }
   }
   
   free(commptr);
}

// -----------------------------------------------------------------------------

// we use wxHTML to display .html files stored in the Help folder

BEGIN_EVENT_TABLE(HelpFrame, wxFrame)
   EVT_BUTTON     (ID_BACK_BUTT,       HelpFrame::OnBackButton)
   EVT_BUTTON     (ID_FORWARD_BUTT,    HelpFrame::OnForwardButton)
   EVT_BUTTON     (ID_CONTENTS_BUTT,   HelpFrame::OnContentsButton)
   EVT_BUTTON     (wxID_CLOSE,         HelpFrame::OnCloseButton)
   EVT_CLOSE      (                    HelpFrame::OnClose)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(HtmlView, wxHtmlWindow)
   EVT_CHAR       (HtmlView::OnChar)
END_EVENT_TABLE()

wxButton *backbutt;        // back button
wxButton *forwbutt;        // forwards button
wxButton *contbutt;        // Contents button

// current help file
char currhelp[64] = "Help/index.html";

// create the help window
HelpFrame::HelpFrame()
   : wxFrame(NULL, wxID_ANY, _(""), wxPoint(helpx,helpy), wxSize(helpwd,helpht))
{
   SetFrameIcon(this);

   #ifdef __WXMSW__
      // avoid default background colour (dark grey)
      SetBackgroundColour(*wxLIGHT_GREY);
   #endif

   htmlwin = new HtmlView(this, wxID_ANY,
                          // specify small size to avoid clipping scroll bar on resize
                          wxDefaultPosition, wxSize(30,30),
                          wxHW_DEFAULT_STYLE | wxSUNKEN_BORDER);
   #ifdef __WXMAC__
      // prevent horizontal scroll bar appearing in Mac html window
      int xunit, yunit;
      htmlwin->GetScrollPixelsPerUnit(&xunit, &yunit);
      htmlwin->SetScrollRate(0, yunit);
   #endif
   htmlwin->SetBorders(4);

   wxBoxSizer *vbox = new wxBoxSizer(wxVERTICAL);
   wxBoxSizer *hbox = new wxBoxSizer(wxHORIZONTAL);

   backbutt = new wxButton(this, ID_BACK_BUTT, "<");
   hbox->Add(backbutt, 0, wxALL | wxALIGN_LEFT, 10);

   forwbutt = new wxButton(this, ID_FORWARD_BUTT, ">");
   hbox->Add(forwbutt, 0, wxTOP | wxBOTTOM | wxALIGN_LEFT, 10);

   contbutt = new wxButton(this, ID_CONTENTS_BUTT, "Contents");
   hbox->Add(contbutt, 0, wxALL | wxALIGN_LEFT, 10);

   hbox->AddStretchSpacer(1);

   wxButton *closebutt = new wxButton(this, wxID_CLOSE, "Close");
   closebutt->SetDefault();
   hbox->Add(closebutt, 0, wxALL | wxALIGN_RIGHT, 10);

   vbox->Add(hbox, 0, wxALL | wxEXPAND | wxALIGN_TOP, 0);

   vbox->Add(htmlwin, 1, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND | wxALIGN_TOP, 10);

   // allow for resize icon
   vbox->AddSpacer(10);

   SetMinSize(wxSize(minhelpwd, minhelpht));
   SetSizer(vbox);

   #ifdef __WXMAC__
      // expand sizer now to avoid seeing small htmlwin and buttons in top left corner
      vbox->SetDimension(0, 0, helpwd, helpht);
   #endif
}

void UpdateHelpButtons() {
   backbutt->Enable( htmlwin->HistoryCanBack() );
   forwbutt->Enable( htmlwin->HistoryCanForward() );
   contbutt->Enable( !htmlwin->GetOpenedPageTitle().Contains("Contents") );
   
   wxString location = htmlwin->GetOpenedPage();
   if ( !location.IsEmpty() ) {
      // set currhelp so user can close help window and then use 'h' to open same page
      strncpy(currhelp, location.c_str(), sizeof(currhelp));
   }
   
   #ifdef __WXMAC__
      // prevent horizontal scroll bar appearing in Mac html window
      int wd, ht, xpos, ypos;
      htmlwin->GetViewStart(&xpos, &ypos);
      htmlwin->GetSize(&wd, &ht);
      // resizing makes scroll bar go away
      htmlwin->SetSize(wd - 1, -1);
      htmlwin->SetSize(wd, -1);
      // resizing also resets pos to top so restore using ypos saved above
      if (ypos > 0) htmlwin->Scroll(-1, ypos);
   #endif
   htmlwin->SetFocus();          // for keyboard shortcuts in HtmlView::OnChar
}

void ShowHelp(const char *helpname) {
   // display given html file in help window
   if (helpptr) {
      // help window exists so bring it to front and display given file
      htmlwin->LoadPage(helpname);
      UpdateHelpButtons();
      helpptr->Raise();
      #ifdef __WXX11__
         helpptr->SetFocus();    // activate window
         htmlwin->SetFocus();    // for keyboard shortcuts in HtmlView::OnChar
      #endif
   } else {
      helpptr = new HelpFrame();
      if (helpptr == NULL) {
         Warning("Could not create help window!");
         return;
      }
      // assume our .html files contain a <title> tag
      htmlwin->SetRelatedFrame(helpptr, _("%s"));
      htmlwin->LoadPage(helpname);

      helpptr->Show(true);

      #ifdef __WXX11__
         // avoid wxX11 bug (probably caused by earlier SetMinSize call);
         // help window needs to be moved to helpx,helpy
         helpptr->Lower();
         // don't call Yield -- doesn't work if we're generating
         while (wxGetApp().Pending()) wxGetApp().Dispatch();
         helpptr->Move(helpx, helpy);
         // oh dear -- Move clobbers effect of SetMinSize!!!
         helpptr->Raise();
         helpptr->SetFocus();
         htmlwin->SetFocus();
      #endif
      
      UpdateHelpButtons();    // must be after Show to avoid hbar appearing on Mac
   }
}

void HelpFrame::OnBackButton(wxCommandEvent& WXUNUSED(event)) {
   if ( htmlwin->HistoryBack() ) {
      UpdateHelpButtons();
   } else {
      wxBell();
   }
}

void HelpFrame::OnForwardButton(wxCommandEvent& WXUNUSED(event)) {
   if ( htmlwin->HistoryForward() ) {
      UpdateHelpButtons();
   } else {
      wxBell();
   }
}

void HelpFrame::OnContentsButton(wxCommandEvent& WXUNUSED(event)) {
   ShowHelp("Help/index.html");
}

void HelpFrame::OnCloseButton(wxCommandEvent& WXUNUSED(event)) {
   Close(true);
}

void HelpFrame::OnClose(wxCloseEvent& WXUNUSED(event)) {
   // save current location and size for later use in SavePrefs
   wxRect r = helpptr->GetRect();
   helpx = r.x;
   helpy = r.y;
   helpwd = r.width;
   helpht = r.height;
   
   Destroy();        // also deletes all child windows (buttons, etc)
   helpptr = NULL;
}

void HtmlView::OnLinkClicked(const wxHtmlLinkInfo& link) {
   wxString url = link.GetHref();
   if ( url.StartsWith("http:") || url.StartsWith("mailto:") ) {
      // pass http/mailto URL to user's preferred browser/emailer
      #ifdef __WXMAC__
         // wxLaunchDefaultBrowser doesn't work on Mac with IE (get msg in console.log)
         // but it's easier just to use the Mac OS X open command
         if ( wxExecute("open " + url, wxEXEC_ASYNC) == -1 )
            Warning("Could not open URL!");
      #else
         if ( !wxLaunchDefaultBrowser(url) )
            Warning("Could not launch browser!");
      #endif
   } else {
      // assume it's a link to a local target or another help file
      LoadPage(url);
      if ( helpptr && helpptr->IsActive() ) {
         UpdateHelpButtons();
      }
   }
}

void HtmlView::OnChar(wxKeyEvent& event) {
   int key = event.GetKeyCode();
   if ( event.CmdDown() || event.AltDown() ) {
      if (key == 'c') {
         // copy any selected text to the clipboard
         wxString text = SelectionToText();
         if ( text.Length() > 0 ) {
            #ifdef __WXX11__
               CreateX11Clipboard( (char *)text.c_str(), text.Length() );
            #else
               if ( wxTheClipboard->Open() ) {
                  if ( !wxTheClipboard->SetData(new wxTextDataObject(text)) ) {
                     Warning("Could not copy selected text to clipboard!");
                  }
                  wxTheClipboard->Close();
               } else {
                  Warning("Could not open clipboard!");
               }
            #endif
         }
      } else {
         event.Skip();
      }
   } else {
      // this handler is also called from ShowAboutBox
      if ( helpptr == NULL || !helpptr->IsActive() ) {
         event.Skip();
         return;
      }
      if ( key == WXK_ESCAPE || key == WXK_RETURN ) {
         helpptr->Close(true);
      } else if ( key == WXK_HOME ) {
         ShowHelp("Help/index.html");
      } else if ( key == '[' ) {
         if ( HistoryBack() ) {
            UpdateHelpButtons();
         }
      } else if ( key == ']' ) {
         if ( HistoryForward() ) {
            UpdateHelpButtons();
         }      
      } else {
         event.Skip();
      }
   }
}

void ShowAboutBox() {
   wxDialog dlg(frameptr, wxID_ANY, wxString("About Golly"));
   
   HtmlView *html = new HtmlView(&dlg, wxID_ANY, wxDefaultPosition, wxSize(386, 220),
                                 wxHW_SCROLLBAR_NEVER | wxSUNKEN_BORDER);
   html->SetBorders(0);
   html->LoadPage("Help/about.html");
   html->SetSize(html->GetInternalRepresentation()->GetWidth(),
                 html->GetInternalRepresentation()->GetHeight());
   
   wxBoxSizer *topsizer = new wxBoxSizer(wxVERTICAL);
   topsizer->Add(html, 1, wxALL, 10);
   wxButton *okbutt = new wxButton(&dlg, wxID_OK, "OK");
   okbutt->SetDefault();
   topsizer->Add(okbutt, 0, wxBOTTOM | wxALIGN_CENTRE, 10);
   dlg.SetSizer(topsizer);
   topsizer->Fit(&dlg);
   dlg.CenterOnParent(wxBOTH);
   dlg.ShowModal();
   // all child windows have been deleted
}

// -----------------------------------------------------------------------------

void ProcessKey(int key) {
   showbanner = false;
   switch (key) {
      case WXK_LEFT:    PanLeft( SmallScroll(currview.getwidth()) ); break;
      case WXK_RIGHT:   PanRight( SmallScroll(currview.getwidth()) ); break;
      case WXK_UP:      PanUp( SmallScroll(currview.getheight()) ); break;
      case WXK_DOWN:    PanDown( SmallScroll(currview.getheight()) ); break;

      case '1':   SetPixelsPerCell(1); break;
      case '2':   SetPixelsPerCell(2); break;
      case '4':   SetPixelsPerCell(4); break;
      case '8':   SetPixelsPerCell(8); break;

      case 'a':
         SelectAll();
         break;

      case 'k':
         RemoveSelection();
         break;

      case 'v':
         PasteClipboard(false);
         break;

      case 'L':
         CyclePasteLocation();
         break;

      case 'M':
         CyclePasteMode();
         break;

      case 'c':
         CycleCursorMode();
         break;

      case 'f':
         FitPattern();
         break;

      case WXK_HOME:
      case 'm':
         ViewMiddle();
         break;

      case WXK_F1:            // F11 is also used on non-Mac platforms (handled by OnMenu)
         ToggleFullScreen();
         break;

      case 'i':
         ShowPatternInfo();
         break;

      case '[':
      case '/':
      case WXK_DIVIDE:        // for X11
         ZoomOut();
         break;

      case ']':
      case '*':
      case WXK_MULTIPLY:      // for X11
         ZoomIn();
         break;

      case ';':
         ToggleStatusBar();
         break;

      case '\'':
         ToggleToolBar();
         break;

      case 'l':
         ToggleGridLines();
         break;

      case 'b':
         ToggleVideo();
         break;

      case 'g':
         GeneratePattern();
         break;

      case ' ':
         NextGeneration(false);  // do only 1 gen
         break;

      case WXK_TAB:
         NextGeneration(true);   // use current increment
         break;

      case 't':
         ToggleAutoFit();
         break;

      case 'T':                  // 't' is for toggling autofit
         DisplayTimingInfo();
         break;

      case '+':
      case '=':
      case WXK_ADD:              // for X11
         GoFaster();
         break;

      case '-':
      case '_':
      case WXK_SUBTRACT:         // for X11
         GoSlower();
         break;
   
      case 'h':
      case WXK_HELP:
         if (waitingforclick) {
            // ignore key
         } else if (helpptr) {
            // help window is open so just bring it to the front
            helpptr->Raise();
            #ifdef __WXX11__
               helpptr->SetFocus();    // activate window
               htmlwin->SetFocus();    // for keyboard shortcuts in HtmlView::OnChar
            #endif
         } else {
            ShowHelp(currhelp);
         }
         break;

      default:
         // any other key turns off full screen mode
         if (fullscreen) ToggleFullScreen();
   }
}

// -----------------------------------------------------------------------------

// surprise, surprise -- drag and drop is not supported by wxX11

#if wxUSE_DRAG_AND_DROP

// derive a simple class for handling dropped files
class DnDFile : public wxFileDropTarget {
public:
   DnDFile() {}
   virtual bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames);
};

bool DnDFile::OnDropFiles(wxCoord, wxCoord, const wxArrayString& filenames) {
   if (generating) return false;

   // is there a wx call to bring app to front???
   #ifdef __WXMAC__
      ProcessSerialNumber process;
      if ( GetCurrentProcess(&process) == noErr )
         SetFrontProcess(&process);
   #endif
   #ifdef __WXMSW__
      SetForegroundWindow( (HWND)frameptr->GetHandle() );
   #endif
   frameptr->Raise();
   // need to process events to avoid crash if info window was in front
   while (wxGetApp().Pending()) wxGetApp().Dispatch();
   
   size_t numfiles = filenames.GetCount();
   for ( size_t n = 0; n < numfiles; n++ ) {
      SetCurrentFile(filenames[n]);
      LoadPattern(GetBaseName(filenames[n]));
   }

   #ifdef __WXMAC__
      // need to call Refresh a bit later to remove colored frame on Mac
      onetimer->Start(10, wxTIMER_ONE_SHOT);
      // OnOneTimer will be called once after a delay of 0.01 sec
   #endif
   
   return true;
}

#endif // wxUSE_DRAG_AND_DROP

// -----------------------------------------------------------------------------

// event table and handlers for main window

BEGIN_EVENT_TABLE(MainFrame, wxFrame)
   EVT_SET_FOCUS  (              MainFrame::OnSetFocus)
   EVT_MENU       (wxID_ANY,     MainFrame::OnMenu)
   EVT_TIMER      (ID_ONE_TIMER, MainFrame::OnOneTimer)
   EVT_ACTIVATE   (              MainFrame::OnActivate)
   EVT_SIZE       (              MainFrame::OnSize)
   EVT_CLOSE      (              MainFrame::OnClose)
END_EVENT_TABLE()

void MainFrame::OnActivate(wxActivateEvent& event) {
   // this is never called on X11!!!
   // note that frameptr->IsActive() doesn't always match event.GetActive()
   UpdateUserInterface(event.GetActive());
   if ( !event.GetActive() ) {
      wxSetCursor(*wxSTANDARD_CURSOR);
   }
   event.Skip();
}

void MainFrame::OnSetFocus(wxFocusEvent& WXUNUSED(event)) {
   #ifdef __WXMSW__
      // fix wxMSW bug: don't let main window get focus after being minimized
      if (viewptr) viewptr->SetFocus();
   #endif
   #ifdef __WXX11__
      // make sure viewport keeps keyboard focus whenever main window is active
      if (viewptr && frameptr->IsActive()) viewptr->SetFocus();
      // fix problems after modal dialog or help window is closed
      UpdateUserInterface(frameptr->IsActive());
   #endif
}

void MainFrame::OnMenu(wxCommandEvent& event) {
   showbanner = false;
   ClearMessage();
   switch (event.GetId())
   {
      // File menu
      case wxID_NEW:          NewPattern(); break;
      case wxID_OPEN:         OpenPattern(); break;
      case ID_OPENCLIP:       OpenClipboard(); break;
      case wxID_SAVE:         SavePattern(); break;
      case wxID_EXIT:         Close(true); break;        // true forces frame to close
      // Edit menu
      case ID_CUT:            CutSelection(); break;
      case ID_COPY:           CopySelection(); break;
      case ID_CLEAR:          ClearSelection(); break;
      case ID_PASTE:          PasteClipboard(false); break;
      case ID_PASTE_SEL:      PasteClipboard(true); break;
      case ID_PL_TL:          SetPasteLocation(TopLeft); break;
      case ID_PL_TR:          SetPasteLocation(TopRight); break;
      case ID_PL_BR:          SetPasteLocation(BottomRight); break;
      case ID_PL_BL:          SetPasteLocation(BottomLeft); break;
      case ID_PL_MID:         SetPasteLocation(Middle); break;
      case ID_PM_COPY:        SetPasteMode(Copy); break;
      case ID_PM_OR:          SetPasteMode(Or); break;
      case ID_PM_XOR:         SetPasteMode(Xor); break;
      case ID_SELALL:         SelectAll(); break;
      case ID_REMOVE:         RemoveSelection(); break;
      case ID_DRAW:           SetCursorMode(curs_pencil); break;
      case ID_SELECT:         SetCursorMode(curs_cross); break;
      case ID_MOVE:           SetCursorMode(curs_hand); break;
      case ID_ZOOMIN:         SetCursorMode(curs_zoomin); break;
      case ID_ZOOMOUT:        SetCursorMode(curs_zoomout); break;
      // Control menu
      case ID_GO:             GeneratePattern(); break;
      case ID_STOP:           StopGenerating(); break;
      case ID_NEXT:           NextGeneration(false); break;
      case ID_STEP:           NextGeneration(true); break;
      case ID_RESET:          ResetPattern(); break;
      case ID_FASTER:         GoFaster(); break;
      case ID_SLOWER:         GoSlower(); break;
      case ID_AUTO:           ToggleAutoFit(); break;
      case ID_HASH:           ToggleHashing(); break;
      case ID_HYPER:          ToggleHyperspeed(); break;
      case ID_MAXMEM:         ChangeMaxMemory(); break;
      case ID_RULE:           ChangeRule(); break;
      // View menu
      case ID_FIT:            FitPattern(); break;
      case ID_MIDDLE:         ViewMiddle(); break;
      case ID_FULL:           ToggleFullScreen(); break;
      case wxID_ZOOM_IN:      ZoomIn(); break;
      case wxID_ZOOM_OUT:     ZoomOut(); break;
      case ID_STATUS:         ToggleStatusBar(); break;
      case ID_TOOL:           ToggleToolBar(); break;
      case ID_GRID:           ToggleGridLines(); break;
      case ID_VIDEO:          ToggleVideo(); break;
      case ID_BUFF:           ToggleBuffering(); break;
      case ID_INFO:           ShowPatternInfo(); break;
      // Help menu
      case ID_HELP_INDEX:     ShowHelp("Help/index.html"); break;
      case ID_HELP_INTRO:     ShowHelp("Help/intro.html"); break;
      case ID_HELP_TIPS:      ShowHelp("Help/tips.html"); break;
      case ID_HELP_SHORTCUTS: ShowHelp("Help/shortcuts.html"); break;
      case ID_HELP_FILE:      ShowHelp("Help/file.html"); break;
      case ID_HELP_EDIT:      ShowHelp("Help/edit.html"); break;
      case ID_HELP_CONTROL:   ShowHelp("Help/control.html"); break;
      case ID_HELP_VIEW:      ShowHelp("Help/view.html"); break;
      case ID_HELP_HELP:      ShowHelp("Help/help.html"); break;
      case ID_HELP_REFS:      ShowHelp("Help/refs.html"); break;
      case ID_HELP_PROBLEMS:  ShowHelp("Help/problems.html"); break;
      case ID_HELP_CHANGES:   ShowHelp("Help/changes.html"); break;
      case ID_HELP_CREDITS:   ShowHelp("Help/credits.html"); break;
      case wxID_ABOUT:        ShowAboutBox(); break;
   }
   UpdateUserInterface(frameptr->IsActive());
}

#ifdef __WXX11__
void MainFrame::OnSize(wxSizeEvent& event)
#else
void MainFrame::OnSize(wxSizeEvent& WXUNUSED(event))
#endif
{
   int wd, ht;
   GetClientSize(&wd, &ht);      // includes status bar and viewport
   if (wd > 0 && ht > 0) {
      // note that statusptr and viewptr might be NULL if OnSize gets called
      // from MainFrame::MainFrame (true if X11)
      if (statusptr && statusht > 0) {
         // adjust size of status bar
         statusptr->SetSize(0, 0, wd, statusht);
      }
      if (viewptr && ht > statusht) {
         // adjust size of viewport
         viewptr->SetSize(0, statusht, wd, ht - statusht);
         SetViewSize();
      }
   }
   #ifdef __WXX11__
      // need to do default processing for X11 menu bar and tool bar
      event.Skip();
   #endif
}

void MainFrame::OnOneTimer(wxTimerEvent& WXUNUSED(event)) {
   // fix drag and drop problem on Mac -- see DnDFile::OnDropFiles
   #ifdef __WXMAC__
      // remove colored frame
      if (viewptr) viewptr->Refresh(false, NULL);
   #endif
}

void MainFrame::OnClose(wxCloseEvent& WXUNUSED(event)) {
   if (helpptr) helpptr->Close(true);
   if (infoptr) infoptr->Close(true);
   FinishApp();
   #ifdef __WXX11__
      // avoid seg fault on X11
      if (generating) exit(0);
   #endif
   if (generating) StopGenerating();
   Destroy();
}

// -----------------------------------------------------------------------------

// event table and handlers for status bar window

BEGIN_EVENT_TABLE(StatusBar, wxWindow)
   EVT_PAINT            (StatusBar::OnPaint)
   EVT_LEFT_DOWN        (StatusBar::OnMouseDown)
   EVT_LEFT_DCLICK      (StatusBar::OnMouseDown)
   EVT_ERASE_BACKGROUND (StatusBar::OnEraseBackground)
END_EVENT_TABLE()

void StatusBar::OnPaint(wxPaintEvent& WXUNUSED(event)) {
   #ifdef __WXMAC__
      // windows on Mac OS X are automatically buffered
      wxPaintDC dc(this);
   #else
      // use wxWidgets buffering to avoid flicker
      int wd, ht;
      GetClientSize(&wd, &ht);
      // wd or ht might be < 1 on Win/X11 platforms
      if (wd < 1) wd = 1;
      if (ht < 1) ht = 1;
      if (wd != statbitmapwd || ht != statbitmapht) {
         // need to create a new bitmap for status bar
         if (statbitmap) delete statbitmap;
         statbitmap = new wxBitmap(wd, ht);
         statbitmapwd = wd;
         statbitmapht = ht;
      }
      if (statbitmap == NULL) Fatal("Not enough memory to render status bar!");
      wxBufferedPaintDC dc(this, *statbitmap);
   #endif

   wxRect updaterect = GetUpdateRegion().GetBox();
   dc.BeginDrawing();
   DrawStatusBar(dc, updaterect);
   dc.EndDrawing();
}

bool ClickInScaleBox(int x, int y) {
   return x >= h_scale && x <= h_step - 20 && y <= statusht/2;
}

bool ClickInStepBox(int x, int y) {
   return x >= h_step && x <= h_xy - 20 && y <= statusht/2;
}

void StatusBar::OnMouseDown(wxMouseEvent& event) {
   ClearMessage();
   if ( ClickInScaleBox(event.GetX(), event.GetY()) ) {
      if (currview.getmag() != 0) {
         // reset scale to 1:1
         SetPixelsPerCell(1);
      }
   } else if ( ClickInStepBox(event.GetX(), event.GetY()) ) {
      if (warp != 0) {
         // reset step to 1 gen
         warp = 0;
         SetGenIncrement();
         // only update status bar
         RefreshStatus();
      }
   }
   #ifdef __WXX11__
      // make sure viewport keeps keyboard focus
      viewptr->SetFocus();
   #endif
}

void StatusBar::OnEraseBackground(wxEraseEvent& WXUNUSED(event)) {
   // do nothing because we'll be painting the entire status bar
}

// -----------------------------------------------------------------------------

// event table and handlers for viewport window

BEGIN_EVENT_TABLE(PatternView, wxWindow)
   EVT_PAINT            (                 PatternView::OnPaint)
   EVT_KEY_DOWN         (                 PatternView::OnKeyDown)
   EVT_KEY_UP           (                 PatternView::OnKeyUp)
   EVT_CHAR             (                 PatternView::OnChar)
   EVT_LEFT_DOWN        (                 PatternView::OnMouseDown)
   EVT_LEFT_DCLICK      (                 PatternView::OnMouseDown)
   EVT_LEFT_UP          (                 PatternView::OnMouseUp)
   EVT_MOTION           (                 PatternView::OnMouseMotion)
   EVT_ENTER_WINDOW     (                 PatternView::OnMouseEnter)
   EVT_LEAVE_WINDOW     (                 PatternView::OnMouseExit)
   EVT_TIMER            (ID_DRAG_TIMER,   PatternView::OnDragTimer)
   EVT_SCROLLWIN        (                 PatternView::OnScroll)
   EVT_ERASE_BACKGROUND (                 PatternView::OnEraseBackground)
END_EVENT_TABLE()

#ifndef __WXMAC__
wxBitmap *viewbitmap;      // viewport bitmap for OnPaint
int viewbitmapwd = -1;     // width of viewport bitmap
int viewbitmapht = -1;     // height of viewport bitmap
#endif

void PatternView::OnPaint(wxPaintEvent& WXUNUSED(event)) {
   int wd, ht;
   GetClientSize(&wd, &ht);
   // wd or ht might be < 1 on Win/X11 platforms
   if (wd < 1) wd = 1;
   if (ht < 1) ht = 1;
   
   if ( wd != currview.getwidth() || ht != currview.getheight() ) {
      // need to change viewport size;
      // can happen on Windows when resizing/maximizing
      SetViewSize();
   }
   
   bool seeselection = SelectionVisible(NULL);
   // wxX11's Blit doesn't support alpha channel
   #ifndef __WXX11__
      if ( seeselection && ((wd != selbitmapwd || ht != selbitmapht)) ) {
         // rescale selection image and create new bitmap
         selimage.Rescale(wd, ht);
         if (selbitmap) delete selbitmap;
         selbitmap = new wxBitmap(selimage);
         selbitmapwd = wd;
         selbitmapht = ht;
      }
   #endif

   #ifdef __WXMAC__
      // windows on Mac OS X are automatically buffered
      wxPaintDC dc(this);
      dc.BeginDrawing();
      currdc = &dc;
      DisplayPattern();
      dc.EndDrawing();
   #else
      if ( buffered || seeselection || waitingforclick || GridVisible() ) {
         // use wxWidgets buffering to avoid flicker
         if (wd != viewbitmapwd || ht != viewbitmapht) {
            // need to create a new bitmap for viewport
            if (viewbitmap) delete viewbitmap;
            viewbitmap = new wxBitmap(wd, ht);
            viewbitmapwd = wd;
            viewbitmapht = ht;
         }
         if (viewbitmap == NULL) Fatal("Not enough memory to do buffering!");
         wxBufferedPaintDC dc(this, *viewbitmap);
         dc.BeginDrawing();
         currdc = &dc;
         DisplayPattern();
         dc.EndDrawing();
      } else {
         wxPaintDC dc(this);
         dc.BeginDrawing();
         currdc = &dc;
         DisplayPattern();
         dc.EndDrawing();
      }
   #endif
}

void PatternView::OnKeyDown(wxKeyEvent& event) {
   int key = event.GetKeyCode();
   if (key == WXK_SHIFT) {
      // pressing shift key temporarily toggles zoom in/out cursor;
      // some platforms (eg. WinXP) send multiple key-down events while
      // a key is pressed so we must be careful to toggle only once
      if (currcurs == curs_zoomin && oldzoom == NULL) {
         oldzoom = curs_zoomin;
         SetCursorMode(curs_zoomout);
         UpdateUserInterface(frameptr->IsActive());
      } else if (currcurs == curs_zoomout && oldzoom == NULL) {
         oldzoom = curs_zoomout;
         SetCursorMode(curs_zoomin);
         UpdateUserInterface(frameptr->IsActive());
      }
   }
   event.Skip();
}

void PatternView::OnKeyUp(wxKeyEvent& event) {
   int key = event.GetKeyCode();
   if (key == WXK_SHIFT) {
      // releasing shift key sets zoom in/out cursor back to original state
      if (oldzoom) {
         SetCursorMode(oldzoom);
         oldzoom = NULL;
         UpdateUserInterface(frameptr->IsActive());
      }
   }
   event.Skip();
}

// handle translated keyboard events
void PatternView::OnChar(wxKeyEvent& event) {
   int key = event.GetKeyCode();
   ClearMessage();
   if ( generating && (key == WXK_ESCAPE || key == '.') ) {
      StopGenerating();
      return;
   }
   if ( waitingforclick && key == WXK_ESCAPE ) {
      // cancel paste
      pastex = -1;
      pastey = -1;
      waitingforclick = false;
      return;
   }
   if ( event.CmdDown() || event.AltDown() ) {
      event.Skip();
   } else {
      ProcessKey(key);
      UpdateUserInterface(frameptr->IsActive());
   }
}

void PatternView::OnMouseDown(wxMouseEvent& event) {
   if (waitingforclick) {
      // set paste location
      pastex = event.GetX();
      pastey = event.GetY();
      waitingforclick = false;
   } else {
      ClearMessage();
      ProcessClick(event.GetX(), event.GetY(), event.ShiftDown());
      UpdateUserInterface(frameptr->IsActive());
   }
}

void PatternView::OnMouseUp(wxMouseEvent& WXUNUSED(event)) {
   // Mac bug: we don't get this event if button released outside window,
   // even if CaptureMouse has been called!!! soln: use Button() in OnDragTimer
   if (drawingcells || selectingcells || movingview) {
      StopDraggingMouse();
   }
}

void PatternView::OnMouseMotion(wxMouseEvent& WXUNUSED(event)) {
   CheckMouseLocation(frameptr->IsActive());
}

void PatternView::OnMouseEnter(wxMouseEvent& WXUNUSED(event)) {
   // Win bug??? we don't get this event if CaptureMouse has been called
   CheckCursor(frameptr->IsActive());
   // no need to call CheckMouseLocation here (OnMouseMotion will be called)
}

void PatternView::OnMouseExit(wxMouseEvent& WXUNUSED(event)) {
   // Win bug??? we don't get this event if CaptureMouse has been called
   CheckCursor(frameptr->IsActive());
   CheckMouseLocation(frameptr->IsActive());
   #ifdef __WXX11__
      // make sure viewport keeps keyboard focus
      if ( frameptr->IsActive() ) {
         viewptr->SetFocus();
      }
   #endif
}

void PatternView::OnDragTimer(wxTimerEvent& WXUNUSED(event)) {
   // called periodically while drawing/selecting/moving
   #ifdef __WXMAC__
      // need to check if button no longer down due to CaptureMouse bug in wxMac!!!
      if ( !Button() ) {
         StopDraggingMouse();
         return;
      }
   #endif

   wxPoint pt = ScreenToClient( wxGetMousePosition() );
   int x = pt.x;
   int y = pt.y;
   // don't test "!PointInView(x, y)" here -- we want to allow scrolling
   // in full screen mode when mouse is at outer edge of view
   if ( x <= 0 || x >= currview.getxmax() ||
        y <= 0 || y >= currview.getymax() ) {
      // scroll view
      int xamount = 0;
      int yamount = 0;
      if (x <= 0) xamount = -SmallScroll(currview.getwidth());
      if (y <= 0) yamount = -SmallScroll(currview.getheight());
      if (x >= currview.getxmax()) xamount = SmallScroll(currview.getwidth());
      if (y >= currview.getymax()) yamount = SmallScroll(currview.getheight());

      if ( drawingcells ) {
         currview.move(xamount, yamount);
         RefreshPatternAndStatus();

      } else if ( selectingcells ) {
         currview.move(xamount, yamount);
         // no need to call RefreshPatternAndStatus() here because
         // it will be called soon in SelectCells, except in this case:
         if (forceh || forcev) {
            // selection might not change so must update pattern
            viewptr->Refresh(false, NULL);
         }

      } else if ( movingview ) {
         // scroll in opposite direction, and if both amounts are non-zero then
         // set both to same (larger) absolute value so user can scroll at 45 degrees
         if ( xamount != 0 && yamount != 0 ) {
            if ( abs(xamount) > abs(yamount) ) {
               yamount = yamount < 0 ? -abs(xamount) : abs(xamount);
            } else {
               xamount = xamount < 0 ? -abs(yamount) : abs(yamount);
            }
         }
         currview.move(-xamount, -yamount);
         RefreshPatternAndStatus();
         // adjust x,y and bigcellx,bigcelly for MoveView call below
         x += xamount;
         y += yamount;
         pair<bigint, bigint> cellpos = currview.at(x, y);
         bigcellx = cellpos.first;
         bigcelly = cellpos.second;
      }
   }

   if ( drawingcells ) {
      // only draw cells within view
      if (x < 0) x = 0;
      if (y < 0) y = 0;
      if (x > currview.getxmax()) x = currview.getxmax();
      if (y > currview.getymax()) y = currview.getymax();
      DrawCells(x, y);

   } else if ( selectingcells ) {
      // only select cells within view
      if (x < 0) x = 0;
      if (y < 0) y = 0;
      if (x > currview.getxmax()) x = currview.getxmax();
      if (y > currview.getymax()) y = currview.getymax();
      SelectCells(x, y);

   } else if ( movingview ) {
      MoveView(x, y);
   }
}

void PatternView::OnScroll(wxScrollWinEvent& event) {
   WXTYPE type = (WXTYPE)event.GetEventType();
   int orient = event.GetOrientation();

   if (type == wxEVT_SCROLLWIN_LINEUP)
   {
      if (orient == wxHORIZONTAL)
         PanLeft( SmallScroll(currview.getwidth()) );
      else
         PanUp( SmallScroll(currview.getheight()) );
   }
   else if (type == wxEVT_SCROLLWIN_LINEDOWN)
   {
      if (orient == wxHORIZONTAL)
         PanRight( SmallScroll(currview.getwidth()) );
      else
         PanDown( SmallScroll(currview.getheight()) );
   }
   else if (type == wxEVT_SCROLLWIN_PAGEUP)
   {
      if (orient == wxHORIZONTAL)
         PanLeft( BigScroll(currview.getwidth()) );
      else
         PanUp( BigScroll(currview.getheight()) );
   }
   else if (type == wxEVT_SCROLLWIN_PAGEDOWN)
   {
      if (orient == wxHORIZONTAL)
         PanRight( BigScroll(currview.getwidth()) );
      else
         PanDown( BigScroll(currview.getheight()) );
   }
   else if (type == wxEVT_SCROLLWIN_THUMBTRACK)
   {
      int newpos = event.GetPosition();
      int amount = newpos - (orient == wxHORIZONTAL ? hthumb : vthumb);
      if (amount != 0) {
         TestAutoFit();
         if (currview.getmag() > 0) {
            // amount is in cells so convert to pixels
            amount = amount << currview.getmag();
         }
         if (orient == wxHORIZONTAL) {
            hthumb = newpos;
            currview.move(amount, 0);
            // don't call RefreshWindow here because it calls UpdateScrollBars
            viewptr->Refresh(false, NULL);
            // don't update immediately (more responsive, especially on X11)
            // viewptr->Update();
         } else {
            vthumb = newpos;
            currview.move(0, amount);
            // don't call RefreshWindow here because it calls UpdateScrollBars
            viewptr->Refresh(false, NULL);
            // don't update immediately (more responsive, especially on X11)
            // viewptr->Update();
         }
      }
      #ifdef __WXX11__
         // need to change the thumb position manually
         SetScrollPos(orient, newpos, true);
      #endif
   }
   else if (type == wxEVT_SCROLLWIN_THUMBRELEASE)
   {
      // now we can call UpdateScrollBars
      RefreshWindow();
   }
}

void PatternView::OnEraseBackground(wxEraseEvent& WXUNUSED(event)) {
   // do nothing because we'll be painting the entire viewport
}

// -----------------------------------------------------------------------------

// create the status bar window
StatusBar::StatusBar(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht)
   : wxWindow(parent, wxID_ANY, wxPoint(xorg,yorg), wxSize(wd,ht),
              wxNO_BORDER | wxFULL_REPAINT_ON_RESIZE
             )
{
   // avoid erasing background (only on GTK+)
   SetBackgroundStyle(wxBG_STYLE_CUSTOM);

   // create font for text in status bar and set textascent for use in DisplayText
   #ifdef __WXMSW__
      // use smaller, narrower font on Windows
      statusfont = wxFont::New(8, wxDEFAULT, wxNORMAL, wxNORMAL);
      
      int major, minor;
      wxGetOsVersion(&major, &minor);
      if ( major > 5 || (major == 5 && minor >= 1) ) {
         // 5.1+ means XP or later
         textascent = 12;
      } else {
         textascent = 10;
      }
   #else
      statusfont = wxFont::New(10, wxMODERN, wxNORMAL, wxNORMAL);
      textascent = 10;
   #endif
   if (statusfont == NULL) Fatal("Failed to create status bar font!");
   
   // determine horizontal offsets for info in status bar
   wxClientDC dc(this);
   dc.BeginDrawing();
   int textwd, textht;
   int mingap = 10;
   SetStatusFont(dc);
   h_gen = 6;
   dc.GetTextExtent("Generation=9.999999e+999", &textwd, &textht);
   h_pop = h_gen + textwd + mingap;
   dc.GetTextExtent("Population=9.999999e+999", &textwd, &textht);
   h_scale = h_pop + textwd + mingap;
   dc.GetTextExtent("Scale=2^9999:1", &textwd, &textht);
   h_step = h_scale + textwd + mingap;
   dc.GetTextExtent("Step=10^9999", &textwd, &textht);
   h_xy = h_step + textwd + mingap;
   dc.EndDrawing();
}

// destroy the status bar window
StatusBar::~StatusBar()
{
   if (statusfont) delete statusfont;
}

// -----------------------------------------------------------------------------

// create the viewport window
PatternView::PatternView(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht)
   : wxWindow(parent, wxID_ANY, wxPoint(xorg,yorg), wxSize(wd,ht),
               #ifdef __WXMSW__
                  // nicer because Windows tool bar has no border
                  wxSIMPLE_BORDER |
               #else
                  wxNO_BORDER |
               #endif
                  wxFULL_REPAINT_ON_RESIZE |
                  wxVSCROLL | wxHSCROLL |
                  wxWANTS_CHARS              // receive all keyboard events
             )
{
   // avoid erasing background (only on GTK+)
   SetBackgroundStyle(wxBG_STYLE_CUSTOM);

   dragtimer = new wxTimer(this, ID_DRAG_TIMER);
}

// destroy the viewport window
PatternView::~PatternView()
{
   delete dragtimer;
}

// -----------------------------------------------------------------------------

void CreatePens() {
   // create some colored pens for use by SetPen
   pen_ltgray = new wxPen(ltgray);
   pen_dkgray = new wxPen(dkgray);
   pen_verydark = new wxPen(verydark);
   pen_notsodark = new wxPen(notsodark);
}

void CreateBrushes() {
   // create some colored brushes for FillRect calls
   brush_yellow = new wxBrush(paleyellow);
   brush_blue = new wxBrush(paleblue);
   brush_dkgray = new wxBrush(dkgray);
}

void CreateCursors() {
   curs_pencil = new wxCursor(wxCURSOR_PENCIL);
   if (curs_pencil == NULL) Fatal("Failed to create pencil cursor!");

   curs_cross = new wxCursor(wxCURSOR_CROSS);
   if (curs_cross == NULL) Fatal("Failed to create cross cursor!");

   curs_hand = new wxCursor(wxCURSOR_HAND);
   if (curs_hand == NULL) Fatal("Failed to create hand cursor!");

   #ifdef __WXX11__
      // wxX11 doesn't support creating cursor from wxImage or from bits!!!
      // don't use plus sign -- confusing with crosshair, and no minus sign for zoom out
      // curs_zoomin = new wxCursor(wxCURSOR_MAGNIFIER);
      curs_zoomin = new wxCursor(wxCURSOR_POINT_RIGHT);
   #else
      wxBitmap bitmap_zoomin = wxBITMAP(zoomin_curs);
      wxImage image_zoomin = bitmap_zoomin.ConvertToImage();
      image_zoomin.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_X, 6);
      image_zoomin.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_Y, 6);
      curs_zoomin = new wxCursor(image_zoomin);
   #endif
   if (curs_zoomin == NULL) Fatal("Failed to create zoomin cursor!");

   #ifdef __WXX11__
      // wxX11 doesn't support creating cursor from wxImage or bits!!!
      curs_zoomout = new wxCursor(wxCURSOR_POINT_LEFT);
   #else
      wxBitmap bitmap_zoomout = wxBITMAP(zoomout_curs);
      wxImage image_zoomout = bitmap_zoomout.ConvertToImage();
      image_zoomout.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_X, 6);
      image_zoomout.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_Y, 6);
      curs_zoomout = new wxCursor(image_zoomout);
   #endif
   if (curs_zoomout == NULL) Fatal("Failed to create zoomout cursor!");

   /* no longer use busy cursor when generating
   #ifdef __WXMSW__
      curs_busy = new wxCursor(wxCURSOR_ARROWWAIT);
      if (curs_busy == NULL) Fatal("Failed to create busy cursor!");
   #else
      curs_busy = wxHOURGLASS_CURSOR;
   #endif
   */
   
   // set currcurs in case we decide not to in NewPattern/LoadPattern
   currcurs = curs_pencil;
}

// create the main window
MainFrame::MainFrame()
   : wxFrame(NULL, wxID_ANY, _(""), wxPoint(mainx,mainy), wxSize(mainwd,mainht))
{
   SetFrameIcon(this);

   // create one-shot timer
   onetimer = new wxTimer(this, ID_ONE_TIMER);

   // create the menus
   wxMenu *fileMenu = new wxMenu();
   wxMenu *editMenu = new wxMenu();
   wxMenu *controlMenu = new wxMenu();
   wxMenu *viewMenu = new wxMenu();
   wxMenu *helpMenu = new wxMenu();

   // create submenus
   wxMenu *plocSubMenu = new wxMenu();
   wxMenu *pmodeSubMenu = new wxMenu();
   wxMenu *cmodeSubMenu = new wxMenu();

   plocSubMenu->AppendCheckItem(ID_PL_TL, _("Top Left"));
   plocSubMenu->AppendCheckItem(ID_PL_TR, _("Top Right"));
   plocSubMenu->AppendCheckItem(ID_PL_BR, _("Bottom Right"));
   plocSubMenu->AppendCheckItem(ID_PL_BL, _("Bottom Left"));
   plocSubMenu->AppendCheckItem(ID_PL_MID, _("Middle"));

   pmodeSubMenu->AppendCheckItem(ID_PM_COPY, _("Copy"));
   pmodeSubMenu->AppendCheckItem(ID_PM_OR, _("Or"));
   pmodeSubMenu->AppendCheckItem(ID_PM_XOR, _("Xor"));

   cmodeSubMenu->AppendCheckItem(ID_DRAW, _("Draw"));
   cmodeSubMenu->AppendCheckItem(ID_SELECT, _("Select"));
   cmodeSubMenu->AppendCheckItem(ID_MOVE, _("Move"));
   cmodeSubMenu->AppendCheckItem(ID_ZOOMIN, _("Zoom In"));
   cmodeSubMenu->AppendCheckItem(ID_ZOOMOUT, _("Zoom Out"));

   fileMenu->Append(wxID_NEW, _("New Pattern\tCtrl+N"));
   fileMenu->AppendSeparator();
   fileMenu->Append(wxID_OPEN, _("Open Pattern...\tCtrl+O"));
   fileMenu->Append(ID_OPENCLIP, _("Open Clipboard\tShift+Ctrl+O"));
   fileMenu->AppendSeparator();
   fileMenu->Append(wxID_SAVE, _("Save Pattern...\tCtrl+S"));
   fileMenu->AppendSeparator();
   // on the Mac the Alt+X gets converted to Cmd-Q
   fileMenu->Append(wxID_EXIT, wxGetStockLabel(wxID_EXIT,true,_T("Alt+X")));

   editMenu->Append(ID_CUT, _("Cut\tCtrl+X"));
   editMenu->Append(ID_COPY, _("Copy\tCtrl+C"));
   editMenu->Append(ID_CLEAR, _("Clear"));
   editMenu->AppendSeparator();
   editMenu->Append(ID_PASTE, _("Paste\tCtrl+V"));
   editMenu->Append(ID_PMODE, _("Paste Mode"), pmodeSubMenu);
   editMenu->Append(ID_PLOCATION, _("Paste Location"), plocSubMenu);
   editMenu->Append(ID_PASTE_SEL, _("Paste to Selection"));
   editMenu->AppendSeparator();
   editMenu->Append(ID_SELALL, _("Select All\tCtrl+A"));
   editMenu->Append(ID_REMOVE, _("Remove Selection\tCtrl+K"));
   editMenu->AppendSeparator();
   editMenu->Append(ID_CMODE, _("Cursor Mode"), cmodeSubMenu);

   controlMenu->Append(ID_GO, _("Go\tCtrl+G"));
   #ifdef __WXMSW__
      // Windows doesn't support Ctrl+<non-alpha> menu shortcuts
      controlMenu->Append(ID_STOP, _("Stop\t."));
   #else
      controlMenu->Append(ID_STOP, _("Stop\tCtrl+."));
   #endif
   // why no space symbol/word after Next item on wxMac???!!!
   controlMenu->Append(ID_NEXT, _("Next\tSpace"));
   controlMenu->Append(ID_STEP, _("Next Step\tTab"));
   controlMenu->Append(ID_RESET, _("Reset\tCtrl+R"));
   controlMenu->AppendSeparator();
   #ifdef __WXMSW__
      // Windows doesn't support Ctrl+<non-alpha> menu shortcuts
      controlMenu->Append(ID_FASTER, _("Faster\t+"));
      controlMenu->Append(ID_SLOWER, _("Slower\t-"));
   #else
      controlMenu->Append(ID_FASTER, _("Faster\tCtrl++"));
      controlMenu->Append(ID_SLOWER, _("Slower\tCtrl+-"));
   #endif
   controlMenu->AppendSeparator();
   controlMenu->AppendCheckItem(ID_AUTO, _("Auto Fit\tCtrl+T"));
   controlMenu->AppendCheckItem(ID_HASH, _("Use Hashing\tCtrl+U"));
   controlMenu->AppendCheckItem(ID_HYPER, _("Hyperspeed"));
   controlMenu->Append(ID_MAXMEM, _("Max Hash Memory..."));
   controlMenu->AppendSeparator();
   controlMenu->Append(ID_RULE, _("Rule..."));

   viewMenu->Append(ID_FIT, _("Fit Pattern\tCtrl+F"));
   viewMenu->Append(ID_MIDDLE, _("Middle\tCtrl+M"));
   #ifdef __WXMAC__
      // F11 is a default activation key for Expose so use F1 instead
      viewMenu->Append(ID_FULL, _("Full Screen\tF1"));
   #else
      viewMenu->Append(ID_FULL, _("Full Screen\tF11"));
   #endif
   viewMenu->AppendSeparator();
   #ifdef __WXMSW__
      // Windows doesn't support Ctrl+<non-alpha> menu shortcuts
      viewMenu->Append(wxID_ZOOM_IN, _("Zoom In\t]"));
      viewMenu->Append(wxID_ZOOM_OUT, _("Zoom Out\t["));
      viewMenu->AppendSeparator();
      viewMenu->AppendCheckItem(ID_STATUS, _("Show Status Bar\t;"));
      viewMenu->AppendCheckItem(ID_TOOL, _("Show Tool Bar\t'"));
   #else
      viewMenu->Append(wxID_ZOOM_IN, _("Zoom In\tCtrl+]"));
      viewMenu->Append(wxID_ZOOM_OUT, _("Zoom Out\tCtrl+["));
      viewMenu->AppendSeparator();
      viewMenu->AppendCheckItem(ID_STATUS, _("Show Status Bar\tCtrl+;"));
      viewMenu->AppendCheckItem(ID_TOOL, _("Show Tool Bar\tCtrl+'"));
   #endif
   viewMenu->AppendCheckItem(ID_GRID, _("Show Grid Lines\tCtrl+L"));
   viewMenu->AppendCheckItem(ID_VIDEO, _("Black on White\tCtrl+B"));
   viewMenu->AppendCheckItem(ID_BUFF, _("Buffered"));
   viewMenu->AppendSeparator();
   viewMenu->Append(ID_INFO, _("Pattern Info\tCtrl+I"));

   helpMenu->Append(ID_HELP_INDEX, _("Contents"));
   helpMenu->Append(ID_HELP_INTRO, _("Introduction"));
   helpMenu->Append(ID_HELP_TIPS, _("Hints and Tips"));
   helpMenu->Append(ID_HELP_SHORTCUTS, _("Shortcuts"));
   helpMenu->AppendSeparator();
   helpMenu->Append(ID_HELP_FILE, _("File Menu"));
   helpMenu->Append(ID_HELP_EDIT, _("Edit Menu"));
   helpMenu->Append(ID_HELP_CONTROL, _("Control Menu"));
   helpMenu->Append(ID_HELP_VIEW, _("View Menu"));
   helpMenu->Append(ID_HELP_HELP, _("Help Menu"));
   helpMenu->AppendSeparator();
   helpMenu->Append(ID_HELP_REFS, _("References"));
   helpMenu->Append(ID_HELP_PROBLEMS, _("Known Problems"));
   helpMenu->Append(ID_HELP_CHANGES, _("Changes"));
   helpMenu->Append(ID_HELP_CREDITS, _("Credits"));
   #ifndef __WXMAC__
      helpMenu->AppendSeparator();
   #endif
   // the About item will be in the app menu on Mac or Help menu on other platforms
   helpMenu->Append(wxID_ABOUT, _("&About Golly"));

   // create the menu bar and append menus
   wxMenuBar *menuBar = new wxMenuBar();
   if (menuBar == NULL) Fatal("Failed to create menu bar!");
   menuBar->Append(fileMenu, _("&File"));
   menuBar->Append(editMenu, _("&Edit"));
   menuBar->Append(controlMenu, _("&Control"));
   menuBar->Append(viewMenu, _("&View"));
   menuBar->Append(helpMenu, _("&Help"));
   
   #ifdef __WXMAC__
      // prevent Window menu being added automatically by wxMac 2.6.1+
      menuBar->SetAutoWindowMenu(false);
   #endif

   // attach menu bar to the frame
   SetMenuBar(menuBar);

   // create tool bar
   #ifdef __WXX11__
      // creating vertical tool bar stuffs up X11 menu bar!!!
      wxToolBar *toolBar = CreateToolBar(wxTB_FLAT | wxNO_BORDER | wxTB_HORIZONTAL);
   #else
      // create vertical tool bar at left edge of frame
      wxToolBar *toolBar = CreateToolBar(wxTB_FLAT | wxNO_BORDER | wxTB_VERTICAL);
   #endif
   
   #ifdef __WXMAC__
      // this results in a tool bar that is 32 pixels wide (matches STATUS_HT)
      toolBar->SetMargins(4, 8);
   #elif __WXMSW__
      // Windows seems to ignore *any* margins!!!
      toolBar->SetMargins(0, 0);
   #else
      // X11 tool bar looks better with these margins
      toolBar->SetMargins(2, 2);
   #endif

   toolBar->SetToolBitmapSize(wxSize(16, 16));
   wxBitmap tbBitmaps[12];
   tbBitmaps[0] = wxBITMAP(play);
   tbBitmaps[1] = wxBITMAP(stop);
   tbBitmaps[2] = wxBITMAP(new);
   tbBitmaps[3] = wxBITMAP(open);
   tbBitmaps[4] = wxBITMAP(save);
   tbBitmaps[5] = wxBITMAP(draw);
   tbBitmaps[6] = wxBITMAP(select);
   tbBitmaps[7] = wxBITMAP(move);
   tbBitmaps[8] = wxBITMAP(zoomin);
   tbBitmaps[9] = wxBITMAP(zoomout);
   tbBitmaps[10] = wxBITMAP(info);
   tbBitmaps[11] = wxBITMAP(hash);

   #ifdef __WXX11__
      // reduce update probs by using toggle buttons
      #define ADD_TOOL(id, bmp, tooltip) \
         toolBar->AddCheckTool(id, "", bmp, wxNullBitmap, tooltip)
   #else
      #define ADD_TOOL(id, bmp, tooltip) \
         toolBar->AddTool(id, "", bmp, tooltip)
   #endif

   #define ADD_RADIO(id, bmp, tooltip) \
      toolBar->AddRadioTool(id, "", bmp, wxNullBitmap, tooltip)
   
   #define ADD_CHECK(id, bmp, tooltip) \
      toolBar->AddCheckTool(id, "", bmp, wxNullBitmap, tooltip)

   gotool = ADD_TOOL(ID_GO, tbBitmaps[0], _("Start generating"));
   stoptool = ADD_TOOL(ID_STOP, tbBitmaps[1], _("Stop generating"));
   ADD_CHECK(ID_HASH, tbBitmaps[11], _("Toggle hashing"));
   toolBar->AddSeparator();
   ADD_TOOL(wxID_NEW, tbBitmaps[2], _("New pattern"));
   ADD_TOOL(wxID_OPEN, tbBitmaps[3], _("Open pattern"));
   ADD_TOOL(wxID_SAVE, tbBitmaps[4], _("Save pattern"));
   toolBar->AddSeparator();
   ADD_RADIO(ID_DRAW, tbBitmaps[5], _("Draw"));
   ADD_RADIO(ID_SELECT, tbBitmaps[6], _("Select"));
   ADD_RADIO(ID_MOVE, tbBitmaps[7], _("Move"));
   ADD_RADIO(ID_ZOOMIN, tbBitmaps[8], _("Zoom in"));
   ADD_RADIO(ID_ZOOMOUT, tbBitmaps[9], _("Zoom out"));
   toolBar->AddSeparator();
   ADD_TOOL(ID_INFO, tbBitmaps[10], _("Pattern information"));
   
   toolBar->Realize();
   
   /* ChangeGoToStop and ChangeStopToGo don't work on Windows!!!
   // stop button will replace go button when generating = true
   toolBar->RemoveTool(ID_STOP);
   */

   int wd, ht;
   GetClientSize(&wd, &ht);
   // wd or ht might be < 1 on Win/X11 platforms
   if (wd < 1) wd = 1;
   if (ht < 1) ht = 1;
   
   // create viewport first so it gets focus whenever frame becomes active;
   // specify minimal size to avoid scroll bars being clipped on Mac
   viewptr = new PatternView(this, 0, 0, 40, 40);
   if (viewptr == NULL) Fatal("Failed to create pattern view!");
   // now set width and height to what we really want
   viewptr->SetSize(0, statusht, wd, ht > statusht ? ht - statusht : 0);

   #if wxUSE_DRAG_AND_DROP
      // let users drop files onto pattern view
      viewptr->SetDropTarget(new DnDFile());
   #endif

   // wxStatusBar can only appear at bottom of frame so we implement our
   // own status bar class which creates a child window at top of frame
   statusptr = new StatusBar(this, 0, 0, 100, 100);
   if (statusptr == NULL) Fatal("Failed to create status bar!");
   statusptr->SetSize(0, 0, wd, statusht);
}

MainFrame::~MainFrame() {
   delete onetimer;
}

// -----------------------------------------------------------------------------

#ifdef __WXMAC__
// handle odoc event
void MyApp::MacOpenFile(const wxString &fullPath)
{
   if (generating) return;

   frameptr->Raise();
   // need to process events to avoid crash if info window was in front
   while (wxGetApp().Pending()) wxGetApp().Dispatch();

   // set currfile using UTF8 encoding so fopen will work
   SetCurrentFile(fullPath);
   LoadPattern(GetBaseName(fullPath));
}
#endif

// app execution starts here
bool MyApp::OnInit()
{
   #ifdef __WXMAC__
      // prevent rectangle animation when windows open/close
      wxSystemOptions::SetOption(wxMAC_WINDOW_PLAIN_TRANSITION, 1);
      // prevent position problem in wxTextCtrl with wxTE_DONTWRAP style
      // (but doesn't fix problem with I-beam cursor over scroll bars)
      wxSystemOptions::SetOption(wxMAC_TEXTCONTROL_USE_MLTE, 1);
   #endif

   // let non-wx modules call Fatal, Warning, etc
   lifeerrors::seterrorhandler(&wxerrhandler);

   // start timer so we can use wxGetElapsedTime(false) to get elapsed millisecs
   wxStartTimer();

   // allow our .html files to include common graphic formats;
   // note that wxBMPHandler is always installed
   wxImage::AddHandler(new wxPNGHandler);
   wxImage::AddHandler(new wxGIFHandler);
   wxImage::AddHandler(new wxJPEGHandler);

   // set appdir -- must do before GetPrefs
   SetAppDirectory();
    
   // get main window location and other user preferences
   GetPrefs();
   
   // create main window
   frameptr = new MainFrame();
   if (frameptr == NULL) Fatal("Failed to create main window!");
   
   // initialize some stuff before showing main window
   CreatePens();
   CreateBrushes();
   CreateCursors();
   InitSelection();
   InitMagnifyTable();
   SetViewSize();
   SetMessage(BANNER);
   
   // load pattern if file supplied on Win/Unix command line
   if (argc > 1) {
      strncpy(currfile, argv[1], sizeof(currfile));
      LoadPattern(GetBaseName(currfile));
   } else {
      NewPattern();
   }   

   if (maximize) frameptr->Maximize(true);
   if (!showstatus) ToggleStatusBar();
   if (!showtool) ToggleToolBar();

   // now show main window
   frameptr->Show(true);
   SetTopWindow(frameptr);

   #ifdef __WXX11__
      // prevent main window being resized very small to avoid nasty errors
      // frameptr->SetMinSize(wxSize(minmainwd, minmainht));
      // above works but moves window to default pos!!!
      // and calling Move clobbers effect of SetMinSize!!! sigh
      // wxGetApp().Yield(true);
      // frameptr->Move(mainx, mainy);
   #endif

   // true means call wxApp::OnRun() which will enter the main event loop;
   // false means exit immediately
   return true;
}
