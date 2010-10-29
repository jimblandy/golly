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

#include "wx/spinctrl.h"   // for wxSpinCtrl
#include "wx/progdlg.h"    // for wxProgressDialog
#include "wx/image.h"      // for wxImage

#include "wxgolly.h"       // for wxGetApp, viewptr, mainptr
#include "wxview.h"        // for viewptr->...
#include "wxmain.h"        // for mainptr->...
#include "wxscript.h"      // for inscript, PassKeyToScript
#include "wxprefs.h"       // for allowbeep
#include "wxutils.h"

#ifdef __WXMAC__
   #include <Carbon/Carbon.h>
   #include "wx/mac/corefoundation/cfstring.h"     // for wxMacCFStringHolder
#endif

// -----------------------------------------------------------------------------

// need platform-specific gap after OK/Cancel buttons
#ifdef __WXMAC__
   const int STDHGAP = 0;
#elif defined(__WXMSW__)
   const int STDHGAP = 6;
#else
   const int STDHGAP = 10;
#endif

// -----------------------------------------------------------------------------

void Note(const wxString& msg)
{
   wxString title = wxGetApp().GetAppName() + _(" note:");
   #ifdef __WXMAC__
      wxSetCursor(*wxSTANDARD_CURSOR);
   #endif
   wxMessageBox(msg, title, wxOK | wxICON_INFORMATION, wxGetActiveWindow());
}

// -----------------------------------------------------------------------------

void Warning(const wxString& msg)
{
   Beep();
   wxString title = wxGetApp().GetAppName() + _(" warning:");
   #ifdef __WXMAC__
      wxSetCursor(*wxSTANDARD_CURSOR);
   #endif
   wxMessageBox(msg, title, wxOK | wxICON_EXCLAMATION, wxGetActiveWindow());
}

// -----------------------------------------------------------------------------

void Fatal(const wxString& msg)
{
   Beep();
   wxString title = wxGetApp().GetAppName() + _(" error:");
   #ifdef __WXMAC__
      wxSetCursor(*wxSTANDARD_CURSOR);
   #endif
   wxMessageBox(msg, title, wxOK | wxICON_ERROR, wxGetActiveWindow());

   exit(1);    // safer than calling wxExit()
}

// -----------------------------------------------------------------------------

void Beep()
{
   if (allowbeep) wxBell();
}

// =============================================================================

// define a modal dialog for getting a string

class StringDialog : public wxDialog
{
public:
   StringDialog(wxWindow* parent, const wxString& title,
                const wxString& prompt, const wxString& instring);

   virtual bool TransferDataFromWindow();    // called when user hits OK

   wxString GetValue() { return result; }

private:   
   wxTextCtrl* textbox;       // text box for entering the string
   wxString result;           // the resulting string
};

// -----------------------------------------------------------------------------

StringDialog::StringDialog(wxWindow* parent, const wxString& title,
                           const wxString& prompt, const wxString& instring)
{
   Create(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize);

   // create the controls
   wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
   SetSizer(topSizer);

   textbox = new wxTextCtrl(this, wxID_ANY, instring);
                            // add wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE
                            // as an option to allow multi-line input???!!!
   wxStaticText* promptlabel = new wxStaticText(this, wxID_STATIC, prompt);

   wxSizer* stdbutts = CreateButtonSizer(wxOK | wxCANCEL);
   
   // position the controls
   wxBoxSizer* stdhbox = new wxBoxSizer(wxHORIZONTAL);
   stdhbox->Add(stdbutts, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxRIGHT, STDHGAP);
   wxSize minsize = stdhbox->GetMinSize();
   if (minsize.GetWidth() < 250) {
      minsize.SetWidth(250);
      stdhbox->SetMinSize(minsize);
   }

   topSizer->AddSpacer(12);
   topSizer->Add(promptlabel, 0, wxLEFT | wxRIGHT, 10);
   topSizer->AddSpacer(10);
   topSizer->Add(textbox, 0, wxGROW | wxLEFT | wxRIGHT, 10);
   topSizer->AddSpacer(12);
   topSizer->Add(stdhbox, 1, wxGROW | wxTOP | wxBOTTOM, 10);

   GetSizer()->Fit(this);
   GetSizer()->SetSizeHints(this);
   Centre();

   // select initial string (must do this last on Windows)
   textbox->SetFocus();
   textbox->SetSelection(0,999);    // wxMac bug: -1,-1 doesn't work here
}

// -----------------------------------------------------------------------------

bool StringDialog::TransferDataFromWindow()
{
   result = textbox->GetValue();
   return true;
}

// -----------------------------------------------------------------------------

bool GetString(const wxString& title, const wxString& prompt,
               const wxString& instring, wxString& outstring)
{
   StringDialog dialog(wxGetApp().GetTopWindow(), title, prompt, instring);
   if ( dialog.ShowModal() == wxID_OK ) {
      outstring = dialog.GetValue();
      return true;
   } else {
      // user hit Cancel button
      return false;
   }
}

// =============================================================================

