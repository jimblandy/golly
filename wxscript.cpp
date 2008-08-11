                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2008 Andrew Trevorrow and Tomas Rokicki.

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

#include "wx/filename.h"   // for wxFileName

#include "wxgolly.h"       // for wxGetApp, mainptr, viewptr, statusptr
#include "wxmain.h"        // for mainptr->...
#include "wxedit.h"        // for Selection
#include "wxview.h"        // for viewptr->...
#include "wxrender.h"      // for SetSelectionColor
#include "wxstatus.h"      // for statusptr->...
#include "wxutils.h"       // for Warning
#include "wxprefs.h"       // for gollydir, allowundo, etc
#include "wxundo.h"        // for undoredo->...
#include "wxalgos.h"       // for *_ALGO, algorgb
#include "wxlayer.h"       // for currlayer, SyncClones
#include "wxperl.h"        // for RunPerlScript, AbortPerlScript
#include "wxpython.h"      // for RunPythonScript, AbortPythonScript
#include "wxscript.h"

// =============================================================================

// globals

bool inscript = false;     // a script is running?
bool plscript = false;     // a Perl script is running?
bool pyscript = false;     // a Python script is running?
bool canswitch;            // can user switch layers while script is running?
bool stop_after_script;    // stop generating pattern after running script?
bool autoupdate;           // update display after each change to current universe?
bool allowcheck;           // allow event checking?
bool showtitle;            // need to update window title?
bool exitcalled;           // GSF_exit was called?
wxString scripterr;        // Perl/Python error message
wxString scriptchars;      // non-escape chars saved by PassKeyToScript
wxString scriptloc;        // location of script file

// -----------------------------------------------------------------------------

void DoAutoUpdate()
{
   if (autoupdate) {
      inscript = false;
      mainptr->UpdatePatternAndStatus();
      if (showtitle) {
         mainptr->SetWindowTitle(wxEmptyString);
         showtitle = false;
      }
      inscript = true;
   }
}

// -----------------------------------------------------------------------------

void ShowTitleLater()
{
   // called from SetWindowTitle when inscript is true;
   // show title at next update (or at end of script)
   showtitle = true;
}

// -----------------------------------------------------------------------------

void ChangeWindowTitle(const wxString& name)
{
   if (autoupdate) {
      // update title bar right now
      inscript = false;
      mainptr->SetWindowTitle(name);
      inscript = true;
      showtitle = false;       // update has been done
   } else {
      // show it later but must still update currlayer->currname and menu item
      mainptr->SetWindowTitle(name);
      // showtitle is now true
   }
}

// =============================================================================

// The following Golly Script Functions are used to reduce code duplication.
// They are called by corresponding pl_* and py_* functions in wxperl.cpp
// and wxpython.cpp respectively.

const char* GSF_open(char* filename, int remember)
{
   if (IsScript(wxString(filename,wxConvLocal))) {
      // avoid re-entrancy
      return "Cannot open a script file while a script is running.";
   }

   // convert non-absolute filename to absolute path relative to scriptloc
   wxString fname = wxString(filename,wxConvLocal);
   wxFileName fullname(fname);
   if (!fullname.IsAbsolute()) fullname = scriptloc + fname;

   // only add file to Open Recent submenu if remember flag is non-zero
   mainptr->OpenFile(fullname.GetFullPath(), remember != 0);
   DoAutoUpdate();
   
   return NULL;
}

// -----------------------------------------------------------------------------

const char* GSF_save(char* filename, char* format, int remember)
{
   // convert non-absolute filename to absolute path relative to scriptloc
   wxString fname = wxString(filename,wxConvLocal);
   wxFileName fullname(fname);
   if (!fullname.IsAbsolute()) fullname = scriptloc + fname;

   // only add file to Open Recent submenu if remember flag is non-zero
   return mainptr->SaveFile(fullname.GetFullPath(),
                            wxString(format,wxConvLocal), remember != 0);
}

// -----------------------------------------------------------------------------

const char* GSF_setalgo(char* algostring)
{
   // find index for given algo name
   algo_type algoindex = -1;
   for (int i = 0; i < NumAlgos(); i++) {
      if (strcmp(algostring, GetAlgoName(i)) == 0) {
         algoindex = i;
         break;
      }
   }
   if (algoindex < 0) {
      static char err[256];
      sprintf(err, "Unknown algorithm: %s", algostring);
      return err;
   }
   
   if (algoindex != currlayer->algtype) {
      mainptr->ChangeAlgorithm(algoindex);
      if (algoindex != currlayer->algtype) {
         return "Algorithm could not be changed (pattern is too big to convert).";
      } else {
         // rule might have changed
         ChangeWindowTitle(wxEmptyString);
         // pattern might have changed or colors might have changed
         DoAutoUpdate();
      }
   }
   
   return NULL;
}

