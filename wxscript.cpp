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

// NOTE: Some of the code in this module is from the wxScript package
// by Francesco Montorsi.  Here is Francesco's copyright notice:

///////////////////////////////////////////////////////////////////////
// wxScript classes                                                  //
// Copyright (C) 2003 by Francesco Montorsi                          //
//                                                                   //
// This program is free software; you can redistribute it and/or     //
// modify it under the terms of the GNU General Public License       //
// as published by the Free Software Foundation; either version 2    //
// of the License, or (at your option) any later version.            //
//                                                                   //
// This program is distributed in the hope that it will be useful,   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the      //
// GNU General Public License for more details.                      //
//                                                                   //
// You should have received a copy of the GNU General Public         //
// License along with this program; if not, write to the Free        //
// Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,   //
// MA 02111-1307, USA.                                               //
//                                                                   //
// For any comment, suggestion or feature request, please contact    //
// frm@users.sourceforge.net                                         //
//                                                                   //
///////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
   #include "wx/wx.h"      // for all others include the necessary headers
#endif

#include "wx/filename.h"   // for wxFileName

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
#undef HAVE_WCHAR_H        // Python.h redefines this if we use Unicode mode
#endif
#include <Python.h>

// =============================================================================

// globals

bool inscript = false;     // script is running?
bool autoupdate;           // update display after each change to current universe?
wxString pyerror;          // Python error message
wxString gollyloc;         // location of Golly app
wxString scriptloc;        // location of script file

const char abortmsg[] = "GOLLY: ABORT SCRIPT";

// =============================================================================

// from Francesco's script.h:

// this will be defined later
class wxPython;

// A singleton class.
class wxScriptInterpreter
{
protected:

   // The shared instance of the wxScriptInterpreter.
   static wxPython *m_pPython;

public:      // ctor & dtor

   wxScriptInterpreter() {}
   virtual ~wxScriptInterpreter() {}

   //! The description of the last error.
   static wxString m_strLastErr;

public:      // static functions

   //! Initializes the script interpreter.
   static bool Init();

   //! Deallocates the script interpreter.
   static void Cleanup();

   //! Load the given scriptfile and then returns an instance to the
   //! scriptfile wrapper, or NULL if the file couldn't be loaded.
   static bool Load(const wxString &filename);
};

// =============================================================================

// from Francesco's scpython.h:

// The wxPython interpreter.
class wxPython : public wxScriptInterpreter
{
public:

   // The current python module.
   PyObject *m_pModule;

   // The current dictionaries being used.
   PyObject *m_pLocals, *m_pGlobals;

public:
   wxPython() {}
   virtual ~wxPython() { Cleanup(); }

   // Inits Python interpreter.
   virtual bool Init();

   // Undoes what Init() does.
   virtual void Cleanup();

   // Return true if the given script is loaded and executed.
   virtual bool Load(const wxString &filename);
};

// =============================================================================

// from Francesco's script.cpp:

// setup static
wxString wxScriptInterpreter::m_strLastErr;
wxPython *wxScriptInterpreter::m_pPython = NULL;

// ----------------------
// wxSCRIPTINTERPRETER
// ----------------------

bool wxScriptInterpreter::Init()
{
   // remove previous instance, if present
   Cleanup();

   m_pPython = new wxPython();
   return m_pPython->Init();
}

void wxScriptInterpreter::Cleanup()
{
   if (m_pPython) delete m_pPython;
   m_pPython = NULL;
}

bool wxScriptInterpreter::Load(const wxString &filename)
{
   if (!wxFileName::FileExists(filename)) {
      wxScriptInterpreter::m_strLastErr = wxT("The script file does not exist: ") + filename;
      return false;
   }

   return m_pPython->Load(filename);
}

// =============================================================================

// from Francesco's scpython.cpp:

// -----------------
// wxPYTHON
// -----------------

bool wxPython::Init()
{
   // inizialize the python interpreter
   Py_Initialize();

   // initialize our "execution frame"
   m_pModule = PyImport_AddModule("__main__");

   // PyImport_AddModule returns a borrowed reference: we'll now transform
   // it to a full reference...
   Py_INCREF(m_pModule);

   // we'll do the same for our dictionary...
   m_pGlobals = PyModule_GetDict(m_pModule);
   Py_INCREF(m_pGlobals);

   // our locals dict will be a reference to the global one...
   m_pLocals = m_pGlobals;
   Py_INCREF(m_pLocals);

   // everything was okay ?
   return Py_IsInitialized() != 0;
}

