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

#include <jni.h>
#include <GLES/gl.h>
#include <unistd.h>		// for usleep

#include "utils.h"		// for Warning, etc
#include "algos.h"		// for InitAlgorithms
#include "prefs.h"      // for GetPrefs, SavePrefs
#include "layer.h"      // for AddLayer, ResizeLayers, currlayer
#include "control.h"    // for SetMinimumStepExponent
#include "file.h"       // for NewPattern
#include "render.h"     // for DrawPattern
#include "view.h"       // for TouchBegan, etc
#include "status.h"     // for UpdateStatusLines, ClearMessage, etc
#include "undo.h"       // for ClearUndoRedo
#include "jnicalls.h"

// -----------------------------------------------------------------------------

// these globals cache info that doesn't change during execution
static JavaVM* javavm;
static jobject mainobj;
static jmethodID id_RefreshPattern;
static jmethodID id_ShowStatusLines;
static jmethodID id_UpdateEditBar;
static jmethodID id_CheckMessageQueue;
static jmethodID id_CopyTextToClipboard;
static jmethodID id_GetTextFromClipboard;

static bool rendering = false;	// in DrawPattern?
static bool paused = false;		// generating has been paused?

// -----------------------------------------------------------------------------

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
	// LOGI("JNI_OnLoad");
    JNIEnv* env;
    javavm = vm;	// save in global

    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
    	LOGE("GetEnv failed!");
        return -1;
    }

    return JNI_VERSION_1_6;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeClassInit(JNIEnv* env, jclass klass)
{
    // save IDs for Java methods in MainActivity
	id_RefreshPattern = env->GetMethodID(klass, "RefreshPattern", "()V");
    id_ShowStatusLines = env->GetMethodID(klass, "ShowStatusLines", "()V");
    id_UpdateEditBar = env->GetMethodID(klass, "UpdateEditBar", "()V");
    id_CheckMessageQueue = env->GetMethodID(klass, "CheckMessageQueue", "()V");
    id_CopyTextToClipboard = env->GetMethodID(klass, "CopyTextToClipboard", "(Ljava/lang/String;)V");
    id_GetTextFromClipboard = env->GetMethodID(klass, "GetTextFromClipboard", "()Ljava/lang/String;");
}

// -----------------------------------------------------------------------------

static JNIEnv* getJNIenv(bool* attached)
{
	// get the JNI environment for the current thread
	JNIEnv* env;
	*attached = false;

    int result = javavm->GetEnv((void **) &env, JNI_VERSION_1_6);
    if (result == JNI_EDETACHED) {
        if (javavm->AttachCurrentThread(&env, NULL) != 0) {
            LOGE("AttachCurrentThread failed!");
            return NULL;
        }
        *attached = true;
    } else if (result == JNI_EVERSION) {
    	LOGE("GetEnv: JNI_VERSION_1_6 is not supported!");
    	return NULL;
    }

	return env;
}

// -----------------------------------------------------------------------------

static std::string ConvertJString(JNIEnv* env, jstring str)
{
   const jsize len = env->GetStringUTFLength(str);
   const char* strChars = env->GetStringUTFChars(str, 0);
   std::string result(strChars, len);
   env->ReleaseStringUTFChars(str, strChars);
   return result;
}

// -----------------------------------------------------------------------------

