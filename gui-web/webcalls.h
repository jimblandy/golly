// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef _WEBCALLS_H_
#define _WEBCALLS_H_

#include <string>   // for std::string

// Web-specific routines (called mainly from gui-common code):

extern bool refresh_pattern;    // DoFrame in main.cpp should call DrawPattern?

void UpdatePattern();
// Redraw the current pattern (actually just sets refresh_pattern to true).

void UpdateStatus();
// Redraw the status bar info.

void PauseGenerating();
// If pattern is generating then temporarily pause.

void ResumeGenerating();
// Resume generating pattern if it was paused.

std::string GetRuleName(const std::string& rule);
// Return name of given rule (empty string if rule is unnamed).

void UpdateButtons();
// Some buttons might need to be enabled/disabled.

void UpdateEditBar();
// Update buttons, show current drawing state and cursor mode.

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

void WebWarning(const char* msg);
// Beep and display message in a modal dialog.

bool WebYesNo(const char* msg);
// Similar to Warning, but there are 2 buttons: Yes and No.
// Returns true if Yes button is hit.

void WebFatal(const char* msg);
// Beep, display message in a modal dialog, then exit app.

void WebBeep();
// Play beep sound, depending on allowbeep setting.

void WebRemoveFile(const std::string& filepath);
// Delete given file.

bool WebMoveFile(const std::string& inpath, const std::string& outpath);
// Return true if input file is successfully moved to output file.
// If the output file existed it is replaced.

void WebFixURLPath(std::string& path);
// Replace "%..." with suitable chars for a file path (eg. %20 is changed to space).

bool WebCopyTextToClipboard(const char* text);
// Copy given text to the clipboard.

bool WebGetTextFromClipboard(std::string& text);
// Get text from the clipboard.

bool PatternSaved(std::string& filename);
// Return true if current pattern is saved in virtual file system using given
// filename (which will have default extension appended if none supplied).

bool WebSaveChanges();
// Show a modal dialog that lets user save their changes.
// Return true if it's ok to continue.

bool WebDownloadFile(const std::string& url, const std::string& filepath);
// Download given url and create given file.

void WebCheckEvents();
// Run main UI thread for a short time so app remains responsive while doing a
// lengthy computation.  Note that event_checker is > 0 while in this routine.

void CopyRuleToLocalStorage(const char* rulepath);
// Copy contents of given .rule file to HTML5's localStorage (using rulepath as
// the key) so that the file can be re-created in the next webGolly session.

#endif
