/*** /
 
 This file is part of Golly, a Game of Life Simulator.
 Copyright (C) 2012 Andrew Trevorrow and Tomas Rokicki.
 
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

#include "utils.h"       // for Fatal, Beep
#include "prefs.h"       // for mindelay, maxdelay, etc
#include "algos.h"       // for algoinfo
#include "layer.h"       // for currlayer
#include "view.h"        // for nopattupdate
#include "status.h"

#import "PatternViewController.h"   // for UpdateStatus
#import "RuleViewController.h"      // for GetRuleName

// -----------------------------------------------------------------------------

std::string statusmsg;        // for messages on bottom line

/* show info on 2nd line in fixed positions!!!???
int h_gen;                    // horizontal position of "Generation"
int h_pop;                    // horizontal position of "Population"
int h_scale;                  // horizontal position of "Scale"
int h_step;                   // horizontal position of "Step"
int h_xy;                     // horizontal position of "XY"
*/

// -----------------------------------------------------------------------------

void DrawStatusBar(CGContextRef context, int wd, int ht, CGRect dirtyrect)
{
    // bg color of status area depends on current algorithm
    [[UIColor colorWithRed:algoinfo[currlayer->algtype]->statusrgb.r / 255.0
                     green:algoinfo[currlayer->algtype]->statusrgb.g / 255.0
                      blue:algoinfo[currlayer->algtype]->statusrgb.b / 255.0
                     alpha:1.0] setFill];
    CGContextFillRect(context, dirtyrect);
    
    // draw thin gray line along bottom
    /* only if edit toolbar is ever hidden!!!
    [[UIColor grayColor] setStroke];
    CGContextSetLineWidth(context, 1.0);
    CGContextMoveToPoint(context, 0, ht);
    CGContextAddLineToPoint(context, wd, ht);
    CGContextStrokePath(context);
    */
    
    // use black for drawing text
    [[UIColor blackColor] setFill];
    
    // add code if showexact???!!!
    
    // add code to limit amount of drawing if dirtyrect's height is smaller than given ht???!!!
    
    // create strings for each line
    std::string strbuf;
    std::string rule = currlayer->algo->getrule();
    
    strbuf = "Pattern=";
    if (currlayer->dirty) {
        // display asterisk to indicate pattern has been modified
        strbuf += "*";
    }
    strbuf += currlayer->currname;
    strbuf += "    Algorithm=";
    strbuf += GetAlgoName(currlayer->algtype);
    strbuf += "    Rule=";
    strbuf += rule;

    // show rule name if one exists and is not same as rule
    // (best NOT to remove any suffix like ":T100,200" in case we allow
    // users to name "B3/S23:T100,200" as "Life on torus")
    std::string rulename = GetRuleName(rule);
    if (!rulename.empty() && rulename != rule) {
        strbuf += " [";
        strbuf += rulename;
        strbuf += "]";
    }
    
    NSString *line1 = [NSString stringWithCString:strbuf.c_str() encoding:NSUTF8StringEncoding];
    
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
    
    strbuf = "Generation=";
    if (nopattupdate) {
        strbuf += "0";
    } else {
        strbuf += Stringify(currlayer->algo->getGeneration());
    }
    strbuf += "    Population=";
    if (nopattupdate) {
        strbuf += "0";
    } else {
        bigint popcount = currlayer->algo->getPopulation();
        if (popcount.sign() < 0) {
            // getPopulation returns -1 if it can't be calculated
            strbuf += "?";
        } else {
            strbuf += Stringify(popcount);
        }
    }
    strbuf += "    Scale=";
    strbuf += scalestr;
    strbuf += "    ";
    strbuf += stepstr;      // starts with Delay or Step
    strbuf += "    XY=";
    strbuf += Stringify(currlayer->view->x);
    strbuf += " ";
    strbuf += Stringify(currlayer->view->y);
    //!!! fix above if we support origin shifting
    
    NSString *line2 = [NSString stringWithCString:strbuf.c_str() encoding:NSUTF8StringEncoding];
    
    // get font to draw lines (do only once)
    static UIFont *font = nil;
    if (font == nil) font = [UIFont systemFontOfSize:13];
    
    // set position of text in each line
    float lineht = ht / 3.0;
    CGRect rect1, rect2;
    rect1.size = [line1 sizeWithFont:font];
    rect2.size = [line2 sizeWithFont:font];
    rect1.origin.x = 5.0;
    rect2.origin.x = 5.0;
    rect1.origin.y = (lineht / 2.0) - (rect1.size.height / 2.0);
    rect2.origin.y = rect1.origin.y + lineht;
    
    // draw the top 2 lines
    [line1 drawInRect:rect1 withFont:font];
    [line2 drawInRect:rect2 withFont:font];
    
    static bool firstcall = true;
    if (firstcall) {
        firstcall = false;
        // set initial message
        statusmsg = "This is Golly version ";
        statusmsg += GOLLY_VERSION;
        statusmsg += " for iOS.  Copyright 2012 The Golly Gang.";
    }

    if (statusmsg.length() > 0) {
        // display status message on bottom line
        NSString *line3 = [NSString stringWithCString:statusmsg.c_str() encoding:NSUTF8StringEncoding];
        CGRect rect3;
        rect3.size = [line3 sizeWithFont:font];
        rect3.origin.x = 5.0;
        rect3.origin.y = rect2.origin.y + lineht;
        [line3 drawInRect:rect3 withFont:font];
    }
}

// -----------------------------------------------------------------------------

void ClearMessage()
{
    if (statusmsg.length() == 0) return;    // no need to clear message
    
    statusmsg.clear();
    UpdateStatus();
}

// -----------------------------------------------------------------------------

void DisplayMessage(const char* s)
{
    statusmsg = s;
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
    statusmsg = s;
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
