// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
    #include "wx/wx.h"     // for all others include the necessary headers
#endif

#include "wx/dcbuffer.h"   // for wxBufferedPaintDC

#include "bigint.h"
#include "lifealgo.h"

#include "wxgolly.h"       // for wxGetApp, etc
#include "wxutils.h"       // for Fatal, Beep, FillRect
#include "wxprefs.h"       // for mindelay, maxdelay, etc
#include "wxview.h"        // for viewptr->...
#include "wxmain.h"        // for mainptr->...
#include "wxscript.h"      // for inscript
#include "wxalgos.h"       // for algoinfo
#include "wxlayer.h"       // for currlayer
#include "wxtimeline.h"    // for TimelineExists
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
int h_x_ex, h_y_ex;

#ifdef __WXMAC__
    // gray line at bottom of status bar (matches line at bottom of OS X title bar)
    wxPen linepen(wxColor(140,140,140));
#endif

// -----------------------------------------------------------------------------

void StatusBar::ClearMessage()
{
    if (inscript) return;                     // let script control messages
    if (viewptr->waitingforclick) return;     // don't clobber message
    if (statusmsg.IsEmpty()) return;          // no need to clear message
    
    statusmsg.Clear();
    if (statusht > 0) {
        int wd, ht;
        GetClientSize(&wd, &ht);
        if (wd > 0 && ht > 0) {
            // update bottom line
            wxRect r = wxRect(wxPoint(0,statusht-BOTGAP+DESCHT-LINEHT),
                              wxPoint(wd-1,ht-1) );
            Refresh(false, &r);
            // nicer not to call Update() here otherwise users can see different
            // colored bands in status bar when changing algos
        }
    }
}

// -----------------------------------------------------------------------------

void StatusBar::DisplayMessage(const wxString& s)
{
    if (inscript) return;                     // let script control messages
    statusmsg = s;
    if (statusht > 0) {
        int wd, ht;
        GetClientSize(&wd, &ht);
        if (wd > 0 && ht > 0) {
            // update bottom line
            wxRect r = wxRect( wxPoint(0,statusht-BOTGAP+DESCHT-LINEHT),
                              wxPoint(wd-1,ht-1) );
            Refresh(false, &r);
            // no need to show message immediately???
            // Update();
        }
    }
}

// -----------------------------------------------------------------------------

void StatusBar::ErrorMessage(const wxString& s)
{
    if (inscript) return;                     // let script control messages
    Beep();
    DisplayMessage(s);
}

// -----------------------------------------------------------------------------

void StatusBar::SetMessage(const wxString& s)
{
    if (inscript) return;                     // let script control messages
    
    // set message string without displaying it
    statusmsg = s;
}

// -----------------------------------------------------------------------------

void StatusBar::UpdateXYLocation()
{
    int wd, ht;
    GetClientSize(&wd, &ht);
    if (ht > 0 && (wd > h_xy || showexact)) {
        wxRect r;
        if (showexact)
            r = wxRect( wxPoint(0, XLINE+DESCHT-LINEHT), wxPoint(wd-1, YLINE+DESCHT) );
        else
            r = wxRect( wxPoint(h_xy, 0), wxPoint(wd-1, BASELINE1+DESCHT) );
        Refresh(false, &r);
    }
}

// -----------------------------------------------------------------------------

void StatusBar::CheckMouseLocation(bool active)
{
    if (statusht == 0 && !inscript) return;
    
    if ( !active ) {
        // main window is not in front so clear XY location
        showxy = false;
        if (statusht > 0) UpdateXYLocation();
        if (inscript) mousepos = wxEmptyString;
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
            if (statusht > 0) UpdateXYLocation();
        } else if (!showxy) {
            showxy = true;
            if (statusht > 0) UpdateXYLocation();
        }
        if (inscript) {
            mousepos = wxString(xpos.tostring('\0'), wxConvLocal);
            mousepos += wxT(" ");
            mousepos += wxString(ypos.tostring('\0'), wxConvLocal);
        }
    } else {
        // outside viewport so clear XY location
        showxy = false;
        if (statusht > 0) UpdateXYLocation();
        if (inscript) mousepos = wxEmptyString;
    }
}

// -----------------------------------------------------------------------------

void StatusBar::SetStatusFont(wxDC& dc)
{
    dc.SetFont(*statusfont);
    dc.SetTextForeground(*wxBLACK);
    dc.SetBrush(*wxBLACK_BRUSH);
    dc.SetBackgroundMode(wxTRANSPARENT);
}

// -----------------------------------------------------------------------------

void StatusBar::DisplayText(wxDC& dc, const wxString& s, wxCoord x, wxCoord y)
{
    // DrawText's y parameter is top of text box but we pass in baseline
    // so adjust by textascent which depends on platform and OS version -- yuk!
    dc.DrawText(s, x, y - textascent);
}

