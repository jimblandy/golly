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

package net.sf.golly;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import android.app.Activity;
import android.content.Context;
import android.graphics.PixelFormat;
import android.opengl.GLSurfaceView;
import android.util.AttributeSet;
import android.util.Log;
import android.view.MotionEvent;

//this class must be public so it can be used in state_layout.xml

public class StateGLSurfaceView extends GLSurfaceView {

    // see jnicalls.cpp for these native routines:
    private native void nativeTouchBegan(int x, int y);
    private native boolean nativeTouchMoved(int x, int y);
    private native boolean nativeTouchEnded();

    private StateRenderer mRenderer;
    private static final int INVALID_POINTER_ID = -1;
    private int mActivePointerId = INVALID_POINTER_ID;
    
    private Activity caller;

    // -----------------------------------------------------------------------------

    public StateGLSurfaceView(Context context, AttributeSet attrs) {
        super(context, attrs);
        super.setEGLConfigChooser(8, 8, 8, 8, 0/*no depth*/, 0);
        getHolder().setFormat(PixelFormat.RGBA_8888);   // avoid crash on some devices
        mRenderer = new StateRenderer();
        setRenderer(mRenderer);
        setRenderMode(RENDERMODE_WHEN_DIRTY);
    }

    // -----------------------------------------------------------------------------

    public void setCallerActivity(Activity activity) {
        // set caller so we can finish StateActivity
        caller = activity;
    }

    // -----------------------------------------------------------------------------

    public boolean onTouchEvent(final MotionEvent ev) {
        final int action = ev.getAction();
        switch (action & MotionEvent.ACTION_MASK) {
            case MotionEvent.ACTION_DOWN: {
                final float x = ev.getX();
                final float y = ev.getY();
                mActivePointerId = ev.getPointerId(0);
                nativeTouchBegan((int)x, (int)y);
                break;
            }

            case MotionEvent.ACTION_MOVE: {
                if (mActivePointerId != INVALID_POINTER_ID) {
                    final int pointerIndex = ev.findPointerIndex(mActivePointerId);
                    final float x = ev.getX(pointerIndex);
                    final float y = ev.getY(pointerIndex);
                    if (nativeTouchMoved((int)x, (int)y)) {
                        // states have scrolled
                        requestRender();
                    }
                }
                break;
            }
                 
            case MotionEvent.ACTION_UP: {
                mActivePointerId = INVALID_POINTER_ID;
                if (nativeTouchEnded()) {
                    // user touched a state box so close dialog
                    caller.finish();
                }
                break;
            }

            case MotionEvent.ACTION_CANCEL: {
                mActivePointerId = INVALID_POINTER_ID;
                nativeTouchEnded();
                break;
            }

            case MotionEvent.ACTION_POINTER_UP: {
                // this never gets called???!!!
                Log.i("onTouchEvent", "ACTION_POINTER_UP");
                mActivePointerId = INVALID_POINTER_ID;
                nativeTouchEnded();
                break;
            }
        }
        return true;
    }

} // StateGLSurfaceView class

//=================================================================================

class StateRenderer implements GLSurfaceView.Renderer {

    // see jnicalls.cpp for these native routines:
    private native void nativeInit();
    private native void nativeResize(int w, int h);
    private native void nativeRender();

    // -----------------------------------------------------------------------------

    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        nativeInit();
    }

    // -----------------------------------------------------------------------------

    public void onSurfaceChanged(GL10 gl, int w, int h) {
        nativeResize(w, h);
    }

    // -----------------------------------------------------------------------------

    public void onDrawFrame(GL10 gl) {
        nativeRender();
    }

} // StateRenderer class
