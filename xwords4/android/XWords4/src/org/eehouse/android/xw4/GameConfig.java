/* -*- compile-command: "cd ../../../../../; ant install"; -*- */

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
import java.util.ArrayList;
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
import android.widget.CompoundButton;
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
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.*;

public class GameConfig extends Activity implements View.OnClickListener {

    private static final int PLAYER_EDIT = 1;
    private static final int ROLE_EDIT_RELAY = 2;
    private static final int ROLE_EDIT_SMS = 3;
    private static final int ROLE_EDIT_BT = 4;

    private Button m_addPlayerButton;
    private Button m_configureButton;
    private String m_path;
    private CurGameInfo m_gi;
    private int m_whichPlayer;
    private Dialog m_curDialog;
    private Spinner m_roleSpinner;
    private Spinner m_connectSpinner;
    private Spinner m_phoniesSpinner;
    private Spinner m_dictSpinner;
    private String[] m_dicts;
    private int m_browsePosition;
    private LinearLayout m_playerLayout;
    private CommsAddrRec m_car;
    private int m_connLayoutID;
    private CommonPrefs m_cp;
    private boolean m_canDoSMS = false;
    private boolean m_canDoBT = false;
    private CommsAddrRec.CommsConnType[] m_types;

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
        case ROLE_EDIT_RELAY:
        case ROLE_EDIT_SMS:
        case ROLE_EDIT_BT:
            factory = LayoutInflater.from(this);
            final View roleEditView
                = factory.inflate( layoutForDlg(id), null );

            dlpos = new DialogInterface.OnClickListener() {
                    public void onClick( DialogInterface dialog, 
                                         int whichButton ) {
                        getRoleSettings();
                    }
                };

