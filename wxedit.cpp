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

#include "bigint.h"
#include "lifealgo.h"

#include "wxgolly.h"       // for viewptr, statusptr
#include "wxutils.h"       // for Fatal
#include "wxprefs.h"       // for showedit, etc
#include "wxstatus.h"      // for statusptr->...
#include "wxscript.h"      // for inscript
#include "wxview.h"        // for viewptr->...
#include "wxalgos.h"       // for AlgoData, algoinfo
#include "wxlayer.h"       // for currlayer, LayerBarHeight
#include "wxedit.h"

// -----------------------------------------------------------------------------

// Define edit bar window:

// derive from wxPanel so we get current theme's background color on Windows
class EditBar : public wxPanel
{
public:
   EditBar(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht);
   ~EditBar();

private:
   // any class wishing to process wxWidgets events must use this macro
   DECLARE_EVENT_TABLE()

   // event handlers
   void OnPaint(wxPaintEvent& event);
   void OnMouseDown(wxMouseEvent& event);

   void SetEditFont(wxDC& dc);
   void DisplayText(wxDC& dc, const wxString& s, wxCoord x, wxCoord y);
   void DrawEditBar(wxDC& dc, int wd, int ht);

   wxBitmap* editbitmap;         // edit bar bitmap
   int editbitmapwd;             // width of edit bar bitmap
   int editbitmapht;             // height of edit bar bitmap
   
   int h_col1;                   // horizontal position of labels
   int h_col2;                   // horizontal position of info for state 0
   int digitwd;                  // width of digit in edit bar font
   int textascent;               // vertical adjustment used in DrawText calls
   wxFont* editfont;             // edit bar font
};

BEGIN_EVENT_TABLE(EditBar, wxPanel)
   EVT_PAINT      (EditBar::OnPaint)
   EVT_LEFT_DOWN  (EditBar::OnMouseDown)
END_EVENT_TABLE()

// -----------------------------------------------------------------------------

EditBar* editbarptr = NULL;         // global pointer to edit bar
const int editbarht = 48;           // height of edit bar

const int LINEHT = 14;              // distance between each baseline
const int BASELINE1 = LINEHT-1;     // baseline of 1st line
const int BASELINE2 = BASELINE1+LINEHT;   // baseline of 2nd line
const int BASELINE3 = BASELINE2+LINEHT;   // baseline of 3rd line
const int COLWD = 20;               // column width of state/color/icon info
const int BOXWD = 9;                // width (and height) of color/icon boxes

// -----------------------------------------------------------------------------

EditBar::EditBar(wxWindow* parent, wxCoord xorg, wxCoord yorg, int wd, int ht)
   : wxPanel(parent, wxID_ANY, wxPoint(xorg,yorg), wxSize(wd,ht),
             wxNO_FULL_REPAINT_ON_RESIZE)
{
   #ifdef __WXGTK__
      // avoid erasing background on GTK+
      SetBackgroundStyle(wxBG_STYLE_CUSTOM);
   #endif

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
   dc.GetTextExtent(_("9"), &digitwd, &textht);

   editbitmap = NULL;
   editbitmapwd = -1;
   editbitmapht = -1;
}

// -----------------------------------------------------------------------------

