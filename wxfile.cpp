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

#include "wx/file.h"       // for wxFile
#include "wx/filename.h"   // for wxFileName
#include "wx/menuitem.h"   // for SetText
#include "wx/clipbrd.h"    // for wxTheClipboard
#include "wx/dataobj.h"    // for wxTextDataObject
#include "wx/zipstrm.h"    // for wxZipEntry, wxZipInputStream
#include "wx/wfstream.h"   // for wxFFileInputStream

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "readpattern.h"   // for readpattern
#include "writepattern.h"  // for writepattern, pattern_format

#include "wxgolly.h"       // for wxGetApp, statusptr, viewptr, bigview
#include "wxutils.h"       // for Warning
#include "wxprefs.h"       // for SavePrefs, allowundo, userrules, etc
#include "wxrule.h"        // for GetRuleName
#include "wxinfo.h"        // for GetInfoFrame
#include "wxstatus.h"      // for statusptr->...
#include "wxview.h"        // for viewptr->...
#include "wxrender.h"      // for SetSelectionColor
#include "wxscript.h"      // for RunScript, inscript
#include "wxmain.h"        // for MainFrame, etc
#include "wxundo.h"        // for currlayer->undoredo->...
#include "wxalgos.h"       // for CreateNewUniverse, algo_type, algoinfo, etc
#include "wxlayer.h"       // for currlayer, etc
#include "wxhelp.h"        // for ShowHelp
#include "wxtimeline.h"    // for InitTimelineFrame, ToggleTimelineBar, etc

#ifdef __WXMAC__
   #include <Carbon/Carbon.h>                      // for OpaqueWindowPtr, etc
   #include "wx/mac/corefoundation/cfstring.h"     // for wxMacCFStringHolder
#endif

#ifdef __WXMAC__
   // convert path to decomposed UTF8 so fopen will work
   #define FILEPATH path.fn_str()
#else
   #define FILEPATH path.mb_str(wxConvLocal)
#endif

// File menu functions:

// -----------------------------------------------------------------------------

wxString MainFrame::GetBaseName(const wxString& path)
{
   // extract basename from given path
   return path.AfterLast(wxFILE_SEP_PATH);
}

// -----------------------------------------------------------------------------

void MainFrame::MySetTitle(const wxString& title)
{
   #ifdef __WXMAC__
      // avoid wxMac's SetTitle call -- it causes an undesirable window refresh
      SetWindowTitleWithCFString((OpaqueWindowPtr*)this->MacGetWindowRef(),
                                 wxMacCFStringHolder(title, wxFONTENCODING_DEFAULT));
   #else
      SetTitle(title);
   #endif
}

// -----------------------------------------------------------------------------

void MainFrame::SetWindowTitle(const wxString& filename)
{
   if ( !filename.IsEmpty() ) {
      // remember current file name
      currlayer->currname = filename;
      // show currname in current layer's menu item
      UpdateLayerItem(currindex);
   }

   if (inscript) {
      // avoid window title flashing; eg. script might be switching layers
      ShowTitleLater();
      return;
   }

   wxString prefix = wxEmptyString;

   // display asterisk if pattern has been modified
   if (currlayer->dirty) prefix += wxT("*");

   int cid = currlayer->cloneid;
   while (cid > 0) {
      // display one or more "=" chars to indicate this is a cloned layer
      prefix += wxT("=");
      cid--;
   }

   wxString rule = GetRuleName( wxString(currlayer->algo->getrule(),wxConvLocal) );
   wxString wtitle;
   #ifdef __WXMAC__
      wtitle.Printf(_("%s%s [%s]"),
                     prefix.c_str(), currlayer->currname.c_str(), rule.c_str());
   #else
      wtitle.Printf(_("%s%s [%s] - Golly"),
                     prefix.c_str(), currlayer->currname.c_str(), rule.c_str());
   #endif

   // nicer to truncate a really long title???
   MySetTitle(wtitle);
}

// -----------------------------------------------------------------------------

void MainFrame::SetGenIncrement()
{
   if (currlayer->currexpo > 0) {
      bigint inc = 1;
      // set increment to currbase^currexpo
      int i = currlayer->currexpo;
      while (i > 0) {
         inc.mul_smallint(currlayer->currbase);
         i--;
      }
      currlayer->algo->setIncrement(inc);
   } else {
      currlayer->algo->setIncrement(1);
   }
}

// -----------------------------------------------------------------------------

void MainFrame::CreateUniverse()
{
   // save current rule
   wxString oldrule = wxString(currlayer->algo->getrule(), wxConvLocal);
   
   // delete old universe and create new one of same type
   delete currlayer->algo;
   currlayer->algo = CreateNewUniverse(currlayer->algtype);
   
   // ensure new universe uses same rule (and thus same # of cell states)
   currlayer->algo->setrule( oldrule.mb_str(wxConvLocal) );

   // increment has been reset to 1 but that's probably not always desirable
   // so set increment using current step size
   SetGenIncrement();
}

// -----------------------------------------------------------------------------

void MainFrame::NewPattern(const wxString& title)
{
   if (generating) {
      // terminate generating loop and set command_pending flag
      Stop();
      command_pending = true;
      cmdevent.SetId(wxID_NEW);
      return;
   }
   
   if (askonnew && !inscript && currlayer->dirty && !SaveCurrentLayer()) return;

   if (inscript) stop_after_script = true;
   currlayer->savestart = false;
   currlayer->currfile.Clear();
   currlayer->startgen = 0;
   
   // reset step size before CreateUniverse calls SetGenIncrement
   currlayer->currbase = algoinfo[currlayer->algtype]->defbase;
   currlayer->currexpo = 0;
   
   // create new, empty universe of same type and using same rule
   CreateUniverse();

   // reset timing info used in DisplayTimingInfo
   endtime = begintime = 0;

   // clear all undo/redo history
   currlayer->undoredo->ClearUndoRedo();

   // rule doesn't change so no need to call setrule

   if (newremovesel) currlayer->currsel.Deselect();
   if (newcurs) currlayer->curs = newcurs;
   viewptr->SetPosMag(bigint::zero, bigint::zero, newmag);

   // best to restore true origin
   if (currlayer->originx != bigint::zero || currlayer->originy != bigint::zero) {
      currlayer->originx = 0;
      currlayer->originy = 0;
      statusptr->SetMessage(origin_restored);
   }
   
   // restore default colors for current algo/rule
   UpdateLayerColors();

   MarkLayerClean(title);     // calls SetWindowTitle
   UpdateEverything();
}

// -----------------------------------------------------------------------------

static bool IsImageFile(const wxString& path)
{
   wxString ext = path.AfterLast('.');
   // if path has no extension then ext == path
   if (ext == path) return false;

   // supported extensions match image handlers added in GollyApp::OnInit()
   return   ext.IsSameAs(wxT("bmp"),false) ||
            ext.IsSameAs(wxT("gif"),false) ||
            ext.IsSameAs(wxT("png"),false) ||
            ext.IsSameAs(wxT("tif"),false) ||
            ext.IsSameAs(wxT("tiff"),false) ||
            ext.IsSameAs(wxT("icons"),false) ||
            // we don't actually support JPEG files but let LoadImage handle them
            ext.IsSameAs(wxT("jpg"),false) ||
            ext.IsSameAs(wxT("jpeg"),false);
}

// -----------------------------------------------------------------------------

