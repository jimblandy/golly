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

#include "wx/stockitem.h"  // for wxGetStockLabel
#include "wx/dnd.h"        // for wxFileDropTarget
#include "wx/file.h"       // for wxFile
#include "wx/filename.h"   // for wxFileName
#include "wx/menuitem.h"   // for SetText
#include "wx/clipbrd.h"    // for wxTheClipboard
#include "wx/dataobj.h"    // for wxTextDataObject
#include "wx/image.h"      // for wxImage

#ifdef __WXMSW__
   // tool bar bitmaps are loaded via .rc file
#else
   // bitmap buttons for the tool bar
   #include "bitmaps/new.xpm"
   #include "bitmaps/open.xpm"
   #include "bitmaps/save.xpm"
   #include "bitmaps/patterns.xpm"
   #include "bitmaps/scripts.xpm"
   #include "bitmaps/play.xpm"
   #include "bitmaps/stop.xpm"
   #include "bitmaps/hash.xpm"
   #include "bitmaps/draw.xpm"
   #include "bitmaps/select.xpm"
   #include "bitmaps/move.xpm"
   #include "bitmaps/zoomin.xpm"
   #include "bitmaps/zoomout.xpm"
   #include "bitmaps/info.xpm"
#endif

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "readpattern.h"
#include "writepattern.h"

#include "wxgolly.h"       // for wxGetApp, statusptr, viewptr
#include "wxutils.h"       // for Warning, Fatal, etc
#include "wxprefs.h"       // for gollydir, SavePrefs, etc
#include "wxrule.h"        // for ChangeRule, GetRuleName
#include "wxinfo.h"        // for ShowInfo
#include "wxhelp.h"        // for ShowHelp
#include "wxstatus.h"      // for statusptr->...
#include "wxview.h"        // for viewptr->...
#include "wxrender.h"      // for InitDrawingData, DestroyDrawingData
#include "wxscript.h"      // for IsScript, RunScript, inscript
#include "wxmain.h"

#ifdef __WXMAC__
   #include <Carbon/Carbon.h>                      // for GetCurrentProcess, etc
   #include "wx/mac/corefoundation/cfstring.h"     // for wxMacCFStringHolder
#endif

// -----------------------------------------------------------------------------

// IDs for timer and menu commands
enum {
   // one-shot timer
   ID_ONE_TIMER = wxID_HIGHEST,

   // go/stop button (not yet implemented!!!)
   ID_GO_STOP,

   // File menu
   // wxID_NEW,
   // wxID_OPEN,
   ID_OPEN_CLIP,
   ID_OPEN_RECENT,
   // last item in Open Recent submenu
   ID_CLEAR_PATTERNS = ID_OPEN_RECENT + MAX_RECENT + 1,
   ID_SHOW_PATTERNS,
   ID_PATTERN_DIR,
   // wxID_SAVE,
   ID_SAVE_XRLE,
   ID_RUN_SCRIPT,
   ID_RUN_CLIP,
   ID_RUN_RECENT,
   // last item in Run Recent submenu
   ID_CLEAR_SCRIPTS = ID_RUN_RECENT + MAX_RECENT + 1,
   ID_SHOW_SCRIPTS,
   ID_SCRIPT_DIR,
   // wxID_PREFERENCES,
   
   // Edit menu
   ID_CUT,
   ID_COPY,
   ID_CLEAR,
   ID_OUTSIDE,
   ID_PASTE,
   ID_PMODE,
   ID_PLOCATION,
   ID_PASTE_SEL,
   ID_SELALL,
   ID_REMOVE,
   ID_SHRINK,
   ID_RANDOM,
   ID_FLIPUD,
   ID_FLIPLR,
   ID_ROTATEC,
   ID_ROTATEA,
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
   ID_HINFO,
   ID_RULE,
   
   // View menu
   ID_FULL,
   ID_FIT,
   ID_FIT_SEL,
   ID_MIDDLE,
   ID_RESTORE00,
   // wxID_ZOOM_IN,
   // wxID_ZOOM_OUT,
   ID_SET_SCALE,
   ID_TOOL,
   ID_STATUS,
   ID_EXACT,
   ID_GRID,
   ID_COLORS,
   ID_BUFF,
   ID_INFO,

   // Set Scale submenu
   ID_SCALE_1,
   ID_SCALE_2,
   ID_SCALE_4,
   ID_SCALE_8,
   ID_SCALE_16,
   
