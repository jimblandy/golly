// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef _STATUS_H_
#define _STATUS_H_

#include "bigint.h"

#include <string>       // for std::string

// The status bar area consists of 3 lines of text:

extern std::string status1;		// top line
extern std::string status2;		// middle line
extern std::string status3;		// bottom line

void UpdateStatusLines();
// set status1 and status2 (SetMessage sets status3)

void ClearMessage();
// erase bottom line of status bar

void DisplayMessage(const char* s);
// display given message on bottom line of status bar

void ErrorMessage(const char* s);
// beep and display given message on bottom line of status bar

void SetMessage(const char* s);
// set status3 without displaying it (until next update)

int GetCurrentDelay();
// return current delay (in millisecs)

char* Stringify(const bigint& b);
// convert given number to string suitable for display

void CheckMouseLocation(int x, int y);
// on devices with a mouse we might need to update the
// cursor's current XY cell location, where the given x,y
// values are the cursor's viewport coordinates (in pixels)

#endif
