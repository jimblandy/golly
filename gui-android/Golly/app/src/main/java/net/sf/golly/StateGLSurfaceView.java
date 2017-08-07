// This file is part of Golly.
// See docs/License.html for the copyright notice.

package net.sf.golly;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import android.app.Activity;
import android.content.Context;
import android.graphics.PixelFormat;
import android.opengl.GLSurfaceView;
import android.util.AttributeSet;
import android.view.MotionEvent;

//this class must be public so it can be used in state_layout.xml

public class StateGLSurfaceView extends GLSurfaceView {

    // see jnicalls.cpp for these native routines:
    private native void nativeTouchBegan(int x, int y);
    private native boolean nativeTouchMoved(int x, int y);
    private native boolean nativeTouchEnded();

    private static final int INVALID_POINTER_ID = -1;
    private int mActivePointerId = INVALID_POINTER_ID;
    
    private Activity caller;

    // -----------------------------------------------------------------------------

    public StateGLSurfaceView(Context context, AttributeSet attrs) {
        super(context, attrs);

        super.setEGLConfigChooser(8, 8, 8, 8, 0/*no depth*/, 0);
        getHolder().setFormat(PixelFormat.RGBA_8888);	// avoid crash on some devices

        setRenderer(new StateRenderer());
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
                mActivePointerId = ev.getPointerId(0);
                final float x = ev.getX();
                final float y = ev.getY();
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
                // secondary pointer has gone up
                final int pointerIndex = (action & MotionEvent.ACTION_POINTER_INDEX_MASK)
                                             >> MotionEvent.ACTION_POINTER_INDEX_SHIFT;
                final int pointerId = ev.getPointerId(pointerIndex);
                if (pointerId == mActivePointerId) {
                    // this was our active pointer going up so choose a new active pointer
                    final int newPointerIndex = pointerIndex == 0 ? 1 : 0;
                    mActivePointerId = ev.getPointerId(newPointerIndex);
                    final float x = ev.getX(newPointerIndex);
                    final float y = ev.getY(newPointerIndex);
                    if (nativeTouchMoved((int)x, (int)y)) {
                        // states have scrolled
                        requestRender();
                    }
                }
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

    public void onSurfaceCreated(GL10 unused, EGLConfig config) {
        nativeInit();
    }

    // -----------------------------------------------------------------------------

    public void onSurfaceChanged(GL10 unused, int w, int h) {
        nativeResize(w, h);
    }

    // -----------------------------------------------------------------------------

    public void onDrawFrame(GL10 unused) {
        nativeRender();
    }

} // StateRenderer class
