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

#include <string>       // for std::string
#include <list>         // for std::list
#include <set>          // for std::set
#include <algorithm>    // for std::count

#include "utils.h"      // for Warning, etc
#include "algos.h"      // for InitAlgorithms
#include "prefs.h"      // for GetPrefs, SavePrefs, userdir, etc
#include "layer.h"      // for AddLayer, ResizeLayers, currlayer
#include "control.h"    // for SetMinimumStepExponent
#include "file.h"       // for NewPattern
#include "view.h"       // for widescreen, fullscreen, TouchBegan, etc
#include "status.h"     // for UpdateStatusLines, ClearMessage, etc
#include "undo.h"       // for ClearUndoRedo
#include "webcalls.h"

#include <emscripten/emscripten.h>      // for EM_ASM

// -----------------------------------------------------------------------------

// the following JavaScript routines are implemented in jslib.js:

extern "C" {
    extern void jsAlert(const char* msg);
    extern bool jsConfirm(const char* query);
    extern void jsSetStatusBarColor(const char* color);
    extern void jsSetAlgo(int index);
    extern void jsSetMode(int index);
    extern void jsSetState(int state);
    extern void jsSetClipboard(const char* text);
    extern const char* jsGetClipboard();
}

// -----------------------------------------------------------------------------

bool refresh_pattern = false;

void UpdatePattern()
{
    refresh_pattern = true;
    // DoFrame in main.cpp will call DrawPattern and reset refresh_pattern to false
}

// -----------------------------------------------------------------------------

static int curralgo = -1;

void UpdateStatus()
{
    if (fullscreen) return;

    UpdateStatusLines();    // sets status1, status2, status3

    // use a nicer looking status bar eventually???!!!
    
    // clear text area first
    EM_ASM( document.getElementById('statusbar').value = '\0'; );
    
    if (curralgo != currlayer->algtype) {
        // algo has changed so change bg color of status bar
        curralgo = currlayer->algtype;
        int r = algoinfo[curralgo]->statusrgb.r;
        int g = algoinfo[curralgo]->statusrgb.g;
        int b = algoinfo[curralgo]->statusrgb.b;
        char rgb[32];
        sprintf(rgb, "rgb(%d,%d,%d)", r, g, b);
        jsSetStatusBarColor(rgb);
        // also change selected algo in dropdown list
        jsSetAlgo(curralgo);
    }
    
    printf("%s\n",status1.c_str());
    printf("%s\n",status2.c_str());
    printf("%s\n",status3.c_str());
}

// -----------------------------------------------------------------------------

static bool paused = false;     // generating has been temporarily stopped?

void PauseGenerating()
{
    if (generating) {
        StopGenerating();
        // generating is now false
        paused = true;
    }
}

// -----------------------------------------------------------------------------

void ResumeGenerating()
{
    if (paused) {
        StartGenerating();
        // generating is probably true (false if pattern is empty)
        paused = false;
    }
}

// -----------------------------------------------------------------------------

std::string GetRuleName(const std::string& rule)
{
    std::string result = "";
    // not yet implemented!!!
    // maybe we should create rule.h and rule.cpp in gui-common???
    return result;
}

// -----------------------------------------------------------------------------

void UpdateEditBar()
{
    if (currlayer->drawingstate >= currlayer->algo->NumCellStates()) {
        // this can happen after an algo/rule change
        currlayer->drawingstate = 1;
    }
    
    if (fullscreen) return;
    
    // use JavaScript code to update the Undo/Redo buttons
    //!!! undobutton.setEnabled(CanUndo());
    //!!! redobutton.setEnabled(CanRedo());

    // show current cursor mode
    jsSetMode(currlayer->touchmode);

    // show current drawing state
    jsSetState(currlayer->drawingstate);
}

// -----------------------------------------------------------------------------

void BeginProgress(const char* title)
{
    //!!!???
}

// -----------------------------------------------------------------------------

bool AbortProgress(double fraction_done, const char* message)
{
    bool result = false;

    //!!!???

    return result;
}

// -----------------------------------------------------------------------------

void EndProgress()
{
    //!!!???
}

// -----------------------------------------------------------------------------

void SwitchToPatternTab()
{
    // no need to do anything???!!!
    // or maybe just close/remove help dialog???
}

// -----------------------------------------------------------------------------

void ShowTextFile(const char* filepath)
{
    // display contents of text file!!!???
}

// -----------------------------------------------------------------------------

void ShowHelp(const char* filepath)
{
    // display html file!!!???
}

// -----------------------------------------------------------------------------

void WebWarning(const char* msg)
{
    jsAlert(msg);
}

// -----------------------------------------------------------------------------

void WebFatal(const char* msg)
{
    jsAlert(msg);
    exit(1);        // no need to do anything else???
}

// -----------------------------------------------------------------------------

bool WebYesNo(const char* query)
{
    return jsConfirm(query);
}

// -----------------------------------------------------------------------------

void WebBeep()
{
    //!!!???
}

// -----------------------------------------------------------------------------

void WebRemoveFile(const std::string& filepath)
{
    //!!!???
}

// -----------------------------------------------------------------------------

bool WebMoveFile(const std::string& inpath, const std::string& outpath)
{
    //!!!???
    return false;
}

// -----------------------------------------------------------------------------

void WebFixURLPath(std::string& path)
{
    // replace "%..." with suitable chars for a file path (eg. %20 is changed to space)

    // no need to do anything!!!???
}

// -----------------------------------------------------------------------------

bool WebCopyTextToClipboard(const char* text)
{
    jsSetClipboard(text);
    return true;
}

// -----------------------------------------------------------------------------

bool WebGetTextFromClipboard(std::string& text)
{
    text = jsGetClipboard();
    if (text.length() == 0) {
        ErrorMessage("There is no text in the clipboard.");
        return false;
    } else {
        return true;
    }
}

// -----------------------------------------------------------------------------

void WebCheckEvents()
{
    // event_checker is > 0 in here (see gui-common/utils.cpp)
    //!!!??? glfwPollEvents();
}

// -----------------------------------------------------------------------------

bool WebDownloadFile(const std::string& url, const std::string& filepath)
{
    //!!!???
    return false;
}
