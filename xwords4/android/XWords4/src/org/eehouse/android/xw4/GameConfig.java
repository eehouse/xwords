/* -*- compile-command: "cd ../../../../../; ant reinstall"; -*- */

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
import android.app.ListActivity;
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
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ListView;
import android.widget.ListAdapter;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import android.widget.Button;
import android.widget.EditText;
import java.io.PrintStream;
import java.io.FileOutputStream;
import android.text.Editable;
import android.database.DataSetObserver;
import android.app.Dialog;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.view.LayoutInflater;
import android.widget.CheckBox;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MenuInflater;
import android.widget.Spinner;
import android.widget.ArrayAdapter;
import android.webkit.WebView;
import java.io.File;

import org.eehouse.android.xw4.jni.*;


/**
 * A generic activity for editing a note in a database.  This can be used
 * either to simply view a note {@link Intent#ACTION_VIEW}, view and edit a note
 * {@link Intent#ACTION_EDIT}, or create a new note {@link Intent#ACTION_INSERT}.  
 */
public class GameConfig extends ListActivity implements View.OnClickListener {

    private static final int PLAYER_EDIT = 1;

    private Button mDoneB;
    private Button m_addPlayerButton;
    private String m_path;
    private CurGameInfo m_gi;
    private int m_whichPlayer;
    private Dialog m_curDialog;
    private Spinner m_dictSpinner;
    private String[] m_dicts;
    private int m_browsePosition;

    private class PlayerListAdapter implements ListAdapter {
        public boolean areAllItemsEnabled() {
            return true;
        }

        public boolean isEnabled( int position ) {
            return position < m_gi.nPlayers;
        }
    
        public int getCount() {
            return m_gi.nPlayers;
        }
    
        public Object getItem( int position ) {
            TextView view = new TextView( GameConfig.this );
            LocalPlayer lp = m_gi.players[position];
            StringBuffer sb = new StringBuffer()
                .append( lp.name )
                .append( lp.isRobot? " (ROBOT)":" (HUMAN)")
                .append( lp.isLocal? " (LOCAL)":" (REMOTE)" )
                ;

            view.setText( sb.toString() );
            return view;
        }

        public long getItemId( int position ) {
            return position;
        }

        public int getItemViewType( int position ) {
            return 0;
        }

        public View getView( int position, View convertView, ViewGroup parent ) {
            return (View)getItem( position );
        }

        public int getViewTypeCount() {
            return 1;
        }

        public boolean hasStableIds() {
            return true;
        }

        public boolean isEmpty() {
            return false;
        }

        public void registerDataSetObserver(DataSetObserver observer) {}
        public void unregisterDataSetObserver(DataSetObserver observer) {}
    } // class PlayerListAdapter 

    @Override
    protected Dialog onCreateDialog( int id )
    {
        switch (id) {
        case PLAYER_EDIT:
            LayoutInflater factory = LayoutInflater.from(this);
            final View playerEditView
                = factory.inflate( R.layout.player_edit, null );

            DialogInterface.OnClickListener dlpos =
                new DialogInterface.OnClickListener() {
                    public void onClick( DialogInterface dialog, 
                                         int whichButton ) {
                        getPlayerSettings();
                        onContentChanged();
                    }
                };

            return new AlertDialog.Builder( this )
                // .setIcon(R.drawable.alert_dialog_icon)
                .setTitle(R.string.player_edit_title)
                .setView(playerEditView)
                .setPositiveButton(R.string.button_save, dlpos )
                .create();
        }
        return null;
    }

    @Override
    protected void onPrepareDialog( int id, Dialog dialog )
    { 
        m_curDialog = dialog;
        setPlayerSettings();
        super.onPrepareDialog( id, dialog );
    }

    private void setPlayerSettings()
    {
        LocalPlayer lp = m_gi.players[m_whichPlayer];
        EditText player = (EditText)
            m_curDialog.findViewById( R.id.player_name_edit );
        player.setText( lp.name );
        CheckBox isRobot = (CheckBox)
            m_curDialog.findViewById( R.id.robot_check );
        isRobot.setChecked( lp.isRobot );
    }

    private void getPlayerSettings()
    {
        LocalPlayer lp = m_gi.players[m_whichPlayer];
        EditText player = (EditText)
            m_curDialog.findViewById( R.id.player_name_edit );
        lp.name = player.getText().toString();
        CheckBox isRobot = (CheckBox)
            m_curDialog.findViewById( R.id.robot_check );
        lp.isRobot = isRobot.isChecked();
    }

