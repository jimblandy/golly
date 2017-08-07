// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef _WXLUA_H_
#define _WXLUA_H_

void RunLuaScript(const wxString& filepath);
// Run the given .lua file.

void AbortLuaScript();
// Abort the currently running Lua script.

void FinishLuaScripting();
// Called when app is quitting.

#endif
