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

/*
   Golly uses an embedded Perl interpreter to execute scripts.
   Perl is Copyright (C) 1993-2007, by Larry Wall and others.
   It is free software; you can redistribute it and/or modify it under the terms of either:
   a) the GNU General Public License as published by the Free Software Foundation;
   either version 1, or (at your option) any later version, or
   b) the "Artistic License" (http://dev.perl.org/licenses/artistic.html).
*/

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
   #include "wx/wx.h"      // for all others include the necessary headers
#endif

#include <limits.h>        // for INT_MAX

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "readpattern.h"
#include "writepattern.h"

#include "wxgolly.h"       // for wxGetApp, mainptr, viewptr, statusptr
#include "wxmain.h"        // for mainptr->...
#include "wxedit.h"        // for Selection
#include "wxview.h"        // for viewptr->...
#include "wxstatus.h"      // for statusptr->...
#include "wxutils.h"       // for Warning, Note, GetString, etc
#include "wxprefs.h"       // for perllib, gollydir, etc
#include "wxinfo.h"        // for ShowInfo
#include "wxhelp.h"        // for ShowHelp
#include "wxundo.h"        // for currlayer->undoredo->...
#include "wxalgos.h"       // for *_ALGO, CreateNewUniverse, algobase
#include "wxlayer.h"       // for AddLayer, currlayer, currindex, etc
#include "wxscript.h"      // for inscript, abortmsg, GSF_*, etc
#include "wxperl.h"

// =============================================================================

// Perl scripting support is implemented by embedding a Perl interpreter.
// See "perldoc perlembed" for details.

#ifndef __WXMAC__
   #include "wx/dynlib.h"     // for wxDynamicLibrary
#endif

// avoid warning about _ being redefined
#undef _

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

/*
 * Quoting Jan Dubois of Active State:
 *    ActivePerl build 822 still identifies itself as 5.8.8 but already
 *    contains many of the changes from the upcoming Perl 5.8.9 release.
 *
 * The changes include addition of two symbols (Perl_sv_2iv_flags,
 * Perl_newXS_flags) not present in earlier releases.
 *
 * Jan Dubois suggested the following guarding scheme:
 */
#if (ACTIVEPERL_VERSION >= 822)
# define PERL589_OR_LATER
#endif
#if (PERL_REVISION == 5) && (PERL_VERSION == 8) && (PERL_SUBVERSION >= 9)
# define PERL589_OR_LATER
#endif
#if (PERL_REVISION == 5) && (PERL_VERSION >= 9)
# define PERL589_OR_LATER
#endif

// restore wxWidgets definition for _ (from include/wx/intl.h)
#undef _
#define _(s) wxGetTranslation(_T(s))

static PerlInterpreter* my_perl;

EXTERN_C void boot_DynaLoader(pTHX_ CV* cv);

// =============================================================================

// On Windows and Linux we try to load the Perl library at runtime
// so Golly will start up even if Perl isn't installed.

//!!! On Linux we can't load libperl.so dynamically because DynaLoader.a
//!!! has to be statically linked (for boot_DynaLoader) but it uses calls
//!!! in libperl.so -- sheesh.
//!!! #ifndef __WXMAC__
#ifdef __WXMSW__
   // load Perl lib at runtime
   #define USE_PERL_DYNAMIC
#endif

#ifdef USE_PERL_DYNAMIC

// declare G_* wrappers for the functions we want to use from Perl lib
extern "C"
{
   perl_key*(*G_Perl_Gthr_key_ptr)(register PerlInterpreter*);
   U8*(*G_Perl_Iexit_flags_ptr)(register PerlInterpreter*);
   I32**(*G_Perl_Tmarkstack_ptr_ptr)(register PerlInterpreter*);
   SV***(*G_Perl_Tstack_base_ptr)(register PerlInterpreter*);
   SV***(*G_Perl_Tstack_max_ptr)(register PerlInterpreter*);
   SV***(*G_Perl_Tstack_sp_ptr)(register PerlInterpreter*);
   SV**(*G_Perl_av_fetch)(pTHX_ AV*, I32, I32);
   I32(*G_Perl_av_len)(pTHX_ AV*);
   void(*G_Perl_av_push)(pTHX_ AV*, SV*);
   void(*G_Perl_croak)(pTHX_ const char*, ...);
   void*(*G_Perl_get_context)(void);
   AV*(*G_Perl_newAV)(pTHX);
   SV*(*G_Perl_newRV)(pTHX_ SV*);
   SV*(*G_Perl_newSViv)(pTHX_ IV);
   SV*(*G_Perl_newSVpv)(pTHX_ const char*, STRLEN);
   CV*(*G_Perl_newXS)(pTHX_ char*, XSUBADDR_t, char*);
   SV**(*G_Perl_stack_grow)(pTHX_ SV**, SV**, int);
   IV(*G_Perl_sv_2iv)(pTHX_ SV*);
   SV*(*G_Perl_sv_2mortal)(pTHX_ SV*);
   char*(*G_Perl_sv_2pv_flags)(pTHX_ SV*, STRLEN*, I32);
   PerlInterpreter*(*G_perl_alloc)(void);
   void(*G_perl_construct)(PerlInterpreter*);
   int(*G_perl_destruct)(PerlInterpreter*);
   void(*G_perl_free)(PerlInterpreter*);
   int(*G_perl_parse)(PerlInterpreter*, XSINIT_t, int, char**, char**);
   int(*G_perl_run)(PerlInterpreter*);
   SV*(*G_Perl_eval_pv)(pTHX_ const char*, I32);
#ifdef PERL589_OR_LATER
   IV (*G_Perl_sv_2iv_flags)(pTHX_ SV* sv, I32 flags);
#endif
#ifdef __WXMSW__
   void(*G_boot_DynaLoader)(pTHX_ CV*);
#endif
}

// redefine Perl functions to their equivalent G_* wrappers
#define Perl_Gthr_key_ptr        G_Perl_Gthr_key_ptr
#define Perl_Iexit_flags_ptr     G_Perl_Iexit_flags_ptr
#define Perl_Tmarkstack_ptr_ptr  G_Perl_Tmarkstack_ptr_ptr
#define Perl_Tstack_base_ptr     G_Perl_Tstack_base_ptr
#define Perl_Tstack_max_ptr      G_Perl_Tstack_max_ptr
#define Perl_Tstack_sp_ptr       G_Perl_Tstack_sp_ptr
#define Perl_av_fetch            G_Perl_av_fetch
#define Perl_av_len              G_Perl_av_len
#define Perl_av_push             G_Perl_av_push
#define Perl_croak               G_Perl_croak
#define Perl_get_context         G_Perl_get_context
#define Perl_newAV               G_Perl_newAV
#define Perl_newRV               G_Perl_newRV
#define Perl_newSViv             G_Perl_newSViv
#define Perl_newSVpv             G_Perl_newSVpv
#define Perl_newXS               G_Perl_newXS
#define Perl_stack_grow          G_Perl_stack_grow
#define Perl_sv_2iv              G_Perl_sv_2iv
#define Perl_sv_2mortal          G_Perl_sv_2mortal
#define Perl_sv_2pv_flags        G_Perl_sv_2pv_flags
#define perl_alloc               G_perl_alloc
#define perl_construct           G_perl_construct
#define perl_destruct            G_perl_destruct
#define perl_free                G_perl_free
#define perl_parse               G_perl_parse
#define perl_run                 G_perl_run
#define Perl_eval_pv             G_Perl_eval_pv
#ifdef PERL589_OR_LATER
#define Perl_sv_2iv_flags        G_Perl_sv_2iv_flags
#endif
#ifdef __WXMSW__
#define boot_DynaLoader          G_boot_DynaLoader
#endif

#ifdef __WXMSW__
   #define PERL_PROC FARPROC
#else
   #define PERL_PROC void *
#endif
#define PERL_FUNC(func) { _T(#func), (PERL_PROC*)&G_ ## func },

