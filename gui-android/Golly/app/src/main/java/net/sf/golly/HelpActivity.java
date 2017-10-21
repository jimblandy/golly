// This file is part of Golly.
// See docs/License.html for the copyright notice.

package net.sf.golly;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.support.v4.app.NavUtils;
import android.util.DisplayMetrics;
// import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.Toast;

public class HelpActivity extends Activity {

    // see jnicalls.cpp for these native routines:
    private static native void nativeClassInit();   // this MUST be static
    private native void nativeCreate();             // the rest must NOT be static
    private native void nativeDestroy();
    private native void nativeGetURL(String url, String pageurl);
    private native void nativeProcessDownload(String filepath);
    private native void nativeUnzipFile(String zippath);
    private native boolean nativeDownloadedFile(String url);

    // local fields:
    private static boolean firstcall = true;
    private static Bundle webbundle = null;         // for saving/restoring scroll position and page history
    private boolean restarted = false;              // onRestart was called before OnResume?
    private boolean clearhistory = false;           // tell onPageFinished to clear page history?
    private WebView gwebview;                       // for displaying html pages
    private Button backbutton;                      // go to previous page
    private Button nextbutton;                      // go to next page
    private static String pageurl;                  // URL for last displayed page
    private ProgressBar progbar;                    // progress bar for downloads
    private LinearLayout proglayout;                // view containing progress bar
    private boolean cancelled;                      // download was cancelled?

    public final static String SHOWHELP_MESSAGE = "net.sf.golly.SHOWHELP";
        
    // -----------------------------------------------------------------------------
    
    static {
    	nativeClassInit();      // caches Java method IDs
    }
    
    // -----------------------------------------------------------------------------
    
    // this class lets us intercept link taps
    private class MyWebViewClient extends WebViewClient {
        @Override
        public boolean shouldOverrideUrlLoading(WebView webview, String url) {
            // look for special prefixes used by Golly and return true if found
            if (url.startsWith("open:")) {
                openFile(url.substring(5));
                return true;
            }
            if (url.startsWith("edit:")) {
                editFile(url.substring(5));
                return true;
            }
            if (url.startsWith("rule:")) {
                changeRule(url.substring(5));
                return true;
            }
            if (url.startsWith("lexpatt:")) {
                // user tapped on pattern in Life Lexicon
                loadLexiconPattern(url.substring(8));
                return true;
            }
            if (url.startsWith("get:")) {
                // download file specifed in link (possibly relative to a previous full url)
                nativeGetURL(url.substring(4), pageurl);
                return true;
            }
            if (url.startsWith("unzip:")) {
                nativeUnzipFile(url.substring(6));
                return true;
            }
            // no special prefix, so look for file with .zip/rle/lif/mc extension and download it
            if (nativeDownloadedFile(url)) {
                return true;
            }
            return false;
        }
        
        @Override
        public void onReceivedError(WebView webview, int errorCode, String description, String failingUrl) {
            Toast.makeText(HelpActivity.this, "Web error! " + description, Toast.LENGTH_SHORT).show();
        }
        
        @Override
        public void onPageFinished(WebView webview, String url) {
            super.onPageFinished(webview, url);
            
            if (clearhistory) {
                // this only works after page is loaded
                gwebview.clearHistory();
                clearhistory = false;
            }
            backbutton.setEnabled(gwebview.canGoBack());
            nextbutton.setEnabled(gwebview.canGoForward());

            // need URL of this page for relative "get:" links
            pageurl = gwebview.getUrl();
            // Log.i("onPageFinished URL", pageurl);
        }  
    }

    // -----------------------------------------------------------------------------
    
    private void openFile(String filepath) {
        // switch to main screen and open given file
        Intent intent = new Intent(this, MainActivity.class);
        intent.putExtra(MainActivity.OPENFILE_MESSAGE, filepath);
        startActivity(intent);
    }
    
    // -----------------------------------------------------------------------------
    
    private void editFile(String filepath) {
        BaseApp baseapp = (BaseApp)getApplicationContext();
        // let user read/edit given file
        if (filepath.startsWith("Supplied/")) {
            // read contents of supplied .rule file into a string
            String prefix = baseapp.supplieddir.getAbsolutePath();
            prefix = prefix.substring(0, prefix.length() - 8);  // strip off "Supplied"
            String fullpath = prefix + filepath;
            // display filecontents
            Intent intent = new Intent(this, InfoActivity.class);
            intent.putExtra(InfoActivity.INFO_MESSAGE, fullpath);
            startActivity(intent);
        } else {
            // let user read or edit user's .rule file
            String fullpath = baseapp.userdir.getAbsolutePath() + "/" + filepath;
            Intent intent = new Intent(this, EditActivity.class);
            intent.putExtra(EditActivity.EDITFILE_MESSAGE, fullpath);
            startActivity(intent);
        }
    }

