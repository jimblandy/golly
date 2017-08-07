// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef _WXINFO_H_
#define _WXINFO_H_

// Routines for displaying comments in a pattern file:

// Open a modeless window and display the comments in given file.
void ShowInfo(const wxString& filepath);

// Return a pointer to the info window.
wxFrame* GetInfoFrame();

#endif
