// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifdef ANDROID_GUI
    #include "jnicalls.h"   // for SwitchToPatternTab, ShowTextFile, ShowHelp, etc
#endif

#ifdef WEB_GUI
    #include "webcalls.h"   // for SwitchToPatternTab, ShowTextFile, ShowHelp, etc
#endif

#ifdef IOS_GUI
    #import "GollyAppDelegate.h"        // for SwitchToPatternTab
    #import "HelpViewController.h"      // for ShowHelp
    #import "InfoViewController.h"      // for ShowTextFile
#endif

// we use MiniZip code for opening zip files and extracting files stored within them
#include "zip.h"
#include "unzip.h"

#include <string>           // for std::string
#include <list>             // for std::list
#include <stdexcept>        // for std::runtime_error and std::exception
#include <sstream>          // for std::ostringstream
#include <algorithm>        // for std::replace

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "ruleloaderalgo.h" // for noTABLEorTREE
#include "readpattern.h"    // for readpattern
#include "writepattern.h"   // for writepattern, pattern_format

#include "utils.h"          // for Warning
#include "prefs.h"          // for SavePrefs, allowundo, userrules, etc
#include "status.h"         // for SetMessage
#include "algos.h"          // for CreateNewUniverse, algo_type, algoinfo, etc
#include "layer.h"          // for currlayer, etc
#include "control.h"        // for SetGenIncrement, ChangeAlgorithm
#include "view.h"           // for origin_restored, nopattupdate
#include "undo.h"           // for UndoRedo
#include "file.h"

// -----------------------------------------------------------------------------

std::string GetBaseName(const char* path)
{
    // extract basename from given path
    std::string basename = path;
    size_t lastsep = basename.rfind('/');
    if (lastsep != std::string::npos) {
        basename = basename.substr(lastsep+1);
    }
    return basename;
}

// -----------------------------------------------------------------------------

void SetPatternTitle(const char* filename)
{
    if ( filename[0] != 0 ) {
        // remember current file name
        currlayer->currname = filename;
        // show currname in current layer's menu item
        //!!! UpdateLayerItem(currindex);
    }
}

// -----------------------------------------------------------------------------

bool SaveCurrentLayer()
{
    if (currlayer->algo->isEmpty()) return true;    // no need to save empty universe

#ifdef WEB_GUI
    // show a modal dialog that lets user save their changes
    return WebSaveChanges();
#else
    // currently ignored in Android and iOS versions
    return true;    
#endif
}

// -----------------------------------------------------------------------------

void CreateUniverse()
{
    // save current rule
    std::string oldrule = currlayer->algo->getrule();

    // delete old universe and create new one of same type
    delete currlayer->algo;
    currlayer->algo = CreateNewUniverse(currlayer->algtype);

    // ensure new universe uses same rule (and thus same # of cell states)
    RestoreRule(oldrule.c_str());

    // increment has been reset to 1 but that's probably not always desirable
    // so set increment using current step size
    SetGenIncrement();
}

// -----------------------------------------------------------------------------

void NewPattern(const char* title)
{
    if (generating) Warning("Bug detected in NewPattern!");

    if (currlayer->dirty && asktosave && !SaveCurrentLayer()) return;

    currlayer->savestart = false;
    currlayer->currfile.clear();
    currlayer->startgen = 0;

    // reset step size before CreateUniverse calls SetGenIncrement
    currlayer->currbase = algoinfo[currlayer->algtype]->defbase;
    currlayer->currexpo = 0;

    // create new, empty universe of same type and using same rule
    CreateUniverse();

    // clear all undo/redo history
    currlayer->undoredo->ClearUndoRedo();

    // possibly clear selection
    currlayer->currsel.Deselect();

    // initially in drawing mode
    currlayer->touchmode = drawmode;

    // reset location and scale
    currlayer->view->setpositionmag(bigint::zero, bigint::zero, MAX_MAG);   // no longer use newmag

    // best to restore true origin
    if (currlayer->originx != bigint::zero || currlayer->originy != bigint::zero) {
        currlayer->originx = 0;
        currlayer->originy = 0;
        SetMessage(origin_restored);
    }

    // restore default colors for current algo/rule
    UpdateLayerColors();

    MarkLayerClean(title);     // calls SetPatternTitle
}

// -----------------------------------------------------------------------------

