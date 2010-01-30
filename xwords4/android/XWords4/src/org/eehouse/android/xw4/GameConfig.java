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
import android.widget.LinearLayout;

import org.eehouse.android.xw4.jni.*;


/**
 * A generic activity for editing a note in a database.  This can be used
 * either to simply view a note {@link Intent#ACTION_VIEW}, view and edit a note
 * {@link Intent#ACTION_EDIT}, or create a new note {@link Intent#ACTION_INSERT}.  
 */
public class GameConfig extends Activity implements View.OnClickListener {

    private static final int PLAYER_EDIT = 1;
    private static final int ROLE_EDIT = 2;

    private Button m_addPlayerButton;
    private Button m_configureButton;
    private String m_path;
    private CurGameInfo m_gi;
    private int m_whichPlayer;
    private Dialog m_curDialog;
    private Spinner m_dictSpinner;
    private Spinner m_roleSpinner;
    private Spinner m_phoniesSpinner;
    private String[] m_dicts;
    private int m_browsePosition;
    private LinearLayout m_playerLayout;
    private CommsAddrRec m_car;

    @Override
    protected Dialog onCreateDialog( int id )
    {
        LayoutInflater factory;
        DialogInterface.OnClickListener dlpos;

        switch (id) {
        case PLAYER_EDIT:
            factory = LayoutInflater.from(this);
            final View playerEditView
                = factory.inflate( R.layout.player_edit, null );

            dlpos = new DialogInterface.OnClickListener() {
                    public void onClick( DialogInterface dialog, 
                                         int whichButton ) {
                        getPlayerSettings();
                        loadPlayers();
                    }
                };

            return new AlertDialog.Builder( this )
                // .setIcon(R.drawable.alert_dialog_icon)
                .setTitle(R.string.player_edit_title)
                .setView(playerEditView)
                .setPositiveButton(R.string.button_save, dlpos )
                .create();
        case ROLE_EDIT:
            factory = LayoutInflater.from(this);
            final View roleEditView
                = factory.inflate( R.layout.role_edit, null );

            dlpos = new DialogInterface.OnClickListener() {
                    public void onClick( DialogInterface dialog, 
                                         int whichButton ) {
                        getRoleSettings();
                    }
                };

            return new AlertDialog.Builder( this )
                // .setIcon(R.drawable.alert_dialog_icon)
                .setTitle(R.string.role_edit_title)
                .setView(roleEditView)
                .setPositiveButton(R.string.button_save, dlpos )
                .create();
        }
        return null;
    }

    @Override
    protected void onPrepareDialog( int id, Dialog dialog )
    { 
        m_curDialog = dialog;
        switch ( id ) {
        case PLAYER_EDIT:
            setPlayerSettings();
            break;
        case ROLE_EDIT:
            setRoleSettings();
            break;
        }
        super.onPrepareDialog( id, dialog );
    }

    private void setPlayerSettings()
    {
        LocalPlayer lp = m_gi.players[m_whichPlayer];
        EditText player = (EditText)
            m_curDialog.findViewById( R.id.player_name_edit );
        player.setText( lp.name );

        Utils.setChecked( m_curDialog, R.id.robot_check, lp.isRobot );
        Utils.setChecked( m_curDialog, R.id.remote_check, ! lp.isLocal );
    }

    private void setRoleSettings()
    {
        if ( null == m_car ) {
            m_car = new CommsAddrRec( CommsAddrRec.get() );
        }
        Utils.setText( m_curDialog, R.id.room_edit, m_car.ip_relay_invite );
        Utils.setText( m_curDialog, R.id.hostname_edit, 
                       m_car.ip_relay_hostName );
        Utils.setInt( m_curDialog, R.id.port_edit, m_car.ip_relay_port );
    }

    private void getRoleSettings()
    {
        m_car.ip_relay_invite = Utils.getText( m_curDialog, R.id.room_edit );
        m_car.ip_relay_hostName = Utils.getText( m_curDialog, 
                                                R.id.hostname_edit );
        m_car.ip_relay_port = Utils.getInt( m_curDialog, R.id.port_edit );
    }

