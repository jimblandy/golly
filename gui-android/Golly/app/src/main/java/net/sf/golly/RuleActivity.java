// This file is part of Golly.
// See docs/License.html for the copyright notice.

package net.sf.golly;

import java.io.File;
import java.util.Arrays;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.EditText;
import android.widget.PopupMenu;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;
import android.widget.Toast;

public class RuleActivity extends Activity {

    // see jnicalls.cpp for these native routines:
    private native void nativeSaveCurrentSelection();
    private native int nativeGetAlgoIndex();
    private native String nativeGetAlgoName(int algoindex);
    private native String nativeGetRule();
    private native String nativeCheckRule(String rule, int algoindex);
    private native int nativeCheckAlgo(String rule, int algoindex);
    private native void nativeSetRule(String rule, int algoindex);
    
    // local fields:
    private static boolean firstcall = true;
    private static int algoindex = 0;           // index of currently displayed algorithm
    private static int numalgos;                // number of algorithms
    private static int[] scroll_pos;            // remember scroll position for each algo
    private Button algobutton;                  // for changing current algorithm
    private EditText ruletext;                  // for changing current rule
    private WebView gwebview;                   // for displaying html data
    
    private String bad_rule = "Given rule is not valid in any algorithm.";
    
    // -----------------------------------------------------------------------------
    
    // this class lets us intercept link taps and restore the scroll position
    private class MyWebViewClient extends WebViewClient {
        @Override
        public boolean shouldOverrideUrlLoading(WebView webview, String url) {
            if (url.startsWith("open:")) {
                openFile(url.substring(5));
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
            if (url.startsWith("rule:")) {
                // copy specified rule into ruletext
                ruletext.setText(url.substring(5));
                ruletext.setSelection(ruletext.getText().length());
                checkAlgo();
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
        }  
    }

    // -----------------------------------------------------------------------------
    
    private void saveScrollPosition() {
        scroll_pos[algoindex] = gwebview.getScrollY();
    }
    
    // -----------------------------------------------------------------------------
    
    private int restoreScrollPosition() {
        return scroll_pos[algoindex];
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
                        showAlgoHelp();
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

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.rule_layout);
        algobutton = (Button) findViewById(R.id.algo);
        ruletext = (EditText) findViewById(R.id.rule);

        gwebview = (WebView) findViewById(R.id.webview);
        gwebview.setWebViewClient(new MyWebViewClient());

        // no need for JavaScript???
        // gwebview.getSettings().setJavaScriptEnabled(true);
        
        DisplayMetrics metrics = getResources().getDisplayMetrics();
        // my Nexus 7 has a density of 320
        if (metrics.densityDpi > 300) {
            // use bigger font size for high density screens (default size is 16)
            gwebview.getSettings().setDefaultFontSize(24);
        }

        getActionBar().hide();

        if (firstcall) {
            firstcall = false;
            initAlgoInfo();
        }

        algoindex = nativeGetAlgoIndex();
        algobutton.setText(nativeGetAlgoName(algoindex));
        // onResume will call showAlgoHelp
        
        ruletext.setText(nativeGetRule());
        ruletext.setSelection(ruletext.getText().length());     // put cursor at end of rule

        // following listener allows us to detect when user is closing the soft keyboard
        // (it assumes rule_layout.xml sets android:imeOptions="actionDone")
        ruletext.setOnEditorActionListener(new OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if (actionId == EditorInfo.IME_ACTION_DONE) {
                    checkAlgo();
                }
                return false;
            }
        });
        
