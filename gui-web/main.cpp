/*** /

 This file is part of Golly, a Game of Life Simulator.
 Copyright (C) 2013 Andrew Trevorrow and Tomas Rokicki.

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

// gollybase
#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "jvnalgo.h"
#include "generationsalgo.h"
#include "ruleloaderalgo.h"

// gui-common
#include "algos.h"      // for InitAlgorithms, NumAlgos, etc
#include "prefs.h"      // for GetPrefs, MAX_MAG, tempdir, etc
#include "layer.h"      // for ResizeLayers, currlayer
#include "control.h"    // for SetMinimumStepExponent, NextGeneration, etc
#include "file.h"       // for NewPattern
#include "view.h"       // for fullscreen, TouchBegan, etc
#include "status.h"     // for SetMessage, CheckMouseLocation, etc
#include "utils.h"      // for Beep, etc
#include "undo.h"       // for currlayer->undoredo->...
#include "render.h"     // for InitOGLES2, DrawPattern

#include "webcalls.h"   // for UpdateStatus, refresh_pattern, etc

#include <stdlib.h>
#include <stdio.h>
#include <GL/glfw.h>
#include <emscripten/emscripten.h>

// -----------------------------------------------------------------------------

// the following JavaScript routines are implemented in jslib.js:

extern "C" {
    extern void jsSetMode(int index);
    extern void jsSetState(int state);
    extern const char* jsSetRule(const char* oldrule);
    extern void jsShowMenu(const char* id, int x, int y);
    extern int jsTextAreaIsActive();
}

// -----------------------------------------------------------------------------

static int currwd, currht;              // current size of viewport (in pixels)
static double last_time;                // time when NextGeneration was last called

static bool alt_down = false;           // alt/option key is currently pressed?
static bool ctrl_down = false;          // ctrl key is currently pressed?
static bool shift_down = false;         // shift key is currently pressed?
static bool meta_down = false;          // cmd/start/menu key is currently pressed?

static bool ok_to_check_mouse = false;
static bool mouse_down = false;
static bool paste_menu_visible = false;
static bool selection_menu_visible = false;

// -----------------------------------------------------------------------------

static void InitPaths()
{
    userdir = "/UserData/";     // can't save user data???!!!

    savedir = userdir + "Saved/";
    //???!!! CreateSubdir(savedir);

    downloaddir = userdir + "Downloads/";
    //???!!! CreateSubdir(downloaddir);

    userrules = "";             // ???!!! userdir + "Rules/";
    //???!!! CreateSubdir(userrules);

    // supplied patterns, rules, help are stored in golly.data via --preload-file option in Makefile
    supplieddir = "/";
    patternsdir = supplieddir + "Patterns/";
    rulesdir = supplieddir + "Rules/";
    helpdir = supplieddir + "Help/";

    tempdir = "";
    clipfile = tempdir + "golly_clipboard";
    prefsfile = "GollyPrefs";                   // where will this be saved???
}

// -----------------------------------------------------------------------------

static void InitEventHandlers()
{
    // the following code fixes bugs in emscripten/src/library_glfw.js:
    // - onMouseWheel fails to use wheelDelta
    // - the onmousewheel handler is assigned to the entire window rather than just the canvas
    // - onKeyChanged always calls event.preventDefault() so browser shortcuts like ctrl-Q/X/C/V
    //   don't work and text can't be typed into clipboard textarea
    
    EM_ASM(
        var wheelpos = 0;
        function on_mouse_wheel(event) {
            // Firefox sets event.detail, other browsers set event.wheelDelta with opposite sign,
            // so set delta to a value between -1 and 1
            var delta = Math.max(-1, Math.min(1, (event.detail || -event.wheelDelta)));
            wheelpos += delta;
            _OnMouseWheel(wheelpos);
            return false;
        };
        // for Firefox:
        Module['canvas'].addEventListener('DOMMouseScroll', on_mouse_wheel, false);
        // for Chrome, Safari, etc:
        Module['canvas'].onmousewheel = on_mouse_wheel;
    );
    
    EM_ASM(
        // also do our own keyboard event handling
        function on_key_changed(event, status) {
            var key = event.keyCode;
            // DEBUG: Module.printErr('keycode='+key+' status='+status);
            // DEBUG: Module.printErr('activeElement='+document.activeElement.tagName);
            var prevent = _OnKeyChanged(key, status);
            // we allow default handler in these 2 cases:
            // 1. if ctrl/meta key is down (allows cmd/ctrl-Q/X/C/V/A/etc to work)
            // 2. if a textarea is active (document.activeElement.tagName == 'TEXTAREA')
            if (prevent) {
                event.preventDefault();
                // DEBUG: Module.printErr('preventDefault called');
                return false;
            }
        };
        function on_key_down(event) {
            on_key_changed(event, 1);   // GLFW_PRESS
        };
        function on_key_up(event) {
            on_key_changed(event, 0);   // GLFW_RELEASE
        };
        // can't seem to assign key event handlers to canvas, so use window
        window.addEventListener("keydown", on_key_down, true);
        window.addEventListener("keyup", on_key_up, true);
    );
}

// -----------------------------------------------------------------------------

static int InitGL()
{
    if (glfwInit() != GL_TRUE) {
        Warning("glfwInit failed!");
        return GL_FALSE;
    }
    InitEventHandlers();

    // initial size doesn't matter -- ResizeCanvas will soon change it
    if (glfwOpenWindow(100, 100, 8, 8, 8, 8, 0, 0, GLFW_WINDOW) != GL_TRUE) {
        Warning("glfwOpenWindow failed!");
        return GL_FALSE;
    }
    glfwSetWindowTitle("Golly");
    
    if (!InitOGLES2()) {
        Warning("InitOGLES2 failed!");
        return GL_FALSE;
    }

    // we only do 2D drawing
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_DITHER);
    glDisable(GL_STENCIL_TEST);
    // following 2 cause WebGL warnings on Chrome and Firefox:
    // glDisable(GL_FOG);
    // glDisable(GL_MULTISAMPLE);

    glEnable(GL_BLEND);
    // this blending function seems similar to the one used in desktop Golly
    // (ie. selected patterns look much the same)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(1.0, 1.0, 1.0, 1.0);

    last_time = glfwGetTime();
    return GL_TRUE;
}

// -----------------------------------------------------------------------------

// many of the following routines are declared as C functions to avoid
// C++ name mangling and make it easy to call them from JavaScript code
// (the routine names, along with a leading underscore, must be listed in
// -s EXPORTED_FUNCTIONS in Makefile)

extern "C" {

void ResizeCanvas() {
    // resize canvas based on current window dimensions
    EM_ASM(
        var trect = document.getElementById('toolbar').getBoundingClientRect();
        // place canvas immediately under toolbar, extending to bottom edge of window
        var top = trect.top + trect.height;
        var left = trect.left;
        var wd = window.innerWidth - left;
        var ht = window.innerHeight - top;
        if (wd < 0) wd = 0;
        if (ht < 0) ht = 0;
        // ensure wd and ht are integer multiples of max cell size so rendering code
        // will draw partially visible cells at the right and bottom edges
        if (wd % 32 > 0) wd += 32 - (wd % 32);
        if (ht % 32 > 0) ht += 32 - (ht % 32);
        var canvas = Module['canvas'];
        canvas.style.top = top + 'px';
        canvas.style.left = left + 'px';
        canvas.style.width = wd + 'px';
        canvas.style.height = ht + 'px';
        _SetViewport(wd, ht);               // call C routine (see below)
    );
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void SetViewport(int width, int height)
{
    // ResizeCanvas has changed canvas size so we need to change OpenGL's viewport size
    glfwSetWindowSize(width, height);
    glViewport(0, 0, width, height);
    currwd = width;
    currht = height;
    if (currwd != currlayer->view->getwidth() ||
        currht != currlayer->view->getheight()) {
        // also change size of Golly's viewport
        ResizeLayers(currwd, currht);
        // to avoid seeing lots of black, draw now rather than call UpdatePattern
        DrawPattern(currindex);
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

static void InitElements()
{
    // note that checkbox ids must match those in shell.html
    
    if (showgridlines) {
        EM_ASM( document.getElementById('grid').checked = true; );
    } else {
        EM_ASM( document.getElementById('grid').checked = false; );
    }
    
    if (showicons) {
        EM_ASM( document.getElementById('icons').checked = true; );
    } else {
        EM_ASM( document.getElementById('icons').checked = false; );
    }
    
    if (showtiming) {
        EM_ASM( document.getElementById('time').checked = true; );
    } else {
        EM_ASM( document.getElementById('time').checked = false; );
    }

    // also initialize clipboard data to a simple RLE pattern
    EM_ASM(
        document.getElementById('cliptext').value =
            '# To paste in this RLE pattern, hit\n'+
            '# the Paste button, drag the floating\n' +
            '# image to the desired location, then\n' +
            '# right-click on it to see some options.\n' +
            'x = 9, y = 5, rule = B3/S23\n' +
            '$bo3b3o$b3o2bo$2bo!';
    );
}

// -----------------------------------------------------------------------------

static void StopIfGenerating()
{
    if (generating) {
        StopGenerating();
        // generating flag is now false so change button label to "Start"
        EM_ASM( Module.setButtonLabel('startStop', 'Start'); );
    }
}

// -----------------------------------------------------------------------------

extern "C" {

void NewUniverse()
{
    // undo/redo history is about to be cleared so no point calling RememberGenFinish
    // if we're generating a (possibly large) pattern
    bool saveundo = allowundo;
    allowundo = false;
    StopIfGenerating();
    allowundo = saveundo;
    
    if (event_checker > 0) {
        // try again after a short delay!!!???
        return;
    }
    
    NewPattern();
    UpdateEverything();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void StartStop()
{
    if (generating) {
        StopGenerating();
        // generating flag is now false so change button label to "Start"
        EM_ASM( Module.setButtonLabel('startStop', 'Start'); );
    } else if (StartGenerating()) {
        // generating flag is now true so change button label to "Stop"
        EM_ASM( Module.setButtonLabel('startStop', 'Stop'); );
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void Next()
{
    StopIfGenerating();
    
    if (event_checker > 0) {
        // previous NextGeneration() hasn't finished
        // try again after a short delay!!!???
        return;
    }

    NextGeneration(false);       // advance by 1
    UpdatePatternAndStatus();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void Step()
{
    StopIfGenerating();
    
    if (event_checker > 0) {
        // previous NextGeneration() hasn't finished
        // try again after a short delay!!!???
        return;
    }

    NextGeneration(true);       // advance by current step size
    UpdatePatternAndStatus();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void GoSlower()
{
    if (currlayer->currexpo > minexpo) {
        currlayer->currexpo--;
        SetGenIncrement();
        UpdateStatus();
    } else {
        Beep();
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void GoFaster()
{
    currlayer->currexpo++;
    SetGenIncrement();
    UpdateStatus();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void StepBy1()
{
    // reset step exponent to 0
    currlayer->currexpo = 0;
    SetGenIncrement();
    UpdateStatus();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void Reset()
{
    if (currlayer->algo->getGeneration() == currlayer->startgen) return;

    StopIfGenerating();
    
    if (event_checker > 0) {
        // try again after a short delay!!!???
        return;
    }
    
    ResetPattern();
    UpdateEverything();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void Fit()
{
    FitInView(1);
    UpdatePatternAndStatus();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void ZoomOut()
{
    currlayer->view->unzoom();
    UpdatePatternAndStatus();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void ZoomIn()
{
    if (currlayer->view->getmag() < MAX_MAG) {
        currlayer->view->zoom();
        UpdatePatternAndStatus();
    } else {
        Beep();
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void Scale1to1()
{
    if (currlayer->view->getmag() != 0) {
        currlayer->view->setmag(0);
        UpdatePatternAndStatus();
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void Help()
{
    // do something else eventually!!!
    EM_ASM(
        alert('You can use these keyboard commands:\n\n' +
              'return -- start/stop generating\n' +
              'space -- do 1 generation\n' +
              '- or _ -- go slower\n' +
              '+ or = -- go faster\n' +
              '0 -- set step exponent to 0\n' +
              '1 -- set scale to 1:1\n' +
              '[ -- zoom out\n' +
              '] -- zoom in\n' +
              'a -- select all\n' +
              'f -- fit\n' +
              'h -- help\n' +
              'i -- toggle icon mode\n' +
              'n -- new (empty) universe\n' +
              'r -- reset\n' +
              'R -- random pattern\n' +
              'v -- paste\n' +
              'V -- cancel paste\n' +
              'z -- undo\n' +
              'Z -- redo\n' +
              'arrow keys -- scrolling'
             );
    );
}

} // extern "C"

// -----------------------------------------------------------------------------

static void ChangePrefs()
{
    //!!! show some sort of modal dialog box that lets user change various settings???
    
    SavePrefs();    // where will this write GollyPrefs file???!!!
}

// -----------------------------------------------------------------------------

static void RandomPattern()
{
    NewUniverse();
    
    // following hack is needed because we use shift-R to call RandomPattern,
    // so we want ToggleCursorMode to restore drawmode when shift key is released
    if (shift_down) currlayer->touchmode = pickmode;
    
    currlayer->currsel.SetRect(-10, -10, 20, 20);
    currlayer->currsel.RandomFill();
    currlayer->currsel.Deselect();
    UpdateEverything();
}

// -----------------------------------------------------------------------------

extern "C" {

void Paste()
{
    StopIfGenerating();
    
    if (event_checker > 0) {
        // try again after a short delay!!!???
        return;
    }
    
    // remove any existing paste image
    if (waitingforpaste) AbortPaste();

    PasteClipboard();
    UpdatePatternAndStatus();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void Undo()
{
    StopIfGenerating();
    
    if (event_checker > 0) {
        // try again after a short delay!!!???
        return;
    }
    
    currlayer->undoredo->UndoChange();
    UpdateEverything();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void Redo()
{
    StopIfGenerating();
    
    if (event_checker > 0) {
        // try again after a short delay!!!???
        return;
    }
    
    currlayer->undoredo->RedoChange();
    UpdateEverything();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void ToggleGrid()
{
    showgridlines = !showgridlines;
    if (showgridlines) {
        EM_ASM( document.getElementById('grid').checked = true; );
    } else {
        EM_ASM( document.getElementById('grid').checked = false; );
    }
    UpdatePattern();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void ToggleIcons()
{
    showicons = !showicons;
    if (showicons) {
        EM_ASM( document.getElementById('icons').checked = true; );
    } else {
        EM_ASM( document.getElementById('icons').checked = false; );
    }
    UpdatePattern();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void ToggleTiming()
{
    showtiming = !showtiming;
    if (showtiming) {
        EM_ASM( document.getElementById('time').checked = true; );
    } else {
        EM_ASM( document.getElementById('time').checked = false; );
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void AlgoChanged(int index)
{
    if (index >= 0 && index < NumAlgos()) {
        ChangeAlgorithm(index, currlayer->algo->getrule());
    } else {
        Warning("Bug detected in AlgoChanged!");
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void ModeChanged(int index)
{
    switch (index) {
        case 0: currlayer->touchmode = drawmode;    return;
        case 1: currlayer->touchmode = pickmode;    return;
        case 2: currlayer->touchmode = selectmode;  return;
        case 3: currlayer->touchmode = movemode;    return;
        case 4: currlayer->touchmode = zoominmode;  return;
        case 5: currlayer->touchmode = zoomoutmode; return;
    }
    Warning("Bug detected in ModeChanged!");
}

} // extern "C"

// -----------------------------------------------------------------------------

static void ToggleCursorMode()
{
    // state of shift key has changed so may need to toggle cursor mode
    if (currlayer->touchmode == drawmode) {
        currlayer->touchmode = pickmode;
        jsSetMode(currlayer->touchmode);
    } else if (currlayer->touchmode == pickmode) {
        currlayer->touchmode = drawmode;
        jsSetMode(currlayer->touchmode);
    } else if (currlayer->touchmode == zoominmode) {
        currlayer->touchmode = zoomoutmode;
        jsSetMode(currlayer->touchmode);
    } else if (currlayer->touchmode == zoomoutmode) {
        currlayer->touchmode = zoominmode;
        jsSetMode(currlayer->touchmode);
    }
}

// -----------------------------------------------------------------------------

extern "C" {

void DecState()
{
    if (currlayer->drawingstate > 0) {
        currlayer->drawingstate--;
        jsSetState(currlayer->drawingstate);
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void IncState()
{
    if (currlayer->drawingstate < currlayer->algo->NumCellStates() - 1) {
        currlayer->drawingstate++;
        jsSetState(currlayer->drawingstate);
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void SetRule()
{
    StopIfGenerating();
    std::string newrule = jsSetRule(currlayer->algo->getrule());
    // newrule is empty if given rule was invalid
    if (newrule.length() > 0) ChangeRule(newrule);
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void PasteAction(int item)
{
    // remove menu first
    EM_ASM( document.getElementById('pastemenu').style.visibility = 'hidden'; );
    paste_menu_visible = false;

    switch (item) {
        case 0:  AbortPaste(); break;
        case 1:  DoPaste(false); break;
        case 2:  DoPaste(true); break;
        case 3:  FlipPastePattern(true); break;
        case 4:  FlipPastePattern(false); break;
        case 5:  RotatePastePattern(true); break;
        case 6:  RotatePastePattern(false); break;
        default: Warning("Bug detected in PasteAction!");
    }
    UpdateEverything();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void SelectionAction(int item)
{
    // remove menu first
    EM_ASM( document.getElementById('selectionmenu').style.visibility = 'hidden'; );
    selection_menu_visible = false;

    if (generating && item >= 1 && item <= 13 &&
        item != 2 && item != 5 && item != 6) {
        // temporarily stop generating for all actions except Remove, Copy, Shrink, Fit
        PauseGenerating();
    }
    switch (item) {
        case 0:  RemoveSelection(); break;                      // WARNING: above test assumes Remove is item 0
        case 1:  CutSelection(); break;
        case 2:  CopySelection(); break;                        // WARNING: above test assumes Copy is item 2
        case 3:  ClearSelection(); break;
        case 4:  ClearOutsideSelection(); break;
        case 5:  ShrinkSelection(false); break;                 // WARNING: above test assumes Shrink is item 5
        case 6:  FitSelection(); break;                         // WARNING: above test assumes Fit is item 6
        case 7:  RandomFill(); break;
        case 8:  FlipSelection(true); break;
        case 9:  FlipSelection(false); break;
        case 10: RotateSelection(true); break;
        case 11: RotateSelection(false); break;
        case 12: currlayer->currsel.Advance(); break;
        case 13: currlayer->currsel.AdvanceOutside(); break;    // WARNING: above test assumes 13 is last item
        default: Warning("Bug detected in SelectionAction!");
    }
    ResumeGenerating();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void ClearStatus()
{
    ClearMessage();
}

} // extern "C"

// -----------------------------------------------------------------------------

extern "C" {

void OpenClickedFile(const char* filepath)
{
    StopIfGenerating();
    OpenFile(filepath);
}

} // extern "C"

// -----------------------------------------------------------------------------

static const int META_KEY = 666;  // cmd key on Mac, start/menu key on Windows (latter untested!!!)

static int TranslateKey(int keycode)
{
    // this is a modified version of DOMToGLFWKeyCode in emscripten/src/library_glfw.js
    switch (keycode) {
        // my changes (based on testing and info at http://unixpapa.com/js/key.html)
        case 224 : return META_KEY;     // cmd key on Firefox (Mac)
        case 91  : return META_KEY;     // left cmd key on Safari, Chrome (Mac)
        case 93  : return META_KEY;     // right cmd key on Safari, Chrome (Mac)
        case 92  : return META_KEY;     // right start key on Firefox, IE (Windows)
        case 219 : return '[';          // Firefox, Chrome and Safari (Mac)
        case 220 : return '\\';         // Firefox, Chrome and Safari (Mac)
        case 221 : return ']';          // Firefox, Chrome and Safari (Mac)
        case 173 : return '-';          // Firefox (Mac)
        case 189 : return '-';          // Chrome and Safari (Mac)
        case 187 : return '=';          // Chrome and Safari (Mac)
        
        case 0x09: return 295 ; //DOM_VK_TAB -> GLFW_KEY_TAB
        case 0x1B: return 255 ; //DOM_VK_ESCAPE -> GLFW_KEY_ESC
        case 0x6A: return 313 ; //DOM_VK_MULTIPLY -> GLFW_KEY_KP_MULTIPLY
        case 0x6B: return 315 ; //DOM_VK_ADD -> GLFW_KEY_KP_ADD
        case 0x6D: return 314 ; //DOM_VK_SUBTRACT -> GLFW_KEY_KP_SUBTRACT
        case 0x6E: return 316 ; //DOM_VK_DECIMAL -> GLFW_KEY_KP_DECIMAL
        case 0x6F: return 312 ; //DOM_VK_DIVIDE -> GLFW_KEY_KP_DIVIDE
        case 0x70: return 258 ; //DOM_VK_F1 -> GLFW_KEY_F1
        case 0x71: return 259 ; //DOM_VK_F2 -> GLFW_KEY_F2
        case 0x72: return 260 ; //DOM_VK_F3 -> GLFW_KEY_F3
        case 0x73: return 261 ; //DOM_VK_F4 -> GLFW_KEY_F4
        case 0x74: return 262 ; //DOM_VK_F5 -> GLFW_KEY_F5
        case 0x75: return 263 ; //DOM_VK_F6 -> GLFW_KEY_F6
        case 0x76: return 264 ; //DOM_VK_F7 -> GLFW_KEY_F7
        case 0x77: return 265 ; //DOM_VK_F8 -> GLFW_KEY_F8
        case 0x78: return 266 ; //DOM_VK_F9 -> GLFW_KEY_F9
        case 0x79: return 267 ; //DOM_VK_F10 -> GLFW_KEY_F10
        case 0x7a: return 268 ; //DOM_VK_F11 -> GLFW_KEY_F11
        case 0x7b: return 269 ; //DOM_VK_F12 -> GLFW_KEY_F12
        case 0x25: return 285 ; //DOM_VK_LEFT -> GLFW_KEY_LEFT
        case 0x26: return 283 ; //DOM_VK_UP -> GLFW_KEY_UP
        case 0x27: return 286 ; //DOM_VK_RIGHT -> GLFW_KEY_RIGHT
        case 0x28: return 284 ; //DOM_VK_DOWN -> GLFW_KEY_DOWN
        case 0x21: return 298 ; //DOM_VK_PAGE_UP -> GLFW_KEY_PAGEUP
        case 0x22: return 299 ; //DOM_VK_PAGE_DOWN -> GLFW_KEY_PAGEDOWN
        case 0x24: return 300 ; //DOM_VK_HOME -> GLFW_KEY_HOME
        case 0x23: return 301 ; //DOM_VK_END -> GLFW_KEY_END
        case 0x2d: return 296 ; //DOM_VK_INSERT -> GLFW_KEY_INSERT
        case 16  : return 287 ; //DOM_VK_SHIFT -> GLFW_KEY_LSHIFT
        case 0x05: return 287 ; //DOM_VK_LEFT_SHIFT -> GLFW_KEY_LSHIFT
        case 0x06: return 288 ; //DOM_VK_RIGHT_SHIFT -> GLFW_KEY_RSHIFT
        case 17  : return 289 ; //DOM_VK_CONTROL -> GLFW_KEY_LCTRL
        case 0x03: return 289 ; //DOM_VK_LEFT_CONTROL -> GLFW_KEY_LCTRL
        case 0x04: return 290 ; //DOM_VK_RIGHT_CONTROL -> GLFW_KEY_RCTRL
        case 18  : return 291 ; //DOM_VK_ALT -> GLFW_KEY_LALT
        case 0x02: return 291 ; //DOM_VK_LEFT_ALT -> GLFW_KEY_LALT
        case 0x01: return 292 ; //DOM_VK_RIGHT_ALT -> GLFW_KEY_RALT
        case 96  : return 302 ; //GLFW_KEY_KP_0
        case 97  : return 303 ; //GLFW_KEY_KP_1
        case 98  : return 304 ; //GLFW_KEY_KP_2
        case 99  : return 305 ; //GLFW_KEY_KP_3
        case 100 : return 306 ; //GLFW_KEY_KP_4
        case 101 : return 307 ; //GLFW_KEY_KP_5
        case 102 : return 308 ; //GLFW_KEY_KP_6
        case 103 : return 309 ; //GLFW_KEY_KP_7
        case 104 : return 310 ; //GLFW_KEY_KP_8
        case 105 : return 311 ; //GLFW_KEY_KP_9
        default  : return keycode;
    };
}

// -----------------------------------------------------------------------------

extern "C" {

int OnKeyChanged(int keycode, int action)
{
    int key = TranslateKey(keycode);

    if (action == GLFW_PRESS) ClearMessage();
    
    // first check for modifier keys (meta/ctrl/alt/shift);
    // note that we need to set flags for these keys BEFORE testing
    // jsTextAreaIsActive so users can do things like ctrl-click in canvas while
    // a textarea has focus and OnMouseClick will be able to test ctrl_down flag
    
    if (key == META_KEY) {
        meta_down = action == GLFW_PRESS;
        return 0;   // don't call preventDefault
    } else if (key == GLFW_KEY_LCTRL || key == GLFW_KEY_RCTRL) {
        ctrl_down = action == GLFW_PRESS;
        return 0;   // don't call preventDefault
    } else if (key == GLFW_KEY_LALT || key == GLFW_KEY_RALT) {
        alt_down = action == GLFW_PRESS;
        return 1;
    } else if (key == GLFW_KEY_LSHIFT || key == GLFW_KEY_RSHIFT) {
        bool oldshift = shift_down;
        shift_down = action == GLFW_PRESS;
        if (oldshift != shift_down) ToggleCursorMode();
        return 1;
    }
    
    if (jsTextAreaIsActive()) {
        // a textarea is active (and probably has focus),
        // so don't handle the key here and don't call preventDefault
        return 0;
    }
    
    if (meta_down || ctrl_down) {
        // could be a browser shortcut like ctrl/cmd-Q/X/C/V,
        // so don't handle the key here and don't call preventDefault
        return 0;
    }

    if (action == GLFW_RELEASE) return 1;   // non-modifier key was released
    
    // a non-modifer key is down (and meta/ctrl key is NOT currently down)
    
    // check for arrow keys and do panning
    if (key == GLFW_KEY_UP) {
        if (shift_down)
            PanNE();
        else
            PanUp( SmallScroll(currlayer->view->getheight()) );
        return 1;
    } else if (key == GLFW_KEY_DOWN) {
        if (shift_down)
            PanSW();
        else
            PanDown( SmallScroll(currlayer->view->getheight()) );
        return 1;
    } else if (key == GLFW_KEY_LEFT) {
        if (shift_down)
            PanNW();
        else
            PanLeft( SmallScroll(currlayer->view->getwidth()) );
        return 1;
    } else if (key == GLFW_KEY_RIGHT) {
        if (shift_down)
            PanSE();
        else
            PanRight( SmallScroll(currlayer->view->getwidth()) );
        return 1;
    }
    
    int ch = key;
    if (key >= 'A' && key <= 'Z' && !shift_down) {
        // convert to lowercase
        ch = ch + 32;
    }
    
    switch (ch) {
        case 13  : StartStop(); break;
        case ' ' : Next(); break;
        case '_' : GoSlower(); break;
        case '-' : GoSlower(); break;
        case '+' : GoFaster(); break;
        case '=' : GoFaster(); break;
        case '0' : StepBy1(); break;
        case '1' : Scale1to1(); break;
        case '[' : ZoomOut(); break;
        case ']' : ZoomIn(); break;
        case 'a' : SelectAll(); break;
        case 'f' : Fit(); break;
        case 'h' : Help(); break;
        case 'i' : ToggleIcons(); break;
        case 'n' : NewUniverse(); break;
        case 'p' : ChangePrefs(); break;
        case 'r' : Reset(); break;
        case 'R' : RandomPattern(); break;
        case 'v' : Paste(); break;
        case 'V' : AbortPaste(); UpdatePattern(); break;
        case 'z' : Undo(); break;
        case 'Z' : Redo(); break;
    }
    
    return 1;   // call preventDefault
}

} // extern "C"

// -----------------------------------------------------------------------------

static void OnMouseClick(int button, int action)
{
    ok_to_check_mouse = true;
    if (action == GLFW_PRESS) {
        // make sure a textarea element no longer has focus (for testing in on_key_changed);
        // note that 'patterns' is a div with a tabindex, and an outline style that prevents
        // a focus ring appearing
        EM_ASM( document.getElementById('patterns').focus(); );
        
        int x, y;
        glfwGetMousePos(&x, &y);
        // DEBUG: EM_ASM( Module.printErr('click at x='+x+' y='+y); );
        
        ClearMessage();
        
        if (paste_menu_visible) {
            EM_ASM( document.getElementById('pastemenu').style.visibility = 'hidden'; );
            paste_menu_visible = false;
            return;
        }
        if (selection_menu_visible) {
            EM_ASM( document.getElementById('selectionmenu').style.visibility = 'hidden'; );
            selection_menu_visible = false;
            return;
        }
        
        // test for ctrl/right click in paste image or selection;
        // button test should be for GLFW_MOUSE_BUTTON_RIGHT which is defined to be 1 in glfw.h
        // but I actually get 2 when right button is pressed in all my browsers (report bug!!!)
        if (button == 2 || ctrl_down) {
            if (waitingforpaste && PointInPasteImage(x, y)) {
                jsShowMenu("pastemenu", x, y);
                paste_menu_visible = true;
            } else if (SelectionExists() && PointInSelection(x, y)) {
                jsShowMenu("selectionmenu", x, y);
                selection_menu_visible = true;
            }
            return;
        }
        
        // check for click outside viewport
        if (x < 0 || x >= currwd || y < 0 || y >= currht) {
            if (mouse_down) TouchEnded();
            mouse_down = false;
            return;
        }
        
        TouchBegan(x, y);
        mouse_down = true;
    
    } else if (action == GLFW_RELEASE) {
        if (mouse_down) TouchEnded();
        mouse_down = false;
    }
}

// -----------------------------------------------------------------------------

static void OnMouseMove(int x, int y)
{
    ok_to_check_mouse = true;
    int mousestate = glfwGetMouseButton(GLFW_MOUSE_BUTTON_LEFT);
    if (mousestate == GLFW_PRESS) {
        // DEBUG: EM_ASM( Module.printErr('moved to x='+x+' y='+y); );
        
        // ignore move outside viewport
        if (x < 0 || x >= currwd || y < 0 || y >= currht) return;
    
        TouchMoved(x, y);
    }
}

// -----------------------------------------------------------------------------

static int prevpos = 0;

extern "C" {

void OnMouseWheel(int pos)
{
    int x, y;
    glfwGetMousePos(&x, &y);
    
    // we use a threshold of 2 in below tests to reduce sensitivity
    if (pos + 2 < prevpos) {
        ZoomInPos(x, y);
        prevpos = pos;
    } else if (pos - 2 > prevpos) {
        ZoomOutPos(x, y);
        prevpos = pos;
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

static void DoFrame()
{
    if (generating && event_checker == 0) {
        if (currlayer->currexpo < 0) {
            // get current delay (in secs)
            float delay = GetCurrentDelay() / 1000.0;
            double current_time = glfwGetTime();
            // check if it's time to call NextGeneration
            if (current_time - last_time >= delay) {
                NextGeneration(true);
                UpdatePatternAndStatus();
                last_time = current_time;
            }
        } else {
            NextGeneration(true);
            UpdatePatternAndStatus();
        }
    }
    
    if (refresh_pattern) {
        refresh_pattern = false;
        DrawPattern(currindex);
    }
    
    glfwSwapBuffers();

    // check the current mouse location continuously, but only after the 1st mouse-click or
    // mouse-move event, because until then glfwGetMousePos returns 0,0 (report bug???!!!)
    if (ok_to_check_mouse) {
        int x, y;
        glfwGetMousePos(&x, &y);
        CheckMouseLocation(x, y);
    }
}

// -----------------------------------------------------------------------------

// we need the EMSCRIPTEN_KEEPALIVE flag to avoid our main() routine disappearing
// (a somewhat strange consequence of using -s EXPORTED_FUNCTIONS in Makefile)

int EMSCRIPTEN_KEEPALIVE main()
{
    SetMessage("This is Golly 0.5 for the web.  Copyright 2014 The Golly Gang.");
    InitPaths();                // init tempdir, prefsfile, etc
    MAX_MAG = 5;                // maximum cell size = 32x32
    InitAlgorithms();           // must initialize algoinfo first
    GetPrefs();                 // load user's preferences
    SetMinimumStepExponent();   // for slowest speed
    AddLayer();                 // create initial layer (sets currlayer)
    NewPattern();               // create new, empty universe
    UpdateStatus();             // show initial message

    InitElements();             // initialize checkboxes and other document elements

    if (InitGL() == GL_TRUE) {
        ResizeCanvas();
        // we do our own keyboard event handling (see InitEventHandlers)
        // glfwSetKeyCallback(OnKeyChanged);
        glfwSetMouseButtonCallback(OnMouseClick);
        glfwSetMousePosCallback(OnMouseMove);
        // we do our own mouse wheel handling (see InitEventHandlers)
        // glfwSetMouseWheelCallback(OnMouseWheel);
        emscripten_set_main_loop(DoFrame, 0, 1);
    }

    glfwTerminate();
    return 0;
}
