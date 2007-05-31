                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2007 Andrew Trevorrow and Tomas Rokicki.

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
#include "wxrender.h"      // for SetSelectionColor
#include "wxstatus.h"      // for statusptr->...
#include "wxutils.h"       // for Warning, Note
#include "wxprefs.h"       // for pythonlib, gollydir, etc
#include "wxinfo.h"        // for ShowInfo
#include "wxhelp.h"        // for ShowHelp
#include "wxlayer.h"       // for AddLayer, currlayer, etc
#include "wxscript.h"

// =============================================================================

// globals

bool pyinited = false;     // Py_Initialize has been successfully called?
bool inscript = false;     // a script is running?
bool plscript = false;     // a Perl script is running?
bool pyscript = false;     // a Python script is running?
bool canswitch;            // can user switch layers while script is running?
bool autoupdate;           // update display after each change to current universe?
bool exitcalled;           // *g_exit was called?
bool allowcheck;           // allow event checking?
bool showtitle;            // need to update window title?
wxString scripterr;        // Perl/Python error message
wxString scriptloc;        // location of script file
wxString scriptchars;      // non-escape chars saved by PassKeyToScript

// special message set by AbortPerlScript/AbortPythonScript
const char abortmsg[] = "GOLLY: ABORT SCRIPT";

// -----------------------------------------------------------------------------

//!!!??? on Windows this must come before including perl.h on
bool IsScript(const wxString& filename)
{
   // currently we support Perl or Python scripts, so return true if
   // filename ends with ".pl" or ".py" (ignoring case)
   wxString ext = filename.AfterLast(wxT('.'));
   return ext.IsSameAs(wxT("pl"), false) || ext.IsSameAs(wxT("py"), false);
}

// =============================================================================

// On Windows and Linux we need to load the Python library at runtime
// so Golly will start up even if Python isn't installed.
// Based on code from Mahogany (mahogany.sourceforge.net) and Vim (www.vim.org).

#ifndef __WXMAC__
   // load Python lib at runtime
   #define USE_PYTHON_DYNAMIC

   #ifdef __UNIX__
      // avoid warning on Linux
      #undef _POSIX_C_SOURCE
   #endif

   // prevent Python.h from adding Python library to link settings
   #define USE_DL_EXPORT
#endif

#include <Python.h>

#ifdef USE_PYTHON_DYNAMIC

#ifdef __WXMSW__
   // avoid warning on Windows
   #undef PyRun_SimpleString
#endif

#include "wx/dynlib.h"     // for wxDynamicLibrary

// declare G_* wrappers for the functions we want to use from Python lib
extern "C"
{
   // startup/shutdown
   void(*G_Py_Initialize)(void) = NULL;
   PyObject*(*G_Py_InitModule4)(char *, struct PyMethodDef *, char *, PyObject *, int) = NULL;
   void(*G_Py_Finalize)(void) = NULL;

   // errors
   PyObject*(*G_PyErr_Occurred)(void) = NULL;
   void(*G_PyErr_SetString)(PyObject *, const char *) = NULL;

   // ints
   long(*G_PyInt_AsLong)(PyObject *) = NULL;
   PyObject*(*G_PyInt_FromLong)(long) = NULL;
   PyTypeObject *G_PyInt_Type = NULL;

   // lists
   PyObject*(*G_PyList_New)(int size) = NULL;
   int(*G_PyList_Append)(PyObject *, PyObject *) = NULL;
   PyObject*(*G_PyList_GetItem)(PyObject *, int) = NULL;
   int(*G_PyList_SetItem)(PyObject *, int, PyObject *) = NULL;
   int(*G_PyList_Size)(PyObject *) = NULL;
   PyTypeObject* G_PyList_Type = NULL;

   // tuples
   PyObject *(*G_PyTuple_New)(int) = NULL;
   int(*G_PyTuple_SetItem)(PyObject *, int, PyObject *) = NULL;
   PyObject *(*G_PyTuple_GetItem)(PyObject *, int) = NULL;

   // misc
   int(*G_PyArg_Parse)(PyObject *, char *, ...) = NULL;
   int(*G_PyArg_ParseTuple)(PyObject *, char *, ...) = NULL;
   PyObject*(*G_PyImport_ImportModule)(const char *) = NULL;
   PyObject*(*G_PyDict_GetItemString)(PyObject *, const char *) = NULL;
   PyObject*(*G_PyModule_GetDict)(PyObject *) = NULL;
   PyObject*(*G_Py_BuildValue)(char *, ...) = NULL;
   PyObject*(*G_Py_FindMethod)(PyMethodDef[], PyObject *, char *) = NULL;
   int(*G_PyRun_SimpleString)(const char *) = NULL;
   PyObject* G__Py_NoneStruct = NULL;                    // used by Py_None
}

// redefine the Py* functions to their equivalent G_* wrappers
#define Py_Initialize G_Py_Initialize
#define Py_InitModule4 G_Py_InitModule4
#define Py_Finalize G_Py_Finalize
#define PyErr_Occurred G_PyErr_Occurred
#define PyErr_SetString G_PyErr_SetString
#define PyInt_AsLong G_PyInt_AsLong
#define PyInt_FromLong G_PyInt_FromLong
#define PyInt_Type (*G_PyInt_Type)
#define PyList_New G_PyList_New
#define PyList_Append G_PyList_Append
#define PyList_GetItem G_PyList_GetItem
#define PyList_SetItem G_PyList_SetItem
#define PyList_Size G_PyList_Size
#define PyList_Type (*G_PyList_Type)
#define PyTuple_New G_PyTuple_New
#define PyTuple_SetItem G_PyTuple_SetItem
#define PyTuple_GetItem G_PyTuple_GetItem
#define Py_BuildValue G_Py_BuildValue
#define PyArg_Parse G_PyArg_Parse
#define PyArg_ParseTuple G_PyArg_ParseTuple
#define PyDict_GetItemString G_PyDict_GetItemString
#define PyImport_ImportModule G_PyImport_ImportModule
#define PyModule_GetDict G_PyModule_GetDict
#define PyRun_SimpleString G_PyRun_SimpleString
#define _Py_NoneStruct (*G__Py_NoneStruct)

#ifdef __WXMSW__
   #define PYTHON_PROC FARPROC
#else
   #define PYTHON_PROC void *
#endif
#define PYTHON_FUNC(func) { _T(#func), (PYTHON_PROC *)&G_ ## func },

// store function names and their addresses in Python lib
static struct PythonFunc
{
   const wxChar *name;     // function name
   PYTHON_PROC *ptr;       // function pointer
} pythonFuncs[] =
{
   PYTHON_FUNC(Py_Initialize)
   PYTHON_FUNC(Py_InitModule4)
   PYTHON_FUNC(Py_Finalize)
   PYTHON_FUNC(PyErr_Occurred)
   PYTHON_FUNC(PyErr_SetString)
   PYTHON_FUNC(PyInt_AsLong)
   PYTHON_FUNC(PyInt_FromLong)
   PYTHON_FUNC(PyInt_Type)
   PYTHON_FUNC(PyList_New)
   PYTHON_FUNC(PyList_Append)
   PYTHON_FUNC(PyList_GetItem)
   PYTHON_FUNC(PyList_SetItem)
   PYTHON_FUNC(PyList_Size)
   PYTHON_FUNC(PyList_Type)
   PYTHON_FUNC(PyTuple_New)
   PYTHON_FUNC(PyTuple_SetItem)
   PYTHON_FUNC(PyTuple_GetItem)
   PYTHON_FUNC(Py_BuildValue)
   PYTHON_FUNC(PyArg_Parse)
   PYTHON_FUNC(PyArg_ParseTuple)
   PYTHON_FUNC(PyDict_GetItemString)
   PYTHON_FUNC(PyImport_ImportModule)
   PYTHON_FUNC(PyModule_GetDict)
   PYTHON_FUNC(PyRun_SimpleString)
   PYTHON_FUNC(_Py_NoneStruct)
   { _T(""), NULL }
};

// imported exception objects -- we can't import the symbols from the
// lib as this can cause errors (importing data symbols is not reliable)
static PyObject *imp_PyExc_RuntimeError = NULL;
static PyObject *imp_PyExc_KeyboardInterrupt = NULL;

#define PyExc_RuntimeError imp_PyExc_RuntimeError
#define PyExc_KeyboardInterrupt imp_PyExc_KeyboardInterrupt

static void GetPythonExceptions()
{
   PyObject *exmod = PyImport_ImportModule("exceptions");
   PyObject *exdict = PyModule_GetDict(exmod);
   PyExc_RuntimeError = PyDict_GetItemString(exdict, "RuntimeError");
   PyExc_KeyboardInterrupt = PyDict_GetItemString(exdict, "KeyboardInterrupt");
   Py_XINCREF(PyExc_RuntimeError);
   Py_XINCREF(PyExc_KeyboardInterrupt);
   Py_XDECREF(exmod);
}

// handle for Python lib
static wxDllType pythondll = NULL;

static void FreePythonLib()
{
   if ( pythondll ) {
      wxDynamicLibrary::Unload(pythondll);
      pythondll = NULL;
   }
}

