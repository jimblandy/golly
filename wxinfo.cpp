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

#include "readpattern.h"   // for readcomments

#include "wxgolly.h"       // so wxGetApp returns reference to GollyApp
#include "wxutils.h"       // for Warning
#include "wxprefs.h"       // for infox/y/wd/ht and mininfo*
#include "wxinfo.h"

// -----------------------------------------------------------------------------

// define a modeless window to display pattern info:

class InfoFrame : public wxFrame
{
public:
   InfoFrame(char *comments);

private:
   // any class wishing to process wxWidgets events must use this macro
   DECLARE_EVENT_TABLE()

   // event handlers
   void OnCloseButton(wxCommandEvent& event);
   void OnClose(wxCloseEvent& event);
};

InfoFrame *infoptr = NULL;    // pattern info window

wxFrame* GetInfoFrame() {
   return infoptr;
}

// -----------------------------------------------------------------------------

// define a child window for viewing comments:

class TextView : public wxTextCtrl
{
public:
   TextView(wxWindow *parent, wxWindowID id, const wxString &value,
            const wxPoint& pos, const wxSize& size, long style)
      : wxTextCtrl(parent, id, value, pos, size, style) { }

private:
   // any class wishing to process wxWidgets events must use this macro
   DECLARE_EVENT_TABLE()

   void OnKeyDown(wxKeyEvent& event);
   void OnSetFocus(wxFocusEvent& event);
};

// -----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(InfoFrame, wxFrame)
   EVT_BUTTON     (wxID_CLOSE,   InfoFrame::OnCloseButton)
   EVT_CLOSE      (              InfoFrame::OnClose)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(TextView, wxTextCtrl)
   EVT_KEY_DOWN   (TextView::OnKeyDown)
   EVT_SET_FOCUS  (TextView::OnSetFocus)
END_EVENT_TABLE()

void TextView::OnKeyDown(wxKeyEvent& event) {
   int key = event.GetKeyCode();
   if ( event.CmdDown() || event.AltDown() ) {
      // let default handler see things like cmd-C 
      event.Skip();
   } else {
      // let escape/return/enter key close info window
      if ( key == WXK_ESCAPE || key == WXK_RETURN || key == WXK_NUMPAD_ENTER ) {
         infoptr->Close(true);
      } else {
         event.Skip();
      }
   }
}

void TextView::OnSetFocus(wxFocusEvent& WXUNUSED(event)) {
   #ifdef __WXMAC__
      // wxMac prob: remove focus ring around read-only textctrl???!!!
   #endif
}

// create the pattern info window
InfoFrame::InfoFrame(char *comments)
   : wxFrame(NULL, wxID_ANY, _("Pattern Info"),
             wxPoint(infox,infoy), wxSize(infowd,infoht))
{
   wxGetApp().SetFrameIcon(this);

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
      // expand sizer now to avoid flicker
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

void ShowInfo(const char *filepath) {
   if (infoptr) {
      // info window exists so just bring it to front
      infoptr->Raise();
      #ifdef __WXX11__
         infoptr->SetFocus();    // activate window
      #endif
      return;
   }

   // create a 128K buffer for receiving comment data (big enough
   // for the comments in Dean Hickerson's stamp collection)
   char *commptr;
   const int maxcommsize = 128 * 1024;
   commptr = (char *)malloc(maxcommsize);
   if (commptr == NULL) {
      Warning("Not enough memory for comments!");
      return;
   }

   // read and display comments in current pattern file
   const char *err;
   err = readcomments(filepath, commptr, maxcommsize);
   if (err) {
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