bool LoadPattern(const char* path, const char* newtitle)
{
    if ( !FileExists(path) ) {
        std::string msg = "The file does not exist:\n";
        msg += path;
        Warning(msg.c_str());
        return false;
    }

    // newtitle is only empty if called from ResetPattern/RestorePattern
    if (newtitle[0] != 0) {
        if (currlayer->dirty && asktosave && !SaveCurrentLayer()) return false;

        currlayer->savestart = false;
        currlayer->currfile = path;

        // reset step size
        currlayer->currbase = algoinfo[currlayer->algtype]->defbase;
        currlayer->currexpo = 0;

        // clear all undo/redo history
        currlayer->undoredo->ClearUndoRedo();
    }

    // disable pattern update so we see gen=0 and pop=0;
    // in particular, it avoids getPopulation being called which would slow down macrocell loading
    nopattupdate = true;

    // save current algo and rule
    algo_type oldalgo = currlayer->algtype;
    std::string oldrule = currlayer->algo->getrule();

    // delete old universe and create new one of same type
    delete currlayer->algo;
    currlayer->algo = CreateNewUniverse(currlayer->algtype);

    const char* err = readpattern(path, *currlayer->algo);
    if (err) {
        // cycle thru all other algos until readpattern succeeds
        for (int i = 0; i < NumAlgos(); i++) {
            if (i != oldalgo) {
                currlayer->algtype = i;
                delete currlayer->algo;
                currlayer->algo = CreateNewUniverse(currlayer->algtype);
                // readpattern will call setrule
                err = readpattern(path, *currlayer->algo);
                if (!err) break;
            }
        }
        if (err) {
            // no algo could read pattern so restore original algo and rule
            currlayer->algtype = oldalgo;
            delete currlayer->algo;
            currlayer->algo = CreateNewUniverse(currlayer->algtype);
            RestoreRule(oldrule.c_str());
            // Warning(err);
            // current error and original error are not necessarily meaningful
            // so report a more generic error
            Warning("File could not be loaded by any algorithm\n(probably due to an unknown rule).");
        }
    }

    // enable pattern update
    nopattupdate = false;

    if (newtitle[0] != 0) {
        MarkLayerClean(newtitle);     // calls SetPatternTitle

        // restore default base step for current algo
        // (currlayer->currexpo was set to 0 above)
        currlayer->currbase = algoinfo[currlayer->algtype]->defbase;

        SetGenIncrement();

        // restore default colors for current algo/rule
        UpdateLayerColors();

        currlayer->currsel.Deselect();

        // initially in moving mode
        currlayer->touchmode = movemode;

        currlayer->algo->fit(*currlayer->view, 1);
        currlayer->startgen = currlayer->algo->getGeneration();     // might be > 0

        UpdateEverything();
    }

    return err == NULL;
}

// -----------------------------------------------------------------------------

void AddRecentPattern(const char* inpath)
{
    std::string path = inpath;

    if (path.find(userdir) == 0) {
        // remove userdir from start of path
        path.erase(0, userdir.length());
    }

    // check if path is already in recentpatterns
    if (!recentpatterns.empty()) {
        std::list<std::string>::iterator next = recentpatterns.begin();
        while (next != recentpatterns.end()) {
            std::string nextpath = *next;
            if (path == nextpath) {
                if (next == recentpatterns.begin()) {
                    // path is in recentpatterns and at top, so we're done
                    return;
                }
                // remove this path from recentpatterns (we'll add it below)
                recentpatterns.erase(next);
                numpatterns--;
                break;
            }
            next++;
        }
    }

    // put given path at start of recentpatterns
    recentpatterns.push_front(path);
    if (numpatterns < maxpatterns) {
        numpatterns++;
    } else {
        // remove the path at end of recentpatterns
        recentpatterns.pop_back();
    }
}

// -----------------------------------------------------------------------------

bool CopyTextToClipboard(const char* text)
{
#ifdef ANDROID_GUI
    return AndroidCopyTextToClipboard(text);
#endif

#ifdef WEB_GUI
    return WebCopyTextToClipboard(text);
#endif

#ifdef IOS_GUI
    UIPasteboard *pboard = [UIPasteboard generalPasteboard];
    pboard.string = [NSString stringWithCString:text encoding:NSUTF8StringEncoding];
    return true;
#endif
}

// -----------------------------------------------------------------------------

bool GetTextFromClipboard(std::string& text)
{
#ifdef ANDROID_GUI
    return AndroidGetTextFromClipboard(text);
#endif

#ifdef WEB_GUI
    return WebGetTextFromClipboard(text);
#endif

#ifdef IOS_GUI
    UIPasteboard *pboard = [UIPasteboard generalPasteboard];
    NSString *str = pboard.string;
    if (str == nil) {
        ErrorMessage("No text in clipboard.");
        return false;
    } else {
        text = [str cStringUsingEncoding:NSUTF8StringEncoding];
        return true;
    }
#endif
}

// -----------------------------------------------------------------------------

