                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2006 Andrew Trevorrow and Tomas Rokicki.

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

/*
   Golly uses an embedded Python interpreter to execute scripts.
   Here is the official Python copyright notice:
   
   Copyright (c) 2001-2005 Python Software Foundation.
   All Rights Reserved.
   
   Copyright (c) 2000 BeOpen.com.
   All Rights Reserved.
   
   Copyright (c) 1995-2001 Corporation for National Research Initiatives.
   All Rights Reserved.
   
   Copyright (c) 1991-1995 Stichting Mathematisch Centrum, Amsterdam.
   All Rights Reserved.
*/

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
   #include "wx/wx.h"      // for all others include the necessary headers
#endif

#include "wx/filename.h"   // for wxFileName

#include <limits.h>        // for INT_MAX

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "liferules.h"     // for global_liferules
#include "readpattern.h"
#include "writepattern.h"

#include "wxgolly.h"       // for wxGetApp, mainptr, viewptr, statusptr
#include "wxmain.h"        // for mainptr->...
#include "wxview.h"        // for viewptr->...
#include "wxstatus.h"      // for statusptr->...
#include "wxutils.h"       // for Warning
#include "wxprefs.h"       // for hashing etc
#include "wxinfo.h"        // for ShowInfo
#include "wxhelp.h"        // for ShowHelp
#include "wxscript.h"

// Python includes
#ifdef HAVE_WCHAR_H
#undef HAVE_WCHAR_H        // Python.h redefines this if we use Unicode
#endif
#include <Python.h>

// =============================================================================

// globals

bool pyinited = false;     // Py_Initialize has been successfully called?
bool inscript = false;     // a script is running?
bool autoupdate;           // update display after each change to current universe?
wxString pyerror;          // Python error message
wxString gollyloc;         // location of Golly app
wxString scriptloc;        // location of script file
wxString scriptkeys;       // non-escape keys saved by PassKeyToScript

// exception message set by AbortScript
const char abortmsg[] = "GOLLY: ABORT SCRIPT";

// =============================================================================

// The following golly_* routines can be called from Python scripts; some are
// based on code in PLife's lifeint.cc (see http://plife.sourceforge.net/).

void AbortScript()
{
   // raise an exception with a special message
   PyErr_SetString(PyExc_KeyboardInterrupt, abortmsg);
}

// -----------------------------------------------------------------------------

bool ScriptAborted()
{
   wxGetApp().Poller()->checkevents();
   
   // if user hit escape key then AbortScript has raised an exception
   // and PyErr_Occurred will be true; if so, caller must return NULL
   // otherwise Python can abort app with this message:
   // Fatal Python error: unexpected exception during garbage collection

   return PyErr_Occurred() != NULL;
}

// -----------------------------------------------------------------------------

void DoAutoUpdate()
{
   if (autoupdate) {
      inscript = false;
      mainptr->UpdatePatternAndStatus();
      inscript = true;
   }
}

// -----------------------------------------------------------------------------

