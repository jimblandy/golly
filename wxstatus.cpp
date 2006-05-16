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

#include "wx/dcbuffer.h"   // for wxBufferedPaintDC

#include "bigint.h"
#include "lifealgo.h"

#include "wxgolly.h"       // for wxGetApp, etc
#include "wxutils.h"       // for Fatal, FillRect
#include "wxprefs.h"       // for hashing, mindelay, maxdelay, etc
#include "wxview.h"        // for viewptr->...
#include "wxmain.h"        // for mainptr->...
#include "wxscript.h"      // for InScript
#include "wxstatus.h"

// -----------------------------------------------------------------------------

// the following is a bit messy but gives good results on all platforms

const int LINEHT = 14;                    // distance between each baseline
const int DESCHT = 4;                     // descender height
const int STATUS_HT = 2*LINEHT+DESCHT;    // normal status bar height
const int STATUS_EXHT = 7*LINEHT+DESCHT;  // height when showing exact numbers

const int BASELINE1 = LINEHT-2;           // baseline of 1st line
const int BOTGAP = 6;                     // to get baseline of message line

// these baseline values are used when showexact is true
const int GENLINE = 1*LINEHT-2;
const int POPLINE = 2*LINEHT-2;
const int SCALELINE = 3*LINEHT-2;
const int STEPLINE = 4*LINEHT-2;
const int XLINE = 5*LINEHT-2;
const int YLINE = 6*LINEHT-2;

// these horizontal offets are used when showexact is true
int h_gen_ex, h_pop_ex, h_x_ex, h_y_ex;

// -----------------------------------------------------------------------------

void StatusBar::ClearMessage()
{
   if (InScript()) return;                   // let script control messages
   if (viewptr->waitingforclick) return;     // don't clobber message
   statusmsg[0] = 0;
   if (statusht > 0) {
      int wd, ht;
      GetClientSize(&wd, &ht);
      if (wd > 0 && ht > 0) {
         // update bottom line
         wxRect r = wxRect( wxPoint(0,statusht-BOTGAP+DESCHT-LINEHT),
                            wxPoint(wd-1,ht-1) );
         Refresh(false, &r);
         // don't call Update() otherwise Win/X11 users see blue & yellow bands
         // when toggling hashing option
      }
   }
}

// -----------------------------------------------------------------------------

void StatusBar::DisplayMessage(const char *s)
{
   strncpy(statusmsg, s, sizeof(statusmsg));
   if (statusht > 0) {
      int wd, ht;
      GetClientSize(&wd, &ht);
      if (wd > 0 && ht > 0) {
         // update bottom line
         wxRect r = wxRect( wxPoint(0,statusht-BOTGAP+DESCHT-LINEHT),
                            wxPoint(wd-1,ht-1) );
         Refresh(false, &r);
         // show message immediately
         Update();
      }
   }
}

// -----------------------------------------------------------------------------

void StatusBar::ErrorMessage(const char *s)
{
   wxBell();
   DisplayMessage(s);
}

// -----------------------------------------------------------------------------

void StatusBar::SetMessage(const char *s)
{
   // set message string without displaying it
   strncpy(statusmsg, s, sizeof(statusmsg));
}

// -----------------------------------------------------------------------------

void StatusBar::UpdateXYLocation()
{
   int wd, ht;
   GetClientSize(&wd, &ht);
   if (wd > h_xy && ht > 0) {
      wxRect r;
      if (showexact)
         r = wxRect( wxPoint(0, XLINE+DESCHT-LINEHT), wxPoint(wd-1, YLINE+DESCHT) );
      else
         r = wxRect( wxPoint(h_xy, 0), wxPoint(wd-1, BASELINE1+DESCHT) );
      Refresh(false, &r);
      // no need to Update() immediately
   }
}

// -----------------------------------------------------------------------------

