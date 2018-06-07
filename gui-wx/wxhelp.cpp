// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
    #include "wx/wx.h"     // for all others include the necessary headers
#endif

#include "wx/wxhtml.h"          // for wxHtmlWindow
#include "wx/file.h"            // for wxFile
#include "wx/protocol/http.h"   // for wxHTTP
#include "wx/wfstream.h"        // for wxFileOutputStream, wxFileInputStream
#include "wx/zipstrm.h"         // for wxZipInputStream

#include "lifealgo.h"           // for lifealgo class
#include "ruleloaderalgo.h"     // for noTABLEorTREE

#include "wxgolly.h"       // for wxGetApp, mainptr, viewptr
#include "wxmain.h"        // for mainptr->...
#include "wxview.h"        // for viewptr->...
#include "wxutils.h"       // for Warning, BeginProgress, etc
#include "wxprefs.h"       // for GetShortcutTable, helpfontsize, gollydir, etc
#include "wxscript.h"      // for inscript, pass_file_events, etc
#include "wxlayer.h"       // for numlayers, GetLayer, etc
#include "wxalgos.h"       // for QLIFE_ALGO
#include "wxhelp.h"

// -----------------------------------------------------------------------------

// define a modeless help window:

class HelpFrame : public wxFrame
{
public:
    HelpFrame();
    
    void SetStatus(const wxString& text) {
        status->SetLabel(text);
    }
    
    bool infront;     // help window is active?
    
private:
    // ids for buttons in help window (see also wxID_CLOSE)
    enum {
        ID_BACK_BUTT = wxID_HIGHEST + 1,
        ID_FORWARD_BUTT,
        ID_CONTENTS_BUTT
    };
    
    // event handlers
    void OnActivate(wxActivateEvent& event);
    void OnBackButton(wxCommandEvent& event);
    void OnForwardButton(wxCommandEvent& event);
    void OnContentsButton(wxCommandEvent& event);
    void OnCloseButton(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);
    
    wxStaticText* status;   // status line at bottom of help window
    
    // any class wishing to process wxWidgets events must use this macro
    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(HelpFrame, wxFrame)
EVT_ACTIVATE   (                    HelpFrame::OnActivate)
EVT_BUTTON     (ID_BACK_BUTT,       HelpFrame::OnBackButton)
EVT_BUTTON     (ID_FORWARD_BUTT,    HelpFrame::OnForwardButton)
EVT_BUTTON     (ID_CONTENTS_BUTT,   HelpFrame::OnContentsButton)
EVT_BUTTON     (wxID_CLOSE,         HelpFrame::OnCloseButton)
EVT_CLOSE      (                    HelpFrame::OnClose)
END_EVENT_TABLE()

// -----------------------------------------------------------------------------

// define a child window for displaying html info:

class HtmlView : public wxHtmlWindow
{
public:
    HtmlView(wxWindow* parent, wxWindowID id, const wxPoint& pos,
             const wxSize& size, long style)
    : wxHtmlWindow(parent, id, pos, size, style) {
        editlink = false;
        linkrect = wxRect(0,0,0,0);
    }
    
    virtual void OnLinkClicked(const wxHtmlLinkInfo& link);
    virtual void OnCellMouseHover(wxHtmlCell* cell, wxCoord x, wxCoord y);
    
    void ClearStatus();  // clear help window's status line
    
    void SetFontSizes(int size);
    void ChangeFontSizes(int size);
    
    void CheckAndLoad(const wxString& filepath);
    
    void StartTimer() {
        htmltimer = new wxTimer(this, wxID_ANY);
        // call OnTimer 10 times per sec
        htmltimer->Start(100, wxTIMER_CONTINUOUS);
    }
    
    void StopTimer() {
        htmltimer->Stop();
        delete htmltimer;
    }
    
    bool editlink;       // open clicked file in editor?
    bool canreload;      // can OnSize call CheckAndLoad?
    
private:
#ifdef __WXMSW__
    // see HtmlView::OnKeyUp for why we do this
    void OnKeyUp(wxKeyEvent& event);
#else
    void OnKeyDown(wxKeyEvent& event);
#endif
    