void LoadRule(const std::string& rulestring)
{
    // load recently installed .rule file
    std::string oldrule = currlayer->algo->getrule();
    int oldmaxstate = currlayer->algo->NumCellStates() - 1;

    // selection might change if grid becomes smaller,
    // so save current selection for RememberRuleChange/RememberAlgoChange
    SaveCurrentSelection();
    
    // InitAlgorithms ensures the RuleLoader algo is the last algo
    int rule_loader_algo = NumAlgos() - 1;
    
    const char* err;
    if (currlayer->algtype == rule_loader_algo) {
        // RuleLoader is current algo so no need to switch
        err = currlayer->algo->setrule(rulestring.c_str());
    } else {
        // switch to RuleLoader algo
        lifealgo* tempalgo = CreateNewUniverse(rule_loader_algo);
        err = tempalgo->setrule(rulestring.c_str());
        delete tempalgo;
        if (!err) {
            // change the current algorithm and switch to the new rule
            ChangeAlgorithm(rule_loader_algo, rulestring.c_str());
            if (rule_loader_algo != currlayer->algtype) {
                RestoreRule(oldrule.c_str());
                Warning("Algorithm could not be changed (pattern is too big to convert).");
            }
            return;
        }
    }
    
    if (err) {
        // RuleLoader algo found some sort of error
        if (strcmp(err, noTABLEorTREE) == 0) {
            // .rule file has no TABLE or TREE section but it might be used
            // to override a built-in rule, so try each algo
            std::string temprule = rulestring;
            std::replace(temprule.begin(), temprule.end(), '_', '/');   // eg. convert B3_S23 to B3/S23
            for (int i = 0; i < NumAlgos(); i++) {
                lifealgo* tempalgo = CreateNewUniverse(i);
                err = tempalgo->setrule(temprule.c_str());
                delete tempalgo;
                if (!err) {
                    // change the current algorithm and switch to the new rule
                    ChangeAlgorithm(i, temprule.c_str());
                    if (i != currlayer->algtype) {
                        RestoreRule(oldrule.c_str());
                        Warning("Algorithm could not be changed (pattern is too big to convert).");
                    }
                    return;
                }
            }
        }
        
        RestoreRule(oldrule.c_str());
        std::string msg = "The rule file is not valid:\n";
        msg += rulestring;
        msg += "\n\nThe error message:\n";
        msg += err;
        Warning(msg.c_str());
        return;
    }

    std::string newrule = currlayer->algo->getrule();
    int newmaxstate = currlayer->algo->NumCellStates() - 1;
    if (oldrule != newrule || oldmaxstate != newmaxstate) {
    	
		// if pattern exists and is at starting gen then ensure savestart is true
		// so that SaveStartingPattern will save pattern to suitable file
		// (and thus undo/reset will work correctly)
		if (currlayer->algo->getGeneration() == currlayer->startgen && !currlayer->algo->isEmpty()) {
			currlayer->savestart = true;
		}

        // if grid is bounded then remove any live cells outside grid edges
        if (currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0) {
            ClearOutsideGrid();
        }

		// new rule might have changed the number of cell states;
		// if there are fewer states then pattern might change
		if (newmaxstate < oldmaxstate && !currlayer->algo->isEmpty()) {
			ReduceCellStates(newmaxstate);
		}
		
        if (allowundo && !currlayer->stayclean) {
            currlayer->undoredo->RememberRuleChange(oldrule.c_str());
        }
    }

    // set colors for new rule
    UpdateLayerColors();
}

// -----------------------------------------------------------------------------