static PyObject *golly_new(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *title;

   if (!PyArg_ParseTuple(args, "z", &title)) return NULL;

   mainptr->NewPattern(title);
   DoAutoUpdate();

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_open(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *file_name;
   int remember = 0;

   if (!PyArg_ParseTuple(args, "z|i", &file_name, &remember)) return NULL;

   if (IsScript(file_name)) {
      // avoid re-entrancy
      PyErr_SetString(PyExc_RuntimeError, "Bad open call: cannot open a script file.");
      return NULL;
   }

   // convert non-absolute file_name to absolute path relative to scriptloc
   // so it can be selected later from Open Recent submenu
   wxFileName fullname(file_name);
   if (!fullname.IsAbsolute()) fullname = scriptloc + wxT(file_name);

   // only add file to Open Recent submenu if remember flag is non-zero
   mainptr->OpenFile(fullname.GetFullPath(), remember != 0);
   DoAutoUpdate();

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_save(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *file_name;
   char *format;
   int remember = 0;

   if (!PyArg_ParseTuple(args, "zz|i", &file_name, &format, &remember)) return NULL;

   // convert non-absolute file_name to absolute path relative to scriptloc
   // so it can be selected later from Open Recent submenu
   wxFileName fullname(file_name);
   if (!fullname.IsAbsolute()) fullname = scriptloc + wxT(file_name);

   // only add file to Open Recent submenu if remember flag is non-zero
   const char *err = mainptr->SaveFile(fullname.GetFullPath().c_str(), format, remember != 0);
   if (err) {
      PyErr_SetString(PyExc_RuntimeError, err);
      return NULL;
   }

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_fit(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   viewptr->FitPattern();
   DoAutoUpdate();

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_fitsel(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   if (viewptr->SelectionExists()) {
      viewptr->FitSelection();
      DoAutoUpdate();
   } else {
      PyErr_SetString(PyExc_RuntimeError, "Bad fitsel call: no selection.");
      return NULL;
   }

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_cut(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   if (viewptr->SelectionExists()) {
      viewptr->CutSelection();
      DoAutoUpdate();
   } else {
      PyErr_SetString(PyExc_RuntimeError, "Bad cut call: no selection.");
      return NULL;
   }

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_copy(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   if (viewptr->SelectionExists()) {
      viewptr->CopySelection();
      DoAutoUpdate();
   } else {
      PyErr_SetString(PyExc_RuntimeError, "Bad copy call: no selection.");
      return NULL;
   }

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_clear(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   int where;

   if (!PyArg_ParseTuple(args, "i", &where)) return NULL;

   if (viewptr->SelectionExists()) {
      if (where == 0)
         viewptr->ClearSelection();
      else
         viewptr->ClearOutsideSelection();
      DoAutoUpdate();
   } else {
      PyErr_SetString(PyExc_RuntimeError, "Bad clear call: no selection.");
      return NULL;
   }

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_paste(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   int x, y;
   char *mode;

   if (!PyArg_ParseTuple(args, "iiz", &x, &y, &mode)) return NULL;

   if (!mainptr->ClipboardHasText()) {
      PyErr_SetString(PyExc_RuntimeError, "Bad paste call: no pattern in clipboard.");
      return NULL;
   }

   // temporarily change selection rect and paste mode
   bigint oldleft = viewptr->selleft;
   bigint oldtop = viewptr->seltop;
   bigint oldright = viewptr->selright;
   bigint oldbottom = viewptr->selbottom;
   
   // create huge selection rect so no possibility of error message
   viewptr->selleft = x;
   viewptr->seltop = y;
   viewptr->selright = viewptr->selleft;   viewptr->selright += INT_MAX;
   viewptr->selbottom = viewptr->seltop;   viewptr->selbottom += INT_MAX;
   
   const char *oldmode = GetPasteMode();
   wxString modestr = wxT(mode);
   if      (modestr.IsSameAs(wxT("copy"), false)) SetPasteMode("Copy");
   else if (modestr.IsSameAs(wxT("or"), false))   SetPasteMode("Or");
   else if (modestr.IsSameAs(wxT("xor"), false))  SetPasteMode("Xor");
   else {
      PyErr_SetString(PyExc_RuntimeError, "Bad paste call: unknown mode.");
      return NULL;
   }

   viewptr->PasteClipboard(true);      // true = paste to selection

   // restore selection rect and paste mode
   viewptr->selleft = oldleft;
   viewptr->seltop = oldtop;
   viewptr->selright = oldright;
   viewptr->selbottom = oldbottom;
   SetPasteMode(oldmode);

   DoAutoUpdate();

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_shrink(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   if (viewptr->SelectionExists()) {
      viewptr->ShrinkSelection(false);    // false == don't fit in viewport
      DoAutoUpdate();
   } else {
      PyErr_SetString(PyExc_RuntimeError, "Bad shrink call: no selection.");
      return NULL;
   }

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_randfill(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   int perc;

   if (!PyArg_ParseTuple(args, "i", &perc)) return NULL;

   if (perc < 1 || perc > 100) {
      PyErr_SetString(PyExc_RuntimeError, "Bad randfill call: percentage must be from 1 to 100.");
      return NULL;
   }

   if (viewptr->SelectionExists()) {
      int oldperc = randomfill;
      randomfill = perc;
      viewptr->RandomFill();
      randomfill = oldperc;
      DoAutoUpdate();
   } else {
      PyErr_SetString(PyExc_RuntimeError, "Bad randfill call: no selection.");
      return NULL;
   }

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_flip(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   int direction;

   if (!PyArg_ParseTuple(args, "i", &direction)) return NULL;

   if (viewptr->SelectionExists()) {
      if (direction == 0)
         viewptr->FlipVertically();       // left-right
      else
         viewptr->FlipHorizontally();     // up-down
      DoAutoUpdate();
   } else {
      PyErr_SetString(PyExc_RuntimeError, "Bad flip call: no selection.");
      return NULL;
   }

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_rotate(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   int direction;

   if (!PyArg_ParseTuple(args, "i", &direction)) return NULL;

   if (viewptr->SelectionExists()) {
      viewptr->RotateSelection(direction == 0);    // 0 = clockwise
      DoAutoUpdate();
   } else {
      PyErr_SetString(PyExc_RuntimeError, "Bad rotate call: no selection.");
      return NULL;
   }

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_setpos(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   int x, y;

   if (!PyArg_ParseTuple(args, "ii", &x, &y)) return NULL;

   bigint bigx = x;
   bigint bigy = y;
   viewptr->SetPosMag(bigx, bigy, viewptr->GetMag());
   DoAutoUpdate();

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_getpos(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   bigint bigx, bigy;
   viewptr->GetPos(bigx, bigy);
   if (viewptr->OutsideLimits(bigy, bigx, bigy, bigx)) {
      PyErr_SetString(PyExc_RuntimeError, "Bad getpos call: cell outside integer limits.");
      return NULL;
   }

   // convert position to x,y tuple
   PyObject *xytuple = PyTuple_New(2);
   PyTuple_SetItem(xytuple, 0, PyInt_FromLong(bigx.toint()));
   PyTuple_SetItem(xytuple, 1, PyInt_FromLong(bigy.toint()));
   // note that PyTuple_SetItem steals the reference from PyInt_FromLong
   // so we don't need to use Py_DECREF
   
   return xytuple;
}

// -----------------------------------------------------------------------------

static PyObject *golly_setmag(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   int mag;

   if (!PyArg_ParseTuple(args, "i", &mag)) return NULL;

   viewptr->SetMag(mag);
   DoAutoUpdate();

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_getmag(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   PyObject *result = Py_BuildValue("i", viewptr->GetMag());
   return result;
}

// -----------------------------------------------------------------------------

static PyObject *golly_setoption(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *optname;
   int oldval, newval;

   if (!PyArg_ParseTuple(args, "zi", &optname, &newval)) return NULL;

   if (strcmp(optname, "autofit") == 0) {
      oldval = autofit ? 1 : 0;
      if (autofit != (newval != 0))
         mainptr->ToggleAutoFit();

   } else if (strcmp(optname, "hashing") == 0) {
      oldval = hashing ? 1 : 0;
      if (hashing != (newval != 0)) {
         mainptr->ToggleHashing();
         DoAutoUpdate();               // status bar color might change
      }

   } else if (strcmp(optname, "hyperspeed") == 0) {
      oldval = hyperspeed ? 1 : 0;
      if (hyperspeed != (newval != 0))
         mainptr->ToggleHyperspeed();

   } else if (strcmp(optname, "fullscreen") == 0) {
      oldval = mainptr->fullscreen ? 1 : 0;
      if (mainptr->fullscreen != (newval != 0)) {
         mainptr->ToggleFullScreen();
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "mindelay") == 0) {
      oldval = mindelay;
      if (newval < 0) newval = 0;
      if (newval > MAX_DELAY) newval = MAX_DELAY;
      if (mindelay != newval) {
         mindelay = newval;
         mainptr->UpdateWarp();
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "maxdelay") == 0) {
      oldval = maxdelay;
      if (newval < 0) newval = 0;
      if (newval > MAX_DELAY) newval = MAX_DELAY;
      if (maxdelay != newval) {
         maxdelay = newval;
         mainptr->UpdateWarp();
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "showpatterns") == 0) {
      oldval = showpatterns ? 1 : 0;
      if (showpatterns != (newval != 0)) {
         mainptr->ToggleShowPatterns();
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "showscripts") == 0) {
      oldval = showscripts ? 1 : 0;
      if (showscripts != (newval != 0)) {
         mainptr->ToggleShowScripts();
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "showstatusbar") == 0) {
      oldval = mainptr->StatusVisible() ? 1 : 0;
      if (mainptr->StatusVisible() != (newval != 0)) {
         mainptr->ToggleStatusBar();
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "showtoolbar") == 0) {
      oldval = mainptr->GetToolBar()->IsShown() ? 1 : 0;
      if (mainptr->GetToolBar()->IsShown() != (newval != 0)) {
         mainptr->ToggleToolBar();
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "blackcells") == 0) {
      oldval = blackcells ? 1 : 0;
      if (blackcells != (newval != 0)) {
         blackcells = (newval != 0);
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "showgrid") == 0) {
      oldval = showgridlines ? 1 : 0;
      if (showgridlines != (newval != 0)) {
         showgridlines = (newval != 0);
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "showboldlines") == 0) {
      oldval = showboldlines ? 1 : 0;
      if (showboldlines != (newval != 0)) {
         showboldlines = (newval != 0);
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "boldspacing") == 0) {
      oldval = boldspacing;
      if (newval < 2) newval = 2;
      if (newval > MAX_SPACING) newval = MAX_SPACING;
      if (boldspacing != newval) {
         boldspacing = newval;
         DoAutoUpdate();
      }
   
   } else {
      PyErr_SetString(PyExc_RuntimeError, "Bad setoption call: unknown option.");
      return NULL;
   }

   // return old value (simplifies saving and restoring settings)
   PyObject *result = Py_BuildValue("i", oldval);
   return result;
}

// -----------------------------------------------------------------------------

static PyObject *golly_getoption(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *optname;
   int optval;

   if (!PyArg_ParseTuple(args, "z", &optname)) return NULL;

   if      (strcmp(optname, "autofit") == 0)       optval = autofit ? 1 : 0;
   else if (strcmp(optname, "hashing") == 0)       optval = hashing ? 1 : 0;
   else if (strcmp(optname, "hyperspeed") == 0)    optval = hyperspeed ? 1 : 0;
   else if (strcmp(optname, "fullscreen") == 0)    optval = mainptr->fullscreen ? 1 : 0;
   else if (strcmp(optname, "mindelay") == 0)      optval = mindelay;
   else if (strcmp(optname, "maxdelay") == 0)      optval = maxdelay;
   else if (strcmp(optname, "showpatterns") == 0)  optval = showpatterns ? 1 : 0;
   else if (strcmp(optname, "showscripts") == 0)   optval = showscripts ? 1 : 0;
   else if (strcmp(optname, "showstatusbar") == 0) optval = mainptr->StatusVisible() ? 1 : 0;
   else if (strcmp(optname, "showtoolbar") == 0)   optval = mainptr->GetToolBar()->IsShown() ? 1 : 0;
   else if (strcmp(optname, "blackcells") == 0)    optval = blackcells ? 1 : 0;
   else if (strcmp(optname, "showgrid") == 0)      optval = showgridlines ? 1 : 0;
   else if (strcmp(optname, "showboldlines") == 0) optval = showboldlines ? 1 : 0;
   else if (strcmp(optname, "boldspacing") == 0)   optval = boldspacing;
   else {
      PyErr_SetString(PyExc_RuntimeError, "Bad getoption call: unknown option.");
      return NULL;
   }

   PyObject *result = Py_BuildValue("i", optval);
   return result;
}

// -----------------------------------------------------------------------------

static PyObject *golly_run(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   int ngens;

   if (!PyArg_ParseTuple(args, "i", &ngens)) return NULL;

   if (ngens > 0) {
      if (ngens > 1) {
         bigint saveinc = curralgo->getIncrement();
         curralgo->setIncrement(ngens);
         mainptr->NextGeneration(true);      // step by ngens
         curralgo->setIncrement(saveinc);
      } else {
         mainptr->NextGeneration(false);     // step 1 gen
      }
      DoAutoUpdate();
   }

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_step(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   mainptr->NextGeneration(true);      // step by current increment
   DoAutoUpdate();

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_setstep(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   int exp;

   if (!PyArg_ParseTuple(args, "i", &exp)) return NULL;

   mainptr->SetWarp(exp);
   DoAutoUpdate();

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_getstep(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   PyObject *result = Py_BuildValue("i", mainptr->GetWarp());
   return result;
}

// -----------------------------------------------------------------------------

static PyObject *golly_setbase(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   int base;

   if (!PyArg_ParseTuple(args, "i", &base)) return NULL;

   if (base < 2) base = 2;
   if (base > MAX_BASESTEP) base = MAX_BASESTEP;

   if (hashing) {
      hbasestep = base;
   } else {
      qbasestep = base;
   }
   mainptr->UpdateWarp();
   DoAutoUpdate();

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_getbase(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   PyObject *result = Py_BuildValue("i", hashing ? hbasestep : qbasestep);
   return result;
}

// -----------------------------------------------------------------------------

static PyObject *golly_advance(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   int where, ngens;

   if (!PyArg_ParseTuple(args, "ii", &where, &ngens)) return NULL;

   if (ngens > 0) {
      if (viewptr->SelectionExists()) {
         while (ngens > 0) {
            ngens--;
            if (where == 0)
               mainptr->AdvanceSelection();
            else
               mainptr->AdvanceOutsideSelection();
         }
         DoAutoUpdate();
      } else {
         PyErr_SetString(PyExc_RuntimeError, "Bad advance call: no selection.");
         return NULL;
      }
   }

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_reset(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   if (curralgo->getGeneration() != bigint::zero) {
      mainptr->ResetPattern();
      DoAutoUpdate();
   }

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_setrule(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *rule_string = NULL;

   if (!PyArg_ParseTuple(args, "z", &rule_string)) return NULL;

   wxString oldrule = wxT( curralgo->getrule() );
   const char *err;
   if (rule_string == NULL || rule_string[0] == 0) {
      err = curralgo->setrule("B3/S23");
   } else {
      err = curralgo->setrule(rule_string);
   }
   if (err) {
      curralgo->setrule( (char*)oldrule.c_str() );
      PyErr_SetString(PyExc_RuntimeError, err);
      return NULL;
   } else if ( global_liferules.hasB0notS8 && hashing ) {
      curralgo->setrule( (char*)oldrule.c_str() );
      PyErr_SetString(PyExc_RuntimeError, "B0-not-S8 rules are not allowed when hashing.");
      return NULL;
   } else {
      // show new rule in main window's title (but don't change name)
      mainptr->SetWindowTitle("");
   }

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_getrule(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   PyObject *result = Py_BuildValue("z", curralgo->getrule());
   return result;
}

// -----------------------------------------------------------------------------

static PyObject *golly_getgen(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   PyObject *result = Py_BuildValue("z", curralgo->getGeneration().tostring());
   return result;
}

// -----------------------------------------------------------------------------

static PyObject *golly_getpop(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   PyObject *result = Py_BuildValue("z", curralgo->getPopulation().tostring());
   return result;
}

// -----------------------------------------------------------------------------

// helper routine used in calls that build cell lists
static void AddCell(PyObject *list, long x, long y)
{
   PyObject *xo = PyInt_FromLong(x);
   PyObject *yo = PyInt_FromLong(y);
   PyList_Append(list, xo);
   PyList_Append(list, yo);
   // must decrement references to avoid Python memory leak
   Py_DECREF(xo);
   Py_DECREF(yo);
}   

// -----------------------------------------------------------------------------

// helper routine to extract cell list from given universe
static bool ExtractCells(PyObject *list, lifealgo *universe, bool shift = false)
{
   if ( !universe->isEmpty() ) {
      bigint top, left, bottom, right;
      universe->findedges(&top, &left, &bottom, &right);
      if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
         PyErr_SetString(PyExc_RuntimeError, "Universe is too big to extract all cells!");
         return false;
      }
      int itop = top.toint();
      int ileft = left.toint();
      int ibottom = bottom.toint();
      int iright = right.toint();
      int cx, cy;
      int cntr = 0;
      for ( cy=itop; cy<=ibottom; cy++ ) {
         for ( cx=ileft; cx<=iright; cx++ ) {
            int skip = universe->nextcell(cx, cy);
            if (skip >= 0) {
               // found next live cell in this row
               cx += skip;
               if (shift) {
                  // shift cells so that top left cell of bounding box is at 0,0
                  AddCell(list, cx - ileft, cy - itop);
               } else {
                  AddCell(list, cx, cy);
               }
            } else {
               cx = iright;  // done this row
            }
            cntr++;
            if ((cntr % 4096) == 0 && ScriptAborted()) return false;
         }
      }
   }
   return true;
}

// -----------------------------------------------------------------------------

static PyObject *golly_parse(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *s;
   long x0, y0, axx, axy, ayx, ayy;

   if (!PyArg_ParseTuple(args, "zllllll", &s, &x0, &y0, &axx, &axy, &ayx, &ayy))
      return NULL;

   PyObject *outlist = PyList_New(0);

   long x = 0, y = 0;

   if (strchr(s, '*')) {
      // parsing 'visual' format
      int c = *s++;
      while (c) {
         switch (c) {
         case '\n': if (x) { x = 0; y++; } break;
         case '.': x++; break;
         case '*':
            AddCell(outlist, x0 + x * axx + y * axy, y0 + x * ayx + y * ayy);
            x++;
            break;
         }
         c = *s++;
      }
   } else {
      // parsing 'RLE' format
      int prefix = 0;
      bool done = false;
      int c = *s++;
      while (c && !done) {
         if (isdigit(c))
            prefix = 10 * prefix + (c - '0');
         else {
            prefix += (prefix == 0);
            switch (c) {
            case '!': done = true; break;
            case '$': x = 0; y += prefix; break;
            case 'b': x += prefix; break;
            case 'o':
               for (int k = 0; k < prefix; k++, x++)
                  AddCell(outlist, x0 + x * axx + y * axy, y0 + x * ayx + y * ayy);
               break;
            }
            prefix = 0;
         }
         c = *s++;
      }
   }

   return outlist;
}

// -----------------------------------------------------------------------------

static PyObject *golly_transform(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   long x0, y0, axx, axy, ayx, ayy;
   PyObject *inlist;

   if (!PyArg_ParseTuple(args, "O!llllll", &PyList_Type, &inlist, &x0, &y0, &axx, &axy, &ayx, &ayy))
      return NULL;

   PyObject *outlist = PyList_New(0);

   int num_cells = PyList_Size(inlist) / 2;
   for (int n = 0; n < num_cells; n++) {
      long x = PyInt_AsLong( PyList_GetItem(inlist, 2 * n) );
      long y = PyInt_AsLong( PyList_GetItem(inlist, 2 * n + 1) );

      AddCell(outlist, x0 + x * axx + y * axy, y0 + x * ayx + y * ayy);

      if ((n % 4096) == 0 && ScriptAborted()) {
         Py_DECREF(outlist);
         return NULL;
      }
   }

   return outlist;
}

// -----------------------------------------------------------------------------

static PyObject *golly_evolve(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   int ngens = 0;
   PyObject *given_list;

   if (!PyArg_ParseTuple(args, "O!i", &PyList_Type, &given_list, &ngens)) return NULL;

   // create temporary qlife universe
   lifealgo *tempalgo;
   tempalgo = new qlifealgo();
   tempalgo->setpoll(wxGetApp().Poller());
   
   // no need for this -- all universes share global rule table
   // tempalgo->setrule( curralgo->getrule() );
   
   // copy cell list into temporary universe
   int num_cells = PyList_Size(given_list) / 2;
   for (int n = 0; n < num_cells; n++) {
      long x = PyInt_AsLong( PyList_GetItem(given_list, 2 * n) );
      long y = PyInt_AsLong( PyList_GetItem(given_list, 2 * n + 1) );

      tempalgo->setcell(x, y, 1);

      if ((n % 4096) == 0 && ScriptAborted()) {
         tempalgo->endofpattern();
         delete tempalgo;
         return NULL;
      }
   }
   tempalgo->endofpattern();

   // advance pattern by ngens
   mainptr->generating = true;
   tempalgo->setIncrement(ngens);
   tempalgo->step();
   mainptr->generating = false;

   // convert new pattern into a new cell list
   PyObject *outlist = PyList_New(0);
   bool done = ExtractCells(outlist, tempalgo);
   delete tempalgo;
   if (!done) {
      Py_DECREF(outlist);
      return NULL;
   }

   return outlist;
}

// -----------------------------------------------------------------------------

static PyObject *golly_load(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *file_name;

   if (!PyArg_ParseTuple(args, "z", &file_name)) return NULL;

   // create temporary qlife universe
   lifealgo *tempalgo;
   tempalgo = new qlifealgo();
   tempalgo->setpoll(wxGetApp().Poller());
   
   // readpatterm might change global rule table
   wxString oldrule = wxT( curralgo->getrule() );
   
   // read pattern into temporary universe
   const char *err = readpattern(file_name, *tempalgo);
   if (err && strcmp(err,cannotreadhash) == 0) {
      // macrocell file, so switch to hlife universe
      delete tempalgo;
      tempalgo = new hlifealgo();
      tempalgo->setpoll(wxGetApp().Poller());
      err = readpattern(file_name, *tempalgo);
   }
   
   // restore rule
   curralgo->setrule( (char*)oldrule.c_str() );

   if (err) {
      delete tempalgo;
      PyErr_SetString(PyExc_RuntimeError, err);
      return NULL;
   }
   
   // convert pattern into a cell list, shifting cell coords so that the
   // bounding box's top left cell is at 0,0
   PyObject *outlist = PyList_New(0);
   bool done = ExtractCells(outlist, tempalgo, true);
   delete tempalgo;
   if (!done) {
      Py_DECREF(outlist);
      return NULL;
   }

   return outlist;
}

// -----------------------------------------------------------------------------

static PyObject *golly_store(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   PyObject *given_list;
   char *file_name;
   char *desc = NULL;      // the description string is currently ignored!!!

   if (!PyArg_ParseTuple(args, "O!z|z", &PyList_Type, &given_list, &file_name, &desc))
      return NULL;

   // create temporary qlife universe
   lifealgo *tempalgo;
   tempalgo = new qlifealgo();
   tempalgo->setpoll(wxGetApp().Poller());
   
   // copy cell list into temporary universe
   int num_cells = PyList_Size(given_list) / 2;
   for (int n = 0; n < num_cells; n++) {
      long x = PyInt_AsLong( PyList_GetItem(given_list, 2 * n) );
      long y = PyInt_AsLong( PyList_GetItem(given_list, 2 * n + 1) );

      tempalgo->setcell(x, y, 1);

      if ((n % 4096) == 0 && ScriptAborted()) {
         tempalgo->endofpattern();
         delete tempalgo;
         return NULL;
      }
   }
   tempalgo->endofpattern();

   // write pattern to given file in RLE format
   bigint top, left, bottom, right;
   tempalgo->findedges(&top, &left, &bottom, &right);
   const char *err = writepattern(file_name, *tempalgo, RLE_format,
                                  top.toint(), left.toint(), bottom.toint(), right.toint());
   delete tempalgo;
   if (err) {
      PyErr_SetString(PyExc_RuntimeError, err);
      return NULL;
   }

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_putcells(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   long x0, y0, axx, axy, ayx, ayy;
   PyObject *list;

   if (!PyArg_ParseTuple(args, "O!llllll", &PyList_Type, &list, &x0, &y0, &axx, &axy, &ayx, &ayy))
      return NULL;

   int num_cells = PyList_Size(list) / 2;
   for (int n = 0; n < num_cells; n++) {
      long x = PyInt_AsLong( PyList_GetItem(list, 2 * n) );
      long y = PyInt_AsLong( PyList_GetItem(list, 2 * n + 1) );

      // paste (possibly transformed) cell into current universe
      curralgo->setcell(x0 + x * axx + y * axy, y0 + x * ayx + y * ayy, 1);
      
      if ((n % 4096) == 0 && ScriptAborted()) {
         curralgo->endofpattern();
         mainptr->savestart = true;
         return NULL;
      }
   }
   curralgo->endofpattern();
   mainptr->savestart = true;
   DoAutoUpdate();

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_getcells(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   PyObject *rect_list;

   if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &rect_list)) return NULL;
   
   // convert pattern in given rect into a cell list
   PyObject *outlist = PyList_New(0);

   int numitems = PyList_Size(rect_list);
   if (numitems == 0) {
      // return empty cell list
   } else if (numitems == 4) {
      int ileft = PyInt_AsLong( PyList_GetItem(rect_list, 0) );
      int itop = PyInt_AsLong( PyList_GetItem(rect_list, 1) );
      int wd = PyInt_AsLong( PyList_GetItem(rect_list, 2) );
      int ht = PyInt_AsLong( PyList_GetItem(rect_list, 3) );
      // first check that wd & ht are > 0
      if (wd <= 0) {
         PyErr_SetString(PyExc_RuntimeError, "Bad getcells call: width must be > 0.");
         Py_DECREF(outlist);
         return NULL;
      }
      if (ht <= 0) {
         PyErr_SetString(PyExc_RuntimeError, "Bad getcells call: height must be > 0.");
         Py_DECREF(outlist);
         return NULL;
      }
      int iright = ileft + wd - 1;
      int ibottom = itop + ht - 1;
      int cx, cy;
      int cntr = 0;
      for ( cy=itop; cy<=ibottom; cy++ ) {
         for ( cx=ileft; cx<=iright; cx++ ) {
            int skip = curralgo->nextcell(cx, cy);
            if (skip >= 0) {
               // found next live cell in this row
               cx += skip;
               if (cx <= iright) AddCell(outlist, cx, cy);
            } else {
               cx = iright;  // done this row
            }
            cntr++;
            if ((cntr % 4096) == 0 && ScriptAborted()) {
               Py_DECREF(outlist);
               return NULL;
            }
         }
      }
   } else {
      PyErr_SetString(PyExc_RuntimeError, "Bad getcells call: arg must be [] or [x,y,wd,ht].");
      Py_DECREF(outlist);
      return NULL;
   }

   return outlist;
}

// -----------------------------------------------------------------------------

static PyObject *golly_getclip(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   if (!mainptr->ClipboardHasText()) {
      PyErr_SetString(PyExc_RuntimeError, "Bad getclip call: no pattern in clipboard.");
      return NULL;
   }
   
   // convert pattern in clipboard into a cell list, but where the first 2 items
   // are the pattern's width and height (not necessarily the minimal bounding box
   // because the pattern might have empty borders, or it might even be empty)
   PyObject *outlist = PyList_New(0);

   // create a temporary universe for storing clipboard pattern
   lifealgo *tempalgo;
   tempalgo = new qlifealgo();               // qlife's setcell/getcell are faster
   tempalgo->setpoll(wxGetApp().Poller());

   // read clipboard pattern into temporary universe and set edges
   // (not a minimal bounding box if pattern is empty or has empty borders)
   bigint top, left, bottom, right;
   if ( viewptr->GetClipboardPattern(tempalgo, &top, &left, &bottom, &right) ) {
      if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
         PyErr_SetString(PyExc_RuntimeError, "Bad getclip call: pattern is too big.");
         Py_DECREF(outlist);
         return NULL;
      }
      int itop = top.toint();
      int ileft = left.toint();
      int ibottom = bottom.toint();
      int iright = right.toint();
      int wd = iright - ileft + 1;
      int ht = ibottom - itop + 1;

      AddCell(outlist, wd, ht);

      // extract cells from tempalgo
      int cx, cy;
      int cntr = 0;
      for ( cy=itop; cy<=ibottom; cy++ ) {
         for ( cx=ileft; cx<=iright; cx++ ) {
            int skip = tempalgo->nextcell(cx, cy);
            if (skip >= 0) {
               // found next live cell in this row
               cx += skip;
               // shift cells so that top left cell of bounding box is at 0,0
               AddCell(outlist, cx - ileft, cy - itop);
            } else {
               cx = iright;  // done this row
            }
            cntr++;
            if ((cntr % 4096) == 0 && ScriptAborted()) {
               delete tempalgo;
               Py_DECREF(outlist);
               return NULL;
            }
         }
      }

      delete tempalgo;
   } else {
      // assume error message has been displayed
      delete tempalgo;
      Py_DECREF(outlist);
      return NULL;
   }

   return outlist;
}

// -----------------------------------------------------------------------------

static PyObject *golly_visrect(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   PyObject *rect_list;

   if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &rect_list)) return NULL;

   int numitems = PyList_Size(rect_list);
   if (numitems != 4) {
      PyErr_SetString(PyExc_RuntimeError, "Bad visrect call: arg must be [x,y,wd,ht].");
      return NULL;
   }

   int x = PyInt_AsLong( PyList_GetItem(rect_list, 0) );
   int y = PyInt_AsLong( PyList_GetItem(rect_list, 1) );
   int wd = PyInt_AsLong( PyList_GetItem(rect_list, 2) );
   int ht = PyInt_AsLong( PyList_GetItem(rect_list, 3) );
   // check that wd & ht are > 0
   if (wd <= 0) {
      PyErr_SetString(PyExc_RuntimeError, "Bad visrect call: width must be > 0.");
      return NULL;
   }
   if (ht <= 0) {
      PyErr_SetString(PyExc_RuntimeError, "Bad visrect call: height must be > 0.");
      return NULL;
   }
   
   bigint left = x;
   bigint top = y;
   bigint right = x + wd - 1;
   bigint bottom = y + ht - 1;
   int visible = viewptr->CellVisible(left, top) &&
                 viewptr->CellVisible(right, bottom);
   
   PyObject *result = Py_BuildValue("i", visible);
   return result;
}

// -----------------------------------------------------------------------------

static PyObject *golly_select(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   PyObject *rect_list;
   int x, y, wd, ht;
   
   if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &rect_list)) return NULL;

   int numitems = PyList_Size(rect_list);
   if (numitems == 0) {
      // remove any existing selection
      viewptr->NoSelection();
   } else if (numitems == 4) {
      x = PyInt_AsLong( PyList_GetItem(rect_list, 0) );
      y = PyInt_AsLong( PyList_GetItem(rect_list, 1) );
      wd = PyInt_AsLong( PyList_GetItem(rect_list, 2) );
      ht = PyInt_AsLong( PyList_GetItem(rect_list, 3) );
      // first check that wd & ht are > 0
      if (wd <= 0) {
         PyErr_SetString(PyExc_RuntimeError, "Bad select call: width must be > 0.");
         return NULL;
      }
      if (ht <= 0) {
         PyErr_SetString(PyExc_RuntimeError, "Bad select call: height must be > 0.");
         return NULL;
      }
      // set selection edges
      viewptr->selleft = x;
      viewptr->seltop = y;
      viewptr->selright = x + wd - 1;
      viewptr->selbottom = y + ht - 1;
   } else {
      PyErr_SetString(PyExc_RuntimeError, "Bad select call: arg must be [] or [x,y,wd,ht].");
      return NULL;
   }

   DoAutoUpdate();

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_getrect(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   PyObject *outlist = PyList_New(0);

   if (!curralgo->isEmpty()) {
      bigint top, left, bottom, right;
      curralgo->findedges(&top, &left, &bottom, &right);
      if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
         PyErr_SetString(PyExc_RuntimeError, "Bad getrect call: pattern is too big.");
         Py_DECREF(outlist);
         return NULL;
      }
      long x = left.toint();
      long y = top.toint();
      long wd = right.toint() - x + 1;
      long ht = bottom.toint() - y + 1;
      
      AddCell(outlist, x, y);
      AddCell(outlist, wd, ht);
   }
   
   return outlist;
}

// -----------------------------------------------------------------------------

static PyObject *golly_getselrect(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   PyObject *outlist = PyList_New(0);

   if (viewptr->SelectionExists()) {
      if ( viewptr->OutsideLimits(viewptr->seltop, viewptr->selleft,
                                  viewptr->selbottom, viewptr->selright) ) {
         PyErr_SetString(PyExc_RuntimeError, "Bad getselrect call: selection is too big.");
         Py_DECREF(outlist);
         return NULL;
      }
      long x = viewptr->selleft.toint();
      long y = viewptr->seltop.toint();
      long wd = viewptr->selright.toint() - x + 1;
      long ht = viewptr->selbottom.toint() - y + 1;
        
      AddCell(outlist, x, y);
      AddCell(outlist, wd, ht);
   }
   
   return outlist;
}

// -----------------------------------------------------------------------------

static PyObject *golly_setcell(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   int x, y, state;

   if (!PyArg_ParseTuple(args, "iii", &x, &y, &state)) return NULL;

   curralgo->setcell(x, y, state);
   curralgo->endofpattern();
   mainptr->savestart = true;
   DoAutoUpdate();
   
   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_getcell(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   int x, y;

   if (!PyArg_ParseTuple(args, "ii", &x, &y)) return NULL;

   PyObject *result = Py_BuildValue("i", curralgo->getcell(x, y));
   return result;
}

// -----------------------------------------------------------------------------

static PyObject *golly_setcursor(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   int newindex;

   if (!PyArg_ParseTuple(args, "i", &newindex)) return NULL;

   int oldindex = CursorToIndex(currcurs);
   wxCursor* curs = IndexToCursor(newindex);
   if (curs) {
      viewptr->SetCursorMode(curs);
   } else {
      PyErr_SetString(PyExc_RuntimeError, "Bad setcursor call: unexpected cursor index.");
      return NULL;
   }

   // return old index (simplifies saving and restoring cursor)
   PyObject *result = Py_BuildValue("i", oldindex);
   return result;
}

// -----------------------------------------------------------------------------

static PyObject *golly_getcursor(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   PyObject *result = Py_BuildValue("i", CursorToIndex(currcurs));
   return result;
}

// -----------------------------------------------------------------------------

static PyObject *golly_update(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   // update viewport and status bar
   inscript = false;
   mainptr->UpdatePatternAndStatus();
   inscript = true;

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_autoupdate(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   int flag;

   if (!PyArg_ParseTuple(args, "i", &flag)) return NULL;

   autoupdate = (flag != 0);

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_getkey(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;
   
   char s[2];
   if (scriptkeys.Length() == 0) {
      // return empty string
      s[0] = '\0';
   } else {
      // return first char in scriptkeys and then remove it
      s[0] = scriptkeys.GetChar(0);
      s[1] = '\0';
      scriptkeys = scriptkeys.AfterFirst(s[0]);
   }
   PyObject *result = Py_BuildValue("z", s);
   return result;
}

// -----------------------------------------------------------------------------

static PyObject *golly_appdir(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   PyObject *result = Py_BuildValue("z", gollyloc.c_str());
   return result;
}

// -----------------------------------------------------------------------------

static PyObject *golly_show(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *s = NULL;

   if (!PyArg_ParseTuple(args, "z", &s)) return NULL;

   statusptr->DisplayMessage(s);

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_error(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *s = NULL;

   if (!PyArg_ParseTuple(args, "z", &s)) return NULL;

   statusptr->ErrorMessage(s);

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_warn(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *s = NULL;

   if (!PyArg_ParseTuple(args, "z", &s)) return NULL;

   Warning(s);

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_stderr(PyObject *self, PyObject *args)
{
   // probably safer not to call checkevents here
   // if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *s = NULL;

   if (!PyArg_ParseTuple(args, "z", &s)) return NULL;

   // accumulate stderr messages in global string for display after script finishes
   pyerror = wxT(s);

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyMethodDef golly_methods[] = {
   // filing
   { "open",         golly_open,       METH_VARARGS, "open given pattern file" },
   { "save",         golly_save,       METH_VARARGS, "save pattern in given file using given format" },
   { "load",         golly_load,       METH_VARARGS, "read pattern file and return cell list" },
   { "store",        golly_store,      METH_VARARGS, "write cell list to a file (in RLE format)" },
   { "appdir",       golly_appdir,     METH_VARARGS, "return location of Golly app" },
   // editing
   { "new",          golly_new,        METH_VARARGS, "create new universe and set window title" },
   { "cut",          golly_cut,        METH_VARARGS, "cut selection to clipboard" },
   { "copy",         golly_copy,       METH_VARARGS, "copy selection to clipboard" },
   { "clear",        golly_clear,      METH_VARARGS, "clear inside/outside selection" },
   { "paste",        golly_paste,      METH_VARARGS, "paste clipboard pattern at x,y using given mode" },
   { "shrink",       golly_shrink,     METH_VARARGS, "shrink selection" },
   { "randfill",     golly_randfill,   METH_VARARGS, "randomly fill selection to given percentage" },
   { "flip",         golly_flip,       METH_VARARGS, "flip selection left-right or up-down" },
   { "rotate",       golly_rotate,     METH_VARARGS, "rotate selection 90 deg clockwise or anticlockwise" },
   { "parse",        golly_parse,      METH_VARARGS, "parse RLE or Life 1.05 string and return cell list" },
   { "transform",    golly_transform,  METH_VARARGS, "apply an affine transformation to cell list" },
   { "evolve",       golly_evolve,     METH_VARARGS, "generate pattern contained in given cell list" },
   { "putcells",     golly_putcells,   METH_VARARGS, "paste given cell list into current universe" },
   { "getcells",     golly_getcells,   METH_VARARGS, "return cell list in given rectangle" },
   { "getclip",      golly_getclip,    METH_VARARGS, "return pattern in clipboard (as cell list)" },
   { "select",       golly_select,     METH_VARARGS, "select [x, y, wd, ht] rectangle or remove if []" },
   { "getrect",      golly_getrect,    METH_VARARGS, "return pattern rectangle as [] or [x, y, wd, ht]" },
   { "getselrect",   golly_getselrect, METH_VARARGS, "return selection rectangle as [] or [x, y, wd, ht]" },
   { "setcell",      golly_setcell,    METH_VARARGS, "set given cell to given state" },
   { "getcell",      golly_getcell,    METH_VARARGS, "get state of given cell" },
   { "setcursor",    golly_setcursor,  METH_VARARGS, "set cursor (returns old cursor)" },
   { "getcursor",    golly_getcursor,  METH_VARARGS, "return current cursor" },
   // control
   { "run",          golly_run,        METH_VARARGS, "run current pattern for given number of gens" },
   { "step",         golly_step,       METH_VARARGS, "run current pattern for current step" },
   { "setstep",      golly_setstep,    METH_VARARGS, "set step exponent" },
   { "getstep",      golly_getstep,    METH_VARARGS, "return current step exponent" },
   { "setbase",      golly_setbase,    METH_VARARGS, "set base step" },
   { "getbase",      golly_getbase,    METH_VARARGS, "return current base step" },
   { "advance",      golly_advance,    METH_VARARGS, "advance inside/outside selection by given gens" },
   { "reset",        golly_reset,      METH_VARARGS, "restore starting pattern" },
   { "getgen",       golly_getgen,     METH_VARARGS, "return current generation as string" },
   { "getpop",       golly_getpop,     METH_VARARGS, "return current population as string" },
   { "setrule",      golly_setrule,    METH_VARARGS, "set current rule according to string" },
   { "getrule",      golly_getrule,    METH_VARARGS, "return current rule string" },
   // viewing
   { "setpos",       golly_setpos,     METH_VARARGS, "move given cell to middle of viewport" },
   { "getpos",       golly_getpos,     METH_VARARGS, "return x,y position of cell in middle of viewport" },
   { "setmag",       golly_setmag,     METH_VARARGS, "set magnification (0=1:1, 1=1:2, -1=2:1, etc)" },
   { "getmag",       golly_getmag,     METH_VARARGS, "return current magnification" },
   { "fit",          golly_fit,        METH_VARARGS, "fit entire pattern in viewport" },
   { "fitsel",       golly_fitsel,     METH_VARARGS, "fit selection in viewport" },
   { "visrect",      golly_visrect,    METH_VARARGS, "return true if given rect is completely visible" },
   { "update",       golly_update,     METH_VARARGS, "update display (viewport and status bar)" },
   { "autoupdate",   golly_autoupdate, METH_VARARGS, "update display after each change to universe?" },
   // miscellaneous
   { "setoption",    golly_setoption,  METH_VARARGS, "set given option to new value (returns old value)" },
   { "getoption",    golly_getoption,  METH_VARARGS, "return current value of given option" },
   { "getkey",       golly_getkey,     METH_VARARGS, "return key hit by user or empty string if none" },
   { "show",         golly_show,       METH_VARARGS, "show given string in status bar" },
   { "error",        golly_error,      METH_VARARGS, "beep and show given string in status bar" },
   { "warn",         golly_warn,       METH_VARARGS, "show given string in warning dialog" },
   // for internal use (don't document)
   { "stderr",       golly_stderr,     METH_VARARGS, "save Python error message" },
   { NULL, NULL, 0, NULL }
};

// =============================================================================

bool InitPython()
{
   if (!pyinited) {
      // only initialize the Python interpreter once, mainly because multiple
      // Py_Initialize/Py_Finalize calls cause leaks of about 12K each time!
      Py_Initialize();
      if (!Py_IsInitialized()) {
         Warning("Could not initialize the Python interpreter!");
         return false;
      }
   
      // allow Python to call the above golly_* routines
      Py_InitModule3("golly", golly_methods, "internal golly routines");
   
      // catch Python messages sent to stderr and pass them to golly_stderr
      if ( PyRun_SimpleString(
            "import golly\n"
            "import sys\n"
            "class StderrCatcher:\n"
            "   def __init__(self):\n"
            "      self.data = ''\n"
            "   def write(self, stuff):\n"
            "      self.data += stuff\n"
            "      golly.stderr(self.data)\n"
            "sys.stderr = StderrCatcher()\n"
            ) < 0
         ) Warning("StderrCatcher code failed!");

      // nicer to reload all modules in case changes were made by user;
      // code comes from http://pyunit.sourceforge.net/notes/reloading.html
      /* unfortunately it causes an AttributeError!!!
      if ( PyRun_SimpleString(
            "import __builtin__\n"
            "class RollbackImporter:\n"
            "   def __init__(self):\n"
            "      self.previousModules = sys.modules.copy()\n"
            "      self.realImport = __builtin__.__import__\n"
            "      __builtin__.__import__ = self._import\n"
            "      self.newModules = {}\n"
            "   def _import(self, name, globals=None, locals=None, fromlist=[]):\n"
            "      result = apply(self.realImport, (name, globals, locals, fromlist))\n"
            "      self.newModules[name] = 1\n"
            "      return result\n"
            "   def uninstall(self):\n"
            "      for modname in self.newModules.keys():\n"
            "         if not self.previousModules.has_key(modname):\n"
            "            del(sys.modules[modname])\n"
            "      __builtin__.__import__ = self.realImport\n"
            "rollbackImporter = RollbackImporter()\n"
            ) < 0
         ) Warning("RollbackImporter code failed!");
      */
      
      pyinited = true;
   } else {
      // Py_Initialize has already been successfully called
      if ( PyRun_SimpleString(
            // Py_Finalize is not used to close stderr so reset it here
            "sys.stderr.data = ''\n"

            // reload all modules in case changes were made by user
            /* this almost works except for strange error the 2nd time we run gun-demo.py!!!
            "import sys\n"
            "for m in sys.modules.keys():\n"
            "   t = str(type(sys.modules[m]))\n"
            "   if t.find('module') < 0 or m == 'golly' or m == 'sys' or m[0] == '_':\n"
            "      pass\n"
            "   else:\n"
            "      reload(sys.modules[m])\n"
            */

            /* RollbackImporter code causes an error!!!
            "if rollbackImporter: rollbackImporter.uninstall()\n"
            "rollbackImporter = RollbackImporter()\n"
            */
            ) < 0
         ) Warning("PyRun_SimpleString failed!");
   }

   return true;
}

// -----------------------------------------------------------------------------

void CleanupPython()
{
   // no longer call Py_Finalize here
   // Py_Finalize();
}

// -----------------------------------------------------------------------------

void ExecuteScript(const wxString &filename)
{
   if (!InitPython()) return;

   wxString fname = filename;

   #ifdef __WXMAC__
      // convert fname from MacRoman to UTF8 so execfile can open names with 8-bit chars;
      // may not be needed after switching to CVS HEAD???!!!
      fname = wxString( fname.wc_str(wxConvLocal), wxConvUTF8 );
   #endif

   // if filename contains backslashes then we must convert them to "\\"
   // to avoid "\a" being treated as escape char
   fname.Replace("\\", "\\\\");

   // build absolute path to Golly's Scripts folder
   wxString scriptsdir = gollyloc + wxT("Scripts");
   scriptsdir.Replace("\\", "\\\\");

   // use PyRun_SimpleString to add Golly's Scripts folder to Python's
   // import search list (so scripts anywhere can do "from glife import *")
   // and then execute the given script
   wxString command = wxT("import sys ; sys.path.append('") + scriptsdir + wxT("')");
   command += wxT(" ; execfile('") + fname + wxT("')");
   PyRun_SimpleString(command.c_str());

   // note that PyRun_SimpleString returns -1 if an exception occurred;
   // the error message (in pyerror) is checked at the end of RunScript

   CleanupPython();
}

// -----------------------------------------------------------------------------

void CheckPythonError()
{
   if (!pyerror.IsEmpty()) {
      if (pyerror.Find(abortmsg) >= 0) {
         // error was caused by AbortScript so don't display pyerror
      } else {
         pyerror.Replace("  File \"<string>\", line 1, in ?\n", "");
         wxBell();
         wxSetCursor(*wxSTANDARD_CURSOR);
         wxMessageBox(pyerror, wxT("Script error:"), wxOK | wxICON_EXCLAMATION, wxGetActiveWindow());
      }
      statusptr->DisplayMessage("Script aborted.");
   }
}

// =============================================================================

// exported routines

void RunScript(const char* filename)
{
   if ( inscript ) return;    // play safe and avoid re-entrancy

   wxString fname = wxT(filename);
   mainptr->showbanner = false;
   statusptr->ClearMessage();
   pyerror.Clear();
   scriptkeys.Clear();
   autoupdate = false;

   // temporarily change current directory to location of script
   gollyloc = wxFileName::GetCwd();
   if ( gollyloc.Last() != wxFILE_SEP_PATH ) gollyloc += wxFILE_SEP_PATH;
   wxFileName fullname(fname);
   fullname.Normalize();
   scriptloc = fullname.GetPath();
   if ( scriptloc.Last() != wxFILE_SEP_PATH ) scriptloc += wxFILE_SEP_PATH;
   wxSetWorkingDirectory(scriptloc);
   
   // let user know we're busy running a script
   wxSetCursor(*wxHOURGLASS_CURSOR);
   viewptr->SetCursor(*wxHOURGLASS_CURSOR);
   mainptr->UpdateToolBar(false);
   mainptr->EnableAllMenus(false);
   
   wxGetApp().PollerReset();

   if ( !wxFileName::FileExists(fullname.GetFullPath()) ) {
      wxString err = wxT("The script file does not exist:\n") + fullname.GetFullPath();
      Warning(err.c_str());
   } else {
      inscript = true;
      ExecuteScript( fullname.GetFullPath() );
      inscript = false;
   }

   // restore current directory to location of Golly app
   wxSetWorkingDirectory(gollyloc);
      
   // update menu bar, cursor, viewport, status bar, tool bar, etc
   mainptr->EnableAllMenus(true);
   mainptr->UpdateEverything();
   
   // display any Python error message
   CheckPythonError();
}

// -----------------------------------------------------------------------------

bool IsScript(const char *filename)
{
   // currently we only support Python scripts, so return true if filename
   // ends with ".py" (ignoring case)
   wxString fname = wxT(filename);
   wxString ext = fname.AfterLast(wxT('.'));

   return ext.IsSameAs(wxT("py"), false);    // false == match any case
}

// -----------------------------------------------------------------------------

bool InScript()
{
   return inscript;
}

// -----------------------------------------------------------------------------

bool PassKeyToScript(char key)
{
   // called from checkevents
   if (inscript) {
      if (key == WXK_ESCAPE) {
         AbortScript();
         return true;
      } else /* if (key == ' ' || key == 'Q' || key == WXK_RETURN || key == WXK_TAB) */ {
               // if we want to allow some keyboard interaction while running a script
               // then we'll need to change the way we disable UpdatePatternAndStatus!!!
         
         // save key for possible consumption by golly_getkey
         scriptkeys += key;
         return true;
      }
   }
   return false;
}

// -----------------------------------------------------------------------------

void FinishScripting()
{
   // called when main window is closing (ie. app is quitting)
   if (inscript) {
      AbortScript();
      wxSetWorkingDirectory(gollyloc);
      inscript = false;
   }
   
   // Py_Finalize can cause an obvious delay, so best not to call it
   // if (pyinited) Py_Finalize();
}
