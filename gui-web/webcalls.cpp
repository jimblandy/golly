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

bool refresh_pattern = false;

void UpdatePattern()
{
    refresh_pattern = true;
    // DoFrame in main.cpp will call DrawPattern and reset refresh_pattern to false
}

// -----------------------------------------------------------------------------

void UpdateStatus()
{
    if (fullscreen) return;

    UpdateStatusLines();    // sets status1, status2, status3

    // call JavaScript code to update the status bar info!!!
    
    // remove following lines eventually!!!???
    // clear text area first
    EM_ASM(
        var element = document.getElementById('output');
        element.value = '\0';
    );
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

    // call JavaScript method to update the buttons in the edit bar!!!???
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
    //!!!???
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
    if (generating) paused = true;

    printf("%s\n",msg);//!!!
    //!!!???

    if (generating) paused = false;
}

// -----------------------------------------------------------------------------

void WebFatal(const char* msg)
{
    paused = true;

    printf("%s\n",msg);//!!!
    //!!!???
    exit(1);
}

// -----------------------------------------------------------------------------

bool WebYesNo(const char* query)
{
    std::string answer;
    if (generating) paused = true;

    //!!!???

    if (generating) paused = false;
    return answer == "yes";
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
    //!!!???
    return true;
}

// -----------------------------------------------------------------------------

bool WebGetTextFromClipboard(std::string& text)
{
    text = "";
    //!!!???
    if (text.length() == 0) {
        ErrorMessage("No text in clipboard.");
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
