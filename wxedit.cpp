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

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
   #include "wx/wx.h"      // for all others include the necessary headers
#endif

#include "wx/dcbuffer.h"   // for wxBufferedPaintDC
#if wxUSE_TOOLTIPS
   #include "wx/tooltip.h" // for wxToolTip
#endif

#include "bigint.h"
#include "lifealgo.h"

#include "wxgolly.h"       // for viewptr, statusptr, mainptr
#include "wxmain.h"        // for mainptr->...
#include "wxutils.h"       // for Fatal
#include "wxprefs.h"       // for showedit, showallstates, etc
#include "wxstatus.h"      // for statusptr->...
#include "wxscript.h"      // for inscript
#include "wxview.h"        // for viewptr->...
#include "wxalgos.h"       // for AlgoData, algoinfo
#include "wxlayer.h"       // for currlayer, LayerBarHeight
#include "wxedit.h"

// -----------------------------------------------------------------------------

enum {
   // ids for bitmap buttons in edit bar
   DRAW_BUTT = 0,
   PICK_BUTT,
   SELECT_BUTT,
   MOVE_BUTT,
   ZOOMIN_BUTT,
   ZOOMOUT_BUTT,
   ALLSTATES_BUTT,
   NUM_BUTTONS,      // must be after all buttons
   LEFT_SCROLL
};

#ifdef __WXMSW__
   // bitmaps are loaded via .rc file
#else
   // bitmaps for edit bar buttons
   #include "bitmaps/draw.xpm"
   #include "bitmaps/pick.xpm"
   #include "bitmaps/select.xpm"
   #include "bitmaps/move.xpm"
   #include "bitmaps/zoomin.xpm"
   #include "bitmaps/zoomout.xpm"
   #include "bitmaps/allstates.xpm"
   // bitmaps for down state of toggle buttons
   #include "bitmaps/draw_down.xpm"
   #include "bitmaps/pick_down.xpm"
   #include "bitmaps/select_down.xpm"
   #include "bitmaps/move_down.xpm"
   #include "bitmaps/zoomin_down.xpm"
   #include "bitmaps/zoomout_down.xpm"
   #include "bitmaps/allstates_down.xpm"
#endif

// -----------------------------------------------------------------------------

// Define edit bar window:

// derive from wxPanel so we get current theme's background color on Windows
class EditBar : public wxPanel
{
public:
   EditBar(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht);
   ~EditBar();

   // add a bitmap button to edit bar
   void AddButton(int id, const wxString& tip);

   // add a vertical gap between buttons
   void AddSeparator();
   
   // enable/disable button
   void EnableButton(int id, bool enable);
   
   // set state of a toggle button
   void SelectButton(int id, bool select);

   // move controls up or down depending on showallstates
   void MoveControls();

   // update left scroll bar
   void UpdateLeftScroll();

   // detect press and release of a bitmap button
   void OnButtonDown(wxMouseEvent& event);
   void OnButtonUp(wxMouseEvent& event);
   void OnKillFocus(wxFocusEvent& event);

private:
   // any class wishing to process wxWidgets events must use this macro
   DECLARE_EVENT_TABLE()

   // event handlers
   void OnPaint(wxPaintEvent& event);
   void OnMouseDown(wxMouseEvent& event);
   void OnButton(wxCommandEvent& event);
   void OnLeftScroll(wxScrollEvent& event);

   void SetEditFont(wxDC& dc);
   void DisplayText(wxDC& dc, const wxString& s, wxCoord x, wxCoord y);
   void DrawAllStates(wxDC& dc);
   void DrawEditBar(wxDC& dc, int wd, int ht);
   
   // bitmaps for normal or down state
   wxBitmap normbutt[NUM_BUTTONS];
   wxBitmap downbutt[NUM_BUTTONS];

   #ifdef __WXMSW__
      // on Windows we need bitmaps for disabled buttons
      wxBitmap disnormbutt[NUM_BUTTONS];
      wxBitmap disdownbutt[NUM_BUTTONS];
   #endif
   
   // remember state of toggle buttons to avoid unnecessary drawing;
   // 0 = not yet initialized, 1 = selected, -1 = not selected
   int buttstate[NUM_BUTTONS];
   
   // positioning data used by AddButton and AddSeparator
   int ypos, xpos, smallgap, biggap;

   wxBitmap* editbitmap;         // edit bar bitmap
   int editbitmapwd;             // width of edit bar bitmap
   int editbitmapht;             // height of edit bar bitmap

   wxRect colorbox;              // box showing current color
   wxRect iconbox;               // box showing current icon

