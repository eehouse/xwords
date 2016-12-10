/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009 - 2016 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.Spinner;
import android.widget.SpinnerAdapter;
import android.widget.TextView;

import junit.framework.Assert;

import org.eehouse.android.xw4.DictLangCache.LangsArrayAdapter;
import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.DlgDelegate.ActionPair;
import org.eehouse.android.xw4.jni.CommonPrefs;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;
import org.eehouse.android.xw4.jni.CurGameInfo;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;
import org.eehouse.android.xw4.jni.JNIThread;
import org.eehouse.android.xw4.jni.LocalPlayer;
import org.eehouse.android.xw4.jni.XwJNI;

public class GameConfigDelegate extends DelegateBase
    implements View.OnClickListener
               ,XWListItem.DeleteCallback
               ,RefreshNamesTask.NoNameFound {
    private static final String TAG = GameConfigDelegate.class.getSimpleName();

    private static final String INTENT_FORRESULT_NEWGAME = "newgame";

    private static final String WHICH_PLAYER = "WHICH_PLAYER";

    private Activity m_activity;
    private CheckBox m_joinPublicCheck;
    private CheckBox m_gameLockedCheck;
    private boolean m_isLocked;
    private boolean m_haveClosed;
    private LinearLayout m_publicRoomsSet;
    private LinearLayout m_privateRoomsSet;

    private CommsConnTypeSet m_conTypes;
    private Button m_addPlayerButton;
    private Button m_changeConnButton;
    private Button m_jugglePlayersButton;
    private Button m_playButton;
    private ImageButton m_refreshRoomsButton;
    private View m_connectSetRelay;
    private Spinner m_dictSpinner;
    private Spinner m_playerDictSpinner;
    private Spinner m_roomChoose;
    // private Button m_configureButton;
    private long m_rowid;
    private boolean m_isNewGame;
    private CurGameInfo m_gi;
    private CurGameInfo m_giOrig;
    private JNIThread m_jniThread;
    private int m_whichPlayer;
    // private Spinner m_roleSpinner;
    // private Spinner m_connectSpinner;
    private Spinner m_phoniesSpinner;
    private Spinner m_boardsizeSpinner;
    private Spinner m_langSpinner;
    private Spinner m_smartnessSpinner;
    private TextView m_connLabel;
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
    private static final int[] s_disabledWhenLocked
        = { R.id.juggle_players,
            R.id.add_player,
            R.id.lang_spinner,
            R.id.dict_spinner,
            R.id.join_public_room_check,
            R.id.room_edit,
            R.id.advertise_new_room_check,
            R.id.room_spinner,
            R.id.refresh_button,
            R.id.hints_allowed,
            R.id.pick_faceup,
            R.id.boardsize_spinner,
            R.id.use_timer,
            R.id.timer_minutes_edit,
            R.id.smart_robot,
            R.id.phonies_spinner,
            R.id.change_connection,
    };

    public GameConfigDelegate( Delegator delegator, Bundle savedInstanceState )
    {
        super( delegator, savedInstanceState, R.layout.game_config );
        m_activity = delegator.getActivity();
    }

    class RemoteChoices extends XWListAdapter {
        public RemoteChoices() { super( m_gi.nPlayers ); }

        public Object getItem( int position) { return m_gi.players[position]; }
        public View getView( final int position, View convertView,
                             ViewGroup parent ) {
            OnCheckedChangeListener lstnr;
            lstnr = new OnCheckedChangeListener() {
                    @Override
                    public void onCheckedChanged( CompoundButton buttonView,
                                                 boolean isChecked )
                    {
                        m_gi.players[position].isLocal = !isChecked;
                    }
                };
            CheckBox cb = new CheckBox( m_activity );
            LocalPlayer lp = m_gi.players[position];
            cb.setText( lp.name );
            cb.setChecked( !lp.isLocal );
            cb.setOnCheckedChangeListener( lstnr );
            return cb;
        }
    }

    protected Dialog onCreateDialog( int id )
    {
        Dialog dialog = super.onCreateDialog( id );

        if ( null == dialog ) {
            DialogInterface.OnClickListener dlpos;
            AlertDialog.Builder ab;

            final DlgID dlgID = DlgID.values()[id];
            switch (dlgID) {
            case PLAYER_EDIT:
                View playerEditView = inflate( R.layout.player_edit );

                dialog = makeAlertBuilder()
                    .setTitle(R.string.player_edit_title)
                    .setView(playerEditView)
                    .setPositiveButton( android.R.string.ok,
                                        new DialogInterface.OnClickListener() {
                                            public void
                                                onClick( DialogInterface dlg,
                                                         int button ) {
                                                GameConfigDelegate self = curThis();
                                                self.getPlayerSettings( dlg );
                                                self.loadPlayersList();
                                            }
                                        })
                    .setNegativeButton( android.R.string.cancel, null )
                    .create();
                break;
                // case ROLE_EDIT_RELAY:
                // case ROLE_EDIT_SMS:
                // case ROLE_EDIT_BT:
                //     dialog = new AlertDialog.Builder( this )
                //         .setTitle(titleForDlg(id))
                //         .setView( LayoutInflater.from(this)
                //                   .inflate( layoutForDlg(id), null ))
                //         .setPositiveButton( android.R.string.ok,
                //                             new DialogInterface.OnClickListener() {
                //                                 public void onClick( DialogInterface dlg,
                //                                                      int whichButton ) {
                //                                     getRoleSettings();
                //                                 }
                //                             })
                //         .setNegativeButton( android.R.string.cancel, null )
                //         .create();
                //     break;

            case FORCE_REMOTE:
                dlpos = new DialogInterface.OnClickListener() {
                        public void onClick( DialogInterface dlg,
                                             int whichButton ) {
                            curThis().loadPlayersList();
                        }
                    };
                dialog = makeAlertBuilder()
                    .setTitle( R.string.force_title )
                    .setView( inflate( layoutForDlg(dlgID) ) )
                    .setPositiveButton( android.R.string.ok, dlpos )
                    .create();
                DialogInterface.OnDismissListener dismiss =
                    new DialogInterface.OnDismissListener() {
                        @Override
                        public void onDismiss( DialogInterface di )
                        {
                            GameConfigDelegate self = curThis();
                            if ( null != self
                                 && self.m_gi.forceRemoteConsistent() ) {
                                self.showToast( R.string.forced_consistent );
                                self.loadPlayersList();
                            } else {
                                DbgUtils.logw( TAG, "onDismiss(): "
                                               + "no visible self" );
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
                            GameConfigDelegate self = curThis();
                            self.applyChanges( true );
                            if ( DlgID.CONFIRM_CHANGE_PLAY == dlgID ) {
                                self.launchGame( true );
                            }
                        }
                    };
                ab = makeAlertBuilder()
                    .setTitle( R.string.confirm_save_title )
                    .setMessage( R.string.confirm_save )
                    .setPositiveButton( R.string.button_save, dlpos );
                if ( DlgID.CONFIRM_CHANGE_PLAY == dlgID ) {
                    dlpos = new DialogInterface.OnClickListener() {
                            public void onClick( DialogInterface dlg,
                                                 int whichButton ) {
                                curThis().finishAndLaunch();
                            }
                        };
                } else {
                    dlpos = null;
                }
                ab.setNegativeButton( R.string.button_discard_changes, dlpos );
                dialog = ab.create();

                dialog.setOnDismissListener( new DialogInterface.
                                             OnDismissListener() {
                        public void onDismiss( DialogInterface di ) {
                            curThis().closeNoSave();
                        }
                    });
                break;
            case NO_NAME_FOUND:
                String langName = DictLangCache.getLangName( m_activity,
                                                             m_gi.dictLang );
                String msg = getString( R.string.no_name_found_fmt,
                                        m_gi.nPlayers, xlateLang( langName ) );
                dialog = makeAlertBuilder()
                    .setPositiveButton( android.R.string.ok, null )
                    // message added below since varies with language etc.
                    .setMessage( msg )
                    .create();
                break;
            case CHANGE_CONN:
                LinearLayout layout = (LinearLayout)inflate( R.layout.conn_types_display );
                final ConnViaViewLayout items = (ConnViaViewLayout)
                    layout.findViewById( R.id.conn_types );
                final CheckBox cb = (CheckBox)layout
                    .findViewById(R.id.default_check);
                cb.setVisibility( View.VISIBLE );

                final DialogInterface.OnClickListener lstnr =
                    new DialogInterface.OnClickListener() {
                        public void onClick( DialogInterface dlg, int button ) {
                            GameConfigDelegate self = curThis();
                            self.m_conTypes = items.getTypes();
                            if ( cb.isChecked()) {
                                XWPrefs.setAddrTypes( self.m_activity, self.m_conTypes );
                            }

                            self.m_car.populate( self.m_activity, self.m_conTypes );

                            self.setConnLabel();
                            self.setupRelayStuffIf( false );
                            self.showHideRelayStuff();
                        }
                    };

                dialog = makeAlertBuilder()
                    .setTitle( R.string.title_addrs_pref )
                    .setView( layout )
                    .setPositiveButton( android.R.string.ok, lstnr )
                    .setNegativeButton( android.R.string.cancel, null )
                    .create();
                break;
            }
        }
        return dialog;
    } // onCreateDialog

    @Override
    protected void prepareDialog( DlgID dlgID, Dialog dialog )
    {
        switch ( dlgID ) {
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

        case CHANGE_CONN:
            ConnViaViewLayout items = (ConnViaViewLayout)
                dialog.findViewById( R.id.conn_types );
            items.configure( m_conTypes,
                             new ConnViaViewLayout.CheckEnabledWarner() {
                                 public void warnDisabled( CommsConnType typ ) {
                                     switch( typ ) {
                                     case COMMS_CONN_SMS:
                                         makeConfirmThenBuilder( R.string.warn_sms_disabled,
                                                                 Action.ENABLE_SMS_ASK )
                                             .setPosButton( R.string.button_enable_sms )
                                             .setNegButton( R.string.button_later )
                                             .show();
                                         break;
                                     case COMMS_CONN_BT:
                                         makeConfirmThenBuilder( R.string.warn_bt_disabled,
                                                                 Action.ENABLE_BT_DO )
                                             .setPosButton( R.string.button_enable_bt )
                                             .setNegButton( R.string.button_later )
                                             .show();
                                         break;
                                     case COMMS_CONN_RELAY:
                                         String msg = getString( R.string.warn_relay_disabled )
                                             + "\n\n" + getString( R.string.warn_relay_later );
                                         makeConfirmThenBuilder( msg, Action.ENABLE_RELAY_DO )
                                             .setPosButton( R.string.button_enable_relay )
                                             .setNegButton( R.string.button_later )
                                             .show();
                                         break;
                                     default:
                                         Assert.fail();
                                     }
                                 }
                             }, null, this );
            break;
        }
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
            String langName = DictLangCache.getLangName( m_activity, m_gi.dictLang );
            String label = getString( R.string.dict_lang_label_fmt, langName );
            dictLabel.setText( label );
        } else {
            dictLabel.setVisibility( View.GONE );
        }
        m_playerDictSpinner = (Spinner)dialog.findViewById( R.id.dict_spinner );
        if ( localOnlyGame() ) {
            configDictSpinner( m_playerDictSpinner, m_gi.dictLang, m_gi.dictName(lp) );
        } else {
            m_playerDictSpinner.setVisibility( View.GONE );
            m_playerDictSpinner = null;
        }

        final View localSet = dialog.findViewById( R.id.local_player_set );

        CheckBox check = (CheckBox)
            dialog.findViewById( R.id.remote_check );
        if ( isServer ) {
            OnCheckedChangeListener lstnr =
                new OnCheckedChangeListener() {
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
        OnCheckedChangeListener lstnr =
            new OnCheckedChangeListener() {
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
            {
                Spinner spinner =
                    (Spinner)((Dialog)di).findViewById( R.id.dict_spinner );
                Assert.assertTrue( m_playerDictSpinner == spinner );
            }
            int position = m_playerDictSpinner.getSelectedItemPosition();
            SpinnerAdapter adapter = m_playerDictSpinner.getAdapter();

            if ( null != adapter && position < adapter.getCount() ) {
                String name = (String)adapter.getItem( position );
                if ( ! name.equals( m_browseText ) ) {
                    lp.dictName = name;
                }
            }
        }

        lp.setIsRobot( Utils.getChecked( dialog, R.id.robot_check ) );
        lp.isLocal = !Utils.getChecked( dialog, R.id.remote_check );
    }

    protected void init( Bundle savedInstanceState )
    {
        getBundledData( savedInstanceState );

        m_browseText = getString( R.string.download_dicts );
        DictLangCache.setLast( m_browseText );

        m_cp = CommonPrefs.get( m_activity );

        Bundle args = getArguments();
        m_rowid = args.getLong( GameUtils.INTENT_KEY_ROWID, DBUtils.ROWID_NOTFOUND );
        Assert.assertTrue( DBUtils.ROWID_NOTFOUND != m_rowid );
        m_isNewGame = args.getBoolean( INTENT_FORRESULT_NEWGAME, false );

        m_connectSetRelay = findViewById( R.id.connect_set_relay );

        m_addPlayerButton = (Button)findViewById(R.id.add_player);
        m_addPlayerButton.setOnClickListener( this );
        m_changeConnButton = (Button)findViewById( R.id.change_connection );
        m_changeConnButton.setOnClickListener( this );
        m_jugglePlayersButton = (Button)findViewById(R.id.juggle_players);
        m_jugglePlayersButton.setOnClickListener( this );
        m_playButton = (Button)findViewById( R.id.play_button );
        m_playButton.setOnClickListener( this );

        m_playerLayout = (LinearLayout)findViewById( R.id.player_list );
        m_phoniesSpinner = (Spinner)findViewById( R.id.phonies_spinner );
        m_boardsizeSpinner = (Spinner)findViewById( R.id.boardsize_spinner );
        m_smartnessSpinner = (Spinner)findViewById( R.id.smart_robot );

        m_connLabel = (TextView)findViewById( R.id.conns_label );

        // This should only be in for one ship! Remove it and all associated
        // strings immediately after shipping it.
        if ( !Utils.onFirstVersion( m_activity )
             && !XWPrefs.getPublicRoomsEnabled( m_activity ) ) {
            ActionPair pair = new ActionPair( Action.SET_ENABLE_PUBLIC,
                                              R.string.enable_pubroom_title );
            makeNotAgainBuilder( R.string.not_again_enablepublic,
                                 R.string.key_notagain_enablepublic,
                                 Action.SKIP_CALLBACK )
                .setActionPair(pair)
                .show();
        }
    } // init

    @Override
    protected void onResume()
    {
        m_jniThread = JNIThread.getRetained( m_rowid );
        super.onResume();
        loadGame();
    }

    protected void onPause()
    {
        m_giOrig = null;        // flag for onStart and onResume
        super.onPause();
        if ( null != m_jniThread ) {
            m_jniThread.release();
            m_jniThread = null;
        }
    }

    protected void onSaveInstanceState( Bundle outState )
    {
        outState.putInt( WHICH_PLAYER, m_whichPlayer );
    }

    @Override
    protected void onActivityResult( RequestCode requestCode, int resultCode, Intent data )
    {
        if ( Activity.RESULT_CANCELED != resultCode ) {
            loadGame();
            switch( requestCode ) {
            case REQUEST_DICT:
                String dictName = data.getStringExtra( DictsDelegate.RESULT_LAST_DICT );
                configDictSpinner( m_dictSpinner, m_gi.dictLang, dictName );
                configDictSpinner( m_playerDictSpinner, m_gi.dictLang, dictName );
                break;
            case REQUEST_LANG_GC:
                String langName = data.getStringExtra( DictsDelegate.RESULT_LAST_LANG );
                selLangChanged( langName );
                setLangSpinnerSelection( langName );
                break;
            default:
                Assert.fail();
            }
        }
    }

    private void loadGame()
    {
        if ( null == m_giOrig ) {
            m_giOrig = new CurGameInfo( m_activity );

            GameLock gameLock;
            XwJNI.GamePtr gamePtr;
            if ( null == m_jniThread ) {
                 gameLock = new GameLock( m_rowid, false ).lock();
                 gamePtr = GameUtils.loadMakeGame( m_activity, m_giOrig, gameLock );
            } else {
                gameLock = m_jniThread.getLock();
                gamePtr = m_jniThread.getGamePtr();
            }

            if ( null == gamePtr ) {
                showDictGoneFinish();
            } else {
                m_gameStarted = XwJNI.model_getNMoves( gamePtr ) > 0
                    || XwJNI.comms_isConnected( gamePtr );

                if ( m_gameStarted ) {
                    if ( null == m_gameLockedCheck ) {
                        m_gameLockedCheck =
                            (CheckBox)findViewById( R.id.game_locked_check );
                        m_gameLockedCheck.setVisibility( View.VISIBLE );
                        m_gameLockedCheck.setOnClickListener( this );
                    }
                    handleLockedChange();
                }

                if ( null == m_gi ) {
                    m_gi = new CurGameInfo( m_activity, m_giOrig );
                }

                m_carOrig = new CommsAddrRec();
                if ( XwJNI.game_hasComms( gamePtr ) ) {
                    XwJNI.comms_getAddr( gamePtr, m_carOrig );
                    m_remoteAddrs = XwJNI.comms_getAddrs( gamePtr );
                } else if ( !localOnlyGame() ) {
                    String relayName = XWPrefs.getDefaultRelayHost( m_activity );
                    int relayPort = XWPrefs.getDefaultRelayPort( m_activity );
                    XwJNI.comms_getInitialAddr( m_carOrig, relayName,
                                                relayPort );
                }
                m_conTypes = (CommsConnTypeSet)m_carOrig.conTypes.clone();

                if ( null == m_jniThread ) {
                    gamePtr.release();
                    gameLock.unlock();
                }

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

                setConnLabel();
                setupRelayStuffIf( false );
                loadPlayersList();
                configLangSpinner();

                m_phoniesSpinner.setSelection( m_gi.phoniesAction.ordinal() );

                setSmartnessSpinner();

                setChecked( R.id.hints_allowed, !m_gi.hintsNotAllowed );
                setChecked( R.id.pick_faceup, m_gi.allowPickTiles );
                setInt( R.id.timer_minutes_edit,
                        m_gi.gameSeconds/60/m_gi.nPlayers );

                CheckBox check = (CheckBox)findViewById( R.id.use_timer );
                OnCheckedChangeListener lstnr =
                    new OnCheckedChangeListener() {
                        public void onCheckedChanged( CompoundButton buttonView,
                                                      boolean checked ) {
                            View view = findViewById( R.id.timer_set );
                            view.setVisibility( checked ? View.VISIBLE : View.GONE );
                        }
                    };
                check.setOnCheckedChangeListener( lstnr );
                setChecked( R.id.use_timer, m_gi.timerEnabled );

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
        showDialog( DlgID.NO_NAME_FOUND );
    }

    @Override
    public void dlgButtonClicked( Action action, int button, Object[] params )
    {
        boolean callSuper = false;
        Assert.assertTrue( curThis() == this );

        if ( AlertDialog.BUTTON_POSITIVE == button ) {
            switch( action ) {
            case LOCKED_CHANGE_ACTION:
                handleLockedChange();
                break;
            case SMS_CONFIG_ACTION:
                Utils.launchSettings( m_activity );
                break;
            case DELETE_AND_EXIT:
                if ( m_isNewGame ) {
                    deleteGame();
                }
                closeNoSave();
                break;
            case SET_ENABLE_PUBLIC:
                XWPrefs.setPrefsBoolean( m_activity, R.string.key_enable_pubroom,
                                         true );
                setupRelayStuffIf( true );
                break;
            default:
                callSuper = true;
            }
        } else if ( AlertDialog.BUTTON_NEGATIVE == button ) {
            switch ( action ) {
            case DELETE_AND_EXIT:
                showDialog( DlgID.CHANGE_CONN );
                break;
            default:
                callSuper = true;
                break;
            }
        } else {
            callSuper = true;
        }

        if ( callSuper ) {
            super.dlgButtonClicked( action, button, params );
        }
    }

    public void onClick( View view )
    {
        if ( isFinishing() ) {
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
            makeNotAgainBuilder( R.string.not_again_unlock,
                                 R.string.key_notagain_unlock,
                                 Action.LOCKED_CHANGE_ACTION )
                .show();
        } else if ( m_refreshRoomsButton == view ) {
            refreshNames();
        } else if ( m_changeConnButton == view ) {
            showDialog( DlgID.CHANGE_CONN );
        } else if ( m_playButton == view ) {
            // Launch BoardActivity for m_name, but ONLY IF user
            // confirms any changes required.  So we either launch
            // from here if there's no confirmation needed, or launch
            // a new dialog whose OK button does the same thing.
            saveChanges();

            if ( !localOnlyGame() && 0 == m_conTypes.size() ) {
                makeConfirmThenBuilder( R.string.config_no_connvia,
                                        Action.DELETE_AND_EXIT )
                    .setPosButton( R.string.button_discard )
                    .setNegButton( R.string.button_edit )
                    .show();
            } else if ( m_isNewGame || !m_gameStarted ) {
                saveAndClose( true );
            } else if ( m_giOrig.changesMatter(m_gi)
                        || m_carOrig.changesMatter(m_car) ) {
                showDialog( DlgID.CONFIRM_CHANGE_PLAY );
            } else {
                finishAndLaunch();
            }

        } else {
            DbgUtils.logw( TAG, "unknown v: " + view.toString() );
        }
    } // onClick

    private void saveAndClose( boolean forceNew )
    {
        DbgUtils.logi( TAG, "saveAndClose(forceNew=%b)", forceNew );
        applyChanges( forceNew );

        finishAndLaunch();
    }

    private void finishAndLaunch()
    {
        if ( !m_haveClosed ) {
            m_haveClosed = true;
            Intent intent = new Intent();
            intent.putExtra( GameUtils.INTENT_KEY_ROWID, m_rowid );
            setResult( Activity.RESULT_OK, intent );
            finish();
        }
    }

    private void closeNoSave()
    {
        if ( !m_haveClosed ) {
            m_haveClosed = true;
            setResult( Activity.RESULT_CANCELED, null );
            finish();
        }
    }

    @Override
    protected boolean handleBackPressed()
    {
        boolean consumed = false;
        if ( ! isFinishing() ) {
            if ( m_isNewGame ) {
                deleteGame();
            } else {
                saveChanges();
                if ( !m_gameStarted ) { // no confirm needed
                    applyChanges( true );
                } else if ( m_giOrig.changesMatter(m_gi)
                            || m_carOrig.changesMatter(m_car) ) {
                    showDialog( DlgID.CONFIRM_CHANGE );
                    consumed = true; // don't dismiss activity yet!
                } else {
                    applyChanges( false );
                }
            }
        }

        return consumed;
    }

    @Override
    protected GameConfigDelegate curThis()
    {
        return (GameConfigDelegate)super.curThis();
    }

    private void deleteGame()
    {
        GameUtils.deleteGame( m_activity, m_rowid, false );
    }

    private void loadPlayersList()
    {
        if ( !isFinishing() ) {
            m_playerLayout.removeAllViews();

            String[] names = m_gi.visibleNames( false );
            // only enable delete if one will remain (or two if networked)
            boolean canDelete = names.length > 2
                || (localOnlyGame() && names.length > 1);
            View.OnClickListener lstnr = new View.OnClickListener() {
                    @Override
                    public void onClick( View view ) {
                        m_whichPlayer = ((XWListItem)view).getPosition();
                        showDialog( DlgID.PLAYER_EDIT );
                    }
                };

            boolean localGame = localOnlyGame();
            boolean unlocked = null == m_gameLockedCheck
                || !m_gameLockedCheck.isChecked();
            for ( int ii = 0; ii < names.length; ++ii ) {
                final XWListItem view = XWListItem.inflate( m_activity, null );
                view.setPosition( ii );
                view.setText( names[ii] );
                if ( localGame && m_gi.players[ii].isLocal ) {
                    view.setComment( m_gi.dictName(ii) );
                }
                if ( canDelete ) {
                    view.setDeleteCallback( this );
                }

                view.setEnabled( unlocked );
                view.setOnClickListener( lstnr );
                m_playerLayout.addView( view );

                View divider = inflate( R.layout.divider_view );
                m_playerLayout.addView( divider );
            }

            m_addPlayerButton
                .setVisibility( names.length >= CurGameInfo.MAX_NUM_PLAYERS?
                                View.GONE : View.VISIBLE );
            m_jugglePlayersButton
                .setVisibility( names.length <= 1 ?
                                View.GONE : View.VISIBLE );

            showHideRelayStuff();

            if ( ! localOnlyGame()
                 && ((0 == m_gi.remoteCount() )
                     || (m_gi.nPlayers == m_gi.remoteCount()) ) ) {
                showDialog( DlgID.FORCE_REMOTE );
            }
            adjustPlayersLabel();
        }
    } // loadPlayersList

    private void configDictSpinner( Spinner dictsSpinner, int lang,
                                    String curDict )
    {
        if ( null != dictsSpinner ) {
            String langName = DictLangCache.getLangName( m_activity, lang );
            dictsSpinner.setPrompt( getString( R.string.dicts_list_prompt_fmt,
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
                            DictsDelegate.downloadForResult( getDelegator(),
                                                             RequestCode.REQUEST_DICT,
                                                             m_gi.dictLang );
                        }
                    }

                    @Override
                    public void onNothingSelected(AdapterView<?> parentView) {}
                };

            ArrayAdapter<String> adapter =
                DictLangCache.getDictsAdapter( m_activity, lang );

            configSpinnerWDownload( dictsSpinner, adapter, onSel, curDict );
        }
    }

    private void configLangSpinner()
    {
        if ( null == m_langSpinner ) {
            m_langSpinner = (Spinner)findViewById( R.id.lang_spinner );

            final LangsArrayAdapter adapter = DictLangCache.getLangsAdapter( m_activity );

            OnItemSelectedListener onSel =
                new OnItemSelectedListener() {
                    @Override
                    public void onItemSelected(AdapterView<?> parentView,
                                               View selectedItemView,
                                               int position, long id ) {
                        if ( ! isFinishing() ) { // not on the way out?
                            String chosen =
                                (String)parentView.getItemAtPosition( position );
                            if ( chosen.equals( m_browseText ) ) {
                                DictsDelegate.downloadForResult( getDelegator(),
                                                                 RequestCode
                                                                 .REQUEST_LANG_GC );
                            } else {
                                String langName = adapter.getLangAtPosition( position );
                                selLangChanged( langName );
                            }
                        }
                    }

                    @Override
                    public void onNothingSelected(AdapterView<?> parentView) {}
                };

            String lang = DictLangCache.getLangName( m_activity, m_gi.dictLang );
            configSpinnerWDownload( m_langSpinner, adapter, onSel, lang );
        }
    }

    private void selLangChanged( String chosen )
    {
        m_gi.setLang( DictLangCache.getLangLangCode( m_activity, chosen ) );
        loadPlayersList();
        configDictSpinner( m_dictSpinner, m_gi.dictLang, m_gi.dictName );
    }

    private void configSpinnerWDownload( Spinner spinner,
                                         ArrayAdapter adapter,
                                         OnItemSelectedListener onSel,
                                         String curSel )
    {
        int resID = android.R.layout.simple_spinner_dropdown_item;
        adapter.setDropDownViewResource( resID );
        spinner.setAdapter( adapter );
        spinner.setOnItemSelectedListener( onSel );
        if ( m_langSpinner == spinner ) {
            setLangSpinnerSelection( curSel );
        } else {
            setSpinnerSelection( spinner, curSel );
        }
    }

    private void setLangSpinnerSelection( String sel )
    {
        LangsArrayAdapter adapter = (LangsArrayAdapter)m_langSpinner.getAdapter();
        int pos = adapter.getPosForLang( sel );
        if ( 0 <= pos ) {
            m_langSpinner.setSelection( pos, true );
        }
    }

    private void setSpinnerSelection( Spinner spinner, String sel )
    {
        if ( null != sel && null != spinner ) {
            SpinnerAdapter adapter = spinner.getAdapter();
            int count = adapter.getCount();
            for ( int ii = 0; ii < count; ++ii ) {
                if ( sel.equals( adapter.getItem( ii ) ) ) {
                    spinner.setSelection( ii, true );
                    break;
                }
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
            DbgUtils.logw( TAG, "setSmartnessSpinner got %d from getRobotSmartness()",
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

    private void adjustPlayersLabel()
    {
        DbgUtils.logi( TAG, "adjustPlayersLabel()" );
        String label;
        if ( localOnlyGame() ) {
            label = getString( R.string.players_label_standalone );
        } else {
            int remoteCount = m_gi.remoteCount();
            label = getString( R.string.players_label_host_fmt,
                               m_gi.nPlayers - remoteCount,
                               remoteCount );
        }
        ((TextView)findViewById( R.id.players_label )).setText( label );
    }

    private void adjustConnectStuff()
    {
        if ( XWPrefs.getPublicRoomsEnabled( m_activity ) ) {
            if ( m_joinPublicCheck.isChecked() ) {
                refreshNames();
                m_privateRoomsSet.setVisibility( View.GONE );
                m_publicRoomsSet.setVisibility( View.VISIBLE );
            } else {
                m_privateRoomsSet.setVisibility( View.VISIBLE );
                m_publicRoomsSet.setVisibility( View.GONE );
            }
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
            view.setEnabled( !locking );
        }

        int nChildren = m_playerLayout.getChildCount();
        for ( int ii = 0; ii < nChildren; ++ii ) {
            View child = m_playerLayout.getChildAt( ii );
            if ( child instanceof XWListItem ) {
                ((XWListItem)child).setEnabled( !locking );
            }
        }
    }

    private int layoutForDlg( DlgID dlgID )
    {
        switch( dlgID ) {
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

        m_gi.hintsNotAllowed = !getChecked( R.id.hints_allowed );
        m_gi.allowPickTiles = getChecked( R.id.pick_faceup );
        m_gi.timerEnabled = getChecked( R.id.use_timer );
        m_gi.gameSeconds =
            60 * m_gi.nPlayers * getInt( R.id.timer_minutes_edit );

        int position = m_phoniesSpinner.getSelectedItemPosition();
        m_gi.phoniesAction = CurGameInfo.XWPhoniesChoice.values()[position];

        position = m_smartnessSpinner.getSelectedItemPosition();
        m_gi.setRobotSmartness(position * 49 + 1);

        position = m_boardsizeSpinner.getSelectedItemPosition();
        m_gi.boardSize = positionToSize( position );

        if ( m_conTypes.contains( CommsConnType.COMMS_CONN_RELAY ) ) {
            m_car.ip_relay_seeksPublicRoom = m_joinPublicCheck.isChecked();
            m_car.ip_relay_advertiseRoom =
                getChecked( R.id.advertise_new_room_check );
            if ( m_car.ip_relay_seeksPublicRoom ) {
                SpinnerAdapter adapter = m_roomChoose.getAdapter();
                if ( null != adapter ) {
                    int pos = m_roomChoose.getSelectedItemPosition();
                    if ( pos >= 0 && pos < adapter.getCount() ) {
                        m_car.ip_relay_invite = (String)adapter.getItem(pos);
                    }
                }
            } else {
                m_car.ip_relay_invite = getText( R.id.room_edit ).trim();
            }
        }

        m_car.conTypes = m_conTypes;
    } // saveChanges

    private void applyChanges( boolean forceNew )
    {
        if ( !isFinishing() ) {
            GameLock gameLock = m_jniThread == null
                ? new GameLock( m_rowid, true ).lock()
                : m_jniThread.getLock();
            GameUtils.applyChanges( m_activity, m_gi, m_car, gameLock,
                                    forceNew );
            DBUtils.saveThumbnail( m_activity, gameLock, null ); // clear it
            if ( null == m_jniThread ) {
                gameLock.unlock();
            }
        }
    }

    private void launchGame( boolean forceNew )
    {
        if ( m_conTypes.contains( CommsConnType.COMMS_CONN_RELAY )
             && 0 == m_car.ip_relay_invite.length() ) {
            makeOkOnlyBuilder( R.string.no_empty_rooms ).show();
        } else {
            saveAndClose( forceNew );
        }
    }

    private void refreshNames()
    {
        if ( !m_isLocked ) {
            new RefreshNamesTask( m_activity, this, m_gi.dictLang,
                                  m_gi.nPlayers, m_roomChoose ).execute();
        }
    }

    @Override
    protected void setTitle()
    {
        int strID;
        if ( null != m_conTypes && 0 < m_conTypes.size() ) {
            strID = R.string.title_gamenet_config_fmt;
        } else {
            strID = R.string.title_game_config_fmt;
        }
        String name = GameUtils.getName( m_activity, m_rowid );
        setTitle( getString( strID, name ) );
    }

    private boolean localOnlyGame()
    {
        return DeviceRole.SERVER_STANDALONE == m_giOrig.serverRole;
    }

    public static void editForResult( Delegator delegator,
                                      RequestCode requestCode,
                                      long rowID, boolean newGame )
    {
        Bundle bundle = new Bundle();
        bundle.putLong( GameUtils.INTENT_KEY_ROWID, rowID );
        bundle.putBoolean( INTENT_FORRESULT_NEWGAME, newGame );

        if ( delegator.inDPMode() ) {
            delegator
                .addFragmentForResult( GameConfigFrag.newInstance( delegator ),
                                       bundle, requestCode );
        } else {
            Activity activity = delegator.getActivity();
            Intent intent = new Intent( activity, GameConfigActivity.class );
            intent.setAction( Intent.ACTION_EDIT );
            intent.putExtras( bundle );
            activity.startActivityForResult( intent, requestCode.ordinal() );
        }
    }

    private void setConnLabel()
    {
        if ( localOnlyGame() ) {
            m_connLabel.setVisibility( View.GONE );
            m_changeConnButton.setVisibility( View.GONE );
        } else {
            String connString = m_conTypes.toString( m_activity, true );
            m_connLabel.setText( getString( R.string.connect_label_fmt, connString ) );
        }
    }

    private void setupRelayStuffIf( boolean reset )
    {
        if ( m_conTypes.contains( CommsConnType.COMMS_CONN_RELAY ) ) {
            boolean publicEnabled = XWPrefs.getPublicRoomsEnabled( m_activity );
            int vis = publicEnabled ? View.VISIBLE : View.GONE;
            if ( reset || null == m_joinPublicCheck ) {
                m_joinPublicCheck =
                    (CheckBox)findViewById(R.id.join_public_room_check);
                m_joinPublicCheck.setVisibility( vis );

                CheckBox advertise = (CheckBox)
                    findViewById( R.id.advertise_new_room_check );
                advertise.setVisibility( vis );
                if ( publicEnabled ) {
                    m_joinPublicCheck.setOnClickListener( this );
                    m_joinPublicCheck.setChecked( m_car.ip_relay_seeksPublicRoom );
                   advertise.setChecked( m_car.ip_relay_advertiseRoom );
                    m_publicRoomsSet =
                        (LinearLayout)findViewById(R.id.public_rooms_set );
                    m_privateRoomsSet =
                        (LinearLayout)findViewById(R.id.private_rooms_set );
                }

                setText( R.id.room_edit, m_car.ip_relay_invite );

                m_roomChoose = (Spinner)findViewById( R.id.room_spinner );
                m_roomChoose.setVisibility( vis );

                m_refreshRoomsButton =
                    (ImageButton)findViewById( R.id.refresh_button );
                m_refreshRoomsButton.setVisibility( vis );
                m_refreshRoomsButton.setOnClickListener( this );

                adjustConnectStuff();
            }
        }
    }

    private void showHideRelayStuff()
    {
        boolean show = m_conTypes.contains( CommsConnType.COMMS_CONN_RELAY );
        m_connectSetRelay.setVisibility( show ? View.VISIBLE : View.GONE );
    }
}