    // -----------------------------------------------------------------------------
    
    private void changeRule(String rule) {
        // switch to main screen and change rule
        Intent intent = new Intent(this, MainActivity.class);
        intent.putExtra(MainActivity.RULE_MESSAGE, rule);
        startActivity(intent);
    }
    
    // -----------------------------------------------------------------------------
    
    private void loadLexiconPattern(String pattern) {
        // switch to main screen and load given lexicon pattern
        Intent intent = new Intent(this, MainActivity.class);
        intent.putExtra(MainActivity.LEXICON_MESSAGE, pattern);
        startActivity(intent);
    }
    
    // -----------------------------------------------------------------------------
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        setContentView(R.layout.help_layout);
        backbutton = (Button) findViewById(R.id.back);
        nextbutton = (Button) findViewById(R.id.forwards);
        progbar = (ProgressBar) findViewById(R.id.progress_bar);
        proglayout = (LinearLayout) findViewById(R.id.progress_layout);
        
        proglayout.setVisibility(LinearLayout.INVISIBLE);
        
        // show the Up button in the action bar
        getActionBar().setDisplayHomeAsUpEnabled(true);
        
        nativeCreate();     // cache this instance
        
        gwebview = (WebView) findViewById(R.id.webview);
        gwebview.setWebViewClient(new MyWebViewClient());
        
        // JavaScript is used to detect device type
        gwebview.getSettings().setJavaScriptEnabled(true);

        // no need???
        // gwebview.getSettings().setDomStorageEnabled(true);
        
        DisplayMetrics metrics = getResources().getDisplayMetrics();
        // my Nexus 7 has a density of 320
        if (metrics.densityDpi > 300) {
            // use bigger font size for high density screens (default size is 16)
            gwebview.getSettings().setDefaultFontSize(24);
        }