bool ExtractZipEntry(const std::string& zippath, const std::string& entryname, const std::string& outfile)
{
    bool found = false;
    bool ok = false;
    std::ostringstream oss;
    int err;
    char* zipdata = NULL;

    unzFile zfile = unzOpen(zippath.c_str());
    if (zfile == NULL) {
        std::string msg = "Could not open zip file:\n" + zippath;
        Warning(msg.c_str());
        return false;
    }

    try {
        err = unzGoToFirstFile(zfile);
        if (err != UNZ_OK) {
            oss << "Error going to first file in zip file:\n" << zippath.c_str();
            throw std::runtime_error(oss.str().c_str());
        }
        do {
            char filename[256];
            unz_file_info file_info;
            err = unzGetCurrentFileInfo(zfile, &file_info, filename, sizeof(filename), NULL, 0, NULL, 0);
            if (err != UNZ_OK) {
                oss << "Error getting current file info in zip file:\n" << zippath.c_str();
                throw std::runtime_error(oss.str().c_str());
            }
            std::string thisname = filename;
            if (thisname == entryname) {
                found = true;
                // we've found the desired entry so copy entry data to given outfile
                zipdata = (char*) malloc(file_info.uncompressed_size);
                if (zipdata == NULL) {
                    throw std::runtime_error("Failed to allocate memory for zip file entry!");
                }
                err = unzOpenCurrentFilePassword(zfile, NULL);
                if (err != UNZ_OK) {
                    oss << "Error opening current zip entry:\n" << entryname.c_str();
                    throw std::runtime_error(oss.str().c_str());
                }
                int bytesRead = unzReadCurrentFile(zfile, zipdata, (unsigned int)file_info.uncompressed_size);
                if (bytesRead < 0) {
                    throw std::runtime_error("Error reading the zip entry data!");
                }
                err = unzCloseCurrentFile(zfile);
                if (err != UNZ_OK) {
                    oss << "Error closing current zip entry:\n" << entryname.c_str();
                    throw std::runtime_error(oss.str().c_str());
                }
                if (file_info.uncompressed_size == 0) {
                    Warning("Zip entry is empty!");
                } else if (bytesRead == (long)(file_info.uncompressed_size)) {
                    // write zipdata to outfile
                    FILE* f = fopen(outfile.c_str(), "wb");
                    if (f) {
                        if (fwrite(zipdata, 1, bytesRead, f) != (size_t)bytesRead) {
                            Warning("Could not write data for zip entry!");
                        } else {
                            ok = true;
                        }
                        fclose(f);
                    } else {
                        Warning("Could not create file for zip entry!");
                    }
                } else {
                    Warning("Failed to read all bytes of zip entry!");
                }
                break;
            }
        } while (unzGoToNextFile(zfile) == UNZ_OK);
    }
    catch(const std::exception& e) {
        // display message set by throw std::runtime_error(...)
        Warning(e.what());
    }
    
    if (zipdata) free(zipdata);

    err = unzClose(zfile);
    if (err != UNZ_OK) {
        Warning("Error closing zip file!");
    }

    if (found && ok) return true;

    if (!found) {
        std::string msg = "Could not find zip file entry:\n" + entryname;
        Warning(msg.c_str());
    } else {
        // outfile is probably incomplete so best to delete it
        if (FileExists(outfile)) RemoveFile(outfile);
    }

    return false;
}

// -----------------------------------------------------------------------------

void UnzipFile(const std::string& zippath, const std::string& entry)
{
    std::string filename = GetBaseName(entry.c_str());
    std::string tempfile = tempdir + filename;

    if ( IsRuleFile(filename) ) {
        // rule-related file should have already been extracted and installed
        // into userrules, so check that file exists and load rule
        std::string rulefile = userrules + filename;
        if (FileExists(rulefile)) {
            // load corresponding rule
            SwitchToPatternTab();
            LoadRule(filename.substr(0, filename.rfind('.')));
        } else {
            std::string msg = "Rule-related file was not installed:\n" + rulefile;
            Warning(msg.c_str());
        }

    } else if ( ExtractZipEntry(zippath, entry, tempfile) ) {
        if ( IsHTMLFile(filename) ) {
            // display html file
            ShowHelp(tempfile.c_str());

        } else if ( IsTextFile(filename) ) {
            // display text file
            ShowTextFile(tempfile.c_str());

        } else if ( IsScriptFile(filename) ) {
            // run script depending on safety check; note that because the script is
            // included in a zip file we don't remember it in the Run Recent submenu
            //!!! CheckBeforeRunning(tempfile, false, zippath);
            Warning("This version of Golly cannot run scripts.");

        } else {
            // open pattern but don't remember in recentpatterns
            OpenFile(tempfile.c_str(), false);
        }
    }
}

// -----------------------------------------------------------------------------

static bool RuleInstalled(unzFile zfile, unz_file_info& info, const std::string& outfile)
{
    if (info.uncompressed_size == 0) {
        Warning("Zip entry is empty!");
        return false;
    }

    char* zipdata = (char*) malloc(info.uncompressed_size);
    if (zipdata == NULL) {
        Warning("Failed to allocate memory for rule file!");
        return false;
    }

    int err = unzOpenCurrentFilePassword(zfile, NULL);
    if (err != UNZ_OK) {
        Warning("Error opening current zip entry!");
        free(zipdata);
        return false;
    }
    
    int bytesRead = unzReadCurrentFile(zfile, zipdata, (unsigned int)info.uncompressed_size);
    if (bytesRead < 0) {
        // error is detected below
    }
    err = unzCloseCurrentFile(zfile);
    if (err != UNZ_OK) {
        Warning("Error closing current zip entry!");
        // but keep going
    }
    
    bool ok = true;
    if (bytesRead == (long)(info.uncompressed_size)) {
        // write zipdata to outfile
        FILE* f = fopen(outfile.c_str(), "wb");
        if (f) {
            if (fwrite(zipdata, 1, bytesRead, f) != (size_t)bytesRead) {
                Warning("Could not write data for zip entry!");
                ok = false;
            }
            fclose(f);
        } else {
            Warning("Could not create file for zip entry!");
            ok = false;
        }
    } else {
        Warning("Failed to read all bytes of zip entry!");
        ok = false;
    }
    free(zipdata);
    return ok;
}

