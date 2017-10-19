// This file is part of Golly.
// See docs/License.html for the copyright notice.

package net.sf.golly;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.util.ArrayDeque;
import java.util.Deque;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Application;
import android.content.DialogInterface;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.DisplayMetrics;
import android.util.Log;

public class BaseApp extends Application {
	
    // see jnicalls.cpp for these native routines:
	private static native void nativeClassInit();          // this MUST be static
	private native void nativeCreate();                    // the rest must NOT be static
    private native void nativeSetUserDirs(String path);
    private native void nativeSetSuppliedDirs(String prefix);
    private native void nativeSetTempDir(String path);
    private native void nativeSetScreenDensity(int dpi);
    private native void nativeSetWideScreen(boolean widescreen);
	
    public File userdir;        // directory for user-created data
    public File supplieddir;    // directory for supplied data

    // keep track of the current foreground activity
    private Deque<Activity> activityStack = new ArrayDeque<Activity>();

    // -----------------------------------------------------------------------------
    
    static {
    	System.loadLibrary("stlport_shared");   // must agree with build.gradle
        System.loadLibrary("golly");            // loads libgolly.so
        nativeClassInit();                      // caches Java method IDs
    }
    
    // -----------------------------------------------------------------------------
    
    @Override
    public void onCreate() {
        super.onCreate();
        
        DisplayMetrics metrics = getResources().getDisplayMetrics();
        nativeSetScreenDensity(metrics.densityDpi);
        // Log.i("Golly","screen density in dpi = " + Integer.toString(metrics.densityDpi));
        // eg. densityDpi = 320 on Nexus 7
        
        DisplayMetrics displayMetrics = getResources().getDisplayMetrics();
        float dpWidth = displayMetrics.widthPixels / displayMetrics.density;
        
        // Log.i("Golly","screen width in dp = " + Integer.toString(config.screenWidthDp));
        // eg. on Nexus 7 screenWidthDp = 600 in portrait, 960 in landscape
        boolean widescreen = dpWidth >= 600;
        nativeSetWideScreen(widescreen);
        
        initPaths();        // sets userdir, supplieddir, etc (must be BEFORE nativeCreate)
        nativeCreate();     // cache this instance and initialize lots of Golly stuff

        this.registerActivityLifecycleCallbacks(new MyActivityLifecycleCallbacks());
    }
    
    // -----------------------------------------------------------------------------

    private class MyActivityLifecycleCallbacks implements ActivityLifecycleCallbacks {

        @Override
        public void onActivityCreated(Activity activity, Bundle savedInstanceState) {}

        @Override
        public void onActivityDestroyed(Activity activity) {}

        @Override
        public void onActivityPaused(Activity activity) {
            // MainActivity is special (singleTask) and can be called into with NewIntent
            if (activity instanceof MainActivity) {
                return;
            }
            activityStack.removeFirst();
        }

        @Override
        public void onActivityResumed(Activity activity) {
            activityStack.addFirst(activity);
        }

        @Override
        public void onActivitySaveInstanceState(Activity activity, Bundle outState) {}

        @Override
        public void onActivityStarted(Activity activity) {}

        @Override
        public void onActivityStopped(Activity activity) {}
    }

    public Activity getCurrentActivity() {
        return activityStack.peekFirst();
    }
    
    // -----------------------------------------------------------------------------

    private void initPaths() {
        // check if external storage is available
        String state = Environment.getExternalStorageState();
        if (Environment.MEDIA_MOUNTED.equals(state)) {
            // use external storage for user's files
            userdir = getExternalFilesDir(null);        // /mnt/sdcard/Android/data/net.sf.golly/files
        } else {
            // use internal storage for user's files
            userdir = getFilesDir();                    // /data/data/net.sf.golly/files
            Log.i("Golly", "External storage is not available, so internal storage will be used.");
        }
        
        // create subdirs in userdir (if they don't exist)
        File subdir;
        subdir = new File(userdir, "Rules");            // for user's .rule files
        subdir.mkdir();
        subdir = new File(userdir, "Saved");            // for saved pattern files
        subdir.mkdir();
        subdir = new File(userdir, "Downloads");        // for downloaded files
        subdir.mkdir();
        
        // set appropriate paths used by C++ code
        nativeSetUserDirs(userdir.getAbsolutePath());
        
        // create a directory in internal storage for supplied Patterns/Rules/Help then
        // create sub-directories for each by unzipping .zip files stored in assets
        supplieddir = new File(getFilesDir(), "Supplied");
        supplieddir.mkdir();
        unzipAsset("Patterns.zip", supplieddir);
        unzipAsset("Rules.zip", supplieddir);
        unzipAsset("Help.zip", supplieddir);
        
        // supplieddir = /data/data/net.sf.golly/files/Supplied
        nativeSetSuppliedDirs(supplieddir.getAbsolutePath());
        
        // set directory path for temporary files
        File tempdir = getCacheDir();
        nativeSetTempDir(tempdir.getAbsolutePath());    // /data/data/net.sf.golly/cache
    }
    
    // -----------------------------------------------------------------------------

    private void unzipAsset(String zipname, File destdir) {
        AssetManager am = getAssets();
        try {
            ZipInputStream zipstream = new ZipInputStream(am.open(zipname));
            for (ZipEntry entry = zipstream.getNextEntry(); entry != null; entry = zipstream.getNextEntry()) {
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
            Log.e("Golly", "Failed to unzip asset: " + zipname + "\nException: ", e);
            Warning("You probably forgot to put " + zipname + " in the assets folder!");
        }
    }
    
    // -----------------------------------------------------------------------------

    // handler for implementing modal dialogs
    static class LooperInterrupter extends Handler {
		public void handleMessage(Message msg) {
            throw new RuntimeException();
        }
	}
    
    // -----------------------------------------------------------------------------
    
    // this method is called from C++ code (see jnicalls.cpp)
    void Warning(String msg) {
        Activity currentActivity = getCurrentActivity();
        if (currentActivity == null) {
            Log.i("Warning", "currentActivity is null!");
        }

        // note that MainActivity might not be the current foreground activity
        AlertDialog.Builder alert = new AlertDialog.Builder(currentActivity);
        alert.setTitle("Warning");
        alert.setMessage(msg);
        alert.setNegativeButton("CANCEL",
            new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int id) {
                    dialog.cancel();
                }
            });
        alert.show();
    }
    
    // -----------------------------------------------------------------------------

    // this method is called from C++ code (see jnicalls.cpp)
    void Fatal(String msg) {
        Activity currentActivity = getCurrentActivity();
        // note that MainActivity might not be the current foreground activity
        AlertDialog.Builder alert = new AlertDialog.Builder(currentActivity);
        alert.setTitle("Fatal error!");
        alert.setMessage(msg);
        alert.setNegativeButton("QUIT",
            new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int id) {
                    dialog.cancel();
                }
            });
        alert.show();

        System.exit(1);
    }
    
    // -----------------------------------------------------------------------------

    private String answer;
    
    // this method is called from C++ code (see jnicalls.cpp)
    String YesNo(String query) {
        // use a handler to get a modal dialog
        // (this must be modal because it is used in an if test)
        final Handler handler = new LooperInterrupter();

        Activity currentActivity = getCurrentActivity();
        // note that MainActivity might not be the current foreground activity
        AlertDialog.Builder alert = new AlertDialog.Builder(currentActivity);
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
    
} // BaseApp class