// store function names and their addresses in Perl lib
static struct PerlFunc
{
   const wxChar* name;     // function name
   PERL_PROC* ptr;         // function pointer
} perlFuncs[] =
{
   PERL_FUNC(Perl_Gthr_key_ptr)
   PERL_FUNC(Perl_Iexit_flags_ptr)
   PERL_FUNC(Perl_Tmarkstack_ptr_ptr)
   PERL_FUNC(Perl_Tstack_base_ptr)
   PERL_FUNC(Perl_Tstack_max_ptr)
   PERL_FUNC(Perl_Tstack_sp_ptr)
   PERL_FUNC(Perl_av_fetch)
   PERL_FUNC(Perl_av_len)
   PERL_FUNC(Perl_av_push)
   PERL_FUNC(Perl_croak)
   PERL_FUNC(Perl_get_context)
   PERL_FUNC(Perl_newAV)
   PERL_FUNC(Perl_newRV)
   PERL_FUNC(Perl_newSViv)
   PERL_FUNC(Perl_newSVpv)
   PERL_FUNC(Perl_newXS)
   PERL_FUNC(Perl_stack_grow)
   PERL_FUNC(Perl_sv_2iv)
   PERL_FUNC(Perl_sv_2mortal)
   PERL_FUNC(Perl_sv_2pv_flags)
   PERL_FUNC(perl_alloc)
   PERL_FUNC(perl_construct)
   PERL_FUNC(perl_destruct)
   PERL_FUNC(perl_free)
   PERL_FUNC(perl_parse)
   PERL_FUNC(perl_run)
   PERL_FUNC(Perl_eval_pv)
#ifdef PERL589_OR_LATER
   PERL_FUNC(Perl_sv_2iv_flags)
#endif
#ifdef __WXMSW__
   PERL_FUNC(boot_DynaLoader)
#endif
   { _T(""), NULL }
};

// handle for Perl library
static wxDllType perldll = NULL;

static void FreePerlLib()
{
   if ( perldll ) {
      wxDynamicLibrary::Unload(perldll);
      perldll = NULL;
   }
}

static bool LoadPerlLib()
{
   // load the Perl library
   wxDynamicLibrary dynlib;

   // don't log errors in here
   wxLogNull noLog;

   // wxDL_GLOBAL corresponds to RTLD_GLOBAL on Linux (ignored on Windows) and
   // is needed to avoid an ImportError when importing some modules (eg. time)
   while ( !dynlib.Load(perllib, wxDL_NOW | wxDL_VERBATIM | wxDL_GLOBAL) ) {
      // prompt user for a different Perl library;
      // on Windows perllib should be something like "perl5.dll"
      // and on Linux it should be something like "libperl.so"
      wxBell();
      wxString str = _("If Perl isn't installed then you'll have to Cancel,");
      str +=         _("\notherwise change the version numbers and try again.");
      #ifdef __WXMSW__
         str +=      _("\nDepending on where you installed Perl you might have");
         str +=      _("\nto enter a full path like C:\\Perl\\bin\\perl58.dll.");
      #endif
      wxTextEntryDialog dialog( wxGetActiveWindow(), str,
                                _("Could not load the Perl library"),
                                perllib, wxOK | wxCANCEL );
      if (dialog.ShowModal() == wxID_OK) {
         perllib = dialog.GetValue();
      } else {
         return false;
      }
   }

   if ( dynlib.IsLoaded() ) {
      // load all functions named in perlFuncs
      void *funcptr;
      PerlFunc *pf = perlFuncs;
      while ( pf->ptr ) {
         funcptr = dynlib.GetSymbol(pf->name);
         if ( !funcptr ) {
            wxString err = _("Perl library does not have this symbol:\n");
            err += pf->name;
            Warning(err);
            FreePerlLib();
            break;
         }

         *(pf++->ptr) = (PERL_PROC)funcptr;
      }

      if ( !pf->ptr ) {
         perldll = dynlib.Detach();
      }
   }

   if ( perldll == NULL ) {
      // should never happen
      Warning(_("Oh dear, the Perl library is not loaded!"));
   }

   return perldll != NULL;
}

#endif // USE_PERL_DYNAMIC

// =============================================================================

// some useful macros

#define RETURN_IF_ABORTED if (PerlScriptAborted()) Perl_croak(aTHX_ NULL)

#define PERL_ERROR(msg) { Perl_croak(aTHX_ msg); }

#ifdef __WXMSW__
   #define IGNORE_UNUSED_PARAMS wxUnusedVar(cv); wxUnusedVar(my_perl);
#else
   #define IGNORE_UNUSED_PARAMS
#endif

#if defined(__WXMAC__) && wxCHECK_VERSION(2, 7, 0)
   // use decomposed UTF8 so fopen will work
   #define FILENAME wxString(filename,wxConvLocal).fn_str()
#else
   #define FILENAME filename
#endif

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

// helper routine to extract cell array from given universe
const char* ExtractCellArray(AV* outarray, lifealgo* universe, bool shift = false)
{
   if ( !universe->isEmpty() ) {
      bigint top, left, bottom, right;
      universe->findedges(&top, &left, &bottom, &right);
      if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
         return "Universe is too big to extract all cells!";
      }
      int itop = top.toint();
      int ileft = left.toint();
      int ibottom = bottom.toint();
      int iright = right.toint();
      int cx, cy;
      int v = 0 ;
      /** FIXME:  support multistate */
      int cntr = 0;
      for ( cy=itop; cy<=ibottom; cy++ ) {
         for ( cx=ileft; cx<=iright; cx++ ) {
            int skip = universe->nextcell(cx, cy, v);
            if (skip >= 0) {
               // found next live cell in this row
               cx += skip;
               if (shift) {
                  // shift cells so that top left cell of bounding box is at 0,0
                  av_push(outarray, newSViv(cx - ileft));
                  av_push(outarray, newSViv(cy - itop));
               } else {
                  av_push(outarray, newSViv(cx));
                  av_push(outarray, newSViv(cy));
               }
            } else {
               cx = iright;  // done this row
            }
            cntr++;
            if ((cntr % 4096) == 0 && PerlScriptAborted()) return NULL;
         }
      }
   }
   return NULL;
}

// =============================================================================

// The following pl_* routines can be called from Perl scripts.

XS(pl_open)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items < 1 || items > 2) PERL_ERROR("Usage: g_open($filename,$remember=0)");

   STRLEN n_a;
   char* filename = SvPV(ST(0), n_a);
   int remember = 0;
   if (items > 1) remember = SvIV(ST(1));
   
   const char* errmsg = GSF_open(filename, remember);
   if (errmsg) PERL_ERROR(errmsg);

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_save)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items < 2 || items > 3) PERL_ERROR("Usage: g_save($filename,$format,$remember=0)");

   STRLEN n_a;
   char* filename = SvPV(ST(0), n_a);
   char* format = SvPV(ST(1), n_a);
   int remember = 0;
   if (items > 2) remember = SvIV(ST(2));
   
   const char* errmsg = GSF_save(filename, format, remember);
   if (errmsg) PERL_ERROR(errmsg);

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_load)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) PERL_ERROR("Usage: $cells = g_load($filename)");

   STRLEN n_a;
   char* filename = SvPV(ST(0), n_a);

   // create temporary qlife universe
   lifealgo* tempalgo = CreateNewUniverse(QLIFE_ALGO, allowcheck);

   // readpattern might change global rule table
   wxString oldrule = wxString(currlayer->algo->getrule(), wxConvLocal);

   // read pattern into temporary universe
   const char* err = readpattern(FILENAME, *tempalgo);
   //!!! forget cannotreadhash test -- try all other algos until readclipboard succeeds
   if (err && strcmp(err,cannotreadhash) == 0) {
      // macrocell file, so switch to hlife universe
      delete tempalgo;
      tempalgo = CreateNewUniverse(HLIFE_ALGO, allowcheck);
      err = readpattern(FILENAME, *tempalgo);
   }

   // restore rule
   currlayer->algo->setrule( oldrule.mb_str(wxConvLocal) );

   if (err) {
      delete tempalgo;
      PERL_ERROR(err);
   }

   // convert pattern into a cell list, shifting cell coords so that the
   // bounding box's top left cell is at 0,0
   AV* outarray = (AV*)sv_2mortal( (SV*)newAV() );
   err = ExtractCellArray(outarray, tempalgo, true);
   delete tempalgo;
   if (err) {
      // assume Perl interpreter will free all memory when it quits???
      // int key = av_len(inarray);
      // while (key >= 0) { av_delete(outarray, key, G_DISCARD); key--; }
      // av_undef(outarray);
      PERL_ERROR(err);
   }

   SP -= items;
   ST(0) = newRV( (SV*)outarray );
   sv_2mortal(ST(0));
   XSRETURN(1);
}