bool MainFrame::LoadImage(const wxString& path)
{
   // don't try to load JPEG file
   wxString ext = path.AfterLast('.');
   if ( ext.IsSameAs(wxT("jpg"),false) ||
        ext.IsSameAs(wxT("jpeg"),false) ) {
      Warning(_("Golly cannot import JPEG data, only BMP/GIF/PNG/TIFF."));
      // pattern will be empty
      return true;
   }

   wxImage image;
   if ( image.LoadFile(path) ) {
      // don't change the current rule here -- that way the image can
      // be loaded into any algo
      unsigned char maskr, maskg, maskb;
      bool hasmask = image.GetOrFindMaskColour(&maskr, &maskg, &maskb);
      int wd = image.GetWidth();
      int ht = image.GetHeight();
      unsigned char* idata = image.GetData();
      int x, y;
      lifealgo* curralgo = currlayer->algo;
      for (y = 0; y < ht; y++) {
         for (x = 0; x < wd; x++) {
            long pos = (y * wd + x) * 3;
            unsigned char r = idata[pos];
            unsigned char g = idata[pos+1];
            unsigned char b = idata[pos+2];
            if ( hasmask && r == maskr && g == maskg && b == maskb ) {
               // treat transparent pixel as a dead cell
            } else if ( r < 255 || g < 255 || b < 255 ) {
               // treat non-white pixel as a live cell
               curralgo->setcell(x, y, 1);
            }
         }
      }
      curralgo->endofpattern();
   } else {
      Warning(_("Could not load image from file!"));
   }
   return true;
}

// -----------------------------------------------------------------------------

void MainFrame::LoadPattern(const wxString& path, const wxString& newtitle,
                            bool updatestatus, bool updateall)
{
   if ( !wxFileName::FileExists(path) ) {
      Warning(_("The file does not exist:\n") + path);
      return;
   }

   // newtitle is only empty if called from ResetPattern/RestorePattern
   if (!newtitle.IsEmpty()) {
      if (askonload && !inscript && currlayer->dirty && !SaveCurrentLayer()) return;

      if (inscript) stop_after_script = true;
      currlayer->savestart = false;
      currlayer->currfile = path;
      
      // reset step size now in case UpdateStatus is called below
      currlayer->currbase = algoinfo[currlayer->algtype]->defbase;
      currlayer->currexpo = 0;
      
      if (GetInfoFrame()) {
         // comments will no longer be relevant so close info window
         GetInfoFrame()->Close(true);
      }

      // reset timing info used in DisplayTimingInfo
      endtime = begintime = 0;

      // clear all undo/redo history
      currlayer->undoredo->ClearUndoRedo();
   }

   if (!showbanner) statusptr->ClearMessage();

   // set nopattupdate BEFORE UpdateStatus() call so we see gen=0 and pop=0;
   // in particular, it avoids getPopulation being called which would
   // slow down hlife pattern loading
   viewptr->nopattupdate = true;

   if (updatestatus) {
      // update all of status bar so we don't see different colored lines;
      // on Mac, DrawView also gets called if there are pending updates
      UpdateStatus();
   }

   // save current algo and rule
   algo_type oldalgo = currlayer->algtype;
   wxString oldrule = wxString(currlayer->algo->getrule(), wxConvLocal);
   
   // delete old universe and create new one of same type
   delete currlayer->algo;
   currlayer->algo = CreateNewUniverse(currlayer->algtype);

   if (!newtitle.IsEmpty()) {
      // show new file name in window title but no rule (which readpattern can change);
      // nicer if user can see file name while loading a very large pattern
      MySetTitle(_("Loading ") + newtitle);
   }

   if (IsImageFile(path)) {
      // ensure new universe uses same rule
      currlayer->algo->setrule( oldrule.mb_str(wxConvLocal) );
      LoadImage(path);
      viewptr->nopattupdate = false;
   } else {
      const char* err = readpattern(FILEPATH, *currlayer->algo);
      if (err) {
         // cycle thru all other algos until readpattern succeeds
         for (int i = 0; i < NumAlgos(); i++) {
            if (i != oldalgo) {
               currlayer->algtype = i;
               delete currlayer->algo;
               currlayer->algo = CreateNewUniverse(currlayer->algtype);
               // readpattern will call setrule
               err = readpattern(FILEPATH, *currlayer->algo);
               if (!err) break;
            }
         }
         viewptr->nopattupdate = false;
         if (err) {
            // no algo could read pattern so restore original algo and rule
            currlayer->algtype = oldalgo;
            delete currlayer->algo;
            currlayer->algo = CreateNewUniverse(currlayer->algtype);
            currlayer->algo->setrule( oldrule.mb_str(wxConvLocal) );
            // Warning( wxString(err,wxConvLocal) );
            // current error and original error are not necessarily meaningful
            // so report a more generic error
            Warning(_("File could not be loaded by any algorithm\n(probably due to an unknown rule)."));
         }
      }
      viewptr->nopattupdate = false;
   }

   if (!newtitle.IsEmpty()) {
      MarkLayerClean(newtitle);     // calls SetWindowTitle
      
      if (TimelineExists()) {
         // we've loaded a .mc file with a timeline so go to 1st frame
         InitTimelineFrame();
         if (!showtimeline) ToggleTimelineBar();
         // switch to the base step and exponent used to record the timeline
         pair<int, int> be = currlayer->algo->getbaseexpo();
         currlayer->currbase = be.first;
         currlayer->currexpo = be.second;
      } else {
         // restore default base step for current algo
         // (currlayer->currexpo was set to 0 above)
         currlayer->currbase = algoinfo[currlayer->algtype]->defbase;
      }
      SetGenIncrement();
   
      // restore default colors for current algo/rule
      UpdateLayerColors();

      if (openremovesel) currlayer->currsel.Deselect();
      if (opencurs) currlayer->curs = opencurs;

      viewptr->FitInView(1);
      currlayer->startgen = currlayer->algo->getGeneration();     // might be > 0
      if (updateall) UpdateEverything();
      showbanner = false;
   } else {
      // ResetPattern/RestorePattern does the update
   }
}

// -----------------------------------------------------------------------------

void MainFrame::CheckBeforeRunning(const wxString& scriptpath, bool remember,
                                   const wxString& zippath)
{
   bool ask;
   if (zippath.IsEmpty()) {
      // script was downloaded via "get:" link (script is in downloaddir --
      // see GetURL in wxhelp.cpp) so always ask user if it's okay to run
      ask = true;
   } else {
      // script is included in zip file (scriptpath starts with tempdir) so only
      // ask user if zip file was downloaded via "get:" link
      ask = zippath.StartsWith(downloaddir);
   }
   
   if (ask) {
      UpdateEverything();     // in case OpenZipFile called LoadPattern
      #ifdef __WXMAC__
         wxSetCursor(*wxSTANDARD_CURSOR);
      #endif
      // create our own dialog with a View button???  probably no need now that
      // user can ctrl/right-click on link to open script in their text editor
      wxString msg = scriptpath + _("\n\nClick \"No\" if the script is from an untrusted source.");
      int answer = wxMessageBox(msg, _("Do you want to run this script?"),
                                wxICON_QUESTION | wxYES_NO | wxNO_DEFAULT,
                                wxGetActiveWindow());
      switch (answer) {
         case wxYES: break;
         case wxNO:  return;
         default:    return;  // No
      }
   }
   
   // also do this???
   // save script info (download path or zip path + script entry) in list of safe scripts
   // (stored in prefs file) so we can search for this script and not ask again

   Raise();
   if (remember) AddRecentScript(scriptpath);
   RunScript(scriptpath);
}