   wxScrollBar* leftbar;         // left scroll bar
   
   int h_col1;                   // horizontal position of labels
   int h_col2;                   // horizontal position of info for state 0
   int digitwd;                  // width of digit in edit bar font
   int digitht;                  // height of digit in edit bar font
   int textascent;               // vertical adjustment used in DrawText calls
   wxFont* editfont;             // edit bar font
};

BEGIN_EVENT_TABLE(EditBar, wxPanel)
   EVT_PAINT            (              EditBar::OnPaint)
   EVT_LEFT_DOWN        (              EditBar::OnMouseDown)
   EVT_BUTTON           (wxID_ANY,     EditBar::OnButton)
   EVT_COMMAND_SCROLL   (LEFT_SCROLL,  EditBar::OnLeftScroll)
END_EVENT_TABLE()

// -----------------------------------------------------------------------------

EditBar* editbarptr = NULL;         // global pointer to edit bar
const int BIGHT = 80;               // height of edit bar if showallstates
const int SMALLHT = 32;             // height of edit bar if not showallstates
static int editbarht;               // current height (BIGHT or SMALLHT)

const int LINEHT = 14;              // distance between each baseline
const int BASELINE1 = LINEHT-1;     // baseline of 1st line
const int BASELINE2 = BASELINE1+LINEHT;   // baseline of 2nd line
const int BASELINE3 = BASELINE2+LINEHT;   // baseline of 3rd line
const int COLWD = 20;               // column width of state/color/icon info
const int BOXWD = 9;                // width (and height) of small color/icon boxes
const int BOXSIZE = 17;             // width and height of colorbox and iconbox
const int PAGESIZE = 10;            // scroll amount when paging

// edit bar buttons (must be global to use Connect/Disconect on Windows)
wxBitmapButton* ebbutt[NUM_BUTTONS];

// -----------------------------------------------------------------------------