// -----------------------------------------------------------------------------

XS(pl_store)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 2) PERL_ERROR("Usage: g_store($cells,$filename)");

   SV* cells = ST(0);
   if ( (!SvROK(cells)) || (SvTYPE(SvRV(cells)) != SVt_PVAV) ) {
       PERL_ERROR("g_store error: 1st parameter is not a valid array reference");
   }
   AV* inarray = (AV*)SvRV(cells);
   int num_cells = (av_len(inarray) + 1) / 2;
   // note that av_len returns max index or -1 if array is empty
   
   STRLEN n_a;
   char* filename = SvPV(ST(1), n_a);

   // create temporary qlife universe
   lifealgo* tempalgo = CreateNewUniverse(QLIFE_ALGO, allowcheck);

   // copy cell list into temporary universe
   for (int n = 0; n < num_cells; n++) {
      int x = SvIV( *av_fetch(inarray, 2 * n, 0) );
      int y = SvIV( *av_fetch(inarray, 2 * n + 1, 0) );

      tempalgo->setcell(x, y, 1);

      if ((n % 4096) == 0 && PerlScriptAborted()) {
         tempalgo->endofpattern();
         delete tempalgo;
         Perl_croak(aTHX_ NULL);
      }
   }
   tempalgo->endofpattern();

   // write pattern to given file in RLE/XRLE format
   bigint top, left, bottom, right;
   tempalgo->findedges(&top, &left, &bottom, &right);
   const char* err = writepattern(FILENAME, *tempalgo,
                        savexrle ? XRLE_format : RLE_format,
                        top.toint(), left.toint(), bottom.toint(), right.toint());
   delete tempalgo;
   if (err) {
      PERL_ERROR(err);
   }

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_appdir)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: $dir = g_appdir()");

   XSRETURN_PV((const char*) gollydir.mb_str(wxConvLocal));
}

// -----------------------------------------------------------------------------

XS(pl_datadir)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: $dir = g_datadir()");

   XSRETURN_PV((const char*) datadir.mb_str(wxConvLocal));
}

// -----------------------------------------------------------------------------

XS(pl_new)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) PERL_ERROR("Usage: g_new($title)");

   STRLEN n_a;
   char* title = SvPV(ST(0), n_a);

   mainptr->NewPattern(wxString(title,wxConvLocal));
   DoAutoUpdate();

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_cut)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: g_cut()");

   if (viewptr->SelectionExists()) {
      viewptr->CutSelection();
      DoAutoUpdate();
   } else {
      PERL_ERROR("g_cut error: no selection");
   }

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_copy)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: g_copy()");

   if (viewptr->SelectionExists()) {
      viewptr->CopySelection();
      DoAutoUpdate();
   } else {
      PERL_ERROR("g_copy error: no selection");
   }

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_clear)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) PERL_ERROR("Usage: g_clear($where)");

   int where = SvIV(ST(0));

   if (viewptr->SelectionExists()) {
      if (where == 0)
         viewptr->ClearSelection();
      else
         viewptr->ClearOutsideSelection();
      DoAutoUpdate();
   } else {
      PERL_ERROR("g_clear error: no selection");
   }

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_paste)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 3) PERL_ERROR("Usage: g_paste($x,$y,$mode)");

   int x = SvIV(ST(0));
   int y = SvIV(ST(1));

   STRLEN n_a;
   char* mode = SvPV(ST(2), n_a);

   if (!mainptr->ClipboardHasText()) {
      PERL_ERROR("g_paste error: no pattern in clipboard");
   }

   // temporarily change selection and paste mode
   Selection oldsel = currlayer->currsel;
   const char* oldmode = GetPasteMode();
   
   wxString modestr = wxString(mode, wxConvLocal);
   if      (modestr.IsSameAs(wxT("copy"), false)) SetPasteMode("Copy");
   else if (modestr.IsSameAs(wxT("or"), false))   SetPasteMode("Or");
   else if (modestr.IsSameAs(wxT("xor"), false))  SetPasteMode("Xor");
   else {
      PERL_ERROR("g_paste error: unknown mode");
   }

   // create huge selection rect so no possibility of error message
   currlayer->currsel.SetRect(x, y, INT_MAX, INT_MAX);

   viewptr->PasteClipboard(true);      // true = paste to selection

   // restore selection and paste mode
   currlayer->currsel = oldsel;
   SetPasteMode(oldmode);

   DoAutoUpdate();

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_shrink)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: g_shrink()");

   if (viewptr->SelectionExists()) {
      viewptr->ShrinkSelection(false);    // false == don't fit in viewport
      DoAutoUpdate();
   } else {
      PERL_ERROR("g_shrink error: no selection");
   }

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_randfill)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) PERL_ERROR("Usage: g_randfill($percentage)");

   int perc = SvIV(ST(0));

   if (perc < 1 || perc > 100) {
      PERL_ERROR("g_randfill error: percentage must be from 1 to 100");
   }

   if (viewptr->SelectionExists()) {
      int oldperc = randomfill;
      randomfill = perc;
      viewptr->RandomFill();
      randomfill = oldperc;
      DoAutoUpdate();
   } else {
      PERL_ERROR("g_randfill error: no selection");
   }

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_flip)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) PERL_ERROR("Usage: g_flip($direction)");

   int direction = SvIV(ST(0));

   if (viewptr->SelectionExists()) {
      viewptr->FlipSelection(direction != 0);    // 1 = top-bottom
      DoAutoUpdate();
   } else {
      PERL_ERROR("g_flip error: no selection");
   }

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_rotate)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) PERL_ERROR("Usage: g_rotate($direction)");

   int direction = SvIV(ST(0));

   if (viewptr->SelectionExists()) {
      viewptr->RotateSelection(direction == 0);    // 0 = clockwise
      DoAutoUpdate();
   } else {
      PERL_ERROR("g_rotate error: no selection");
   }

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_parse)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items < 1 || items > 7)
      PERL_ERROR("Usage: $outcells = g_parse($string,$x=0,$y=0,$axx=1,$axy=0,$ayx=0,$ayy=1)");

   STRLEN n_a;
   char* s = SvPV(ST(0), n_a);

   // default values for optional params
   int x0  = 0;
   int y0  = 0;
   int axx = 1;
   int axy = 0;
   int ayx = 0;
   int ayy = 1;
   if (items > 1) x0  = SvIV(ST(1));
   if (items > 2) y0  = SvIV(ST(2));
   if (items > 3) axx = SvIV(ST(3));
   if (items > 4) axy = SvIV(ST(4));
   if (items > 5) ayx = SvIV(ST(5));
   if (items > 6) ayy = SvIV(ST(6));

   AV* outarray = (AV*)sv_2mortal( (SV*)newAV() );

   int x = 0, y = 0;

   if (strchr(s, '*')) {
      // parsing 'visual' format
      int c = *s++;
      while (c) {
         switch (c) {
         case '\n': if (x) { x = 0; y++; } break;
         case '.': x++; break;
         case '*':
            av_push(outarray, newSViv(x0 + x * axx + y * axy));
            av_push(outarray, newSViv(y0 + x * ayx + y * ayy));
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
               for (int k = 0; k < prefix; k++, x++) {
                  av_push(outarray, newSViv(x0 + x * axx + y * axy));
                  av_push(outarray, newSViv(y0 + x * ayx + y * ayy));
               }
               break;
            }
            prefix = 0;
         }
         c = *s++;
      }
   }

   SP -= items;
   ST(0) = newRV( (SV*)outarray );
   sv_2mortal(ST(0));
   XSRETURN(1);
}

