                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2010 Andrew Trevorrow and Tomas Rokicki.

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

wxFrame* GetHelpFrame();
// Return a pointer to the help window.

#endif