   // Help menu
   ID_HELP_INDEX,
   ID_HELP_INTRO,
   ID_HELP_TIPS,
   ID_HELP_SHORTCUTS,
   ID_HELP_SCRIPTING,
   ID_HELP_LEXICON,
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

// static routines used by GetPrefs() to get IDs for items in Open/Run Recent submenus;
// can't be MainFrame methods because GetPrefs() is called before creating main window
// and I'd rather not expose the IDs in the header file

int GetID_CLEAR_PATTERNS() { return ID_CLEAR_PATTERNS; }
int GetID_OPEN_RECENT()    { return ID_OPEN_RECENT; }
int GetID_CLEAR_SCRIPTS()  { return ID_CLEAR_SCRIPTS; }
int GetID_RUN_RECENT()     { return ID_RUN_RECENT; }

// -----------------------------------------------------------------------------

// one-shot timer to fix problems in wxMac and wxGTK -- see OnOneTimer;
// must be static because it's used in DnDFile::OnDropFiles
wxTimer *onetimer;

// name of temporary file created by SaveStartingPattern and OpenClipboard;
// it can be used to reset pattern or to show comments
wxString tempstart;

// name of temporary file created by RunClipboard
wxString scriptfile;

// name of temporary file for storing clipboard data
wxString clipfile;

#ifdef __WXMSW__
bool callUnselect = false;    // OnIdle needs to call Unselect?
#endif

// -----------------------------------------------------------------------------

// bitmaps for tool bar buttons
const int go_index = 0;
const int stop_index = 1;
const int new_index = 2;
const int open_index = 3;
const int save_index = 4;
const int patterns_index = 5;
const int scripts_index = 6;
const int draw_index = 7;
const int sel_index = 8;
const int move_index = 9;
const int zoomin_index = 10;
const int zoomout_index = 11;
const int info_index = 12;
const int hash_index = 13;
wxBitmap tbBitmaps[14];

// -----------------------------------------------------------------------------

// update functions:

// update tool bar buttons according to the current state
void MainFrame::UpdateToolBar(bool active)
{
   wxToolBar *tbar = GetToolBar();
   if (tbar && tbar->IsShown()) {
      if (viewptr->waitingforclick) active = false;
            
      #ifdef __WXX11__
         // avoid problems by first toggling off all buttons
         tbar->ToggleTool(ID_GO, false);
         tbar->ToggleTool(ID_STOP, false);
         tbar->ToggleTool(ID_HASH, false);
         tbar->ToggleTool(wxID_NEW, false);
         tbar->ToggleTool(wxID_OPEN, false);
         tbar->ToggleTool(wxID_SAVE, false);
         tbar->ToggleTool(ID_SHOW_PATTERNS, false);
         tbar->ToggleTool(ID_SHOW_SCRIPTS, false);
         tbar->ToggleTool(ID_DRAW, false);
         tbar->ToggleTool(ID_SELECT, false);
         tbar->ToggleTool(ID_MOVE, false);
         tbar->ToggleTool(ID_ZOOMIN, false);
         tbar->ToggleTool(ID_ZOOMOUT, false);
         tbar->ToggleTool(ID_INFO, false);
      #endif
      
      bool busy = generating || inscript;
      
      tbar->EnableTool(ID_GO,             active && !busy);
      tbar->EnableTool(ID_STOP,           active && busy);
      tbar->EnableTool(ID_HASH,           active && !busy);
      tbar->EnableTool(wxID_NEW,          active && !busy);
      tbar->EnableTool(wxID_OPEN,         active && !busy);
      tbar->EnableTool(wxID_SAVE,         active && !busy);
      tbar->EnableTool(ID_SHOW_PATTERNS,  active);
      tbar->EnableTool(ID_SHOW_SCRIPTS,   active);
      tbar->EnableTool(ID_DRAW,           active);
      tbar->EnableTool(ID_SELECT,         active);
      tbar->EnableTool(ID_MOVE,           active);
      tbar->EnableTool(ID_ZOOMIN,         active);
      tbar->EnableTool(ID_ZOOMOUT,        active);
      tbar->EnableTool(ID_INFO,           active && !currfile.IsEmpty());

      // call ToggleTool for tools added via AddCheckTool or AddRadioTool
      tbar->ToggleTool(ID_HASH,           hashing);
      tbar->ToggleTool(ID_SHOW_PATTERNS,  showpatterns);
      tbar->ToggleTool(ID_SHOW_SCRIPTS,   showscripts);
      tbar->ToggleTool(ID_DRAW,           currcurs == curs_pencil);
      tbar->ToggleTool(ID_SELECT,         currcurs == curs_cross);
      tbar->ToggleTool(ID_MOVE,           currcurs == curs_hand);
      tbar->ToggleTool(ID_ZOOMIN,         currcurs == curs_zoomin);
      tbar->ToggleTool(ID_ZOOMOUT,        currcurs == curs_zoomout);
   }
}

bool MainFrame::ClipboardHasText()
{
   #ifdef __WXX11__
      return wxFileExists(clipfile);
   #else
      bool hastext = false;
      if ( wxTheClipboard->Open() ) {
         hastext = wxTheClipboard->IsSupported( wxDF_TEXT );
         if (!hastext) {
            // we'll try to convert bitmap data to text pattern
            hastext = wxTheClipboard->IsSupported( wxDF_BITMAP );
         }
         wxTheClipboard->Close();
      }
      return hastext;
   #endif
}

bool MainFrame::StatusVisible()
{
   return (statusptr && statusptr->statusht > 0);
}

void MainFrame::EnableAllMenus(bool enable)
{
   #ifdef __WXMAC__
      // enable/disable all menus, including Help menu and items in app menu
      if (enable)
         EndAppModalStateForWindow( (OpaqueWindowPtr*)this->MacGetWindowRef() );
      else
         BeginAppModalStateForWindow( (OpaqueWindowPtr*)this->MacGetWindowRef() );
   #else
      wxMenuBar *mbar = GetMenuBar();
      if (mbar) {
         int count = mbar->GetMenuCount();
         int i;
         for (i = 0; i<count; i++) {
            mbar->EnableTop(i, enable);
         }
      }
   #endif
}

// update menu bar items according to the given state
void MainFrame::UpdateMenuItems(bool active)
{
   wxMenuBar *mbar = GetMenuBar();
   wxToolBar *tbar = GetToolBar();
   if (mbar) {
      if (viewptr->waitingforclick) active = false;
      bool textinclip = ClipboardHasText();
      bool selexists = viewptr->SelectionExists();
      bool busy = generating || inscript;
      
      mbar->Enable(wxID_NEW,           active && !busy);
      mbar->Enable(wxID_OPEN,          active && !busy);
      mbar->Enable(ID_OPEN_CLIP,       active && !busy && textinclip);
      mbar->Enable(ID_OPEN_RECENT,     active && !busy && numpatterns > 0);
      mbar->Enable(ID_SHOW_PATTERNS,   active);
      mbar->Enable(ID_PATTERN_DIR,     active);
      mbar->Enable(wxID_SAVE,          active && !busy);
      mbar->Enable(ID_SAVE_XRLE,       active);
      mbar->Enable(ID_RUN_SCRIPT,      active && !busy);
      mbar->Enable(ID_RUN_CLIP,        active && !busy && textinclip);
      mbar->Enable(ID_RUN_RECENT,      active && !busy && numscripts > 0);
      mbar->Enable(ID_SHOW_SCRIPTS,    active);
      mbar->Enable(ID_SCRIPT_DIR,      active);
      mbar->Enable(wxID_PREFERENCES,   !busy);

      mbar->Enable(ID_CUT,       active && !busy && selexists);
      mbar->Enable(ID_COPY,      active && !busy && selexists);
      mbar->Enable(ID_CLEAR,     active && !busy && selexists);
      mbar->Enable(ID_OUTSIDE,   active && !busy && selexists);
      mbar->Enable(ID_PASTE,     active && !busy && textinclip);
      mbar->Enable(ID_PASTE_SEL, active && !busy && textinclip && selexists);
      mbar->Enable(ID_PLOCATION, active);
      mbar->Enable(ID_PMODE,     active);
      mbar->Enable(ID_SELALL,    active);
      mbar->Enable(ID_REMOVE,    active && selexists);
      mbar->Enable(ID_SHRINK,    active && selexists);
      mbar->Enable(ID_RANDOM,    active && !busy && selexists);
      mbar->Enable(ID_FLIPUD,    active && !busy && selexists);
      mbar->Enable(ID_FLIPLR,    active && !busy && selexists);
      mbar->Enable(ID_ROTATEC,   active && !busy && selexists);
      mbar->Enable(ID_ROTATEA,   active && !busy && selexists);
      mbar->Enable(ID_CMODE,     active);

      mbar->Enable(ID_GO,        active && !busy);
      mbar->Enable(ID_STOP,      active && busy);
      mbar->Enable(ID_NEXT,      active && !busy);
      mbar->Enable(ID_STEP,      active && !busy);
      mbar->Enable(ID_RESET,     active && !busy && curralgo->getGeneration() > startgen);
      mbar->Enable(ID_FASTER,    active);
      mbar->Enable(ID_SLOWER,    active && warp > minwarp);
      mbar->Enable(ID_AUTO,      active);
      mbar->Enable(ID_HASH,      active && !busy);
      mbar->Enable(ID_HYPER,     active && curralgo->hyperCapable());
      mbar->Enable(ID_HINFO,     active && curralgo->hyperCapable());
      mbar->Enable(ID_RULE,      active && !busy);

      mbar->Enable(ID_FULL,      active);
      mbar->Enable(ID_FIT,       active);
      mbar->Enable(ID_FIT_SEL,   active && selexists);
      mbar->Enable(ID_MIDDLE,    active);
      mbar->Enable(ID_RESTORE00, active && (viewptr->originx != bigint::zero ||
                                            viewptr->originy != bigint::zero));
      mbar->Enable(wxID_ZOOM_IN, active && viewptr->GetMag() < MAX_MAG);
      mbar->Enable(wxID_ZOOM_OUT, active);
      mbar->Enable(ID_SET_SCALE, active);
      mbar->Enable(ID_TOOL,      active);
      mbar->Enable(ID_STATUS,    active);
      mbar->Enable(ID_EXACT,     active);
      mbar->Enable(ID_GRID,      active);
      mbar->Enable(ID_COLORS,    active);
      #ifdef __WXMAC__
         // windows on Mac OS X are automatically buffered
         mbar->Enable(ID_BUFF,   false);
         mbar->Check(ID_BUFF,    true);
      #else
         mbar->Enable(ID_BUFF,   active);
         mbar->Check(ID_BUFF,    buffered);
      #endif
      mbar->Enable(ID_INFO,      !currfile.IsEmpty());

      // tick/untick menu items created using AppendCheckItem
      mbar->Check(ID_SAVE_XRLE,     savexrle);
      mbar->Check(ID_SHOW_PATTERNS, showpatterns);
      mbar->Check(ID_SHOW_SCRIPTS,  showscripts);
      mbar->Check(ID_AUTO,       autofit);
      mbar->Check(ID_HASH,       hashing);
      mbar->Check(ID_HYPER,      hyperspeed);
      mbar->Check(ID_HINFO,      hlifealgo::getVerbose() != 0);
      mbar->Check(ID_TOOL,       tbar && tbar->IsShown());
      mbar->Check(ID_STATUS,     StatusVisible());
      mbar->Check(ID_EXACT,      showexact);
      mbar->Check(ID_GRID,       showgridlines);
      mbar->Check(ID_COLORS,     swapcolors);
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
      mbar->Check(ID_SCALE_1,    viewptr->GetMag() == 0);
      mbar->Check(ID_SCALE_2,    viewptr->GetMag() == 1);
      mbar->Check(ID_SCALE_4,    viewptr->GetMag() == 2);
      mbar->Check(ID_SCALE_8,    viewptr->GetMag() == 3);
      mbar->Check(ID_SCALE_16,   viewptr->GetMag() == 4);
   }
}

void MainFrame::UpdateUserInterface(bool active)
{
   UpdateToolBar(active);
   UpdateMenuItems(active);
   viewptr->CheckCursor(active);
   statusptr->CheckMouseLocation(active);
}

// update everything in main window, and menu bar and cursor
void MainFrame::UpdateEverything()
{
   if (IsIconized()) {
      // main window has been minimized, so only update menu bar items
      UpdateMenuItems(false);
      return;
   }

   // update tool bar, menu bar and cursor
   UpdateUserInterface(IsActive());

   if (inscript) {
      // make sure scroll bars are accurate while running script
      viewptr->UpdateScrollBars();
      return;
   }

   int wd, ht;
   GetClientSize(&wd, &ht);      // includes status bar and viewport

   if (wd > 0 && ht > statusptr->statusht) {
      viewptr->Refresh(false, NULL);
      viewptr->Update();
      viewptr->UpdateScrollBars();
   }
   
   if (wd > 0 && ht > 0 && StatusVisible()) {
      statusptr->Refresh(false, NULL);
      statusptr->Update();
   }
}

// only update viewport and status bar
void MainFrame::UpdatePatternAndStatus()
{
   if (inscript) return;

   if (!IsIconized()) {
      viewptr->Refresh(false, NULL);
      viewptr->Update();
      if (StatusVisible()) {
         statusptr->CheckMouseLocation(IsActive());
         statusptr->Refresh(false, NULL);
         statusptr->Update();
      }
   }
}

// only update status bar
void MainFrame::UpdateStatus()
{
   if (!IsIconized()) {
      if (StatusVisible()) {
         statusptr->CheckMouseLocation(IsActive());
         statusptr->Refresh(false, NULL);
         statusptr->Update();
      }
   }
}

// -----------------------------------------------------------------------------

// file functions:

const wxString B0message = _("Hashing has been turned off due to B0-not-S8 rule.");

void MainFrame::MySetTitle(const wxString& title)
{
   #ifdef __WXMAC__
      // avoid wxMac's SetTitle call -- it causes an undesirable window refresh
      SetWindowTitleWithCFString((OpaqueWindowPtr*)this->MacGetWindowRef(),
                                 wxMacCFStringHolder(title, wxFONTENCODING_DEFAULT)) ;
   #else
      SetTitle(title);
   #endif
}

void MainFrame::SetWindowTitle(const wxString& filename)
{
   wxString wtitle;
   if ( !filename.IsEmpty() ) {
      // remember current file name
      currname = filename;
   }
   wxString rule = GetRuleName( wxString(curralgo->getrule(),wxConvLocal) );
   #ifdef __WXMAC__
      wtitle.Printf(_("%s [%s]"), currname.c_str(), rule.c_str());
   #else
      wtitle.Printf(_("%s [%s] - Golly"), currname.c_str(), rule.c_str());
   #endif
   // better to truncate a really long title???!!!
   MySetTitle(wtitle);
}

void MainFrame::SetGenIncrement()
{
   if (warp > 0) {
      bigint inc = 1;
      // WARNING: if this code changes then also change StatusBar::DrawStatusBar
      if (hashing) {
         // set inc to hbasestep^warp
         int i = warp;
         while (i > 0) { inc.mul_smallint(hbasestep); i--; }
      } else {
         // set inc to qbasestep^warp
         int i = warp;
         while (i > 0) { inc.mul_smallint(qbasestep); i--; }
      }
      curralgo->setIncrement(inc);
   } else {
      curralgo->setIncrement(1);
   }
}

void MainFrame::CreateUniverse()
{
   // first delete old universe if it exists
   if (curralgo) delete curralgo;

   if (hashing) {
      curralgo = new hlifealgo();
      curralgo->setMaxMemory(maxhashmem);
   } else {
      curralgo = new qlifealgo();
   }

   // curralgo->step() will call wx_poll::checkevents()
   curralgo->setpoll(wxGetApp().Poller());

   // increment has been reset to 1 but that's probably not always desirable
   // so set increment using current warp value
   SetGenIncrement();
}

void MainFrame::NewPattern(const wxString& title)
{
   if (generating) return;
   savestart = false;
   currfile.Clear();
   startgen = 0;
   warp = 0;
   CreateUniverse();

   if (initrule[0]) {
      // this is the first call of NewPattern when app starts
      const char *err = curralgo->setrule(initrule);
      if (err) Warning(wxString(err,wxConvLocal));
      if (global_liferules.hasB0notS8 && hashing) {
         hashing = false;
         statusptr->SetMessage(B0message);
         CreateUniverse();
      }
      initrule[0] = 0;        // don't use it again
   }

   if (newremovesel) viewptr->NoSelection();
   if (newcurs) currcurs = newcurs;
   viewptr->SetPosMag(bigint::zero, bigint::zero, newmag);

   // best to restore true origin
   if (viewptr->originx != bigint::zero || viewptr->originy != bigint::zero) {
      viewptr->originy = 0;
      viewptr->originx = 0;
      statusptr->SetMessage(origin_restored);
   }

   // window title will also show curralgo->getrule()
   SetWindowTitle(title);

   UpdateEverything();
}

bool MainFrame::LoadImage()
{
   wxString ext = currfile.AfterLast(wxT('.'));
   
   // supported extensions match image handlers added in GollyApp::OnInit()
   if ( ext.IsSameAs(wxT("bmp"),false) ||
        ext.IsSameAs(wxT("gif"),false) ||
        ext.IsSameAs(wxT("png"),false) ||
        ext.IsSameAs(wxT("tif"),false) ||
        ext.IsSameAs(wxT("tiff"),false) ) {
      wxImage image;
      if ( image.LoadFile(currfile) ) {
         curralgo->setrule("B3/S23");
         
         unsigned char maskr, maskg, maskb;
         bool hasmask = image.GetOrFindMaskColour(&maskr, &maskg, &maskb);
         int wd = image.GetWidth();
         int ht = image.GetHeight();
         unsigned char *idata = image.GetData();
         int x, y;
         for (y = 0; y < ht; y++)
            for (x = 0; x < wd; x++) {
               long pos = (y * wd + x) * 3;
               unsigned char r = idata[pos];
               unsigned char g = idata[pos+1];
               unsigned char b = idata[pos+2];
               if ( hasmask && r == maskr && g == maskg && b == maskb ) {
                  // treat transparent pixel as a dead cell
               } else if ( r < 255 || g < 255 || b < 255 ) {
                  // treat non-white pixel as a live cell
                  curralgo->setcell(x, y, 1);
               }
            }
         
         curralgo->endofpattern();
      } else {
         Warning(_("Could not load image from file!"));
      }
      return true;
   } else {
      return false;
   }
}

void MainFrame::LoadPattern(const wxString& newtitle)
{
   // don't use initrule in future NewPattern calls
   initrule[0] = 0;
   if (!newtitle.IsEmpty()) {
      savestart = false;
      warp = 0;
      if (GetInfoFrame()) {
         // comments will no longer be relevant so close info window
         GetInfoFrame()->Close(true);
      }
   }
   if (!showbanner) statusptr->ClearMessage();

   // set this flag BEFORE UpdateStatus() call so we see gen=0 and pop=0;
   // in particular, it avoids getPopulation being called which would
   // slow down hlife pattern loading
   viewptr->nopattupdate = true;
   
   // update all of status bar so we don't see different colored lines;
   // on Mac, DrawView also gets called if there are pending updates
   UpdateStatus();
   CreateUniverse();

   if (!newtitle.IsEmpty()) {
      // show new file name in window title but no rule (which readpattern can change);
      // nicer if user can see file name while loading a very large pattern
      MySetTitle(_("Loading ") + newtitle);
   }

   if (LoadImage()) {
      viewptr->nopattupdate = false;
   } else {
      const char *err = readpattern(currfile.mb_str(wxConvLocal), *curralgo);
      if (err && strcmp(err,cannotreadhash) == 0 && !hashing) {
         hashing = true;
         statusptr->SetMessage(_("Hashing has been turned on for macrocell format."));
         // update all of status bar so we don't see different colored lines
         UpdateStatus();
         CreateUniverse();
         err = readpattern(currfile.mb_str(wxConvLocal), *curralgo);
      } else if (global_liferules.hasB0notS8 && hashing && !newtitle.IsEmpty()) {
         hashing = false;
         statusptr->SetMessage(B0message);
         // update all of status bar so we don't see different colored lines
         UpdateStatus();
         CreateUniverse();
         err = readpattern(currfile.mb_str(wxConvLocal), *curralgo);
      }
      viewptr->nopattupdate = false;
      if (err) Warning(wxString(err,wxConvLocal));
   }

   if (!newtitle.IsEmpty()) {
      // show full window title after readpattern has set rule
      SetWindowTitle(newtitle);
      if (openremovesel) viewptr->NoSelection();
      if (opencurs) currcurs = opencurs;
      viewptr->FitInView(1);
      startgen = curralgo->getGeneration();     // might be > 0
      UpdateEverything();
      showbanner = false;
   } else {
      // ResetPattern sets rule, window title, scale and location
   }
}

void MainFrame::ResetPattern()
{
   if (generating || curralgo->getGeneration() == startgen) return;
   
   if (curralgo->getGeneration() < startgen) {
      // if this happens then startgen logic is wrong
      Warning(_("Current gen < starting gen!"));
      return;
   }
   
   if (startfile.IsEmpty() && currfile.IsEmpty()) {
      // if this happens then savestart logic is wrong
      Warning(_("Starting pattern cannot be restored!"));
      return;
   }
   
   // restore pattern and settings saved by SaveStartingPattern;
   // first restore step size, hashing option and starting pattern
   warp = startwarp;
   hashing = starthash;

   wxString oldfile;
   if ( !startfile.IsEmpty() ) {
      // temporarily change currfile to startfile
      oldfile = currfile;
      currfile = startfile;
   }
   
   // restore starting pattern from currfile;
   // pass in empty string so savestart, warp and currcurs won't change
   LoadPattern(wxEmptyString);
   // gen count has been reset to startgen
   
   if ( !startfile.IsEmpty() ) {
      // restore currfile
      currfile = oldfile;
      savestart = true;       // should not be necessary, but play safe
   }
   
   // now restore rule, window title, scale and location
   curralgo->setrule(startrule.mb_str(wxConvLocal));
   SetWindowTitle(wxEmptyString);
   viewptr->SetPosMag(startx, starty, startmag);
   UpdateEverything();
}

wxString MainFrame::GetBaseName(const wxString& fullpath)
{
   // extract basename from given path
   return fullpath.AfterLast(wxFILE_SEP_PATH);
}

void MainFrame::SetCurrentFile(const wxString& path)
{
   #ifdef __WXMAC__
      // copy given path to currfile but as decomposed UTF8 so fopen will work
      #if wxCHECK_VERSION(2, 7, 0)
         currfile = wxString(path.fn_str(), wxConvLocal);
      #else
         // wxMac 2.6.x or older (but conversion doesn't always work on OS 10.4)
         currfile = wxString(path.wc_str(wxConvLocal), wxConvUTF8);
      #endif
   #else
      currfile = path;
   #endif
}

void MainFrame::OpenFile(const wxString& path, bool remember)
{
   if ( IsScript(path) ) {
      // execute script
      if (remember) AddRecentScript(path);
      RunScript(path);
   } else {
      // load pattern
      SetCurrentFile(path);
      if (remember) AddRecentPattern(path);
      LoadPattern( GetBaseName(path) );
   }
}

void MainFrame::AddRecentPattern(const wxString& path)
{
   // put given path at start of patternSubMenu
   int id = patternSubMenu->FindItem(path);
   if ( id == wxNOT_FOUND ) {
      if ( numpatterns < maxpatterns ) {
         // add new path
         numpatterns++;
         id = ID_OPEN_RECENT + numpatterns;
         patternSubMenu->Insert(numpatterns - 1, id, path);
      } else {
         // replace last item with new path
         wxMenuItem *item = patternSubMenu->FindItemByPosition(maxpatterns - 1);
         item->SetText(path);
         id = ID_OPEN_RECENT + maxpatterns;
      }
   }
   // path exists in patternSubMenu 
   if ( id > ID_OPEN_RECENT + 1 ) {
      // move path to start of menu (item IDs don't change)
      wxMenuItem *item;
      while ( id > ID_OPEN_RECENT + 1 ) {
         wxMenuItem *previtem = patternSubMenu->FindItem(id - 1);
         wxString prevpath = previtem->GetText();
         item = patternSubMenu->FindItem(id);
         item->SetText(prevpath);
         id--;
      }
      item = patternSubMenu->FindItem(id);
      item->SetText(path);
   }
}

void MainFrame::AddRecentScript(const wxString& path)
{
   // put given path at start of scriptSubMenu
   int id = scriptSubMenu->FindItem(path);
   if ( id == wxNOT_FOUND ) {
      if ( numscripts < maxscripts ) {
         // add new path
         numscripts++;
         id = ID_RUN_RECENT + numscripts;
         scriptSubMenu->Insert(numscripts - 1, id, path);
      } else {
         // replace last item with new path
         wxMenuItem *item = scriptSubMenu->FindItemByPosition(maxscripts - 1);
         item->SetText(path);
         id = ID_RUN_RECENT + maxscripts;
      }
   }
   // path exists in scriptSubMenu 
   if ( id > ID_RUN_RECENT + 1 ) {
      // move path to start of menu (item IDs don't change)
      wxMenuItem *item;
      while ( id > ID_RUN_RECENT + 1 ) {
         wxMenuItem *previtem = scriptSubMenu->FindItem(id - 1);
         wxString prevpath = previtem->GetText();
         item = scriptSubMenu->FindItem(id);
         item->SetText(prevpath);
         id--;
      }
      item = scriptSubMenu->FindItem(id);
      item->SetText(path);
   }
}

void MainFrame::OpenPattern()
{
   if (generating) return;

   wxString filetypes = _("All files (*)|*");
   filetypes +=         _("|RLE (*.rle)|*.rle");
   filetypes +=         _("|Macrocell (*.mc)|*.mc");
   filetypes +=         _("|Life 1.05/1.06 (*.lif)|*.lif");
   filetypes +=         _("|dblife (*.l)|*.l");
   filetypes +=         _("|Gzip (*.gz)|*.gz");
   filetypes +=         _("|BMP (*.bmp)|*.bmp");
   filetypes +=         _("|GIF (*.gif)|*.gif");
   filetypes +=         _("|PNG (*.png)|*.png");
   filetypes +=         _("|TIFF (*.tiff;*.tif)|*.tiff;*.tif");
   
   wxFileDialog opendlg(this, _("Choose a pattern file"),
                        opensavedir, wxEmptyString, filetypes,
                        wxOPEN | wxFILE_MUST_EXIST);

   if ( opendlg.ShowModal() == wxID_OK ) {
      wxFileName fullpath( opendlg.GetPath() );
      opensavedir = fullpath.GetPath();
      SetCurrentFile( opendlg.GetPath() );
      AddRecentPattern( opendlg.GetPath() );
      LoadPattern( opendlg.GetFilename() );
   }
}

void MainFrame::OpenScript()
{
   if (generating) return;

   wxFileDialog opendlg(this, _("Choose a Python script"),
                        rundir, wxEmptyString,
                        _("Python script (*.py)|*.py"),
                        wxOPEN | wxFILE_MUST_EXIST);

   if ( opendlg.ShowModal() == wxID_OK ) {
      wxFileName fullpath( opendlg.GetPath() );
      rundir = fullpath.GetPath();
      AddRecentScript( opendlg.GetPath() );
      RunScript( opendlg.GetPath() );
   }
}

bool MainFrame::CopyTextToClipboard(wxString &text)
{
   bool result = true;
   #ifdef __WXX11__
      // no global clipboard support on X11 so we save data in a file
      wxFile tmpfile(clipfile, wxFile::write);
      if ( tmpfile.IsOpened() ) {
         size_t textlen = text.Length();
         if ( tmpfile.Write( text.c_str(), textlen ) < textlen ) {
            Warning(_("Could not write all data to clipboard file!"));
            result = false;
         }
         tmpfile.Close();
      } else {
         Warning(_("Could not create clipboard file!"));
         result = false;
      }
   #else
      if (wxTheClipboard->Open()) {
         if ( !wxTheClipboard->SetData(new wxTextDataObject(text)) ) {
            Warning(_("Could not copy text to clipboard!"));
            result = false;
         }
         wxTheClipboard->Close();
      } else {
         Warning(_("Could not open clipboard!"));
         result = false;
      }
   #endif
   return result;
}

bool MainFrame::GetTextFromClipboard(wxTextDataObject *textdata)
{
   bool gotdata = false;
   
   if ( wxTheClipboard->Open() ) {
      if ( wxTheClipboard->IsSupported( wxDF_TEXT ) ) {
         gotdata = wxTheClipboard->GetData( *textdata );
         if (!gotdata) {
            statusptr->ErrorMessage(_("Could not get clipboard text!"));
         }

      } else if ( wxTheClipboard->IsSupported( wxDF_BITMAP ) ) {
         wxBitmapDataObject bmapdata;
         gotdata = wxTheClipboard->GetData( bmapdata );
         if (gotdata) {
            // convert bitmap data to text data
            wxString str;
            wxBitmap bmap = bmapdata.GetBitmap();
            wxImage image = bmap.ConvertToImage();
            if (image.Ok()) {
               /* there doesn't seem to be any mask or alpha info, at least on Mac
               if (bmap.GetMask() != NULL) Warning("Bitmap has mask!");
               if (image.HasMask()) Warning("Image has mask!");
               if (image.HasAlpha()) Warning("Image has alpha!");
               */
               int wd = image.GetWidth();
               int ht = image.GetHeight();
               unsigned char *idata = image.GetData();
               int x, y;
               for (y = 0; y < ht; y++) {
                  for (x = 0; x < wd; x++) {
                     long pos = (y * wd + x) * 3;
                     if ( idata[pos] < 255 || idata[pos+1] < 255 || idata[pos+2] < 255 ) {
                        // non-white pixel is a live cell
                        str += 'o';
                     } else {
                        // white pixel is a dead cell
                        str += '.';
                     }
                  }
                  str += '\n';
               }
               textdata->SetText(str);
            } else {
               statusptr->ErrorMessage(_("Could not convert clipboard bitmap!"));
               gotdata = false;
            }
         } else {
            statusptr->ErrorMessage(_("Could not get clipboard bitmap!"));
         }

      } else {
         #ifdef __WXX11__
            statusptr->ErrorMessage(_("Sorry, but there is no clipboard support for X11."));
            // do X11 apps like xlife or fontforge have clipboard support???!!!
         #else
            statusptr->ErrorMessage(_("No data in clipboard."));
         #endif
      }
      wxTheClipboard->Close();

   } else {
      statusptr->ErrorMessage(_("Could not open clipboard!"));
   }
   
   return gotdata;
}

void MainFrame::OpenClipboard()
{
   if (generating) return;
   // load and view pattern data stored in clipboard
   #ifdef __WXX11__
      // on X11 the clipboard data is in non-temporary clipfile, so copy
      // clipfile to tempstart (for use by ResetPattern and ShowPatternInfo)
      if ( wxCopyFile(clipfile, tempstart, true) ) {
         currfile = tempstart;
         LoadPattern(_("clipboard"));
      } else {
         statusptr->ErrorMessage(_("Could not copy clipfile!"));
      }
   #else
      wxTextDataObject data;
      if (GetTextFromClipboard(&data)) {
         // copy clipboard data to tempstart so we can handle all formats
         // supported by readpattern
         wxFile outfile(tempstart, wxFile::write);
         if ( outfile.IsOpened() ) {
            outfile.Write( data.GetText() );
            outfile.Close();
            currfile = tempstart;
            LoadPattern(_("clipboard"));
            // do NOT delete tempstart -- it can be reloaded by ResetPattern
            // or used by ShowPatternInfo
         } else {
            statusptr->ErrorMessage(_("Could not create tempstart file!"));
         }
      }
   #endif
}

void MainFrame::RunClipboard()
{
   if (generating) return;
   // run script stored in clipboard
   wxTextDataObject data;
   if (GetTextFromClipboard(&data)) {
      // copy clipboard data to scriptfile
      wxFile outfile(scriptfile, wxFile::write);
      if ( outfile.IsOpened() ) {
         outfile.Write( data.GetText() );
         outfile.Close();
         RunScript(scriptfile);
      } else {
         statusptr->ErrorMessage(_("Could not create script file!"));
      }
   }
}

void MainFrame::OpenRecentPattern(int id)
{
   wxMenuItem *item = patternSubMenu->FindItem(id);
   if (item) {
      wxString path = item->GetText();
      SetCurrentFile(path);
      AddRecentPattern(path);
      LoadPattern(GetBaseName(path));
   }
}

void MainFrame::OpenRecentScript(int id)
{
   wxMenuItem *item = scriptSubMenu->FindItem(id);
   if (item) {
      wxString path = item->GetText();
      AddRecentScript(path);
      RunScript(path);
   }
}

void MainFrame::ClearRecentPatterns()
{
   while (numpatterns > 0) {
      patternSubMenu->Delete( patternSubMenu->FindItemByPosition(0) );
      numpatterns--;
   }
   wxMenuBar *mbar = GetMenuBar();
   if (mbar) mbar->Enable(ID_OPEN_RECENT, false);
}

void MainFrame::ClearRecentScripts()
{
   while (numscripts > 0) {
      scriptSubMenu->Delete( scriptSubMenu->FindItemByPosition(0) );
      numscripts--;
   }
   wxMenuBar *mbar = GetMenuBar();
   if (mbar) mbar->Enable(ID_RUN_RECENT, false);
}

const char *WritePattern(const wxString& path,
                         pattern_format format,
                         int top, int left, int bottom, int right)
{
   #if defined(__WXMAC__) && wxCHECK_VERSION(2, 7, 0)
      // use decomposed UTF8 so fopen will work
      const char *err = writepattern(path.fn_str(),
                        *curralgo, format, top, left, bottom, right);
   #else
      const char *err = writepattern(path.mb_str(wxConvLocal),
                        *curralgo, format, top, left, bottom, right);
   #endif
   return err;
}

void MainFrame::SavePattern()
{
   if (generating) return;
   
   wxString filetypes;
   int RLEindex, L105index, MCindex;
   
   // initially all formats are not allowed (use any -ve number)
   RLEindex = L105index = MCindex = -1;
   
   bigint top, left, bottom, right;
   int itop, ileft, ibottom, iright;
   curralgo->findedges(&top, &left, &bottom, &right);
   
   wxString RLEstring;
   if (savexrle)
      RLEstring = _("Extended RLE (*.rle)|*.rle");
   else
      RLEstring = _("RLE (*.rle)|*.rle");
   
   if (hashing) {
      if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
         // too big so only allow saving as MC file
         itop = ileft = ibottom = iright = 0;
         filetypes = _("Macrocell (*.mc)|*.mc");
         MCindex = 0;
      } else {
         // allow saving as RLE/MC file
         itop = top.toint();
         ileft = left.toint();
         ibottom = bottom.toint();
         iright = right.toint();
         filetypes = RLEstring;
         RLEindex = 0;
         filetypes += _("|Macrocell (*.mc)|*.mc");
         MCindex = 1;
      }
   } else {
      // allow saving file only if pattern is small enough
      if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
         statusptr->ErrorMessage(_("Pattern is outside +/- 10^9 boundary."));
         return;
      }   
      itop = top.toint();
      ileft = left.toint();
      ibottom = bottom.toint();
      iright = right.toint();
      filetypes = RLEstring;
      RLEindex = 0;
      /* Life 1.05 format not yet implemented!!!
      filetypes += _("|Life 1.05 (*.lif)|*.lif");
      L105index = 1;
      */
   }

   wxFileDialog savedlg( this, _("Save pattern"),
                         opensavedir, wxEmptyString, filetypes,
                         wxSAVE | wxOVERWRITE_PROMPT );

   if ( savedlg.ShowModal() == wxID_OK ) {
      wxFileName fullpath( savedlg.GetPath() );
      opensavedir = fullpath.GetPath();
      wxString ext = fullpath.GetExt();
      pattern_format format;
      // if user supplied a known extension then use that format if it is
      // allowed, otherwise use current format specified in filter menu
      if ( ext.IsSameAs(wxT("rle"),false) && RLEindex >= 0 ) {
         format = savexrle ? XRLE_format : RLE_format;
      /* Life 1.05 format not yet implemented!!!
      } else if ( ext.IsSameAs("lif",false) && L105index >= 0 ) {
         format = L105_format;
      */
      } else if ( ext.IsSameAs(wxT("mc"),false) && MCindex >= 0 ) {
         format = MC_format;
      } else if ( savedlg.GetFilterIndex() == RLEindex ) {
         format = savexrle ? XRLE_format : RLE_format;
      } else if ( savedlg.GetFilterIndex() == L105index ) {
         format = L105_format;
      } else if ( savedlg.GetFilterIndex() == MCindex ) {
         format = MC_format;
      } else {
         statusptr->ErrorMessage(_("Bug in SavePattern!"));
         return;
      }
      SetCurrentFile( savedlg.GetPath() );
      AddRecentPattern( savedlg.GetPath() );
      SetWindowTitle( savedlg.GetFilename() );
      const char *err = WritePattern(savedlg.GetPath(), format,
                                     itop, ileft, ibottom, iright);
      if (err) {
         statusptr->ErrorMessage(wxString(err,wxConvLocal));
      } else {
         statusptr->DisplayMessage(_("Pattern saved in file."));
         if ( curralgo->getGeneration() == startgen ) {
            // no need to save starting pattern (ResetPattern can load currfile)
            savestart = false;
         }
      }
   }
}