// -----------------------------------------------------------------------------

XS(pl_transform)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items < 3 || items > 7)
      PERL_ERROR("Usage: $outcells = g_transform($cells,$x,$y,$axx=1,$axy=0,$ayx=0,$ayy=1)");

   SV* cells = ST(0);
   if ( (!SvROK(cells)) || (SvTYPE(SvRV(cells)) != SVt_PVAV) ) {
       PERL_ERROR("g_transform error: 1st parameter is not a valid array reference");
   }
   AV* inarray = (AV*)SvRV(cells);
   int num_cells = (av_len(inarray) + 1) / 2;
   // note that av_len returns max index or -1 if array is empty

   int x0  = SvIV(ST(1));
   int y0  = SvIV(ST(2));

   // default values for optional params
   int axx = 1;
   int axy = 0;
   int ayx = 0;
   int ayy = 1;
   if (items > 3) axx = SvIV(ST(3));
   if (items > 4) axy = SvIV(ST(4));
   if (items > 5) ayx = SvIV(ST(5));
   if (items > 6) ayy = SvIV(ST(6));

   AV* outarray = (AV*)sv_2mortal( (SV*)newAV() );

   for (int n = 0; n < num_cells; n++) {
      int x = SvIV( *av_fetch(inarray, 2 * n, 0) );
      int y = SvIV( *av_fetch(inarray, 2 * n + 1, 0) );

      av_push(outarray, newSViv(x0 + x * axx + y * axy));
      av_push(outarray, newSViv(y0 + x * ayx + y * ayy));

      if ((n % 4096) == 0 && PerlScriptAborted()) break;
   }

   SP -= items;
   ST(0) = newRV( (SV*)outarray );
   sv_2mortal(ST(0));
   XSRETURN(1);
}

// -----------------------------------------------------------------------------

XS(pl_evolve)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 2) PERL_ERROR("Usage: $outcells = g_evolve($cells,$numgens)");

   SV* cells = ST(0);
   if ( (!SvROK(cells)) || (SvTYPE(SvRV(cells)) != SVt_PVAV) ) {
       PERL_ERROR("g_evolve error: 1st parameter is not a valid array reference");
   }
   AV* inarray = (AV*)SvRV(cells);
   int num_cells = (av_len(inarray) + 1) / 2;
   // note that av_len returns max index or -1 if array is empty

   int ngens = SvIV(ST(1));

   // create a temporary universe of same type as current universe so we
   // don't have to update the global rule table (in case it's a Wolfram rule)
   lifealgo* tempalgo = CreateNewUniverse(currlayer->algtype, allowcheck);

   // copy cell array into temporary universe
   for (int n = 0; n < num_cells; n++) {
      int x = SvIV( *av_fetch(inarray, 2 * n, 0) );
      int y = SvIV( *av_fetch(inarray, 2 * n + 1, 0) );

      tempalgo->setcell(x, y, 1);

      if ((n % 4096) == 0 && PerlScriptAborted()) {
         tempalgo->endofpattern();
         delete tempalgo;
         Perl_croak(aTHX_ NULL);
      }
   }
   tempalgo->endofpattern();

   // advance pattern by ngens
   mainptr->generating = true;
   tempalgo->setIncrement(ngens);
   tempalgo->step();
   mainptr->generating = false;

   // convert new pattern into a new cell array
   AV* outarray = (AV*)sv_2mortal( (SV*)newAV() );
   const char* err = ExtractCellArray(outarray, tempalgo);
   delete tempalgo;
   if (err) {
      // assume Perl interpreter will free all memory when it quits???
      // int key = av_len(inarray);
      // while (key >= 0) { av_delete(outarray, key, G_DISCARD); key--; }
      // av_undef(outarray);
      PERL_ERROR(err);
   }

   SP -= items;
   ST(0) = newRV( (SV*)outarray );
   sv_2mortal(ST(0));
   XSRETURN(1);
}

// -----------------------------------------------------------------------------

XS(pl_putcells)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items < 1 || items > 8)
      PERL_ERROR("Usage: g_putcells($cells,$x=0,$y=0,$axx=1,$axy=0,$ayx=0,$ayy=1,$mode='or')");

   SV* cells = ST(0);
   if ( (!SvROK(cells)) || (SvTYPE(SvRV(cells)) != SVt_PVAV) ) {
       PERL_ERROR("g_putcells error: 1st parameter is not a valid array reference");
   }
   AV* inarray = (AV*)SvRV(cells);
   int num_cells = (av_len(inarray) + 1) / 2;
   // note that av_len returns max index or -1 if array is empty

   // default values for optional params
   int x0  = 0;
   int y0  = 0;
   int axx = 1;
   int axy = 0;
   int ayx = 0;
   int ayy = 1;
   // default for mode is 'or'; 'xor' mode is also supported;
   // 'copy' mode currently has the same effect as 'or' mode
   // because there is no bounding box to set OFF cells
   char* mode = "or";
   
   STRLEN n_a;
   if (items > 1) x0  = SvIV(ST(1));
   if (items > 2) y0  = SvIV(ST(2));
   if (items > 3) axx = SvIV(ST(3));
   if (items > 4) axy = SvIV(ST(4));
   if (items > 5) ayx = SvIV(ST(5));
   if (items > 6) ayy = SvIV(ST(6));
   if (items > 7) mode = SvPV(ST(7), n_a);

   lifealgo* curralgo = currlayer->algo;

   // save cell changes if undo/redo is enabled and script isn't constructing a pattern
   bool savecells = allowundo && !currlayer->stayclean;
   // better to use ChangeCell and combine all changes due to consecutive setcell/putcells
   // if (savecells) SavePendingChanges();

   wxString modestr = wxString(mode, wxConvLocal);
   if ( !(modestr.IsSameAs(wxT("or"), false)
          || modestr.IsSameAs(wxT("xor"), false)
          || modestr.IsSameAs(wxT("copy"), false)
          || modestr.IsSameAs(wxT("not"), false)) ) {
      PERL_ERROR("g_putcells error: unknown mode");
   }
   
   if (modestr.IsSameAs(wxT("copy"), false)) {
      // TODO: find bounds of cell array and call ClearRect here (to be added to wxedit.cpp)
   }

   if (modestr.IsSameAs(wxT("xor"), false)) {
      // loop code is duplicated here to allow 'or' case to execute faster
      for (int n = 0; n < num_cells; n++) {
         int x = SvIV( *av_fetch(inarray, 2 * n, 0) );
         int y = SvIV( *av_fetch(inarray, 2 * n + 1, 0) );
         int newx = x0 + x * axx + y * axy;
         int newy = y0 + x * ayx + y * ayy;
         int s = curralgo->getcell(newx, newy);

         if (savecells) ChangeCell(newx, newy);

         // paste (possibly transformed) cell into current universe
         curralgo->setcell(newx, newy, 1-s);

         if ((n % 4096) == 0 && PerlScriptAborted()) break;
      }
   } else {
      int cellstate = (modestr.IsSameAs(wxT("not"), false)) ? 0 : 1 ;
      for (int n = 0; n < num_cells; n++) {
         int x = SvIV( *av_fetch(inarray, 2 * n, 0) );
         int y = SvIV( *av_fetch(inarray, 2 * n + 1, 0) );
         int newx = x0 + x * axx + y * axy;
         int newy = y0 + x * ayx + y * ayy;

         if (savecells && cellstate != currlayer->algo->getcell(newx, newy))
            ChangeCell(newx, newy);

         // paste (possibly transformed) cell into current universe
         curralgo->setcell(newx, newy, cellstate);

         if ((n % 4096) == 0 && PerlScriptAborted()) break;
      }
   }

   curralgo->endofpattern();

   MarkLayerDirty();
   DoAutoUpdate();

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_getcells)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0 && items != 4) PERL_ERROR("Usage: $cells = g_getcells(@rect)");

   // convert pattern in given rect into a cell array (ie. array of live cell coords)
   AV* outarray = (AV*)sv_2mortal( (SV*)newAV() );

   if (items == 0) {
      // return empty cell array
   } else {
      // items == 4
      int x  = SvIV(ST(0));
      int y  = SvIV(ST(1));
      int wd = SvIV(ST(2));
      int ht = SvIV(ST(3));
      // first check that wd & ht are > 0
      if (wd <= 0) PERL_ERROR("g_getcells error: width must be > 0");
      if (ht <= 0) PERL_ERROR("g_getcells error: height must be > 0");
      int right = x + wd - 1;
      int bottom = y + ht - 1;
      int cx, cy;
      int v = 0 ;
      int cntr = 0;
      lifealgo* curralgo = currlayer->algo;
      for ( cy=y; cy<=bottom; cy++ ) {
         for ( cx=x; cx<=right; cx++ ) {
            /** FIXME:  make it work with multistate */
            int skip = curralgo->nextcell(cx, cy, v);
            if (skip >= 0) {
               // found next live cell in this row so add coords to outarray
               cx += skip;
               if (cx <= right) {
                  av_push(outarray, newSViv(cx));
                  av_push(outarray, newSViv(cy));
               }
            } else {
               cx = right;  // done this row
            }
            cntr++;
            if ((cntr % 4096) == 0) RETURN_IF_ABORTED;
         }
      }
   }

   SP -= items;
   ST(0) = newRV( (SV*)outarray );
   sv_2mortal(ST(0));
   XSRETURN(1);
}

