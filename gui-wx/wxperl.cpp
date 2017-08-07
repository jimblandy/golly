// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
    #include "wx/wx.h"     // for all others include the necessary headers
#endif

#include "wx/filename.h"   // for wxFileName

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "readpattern.h"
#include "writepattern.h"

#include "wxgolly.h"       // for wxGetApp, mainptr, viewptr, statusptr
#include "wxmain.h"        // for mainptr->...
#include "wxselect.h"      // for Selection
#include "wxview.h"        // for viewptr->...
#include "wxstatus.h"      // for statusptr->...
#include "wxutils.h"       // for Warning, Note, GetString, etc
#include "wxprefs.h"       // for perllib, gollydir, etc
#include "wxinfo.h"        // for ShowInfo
#include "wxhelp.h"        // for ShowHelp
#include "wxundo.h"        // for currlayer->undoredo->...
#include "wxalgos.h"       // for *_ALGO, CreateNewUniverse, etc
#include "wxlayer.h"       // for AddLayer, currlayer, currindex, etc
#include "wxscript.h"      // for inscript, abortmsg, GSF_*, etc
#include "wxperl.h"

// =============================================================================

#ifdef ENABLE_PERL

/*
    Golly uses an embedded Perl interpreter to execute scripts.
    See "perldoc perlembed" for details.
    Perl is Copyright (C) 1993-2007, by Larry Wall and others.
    It is free software; you can redistribute it and/or modify it under the terms of either:
    a) the GNU General Public License as published by the Free Software Foundation;
       either version 1, or (at your option) any later version, or
    b) the "Artistic License" (http://dev.perl.org/licenses/artistic.html).
*/

#ifndef __WXMAC__
    #include "wx/dynlib.h"     // for wxDynamicLibrary
#endif

// avoid warning about _ being redefined
#undef _

#ifdef __WXMSW__
    // on Windows, wxWidgets defines uid_t/gid_t which breaks Perl's typedefs:
    #undef uid_t
    #undef gid_t
    // can't do "#undef mode_t" for a typedef so use this hack:
    typedef unsigned short MODE1;  // from C:\Perl\lib\CORE\win32.h
    typedef int MODE2;             // from C:\wxWidgets\include\wx\filefn.h
    #define mode_t MODE1
#endif

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#ifdef __WXMSW__
    #undef mode_t
    #define mode_t MODE2
#endif

// restore wxWidgets definition for _ (from include/wx/intl.h)
#undef _
#define _(s) wxGetTranslation(_T(s))

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
#define PERL589_OR_LATER
#endif
#if (PERL_REVISION == 5) && (PERL_VERSION == 8) && (PERL_SUBVERSION >= 9)
#define PERL589_OR_LATER
#endif
#if (PERL_REVISION == 5) && (PERL_VERSION >= 9)
#define PERL589_OR_LATER
#endif

// check if we're building with Perl 5.10 or later
#if (ACTIVEPERL_VERSION >= 1000)
#define PERL510_OR_LATER
#endif
#if (PERL_REVISION == 5) && (PERL_VERSION >= 10)
#define PERL510_OR_LATER
#endif

// check if we're building with Perl 5.10.1 or later
#if (PERL_REVISION == 5) && (PERL_VERSION == 10) && (PERL_SUBVERSION >= 1)
#define PERL5101_OR_LATER
#endif
#if (PERL_REVISION == 5) && (PERL_VERSION >= 11)
#define PERL5101_OR_LATER
#endif

// check if we're building with Perl 5.14 or later
#if (PERL_REVISION == 5) && (PERL_VERSION >= 14)
#define PERL514_OR_LATER
#endif

// Check if PL_thr_key is a real variable or instead a macro which calls
// Perl_Gthr_key_ptr(NULL), which was the default before Perl 5.14:
#ifdef PL_thr_key
#define PERL_THR_KEY_FUNC 1
#endif

static PerlInterpreter* my_perl = NULL;

EXTERN_C void boot_DynaLoader(pTHX_ CV* cv);

// =============================================================================

// On Windows and Linux we try to load the Perl library at runtime so Golly
// will start up even if Perl isn't installed.

// On Linux we can only load libperl dynamically if using Perl 5.10 or later.
// In older Perl versions boot_DynaLoader is in DynaLoader.a and so libperl
// has to be statically linked.

#if defined(__WXMSW__) || (defined(__WXGTK__) && defined(PERL510_OR_LATER))
    // load Perl lib at runtime
    #define USE_PERL_DYNAMIC
#endif

#ifdef USE_PERL_DYNAMIC

// declare G_* wrappers for the functions we want to use from Perl lib
extern "C"
{
#ifdef USE_ITHREADS
#ifdef PERL_THR_KEY_FUNC
    perl_key*(*G_Perl_Gthr_key_ptr)(register PerlInterpreter*);
#else
    perl_key *G_PL_thr_key;
#endif
#endif
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
    IV(*G_Perl_sv_2iv_flags)(pTHX_ SV* sv, I32 flags);
#endif
#ifdef PERL510_OR_LATER
    void(*G_Perl_sys_init3)(int*, char***, char***);
    void(*G_Perl_sys_term)(void);
#endif
#ifdef PERL5101_OR_LATER
    SV*(*G_Perl_newSV_type)(pTHX_ svtype type);
#endif
    void(*G_boot_DynaLoader)(pTHX_ CV*);
    
#ifdef MULTIPLICITY
#ifdef PERL510_OR_LATER
    SV***(*G_Perl_Istack_sp_ptr)(register PerlInterpreter*);
    SV***(*G_Perl_Istack_base_ptr)(register PerlInterpreter*);
    SV***(*G_Perl_Istack_max_ptr)(register PerlInterpreter*);
    I32**(*G_Perl_Imarkstack_ptr_ptr)(register PerlInterpreter*);
#else
    SV***(*G_Perl_Tstack_sp_ptr)(register PerlInterpreter*);
    SV***(*G_Perl_Tstack_base_ptr)(register PerlInterpreter*);
    SV***(*G_Perl_Tstack_max_ptr)(register PerlInterpreter*);
    I32**(*G_Perl_Tmarkstack_ptr_ptr)(register PerlInterpreter*);
#endif
    U8*(*G_Perl_Iexit_flags_ptr)(register PerlInterpreter*);
    signed char *(*G_Perl_Iperl_destruct_level_ptr)(register PerlInterpreter*);
#else
    SV ***G_PL_stack_sp;
    SV ***G_PL_stack_base;
    SV ***G_PL_stack_max;
    I32 **G_PL_markstack_ptr;
    U8 *G_PL_exit_flags;
    signed char *G_PL_perl_destruct_level;
#endif
}

// redefine Perl functions to their equivalent G_* wrappers
#ifdef USE_ITHREADS
#ifdef PERL_THR_KEY_FUNC
#define Perl_Gthr_key_ptr        G_Perl_Gthr_key_ptr
#else
#define PL_thr_key               (*G_PL_thr_key)
#endif
#endif
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
#ifdef PERL510_OR_LATER
#define Perl_sys_init3           G_Perl_sys_init3
#define Perl_sys_term            G_Perl_sys_term
#endif
#ifdef MULTIPLICITY
#ifdef PERL510_OR_LATER
#define Perl_Imarkstack_ptr_ptr  G_Perl_Imarkstack_ptr_ptr
#define Perl_Istack_base_ptr     G_Perl_Istack_base_ptr
#define Perl_Istack_max_ptr      G_Perl_Istack_max_ptr
#define Perl_Istack_sp_ptr       G_Perl_Istack_sp_ptr
#else
#define Perl_Tmarkstack_ptr_ptr  G_Perl_Tmarkstack_ptr_ptr
#define Perl_Tstack_base_ptr     G_Perl_Tstack_base_ptr
#define Perl_Tstack_max_ptr      G_Perl_Tstack_max_ptr
#define Perl_Tstack_sp_ptr       G_Perl_Tstack_sp_ptr
#endif
#define Perl_Iexit_flags_ptr          G_Perl_Iexit_flags_ptr
#define Perl_Iperl_destruct_level_ptr G_Perl_Iperl_destruct_level_ptr
#else  /* no MULTIPLICITY */
#define PL_stack_sp               (*G_PL_stack_sp)
#define PL_stack_base             (*G_PL_stack_base)
#define PL_stack_max              (*G_PL_stack_max)
#define PL_markstack_ptr          (*G_PL_markstack_ptr)
#define PL_exit_flags             (*G_PL_exit_flags)
#define PL_perl_destruct_level    (*G_PL_perl_destruct_level)
#endif
#ifdef PERL5101_OR_LATER
#define Perl_newSV_type          G_Perl_newSV_type
#endif
#define boot_DynaLoader          G_boot_DynaLoader

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
#ifdef USE_ITHREADS
#ifdef PERL_THR_KEY_FUNC
    PERL_FUNC(Perl_Gthr_key_ptr)
#else
    PERL_FUNC(PL_thr_key)
#endif
#endif
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
#ifdef PERL510_OR_LATER
    PERL_FUNC(Perl_sys_init3)
    PERL_FUNC(Perl_sys_term)
#endif
#ifdef MULTIPLICITY
#ifndef PERL514_OR_LATER
    // before Perl 5.14:
    PERL_FUNC(Perl_Iexit_flags_ptr)
    PERL_FUNC(Perl_Iperl_destruct_level_ptr)
#ifdef PERL510_OR_LATER
    // Perl 5.10/5.12 only:
    PERL_FUNC(Perl_Imarkstack_ptr_ptr)
    PERL_FUNC(Perl_Istack_base_ptr)
    PERL_FUNC(Perl_Istack_max_ptr)
    PERL_FUNC(Perl_Istack_sp_ptr)
#else
    // before Perl 5.10:
    PERL_FUNC(Perl_Tmarkstack_ptr_ptr)
    PERL_FUNC(Perl_Tstack_base_ptr)
    PERL_FUNC(Perl_Tstack_max_ptr)
    PERL_FUNC(Perl_Tstack_sp_ptr)
#endif
#endif
#else  /* no MULTIPLICITY */
    /* N.B. these are actually variables, not functions, but the distinction does
       not matter for symbol resolution: */
    PERL_FUNC(PL_stack_sp)
    PERL_FUNC(PL_stack_base)
    PERL_FUNC(PL_stack_max)
    PERL_FUNC(PL_markstack_ptr)
    PERL_FUNC(PL_exit_flags)
    PERL_FUNC(PL_perl_destruct_level)