EditBar::EditBar(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht)
   : wxPanel(parent, wxID_ANY, wxPoint(xorg,yorg), wxSize(wd,ht),
             wxNO_FULL_REPAINT_ON_RESIZE)
{
   #ifdef __WXGTK__
      // avoid erasing background on GTK+
      SetBackgroundStyle(wxBG_STYLE_CUSTOM);
   #endif

   // init bitmaps for normal state
   normbutt[DRAW_BUTT] =      wxBITMAP(draw);
   normbutt[PICK_BUTT] =      wxBITMAP(pick);
   normbutt[SELECT_BUTT] =    wxBITMAP(select);
   normbutt[MOVE_BUTT] =      wxBITMAP(move);
   normbutt[ZOOMIN_BUTT] =    wxBITMAP(zoomin);
   normbutt[ZOOMOUT_BUTT] =   wxBITMAP(zoomout);
   normbutt[ALLSTATES_BUTT] = wxBITMAP(allstates);
   
   // toggle buttons also have a down state
   downbutt[DRAW_BUTT] =      wxBITMAP(draw_down);
   downbutt[PICK_BUTT] =      wxBITMAP(pick_down);
   downbutt[SELECT_BUTT] =    wxBITMAP(select_down);
   downbutt[MOVE_BUTT] =      wxBITMAP(move_down);
   downbutt[ZOOMIN_BUTT] =    wxBITMAP(zoomin_down);
   downbutt[ZOOMOUT_BUTT] =   wxBITMAP(zoomout_down);
   downbutt[ALLSTATES_BUTT] = wxBITMAP(allstates_down);

   #ifdef __WXMSW__
      for (int i = 0; i < NUM_BUTTONS; i++) {
         CreatePaleBitmap(normbutt[i], disnormbutt[i]);
      }
      CreatePaleBitmap(downbutt[DRAW_BUTT],       disdownbutt[DRAW_BUTT]);
      CreatePaleBitmap(downbutt[PICK_BUTT],       disdownbutt[PICK_BUTT]);
      CreatePaleBitmap(downbutt[SELECT_BUTT],     disdownbutt[SELECT_BUTT]);
      CreatePaleBitmap(downbutt[MOVE_BUTT],       disdownbutt[MOVE_BUTT]);
      CreatePaleBitmap(downbutt[ZOOMIN_BUTT],     disdownbutt[ZOOMIN_BUTT]);
      CreatePaleBitmap(downbutt[ZOOMOUT_BUTT],    disdownbutt[ZOOMOUT_BUTT]);
      CreatePaleBitmap(downbutt[ALLSTATES_BUTT],  disdownbutt[ALLSTATES_BUTT]);
   #endif

   for (int i = 0; i < NUM_BUTTONS; i++) {
      buttstate[i] = 0;
   }

   // init position variables used by AddButton and AddSeparator
   xpos = 4;
   #ifdef __WXGTK__
      ypos = 3;
      smallgap = 6;
   #else
      ypos = 4;
      smallgap = 4;
   #endif
   if (showallstates) ypos += BIGHT - SMALLHT;
   biggap = 16;

   // add buttons
   AddButton(DRAW_BUTT,       _("Draw"));
   AddButton(PICK_BUTT,       _("Pick"));
   AddButton(SELECT_BUTT,     _("Select"));
   AddButton(MOVE_BUTT,       _("Move"));
   AddButton(ZOOMIN_BUTT,     _("Zoom in"));
   AddButton(ZOOMOUT_BUTT,    _("Zoom out"));
   AddSeparator();
   AddButton(ALLSTATES_BUTT,  _("Show/hide all states"));

   // create font for text in edit bar and set textascent for use in DisplayText
   #ifdef __WXMSW__
      // use smaller, narrower font on Windows
      editfont = wxFont::New(8, wxDEFAULT, wxNORMAL, wxNORMAL);
      int major, minor;
      wxGetOsVersion(&major, &minor);
      if ( major > 5 || (major == 5 && minor >= 1) ) {
         // 5.1+ means XP or later (Vista if major >= 6)
         textascent = 11;
      } else {
         textascent = 10;
      }
   #elif defined(__WXGTK__)
      // use smaller font on GTK
      editfont = wxFont::New(8, wxMODERN, wxNORMAL, wxNORMAL);
      textascent = 11;
   #else
      editfont = wxFont::New(10, wxMODERN, wxNORMAL, wxNORMAL);
      textascent = 10;
   #endif
   if (editfont == NULL) Fatal(_("Failed to create edit bar font!"));
   
   // determine horizontal offsets for info in edit bar
   wxClientDC dc(this);
   int textwd, textht;
   SetEditFont(dc);
   h_col1 = 4;
   dc.GetTextExtent(_("State:"), &textwd, &textht);
   h_col2 = h_col1 + textwd + 4;
   dc.GetTextExtent(_("9"), &digitwd, &digitht);
   #ifdef __WXMAC__
      digitht -= 4;
   #elif defined(__WXMSW__)
      digitht -= 4;
   #else // Linux
      digitht -= 6;
   #endif

   editbitmap = NULL;
   editbitmapwd = -1;
   editbitmapht = -1;
   
   // add scroll bar
   #ifdef __WXMAC__
      int scrollbarht = 15;   // must be this height on Mac
   #else
      int scrollbarht = BOXSIZE;
   #endif
      int x = xpos + 3*digitwd + smallgap + 2*(BOXSIZE + smallgap);
      int y = editbarht - SMALLHT + (SMALLHT - (scrollbarht + 1)) / 2;
   #ifdef __WXGTK__
      y++;
   #endif
   leftbar = new wxScrollBar(this, LEFT_SCROLL, wxPoint(x, y),
                             wxSize(100, scrollbarht),
                             wxSB_HORIZONTAL);
   if (leftbar == NULL) Fatal(_("Failed to create scroll bar!"));

   #ifdef __WXGTK__
      // wxGTK bug? need this so OnLeftScroll will be called
      leftbar->SetScrollbar(0, 1, 100, 1, true);
   #endif
}

// -----------------------------------------------------------------------------

EditBar::~EditBar()
{
   delete editfont;
   delete editbitmap;
   delete leftbar;
}

// -----------------------------------------------------------------------------

void EditBar::SetEditFont(wxDC& dc)
{
   dc.SetFont(*editfont);
   dc.SetTextForeground(*wxBLACK);
   dc.SetBrush(*wxBLACK_BRUSH);           // avoids problem on Linux/X11
   dc.SetBackgroundMode(wxTRANSPARENT);
}

// -----------------------------------------------------------------------------

void EditBar::DisplayText(wxDC& dc, const wxString& s, wxCoord x, wxCoord y)
{
   // DrawText's y parameter is top of text box but we pass in baseline
   // so adjust by textascent which depends on platform and OS version -- yuk!
   dc.DrawText(s, x, y - textascent);
}

// -----------------------------------------------------------------------------

