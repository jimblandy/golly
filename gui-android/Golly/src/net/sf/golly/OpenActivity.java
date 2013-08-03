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
import android.webkit.WebView;

public class OpenActivity extends Activity {

    private enum PATTERNS {
        SUPPLIED, RECENT, SAVED, DOWNLOADED;
    }
    
    private static PATTERNS currpatterns = PATTERNS.SUPPLIED;

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
    
    private void showSuppliedPatterns() {
        String html = "<html><body>Supplied patterns:</body></html>";
        //!!!

        WebView webview = (WebView) findViewById(R.id.webview);
        //!!!??? webview.getSettings().setJavaScriptEnabled(true);
        webview.loadDataWithBaseURL(null, html, "text/html", "UTF-8", null);
    }

    // -----------------------------------------------------------------------------
    
    private void showRecentPatterns() {
        String html = "<html><body>Recent patterns:</body></html>";
        //!!!

        WebView webview = (WebView) findViewById(R.id.webview);
        //!!!??? webview.getSettings().setJavaScriptEnabled(true);
        webview.loadDataWithBaseURL(null, html, "text/html", "UTF-8", null);
    }

    // -----------------------------------------------------------------------------
    
    private void showSavedPatterns() {
        String html = "<html><body>Saved patterns:</body></html>";
        //!!!

        WebView webview = (WebView) findViewById(R.id.webview);
        //!!!??? webview.getSettings().setJavaScriptEnabled(true);
        webview.loadDataWithBaseURL(null, html, "text/html", "UTF-8", null);
    }

    // -----------------------------------------------------------------------------
    
    private void showDownloadedPatterns() {
        String html = "<html><body>Downloaded patterns:</body></html>";
        //!!!

        WebView webview = (WebView) findViewById(R.id.webview);
        //!!!??? webview.getSettings().setJavaScriptEnabled(true);
        webview.loadDataWithBaseURL(null, html, "text/html", "UTF-8", null);
    }

} // OpenActivity class
