// This file is part of Golly.
// See docs/License.html for the copyright notice.

package net.sf.golly;

import java.io.File;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.media.AudioManager;
import android.media.ToneGenerator;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.MessageQueue;
import android.text.InputType;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.ProgressBar;
import android.widget.RelativeLayout;
import android.widget.TextView;

public class MainActivity extends Activity {

    // see jnicalls.cpp for these native routines:
    private static native void nativeClassInit();   // this MUST be static
    private native void nativeCreate();             // the rest must NOT be static
    private native void nativeDestroy();
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
    private native boolean nativeInfoAvailable();
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
    private native void nativeFitSelection();
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
    private native void nativeSetFullScreen(boolean fullscreen);
    private native void nativeChangeRule(String rule);
    private native void nativeLexiconPattern(String pattern);
    private native int nativeDrawingState();

    // local fields:
    private static boolean fullscreen = false;      // in full screen mode?
    private boolean widescreen;                     // use different layout for wide screens?
    private Button ssbutton;                        // Start/Stop button
    private Button resetbutton;                     // Reset button (used if wide screen)
    private Button undobutton;                      // Undo button
    private Button redobutton;                      // Redo button
    private Button editbutton;                      // Edit/Paste button
    private Button statebutton;                     // button to change drawing state
    private Button modebutton;                      // Draw/Pick/Select/Move button
    private Button drawbutton;                      // Draw button (used if wide screen)
    private Button pickbutton;                      // Pick button (used if wide screen)
    private Button selectbutton;                    // Select button (used if wide screen)
    private Button movebutton;                      // Move button (used if wide screen)
    private Button infobutton;                      // Info button
    private Button restorebutton;                   // Restore button
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
    private PopupMenu popup;                        // used for all pop-up menus
    private boolean stopped = true;                 // generating is stopped?

    // -----------------------------------------------------------------------------
    
    // this stuff is used to display a progress bar:
    
    private static boolean cancelProgress = false;  // cancel progress dialog?
    private static long progstart, prognext;        // for progress timing
    private static int progresscount = 0;           // if > 0 then BeginProgress has been called
    private TextView progtitle;                     // title above progress bar
    private ProgressBar progbar;                    // progress bar
    private LinearLayout proglayout;                // view containing progress bar

    // -----------------------------------------------------------------------------
    
    // this stuff is used in other activities:
    