void UpdateStatus()
{
	UpdateStatusLines();	// sets status1, status2, status3

	// call Java method in MainActivity class to update the status bar info
	bool attached;
	JNIEnv* env = getJNIenv(&attached);
    if (env) env->CallVoidMethod(mainobj, id_ShowStatusLines);
	if (attached) javavm->DetachCurrentThread();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT jstring JNICALL Java_net_sf_golly_MainActivity_nativeGetStatusLine(JNIEnv* env, jobject obj, jint line)
{
    switch (line) {
    	case 1: return env->NewStringUTF(status1.c_str());
    	case 2: return env->NewStringUTF(status2.c_str());
    	case 3: return env->NewStringUTF(status3.c_str());
    }
    return env->NewStringUTF("Fix bug in nativeGetStatusLine!");
}

// -----------------------------------------------------------------------------

void PauseGenerating()
{
	if (generating) {
		StopGenerating();
		// generating is now false
		paused = true;
	}
}

// -----------------------------------------------------------------------------

void ResumeGenerating()
{
	if (paused) {
		StartGenerating();
		// generating is probably true (false if pattern is empty)
		paused = false;
	}
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeCreate(JNIEnv* env, jobject obj)
{
    // LOGI("nativeCreate");

	// save obj for calling Java methods in this instance of MainActivity
    mainobj = env->NewGlobalRef(obj);

    static bool firstcall = true;
    if (firstcall) {
    	firstcall = false;
    	std::string msg = "This is Golly version ";
    	msg += GOLLY_VERSION;
    	msg += " for Android.  Copyright 2013 The Golly Gang.";
    	SetMessage(msg.c_str());
    	MAX_MAG = 5;                // maximum cell size = 32x32
    	InitAlgorithms();           // must initialize algoinfo first
    	GetPrefs();                 // load user's preferences
    	SetMinimumStepExponent();   // for slowest speed
    	AddLayer();                 // create initial layer (sets currlayer)
    	NewPattern();               // create new, empty universe
    	UpdateStatus();				// show initial message
    	showtiming = true; //!!! test
    	showicons = true; //!!! test
    }
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeDestroy(JNIEnv* env)
{
    // LOGI("nativeDestroy");

/* Android apps aren't really meant to quit so best not to do this stuff:

    // avoid nativeResume restarting pattern generation
	paused = false;
	generating = false;

	if (currlayer->dirty) {
		// ask if changes should be saved
		//!!!
	}

    SavePrefs();

    // clear all undo/redo history for this layer
    currlayer->undoredo->ClearUndoRedo();

    if (numlayers > 1) DeleteOtherLayers();
    delete currlayer;

    // reset some globals so we can call AddLayer again in nativeCreate
    numlayers = 0;
    numclones = 0;
    currlayer = NULL;

    // delete stuff created by InitAlgorithms()
    DeleteAlgorithms();

	// reset some static info so we can call InitAlgorithms again
	staticAlgoInfo::nextAlgoId = 0;
	staticAlgoInfo::head = NULL;

*/

	// the current instance of MainActivity is being destroyed
    env->DeleteGlobalRef(mainobj);
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeSetGollyDir(JNIEnv* env, jobject obj, jstring path)
{
	gollydir = ConvertJString(env, path) + "/";
	// LOGI("gollydir = %s", gollydir.c_str());
	// gollydir = /data/data/pkg.name/files/

	// set paths to various subdirs (created by caller)
	userrules = gollydir + "Rules/";
	datadir = gollydir + "Saved/";
	downloaddir = gollydir + "Downloads/";
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeSetTempDir(JNIEnv* env, jobject obj, jstring path)
{
	tempdir = ConvertJString(env, path) + "/";
	// LOGI("tempdir = %s", tempdir.c_str());
	// tempdir = /data/data/pkg.name/cache/
	clipfile = tempdir + "golly_clipboard";
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeSetSuppliedDirs(JNIEnv* env, jobject obj, jstring path)
{
	std::string prefix = ConvertJString(env, path);
	// LOGI("prefix = %s", prefix.c_str());
	// prefix = /data/data/pkg.name/app_
	helpdir = prefix + "Help/";
	rulesdir = prefix + "Rules/";
	patternsdir = prefix + "Patterns/";
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT bool JNICALL Java_net_sf_golly_MainActivity_nativeAllowUndo(JNIEnv* env)
{
	return allowundo;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT bool JNICALL Java_net_sf_golly_MainActivity_nativeCanUndo(JNIEnv* env)
{
	return currlayer->undoredo->CanUndo();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT bool JNICALL Java_net_sf_golly_MainActivity_nativeCanRedo(JNIEnv* env)
{
	return currlayer->undoredo->CanRedo();
}

// -----------------------------------------------------------------------------

static void CheckIfRendering()
{
    while (rendering) {
    	// wait for DrawPattern to finish
    	// LOGI("waiting for DrawPattern to finish");
    	usleep(1000);	// 1 millisec
    }
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeUndo(JNIEnv* env)
{
	if (generating) StopGenerating();
    if (event_checker > 0) {
        // try again after a short delay
        //!!! CallAfterDelay("Undo");
        return;
    }

    ClearMessage();
    CheckIfRendering();
    currlayer->undoredo->UndoChange();
    UpdateEverything();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeRedo(JNIEnv* env)
{
	if (generating) Warning("Bug: generating is true in nativeRedo!");
    if (event_checker > 0) Warning("Bug: event_checker > 0 in nativeRedo!");

    ClearMessage();
    currlayer->undoredo->RedoChange();
    UpdateEverything();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT bool JNICALL Java_net_sf_golly_MainActivity_nativeCanReset(JNIEnv* env)
{
    return currlayer->algo->getGeneration() > currlayer->startgen;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeResetPattern(JNIEnv* env)
{
	if (generating) StopGenerating();
    if (event_checker > 0) {
        // try again after a short delay
        //!!! CallAfterDelay("ResetPattern");
        return;
    }

    ClearMessage();
    CheckIfRendering();
    ResetPattern();
    UpdateEverything();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeStartStop(JNIEnv* env)
{
    if (generating) {
    	StopGenerating();
    	// generating is now false
    } else {
    	StartGenerating();
    	// generating might still be false (eg. if pattern is empty)
    }
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT bool JNICALL Java_net_sf_golly_MainActivity_nativeIsGenerating(JNIEnv* env)
{
    return generating;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeGenerate(JNIEnv* env)
{
	if (paused) return;		// PauseGenerating has been called

	NextGeneration(true);	// calls currlayer->algo->step() using current gen increment
	UpdateStatus();
	UpdatePattern();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeStep(JNIEnv* env)
{
    if (generating) StopGenerating();
    if (event_checker > 0) {
        // try again after a short delay
        //!!! CallAfterDelay("Step");
        return;
    }

    ClearMessage();
    CheckIfRendering();
    NextGeneration(true);
	UpdateStatus();
	UpdatePattern();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT int JNICALL Java_net_sf_golly_MainActivity_nativeCalculateSpeed(JNIEnv* env)
{
	// calculate the interval (in millisecs) between nativeGenerate calls

	int interval = 1000/60;		// max speed is 60 fps

    // increase interval if user wants a delay
    if (currlayer->currexpo < 0) {
        interval = GetCurrentDelay();
    }

	return interval;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeStep1(JNIEnv* env)
{
    ClearMessage();
    // reset step exponent to 0
    currlayer->currexpo = 0;
    SetGenIncrement();
    UpdateStatus();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeFaster(JNIEnv* env)
{
    ClearMessage();
    // go faster by incrementing step exponent
    currlayer->currexpo++;
    SetGenIncrement();
    UpdateStatus();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeSlower(JNIEnv* env)
{
    ClearMessage();
    // go slower by decrementing step exponent
    if (currlayer->currexpo > minexpo) {
        currlayer->currexpo--;
        SetGenIncrement();
    } else {
        Beep();
    }
    UpdateStatus();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeNewPattern(JNIEnv* env)
{
    // undo/redo history is about to be cleared so no point calling RememberGenFinish
    // if we're generating a (possibly large) pattern
    bool saveundo = allowundo;
    allowundo = false;
    if (generating) StopGenerating();
    allowundo = saveundo;

    if (event_checker > 0 /* || rendering ???!!! */ ) {
        // try again after a short delay that gives time for NextGeneration or DrawPattern to terminate
        //!!!??? call Java method that will call nativeNewPattern again after a short delay
        //!!! CallAfterDelay("NewPattern");
        return;
    }

    ClearMessage();
    CheckIfRendering();
    NewPattern();
    UpdateStatus();
    UpdatePattern();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeFitPattern(JNIEnv* env)
{
    ClearMessage();
    currlayer->algo->fit(*currlayer->view, 1);
    UpdateStatus();
    UpdatePattern();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeScale1to1(JNIEnv* env)
{
    ClearMessage();
    // set scale to 1:1
    if (currlayer->view->getmag() != 0) {
        currlayer->view->setmag(0);
        UpdateStatus();
        UpdatePattern();
    }
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeBigger(JNIEnv* env)
{
    ClearMessage();
    // zoom in
    if (currlayer->view->getmag() < MAX_MAG) {
        currlayer->view->zoom();
        UpdateStatus();
        UpdatePattern();
    } else {
        Beep();
    }
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeSmaller(JNIEnv* env)
{
    ClearMessage();
    // zoom out
    currlayer->view->unzoom();
    UpdateStatus();
    UpdatePattern();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeMiddle(JNIEnv* env)
{
    ClearMessage();
    if (currlayer->originx == bigint::zero && currlayer->originy == bigint::zero) {
        currlayer->view->center();
    } else {
        // put cell saved by ChangeOrigin (not yet implemented!!!) in middle
        currlayer->view->setpositionmag(currlayer->originx, currlayer->originy, currlayer->view->getmag());
    }
    UpdateStatus();
    UpdatePattern();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT int JNICALL Java_net_sf_golly_MainActivity_nativeGetMode(JNIEnv* env)
{
    switch (currlayer->touchmode) {
    	case drawmode:   return 0;
    	case pickmode:   return 1;
    	case selectmode: return 2;
    	case movemode:   return 3;
    }
    Warning("Bug detected in nativeGetMode!");
    return 0;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeSetMode(JNIEnv* env, jobject obj, jint mode)
{
	ClearMessage();
    switch (mode) {
    	case 0: currlayer->touchmode = drawmode;   return;
    	case 1: currlayer->touchmode = pickmode;   return;
    	case 2: currlayer->touchmode = selectmode; return;
    	case 3: currlayer->touchmode = movemode;   return;
    }
    Warning("Bug detected in nativeSetMode!");
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT int JNICALL Java_net_sf_golly_MainActivity_nativeNumLayers(JNIEnv* env)
{
	return numlayers;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT bool JNICALL Java_net_sf_golly_MainActivity_nativePasteExists(JNIEnv* env)
{
	return waitingforpaste;
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT bool JNICALL Java_net_sf_golly_MainActivity_nativeSelectionExists(JNIEnv* env)
{
	return currlayer->currsel.Exists();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativePaste(JNIEnv* env)
{
	if (generating) StopGenerating();
    if (event_checker > 0) {
        // try again after a short delay
        //!!! CallAfterDelay("Paste");
        return;
    }
    ClearMessage();
    PasteClipboard();
    UpdatePattern();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeSelectAll(JNIEnv* env)
{
    ClearMessage();
    SelectAll();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeRemoveSelection(JNIEnv* env)
{
	// no need to pause generating
	ClearMessage();
	RemoveSelection();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeCutSelection(JNIEnv* env)
{
    if (generating) {
        // temporarily stop generating
        PauseGenerating();
        if (event_checker > 0) {
            // try again after a short delay
        	//!!! CallAfterDelay("Cut");
            return;
        }
    }
    ClearMessage();
	CutSelection();
	ResumeGenerating();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeCopySelection(JNIEnv* env)
{
	// no need to pause generating
	ClearMessage();
	CopySelection();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeClearSelection(JNIEnv* env, jobject obj, jint inside)
{
    if (generating) {
        // temporarily stop generating
        PauseGenerating();
        if (event_checker > 0) {
            // try again after a short delay
        	//!!! CallAfterDelay(inside ? "Clear" : "ClearOutside");
            return;
        }
    }
    ClearMessage();
	if (inside) {
		ClearSelection();
	} else {
		ClearOutsideSelection();
	}
	ResumeGenerating();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeShrinkSelection(JNIEnv* env)
{
	// no need to pause generating
	ClearMessage();
	ShrinkSelection(false);
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeRandomFill(JNIEnv* env)
{
    if (generating) {
        // temporarily stop generating
        PauseGenerating();
        if (event_checker > 0) {
            // try again after a short delay
        	//!!! CallAfterDelay("Random");
            return;
        }
    }
    ClearMessage();
	RandomFill();
	ResumeGenerating();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeFlipSelection(JNIEnv* env, jobject obj, jint y)
{
    if (generating) {
        // temporarily stop generating
        PauseGenerating();
        if (event_checker > 0) {
            // try again after a short delay
        	//!!! CallAfterDelay(y ? "FlipY" : "FlipX");
            return;
        }
    }
    ClearMessage();
	FlipSelection(y);
	ResumeGenerating();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeRotateSelection(JNIEnv* env, jobject obj, jint clockwise)
{
    if (generating) {
        // temporarily stop generating
        PauseGenerating();
        if (event_checker > 0) {
            // try again after a short delay
        	//!!! CallAfterDelay(clockwise ? "RotateC" : "RotateA");
            return;
        }
    }
    ClearMessage();
	RotateSelection(clockwise);
	ResumeGenerating();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeAdvanceSelection(JNIEnv* env, jobject obj, jint inside)
{
    if (generating) {
        // temporarily stop generating
        PauseGenerating();
        if (event_checker > 0) {
            // try again after a short delay
        	//!!! CallAfterDelay(inside ? "Advance" : "AdvanceOutside");
            return;
        }
    }
    ClearMessage();
	if (inside) {
		currlayer->currsel.Advance();
	} else {
		currlayer->currsel.AdvanceOutside();
	}
	ResumeGenerating();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeAbortPaste(JNIEnv* env)
{
	ClearMessage();
	AbortPaste();
	UpdateEverything();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeDoPaste(JNIEnv* env, jobject obj, jint toselection)
{
	if (generating) StopGenerating();
    if (event_checker > 0) {
        // try again after a short delay
        //!!! CallAfterDelay(toselection ? "DoPasteSel" : "DoPaste");
        return;
    }

	ClearMessage();
	DoPaste(toselection);
	UpdateEverything();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeFlipPaste(JNIEnv* env, jobject obj, jint y)
{
	ClearMessage();
	FlipPastePattern(y);
	UpdateEverything();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeRotatePaste(JNIEnv* env, jobject obj, jint clockwise)
{
	ClearMessage();
	RotatePastePattern(clockwise);
	UpdateEverything();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_PatternGLSurfaceView_nativePause(JNIEnv* env)
{
    // LOGI("nativePause");
	PauseGenerating();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_PatternGLSurfaceView_nativeResume(JNIEnv* env)
{
    // LOGI("nativeResume");
	ResumeGenerating();
	UpdateStatus();
	UpdatePattern();
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_PatternGLSurfaceView_nativeTouchBegan(JNIEnv* env, jobject obj, jint x, jint y)
{
    // LOGI("touch began: x=%d y=%d", x, y);
	ClearMessage();
    TouchBegan(x, y);
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_PatternGLSurfaceView_nativeTouchMoved(JNIEnv* env, jobject obj, jint x, jint y)
{
    // LOGI("touch moved: x=%d y=%d", x, y);
    TouchMoved(x, y);
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_PatternGLSurfaceView_nativeTouchEnded(JNIEnv* env)
{
    // LOGI("touch ended");
	TouchEnded();
}

// -----------------------------------------------------------------------------

// called to initialize the graphics state
extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_PatternRenderer_nativeInit(JNIEnv* env)
{
    // we only do 2D drawing
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_DITHER);
    glDisable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY);

    glEnable(GL_BLEND);
    // this blending function seems similar to the one used in desktop Golly
    // (ie. selected patterns look much the same)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_PatternRenderer_nativeResize(JNIEnv* env, jobject obj, jint w, jint h)
{
    // LOGI("resize w=%d h=%d", w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0, w, h, 0, -1, 1);     // origin is top left and y increases down
	glViewport(0, 0, w, h);
    glMatrixMode(GL_MODELVIEW);

    if (w != currlayer->view->getwidth() || h != currlayer->view->getheight()) {
    	// update size of viewport
    	ResizeLayers(w, h);
    	UpdatePattern();
    }
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_PatternRenderer_nativeRender(JNIEnv* env)
{
	// render the current pattern; note that this occurs on a different thread so we use
	// a global flag to prevent calls like NewPattern being called before DrawPattern
	// has finished (is there a safer way???!!!)
	rendering = true;
	DrawPattern(currindex);
	rendering = false;
}

// -----------------------------------------------------------------------------

void UpdatePattern()
{
	// call a Java method that calls GLSurfaceView.requestRender() which calls nativeRender
	bool attached;
	JNIEnv* env = getJNIenv(&attached);
    if (env) env->CallVoidMethod(mainobj, id_RefreshPattern);
	if (attached) javavm->DetachCurrentThread();
}

// -----------------------------------------------------------------------------

std::string GetRuleName(const std::string& rule)
{
	std::string result = "";
	// not yet implemented!!!
	// maybe we should do this in rule.h and rule.cpp in gui-common???
	return result;
}

// -----------------------------------------------------------------------------

void UpdateEditBar()
{
	// call Java method in MainActivity class to update the buttons in the edit bar
	bool attached;
	JNIEnv* env = getJNIenv(&attached);
    if (env) env->CallVoidMethod(mainobj, id_UpdateEditBar);
	if (attached) javavm->DetachCurrentThread();
}

// -----------------------------------------------------------------------------

void BeginProgress(const char* title)
{
	/* not yet implemented!!!
    if (progresscount == 0) {
        // disable interaction with all views but don't show progress view just yet
        [globalController enableInteraction:NO];
        [globalTitle setText:[NSString stringWithCString:title encoding:NSUTF8StringEncoding]];
        cancelProgress = false;
        progstart = TimeInSeconds();
    }
    progresscount++;    // handles nested calls
    */
}

// -----------------------------------------------------------------------------

bool AbortProgress(double fraction_done, const char* message)
{
	/* not yet implemented!!!
    if (progresscount <= 0) Fatal("Bug detected in AbortProgress!");
    double secs = TimeInSeconds() - progstart;
    if (!globalProgress.hidden) {
        if (secs < prognext) return false;
        prognext = secs + 0.1;     // update progress bar about 10 times per sec
        if (fraction_done < 0.0) {
            // show indeterminate progress gauge???
        } else {
            [globalController updateProgressBar:fraction_done];
        }
        return cancelProgress;
    } else {
        // note that fraction_done is not always an accurate estimator for how long
        // the task will take, especially when we use nextcell for cut/copy
        if ( (secs > 1.0 && fraction_done < 0.3) || secs > 2.0 ) {
            // task is probably going to take a while so show progress view
            globalProgress.hidden = NO;
            [globalController updateProgressBar:fraction_done];
        }
        prognext = secs + 0.01;     // short delay until 1st progress update
    }
    */
    return false;
}

// -----------------------------------------------------------------------------

void EndProgress()
{
	/* not yet implemented!!!
    if (progresscount <= 0) Fatal("Bug detected in EndProgress!");
    progresscount--;
    if (progresscount == 0) {
        // hide the progress view and enable interaction with other views
        globalProgress.hidden = YES;
        [globalController enableInteraction:YES];
    }
    */
}

// -----------------------------------------------------------------------------

void SwitchToPatternTab()
{
	// switch to pattern view (ie. MainActivity)
	// not yet implemented!!!
	LOGI("SwitchToPatternTab");
}

// -----------------------------------------------------------------------------

void ShowTextFile(const char* filepath)
{
	// display contents of text file in modal view (TextFileActivity!!!)
	// not yet implemented!!!
	LOGI("ShowTextFile: %s", filepath);
}

// -----------------------------------------------------------------------------

void ShowHelp(const char* filepath)
{
	// display html file in help view (HelpActivity!!!)
	// not yet implemented!!!
	LOGI("ShowHelp: %s", filepath);
}

// -----------------------------------------------------------------------------

void AndroidWarning(const char* msg)
{
    // not yet implemented!!!
    LOGE("WARNING: %s", msg);
}

// -----------------------------------------------------------------------------

bool AndroidYesNo(const char* msg)
{
    // not yet implemented!!!
    LOGE("AndroidYesNo: %s", msg);
    return true; //!!!
}

// -----------------------------------------------------------------------------

void AndroidFatal(const char* msg)
{
    // not yet implemented!!!
    LOGE("FATAL ERROR: %s", msg);
    //!!!??? System.exit(1);
    exit(1);
}

// -----------------------------------------------------------------------------

void AndroidBeep()
{
    // not yet implemented!!!
    LOGI("BEEP");//!!!
}

// -----------------------------------------------------------------------------

void AndroidRemoveFile(const std::string& filepath)
{
    // not yet implemented!!!
    LOGE("AndroidRemoveFile: %s", filepath.c_str());
}

// -----------------------------------------------------------------------------

bool AndroidMoveFile(const std::string& inpath, const std::string& outpath)
{
    // not yet implemented!!!
    LOGE("AndroidMoveFile: %s to %s", inpath.c_str(), outpath.c_str());
    return false;//!!!
}

// -----------------------------------------------------------------------------

void AndroidFixURLPath(std::string& path)
{
    // replace "%..." with suitable chars for a file path (eg. %20 is changed to space)
    // not yet implemented!!!
    LOGE("AndroidFixURLPath: %s", path.c_str());
}

// -----------------------------------------------------------------------------

void AndroidCheckEvents()
{
	bool attached;
	JNIEnv* env = getJNIenv(&attached);
    if (env) env->CallVoidMethod(mainobj, id_CheckMessageQueue);
	if (attached) javavm->DetachCurrentThread();
}

// -----------------------------------------------------------------------------

bool AndroidCopyTextToClipboard(const char* text)
{
	bool attached;
	JNIEnv* env = getJNIenv(&attached);
    if (env) {
    	jstring jtext = env->NewStringUTF(text);
    	env->CallVoidMethod(mainobj, id_CopyTextToClipboard, jtext);
    	env->DeleteLocalRef(jtext);
    }
	if (attached) javavm->DetachCurrentThread();

    return env != NULL;
}

// -----------------------------------------------------------------------------

bool AndroidGetTextFromClipboard(std::string& text)
{
	text = "";
	bool attached;
	JNIEnv* env = getJNIenv(&attached);
    if (env) {
    	jstring jtext = (jstring) env->CallObjectMethod(mainobj, id_GetTextFromClipboard);
    	text = ConvertJString(env, jtext);
    	env->DeleteLocalRef(jtext);
    }
	if (attached) javavm->DetachCurrentThread();

    return text.length() > 0;
}

// -----------------------------------------------------------------------------

bool AndroidDownloadFile(const std::string& url, const std::string& filepath)
{
    // not yet implemented!!!
    LOGI("AndroidDownloadFile: url=%s file=%s", url.c_str(), filepath.c_str());
    return false;//!!!
}