// define a modal dialog for getting an integer

class IntegerDialog : public wxDialog
{
public:
   IntegerDialog(wxWindow* parent,
                 const wxString& title,
                 const wxString& prompt,
                 int inval, int minval, int maxval);

   virtual bool TransferDataFromWindow();    // called when user hits OK

   #ifdef __WXMAC__
      void OnSpinCtrlChar(wxKeyEvent& event);
   #endif

   int GetValue() { return result; }

private:   
   enum {
      ID_SPIN_CTRL = wxID_HIGHEST + 1
   };
   wxSpinCtrl* spinctrl;   // for entering the integer
   int minint;             // minimum value
   int maxint;             // maximum value
   int result;             // the resulting integer
};

// -----------------------------------------------------------------------------

#ifdef __WXMAC__

// override key event handler for wxSpinCtrl to allow key checking
class MySpinCtrl : public wxSpinCtrl
{
public:
   MySpinCtrl(wxWindow* parent, wxWindowID id) : wxSpinCtrl(parent, id)
   {
      // create a dynamic event handler for the underlying wxTextCtrl
      wxTextCtrl* textctrl = GetText();
      if (textctrl) {
         textctrl->Connect(wxID_ANY, wxEVT_CHAR,
                           wxKeyEventHandler(IntegerDialog::OnSpinCtrlChar));
      }
   }
};

void IntegerDialog::OnSpinCtrlChar(wxKeyEvent& event)
{
   int key = event.GetKeyCode();
   
   if (event.CmdDown()) {
      // allow handling of cmd-x/v/etc
      event.Skip();

   } else if ( key == WXK_TAB ) {
      /* why does this crash???!!!
      spinctrl->SetFocus();
      spinctrl->SetSelection(0,999);
      */
      wxSpinCtrl* sc = (wxSpinCtrl*) FindWindowById(ID_SPIN_CTRL);
      if ( sc ) {
         sc->SetFocus();
         sc->SetSelection(0,999);
      }

   } else if ( key >= ' ' && key <= '~' ) {
      if ( (key >= '0' && key <= '9') || key == '+' || key == '-' ) {
         // allow digits and + or -
         event.Skip();
      } else {
         // disallow any other displayable ascii char
         Beep();
      }

   } else {
      event.Skip();
   }
}

#else

#define MySpinCtrl wxSpinCtrl

#endif // __WXMAC__

// -----------------------------------------------------------------------------

IntegerDialog::IntegerDialog(wxWindow* parent,
                             const wxString& title,
                             const wxString& prompt,
                             int inval, int minval, int maxval)
{
   Create(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize);

   minint = minval;
   maxint = maxval;

   // create the controls
   wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
   SetSizer(topSizer);

   spinctrl = new MySpinCtrl(this, ID_SPIN_CTRL);
   spinctrl->SetRange(minval, maxval);
   spinctrl->SetValue(inval);
   
   wxStaticText* promptlabel = new wxStaticText(this, wxID_STATIC, prompt);

   wxSizer* stdbutts = CreateButtonSizer(wxOK | wxCANCEL);
   
   // position the controls
   wxBoxSizer* stdhbox = new wxBoxSizer(wxHORIZONTAL);
   stdhbox->Add(stdbutts, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxRIGHT, STDHGAP);
   wxSize minsize = stdhbox->GetMinSize();
   if (minsize.GetWidth() < 250) {
      minsize.SetWidth(250);
      stdhbox->SetMinSize(minsize);
   }

   topSizer->AddSpacer(12);
   topSizer->Add(promptlabel, 0, wxLEFT | wxRIGHT, 10);
   topSizer->AddSpacer(10);
   topSizer->Add(spinctrl, 0, wxGROW | wxLEFT | wxRIGHT, 10);
   topSizer->AddSpacer(12);
   topSizer->Add(stdhbox, 1, wxGROW | wxTOP | wxBOTTOM, 10);

   GetSizer()->Fit(this);
   GetSizer()->SetSizeHints(this);
   Centre();

   // select initial value (must do this last on Windows)
   spinctrl->SetFocus();
   spinctrl->SetSelection(0,999);    // wxMac bug: -1,-1 doesn't work here
}

// -----------------------------------------------------------------------------

bool IntegerDialog::TransferDataFromWindow()
{
#if defined(__WXMSW__) || defined(__WXGTK__)
   // spinctrl->GetValue() always returns a value within range even if
   // the text ctrl doesn't contain a valid number -- yuk!!!
   result = spinctrl->GetValue();
   if (result < minint || result > maxint) {
#else
   // GetTextValue returns FALSE if text ctrl doesn't contain a valid number
   // or the number is out of range, but it's not available in wxMSW or wxGTK
   if ( !spinctrl->GetTextValue(&result) || result < minint || result > maxint ) {
#endif
      wxString msg;
      msg.Printf(_("Value must be from %d to %d."), minint, maxint);
      Warning(msg);
      spinctrl->SetFocus();
      spinctrl->SetSelection(0,999);   // wxMac bug: -1,-1 doesn't work here
      return false;
   } else {
      return true;
   }
}

// -----------------------------------------------------------------------------

bool GetInteger(const wxString& title, const wxString& prompt,
                int inval, int minval, int maxval, int* outval)
{
   IntegerDialog dialog(wxGetApp().GetTopWindow(), title, prompt,
                        inval, minval, maxval);
   if ( dialog.ShowModal() == wxID_OK ) {
      *outval = dialog.GetValue();
      return true;
   } else {
      // user hit Cancel button
      return false;
   }
}

// =============================================================================

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

#endif // __WXMAC__

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
#endif // __WXMAC__
}