void wxPython::Cleanup()
{
   // free all our references
   Py_DECREF(m_pModule);
   Py_DECREF(m_pGlobals);
   Py_DECREF(m_pLocals);

   // and everything else which is python-related
   Py_Finalize();
}

bool wxPython::Load(const wxString &filename)
{
   // if filename contains backslashes then we must convert them to "\\"
   // to avoid "\a" being treated as escape char
   wxString fname = filename;
   fname.Replace("\\", "\\\\");

   // build absolute path to Golly's Scripts folder
   wxString scriptsdir = gollyloc + wxT("Scripts");
   scriptsdir.Replace("\\", "\\\\");

   // use PyRun_SimpleString to add Golly's Scripts folder to Python's
   // import search list (so scripts anywhere can do "from glife import *")
   // and then execute the given script
   wxString command = wxT("import sys ; sys.path.append(\"") + scriptsdir + wxT("\")");
   command += wxT(" ; execfile(\"") + fname + wxT("\")");
   PyRun_SimpleString(command.c_str());
   
   // note that PyRun_SimpleString returns -1 if an exception occurred,
   // but we always return true because the error message (in pyerror)
   // will be checked later at the end of RunScript

   return true;
}

// =============================================================================

// The following golly_* routines can be called from Python scripts; some are
// based on code in PLife's lifeint.cc (see http://plife.sourceforge.net/).

static bool ScriptAborted()
{
   wxGetApp().Poller()->checkevents();
   
   // if user hit escape key then AbortScript has raised an exception
   // and PyErr_Occurred will be true; if so, caller must return NULL
   // otherwise Python can abort app with this message:
   // Fatal Python error: unexpected exception during garbage collection

   return PyErr_Occurred() != NULL;
}

// -----------------------------------------------------------------------------

static void DoAutoUpdate()
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
   char *title = NULL;

   if (!PyArg_ParseTuple(args, "z", &title)) return NULL;

   mainptr->NewPattern();
   if (title != NULL && title[0] != 0) {
      mainptr->SetWindowTitle(title);
   } else {
      // NewPattern has set title to "untitled"
   }
   DoAutoUpdate();

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
      Warning("Bad fitsel call: no selection.");
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
      Warning("Bad clear call: no selection.");
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

   if (viewptr->SelectionExists()) {
      if (perc < 1) perc = 1;
      if (perc > 100) perc = 100;
      int oldperc = randomfill;
      randomfill = perc;
      viewptr->RandomFill();
      randomfill = oldperc;
      DoAutoUpdate();
   } else {
      Warning("Bad randfill call: no selection.");
      return NULL;
   }

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyObject *golly_view(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   int x, y;

   if (!PyArg_ParseTuple(args, "ii" , &x, &y)) return NULL;

   bigint bigx = x;
   bigint bigy = y;
   viewptr->SetPosMag(bigx, bigy, viewptr->GetMag());
   DoAutoUpdate();

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
      Warning(err);
      return NULL;
   } else if ( global_liferules.hasB0notS8 && hashing ) {
      curralgo->setrule( (char*)oldrule.c_str() );
      Warning("B0-not-S8 rules are not allowed when hashing.");
      return NULL;
   } else {
      // show new rule in main window's title (but don't change name)
      mainptr->SetWindowTitle("");
   }

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

// helper routine used in calls that build cell lists
static void AddCell(PyObject *list, long x, long y)
{
   PyList_Append(list, PyInt_FromLong(x));
   PyList_Append(list, PyInt_FromLong(y));
}   

// -----------------------------------------------------------------------------