// -----------------------------------------------------------------------------

bool MainFrame::ExtractZipEntry(const wxString& zippath,
                                const wxString& entryname,
                                const wxString& outfile)
{
   wxFFileInputStream instream(zippath);
   if (!instream.Ok()) {
      Warning(_("Could not create input stream for zip file:\n") + zippath);
      return false;
   }
   wxZipInputStream zip(instream);
   
   wxZipEntry* entry;
   while ((entry = zip.GetNextEntry()) != NULL) {
      wxString thisname = entry->GetName();
      if (thisname == entryname) {
         // we've found the desired entry so copy entry data to given output file
         wxFileOutputStream outstream(outfile);
         if (outstream.Ok()) {
            // read and write in chunks so we can show a progress dialog
            const int BUFFER_SIZE = 4000;
            char buf[BUFFER_SIZE];
            size_t incount = 0;
            size_t outcount = 0;
            size_t lastread, lastwrite;
            double filesize = (double) entry->GetSize();
            if (filesize <= 0.0) filesize = -1.0;        // show indeterminate progress
            
            BeginProgress(_("Extracting file"));
            while (true) {
               zip.Read(buf, BUFFER_SIZE);
               lastread = zip.LastRead();
               if (lastread == 0) break;
               outstream.Write(buf, lastread);
               lastwrite = outstream.LastWrite();
               incount += lastread;
               outcount += lastwrite;
               if (incount != outcount) {
                  Warning(_("Error occurred while writing file:\n") + outfile);
                  break;
               }
               char msg[128];
               sprintf(msg, "File size: %.2f MB", double(incount) / 1048576.0);
               if (AbortProgress((double)incount / filesize, wxString(msg,wxConvLocal))) {
                  outcount = 0;
                  break;
               }
            }
            EndProgress();
            
            if (incount == outcount) {
               // successfully copied entry data to outfile
               delete entry;
               return true;
            } else {
               // delete incomplete outfile
               if (wxFileExists(outfile)) wxRemoveFile(outfile);
            }
         } else {
            Warning(_("Could not open output stream for file:\n") + outfile);
         }
         delete entry;
         return false;
      }
      delete entry;
   }
   
   // should not get here
   Warning(_("Could not find zip file entry:\n") + entryname);
   return false;
}

// -----------------------------------------------------------------------------

