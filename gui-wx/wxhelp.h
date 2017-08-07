// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef _WXHELP_H_
#define _WXHELP_H_

// Routines for displaying html help files stored in the Help folder:

void ShowHelp(const wxString& filepath);
// Open a modeless window and display the given html file.
// If filepath is empty then either the help window is brought to the
// front if it's open, or it is opened and the most recent html file
// is displayed.

const wxString SHOW_KEYBOARD_SHORTCUTS = wxT("keyboard.html");
// If ShowHelp is called with this string then a temporary HTML file
// is created to show the user's current keyboard shortcuts.

void ShowAboutBox();
// Open a modal dialog and display info about the app.

void LoadLexiconPattern();
// Load the lexicon pattern clicked by user.

void LoadRule(const wxString& rulestring, bool fromfile = true);
// Load given rule from a .rule file if fromfile is true,
// otherwise switch to given rule specified in a "rule:" link.

wxFrame* GetHelpFrame();
// Return a pointer to the help window.

#endif
