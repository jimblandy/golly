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

#include "wx/progdlg.h"    // for wxProgressDialog
#include "wx/image.h"      // for wxImage

#include "wxgolly.h"       // for wxGetApp, etc
#include "wxview.h"        // for viewptr->...
#include "wxmain.h"        // for mainptr->...
#include "wxutils.h"

#ifdef __WXMAC__
   #include <Carbon/Carbon.h>
   #include "wx/mac/corefoundation/cfstring.h"     // for wxMacCFStringHolder
#endif

// -----------------------------------------------------------------------------

void Note(const wxString& msg)
{
   wxString title = wxGetApp().GetAppName() + _(" note:");
   wxMessageBox(msg, title, wxOK | wxICON_INFORMATION, wxGetActiveWindow());
}

// -----------------------------------------------------------------------------

void Warning(const wxString& msg)
{
   wxBell();
   wxString title = wxGetApp().GetAppName() + _(" warning:");
   wxMessageBox(msg, title, wxOK | wxICON_EXCLAMATION, wxGetActiveWindow());
}

// -----------------------------------------------------------------------------

void Fatal(const wxString& msg)
{
   wxBell();
   wxString title = wxGetApp().GetAppName() + _(" error:");
   wxMessageBox(msg, title, wxOK | wxICON_ERROR, wxGetActiveWindow());
   // calling wxExit() results in a bus error on X11
   exit(1);
}

// -----------------------------------------------------------------------------

#ifdef __WXMAC__

// this filter is used to detect cmd-D in SaveChanges dialog

Boolean SaveChangesFilter(DialogRef dlg, EventRecord* event, DialogItemIndex* item)
{
   if (event->what == keyDown && (event->modifiers & cmdKey) && 
         toupper(event->message & charCodeMask) == 'D') {

      // temporarily highlight the Don't Save button
      ControlRef ctrl;
      Rect box;
      GetDialogItemAsControl(dlg, kAlertStdAlertOtherButton, &ctrl);
      HiliteControl(ctrl, kControlButtonPart);
      GetControlBounds(ctrl, &box);
      InvalWindowRect(GetDialogWindow(dlg), &box);
      HIWindowFlush(GetDialogWindow(dlg));
      Delay(6, NULL);
      HiliteControl(ctrl, 0);

      *item = kAlertStdAlertOtherButton;
      return true;
   }
   return StdFilterProc(dlg, event, item);
}

#endif

// -----------------------------------------------------------------------------

int SaveChanges(const wxString& query, const wxString& msg)
{
#ifdef __WXMAC__
   // need a more standard dialog on Mac; ie. Save/Don't Save buttons
   // instead of Yes/No, and cmd-D is a shortcut for Don't Save

   short result;
   AlertStdCFStringAlertParamRec param;

   wxMacCFStringHolder cfSave(_("Save"), wxFONTENCODING_DEFAULT);
   wxMacCFStringHolder cfDontSave(_("Don't Save"), wxFONTENCODING_DEFAULT);
   
   wxMacCFStringHolder cfTitle(query, wxFONTENCODING_DEFAULT);
   wxMacCFStringHolder cfText(msg, wxFONTENCODING_DEFAULT);

   param.version =         kStdCFStringAlertVersionOne;
   param.position =        kWindowAlertPositionParentWindow;
   param.movable =         true;
   param.flags =           0;
   param.defaultText =     cfSave;
   param.cancelText =      (CFStringRef) kAlertDefaultCancelText;
   param.otherText =       cfDontSave;
   param.helpButton =      false;
   param.defaultButton =   kAlertStdAlertOKButton;
   param.cancelButton =    kAlertStdAlertCancelButton;

   ModalFilterUPP filterProc = NewModalFilterUPP(SaveChangesFilter);
   
   DialogRef alertRef;
   CreateStandardAlert(kAlertNoteAlert, cfTitle, cfText, &param, &alertRef);
   RunStandardAlert(alertRef, filterProc, &result);
   
   DisposeModalFilterUPP(filterProc);

   switch (result) {
      case 1:  return 2;    // Save
      case 2:  return 0;    // Cancel
      case 3:  return 1;    // Don't Save
      default: return 0;
   }
#else
   int answer = wxMessageBox(msg, query,
                             wxICON_QUESTION | wxYES_NO | wxCANCEL,
                             wxGetActiveWindow());
   switch (answer) {
      case wxYES: return 2;
      case wxNO:  return 1;
      default:    return 0;   // answer == wxCANCEL
   }
#endif
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

void BeginProgress(const wxString& dlgtitle)
{
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

bool AbortProgress(double fraction_done, const wxString& newmsg)
{
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

void EndProgress()
{
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

void FillRect(wxDC& dc, wxRect& rect, wxBrush& brush)
{
   // set pen transparent so brush fills rect
   dc.SetPen(*wxTRANSPARENT_PEN);
   dc.SetBrush(brush);
   
   dc.DrawRectangle(rect);
   
   dc.SetBrush(wxNullBrush);     // restore brush
   dc.SetPen(wxNullPen);         // restore pen
}

// -----------------------------------------------------------------------------

void CreatePaleBitmap(const wxBitmap& inmap, wxBitmap& outmap)
{
   wxImage oldimg = inmap.ConvertToImage();

   wxImage newimg;
   newimg.Create(oldimg.GetWidth(), oldimg.GetHeight(), false);
   unsigned char *dest = newimg.GetData();

   unsigned char *src = oldimg.GetData();
   bool hasMask = oldimg.HasMask();
   unsigned char maskRed = oldimg.GetMaskRed();
   unsigned char maskGreen = oldimg.GetMaskGreen();
   unsigned char maskBlue = oldimg.GetMaskBlue();

   if (hasMask)
      newimg.SetMaskColour(maskRed, maskGreen, maskBlue);
   
   const long size = oldimg.GetWidth() * oldimg.GetHeight();
   for ( long i = 0; i < size; i++, src += 3, dest += 3 ) {
      if ( hasMask && src[0] == maskRed && src[1] == maskGreen && src[2] == maskBlue ) {
         // don't modify the mask
         memcpy(dest, src, 3);
      } else {
         // make pixel a pale shade of gray
         int gray = (int) ((src[0] + src[1] + src[2]) / 3.0);
         gray = (int) (170.0 + (gray / 4.0));
         dest[0] = dest[1] = dest[2] = gray;
      }
   }

   // copy the alpha channel, if any
   if (oldimg.HasAlpha()) {
      const size_t alphaSize = oldimg.GetWidth() * oldimg.GetHeight();
      unsigned char *alpha = (unsigned char*) malloc(alphaSize);
      memcpy(alpha, oldimg.GetAlpha(), alphaSize);
      newimg.InitAlpha();
      newimg.SetAlpha(alpha);
   }

   outmap = wxBitmap(newimg);
}
