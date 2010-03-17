                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2010 Andrew Trevorrow and Tomas Rokicki.

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
#include "wxutils.h"       // for Warning, etc
#include "wxprefs.h"       // for showtimeline, etc
#include "wxstatus.h"      // for statusptr->...
#include "wxscript.h"      // for inscript
#include "wxview.h"        // for viewptr->...
#include "wxlayer.h"       // for currlayer
#include "wxtimeline.h"

#ifdef __WXMSW__
   // bitmaps are loaded via .rc file
#else
   // bitmaps for timeline bar buttons
   #include "bitmaps/record.xpm"
   #include "bitmaps/backwards.xpm"
   #include "bitmaps/forwards.xpm"
   #include "bitmaps/stopplay.xpm"
   #include "bitmaps/deltime.xpm"
#endif

// -----------------------------------------------------------------------------

// these need to be per layer rather than here???!!!
// note also that New Pattern doesn't update play buttons!!!

static int currframe;         // current frame in timeline
static int autoplay;          // +ve = play forwards, -ve = play backwards, 0 = stop
static long lastframe = 0;    // time when last frame was displayed
static int speed;             // controls speed at which frames are played

const int MINSPEED = -10;     // delay 2^10 msecs between each frame
const int MAXSPEED = 10;      // skip 2^10 frames

// -----------------------------------------------------------------------------

// Define timeline bar window:

enum {
   // ids for bitmap buttons in timeline bar
   RECORD_BUTT = 0,
   BACKWARDS_BUTT,
   FORWARDS_BUTT,
   STOPPLAY_BUTT,
   DELETE_BUTT,
   NUM_BUTTONS,      // must be after all buttons
   ID_SLIDER,        // for slider
   ID_SCROLL         // for scroll bar
};

// derive from wxPanel so we get current theme's background color on Windows
class TimelineBar : public wxPanel
{
public:
   TimelineBar(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht);
   ~TimelineBar();

   void AddButton(int id, const wxString& tip);
   void AddSeparator();
   void EnableButton(int id, bool enable);
   void UpdateButtons();
   void UpdateSlider();
   void UpdateScrollBar();
   void DisplayCurrentFrame();

   // detect press and release of a bitmap button
   void OnButtonDown(wxMouseEvent& event);
   void OnButtonUp(wxMouseEvent& event);
   void OnKillFocus(wxFocusEvent& event);

   wxSlider* slider;             // slider for controlling autoplay speed
   wxScrollBar* framebar;        // scroll bar for displaying timeline frames

private:
   // any class wishing to process wxWidgets events must use this macro
   DECLARE_EVENT_TABLE()

   // event handlers
   void OnPaint(wxPaintEvent& event);
   void OnMouseDown(wxMouseEvent& event);
   void OnButton(wxCommandEvent& event);
   void OnSlider(wxScrollEvent& event);
   void OnScroll(wxScrollEvent& event);

   void SetTimelineFont(wxDC& dc);
   void DisplayText(wxDC& dc, const wxString& s, wxCoord x, wxCoord y);
   void DrawTimelineBar(wxDC& dc, int wd, int ht);
   
   // bitmaps for normal buttons
   wxBitmap normbutt[NUM_BUTTONS];

   #ifdef __WXMSW__
      // on Windows we need bitmaps for disabled buttons
      wxBitmap disnormbutt[NUM_BUTTONS];
   #endif
   
   // remember state of buttons to avoid unnecessary updates
   int buttstate[NUM_BUTTONS];
   
   // positioning data used by AddButton and AddSeparator
   int ypos, xpos, smallgap, biggap;

   wxBitmap* timelinebitmap;     // timeline bar bitmap
   int timelinebitmapwd;         // width of timeline bar bitmap
   int timelinebitmapht;         // height of timeline bar bitmap
   
   int digitwd;                  // width of digit in timeline bar font
   int digitht;                  // height of digit in timeline bar font
   int textascent;               // vertical adjustment used in DrawText calls
   wxFont* timelinefont;         // timeline bar font
};