// called by script command to save current pattern to given file
wxString MainFrame::SaveFile(const wxString& path, const wxString& format, bool remember)
{
   // check that given format is valid and allowed
   bigint top, left, bottom, right;
   int itop, ileft, ibottom, iright;
   curralgo->findedges(&top, &left, &bottom, &right);
   
   pattern_format pattfmt;
   if ( format.IsSameAs(wxT("rle"),false) ) {
      if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
         return _("Pattern is too big to save as RLE.");
      }   
      pattfmt = savexrle ? XRLE_format : RLE_format;
      itop = top.toint();
      ileft = left.toint();
      ibottom = bottom.toint();
      iright = right.toint();
   } else if ( format.IsSameAs(wxT("mc"),false) ) {
      if (!hashing) {
         return _("Macrocell format is only allowed if hashing.");
      }
      pattfmt = MC_format;
      // writepattern will ignore itop, ileft, ibottom, iright
      itop = ileft = ibottom = iright = 0;
   } else {
      return _("Unknown pattern format.");
   }   
   
   SetCurrentFile(path);
   if (remember) AddRecentPattern(path);
   SetWindowTitle( GetBaseName(path) );
   const char *err = WritePattern(path, pattfmt, itop, ileft, ibottom, iright);
   if (!err) {
      if ( curralgo->getGeneration() == startgen ) {
         // no need to save starting pattern (ResetPattern can load currfile)
         savestart = false;
      }
   }
   return wxString(err, wxConvLocal);
}

