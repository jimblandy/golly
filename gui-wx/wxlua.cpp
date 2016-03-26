/*** /
 
 This file is part of Golly, a Game of Life Simulator.
 Copyright (C) 2013 Andrew Trevorrow and Tomas Rokicki.
 
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
    Golly uses a statically embedded Lua interpreter to execute .lua scripts.
    Here is the official Lua copyright notice:

    Copyright 1994-2015 Lua.org, PUC-Rio.
    
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:
    
    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.
    
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#include "wx/wxprec.h"      // for compilers that support precompilation
#ifndef WX_PRECOMP
    #include "wx/wx.h"      // for all others include the necessary headers
#endif

#include "wx/filename.h"    // for wxFileName

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "readpattern.h"
#include "writepattern.h"

#include "wxgolly.h"        // for wxGetApp, mainptr, viewptr, statusptr
#include "wxmain.h"         // for mainptr->...
#include "wxselect.h"       // for Selection
#include "wxview.h"         // for viewptr->...
#include "wxstatus.h"       // for statusptr->...
#include "wxutils.h"        // for Warning, Note, GetString, etc
#include "wxprefs.h"        // for gollydir, etc
#include "wxinfo.h"         // for ShowInfo
#include "wxhelp.h"         // for ShowHelp
#include "wxundo.h"         // for currlayer->undoredo->...
#include "wxalgos.h"        // for *_ALGO, CreateNewUniverse, etc
#include "wxlayer.h"        // for AddLayer, currlayer, currindex, etc
#include "wxscript.h"       // for inscript, abortmsg, GSF_*, etc
#include "wxlua.h"

#include "lua.hpp"          // Lua header files for C++

// -----------------------------------------------------------------------------

static bool aborted = false;    // stop the current script?

static void CheckEvents(lua_State* L)
{
    // this routine is called at the start of every g_* function so we can
    // detect user events (eg. hitting the stop button or escape key)

    if (allowcheck) wxGetApp().Poller()->checkevents();
    
    if (insideYield) return;
    
    // we're outside Yield so safe to do a longjmp from lua_error
    if (aborted) {
        // AbortLuaScript was called
        lua_pushstring(L, abortmsg);
        lua_error(L);
    }
}

// -----------------------------------------------------------------------------

static void GollyError(lua_State* L, const char* errmsg)
{
    // handle an error detected in a g_* function
    lua_pushstring(L, errmsg);
    lua_error(L);
}

// -----------------------------------------------------------------------------

static int g_getdir(lua_State* L)
{
    CheckEvents(L);
    
    const char* dirname = luaL_checkstring(L, 1);

    const char* dirstring = GSF_getdir((char *) dirname);
    if (dirstring == NULL) GollyError(L, "getdir error: unknown directory name.");

    lua_pushstring(L, dirstring);
    
    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_getrect(lua_State* L)
{
    CheckEvents(L);

    lua_newtable(L);

    if (!currlayer->algo->isEmpty()) {
        bigint top, left, bottom, right;
        currlayer->algo->findedges(&top, &left, &bottom, &right);
        if (viewptr->OutsideLimits(top, left, bottom, right)) {
            GollyError(L, "getrect error: pattern is too big.");
        }
        
        int x = left.toint();
        int y = top.toint();
        int wd = right.toint() - x + 1;
        int ht = bottom.toint() - y + 1;
        
        lua_pushinteger(L, x);
        lua_rawseti(L, -2, 1);
        
        lua_pushinteger(L, y);
        lua_rawseti(L, -2, 2);
        
        lua_pushinteger(L, wd);
        lua_rawseti(L, -2, 3);
        
        lua_pushinteger(L, ht);
        lua_rawseti(L, -2, 4);
    }
    
    return 1;   // result is a table (empty or with 4 ints)
}

// -----------------------------------------------------------------------------

static int g_getselrect(lua_State* L)
{
    CheckEvents(L);

    lua_newtable(L);

    if (viewptr->SelectionExists()) {
        if (currlayer->currsel.TooBig()) {
            GollyError(L, "getselrect error: selection is too big.");
        }
        
        int x, y, wd, ht;
        currlayer->currsel.GetRect(&x, &y, &wd, &ht);
        
        lua_pushinteger(L, x);
        lua_rawseti(L, -2, 1);
        
        lua_pushinteger(L, y);
        lua_rawseti(L, -2, 2);
        
        lua_pushinteger(L, wd);
        lua_rawseti(L, -2, 3);
        
        lua_pushinteger(L, ht);
        lua_rawseti(L, -2, 4);
    }
    
    return 1;   // result is a table (empty or with 4 ints)
}

// -----------------------------------------------------------------------------

static int g_setcell(lua_State* L)
{
    CheckEvents(L);
    
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int state = luaL_checkinteger(L, 3);
    
    const char* err = GSF_setcell(x, y, state);
    if (err) GollyError(L, err);
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_getcell(lua_State* L)
{
    CheckEvents(L);
    
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    
    // check if x,y is outside bounded grid
    const char* err = GSF_checkpos(currlayer->algo, x, y);
    if (err) GollyError(L, err);
    
    lua_pushinteger(L, currlayer->algo->getcell(x, y));
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_getpop(lua_State* L)
{
    CheckEvents(L);
    
    char sepchar = '\0';
    if (lua_gettop(L) > 0) {
        const char* s = luaL_checkstring(L, 1);
        sepchar = s[0];
    }
    
    lua_pushstring(L, currlayer->algo->getPopulation().tostring(sepchar));

    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_numstates(lua_State* L)
{
    CheckEvents(L);
    
    lua_pushinteger(L, currlayer->algo->NumCellStates());
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_fitsel(lua_State* L)
{
    CheckEvents(L);
    
    if (viewptr->SelectionExists()) {
        viewptr->FitSelection();
        DoAutoUpdate();
    } else {
        GollyError(L, "fitsel error: no selection.");
    }
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_visrect(lua_State* L)
{
    CheckEvents(L);
    
    // arg must be a table with 4 ints
    luaL_checktype(L, 1, LUA_TTABLE);
    
    int numints = luaL_len(L, 1);
    if (numints != 4) {
        GollyError(L, "visrect error: table must have 4 integers.");
    }
    
    lua_rawgeti(L, 1, 1);   // push x
    lua_rawgeti(L, 1, 2);   // push y
    lua_rawgeti(L, 1, 3);   // push wd
    lua_rawgeti(L, 1, 4);   // push ht
    
    int x = luaL_checkinteger(L, 2);
    int y = luaL_checkinteger(L, 3);
    int wd = luaL_checkinteger(L, 4);
    int ht = luaL_checkinteger(L, 5);
    const char* err = GSF_checkrect(x, y, wd, ht);
    if (err) GollyError(L, err);

    bigint left = x;
    bigint top = y;
    bigint right = x + wd - 1;
    bigint bottom = y + ht - 1;
    int visible = viewptr->CellVisible(left, top) &&
                  viewptr->CellVisible(right, bottom);
    
    lua_pushboolean(L, visible);
    
    return 1;   // result is a boolean
}

// -----------------------------------------------------------------------------

static int g_update(lua_State* L)
{
    CheckEvents(L);
    
    GSF_update();
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_getstring(lua_State* L)
{
    CheckEvents(L);

    const char* prompt = luaL_checkstring(L, 1);
    const char* initial = lua_gettop(L) > 1 ? luaL_checkstring(L, 2) : "";
    const char* title = lua_gettop(L) > 2 ? luaL_checkstring(L, 3) : "";
    
    wxString result;
    if (!GetString(wxString(title,wxConvUTF8),
                   wxString(prompt,wxConvUTF8),
                   wxString(initial,wxConvUTF8),
                   result)) {
        // user hit Cancel button
        AbortLuaScript();
        lua_pushstring(L, abortmsg);
        lua_error(L);
    }
    
    lua_pushstring(L, result.mb_str(wxConvUTF8));
    
    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_show(lua_State* L)
{
    CheckEvents(L);
    
    const char* s = luaL_checkstring(L, 1);

    inscript = false;
    statusptr->DisplayMessage(wxString(s, wxConvUTF8));
    inscript = true;
    // make sure status bar is visible
    if (!showstatus) mainptr->ToggleStatusBar();
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_error(lua_State* L)
{
    CheckEvents(L);

    const char* s = luaL_checkstring(L, 1);
    
    inscript = false;
    statusptr->ErrorMessage(wxString(s, wxConvUTF8));
    inscript = true;
    // make sure status bar is visible
    if (!showstatus) mainptr->ToggleStatusBar();
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_warn(lua_State* L)
{
    CheckEvents(L);

    const char* s = luaL_checkstring(L, 1);
        
    Warning(wxString(s, wxConvUTF8));
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_note(lua_State* L)
{
    CheckEvents(L);
    
    const char* s = luaL_checkstring(L, 1);
    Note(wxString(s, wxConvUTF8));
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_help(lua_State* L)
{
    CheckEvents(L);
    
    const char* htmlfile = luaL_checkstring(L, 1);
    ShowHelp(wxString(htmlfile, wxConvUTF8));
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_check(lua_State* L)
{
    // CheckEvents(L);
    // don't call CheckEvents here otherwise we can't safely write code like
    //    if g.getlayer() == target then
    //       g.check(0)
    //       ... do stuff to target layer ...
    //       g.check(1)
    
    allowcheck = (luaL_checkinteger(L, 1) != 0);
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_exit(lua_State* L)
{
    // script will terminate so no point calling CheckEvents here

    if (lua_gettop(L) == 0) {
        GSF_exit(NULL);
    } else {
        GSF_exit((char *) luaL_checkstring(L, 1));
    }
    
    AbortLuaScript();
    lua_pushstring(L, abortmsg);
    lua_error(L);
    
    return 0;   // never get here (lua_error does a longjmp)
}

// -----------------------------------------------------------------------------

static const struct luaL_Reg gollyfuncs [] = {
    // filing
//!!{ "open",         g_open },         // open given pattern/rule/html file
//!!{ "save",         g_save },         // save pattern in given file using given format
//!!{ "opendialog",   g_opendialog },   // return input path and filename chosen by user
//!!{ "savedialog",   g_savedialog },   // return output path and filename chosen by user
//!!{ "load",         g_load },         // read pattern file and return cell list
//!!{ "store",        g_store },        // write cell list to a file (in RLE format)
//!!{ "setdir",       g_setdir },       // set location of specified directory
    { "getdir",       g_getdir },       // return location of specified directory
    // editing
//!!{ "new",          g_new },          // create new universe and set window title
//!!{ "cut",          g_cut },          // cut selection to clipboard
//!!{ "copy",         g_copy },         // copy selection to clipboard
//!!{ "clear",        g_clear },        // clear inside/outside selection
//!!{ "paste",        g_paste },        // paste clipboard pattern at x,y using given mode
//!!{ "shrink",       g_shrink },       // shrink selection
//!!{ "randfill",     g_randfill },     // randomly fill selection to given percentage
//!!{ "flip",         g_flip },         // flip selection top-bottom or left-right
//!!{ "rotate",       g_rotate },       // rotate selection 90 deg clockwise or anticlockwise
//!!{ "parse",        g_parse },        // parse RLE or Life 1.05 string and return cell list
//!!{ "transform",    g_transform },    // apply an affine transformation to cell list
//!!{ "evolve",       g_evolve },       // generate pattern contained in given cell list
//!!{ "putcells",     g_putcells },     // paste given cell list into current universe
//!!{ "getcells",     g_getcells },     // return cell list in given rectangle
//!!{ "join",         g_join },         // return concatenation of given cell lists
//!!{ "hash",         g_hash },         // return hash value for pattern in given rectangle
//!!{ "getclip",      g_getclip },      // return pattern in clipboard (as cell list)
//!!{ "select",       g_select },       // select [x, y, wd, ht] rectangle or remove if []
    { "getrect",      g_getrect },      // return pattern rectangle as [] or [x, y, wd, ht]
    { "getselrect",   g_getselrect },   // return selection rectangle as [] or [x, y, wd, ht]
    { "setcell",      g_setcell },      // set given cell to given state
    { "getcell",      g_getcell },      // get state of given cell
//!!{ "setcursor",    g_setcursor },    // set cursor (returns old cursor)
//!!{ "getcursor",    g_getcursor },    // return current cursor
    // control
//!!{ "empty",        g_empty },        // return true if universe is empty
//!!{ "run",          g_run },          // run current pattern for given number of gens
//!!{ "step",         g_step },         // run current pattern for current step
//!!{ "setstep",      g_setstep },      // set step exponent
//!!{ "getstep",      g_getstep },      // return current step exponent
//!!{ "setbase",      g_setbase },      // set base step
//!!{ "getbase",      g_getbase },      // return current base step
//!!{ "advance",      g_advance },      // advance inside/outside selection by given gens
//!!{ "reset",        g_reset },        // restore starting pattern
//!!{ "setgen",       g_setgen },       // set current generation to given string
//!!{ "getgen",       g_getgen },       // return current generation as string
    { "getpop",       g_getpop },       // return current population as string
    { "numstates",    g_numstates },    // return number of cell states in current universe
//!!{ "numalgos",     g_numalgos },     // return number of algorithms
//!!{ "setalgo",      g_setalgo },      // set current algorithm using given string
//!!{ "getalgo",      g_getalgo },      // return name of given or current algorithm
//!!{ "setrule",      g_setrule },      // set current rule using given string
//!!{ "getrule",      g_getrule },      // return current rule
//!!{ "getwidth",     g_getwidth },     // return width of universe (0 if unbounded)
//!!{ "getheight",    g_getheight },    // return height of universe (0 if unbounded)
    // viewing
//!!{ "setpos",       g_setpos },       // move given cell to middle of viewport
//!!{ "getpos",       g_getpos },       // return x,y position of cell in middle of viewport
//!!{ "setmag",       g_setmag },       // set magnification (0=1:1, 1=1:2, -1=2:1, etc)
//!!{ "getmag",       g_getmag },       // return current magnification
//!!{ "fit",          g_fit },          // fit entire pattern in viewport
    { "fitsel",       g_fitsel },       // fit selection in viewport
    { "visrect",      g_visrect },      // return true if given rect is completely visible
    { "update",       g_update },       // update display (viewport and status bar)
//!!{ "autoupdate",   g_autoupdate },   // update display after each change to universe?
    // layers
//!!{ "addlayer",     g_addlayer },     // add a new layer
//!!{ "clone",        g_clone },        // add a cloned layer (shares universe)
//!!{ "duplicate",    g_duplicate },    // add a duplicate layer (copies universe)
//!!{ "dellayer",     g_dellayer },     // delete current layer
//!!{ "movelayer",    g_movelayer },    // move given layer to new index
//!!{ "setlayer",     g_setlayer },     // switch to given layer
//!!{ "getlayer",     g_getlayer },     // return index of current layer
//!!{ "numlayers",    g_numlayers },    // return current number of layers
//!!{ "maxlayers",    g_maxlayers },    // return maximum number of layers
//!!{ "setname",      g_setname },      // set name of given layer
//!!{ "getname",      g_getname },      // get name of given layer
//!!{ "setcolors",    g_setcolors },    // set color(s) used in current layer
//!!{ "getcolors",    g_getcolors },    // get color(s) used in current layer
    // miscellaneous
//!!{ "setoption",    g_setoption },    // set given option to new value (and return old value)
//!!{ "getoption",    g_getoption },    // return current value of given option
//!!{ "setcolor",     g_setcolor },     // set given color to new r,g,b (returns old r,g,b)
//!!{ "getcolor",     g_getcolor },     // return r,g,b values of given color
//!!{ "setclipstr",   g_setclipstr },   // set the clipboard contents to a given string value
//!!{ "getclipstr",   g_getclipstr },   // retrieve the contents of the clipboard as a string
    { "getstring",    g_getstring },    // display dialog box and get string from user
//!!{ "getxy",        g_getxy },        // return current grid location of mouse
//!!{ "getevent",     g_getevent },     // return keyboard/mouse event or empty string if none
//!!{ "doevent",      g_doevent },      // pass given keyboard/mouse event to Golly to handle
    { "show",         g_show },         // show given string in status bar
    { "error",        g_error },        // beep and show given string in status bar
    { "warn",         g_warn },         // show given string in warning dialog
    { "note",         g_note },         // show given string in note dialog
    { "help",         g_help },         // show given HTML file in help window
    { "check",        g_check },        // allow event checking?
    { "exit",         g_exit },         // exit script with optional error message
    {NULL, NULL}
};

// -----------------------------------------------------------------------------

static int create_gollylib(lua_State* L)
{
    // create a table with our g_* functions and register them
    luaL_newlib(L, gollyfuncs);
    return 1;
}

// -----------------------------------------------------------------------------

void RunLuaScript(const wxString& filepath)
{    
    aborted = false;
    
    lua_State* L = luaL_newstate();
    
    luaL_openlibs(L);

    // we want our g_* functions to be called from Lua as g.*
    lua_pushcfunction(L, create_gollylib); lua_setglobal(L, "gollylib");    // or just "golly" ???!!!

    // It would be nice if we could do this now:
    //
    // luaL_dostring(L, "local g = gollylib()");
    //
    // But it doesn't work because g goes out of scope, so users
    // will have to start their scripts with that line.
    // Note that we could do this:
    //
    // luaL_dostring(L, "g = gollylib()");
    //
    // But it's ~10% slower to access functions because g is global.
    
    if (luaL_dofile(L, filepath.mb_str(wxConvUTF8))) {
        scripterr += wxString(lua_tostring(L, -1), wxConvUTF8);
        scripterr += wxT("\n");
        // scripterr is checked at the end of RunScript in wxscript.cpp
        lua_pop(L, 1);
    }
    
    lua_close(L);
}

// -----------------------------------------------------------------------------

void AbortLuaScript()
{
    // calling lua_error here causes nasty problems (presumably due to doing
    // a longjmp from inside Yield) so we set the aborted flag and check it
    // in CheckEvents only after Yield has finished
    
    aborted = true;
}

// -----------------------------------------------------------------------------

void FinishLuaScripting()
{
    // no need to do anything
}