void MainFrame::OpenZipFile(const wxString& zippath)
{
   // Process given zip file in the following manner:
   // - If it contains any rule files (.table/tree/colors/icons) then extract and
   //   install those files into userrules (the user's rules directory).
   // - If the zip file is "complex" (contains any folders, rule files, text files,
   //   or more than one pattern, or more than one script), build a temporary html
   //   file with clickable links to each file entry and show it in the help window.
   // - If the zip file contains at most one pattern and at most one script (both
   //   at the root level) then load the pattern (if present) and then run the script
   //   (if present and if allowed).
   
   const wxString indent = wxT("&nbsp;&nbsp;&nbsp;&nbsp;");
   bool dirseen = false;
   bool diffdirs = (userrules != rulesdir);
   wxString firstdir = wxEmptyString;
   wxString lastpattern = wxEmptyString;
   wxString lastscript = wxEmptyString;
   int patternseps = 0;                   // # of separators in lastpattern
   int scriptseps = 0;                    // # of separators in lastscript
   int patternfiles = 0;
   int scriptfiles = 0;
   int rulefiles = 0;
   int textfiles = 0;                     // includes html files
   
   wxString contents = wxT("<html><title>") + GetBaseName(zippath);
   contents += wxT("</title>\n");
   contents += wxT("<body bgcolor=\"#FFFFCE\">\n");
   contents += wxT("<p>\n");
   contents += wxT("Zip file: ");
   contents += zippath;
   contents += wxT("<p>\n");
   contents += wxT("Contents:<br>\n");
   
   wxFFileInputStream instream(zippath);
   if (!instream.Ok()) {
      Warning(_("Could not create input stream for zip file:\n") + zippath);
      return;
   }
   wxZipInputStream zip(instream);
   
   // examine each entry in zip file and build contents string;
   // also install any .table/tree/colors/icons files
   wxZipEntry* entry;
   while ((entry = zip.GetNextEntry()) != NULL) {
      wxString name = entry->GetName();      
      if (name.StartsWith(wxT("__MACOSX")) || name.EndsWith(wxT(".DS_Store"))) {
         // ignore meta-data stuff in zip file created on Mac
      } else {
         // indent depending on # of separators in name
         unsigned int sepcount = 0;
         unsigned int i = 0;
         unsigned int len = name.length();
         while (i < len) {
            if (name[i] == wxFILE_SEP_PATH) sepcount++;
            i++;
         }
         // check if 1st directory has multiple separators (eg. in jslife.zip)
         if (entry->IsDir() && !dirseen && sepcount > 1) {
            firstdir = name.BeforeFirst(wxFILE_SEP_PATH);
            contents += firstdir;
            contents += wxT("<br>\n");
         }
         for (i = 1; i < sepcount; i++) contents += indent;
         
         if (entry->IsDir()) {
            // remove terminating separator from directory name
            name = name.BeforeLast(wxFILE_SEP_PATH);
            name = name.AfterLast(wxFILE_SEP_PATH);
            if (dirseen && name == firstdir) {
               // ignore dir already output earlier (eg. in jslife.zip)
            } else {
               contents += name;
               contents += wxT("<br>\n");
            }
            dirseen = true;

         } else {
            // entry is for some sort of file
            wxString filename = name.AfterLast(wxFILE_SEP_PATH);

            // user can extract file via special "unzip:" link
            if (dirseen) contents += indent;
            contents += wxT("<a href=\"unzip:");
            contents += zippath;
            contents += wxT(":");
            contents += name;
            contents += wxT("\">");
            contents += filename;
            contents += wxT("</a>");
            
            if ( IsRuleFile(filename) ) {
               // extract and install .table/tree/colors/icons file into userrules
               wxString outfile = userrules + filename;
               wxFileOutputStream outstream(outfile);
               bool ok = outstream.Ok();
               if (ok) {
                  zip.Read(outstream);
                  ok = (outstream.GetLastError() == wxSTREAM_NO_ERROR);
               }
               if (ok) {
                  // file successfully installed
                  contents += indent;
                  contents += wxT("[installed]");
                  if (diffdirs) {
                     // check if this file overrides similarly named file in rulesdir
                     wxString clashfile = rulesdir + filename;
                     if (wxFileExists(clashfile)) {
                        contents += indent;
                        contents += wxT("(overrides file in Rules folder)");
                     }
                  }
               } else {
                  // file could not be installed
                  contents += indent;
                  contents += wxT("[NOT installed]");
                  // file is probably incomplete so best to delete it
                  if (wxFileExists(outfile)) wxRemoveFile(outfile);
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
            contents += wxT("<br>\n");
         }
      }
      delete entry;
   }  // end while

   if (rulefiles > 0) {
      contents += wxT("<p>Files marked as \"[installed]\" have been stored in your rules folder:<br>\n");
      contents += userrules;
      contents += wxT("\n");
   }
   contents += wxT("\n</body></html>");
   
   if (dirseen || rulefiles > 0 || textfiles > 0 || patternfiles > 1 || scriptfiles > 1) {
      // complex zip, so write contents to a temporary html file and display it in help window;
      // use a unique file name so user can go back/forwards
      wxString htmlfile = wxFileName::CreateTempFileName(tempdir + wxT("zip_contents_"));
      wxRemoveFile(htmlfile);
      htmlfile += wxT(".html");
      wxFile outfile(htmlfile, wxFile::write);
      if (outfile.IsOpened()) {
         outfile.Write(contents);
         outfile.Close();
         ShowHelp(htmlfile);
      } else {
         Warning(_("Could not create html file:\n") + htmlfile);
      }
   }
   
   if (patternfiles <= 1 && scriptfiles <= 1 && patternseps == 0 && scriptseps == 0) {
      // load lastpattern (if present), then run lastscript (if present);
      // the script might be a long-running one that allows user interaction,
      // so it's best to run it AFTER calling ShowHelp above
      if (patternfiles == 1) {
         wxString tempfile = tempdir + lastpattern.AfterLast(wxFILE_SEP_PATH);
         if (ExtractZipEntry(zippath, lastpattern, tempfile)) {
            Raise();
            // don't call AddRecentPattern(tempfile) here; OpenFile has added
            // zippath to recent patterns
            LoadPattern(tempfile, GetBaseName(tempfile), true, scriptfiles == 0);
         }
      }
      if (scriptfiles == 1) {
         wxString tempfile = tempdir + lastscript.AfterLast(wxFILE_SEP_PATH);
         if (ExtractZipEntry(zippath, lastscript, tempfile)) {
            // run script depending on safety check
            CheckBeforeRunning(tempfile, false, zippath);
         } else {
            // should never happen but play safe
            UpdateEverything();
         }
      }
   }
}

// -----------------------------------------------------------------------------

void MainFrame::OpenFile(const wxString& path, bool remember)
{
   if (IsHTMLFile(path)) {
      // show HTML file in help window
      ShowHelp(path);
      return;
   }
   
   if (IsTextFile(path)) {
      // open text file in user's preferred text editor
      EditFile(path);
      return;
   }

   if (generating) {
      // terminate generating loop and set command_pending flag
      Stop();
      command_pending = true;
      // assume remember is true (should only be false if called from a script)
      if ( IsScriptFile(path) ) {
         AddRecentScript(path);
         cmdevent.SetId(ID_RUN_RECENT + 1);
      } else {
         AddRecentPattern(path);
         cmdevent.SetId(ID_OPEN_RECENT + 1);
      }
      return;
   }
   
   if (IsScriptFile(path)) {
      // execute script
      if (remember) AddRecentScript(path);
      RunScript(path);

   } else if (IsZipFile(path)) {
      // process zip file
      if (remember) AddRecentPattern(path);     // treat it like a pattern
      OpenZipFile(path);
   
   } else {
      // load pattern
      if (remember) AddRecentPattern(path);
      
      // ensure path is a full path because a script might want to reset() to it
      // (in which case the cwd is the script's directory, not gollydir)
      wxString newpath = path;
      wxFileName fname(newpath);
      if (!fname.IsAbsolute()) newpath = gollydir + path;
      
      LoadPattern(newpath, GetBaseName(path));
   }
}

// -----------------------------------------------------------------------------

void MainFrame::AddRecentPattern(const wxString& inpath)
{
   if (inpath.IsEmpty()) return;
   wxString path = inpath;
   if (path.StartsWith(gollydir)) {
      // remove gollydir from start of path
      path.erase(0, gollydir.length());
   }

   // duplicate any ampersands so they appear in menu
   path.Replace(wxT("&"), wxT("&&"));

   // put given path at start of patternSubMenu
   #ifdef __WXGTK__
      // avoid wxGTK bug in FindItem if path contains underscores
      int id = wxNOT_FOUND;
      for (int i = 0; i < numpatterns; i++) {
         wxMenuItem* item = patternSubMenu->FindItemByPosition(i);
         wxString temp = item->GetText();
         temp.Replace(wxT("__"), wxT("_"));
         temp.Replace(wxT("&"), wxT("&&"));
         if (temp == path) {
            id = ID_OPEN_RECENT + 1 + i;
            break;
         }
      }
   #else
      int id = patternSubMenu->FindItem(path);
   #endif
   if ( id == wxNOT_FOUND ) {
      if ( numpatterns < maxpatterns ) {
         // add new path
         numpatterns++;
         id = ID_OPEN_RECENT + numpatterns;
         patternSubMenu->Insert(numpatterns - 1, id, path);
      } else {
         // replace last item with new path
         wxMenuItem* item = patternSubMenu->FindItemByPosition(maxpatterns - 1);
         item->SetText(path);
         id = ID_OPEN_RECENT + maxpatterns;
      }
   }
   // path exists in patternSubMenu
   if ( id > ID_OPEN_RECENT + 1 ) {
      // move path to start of menu
      wxMenuItem* item;
      while ( id > ID_OPEN_RECENT + 1 ) {
         wxMenuItem* previtem = patternSubMenu->FindItem(id - 1);
         wxString prevpath = previtem->GetText();
         #ifdef __WXGTK__
            // remove duplicate underscores
            prevpath.Replace(wxT("__"), wxT("_"));
            prevpath.Replace(wxT("&"), wxT("&&"));
         #endif
         item = patternSubMenu->FindItem(id);
         item->SetText(prevpath);
         id--;
      }
      item = patternSubMenu->FindItem(id);
      item->SetText(path);
   }
}

// -----------------------------------------------------------------------------

void MainFrame::AddRecentScript(const wxString& inpath)
{
   if (inpath.IsEmpty()) return;
   wxString path = inpath;
   if (path.StartsWith(gollydir)) {
      // remove gollydir from start of path
      path.erase(0, gollydir.length());
   }

   // duplicate ampersands so they appear in menu
   path.Replace(wxT("&"), wxT("&&"));

   // put given path at start of scriptSubMenu
   #ifdef __WXGTK__
      // avoid wxGTK bug in FindItem if path contains underscores
      int id = wxNOT_FOUND;
      for (int i = 0; i < numscripts; i++) {
         wxMenuItem* item = scriptSubMenu->FindItemByPosition(i);
         wxString temp = item->GetText();
         temp.Replace(wxT("__"), wxT("_"));
         temp.Replace(wxT("&"), wxT("&&"));
         if (temp == path) {
            id = ID_RUN_RECENT + 1 + i;
            break;
         }
      }
   #else
      int id = scriptSubMenu->FindItem(path);
   #endif
   if ( id == wxNOT_FOUND ) {
      if ( numscripts < maxscripts ) {
         // add new path
         numscripts++;
         id = ID_RUN_RECENT + numscripts;
         scriptSubMenu->Insert(numscripts - 1, id, path);
      } else {
         // replace last item with new path
         wxMenuItem* item = scriptSubMenu->FindItemByPosition(maxscripts - 1);
         item->SetText(path);
         id = ID_RUN_RECENT + maxscripts;
      }
   }
   // path exists in scriptSubMenu
   if ( id > ID_RUN_RECENT + 1 ) {
      // move path to start of menu
      wxMenuItem* item;
      while ( id > ID_RUN_RECENT + 1 ) {
         wxMenuItem* previtem = scriptSubMenu->FindItem(id - 1);
         wxString prevpath = previtem->GetText();
         #ifdef __WXGTK__
            // remove duplicate underscores
            prevpath.Replace(wxT("__"), wxT("_"));
            prevpath.Replace(wxT("&"), wxT("&&"));
         #endif
         item = scriptSubMenu->FindItem(id);
         item->SetText(prevpath);
         id--;
      }
      item = scriptSubMenu->FindItem(id);
      item->SetText(path);
   }
}

// -----------------------------------------------------------------------------

void MainFrame::OpenPattern()
{
   if (generating) {
      // terminate generating loop and set command_pending flag
      Stop();
      command_pending = true;
      cmdevent.SetId(wxID_OPEN);
      return;
   }

   wxString filetypes = _("All files (*)|*");
   filetypes +=         _("|RLE (*.rle)|*.rle");
   filetypes +=         _("|Macrocell (*.mc)|*.mc");
   filetypes +=         _("|Life 1.05/1.06 (*.lif)|*.lif");
   filetypes +=         _("|dblife (*.l)|*.l");
   filetypes +=         _("|MCell (*.mcl)|*.mcl");
   filetypes +=         _("|Gzip (*.gz)|*.gz");
   filetypes +=         _("|Zip (*.zip;*.gar)|*.zip;*.gar");
   filetypes +=         _("|BMP (*.bmp)|*.bmp");
   filetypes +=         _("|GIF (*.gif)|*.gif");
   filetypes +=         _("|PNG (*.png)|*.png");
   filetypes +=         _("|TIFF (*.tiff;*.tif)|*.tiff;*.tif");

   wxFileDialog opendlg(this, _("Choose a pattern"),
                        opensavedir, wxEmptyString, filetypes,
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);

   #ifdef __WXGTK__
      // opensavedir is ignored above (bug in wxGTK 2.8.0???)
      opendlg.SetDirectory(opensavedir);
   #endif

   if ( opendlg.ShowModal() == wxID_OK ) {
      wxFileName fullpath( opendlg.GetPath() );
      opensavedir = fullpath.GetPath();
      OpenFile( opendlg.GetPath() );
   }
}

// -----------------------------------------------------------------------------

void MainFrame::OpenScript()
{
   if (generating) {
      // terminate generating loop and set command_pending flag
      Stop();
      command_pending = true;
      cmdevent.SetId(ID_RUN_SCRIPT);
      return;
   }

   wxString filetypes = _("Perl or Python (*.pl;*.py)|*.pl;*.py");
   filetypes +=         _("|Perl (*.pl)|*.pl");
   filetypes +=         _("|Python (*.py)|*.py");

   wxFileDialog opendlg(this, _("Choose a script"),
                        rundir, wxEmptyString, filetypes,
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);

   #ifdef __WXGTK__
      // rundir is ignored above (bug in wxGTK 2.8.0???)
      opendlg.SetDirectory(rundir);
   #endif

   if ( opendlg.ShowModal() == wxID_OK ) {
      wxFileName fullpath( opendlg.GetPath() );
      rundir = fullpath.GetPath();
      AddRecentScript( opendlg.GetPath() );
      RunScript( opendlg.GetPath() );
   }
}

// -----------------------------------------------------------------------------

bool MainFrame::CopyTextToClipboard(const wxString& text)
{
   bool result = true;
   if (wxTheClipboard->Open()) {
      if ( !wxTheClipboard->SetData(new wxTextDataObject(text)) ) {
         Warning(_("Could not copy text to clipboard!"));
         result = false;
      }
      wxTheClipboard->Close();
   } else {
      Warning(_("Could not open clipboard!"));
      result = false;
   }
   return result;
}

// -----------------------------------------------------------------------------

bool MainFrame::GetTextFromClipboard(wxTextDataObject* textdata)
{
   bool gotdata = false;

   if ( wxTheClipboard->Open() ) {
      if ( wxTheClipboard->IsSupported( wxDF_TEXT ) ) {
         gotdata = wxTheClipboard->GetData( *textdata );
         if (!gotdata) {
            statusptr->ErrorMessage(_("Could not get clipboard text!"));
         }

      } else if ( wxTheClipboard->IsSupported( wxDF_BITMAP ) ) {
         wxBitmapDataObject bmapdata;
         gotdata = wxTheClipboard->GetData( bmapdata );
         if (gotdata) {
            // convert bitmap data to text data
            wxString str;
            wxBitmap bmap = bmapdata.GetBitmap();
            wxImage image = bmap.ConvertToImage();
            if (image.Ok()) {
               /* there doesn't seem to be any mask or alpha info, at least on Mac
               if (bmap.GetMask() != NULL) Warning(_("Bitmap has mask!"));
               if (image.HasMask()) Warning(_("Image has mask!"));
               if (image.HasAlpha()) Warning(_("Image has alpha!"));
               */
               int wd = image.GetWidth();
               int ht = image.GetHeight();
               unsigned char* idata = image.GetData();
               int x, y;
               for (y = 0; y < ht; y++) {
                  for (x = 0; x < wd; x++) {
                     long pos = (y * wd + x) * 3;
                     if ( idata[pos] < 255 || idata[pos+1] < 255 || idata[pos+2] < 255 ) {
                        // non-white pixel is a live cell
                        str += 'o';
                     } else {
                        // white pixel is a dead cell
                        str += '.';
                     }
                  }
                  str += '\n';
               }
               textdata->SetText(str);
            } else {
               statusptr->ErrorMessage(_("Could not convert clipboard bitmap!"));
               gotdata = false;
            }
         } else {
            statusptr->ErrorMessage(_("Could not get clipboard bitmap!"));
         }

      } else {
         statusptr->ErrorMessage(_("No data in clipboard."));
      }
      wxTheClipboard->Close();

   } else {
      statusptr->ErrorMessage(_("Could not open clipboard!"));
   }

   return gotdata;
}

// -----------------------------------------------------------------------------

void MainFrame::OpenClipboard()
{
   if (generating) {
      // terminate generating loop and set command_pending flag
      Stop();
      command_pending = true;
      cmdevent.SetId(ID_OPEN_CLIP);
      return;
   }

   // load and view pattern data stored in clipboard
   wxTextDataObject data;
   if (GetTextFromClipboard(&data)) {
      // copy clipboard data to tempstart so we can handle all formats
      // supported by readpattern
      wxFile outfile(currlayer->tempstart, wxFile::write);
      if ( outfile.IsOpened() ) {
         outfile.Write( data.GetText() );
         outfile.Close();
         LoadPattern(currlayer->tempstart, _("clipboard"));
         // do NOT delete tempstart -- it can be reloaded by ResetPattern
         // or used by ShowPatternInfo
      } else {
         statusptr->ErrorMessage(_("Could not create tempstart file!"));
      }
   }
}

// -----------------------------------------------------------------------------

wxString MainFrame::GetScriptFileName(const wxString& text)
{
   // examine given text to see if it contains Perl or Python code;
   // if "use" or "my" occurs at start of line then we assume Perl,
   // if "import" or "from" occurs at start of line then we assume Python,
   // otherwise we compare counts for dollars + semicolons vs colons
   int dollars = 0;
   int semicolons = 0;
   int colons = 0;
   int linelen = 0;

   // need to be careful converting Unicode wxString to char*
   wxCharBuffer buff = text.mb_str(wxConvLocal);
   const char* p = (const char*) buff;
   while (*p) {
      switch (*p) {
         case '#':
            // probably a comment, so ignore rest of line
            while (*p && *p != 13 && *p != 10) p++;
            linelen = 0;
            if (*p) p++;
            break;
         case 34: // double quote -- ignore until quote closes, even multiple lines
            p++;
            while (*p && *p != 34) p++;
            linelen = 0;
            if (*p) p++;
            break;
         case 39: // single quote -- ignore until quote closes
            p++;
            while (*p && *p != 39 && *p != 13 && *p != 10) p++;
            linelen = 0;
            if (*p) p++;
            break;
         case '$': dollars++; linelen++; p++;
            break;
         case ':': colons++; linelen++; p++;
            break;
         case ';': semicolons++; linelen++; p++;
            break;
         case 13: case 10:
            // if colon/semicolon is at eol then count it twice
            if (linelen > 0 && p[-1] == ':') colons++;
            if (linelen > 0 && p[-1] == ';') semicolons++;
            linelen = 0;
            p++;
            break;
         case ' ':
            // look for language-specific keyword at start of line
            if (linelen == 2 && strncmp(p-2,"my",2) == 0) return perlfile;
            if (linelen == 3 && strncmp(p-3,"use",3) == 0) return perlfile;
            if (linelen == 4 && strncmp(p-4,"from",4) == 0) return pythonfile;
            if (linelen == 6 && strncmp(p-6,"import",6) == 0) return pythonfile;
            // don't break
         default:
            if (linelen == 0 && (*p == ' ' || *p == 9)) {
               // ignore spaces/tabs at start of line
            } else {
               linelen++;
            }
            p++;
      }
   }

   /* check totals:
   char msg[128];
   sprintf(msg, "dollars=%d semicolons=%d colons=%d", dollars, semicolons, colons);
   Note(wxString(msg,wxConvLocal));
   */

   if (dollars + semicolons > colons)
      return perlfile;
   else
      return pythonfile;
}

// -----------------------------------------------------------------------------

void MainFrame::RunClipboard()
{
   if (generating) {
      // terminate generating loop and set command_pending flag
      Stop();
      command_pending = true;
      cmdevent.SetId(ID_RUN_CLIP);
      return;
   }

   // run script stored in clipboard
   wxTextDataObject data;
   if (GetTextFromClipboard(&data)) {
      // scriptfile extension depends on whether the clipboard data
      // contains Perl or Python code
      wxString scriptfile = GetScriptFileName( data.GetText() );
      // copy clipboard data to scriptfile
      wxFile outfile(scriptfile, wxFile::write);
      if (outfile.IsOpened()) {
         #ifdef __WXMAC__
            if (scriptfile == perlfile) {
               // Perl script, so replace CRs with LFs
               wxString str = data.GetText();
               str.Replace(wxT("\015"), wxT("\012"));
               outfile.Write( str );
            } else {
               outfile.Write( data.GetText() );
            }
         #else
            outfile.Write( data.GetText() );
         #endif
         outfile.Close();
         RunScript(scriptfile);
      } else {
         statusptr->ErrorMessage(_("Could not create script file!"));
      }
   }
}

// -----------------------------------------------------------------------------

void MainFrame::OpenRecentPattern(int id)
{
   if (generating) {
      // terminate generating loop and set command_pending flag
      Stop();
      command_pending = true;
      cmdevent.SetId(id);
      return;
   }

   wxMenuItem* item = patternSubMenu->FindItem(id);
   if (item) {
      wxString path = item->GetText();
      #ifdef __WXGTK__
         // remove duplicate underscores
         path.Replace(wxT("__"), wxT("_"));
      #endif
      // remove duplicate ampersands
      path.Replace(wxT("&&"), wxT("&"));

      // if path isn't absolute then prepend Golly directory
      wxFileName fname(path);
      if (!fname.IsAbsolute()) path = gollydir + path;

      // path might be a zip file so call OpenFile rather than LoadPattern
      OpenFile(path);
      /*
      AddRecentPattern(path);
      LoadPattern(path, GetBaseName(path));
      */
   }
}

// -----------------------------------------------------------------------------

void MainFrame::OpenRecentScript(int id)
{
   if (generating) {
      // terminate generating loop and set command_pending flag
      Stop();
      command_pending = true;
      cmdevent.SetId(id);
      return;
   }

   wxMenuItem* item = scriptSubMenu->FindItem(id);
   if (item) {
      wxString path = item->GetText();
      #ifdef __WXGTK__
         // remove duplicate underscores
         path.Replace(wxT("__"), wxT("_"));
      #endif
      // remove duplicate ampersands
      path.Replace(wxT("&&"), wxT("&"));

      // if path isn't absolute then prepend Golly directory
      wxFileName fname(path);
      if (!fname.IsAbsolute()) path = gollydir + path;

      AddRecentScript(path);
      RunScript(path);
   }
}

// -----------------------------------------------------------------------------

void MainFrame::ClearMissingPatterns()
{
   int pos = 0;
   while (pos < numpatterns) {
      wxMenuItem* item = patternSubMenu->FindItemByPosition(pos);
      wxString path = item->GetText();
      #ifdef __WXGTK__
         // remove duplicate underscores
         path.Replace(wxT("__"), wxT("_"));
      #endif
      // remove duplicate ampersands
      path.Replace(wxT("&&"), wxT("&"));

      // if path isn't absolute then prepend Golly directory
      wxFileName fname(path);
      if (!fname.IsAbsolute()) path = gollydir + path;

      if (wxFileExists(path)) {
         // keep this item
         pos++;
      } else {
         // remove this item by shifting up later items
         int nextpos = pos + 1;
         while (nextpos < numpatterns) {
            wxMenuItem* nextitem = patternSubMenu->FindItemByPosition(nextpos);
            #ifdef __WXGTK__
               // avoid wxGTK bug if item contains underscore
               wxString temp = nextitem->GetText();
               temp.Replace(wxT("__"), wxT("_"));
               temp.Replace(wxT("&"), wxT("&&"));
               item->SetText( temp );
            #else
               item->SetText( nextitem->GetText() );
            #endif
            item = nextitem;
            nextpos++;
         }
         // delete last item
         patternSubMenu->Delete(item);
         numpatterns--;
      }
   }
   wxMenuBar* mbar = GetMenuBar();
   if (mbar) mbar->Enable(ID_OPEN_RECENT, numpatterns > 0);
}

// -----------------------------------------------------------------------------

void MainFrame::ClearMissingScripts()
{
   int pos = 0;
   while (pos < numscripts) {
      wxMenuItem* item = scriptSubMenu->FindItemByPosition(pos);
      wxString path = item->GetText();
      #ifdef __WXGTK__
         // remove duplicate underscores
         path.Replace(wxT("__"), wxT("_"));
      #endif
      // remove duplicate ampersands
      path.Replace(wxT("&&"), wxT("&"));

      // if path isn't absolute then prepend Golly directory
      wxFileName fname(path);
      if (!fname.IsAbsolute()) path = gollydir + path;

      if (wxFileExists(path)) {
         // keep this item
         pos++;
      } else {
         // remove this item by shifting up later items
         int nextpos = pos + 1;
         while (nextpos < numscripts) {
            wxMenuItem* nextitem = scriptSubMenu->FindItemByPosition(nextpos);
            #ifdef __WXGTK__
               // avoid wxGTK bug if item contains underscore
               wxString temp = nextitem->GetText();
               temp.Replace(wxT("__"), wxT("_"));
               temp.Replace(wxT("&"), wxT("&&"));
               item->SetText( temp );
            #else
               item->SetText( nextitem->GetText() );
            #endif
            item = nextitem;
            nextpos++;
         }
         // delete last item
         scriptSubMenu->Delete(item);
         numscripts--;
      }
   }
   wxMenuBar* mbar = GetMenuBar();
   if (mbar) mbar->Enable(ID_RUN_RECENT, numscripts > 0);
}

// -----------------------------------------------------------------------------

void MainFrame::ClearAllPatterns()
{
   while (numpatterns > 0) {
      patternSubMenu->Delete( patternSubMenu->FindItemByPosition(0) );
      numpatterns--;
   }
   wxMenuBar* mbar = GetMenuBar();
   if (mbar) mbar->Enable(ID_OPEN_RECENT, false);
}

// -----------------------------------------------------------------------------

void MainFrame::ClearAllScripts()
{
   while (numscripts > 0) {
      scriptSubMenu->Delete( scriptSubMenu->FindItemByPosition(0) );
      numscripts--;
   }
   wxMenuBar* mbar = GetMenuBar();
   if (mbar) mbar->Enable(ID_RUN_RECENT, false);
}

// -----------------------------------------------------------------------------

const char* MainFrame::WritePattern(const wxString& path,
                                    pattern_format format,
                                    int top, int left, int bottom, int right)
{
   // if the format is RLE_format and the grid is bounded then force XRLE_format so that
   // position info is recorded (this position will be used when the file is read)
   if (format == RLE_format && (currlayer->algo->gridwd > 0 || currlayer->algo->gridht > 0))
      format = XRLE_format;
   const char* err = writepattern(FILEPATH, *currlayer->algo, format,
                                  top, left, bottom, right);

   #ifdef __WXMAC__
      if (!err) {
         // set the file's creator and type
         wxFileName filename(path);
         wxUint32 creator = 'GoLy';
         wxUint32 type = 'GoLR';       // RLE or XRLE
         if (format == MC_format) {
            type = 'GoLM';
         } else if (format == L105_format) {
            type = 'GoLL';
         }
         filename.MacSetTypeAndCreator(type, creator);
      }
   #endif

   return err;
}

// -----------------------------------------------------------------------------

bool MainFrame::SavePattern()
{
   if (generating) {
      // terminate generating loop and set command_pending flag
      Stop();
      command_pending = true;
      cmdevent.SetId(wxID_SAVE);
      return false;
   }

   wxString filetypes;
   int RLEindex, L105index, MCindex;

   // initially all formats are not allowed (use any -ve number)
   RLEindex = L105index = MCindex = -1;

   bigint top, left, bottom, right;
   int itop, ileft, ibottom, iright;
   currlayer->algo->findedges(&top, &left, &bottom, &right);

   wxString RLEstring;
   if (savexrle)
      RLEstring = _("Extended RLE (*.rle)|*.rle");
   else
      RLEstring = _("RLE (*.rle)|*.rle");

   //!!! need currlayer->algo->CanWriteMC()???
   if (currlayer->algo->hyperCapable()) {
      if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
         // too big so only allow saving as MC file
         itop = ileft = ibottom = iright = 0;
         filetypes = _("Macrocell (*.mc)|*.mc");
         MCindex = 0;
      } else {
         // allow saving as RLE/MC file
         itop = top.toint();
         ileft = left.toint();
         ibottom = bottom.toint();
         iright = right.toint();
         filetypes = RLEstring;
         RLEindex = 0;
         filetypes += _("|Macrocell (*.mc)|*.mc");
         MCindex = 1;
      }
   } else {
      // allow saving file only if pattern is small enough
      if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
         statusptr->ErrorMessage(_("Pattern is outside +/- 10^9 boundary."));
         return false;
      }
      itop = top.toint();
      ileft = left.toint();
      ibottom = bottom.toint();
      iright = right.toint();
      filetypes = RLEstring;
      RLEindex = 0;
      /* Life 1.05 format not yet implemented!!!
      filetypes += _("|Life 1.05 (*.lif)|*.lif");
      L105index = 1;
      */
   }

   wxFileDialog savedlg( this, _("Save pattern"),
                         opensavedir, currlayer->currname, filetypes,
                         wxFD_SAVE | wxFD_OVERWRITE_PROMPT );

   #ifdef __WXGTK__
      // opensavedir is ignored above (bug in wxGTK 2.8.0???)
      savedlg.SetDirectory(opensavedir);
   #endif

   if ( savedlg.ShowModal() == wxID_OK ) {
      wxFileName fullpath( savedlg.GetPath() );
      opensavedir = fullpath.GetPath();
      wxString ext = fullpath.GetExt();
      pattern_format format;
      // if user supplied a known extension then use that format if it is
      // allowed, otherwise use current format specified in filter menu
      if ( ext.IsSameAs(wxT("rle"),false) && RLEindex >= 0 ) {
         format = savexrle ? XRLE_format : RLE_format;
      /* Life 1.05 format not yet implemented!!!
      } else if ( ext.IsSameAs("lif",false) && L105index >= 0 ) {
         format = L105_format;
      */
      } else if ( ext.IsSameAs(wxT("mc"),false) && MCindex >= 0 ) {
         format = MC_format;
      } else if ( savedlg.GetFilterIndex() == RLEindex ) {
         format = savexrle ? XRLE_format : RLE_format;
      } else if ( savedlg.GetFilterIndex() == L105index ) {
         format = L105_format;
      } else if ( savedlg.GetFilterIndex() == MCindex ) {
         format = MC_format;
      } else {
         statusptr->ErrorMessage(_("Bug in SavePattern!"));
         return false;
      }

      const char* err = WritePattern(savedlg.GetPath(), format,
                                     itop, ileft, ibottom, iright);
      if (err) {
         statusptr->ErrorMessage(wxString(err,wxConvLocal));
      } else {
         statusptr->DisplayMessage(_("Pattern saved in file: ") + savedlg.GetPath());
         AddRecentPattern(savedlg.GetPath());
         SaveSucceeded(savedlg.GetPath());
         return true;
      }
   }
   return false;
}