// -----------------------------------------------------------------------------

wxString StatusBar::Stringify(const bigint& b)
{
    static char buf[32];
    char* p = buf;
    double d = b.toscinot();
    if ( fabs(d) < 10.0 ) {
        // show exact value with commas inserted for readability
        d = b.todouble();
        if ( d < 0 ) {
            d = - d;
            *p++ = '-';
        }
        sprintf(p, "%.f", d);
        int len = (int)strlen(p);
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
    } else {
      // use e notation for abs value > 10^9 (agrees with min & max_coord)
      const char * sign = "";
      if (d < 0) {
        sign = "-";
        d = 0.0 - d;
      }
      double exp = floor(d);
      d = (d - exp) * 10.0;
      exp = exp - 1.0;
      // UI has been set up to accomodate "9.999999e+999"
      //       which is the same width as "9.9999999e+99", etc.
      if (exp < 100.0) {
        // two-digit exponent
        sprintf(p, "%s%9.7fe+%.0f", sign, d, exp); // 9.9999999e+99
      } else if (exp < 1000.0) {
        sprintf(p, "%s%8.6fe+%.0f", sign, d, exp); // 9.999999e+999
      } else if (exp < 10000.0) {
        sprintf(p, "%s%7.5fe+%.0f", sign, d, exp); // 9.99999e+9999
      } else if (exp < 100000.0) {
        sprintf(p, "%s%6.4fe+%.0f", sign, d, exp); // 9.9999e+99999
      } else {
        // for 6-digit exponent or larger we'll just always show a "d.ddd"
        // mantissa. 7-digit exponent appears unattainable at the present
        // time (late 2011)
        sprintf(p, "%s%5.3fe+%.0f", sign, d, exp);
      }
    }

    return wxString(p, wxConvLocal);
}

// -----------------------------------------------------------------------------

int StatusBar::GetCurrentDelay()
{
    int gendelay = mindelay * (1 << (-currlayer->currexpo - 1));
    if (gendelay > maxdelay) gendelay = maxdelay;
    return gendelay;
}

// -----------------------------------------------------------------------------

