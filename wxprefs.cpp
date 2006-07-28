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

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
   #include "wx/wx.h"      // for all others include the necessary headers
#endif

#include "wx/filename.h"   // for wxFileName
#include "wx/stdpaths.h"   // for wxStandardPaths
#include "wx/propdlg.h"    // for wxPropertySheetDialog
#include "wx/colordlg.h"   // for wxColourDialog
#include "wx/bookctrl.h"   // for wxBookCtrlBase
#include "wx/notebook.h"   // for wxNotebookEvent
#include "wx/spinctrl.h"   // for wxSpinCtrl
#include "wx/image.h"      // for wxImage

#if defined(__WXMSW__) || defined(__WXGTK__)
   // can't seem to disable tool tips on Windows or Linux/GTK!!!
   // ie. wxToolTip::Enable and wxToolTip::SetDelay are both ignored;
   // yet another reason to eventually implement our own custom tool bar
   #undef wxUSE_TOOLTIPS
   #define wxUSE_TOOLTIPS 0
#endif
#if wxUSE_TOOLTIPS
   #include "wx/tooltip.h" // for wxToolTip
#endif

#include "lifealgo.h"      // for curralgo->...
#include "hlifealgo.h"     // for setVerbose, getVerbose
#include "viewport.h"      // for MAX_MAG

#include "wxgolly.h"       // for wxGetApp, mainptr
#include "wxmain.h"        // for GetID_CLEAR_PATTERNS, GetID_OPEN_RECENT, mainptr->...
#include "wxutils.h"       // for Warning, FillRect
#include "wxhelp.h"        // for GetHelpFrame
#include "wxinfo.h"        // for GetInfoFrame
#include "wxprefs.h"

#ifdef __WXMSW__
   // cursor bitmaps are loaded via .rc file
#else
   #ifdef __WXX11__
      // wxX11 doesn't support creating cursors from a bitmap file -- darn it!
   #else
      #include "bitmaps/zoomin_curs.xpm"
      #include "bitmaps/zoomout_curs.xpm"
   #endif
#endif

// -----------------------------------------------------------------------------

// Golly's preferences file is a simple text file created in the same directory
// as the application.  This makes uninstalling simple and allows multiple
// copies of the app to have separate settings.
const char PREFS_NAME[] = "GollyPrefs";

// location of supplied pattern collection (relative to app)
const wxString PATT_DIR = wxT("Patterns");

// location of supplied scripts (relative to app)
const wxString SCRIPT_DIR = wxT("Scripts");

const int PREFS_VERSION = 1;     // may change if file syntax changes
const int PREF_LINE_SIZE = 5000; // must be quite long for storing file paths

const int BITMAP_WD = 60;        // width of bitmap in color buttons
const int BITMAP_HT = 20;        // height of bitmap in color buttons

// initialize exported preferences:

int mainx = 30;                  // main window's initial location
int mainy = 40;
int mainwd = 800;                // main window's initial size
int mainht = 600;
bool maximize = false;           // maximize main window?

int helpx = 60;                  // help window's initial location
int helpy = 60;
int helpwd = 700;                // help window's initial size
int helpht = 500;
#ifdef __WXMAC__
   int helpfontsize = 12;        // font size in help window
#else
   int helpfontsize = 10;        // font size in help window
#endif

int infox = 90;                  // info window's initial location
int infoy = 90;
int infowd = 700;                // info window's initial size
int infoht = 500;

bool autofit = false;            // auto fit pattern while generating?
bool hashing = false;            // use hlife algorithm?
bool hyperspeed = false;         // use hyperspeed if supported by current algo?
bool showtool = true;            // show tool bar?
bool showstatus = true;          // show status bar?
bool showexact = false;          // show exact numbers in status bar?
bool showgridlines = true;       // display grid lines?
bool swapcolors = false;         // swap colors used for cell states?
bool buffered = true;            // use wxWdgets buffering to avoid flicker?
int randomfill = 50;             // random fill percentage (1..100)
int maxhashmem = 300;            // maximum hash memory (in megabytes)
int mingridmag = 2;              // minimum mag to draw grid lines
int boldspacing = 10;            // spacing of bold grid lines
bool showboldlines = true;       // show bold grid lines?
bool mathcoords = false;         // show Y values increasing upwards?
int newmag = MAX_MAG;            // mag setting for new pattern
bool newremovesel = true;        // new pattern removes selection?
bool openremovesel = true;       // opening pattern removes selection?
wxCursor *newcurs = NULL;        // cursor after creating new pattern (if not NULL)
wxCursor *opencurs = NULL;       // cursor after opening pattern (if not NULL)
char initrule[128] = "B3/S23";   // for first NewPattern before prefs saved
int mousewheelmode = 1;          // 0:Ignore, 1:forward=ZoomOut, 2:forward=ZoomIn
int thumbrange = 10;             // thumb box scrolling range in terms of view wd/ht
int qbasestep = 10;              // qlife's base step
int hbasestep = 8;               // hlife's base step (best if power of 2)
int mindelay = 250;              // minimum millisec delay (when warp = -1)
int maxdelay = 2000;             // maximum millisec delay
wxString opensavedir;            // directory for Open and Save dialogs
wxString rundir;                 // directory for Run Script dialog
wxString patterndir;             // directory used by Show Patterns
wxString scriptdir;              // directory used by Show Scripts
wxString pythonlib;              // name of Python library (loaded at runtime)
int dirwinwd = 180;              // width of directory window
bool showpatterns = true;        // show pattern directory?
bool showscripts = false;        // show script directory?
wxMenu *patternSubMenu = NULL;   // submenu of recent pattern files
wxMenu *scriptSubMenu = NULL;    // submenu of recent script files
int numpatterns = 0;             // current number of recent pattern files
int numscripts = 0;              // current number of recent script files
int maxpatterns = 20;            // maximum number of recent pattern files (1..MAX_RECENT)
int maxscripts = 20;             // maximum number of recent script files (1..MAX_RECENT)
wxArrayString namedrules;        // initialized in GetPrefs

wxColor *livergb;                // color for live cells
wxColor *deadrgb;                // color for dead cells
wxColor *pastergb;               // color for pasted pattern
wxColor *selectrgb;              // color for selected cells
wxColor *qlifergb;               // status bar background when using qlifealgo
wxColor *hlifergb;               // status bar background when using hlifealgo

wxBrush *livebrush;              // for drawing live cells
wxBrush *deadbrush;              // for drawing dead cells
wxBrush *qlifebrush;             // for status bar background when using qlifealgo
wxBrush *hlifebrush;             // for status bar background when using hlifealgo
wxPen *pastepen;                 // for drawing paste rect
wxPen *gridpen;                  // for drawing plain grid
wxPen *boldpen;                  // for drawing bold grid
wxPen *sgridpen;                 // for drawing plain grid if swapcolors is true
wxPen *sboldpen;                 // for drawing bold grid if swapcolors is true

// these settings must be static -- they are changed by GetPrefs *before* the
// view window is created
paste_location plocation = TopLeft;
paste_mode pmode = Or;

// these must be static -- they are created before the view window is created
wxCursor *curs_pencil;           // for drawing cells
wxCursor *curs_cross;            // for selecting cells
wxCursor *curs_hand;             // for moving view by dragging
wxCursor *curs_zoomin;           // for zooming in to a clicked cell
wxCursor *curs_zoomout;          // for zooming out from a clicked cell
wxCursor *currcurs;              // set to one of the above cursors

// local (ie. non-exported) globals:

wxString appdir;                 // path of directory containing app
bool showtips = true;            // show tool tips?
int mingridindex;                // mingridmag - 2
int newcursindex;
int opencursindex;

// -----------------------------------------------------------------------------

void CreateCursors()
{
   curs_pencil = new wxCursor(wxCURSOR_PENCIL);
   if (curs_pencil == NULL) Fatal(_("Failed to create pencil cursor!"));

   #ifdef __WXMSW__
      // don't use wxCURSOR_CROSS because it disappears on black background
      wxBitmap bitmap_cross = wxBITMAP(cross_curs);
      wxImage image_cross = bitmap_cross.ConvertToImage();
      image_cross.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_X, 8);
      image_cross.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_Y, 8);
      curs_cross = new wxCursor(image_cross);
   #else
      curs_cross = new wxCursor(wxCURSOR_CROSS);
   #endif
   if (curs_cross == NULL) Fatal(_("Failed to create cross cursor!"));

   curs_hand = new wxCursor(wxCURSOR_HAND);
   if (curs_hand == NULL) Fatal(_("Failed to create hand cursor!"));

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
   if (curs_zoomin == NULL) Fatal(_("Failed to create zoomin cursor!"));

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
   if (curs_zoomout == NULL) Fatal(_("Failed to create zoomout cursor!"));
   
   // set currcurs in case newcurs/opencurs are set to "No Change"
   currcurs = curs_pencil;
   
   // default cursors for new pattern or after opening pattern
   newcurs = curs_pencil;
   opencurs = curs_zoomin;
}

const char* CursorToString(wxCursor *curs)
{
   if (curs == curs_pencil) return "Draw";
   if (curs == curs_cross) return "Select";
   if (curs == curs_hand) return "Move";
   if (curs == curs_zoomin) return "Zoom In";
   if (curs == curs_zoomout) return "Zoom Out";
   return "No Change";   // curs is NULL
}

wxCursor* StringToCursor(const char *s)
{
   if (strcmp(s, "Draw") == 0) return curs_pencil;
   if (strcmp(s, "Select") == 0) return curs_cross;
   if (strcmp(s, "Move") == 0) return curs_hand;
   if (strcmp(s, "Zoom In") == 0) return curs_zoomin;
   if (strcmp(s, "Zoom Out") == 0) return curs_zoomout;
   return NULL;   // "No Change"
}

int CursorToIndex(wxCursor *curs)
{
   if (curs == curs_pencil) return 0;
   if (curs == curs_cross) return 1;
   if (curs == curs_hand) return 2;
   if (curs == curs_zoomin) return 3;
   if (curs == curs_zoomout) return 4;
   return 5;   // curs is NULL
}

wxCursor* IndexToCursor(int i)
{
   if (i == 0) return curs_pencil;
   if (i == 1) return curs_cross;
   if (i == 2) return curs_hand;
   if (i == 3) return curs_zoomin;
   if (i == 4) return curs_zoomout;
   return NULL;   // "No Change"
}

// -----------------------------------------------------------------------------

// following routines cannot be PatternView methods -- they are called by
// GetPrefs() before the view window is created

const char* GetPasteLocation()
{
   switch (plocation) {
      case TopLeft:     return "TopLeft";
      case TopRight:    return "TopRight";
      case BottomRight: return "BottomRight";
      case BottomLeft:  return "BottomLeft";
      case Middle:      return "Middle";
      default:          return "unknown";
   }
}