void EditBar::DrawAllStates(wxDC& dc)
{
   DisplayText(dc, _("State:"), h_col1, BASELINE1);
   DisplayText(dc, _("Color:"), h_col1, BASELINE2);
   DisplayText(dc, _("Icon:"),  h_col1, BASELINE3);

   AlgoData* ad = algoinfo[currlayer->algtype];
   wxBitmap** iconmaps = ad->icons7x7;

   // set rgb values for dead state
   ad->cellr[0] = swapcolors ? livergb[currindex]->Red() : deadrgb->Red();
   ad->cellg[0] = swapcolors ? livergb[currindex]->Green() : deadrgb->Green();
   ad->cellb[0] = swapcolors ? livergb[currindex]->Blue() : deadrgb->Blue();
   wxColor deadcolor(ad->cellr[0], ad->cellg[0], ad->cellb[0]);
   
   unsigned char saver=0, saveg=0, saveb=0;
   if (currlayer->algo->NumCellStates() == 2) {
      // set rgb values for live cells in 2-state universe, but only temporarily
      // because the current algo might allow rules with a varying # of cell states
      // (eg. current Generations rule could be 12/34/2)
      //!!! yuk -- try to always use cellr/g/b, regardless of # of states?
      saver = ad->cellr[1];
      saveg = ad->cellg[1];
      saveb = ad->cellb[1];
      ad->cellr[1] = swapcolors ? deadrgb->Red() : livergb[currindex]->Red();
      ad->cellg[1] = swapcolors ? deadrgb->Green() : livergb[currindex]->Green();
      ad->cellb[1] = swapcolors ? deadrgb->Blue() : livergb[currindex]->Blue();
   }

   dc.SetPen(*wxBLACK_PEN);

   for (int i = 0; i < currlayer->algo->NumCellStates(); i++) {
      wxString strbuf;
      wxRect r;
      int x;
      
      // draw state value
      strbuf.Printf(_("%d"), i);
      x = h_col2 + i * COLWD + (COLWD - strbuf.length() * digitwd) / 2;
      DisplayText(dc, strbuf, x, BASELINE1);
      
      // draw color box
      x = 1 + h_col2 + i * COLWD + (COLWD - BOXWD) / 2;
      wxColor color(ad->cellr[i], ad->cellg[i], ad->cellb[i]);
      r = wxRect(x, BASELINE2 - BOXWD, BOXWD, BOXWD);
      dc.SetBrush(wxBrush(color));
      dc.DrawRectangle(r);
      dc.SetBrush(wxNullBrush);
      
      // draw icon box
      r = wxRect(x, BASELINE3 - BOXWD, BOXWD, BOXWD);
      if (iconmaps && iconmaps[i]) {
         dc.SetBrush(wxBrush(deadcolor));
         dc.DrawRectangle(r);
         dc.SetBrush(wxNullBrush);
         dc.DrawBitmap(*iconmaps[i], x + 1, BASELINE3 - BOXWD + 1, true);
      } else {
         dc.SetBrush(*wxTRANSPARENT_BRUSH);
         dc.DrawRectangle(r);
         dc.SetBrush(wxNullBrush);
      }
   }

   if (currlayer->algo->NumCellStates() == 2) {
      // restore live cell color changed above
      ad->cellr[1] = saver;
      ad->cellg[1] = saveg;
      ad->cellb[1] = saveb;
   }
   
   // draw rect around current drawing state
   int x = 1 + h_col2 + COLWD * currlayer->drawingstate;
   wxRect r(x, 2, COLWD - 1, BIGHT - SMALLHT - 4);
   dc.SetBrush(*wxTRANSPARENT_BRUSH);
   dc.DrawRectangle(r);
   dc.SetBrush(wxNullBrush);

   dc.SetPen(wxNullPen);
}

// -----------------------------------------------------------------------------

