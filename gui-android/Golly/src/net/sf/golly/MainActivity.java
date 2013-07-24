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

import java.io.File;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.PopupMenu;
import android.widget.TextView;

public class MainActivity extends Activity
{    
	// see jnicalls.cpp for these native routines:
    private static native void nativeClassInit();	// must be static
    private native void nativeCreate();				// must NOT be static
    private native void nativeDestroy();
    private native void nativeSetGollyDir(String path);
    private native void nativeSetTempDir(String path);
    private native void nativeSetSuppliedDirs(String prefix);
    private native void nativeStartStop();
    private native void nativeResetPattern();
    private native void nativeUndo();
    private native void nativeRedo();
    private native boolean nativeCanReset();
    private native boolean nativeAllowUndo();
    private native boolean nativeCanUndo();
    private native boolean nativeCanRedo();
    private native boolean nativeIsGenerating();
    private native String nativeGetStatusLine(int line);
    private native void nativeNewPattern();
    private native void nativeFitPattern();
    private native void nativeGenerate();
    private native void nativeStep();
    private native void nativeStep1();
    private native void nativeFaster();
    private native void nativeSlower();
    private native void nativeScale1to1();
    private native void nativeBigger();
    private native void nativeSmaller();
    private native void nativeMiddle();
    private native void nativeSetMode(int mode);
    private native int nativeGetMode();
    private native int nativeCalculateSpeed();
    private native int nativeNumLayers();
    private native boolean nativePasteExists();
    private native boolean nativeSelectionExists();
    private native void nativePaste();
    private native void nativeSelectAll();
    private native void nativeRemoveSelection();
    private native void nativeCutSelection();
    private native void nativeCopySelection();
    private native void nativeClearSelection(int inside);
    private native void nativeShrinkSelection();
    private native void nativeRandomFill();
    private native void nativeFlipSelection(int y);
    private native void nativeRotateSelection(int clockwise);
    private native void nativeAdvanceSelection(int inside);
    private native void nativeAbortPaste();
    private native void nativeDoPaste(int toselection);
    private native void nativeFlipPaste(int y);
    private native void nativeRotatePaste(int clockwise);

    // local fields:
    private static boolean firstcall = true;
	private Button ssbutton;						// Start/Stop button
	private Button undobutton;						// Undo button
	private Button redobutton;						// Redo button
	private Button editbutton;						// Edit/Paste button
	private Button modebutton;						// Draw/Pick/Select/Move button
    private TextView status1, status2, status3;		// status bar has 3 lines
    private PatternGLSurfaceView pattView;			// OpenGL ES is used to draw patterns
    private Handler genhandler;						// for generating patterns
    private Runnable generate;						// code started/stopped by genhandler
    private int geninterval;						// interval between nativeGenerate calls (in millisecs)

	// -----------------------------------------------------------------------------
    
    static {
    	System.loadLibrary("stlport_shared");	// must agree with Application.mk
        System.loadLibrary("golly");			// loads libgolly.so
        nativeClassInit();						// caches Java method IDs
    }