EditBar::~EditBar()
{
   delete editfont;
   delete editbitmap;
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
   
   SetEditFont(dc);     // for DisplayText calls

   DisplayText(dc, _("State:"), h_col1, BASELINE1);
   DisplayText(dc, _("Color:"), h_col1, BASELINE2);
   DisplayText(dc, _("Icon:"),  h_col1, BASELINE3);

   AlgoData* ad = algoinfo[currlayer->algtype];
   wxBitmap** iconmaps = ad->icons7x7;

   // set rgb values for dead cells
   ad->cellr[0] = swapcolors ? livergb[currindex]->Red() : deadrgb->Red();
   ad->cellg[0] = swapcolors ? livergb[currindex]->Green() : deadrgb->Green();
   ad->cellb[0] = swapcolors ? livergb[currindex]->Blue() : deadrgb->Blue();
   
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
      int x;
      
      // draw state value
      strbuf.Printf(_("%d"), i);
      x = h_col2 + i * COLWD + (COLWD - strbuf.length() * digitwd) / 2;
      DisplayText(dc, strbuf, x, BASELINE1);
      
      // draw color box
      x = 1 + h_col2 + i * COLWD + (COLWD - BOXWD) / 2;
      wxColor color(ad->cellr[i], ad->cellg[i], ad->cellb[i]);
      wxBrush brush(color);
      wxRect r(x, BASELINE2 - BOXWD, BOXWD, BOXWD);
      dc.SetBrush(brush);
      dc.DrawRectangle(r);
      dc.SetBrush(wxNullBrush);
      
      // draw icon box or "x" if no icon
      if (iconmaps && iconmaps[i]) {
         wxRect r(x, BASELINE3 - BOXWD, BOXWD, BOXWD);
         dc.SetBrush(*deadbrush);
         dc.DrawRectangle(r);
         dc.SetBrush(wxNullBrush);
         dc.DrawBitmap(*iconmaps[i], x + 1, BASELINE3 - BOXWD + 1, true);
      } else {
         x = h_col2 + i * COLWD + (COLWD - digitwd) / 2;
         DisplayText(dc, _("x"), x, BASELINE3);
      }
   }

   if (currlayer->algo->NumCellStates() == 2) {
      // restore live cell color changed above
      ad->cellr[1] = saver;
      ad->cellg[1] = saveg;
      ad->cellb[1] = saveb;
   }
   
   // reset drawing state in case it's no longer valid (due to algo/rule change)
   if (currlayer->drawingstate >= currlayer->algo->NumCellStates()) {
      currlayer->drawingstate = 1;
   }
   
   // draw rect around current drawing state
   int x = 1 + h_col2 + COLWD * currlayer->drawingstate;
   r = wxRect(x, 2, COLWD - 1, editbarht - 4);
   dc.SetBrush(*wxTRANSPARENT_BRUSH);
   dc.DrawRectangle(r);
   dc.SetBrush(wxNullBrush);

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
   if (inscript) return;    // let script control drawing state
   
   statusptr->ClearMessage();

   // user can change drawing state by clicking in appropriate box
   int x = event.GetX();
   int right = h_col2 + COLWD * currlayer->algo->NumCellStates();
   int box = -1;
   
   if (x > h_col2 && x < right) {
      box = (x - h_col2) / COLWD;
   }
   
   if (box >= 0 && box < currlayer->algo->NumCellStates() &&
       currlayer->drawingstate != box) {
      currlayer->drawingstate = box;
      Refresh(false);
      Update();
   }
   
   // on Windows we need to reset keyboard focus to viewport window
   viewptr->SetFocus();
}

// -----------------------------------------------------------------------------

void CreateEditBar(wxWindow* parent)
{
   int wd, ht;
   parent->GetClientSize(&wd, &ht);

   int y = 0;
   if (showlayer) y += LayerBarHeight();
   editbarptr = new EditBar(parent, 0, y, wd, editbarht);
   if (editbarptr == NULL) Fatal(_("Failed to create edit bar!"));

   //!!! eventually edit bar will all have all buttons for setting cursor mode???
   //!!! editbarptr->AddButton(EYE_DROPPER, 0, _("Eye dropper"));
      
   editbarptr->Show(showedit);
}

// -----------------------------------------------------------------------------

int EditBarHeight() {
   return editbarht;
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
      //!!! eventually edit bar will all have all buttons for setting cursor mode???
      // editbarptr->EnableButton(EYE_DROPPER, active);
      
      editbarptr->Refresh(false);
      editbarptr->Update();
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
}

// -----------------------------------------------------------------------------

void ShiftEditBar(int yamount)
{
   int x, y;
   editbarptr->GetPosition(&x, &y);
   editbarptr->Move(x, y + yamount);
}
