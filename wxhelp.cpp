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

// for compilers that support precompilation
#include "wx/wxprec.h"

// for all others, include the necessary headers
#ifndef WX_PRECOMP
   #include "wx/wx.h"
#endif

#include "wx/wxhtml.h"     // for wxHtmlWindow

#include "wxgolly.h"       // for wxGetApp, mainptr
#include "wxmain.h"        // for mainptr->...
#include "wxutils.h"       // for Warning
#include "wxprefs.h"       // for helpx/y/wd/ht and minhelp*
#include "wxhelp.h"

// -----------------------------------------------------------------------------

// define a modeless help window:

class HelpFrame : public wxFrame
{
public:
   HelpFrame();

private:
   // IDs for buttons in help window (see also wxID_CLOSE)
   enum {
      ID_BACK_BUTT = wxID_HIGHEST,
      ID_FORWARD_BUTT,
      ID_CONTENTS_BUTT
   };

   // any class wishing to process wxWidgets events must use this macro
   DECLARE_EVENT_TABLE()

   // event handlers
   void OnBackButton(wxCommandEvent& event);
   void OnForwardButton(wxCommandEvent& event);
   void OnContentsButton(wxCommandEvent& event);
   void OnCloseButton(wxCommandEvent& event);
   void OnClose(wxCloseEvent& event);
};

// define a child window for displaying html info:

class HtmlView : public wxHtmlWindow
{
public:
   HtmlView(wxWindow *parent, wxWindowID id, const wxPoint& pos,
            const wxSize& size, long style)
      : wxHtmlWindow(parent, id, pos, size, style) { }
   virtual void OnLinkClicked(const wxHtmlLinkInfo& link);

private:
   // any class wishing to process wxWidgets events must use this macro
   DECLARE_EVENT_TABLE()

   #ifdef __WXMSW__
      // see HtmlView::OnKeyUp for why we do this
      void OnKeyUp(wxKeyEvent& event);
   #else
      void OnKeyDown(wxKeyEvent& event);
   #endif
};

HelpFrame *helpptr = NULL;    // help window
HtmlView *htmlwin = NULL;     // html child window

wxFrame* GetHelpFrame() {
   return helpptr;
}

wxWindow* GetHtmlWindow() {
   return htmlwin;
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
#ifdef __WXMSW__
   // see HtmlView::OnKeyUp for why we do this
   EVT_KEY_UP     (HtmlView::OnKeyUp)
#else
   EVT_KEY_DOWN   (HtmlView::OnKeyDown)
#endif
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
   wxGetApp().SetFrameIcon(this);

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

   backbutt = new wxButton(this, ID_BACK_BUTT, "<",
                           wxDefaultPosition, wxSize(40,wxDefaultCoord));
   hbox->Add(backbutt, 0, wxALL | wxALIGN_LEFT, 10);

   forwbutt = new wxButton(this, ID_FORWARD_BUTT, ">",
                           wxDefaultPosition, wxSize(40,wxDefaultCoord));
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
      // set currhelp so user can close help window and then open same page
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
   htmlwin->SetFocus();          // for keyboard shortcuts
}

