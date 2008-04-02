                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2007 Andrew Trevorrow and Tomas Rokicki.

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

#include "wx/wxhtml.h"     // for wxHtmlWindow
#include "wx/file.h"       // for wxFile

#include "wxgolly.h"       // for wxGetApp, mainptr
#include "wxmain.h"        // for mainptr->...
#include "wxutils.h"       // for Warning
#include "wxprefs.h"       // for helpx/y/wd/ht, helpfontsize, gollydir, etc
#include "wxscript.h"      // for inscript
#include "wxlayer.h"       // for numlayers, GetLayer, etc
#include "wxhelp.h"

// -----------------------------------------------------------------------------

// define a modeless help window:

class HelpFrame : public wxFrame
{
public:
   HelpFrame();

private:
   // ids for buttons in help window (see also wxID_CLOSE)
   enum {
      ID_BACK_BUTT = wxID_HIGHEST + 1,
      ID_FORWARD_BUTT,
      ID_CONTENTS_BUTT
   };

   // event handlers
   void OnActivate(wxActivateEvent& event);
   void OnBackButton(wxCommandEvent& event);
   void OnForwardButton(wxCommandEvent& event);
   void OnContentsButton(wxCommandEvent& event);
   void OnCloseButton(wxCommandEvent& event);
   void OnClose(wxCloseEvent& event);

   // any class wishing to process wxWidgets events must use this macro
   DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(HelpFrame, wxFrame)
   EVT_ACTIVATE   (                    HelpFrame::OnActivate)
   EVT_BUTTON     (ID_BACK_BUTT,       HelpFrame::OnBackButton)
   EVT_BUTTON     (ID_FORWARD_BUTT,    HelpFrame::OnForwardButton)
   EVT_BUTTON     (ID_CONTENTS_BUTT,   HelpFrame::OnContentsButton)
   EVT_BUTTON     (wxID_CLOSE,         HelpFrame::OnCloseButton)
   EVT_CLOSE      (                    HelpFrame::OnClose)
END_EVENT_TABLE()

// -----------------------------------------------------------------------------

// define a child window for displaying html info:

class HtmlView : public wxHtmlWindow
{
public:
   HtmlView(wxWindow* parent, wxWindowID id, const wxPoint& pos,
            const wxSize& size, long style)
      : wxHtmlWindow(parent, id, pos, size, style) { }

   virtual void OnLinkClicked(const wxHtmlLinkInfo& link);

   void CheckAndLoad(const wxString& filepath);

   void StartTimer() {
      htmltimer = new wxTimer(this, wxID_ANY);
      // call OnTimer 10 times per sec
      htmltimer->Start(100, wxTIMER_CONTINUOUS);
   }

   void StopTimer() {
      htmltimer->Stop();
      delete htmltimer;
   }

private:
   #ifdef __WXMSW__
      // see HtmlView::OnKeyUp for why we do this
      void OnKeyUp(wxKeyEvent& event);
   #else
      void OnKeyDown(wxKeyEvent& event);
   #endif
   
   void OnChar(wxKeyEvent& event);
   void OnSize(wxSizeEvent& event);
   void OnTimer(wxTimerEvent& event);

   wxTimer* htmltimer;