void StatusBar::CheckMouseLocation(bool active)
{
   if (statusht == 0)
      return;

   if ( !active ) {
      // main window is not in front so clear XY location
      showxy = false;
      UpdateXYLocation();
      return;
   }

   // may need to update XY location in status bar
   bigint xpos, ypos;
   if ( viewptr->GetCellPos(xpos, ypos) ) {
      if ( xpos != currx || ypos != curry ) {
         // show new XY location
         currx = xpos;
         curry = ypos;
         showxy = true;
         UpdateXYLocation();
      } else if (!showxy) {
         showxy = true;
         UpdateXYLocation();
      }
   } else {
      // outside viewport so clear XY location
      showxy = false;
      UpdateXYLocation();
   }
}

// -----------------------------------------------------------------------------

void StatusBar::SetStatusFont(wxDC &dc)
{
   dc.SetFont(*statusfont);
   dc.SetTextForeground(*wxBLACK);
   dc.SetBrush(*wxBLACK_BRUSH);           // avoids problem on Linux/X11
   dc.SetBackgroundMode(wxTRANSPARENT);
}

// -----------------------------------------------------------------------------

void StatusBar::DisplayText(wxDC &dc, const char *s, wxCoord x, wxCoord y)
{
   // DrawText's y parameter is top of text box but we pass in baseline
   // so adjust by textascent which depends on platform and OS version -- yuk!
   dc.DrawText(_(s), x, y - textascent);
}

// -----------------------------------------------------------------------------

// ping-pong in the buffer so we can use multiple at a time
const int STRINGIFYSIZE = 20;

const char* StatusBar::Stringify(double d)
{
   static char buf[120];
   static char *p = buf;
   if ( p + STRINGIFYSIZE + 1 >= buf + sizeof(buf) )
      p = buf;
   // use e notation for abs values > 1 billion (agrees with min & max_coord)
   if ( fabs(d) <= 1000000000.0 ) {
      if ( d < 0 ) {
         d = - d;
         *p++ = '-';
      }
      sprintf(p, "%.f", d);
      int len = strlen(p);
      int commas = ((len + 2) / 3) - 1;
      int dest = len + commas;
      int src = len;
      p[dest] = 0;
      while (commas > 0) {
         p[--dest] = p[--src];
         p[--dest] = p[--src];
         p[--dest] = p[--src];
         p[--dest] = ',';
         commas--;
      }
      if ( p[-1] == '-' ) p--;
   } else
      sprintf(p, "%g", d);
   char *r = p;
   p += strlen(p)+1;
   return r;
}

const char* StatusBar::Stringify(const bigint &b)
{
   return Stringify(b.todouble());
}

// -----------------------------------------------------------------------------

int StatusBar::GetCurrentDelay()
{
   int gendelay = mindelay * (1 << (-mainptr->GetWarp() - 1));
   if (gendelay > maxdelay) gendelay = maxdelay;
   return gendelay;
}

// -----------------------------------------------------------------------------