void ShowHelp(const char *filepath) {
   // display given html file in help window
   if (helpptr) {
      // help window exists so bring it to front and display given file
      if (filepath[0] != 0) {
         htmlwin->LoadPage(filepath);
         UpdateHelpButtons();
      }
      helpptr->Raise();
      #ifdef __WXX11__
         helpptr->SetFocus();    // activate window
         htmlwin->SetFocus();    // for keyboard shortcuts
      #endif
   } else {
      helpptr = new HelpFrame();
      if (helpptr == NULL) {
         Warning("Could not create help window!");
         return;
      }
      // assume our .html files contain a <title> tag
      htmlwin->SetRelatedFrame(helpptr, _("%s"));
      
      if (filepath[0] != 0) {
         htmlwin->LoadPage(filepath);
      } else {
         htmlwin->LoadPage(currhelp);
      }

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

void AddEOL(wxString &str) {
   // append eol char(s) to given string
   #ifdef __WXMAC__
      str += '\r';
   #elif defined(__WXMSW__)
      str += '\r';
      str += '\n';
   #else // assume Unix
      str += '\n';
   #endif
}

void LoadLexiconPattern(const wxHtmlCell *htmlcell) {
   if (mainptr->generating) {
      Warning("Another pattern is currently generating.");
      return;
   }
   if (htmlcell) {
      wxHtmlContainerCell *parent = htmlcell->GetParent();
      if (parent) {
         parent = parent->GetParent();
         if (parent) {
            wxHtmlCell *container = parent->GetFirstChild();
            wxString textpict;
            while (container) {
               wxHtmlCell *cell = container->GetFirstChild();
               while (cell) {
                  wxString celltext = cell->ConvertToText(NULL);
                  if (celltext.IsEmpty()) {
                     // probably a formatting cell
                  } else {
                     textpict += celltext;
                     AddEOL(textpict);
                  }
                  cell = cell->GetNext();
               }
               container = container->GetNext();
            }
            if (!textpict.IsEmpty() && mainptr->CopyTextToClipboard(textpict)) {
               mainptr->Raise();
               #ifdef __WXX11__
                  mainptr->SetFocus();    // activate window
               #endif
               // need to process pending events to update window
               // and to update clipboard on Windows
               while (wxGetApp().Pending()) wxGetApp().Dispatch();
               // may need to call wxMilliSleep or Yield here
               // to avoid occasional clipboard error on Windows???
               mainptr->OpenClipboard();
            }
         }
      }
   }
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
   } else if ( url.StartsWith("lexpatt:") ) {
      // user clicked on pattern in Life Lexicon
      LoadLexiconPattern( link.GetHtmlCell() );
   } else {
      // assume it's a link to a local target or another help file
      LoadPage(url);
      if ( helpptr && helpptr->IsActive() ) {
         UpdateHelpButtons();
      }
   }
}

#ifdef __WXMSW__
// we have to use OnKeyUp handler on Windows otherwise wxHtmlWindow's OnKeyUp
// gets called which detects ctrl-C and clobbers our clipboard fix
void HtmlView::OnKeyUp(wxKeyEvent& event)
#else
// we have to use OnKeyDown handler on Mac -- if OnKeyUp handler is used and
// cmd-C is pressed quickly then key code is 400!!!
void HtmlView::OnKeyDown(wxKeyEvent& event)
#endif
{
   int key = event.GetKeyCode();
   if ( event.CmdDown() || event.AltDown() ) {
      if ( key == 'C' ) {
         // copy any selected text to the clipboard
         wxString text = SelectionToText();
         if ( text.Length() > 0 ) {
            if ( helpptr && helpptr->IsActive() &&
                 GetOpenedPageTitle().StartsWith("Life Lexicon") ) {
               // avoid wxHTML bug when copying text inside <pre>...</pre>!!!
               // if there are at least 2 lines and the 1st line is twice
               // the size of the 2nd line then insert \n in middle of 1st line
               if ( text.Freq('\n') > 0 ) {
                  wxString line1 = text.BeforeFirst('\n');
                  wxString aftern = text.AfterFirst('\n');
                  wxString line2 = aftern.BeforeFirst('\n');
                  size_t line1len = line1.Length();
                  size_t line2len = line2.Length();
                  if ( line1len == 2 * line2len ) {
                     wxString left = text.Left(line2len);
                     wxString right = text.Mid(line2len);
                     text = left;
                     text += '\n';
                     text += right;
                  }
               }
            }
            mainptr->CopyTextToClipboard(text);
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
      // let escape/return/enter key close help window
      if ( key == WXK_ESCAPE || key == WXK_RETURN || key == WXK_NUMPAD_ENTER ) {
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
   wxDialog dlg(mainptr, wxID_ANY, wxString("About Golly"));
   
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
   topsizer->Add(okbutt, 0, wxBOTTOM | wxALIGN_CENTER, 10);
   dlg.SetSizer(topsizer);
   topsizer->Fit(&dlg);
   dlg.CenterOnParent(wxBOTH);
   dlg.ShowModal();
   // all child windows have been deleted
}