// -----------------------------------------------------------------------------

void OpenZipFile(const char* zippath)
{
    // Process given zip file in the following manner:
    // - If it contains any .rule files then extract and install those files
    //   into userrules (the user's rules directory).
    // - Build a temporary html file with clickable links to each file entry
    //   and show it in the Help tab.

    const std::string indent = "&nbsp;&nbsp;&nbsp;&nbsp;";
    bool dirseen = false;
    bool diffdirs = (userrules != rulesdir);
    std::string firstdir = "";
    std::string lastpattern = "";
    std::string lastscript = "";
    int patternseps = 0;                // # of separators in lastpattern
    int scriptseps = 0;                 // # of separators in lastscript
    int patternfiles = 0;
    int scriptfiles = 0;
    int textfiles = 0;                  // includes html files
    int rulefiles = 0;
    int deprecated = 0;                 // # of .table/tree files
    std::list<std::string> deplist;     // list of installed deprecated files
    std::list<std::string> rulelist;    // list of installed .rule files
    int err;

    // strip off patternsdir or userdir
    std::string relpath = zippath;
    size_t pos = relpath.find(patternsdir);
    if (pos == 0) {
        relpath.erase(0, patternsdir.length());
    } else {
        pos = relpath.find(userdir);
        if (pos == 0) relpath.erase(0, userdir.length());
    }

    std::string contents = "<html><body bgcolor=\"#FFFFCE\"><font size=+1><b><p>\nContents of ";
    contents += relpath;
    contents += ":<p>\n";

    unzFile zfile = unzOpen(zippath);
    if (zfile == NULL) {
        std::string msg = "Could not open zip file:\n";
        msg += zippath;
        Warning(msg.c_str());
        return;
    }

    try {
        err = unzGoToFirstFile(zfile);
        if (err != UNZ_OK) {
            throw std::runtime_error("Error going to first file in zip file!");
        }
        do {
            // examine each entry in zip file and build contents string;
            // also install any .rule files
            char entryname[256];
            unz_file_info file_info;
            err = unzGetCurrentFileInfo(zfile, &file_info, entryname, sizeof(entryname), NULL, 0, NULL, 0);
            if (err != UNZ_OK) {
                throw std::runtime_error("Error getting current file info in zip file!");
            }
            std::string name = entryname;
            if (name.find("__MACOSX") == 0 || name.rfind(".DS_Store") != std::string::npos) {
                // ignore meta-data stuff in zip file created on Mac
            } else {
                // indent depending on # of separators in name
                unsigned int sepcount = 0;
                unsigned int i = 0;
                unsigned int len = (unsigned int)name.length();
                while (i < len) {
                    if (name[i] == '/') sepcount++;
                    i++;
                }
                // check if 1st directory has multiple separators (eg. in jslife.zip)
                if (name[len-1] == '/' && !dirseen && sepcount > 1) {
                    firstdir = name.substr(0, name.find('/'));
                    contents += firstdir;
                    contents += "<br>\n";
                }
                for (i = 1; i < sepcount; i++) contents += indent;

                if (name[len-1] == '/') {
                    // remove terminating separator from directory name
                    name = name.substr(0, name.rfind('/'));
                    name = GetBaseName(name.c_str());
                    if (dirseen && name == firstdir) {
                        // ignore dir already output earlier (eg. in jslife.zip)
                    } else {
                        contents += name;
                        contents += "<br>\n";
                    }
                    dirseen = true;

                } else {
                    // entry is for some sort of file
                    std::string filename = GetBaseName(name.c_str());
                    if (dirseen) contents += indent;

                    if ( IsRuleFile(filename) && !EndsWith(filename,".rule") ) {
                        // this is a deprecated .table/tree/colors/icons file
                        if (EndsWith(filename,".colors") || EndsWith(filename,".icons")) {
                            // these files are no longer supported and are simply ignored
                            contents += filename;
                            contents += indent;
                            contents += "[ignored]";
                            // don't add to deprecated list
                        } else {
                            // .table/.tree file
                            contents += filename;
                            contents += indent;
                            contents += "[deprecated]";
                            deprecated++;
                            // install it into userrules so it can be used below to create a .rule file
                            std::string outfile = userrules + filename;
                            if (RuleInstalled(zfile, file_info, outfile)) {
                                deplist.push_back(filename);
                            } else {
                                contents += indent;
                                contents += "INSTALL FAILED!";
                            }
                        }

                    } else {
                        // user can extract file via special "unzip:" link
                        contents += "<a href=\"unzip:";
                        contents += zippath;
                        contents += ":";
                        contents += name;
                        contents += "\">";
                        contents += filename;
                        contents += "</a>";

                        if ( IsRuleFile(filename) ) {
                            // extract and install .rule file into userrules
                            std::string outfile = userrules + filename;
                            if (RuleInstalled(zfile, file_info, outfile)) {
                                // file successfully installed
                                rulelist.push_back(filename);
                                contents += indent;
                                contents += "[installed]";
                                if (diffdirs) {
                                    // check if this file overrides similarly named file in rulesdir
                                    std::string clashfile = rulesdir + filename;
                                    if (FileExists(clashfile)) {
                                        contents += indent;
                                        contents += "(overrides file in Rules folder)";
                                    }
                                }
                                #ifdef WEB_GUI
                                    // ensure the .rule file persists beyond the current session
                                    CopyRuleToLocalStorage(outfile.c_str());
                                #endif
                            } else {
                                // file could not be installed
                                contents += indent;
                                contents += "[NOT installed]";
                                // file is probably incomplete so best to delete it
                                if (FileExists(outfile)) RemoveFile(outfile);
                            }
                            rulefiles++;

                        } else if ( IsHTMLFile(filename) || IsTextFile(filename) ) {
                            textfiles++;

                        } else if ( IsScriptFile(filename) ) {
                            scriptfiles++;
                            lastscript = name;
                            scriptseps = sepcount;

                        } else {
                            patternfiles++;
                            lastpattern = name;
                            patternseps = sepcount;
                        }
                    }
                    contents += "<br>\n";
                }
            }

        } while (unzGoToNextFile(zfile) == UNZ_OK);
    }
    catch(const std::exception& e) {
        // display message set by throw std::runtime_error(...)
        Warning(e.what());
    }

    err = unzClose(zfile);
    if (err != UNZ_OK) {
        Warning("Error closing zip file!");
    }

    if (rulefiles > 0) {
        relpath = userrules;
        pos = relpath.find(userdir);
        if (pos == 0) relpath.erase(0, userdir.length());
        contents += "<p>Files marked as \"[installed]\" have been stored in ";
        contents += relpath;
        contents += ".";
    }
    if (deprecated > 0) {
        std::string newrules = CreateRuleFiles(deplist, rulelist);
        if (newrules.length() > 0) {
            contents += "<p>Files marked as \"[deprecated]\" have been used to create new .rule files:<br>\n";
            contents += newrules;
        }
    }
    contents += "\n</b></font></body></html>";

    // NOTE: The desktop version of Golly will load a pattern if it's in a "simple" zip file
    // but for the iPad version it's probably less confusing if the zip file's contents are
    // *always* displayed in the Help tab.  We might change this if script support is added.

    // write contents to a unique temporary html file
    std::string htmlfile = CreateTempFileName("zip_contents");
    htmlfile += ".html";
    FILE* f = fopen(htmlfile.c_str(), "w");
    if (f) {
        if (fputs(contents.c_str(), f) == EOF) {
            fclose(f);
            Warning("Could not write HTML data to temporary file!");
            return;
        }
    } else {
        Warning("Could not create temporary HTML file!");
        return;
    }
    fclose(f);

    // display temporary html file in Help tab
    ShowHelp(htmlfile.c_str());
}