    // -----------------------------------------------------------------------------

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main_layout);
        ssbutton = (Button) findViewById(R.id.startstop);
        undobutton = (Button) findViewById(R.id.undo);
        redobutton = (Button) findViewById(R.id.redo);
        editbutton = (Button) findViewById(R.id.edit);
        modebutton = (Button) findViewById(R.id.touchmode);
        status1 = (TextView) findViewById(R.id.status1);
        status2 = (TextView) findViewById(R.id.status2);
        status3 = (TextView) findViewById(R.id.status3);

        if (firstcall) {
        	firstcall = false;
            InitPaths();		// sets tempdir, etc
        }
        nativeCreate();			// must be called every time (to cache jobject)
        
        // this will call the PatternGLSurfaceView constructor
        pattView = (PatternGLSurfaceView) findViewById(R.id.patternview);
        
        geninterval = nativeCalculateSpeed();
        genhandler = new Handler();
        generate = new Runnable() {
        	@Override
        	public void run() {
        		nativeGenerate();
        		genhandler.postDelayed(this, geninterval);
        	}
        };
    }

    // -----------------------------------------------------------------------------

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // this adds items to the action bar
        getMenuInflater().inflate(R.menu.main, menu);
        return true;
    }

    // -----------------------------------------------------------------------------

    // called when PATTERN item in action bar is tapped
    public void doPattern(MenuItem item) {
    	// not yet implemented!!!
    	Log.i("Golly", "switch to Pattern screen");
    }

    // -----------------------------------------------------------------------------

    // called when OPEN item in action bar is tapped
    public void doOpen(MenuItem item) {
    	// not yet implemented!!!
    	Log.i("Golly", "switch to Open screen");
    }

    // -----------------------------------------------------------------------------

    // called when SETTINGS item in action bar is tapped
    public void doSettings(MenuItem item) {
    	// not yet implemented!!!
    	Log.i("Golly", "switch to Settings screen");
    }

    // -----------------------------------------------------------------------------

    // called when HELP item in action bar is tapped
    public void doHelp(MenuItem item) {
    	// not yet implemented!!!
    	Log.i("Golly", "switch to Help screen");
    }

    // -----------------------------------------------------------------------------

    @Override
    protected void onPause() {
        super.onPause();
        pattView.onPause();
        genhandler.removeCallbacks(generate);
    }

    // -----------------------------------------------------------------------------

    @Override
    protected void onResume() {
        super.onResume();
        pattView.onResume();
        UpdateButtons();
        if (nativeIsGenerating()) {
    		genhandler.post(generate);
    	}
    }

    // -----------------------------------------------------------------------------

    @Override
    protected void onDestroy() {
    	genhandler.removeCallbacks(generate);		// should have been done in OnPause, but play safe
        nativeDestroy();
        super.onDestroy();
    }

    // -----------------------------------------------------------------------------

    public void InitPaths() {
    	// first create subdirs if they don't exist
    	File filesdir = getFilesDir();
    	File subdir;
    	subdir = new File(filesdir, "Rules");		// for user's .rule files
    	subdir.mkdir();
    	subdir = new File(filesdir, "Saved");		// for saved pattern files
    	subdir.mkdir();
    	subdir = new File(filesdir, "Downloads");	// for downloaded pattern files
    	subdir.mkdir();
    	
    	// now set gollydir (parent of above subdirs)
		String gollydir = filesdir.getAbsolutePath();
		nativeSetGollyDir(gollydir);
    	
    	// set tempdir
    	File dir = getCacheDir();
    	String tempdir = dir.getAbsolutePath();
    	nativeSetTempDir(tempdir);
		
		// create subdirs for files supplied with app here!!!???
    	// (need to extract from assets??? use OBB???)
		subdir = getDir("Patterns", MODE_PRIVATE);
		subdir = getDir("Rules", MODE_PRIVATE);
		subdir = getDir("Help", MODE_PRIVATE);
		// subdir = /data/data/net.sf.golly/app_Help
		String prefix = subdir.getAbsolutePath();
		prefix = prefix.substring(0, prefix.length() - 4); // remove "Help"
		nativeSetSuppliedDirs(prefix);
    }

    // -----------------------------------------------------------------------------

    public void DeleteTempFiles() {
    	File dir = getCacheDir();
    	File[] files = dir.listFiles();
    	if (files != null) {
    	    for (File file : files) file.delete();
    	}
    }

    // -----------------------------------------------------------------------------

    public void UpdateButtons() {
    	if (nativeIsGenerating()) {
    		ssbutton.setText("Stop");
    		ssbutton.setTextColor(Color.rgb(255,0,0));
        	undobutton.setEnabled(nativeAllowUndo());
        	redobutton.setEnabled(false);
    	} else {
    		ssbutton.setText("Start");
    		ssbutton.setTextColor(Color.rgb(0,255,0));
        	undobutton.setEnabled(nativeAllowUndo() && (nativeCanReset() || nativeCanUndo()));
        	redobutton.setEnabled(nativeCanRedo());
    	}
    }

    // -----------------------------------------------------------------------------
    
    // called when the Start/Stop button is tapped
    public void doStartStop(View view) {
    	nativeStartStop();
    	if (nativeIsGenerating()) {
    		// start generating immediately
    		geninterval = nativeCalculateSpeed();
    		genhandler.post(generate);
    	} else {
    		// stop generating
    		genhandler.removeCallbacks(generate);
    	}
    	UpdateButtons();
    }

    // -----------------------------------------------------------------------------
    
    // called when the Step button is tapped
    public void doStep(View view) {
    	genhandler.removeCallbacks(generate);
    	nativeStep();
    	UpdateButtons();
    }

    // -----------------------------------------------------------------------------
    
    // called when the Control button is tapped
    public void doControl(View view) {
    	// display pop-up menu with these items: Step=1, Faster, Slower, Reset
        PopupMenu popup = new PopupMenu(this, view);
        MenuInflater inflater = popup.getMenuInflater();
        inflater.inflate(R.menu.control_menu, popup.getMenu());
        popup.show();
    }

    // -----------------------------------------------------------------------------
    
    // called when item from control_menu is selected
    public void doControlItem(MenuItem item) {
    	genhandler.removeCallbacks(generate);
        switch (item.getItemId()) {
    		case R.id.step1:  nativeStep1(); break;
    		case R.id.faster: nativeFaster(); break;
    		case R.id.slower: nativeSlower(); break;
    		case R.id.reset:  nativeResetPattern(); break;
    		default:          // should never happen
        }
    	if (item.getItemId() == R.id.reset) {
    		UpdateButtons();
    	} else {
        	geninterval = nativeCalculateSpeed();
            if (nativeIsGenerating()) genhandler.post(generate);
    	}
    }

    // -----------------------------------------------------------------------------
    
    // called when the Fit button is tapped
    public void doFitPattern(View view) {
    	nativeFitPattern();
    }

    // -----------------------------------------------------------------------------
    
    // called when the View button is tapped
    public void doView(View view) {
    	// display pop-up menu with these items: Scale=1:1, Bigger, Smaller, Middle
        PopupMenu popup = new PopupMenu(this, view);
        MenuInflater inflater = popup.getMenuInflater();
        inflater.inflate(R.menu.view_menu, popup.getMenu());
        popup.show();
    }

    // -----------------------------------------------------------------------------
    
    // called when item from view_menu is selected
    public void doViewItem(MenuItem item) {
        switch (item.getItemId()) {
    		case R.id.scale1to1: nativeScale1to1(); break;
    		case R.id.bigger:    nativeBigger(); break;
    		case R.id.smaller:   nativeSmaller(); break;
    		case R.id.middle:    nativeMiddle(); break;
    		default:             // should never happen
        }
    }

    // -----------------------------------------------------------------------------
    
    // called when the Undo button is tapped
    public void doUndo(View view) {
    	genhandler.removeCallbacks(generate);
    	nativeUndo();
    	UpdateButtons();
    	
    }

    // -----------------------------------------------------------------------------
    
    // called when the Redo button is tapped
    public void doRedo(View view) {
    	// nativeIsGenerating() should never be true here
    	nativeRedo();
    	UpdateButtons();
    }

    // -----------------------------------------------------------------------------
    
    // called when the Edit/Paste button is tapped
    public void doEdit(View view) {
    	// display pop-up menu with items that depend on whether a selection or paste image exists
        PopupMenu popup = new PopupMenu(this, view);
        MenuInflater inflater = popup.getMenuInflater();
        if (nativePasteExists()) {
        	inflater.inflate(R.menu.paste_menu, popup.getMenu());
        } else if (nativeSelectionExists()) {
        	inflater.inflate(R.menu.selection_menu, popup.getMenu());
        } else {
        	inflater.inflate(R.menu.edit_menu, popup.getMenu());
        }
        popup.show();
    }

    // -----------------------------------------------------------------------------
    
    // called when item from edit_menu is selected
    public void doEditItem(MenuItem item) {
        switch (item.getItemId()) {
    		case R.id.paste: nativePaste(); break;
    		case R.id.all:   nativeSelectAll(); break;
    		default:         // should never happen
        }
        // check if nativePaste() created a paste image
        if (nativePasteExists()) editbutton.setText("Paste");
    }

    // -----------------------------------------------------------------------------
    
    // called when item from selection_menu is selected
    public void doSelectionItem(MenuItem item) {
        switch (item.getItemId()) {
            // let doEditItem handle the top 2 items:
			// case R.id.paste: nativePaste(); break;
			// case R.id.all:   nativeSelectAll(); break;
			case R.id.remove:   nativeRemoveSelection(); break;
            case R.id.cut:      nativeCutSelection(); break;
            case R.id.copy:     nativeCopySelection(); break;
            case R.id.clear:    nativeClearSelection(1); break;
            case R.id.clearo:   nativeClearSelection(0); break;
            case R.id.shrink:   nativeShrinkSelection(); break;
            case R.id.random:   nativeRandomFill(); break;
            case R.id.flipy:    nativeFlipSelection(1); break;
            case R.id.flipx:    nativeFlipSelection(0); break;
            case R.id.rotatec:  nativeRotateSelection(1); break;
            case R.id.rotatea:  nativeRotateSelection(0); break;
            case R.id.advance:  nativeAdvanceSelection(1); break;
            case R.id.advanceo: nativeAdvanceSelection(0); break;
    		default:            // should never happen
        }
    }

    // -----------------------------------------------------------------------------
    
    // called when item from paste_menu is selected
    public void doPasteItem(MenuItem item) {
        switch (item.getItemId()) {
        	case R.id.abort:     nativeAbortPaste(); break;
        	case R.id.pastemode: nativeDoPaste(0); break;
        	case R.id.pastesel:  nativeDoPaste(1); break;
        	case R.id.pflipy:    nativeFlipPaste(1); break;
        	case R.id.pflipx:    nativeFlipPaste(0); break;
        	case R.id.protatec:  nativeRotatePaste(1); break;
        	case R.id.protatea:  nativeRotatePaste(0); break;
    		default:             // should never happen
        }
        // if paste image no longer exists then change Paste button back to Edit
        if (!nativePasteExists()) editbutton.setText("Edit");
    }
    
    // -----------------------------------------------------------------------------
    
    // called when the Draw/Pick/Select/Move button is tapped
    public void doSetTouchMode(View view) {
    	// display pop-up menu with these items: Draw, Pick, Select, Move
        PopupMenu popup = new PopupMenu(this, view);
        MenuInflater inflater = popup.getMenuInflater();
        inflater.inflate(R.menu.mode_menu, popup.getMenu());
        popup.show();
    }

    // -----------------------------------------------------------------------------
    
    // called when item from mode_menu is selected
    public void doModeItem(MenuItem item) {
        switch (item.getItemId()) {
        	case R.id.draw:   nativeSetMode(0); break;
        	case R.id.pick:   nativeSetMode(1); break;
        	case R.id.select: nativeSetMode(2); break;
        	case R.id.move:   nativeSetMode(3); break;
        	default:		  // should never happen
        }
        UpdateEditBar();	  // update modebutton text
    }

    // -----------------------------------------------------------------------------
    
    // called when the New button is tapped
    public void doNewPattern(View view) {
    	genhandler.removeCallbacks(generate);
    	nativeNewPattern();
    	UpdateButtons();
    	UpdateEditBar();
    	
    	if (nativeNumLayers() == 1) {
    		// delete all files in tempdir
        	DeleteTempFiles();
    	}
    }

    // -----------------------------------------------------------------------------
    
    // called when the Rule button is tapped
    public void doRule(View view) {
    	// not yet implemented!!!
    }

    // -----------------------------------------------------------------------------
    
    // called when the Info button is tapped
    public void doInfo(View view) {
    	// not yet implemented!!!
    }

    // -----------------------------------------------------------------------------
    
    // called when the Save button is tapped
    public void doSave(View view) {
    	// not yet implemented!!!
    }

    // -----------------------------------------------------------------------------
    
    // called when the Full button is tapped
    public void doFullScreen(View view) {
    	// not yet implemented!!!
    }

    // -----------------------------------------------------------------------------

    // this method is called from C++ code (see jnicalls.cpp)
    private void RefreshPattern() {
    	// this can be called from any thread
    	pattView.requestRender();
    }

    // -----------------------------------------------------------------------------

    // this method is called from C++ code (see jnicalls.cpp)
    private void ShowStatusLines() {
    	// this might be called from a non-UI thread
        runOnUiThread(new Runnable() {
            public void run() {
                status1.setText(nativeGetStatusLine(1));
                status2.setText(nativeGetStatusLine(2));
                status3.setText(nativeGetStatusLine(3));
        	}
        });
    }

    // -----------------------------------------------------------------------------

    // this method is called from C++ code (see jnicalls.cpp)
    private void UpdateEditBar() {
    	// this might be called from a non-UI thread
        runOnUiThread(new Runnable() {
            public void run() {
                undobutton.setEnabled(nativeCanUndo());
                redobutton.setEnabled(nativeCanRedo());
                switch (nativeGetMode()) {
        			case 0: modebutton.setText("Draw"); break;
        			case 1: modebutton.setText("Pick"); break;
        			case 2: modebutton.setText("Select"); break;
        			case 3: modebutton.setText("Move"); break;
        			default: // should never happen
                }
                //!!! also show current drawing state
        	}
        });
    }

    // -----------------------------------------------------------------------------

    // this method is called from C++ code (see jnicalls.cpp)
    private void CheckMessageQueue() {
    	// not yet implemented!!!
        // use Looper on main UI thread???!!!
        // see http://developer.android.com/reference/android/os/Looper.html
    	//??? see http://stackoverflow.com/questions/4994263/how-can-i-do-non-blocking-events-processing-on-android?rq=1
    }

    // -----------------------------------------------------------------------------

    // this method is called from C++ code (see jnicalls.cpp)
    private void CopyTextToClipboard(String text) {
    	// see http://developer.android.com/guide/topics/text/copy-paste.html
    	ClipboardManager clipboard = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
    	ClipData clip = ClipData.newPlainText("RLE data", text);
    	clipboard.setPrimaryClip(clip);
    	// Log.i("CopyTextToClipboard", text);
    }

    // -----------------------------------------------------------------------------

    // this method is called from C++ code (see jnicalls.cpp)
    private String GetTextFromClipboard() {
    	// see http://developer.android.com/guide/topics/text/copy-paste.html
    	ClipboardManager clipboard = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
    	String text = "";
    	if (clipboard.hasPrimaryClip()) {
    		ClipData.Item item = clipboard.getPrimaryClip().getItemAt(0);
    		text = (String) item.getText();
    	}
    	// Log.i("GetTextFromClipboard", text);
    	return text;
    }

} // MainActivity class