void StatusBar::DrawStatusBar(wxDC &dc, wxRect &updaterect)
{
   int wd, ht;
   GetClientSize(&wd, &ht);
   if (wd < 1 || ht < 1) return;

   wxRect r = wxRect(0, 0, wd, ht);
   FillRect(dc, r, hashing ? *brush_hlife : *brush_qlife);

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
   
   if (updaterect.y >= statusht-BOTGAP+DESCHT-LINEHT) {
      // only show possible message in bottom line -- see below
      
   } else if (showexact) {
      // might only need to display X and Y lines
      if (updaterect.y < XLINE+DESCHT-LINEHT) {
         DisplayText(dc, "Generation =", h_gen, GENLINE);
         DisplayText(dc, curralgo == NULL ? "0" : curralgo->getGeneration().tostring(),
                     h_gen_ex, GENLINE);
   
         DisplayText(dc, "Population =", h_gen, POPLINE);
         const char *p = curralgo == NULL ? "0" : curralgo->getPopulation().tostring();
         DisplayText(dc, *p == '-' ? "(pending)" : p, h_pop_ex, POPLINE);
         
         // no real need to show scale as an exact number???
         if (viewptr->GetMag() < 0) {
            sprintf(strbuf, "Scale = 2^%d:1", -viewptr->GetMag());
         } else {
            sprintf(strbuf, "Scale = 1:%d", 1 << viewptr->GetMag());
         }
         DisplayText(dc, strbuf, h_gen, SCALELINE);
         
         if (mainptr->GetWarp() < 0) {
            // show delay in secs
            sprintf(strbuf, "Delay = %gs", (double)GetCurrentDelay() / 1000.0);
         } else {
            // no real need to show step as an exact number???
            if (hashing) {
               sprintf(strbuf, "Step = %d^%d", hbasestep, mainptr->GetWarp());
            } else {
               sprintf(strbuf, "Step = %d^%d", qbasestep, mainptr->GetWarp());
            }
         }
         DisplayText(dc, strbuf, h_gen, STEPLINE);
      }
      
      DisplayText(dc, "X =", h_gen, XLINE);
      DisplayText(dc, "Y =", h_gen, YLINE);
      if (showxy) {
         bigint xo, yo;
         bigint xpos = currx;   xpos -= viewptr->originx;
         bigint ypos = curry;   ypos -= viewptr->originy;
         if (mathcoords) {
            // Y values increase upwards
            bigint temp = 0;
            temp -= ypos;
            ypos = temp;
         }
         DisplayText(dc, xpos.tostring(), h_x_ex, XLINE);
         DisplayText(dc, ypos.tostring(), h_y_ex, YLINE);
      }

   } else {
      // show info in top line
      if (updaterect.x < h_xy) {
         // show all info
         sprintf(strbuf, "Generation=%s",
                 curralgo == NULL ? "0" : Stringify(curralgo->getGeneration()));
         DisplayText(dc, strbuf, h_gen, BASELINE1);
      
         double pop = curralgo == NULL ? 0.0 : curralgo->getPopulation().todouble();
         if (pop >= 0) {
            sprintf(strbuf, "Population=%s", Stringify(pop));
         } else {
            sprintf(strbuf, "Population=(pending)");
         }
         DisplayText(dc, strbuf, h_pop, BASELINE1);
      
         if (viewptr->GetMag() < 0) {
            sprintf(strbuf, "Scale=2^%d:1", -viewptr->GetMag());
         } else {
            sprintf(strbuf, "Scale=1:%d", 1 << viewptr->GetMag());
         }
         DisplayText(dc, strbuf, h_scale, BASELINE1);
         
         if (mainptr->GetWarp() < 0) {
            // show delay in secs
            sprintf(strbuf, "Delay=%gs", (double)GetCurrentDelay() / 1000.0);
         } else {
            if (hashing) {
               sprintf(strbuf, "Step=%d^%d", hbasestep, mainptr->GetWarp());
            } else {
               sprintf(strbuf, "Step=%d^%d", qbasestep, mainptr->GetWarp());
            }
         }
         DisplayText(dc, strbuf, h_step, BASELINE1);
      }
      if (showxy) {
         bigint xo, yo;
         bigint xpos = currx;   xpos -= viewptr->originx;
         bigint ypos = curry;   ypos -= viewptr->originy;
         if (mathcoords) {
            // Y values increase upwards
            bigint temp = 0;
            temp -= ypos;
            ypos = temp;
         }
         sprintf(strbuf, "XY=%s %s", Stringify(xpos), Stringify(ypos));
      } else {
         sprintf(strbuf, "XY=");
      }
      DisplayText(dc, strbuf, h_xy, BASELINE1);
   }

   if (statusmsg[0]) {
      // display status message on bottom line
      DisplayText(dc, statusmsg, h_gen, statusht - BOTGAP);
   }
}

// -----------------------------------------------------------------------------

// event table and handlers:

BEGIN_EVENT_TABLE(StatusBar, wxWindow)
   EVT_PAINT            (StatusBar::OnPaint)
   EVT_LEFT_DOWN        (StatusBar::OnMouseDown)
   EVT_LEFT_DCLICK      (StatusBar::OnMouseDown)
   EVT_ERASE_BACKGROUND (StatusBar::OnEraseBackground)
END_EVENT_TABLE()

// -----------------------------------------------------------------------------

