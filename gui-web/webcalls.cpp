// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include <vector>       // for std::vector
#include <string>       // for std::string
#include <list>         // for std::list
#include <set>          // for std::set
#include <algorithm>    // for std::count

#include "util.h"       // for linereader

#include "utils.h"      // for Warning, etc
#include "algos.h"      // for InitAlgorithms
#include "prefs.h"      // for GetPrefs, SavePrefs, userdir, etc
#include "layer.h"      // for AddLayer, ResizeLayers, currlayer
#include "control.h"    // for SetMinimumStepExponent, etc
#include "file.h"       // for NewPattern, OpenFile, LoadRule, etc
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
    extern void jsSetBackgroundColor(const char* id, const char* color);
    extern void jsSetStatus(const char* line1, const char* line2, const char* line3);
    extern void jsSetMode(int index);
    extern void jsSetState(int state, int numstates);
    extern void jsSetClipboard(const char* text);
    extern const char* jsGetClipboard();
    extern void jsEnableButton(const char* id, bool enable);
    extern void jsEnableImgButton(const char* id, bool enable);
    extern void jsSetInnerHTML(const char* id, const char* text);
    extern void jsSetCheckBox(const char* id, bool flag);
    extern void jsMoveToAnchor(const char* anchor);
    extern void jsSetScrollTop(const char* id, int pos);
    extern int jsGetScrollTop(const char* id);
    extern bool jsElementIsVisible(const char* id);
    extern void jsDownloadFile(const char* url, const char* filepath);
    extern void jsBeep();
    extern void jsDeleteFile(const char* filepath);
    extern bool jsMoveFile(const char* inpath, const char* outpath);
    extern void jsBeginProgress(const char* title);
    extern bool jsAbortProgress(int percentage);
    extern void jsEndProgress();
    extern void jsCancelProgress();
    extern void jsStoreRule(const char* rulepath);
    extern const char* jsGetSaveName(const char* currname);
    extern void jsSaveFile(const char* filename);
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
    //!!!??? EM_ASM( document.getElementById('statusbar').value = '\0'; );
    
    if (curralgo != currlayer->algtype) {
        // algo has changed so change background color of status bar
        curralgo = currlayer->algtype;
        int r = algoinfo[curralgo]->statusrgb.r;
        int g = algoinfo[curralgo]->statusrgb.g;
        int b = algoinfo[curralgo]->statusrgb.b;
        char rgb[32];
        sprintf(rgb, "rgb(%d,%d,%d)", r, g, b);
        jsSetBackgroundColor("statusbar", rgb);
    }
    
    jsSetStatus(status1.c_str(), status2.c_str(), status3.c_str());
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
    // (Set Rule dialog would need to let users create/delete named rules
    // and save them in GollyPrefs)
    return result;
}

// -----------------------------------------------------------------------------

void UpdateButtons()
{
    if (fullscreen) return;
    jsEnableImgButton("reset", currlayer->algo->getGeneration() > currlayer->startgen);
    jsEnableImgButton("undo", currlayer->undoredo->CanUndo());
    jsEnableImgButton("redo", currlayer->undoredo->CanRedo());
    jsEnableImgButton("info", currlayer->currname != "untitled");
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

    // update check boxes
    jsSetCheckBox("toggle_icons", showicons);
    jsSetCheckBox("toggle_autofit", currlayer->autofit);
}

// -----------------------------------------------------------------------------

static int progresscount = 0;   // if > 0 then BeginProgress has been called

void BeginProgress(const char* title)
{
    if (progresscount == 0) {
        jsBeginProgress(title);
    }
    progresscount++;    // handles nested calls
}

// -----------------------------------------------------------------------------

bool AbortProgress(double fraction_done, const char* message)
{
    if (progresscount <= 0) Fatal("Bug detected in AbortProgress!");
    // don't use message (empty string)
    return jsAbortProgress(int(fraction_done*100));
}

// -----------------------------------------------------------------------------

void EndProgress()
{
    if (progresscount <= 0) Fatal("Bug detected in EndProgress!");
    progresscount--;
    if (progresscount == 0) {
        jsEndProgress();
    }
}

// -----------------------------------------------------------------------------

extern "C" {

void CancelProgress()
{
    // called if user hits Cancel button in progress dialog
    jsCancelProgress();
}

} // extern "C"

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
        contents += "Failed to open text file!\n";
        contents += filepath;
    }
    
    // update the contents of the info dialog
    contents += "</pre>";
    jsSetInnerHTML("info_text", contents.c_str());

    // display the info dialog
    EM_ASM( document.getElementById('info_overlay').style.visibility = 'visible'; );
}

// -----------------------------------------------------------------------------

extern "C" {

void CloseInfo()
{
    // close the info dialog
    EM_ASM( document.getElementById('info_overlay').style.visibility = 'hidden'; );
}

} // extern "C"