// -----------------------------------------------------------------------------

XS(pl_hash)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 4) PERL_ERROR("Usage: $int = g_hash(@rect)");

   int x  = SvIV(ST(0));
   int y  = SvIV(ST(1));
   int wd = SvIV(ST(2));
   int ht = SvIV(ST(3));
   if (wd <= 0) PERL_ERROR("g_hash error: width must be > 0");
   if (ht <= 0) PERL_ERROR("g_hash error: height must be > 0");
   int right = x + wd - 1;
   int bottom = y + ht - 1;
   int cx, cy;
   int v = 0 ;
   int cntr = 0;
   
   // calculate a hash value for pattern in given rect
   int hash = 31415962;
   lifealgo* curralgo = currlayer->algo;
   for ( cy=y; cy<=bottom; cy++ ) {
      int yshift = cy - y;
      for ( cx=x; cx<=right; cx++ ) {
         /** FIXME:  make it work with multistate */
         int skip = curralgo->nextcell(cx, cy, v);
         if (skip >= 0) {
            // found next live cell in this row
            cx += skip;
            if (cx <= right) {
               hash = (hash * 33 + yshift) ^ (cx - x);
            }
         } else {
            cx = right;  // done this row
         }
         cntr++;
         if ((cntr % 4096) == 0) RETURN_IF_ABORTED;
      }
   }

   XSRETURN_IV(hash);
}

// -----------------------------------------------------------------------------

XS(pl_getclip)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: $cells = g_getclip()");

   if (!mainptr->ClipboardHasText()) {
      PERL_ERROR("g_getclip error: no pattern in clipboard");
   }

   // convert pattern in clipboard into a cell array, but where the first 2 items
   // are the pattern's width and height (not necessarily the minimal bounding box
   // because the pattern might have empty borders, or it might even be empty)
   AV* outarray = (AV*)sv_2mortal( (SV*)newAV() );

   // create a temporary universe for storing clipboard pattern;
   // use qlife because its setcell/getcell calls are faster
   lifealgo* tempalgo = CreateNewUniverse(QLIFE_ALGO, allowcheck);

   // read clipboard pattern into temporary universe and set edges
   // (not a minimal bounding box if pattern is empty or has empty borders)
   bigint top, left, bottom, right;
   if ( viewptr->GetClipboardPattern(&tempalgo, &top, &left, &bottom, &right) ) {
      if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
         PERL_ERROR("g_getclip error: pattern is too big");
      }
      int itop = top.toint();
      int ileft = left.toint();
      int ibottom = bottom.toint();
      int iright = right.toint();
      int wd = iright - ileft + 1;
      int ht = ibottom - itop + 1;

      av_push(outarray, newSViv(wd));
      av_push(outarray, newSViv(ht));

      // extract cells from tempalgo
      int cx, cy;
      int cntr = 0;
      int v = 0 ;
      for ( cy=itop; cy<=ibottom; cy++ ) {
         for ( cx=ileft; cx<=iright; cx++ ) {
            /** FIXME:  make it work with multistate */
            int skip = tempalgo->nextcell(cx, cy, v);
            if (skip >= 0) {
               // found next live cell in this row
               cx += skip;
               // shift cells so that top left cell of bounding box is at 0,0
               av_push(outarray, newSViv(cx - ileft));
               av_push(outarray, newSViv(cy - itop));
            } else {
               cx = iright;  // done this row
            }
            cntr++;
            if ((cntr % 4096) == 0 && PerlScriptAborted()) {
               delete tempalgo;
               Perl_croak(aTHX_ NULL);
            }
         }
      }

      delete tempalgo;
   } else {
      // assume error message has been displayed
      delete tempalgo;
      Perl_croak(aTHX_ NULL);
   }

   SP -= items;
   ST(0) = newRV( (SV*)outarray );
   sv_2mortal(ST(0));
   XSRETURN(1);
}

// -----------------------------------------------------------------------------

XS(pl_select)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0 && items != 4) PERL_ERROR("Usage: g_select(@rect)");

   if (items == 0) {
      // remove any existing selection
      GSF_select(0, 0, 0, 0);
   } else {
      // items == 4
      int x  = SvIV(ST(0));
      int y  = SvIV(ST(1));
      int wd = SvIV(ST(2));
      int ht = SvIV(ST(3));
      // first check that wd & ht are > 0
      if (wd <= 0) PERL_ERROR("g_select error: width must be > 0");
      if (ht <= 0) PERL_ERROR("g_select error: height must be > 0");
      // set selection rect
      GSF_select(x, y, wd, ht);
   }
   DoAutoUpdate();

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_getrect)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: @rect = g_getrect()");

   if (!currlayer->algo->isEmpty()) {
      bigint top, left, bottom, right;
      currlayer->algo->findedges(&top, &left, &bottom, &right);
      if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
         PERL_ERROR("g_getrect error: pattern is too big");
      }
      int x = left.toint();
      int y = top.toint();
      int wd = right.toint() - x + 1;
      int ht = bottom.toint() - y + 1;

      // items == 0 so no need to reset stack pointer
      // SP -= items;
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

XS(pl_getselrect)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: @rect = g_getselrect()");
   
   if (viewptr->SelectionExists()) {
      if (currlayer->currsel.TooBig()) {
         PERL_ERROR("g_getselrect error: selection is too big");
      }
      int x, y, wd, ht;
      currlayer->currsel.GetRect(&x, &y, &wd, &ht);
      
      // items == 0 so no need to reset stack pointer
      // SP -= items;
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

XS(pl_setcell)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 3) PERL_ERROR("Usage: g_setcell($x,$y,$state)");
   
   int x = SvIV(ST(0));
   int y = SvIV(ST(1));
   int state = SvIV(ST(2));

   GSF_setcell(x, y, state);
   
   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_getcell)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 2) PERL_ERROR("Usage: $state = g_getcell($x,$y)");

   int state = currlayer->algo->getcell(SvIV(ST(0)), SvIV(ST(1)));
   
   XSRETURN_IV(state);
}

// -----------------------------------------------------------------------------

XS(pl_setcursor)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) PERL_ERROR("Usage: $oldcurs = g_setcursor($newcurs)");

   int oldindex = CursorToIndex(currlayer->curs);
   wxCursor* curs = IndexToCursor(SvIV(ST(0)));
   if (curs) {
      viewptr->SetCursorMode(curs);
      // see the cursor change, including in tool bar
      mainptr->UpdateUserInterface(mainptr->IsActive());
   } else {
      PERL_ERROR("g_setcursor error: bad cursor index");
   }

   // return old index (simplifies saving and restoring cursor)
   XSRETURN_IV(oldindex);
}

