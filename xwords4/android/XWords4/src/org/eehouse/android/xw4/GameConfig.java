/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
/*
 * Copyright 2009-2010 by Eric House (xwords@eehouse.org).  All
 * rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

package org.eehouse.android.xw4;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import java.util.ArrayList;
import android.view.Gravity;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.app.Dialog;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.view.LayoutInflater;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.view.MenuInflater;
import android.view.KeyEvent;
import android.widget.Spinner;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.ListAdapter;
import android.database.DataSetObserver;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.*;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;

public class GameConfig extends Activity implements View.OnClickListener,
                                                    XWListItem.DeleteCallback {

    private static final int PLAYER_EDIT = 1;
    private static final int ROLE_EDIT_RELAY = 2;
    private static final int ROLE_EDIT_SMS = 3;
    private static final int ROLE_EDIT_BT = 4;
    private static final int FORCE_REMOTE = 5;
    private static final int CONFIRM_CHANGE = 6;

    private Button m_addPlayerButton;
    private Button m_jugglePlayersButton;
    private Button m_configureButton;
    private String m_path;
    private CurGameInfo m_gi;
    private CurGameInfo m_giOrig;
    private int m_whichPlayer;
    private Dialog m_curDialog;
    private Spinner m_roleSpinner;
    private Spinner m_connectSpinner;
    private Spinner m_phoniesSpinner;
    private Spinner m_dictSpinner;
    private String[] m_dicts;
    private int m_browsePosition;
    private LinearLayout m_playerLayout;
    private CommsAddrRec m_carOrig;
    private CommsAddrRec m_car;
    private CommonPrefs m_cp;
    private boolean m_canDoSMS = false;
    private boolean m_canDoBT = false;
    private int m_nMoves = 0;
    private CommsAddrRec.CommsConnType[] m_types;
    private String[] m_connStrings;

    class RemoteChoices extends XWListAdapter {
        public RemoteChoices() { super( GameConfig.this, m_gi.nPlayers ); }

        public Object getItem( int position) { return m_gi.players[position]; }
        public View getView( final int position, View convertView, 
                             ViewGroup parent ) {
            CompoundButton.OnCheckedChangeListener lstnr;
            lstnr = new CompoundButton.OnCheckedChangeListener() {
                    @Override
                    public void onCheckedChanged( CompoundButton buttonView, 
                                                 boolean isChecked )
                    {
                        m_gi.players[position].isLocal = !isChecked;
                    }
                };
            CheckBox cb = new CheckBox( GameConfig.this );
            LocalPlayer lp = m_gi.players[position];
            cb.setText( lp.name );
            cb.setChecked( !lp.isLocal );
            cb.setOnCheckedChangeListener( lstnr );
            return cb;
        }
    }

    @Override
    protected Dialog onCreateDialog( int id )
    {
        Dialog dialog = null;
        LayoutInflater factory;
        DialogInterface.OnClickListener dlpos;
        AlertDialog.Builder ab;

        switch (id) {
        case PLAYER_EDIT:
            factory = LayoutInflater.from(this);
            final View playerEditView
                = factory.inflate( R.layout.player_edit, null );

            dialog = new AlertDialog.Builder( this )
                .setTitle(R.string.player_edit_title)
                .setView(playerEditView)
                .setPositiveButton( R.string.button_ok,
                                    new DialogInterface.OnClickListener() {
                                        public void onClick( DialogInterface dlg, 
                                                             int whichButton ) {
                                            getPlayerSettings();
                                            loadPlayers();
                                        }
                                    })
                .setNegativeButton( R.string.button_cancel, null )
                .create();
            break;
        case ROLE_EDIT_RELAY:
        case ROLE_EDIT_SMS:
        case ROLE_EDIT_BT:
            dialog = new AlertDialog.Builder( this )
                .setTitle(titleForDlg(id))
                .setView( LayoutInflater.from(this)
                          .inflate( layoutForDlg(id), null ))
                .setPositiveButton( R.string.button_ok,
                                    new DialogInterface.OnClickListener() {
                                        public void onClick( DialogInterface dlg, 
                                                             int whichButton ) {
                                            getRoleSettings();
                                        }
                                    })
                .setNegativeButton( R.string.button_cancel, null )
                .create();
            break;

        case FORCE_REMOTE:
            dialog = new AlertDialog.Builder( this )
                .setTitle( R.string.force_title )
                .setView( LayoutInflater.from(this)
                          .inflate( layoutForDlg(id), null ) )
                .setPositiveButton( R.string.button_ok,
                                    new DialogInterface.OnClickListener() {
                                        public void onClick( DialogInterface dlg, 
                                                             int whichButton ) {
                                            loadPlayers();
                                        }
                                    })
                .setNegativeButton( R.string.button_cancel, null )
                .create();
            dialog.setOnDismissListener( new DialogInterface.OnDismissListener() {
                    @Override
                    public void onDismiss( DialogInterface di ) 
                    {
                        if ( m_gi.remoteCount() == 0 ) {
                            // force one to remote -- or make it
                            // standalone???
                            m_gi.players[0].isLocal = false;
                            loadPlayers();
                        }
                    }
                });
            break;
        case CONFIRM_CHANGE:
            dialog = new AlertDialog.Builder( this )
                .setTitle( R.string.confirm_save_title )
                .setMessage( R.string.confirm_save )
                .setPositiveButton( R.string.button_save,
                                    new DialogInterface.OnClickListener() {
                                        public void onClick( DialogInterface dlg, 
                                                             int whichButton ) {
                                            applyChanges( true );
                                            finish();
                                        }
                                    })
                .setNegativeButton( R.string.button_discard, 
                                    new DialogInterface.OnClickListener() {
                                        public void onClick( DialogInterface dlg, 
                                                             int whichButton ) {
                                            finish();
                                        }
                                    })
                .create();
            break;
        }
        return dialog;
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
            setRoleHints( id, dialog );
            setRoleSettings();
            break;
        case FORCE_REMOTE:
            ListView listview = (ListView)dialog.findViewById( R.id.players );
            listview.setAdapter( new RemoteChoices() );
            break;
        }
        super.onPrepareDialog( id, dialog );
    }

    private void setPlayerSettings()
    {
        // Hide remote option if in standalone mode...
        boolean isServer = DeviceRole.SERVER_ISSERVER == curRole();

        LocalPlayer lp = m_gi.players[m_whichPlayer];
        Utils.setText( m_curDialog, R.id.player_name_edit, lp.name );
        Utils.setText( m_curDialog, R.id.password_edit, lp.password );

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
            check.setVisibility( View.VISIBLE );
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

    private void setRoleHints( int id, Dialog dialog )
    {
        int[] guestHints = null;
        int[] hostHints = null;
        switch( id ) {
        case ROLE_EDIT_RELAY:
            // Can these be in an array in a resource?
            guestHints = new int[] { R.id.room_edit_hint_guest };
            hostHints = new int[] { R.id.room_edit_hint_host };
            break;
        case ROLE_EDIT_SMS:
        case ROLE_EDIT_BT:
        }

        DeviceRole role = m_gi.serverRole;
        if ( null != guestHints ) {
            for ( int hintID : guestHints ) {
                View view = dialog.findViewById( hintID );
                view.setVisibility( DeviceRole.SERVER_ISCLIENT == role ?
                                    View.VISIBLE : View.GONE );
            }
        }
        if ( null != hostHints ) {
            for ( int hintID : hostHints ) {
                View view = dialog.findViewById( hintID );
                view.setVisibility( DeviceRole.SERVER_ISSERVER == role ?
                                    View.VISIBLE : View.GONE );
            }
        }
    }

    private void setRoleSettings()
    {
        switch( m_types[m_connectSpinner.getSelectedItemPosition()] ) {
        case COMMS_CONN_RELAY:
            Utils.setText( m_curDialog, R.id.room_edit, m_car.ip_relay_invite );
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
        lp.name = Utils.getText( m_curDialog, R.id.player_name_edit );
        lp.password = Utils.getText( m_curDialog, R.id.password_edit );

        lp.isRobot = Utils.getChecked( m_curDialog, R.id.robot_check );
        lp.isLocal = !Utils.getChecked( m_curDialog, R.id.remote_check );
    }

    @Override
    public void onCreate( Bundle savedInstanceState )
    {
        super.onCreate(savedInstanceState);

        // 1.5 doesn't have SDK_INT.  So parse the string version.
        // int sdk_int = 0;
        // try {
        //     sdk_int = Integer.decode( android.os.Build.VERSION.SDK );
        // } catch ( Exception ex ) {}
        // m_canDoSMS = sdk_int >= android.os.Build.VERSION_CODES.DONUT;

        m_cp = CommonPrefs.get( this );

        Intent intent = getIntent();
        Uri uri = intent.getData();
        m_path = uri.getPath();
        if ( m_path.charAt(0) == '/' ) {
            m_path = m_path.substring( 1 );
        }

        int gamePtr = XwJNI.initJNI();
        m_giOrig = new CurGameInfo( this );
        GameUtils.loadMakeGame( this, gamePtr, m_giOrig, m_path );
        m_nMoves = XwJNI.model_getNMoves( gamePtr );
        m_giOrig.setInProgress( 0 < m_nMoves );
        m_gi = new CurGameInfo( m_giOrig );

        int curSel = listAvailableDicts( m_gi.dictName );

        m_carOrig = new CommsAddrRec( this );
        if ( XwJNI.game_hasComms( gamePtr ) ) {
            XwJNI.comms_getAddr( gamePtr, m_carOrig );
        } else {
            String relayName = CommonPrefs.getDefaultRelayHost( this );
            int relayPort = CommonPrefs.getDefaultRelayPort( this );
            XwJNI.comms_getInitialAddr( m_carOrig, relayName, relayPort );
        }
        XwJNI.game_dispose( gamePtr );

        m_car = new CommsAddrRec( m_carOrig );

        setContentView(R.layout.game_config);

        m_addPlayerButton = (Button)findViewById(R.id.add_player);
        m_addPlayerButton.setOnClickListener( this );
        m_jugglePlayersButton = (Button)findViewById(R.id.juggle_players);
        m_jugglePlayersButton.setOnClickListener( this );
        m_configureButton = (Button)findViewById(R.id.configure_role);
        m_configureButton.setOnClickListener( this );

        m_playerLayout = (LinearLayout)findViewById( R.id.player_list );
        loadPlayers();

        m_dictSpinner = (Spinner)findViewById( R.id.dict_spinner );
        configDictSpinner();

        m_roleSpinner = (Spinner)findViewById( R.id.role_spinner );
        m_roleSpinner.setSelection( m_gi.serverRole.ordinal() );
        m_roleSpinner.setOnItemSelectedListener(new OnItemSelectedListener() {
                @Override
                public void onItemSelected(AdapterView<?> parentView, 
                                           View selectedItemView, int position, 
                                           long id ) {
                    m_gi.setServerRole( DeviceRole.values()[position] );
                    adjustVisibility();
                    loadPlayers();
                }

                @Override
                public void onNothingSelected(AdapterView<?> parentView) {
                }
            });

        configConnectSpinner();

        m_phoniesSpinner = (Spinner)findViewById( R.id.phonies_spinner );
        m_phoniesSpinner.setSelection( m_gi.phoniesAction.ordinal() );

        Utils.setChecked( this, R.id.hints_allowed, !m_gi.hintsNotAllowed );
        Utils.setInt( this, R.id.timer_minutes_edit, 
                      m_gi.gameSeconds/60/m_gi.nPlayers );

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

        Utils.setChecked( this, R.id.smart_robot, 0 < m_gi.robotSmartness );

        adjustVisibility();

        String fmt = getString( R.string.title_game_configf );
        setTitle( String.format( fmt, GameUtils.gameName( this, m_path ) ) );
    } // onCreate

    // DeleteCallback interface
    public void deleteCalled( int myPosition )
    {
        if ( m_gi.delete( myPosition ) ) {
            loadPlayers();
        }
    }

    public void onClick( View view ) 
    {
        if ( m_addPlayerButton == view ) {
            int curIndex = m_gi.nPlayers;
            if ( curIndex < CurGameInfo.MAX_NUM_PLAYERS ) {
                m_gi.addPlayer(); // ups nPlayers
                loadPlayers();
            }
        } else if ( m_jugglePlayersButton == view ) {
            m_gi.juggle();
            loadPlayers();
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

    @Override
    public boolean onKeyDown( int keyCode, KeyEvent event )
    {
        boolean consumed = false;
        if ( keyCode == KeyEvent.KEYCODE_BACK ) {
            saveChanges();
            if ( 0 >= m_nMoves ) { // no confirm needed 
                applyChanges( true );
            } else if ( m_giOrig.changesMatter(m_gi) 
                        || m_carOrig.changesMatter(m_car) ) {
                showDialog( CONFIRM_CHANGE );
                consumed = true; // don't dismiss activity yet!
            } else {
                applyChanges( false );
            }
        }

        return consumed || super.onKeyDown( keyCode, event );
    }

    @Override
    protected void onResume()
    {
        configDictSpinner();
        super.onResume();
    }

    private void loadPlayers()
    {
        m_playerLayout.removeAllViews();

        String[] names = m_gi.visibleNames( this );
        LayoutInflater factory = LayoutInflater.from(this);
        for ( int ii = 0; ii < names.length; ++ii ) {

            final XWListItem view
                = (XWListItem)factory.inflate( R.layout.list_item, null );
            view.setPosition( ii );
            view.setText( names[ii] );
            view.setGravity( Gravity.CENTER );
            // only enable delete if one will remain
            if ( 1 < names.length ) {
                view.setDeleteCallback( this );
            }

            view.setOnClickListener( new View.OnClickListener() {
                    @Override
                    public void onClick( View view ) {
                        m_whichPlayer = ((XWListItem)view).getPosition();
                        showDialog( PLAYER_EDIT );
                    }
                } );
            m_playerLayout.addView( view );

            View divider = factory.inflate( R.layout.divider_view, null );
            divider.setVisibility( View.VISIBLE );
            m_playerLayout.addView( divider );
        }

        m_addPlayerButton
            .setVisibility( names.length >= CurGameInfo.MAX_NUM_PLAYERS?
                            View.GONE : View.VISIBLE );
        m_jugglePlayersButton
            .setVisibility( names.length <= 1 ?
                            View.GONE : View.VISIBLE );

        if ( DeviceRole.SERVER_ISSERVER == m_gi.serverRole
             && 0 == m_gi.remoteCount() ) {
            showDialog( FORCE_REMOTE );
        }
    } // loadPlayers

    private int listAvailableDicts( String curDict )
    {
        int curSel = -1;

        String[] list = GameUtils.dictList( this );

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

    private void configDictSpinner()
    {
        int curSel = listAvailableDicts( m_gi.dictName );

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
                                       View selectedItemView, 
                                       int position, long id ) {
                if ( position == m_browsePosition ) {
                    startActivity( Utils.mkDownloadActivity(GameConfig.this) );
                } else {
                    m_gi.dictName = m_dicts[position];
                    Utils.logf( "assigned dictName: " + m_gi.dictName );
                }
            }

            @Override
            public void onNothingSelected(AdapterView<?> parentView) {}
            });
    }

    private void configConnectSpinner()
    {
        m_connectSpinner = (Spinner)findViewById( R.id.connect_spinner );
        m_connStrings = makeXportStrings();
        ArrayAdapter<String> adapter = 
            new ArrayAdapter<String>( this,
                                      android.R.layout.simple_spinner_item,
                                      m_connStrings );
        adapter.setDropDownViewResource( android.R.layout
                                         .simple_spinner_dropdown_item );
        m_connectSpinner.setAdapter( adapter );
        m_connectSpinner.setSelection( connTypeToPos( m_car.conType ) );
        AdapterView.OnItemSelectedListener
            lstnr = new AdapterView.OnItemSelectedListener() {
                    @Override
                    public void onItemSelected(AdapterView<?> parentView, 
                                               View selectedItemView, 
                                               int position, 
                                               long id ) 
                    {
                        String fmt = getString( R.string.configure_rolef );
                        m_configureButton
                            .setText( String.format( fmt, 
                                                     m_connStrings[position] ));
                    }

                    @Override
                    public void onNothingSelected(AdapterView<?> parentView) 
                    {
                    }
                };
        m_connectSpinner.setOnItemSelectedListener( lstnr );

    } // configConnectSpinner

    private void adjustVisibility()
    {
        // compiler insists these be initialized, so set 'em for
        // SERVER_STANDALONE
        int vis = View.GONE;
        int labelId = R.string.players_label_standalone;

        switch ( curRole() ) {
        case SERVER_ISSERVER:
            vis = View.VISIBLE;
            labelId = R.string.players_label_host;
            break;
        case SERVER_ISCLIENT:
            vis = View.VISIBLE;
            labelId = R.string.players_label_guest;
            break;
        }

        int[] ids = { R.id.connection_label,
                      R.id.connect_spinner, 
                      R.id.configure_role };
        for ( int id : ids ) {
            findViewById( id ).setVisibility( vis );
        }

        ((TextView)findViewById( R.id.players_label )).
            setText( getString(labelId) );
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
        case FORCE_REMOTE:
            return R.layout.force_remote;
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

    private DeviceRole curRole()
    {
        int position = m_roleSpinner.getSelectedItemPosition();
        return DeviceRole.values()[position];
    }

    private void saveChanges()
    {
        m_gi.hintsNotAllowed = !Utils.getChecked( this, R.id.hints_allowed );
        m_gi.timerEnabled = Utils.getChecked(  this, R.id.use_timer );
        m_gi.gameSeconds = 60 * m_gi.nPlayers *
            Utils.getInt(  this, R.id.timer_minutes_edit );
        m_gi.robotSmartness
            = Utils.getChecked( this, R.id.smart_robot ) ? 1 : 0;

        int position = m_phoniesSpinner.getSelectedItemPosition();
        m_gi.phoniesAction = CurGameInfo.XWPhoniesChoice.values()[position];

        m_gi.fixup();

        position = m_connectSpinner.getSelectedItemPosition();
        m_car.conType = m_types[ position ];
    }

    private void applyChanges( boolean forceNew )
    {
        // This should be a separate function, commitChanges() or
        // somesuch.  But: do we have a way to save changes to a gi
        // that don't reset the game, e.g. player name for standalone
        // games?
        byte[] dictBytes = GameUtils.openDict( this, m_gi.dictName );
        int gamePtr = XwJNI.initJNI();
        boolean madeGame = false;

        if ( !forceNew ) {
            byte[] stream = GameUtils.savedGame( this, m_path );
            // Will fail if there's nothing in the stream but a gi.
            madeGame = XwJNI.game_makeFromStream( gamePtr, stream, 
                                                  JNIUtilsImpl.get(),
                                                  new CurGameInfo(this), 
                                                  dictBytes, m_gi.dictName, 
                                                  m_cp );
        }

        if ( forceNew || !madeGame ) {
            m_gi.setInProgress( false );
            m_gi.fixup();
            XwJNI.game_makeNewGame( gamePtr, m_gi, JNIUtilsImpl.get(), 
                                    m_cp, dictBytes, m_gi.dictName );
        }

        if ( null != m_car ) {
            XwJNI.comms_setAddr( gamePtr, m_car );
        }

        GameUtils.saveGame( this, gamePtr, m_gi, m_path );

        GameSummary summary = new GameSummary();
        XwJNI.game_summarize( gamePtr, m_gi.nPlayers, summary );
        DBUtils.saveSummary( m_path, summary );

        XwJNI.game_dispose( gamePtr );
    }

}