// -----------------------------------------------------------------------------

const char* GSF_setrule(char* rulestring)
{
   wxString oldrule = wxString(currlayer->algo->getrule(),wxConvLocal);
   int oldmaxstate = currlayer->algo->NumCellStates() - 1;

   // inscript should be true but play safe
   if (allowundo && !currlayer->stayclean && inscript) {
      // note that we must save pending gen changes BEFORE changing rule
      // otherwise temporary files will store incorrect rule info
      SavePendingChanges();
   }

   const char* err;
   if (rulestring == NULL || rulestring[0] == 0) {
      // set normal Life
      err = currlayer->algo->setrule("B3/S23");
   } else {
      err = currlayer->algo->setrule(rulestring);
   }
   if (err) {
      // try to find another algorithm that supports the new rule
      for (int i = 0; i < NumAlgos(); i++) {
         if (i != currlayer->algtype) {
            lifealgo* tempalgo = CreateNewUniverse(i);
            err = tempalgo->setrule(rulestring);
            delete tempalgo;
            if (!err) {
               // change the current algorithm and switch to the new rule
               mainptr->ChangeAlgorithm(i, wxString(rulestring,wxConvLocal));
               if (i != currlayer->algtype) {
                  currlayer->algo->setrule( oldrule.mb_str(wxConvLocal) );
                  return "Algorithm could not be changed (pattern is too big to convert).";
               } else {
                  ChangeWindowTitle(wxEmptyString);
                  DoAutoUpdate();
                  return NULL;
               }
            }
         }
      }
      currlayer->algo->setrule( oldrule.mb_str(wxConvLocal) );
      return "Given rule is not valid in any algorithm.";
   }

   wxString newrule = wxString(currlayer->algo->getrule(),wxConvLocal);
   if (oldrule != newrule) {
      // show new rule in main window's title but don't change name
      ChangeWindowTitle(wxEmptyString);

      // rule change might have changed the number of cell states;
      // if there are fewer states then pattern might change
      int newmaxstate = currlayer->algo->NumCellStates() - 1;
      if (newmaxstate < oldmaxstate && !currlayer->algo->isEmpty()) {
         mainptr->ReduceCellStates(newmaxstate);
      }

      // pattern might have changed or new rule might have changed colors
      DoAutoUpdate();
      
      if (allowundo && !currlayer->stayclean) {
         currlayer->undoredo->RememberRuleChange(oldrule);
      }
   }
   
   return NULL;
}

// -----------------------------------------------------------------------------

const char* GSF_setgen(char* genstring)
{
   const char* err = mainptr->ChangeGenCount(genstring);
   if (!err) DoAutoUpdate();

   return err;
}

// -----------------------------------------------------------------------------

const char* GSF_setpos(char* x, char* y)
{
   // disallow alphabetic chars in x,y
   int i;
   int xlen = strlen(x);
   for (i = 0; i < xlen; i++)
      if ( (x[i] >= 'a' && x[i] <= 'z') || (x[i] >= 'A' && x[i] <= 'Z') )
         return "Illegal character in x value.";

   int ylen = strlen(y);
   for (i = 0; i < ylen; i++)
      if ( (y[i] >= 'a' && y[i] <= 'z') || (y[i] >= 'A' && y[i] <= 'Z') )
         return "Illegal character in y value.";

   bigint bigx(x);
   bigint bigy(y);
   viewptr->SetPosMag(bigx, bigy, viewptr->GetMag());
   DoAutoUpdate();

   return NULL;
}

// -----------------------------------------------------------------------------

void GSF_setname(char* name, int index)
{
   if (name == NULL || name[0] == 0) return;

   // inscript should be true but play safe
   if (allowundo && !currlayer->stayclean && inscript)
      SavePendingChanges();

   if (index == currindex) {
      // save old name for RememberNameChange
      wxString oldname = currlayer->currname;
      
      // show new name in main window's title;
      // also sets currlayer->currname and updates menu item
      ChangeWindowTitle(wxString(name,wxConvLocal));

      if (allowundo && !currlayer->stayclean) {
         // note that currfile and savestart/dirty flags don't change
         currlayer->undoredo->RememberNameChange(oldname, currlayer->currfile,
                                                 currlayer->savestart, currlayer->dirty);
      }
   } else {
      // temporarily change currlayer (used in RememberNameChange)
      Layer* savelayer = currlayer;
      currlayer = GetLayer(index);
      wxString oldname = currlayer->currname;

      currlayer->currname = wxString(name,wxConvLocal);

      if (allowundo && !currlayer->stayclean) {
         // note that currfile and savestart/dirty flags don't change
         currlayer->undoredo->RememberNameChange(oldname, currlayer->currfile,
                                                 currlayer->savestart, currlayer->dirty);
      }
      
      // restore currlayer
      currlayer = savelayer;
      
      // show name in given layer's menu item
      mainptr->UpdateLayerItem(index);
   }
}