// -----------------------------------------------------------------------------

XS(pl_getcursor)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: $int = g_getcursor()");

   XSRETURN_IV(CursorToIndex(currlayer->curs));
}

// -----------------------------------------------------------------------------

XS(pl_empty)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: $bool = g_empty()");

   XSRETURN_IV(currlayer->algo->isEmpty() ? 1 : 0);
}

// -----------------------------------------------------------------------------

XS(pl_run)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) PERL_ERROR("Usage: g_run($numgens)");

   int ngens = SvIV(ST(0));

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

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_step)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: g_step()");

   if (!currlayer->algo->isEmpty()) {
      mainptr->NextGeneration(true);      // step by current increment
      DoAutoUpdate();
   }

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_setstep)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) PERL_ERROR("Usage: g_setstep($int)");

   mainptr->SetWarp(SvIV(ST(0)));
   DoAutoUpdate();

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_getstep)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: $int = g_getstep()");

   XSRETURN_IV(currlayer->warp);
}

// -----------------------------------------------------------------------------

XS(pl_setbase)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) PERL_ERROR("Usage: g_setbase($int)");

   int base = SvIV(ST(0));

   if (base < 2) base = 2;
   if (base > MAX_BASESTEP) base = MAX_BASESTEP;
   algobase[currlayer->algtype] = base;
   mainptr->UpdateWarp();
   DoAutoUpdate();

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_getbase)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: $int = g_getbase()");

   XSRETURN_IV(algobase[currlayer->algtype]);
}

// -----------------------------------------------------------------------------

XS(pl_advance)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 2) PERL_ERROR("Usage: g_advance($where,$numgens)");

   int where = SvIV(ST(0));
   int ngens = SvIV(ST(1));

   if (ngens > 0) {
      if (viewptr->SelectionExists()) {
         while (ngens > 0) {
            ngens--;
            if (where == 0)
               currlayer->currsel.Advance();
            else
               currlayer->currsel.AdvanceOutside();
         }
         DoAutoUpdate();
      } else {
         PERL_ERROR("g_advance error: no selection");
      }
   }

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_reset)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: g_reset()");

   if (currlayer->algo->getGeneration() != currlayer->startgen) {
      mainptr->ResetPattern();
      DoAutoUpdate();
   }

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_setgen)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) PERL_ERROR("Usage: g_setgen($string)");

   STRLEN n_a;
   char* genstring = SvPV(ST(0), n_a);

   const char* errmsg = GSF_setgen(genstring);
   if (errmsg) PERL_ERROR(errmsg);

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_getgen)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items > 1) PERL_ERROR("Usage: $string = g_getgen($sepchar='')");

   char sepchar = '\0';
   if (items > 0) {
      STRLEN n_a;
      char* s = SvPV(ST(0), n_a);
      sepchar = s[0];
   }

   XSRETURN_PV(currlayer->algo->getGeneration().tostring(sepchar));
}

// -----------------------------------------------------------------------------

XS(pl_getpop)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items > 1) PERL_ERROR("Usage: $string = g_getpop($sepchar='')");

   char sepchar = '\0';
   if (items > 0) {
      STRLEN n_a;
      char* s = SvPV(ST(0), n_a);
      sepchar = s[0];
   }

   XSRETURN_PV(currlayer->algo->getPopulation().tostring(sepchar));
}

// -----------------------------------------------------------------------------

XS(pl_setrule)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) PERL_ERROR("Usage: g_setrule($string)");

   STRLEN n_a;
   char* rulestring = SvPV(ST(0), n_a);

   const char* errmsg = GSF_setrule(rulestring);
   if (errmsg) PERL_ERROR(errmsg);

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_getrule)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: $string = g_getrule()");

   XSRETURN_PV(currlayer->algo->getrule());
}

// -----------------------------------------------------------------------------

XS(pl_setpos)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 2) PERL_ERROR("Usage: g_setpos($xstring,$ystring)");

   STRLEN n_a;
   char* x = SvPV(ST(0), n_a);
   char* y = SvPV(ST(1), n_a);

   const char* errmsg = GSF_setpos(x, y);
   if (errmsg) PERL_ERROR(errmsg);

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_getpos)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items > 1) PERL_ERROR("Usage: @xy = g_getpos($sepchar='')");

   char sepchar = '\0';
   if (items > 0) {
      STRLEN n_a;
      char* s = SvPV(ST(0), n_a);
      sepchar = s[0];
   }

   bigint bigx, bigy;
   viewptr->GetPos(bigx, bigy);

   // return position as x,y strings
   SP -= items;
   XPUSHs(sv_2mortal(newSVpv( bigx.tostring(sepchar), 0 )));
   XPUSHs(sv_2mortal(newSVpv( bigy.tostring(sepchar), 0 )));
   XSRETURN(2);
}

// -----------------------------------------------------------------------------

XS(pl_setmag)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) PERL_ERROR("Usage: g_setmag($int)");

   int mag = SvIV(ST(0));

   viewptr->SetMag(mag);
   DoAutoUpdate();

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_getmag)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: $int = g_getmag()");

   XSRETURN_IV(viewptr->GetMag());
}

// -----------------------------------------------------------------------------

XS(pl_fit)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: g_fit()");

   viewptr->FitPattern();
   DoAutoUpdate();

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_fitsel)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: g_fitsel()");

   if (viewptr->SelectionExists()) {
      viewptr->FitSelection();
      DoAutoUpdate();
   } else {
      PERL_ERROR("g_fitsel error: no selection");
   }

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_visrect)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 4) PERL_ERROR("Usage: $bool = g_visrect(@rect)");

   int x = SvIV(ST(0));
   int y = SvIV(ST(1));
   int wd = SvIV(ST(2));
   int ht = SvIV(ST(3));
   // check that wd & ht are > 0
   if (wd <= 0) PERL_ERROR("g_visrect error: width must be > 0");
   if (ht <= 0) PERL_ERROR("g_visrect error: height must be > 0");

   bigint left = x;
   bigint top = y;
   bigint right = x + wd - 1;
   bigint bottom = y + ht - 1;
   int visible = viewptr->CellVisible(left, top) &&
                 viewptr->CellVisible(right, bottom);

   XSRETURN_IV(visible);
}

// -----------------------------------------------------------------------------

XS(pl_update)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: g_update()");

   GSF_update();
   
   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_autoupdate)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) PERL_ERROR("Usage: g_autoupdate($bool)");

   autoupdate = (SvIV(ST(0)) != 0);

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_addlayer)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: $newindex = g_addlayer()");

   if (numlayers >= MAX_LAYERS) {
      PERL_ERROR("g_addlayer error: no more layers can be added");
   } else {
      AddLayer();
      DoAutoUpdate();
   }

   // return index of new layer
   XSRETURN_IV(currindex);
}

// -----------------------------------------------------------------------------

XS(pl_clone)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: $newindex = g_clone()");

   if (numlayers >= MAX_LAYERS) {
      PERL_ERROR("g_clone error: no more layers can be added");
   } else {
      CloneLayer();
      DoAutoUpdate();
   }

   // return index of new layer
   XSRETURN_IV(currindex);
}

// -----------------------------------------------------------------------------

XS(pl_duplicate)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: $newindex = g_duplicate()");

   if (numlayers >= MAX_LAYERS) {
      PERL_ERROR("g_duplicate error: no more layers can be added");
   } else {
      DuplicateLayer();
      DoAutoUpdate();
   }

   // return index of new layer
   XSRETURN_IV(currindex);
}

// -----------------------------------------------------------------------------

