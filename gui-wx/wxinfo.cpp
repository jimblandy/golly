// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
    #include "wx/wx.h"     // for all others include the necessary headers
#endif

#include "readpattern.h"   // for readcomments

#include "wxgolly.h"       // for wxGetApp, mainptr
#include "wxmain.h"        // for mainptr->...
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
    // event handlers
    void OnActivate(wxActivateEvent& event);
    void OnCloseButton(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);
    
    // any class wishing to process wxWidgets events must use this macro
    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(InfoFrame, wxFrame)
EVT_ACTIVATE   (              InfoFrame::OnActivate)
EVT_BUTTON     (wxID_CLOSE,   InfoFrame::OnCloseButton)
EVT_CLOSE      (              InfoFrame::OnClose)
END_EVENT_TABLE()

// -----------------------------------------------------------------------------

InfoFrame *infoptr = NULL;    // pattern info window

wxFrame* GetInfoFrame()
{
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
    // event handlers
    void OnKeyDown(wxKeyEvent& event);
    
    // any class wishing to process wxWidgets events must use this macro
    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(TextView, wxTextCtrl)
EVT_KEY_DOWN   (TextView::OnKeyDown)
END_EVENT_TABLE()

// -----------------------------------------------------------------------------

void TextView::OnKeyDown(wxKeyEvent& event)
{
    int key = event.GetKeyCode();
#ifdef __WXMAC__
    // let cmd-W close info window
    if (event.CmdDown() && key == 'W') {
        infoptr->Close(true);
        return;
    }
    // let cmd-A select all text
    if (event.CmdDown() && key == 'A') {
        SetSelection(-1, -1);
        return;
    }
#endif
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

// -----------------------------------------------------------------------------

// create the pattern info window
InfoFrame::InfoFrame(char *comments)
: wxFrame(NULL, wxID_ANY, _("Pattern Info"),
          wxPoint(infox,infoy), wxSize(infowd,infoht))
{
    wxGetApp().SetFrameIcon(this);
    
#ifdef __WXMSW__
    // use current theme's background colour
    SetBackgroundColour(wxNullColour);
#endif
    
    TextView* textctrl = new TextView(this, wxID_ANY, wxEmptyString,
                                      wxDefaultPosition, wxDefaultSize,
                                      wxTE_RICH | // needed for font changing on Windows
                                      wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
    
    // use a fixed-width font
    wxTextAttr textattr(wxNullColour, wxNullColour,
#if defined(__WXOSX_COCOA__)
        // we need to specify facename to get Monaco instead of Courier
        wxFont(12, wxMODERN, wxNORMAL, wxNORMAL, false, wxT("Monaco")));
#else
        wxFont(10, wxMODERN, wxNORMAL, wxNORMAL));
#endif
    textctrl->SetDefaultStyle(textattr);
    
    if (comments[0] == 0) {
        textctrl->WriteText(_("No comments found."));
    } else {
        textctrl->WriteText(wxString(comments,wxConvLocal));
#if defined(__WXOSX_COCOA__)
        // sigh... wxOSX seems to ignore SetDefaultStyle
        textctrl->SetStyle(0, strlen(comments), textattr);
#endif
    }

    textctrl->ShowPosition(0);
    textctrl->SetInsertionPoint(0);
    
    wxButton *closebutt = new wxButton(this, wxID_CLOSE, _("Close"));
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
    
    // need this on Linux
    textctrl->SetFocus();
}

// -----------------------------------------------------------------------------

void InfoFrame::OnActivate(wxActivateEvent& event)
{
    if ( event.GetActive() ) {
        // ensure correct menu items, esp after info window
        // is clicked while app is in background
        mainptr->UpdateMenuItems();
    }
    event.Skip();
}

// -----------------------------------------------------------------------------

void InfoFrame::OnCloseButton(wxCommandEvent& WXUNUSED(event))
{
    Close(true);
}

// -----------------------------------------------------------------------------

void InfoFrame::OnClose(wxCloseEvent& WXUNUSED(event))
{
#ifdef __WXMSW__
    if (!IsIconized()) {
#endif
        // save current location and size for later use in SavePrefs
        wxRect r = GetRect();
        infox = r.x;
        infoy = r.y;
        infowd = r.width;
        infoht = r.height;
#ifdef __WXMSW__
    }
#endif
    
    Destroy();        // also deletes all child windows (buttons, etc)
    infoptr = NULL;
}

// -----------------------------------------------------------------------------

#ifdef __WXMAC__
    // convert path to decomposed UTF8 so fopen will work
    #define FILEPATH filepath.fn_str()
#else
    #define FILEPATH filepath.mb_str(wxConvLocal)
#endif

void ShowInfo(const wxString& filepath)
{
    if (infoptr) {
        // info window exists so just bring it to front
        infoptr->Raise();
        return;
    }
    
    // buffer for receiving comment data (allocated by readcomments)
    char *commptr = NULL;
    
    // read and display comments in current pattern file
    const char *err = readcomments(FILEPATH, &commptr);
    if (err) {
        Warning(wxString(err,wxConvLocal));
    } else {
        infoptr = new InfoFrame(commptr);
        if (infoptr == NULL) {
            Warning(_("Could not create info window!"));
        } else {
            infoptr->Show(true);
        }
    }
    
    if (commptr) free(commptr);
}