// -----------------------------------------------------------------------------

// called by script command to save current pattern to given file
const char* MainFrame::SaveFile(const wxString& path, const wxString& format, bool remember)
{
   // check that given format is valid and allowed
   bigint top, left, bottom, right;
   int itop, ileft, ibottom, iright;
   currlayer->algo->findedges(&top, &left, &bottom, &right);

   pattern_format pattfmt;
   if ( format.IsSameAs(wxT("rle"),false) ) {
      if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
         return "Pattern is too big to save as RLE.";
      }
      pattfmt = savexrle ? XRLE_format : RLE_format;
      itop = top.toint();
      ileft = left.toint();
      ibottom = bottom.toint();
      iright = right.toint();
   } else if ( format.IsSameAs(wxT("mc"),false) ) {
      //!!! need currlayer->algo->CanWriteMC()???
      if (!currlayer->algo->hyperCapable()) {
         return "Macrocell format is not supported by the current algorithm.";
      }
      pattfmt = MC_format;
      // writepattern will ignore itop, ileft, ibottom, iright
      itop = ileft = ibottom = iright = 0;
   } else {
      return "Unknown pattern format.";
   }

   const char* err = WritePattern(path, pattfmt, itop, ileft, ibottom, iright);
   if (!err) {
      if (remember) AddRecentPattern(path);
      SaveSucceeded(path);
   }

   return err;
}