// -----------------------------------------------------------------------------

const char* GSF_setcell(int x, int y, int newstate)
{
   int oldstate = currlayer->algo->getcell(x, y);
   if (newstate != oldstate) {
      if (currlayer->algo->setcell(x, y, newstate) < 0) {
         return "setcell error: state value is out of range.";
      }
      currlayer->algo->endofpattern();
      if (allowundo && !currlayer->stayclean) {
         ChangeCell(x, y, oldstate, newstate);
      }
      MarkLayerDirty();
      DoAutoUpdate();
   }
   return 0;
}

// -----------------------------------------------------------------------------

void GSF_select(int x, int y, int wd, int ht)
{
   if (wd < 1 || ht < 1) {
      // remove any existing selection
      viewptr->SaveCurrentSelection();
      currlayer->currsel.Deselect();
      viewptr->RememberNewSelection(_("Deselection"));
   } else {
      // set selection edges
      viewptr->SaveCurrentSelection();
      currlayer->currsel.SetRect(x, y, wd, ht);
      viewptr->RememberNewSelection(_("Selection"));
   }
}

// -----------------------------------------------------------------------------

bool GSF_setoption(char* optname, int newval, int* oldval)
{
   if (strcmp(optname, "autofit") == 0) {
      *oldval = currlayer->autofit ? 1 : 0;
      if (*oldval != newval) {
         mainptr->ToggleAutoFit();
         // autofit option only applies to a generating pattern
         // DoAutoUpdate();
      }

   } else if (strcmp(optname, "fullscreen") == 0) {
      *oldval = mainptr->fullscreen ? 1 : 0;
      if (*oldval != newval) {
         mainptr->ToggleFullScreen();
         // above always does an update (due to resizing viewport window)
         // DoAutoUpdate();
      }

   } else if (strcmp(optname, "hyperspeed") == 0) {
      *oldval = currlayer->hyperspeed ? 1 : 0;
      if (*oldval != newval)
         mainptr->ToggleHyperspeed();

   } else if (strcmp(optname, "showhashinfo") == 0) {
      *oldval = currlayer->showhashinfo ? 1 : 0;
      if (*oldval != newval)
         mainptr->ToggleHashInfo();

   } else if (strcmp(optname, "mindelay") == 0) {
      *oldval = mindelay;
      if (newval < 0) newval = 0;
      if (newval > MAX_DELAY) newval = MAX_DELAY;
      if (*oldval != newval) {
         mindelay = newval;
         mainptr->UpdateWarp();
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "maxdelay") == 0) {
      *oldval = maxdelay;
      if (newval < 0) newval = 0;
      if (newval > MAX_DELAY) newval = MAX_DELAY;
      if (*oldval != newval) {
         maxdelay = newval;
         mainptr->UpdateWarp();
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "opacity") == 0) {
      *oldval = opacity;
      if (newval < 1) newval = 1;
      if (newval > 100) newval = 100;
      if (*oldval != newval) {
         opacity = newval;
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "showlayerbar") == 0) {
      *oldval = showlayer ? 1 : 0;
      if (*oldval != newval) {
         ToggleLayerBar();
         // above always does an update (due to resizing viewport window)
         // DoAutoUpdate();
      }

   } else if (strcmp(optname, "showeditbar") == 0) {
      *oldval = showedit ? 1 : 0;
      if (*oldval != newval) {
         ToggleEditBar();
         // above always does an update (due to resizing viewport window)
         // DoAutoUpdate();
      }

   } else if (strcmp(optname, "showpatterns") == 0) {
      *oldval = showpatterns ? 1 : 0;
      if (*oldval != newval) {
         mainptr->ToggleShowPatterns();
         // above always does an update (due to resizing viewport window)
         // DoAutoUpdate();
      }

   } else if (strcmp(optname, "showscripts") == 0) {
      *oldval = showscripts ? 1 : 0;
      if (*oldval != newval) {
         mainptr->ToggleShowScripts();
         // above always does an update (due to resizing viewport window)
         // DoAutoUpdate();
      }

   } else if (strcmp(optname, "showtoolbar") == 0) {
      *oldval = showtool ? 1 : 0;
      if (*oldval != newval) {
         mainptr->ToggleToolBar();
         // above always does an update (due to resizing viewport window)
         // DoAutoUpdate();
      }

   } else if (strcmp(optname, "showstatusbar") == 0) {
      *oldval = showstatus ? 1 : 0;
      if (*oldval != newval) {
         mainptr->ToggleStatusBar();
         // above always does an update (due to resizing viewport window)
         // DoAutoUpdate();
      }

   } else if (strcmp(optname, "showexact") == 0) {
      *oldval = showexact ? 1 : 0;
      if (*oldval != newval) {
         mainptr->ToggleExactNumbers();
         // above always does an update (due to resizing viewport window)
         // DoAutoUpdate();
      }

   } else if (strcmp(optname, "showicons") == 0) {
      *oldval = showicons ? 1 : 0;
      if (*oldval != newval) {
         viewptr->ToggleCellIcons();
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "swapcolors") == 0) {
      *oldval = swapcolors ? 1 : 0;
      if (*oldval != newval) {
         viewptr->ToggleCellColors();
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "showgrid") == 0) {
      *oldval = showgridlines ? 1 : 0;
      if (*oldval != newval) {
         showgridlines = !showgridlines;
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "showboldlines") == 0) {
      *oldval = showboldlines ? 1 : 0;
      if (*oldval != newval) {
         showboldlines = !showboldlines;
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "syncviews") == 0) {
      *oldval = syncviews ? 1 : 0;
      if (*oldval != newval) {
         ToggleSyncViews();
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "synccursors") == 0) {
      *oldval = synccursors ? 1 : 0;
      if (*oldval != newval) {
         ToggleSyncCursors();
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "switchlayers") == 0) {
      *oldval = canswitch ? 1 : 0;
      if (*oldval != newval) {
         canswitch = !canswitch;
         // no need for DoAutoUpdate();
      }

   } else if (strcmp(optname, "stacklayers") == 0) {
      *oldval = stacklayers ? 1 : 0;
      if (*oldval != newval) {
         ToggleStackLayers();
         // above always does an update
         // DoAutoUpdate();
      }

   } else if (strcmp(optname, "tilelayers") == 0) {
      *oldval = tilelayers ? 1 : 0;
      if (*oldval != newval) {
         ToggleTileLayers();
         // above always does an update
         // DoAutoUpdate();
      }

   } else if (strcmp(optname, "boldspacing") == 0) {
      *oldval = boldspacing;
      if (newval < 2) newval = 2;
      if (newval > MAX_SPACING) newval = MAX_SPACING;
      if (*oldval != newval) {
         boldspacing = newval;
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "savexrle") == 0) {
      *oldval = savexrle ? 1 : 0;
      if (*oldval != newval) {
         savexrle = !savexrle;
         // no need for DoAutoUpdate();
      }

   // this option is deprecated (use setalgo command)
   } else if (strcmp(optname, "hashing") == 0) {
      *oldval = (currlayer->algtype == HLIFE_ALGO) ? 1 : 0;
      if (*oldval != newval) {
         mainptr->ChangeAlgorithm(newval ? HLIFE_ALGO : QLIFE_ALGO);
         DoAutoUpdate();
      }

   } else {
      // unknown option
      return false;
   }

   if (*oldval != newval) {
      mainptr->UpdateMenuItems(mainptr->IsActive());
   }

   return true;
}