    void OnChar(wxKeyEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnMouseMotion(wxMouseEvent& event);
    void OnMouseLeave(wxMouseEvent& event);
    void OnMouseDown(wxMouseEvent& event);
    void OnTimer(wxTimerEvent& event);
    
    wxTimer* htmltimer;
    wxRect linkrect;     // rect for cell containing link
    
    // any class wishing to process wxWidgets events must use this macro
    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(HtmlView, wxHtmlWindow)
#ifdef __WXMSW__
// see HtmlView::OnKeyUp for why we do this
EVT_KEY_UP        (HtmlView::OnKeyUp)
#else
EVT_KEY_DOWN      (HtmlView::OnKeyDown)
#endif
EVT_CHAR          (HtmlView::OnChar)
EVT_SIZE          (HtmlView::OnSize)
EVT_MOTION        (HtmlView::OnMouseMotion)
EVT_ENTER_WINDOW  (HtmlView::OnMouseMotion)
EVT_LEAVE_WINDOW  (HtmlView::OnMouseLeave)
EVT_LEFT_DOWN     (HtmlView::OnMouseDown)
EVT_RIGHT_DOWN    (HtmlView::OnMouseDown)
EVT_TIMER         (wxID_ANY, HtmlView::OnTimer)
END_EVENT_TABLE()

// -----------------------------------------------------------------------------

HelpFrame* helpptr = NULL;    // help window
HtmlView* htmlwin = NULL;     // html child window

wxButton* backbutt;           // back button
wxButton* forwbutt;           // forwards button
wxButton* contbutt;           // Contents button

long whenactive;              // when help window became active (elapsed millisecs)

const wxString helphome = _("Help/index.html");    // contents page
wxString currhelp = helphome;                      // current help file
const wxString lexicon_name = _("lexicon");        // name of lexicon layer

int lexlayer;                 // index of existing lexicon layer (-ve if not present)
wxString lexpattern;          // lexicon pattern data

// prefix of most recent full URL in a html get ("get:http://.../foo.html")
// to allow later relative gets ("get:foo.rle")
wxString urlprefix = wxEmptyString;
const wxString HTML_PREFIX = _("GET---");          // prepended to html filename

// -----------------------------------------------------------------------------

wxFrame* GetHelpFrame() {
    return helpptr;
}

// -----------------------------------------------------------------------------

// create the help window
HelpFrame::HelpFrame()
: wxFrame(NULL, wxID_ANY, _(""), wxPoint(helpx,helpy), wxSize(helpwd,helpht))
{
    wxGetApp().SetFrameIcon(this);
    
#ifdef __WXMSW__
    // use current theme's background colour
    SetBackgroundColour(wxNullColour);
#endif
    
    htmlwin = new HtmlView(this, wxID_ANY,
                           // specify small size to avoid clipping scroll bar on resize
                           wxDefaultPosition, wxSize(30,30),
                           wxHW_DEFAULT_STYLE | wxSUNKEN_BORDER);
    htmlwin->StartTimer();
    htmlwin->SetBorders(4);
    htmlwin->SetFontSizes(helpfontsize);
    
    wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* hbox = new wxBoxSizer(wxHORIZONTAL);
    
    backbutt = new wxButton(this, ID_BACK_BUTT, _("<"),
                            wxDefaultPosition, wxSize(40,wxDefaultCoord));
    hbox->Add(backbutt, 0, wxALL | wxALIGN_LEFT, 10);
    
    forwbutt = new wxButton(this, ID_FORWARD_BUTT, _(">"),
                            wxDefaultPosition, wxSize(40,wxDefaultCoord));
    hbox->Add(forwbutt, 0, wxTOP | wxBOTTOM | wxALIGN_LEFT, 10);
    
    contbutt = new wxButton(this, ID_CONTENTS_BUTT, _("Contents"));
    hbox->Add(contbutt, 0, wxALL | wxALIGN_LEFT, 10);
    
    hbox->AddStretchSpacer(1);
    
    wxButton* closebutt = new wxButton(this, wxID_CLOSE, _("Close"));
    closebutt->SetDefault();
    hbox->Add(closebutt, 0, wxALL, 10);
    
    vbox->Add(hbox, 0, wxALL | wxEXPAND | wxALIGN_TOP, 0);
    
    vbox->Add(htmlwin, 1, wxLEFT | wxRIGHT | wxEXPAND | wxALIGN_TOP, 10);
    
    status = new wxStaticText(this, wxID_STATIC, wxEmptyString);
#ifdef __WXMAC__
    status->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
#endif
    wxBoxSizer* statbox = new wxBoxSizer(wxHORIZONTAL);
    statbox->Add(status);
    vbox->AddSpacer(2);
    vbox->Add(statbox, 0, wxLEFT | wxALIGN_LEFT, 10);
    vbox->AddSpacer(4);
    
    SetMinSize(wxSize(minhelpwd, minhelpht));
    SetSizer(vbox);
    
    // expand sizer now to avoid seeing small htmlwin and buttons in top left corner
    vbox->SetDimension(0, 0, helpwd, helpht);
}

// -----------------------------------------------------------------------------

void UpdateHelpButtons()
{
    backbutt->Enable( htmlwin->HistoryCanBack() );
    forwbutt->Enable( htmlwin->HistoryCanForward() );
    // check for title used in Help/index.html
    contbutt->Enable( htmlwin->GetOpenedPageTitle() != _("Golly Help: Contents") );
    
    wxString location = htmlwin->GetOpenedPage();
    if ( !location.IsEmpty() ) {

        if (location.StartsWith(wxT("file:"))) {
            // this happens in wx 3.1.0, so convert location from URL to file path
            wxFileName fname = wxFileSystem::URLToFileName(location);
            location = fname.GetFullPath();
            #ifdef __WXMSW__
                location.Replace(wxT("\\"), wxT("/"));
            #endif
        }
        
        // avoid bug in wx 3.1.0
        location.Replace(wxT("%20"), wxT(" "));
        location.Replace(wxT("%23"), wxT("#"));
        
        // set currhelp so user can close help window and then open same page
        currhelp = location;
        
        // if filename starts with HTML_PREFIX then set urlprefix to corresponding
        // url so any later relative "get:foo.rle" links will work
        wxString filename = location.AfterLast('/');
        if (filename.StartsWith(HTML_PREFIX)) {
            // replace HTML_PREFIX with "http://" and convert spaces to '/'
            // (ie. reverse what we did in GetURL)
            urlprefix = filename;
            urlprefix.Replace(HTML_PREFIX, wxT("http://"), false);   // do once
            urlprefix.Replace(wxT(" "), wxT("/"));
            urlprefix = urlprefix.BeforeLast('/');
            urlprefix += wxT("/");     // must end in slash
        }
    }
    
    htmlwin->ClearStatus();
    htmlwin->SetFocus();       // for keyboard shortcuts
}

// -----------------------------------------------------------------------------

void ShowHelp(const wxString& filepath)
{
    // display given html file in help window
    if (helpptr) {
        // help window exists so bring it to front and display given file
        if (!filepath.IsEmpty()) {
            htmlwin->CheckAndLoad(filepath);
            UpdateHelpButtons();
        }
        helpptr->Raise();
        
    } else {
        helpptr = new HelpFrame();
        if (helpptr == NULL) {
            Warning(_("Could not create help window!"));
            return;
        }
        
        // assume our .html files contain a <title> tag
        htmlwin->SetRelatedFrame(helpptr, _("%s"));
        
        if (!filepath.IsEmpty()) {
            htmlwin->CheckAndLoad(filepath);
        } else {
            htmlwin->CheckAndLoad(currhelp);
        }
        
        // prevent HtmlView::OnSize calling CheckAndLoad twice
        htmlwin->canreload = false;
        
        helpptr->Show(true);
        UpdateHelpButtons();    // must be after Show to avoid scroll bar appearing on Mac
        
        // allow HtmlView::OnSize to call CheckAndLoad if window is resized
        htmlwin->canreload = true;
    }
    whenactive = 0;
}

// -----------------------------------------------------------------------------

void HelpFrame::OnActivate(wxActivateEvent& event)
{
    // IsActive() is not always reliable so we set infront flag
    infront = event.GetActive();
    if (infront) {
        // help window is being activated
        whenactive = stopwatch->Time();
        
        // ensure correct menu items, esp after help window
        // is clicked while app is in background
        mainptr->UpdateMenuItems();
    }
    event.Skip();
}

// -----------------------------------------------------------------------------

void HelpFrame::OnBackButton(wxCommandEvent& WXUNUSED(event))
{
    if ( htmlwin->HistoryBack() ) {
        UpdateHelpButtons();
    } else {
        Beep();
    }
}

// -----------------------------------------------------------------------------

void HelpFrame::OnForwardButton(wxCommandEvent& WXUNUSED(event))
{
    if ( htmlwin->HistoryForward() ) {
        UpdateHelpButtons();
    } else {
        Beep();
    }
}

// -----------------------------------------------------------------------------

void HelpFrame::OnContentsButton(wxCommandEvent& WXUNUSED(event))
{
    ShowHelp(helphome);
}

// -----------------------------------------------------------------------------

void HelpFrame::OnCloseButton(wxCommandEvent& WXUNUSED(event))
{
    Close(true);
}

// -----------------------------------------------------------------------------

void HelpFrame::OnClose(wxCloseEvent& WXUNUSED(event))
{
#ifdef __WXMSW__
    if (!IsIconized()) {
#endif
        // save current location and size for later use in SavePrefs
        wxRect r = GetRect();
        helpx = r.x;
        helpy = r.y;
        helpwd = r.width;
        helpht = r.height;
#ifdef __WXMSW__
    }
#endif
    
    // stop htmltimer immediately (if we do it in ~HtmlView dtor then timer
    // only stops when app becomes idle)
    htmlwin->StopTimer();
    
    Destroy();        // also deletes all child windows (buttons, etc)
    helpptr = NULL;
}

// -----------------------------------------------------------------------------

void LoadRule(const wxString& rulestring, bool fromfile)
{
    wxString oldrule = wxString(currlayer->algo->getrule(),wxConvLocal);
    int oldmaxstate = currlayer->algo->NumCellStates() - 1;
    const char* err;
    
    // selection might change if grid becomes smaller,
    // so save current selection for RememberRuleChange/RememberAlgoChange
    viewptr->SaveCurrentSelection();
    
    mainptr->Raise();
    
    if (mainptr->generating) {
        Warning(_("Cannot change rule while generating a pattern."));
        return;
    } else if (inscript) {
        Warning(_("Cannot change rule while a script is running."));
        return;
    }
    
    if (fromfile) {
        // load given rule from a .rule file
        
        // InitAlgorithms ensures the RuleLoader algo is the last algo
        int rule_loader_algo = NumAlgos() - 1;
        
        if (currlayer->algtype == rule_loader_algo) {
            // RuleLoader is current algo so no need to switch
            err = currlayer->algo->setrule( rulestring.mb_str(wxConvLocal) );
        } else {
            // switch to RuleLoader algo
            lifealgo* tempalgo = CreateNewUniverse(rule_loader_algo);
            err = tempalgo->setrule( rulestring.mb_str(wxConvLocal) );
            delete tempalgo;
            if (!err) {
                // change the current algorithm and switch to the new rule
                mainptr->ChangeAlgorithm(rule_loader_algo, rulestring);
                if (rule_loader_algo != currlayer->algtype) {
                    RestoreRule(oldrule);
                    Warning(_("Algorithm could not be changed (pattern is too big to convert)."));
                } else {
                    mainptr->SetWindowTitle(wxEmptyString);
                    mainptr->UpdateEverything();
                }
                return;
            }
        }
        
        if (err) {
            // RuleLoader algo found some sort of error
            if (strcmp(err, noTABLEorTREE) == 0) {
                // .rule file has no TABLE or TREE section but it might be used
                // to override a built-in rule, so try each algo
                wxString temprule = rulestring;
                temprule.Replace(wxT("_"), wxT("/"));   // eg. convert B3_S23 to B3/S23
                for (int i = 0; i < NumAlgos(); i++) {
                    lifealgo* tempalgo = CreateNewUniverse(i);
                    err = tempalgo->setrule( temprule.mb_str(wxConvLocal) );
                    delete tempalgo;
                    if (!err) {
                        // change the current algorithm and switch to the new rule
                        mainptr->ChangeAlgorithm(i, temprule);
                        if (i != currlayer->algtype) {
                            RestoreRule(oldrule);
                            Warning(_("Algorithm could not be changed (pattern is too big to convert)."));
                        } else {
                            mainptr->SetWindowTitle(wxEmptyString);
                            mainptr->UpdateEverything();
                        }
                        return;
                    }
                }
            }
            RestoreRule(oldrule);
            wxString msg = _("The rule file is not valid:\n") + rulestring;
            msg += _("\n\nThe error message:\n") + wxString(err,wxConvLocal);
            Warning(msg);
            return;
        }
    
    } else {
        // fromfile is false, so switch to rule given in a "rule:" link
        
        err = currlayer->algo->setrule( rulestring.mb_str(wxConvLocal) );
        if (err) {
            // try to find another algorithm that supports the given rule
            for (int i = 0; i < NumAlgos(); i++) {
                if (i != currlayer->algtype) {
                    lifealgo* tempalgo = CreateNewUniverse(i);
                    err = tempalgo->setrule( rulestring.mb_str(wxConvLocal) );
                    delete tempalgo;
                    if (!err) {
                        // change the current algorithm and switch to the new rule
                        mainptr->ChangeAlgorithm(i, rulestring);
                        if (i != currlayer->algtype) {
                            RestoreRule(oldrule);
                            Warning(_("Algorithm could not be changed (pattern is too big to convert)."));
                        } else {
                            mainptr->SetWindowTitle(wxEmptyString);
                            mainptr->UpdateEverything();
                        }
                        return;
                    }
                }
            }
            RestoreRule(oldrule);
            Warning(_("Given rule is not valid in any algorithm:\n") + rulestring);
            return;
        }
    }
    
    wxString newrule = wxString(currlayer->algo->getrule(),wxConvLocal);
    int newmaxstate = currlayer->algo->NumCellStates() - 1;
    if (oldrule != newrule || oldmaxstate != newmaxstate) {
        // show new rule in main window's title but don't change name
        mainptr->SetWindowTitle(wxEmptyString);
        
        // if pattern exists and is at starting gen then ensure savestart is true
        // so that SaveStartingPattern will save pattern to suitable file
        // (and thus undo/reset will work correctly)
        if (currlayer->algo->getGeneration() == currlayer->startgen && !currlayer->algo->isEmpty()) {
            currlayer->savestart = true;
        }
        
        // if grid is bounded then remove any live cells outside grid edges
        if (currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0) {
            mainptr->ClearOutsideGrid();
        }
    
        // new rule might have changed the number of cell states;
        // if there are fewer states then pattern might change
        if (newmaxstate < oldmaxstate && !currlayer->algo->isEmpty()) {
            mainptr->ReduceCellStates(newmaxstate);
        }

        if (allowundo && !currlayer->stayclean) {
            currlayer->undoredo->RememberRuleChange(oldrule);
        }
    }
    
    // update colors and/or icons for the new rule
    UpdateLayerColors();
    
    // pattern might have changed or colors/icons might have changed
    mainptr->UpdateEverything();
}

// -----------------------------------------------------------------------------

bool DownloadFile(const wxString& url, const wxString& filepath)
{
    bool result = false;
    
    wxHTTP http;
    http.SetTimeout(5);                                // secs
    http.SetHeader(wxT("Accept") , wxT("*/*"));        // any file type
    http.SetHeader(wxT("User-Agent"), wxT("Golly"));
    
    // Connect() wants a server address (eg. "www.foo.com"), not a full URL
    wxString temp = url.AfterFirst('/');
    while (temp[0] == '/') temp = temp.Mid(1);
    size_t slashpos = temp.Find('/');
    wxString server = temp.Left(slashpos);
    if (http.Connect(server, 80)) {
        // GetInputStream() wants everything after the server address
        wxString respath = temp.Right(temp.length() - slashpos);
        wxInputStream* instream = http.GetInputStream(respath);
        if (instream) {
            
            wxFileOutputStream outstream(filepath);
            if (outstream.Ok()) {
                // read and write in chunks so we can show a progress dialog
                const int BUFFER_SIZE = 4000;             // seems ok (on Mac at least)
                char buf[BUFFER_SIZE];
                size_t incount = 0;
                size_t outcount = 0;
                size_t lastread, lastwrite;
                double filesize = (double) instream->GetSize();
                if (filesize <= 0.0) filesize = -1.0;     // show indeterminate progress
                
                BeginProgress(_("Downloading file"));
                while (true) {
                    instream->Read(buf, BUFFER_SIZE);
                    lastread = instream->LastRead();
                    if (lastread == 0) break;
                    outstream.Write(buf, lastread);
                    lastwrite = outstream.LastWrite();
                    incount += lastread;
                    outcount += lastwrite;
                    if (incount != outcount) {
                        Warning(_("Error occurred while writing file:\n") + filepath);
                        break;
                    }
                    char msg[128];
                    sprintf(msg, "File size: %.2f MB", double(incount) / 1048576.0);
                    if (AbortProgress((double)incount / filesize, wxString(msg,wxConvLocal))) {
                        // force false result
                        outcount = 0;
                        break;
                    }
                }
                EndProgress();
                
                result = (incount == outcount);
                if (!result) {
                    // delete incomplete filepath
                    if (wxFileExists(filepath)) wxRemoveFile(filepath);
                }
            } else {
                Warning(_("Could not open output stream for file:\n") + filepath);
            }
            delete instream;
            
        } else {
            int err = http.GetError();
            if (err == wxPROTO_NOFILE) {
                Warning(_("Remote file does not exist:\n") + url);
            } else {
                // we get wxPROTO_NETERR (generic network error) with some naughty servers
                // that use LF rather than CRLF to terminate HTTP header messages
                // eg: http://fano.ics.uci.edu/ca/rules/b0135s014/g1.lif
                // (wxProtocol::ReadLine needs to be made more tolerant)
                Warning(wxString::Format(_("Could not download file (error %d):\n"),err) + url);
            }
        }
    } else {
        Warning(_("Could not connect to server:\n") + server);
    }
    
    http.Close();
    return result;
}

// -----------------------------------------------------------------------------

void GetURL(const wxString& url)
{
    wxString fullurl;
    if (url.StartsWith(wxT("http:"))) {
        fullurl = url;
    } else {
        // relative get, so prepend prefix set earlier in UpdateHelpButtons
        fullurl = urlprefix + url;
    }
    
    wxString filename = fullurl.AfterLast('/');
    
    // remove ugly stuff at start of file names downloaded from ConwayLife.com
    if (filename.StartsWith(wxT("download.php?f=")) ||
        filename.StartsWith(wxT("pattern.asp?p=")) ||
        filename.StartsWith(wxT("script.asp?s="))) {
        filename = filename.AfterFirst('=');
    }
    
    // create full path for downloaded file based on given url;
    // first remove initial "http://"
    wxString filepath = fullurl.AfterFirst('/');
    while (filepath[0] == '/') filepath = filepath.Mid(1);
    if (IsHTMLFile(filename)) {
        // create special name for html file so UpdateHelpButtons can detect it
        // and set urlprefix
        filepath.Replace(wxT("/"), wxT(" "));     // assume url has no spaces
        filepath = HTML_PREFIX + filepath;
    } else {
        // no need for url info in file name
        filepath = filename;
    }
#ifdef __WXMSW__
    // replace chars that can appear in URLs but are not allowed in filenames on Windows
    filepath.Replace(wxT("*"), wxT("_"));
    filepath.Replace(wxT("?"), wxT("_"));
#endif
    
    if (IsRuleFile(filename)) {
        // create file in user's rules directory (rulesdir might be read-only)
        filepath = userrules + filename;
    } else if (IsHTMLFile(filename)) {
        // nicer to store html files in temporary directory
        filepath = tempdir + filepath;
    } else {
        // all other files are stored in user's download directory
        filepath = downloaddir + filepath;
    }
    
    // try to download the file
    if ( !DownloadFile(fullurl, filepath) ) return;
    
    if (htmlwin->editlink) {
        if (IsRuleFile(filename) && filename.Lower().EndsWith(wxT(".icons"))) {
            // let user see b&w image in .icons file
            mainptr->Raise();
            mainptr->OpenFile(filepath);
        } else {
            mainptr->EditFile(filepath);
        }
        return;
    }
    
    if (IsHTMLFile(filename)) {
        // display html file in help window
        htmlwin->LoadPage(filepath);
        
    } else if (IsRuleFile(filename)) {
        // load corresponding .rule file
        LoadRule(filename.BeforeLast('.'));
        
    } else if (IsTextFile(filename)) {
        // open text file in user's text editor
        mainptr->EditFile(filepath);
        
    } else if (IsZipFile(filename)) {
        // open zip file (don't raise main window here)
        mainptr->OpenFile(filepath);
        
    } else if (IsScriptFile(filename)) {
        // run script depending on safety check; if it is allowed to run
        // then we remember script in the Run Recent submenu
        mainptr->CheckBeforeRunning(filepath, true, wxEmptyString);
        
    } else {
        // assume pattern file, so try to load it
        mainptr->Raise();
        mainptr->OpenFile(filepath);
    }
    
    if (helpptr && helpptr->infront) UpdateHelpButtons();
}

// -----------------------------------------------------------------------------

void UnzipFile(const wxString& zippath, const wxString& entry)
{
    wxString filename = entry.AfterLast(wxFILE_SEP_PATH);
    wxString tempfile = tempdir + filename;
    
    if ( IsRuleFile(filename) ) {
        // rule-related file should have already been extracted and installed
        // into userrules, so check that file exists and load rule
        wxString rulefile = userrules + filename;
        if (wxFileExists(rulefile)) {
            if (htmlwin->editlink) {
                if (filename.Lower().EndsWith(wxT(".icons"))) {
                    // let user see b&w image in .icons file
                    mainptr->Raise();
                    mainptr->OpenFile(rulefile);
                } else {
                    mainptr->EditFile(rulefile);
                }
            } else {         
                // load corresponding .rule file
                LoadRule(filename.BeforeLast('.'));
            }
        } else {
            Warning(_("Rule-related file was not installed:\n") + rulefile);
        }
        
    } else if ( mainptr->ExtractZipEntry(zippath, entry, tempfile) ) {
        if (htmlwin->editlink) {
            mainptr->EditFile(tempfile);
            
        } else if ( IsHTMLFile(filename) ) {
            // display html file
            htmlwin->LoadPage(tempfile);
            if (helpptr && helpptr->infront) UpdateHelpButtons();
            
        } else if ( IsTextFile(filename) ) {
            // open text file in user's text editor
            mainptr->EditFile(tempfile);
            
        } else if ( IsScriptFile(filename) ) {
            // run script depending on safety check; note that because the script is
            // included in a zip file we don't remember it in the Run Recent submenu
            mainptr->CheckBeforeRunning(tempfile, false, zippath);
            
        } else {
            // open pattern but don't remember in Open Recent menu
            mainptr->Raise();
            mainptr->OpenFile(tempfile, false);
        }
    }
}

// -----------------------------------------------------------------------------

void ClickLexiconPattern(const wxHtmlCell* htmlcell)
{
    if (htmlcell) {
        wxHtmlContainerCell* parent = htmlcell->GetParent();
        if (parent) {
            parent = parent->GetParent();
            if (parent) {
                wxHtmlCell* container = parent->GetFirstChild();
                
                // extract pattern data and store in lexpattern
                lexpattern.Clear();
                while (container) {
                    wxHtmlCell* cell = container->GetFirstChild();
                    while (cell) {
                        wxString celltext = cell->ConvertToText(NULL);
                        if (celltext.IsEmpty()) {
                            // probably a formatting cell
                        } else {
                            lexpattern += celltext;
                            // append eol char(s)
#ifdef __WXMSW__
                            // use DOS line ending (CR+LF) on Windows
                            lexpattern += '\r';
                            lexpattern += '\n';
#else
                            // use LF on Linux or Mac
                            lexpattern += '\n';
#endif
                        }
                        cell = cell->GetNext();
                    }
                    container = container->GetNext();
                }
                
                if (!lexpattern.IsEmpty()) {
                    mainptr->Raise();
                    // look for existing lexicon layer
                    lexlayer = -1;
                    for (int i = 0; i < numlayers; i++) {
                        if (GetLayer(i)->currname == lexicon_name) {
                            lexlayer = i;
                            break;
                        }
                    }
                    if (lexlayer < 0 && numlayers == MAX_LAYERS) {
                        Warning(_("Cannot create new layer for lexicon pattern."));
                        return;
                    }
                    
                    if (mainptr->generating) {
                        mainptr->command_pending = true;
                        mainptr->cmdevent.SetId(ID_LOAD_LEXICON);
                        mainptr->Stop();
                        return;
                    }
                    
                    LoadLexiconPattern();
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------

void LoadLexiconPattern()
{
    // switch to existing lexicon layer or create a new such layer
    if (lexlayer >= 0) {
        SetLayer(lexlayer);
    } else {
        AddLayer();
        mainptr->SetWindowTitle(lexicon_name);
    }
    
    // copy lexpattern data to tempstart file so we can handle
    // all formats supported by readpattern
    wxFile outfile(currlayer->tempstart, wxFile::write);
    if ( outfile.IsOpened() ) {
        outfile.Write(lexpattern);
        outfile.Close();
        
        // all Life Lexicon patterns assume we're using Conway's Life so try
        // switching to B3/S23 or Life; if that fails then switch to QuickLife
        const char* err = currlayer->algo->setrule("B3/S23");
        if (err) {
            // try "Life" in case current algo is RuleLoader and Life.rule exists
            // (also had to make a similar change to the loadpattern code in readpattern.cpp)
            err = currlayer->algo->setrule("Life");
        }
        if (err) {
            mainptr->ChangeAlgorithm(QLIFE_ALGO, wxString("B3/S23",wxConvLocal));
        }
        
        // load lexicon pattern into current layer
        mainptr->LoadPattern(currlayer->tempstart, lexicon_name);
    } else {
        Warning(_("Could not create tempstart file!"));
    }
}

// -----------------------------------------------------------------------------

void HtmlView::OnLinkClicked(const wxHtmlLinkInfo& link)
{
#ifdef __WXMAC__
    if ( stopwatch->Time() - whenactive < 500 ) {
        // avoid problem on Mac:
        // ignore click in link if the help window was in the background;
        // this isn't fail safe because OnLinkClicked is only called AFTER
        // the mouse button is released (better soln would be to set an
        // ignoreclick flag in OnMouseDown handler if click occurred very
        // soon after activate)
        return;
    }
#endif
    
    wxString url = link.GetHref();
    if ( url.StartsWith(wxT("http:")) || url.StartsWith(wxT("mailto:")) ) {
        // pass http/mailto URL to user's preferred browser/emailer
        if ( !wxLaunchDefaultBrowser(url) )
            Warning(_("Could not open URL in browser!"));
        
    } else if ( url.StartsWith(wxT("get:")) ) {
        if (mainptr->generating) {
            Warning(_("Cannot download file while generating a pattern."));
        } else if (inscript) {
            Warning(_("Cannot download file while a script is running."));
        } else if (editlink && IsZipFile(url)) {
            Warning(_("Opening a zip file in a text editor is not a good idea."));
        } else {
            // download clicked file
            GetURL( url.AfterFirst(':') );
        }
        
    } else if ( url.StartsWith(wxT("unzip:")) ) {
        if (inscript) {
            Warning(_("Cannot extract zip entry while a script is running."));
        } else {
            // extract clicked entry from zip file
            wxString zippath = url.AfterFirst(':');
            wxString entry = url.AfterLast(':');
            zippath = zippath.BeforeLast(':');
            UnzipFile(zippath, entry);
        }
        
    } else if ( url.StartsWith(wxT("edit:")) ) {
        // open clicked file in user's preferred text editor
        wxString path = url.AfterFirst(':');
#ifdef __WXMSW__
        path.Replace(wxT("/"), wxT("\\"));
#endif
        wxFileName fname(path);
        if (!fname.IsAbsolute()) path = gollydir + path;
        mainptr->EditFile(path);
        
    } else if ( url.StartsWith(wxT("lexpatt:")) ) {
        if (inscript) {
            Warning(_("Cannot load lexicon pattern while a script is running."));
        } else {
            // user clicked on pattern in Life Lexicon
            ClickLexiconPattern( link.GetHtmlCell() );
        }
        
    } else if ( url.StartsWith(wxT("prefs:")) ) {
        // user clicked on link to Preferences dialog
        mainptr->ShowPrefsDialog( url.AfterFirst(':') );
        
    } else if ( url.StartsWith(wxT("open:")) ) {
        wxString path = url.AfterFirst(':');
#ifdef __WXMSW__
        path.Replace(wxT("/"), wxT("\\"));
#endif
        wxFileName fname(path);
        if (!fname.IsAbsolute()) path = gollydir + path;
        if (inscript) {
            if (pass_file_events) {
                PassFileToScript(path);
            }
        } else {
            // open clicked file
            if (editlink) {
                mainptr->EditFile(path);
            } else {
                mainptr->Raise();
                mainptr->OpenFile(path);
            }
        }
        
    } else if ( url.StartsWith(wxT("rule:")) ) {
        // switch to given rule (false = not necessarily in a .rule file)
        LoadRule( url.AfterFirst(':'), false );
        
    } else {
        // assume it's a link to a local target or another help file
        CheckAndLoad(url);
        if (helpptr && helpptr->infront) UpdateHelpButtons();
    }
}

// -----------------------------------------------------------------------------

void HtmlView::OnCellMouseHover(wxHtmlCell* cell, wxCoord x, wxCoord y)
{
    if (helpptr && helpptr->infront && cell) {
        wxHtmlLinkInfo* link = cell->GetLink(x,y);
        if (link) {
            wxString href = link->GetHref();
            href.Replace(wxT("&"), wxT("&&"));
            helpptr->SetStatus(href);
            wxPoint pt = ScreenToClient( wxGetMousePosition() );
            linkrect = wxRect(pt.x-x, pt.y-y, cell->GetWidth(), cell->GetHeight());
        } else {
            ClearStatus();
        }
    }
}

// -----------------------------------------------------------------------------

void HtmlView::OnMouseMotion(wxMouseEvent& event)
{
    if (helpptr && helpptr->infront && !linkrect.IsEmpty()) {
        int x = event.GetX();
        int y = event.GetY();
        if (!linkrect.Contains(x,y)) ClearStatus();
    }
    event.Skip();
}

// -----------------------------------------------------------------------------

void HtmlView::OnMouseLeave(wxMouseEvent& event)
{
    if (helpptr && helpptr->infront) {
        ClearStatus();
    }
    event.Skip();
}

// -----------------------------------------------------------------------------

void HtmlView::ClearStatus()
{
    if (helpptr) {
        helpptr->SetStatus(wxEmptyString);
        linkrect = wxRect(0,0,0,0);
    }
}

// -----------------------------------------------------------------------------

#if defined(__WXMAC__) && wxCHECK_VERSION(2,9,0)
    // wxMOD_CONTROL has been changed to mean Command key down (sheesh!)
    #define wxMOD_CONTROL wxMOD_RAW_CONTROL
    #define ControlDown RawControlDown
#endif

void HtmlView::OnMouseDown(wxMouseEvent& event)
{
    // set flag so ctrl/right-clicked file can be opened in editor
    // (this is consistent with how we handle clicks in the file pane)
    editlink = event.ControlDown() || event.RightDown();
    event.Skip();
}

// -----------------------------------------------------------------------------

void HtmlView::CheckAndLoad(const wxString& filepath)
{
    if (filepath == SHOW_KEYBOARD_SHORTCUTS) {
        // build HTML string to display current keyboard shortcuts
        wxString contents = GetShortcutTable();
        
        // write contents to file and call LoadPage so that back/forwards buttons work
        wxString htmlfile = tempdir + SHOW_KEYBOARD_SHORTCUTS;
        wxFile outfile(htmlfile, wxFile::write);
        if (outfile.IsOpened()) {
            outfile.Write(contents);
            outfile.Close();
            LoadPage(htmlfile);
        } else {
            Warning(_("Could not create file:\n") + htmlfile);
            // might as well show contents
            SetPage(contents);
            currhelp = SHOW_KEYBOARD_SHORTCUTS;
        }
        
    } else if ( filepath.StartsWith(_("Help/")) ) {
        // prepend location of Golly so user can open help while running a script
        wxString fullpath = gollydir + filepath;
        LoadPage(fullpath);
        
    } else {
        // assume full path or local link
        #if defined(__WXMSW__) && wxCHECK_VERSION(3,1,0)
            wxFileName fname(filepath);
            if (fname.IsAbsolute()) {
                // call LoadFile rather than LoadPage to avoid bug in wx 3.1.0 on Windows 10
                LoadFile(fname);
            } else {
                LoadPage(filepath);
            }
        #else
            LoadPage(filepath);
        #endif
    }
}

// -----------------------------------------------------------------------------

#ifdef __WXMSW__
// we have to use OnKeyUp handler on Windows otherwise wxHtmlWindow's OnKeyUp
// gets called which detects ctrl-C and clobbers our clipboard fix
void HtmlView::OnKeyUp(wxKeyEvent& event)
#else
// we have to use OnKeyDown handler on Mac -- if OnKeyUp handler is used and
// cmd-C is pressed quickly then key code is 400!!!
void HtmlView::OnKeyDown(wxKeyEvent& event)
#endif
{
    int key = event.GetKeyCode();
    if (event.CmdDown()) {
        // let cmd-A select all text
        if (key == 'A') {
            SelectAll();
            return;
        }
#ifdef __WXMAC__
        // let cmd-W close help window or about box
        if (key == 'W') {
            GetParent()->Close(true);
            return;
        }
#endif
    }
    if ( event.CmdDown() || event.AltDown() ) {
        if ( key == 'C' ) {
            // copy any selected text to the clipboard
            wxString text = SelectionToText();
            if ( text.Length() > 0 ) {
                if ( helpptr && helpptr->infront &&
                    GetOpenedPageTitle().StartsWith(wxT("Life Lexicon")) ) {
                    // avoid wxHTML bug when copying text inside <pre>...</pre>!!!
                    // if there are at least 2 lines and the 1st line is twice
                    // the size of the 2nd line then insert \n in middle of 1st line
                    if ( text.Freq('\n') > 0 ) {
                        wxString line1 = text.BeforeFirst('\n');
                        wxString aftern = text.AfterFirst('\n');
                        wxString line2 = aftern.BeforeFirst('\n');
                        size_t line1len = line1.Length();
                        size_t line2len = line2.Length();
                        if ( line1len == 2 * line2len ) {
                            wxString left = text.Left(line2len);
                            wxString right = text.Mid(line2len);
                            text = left;
                            text += '\n';
                            text += right;
                        }
                    }
                }
                mainptr->CopyTextToClipboard(text);
            }
        } else {
            event.Skip();
        }
    } else {
        // this handler is also called from ShowAboutBox
        if ( helpptr == NULL || !helpptr->infront ) {
            if ( key == WXK_NUMPAD_ENTER || key == WXK_RETURN ) {
                // allow enter key or return key to close about box
                GetParent()->Close(true);
                return;
            }
            event.Skip();
            return;
        }
        // let escape/return/enter key close help window
        if ( key == WXK_ESCAPE || key == WXK_RETURN || key == WXK_NUMPAD_ENTER ) {
            helpptr->Close(true);
        } else if ( key == WXK_HOME ) {
            ShowHelp(helphome);
        } else {
            event.Skip();
        }
    }
}

// -----------------------------------------------------------------------------

void HtmlView::OnChar(wxKeyEvent& event)
{
    // this handler is also called from ShowAboutBox
    if ( helpptr == NULL || !helpptr->infront ) {
        event.Skip();
        return;
    }
    int key = event.GetKeyCode();
    if ( key == '+' || key == '=' || key == WXK_ADD ) {
        if ( helpfontsize < maxfontsize ) {
            helpfontsize++;
            ChangeFontSizes(helpfontsize);
        }
    } else if ( key == '-' || key == WXK_SUBTRACT ) {
        if ( helpfontsize > minfontsize ) {
            helpfontsize--;
            ChangeFontSizes(helpfontsize);
        }
    } else if ( key == '[' || key == WXK_LEFT ) {
        if ( HistoryBack() ) {
            UpdateHelpButtons();
        }
    } else if ( key == ']' || key == WXK_RIGHT ) {
        if ( HistoryForward() ) {
            UpdateHelpButtons();
        }
    } else {
        event.Skip();     // so up/down arrows and pg up/down keys work
    }
}

// -----------------------------------------------------------------------------

// avoid scroll position being reset to top when wxHtmlWindow is resized
void HtmlView::OnSize(wxSizeEvent& event)
{
    int x, y;
    GetViewStart(&x, &y);         // save current position
    
    wxHtmlWindow::OnSize(event);
    
    wxString location = GetOpenedPage();
    if ( !location.IsEmpty() && canreload ) {

        if (location.StartsWith(wxT("file:"))) {
            // this happens in wx 3.1.0, so convert location from URL to file path
            wxFileName fname = wxFileSystem::URLToFileName(location);
            location = fname.GetFullPath();
            #ifdef __WXMSW__
                location.Replace(wxT("\\"), wxT("/"));
            #endif
        }
        
        // avoid bug in wx 3.1.0
        location.Replace(wxT("%20"), wxT(" "));
        location.Replace(wxT("%23"), wxT("#"));
    
        CheckAndLoad(location);    // reload page
        Scroll(x, y);              // scroll to old position
    }
    
    // prevent wxHtmlWindow::OnSize being called again
    event.Skip(false);
}

// -----------------------------------------------------------------------------

void HtmlView::OnTimer(wxTimerEvent& WXUNUSED(event))
{
    if (helpptr && helpptr->infront) {
        // send idle event to html window so cursor gets updated
        // even while app is busy doing something else (eg. generating)
        wxIdleEvent idleevent;
#if wxCHECK_VERSION(2,9,2)
        // SendIdleEvents is now in wxWindow
        SendIdleEvents(idleevent);
#else
        wxGetApp().SendIdleEvents(this, idleevent);
#endif
    }
}

// -----------------------------------------------------------------------------

void HtmlView::SetFontSizes(int size)
{
    // set font sizes for <FONT SIZE=-2> to <FONT SIZE=+4>
    int f_sizes[7];
    f_sizes[0] = int(size * 0.6);
    f_sizes[1] = int(size * 0.8);
    f_sizes[2] = size;
    f_sizes[3] = int(size * 1.2);
    f_sizes[4] = int(size * 1.4);
    f_sizes[5] = int(size * 1.6);
    f_sizes[6] = int(size * 1.8);
#ifdef __WXOSX_COCOA__
    SetFonts(wxT("Lucida Grande"), wxT("Monaco"), f_sizes);
#else
    SetFonts(wxEmptyString, wxEmptyString, f_sizes);
#endif
}

// -----------------------------------------------------------------------------

void HtmlView::ChangeFontSizes(int size)
{
    // changing font sizes resets pos to top, so save and restore pos
    int x, y;
    GetViewStart(&x, &y);
    SetFontSizes(size);
    if (y > 0) Scroll(-1, y);
}

// -----------------------------------------------------------------------------

void ShowAboutBox()
{
    if (viewptr->waitingforclick) return;
    
    wxBoxSizer* topsizer = new wxBoxSizer(wxVERTICAL);
    wxDialog dlg(mainptr, wxID_ANY, wxString(_("About Golly")));
    
    HtmlView* html = new HtmlView(&dlg, wxID_ANY, wxDefaultPosition,
#if wxCHECK_VERSION(2,9,0)
                                  // work around SetSize bug below!!!
                                  wxSize(400, 320),
#elif defined(__WXGTK__)
                                  wxSize(460, 220),
#else
                                  wxSize(386, 220),
#endif
                                  wxHW_SCROLLBAR_NEVER | wxSUNKEN_BORDER);
    html->SetBorders(0);
#ifdef __WXOSX_COCOA__
    html->SetFontSizes(helpfontsize);
#endif
    html->CheckAndLoad(_("Help/about.html"));
    
    // avoid HtmlView::OnSize calling CheckAndLoad again
    html->canreload = false;
    
    // this call seems to be ignored in __WXOSX_COCOA__!!!
    html->SetSize(html->GetInternalRepresentation()->GetWidth(),
                  html->GetInternalRepresentation()->GetHeight());
    
    topsizer->Add(html, 1, wxALL, 10);
    
    wxButton* okbutt = new wxButton(&dlg, wxID_OK, _("OK"));
    okbutt->SetDefault();
    topsizer->Add(okbutt, 0, wxBOTTOM | wxALIGN_CENTER, 10);
    
    dlg.SetSizer(topsizer);
    topsizer->Fit(&dlg);
    dlg.Centre();
    dlg.ShowModal();
}