BEGIN_EVENT_TABLE(TimelineBar, wxPanel)
   EVT_PAINT            (           TimelineBar::OnPaint)
   EVT_LEFT_DOWN        (           TimelineBar::OnMouseDown)
   EVT_BUTTON           (wxID_ANY,  TimelineBar::OnButton)
   EVT_COMMAND_SCROLL   (ID_SLIDER, TimelineBar::OnSlider)
   EVT_COMMAND_SCROLL   (ID_SCROLL, TimelineBar::OnScroll)
END_EVENT_TABLE()

// -----------------------------------------------------------------------------

static TimelineBar* tbarptr = NULL;    // global pointer to timeline bar
const int tbarht = 32;                 // height of timeline bar
static int mindelpos;                  // minimum x position of DELETE_BUTT

const int SCROLLHT = 17;               // height of scroll bar
const int PAGESIZE = 10;               // scroll amount when paging
const int BUTTON_WD = 24;              // nominal width of bitmap buttons

// timeline bar buttons (must be global to use Connect/Disconnect on Windows)
wxBitmapButton* tlbutt[NUM_BUTTONS];

// -----------------------------------------------------------------------------

TimelineBar::TimelineBar(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht)
   : wxPanel(parent, wxID_ANY, wxPoint(xorg,yorg), wxSize(wd,ht),
             wxNO_FULL_REPAINT_ON_RESIZE)
{
   #ifdef __WXGTK__
      // avoid erasing background on GTK+
      SetBackgroundStyle(wxBG_STYLE_CUSTOM);
   #endif

   // init bitmaps for normal state
   normbutt[RECORD_BUTT] =    wxBITMAP(record);
   normbutt[BACKWARDS_BUTT] = wxBITMAP(backwards);
   normbutt[FORWARDS_BUTT] =  wxBITMAP(forwards);
   normbutt[STOPPLAY_BUTT] =  wxBITMAP(stopplay);
   normbutt[DELETE_BUTT] =    wxBITMAP(deltime);

   #ifdef __WXMSW__
      for (int i = 0; i < NUM_BUTTONS; i++) {
         CreatePaleBitmap(normbutt[i], disnormbutt[i]);
      }
   #endif

   for (int i = 0; i < NUM_BUTTONS; i++) {
      buttstate[i] = 0;
   }

   // init position variables used by AddButton and AddSeparator
   #ifdef __WXGTK__
      // buttons are a different size in wxGTK
      xpos = 2;
      ypos = 2;
      smallgap = 6;
   #else
      xpos = 4;
      ypos = 4;
      smallgap = 4;
   #endif
   biggap = 16;

   // add buttons
   AddButton(RECORD_BUTT, _("Start recording"));
   AddSeparator();
   AddButton(BACKWARDS_BUTT, _("Play backwards"));
   AddButton(FORWARDS_BUTT, _("Play forwards"));

   // create font for text in timeline bar and set textascent for use in DisplayText
   #ifdef __WXMSW__
      // use smaller, narrower font on Windows
      timelinefont = wxFont::New(8, wxDEFAULT, wxNORMAL, wxNORMAL);
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
      timelinefont = wxFont::New(8, wxMODERN, wxNORMAL, wxNORMAL);
      textascent = 11;
   #elif defined(__WXMAC__)
      timelinefont = wxFont::New(10, wxMODERN, wxNORMAL, wxNORMAL);
      textascent = 10;
   #else
      timelinefont = wxFont::New(10, wxMODERN, wxNORMAL, wxNORMAL);
      textascent = 10;
   #endif
   if (timelinefont == NULL) Fatal(_("Failed to create timeline bar font!"));
   
   wxClientDC dc(this);
   SetTimelineFont(dc);
   dc.GetTextExtent(_("9"), &digitwd, &digitht);
   digitht -= 4;

   timelinebitmap = NULL;
   timelinebitmapwd = -1;
   timelinebitmapht = -1;
   
   // add slider
   int x, y;
   int sliderwd = 80;
   #ifdef __WXMAC__
      int sliderht = 15;
   #else
      int sliderht = 24;   // best for Windows (and wxGTK???)
   #endif
   x = xpos + 20 - smallgap;
   y = (tbarht - (sliderht + 1)) / 2;
   slider = new wxSlider(this, ID_SLIDER, 0, MINSPEED, MAXSPEED, wxPoint(x, y),
                         wxSize(sliderwd, sliderht),
                         wxSL_HORIZONTAL);
   if (slider == NULL) Fatal(_("Failed to create timeline slider!"));
   #ifdef __WXMAC__
      slider->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
      slider->Move(x, y+1);
   #endif
   #ifdef __WXMSW__
      slider->SetTick(0);     // doesn't seem to do anything!
   #endif
   xpos = x + sliderwd;
   
   // add scroll bar
   int scrollbarwd = 60;      // minimum width (ResizeTimelineBar will alter it)
   #ifdef __WXMAC__
      int scrollbarht = 15;   // must be this height on Mac
   #else
      int scrollbarht = SCROLLHT;
   #endif
   x = xpos + 20;
   y = (tbarht - (scrollbarht + 1)) / 2;
   framebar = new wxScrollBar(this, ID_SCROLL, wxPoint(x, y),
                              wxSize(scrollbarwd, scrollbarht),
                              wxSB_HORIZONTAL);
   if (framebar == NULL) Fatal(_("Failed to create timeline scroll bar!"));

   xpos = x + scrollbarwd + 4;
   mindelpos = xpos;
   AddButton(DELETE_BUTT, _("Delete timeline"));
   // ResizeTimelineBar will move this button to right end of scroll bar
   
   currframe = 0;
   autoplay = 0;
   speed = 0;
}