static bool LoadPythonLib()
{
   // load the Python library
   wxDynamicLibrary dynlib;

   // don't log errors in here
   wxLogNull noLog;

   // wxDL_GLOBAL corresponds to RTLD_GLOBAL on Linux (ignored on Windows) and
   // is needed to avoid an ImportError when importing some modules (eg. time)
   while ( !dynlib.Load(pythonlib, wxDL_NOW | wxDL_VERBATIM | wxDL_GLOBAL) ) {
      // prompt user for a different Python library;
      // on Windows pythonlib should be something like "python24.dll"
      // and on Linux it should be something like "libpython2.4.so"
      wxBell();
      wxString str = _("If Python isn't installed then you'll have to Cancel,");
      str +=         _("\notherwise change the version numbers and try again.");
      #ifdef __WXMSW__
         str +=      _("\nDepending on where you installed Python you might have");
         str +=      _("\nto enter a full path like C:\\Python25\\python25.dll.");
      #endif
      wxTextEntryDialog dialog( wxGetActiveWindow(), str,
                                _("Could not load the Python library"),
                                pythonlib, wxOK | wxCANCEL );
      if (dialog.ShowModal() == wxID_OK) {
         pythonlib = dialog.GetValue();
      } else {
         return false;
      }
   }

   if ( dynlib.IsLoaded() ) {
      // load all functions named in pythonFuncs
      void *funcptr;
      PythonFunc *pf = pythonFuncs;
      while ( pf->ptr ) {
         funcptr = dynlib.GetSymbol(pf->name);
         if ( !funcptr ) {
            wxString err = _("Python library does not have this symbol:\n");
            err += pf->name;
            Warning(err);
            FreePythonLib();
            break;
         }

         *(pf++->ptr) = (PYTHON_PROC)funcptr;
      }

      if ( !pf->ptr ) {
         pythondll = dynlib.Detach();
      }
   }

   if ( pythondll == NULL ) {
      // should never happen
      Warning(_("Oh dear, the Python library is not loaded!"));
   }

   return pythondll != NULL;
}

#endif // USE_PYTHON_DYNAMIC

// =============================================================================

// The following pyg_* routines can be called from Python scripts; some are
// based on code in PLife's lifeint.cc (see http://plife.sourceforge.net/).

void AbortPythonScript()
{
   // raise an exception with a special message
   PyErr_SetString(PyExc_KeyboardInterrupt, abortmsg);
}

// -----------------------------------------------------------------------------

