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

#include "wx/progdlg.h"    // for wxProgressDialog

#include "wxgolly.h"       // for wxGetApp, etc
#include "wxview.h"        // for viewptr->...
#include "wxmain.h"        // for mainptr->...
#include "wxutils.h"

#ifdef __WXMAC__
#include <Carbon/Carbon.h>
#endif

// -----------------------------------------------------------------------------

void Note(const wxString &msg) {
   #ifdef __WXMAC__
      wxSetCursor(*wxSTANDARD_CURSOR);
   #endif
   wxString title = wxGetApp().GetAppName() + _(" note:");
   wxMessageBox(msg, title, wxOK | wxICON_INFORMATION, wxGetActiveWindow());
}

// -----------------------------------------------------------------------------

void Warning(const wxString &msg) {
   wxBell();
   #ifdef __WXMAC__
      wxSetCursor(*wxSTANDARD_CURSOR);
   #endif
   wxString title = wxGetApp().GetAppName() + _(" warning:");
   wxMessageBox(msg, title, wxOK | wxICON_EXCLAMATION, wxGetActiveWindow());
}

// -----------------------------------------------------------------------------

void Fatal(const wxString &msg) {
   wxBell();
   #ifdef __WXMAC__
      wxSetCursor(*wxSTANDARD_CURSOR);
   #endif
   wxString title = wxGetApp().GetAppName() + _(" error:");
   wxMessageBox(msg, title, wxOK | wxICON_ERROR, wxGetActiveWindow());
   // calling wxExit() results in a bus error on X11
   exit(1);
}

// -----------------------------------------------------------------------------

// globals for showing progress

wxProgressDialog *progdlg = NULL;         // progress dialog
#ifdef __WXX11__
   const int maxprogrange = 10000;        // maximum range must be < 32K on X11?
#else
   const int maxprogrange = 1000000000;   // maximum range (best if very large)
#endif
wxStopWatch *progwatch = NULL;            // stopwatch for progress dialog
long prognext;                            // when to update progress dialog
wxString progtitle;                       // title for progress dialog

// -----------------------------------------------------------------------------

#ifdef __WXMAC__

// define a key event handler to allow escape key to cancel progress dialog
class ProgressHandler : public wxEvtHandler
{
public:
   void OnKeyDown(wxKeyEvent& event);
   DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(ProgressHandler, wxEvtHandler)
   EVT_KEY_DOWN (ProgressHandler::OnKeyDown)
END_EVENT_TABLE()

void ProgressHandler::OnKeyDown(wxKeyEvent& event)
{
   int key = event.GetKeyCode();
   if ( key == WXK_ESCAPE || key == '.' ) {
      wxCommandEvent cancel(wxEVT_COMMAND_BUTTON_CLICKED, wxID_CANCEL);
      wxWindow* buttwin = progdlg->FindWindow(wxID_CANCEL);
      if (buttwin) {
         cancel.SetEventObject(buttwin);
         buttwin->ProcessEvent(cancel);
      }
   } else {
      event.Skip();
   }
}

#endif // __WXMAC__

// -----------------------------------------------------------------------------

void BeginProgress(const wxString &dlgtitle) {
   if (progdlg) {
      // better do this in case of nested call
      delete progdlg;
      progdlg = NULL;
   }
   if (progwatch) {
      delete progwatch;
   }
   progwatch = new wxStopWatch();
   progtitle = dlgtitle;
   // let user know they'll have to wait
   #ifdef __WXMAC__
      wxSetCursor(*wxHOURGLASS_CURSOR);
   #endif
   viewptr->SetCursor(*wxHOURGLASS_CURSOR);
}

// -----------------------------------------------------------------------------

bool AbortProgress(double fraction_done, const wxString &newmsg) {
   long msecs = progwatch->Time();
   if (progdlg) {
      if (msecs < prognext) return false;
      #ifdef __WXX11__
         prognext = msecs + 1000;    // call Update about once per sec on X11
      #else
         prognext = msecs + 100;     // call Update about 10 times per sec
      #endif
      // Update returns false if user hits Cancel button
      return !progdlg->Update(int((double)maxprogrange * fraction_done), newmsg);
   } else {
      // note that fraction_done is not always an accurate estimator for how long
      // the task will take, especially when we use nextcell for cut/copy
      if ( (msecs > 1000 && fraction_done < 0.3) || msecs > 2500 ) {
         // task is probably going to take a while so create progress dialog
         progdlg = new wxProgressDialog(progtitle, wxEmptyString,
                                        maxprogrange, mainptr,
                                        wxPD_AUTO_HIDE | wxPD_APP_MODAL |
                                        wxPD_CAN_ABORT | wxPD_SMOOTH |
                                        wxPD_ESTIMATED_TIME | wxPD_REMAINING_TIME);
         #ifdef __WXMAC__
            if (progdlg) {
               // avoid user selecting Quit or bringing another window to front
               BeginAppModalStateForWindow( (OpaqueWindowPtr*)progdlg->MacGetWindowRef() );
               // install key event handler
               progdlg->PushEventHandler(new ProgressHandler());
            }
         #endif
      }
      prognext = msecs + 10;     // short delay until 1st Update
      return false;              // don't abort
   }
}

// -----------------------------------------------------------------------------

void EndProgress() {
   if (progdlg) {
      #ifdef __WXMAC__
         EndAppModalStateForWindow( (OpaqueWindowPtr*)progdlg->MacGetWindowRef() );
         // remove and delete ProgressHandler
         progdlg->PopEventHandler(true);
      #endif
      delete progdlg;
      progdlg = NULL;
      #ifdef __WXX11__
         // fix activate problem on X11 if user hit Cancel button
         mainptr->SetFocus();
      #endif
   }
   if (progwatch) {
      delete progwatch;
      progwatch = NULL;
   }
   // BeginProgress changed cursor so reset it
   viewptr->CheckCursor(mainptr->IsActive());
}

// -----------------------------------------------------------------------------

void FillRect(wxDC &dc, wxRect &rect, wxBrush &brush) {
   // set pen transparent so brush fills rect
   dc.SetPen(*wxTRANSPARENT_PEN);
   dc.SetBrush(brush);
   
   dc.DrawRectangle(rect);
   
   dc.SetBrush(wxNullBrush);     // restore brush
   dc.SetPen(wxNullPen);         // restore pen
}
