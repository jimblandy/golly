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

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileWriter;
import java.io.InputStreamReader;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Toast;

public class EditActivity extends Activity {
    
    public final static String EDITFILE_MESSAGE = "net.sf.golly.EDITFILE";
    
    private EditText etext;         // the text to be edited
    private boolean textchanged;    // true if user changed text
    private String filename;        // name of file being edited
    private String fileextn;        // filename's extension
    private String dirpath;         // location of filename
    
    // -----------------------------------------------------------------------------

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.edit_layout);

        etext = (EditText) findViewById(R.id.edit_text);
        // next call prevents long lines wrapping and enables horizontal scrolling
        etext.setHorizontallyScrolling(true);
        textchanged = false;

        getActionBar().hide();
        
        // get full file path sent by other activity
        Intent intent = getIntent();
        String filepath = intent.getStringExtra(EDITFILE_MESSAGE);
        if (filepath != null) {
            editTextFile(filepath);
        }
    }
    
    // -----------------------------------------------------------------------------
    
    private boolean error = false;
    
    private void editTextFile(String filepath) {
        File file = new File(filepath);
        dirpath = filepath.substring(0, filepath.lastIndexOf('/') + 1);
        filename = file.getName();
        fileextn = "";
        int i = filename.lastIndexOf('.');
        if (i > 0) fileextn = filename.substring(i);
        
        // read contents of supplied file into a string
        String filecontents;
        try {
            FileInputStream instream = new FileInputStream(file);
            BufferedReader reader = new BufferedReader(new InputStreamReader(instream));
            StringBuilder sb = new StringBuilder();
            String line = null;
            while ((line = reader.readLine()) != null) {
                sb.append(line);
                sb.append("\n");
            }
            filecontents = sb.toString();
            instream.close();        
        } catch (Exception e) {
            filecontents = "Error reading file:\n" + e.toString();
            // best to hide Save button if error occurred
            Button savebutton = (Button) findViewById(R.id.save);
            savebutton.setVisibility(View.INVISIBLE);
            error = true;
        }
        
        // display file contents (or error message)
        etext.setText(filecontents);
        
        // following stuff will detect any changes to text
        etext.addTextChangedListener(new TextWatcher() {
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                if (!error) textchanged = true;
            }
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {
                // ignore
            }
            public void afterTextChanged(Editable s) {
                // ignore
            }
        });
    }
    
    // -----------------------------------------------------------------------------
    
    private boolean writeFile(File file, final String contents) {
        try {
            FileWriter out = new FileWriter(file);
            out.write(contents);
            out.close();
        } catch (Exception e) {
            Toast.makeText(this, "Error writing file: " + e.toString(), Toast.LENGTH_LONG).show();
            return false;
        }
        return true;
    }

    // -----------------------------------------------------------------------------

    private int answer;
    
    private boolean ask(String title, String msg) {
        // use a handler to get a modal dialog
        final Handler handler = new Handler() {
            public void handleMessage(Message msg) {
                throw new RuntimeException();
            } 
        };
        
        AlertDialog.Builder alert = new AlertDialog.Builder(this);
        alert.setTitle(title);
        alert.setMessage(msg);
        alert.setPositiveButton("YES",
            new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int id) {
                    answer = 1;
                    dialog.cancel();
                    handler.sendMessage(handler.obtainMessage());
                }
            });
        alert.setNegativeButton("NO",
            new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int id) {
                    answer = 0;
                    dialog.cancel();
                    handler.sendMessage(handler.obtainMessage());
                }
            });
        alert.show();
        
        // loop until runtime exception is triggered
        try { Looper.loop(); } catch(RuntimeException re) {}

        return answer == 1;
    }
    
    // -----------------------------------------------------------------------------
    
    // called when the Save button is tapped
    public void doSave(View view) {
        AlertDialog.Builder alert = new AlertDialog.Builder(this);

        alert.setTitle("Save file as:");
        if (fileextn.length() > 0)
            alert.setMessage("Extension must be " + fileextn);

        // use an EditText view to get (possibly new) file name
        final EditText input = new EditText(this);
        input.setSingleLine(true);
        input.setText(filename);
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
                    String newname = input.getText().toString();
                    if (newname.length() == 0) {
                        Toast.makeText(getApplicationContext(), "Enter a file name.", Toast.LENGTH_SHORT).show();
                        input.setText(filename);
                        return;
                    }
                    // check for valid extension
                    if (fileextn.length() > 0 && !newname.endsWith(fileextn)) {
                        Toast.makeText(getApplicationContext(), "File extension must be " + fileextn, Toast.LENGTH_SHORT).show();
                        return;
                    }
                    // check if given file already exists
                    File newfile = new File(dirpath + newname);
                    if (newfile.exists()) {
                        boolean replace = ask("Replace " + newname + "?",
                                              "A file with that name already exists.  Do you want to replace that file?");
                        if (!replace) return;
                    }
                    if (!writeFile(newfile, etext.getText().toString())) {
                        return;
                    }
                    // save succeeded
                    textchanged = false;
                    filename = newname;
                    dialog.dismiss();
                }
            });
    }
    
    // -----------------------------------------------------------------------------
    
    // called when the Cancel button is tapped
    public void doCancel(View view) {
        if (textchanged && ask("Save changes?",
                               "Do you want to save the changes you made?")) {
            doSave(view);
        } else {
            // return to previous activity
            finish();
        }
    }

} // EditActivity class