// -----------------------------------------------------------------------------

bool GSF_getoption(char* optname, int* optval)
{
   if      (strcmp(optname, "autofit") == 0)       *optval = currlayer->autofit ? 1 : 0;
   else if (strcmp(optname, "boldspacing") == 0)   *optval = boldspacing;
   else if (strcmp(optname, "fullscreen") == 0)    *optval = mainptr->fullscreen ? 1 : 0;
   else if (strcmp(optname, "hyperspeed") == 0)    *optval = currlayer->hyperspeed ? 1 : 0;
   else if (strcmp(optname, "mindelay") == 0)      *optval = mindelay;
   else if (strcmp(optname, "maxdelay") == 0)      *optval = maxdelay;
   else if (strcmp(optname, "opacity") == 0)       *optval = opacity;
   else if (strcmp(optname, "savexrle") == 0)      *optval = savexrle ? 1 : 0;
   else if (strcmp(optname, "showboldlines") == 0) *optval = showboldlines ? 1 : 0;
   else if (strcmp(optname, "showeditbar") == 0)   *optval = showedit ? 1 : 0;
   else if (strcmp(optname, "showexact") == 0)     *optval = showexact ? 1 : 0;
   else if (strcmp(optname, "showgrid") == 0)      *optval = showgridlines ? 1 : 0;
   else if (strcmp(optname, "showhashinfo") == 0)  *optval = currlayer->showhashinfo ? 1 : 0;
   else if (strcmp(optname, "showicons") == 0)     *optval = showicons ? 1 : 0;
   else if (strcmp(optname, "showlayerbar") == 0)  *optval = showlayer ? 1 : 0;
   else if (strcmp(optname, "showpatterns") == 0)  *optval = showpatterns ? 1 : 0;
   else if (strcmp(optname, "showscripts") == 0)   *optval = showscripts ? 1 : 0;
   else if (strcmp(optname, "showstatusbar") == 0) *optval = showstatus ? 1 : 0;
   else if (strcmp(optname, "showtoolbar") == 0)   *optval = showtool ? 1 : 0;
   else if (strcmp(optname, "stacklayers") == 0)   *optval = stacklayers ? 1 : 0;
   else if (strcmp(optname, "swapcolors") == 0)    *optval = swapcolors ? 1 : 0;
   else if (strcmp(optname, "switchlayers") == 0)  *optval = canswitch ? 1 : 0;
   else if (strcmp(optname, "synccursors") == 0)   *optval = synccursors ? 1 : 0;
   else if (strcmp(optname, "syncviews") == 0)     *optval = syncviews ? 1 : 0;
   else if (strcmp(optname, "tilelayers") == 0)    *optval = tilelayers ? 1 : 0;
   // this option is deprecated (use getalgo command)
   else if (strcmp(optname, "hashing") == 0)
           *optval = (currlayer->algtype == HLIFE_ALGO) ? 1 : 0;
   else {
      // unknown option
      return false;
   }
   return true;
}

