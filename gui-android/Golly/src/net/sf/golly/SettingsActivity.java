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
import android.content.Intent;
import android.os.Bundle;
import android.support.v4.app.NavUtils;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.CheckBox;

public class SettingsActivity extends Activity {

    // see jnicalls.cpp for these native routines:
    private native void nativeOpenSettings();
    private native void nativeCloseSettings();
    private native int nativeGetPref(String pref);
    private native void nativeSetPref(String pref, int val);

    // -----------------------------------------------------------------------------

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.settings_layout);
        
        // show the Up button in the action bar
        getActionBar().setDisplayHomeAsUpEnabled(true);
        
        // initialize check boxes, etc
        initSettings();
        
        nativeOpenSettings();
    }

    // -----------------------------------------------------------------------------

    @Override
    protected void onPause() {
        super.onPause();
        nativeCloseSettings();
    }

    // -----------------------------------------------------------------------------

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // add main.xml items to the action bar
        getMenuInflater().inflate(R.menu.main, menu);
        
        // disable the item for this activity
        MenuItem item = menu.findItem(R.id.settings);
        item.setEnabled(false);
        
        return true;
    }

    // -----------------------------------------------------------------------------

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // action bar item has been tapped
        Intent intent;
        switch (item.getItemId()) {
            case android.R.id.home:
                // the Home or Up button will start MainActivity
                NavUtils.navigateUpFromSameTask(this);
                return true;
            case R.id.open:
                intent = new Intent(this, OpenActivity.class);
                startActivity(intent);
                return true;
            case R.id.settings:
                // do nothing
                break;
            case R.id.help:
                intent = new Intent(this, HelpActivity.class);
                startActivity(intent);
                return true;
        }
        return super.onOptionsItemSelected(item);
    }

    // -----------------------------------------------------------------------------
    
    private void initSettings() {
        ((CheckBox) findViewById(R.id.checkbox_hash)).setChecked( nativeGetPref("hash") == 1 );
        ((CheckBox) findViewById(R.id.checkbox_time)).setChecked( nativeGetPref("time") == 1 );
        ((CheckBox) findViewById(R.id.checkbox_beep)).setChecked( nativeGetPref("beep") == 1 );
        ((CheckBox) findViewById(R.id.checkbox_swap)).setChecked( nativeGetPref("swap") == 1 );
        ((CheckBox) findViewById(R.id.checkbox_icon)).setChecked( nativeGetPref("icon") == 1 );
        ((CheckBox) findViewById(R.id.checkbox_undo)).setChecked( nativeGetPref("undo") == 1 );
        ((CheckBox) findViewById(R.id.checkbox_grid)).setChecked( nativeGetPref("grid") == 1 );
    }

    // -----------------------------------------------------------------------------
    
    public void onCheckboxClicked(View view) {
        boolean checked = ((CheckBox) view).isChecked();
        switch(view.getId()) {
            case R.id.checkbox_hash: nativeSetPref("hash", checked ? 1 : 0); break;
            case R.id.checkbox_time: nativeSetPref("time", checked ? 1 : 0); break;
            case R.id.checkbox_beep: nativeSetPref("beep", checked ? 1 : 0); break;
            case R.id.checkbox_swap: nativeSetPref("swap", checked ? 1 : 0); break;
            case R.id.checkbox_icon: nativeSetPref("icon", checked ? 1 : 0); break;
            case R.id.checkbox_undo: nativeSetPref("undo", checked ? 1 : 0); break;
            case R.id.checkbox_grid: nativeSetPref("grid", checked ? 1 : 0); break;
        }
    }

} // SettingsActivity class
