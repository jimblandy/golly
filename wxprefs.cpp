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

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
   #include "wx/wx.h"      // for all others include the necessary headers
#endif

#include "wx/filename.h"   // for wxFileName
#include "wx/stdpaths.h"   // for wxStandardPaths
#include "wx/propdlg.h"    // for wxPropertySheetDialog
#include "wx/bookctrl.h"   // for wxBookCtrlBase
#include "wx/notebook.h"   // for wxNotebookEvent
#include "wx/spinctrl.h"   // for wxSpinCtrl
#include "wx/image.h"      // for wxImage

#include "lifealgo.h"      // for curralgo->...
#include "viewport.h"      // for MAX_MAG

#include "wxgolly.h"       // for wxGetApp, mainptr
#include "wxmain.h"        // for GetID_RECENT_CLEAR, GetID_RECENT, mainptr->...
#include "wxutils.h"       // for Warning
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
const char PATT_DIR[] = "Patterns";

const int PREFS_VERSION = 1;     // may change if file syntax changes
const int PREF_LINE_SIZE = 5000; // must be quite long for storing file paths
const int MAX_SPACING = 1000;    // maximum value of boldspacing
const int MIN_HASHMB = 10;       // minimum value of maxhashmem
const int MAX_HASHMB = 4000;     // make bigger when hlifealgo is 64-bit clean
const int MAX_BASESTEP = 100;    // maximum qbasestep or hbasestep
const int MAX_DELAY = 5000;      // maximum mindelay or maxdelay
const int MAX_THUMBRANGE = 500;  // maximum thumbrange
const int MIN_PATTDIRWD = 50;    // minimum pattdirwd

// initialize exported preferences:

int mainx = 30;                  // main window's initial location
int mainy = 30;
int mainwd = 640;                // main window's initial size
int mainht = 480;
bool maximize = true;            // maximize main window?

int helpx = 60;                  // help window's initial location
int helpy = 60;
int helpwd = 600;                // help window's initial size
int helpht = 400;
#ifdef __WXMSW__
   int helpfontsize = 10;        // font size in help window
#else
   int helpfontsize = 12;        // font size in help window
#endif

int infox = 100;                 // info window's initial location
int infoy = 100;
int infowd = 600;                // info window's initial size
int infoht = 400;

bool autofit = false;            // auto fit pattern while generating?
bool hashing = false;            // use hlife algorithm?
bool hyperspeed = false;         // use hyperspeed if supported by current algo?
bool blackcells = true;          // live cells are black?
bool showgridlines = true;       // display grid lines?
bool buffered = true;            // use wxWdgets buffering to avoid flicker?
bool showstatus = true;          // show status bar?
bool showtool = true;            // show tool bar?
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
wxString opensavedir;            // directory for open and save dialogs
wxString patterndir;             // directory used by Show Patterns
int pattdirwd = 180;             // width of pattern directory window
bool showpatterns = true;        // show pattern directory?
wxMenu *recentSubMenu = NULL;    // menu of recent files
int numrecent = 0;               // current number of recent files
int maxrecent = 20;              // maximum number of recent files (1..MAX_RECENT)
wxArrayString namedrules;        // initialized in GetPrefs

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

int mingridindex;                // mingridmag - 2
int newcursindex;
int opencursindex;

wxString appdir;                 // path of directory containing app

// -----------------------------------------------------------------------------