#endif
#ifdef PERL5101_OR_LATER
    PERL_FUNC(Perl_newSV_type)
#endif
    PERL_FUNC(boot_DynaLoader)
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
    
    // wxDL_GLOBAL corresponds to RTLD_GLOBAL on Linux (ignored on Windows)
    while ( !dynlib.Load(perllib, wxDL_NOW | wxDL_VERBATIM | wxDL_GLOBAL) ) {
        // prompt user for a different Perl library;
        // on Windows perllib should be something like "perl510.dll"
        // and on Linux it should be something like "libperl.so.5.10"
        Beep();
        wxString str = _("If Perl isn't installed then you'll have to Cancel,");
        str +=         _("\notherwise change the version numbers to match the");
        str +=         _("\nversion installed on your system and try again.");
#ifdef __WXMSW__
        str +=         _("\n\nIf that fails, search your system for a perl*.dll");
        str +=         _("\nfile and enter the full path to that file.");
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
        void* funcptr;
        PerlFunc* pf = perlFuncs;
        while ( pf->name[0] ) {
            funcptr = dynlib.GetSymbol(pf->name);
            if ( !funcptr ) {
                wxString err = _("The Perl library does not have this symbol:\n");
                err         += pf->name;
                err         += _("\nYou need to install Perl ");
#ifdef PERL510_OR_LATER
                err         += _("5.10 or later.");
#else
                err         += _("5.8.x.");
#endif
                Warning(err);
                return false;
            }
            *(pf++->ptr) = (PERL_PROC)funcptr;
        }
        perldll = dynlib.Detach();
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

#define PERL_ERROR(msg) { Perl_croak(aTHX_ "%s", msg); }

#define CheckRGB(r,g,b,cmd)                                             \
    if (r < 0 || r > 255 || g < 0 || g > 255 || g < 0 || g > 255) {     \
        char msg[128];                                                  \
        sprintf(msg, "Bad rgb value in %s (%d,%d,%d).", cmd, r, g, b);  \
        PERL_ERROR(msg);                                                \
    }

#ifdef __WXMSW__
    #define IGNORE_UNUSED_PARAMS wxUnusedVar(cv); wxUnusedVar(my_perl);
#else
    #define IGNORE_UNUSED_PARAMS
#endif

#ifdef __WXMAC__
    // use decomposed UTF8 so fopen will work
    #define FILENAME wxString(filename,wxConvLocal).fn_str()
#else
    #define FILENAME filename
#endif

// -----------------------------------------------------------------------------

bool PerlScriptAborted()
{
    if (allowcheck) wxGetApp().Poller()->checkevents();
    
    // if user hit escape key then PassKeyToScript has called AbortPerlScript
    
    return !scripterr.IsEmpty();
}

// -----------------------------------------------------------------------------

static void AddPadding(AV* array)
{
    // assume array is multi-state and add an extra int if necessary so the array
    // has an odd number of ints (this is how we distinguish multi-state arrays
    // from one-state arrays -- the latter always have an even number of ints)
    int len = av_len(array) + 1;
    if (len == 0) return;         // always return () rather than (0)
    if ((len & 1) == 0) {
        av_push(array, newSViv(0));
    }
}

// -----------------------------------------------------------------------------

static const char* ExtractCellArray(AV* outarray, lifealgo* universe, bool shift = false)
{
    // extract cell array from given universe
    if ( !universe->isEmpty() ) {
        bigint top, left, bottom, right;
        universe->findedges(&top, &left, &bottom, &right);
        if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
            return "Universe is too big to extract all cells!";
        }
        bool multistate = universe->NumCellStates() > 2;
        int itop = top.toint();
        int ileft = left.toint();
        int ibottom = bottom.toint();
        int iright = right.toint();
        int cx, cy;
        int v = 0;
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
                    if (multistate) av_push(outarray, newSViv(v));
                } else {
                    cx = iright;  // done this row
                }
                cntr++;
                if ((cntr % 4096) == 0 && PerlScriptAborted()) return NULL;
            }
        }
        if (multistate) AddPadding(outarray);
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
    if (items < 1 || items > 2) PERL_ERROR("Usage: g_open($filename,$remember=0).");
    
    STRLEN n_a;
    const char* filename = SvPV(ST(0), n_a);
    int remember = 0;
    if (items > 1) remember = SvIV(ST(1));
    
    const char* err = GSF_open(wxString(filename,wxConvLocal), remember);
    if (err) PERL_ERROR(err);
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_save)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items < 2 || items > 3) PERL_ERROR("Usage: g_save($filename,$format,$remember=0).");
    
    STRLEN n_a;
    const char* filename = SvPV(ST(0), n_a);
    const char* format = SvPV(ST(1), n_a);
    int remember = 0;
    if (items > 2) remember = SvIV(ST(2));
    
    const char* err = GSF_save(wxString(filename,wxConvLocal), format, remember);
    if (err) PERL_ERROR(err);
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------
XS(pl_opendialog)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items > 5) PERL_ERROR("Usage: g_opendialog($title, $filetypes,"
                              "$initialdir, $initialfname, $mustexist=1).");
    
    const char* title = "Choose a file";
    const char* filetypes = "All files (*)|*";
    const char* initialdir = "";
    const char* initialfname = "";
    int mustexist = 1;
    STRLEN n_a;
    if (items > 0) title = SvPV(ST(0), n_a);
    if (items > 1) filetypes = SvPV(ST(1), n_a);
    if (items > 2) initialdir = SvPV(ST(2), n_a);
    if (items > 3) initialfname = SvPV(ST(3), n_a);
    if (items > 4) mustexist = SvIV(ST(4));
    
    wxString wxs_title(title, wxConvLocal);
    wxString wxs_filetypes(filetypes, wxConvLocal);
    wxString wxs_initialdir(initialdir, wxConvLocal);
    wxString wxs_initialfname(initialfname, wxConvLocal);
    wxString wxs_result = wxEmptyString;
    
    if (wxs_initialdir.IsEmpty()) wxs_initialdir = wxFileName::GetCwd();
    
    if (wxs_filetypes == wxT("dir")) {
        // let user choose a directory
        wxDirDialog dirdlg(NULL, wxs_title, wxs_initialdir, wxDD_NEW_DIR_BUTTON);
        if (dirdlg.ShowModal() == wxID_OK) {
            wxs_result = dirdlg.GetPath();
            if (wxs_result.Last() != wxFILE_SEP_PATH) wxs_result += wxFILE_SEP_PATH;
        }
    } else {
        // let user choose a file
        wxFileDialog opendlg(NULL, wxs_title, wxs_initialdir, wxs_initialfname, wxs_filetypes,
                             wxFD_OPEN | (mustexist == 0 ? 0 : wxFD_FILE_MUST_EXIST) );
        if (opendlg.ShowModal() == wxID_OK) wxs_result = opendlg.GetPath();
    }
    
    XSRETURN_PV((const char*)wxs_result.mb_str(wxConvLocal));
}

// -----------------------------------------------------------------------------

XS(pl_savedialog)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items > 5) PERL_ERROR("Usage: g_savedialog($title, $filetypes,"
                              " $initialdir, $initialfname, $suppressprompt=0).");
    
    const char* title = "Choose a save location and filename";
    const char* filetypes = "All files (*)|*";
    const char* initialdir = "";
    const char* initialfname = "";
    STRLEN n_a;
    if (items > 0) title = SvPV(ST(0), n_a);
    if (items > 1) filetypes = SvPV(ST(1), n_a);
    if (items > 2) initialdir = SvPV(ST(2), n_a);
    if (items > 3) initialfname = SvPV(ST(3), n_a);
    int suppressprompt = 0;
    if (items > 4) suppressprompt = SvIV(ST(4));
    
    wxString wxs_title(title, wxConvLocal);
    wxString wxs_filetypes(filetypes, wxConvLocal);
    wxString wxs_initialdir(initialdir, wxConvLocal);
    wxString wxs_initialfname(initialfname, wxConvLocal);
    
    if (wxs_initialdir.IsEmpty()) wxs_initialdir = wxFileName::GetCwd();
    
    // suppress Overwrite? popup if user just wants to retrieve the string
    wxFileDialog savedlg( NULL, wxs_title, wxs_initialdir, wxs_initialfname, wxs_filetypes,
                         wxFD_SAVE | (suppressprompt == 0 ? wxFD_OVERWRITE_PROMPT : 0) );
    
    wxString wxs_savefname = wxEmptyString;
    if ( savedlg.ShowModal() == wxID_OK ) wxs_savefname = savedlg.GetPath();
    
    XSRETURN_PV((const char*)wxs_savefname.mb_str(wxConvLocal));
}

// -----------------------------------------------------------------------------

