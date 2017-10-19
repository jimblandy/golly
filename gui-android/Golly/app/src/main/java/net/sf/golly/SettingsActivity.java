// This file is part of Golly.
// See docs/License.html for the copyright notice.

package net.sf.golly;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.support.v4.app.NavUtils;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.PopupMenu;

public class SettingsActivity extends Activity {

    // see jnicalls.cpp for these native routines:
    private native void nativeOpenSettings();
    private native void nativeCloseSettings();
    private native int nativeGetPref(String pref);
    private native void nativeSetPref(String pref, int val);
    private native String nativeGetPasteMode();

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
        
        int rval;
        try {
            rval = Integer.parseInt(((EditText) findViewById(R.id.randperc)).getText().toString());
        } catch (NumberFormatException e) {
            rval = 50;  // 50% is default value for random fill percentage
        }
        int mval;
        try {
            mval = Integer.parseInt(((EditText) findViewById(R.id.maxmem)).getText().toString());
        } catch (NumberFormatException e) {
            mval = 100; // 100MB is default value for max hash memory
        }
        nativeSetPref("rand", rval);
        nativeSetPref("maxm", mval);
        
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
                // the Home or Up button will go back to MainActivity
                NavUtils.navigateUpFromSameTask(this);
                return true;
            case R.id.open:
            	finish();
                intent = new Intent(this, OpenActivity.class);
                startActivity(intent);
                return true;
            case R.id.settings:
                // do nothing
                break;
            case R.id.help:
            	finish();
                intent = new Intent(this, HelpActivity.class);
                startActivity(intent);
                return true;
        }
        return super.onOptionsItemSelected(item);
    }

    // -----------------------------------------------------------------------------
    
    private void initSettings() {
        ((EditText) findViewById(R.id.randperc)).setText(Integer.toString(nativeGetPref("rand")));
        ((EditText) findViewById(R.id.maxmem)).setText(Integer.toString(nativeGetPref("maxm")));
        
        ((CheckBox) findViewById(R.id.checkbox_hash)).setChecked( nativeGetPref("hash") == 1 );
        ((CheckBox) findViewById(R.id.checkbox_time)).setChecked( nativeGetPref("time") == 1 );
        ((CheckBox) findViewById(R.id.checkbox_beep)).setChecked( nativeGetPref("beep") == 1 );
        ((CheckBox) findViewById(R.id.checkbox_swap)).setChecked( nativeGetPref("swap") == 1 );
        ((CheckBox) findViewById(R.id.checkbox_icon)).setChecked( nativeGetPref("icon") == 1 );
        ((CheckBox) findViewById(R.id.checkbox_undo)).setChecked( nativeGetPref("undo") == 1 );
        ((CheckBox) findViewById(R.id.checkbox_grid)).setChecked( nativeGetPref("grid") == 1 );
        
        showPasteMode();
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

    // -----------------------------------------------------------------------------
    
    // called when the pmode button is tapped
    public void doPasteMode(View view) {
        // display pop-up menu with these items: AND, COPY, OR, XOR
        PopupMenu popup = new PopupMenu(this, view);
        MenuInflater inflater = popup.getMenuInflater();
        inflater.inflate(R.menu.pastemode_menu, popup.getMenu());
        popup.show();
    }

    // -----------------------------------------------------------------------------
    
    // called when item from pastemode_menu is selected
    public void doPasteModeItem(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.AND:  nativeSetPref("pmode", 0); break;
            case R.id.COPY: nativeSetPref("pmode", 1); break;
            case R.id.OR:   nativeSetPref("pmode", 2); break;
            case R.id.XOR:  nativeSetPref("pmode", 3); break;
            default:        Log.e("Golly","Fix bug in doPasteModeItem!"); return;
        }
        showPasteMode();
    }

    // -----------------------------------------------------------------------------
    
    private void showPasteMode() {
        // show current paste mode in button
        ((Button) findViewById(R.id.pmode)).setText("Paste mode: " + nativeGetPasteMode());
    }

} // SettingsActivity class
