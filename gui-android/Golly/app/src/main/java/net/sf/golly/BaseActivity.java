// This file is part of Golly.
// See docs/License.html for the copyright notice.

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