// -----------------------------------------------------------------------------

TimelineBar::~TimelineBar()
{
   delete timelinefont;
   delete timelinebitmap;
   delete slider;
   delete framebar;
}

// -----------------------------------------------------------------------------

void TimelineBar::SetTimelineFont(wxDC& dc)
{
   dc.SetFont(*timelinefont);
   dc.SetTextForeground(*wxBLACK);
   dc.SetBrush(*wxBLACK_BRUSH);           // avoids problem on Linux/X11
   dc.SetBackgroundMode(wxTRANSPARENT);
}

// -----------------------------------------------------------------------------

void TimelineBar::DisplayText(wxDC& dc, const wxString& s, wxCoord x, wxCoord y)
{
   // DrawText's y parameter is top of text box but we pass in baseline
   // so adjust by textascent which depends on platform and OS version -- yuk!
   dc.DrawText(s, x, y - textascent);
}

// -----------------------------------------------------------------------------

void TimelineBar::DrawTimelineBar(wxDC& dc, int wd, int ht)
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
   
   // draw gray border line at top edge
   #if defined(__WXMSW__)
      dc.SetPen(*wxGREY_PEN);
   #elif defined(__WXMAC__)
      wxPen linepen(wxColor(140,140,140));
      dc.SetPen(linepen);
   #else
      dc.SetPen(*wxLIGHT_GREY_PEN);
   #endif
   dc.DrawLine(0, 0, r.width, 0);
   dc.SetPen(wxNullPen);

   if (currlayer->algo->hyperCapable()) {
      tlbutt[RECORD_BUTT]->Show(true);
      tlbutt[BACKWARDS_BUTT]->Show(TimelineExists());
      tlbutt[FORWARDS_BUTT]->Show(TimelineExists());
      tlbutt[DELETE_BUTT]->Show(TimelineExists());
      slider->Show(TimelineExists());
      framebar->Show(TimelineExists());
   } else {
      tlbutt[RECORD_BUTT]->Show(false);
      tlbutt[BACKWARDS_BUTT]->Show(false);
      tlbutt[FORWARDS_BUTT]->Show(false);
      tlbutt[DELETE_BUTT]->Show(false);
      slider->Show(false);
      framebar->Show(false);
      
      SetTimelineFont(dc);
      dc.SetPen(*wxBLACK_PEN);
      int x = 6;
      int y = tbarht - 8;
      DisplayText(dc, _("The current algorithm does not support timelines."),
                  x, y - (SCROLLHT - digitht)/2);
      dc.SetPen(wxNullPen);
   }
}