    @Override
    public void onCreate( Bundle savedInstanceState )
    {
        super.onCreate(savedInstanceState);

        Intent intent = getIntent();
        Uri uri = intent.getData();
        m_path = uri.getPath();
        if ( m_path.charAt(0) == '/' ) {
            m_path = m_path.substring( 1 );
        }

        byte[] stream = Utils.savedGame( this, m_path );
        m_gi = new CurGameInfo();
        XwJNI.gi_from_stream( m_gi, stream );
        int curSel = listAvailableDicts( m_gi.dictName );
        Utils.logf( "listAvailableDicts done" );

        setContentView(R.layout.game_config);
        registerForContextMenu( getListView() );

        mDoneB = (Button)findViewById(R.id.game_config_done);
        mDoneB.setOnClickListener( this );

        m_addPlayerButton = (Button)findViewById(R.id.add_player);
        m_addPlayerButton.setOnClickListener( this );

        m_dictSpinner = (Spinner)findViewById( R.id.dict_spinner );
        ArrayAdapter<String> adapter = 
            new ArrayAdapter<String>( this,
                                      android.R.layout.simple_spinner_item,
                                      m_dicts );
        int resID = android.R.layout.simple_spinner_dropdown_item;
        adapter.setDropDownViewResource( resID );
        m_dictSpinner.setAdapter( adapter );
        if ( curSel >= 0 ) {
            m_dictSpinner.setSelection( curSel );
        } 

        m_dictSpinner.setOnItemSelectedListener(new OnItemSelectedListener() {
                @Override
                public void onItemSelected(AdapterView<?> parentView, 
                                           View selectedItemView, int position, 
                                           long id) {
                    if ( position == m_browsePosition ) {
                        launchDictBrowser();
                    } else {
                        m_gi.dictName = m_dicts[position];
                        Utils.logf( "assigned dictName: " + m_gi.dictName );
                    }
                }

                @Override
                public void onNothingSelected(AdapterView<?> parentView) {
                }
            });

        setListAdapter( new PlayerListAdapter() );
    } // onCreate

    @Override
    public void onCreateContextMenu( ContextMenu menu, View view, 
                                     ContextMenuInfo menuInfo ) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate( R.menu.players_list_item_menu, menu );
    }

    @Override
    public boolean onContextItemSelected( MenuItem item ) 
    {
        boolean handled = true;
        boolean changed = false;

        AdapterView.AdapterContextMenuInfo info;
        try {
            info = (AdapterView.AdapterContextMenuInfo) item.getMenuInfo();
        } catch (ClassCastException e) {
            Utils.logf( "bad menuInfo:" + e.toString() );
            return false;
        }

        switch (item.getItemId()) {
            case R.id.player_list_item_edit:
                showDialog( PLAYER_EDIT );
                break;
            case R.id.player_list_item_up:
                changed = m_gi.moveUp( info.position );
                break;
            case R.id.player_list_item_down:
                changed = m_gi.moveDown( info.position );
                break;
            case R.id.player_list_item_delete:
                changed = m_gi.delete( info.position );
                break;
        default:
            handled = false;
        }

        if ( changed ) {
            onContentChanged();
        }

        return handled;
    }

    @Override
    protected void onListItemClick(ListView l, View v, int position, long id) {
        m_whichPlayer = position;
        showDialog( PLAYER_EDIT );
    }

    @Override
    public boolean onCreateOptionsMenu( Menu menu )
    {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate( R.menu.game_config_menu, menu );
        return true;
    }

    @Override
    public boolean onOptionsItemSelected( MenuItem item )
    {
        boolean handled = true;

        switch (item.getItemId()) {
        case R.id.game_config_juggle:
            m_gi.juggle();
            onContentChanged();
            break;
        default:
            handled = false;
        }
        return handled;
    }

    public void onClick( View view ) 
    {
        if ( mDoneB == view ) {
            byte[] bytes = XwJNI.gi_to_stream( m_gi );
            if ( null == bytes ) {
                Utils.logf( "gi_to_stream failed" );
            } else {
                Utils.logf( "got " + bytes.length + " bytes." );
                Utils.saveGame( this, bytes, m_path );
            }
            finish();
        } else if ( m_addPlayerButton == view ) {
            int curIndex = m_gi.nPlayers;
            if ( curIndex < CurGameInfo.MAX_NUM_PLAYERS ) {
                m_gi.addPlayer();
                m_whichPlayer = curIndex;
                showDialog( PLAYER_EDIT );
            }
        } else {
            Utils.logf( "unknown v: " + view.toString() );
        }
    } // onClick


    private int listAvailableDicts( String curDict )
    {
        int curSel = -1;

        String[] list = Utils.listDicts( this );

        m_browsePosition = list.length;
        m_dicts = new String[m_browsePosition+1];
        m_dicts[m_browsePosition] = getString( R.string.download_dicts );
        
        for ( int ii = 0; ii < m_browsePosition; ++ii ) {
            String dict = list[ii];
            m_dicts[ii] = dict;
            if ( dict.equals( curDict ) ) {
                curSel = ii;
            }
        }

        return curSel;
    }

    private void launchDictBrowser()
    {
        Intent intent = new Intent( this, DictActivity.class );
        intent.setAction( Intent.ACTION_EDIT );
        startActivity( intent );
    }
}