// -----------------------------------------------------------------------------

bool GSF_setcolor(char* colname, wxColor& newcol, wxColor& oldcol)
{
   // note that "livecells" = "livecells0"
   if (strncmp(colname, "livecells", 9) == 0) {
      int layer = 0;
      if (colname[9] >= '0' && colname[9] <= '9') {
         layer = colname[9] - '0';
      }
      oldcol = *livergb[layer];
      if (oldcol != newcol) {
         *livergb[layer] = newcol;
         SetBrushesAndPens();
         DoAutoUpdate();
      }

   } else if (strcmp(colname, "deadcells") == 0) {
      oldcol = *deadrgb;
      if (oldcol != newcol) {
         *deadrgb = newcol;
         SetBrushesAndPens();
         DoAutoUpdate();
      }

   } else if (strcmp(colname, "paste") == 0) {
      oldcol = *pastergb;
      if (oldcol != newcol) {
         *pastergb = newcol;
         SetBrushesAndPens();
         DoAutoUpdate();
      }

   } else if (strcmp(colname, "select") == 0) {
      oldcol = *selectrgb;
      if (oldcol != newcol) {
         *selectrgb = newcol;
         SetBrushesAndPens();
         SetSelectionColor();    // see wxrender.cpp
         DoAutoUpdate();
      }

   } else if (strcmp(colname, "hashing") == 0) {      // deprecated
      oldcol = *(algoinfo[HLIFE_ALGO]->algorgb);
      if (oldcol != newcol) {
         *(algoinfo[HLIFE_ALGO]->algorgb) = newcol;
         SetBrushesAndPens();
         DoAutoUpdate();
      }

   } else if (strcmp(colname, "nothashing") == 0) {   // deprecated
      oldcol = *(algoinfo[QLIFE_ALGO]->algorgb);
      if (oldcol != newcol) {
         *(algoinfo[QLIFE_ALGO]->algorgb) = newcol;
         SetBrushesAndPens();
         DoAutoUpdate();
      }

   } else {
      // look for algo name
      for (int i = 0; i < NumAlgos(); i++) {
         if (strcmp(colname, GetAlgoName(i)) == 0) {
            oldcol = *(algoinfo[i]->algorgb);
            if (oldcol != newcol) {
               *(algoinfo[i]->algorgb) = newcol;
               SetBrushesAndPens();
               DoAutoUpdate();
            }
            return true;
         }
      }
      // unknown color name
      return false;
   }
   return true;
}

// -----------------------------------------------------------------------------