        // selection might change if grid becomes smaller, so save current selection
        // in case RememberRuleChange/RememberAlgoChange is called later
        nativeSaveCurrentSelection();
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
        restoreScrollPosition();
        showAlgoHelp();
    }
    
    // -----------------------------------------------------------------------------

    private void initAlgoInfo() {
        // initialize number of algorithms and scroll positions
        numalgos = 0;
        while (true) {
            String algoname = nativeGetAlgoName(numalgos);
            if (algoname.length() == 0) break;
            numalgos++;
        }
        scroll_pos = new int[numalgos];
        for (int i = 0; i < numalgos; i++) {
            scroll_pos[i] = 0;
        }
    }
    
    // -----------------------------------------------------------------------------
    
    private String createRuleLinks(File dir, String prefix, String title, boolean candelete) {
        String htmldata = "<p>" + title + "</p><p>";
        int rulecount = 0;
        File[] files = dir.listFiles();
        if (files != null) {
            Arrays.sort(files);         // sort into alphabetical order
            for (File file : files) {
                if (file.isDirectory()) {
                    // ignore directory
                } else {
                    String filename = file.getName();
                    if (filename.endsWith(".rule")) {
                        if (candelete) {
                            // allow user to delete .rule file
                            htmldata += "<a href=\"delete:";
                            htmldata += prefix;
                            htmldata += filename;
                            htmldata += "\"><font size=-2 color='red'>DELETE</font></a>&nbsp;&nbsp;&nbsp;";
                            // allow user to edit .rule file
                            htmldata += "<a href=\"edit:";
                            htmldata += prefix;
                            htmldata += filename;
                            htmldata += "\"><font size=-2 color='green'>EDIT</font></a>&nbsp;&nbsp;&nbsp;";
                        } else {
                            // allow user to read supplied .rule file
                            htmldata += "<a href=\"edit:";
                            htmldata += prefix;
                            htmldata += filename;
                            htmldata += "\"><font size=-2 color='green'>READ</font></a>&nbsp;&nbsp;&nbsp;";
                        }
                        // use "open:" link rather than "rule:" link
                        htmldata += "<a href=\"open:";
                        htmldata += prefix;
                        htmldata += filename;
                        htmldata += "\">";
                        htmldata += filename.substring(0, filename.length() - 5);    // strip off .rule
                        htmldata += "</a><br>";
                        rulecount++;
                    }
                }
            }
        }
        if (rulecount == 0) htmldata += "</b>None.<b>";
        return htmldata;
    }
    
    // -----------------------------------------------------------------------------
    
    private void showAlgoHelp() {
        BaseApp baseapp = (BaseApp)getApplicationContext();
        String algoname = nativeGetAlgoName(algoindex);
        if (algoname.equals("RuleLoader")) {
            // create html data with links to the user's .rule files and the supplied .rule files
            File userrules = new File(baseapp.userdir, "Rules");
            File rulesdir = new File(baseapp.supplieddir, "Rules");
            String htmldata = "";
            htmldata += "<html><body bgcolor=\"#FFFFCE\"><a name=\"top\"></a>";
            htmldata += "<p>The RuleLoader algorithm allows CA rules to be specified in .rule files.";
            htmldata += " Given the rule string \"Foo\", RuleLoader will search for a file called Foo.rule";
            htmldata += " in your rules folder, then in the rules folder supplied with Golly.";
            htmldata += "</p><font color=\"black\"><b>";
            
            htmldata += createRuleLinks(userrules, "Rules/", "Your .rule files:", true);
            htmldata += createRuleLinks(rulesdir, "Supplied/Rules/", "Supplied .rule files:", false);
            // note that we use a prefix starting with "Supplied/" so editFile can distinguish
            // between a user rule and a supplied rule
            
            htmldata += "</b></font></body></html>";
            gwebview.loadDataWithBaseURL(null, htmldata, "text/html", "utf-8", null);
        } else {
            // replace any spaces with underscores
            algoname = algoname.replaceAll(" ", "_");
            // display html data in supplieddir/Help/Algorithms/algoname.html
            String fullpath = baseapp.supplieddir.getAbsolutePath() + "/Help/Algorithms/" + algoname + ".html";
            File htmlfile = new File(fullpath);
            if (htmlfile.exists()) {
                gwebview.loadUrl("file://" + fullpath);     // fullpath starts with "/"
            } else {
                // should never happen
                String htmldata = "<html><center><font color=\"red\">Failed to find html file!</font></center></html>";
                gwebview.loadDataWithBaseURL(null, htmldata, "text/html", "utf-8", null);
            }
        }
    }
    
    // -----------------------------------------------------------------------------
    
    // called when algobutton is tapped
    public void doAlgo(View view) {
        // display pop-up menu with algorithm names
        PopupMenu popup = new PopupMenu(this, view);
        for (int id = 0; id < numalgos; id++) {
            String algoname = nativeGetAlgoName(id);
            popup.getMenu().add(Menu.NONE, id, Menu.NONE, algoname);
        }
        
        popup.setOnMenuItemClickListener(new PopupMenu.OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem item) {
                int id = item.getItemId();
                String algoname = nativeGetAlgoName(id);
                if (id != algoindex && algoname.length() > 0) {
                    // change algorithm
                    saveScrollPosition();
                    algoindex = id;
                    algobutton.setText(algoname);
                    showAlgoHelp();
                    // rule needs to change if it isn't valid in new algo
                    String currentrule = ruletext.getText().toString();
                    String newrule = nativeCheckRule(currentrule, algoindex);
                    if (!newrule.equals(currentrule)) {
                        ruletext.setText(newrule);
                        ruletext.setSelection(ruletext.getText().length());
                    }
                }
                return true;
            }
        });
        
        popup.show();
    }
    
    // -----------------------------------------------------------------------------

    private void checkAlgo() {
        // check displayed rule and show message if it's not valid in any algo,
        // or if the rule is valid then displayed algo might need to change
        String rule = ruletext.getText().toString();
        if (rule.length() == 0) {
            // change empty rule to Life
            rule = "B3/S23";
            ruletext.setText(rule);
            ruletext.setSelection(ruletext.getText().length());
        }
        int newalgo = nativeCheckAlgo(rule, algoindex);
        if (newalgo == -1) {
            Toast.makeText(this, bad_rule, Toast.LENGTH_LONG).show();
        } else if (newalgo != algoindex) {
            // change algorithm
            saveScrollPosition();
            algoindex = newalgo;
            algobutton.setText(nativeGetAlgoName(algoindex));
            showAlgoHelp();
        }
    }
    
    // -----------------------------------------------------------------------------
    
    // called when the Done button is tapped (not the soft keyboard's Done button)
    public void doDone(View view) {
        String rule = ruletext.getText().toString();
        // no need to check for empty rule here
        int newalgo = nativeCheckAlgo(rule, algoindex);
        if (newalgo == -1) {
            Toast.makeText(this, bad_rule, Toast.LENGTH_LONG).show();
        } else {
            // dismiss dialog first in case ChangeAlgorithm calls BeginProgress
            finish();
            nativeSetRule(rule, newalgo);
        }
    }
    
    // -----------------------------------------------------------------------------
    
    // called when the Cancel button is tapped
    public void doCancel(View view) {
        finish();
    }

} // RuleActivity class
