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

public class GameConfig extends XWActivity 
    implements View.OnClickListener
               ,XWListItem.DeleteCallback
               ,RefreshNamesTask.NoNameFound {

    private static final int PLAYER_EDIT = DlgDelegate.DIALOG_LAST + 1;
    private static final int FORCE_REMOTE = PLAYER_EDIT + 1;
    private static final int CONFIRM_CHANGE = PLAYER_EDIT + 2;
    private static final int CONFIRM_CHANGE_PLAY = PLAYER_EDIT + 3;
    private static final int NO_NAME_FOUND = PLAYER_EDIT + 4;

    private CheckBox m_joinPublicCheck;
    private CheckBox m_gameLockedCheck;
    private boolean m_isLocked;
    private LinearLayout m_publicRoomsSet;
    private LinearLayout m_privateRoomsSet;

    private boolean m_notNetworkedGame;
    private Button m_addPlayerButton;
    private Button m_jugglePlayersButton;
    private Button m_playButton;
    private ImageButton m_refreshRoomsButton;
    private View m_connectSet;  // really a LinearLayout
    private Spinner m_roomChoose;
    // private Button m_configureButton;
    private String m_path;
    private CurGameInfo m_gi;
    private CurGameInfo m_giOrig;
    private GameUtils.GameLock m_gameLock;
    private int m_whichPlayer;
    // private Spinner m_roleSpinner;
    // private Spinner m_connectSpinner;
    private Spinner m_phoniesSpinner;
    private Spinner m_langSpinner;
    private Spinner m_smartnessSpinner;
    private String m_browseText;
    private LinearLayout m_playerLayout;
    private CommsAddrRec m_carOrig;
    private CommsAddrRec m_car;
    private CommonPrefs m_cp;
    private boolean m_canDoSMS = false;
    private boolean m_canDoBT = false;
    private boolean m_gameStarted = false;
    private CommsAddrRec.CommsConnType[] m_types;
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
                                                        ,R.id.use_timer
                                                        ,R.id.timer_minutes_edit
                                                        ,R.id.smart_robot
                                                        ,R.id.phonies_spinner
    };

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
    protected Dialog onCreateDialog( final int id )
    {
        Dialog dialog = super.onCreateDialog( id );
        if ( null == dialog ) {
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
                                            public void 
                                                onClick( DialogInterface dlg, 
                                                                 int button ) {
                                                getPlayerSettings( dlg );
                                                loadPlayers();
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
                    .create();
                dialog.setOnDismissListener( new DialogInterface.OnDismissListener() {
                        @Override
                            public void onDismiss( DialogInterface di ) 
                        {
                            if ( m_gi.forceRemoteConsistent() ) {
                                Toast.makeText( GameConfig.this, 
                                                R.string.forced_consistent,
                                                Toast.LENGTH_SHORT).show();
                                loadPlayers();
                            }
                        }
                    });
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
                String format = getString( R.string.no_name_found_f );
                String msg = 
                    String.format( format, m_gi.nPlayers, DictLangCache.
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
        // Hide remote option if in standalone mode...
        boolean isServer = !m_notNetworkedGame;
        LocalPlayer lp = m_gi.players[m_whichPlayer];
        Utils.setText( dialog, R.id.player_name_edit, lp.name );
        Utils.setText( dialog, R.id.password_edit, lp.password );

        // Dicts spinner with label
        String langName = DictLangCache.getLangName( this, m_gi.dictLang );
        String label = String.format( getString( R.string.dict_lang_labelf ),
                                      langName );
        TextView text = (TextView)dialog.findViewById( R.id.dict_label );
        text.setText( label );
        configDictSpinner( dialog, lp );

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
        Spinner spinner =
            (Spinner)((Dialog)di).findViewById( R.id.dict_spinner );
        int position = spinner.getSelectedItemPosition();
        lp.dictName = DictLangCache.getHaveLang( this, m_gi.dictLang )[position];
        Utils.logf( "reading name for player %d via position %d: %s", 
                    m_whichPlayer, position, lp.dictName );

        lp.setIsRobot( Utils.getChecked( dialog, R.id.robot_check ) );
        lp.isLocal = !Utils.getChecked( dialog, R.id.remote_check );
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
        m_browseText = getString( R.string.download_dicts );
        DictLangCache.setLast( m_browseText );

        m_cp = CommonPrefs.get( this );

        Intent intent = getIntent();
        Uri uri = intent.getData();
        m_path = uri.getPath();
        if ( m_path.charAt(0) == '/' ) {
            m_path = m_path.substring( 1 );
        }

        setContentView(R.layout.game_config);


        m_connectSet = findViewById(R.id.connect_set);
        m_addPlayerButton = (Button)findViewById(R.id.add_player);
        m_addPlayerButton.setOnClickListener( this );
        m_jugglePlayersButton = (Button)findViewById(R.id.juggle_players);
        m_jugglePlayersButton.setOnClickListener( this );
        m_playButton = (Button)findViewById( R.id.play_button );
        m_playButton.setOnClickListener( this );

        m_playerLayout = (LinearLayout)findViewById( R.id.player_list );
        m_langSpinner = (Spinner)findViewById( R.id.lang_spinner );
        m_phoniesSpinner = (Spinner)findViewById( R.id.phonies_spinner );
        m_smartnessSpinner = (Spinner)findViewById( R.id.smart_robot );

        String fmt = getString( m_notNetworkedGame ?
                                R.string.title_game_configf
                                : R.string.title_gamenet_configf );
        setTitle( String.format( fmt, GameUtils.gameName( this, m_path ) ) );
    } // onCreate

    @Override
    protected void onResume()
    {
        super.onResume();

        int gamePtr = XwJNI.initJNI();
        m_giOrig = new CurGameInfo( this );
        // Lock in case we're going to config.  We *could* re-get the
        // lock once the user decides to make changes.  PENDING.
        m_gameLock = new GameUtils.GameLock( m_path, true ).lock();
        GameUtils.loadMakeGame( this, gamePtr, m_giOrig, m_gameLock );
        m_gameStarted = XwJNI.model_getNMoves( gamePtr ) > 0
            || XwJNI.comms_isConnected( gamePtr );
        m_giOrig.setInProgress( m_gameStarted );

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

        m_gi = new CurGameInfo( this, m_giOrig );

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

        m_notNetworkedGame = DeviceRole.SERVER_STANDALONE == m_gi.serverRole;

        if ( !m_notNetworkedGame ) {
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

        loadPlayers();
        configLangSpinner();

        m_phoniesSpinner.setSelection( m_gi.phoniesAction.ordinal() );

        setSmartnessSpinner();

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
    } // onResume

    @Override
    protected void onPause()
    {
        if ( null != m_gameLock ) {
            m_gameLock.unlock();
            m_gameLock = null;
        }
        super.onPause();
    }

    // DeleteCallback interface
    public void deleteCalled( int myPosition )
    {
        if ( m_gi.delete( myPosition ) ) {
            loadPlayers();
        }
    }

    // NoNameFound interface
    public void NoNameFound()
    {
        showDialog( NO_NAME_FOUND );
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
        } else if ( m_joinPublicCheck == view ) {
            adjustConnectStuff();
        } else if ( m_gameLockedCheck == view ) {
            showNotAgainDlgThen( R.string.not_again_unlock, 
                                 R.string.key_notagain_unlock,
                                 new Runnable() {
                                     public void run() {
                                         handleLockedChange();
                                     }
                                 });
        } else if ( m_refreshRoomsButton == view ) {
            refreshNames();
        } else if ( m_playButton == view ) {
            // Launch BoardActivity for m_path, but ONLY IF user
            // confirms any changes required.  So we either launch
            // from here if there's no confirmation needed, or launch
            // a new dialog whose OK button does the same thing.
            saveChanges();
            if ( !m_gameStarted ) { // no confirm needed 
                applyChanges( true );
                launchGame();
            } else if ( m_giOrig.changesMatter(m_gi) 
                        || (! m_notNetworkedGame
                            && m_carOrig.changesMatter(m_car) ) ) {
                showDialog( CONFIRM_CHANGE_PLAY );
            } else {
                applyChanges( false );
                launchGame();
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
            if ( !m_gameStarted ) { // no confirm needed 
                applyChanges( true );
            } else if ( m_giOrig.changesMatter(m_gi) 
                        || (! m_notNetworkedGame
                            && m_carOrig.changesMatter(m_car) ) ) {
                showDialog( CONFIRM_CHANGE );
                consumed = true; // don't dismiss activity yet!
            } else {
                applyChanges( false );
            }
        }

        return consumed || super.onKeyDown( keyCode, event );
    }

    private void loadPlayers()
    {
        m_playerLayout.removeAllViews();

        String[] names = m_gi.visibleNames();
        // only enable delete if one will remain (or two if networked)
        boolean canDelete = names.length > 2
            || (m_notNetworkedGame && names.length > 1);
        LayoutInflater factory = LayoutInflater.from(this);
        for ( int ii = 0; ii < names.length; ++ii ) {

            final XWListItem view
                = (XWListItem)factory.inflate( R.layout.list_item, null );
            view.setPosition( ii );
            view.setText( names[ii] );
            view.setGravity( Gravity.CENTER );
            if ( canDelete ) {
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
            view.setEnabled( !m_isLocked );

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
        m_connectSet.setVisibility( m_notNetworkedGame?
                                    View.GONE : View.VISIBLE );

        if ( ! m_notNetworkedGame
             && ((0 == m_gi.remoteCount() )
                 || (m_gi.nPlayers == m_gi.remoteCount()) ) ) {
            showDialog( FORCE_REMOTE );
        }
        adjustPlayersLabel();
    } // loadPlayers

    private String[] buildListWithBrowse( String[] input )
    {
        Arrays.sort( input );
        int browsePosn = input.length;
        String[] result = new String[browsePosn+1];
        result[browsePosn] = getString( R.string.download_dicts );
        
        for ( int ii = 0; ii < browsePosn; ++ii ) {
            String lang = input[ii];
            result[ii] = lang;
        }
        return result;
    }

    private void configDictSpinner( final Dialog dialog, final LocalPlayer lp )
    {
        Spinner dictsSpinner = 
            (Spinner)dialog.findViewById( R.id.dict_spinner );

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
                    } else {
                        lp.dictName = chosen;
                    }
                }

                @Override
                public void onNothingSelected(AdapterView<?> parentView) {}
            };

        ArrayAdapter<String> adapter = 
            DictLangCache.getDictsAdapter( this, m_gi.dictLang );
        configSpinnerWDownload( dictsSpinner, adapter, onSel, 
                                m_gi.dictName(lp) );
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
        for ( int ii = 0; ii < adapter.getCount(); ++ii ) {
            if ( sel.equals( adapter.getItem(ii) ) ) {
                spinner.setSelection( ii );
                break;
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
            Utils.logf( "setSmartnessSpinner got %d from getRobotSmartness()", 
                        m_gi.getRobotSmartness() );
            Assert.fail();
        }
        m_smartnessSpinner.setSelection( setting );
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
        Utils.logf( "adjustPlayersLabel()" );
        String label;
        if ( m_notNetworkedGame ) {
            label = getString( R.string.players_label_standalone );
        } else {
            String fmt = getString( R.string.players_label_host );
            int remoteCount = m_gi.remoteCount();
            label = String.format( fmt, m_gi.nPlayers - remoteCount, 
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
        if ( null != m_playerLayout ) {
            for ( int ii = m_playerLayout.getChildCount()-1; ii >= 0; --ii ) {
                View view = m_playerLayout.getChildAt( ii );
                view.setEnabled( !m_isLocked );
            }
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

    private int titleForDlg( int id ) 
    {
        switch( id ) {
        // case ROLE_EDIT_RELAY:
        //     return R.string.tab_relay;
        // case ROLE_EDIT_SMS:
        //     return R.string.tab_sms;
        // case ROLE_EDIT_BT:
        //     return R.string.tab_bluetooth;
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

    private void saveChanges()
    {
        m_gi.hintsNotAllowed = !Utils.getChecked( this, R.id.hints_allowed );
        m_gi.timerEnabled = Utils.getChecked(  this, R.id.use_timer );
        m_gi.gameSeconds = 60 * m_gi.nPlayers *
            Utils.getInt(  this, R.id.timer_minutes_edit );

        int position = m_phoniesSpinner.getSelectedItemPosition();
        m_gi.phoniesAction = CurGameInfo.XWPhoniesChoice.values()[position];

        position = m_smartnessSpinner.getSelectedItemPosition();
        m_gi.setRobotSmartness(position * 49 + 1);

        if ( !m_notNetworkedGame ) {
            m_car.ip_relay_seeksPublicRoom = m_joinPublicCheck.isChecked();
            Utils.logf( "ip_relay_seeksPublicRoom: %s", 
                        m_car.ip_relay_seeksPublicRoom?"true":"false" );
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
        }

        // position = m_connectSpinner.getSelectedItemPosition();
        // m_car.conType = m_types[ position ];

        m_car.conType = m_notNetworkedGame
            ? CommsAddrRec.CommsConnType.COMMS_CONN_NONE
            : CommsAddrRec.CommsConnType.COMMS_CONN_RELAY;
    } // saveChanges

    private void applyChanges( boolean forceNew )
    {
        GameUtils.applyChanges( this, m_gi, m_car, m_gameLock, forceNew );
    }

    private void launchGame()
    {
        if ( m_notNetworkedGame || m_car.ip_relay_invite.length() > 0 ) {
            m_gameLock.unlock();
            m_gameLock = null;
            GameUtils.launchGameAndFinish( this, m_path );
        } else {
            showOKOnlyDialog( R.string.no_empty_rooms );            
        }
    }

    private void refreshNames()
    {
        new RefreshNamesTask( this, this, m_gi.dictLang, 
                              m_gi.nPlayers, m_roomChoose ).execute();
    }

}