// -----------------------------------------------------------------------------

void MainFrame::SaveSucceeded(const wxString& path)
{
   // save old info for RememberNameChange
   wxString oldname = currlayer->currname;
   wxString oldfile = currlayer->currfile;
   bool oldsave = currlayer->savestart;
   bool olddirty = currlayer->dirty;

   if (allowundo && !currlayer->stayclean && inscript) {
      SavePendingChanges();
   }

   if ( currlayer->algo->getGeneration() == currlayer->startgen ) {
      // no need to save starting pattern (ResetPattern can load currfile)
      currlayer->currfile = path;
      currlayer->savestart = false;
   }

   // set dirty flag false and update currlayer->currname
   MarkLayerClean(GetBaseName(path));

   if (allowundo && !currlayer->stayclean) {
      currlayer->undoredo->RememberNameChange(oldname, oldfile, oldsave, olddirty);
   }
}

// -----------------------------------------------------------------------------

void MainFrame::ToggleShowPatterns()
{
   if (splitwin->IsSplit()) dirwinwd = splitwin->GetSashPosition();
   #ifndef __WXMAC__
      // hide scroll bars
      bigview->SetScrollbar(wxHORIZONTAL, 0, 0, 0, true);
      bigview->SetScrollbar(wxVERTICAL, 0, 0, 0, true);
   #endif

   showpatterns = !showpatterns;
   if (showpatterns && showscripts) {
      showscripts = false;
      splitwin->Unsplit(scriptctrl);
      splitwin->SplitVertically(patternctrl, RightPane(), dirwinwd);
   } else {
      if (splitwin->IsSplit()) {
         // hide left pane
         splitwin->Unsplit(patternctrl);
      } else {
         splitwin->SplitVertically(patternctrl, RightPane(), dirwinwd);
      }
      viewptr->SetFocus();
   }

   #ifndef __WXMAC__
      // restore scroll bars
      bigview->UpdateScrollBars();
   #endif
}