void EditBar::DrawEditBar(wxDC& dc, int wd, int ht)
{
   wxRect r = wxRect(0, 0, wd, ht);
   
   #ifdef __WXMAC__
      wxBrush brush(wxColor(202,202,202));
      FillRect(dc, r, brush);
   #endif
   
   #ifdef __WXMSW__
      // use theme background color on Windows
      wxBrush brush(GetBackgroundColour());
      FillRect(dc, r, brush);
   #endif
   
   // draw gray border line at bottom edge
   #if defined(__WXMSW__)
      dc.SetPen(*wxGREY_PEN);
   #elif defined(__WXMAC__)
      wxPen linepen(wxColor(140,140,140));
      dc.SetPen(linepen);
   #else
      dc.SetPen(*wxLIGHT_GREY_PEN);
   #endif
   dc.DrawLine(0, r.GetBottom(), r.width, r.GetBottom());
   dc.SetPen(wxNullPen);
   
   // reset drawing state in case it's no longer valid (due to algo/rule change)
   if (currlayer->drawingstate >= currlayer->algo->NumCellStates()) {
      currlayer->drawingstate = 1;
   }

   SetEditFont(dc);  // for DisplayText calls

   if (showallstates) DrawAllStates(dc);

   AlgoData* ad = algoinfo[currlayer->algtype];
   unsigned char saver=0, saveg=0, saveb=0;

   dc.SetPen(*wxBLACK_PEN);
   
   // draw current drawing state
   int state = currlayer->drawingstate;
   int x = xpos;
   int y = editbarht - 8;
   wxString strbuf;
   if (state < 10) x += digitwd;
   if (state < 100) x += digitwd;
   strbuf.Printf(_("%d"), state);
   DisplayText(dc, strbuf, x, y - (BOXSIZE - digitht)/2);

   // set rgb values for dead state
   ad->cellr[0] = swapcolors ? livergb[currindex]->Red() : deadrgb->Red();
   ad->cellg[0] = swapcolors ? livergb[currindex]->Green() : deadrgb->Green();
   ad->cellb[0] = swapcolors ? livergb[currindex]->Blue() : deadrgb->Blue();
   wxColor deadcolor(ad->cellr[0], ad->cellg[0], ad->cellb[0]);

   if (state == 1 && currlayer->algo->NumCellStates() == 2) {
      // set rgb values for state 1 in 2-state universe, but only temporarily
      // because the current algo might allow rules with a varying # of cell states
      // (eg. current Generations rule could be 12/34/2)
      //!!! yuk -- try to always use cellr/g/b, regardless of # of states?
      saver = ad->cellr[1];
      saveg = ad->cellg[1];
      saveb = ad->cellb[1];
      ad->cellr[1] = swapcolors ? deadrgb->Red() : livergb[currindex]->Red();
      ad->cellg[1] = swapcolors ? deadrgb->Green() : livergb[currindex]->Green();
      ad->cellb[1] = swapcolors ? deadrgb->Blue() : livergb[currindex]->Blue();
   }
   wxColor color(ad->cellr[state], ad->cellg[state], ad->cellb[state]);
   if (state == 1 && currlayer->algo->NumCellStates() == 2) {
      // restore state 1 color changed above
      ad->cellr[1] = saver;
      ad->cellg[1] = saveg;
      ad->cellb[1] = saveb;
   }
   
   // draw color box
   x = xpos + 3*digitwd + smallgap;
   colorbox = wxRect(x, y - BOXSIZE, BOXSIZE, BOXSIZE);
   dc.SetBrush(wxBrush(color));
   dc.DrawRectangle(colorbox);
   dc.SetBrush(wxNullBrush);
   
   // draw icon box
   x += BOXSIZE + smallgap;
   iconbox = wxRect(x, y - BOXSIZE, BOXSIZE, BOXSIZE);
   wxBitmap** iconmaps = ad->icons15x15;
   if (iconmaps && iconmaps[state]) {
      dc.SetBrush(wxBrush(deadcolor));
      dc.DrawRectangle(iconbox);
      dc.SetBrush(wxNullBrush);
      dc.DrawBitmap(*iconmaps[state], x + 1, y - BOXSIZE + 1, true);
   } else {
      dc.SetBrush(*wxTRANSPARENT_BRUSH);
      dc.DrawRectangle(iconbox);
      dc.SetBrush(wxNullBrush);
   }

   dc.SetPen(wxNullPen);
}

// -----------------------------------------------------------------------------

void EditBar::OnPaint(wxPaintEvent& WXUNUSED(event))
{
   int wd, ht;
   GetClientSize(&wd, &ht);
   // wd or ht might be < 1 on Win/X11 platforms
   if (wd < 1) wd = 1;
   if (ht < 1) ht = 1;

   #if defined(__WXMAC__) || defined(__WXGTK__)
      // windows on Mac OS X and GTK+ 2.0 are automatically buffered
      wxPaintDC dc(this);
   #else
      // use wxWidgets buffering to avoid flicker
      if (wd != editbitmapwd || ht != editbitmapht) {
         // need to create a new bitmap for edit bar
         delete editbitmap;
         editbitmap = new wxBitmap(wd, ht);
         editbitmapwd = wd;
         editbitmapht = ht;
      }
      if (editbitmap == NULL) Fatal(_("Not enough memory to render edit bar!"));
      wxBufferedPaintDC dc(this, *editbitmap);
   #endif
   
   if (!showedit) return;
   
   DrawEditBar(dc, wd, ht);
}

