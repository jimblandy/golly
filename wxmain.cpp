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

#include "wx/stockitem.h"  // for wxGetStockLabel
#include "wx/dnd.h"        // for wxFileDropTarget
#include "wx/file.h"       // for wxFile
#include "wx/filename.h"   // for wxFileName
#include "wx/menuitem.h"   // for SetText
#include "wx/clipbrd.h"    // for wxTheClipboard
#include "wx/dataobj.h"    // for wxTextDataObject

#ifdef __WXMSW__
   // tool bar bitmaps are loaded via .rc file
#else
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
#endif

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "readpattern.h"
#include "writepattern.h"

#include "wxgolly.h"       // for wxGetApp, statusptr, viewptr
#include "wxutils.h"       // for Warning, Fatal, etc
#include "wxprefs.h"       // for SavePrefs, etc
#include "wxrule.h"        // for ChangeRule, GetRuleName
#include "wxinfo.h"        // for ShowInfo
#include "wxhelp.h"        // for ShowHelp
#include "wxstatus.h"      // for statusptr->...
#include "wxview.h"        // for viewptr->...
#include "wxrender.h"      // for InitDrawingData, DestroyDrawingData
#include "wxmain.h"

#ifdef __WXMAC__
   #include <Carbon/Carbon.h>    // for GetCurrentProcess, etc
#endif

// -----------------------------------------------------------------------------

// IDs for timer and menu commands
enum {
   // one-shot timer
   ID_ONE_TIMER = wxID_HIGHEST,

   // File menu (see also wxID_NEW, wxID_OPEN, wxID_SAVE)
   ID_OPEN_CLIP,
   ID_RECENT,
   
   // last item in Open Recent submenu
   ID_RECENT_CLEAR = ID_RECENT + MAX_RECENT + 1,
   
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
   ID_FLIPV,
   ID_FLIPH,
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
   ID_RULE,
   
