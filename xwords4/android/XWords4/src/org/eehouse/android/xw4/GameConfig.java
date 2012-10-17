/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
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
import android.os.Bundle;
import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import android.view.Gravity;
import android.view.Menu;
import android.view.MenuItem;
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
import android.widget.ImageButton;
import android.view.MenuInflater;
import android.view.KeyEvent;
import android.widget.Spinner;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.ListAdapter;
import android.widget.SpinnerAdapter;
import android.widget.Toast;
import android.database.DataSetObserver;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.*;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;

public class GameConfig extends XWActivity 
    implements View.OnClickListener
               ,XWListItem.DeleteCallback
               ,RefreshNamesTask.NoNameFound {

    private static final int PLAYER_EDIT = DlgDelegate.DIALOG_LAST + 1;
    private static final int FORCE_REMOTE = PLAYER_EDIT + 1;
    private static final int CONFIRM_CHANGE = PLAYER_EDIT + 2;
    private static final int CONFIRM_CHANGE_PLAY = PLAYER_EDIT + 3;
    private static final int NO_NAME_FOUND = PLAYER_EDIT + 4;

    private static final String WHICH_PLAYER = "WHICH_PLAYER";
    private static final int LOCKED_CHANGE_ACTION = 1;

    private CheckBox m_joinPublicCheck;
    private CheckBox m_gameLockedCheck;
    private boolean m_isLocked;
    private LinearLayout m_publicRoomsSet;
    private LinearLayout m_privateRoomsSet;

    private CommsConnType m_conType;
    private Button m_addPlayerButton;
    private Button m_jugglePlayersButton;
    private Button m_playButton;
    private ImageButton m_refreshRoomsButton;
    private View m_connectSetRelay;
    private View m_connectSetSMS;
    private Spinner m_dictSpinner;
    private Spinner m_roomChoose;
    // private Button m_configureButton;
    private long m_rowid;
    private boolean m_forResult;
    private CurGameInfo m_gi;
    private CurGameInfo m_giOrig;
    private GameUtils.GameLock m_gameLock;
    private int m_whichPlayer;
    // private Spinner m_roleSpinner;
    // private Spinner m_connectSpinner;
    private Spinner m_phoniesSpinner;
    private Spinner m_boardsizeSpinner;
    private Spinner m_langSpinner;
    private Spinner m_smartnessSpinner;
    private String m_browseText;
    private LinearLayout m_playerLayout;
    private CommsAddrRec m_carOrig;
    private CommsAddrRec[] m_remoteAddrs;
    private CommsAddrRec m_car;
    private CommonPrefs m_cp;
    // private boolean m_canDoSMS = false;
    // private boolean m_canDoBT = false;
    private boolean m_gameStarted = false;
    private CommsConnType[] m_types;
    private String[] m_connStrings;
    private static final int[] s_disabledWhenLocked = { R.id.juggle_players
                                                        ,R.id.add_player
                                                        ,R.id.lang_spinner
                                                        ,R.id.join_public_room_check
                                                        ,R.id.room_edit
                                                        ,R.id.advertise_new_room_check
                                                        ,R.id.room_spinner
                                                        ,R.id.refresh_button
                                                        ,R.id.hints_allowed
                                                        ,R.id.pick_faceup
                                                        ,R.id.boardsize_spinner
                                                        ,R.id.use_timer
                                                        ,R.id.timer_minutes_edit
                                                        ,R.id.smart_robot
                                                        ,R.id.phonies_spinner
    };

    class RemoteChoices extends XWListAdapter {
        public RemoteChoices() { super( m_gi.nPlayers ); }

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
    protected Dialog onCreateDialog( final int id )
    {
        Dialog dialog = super.onCreateDialog( id );
        if ( null == dialog ) {
            LayoutInflater factory;
            DialogInterface.OnClickListener dlpos;
            AlertDialog.Builder ab;

            switch (id) {
            case PLAYER_EDIT:
                View playerEditView
                    = Utils.inflate( this, R.layout.player_edit );

                dialog = new AlertDialog.Builder( this )
                    .setTitle(R.string.player_edit_title)
                    .setView(playerEditView)
                    .setPositiveButton( R.string.button_ok,
                                        new DialogInterface.OnClickListener() {
                                            public void 
                                                onClick( DialogInterface dlg, 
                                                                 int button ) {
                                                getPlayerSettings( dlg );
                                                loadPlayersList();
                                            }
                                        })
                    .setNegativeButton( R.string.button_cancel, null )
                    .create();
                break;
                // case ROLE_EDIT_RELAY:
                // case ROLE_EDIT_SMS:
                // case ROLE_EDIT_BT:
                //     dialog = new AlertDialog.Builder( this )
                //         .setTitle(titleForDlg(id))
                //         .setView( LayoutInflater.from(this)
                //                   .inflate( layoutForDlg(id), null ))
                //         .setPositiveButton( R.string.button_ok,
                //                             new DialogInterface.OnClickListener() {
                //                                 public void onClick( DialogInterface dlg, 
                //                                                      int whichButton ) {
                //                                     getRoleSettings();
                //                                 }
                //                             })
                //         .setNegativeButton( R.string.button_cancel, null )
                //         .create();
                //     break;

            case FORCE_REMOTE:
                dlpos = new DialogInterface.OnClickListener() {
                        public void onClick( DialogInterface dlg, 
                                             int whichButton ) {
                            loadPlayersList();
                        }
                    };
                dialog = new AlertDialog.Builder( this )
                    .setTitle( R.string.force_title )
                    .setView( Utils.inflate( this, layoutForDlg(id) ) )
                    .setPositiveButton( R.string.button_ok, dlpos )
                    .create();
                DialogInterface.OnDismissListener dismiss = 
                    new DialogInterface.OnDismissListener() {
                        @Override
                        public void onDismiss( DialogInterface di ) 
                        {
                            if ( m_gi.forceRemoteConsistent() ) {
                                Toast.makeText( GameConfig.this, 
                                                R.string.forced_consistent,
                                                Toast.LENGTH_SHORT).show();
                                loadPlayersList();
                            }
                        }
                    };
                dialog.setOnDismissListener( dismiss );
                break;
            case CONFIRM_CHANGE_PLAY:
            case CONFIRM_CHANGE:
                dlpos = new DialogInterface.OnClickListener() {
                        public void onClick( DialogInterface dlg, 
                                             int whichButton ) {
                            applyChanges( true );
                            if ( CONFIRM_CHANGE_PLAY == id ) {
                                launchGame();
                            }
                        }
                    };
                ab = new AlertDialog.Builder( this )
                    .setTitle( R.string.confirm_save_title )
                    .setMessage( R.string.confirm_save )
                    .setPositiveButton( R.string.button_save, dlpos );
                if ( CONFIRM_CHANGE_PLAY == id ) {
                    dlpos = new DialogInterface.OnClickListener() {
                            public void onClick( DialogInterface dlg, 
                                                 int whichButton ) {
                                launchGame();
                            }
                        };
                } else {
                    dlpos = null;
                }
                ab.setNegativeButton( R.string.button_discard, dlpos );
                dialog = ab.create();

                dialog.setOnDismissListener( new DialogInterface.
                                             OnDismissListener() {
                        public void onDismiss( DialogInterface di ) {
                            finish();
                        }
                    });
                break;
            case NO_NAME_FOUND:
                String msg = getString( R.string.no_name_found_f,
                                        m_gi.nPlayers, DictLangCache.
                                        getLangName( this, m_gi.dictLang ) );
                dialog = new AlertDialog.Builder( this )
                    .setPositiveButton( R.string.button_ok, null )
                    // message added below since varies with language etc.
                    .setMessage( msg )
                    .create();
                break;
            }
        }
        return dialog;
    } // onCreateDialog

    @Override
    protected void onPrepareDialog( int id, Dialog dialog )
    { 
        switch ( id ) {
        case PLAYER_EDIT:
            setPlayerSettings( dialog );
            break;
        // case ROLE_EDIT_RELAY:
        // case ROLE_EDIT_SMS:
        // case ROLE_EDIT_BT:
        //     setRoleHints( id, dialog );
        //     setRoleSettings();
        //     break;
        case FORCE_REMOTE:
            ListView listview = (ListView)dialog.findViewById( R.id.players );
            listview.setAdapter( new RemoteChoices() );
            break;
        }
        super.onPrepareDialog( id, dialog );
    }

    private void setPlayerSettings( final Dialog dialog )
    {
        boolean isServer = ! localOnlyGame();

        // Independent of other hide/show logic, these guys are
        // information-only if the game's locked.  (Except that in a
        // local game you can always toggle a player's robot state.)
        Utils.setEnabled( dialog, R.id.remote_check, !m_isLocked );
        Utils.setEnabled( dialog, R.id.player_name_edit, !m_isLocked );
        Utils.setEnabled( dialog, R.id.robot_check, !m_isLocked || !isServer );

        // Hide remote option if in standalone mode...
        LocalPlayer lp = m_gi.players[m_whichPlayer];
        Utils.setText( dialog, R.id.player_name_edit, lp.name );
        Utils.setText( dialog, R.id.password_edit, lp.password );

        // Dicts spinner with label
        TextView dictLabel = (TextView)dialog.findViewById( R.id.dict_label );
        if ( localOnlyGame() ) {
            String langName = DictLangCache.getLangName( this, m_gi.dictLang );
            String label = getString( R.string.dict_lang_labelf, langName );
            dictLabel.setText( label );
        } else {
            dictLabel.setVisibility( View.GONE );
        }
        Spinner dictSpinner = (Spinner)dialog.findViewById( R.id.dict_spinner );
        if ( localOnlyGame() ) {
            configDictSpinner( dictSpinner, m_gi.dictLang, m_gi.dictName(lp) );
        } else {
            dictSpinner.setVisibility( View.GONE );
        }

        final View localSet = dialog.findViewById( R.id.local_player_set );

        CheckBox check = (CheckBox)
            dialog.findViewById( R.id.remote_check );
        if ( isServer ) {
            CompoundButton.OnCheckedChangeListener lstnr =
                new CompoundButton.OnCheckedChangeListener() {
                    public void onCheckedChanged( CompoundButton buttonView, 
                                                  boolean checked ) {
                        localSet.setVisibility( checked ? 
                                                View.GONE : View.VISIBLE );
                    }
                };
            check.setOnCheckedChangeListener( lstnr );
            check.setVisibility( View.VISIBLE );
        } else {
            check.setVisibility( View.GONE );
            localSet.setVisibility( View.VISIBLE );
        }

        check = (CheckBox)dialog.findViewById( R.id.robot_check );
        CompoundButton.OnCheckedChangeListener lstnr =
            new CompoundButton.OnCheckedChangeListener() {
                public void onCheckedChanged( CompoundButton buttonView, 
                                              boolean checked ) {
                    View view = dialog.findViewById( R.id.password_set );
                    view.setVisibility( checked ? View.GONE : View.VISIBLE );
                }
            };
        check.setOnCheckedChangeListener( lstnr );

        Utils.setChecked( dialog, R.id.robot_check, lp.isRobot() );
        Utils.setChecked( dialog, R.id.remote_check, ! lp.isLocal );
    }

    private void getPlayerSettings( DialogInterface di )
    {
        Dialog dialog = (Dialog)di;
        LocalPlayer lp = m_gi.players[m_whichPlayer];
        lp.name = Utils.getText( dialog, R.id.player_name_edit );
        lp.password = Utils.getText( dialog, R.id.password_edit );

        if ( localOnlyGame() ) {
            Spinner spinner =
                (Spinner)((Dialog)di).findViewById( R.id.dict_spinner );
            int position = spinner.getSelectedItemPosition();
            SpinnerAdapter adapter = spinner.getAdapter();

            if ( position < adapter.getCount() ) {
                String name = (String)adapter.getItem( position );
                if ( ! name.equals( m_browseText ) ) {
                    lp.dictName = name;
                }
            }
        }

        lp.setIsRobot( Utils.getChecked( dialog, R.id.robot_check ) );
        lp.isLocal = !Utils.getChecked( dialog, R.id.remote_check );
    }

    @Override
    public void onCreate( Bundle savedInstanceState )
    {
        super.onCreate(savedInstanceState);
        getBundledData( savedInstanceState );

        // 1.5 doesn't have SDK_INT.  So parse the string version.
        // int sdk_int = 0;
        // try {
        //     sdk_int = Integer.decode( android.os.Build.VERSION.SDK );
        // } catch ( Exception ex ) {}
        // m_canDoSMS = sdk_int >= android.os.Build.VERSION_CODES.DONUT;
        m_browseText = getString( R.string.download_dicts );
        DictLangCache.setLast( m_browseText );

        m_cp = CommonPrefs.get( this );

        Intent intent = getIntent();
        m_rowid = intent.getLongExtra( GameUtils.INTENT_KEY_ROWID, -1 );
        m_forResult = intent.getBooleanExtra( GameUtils.INTENT_FORRESULT_ROWID, 
                                              false );

        setContentView(R.layout.game_config);

        m_connectSetRelay = findViewById(R.id.connect_set_relay);
        m_connectSetSMS = findViewById(R.id.connect_set_sms);
        if ( !XWApp.SMSSUPPORTED ) {
            m_connectSetSMS.setVisibility( View.GONE );
        }

        m_addPlayerButton = (Button)findViewById(R.id.add_player);
        m_addPlayerButton.setOnClickListener( this );
        m_jugglePlayersButton = (Button)findViewById(R.id.juggle_players);
        m_jugglePlayersButton.setOnClickListener( this );
        m_playButton = (Button)findViewById( R.id.play_button );
        m_playButton.setOnClickListener( this );

        m_playerLayout = (LinearLayout)findViewById( R.id.player_list );
        m_langSpinner = (Spinner)findViewById( R.id.lang_spinner );
        m_phoniesSpinner = (Spinner)findViewById( R.id.phonies_spinner );
        m_boardsizeSpinner = (Spinner)findViewById( R.id.boardsize_spinner );
        m_smartnessSpinner = (Spinner)findViewById( R.id.smart_robot );
    } // onCreate

    @Override
    protected void onStart()
    {
        super.onStart();
        loadGame();
    }

    @Override
    protected void onResume()
    {
        super.onResume();
        loadGame();
    }

    @Override
    protected void onPause()
    {
        if ( null != m_gameLock ) {
            m_gameLock.unlock();
            m_gameLock = null;
        }
        m_giOrig = null;        // flag for onStart and onResume
        super.onPause();
    }

    @Override
    protected void onSaveInstanceState( Bundle outState ) 
    {
        super.onSaveInstanceState( outState );
        outState.putInt( WHICH_PLAYER, m_whichPlayer );
    }

    private void loadGame()
    {
        if ( null == m_giOrig ) {
            m_giOrig = new CurGameInfo( this );

            // Lock in case we're going to config.  We *could* re-get the
            // lock once the user decides to make changes.  PENDING.
            m_gameLock = new GameUtils.GameLock( m_rowid, true ).lock();
            int gamePtr = GameUtils.loadMakeGame( this, m_giOrig, m_gameLock );
            if ( 0 == gamePtr ) {
                showDictGoneFinish();
            } else {
                m_gameStarted = XwJNI.model_getNMoves( gamePtr ) > 0
                    || XwJNI.comms_isConnected( gamePtr );

                if ( m_gameStarted ) {
                    if ( null == m_gameLockedCheck ) {
                        m_gameLockedCheck = 
                            (CheckBox)findViewById( R.id.game_locked_check );
                        m_gameLockedCheck.setVisibility( View.VISIBLE );
                        m_gameLockedCheck.setChecked( true );
                        m_gameLockedCheck.setOnClickListener( this );
                    }
                    handleLockedChange();
                }

                if ( null == m_gi ) {
                    m_gi = new CurGameInfo( this, m_giOrig );
                }

                m_carOrig = new CommsAddrRec();
                if ( XwJNI.game_hasComms( gamePtr ) ) {
                    XwJNI.comms_getAddr( gamePtr, m_carOrig );
                    m_remoteAddrs = XwJNI.comms_getAddrs( gamePtr );
                } else if (DeviceRole.SERVER_STANDALONE != m_giOrig.serverRole){
                    String relayName = XWPrefs.getDefaultRelayHost( this );
                    int relayPort = XWPrefs.getDefaultRelayPort( this );
                    XwJNI.comms_getInitialAddr( m_carOrig, relayName, relayPort );
                }
                m_conType = m_carOrig.conType;
                XwJNI.game_dispose( gamePtr );

                m_car = new CommsAddrRec( m_carOrig );

                setTitle();

                TextView label = (TextView)findViewById( R.id.lang_separator );
                label.setText( getString( localOnlyGame() ? R.string.lang_label
                                          : R.string.langdict_label ) );

                m_dictSpinner = (Spinner)findViewById( R.id.dict_spinner );
                if ( localOnlyGame() ) {
                    m_dictSpinner.setVisibility( View.GONE );
                    m_dictSpinner = null;
                }

                if ( m_conType.equals( CommsConnType.COMMS_CONN_RELAY ) ) {
                    m_joinPublicCheck = 
                        (CheckBox)findViewById(R.id.join_public_room_check);
                    m_joinPublicCheck.setOnClickListener( this );
                    m_joinPublicCheck.setChecked( m_car.ip_relay_seeksPublicRoom );
                    Utils.setChecked( this, R.id.advertise_new_room_check, 
                                      m_car.ip_relay_advertiseRoom );
                    m_publicRoomsSet = 
                        (LinearLayout)findViewById(R.id.public_rooms_set );
                    m_privateRoomsSet = 
                        (LinearLayout)findViewById(R.id.private_rooms_set );

                    Utils.setText( this, R.id.room_edit, m_car.ip_relay_invite );
        
                    m_roomChoose = (Spinner)findViewById( R.id.room_spinner );

                    m_refreshRoomsButton = 
                        (ImageButton)findViewById( R.id.refresh_button );
                    m_refreshRoomsButton.setOnClickListener( this );

                    adjustConnectStuff();
                }

                loadPlayersList();
                configLangSpinner();

                loadPhones();

                m_phoniesSpinner.setSelection( m_gi.phoniesAction.ordinal() );

                setSmartnessSpinner();

                Utils.setChecked( this, R.id.hints_allowed, 
                                  !m_gi.hintsNotAllowed );
                Utils.setChecked( this, R.id.pick_faceup, 
                                  m_gi.allowPickTiles );
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

                setBoardsizeSpinner();
            }
        }
    } // loadGame

    private void getBundledData( Bundle bundle )
    {
        if ( null != bundle ) {
            m_whichPlayer = bundle.getInt( WHICH_PLAYER );
        }
    }

    // DeleteCallback interface
    public void deleteCalled( XWListItem item )
    {
        if ( m_gi.delete( item.getPosition() ) ) {
            loadPlayersList();
        }
    }

    // NoNameFound interface
    public void NoNameFound()
    {
        showDialog( NO_NAME_FOUND );
    }

    @Override
    public void dlgButtonClicked( int id, int button )
    {
        switch( id ) {
        case LOCKED_CHANGE_ACTION:
            if ( AlertDialog.BUTTON_POSITIVE == button ) {
                handleLockedChange();
            }
            break;
        default:
            Assert.fail();
        }
    }

    public void onClick( View view ) 
    {
        if ( null == m_gameLock ) {
            // do nothing; we're on the way out
        } else if ( m_addPlayerButton == view ) {
            int curIndex = m_gi.nPlayers;
            if ( curIndex < CurGameInfo.MAX_NUM_PLAYERS ) {
                m_gi.addPlayer(); // ups nPlayers
                loadPlayersList();
            }
        } else if ( m_jugglePlayersButton == view ) {
            m_gi.juggle();
            loadPlayersList();
        } else if ( m_joinPublicCheck == view ) {
            adjustConnectStuff();
        } else if ( m_gameLockedCheck == view ) {
            showNotAgainDlgThen( R.string.not_again_unlock, 
                                 R.string.key_notagain_unlock,
                                 LOCKED_CHANGE_ACTION );
        } else if ( m_refreshRoomsButton == view ) {
            refreshNames();
        } else if ( m_playButton == view ) {
            // Launch BoardActivity for m_name, but ONLY IF user
            // confirms any changes required.  So we either launch
            // from here if there's no confirmation needed, or launch
            // a new dialog whose OK button does the same thing.
            saveChanges();
            if ( m_forResult ) {
                applyChanges( true );
                setResult( Activity.RESULT_OK, null );
                finish();
            } else if ( !m_gameStarted ) { // no confirm needed 
                applyChanges( true );
                launchGame();
            } else if ( m_giOrig.changesMatter(m_gi) 
                        || m_carOrig.changesMatter(m_car) ) {
                showDialog( CONFIRM_CHANGE_PLAY );
            } else {
                applyChanges( false );
                launchGame();
            }

        } else {
            DbgUtils.logf( "unknown v: " + view.toString() );
        }
    } // onClick

    @Override
    public boolean onKeyDown( int keyCode, KeyEvent event )
    {
        boolean consumed = false;
        if ( null == m_gameLock ) {
            // Do nothing; we're on our way out
        } else if ( keyCode == KeyEvent.KEYCODE_BACK ) {
            saveChanges();
            if ( !m_gameStarted ) { // no confirm needed 
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

    private void loadPlayersList()
    {
        m_playerLayout.removeAllViews();

        String[] names = m_gi.visibleNames( false );
        // only enable delete if one will remain (or two if networked)
        boolean canDelete = names.length > 2
            || (localOnlyGame() && names.length > 1);
        LayoutInflater factory = LayoutInflater.from(this);
        View.OnClickListener lstnr = new View.OnClickListener() {
                @Override
                public void onClick( View view ) {
                    m_whichPlayer = ((XWListItem)view).getPosition();
                    showDialog( PLAYER_EDIT );
                }
            };
 
        boolean localGame = localOnlyGame();
        for ( int ii = 0; ii < names.length; ++ii ) {
            final XWListItem view
                = (XWListItem)factory.inflate( R.layout.list_item, null );
            view.setPosition( ii );
            view.setText( names[ii] );
            if ( localGame && m_gi.players[ii].isLocal ) {
                view.setComment( m_gi.dictName(ii) );
            }
            if ( canDelete ) {
                view.setDeleteCallback( this );
            }

            view.setOnClickListener( lstnr );
            m_playerLayout.addView( view );

            View divider = factory.inflate( R.layout.divider_view, null );
            m_playerLayout.addView( divider );
        }

        m_addPlayerButton
            .setVisibility( names.length >= CurGameInfo.MAX_NUM_PLAYERS?
                            View.GONE : View.VISIBLE );
        m_jugglePlayersButton
            .setVisibility( names.length <= 1 ?
                            View.GONE : View.VISIBLE );
        
        m_connectSetRelay.
            setVisibility( m_conType == CommsConnType.COMMS_CONN_RELAY ?
                           View.VISIBLE : View.GONE );
        if ( XWApp.SMSSUPPORTED ) {
            m_connectSetSMS.
                setVisibility( m_conType == CommsConnType.COMMS_CONN_SMS ?
                               View.VISIBLE : View.GONE );
        }

        if ( ! localOnlyGame()
             && ((0 == m_gi.remoteCount() )
                 || (m_gi.nPlayers == m_gi.remoteCount()) ) ) {
            showDialog( FORCE_REMOTE );
        }
        adjustPlayersLabel();
    } // loadPlayersList

    private void configDictSpinner( Spinner dictsSpinner, int lang,
                                    String curDict )
    {
        String langName = DictLangCache.getLangName( this, lang );
        dictsSpinner.setPrompt( getString( R.string.dicts_list_promptf, 
                                           langName ) );

        OnItemSelectedListener onSel = 
            new OnItemSelectedListener() {
                @Override
                public void onItemSelected( AdapterView<?> parentView, 
                                            View selectedItemView, 
                                            int position, long id ) {
                    String chosen = 
                        (String)parentView.getItemAtPosition( position );

                    if ( chosen.equals( m_browseText ) ) {
                        DictsActivity.launchAndDownload( GameConfig.this, 
                                                         m_gi.dictLang );
                    }
                }

                @Override
                public void onNothingSelected(AdapterView<?> parentView) {}
            };

        ArrayAdapter<String> adapter = 
            DictLangCache.getDictsAdapter( this, lang );

        configSpinnerWDownload( dictsSpinner, adapter, onSel, curDict );
    }

    private void configLangSpinner()
    {
        OnItemSelectedListener onSel = 
            new OnItemSelectedListener() {
                @Override
                public void onItemSelected(AdapterView<?> parentView, 
                                           View selectedItemView, 
                                           int position, long id ) {
                    String chosen = 
                        (String)parentView.getItemAtPosition( position );
                    if ( chosen.equals( m_browseText ) ) {
                        DictsActivity.launchAndDownload( GameConfig.this, 0 );
                    } else {
                        m_gi.setLang( DictLangCache.
                                      getLangLangCode( GameConfig.this, 
                                                       chosen ) );
                        loadPlayersList();
                        if ( null != m_dictSpinner ) {
                            configDictSpinner( m_dictSpinner, m_gi.dictLang, 
                                               m_gi.dictName );
                        }
                    }
                }

                @Override
                    public void onNothingSelected(AdapterView<?> parentView) {}
            };

        ArrayAdapter<String> adapter = 
            DictLangCache.getLangsAdapter( this );
        String lang = DictLangCache.getLangName( this, m_gi.dictLang );
        configSpinnerWDownload( m_langSpinner, adapter, onSel, lang );
    }

    private void configSpinnerWDownload( Spinner spinner, 
                                         ArrayAdapter<String> adapter,
                                         OnItemSelectedListener onSel,
                                         String curSel )
    {
        int resID = android.R.layout.simple_spinner_dropdown_item;
        adapter.setDropDownViewResource( resID );
        spinner.setAdapter( adapter );
        spinner.setOnItemSelectedListener( onSel );
        setSpinnerSelection( spinner, adapter, curSel );
    }

    private void setSpinnerSelection( Spinner spinner, 
                                      ArrayAdapter<String> adapter,
                                      String sel )
    {
        if ( null != sel ) {
            for ( int ii = 0; ii < adapter.getCount(); ++ii ) {
                if ( sel.equals( adapter.getItem(ii) ) ) {
                    spinner.setSelection( ii );
                    break;
                }
            }
        }
    }

    private void loadPhones()
    {
        if ( XWApp.SMSSUPPORTED && null != m_remoteAddrs ) {
            LinearLayout phoneList = 
                (LinearLayout)findViewById(R.id.sms_phones);
            for ( CommsAddrRec addr : m_remoteAddrs ) {
                XWListItem item = 
                    (XWListItem)Utils.inflate( this, R.layout.list_item );
                item.setText( addr.sms_phone );
                String name = Utils.phoneToContact( this, addr.sms_phone, 
                                                    false );
                item.setComment( name );
                item.setEnabled( false );
                phoneList.addView( item );
            }
        }
    }

    private void setSmartnessSpinner()
    {
        int setting = -1;
        switch ( m_gi.getRobotSmartness() ) {
        case 1:
            setting = 0;
            break;
        case 50:
            setting = 1;
            break;
        case 99:
        case 100:
            setting = 2;
            break;
        default:
            DbgUtils.logf( "setSmartnessSpinner got %d from getRobotSmartness()", 
                           m_gi.getRobotSmartness() );
            Assert.fail();
        }
        m_smartnessSpinner.setSelection( setting );
    }

    private int positionToSize( int position ) {
        switch( position ) {
        case 0: return 15;
        case 1: return 13;
        case 2: return 11;
        default:
            Assert.fail();
        }
        return -1;
    }

    private void setBoardsizeSpinner()
    {
        int size = m_gi.boardSize;
        int selection = 0;
        switch( size ) {
        case 15:
            selection = 0;
            break;
        case 13:
            selection = 1;
            break;
        case 11:
            selection = 2;
            break;
        default:
            Assert.fail();
            break;
        }
        Assert.assertTrue( size == positionToSize(selection) );
        m_boardsizeSpinner.setSelection( selection );
    }

    // private void configConnectSpinner()
    // {
    //     m_connectSpinner = (Spinner)findViewById( R.id.connect_spinner );
    //     m_connStrings = makeXportStrings();
    //     ArrayAdapter<String> adapter = 
    //         new ArrayAdapter<String>( this,
    //                                   android.R.layout.simple_spinner_item,
    //                                   m_connStrings );
    //     adapter.setDropDownViewResource( android.R.layout
    //                                      .simple_spinner_dropdown_item );
    //     m_connectSpinner.setAdapter( adapter );
    //     m_connectSpinner.setSelection( connTypeToPos( m_car.conType ) );
    //     AdapterView.OnItemSelectedListener
    //         lstnr = new AdapterView.OnItemSelectedListener() {
    //                 @Override
    //                 public void onItemSelected(AdapterView<?> parentView, 
    //                                            View selectedItemView, 
    //                                            int position, 
    //                                            long id ) 
    //                 {
    //                     String fmt = getString( R.string.configure_rolef );
    //                     m_configureButton
    //                         .setText( String.format( fmt, 
    //                                                  m_connStrings[position] ));
    //                 }

    //                 @Override
    //                 public void onNothingSelected(AdapterView<?> parentView) 
    //                 {
    //                 }
    //             };
    //     m_connectSpinner.setOnItemSelectedListener( lstnr );

    // } // configConnectSpinner

    private void adjustPlayersLabel()
    {
        DbgUtils.logf( "adjustPlayersLabel()" );
        String label;
        if ( localOnlyGame() ) {
            label = getString( R.string.players_label_standalone );
        } else {
            int remoteCount = m_gi.remoteCount();
            label = getString( R.string.players_label_host,
                               m_gi.nPlayers - remoteCount, 
                               remoteCount );
        }
        ((TextView)findViewById( R.id.players_label )).setText( label );
    }

    private void adjustConnectStuff()
    {
        if ( m_joinPublicCheck.isChecked() ) {
            refreshNames();
            m_privateRoomsSet.setVisibility( View.GONE );
            m_publicRoomsSet.setVisibility( View.VISIBLE );

            // // make the room spinner match the saved value if present
            // String invite = m_car.ip_relay_invite;
            // ArrayAdapter<String> adapter = 
            //     (ArrayAdapter<String>)m_roomChoose.getAdapter();
            // if ( null != adapter ) {
            //     for ( int ii = 0; ii < adapter.getCount(); ++ii ) {
            //         if ( adapter.getItem(ii).equals( invite ) ) {
            //             m_roomChoose.setSelection( ii );
            //             break;
            //         }
            //     }
            // }

        } else {
            m_privateRoomsSet.setVisibility( View.VISIBLE );
            m_publicRoomsSet.setVisibility( View.GONE );
        }
    }

    // User's toggling whether everything's locked.  That should mean
    // we enable/disable a bunch of widgits.  And if we're going from
    // unlocked to locked we need to confirm that everything can be
    // reverted.
    private void handleLockedChange()
    {
        boolean locking = m_gameLockedCheck.isChecked();
        m_isLocked = locking;
        for ( int id : s_disabledWhenLocked ) {
            View view = findViewById( id );
            view.setEnabled( !m_isLocked );
        }
    }
    
    private int layoutForDlg( int id ) 
    {
        switch( id ) {
        // case ROLE_EDIT_RELAY:
        //     return R.layout.role_edit_relay;
        // case ROLE_EDIT_SMS:
        //     return R.layout.role_edit_sms;
        // case ROLE_EDIT_BT:
        //     return R.layout.role_edit_bt;
        case FORCE_REMOTE:
            return R.layout.force_remote;
        }
        Assert.fail();
        return 0;
    }

    // private int titleForDlg( int id ) 
    // {
    //     switch( id ) {
    //     // case ROLE_EDIT_RELAY:
    //     //     return R.string.tab_relay;
    //     // case ROLE_EDIT_SMS:
    //     //     return R.string.tab_sms;
    //     // case ROLE_EDIT_BT:
    //     //     return R.string.tab_bluetooth;
    //     }
    //     Assert.fail();
    //     return -1;
    // }

    // private String[] makeXportStrings()
    // {
    //     ArrayList<String> strings = new ArrayList<String>();
    //     ArrayList<CommsAddrRec.CommsConnType> types
    //         = new ArrayList<CommsAddrRec.CommsConnType>();

    //     strings.add( getString(R.string.tab_relay) );
    //     types.add( CommsAddrRec.CommsConnType.COMMS_CONN_RELAY );

    //     if ( m_canDoSMS ) {
    //         strings.add( getString(R.string.tab_sms) );
    //         types.add( CommsAddrRec.CommsConnType.COMMS_CONN_SMS );
    //     }
    //     if ( m_canDoBT ) {
    //         strings.add( getString(R.string.tab_bluetooth) );
    //         types.add( CommsAddrRec.CommsConnType.COMMS_CONN_BT );
    //     }
    //     m_types = types.toArray( new CommsAddrRec.CommsConnType[types.size()] );
    //     return strings.toArray( new String[strings.size()] );
    // }

    private void saveChanges()
    {
        if ( !localOnlyGame() ) {
            Spinner dictSpinner = (Spinner)findViewById( R.id.dict_spinner );
            String name = (String)dictSpinner.getSelectedItem();
            if ( !m_browseText.equals( name ) ) {
                m_gi.dictName = name;
            }
        }

        m_gi.hintsNotAllowed = !Utils.getChecked( this, R.id.hints_allowed );
        m_gi.allowPickTiles = Utils.getChecked( this, R.id.pick_faceup );
        m_gi.timerEnabled = Utils.getChecked(  this, R.id.use_timer );
        m_gi.gameSeconds = 60 * m_gi.nPlayers *
            Utils.getInt(  this, R.id.timer_minutes_edit );

        int position = m_phoniesSpinner.getSelectedItemPosition();
        m_gi.phoniesAction = CurGameInfo.XWPhoniesChoice.values()[position];

        position = m_smartnessSpinner.getSelectedItemPosition();
        m_gi.setRobotSmartness(position * 49 + 1);

        position = m_boardsizeSpinner.getSelectedItemPosition();
        m_gi.boardSize = positionToSize( position );

        switch( m_conType ) {
        case COMMS_CONN_RELAY:
            m_car.ip_relay_seeksPublicRoom = m_joinPublicCheck.isChecked();
            DbgUtils.logf( "ip_relay_seeksPublicRoom: %b", 
                           m_car.ip_relay_seeksPublicRoom );
            m_car.ip_relay_advertiseRoom = 
                Utils.getChecked( this, R.id.advertise_new_room_check );
            if ( m_car.ip_relay_seeksPublicRoom ) {
                SpinnerAdapter adapter = m_roomChoose.getAdapter();
                if ( null != adapter ) {
                    int pos = m_roomChoose.getSelectedItemPosition();
                    if ( pos >= 0 && pos < adapter.getCount() ) {
                        m_car.ip_relay_invite = (String)adapter.getItem(pos);
                    }
                }
            } else {
                m_car.ip_relay_invite = 
                    Utils.getText( this, R.id.room_edit ).trim();
            }
            break;
            // nothing to save for BT yet
        }

        m_car.conType = m_conType;
    } // saveChanges

    private void applyChanges( boolean forceNew )
    {
        GameUtils.applyChanges( this, m_gi, m_car, m_gameLock, forceNew );
    }

    private void launchGame()
    {
        if ( m_conType == CommsConnType.COMMS_CONN_RELAY 
             && 0 == m_car.ip_relay_invite.length() ) {
            showOKOnlyDialog( R.string.no_empty_rooms );            
        } else {
            m_gameLock.unlock();
            m_gameLock = null;
            GameUtils.launchGameAndFinish( this, m_rowid );
        }
    }

    private void refreshNames()
    {
        if ( !m_isLocked ) {
            new RefreshNamesTask( this, this, m_gi.dictLang, 
                                  m_gi.nPlayers, m_roomChoose ).execute();
        }
    }

    private void setTitle()
    {
        int strID;
        switch( m_conType ) {
        case COMMS_CONN_RELAY:
            strID = R.string.title_gamenet_configf;
            break;
        case COMMS_CONN_BT:
            strID = R.string.title_gamebt_configf;
            break;
        default:
            strID = R.string.title_game_configf;
            break;
        }
        setTitle( Utils.format( this, strID, 
                                GameUtils.getName( this, m_rowid ) ) );
    }

    private boolean localOnlyGame()
    {
        return m_conType == CommsConnType.COMMS_CONN_NONE;
    }

}