// -----------------------------------------------------------------------------

void TimelineBar::OnPaint(wxPaintEvent& WXUNUSED(event))
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
      if (wd != timelinebitmapwd || ht != timelinebitmapht) {
         // need to create a new bitmap for timeline bar
         delete timelinebitmap;
         timelinebitmap = new wxBitmap(wd, ht);
         timelinebitmapwd = wd;
         timelinebitmapht = ht;
      }
      if (timelinebitmap == NULL) Fatal(_("Not enough memory to render timeline bar!"));
      wxBufferedPaintDC dc(this, *timelinebitmap);
   #endif
   
   if (!showtimeline) return;
   
   DrawTimelineBar(dc, wd, ht);
}

// -----------------------------------------------------------------------------

void TimelineBar::OnMouseDown(wxMouseEvent& WXUNUSED(event))
{
   // on Win/Linux we need to reset keyboard focus to viewport window
   viewptr->SetFocus();
   
   mainptr->showbanner = false;
   statusptr->ClearMessage();
}

// -----------------------------------------------------------------------------

void TimelineBar::OnButton(wxCommandEvent& event)
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
      case RECORD_BUTT:    cmdid = ID_RECORD; break;
      case BACKWARDS_BUTT: PlayTimeline(-1); return;
      case FORWARDS_BUTT:  PlayTimeline(1); return;
      case DELETE_BUTT:    cmdid = ID_DELTIME; break;
      default:             Warning(_("Unexpected button id!")); return;
   }
   
   // call MainFrame::OnMenu after OnButton finishes
   wxCommandEvent cmdevt(wxEVT_COMMAND_MENU_SELECTED, cmdid);
   wxPostEvent(mainptr->GetEventHandler(), cmdevt);
}

// -----------------------------------------------------------------------------

void TimelineBar::OnSlider(wxScrollEvent& event)
{
   WXTYPE type = event.GetEventType();

   if (type == wxEVT_SCROLL_LINEUP) {
      speed--;
      if (speed < MINSPEED) speed = MINSPEED;

   } else if (type == wxEVT_SCROLL_LINEDOWN) {
      speed++;
      if (speed > MAXSPEED) speed = MAXSPEED;

   } else if (type == wxEVT_SCROLL_PAGEUP) {
      speed -= PAGESIZE;
      if (speed < MINSPEED) speed = MINSPEED;

   } else if (type == wxEVT_SCROLL_PAGEDOWN) {
      speed += PAGESIZE;
      if (speed > MAXSPEED) speed = MAXSPEED;

   } else if (type == wxEVT_SCROLL_THUMBTRACK) {
      speed = event.GetPosition();
      if (speed < MINSPEED) speed = MINSPEED;
      if (speed > MAXSPEED) speed = MAXSPEED;

   } else if (type == wxEVT_SCROLL_THUMBRELEASE) {
      UpdateSlider();
   }

   #ifndef __WXMAC__
      viewptr->SetFocus();    // need on Win/Linux
   #endif
}

// -----------------------------------------------------------------------------

