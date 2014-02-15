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

#include "util.h"       // for linereader

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

// the following JavaScript functions are implemented in jslib.js:

extern "C" {
    extern void jsAlert(const char* msg);
    extern bool jsConfirm(const char* query);
    extern void jsSetStatusBarColor(const char* color);
    extern void jsSetMode(int index);
    extern void jsSetState(int state, int numstates);
    extern void jsSetClipboard(const char* text);
    extern const char* jsGetClipboard();
    extern void jsEnableButton(const char* id, bool enable);
    extern void jsSetInnerHTML(const char* id, const char* text);
    extern void jsMoveToAnchor(const char* anchor);
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
    
    // clear text area first
    EM_ASM( document.getElementById('statusbar').value = '\0'; );
    
    if (curralgo != currlayer->algtype) {
        // algo has changed so change background color of status bar
        curralgo = currlayer->algtype;
        int r = algoinfo[curralgo]->statusrgb.r;
        int g = algoinfo[curralgo]->statusrgb.g;
        int b = algoinfo[curralgo]->statusrgb.b;
        char rgb[32];
        sprintf(rgb, "rgb(%d,%d,%d)", r, g, b);
        jsSetStatusBarColor(rgb);
    }
    
    printf("%s\n", status1.c_str());
    printf("%s\n", status2.c_str());
    printf("%s\n", status3.c_str());
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

void UpdateButtons()
{
    if (fullscreen) return;
    jsEnableButton("reset", currlayer->algo->getGeneration() > currlayer->startgen);
    jsEnableButton("undo", currlayer->undoredo->CanUndo());
    jsEnableButton("redo", currlayer->undoredo->CanRedo());
    jsEnableButton("info", currlayer->currname != "untitled");
}

// -----------------------------------------------------------------------------

void UpdateEditBar()
{
    if (currlayer->drawingstate >= currlayer->algo->NumCellStates()) {
        // this can happen after an algo/rule change
        currlayer->drawingstate = 1;
    }
    
    if (fullscreen) return;
    
    UpdateButtons();

    // show current cursor mode
    jsSetMode(currlayer->touchmode);

    // show current drawing state and update number of options if necessary
    jsSetState(currlayer->drawingstate, currlayer->algo->NumCellStates());
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

void ShowTextFile(const char* filepath)
{
    // check if path ends with .gz or .zip
    if (EndsWith(filepath,".gz") || EndsWith(filepath,".zip")) {
        Warning("Compressed file cannot be displayed.");
        return;
    }
    
    // get contents of given text file and wrap in <pre>...</pre>
    std::string contents = "<pre>";
    FILE* textfile = fopen(filepath, "r");
    if (textfile) {
        // read entire file into contents
        const int MAXLINELEN = 4095;
        char linebuf[MAXLINELEN + 1];
        linereader reader(textfile);
        while (true) {
            if (reader.fgets(linebuf, MAXLINELEN) == 0) break;
            contents += linebuf;
            contents += "\n";
        }
        reader.close();
        // fclose(textfile) has been called
    } else {
        ErrorMessage(filepath);
        Warning("Failed to open text file!");
        return;
    }
    
    // update the contents of the info dialog
    contents += "</pre>";
    jsSetInnerHTML("info_text", contents.c_str());
    
    // display the info dialog
    EM_ASM(
        var infodlg = document.getElementById('info_overlay');
        if (infodlg.style.visibility != 'visible') {
            infodlg.style.visibility = 'visible';
        }
    );
}

// -----------------------------------------------------------------------------

extern "C" {

void CloseInfo()
{
    // close the info dialog
    EM_ASM(
        var infodlg = document.getElementById('info_overlay');
        if (infodlg.style.visibility == 'visible') {
            infodlg.style.visibility = 'hidden';
        }
    );
}

} // extern "C"

// -----------------------------------------------------------------------------

static std::string currpage = "/Help/index.html";

void ShowHelp(const char* filepath)
{
    if (filepath[0] == 0) {
        // use currpage
    } else {
        currpage = filepath;
    }
    
    // if anchor present then strip it off (and call jsMoveToAnchor below)
    std::string anchor = "";
    size_t hashpos = currpage.rfind('#');
    if (hashpos != std::string::npos) {
        anchor = currpage.substr(hashpos+1);
        currpage = currpage.substr(0, hashpos);
    }

    // get contents of currpage
    std::string contents;
    FILE* helpfile = fopen(currpage.c_str(), "r");
    if (helpfile) {
        // read entire file into contents
        const int MAXLINELEN = 4095;
        char linebuf[MAXLINELEN + 1];
        linereader reader(helpfile);
        while (true) {
            if (reader.fgets(linebuf, MAXLINELEN) == 0) break;
            contents += linebuf;
            contents += "\n";
        }
        reader.close();
        // fclose(helpfile) has been called
    } else {
        ErrorMessage(currpage.c_str());
        Warning("Failed to open help file!");
        return;
    }
    
    // update the contents of the help dialog
    jsSetInnerHTML("help_text", contents.c_str());
    
    // display the help dialog and start listening for clicks on links
    EM_ASM(
        var helpdlg = document.getElementById('help_overlay');
        if (helpdlg.style.visibility != 'visible') {
            helpdlg.style.visibility = 'visible';
            
            // this didn't work -- key handler needs to detect if help dlg is open!!!???
            // document.getElementById('close_help').focus();
            
            // note that on_help_click is implemented in shell.html so CloseHelp can remove it
            window.addEventListener('click', on_help_click, false);
        }
    );
    
    if (anchor.length() > 0) {
        jsMoveToAnchor(anchor.c_str());
    }
}

// -----------------------------------------------------------------------------

extern "C" {

void CloseHelp()
{
    // close the help dialog and remove the click event handler
    EM_ASM(
        var helpdlg = document.getElementById('help_overlay');
        if (helpdlg.style.visibility == 'visible') {
            helpdlg.style.visibility = 'hidden';
            window.removeEventListener('click', on_help_click, false);
        }
    );
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

int DoHelpClick(const char* href)
{
    // look for special prefixes used by Golly and return 1 to prevent browser handling link
    std::string link = href;
    
    if (link.find("open:") == 0) {
        // open specified file
        std::string path = link.substr(5);
        FixURLPath(path);
        OpenFile(path.c_str());
        // OpenFile will close help dialog if necessary (might not if .zip file)
        return 1;
    }
    
    if (link.find("rule:") == 0) {
        // switch to specified rule
        std::string newrule = link.substr(5);
        CloseHelp();
        ChangeRule(newrule);
        return 1;
    }
    
    if (link.find("lexpatt:") == 0) {
        // user clicked on pattern in Life Lexicon
        std::string pattern = link.substr(8);
        std::replace(pattern.begin(), pattern.end(), '$', '\n');
        LoadLexiconPattern(pattern);
        // SwitchToPatternTab will call CloseHelp
        return 1;
    }
    
    if (link.find("edit:") == 0) {
        std::string path = link.substr(5);
        // convert path to a full path if necessary
        std::string fullpath = path;
        if (path[0] != '/') {
            fullpath = userdir + fullpath;  //!!!???
        }
        ShowTextFile(fullpath.c_str());
        return 1;
    }
    
    if (link.find("get:") == 0) {
        std::string geturl = link.substr(4);
        // download file specifed in link (possibly relative to a previous full url)
        GetURL(geturl, currpage);  // or use window.location.href???!!!
        return 1;
    }
    
    if (link.find("unzip:") == 0) {
        std::string zippath = link.substr(6);
        FixURLPath(zippath);
        std::string entry = zippath.substr(zippath.rfind(':') + 1);
        zippath = zippath.substr(0, zippath.rfind(':'));
        UnzipFile(zippath, entry);
        return 1;
    }
        
    // no special prefix, so look for file with .zip/rle/lif/mc extension
    std::string path = link;
    path = path.substr(path.rfind('/')+1);
    size_t dotpos = path.rfind('.');
    std::string ext = "";
    if (dotpos != std::string::npos) ext = path.substr(dotpos+1);
    if ( (IsZipFile(path) ||
            strcasecmp(ext.c_str(),"rle") == 0 ||
            strcasecmp(ext.c_str(),"life") == 0 ||
            strcasecmp(ext.c_str(),"mc") == 0)
            // also check for '?' to avoid opening links like ".../detail?name=foo.zip"
            && path.find('?') == std::string::npos) {
        // download file to downloaddir and open it
        path = downloaddir + path;
        if (DownloadFile(link, path)) {
            OpenFile(path.c_str());
        }
        return 1;
    }
    
    // if link doesn't contain ':' then assume it's relative to currpage
    if (link.find(':') == std::string::npos) {
        
        // if link starts with '#' then let browser move to that anchor on current page
        if (link[0] == '#') return 0;
        
        std::string newpage = currpage.substr(0, currpage.rfind('/')+1) + link;
        ShowHelp(newpage.c_str());
        return 1;
    }
    
    // let browser handle this link
    return 0;
}

} // extern "C"

// -----------------------------------------------------------------------------

void SwitchToPatternTab()
{
    CloseHelp();
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
    
    // can't do anything here???!!! will need to use a Web Worker???!!!
    // note that glfwPollEvents() does nothing (see emscripten/src/library_glfw.js)
    // glfwPollEvents();
}

// -----------------------------------------------------------------------------

bool WebDownloadFile(const std::string& url, const std::string& filepath)
{
    //!!!???
    return false;
}