void StatusBar::OnPaint(wxPaintEvent& WXUNUSED(event))
{
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

// -----------------------------------------------------------------------------

bool StatusBar::ClickInScaleBox(int x, int y)
{
   if (showexact)
      return x >= 0 && y > (SCALELINE+DESCHT-LINEHT) && y <= (SCALELINE+DESCHT);
   else
      return x >= h_scale && x <= h_step - 20 && y <= (BASELINE1+DESCHT);
}

// -----------------------------------------------------------------------------

bool StatusBar::ClickInStepBox(int x, int y)
{
   if (showexact)
      return x >= 0 && y > (STEPLINE+DESCHT-LINEHT) && y <= (STEPLINE+DESCHT);
   else
      return x >= h_step && x <= h_xy - 20 && y <= (BASELINE1+DESCHT);
}

// -----------------------------------------------------------------------------

void StatusBar::OnMouseDown(wxMouseEvent& event)
{
   if (InScript()) return;    // let script control scale and step
   ClearMessage();
   if ( ClickInScaleBox(event.GetX(), event.GetY()) ) {
      if (viewptr->GetMag() != 0) {
         // reset scale to 1:1
         viewptr->SetMag(0);
      }
   } else if ( ClickInStepBox(event.GetX(), event.GetY()) ) {
      if (mainptr->GetWarp() != 0) {
         // reset step to 1 gen
         mainptr->SetWarp(0);
         // update status bar
         Refresh(false, NULL);
         Update();
      }
   }
   #ifdef __WXX11__
      // make sure viewport keeps keyboard focus
      viewptr->SetFocus();
   #endif
}

// -----------------------------------------------------------------------------

void StatusBar::OnEraseBackground(wxEraseEvent& WXUNUSED(event))
{
   // do nothing because we'll be painting the entire status bar
}

// -----------------------------------------------------------------------------

// create the status bar window

StatusBar::StatusBar(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht)
   : wxWindow(parent, wxID_ANY, wxPoint(xorg,yorg), wxSize(wd,ht),
              wxNO_BORDER | wxFULL_REPAINT_ON_RESIZE)
{
   // avoid erasing background on GTK+
   SetBackgroundStyle(wxBG_STYLE_CUSTOM);

   // create colored brushes for background
   brush_qlife = new wxBrush(wxColour(0xFF,0xFF,0xCE));     // pale yellow
   brush_hlife = new wxBrush(wxColour(0xE2,0xFA,0xF8));     // pale blue
   if (brush_qlife == NULL || brush_hlife == NULL)
      Fatal("Failed to create brushes for status bar!");

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
   #elif defined(__WXGTK__)
      // use smaller font on GTK
      statusfont = wxFont::New(8, wxMODERN, wxNORMAL, wxNORMAL);
      textascent = 11;
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
   // when showexact is false:
   dc.GetTextExtent("Generation=9.999999e+999", &textwd, &textht);
   h_pop = h_gen + textwd + mingap;
   dc.GetTextExtent("Population=9.999999e+999", &textwd, &textht);
   h_scale = h_pop + textwd + mingap;
   dc.GetTextExtent("Scale=2^9999:1", &textwd, &textht);
   h_step = h_scale + textwd + mingap;
   dc.GetTextExtent("Step=10^9999", &textwd, &textht);
   h_xy = h_step + textwd + mingap;
   // when showexact is true:
   dc.GetTextExtent("Generation = ", &textwd, &textht);
   h_gen_ex = h_gen + textwd;
   dc.GetTextExtent("Population = ", &textwd, &textht);
   h_pop_ex = h_gen + textwd;
   dc.GetTextExtent("X = ", &textwd, &textht);
   h_x_ex = h_gen + textwd;
   dc.GetTextExtent("Y = ", &textwd, &textht);
   h_y_ex = h_gen + textwd;
   dc.EndDrawing();

   // status bar is initially visible
   statusht = showexact ? STATUS_EXHT : STATUS_HT;
   showxy = false;
   #ifndef __WXMAC__
      statbitmap = NULL;
      statbitmapwd = -1;
      statbitmapht = -1;
   #endif
}

// -----------------------------------------------------------------------------

StatusBar::~StatusBar()
{
   if (brush_qlife) delete brush_qlife;
   if (brush_hlife) delete brush_hlife;
   if (statusfont) delete statusfont;
   #ifndef __WXMAC__
      if (statbitmap) delete statbitmap;
   #endif
}