void TimelineBar::DisplayCurrentFrame()
{
   currlayer->algo->gotoframe(currframe);
   // should we use FitInView(0)??? less jerky but has the disadvantage
   // that pattern won't fill view if it shrinks when going backwards
   if (currlayer->autofit) viewptr->FitInView(1);
   mainptr->UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

void TimelineBar::OnScroll(wxScrollEvent& event)
{
   WXTYPE type = event.GetEventType();
   
   // best to stop autoplay if scroll bar is used
   if (autoplay != 0) {
      autoplay = 0;
      tbarptr->UpdateButtons();
   }

   if (type == wxEVT_SCROLL_LINEUP) {
      currframe--;
      if (currframe < 0) currframe = 0;
      DisplayCurrentFrame();

   } else if (type == wxEVT_SCROLL_LINEDOWN) {
      currframe++;
      if (currframe >= currlayer->algo->getframecount())
         currframe = currlayer->algo->getframecount() - 1;
      DisplayCurrentFrame();

   } else if (type == wxEVT_SCROLL_PAGEUP) {
      currframe -= PAGESIZE;
      if (currframe < 0) currframe = 0;
      DisplayCurrentFrame();

   } else if (type == wxEVT_SCROLL_PAGEDOWN) {
      currframe += PAGESIZE;
      if (currframe >= currlayer->algo->getframecount())
         currframe = currlayer->algo->getframecount() - 1;
      DisplayCurrentFrame();

   } else if (type == wxEVT_SCROLL_THUMBTRACK) {
      currframe = event.GetPosition();
      if (currframe < 0)
         currframe = 0;
      if (currframe >= currlayer->algo->getframecount())
         currframe = currlayer->algo->getframecount() - 1;
      DisplayCurrentFrame();

   } else if (type == wxEVT_SCROLL_THUMBRELEASE) {
      UpdateScrollBar();
   }

   #ifndef __WXMAC__
      viewptr->SetFocus();    // need on Win/Linux
   #endif
}

// -----------------------------------------------------------------------------

void TimelineBar::OnKillFocus(wxFocusEvent& event)
{
   int id = event.GetId();
   tlbutt[id]->SetFocus();   // don't let button lose focus
}

// -----------------------------------------------------------------------------

void TimelineBar::OnButtonDown(wxMouseEvent& event)
{
   // timeline bar button has been pressed
   int id = event.GetId();
   
   // connect a handler that keeps focus with the pressed button
   tlbutt[id]->Connect(id, wxEVT_KILL_FOCUS, wxFocusEventHandler(TimelineBar::OnKillFocus));

   event.Skip();
}

// -----------------------------------------------------------------------------

void TimelineBar::OnButtonUp(wxMouseEvent& event)
{
   // timeline bar button has been released
   int id = event.GetId();

   wxPoint pt = tlbutt[id]->ScreenToClient( wxGetMousePosition() );

   int wd, ht;
   tlbutt[id]->GetClientSize(&wd, &ht);
   wxRect r(0, 0, wd, ht);

   // diconnect kill-focus handler
   tlbutt[id]->Disconnect(id, wxEVT_KILL_FOCUS, wxFocusEventHandler(TimelineBar::OnKillFocus));
   viewptr->SetFocus();

   if (r.Contains(pt)) {
      // call OnButton
      wxCommandEvent buttevt(wxEVT_COMMAND_BUTTON_CLICKED, id);
      buttevt.SetEventObject(tlbutt[id]);
      tlbutt[id]->ProcessEvent(buttevt);
   }
}

// -----------------------------------------------------------------------------

void TimelineBar::AddButton(int id, const wxString& tip)
{
   tlbutt[id] = new wxBitmapButton(this, id, normbutt[id], wxPoint(xpos,ypos));
   if (tlbutt[id] == NULL) {
      Fatal(_("Failed to create timeline bar button!"));
   } else {
      xpos += BUTTON_WD + smallgap;
      tlbutt[id]->SetToolTip(tip);
      #ifdef __WXMSW__
         // fix problem with timeline bar buttons when generating/inscript
         // due to focus being changed to viewptr
         tlbutt[id]->Connect(id, wxEVT_LEFT_DOWN, wxMouseEventHandler(TimelineBar::OnButtonDown));
         tlbutt[id]->Connect(id, wxEVT_LEFT_UP, wxMouseEventHandler(TimelineBar::OnButtonUp));
      #endif
   }
}

// -----------------------------------------------------------------------------

void TimelineBar::AddSeparator()
{
   xpos += biggap - smallgap;
}

// -----------------------------------------------------------------------------

void TimelineBar::EnableButton(int id, bool enable)
{
   if (enable == tlbutt[id]->IsEnabled()) return;

   #ifdef __WXMSW__
      tlbutt[id]->SetBitmapDisabled(disnormbutt[id]);
   #endif

   tlbutt[id]->Enable(enable);
}

// -----------------------------------------------------------------------------

void TimelineBar::UpdateButtons()
{
   if (autoplay == 0) {
      if (buttstate[BACKWARDS_BUTT] != 1) {
         buttstate[BACKWARDS_BUTT] = 1;
         tlbutt[BACKWARDS_BUTT]->SetBitmapLabel(normbutt[BACKWARDS_BUTT]);
         tlbutt[BACKWARDS_BUTT]->SetToolTip(_("Play backwards"));
      }
      if (buttstate[FORWARDS_BUTT] != 1) {
         buttstate[FORWARDS_BUTT] = 1;
         tlbutt[FORWARDS_BUTT]->SetBitmapLabel(normbutt[FORWARDS_BUTT]);
         tlbutt[FORWARDS_BUTT]->SetToolTip(_("Play forwards"));
      }
   } else if (autoplay > 0) {
      if (buttstate[BACKWARDS_BUTT] != 1) {
         buttstate[BACKWARDS_BUTT] = 1;
         tlbutt[BACKWARDS_BUTT]->SetBitmapLabel(normbutt[BACKWARDS_BUTT]);
         tlbutt[BACKWARDS_BUTT]->SetToolTip(_("Play backwards"));
      }
      if (buttstate[FORWARDS_BUTT] != -1) {
         buttstate[FORWARDS_BUTT] = -1;
         tlbutt[FORWARDS_BUTT]->SetBitmapLabel(normbutt[STOPPLAY_BUTT]);
         tlbutt[FORWARDS_BUTT]->SetToolTip(_("Stop playing"));
      }
   } else { // autoplay < 0
      if (buttstate[BACKWARDS_BUTT] != -1) {
         buttstate[BACKWARDS_BUTT] = -1;
         tlbutt[BACKWARDS_BUTT]->SetBitmapLabel(normbutt[STOPPLAY_BUTT]);
         tlbutt[BACKWARDS_BUTT]->SetToolTip(_("Stop playing"));
      }
      if (buttstate[FORWARDS_BUTT] != 1) {
         buttstate[FORWARDS_BUTT] = 1;
         tlbutt[FORWARDS_BUTT]->SetBitmapLabel(normbutt[FORWARDS_BUTT]);
         tlbutt[FORWARDS_BUTT]->SetToolTip(_("Play forwards"));
      }
   }
   
   if (showtimeline) {
      tlbutt[BACKWARDS_BUTT]->Refresh(false);
      tlbutt[FORWARDS_BUTT]->Refresh(false);
   }
}

// -----------------------------------------------------------------------------

void TimelineBar::UpdateSlider()
{
   slider->SetValue(speed);
}

// -----------------------------------------------------------------------------

void TimelineBar::UpdateScrollBar()
{
   framebar->SetScrollbar(currframe, 1,
                          currlayer->algo->getframecount(), PAGESIZE, true);
}

// -----------------------------------------------------------------------------

void CreateTimelineBar(wxWindow* parent)
{
   // create timeline bar underneath given window
   int wd, ht;
   parent->GetClientSize(&wd, &ht);

   tbarptr = new TimelineBar(parent, 0, ht - tbarht, wd, tbarht);
   if (tbarptr == NULL) Fatal(_("Failed to create timeline bar!"));
      
   tbarptr->Show(showtimeline);
}

// -----------------------------------------------------------------------------

int TimelineBarHeight() {
   return (showtimeline ? tbarht : 0);
}

// -----------------------------------------------------------------------------

void UpdateTimelineBar(bool active)
{
   if (tbarptr && showtimeline) {
      if (viewptr->waitingforclick || inscript) active = false;
      
      // may need to change bitmap in backwards/forwards button
      tbarptr->UpdateButtons();

      tbarptr->EnableButton(RECORD_BUTT, active && currlayer->algo->hyperCapable());
      tbarptr->EnableButton(BACKWARDS_BUTT, active && TimelineExists());
      tbarptr->EnableButton(FORWARDS_BUTT, active && TimelineExists());
      tbarptr->EnableButton(DELETE_BUTT, active && TimelineExists());
      
      tbarptr->UpdateSlider();
      tbarptr->UpdateScrollBar();
      
      tbarptr->Refresh(false);
      tbarptr->Update();
   }
}

// -----------------------------------------------------------------------------

void ResizeTimelineBar(int y, int wd)
{
   if (tbarptr) {
      tbarptr->SetSize(0, y, wd, tbarht);
      // change width of scroll bar to nearly fill timeline bar
      wxRect r = tbarptr->framebar->GetRect();
      r.width = wd - r.x - 20 - BUTTON_WD - 20;
      tbarptr->framebar->SetSize(r);
      
      // move DELETE_BUTT to right edge of timeline bar
      r = tlbutt[DELETE_BUTT]->GetRect();
      r.x = wd - 20 - BUTTON_WD;
      if (r.x < mindelpos && TimelineExists()) r.x = mindelpos;
      tlbutt[DELETE_BUTT]->SetSize(r);
   }
}

// -----------------------------------------------------------------------------

void ToggleTimelineBar()
{
   showtimeline = !showtimeline;
   wxRect r = bigview->GetRect();
   
   if (showtimeline) {
      // show timeline bar underneath viewport window
      r.height -= tbarht;
      ResizeTimelineBar(r.y + r.height, r.width);
   } else {
      // hide timeline bar
      r.height += tbarht;
   }
   bigview->SetSize(r);
   tbarptr->Show(showtimeline);    // needed on Windows
   
   mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

void StartStopRecording()
{
   if (!inscript && currlayer->algo->hyperCapable()) {
      Warning(_("Recording isn't implemented yet!"));//!!!
      /* !!! do something like this:
      if (!showtimeline) ToggleTimelineBar();
      if (currlayer->algo->isrecording()) {
         // stop the current recording
         currlayer->algo->stoprecording();
         mainptr->Stop();
         //!!!??? sync undo/redo history
         mainptr->UpdateUserInterface(true);
      } else {
         if (currlayer->algo->isEmpty()) {   
            statusptr->ErrorMessage(_("There is no pattern to record."));
            return;
         }
         if (TimelineExists()) {
            // extend the existing timeline
            currlayer->algo->extendtimeline();
         }
         // start recording a new timeline
         if (currlayer->algo->startrecording(currlayer->currbase, currlayer->currexpo) > 0) {
            mainptr->GeneratePattern();
         }
      }
      !!! */
   }
}

// -----------------------------------------------------------------------------

void DeleteTimeline()
{
   if (!inscript && TimelineExists()) {
      // stop any recording
      if (currlayer->algo->isrecording()) currlayer->algo->stoprecording();

      currframe = 0;
      autoplay = 0;
      speed = 0;
      
      // prevent user selecting Reset/Undo by making current frame
      // the new starting pattern
      //!!! remove these 2 lines if we solve PROBLEM below!!!
      currlayer->startgen = currlayer->algo->getGeneration();
      currlayer->savestart = true;     // do NOT reload .mc file
      
      /* PROBLEM: SaveStartingPattern writes a .mc file with timeline!!!
         ditto for temp .mc files written by RememberGenStart/Finish!!!
         SOLN: set flag that tells writeNativeFormat to only write current frame???
         or add writetimeline(bool) call to temporarily disable/enable writing timeline???
      if (currframe > 0) {
         // do stuff so user can select Reset/Undo to go back to 1st frame
         currlayer->algo->gotoframe(0);
         if (currlayer->algo->getGeneration() == currlayer->startgen) {
            mainptr->SaveStartingPattern();
         }
         if (allowundo) currlayer->undoredo->RememberGenStart();
         
         // return to the current frame
         currlayer->algo->gotoframe(currframe);
         if (allowundo) currlayer->undoredo->RememberGenFinish();
      }
      */

      currlayer->algo->destroytimeline();
      mainptr->UpdateUserInterface(true);
   }
}

// -----------------------------------------------------------------------------

void InitTimelineFrame()
{
   // the user has just loaded a .mc file with a timeline,
   // so prepare to display the first frame
   currlayer->algo->gotoframe(0);
   currframe = 0;
   autoplay = 0;
   speed = 0;

   // first frame is starting gen (need this for DeleteTimeline)
   currlayer->startgen = currlayer->algo->getGeneration();
   currlayer->savestart = true;     // do NOT reload .mc file
}

// -----------------------------------------------------------------------------

bool TimelineExists()
{
   // on Linux MainFrame::OnIdle is called before CreateTimelineBar sets tbarptr
   return tbarptr && currlayer->algo->getframecount() > 0;
}

// -----------------------------------------------------------------------------

void DoIdleTimeline()
{
   // assume currlayer->algo->getframecount() > 0
   if (autoplay == 0) return;
   
   int frameinc = 1;
   long delay = 0;
   // if speed is > 0 then we skip 2^speed frames
   if (speed > 0) frameinc = 1 << speed;
   // if speed is < 0 then we delay 2^(-speed) msecs between each frame
   if (speed < 0) {
      delay = 1 << (-speed);
      if (stopwatch->Time() - lastframe < delay) {
         #ifndef __WXMAC__
            // need to send another idle event on Windows and Linux
            wxWakeUpIdle();
            wxMilliSleep(1);
         #endif
         return;
      }
   }
   
   #ifdef __WXMSW__
      // need to slow things down on Windows!
      wxMilliSleep(20);
   #endif
   
   if (autoplay > 0) {
      // play timeline forwards
      currframe += frameinc;
      if (currframe >= currlayer->algo->getframecount() - 1) {
         currframe = currlayer->algo->getframecount() - 1;
         // autoplay = 0;           // stop when we hit last frame???
         autoplay = -1;             // reverse direction when we hit last frame
         tbarptr->UpdateButtons();
      }
   } else {
      // autoplay < 0 so play timeline backwards
      currframe -= frameinc;
      if (currframe <= 0) {
         currframe = 0;
         // autoplay = 0;           // stop when we hit first frame???
         autoplay = 1;              // reverse direction when we hit first frame
         tbarptr->UpdateButtons();
      }
   }
   
   tbarptr->DisplayCurrentFrame();
   tbarptr->UpdateScrollBar();
   lastframe = stopwatch->Time();
   wxWakeUpIdle();                  // send another idle event
}

// -----------------------------------------------------------------------------

void PlayTimeline(int direction)
{
   if (direction > 0 && autoplay > 0) {
      autoplay = 0;
   } else if (direction < 0 && autoplay < 0) {
      autoplay = 0;
   } else {
      autoplay = direction;
   }
   if (showtimeline) tbarptr->UpdateButtons();
}

// -----------------------------------------------------------------------------

void PlayTimelineFaster()
{
   if (speed < MAXSPEED) {
      speed++;
      if (showtimeline) tbarptr->UpdateSlider();
   }
}

// -----------------------------------------------------------------------------

void PlayTimelineSlower()
{
   if (speed > MINSPEED) {
      speed--;
      if (showtimeline) tbarptr->UpdateSlider();
   }
}

// -----------------------------------------------------------------------------

void ResetTimelineSpeed()
{
   speed = 0;
   if (showtimeline) tbarptr->UpdateSlider();
}
