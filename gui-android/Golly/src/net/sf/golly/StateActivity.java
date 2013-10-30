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

import android.content.res.Configuration;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.CheckBox;

public class StateActivity extends BaseActivity {

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
        Configuration config = getResources().getConfiguration();
        int numstates = nativeNumStates();
        if (config.densityDpi > 300) {
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