bool GSF_getcolor(char* colname, wxColor& color)
{
   // note that "livecells" = "livecells0"
   if (strncmp(colname, "livecells", 9) == 0) {
      int layer = 0;
      if (colname[9] >= '0' && colname[9] <= '9') {
         layer = colname[9] - '0';
      }
      color = *livergb[layer];
   }
   else if (strcmp(colname, "deadcells") == 0)  color = *deadrgb;
   else if (strcmp(colname, "paste") == 0)      color = *pastergb;
   else if (strcmp(colname, "select") == 0)     color = *selectrgb;
   // next two are deprecated
   else if (strcmp(colname, "hashing") == 0)    color = *(algoinfo[HLIFE_ALGO]->algorgb);
   else if (strcmp(colname, "nothashing") == 0) color = *(algoinfo[QLIFE_ALGO]->algorgb);
   else {
      // look for algo name
      for (int i = 0; i < NumAlgos(); i++) {
         if (strcmp(colname, GetAlgoName(i)) == 0) {
            color = *(algoinfo[i]->algorgb);
            return true;
         }
      }
      // unknown color name
      return false;
   }
   return true;
}

// -----------------------------------------------------------------------------

void GSF_getkey(char* s)
{
   if (scriptchars.Length() == 0) {
      // return empty string
      s[0] = '\0';
   } else {
      // return first char in scriptchars and then remove it
      s[0] = scriptchars.GetChar(0);
      s[1] = '\0';
      scriptchars = scriptchars.AfterFirst(s[0]);
   }
}

// -----------------------------------------------------------------------------

// also allow mouse interaction??? ie. g.doclick( g.getclick() ) where
// getclick returns [] or [x,y,button,modifiers]

void GSF_dokey(char* ascii)
{
   if (*ascii) {
      // convert ascii char to corresponding wx key code;
      // note that PassKeyToScript does the reverse conversion
      int key;
      switch (*ascii) {
         case 127:   // treat delete like backspace
         case 8:     key = WXK_BACK;   break;
         case 9:     key = WXK_TAB;    break;
         case 10:    // treat linefeed like return
         case 13:    key = WXK_RETURN; break;
         case 28:    key = WXK_LEFT;   break;
         case 29:    key = WXK_RIGHT;  break;
         case 30:    key = WXK_UP;     break;
         case 31:    key = WXK_DOWN;   break;
         default:    key = *ascii;
      }

      // can we handle modifiers and be backward compatible???
      // or do we need new getmodkey & domodkey commands???
      viewptr->ProcessKey(key, wxMOD_NONE);

      // see any cursor change, including in tool bar
      mainptr->UpdateUserInterface(mainptr->IsActive());

      // update viewport, status bar, scroll bars, etc
      inscript = false;
      mainptr->UpdatePatternAndStatus();
      bigview->UpdateScrollBars();
      if (showtitle) {
         mainptr->SetWindowTitle(wxEmptyString);
         showtitle = false;
      }
      inscript = true;
   }
}

// -----------------------------------------------------------------------------

void GSF_update()
{
   // update viewport, status bar and possibly title bar
   inscript = false;
   mainptr->UpdatePatternAndStatus();
   if (showtitle) {
      mainptr->SetWindowTitle(wxEmptyString);
      showtitle = false;
   }
   inscript = true;
}

// -----------------------------------------------------------------------------

void GSF_exit(char* errmsg)
{
   if (errmsg && errmsg[0] != 0) {
      // display given error message
      inscript = false;
      statusptr->ErrorMessage(wxString(errmsg,wxConvLocal));
      inscript = true;
      // make sure status bar is visible
      if (!showstatus) mainptr->ToggleStatusBar();
   }

   exitcalled = true;   // prevent CheckScriptError changing message
}

// =============================================================================

bool IsScript(const wxString& filename)
{
   // currently we support Perl or Python scripts, so return true if
   // filename ends with ".pl" or ".py" (ignoring case)
   wxString ext = filename.AfterLast(wxT('.'));
   return ext.IsSameAs(wxT("pl"), false) || ext.IsSameAs(wxT("py"), false);
}

// -----------------------------------------------------------------------------

