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

#ifndef _STATUS_H_
#define _STATUS_H_

#include "bigint.h"

#include <string>       // for std::string

// The status bar area consists of 3 lines of text:

extern std::string status1;		// top line
extern std::string status2;		// middle line
extern std::string status3;		// bottom line

void UpdateStatusLines();
// set status1 and status2 (SetMessage sets status3)

void ClearMessage();
// erase bottom line of status bar

void DisplayMessage(const char* s);
// display given message on bottom line of status bar

void ErrorMessage(const char* s);
// beep and display given message on bottom line of status bar

void SetMessage(const char* s);
// set status3 without displaying it (until next update)

int GetCurrentDelay();
// return current delay (in millisecs)

char* Stringify(const bigint& b);
// convert given number to string suitable for display

#endif
