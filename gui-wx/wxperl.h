// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef _WXPERL_H_
#define _WXPERL_H_

void RunPerlScript(const wxString& filepath);
// Run the given .pl file.

void AbortPerlScript();
// Abort the currently running Perl script.

void FinishPerlScripting();
// Called when app is quitting.

#endif
