/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.eehouse.android.xw4;

import android.app.Activity;
import android.content.ComponentName;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Bundle;
import android.util.AttributeSet;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.EditText;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import java.io.PrintStream;
import java.io.FileOutputStream;
import android.text.Editable;

/**
 * A generic activity for editing a note in a database.  This can be used
 * either to simply view a note {@link Intent#ACTION_VIEW}, view and edit a note
 * {@link Intent#ACTION_EDIT}, or create a new note {@link Intent#ACTION_INSERT}.  
 */
public class GameConfig extends Activity implements View.OnClickListener {
    private static final String TAG = "Games";

    /** The index of the note column */
    private static final int COLUMN_INDEX_NOTE = 1;
    
    // This is our state data that is stored when freezing.
    private static final String ORIGINAL_CONTENT = "origContent";

    // Identifiers for our menu items.
    private static final int REVERT_ID = Menu.FIRST;
    private static final int DISCARD_ID = Menu.FIRST + 1;
    private static final int DELETE_ID = Menu.FIRST + 2;

    // The different distinct states the activity can be run in.
    private static final int STATE_EDIT = 0;
    private static final int STATE_INSERT = 1;

    private int mState;
    private boolean mNoteOnly = false;
    private Uri mUri;
    private String mOriginalContent;

    private Button mDoneB;
    private Button mOpenB;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        final Intent intent = getIntent();

        // Do some setup based on the action being performed.

        final String action = intent.getAction();
        Utils.logf( "action: " + action );
        if (Intent.ACTION_EDIT.equals(action)) {
            // Requested to edit: set that state, and the data being edited.
            mState = STATE_EDIT;
            mUri = intent.getData();
        } else if (Intent.ACTION_INSERT.equals(action)) {
            Utils.logf( "matches insert" );
            // Requested to insert: set that state, and create a new entry
            // in the container.
            mState = STATE_INSERT;
            // mUri = getContentResolver().insert(intent.getData(), null);
            // Utils.logf( mUri.toString() );

            // If we were unable to create a new note, then just finish
            // this activity.  A RESULT_CANCELED will be sent back to the
            // original activity if they requested a result.
            // if (mUri == null) {
            //     Log.e(TAG, "Failed to insert new note into " + getIntent().getData());
            //     finish();
            //     return;
            // }

            // The new entry was created, so assume all will end well and
            // set the result to be returned.
            // setResult(RESULT_OK, (new Intent()).setAction(mUri.toString()));

        } else {
            // Whoops, unknown action!  Bail.
            Utils.logf("Unknown action, exiting");
            finish();
            return;
        }

        setContentView(R.layout.game_config);

        // mOpenB = (Button)findViewById(R.id.game_config_open);
        // mOpenB.setOnClickListener( this );
        mDoneB = (Button)findViewById(R.id.game_config_done);
        mDoneB.setOnClickListener( this );

        // If an instance of this activity had previously stopped, we can
        // get the original text it started with.
        // if (savedInstanceState != null) {
        //     mOriginalContent = savedInstanceState.getString(ORIGINAL_CONTENT);
        // }
    } // onCreate

    public void onClick( View view ) {

        if ( mDoneB == view ) {
            EditText et = (EditText)findViewById( R.id.player_1_name );
            Editable text = et.getText(); 
            String name1 = text.toString();
            et = (EditText)findViewById( R.id.player_2_name );
            text = et.getText();
            String name2 = text.toString();

            if ( name1.length() > 0 && name2.length() > 0 ) {
                Integer num = 0;
                int ii;
                String[] files = fileList();
                String name = null;

                while ( name == null ) {
                    name = "game " + num.toString();
                    for ( ii = 0; ii < files.length; ++ii ) {
                        Utils.logf( "comparing " + name + " with " + files[ii] );
                        if ( files[ii].equals(name) ) {
                            ++num;
                            name = null;
                        }
                    }
                }
                Utils.logf( "using name " + name );

                FileOutputStream out;
                try {
                    out = openFileOutput( name, MODE_PRIVATE );
                    PrintStream ps = new PrintStream( out );
                    ps.println( name1 );
                    ps.println( name2 );
                    ps.close();
                    try {
                        out.close();
                    } catch ( java.io.IOException ex ) {
                        Utils.logf( "got IOException: " + ex.toString() );
                    }
                } catch ( java.io.FileNotFoundException ex ) {
                    Utils.logf( "got FileNotFoundException: " + ex.toString() );
                }
            }

            setResult( 1 );
            finish();
        } else if ( mOpenB == view ) {
            // finish but after posting an intent that'll cause the
            // list view to launch us -- however that's done.
            Utils.logf( "got open" );
        } else {
            Utils.logf( "unknown v: " + view.toString() );
        }
    } // onClick

}