bool PythonScriptAborted()
{
   if (allowcheck) wxGetApp().Poller()->checkevents();

   // if user hit escape key then AbortPythonScript has raised an exception
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
   // show title at next update (eg. pyg_update or end of script)
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

// -----------------------------------------------------------------------------

static PyObject *pyg_new(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *title;

   if (!PyArg_ParseTuple(args, "s", &title)) return NULL;

   mainptr->NewPattern(wxString(title,wxConvLocal));
   DoAutoUpdate();

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_open(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *filename;
   int remember = 0;

   if (!PyArg_ParseTuple(args, "s|i", &filename, &remember)) return NULL;

   if (IsScript(wxString(filename,wxConvLocal))) {
      // avoid re-entrancy
      PyErr_SetString(PyExc_RuntimeError, "Bad open call: cannot open a script file.");
      return NULL;
   }

   // convert non-absolute filename to absolute path relative to scriptloc
   // so it can be selected later from Open Recent submenu
   wxString fname = wxString(filename,wxConvLocal);
   wxFileName fullname(fname);
   if (!fullname.IsAbsolute()) fullname = scriptloc + fname;

   // only add file to Open Recent submenu if remember flag is non-zero
   mainptr->OpenFile(fullname.GetFullPath(), remember != 0);
   DoAutoUpdate();

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_save(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *filename;
   char *format;
   int remember = 0;

   if (!PyArg_ParseTuple(args, "ss|i", &filename, &format, &remember)) return NULL;

   // convert non-absolute filename to absolute path relative to scriptloc
   // so it can be selected later from Open Recent submenu
   wxString fname = wxString(filename,wxConvLocal);
   wxFileName fullname(fname);
   if (!fullname.IsAbsolute()) fullname = scriptloc + fname;

   // only add file to Open Recent submenu if remember flag is non-zero
   wxString err = mainptr->SaveFile(fullname.GetFullPath(),
                                    wxString(format,wxConvLocal), remember != 0);
   if (!err.IsEmpty()) {
      PyErr_SetString(PyExc_RuntimeError, err.mb_str(wxConvLocal));
      return NULL;
   }

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_fit(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   viewptr->FitPattern();
   DoAutoUpdate();

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_fitsel(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
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

static PyObject *pyg_cut(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
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

static PyObject *pyg_copy(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
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

static PyObject *pyg_clear(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
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

static PyObject *pyg_paste(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   int x, y;
   char *mode;

   if (!PyArg_ParseTuple(args, "iis", &x, &y, &mode)) return NULL;

   if (!mainptr->ClipboardHasText()) {
      PyErr_SetString(PyExc_RuntimeError, "Bad paste call: no pattern in clipboard.");
      return NULL;
   }

   // temporarily change selection rect and paste mode
   bigint oldleft = currlayer->selleft;
   bigint oldtop = currlayer->seltop;
   bigint oldright = currlayer->selright;
   bigint oldbottom = currlayer->selbottom;

   const char *oldmode = GetPasteMode();
   wxString modestr = wxString(mode, wxConvLocal);
   if      (modestr.IsSameAs(wxT("copy"), false)) SetPasteMode("Copy");
   else if (modestr.IsSameAs(wxT("or"), false))   SetPasteMode("Or");
   else if (modestr.IsSameAs(wxT("xor"), false))  SetPasteMode("Xor");
   else {
      PyErr_SetString(PyExc_RuntimeError, "Bad paste call: unknown mode.");
      return NULL;
   }

   // create huge selection rect so no possibility of error message
   currlayer->selleft = x;
   currlayer->seltop = y;
   currlayer->selright = currlayer->selleft;   currlayer->selright += INT_MAX;
   currlayer->selbottom = currlayer->seltop;   currlayer->selbottom += INT_MAX;

   viewptr->PasteClipboard(true);      // true = paste to selection

   // restore selection rect and paste mode
   currlayer->selleft = oldleft;
   currlayer->seltop = oldtop;
   currlayer->selright = oldright;
   currlayer->selbottom = oldbottom;
   SetPasteMode(oldmode);

   DoAutoUpdate();

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_shrink(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
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

static PyObject *pyg_randfill(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
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

static PyObject *pyg_flip(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   int direction;

   if (!PyArg_ParseTuple(args, "i", &direction)) return NULL;

   if (viewptr->SelectionExists()) {
      if (direction == 0)
         viewptr->FlipLeftRight();
      else
         viewptr->FlipTopBottom();
      DoAutoUpdate();
   } else {
      PyErr_SetString(PyExc_RuntimeError, "Bad flip call: no selection.");
      return NULL;
   }

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_rotate(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
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

static PyObject *pyg_setpos(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *x;
   char *y;

   if (!PyArg_ParseTuple(args, "ss", &x, &y)) return NULL;

   // disallow alphabetic chars in x,y
   int i;
   int xlen = strlen(x);
   for (i=0; i<xlen; i++)
      if ( (x[i] >= 'a' && x[i] <= 'z') || (x[i] >= 'A' && x[i] <= 'Z') ) {
         PyErr_SetString(PyExc_RuntimeError, "Bad setpos call: illegal character in x value.");
         return NULL;
      }
   int ylen = strlen(y);
   for (i=0; i<ylen; i++)
      if ( (y[i] >= 'a' && y[i] <= 'z') || (y[i] >= 'A' && y[i] <= 'Z') ) {
         PyErr_SetString(PyExc_RuntimeError, "Bad setpos call: illegal character in y value.");
         return NULL;
      }

   bigint bigx(x);
   bigint bigy(y);
   viewptr->SetPosMag(bigx, bigy, viewptr->GetMag());
   DoAutoUpdate();

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_getpos(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   char sepchar = '\0';

   if (!PyArg_ParseTuple(args, "|c", &sepchar)) return NULL;

   bigint bigx, bigy;
   viewptr->GetPos(bigx, bigy);

   // return position as x,y tuple
   PyObject *xytuple = PyTuple_New(2);
   PyTuple_SetItem(xytuple, 0, Py_BuildValue("s",bigx.tostring(sepchar)));
   PyTuple_SetItem(xytuple, 1, Py_BuildValue("s",bigy.tostring(sepchar)));
   return xytuple;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_setmag(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   int mag;

   if (!PyArg_ParseTuple(args, "i", &mag)) return NULL;

   viewptr->SetMag(mag);
   DoAutoUpdate();

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_getmag(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   return Py_BuildValue("i", viewptr->GetMag());
}

// -----------------------------------------------------------------------------

static PyObject *pyg_addlayer(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   if (numlayers >= maxlayers) {
      PyErr_SetString(PyExc_RuntimeError, "Bad addlayer call: no more layers can be added.");
      return NULL;
   } else {
      AddLayer();
      DoAutoUpdate();
   }

   // return index of new layer
   return Py_BuildValue("i", currindex);
}

// -----------------------------------------------------------------------------

static PyObject *pyg_clone(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   if (numlayers >= maxlayers) {
      PyErr_SetString(PyExc_RuntimeError, "Bad clone call: no more layers can be added.");
      return NULL;
   } else {
      CloneLayer();
      DoAutoUpdate();
   }

   // return index of new layer
   return Py_BuildValue("i", currindex);
}

// -----------------------------------------------------------------------------

static PyObject *pyg_duplicate(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   if (numlayers >= maxlayers) {
      PyErr_SetString(PyExc_RuntimeError, "Bad duplicate call: no more layers can be added.");
      return NULL;
   } else {
      DuplicateLayer();
      DoAutoUpdate();
   }

   // return index of new layer
   return Py_BuildValue("i", currindex);
}

// -----------------------------------------------------------------------------

static PyObject *pyg_dellayer(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   if (numlayers <= 1) {
      PyErr_SetString(PyExc_RuntimeError, "Bad dellayer call: there is only one layer.");
      return NULL;
   } else {
      DeleteLayer();
      DoAutoUpdate();
   }

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_movelayer(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   int fromindex, toindex;

   if (!PyArg_ParseTuple(args, "ii", &fromindex, &toindex)) return NULL;

   if (fromindex < 0 || fromindex >= numlayers) {
      char msg[64];
      sprintf(msg, "Bad movelayer fromindex: %d", fromindex);
      PyErr_SetString(PyExc_RuntimeError, msg);
      return NULL;
   }
   if (toindex < 0 || toindex >= numlayers) {
      char msg[64];
      sprintf(msg, "Bad movelayer toindex: %d", toindex);
      PyErr_SetString(PyExc_RuntimeError, msg);
      return NULL;
   }

   MoveLayer(fromindex, toindex);
   DoAutoUpdate();

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_setlayer(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   int index;

   if (!PyArg_ParseTuple(args, "i", &index)) return NULL;

   if (index < 0 || index >= numlayers) {
      char msg[64];
      sprintf(msg, "Bad setlayer index: %d", index);
      PyErr_SetString(PyExc_RuntimeError, msg);
      return NULL;
   }

   SetLayer(index);
   DoAutoUpdate();

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_getlayer(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   return Py_BuildValue("i", currindex);
}

// -----------------------------------------------------------------------------

static PyObject *pyg_numlayers(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   return Py_BuildValue("i", numlayers);
}

// -----------------------------------------------------------------------------

static PyObject *pyg_maxlayers(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   return Py_BuildValue("i", maxlayers);
}

// -----------------------------------------------------------------------------

static PyObject *pyg_setname(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *name;
   int index = currindex;

   if (!PyArg_ParseTuple(args, "s|i", &name, &index)) return NULL;

   if (index < 0 || index >= numlayers) {
      char msg[64];
      sprintf(msg, "Bad setname index: %d", index);
      PyErr_SetString(PyExc_RuntimeError, msg);
      return NULL;
   }

   if (name[0]) {
      if (index == currindex) {
         // show new name in main window's title;
         // also sets currlayer->currname and updates menu item
         ChangeWindowTitle(wxString(name, wxConvLocal));
      } else {
         GetLayer(index)->currname = wxString(name, wxConvLocal);
         // show name in given layer's menu item
         mainptr->UpdateLayerItem(index);
      }
   }

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_getname(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   int index = currindex;

   if (!PyArg_ParseTuple(args, "|i", &index)) return NULL;

   if (index < 0 || index >= numlayers) {
      char msg[64];
      sprintf(msg, "Bad getname index: %d", index);
      PyErr_SetString(PyExc_RuntimeError, msg);
      return NULL;
   }

   const char* name = GetLayer(index)->currname.mb_str(wxConvLocal);
   return Py_BuildValue("s", name);
}

// -----------------------------------------------------------------------------

static PyObject *pyg_setoption(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *optname;
   int oldval, newval;

   if (!PyArg_ParseTuple(args, "si", &optname, &newval)) return NULL;

   if (strcmp(optname, "autofit") == 0) {
      oldval = currlayer->autofit ? 1 : 0;
      if (oldval != newval) {
         mainptr->ToggleAutoFit();
         // autofit option only applies to a generating pattern
         // DoAutoUpdate();
      }

   } else if (strcmp(optname, "fullscreen") == 0) {
      oldval = mainptr->fullscreen ? 1 : 0;
      if (oldval != newval) {
         mainptr->ToggleFullScreen();
         // above always does an update (due to resizing viewport window)
         // DoAutoUpdate();
      }

   } else if (strcmp(optname, "hashing") == 0) {
      oldval = currlayer->hash ? 1 : 0;
      if (oldval != newval) {
         mainptr->ToggleHashing();
         DoAutoUpdate();               // status bar color might change
      }

   } else if (strcmp(optname, "hyperspeed") == 0) {
      oldval = currlayer->hyperspeed ? 1 : 0;
      if (oldval != newval)
         mainptr->ToggleHyperspeed();

   } else if (strcmp(optname, "showhashinfo") == 0) {
      oldval = currlayer->showhashinfo ? 1 : 0;
      if (oldval != newval)
         mainptr->ToggleHashInfo();

   } else if (strcmp(optname, "mindelay") == 0) {
      oldval = mindelay;
      if (newval < 0) newval = 0;
      if (newval > MAX_DELAY) newval = MAX_DELAY;
      if (oldval != newval) {
         mindelay = newval;
         mainptr->UpdateWarp();
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "maxdelay") == 0) {
      oldval = maxdelay;
      if (newval < 0) newval = 0;
      if (newval > MAX_DELAY) newval = MAX_DELAY;
      if (oldval != newval) {
         maxdelay = newval;
         mainptr->UpdateWarp();
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "opacity") == 0) {
      oldval = opacity;
      if (newval < 1) newval = 1;
      if (newval > 100) newval = 100;
      if (oldval != newval) {
         opacity = newval;
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "showlayerbar") == 0) {
      oldval = showlayer ? 1 : 0;
      if (oldval != newval) {
         ToggleLayerBar();
         // above always does an update (due to resizing viewport window)
         // DoAutoUpdate();
      }

   } else if (strcmp(optname, "showpatterns") == 0) {
      oldval = showpatterns ? 1 : 0;
      if (oldval != newval) {
         mainptr->ToggleShowPatterns();
         // above always does an update (due to resizing viewport window)
         // DoAutoUpdate();
      }

   } else if (strcmp(optname, "showscripts") == 0) {
      oldval = showscripts ? 1 : 0;
      if (oldval != newval) {
         mainptr->ToggleShowScripts();
         // above always does an update (due to resizing viewport window)
         // DoAutoUpdate();
      }

   } else if (strcmp(optname, "showtoolbar") == 0) {
      oldval = showtool ? 1 : 0;
      if (oldval != newval) {
         mainptr->ToggleToolBar();
         // above always does an update (due to resizing viewport window)
         // DoAutoUpdate();
      }

   } else if (strcmp(optname, "showstatusbar") == 0) {
      oldval = showstatus ? 1 : 0;
      if (oldval != newval) {
         mainptr->ToggleStatusBar();
         // above always does an update (due to resizing viewport window)
         // DoAutoUpdate();
      }

   } else if (strcmp(optname, "showexact") == 0) {
      oldval = showexact ? 1 : 0;
      if (oldval != newval) {
         mainptr->ToggleExactNumbers();
         // above always does an update (due to resizing viewport window)
         // DoAutoUpdate();
      }

   } else if (strcmp(optname, "swapcolors") == 0) {
      oldval = swapcolors ? 1 : 0;
      if (oldval != newval) {
         viewptr->ToggleCellColors();
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "showgrid") == 0) {
      oldval = showgridlines ? 1 : 0;
      if (oldval != newval) {
         showgridlines = !showgridlines;
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "showboldlines") == 0) {
      oldval = showboldlines ? 1 : 0;
      if (oldval != newval) {
         showboldlines = !showboldlines;
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "syncviews") == 0) {
      oldval = syncviews ? 1 : 0;
      if (oldval != newval) {
         ToggleSyncViews();
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "synccursors") == 0) {
      oldval = synccursors ? 1 : 0;
      if (oldval != newval) {
         ToggleSyncCursors();
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "switchlayers") == 0) {
      oldval = canswitch ? 1 : 0;
      if (oldval != newval) {
         canswitch = !canswitch;
         // no need for DoAutoUpdate();
      }

   } else if (strcmp(optname, "stacklayers") == 0) {
      oldval = stacklayers ? 1 : 0;
      if (oldval != newval) {
         ToggleStackLayers();
         // above always does an update
         // DoAutoUpdate();
      }

   } else if (strcmp(optname, "tilelayers") == 0) {
      oldval = tilelayers ? 1 : 0;
      if (oldval != newval) {
         ToggleTileLayers();
         // above always does an update
         // DoAutoUpdate();
      }

   } else if (strcmp(optname, "boldspacing") == 0) {
      oldval = boldspacing;
      if (newval < 2) newval = 2;
      if (newval > MAX_SPACING) newval = MAX_SPACING;
      if (oldval != newval) {
         boldspacing = newval;
         DoAutoUpdate();
      }

   } else if (strcmp(optname, "savexrle") == 0) {
      oldval = savexrle ? 1 : 0;
      if (oldval != newval) {
         savexrle = !savexrle;
         // no need for DoAutoUpdate();
      }

   } else {
      PyErr_SetString(PyExc_RuntimeError, "Bad setoption call: unknown option.");
      return NULL;
   }

   if (oldval != newval) {
      mainptr->UpdateMenuItems(mainptr->IsActive());
   }

   // return old value (simplifies saving and restoring settings)
   return Py_BuildValue("i", oldval);
}

// -----------------------------------------------------------------------------

static PyObject *pyg_getoption(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *optname;
   int optval;

   if (!PyArg_ParseTuple(args, "s", &optname)) return NULL;

   if      (strcmp(optname, "autofit") == 0)       optval = currlayer->autofit ? 1 : 0;
   else if (strcmp(optname, "boldspacing") == 0)   optval = boldspacing;
   else if (strcmp(optname, "fullscreen") == 0)    optval = mainptr->fullscreen ? 1 : 0;
   else if (strcmp(optname, "hashing") == 0)       optval = currlayer->hash ? 1 : 0;
   else if (strcmp(optname, "hyperspeed") == 0)    optval = currlayer->hyperspeed ? 1 : 0;
   else if (strcmp(optname, "mindelay") == 0)      optval = mindelay;
   else if (strcmp(optname, "maxdelay") == 0)      optval = maxdelay;
   else if (strcmp(optname, "opacity") == 0)       optval = opacity;
   else if (strcmp(optname, "savexrle") == 0)      optval = savexrle ? 1 : 0;
   else if (strcmp(optname, "showboldlines") == 0) optval = showboldlines ? 1 : 0;
   else if (strcmp(optname, "showexact") == 0)     optval = showexact ? 1 : 0;
   else if (strcmp(optname, "showgrid") == 0)      optval = showgridlines ? 1 : 0;
   else if (strcmp(optname, "showhashinfo") == 0)  optval = currlayer->showhashinfo ? 1 : 0;
   else if (strcmp(optname, "showlayerbar") == 0)  optval = showlayer ? 1 : 0;
   else if (strcmp(optname, "showpatterns") == 0)  optval = showpatterns ? 1 : 0;
   else if (strcmp(optname, "showscripts") == 0)   optval = showscripts ? 1 : 0;
   else if (strcmp(optname, "showstatusbar") == 0) optval = showstatus ? 1 : 0;
   else if (strcmp(optname, "showtoolbar") == 0)   optval = showtool ? 1 : 0;
   else if (strcmp(optname, "stacklayers") == 0)   optval = stacklayers ? 1 : 0;
   else if (strcmp(optname, "swapcolors") == 0)    optval = swapcolors ? 1 : 0;
   else if (strcmp(optname, "switchlayers") == 0)  optval = canswitch ? 1 : 0;
   else if (strcmp(optname, "synccursors") == 0)   optval = synccursors ? 1 : 0;
   else if (strcmp(optname, "syncviews") == 0)     optval = syncviews ? 1 : 0;
   else if (strcmp(optname, "tilelayers") == 0)    optval = tilelayers ? 1 : 0;
   else {
      PyErr_SetString(PyExc_RuntimeError, "Bad getoption call: unknown option.");
      return NULL;
   }

   return Py_BuildValue("i", optval);
}

// -----------------------------------------------------------------------------

static PyObject *pyg_setcolor(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   char* colname;
   int r, g, b;
   wxColor oldcol;

   if (!PyArg_ParseTuple(args, "siii", &colname, &r, &g, &b)) return NULL;

   wxColor newcol(r, g, b);

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

   } else if (strcmp(colname, "hashing") == 0) {
      oldcol = *hlifergb;
      if (oldcol != newcol) {
         *hlifergb = newcol;
         SetBrushesAndPens();
         DoAutoUpdate();
      }

   } else if (strcmp(colname, "nothashing") == 0) {
      oldcol = *qlifergb;
      if (oldcol != newcol) {
         *qlifergb = newcol;
         SetBrushesAndPens();
         DoAutoUpdate();
      }

   } else {
      PyErr_SetString(PyExc_RuntimeError, "Bad setcolor call: unknown color.");
      return NULL;
   }

   // return old r,g,b values (simplifies saving and restoring colors)
   PyObject* rgbtuple = PyTuple_New(3);
   PyTuple_SetItem(rgbtuple, 0, Py_BuildValue("i",oldcol.Red()));
   PyTuple_SetItem(rgbtuple, 1, Py_BuildValue("i",oldcol.Green()));
   PyTuple_SetItem(rgbtuple, 2, Py_BuildValue("i",oldcol.Blue()));
   return rgbtuple;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_getcolor(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   char* colname;
   wxColor* cptr;

   if (!PyArg_ParseTuple(args, "s", &colname)) return NULL;

   // note that "livecells" = "livecells0"
   if (strncmp(colname, "livecells", 9) == 0) {
      int layer = 0;
      if (colname[9] >= '0' && colname[9] <= '9') {
         layer = colname[9] - '0';
      }
      cptr = livergb[layer];
   }
   else if (strcmp(colname, "deadcells") == 0)     cptr = deadrgb;
   else if (strcmp(colname, "paste") == 0)         cptr = pastergb;
   else if (strcmp(colname, "select") == 0)        cptr = selectrgb;
   else if (strcmp(colname, "hashing") == 0)       cptr = hlifergb;
   else if (strcmp(colname, "nothashing") == 0)    cptr = qlifergb;
   else {
      PyErr_SetString(PyExc_RuntimeError, "Bad getcolor call: unknown color.");
      return NULL;
   }

   // return r,g,b tuple
   PyObject* rgbtuple = PyTuple_New(3);
   PyTuple_SetItem(rgbtuple, 0, Py_BuildValue("i",cptr->Red()));
   PyTuple_SetItem(rgbtuple, 1, Py_BuildValue("i",cptr->Green()));
   PyTuple_SetItem(rgbtuple, 2, Py_BuildValue("i",cptr->Blue()));
   return rgbtuple;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_empty(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   return Py_BuildValue("i", currlayer->algo->isEmpty() ? 1 : 0);
}

// -----------------------------------------------------------------------------

static PyObject *pyg_run(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   int ngens;

   if (!PyArg_ParseTuple(args, "i", &ngens)) return NULL;

   if (ngens > 0 && !currlayer->algo->isEmpty()) {
      if (ngens > 1) {
         bigint saveinc = currlayer->algo->getIncrement();
         currlayer->algo->setIncrement(ngens);
         mainptr->NextGeneration(true);            // step by ngens
         currlayer->algo->setIncrement(saveinc);
      } else {
         mainptr->NextGeneration(false);           // step 1 gen
      }
      DoAutoUpdate();
   }

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_step(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   if (!currlayer->algo->isEmpty()) {
      mainptr->NextGeneration(true);      // step by current increment
      DoAutoUpdate();
   }

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_setstep(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   int exp;

   if (!PyArg_ParseTuple(args, "i", &exp)) return NULL;

   mainptr->SetWarp(exp);
   DoAutoUpdate();

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_getstep(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   return Py_BuildValue("i", currlayer->warp);
}

// -----------------------------------------------------------------------------

static PyObject *pyg_setbase(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   int base;

   if (!PyArg_ParseTuple(args, "i", &base)) return NULL;

   if (base < 2) base = 2;
   if (base > MAX_BASESTEP) base = MAX_BASESTEP;

   if (currlayer->hash) {
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

static PyObject *pyg_getbase(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   return Py_BuildValue("i", currlayer->hash ? hbasestep : qbasestep);
}

// -----------------------------------------------------------------------------

static PyObject *pyg_advance(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
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

static PyObject *pyg_reset(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   if (currlayer->algo->getGeneration() != bigint::zero) {
      mainptr->ResetPattern();
      DoAutoUpdate();
   }

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_getgen(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   char sepchar = '\0';

   if (!PyArg_ParseTuple(args, "|c", &sepchar)) return NULL;

   return Py_BuildValue("s", currlayer->algo->getGeneration().tostring(sepchar));
}

// -----------------------------------------------------------------------------

static PyObject *pyg_getpop(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   char sepchar = '\0';

   if (!PyArg_ParseTuple(args, "|c", &sepchar)) return NULL;

   return Py_BuildValue("s", currlayer->algo->getPopulation().tostring(sepchar));
}

// -----------------------------------------------------------------------------

static PyObject *pyg_setrule(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *rule_string = NULL;

   if (!PyArg_ParseTuple(args, "s", &rule_string)) return NULL;

   wxString oldrule = wxString(currlayer->algo->getrule(), wxConvLocal);
   const char *err;
   if (rule_string == NULL || rule_string[0] == 0) {
      err = currlayer->algo->setrule("B3/S23");
   } else {
      err = currlayer->algo->setrule(rule_string);
   }
   if (err) {
      currlayer->algo->setrule( oldrule.mb_str(wxConvLocal) );
      PyErr_SetString(PyExc_RuntimeError, err);
      return NULL;
   } else if ( global_liferules.hasB0notS8 && currlayer->hash ) {
      currlayer->algo->setrule( oldrule.mb_str(wxConvLocal) );
      PyErr_SetString(PyExc_RuntimeError, "B0-not-S8 rules are not allowed when hashing.");
      return NULL;
   } else {
      // show new rule in main window's title but don't change name
      ChangeWindowTitle(wxEmptyString);
   }

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_getrule(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   return Py_BuildValue("s", currlayer->algo->getrule());
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
            if ((cntr % 4096) == 0 && PythonScriptAborted()) return false;
         }
      }
   }
   return true;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_parse(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *s;
   long x0, y0, axx, axy, ayx, ayy;

   if (!PyArg_ParseTuple(args, "sllllll", &s, &x0, &y0, &axx, &axy, &ayx, &ayy))
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

static PyObject *pyg_transform(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
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

      if ((n % 4096) == 0 && PythonScriptAborted()) {
         Py_DECREF(outlist);
         return NULL;
      }
   }

   return outlist;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_evolve(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   int ngens = 0;
   PyObject *given_list;

   if (!PyArg_ParseTuple(args, "O!i", &PyList_Type, &given_list, &ngens)) return NULL;

   // create a temporary universe of same type as current universe so we
   // don't have to update the global rule table (in case it's a Wolfram rule)
   lifealgo *tempalgo;
   if (currlayer->hash) {
      tempalgo = new hlifealgo();
      tempalgo->setMaxMemory(maxhashmem);
   } else {
      tempalgo = new qlifealgo();
   }
   if (allowcheck) tempalgo->setpoll(wxGetApp().Poller());

   // copy cell list into temporary universe
   int num_cells = PyList_Size(given_list) / 2;
   for (int n = 0; n < num_cells; n++) {
      long x = PyInt_AsLong( PyList_GetItem(given_list, 2 * n) );
      long y = PyInt_AsLong( PyList_GetItem(given_list, 2 * n + 1) );

      tempalgo->setcell(x, y, 1);

      if ((n % 4096) == 0 && PythonScriptAborted()) {
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

#if defined(__WXMAC__) && wxCHECK_VERSION(2, 7, 0)
   // use decomposed UTF8 so fopen will work
   #define FILENAME wxString(filename,wxConvLocal).fn_str()
#else
   #define FILENAME filename
#endif

static PyObject *pyg_load(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *filename;

   if (!PyArg_ParseTuple(args, "s", &filename)) return NULL;

   // create temporary qlife universe
   lifealgo *tempalgo;
   tempalgo = new qlifealgo();
   if (allowcheck) tempalgo->setpoll(wxGetApp().Poller());

   // readpattern might change global rule table
   wxString oldrule = wxString(currlayer->algo->getrule(), wxConvLocal);

   // read pattern into temporary universe
   const char *err = readpattern(FILENAME, *tempalgo);
   if (err && strcmp(err,cannotreadhash) == 0) {
      // macrocell file, so switch to hlife universe
      delete tempalgo;
      tempalgo = new hlifealgo();
      tempalgo->setMaxMemory(maxhashmem);
      if (allowcheck) tempalgo->setpoll(wxGetApp().Poller());
      err = readpattern(FILENAME, *tempalgo);
   }

   // restore rule
   currlayer->algo->setrule( oldrule.mb_str(wxConvLocal) );

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

static PyObject *pyg_store(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   PyObject *given_list;
   char *filename;
   char *desc = NULL;      // the description string is currently ignored!!!

   if (!PyArg_ParseTuple(args, "O!s|s", &PyList_Type, &given_list, &filename, &desc))
      return NULL;

   // create temporary qlife universe
   lifealgo *tempalgo;
   tempalgo = new qlifealgo();
   if (allowcheck) tempalgo->setpoll(wxGetApp().Poller());

   // copy cell list into temporary universe
   int num_cells = PyList_Size(given_list) / 2;
   for (int n = 0; n < num_cells; n++) {
      long x = PyInt_AsLong( PyList_GetItem(given_list, 2 * n) );
      long y = PyInt_AsLong( PyList_GetItem(given_list, 2 * n + 1) );

      tempalgo->setcell(x, y, 1);

      if ((n % 4096) == 0 && PythonScriptAborted()) {
         tempalgo->endofpattern();
         delete tempalgo;
         return NULL;
      }
   }
   tempalgo->endofpattern();

   // write pattern to given file in RLE/XRLE format
   bigint top, left, bottom, right;
   tempalgo->findedges(&top, &left, &bottom, &right);
   const char *err = writepattern(FILENAME, *tempalgo,
                        savexrle ? XRLE_format : RLE_format,
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

static PyObject *pyg_putcells(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   long x0, y0, axx, axy, ayx, ayy;
   // defaults for affine transform params
   x0=0;
   y0=0;
   axx=1;
   axy=0;
   ayx=0;
   ayy=1;
   // default for mode is 'or'; 'xor' mode is also supported
   // 'copy' mode currently has the same effect as 'or' mode,
   // because there is no bounding box to retrieve OFF cells from.
   char *mode="or";
   PyObject *list;

   if (!PyArg_ParseTuple(args, "O!|lllllls", &PyList_Type, &list, &x0, &y0, &axx, &axy, &ayx, &ayy, &mode))
      return NULL;

   int num_cells = PyList_Size(list) / 2;
   lifealgo *curralgo = currlayer->algo;

   wxString modestr = wxString(mode, wxConvLocal);
   if ( !(modestr.IsSameAs(wxT("or"), false)
          || modestr.IsSameAs(wxT("xor"), false)
          || modestr.IsSameAs(wxT("copy"), false)
          || modestr.IsSameAs(wxT("not"), false)) ) {
      PyErr_SetString(PyExc_RuntimeError, "Bad putcells call: unknown mode.");
      return NULL;
   }
   if (modestr.IsSameAs(wxT("copy"), false)) {
   // TODO: find bounds of cell list and call ClearRect here (to be added to wxedit.cpp)
   }

   if (modestr.IsSameAs(wxT("xor"), false)) {
      // loop code is duplicated here to allow 'or' case to execute faster (?)
      for (int n = 0; n < num_cells; n++) {
         long x = PyInt_AsLong( PyList_GetItem(list, 2 * n) );
         long y = PyInt_AsLong( PyList_GetItem(list, 2 * n + 1) );
         int s = curralgo->getcell(x0 + x * axx + y * axy, y0 + x * ayx + y * ayy);

         // paste (possibly transformed) cell into current universe
         curralgo->setcell(x0 + x * axx + y * axy, y0 + x * ayx + y * ayy, 1-s);

         if ((n % 4096) == 0 && PythonScriptAborted()) {
            curralgo->endofpattern();
            currlayer->savestart = true;
            MarkLayerDirty();
            return NULL;
         }
      }
   } else {
      int cellstate = (modestr.IsSameAs(wxT("not"), false)) ? 0 : 1 ;
      for (int n = 0; n < num_cells; n++) {
         long x = PyInt_AsLong( PyList_GetItem(list, 2 * n) );
         long y = PyInt_AsLong( PyList_GetItem(list, 2 * n + 1) );

         // paste (possibly transformed) cell into current universe
         curralgo->setcell(x0 + x * axx + y * axy, y0 + x * ayx + y * ayy, cellstate);

         if ((n % 4096) == 0 && PythonScriptAborted()) {
            curralgo->endofpattern();
            currlayer->savestart = true;
            MarkLayerDirty();
            return NULL;
         }
      }
   }

   curralgo->endofpattern();
   currlayer->savestart = true;
   MarkLayerDirty();
   DoAutoUpdate();

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_getcells(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
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
      lifealgo *curralgo = currlayer->algo;
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
            if ((cntr % 4096) == 0 && PythonScriptAborted()) {
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

static PyObject *pyg_getclip(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
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
   tempalgo = new qlifealgo();   // qlife's setcell/getcell are faster
   if (allowcheck) tempalgo->setpoll(wxGetApp().Poller());

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
            if ((cntr % 4096) == 0 && PythonScriptAborted()) {
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

static PyObject *pyg_visrect(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
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

   return Py_BuildValue("i", visible);
}

// -----------------------------------------------------------------------------

static PyObject *pyg_select(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
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
      currlayer->selleft = x;
      currlayer->seltop = y;
      currlayer->selright = x + wd - 1;
      currlayer->selbottom = y + ht - 1;
   } else {
      PyErr_SetString(PyExc_RuntimeError, "Bad select call: arg must be [] or [x,y,wd,ht].");
      return NULL;
   }

   DoAutoUpdate();

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_getrect(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   PyObject *outlist = PyList_New(0);

   if (!currlayer->algo->isEmpty()) {
      bigint top, left, bottom, right;
      currlayer->algo->findedges(&top, &left, &bottom, &right);
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

static PyObject *pyg_getselrect(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   PyObject *outlist = PyList_New(0);

   if (viewptr->SelectionExists()) {
      if ( viewptr->OutsideLimits(currlayer->seltop, currlayer->selleft,
                                  currlayer->selbottom, currlayer->selright) ) {
         PyErr_SetString(PyExc_RuntimeError, "Bad getselrect call: selection is too big.");
         Py_DECREF(outlist);
         return NULL;
      }
      long x = currlayer->selleft.toint();
      long y = currlayer->seltop.toint();
      long wd = currlayer->selright.toint() - x + 1;
      long ht = currlayer->selbottom.toint() - y + 1;

      AddCell(outlist, x, y);
      AddCell(outlist, wd, ht);
   }

   return outlist;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_setcell(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   int x, y, state;

   if (!PyArg_ParseTuple(args, "iii", &x, &y, &state)) return NULL;

   currlayer->algo->setcell(x, y, state);
   currlayer->algo->endofpattern();
   currlayer->savestart = true;
   MarkLayerDirty();
   DoAutoUpdate();

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_getcell(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   int x, y;

   if (!PyArg_ParseTuple(args, "ii", &x, &y)) return NULL;

   return Py_BuildValue("i", currlayer->algo->getcell(x, y));
}

// -----------------------------------------------------------------------------

static PyObject *pyg_setcursor(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   int newindex;

   if (!PyArg_ParseTuple(args, "i", &newindex)) return NULL;

   int oldindex = CursorToIndex(currlayer->curs);
   wxCursor* curs = IndexToCursor(newindex);
   if (curs) {
      viewptr->SetCursorMode(curs);
      // see the cursor change, including in tool bar
      mainptr->UpdateUserInterface(mainptr->IsActive());
   } else {
      PyErr_SetString(PyExc_RuntimeError, "Bad setcursor call: unexpected cursor index.");
      return NULL;
   }

   // return old index (simplifies saving and restoring cursor)
   return Py_BuildValue("i", oldindex);
}

// -----------------------------------------------------------------------------

static PyObject *pyg_getcursor(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   return Py_BuildValue("i", CursorToIndex(currlayer->curs));
}

// -----------------------------------------------------------------------------

static PyObject *pyg_update(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   // update viewport, status bar and possibly title bar
   inscript = false;
   mainptr->UpdatePatternAndStatus();
   if (showtitle) {
      mainptr->SetWindowTitle(wxEmptyString);
      showtitle = false;
   }
   inscript = true;

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_autoupdate(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   int flag;

   if (!PyArg_ParseTuple(args, "i", &flag)) return NULL;

   autoupdate = (flag != 0);

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_getkey(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   char s[2];
   if (scriptchars.Length() == 0) {
      // return empty string
      s[0] = '\0';
   } else {
      // return first char in scriptchars and then remove it
      s[0] = scriptchars.GetChar(0);
      s[1] = '\0';
      scriptchars = scriptchars.AfterFirst(s[0]);
   }

   return Py_BuildValue("s", s);
}

// -----------------------------------------------------------------------------

// also allow mouse interaction??? ie. g.doclick( g.getclick() ) where
// getclick returns [] or [x,y,button,shift,ctrl,alt]

static PyObject *pyg_dokey(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   char* ascii = 0;

   if (!PyArg_ParseTuple(args, "s", &ascii)) return NULL;

   if (*ascii) {
      // convert ascii char to corresponding wx key code;
      // note that PassKeyToScript does the reverse conversion
      int key;
      switch (*ascii) {
         case 8:  key = WXK_BACK;   break;
         case 9:  key = WXK_TAB;    break;
         case 10: // play safe
         case 13: key = WXK_RETURN; break;
         case 28: key = WXK_LEFT;   break;
         case 29: key = WXK_RIGHT;  break;
         case 30: key = WXK_UP;     break;
         case 31: key = WXK_DOWN;   break;
         default: key = *ascii;
      }

      viewptr->ProcessKey(key, false);

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

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_appdir(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   return Py_BuildValue("s", (const char*)gollydir.mb_str(wxConvLocal));
}

// -----------------------------------------------------------------------------

static PyObject *pyg_show(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *s = NULL;

   if (!PyArg_ParseTuple(args, "s", &s)) return NULL;

   inscript = false;
   statusptr->DisplayMessage(wxString(s,wxConvLocal));
   inscript = true;
   // make sure status bar is visible
   if (!showstatus) mainptr->ToggleStatusBar();

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_error(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *s = NULL;

   if (!PyArg_ParseTuple(args, "s", &s)) return NULL;

   inscript = false;
   statusptr->ErrorMessage(wxString(s,wxConvLocal));
   inscript = true;
   // make sure status bar is visible
   if (!showstatus) mainptr->ToggleStatusBar();

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_warn(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *s = NULL;

   if (!PyArg_ParseTuple(args, "s", &s)) return NULL;

   Warning(wxString(s,wxConvLocal));

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_note(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *s = NULL;

   if (!PyArg_ParseTuple(args, "s", &s)) return NULL;

   Note(wxString(s,wxConvLocal));

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_check(PyObject *self, PyObject *args)
{
   // don't call checkevents() here otherwise we can't safely write code like
   //    if g.getlayer() == target:
   //       g.check(0)
   //       ... do stuff to target layer ...
   //       g.check(1)
   // if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   int flag;

   if (!PyArg_ParseTuple(args, "i", &flag)) return NULL;

   allowcheck = (flag != 0);

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_exit(PyObject *self, PyObject *args)
{
   if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *errmsg = NULL;

   if (!PyArg_ParseTuple(args, "|s", &errmsg)) return NULL;

   if (errmsg && errmsg[0] != 0) {
      // display given error message
      inscript = false;
      statusptr->ErrorMessage(wxString(errmsg,wxConvLocal));
      inscript = true;
      // make sure status bar is visible
      if (!showstatus) mainptr->ToggleStatusBar();
   }

   exitcalled = true;   // prevent CheckScriptError changing message
   AbortPythonScript();

   // exception raised so must return NULL
   return NULL;
}

// -----------------------------------------------------------------------------

static PyObject *pyg_stderr(PyObject *self, PyObject *args)
{
   // probably safer not to call checkevents here
   // if (PythonScriptAborted()) return NULL;
   wxUnusedVar(self);
   char *s = NULL;

   if (!PyArg_ParseTuple(args, "s", &s)) return NULL;

   // accumulate stderr messages in global string (shown after script finishes)
   scripterr = wxString(s,wxConvLocal);

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyMethodDef pyg_methods[] = {
   // filing
   { "open",         pyg_open,       METH_VARARGS, "open given pattern file" },
   { "save",         pyg_save,       METH_VARARGS, "save pattern in given file using given format" },
   { "load",         pyg_load,       METH_VARARGS, "read pattern file and return cell list" },
   { "store",        pyg_store,      METH_VARARGS, "write cell list to a file (in RLE format)" },
   { "appdir",       pyg_appdir,     METH_VARARGS, "return location of Golly app" },
   // editing
   { "new",          pyg_new,        METH_VARARGS, "create new universe and set window title" },
   { "cut",          pyg_cut,        METH_VARARGS, "cut selection to clipboard" },
   { "copy",         pyg_copy,       METH_VARARGS, "copy selection to clipboard" },
   { "clear",        pyg_clear,      METH_VARARGS, "clear inside/outside selection" },
   { "paste",        pyg_paste,      METH_VARARGS, "paste clipboard pattern at x,y using given mode" },
   { "shrink",       pyg_shrink,     METH_VARARGS, "shrink selection" },
   { "randfill",     pyg_randfill,   METH_VARARGS, "randomly fill selection to given percentage" },
   { "flip",         pyg_flip,       METH_VARARGS, "flip selection left-right or up-down" },
   { "rotate",       pyg_rotate,     METH_VARARGS, "rotate selection 90 deg clockwise or anticlockwise" },
   { "parse",        pyg_parse,      METH_VARARGS, "parse RLE or Life 1.05 string and return cell list" },
   { "transform",    pyg_transform,  METH_VARARGS, "apply an affine transformation to cell list" },
   { "evolve",       pyg_evolve,     METH_VARARGS, "generate pattern contained in given cell list" },
   { "putcells",     pyg_putcells,   METH_VARARGS, "paste given cell list into current universe" },
   { "getcells",     pyg_getcells,   METH_VARARGS, "return cell list in given rectangle" },
   { "getclip",      pyg_getclip,    METH_VARARGS, "return pattern in clipboard (as cell list)" },
   { "select",       pyg_select,     METH_VARARGS, "select [x, y, wd, ht] rectangle or remove if []" },
   { "getrect",      pyg_getrect,    METH_VARARGS, "return pattern rectangle as [] or [x, y, wd, ht]" },
   { "getselrect",   pyg_getselrect, METH_VARARGS, "return selection rectangle as [] or [x, y, wd, ht]" },
   { "setcell",      pyg_setcell,    METH_VARARGS, "set given cell to given state" },
   { "getcell",      pyg_getcell,    METH_VARARGS, "get state of given cell" },
   { "setcursor",    pyg_setcursor,  METH_VARARGS, "set cursor (returns old cursor)" },
   { "getcursor",    pyg_getcursor,  METH_VARARGS, "return current cursor" },
   // control
   { "empty",        pyg_empty,      METH_VARARGS, "return true if universe is empty" },
   { "run",          pyg_run,        METH_VARARGS, "run current pattern for given number of gens" },
   { "step",         pyg_step,       METH_VARARGS, "run current pattern for current step" },
   { "setstep",      pyg_setstep,    METH_VARARGS, "set step exponent" },
   { "getstep",      pyg_getstep,    METH_VARARGS, "return current step exponent" },
   { "setbase",      pyg_setbase,    METH_VARARGS, "set base step" },
   { "getbase",      pyg_getbase,    METH_VARARGS, "return current base step" },
   { "advance",      pyg_advance,    METH_VARARGS, "advance inside/outside selection by given gens" },
   { "reset",        pyg_reset,      METH_VARARGS, "restore starting pattern" },
   { "getgen",       pyg_getgen,     METH_VARARGS, "return current generation as string" },
   { "getpop",       pyg_getpop,     METH_VARARGS, "return current population as string" },
   { "setrule",      pyg_setrule,    METH_VARARGS, "set current rule according to string" },
   { "getrule",      pyg_getrule,    METH_VARARGS, "return current rule string" },
   // viewing
   { "setpos",       pyg_setpos,     METH_VARARGS, "move given cell to middle of viewport" },
   { "getpos",       pyg_getpos,     METH_VARARGS, "return x,y position of cell in middle of viewport" },
   { "setmag",       pyg_setmag,     METH_VARARGS, "set magnification (0=1:1, 1=1:2, -1=2:1, etc)" },
   { "getmag",       pyg_getmag,     METH_VARARGS, "return current magnification" },
   { "fit",          pyg_fit,        METH_VARARGS, "fit entire pattern in viewport" },
   { "fitsel",       pyg_fitsel,     METH_VARARGS, "fit selection in viewport" },
   { "visrect",      pyg_visrect,    METH_VARARGS, "return true if given rect is completely visible" },
   { "update",       pyg_update,     METH_VARARGS, "update display (viewport and status bar)" },
   { "autoupdate",   pyg_autoupdate, METH_VARARGS, "update display after each change to universe?" },
   // layers
   { "addlayer",     pyg_addlayer,   METH_VARARGS, "add a new layer" },
   { "clone",        pyg_clone,      METH_VARARGS, "add a cloned layer (shares universe)" },
   { "duplicate",    pyg_duplicate,  METH_VARARGS, "add a duplicate layer (copies universe)" },
   { "dellayer",     pyg_dellayer,   METH_VARARGS, "delete current layer" },
   { "movelayer",    pyg_movelayer,  METH_VARARGS, "move given layer to new index" },
   { "setlayer",     pyg_setlayer,   METH_VARARGS, "switch to given layer" },
   { "getlayer",     pyg_getlayer,   METH_VARARGS, "return index of current layer" },
   { "numlayers",    pyg_numlayers,  METH_VARARGS, "return current number of layers" },
   { "maxlayers",    pyg_maxlayers,  METH_VARARGS, "return maximum number of layers" },
   { "setname",      pyg_setname,    METH_VARARGS, "set name of given layer" },
   { "getname",      pyg_getname,    METH_VARARGS, "get name of given layer" },
   // miscellaneous
   { "setoption",    pyg_setoption,  METH_VARARGS, "set given option to new value (returns old value)" },
   { "getoption",    pyg_getoption,  METH_VARARGS, "return current value of given option" },
   { "setcolor",     pyg_setcolor,   METH_VARARGS, "set given color to new r,g,b (returns old r,g,b)" },
   { "getcolor",     pyg_getcolor,   METH_VARARGS, "return r,g,b values of given color" },
   { "getkey",       pyg_getkey,     METH_VARARGS, "return key hit by user or empty string if none" },
   { "dokey",        pyg_dokey,      METH_VARARGS, "pass given key to Golly's standard key handler" },
   { "show",         pyg_show,       METH_VARARGS, "show given string in status bar" },
   { "error",        pyg_error,      METH_VARARGS, "beep and show given string in status bar" },
   { "warn",         pyg_warn,       METH_VARARGS, "show given string in warning dialog" },
   { "note",         pyg_note,       METH_VARARGS, "show given string in note dialog" },
   { "check",        pyg_check,      METH_VARARGS, "allow event checking?" },
   { "exit",         pyg_exit,       METH_VARARGS, "exit script with optional error message" },
   // for internal use (don't document)
   { "stderr",       pyg_stderr,     METH_VARARGS, "save Python error message" },
   { NULL, NULL, 0, NULL }
};

// =============================================================================

bool InitPython()
{
   if (!pyinited) {
      #ifdef USE_PYTHON_DYNAMIC
         // try to load Python library
         if ( !LoadPythonLib() ) return false;
      #endif

      // only initialize the Python interpreter once, mainly because multiple
      // Py_Initialize/Py_Finalize calls cause leaks of about 12K each time!
      Py_Initialize();

      #ifdef USE_PYTHON_DYNAMIC
         GetPythonExceptions();
      #endif

      // allow Python to call the above pyg_* routines
      Py_InitModule("golly", pyg_methods);

      // catch Python messages sent to stderr and pass them to pyg_stderr
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

            // also create dummy sys.argv so scripts can import Tkinter
            "sys.argv = ['golly-app']\n"
            // works, but Golly's menus get permanently changed on Mac!!!
            ) < 0
         ) Warning(_("StderrCatcher code failed!"));

      // build absolute path to Golly's Scripts folder and add to Python's
      // import search list so scripts can import glife from anywhere
      wxString scriptsdir = gollydir + _("Scripts");
      scriptsdir.Replace(wxT("\\"), wxT("\\\\"));
      wxString command = wxT("import sys ; sys.path.append('") + scriptsdir + wxT("')");
      if ( PyRun_SimpleString(command.mb_str(wxConvLocal)) < 0 )
         Warning(_("Failed to append Scripts path!"));

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
         ) Warning(_("RollbackImporter code failed!"));
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
         ) Warning(_("PyRun_SimpleString failed!"));
   }

   return true;
}

// -----------------------------------------------------------------------------

void RunPythonScript(const wxString &filepath)
{
   if (!InitPython()) return;

   // if file name contains backslashes then we must convert them to "\\"
   // to avoid "\a" being treated as escape char
   wxString fpath = filepath;
   fpath.Replace(wxT("\\"), wxT("\\\\"));

   // execute the given script
   wxString command = wxT("execfile('") + fpath + wxT("')");
   PyRun_SimpleString(command.mb_str(wxConvLocal));

   // note that PyRun_SimpleString returns -1 if an exception occurred;
   // the error message (in scripterr) is checked at the end of RunScript
}

// -----------------------------------------------------------------------------

void CheckScriptError()
{
   if (scripterr.IsEmpty()) {
      return;
   } else if (scripterr.Find(wxString(abortmsg,wxConvLocal)) >= 0) {
      // error was caused by AbortPerlScript/AbortPythonScript
      // so don't display scripterr
   } else {
      scripterr.Replace(wxT("  File \"<string>\", line 1, in ?\n"), wxT(""));
      wxBell();
      #ifdef __WXMAC__
         wxSetCursor(*wxSTANDARD_CURSOR);
      #endif
      wxMessageBox(scripterr, _("Script error:"),
                     wxOK | wxICON_EXCLAMATION, wxGetActiveWindow());
   }
   // don't change message if *g_exit was used to stop script
   if (!exitcalled) statusptr->DisplayMessage(_("Script aborted."));
}

// =============================================================================

// embed a Perl interpreter (see "perldoc perlembed" for details)

// avoid warning about _ being redefined
#undef _

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

// restore wxWidgets definition for _ (from include/wx/intl.h)
#undef _
#define _(s) wxGetTranslation(_T(s))

static PerlInterpreter* my_perl;

// -----------------------------------------------------------------------------

void AbortPerlScript()
{
   scripterr = wxString(abortmsg,wxConvLocal);
   // can't call Perl_croak here (done via RETURN_IF_ABORTED)
}

// -----------------------------------------------------------------------------

bool PerlScriptAborted()
{
   if (allowcheck) wxGetApp().Poller()->checkevents();

   // if user hit escape key then PassKeyToScript has called AbortPerlScript

   return !scripterr.IsEmpty();
}

// -----------------------------------------------------------------------------

#define RETURN_IF_ABORTED if (PerlScriptAborted()) Perl_croak(aTHX_ NULL)

#ifdef __WXMSW__
   #define IGNORE_UNUSED_PARAMS wxUnusedVar(cv); wxUnusedVar(my_perl);
#else
   #define IGNORE_UNUSED_PARAMS
#endif

// -----------------------------------------------------------------------------

XS(plg_getselrect)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) {
      Perl_croak(aTHX_ "Usage: @rect = g_getselrect()");
   }
   
   // items == 0 so no need to reset stack pointer
   // SP -= items;
   
   if (viewptr->SelectionExists()) {
      if ( viewptr->OutsideLimits(currlayer->seltop, currlayer->selleft,
                                  currlayer->selbottom, currlayer->selright) ) {
         Perl_croak(aTHX_ "g_getselrect error: selection is too big.");
      }
      int x = currlayer->selleft.toint();
      int y = currlayer->seltop.toint();
      int wd = currlayer->selright.toint() - x + 1;
      int ht = currlayer->selbottom.toint() - y + 1;

      XPUSHs(sv_2mortal(newSViv(x)));
      XPUSHs(sv_2mortal(newSViv(y)));
      XPUSHs(sv_2mortal(newSViv(wd)));
      XPUSHs(sv_2mortal(newSViv(ht)));
      XSRETURN(4);
   } else {
      XSRETURN(0);
   }
}

// -----------------------------------------------------------------------------

XS(plg_setcell)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 3) {
      Perl_croak(aTHX_ "Usage: g_setcell($x,$y,$state)");
   }

   currlayer->algo->setcell(SvIV(ST(0)), SvIV(ST(1)), SvIV(ST(2)));
   currlayer->algo->endofpattern();
   currlayer->savestart = true;
   MarkLayerDirty();
   DoAutoUpdate();
   
   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(plg_getcell)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 2) {
      Perl_croak(aTHX_ "Usage: $state = g_getcell($x,$y)");
   }

   int state = currlayer->algo->getcell(SvIV(ST(0)), SvIV(ST(1)));
   
   XSRETURN_IV(state);
   /* above is equivalent to:
   SP -= items;
   XPUSHs(sv_2mortal(newSViv(state)));
   PUTBACK;
   */
}

// -----------------------------------------------------------------------------

XS(plg_fitsel)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) {
      Perl_croak(aTHX_ "Usage: g_fitsel()");
   }

   if (viewptr->SelectionExists()) {
      viewptr->FitSelection();
      DoAutoUpdate();
   } else {
      Perl_croak(aTHX_ "g_fitsel error: no selection.");
   }

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(plg_visrect)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 4) {
      Perl_croak(aTHX_ "Usage: $bool = g_visrect(@rect)");
   }

   int x = SvIV(ST(0));
   int y = SvIV(ST(1));
   int wd = SvIV(ST(2));
   int ht = SvIV(ST(3));
   // check that wd & ht are > 0
   if (wd <= 0) {
      Perl_croak(aTHX_ "g_visrect error: width must be > 0.");
   }
   if (ht <= 0) {
      Perl_croak(aTHX_ "g_visrect error: height must be > 0.");
   }

   bigint left = x;
   bigint top = y;
   bigint right = x + wd - 1;
   bigint bottom = y + ht - 1;
   int visible = viewptr->CellVisible(left, top) &&
                 viewptr->CellVisible(right, bottom);

   XSRETURN_IV(visible);
}

// -----------------------------------------------------------------------------

XS(plg_update)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) {
      Perl_croak(aTHX_ "Usage: g_update()");
   }

   // update viewport, status bar and possibly title bar
   inscript = false;
   mainptr->UpdatePatternAndStatus();
   if (showtitle) {
      mainptr->SetWindowTitle(wxEmptyString);
      showtitle = false;
   }
   inscript = true;
   
   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(plg_getkey)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) {
      Perl_croak(aTHX_ "Usage: $char = g_getkey()");
   }

   char s[2];
   if (scriptchars.Length() == 0) {
      // return empty string
      s[0] = '\0';
   } else {
      // return first char in scriptchars and then remove it
      s[0] = scriptchars.GetChar(0);
      s[1] = '\0';
      scriptchars = scriptchars.AfterFirst(s[0]);
   }

   XSRETURN_PV(s);
}

// -----------------------------------------------------------------------------

XS(plg_dokey)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) {
      Perl_croak(aTHX_ "Usage: g_dokey($char)");
   }
   STRLEN n_a;
   char* ascii = SvPV(ST(0), n_a);

   if (*ascii) {
      // convert ascii char to corresponding wx key code;
      // note that PassKeyToScript does the reverse conversion
      int key;
      switch (*ascii) {
         case 8:  key = WXK_BACK;   break;
         case 9:  key = WXK_TAB;    break;
         case 10: // play safe
         case 13: key = WXK_RETURN; break;
         case 28: key = WXK_LEFT;   break;
         case 29: key = WXK_RIGHT;  break;
         case 30: key = WXK_UP;     break;
         case 31: key = WXK_DOWN;   break;
         default: key = *ascii;
      }

      viewptr->ProcessKey(key, false);

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

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(plg_show)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) {
      Perl_croak(aTHX_ "Usage: g_show($string)");
   }
   STRLEN n_a;
   char* s = SvPV(ST(0), n_a);

   inscript = false;
   statusptr->DisplayMessage(wxString(s,wxConvLocal));
   inscript = true;
   // make sure status bar is visible
   if (!showstatus) mainptr->ToggleStatusBar();
   
   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(plg_note)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) {
      Perl_croak(aTHX_ "Usage: g_note($string)");
   }
   STRLEN n_a;
   char* s = SvPV(ST(0), n_a);

   Note(wxString(s,wxConvLocal));
   
   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(plg_exit)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items > 1) {
      Perl_croak(aTHX_ "Usage: g_exit($string='')");
   }

   if (items == 1) {
      // display given error message
      STRLEN n_a;
      char* errmsg = SvPV(ST(0), n_a);
      inscript = false;
      statusptr->ErrorMessage(wxString(errmsg,wxConvLocal));
      inscript = true;
      // make sure status bar is visible
      if (!showstatus) mainptr->ToggleStatusBar();
   }

   exitcalled = true;   // prevent CheckScriptError changing message
   AbortPerlScript();
   
   XSRETURN(0);
}

// -----------------------------------------------------------------------------

/* can't get this approach to work!!!
XS(boot_golly)
{
   IGNORE_UNUSED_PARAMS;
   dXSARGS;
   if (items != 1) {
      Warning(_("Possible problem in boot_golly!"));
   }
   
   // declare routines in golly module
   newXS("golly::g_setcell",      plg_setcell,      "");
   newXS("golly::g_getcell",      plg_getcell,      "");
   // etc...
   
   XSRETURN_YES;
}
*/

// -----------------------------------------------------------------------------

EXTERN_C void boot_DynaLoader(pTHX_ CV* cv);

// xs_init is passed into perl_parse and initializes the
// statically linked extension modules

EXTERN_C void xs_init(pTHX)
{
   #ifdef __WXMSW__
      wxUnusedVar(my_perl);
   #endif
   char* file = __FILE__;
   dXSUB_SYS;

   // DynaLoader allows dynamic loading of other Perl extensions
   newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);

   /* can't get this approach to work!!!
      "use golly" causes error: Can't locate golly.pm in @INC
      newXS("golly::boot_golly", boot_golly, file);
   */

   // filing
   //!!!newXS("g_open",         plg_open,       file);
   //!!!newXS("g_save",         plg_save,       file);
   //!!!newXS("g_load",         plg_load,       file);
   //!!!newXS("g_store",        plg_store,      file);
   //!!!newXS("g_appdir",       plg_appdir,     file);
   // editing
   //!!!newXS("g_new",          plg_new,        file);
   //!!!newXS("g_cut",          plg_cut,        file);
   //!!!newXS("g_copy",         plg_copy,       file);
   //!!!newXS("g_clear",        plg_clear,      file);
   //!!!newXS("g_paste",        plg_paste,      file);
   //!!!newXS("g_shrink",       plg_shrink,     file);
   //!!!newXS("g_randfill",     plg_randfill,   file);
   //!!!newXS("g_flip",         plg_flip,       file);
   //!!!newXS("g_rotate",       plg_rotate,     file);
   //!!!newXS("g_parse",        plg_parse,      file);
   //!!!newXS("g_transform",    plg_transform,  file);
   //!!!newXS("g_evolve",       plg_evolve,     file);
   //!!!newXS("g_putcells",     plg_putcells,   file);
   //!!!newXS("g_getcells",     plg_getcells,   file);
   //!!!newXS("g_getclip",      plg_getclip,    file);
   //!!!newXS("g_select",       plg_select,     file);
   //!!!newXS("g_getrect",      plg_getrect,    file);
   newXS("g_getselrect",   plg_getselrect, file);
   newXS("g_setcell",      plg_setcell,    file);
   newXS("g_getcell",      plg_getcell,    file);
   //!!!newXS("g_setcursor",    plg_setcursor,  file);
   //!!!newXS("g_getcursor",    plg_getcursor,  file);
   // control
   //!!!newXS("g_empty",        plg_empty,      file);
   //!!!newXS("g_run",          plg_run,        file);
   //!!!newXS("g_step",         plg_step,       file);
   //!!!newXS("g_setstep",      plg_setstep,    file);
   //!!!newXS("g_getstep",      plg_getstep,    file);
   //!!!newXS("g_setbase",      plg_setbase,    file);
   //!!!newXS("g_getbase",      plg_getbase,    file);
   //!!!newXS("g_advance",      plg_advance,    file);
   //!!!newXS("g_reset",        plg_reset,      file);
   //!!!newXS("g_getgen",       plg_getgen,     file);
   //!!!newXS("g_getpop",       plg_getpop,     file);
   //!!!newXS("g_setrule",      plg_setrule,    file);
   //!!!newXS("g_getrule",      plg_getrule,    file);
   // viewing
   //!!!newXS("g_setpos",       plg_setpos,     file);
   //!!!newXS("g_getpos",       plg_getpos,     file);
   //!!!newXS("g_setmag",       plg_setmag,     file);
   //!!!newXS("g_getmag",       plg_getmag,     file);
   //!!!newXS("g_fit",          plg_fit,        file);
   newXS("g_fitsel",       plg_fitsel,     file);
   newXS("g_visrect",      plg_visrect,    file);
   newXS("g_update",       plg_update,     file);
   //!!!newXS("g_autoupdate",   plg_autoupdate, file);
   // layers
   //!!!newXS("g_addlayer",     plg_addlayer,   file);
   //!!!newXS("g_clone",        plg_clone,      file);
   //!!!newXS("g_duplicate",    plg_duplicate,  file);
   //!!!newXS("g_dellayer",     plg_dellayer,   file);
   //!!!newXS("g_movelayer",    plg_movelayer,  file);
   //!!!newXS("g_setlayer",     plg_setlayer,   file);
   //!!!newXS("g_getlayer",     plg_getlayer,   file);
   //!!!newXS("g_numlayers",    plg_numlayers,  file);
   //!!!newXS("g_maxlayers",    plg_maxlayers,  file);
   //!!!newXS("g_setname",      plg_setname,    file);
   //!!!newXS("g_getname",      plg_getname,    file);
   // miscellaneous
   //!!!newXS("g_setoption",    plg_setoption,  file);
   //!!!newXS("g_getoption",    plg_getoption,  file);
   //!!!newXS("g_setcolor",     plg_setcolor,   file);
   //!!!newXS("g_getcolor",     plg_getcolor,   file);
   newXS("g_getkey",       plg_getkey,     file);
   newXS("g_dokey",        plg_dokey,      file);
   newXS("g_show",         plg_show,       file);
   //!!!newXS("g_error",        plg_error,      file);
   //!!!newXS("g_warn",         plg_warn,       file);
   newXS("g_note",         plg_note,       file);
   //!!!newXS("g_check",        plg_check,      file);
   newXS("g_exit",         plg_exit,       file);
}

// -----------------------------------------------------------------------------

void RunPerlScript(const wxString &filepath)
{
   char* my_argv[2];
   my_argv[0] = "";
   my_argv[1] = (char*) filepath.mb_str(wxConvLocal);

   //!!!??? PERL_SYS_INIT3(NULL, NULL, NULL);
   
   my_perl = perl_alloc();
   if (!my_perl) {
      Warning(_("Could not create Perl interpreter!"));
      return;
   }
   perl_construct(my_perl);
   PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
   perl_parse(my_perl, xs_init, 2, my_argv, NULL);
   perl_run(my_perl);
   perl_destruct(my_perl);
   perl_free(my_perl);
   
   //!!!??? PERL_SYS_TERM();
}

// =============================================================================

// exported routines

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
         fpath = wxString(fpath.fn_str(), wxConvLocal);
      #else
         // wxMac 2.6.x or older
         fpath = wxString(fpath.wc_str(wxConvLocal), wxConvUTF8);
      #endif
   #endif

   inscript = true;
   mainptr->UpdateUserInterface(mainptr->IsActive());

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
   
   inscript = false;
   plscript = false;
   pyscript = false;

   // reset all stayclean flags (set by MarkLayerClean)
   for ( int i = 0; i < numlayers; i++ )
      GetLayer(i)->stayclean = false;

   // restore current directory to location of Golly app
   wxSetWorkingDirectory(gollydir);

   // display any Perl/Python error message
   CheckScriptError();

   // update title, menu bar, cursor, viewport, status bar, tool bar, etc
   if (showtitle) mainptr->SetWindowTitle(wxEmptyString);
   mainptr->UpdateEverything();
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
      // note that *g_dokey does the reverse conversion
      /*
      char msg[64];
      sprintf(msg, "key=%d ch=%c", key, (char)key);
      Warning(msg);
      */
      char ascii;
      switch (key) {
         case WXK_DELETE:
         case WXK_BACK:       ascii = 8;     break;
         case WXK_TAB:        ascii = 9;     break;
         case WXK_NUMPAD_ENTER: // treat enter key like return key
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
      // save ascii char for possible consumption by *g_getkey
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

   // Py_Finalize can cause an obvious delay, so best not to call it
   // if (pyinited) Py_Finalize();

   // probably don't really need this either
   #ifdef USE_PYTHON_DYNAMIC
      FreePythonLib();
   #endif
}
