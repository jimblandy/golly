// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef _WXSCRIPT_H_
#define _WXSCRIPT_H_

#include "lifealgo.h"   // for lifealgo class

extern bool inscript;
// Is a script currently running?  We allow access to this flag
// so clients can temporarily save and restore its setting.

extern bool pass_key_events;
extern bool pass_mouse_events;
extern bool pass_file_events;
// Pass keyboard/mouse/file events to script?  All flags are initially false
// at the start of a script; all become true if the script calls getevent().
// If the script calls getkey (deprecated) then only pass_key_events becomes true.

extern bool canswitch;          // can user switch layers while a script is running?
extern bool stop_after_script;  // stop generating pattern after running script?
extern bool autoupdate;         // update display after changing current universe?
extern bool allowcheck;         // allow event checking?
extern bool showprogress;       // script can display the progress dialog?
extern wxString scripterr;      // error message
extern wxString mousepos;       // current mouse position
extern wxString scripttitle;    // window title set by settitle command
extern wxString rle3path;       // path of .rle3 file to be sent to 3D.lua

void RunScript(const wxString& filename);
// Run the given script.

void PassOverlayClickToScript(int ox, int oy, int button, int modifiers);
// Called if a script is running and user clicks mouse in overlay
// at the given pixel location.

void PassClickToScript(const bigint& x, const bigint& y, int button, int modifiers);
// Called if a script is running and user clicks mouse in grid
// at the given cell location.

void PassMouseUpToScript(int button);
// Called if a script is running and user releases a mouse button.

void PassZoomInToScript(int x, int y);
// Called if a script is running and mouse wheel is used to zoom in.

void PassZoomOutToScript(int x, int y);
// Called if a script is running and mouse wheel is used to zoom out.

void PassKeyToScript(int key, int modifiers = 0);
// Called if a script is running and user hits a key.
// Can also be used to abort a script by passing WXK_ESCAPE.

void PassKeyUpToScript(int key);
// Called if a script is running and user releases a key.

void PassFileToScript(const wxString& filepath);
// Called if a script is running and user opens a file.

void ShowTitleLater();
// Called if a script is running and window title has changed.

void ChangeCell(int x, int y, int oldstate, int newstate);
// A setcell/putcells command is changing state of cell at x,y.

void SavePendingChanges(bool checkgenchanges = true);
// Called to save any pending cell changes made by ChangeCell calls.
// If checkgenchanges is true then it will also save any pending
// generating changes.  This lets us accumulate consecutive cell/gen
// changes in a single undo/redo change node, which is better for
// scripts like invert.py that can call setcell() many times, or
// for scripts like oscar.py that can call run() many times.
// Must be called BEFORE all undoredo->Remember... calls but
// only if inscript && allowundo && !currlayer->stayclean.

void FinishScripting();
// Called when app quits to abort a running script.

const char abortmsg[] = "GOLLY: ABORT SCRIPT";
// special message used to indicate that the script was aborted

void DoAutoUpdate();          // update display if autoupdate is true

// The following Golly Script Functions are used to reduce code duplication.
// They are called by corresponding functions in wxlua/wxperl/wxpython.cpp.

const char* GSF_open(const wxString& filename, int remember);
const char* GSF_save(const wxString& filename, const char* format, int remember);
const char* GSF_setdir(const char* dirname, const wxString& newdir);
const char* GSF_getdir(const char* dirname);
const char* GSF_os();
const char* GSF_setalgo(const char* algostring);
const char* GSF_setrule(const char* rulestring);
const char* GSF_setgen(const char* genstring);
const char* GSF_setpos(const char* x, const char* y);
const char* GSF_setcell(int x, int y, int newstate);
const char* GSF_paste(int x, int y, const char* mode);
const char* GSF_checkpos(lifealgo* algo, int x, int y);
const char* GSF_checkrect(int x, int y, int wd, int ht);
int GSF_hash(int x, int y, int wd, int ht);
bool GSF_setoption(const char* optname, int newval, int* oldval);
bool GSF_getoption(const char* optname, int* optval);
bool GSF_setcolor(const char* colname, wxColor& newcol, wxColor& oldcol);
bool GSF_getcolor(const char* colname, wxColor& color);
void GSF_setname(const wxString& name, int index);
void GSF_select(int x, int y, int wd, int ht);
void GSF_getevent(wxString& event, int get);
const char* GSF_doevent(const wxString& event);
char GSF_getkey();
void GSF_dokey(const char* ascii);
void GSF_update();
void GSF_exit(const wxString& errmsg);
const char* GSF_getpath();
const char* GSF_getinfo();

#endif