// -----------------------------------------------------------------------------

void EditBar::OnMouseDown(wxMouseEvent& event)
{
   // on Windows we need to reset keyboard focus to viewport window
   viewptr->SetFocus();
   
   mainptr->showbanner = false;
   statusptr->ClearMessage();

   if (inscript) return;    // let script control drawing state

   int x = event.GetX();
   int y = event.GetY();
   
   if (showallstates) {
      // user can change drawing state by clicking in appropriate box
      int right = h_col2 + COLWD * currlayer->algo->NumCellStates();
      int box = -1;
      
      if (x > h_col2 && x < right && y < (BIGHT - SMALLHT)) {
         box = (x - h_col2) / COLWD;
      }
      
      if (box >= 0 && box < currlayer->algo->NumCellStates() &&
          currlayer->drawingstate != box) {
         currlayer->drawingstate = box;
         Refresh(false);
         Update();
         UpdateLeftScroll();
      }
   }
   
   // user can change icon mode by clicking in icon/color box
   if (iconbox.Contains(x,y) && !showicons) {
      viewptr->ToggleCellIcons();
   } else if (colorbox.Contains(x,y) && showicons) {
      viewptr->ToggleCellIcons();
   }
}

// -----------------------------------------------------------------------------

void EditBar::OnButton(wxCommandEvent& event)
{
   #ifdef __WXMAC__
      // close any open tool tip window (fixes wxMac bug?)
      wxToolTip::RemoveToolTips();
   #endif
   
   mainptr->showbanner = false;
   statusptr->ClearMessage();

   int id = event.GetId();

   int cmdid;
   switch (id) {
      case DRAW_BUTT:      cmdid = ID_DRAW; break;
      case PICK_BUTT:      cmdid = ID_PICK; break;
      case SELECT_BUTT:    cmdid = ID_SELECT; break;
      case MOVE_BUTT:      cmdid = ID_MOVE; break;
      case ZOOMIN_BUTT:    cmdid = ID_ZOOMIN; break;
      case ZOOMOUT_BUTT:   cmdid = ID_ZOOMOUT; break;
      case ALLSTATES_BUTT: cmdid = ID_ALL_STATES; break;
      default:             Warning(_("Unexpected button id!")); return;
   }
   
   // call MainFrame::OnMenu after OnButton finishes
   wxCommandEvent cmdevt(wxEVT_COMMAND_MENU_SELECTED, cmdid);
   wxPostEvent(mainptr->GetEventHandler(), cmdevt);
}

// -----------------------------------------------------------------------------

void EditBar::OnLeftScroll(wxScrollEvent& event)
{
   WXTYPE type = event.GetEventType();

   if (type == wxEVT_SCROLL_LINEUP) {
      currlayer->drawingstate--;
      if (currlayer->drawingstate < 0)
         currlayer->drawingstate = 0;
      Refresh(false);

   } else if (type == wxEVT_SCROLL_LINEDOWN) {
      currlayer->drawingstate++;
      if (currlayer->drawingstate >= currlayer->algo->NumCellStates())
         currlayer->drawingstate = currlayer->algo->NumCellStates() - 1;
      Refresh(false);

   } else if (type == wxEVT_SCROLL_PAGEUP) {
      currlayer->drawingstate -= PAGESIZE;
      if (currlayer->drawingstate < 0)
         currlayer->drawingstate = 0;
      Refresh(false);

   } else if (type == wxEVT_SCROLL_PAGEDOWN) {
      currlayer->drawingstate += PAGESIZE;
      if (currlayer->drawingstate >= currlayer->algo->NumCellStates())
         currlayer->drawingstate = currlayer->algo->NumCellStates() - 1;
      Refresh(false);

   } else if (type == wxEVT_SCROLL_THUMBTRACK) {
      currlayer->drawingstate = event.GetPosition();
      if (currlayer->drawingstate < 0)
         currlayer->drawingstate = 0;
      if (currlayer->drawingstate >= currlayer->algo->NumCellStates())
         currlayer->drawingstate = currlayer->algo->NumCellStates() - 1;
      Refresh(false);
   }
   
   UpdateLeftScroll();
}

// -----------------------------------------------------------------------------

void EditBar::OnKillFocus(wxFocusEvent& event)
{
   int id = event.GetId();
   ebbutt[id]->SetFocus();   // don't let button lose focus
}

// -----------------------------------------------------------------------------