void SetPasteLocation(const char *s)
{
   if (strcmp(s, "TopLeft") == 0) {
      plocation = TopLeft;
   } else if (strcmp(s, "TopRight") == 0) {
      plocation = TopRight;
   } else if (strcmp(s, "BottomRight") == 0) {
      plocation = BottomRight;
   } else if (strcmp(s, "BottomLeft") == 0) {
      plocation = BottomLeft;
   } else {
      plocation = Middle;
   }
}

const char* GetPasteMode()
{
   switch (pmode) {
      case Copy:  return "Copy";
      case Or:    return "Or";
      case Xor:   return "Xor";
      default:    return "unknown";
   }
}

void SetPasteMode(const char *s)
{
   if (strcmp(s, "Copy") == 0) {
      pmode = Copy;
   } else if (strcmp(s, "Or") == 0) {
      pmode = Or;
   } else {
      pmode = Xor;
   }
}

// -----------------------------------------------------------------------------

void SetGridPens(wxColor* c, wxPen* ppen, wxPen* bpen)
{
   int r = c->Red();
   int g = c->Green();
   int b = c->Blue();
   // no need to use this standard grayscale conversion???
   // gray = (int) (0.299*r + 0.587*g + 0.114*b);
   int gray = (int) ((r + g + b) / 3.0);
   if (gray > 127) {
      // use darker grid
      ppen->SetColour(r > 32 ? r - 32 : 0,
                      g > 32 ? g - 32 : 0,
                      b > 32 ? b - 32 : 0);
      bpen->SetColour(r > 64 ? r - 64 : 0,
                      g > 64 ? g - 64 : 0,
                      b > 64 ? b - 64 : 0);
   } else {
      // use lighter grid
      ppen->SetColour(r + 32 < 256 ? r + 32 : 255,
                      g + 32 < 256 ? g + 32 : 255,
                      b + 32 < 256 ? b + 32 : 255);
      bpen->SetColour(r + 64 < 256 ? r + 64 : 255,
                      g + 64 < 256 ? g + 64 : 255,
                      b + 64 < 256 ? b + 64 : 255);
   }
}

void SetBrushesAndPens()
{
   livebrush->SetColour(*livergb);
   deadbrush->SetColour(*deadrgb);
   qlifebrush->SetColour(*qlifergb);
   hlifebrush->SetColour(*hlifergb);
   pastepen->SetColour(*pastergb);
   SetGridPens(deadrgb, gridpen, boldpen);
   SetGridPens(livergb, sgridpen, sboldpen);
}

void CreateDefaultColors()
{
   livergb = new wxColor(255, 255, 255);     // white
   deadrgb = new wxColor(48, 48, 48);        // dark gray (nicer if no alpha channel support)
   pastergb = new wxColor(255, 0, 0);        // red
   selectrgb = new wxColor(75, 175, 0);      // darkish green (becomes 50% transparent)
   qlifergb = new wxColor(255, 255, 206);    // pale yellow
   hlifergb = new wxColor(226, 250, 248);    // pale blue

   // create brushes and pens
   livebrush = new wxBrush(*wxBLACK);
   deadbrush = new wxBrush(*wxBLACK);
   qlifebrush = new wxBrush(*wxBLACK);
   hlifebrush = new wxBrush(*wxBLACK);
   pastepen = new wxPen(*wxBLACK);
   gridpen = new wxPen(*wxBLACK);
   boldpen = new wxPen(*wxBLACK);
   sgridpen = new wxPen(*wxBLACK);
   sboldpen = new wxPen(*wxBLACK);
   
   // set their default colors (in case prefs file doesn't exist)
   SetBrushesAndPens();
}

void GetColor(const char *value, wxColor *rgb)
{
   unsigned int r, g, b;
   sscanf(value, "%u,%u,%u", &r, &g, &b);
   rgb->Set(r, g, b);
}

void SaveColor(FILE *f, const char *name, const wxColor *rgb)
{
   fprintf(f, "%s=%d,%d,%d\n", name, rgb->Red(), rgb->Green(), rgb->Blue());
}

// -----------------------------------------------------------------------------

#define STRINGIFY(arg) STR2(arg)
#define STR2(arg) #arg
const char *GOLLY_VERSION = STRINGIFY(VERSION);