// helper routine to extract cell list from given universe
static bool ExtractCells(PyObject *list, lifealgo *universe, bool shift = false)
{
   if ( !universe->isEmpty() ) {
      bigint top, left, bottom, right;
      universe->findedges(&top, &left, &bottom, &right);
      if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
         Warning("Universe is too big to extract all cells!");
         return false;
      }
      int itop = top.toint();
      int ileft = left.toint();
      int ibottom = bottom.toint();
      int iright = right.toint();
      int cx, cy;
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

   PyObject *list = PyList_New (0);

   long x = 0, y = 0;

   if (strchr(s, '*')) {
      // parsing 'visual' format
      int c = *s++;
      while (c) {
         switch (c) {
         case '\n': if (x) { x = 0; y++; } break;
         case '.': x++; break;
         case '*':
            AddCell(list, x0 + x * axx + y * axy, y0 + x * ayx + y * ayy);
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
                  AddCell(list, x0 + x * axx + y * axy, y0 + x * ayx + y * ayy);
               break;
            }
            prefix = 0;
         }
         c = *s++;
      }
   }

   return list;
}

// -----------------------------------------------------------------------------

static PyObject *golly_transform(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   long x0, y0, axx, axy, ayx, ayy;

   PyObject *list;
   PyObject *new_list;

   if (!PyArg_ParseTuple(args, "O!llllll", &PyList_Type, &list, &x0, &y0, &axx, &axy, &ayx, &ayy))
      return NULL;

   new_list = PyList_New (0);

   int num_cells = PyList_Size (list) / 2;
   for (int n = 0; n < num_cells; n++) {
      long x = PyInt_AsLong( PyList_GetItem (list, 2 * n) );
      long y = PyInt_AsLong( PyList_GetItem (list, 2 * n + 1) );

      AddCell(new_list, x0 + x * axx + y * axy, y0 + x * ayx + y * ayy);
   }

   return new_list;
}

// -----------------------------------------------------------------------------

static PyObject *golly_evolve(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   int N = 0;

   PyObject *given_list;
   PyObject *evolved_list;

   if (!PyArg_ParseTuple(args, "O!i", &PyList_Type, &given_list, &N)) return NULL;

   // create temporary qlife universe
   lifealgo *tempalgo;
   tempalgo = new qlifealgo();
   tempalgo->setpoll(wxGetApp().Poller());
   
   // no need for this -- all universes share global rule table
   // tempalgo->setrule( curralgo->getrule() );
   
   // copy cell list into temporary universe
   int num_cells = PyList_Size (given_list) / 2;
   for (int n = 0; n < num_cells; n++) {
      long x = PyInt_AsLong( PyList_GetItem(given_list, 2 * n) );
      long y = PyInt_AsLong( PyList_GetItem(given_list, 2 * n + 1) );
      tempalgo->setcell(x, y, 1);
   }
   tempalgo->endofpattern();

   // advance pattern by N gens
   mainptr->generating = true;
   tempalgo->setIncrement(N);
   tempalgo->step();
   mainptr->generating = false;

   // convert new pattern into a new cell list
   evolved_list = PyList_New(0);
   bool done = ExtractCells(evolved_list, tempalgo);
   delete tempalgo;
   if (!done) return NULL;

   return evolved_list;
}

// -----------------------------------------------------------------------------

static PyObject *golly_load(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   PyObject *list;
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
      Warning(err);
      return NULL;
   }
   
   // convert pattern into a cell list
   list = PyList_New(0);
   // shift cell coords so bbox's top left cell is at 0,0
   bool done = ExtractCells(list, tempalgo, true);
   delete tempalgo;
   if (!done) return NULL;

   return list;
}

// -----------------------------------------------------------------------------

static PyObject *golly_save(PyObject *self, PyObject *args)
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
   int num_cells = PyList_Size (given_list) / 2;
   for (int n = 0; n < num_cells; n++) {
      long x = PyInt_AsLong( PyList_GetItem(given_list, 2 * n) );
      long y = PyInt_AsLong( PyList_GetItem(given_list, 2 * n + 1) );
      tempalgo->setcell(x, y, 1);
   }
   tempalgo->endofpattern();

   // write pattern to given file in RLE format
   bigint top, left, bottom, right;
   tempalgo->findedges(&top, &left, &bottom, &right);
   const char *err = writepattern(file_name, *tempalgo, RLE_format,
                                  top.toint(), left.toint(), bottom.toint(), right.toint());
   delete tempalgo;
   if (err) {
      Warning(err);
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
   PyObject *cell_list = PyList_New(0);

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
         Warning("Bad getcells call: width must be > 0.");
         return NULL;
      }
      if (ht <= 0) {
         Warning("Bad getcells call: height must be > 0.");
         return NULL;
      }
      int iright = ileft + wd - 1;
      int ibottom = itop + ht - 1;
      int cx, cy;
      for ( cy=itop; cy<=ibottom; cy++ ) {
         for ( cx=ileft; cx<=iright; cx++ ) {
            int skip = curralgo->nextcell(cx, cy);
            if (skip >= 0) {
               // found next live cell in this row
               cx += skip;
               if (cx <= iright) AddCell(cell_list, cx, cy);
            } else {
               cx = iright;  // done this row
            }
         }
      }
   } else {
      Warning("Bad getcells call: arg must be [] or [x,y,wd,ht].");
      return NULL;
   }

   return cell_list;
}