// -----------------------------------------------------------------------------

void MainFrame::ToggleShowScripts()
{
   if (splitwin->IsSplit()) dirwinwd = splitwin->GetSashPosition();
   #ifndef __WXMAC__
      // hide scroll bars
      bigview->SetScrollbar(wxHORIZONTAL, 0, 0, 0, true);
      bigview->SetScrollbar(wxVERTICAL, 0, 0, 0, true);
   #endif

   showscripts = !showscripts;
   if (showscripts && showpatterns) {
      showpatterns = false;
      splitwin->Unsplit(patternctrl);
      splitwin->SplitVertically(scriptctrl, RightPane(), dirwinwd);
   } else {
      if (splitwin->IsSplit()) {
         // hide left pane
         splitwin->Unsplit(scriptctrl);
      } else {
         splitwin->SplitVertically(scriptctrl, RightPane(), dirwinwd);
      }
      viewptr->SetFocus();
   }

   #ifndef __WXMAC__
      // restore scroll bars
      bigview->UpdateScrollBars();
   #endif
}

// -----------------------------------------------------------------------------

void MainFrame::ChangePatternDir()
{
   wxDirDialog dirdlg(this, _("Choose a new pattern folder"), patterndir, wxDD_NEW_DIR_BUTTON);
   if (dirdlg.ShowModal() == wxID_OK)
      SetPatternDir(dirdlg.GetPath());
}

