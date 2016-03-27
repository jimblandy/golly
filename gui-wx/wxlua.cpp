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

static int g_open(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_save(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_opendialog(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_savedialog(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_load(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_store(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_setdir(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_getdir(lua_State* L)
{
    CheckEvents(L);
    
    const char* dirname = luaL_checkstring(L, 1);

    const char* dirstring = GSF_getdir((char*) dirname);
    if (dirstring == NULL) GollyError(L, "getdir error: unknown directory name.");

    lua_pushstring(L, dirstring);
    
    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_new(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_cut(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_copy(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_clear(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_paste(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_shrink(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_randfill(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_flip(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_rotate(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_parse(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_transform(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_evolve(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_putcells(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_getcells(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_join(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_hash(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_getclip(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_select(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
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
        
        lua_pushinteger(L, x);  lua_rawseti(L, -2, 1);
        lua_pushinteger(L, y);  lua_rawseti(L, -2, 2);
        lua_pushinteger(L, wd); lua_rawseti(L, -2, 3);
        lua_pushinteger(L, ht); lua_rawseti(L, -2, 4);
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
        
        lua_pushinteger(L, x);  lua_rawseti(L, -2, 1);
        lua_pushinteger(L, y);  lua_rawseti(L, -2, 2);
        lua_pushinteger(L, wd); lua_rawseti(L, -2, 3);
        lua_pushinteger(L, ht); lua_rawseti(L, -2, 4);
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

static int g_setcursor(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_getcursor(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_empty(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_run(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_step(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_setstep(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_getstep(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_setbase(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_getbase(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_advance(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_reset(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_setgen(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_getgen(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
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

static int g_numalgos(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_setalgo(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_getalgo(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_setrule(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_getrule(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_getwidth(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_getheight(lua_State* L)
{
    CheckEvents(L);

    //!!!
    
    return 0;   // no result???!!!
}

// -----------------------------------------------------------------------------

static int g_setpos(lua_State* L)
{
    CheckEvents(L);

    const char* x = luaL_checkstring(L, 1);
    const char* y = luaL_checkstring(L, 2);

    const char* err = GSF_setpos((char*) x, (char*) y);
    if (err) GollyError(L, err);
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_getpos(lua_State* L)
{
    CheckEvents(L);

    char sepchar = '\0';
    if (lua_gettop(L) > 0) {
        const char* s = luaL_checkstring(L, 1);
        sepchar = s[0];
    }
    
    bigint bigx, bigy;
    viewptr->GetPos(bigx, bigy);

    // return position as x,y strings
    lua_pushstring(L, bigx.tostring(sepchar));
    lua_pushstring(L, bigy.tostring(sepchar));
    
    return 2;   // result is 2 strings
}

// -----------------------------------------------------------------------------

static int g_setmag(lua_State* L)
{
    CheckEvents(L);

    int mag = luaL_checkinteger(L, 1);
    
    viewptr->SetMag(mag);
    DoAutoUpdate();
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_getmag(lua_State* L)
{
    CheckEvents(L);

    lua_pushinteger(L, viewptr->GetMag());
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_fit(lua_State* L)
{
    CheckEvents(L);

    viewptr->FitPattern();
    DoAutoUpdate();
    
    return 0;   // no result
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
    
    lua_rawgeti(L, 1, 1); int x  = luaL_checkinteger(L,-1); lua_pop(L,1);
    lua_rawgeti(L, 1, 2); int y  = luaL_checkinteger(L,-1); lua_pop(L,1);
    lua_rawgeti(L, 1, 3); int wd = luaL_checkinteger(L,-1); lua_pop(L,1);
    lua_rawgeti(L, 1, 4); int ht = luaL_checkinteger(L,-1); lua_pop(L,1);
    
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

static int g_autoupdate(lua_State* L)
{
    CheckEvents(L);

    autoupdate = lua_toboolean(L, 1);
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_addlayer(lua_State* L)
{
    CheckEvents(L);

    if (numlayers >= MAX_LAYERS) {
        GollyError(L, "addlayer error: no more layers can be added.");
    } else {
        AddLayer();
        DoAutoUpdate();
    }
    
    // return index of new layer
    lua_pushinteger(L, currindex);
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_clone(lua_State* L)
{
    CheckEvents(L);

    if (numlayers >= MAX_LAYERS) {
        GollyError(L, "clone error: no more layers can be added.");
    } else {
        CloneLayer();
        DoAutoUpdate();
    }
    
    // return index of new layer
    lua_pushinteger(L, currindex);
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_duplicate(lua_State* L)
{
    CheckEvents(L);

    if (numlayers >= MAX_LAYERS) {
        GollyError(L, "duplicate error: no more layers can be added.");
    } else {
        DuplicateLayer();
        DoAutoUpdate();
    }
    
    // return index of new layer
    lua_pushinteger(L, currindex);
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_dellayer(lua_State* L)
{
    CheckEvents(L);

    if (numlayers <= 1) {
        GollyError(L, "dellayer error: there is only one layer.");
    } else {
        DeleteLayer();
        DoAutoUpdate();
    }
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_movelayer(lua_State* L)
{
    CheckEvents(L);
    
    int fromindex = luaL_checkinteger(L, 1);
    int toindex = luaL_checkinteger(L, 2);

    if (fromindex < 0 || fromindex >= numlayers) {
        char msg[64];
        sprintf(msg, "movelayer error: bad fromindex (%d)", fromindex);
        GollyError(L, msg);
    }
    if (toindex < 0 || toindex >= numlayers) {
        char msg[64];
        sprintf(msg, "movelayer error: bad toindex (%d)", toindex);
        GollyError(L, msg);
    }
    
    MoveLayer(fromindex, toindex);
    DoAutoUpdate();
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_setlayer(lua_State* L)
{
    CheckEvents(L);

    int index = luaL_checkinteger(L, 1);

    if (index < 0 || index >= numlayers) {
        char msg[64];
        sprintf(msg, "setlayer error: bad index (%d)", index);
        GollyError(L, msg);
    }
    
    SetLayer(index);
    DoAutoUpdate();
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_getlayer(lua_State* L)
{
    CheckEvents(L);

    lua_pushinteger(L, currindex);
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_numlayers(lua_State* L)
{
    CheckEvents(L);

    lua_pushinteger(L, numlayers);
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_maxlayers(lua_State* L)
{
    CheckEvents(L);

    lua_pushinteger(L, MAX_LAYERS);
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_setname(lua_State* L)
{
    CheckEvents(L);
    
    const char* name = luaL_checkstring(L, 1);
    int index = currindex;
    if (lua_gettop(L) > 1) index = luaL_checkinteger(L, 2);

    if (index < 0 || index >= numlayers) {
        char msg[64];
        sprintf(msg, "setname error: bad index (%d)", index);
        GollyError(L, msg);
    }
    
    GSF_setname((char*) name, index);
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_getname(lua_State* L)
{
    CheckEvents(L);

    int index = currindex;
    if (lua_gettop(L) > 0) index = luaL_checkinteger(L, 1);
    
    if (index < 0 || index >= numlayers) {
        char msg[64];
        sprintf(msg, "getname error: bad index (%d)", index);
        GollyError(L, msg);
    }
    
    lua_pushstring(L, GetLayer(index)->currname.mb_str(wxConvUTF8));
    
    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

#define CheckRGB(r,g,b,cmd)                                                 \
    if (r < 0 || r > 255 || g < 0 || g > 255 || g < 0 || g > 255) {         \
        char msg[128];                                                      \
        sprintf(msg, "%s error: bad rgb value (%d,%d,%d)", cmd, r, g, b);   \
        GollyError(L, msg);                                                 \
    }

// -----------------------------------------------------------------------------

static int g_setcolors(lua_State* L)
{
    CheckEvents(L);
    
    luaL_checktype(L, 1, LUA_TTABLE);
    
    int len = luaL_len(L, 1);
    if (len == 0) {
        // restore default colors in current layer and its clones
        UpdateLayerColors();
    } else if (len == 6) {
        // create gradient from r1,g1,b1 to r2,g2,b2
        lua_rawgeti(L, 1, 1); int r1 = lua_tointeger(L,-1); lua_pop(L,1);
        lua_rawgeti(L, 1, 2); int g1 = lua_tointeger(L,-1); lua_pop(L,1);
        lua_rawgeti(L, 1, 3); int b1 = lua_tointeger(L,-1); lua_pop(L,1);
        lua_rawgeti(L, 1, 4); int r2 = lua_tointeger(L,-1); lua_pop(L,1);
        lua_rawgeti(L, 1, 5); int g2 = lua_tointeger(L,-1); lua_pop(L,1);
        lua_rawgeti(L, 1, 6); int b2 = lua_tointeger(L,-1); lua_pop(L,1);
        CheckRGB(r1, g1, b1, "setcolors");
        CheckRGB(r2, g2, b2, "setcolors");
        currlayer->fromrgb.Set(r1, g1, b1);
        currlayer->torgb.Set(r2, g2, b2);
        CreateColorGradient();
        UpdateIconColors();
        UpdateCloneColors();
    } else if (len % 4 == 0) {
        int i = 0;
        while (i < len) {
            lua_rawgeti(L, 1, ++i); int s = lua_tointeger(L,-1); lua_pop(L,1);
            lua_rawgeti(L, 1, ++i); int r = lua_tointeger(L,-1); lua_pop(L,1);
            lua_rawgeti(L, 1, ++i); int g = lua_tointeger(L,-1); lua_pop(L,1);
            lua_rawgeti(L, 1, ++i); int b = lua_tointeger(L,-1); lua_pop(L,1);
            CheckRGB(r, g, b, "setcolors");
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
                    sprintf(msg, "setcolors error: bad state (%d)", s);
                    GollyError(L, msg);
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
        GollyError(L, "setcolors error: list length is not a multiple of 4.");
    }
    
    DoAutoUpdate();
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_getcolors(lua_State* L)
{
    CheckEvents(L);

    int state = -1;
    if (lua_gettop(L) > 0) state = luaL_checkinteger(L, 1);
    
    lua_newtable(L);
    
    if (state == -1) {
        // return colors for ALL states, including state 0
        int tindex = 1;
        for (state = 0; state < currlayer->algo->NumCellStates(); state++) {
            lua_pushinteger(L, state);                   lua_rawseti(L, -2, tindex++);
            lua_pushinteger(L, currlayer->cellr[state]); lua_rawseti(L, -2, tindex++);
            lua_pushinteger(L, currlayer->cellg[state]); lua_rawseti(L, -2, tindex++);
            lua_pushinteger(L, currlayer->cellb[state]); lua_rawseti(L, -2, tindex++);
        }
    } else if (state >= 0 && state < currlayer->algo->NumCellStates()) {
        lua_pushinteger(L, state);                   lua_rawseti(L, -2, 1);
        lua_pushinteger(L, currlayer->cellr[state]); lua_rawseti(L, -2, 2);
        lua_pushinteger(L, currlayer->cellg[state]); lua_rawseti(L, -2, 3);
        lua_pushinteger(L, currlayer->cellb[state]); lua_rawseti(L, -2, 4);
    } else {
        char msg[64];
        sprintf(msg, "getcolors error: bad state (%d)", state);
        GollyError(L, msg);
    }
    
    return 1;   // result is a table
}

// -----------------------------------------------------------------------------

static int g_setoption(lua_State* L)
{
    CheckEvents(L);

    const char* optname = luaL_checkstring(L, 1);
    int newval = luaL_checkinteger(L, 2);
    
    int oldval;
    if (!GSF_setoption((char*) optname, newval, &oldval)) {
        GollyError(L, "setoption error: unknown option.");
    }
    
    // return old value (simplifies saving and restoring settings)
    lua_pushinteger(L, oldval);
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_getoption(lua_State* L)
{
    CheckEvents(L);

    const char* optname = luaL_checkstring(L, 1);

    int optval;
    if (!GSF_getoption((char*) optname, &optval)) {
        GollyError(L, "getoption error: unknown option.");
    }
    
    lua_pushinteger(L, optval);
    
    return 1;   // result is an integer
}

// -----------------------------------------------------------------------------

static int g_setcolor(lua_State* L)
{
    CheckEvents(L);

    const char* colname = luaL_checkstring(L, 1);
    int r = luaL_checkinteger(L, 2);
    int g = luaL_checkinteger(L, 3);
    int b = luaL_checkinteger(L, 4);
    
    wxColor newcol(r, g, b);
    wxColor oldcol;
    
    if (!GSF_setcolor((char*) colname, newcol, oldcol)) {
        GollyError(L, "setcolor error: unknown color.");
    }
    
    // return old r,g,b values (simplifies saving and restoring colors)
    lua_pushinteger(L, oldcol.Red());
    lua_pushinteger(L, oldcol.Green());
    lua_pushinteger(L, oldcol.Blue());

    return 3;   // result is 3 ints
}

// -----------------------------------------------------------------------------

static int g_getcolor(lua_State* L)
{
    CheckEvents(L);

    const char* colname = luaL_checkstring(L, 1);

    wxColor color;
    if (!GSF_getcolor((char*) colname, color)) {
        GollyError(L, "getcolor error: unknown color.");
    }
    
    // return r,g,b values
    lua_pushinteger(L, color.Red());
    lua_pushinteger(L, color.Green());
    lua_pushinteger(L, color.Blue());

    return 3;   // result is 3 ints
}

// -----------------------------------------------------------------------------

static int g_setclipstr(lua_State* L)
{
    CheckEvents(L);
    
    const char* clipstr = luaL_checkstring(L, 1);
    mainptr->CopyTextToClipboard(wxString(clipstr,wxConvLocal));
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_getclipstr(lua_State* L)
{
    CheckEvents(L);

    wxTextDataObject data;
    if (!mainptr->GetTextFromClipboard(&data)) {
        GollyError(L, "getclipstr error: no text in clipboard.");
    }
    
    wxString clipstr = data.GetText();
    lua_pushstring(L, clipstr.mb_str(wxConvUTF8));
    
    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_getstring(lua_State* L)
{
    CheckEvents(L);

    const char* prompt = luaL_checkstring(L, 1);
    const char* initial = lua_gettop(L) > 1 ? luaL_checkstring(L, 2) : "";
    const char* title = lua_gettop(L) > 2 ? luaL_checkstring(L, 3) : "";
    
    wxString result;
    if (!GetString(wxString(title,wxConvLocal),
                   wxString(prompt,wxConvLocal),
                   wxString(initial,wxConvLocal),
                   result)) {
        // user hit Cancel button so abort script
        lua_pushstring(L, abortmsg);
        lua_error(L);
    }
    
    lua_pushstring(L, result.mb_str(wxConvUTF8));
    
    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_getxy(lua_State* L)
{
    CheckEvents(L);

    statusptr->CheckMouseLocation(mainptr->infront);   // sets mousepos
    if (viewptr->showcontrols) mousepos = wxEmptyString;
    
    lua_pushstring(L, mousepos.mb_str(wxConvUTF8));
    
    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_getevent(lua_State* L)
{
    CheckEvents(L);

    int get = 1;
    if (lua_gettop(L) > 0) get = lua_toboolean(L, 1) ? 1 : 0;
    
    wxString event;
    GSF_getevent(event, get);
    
    lua_pushstring(L, event.mb_str(wxConvUTF8));
    
    return 1;   // result is a string
}

// -----------------------------------------------------------------------------

static int g_doevent(lua_State* L)
{
    CheckEvents(L);

    const char* event = luaL_checkstring(L, 1);
    
    if (event[0]) {
        const char* err = GSF_doevent(wxString(event,wxConvLocal));
        if (err) GollyError(L, err);
    }
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_show(lua_State* L)
{
    CheckEvents(L);
    
    const char* s = luaL_checkstring(L, 1);

    inscript = false;
    statusptr->DisplayMessage(wxString(s,wxConvLocal));
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
    statusptr->ErrorMessage(wxString(s,wxConvLocal));
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
        
    Warning(wxString(s,wxConvLocal));
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_note(lua_State* L)
{
    CheckEvents(L);
    
    const char* s = luaL_checkstring(L, 1);
    Note(wxString(s,wxConvLocal));
    
    return 0;   // no result
}

// -----------------------------------------------------------------------------

static int g_help(lua_State* L)
{
    CheckEvents(L);
    
    const char* htmlfile = luaL_checkstring(L, 1);
    ShowHelp(wxString(htmlfile,wxConvLocal));
    
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
        GSF_exit((char*) luaL_checkstring(L, 1));
    }
    
    lua_pushstring(L, abortmsg);
    lua_error(L);
    
    return 0;   // never get here (lua_error does a longjmp)
}

// -----------------------------------------------------------------------------

static const struct luaL_Reg gollyfuncs [] = {
    // filing
    { "open",         g_open },         // open given pattern/rule/html file
    { "save",         g_save },         // save pattern in given file using given format
    { "opendialog",   g_opendialog },   // return input path and filename chosen by user
    { "savedialog",   g_savedialog },   // return output path and filename chosen by user
    { "load",         g_load },         // read pattern file and return cell array
    { "store",        g_store },        // write cell array to a file (in RLE format)
    { "setdir",       g_setdir },       // set location of specified directory
    { "getdir",       g_getdir },       // return location of specified directory
    // editing
    { "new",          g_new },          // create new universe and set window title
    { "cut",          g_cut },          // cut selection to clipboard
    { "copy",         g_copy },         // copy selection to clipboard
    { "clear",        g_clear },        // clear inside/outside selection
    { "paste",        g_paste },        // paste clipboard pattern at x,y using given mode
    { "shrink",       g_shrink },       // shrink selection
    { "randfill",     g_randfill },     // randomly fill selection to given percentage
    { "flip",         g_flip },         // flip selection top-bottom or left-right
    { "rotate",       g_rotate },       // rotate selection 90 deg clockwise or anticlockwise
    { "parse",        g_parse },        // parse RLE or Life 1.05 string and return cell array
    { "transform",    g_transform },    // apply an affine transformation to cell array
    { "evolve",       g_evolve },       // generate pattern contained in given cell array
    { "putcells",     g_putcells },     // paste given cell array into current universe
    { "getcells",     g_getcells },     // return cell array in given rectangle
    { "join",         g_join },         // return concatenation of given cell arrays
    { "hash",         g_hash },         // return hash value for pattern in given rectangle
    { "getclip",      g_getclip },      // return pattern in clipboard (as cell array)
    { "select",       g_select },       // select {x, y, wd, ht} rectangle or remove if {}
    { "getrect",      g_getrect },      // return pattern rectangle as {} or {x, y, wd, ht}
    { "getselrect",   g_getselrect },   // return selection rectangle as {} or {x, y, wd, ht}
    { "setcell",      g_setcell },      // set given cell to given state
    { "getcell",      g_getcell },      // get state of given cell
    { "setcursor",    g_setcursor },    // set cursor (returns old cursor)
    { "getcursor",    g_getcursor },    // return current cursor
    // control
    { "empty",        g_empty },        // return true if universe is empty
    { "run",          g_run },          // run current pattern for given number of gens
    { "step",         g_step },         // run current pattern for current step
    { "setstep",      g_setstep },      // set step exponent
    { "getstep",      g_getstep },      // return current step exponent
    { "setbase",      g_setbase },      // set base step
    { "getbase",      g_getbase },      // return current base step
    { "advance",      g_advance },      // advance inside/outside selection by given gens
    { "reset",        g_reset },        // restore starting pattern
    { "setgen",       g_setgen },       // set current generation to given string
    { "getgen",       g_getgen },       // return current generation as string
    { "getpop",       g_getpop },       // return current population as string
    { "numstates",    g_numstates },    // return number of cell states in current universe
    { "numalgos",     g_numalgos },     // return number of algorithms
    { "setalgo",      g_setalgo },      // set current algorithm using given string
    { "getalgo",      g_getalgo },      // return name of given or current algorithm
    { "setrule",      g_setrule },      // set current rule using given string
    { "getrule",      g_getrule },      // return current rule
    { "getwidth",     g_getwidth },     // return width of universe (0 if unbounded)
    { "getheight",    g_getheight },    // return height of universe (0 if unbounded)
    // viewing
    { "setpos",       g_setpos },       // move given cell to middle of viewport
    { "getpos",       g_getpos },       // return x,y position of cell in middle of viewport
    { "setmag",       g_setmag },       // set magnification (0=1:1, 1=1:2, -1=2:1, etc)
    { "getmag",       g_getmag },       // return current magnification
    { "fit",          g_fit },          // fit entire pattern in viewport
    { "fitsel",       g_fitsel },       // fit selection in viewport
    { "visrect",      g_visrect },      // return true if given rect is completely visible
    { "update",       g_update },       // update display (viewport and status bar)
    { "autoupdate",   g_autoupdate },   // update display after each change to universe?
    // layers
    { "addlayer",     g_addlayer },     // add a new layer
    { "clone",        g_clone },        // add a cloned layer (shares universe)
    { "duplicate",    g_duplicate },    // add a duplicate layer (copies universe)
    { "dellayer",     g_dellayer },     // delete current layer
    { "movelayer",    g_movelayer },    // move given layer to new index
    { "setlayer",     g_setlayer },     // switch to given layer
    { "getlayer",     g_getlayer },     // return index of current layer
    { "numlayers",    g_numlayers },    // return current number of layers
    { "maxlayers",    g_maxlayers },    // return maximum number of layers
    { "setname",      g_setname },      // set name of given layer
    { "getname",      g_getname },      // get name of given layer
    { "setcolors",    g_setcolors },    // set color(s) used in current layer
    { "getcolors",    g_getcolors },    // get color(s) used in current layer
    // miscellaneous
    { "setoption",    g_setoption },    // set given option to new value (and return old value)
    { "getoption",    g_getoption },    // return current value of given option
    { "setcolor",     g_setcolor },     // set given color to new r,g,b (returns old r,g,b)
    { "getcolor",     g_getcolor },     // return r,g,b values of given color
    { "setclipstr",   g_setclipstr },   // set the clipboard contents to a given string value
    { "getclipstr",   g_getclipstr },   // retrieve the contents of the clipboard as a string
    { "getstring",    g_getstring },    // display dialog box and get string from user
    { "getxy",        g_getxy },        // return current grid location of mouse
    { "getevent",     g_getevent },     // return keyboard/mouse event or empty string if none
    { "doevent",      g_doevent },      // pass given keyboard/mouse event to Golly to handle
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
        scripterr += wxString(lua_tostring(L, -1),wxConvLocal);
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
