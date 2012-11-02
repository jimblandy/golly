/*** /
 
 This file is part of Golly, a Game of Life Simulator.
 Copyright (C) 2012 Andrew Trevorrow and Tomas Rokicki.
 
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

#ifndef _UTILS_H_
#define _UTILS_H_

class lifepoll;

#include <string>       // for std::string

// Various types and utility routines:

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} gColor;               // a color in RGB space

typedef struct {
    int x;
    int y;
    int width;
    int height;
} gRect;                // a rectangle

void SetColor(gColor& color, unsigned char red, unsigned char green, unsigned char blue);
// Set given gColor to given RGB values.

void SetRect(gRect& rect, int x, int y, int width, int height);
// Set given gRect to given location and size.

void Warning(const char* msg);
// Beep and display message in a modal dialog.

bool YesNo(const char* msg);
// Similar to Warning, but there are 2 buttons: Yes and No.
// Returns true if Yes button is hit.

void Fatal(const char* msg);
// Beep, display message in a modal dialog, then exit app.

void Beep();
// Play beep sound, depending on user setting.

double TimeInSeconds();
// Get time of day, in seconds (accuracy in microsecs).

std::string CreateTempFileName(const char* prefix);
// Return path to a unique temporary file.

bool FileExists(const std::string& filepath);
// Does given file exist?

void RemoveFile(const std::string& filepath);
// Delete given file.

bool CopyFile(const std::string& inpath, const std::string& outpath);
// Return true if input file is successfully copied to output file.

void FixURLPath(std::string& path);
// Replace any "%20" with a space.

bool IsHTMLFile(const std::string& filename);
// Return true if the given file's extension is .htm or .html
// (ignoring case).

bool IsTextFile(const std::string& filename);
// Return true if the given file's extension is .txt or .doc,
// or if it's not a HTML file and its name contains "readme"
// (ignoring case).

bool IsZipFile(const std::string& filename);
// Return true if the given file's extension is .zip or .gar
// (ignoring case).

bool IsRuleFile(const std::string& filename);
// Return true if the given file is a rule-related file with
// an extension of .rule or .table or .tree or .colors or .icons
// (ignoring case).

bool IsScriptFile(const std::string& filename);
// Return true if the given file is a Perl or Python script.
// It simply checks if the file's extension is .pl or .py
// (ignoring case).

lifepoll* Poller();
void PollerReset();
void PollerInterrupt();
extern int event_checker;
// Poller is used by gollybase modules to process events.
// If event_checker > 0 then we've been called from the event checking code.

#endif
