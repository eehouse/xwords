/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2020 by Eric House (xwords@eehouse.org).  All rights
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
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.CompoundButton;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.Spinner;
import android.widget.SpinnerAdapter;
import android.widget.TextView;

import java.util.HashMap;
import java.util.Map;

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
    implements View.OnClickListener, XWListItem.DeleteCallback {
    private static final String TAG = GameConfigDelegate.class.getSimpleName();

    private static final String INTENT_FORRESULT_NEWGAME = "newgame";

    private static final String WHICH_PLAYER = "WHICH_PLAYER";
    private static final String LOCAL_GI = "LOCAL_GI";
    private static final String LOCAL_TYPES = "LOCAL_TYPES";
    private static final String DIS_MAP = "DIS_MAP";

    private Activity m_activity;
    private CheckBox m_gameLockedCheck;
    private boolean m_isLocked;
    private boolean m_haveClosed;

    private CommsConnTypeSet m_conTypes;
    private Button m_addPlayerButton;
    private Button m_changeConnButton;
    private Button m_jugglePlayersButton;
    private Spinner m_dictSpinner;
    private Spinner m_playerDictSpinner;
    private long m_rowid;
    private boolean m_isNewGame;
    private CurGameInfo m_gi;
    private CurGameInfo m_giOrig;
    private JNIThread m_jniThread;
    private int m_whichPlayer;
    private Spinner m_phoniesSpinner;
    private Spinner m_boardsizeSpinner;
    private Spinner m_traysizeSpinner;
    private Spinner m_langSpinner;
    private Spinner m_smartnessSpinner;
    private TextView m_connLabel;
    private String m_browseText;
    private LinearLayout m_playerLayout;
    private CommsAddrRec m_carOrig;
    private CommsAddrRec[] m_remoteAddrs;
    private CommsAddrRec m_car;
    private CommonPrefs m_cp;
    private boolean m_gameStarted = false;
    private String[] m_connStrings;
    private HashMap<CommsConnType, boolean[]> m_disabMap;
    private static final int[] s_disabledWhenLocked
        = { R.id.juggle_players,
            R.id.add_player,
            R.id.lang_spinner,
            R.id.dict_spinner,
            R.id.hints_allowed,
            R.id.duplicate_check,
            R.id.pick_faceup,
            R.id.boardsize_spinner,
            R.id.traysize_spinner,
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

    @Override
    protected Dialog makeDialog( DBAlert alert, Object[] params )
    {
        Dialog dialog = null;
        final DlgID dlgID = alert.getDlgID();
        Log.d( TAG, "makeDialog(%s)", dlgID.toString() );

        DialogInterface.OnClickListener dlpos;
        AlertDialog.Builder ab;

        switch ( dlgID ) {
        case PLAYER_EDIT: {
            View playerEditView = inflate( R.layout.player_edit );
            setPlayerSettings( playerEditView );

            dialog = makeAlertBuilder()
                .setTitle( R.string.player_edit_title )
                .setView( playerEditView )
                .setPositiveButton( android.R.string.ok,
                                    new DialogInterface.OnClickListener() {
                                        public void
                                            onClick( DialogInterface dlg,
                                                     int button ) {
                                            getPlayerSettings( dlg );
                                            loadPlayersList();
                                        }
                                    })
                .setNegativeButton( android.R.string.cancel, null )
                .create();

        }
            break;

        case FORCE_REMOTE: {
            dlpos = new DialogInterface.OnClickListener() {
                    public void onClick( DialogInterface dlg,
                                         int whichButton ) {
                        loadPlayersList();
                    }
                };

            View view = inflate( layoutForDlg(dlgID) );
            ListView listview = (ListView)view.findViewById( R.id.players );
            listview.setAdapter( new RemoteChoices() );
                
            dialog = makeAlertBuilder()
                .setTitle( R.string.force_title )
                .setView( view )
                .setPositiveButton( android.R.string.ok, dlpos )
                .create();
            alert.setOnDismissListener( new DBAlert.OnDismissListener() {
                    @Override
                    public void onDismissed( XWDialogFragment frag ) {
                        if ( m_gi.forceRemoteConsistent() ) {
                            showToast( R.string.forced_consistent );
                            loadPlayersList();
                        }
                    }
                });
        }
            break;
        case CONFIRM_CHANGE_PLAY:
        case CONFIRM_CHANGE: {
            dlpos = new DialogInterface.OnClickListener() {
                    public void onClick( DialogInterface dlg,
                                         int whichButton ) {
                        applyChanges( true );
                        if ( DlgID.CONFIRM_CHANGE_PLAY == dlgID ) {
                            launchGame( true );
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
                            finishAndLaunch();
                        }
                    };
            } else {
                dlpos = null;
            }
            ab.setNegativeButton( R.string.button_discard_changes, dlpos );
            dialog = ab.create();

            alert.setOnDismissListener( new DBAlert.OnDismissListener() {
                    @Override
                    public void onDismissed( XWDialogFragment frag ) {
                        closeNoSave();
                    }
                } );
        }
            break;
        case CHANGE_CONN: {
            CommsConnTypeSet conTypes = (CommsConnTypeSet)params[0];
            LinearLayout layout = (LinearLayout)inflate( R.layout.conn_types_display );
            final ConnViaViewLayout items = (ConnViaViewLayout)
                layout.findViewById( R.id.conn_types );
            items.setActivity( m_activity );

            items.configure( conTypes,
                             new ConnViaViewLayout.CheckEnabledWarner() {
                                 public void warnDisabled( CommsConnType typ ) {
                                     switch( typ ) {
                                     case COMMS_CONN_SMS:
                                         makeConfirmThenBuilder( R.string.warn_sms_disabled,
                                                                 Action.ENABLE_NBS_ASK )
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
                                         Assert.failDbg();
                                         break;
                                     case COMMS_CONN_MQTT:
                                         String msg = getString( R.string.warn_mqtt_disabled )
                                             + "\n\n" + getString( R.string.warn_mqtt_later );
                                         makeConfirmThenBuilder( msg, Action.ENABLE_MQTT_DO )
                                             .setPosButton( R.string.button_enable_mqtt )
                                             .setNegButton( R.string.button_later )
                                             .show();
                                         break;
                                     default:
                                         Assert.failDbg();
                                     }
                                 }
                             }, null, this );

            final CheckBox cb = (CheckBox)layout
                .findViewById( R.id.default_check );
            cb.setVisibility( View.VISIBLE ); // "gone" in .xml file

            DialogInterface.OnClickListener lstnr =
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick( DialogInterface dlg, int button ) {
                        m_conTypes = items.getTypes();
                        // Remove it if it's actually possible it's there
                        Assert.assertTrueNR( !m_conTypes.contains( CommsConnType.COMMS_CONN_RELAY ) );
                        if ( cb.isChecked()) {
                            XWPrefs.setAddrTypes( m_activity, m_conTypes );
                        }

                        m_car.populate( m_activity, m_conTypes );

                        setConnLabel();
                        setDisableds();
                    }
                };

            dialog = makeAlertBuilder()
                .setTitle( R.string.title_addrs_pref )
                .setView( layout )
                .setPositiveButton( android.R.string.ok, lstnr )
                .setNegativeButton( android.R.string.cancel, null )
                .create();
        }
            break;
        default:
            dialog = super.makeDialog( alert, params );
            break;
        }

        Assert.assertTrue( dialog != null || !BuildConfig.DEBUG );
        return dialog;
    } // makeDialog

    private void setPlayerSettings( final View playerView )
    {
        Log.d( TAG, "setPlayerSettings()" );
        boolean isServer = ! localOnlyGame();

        // Independent of other hide/show logic, these guys are
        // information-only if the game's locked.  (Except that in a
        // local game you can always toggle a player's robot state.)
        Utils.setEnabled( playerView, R.id.remote_check, !m_isLocked );
        Utils.setEnabled( playerView, R.id.player_name_edit, !m_isLocked );
        Utils.setEnabled( playerView, R.id.robot_check,
                          !m_isLocked || !isServer );

        // Hide remote option if in standalone mode...
        final LocalPlayer lp = m_gi.players[m_whichPlayer];
        Utils.setText( playerView, R.id.player_name_edit, lp.name );
        if ( BuildConfig.HAVE_PASSWORD ) {
            Utils.setText( playerView, R.id.password_edit, lp.password );
        } else {
            playerView.findViewById(R.id.password_set).setVisibility( View.GONE );
        }

        // Dicts spinner with label
        TextView dictLabel = (TextView)playerView
            .findViewById( R.id.dict_label );
        if ( localOnlyGame() ) {
            String langName = DictLangCache.getLangName( m_activity, m_gi.dictLang );
            String label = getString( R.string.dict_lang_label_fmt, langName );
            dictLabel.setText( label );
        } else {
            dictLabel.setVisibility( View.GONE );
        }
        m_playerDictSpinner = ((LabeledSpinner)playerView
                               .findViewById( R.id.player_dict_spinner ))
            .getSpinner();
        if ( localOnlyGame() ) {
            configDictSpinner( m_playerDictSpinner, m_gi.dictLang, m_gi.dictName(lp) );
        } else {
            m_playerDictSpinner.setVisibility( View.GONE );
            m_playerDictSpinner = null;
        }

        final View localSet = playerView.findViewById( R.id.local_player_set );

        CheckBox check = (CheckBox)playerView.findViewById( R.id.remote_check );
        if ( isServer ) {
            OnCheckedChangeListener lstnr =
                new OnCheckedChangeListener() {
                    public void onCheckedChanged( CompoundButton buttonView,
                                                  boolean checked ) {
                        lp.isLocal = !checked;
                        Utils.setEnabled( localSet, !checked );
                        checkShowPassword( playerView, lp );
                    }
                };
            check.setOnCheckedChangeListener( lstnr );
            check.setVisibility( View.VISIBLE );
        } else {
            check.setVisibility( View.GONE );
            Utils.setEnabled( localSet, true );
        }

        check = (CheckBox)playerView.findViewById( R.id.robot_check );
        OnCheckedChangeListener lstnr =
            new OnCheckedChangeListener() {
                public void onCheckedChanged( CompoundButton buttonView,
                                              boolean checked ) {
                    lp.setIsRobot( checked );
                    setPlayerName( playerView, lp );
                    checkShowPassword( playerView, lp );
                }
            };
        check.setOnCheckedChangeListener( lstnr );

        Utils.setChecked( playerView, R.id.robot_check, lp.isRobot() );
        Utils.setChecked( playerView, R.id.remote_check, ! lp.isLocal );
        checkShowPassword( playerView, lp );
        Log.d( TAG, "setPlayerSettings() DONE" );
    }

    private void setPlayerName( View playerView, LocalPlayer lp )
    {
        String name = lp.isRobot()
            ? CommonPrefs.getDefaultRobotName( m_activity )
            : CommonPrefs.getDefaultPlayerName( m_activity, m_whichPlayer );
        setText( playerView, R.id.player_name_edit, name );
    }

    // We show the password stuff only if: non-robot player AND there's more
    // than one local non-robot OR there's already a password set.
    private void checkShowPassword( View playerView, LocalPlayer lp )
    {
        boolean isRobotChecked = lp.isRobot();
        // Log.d( TAG, "checkShowPassword(isRobotChecked=%b)", isRobotChecked );
        boolean showPassword = !isRobotChecked && BuildConfig.HAVE_PASSWORD;

        if ( showPassword ) {
            String pwd = getText( playerView, R.id.password_edit );
            // If it's non-empty, we show it. Else count players
            if ( TextUtils.isEmpty(pwd) ) {
                int nLocalNonRobots = 0;
                for ( int ii = 0; ii < m_gi.nPlayers; ++ii ) {
                    LocalPlayer oneLP = m_gi.players[ii];
                    if ( oneLP.isLocal && !oneLP.isRobot() ) {
                        ++nLocalNonRobots;
                    }
                }
                // Log.d( TAG, "nLocalNonRobots: %d", nLocalNonRobots );
                showPassword = 1 < nLocalNonRobots;
            }
        }

        playerView.findViewById( R.id.password_set )
            .setVisibility( showPassword ? View.VISIBLE : View.GONE  );
    }

    private void getPlayerSettings( DialogInterface di )
    {
        Dialog dialog = (Dialog)di;
        LocalPlayer lp = m_gi.players[m_whichPlayer];
        lp.name = Utils.getText( dialog, R.id.player_name_edit );
        if ( BuildConfig.HAVE_PASSWORD ) {
            lp.password = Utils.getText( dialog, R.id.password_edit );
        }

        if ( localOnlyGame() ) {
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

    @Override
    protected void init( Bundle savedInstanceState )
    {
        getBundledData( savedInstanceState );

        m_browseText = getString( R.string.download_more );
        DictLangCache.setLast( m_browseText );

        m_cp = CommonPrefs.get( m_activity );

        Bundle args = getArguments();
        m_rowid = args.getLong( GameUtils.INTENT_KEY_ROWID, DBUtils.ROWID_NOTFOUND );
        Assert.assertTrue( DBUtils.ROWID_NOTFOUND != m_rowid );
        m_isNewGame = args.getBoolean( INTENT_FORRESULT_NEWGAME, false );

        m_addPlayerButton = (Button)findViewById(R.id.add_player);
        m_addPlayerButton.setOnClickListener( this );
        m_changeConnButton = (Button)findViewById( R.id.change_connection );
        m_changeConnButton.setOnClickListener( this );
        m_jugglePlayersButton = (Button)findViewById(R.id.juggle_players);
        m_jugglePlayersButton.setOnClickListener( this );
        findViewById( R.id.play_button ).setOnClickListener( this );

        m_playerLayout = (LinearLayout)findViewById( R.id.player_list );

        m_phoniesSpinner = ((LabeledSpinner)findViewById( R.id.phonies_spinner ))
            .getSpinner();
        m_boardsizeSpinner = ((LabeledSpinner)findViewById( R.id.boardsize_spinner ))
            .getSpinner();
        m_traysizeSpinner = ((LabeledSpinner)findViewById( R.id.traysize_spinner ))
            .getSpinner();
        m_smartnessSpinner = ((LabeledSpinner)findViewById( R.id.smart_robot ))
            .getSpinner();

        m_connLabel = (TextView)findViewById( R.id.conns_label );
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
        saveChanges();          // save before clearing m_giOrig!
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
        outState.putSerializable( LOCAL_GI, m_gi );
        outState.putSerializable( LOCAL_TYPES, m_conTypes );
        if ( BuildConfig.DEBUG ) {
            outState.putSerializable( DIS_MAP, m_disabMap );
        }
    }

    @Override
    protected void onActivityResult( RequestCode requestCode, int resultCode, Intent data )
    {
        boolean cancelled = Activity.RESULT_CANCELED == resultCode;

        loadGame();
        switch( requestCode ) {
        case REQUEST_DICT:
            String dictName = cancelled ? m_gi.dictName
                : data.getStringExtra( DictsDelegate.RESULT_LAST_DICT );
            configDictSpinner( m_dictSpinner, m_gi.dictLang, dictName );
            configDictSpinner( m_playerDictSpinner, m_gi.dictLang, dictName );
            break;
        case REQUEST_LANG_GC:
            String langName = cancelled
                ? DictLangCache.getLangName( m_activity, m_gi.dictLang )
                : data.getStringExtra( DictsDelegate.RESULT_LAST_LANG );
            selLangChanged( langName );
            setLangSpinnerSelection( langName );
            break;
        default:
            Assert.failDbg();
        }
    }

    private void loadGame()
    {
        if ( null == m_giOrig ) {
            m_giOrig = new CurGameInfo( m_activity );

            if ( null != m_jniThread ) {
                try ( XwJNI.GamePtr gamePtr = m_jniThread
                      .getGamePtr().retain() ) {
                    loadGame( gamePtr );
                }
            } else {
                try ( GameLock lock = GameLock.tryLockRO( m_rowid ) ) {
                    if ( null != lock ) {
                        try ( XwJNI.GamePtr gamePtr = GameUtils.
                              loadMakeGame( m_activity, m_giOrig, lock ) ) {
                            loadGame( gamePtr );
                        }
                    }
                }
            }
        }
    }

    // Exists only to be called from inside two try-with-resource blocks above
    private void loadGame( XwJNI.GamePtr gamePtr )
    {
        if ( null == gamePtr ) {
            Assert.failDbg();
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
                m_gi = new CurGameInfo( m_giOrig );
            }

            if ( XwJNI.game_hasComms( gamePtr ) ) {
                m_carOrig = XwJNI.comms_getAddr( gamePtr );
                m_remoteAddrs = XwJNI.comms_getAddrs( gamePtr );
            } else if ( !localOnlyGame() ) {
                m_carOrig = XwJNI.comms_getInitialAddr();
            } else {
                // Leaving this null breaks stuff: an empty set, rather than a
                // null one, represents a standalone game
                m_carOrig = new CommsAddrRec();
            }

            // load if the first time through....
            if ( null == m_conTypes ) {
                m_conTypes = (CommsConnTypeSet)m_carOrig.conTypes.clone();
            }

            buildDisabledsMap( gamePtr );
            setDisableds();

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
            loadPlayersList();
            configLangSpinner();

            m_phoniesSpinner.setSelection( m_gi.phoniesAction.ordinal() );

            setSmartnessSpinner();

            tweakTimerStuff();

            setChecked( R.id.hints_allowed, !m_gi.hintsNotAllowed );
            setChecked( R.id.pick_faceup, m_gi.allowPickTiles );

            setBoardsizeSpinner();

            final int[] curSel = {-1};
            String val = String.format( "%d", m_gi.traySize );
            SpinnerAdapter adapter = m_traysizeSpinner.getAdapter();
            for ( int ii = 0; ii < adapter.getCount(); ++ii ) {
                if ( val.equals( adapter.getItem(ii) ) ) {
                    m_traysizeSpinner.setSelection( ii );
                    curSel[0] = ii;
                    break;
                }
            }
            m_traysizeSpinner
                .setOnItemSelectedListener( new Utils.OnNothingSelDoesNothing() {
                    @Override
                    public void onItemSelected( AdapterView<?> parent, View spinner,
                                                int position, long id ) {
                        if ( curSel[0] != position ) {
                            curSel[0] = position;
                            makeNotAgainBuilder( R.string.not_again_traysize,
                                                 R.string.key_na_traysize )
                                .show();
                        }
                    }
                } );

        }
    } // loadGame

    private boolean mTimerStuffInited = false;
    private void tweakTimerStuff()
    {
        // one-time only stuff
        if ( ! mTimerStuffInited ) {
            mTimerStuffInited = true;

            // dupe-mode check is GONE by default (in the .xml)
            if ( CommonPrefs.getDupModeHidden( m_activity ) ) {
                setChecked( R.id.duplicate_check, false );
            } else {
                CheckBox check = (CheckBox)findViewById( R.id.duplicate_check );
                check.setVisibility( View.VISIBLE );
                check.setChecked( m_gi.inDuplicateMode );
                check.setOnCheckedChangeListener( new OnCheckedChangeListener() {
                        @Override
                        public void onCheckedChanged( CompoundButton buttonView,
                                                      boolean checked ) {
                            tweakTimerStuff();
                        }
                    } );
            }
            CheckBox check = (CheckBox)findViewById( R.id.use_timer );
            OnCheckedChangeListener lstnr =
                new OnCheckedChangeListener() {
                    public void onCheckedChanged( CompoundButton buttonView,
                                                  boolean checked ) {
                        tweakTimerStuff();
                    }
                };
            check.setOnCheckedChangeListener( lstnr );
            check.setChecked( m_gi.timerEnabled );
        }

        boolean dupModeChecked = getChecked( R.id.duplicate_check );
        CheckBox check = (CheckBox)findViewById( R.id.use_timer );
        check.setText( dupModeChecked ? R.string.use_duptimer : R.string.use_timer );

        boolean timerSet = getChecked( R.id.use_timer );
        showTimerSet( timerSet );

        int id = dupModeChecked ? R.string.dup_minutes_label : R.string.minutes_label;
        TextView label = (TextView)findViewById(R.id.timer_label );
        label.setText( id );

        // setInt( R.id.timer_minutes_edit,
        //         m_gi.gameSeconds/60/m_gi.nPlayers );

        // setChecked( R.id.use_timer, m_gi.timerEnabled );
        // showTimerSet( m_gi.timerEnabled );
    }

    private void showTimerSet( boolean show )
    {
        View view = findViewById( R.id.timer_set );
        view.setVisibility( show ? View.VISIBLE : View.GONE );
    }

    private void getBundledData( Bundle bundle )
    {
        if ( null != bundle ) {
            m_whichPlayer = bundle.getInt( WHICH_PLAYER );
            m_gi = (CurGameInfo)bundle.getSerializable( LOCAL_GI );
            m_conTypes = (CommsConnTypeSet)bundle.getSerializable( LOCAL_TYPES );
            if ( BuildConfig.DEBUG ) {
                m_disabMap = (HashMap)bundle.getSerializable( DIS_MAP );
            }
        }
    }

    // DeleteCallback interface
    @Override
    public void deleteCalled( XWListItem item )
    {
        if ( m_gi.delete( item.getPosition() ) ) {
            loadPlayersList();
        }
    }

    @Override
    public boolean onPosButton( Action action, Object[] params )
    {
        boolean handled = true;
        Assert.assertTrue( curThis() == this );
        switch( action ) {
        case LOCKED_CHANGE_ACTION:
            handleLockedChange();
            break;
        case SMS_CONFIG_ACTION:
            PrefsDelegate.launch( m_activity );
            break;
        case DELETE_AND_EXIT:
            if ( m_isNewGame ) {
                deleteGame();
            }
            closeNoSave();
            break;
        case ASKED_PHONE_STATE:
            showDialogFragment( DlgID.CHANGE_CONN, m_conTypes );
            break;

        default:
            handled = super.onPosButton( action, params );
        }
        return handled;
    }

    @Override
    public boolean onNegButton( Action action, Object[] params )
    {
        boolean handled = true;
        switch ( action ) {
        case DELETE_AND_EXIT:
            showConnAfterCheck();
            break;
        case ASKED_PHONE_STATE:
            showDialogFragment( DlgID.CHANGE_CONN, m_conTypes );
            break;
        default:
            handled = super.onNegButton( action, params );
            break;
        }
        return handled;
    }

    public void onClick( View view )
    {
        if ( isFinishing() ) {
            // do nothing; we're on the way out
        } else {
            switch ( view.getId() ) {
            case R.id.add_player:
                int curIndex = m_gi.nPlayers;
                if ( curIndex < CurGameInfo.MAX_NUM_PLAYERS ) {
                    m_gi.addPlayer(); // ups nPlayers
                    loadPlayersList();
                }
                break;
            case R.id.juggle_players:
                m_gi.juggle();
                loadPlayersList();
                break;
            case R.id.game_locked_check:
                makeNotAgainBuilder( R.string.not_again_unlock,
                                     R.string.key_notagain_unlock,
                                     Action.LOCKED_CHANGE_ACTION )
                    .show();
                break;
            case R.id.change_connection:
                showConnAfterCheck();
                break;
            case R.id.play_button:
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
                    showDialogFragment( DlgID.CONFIRM_CHANGE_PLAY );
                } else {
                    saveAndClose( false );
                }
                break;
            default:
                Log.w( TAG, "unknown v: " + view.toString() );
                Assert.failDbg();
            }
        }
    } // onClick

    private void showConnAfterCheck()
    {
        if ( null == SMSPhoneInfo.get( m_activity ) ) {
            Perms23.tryGetPermsNA( this, Perms23.Perm.READ_PHONE_STATE,
                                   R.string.phone_state_rationale,
                                   R.string.key_na_perms_phonestate,
                                   Action.ASKED_PHONE_STATE );
        } else {
            showDialogFragment( DlgID.CHANGE_CONN, m_conTypes );
        }
    }

    private void saveAndClose( boolean forceNew )
    {
        Log.i( TAG, "saveAndClose(forceNew=%b)", forceNew );
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
        if ( ! isFinishing() && null != m_gi ) {
            if ( m_isNewGame ) {
                deleteGame();
            } else {
                saveChanges();
                if ( !m_gameStarted ) { // no confirm needed
                    applyChanges( true );
                } else if ( m_giOrig.changesMatter(m_gi)
                            || m_carOrig.changesMatter(m_car) ) {
                    showDialogFragment( DlgID.CONFIRM_CHANGE );
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
        GameUtils.deleteGame( m_activity, m_rowid, false, false );
    }

    private void loadPlayersList()
    {
        if ( !isFinishing() ) {
            m_playerLayout.removeAllViews();

            String[] names = m_gi.visibleNames( m_activity, false );
            // only enable delete if one will remain (or two if networked)
            boolean canDelete = names.length > 2
                || (localOnlyGame() && names.length > 1);
            View.OnClickListener lstnr = new View.OnClickListener() {
                    @Override
                    public void onClick( View view ) {
                        m_whichPlayer = ((XWListItem)view).getPosition();
                        showDialogFragment( DlgID.PLAYER_EDIT );
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

            if ( ! localOnlyGame()
                 && ((0 == m_gi.remoteCount() )
                     || (m_gi.nPlayers == m_gi.remoteCount()) ) ) {
                showDialogFragment( DlgID.FORCE_REMOTE );
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

            OnItemSelectedListener onSel = new Utils.OnNothingSelDoesNothing() {
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

            OnItemSelectedListener onSel = new Utils.OnNothingSelDoesNothing() {
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
                };

            String lang = DictLangCache.getLangName( m_activity, m_gi.dictLang );
            configSpinnerWDownload( m_langSpinner, adapter, onSel, lang );
        }
    }

    private void selLangChanged( String chosen )
    {
        m_gi.setLang( m_activity, DictLangCache
                      .getLangLangCode( m_activity, chosen ) );
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
            Log.w( TAG, "setSmartnessSpinner got %d from getRobotSmartness()",
                   m_gi.getRobotSmartness() );
            Assert.failDbg();
        }
        m_smartnessSpinner.setSelection( setting );
    }

    private int positionToSize( int position )
    {
        int result = 15;
        String[] sizes = getStringArray( R.array.board_sizes );
        Assert.assertTrueNR( position < sizes.length );
        if ( position < sizes.length ) {
            String sizeStr = sizes[position];
            result = Integer.parseInt( sizeStr.substring( 0, 2 ) );
        }
        return result;
    }

    private void setBoardsizeSpinner()
    {
        int selection = 0;
        String sizeStr = String.format( "%d", m_gi.boardSize );
        String[] sizes = getStringArray( R.array.board_sizes );
        for ( int ii = 0; ii < sizes.length; ++ii ) {
            if ( sizes[ii].startsWith( sizeStr ) ) {
                selection = ii;
                break;
            }
        }
        Assert.assertTrue( m_gi.boardSize == positionToSize(selection) );
        m_boardsizeSpinner.setSelection( selection );
    }

    private void buildDisabledsMap( XwJNI.GamePtr gamePtr )
    {
        if ( BuildConfig.DEBUG && !localOnlyGame() ) {
            if ( null == m_disabMap ) {
                m_disabMap = new HashMap<>();
                for ( CommsConnType typ : CommsConnType.values() ) {
                    boolean[] bools = new boolean[] {
                        XwJNI.comms_getAddrDisabled( gamePtr, typ, false ),
                        XwJNI.comms_getAddrDisabled( gamePtr, typ, true ),
                    };
                    m_disabMap.put( typ, bools );
                }
            }
        }
    }

    private void setDisableds()
    {
        if ( BuildConfig.DEBUG && !localOnlyGame() ) {
            LinearLayout disableds = (LinearLayout)findViewById( R.id.disableds );
            disableds.setVisibility( View.VISIBLE );

            for ( int ii = disableds.getChildCount() - 1; ii >= 0; --ii ) {
                View child = disableds.getChildAt( ii );
                if ( child instanceof DisablesItem ) {
                    disableds.removeView( child );
                }
            }
            for ( CommsConnType typ : m_conTypes ) {
                boolean[] bools = m_disabMap.get( typ );
                DisablesItem item = (DisablesItem)inflate( R.layout.disables_item );
                item.init( typ, bools );
                disableds.addView( item );
            }
        }
    }

    private void adjustPlayersLabel()
    {
        String label = getString( R.string.players_label_standalone );
        ((TextView)findViewById( R.id.players_label )).setText( label );
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
        Assert.failDbg();
        return 0;
    }

    private void saveChanges()
    {
        if ( !localOnlyGame() ) {
            Spinner dictSpinner = (Spinner)findViewById( R.id.dict_spinner );
            String name = (String)dictSpinner.getSelectedItem();
            if ( !m_browseText.equals( name ) ) {
                m_gi.dictName = name;
            }
        }

        m_gi.inDuplicateMode = getChecked( R.id.duplicate_check );
        m_gi.hintsNotAllowed = !getChecked( R.id.hints_allowed );
        m_gi.allowPickTiles = getChecked( R.id.pick_faceup );
        m_gi.timerEnabled = getChecked( R.id.use_timer );

        // Get timer value. It's per-move minutes in duplicate mode, otherwise
        // it's for the whole game.
        int seconds = 60 * getInt( R.id.timer_minutes_edit );
        if ( m_gi.inDuplicateMode ) {
            m_gi.gameSeconds = seconds;
        } else {
            m_gi.gameSeconds = seconds * m_gi.nPlayers;
        }

        int position = m_phoniesSpinner.getSelectedItemPosition();
        m_gi.phoniesAction = CurGameInfo.XWPhoniesChoice.values()[position];

        position = m_smartnessSpinner.getSelectedItemPosition();
        m_gi.setRobotSmartness(position * 49 + 1);

        position = m_boardsizeSpinner.getSelectedItemPosition();
        m_gi.boardSize = positionToSize( position );
        m_gi.traySize = Integer.parseInt( m_traysizeSpinner.getSelectedItem().toString() );

        m_car.conTypes = m_conTypes;
    } // saveChanges

    private void applyChanges( GameLock lock, boolean forceNew )
    {
        GameUtils.applyChanges( m_activity, m_gi, m_car, m_disabMap,
                                lock, forceNew );
        DBUtils.saveThumbnail( m_activity, lock, null ); // clear it
    }

    private void applyChanges( boolean forceNew )
    {
        if ( !isFinishing() ) {
            if ( null != m_jniThread ) {
                applyChanges( m_jniThread.getLock(), forceNew );
            } else {
                try ( GameLock lock = GameLock.lock( m_rowid, 100L ) ) {
                    applyChanges( lock, forceNew );
                } catch ( GameLock.GameLockedException gle ) {
                    Log.e( TAG, "applyChanges(): failed to get lock" );
                }
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
        return DeviceRole.SERVER_STANDALONE == m_gi.serverRole; // m_giOrig is null...
    }

    public static void editForResult( Delegator delegator,
                                      RequestCode requestCode,
                                      long rowID, boolean newGame )
    {
        Bundle bundle = new Bundle();
        bundle.putLong( GameUtils.INTENT_KEY_ROWID, rowID );
        bundle.putBoolean( INTENT_FORRESULT_NEWGAME, newGame );

        delegator
            .addFragmentForResult( GameConfigFrag.newInstance( delegator ),
                                   bundle, requestCode );
    }

    private void setConnLabel()
    {
        if ( localOnlyGame() ) {
            m_connLabel.setVisibility( View.GONE );
            m_changeConnButton.setVisibility( View.GONE );
        } else {
            String connString = m_conTypes.toString( m_activity, true );
            m_connLabel.setText( getString( R.string.connect_label_fmt, connString ) );
            // hide pick-face-up button for networked games
            findViewById( R.id.pick_faceup ).setVisibility( View.GONE );
        }
    }
}
