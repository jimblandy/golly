// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef _WXPYTHON_H_
#define _WXPYTHON_H_

void RunPythonScript(const wxString& filepath);
// Run the given .py file.

void AbortPythonScript();
// Abort the currently running Python script.

void FinishPythonScripting();
// Called when app is quitting.

#endif