XS(pl_load)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 1) PERL_ERROR("Usage: $cells = g_load($filename).");
    
    STRLEN n_a;
    const char* filename = SvPV(ST(0), n_a);
    
    // create temporary universe of same type as current universe
    lifealgo* tempalgo = CreateNewUniverse(currlayer->algtype, allowcheck);
    // readpattern will call setrule
    
    // read pattern into temporary universe
    const char* err = readpattern(FILENAME, *tempalgo);
    if (err) {
        // try all other algos until readpattern succeeds
        for (int i = 0; i < NumAlgos(); i++) {
            if (i != currlayer->algtype) {
                delete tempalgo;
                tempalgo = CreateNewUniverse(i, allowcheck);
                err = readpattern(FILENAME, *tempalgo);
                if (!err) break;
            }
        }
    }
    
    if (err) {
        delete tempalgo;
        PERL_ERROR(err);
    }
    
    // convert pattern into a cell array, shifting cell coords so that the
    // bounding box's top left cell is at 0,0
    AV* outarray = (AV*)sv_2mortal( (SV*)newAV() );
    err = ExtractCellArray(outarray, tempalgo, true);
    delete tempalgo;
    if (err) PERL_ERROR(err);
    
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
    if (items != 2) PERL_ERROR("Usage: g_store($cells,$filename).");
    
    SV* cells = ST(0);
    if ( (!SvROK(cells)) || (SvTYPE(SvRV(cells)) != SVt_PVAV) ) {
        PERL_ERROR("g_store error: 1st parameter is not a valid array reference.");
    }
    AV* inarray = (AV*)SvRV(cells);
    
    STRLEN n_a;
    const char* filename = SvPV(ST(1), n_a);
    
    // create temporary universe of same type as current universe
    lifealgo* tempalgo = CreateNewUniverse(currlayer->algtype, allowcheck);
    const char* err = tempalgo->setrule(currlayer->algo->getrule());
    if (err) tempalgo->setrule(tempalgo->DefaultRule());
    
    // copy cell array into temporary universe
    bool multistate = ((av_len(inarray) + 1) & 1) == 1;
    int ints_per_cell = multistate ? 3 : 2;
    int num_cells = (av_len(inarray) + 1) / ints_per_cell;
    for (int n = 0; n < num_cells; n++) {
        int item = ints_per_cell * n;
        int x = SvIV( *av_fetch(inarray, item, 0) );
        int y = SvIV( *av_fetch(inarray, item + 1, 0) );
        // check if x,y is outside bounded grid
        const char* err = GSF_checkpos(tempalgo, x, y);
        if (err) { delete tempalgo; PERL_ERROR(err); }
        if (multistate) {
            int state = SvIV( *av_fetch(inarray, item + 2, 0) );
            if (tempalgo->setcell(x, y, state) < 0) {
                tempalgo->endofpattern();
                delete tempalgo;
                PERL_ERROR("g_store error: state value is out of range.");
            }
        } else {
            tempalgo->setcell(x, y, 1);
        }
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
    pattern_format format = savexrle ? XRLE_format : RLE_format;
    // if grid is bounded then force XRLE_format so that position info is recorded
    if (tempalgo->gridwd > 0 || tempalgo->gridht > 0) format = XRLE_format;
    err = writepattern(FILENAME, *tempalgo, format, no_compression,
                       top.toint(), left.toint(), bottom.toint(), right.toint());
    delete tempalgo;
    if (err) PERL_ERROR(err);
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

// deprecated (use pl_getdir)
XS(pl_appdir)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 0) PERL_ERROR("Usage: $dir = g_appdir().");
    
    XSRETURN_PV((const char*)gollydir.mb_str(wxConvLocal));
}

// -----------------------------------------------------------------------------

// deprecated (use pl_getdir)
XS(pl_datadir)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 0) PERL_ERROR("Usage: $dir = g_datadir().");
    
    XSRETURN_PV((const char*)datadir.mb_str(wxConvLocal));
}

// -----------------------------------------------------------------------------

XS(pl_setdir)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 2) PERL_ERROR("Usage: g_setdir($dirname,$newdir).");
    
    STRLEN n_a;
    const char* dirname = SvPV(ST(0), n_a);
    const char* newdir = SvPV(ST(1), n_a);
    
    const char* err = GSF_setdir(dirname, wxString(newdir,wxConvLocal));
    if (err) PERL_ERROR(err);
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_getdir)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 1) PERL_ERROR("Usage: $dir = g_getdir($dirname).");
    
    STRLEN n_a;
    const char* dirname = SvPV(ST(0), n_a);
    
    const char* dirstring = GSF_getdir(dirname);
    if (dirstring == NULL) PERL_ERROR("g_getdir error: unknown directory name.");
    
    XSRETURN_PV(dirstring);
}

// -----------------------------------------------------------------------------

XS(pl_new)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 1) PERL_ERROR("Usage: g_new($title).");
    
    STRLEN n_a;
    const char* title = SvPV(ST(0), n_a);
    
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
    if (items != 0) PERL_ERROR("Usage: g_cut().");
    
    if (viewptr->SelectionExists()) {
        viewptr->CutSelection();
        DoAutoUpdate();
    } else {
        PERL_ERROR("g_cut error: no selection.");
    }
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_copy)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 0) PERL_ERROR("Usage: g_copy().");
    
    if (viewptr->SelectionExists()) {
        viewptr->CopySelection();
        DoAutoUpdate();
    } else {
        PERL_ERROR("g_copy error: no selection.");
    }
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_clear)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 1) PERL_ERROR("Usage: g_clear($where).");
    
    int where = SvIV(ST(0));
    
    if (viewptr->SelectionExists()) {
        if (where == 0)
            viewptr->ClearSelection();
        else
            viewptr->ClearOutsideSelection();
        DoAutoUpdate();
    } else {
        PERL_ERROR("g_clear error: no selection.");
    }
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_paste)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 3) PERL_ERROR("Usage: g_paste($x,$y,$mode).");
    
    int x = SvIV(ST(0));
    int y = SvIV(ST(1));
    
    STRLEN n_a;
    const char* mode = SvPV(ST(2), n_a);
    
    const char* err = GSF_paste(x, y, mode);
    if (err) PERL_ERROR(err);
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_shrink)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 0) PERL_ERROR("Usage: g_shrink().");
    
    if (viewptr->SelectionExists()) {
        viewptr->ShrinkSelection(false);    // false == don't fit in viewport
        DoAutoUpdate();
    } else {
        PERL_ERROR("g_shrink error: no selection.");
    }
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_randfill)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 1) PERL_ERROR("Usage: g_randfill($percentage).");
    
    int perc = SvIV(ST(0));
    
    if (perc < 1 || perc > 100) {
        PERL_ERROR("g_randfill error: percentage must be from 1 to 100.");
    }
    
    if (viewptr->SelectionExists()) {
        int oldperc = randomfill;
        randomfill = perc;
        viewptr->RandomFill();
        randomfill = oldperc;
        DoAutoUpdate();
    } else {
        PERL_ERROR("g_randfill error: no selection.");
    }
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_flip)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 1) PERL_ERROR("Usage: g_flip($direction).");
    
    int direction = SvIV(ST(0));
    
    if (viewptr->SelectionExists()) {
        viewptr->FlipSelection(direction != 0);    // 1 = top-bottom
        DoAutoUpdate();
    } else {
        PERL_ERROR("g_flip error: no selection.");
    }
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_rotate)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 1) PERL_ERROR("Usage: g_rotate($direction).");
    
    int direction = SvIV(ST(0));
    
    if (viewptr->SelectionExists()) {
        viewptr->RotateSelection(direction == 0);    // 0 = clockwise
        DoAutoUpdate();
    } else {
        PERL_ERROR("g_rotate error: no selection.");
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
        PERL_ERROR("Usage: $outcells = g_parse($string,$x=0,$y=0,$axx=1,$axy=0,$ayx=0,$ayy=1).");
    
    STRLEN n_a;
    const char* s = SvPV(ST(0), n_a);
    
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
        // parsing RLE format; first check if multi-state data is present
        bool multistate = false;
        const char* p = s;
        while (*p) {
            char c = *p++;
            if ((c == '.') || ('p' <= c && c <= 'y') || ('A' <= c && c <= 'X')) {
                multistate = true;
                break;
            }
        }
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
                    case '.': x += prefix; break;
                    case 'o':
                        for (int k = 0; k < prefix; k++, x++) {
                            av_push(outarray, newSViv(x0 + x * axx + y * axy));
                            av_push(outarray, newSViv(y0 + x * ayx + y * ayy));
                            if (multistate) av_push(outarray, newSViv(1));
                        }
                        break;
                    default:
                        if (('p' <= c && c <= 'y') || ('A' <= c && c <= 'X')) {
                            // multistate must be true
                            int state;
                            if (c < 'p') {
                                state = c - 'A' + 1;
                            } else {
                                state = 24 * (c - 'p' + 1);
                                c = *s++;
                                if ('A' <= c && c <= 'X') {
                                    state = state + c - 'A' + 1;
                                } else {
                                    // PERL_ERROR("g_parse error: illegal multi-char state.");
                                    // be more forgiving and treat 'p'..'y' like 'o'
                                    state = 1;
                                    s--;
                                }
                            }
                            for (int k = 0; k < prefix; k++, x++) {
                                av_push(outarray, newSViv(x0 + x * axx + y * axy));
                                av_push(outarray, newSViv(y0 + x * ayx + y * ayy));
                                av_push(outarray, newSViv(state));
                            }
                        }
                }
                prefix = 0;
            }
            c = *s++;
        }
        if (multistate) AddPadding(outarray);
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
        PERL_ERROR("Usage: $outcells = g_transform($cells,$x,$y,$axx=1,$axy=0,$ayx=0,$ayy=1).");
    
    SV* cells = ST(0);
    if ( (!SvROK(cells)) || (SvTYPE(SvRV(cells)) != SVt_PVAV) ) {
        PERL_ERROR("g_transform error: 1st parameter is not a valid array reference.");
    }
    AV* inarray = (AV*)SvRV(cells);
    
    int x0 = SvIV(ST(1));
    int y0 = SvIV(ST(2));
    
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
    
    bool multistate = ((av_len(inarray) + 1) & 1) == 1;
    int ints_per_cell = multistate ? 3 : 2;
    int num_cells = (av_len(inarray) + 1) / ints_per_cell;
    for (int n = 0; n < num_cells; n++) {
        int item = ints_per_cell * n;
        int x = SvIV( *av_fetch(inarray, item, 0) );
        int y = SvIV( *av_fetch(inarray, item + 1, 0) );
        av_push(outarray, newSViv(x0 + x * axx + y * axy));
        av_push(outarray, newSViv(y0 + x * ayx + y * ayy));
        if (multistate) {
            int state = SvIV( *av_fetch(inarray, item + 2, 0) );
            av_push(outarray, newSViv(state));
        }
        if ((n % 4096) == 0 && PerlScriptAborted()) break;
    }
    if (multistate) AddPadding(outarray);
    
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
    if (items != 2) PERL_ERROR("Usage: $outcells = g_evolve($cells,$numgens).");
    
    SV* cells = ST(0);
    if ( (!SvROK(cells)) || (SvTYPE(SvRV(cells)) != SVt_PVAV) ) {
        PERL_ERROR("g_evolve error: 1st parameter is not a valid array reference.");
    }
    AV* inarray = (AV*)SvRV(cells);
    
    int ngens = SvIV(ST(1));
    
    if (ngens < 0) {
        PERL_ERROR("g_evolve error: number of generations is negative.");
    }
    
    // create a temporary universe of same type as current universe
    lifealgo* tempalgo = CreateNewUniverse(currlayer->algtype, allowcheck);
    const char* err = tempalgo->setrule(currlayer->algo->getrule());
    if (err) tempalgo->setrule(tempalgo->DefaultRule());
    
    // copy cell array into temporary universe
    bool multistate = ((av_len(inarray) + 1) & 1) == 1;
    int ints_per_cell = multistate ? 3 : 2;
    int num_cells = (av_len(inarray) + 1) / ints_per_cell;
    for (int n = 0; n < num_cells; n++) {
        int item = ints_per_cell * n;
        int x = SvIV( *av_fetch(inarray, item, 0) );
        int y = SvIV( *av_fetch(inarray, item + 1, 0) );
        // check if x,y is outside bounded grid
        const char* err = GSF_checkpos(tempalgo, x, y);
        if (err) { delete tempalgo; PERL_ERROR(err); }
        if (multistate) {
            int state = SvIV( *av_fetch(inarray, item + 2, 0) );
            if (tempalgo->setcell(x, y, state) < 0) {
                tempalgo->endofpattern();
                delete tempalgo;
                PERL_ERROR("g_evolve error: state value is out of range.");
            }
        } else {
            tempalgo->setcell(x, y, 1);
        }
        if ((n % 4096) == 0 && PerlScriptAborted()) {
            tempalgo->endofpattern();
            delete tempalgo;
            Perl_croak(aTHX_ NULL);
        }
    }
    tempalgo->endofpattern();
    
    // advance pattern by ngens
    mainptr->generating = true;
    if (tempalgo->unbounded && (tempalgo->gridwd > 0 || tempalgo->gridht > 0)) {
        // a bounded grid must use an increment of 1 so we can call
        // CreateBorderCells and DeleteBorderCells around each step()
        tempalgo->setIncrement(1);
        while (ngens > 0) {
            if (PerlScriptAborted()) {
                mainptr->generating = false;
                delete tempalgo;
                Perl_croak(aTHX_ NULL);
            }
            if (!tempalgo->CreateBorderCells()) break;
            tempalgo->step();
            if (!tempalgo->DeleteBorderCells()) break;
            ngens--;
        }
    } else {
        tempalgo->setIncrement(ngens);
        tempalgo->step();
    }
    mainptr->generating = false;
    
    // convert new pattern into a new cell array
    AV* outarray = (AV*)sv_2mortal( (SV*)newAV() );
    err = ExtractCellArray(outarray, tempalgo);
    delete tempalgo;
    if (err) PERL_ERROR(err);
    
    SP -= items;
    ST(0) = newRV( (SV*)outarray );
    sv_2mortal(ST(0));
    XSRETURN(1);
}