   // any class wishing to process wxWidgets events must use this macro
   DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(HtmlView, wxHtmlWindow)
#ifdef __WXMSW__
   // see HtmlView::OnKeyUp for why we do this
   EVT_KEY_UP     (HtmlView::OnKeyUp)
#else
   EVT_KEY_DOWN   (HtmlView::OnKeyDown)
#endif
   EVT_CHAR       (HtmlView::OnChar)
   EVT_SIZE       (HtmlView::OnSize)
   EVT_TIMER      (wxID_ANY, HtmlView::OnTimer)
END_EVENT_TABLE()

// -----------------------------------------------------------------------------

HelpFrame* helpptr = NULL;    // help window
HtmlView* htmlwin = NULL;     // html child window

wxButton* backbutt;           // back button
wxButton* forwbutt;           // forwards button
wxButton* contbutt;           // Contents button

long whenactive;              // when help window became active (elapsed millisecs)

const wxString helphome = _("Help/index.html");    // contents page
wxString currhelp = helphome;                      // current help file
const wxString lexicon_name = _("lexicon");        // name of lexicon layer

int lexlayer;                 // index of existing lexicon layer (-ve if not present)
wxString lexpattern;          // lexicon pattern data

// -----------------------------------------------------------------------------

wxFrame* GetHelpFrame() {
   return helpptr;
}

// -----------------------------------------------------------------------------

void SetFontSizes(int size)
{
   // set font sizes for <FONT SIZE=-2> to <FONT SIZE=+4>
   int f_sizes[7];
   f_sizes[0] = int(size * 0.6);
   f_sizes[1] = int(size * 0.8);
   f_sizes[2] = size;
   f_sizes[3] = int(size * 1.2);
   f_sizes[4] = int(size * 1.4);
   f_sizes[5] = int(size * 1.6);
   f_sizes[6] = int(size * 1.8);
   htmlwin->SetFonts(wxEmptyString, wxEmptyString, f_sizes);
}

// -----------------------------------------------------------------------------

void ChangeFontSizes(int size)
{
   // changing font sizes resets pos to top, so save and restore pos
   int x, y;
   htmlwin->GetViewStart(&x, &y);
   SetFontSizes(size);
   if (y > 0) htmlwin->Scroll(-1, y);
}

// -----------------------------------------------------------------------------

// create the help window
HelpFrame::HelpFrame()
   : wxFrame(NULL, wxID_ANY, _(""), wxPoint(helpx,helpy), wxSize(helpwd,helpht))
{
   wxGetApp().SetFrameIcon(this);

   #ifdef __WXMSW__
      // use current theme's background colour
      SetBackgroundColour(wxNullColour);
   #endif

   htmlwin = new HtmlView(this, wxID_ANY,
                          // specify small size to avoid clipping scroll bar on resize
                          wxDefaultPosition, wxSize(30,30),
                          wxHW_DEFAULT_STYLE | wxSUNKEN_BORDER);
   htmlwin->StartTimer();
   htmlwin->SetBorders(4);
   SetFontSizes(helpfontsize);

   wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);
   wxBoxSizer* hbox = new wxBoxSizer(wxHORIZONTAL);

   backbutt = new wxButton(this, ID_BACK_BUTT, _("<"),
                           wxDefaultPosition, wxSize(40,wxDefaultCoord));
   hbox->Add(backbutt, 0, wxALL | wxALIGN_LEFT, 10);

   forwbutt = new wxButton(this, ID_FORWARD_BUTT, _(">"),
                           wxDefaultPosition, wxSize(40,wxDefaultCoord));
   hbox->Add(forwbutt, 0, wxTOP | wxBOTTOM | wxALIGN_LEFT, 10);

   contbutt = new wxButton(this, ID_CONTENTS_BUTT, _("Contents"));
   hbox->Add(contbutt, 0, wxALL | wxALIGN_LEFT, 10);

   hbox->AddStretchSpacer(1);