// =============================================================================

// globals for showing progress

wxProgressDialog* progdlg = NULL;         // progress dialog
const int maxprogrange = 1000000000;      // maximum range (best if very large)
wxStopWatch* progwatch = NULL;            // stopwatch for progress dialog
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
   if (mainptr->IsActive()) viewptr->SetCursor(*wxHOURGLASS_CURSOR);
}

// -----------------------------------------------------------------------------

bool AbortProgress(double fraction_done, const wxString& newmsg)
{
   long msecs = progwatch->Time();
   if (progdlg) {
      if (msecs < prognext) return false;
      prognext = msecs + 100;    // call Update/Pulse about 10 times per sec
      bool cancel;
      if (fraction_done < 0.0) {
         // show indeterminate progress gauge
         cancel = !progdlg->Pulse(newmsg);
      } else {
         cancel = !progdlg->Update(int((double)maxprogrange * fraction_done), newmsg);
      }
      if (cancel) {
         // user hit Cancel button
         if (inscript) PassKeyToScript(WXK_ESCAPE);     // abort script
         return true;
      } else {
         return false;
      }
   } else {
      // note that fraction_done is not always an accurate estimator for how long
      // the task will take, especially when we use nextcell for cut/copy
      if ( (msecs > 1000 && fraction_done < 0.3) || msecs > 2500 ) {
         // task is probably going to take a while so create progress dialog
         progdlg = new wxProgressDialog(progtitle, wxEmptyString,
                                        maxprogrange, wxGetActiveWindow(),
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
      prognext = msecs + 10;     // short delay until 1st Update/Pulse
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
   }
   if (progwatch) {
      delete progwatch;
      progwatch = NULL;
   }
   // BeginProgress changed cursor so reset it
   viewptr->CheckCursor(mainptr->IsActive());
}

// =============================================================================

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
   unsigned char* dest = newimg.GetData();
   unsigned char* src = oldimg.GetData();
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
      unsigned char* alpha = (unsigned char*) malloc(alphaSize);
      if (alpha) {
         memcpy(alpha, oldimg.GetAlpha(), alphaSize);
         newimg.InitAlpha();
         newimg.SetAlpha(alpha);
      }
   }

   outmap = wxBitmap(newimg);
}

// -----------------------------------------------------------------------------

bool IsScriptFile(const wxString& filename)
{
   wxString ext = filename.AfterLast('.');
   // if filename has no extension then ext == filename
   if (ext == filename) return false;
   // currently we support Perl or Python scripts
   return ( ext.IsSameAs(wxT("pl"),false) ||
            ext.IsSameAs(wxT("py"),false) );
}

// -----------------------------------------------------------------------------

bool IsHTMLFile(const wxString& filename)
{
   wxString ext = filename.AfterLast('.');
   // if filename has no extension then ext == filename
   if (ext == filename) return false;
   return ( ext.IsSameAs(wxT("htm"),false) ||
            ext.IsSameAs(wxT("html"),false) );
}

// -----------------------------------------------------------------------------

bool IsTextFile(const wxString& filename)
{
   if (!IsHTMLFile(filename)) {
      // if non-html file name contains "readme" then assume it's a text file
      wxString name = filename.AfterLast(wxFILE_SEP_PATH).MakeLower();
      if (name.Contains(wxT("readme"))) return true;
   }
   wxString ext = filename.AfterLast('.');
   // if filename has no extension then ext == filename
   if (ext == filename) return false;
   return ( ext.IsSameAs(wxT("txt"),false) ||
            ext.IsSameAs(wxT("doc"),false) );
}

// -----------------------------------------------------------------------------

bool IsZipFile(const wxString& filename)
{
   wxString ext = filename.AfterLast('.');
   // if filename has no extension then ext == filename
   if (ext == filename) return false;
   return ( ext.IsSameAs(wxT("zip"),false) ||
            ext.IsSameAs(wxT("gar"),false) );
}

// -----------------------------------------------------------------------------

bool IsRuleFile(const wxString& filename)
{
   wxString ext = filename.AfterLast('.');
   // if filename has no extension then ext == filename
   if (ext == filename) return false;
   return ( ext.IsSameAs(wxT("table"),false) ||
            ext.IsSameAs(wxT("tree"),false) ||
            ext.IsSameAs(wxT("colors"),false) ||
            ext.IsSameAs(wxT("icons"),false) );
}
