/*** /

 This file is part of Golly, a Game of Life Simulator.
 Copyright (C) 2013 Andrew Trevorrow and Tomas Rokicki.

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

#include "bigint.h"
#include "lifealgo.h"

#include "utils.h"      // for Fatal, Beep
#include "prefs.h"      // for mindelay, maxdelay, etc
#include "algos.h"      // for algoinfo
#include "layer.h"      // for currlayer
#include "view.h"       // for nopattupdate
#include "status.h"

#ifdef ANDROID_GUI
    #include "jnicalls.h"   // for UpdateStatus, GetRuleName
#endif
#ifdef IOS_GUI
    #import "PatternViewController.h"   // for UpdateStatus
    #import "RuleViewController.h"      // for GetRuleName
#endif

// -----------------------------------------------------------------------------

std::string status1;    // top line
std::string status2;    // middle line
std::string status3;    // bottom line

//!!! eventually we'll make the following prefixes dynamic strings
// that depend on whether the device's screen size is large or small
// (and whether it is in portrait mode or landscape mode???)
#ifdef ANDROID_GUI
    const char* algo_prefix =  "   Algo=";
    const char* rule_prefix =  "   Rule=";
    const char* gen_prefix =   "Gen=";
    const char* pop_prefix =   "   Pop=";
    const char* scale_prefix = "   Scale=";
    const char* step_prefix =  "   ";
    const char* xy_prefix =    "   XY=";
#endif
#ifdef IOS_GUI
    const char* algo_prefix =  "    Algorithm=";
    const char* rule_prefix =  "    Rule=";
    const char* gen_prefix =   "Generation=";
    const char* pop_prefix =   "    Population=";
    const char* scale_prefix = "    Scale=";
    const char* step_prefix =  "    ";
    const char* xy_prefix =    "    XY=";
#endif

// -----------------------------------------------------------------------------

void UpdateStatusLines()
{
    std::string rule = currlayer->algo->getrule();

    status1 = "Pattern=";
    if (currlayer->dirty) {
        // display asterisk to indicate pattern has been modified
        status1 += "*";
    }
    status1 += currlayer->currname;
    status1 += algo_prefix;
    status1 += GetAlgoName(currlayer->algtype);
    status1 += rule_prefix;
    status1 += rule;

    // show rule name if one exists and is not same as rule
    // (best NOT to remove any suffix like ":T100,200" in case we allow
    // users to name "B3/S23:T100,200" as "Life on torus")
    std::string rulename = GetRuleName(rule);
    if (!rulename.empty() && rulename != rule) {
        status1 += " [";
        status1 += rulename;
        status1 += "]";
    }

    char scalestr[32];
    int mag = currlayer->view->getmag();
    if (mag < 0) {
        sprintf(scalestr, "2^%d:1", -mag);
    } else {
        sprintf(scalestr, "1:%d", 1 << mag);
    }

    char stepstr[32];
    if (currlayer->currexpo < 0) {
        // show delay in secs
        sprintf(stepstr, "Delay=%gs", (double)GetCurrentDelay() / 1000.0);
    } else {
        sprintf(stepstr, "Step=%d^%d", currlayer->currbase, currlayer->currexpo);
    }

    status2 = gen_prefix;
    if (nopattupdate) {
        status2 += "0";
    } else {
        status2 += Stringify(currlayer->algo->getGeneration());
    }
    status2 += pop_prefix;
    if (nopattupdate) {
        status2 += "0";
    } else {
        bigint popcount = currlayer->algo->getPopulation();
        if (popcount.sign() < 0) {
            // getPopulation returns -1 if it can't be calculated
            status2 += "?";
        } else {
            status2 += Stringify(popcount);
        }
    }
    status2 += scale_prefix;
    status2 += scalestr;
    status2 += step_prefix;
    status2 += stepstr;      // starts with Delay or Step
    status2 += xy_prefix;
    status2 += Stringify(currlayer->view->x);
    status2 += " ";
    status2 += Stringify(currlayer->view->y);
}

// -----------------------------------------------------------------------------

void ClearMessage()
{
    if (status3.length() == 0) return;    // no need to clear message

    status3.clear();
    UpdateStatus();
}

// -----------------------------------------------------------------------------

void DisplayMessage(const char* s)
{
    status3 = s;
    UpdateStatus();
}

// -----------------------------------------------------------------------------

void ErrorMessage(const char* s)
{
    Beep();
    DisplayMessage(s);
}

// -----------------------------------------------------------------------------

void SetMessage(const char* s)
{
    // set message string without displaying it
    status3 = s;
}

// -----------------------------------------------------------------------------

void UpdateXYLocation()
{
    /* no need???!!!
    int wd, ht;
    GetClientSize(&wd, &ht);
    if (wd > h_xy && ht > 0) {
        wxRect r;
        if (showexact)
            r = wxRect( wxPoint(0, XLINE+DESCHT-LINEHT), wxPoint(wd-1, YLINE+DESCHT) );
        else
            r = wxRect( wxPoint(h_xy, 0), wxPoint(wd-1, BASELINE1+DESCHT) );
        Refresh(false, &r);
    }
    */
}

// -----------------------------------------------------------------------------

int GetCurrentDelay()
{
    int gendelay = mindelay * (1 << (-currlayer->currexpo - 1));
    if (gendelay > maxdelay) gendelay = maxdelay;
    return gendelay;
}

// -----------------------------------------------------------------------------

char* Stringify(const bigint& b)
{
    static char buf[32];
    char* p = buf;
    double d = b.todouble();
    if ( fabs(d) > 1000000000.0 ) {
        // use e notation for abs value > 10^9 (agrees with min & max_coord)
        sprintf(p, "%g", d);
    } else {
        // show exact value with commas inserted for readability
        if ( d < 0 ) {
            d = - d;
            *p++ = '-';
        }
        sprintf(p, "%.f", d);
        int len = strlen(p);
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
    }
    return p;
}