// -----------------------------------------------------------------------------

static const char* BAD_STATE = "g_putcells error: state value is out of range.";

XS(pl_putcells)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items < 1 || items > 8)
        PERL_ERROR("Usage: g_putcells($cells,$x=0,$y=0,$axx=1,$axy=0,$ayx=0,$ayy=1,$mode='or').");
    
    SV* cells = ST(0);
    if ( (!SvROK(cells)) || (SvTYPE(SvRV(cells)) != SVt_PVAV) ) {
        PERL_ERROR("g_putcells error: 1st parameter is not a valid array reference.");
    }
    AV* inarray = (AV*)SvRV(cells);
    
    // default values for optional params
    int x0  = 0;
    int y0  = 0;
    int axx = 1;
    int axy = 0;
    int ayx = 0;
    int ayy = 1;
    // default for mode is 'or'; 'xor' mode is also supported;
    // for a one-state array 'copy' mode currently has the same effect as 'or' mode
    // because there is no bounding box to set dead cells, but a multi-state array can
    // have dead cells so in that case 'copy' mode is not the same as 'or' mode
    const char* mode = "or";
    
    STRLEN n_a;
    if (items > 1) x0  = SvIV(ST(1));
    if (items > 2) y0  = SvIV(ST(2));
    if (items > 3) axx = SvIV(ST(3));
    if (items > 4) axy = SvIV(ST(4));
    if (items > 5) ayx = SvIV(ST(5));
    if (items > 6) ayy = SvIV(ST(6));
    if (items > 7) mode = SvPV(ST(7), n_a);
    
    wxString modestr = wxString(mode, wxConvLocal);
    if ( !(  modestr.IsSameAs(wxT("or"), false)
           || modestr.IsSameAs(wxT("xor"), false)
           || modestr.IsSameAs(wxT("copy"), false)
           || modestr.IsSameAs(wxT("and"), false)
           || modestr.IsSameAs(wxT("not"), false)) ) {
        PERL_ERROR("g_putcells error: unknown mode.");
    }
    
    // save cell changes if undo/redo is enabled and script isn't constructing a pattern
    bool savecells = allowundo && !currlayer->stayclean;
    // use ChangeCell below and combine all changes due to consecutive setcell/putcells
    // if (savecells) SavePendingChanges();
    
    // note that av_len returns max index or -1 if array is empty
    bool multistate = ((av_len(inarray) + 1) & 1) == 1;
    int ints_per_cell = multistate ? 3 : 2;
    int num_cells = (av_len(inarray) + 1) / ints_per_cell;
    const char* err = NULL;
    bool pattchanged = false;
    lifealgo* curralgo = currlayer->algo;
    
    if (modestr.IsSameAs(wxT("copy"), false)) {
        // TODO: find bounds of cell array and call ClearRect here (to be added to wxedit.cpp)
    }
    
    if (modestr.IsSameAs(wxT("and"), false)) {
        if (!curralgo->isEmpty()) {
            int newstate = 1;
            for (int n = 0; n < num_cells; n++) {
                int item = ints_per_cell * n;
                int x = SvIV( *av_fetch(inarray, item, 0) );
                int y = SvIV( *av_fetch(inarray, item + 1, 0) );
                int newx = x0 + x * axx + y * axy;
                int newy = y0 + x * ayx + y * ayy;
                // check if newx,newy is outside bounded grid
                err = GSF_checkpos(curralgo, newx, newy);
                if (err) break;
                int oldstate = curralgo->getcell(newx, newy);
                if (multistate) {
                    // multi-state lists can contain dead cells so newstate might be 0
                    newstate = SvIV( *av_fetch(inarray, item + 2, 0) );
                }
                if (newstate != oldstate && oldstate > 0) {
                    curralgo->setcell(newx, newy, 0);
                    if (savecells) ChangeCell(newx, newy, oldstate, 0);
                    pattchanged = true;
                }
                if ((n % 4096) == 0 && PerlScriptAborted()) break;
            }
        }
    } else if (modestr.IsSameAs(wxT("xor"), false)) {
        // loop code is duplicated here to allow 'or' case to execute faster
        int numstates = curralgo->NumCellStates();
        for (int n = 0; n < num_cells; n++) {
            int item = ints_per_cell * n;
            int x = SvIV( *av_fetch(inarray, item, 0) );
            int y = SvIV( *av_fetch(inarray, item + 1, 0) );
            int newx = x0 + x * axx + y * axy;
            int newy = y0 + x * ayx + y * ayy;
            // check if newx,newy is outside bounded grid
            err = GSF_checkpos(curralgo, newx, newy);
            if (err) break;
            int oldstate = curralgo->getcell(newx, newy);
            int newstate;
            if (multistate) {
                // multi-state arrays can contain dead cells so newstate might be 0
                newstate = SvIV( *av_fetch(inarray, item + 2, 0) );
                if (newstate == oldstate) {
                    if (oldstate != 0) newstate = 0;
                } else {
                    newstate = newstate ^ oldstate;
                    // if xor overflows then don't change current state
                    if (newstate >= numstates) newstate = oldstate;
                }
                if (newstate != oldstate) {
                    // paste (possibly transformed) cell into current universe
                    if (curralgo->setcell(newx, newy, newstate) < 0) {
                        err = BAD_STATE;
                        break;
                    }
                    if (savecells) ChangeCell(newx, newy, oldstate, newstate);
                    pattchanged = true;
                }
            } else {
                // one-state arrays only contain live cells
                newstate = 1 - oldstate;
                // paste (possibly transformed) cell into current universe
                if (curralgo->setcell(newx, newy, newstate) < 0) {
                    err = BAD_STATE;
                    break;
                }
                if (savecells) ChangeCell(newx, newy, oldstate, newstate);
                pattchanged = true;
            }
            if ((n % 4096) == 0 && PerlScriptAborted()) break;
        }
    } else {
        bool negate = modestr.IsSameAs(wxT("not"), false);
        bool ormode = modestr.IsSameAs(wxT("or"), false);
        int newstate = negate ? 0 : 1;
        int maxstate = curralgo->NumCellStates() - 1;
        for (int n = 0; n < num_cells; n++) {
            int item = ints_per_cell * n;
            int x = SvIV( *av_fetch(inarray, item, 0) );
            int y = SvIV( *av_fetch(inarray, item + 1, 0) );
            int newx = x0 + x * axx + y * axy;
            int newy = y0 + x * ayx + y * ayy;
            // check if newx,newy is outside bounded grid
            err = GSF_checkpos(curralgo, newx, newy);
            if (err) break;
            int oldstate = curralgo->getcell(newx, newy);
            if (multistate) {
                // multi-state arrays can contain dead cells so newstate might be 0
                newstate = SvIV( *av_fetch(inarray, item + 2, 0) );
                if (negate) newstate = maxstate - newstate;
                if (ormode && newstate == 0) newstate = oldstate;
            }
            if (newstate != oldstate) {
                // paste (possibly transformed) cell into current universe
                if (curralgo->setcell(newx, newy, newstate) < 0) {
                    err = BAD_STATE;
                    break;
                }
                if (savecells) ChangeCell(newx, newy, oldstate, newstate);
                pattchanged = true;
            }
            if ((n % 4096) == 0 && PerlScriptAborted()) break;
        }
    }
    
    if (pattchanged) {
        curralgo->endofpattern();
        MarkLayerDirty();
        DoAutoUpdate();
    }
    
    if (err) PERL_ERROR(err);
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_getcells)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 0 && items != 4) PERL_ERROR("Usage: $cells = g_getcells(@rect).");
    
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
        const char* err = GSF_checkrect(x, y, wd, ht);
        if (err) PERL_ERROR(err);
        int right = x + wd - 1;
        int bottom = y + ht - 1;
        int cx, cy;
        int v = 0;
        int cntr = 0;
        lifealgo* curralgo = currlayer->algo;
        bool multistate = curralgo->NumCellStates() > 2;
        for ( cy=y; cy<=bottom; cy++ ) {
            for ( cx=x; cx<=right; cx++ ) {
                int skip = curralgo->nextcell(cx, cy, v);
                if (skip >= 0) {
                    // found next live cell in this row so add coords to outarray
                    cx += skip;
                    if (cx <= right) {
                        av_push(outarray, newSViv(cx));
                        av_push(outarray, newSViv(cy));
                        if (multistate) av_push(outarray, newSViv(v));
                    }
                } else {
                    cx = right;  // done this row
                }
                cntr++;
                if ((cntr % 4096) == 0) RETURN_IF_ABORTED;
            }
        }
        if (multistate) AddPadding(outarray);
    }
    
    SP -= items;
    ST(0) = newRV( (SV*)outarray );
    sv_2mortal(ST(0));
    XSRETURN(1);
}