        if (firstcall) {
            firstcall = false;
            backbutton.setEnabled(false);
            nextbutton.setEnabled(false);
            showContentsPage();
            // pageurl has now been initialized
        }
    }
    
    // -----------------------------------------------------------------------------
    
    @Override
    protected void onNewIntent(Intent intent) {
    	setIntent(intent);
    	restarted = true;
    }
    
    // -----------------------------------------------------------------------------
    
    @Override
    protected void onPause() {
        super.onPause();
        gwebview.onPause();
        
        // save scroll position and back/forward history
        if (webbundle == null) webbundle = new Bundle();
        gwebview.saveState(webbundle);
    }

    // -----------------------------------------------------------------------------
    
    @Override
    protected void onResume() {
        super.onResume();
        gwebview.onResume();

        // restore scroll position and back/forward history
        if (webbundle != null && !restarted) {
            gwebview.restoreState(webbundle);
        }
        restarted = false;

        // check for messages sent by other activities
        Intent intent = getIntent();
        String filepath = intent.getStringExtra(SHOWHELP_MESSAGE);
        if (filepath != null) {
            // replace any spaces with %20
            filepath = filepath.replaceAll(" ", "%20");
            // Log.i("onResume filepath", filepath);
            gwebview.loadUrl("file://" + filepath);
        } else {
            // no need: gwebview.reload();
        }
    }

    // -----------------------------------------------------------------------------

    @Override
    protected void onRestart() {
        super.onRestart();
        // set flag to prevent onResume calling gwebview.restoreState
        // causing app to crash for some unknown reason
        restarted = true;
    }

    // -----------------------------------------------------------------------------

    @Override
    protected void onDestroy() {
        nativeDestroy();
        super.onDestroy();
    }

    // -----------------------------------------------------------------------------

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // add main.xml items to the action bar
        getMenuInflater().inflate(R.menu.main, menu);
        
        // disable the item for this activity
        MenuItem item = menu.findItem(R.id.help);
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
            	finish();
                intent = new Intent(this, SettingsActivity.class);
                startActivity(intent);
                return true;
            case R.id.help:
                // do nothing
                break;
        }
        return super.onOptionsItemSelected(item);
    }
    
    // -----------------------------------------------------------------------------
    
    private void showContentsPage() {
        BaseApp baseapp = (BaseApp)getApplicationContext();
        // display html data in supplieddir/Help/index.html
        String fullpath = baseapp.supplieddir.getAbsolutePath() + "/Help/index.html";
        File htmlfile = new File(fullpath);
        if (htmlfile.exists()) {
            gwebview.loadUrl("file://" + fullpath);     // fullpath starts with "/"
        } else {
            // should never happen
            String htmldata = "<html><center><font size=+1 color=\"red\">Failed to find index.html!</font></center></html>";
            gwebview.loadDataWithBaseURL(null, htmldata, "text/html", "utf-8", null);
        }
    }
    
    // -----------------------------------------------------------------------------
    
    // called when the Contents button is tapped
    public void doContents(View view) {
        clearhistory = true;    // tell onPageFinished to clear page history
        showContentsPage();
        if (webbundle != null) {
            webbundle.clear();
            webbundle = null;
        }
    }
    
    // -----------------------------------------------------------------------------
    
    // called when backbutton is tapped
    public void doBack(View view) {
        gwebview.goBack();
    }
    
    // -----------------------------------------------------------------------------
    
    // called when nextbutton is tapped
    public void doForwards(View view) {
        gwebview.goForward();
    }
    
    // -----------------------------------------------------------------------------
    
    // called when Cancel is tapped
    public void doCancel(View view) {
        cancelled = true;
    }
    
    // -----------------------------------------------------------------------------

    private String downloadURL(String urlstring, String filepath) {
        // download given url and save data in given file
        try {
            File outfile = new File(filepath);
            final int BUFFSIZE = 8192;
            FileOutputStream outstream = null;
            try {
                outstream = new FileOutputStream(outfile);
            } catch (FileNotFoundException e) {
                return "File not found: " + filepath;
            }
            
            long starttime = System.nanoTime();
            // Log.i("downloadURL", urlstring);
            URL url = new URL(urlstring);
            HttpURLConnection connection = (HttpURLConnection) url.openConnection();
            connection.setAllowUserInteraction(false);
            connection.setInstanceFollowRedirects(true);
            connection.setRequestMethod("GET");
            connection.connect();
            if (connection.getResponseCode() != HttpURLConnection.HTTP_OK) {
                outstream.close();
                return "No HTTP_OK response.";
            }
            
            // init info for progress bar
            int filesize = connection.getContentLength();
            int downloaded = 0;
            int percent;
            int lastpercent = 0;
            
            // stream the data to given file
            InputStream instream = connection.getInputStream();
            byte[] buffer = new byte[BUFFSIZE];
            int bufflen = 0;
            while ((bufflen = instream.read(buffer, 0, BUFFSIZE)) > 0) {
                outstream.write(buffer, 0, bufflen);
                downloaded += bufflen;
                percent = (int) ((downloaded / (float)filesize) * 100);
                if (percent > lastpercent) {
                    progbar.setProgress(percent);
                    lastpercent = percent;
                }
                // show proglayout only if download takes more than 1 second
                if (System.nanoTime() - starttime > 1000000000L) {
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            proglayout.setVisibility(LinearLayout.VISIBLE);
                        }
                    });
                    starttime = Long.MAX_VALUE; // only show it once
                }

                if (cancelled) break;
            }
            outstream.close();
            connection.disconnect();
            if (cancelled) return "Cancelled.";
            
        } catch (MalformedURLException e) {
            return "Bad URL string: " + urlstring;
        } catch (IOException e) {
            return "Could not connect to URL: " + urlstring;
        }
        return "";  // success
    }
    
    // -----------------------------------------------------------------------------

    private String dresult;

    // this method is called from C++ code (see jnicalls.cpp)
    private void DownloadFile(String urlstring, String filepath) {
        cancelled = false;
        progbar.setProgress(0);
        // don't show proglayout immediately
        // proglayout.setVisibility(LinearLayout.VISIBLE);

        // we cannot do network connections on main thread
        final Handler handler = new Handler();
        final String durl = urlstring;
        final String dfile = filepath;
        Thread download_thread = new Thread(new Runnable() {
            @Override
            public void run() {
                dresult = downloadURL(durl, dfile);
                handler.post(new Runnable() {
                    @Override
                    public void run() {
                        proglayout.setVisibility(LinearLayout.INVISIBLE);
                        if (dresult.length() == 0) {
                            // download succeeded
                            nativeProcessDownload(dfile);
                        } else if (!cancelled) {
                            Toast.makeText(HelpActivity.this, "Download failed! " + dresult, Toast.LENGTH_SHORT).show();
                        }
                    }
                });
            }
        });
        
        download_thread.setPriority(Thread.MAX_PRIORITY);
        download_thread.start();
    }

} // HelpActivity class