XS(pl_dellayer)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: g_dellayer()");

   if (numlayers <= 1) {
      PERL_ERROR("g_dellayer error: there is only one layer");
   } else {
      DeleteLayer();
      DoAutoUpdate();
   }

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_movelayer)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 2) PERL_ERROR("Usage: g_movelayer($from,$to)");

   int fromindex = SvIV(ST(0));
   int toindex = SvIV(ST(1));

   if (fromindex < 0 || fromindex >= numlayers) {
      char msg[64];
      sprintf(msg, "Bad g_movelayer fromindex (%d)", fromindex);
      PERL_ERROR(msg);
   }
   if (toindex < 0 || toindex >= numlayers) {
      char msg[64];
      sprintf(msg, "Bad g_movelayer toindex (%d)", toindex);
      PERL_ERROR(msg);
   }

   MoveLayer(fromindex, toindex);
   DoAutoUpdate();

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_setlayer)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) PERL_ERROR("Usage: g_setlayer($index)");

   int index = SvIV(ST(0));

   if (index < 0 || index >= numlayers) {
      char msg[64];
      sprintf(msg, "Bad g_setlayer index (%d)", index);
      PERL_ERROR(msg);
   }

   SetLayer(index);
   DoAutoUpdate();

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_getlayer)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: g_getlayer()");

   XSRETURN_IV(currindex);
}

// -----------------------------------------------------------------------------

XS(pl_numlayers)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: g_numlayers()");

   XSRETURN_IV(numlayers);
}

// -----------------------------------------------------------------------------

XS(pl_maxlayers)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: g_maxlayers()");

   XSRETURN_IV(MAX_LAYERS);
}

// -----------------------------------------------------------------------------

XS(pl_setname)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items < 1 || items > 2) PERL_ERROR("Usage: g_setname($name,$index=current)");

   STRLEN n_a;
   char* name = SvPV(ST(0), n_a);
   int index = currindex;
   if (items > 1) index = SvIV(ST(1));

   if (index < 0 || index >= numlayers) {
      char msg[64];
      sprintf(msg, "Bad g_setname index (%d)", index);
      PERL_ERROR(msg);
   }
   
   GSF_setname(name, index);

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_getname)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items > 1) PERL_ERROR("Usage: $name = g_getname($index=current)");

   int index = currindex;
   if (items > 0) index = SvIV(ST(0));

   if (index < 0 || index >= numlayers) {
      char msg[64];
      sprintf(msg, "Bad g_getname index (%d)", index);
      PERL_ERROR(msg);
   }

   // need to be careful converting Unicode wxString to char*
   wxCharBuffer name = GetLayer(index)->currname.mb_str(wxConvLocal);
   XSRETURN_PV((const char*)name);
}

// -----------------------------------------------------------------------------

XS(pl_setoption)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 2) PERL_ERROR("Usage: $oldval = g_setoption($name,$newval)");

   STRLEN n_a;
   char* optname = SvPV(ST(0), n_a);
   int newval = SvIV(ST(1));
   int oldval;

   if (!GSF_setoption(optname, newval, &oldval)) {
      PERL_ERROR("g_setoption error: unknown option");
   }

   // return old value (simplifies saving and restoring settings)
   XSRETURN_IV(oldval);
}

// -----------------------------------------------------------------------------

XS(pl_getoption)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) PERL_ERROR("Usage: $int = g_getoption($name)");

   STRLEN n_a;
   char* optname = SvPV(ST(0), n_a);
   int optval;

   if (!GSF_getoption(optname, &optval)) {
      PERL_ERROR("g_getoption error: unknown option");
   }

   XSRETURN_IV(optval);
}

// -----------------------------------------------------------------------------

XS(pl_setcolor)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 4) PERL_ERROR("Usage: @oldrgb = g_setcolor($name,$r,$g,$b)");

   STRLEN n_a;
   char* colname = SvPV(ST(0), n_a);
   wxColor newcol(SvIV(ST(1)), SvIV(ST(2)), SvIV(ST(3)));
   wxColor oldcol;

   if (!GSF_setcolor(colname, newcol, oldcol)) {
      PERL_ERROR("g_setcolor error: unknown color");
   }

   // return old r,g,b values (simplifies saving and restoring colors)
   SP -= items;
   XPUSHs(sv_2mortal(newSViv(oldcol.Red())));
   XPUSHs(sv_2mortal(newSViv(oldcol.Green())));
   XPUSHs(sv_2mortal(newSViv(oldcol.Blue())));
   XSRETURN(3);
}

// -----------------------------------------------------------------------------

XS(pl_getcolor)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) PERL_ERROR("Usage: @rgb = g_getcolor($name)");

   STRLEN n_a;
   char* colname = SvPV(ST(0), n_a);
   wxColor color;

   if (!GSF_getcolor(colname, color)) {
      PERL_ERROR("g_getcolor error: unknown color");
   }

   // return r,g,b values
   SP -= items;
   XPUSHs(sv_2mortal(newSViv(color.Red())));
   XPUSHs(sv_2mortal(newSViv(color.Green())));
   XPUSHs(sv_2mortal(newSViv(color.Blue())));
   XSRETURN(3);
}

// -----------------------------------------------------------------------------

XS(pl_getstring)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items < 1 || items > 3)
      PERL_ERROR("Usage: $string = g_getstring($prompt,$default='',$title='')");

   STRLEN n_a;
   char* prompt = SvPV(ST(0), n_a);
   char* initial = "";
   char* title = "";
   if (items > 1) initial = SvPV(ST(1),n_a);
   if (items > 2) title = SvPV(ST(2),n_a);
   
   wxString result;
   if ( !GetString(wxString(title,wxConvLocal), wxString(prompt,wxConvLocal),
                   wxString(initial,wxConvLocal), result) ) {
      // user hit Cancel button
      AbortPerlScript();
      Perl_croak(aTHX_ NULL);
   }

   XSRETURN_PV(result.mb_str(wxConvLocal));
}

// -----------------------------------------------------------------------------

XS(pl_getkey)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 0) PERL_ERROR("Usage: $char = g_getkey()");

   char s[2];        // room for char + NULL
   GSF_getkey(s);

   XSRETURN_PV(s);
}

// -----------------------------------------------------------------------------

XS(pl_dokey)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) PERL_ERROR("Usage: g_dokey($char)");

   STRLEN n_a;
   char* ascii = SvPV(ST(0), n_a);

   GSF_dokey(ascii);

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_show)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) PERL_ERROR("Usage: g_show($string)");

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

XS(pl_error)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) PERL_ERROR("Usage: g_error($string)");

   STRLEN n_a;
   char* s = SvPV(ST(0), n_a);

   inscript = false;
   statusptr->ErrorMessage(wxString(s,wxConvLocal));
   inscript = true;
   // make sure status bar is visible
   if (!showstatus) mainptr->ToggleStatusBar();
   
   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_warn)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) PERL_ERROR("Usage: g_warn($string)");

   STRLEN n_a;
   char* s = SvPV(ST(0), n_a);

   Warning(wxString(s,wxConvLocal));
   
   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_note)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) PERL_ERROR("Usage: g_note($string)");

   STRLEN n_a;
   char* s = SvPV(ST(0), n_a);

   Note(wxString(s,wxConvLocal));
   
   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_help)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) PERL_ERROR("Usage: g_help($string)");

   STRLEN n_a;
   char* htmlfile = SvPV(ST(0), n_a);

   ShowHelp(wxString(htmlfile,wxConvLocal));
   
   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_check)
{
   IGNORE_UNUSED_PARAMS;
   // don't call checkevents() here otherwise we can't safely write code like
   //    if (g_getlayer() == target) {
   //       g_check(0);
   //       ... do stuff to target layer ...
   //       g_check(1);
   //    }
   // RETURN_IF_ABORTED;
   dXSARGS;
   if (items != 1) PERL_ERROR("Usage: g_check($bool)");

   allowcheck = (SvIV(ST(0)) != 0);

   XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_exit)
{
   IGNORE_UNUSED_PARAMS;
   RETURN_IF_ABORTED;
   dXSARGS;
   if (items > 1) PERL_ERROR("Usage: g_exit($string='')");

   STRLEN n_a;
   char* errmsg = (items == 1) ? SvPV(ST(0),n_a) : NULL;
   
   GSF_exit(errmsg);
   AbortPerlScript();
   Perl_croak(aTHX_ NULL);
}