void StatusBar::DrawStatusBar(wxDC& dc, wxRect& updaterect)
{
    int wd, ht;
    GetClientSize(&wd, &ht);
    if (wd < 1 || ht < 1) return;
    
    wxRect r = wxRect(0, 0, wd, ht);
    FillRect(dc, r, *(algoinfo[currlayer->algtype]->statusbrush));
    
#if defined(__WXMSW__)
    // draw gray lines at top and left edges
    dc.SetPen(*wxGREY_PEN);
    dc.DrawLine(0, 0, r.width, 0);
    dc.DrawLine(0, 0, 0, r.height);
    // don't draw right edge on XP
    // dc.DrawLine(r.GetRight(), 0, r.GetRight(), r.height);
#elif defined(__WXMAC__)
    // draw gray line at bottom edge
    dc.SetPen(linepen);
    dc.DrawLine(0, r.GetBottom(), r.width, r.GetBottom());
#else
    // draw gray line at bottom edge
    dc.SetPen(*wxLIGHT_GREY_PEN);
    dc.DrawLine(0, r.GetBottom(), r.width, r.GetBottom());
#endif
    dc.SetPen(wxNullPen);
    
    // must be here rather than in StatusBar::OnPaint; it looks like
    // some call resets the font
    SetStatusFont(dc);
    
    wxString strbuf;
    
    if (updaterect.y >= statusht-BOTGAP+DESCHT-LINEHT) {
        // only show possible message in bottom line -- see below
        
    } else if (showexact) {
        // might only need to display X and Y lines
        if (updaterect.y < XLINE+DESCHT-LINEHT) {
            strbuf = _("Generation = ");
            if (viewptr->nopattupdate) {
                strbuf += _("0");
            } else {
                strbuf += wxString(currlayer->algo->getGeneration().tostring(), wxConvLocal);
            }
            DisplayText(dc, strbuf, h_gen, GENLINE);
            
            strbuf = _("Population = ");
            if (viewptr->nopattupdate) {
                strbuf += _("0");
            } else if (mainptr->generating && !showpopulation) {
                strbuf += _("disabled");
            } else {
                bigint popcount = currlayer->algo->getPopulation();
                if (popcount.sign() < 0) {
                    // getPopulation returns -1 if it can't be calculated
                    strbuf += _("?");
                } else {
                    strbuf += wxString(popcount.tostring(), wxConvLocal);
                }
            }
            DisplayText(dc, strbuf, h_gen, POPLINE);
            
            // no need to show scale as an exact number
            if (viewptr->GetMag() < 0) {
                strbuf.Printf(_("Scale = 2^%d:1"), -viewptr->GetMag());
            } else {
                strbuf.Printf(_("Scale = 1:%d"), 1 << viewptr->GetMag());
            }
            DisplayText(dc, strbuf, h_gen, SCALELINE);
            
            if (currlayer->currexpo < 0) {
                // show delay in secs
                strbuf.Printf(_("Delay = %gs"), (double)GetCurrentDelay() / 1000.0);
            } else {
                // no real need to show step as an exact number
                strbuf.Printf(_("Step = %d^%d"), currlayer->currbase, currlayer->currexpo);
            }
            DisplayText(dc, strbuf, h_gen, STEPLINE);
        }
        
        DisplayText(dc, _("X ="), h_gen, XLINE);
        DisplayText(dc, _("Y ="), h_gen, YLINE);
        if (showxy) {
            bigint xo, yo;
            bigint xpos = currx;   xpos -= currlayer->originx;
            bigint ypos = curry;   ypos -= currlayer->originy;
            if (mathcoords) {
                // Y values increase upwards
                bigint temp = 0;
                temp -= ypos;
                ypos = temp;
            }
            DisplayText(dc, wxString(xpos.tostring(), wxConvLocal),
                        h_x_ex, XLINE);
            DisplayText(dc, wxString(ypos.tostring(), wxConvLocal),
                        h_y_ex, YLINE);
        }
        
    } else {
        // showexact is false so show info in top line
        if (updaterect.x < h_xy) {
            // show all info
            strbuf = _("Generation=");
            if (viewptr->nopattupdate) {
                strbuf += _("0");
            } else {
                strbuf += Stringify(currlayer->algo->getGeneration());
            }
            DisplayText(dc, strbuf, h_gen, BASELINE1);
            
            strbuf = _("Population=");
            if (viewptr->nopattupdate) {
                strbuf += _("0");
            } else if (mainptr->generating && !showpopulation) {
                strbuf += _("disabled");
            } else {
                bigint popcount = currlayer->algo->getPopulation();
                if (popcount.sign() < 0) {
                    // getPopulation returns -1 if it can't be calculated
                    strbuf += _("?");
                } else {
                    strbuf += Stringify(popcount);
                }
            }
            DisplayText(dc, strbuf, h_pop, BASELINE1);
            
            if (viewptr->GetMag() < 0) {
                strbuf.Printf(_("Scale=2^%d:1"), -viewptr->GetMag());
            } else {
                strbuf.Printf(_("Scale=1:%d"), 1 << viewptr->GetMag());
            }
            DisplayText(dc, strbuf, h_scale, BASELINE1);
            
            if (currlayer->currexpo < 0) {
                // show delay in secs
                strbuf.Printf(_("Delay=%gs"), (double)GetCurrentDelay() / 1000.0);
            } else {
                strbuf.Printf(_("Step=%d^%d"), currlayer->currbase, currlayer->currexpo);
            }
            DisplayText(dc, strbuf, h_step, BASELINE1);
        }
        
        strbuf = _("XY=");
        if (showxy) {
            bigint xo, yo;
            bigint xpos = currx;   xpos -= currlayer->originx;
            bigint ypos = curry;   ypos -= currlayer->originy;
            if (mathcoords) {
                // Y values increase upwards
                bigint temp = 0;
                temp -= ypos;
                ypos = temp;
            }
            strbuf += Stringify(xpos);
            strbuf += wxT(" ");
            strbuf += Stringify(ypos);
        }
        DisplayText(dc, strbuf, h_xy, BASELINE1);
    }
    
    if (!statusmsg.IsEmpty()) {
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
#if defined(__WXMAC__) || defined(__WXGTK__)
    // windows on Mac OS X and GTK+ 2.0 are automatically buffered
    wxPaintDC dc(this);
#else
    // use wxWidgets buffering to avoid flicker
    int wd, ht;
    GetClientSize(&wd, &ht);
    // wd or ht might be < 1 on Windows
    if (wd < 1) wd = 1;
    if (ht < 1) ht = 1;
    if (wd != statbitmapwd || ht != statbitmapht) {
        // need to create a new bitmap for status bar
        delete statbitmap;
        statbitmap = new wxBitmap(wd, ht);
        statbitmapwd = wd;
        statbitmapht = ht;
    }
    if (statbitmap == NULL) Fatal(_("Not enough memory to render status bar!"));
    wxBufferedPaintDC dc(this, *statbitmap);
#endif
    
    wxRect updaterect = GetUpdateRegion().GetBox();
    DrawStatusBar(dc, updaterect);
}

// -----------------------------------------------------------------------------

bool StatusBar::ClickInGenBox(int x, int y)
{
    if (showexact)
        return x >= 0 && y > (GENLINE+DESCHT-LINEHT) && y <= (GENLINE+DESCHT);
    else
        return x >= h_gen && x <= h_pop - 20 && y <= (BASELINE1+DESCHT);
}

// -----------------------------------------------------------------------------

bool StatusBar::ClickInPopBox(int x, int y)
{
    if (showexact)
        return x >= 0 && y > (POPLINE+DESCHT-LINEHT) && y <= (POPLINE+DESCHT);
    else
        return x >= h_pop && x <= h_scale - 20 && y <= (BASELINE1+DESCHT);
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
    if (inscript) return;    // let script control scale, step, etc
    ClearMessage();
    
    if ( ClickInGenBox(event.GetX(), event.GetY()) && !mainptr->generating ) {
        if (TimelineExists()) {
            ErrorMessage(_("You can't change the generation count if there is a timeline."));
        } else {
            mainptr->SetGeneration();
        }
        
    } else if ( ClickInPopBox(event.GetX(), event.GetY()) ) {
        if (mainptr->generating) {
            mainptr->ToggleShowPopulation();
            mainptr->UpdateMenuItems();
        }
        
    } else if ( ClickInScaleBox(event.GetX(), event.GetY()) ) {
        if (viewptr->GetMag() != 0) {
            // reset scale to 1:1
            viewptr->SetMag(0);
        }
        
    } else if ( ClickInStepBox(event.GetX(), event.GetY()) ) {
        if (TimelineExists()) {
            ErrorMessage(_("You can't change the step size if there is a timeline."));
        } else if (currlayer->currbase != algoinfo[currlayer->algtype]->defbase ||
                   currlayer->currexpo != 0) {
            // reset base step to default value and step exponent to 0
            currlayer->currbase = algoinfo[currlayer->algtype]->defbase;
            mainptr->SetStepExponent(0);
            // update status bar
            Refresh(false);
        }
    }
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
#ifdef __WXGTK__
    // avoid erasing background on GTK+
    SetBackgroundStyle(wxBG_STYLE_CUSTOM);
#endif
    
    // create font for text in status bar and set textascent for use in DisplayText
#ifdef __WXMSW__
    // use smaller, narrower font on Windows
    statusfont = wxFont::New(8, wxDEFAULT, wxNORMAL, wxNORMAL);
    
    int major, minor;
    wxGetOsVersion(&major, &minor);
    if ( major > 5 || (major == 5 && minor >= 1) ) {
        // 5.1+ means XP or later (Vista or later if major >= 6)
        textascent = 11;
    } else {
        textascent = 10;
    }
#elif defined(__WXGTK__)
    // use smaller font on GTK
    statusfont = wxFont::New(8, wxMODERN, wxNORMAL, wxNORMAL);
    textascent = 11;
#elif defined(__WXOSX_COCOA__)
    // we need to specify facename to get Monaco instead of Courier
    statusfont = wxFont::New(10, wxMODERN, wxNORMAL, wxNORMAL, false, wxT("Monaco"));
    textascent = 10;
#else
    statusfont = wxFont::New(10, wxMODERN, wxNORMAL, wxNORMAL);
    textascent = 10;
#endif
    if (statusfont == NULL) Fatal(_("Failed to create status bar font!"));
    
    // determine horizontal offsets for info in status bar
    wxClientDC dc(this);
    int textwd, textht;
    int mingap = 10;
    SetStatusFont(dc);
    h_gen = 6;
    // when showexact is false:
    dc.GetTextExtent(_("Generation=9.999999e+999"), &textwd, &textht);
    h_pop = h_gen + textwd + mingap;
    dc.GetTextExtent(_("Population=9.999999e+999"), &textwd, &textht);
    h_scale = h_pop + textwd + mingap;
    dc.GetTextExtent(_("Scale=2^9999:1"), &textwd, &textht);
    h_step = h_scale + textwd + mingap;
    dc.GetTextExtent(_("Step=1000000000^9"), &textwd, &textht);
    h_xy = h_step + textwd + mingap;
    // when showexact is true:
    dc.GetTextExtent(_("X = "), &textwd, &textht);
    h_x_ex = h_gen + textwd;
    dc.GetTextExtent(_("Y = "), &textwd, &textht);
    h_y_ex = h_gen + textwd;
    
    statusht = ht;
    showxy = false;
    
    statbitmap = NULL;
    statbitmapwd = -1;
    statbitmapht = -1;
    
    statusmsg.Clear();
}

// -----------------------------------------------------------------------------

StatusBar::~StatusBar()
{
    delete statusfont;
    delete statbitmap;
}