// -----------------------------------------------------------------------------

void OpenFile(const char* path, bool remember)
{
    // convert path to a full path if necessary
    std::string fullpath = path;
    if (path[0] != '/') {
        if (fullpath.find("Patterns/") == 0) {
            // Patterns directory is inside supplieddir
            fullpath = supplieddir + fullpath;
        } else {
            fullpath = userdir + fullpath;
        }
    }

    if (IsHTMLFile(path)) {
        // show HTML file in Help tab
        ShowHelp(fullpath.c_str());
        return;
    }

    if (IsTextFile(path)) {
        // show text file using InfoViewController
        ShowTextFile(fullpath.c_str());
        return;
    }

    if (IsScriptFile(path)) {
        // execute script
        /*!!!
        if (remember) AddRecentScript(path);
        RunScript(path);
        */
        Warning("This version of Golly cannot run scripts.");
        return;
    }

    if (IsZipFile(path)) {
        // process zip file
        if (remember) AddRecentPattern(path);   // treat zip file like a pattern file
        OpenZipFile(fullpath.c_str());          // must use full path
        return;
    }


    if (IsRuleFile(path)) {
        // switch to rule (.rule file must be in rulesdir or userrules)
        SwitchToPatternTab();
        std::string basename = GetBaseName(path);
        LoadRule(basename.substr(0, basename.rfind('.')));
        return;
    }

    // anything else is a pattern file
    if (remember) AddRecentPattern(path);
    std::string basename = GetBaseName(path);
    // best to switch to Pattern tab first in case progress view appears
    SwitchToPatternTab();
    LoadPattern(fullpath.c_str(), basename.c_str());
}

