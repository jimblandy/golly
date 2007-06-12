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
#ifndef _WXSCRIPT_H_
#define _WXSCRIPT_H_

extern bool inscript;
// Is a script currently running?  We allow access to global flag
// so clients can temporarily save and restore its setting.

extern bool canswitch;
// Can user switch layers while a script is running?

bool IsScript(const wxString& filename);
// Return true if the given file is a Perl or Python script.
// It simply checks if the file's extension is ".pl" or ".py"
// (ignoring case).

void RunScript(const wxString& filename);
// Run the given Perl or Python script.

void PassKeyToScript(int key);
// Called if a script is running and user hits a key.

void ShowTitleLater();
// Called if a script is running and window title has changed.

void FinishScripting();
// Called when app quits to abort a running script.

// Following are used in wxperl.cpp and wxpython.cpp:

extern bool autoupdate;       // update display after changing current universe?
extern bool allowcheck;       // allow event checking?
extern wxString scripterr;    // Perl/Python error message

const char abortmsg[] = "GOLLY: ABORT SCRIPT";
// special message used to indicate that the script was aborted

void DoAutoUpdate();          // update display if autoupdate is true

// The following Golly Script Functions are used to reduce code duplication.
// They are called by corresponding pl_* and py_* functions in wxperl.cpp
// and wxpython.cpp respectively.

const char* GSF_open(char* filename, int remember);
const char* GSF_save(char* filename, char* format, int remember);
const char* GSF_setrule(char* rulestring);
void GSF_setname(char* name, int index);
bool GSF_setoption(char* optname, int newval, int* oldval);
bool GSF_getoption(char* optname, int* optval);
bool GSF_setcolor(char* colname, wxColor& newcol, wxColor& oldcol);
bool GSF_getcolor(char* colname, wxColor& color);
void GSF_getkey(char* s);
void GSF_dokey(char* ascii);
void GSF_update();
void GSF_exit(char* errmsg);

#endif
