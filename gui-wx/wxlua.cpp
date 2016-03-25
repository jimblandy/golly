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

// The following g_* functions can be called from Lua:

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

static int g_help(lua_State* L)
{
    CheckEvents(L);
    
    const char* htmlfile = luaL_checkstring(L, 1);
    ShowHelp(wxString(htmlfile, wxConvUTF8));
    
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

static int g_numstates(lua_State* L)
{
    CheckEvents(L);
    
    lua_pushinteger(L, currlayer->algo->NumCellStates());
    
    return 1;   // result is an integer
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

static int g_update(lua_State* L)
{
    CheckEvents(L);
    
    GSF_update();
    
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

static const struct luaL_Reg gollylib [] = {
    {"exit",        g_exit},
    {"fitsel",      g_fitsel},
    {"getcell",     g_getcell},
    {"getdir",      g_getdir},
    {"getselrect",  g_getselrect},
    {"getstring",   g_getstring},
    {"help",        g_help},
    {"note",        g_note},
    {"numstates",   g_numstates},
    {"setcell",     g_setcell},
    {"show",        g_show},
    {"update",      g_update},
    {"visrect",     g_visrect},
    {NULL, NULL}
};

static int create_gollylib(lua_State* L)
{
    // create a table with our g_* functions and register them
    luaL_newlib(L, gollylib);
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
