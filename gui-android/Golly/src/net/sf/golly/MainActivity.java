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

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.AssetManager;
import android.graphics.Color;
import android.media.AudioManager;
import android.media.ToneGenerator;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.MessageQueue;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.PopupMenu;
import android.widget.TextView;

public class MainActivity extends Activity {

    // see jnicalls.cpp for these native routines:
    private static native void nativeClassInit();   // must be static
    private native void nativeCreate();             // must NOT be static
    private native void nativeDestroy();
    private native void nativeSetGollyDir(String path);
    private native void nativeSetTempDir(String path);
    private native void nativeSetSuppliedDirs(String prefix);
    private native void nativeStartGenerating();
    private native void nativeStopGenerating();
    private native void nativeStopBeforeNew();
    private native void nativePauseGenerating();
    private native void nativeResumeGenerating();
    private native void nativeResetPattern();
    private native void nativeUndo();
    private native void nativeRedo();
    private native boolean nativeCanReset();
    private native boolean nativeAllowUndo();
    private native boolean nativeCanUndo();
    private native boolean nativeCanRedo();
    private native boolean nativeIsGenerating();
    private native int nativeGetStatusColor();
    private native String nativeGetStatusLine(int line);
    private native String nativeGetPasteMode();
    private native String nativeGetRandomFill();
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
    private native void nativeClearMessage();
    private native String nativeGetValidExtensions();
    private native boolean nativeValidExtension(String filename);
    private native boolean nativeFileExists(String filename);
    private native void nativeSavePattern(String filename);
    private native void nativeOpenFile(String filepath);

    // local fields:
    private static boolean firstcall = true;
    private Button ssbutton;                        // Start/Stop button
    private Button undobutton;                      // Undo button
    private Button redobutton;                      // Redo button
    private Button editbutton;                      // Edit/Paste button
    private Button modebutton;                      // Draw/Pick/Select/Move button
    private TextView status1, status2, status3;     // status bar has 3 lines
    private int statuscolor = 0xFF000000;           // background color of status bar
    private PatternGLSurfaceView pattView;          // OpenGL ES is used to draw patterns
    private Handler genhandler;                     // for generating patterns
    private Runnable generate;                      // code started/stopped by genhandler
    private int geninterval;                        // interval between nativeGenerate calls (in millisecs)
    private Handler callhandler;                    // for calling a method again
    private Runnable callagain;                     // code that calls method
    private String methodname;                      // name of method to call
    private View currview;                          // current view parameter
    private MenuItem curritem;                      // current menu item parameter
    private boolean stopped = true;                 // generating is stopped?
    
    // -----------------------------------------------------------------------------

    // the following stuff allows time consuming code (like nativeGenerate) to periodically
    // check if any user events need to be processed, but without blocking the UI thread
    // (thanks to http://stackoverflow.com/questions/4994263/how-can-i-do-non-blocking-events-processing-on-android)
    
    private boolean processingevents = false;
    private Handler evthandler = null;

    private Runnable doevents = new Runnable() {
        public void run() {
            Looper looper = Looper.myLooper();
            looper.quit();
            evthandler.removeCallbacks(this);    
            evthandler = null;
        }
    };

    private class IdleHandler implements MessageQueue.IdleHandler {
        private Looper idlelooper;
        protected IdleHandler(Looper looper) {
            idlelooper = looper;
        }
        public boolean queueIdle() {
            evthandler = new Handler(idlelooper);
            evthandler.post(doevents);
            return(false);
        }
    };

    // this method is called from C++ code (see jnicalls.cpp)
    private void CheckMessageQueue() {
        // process any pending UI events in message queue
        if (!processingevents) {
            Looper.myQueue().addIdleHandler(new IdleHandler(Looper.myLooper()));
            processingevents = true;
            try {
                Looper.loop();
            } catch (RuntimeException re) {
                // looper.quit() in doevents causes an exception
            }
            processingevents = false;
        }
    }
    
    // -----------------------------------------------------------------------------
    