// -----------------------------------------------------------------------------

const char* WritePattern(const char* path,
                         pattern_format format,
                         output_compression compression,
                         int top, int left, int bottom, int right)
{
    // if the format is RLE_format and the grid is bounded then force XRLE_format so that
    // position info is recorded (this position will be used when the file is read)
    if (format == RLE_format && (currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0))
        format = XRLE_format;
    const char* err = writepattern(path, *currlayer->algo, format,
                                   compression, top, left, bottom, right);
    return err;
}

// -----------------------------------------------------------------------------

void SaveSucceeded(const std::string& path)
{
    // save old info for RememberNameChange
    std::string oldname = currlayer->currname;
    std::string oldfile = currlayer->currfile;
    bool oldsave = currlayer->savestart;
    bool olddirty = currlayer->dirty;

    //!!! if (allowundo && !currlayer->stayclean) SavePendingChanges();

    if ( currlayer->algo->getGeneration() == currlayer->startgen ) {
        // no need to save starting pattern (ResetPattern can load currfile)
        currlayer->currfile = path;
        currlayer->savestart = false;
    }

    // set dirty flag false and update currlayer->currname
    std::string basename = GetBaseName(path.c_str());
    MarkLayerClean(basename.c_str());

    if (allowundo && !currlayer->stayclean) {
        currlayer->undoredo->RememberNameChange(oldname.c_str(), oldfile.c_str(), oldsave, olddirty);
    }
}

// -----------------------------------------------------------------------------

bool SavePattern(const std::string& path, pattern_format format, output_compression compression)
{
    bigint top, left, bottom, right;
    int itop, ileft, ibottom, iright;
    currlayer->algo->findedges(&top, &left, &bottom, &right);

    if (currlayer->algo->hyperCapable()) {
        // algorithm uses hashlife
        if ( OutsideLimits(top, left, bottom, right) ) {
            // too big so only allow saving as MC file
            if (format != MC_format) {
                Warning("Pattern is outside +/- 10^9 boundary and can't be saved in RLE format.");
                return false;
            }
            itop = ileft = ibottom = iright = 0;
        } else {
            // allow saving as MC or RLE file
            itop = top.toint();
            ileft = left.toint();
            ibottom = bottom.toint();
            iright = right.toint();
        }
    } else {
        // allow saving file only if pattern is small enough
        if ( OutsideLimits(top, left, bottom, right) ) {
            Warning("Pattern is outside +/- 10^9 boundary and can't be saved.");
            return false;
        }
        itop = top.toint();
        ileft = left.toint();
        ibottom = bottom.toint();
        iright = right.toint();
    }

    const char* err = WritePattern(path.c_str(), format, compression, itop, ileft, ibottom, iright);
    if (err) {
        Warning(err);
        return false;
    } else {
        std::string msg = "Pattern saved as ";
        msg += GetBaseName(path.c_str());
        DisplayMessage(msg.c_str());
        AddRecentPattern(path.c_str());
        SaveSucceeded(path);
        return true;
    }
}

// -----------------------------------------------------------------------------

