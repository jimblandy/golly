                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2008 Andrew Trevorrow and Tomas Rokicki.

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
#ifndef _WXUTILS_H_
#define _WXUTILS_H_

// Various utility routines:

void Note(const wxString& msg);
// Display given message in a modal dialog.

void Warning(const wxString& msg);
// Beep and display message in a modal dialog.

void Fatal(const wxString& msg);
// Beep, display message in a modal dialog, then exit app.

bool GetString(const wxString& title, const wxString& prompt,
               const wxString& instring, wxString& outstring);
// Display a dialog box to get a string from the user.
// Returns false if user hits Cancel button.

bool GetInteger(const wxString& title, const wxString& prompt,
                int inval, int minval, int maxval, int* outval);
// Display a dialog box to get an integer value from the user.
// Returns false if user hits Cancel button.

int SaveChanges(const wxString& query, const wxString& msg);
// Ask user if changes should be saved and return following result:
// 2 if user selects Yes/Save button,
// 1 if user selects No/Don't Save button,
// 0 if user selects Cancel button.

void BeginProgress(const wxString& dlgtitle);
// Call at the start of a lengthy task.  The cursor changes to indicate
// the app is busy but the progress dialog won't appear immediately.

bool AbortProgress(double fraction_done, const wxString& newmsg);
// Call frequently while the task is being carried out.  The progress
// dialog only appears if the task is likely to take more than a few secs.
// Pass in a fraction from 0.0 to 1.0 indicating how much has been done.
// The given string can be used to display extra information.
// The call returns true if the user cancels the progress dialog.

void EndProgress();
// Call when the task has finished (even if it was aborted).

void FillRect(wxDC& dc, wxRect& rect, wxBrush& brush);
// Fill given rectangle using given brush.

void CreatePaleBitmap(const wxBitmap& inmap, wxBitmap& outmap);
// Create a pale gray version of given bitmap.

#endif