void CheckScriptError(const wxString& ext)
{
   if (scripterr.IsEmpty()) {
      return;
   } else if (scripterr.Find(wxString(abortmsg,wxConvLocal)) >= 0) {
      // error was caused by AbortPerlScript/AbortPythonScript
      // so don't display scripterr
   } else {
      wxString errtype;
      if (ext.IsSameAs(wxT("pl"), false)) {
         errtype = _("Perl error:");
         scripterr.Replace(wxT(". at "), wxT("\nat "));
      } else {
         errtype = _("Python error:");
         scripterr.Replace(wxT("  File \"<string>\", line 1, in ?\n"), wxT(""));
      }
      wxBell();
      #ifdef __WXMAC__
         wxSetCursor(*wxSTANDARD_CURSOR);
      #endif
      wxMessageBox(scripterr, errtype, wxOK | wxICON_EXCLAMATION, wxGetActiveWindow());
   }
   // don't change status message if GSF_exit was used to stop script
   if (!exitcalled) statusptr->DisplayMessage(_("Script aborted."));
}

// -----------------------------------------------------------------------------

void ChangeCell(int x, int y, int oldstate, int newstate)
{
   // first check if there are any pending gen changes that need to be remembered
   if (currlayer->undoredo->savegenchanges) {
      currlayer->undoredo->savegenchanges = false;
      currlayer->undoredo->RememberGenFinish();
   }

   // setcell/putcells command is changing state of cell at x,y
   currlayer->undoredo->SaveCellChange(x, y, oldstate, newstate);
   if (!currlayer->undoredo->savecellchanges) {
      currlayer->undoredo->savecellchanges = true;
      // save layer's dirty state for next RememberCellChanges call
      currlayer->savedirty = currlayer->dirty;
   }
}

// -----------------------------------------------------------------------------

void SavePendingChanges(bool checkgenchanges)
{
   // this should only be called if inscript && allowundo && !currlayer->stayclean
   if ( !(inscript && allowundo && !currlayer->stayclean) )
      Warning(_("Bug detected in SavePendingChanges!"));
   
   if (currlayer->undoredo->savecellchanges) {
      currlayer->undoredo->savecellchanges = false;
      // remember accumulated cell changes
      currlayer->undoredo->RememberCellChanges(_("bug1"), currlayer->savedirty);
      // action string should never be seen
   }

   if (checkgenchanges && currlayer->undoredo->savegenchanges) {
      currlayer->undoredo->savegenchanges = false;
      // remember accumulated gen changes
      currlayer->undoredo->RememberGenFinish();
   }
}

// -----------------------------------------------------------------------------