// -----------------------------------------------------------------------------

XS(pl_join)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 2) PERL_ERROR("Usage: $outcells = g_join($cells1,$cells2).");
    
    SV* cells1 = ST(0);
    SV* cells2 = ST(1);
    
    if ( (!SvROK(cells1)) || (SvTYPE(SvRV(cells1)) != SVt_PVAV) ) {
        PERL_ERROR("g_join error: 1st parameter is not a valid array reference.");
    }
    if ( (!SvROK(cells2)) || (SvTYPE(SvRV(cells2)) != SVt_PVAV) ) {
        PERL_ERROR("g_join error: 2nd parameter is not a valid array reference.");
    }
    
    AV* inarray1 = (AV*)SvRV(cells1);
    AV* inarray2 = (AV*)SvRV(cells2);
    
    bool multi1 = ((av_len(inarray1) + 1) & 1) == 1;
    bool multi2 = ((av_len(inarray2) + 1) & 1) == 1;
    bool multiout = multi1 || multi2;
    int ints_per_cell, num_cells;
    int x, y, state;
    AV* outarray = (AV*)sv_2mortal( (SV*)newAV() );
    
    // append 1st array
    ints_per_cell = multi1 ? 3 : 2;
    num_cells = (av_len(inarray1) + 1) / ints_per_cell;
    for (int n = 0; n < num_cells; n++) {
        int item = ints_per_cell * n;
        x = SvIV( *av_fetch(inarray1, item, 0) );
        y = SvIV( *av_fetch(inarray1, item + 1, 0) );
        if (multi1) {
            state = SvIV( *av_fetch(inarray1, item + 2, 0) );
        } else {
            state = 1;
        }
        av_push(outarray, newSViv(x));
        av_push(outarray, newSViv(y));
        if (multiout) av_push(outarray, newSViv(state));
        if ((n % 4096) == 0 && PerlScriptAborted()) {
            Perl_croak(aTHX_ NULL);
        }
    }
    
    // append 2nd array
    ints_per_cell = multi2 ? 3 : 2;
    num_cells = (av_len(inarray2) + 1) / ints_per_cell;
    for (int n = 0; n < num_cells; n++) {
        int item = ints_per_cell * n;
        x = SvIV( *av_fetch(inarray2, item, 0) );
        y = SvIV( *av_fetch(inarray2, item + 1, 0) );
        if (multi2) {
            state = SvIV( *av_fetch(inarray2, item + 2, 0) );
        } else {
            state = 1;
        }
        av_push(outarray, newSViv(x));
        av_push(outarray, newSViv(y));
        if (multiout) av_push(outarray, newSViv(state));
        if ((n % 4096) == 0 && PerlScriptAborted()) {
            Perl_croak(aTHX_ NULL);
        }
    }
    
    if (multiout) AddPadding(outarray);
    
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
    if (items != 4) PERL_ERROR("Usage: $int = g_hash(@rect).");
    
    int x  = SvIV(ST(0));
    int y  = SvIV(ST(1));
    int wd = SvIV(ST(2));
    int ht = SvIV(ST(3));
    const char* err = GSF_checkrect(x, y, wd, ht);
    if (err) PERL_ERROR(err);
    
    int hash = GSF_hash(x, y, wd, ht);
    
    XSRETURN_IV(hash);
}

// -----------------------------------------------------------------------------

XS(pl_getclip)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 0) PERL_ERROR("Usage: $cells = g_getclip().");
    
    if (!mainptr->ClipboardHasText()) {
        PERL_ERROR("g_getclip error: no pattern in clipboard.");
    }
    
    // convert pattern in clipboard into a cell array, but where the first 2 items
    // are the pattern's width and height (not necessarily the minimal bounding box
    // because the pattern might have empty borders, or it might even be empty)
    AV* outarray = (AV*)sv_2mortal( (SV*)newAV() );
    
    // create a temporary layer for storing the clipboard pattern
    Layer* templayer = CreateTemporaryLayer();
    if (!templayer) {
        PERL_ERROR("g_getclip error: failed to create temporary layer.");
    }
    
    // read clipboard pattern into temporary universe and set edges
    // (not a minimal bounding box if pattern is empty or has empty borders)
    bigint top, left, bottom, right;
    if ( viewptr->GetClipboardPattern(templayer, &top, &left, &bottom, &right) ) {
        if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
            delete templayer;
            PERL_ERROR("g_getclip error: pattern is too big.");
        }
        int itop = top.toint();
        int ileft = left.toint();
        int ibottom = bottom.toint();
        int iright = right.toint();
        int wd = iright - ileft + 1;
        int ht = ibottom - itop + 1;
        
        av_push(outarray, newSViv(wd));
        av_push(outarray, newSViv(ht));
        
        // extract cells from templayer
        lifealgo* tempalgo = templayer->algo;
        bool multistate = tempalgo->NumCellStates() > 2;
        int cx, cy;
        int cntr = 0;
        int v = 0;
        for ( cy=itop; cy<=ibottom; cy++ ) {
            for ( cx=ileft; cx<=iright; cx++ ) {
                int skip = tempalgo->nextcell(cx, cy, v);
                if (skip >= 0) {
                    // found next live cell in this row
                    cx += skip;
                    // shift cells so that top left cell of bounding box is at 0,0
                    av_push(outarray, newSViv(cx - ileft));
                    av_push(outarray, newSViv(cy - itop));
                    if (multistate) av_push(outarray, newSViv(v));
                } else {
                    cx = iright;  // done this row
                }
                cntr++;
                if ((cntr % 4096) == 0 && PerlScriptAborted()) {
                    delete templayer;
                    Perl_croak(aTHX_ NULL);
                }
            }
        }
        // if no live cells then return (wd,ht) rather than (wd,ht,0)
        if (multistate && (av_len(outarray) + 1) > 2) {
            AddPadding(outarray);
        }
        
        delete templayer;
    } else {
        // assume error message has been displayed
        delete templayer;
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
    if (items != 0 && items != 4) PERL_ERROR("Usage: g_select(@rect).");
    
    if (items == 0) {
        // remove any existing selection
        GSF_select(0, 0, 0, 0);
    } else {
        // items == 4
        int x  = SvIV(ST(0));
        int y  = SvIV(ST(1));
        int wd = SvIV(ST(2));
        int ht = SvIV(ST(3));
        const char* err = GSF_checkrect(x, y, wd, ht);
        if (err) PERL_ERROR(err);
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
    if (items != 0) PERL_ERROR("Usage: @rect = g_getrect().");
    
    if (!currlayer->algo->isEmpty()) {
        bigint top, left, bottom, right;
        currlayer->algo->findedges(&top, &left, &bottom, &right);
        if ( viewptr->OutsideLimits(top, left, bottom, right) ) {
            PERL_ERROR("g_getrect error: pattern is too big.");
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
    if (items != 0) PERL_ERROR("Usage: @rect = g_getselrect().");
    
    if (viewptr->SelectionExists()) {
        if (currlayer->currsel.TooBig()) {
            PERL_ERROR("g_getselrect error: selection is too big.");
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
    if (items != 3) PERL_ERROR("Usage: g_setcell($x,$y,$state).");
    
    int x = SvIV(ST(0));
    int y = SvIV(ST(1));
    int state = SvIV(ST(2));
    
    const char* err = GSF_setcell(x, y, state);
    if (err) PERL_ERROR(err);
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_getcell)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 2) PERL_ERROR("Usage: $state = g_getcell($x,$y).");
    
    int x = SvIV(ST(0));
    int y = SvIV(ST(1));
    
    // check if x,y is outside bounded grid
    const char* err = GSF_checkpos(currlayer->algo, x, y);
    if (err) PERL_ERROR(err);
    
    int state = currlayer->algo->getcell(x, y);
    
    XSRETURN_IV(state);
}

// -----------------------------------------------------------------------------

XS(pl_setcursor)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 1) PERL_ERROR("Usage: $oldcurs = g_setcursor($newcurs).");
    
    STRLEN n_a;
    const char* newcursor = SvPV(ST(0), n_a);
    const char* oldcursor = CursorToString(currlayer->curs);
    wxCursor* cursptr = StringToCursor(newcursor);
    if (cursptr) {
        viewptr->SetCursorMode(cursptr);
        // see the cursor change, including button in edit bar
        mainptr->UpdateUserInterface();
    } else {
        PERL_ERROR("g_setcursor error: unknown cursor string.");
    }
    
    // return old cursor (simplifies saving and restoring cursor)
    XSRETURN_PV(oldcursor);
}

// -----------------------------------------------------------------------------

XS(pl_getcursor)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 0) PERL_ERROR("Usage: $string = g_getcursor().");
    
    XSRETURN_PV(CursorToString(currlayer->curs));
}

// -----------------------------------------------------------------------------

XS(pl_empty)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 0) PERL_ERROR("Usage: $bool = g_empty().");
    
    XSRETURN_IV(currlayer->algo->isEmpty() ? 1 : 0);
}

// -----------------------------------------------------------------------------

XS(pl_run)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 1) PERL_ERROR("Usage: g_run($numgens).");
    
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
    if (items != 0) PERL_ERROR("Usage: g_step().");
    
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
    if (items != 1) PERL_ERROR("Usage: g_setstep($int).");
    
    mainptr->SetStepExponent(SvIV(ST(0)));
    DoAutoUpdate();
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_getstep)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 0) PERL_ERROR("Usage: $int = g_getstep().");
    
    XSRETURN_IV(currlayer->currexpo);
}