void SavePrefs()
{
   if (mainptr == NULL || curralgo == NULL) {
      // should never happen but play safe
      return;
   }
   
   FILE *f = fopen(PREFS_NAME, "w");
   if (f == NULL) {
      Warning(_("Could not save preferences file!"));
      return;
   }
   
   fprintf(f, "# NOTE: If you edit this file then do so when Golly isn't running\n");
   fprintf(f, "# otherwise all your changes will be clobbered when Golly quits.\n\n");
   fprintf(f, "prefs_version=%d\n", PREFS_VERSION);
   fprintf(f, "golly_version=%s\n", GOLLY_VERSION);
   #if defined(__WXMAC__)
      fprintf(f, "platform=Mac\n");
   #elif defined(__WXMSW__)
      fprintf(f, "platform=Windows\n");
   #elif defined(__WXX11__)
      fprintf(f, "platform=Linux/X11\n");
   #elif defined(__WXGTK__)
      fprintf(f, "platform=Linux/GTK\n");
   #else
      fprintf(f, "platform=unknown\n");
   #endif

   // save main window's location and size
   #ifdef __WXMSW__
   if (mainptr->fullscreen || mainptr->IsIconized()) {
      // use mainx, mainy, mainwd, mainht set by mainptr->ToggleFullScreen()
      // or by mainptr->OnSize
   #else
   if (mainptr->fullscreen) {
      // use mainx, mainy, mainwd, mainht set by mainptr->ToggleFullScreen()
   #endif
   } else {
      wxRect r = mainptr->GetRect();
      mainx = r.x;
      mainy = r.y;
      mainwd = r.width;
      mainht = r.height;
   }
   fprintf(f, "main_window=%d,%d,%d,%d\n", mainx, mainy, mainwd, mainht);

   #ifdef __WXX11__
      // mainptr->IsMaximized() is always true on X11 so avoid it
      fprintf(f, "maximize=%d\n", 0);
   #else
      fprintf(f, "maximize=%d\n", mainptr->IsMaximized() ? 1 : 0);
   #endif

   #ifdef __WXMSW__
   if (GetHelpFrame() && !GetHelpFrame()->IsIconized()) {
   #else
   if (GetHelpFrame()) {
   #endif
      wxRect r = GetHelpFrame()->GetRect();
      helpx = r.x;
      helpy = r.y;
      helpwd = r.width;
      helpht = r.height;
   }
   fprintf(f, "help_window=%d,%d,%d,%d\n", helpx, helpy, helpwd, helpht);
   fprintf(f, "help_font_size=%d (%d..%d)\n", helpfontsize, minfontsize, maxfontsize);

   #ifdef __WXMSW__
   if (GetInfoFrame() && !GetInfoFrame()->IsIconized()) {
   #else
   if (GetInfoFrame()) {
   #endif
      wxRect r = GetInfoFrame()->GetRect();
      infox = r.x;
      infoy = r.y;
      infowd = r.width;
      infoht = r.height;
   }
   fprintf(f, "info_window=%d,%d,%d,%d\n", infox, infoy, infowd, infoht);

   fprintf(f, "paste_location=%s\n", GetPasteLocation());
   fprintf(f, "paste_mode=%s\n", GetPasteMode());
   fprintf(f, "random_fill=%d (1..100)\n", randomfill);
   fprintf(f, "q_base_step=%d (2..%d)\n", qbasestep, MAX_BASESTEP);
   fprintf(f, "h_base_step=%d (2..%d, best if power of 2)\n", hbasestep, MAX_BASESTEP);
   fprintf(f, "min_delay=%d (0..%d millisecs)\n", mindelay, MAX_DELAY);
   fprintf(f, "max_delay=%d (0..%d millisecs)\n", maxdelay, MAX_DELAY);
   fprintf(f, "auto_fit=%d\n", autofit ? 1 : 0);
   fprintf(f, "hashing=%d\n", hashing ? 1 : 0);
   fprintf(f, "hyperspeed=%d\n", hyperspeed ? 1 : 0);
   fprintf(f, "hash_info=%d\n", hlifealgo::getVerbose() ? 1 : 0);
   fprintf(f, "max_hash_mem=%d\n", maxhashmem);
   
   fprintf(f, "\n");

#if defined(__WXMAC__) && wxCHECK_VERSION(2, 7, 0) && wxUSE_UNICODE
   //!!! avoid wxMac 2.7 bug in mb_str() in Unicode build
   wxCSConv convto(wxFONTENCODING_MACROMAN);
   #define MY_STR() mb_str(convto)
#else
   #define MY_STR() mb_str()
#endif

   fprintf(f, "rule=%s\n", curralgo->getrule());
   if (namedrules.GetCount() > 1) {
      size_t i;
      for (i=1; i<namedrules.GetCount(); i++)
         fprintf(f, "named_rule=%s\n", (const char*)namedrules[i].MY_STR());
   }
   
   fprintf(f, "\n");

   fprintf(f, "show_tool=%d\n", mainptr->GetToolBar()->IsShown() ? 1 : 0);
   fprintf(f, "show_tips=%d\n", showtips ? 1 : 0);
   fprintf(f, "show_status=%d\n", mainptr->StatusVisible() ? 1 : 0);
   fprintf(f, "show_exact=%d\n", showexact ? 1 : 0);
   fprintf(f, "grid_lines=%d\n", showgridlines ? 1 : 0);
   fprintf(f, "min_grid_mag=%d (2..%d)\n", mingridmag, MAX_MAG);
   fprintf(f, "bold_spacing=%d (2..%d)\n", boldspacing, MAX_SPACING);
   fprintf(f, "show_bold_lines=%d\n", showboldlines ? 1 : 0);
   fprintf(f, "math_coords=%d\n", mathcoords ? 1 : 0);
   
   fprintf(f, "\n");

   fprintf(f, "swap_colors=%d\n", swapcolors ? 1 : 0);
   SaveColor(f, "live_rgb", livergb);
   SaveColor(f, "dead_rgb", deadrgb);
   SaveColor(f, "paste_rgb", pastergb);
   SaveColor(f, "select_rgb", selectrgb);
   SaveColor(f, "qlife_rgb", qlifergb);
   SaveColor(f, "hlife_rgb", hlifergb);
   
   fprintf(f, "\n");
   
   fprintf(f, "buffered=%d\n", buffered ? 1 : 0);
   fprintf(f, "mouse_wheel_mode=%d\n", mousewheelmode);
   fprintf(f, "thumb_range=%d (2..%d)\n", thumbrange, MAX_THUMBRANGE);
   fprintf(f, "new_mag=%d (0..%d)\n", newmag, MAX_MAG);
   fprintf(f, "new_remove_sel=%d\n", newremovesel ? 1 : 0);
   fprintf(f, "new_cursor=%s\n", CursorToString(newcurs));
   fprintf(f, "open_remove_sel=%d\n", openremovesel ? 1 : 0);
   fprintf(f, "open_cursor=%s\n", CursorToString(opencurs));
   
   fprintf(f, "\n");

   fprintf(f, "open_save_dir=%s\n", (const char*)opensavedir.MY_STR());
   fprintf(f, "run_dir=%s\n", (const char*)rundir.MY_STR());
   fprintf(f, "pattern_dir=%s\n", (const char*)patterndir.MY_STR());
   fprintf(f, "script_dir=%s\n", (const char*)scriptdir.MY_STR());
   fprintf(f, "python_lib=%s\n", (const char*)pythonlib.MY_STR());
   fprintf(f, "dir_width=%d\n", dirwinwd);
   fprintf(f, "show_patterns=%d\n", showpatterns ? 1 : 0);
   fprintf(f, "show_scripts=%d\n", showscripts ? 1 : 0);
   fprintf(f, "max_patterns=%d (1..%d)\n", maxpatterns, MAX_RECENT);
   fprintf(f, "max_scripts=%d (1..%d)\n", maxscripts, MAX_RECENT);

   if (numpatterns > 0) {
      fprintf(f, "\n");
      int i;
      for (i=0; i<numpatterns; i++) {
         wxMenuItem *item = patternSubMenu->FindItemByPosition(i);
         if (item) fprintf(f, "recent_pattern=%s\n",
                           (const char*)item->GetText().MY_STR());
      }
   }

   if (numscripts > 0) {
      fprintf(f, "\n");
      int i;
      for (i=0; i<numscripts; i++) {
         wxMenuItem *item = scriptSubMenu->FindItemByPosition(i);
         if (item) fprintf(f, "recent_script=%s\n",
                           (const char*)item->GetText().MY_STR());
      }
   }
   
   fclose(f);
}

// -----------------------------------------------------------------------------

wxString FindAppDir()
{
   // return path to app's directory, terminated by path separator
   wxString argv0 = wxGetApp().argv[0];
   wxString currdir = wxGetCwd();

   #ifdef __WXMSW__

      // on Windows we don't need to use argv0 or currdir
      wxStandardPaths wxstdpaths;
      wxString str = wxstdpaths.GetDataDir();
      if (str.Last() != wxFILE_SEP_PATH) str += wxFILE_SEP_PATH;
      return str;

   #elif defined(__WXMAC__)

      // on Mac OS X argv0 is an absolute path to the executable:
      // eg. "/Volumes/HD/Apps/foo.app/Contents/MacOS/foo"
      // and currdir is an absolute path to the bundled app's location:
      // eg. "/Volumes/HD/Apps"
      if (currdir.Last() != wxFILE_SEP_PATH) currdir += wxFILE_SEP_PATH;
      return currdir;

   #elif defined(__UNIX__)

      // argv0 is 1st string on command line: eg. "./golly" or "/usr/apps/golly"
      // and currdir is the current working directory where the command was
      // invoked, which is not necessarily where the app is located
      wxString str;
      if (wxIsAbsolutePath(argv0)) {
         str = wxPathOnly(argv0);
      } else {
         // relative path so remove "./" prefix if present
         if (argv0.StartsWith(wxT("./"))) argv0 = argv0.AfterFirst('/');
         if (currdir.Last() != wxFILE_SEP_PATH) currdir += wxFILE_SEP_PATH;
         str = currdir + argv0;
         str = wxPathOnly(str);
      }
      if (str.Last() != wxFILE_SEP_PATH) str += wxFILE_SEP_PATH;
      return str;
   
   #endif
}

// -----------------------------------------------------------------------------

void AddDefaultRules()
{
   namedrules.Add(wxT("3-4 Life|B34/S34"));
   namedrules.Add(wxT("HighLife|B36/S23"));
   namedrules.Add(wxT("AntiLife|B0123478/S01234678"));
   namedrules.Add(wxT("Life without Death|B3/S012345678"));
   namedrules.Add(wxT("Plow World|B378/S012345678"));
   namedrules.Add(wxT("Day and Night|B3678/S34678"));
   namedrules.Add(wxT("Diamoeba|B35678/S5678"));
   namedrules.Add(wxT("LongLife|B345/S5"));
   namedrules.Add(wxT("Seeds|B2"));
   namedrules.Add(wxT("Persian Rug|B234"));
   namedrules.Add(wxT("Replicator|B1357/S1357"));
   namedrules.Add(wxT("Fredkin|B1357/S02468"));
   namedrules.Add(wxT("Morley|B368/S245"));
   namedrules.Add(wxT("Wolfram 22|W22"));
   namedrules.Add(wxT("Wolfram 30|W30"));
   namedrules.Add(wxT("Wolfram 110|W110"));
}

// -----------------------------------------------------------------------------

bool GetKeyVal(FILE *f, char *line, char **keyword, char **value)
{
   while ( fgets(line, PREF_LINE_SIZE, f) != 0 ) {
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

// -----------------------------------------------------------------------------

void CheckVisibility(int *x, int *y, int *wd, int *ht)
{
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

// -----------------------------------------------------------------------------

void GetPrefs()
{
   appdir = FindAppDir();
   opensavedir = appdir + PATT_DIR;
   rundir = appdir + SCRIPT_DIR;
   patterndir = appdir + PATT_DIR;
   scriptdir = appdir + SCRIPT_DIR;
   
   #ifdef __WXMSW__
      pythonlib = wxT("python24.dll");
   #elif defined(__WXMAC__)
      pythonlib = wxT("not used");
   #elif defined(__UNIX__)
      pythonlib = wxT("libpython2.4.so");
   #endif

   // create curs_* and initialize newcurs, opencurs and currcurs
   CreateCursors();
   
   CreateDefaultColors();
   
   // initialize Open Recent submenu
   patternSubMenu = new wxMenu();
   patternSubMenu->AppendSeparator();
   patternSubMenu->Append(GetID_CLEAR_PATTERNS(), _("Clear Menu"));
   
   // initialize Run Recent submenu
   scriptSubMenu = new wxMenu();
   scriptSubMenu->AppendSeparator();
   scriptSubMenu->Append(GetID_CLEAR_SCRIPTS(), _("Clear Menu"));

   namedrules.Add(wxT("Life|B3/S23"));      // must be 1st entry

   if ( !wxFileExists(wxString(PREFS_NAME,wxConvLibc)) ) {
      AddDefaultRules();
      return;
   }
   
   FILE *f = fopen(PREFS_NAME, "r");
   if (f == NULL) {
      Warning(_("Could not read preferences file!"));
      return;
   }

#if defined(__WXMAC__) && wxCHECK_VERSION(2, 7, 0) && wxUSE_UNICODE
   //!!! avoid wxMac 2.7 bug in mb_str() in Unicode build
   wxCSConv convfrom(wxFONTENCODING_MACROMAN);
   #define MY_CONV convfrom
#else
   #define MY_CONV wxConvLibc
#endif

   char line[PREF_LINE_SIZE];
   char *keyword;
   char *value;
   while ( GetKeyVal(f, line, &keyword, &value) ) {
      // remove \n from end of value
      int len = strlen(value);
      if ( len > 0 && value[len-1] == '\n' ) {
         value[len-1] = 0;
      }

      if (strcmp(keyword, "prefs_version") == 0) {
         int currversion;
         if (sscanf(value, "%d", &currversion) == 1 && currversion < PREFS_VERSION) {
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

      } else if (strcmp(keyword, "help_font_size") == 0) {
         sscanf(value, "%d", &helpfontsize);
         if (helpfontsize < minfontsize) helpfontsize = minfontsize;
         if (helpfontsize > maxfontsize) helpfontsize = maxfontsize;

      } else if (strcmp(keyword, "info_window") == 0) {
         sscanf(value, "%d,%d,%d,%d", &infox, &infoy, &infowd, &infoht);
         if (infowd < mininfowd) infowd = mininfowd;
         if (infoht < mininfoht) infoht = mininfoht;
         CheckVisibility(&infox, &infoy, &infowd, &infoht);

      } else if (strcmp(keyword, "paste_location") == 0) {
         SetPasteLocation(value);

      } else if (strcmp(keyword, "paste_mode") == 0) {
         SetPasteMode(value);

      } else if (strcmp(keyword, "random_fill") == 0) {
         sscanf(value, "%d", &randomfill);
         if (randomfill < 1) randomfill = 1;
         if (randomfill > 100) randomfill = 100;

      } else if (strcmp(keyword, "q_base_step") == 0) {
         sscanf(value, "%d", &qbasestep);
         if (qbasestep < 2) qbasestep = 2;
         if (qbasestep > MAX_BASESTEP) qbasestep = MAX_BASESTEP;

      } else if (strcmp(keyword, "h_base_step") == 0) {
         sscanf(value, "%d", &hbasestep);
         if (hbasestep < 2) hbasestep = 2;
         if (hbasestep > MAX_BASESTEP) hbasestep = MAX_BASESTEP;

      } else if (strcmp(keyword, "min_delay") == 0) {
         sscanf(value, "%d", &mindelay);
         if (mindelay < 0) mindelay = 0;
         if (mindelay > MAX_DELAY) mindelay = MAX_DELAY;

      } else if (strcmp(keyword, "max_delay") == 0) {
         sscanf(value, "%d", &maxdelay);
         if (maxdelay < 0) maxdelay = 0;
         if (maxdelay > MAX_DELAY) maxdelay = MAX_DELAY;

      } else if (strcmp(keyword, "auto_fit") == 0) {
         autofit = value[0] == '1';

      } else if (strcmp(keyword, "hashing") == 0) {
         hashing = value[0] == '1';

      } else if (strcmp(keyword, "hyperspeed") == 0) {
         hyperspeed = value[0] == '1';

      } else if (strcmp(keyword, "hash_info") == 0) {
         hlifealgo::setVerbose(value[0] == '1');

      } else if (strcmp(keyword, "max_hash_mem") == 0) {
         sscanf(value, "%d", &maxhashmem);
         if (maxhashmem < MIN_HASHMB) maxhashmem = MIN_HASHMB;
         if (maxhashmem > MAX_HASHMB) maxhashmem = MAX_HASHMB;

      } else if (strcmp(keyword, "rule") == 0) {
         strncpy(initrule, value, sizeof(initrule));

      } else if (strcmp(keyword, "named_rule") == 0) {
         namedrules.Add(wxString(value,MY_CONV));

      } else if (strcmp(keyword, "show_tool") == 0) {
         showtool = value[0] == '1';

      } else if (strcmp(keyword, "show_tips") == 0) {
         showtips = value[0] == '1';

      } else if (strcmp(keyword, "show_status") == 0) {
         showstatus = value[0] == '1';

      } else if (strcmp(keyword, "show_exact") == 0) {
         showexact = value[0] == '1';

      } else if (strcmp(keyword, "grid_lines") == 0) {
         showgridlines = value[0] == '1';

      } else if (strcmp(keyword, "min_grid_mag") == 0) {
         sscanf(value, "%d", &mingridmag);
         if (mingridmag < 2) mingridmag = 2;
         if (mingridmag > MAX_MAG) mingridmag = MAX_MAG;

      } else if (strcmp(keyword, "bold_spacing") == 0) {
         sscanf(value, "%d", &boldspacing);
         if (boldspacing < 2) boldspacing = 2;
         if (boldspacing > MAX_SPACING) boldspacing = MAX_SPACING;

      } else if (strcmp(keyword, "show_bold_lines") == 0) {
         showboldlines = value[0] == '1';

      } else if (strcmp(keyword, "math_coords") == 0) {
         mathcoords = value[0] == '1';

      } else if (strcmp(keyword, "swap_colors") == 0) {
         swapcolors = value[0] == '1';

      } else if (strcmp(keyword, "live_rgb") == 0) {
         GetColor(value, livergb);

      } else if (strcmp(keyword, "dead_rgb") == 0) {
         GetColor(value, deadrgb);

      } else if (strcmp(keyword, "paste_rgb") == 0) {
         GetColor(value, pastergb);

      } else if (strcmp(keyword, "select_rgb") == 0) {
         GetColor(value, selectrgb);

      } else if (strcmp(keyword, "qlife_rgb") == 0) {
         GetColor(value, qlifergb);

      } else if (strcmp(keyword, "hlife_rgb") == 0) {
         GetColor(value, hlifergb);

      } else if (strcmp(keyword, "buffered") == 0) {
         buffered = value[0] == '1';

      } else if (strcmp(keyword, "mouse_wheel_mode") == 0) {
         sscanf(value, "%d", &mousewheelmode);
         if (mousewheelmode < 0) mousewheelmode = 0;
         if (mousewheelmode > 2) mousewheelmode = 2;

      } else if (strcmp(keyword, "thumb_range") == 0) {
         sscanf(value, "%d", &thumbrange);
         if (thumbrange < 2) thumbrange = 2;
         if (thumbrange > MAX_THUMBRANGE) thumbrange = MAX_THUMBRANGE;

      } else if (strcmp(keyword, "new_mag") == 0) {
         sscanf(value, "%d", &newmag);
         if (newmag < 0) newmag = 0;
         if (newmag > MAX_MAG) newmag = MAX_MAG;
         
      } else if (strcmp(keyword, "new_remove_sel") == 0) {
         newremovesel = value[0] == '1';

      } else if (strcmp(keyword, "new_cursor") == 0) {
         newcurs = StringToCursor(value);
         
      } else if (strcmp(keyword, "open_remove_sel") == 0) {
         openremovesel = value[0] == '1';

      } else if (strcmp(keyword, "open_cursor") == 0) {
         opencurs = StringToCursor(value);

      } else if (strcmp(keyword, "open_save_dir") == 0) {
         opensavedir = wxString(value,MY_CONV);
         if ( !wxFileName::DirExists(opensavedir) ) {
            // reset to supplied pattern directory
            opensavedir = appdir + PATT_DIR;
         }

      } else if (strcmp(keyword, "run_dir") == 0) {
         rundir = wxString(value,MY_CONV);
         if ( !wxFileName::DirExists(rundir) ) {
            // reset to supplied script directory
            rundir = appdir + SCRIPT_DIR;
         }

      } else if (strcmp(keyword, "pattern_dir") == 0) {
         patterndir = wxString(value,MY_CONV);
         if ( !wxFileName::DirExists(patterndir) ) {
            // reset to supplied pattern directory
            patterndir = appdir + PATT_DIR;
         }

      } else if (strcmp(keyword, "script_dir") == 0) {
         scriptdir = wxString(value,MY_CONV);
         if ( !wxFileName::DirExists(scriptdir) ) {
            // reset to supplied script directory
            scriptdir = appdir + SCRIPT_DIR;
         }

      } else if (strcmp(keyword, "python_lib") == 0) {
         pythonlib = wxString(value,MY_CONV);
         
      } else if (strcmp(keyword, "dir_width") == 0) {
         sscanf(value, "%d", &dirwinwd);
         if (dirwinwd < MIN_DIRWD) dirwinwd = MIN_DIRWD;
         
      } else if (strcmp(keyword, "show_patterns") == 0) {
         showpatterns = value[0] == '1';
         
      } else if (strcmp(keyword, "show_scripts") == 0) {
         showscripts = value[0] == '1';

      } else if (strcmp(keyword, "max_patterns") == 0) {
         sscanf(value, "%d", &maxpatterns);
         if (maxpatterns < 1) maxpatterns = 1;
         if (maxpatterns > MAX_RECENT) maxpatterns = MAX_RECENT;

      } else if (strcmp(keyword, "max_scripts") == 0) {
         sscanf(value, "%d", &maxscripts);
         if (maxscripts < 1) maxscripts = 1;
         if (maxscripts > MAX_RECENT) maxscripts = MAX_RECENT;

      } else if (strcmp(keyword, "recent_pattern") == 0) {
         // append path to Open Recent submenu
         if (numpatterns < maxpatterns) {
            numpatterns++;
            patternSubMenu->Insert(numpatterns - 1, GetID_OPEN_RECENT() + numpatterns,
                                   wxString(value,MY_CONV));
         }

      } else if (strcmp(keyword, "recent_script") == 0) {
         // append path to Run Recent submenu
         if (numscripts < maxscripts) {
            numscripts++;
            scriptSubMenu->Insert(numscripts - 1, GetID_RUN_RECENT() + numscripts,
                                  wxString(value,MY_CONV));
         }
      }
   }
   fclose(f);

   // colors for brushes and pens may have changed
   SetBrushesAndPens();
   
   // showpatterns and showscripts must not both be true
   if (showpatterns && showscripts) showscripts = false;
   
   // if no named_rule entries then add default names
   if (namedrules.GetCount() == 1) AddDefaultRules();
   
   #if wxUSE_TOOLTIPS
      wxToolTip::Enable(showtips);
      wxToolTip::SetDelay(1000);    // 1 sec
   #endif
}

// -----------------------------------------------------------------------------

// define a multi-page dialog for changing various preferences

size_t currpage = 0;    // current page in PrefsDialog

class PrefsDialog : public wxPropertySheetDialog
{
public:
   PrefsDialog(wxWindow* parent);
   ~PrefsDialog();

   wxPanel* CreateFilePrefs(wxWindow* parent);
   wxPanel* CreateEditPrefs(wxWindow* parent);
   wxPanel* CreateControlPrefs(wxWindow* parent);
   wxPanel* CreateViewPrefs(wxWindow* parent);
   wxPanel* CreateColorPrefs(wxWindow* parent);

   virtual bool TransferDataFromWindow();    // called when user hits OK

   #ifdef __WXMAC__
      // allow hitting tab to switch focus
      void OnSpinCtrlChar(wxKeyEvent& event);
   #endif

private:
   enum {
      // these *_PAGE values must correspond to currpage values
      FILE_PAGE = 0,
      EDIT_PAGE,
      CONTROL_PAGE,
      VIEW_PAGE,
      COLOR_PAGE,
      // File prefs
      PREF_NEW_REM_SEL,
      PREF_NEW_CURSOR,
      PREF_NEW_SCALE,
      PREF_OPEN_REM_SEL,
      PREF_OPEN_CURSOR,
      PREF_MAX_PATTERNS,
      PREF_MAX_SCRIPTS,
      // Edit prefs
      PREF_RANDOM_FILL,
      // Control prefs
      PREF_MAX_HASH_MEM,
      PREF_QBASE,
      PREF_HBASE,
      PREF_MIN_DELAY,
      PREF_MAX_DELAY,
      // View prefs
      PREF_SHOW_TIPS,
      PREF_Y_UP,
      PREF_SHOW_BOLD,
      PREF_BOLD_SPACING,
      PREF_MIN_GRID_SCALE,
      PREF_MOUSE_WHEEL,
      PREF_THUMB_RANGE,
      // Color prefs
      PREF_LIVE_RGB,
      PREF_DEAD_RGB,
      PREF_PASTE_RGB,
      PREF_SELECT_RGB,
      PREF_QLIFE_RGB,
      PREF_HLIFE_RGB
   };

   bool GetCheckVal(long id);
   int GetChoiceVal(long id);
   int GetSpinVal(long id);
   bool BadSpinVal(int id, int minval, int maxval, const wxString& prefix);
   bool ValidatePage();
   void ChangeColor(int id, wxColor* rgb);
   void AddColorButton(wxWindow* parent, wxBoxSizer* vbox,
                       int id, wxColor* rgb, const wxString& text);
   
   void OnCheckBoxClicked(wxCommandEvent& event);
   void OnColorButton(wxCommandEvent& event);
   void OnPageChanging(wxNotebookEvent& event);
   void OnPageChanged(wxNotebookEvent& event);

   bool ignore_page_event;       // used to prevent currpage being changed
   bool color_changed;           // have one or more colors changed?

   wxColor *new_livergb;         // new color for live cells
   wxColor *new_deadrgb;         // new color for dead cells
   wxColor *new_pastergb;        // new color for pasted pattern
   wxColor *new_selectrgb;       // new color for selected cells
   wxColor *new_qlifergb;        // new status bar color when using qlifealgo
   wxColor *new_hlifergb;        // new status bar color when using hlifealgo

   DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(PrefsDialog, wxPropertySheetDialog)
   EVT_CHECKBOX               (wxID_ANY, PrefsDialog::OnCheckBoxClicked)
   EVT_BUTTON                 (wxID_ANY, PrefsDialog::OnColorButton)
   EVT_NOTEBOOK_PAGE_CHANGING (wxID_ANY, PrefsDialog::OnPageChanging)
   EVT_NOTEBOOK_PAGE_CHANGED  (wxID_ANY, PrefsDialog::OnPageChanged)
END_EVENT_TABLE()

// -----------------------------------------------------------------------------

#ifdef __WXMAC__

// fix wxMac bug???
// override key event handler for wxSpinCtrl to allow tab key handling
class MySpinCtrl : public wxSpinCtrl
{
public:
   MySpinCtrl(wxWindow *parent, wxWindowID id, const wxString& str,
              const wxPoint& pos, const wxSize& size)
      : wxSpinCtrl(parent, id, str, pos, size)
   {
      // create a dynamic event handler for the underlying wxTextCtrl
      wxTextCtrl* textctrl = GetText();
      if (textctrl) {
         textctrl->Connect(wxID_ANY, wxEVT_CHAR,
                           wxKeyEventHandler(PrefsDialog::OnSpinCtrlChar));
      }
   }
};

void PrefsDialog::OnSpinCtrlChar(wxKeyEvent& event)
{
   int key = event.GetKeyCode();

   if ( key == WXK_TAB ) {
      // wxMac bug??? FindFocus() returns pointer to wxTextCtrl window in wxSpinCtrl!!!
      // does that explain why Navigate() does nothing???
      if ( currpage == FILE_PAGE ) {
         wxSpinCtrl* s1 = (wxSpinCtrl*) FindWindowById(PREF_MAX_PATTERNS);
         wxSpinCtrl* s2 = (wxSpinCtrl*) FindWindowById(PREF_MAX_SCRIPTS);
         wxTextCtrl* t1 = s1->GetText();
         wxTextCtrl* t2 = s2->GetText();
         wxWindow* focus = FindFocus();
         if ( focus == t1 ) { s2->SetFocus(); s2->SetSelection(-1,-1); }
         if ( focus == t2 ) { s1->SetFocus(); s1->SetSelection(-1,-1); }
      } else if ( currpage == EDIT_PAGE ) {
         // only 1 spin ctrl on this page
         wxSpinCtrl* s1 = (wxSpinCtrl*) FindWindowById(PREF_RANDOM_FILL);
         if ( s1 ) { s1->SetFocus(); s1->SetSelection(-1,-1); }
      } else if ( currpage == CONTROL_PAGE ) {
         wxSpinCtrl* s1 = (wxSpinCtrl*) FindWindowById(PREF_MAX_HASH_MEM);
         wxSpinCtrl* s2 = (wxSpinCtrl*) FindWindowById(PREF_QBASE);
         wxSpinCtrl* s3 = (wxSpinCtrl*) FindWindowById(PREF_HBASE);
         wxSpinCtrl* s4 = (wxSpinCtrl*) FindWindowById(PREF_MIN_DELAY);
         wxSpinCtrl* s5 = (wxSpinCtrl*) FindWindowById(PREF_MAX_DELAY);
         wxTextCtrl* t1 = s1->GetText();
         wxTextCtrl* t2 = s2->GetText();
         wxTextCtrl* t3 = s3->GetText();
         wxTextCtrl* t4 = s4->GetText();
         wxTextCtrl* t5 = s5->GetText();
         wxWindow* focus = FindFocus();
         if ( focus == t1 ) { s2->SetFocus(); s2->SetSelection(-1,-1); }
         if ( focus == t2 ) { s3->SetFocus(); s3->SetSelection(-1,-1); }
         if ( focus == t3 ) { s4->SetFocus(); s4->SetSelection(-1,-1); }
         if ( focus == t4 ) { s5->SetFocus(); s5->SetSelection(-1,-1); }
         if ( focus == t5 ) { s1->SetFocus(); s1->SetSelection(-1,-1); }
      } else if ( currpage == VIEW_PAGE ) {
         wxSpinCtrl* s1 = (wxSpinCtrl*) FindWindowById(PREF_BOLD_SPACING);
         wxSpinCtrl* s2 = (wxSpinCtrl*) FindWindowById(PREF_THUMB_RANGE);
         wxTextCtrl* t1 = s1->GetText();
         wxTextCtrl* t2 = s2->GetText();
         wxWindow* focus = FindFocus();
         if ( focus == t1 ) { s2->SetFocus(); s2->SetSelection(-1,-1); }
         if ( focus == t2 ) { s1->SetFocus(); s1->SetSelection(-1,-1); }
      }

   } else if ( key >= ' ' && key <= '~' ) {
      if ( key >= '0' && key <= '9' ) {
         // allow digits
         event.Skip();
      } else {
         // disallow any other displayable ascii char
         wxBell();
      }

   } else {
      event.Skip();
   }
}

#else

#define MySpinCtrl wxSpinCtrl

#endif // __WXMAC__

// -----------------------------------------------------------------------------

PrefsDialog::PrefsDialog(wxWindow* parent)
{
   // not using validators so no need for this:
   // SetExtraStyle(wxWS_EX_VALIDATE_RECURSIVELY);
   
   Create(parent, wxID_ANY, _("Preferences"), wxDefaultPosition, wxDefaultSize);
   CreateButtons(wxOK | wxCANCEL);
   
   wxBookCtrlBase* notebook = GetBookCtrl();
   
   wxPanel* filePrefs = CreateFilePrefs(notebook);
   wxPanel* editPrefs = CreateEditPrefs(notebook);
   wxPanel* ctrlPrefs = CreateControlPrefs(notebook);
   wxPanel* viewPrefs = CreateViewPrefs(notebook);
   wxPanel* colorPrefs = CreateColorPrefs(notebook);
   
   // AddPage and SetSelection cause OnPageChanging and OnPageChanged to be called
   // so we use a flag to prevent currpage being changed (and unnecessary validation)
   ignore_page_event = true;

   notebook->AddPage(filePrefs, _("File"));
   notebook->AddPage(editPrefs, _("Edit"));
   notebook->AddPage(ctrlPrefs, _("Control"));
   notebook->AddPage(viewPrefs, _("View"));
   notebook->AddPage(colorPrefs, _("Color"));

   // show last selected page
   notebook->SetSelection(currpage);

   ignore_page_event = false;
   color_changed = false;

   #ifdef __WXMAC__
      // wxMac bug??? avoid ALL spin control values being selected
      wxSpinCtrl* sp;
      // deselect other spin control on FILE_PAGE
      sp = (wxSpinCtrl*) FindWindow(PREF_MAX_SCRIPTS); sp->SetSelection(0,0);
      // deselect other spin controls on CONTROL_PAGE
      sp = (wxSpinCtrl*) FindWindow(PREF_QBASE); sp->SetSelection(0,0);
      sp = (wxSpinCtrl*) FindWindow(PREF_HBASE); sp->SetSelection(0,0);
      sp = (wxSpinCtrl*) FindWindow(PREF_MIN_DELAY); sp->SetSelection(0,0);
      sp = (wxSpinCtrl*) FindWindow(PREF_MAX_DELAY); sp->SetSelection(0,0);
      // deselect other spin control on VIEW_PAGE
      if (showboldlines) {
         sp = (wxSpinCtrl*) FindWindow(PREF_THUMB_RANGE); sp->SetSelection(0,0);
      } else {
         sp = (wxSpinCtrl*) FindWindow(PREF_BOLD_SPACING); sp->SetSelection(0,0);
      }
   #endif
   
   LayoutDialog();
}

// -----------------------------------------------------------------------------

// these consts are used to get nicely spaced controls on each platform:

#ifdef __WXMAC__
   #define GROUPGAP (12)      // vertical gap between a group of controls
   #define SBTOPGAP (2)       // vertical gap before first item in wxStaticBoxSizer
   #define SBBOTGAP (2)       // vertical gap after last item in wxStaticBoxSizer
   #define SVGAP (4)          // vertical gap above wxSpinCtrl box
   #define S2VGAP (0)         // vertical gap between 2 wxSpinCtrl boxes
   #define CH2VGAP (9)        // vertical gap between 2 check boxes
   #define CVGAP (9)          // vertical gap above wxChoice box
   #define LRGAP (5)          // space left and right of vertically stacked boxes
   #define SPINGAP (3)        // horizontal gap around each wxSpinCtrl box
   #define CHOICEGAP (6)      // horizontal gap to left of wxChoice box
#elif defined(__WXMSW__)
   #define GROUPGAP (10)
   #define SBTOPGAP (7)
   #define SBBOTGAP (7)
   #define SVGAP (7)
   #define S2VGAP (5)
   #define CH2VGAP (11)
   #define CVGAP (7)
   #define LRGAP (5)
   #define SPINGAP (6)
   #define CHOICEGAP (6)
#else
   #define GROUPGAP (10)
   #define SBTOPGAP (12)
   #define SBBOTGAP (7)
   #define SVGAP (7)
   #define S2VGAP (5)
   #define CH2VGAP (11)
   #define CVGAP (7)
   #define LRGAP (5)
   #define SPINGAP (6)
   #define CHOICEGAP (6)
#endif

// -----------------------------------------------------------------------------

wxPanel* PrefsDialog::CreateFilePrefs(wxWindow* parent)
{
   wxPanel* panel = new wxPanel(parent, wxID_ANY);
   wxBoxSizer* topSizer = new wxBoxSizer( wxVERTICAL );
   wxBoxSizer* vbox = new wxBoxSizer( wxVERTICAL );

   wxArrayString newcursorChoices;
   newcursorChoices.Add(wxT("Draw"));
   newcursorChoices.Add(wxT("Select"));
   newcursorChoices.Add(wxT("Move"));
   newcursorChoices.Add(wxT("Zoom In"));
   newcursorChoices.Add(wxT("Zoom Out"));
   newcursorChoices.Add(wxT("No Change"));

   wxArrayString opencursorChoices = newcursorChoices;

   wxArrayString newscaleChoices;
   newscaleChoices.Add(wxT("1:1"));
   newscaleChoices.Add(wxT("1:2"));
   newscaleChoices.Add(wxT("1:4"));
   newscaleChoices.Add(wxT("1:8"));
   newscaleChoices.Add(wxT("1:16"));
   
   // on new pattern
   
   wxStaticBox* sbox1 = new wxStaticBox(panel, wxID_ANY, _("On creating a new pattern:"));
   wxBoxSizer* ssizer1 = new wxStaticBoxSizer( sbox1, wxVERTICAL );
   vbox->Add(ssizer1, 0, wxGROW | wxALL, 2);

   ssizer1->AddSpacer(SBTOPGAP);
   wxCheckBox* check1 = new wxCheckBox(panel, PREF_NEW_REM_SEL,
                                       _("Remove selection"),
                                       wxDefaultPosition, wxDefaultSize);
   ssizer1->Add(check1, 0, wxLEFT | wxRIGHT, LRGAP);

   wxBoxSizer* setcursbox = new wxBoxSizer( wxHORIZONTAL );
   setcursbox->Add(new wxStaticText(panel, wxID_STATIC, _("Set cursor:")), 0, wxALL, 0);

   wxBoxSizer* setscalebox = new wxBoxSizer( wxHORIZONTAL );
   setscalebox->Add(new wxStaticText(panel, wxID_STATIC, _("Set scale:")), 0, wxALL, 0);

   wxChoice* choice3 = new wxChoice(panel, PREF_NEW_CURSOR,
                                    wxDefaultPosition, wxDefaultSize,
                                    newcursorChoices);

   wxChoice* choice1 = new wxChoice(panel, PREF_NEW_SCALE,
                                    #ifdef __WXX11__
                                    wxDefaultPosition, wxSize(60, wxDefaultCoord),
                                    #else
                                    wxDefaultPosition, wxDefaultSize,
                                    #endif
                                    newscaleChoices);
   
   wxBoxSizer* hbox3 = new wxBoxSizer( wxHORIZONTAL );
   hbox3->Add(setcursbox, 0, wxALIGN_CENTER_VERTICAL, 0);
   hbox3->Add(choice3, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, CHOICEGAP);
   hbox3->AddSpacer(20);
   hbox3->Add(setscalebox, 0, wxALIGN_CENTER_VERTICAL, 0);
   hbox3->Add(choice1, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, CHOICEGAP);
   #ifdef __WXX11__
      hbox3->AddSpacer(10);
   #endif

   ssizer1->AddSpacer(CVGAP);
   ssizer1->Add(hbox3, 0, wxLEFT | wxRIGHT, LRGAP);
   ssizer1->AddSpacer(SBBOTGAP);
   
   // on opening pattern
   
   vbox->AddSpacer(10);
   
   wxStaticBox* sbox2 = new wxStaticBox(panel, wxID_ANY, _("On opening a pattern file:"));
   wxBoxSizer* ssizer2 = new wxStaticBoxSizer( sbox2, wxVERTICAL );
   vbox->Add(ssizer2, 0, wxGROW | wxALL, 2);
   
   ssizer2->AddSpacer(SBTOPGAP);
   wxCheckBox* check2 = new wxCheckBox(panel, PREF_OPEN_REM_SEL,
                                       _("Remove selection"),
                                       wxDefaultPosition, wxDefaultSize);
   ssizer2->Add(check2, 0, wxLEFT | wxRIGHT, LRGAP);
   
   wxBoxSizer* hbox4 = new wxBoxSizer( wxHORIZONTAL );
   wxChoice* choice4 = new wxChoice(panel, PREF_OPEN_CURSOR,
                                    wxDefaultPosition, wxDefaultSize,
                                    opencursorChoices);
   hbox4->Add(new wxStaticText(panel, wxID_STATIC, _("Set cursor:")),
              0, wxALIGN_CENTER_VERTICAL, 0);
   hbox4->Add(choice4, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, CHOICEGAP);

   ssizer2->AddSpacer(CVGAP);
   ssizer2->Add(hbox4, 0, wxLEFT | wxRIGHT, LRGAP);
   ssizer2->AddSpacer(SBBOTGAP);
   
   // max_patterns and max_scripts
   
   vbox->AddSpacer(10);

   wxBoxSizer* maxbox = new wxBoxSizer( wxHORIZONTAL );
   maxbox->Add(new wxStaticText(panel, wxID_STATIC, _("Maximum number of recent patterns:")),
                                0, wxALL, 0);

   wxBoxSizer* minbox = new wxBoxSizer( wxHORIZONTAL );
   minbox->Add(new wxStaticText(panel, wxID_STATIC, _("Maximum number of recent scripts:")),
                                0, wxALL, 0);

   // align spin controls by setting minbox same width as maxbox
   minbox->SetMinSize( maxbox->GetMinSize() );

   wxSpinCtrl* spin1 = new MySpinCtrl(panel, PREF_MAX_PATTERNS, wxEmptyString,
                                      wxDefaultPosition, wxSize(70, wxDefaultCoord));
   
   wxSpinCtrl* spin2 = new MySpinCtrl(panel, PREF_MAX_SCRIPTS, wxEmptyString,
                                      wxDefaultPosition, wxSize(70, wxDefaultCoord));

   wxBoxSizer* hpbox = new wxBoxSizer( wxHORIZONTAL );
   hpbox->Add(maxbox, 0, wxALIGN_CENTER_VERTICAL, 0);
   hpbox->Add(spin1, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);

   wxBoxSizer* hsbox = new wxBoxSizer( wxHORIZONTAL );
   hsbox->Add(minbox, 0, wxALIGN_CENTER_VERTICAL, 0);
   hsbox->Add(spin2, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);

   vbox->Add(hpbox, 0, wxLEFT | wxRIGHT, LRGAP);
   vbox->AddSpacer(S2VGAP);
   vbox->Add(hsbox, 0, wxLEFT | wxRIGHT, LRGAP);

   #ifdef __WXX11__
      vbox->AddSpacer(15);
   #endif
     
   // init control values
   check1->SetValue(newremovesel);
   check2->SetValue(openremovesel);
   choice1->SetSelection(newmag);
   newcursindex = CursorToIndex(newcurs);
   opencursindex = CursorToIndex(opencurs);
   choice3->SetSelection(newcursindex);
   choice4->SetSelection(opencursindex);
   spin1->SetRange(1, MAX_RECENT); spin1->SetValue(maxpatterns);
   spin2->SetRange(1, MAX_RECENT); spin2->SetValue(maxscripts);
   
   topSizer->Add(vbox, 1, wxGROW | wxALIGN_CENTER | wxALL, 5);
   panel->SetSizer(topSizer);
   topSizer->Fit(panel);
   return panel;
}

// -----------------------------------------------------------------------------

wxPanel* PrefsDialog::CreateEditPrefs(wxWindow* parent)
{
   wxPanel* panel = new wxPanel(parent, wxID_ANY);
   wxBoxSizer* topSizer = new wxBoxSizer( wxVERTICAL );
   wxBoxSizer* vbox = new wxBoxSizer( wxVERTICAL );
   
   // random_fill

   wxBoxSizer* hbox1 = new wxBoxSizer( wxHORIZONTAL );
   hbox1->Add(new wxStaticText(panel, wxID_STATIC, _("Random fill percentage:")),
              0, wxALIGN_CENTER_VERTICAL, 0);
   wxSpinCtrl* spin1 = new MySpinCtrl(panel, PREF_RANDOM_FILL, wxEmptyString,
                                      wxDefaultPosition, wxSize(70, wxDefaultCoord));
   hbox1->Add(spin1, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);
   vbox->AddSpacer(SVGAP);
   vbox->Add(hbox1, 0, wxLEFT | wxRIGHT, LRGAP);
   
   // init control value
   spin1->SetRange(1, 100); spin1->SetValue(randomfill);
   
   topSizer->Add(vbox, 1, wxGROW | wxALIGN_CENTER | wxALL, 5);
   panel->SetSizer(topSizer);
   topSizer->Fit(panel);
   return panel;
}

// -----------------------------------------------------------------------------

wxPanel* PrefsDialog::CreateControlPrefs(wxWindow* parent)
{
   wxPanel* panel = new wxPanel(parent, wxID_ANY);
   wxBoxSizer* topSizer = new wxBoxSizer( wxVERTICAL );
   wxBoxSizer* vbox = new wxBoxSizer( wxVERTICAL );
   
   // max_hash_mem

   wxBoxSizer* hbox5 = new wxBoxSizer( wxHORIZONTAL );
   hbox5->Add(new wxStaticText(panel, wxID_STATIC, _("Maximum memory for hashing:")),
              0, wxALIGN_CENTER_VERTICAL, 0);
   wxSpinCtrl* spin5 = new MySpinCtrl(panel, PREF_MAX_HASH_MEM, wxEmptyString,
                                      wxDefaultPosition, wxSize(80, wxDefaultCoord));
   hbox5->Add(spin5, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);
   hbox5->Add(new wxStaticText(panel, wxID_STATIC, _("megabytes")),
              0, wxALIGN_CENTER_VERTICAL, 0);
   vbox->AddSpacer(SVGAP);
   vbox->Add(hbox5, 0, wxLEFT | wxRIGHT, LRGAP);
   
   // q_base_step and h_base_step

   vbox->AddSpacer(GROUPGAP);

   wxBoxSizer* longbox = new wxBoxSizer( wxHORIZONTAL );
   longbox->Add(new wxStaticText(panel, wxID_STATIC, _("Base step if not hashing:")),
                0, wxALL, 0);

   wxBoxSizer* shortbox = new wxBoxSizer( wxHORIZONTAL );
   shortbox->Add(new wxStaticText(panel, wxID_STATIC, _("Base step if hashing:")),
                 0, wxALL, 0);

   // align spin controls by setting shortbox same width as longbox
   shortbox->SetMinSize( longbox->GetMinSize() );

   wxBoxSizer* hbox1 = new wxBoxSizer( wxHORIZONTAL );
   hbox1->Add(longbox, 0, wxALIGN_CENTER_VERTICAL, 0);
   wxSpinCtrl* spin1 = new MySpinCtrl(panel, PREF_QBASE, wxEmptyString,
                                      wxDefaultPosition, wxSize(80, wxDefaultCoord));
   hbox1->Add(spin1, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);
   vbox->AddSpacer(SVGAP);
   vbox->Add(hbox1, 0, wxLEFT | wxRIGHT, LRGAP);

   wxBoxSizer* hbox2 = new wxBoxSizer( wxHORIZONTAL );
   hbox2->Add(shortbox, 0, wxALIGN_CENTER_VERTICAL, 0);
   wxSpinCtrl* spin2 = new MySpinCtrl(panel, PREF_HBASE, wxEmptyString,
                                      wxDefaultPosition, wxSize(80, wxDefaultCoord));
   hbox2->Add(spin2, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);
#ifdef __WXX11__
   hbox2->Add(new wxStaticText(panel, wxID_STATIC, _("(best if power of 2)  ")),
#else
   hbox2->Add(new wxStaticText(panel, wxID_STATIC, _("(best if power of 2)")),
#endif
              0, wxALIGN_CENTER_VERTICAL, 0);
   vbox->AddSpacer(S2VGAP);
   vbox->Add(hbox2, 0, wxLEFT | wxRIGHT, LRGAP);
   
   // min_delay and max_delay

   vbox->AddSpacer(GROUPGAP);

   wxBoxSizer* minbox = new wxBoxSizer( wxHORIZONTAL );
   minbox->Add(new wxStaticText(panel, wxID_STATIC, _("Minimum delay:")), 0, wxALL, 0);

   wxBoxSizer* maxbox = new wxBoxSizer( wxHORIZONTAL );
   maxbox->Add(new wxStaticText(panel, wxID_STATIC, _("Maximum delay:")), 0, wxALL, 0);

   // align spin controls by setting minbox same width as maxbox
   minbox->SetMinSize( maxbox->GetMinSize() );

   wxBoxSizer* hbox3 = new wxBoxSizer( wxHORIZONTAL );
   hbox3->Add(minbox, 0, wxALIGN_CENTER_VERTICAL, 0);
   wxSpinCtrl* spin3 = new MySpinCtrl(panel, PREF_MIN_DELAY, wxEmptyString,
                                      wxDefaultPosition, wxSize(80, wxDefaultCoord));
   hbox3->Add(spin3, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);
   hbox3->Add(new wxStaticText(panel, wxID_STATIC, _("millisecs")),
              0, wxALIGN_CENTER_VERTICAL, 0);
   vbox->AddSpacer(SVGAP);
   vbox->Add(hbox3, 0, wxLEFT | wxRIGHT, LRGAP);
   
   wxBoxSizer* hbox4 = new wxBoxSizer( wxHORIZONTAL );
   hbox4->Add(maxbox, 0, wxALIGN_CENTER_VERTICAL, 0);
   wxSpinCtrl* spin4 = new MySpinCtrl(panel, PREF_MAX_DELAY, wxEmptyString,
                                      wxDefaultPosition, wxSize(80, wxDefaultCoord));
   hbox4->Add(spin4, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);
   hbox4->Add(new wxStaticText(panel, wxID_STATIC, _("millisecs")),
              0, wxALIGN_CENTER_VERTICAL, 0);
   vbox->AddSpacer(S2VGAP);
   vbox->Add(hbox4, 0, wxLEFT | wxRIGHT, LRGAP);
   
   // init control values;
   // to avoid a wxGTK bug we use SetRange and SetValue rather than specifying
   // the min,max,init values in the wxSpinCtrl constructor
   spin1->SetRange(2, MAX_BASESTEP);        spin1->SetValue(qbasestep);
   spin2->SetRange(2, MAX_BASESTEP);        spin2->SetValue(hbasestep);
   spin3->SetRange(0, MAX_DELAY);           spin3->SetValue(mindelay);
   spin4->SetRange(0, MAX_DELAY);           spin4->SetValue(maxdelay);
   spin5->SetRange(MIN_HASHMB, MAX_HASHMB); spin5->SetValue(maxhashmem);
   
   topSizer->Add(vbox, 1, wxGROW | wxALIGN_CENTER | wxALL, 5);
   panel->SetSizer(topSizer);
   topSizer->Fit(panel);
   return panel;
}

// -----------------------------------------------------------------------------

wxPanel* PrefsDialog::CreateViewPrefs(wxWindow* parent)
{
   wxPanel* panel = new wxPanel(parent, wxID_ANY);
   wxBoxSizer* topSizer = new wxBoxSizer( wxVERTICAL );
   wxBoxSizer* vbox = new wxBoxSizer( wxVERTICAL );
   
   // show_tips
   
#if wxUSE_TOOLTIPS
   wxCheckBox* check3 = new wxCheckBox(panel, PREF_SHOW_TIPS,
                                       _("Show tool tips"),
                                       wxDefaultPosition, wxDefaultSize);
#endif
   
   // math_coords
   
   wxCheckBox* check1 = new wxCheckBox(panel, PREF_Y_UP,
                                       _("Y coordinates increase upwards"),
                                       wxDefaultPosition, wxDefaultSize);
   
   // show_bold_lines and bold_spacing
   
   wxBoxSizer* hbox2 = new wxBoxSizer( wxHORIZONTAL );
   wxCheckBox* check2 = new wxCheckBox(panel, PREF_SHOW_BOLD,
                                       _("Show bold grid lines every"),
                                       wxDefaultPosition, wxDefaultSize);
   
   wxSpinCtrl* spin2 = new MySpinCtrl(panel, PREF_BOLD_SPACING, wxEmptyString,
                                      wxDefaultPosition, wxSize(70, wxDefaultCoord));
   
   hbox2->Add(check2, 0, wxALIGN_CENTER_VERTICAL, 0);
   hbox2->Add(spin2, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);
   hbox2->Add(new wxStaticText(panel, wxID_STATIC, _("cells")),
              0, wxALIGN_CENTER_VERTICAL, 0);
   
   // min_grid_mag (2..MAX_MAG)

   wxBoxSizer* hbox3 = new wxBoxSizer( wxHORIZONTAL );
   
   wxArrayString mingridChoices;
   mingridChoices.Add(wxT("1:4"));
   mingridChoices.Add(wxT("1:8"));
   mingridChoices.Add(wxT("1:16"));
   wxChoice* choice3 = new wxChoice(panel, PREF_MIN_GRID_SCALE,
                                    #ifdef __WXX11__
                                    wxDefaultPosition, wxSize(60, wxDefaultCoord),
                                    #else
                                    wxDefaultPosition, wxDefaultSize,
                                    #endif
                                    mingridChoices);

   wxBoxSizer* longbox = new wxBoxSizer( wxHORIZONTAL );
   longbox->Add(new wxStaticText(panel, wxID_STATIC, _("Minimum scale for grid:")),
                0, wxALL, 0);

   wxBoxSizer* shortbox = new wxBoxSizer( wxHORIZONTAL );
   shortbox->Add(new wxStaticText(panel, wxID_STATIC, _("Mouse wheel action:")),
                 0, wxALL, 0);

   // align controls by setting shortbox same width as longbox
   shortbox->SetMinSize( longbox->GetMinSize() );
   
   hbox3->Add(longbox, 0, wxALIGN_CENTER_VERTICAL, 0);
   hbox3->Add(choice3, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, CHOICEGAP);

   // mouse_wheel_mode

   wxBoxSizer* hbox4 = new wxBoxSizer( wxHORIZONTAL );
   
   wxArrayString mousewheelChoices;
   mousewheelChoices.Add(wxT("Disabled"));
   mousewheelChoices.Add(wxT("Forward zooms out"));
   mousewheelChoices.Add(wxT("Forward zooms in"));
   wxChoice* choice4 = new wxChoice(panel, PREF_MOUSE_WHEEL,
                                    wxDefaultPosition, wxDefaultSize,
                                    mousewheelChoices);
   
   hbox4->Add(shortbox, 0, wxALIGN_CENTER_VERTICAL, 0);
   hbox4->Add(choice4, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, CHOICEGAP);

   // thumb_range

   wxBoxSizer* thumblabel = new wxBoxSizer( wxHORIZONTAL );
   thumblabel->Add(new wxStaticText(panel, wxID_STATIC, _("Thumb scroll range:")),
                   0, wxALL, 0);

   // align controls
   thumblabel->SetMinSize( longbox->GetMinSize() );

   wxBoxSizer* hbox5 = new wxBoxSizer( wxHORIZONTAL );
   hbox5->Add(thumblabel, 0, wxALIGN_CENTER_VERTICAL, 0);
   wxSpinCtrl* spin5 = new MySpinCtrl(panel, PREF_THUMB_RANGE, wxEmptyString,
                                      wxDefaultPosition, wxSize(70, wxDefaultCoord));
   hbox5->Add(spin5, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);
   hbox5->Add(new wxStaticText(panel, wxID_STATIC, _("times view size")),
              0, wxALIGN_CENTER_VERTICAL, 0);

   vbox->AddSpacer(5);
#if wxUSE_TOOLTIPS
   vbox->Add(check3, 0, wxLEFT | wxRIGHT, LRGAP);
   vbox->AddSpacer(CH2VGAP);
#endif
   vbox->Add(check1, 0, wxLEFT | wxRIGHT, LRGAP);
   vbox->AddSpacer(SVGAP);
   vbox->Add(hbox2, 0, wxLEFT | wxRIGHT, LRGAP);
   vbox->AddSpacer(SVGAP);
   vbox->Add(hbox3, 0, wxLEFT | wxRIGHT, LRGAP);
   vbox->AddSpacer(CVGAP);
   vbox->Add(hbox4, 0, wxLEFT | wxRIGHT, LRGAP);
   vbox->AddSpacer(SVGAP);
   vbox->Add(hbox5, 0, wxLEFT | wxRIGHT, LRGAP);

   // init control values
   check1->SetValue(mathcoords);
   check2->SetValue(showboldlines);
#if wxUSE_TOOLTIPS
   check3->SetValue(showtips);
#endif
   spin5->SetRange(2, MAX_THUMBRANGE); spin5->SetValue(thumbrange);
   spin2->SetRange(2, MAX_SPACING);    spin2->SetValue(boldspacing);
   spin2->Enable(showboldlines);
   mingridindex = mingridmag - 2;
   choice3->SetSelection(mingridindex);
   choice4->SetSelection(mousewheelmode);
   
   topSizer->Add(vbox, 1, wxGROW | wxALIGN_CENTER | wxALL, 5);
   panel->SetSizer(topSizer);
   topSizer->Fit(panel);
   return panel;
}

// -----------------------------------------------------------------------------

void PrefsDialog::AddColorButton(wxWindow* parent, wxBoxSizer* vbox,
                                 int id, wxColor* rgb, const wxString& text)
{
   wxBitmap bitmap(BITMAP_WD, BITMAP_HT);
   wxMemoryDC dc;
   dc.SelectObject(bitmap);
   wxRect rect(0, 0, BITMAP_WD, BITMAP_HT);
   wxBrush brush(*rgb);
   FillRect(dc, rect, brush);
   dc.SelectObject(wxNullBitmap);
   
   wxBitmapButton* bb = new wxBitmapButton(parent, id, bitmap, wxPoint(0, 0));
   if (bb == NULL) {
      Warning(_("Failed to create color button!"));
   } else {
      wxBoxSizer* hbox = new wxBoxSizer(wxHORIZONTAL);
      hbox->Add(bb, 0, wxALIGN_CENTER_VERTICAL, 0);
      hbox->Add(new wxStaticText(parent, wxID_STATIC, text),
                 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
      vbox->AddSpacer(5);
      vbox->Add(hbox, 0, wxLEFT | wxRIGHT, LRGAP);
   }
}

// -----------------------------------------------------------------------------

wxPanel* PrefsDialog::CreateColorPrefs(wxWindow* parent)
{
   wxPanel* panel = new wxPanel(parent, wxID_ANY);
   wxBoxSizer* topSizer = new wxBoxSizer( wxVERTICAL );
   wxBoxSizer* vbox = new wxBoxSizer( wxVERTICAL );
   
   AddColorButton(panel, vbox, PREF_LIVE_RGB, livergb, _("Live cells"));
   AddColorButton(panel, vbox, PREF_DEAD_RGB, deadrgb, _("Dead cells"));
   vbox->AddSpacer(GROUPGAP);
   AddColorButton(panel, vbox, PREF_PASTE_RGB, pastergb, _("Pasted pattern"));
   AddColorButton(panel, vbox, PREF_SELECT_RGB, selectrgb, _("Selection (will be 50% transparent)"));
   vbox->AddSpacer(GROUPGAP);
   AddColorButton(panel, vbox, PREF_QLIFE_RGB, qlifergb, _("Status bar background if not hashing"));
   AddColorButton(panel, vbox, PREF_HLIFE_RGB, hlifergb, _("Status bar background if hashing"));

   #ifdef __WXMAC__
      // wxMac bug: need this hidden control so escape/return keys select Cancel/OK buttons
      wxSpinCtrl* dummy = new MySpinCtrl(panel, wxID_ANY, wxEmptyString,
                                         wxPoint(-666,-666), wxDefaultSize);
      if (!dummy) Warning(_("Bug in CreateColorPrefs!"));
   #endif

   new_livergb = new wxColor(*livergb);
   new_deadrgb = new wxColor(*deadrgb);
   new_pastergb = new wxColor(*pastergb);
   new_selectrgb = new wxColor(*selectrgb);
   new_qlifergb = new wxColor(*qlifergb);
   new_hlifergb = new wxColor(*hlifergb);
   
   topSizer->Add(vbox, 1, wxGROW | wxALIGN_CENTER | wxALL, 5);
   panel->SetSizer(topSizer);
   topSizer->Fit(panel);
   return panel;
}

// -----------------------------------------------------------------------------

void PrefsDialog::OnCheckBoxClicked(wxCommandEvent& event)
{
   if ( event.GetId() == PREF_SHOW_BOLD ) {
      // enable/disable PREF_BOLD_SPACING spin control
      wxCheckBox* checkbox = (wxCheckBox*) FindWindow(PREF_SHOW_BOLD);
      wxSpinCtrl* spinctrl = (wxSpinCtrl*) FindWindow(PREF_BOLD_SPACING);
      if (checkbox && spinctrl) {
         bool ticked = checkbox->GetValue();
         spinctrl->Enable(ticked);
         if (ticked) spinctrl->SetFocus();
      }
   }
}

// -----------------------------------------------------------------------------

void PrefsDialog::ChangeColor(int id, wxColor* rgb)
{
#ifdef __WXX11__
   // avoid horrible wxX11 bugs
   Warning(_("Sorry, but due to wxX11 bugs you'll have to change colors\n"
             "by quitting Golly, editing GollyPrefs and restarting."));
#else
   wxColourData data;
   data.SetChooseFull(true);    // for Windows
   data.SetColour(*rgb);
   
   wxColourDialog dialog(this, &data);
   if ( dialog.ShowModal() == wxID_OK ) {
      wxColourData retData = dialog.GetColourData();
      wxColour c = retData.GetColour();
      
      if (*rgb != c) {
         // change given color
         rgb->Set(c.Red(), c.Green(), c.Blue());
         color_changed = true;
         
         // also change color of bitmap in corresponding button
         wxBitmapButton* bb = (wxBitmapButton*) FindWindow(id);
         if (bb) {
            wxBitmap bitmap(BITMAP_WD, BITMAP_HT);
            wxMemoryDC dc;
            dc.SelectObject(bitmap);
            wxRect rect(0, 0, BITMAP_WD, BITMAP_HT);
            wxBrush brush(*rgb);
            FillRect(dc, rect, brush);
            dc.SelectObject(wxNullBitmap);
   
            bb->SetBitmapLabel(bitmap);
            bb->Refresh();
            bb->Update();
         }
      }
   }
#endif
}

// -----------------------------------------------------------------------------

void PrefsDialog::OnColorButton(wxCommandEvent& event)
{
   if ( event.GetId() == PREF_LIVE_RGB ) {
      ChangeColor(PREF_LIVE_RGB, new_livergb);

   } else if ( event.GetId() == PREF_DEAD_RGB ) {
      ChangeColor(PREF_DEAD_RGB, new_deadrgb);

   } else if ( event.GetId() == PREF_PASTE_RGB ) {
      ChangeColor(PREF_PASTE_RGB, new_pastergb);

   } else if ( event.GetId() == PREF_SELECT_RGB ) {
      ChangeColor(PREF_SELECT_RGB, new_selectrgb);

   } else if ( event.GetId() == PREF_QLIFE_RGB ) {
      ChangeColor(PREF_QLIFE_RGB, new_qlifergb);

   } else if ( event.GetId() == PREF_HLIFE_RGB ) {
      ChangeColor(PREF_HLIFE_RGB, new_hlifergb);
   
   } else {
      // process other buttons like Cancel and OK
      event.Skip();
   }
}

// -----------------------------------------------------------------------------

bool PrefsDialog::GetCheckVal(long id)
{
   wxCheckBox* checkbox = (wxCheckBox*) FindWindow(id);
   if (checkbox) {
      return checkbox->GetValue();
   } else {
      Warning(_("Bug in GetCheckVal!"));
      return false;
   }
}

// -----------------------------------------------------------------------------

int PrefsDialog::GetChoiceVal(long id)
{
   wxChoice* choice = (wxChoice*) FindWindow(id);
   if (choice) {
      return choice->GetSelection();
   } else {
      Warning(_("Bug in GetChoiceVal!"));
      return 0;
   }
}

// -----------------------------------------------------------------------------

int PrefsDialog::GetSpinVal(long id)
{
   wxSpinCtrl* spinctrl = (wxSpinCtrl*) FindWindow(id);
   if (spinctrl) {
      return spinctrl->GetValue();
   } else {
      Warning(_("Bug in GetSpinVal!"));
      return 0;
   }
}

// -----------------------------------------------------------------------------

bool PrefsDialog::BadSpinVal(int id, int minval, int maxval, const wxString &prefix)
{
   wxSpinCtrl* spinctrl = (wxSpinCtrl*) FindWindow(id);
#if defined(__WXMSW__) || defined(__WXGTK__)
   // spinctrl->GetValue() always returns a value within range even if
   // the text ctrl doesn't contain a valid number -- yuk!!!
   int i = spinctrl->GetValue();
   if (i < minval || i > maxval) {
#else
   // GetTextValue returns FALSE if text ctrl doesn't contain a valid number
   // or the number is out of range, but it's not available in wxMSW or wxGTK
   int i;
   if ( !spinctrl->GetTextValue(&i) || i < minval || i > maxval ) {
#endif
      wxString msg;
      msg.Printf(_("%s must be from %d to %d."), prefix.c_str(), minval, maxval);
      Warning(msg);
      spinctrl->SetFocus();
      spinctrl->SetSelection(-1,-1);
      return true;
   } else {
      return false;
   }
}

// -----------------------------------------------------------------------------

bool PrefsDialog::ValidatePage()
{
   // validate all spin control values on current page
   if (currpage == FILE_PAGE) {
      if ( BadSpinVal(PREF_MAX_PATTERNS, 1, MAX_RECENT, _("Maximum number of recent patterns")) )
         return false;
      if ( BadSpinVal(PREF_MAX_SCRIPTS, 1, MAX_RECENT, _("Maximum number of recent scripts")) )
         return false;

   } else if (currpage == EDIT_PAGE) {
      if ( BadSpinVal(PREF_RANDOM_FILL, 1, 100, _("Random fill percentage")) )
         return false;

   } else if (currpage == CONTROL_PAGE) {
      if ( BadSpinVal(PREF_MAX_HASH_MEM, MIN_HASHMB, MAX_HASHMB, _("Maximum memory for hashing")) )
         return false;
      if ( BadSpinVal(PREF_QBASE, 2, MAX_BASESTEP, _("Base step if not hashing")) )
         return false;
      if ( BadSpinVal(PREF_HBASE, 2, MAX_BASESTEP, _("Base step if hashing")) )
         return false;
      if ( BadSpinVal(PREF_MIN_DELAY, 0, MAX_DELAY, _("Minimum delay")) )
         return false;
      if ( BadSpinVal(PREF_MAX_DELAY, 0, MAX_DELAY, _("Maximum delay")) )
         return false;

   } else if (currpage == VIEW_PAGE) {
      if ( BadSpinVal(PREF_BOLD_SPACING, 2, MAX_SPACING, _("Spacing of bold grid lines")) )
         return false;
      if ( BadSpinVal(PREF_THUMB_RANGE, 2, MAX_THUMBRANGE, _("Thumb scrolling range")) )
         return false;

   } else if (currpage == COLOR_PAGE) {
      // no spin controls on this page
   
   } else {
      Warning(_("Bug in ValidatePage!"));
      return false;
   }
   
   return true;
}

// -----------------------------------------------------------------------------

void PrefsDialog::OnPageChanging(wxNotebookEvent& event)
{
   if (ignore_page_event) return;
   // validate current page and veto change if invalid
   if (!ValidatePage()) event.Veto();
}

// -----------------------------------------------------------------------------

void PrefsDialog::OnPageChanged(wxNotebookEvent& event)
{
   if (ignore_page_event) return;
   currpage = event.GetSelection();
}

// -----------------------------------------------------------------------------

bool PrefsDialog::TransferDataFromWindow()
{
   if (!ValidatePage()) return false;
   
   // set global prefs to current control values

   // FILE_PAGE
   newremovesel  = GetCheckVal(PREF_NEW_REM_SEL);
   newcursindex  = GetChoiceVal(PREF_NEW_CURSOR);
   newmag        = GetChoiceVal(PREF_NEW_SCALE);
   openremovesel = GetCheckVal(PREF_OPEN_REM_SEL);
   opencursindex = GetChoiceVal(PREF_OPEN_CURSOR);
   maxpatterns   = GetSpinVal(PREF_MAX_PATTERNS);
   maxscripts    = GetSpinVal(PREF_MAX_SCRIPTS);

   // EDIT_PAGE
   randomfill = GetSpinVal(PREF_RANDOM_FILL);

   // CONTROL_PAGE
   maxhashmem = GetSpinVal(PREF_MAX_HASH_MEM);
   qbasestep  = GetSpinVal(PREF_QBASE);
   hbasestep  = GetSpinVal(PREF_HBASE);
   mindelay   = GetSpinVal(PREF_MIN_DELAY);
   maxdelay   = GetSpinVal(PREF_MAX_DELAY);

   // VIEW_PAGE
#if wxUSE_TOOLTIPS
   showtips       = GetCheckVal(PREF_SHOW_TIPS);
   wxToolTip::Enable(showtips);
#endif
   mathcoords     = GetCheckVal(PREF_Y_UP);
   showboldlines  = GetCheckVal(PREF_SHOW_BOLD);
   boldspacing    = GetSpinVal(PREF_BOLD_SPACING);
   mingridindex   = GetChoiceVal(PREF_MIN_GRID_SCALE);
   mousewheelmode = GetChoiceVal(PREF_MOUSE_WHEEL);
   thumbrange     = GetSpinVal(PREF_THUMB_RANGE);

   // COLOR_PAGE
   if (color_changed) {
      // strictly speaking we shouldn't need the color_changed flag but it
      // minimizes problems caused by bug in wxX11
      *livergb     = *new_livergb;
      *deadrgb     = *new_deadrgb;
      *pastergb    = *new_pastergb;
      *selectrgb   = *new_selectrgb;
      *qlifergb    = *new_qlifergb;
      *hlifergb    = *new_hlifergb;
   
      // update colors for brushes and pens
      SetBrushesAndPens();
   }

   // update globals corresponding to the wxChoice menu selections
   mingridmag = mingridindex + 2;
   newcurs = IndexToCursor(newcursindex);
   opencurs = IndexToCursor(opencursindex);
      
   return true;
}

// -----------------------------------------------------------------------------

PrefsDialog::~PrefsDialog()
{
   delete new_livergb;
   delete new_deadrgb;
   delete new_pastergb;
   delete new_selectrgb;
   delete new_qlifergb;
   delete new_hlifergb;
}

// -----------------------------------------------------------------------------

bool ChangePrefs()
{
   PrefsDialog dialog(mainptr);
   if (dialog.ShowModal() == wxID_OK) {
      // TransferDataFromWindow has validated and updated all global prefs
      return true;
   } else {
      return false;
   }
}