// -----------------------------------------------------------------------------

static const char* contents_page = "/Help/index.html";
static std::string currpage = contents_page;
static std::vector<std::string> page_history;
static std::vector<int> page_scroll;
static unsigned int page_index = 0;
static bool shifting_history = false;   // in HelpBack or HelpNext?

// -----------------------------------------------------------------------------

static bool CanGoBack()
{
    return page_index > 0;
}

// -----------------------------------------------------------------------------

static bool CanGoNext()
{
    return page_history.size() > 1 && page_index < (page_history.size() - 1);
}

// -----------------------------------------------------------------------------

static void UpdateHelpButtons()
{
    jsEnableButton("help_back", CanGoBack());
    jsEnableButton("help_next", CanGoNext());
    jsEnableButton("help_contents", currpage != contents_page);
}

// -----------------------------------------------------------------------------

static void DisplayHelpDialog()
{
    // display the help dialog and start listening for clicks on links
    EM_ASM(
        var helpdlg = document.getElementById('help_overlay');
        if (helpdlg.style.visibility != 'visible') {
            helpdlg.style.visibility = 'visible';
            // note that on_help_click is implemented in shell.html so CloseHelp can remove it
            window.addEventListener('click', on_help_click, false);
        }
    );
}

// -----------------------------------------------------------------------------

