// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef _JNICALLS_H_
#define _JNICALLS_H_

#include <string>       // for std::string

// for displaying info/error messages in LogCat
#include <android/log.h>
#define LOG_TAG "Golly"
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

// Routines for calling C++ from Java and vice versa:

void UpdatePattern();
// Redraw the current pattern.

void UpdateStatus();
// Redraw the status bar info.

void PauseGenerating();
// If pattern is generating then temporarily pause.

void ResumeGenerating();
// Resume generating pattern if it was paused.

std::string GetRuleName(const std::string& rule);
// Return name of given rule (empty string if rule is unnamed).

void UpdateEditBar();
// Update Undo and Redo buttons, show current drawing state and touch mode.

void BeginProgress(const char* title);
bool AbortProgress(double fraction_done, const char* message);
void EndProgress();
// These calls display a progress bar while a lengthy task is carried out.

void SwitchToPatternTab();
// Switch to main screen for displaying/editing/generating patterns.

void ShowTextFile(const char* filepath);
// Display contents of given text file in a modal view.

void ShowHelp(const char* filepath);
// Display given HTML file in Help screen.

void AndroidWarning(const char* msg);
// Beep and display message in a modal dialog.

bool AndroidYesNo(const char* msg);
// Similar to Warning, but there are 2 buttons: Yes and No.
// Returns true if Yes button is hit.

void AndroidFatal(const char* msg);
// Beep, display message in a modal dialog, then exit app.

void AndroidBeep();
// Play beep sound, depending on user setting.

void AndroidRemoveFile(const std::string& filepath);
// Delete given file.

bool AndroidMoveFile(const std::string& inpath, const std::string& outpath);
// Return true if input file is successfully moved to output file.
// If the output file existed it is replaced.

void AndroidFixURLPath(std::string& path);
// Replace "%..." with suitable chars for a file path (eg. %20 is changed to space).

void AndroidCheckEvents();
// Run main UI thread for a short time so app remains responsive while doing a
// lengthy computation.  Note that event_checker is > 0 while in this routine.

bool AndroidCopyTextToClipboard(const char* text);
// Copy given text to the clipboard.

bool AndroidGetTextFromClipboard(std::string& text);
// Get text from the clipboard.

void AndroidDownloadFile(const std::string& url, const std::string& filepath);
// Download given url and create given file.

#endif