// -----------------------------------------------------------------------------

void SimplifyTree(wxString &dir, wxTreeCtrl* treectrl, wxTreeItemId root)
{
   // delete old tree (except root)
   treectrl->DeleteChildren(root);

   // append dir as only child
   wxDirItemData* diritem = new wxDirItemData(dir, dir, true);
   wxTreeItemId id;
   id = treectrl->AppendItem(root, dir.AfterLast(wxFILE_SEP_PATH), 0, 0, diritem);
   if ( diritem->HasFiles() || diritem->HasSubDirs() ) {
      treectrl->SetItemHasChildren(id);
      treectrl->Expand(id);
      #ifndef __WXMSW__
         // causes crash on Windows!!!
         treectrl->ScrollTo(root);
      #endif
   }
}

void DeselectTree(wxTreeCtrl* treectrl, wxTreeItemId root)
{
   // recursively traverse tree and reset each file item background to white
   wxTreeItemIdValue cookie;
   wxTreeItemId id = treectrl->GetFirstChild(root, cookie);
   while ( id.IsOk() ) {
      if ( treectrl->ItemHasChildren(id) ) {
         DeselectTree(treectrl, id);
      } else {
         wxColor currcolor = treectrl->GetItemBackgroundColour(id);
         if ( currcolor != *wxWHITE ) {
            treectrl->SetItemBackgroundColour(id, *wxWHITE);
         }
      }
      id = treectrl->GetNextChild(root, cookie);
   }
}

void MainFrame::ToggleShowPatterns()
{
   showpatterns = !showpatterns;
   if (showpatterns && showscripts) {
      showscripts = false;
      splitwin->Unsplit(scriptctrl);
      splitwin->SplitVertically(patternctrl, viewptr, dirwinwd);
   } else {
      if (splitwin->IsSplit()) {
         // hide left pane
         dirwinwd = splitwin->GetSashPosition();
         splitwin->Unsplit(patternctrl);
      } else {
         splitwin->SplitVertically(patternctrl, viewptr, dirwinwd);
      }
      // resize viewport (ie. currview)
      viewptr->SetViewSize();
      viewptr->SetFocus();
   }
}

void MainFrame::ToggleShowScripts()
{
   showscripts = !showscripts;
   if (showscripts && showpatterns) {
      showpatterns = false;
      splitwin->Unsplit(patternctrl);
      splitwin->SplitVertically(scriptctrl, viewptr, dirwinwd);
   } else {
      if (splitwin->IsSplit()) {
         // hide left pane
         dirwinwd = splitwin->GetSashPosition();
         splitwin->Unsplit(scriptctrl);
      } else {
         splitwin->SplitVertically(scriptctrl, viewptr, dirwinwd);
      }
      // resize viewport (ie. currview)
      viewptr->SetViewSize();
      viewptr->SetFocus();
   }
}

void MainFrame::ChangePatternDir()
{
   // wxMac bug: 3rd parameter seems to be ignored!!!
   wxDirDialog dirdlg(this, _("Choose a new pattern folder"),
                      patterndir, wxDD_NEW_DIR_BUTTON);
   if ( dirdlg.ShowModal() == wxID_OK ) {
      wxString newdir = dirdlg.GetPath();
      if ( patterndir != newdir ) {
         patterndir = newdir;
         if ( showpatterns ) {
            // show new pattern directory
            SimplifyTree(patterndir, patternctrl->GetTreeCtrl(), patternctrl->GetRootId());
         }
      }
   }
}

void MainFrame::ChangeScriptDir()
{
   // wxMac bug: 3rd parameter seems to be ignored!!!
   wxDirDialog dirdlg(this, _("Choose a new script folder"),
                      scriptdir, wxDD_NEW_DIR_BUTTON);
   if ( dirdlg.ShowModal() == wxID_OK ) {
      wxString newdir = dirdlg.GetPath();
      if ( scriptdir != newdir ) {
         scriptdir = newdir;
         if ( showscripts ) {
            // show new script directory
            SimplifyTree(scriptdir, scriptctrl->GetTreeCtrl(), scriptctrl->GetRootId());
         }
      }
   }
}

// -----------------------------------------------------------------------------

// prefs functions:

void MainFrame::SetRandomFillPercentage()
{
   // update Random Fill menu item to show randomfill value
   wxMenuBar *mbar = GetMenuBar();
   if (mbar) {
      wxString randlabel;
      randlabel.Printf(_("Random Fill (%d%c)\tCtrl+5"), randomfill, '%');
      mbar->SetLabel(ID_RANDOM, randlabel);
   }
}

void MainFrame::SetMinimumWarp()
{
   // set minwarp depending on mindelay and maxdelay
   minwarp = 0;
   if (mindelay > 0) {
      int d = mindelay;
      minwarp--;
      while (d < maxdelay) {
         d *= 2;
         minwarp--;
      }
   }
}

void MainFrame::UpdateWarp()
{
   SetMinimumWarp();
   if (warp < minwarp) {
      warp = minwarp;
      curralgo->setIncrement(1);    // warp is <= 0
   } else if (warp > 0) {
      SetGenIncrement();            // in case qbasestep/hbasestep changed
   }
}

void MainFrame::ShowPrefsDialog()
{
   if (inscript || generating || viewptr->waitingforclick) return;
   
   if (ChangePrefs()) {
      // user hit OK button
   
      // selection color may have changed
      SetSelectionColor();

      // if maxpatterns was reduced then we may need to remove some paths
      while (numpatterns > maxpatterns) {
         numpatterns--;
         patternSubMenu->Delete( patternSubMenu->FindItemByPosition(numpatterns) );
      }

      // if maxscripts was reduced then we may need to remove some paths
      while (numscripts > maxscripts) {
         numscripts--;
         scriptSubMenu->Delete( scriptSubMenu->FindItemByPosition(numscripts) );
      }
      
      // randomfill might have changed
      SetRandomFillPercentage();
      
      // if mindelay/maxdelay changed then may need to change minwarp and warp
      UpdateWarp();
      
      // we currently don't allow user to edit prefs while generating,
      // but in case that changes:
      if (generating && warp < 0) {
         whentosee = 0;                // best to see immediately
      }
      
      // maxhashmem might have changed
      if (hashing) curralgo->setMaxMemory(maxhashmem);
      
      SavePrefs();
      UpdateEverything();
   }
}

// -----------------------------------------------------------------------------

// control functions:

void MainFrame::ChangeGoToStop()
{
   /* single go/stop button is not yet implemented!!!
   gostopbutt->SetBitmapLabel(tbBitmaps[stop_index]);
   gostopbutt->Refresh(false, NULL);
   gostopbutt->Update();
   gostopbutt->SetToolTip(_("Stop generating"));
   */
}

void MainFrame::ChangeStopToGo()
{
   /* single go/stop button is not yet implemented!!!
   gostopbutt->SetBitmapLabel(tbBitmaps[go_index]);
   gostopbutt->Refresh(false, NULL);
   gostopbutt->Update();
   gostopbutt->SetToolTip(_("Start generating"));
   */
}

bool MainFrame::SaveStartingPattern()
{
   if ( curralgo->getGeneration() > startgen ) {
      // don't do anything if current gen count > starting gen
      return true;
   }
   
   // save current rule, scale, location, step size and hashing option
   startrule = wxString(curralgo->getrule(), wxConvLocal);
   startmag = viewptr->GetMag();
   viewptr->GetPos(startx, starty);
   startwarp = warp;
   starthash = hashing;
   
   if ( !savestart ) {
      // no need to save pattern; ResetPattern will load currfile
      // (note that currfile == tempstart if pattern created via OpenClipboard)
      startfile.Clear();
      return true;
   }

   // save starting pattern in tempstart file
   if ( hashing ) {
      // much faster to save hlife pattern in a macrocell file
      const char *err = WritePattern(tempstart, MC_format, 0, 0, 0, 0);
      if (err) {
         statusptr->ErrorMessage(wxString(err,wxConvLocal));
         // don't allow user to continue generating
         return false;
      }
   } else {
      // can only save qlife pattern if edges are within getcell/setcell limits
      bigint top, left, bottom, right;
      curralgo->findedges(&top, &left, &bottom, &right);      
      if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
         statusptr->ErrorMessage(_("Starting pattern is outside +/- 10^9 boundary."));
         // don't allow user to continue generating
         return false;
      }
      int itop = top.toint();
      int ileft = left.toint();
      int ibottom = bottom.toint();
      int iright = right.toint();      
      // use XRLE format so the pattern's top left location and the current
      // generation count are stored in the file
      const char *err = WritePattern(tempstart, XRLE_format,
                                     itop, ileft, ibottom, iright);
      if (err) {
         statusptr->ErrorMessage(wxString(err,wxConvLocal));
         // don't allow user to continue generating
         return false;
      }
   }
   
   startfile = tempstart;   // ResetPattern will load tempstart
   return true;
}

void MainFrame::GoFaster()
{
   warp++;
   SetGenIncrement();
   // only need to refresh status bar
   UpdateStatus();
   if (generating && warp < 0) {
      whentosee -= statusptr->GetCurrentDelay();
   }
}

void MainFrame::GoSlower()
{
   if (warp > minwarp) {
      warp--;
      SetGenIncrement();
      // only need to refresh status bar
      UpdateStatus();
      if (generating && warp < 0) {
         whentosee += statusptr->GetCurrentDelay();
      }
   } else {
      wxBell();
   }
}

void MainFrame::GeneratePattern()
{
   if (generating || viewptr->drawingcells || viewptr->waitingforclick) {
      wxBell();
      return;
   }
   
   if (curralgo->isEmpty()) {
      statusptr->ErrorMessage(empty_pattern);
      return;
   }
   
   if (!SaveStartingPattern()) {
      return;
   }

   // for DisplayTimingInfo
   begintime = stopwatch->Time();
   begingen = curralgo->getGeneration().todouble();
   
   generating = true;               // avoid recursion
   ChangeGoToStop();
   wxGetApp().PollerReset();
   UpdateUserInterface(IsActive());
   
   if (warp < 0) {
      whentosee = stopwatch->Time() + statusptr->GetCurrentDelay();
   }
   int hypdown = 64;

   while (true) {
      if (warp < 0) {
         // slow down by only doing one gen every GetCurrentDelay() millisecs
         long currmsec = stopwatch->Time();
         if (currmsec >= whentosee) {
            curralgo->step();
            if (autofit) viewptr->FitInView(0);
            // don't call UpdateEverything() -- no need to update menu/tool/scroll bars
            UpdatePatternAndStatus();
            if (wxGetApp().Poller()->checkevents()) break;
            // add delay to current time rather than currmsec
            // otherwise pauses can occur in Win app???
            whentosee = stopwatch->Time() + statusptr->GetCurrentDelay();
         } else {
            // process events while we wait
            if (wxGetApp().Poller()->checkevents()) break;
            // don't hog CPU
            wxMilliSleep(1);     // keep small (ie. <= mindelay)
         }
      } else {
         // warp >= 0 so only show results every curralgo->getIncrement() gens
         curralgo->step();
         if (autofit) viewptr->FitInView(0);
         // don't call UpdateEverything() -- no need to update menu/tool/scroll bars
         UpdatePatternAndStatus();
         if (wxGetApp().Poller()->checkevents()) break;
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
   endtime = stopwatch->Time();
   endgen = curralgo->getGeneration().todouble();
   
   ChangeStopToGo();
   
   // display the final pattern
   if (autofit) viewptr->FitInView(0);
   UpdateEverything();
}

void MainFrame::StopGenerating()
{
   if (inscript) {
      PassKeyToScript(WXK_ESCAPE);
   } else if (generating) {
      wxGetApp().PollerInterrupt();
   }
}

void MainFrame::DisplayTimingInfo()
{
   if (viewptr->waitingforclick) return;
   if (generating) {
      endtime = stopwatch->Time();
      endgen = curralgo->getGeneration().todouble();
   }
   if (endtime > begintime) {
      double secs = (double)(endtime - begintime) / 1000.0;
      double gens = endgen - begingen;
      wxString s;
      s.Printf(_("%g gens in %g secs (%g gens/sec)"), gens, secs, gens / secs);
      statusptr->DisplayMessage(s);
   }
}

void MainFrame::AdvanceOutsideSelection()
{
   if (generating || viewptr->drawingcells || viewptr->waitingforclick) return;

   if (!viewptr->SelectionExists()) {
      statusptr->ErrorMessage(no_selection);
      return;
   }

   if (curralgo->isEmpty()) {
      statusptr->ErrorMessage(empty_outside);
      return;
   }
   
   bigint top, left, bottom, right;
   curralgo->findedges(&top, &left, &bottom, &right);

   // check if selection encloses entire pattern
   if ( viewptr->seltop <= top && viewptr->selbottom >= bottom &&
        viewptr->selleft <= left && viewptr->selright >= right ) {
      statusptr->ErrorMessage(empty_outside);
      return;
   }

   // check if selection is completely outside pattern edges;
   // can't do this if qlife because it uses gen parity to decide which bits to draw
   if ( hashing &&
         ( viewptr->seltop > bottom || viewptr->selbottom < top ||
           viewptr->selleft > right || viewptr->selright < left ) ) {
      generating = true;
      ChangeGoToStop();
      wxGetApp().PollerReset();

      // step by one gen without changing gen count
      bigint savegen = curralgo->getGeneration();
      bigint saveinc = curralgo->getIncrement();
      curralgo->setIncrement(1);
      curralgo->step();
      curralgo->setIncrement(saveinc);
      curralgo->setGeneration(savegen);
   
      generating = false;
      ChangeStopToGo();
      
      // if pattern expanded then may need to clear ONE edge of selection!!!
      viewptr->ClearSelection();
      UpdateEverything();
      return;
   }

   // check that pattern is within setcell/getcell limits
   if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
      statusptr->ErrorMessage(_("Pattern is outside +/- 10^9 boundary."));
      return;
   }
   
   // create a new universe of same type
   lifealgo *newalgo;
   if ( hashing ) {
      newalgo = new hlifealgo();
      newalgo->setMaxMemory(maxhashmem);
   } else {
      newalgo = new qlifealgo();
   }
   newalgo->setpoll(wxGetApp().Poller());
   newalgo->setGeneration( curralgo->getGeneration() );
   
   // copy (and kill) live cells in selection to new universe
   int iseltop = viewptr->seltop.toint();
   int iselleft = viewptr->selleft.toint();
   int iselbottom = viewptr->selbottom.toint();
   int iselright = viewptr->selright.toint();
   if ( !viewptr->CopyRect(iseltop, iselleft, iselbottom, iselright,
                           curralgo, newalgo, true, _("Saving and erasing selection")) ) {
      // aborted, so best to restore selection
      if ( !newalgo->isEmpty() ) {
         newalgo->findedges(&top, &left, &bottom, &right);
         viewptr->CopyRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                           newalgo, curralgo, false, _("Restoring selection"));
      }
      delete newalgo;
      UpdateEverything();
      return;
   }
   
   // advance current universe by 1 generation
   generating = true;
   ChangeGoToStop();
   wxGetApp().PollerReset();
   curralgo->setIncrement(1);
   curralgo->step();
   generating = false;
   ChangeStopToGo();
   
   // note that we have to copy advanced pattern to new universe because
   // qlife uses gen parity to decide which bits to draw
   
   if ( !curralgo->isEmpty() ) {
      // find new edges and copy current pattern to new universe,
      // except for any cells that were created in selection
      curralgo->findedges(&top, &left, &bottom, &right);
      int itop = top.toint();
      int ileft = left.toint();
      int ibottom = bottom.toint();
      int iright = right.toint();
      int ht = ibottom - itop + 1;
      int cx, cy;
   
      // for showing accurate progress we need to add pattern height to pop count
      // in case this is a huge pattern with many blank rows
      double maxcount = curralgo->getPopulation().todouble() + ht;
      double accumcount = 0;
      int currcount = 0;
      bool abort = false;
      BeginProgress(_("Copying advanced pattern"));
   
      for ( cy=itop; cy<=ibottom; cy++ ) {
         currcount++;
         for ( cx=ileft; cx<=iright; cx++ ) {
            int skip = curralgo->nextcell(cx, cy);
            if (skip >= 0) {
               // found next live cell in this row
               cx += skip;
               
               // only copy cell if outside selection
               if ( cx < iselleft || cx > iselright ||
                    cy < iseltop || cy > iselbottom ) {
                  newalgo->setcell(cx, cy, 1);
               }
               
               currcount++;
            } else {
               cx = iright;  // done this row
            }
            if (currcount > 1024) {
               accumcount += currcount;
               currcount = 0;
               abort = AbortProgress(accumcount / maxcount, wxEmptyString);
               if (abort) break;
            }
         }
         if (abort) break;
      }
      
      newalgo->endofpattern();
      EndProgress();
   }
   
   // switch to new universe (best to do this even if aborted)
   savestart = true;
   delete curralgo;
   curralgo = newalgo;
   SetGenIncrement();
   UpdateEverything();
}