    static {
        System.loadLibrary("stlport_shared");   // must agree with Application.mk
        System.loadLibrary("golly");            // loads libgolly.so
        nativeClassInit();                      // caches Java method IDs
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
            initPaths();        // sets tempdir, etc
        }
        nativeCreate();         // must be called every time (to cache jobject)
        
        // this will call the PatternGLSurfaceView constructor
        pattView = (PatternGLSurfaceView) findViewById(R.id.patternview);
        
        // check for messages sent by other activities
        Intent intent = getIntent();
        String filepath = intent.getStringExtra(OpenActivity.OPENFILE_MESSAGE);
        if (filepath != null) {
            nativeOpenFile(filepath);
        }
        
        // create handler and runnable for generating patterns
        geninterval = nativeCalculateSpeed();
        genhandler = new Handler();
        generate = new Runnable() {
            public void run() {
                if (!stopped) {
                    nativeGenerate();
                    genhandler.postDelayed(this, geninterval);
                    // nativeGenerate will be called again after given interval
                }
            }
        };
        
        // create handler and runnable for calling a method again when the user
        // invokes certain events while the next generation is being calculated
        callhandler = new Handler();
        callagain = new Runnable() {
            public void run() {
                if (methodname.equals("doStartStop")) doStartStop(currview); else
                if (methodname.equals("doStep"))      doStep(currview); else
                if (methodname.equals("doNew"))       doNewPattern(currview); else
                if (methodname.equals("doUndo"))      doUndo(currview); else
                if (methodname.equals("doRedo"))      doRedo(currview); else
                if (methodname.equals("doSave"))      doSave(currview); else
                if (methodname.equals("doReset"))     doControlItem(curritem); else
                if (methodname.equals("doPaste"))     doEditItem(curritem); else
                if (methodname.equals("doSelItem"))   doSelectionItem(curritem); else
                // string mismatch
                Log.e("Fix callagain", methodname);
            }
        };
    }

    // -----------------------------------------------------------------------------

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // add main.xml items to the action bar
        getMenuInflater().inflate(R.menu.main, menu);
        return true;
    }

    // -----------------------------------------------------------------------------

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // action bar item has been tapped
        nativeClearMessage();
        Intent intent;
        switch (item.getItemId()) {
            case R.id.open:
                intent = new Intent(this, OpenActivity.class);
                startActivity(intent);
                return true;
            case R.id.settings:
                intent = new Intent(this, SettingsActivity.class);
                startActivity(intent);
                return true;
            case R.id.help:
                intent = new Intent(this, HelpActivity.class);
                startActivity(intent);
                return true;
        }
        return super.onOptionsItemSelected(item);
    }

    // -----------------------------------------------------------------------------

    @Override
    protected void onPause() {
        super.onPause();
        pattView.onPause();
        stopped = true;
    }

    // -----------------------------------------------------------------------------

    @Override
    protected void onResume() {
        super.onResume();
        pattView.onResume();
        UpdateStartStopButtons();
        UpdateEditBar();
        if (nativeIsGenerating()) {
            stopped = false;
            genhandler.post(generate);
        }
    }

    // -----------------------------------------------------------------------------

    @Override
    protected void onDestroy() {
        stopped = true;             // should have been done in OnPause, but play safe
        nativeDestroy();
        super.onDestroy();
    }

    // -----------------------------------------------------------------------------

    public void unzipAsset(String zipname, File destdir) {
        // Log.i("unzipping", zipname);
        AssetManager am = getAssets();
        try {
            ZipInputStream zipstream = new ZipInputStream(am.open(zipname));
            for (ZipEntry entry = zipstream.getNextEntry(); entry != null; entry = zipstream.getNextEntry()) {
                // Log.i("entry", entry.getName());
                File destfile = new File(destdir, entry.getName());
                if (entry.isDirectory()) {
                    // create any missing sub-directories
                    destfile.mkdirs();
                } else {
                    // create a file
                    final int BUFFSIZE = 8192;
                    BufferedOutputStream buffstream = new BufferedOutputStream(new FileOutputStream(destfile), BUFFSIZE);
                    int count = 0;
                    byte[] data = new byte[BUFFSIZE];
                    while ((count = zipstream.read(data, 0, BUFFSIZE)) != -1) {
                        buffstream.write(data, 0, count);
                    }
                    buffstream.flush();
                    buffstream.close();
                }
                zipstream.closeEntry();
            }
            zipstream.close();
        } catch (Exception e) {
            Log.e("Golly", "unzipAsset failed!", e);
        }
        // Log.i("finished", zipname);
    }
    
    // -----------------------------------------------------------------------------

    public void initPaths() {
        // first create subdirs if they don't exist
        File filesdir = getFilesDir();
        File subdir;
        subdir = new File(filesdir, "Rules");           // for user's .rule files
        subdir.mkdir();
        subdir = new File(filesdir, "Saved");           // for saved pattern files
        subdir.mkdir();
        subdir = new File(filesdir, "Downloads");       // for downloaded pattern files
        subdir.mkdir();

        // or use this location (normally on SD card) instead of getFilesDir()???!!!
        // see http://developer.android.com/training/basics/data-storage/files.html
        // for how to check if external storage device is mounted
        // filesdir = getExternalFilesDir(null);
        // if (filesdir != null) Log.i("getExternalFilesDir", filesdir.getAbsolutePath());
        // on emulator filesdir = /mnt/sdcard/Android/data/net.sf.golly/files
        
        // now set gollydir (parent of above subdirs)
        String gollydir = filesdir.getAbsolutePath();   // /data/data/net.sf.golly/files
        nativeSetGollyDir(gollydir);
        
        // set tempdir
        File dir = getCacheDir();
        String tempdir = dir.getAbsolutePath();         // /data/data/net.sf.golly/cache
        nativeSetTempDir(tempdir);
        
        
        // create a directory for storing supplied Patterns/Rules/Help then
        // create subdirs for each by unzipping .zip files stored in assets
        subdir = new File(filesdir, "Supplied");
        subdir.mkdir();
        unzipAsset("Patterns.zip", subdir);
        unzipAsset("Rules.zip", subdir);
        // unzipAsset("Help.zip", subdir);
        
        // subdir = /data/data/net.sf.golly/files/Supplied
        nativeSetSuppliedDirs(subdir.getAbsolutePath());
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

    public void UpdateStartStopButtons() {
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

    public boolean CallAgainAfterDelay(String callname, View view, MenuItem item) {
        if (processingevents) {
            // CheckMessageQueue has been called inside a (possibly) lengthy task
            // so call the given method again after a short delay
            methodname = callname;
            if (view != null) currview = view;
            if (item != null) curritem = item;
            callhandler.postDelayed(callagain, 5);    // call after 5 millisecs
            return true;
        } else {
            return false;
        }
    }

    // -----------------------------------------------------------------------------
    
    // called when the Start/Stop button is tapped
    public void doStartStop(View view) {
        if (CallAgainAfterDelay("doStartStop", view, null)) return;
        if (nativeIsGenerating()) {
            // stop calling nativeGenerate
            stopped = true;
            nativeStopGenerating();
        } else {
            nativeStartGenerating();
            // nativeIsGenerating() might still be false (eg. if pattern is empty)
            if (nativeIsGenerating()) {
                // start calling nativeGenerate
                geninterval = nativeCalculateSpeed();
                stopped = false;
                genhandler.post(generate);
            }
        }
        UpdateStartStopButtons();
    }

    // -----------------------------------------------------------------------------

    public void StopIfGenerating() {
        if (nativeIsGenerating()) {
            // note that genhandler.removeCallbacks(generate) doesn't work if
            // processingevents is true, so we use a global flag to start/stop
            // making calls to nativeGenerate
            stopped = true;
            nativeStopGenerating();
        }
    }

    // -----------------------------------------------------------------------------
    
    // called when the Step button is tapped
    public void doStep(View view) {
        StopIfGenerating();
        if (CallAgainAfterDelay("doStep", view, null)) return;
        nativeStep();
        UpdateStartStopButtons();
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
        if (item.getItemId() == R.id.reset) {
            StopIfGenerating();
            if (CallAgainAfterDelay("doReset", null, item)) return;
        }
        switch (item.getItemId()) {
            case R.id.step1:  nativeStep1(); break;
            case R.id.faster: nativeFaster(); break;
            case R.id.slower: nativeSlower(); break;
            case R.id.reset:  nativeResetPattern(); break;
            default:          Log.e("Golly","Fix bug in doControlItem!");
        }
        if (item.getItemId() == R.id.reset) {
            UpdateStartStopButtons();
        } else {
            geninterval = nativeCalculateSpeed();
            stopped = false;
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
            default:             Log.e("Golly","Fix bug in doViewItem!");
        }
    }

    // -----------------------------------------------------------------------------
    
    // called when the Undo button is tapped
    public void doUndo(View view) {
        StopIfGenerating();
        if (CallAgainAfterDelay("doUndo", view, null)) return;
        nativeUndo();
        UpdateStartStopButtons();
        
    }

    // -----------------------------------------------------------------------------
    
    // called when the Redo button is tapped
    public void doRedo(View view) {
        // nativeIsGenerating() should never be true here
        if (CallAgainAfterDelay("doRedo", view, null)) return;
        nativeRedo();
        UpdateStartStopButtons();
    }

    // -----------------------------------------------------------------------------
    
    // called when the Edit/Paste button is tapped
    public void doEditPaste(View view) {
        // display pop-up menu with items that depend on whether a selection or paste image exists
        PopupMenu popup = new PopupMenu(this, view);
        MenuInflater inflater = popup.getMenuInflater();
        if (nativePasteExists()) {
            Menu menu = popup.getMenu();
            inflater.inflate(R.menu.paste_menu, menu);
            MenuItem item = menu.findItem(R.id.pastesel);
            item.setEnabled(nativeSelectionExists());
            item = menu.findItem(R.id.pastemode);
            item.setTitle("Paste (" + nativeGetPasteMode() + ")");
            if (nativeIsGenerating()) {
                // probably best to stop generating when Paste button is tapped
                // (consistent with iOS Golly)
                stopped = true;
                nativeStopGenerating();
                UpdateStartStopButtons();
            }
        } else if (nativeSelectionExists()) {
            Menu menu = popup.getMenu();
            inflater.inflate(R.menu.selection_menu, menu);
            MenuItem item = menu.findItem(R.id.random);
            item.setTitle("Random Fill (" + nativeGetRandomFill() + "%)");
        } else {
            inflater.inflate(R.menu.edit_menu, popup.getMenu());
        }
        popup.show();
    }

    // -----------------------------------------------------------------------------
    
    // called when item from edit_menu is selected
    public void doEditItem(MenuItem item) {
        if (item.getItemId() == R.id.paste) {
            StopIfGenerating();
            if (CallAgainAfterDelay("doPaste", null, item)) return;
        }
        switch (item.getItemId()) {
            case R.id.paste: nativePaste(); break;
            case R.id.all:   nativeSelectAll(); break;
            default:         Log.e("Golly","Fix bug in doEditItem!");
        }
        UpdateStartStopButtons();
        UpdateEditBar();
    }

    // -----------------------------------------------------------------------------
    
    // called when item from selection_menu is selected
    public void doSelectionItem(MenuItem item) {
        if (item.getItemId() != R.id.remove &&
            item.getItemId() != R.id.copy &&
            item.getItemId() != R.id.shrink) {
            // item can modify the current pattern so we must stop generating,
            // but nicer if we only stop temporarily and resume when done
            if (nativeIsGenerating()) {
                // no need to set stopped = true
                nativePauseGenerating();
            }
            if (CallAgainAfterDelay("doSelItem", null, item)) return;
        }
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
            default:            Log.e("Golly","Fix bug in doSelectionItem!");
        }
        // resume generating (only if nativePauseGenerating was called)
        nativeResumeGenerating();
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
            default:             Log.e("Golly","Fix bug in doPasteItem!");
        }
        // if paste image no longer exists then change Paste button back to Edit
        UpdateEditBar();
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
            default:          Log.e("Golly","Fix bug in doModeItem!");
        }
        UpdateEditBar();      // update modebutton text
    }

    // -----------------------------------------------------------------------------
    
    // called when the New button is tapped
    public void doNewPattern(View view) {
        // StopIfGenerating() would work here but we use smarter code that
        // avoids trying to save the current pattern (potentially very large)
        if (nativeIsGenerating()) {
            stopped = true;
            nativeStopBeforeNew();
        }
        if (CallAgainAfterDelay("doNew", view, null)) return;
        nativeNewPattern();
        UpdateStartStopButtons();
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
        nativeClearMessage();
        StopIfGenerating();
        if (CallAgainAfterDelay("doSave", view, null)) return;
        
        AlertDialog.Builder alert = new AlertDialog.Builder(this);

        alert.setTitle("Save current pattern");
        alert.setMessage("Valid file name extensions are\n" + nativeGetValidExtensions());
        
        // or use radio buttons as in iPad Golly???!!!
        // might be better to create a new SaveActivity and make it appear in a dialog
        // by setting its theme to Theme.Holo.Dialog in the <activity> manifest element:
        // <activity android:theme="@android:style/Theme.Holo.Dialog">

        // use an EditText view to get filename
        final EditText input = new EditText(this);
        input.setSingleLine(true);
        input.setHint("enter a file name");
        alert.setView(input);

        alert.setPositiveButton("SAVE",
            new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int id) {
                    // do nothing here (see below)
                }
            });
        alert.setNegativeButton("CANCEL",
            new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int id) {
                    dialog.cancel();
                }
            });

        // don't call alert.show() here -- instead we use the following stuff
        // which allows us to prevent the alert dialog from closing if the
        // given file name is invalid
        
        final AlertDialog dialog = alert.create();
        dialog.show();
        dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(
            new View.OnClickListener() {
                public void onClick(View v) {
                    String filename = input.getText().toString();
                    if (filename.length() == 0) {
                        PlayBeepSound();
                        return;
                    }
                    // check for valid extension
                    if (!nativeValidExtension(filename)) {
                        Warning("Invalid file extension.");
                        return;
                    }
                    // check if given file already exists
                    if (nativeFileExists(filename)) {
                        String answer = YesNo("A file with that name already exists.  Do you want to replace that file?");
                        if (answer.equals("no")) return;
                    }
                     // dismiss dialog first in case SavePattern calls BeginProgress
                    dialog.dismiss();
                    nativeSavePattern(filename);
                }
            });
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
                int bgcolor = nativeGetStatusColor();
                if (statuscolor != bgcolor) {
                    // change background color of status lines
                    statuscolor = bgcolor;
                    status1.setBackgroundColor(statuscolor);
                    status2.setBackgroundColor(statuscolor);
                    status3.setBackgroundColor(statuscolor);
                }
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
                
                if (nativePasteExists())
                    editbutton.setText("Paste");
                else
                    editbutton.setText("Edit");
                
                switch (nativeGetMode()) {
                    case 0:  modebutton.setText("Draw"); break;
                    case 1:  modebutton.setText("Pick"); break;
                    case 2:  modebutton.setText("Select"); break;
                    case 3:  modebutton.setText("Move"); break;
                    default: Log.e("Golly","Fix bug in UpdateEditBar!");
                }
                
                //!!! also show current drawing state in another button
            }
        });
    }

    // -----------------------------------------------------------------------------

    // this method is called from C++ code (see jnicalls.cpp)
    private void PlayBeepSound() {
        ToneGenerator tg = new ToneGenerator(AudioManager.STREAM_NOTIFICATION, 100);
        if (tg != null) {
            tg.startTone(ToneGenerator.TONE_PROP_BEEP);
            tg.release();
        }
    }
    
    // -----------------------------------------------------------------------------

    // this method is called from C++ code (see jnicalls.cpp)
    private void Warning(String msg) {
        // use a handler to get a modal dialog
        final Handler handler = new Handler() {
            public void handleMessage(Message mesg) {
                throw new RuntimeException();
            } 
        };
        
        AlertDialog.Builder alert = new AlertDialog.Builder(this);
        alert.setTitle("Warning");
        alert.setMessage(msg);
        alert.setNegativeButton("CANCEL",
            new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int id) {
                    dialog.cancel();
                    handler.sendMessage(handler.obtainMessage());
                }
            });
        alert.show();
        
        // loop until runtime exception is triggered
        try { Looper.loop(); } catch(RuntimeException re) {}
    }
    
    // -----------------------------------------------------------------------------

    // this method is called from C++ code (see jnicalls.cpp)
    private void Fatal(String msg) {
        // use a handler to get a modal dialog
        final Handler handler = new Handler() {
            public void handleMessage(Message mesg) {
                throw new RuntimeException();
            } 
        };
        
        AlertDialog.Builder alert = new AlertDialog.Builder(this);
        alert.setTitle("Fatal error!");
        alert.setMessage(msg);
        alert.setNegativeButton("QUIT",
            new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int id) {
                    dialog.cancel();
                    handler.sendMessage(handler.obtainMessage());
                }
            });
        alert.show();
        
        // loop until runtime exception is triggered
        try { Looper.loop(); } catch(RuntimeException re) {}
        
        System.exit(1);
    }
    
    // -----------------------------------------------------------------------------

    private String answer;
    
    // this method is called from C++ code (see jnicalls.cpp)
    private String YesNo(String query) {
        // use a handler to get a modal dialog
        final Handler handler = new Handler() {
            public void handleMessage(Message mesg) {
                throw new RuntimeException();
            } 
        };
        
        AlertDialog.Builder alert = new AlertDialog.Builder(this);
        alert.setTitle("A question...");
        alert.setMessage(query);
        alert.setPositiveButton("YES",
            new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int id) {
                    answer = "yes";
                    dialog.cancel();
                    handler.sendMessage(handler.obtainMessage());
                }
            });
        alert.setNegativeButton("NO",
            new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int id) {
                    answer = "no";
                    dialog.cancel();
                    handler.sendMessage(handler.obtainMessage());
                }
            });
        alert.show();
        
        // loop until runtime exception is triggered
        try { Looper.loop(); } catch(RuntimeException re) {}

        return answer;
    }
    
    // -----------------------------------------------------------------------------

    // this method is called from C++ code (see jnicalls.cpp)
    private void RemoveFile(String filepath) {
        File file = new File(filepath);
        if (!file.delete()) {
            Log.e("RemoveFile failed", filepath);
        }
    }
    
    // -----------------------------------------------------------------------------

    // this method is called from C++ code (see jnicalls.cpp)
    private String MoveFile(String oldpath, String newpath) {
        String result = "";
        File oldfile = new File(oldpath);
        File newfile = new File(newpath);
        if (!oldfile.renameTo(newfile)) {
            Log.e("MoveFile failed: old", oldpath);
            Log.e("MoveFile failed: new", newpath);
            result = "MoveFile failed";
        }
        return result;
    }
    
    // -----------------------------------------------------------------------------

    // this method is called from C++ code (see jnicalls.cpp)
    private void CopyTextToClipboard(String text) {
        ClipboardManager clipboard = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
        ClipData clip = ClipData.newPlainText("RLE data", text);
        clipboard.setPrimaryClip(clip);
        // Log.i("CopyTextToClipboard", text);
    }

    // -----------------------------------------------------------------------------

    // this method is called from C++ code (see jnicalls.cpp)
    private String GetTextFromClipboard() {
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