   wxButton* closebutt = new wxButton(this, wxID_CLOSE, _("Close"));
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

// -----------------------------------------------------------------------------

void UpdateHelpButtons()
{
   backbutt->Enable( htmlwin->HistoryCanBack() );
   forwbutt->Enable( htmlwin->HistoryCanForward() );
   contbutt->Enable( !htmlwin->GetOpenedPageTitle().Contains(_("Contents")) );
   
   wxString location = htmlwin->GetOpenedPage();
   if ( !location.IsEmpty() ) {
      // set currhelp so user can close help window and then open same page
      currhelp = location;
   }
   
   htmlwin->SetFocus();    // for keyboard shortcuts
}

// -----------------------------------------------------------------------------

void ShowHelp(const wxString& filepath)
{
   // display given html file in help window
   if (helpptr) {
      // help window exists so bring it to front and display given file
      if (!filepath.IsEmpty()) {
         htmlwin->CheckAndLoad(filepath);
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
         Warning(_("Could not create help window!"));
         return;
      }
      // assume our .html files contain a <title> tag
      htmlwin->SetRelatedFrame(helpptr, _("%s"));
      
      if (!filepath.IsEmpty()) {
         htmlwin->CheckAndLoad(filepath);
      } else {
         htmlwin->CheckAndLoad(currhelp);
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
   whenactive = 0;
}

// -----------------------------------------------------------------------------

void HelpFrame::OnActivate(wxActivateEvent& event)
{
   if ( event.GetActive() ) {
      // help window is being activated
      whenactive = stopwatch->Time();
      
      // ensure correct menu items, esp after help window
      // is clicked while app is in background
      mainptr->UpdateMenuItems(false);
   }
   event.Skip();
}

// -----------------------------------------------------------------------------

void HelpFrame::OnBackButton(wxCommandEvent& WXUNUSED(event))
{
   if ( htmlwin->HistoryBack() ) {
      UpdateHelpButtons();
   } else {
      wxBell();
   }
}

// -----------------------------------------------------------------------------

void HelpFrame::OnForwardButton(wxCommandEvent& WXUNUSED(event))
{
   if ( htmlwin->HistoryForward() ) {
      UpdateHelpButtons();
   } else {
      wxBell();
   }
}

// -----------------------------------------------------------------------------

void HelpFrame::OnContentsButton(wxCommandEvent& WXUNUSED(event))
{
   ShowHelp(helphome);
}

// -----------------------------------------------------------------------------

void HelpFrame::OnCloseButton(wxCommandEvent& WXUNUSED(event))
{
   Close(true);
}

// -----------------------------------------------------------------------------

void HelpFrame::OnClose(wxCloseEvent& WXUNUSED(event))
{
   #ifdef __WXMSW__
   if (!IsIconized()) {
   #endif
      // save current location and size for later use in SavePrefs
      wxRect r = GetRect();
      helpx = r.x;
      helpy = r.y;
      helpwd = r.width;
      helpht = r.height;
   #ifdef __WXMSW__
   }
   #endif
   
   // stop htmltimer immediately (if we do it in ~HtmlView dtor then timer
   // only stops when app becomes idle)
   htmlwin->StopTimer();
   
   Destroy();        // also deletes all child windows (buttons, etc)
   helpptr = NULL;
}

// -----------------------------------------------------------------------------

void ClickLexiconPattern(const wxHtmlCell* htmlcell)
{
   if (inscript) {
      Warning(_("Cannot load lexicon pattern while a script is running."));
      return;
   }
   
   if (htmlcell) {
      wxHtmlContainerCell* parent = htmlcell->GetParent();
      if (parent) {
         parent = parent->GetParent();
         if (parent) {
            wxHtmlCell* container = parent->GetFirstChild();
            
            // extract pattern data and store in lexpattern
            lexpattern.Clear();
            while (container) {
               wxHtmlCell* cell = container->GetFirstChild();
               while (cell) {
                  wxString celltext = cell->ConvertToText(NULL);
                  if (celltext.IsEmpty()) {
                     // probably a formatting cell
                  } else {
                     lexpattern += celltext;
                     // append eol char(s)
                     #ifdef __WXMAC__
                        lexpattern += '\r';
                     #elif defined(__WXMSW__)
                        lexpattern += '\r';
                        lexpattern += '\n';
                     #else // assume Unix
                        lexpattern += '\n';
                     #endif
                  }
                  cell = cell->GetNext();
               }
               container = container->GetNext();
            }
            
            if (!lexpattern.IsEmpty()) {
               mainptr->Raise();
               #ifdef __WXX11__
                  mainptr->SetFocus();    // activate window
               #endif
               
               // look for existing lexicon layer
               lexlayer = -1;
               for (int i = 0; i < numlayers; i++) {
                  if (GetLayer(i)->currname == lexicon_name) {
                     lexlayer = i;
                     break;
                  }
               }
               if (lexlayer < 0 && numlayers == MAX_LAYERS) {
                  Warning(_("Cannot create new layer for lexicon pattern."));
                  return;
               }
               
               if (mainptr->generating) {
                  // terminate generating loop and set command_pending flag
                  mainptr->Stop();
                  mainptr->command_pending = true;
                  mainptr->cmdevent.SetId(ID_LOAD_LEXICON);
                  return;
               }
               
               LoadLexiconPattern();
            }
         }
      }
   }
}

// -----------------------------------------------------------------------------

void LoadLexiconPattern()
{
   // switch to existing lexicon layer or create a new such layer
   if (lexlayer >= 0) {
      SetLayer(lexlayer);
   } else {
      AddLayer();
      mainptr->SetWindowTitle(lexicon_name);
   }
   
   // copy lexpattern data to tempstart file so we can handle
   // all formats supported by readpattern
   wxFile outfile(currlayer->tempstart, wxFile::write);
   if ( outfile.IsOpened() ) {
      outfile.Write(lexpattern);
      outfile.Close();
      currlayer->currfile = currlayer->tempstart;
      // load lexicon pattern into current layer
      mainptr->LoadPattern(currlayer->currfile, lexicon_name);
   } else {
      Warning(_("Could not create tempstart file!"));
   }
}

// -----------------------------------------------------------------------------

void HtmlView::OnLinkClicked(const wxHtmlLinkInfo& link)
{
   #ifdef __WXMAC__
      if ( stopwatch->Time() - whenactive < 500 ) {
         // avoid problem on Mac:
         // ignore click in link if the help window was in the background;
         // this isn't fail safe because OnLinkClicked is only called AFTER
         // the mouse button is released (better soln would be to set an
         // ignoreclick flag in OnMouseDown handler if click occurred very
         // soon after activate)
         return;
      }
   #endif
   wxString url = link.GetHref();
   if ( url.StartsWith(wxT("http:")) || url.StartsWith(wxT("mailto:")) ) {
      // pass http/mailto URL to user's preferred browser/emailer
      #ifdef __WXMAC__
         // wxLaunchDefaultBrowser doesn't work on Mac with IE (get msg in console.log)
         // but it's easier just to use the Mac OS X open command
         if ( wxExecute(wxT("open ") + url, wxEXEC_ASYNC) == -1 )
            Warning(_("Could not open URL!"));
      #elif defined(__WXGTK__)
         // wxLaunchDefaultBrowser is not reliable on Linux/GTK so we call gnome-open;
         // unfortunately it does not bring browser to front if it's already running!!!
         if ( wxExecute(wxT("gnome-open ") + url, wxEXEC_ASYNC) == -1 )
            Warning(_("Could not open URL!"));
      #else
         if ( !wxLaunchDefaultBrowser(url) )
            Warning(_("Could not launch browser!"));
      #endif
   } else if ( url.StartsWith(wxT("lexpatt:")) ) {
      // user clicked on pattern in Life Lexicon
      ClickLexiconPattern( link.GetHtmlCell() );
   } else {
      // assume it's a link to a local target or another help file
      CheckAndLoad(url);
      if ( helpptr && helpptr->IsActive() ) {
         UpdateHelpButtons();
      }
   }
}

// -----------------------------------------------------------------------------

void HtmlView::CheckAndLoad(const wxString& filepath)
{
   if (filepath == SHOW_KEYBOARD_SHORTCUTS) {
      // build HTML string to display current keyboard shortcuts
      wxString contents =
      wxT("<html><title>Golly Help: Keyboard Shortcuts</title>")
      wxT("<body bgcolor=\"#FFFFCE\">")
      wxT("<p><font size=+1><b>Keyboard shortcuts</b></font>")
      wxT("<p>Use Preferences > Keyboard to change the following keyboard shortcuts.")
      wxT("<p><center>")
      wxT("<table cellspacing=1 border=2 cols=2 width=\"90%\">")
      wxT("<tr><td align=center>Key Combination</td><td align=center>Action</td></tr>");
      contents += GetShortcutTable();
      contents += wxT("</table></center></body></html>");
      SetPage(contents);
      currhelp = SHOW_KEYBOARD_SHORTCUTS;

   } else if ( filepath.StartsWith(_("Help/")) ) {
      // prepend location of Golly so user can open help while running a script
      wxString fullpath = gollydir + filepath;
      LoadPage(fullpath);

   } else {
      // assume full path or local link
      LoadPage(filepath);
   }
}

// -----------------------------------------------------------------------------

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
   #ifdef __WXMAC__
      // let cmd-W close help window or about box
      if (event.CmdDown() && key == 'W') {
         GetParent()->Close(true);
         return;
      }
   #endif
   if ( event.CmdDown() || event.AltDown() ) {
      if ( key == 'C' ) {
         // copy any selected text to the clipboard
         wxString text = SelectionToText();
         if ( text.Length() > 0 ) {
            if ( helpptr && helpptr->IsActive() &&
                 GetOpenedPageTitle().StartsWith(wxT("Life Lexicon")) ) {
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
         if ( key == WXK_NUMPAD_ENTER ) {
            // fix wxMac problem: allow enter key to close about box
            GetParent()->Close(true);
            return;
         }
         event.Skip();
         return;
      }
      // let escape/return/enter key close help window
      if ( key == WXK_ESCAPE || key == WXK_RETURN || key == WXK_NUMPAD_ENTER ) {
         helpptr->Close(true);
      } else if ( key == WXK_HOME ) {
         ShowHelp(helphome);
      } else {
         event.Skip();
      }
   }
}

// -----------------------------------------------------------------------------

void HtmlView::OnChar(wxKeyEvent& event)
{
   // this handler is also called from ShowAboutBox
   if ( helpptr == NULL || !helpptr->IsActive() ) {
      event.Skip();
      return;
   }
   int key = event.GetKeyCode();
   if ( key == '+' || key == '=' || key == WXK_ADD ) {
      if ( helpfontsize < maxfontsize ) {
         helpfontsize++;
         ChangeFontSizes(helpfontsize);
      }
   } else if ( key == '-' || key == WXK_SUBTRACT ) {
      if ( helpfontsize > minfontsize ) {
         helpfontsize--;
         ChangeFontSizes(helpfontsize);
      }
   } else if ( key == '[' || key == WXK_LEFT ) {
      if ( HistoryBack() ) {
         UpdateHelpButtons();
      }
   } else if ( key == ']' || key == WXK_RIGHT ) {
      if ( HistoryForward() ) {
         UpdateHelpButtons();
      }
   } else {
      event.Skip();     // so up/down arrows and pg up/down keys work
   }
}

// -----------------------------------------------------------------------------

// avoid scroll position being reset to top when wxHtmlWindow is resized
void HtmlView::OnSize(wxSizeEvent& event)
{
   int x, y;
   GetViewStart(&x, &y);         // save current position

   wxHtmlWindow::OnSize(event);

   wxString currpage = GetOpenedPage();
   if ( !currpage.IsEmpty() ) {
      CheckAndLoad(currpage);    // reload page
      Scroll(x, y);              // scroll to old position
   }
   
   // prevent wxHtmlWindow::OnSize being called again
   event.Skip(false);
}

// -----------------------------------------------------------------------------

void HtmlView::OnTimer(wxTimerEvent& WXUNUSED(event))
{
   #ifdef __WXX11__
      // no need to do anything
   #else
      if (helpptr && helpptr->IsActive()) {
         // send idle event to html window so cursor gets updated
         // even while app is busy doing something else (eg. generating)
         wxIdleEvent idleevent;
         wxGetApp().SendIdleEvents(this, idleevent);
      }
   #endif
}

// -----------------------------------------------------------------------------

void ShowAboutBox()
{
   const wxString title = _("About Golly");
   wxDialog dlg(mainptr, wxID_ANY, title);
   
   HtmlView* html = new HtmlView(&dlg, wxID_ANY, wxDefaultPosition,
                                 #if defined(__WXX11__) || defined(__WXGTK__)
                                    wxSize(460, 220),
                                 #else
                                    wxSize(386, 220),
                                 #endif
                                 wxHW_SCROLLBAR_NEVER | wxSUNKEN_BORDER);
   html->SetBorders(0);
   html->CheckAndLoad(_("Help/about.html"));
   html->SetSize(html->GetInternalRepresentation()->GetWidth(),
                 html->GetInternalRepresentation()->GetHeight());
   
   wxBoxSizer* topsizer = new wxBoxSizer(wxVERTICAL);
   topsizer->Add(html, 1, wxALL, 10);

   wxButton* okbutt = new wxButton(&dlg, wxID_OK, _("OK"));
   okbutt->SetDefault();
   topsizer->Add(okbutt, 0, wxBOTTOM | wxALIGN_CENTER, 10);
   
   dlg.SetSizer(topsizer);
   topsizer->Fit(&dlg);
   dlg.Centre();   
   dlg.ShowModal();
}