// -----------------------------------------------------------------------------

XS(pl_setbase)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 1) PERL_ERROR("Usage: g_setbase($int).");
    
    int base = SvIV(ST(0));
    
    if (base < 2) base = 2;
    if (base > MAX_BASESTEP) base = MAX_BASESTEP;
    currlayer->currbase = base;
    mainptr->SetGenIncrement();
    DoAutoUpdate();
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_getbase)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 0) PERL_ERROR("Usage: $int = g_getbase().");
    
    XSRETURN_IV(currlayer->currbase);
}

// -----------------------------------------------------------------------------

XS(pl_advance)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 2) PERL_ERROR("Usage: g_advance($where,$numgens).");
    
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
            PERL_ERROR("g_advance error: no selection.");
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
    if (items != 0) PERL_ERROR("Usage: g_reset().");
    
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
    if (items != 1) PERL_ERROR("Usage: g_setgen($string).");
    
    STRLEN n_a;
    const char* genstring = SvPV(ST(0), n_a);
    
    const char* err = GSF_setgen(genstring);
    if (err) PERL_ERROR(err);
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_getgen)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items > 1) PERL_ERROR("Usage: $string = g_getgen($sepchar='').");
    
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
    if (items > 1) PERL_ERROR("Usage: $string = g_getpop($sepchar='').");
    
    char sepchar = '\0';
    if (items > 0) {
        STRLEN n_a;
        char* s = SvPV(ST(0), n_a);
        sepchar = s[0];
    }
    
    XSRETURN_PV(currlayer->algo->getPopulation().tostring(sepchar));
}

// -----------------------------------------------------------------------------

XS(pl_setalgo)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 1) PERL_ERROR("Usage: g_setalgo($string).");
    
    STRLEN n_a;
    const char* algostring = SvPV(ST(0), n_a);
    
    const char* err = GSF_setalgo(algostring);
    if (err) PERL_ERROR(err);
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_getalgo)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items > 1) PERL_ERROR("Usage: $algo = g_getalgo($index=current).");
    
    int index = currlayer->algtype;
    if (items > 0) index = SvIV(ST(0));
    
    if (index < 0 || index >= NumAlgos()) {
        char msg[64];
        sprintf(msg, "Bad g_getalgo index (%d).", index);
        PERL_ERROR(msg);
    }
    
    XSRETURN_PV(GetAlgoName(index));
}

// -----------------------------------------------------------------------------

XS(pl_setrule)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 1) PERL_ERROR("Usage: g_setrule($string).");
    
    STRLEN n_a;
    const char* rulestring = SvPV(ST(0), n_a);
    
    const char* err = GSF_setrule(rulestring);
    if (err) PERL_ERROR(err);
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_getrule)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 0) PERL_ERROR("Usage: $string = g_getrule().");
    
    XSRETURN_PV(currlayer->algo->getrule());
}

// -----------------------------------------------------------------------------

XS(pl_getwidth)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 0) PERL_ERROR("Usage: $int = g_getwidth().");
    
    XSRETURN_IV(currlayer->algo->gridwd);
}

// -----------------------------------------------------------------------------

XS(pl_getheight)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 0) PERL_ERROR("Usage: $int = g_getheight().");
    
    XSRETURN_IV(currlayer->algo->gridht);
}

// -----------------------------------------------------------------------------

XS(pl_numstates)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 0) PERL_ERROR("Usage: $int = g_numstates().");
    
    XSRETURN_IV(currlayer->algo->NumCellStates());
}

// -----------------------------------------------------------------------------

XS(pl_numalgos)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 0) PERL_ERROR("Usage: $int = g_numalgos().");
    
    XSRETURN_IV(NumAlgos());
}

// -----------------------------------------------------------------------------

XS(pl_setpos)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 2) PERL_ERROR("Usage: g_setpos($xstring,$ystring).");
    
    STRLEN n_a;
    const char* x = SvPV(ST(0), n_a);
    const char* y = SvPV(ST(1), n_a);
    
    const char* err = GSF_setpos(x, y);
    if (err) PERL_ERROR(err);
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_getpos)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items > 1) PERL_ERROR("Usage: @xy = g_getpos($sepchar='').");
    
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
    if (items != 1) PERL_ERROR("Usage: g_setmag($int).");
    
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
    if (items != 0) PERL_ERROR("Usage: $int = g_getmag().");
    
    XSRETURN_IV(viewptr->GetMag());
}

// -----------------------------------------------------------------------------

XS(pl_fit)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 0) PERL_ERROR("Usage: g_fit().");
    
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
    if (items != 0) PERL_ERROR("Usage: g_fitsel().");
    
    if (viewptr->SelectionExists()) {
        viewptr->FitSelection();
        DoAutoUpdate();
    } else {
        PERL_ERROR("g_fitsel error: no selection.");
    }
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_visrect)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 4) PERL_ERROR("Usage: $bool = g_visrect(@rect).");
    
    int x = SvIV(ST(0));
    int y = SvIV(ST(1));
    int wd = SvIV(ST(2));
    int ht = SvIV(ST(3));
    const char* err = GSF_checkrect(x, y, wd, ht);
    if (err) PERL_ERROR(err);
    
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
    if (items != 0) PERL_ERROR("Usage: g_update().");
    
    GSF_update();
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_autoupdate)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 1) PERL_ERROR("Usage: g_autoupdate($bool).");
    
    autoupdate = (SvIV(ST(0)) != 0);
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_addlayer)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 0) PERL_ERROR("Usage: $newindex = g_addlayer().");
    
    if (numlayers >= MAX_LAYERS) {
        PERL_ERROR("g_addlayer error: no more layers can be added.");
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
    if (items != 0) PERL_ERROR("Usage: $newindex = g_clone().");
    
    if (numlayers >= MAX_LAYERS) {
        PERL_ERROR("g_clone error: no more layers can be added.");
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
    if (items != 0) PERL_ERROR("Usage: $newindex = g_duplicate().");
    
    if (numlayers >= MAX_LAYERS) {
        PERL_ERROR("g_duplicate error: no more layers can be added.");
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
    if (items != 0) PERL_ERROR("Usage: g_dellayer().");
    
    if (numlayers <= 1) {
        PERL_ERROR("g_dellayer error: there is only one layer.");
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
    if (items != 2) PERL_ERROR("Usage: g_movelayer($from,$to).");
    
    int fromindex = SvIV(ST(0));
    int toindex = SvIV(ST(1));
    
    if (fromindex < 0 || fromindex >= numlayers) {
        char msg[64];
        sprintf(msg, "Bad g_movelayer fromindex (%d).", fromindex);
        PERL_ERROR(msg);
    }
    if (toindex < 0 || toindex >= numlayers) {
        char msg[64];
        sprintf(msg, "Bad g_movelayer toindex (%d).", toindex);
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
    if (items != 1) PERL_ERROR("Usage: g_setlayer($index).");
    
    int index = SvIV(ST(0));
    
    if (index < 0 || index >= numlayers) {
        char msg[64];
        sprintf(msg, "Bad g_setlayer index (%d).", index);
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
    if (items != 0) PERL_ERROR("Usage: $int = g_getlayer().");
    
    XSRETURN_IV(currindex);
}

// -----------------------------------------------------------------------------

XS(pl_numlayers)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 0) PERL_ERROR("Usage: $int = g_numlayers().");
    
    XSRETURN_IV(numlayers);
}

// -----------------------------------------------------------------------------

XS(pl_maxlayers)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 0) PERL_ERROR("Usage: $int = g_maxlayers().");
    
    XSRETURN_IV(MAX_LAYERS);
}

// -----------------------------------------------------------------------------

XS(pl_setname)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items < 1 || items > 2) PERL_ERROR("Usage: g_setname($name,$index=current).");
    
    STRLEN n_a;
    const char* name = SvPV(ST(0), n_a);
    int index = currindex;
    if (items > 1) index = SvIV(ST(1));
    
    if (index < 0 || index >= numlayers) {
        char msg[64];
        sprintf(msg, "Bad g_setname index (%d).", index);
        PERL_ERROR(msg);
    }
    
    GSF_setname(wxString(name,wxConvLocal), index);
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_getname)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items > 1) PERL_ERROR("Usage: $name = g_getname($index=current).");
    
    int index = currindex;
    if (items > 0) index = SvIV(ST(0));
    
    if (index < 0 || index >= numlayers) {
        char msg[64];
        sprintf(msg, "Bad g_getname index (%d).", index);
        PERL_ERROR(msg);
    }
    
    XSRETURN_PV((const char*)GetLayer(index)->currname.mb_str(wxConvLocal));
}

// -----------------------------------------------------------------------------