void ShowHelp(const char* filepath)
{
    if (filepath[0] == 0) {
        // this only happens if user hits 'h' key or clicks '?' button
        if (page_history.size() > 0) {
            // just need to show the help dialog
            DisplayHelpDialog();
            return;
        }
        // else this is very 1st call and currpage = contents_page
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
    
    if (!shifting_history) {
        // user didn't hit back/next button
        bool help_visible = jsElementIsVisible("help_overlay");
        if (!help_visible && page_history.size() > 0 && currpage == page_history[page_index]) {
            // same page requested so just need to show the help dialog
            DisplayHelpDialog();
            return;
        }
        if (help_visible) {
            // remember scroll position of current page
            page_scroll[page_index] = jsGetScrollTop("help_text");
            // remove any following pages
            while (CanGoNext()) {
                page_history.pop_back();
                page_scroll.pop_back();
            }
        }
        page_history.push_back(currpage);
        page_scroll.push_back(0);
        page_index = page_history.size() - 1;
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
        contents += "<p>Failed to open help file!<br>";
        contents += currpage;
    }
    
    // update the contents of the help dialog
    jsSetInnerHTML("help_text", contents.c_str());

    // if contents has 'body bgcolor="..."' then use that color, otherwise use white
    std::string bgcolor = "#FFF";
    size_t startpos = contents.find("body bgcolor=\"");
    if (startpos != std::string::npos) {
        startpos += 14;
        size_t len = 0;
        while (contents[startpos+len] != '"' && len < 16) len++;  // allow for "rgb(255,255,255)"
        bgcolor = contents.substr(startpos, len);
    }
    jsSetBackgroundColor("help_text", bgcolor.c_str());

    UpdateHelpButtons();
    DisplayHelpDialog();
        
    if (anchor.length() > 0) {
        jsMoveToAnchor(anchor.c_str());
    } else {
        jsSetScrollTop("help_text", page_scroll[page_index]);
    }
}

// -----------------------------------------------------------------------------

extern "C" {

void HelpBack()
{
    if (CanGoBack()) {
        // remember scroll position of current page and go to previous page
        page_scroll[page_index] = jsGetScrollTop("help_text");
        page_index--;
        shifting_history = true;
        ShowHelp(page_history[page_index].c_str());
        shifting_history = false;
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void HelpNext()
{
    if (CanGoNext()) {
        // remember scroll position of current page and go to next page
        page_scroll[page_index] = jsGetScrollTop("help_text");
        page_index++;
        shifting_history = true;
        ShowHelp(page_history[page_index].c_str());
        shifting_history = false;
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void HelpContents()
{
    // probably best to clear history
    page_history.clear();
    page_scroll.clear();
    page_index = 0;

    ShowHelp(contents_page);
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void CloseHelp()
{
    if (jsElementIsVisible("help_overlay")) {
        // remember scroll position of current page for later use
        page_scroll[page_index] = jsGetScrollTop("help_text");
        
        // close the help dialog and remove the click event handler
        EM_ASM(
            document.getElementById('help_overlay').style.visibility = 'hidden';
            window.removeEventListener('click', on_help_click, false);
        );
    }
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
        SwitchToPatternTab();   // calls CloseHelp
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
            fullpath = userdir + fullpath;
        }
        ShowTextFile(fullpath.c_str());
        return 1;
    }
    
    if (link.find("get:") == 0) {
        std::string geturl = link.substr(4);
        // download file specifed in link (possibly relative to a previous full url)
        GetURL(geturl, currpage);
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
    
    // if link doesn't contain ':' then assume it's relative to currpage
    if (link.find(':') == std::string::npos) {
        
        // if link starts with '#' then move to that anchor on current page
        if (link[0] == '#') {
            std::string newpage = currpage + link;
            ShowHelp(newpage.c_str());
            return 1;
        }
        
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
    jsBeep();
}

// -----------------------------------------------------------------------------

void WebRemoveFile(const std::string& filepath)
{
    jsDeleteFile(filepath.c_str());
}

// -----------------------------------------------------------------------------

bool WebMoveFile(const std::string& inpath, const std::string& outpath)
{
    return jsMoveFile(inpath.c_str(), outpath.c_str());
}

// -----------------------------------------------------------------------------

void WebFixURLPath(std::string& path)
{
    // no need to do anything
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

bool PatternSaved(std::string& filename)
{
    // append default extension if not supplied
    size_t dotpos = filename.find('.');
    if (dotpos == std::string::npos) {
        if (currlayer->algo->hyperCapable()) {
            // macrocell format is best for hash-based algos
            filename += ".mc";
        } else {
            filename += ".rle";
        }
    } else {
        // check that the supplied extension is valid
        if (currlayer->algo->hyperCapable()) {
            if (!EndsWith(filename,".mc") && !EndsWith(filename,".mc.gz") &&
                !EndsWith(filename,".rle") && !EndsWith(filename,".rle.gz")) {
                Warning("File extension must be .mc or .mc.gz or .rle or .rle.gz.");
                return false;
            }
        } else {
            if (!EndsWith(filename,".rle") && !EndsWith(filename,".rle.gz")) {
                Warning("File extension must be .rle or .rle.gz.");
                return false;
            }
        }
    }
    
    pattern_format format = XRLE_format;
    if (EndsWith(filename,".mc") || EndsWith(filename,".mc.gz")) format = MC_format;
    
    output_compression compression = no_compression;
    if (EndsWith(filename,".gz")) compression = gzip_compression;

    return SavePattern(filename, format, compression);
}

// -----------------------------------------------------------------------------

bool WebSaveChanges()
{
    std::string query = "Save your changes?";
    if (numlayers > 1) {
        // make it clear which layer we're asking about
        query = "Save your changes to this layer: \"",
        query += currlayer->currname;
        query += "\"?";
    }
    if (jsConfirm(query.c_str())) {
        // prompt user for name of file in which to save pattern
        // (must be a blocking dialog so we can't use our custom save dialog)
        std::string filename = jsGetSaveName(currlayer->currname.c_str());
        
        // filename is empty if user hit Cancel, so don't continue
        if (filename.length() == 0) return false;
        
        if (PatternSaved(filename)) {
            ClearMessage();
            // filename successfully created (in virtual file system),
            // so download it to user's computer and continue
            jsSaveFile(filename.c_str());
            return true;
        } else {
            return false;   // don't continue
        }
    } else {
        // user hit Cancel so don't save changes (but continue)
        return true;
    }
}

// -----------------------------------------------------------------------------

bool WebDownloadFile(const std::string& url, const std::string& filepath)
{
    // jsDownloadFile does an asynchronous file transfer and will call FileCreated()
    // only if filepath is successfully created
    jsDownloadFile(url.c_str(), filepath.c_str());
    
    // we must return false so GetURL won't proceed beyond the DownloadFile call
    return false;
}

// -----------------------------------------------------------------------------

extern "C" {

void FileCreated(const char* filepath)
{
    // following code matches that in gui-common/file.cpp after DownloadFile
    // returns false in GetURL
    
    std::string filename = GetBaseName(filepath);
    
    if (IsHTMLFile(filename)) {
        ShowHelp(filepath);

    } else if (IsRuleFile(filename)) {
        // load corresponding rule
        SwitchToPatternTab();
        LoadRule(filename.substr(0, filename.rfind('.')));
        // ensure the .rule file persists beyond the current session
        CopyRuleToLocalStorage(filepath);

    } else if (IsTextFile(filename)) {
        ShowTextFile(filepath);

    } else if (IsScriptFile(filename)) {
        Warning("This version of Golly cannot run scripts.");

    } else {
        // assume it's a pattern/zip file, so open it
        OpenFile(filepath);
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

void CopyRuleToLocalStorage(const char* rulepath)
{
    jsStoreRule(rulepath);
}

// -----------------------------------------------------------------------------

void WebCheckEvents()
{
    // event_checker is > 0 in here (see gui-common/utils.cpp)
    
    // it looks like JavaScript doesn't have any access to the browser's event queue
    // so we can't do anything here???!!! we'll need to use a Web Worker???!!!
    // note that glfwPollEvents() does nothing (see emscripten/src/library_glfw.js)
    // glfwPollEvents();
}
