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
import java.util.Arrays;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.support.v4.app.NavUtils;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.webkit.WebView;
import android.webkit.WebViewClient;

public class OpenActivity extends Activity {

    // see jnicalls.cpp for these native routines:
    private native String nativeGetRecentPatterns();
    private native String nativeGetSavedPatterns(String paths);
    private native String nativeGetDownloadedPatterns(String paths);
    private native String nativeGetSuppliedPatterns(String paths);
    
    private enum PATTERNS {
        SUPPLIED, RECENT, SAVED, DOWNLOADED;
    }
    
    private static PATTERNS currpatterns = PATTERNS.SUPPLIED;
    
    // -----------------------------------------------------------------------------
    
    public final static String OPENFILE_MESSAGE = "net.sf.golly.OPENFILE";
    
    // this class lets us intercept link taps
    private class MyWebViewClient extends WebViewClient {
        @Override
        public boolean shouldOverrideUrlLoading(WebView view, String url) {
            if (url.startsWith("open:")) {
                openFile(url.substring(5));
                return true;
            }
            if (url.startsWith("toggledir:")) {
                //!!! nativeToggleDir(url.substring(10));
                return true;
            }
            if (url.startsWith("delete:")) {
                //!!! nativeDeleteFile(url.substring(7));
                return true;
            }
            if (url.startsWith("edit:")) {
                //!!! nativeEditFile(url.substring(5));
                return true;
            }
            return false;
        }
    }
    
    private void openFile(String filepath) {
        // switch to main screen and open specified file
        Intent intent = new Intent(this, MainActivity.class);
        intent.putExtra(OPENFILE_MESSAGE, filepath);
        startActivity(intent);
    }
    
    // -----------------------------------------------------------------------------

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.open_layout);
        
        // show the Up button in the action bar
        getActionBar().setDisplayHomeAsUpEnabled(true);
        
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
                // the Home or Up button will start MainActivity
                NavUtils.navigateUpFromSameTask(this);
                return true;
            case R.id.open:
                // do nothing
                break;
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
    
    // called when the Supplied button is tapped
    public void doSupplied(View view) {
        if (currpatterns != PATTERNS.SUPPLIED) {
            currpatterns = PATTERNS.SUPPLIED;
            showSuppliedPatterns();
        }
    }

    // -----------------------------------------------------------------------------
    
    // called when the Recent button is tapped
    public void doRecent(View view) {
        if (currpatterns != PATTERNS.RECENT) {
            currpatterns = PATTERNS.RECENT;
            showRecentPatterns();
        }
    }

    // -----------------------------------------------------------------------------
    
    // called when the Saved button is tapped
    public void doSaved(View view) {
        if (currpatterns != PATTERNS.SAVED) {
            currpatterns = PATTERNS.SAVED;
            showSavedPatterns();
        }
    }

    // -----------------------------------------------------------------------------
    
    // called when the Downloaded button is tapped
    public void doDownloaded(View view) {
        if (currpatterns != PATTERNS.DOWNLOADED) {
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
        Arrays.sort(files);             // sort into alphabetical order
        if (files != null) {
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
        String paths = enumerateDirectory(getDir("Patterns",MODE_PRIVATE), "");    // replace getDir???!!!
        String htmldata = nativeGetSuppliedPatterns(paths);

        WebView webview = (WebView) findViewById(R.id.webview);
        //!!!??? webview.getSettings().setJavaScriptEnabled(true);
        webview.setWebViewClient(new MyWebViewClient());
        webview.loadDataWithBaseURL(null, htmldata, "text/html", "UTF-8", null);
    }

    // -----------------------------------------------------------------------------
    
    private void showRecentPatterns() {
        String htmldata = nativeGetRecentPatterns();

        WebView webview = (WebView) findViewById(R.id.webview);
        //!!!??? webview.getSettings().setJavaScriptEnabled(true);
        webview.setWebViewClient(new MyWebViewClient());
        webview.loadDataWithBaseURL(null, htmldata, "text/html", "UTF-8", null);
    }

    // -----------------------------------------------------------------------------
    
    private void showSavedPatterns() {
        String paths = enumerateDirectory(new File(getFilesDir(), "Saved"), "");
        String htmldata = nativeGetSavedPatterns(paths);

        WebView webview = (WebView) findViewById(R.id.webview);
        //!!!??? webview.getSettings().setJavaScriptEnabled(true);
        webview.setWebViewClient(new MyWebViewClient());
        webview.loadDataWithBaseURL(null, htmldata, "text/html", "UTF-8", null);
    }

    // -----------------------------------------------------------------------------
    
    private void showDownloadedPatterns() {
        String paths = enumerateDirectory(new File(getFilesDir(), "Downloads"), "");
        String htmldata = nativeGetDownloadedPatterns(paths);

        WebView webview = (WebView) findViewById(R.id.webview);
        //!!!??? webview.getSettings().setJavaScriptEnabled(true);
        webview.setWebViewClient(new MyWebViewClient());
        webview.loadDataWithBaseURL(null, htmldata, "text/html", "UTF-8", null);
    }

} // OpenActivity class
