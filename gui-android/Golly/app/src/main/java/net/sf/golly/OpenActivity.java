// This file is part of Golly.
// See docs/License.html for the copyright notice.

package net.sf.golly;

import java.io.File;
import java.util.Arrays;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.support.v4.app.NavUtils;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.webkit.WebSettings.LayoutAlgorithm;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Toast;

public class OpenActivity extends Activity {

    // see jnicalls.cpp for these native routines:
    private native String nativeGetRecentPatterns();
    private native String nativeGetSavedPatterns(String paths);
    private native String nativeGetDownloadedPatterns(String paths);
    private native String nativeGetSuppliedPatterns(String paths);
    private native void nativeToggleDir(String path);
    
    private enum PATTERNS {
        SUPPLIED, RECENT, SAVED, DOWNLOADED;
    }
    
    private static PATTERNS currpatterns = PATTERNS.SUPPLIED;
    
    // remember scroll positions for each type of patterns
    private static int supplied_pos = 0;
    private static int recent_pos = 0;
    private static int saved_pos = 0;
    private static int downloaded_pos = 0;
    
    private WebView gwebview;   // for displaying html data
    
    // -----------------------------------------------------------------------------
    
    // this class lets us intercept link taps and restore the scroll position
    private class MyWebViewClient extends WebViewClient {
        @Override
        public boolean shouldOverrideUrlLoading(WebView webview, String url) {
            if (url.startsWith("open:")) {
                openFile(url.substring(5));
                return true;
            }
            if (url.startsWith("toggledir:")) {
                nativeToggleDir(url.substring(10));
                saveScrollPosition();
                showSuppliedPatterns();
                return true;
            }
            if (url.startsWith("delete:")) {
                removeFile(url.substring(7));
                return true;
            }
            if (url.startsWith("edit:")) {
                editFile(url.substring(5));
                return true;
            }
            return false;
        }
        
        @Override  
        public void onPageFinished(WebView webview, String url) {
            super.onPageFinished(webview, url);
            // webview.scrollTo doesn't always work here;
            // we need to delay until webview.getContentHeight() > 0
            final int scrollpos = restoreScrollPosition();
            if (scrollpos > 0) {
                final Handler handler = new Handler();
                Runnable runnable = new Runnable() {
                    public void run() {
                        if (gwebview.getContentHeight() > 0) {
                            gwebview.scrollTo(0, scrollpos);
                        } else {
                            // try again a bit later
                            handler.postDelayed(this, 100);
                        }
                    }
                };
                handler.postDelayed(runnable, 100);
            }
            
            /* following also works if we setJavaScriptEnabled(true), but is not quite as nice
               when a folder is closed because the scroll position can change to force the
               last line to appear at the bottom of the webview
            int scrollpos = restoreScrollPosition();
            if (scrollpos > 0) {
                final StringBuilder sb = new StringBuilder("javascript:window.scrollTo(0, ");
                sb.append(scrollpos);
                sb.append("/ window.devicePixelRatio);");
                webview.loadUrl(sb.toString());
            }
            super.onPageFinished(webview, url);
            */
        }  
    }

    // -----------------------------------------------------------------------------
    
    private void saveScrollPosition() {
        switch (currpatterns) {
            case SUPPLIED:   supplied_pos = gwebview.getScrollY(); break;
            case RECENT:     recent_pos = gwebview.getScrollY(); break;
            case SAVED:      saved_pos = gwebview.getScrollY(); break;
            case DOWNLOADED: downloaded_pos = gwebview.getScrollY(); break;
        }
    }
    
    // -----------------------------------------------------------------------------
    
    private int restoreScrollPosition() {
        switch (currpatterns) {
            case SUPPLIED:   return supplied_pos;
            case RECENT:     return recent_pos;
            case SAVED:      return saved_pos;
            case DOWNLOADED: return downloaded_pos;
        }
        return 0;   // should never get here
    }

    // -----------------------------------------------------------------------------
    
    private void openFile(String filepath) {
        // switch to main screen and open given file
        Intent intent = new Intent(this, MainActivity.class);
        intent.putExtra(MainActivity.OPENFILE_MESSAGE, filepath);
        startActivity(intent);
    }
    
