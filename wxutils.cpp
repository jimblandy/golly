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

#include "wx/progdlg.h"    // for wxProgressDialog

#include "wxgolly.h"       // for wxGetApp, etc
#include "wxview.h"        // for viewptr->...
#include "wxmain.h"        // for mainptr->...
#include "wxutils.h"

// -----------------------------------------------------------------------------

#ifdef __WXMAC__

#include <Carbon/Carbon.h>

// do our own Mac-specific warning and fatal dialogs using StandardAlert
// so we can center dialogs on front window and also see modified app icon

bool AppInBackground() {
   ProcessSerialNumber frontPSN, currentPSN;
   bool sameProcess = false;

   GetCurrentProcess(&currentPSN);
   GetFrontProcess(&frontPSN);
   SameProcess(&currentPSN, &frontPSN, (Boolean *)&sameProcess);

   return sameProcess == false;
}

void NotifyUser() {
   NMRec myNMRec;
   if ( AppInBackground() ) {
      myNMRec.qType = nmType;
      myNMRec.nmMark = 1;
      myNMRec.nmIcon = NULL;
      myNMRec.nmSound = NULL;
      myNMRec.nmStr = NULL;
      myNMRec.nmResp = NULL;
      myNMRec.nmRefCon = 0;
      if ( NMInstall(&myNMRec) == noErr ) {
         // wait for resume event to bring us to foreground
         do {
            EventRef event;
            EventTargetRef target;
            if ( ReceiveNextEvent(0, NULL, kEventDurationNoWait, true, &event) == noErr ) {
               target = GetEventDispatcherTarget();
               SendEventToEventTarget(event, target);
               ReleaseEvent(event);
            }
            Delay(6,NULL);                // don't hog CPU
         } while ( AppInBackground() );
         NMRemove(&myNMRec);
      }
   }
}

void MacWarning(const char *title, const char *msg) {
   short itemHit;
   AlertStdAlertParamRec alertParam;
   Str255 ptitle, pmsg;

   CopyCStringToPascal(title, ptitle);
   CopyCStringToPascal(msg, pmsg);

   NotifyUser();
   alertParam.movable = true;
   alertParam.helpButton = false;
   alertParam.filterProc = NULL;
   alertParam.defaultText = NULL;
   alertParam.cancelText = NULL;
   alertParam.otherText = NULL;
   alertParam.defaultButton = kAlertStdAlertOKButton;
   alertParam.cancelButton = 0;
   alertParam.position = kWindowAlertPositionParentWindow;
   StandardAlert(kAlertCautionAlert, ptitle, pmsg, &alertParam, &itemHit);
}

void MacFatal(const char *title, const char *msg) {
   short itemHit;
   AlertStdAlertParamRec alertParam;
   Str255 ptitle, pmsg, pquit;

   CopyCStringToPascal(title, ptitle);
   CopyCStringToPascal(msg, pmsg);
   CopyCStringToPascal("Quit", pquit);

   NotifyUser();
   alertParam.movable = true;
   alertParam.helpButton = false;
   alertParam.filterProc = NULL;
   alertParam.defaultText = pquit;
   alertParam.cancelText = NULL;
   alertParam.otherText = NULL;
   alertParam.defaultButton = kAlertStdAlertOKButton;
   alertParam.cancelButton = 0;
   alertParam.position = kWindowAlertPositionParentWindow;
   StandardAlert(kAlertStopAlert, ptitle, pmsg, &alertParam, &itemHit);
}

#endif // __WXMAC__

// -----------------------------------------------------------------------------

void Warning(const char *msg) {
   wxBell();
   wxSetCursor(*wxSTANDARD_CURSOR);
   // use wxGetApp().GetAppName() and append " warning:" !!!
   #ifdef __WXMAC__
      MacWarning("Golly warning:", msg);
   #else
      wxMessageBox(_(msg), _("Golly warning:"), wxOK | wxICON_EXCLAMATION,
                   wxGetActiveWindow());  // NULL on X11, ie. centered on screen
   #endif
}

void Fatal(const char *msg) {
   wxBell();
   wxSetCursor(*wxSTANDARD_CURSOR);
   // use wxGetApp().GetAppName() and append " error:" !!!
   #ifdef __WXMAC__
      MacFatal("Golly error:", msg);
   #else
      wxMessageBox(_(msg), _("Golly error:"), wxOK | wxICON_ERROR,
                   wxGetActiveWindow());  // NULL on X11, ie. centered on screen
   #endif
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
long progstart;                           // starting time (in millisecs)
long prognext;                            // when to update progress dialog
char progtitle[128];                      // title for progress dialog

void BeginProgress(const char *dlgtitle) {
   if (progdlg) {
      // better do this in case of nested call
      delete progdlg;
      progdlg = NULL;
   }
   strncpy(progtitle, dlgtitle, sizeof(progtitle));
   progstart = wxGetElapsedTime(false);
   // let user know they'll have to wait
   wxSetCursor(*wxHOURGLASS_CURSOR);
   // do we really need to do this???!!! maybe only on Mac???
   viewptr->SetCursor(*wxHOURGLASS_CURSOR);
}

bool AbortProgress(double fraction_done, const char *newmsg) {
   long t = wxGetElapsedTime(false);
   if (progdlg) {
      if (t < prognext) return false;
      #ifdef __WXX11__
         prognext = t + 1000;    // call Update about once per sec on X11
      #else
         prognext = t + 100;     // call Update about 10 times per sec
      #endif
      // wxMac and wxX11 don't let user hit escape key to cancel dialog!!!
      // Update returns false if user hits Cancel button
      return !progdlg->Update(int((double)maxprogrange * fraction_done), newmsg);
   } else {
      // note that fraction_done is not always an accurate estimator for how long
      // the task will take, especially when we use nextcell for cut/copy
      long msecs = t - progstart;
      if ( (msecs > 1000 && fraction_done < 0.3) || msecs > 2500 ) {
         // task is probably going to take a while so create progress dialog
         progdlg = new wxProgressDialog(_T(progtitle), _T(""),
                                        maxprogrange, mainptr,
                                        wxPD_AUTO_HIDE | wxPD_APP_MODAL |
                                        wxPD_CAN_ABORT | wxPD_SMOOTH |
                                        wxPD_ESTIMATED_TIME | wxPD_REMAINING_TIME);
         #ifdef __WXMAC__
            // avoid user selecting Quit or bringing another window to front
            if (progdlg)
               BeginAppModalStateForWindow( (OpaqueWindowPtr*)progdlg->MacGetWindowRef() );
         #endif
      }
      prognext = t + 10;      // short delay until 1st Update
      return false;           // don't abort
   }
}

void EndProgress() {
   if (progdlg) {
      #ifdef __WXMAC__
         EndAppModalStateForWindow( (OpaqueWindowPtr*)progdlg->MacGetWindowRef() );
      #endif
      delete progdlg;
      progdlg = NULL;
      #ifdef __WXX11__
         // fix activate problem on X11 if user hit Cancel button
         mainptr->SetFocus();
      #endif
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