            return new AlertDialog.Builder( this )
                // .setIcon(R.drawable.alert_dialog_icon)
                .setTitle(titleForDlg(id))
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
        case ROLE_EDIT_RELAY:
        case ROLE_EDIT_SMS:
        case ROLE_EDIT_BT:
            setRoleSettings();
            break;
        }
        super.onPrepareDialog( id, dialog );
    }

    private void setPlayerSettings()
    {
        // Hide remote option if in standalone mode...
        boolean isServer = 
            CurGameInfo.DeviceRole.SERVER_ISSERVER == curRole();

        LocalPlayer lp = m_gi.players[m_whichPlayer];
        Utils.setText( m_curDialog, R.id.player_name_edit, lp.name );

        CheckBox check = (CheckBox)
            m_curDialog.findViewById( R.id.remote_check );
        if ( isServer ) {
            CompoundButton.OnCheckedChangeListener lstnr =
                new CompoundButton.OnCheckedChangeListener() {
                    public void onCheckedChanged( CompoundButton buttonView, 
                                                  boolean checked ) {
                        View view
                            = m_curDialog.findViewById( R.id.local_player_set );
                        view.setVisibility( checked ? View.GONE : View.VISIBLE );
                    }
                };
            check.setOnCheckedChangeListener( lstnr );
        } else {
            check.setVisibility( View.GONE );
        }

        check = (CheckBox)m_curDialog.findViewById( R.id.robot_check );
        CompoundButton.OnCheckedChangeListener lstnr =
            new CompoundButton.OnCheckedChangeListener() {
                public void onCheckedChanged( CompoundButton buttonView, 
                                              boolean checked ) {
                    View view = m_curDialog.findViewById( R.id.password_set );
                    view.setVisibility( checked ? View.GONE : View.VISIBLE );
                }
            };
        check.setOnCheckedChangeListener( lstnr );

        Utils.setChecked( m_curDialog, R.id.robot_check, lp.isRobot );
        Utils.setChecked( m_curDialog, R.id.remote_check, ! lp.isLocal );
    }

    private void setRoleSettings()
    {
        switch( m_types[m_connectSpinner.getSelectedItemPosition()] ) {
        case COMMS_CONN_RELAY:
            Utils.setText( m_curDialog, R.id.room_edit, m_car.ip_relay_invite );
            Utils.setText( m_curDialog, R.id.hostname_edit, 
                           m_car.ip_relay_hostName );
            Utils.setInt( m_curDialog, R.id.port_edit, m_car.ip_relay_port );
            break;
        case COMMS_CONN_SMS:
            Utils.setText( m_curDialog, R.id.sms_phone_edit, m_car.sms_phone );
            Utils.logf( "set phone: " + m_car.sms_phone );
            Utils.setInt( m_curDialog, R.id.sms_port_edit, m_car.sms_port );
            break;
        case COMMS_CONN_BT:
        }
    }

    private void getRoleSettings()
    {
        m_car.conType = m_types[ m_connectSpinner.getSelectedItemPosition() ];
        switch ( m_car.conType ) {
        case COMMS_CONN_RELAY:
            m_car.ip_relay_invite = Utils.getText( m_curDialog, R.id.room_edit );
            m_car.ip_relay_hostName = Utils.getText( m_curDialog, 
                                                     R.id.hostname_edit );
            m_car.ip_relay_port = Utils.getInt( m_curDialog, R.id.port_edit );
            break;
        case COMMS_CONN_SMS:
            m_car.sms_phone = Utils.getText( m_curDialog, R.id.sms_phone_edit );
            Utils.logf( "grabbed phone: " + m_car.sms_phone );
            m_car.sms_port = (short)Utils.getInt( m_curDialog, 
                                                  R.id.sms_port_edit );
            break;
        case COMMS_CONN_BT:
            break;
        }
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

        // 1.5 doesn't have SDK_INT.  So parse the string version.
        int sdk_int = 0;
        try {
            sdk_int = Integer.decode( android.os.Build.VERSION.SDK );
        } catch ( Exception ex ) {}
        m_canDoSMS = sdk_int >= android.os.Build.VERSION_CODES.DONUT;

        m_cp = CommonPrefs.get();

        Intent intent = getIntent();
        Uri uri = intent.getData();
        m_path = uri.getPath();
        if ( m_path.charAt(0) == '/' ) {
            m_path = m_path.substring( 1 );
        }

        byte[] stream = Utils.savedGame( this, m_path );
        m_gi = new CurGameInfo( this );
        XwJNI.gi_from_stream( m_gi, stream );
        byte[] dictBytes = Utils.openDict( this, m_gi.dictName );

        int gamePtr = XwJNI.initJNI();
        if ( !XwJNI.game_makeFromStream( gamePtr, stream, JNIUtilsImpl.get(),
                                         m_gi, dictBytes, m_cp ) ) {
             XwJNI.game_makeNewGame( gamePtr, m_gi, JNIUtilsImpl.get(), 
                                     m_cp, dictBytes );
        }

        int curSel = listAvailableDicts( m_gi.dictName );

        m_car = new CommsAddrRec();
        if ( XwJNI.game_hasComms( gamePtr ) ) {
            XwJNI.comms_getAddr( gamePtr, m_car );
        } else {
            XwJNI.comms_getInitialAddr( m_car );
        }
        XwJNI.game_dispose( gamePtr );

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
        adapter.setDropDownViewResource( android.R.layout
                                         .simple_spinner_dropdown_item );
        m_roleSpinner.setAdapter( adapter );
        m_roleSpinner.setSelection( m_gi.serverRole.ordinal() );
        m_roleSpinner.setOnItemSelectedListener(new OnItemSelectedListener() {
                @Override
                public void onItemSelected(AdapterView<?> parentView, 
                                           View selectedItemView, int position, 
                                           long id ) {
                    adjustVisibility( position );
                }

                @Override
                public void onNothingSelected(AdapterView<?> parentView) {
                }
            });

        configConnectSpinner();

        m_phoniesSpinner = (Spinner)findViewById( R.id.phonies_spinner );
        adapter = 
            new ArrayAdapter<String>( this,
                                      android.R.layout.simple_spinner_item,
                                      new String[] {
                                          getString(R.string.phonies_ignore),
                                          getString(R.string.phonies_warn),
                                          getString(R.string.phonies_disallow),
                                      } );
        adapter.setDropDownViewResource( android.R.layout
                                         .simple_spinner_dropdown_item );
        m_phoniesSpinner.setAdapter( adapter );
        m_phoniesSpinner.setSelection( m_gi.phoniesAction.ordinal() );

        Utils.setChecked( this, R.id.hints_allowed, !m_gi.hintsNotAllowed );
        Utils.setInt( this, R.id.timer_minutes_edit, m_gi.gameSeconds/60 );

        CheckBox check = (CheckBox)findViewById( R.id.use_timer );
        CompoundButton.OnCheckedChangeListener lstnr =
            new CompoundButton.OnCheckedChangeListener() {
                public void onCheckedChanged( CompoundButton buttonView, 
                                              boolean checked ) {
                    View view = findViewById( R.id.timer_set );
                    view.setVisibility( checked ? View.VISIBLE : View.GONE );
                }
            };
        check.setOnCheckedChangeListener( lstnr );
        Utils.setChecked( this, R.id.use_timer, m_gi.timerEnabled );

        Utils.setChecked( this, R.id.color_tiles, m_gi.showColors );
        Utils.setChecked( this, R.id.smart_robot, 0 < m_gi.robotSmartness );

        adjustVisibility(-1);
    } // onCreate

    @Override
    protected void onPause()
    {
        saveChanges();
        super.onPause();        // skip this and get a crash :-)
    }

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
            // case R.id.player_list_item_edit:
            //     showDialog( PLAYER_EDIT );
            //     break;
            case R.id.player_list_item_delete:
                changed = m_gi.delete( m_whichPlayer );
                break;
            case R.id.player_list_item_up:
                changed = m_gi.moveUp( m_whichPlayer );
                break;
            case R.id.player_list_item_down:
                changed = m_gi.moveDown( m_whichPlayer );
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

        switch( item.getItemId() ) {
        case R.id.game_config_juggle:
            m_gi.juggle();
            loadPlayers();
            break;
        case R.id.game_config_revert:
            Utils.notImpl( this );
            break;
        default:
            handled = false;
        }
        return handled;
    } // onOptionsItemSelected

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
            int position = m_connectSpinner.getSelectedItemPosition();
            switch ( m_types[ position ] ) {
            case COMMS_CONN_RELAY:
                showDialog( ROLE_EDIT_RELAY );
                break;
            case COMMS_CONN_SMS:
                showDialog( ROLE_EDIT_SMS );
                break;
            case COMMS_CONN_BT:
                showDialog( ROLE_EDIT_BT );
                break;
            }
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
            StringBuffer sb = new StringBuffer().append( "\n" );
            if ( lp.isLocal ) {
                sb.append( lp.name )
                    .append( lp.isRobot? " (ROBOT)":" (HUMAN)");
            } else {
                sb.append( "<REMOTE>" );
            }
            view.setText( sb.toString() );

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
        startActivity( intent );
    }

    private void configConnectSpinner()
    {
        m_connectSpinner = (Spinner)findViewById( R.id.connect_spinner );
        ArrayAdapter<String> adapter = 
            new ArrayAdapter<String>( this,
                                      android.R.layout.simple_spinner_item,
                                      makeXportStrings() );
        adapter.setDropDownViewResource( android.R.layout
                                         .simple_spinner_dropdown_item );
        m_connectSpinner.setAdapter( adapter );
        m_connectSpinner.setSelection( connTypeToPos( m_car.conType ) );
    } // configConnectSpinner

    private void adjustVisibility( int position )
    {
        int[] ids = { R.id.connect_via_label, 
                      R.id.connect_spinner, 
                      R.id.configure_role };
        if ( position == -1 ) {
            position = m_roleSpinner.getSelectedItemPosition();
        }
        int vis = 0 == position ? View.GONE : View.VISIBLE;

        for ( int id : ids ) {
            View view = findViewById( id );
            view.setVisibility( vis );
        }
    }
    
    private int connTypeToPos( CommsAddrRec.CommsConnType typ )
    {
        switch( typ ) {
        case COMMS_CONN_RELAY:
            return 0;
        case COMMS_CONN_SMS:
            return 1;
        case COMMS_CONN_BT:
            return 2;
        }
        return -1;
    }

    private int layoutForDlg( int id ) 
    {
        switch( id ) {
        case ROLE_EDIT_RELAY:
            return R.layout.role_edit_relay;
        case ROLE_EDIT_SMS:
            return R.layout.role_edit_sms;
        case ROLE_EDIT_BT:
            return R.layout.role_edit_bt;
        }
        Assert.fail();
        return 0;
    }

    private int titleForDlg( int id ) 
    {
        switch( id ) {
        case ROLE_EDIT_RELAY:
            return R.string.tab_relay;
        case ROLE_EDIT_SMS:
            return R.string.tab_sms;
        case ROLE_EDIT_BT:
            return R.string.tab_bluetooth;
        }
        Assert.fail();
        return -1;
    }

    private String[] makeXportStrings()
    {
        ArrayList<String> strings = new ArrayList<String>();
        ArrayList<CommsAddrRec.CommsConnType> types
            = new ArrayList<CommsAddrRec.CommsConnType>();

        strings.add( getString(R.string.tab_relay) );
        types.add( CommsAddrRec.CommsConnType.COMMS_CONN_RELAY );

        if ( m_canDoSMS ) {
            strings.add( getString(R.string.tab_sms) );
            types.add( CommsAddrRec.CommsConnType.COMMS_CONN_SMS );
        }
        if ( m_canDoBT ) {
            strings.add( getString(R.string.tab_bluetooth) );
            types.add( CommsAddrRec.CommsConnType.COMMS_CONN_BT );
        }
        m_types = types.toArray( new CommsAddrRec.CommsConnType[types.size()] );
        return strings.toArray( new String[strings.size()] );
    }

    private CurGameInfo.DeviceRole curRole()
    {
        int position = m_roleSpinner.getSelectedItemPosition();
        return CurGameInfo.DeviceRole.values()[position];
    }

    private void saveChanges()
    {
        m_gi.hintsNotAllowed = !Utils.getChecked( this, R.id.hints_allowed );
        m_gi.timerEnabled = Utils.getChecked(  this, R.id.use_timer );
        m_gi.gameSeconds = 60 * Utils.getInt(  this, R.id.timer_minutes_edit );
        m_gi.showColors = Utils.getChecked( this, R.id.color_tiles );
        m_gi.robotSmartness
            = Utils.getChecked( this, R.id.smart_robot ) ? 1 : 0;

        m_gi.serverRole = curRole();

        int position = m_phoniesSpinner.getSelectedItemPosition();
        m_gi.phoniesAction = CurGameInfo.XWPhoniesChoice.values()[position];

        position = m_connectSpinner.getSelectedItemPosition();
        m_car.conType = m_types[ position ];

        byte[] dictBytes = Utils.openDict( this, m_gi.dictName );
        int gamePtr = XwJNI.initJNI();
        XwJNI.game_makeNewGame( gamePtr, m_gi, JNIUtilsImpl.get(), 
                                m_cp, dictBytes );

        if ( null != m_car ) {
            XwJNI.comms_setAddr( gamePtr, m_car );
        }
        byte[] stream = XwJNI.game_saveToStream( gamePtr, m_gi );
        Utils.saveGame( this, stream, m_path );
        XwJNI.game_dispose( gamePtr );
    }

}