void MainFrame::AdvanceSelection()
{
   if (generating || viewptr->drawingcells || viewptr->waitingforclick) return;

   if (!viewptr->SelectionExists()) {
      statusptr->ErrorMessage(no_selection);
      return;
   }

   if (curralgo->isEmpty()) {
      statusptr->ErrorMessage(empty_selection);
      return;
   }
   
   bigint top, left, bottom, right;
   curralgo->findedges(&top, &left, &bottom, &right);

   // check if selection is completely outside pattern edges
   if ( viewptr->seltop > bottom || viewptr->selbottom < top ||
        viewptr->selleft > right || viewptr->selright < left ) {
      statusptr->ErrorMessage(empty_selection);
      return;
   }

   // check if selection encloses entire pattern;
   // can't do this if qlife because it uses gen parity to decide which bits to draw
   if ( hashing &&
        viewptr->seltop <= top && viewptr->selbottom >= bottom &&
        viewptr->selleft <= left && viewptr->selright >= right ) {
      generating = true;
      ChangeGoToStop();
      wxGetApp().PollerReset();

      // step by one gen without changing gen count
      bigint savegen = curralgo->getGeneration();
      bigint saveinc = curralgo->getIncrement();
      curralgo->setIncrement(1);
      curralgo->step();
      curralgo->setIncrement(saveinc);
      curralgo->setGeneration(savegen);
   
      generating = false;
      ChangeStopToGo();
      
      // only need to clear 1-cell thick strips just outside selection!!!
      viewptr->ClearOutsideSelection();
      UpdateEverything();
      return;
   }
   
   // find intersection of selection and pattern to minimize work
   if (viewptr->seltop > top) top = viewptr->seltop;
   if (viewptr->selleft > left) left = viewptr->selleft;
   if (viewptr->selbottom < bottom) bottom = viewptr->selbottom;
   if (viewptr->selright < right) right = viewptr->selright;

   // check that intersection is within setcell/getcell limits
   if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
      statusptr->ErrorMessage(selection_too_big);
      return;
   }
   
   // create a new temporary universe
   lifealgo *tempalgo;
   tempalgo = new qlifealgo();         // qlife's setcell/getcell are faster
   tempalgo->setpoll(wxGetApp().Poller());
   
   // copy live cells in selection to temporary universe
   if ( viewptr->CopyRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                          curralgo, tempalgo, false, _("Saving selection")) ) {
      if ( tempalgo->isEmpty() ) {
         statusptr->ErrorMessage(empty_selection);
      } else {
         // advance temporary universe by one gen
         generating = true;
         ChangeGoToStop();
         wxGetApp().PollerReset();
         tempalgo->setIncrement(1);
         tempalgo->step();
         generating = false;
         ChangeStopToGo();
         
         // temporary pattern might have expanded
         bigint temptop, templeft, tempbottom, tempright;
         tempalgo->findedges(&temptop, &templeft, &tempbottom, &tempright);
         if (temptop < top) top = temptop;
         if (templeft < left) left = templeft;
         if (tempbottom > bottom) bottom = tempbottom;
         if (tempright > right) right = tempright;

         // but ignore live cells created outside selection edges
         if (top < viewptr->seltop) top = viewptr->seltop;
         if (left < viewptr->selleft) left = viewptr->selleft;
         if (bottom > viewptr->selbottom) bottom = viewptr->selbottom;
         if (right > viewptr->selright) right = viewptr->selright;
         
         // copy all cells in new selection from tempalgo to curralgo
         viewptr->CopyAllRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                              tempalgo, curralgo, _("Copying advanced selection"));
         savestart = true;

         UpdateEverything();
      }
   }
   
   delete tempalgo;
}

