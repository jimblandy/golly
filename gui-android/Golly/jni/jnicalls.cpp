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

#include "utils.h"		// for LOGI, Warning, etc
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
    if (id_RefreshPattern == 0) { LOGI("GetMethodID failed for RefreshPattern"); }

    id_ShowStatusLines = env->GetMethodID(klass, "ShowStatusLines", "()V");
    if (id_ShowStatusLines == 0) { LOGI("GetMethodID failed for ShowStatusLines"); }
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
    	case 1: return env->NewStringUTF(status1.c_str()); break;
    	case 2: return env->NewStringUTF(status2.c_str()); break;
    	case 3: return env->NewStringUTF(status3.c_str()); break;
    }
    return env->NewStringUTF("Fix bug in nativeGetStatusLine!");
}

// -----------------------------------------------------------------------------

void PauseGenerating()
{
	if (generating) {
		generating = false;
		paused = true;
	}
}

// -----------------------------------------------------------------------------

void ResumeGenerating()
{
	if (paused) {
		paused = false;
		generating = true;
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
    }
}

// -----------------------------------------------------------------------------

extern "C"
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeDestroy(JNIEnv* env)
{
    // LOGI("nativeDestroy");

	/* Android apps aren't really meant to quit so best not to do anything!!!???
    // avoid nativeResume restarting pattern generation
	paused = false;
	generating = false;

	if (currlayer->dirty) {
		// ask if changes should be saved!!!???
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
JNIEXPORT void JNICALL Java_net_sf_golly_MainActivity_nativeStep(JNIEnv* env)
{
	if (paused) return;		// PauseGenerating has been called

	NextGeneration(true);	// calls currlayer->algo->step()
	UpdateStatus();
	UpdatePattern();
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
        // try again after a short delay that gives time for NextGeneration() to terminate
        //!!!??? call Java method that will call nativeNewPattern again after a short delay
        //!!! [self performSelector:@selector(doNew:) withObject:sender afterDelay:0.01];
        return;
    }

    ClearMessage();
    while (rendering) {
    	// wait for DrawPattern to finish
    	// LOGI("waiting for DrawPattern to finish");
    	usleep(1000);	// 1 millisec
    }
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
JNIEXPORT bool JNICALL Java_net_sf_golly_MainActivity_nativeIsGenerating(JNIEnv* env)
{
    return generating;
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

void UpdatePattern()
{
	// call a Java method that calls GLSurfaceView.requestRender() which calls nativeRender
	bool attached;
	JNIEnv* env = getJNIenv(&attached);
    if (env) env->CallVoidMethod(mainobj, id_RefreshPattern);
	if (attached) javavm->DetachCurrentThread();
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