void EditBar::OnButtonDown(wxMouseEvent& event)
{
   // edit bar button has been pressed
   int id = event.GetId();
   
   // connect a handler that keeps focus with the pressed button
   ebbutt[id]->Connect(id, wxEVT_KILL_FOCUS, wxFocusEventHandler(EditBar::OnKillFocus));

   event.Skip();
}

// -----------------------------------------------------------------------------

void EditBar::OnButtonUp(wxMouseEvent& event)
{
   // edit bar button has been released
   int id = event.GetId();

   wxPoint pt = ebbutt[id]->ScreenToClient( wxGetMousePosition() );

   int wd, ht;
   ebbutt[id]->GetClientSize(&wd, &ht);
   wxRect r(0, 0, wd, ht);

   // diconnect kill-focus handler
   ebbutt[id]->Disconnect(id, wxEVT_KILL_FOCUS, wxFocusEventHandler(EditBar::OnKillFocus));
   viewptr->SetFocus();

   if (r.Contains(pt)) {
      // call OnButton
      wxCommandEvent buttevt(wxEVT_COMMAND_BUTTON_CLICKED, id);
      buttevt.SetEventObject(ebbutt[id]);
      ebbutt[id]->ProcessEvent(buttevt);
   }
}

// -----------------------------------------------------------------------------

void EditBar::AddButton(int id, const wxString& tip)
{
   ebbutt[id] = new wxBitmapButton(this, id, normbutt[id], wxPoint(xpos,ypos));
   if (ebbutt[id] == NULL) {
      Fatal(_("Failed to create edit bar button!"));
   } else {
      const int BUTTON_WD = 24;        // nominal width of bitmap buttons
      xpos += BUTTON_WD + smallgap;
      ebbutt[id]->SetToolTip(tip);
      #ifdef __WXMSW__
         // fix problem with edit bar buttons when generating/inscript
         // due to focus being changed to viewptr
         ebbutt[id]->Connect(id, wxEVT_LEFT_DOWN, wxMouseEventHandler(EditBar::OnButtonDown));
         ebbutt[id]->Connect(id, wxEVT_LEFT_UP, wxMouseEventHandler(EditBar::OnButtonUp));
      #endif
   }
}

// -----------------------------------------------------------------------------

void EditBar::AddSeparator()
{
   xpos += biggap - smallgap;
}

// -----------------------------------------------------------------------------

void EditBar::EnableButton(int id, bool enable)
{
   if (enable == ebbutt[id]->IsEnabled()) return;

   #ifdef __WXMSW__
      if (id == DRAW_BUTT && currlayer->curs == curs_pencil) {
         ebbutt[id]->SetBitmapDisabled(disdownbutt[id]);
         
      } else if (id == PICK_BUTT && currlayer->curs == curs_pick) {
         ebbutt[id]->SetBitmapDisabled(disdownbutt[id]);
         
      } else if (id == SELECT_BUTT && currlayer->curs == curs_cross) {
         ebbutt[id]->SetBitmapDisabled(disdownbutt[id]);
         
      } else if (id == MOVE_BUTT && currlayer->curs == curs_hand) {
         ebbutt[id]->SetBitmapDisabled(disdownbutt[id]);
         
      } else if (id == ZOOMIN_BUTT && currlayer->curs == curs_zoomin) {
         ebbutt[id]->SetBitmapDisabled(disdownbutt[id]);
         
      } else if (id == ZOOMOUT_BUTT && currlayer->curs == curs_zoomout) {
         ebbutt[id]->SetBitmapDisabled(disdownbutt[id]);
         
      } else if (id == ALLSTATES_BUTT && showallstates) {
         ebbutt[id]->SetBitmapDisabled(disdownbutt[id]);
         
      } else {
         ebbutt[id]->SetBitmapDisabled(disnormbutt[id]);
      }
   #endif

   ebbutt[id]->Enable(enable);
}

// -----------------------------------------------------------------------------

void EditBar::SelectButton(int id, bool select)
{
   if (select) {
      if (buttstate[id] == 1) return;
      buttstate[id] = 1;
      ebbutt[id]->SetBitmapLabel(downbutt[id]);
   } else {
      if (buttstate[id] == -1) return;
      buttstate[id] = -1;
      ebbutt[id]->SetBitmapLabel(normbutt[id]);
   }

   #ifdef __WXX11__
      ebbutt[id]->ClearBackground();    // fix wxX11 problem
   #endif

   ebbutt[id]->Refresh(false);
}

// -----------------------------------------------------------------------------

