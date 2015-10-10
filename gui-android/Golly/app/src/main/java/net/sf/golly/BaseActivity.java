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

import android.app.Activity;
import android.os.Bundle;

//this class (along with BaseApp) allows our app to keep track of the current foreground activity
//(thanks to http://stackoverflow.com/questions/11411395/how-to-get-current-foreground-activity-context-in-android)

public abstract class BaseActivity extends Activity {

    protected BaseApp baseapp;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        baseapp = (BaseApp) this.getApplicationContext();
        baseapp.setCurrentActivity(this);
    }

    @Override
    protected void onResume() {
        super.onResume();
        baseapp.setCurrentActivity(this);
    }

    @Override
    protected void onPause() {
        clearReferences();
        super.onPause();
    }

    @Override
    protected void onDestroy() {        
        clearReferences();
        super.onDestroy();
    }

    private void clearReferences() {
        Activity curractivity = baseapp.getCurrentActivity();
        if (curractivity != null && curractivity.equals(this))
            baseapp.setCurrentActivity(null);
    }

} // BaseActivity class
