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
#ifndef _WXSCRIPT_H_
#define _WXSCRIPT_H_

// Return true if the given file is a Python script.
// It simply checks if the file's extension is ".py" (ignoring case).
bool IsScript(const char *filename);

// Run the given script file by passing it to the Python interpreter.
void RunScript(const char* filename);

// Is a script currently running?  We allow access to global flag
// so clients can temporarily save and restore its setting.
extern bool inscript;

// Called if a script is running and user hits a key.
void PassKeyToScript(int key);

// Called when app quits to abort a running script and tidy up
// the Python interpreter.
void FinishScripting();

#endif