void MainFrame::NextGeneration(bool useinc)
{
   if (generating || viewptr->drawingcells || viewptr->waitingforclick) {
      // don't play sound here because it'll be heard if user holds down tab key
      // wxBell();
      return;
   }

   if (curralgo->isEmpty()) {
      statusptr->ErrorMessage(empty_pattern);
      return;
   }
   
   if (!SaveStartingPattern()) {
      return;
   }
      
   // curralgo->step() calls checkevents so set generating flag to avoid recursion
   generating = true;
   
   // avoid doing some things if NextGeneration is called from a script;
   // note in particular that RunScript calls PollerReset which sets nextcheck to 0
   if (!inscript) {
      ChangeGoToStop();
      wxGetApp().PollerReset();
      viewptr->CheckCursor(IsActive());
   }

   if (useinc) {
      // step by current increment
      if (curralgo->getIncrement() > bigint::one && !inscript) {
         UpdateToolBar(IsActive());
         UpdateMenuItems(IsActive());
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

   if (!inscript) {
      ChangeStopToGo();
      // autofit is only used when doing many gens
      if (autofit && useinc && curralgo->getIncrement() > bigint::one)
         viewptr->FitInView(0);
      UpdateEverything();
   }
}

void MainFrame::ToggleAutoFit()
{
   autofit = !autofit;
   // we only use autofit when generating; that's why the Auto Fit item
   // is in the Control menu and not in the View menu
   if (autofit && generating) {
      viewptr->FitInView(0);
      UpdateEverything();
   }
}

void MainFrame::ToggleHashing()
{
   if (generating) return;

   if ( global_liferules.hasB0notS8 && !hashing ) {
      statusptr->ErrorMessage(_("Hashing cannot be used with a B0-not-S8 rule."));
      return;
   }

   // check if current pattern is too big to use getcell/setcell
   bigint top, left, bottom, right;
   if ( !curralgo->isEmpty() ) {
      curralgo->findedges(&top, &left, &bottom, &right);
      if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
         statusptr->ErrorMessage(_("Pattern cannot be converted (outside +/- 10^9 boundary)."));
         // ask user if they want to continue anyway???
         return;
      }
   }

   // toggle hashing option and update status bar immediately
   hashing = !hashing;
   warp = 0;
   UpdateStatus();

   // create a new universe of the right flavor
   lifealgo *newalgo;
   if ( hashing ) {
      newalgo = new hlifealgo();
      newalgo->setMaxMemory(maxhashmem);
   } else {
      newalgo = new qlifealgo();
   }
   newalgo->setpoll(wxGetApp().Poller());
   
   // even though universes share a global rule table we still need to call setrule
   // due to internal differences in the handling of Wolfram rules
   newalgo->setrule( (char*)curralgo->getrule() );
   
   // set same gen count
   newalgo->setGeneration( curralgo->getGeneration() );

   if ( !curralgo->isEmpty() ) {
      // copy pattern in current universe to new universe
      int itop = top.toint();
      int ileft = left.toint();
      int ibottom = bottom.toint();
      int iright = right.toint();
      int ht = ibottom - itop + 1;
      int cx, cy;
   
      // for showing accurate progress we need to add pattern height to pop count
      // in case this is a huge pattern with many blank rows
      double maxcount = curralgo->getPopulation().todouble() + ht;
      double accumcount = 0;
      int currcount = 0;
      bool abort = false;
      BeginProgress(_("Converting pattern"));
   
      for ( cy=itop; cy<=ibottom; cy++ ) {
         currcount++;
         for ( cx=ileft; cx<=iright; cx++ ) {
            int skip = curralgo->nextcell(cx, cy);
            if (skip >= 0) {
               // found next live cell in this row
               cx += skip;
               newalgo->setcell(cx, cy, 1);
               currcount++;
            } else {
               cx = iright;  // done this row
            }
            if (currcount > 1024) {
               accumcount += currcount;
               currcount = 0;
               abort = AbortProgress(accumcount / maxcount, wxEmptyString);
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
   UpdateEverything();
}

void MainFrame::ToggleHyperspeed()
{
   if ( curralgo->hyperCapable() ) {
      hyperspeed = !hyperspeed;
   }
}

void MainFrame::ToggleHashInfo()
{
   if ( curralgo->hyperCapable() ) {
      hlifealgo::setVerbose(!hlifealgo::getVerbose()) ;
   }
}

int MainFrame::GetWarp()
{
   return warp;
}

void MainFrame::SetWarp(int newwarp)
{
   warp = newwarp;
   if (warp < minwarp) warp = minwarp;
   SetGenIncrement();
}

void MainFrame::ShowRuleDialog()
{
   if (generating) return;
   if (ChangeRule()) {
      // show rule in window title (file name doesn't change)
      SetWindowTitle(wxEmptyString);
   }
}

// -----------------------------------------------------------------------------

// view functions:

void MainFrame::ResizeSplitWindow()
{
   int wd, ht;
   GetClientSize(&wd, &ht);

   splitwin->SetSize(0, statusptr->statusht, wd,
                     ht > statusptr->statusht ? ht - statusptr->statusht : 0);

   // wxSplitterWindow automatically resizes left and right panes
   // but we still need to resize viewport (ie. currview)
   viewptr->SetViewSize();

   #ifdef __WXGTK__
      // need to reset scroll bars
      viewptr->UpdateScrollBars();
   #endif
}

void MainFrame::ToggleStatusBar()
{
   int wd, ht;
   GetClientSize(&wd, &ht);
   if (StatusVisible()) {
      statusptr->statusht = 0;
      statusptr->SetSize(0, 0, 0, 0);
      #ifdef __WXX11__
         // move so we don't see small portion
         statusptr->Move(-100, -100);
      #endif
   } else {
      statusptr->statusht = showexact ? STATUS_EXHT : STATUS_HT;
      statusptr->SetSize(0, 0, wd, statusptr->statusht);
   }
   ResizeSplitWindow();
   UpdateEverything();
}

void MainFrame::ToggleExactNumbers()
{
   int wd, ht;
   GetClientSize(&wd, &ht);
   showexact = !showexact;
   if (StatusVisible()) {
      statusptr->statusht = showexact ? STATUS_EXHT : STATUS_HT;
      statusptr->SetSize(0, 0, wd, statusptr->statusht);
      ResizeSplitWindow();
      UpdateEverything();
   } else {
      // show the status bar using new size
      ToggleStatusBar();
   }
}

void MainFrame::ToggleToolBar()
{
   #ifdef __WXX11__
      // Show(false) does not hide tool bar!!!
      statusptr->ErrorMessage(_("Sorry, tool bar hiding is not implemented for X11."));
   #else
      wxToolBar *tbar = GetToolBar();
      if (tbar) {
         if (tbar->IsShown()) {
            tbar->Show(false);
         } else {
            tbar->Show(true);
         }
         int wd, ht;
         #ifdef __WXGTK__
            // wxGTK bug??? do a temporary size change to force origin to change -- sheesh
            GetSize(&wd, &ht);
            SetSize(wd-1, ht-1);
            SetSize(wd, ht);
         #endif
         GetClientSize(&wd, &ht);
         if (StatusVisible()) {
            // adjust size of status bar
            statusptr->SetSize(0, 0, wd, statusptr->statusht);
         }
         ResizeSplitWindow();
         UpdateEverything();
      }
   #endif
}

void MainFrame::ToggleFullScreen()
{
   #ifdef __WXX11__
      // ShowFullScreen(true) does nothing!!!
      statusptr->ErrorMessage(_("Sorry, full screen mode is not implemented for X11."));
   #else
      static bool restorestatus;    // restore status bar at end of full screen mode?
      static bool restoretoolbar;   // restore tool bar?
      static bool restorepattdir;   // restore pattern directory?
      static bool restorescrdir;    // restore script directory?

      if (!fullscreen) {
         // save current location and size for use in SavePrefs
         wxRect r = GetRect();
         mainx = r.x;
         mainy = r.y;
         mainwd = r.width;
         mainht = r.height;
      }

      fullscreen = !fullscreen;
      ShowFullScreen(fullscreen,
          // don't use wxFULLSCREEN_ALL because that prevents tool bar being
          // toggled in full screen mode on Windows
          wxFULLSCREEN_NOMENUBAR | wxFULLSCREEN_NOBORDER | wxFULLSCREEN_NOCAPTION);
      wxToolBar *tbar = GetToolBar();

      if (fullscreen) {
         // hide scroll bars
         viewptr->SetScrollbar(wxHORIZONTAL, 0, 0, 0, true);
         viewptr->SetScrollbar(wxVERTICAL, 0, 0, 0, true);
         // hide status bar if necessary
         restorestatus = StatusVisible();
         if (restorestatus) {
            statusptr->statusht = 0;
            statusptr->SetSize(0, 0, 0, 0);
         }
         // hide tool bar if necessary
         restoretoolbar = tbar && tbar->IsShown();
         if (restoretoolbar) {
            tbar->Show(false);
         }
         // hide pattern/script directory if necessary
         restorepattdir = showpatterns;
         restorescrdir = showscripts;
         if (restorepattdir) {
            dirwinwd = splitwin->GetSashPosition();
            splitwin->Unsplit(patternctrl);
            showpatterns = false;
         } else if (restorescrdir) {
            dirwinwd = splitwin->GetSashPosition();
            splitwin->Unsplit(scriptctrl);
            showscripts = false;
         }
      } else {
         // first show tool bar if necessary
         if (restoretoolbar && tbar && !tbar->IsShown()) {
            tbar->Show(true);
            if (StatusVisible()) {
               // reduce width of status bar below
               restorestatus = true;
            }
         }
         // now show status bar if necessary;
         // note that even if it's visible we may have to resize width
         if (restorestatus) {
            statusptr->statusht = showexact ? STATUS_EXHT : STATUS_HT;
            int wd, ht;
            GetClientSize(&wd, &ht);
            statusptr->SetSize(0, 0, wd, statusptr->statusht);
         }
         // now restore pattern/script directory if necessary
         if ( restorepattdir && !splitwin->IsSplit() ) {
            splitwin->SplitVertically(patternctrl, viewptr, dirwinwd);
            showpatterns = true;
         } else if ( restorescrdir && !splitwin->IsSplit() ) {
            splitwin->SplitVertically(scriptctrl, viewptr, dirwinwd);
            showscripts = true;
         }
      }

      if (!fullscreen) {
         // restore scroll bars BEFORE setting viewport size
         viewptr->UpdateScrollBars();
      }
      // adjust size of viewport (and pattern/script directory if visible)
      ResizeSplitWindow();
      UpdateEverything();
   #endif
}

void MainFrame::ShowPatternInfo()
{
   if (viewptr->waitingforclick || currfile.IsEmpty()) return;
   ShowInfo(currfile);
}

// -----------------------------------------------------------------------------

// event table and handlers for main window:

BEGIN_EVENT_TABLE(MainFrame, wxFrame)
   EVT_MENU                (wxID_ANY,        MainFrame::OnMenu)
   EVT_BUTTON              (wxID_ANY,        MainFrame::OnButton)
   EVT_SET_FOCUS           (                 MainFrame::OnSetFocus)
   EVT_ACTIVATE            (                 MainFrame::OnActivate)
   EVT_SIZE                (                 MainFrame::OnSize)
   EVT_IDLE                (                 MainFrame::OnIdle)
#ifdef __WXMAC__
   EVT_TREE_ITEM_EXPANDED  (wxID_TREECTRL,   MainFrame::OnDirTreeExpand)
   // wxMac bug??? EVT_TREE_ITEM_COLLAPSED doesn't get called
   EVT_TREE_ITEM_COLLAPSING(wxID_TREECTRL,   MainFrame::OnDirTreeCollapse)
#endif
   EVT_TREE_SEL_CHANGED    (wxID_TREECTRL,   MainFrame::OnDirTreeSelection)
   EVT_SPLITTER_DCLICK     (wxID_ANY,        MainFrame::OnSashDblClick)
   EVT_TIMER               (ID_ONE_TIMER,    MainFrame::OnOneTimer)
   EVT_CLOSE               (                 MainFrame::OnClose)
END_EVENT_TABLE()

void MainFrame::OnMenu(wxCommandEvent& event)
{
   showbanner = false;
   statusptr->ClearMessage();
   int id = event.GetId();
   switch (id)
   {
      // File menu
      case wxID_NEW:          NewPattern(); break;
      case wxID_OPEN:         OpenPattern(); break;
      case ID_OPEN_CLIP:      OpenClipboard(); break;
      case ID_CLEAR_PATTERNS: ClearRecentPatterns(); break;
      case ID_SHOW_PATTERNS:  ToggleShowPatterns(); break;
      case ID_PATTERN_DIR:    ChangePatternDir(); break;
      case wxID_SAVE:         SavePattern(); break;
      case ID_SAVE_XRLE:      savexrle = !savexrle; break;
      case ID_RUN_SCRIPT:     OpenScript(); break;
      case ID_RUN_CLIP:       RunClipboard(); break;
      case ID_CLEAR_SCRIPTS:  ClearRecentScripts(); break;
      case ID_SHOW_SCRIPTS:   ToggleShowScripts(); break;
      case ID_SCRIPT_DIR:     ChangeScriptDir(); break;
      case wxID_PREFERENCES:  ShowPrefsDialog(); break;
      case wxID_EXIT:         Close(true); break;        // true forces frame to close
      // Edit menu
      case ID_CUT:            viewptr->CutSelection(); break;
      case ID_COPY:           viewptr->CopySelection(); break;
      case ID_CLEAR:          viewptr->ClearSelection(); break;
      case ID_OUTSIDE:        viewptr->ClearOutsideSelection(); break;
      case ID_PASTE:          viewptr->PasteClipboard(false); break;
      case ID_PASTE_SEL:      viewptr->PasteClipboard(true); break;
      case ID_PL_TL:          SetPasteLocation("TopLeft"); break;
      case ID_PL_TR:          SetPasteLocation("TopRight"); break;
      case ID_PL_BR:          SetPasteLocation("BottomRight"); break;
      case ID_PL_BL:          SetPasteLocation("BottomLeft"); break;
      case ID_PL_MID:         SetPasteLocation("Middle"); break;
      case ID_PM_COPY:        SetPasteMode("Copy"); break;
      case ID_PM_OR:          SetPasteMode("Or"); break;
      case ID_PM_XOR:         SetPasteMode("Xor"); break;
      case ID_SELALL:         viewptr->SelectAll(); break;
      case ID_REMOVE:         viewptr->RemoveSelection(); break;
      case ID_SHRINK:         viewptr->ShrinkSelection(false); break;
      case ID_RANDOM:         viewptr->RandomFill(); break;
      case ID_FLIPUD:         viewptr->FlipUpDown(); break;
      case ID_FLIPLR:         viewptr->FlipLeftRight(); break;
      case ID_ROTATEC:        viewptr->RotateSelection(true); break;
      case ID_ROTATEA:        viewptr->RotateSelection(false); break;
      case ID_DRAW:           viewptr->SetCursorMode(curs_pencil); break;
      case ID_SELECT:         viewptr->SetCursorMode(curs_cross); break;
      case ID_MOVE:           viewptr->SetCursorMode(curs_hand); break;
      case ID_ZOOMIN:         viewptr->SetCursorMode(curs_zoomin); break;
      case ID_ZOOMOUT:        viewptr->SetCursorMode(curs_zoomout); break;
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
      case ID_HINFO:          ToggleHashInfo(); break;
      case ID_RULE:           ShowRuleDialog(); break;
      // View menu
      case ID_FULL:           ToggleFullScreen(); break;
      case ID_FIT:            viewptr->FitPattern(); break;
      case ID_FIT_SEL:        viewptr->FitSelection(); break;
      case ID_MIDDLE:         viewptr->ViewOrigin(); break;
      case ID_RESTORE00:      viewptr->RestoreOrigin(); break;
      case wxID_ZOOM_IN:      viewptr->ZoomIn(); break;
      case wxID_ZOOM_OUT:     viewptr->ZoomOut(); break;
      case ID_SCALE_1:        viewptr->SetPixelsPerCell(1); break;
      case ID_SCALE_2:        viewptr->SetPixelsPerCell(2); break;
      case ID_SCALE_4:        viewptr->SetPixelsPerCell(4); break;
      case ID_SCALE_8:        viewptr->SetPixelsPerCell(8); break;
      case ID_SCALE_16:       viewptr->SetPixelsPerCell(16); break;
      case ID_TOOL:           ToggleToolBar(); break;
      case ID_STATUS:         ToggleStatusBar(); break;
      case ID_EXACT:          ToggleExactNumbers(); break;
      case ID_GRID:           viewptr->ToggleGridLines(); break;
      case ID_COLORS:         viewptr->ToggleCellColors(); break;
      case ID_BUFF:           viewptr->ToggleBuffering(); break;
      case ID_INFO:           ShowPatternInfo(); break;
      // Help menu
      case ID_HELP_INDEX:     ShowHelp(_("Help/index.html")); break;
      case ID_HELP_INTRO:     ShowHelp(_("Help/intro.html")); break;
      case ID_HELP_TIPS:      ShowHelp(_("Help/tips.html")); break;
      case ID_HELP_SHORTCUTS: ShowHelp(_("Help/shortcuts.html")); break;
      case ID_HELP_SCRIPTING: ShowHelp(_("Help/scripting.html")); break;
      case ID_HELP_LEXICON:   ShowHelp(_("Help/Lexicon/lex.htm")); break;
      case ID_HELP_FILE:      ShowHelp(_("Help/file.html")); break;
      case ID_HELP_EDIT:      ShowHelp(_("Help/edit.html")); break;
      case ID_HELP_CONTROL:   ShowHelp(_("Help/control.html")); break;
      case ID_HELP_VIEW:      ShowHelp(_("Help/view.html")); break;
      case ID_HELP_HELP:      ShowHelp(_("Help/help.html")); break;
      case ID_HELP_REFS:      ShowHelp(_("Help/refs.html")); break;
      case ID_HELP_PROBLEMS:  ShowHelp(_("Help/problems.html")); break;
      case ID_HELP_CHANGES:   ShowHelp(_("Help/changes.html")); break;
      case ID_HELP_CREDITS:   ShowHelp(_("Help/credits.html")); break;
      case wxID_ABOUT:        ShowAboutBox(); break;
      default:
         if ( id > ID_OPEN_RECENT && id <= ID_OPEN_RECENT + numpatterns ) {
            OpenRecentPattern(id);
         }
         if ( id > ID_RUN_RECENT && id <= ID_RUN_RECENT + numscripts ) {
            OpenRecentScript(id);
         }
   }
   UpdateUserInterface(IsActive());
   
   // allow user interaction while running script
   if (inscript) {
      inscript = false;
      UpdatePatternAndStatus();
      inscript = true;
   }
}

void MainFrame::OnButton(wxCommandEvent& WXUNUSED(event))
{
   /* when we have a working go/stop button we may need code like this!!!
   showbanner = false;
   statusptr->ClearMessage();
   viewptr->SetFocus();
   if ( event.GetId() == ID_GO_STOP ) {
      if (generating) {
         StopGenerating();
      } else {
         GeneratePattern();
      }
   }
   */
}

void MainFrame::OnSetFocus(wxFocusEvent& WXUNUSED(event))
{
   // this is never called in Mac app, presumably because it doesn't
   // make any sense for a wxFrame to get the keyboard focus,
   // so we'll need a different solution!!!
   /*
   #ifdef __WXMAC__
      // avoid pattern/script directory getting keyboard focus
      if (viewptr) viewptr->SetFocus();
   #endif
   */

   #ifdef __WXMSW__
      // fix wxMSW bug: don't let main window get focus after being minimized
      if (viewptr) viewptr->SetFocus();
   #endif

   #ifdef __WXX11__
      // make sure viewport keeps keyboard focus whenever main window is active
      if (viewptr && IsActive()) viewptr->SetFocus();
      // fix problems after modal dialog or help window is closed
      UpdateUserInterface(IsActive());
   #endif
}

void MainFrame::OnActivate(wxActivateEvent& event)
{
   // this is never called in X11 app!!!
   // note that IsActive() doesn't always match event.GetActive()

   #ifdef __WXMAC__
      if (!event.GetActive()) wxSetCursor(*wxSTANDARD_CURSOR);
      // to avoid disabled menu items after a modal dialog closes
      // don't call UpdateMenuItems on deactivation
      if (event.GetActive()) UpdateUserInterface(true);
   #else
      UpdateUserInterface(event.GetActive());
   #endif
   
   #ifdef __WXGTK__
      if (event.GetActive()) onetimer->Start(20, wxTIMER_ONE_SHOT);
      // OnOneTimer will be called after delay of 0.02 secs
   #endif
   
   event.Skip();
}

#if defined(__WXX11__) || defined(__WXGTK__)
void MainFrame::OnSize(wxSizeEvent& event)
#else
void MainFrame::OnSize(wxSizeEvent& WXUNUSED(event))
#endif
{
   #ifdef __WXMSW__
      // save current location and size for use in SavePrefs if app
      // is closed when window is minimized
      wxRect r = GetRect();
      mainx = r.x;
      mainy = r.y;
      mainwd = r.width;
      mainht = r.height;
   #endif

   int wd, ht;
   GetClientSize(&wd, &ht);
   if (wd > 0 && ht > 0) {
      // note that statusptr and viewptr might be NULL if OnSize gets called
      // from MainFrame::MainFrame (true if X11)
      if (StatusVisible()) {
         // adjust size of status bar
         statusptr->SetSize(0, 0, wd, statusptr->statusht);
      }
      if (viewptr && statusptr && ht > statusptr->statusht) {
         // adjust size of viewport (and pattern/script directory if visible)
         ResizeSplitWindow();
      }
   }
   #if defined(__WXX11__) || defined(__WXGTK__)
      // need to do default processing for menu bar and tool bar
      event.Skip();
   #endif
}

void MainFrame::OnIdle(wxIdleEvent& WXUNUSED(event))
{
   #ifdef __WXX11__
      // don't change focus here because it prevents menus staying open
      return;
   #endif
   
   // ensure viewport window has keyboard focus if main window is active
   if ( IsActive() && viewptr ) viewptr->SetFocus();

   #ifdef __WXMSW__
      if ( callUnselect ) {
         // deselect file/folder so user can click the same item
         if (showpatterns) patternctrl->GetTreeCtrl()->Unselect();
         if (showscripts) scriptctrl->GetTreeCtrl()->Unselect();
         callUnselect = false;
      }
   #endif
}

void MainFrame::OnDirTreeExpand(wxTreeEvent& WXUNUSED(event))
{
   if ((generating || inscript) && (showpatterns || showscripts)) {
      // send idle event so directory tree gets updated
      wxIdleEvent idleevent;
      wxGetApp().SendIdleEvents(this, idleevent);
   }
}

void MainFrame::OnDirTreeCollapse(wxTreeEvent& WXUNUSED(event))
{
   if ((generating || inscript) && (showpatterns || showscripts)) {
      // send idle event so directory tree gets updated
      wxIdleEvent idleevent;
      wxGetApp().SendIdleEvents(this, idleevent);
   }
}

void MainFrame::OnDirTreeSelection(wxTreeEvent& event)
{
   // note that viewptr will be NULL if called from MainFrame::MainFrame
   if ( viewptr ) {
      wxTreeItemId id = event.GetItem();
      if ( !id.IsOk() ) return;

      wxGenericDirCtrl* dirctrl = NULL;
      if (showpatterns) dirctrl = patternctrl;
      if (showscripts) dirctrl = scriptctrl;
      if (dirctrl == NULL) return;
      
      wxString filepath = dirctrl->GetFilePath();

      // deselect file/folder so this handler will be called if user clicks same item
      wxTreeCtrl* treectrl = dirctrl->GetTreeCtrl();
      #ifdef __WXMSW__
         // can't call UnselectAll() or Unselect() here
      #else
         treectrl->UnselectAll();
      #endif

      if ( filepath.IsEmpty() ) {
         // user clicked on a folder name so expand or collapse it???
         // unfortunately, using Collapse/Expand causes this handler to be
         // called again and there's no easy way to distinguish between
         // a click in the folder name or a dbl-click (or a click in the
         // +/-/arrow image)
         /*
         if ( treectrl->IsExpanded(id) ) {
            treectrl->Collapse(id);
         } else {
            treectrl->Expand(id);
         }
         */
      } else {
         // user clicked on a file name
         if ( inscript ) {
            // use Warning because statusptr->ErrorMessage does nothing if inscript
            if ( showpatterns )
               Warning(_("Cannot load pattern while a script is running."));
            else
               Warning(_("Cannot run script while another one is running."));
         } else if ( generating ) {
            if ( showpatterns )
               statusptr->ErrorMessage(_("Cannot load pattern while generating."));
            else
               statusptr->ErrorMessage(_("Cannot run script while generating."));
         } else {
            // reset background of previously selected file by traversing entire tree;
            // we can't just remember previously selected id because ids don't persist
            // after a folder has been collapsed and expanded
            DeselectTree(treectrl, treectrl->GetRootItem());

            // indicate the selected file
            treectrl->SetItemBackgroundColour(id, *wxLIGHT_GREY);
            
            #ifdef __WXX11__
               // needed for scripts like goto.py which prompt user to enter string
               viewptr->SetFocus();
            #endif
            
            // load pattern or run script
            OpenFile(filepath);
         }
      }

      #ifdef __WXMSW__
         // calling Unselect() here causes a crash so do later in OnIdle
         callUnselect = true;
      #endif

      // changing focus here works on X11 but not on Mac (presumably because
      // wxMac sets focus to treectrl after this call)
      viewptr->SetFocus();
   }
}

void MainFrame::OnSashDblClick(wxSplitterEvent& WXUNUSED(event))
{
   // splitwin's sash was double-clicked
   if (showpatterns) ToggleShowPatterns();
   if (showscripts) ToggleShowScripts();
   UpdateMenuItems(IsActive());
   UpdateToolBar(IsActive());
}

void MainFrame::OnOneTimer(wxTimerEvent& WXUNUSED(event))
{
   // fix drag and drop problem on Mac -- see DnDFile::OnDropFiles
   #ifdef __WXMAC__
      // remove colored frame
      if (viewptr) viewptr->Refresh(false, NULL);
   #endif
   
   // fix menu item problem on Linux after modal dialog has closed
   #ifdef __WXGTK__
      UpdateMenuItems(true);
   #endif
}

void MainFrame::OnClose(wxCloseEvent& WXUNUSED(event))
{
   if (GetHelpFrame()) GetHelpFrame()->Close(true);
   if (GetInfoFrame()) GetInfoFrame()->Close(true);
   
   if (splitwin->IsSplit()) dirwinwd = splitwin->GetSashPosition();
   
   #ifndef __WXMAC__
      // if script is running we need to call exit below
      bool wasinscript = inscript;
   #endif

   // abort any running script and tidy up; also restores current directory
   // to location of Golly app so prefs file will be saved in correct place
   FinishScripting();

   // save main window location and other user preferences
   SavePrefs();
   
   // delete any temporary files
   if (wxFileExists(tempstart)) wxRemoveFile(tempstart);
   if (wxFileExists(scriptfile)) wxRemoveFile(scriptfile);

   #ifndef __WXMAC__
      // avoid error message on Windows or seg fault on Linux
      if (wasinscript) exit(0);
   #endif

   #if defined(__WXX11__) || defined(__WXGTK__)
      // avoid seg fault on Linux
      if (generating) exit(0);
   #else
      if (generating) StopGenerating();
   #endif
   
   Destroy();
}

// -----------------------------------------------------------------------------

// note that drag and drop is not supported by wxX11

#if wxUSE_DRAG_AND_DROP

// derive a simple class for handling dropped files
class DnDFile : public wxFileDropTarget
{
public:
   DnDFile() {}
   virtual bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames);
};

bool DnDFile::OnDropFiles(wxCoord, wxCoord, const wxArrayString& filenames)
{
   if (mainptr->generating) return false;

   // is there a wx call to bring app to front???
   #ifdef __WXMAC__
      ProcessSerialNumber process;
      if ( GetCurrentProcess(&process) == noErr )
         SetFrontProcess(&process);
   #endif
   #ifdef __WXMSW__
      SetForegroundWindow( (HWND)mainptr->GetHandle() );
   #endif
   mainptr->Raise();
   
   size_t numfiles = filenames.GetCount();
   for ( size_t n = 0; n < numfiles; n++ ) {
      mainptr->OpenFile(filenames[n]);
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

void MainFrame::CreateMenus()
{
   wxMenu *fileMenu = new wxMenu();
   wxMenu *editMenu = new wxMenu();
   wxMenu *controlMenu = new wxMenu();
   wxMenu *viewMenu = new wxMenu();
   wxMenu *helpMenu = new wxMenu();

   // create submenus
   wxMenu *plocSubMenu = new wxMenu();
   wxMenu *pmodeSubMenu = new wxMenu();
   wxMenu *cmodeSubMenu = new wxMenu();
   wxMenu *scaleSubMenu = new wxMenu();

   plocSubMenu->AppendCheckItem(ID_PL_TL, _("Top Left"));
   plocSubMenu->AppendCheckItem(ID_PL_TR, _("Top Right"));
   plocSubMenu->AppendCheckItem(ID_PL_BR, _("Bottom Right"));
   plocSubMenu->AppendCheckItem(ID_PL_BL, _("Bottom Left"));
   plocSubMenu->AppendCheckItem(ID_PL_MID, _("Middle"));

   pmodeSubMenu->AppendCheckItem(ID_PM_COPY, _("Copy"));
   pmodeSubMenu->AppendCheckItem(ID_PM_OR, _("Or"));
   pmodeSubMenu->AppendCheckItem(ID_PM_XOR, _("Xor"));

   cmodeSubMenu->AppendCheckItem(ID_DRAW, _("Draw\tF5"));
   cmodeSubMenu->AppendCheckItem(ID_SELECT, _("Select\tF6"));
   cmodeSubMenu->AppendCheckItem(ID_MOVE, _("Move\tF7"));
   cmodeSubMenu->AppendCheckItem(ID_ZOOMIN, _("Zoom In\tF8"));
   cmodeSubMenu->AppendCheckItem(ID_ZOOMOUT, _("Zoom Out\tF9"));

   scaleSubMenu->AppendCheckItem(ID_SCALE_1, _("1:1\tCtrl+1"));
   scaleSubMenu->AppendCheckItem(ID_SCALE_2, _("1:2\tCtrl+2"));
   scaleSubMenu->AppendCheckItem(ID_SCALE_4, _("1:4\tCtrl+4"));
   scaleSubMenu->AppendCheckItem(ID_SCALE_8, _("1:8\tCtrl+8"));
   scaleSubMenu->AppendCheckItem(ID_SCALE_16, _("1:16\tCtrl+6"));

   fileMenu->Append(wxID_NEW, _("New Pattern\tCtrl+N"));
   fileMenu->AppendSeparator();
   fileMenu->Append(wxID_OPEN, _("Open Pattern...\tCtrl+O"));
   fileMenu->Append(ID_OPEN_CLIP, _("Open Clipboard\tShift+Ctrl+O"));
   fileMenu->Append(ID_OPEN_RECENT, _("Open Recent"), patternSubMenu);
   fileMenu->AppendSeparator();
   fileMenu->AppendCheckItem(ID_SHOW_PATTERNS, _("Show Patterns\tCtrl+P"));
   fileMenu->Append(ID_PATTERN_DIR, _("Set Pattern Folder..."));
   fileMenu->AppendSeparator();
   fileMenu->Append(wxID_SAVE, _("Save Pattern...\tCtrl+S"));
   fileMenu->AppendCheckItem(ID_SAVE_XRLE, _("Save Extended RLE"));
   fileMenu->AppendSeparator();
   fileMenu->Append(ID_RUN_SCRIPT, _("Run Script..."));
   fileMenu->Append(ID_RUN_CLIP, _("Run Clipboard"));
   fileMenu->Append(ID_RUN_RECENT, _("Run Recent"), scriptSubMenu);
   fileMenu->AppendSeparator();
   fileMenu->AppendCheckItem(ID_SHOW_SCRIPTS, _("Show Scripts\tShift+Ctrl+P"));
   fileMenu->Append(ID_SCRIPT_DIR, _("Set Script Folder..."));
   fileMenu->AppendSeparator();
   #ifdef __WXMSW__
      // Windows doesn't support Ctrl+<non-alpha> menu shortcut, and best not to
      // use non-Ctrl shortcut because it can't be used when menu is disabled
      fileMenu->Append(wxID_PREFERENCES, _("Preferences..."));
   #else
      // on the Mac the Preferences item gets moved to the app menu
      fileMenu->Append(wxID_PREFERENCES, _("Preferences...\tCtrl+,"));
   #endif
   fileMenu->AppendSeparator();
   // on the Mac the Ctrl+Q is changed to Cmd-Q and the item is moved to the app menu
   #if wxCHECK_VERSION(2, 7, 1)
      fileMenu->Append(wxID_EXIT, wxGetStockLabel(wxID_EXIT));
   #else
      fileMenu->Append(wxID_EXIT, wxGetStockLabel(wxID_EXIT,true,_("Ctrl+Q")));
   #endif

   editMenu->Append(ID_CUT, _("Cut\tCtrl+X"));
   editMenu->Append(ID_COPY, _("Copy\tCtrl+C"));
   #ifdef __WXMSW__
      // avoid non-Ctrl shortcut because it can't be used when menu is disabled
      editMenu->Append(ID_CLEAR, _("Clear"));
      editMenu->Append(ID_OUTSIDE, _("Clear Outside"));
   #else
      editMenu->Append(ID_CLEAR, _("Clear\tDelete"));
      editMenu->Append(ID_OUTSIDE, _("Clear Outside\tShift+Delete"));
   #endif
   editMenu->AppendSeparator();
   editMenu->Append(ID_PASTE, _("Paste\tCtrl+V"));
   editMenu->Append(ID_PMODE, _("Paste Mode"), pmodeSubMenu);
   editMenu->Append(ID_PLOCATION, _("Paste Location"), plocSubMenu);
   editMenu->Append(ID_PASTE_SEL, _("Paste to Selection"));
   editMenu->AppendSeparator();
   editMenu->Append(ID_SELALL, _("Select All\tCtrl+A"));
   editMenu->Append(ID_REMOVE, _("Remove Selection\tCtrl+K"));
   editMenu->Append(ID_SHRINK, _("Shrink Selection"));
   // full label will be set later by SetRandomFillPercentage
   editMenu->Append(ID_RANDOM, _("Random Fill\tCtrl+5"));
   editMenu->Append(ID_FLIPUD, _("Flip Up-Down"));
   editMenu->Append(ID_FLIPLR, _("Flip Left-Right"));
   editMenu->Append(ID_ROTATEC, _("Rotate Clockwise"));
   editMenu->Append(ID_ROTATEA, _("Rotate Anticlockwise"));
   editMenu->AppendSeparator();
   editMenu->Append(ID_CMODE, _("Cursor Mode"), cmodeSubMenu);

   controlMenu->Append(ID_GO, _("Go\tCtrl+G"));
   #ifdef __WXMSW__
      // Windows doesn't support Ctrl+<non-alpha> menu shortcut, and best not to
      // use non-Ctrl shortcut because it can't be used when menu is disabled
      controlMenu->Append(ID_STOP, _("Stop"));
      controlMenu->Append(ID_NEXT, _("Next"));
      controlMenu->Append(ID_STEP, _("Next Step"));
   #else
      controlMenu->Append(ID_STOP, _("Stop\tCtrl+."));
      controlMenu->Append(ID_NEXT, _("Next\tSpace"));
      controlMenu->Append(ID_STEP, _("Next Step\tTab"));
   #endif
   controlMenu->Append(ID_RESET, _("Reset\tCtrl+R"));
   controlMenu->AppendSeparator();
   #ifdef __WXMSW__
      // Windows doesn't support Ctrl+<non-alpha> menu shortcut, and best not to
      // use non-Ctrl shortcut because it can't be used when menu is disabled
      controlMenu->Append(ID_FASTER, _("Faster"));
      controlMenu->Append(ID_SLOWER, _("Slower"));
   #else
      controlMenu->Append(ID_FASTER, _("Faster\tCtrl++"));
      controlMenu->Append(ID_SLOWER, _("Slower\tCtrl+-"));
   #endif
   controlMenu->AppendSeparator();
   controlMenu->AppendCheckItem(ID_AUTO, _("Auto Fit\tCtrl+T"));
   controlMenu->AppendCheckItem(ID_HASH, _("Use Hashing\tCtrl+U"));
   controlMenu->AppendCheckItem(ID_HYPER, _("Hyperspeed"));
   controlMenu->AppendCheckItem(ID_HINFO, _("Show Hash Info"));
   controlMenu->AppendSeparator();
   controlMenu->Append(ID_RULE, _("Rule..."));

   #ifdef __WXMAC__
      // F11 is a default activation key for Expose so use F1 instead
      viewMenu->Append(ID_FULL, _("Full Screen\tF1"));
   #else
      viewMenu->Append(ID_FULL, _("Full Screen\tF11"));
   #endif
   viewMenu->AppendSeparator();
   viewMenu->Append(ID_FIT, _("Fit Pattern\tCtrl+F"));
   viewMenu->Append(ID_FIT_SEL, _("Fit Selection\tShift+Ctrl+F"));
   viewMenu->Append(ID_MIDDLE, _("Middle\tCtrl+M"));
   viewMenu->Append(ID_RESTORE00, _("Restore Origin\tCtrl+9"));
   viewMenu->AppendSeparator();
   #ifdef __WXMSW__
      // Windows doesn't support Ctrl+<non-alpha> menu shortcut, and best not to
      // use non-Ctrl shortcut because it can't be used when menu is disabled
      viewMenu->Append(wxID_ZOOM_IN, _("Zoom In"));
      viewMenu->Append(wxID_ZOOM_OUT, _("Zoom Out"));
   #else
      viewMenu->Append(wxID_ZOOM_IN, _("Zoom In\tCtrl+]"));
      viewMenu->Append(wxID_ZOOM_OUT, _("Zoom Out\tCtrl+["));
   #endif
   viewMenu->Append(ID_SET_SCALE, _("Set Scale"), scaleSubMenu);
   viewMenu->AppendSeparator();
   #ifdef __WXMSW__
      // Windows doesn't support Ctrl+<non-alpha> menu shortcut, and best not to
      // use non-Ctrl shortcut because it can't be used when menu is disabled
      viewMenu->AppendCheckItem(ID_TOOL, _("Show Tool Bar"));
      viewMenu->AppendCheckItem(ID_STATUS, _("Show Status Bar"));
   #else
      viewMenu->AppendCheckItem(ID_TOOL, _("Show Tool Bar\tCtrl+'"));
      viewMenu->AppendCheckItem(ID_STATUS, _("Show Status Bar\tCtrl+;"));
   #endif
   viewMenu->AppendCheckItem(ID_EXACT, _("Show Exact Numbers\tCtrl+E"));
   viewMenu->AppendCheckItem(ID_GRID, _("Show Grid Lines\tCtrl+L"));
   viewMenu->AppendCheckItem(ID_COLORS, _("Swap Cell Colors\tCtrl+B"));
   viewMenu->AppendCheckItem(ID_BUFF, _("Buffered"));
   viewMenu->AppendSeparator();
   viewMenu->Append(ID_INFO, _("Pattern Info\tCtrl+I"));

   helpMenu->Append(ID_HELP_INDEX, _("Contents"));
   helpMenu->Append(ID_HELP_INTRO, _("Introduction"));
   helpMenu->Append(ID_HELP_TIPS, _("Hints and Tips"));
   helpMenu->Append(ID_HELP_SHORTCUTS, _("Shortcuts"));
   helpMenu->Append(ID_HELP_SCRIPTING, _("Scripting"));
   helpMenu->Append(ID_HELP_LEXICON, _("Life Lexicon"));
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
   // on the Mac the About item gets moved to the app menu
   helpMenu->Append(wxID_ABOUT, _("About Golly"));

   // create the menu bar and append menus
   wxMenuBar *menuBar = new wxMenuBar();
   if (menuBar == NULL) Fatal(_("Failed to create menu bar!"));
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
}

// -----------------------------------------------------------------------------

void MainFrame::CreateToolbar()
{
   #ifdef __WXX11__
      // creating vertical tool bar stuffs up X11 menu bar!!!
      wxToolBar *toolBar = CreateToolBar(wxTB_FLAT | wxNO_BORDER | wxTB_HORIZONTAL);
   #elif defined(__WXGTK__)
      // create vertical tool bar at left edge of frame
      wxToolBar *toolBar = CreateToolBar(wxTB_VERTICAL);
   #else
      // create vertical tool bar at left edge of frame
      wxToolBar *toolBar = CreateToolBar(wxTB_FLAT | wxNO_BORDER | wxTB_VERTICAL);
   #endif
   
   #ifdef __WXMAC__
      // this results in a tool bar that is 32 pixels wide (matches STATUS_HT)
      toolBar->SetMargins(4, 8);
   #elif defined(__WXMSW__)
      // Windows seems to ignore *any* margins
      toolBar->SetMargins(0, 0);
   #else
      // X11/GTK tool bar looks better with these margins
      toolBar->SetMargins(2, 2);
   #endif

   toolBar->SetToolBitmapSize(wxSize(16, 16));

   tbBitmaps[go_index] = wxBITMAP(play);
   tbBitmaps[stop_index] = wxBITMAP(stop);
   tbBitmaps[new_index] = wxBITMAP(new);
   tbBitmaps[open_index] = wxBITMAP(open);
   tbBitmaps[save_index] = wxBITMAP(save);
   tbBitmaps[patterns_index] = wxBITMAP(patterns);
   tbBitmaps[scripts_index] = wxBITMAP(scripts);
   tbBitmaps[draw_index] = wxBITMAP(draw);
   tbBitmaps[sel_index] = wxBITMAP(select);
   tbBitmaps[move_index] = wxBITMAP(move);
   tbBitmaps[zoomin_index] = wxBITMAP(zoomin);
   tbBitmaps[zoomout_index] = wxBITMAP(zoomout);
   tbBitmaps[info_index] = wxBITMAP(info);
   tbBitmaps[hash_index] = wxBITMAP(hash);

   #ifdef __WXX11__
      // reduce update probs by using toggle buttons
      #define ADD_TOOL(id, bmp, tooltip) \
         toolBar->AddCheckTool(id, wxEmptyString, bmp, wxNullBitmap, tooltip)
   #else
      #define ADD_TOOL(id, bmp, tooltip) \
         toolBar->AddTool(id, wxEmptyString, bmp, tooltip)
   #endif

   #define ADD_RADIO(id, bmp, tooltip) \
      toolBar->AddRadioTool(id, wxEmptyString, bmp, wxNullBitmap, tooltip)
   
   #define ADD_CHECK(id, bmp, tooltip) \
      toolBar->AddCheckTool(id, wxEmptyString, bmp, wxNullBitmap, tooltip)

   ADD_TOOL(ID_GO, tbBitmaps[go_index], _("Start generating"));
   ADD_TOOL(ID_STOP, tbBitmaps[stop_index], _("Stop generating"));
   ADD_CHECK(ID_HASH, tbBitmaps[hash_index], _("Toggle hashing"));
   toolBar->AddSeparator();
   ADD_TOOL(wxID_NEW, tbBitmaps[new_index], _("New pattern"));
   ADD_TOOL(wxID_OPEN, tbBitmaps[open_index], _("Open pattern"));
   ADD_TOOL(wxID_SAVE, tbBitmaps[save_index], _("Save pattern"));
   toolBar->AddSeparator();
   ADD_CHECK(ID_SHOW_PATTERNS, tbBitmaps[patterns_index], _("Show/hide patterns"));
   ADD_CHECK(ID_SHOW_SCRIPTS, tbBitmaps[scripts_index], _("Show/hide scripts"));
   toolBar->AddSeparator();
   ADD_RADIO(ID_DRAW, tbBitmaps[draw_index], _("Draw"));
   ADD_RADIO(ID_SELECT, tbBitmaps[sel_index], _("Select"));
   ADD_RADIO(ID_MOVE, tbBitmaps[move_index], _("Move"));
   ADD_RADIO(ID_ZOOMIN, tbBitmaps[zoomin_index], _("Zoom in"));
   ADD_RADIO(ID_ZOOMOUT, tbBitmaps[zoomout_index], _("Zoom out"));
   toolBar->AddSeparator();
   ADD_TOOL(ID_INFO, tbBitmaps[info_index], _("Pattern information"));
   
   toolBar->Realize();
}

// -----------------------------------------------------------------------------

void MainFrame::CreateDirControls()
{
   patternctrl = new wxGenericDirCtrl(splitwin, wxID_ANY, wxEmptyString,
                                      wxDefaultPosition, wxDefaultSize,
                                      #ifdef __WXMSW__
                                         // speed up a bit
                                         wxDIRCTRL_DIR_ONLY | wxNO_BORDER,
                                      #else
                                         wxNO_BORDER,
                                      #endif
                                      wxEmptyString   // see all file types
                                     );
   if (patternctrl == NULL) Fatal(_("Failed to create pattern directory control!"));

   scriptctrl = new wxGenericDirCtrl(splitwin, wxID_ANY, wxEmptyString,
                                     wxDefaultPosition, wxDefaultSize,
                                     #ifdef __WXMSW__
                                        // speed up a bit
                                        wxDIRCTRL_DIR_ONLY | wxNO_BORDER,
                                     #else
                                        wxNO_BORDER,
                                     #endif
                                     // using *.py prevents seeing a folder alias or
                                     // sym link so best to show all files???
                                     _T("Python scripts|*.py")
                                    );
   if (scriptctrl == NULL) Fatal(_("Failed to create script directory control!"));
   
   #ifdef __WXMSW__
      // now remove wxDIRCTRL_DIR_ONLY so we see files
      patternctrl->SetWindowStyle(wxNO_BORDER);
      scriptctrl->SetWindowStyle(wxNO_BORDER);
   #endif

   #ifdef __WXGTK__
      // make sure background is white when using KDE's GTK theme
      patternctrl->GetTreeCtrl()->SetBackgroundStyle(wxBG_STYLE_COLOUR);
      scriptctrl->GetTreeCtrl()->SetBackgroundStyle(wxBG_STYLE_COLOUR);
      patternctrl->GetTreeCtrl()->SetBackgroundColour(*wxWHITE);
      scriptctrl->GetTreeCtrl()->SetBackgroundColour(*wxWHITE);
      // reduce indent a bit
      patternctrl->GetTreeCtrl()->SetIndent(8);
      scriptctrl->GetTreeCtrl()->SetIndent(8);
   #else
      // reduce indent a lot
      patternctrl->GetTreeCtrl()->SetIndent(4);
      scriptctrl->GetTreeCtrl()->SetIndent(4);
   #endif
   
   // reduce font size -- doesn't seem to reduce line height
   // wxFont font = *(statusptr->GetStatusFont());
   // patternctrl->GetTreeCtrl()->SetFont(font);
   // scriptctrl->GetTreeCtrl()->SetFont(font);
   
   if ( wxFileName::DirExists(patterndir) ) {
      // only show patterndir and its contents
      SimplifyTree(patterndir, patternctrl->GetTreeCtrl(), patternctrl->GetRootId());
   }
   if ( wxFileName::DirExists(scriptdir) ) {
      // only show scriptdir and its contents
      SimplifyTree(scriptdir, scriptctrl->GetTreeCtrl(), scriptctrl->GetRootId());
   }
}

// -----------------------------------------------------------------------------

// create the main window
MainFrame::MainFrame()
   : wxFrame(NULL, wxID_ANY, wxEmptyString, wxPoint(mainx,mainy), wxSize(mainwd,mainht))
{
   wxGetApp().SetFrameIcon(this);

   // initialize hidden files to be in same folder as Golly app;
   // they must be absolute paths in case they are used from a script command when
   // the current directory has been changed to the location of the script file
   tempstart = gollydir + wxT(".golly_start");
   scriptfile = gollydir + wxT(".golly_clip.py");
   clipfile = gollydir + wxT(".golly_clipboard");

   // create one-shot timer (see OnOneTimer)
   onetimer = new wxTimer(this, ID_ONE_TIMER);

   CreateMenus();
   CreateToolbar();

   int wd, ht;
   GetClientSize(&wd, &ht);
   // wd or ht might be < 1 on Win/X11 platforms
   if (wd < 1) wd = 1;
   if (ht < 1) ht = 1;

   // wxStatusBar can only appear at bottom of frame so we use our own
   // status bar class which creates a child window at top of frame
   int statht = showexact ? STATUS_EXHT : STATUS_HT;
   statusptr = new StatusBar(this, 0, 0, wd, statht);
   if (statusptr == NULL) Fatal(_("Failed to create status bar!"));
   
   // create a split window with pattern/script directory in left pane
   // and pattern viewport in right pane
   splitwin = new wxSplitterWindow(this, wxID_ANY,
                                   wxPoint(0, statht),
                                   wxSize(wd, ht - statht),
                                   #ifdef __WXMSW__
                                      wxSP_BORDER |
                                   #endif
                                   wxSP_3DSASH | wxSP_NO_XP_THEME | wxSP_LIVE_UPDATE);
   if (splitwin == NULL) Fatal(_("Failed to create split window!"));

   // create patternctrl and scriptctrl
   CreateDirControls();
   
   // create viewport at minimum size to avoid scroll bars being clipped on Mac
   viewptr = new PatternView(splitwin, 0, 0, 40, 40);
   if (viewptr == NULL) Fatal(_("Failed to create viewport window!"));
   
   #if wxUSE_DRAG_AND_DROP
      // let users drop files onto viewport
      viewptr->SetDropTarget(new DnDFile());
   #endif
   
   // these seemingly redundant steps are needed to avoid problems on Windows
   splitwin->SplitVertically(patternctrl, viewptr, dirwinwd);
   splitwin->SetSashPosition(dirwinwd);
   splitwin->SetMinimumPaneSize(50);
   splitwin->Unsplit(patternctrl);
   splitwin->UpdateSize();

   splitwin->SplitVertically(scriptctrl, viewptr, dirwinwd);
   splitwin->SetSashPosition(dirwinwd);
   splitwin->SetMinimumPaneSize(50);
   splitwin->Unsplit(scriptctrl);
   splitwin->UpdateSize();

   if (showpatterns) splitwin->SplitVertically(patternctrl, viewptr, dirwinwd);
   if (showscripts) splitwin->SplitVertically(scriptctrl, viewptr, dirwinwd);

   InitDrawingData();      // do this after viewport size has been set

   generating = false;     // not generating pattern
   fullscreen = false;     // not in full screen mode
   showbanner = true;      // avoid first file clearing banner message
   savestart = false;      // no need to save starting pattern just yet
   startfile.Clear();      // no starting pattern
   startgen = 0;           // initial starting generation
   warp = 0;               // initial speed setting
}

MainFrame::~MainFrame()
{
   delete onetimer;
   DestroyDrawingData();
}
