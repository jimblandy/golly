// This file is part of Golly.
// See docs/License.html for the copyright notice.

package net.sf.golly;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import android.content.Context;
import android.graphics.PixelFormat;
import android.opengl.GLSurfaceView;
import android.util.AttributeSet;
// import android.util.Log;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;

// this class must be public so it can be used in main_layout.xml

public class PatternGLSurfaceView extends GLSurfaceView {

	// see jnicalls.cpp for these native routines:
    private native void nativeTouchBegan(int x, int y);
    private native void nativeTouchMoved(int x, int y);
    private native void nativeTouchEnded();
    private native void nativeMoveMode();
    private native void nativeRestoreMode();
    private native void nativeZoomIn(int x, int y);
    private native void nativeZoomOut(int x, int y);
    private native void nativePause();
    private native void nativeResume();

    private static final int INVALID_POINTER_ID = -1;
    private int mActivePointerId = INVALID_POINTER_ID;
    
    private int beginx, beginy;
    private boolean callTouchBegan = false;     // nativeTouchBegan needs to be called?
    private boolean multifinger = false;        // are we doing multi-finger panning or zooming?
    private boolean zooming = false;            // are we zooming?
    private int pancount = 0;                   // prevents panning changing to zooming

    // -----------------------------------------------------------------------------
    
    // this stuff handles zooming in/out via two-finger pinch gestures
    
    private ScaleGestureDetector zoomDetector;

    private class ScaleListener implements ScaleGestureDetector.OnScaleGestureListener {

        private float oldscale, newscale;
        private int zoomx, zoomy;
        
        @Override
        public boolean onScaleBegin(ScaleGestureDetector detector) {
            zooming = true;
            oldscale = 1.0f;
            newscale = 1.0f;
            // set zoom point at start of pinch
            zoomx = (int)detector.getFocusX();
            zoomy = (int)detector.getFocusY();
            // Log.i("onScaleBegin", "zoomx=" + Integer.toString(zoomx) + " zoomy=" + Integer.toString(zoomy));
            return true;
        }
        
        @Override
        public boolean onScale(ScaleGestureDetector detector) {
            newscale *= detector.getScaleFactor();
            // Log.i("onScale", "newscale=" + Float.toString(newscale) + " oldscale=" + Float.toString(oldscale));
            if (newscale - oldscale < -0.1f) {
                // fingers moved closer, so zoom out
                nativeZoomOut(zoomx, zoomy);
                oldscale = newscale;
            } else if (newscale - oldscale > 0.1f) {
                // fingers moved apart, so zoom in
                nativeZoomIn(zoomx, zoomy);
                oldscale = newscale;
            }
            return true;
        }
        
        @Override
        public void onScaleEnd(ScaleGestureDetector detector) {
            // Log.i("onScaleEnd", "---");
            oldscale = 1.0f;
            newscale = 1.0f;
            // don't reset zooming flag here
        }
    }

    // -----------------------------------------------------------------------------

	public PatternGLSurfaceView(Context context, AttributeSet attrs) {
        super(context, attrs);

        super.setEGLConfigChooser(8, 8, 8, 8, 0/*no depth*/, 0);
        getHolder().setFormat(PixelFormat.RGBA_8888);	// avoid crash on some devices
        
        setRenderer(new PatternRenderer());
        setRenderMode(RENDERMODE_WHEN_DIRTY);
        
        zoomDetector = new ScaleGestureDetector(context, new ScaleListener());
    }

    // -----------------------------------------------------------------------------