void EditBar::MoveControls()
{
   // showallstates has just been toggled
   int x, y;
   int yshift = BIGHT - SMALLHT;
   if (!showallstates) yshift *= -1;
   for (int id = 0; id < NUM_BUTTONS; id++) {
      ebbutt[id]->GetPosition(&x, &y);
      ebbutt[id]->Move(x, y + yshift);
   }
   leftbar->GetPosition(&x, &y);
   leftbar->Move(x, y + yshift);
}

// -----------------------------------------------------------------------------

void EditBar::UpdateLeftScroll()
{
   leftbar->SetScrollbar(currlayer->drawingstate, 1,
                         currlayer->algo->NumCellStates(), PAGESIZE, true);
   #ifndef __WXMAC__
      viewptr->SetFocus();    // need on Win/Linux
   #endif
}

// -----------------------------------------------------------------------------

void CreateEditBar(wxWindow* parent)
{
   // create edit bar underneath layer bar
   int wd, ht;
   parent->GetClientSize(&wd, &ht);

   editbarht = showallstates ? BIGHT : SMALLHT;
   editbarptr = new EditBar(parent, 0, LayerBarHeight(), wd, editbarht);
   if (editbarptr == NULL) Fatal(_("Failed to create edit bar!"));
      
   editbarptr->Show(showedit);
}

// -----------------------------------------------------------------------------

int EditBarHeight() {
   return (showedit ? editbarht : 0);
}

// -----------------------------------------------------------------------------

void ResizeEditBar(int wd)
{
   if (editbarptr) {
      editbarptr->SetSize(wd, editbarht);
   }
}

// -----------------------------------------------------------------------------

void UpdateEditBar(bool active)
{
   if (editbarptr && showedit) {
      if (viewptr->waitingforclick) active = false;

      // set state of toggle buttons
      editbarptr->SelectButton(DRAW_BUTT,       currlayer->curs == curs_pencil);
      editbarptr->SelectButton(PICK_BUTT,       currlayer->curs == curs_pick);
      editbarptr->SelectButton(SELECT_BUTT,     currlayer->curs == curs_cross);
      editbarptr->SelectButton(MOVE_BUTT,       currlayer->curs == curs_hand);
      editbarptr->SelectButton(ZOOMIN_BUTT,     currlayer->curs == curs_zoomin);
      editbarptr->SelectButton(ZOOMOUT_BUTT,    currlayer->curs == curs_zoomout);
      editbarptr->SelectButton(ALLSTATES_BUTT,  showallstates);

      editbarptr->EnableButton(DRAW_BUTT,       active);
      editbarptr->EnableButton(PICK_BUTT,       active);
      editbarptr->EnableButton(SELECT_BUTT,     active);
      editbarptr->EnableButton(MOVE_BUTT,       active);
      editbarptr->EnableButton(ZOOMIN_BUTT,     active);
      editbarptr->EnableButton(ZOOMOUT_BUTT,    active);
      editbarptr->EnableButton(ALLSTATES_BUTT,  active);
      
      editbarptr->Refresh(false);
      editbarptr->Update();
      
      // currlayer->drawingstate might have changed
      editbarptr->UpdateLeftScroll();
   }
}

// -----------------------------------------------------------------------------

void ToggleEditBar()
{
   showedit = !showedit;
   wxRect r = bigview->GetRect();
   
   if (showedit) {
      // show edit bar at top of viewport window or underneath layer bar
      r.y += editbarht;
      r.height -= editbarht;
      ResizeEditBar(r.width);
   } else {
      // hide edit bar
      r.y -= editbarht;
      r.height += editbarht;
   }
   bigview->SetSize(r);
   editbarptr->Show(showedit);    // needed on Windows
   
   if (showlayer) {
      // line at bottom of layer bar may need to be added/removed
      RedrawLayerBar();
   }
   
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void ToggleAllStates()
{
   showallstates = !showallstates;
   editbarht = showallstates ? BIGHT : SMALLHT;
   // move controls up/down
   editbarptr->MoveControls();
   if (showedit) {
      int diff = BIGHT - SMALLHT;
      if (!showallstates) diff *= -1;
      wxRect r = bigview->GetRect();
      ResizeEditBar(r.width);
      r.y += diff;
      r.height -= diff;
      bigview->SetSize(r);
      mainptr->UpdateEverything();
   } else if (showallstates) {
      // show the edit bar using new height
      ToggleEditBar();
   } else {
      mainptr->UpdateMenuItems(mainptr->IsActive());
   }
}

// -----------------------------------------------------------------------------

void ShiftEditBar(int yamount)
{
   int x, y;
   editbarptr->GetPosition(&x, &y);
   editbarptr->Move(x, y + yamount);
}
