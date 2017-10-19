// This file is part of Golly.
// See docs/License.html for the copyright notice.

package net.sf.golly;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.text.method.ScrollingMovementMethod;
import android.util.DisplayMetrics;
import android.view.View;
import android.widget.TextView;

public class InfoActivity extends Activity {
    
	private native String nativeGetInfo();             // the rest must NOT be static
	
    public final static String INFO_MESSAGE = "net.sf.golly.INFO";
    
    // -----------------------------------------------------------------------------

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.info_layout);
        
        getActionBar().hide();
        
        String text = null;
        
        // get info sent by other activity
        Intent intent = getIntent();
        String infoMsg = intent.getStringExtra(INFO_MESSAGE);
        if (infoMsg != null) {
        	if (infoMsg.equals("native")) {
        		text = nativeGetInfo();
        	} else {
        		// read contents of supplied file into a string
                File file = new File(infoMsg);
                try {
                    FileInputStream instream = new FileInputStream(file);
                    BufferedReader reader = new BufferedReader(new InputStreamReader(instream));
                    StringBuilder sb = new StringBuilder();
                    String line = null;
                    while ((line = reader.readLine()) != null) {
                        sb.append(line);
                        sb.append("\n");
                    }
                    text = sb.toString();
                    instream.close();        
                } catch (Exception e) {
                    text = "Error reading file:\n" + e.toString();
                }
        	}
        }
        
        if (text != null) {
            TextView tv = (TextView) findViewById(R.id.info_text);
            tv.setMovementMethod(new ScrollingMovementMethod());
            // above call enables vertical scrolling;
            // next call prevents long lines wrapping and enables horizontal scrolling
            tv.setHorizontallyScrolling(true);
            tv.setTextIsSelectable(true);
            tv.setText(text);
            
            DisplayMetrics displayMetrics = getResources().getDisplayMetrics();
            float dpWidth = displayMetrics.widthPixels / displayMetrics.density;
            
            if (dpWidth >= 600) {
                // use bigger font size for wide screens (layout size is 10sp)
                tv.setTextSize(12);
            }
        }
    }
    
    // -----------------------------------------------------------------------------
    
    // called when the Cancel button is tapped
    public void doCancel(View view) {
        // return to previous activity
        finish();
    }

} // InfoActivity class