    public final static String OPENFILE_MESSAGE = "net.sf.golly.OPENFILE";
    public final static String RULE_MESSAGE = "net.sf.golly.RULE";
    public final static String LEXICON_MESSAGE = "net.sf.golly.LEXICON";
    
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
            Looper looper = Looper.myLooper();
            looper.myQueue().addIdleHandler(new IdleHandler(looper));
            processingevents = true;
            try {
                looper.loop();
            } catch (RuntimeException re) {
                // looper.quit() in doevents causes an exception
            }
            processingevents = false;
        }
    }

    // -----------------------------------------------------------------------------
    
    static {
        nativeClassInit();      // caches Java method IDs
    }

    // -----------------------------------------------------------------------------
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        // Log.i("Golly","screen density in dpi = " + Integer.toString(metrics.densityDpi));
        // eg. densityDpi = 320 on Nexus 7
        
        DisplayMetrics displayMetrics = getResources().getDisplayMetrics();
        float dpWidth = displayMetrics.widthPixels / displayMetrics.density;

        // Log.i("Golly","screen width in dp = " + Integer.toString(config.screenWidthDp));
        // eg. on Nexus 7 screenWidthDp = 600 in portrait, 960 in landscape
        widescreen = dpWidth >= 600;
        
        if (widescreen) {
            // use a layout that has more buttons
            setContentView(R.layout.main_layout_wide);
            resetbutton = (Button) findViewById(R.id.reset);
            drawbutton = (Button) findViewById(R.id.drawmode);
            pickbutton = (Button) findViewById(R.id.pickmode);
            selectbutton = (Button) findViewById(R.id.selectmode);
            movebutton = (Button) findViewById(R.id.movemode);
            if (dpWidth >= 800) {
                // show nicer text in some buttons
                ((Button) findViewById(R.id.slower)).setText("Slower");
                ((Button) findViewById(R.id.faster)).setText("Faster");
                ((Button) findViewById(R.id.smaller)).setText("Smaller");
                ((Button) findViewById(R.id.scale1to1)).setText("Scale=1:1");
                ((Button) findViewById(R.id.bigger)).setText("Bigger");
                ((Button) findViewById(R.id.middle)).setText("Middle");
            }
        } else {
            // screen isn't wide enough to have all the buttons we'd like
            setContentView(R.layout.main_layout);
            modebutton = (Button) findViewById(R.id.touchmode);
        }
        
        // following stuff is used in both layouts
        ssbutton = (Button) findViewById(R.id.startstop);
        undobutton = (Button) findViewById(R.id.undo);
        redobutton = (Button) findViewById(R.id.redo);
        editbutton = (Button) findViewById(R.id.edit);
        statebutton = (Button) findViewById(R.id.state);
        infobutton = (Button) findViewById(R.id.info);
        restorebutton = (Button) findViewById(R.id.restore);
        status1 = (TextView) findViewById(R.id.status1);
        status2 = (TextView) findViewById(R.id.status2);
        status3 = (TextView) findViewById(R.id.status3);
        progbar = (ProgressBar) findViewById(R.id.progress_bar);
        proglayout = (LinearLayout) findViewById(R.id.progress_layout);
        progtitle = (TextView) findViewById(R.id.progress_title);
        
        nativeCreate();         // must be called every time (to cache this instance)
        
        // this will call the PatternGLSurfaceView constructor
        pattView = (PatternGLSurfaceView) findViewById(R.id.patternview);
        
        restorebutton.setVisibility(View.INVISIBLE);
        proglayout.setVisibility(LinearLayout.INVISIBLE);

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
                if (methodname.equals("doRule"))      doRule(currview); else
                if (methodname.equals("doInfo"))      doInfo(currview); else
                if (methodname.equals("doSave"))      doSave(currview); else
                if (methodname.equals("doReset"))     doReset(currview); else
                if (methodname.equals("doPaste"))     doPaste(currview); else
                if (methodname.equals("doSelItem"))   doSelectionItem(curritem); else
                // string mismatch
                Log.e("Fix callagain", methodname);
            }
        };
    }
    
    // -----------------------------------------------------------------------------
    
    @Override
    protected void onNewIntent(Intent intent) {
    	// check for messages sent by other activities
    	String filepath = intent.getStringExtra(OPENFILE_MESSAGE);
    	if (filepath != null) {
    	    nativeOpenFile(filepath);
    	}
    	String rule = intent.getStringExtra(RULE_MESSAGE);
    	if (rule != null) {
    	    nativeChangeRule(rule);
    	}
    	String pattern = intent.getStringExtra(LEXICON_MESSAGE);
    	if (pattern != null) {
    	    nativeLexiconPattern(pattern);
    	}
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
        
        if (fullscreen) {
            fullscreen = false;         // following will set it true
            toggleFullScreen(null);
        } else {
            updateButtons();
            UpdateEditBar();
        }
        
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

    private void deleteTempFiles() {
        File dir = getCacheDir();
        File[] files = dir.listFiles();
        if (files != null) {
            for (File file : files) {
                if (file.getName().startsWith("GET-") || file.getName().endsWith(".html")) {
                    // don't delete files created via "get:" links in HelpActivity
                } else {
                    file.delete();
                }
            }
        }
    }

    // -----------------------------------------------------------------------------

    private void updateButtons() {
        if (fullscreen) return;
        
        if (nativeIsGenerating()) {
            ssbutton.setText("Stop");
            ssbutton.setTextColor(Color.rgb(255,0,0));
            if (widescreen) resetbutton.setEnabled(true);
            undobutton.setEnabled(nativeAllowUndo());
            redobutton.setEnabled(false);
        } else {
            ssbutton.setText("Start");
            ssbutton.setTextColor(Color.rgb(0,255,0));
            if (widescreen) resetbutton.setEnabled(nativeCanReset());
            undobutton.setEnabled(nativeAllowUndo() && (nativeCanReset() || nativeCanUndo()));
            redobutton.setEnabled(nativeCanRedo());
        }
        infobutton.setEnabled(nativeInfoAvailable());
    }

    // -----------------------------------------------------------------------------

    private void updateGeneratingSpeed() {
        geninterval = nativeCalculateSpeed();
        genhandler.removeCallbacks(generate);
        stopped = false;
        if (nativeIsGenerating()) genhandler.post(generate);
    }

    // -----------------------------------------------------------------------------

    private boolean callAgainAfterDelay(String callname, View view, MenuItem item) {
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
        nativeClearMessage();
        if (callAgainAfterDelay("doStartStop", view, null)) return;
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
                genhandler.removeCallbacks(generate);   // probably unnecessary here but play safe
                stopped = false;
                genhandler.post(generate);
            }
        }
        updateButtons();
    }

    // -----------------------------------------------------------------------------

    private void stopIfGenerating() {
        if (nativeIsGenerating()) {
            // note that genhandler.removeCallbacks(generate) doesn't always work if
            // processingevents is true, so we use a global flag to start/stop
            // making calls to nativeGenerate
            stopped = true;
            nativeStopGenerating();
        }
    }

    // -----------------------------------------------------------------------------
    
    // called when the Step button is tapped
    public void doStep(View view) {
        nativeClearMessage();
        stopIfGenerating();
        if (callAgainAfterDelay("doStep", view, null)) return;
        nativeStep();
        updateButtons();
    }

    // -----------------------------------------------------------------------------
    
    // called when the Slower button is tapped
    public void doSlower(View view) {
        nativeClearMessage();
        nativeSlower();
        updateGeneratingSpeed();
    }

    // -----------------------------------------------------------------------------
    
    // called when the Step=1 button is tapped
    public void doStep1(View view) {
        nativeClearMessage();
        nativeStep1();
        updateGeneratingSpeed();
    }

    // -----------------------------------------------------------------------------
    
    // called when the Faster button is tapped
    public void doFaster(View view) {
        nativeClearMessage();
        nativeFaster();
        updateGeneratingSpeed();
    }

    // -----------------------------------------------------------------------------
    
    // called when the Reset button is tapped
    public void doReset(View view) {
        nativeClearMessage();
        stopIfGenerating();
        if (callAgainAfterDelay("doReset", view, null)) return;
        nativeResetPattern();
        updateButtons();
    }

    // -----------------------------------------------------------------------------
    
    // called when the Control button is tapped
    public void doControl(View view) {
        nativeClearMessage();
        // display pop-up menu with these items: Step=1, Faster, Slower, Reset
        popup = new PopupMenu(this, view);
        MenuInflater inflater = popup.getMenuInflater();
        inflater.inflate(R.menu.control_menu, popup.getMenu());
        popup.show();
    }

    // -----------------------------------------------------------------------------
    
    // called when item from control_menu is selected
    public void doControlItem(MenuItem item) {
        if (item.getItemId() == R.id.reset) {
            stopIfGenerating();
            if (callAgainAfterDelay("doReset", null, item)) return;
        }
        popup.dismiss();
        switch (item.getItemId()) {
            case R.id.step1:  nativeStep1(); break;
            case R.id.faster: nativeFaster(); break;
            case R.id.slower: nativeSlower(); break;
            case R.id.reset:  nativeResetPattern(); break;
            default:          Log.e("Golly","Fix bug in doControlItem!");
        }
        if (item.getItemId() == R.id.reset) {
            updateButtons();
        } else {
            updateGeneratingSpeed();
        }
    }

    // -----------------------------------------------------------------------------
    
    // called when the Fit button is tapped
    public void doFitPattern(View view) {
        nativeClearMessage();
        nativeFitPattern();
    }

    // -----------------------------------------------------------------------------
    
    // called when the Smaller button is tapped
    public void doSmaller(View view) {
        nativeClearMessage();
        nativeSmaller();
    }

    // -----------------------------------------------------------------------------
    
    // called when the Scale=1:1 button is tapped
    public void doScale1to1(View view) {
        nativeClearMessage();
        nativeScale1to1();
    }

    // -----------------------------------------------------------------------------
    
    // called when the Bigger button is tapped
    public void doBigger(View view) {
        nativeClearMessage();
        nativeBigger();
    }

    // -----------------------------------------------------------------------------
    
    // called when the Middle button is tapped
    public void doMiddle(View view) {
        nativeClearMessage();
        nativeMiddle();
    }

    // -----------------------------------------------------------------------------
    
    // called when the View button is tapped
    public void doView(View view) {
        nativeClearMessage();
        // display pop-up menu with these items: Scale=1:1, Bigger, Smaller, Middle
        popup = new PopupMenu(this, view);
        MenuInflater inflater = popup.getMenuInflater();
        inflater.inflate(R.menu.view_menu, popup.getMenu());
        popup.show();
    }

    // -----------------------------------------------------------------------------
    
    // called when item from view_menu is selected
    public void doViewItem(MenuItem item) {
        popup.dismiss();
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
        nativeClearMessage();
        stopIfGenerating();
        if (callAgainAfterDelay("doUndo", view, null)) return;
        nativeUndo();
        updateButtons();
        
    }

    // -----------------------------------------------------------------------------
    
    // called when the Redo button is tapped
    public void doRedo(View view) {
        nativeClearMessage();
        // nativeIsGenerating() should never be true here
        if (callAgainAfterDelay("doRedo", view, null)) return;
        nativeRedo();
        updateButtons();
    }

    // -----------------------------------------------------------------------------
    
    // called when the All button is tapped
    public void doSelectAll(View view) {
        nativeClearMessage();
        nativeSelectAll();
        UpdateEditBar();
    }

    // -----------------------------------------------------------------------------

    private void createSelectionMenu(MenuInflater inflater) {
        Menu menu = popup.getMenu();
        inflater.inflate(R.menu.selection_menu, menu);
        MenuItem item = menu.findItem(R.id.random);
        item.setTitle("Random Fill (" + nativeGetRandomFill() + "%)");
        if (widescreen) {
            // delete the top 2 items
            menu.removeItem(R.id.paste);
            menu.removeItem(R.id.all);
        }
    }

    // -----------------------------------------------------------------------------
    
    // called when the Edit button is tapped (widescreen is true)
    public void doEdit(View view) {
        nativeClearMessage();
        if (nativeSelectionExists()) {
            popup = new PopupMenu(this, view);
            MenuInflater inflater = popup.getMenuInflater();
            createSelectionMenu(inflater);
            popup.show();
        } else {
            // editbutton should be disabled if there's no selection
            Log.e("Golly","Bug detected in doEdit!");
        }
    }

    // -----------------------------------------------------------------------------

    private void createPasteMenu(MenuInflater inflater) {
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
            updateButtons();
        }
    }

    // -----------------------------------------------------------------------------
    
    // called when the Paste button is tapped (widescreen is true)
    public void doPaste(View view) {
        nativeClearMessage();
        if (nativePasteExists()) {
            // show pop-up menu with various paste options
            popup = new PopupMenu(this, view);
            MenuInflater inflater = popup.getMenuInflater();
            createPasteMenu(inflater);
            popup.show();
        } else {
            // create the paste image
            stopIfGenerating();
            if (callAgainAfterDelay("doPaste", view, null)) return;
            nativePaste();
            UpdateEditBar();
        }
    }

    // -----------------------------------------------------------------------------
    
    // called when the Edit/Paste button is tapped (widescreen is false)
    public void doEditPaste(View view) {
        nativeClearMessage();
        // display pop-up menu with items that depend on whether a selection or paste image exists
        popup = new PopupMenu(this, view);
        MenuInflater inflater = popup.getMenuInflater();
        if (nativePasteExists()) {
            createPasteMenu(inflater);
        } else if (nativeSelectionExists()) {
            createSelectionMenu(inflater);
        } else {
            inflater.inflate(R.menu.edit_menu, popup.getMenu());
        }
        popup.show();
    }

    // -----------------------------------------------------------------------------
    
    // called when item from edit_menu is selected
    public void doEditItem(MenuItem item) {
        if (item.getItemId() == R.id.paste) {
            stopIfGenerating();
            if (callAgainAfterDelay("doPaste", null, item)) return;
        }
        popup.dismiss();
        switch (item.getItemId()) {
            case R.id.paste: nativePaste(); break;
            case R.id.all:   nativeSelectAll(); break;
            default:         Log.e("Golly","Fix bug in doEditItem!");
        }
        UpdateEditBar();
    }

    // -----------------------------------------------------------------------------
    
    // called when item from selection_menu is selected
    public void doSelectionItem(MenuItem item) {
        if (item.getItemId() != R.id.remove &&
            item.getItemId() != R.id.copy &&
            item.getItemId() != R.id.shrink &&
            item.getItemId() != R.id.fitsel) {
            // item can modify the current pattern so we must stop generating,
            // but nicer if we only stop temporarily and resume when done
            if (nativeIsGenerating()) {
                // no need to set stopped = true
                nativePauseGenerating();
            }
            if (callAgainAfterDelay("doSelItem", null, item)) return;
        }
        popup.dismiss();
        switch (item.getItemId()) {
            // doEditItem handles the top 2 items (if widescreen is false)
            // case R.id.paste: nativePaste(); break;
            // case R.id.all:   nativeSelectAll(); break;
            case R.id.remove:   nativeRemoveSelection(); break;
            case R.id.cut:      nativeCutSelection(); break;
            case R.id.copy:     nativeCopySelection(); break;
            case R.id.clear:    nativeClearSelection(1); break;
            case R.id.clearo:   nativeClearSelection(0); break;
            case R.id.shrink:   nativeShrinkSelection(); break;
            case R.id.fitsel:   nativeFitSelection(); break;
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
        popup.dismiss();
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
        UpdateEditBar();
    }
    
    // -----------------------------------------------------------------------------
    
    // called when the Draw/Pick/Select/Move button is tapped
    public void doSetTouchMode(View view) {
        nativeClearMessage();
        // display pop-up menu with these items: Draw, Pick, Select, Move
        popup = new PopupMenu(this, view);
        MenuInflater inflater = popup.getMenuInflater();
        inflater.inflate(R.menu.mode_menu, popup.getMenu());
        popup.show();
    }

    // -----------------------------------------------------------------------------
    
    // called when item from mode_menu is selected
    public void doModeItem(MenuItem item) {
        popup.dismiss();
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
    
    // called when the Draw button is tapped
    public void doDrawMode(View view) {
        nativeClearMessage();
        if (nativeGetMode() != 0) {
            nativeSetMode(0);
            UpdateEditBar();
        }
    }

    // -----------------------------------------------------------------------------
    
    // called when the Pick button is tapped
    public void doPickMode(View view) {
        nativeClearMessage();
        if (nativeGetMode() != 1) {
            nativeSetMode(1);
            UpdateEditBar();
        }
    }

    // -----------------------------------------------------------------------------
    
    // called when the Select button is tapped
    public void doSelectMode(View view) {
        nativeClearMessage();
        if (nativeGetMode() != 2) {
            nativeSetMode(2);
            UpdateEditBar();
        }
    }

    // -----------------------------------------------------------------------------
    
    // called when the Move button is tapped
    public void doMoveMode(View view) {
        nativeClearMessage();
        if (nativeGetMode() != 3) {
            nativeSetMode(3);
            UpdateEditBar();
        }
    }

    // -----------------------------------------------------------------------------
    
    // called when the state button is tapped
    public void doChangeState(View view) {
        nativeClearMessage();
        // let user change the current drawing state
        Intent intent = new Intent(this, StateActivity.class);
        startActivity(intent);
    }

    // -----------------------------------------------------------------------------
    
    // called when the New button is tapped
    public void doNewPattern(View view) {
        nativeClearMessage();
        // stopIfGenerating() would work here but we use smarter code that
        // avoids trying to save the current pattern (potentially very large)
        if (nativeIsGenerating()) {
            stopped = true;
            nativeStopBeforeNew();
        }
        if (callAgainAfterDelay("doNew", view, null)) return;
        
        nativeNewPattern();
        updateButtons();
        UpdateEditBar();
        
        if (nativeNumLayers() == 1) {
            // delete all files in tempdir
            deleteTempFiles();
        }
    }

    // -----------------------------------------------------------------------------
    
    // called when the Rule button is tapped
    public void doRule(View view) {
        nativeClearMessage();
        stopIfGenerating();
        if (callAgainAfterDelay("doRule", view, null)) return;
        
        // let user change the current rule and/or algorithm
        Intent intent = new Intent(this, RuleActivity.class);
        startActivity(intent);
    }

    // -----------------------------------------------------------------------------
        
    // called when the Info button is tapped
    public void doInfo(View view) {
        nativeClearMessage();
        stopIfGenerating();
        if (callAgainAfterDelay("doInfo", view, null)) return;
        
        // display any comments in current pattern file
        Intent intent = new Intent(this, InfoActivity.class);
        intent.putExtra(InfoActivity.INFO_MESSAGE, "native");
        startActivity(intent);
    }

    // -----------------------------------------------------------------------------
    
    // called when the Save button is tapped
    public void doSave(View view) {
        nativeClearMessage();
        stopIfGenerating();
        if (callAgainAfterDelay("doSave", view, null)) return;
        
        AlertDialog.Builder alert = new AlertDialog.Builder(this);
        alert.setTitle("Save current pattern");
        alert.setMessage("Valid file name extensions are\n" + nativeGetValidExtensions());
        
        // or use radio buttons as in iPad Golly???
        // might be better to create a new SaveActivity and make it appear in a dialog
        // by setting its theme to Theme.Holo.Dialog in the <activity> manifest element

        // use an EditText view to get filename
        final EditText input = new EditText(this);
        input.setSingleLine(true);
        input.setHint("enter a file name");
        input.setInputType(InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
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
                    BaseApp baseapp = (BaseApp)getApplicationContext();
                    // check for valid extension
                    if (!nativeValidExtension(filename)) {
                        baseapp.Warning("Invalid file extension.");
                        return;
                    }
                    // check if given file already exists
                    if (nativeFileExists(filename)) {
                        String answer = baseapp.YesNo("A file with that name already exists.  Do you want to replace that file?");
                        if (answer.equals("no")) return;
                    }
                     // dismiss dialog first in case SavePattern calls BeginProgress
                    dialog.dismiss();
                    nativeSavePattern(filename);
                }
            });
    }

    // -----------------------------------------------------------------------------
    
    // called when the Full/Restore button is tapped
    public void toggleFullScreen(View view) {
        // no need to call nativeClearMessage()
        LinearLayout topbar = (LinearLayout) findViewById(R.id.top_bar);
        LinearLayout editbar = (LinearLayout) findViewById(R.id.edit_bar);
        RelativeLayout bottombar = (RelativeLayout) findViewById(R.id.bottom_bar);
        
        if (fullscreen) {
            fullscreen = false;
            restorebutton.setVisibility(View.INVISIBLE);
            
            status1.setVisibility(View.VISIBLE);
            status2.setVisibility(View.VISIBLE);
            status3.setVisibility(View.VISIBLE);

            topbar.setVisibility(LinearLayout.VISIBLE);
            editbar.setVisibility(LinearLayout.VISIBLE);
            bottombar.setVisibility(RelativeLayout.VISIBLE);
            
            getActionBar().show();
        } else {
            fullscreen = true;
            restorebutton.setVisibility(View.VISIBLE);
            
            status1.setVisibility(View.GONE);
            status2.setVisibility(View.GONE);
            status3.setVisibility(View.GONE);
            
            topbar.setVisibility(LinearLayout.GONE);
            editbar.setVisibility(LinearLayout.GONE);
            bottombar.setVisibility(RelativeLayout.GONE);
            
            getActionBar().hide();
        }
        
        // need to let native code know (this will update status bar if not fullscreen)
        nativeSetFullScreen(fullscreen);
        
        if (!fullscreen) {
            updateButtons();
            UpdateEditBar();
        }
    }

    // -----------------------------------------------------------------------------

    // this method is called from C++ code (see jnicalls.cpp)
    private void StartMainActivity() {
        Intent intent = new Intent(this, MainActivity.class);
        startActivity(intent);
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
        // no need to check fullscreen flag here (caller checks it)
        
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
        if (fullscreen) return;
        
        // this might be called from a non-UI thread
        runOnUiThread(new Runnable() {
            public void run() {
                undobutton.setEnabled(nativeCanUndo());
                redobutton.setEnabled(nativeCanRedo());
                
                // update Edit button
                if (widescreen) {
                    editbutton.setEnabled(nativeSelectionExists());
                } else {
                    if (nativePasteExists()) {
                        editbutton.setText("Paste");
                    } else {
                        editbutton.setText("Edit");
                    }
                }
                
                // show current drawing state
                statebutton.setText("State=" + Integer.toString(nativeDrawingState()));
                
                // show current touch mode
                if (widescreen) {
                    drawbutton.setTypeface(Typeface.DEFAULT);
                    pickbutton.setTypeface(Typeface.DEFAULT);
                    selectbutton.setTypeface(Typeface.DEFAULT);
                    movebutton.setTypeface(Typeface.DEFAULT);
                    drawbutton.setTextColor(Color.rgb(0,0,0));
                    pickbutton.setTextColor(Color.rgb(0,0,0));
                    selectbutton.setTextColor(Color.rgb(0,0,0));
                    movebutton.setTextColor(Color.rgb(0,0,0));
                    switch (nativeGetMode()) {
                        case 0:  drawbutton.setTypeface(Typeface.DEFAULT_BOLD);
                                 drawbutton.setTextColor(Color.rgb(0,0,255));
                                 break;
                        case 1:  pickbutton.setTypeface(Typeface.DEFAULT_BOLD);
                                 pickbutton.setTextColor(Color.rgb(0,0,255));
                                 break;
                        case 2:  selectbutton.setTypeface(Typeface.DEFAULT_BOLD);
                                 selectbutton.setTextColor(Color.rgb(0,0,255));
                                 break;
                        case 3:  movebutton.setTypeface(Typeface.DEFAULT_BOLD);
                                 movebutton.setTextColor(Color.rgb(0,0,255));
                                 break;
                        default: Log.e("Golly","Fix bug in UpdateEditBar!");
                    }
                } else {
                    switch (nativeGetMode()) {
                        case 0:  modebutton.setText("Draw"); break;
                        case 1:  modebutton.setText("Pick"); break;
                        case 2:  modebutton.setText("Select"); break;
                        case 3:  modebutton.setText("Move"); break;
                        default: Log.e("Golly","Fix bug in UpdateEditBar!");
                    }
                }
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
        BaseApp baseapp = (BaseApp)getApplicationContext();
        ClipboardManager clipboard = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
        String text = "";
        if (clipboard.hasPrimaryClip()) {
            ClipData clipData = clipboard.getPrimaryClip();
            text = clipData.getItemAt(0).coerceToText(this).toString();
            if (text.length() == 0) {
            	baseapp.Warning("Failed to get text from clipboard!");
            }
        }
        // Log.i("GetTextFromClipboard", text);
        return text;
    }
    
    // -----------------------------------------------------------------------------

    // this method is called from C++ code (see jnicalls.cpp)
    private void ShowTextFile(String filepath) {
        BaseApp baseapp = (BaseApp)getApplicationContext();
        // display file contents
        Intent intent = new Intent(baseapp.getCurrentActivity(), InfoActivity.class);
        intent.putExtra(InfoActivity.INFO_MESSAGE, filepath);
        startActivity(intent);
    }
    
    // -----------------------------------------------------------------------------

    // this method is called from C++ code (see jnicalls.cpp)
    private void ShowHelp(String filepath) {
        Intent intent = new Intent(this, HelpActivity.class);
        intent.putExtra(HelpActivity.SHOWHELP_MESSAGE, filepath);
        startActivity(intent);
    }
    
    // -----------------------------------------------------------------------------

    // this method is called from C++ code (see jnicalls.cpp)
    private void BeginProgress(String title) {
        if (progresscount == 0) {
            // can we disable interaction with all views outside proglayout???
            progtitle.setText(title);
            progbar.setProgress(0);
            // proglayout.setVisibility(LinearLayout.VISIBLE);
            cancelProgress = false;
            progstart = System.nanoTime();
        }
        progresscount++;    // handles nested calls
    }
    
    // -----------------------------------------------------------------------------

    // this method is called from C++ code (see jnicalls.cpp)
    private boolean AbortProgress(int percentage, String message) {
        if (progresscount <= 0) {
            Log.e("Golly","Bug detected in AbortProgress!");
            return true;
        }
        long nanosecs = System.nanoTime() - progstart;
        if (proglayout.getVisibility() == LinearLayout.VISIBLE) {
            if (nanosecs < prognext) return false;
            prognext = nanosecs + 100000000L;     // update progress bar about 10 times per sec
            updateProgressBar(percentage);
            return cancelProgress;
        } else {
            // note that percentage is not always an accurate estimator for how long
            // the task will take, especially when we use nextcell for cut/copy
            if ( (nanosecs > 1000000000L && percentage < 30) || nanosecs > 2000000000L ) {
                // task is probably going to take a while so show progress bar
                proglayout.setVisibility(LinearLayout.VISIBLE);
                updateProgressBar(percentage);
            }
            prognext = nanosecs + 10000000L;     // 0.01 sec delay until 1st progress update
        }
        return false;
    }
    
    // -----------------------------------------------------------------------------

    private void updateProgressBar(int percentage) {
        if (percentage < 0 || percentage > 100) return;
        progbar.setProgress(percentage);
        CheckMessageQueue();
    }
    
    // -----------------------------------------------------------------------------

    // this method is called from C++ code (see jnicalls.cpp)
    private void EndProgress() {
        if (progresscount <= 0) {
            Log.e("Golly","Bug detected in EndProgress!");
            return;
        }
        progresscount--;
        if (progresscount == 0) {
            proglayout.setVisibility(LinearLayout.INVISIBLE);
        }
    }

    // -----------------------------------------------------------------------------

    // called when the Cancel button is tapped
    public void doCancel(View view) {
        cancelProgress = true;
    }

} // MainActivity class