XS(pl_setcolors)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 1) PERL_ERROR("Usage: g_setcolors($colors).");
    
    SV* colors = ST(0);
    if ( (!SvROK(colors)) || (SvTYPE(SvRV(colors)) != SVt_PVAV) ) {
        PERL_ERROR("g_setcolors error: 1st parameter is not a valid array reference.");
    }
    AV* inarray = (AV*)SvRV(colors);
    
    int len = av_len(inarray) + 1;
    if (len == 0) {
        // restore default colors in current layer and its clones
        UpdateLayerColors();
    } else if (len == 6) {
        // create gradient from r1,g1,b1 to r2,g2,b2
        int r1 = SvIV( *av_fetch(inarray, 0, 0) );
        int g1 = SvIV( *av_fetch(inarray, 1, 0) );
        int b1 = SvIV( *av_fetch(inarray, 2, 0) );
        int r2 = SvIV( *av_fetch(inarray, 3, 0) );
        int g2 = SvIV( *av_fetch(inarray, 4, 0) );
        int b2 = SvIV( *av_fetch(inarray, 5, 0) );
        CheckRGB(r1, g1, b1, "g_setcolors");
        CheckRGB(r2, g2, b2, "g_setcolors");
        currlayer->fromrgb.Set(r1, g1, b1);
        currlayer->torgb.Set(r2, g2, b2);
        CreateColorGradient();
        UpdateIconColors();
        UpdateCloneColors();
    } else if (len % 4 == 0) {
        int i = 0;
        while (i < len) {
            int s = SvIV( *av_fetch(inarray, i, 0) ); i++;
            int r = SvIV( *av_fetch(inarray, i, 0) ); i++;
            int g = SvIV( *av_fetch(inarray, i, 0) ); i++;
            int b = SvIV( *av_fetch(inarray, i, 0) ); i++;
            CheckRGB(r, g, b, "g_setcolors");
            if (s == -1) {
                // set all LIVE states to r,g,b (best not to alter state 0)
                for (s = 1; s < currlayer->algo->NumCellStates(); s++) {
                    currlayer->cellr[s] = r;
                    currlayer->cellg[s] = g;
                    currlayer->cellb[s] = b;
                }
            } else {
                if (s < 0 || s >= currlayer->algo->NumCellStates()) {
                    char msg[64];
                    sprintf(msg, "Bad state in g_setcolors (%d).", s);
                    PERL_ERROR(msg);
                } else {
                    currlayer->cellr[s] = r;
                    currlayer->cellg[s] = g;
                    currlayer->cellb[s] = b;
                }
            }
        }
        UpdateIconColors();
        UpdateCloneColors();
    } else {
        PERL_ERROR("g_setcolors error: array length is not a multiple of 4.");
    }
    
    DoAutoUpdate();
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_getcolors)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items > 1) PERL_ERROR("Usage: $colors = g_getcolors($state=-1).");
    
    int state = -1;
    if (items > 0) state = SvIV(ST(0));
    
    AV* outarray = (AV*)sv_2mortal( (SV*)newAV() );
    
    if (state == -1) {
        // return colors for ALL states, including state 0
        for (state = 0; state < currlayer->algo->NumCellStates(); state++) {
            av_push(outarray, newSViv(state));
            av_push(outarray, newSViv(currlayer->cellr[state]));
            av_push(outarray, newSViv(currlayer->cellg[state]));
            av_push(outarray, newSViv(currlayer->cellb[state]));
        }
    } else if (state >= 0 && state < currlayer->algo->NumCellStates()) {
        av_push(outarray, newSViv(state));
        av_push(outarray, newSViv(currlayer->cellr[state]));
        av_push(outarray, newSViv(currlayer->cellg[state]));
        av_push(outarray, newSViv(currlayer->cellb[state]));
    } else {
        char msg[64];
        sprintf(msg, "Bad g_getcolors state (%d).", state);
        PERL_ERROR(msg);
    }
    
    SP -= items;
    ST(0) = newRV( (SV*)outarray );
    sv_2mortal(ST(0));
    XSRETURN(1);
}

// -----------------------------------------------------------------------------

XS(pl_setoption)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 2) PERL_ERROR("Usage: $oldval = g_setoption($name,$newval).");
    
    STRLEN n_a;
    const char* optname = SvPV(ST(0), n_a);
    int newval = SvIV(ST(1));
    int oldval;
    
    if (!GSF_setoption(optname, newval, &oldval)) {
        PERL_ERROR("g_setoption error: unknown option.");
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
    if (items != 1) PERL_ERROR("Usage: $int = g_getoption($name).");
    
    STRLEN n_a;
    const char* optname = SvPV(ST(0), n_a);
    int optval;
    
    if (!GSF_getoption(optname, &optval)) {
        PERL_ERROR("g_getoption error: unknown option.");
    }
    
    XSRETURN_IV(optval);
}

// -----------------------------------------------------------------------------

XS(pl_setcolor)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 4) PERL_ERROR("Usage: @oldrgb = g_setcolor($name,$r,$g,$b).");
    
    STRLEN n_a;
    const char* colname = SvPV(ST(0), n_a);
    wxColor newcol(SvIV(ST(1)), SvIV(ST(2)), SvIV(ST(3)));
    wxColor oldcol;
    
    if (!GSF_setcolor(colname, newcol, oldcol)) {
        PERL_ERROR("g_setcolor error: unknown color.");
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
    if (items != 1) PERL_ERROR("Usage: @rgb = g_getcolor($name).");
    
    STRLEN n_a;
    const char* colname = SvPV(ST(0), n_a);
    wxColor color;
    
    if (!GSF_getcolor(colname, color)) {
        PERL_ERROR("g_getcolor error: unknown color.");
    }
    
    // return r,g,b values
    SP -= items;
    XPUSHs(sv_2mortal(newSViv(color.Red())));
    XPUSHs(sv_2mortal(newSViv(color.Green())));
    XPUSHs(sv_2mortal(newSViv(color.Blue())));
    XSRETURN(3);
}

// -----------------------------------------------------------------------------

XS(pl_setclipstr)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 1) PERL_ERROR("Usage: g_setclipstr($string).");
    
    STRLEN n_a;
    const char* clipstr = SvPV(ST(0), n_a);
    wxString wxs_clip(clipstr, wxConvLocal);
    
    mainptr->CopyTextToClipboard(wxs_clip);
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_getclipstr)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items > 0) PERL_ERROR("Usage: $string = g_getclipstr().");
    
    wxTextDataObject data;
    if ( !mainptr->GetTextFromClipboard(&data) ) PERL_ERROR("Could not get data from clipboard!");
    
    wxString wxs_clipstr = data.GetText();
    XSRETURN_PV((const char*)wxs_clipstr.mb_str(wxConvLocal));
}

// -----------------------------------------------------------------------------

XS(pl_getstring)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items < 1 || items > 3)
        PERL_ERROR("Usage: $string = g_getstring($prompt,$default='',$title='').");
    
    STRLEN n_a;
    const char* prompt = SvPV(ST(0), n_a);
    const char* initial = "";
    const char* title = "";
    if (items > 1) initial = SvPV(ST(1),n_a);
    if (items > 2) title = SvPV(ST(2),n_a);
    
    wxString result;
    if ( !GetString(wxString(title,wxConvLocal), wxString(prompt,wxConvLocal),
                    wxString(initial,wxConvLocal), result) ) {
        // user hit Cancel button
        AbortPerlScript();
        Perl_croak(aTHX_ NULL);
    }
    
    XSRETURN_PV((const char*)result.mb_str(wxConvLocal));
}

// -----------------------------------------------------------------------------

XS(pl_getxy)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 0) PERL_ERROR("Usage: $string = g_getxy().");
    
    statusptr->CheckMouseLocation(mainptr->infront);   // sets mousepos
    if (viewptr->showcontrols) mousepos = wxEmptyString;
    
    XSRETURN_PV((const char*)mousepos.mb_str(wxConvLocal));
}

// -----------------------------------------------------------------------------

XS(pl_getevent)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items > 1) PERL_ERROR("Usage: $string = g_getevent($get=1).");
    
    int get = 1;
    if (items > 0) get = SvIV(ST(0));
    
    wxString event;
    GSF_getevent(event, get);
    
    XSRETURN_PV((const char*)event.mb_str(wxConvLocal));
}

// -----------------------------------------------------------------------------

XS(pl_doevent)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 1) PERL_ERROR("Usage: g_doevent($string).");
    
    STRLEN n_a;
    const char* event = SvPV(ST(0), n_a);
    
    if (event[0]) {
        const char* err = GSF_doevent(wxString(event,wxConvLocal));
        if (err) PERL_ERROR(err);
    }
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_getkey)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 0) PERL_ERROR("Usage: $char = g_getkey().");
    
    char s[2];        // room for char + NULL
    s[0] = GSF_getkey();
    s[1] = '\0';
    
    XSRETURN_PV(s);
}

// -----------------------------------------------------------------------------

XS(pl_dokey)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 1) PERL_ERROR("Usage: g_dokey($char).");
    
    STRLEN n_a;
    const char* ascii = SvPV(ST(0), n_a);
    
    GSF_dokey(ascii);
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_show)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 1) PERL_ERROR("Usage: g_show($string).");
    
    STRLEN n_a;
    const char* s = SvPV(ST(0), n_a);
    
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
    if (items != 1) PERL_ERROR("Usage: g_error($string).");
    
    STRLEN n_a;
    const char* s = SvPV(ST(0), n_a);
    
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
    if (items != 1) PERL_ERROR("Usage: g_warn($string).");
    
    STRLEN n_a;
    const char* s = SvPV(ST(0), n_a);
    
    Warning(wxString(s,wxConvLocal));
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_note)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 1) PERL_ERROR("Usage: g_note($string).");
    
    STRLEN n_a;
    const char* s = SvPV(ST(0), n_a);
    
    Note(wxString(s,wxConvLocal));
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_help)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items != 1) PERL_ERROR("Usage: g_help($string).");
    
    STRLEN n_a;
    const char* htmlfile = SvPV(ST(0), n_a);
    
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
    if (items != 1) PERL_ERROR("Usage: g_check($bool).");
    
    allowcheck = (SvIV(ST(0)) != 0);
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

XS(pl_exit)
{
    IGNORE_UNUSED_PARAMS;
    RETURN_IF_ABORTED;
    dXSARGS;
    if (items > 1) PERL_ERROR("Usage: g_exit($string='').");
    
    STRLEN n_a;
    const char* err = (items == 1) ? SvPV(ST(0),n_a) : NULL;
    
    GSF_exit(wxString(err, wxConvLocal));
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
    const char* err = SvPV(ST(0),n_a);
    
    if (scripterr == wxString(abortmsg,wxConvLocal)) {
        // this can happen in Perl 5.14 so don't change scripterr
        // otherwise a message box will appear
    } else {
        // store message in global string (shown after script finishes)
        scripterr = wxString(err, wxConvLocal);
    }
    
    XSRETURN(0);
}

// -----------------------------------------------------------------------------

// xs_init is passed into perl_parse and initializes statically linked extensions

