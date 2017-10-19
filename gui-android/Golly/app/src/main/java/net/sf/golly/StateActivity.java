// This file is part of Golly.
// See docs/License.html for the copyright notice.

package net.sf.golly;

import android.app.Activity;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.CheckBox;

public class StateActivity extends Activity {

    // see jnicalls.cpp for these native routines:
    private native int nativeNumStates();
    private native boolean nativeShowIcons();
    private native void nativeToggleIcons();

    private StateGLSurfaceView stateView;       // OpenGL ES is used to draw states

    // -----------------------------------------------------------------------------

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        this.requestWindowFeature(Window.FEATURE_NO_TITLE);
        setContentView(R.layout.state_layout);
        
        ((CheckBox) findViewById(R.id.icons)).setChecked( nativeShowIcons() );
        
        // this will call the PatternGLSurfaceView constructor
        stateView = (StateGLSurfaceView) findViewById(R.id.stateview);
        stateView.setCallerActivity(this);
        
        // avoid this GL surface being darkened like PatternGLSurfaceView (underneath dialog box)
        stateView.setZOrderOnTop(true);
        
        // change dimensions of stateView (initially 321x321) depending on screen density
        // and number of states in current rule
        int numstates = nativeNumStates();
        DisplayMetrics metrics = getResources().getDisplayMetrics();
        if (metrics.densityDpi > 300) {
            ViewGroup.LayoutParams params = stateView.getLayoutParams();
            params.width = 642;
            if (numstates <= 90) {
                params.height = ((numstates + 9) / 10) * 64 + 2;
            } else {
                params.height = 642;
            }
            stateView.setLayoutParams(params);
        } else if (numstates <= 90) {
            ViewGroup.LayoutParams params = stateView.getLayoutParams();
            params.height = ((numstates + 9) / 10) * 32 + 1;
            stateView.setLayoutParams(params);
        }
        
        stateView.requestRender();          // display states
    }

    // -----------------------------------------------------------------------------

    @Override
    protected void onPause() {
        super.onPause();
        stateView.onPause();
    }

    // -----------------------------------------------------------------------------

    @Override
    protected void onResume() {
        super.onResume();
        stateView.onResume();
    }

    // -----------------------------------------------------------------------------
    
    // called when the "Show icons" check box is tapped
    public void doToggleIcons(View view) {
        nativeToggleIcons();
        stateView.requestRender();
    }

} // StateActivity class