    public boolean onTouchEvent(final MotionEvent ev) {
        if (pancount < 4) {
            // check for zooming
            zoomDetector.onTouchEvent(ev);
            if (zoomDetector.isInProgress()) return true;
        }
        
        final int action = ev.getAction();
        switch (action & MotionEvent.ACTION_MASK) {
	        case MotionEvent.ACTION_DOWN: {
	            // Log.i("onTouchEvent", "ACTION_DOWN");
                mActivePointerId = ev.getPointerId(0);
	            beginx = (int)ev.getX();
	            beginy = (int)ev.getY();
	            callTouchBegan = true;
	            break;
	        }
	            
	        case MotionEvent.ACTION_MOVE: {
	            // we need to test zooming flag because it's possible to get
	            // ACTION_MOVE after ACTION_POINTER_UP (and after onScaleEnd)
	            if (mActivePointerId != INVALID_POINTER_ID && !zooming) {
	                int pointerIndex = ev.findPointerIndex(mActivePointerId);
	                int x = (int)ev.getX(pointerIndex);
	                int y = (int)ev.getY(pointerIndex);
	                if (callTouchBegan) {
	                    nativeTouchBegan(beginx, beginy);
	                    callTouchBegan = false;
	                }
	                if (multifinger && ev.getPointerCount() == 1) {
	                    // there is only one pointer down so best not to move
	                } else {
	                    nativeTouchMoved(x, y);
	                }
	                if (multifinger) {
	                    pancount++;
	                }
	                // Log.i("onTouchEvent", "ACTION_MOVE" + " deltax=" + Integer.toString(x - beginx) +
	                //                                       " deltay=" + Integer.toString(y - beginy));
	            }
	            break;
	        }
	            
	        case MotionEvent.ACTION_UP: {
	            // Log.i("onTouchEvent", "ACTION_UP");
	            mActivePointerId = INVALID_POINTER_ID;
                if (callTouchBegan) {
                    nativeTouchBegan(beginx, beginy);
                    callTouchBegan = false;
                }
	            nativeTouchEnded();
                if (multifinger) {
                    nativeRestoreMode();
                    multifinger = false;
                }
                pancount = 0;
                zooming = false;
	            break;
	        }
	            
	        case MotionEvent.ACTION_CANCEL: {
	            // Log.i("onTouchEvent", "ACTION_CANCEL");
	            mActivePointerId = INVALID_POINTER_ID;
                if (callTouchBegan) {
                    nativeTouchBegan(beginx, beginy);
                    callTouchBegan = false;
                }
	            nativeTouchEnded();
	            if (multifinger) {
	                nativeRestoreMode();
	                multifinger = false;
	            }
	            pancount = 0;
	            zooming = false;
	            break;
	        }
            
	        case MotionEvent.ACTION_POINTER_DOWN: {
	            // Log.i("onTouchEvent", "ACTION_POINTER_DOWN");
                // another pointer has gone down
	            if (callTouchBegan) {
	                // ACTION_DOWN has been seen but no ACTION_MOVE as yet,
                    // so switch to panning mode (which might become zooming)
	                nativeMoveMode();
	                multifinger = true;
	            }
	            break;
	        }
	        
	        case MotionEvent.ACTION_POINTER_UP: {
                // Log.i("onTouchEvent", "ACTION_POINTER_UP");
	            // a pointer has gone up (but another pointer is still down)
	            final int pointerIndex = (action & MotionEvent.ACTION_POINTER_INDEX_MASK)
	                    				     >> MotionEvent.ACTION_POINTER_INDEX_SHIFT;
	            final int pointerId = ev.getPointerId(pointerIndex);
	            if (pointerId == mActivePointerId) {
	                // this was our active pointer going up so choose a new active pointer
	            	final int newPointerIndex = pointerIndex == 0 ? 1 : 0;
	            	mActivePointerId = ev.getPointerId(newPointerIndex);
	            }
	            break;
	        }
        }
        
        return true;
    }

    // -----------------------------------------------------------------------------

    @Override
    public void onPause() {
        super.onPause();
        nativePause();
    }

    // -----------------------------------------------------------------------------

    @Override
    public void onResume() {
        super.onResume();
        nativeResume();
    }

} // PatternGLSurfaceView class

// =================================================================================

class PatternRenderer implements GLSurfaceView.Renderer {

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

} // PatternRenderer class