EXTERN_C void xs_init(pTHX)
{
#ifdef __WXMSW__
    wxUnusedVar(my_perl);
#endif
    const char* file = __FILE__;
    dXSUB_SYS;
    
    // DynaLoader allows dynamic loading of other Perl extensions
    newXS((char*)"DynaLoader::boot_DynaLoader", boot_DynaLoader, (char*)file);
    
    // filing
    newXS((char*)"g_open",         pl_open,         (char*)file);
    newXS((char*)"g_save",         pl_save,         (char*)file);
    newXS((char*)"g_opendialog",   pl_opendialog,   (char*)file);
    newXS((char*)"g_savedialog",   pl_savedialog,   (char*)file);
    newXS((char*)"g_load",         pl_load,         (char*)file);
    newXS((char*)"g_store",        pl_store,        (char*)file);
    newXS((char*)"g_setdir",       pl_setdir,       (char*)file);
    newXS((char*)"g_getdir",       pl_getdir,       (char*)file);
    // next two are deprecated (use g_getdir)
    newXS((char*)"g_appdir",       pl_appdir,       (char*)file);
    newXS((char*)"g_datadir",      pl_datadir,      (char*)file);
    // editing
    newXS((char*)"g_new",          pl_new,          (char*)file);
    newXS((char*)"g_cut",          pl_cut,          (char*)file);
    newXS((char*)"g_copy",         pl_copy,         (char*)file);
    newXS((char*)"g_clear",        pl_clear,        (char*)file);
    newXS((char*)"g_paste",        pl_paste,        (char*)file);
    newXS((char*)"g_shrink",       pl_shrink,       (char*)file);
    newXS((char*)"g_randfill",     pl_randfill,     (char*)file);
    newXS((char*)"g_flip",         pl_flip,         (char*)file);
    newXS((char*)"g_rotate",       pl_rotate,       (char*)file);
    newXS((char*)"g_parse",        pl_parse,        (char*)file);
    newXS((char*)"g_transform",    pl_transform,    (char*)file);
    newXS((char*)"g_evolve",       pl_evolve,       (char*)file);
    newXS((char*)"g_putcells",     pl_putcells,     (char*)file);
    newXS((char*)"g_getcells",     pl_getcells,     (char*)file);
    newXS((char*)"g_join",         pl_join,         (char*)file);
    newXS((char*)"g_hash",         pl_hash,         (char*)file);
    newXS((char*)"g_getclip",      pl_getclip,      (char*)file);
    newXS((char*)"g_select",       pl_select,       (char*)file);
    newXS((char*)"g_getrect",      pl_getrect,      (char*)file);
    newXS((char*)"g_getselrect",   pl_getselrect,   (char*)file);
    newXS((char*)"g_setcell",      pl_setcell,      (char*)file);
    newXS((char*)"g_getcell",      pl_getcell,      (char*)file);
    newXS((char*)"g_setcursor",    pl_setcursor,    (char*)file);
    newXS((char*)"g_getcursor",    pl_getcursor,    (char*)file);
    // control
    newXS((char*)"g_empty",        pl_empty,        (char*)file);
    newXS((char*)"g_run",          pl_run,          (char*)file);
    newXS((char*)"g_step",         pl_step,         (char*)file);
    newXS((char*)"g_setstep",      pl_setstep,      (char*)file);
    newXS((char*)"g_getstep",      pl_getstep,      (char*)file);
    newXS((char*)"g_setbase",      pl_setbase,      (char*)file);
    newXS((char*)"g_getbase",      pl_getbase,      (char*)file);
    newXS((char*)"g_advance",      pl_advance,      (char*)file);
    newXS((char*)"g_reset",        pl_reset,        (char*)file);
    newXS((char*)"g_setgen",       pl_setgen,       (char*)file);
    newXS((char*)"g_getgen",       pl_getgen,       (char*)file);
    newXS((char*)"g_getpop",       pl_getpop,       (char*)file);
    newXS((char*)"g_numstates",    pl_numstates,    (char*)file);
    newXS((char*)"g_numalgos",     pl_numalgos,     (char*)file);
    newXS((char*)"g_setalgo",      pl_setalgo,      (char*)file);
    newXS((char*)"g_getalgo",      pl_getalgo,      (char*)file);
    newXS((char*)"g_setrule",      pl_setrule,      (char*)file);
    newXS((char*)"g_getrule",      pl_getrule,      (char*)file);
    newXS((char*)"g_getwidth",     pl_getwidth,     (char*)file);
    newXS((char*)"g_getheight",    pl_getheight,    (char*)file);
    // viewing
    newXS((char*)"g_setpos",       pl_setpos,       (char*)file);
    newXS((char*)"g_getpos",       pl_getpos,       (char*)file);
    newXS((char*)"g_setmag",       pl_setmag,       (char*)file);
    newXS((char*)"g_getmag",       pl_getmag,       (char*)file);
    newXS((char*)"g_fit",          pl_fit,          (char*)file);
    newXS((char*)"g_fitsel",       pl_fitsel,       (char*)file);
    newXS((char*)"g_visrect",      pl_visrect,      (char*)file);
    newXS((char*)"g_update",       pl_update,       (char*)file);
    newXS((char*)"g_autoupdate",   pl_autoupdate,   (char*)file);
    // layers
    newXS((char*)"g_addlayer",     pl_addlayer,     (char*)file);
    newXS((char*)"g_clone",        pl_clone,        (char*)file);
    newXS((char*)"g_duplicate",    pl_duplicate,    (char*)file);
    newXS((char*)"g_dellayer",     pl_dellayer,     (char*)file);
    newXS((char*)"g_movelayer",    pl_movelayer,    (char*)file);
    newXS((char*)"g_setlayer",     pl_setlayer,     (char*)file);
    newXS((char*)"g_getlayer",     pl_getlayer,     (char*)file);
    newXS((char*)"g_numlayers",    pl_numlayers,    (char*)file);
    newXS((char*)"g_maxlayers",    pl_maxlayers,    (char*)file);
    newXS((char*)"g_setname",      pl_setname,      (char*)file);
    newXS((char*)"g_getname",      pl_getname,      (char*)file);
    newXS((char*)"g_setcolors",    pl_setcolors,    (char*)file);
    newXS((char*)"g_getcolors",    pl_getcolors,    (char*)file);
    // miscellaneous
    newXS((char*)"g_setoption",    pl_setoption,    (char*)file);
    newXS((char*)"g_getoption",    pl_getoption,    (char*)file);
    newXS((char*)"g_setcolor",     pl_setcolor,     (char*)file);
    newXS((char*)"g_getcolor",     pl_getcolor,     (char*)file);
    newXS((char*)"g_setclipstr",   pl_setclipstr,   (char*)file);
    newXS((char*)"g_getclipstr",   pl_getclipstr,   (char*)file);
    newXS((char*)"g_getstring",    pl_getstring,    (char*)file);
    newXS((char*)"g_getxy",        pl_getxy,        (char*)file);
    newXS((char*)"g_getevent",     pl_getevent,     (char*)file);
    newXS((char*)"g_doevent",      pl_doevent,      (char*)file);
    // next two are deprecated (use g_getevent and g_doevent)
    newXS((char*)"g_getkey",       pl_getkey,       (char*)file);
    newXS((char*)"g_dokey",        pl_dokey,        (char*)file);
    newXS((char*)"g_show",         pl_show,         (char*)file);
    newXS((char*)"g_error",        pl_error,        (char*)file);
    newXS((char*)"g_warn",         pl_warn,         (char*)file);
    newXS((char*)"g_note",         pl_note,         (char*)file);
    newXS((char*)"g_help",         pl_help,         (char*)file);
    newXS((char*)"g_check",        pl_check,        (char*)file);
    newXS((char*)"g_exit",         pl_exit,         (char*)file);
    // internal use only (don't document)
    newXS((char*)"g_fatal",        pl_fatal,        (char*)file);
}

#ifdef PERL510_OR_LATER
    static bool inited = false;
#endif

#endif // ENABLE_PERL

// =============================================================================

void RunPerlScript(const wxString &filepath)
{
#ifdef ENABLE_PERL

    // allow re-entrancy
    bool already_in_perl = (my_perl != NULL);
    
    if (!already_in_perl) {
        #ifdef USE_PERL_DYNAMIC
            if (perldll == NULL) {
                // try to load Perl library
                if ( !LoadPerlLib() ) return;
            }
        #endif
        
        // create a dummy environment for initializing the embedded interpreter
        static int argc = 3;
        static char arg1[] = "-e", arg2[] = "0";
        static char *args[] = { NULL, arg1, arg2, NULL }, **argv = &args[0];
        
        #ifdef PERL510_OR_LATER
            static char *ens[] = { NULL }, **env = &ens[0];
            if (!inited) {
                PERL_SYS_INIT3(&argc, &argv, &env);
                inited = true;
            }
        #endif
        
        my_perl = perl_alloc();
        if (!my_perl) {
            Warning(_("Could not create Perl interpreter!"));
            return;
        }
        
        PL_perl_destruct_level = 1;
        perl_construct(my_perl);
        
        // set PERL_EXIT_DESTRUCT_END flag so that perl_destruct will execute
        // any END blocks in given script (this flag requires Perl 5.7.2+)
        PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
        
        perl_parse(my_perl, xs_init, argc, argv, NULL);
        perl_run(my_perl);
    }
    
    // convert any \ to \\ and then convert any ' to \'
    wxString fpath = filepath;
    fpath.Replace(wxT("\\"), wxT("\\\\"));
    fpath.Replace(wxT("'"), wxT("\\'"));
    
    // construct a command to run the given script file and capture errors
    wxString command = wxT("do '") + fpath + wxT("'; g_fatal($@) if $@;");
    perl_eval_pv(command.mb_str(wxConvLocal), TRUE);
    
    if (!already_in_perl) {
        // any END blocks will now be executed by perl_destruct, so we temporarily
        // clear scripterr so that RETURN_IF_ABORTED won't call Perl_croak;
        // this allows g_* commands in END blocks to work after user hits escape
        // or if g_exit has been called
        wxString savestring = scripterr;
        scripterr = wxEmptyString;
        PL_perl_destruct_level = 1;
        perl_destruct(my_perl);
        scripterr = savestring;
        
        perl_free(my_perl);
        my_perl = NULL;
    }

#else

    Warning(_("Sorry, but Perl scripting is no longer supported."));

#endif // ENABLE_PERL
}

// -----------------------------------------------------------------------------

void AbortPerlScript()
{
#ifdef ENABLE_PERL

    scripterr = wxString(abortmsg,wxConvLocal);
    // can't call Perl_croak here (done via RETURN_IF_ABORTED)

#endif // ENABLE_PERL
}

// -----------------------------------------------------------------------------

void FinishPerlScripting()
{
#ifdef ENABLE_PERL

#ifdef PERL510_OR_LATER
    if (inited) {
        PERL_SYS_TERM();
    }
#endif
    
#ifdef USE_PERL_DYNAMIC
    // probably don't really need to do this
    FreePerlLib();
#endif

#endif // ENABLE_PERL
}
