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

#ifndef _JNICALLS_H_
#define _JNICALLS_H_

#include <string>       // for std::string

// for displaying info/error messages in LogCat
#include <android/log.h>
#define LOG_TAG "Golly"
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

// Routines for calling C++ from Java and vice versa.

void UpdatePattern();
// redraw the current pattern

void UpdateStatus();
// redraw the status bar info

void PauseGenerating();
// if pattern is generating then temporarily pause

void ResumeGenerating();
// resume generating pattern if it was paused

std::string GetRuleName(const std::string& rule);
// return name of given rule (empty string if rule is unnamed)

void UpdateEditBar();
// update Undo and Redo buttons, show current drawing state and touch mode

void BeginProgress(const char* title);
bool AbortProgress(double fraction_done, const char* message);
void EndProgress();
// these calls display a progress bar while a lengthy task is carried out

void SwitchToPatternTab();
// switch to pattern view (MainActivity)

void ShowTextFile(const char* filepath);
// display contents of text file in modal view (TextFileActivity!!!)

void ShowHelp(const char* filepath);
// display html file in help view (HelpActivity!!!)

#endif