// -----------------------------------------------------------------------------

void MainFrame::ChangeScriptDir()
{
   wxDirDialog dirdlg(this, _("Choose a new script folder"), scriptdir, wxDD_NEW_DIR_BUTTON);
   if (dirdlg.ShowModal() == wxID_OK)
      SetScriptDir(dirdlg.GetPath());
}

// -----------------------------------------------------------------------------

void MainFrame::SetPatternDir(const wxString& newdir)
{
   if (patterndir != newdir) {
      patterndir = newdir;
      if (showpatterns) {
         // show new pattern directory
         SimplifyTree(patterndir, patternctrl->GetTreeCtrl(), patternctrl->GetRootId());
      }
   }
}

// -----------------------------------------------------------------------------

void MainFrame::SetScriptDir(const wxString& newdir)
{
   if (scriptdir != newdir) {
      scriptdir = newdir;
      if (showscripts) {
         // show new script directory
         SimplifyTree(scriptdir, scriptctrl->GetTreeCtrl(), scriptctrl->GetRootId());
      }
   }
}

// -----------------------------------------------------------------------------

void MainFrame::SetStepExponent(int newexpo)
{
   currlayer->currexpo = newexpo;
   if (currlayer->currexpo < minexpo) currlayer->currexpo = minexpo;
   SetGenIncrement();
}

// -----------------------------------------------------------------------------

void MainFrame::SetMinimumStepExponent()
{
   // set minexpo depending on mindelay and maxdelay
   minexpo = 0;
   if (mindelay > 0) {
      int d = mindelay;
      minexpo--;
      while (d < maxdelay) {
         d *= 2;
         minexpo--;
      }
   }
}

// -----------------------------------------------------------------------------

void MainFrame::UpdateStepExponent()
{
   SetMinimumStepExponent();
   if (currlayer->currexpo < minexpo) currlayer->currexpo = minexpo;
   SetGenIncrement();
}

// -----------------------------------------------------------------------------

void MainFrame::ShowPrefsDialog(const wxString& page)
{
   if (viewptr->waitingforclick) return;

   if (generating) {
      // terminate generating loop and set command_pending flag
      Stop();
      command_pending = true;
      cmdevent.SetId(wxID_PREFERENCES);
      return;
   }
   
   if (inscript) {
      // safe to allow prefs dialog while script is running???
      // if so, maybe we need some sort of warning like this:
      // Warning(_("The currently running script might clobber any changes you make."));
   }
   
   int oldtileborder = tileborder;
   int oldcontrolspos = controlspos;

   if (ChangePrefs(page)) {
      // user hit OK button

      // selection color may have changed
      SetSelectionColor();

      // if maxpatterns was reduced then we may need to remove some paths
      while (numpatterns > maxpatterns) {
         numpatterns--;
         patternSubMenu->Delete( patternSubMenu->FindItemByPosition(numpatterns) );
      }

      // if maxscripts was reduced then we may need to remove some paths
      while (numscripts > maxscripts) {
         numscripts--;
         scriptSubMenu->Delete( scriptSubMenu->FindItemByPosition(numscripts) );
      }

      // randomfill might have changed
      SetRandomFillPercentage();

      // if mindelay/maxdelay changed then may need to change minexpo and currexpo
      UpdateStepExponent();

      // maximum memory might have changed
      for (int i = 0; i < numlayers; i++) {
         Layer* layer = GetLayer(i);
         AlgoData* ad = algoinfo[layer->algtype];
         if (ad->algomem >= 0)
            layer->algo->setMaxMemory(ad->algomem);
      }

      // tileborder might have changed
      if (tilelayers && numlayers > 1 && tileborder != oldtileborder) {
         int wd, ht;
         bigview->GetClientSize(&wd, &ht);
         // wd or ht might be < 1 on Windows
         if (wd < 1) wd = 1;
         if (ht < 1) ht = 1;
         ResizeLayers(wd, ht);
      }
      
      // position of translucent controls might have changed
      if (controlspos != oldcontrolspos) {
         int wd, ht;
         if (tilelayers && numlayers > 1) {
            for (int i = 0; i < numlayers; i++) {
               Layer* layer = GetLayer(i);
               layer->tilewin->GetClientSize(&wd, &ht);
               layer->tilewin->SetViewSize(wd, ht);
            }
         }
         bigview->GetClientSize(&wd, &ht);
         bigview->SetViewSize(wd, ht);
      }

      SavePrefs();
   }
   
   // safer to update everything even if user hit Cancel
   UpdateEverything();
}
