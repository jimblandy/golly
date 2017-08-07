// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef _WXRULE_H_
#define _WXRULE_H_

bool ChangeRule();
// Open a dialog that lets the user change the current rule.
// Return true if the rule change succeeds.  Note that the
// current algorithm might also change.

wxString GetRuleName(const wxString& rulestring);
// If given rule has a name then return name, otherwise return the rule.

#endif