// -----------------------------------------------------------------------------

static PyObject *golly_getclip(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;
   
   // convert pattern in clipboard into a cell list, but where the first 2 items
   // are the pattern's width and height (not necessarily the minimal bounding box
   // because the pattern might have empty borders, or it might even be empty)
   PyObject *clip_list = PyList_New(0);

   if (!mainptr->ClipboardHasText()) {
      Warning("Error in getclip: no pattern in clipboard.");
      return NULL;
   }

   // create a temporary universe for storing clipboard pattern
   lifealgo *tempalgo;
   tempalgo = new qlifealgo();               // qlife's setcell/getcell are faster
   tempalgo->setpoll(wxGetApp().Poller());

   // read clipboard pattern into temporary universe and set edges
   // (not a minimal bounding box if pattern is empty or has empty borders)
   bigint top, left, bottom, right;
   if ( viewptr->GetClipboardPattern(tempalgo, &top, &left, &bottom, &right) ) {
      if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
         Warning("Error in getclip: pattern is too big.");
         return NULL;
      }
      int itop = top.toint();
      int ileft = left.toint();
      int ibottom = bottom.toint();
      int iright = right.toint();
      int wd = iright - ileft + 1;
      int ht = ibottom - itop + 1;

      AddCell(clip_list, wd, ht);

      // extract cells from tempalgo
      int cx, cy;
      for ( cy=itop; cy<=ibottom; cy++ ) {
         for ( cx=ileft; cx<=iright; cx++ ) {
            int skip = tempalgo->nextcell(cx, cy);
            if (skip >= 0) {
               // found next live cell in this row
               cx += skip;
               // shift cells so that top left cell of bounding box is at 0,0
               AddCell(clip_list, cx - ileft, cy - itop);
            } else {
               cx = iright;  // done this row
            }
         }
      }

      delete tempalgo;
   } else {
      // assume error message has been displayed
      delete tempalgo;
      return NULL;
   }

   return clip_list;
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
      Warning("Bad visrect call: arg must be [x,y,wd,ht].");
      return NULL;
   }

   int x = PyInt_AsLong( PyList_GetItem(rect_list, 0) );
   int y = PyInt_AsLong( PyList_GetItem(rect_list, 1) );
   int wd = PyInt_AsLong( PyList_GetItem(rect_list, 2) );
   int ht = PyInt_AsLong( PyList_GetItem(rect_list, 3) );
   // check that wd & ht are > 0
   if (wd <= 0) {
      Warning("Bad visrect call: width must be > 0.");
      return NULL;
   }
   if (ht <= 0) {
      Warning("Bad visrect call: height must be > 0.");
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

   if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &rect_list)) return NULL;

   int numitems = PyList_Size(rect_list);
   if (numitems == 0) {
      // remove any existing selection
      viewptr->NoSelection();
   } else if (numitems == 4) {
      int x = PyInt_AsLong( PyList_GetItem(rect_list, 0) );
      int y = PyInt_AsLong( PyList_GetItem(rect_list, 1) );
      int wd = PyInt_AsLong( PyList_GetItem(rect_list, 2) );
      int ht = PyInt_AsLong( PyList_GetItem(rect_list, 3) );
      // first check that wd & ht are > 0
      if (wd <= 0) {
         Warning("Bad select call: width must be > 0.");
         return NULL;
      }
      if (ht <= 0) {
         Warning("Bad select call: height must be > 0.");
         return NULL;
      }
      // set selection edges
      viewptr->selleft = x;
      viewptr->seltop = y;
      viewptr->selright = x + wd - 1;
      viewptr->selbottom = y + ht - 1;
   } else {
      Warning("Bad select call: arg must be [] or [x,y,wd,ht].");
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

   PyObject *rect_list = PyList_New(0);

   if (!curralgo->isEmpty()) {
      bigint top, left, bottom, right;
      curralgo->findedges(&top, &left, &bottom, &right);
      if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
         Warning("Error in getrect: pattern is too big.");
         return NULL;
      }
      long x = left.toint();
      long y = top.toint();
      long wd = right.toint() - x + 1;
      long ht = bottom.toint() - y + 1;
        
      PyList_Append(rect_list, PyInt_FromLong(x));
      PyList_Append(rect_list, PyInt_FromLong(y));
      PyList_Append(rect_list, PyInt_FromLong(wd));
      PyList_Append(rect_list, PyInt_FromLong(ht));
   }
   
   return rect_list;
}