// -----------------------------------------------------------------------------

XS(pl_fatal)
{
   IGNORE_UNUSED_PARAMS;
   // don't call RETURN_IF_ABORTED;
   dXSARGS;
   // don't call PERL_ERROR in here
   if (items != 1) Warning(_("Bug: usage is g_fatal($string)"));

   STRLEN n_a;
   char* errmsg = SvPV(ST(0),n_a);
   
   // store message in global string (shown after script finishes)
   scripterr = wxString(errmsg, wxConvLocal);
   
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
   newXS("golly::g_setcell",      pl_setcell,      "");
   newXS("golly::g_getcell",      pl_getcell,      "");
   // etc...
   
   XSRETURN_YES;
}
*/

// -----------------------------------------------------------------------------

// xs_init is passed into perl_parse and initializes statically linked extensions

EXTERN_C void xs_init(pTHX)
{
   #ifdef __WXMSW__
      wxUnusedVar(my_perl);
   #endif
   char* file = __FILE__;
   dXSUB_SYS;

   // DynaLoader allows dynamic loading of other Perl extensions
   newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);

   /* for this approach to work we presumably need to create a golly.pm file
      in Scripts/Perl and add that path to @INC!!!
      "use golly" causes error: Can't locate golly.pm in @INC
      newXS("golly::boot_golly", boot_golly, file);
   */

   // filing
   newXS("g_open",         pl_open,         file);
   newXS("g_save",         pl_save,         file);
   newXS("g_load",         pl_load,         file);
   newXS("g_store",        pl_store,        file);
   newXS("g_appdir",       pl_appdir,       file);
   newXS("g_datadir",      pl_datadir,      file);
   // editing
   newXS("g_new",          pl_new,          file);
   newXS("g_cut",          pl_cut,          file);
   newXS("g_copy",         pl_copy,         file);
   newXS("g_clear",        pl_clear,        file);
   newXS("g_paste",        pl_paste,        file);
   newXS("g_shrink",       pl_shrink,       file);
   newXS("g_randfill",     pl_randfill,     file);
   newXS("g_flip",         pl_flip,         file);
   newXS("g_rotate",       pl_rotate,       file);
   newXS("g_parse",        pl_parse,        file);
   newXS("g_transform",    pl_transform,    file);
   newXS("g_evolve",       pl_evolve,       file);
   newXS("g_putcells",     pl_putcells,     file);
   newXS("g_getcells",     pl_getcells,     file);
   newXS("g_hash",         pl_hash,         file);
   newXS("g_getclip",      pl_getclip,      file);
   newXS("g_select",       pl_select,       file);
   newXS("g_getrect",      pl_getrect,      file);
   newXS("g_getselrect",   pl_getselrect,   file);
   newXS("g_setcell",      pl_setcell,      file);
   newXS("g_getcell",      pl_getcell,      file);
   newXS("g_setcursor",    pl_setcursor,    file);
   newXS("g_getcursor",    pl_getcursor,    file);
   // control
   newXS("g_empty",        pl_empty,        file);
   newXS("g_run",          pl_run,          file);
   newXS("g_step",         pl_step,         file);
   newXS("g_setstep",      pl_setstep,      file);
   newXS("g_getstep",      pl_getstep,      file);
   newXS("g_setbase",      pl_setbase,      file);
   newXS("g_getbase",      pl_getbase,      file);
   newXS("g_advance",      pl_advance,      file);
   newXS("g_reset",        pl_reset,        file);
   newXS("g_setgen",       pl_setgen,       file);
   newXS("g_getgen",       pl_getgen,       file);
   newXS("g_getpop",       pl_getpop,       file);
   newXS("g_setrule",      pl_setrule,      file);
   newXS("g_getrule",      pl_getrule,      file);
   // viewing
   newXS("g_setpos",       pl_setpos,       file);
   newXS("g_getpos",       pl_getpos,       file);
   newXS("g_setmag",       pl_setmag,       file);
   newXS("g_getmag",       pl_getmag,       file);
   newXS("g_fit",          pl_fit,          file);
   newXS("g_fitsel",       pl_fitsel,       file);
   newXS("g_visrect",      pl_visrect,      file);
   newXS("g_update",       pl_update,       file);
   newXS("g_autoupdate",   pl_autoupdate,   file);
   // layers
   newXS("g_addlayer",     pl_addlayer,     file);
   newXS("g_clone",        pl_clone,        file);
   newXS("g_duplicate",    pl_duplicate,    file);
   newXS("g_dellayer",     pl_dellayer,     file);
   newXS("g_movelayer",    pl_movelayer,    file);
   newXS("g_setlayer",     pl_setlayer,     file);
   newXS("g_getlayer",     pl_getlayer,     file);
   newXS("g_numlayers",    pl_numlayers,    file);
   newXS("g_maxlayers",    pl_maxlayers,    file);
   newXS("g_setname",      pl_setname,      file);
   newXS("g_getname",      pl_getname,      file);
   // miscellaneous
   newXS("g_setoption",    pl_setoption,    file);
   newXS("g_getoption",    pl_getoption,    file);
   newXS("g_setcolor",     pl_setcolor,     file);
   newXS("g_getcolor",     pl_getcolor,     file);
   newXS("g_getstring",    pl_getstring,    file);
   newXS("g_getkey",       pl_getkey,       file);
   newXS("g_dokey",        pl_dokey,        file);
   newXS("g_show",         pl_show,         file);
   newXS("g_error",        pl_error,        file);
   newXS("g_warn",         pl_warn,         file);
   newXS("g_note",         pl_note,         file);
   newXS("g_help",         pl_help,         file);
   newXS("g_check",        pl_check,        file);
   newXS("g_exit",         pl_exit,         file);
   // internal use only (don't document)
   newXS("g_fatal",        pl_fatal,        file);
}

// =============================================================================

void RunPerlScript(const wxString &filepath)
{
   #ifdef USE_PERL_DYNAMIC
      if (perldll == NULL) {
         // try to load Perl library
         if ( !LoadPerlLib() ) return;
      }
   #endif

   my_perl = perl_alloc();
   if (!my_perl) {
      Warning(_("Could not create Perl interpreter!"));
      return;
   }
   
   perl_construct(my_perl);
   
   // set PERL_EXIT_DESTRUCT_END flag so that perl_destruct will execute
   // any END blocks in given script (this flag requires Perl 5.7.2+)
   PL_exit_flags |= PERL_EXIT_DESTRUCT_END;

   /* this code works but doesn't let us capture error messages
   wxCharBuffer buff = filepath.mb_str(wxConvLocal);
   char* my_argv[2];
   my_argv[0] = "";
   my_argv[1] = (char*) buff;
   perl_parse(my_perl, xs_init, 2, my_argv, NULL);
   perl_run(my_perl);
   */

   static char* embedding[] = { "", "-e", "" };
   perl_parse(my_perl, xs_init, 3, embedding, NULL);
   perl_run(my_perl);

   // convert any \ to \\ and then convert any ' to \'
   wxString fpath = filepath;
   fpath.Replace(wxT("\\"), wxT("\\\\"));
   fpath.Replace(wxT("'"), wxT("\\'"));

   // construct a command to run the given script file and capture errors
   wxString command = wxT("do '") + fpath + wxT("'; g_fatal($@) if $@;");
   perl_eval_pv(command.mb_str(wxConvLocal), TRUE);

   // any END blocks will now be executed by perl_destruct, so we temporarily
   // clear scripterr so that RETURN_IF_ABORTED won't call Perl_croak;
   // this allows g_* commands in END blocks to work after user hits escape
   // or if g_exit has been called
   wxString savestring = scripterr;
   scripterr = wxEmptyString;
   perl_destruct(my_perl);
   scripterr = savestring;
   
   perl_free(my_perl);
}

// -----------------------------------------------------------------------------

void FinishPerlScripting()
{
   #ifdef USE_PERL_DYNAMIC
      // probably don't really need to do this
      FreePerlLib();
   #endif
}