    // -----------------------------------------------------------------------------
    
    private void removeFile(String filepath) {
        BaseApp baseapp = (BaseApp)getApplicationContext();
        final String fullpath = baseapp.userdir.getAbsolutePath() + "/" + filepath;
        final File file = new File(fullpath);
        
        // ask user if it's okay to delete given file
        AlertDialog.Builder alert = new AlertDialog.Builder(this);
        alert.setTitle("Delete file?");
        alert.setMessage("Do you really want to delete " + file.getName() + "?");
        alert.setPositiveButton("DELETE",
            new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int id) {
                    if (file.delete()) {
                        // file has been deleted so refresh gwebview
                        saveScrollPosition();
                        switch (currpatterns) {
                            case SUPPLIED:   break; // should never happen
                            case RECENT:     break; // should never happen
                            case SAVED:      showSavedPatterns(); break;
                            case DOWNLOADED: showDownloadedPatterns(); break;
                        }
                    } else {
                        // should never happen
                        Log.e("removeFile", "Failed to delete file: " + fullpath);
                    }
                }
            });
        alert.setNegativeButton("CANCEL", null);
        alert.show();
    }
    
    // -----------------------------------------------------------------------------
    
    private void editFile(String filepath) {
        // check if filepath is compressed
        if (filepath.endsWith(".gz") || filepath.endsWith(".zip")) {
            if (currpatterns == PATTERNS.SUPPLIED) {
                Toast.makeText(this, "Reading a compressed file is not supported.", Toast.LENGTH_SHORT).show();
            } else {
                Toast.makeText(this, "Editing a compressed file is not supported.", Toast.LENGTH_SHORT).show();
            }
            return;
        }

        BaseApp baseapp = (BaseApp)getApplicationContext();
        // let user read/edit given file
        if (currpatterns == PATTERNS.SUPPLIED) {
            // read contents of supplied file into a string
            String fullpath = baseapp.supplieddir.getAbsolutePath() + "/" + filepath;
            // display filecontents
            Intent intent = new Intent(this, InfoActivity.class);
            intent.putExtra(InfoActivity.INFO_MESSAGE, fullpath);
            startActivity(intent);
        } else {
            // let user read or edit a saved or downloaded file
            String fullpath = baseapp.userdir.getAbsolutePath() + "/" + filepath;
            Intent intent = new Intent(this, EditActivity.class);
            intent.putExtra(EditActivity.EDITFILE_MESSAGE, fullpath);
            startActivity(intent);
        }
    }
   
    // -----------------------------------------------------------------------------

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.open_layout);
        
        gwebview = (WebView) findViewById(R.id.webview);
        gwebview.setWebViewClient(new MyWebViewClient());
        
        // avoid wrapping long lines -- this doesn't work:
        // gwebview.getSettings().setUseWideViewPort(true);
        // this is better:
        gwebview.getSettings().setLayoutAlgorithm(LayoutAlgorithm.SINGLE_COLUMN);
        
        DisplayMetrics metrics = getResources().getDisplayMetrics();
        if (metrics.densityDpi > 300) {
            // use bigger font size for high density screens (default size is 16)
            gwebview.getSettings().setDefaultFontSize(32);
        }
        
        // no need for JavaScript???
        // gwebview.getSettings().setJavaScriptEnabled(true);
        
        // allow zooming???
        // gwebview.getSettings().setBuiltInZoomControls(true);

        // show the Up button in the action bar
        getActionBar().setDisplayHomeAsUpEnabled(true);
        
        // note that onResume will do the initial display
    }

    // -----------------------------------------------------------------------------

    @Override
    protected void onPause() {
        super.onPause();
        saveScrollPosition();
    }

    // -----------------------------------------------------------------------------

    @Override
    protected void onResume() {
        super.onResume();
        // update gwebview
        restoreScrollPosition();
        switch (currpatterns) {
            case SUPPLIED:   showSuppliedPatterns(); break;
            case RECENT:     showRecentPatterns(); break;
            case SAVED:      showSavedPatterns(); break;
            case DOWNLOADED: showDownloadedPatterns(); break;
        }
    }

    // -----------------------------------------------------------------------------

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // add main.xml items to the action bar
        getMenuInflater().inflate(R.menu.main, menu);
        
        // disable the item for this activity
        MenuItem item = menu.findItem(R.id.open);
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
                // do nothing
                break;
            case R.id.settings:
            	finish();
                intent = new Intent(this, SettingsActivity.class);
                startActivity(intent);
                return true;
            case R.id.help:
            	finish();
                intent = new Intent(this, HelpActivity.class);
                startActivity(intent);
                return true;
        }
        return super.onOptionsItemSelected(item);
    }
    
    // -----------------------------------------------------------------------------
    
    // called when the Supplied button is tapped
    public void doSupplied(View view) {
        if (currpatterns != PATTERNS.SUPPLIED) {
            saveScrollPosition();
            currpatterns = PATTERNS.SUPPLIED;
            showSuppliedPatterns();
        }
    }

    // -----------------------------------------------------------------------------
    
    // called when the Recent button is tapped
    public void doRecent(View view) {
        if (currpatterns != PATTERNS.RECENT) {
            saveScrollPosition();
            currpatterns = PATTERNS.RECENT;
            showRecentPatterns();
        }
    }

    // -----------------------------------------------------------------------------
    
    // called when the Saved button is tapped
    public void doSaved(View view) {
        if (currpatterns != PATTERNS.SAVED) {
            saveScrollPosition();
            currpatterns = PATTERNS.SAVED;
            showSavedPatterns();
        }
    }

    // -----------------------------------------------------------------------------
    
    // called when the Downloaded button is tapped
    public void doDownloaded(View view) {
        if (currpatterns != PATTERNS.DOWNLOADED) {
            saveScrollPosition();
            currpatterns = PATTERNS.DOWNLOADED;
            showDownloadedPatterns();
        }
    }

    // -----------------------------------------------------------------------------
    
    private String enumerateDirectory(File dir, String prefix) {
        // return the files and/or sub-directories in the given directory
        // as a string of paths where:
        // - paths are relative to to the initial directory
        // - directory paths end with '/'
        // - each path is terminated by \n
        String result = "";
        File[] files = dir.listFiles();
        if (files != null) {
            Arrays.sort(files);         // sort into alphabetical order
            for (File file : files) {
                if (file.isDirectory()) {
                    String dirname = prefix + file.getName() + "/";
                    result += dirname + "\n";
                    result += enumerateDirectory(file, dirname);
                } else {
                    result += prefix + file.getName() + "\n";
                }
            }
        }
        return result;
    }

    // -----------------------------------------------------------------------------
    
    private void showSuppliedPatterns() {
        BaseApp baseapp = (BaseApp)getApplicationContext();
        String paths = enumerateDirectory(new File(baseapp.supplieddir, "Patterns"), "");
        String htmldata = nativeGetSuppliedPatterns(paths);
        // use a special base URL so that <img src="foo.png"/> will extract foo.png from the assets folder
        gwebview.loadDataWithBaseURL("file:///android_asset/", htmldata, "text/html", "utf-8", null);
    }

    // -----------------------------------------------------------------------------
    
    private void showRecentPatterns() {
        String htmldata = nativeGetRecentPatterns();
        gwebview.loadDataWithBaseURL(null, htmldata, "text/html", "utf-8", null);
    }

    // -----------------------------------------------------------------------------
    
    private void showSavedPatterns() {
        BaseApp baseapp = (BaseApp)getApplicationContext();
        String paths = enumerateDirectory(new File(baseapp.userdir, "Saved"), "");
        String htmldata = nativeGetSavedPatterns(paths);
        gwebview.loadDataWithBaseURL(null, htmldata, "text/html", "utf-8", null);
    }

    // -----------------------------------------------------------------------------
    
    private void showDownloadedPatterns() {
        BaseApp baseapp = (BaseApp)getApplicationContext();
        String paths = enumerateDirectory(new File(baseapp.userdir, "Downloads"), "");
        String htmldata = nativeGetDownloadedPatterns(paths);
        gwebview.loadDataWithBaseURL(null, htmldata, "text/html", "utf-8", null);
    }

} // OpenActivity class