void RunScript(const wxString& filename)
{
   if ( inscript ) return;    // play safe and avoid re-entrancy

   if ( !wxFileName::FileExists(filename) ) {
      wxString err = _("The script file does not exist:\n") + filename;
      Warning(err);
      return;
   }

   mainptr->showbanner = false;
   statusptr->ClearMessage();
   scripterr.Clear();
   scriptchars.Clear();
   canswitch = false;
   stop_after_script = false;
   autoupdate = false;
   exitcalled = false;
   allowcheck = true;
   showtitle = false;
   wxGetApp().PollerReset();

   // temporarily change current directory to location of script
   wxFileName fullname(filename);
   fullname.Normalize();
   scriptloc = fullname.GetPath();
   if ( scriptloc.Last() != wxFILE_SEP_PATH ) scriptloc += wxFILE_SEP_PATH;
   wxSetWorkingDirectory(scriptloc);

   wxString fpath = fullname.GetFullPath();
   #ifdef __WXMAC__
      // use decomposed UTF8 so interpreter can open names with non-ASCII chars
      #if wxCHECK_VERSION(2, 7, 0)
         fpath = wxString(fpath.fn_str(),wxConvLocal);
      #else
         // wxMac 2.6.x or older
         fpath = wxString(fpath.wc_str(wxConvLocal), wxConvUTF8);
      #endif
   #endif

   if (allowundo) {
      // save each layer's dirty state for use by next RememberCellChanges call
      for ( int i = 0; i < numlayers; i++ ) {
         Layer* layer = GetLayer(i);
         layer->savedirty = layer->dirty;
         // at start of script there are no pending cell/gen changes
         layer->undoredo->savecellchanges = false;
         layer->undoredo->savegenchanges = false;
         // add a special node to indicate that the script is about to start so
         // that all changes made by the script can be undone/redone in one go;
         // note that the UndoRedo ctor calls RememberScriptStart if the script
         // creates a new non-cloned layer, and we let RememberScriptStart handle
         // multiple calls if this layer is a clone
         layer->undoredo->RememberScriptStart();
      }
   }

   inscript = true;
   mainptr->UpdateUserInterface(mainptr->IsActive());
   #ifdef __WXMSW__
      // temporarily clear non-ctrl and non-func key accelerators from
      // menu items so keys like tab/enter/space can be passed to script
      mainptr->UpdateMenuAccelerators();
   #endif

   wxString ext = filename.AfterLast(wxT('.'));
   if (ext.IsSameAs(wxT("pl"), false)) {
      plscript = true;
      RunPerlScript(fpath);
   } else if (ext.IsSameAs(wxT("py"), false)) {
      pyscript = true;
      RunPythonScript(fpath);
   } else {
      // should never happen
      wxString err = _("Unexpected extension in script file:\n") + filename;
      Warning(err);
   }

   // tidy up the undo/redo history for each layer; note that some calls
   // use currlayer (eg. RememberGenFinish) so we temporarily set currlayer
   // to each layer -- this is a bit yukky but should be safe as long as we
   // synchronize clone info, especially currlayer->algo ptrs because they
   // can change if the script called new()
   SyncClones();
   Layer* savelayer = currlayer;
   for ( int i = 0; i < numlayers; i++ ) {
      currlayer = GetLayer(i);
      if (allowundo) {
         if (currlayer->undoredo->savecellchanges) {
            currlayer->undoredo->savecellchanges = false;
            // remember pending cell change(s)
            if (currlayer->stayclean)
               currlayer->undoredo->ForgetCellChanges();
            else
               currlayer->undoredo->RememberCellChanges(_("bug2"), currlayer->savedirty);
               // action string should never be seen
         }
         if (currlayer->undoredo->savegenchanges) {
            currlayer->undoredo->savegenchanges = false;
            // remember pending gen change(s); no need to test stayclean flag
            // (if it's true then NextGeneration called RememberGenStart)
            currlayer->undoredo->RememberGenFinish();
         }
         // add special node to indicate that the script has finished;
         // we let RememberScriptFinish handle multiple calls if this
         // layer is a clone
         currlayer->undoredo->RememberScriptFinish();
      }
      // reset the stayclean flag in case it was set by MarkLayerClean
      currlayer->stayclean = false;
   }
   currlayer = savelayer;
   
   // must reset inscript AFTER RememberGenFinish
   inscript = false;
   plscript = false;
   pyscript = false;

   // restore current directory to location of Golly app
   wxSetWorkingDirectory(gollydir);
   
   // update Undo/Redo items based on current layer's history
   if (allowundo) currlayer->undoredo->UpdateUndoRedoItems();

   // display any Perl/Python error message
   CheckScriptError(ext);

   // update title, menu bar, cursor, viewport, status bar, tool bar, etc
   if (showtitle) mainptr->SetWindowTitle(wxEmptyString);
   mainptr->UpdateEverything();
   
   #ifdef __WXMSW__
      // restore accelerators that were cleared above
      mainptr->UpdateMenuAccelerators();
   #endif
}

// -----------------------------------------------------------------------------

void PassKeyToScript(int key)
{
   if (key == WXK_ESCAPE) {
      if (plscript) AbortPerlScript();
      if (pyscript) AbortPythonScript();
   } else {
      // convert wx key code to corresponding ascii char (if possible)
      // so that scripts can be platform-independent;
      // note that GSF_dokey does the reverse conversion
      /*
      Warning(wxString::Format(_("key=%d ch=%c"), key, (char)key));
      */
      char ascii;
      switch (key) {
         case WXK_DELETE:     // treat delete like backspace
         case WXK_BACK:       ascii = 8;     break;
         case WXK_TAB:        ascii = 9;     break;
         case WXK_NUMPAD_ENTER: // treat enter like return
         case WXK_RETURN:     ascii = 13;    break;
         case WXK_LEFT:       ascii = 28;    break;
         case WXK_RIGHT:      ascii = 29;    break;
         case WXK_UP:         ascii = 30;    break;
         case WXK_DOWN:       ascii = 31;    break;
         case WXK_ADD:        ascii = '+';   break;
         case WXK_SUBTRACT:   ascii = '-';   break;
         case WXK_DIVIDE:     ascii = '/';   break;
         case WXK_MULTIPLY:   ascii = '*';   break;
         // map F1..F12 to ascii 14..25???
         default:
            if (key >= ' ' && key <= '~') {
               ascii = key;
            } else {
               // ignore all other key codes???
               // or maybe allow codes < ' ' but ignore > '~'???
               return;
            }
      }
      // save ascii char for possible consumption by GSF_getkey
      scriptchars += ascii;
   }
}

// -----------------------------------------------------------------------------

void FinishScripting()
{
   // called when main window is closing (ie. app is quitting)
   if (inscript) {
      if (plscript) AbortPerlScript();
      if (pyscript) AbortPythonScript();
      wxSetWorkingDirectory(gollydir);
      inscript = false;
   }

   FinishPerlScripting();
   FinishPythonScripting();
}