    private void getPlayerSettings()
    {
        LocalPlayer lp = m_gi.players[m_whichPlayer];
        EditText player = (EditText)
            m_curDialog.findViewById( R.id.player_name_edit );
        lp.name = player.getText().toString();

        lp.isRobot = Utils.getChecked( m_curDialog, R.id.robot_check );
        lp.isLocal = !Utils.getChecked( m_curDialog, R.id.remote_check );
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
        m_gi = new CurGameInfo( this );
        XwJNI.gi_from_stream( m_gi, stream );
        int curSel = listAvailableDicts( m_gi.dictName );

        setContentView(R.layout.game_config);

        m_addPlayerButton = (Button)findViewById(R.id.add_player);
        m_addPlayerButton.setOnClickListener( this );
        m_configureButton = (Button)findViewById(R.id.configure_role);
        m_configureButton.setOnClickListener( this );

        m_playerLayout = (LinearLayout)findViewById( R.id.player_list );
        loadPlayers();

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

        m_roleSpinner = (Spinner)findViewById( R.id.role_spinner );
        adapter = 
            new ArrayAdapter<String>( this,
                                      android.R.layout.simple_spinner_item,
                                      new String[] {
                                          getString(R.string.role_standalone),
                                          getString(R.string.role_host),
                                          getString(R.string.role_guest),
                                      } );
        adapter.setDropDownViewResource( android.R.layout.simple_spinner_dropdown_item );
        m_roleSpinner.setAdapter( adapter );
        m_roleSpinner.setSelection( m_gi.serverRole.ordinal() );

        m_phoniesSpinner = (Spinner)findViewById( R.id.phonies_spinner );
        adapter = 
            new ArrayAdapter<String>( this,
                                      android.R.layout.simple_spinner_item,
                                      new String[] {
                                          getString(R.string.phonies_ignore),
                                          getString(R.string.phonies_warn),
                                          getString(R.string.phonies_disallow),
                                      } );
        adapter.setDropDownViewResource( android.R.layout.simple_spinner_dropdown_item );
        m_phoniesSpinner.setAdapter( adapter );
        m_phoniesSpinner.setSelection( m_gi.phoniesAction.ordinal() );

        Utils.setChecked( this, R.id.hints_allowed, !m_gi.hintsNotAllowed );
        Utils.setChecked( this, R.id.use_timer, m_gi.timerEnabled );
        Utils.setChecked( this, R.id.color_tiles, m_gi.showColors );
        Utils.setChecked( this, R.id.smart_robot, 0 < m_gi.robotSmartness );
    } // onCreate

    @Override
    public void onCreateContextMenu( ContextMenu menu, View view, 
                                     ContextMenuInfo menuInfo ) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate( R.menu.players_list_item_menu, menu );
        m_whichPlayer = ((PlayerView)view).getPosition();
    }

    @Override
    public boolean onContextItemSelected( MenuItem item ) 
    {
        boolean handled = true;
        boolean changed = false;

        switch (item.getItemId()) {
            case R.id.player_list_item_edit:
                showDialog( PLAYER_EDIT );
                break;
            case R.id.player_list_item_up:
                changed = m_gi.moveUp( m_whichPlayer );
                break;
            case R.id.player_list_item_down:
                changed = m_gi.moveDown( m_whichPlayer );
                break;
            case R.id.player_list_item_delete:
                changed = m_gi.delete( m_whichPlayer );
                break;
        default:
            handled = false;
        }

        if ( changed ) {
            loadPlayers();
        }

        return handled;
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
        case R.id.game_config_done:
            m_gi.hintsNotAllowed = !Utils.getChecked( this, R.id.hints_allowed );
            m_gi.timerEnabled = Utils.getChecked(  this, R.id.use_timer );
            m_gi.showColors = Utils.getChecked( this, R.id.color_tiles );
            m_gi.robotSmartness
                = Utils.getChecked( this, R.id.smart_robot ) ? 1 : 0;
            int position = m_roleSpinner.getSelectedItemPosition();
            Utils.logf( "setting serverrole: " + position );
            m_gi.serverRole = CurGameInfo.DeviceRole.values()[position];

            byte[] bytes = XwJNI.gi_to_stream( m_gi );
            if ( null == bytes ) {
                Utils.logf( "gi_to_stream failed" );
            } else {
                Utils.logf( "got " + bytes.length + " bytes." );
                Utils.saveGame( this, bytes, m_path );
            }

            if ( null != m_car ) {
                CommsAddrRec.set( m_car );
            }

            finish();
            break;
        default:
            handled = false;
        }
        return handled;
    }

    public void onClick( View view ) 
    {
        if ( m_addPlayerButton == view ) {
            int curIndex = m_gi.nPlayers;
            if ( curIndex < CurGameInfo.MAX_NUM_PLAYERS ) {
                m_gi.addPlayer(); // ups nPlayers
                loadPlayers();
                m_whichPlayer = curIndex;
                showDialog( PLAYER_EDIT );
            }
        } else if ( m_configureButton == view ) {
            showDialog( ROLE_EDIT );
        } else {
            Utils.logf( "unknown v: " + view.toString() );
        }
    } // onClick

    private void loadPlayers()
    {
        m_playerLayout.removeAllViews();

        for ( int ii = 0; ii < m_gi.nPlayers; ++ii ) {
            LayoutInflater factory = LayoutInflater.from(this);
            final PlayerView view
                = (PlayerView)factory.inflate( R.layout.player_view, null );
            view.setPosition( ii );
            // PlayerView view = new PlayerView( this, ii );
            LocalPlayer lp = m_gi.players[ii];
            view.setText( new StringBuffer()
                          .append( "\n" )
                          .append( lp.name )
                          .append( lp.isRobot? " (ROBOT)":" (HUMAN)")
                          .append( lp.isLocal? " (LOCAL)":" (REMOTE)" )
                          .toString() ); 
            registerForContextMenu( view );
            view.setOnClickListener( new View.OnClickListener() {
                    @Override
                    public void onClick( View view ) {
                        m_whichPlayer = ((PlayerView)view).getPosition();
                        showDialog( PLAYER_EDIT );
                    }
                } );
            m_playerLayout.addView( view );
            Utils.logf( "view.isFocusableInTouchMode()=>" + 
                        (view.isFocusableInTouchMode()?"true":"false" ) );
        }
    }

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