void GetURL(const std::string& url, const std::string& pageurl)
{
    const char* HTML_PREFIX = "GET-";   // prepended to html filename

#ifdef ANDROID_GUI
    // LOGI("GetURL url=%s\n", url.c_str());
    // LOGI("GetURL pageurl=%s\n", pageurl.c_str());
#endif
#ifdef IOS_GUI
    // NSLog(@"GetURL url = %s", url.c_str());
    // NSLog(@"GetURL pageurl = %s", pageurl.c_str());
#endif

    std::string fullurl;
    if (url.find("http:") == 0) {
        fullurl = url;
    } else {
        // relative get, so prepend full prefix extracted from pageurl
        std::string urlprefix = GetBaseName(pageurl.c_str());
        // replace HTML_PREFIX with "http://" and convert spaces to '/'
        // (ie. reverse what we do below when building filepath)
        urlprefix.erase(0, strlen(HTML_PREFIX));
        urlprefix = "http://" + urlprefix;
        std::replace(urlprefix.begin(), urlprefix.end(), ' ', '/');
        urlprefix = urlprefix.substr(0, urlprefix.rfind('/')+1);
        fullurl = urlprefix + url;
    }

    std::string filename = GetBaseName(fullurl.c_str());
    // remove ugly stuff at start of file names downloaded from ConwayLife.com
    if (filename.find("download.php?f=") == 0 ||
        filename.find("pattern.asp?p=") == 0 ||
        filename.find("script.asp?s=") == 0) {
        filename = filename.substr( filename.find('=')+1 );
    }

    // create full path for downloaded file based on given url;
    // first remove initial "http://"
    std::string filepath = fullurl.substr( fullurl.find('/')+1 );
    while (filepath[0] == '/') filepath.erase(0,1);
    if (IsHTMLFile(filename)) {
        // create special name for html file so above code can extract it and set urlprefix
        std::replace(filepath.begin(), filepath.end(), '/', ' ');
        #ifdef ANDROID_GUI
            // replace "?" with something else to avoid problem in Android's WebView.loadUrl
            std::replace(filepath.begin(), filepath.end(), '?', '$');
        #endif
        filepath = HTML_PREFIX + filepath;
    } else {
        // no need for url info in file name
        filepath = filename;
    }

    if (IsRuleFile(filename)) {
        // create file in user's rules directory
        filepath = userrules + filename;
    } else if (IsHTMLFile(filename)) {
        // nicer to store html files in temporary directory
        filepath = tempdir + filepath;
    } else {
        // all other files are stored in user's download directory
        filepath = downloaddir + filepath;
    }

    // download the file and store it in filepath
    if (DownloadFile(fullurl, filepath)) {
        ProcessDownload(filepath);
    }
}

// -----------------------------------------------------------------------------

bool DownloadFile(const std::string& url, const std::string& filepath)
{
#ifdef ANDROID_GUI
    AndroidDownloadFile(url, filepath);
    // on Android the file is downloaded asynchronously, so we need to return false
    // here so that GetURL won't call ProcessDownload immediately (it will be called
    // later if the download succeeds)
    return false;
#endif

#ifdef WEB_GUI
    return WebDownloadFile(url, filepath);
#endif

#ifdef IOS_GUI
    NSURL *nsurl = [NSURL URLWithString:[NSString stringWithCString:url.c_str() encoding:NSUTF8StringEncoding]];
    if (nsurl == nil) {
        std::string msg = "Bad URL: " + url;
        Warning(msg.c_str());
        return false;
    }
    NSData *urlData = [NSData dataWithContentsOfURL:nsurl];
    if (urlData) {
        [urlData writeToFile:[NSString stringWithCString:filepath.c_str() encoding:NSUTF8StringEncoding] atomically:YES];
        return true;
    } else {
        Warning("Failed to download file!");
        return false;
    }
#endif
}

// -----------------------------------------------------------------------------

void ProcessDownload(const std::string& filepath)
{
    // process a successfully downloaded file
    std::string filename = GetBaseName(filepath.c_str());
    if (IsHTMLFile(filename)) {
        // display html file in Help tab
        ShowHelp(filepath.c_str());

    } else if (IsRuleFile(filename)) {
        // load corresponding rule
        SwitchToPatternTab();
        LoadRule(filename.substr(0, filename.rfind('.')));

    } else if (IsTextFile(filename)) {
        // open text file in modal view
        ShowTextFile(filepath.c_str());

    } else if (IsScriptFile(filename)) {
        // run script depending on safety check; if it is allowed to run
        // then we remember script in the Run Recent submenu
        //!!! CheckBeforeRunning(filepath, true, wxEmptyString);
        Warning("This version of Golly cannot run scripts.");

    } else {
        // assume it's a pattern/zip file, so open it
        OpenFile(filepath.c_str());
    }
}

// -----------------------------------------------------------------------------

void LoadLexiconPattern(const std::string& lexpattern)
{
    // copy lexpattern data to tempstart file
    FILE* f = fopen(currlayer->tempstart.c_str(), "w");
    if (f) {
        if (fputs(lexpattern.c_str(), f) == EOF) {
            fclose(f);
            Warning("Could not write lexicon pattern to tempstart file!");
            return;
        }
    } else {
        Warning("Could not create tempstart file!");
        return;
    }
    fclose(f);

    // avoid any pattern conversion (possibly causing ChangeAlgorithm to beep with a message)
    NewPattern();

    // all Life Lexicon patterns assume we're using Conway's Life so try
    // switching to B3/S23 or Life; if that fails then switch to QuickLife
    const char* err = currlayer->algo->setrule("B3/S23");
    if (err) {
        // try "Life" in case current algo is RuleLoader and Life.rule/table/tree exists
        err = currlayer->algo->setrule("Life");
    }
    if (err) {
        ChangeAlgorithm(QLIFE_ALGO, "B3/S23");
    }

    // load lexicon pattern
    SwitchToPatternTab();
    LoadPattern(currlayer->tempstart.c_str(), "lexicon");
}