// -----------------------------------------------------------------------------

static PyObject *golly_getselrect(PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   PyObject *rect_list = PyList_New(0);

   if (viewptr->SelectionExists()) {
      if ( viewptr->OutsideLimits(viewptr->seltop, viewptr->selleft,
                                  viewptr->selbottom, viewptr->selright) ) {
         Warning("Error in getselrect: selection is too big.");
         return NULL;
      }
      long x = viewptr->selleft.toint();
      long y = viewptr->seltop.toint();
      long wd = viewptr->selright.toint() - x + 1;
      long ht = viewptr->selbottom.toint() - y + 1;
        
      PyList_Append(rect_list, PyInt_FromLong(x));
      PyList_Append(rect_list, PyInt_FromLong(y));
      PyList_Append(rect_list, PyInt_FromLong(wd));
      PyList_Append(rect_list, PyInt_FromLong(ht));
   }
   
   return rect_list;
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

static PyObject *golly_getcell (PyObject *self, PyObject *args)
{
   if (ScriptAborted()) return NULL;
   wxUnusedVar(self);
   int x, y;

   if (!PyArg_ParseTuple(args, "ii", &x, &y)) return NULL;

   PyObject *result = Py_BuildValue("i", curralgo->getcell(x, y));
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

   // save Python's stderr messages in global string for display after script finishes;
   // relies on StderrCatcher code in Scripts/glife/__init__.py
   pyerror = wxT(s);

   Py_INCREF(Py_None);
   return Py_None;
}

// -----------------------------------------------------------------------------

static PyMethodDef golly_methods[] = {
   { "new",          golly_new,        METH_VARARGS, "create new universe and optionally set title" },
   { "fit",          golly_fit,        METH_VARARGS, "fit entire pattern in viewport" },
   { "fitsel",       golly_fitsel,     METH_VARARGS, "fit selection in viewport" },
   { "clear",        golly_clear,      METH_VARARGS, "clear inside/outside selection" },
   { "randfill",     golly_randfill,   METH_VARARGS, "randomly fill selection to given percentage" },
   { "view",         golly_view,       METH_VARARGS, "display given cell in middle of viewport" },
   { "setrule",      golly_setrule,    METH_VARARGS, "set current rule according to string" },
   { "parse",        golly_parse,      METH_VARARGS, "parse RLE or Life 1.05 string and return cell list" },
   { "transform",    golly_transform,  METH_VARARGS, "apply an affine transformation to cell list" },
   { "evolve",       golly_evolve,     METH_VARARGS, "generate pattern contained in given cell list" },
   { "load",         golly_load,       METH_VARARGS, "load pattern from file and return cell list" },
   { "save",         golly_save,       METH_VARARGS, "save cell list to a file (in RLE format)" },
   { "putcells",     golly_putcells,   METH_VARARGS, "paste given cell list into current universe" },
   { "getcells",     golly_getcells,   METH_VARARGS, "return cell list in given rectangle" },
   { "getclip",      golly_getclip,    METH_VARARGS, "return pattern in clipboard (as cell list)" },
   { "visrect",      golly_visrect,    METH_VARARGS, "return true if given rect is completely visible" },
   { "select",       golly_select,     METH_VARARGS, "select [x, y, wd, ht] rectangle or remove if []" },
   { "getrect",      golly_getrect,    METH_VARARGS, "return pattern rectangle as [] or [x, y, wd, ht]" },
   { "getselrect",   golly_getselrect, METH_VARARGS, "return selection rectangle as [] or [x, y, wd, ht]" },
   { "setcell",      golly_setcell,    METH_VARARGS, "set given cell to given state" },
   { "getcell",      golly_getcell,    METH_VARARGS, "get state of given cell" },
   { "update",       golly_update,     METH_VARARGS, "update display (viewport and status bar)" },
   { "autoupdate",   golly_autoupdate, METH_VARARGS, "update display after each change to universe?" },
   { "appdir",       golly_appdir,     METH_VARARGS, "return location of Golly app" },
   { "show",         golly_show,       METH_VARARGS, "show given string in status bar" },
   { "error",        golly_error,      METH_VARARGS, "beep and show given string in status bar" },
   { "warn",         golly_warn,       METH_VARARGS, "show given string in warning dialog" },
   { "stderr",       golly_stderr,     METH_VARARGS, "save Python error message" },
   { NULL, NULL, 0, NULL }
};

// =============================================================================

// Exported routines:

void RunScript(const char* filename)
{
   if ( inscript ) return;    // play safe and avoid re-entrancy

   wxString fname = wxT(filename);
   mainptr->showbanner = false;
   statusptr->ClearMessage();
   pyerror = wxEmptyString;
   autoupdate = false;
   
   if (!wxScriptInterpreter::Init()) {
      Warning("Could not initialize the Python interpreter!  Is it installed?");
      wxScriptInterpreter::Cleanup();
      return;
   }

   // allow Python to call the above golly_* routines
   Py_InitModule3("golly", golly_methods, "Internal golly routines");

   // temporarily change current directory to location of script
   gollyloc = wxFileName::GetCwd();
   if (gollyloc.Last() != wxFILE_SEP_PATH) gollyloc += wxFILE_SEP_PATH;
   wxFileName fullname(fname);
   fullname.Normalize();
   scriptloc = fullname.GetPath();
   if (!scriptloc.IsEmpty()) {
      if (scriptloc.Last() != wxFILE_SEP_PATH) scriptloc += wxFILE_SEP_PATH;
      wxSetWorkingDirectory(scriptloc);
   }
   
   // let user know we're busy running a script
   wxSetCursor(*wxHOURGLASS_CURSOR);
   viewptr->SetCursor(*wxHOURGLASS_CURSOR);
   mainptr->UpdateToolBar(false);
   mainptr->EnableAllMenus(false);
   
   inscript = true;
   wxGetApp().PollerReset();

   // execute the script
   if (!wxScriptInterpreter::Load(fname)) {
      // assume m_strLastErr has been set
      Warning(wxScriptInterpreter::m_strLastErr.c_str());
   }
   
   // restore current directory to location of Golly app
   if (!scriptloc.IsEmpty()) wxSetWorkingDirectory(gollyloc);

   wxScriptInterpreter::Cleanup();
   
   inscript = false;
   
   // update menu bar, cursor, viewport, status bar, tool bar, etc
   mainptr->EnableAllMenus(true);
   mainptr->UpdateEverything();
   
   // display any Python error message
   if (!pyerror.IsEmpty()) {
      if (pyerror.Find(abortmsg) >= 0) {
         // error was caused by AbortScript so don't display pyerror
         statusptr->DisplayMessage("Script aborted.");
      } else {
         wxBell();
         wxSetCursor(*wxSTANDARD_CURSOR);
         wxMessageBox(pyerror, wxT("Python error:"), wxOK | wxICON_EXCLAMATION, wxGetActiveWindow());
      }
   }
}

// -----------------------------------------------------------------------------

bool IsScript(const char *filename)
{
   // currently we only support Python scripts, so return true only if filename
   // ends with ".py" (ignoring case)
   wxString fname = wxT(filename);
   wxString ext = fname.AfterLast(wxT('.'));

   // false means case-insensitive comparison
   return ext.IsSameAs(wxT("PY"), false);
}

// -----------------------------------------------------------------------------

bool InScript()
{
   return inscript;
}

// -----------------------------------------------------------------------------

// this will be called from checkevents() if user hits escape key
void AbortScript()
{
   if (inscript) {
      // raise an exception with a special message
      PyErr_SetString(PyExc_KeyboardInterrupt, abortmsg);
   }
}