   // View menu (see also wxID_ZOOM_IN, wxID_ZOOM_OUT)
   ID_FULL,
   ID_FIT,
   ID_FIT_SEL,
   ID_MIDDLE,
   ID_RESTORE00,
   ID_SET_SCALE,
   ID_STATUS,
   ID_TOOL,
   ID_GRID,
   ID_VIDEO,
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

// static routines used by GetPrefs() to get IDs for items in Open Recent submenu;
// can't be MainFrame methods because GetPrefs() is called before creating main window
// and I'd rather not expose the IDs in the header file

int GetID_RECENT_CLEAR() { return ID_RECENT_CLEAR; }
int GetID_RECENT() { return ID_RECENT; }

// -----------------------------------------------------------------------------

// one-shot timer to fix wxMac bug;
// must be static because it's used in DnDFile::OnDropFiles
wxTimer *onetimer;

// temporary file created by OpenClipboard;
// it can be used to reset pattern or to show comments
const char gen0file[] = ".golly_gen0";

// -----------------------------------------------------------------------------

// update functions:

// update tool bar buttons according to the current state
void MainFrame::UpdateToolBar(bool active)
{
   wxToolBar *tbar = GetToolBar();
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
      if (viewptr->waitingforclick)
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

bool MainFrame::ClipboardHasText()
{
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

bool MainFrame::StatusVisible()
{
   return (statusptr && statusptr->statusht > 0);
}

// update menu bar items according to the current state
void MainFrame::UpdateMenuItems(bool active)
{
   wxMenuBar *mbar = GetMenuBar();
   wxToolBar *tbar = GetToolBar();
   bool textinclip = ClipboardHasText();
   if (mbar) {
      // disable most items if main window is inactive
      if (viewptr->waitingforclick)
         active = false;
      mbar->Enable(wxID_NEW,     active && !generating);
      mbar->Enable(wxID_OPEN,    active && !generating);
      mbar->Enable(ID_OPEN_CLIP, active && !generating && textinclip);
      mbar->Enable(ID_RECENT,    active && !generating && numrecent > 0);
      mbar->Enable(wxID_SAVE,    active && !generating);
      mbar->Enable(wxID_PREFERENCES, !generating);

      mbar->Enable(ID_CUT,       active && !generating && viewptr->SelectionExists());
      mbar->Enable(ID_COPY,      active && !generating && viewptr->SelectionExists());
      mbar->Enable(ID_CLEAR,     active && !generating && viewptr->SelectionExists());
      mbar->Enable(ID_OUTSIDE,   active && !generating && viewptr->SelectionExists());
      mbar->Enable(ID_PASTE,     active && !generating && textinclip);
      mbar->Enable(ID_PASTE_SEL, active && !generating &&
                                 viewptr->SelectionExists() && textinclip);
      mbar->Enable(ID_PLOCATION, active);
      mbar->Enable(ID_PMODE,     active);
      mbar->Enable(ID_SELALL,    active);
      mbar->Enable(ID_REMOVE,    active && viewptr->SelectionExists());
      mbar->Enable(ID_SHRINK,    active && viewptr->SelectionExists());
      mbar->Enable(ID_RANDOM,    active && !generating && viewptr->SelectionExists());
      mbar->Enable(ID_FLIPV,     active && !generating && viewptr->SelectionExists());
      mbar->Enable(ID_FLIPH,     active && !generating && viewptr->SelectionExists());
      mbar->Enable(ID_ROTATEC,   active && !generating && viewptr->SelectionExists());
      mbar->Enable(ID_ROTATEA,   active && !generating && viewptr->SelectionExists());
      mbar->Enable(ID_CMODE,     active);

      mbar->Enable(ID_GO,        active && !generating);
      mbar->Enable(ID_STOP,      active && generating);
      mbar->Enable(ID_NEXT,      active && !generating);
      mbar->Enable(ID_STEP,      active && !generating);
      mbar->Enable(ID_RESET,     active && !generating &&
                                 curralgo->getGeneration() > bigint::zero);
      mbar->Enable(ID_FASTER,    active);
      mbar->Enable(ID_SLOWER,    active && warp > minwarp);
      mbar->Enable(ID_AUTO,      active);
      mbar->Enable(ID_HASH,      active && !generating);
      mbar->Enable(ID_HYPER,     active && curralgo->hyperCapable());
      mbar->Enable(ID_RULE,      active && !generating);

      mbar->Enable(ID_FULL,      active);
      mbar->Enable(ID_FIT,       active);
      mbar->Enable(ID_FIT_SEL,   active && viewptr->SelectionExists());
      mbar->Enable(ID_MIDDLE,    active);
      mbar->Enable(ID_RESTORE00, active && (viewptr->originx != bigint::zero ||
                                            viewptr->originy != bigint::zero));
      mbar->Enable(wxID_ZOOM_IN, active && viewptr->GetMag() < MAX_MAG);
      mbar->Enable(wxID_ZOOM_OUT, active);
      mbar->Enable(ID_SET_SCALE, active);
      mbar->Enable(ID_STATUS,    active);
      mbar->Enable(ID_TOOL,      active);
      mbar->Enable(ID_GRID,      active);
      mbar->Enable(ID_VIDEO,     active);
      #ifdef __WXMAC__
         // windows on Mac OS X are automatically buffered
         mbar->Enable(ID_BUFF,   false);
         mbar->Check(ID_BUFF,    true);
      #else
         mbar->Enable(ID_BUFF,   active);
         mbar->Check(ID_BUFF,    buffered);
      #endif
      mbar->Enable(ID_INFO,      currfile[0] != 0);

      // tick/untick menu items created using AppendCheckItem
      mbar->Check(ID_AUTO,       autofit);
      mbar->Check(ID_HASH,       hashing);
      mbar->Check(ID_HYPER,      hyperspeed);
      mbar->Check(ID_STATUS,     StatusVisible());
      mbar->Check(ID_TOOL,       tbar && tbar->IsShown());
      mbar->Check(ID_GRID,       showgridlines);
      mbar->Check(ID_VIDEO,      blackcells);
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
   
   // update tool bar, menu bar and cursor
   UpdateUserInterface(IsActive());
}

// only update viewport and status bar
void MainFrame::UpdatePatternAndStatus()
{
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

const char B0message[] = "Hashing has been turned off due to B0-not-S8 rule.";

void MainFrame::MySetTitle(const char *title)
{
   #ifdef __WXMAC__
      if (FrontWindow() != NULL) {
         // avoid wxMac's SetTitle call -- it causes an undesirable window refresh
         Str255 ptitle;
         CopyCStringToPascal(title, ptitle);
         SetWTitle(FrontWindow(), (unsigned char const *)ptitle);
      } else {
         // this can happen before main window is shown
         SetTitle(title);
      }
   #else
      SetTitle(title);
   #endif
}

void MainFrame::SetWindowTitle(const char *filename)
{
   char wtitle[128];
   // save filename for use when changing rule
   strncpy(currname, filename, sizeof(currname));
#ifdef __WXMAC__
   sprintf(wtitle, "Golly: %s [%s]", filename, GetRuleName(curralgo->getrule()));
#else
   sprintf(wtitle, "%s [%s] - Golly", filename, GetRuleName(curralgo->getrule()));
#endif
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

void MainFrame::NewPattern()
{
   if (generating) return;
   savestart = false;
   currfile[0] = 0;
   warp = 0;
   CreateUniverse();

   if (initrule[0]) {
      // this is the first call of NewPattern when app starts
      const char *err = curralgo->setrule(initrule);
      if (err)
         Warning(err);
      if (global_liferules.hasB0notS8 && hashing) {
         hashing = false;
         statusptr->SetMessage(B0message);
         CreateUniverse();
      }
      initrule[0] = 0;        // don't use it again
   }

   // window title will also show curralgo->getrule()
   SetWindowTitle("untitled");

   if (newremovesel) viewptr->NoSelection();
   if (newcurs) currcurs = newcurs;
   viewptr->SetPosMag(bigint::zero, bigint::zero, newmag);

   // best to restore true origin
   if (viewptr->originx != bigint::zero || viewptr->originy != bigint::zero) {
      viewptr->originy = 0;
      viewptr->originx = 0;
      statusptr->SetMessage(origin_restored);
   }

   UpdateEverything();
}

void MainFrame::LoadPattern(const char *newtitle)
{
   // don't use initrule in future NewPattern calls
   initrule[0] = 0;
   if (newtitle) {
      savestart = false;
      warp = 0;
      if (GetInfoFrame()) {
         // comments will no longer be relevant so close info window
         GetInfoFrame()->Close(true);
      }
   }
   if (!showbanner) statusptr->ClearMessage();

   if (curralgo) {
      // delete old universe and set NULL so status bar shows gen=0 and pop=0
      delete curralgo;
      curralgo = NULL;
   }
   // update all of status bar so we don't see different colored lines
   UpdateStatus();
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

   viewptr->nopattupdate = true;
   const char *err = readpattern(currfile, *curralgo);
   if (err && strcmp(err,cannotreadhash) == 0 && !hashing) {
      hashing = true;
      statusptr->SetMessage("Hashing has been turned on for macrocell format.");
      // update all of status bar so we don't see different colored lines
      UpdateStatus();
      CreateUniverse();
      err = readpattern(currfile, *curralgo);
   } else if (global_liferules.hasB0notS8 && hashing && newtitle) {
      hashing = false;
      statusptr->SetMessage(B0message);
      // update all of status bar so we don't see different colored lines
      UpdateStatus();
      CreateUniverse();
      err = readpattern(currfile, *curralgo);
   }
   viewptr->nopattupdate = false;
   if (err) Warning(err);

   if (newtitle) {
      // show full window title after readpattern has set rule
      SetWindowTitle(newtitle);
      if (openremovesel) viewptr->NoSelection();
      if (opencurs) currcurs = opencurs;
      viewptr->FitInView(1);
      UpdateEverything();
      showbanner = false;
   } else {
      // ResetPattern sets rule, window title, scale and location
   }
}

void MainFrame::ResetPattern()
{
   if (generating || curralgo->getGeneration() == bigint::zero) return;
   
   if (gen0algo == NULL && currfile[0] == 0) {
      // if this happens then savestart logic is wrong
      Warning("Starting pattern cannot be restored!");
      return;
   }
   
   // restore pattern and settings saved by SaveStartingPattern;
   // first restore step size, hashing option and starting pattern
   warp = gen0warp;
   hashing = gen0hash;
   if (gen0algo) {
      // restore starting pattern saved in gen0algo
      delete curralgo;
      curralgo = gen0algo;
      gen0algo = NULL;
      savestart = true;
      SetGenIncrement();
      curralgo->setMaxMemory(maxhashmem);
      curralgo->setGeneration(bigint::zero);
   } else {
      // restore starting pattern from currfile;
      // pass in NULL so savestart, warp and currcurs won't change
      LoadPattern(NULL);
      // gen count has been reset to 0
   }
   // now restore rule, scale and location
   curralgo->setrule(gen0rule);
   SetWindowTitle(currname);
   viewptr->SetPosMag(gen0x, gen0y, gen0mag);
   UpdateEverything();
}

const char* MainFrame::GetBaseName(const char *fullpath)
{
   // extract basename from given path
   int len = strlen(fullpath);
   while (len > 0) {
      len--;
      if (fullpath[len] == wxFILE_SEP_PATH) {
         len++;
         break;
      }
   }
   return (const char *)&fullpath[len];
}

void MainFrame::SetCurrentFile(const char *path)
{
   #ifdef __WXMAC__
      // copy given path to currfile but with UTF8 encoding so fopen will work
      CFURLRef url = CFURLCreateWithBytes(NULL,
                                          (const UInt8*)path,
                                          strlen(path),
                                          kCFStringEncodingMacRoman,
                                          NULL);
      CFStringRef str = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
      CFRelease(url);
      CFStringGetCString(str, currfile, sizeof(currfile), kCFStringEncodingUTF8);
      CFRelease(str);
   #else
      strncpy(currfile, path, sizeof(currfile));
   #endif
}

void MainFrame::ConvertPathAndOpen(const char *path, bool convertUTF8)
{
   if (convertUTF8) {
      // on Mac we need to convert path to UTF8 so fopen will work
      SetCurrentFile(path);
   } else {
      strncpy(currfile, path, sizeof(currfile));
   }
   AddRecentFile(path);
   LoadPattern( GetBaseName(path) );
}

void MainFrame::AddRecentFile(const char *path)
{
   // put given path at start of recentSubMenu
   int id = recentSubMenu->FindItem(path);
   if ( id == wxNOT_FOUND ) {
      if ( numrecent < maxrecent ) {
         // add new path
         numrecent++;
         id = ID_RECENT + numrecent;
         recentSubMenu->Insert(numrecent - 1, id, path);
      } else {
         // replace last item with new path
         wxMenuItem *item = recentSubMenu->FindItemByPosition(maxrecent - 1);
         item->SetText(path);
         id = ID_RECENT + maxrecent;
      }
   }
   // path exists in recentSubMenu 
   if ( id > ID_RECENT + 1 ) {
      // move path to start of menu (item IDs don't change)
      wxMenuItem *item;
      while ( id > ID_RECENT + 1 ) {
         wxMenuItem *previtem = recentSubMenu->FindItem(id - 1);
         wxString prevpath = previtem->GetText();
         item = recentSubMenu->FindItem(id);
         item->SetText(prevpath);
         id--;
      }
      item = recentSubMenu->FindItem(id);
      item->SetText(path);
   }
}

void MainFrame::OpenPattern()
{
   if (generating) return;

   wxFileDialog opendlg(this, _("Choose a pattern file"),
                        opensavedir, wxEmptyString,
                        _T("All files (*)|*")
                        _T("|RLE (*.rle)|*.rle")
                        _T("|Life 1.05/1.06 (*.lif)|*.lif")
                        _T("|Macrocell (*.mc)|*.mc")
                        _T("|Gzip (*.gz)|*.gz"),
                        wxOPEN | wxFILE_MUST_EXIST);

   if ( opendlg.ShowModal() == wxID_OK ) {
      #ifdef __WXMAC__
         // Mac bug: need to update window now to avoid crash
         UpdateEverything();
      #endif
      wxFileName fullpath( opendlg.GetPath() );
      opensavedir = fullpath.GetPath();
      SetCurrentFile( opendlg.GetPath() );
      AddRecentFile( opendlg.GetPath() );
      LoadPattern( opendlg.GetFilename() );
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
            Warning("Could not write all data to clipboard file!");
            result = false;
         }
         tmpfile.Close();
      } else {
         Warning("Could not create clipboard file!");
         result = false;
      }
   #else
      if (wxTheClipboard->Open()) {
         if ( !wxTheClipboard->SetData(new wxTextDataObject(text)) ) {
            Warning("Could not copy text to clipboard!");
            result = false;
         }
         wxTheClipboard->Close();
      } else {
         Warning("Could not open clipboard!");
         result = false;
      }
   #endif
   return result;
}

bool MainFrame::GetTextFromClipboard(wxTextDataObject *data)
{
   bool gotdata = false;
   if ( wxTheClipboard->Open() ) {
      if ( wxTheClipboard->IsSupported( wxDF_TEXT ) ) {
         gotdata = wxTheClipboard->GetData( *data );
         if (!gotdata) {
            statusptr->ErrorMessage("Could not get clipboard data!");
         }
      } else {
         #ifdef __WXX11__
            statusptr->ErrorMessage("Sorry, but there is no clipboard support for X11.");
            // do X11 apps like xlife or fontforge have clipboard support???!!!
         #else
            statusptr->ErrorMessage("No text in clipboard.");
         #endif
      }
      wxTheClipboard->Close();
   } else {
      statusptr->ErrorMessage("Could not open clipboard!");
   }
   return gotdata;
}

void MainFrame::OpenClipboard()
{
   if (generating) return;
   // load and view pattern data stored in clipboard
   #ifdef __WXX11__
      // on X11 the clipboard data is in non-temporary clipfile, so copy
      // clipfile to gen0file (for use by ResetPattern and ShowPatternInfo)
      if ( wxCopyFile(clipfile, gen0file, true) ) {
         strncpy(currfile, gen0file, sizeof(currfile));
         LoadPattern("clipboard");
      } else {
         statusptr->ErrorMessage("Could not copy clipfile!");
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
            statusptr->ErrorMessage("Could not create gen0file!");
         }
      }
   #endif
}

void MainFrame::OpenRecentFile(int id)
{
   wxMenuItem *item = recentSubMenu->FindItem(id);
   if (item) {
      wxString path = item->GetText();
      SetCurrentFile(path);
      AddRecentFile(path);
      LoadPattern(GetBaseName(path));
   }
}

void MainFrame::ClearRecentFiles()
{
   while (numrecent > 0) {
      recentSubMenu->Delete( recentSubMenu->FindItemByPosition(0) );
      numrecent--;
   }
   wxMenuBar *mbar = GetMenuBar();
   if (mbar) mbar->Enable(ID_RECENT, false);
}

void MainFrame::SavePattern()
{
   if (generating) return;
   
   wxString filetypes;
   int RLEindex, L105index, MCindex;
   RLEindex = L105index = MCindex = -1;   // format not allowed (any -ve number)
   
   bigint top, left, bottom, right;
   int itop, ileft, ibottom, iright;
   curralgo->findedges(&top, &left, &bottom, &right);
   if (hashing) {
      if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
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
      if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
         statusptr->ErrorMessage("Pattern is outside +/- 10^9 boundary.");
         return;
      }   
      itop = top.toint();
      ileft = left.toint();
      ibottom = bottom.toint();
      iright = right.toint();
      // Life 1.05 format not yet implemented!!!
      // filetypes = _("RLE (*.rle)|*.rle|Life 1.05 (*.lif)|*.lif");
      filetypes = _("RLE (*.rle)|*.rle");
      RLEindex = 0;
      // Life 1.05 format not yet implemented!!!
      // L105index = 1;
   }

   wxFileDialog savedlg( this, _("Save pattern"),
                         opensavedir, wxEmptyString, filetypes,
                         wxSAVE | wxOVERWRITE_PROMPT );

   if ( savedlg.ShowModal() == wxID_OK ) {
      wxFileName fullpath( savedlg.GetPath() );
      opensavedir = fullpath.GetPath();
      wxString ext = fullpath.GetExt();
      pattern_format format;
      // if user supplied a known extension (rle/lif/mc) then use that format if
      // it is allowed, otherwise use current format specified in filter menu
      if ( ext.IsSameAs("rle",false) && RLEindex >= 0 ) {
         format = RLE_format;
      // Life 1.05 format not yet implemented!!!
      // } else if ( ext.IsSameAs("lif",false) && L105index >= 0 ) {
      //   format = L105_format;
      } else if ( ext.IsSameAs("mc",false) && MCindex >= 0 ) {
         format = MC_format;
      } else if ( savedlg.GetFilterIndex() == RLEindex ) {
         format = RLE_format;
      } else if ( savedlg.GetFilterIndex() == L105index ) {
         format = L105_format;
      } else if ( savedlg.GetFilterIndex() == MCindex ) {
         format = MC_format;
      } else {
         statusptr->ErrorMessage("Bug in SavePattern!");
         return;
      }
      SetCurrentFile( savedlg.GetPath() );
      AddRecentFile( savedlg.GetPath() );
      SetWindowTitle( savedlg.GetFilename() );
      const char *err = writepattern(savedlg.GetPath(), *curralgo, format,
                                     itop, ileft, ibottom, iright);
      if (err) {
         statusptr->ErrorMessage(err);
      } else {
         statusptr->DisplayMessage("Pattern saved in file.");
         if ( curralgo->getGeneration() == bigint::zero ) {
            // no need to save starting pattern (ResetPattern can load file)
            savestart = false;
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
      randlabel.Printf("Random Fill (%d%c)\tCtrl+5", randomfill, '%');
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

void MainFrame::ShowPrefsDialog()
{
   if (generating || viewptr->waitingforclick) return;
   
   if (ChangePrefs()) {
      // user hit OK button
   
      // if maxrecent was reduced then we may need to remove some paths
      while (numrecent > maxrecent) {
         numrecent--;
         recentSubMenu->Delete( recentSubMenu->FindItemByPosition(numrecent) );
      }
      
      // randomfill might have changed
      SetRandomFillPercentage();
      
      // if mindelay/maxdelay changed then may need to change minwarp and warp
      SetMinimumWarp();
      if (warp < minwarp) {
         warp = minwarp;
         curralgo->setIncrement(1);    // warp is <= 0
      } else if (warp > 0) {
         SetGenIncrement();            // in case qbasestep/hbasestep changed
      }
      
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
   /* doesn't work on Windows -- all the other tools go missing!!!
   // replace tool bar's go button with stop button
   wxToolBar *tbar = GetToolBar();
   if (tbar) {
      tbar->RemoveTool(ID_GO);
      tbar->InsertTool(0, stoptool);
      tbar->Realize();
   }
   */
}

void MainFrame::ChangeStopToGo()
{
   /* doesn't work on Windows!!!
   // replace tool bar's stop button with go button
   wxToolBar *tbar = GetToolBar();
   if (tbar) {
      tbar->RemoveTool(ID_STOP);
      tbar->InsertTool(0, gotool);
      tbar->Realize();
   }
   */
}

bool MainFrame::SaveStartingPattern()
{
   if ( curralgo->getGeneration() > bigint::zero ) {
      // don't save pattern if gen count > 0
      return true;
   }

   if ( gen0algo ) {
      // delete old starting pattern
      delete gen0algo;
      gen0algo = NULL;
   }
   
   // save current rule, scale, location, step size and hashing option
   strncpy(gen0rule, curralgo->getrule(), sizeof(gen0rule));
   gen0mag = viewptr->GetMag();
   viewptr->GetPos(gen0x, gen0y);
   gen0warp = warp;
   gen0hash = hashing;
   
   if ( !savestart ) {
      // no need to save pattern stored in currfile
      return true;
   }

   // only save pattern if its edges are within getcell/setcell limits
   bigint top, left, bottom, right;
   curralgo->findedges(&top, &left, &bottom, &right);
   if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
      statusptr->ErrorMessage("Starting pattern is outside +/- 10^9 boundary.");
      // ask user if they want to continue generating???
      return false;
   }   
   
   // create gen0algo and duplicate current pattern
   if ( hashing ) {
      gen0algo = new hlifealgo();
   } else {
      gen0algo = new qlifealgo();
   }
   gen0algo->setpoll(wxGetApp().Poller());

   // copy (non-empty) pattern in current universe to gen0algo
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
   BeginProgress("Saving starting pattern");

   for ( cy=itop; cy<=ibottom; cy++ ) {
      currcount++;
      for ( cx=ileft; cx<=iright; cx++ ) {
         int skip = curralgo->nextcell(cx, cy);
         if (skip >= 0) {
            // found next live cell in this row
            cx += skip;
            gen0algo->setcell(cx, cy, 1);
            currcount++;
         } else {
            cx = iright;  // done this row
         }
         if (currcount > 1024) {
            accumcount += currcount;
            currcount = 0;
            abort = AbortProgress(accumcount / maxcount, "");
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
   starttime = wxGetElapsedTime(false);
   startgen = curralgo->getGeneration().todouble();
   
   generating = true;               // avoid recursion
   ChangeGoToStop();
   wxGetApp().PollerReset();
   UpdateUserInterface(IsActive());
   
   if (warp < 0) {
      whentosee = wxGetElapsedTime(false) + statusptr->GetCurrentDelay();
   }
   int hypdown = 64;

   while (true) {
      if (warp < 0) {
         // slow down by only doing one gen every GetCurrentDelay() millisecs
         long currmsec = wxGetElapsedTime(false);
         if (currmsec >= whentosee) {
            curralgo->step();
            if (autofit) viewptr->FitInView(0);
            // don't call UpdateEverything() -- no need to update menu/tool/scroll bars
            UpdatePatternAndStatus();
            if (wxGetApp().Poller()->checkevents()) break;
            whentosee = currmsec + statusptr->GetCurrentDelay();
         } else {
            // process events while we wait
            if (wxGetApp().Poller()->checkevents()) break;
            // don't hog CPU
            wxMilliSleep(10);
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
   endtime = wxGetElapsedTime(false);
   endgen = curralgo->getGeneration().todouble();
   
   ChangeStopToGo();
   
   // display the final pattern
   if (autofit) viewptr->FitInView(0);
   UpdateEverything();
}

void MainFrame::StopGenerating()
{
   if (generating) wxGetApp().PollerInterrupt();
}

void MainFrame::DisplayTimingInfo()
{
   if (viewptr->waitingforclick) return;
   if (generating) {
      endtime = wxGetElapsedTime(false);
      endgen = curralgo->getGeneration().todouble();
   }
   if (endtime > starttime) {
      char s[128];
      sprintf(s,"%g gens in %g secs (%g gens/sec)",
                endgen - startgen,
                (double)(endtime - starttime) / 1000.0,
                (double)(endgen - startgen) / ((double)(endtime - starttime) / 1000.0));
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
      statusptr->ErrorMessage("Pattern is outside +/- 10^9 boundary.");
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
                           curralgo, newalgo, true, "Saving and erasing selection") ) {
      // aborted, so best to restore selection
      if ( !newalgo->isEmpty() ) {
         newalgo->findedges(&top, &left, &bottom, &right);
         viewptr->CopyRect(top.toint(), left.toint(), bottom.toint(), right.toint(),
                           newalgo, curralgo, false, "Restoring selection");
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
      BeginProgress("Copying advanced pattern");
   
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
               abort = AbortProgress(accumcount / maxcount, "");
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
                          curralgo, tempalgo, false, "Saving selection") ) {
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
                              tempalgo, curralgo, "Copying advanced selection");
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
   
   #ifdef __WXMSW__
      // on Windows we do these tests here because space is an accelerator
      // for ID_NEXT and so we get called from OnMenu;
      // NOTE: we avoid using wxGetKeyState on wxMac because the first call
      // causes a ridiculously long delay -- 2 secs on my 400MHz G4!!!
      if ( !useinc && wxGetKeyState(WXK_SHIFT) ) {
         AdvanceOutsideSelection();
         return;
      } else if ( !useinc && wxGetKeyState(WXK_CONTROL) ) {
         AdvanceSelection();
         return;
      }
   #endif

   if (curralgo->isEmpty()) {
      statusptr->ErrorMessage(empty_pattern);
      return;
   }
   
   if (!SaveStartingPattern()) {
      return;
   }
      
   // curralgo->step() calls checkevents so set generating flag to avoid recursion
   generating = true;
   ChangeGoToStop();
   wxGetApp().PollerReset();
   viewptr->CheckCursor(IsActive());

   if (useinc) {
      // step by current increment
      if (curralgo->getIncrement() > bigint::one) {
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
   ChangeStopToGo();
   
   // autofit is only used when doing many gens
   if (autofit && useinc && curralgo->getIncrement() > bigint::one)
      viewptr->FitInView(0);
   UpdateEverything();
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
      statusptr->ErrorMessage("Hashing cannot be used with a B0-not-S8 rule.");
      return;
   }

   // check if current pattern is too big to use getcell/setcell
   bigint top, left, bottom, right;
   if ( !curralgo->isEmpty() ) {
      curralgo->findedges(&top, &left, &bottom, &right);
      if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
         statusptr->ErrorMessage("Pattern cannot be converted (outside +/- 10^9 boundary).");
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
      BeginProgress("Converting pattern");
   
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
               abort = AbortProgress(accumcount / maxcount, "");
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
      // show rule in window title (current file name doesn't change)
      SetWindowTitle(currname);
   }
}

// -----------------------------------------------------------------------------

// view functions:

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
      statusptr->statusht = STATUS_HT;
      statusptr->SetSize(0, 0, wd, statusptr->statusht);
   }
   viewptr->SetSize(0, statusptr->statusht, wd,
                       ht > statusptr->statusht ? ht - statusptr->statusht : 0);
   viewptr->SetViewSize();
   UpdateEverything();
}

void MainFrame::ToggleToolBar()
{
   #ifdef __WXX11__
      // Show(false) does not hide tool bar!!!
      statusptr->ErrorMessage("Sorry, tool bar hiding is not implemented for X11.");
   #else
      wxToolBar *tbar = GetToolBar();
      if (tbar) {
         if (tbar->IsShown()) {
            tbar->Show(false);
         } else {
            tbar->Show(true);
         }
         int wd, ht;
         GetClientSize(&wd, &ht);
         if (StatusVisible()) {
            // adjust size of status bar
            statusptr->SetSize(0, 0, wd, statusptr->statusht);
         }
         // adjust size of viewport
         viewptr->SetSize(0, statusptr->statusht, wd,
                             ht > statusptr->statusht ? ht - statusptr->statusht : 0);
         viewptr->SetViewSize();
         UpdateEverything();
      }
   #endif
}

void MainFrame::ToggleFullScreen()
{
   #ifdef __WXX11__
      // ShowFullScreen(true) does nothing!!!
      statusptr->ErrorMessage("Sorry, full screen mode is not implemented for X11.");
   #else
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
         if (tbar && tbar->IsShown()) {
            tbar->Show(false);
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
            statusptr->statusht = STATUS_HT;
            int wd, ht;
            GetClientSize(&wd, &ht);
            statusptr->SetSize(0, 0, wd, statusptr->statusht);
         }
      }
      // adjust size of viewport
      int wd, ht;
      GetClientSize(&wd, &ht);
      viewptr->SetSize(0, statusptr->statusht, wd,
                          ht > statusptr->statusht ? ht - statusptr->statusht : 0);
      if (!fullscreen) {
         // restore scroll bars BEFORE setting viewport size
         viewptr->UpdateScrollBars();
      }
      viewptr->SetViewSize();
      UpdateEverything();
   #endif
}

void MainFrame::ShowPatternInfo()
{
   if (viewptr->waitingforclick || currfile[0] == 0) return;
   ShowInfo(currfile);
}

// -----------------------------------------------------------------------------

// event table and handlers for main window:

BEGIN_EVENT_TABLE(MainFrame, wxFrame)
   EVT_MENU       (wxID_ANY,     MainFrame::OnMenu)
   EVT_SET_FOCUS  (              MainFrame::OnSetFocus)
   EVT_ACTIVATE   (              MainFrame::OnActivate)
   EVT_SIZE       (              MainFrame::OnSize)
   EVT_TIMER      (ID_ONE_TIMER, MainFrame::OnOneTimer)
   EVT_CLOSE      (              MainFrame::OnClose)
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
      case ID_RECENT_CLEAR:   ClearRecentFiles(); break;
      case wxID_SAVE:         SavePattern(); break;
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
      case ID_FLIPV:          viewptr->FlipVertically(); break;
      case ID_FLIPH:          viewptr->FlipHorizontally(); break;
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
      case ID_STATUS:         ToggleStatusBar(); break;
      case ID_TOOL:           ToggleToolBar(); break;
      case ID_GRID:           viewptr->ToggleGridLines(); break;
      case ID_VIDEO:          viewptr->ToggleVideo(); break;
      case ID_BUFF:           viewptr->ToggleBuffering(); break;
      case ID_INFO:           ShowPatternInfo(); break;
      // Help menu
      case ID_HELP_INDEX:     ShowHelp("Help/index.html"); break;
      case ID_HELP_INTRO:     ShowHelp("Help/intro.html"); break;
      case ID_HELP_TIPS:      ShowHelp("Help/tips.html"); break;
      case ID_HELP_SHORTCUTS: ShowHelp("Help/shortcuts.html"); break;
      case ID_HELP_LEXICON:   ShowHelp("Help/Lexicon/lex.htm"); break;
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
      default:
         if ( id > ID_RECENT && id <= ID_RECENT + numrecent ) {
            OpenRecentFile(id);
         }
   }
   UpdateUserInterface(IsActive());
}

void MainFrame::OnSetFocus(wxFocusEvent& WXUNUSED(event))
{
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
   // this is never called on X11!!!
   // note that IsActive() doesn't always match event.GetActive()
   UpdateUserInterface(event.GetActive());
   if ( !event.GetActive() ) {
      wxSetCursor(*wxSTANDARD_CURSOR);
   }
   event.Skip();
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
      if (StatusVisible()) {
         // adjust size of status bar
         statusptr->SetSize(0, 0, wd, statusptr->statusht);
      }
      if (viewptr && statusptr && ht > statusptr->statusht) {
         // adjust size of viewport
         viewptr->SetSize(0, statusptr->statusht, wd, ht - statusptr->statusht);
         viewptr->SetViewSize();
      }
   }
   #ifdef __WXX11__
      // need to do default processing for X11 menu bar and tool bar
      event.Skip();
   #endif
}

void MainFrame::OnOneTimer(wxTimerEvent& WXUNUSED(event))
{
   // fix drag and drop problem on Mac -- see DnDFile::OnDropFiles
   #ifdef __WXMAC__
      // remove colored frame
      if (viewptr) viewptr->Refresh(false, NULL);
   #endif
}

void MainFrame::OnClose(wxCloseEvent& WXUNUSED(event))
{
   if (GetHelpFrame()) GetHelpFrame()->Close(true);
   if (GetInfoFrame()) GetInfoFrame()->Close(true);

   // save main window location and other user preferences
   SavePrefs();
   
   // delete any temporary files
   if (wxFileExists(gen0file)) wxRemoveFile(gen0file);

   #ifdef __WXX11__
      // avoid seg fault on X11
      if (generating) exit(0);
   #endif
   if (generating) StopGenerating();
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
   // need to process events to avoid crash if info window was in front
   while (wxGetApp().Pending()) wxGetApp().Dispatch();
   
   size_t numfiles = filenames.GetCount();
   for ( size_t n = 0; n < numfiles; n++ ) {
      mainptr->ConvertPathAndOpen(filenames[n], true);
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

// create the main window
MainFrame::MainFrame()
   : wxFrame(NULL, wxID_ANY, _(""), wxPoint(mainx,mainy), wxSize(mainwd,mainht))
{
   wxGetApp().SetFrameIcon(this);

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
   // F9 might be reserved in Mac OS 10.4???
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
   fileMenu->Append(ID_RECENT, _("Open Recent"), recentSubMenu);
   fileMenu->AppendSeparator();
   fileMenu->Append(wxID_SAVE, _("Save Pattern...\tCtrl+S"));
   fileMenu->AppendSeparator();
   #ifdef __WXMSW__
      // Windows doesn't support Ctrl+<non-alpha> menu shortcuts
      fileMenu->Append(wxID_PREFERENCES, _("Preferences...\t,"));
   #else
      // on the Mac the Preferences item gets moved to the app menu
      fileMenu->Append(wxID_PREFERENCES, _("Preferences...\tCtrl+,"));
   #endif
   fileMenu->AppendSeparator();
   // on the Mac the Alt+X is changed to Cmd-Q and the item is moved to the app menu
   fileMenu->Append(wxID_EXIT, wxGetStockLabel(wxID_EXIT,true,_T("Alt+X")));

   editMenu->Append(ID_CUT, _("Cut\tCtrl+X"));
   editMenu->Append(ID_COPY, _("Copy\tCtrl+C"));
   editMenu->Append(ID_CLEAR, _("Clear\tDelete"));
   editMenu->Append(ID_OUTSIDE, _("Clear Outside\tShift+Delete"));
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
   editMenu->Append(ID_FLIPV, _("Flip Vertically"));
   editMenu->Append(ID_FLIPH, _("Flip Horizontally"));
   editMenu->Append(ID_ROTATEC, _("Rotate Clockwise"));
   editMenu->Append(ID_ROTATEA, _("Rotate Anticlockwise"));
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
      // Windows doesn't support Ctrl+<non-alpha> menu shortcuts
      viewMenu->Append(wxID_ZOOM_IN, _("Zoom In\t]"));
      viewMenu->Append(wxID_ZOOM_OUT, _("Zoom Out\t["));
   #else
      viewMenu->Append(wxID_ZOOM_IN, _("Zoom In\tCtrl+]"));
      viewMenu->Append(wxID_ZOOM_OUT, _("Zoom Out\tCtrl+["));
   #endif
   viewMenu->Append(ID_SET_SCALE, _("Set Scale"), scaleSubMenu);
   viewMenu->AppendSeparator();
   #ifdef __WXMSW__
      // Windows doesn't support Ctrl+<non-alpha> menu shortcuts
      viewMenu->AppendCheckItem(ID_STATUS, _("Show Status Bar\t;"));
      viewMenu->AppendCheckItem(ID_TOOL, _("Show Tool Bar\t'"));
   #else
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
   #elif defined(__WXMSW__)
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

   #if wxUSE_DRAG_AND_DROP
      // let users drop files onto pattern view
      viewptr->SetDropTarget(new DnDFile());
   #endif

   // wxStatusBar can only appear at bottom of frame so we use our own
   // status bar class which creates a child window at top of frame
   statusptr = new StatusBar(this, 0, 0, 100, 100);
   if (statusptr == NULL) Fatal("Failed to create status bar!");

   // now set width and height to what we really want
   viewptr->SetSize(0, statusptr->statusht, wd,
                       ht > statusptr->statusht ? ht - statusptr->statusht : 0);
   statusptr->SetSize(0, 0, wd, statusptr->statusht);

   InitDrawingData();      // do this after viewport size has been set

   generating = false;     // not generating pattern
   fullscreen = false;     // not in full screen mode
   showbanner = true;      // avoid first file clearing banner message
   savestart = false;      // no need to save starting pattern just yet
   gen0algo = NULL;        // no starting pattern
   warp = 0;               // initial speed setting
}

MainFrame::~MainFrame()
{
   delete onetimer;
   DestroyDrawingData();
}