void CreateCursors()
{
   curs_pencil = new wxCursor(wxCURSOR_PENCIL);
   if (curs_pencil == NULL) Fatal("Failed to create pencil cursor!");

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

void SavePrefs()
{
   if (mainptr == NULL || curralgo == NULL) {
      // should never happen but play safe
      return;
   }
   
   FILE *f = fopen(PREFS_NAME, "w");
   if (f == NULL) {
      Warning("Could not save preferences file!");
      return;
   }
   
   fprintf(f, "# NOTE: If you edit this file then do so when Golly isn't running\n");
   fprintf(f, "# otherwise all your changes will be clobbered when Golly quits.\n");
   fprintf(f, "version=%d\n", PREFS_VERSION);
   // save main window's location and size
   if (mainptr->fullscreen) {
      // use mainx, mainy, mainwd, mainht set by mainptr->ToggleFullScreen()
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
   if (GetHelpFrame()) {
      wxRect r = GetHelpFrame()->GetRect();
      helpx = r.x;
      helpy = r.y;
      helpwd = r.width;
      helpht = r.height;
   }
   fprintf(f, "help_window=%d,%d,%d,%d\n", helpx, helpy, helpwd, helpht);
   fprintf(f, "help_font_size=%d (%d..%d)\n", helpfontsize, minfontsize, maxfontsize);
   if (GetInfoFrame()) {
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
   fprintf(f, "max_hash_mem=%d\n", maxhashmem);
   fprintf(f, "rule=%s\n", curralgo->getrule());
   if (namedrules.GetCount() > 1) {
      size_t i;
      for (i=1; i<namedrules.GetCount(); i++)
         fprintf(f, "named_rule=%s\n", namedrules[i].c_str());
   }
   fprintf(f, "show_status=%d\n", mainptr->StatusVisible() ? 1 : 0);
   fprintf(f, "show_tool=%d\n", mainptr->GetToolBar()->IsShown() ? 1 : 0);
   fprintf(f, "grid_lines=%d\n", showgridlines ? 1 : 0);
   fprintf(f, "min_grid_mag=%d (2..%d)\n", mingridmag, MAX_MAG);
   fprintf(f, "bold_spacing=%d (2..%d)\n", boldspacing, MAX_SPACING);
   fprintf(f, "show_bold_lines=%d\n", showboldlines ? 1 : 0);
   fprintf(f, "math_coords=%d\n", mathcoords ? 1 : 0);
   fprintf(f, "black_on_white=%d\n", blackcells ? 1 : 0);
   fprintf(f, "buffered=%d\n", buffered ? 1 : 0);
   fprintf(f, "mouse_wheel_mode=%d\n", mousewheelmode);
   fprintf(f, "thumb_range=%d (2..%d)\n", thumbrange, MAX_THUMBRANGE);
   fprintf(f, "new_mag=%d (0..%d)\n", newmag, MAX_MAG);
   fprintf(f, "new_remove_sel=%d\n", newremovesel ? 1 : 0);
   fprintf(f, "new_cursor=%s\n", CursorToString(newcurs));
   fprintf(f, "open_remove_sel=%d\n", openremovesel ? 1 : 0);
   fprintf(f, "open_cursor=%s\n", CursorToString(opencurs));
   fprintf(f, "open_save_dir=%s\n", opensavedir.c_str());
   fprintf(f, "pattern_dir=%s\n", patterndir.c_str());
   fprintf(f, "patt_dir_width=%d\n", pattdirwd);
   fprintf(f, "show_patterns=%d\n", showpatterns ? 1 : 0);
   fprintf(f, "max_recent=%d (1..%d)\n", maxrecent, MAX_RECENT);
   if (numrecent > 0) {
      int i;
      for (i=0; i<numrecent; i++) {
         wxMenuItem *item = recentSubMenu->FindItemByPosition(i);
         if (item) fprintf(f, "recent_file=%s\n", item->GetText().c_str());
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
         if (argv0.StartsWith("./")) argv0 = argv0.AfterFirst('/');
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
   namedrules.Add("3-4 Life|B34/S34");
   namedrules.Add("HighLife|B36/S23");
   namedrules.Add("AntiLife|B0123478/S01234678");
   namedrules.Add("Life without Death|B3/S012345678");
   namedrules.Add("Plow World|B378/S012345678");
   namedrules.Add("Day and Night|B3678/S34678");
   namedrules.Add("Diamoeba|B35678/S5678");
   namedrules.Add("LongLife|B345/S5");
   namedrules.Add("Seeds|B2");
   namedrules.Add("Persian Rug|B234");
   namedrules.Add("Replicator|B1357/S1357");
   namedrules.Add("Fredkin|B1357/S02468");
   namedrules.Add("Morley|B368/S245");
   namedrules.Add("Wolfram 22|W22");
   namedrules.Add("Wolfram 30|W30");
   namedrules.Add("Wolfram 110|W110");
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
   patterndir = appdir + PATT_DIR;

   // create curs_* and initialize newcurs, opencurs and currcurs
   CreateCursors();
   
   // initialize Open Recent submenu
   recentSubMenu = new wxMenu();
   recentSubMenu->AppendSeparator();
   recentSubMenu->Append(GetID_RECENT_CLEAR(), _("Clear Menu"));

   namedrules.Add("Life|B3/S23");      // must be 1st entry

   if ( !wxFileExists(PREFS_NAME) ) {
      // use initial preference values
      AddDefaultRules();
      return;
   }
   
   FILE *f = fopen(PREFS_NAME, "r");
   if (f == NULL) {
      Warning("Could not read preferences file!");
      return;
   }
   
   char line[PREF_LINE_SIZE];
   char *keyword;
   char *value;
   while ( GetKeyVal(f, line, &keyword, &value) ) {
      // remove \n from end of value
      int len = strlen(value);
      if ( len > 0 && value[len-1] == '\n' ) {
         value[len-1] = 0;
      }

      if (strcmp(keyword, "version") == 0) {
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

      } else if (strcmp(keyword, "max_hash_mem") == 0) {
         sscanf(value, "%d", &maxhashmem);
         if (maxhashmem < MIN_HASHMB) maxhashmem = MIN_HASHMB;
         if (maxhashmem > MAX_HASHMB) maxhashmem = MAX_HASHMB;

      } else if (strcmp(keyword, "rule") == 0) {
         strncpy(initrule, value, sizeof(initrule));

      } else if (strcmp(keyword, "named_rule") == 0) {
         namedrules.Add(value);

      } else if (strcmp(keyword, "show_status") == 0) {
         showstatus = value[0] == '1';

      } else if (strcmp(keyword, "show_tool") == 0) {
         showtool = value[0] == '1';

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

      } else if (strcmp(keyword, "black_on_white") == 0) {
         blackcells = value[0] == '1';

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
         opensavedir = value;
         if ( !wxFileName::DirExists(opensavedir) ) {
            // reset to supplied pattern directory
            opensavedir = appdir + PATT_DIR;
         }

      } else if (strcmp(keyword, "pattern_dir") == 0) {
         patterndir = value;
         if ( !wxFileName::DirExists(patterndir) ) {
            // reset to supplied pattern directory
            patterndir = appdir + PATT_DIR;
         }
         
      } else if (strcmp(keyword, "patt_dir_width") == 0) {
         sscanf(value, "%d", &pattdirwd);
         if (pattdirwd < MIN_PATTDIRWD) pattdirwd = MIN_PATTDIRWD;
         
      } else if (strcmp(keyword, "show_patterns") == 0) {
         showpatterns = value[0] == '1';

      } else if (strcmp(keyword, "max_recent") == 0) {
         sscanf(value, "%d", &maxrecent);
         if (maxrecent < 1) maxrecent = 1;
         if (maxrecent > MAX_RECENT) maxrecent = MAX_RECENT;

      } else if (strcmp(keyword, "recent_file") == 0) {
         // append path to Open Recent submenu
         if (numrecent < maxrecent) {
            numrecent++;
            recentSubMenu->Insert(numrecent - 1, GetID_RECENT() + numrecent, value);
         }
      }
   }
   fclose(f);
   
   // if no named_rule entries then add default names
   if (namedrules.GetCount() == 1) AddDefaultRules();
}

// -----------------------------------------------------------------------------

// define a multi-page dialog for changing various preferences

size_t prefspage = 0;      // current page in PrefsDialog
bool ignore_page_event;    // used to prevent prefspage being changed

class PrefsDialog : public wxPropertySheetDialog
{
public:
   PrefsDialog(wxWindow* parent);
   virtual bool TransferDataFromWindow();    // called when user hits OK

private:
   enum {
      // these *_PAGE values must correspond to prefspage values
      FILE_PAGE = 0,
      EDIT_PAGE,
      CONTROL_PAGE,
      VIEW_PAGE,
      // File prefs
      PREF_NEW_REM_SEL,
      PREF_NEW_CURSOR,
      PREF_NEW_SCALE,
      PREF_OPEN_REM_SEL,
      PREF_OPEN_CURSOR,
      PREF_MAX_RECENT,
      // Edit prefs
      PREF_RANDOM_FILL,
      // Control prefs
      PREF_MAX_HASH_MEM,
      PREF_QBASE,
      PREF_HBASE,
      PREF_MIN_DELAY,
      PREF_MAX_DELAY,
      // View prefs
      PREF_Y_UP,
      PREF_SHOW_BOLD,
      PREF_BOLD_SPACING,
      PREF_MIN_GRID_SCALE,
      PREF_MOUSE_WHEEL,
      PREF_THUMB_RANGE
   };

   wxPanel* CreateFilePrefs(wxWindow* parent);
   wxPanel* CreateEditPrefs(wxWindow* parent);
   wxPanel* CreateControlPrefs(wxWindow* parent);
   wxPanel* CreateViewPrefs(wxWindow* parent);

   bool GetCheckVal(long id);
   int GetChoiceVal(long id);
   int GetSpinVal(long id);
   bool BadSpinVal(int id, int minval, int maxval, const char *prefix);
   bool ValidateCurrentPage();
   
   void OnCheckBoxClicked(wxCommandEvent& event);
   void OnPageChanging(wxNotebookEvent& event);
   void OnPageChanged(wxNotebookEvent& event);

   DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(PrefsDialog, wxPropertySheetDialog)
   EVT_CHECKBOX               (wxID_ANY, PrefsDialog::OnCheckBoxClicked)
   EVT_NOTEBOOK_PAGE_CHANGING (wxID_ANY, PrefsDialog::OnPageChanging)
   EVT_NOTEBOOK_PAGE_CHANGED  (wxID_ANY, PrefsDialog::OnPageChanged)
END_EVENT_TABLE()

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
   
   // AddPage and SetSelection cause OnPageChanging and OnPageChanged to be called
   // so we use a flag to prevent prefspage being changed (and unnecessary validation)
   ignore_page_event = true;

   notebook->AddPage(filePrefs, _("File"));
   notebook->AddPage(editPrefs, _("Edit"));
   notebook->AddPage(ctrlPrefs, _("Control"));
   notebook->AddPage(viewPrefs, _("View"));
   
   // show last selected page
   notebook->SetSelection(prefspage);

   ignore_page_event = false;

   #ifdef __WXMAC__
      // give focus to first edit box on each page; also allows use of escape
      if (prefspage == FILE_PAGE) FindWindow(PREF_MAX_RECENT)->SetFocus();
      if (prefspage == EDIT_PAGE) FindWindow(PREF_RANDOM_FILL)->SetFocus();
      if (prefspage == CONTROL_PAGE) FindWindow(PREF_MAX_HASH_MEM)->SetFocus();
      if (prefspage == VIEW_PAGE)
         if (showboldlines)
            FindWindow(PREF_BOLD_SPACING)->SetFocus();
         else
            FindWindow(PREF_THUMB_RANGE)->SetFocus();
      // deselect other spin controls on CONTROL_PAGE
      wxSpinCtrl* sp;
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
   #define CVGAP (7)
   #define LRGAP (5)
   #define SPINGAP (6)
   #define CHOICEGAP (6)
#endif

// -----------------------------------------------------------------------------

wxPanel* PrefsDialog::CreateFilePrefs(wxWindow* parent)
{
   wxPanel* panel = new wxPanel(parent, wxID_ANY);
   wxBoxSizer *topSizer = new wxBoxSizer( wxVERTICAL );
   wxBoxSizer *vbox = new wxBoxSizer( wxVERTICAL );

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
   
   wxStaticBox* sbox1 = new wxStaticBox(panel, wxID_ANY, _("On new pattern:"));
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

   // nicer if setscalebox is same width as setcursbox
   setscalebox->SetMinSize( setcursbox->GetMinSize() );
   
   wxBoxSizer* hbox3 = new wxBoxSizer( wxHORIZONTAL );
   wxChoice* choice3 = new wxChoice(panel, PREF_NEW_CURSOR,
                                    wxDefaultPosition, wxDefaultSize,
                                    newcursorChoices);
   hbox3->Add(setcursbox, 0, wxALIGN_CENTER_VERTICAL, 0);
   hbox3->Add(choice3, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, CHOICEGAP);
   ssizer1->AddSpacer(CVGAP);
   ssizer1->Add(hbox3, 0, wxLEFT | wxRIGHT, LRGAP);
   
   wxBoxSizer* hbox1 = new wxBoxSizer( wxHORIZONTAL );
   wxChoice* choice1 = new wxChoice(panel, PREF_NEW_SCALE,
                                    wxDefaultPosition, wxDefaultSize,
                                    newscaleChoices);
   hbox1->Add(setscalebox, 0, wxALIGN_CENTER_VERTICAL, 0);
   hbox1->Add(choice1, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, CHOICEGAP);
   ssizer1->AddSpacer(CVGAP);
   ssizer1->Add(hbox1, 0, wxLEFT | wxRIGHT, LRGAP);
   ssizer1->AddSpacer(SBBOTGAP);
   
   // on opening pattern
   
   vbox->AddSpacer(5);
   
   wxStaticBox* sbox2 = new wxStaticBox(panel, wxID_ANY, _("On opening pattern:"));
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
   
   wxBoxSizer* hbox2 = new wxBoxSizer( wxHORIZONTAL );
   hbox2->Add(new wxStaticText(panel, wxID_STATIC, _("Maximum number of recent files:")),
              0, wxALIGN_CENTER_VERTICAL, 0);
   wxSpinCtrl* spin2 = new wxSpinCtrl(panel, PREF_MAX_RECENT, wxEmptyString,
                                      wxDefaultPosition, wxSize(70, wxDefaultCoord),
                                      wxSP_ARROW_KEYS, 1, MAX_RECENT, maxrecent);
   hbox2->Add(spin2, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);
   ssizer2->AddSpacer(SVGAP);
   ssizer2->Add(hbox2, 0, wxLEFT | wxRIGHT, LRGAP);
   ssizer2->AddSpacer(SBBOTGAP);

   #ifdef __WXX11__
      vbox->AddSpacer(15);
   #endif
     
   // init control values
   check1->SetValue(newremovesel);
   check2->SetValue(openremovesel);
   spin2->SetValue(maxrecent);
   choice1->SetSelection(newmag);
   newcursindex = CursorToIndex(newcurs);
   opencursindex = CursorToIndex(opencurs);
   choice3->SetSelection(newcursindex);
   choice4->SetSelection(opencursindex);
   
   topSizer->Add(vbox, 1, wxGROW | wxALIGN_CENTER | wxALL, 5);
   panel->SetSizer(topSizer);
   topSizer->Fit(panel);
   return panel;
}

// -----------------------------------------------------------------------------

wxPanel* PrefsDialog::CreateEditPrefs(wxWindow* parent)
{
   wxPanel* panel = new wxPanel(parent, wxID_ANY);
   wxBoxSizer *topSizer = new wxBoxSizer( wxVERTICAL );
   wxBoxSizer *vbox = new wxBoxSizer( wxVERTICAL );
   
   // random_fill

   wxBoxSizer* hbox1 = new wxBoxSizer( wxHORIZONTAL );
   hbox1->Add(new wxStaticText(panel, wxID_STATIC, _("Random fill percentage:")),
              0, wxALIGN_CENTER_VERTICAL, 0);
   wxSpinCtrl* spin1 = new wxSpinCtrl(panel, PREF_RANDOM_FILL, wxEmptyString,
                                      wxDefaultPosition, wxSize(70, wxDefaultCoord),
                                      wxSP_ARROW_KEYS, 1, 100, randomfill);
   hbox1->Add(spin1, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);
   vbox->AddSpacer(SVGAP);
   vbox->Add(hbox1, 0, wxLEFT | wxRIGHT, LRGAP);
   
   // init control value
   spin1->SetValue(randomfill);
   
   topSizer->Add(vbox, 1, wxGROW | wxALIGN_CENTER | wxALL, 5);
   panel->SetSizer(topSizer);
   topSizer->Fit(panel);
   return panel;
}

// -----------------------------------------------------------------------------

wxPanel* PrefsDialog::CreateControlPrefs(wxWindow* parent)
{
   wxPanel* panel = new wxPanel(parent, wxID_ANY);
   wxBoxSizer *topSizer = new wxBoxSizer( wxVERTICAL );
   wxBoxSizer *vbox = new wxBoxSizer( wxVERTICAL );
   
   // max_hash_mem

   wxBoxSizer* hbox5 = new wxBoxSizer( wxHORIZONTAL );
   hbox5->Add(new wxStaticText(panel, wxID_STATIC, _("Maximum memory for hashing:")),
              0, wxALIGN_CENTER_VERTICAL, 0);
   wxSpinCtrl* spin5 = new wxSpinCtrl(panel, PREF_MAX_HASH_MEM, wxEmptyString,
                                      wxDefaultPosition, wxSize(70, wxDefaultCoord),
                                      wxSP_ARROW_KEYS, MIN_HASHMB, MAX_HASHMB, maxhashmem);
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
   wxSpinCtrl* spin1 = new wxSpinCtrl(panel, PREF_QBASE, wxEmptyString,
                                      wxDefaultPosition, wxSize(70, wxDefaultCoord),
                                      wxSP_ARROW_KEYS, 2, MAX_BASESTEP, qbasestep);
   hbox1->Add(spin1, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);
   vbox->AddSpacer(SVGAP);
   vbox->Add(hbox1, 0, wxLEFT | wxRIGHT, LRGAP);

   wxBoxSizer* hbox2 = new wxBoxSizer( wxHORIZONTAL );
   hbox2->Add(shortbox, 0, wxALIGN_CENTER_VERTICAL, 0);
   wxSpinCtrl* spin2 = new wxSpinCtrl(panel, PREF_HBASE, wxEmptyString,
                                      wxDefaultPosition, wxSize(70, wxDefaultCoord),
                                      wxSP_ARROW_KEYS, 2, MAX_BASESTEP, hbasestep);
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
   wxSpinCtrl* spin3 = new wxSpinCtrl(panel, PREF_MIN_DELAY, wxEmptyString,
                                      wxDefaultPosition, wxSize(70, wxDefaultCoord),
                                      wxSP_ARROW_KEYS, 0, MAX_DELAY, mindelay);
   hbox3->Add(spin3, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);
   hbox3->Add(new wxStaticText(panel, wxID_STATIC, _("millisecs")),
              0, wxALIGN_CENTER_VERTICAL, 0);
   vbox->AddSpacer(SVGAP);
   vbox->Add(hbox3, 0, wxLEFT | wxRIGHT, LRGAP);
   
   wxBoxSizer* hbox4 = new wxBoxSizer( wxHORIZONTAL );
   hbox4->Add(maxbox, 0, wxALIGN_CENTER_VERTICAL, 0);
   wxSpinCtrl* spin4 = new wxSpinCtrl(panel, PREF_MAX_DELAY, wxEmptyString,
                                      wxDefaultPosition, wxSize(70, wxDefaultCoord),
                                      wxSP_ARROW_KEYS, 0, MAX_DELAY, maxdelay);
   hbox4->Add(spin4, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);
   hbox4->Add(new wxStaticText(panel, wxID_STATIC, _("millisecs")),
              0, wxALIGN_CENTER_VERTICAL, 0);
   vbox->AddSpacer(S2VGAP);
   vbox->Add(hbox4, 0, wxLEFT | wxRIGHT, LRGAP);
   
   // init control values
   spin1->SetValue(qbasestep);
   spin2->SetValue(hbasestep);
   spin3->SetValue(mindelay);
   spin4->SetValue(maxdelay);
   spin5->SetValue(maxhashmem);
   
   topSizer->Add(vbox, 1, wxGROW | wxALIGN_CENTER | wxALL, 5);
   panel->SetSizer(topSizer);
   topSizer->Fit(panel);
   return panel;
}

// -----------------------------------------------------------------------------

wxPanel* PrefsDialog::CreateViewPrefs(wxWindow* parent)
{
   wxPanel* panel = new wxPanel(parent, wxID_ANY);
   wxBoxSizer *topSizer = new wxBoxSizer( wxVERTICAL );
   wxBoxSizer *vbox = new wxBoxSizer( wxVERTICAL );
   
   // math_coords
   
   vbox->AddSpacer(5);
   wxCheckBox* check1 = new wxCheckBox(panel, PREF_Y_UP,
                                       _("Y coordinates increase upwards"),
                                       wxDefaultPosition, wxDefaultSize);
   vbox->Add(check1, 0, wxLEFT | wxRIGHT, LRGAP);
   
   // show_bold_lines and bold_spacing
   
   wxBoxSizer* hbox2 = new wxBoxSizer( wxHORIZONTAL );
   wxCheckBox* check2 = new wxCheckBox(panel, PREF_SHOW_BOLD,
                                       _("Show bold grid lines every"),
                                       wxDefaultPosition, wxDefaultSize);
   
   wxSpinCtrl* spin2 = new wxSpinCtrl(panel, PREF_BOLD_SPACING, wxEmptyString,
                                      wxDefaultPosition, wxSize(70, wxDefaultCoord),
                                      wxSP_ARROW_KEYS, 2, MAX_SPACING, boldspacing);
   
   hbox2->Add(check2, 0, wxALIGN_CENTER_VERTICAL, 0);
   hbox2->Add(spin2, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);
   hbox2->Add(new wxStaticText(panel, wxID_STATIC, _("cells")),
              0, wxALIGN_CENTER_VERTICAL, 0);
   vbox->AddSpacer(SVGAP);
   vbox->Add(hbox2, 0, wxLEFT | wxRIGHT, LRGAP);
   
   // min_grid_mag (2..MAX_MAG)

   wxBoxSizer* hbox3 = new wxBoxSizer( wxHORIZONTAL );
   
   wxArrayString mingridChoices;
   mingridChoices.Add(wxT("1:4"));
   mingridChoices.Add(wxT("1:8"));
   mingridChoices.Add(wxT("1:16"));
   wxChoice* choice3 = new wxChoice(panel, PREF_MIN_GRID_SCALE,
                                    wxDefaultPosition, wxDefaultSize,
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
   vbox->AddSpacer(SVGAP);
   vbox->Add(hbox3, 0, wxLEFT | wxRIGHT, LRGAP);

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
   vbox->AddSpacer(CVGAP);
   vbox->Add(hbox4, 0, wxLEFT | wxRIGHT, LRGAP);

   // thumb_range

   wxBoxSizer* thumblabel = new wxBoxSizer( wxHORIZONTAL );
   thumblabel->Add(new wxStaticText(panel, wxID_STATIC, _("Thumb scroll range:")),
                   0, wxALL, 0);

   // align controls
   thumblabel->SetMinSize( longbox->GetMinSize() );

   wxBoxSizer* hbox5 = new wxBoxSizer( wxHORIZONTAL );
   hbox5->Add(thumblabel, 0, wxALIGN_CENTER_VERTICAL, 0);
   wxSpinCtrl* spin5 = new wxSpinCtrl(panel, PREF_THUMB_RANGE, wxEmptyString,
                                      wxDefaultPosition, wxSize(70, wxDefaultCoord),
                                      wxSP_ARROW_KEYS, 2, MAX_THUMBRANGE, thumbrange);
   hbox5->Add(spin5, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, SPINGAP);
   hbox5->Add(new wxStaticText(panel, wxID_STATIC, _("times view size")),
              0, wxALIGN_CENTER_VERTICAL, 0);
   vbox->AddSpacer(SVGAP);
   vbox->Add(hbox5, 0, wxLEFT | wxRIGHT, LRGAP);


   // init control values
   check1->SetValue(mathcoords);
   check2->SetValue(showboldlines);
   spin2->SetValue(boldspacing);
   spin2->Enable(showboldlines);
   mingridindex = mingridmag - 2;
   choice3->SetSelection(mingridindex);
   choice4->SetSelection(mousewheelmode);
   spin5->SetValue(thumbrange);
   
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

bool PrefsDialog::GetCheckVal(long id)
{
   wxCheckBox* checkbox = (wxCheckBox*) FindWindow(id);
   if (checkbox) {
      return checkbox->GetValue();
   } else {
      Warning("Bug in GetCheckVal!");
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
      Warning("Bug in GetChoiceVal!");
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
      Warning("Bug in GetSpinVal!");
      return 0;
   }
}

// -----------------------------------------------------------------------------

bool PrefsDialog::BadSpinVal(int id, int minval, int maxval, const char *prefix)
{
   wxSpinCtrl* spinctrl = (wxSpinCtrl*) FindWindow(id);
#ifdef __WXMSW__
   // spinctrl->GetValue() always returns a value within range even if
   // the text ctrl doesn't contain a valid number -- yuk!!!
   int i = spinctrl->GetValue();
   if (i < minval || i > maxval) {
#else
   // GetTextValue returns FALSE if text ctrl doesn't contain a valid number
   // or the number is out of range, but it's not available in wxMSW
   int i;
   if ( !spinctrl->GetTextValue(&i) || i < minval || i > maxval ) {
#endif
      wxString msg;
      msg.Printf("%s must be from %d to %d.", prefix, minval, maxval);
      Warning(msg);
      spinctrl->SetFocus();
      spinctrl->SetSelection(-1,-1);
      return true;
   } else {
      return false;
   }
}

// -----------------------------------------------------------------------------

bool PrefsDialog::ValidateCurrentPage()
{
   // validate all spin control values on current page
   if (prefspage == FILE_PAGE) {
      if ( BadSpinVal(PREF_MAX_RECENT, 1, MAX_RECENT, "Maximum number of recent files") )
         return false;

   } else if (prefspage == EDIT_PAGE) {
      if ( BadSpinVal(PREF_RANDOM_FILL, 1, 100, "Random fill percentage") )
         return false;

   } else if (prefspage == CONTROL_PAGE) {
      if ( BadSpinVal(PREF_MAX_HASH_MEM, MIN_HASHMB, MAX_HASHMB, "Maximum memory for hashing") )
         return false;
      if ( BadSpinVal(PREF_QBASE, 2, MAX_BASESTEP, "Base step if not hashing") )
         return false;
      if ( BadSpinVal(PREF_HBASE, 2, MAX_BASESTEP, "Base step if hashing") )
         return false;
      if ( BadSpinVal(PREF_MIN_DELAY, 0, MAX_DELAY, "Minimum delay") )
         return false;
      if ( BadSpinVal(PREF_MAX_DELAY, 0, MAX_DELAY, "Maximum delay") )
         return false;

   } else if (prefspage == VIEW_PAGE) {
      if ( BadSpinVal(PREF_BOLD_SPACING, 2, MAX_SPACING, "Spacing of bold grid lines") )
         return false;
      if ( BadSpinVal(PREF_THUMB_RANGE, 2, MAX_THUMBRANGE, "Thumb scrolling range") )
         return false;
   
   } else {
      Warning("Bug in ValidateCurrentPage!");
      return false;
   }
   
   return true;
}

// -----------------------------------------------------------------------------

void PrefsDialog::OnPageChanging(wxNotebookEvent& event)
{
   if (ignore_page_event) return;
   // validate current page and veto change if invalid
   if (!ValidateCurrentPage()) event.Veto();
}

// -----------------------------------------------------------------------------

void PrefsDialog::OnPageChanged(wxNotebookEvent& event)
{
   if (ignore_page_event) return;
   prefspage = event.GetSelection();
}

// -----------------------------------------------------------------------------

bool PrefsDialog::TransferDataFromWindow()
{
   if (!ValidateCurrentPage()) return false;
   
   // set global prefs to current control values

   // FILE_PAGE
   newremovesel  = GetCheckVal(PREF_NEW_REM_SEL);
   newcursindex  = GetChoiceVal(PREF_NEW_CURSOR);
   newmag        = GetChoiceVal(PREF_NEW_SCALE);
   openremovesel = GetCheckVal(PREF_OPEN_REM_SEL);
   opencursindex = GetChoiceVal(PREF_OPEN_CURSOR);
   maxrecent     = GetSpinVal(PREF_MAX_RECENT);

   // EDIT_PAGE
   randomfill = GetSpinVal(PREF_RANDOM_FILL);

   // CONTROL_PAGE
   maxhashmem = GetSpinVal(PREF_MAX_HASH_MEM);
   qbasestep  = GetSpinVal(PREF_QBASE);
   hbasestep  = GetSpinVal(PREF_HBASE);
   mindelay   = GetSpinVal(PREF_MIN_DELAY);
   maxdelay   = GetSpinVal(PREF_MAX_DELAY);

   // VIEW_PAGE
   mathcoords     = GetCheckVal(PREF_Y_UP);
   showboldlines  = GetCheckVal(PREF_SHOW_BOLD);
   boldspacing    = GetSpinVal(PREF_BOLD_SPACING);
   mingridindex   = GetChoiceVal(PREF_MIN_GRID_SCALE);
   mousewheelmode = GetChoiceVal(PREF_MOUSE_WHEEL);
   thumbrange     = GetSpinVal(PREF_THUMB_RANGE);

   // update globals corresponding to the wxChoice menu selections
   mingridmag = mingridindex + 2;
   newcurs = IndexToCursor(newcursindex);
   opencurs = IndexToCursor(opencursindex);
   
   return true;
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
