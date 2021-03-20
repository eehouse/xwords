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
import android.content.Context;
import android.content.DialogInterface.OnCancelListener;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface.OnDismissListener;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.TextView;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.eehouse.android.xw4.DBUtils.SentInvitesInfo;
import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.DlgDelegate.ActionPair;
import org.eehouse.android.xw4.Perms23.Perm;
import org.eehouse.android.xw4.Toolbar.Buttons;
import org.eehouse.android.xw4.jni.CommonPrefs;
import org.eehouse.android.xw4.jni.CommonPrefs.TileValueType;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;
import org.eehouse.android.xw4.jni.CurGameInfo;
import org.eehouse.android.xw4.jni.DUtilCtxt;
import org.eehouse.android.xw4.jni.GameSummary;
import org.eehouse.android.xw4.jni.JNIThread.JNICmd;
import org.eehouse.android.xw4.jni.JNIThread;
import org.eehouse.android.xw4.jni.JNIUtils;
import org.eehouse.android.xw4.jni.JNIUtilsImpl;
import org.eehouse.android.xw4.jni.LastMoveInfo;
import org.eehouse.android.xw4.jni.TransportProcs;
import org.eehouse.android.xw4.jni.UtilCtxt;
import org.eehouse.android.xw4.jni.UtilCtxtImpl;
import org.eehouse.android.xw4.jni.XwJNI.GamePtr;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;
import org.eehouse.android.xw4.TilePickAlert.TilePickState;
import org.eehouse.android.xw4.NFCUtils.Wrapper;

public class BoardDelegate extends DelegateBase
    implements TransportProcs.TPMsgHandler, View.OnClickListener,
               DwnldDelegate.DownloadFinishedListener,
               ConnStatusHandler.ConnStatusCBacks,
               Wrapper.Procs, InvitesNeededAlert.Callbacks {
    private static final String TAG = BoardDelegate.class.getSimpleName();

    private static final int SCREEN_ON_TIME = 10 * 60 * 1000; // 10 mins

    private static final String SAVE_MYSIS = TAG + "/MYSIS";

    static final String PAUSER_KEY = TAG + "/pauser";

    private Activity m_activity;
    private BoardView m_view;
    private GamePtr m_jniGamePtr;
    private CurGameInfo m_gi;
    private GameSummary m_summary;
    private boolean m_relayMissing;
    private Handler m_handler = null;
    private TimerRunnable[] m_timers;
    private Runnable m_screenTimer;
    private long m_rowid;
    private Toolbar m_toolbar;
    private View m_tradeButtons;
    private Button m_exchCommmitButton;
    private Button m_exchCancelButton;
    private SentInvitesInfo m_sentInfo;
    private Perms23.PermCbck m_permCbck;

    private CommsConnTypeSet m_connTypes = null;
    private String[] m_missingDevs;
    private int[] m_missingCounts;
    private boolean m_remotesAreRobots;
    private InviteMeans m_missingMeans = null;
    private boolean m_progressShown = false;
    private boolean m_isFirstLaunch;
    private boolean m_firingPrefs;
    private BoardUtilCtxt m_utils;
    private boolean m_gameOver = false;

    private volatile JNIThread m_jniThread;
    private JNIThread m_jniThreadRef;
    private boolean m_resumeSkipped;
    private boolean m_startSkipped;
    private JNIThread.GameStateInfo m_gsi;

    private boolean m_showedReInvite;
    private boolean m_overNotShown;
    private boolean m_dropRelayOnDismiss;
    private boolean m_haveStartedShowing;

    private Wrapper mNFCWrapper;
    private GameOverAlert mGameOverAlert; // how to clear after?

    public class TimerRunnable implements Runnable {
        private int m_why;
        private int m_when;
        private int m_handle;
        private TimerRunnable( int why, int when, int handle ) {
            m_why = why;
            m_when = when;
            m_handle = handle;
        }
        @Override
        public void run() {
            m_timers[m_why] = null;
            if ( null != m_jniThread ) {
                m_jniThread.handleBkgrnd( JNICmd.CMD_TIMER_FIRED,
                                          m_why, m_when, m_handle );
            }
        }
    }


    // Quick hack to manage a series of alerts meant to be presented
    // one-at-a-time in order. Each tests whether it's its turn, and if so
    // checks its conditions for being shown (e.g. NO_MEANS for no way to
    // communicate). If the conditions aren't met (no need to show alert), it
    // just sets the StartAlertOrder ivar to the next value and
    // exits. Otherwise it needs to ensure that however the alert it's posting
    // exits that ivar is incremented as well.
    private static enum StartAlertOrder { NBS_PERMS, NO_MEANS, INVITE, DONE, };

    private static class MySIS implements Serializable {
        String toastStr;
        String[] words;
        String getDict;
        int nMissing = -1;
        int nGuestDevs;
        boolean isServer;
        boolean inTrade;
        StartAlertOrder mAlertOrder = StartAlertOrder.values()[0];
    }
    private MySIS m_mySIS;

    private boolean alertOrderAt( StartAlertOrder ord )
    {
        boolean result = m_mySIS.mAlertOrder == ord;
        // Log.d( TAG, "alertOrderAt(%s) => %b (at %s)", ord, result,
        // m_mySIS.mAlertOrder );
        return result;
    }

    private void alertOrderIncrIfAt( StartAlertOrder ord )
    {
        Log.d( TAG, "alertOrderIncrIfAt(%s)", ord );
        if ( alertOrderAt( ord ) ) {
            m_mySIS.mAlertOrder = ord.values()[ord.ordinal() + 1];
            doNext();
        }
    }

    private void doNext()
    {
        switch ( m_mySIS.mAlertOrder ) {
        case NBS_PERMS:
            askNBSPermissions();
            break;
        case NO_MEANS:
            warnIfNoTransport();
            break;
        case INVITE:
            showInviteAlertIf();
            break;
        }
    }

    @Override
    protected Dialog makeDialog( DBAlert alert, Object[] params )
    {
        final DlgID dlgID = alert.getDlgID();
        Log.d( TAG, "makeDialog(%s)", dlgID.toString() );
        OnClickListener lstnr;
        AlertDialog.Builder ab = makeAlertBuilder(); // used everywhere...

        Dialog dialog;
        switch ( dlgID ) {
        case DLG_RETRY:
        case DLG_OKONLY: {
            int title = (Integer)params[0];
            String msg = (String)params[1];
            ab.setTitle( title )
                .setMessage( msg )
                .setPositiveButton( android.R.string.ok, null );
            if ( DlgID.DLG_RETRY == dlgID ) {
                lstnr = new OnClickListener() {
                        @Override
                        public void onClick( DialogInterface dlg,
                                             int whichButton ) {
                            handleViaThread( JNICmd.CMD_RESET );
                        }
                    };
                ab.setNegativeButton( R.string.button_retry, lstnr );
            }
            dialog = ab.create();
        }
            break;

        case DLG_USEDICT:
        case DLG_GETDICT: {
            int title = (Integer)params[0];
            String msg = (String)params[1];
            lstnr = new OnClickListener() {
                    @Override
                    public void onClick( DialogInterface dlg,
                                         int whichButton ) {
                        if ( DlgID.DLG_USEDICT == dlgID ) {
                            setGotGameDict( m_mySIS.getDict );
                        } else {
                            DwnldDelegate
                                .downloadDictInBack( m_activity, m_gi.dictLang,
                                                     m_mySIS.getDict,
                                                     BoardDelegate.this );
                        }
                    }
                };
            dialog = ab.setTitle( title )
                .setMessage( msg )
                .setPositiveButton( R.string.button_yes, lstnr )
                .setNegativeButton( R.string.button_no, null )
                .create();
        }
            break;

        case DLG_DELETED: {
            String gameName = GameUtils.getName( m_activity, m_rowid );
            CommsAddrRec.ConnExpl expl = params.length == 0 ? null
                : (CommsAddrRec.ConnExpl)params[0];
            String message = getString( R.string.msg_dev_deleted_fmt, gameName );
            if ( BuildConfig.NON_RELEASE && null != expl ) {
                message += "\n\n" + expl.getUserExpl( m_activity );
            }
            ab = ab.setTitle( R.string.query_title )
                .setMessage( message )
                .setPositiveButton( android.R.string.ok, null );
            lstnr = new OnClickListener() {
                    @Override
                    public void onClick( DialogInterface dlg,
                                         int whichButton ) {
                        deleteAndClose();
                    }
                };
            ab.setNegativeButton( R.string.button_delete, lstnr );
            dialog = ab.create();
        }
            break;

        case QUERY_TRADE:
        case QUERY_MOVE: {
            String msg = (String)params[0];
            lstnr = new OnClickListener() {
                    @Override
                    public void onClick( DialogInterface dialog,
                                         int whichButton ) {
                        handleViaThread( JNICmd.CMD_COMMIT, true, true );
                    }
                };
            dialog = ab.setMessage( msg )
                .setTitle( R.string.query_title )
                .setPositiveButton( R.string.button_yes, lstnr )
                .setNegativeButton( android.R.string.cancel, null )
                .create();
        }
            break;

        case NOTIFY_BADWORDS: {
            lstnr = new OnClickListener() {
                    @Override
                    public void onClick( DialogInterface dlg, int bx ) {
                        handleViaThread( JNICmd.CMD_COMMIT, true, false );
                    }
                };
            dialog = ab.setTitle( R.string.phonies_found_title )
                .setMessage( (String)params[0] )
                .setPositiveButton( R.string.button_yes, lstnr )
                .setNegativeButton( android.R.string.cancel, null )
                .create();
        }
            break;

        case DLG_BADWORDS:
        case DLG_SCORES: {
            int title = (Integer)params[0];
            String msg = (String)params[1];
            ab.setMessage( msg );
            if ( 0 != title ) {
                ab.setTitle( title );
            }
            ab.setPositiveButton( android.R.string.ok, null );
            if ( DlgID.DLG_SCORES == dlgID ) {
                if ( null != m_mySIS.words && m_mySIS.words.length > 0 ) {
                    String buttonTxt;
                    boolean studyOn = XWPrefs.getStudyEnabled( m_activity );
                    if ( m_mySIS.words.length == 1 ) {
                        int resID = studyOn
                            ? R.string.button_lookup_study_fmt
                            : R.string.button_lookup_fmt;
                        buttonTxt = getString( resID, m_mySIS.words[0] );
                    } else {
                        int resID = studyOn ? R.string.button_lookup_study
                            : R.string.button_lookup;
                        buttonTxt = getString( resID );
                    }
                    lstnr = new OnClickListener() {
                            @Override
                            public void onClick( DialogInterface dialog,
                                                 int whichButton ) {
                                    makeNotAgainBuilder( R.string.not_again_lookup,
                                                         R.string.key_na_lookup,
                                                         Action.LOOKUP_ACTION )
                                    .show();
                            }
                        };
                    ab.setNegativeButton( buttonTxt, lstnr );
                }
            }

            dialog = ab.create();
        }
            break;

        case ASK_PASSWORD: {
            final int player = (Integer)params[0];
            String name = (String)params[1];
            LinearLayout pwdLayout =
                (LinearLayout)inflate( R.layout.passwd_view );
            final EditText edit = (EditText)pwdLayout.findViewById( R.id.edit );
            ab.setTitle( getString( R.string.msg_ask_password_fmt, name ) )
                .setView( pwdLayout )
                .setPositiveButton( android.R.string.ok,
                                    new OnClickListener() {
                                        public void
                                            onClick( DialogInterface dlg,
                                                     int whichButton ) {
                                            String pwd = edit.getText().toString();
                                            handleViaThread( JNICmd.CMD_PASS_PASSWD,
                                                             player, pwd );
                                        }
                                    });
            dialog = ab.create();
        }
            break;

        case ASK_DUP_PAUSE: {
            final boolean isPause = (Boolean)params[0];
            final ConfirmPauseView pauseView =
                ((ConfirmPauseView)inflate( R.layout.pause_view ))
                .setIsPause( isPause );
            int buttonId = isPause ? R.string.board_menu_game_pause
                : R.string.board_menu_game_unpause;
            dialog = ab
                .setTitle(isPause ? R.string.pause_title : R.string.unpause_title)
                .setView( pauseView )
                .setPositiveButton( buttonId, new OnClickListener() {
                        @Override
                        public void
                            onClick( DialogInterface dlg,
                                     int whichButton ) {
                            String msg = pauseView.getMsg();
                            handleViaThread( isPause ? JNICmd.CMD_PAUSE
                                             : JNICmd.CMD_UNPAUSE, msg );
                        }
                    })
                .setNegativeButton( android.R.string.cancel, null )
                .create();
        }
            break;

        case QUERY_ENDGAME:
            dialog = ab.setTitle( R.string.query_title )
                .setMessage( R.string.ids_endnow )
                .setPositiveButton( R.string.button_yes,
                                    new OnClickListener() {
                                        public void
                                            onClick( DialogInterface dlg,
                                                     int item ) {
                                            handleViaThread(JNICmd.CMD_ENDGAME);
                                        }
                                    })
                .setNegativeButton( R.string.button_no, null )
                .create();
            break;
        case DLG_INVITE:
            dialog = getINAWrapper().make( alert, params );
            break;

        case ENABLE_NFC:
            dialog = NFCUtils.makeEnableNFCDialog( m_activity );
            break;

        default:
            dialog = super.makeDialog( alert, params );
            break;
        }

        return dialog;
    } // makeDialog

    private boolean mDeletePosted;
    private void postDeleteOnce( final CommsAddrRec.ConnExpl expl )
    {
        if ( !mDeletePosted ) {
            // PENDING: could clear this if user says "ok" rather than "delete"
            mDeletePosted = true;
            post( new Runnable() {
                    @Override
                    public void run() {
                        showDialogFragment( DlgID.DLG_DELETED, expl );
                    }
                } );
        }
    }

    public BoardDelegate( Delegator delegator, Bundle savedInstanceState )
    {
        super( delegator, savedInstanceState, R.layout.board, R.menu.board_menu );
        m_activity = delegator.getActivity();
    }

    private static int s_noLockCount = 0; // supports a quick debugging hack
    @Override
    protected void init( Bundle savedInstanceState )
    {
        m_isFirstLaunch = null == savedInstanceState;
        getBundledData( savedInstanceState );

        int devID = DevID.getNFCDevID( m_activity );
        mNFCWrapper = Wrapper.init( m_activity, this, devID );

        m_utils = new BoardUtilCtxt();
        // needs to be in sync with XWTimerReason
        m_timers = new TimerRunnable[UtilCtxt.NUM_TIMERS_PLUS_ONE];
        m_view = (BoardView)findViewById( R.id.board_view );
        if ( ! ABUtils.haveActionBar() ) {
            m_tradeButtons = findViewById( R.id.exchange_buttons );
            if ( null != m_tradeButtons ) {
                m_exchCommmitButton = (Button)
                    findViewById( R.id.exchange_commit );
                m_exchCommmitButton.setOnClickListener( this );
                m_exchCancelButton = (Button)
                    findViewById( R.id.exchange_cancel );
                m_exchCancelButton.setOnClickListener( this );
            }
        }

        Bundle args = getArguments();
        m_rowid = args.getLong( GameUtils.INTENT_KEY_ROWID, -1 );
        Log.i( TAG, "opening rowid %d", m_rowid );
        m_overNotShown = true;
    } // init

    private void getLock()
    {
        GameLock.getLockThen( m_rowid, 100L, new Handler(), // this doesn't unlock
                              new GameLock.GotLockProc() {
                @Override
                public void gotLock( GameLock lock ) {
                    if ( null == lock ) {
                        finish();
                        if ( BuildConfig.REPORT_LOCKS && ++s_noLockCount == 3 ) {
                            String msg = "BoardDelegate unable to get lock; holder stack: "
                                + GameLock.getHolderDump( m_rowid );
                            Log.e( TAG, msg );
                        }
                    } else {
                        s_noLockCount = 0;
                        m_jniThreadRef = JNIThread.getRetained( lock );
                        lock.release();

                        // see http://stackoverflow.com/questions/680180/where-to-stop- \
                        // destroy-threads-in-android-service-class
                        m_jniThreadRef.setDaemonOnce( true );
                        m_jniThreadRef.startOnce();

                        setBackgroundColor();
                        setKeepScreenOn();

                        if ( m_startSkipped ) {
                            doResume( true );
                        }
                        if ( m_resumeSkipped ) {
                            doResume( false );
                        }
                    }
                }
            } );
    } // getLock

    @Override
    protected void onStart()
    {
        super.onStart();
        if ( null != m_jniThreadRef ) {
            doResume( true );
        } else {
            m_startSkipped = true;
        }
    }

    @Override
    protected void onResume()
    {
        super.onResume();
        Wrapper.setResumed( mNFCWrapper, true );
        if ( null != m_jniThreadRef ) {
            doResume( false );
        } else {
            m_resumeSkipped = true;
            getLock();
        }
    }

    @Override
    protected void onPause()
    {
        Wrapper.setResumed( mNFCWrapper, false );
        closeIfFinishing( false );
        m_handler = null;
        ConnStatusHandler.setHandler( null );
        waitCloseGame( true );
        pauseGame();            // sets m_jniThread to null
        super.onPause();
    }

    @Override
    protected void onStop()
    {
        if ( isFinishing() ) {
            releaseThreadOnce();
        }
        super.onStop();
    }

    @Override
    protected void onDestroy()
    {
        closeIfFinishing( true );
        releaseThreadOnce();
        GamesListDelegate.boardDestroyed( m_rowid );
        super.onDestroy();
    }

    @Override
    public void finalize() throws java.lang.Throwable
    {
        // This logging never shows up. Likely a logging limit
        Log.d( TAG, "finalize()" );
        if ( releaseThreadOnce() ) {
            Log.e( TAG, "oops! Caught the leak" );
        }
        super.finalize();
    }

    private synchronized boolean releaseThreadOnce()
    {
        boolean needsRelease = null != m_jniThreadRef;
        if ( needsRelease ) {
            m_jniThreadRef.release();
            m_jniThreadRef = null;
        }
        return needsRelease;
    }

    @Override
    protected void onSaveInstanceState( Bundle outState )
    {
        outState.putSerializable( SAVE_MYSIS, m_mySIS );
        super.onSaveInstanceState( outState );
    }

    private void getBundledData( Bundle bundle )
    {
        if ( null != bundle ) {
            m_mySIS = (MySIS)bundle.getSerializable( SAVE_MYSIS );
        } else {
            m_mySIS = new MySIS();
        }
    }

    @Override
    protected void onActivityResult( RequestCode requestCode, int resultCode,
                                     Intent data )
    {
        if ( Activity.RESULT_CANCELED != resultCode ) {
            InviteMeans missingMeans = null;
            switch ( requestCode ) {
            case BT_INVITE_RESULT:
                missingMeans = InviteMeans.BLUETOOTH;
                break;
            case SMS_DATA_INVITE_RESULT:
                missingMeans = InviteMeans.SMS_DATA;
                break;
            case SMS_USER_INVITE_RESULT:
                missingMeans = InviteMeans.SMS_USER;
                break;
            case RELAY_INVITE_RESULT:
                missingMeans = InviteMeans.RELAY;
                break;
            case MQTT_INVITE_RESULT:
                missingMeans = InviteMeans.MQTT;
                break;
            case P2P_INVITE_RESULT:
                missingMeans = InviteMeans.WIFIDIRECT;
                break;
            }

            if ( null != missingMeans ) {
                // onActivityResult is called immediately *before* onResume --
                // meaning m_gi etc are still null.
                m_missingDevs = data.getStringArrayExtra( InviteDelegate.DEVS );
                m_missingCounts = data.getIntArrayExtra( InviteDelegate.COUNTS );
                m_remotesAreRobots = data.getBooleanExtra( InviteDelegate.RAR, false );
                m_missingMeans = missingMeans;
            }
        }
    }

    protected void onWindowFocusChanged( boolean hasFocus )
    {
        // This is not called when dialog fragment comes/goes away
        if ( hasFocus ) {
            if ( m_firingPrefs ) {
                m_firingPrefs = false;
                if ( null != m_jniThread ) {
                    handleViaThread( JNICmd.CMD_PREFS_CHANGE );
                }
                // in case of change...
                setBackgroundColor();
                setKeepScreenOn();
            } else {
                warnIfNoTransport();
                showInviteAlertIf();
            }
        }
    }

    // Invitations need to check phone state to decide whether to offer SMS
    // invitation. Complexity (showRationale) boolean is to prevent infinite
    // loop of showing the rationale over and over. Android will always tell
    // us to show the rationale, but if we've done it already we need to go
    // straight to asking for the permission.
    private void callInviteChoices()
    {
        Perms23.tryGetPermsNA( this, Perm.READ_PHONE_STATE,
                               R.string.phone_state_rationale,
                               R.string.key_na_perms_phonestate,
                               Action.ASKED_PHONE_STATE );
    }

    private void showInviteChoicesThen()
    {
        NetLaunchInfo nli = nliForMe();
        showInviteChoicesThen( Action.LAUNCH_INVITE_ACTION, nli,
                               m_mySIS.nMissing );
    }

    @Override
    protected void setTitle()
    {
        String title = GameUtils.getName( m_activity, m_rowid );
        if ( null != m_gi && m_gi.inDuplicateMode ) {
            title = LocUtils.getString( m_activity, R.string.dupe_title_fmt, title );
        }
        setTitle( title );
    }

    private void initToolbar()
    {
        // Wait until we're attached....
        if ( null != findViewById( R.id.tbar_parent_hor ) ) {
            if ( null == m_toolbar ) {
                m_toolbar = new Toolbar( m_activity, this );
                populateToolbar();
            }
        }
    }

    protected boolean onKeyDown( int keyCode, KeyEvent event )
    {
        if ( null != m_jniThread ) {
            XwJNI.XP_Key xpKey = keyCodeToXPKey( keyCode );
            if ( XwJNI.XP_Key.XP_KEY_NONE != xpKey ) {
                handleViaThread( JNICmd.CMD_KEYDOWN, xpKey );
            }
        }
        return false;
    }

    protected boolean onKeyUp( int keyCode, KeyEvent event )
    {
        boolean handled = false;
        if ( null != m_jniThread ) {
            XwJNI.XP_Key xpKey = keyCodeToXPKey( keyCode );
            if ( XwJNI.XP_Key.XP_KEY_NONE != xpKey ) {
                handleViaThread( JNICmd.CMD_KEYUP, xpKey );
                handled = true;
            }
        }
        return handled;
    }

    @Override
    public boolean onPrepareOptionsMenu( Menu menu )
    {
        boolean inTrade = false;
        MenuItem item;
        int strId;
        boolean enable;

        if ( null != m_gsi ) {
            inTrade = m_gsi.inTrade;
            menu.setGroupVisible( R.id.group_done, !inTrade );
            menu.setGroupVisible( R.id.group_exchange, inTrade );

            if ( UtilCtxt.TRAY_REVEALED == m_gsi.trayVisState ) {
                strId = R.string.board_menu_tray_hide;
            } else {
                strId = R.string.board_menu_tray_show;
            }
            item = menu.findItem( R.id.board_menu_tray );
            item.setTitle( getString( strId ) );

            Utils.setItemVisible( menu, R.id.board_menu_flip,
                                  m_gsi.visTileCount >= 1 );
            Utils.setItemVisible( menu, R.id.board_menu_juggle,
                                  m_gsi.canShuffle );
            Utils.setItemVisible( menu, R.id.board_menu_undo_current,
                                  m_gsi.canRedo );
            Utils.setItemVisible( menu, R.id.board_menu_hint_prev,
                                  m_gsi.canHint );
            Utils.setItemVisible( menu, R.id.board_menu_hint_next,
                                  m_gsi.canHint );
            Utils.setItemVisible( menu, R.id.board_menu_chat,
                                  m_gsi.canChat );
            Utils.setItemVisible( menu, R.id.board_menu_tray,
                                  !inTrade && m_gsi.canHideRack );
            Utils.setItemVisible( menu, R.id.board_menu_trade,
                                  m_gsi.canTrade );
            Utils.setItemVisible( menu, R.id.board_menu_undo_last,
                                  m_gsi.canUndo );

            Utils.setItemVisible( menu, R.id.board_menu_game_pause,
                                  m_gsi.canPause );
            Utils.setItemVisible( menu, R.id.board_menu_game_unpause,
                                  m_gsi.canUnpause );
        }

        Utils.setItemVisible( menu, R.id.board_menu_trade_cancel, inTrade );
        Utils.setItemVisible( menu, R.id.board_menu_trade_commit,
                              inTrade && m_gsi.tradeTilesSelected );
        Utils.setItemVisible( menu, R.id.board_menu_game_resign, !inTrade );

        if ( !inTrade ) {
            enable = null == m_gsi || m_gsi.curTurnSelected;
            item = menu.findItem( R.id.board_menu_done );
            item.setVisible( enable );
            if ( enable ) {
                if ( 0 >= m_view.curPending() ) {
                    strId = R.string.board_menu_pass;
                } else {
                    strId = R.string.board_menu_done;
                }
                item.setTitle( getString( strId ) );
            }
            if ( m_gameOver || DBUtils.gameOver( m_activity, m_rowid ) ) {
                m_gameOver = true;
                item = menu.findItem( R.id.board_menu_game_resign );
                item.setTitle( getString( R.string.board_menu_game_final ) );
            }
        }

        enable = m_gameOver && rematchSupported( false );
        Utils.setItemVisible( menu, R.id.board_menu_rematch, enable );

        enable = m_gameOver && !inArchiveGroup();
        Utils.setItemVisible( menu, R.id.board_menu_archive, enable );

        boolean netGame = null != m_gi
            && DeviceRole.SERVER_STANDALONE != m_gi.serverRole;
        Utils.setItemVisible( menu, R.id.gamel_menu_checkmoves, netGame );
        enable = netGame && null != m_gsi && 0 < m_gsi.nPendingMessages;
        Utils.setItemVisible( menu, R.id.board_menu_game_resend,  enable );

        enable = netGame && (BuildConfig.DEBUG
                             || XWPrefs.getDebugEnabled( m_activity ) );
        Utils.setItemVisible( menu, R.id.board_menu_game_netstats, enable );
        Utils.setItemVisible( menu, R.id.board_menu_game_invites, enable );

        enable = XWPrefs.getStudyEnabled( m_activity );
        Utils.setItemVisible( menu, R.id.board_menu_study, enable );

        return true;
    } // onPrepareOptionsMenu

    @Override
    public boolean onOptionsItemSelected( MenuItem item )
    {
        boolean handled = true;
        JNICmd cmd = JNICmd.CMD_NONE;
        Runnable proc = null;

        final int id = item.getItemId();
        switch ( id ) {
        case R.id.board_menu_done:
            int nTiles = XwJNI.model_getNumTilesInTray( m_jniGamePtr,
                                                        m_view.getCurPlayer() );
            if ( XWApp.MAX_TRAY_TILES > nTiles ) {
                makeNotAgainBuilder( R.string.not_again_done,
                                     R.string.key_notagain_done,
                                     Action.COMMIT_ACTION )
                    .show();
            } else {
                onPosButton( Action.COMMIT_ACTION, null );
            }
            break;

        case R.id.board_menu_rematch:
            doRematchIf();
            break;

        case R.id.board_menu_archive:
            showArchiveNA( false );
            break;

        case R.id.board_menu_trade_commit:
            cmd = JNICmd.CMD_COMMIT;
            break;
        case R.id.board_menu_trade_cancel:
            cmd = JNICmd.CMD_CANCELTRADE;
            break;

        case R.id.board_menu_hint_prev:
            cmd = JNICmd.CMD_PREV_HINT;
            break;
        case R.id.board_menu_hint_next:
            cmd = JNICmd.CMD_NEXT_HINT;
            break;
        case R.id.board_menu_juggle:
            cmd = JNICmd.CMD_JUGGLE;
            break;
        case R.id.board_menu_flip:
            cmd = JNICmd.CMD_FLIP;
            break;
        case R.id.board_menu_chat:
            startChatActivity();
            break;

        case R.id.board_menu_trade:
            String msg = getString( R.string.not_again_trading );
            int strID = ABUtils.haveActionBar() ? R.string.not_again_trading_menu
                : R.string.not_again_trading_buttons;
            msg += getString( strID );
            makeNotAgainBuilder( msg, R.string.key_notagain_trading,
                                 Action.START_TRADE_ACTION )
                .show();
            break;

        case R.id.board_menu_tray:
            cmd = JNICmd.CMD_TOGGLE_TRAY;
            break;
        case R.id.board_menu_study:
            StudyListDelegate.launchOrAlert( getDelegator(), m_gi.dictLang, this );
            break;
        case R.id.board_menu_game_netstats:
            handleViaThread( JNICmd.CMD_NETSTATS, R.string.netstats_title );
            break;
        case R.id.board_menu_game_invites:
            SentInvitesInfo sentInfo = DBUtils.getInvitesFor( m_activity, m_rowid );
            makeOkOnlyBuilder( sentInfo.getAsText( m_activity ) ).show();
            break;
        case R.id.board_menu_undo_current:
            cmd = JNICmd.CMD_UNDO_CUR;
            break;
        case R.id.board_menu_undo_last:
            makeConfirmThenBuilder( R.string.confirm_undo_last, Action.UNDO_LAST_ACTION )
                .show();
            break;

        case R.id.board_menu_game_pause:
        case R.id.board_menu_game_unpause:
            getConfirmPause( R.id.board_menu_game_pause == id );
            break;

            // small devices only
        case R.id.board_menu_dict:
            String dictName = m_gi.dictName( m_view.getCurPlayer() );
            DictBrowseDelegate.launch( getDelegator(), dictName );
            break;

        case R.id.board_menu_game_counts:
            handleViaThread( JNICmd.CMD_COUNTS_VALUES,
                             R.string.counts_values_title );
            break;
        case R.id.board_menu_game_left:
            handleViaThread( JNICmd.CMD_REMAINING, R.string.tiles_left_title );
            break;

        case R.id.board_menu_game_history:
            handleViaThread( JNICmd.CMD_HISTORY, R.string.history_title );
            break;

        case R.id.board_menu_game_resign:
            handleViaThread( JNICmd.CMD_FINAL, R.string.history_title );
            break;

        case R.id.board_menu_game_resend:
            handleViaThread( JNICmd.CMD_RESEND, true, false, true );
            break;

        case R.id.gamel_menu_checkmoves:
            makeNotAgainBuilder( R.string.not_again_sync,
                                 R.string.key_notagain_sync,
                                 Action.SYNC_ACTION )
                .show();
            break;

        case R.id.board_menu_file_prefs:
            m_firingPrefs = true;
            PrefsDelegate.launch( m_activity );
            break;

        default:
            Log.w( TAG, "menuitem %d not handled", id );
            handled = false;
        }

        if ( handled && cmd != JNICmd.CMD_NONE ) {
            handleViaThread( cmd );
        }
        return handled;
    }

    //////////////////////////////////////////////////
    // DlgDelegate.DlgClickNotify interface
    //////////////////////////////////////////////////

    @Override
    public boolean onPosButton( Action action, final Object[] params )
    {
        Log.d( TAG, "onPosButton(%s, %s)", action, DbgUtils.toStr( params ) );
        boolean handled = true;
        JNICmd cmd = null;
        switch ( action ) {
        case ENABLE_RELAY_DO_OR:
            RelayService.setEnabled( m_activity, true );
            break;
        case UNDO_LAST_ACTION:
            cmd = JNICmd.CMD_UNDO_LAST;
            break;
        case SYNC_ACTION:
            doSyncMenuitem();
            break;
        case SMS_CONFIG_ACTION:
            PrefsDelegate.launch( m_activity );
            break;
        case COMMIT_ACTION:
            cmd = JNICmd.CMD_COMMIT;
            break;
        case SHOW_EXPL_ACTION:
            showToast( m_mySIS.toastStr );
            m_mySIS.toastStr = null;
            break;
        case BUTTON_BROWSEALL_ACTION:
        case BUTTON_BROWSE_ACTION:
            String curDict = m_gi.dictName( m_view.getCurPlayer() );
            View button = m_toolbar.getButtonFor( Buttons.BUTTON_BROWSE_DICT );
            if ( Action.BUTTON_BROWSEALL_ACTION == action &&
                 DictsDelegate.handleDictsPopup( getDelegator(), button,
                                                 curDict, m_gi.dictLang ) ) {
                // do nothing
            } else {
                String selDict = DictsDelegate.prevSelFor( m_activity, m_gi.dictLang );
                if ( null == selDict ) {
                    selDict = curDict;
                }
                DictBrowseDelegate.launch( getDelegator(), selDict );
            }
            break;
        case PREV_HINT_ACTION:
            cmd = JNICmd.CMD_PREV_HINT;
            break;
        case NEXT_HINT_ACTION:
            cmd = JNICmd.CMD_NEXT_HINT;
            break;
        case JUGGLE_ACTION:
            cmd = JNICmd.CMD_JUGGLE;
            break;
        case FLIP_ACTION:
            cmd = JNICmd.CMD_FLIP;
            break;
        case UNDO_ACTION:
            cmd = JNICmd.CMD_UNDO_CUR;
            break;
        case VALUES_ACTION:
            doValuesPopup( m_toolbar.getButtonFor( Buttons.BUTTON_VALUES ) );
            break;
        case CHAT_ACTION:
            startChatActivity();
            break;
        case START_TRADE_ACTION:
            showToast( R.string.entering_trade );
            cmd = JNICmd.CMD_TRADE;
            break;
        case LOOKUP_ACTION:
            launchLookup( m_mySIS.words, m_gi.dictLang );
            break;
        case DROP_RELAY_ACTION:
            dropConViaAndRestart( CommsConnType.COMMS_CONN_RELAY );
            break;
        case DELETE_AND_EXIT:
            deleteAndClose();
            break;
        case DROP_SMS_ACTION:   // do nothing; work done in onNegButton case
            alertOrderIncrIfAt( StartAlertOrder.NBS_PERMS );
            break;

        case INVITE_SMS_DATA:
            int nMissing = (Integer)params[0];
            SentInvitesInfo info = (SentInvitesInfo)params[1];
            launchPhoneNumberInvite( nMissing, info,
                                     RequestCode.SMS_DATA_INVITE_RESULT );
            break;

        case ASKED_PHONE_STATE:
            showInviteChoicesThen();
            break;

        case BLANK_PICKED:
            TilePickAlert.TilePickState tps
                = (TilePickAlert.TilePickState)params[0];
            int[] newTiles = (int[])params[1];
            handleViaThread( JNICmd.CMD_SET_BLANK, tps.playerNum,
                             tps.col, tps.row, newTiles[0] );
            break;

        case TRAY_PICKED:
            tps = (TilePickAlert.TilePickState)params[0];
            newTiles = (int[])params[1];
            if ( tps.isInitial ) {
                handleViaThread( JNICmd.CMD_TILES_PICKED, tps.playerNum, newTiles );
            } else {
                handleViaThread( JNICmd.CMD_COMMIT, true, true, newTiles );
            }
            break;

        case DISABLE_DUALPANE:
            XWPrefs.setPrefsString( m_activity, R.string.key_force_tablet,
                                    getString(R.string.force_tablet_phone) );
            makeOkOnlyBuilder( R.string.after_restart ).show();
            break;

        case ARCHIVE_ACTION:
            boolean rematchAfter = params.length >= 1 && (Boolean)params[0];
            long curGroup = DBUtils.getGroupForGame( m_activity, m_rowid );
            archiveGame( !rematchAfter );
            if ( rematchAfter ) {
                doRematchIf( curGroup );      // closes game
            }
            break;

        case REMATCH_ACTION:
            if ( params.length >= 1 && (Boolean)params[0] ) {
                showArchiveNA( true );
            } else {
                doRematchIf();      // closes game
            }
            break;

        case ARCHIVE_SEL_ACTION:
            showArchiveNA( false );
            break;

        case DELETE_ACTION:
            if ( 0 < params.length && (Boolean)params[0] ) {
                deleteAndClose();
            } else {
                makeConfirmThenBuilder( R.string.confirm_delete,
                                        Action.DELETE_ACTION )
                    .setParams(true)
                    .show();
            }
            break;

        case LAUNCH_INVITE_ACTION:
            for ( Object obj : params ) {
                if ( obj instanceof CommsAddrRec ) {
                    tryOtherInvites( (CommsAddrRec)obj );
                } else {
                    break;
                }
            }
            break;

        case ENABLE_NBS_DO:
            post( new Runnable() {
                    @Override
                    public void run() {
                        retryNBSInvites( params );
                    }
                } );
            // FALLTHRU: so super gets called, before
        default:
            handled = super.onPosButton( action, params );
        }

        if ( null != cmd ) {
            handleViaThread( cmd );
        }

        return handled;
    }

    @Override
    public boolean onNegButton( Action action, Object[] params )
    {
        Log.d( TAG, "onNegButton(%s, %s)", action, DbgUtils.toStr( params ) );
        boolean handled = true;
        switch ( action ) {
        case ENABLE_RELAY_DO_OR:
            m_dropRelayOnDismiss = true;
            break;
        case DROP_SMS_ACTION:
            dropConViaAndRestart( CommsConnType.COMMS_CONN_SMS );
            break;
        case DELETE_AND_EXIT:
            finish();
            break;
        case ASKED_PHONE_STATE:
            showInviteChoicesThen();
            break;
        case INVITE_SMS_DATA:
            Perms23.Perm[] perms = (Perms23.Perm[])params[2];
            if ( Perms23.bannedWithWorkaround( m_activity, perms ) ) {
                int nMissing = (Integer)params[0];
                SentInvitesInfo info = (SentInvitesInfo)params[1];
                launchPhoneNumberInvite( nMissing, info,
                                         RequestCode.SMS_DATA_INVITE_RESULT );
            } else if ( Perms23.anyBanned( m_activity, perms ) ) {
                makeOkOnlyBuilder( R.string.sms_banned_ok_only )
                    .setActionPair(Action.PERMS_BANNED_INFO,
                                   R.string.button_more_info )
                    .show();
            }
            break;
        default:
            handled = super.onNegButton( action, params );
        }
        return handled;
    }

    @Override
    public boolean onDismissed( Action action, Object[] params )
    {
        Log.d( TAG, "onDismissed(%s, %s)", action, DbgUtils.toStr( params ) );
        boolean handled = true;
        switch ( action ) {
        case ENABLE_RELAY_DO_OR:
            if ( m_dropRelayOnDismiss ) {
                postDelayed( new Runnable() {
                        @Override
                        public void run() {
                            askDropRelay();
                        }
                    }, 10 );
            }
            break;
        case DELETE_AND_EXIT:
            finish();
            break;

        case BLANK_PICKED:
        case TRAY_PICKED:
            // If the user cancels the tile picker the common code doesn't
            // know, and won't put it up again as long as this game remains
            // loaded. There might be a way to fix that, but the safest thing
            // to do for now is to close. User will have to begin the process
            // of committing turn again on re-launching the game.
            finish();
            break;

        case DROP_SMS_ACTION:
            alertOrderIncrIfAt( StartAlertOrder.NBS_PERMS );
            break;

        case LAUNCH_INVITE_ACTION:
            showInviteAlertIf();
            break;

        default:
            handled = super.onDismissed( action, params );
            break;
        }
        return handled;
    }

    @Override
    public void inviteChoiceMade( Action action, InviteMeans means,
                                  Object... params )
    {
        if ( action == Action.LAUNCH_INVITE_ACTION ) {
            SentInvitesInfo info = 0 < params.length
                && params[0] instanceof SentInvitesInfo
                ? (SentInvitesInfo)params[0] : null;
            switch ( means ) {
            case NFC:
                if ( ! NFCUtils.nfcAvail( m_activity )[1] ) {
                    showDialogFragment( DlgID.ENABLE_NFC );
                } else {
                    makeOkOnlyBuilder( R.string.nfc_just_tap ).show();
                }
                break;
            case BLUETOOTH:
                BTInviteDelegate.launchForResult( m_activity, m_mySIS.nMissing, info,
                                                  RequestCode.BT_INVITE_RESULT );
                break;
            case SMS_DATA:
                Perm[] perms = { Perm.SEND_SMS, Perm.RECEIVE_SMS };
                Perms23.tryGetPerms( this, perms, R.string.sms_invite_rationale,
                                     Action.INVITE_SMS_DATA, m_mySIS.nMissing,
                                     info, perms );
                break;
            case RELAY:
            case MQTT:
                // These have been removed as options
                Assert.failDbg();
                break;
            case WIFIDIRECT:
                WiDirInviteDelegate.launchForResult( m_activity,
                                                     m_mySIS.nMissing, info,
                                                     RequestCode.P2P_INVITE_RESULT );
                break;
            case SMS_USER:
            case EMAIL:
            case CLIPBOARD:
                NetLaunchInfo nli = new NetLaunchInfo( m_activity, m_summary, m_gi,
                                                       1, // nPlayers
                                                       1 + m_mySIS.nGuestDevs ); // fc
                if ( m_relayMissing ) {
                    nli.removeAddress( CommsConnType.COMMS_CONN_RELAY );
                }
                switch ( means ) {
                case EMAIL:
                    GameUtils.launchEmailInviteActivity( m_activity, nli );
                    break;
                case SMS_USER:
                    GameUtils.launchSMSInviteActivity( m_activity, nli );
                    break;
                case CLIPBOARD:
                    GameUtils.inviteURLToClip( m_activity, nli );
                    break;
                }
                recordInviteSent( means, null );

                break;
            case QRCODE:
                break;          // nothing to do
            default:
                Assert.failDbg();
            }
        }
    }

    //////////////////////////////////////////////////
    // View.OnClickListener interface
    //////////////////////////////////////////////////
    @Override
    public void onClick( View view )
    {
        if ( view == m_exchCommmitButton ) {
            handleViaThread( JNICmd.CMD_COMMIT );
        } else if ( view == m_exchCancelButton ) {
            handleViaThread( JNICmd.CMD_CANCELTRADE );
        }
    }

    //////////////////////////////////////////////////
    // MultiService.MultiEventListener interface
    //////////////////////////////////////////////////
    @Override
    @SuppressWarnings("fallthrough")
    public void eventOccurred( MultiService.MultiEvent event, final Object ... args )
    {
        boolean doStopProgress = false;
        switch( event ) {
        case MESSAGE_ACCEPTED:
        case MESSAGE_REFUSED:
            ConnStatusHandler.
                updateStatusIn( m_activity, this, CommsConnType.COMMS_CONN_BT,
                                MultiService.MultiEvent.MESSAGE_ACCEPTED == event);
            doStopProgress = true;
            break;
        case MESSAGE_NOGAME:
            final int gameID = (Integer)args[0];
            if ( null != m_gi && gameID == m_gi.gameID && !isFinishing() ) {
                CommsAddrRec.ConnExpl expl = null;
                if ( 1 < args.length && args[1] instanceof CommsAddrRec.ConnExpl ) {
                    expl = (CommsAddrRec.ConnExpl)args[1];
                }
                postDeleteOnce( expl );
            }
            break;

        case BT_ENABLED:
            pingBTRemotes();
            break;

            // This can be BT or SMS.  In BT case there's a progress
            // thing going.  Not in SMS case.
        case NEWGAME_FAILURE:
            Log.w( TAG, "failed to create game" );
            break;
        case NEWGAME_DUP_REJECTED:
            doStopProgress = true;
            final String msg =
                getString( R.string.err_dup_invite_fmt, (String)args[0] );
            post( new Runnable() {
                    @Override
                    public void run() {
                        makeOkOnlyBuilder( msg ).show();
                    }
                } );
            break;

        case SMS_SEND_OK:
            ConnStatusHandler.showSuccessOut( this );
            break;
        case SMS_RECEIVE_OK:
            ConnStatusHandler.showSuccessIn( this );
            break;
        case SMS_SEND_FAILED:
        case SMS_SEND_FAILED_NORADIO:
        case SMS_SEND_FAILED_NOPERMISSION:
            // Don't bother warning if they're banned. Too frequent
            if ( Perms23.havePermissions( m_activity, Perm.SEND_SMS,
                                          Perm.RECEIVE_SMS ) ) {
                DbgUtils.showf( m_activity, R.string.sms_send_failed );
            }
            break;

        default:
            doStopProgress = true; // in case it's a BT invite
            super.eventOccurred( event, args );
            break;
        }

        if ( doStopProgress && m_progressShown ) {
            m_progressShown = false;
            stopProgress();
        }
    }

    //////////////////////////////////////////////////
    // TransportProcs.TPMsgHandler interface
    //////////////////////////////////////////////////

    @Override
    public void tpmRelayErrorProc( TransportProcs.XWRELAY_ERROR relayErr )
    {
        int strID = -1;
        DlgID dlgID = DlgID.NONE;
        boolean doToast = false;
        Object[] params = null;

        switch ( relayErr ) {
        case TOO_MANY:
            strID = R.string.msg_too_many;
            dlgID = DlgID.DLG_OKONLY;
            break;
        case NO_ROOM:
            strID = R.string.msg_no_room;
            dlgID = DlgID.DLG_RETRY;
            break;
        case DUP_ROOM:
            strID = R.string.msg_dup_room;
            dlgID = DlgID.DLG_OKONLY;
            break;
        case LOST_OTHER:
        case OTHER_DISCON:
            strID = R.string.msg_lost_other;
            doToast = true;
            break;

        case DEADGAME:
        case DELETED:
            postDeleteOnce( null );
            break;

        case OLDFLAGS:
        case BADPROTO:
        case RELAYBUSY:
        case SHUTDOWN:
        case TIMEOUT:
        case HEART_YOU:
        case HEART_OTHER:
            break;
        }

        if ( doToast ) {
            showToast( strID );
        } else if ( dlgID != DlgID.NONE ) {
            final DlgID dlgIDf = dlgID;
            final Object[] paramsF = null == params
                ? new Object[] {R.string.relay_alert, getString( strID )}
                : params;
            post( new Runnable() {
                    @Override
                    public void run() {
                        showDialogFragment( dlgIDf, paramsF );
                    }
                });
        }
    }

    @Override
    public void tpmCountChanged( final int newCount )
    {
        Log.d( TAG, "tpmCountChanged(%d)", newCount );
        ConnStatusHandler.updateMoveCount( m_activity, newCount );

        final GameOverAlert goAlert = mGameOverAlert;
        if ( null != goAlert ) {
            runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        goAlert.pendingCountChanged( newCount );
                    }
                });
        }
    }

    //////////////////////////////////////////////////
    // DwnldActivity.DownloadFinishedListener interface
    //////////////////////////////////////////////////
    @Override
    public void downloadFinished( String lang, final String name,
                                  boolean success )
    {
        if ( success ) {
            post( new Runnable() {
                    @Override
                    public void run() {
                        setGotGameDict( name );
                    }
                } );
        }
    }

    //////////////////////////////////////////////////
    // ConnStatusHandler.ConnStatusCBacks
    //////////////////////////////////////////////////
    @Override
    public void invalidateParent()
    {
        runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    m_view.invalidate();
                }
            });
    }

    @Override
    public void onStatusClicked()
    {
        onStatusClicked( m_jniGamePtr );
    }

    @Override
    public Handler getHandler()
    {
        return m_handler;
    }

    ////////////////////////////////////////////////////////////
    // NFCCardService.Wrapper.Procs
    ////////////////////////////////////////////////////////////
    @Override
    public void onReadingChange( boolean nowReading )
    {
        // Do we need this?
    }

    ////////////////////////////////////////////////////////////
    // InvitesNeededAlert.Callbacks
    ////////////////////////////////////////////////////////////
    @Override
    public DelegateBase getDelegate() { return this; }

    @Override
    public void onCloseClicked()
    {
        post( new Runnable() {
                @Override
                public void run() {
                    getINAWrapper().dismiss();
                    finish();
                }
            } );
    }

    @Override
    public void onInviteClicked()
    {
        getINAWrapper().dismiss();
        callInviteChoices();
    }

    @Override
    public void onInfoClicked()
    {
        SentInvitesInfo sentInfo = DBUtils.getInvitesFor( m_activity, m_rowid );
        String msg = sentInfo.getAsText( m_activity );
        makeOkOnlyBuilder( msg )
            .setTitle( R.string.title_invite_history )
            .setAction( Action.INVITE_INFO )
            .show();
    }

    @Override
    public long getRowID()
    {
        return m_rowid;
    }

    private byte[] getInvite()
    {
        byte[] result = null;
        if ( 0 < m_mySIS.nMissing // Isn't there a better test??
             && DeviceRole.SERVER_ISSERVER == m_gi.serverRole ) {
            NetLaunchInfo nli = new NetLaunchInfo( m_gi );
            Assert.assertTrue( 0 <= m_mySIS.nGuestDevs );
            nli.forceChannel = 1 + m_mySIS.nGuestDevs;

            for ( Iterator<CommsConnType> iter = m_connTypes.iterator();
                  iter.hasNext(); ) {
                CommsConnType typ = iter.next();
                switch ( typ ) {
                case COMMS_CONN_RELAY:
                    String room = m_summary.roomName;
                    Assert.assertNotNull( room );
                    String inviteID = String.format( "%X", m_gi.gameID );
                    nli.addRelayInfo( room, inviteID );
                    break;
                case COMMS_CONN_BT:
                    nli.addBTInfo();
                    break;
                case COMMS_CONN_SMS:
                    nli.addSMSInfo( m_activity );
                    break;
                case COMMS_CONN_P2P:
                    nli.addP2PInfo( m_activity );
                    break;
                case COMMS_CONN_NFC:
                    nli.addNFCInfo();
                    break;
                case COMMS_CONN_MQTT:
                    nli.addMQTTInfo();
                    break;
                default:
                    Log.w( TAG, "Not doing NFC join for conn type %s",
                           typ.toString() );
                }
            }
            result = nli.asByteArray();
        }
        return result;
    }

    private void launchPhoneNumberInvite( int nMissing, SentInvitesInfo info,
                                          RequestCode code )
    {
        SMSInviteDelegate.launchForResult( m_activity, nMissing, info, code );
    }

    private void deleteAndClose()
    {
        if ( null == m_jniThread ) { // test probably no longer necessary
            Assert.failDbg();
        } else {
            GameUtils.deleteGame( m_activity, m_jniThread.getLock(), false, false );
        }
        waitCloseGame( false );
        finish();
    }

    private void askDropRelay()
    {
        String msg = getString( R.string.confirm_drop_relay );
        if ( m_connTypes.contains(CommsConnType.COMMS_CONN_BT) ) {
            msg += " " + getString( R.string.confirm_drop_relay_bt );
        }
        if ( m_connTypes.contains(CommsConnType.COMMS_CONN_SMS) ) {
            msg += " " + getString( R.string.confirm_drop_relay_sms );
        }
        makeConfirmThenBuilder( msg, Action.DROP_RELAY_ACTION ).show();
    }

    private void dropConViaAndRestart( CommsConnType typ )
    {
        XwJNI.comms_dropHostAddr( m_jniGamePtr, typ );

        finish();

        GameUtils.launchGame( getDelegator(), m_rowid );
    }

    private void setGotGameDict( String getDict )
    {
        m_jniThread.setSaveDict( getDict );

        String msg = getString( R.string.reload_new_dict_fmt, getDict );
        showToast( msg );
        finish();
        GameUtils.launchGame( getDelegator(), m_rowid );
    }

    private XwJNI.XP_Key keyCodeToXPKey( int keyCode )
    {
        XwJNI.XP_Key xpKey = XwJNI.XP_Key.XP_KEY_NONE;
        switch( keyCode ) {
        case KeyEvent.KEYCODE_DPAD_CENTER:
        case KeyEvent.KEYCODE_ENTER:
            xpKey = XwJNI.XP_Key.XP_RETURN_KEY;
            break;
        case KeyEvent.KEYCODE_DPAD_DOWN:
            xpKey = XwJNI.XP_Key.XP_CURSOR_KEY_DOWN;
            break;
        case KeyEvent.KEYCODE_DPAD_LEFT:
            xpKey = XwJNI.XP_Key.XP_CURSOR_KEY_LEFT;
            break;
        case KeyEvent.KEYCODE_DPAD_RIGHT:
            xpKey = XwJNI.XP_Key.XP_CURSOR_KEY_RIGHT;
            break;
        case KeyEvent.KEYCODE_DPAD_UP:
            xpKey = XwJNI.XP_Key.XP_CURSOR_KEY_UP;
            break;
        case KeyEvent.KEYCODE_SPACE:
            xpKey = XwJNI.XP_Key.XP_RAISEFOCUS_KEY;
            break;
        }
        return xpKey;
    }

    private class BoardUtilCtxt extends UtilCtxtImpl {

        public BoardUtilCtxt()
        {
            super( m_activity );
        }

        @Override
        public void requestTime()
        {
            runOnUiThread( new Runnable() {
                    @Override
                    public void run() {
                        if ( null != m_jniThread ) {
                            m_jniThread.handleBkgrnd( JNICmd.CMD_DO );
                        }
                    }
                } );
        }

        @Override
        public void remSelected()
        {
            handleViaThread( JNICmd.CMD_REMAINING, R.string.tiles_left_title );
        }

        @Override
        public void timerSelected( boolean inDuplicateMode, final boolean canPause )
        {
            if ( inDuplicateMode ) {
                runOnUiThread( new Runnable() {
                        @Override
                        public void run() {
                            getConfirmPause( canPause );
                        }
                    } );
            }
        }

        @Override
        public void bonusSquareHeld( int bonus )
        {
            int id = 0;
            switch( bonus ) {
            case BONUS_DOUBLE_LETTER:
                id = R.string.bonus_l2x;
                break;
            case BONUS_DOUBLE_WORD:
                id = R.string.bonus_w2x;
                break;
            case BONUS_TRIPLE_LETTER:
                id = R.string.bonus_l3x;
                break;
            case BONUS_TRIPLE_WORD:
                id = R.string.bonus_w3x;
                break;
            default:
                Assert.failDbg();
            }

            if ( 0 != id ) {
                final String bonusStr = getString( id );
                post( new Runnable() {
                        @Override
                        public void run() {
                            showToast( bonusStr );
                        }
                    } );
            }
        }

        @Override
        public void informWordsBlocked( int nWords, final String words, final String dict )
        {
            runOnUiThread( new Runnable() {
                    @Override
                    public void run() {
                        String fmtd = TextUtils.join( ", ", wordsToArray( words ) );
                        String msg = LocUtils
                            .getString( m_activity, R.string.word_blocked_by_phony,
                                        fmtd, dict );
                        makeOkOnlyBuilder( msg ).show();
                    }
                } );
        }

        @Override
        public void playerScoreHeld( int player )
        {
            LastMoveInfo lmi = XwJNI.model_getPlayersLastScore( m_jniGamePtr, player );
            String expl = lmi.format( m_activity );
            if ( null == expl || 0 == expl.length() ) {
                expl = getString( R.string.no_moves_made );
            }
            final String text = expl;
            post( new Runnable() {
                    @Override
                    public void run() {
                        makeOkOnlyBuilder( text ).show();
                    }
                } );
        }

        @Override
        public void cellSquareHeld( final String words )
        {
            post( new Runnable() {
                    @Override
                    public void run() {
                        launchLookup( wordsToArray( words ), m_gi.dictLang );
                    }
                } );
        }

        @Override
        public void setTimer( int why, int when, int handle )
        {
            if ( null != m_timers[why] ) {
                removeCallbacks( m_timers[why] );
            }

            m_timers[why] = new TimerRunnable( why, when, handle );

            int inHowLong;
            switch ( why ) {
            case UtilCtxt.TIMER_COMMS:
            case UtilCtxt.TIMER_DUP_TIMERCHECK:
                inHowLong = when * 1000;
                break;
            case UtilCtxt.TIMER_TIMERTICK:
                inHowLong = 1000;   // when is 0 for TIMER_TIMERTICK
                break;
            default:
                inHowLong = 500;
            }
            postDelayed( m_timers[why], inHowLong );
        }

        @Override
        public void clearTimer( int why )
        {
            if ( null != m_timers[why] ) {
                removeCallbacks( m_timers[why] );
                m_timers[why] = null;
            }
        }

        private void startTP( final Action action,
                              final TilePickAlert.TilePickState tps )
        {
            runOnUiThread( new Runnable() {
                    @Override
                    public void run() {
                        show( TilePickAlert.newInstance( action, tps ) );
                    }
                } );
        }

        // This is supposed to be called from the jni thread
        @Override
        public void notifyPickTileBlank( int playerNum, int col, int row,
                                         String[] texts )
        {
            TilePickAlert.TilePickState tps =
                new TilePickAlert.TilePickState( playerNum, texts, col, row );
            startTP( Action.BLANK_PICKED, tps );
        }

        @Override
        public void informNeedPickTiles( boolean isInitial,
                                         int playerNum, int nToPick,
                                         String[] texts, int[] counts )
        {
            TilePickAlert.TilePickState tps
                = new TilePickAlert.TilePickState( isInitial, playerNum, nToPick,
                                                   texts, counts );
            startTP( Action.TRAY_PICKED, tps );
        }

        @Override
        public void informNeedPassword( int player, String name )
        {
            showDialogFragment( DlgID.ASK_PASSWORD, player, name );
        }

        @Override
        public void turnChanged( int newTurn )
        {
            if ( 0 <= newTurn ) {
                m_mySIS.nMissing = 0;
                post( new Runnable() {
                        @Override
                        public void run() {
                            makeNotAgainBuilder( R.string.not_again_turnchanged,
                                                 R.string.key_notagain_turnchanged )
                                .show();
                        }
                    } );
                handleViaThread( JNICmd.CMD_ZOOM, -8 );
                handleViaThread( JNICmd.CMD_SAVE );
            }
        }

        @Override
        public boolean engineProgressCallback()
        {
            // return true if engine should keep going
            JNIThread jnit = m_jniThread;
            return jnit != null && !jnit.busy();
        }

        @Override
        public void notifyMove( String msg )
        {
            showDialogFragment( DlgID.QUERY_MOVE, msg );
        }

        @Override
        public void notifyTrade( String[] tiles )
        {
            String dlgBytes =
                getQuantityString( R.plurals.query_trade_fmt, tiles.length,
                                   tiles.length, TextUtils.join( ", ", tiles ));
            showDialogFragment( DlgID.QUERY_TRADE, dlgBytes );
        }

        @Override
        public void notifyDupStatus( boolean amHost, final String msg )
        {
            final int key = amHost ? R.string.key_na_dupstatus_host
                : R.string.key_na_dupstatus_guest;
            runOnUiThread( new Runnable() {
                    @Override
                    public void run() {
                        makeNotAgainBuilder( msg, key )
                            .show();
                    }
                } );
        }

        @Override
        public void userError( int code )
        {
            int resid = 0;
            boolean asToast = false;
            String msg = null;
            switch( code ) {
            case ERR_TILES_NOT_IN_LINE:
                resid = R.string.str_tiles_not_in_line;
                break;
            case ERR_NO_EMPTIES_IN_TURN:
                resid = R.string.str_no_empties_in_turn;
                break;
            case ERR_TWO_TILES_FIRST_MOVE:
                resid = R.string.str_two_tiles_first_move;
                break;
            case ERR_TILES_MUST_CONTACT:
                resid = R.string.str_tiles_must_contact;
                break;
            case ERR_NOT_YOUR_TURN:
                resid = R.string.str_not_your_turn;
                break;
            case ERR_NO_PEEK_ROBOT_TILES:
                resid = R.string.str_no_peek_robot_tiles;
                break;
            case ERR_NO_EMPTY_TRADE:
                // This should not be possible as the button's
                // disabled when no tiles selected.
                Assert.failDbg();
                break;
            case ERR_TOO_MANY_TRADE:
                int nLeft = XwJNI.server_countTilesInPool( m_jniGamePtr );
                msg = getQuantityString( R.plurals.too_many_trade_fmt,
                                         nLeft, nLeft );
                break;
            case ERR_TOO_FEW_TILES_LEFT_TO_TRADE:
                resid = R.string.str_too_few_tiles_left_to_trade;
                break;
            case ERR_CANT_UNDO_TILEASSIGN:
                resid = R.string.str_cant_undo_tileassign;
                break;
            case ERR_CANT_HINT_WHILE_DISABLED:
                resid = R.string.str_cant_hint_while_disabled;
                break;
            case ERR_NO_PEEK_REMOTE_TILES:
                resid = R.string.str_no_peek_remote_tiles;
                break;
            case ERR_REG_UNEXPECTED_USER:
                resid = R.string.str_reg_unexpected_user;
                break;
            case ERR_SERVER_DICT_WINS:
                resid = R.string.str_server_dict_wins;
                break;
            case ERR_REG_SERVER_SANS_REMOTE:
                resid = R.string.str_reg_server_sans_remote;
                break;
            case ERR_NO_HINT_FOUND:
                resid = R.string.str_no_hint_found;
                asToast = true;
                break;
            }

            if ( null == msg && resid != 0 ) {
                msg = getString( resid );
            }

            if ( null != msg ) {
                if ( asToast ) {
                    final String msgf = msg;
                    runOnUiThread( new Runnable() {
                            @Override
                            public void run() {
                                showToast( msgf );
                            }
                        } );
                } else {
                    nonBlockingDialog( DlgID.DLG_OKONLY, msg );
                }
            }
        } // userError

        // Called from server_makeFromStream, whether there's something
        // missing or not.
        @Override
        public void informMissing( boolean isServer, CommsConnTypeSet connTypes,
                                   int nDevs, int nMissing )
        {
            // Log.d( TAG, "informMissing(isServer: %b, nDevs: %d; nMissing: %d",
            //        isServer, nDevs, nMissing );
            m_mySIS.nMissing = nMissing; // will be 0 unless isServer is true
            m_mySIS.nGuestDevs = nDevs;
            m_mySIS.isServer = isServer;
            m_connTypes = connTypes;

            runOnUiThread( new Runnable() {
                    @Override
                    public void run() {
                        showInviteAlertIf();
                    }
                } );
        }

        @Override
        public void informMove( int turn, String expl, String words )
        {
            m_mySIS.words = null == words? null : wordsToArray( words );
            nonBlockingDialog( DlgID.DLG_SCORES, expl );

            // Post a notification if in background, or play sound if not. But
            // do nothing for standalone case.
            if ( DeviceRole.SERVER_STANDALONE == m_gi.serverRole ) {
                // do nothing
            } else if ( isVisible() ) {
                Utils.playNotificationSound( m_activity );
            } else {
                GameUtils.BackMoveResult bmr = new GameUtils.BackMoveResult();
                bmr.m_lmi = XwJNI.model_getPlayersLastScore( m_jniGamePtr, turn );
                boolean[] locals = m_gi.playersLocal();
                GameUtils.postMoveNotification( m_activity, m_rowid,
                                                bmr, locals[turn] );
            }
        }

        @Override
        public void informUndo()
        {
            nonBlockingDialog( DlgID.DLG_OKONLY,
                               getString( R.string.remote_undone ) );
        }

        @Override
        public void informNetDict( int code, String oldName,
                                   String newName, String newSum,
                                   CurGameInfo.XWPhoniesChoice phonies )
        {
            // If it's same dict and same sum, we're good.  That
            // should be the normal case.  Otherwise: if same name but
            // different sum, notify and offer to upgrade.  If
            // different name, offer to install.
            String msg = null;
            if ( oldName.equals( newName ) ) {
                String oldSum = DictLangCache.getDictMD5Sum( m_activity,
                                                             oldName );
                if ( !oldSum.equals( newSum ) ) {
                    // Same dict, different versions
                    msg = getString( R.string.inform_dict_diffversion_fmt,
                                     oldName );
                }
            } else {
                // Different dict!  If we have the other one, switch
                // to it.  Otherwise offer to download
                DlgID dlgID;
                msg = getString( R.string.inform_dict_diffdict_fmt,
                                 oldName, newName, newName );
                if ( DictLangCache.haveDict( m_activity, code,
                                             newName ) ) {
                    dlgID = DlgID.DLG_USEDICT;
                } else {
                    dlgID = DlgID.DLG_GETDICT;
                    msg += getString( R.string.inform_dict_download );
                }
                m_mySIS.getDict = newName;
                nonBlockingDialog( dlgID, msg );
            }
        }

        @Override
        public void notifyGameOver()
        {
            m_gameOver = true;
            handleViaThread( JNICmd.CMD_POST_OVER );
        }

        @Override
        public void notifyIllegalWords( String dict, String[] words, int turn,
                                        boolean turnLost )
        {
            String wordsString = TextUtils.join( ", ", words );
            String message =
                getString( R.string.ids_badwords_fmt, wordsString, dict );

            if ( turnLost ) {
                showDialogFragment( DlgID.DLG_BADWORDS, R.string.badwords_title,
                                    message + getString( R.string.badwords_lost ) );
            } else {
                String msg = message + getString( R.string.badwords_accept );
                showDialogFragment( DlgID.NOTIFY_BADWORDS, msg );
            }
        }

        // Let's have this block in case there are multiple messages.  If
        // we don't block the jni thread will continue processing messages
        // and may stack dialogs on top of this one.  Including later
        // chat-messages.
        @Override
        public void showChat( final String msg, final int fromIndx,
                              String fromPlayer, final int tsSeconds )
        {
            runOnUiThread( new Runnable() {
                    @Override
                    public void run() {
                        DBUtils.appendChatHistory( m_activity, m_rowid, msg,
                                                   fromIndx, tsSeconds );
                        if ( ! ChatDelegate.append( m_rowid, msg,
                                                    fromIndx, tsSeconds ) ) {
                            startChatActivity();
                        }
                    }
                } );
        }

        @Override
        public String formatPauseHistory( int pauseTyp, int player,
                                          int whenPrev, int whenCur, String msg )
        {
            Log.d( TAG, "formatPauseHistory(prev: %d, cur: %d)", whenPrev, whenCur );
            String result = null;
            String name = 0 > player ? null : m_gi.players[player].name;
            switch ( pauseTyp ) {
            case DUtilCtxt.UNPAUSED:
                String interval = DateUtils
                    .formatElapsedTime( whenCur - whenPrev )
                    .toString();
                result = LocUtils.getString( m_activity, R.string.history_unpause_fmt,
                                             name, interval );
                break;
            case DUtilCtxt.PAUSED:
                result = LocUtils.getString( m_activity, R.string.history_pause_fmt,
                                             name );
                break;
            case DUtilCtxt.AUTOPAUSED:
                result = LocUtils.getString( m_activity, R.string.history_autopause );
                break;
            }

            if ( null != msg ) {
                result += " " + LocUtils
                    .getString( m_activity, R.string.history_msg_fmt, msg );
            }

            return result;
        }
    } // class BoardUtilCtxt

    private void doResume( boolean isStart )
    {
        boolean success = null != m_jniThreadRef;
        boolean firstStart = null == m_handler;
        if ( success && firstStart ) {
            m_handler = new Handler();

            success = m_jniThreadRef.configure( m_activity, m_view, m_utils, this,
                                                makeJNIHandler() );
            if ( success ) {
                m_jniGamePtr = m_jniThreadRef.getGamePtr(); // .retain()?
                Assert.assertNotNull( m_jniGamePtr );
            }
        }

        if ( success ) {
            try {
                resumeGame( isStart );
                if ( !isStart ) {
                    setKeepScreenOn();
                    ConnStatusHandler.setHandler( this );
                }
            } catch ( GameUtils.NoSuchGameException | NullPointerException ex ) {
                Log.ex( TAG, ex );
                success = false;
            }

        }
        if ( !success ) {
            finish();
        }
    }

    private Handler makeJNIHandler()
    {
        Handler handler = new Handler() {
                public void handleMessage( final Message msg ) {
                    switch( msg.what ) {
                    case JNIThread.DIALOG:
                        showDialogFragment( DlgID.DLG_OKONLY, msg.arg1,
                                            (String)msg.obj );
                        break;
                    case JNIThread.QUERY_ENDGAME:
                        showDialogFragment( DlgID.QUERY_ENDGAME );
                        break;
                    case JNIThread.TOOLBAR_STATES:
                        if ( null != m_jniThread ) {
                            m_gsi =
                                m_jniThread.getGameStateInfo();
                            updateToolbar();
                            if ( m_mySIS.inTrade != m_gsi.inTrade ) {
                                m_mySIS.inTrade = m_gsi.inTrade;
                            }
                            m_view.setInTrade( m_mySIS.inTrade );
                            adjustTradeVisibility();
                            invalidateOptionsMenuIf();
                        }
                        break;
                    case JNIThread.GAME_OVER:
                        if ( m_isFirstLaunch ) {
                            runOnUiThread( new Runnable() {
                                    @Override
                                    public void run() {
                                        boolean hasPending = 0 < XwJNI.
                                            comms_countPendingPackets( m_jniGamePtr );
                                        mGameOverAlert = GameOverAlert
                                            .newInstance( m_summary,
                                                          msg.arg1,
                                                          (String)msg.obj,
                                                          hasPending,
                                                          inArchiveGroup() );
                                        show( mGameOverAlert );
                                    }
                                } );
                        }
                        break;
                    case JNIThread.MSGS_SENT:
                        int nSent = (Integer)msg.obj;
                        showToast( getQuantityString( R.plurals.resent_msgs_fmt,
                                                      nSent, nSent ) );
                        break;

                    case JNIThread.GOT_PAUSE:
                        runOnUiThread( new Runnable() {
                                @Override
                                public void run() {
                                    makeOkOnlyBuilder( (String)msg.obj )
                                        .show();
                                }
                            } );
                        break;
                    }
                }
            };
        return handler;
    }

    private void resumeGame( boolean isStart )
    {
        if ( null == m_jniThread ) {
            m_jniThread = m_jniThreadRef.retain();
            m_gi = m_jniThread.getGI(); // this can be null, per Play Store report

            m_summary = m_jniThread.getSummary();

            Wrapper.setGameID( mNFCWrapper, m_gi.gameID );
            byte[] invite = getInvite();
            if ( null != invite ) {
                NFCUtils.addInvitationFor( invite, m_gi.gameID );
            }

            m_view.startHandling( m_activity, m_jniThread, m_connTypes );

            handleViaThread( JNICmd.CMD_START );

            if ( !CommonPrefs.getHideTitleBar( m_activity ) ) {
                setTitle();
            }

            initToolbar();
            adjustTradeVisibility();

            int flags = DBUtils.getMsgFlags( m_activity, m_rowid );
            if ( 0 != (GameSummary.MSG_FLAGS_CHAT & flags) ) {
                post( new Runnable() {
                        @Override
                        public void run() {
                            startChatActivity();
                        }
                    } );
            }
            if ( m_overNotShown ) {
                boolean auto = false;
                if ( 0 != (GameSummary.MSG_FLAGS_GAMEOVER & flags) ) {
                    m_gameOver = true;
                } else if ( DBUtils.gameOver( m_activity, m_rowid ) ) {
                    m_gameOver = true;
                    auto = true;
                }
                if ( m_gameOver ) {
                    m_overNotShown = false;
                    handleViaThread( JNICmd.CMD_POST_OVER, auto );
                }
            }
            if ( 0 != flags ) {
                DBUtils.setMsgFlags( m_activity, m_rowid,
                                     GameSummary.MSG_FLAGS_NONE );
            }

            Utils.cancelNotification( m_activity, m_rowid );

            askNBSPermissions();

            if ( m_gi.serverRole != DeviceRole.SERVER_STANDALONE ) {
                warnIfNoTransport();
                tickle( isStart );
                tryInvites();
            }

            Bundle args = getArguments();
            if ( args.getBoolean( PAUSER_KEY, false ) ) {
                getConfirmPause( true );
            }
        }
    } // resumeGame

    private void askNBSPermissions()
    {
        final StartAlertOrder thisOrder = StartAlertOrder.NBS_PERMS;
        if ( alertOrderAt( thisOrder ) // already asked?
             && m_summary.conTypes.contains( CommsConnType.COMMS_CONN_SMS ) ) {
            Perm[] nbsPerms = { Perm.SEND_SMS, Perm.RECEIVE_SMS };
            if ( Perms23.havePermissions( m_activity, nbsPerms ) ) {
                // We have them or a workaround; cool! proceed
                alertOrderIncrIfAt( thisOrder );
            } else {
                // Make sure these can be treated the same!!!
                Assert.assertTrue( nbsPerms.length == 2 &&
                                   nbsPerms[0].isBanned(m_activity)
                                   == nbsPerms[1].isBanned(m_activity) );

                m_permCbck = new Perms23.PermCbck() {
                        @Override
                        public void onPermissionResult( boolean allGood,
                                                        Map<Perm, Boolean> perms )
                        {
                            if ( allGood ) {
                                // Yay! nothing to do
                                alertOrderIncrIfAt( thisOrder );
                            } else {
                                boolean banned = Perm.SEND_SMS.isBanned(m_activity);
                                int explID = banned
                                    ? R.string.banned_nbs_perms : R.string.missing_sms_perms;
                                DlgDelegate.Builder builder =
                                    makeConfirmThenBuilder( explID, Action.DROP_SMS_ACTION );
                                if ( banned ) {
                                    builder.setActionPair( Action.PERMS_BANNED_INFO,
                                                           R.string.button_more_info )
                                        .setNAKey( R.string.key_na_sms_banned )
                                        ;
                                }
                                builder.setNegButton( R.string.remove_sms )
                                    .show();
                            }
                        }
                    };
                new Perms23.Builder( nbsPerms )
                    .asyncQuery( m_activity, m_permCbck );
            }
        } else {
            alertOrderIncrIfAt( thisOrder );
        }
    }

    private void tickle( boolean force )
    {
        for ( Iterator<CommsConnType> iter = m_connTypes.iterator();
              iter.hasNext(); ) {
            CommsConnType typ = iter.next();
            switch( typ ) {
            case COMMS_CONN_BT:
                pingBTRemotes();
                break;
            case COMMS_CONN_RELAY:
            case COMMS_CONN_SMS:
            case COMMS_CONN_P2P:
            case COMMS_CONN_NFC:
            case COMMS_CONN_MQTT:
                break;
            default:
                Log.w( TAG, "tickle: unexpected type %s", typ.toString() );
                Assert.failDbg();
            }
        }

        if ( 0 < m_connTypes.size() ) {
            handleViaThread( JNIThread.JNICmd.CMD_RESEND, force, true, false );
        }
    }

    private void pingBTRemotes()
    {
        if ( null != m_connTypes
             && m_connTypes.contains( CommsConnType.COMMS_CONN_BT ) ) {
            CommsAddrRec[] addrs = XwJNI.comms_getAddrs( m_jniGamePtr );
            for ( CommsAddrRec addr : addrs ) {
                if ( addr.contains( CommsConnType.COMMS_CONN_BT ) ) {
                    BTUtils.pingHost( m_activity, addr.bt_btAddr, m_gi.gameID );
                }
            }
        }
    }

    private void populateToolbar()
    {
        Assert.assertTrue( null != m_toolbar || !BuildConfig.DEBUG );
        if ( null != m_toolbar ) {
            m_toolbar.setListener( Buttons.BUTTON_BROWSE_DICT,
                                   R.string.not_again_browseall,
                                   R.string.key_na_browseall,
                                   Action.BUTTON_BROWSEALL_ACTION )
                .setLongClickListener( Buttons.BUTTON_BROWSE_DICT,
                                       R.string.not_again_browse,
                                       R.string.key_na_browse,
                                       Action.BUTTON_BROWSE_ACTION )
                .setListener( Buttons.BUTTON_HINT_PREV,
                              R.string.not_again_hintprev,
                              R.string.key_notagain_hintprev,
                              Action.PREV_HINT_ACTION )
                .setListener( Buttons.BUTTON_HINT_NEXT,
                              R.string.not_again_hintnext,
                              R.string.key_notagain_hintnext,
                              Action.NEXT_HINT_ACTION )
                .setListener( Buttons.BUTTON_JUGGLE,
                              R.string.not_again_juggle,
                              R.string.key_notagain_juggle,
                              Action.JUGGLE_ACTION )
                .setListener( Buttons.BUTTON_FLIP,
                              R.string.not_again_flip,
                              R.string.key_notagain_flip,
                              Action.FLIP_ACTION )
                .setListener( Buttons.BUTTON_VALUES,
                              R.string.not_again_values,
                              R.string.key_na_values,
                              Action.VALUES_ACTION )
                .setListener( Buttons.BUTTON_UNDO,
                              R.string.not_again_undo,
                              R.string.key_notagain_undo,
                              Action.UNDO_ACTION )
                .setListener( Buttons.BUTTON_CHAT,
                              R.string.not_again_chat,
                              R.string.key_notagain_chat,
                              Action.CHAT_ACTION )
                .installListeners();
        } else {
            Log.e( TAG, "not initing toolbar; still null" );
        }
    } // populateToolbar

    private void nonBlockingDialog( final DlgID dlgID, final String txt )
    {
        int dlgTitle = 0;
        switch ( dlgID ) {
        case DLG_OKONLY:
        case DLG_SCORES:
            break;
        case DLG_USEDICT:
        case DLG_GETDICT:
            dlgTitle = R.string.inform_dict_title;
            break;

        default:
            Assert.failDbg();
        }

        showDialogFragment( dlgID, dlgTitle, txt );
    }

    // This is failing sometimes, and so the null == m_inviteAlert test means
    // we never post it. BUT on a lot of devices without the test we wind up
    // trying over and over to put the thing up.
    private void showInviteAlertIf()
    {
        if ( alertOrderAt( StartAlertOrder.INVITE ) && ! isFinishing() ) {
            showOrHide( getINAWrapper() );
        }
    }

    private void showOrHide( InvitesNeededAlert.Wrapper wrapper )
    {
        boolean isRematch = null != m_summary && m_summary.hasRematchInfo();
        wrapper.showOrHide( m_mySIS.isServer, m_mySIS.nMissing, isRematch );
    }

    private InvitesNeededAlert.Wrapper mINAWrapper;
    private InvitesNeededAlert.Wrapper getINAWrapper()
    {
        if ( null == mINAWrapper ) {
            mINAWrapper = new InvitesNeededAlert.Wrapper( this );
            showOrHide( mINAWrapper );
        }
        return mINAWrapper;
    }

    private boolean doZoom( int zoomBy )
    {
        boolean handled = null != m_jniThread;
        if ( handled ) {
            handleViaThread( JNICmd.CMD_ZOOM, zoomBy );
        }
        return handled;
    }

    private void startChatActivity()
    {
        int curPlayer = XwJNI.board_getSelPlayer( m_jniGamePtr );
        String[] names = m_gi.playerNames();
        boolean[] locs = m_gi.playersLocal(); // to convert old histories
        ChatDelegate.start( getDelegator(), m_rowid, curPlayer,
                            names, locs );
    }

    private void doValuesPopup( View button )
    {
        final int FAKE_GROUP = 100;
        final TileValueType selType = CommonPrefs.get(m_activity).tvType;
        PopupMenu popup = new PopupMenu( m_activity, button );
        Menu menu = popup.getMenu();

        final Map<MenuItem, TileValueType> map = new HashMap<>();
        for ( TileValueType typ : TileValueType.values() ) {
            MenuItem item = menu.add( FAKE_GROUP, Menu.NONE, Menu.NONE, typ.getExpl() );
            map.put( item, typ );

            if ( selType == typ ) {
                item.setChecked(true);
            }
        }
        menu.setGroupCheckable( FAKE_GROUP, true, true );

        popup.setOnMenuItemClickListener( new PopupMenu.OnMenuItemClickListener() {
                @Override
                public boolean onMenuItemClick( MenuItem item ) {
                    TileValueType typ = map.get( item );
                    XWPrefs.setPrefsInt( m_activity,
                                         R.string.key_tile_valuetype,
                                         typ.ordinal() );
                    handleViaThread( JNICmd.CMD_PREFS_CHANGE );
                    return true;
                }
            } );

        popup.show();
    }

    private void getConfirmPause( boolean isPause )
    {
        showDialogFragment( DlgID.ASK_DUP_PAUSE, isPause );
    }

    private void closeIfFinishing( boolean force )
    {
        if ( null == m_handler ) {
            // DbgUtils.logf( "closeIfFinishing(): already closed" );
        } else if ( force || isFinishing() ) {
            // DbgUtils.logf( "closeIfFinishing: closing rowid %d", m_rowid );
            m_handler = null;
            ConnStatusHandler.setHandler( null );
            waitCloseGame( true );
        } else {
            handleViaThread( JNICmd.CMD_SAVE );
            // DbgUtils.logf( "closeIfFinishing(): not finishing (yet)" );
        }
    }

    private void pauseGame()
    {
        if ( null != m_jniThread ) {
            m_jniThread.release();
            m_jniThread = null;

            m_view.stopHandling();
        }
    }

    private void waitCloseGame( boolean save )
    {
        pauseGame();
        if ( null != m_jniThread ) {
            // m_jniGamePtr.release();
            // m_jniGamePtr = null;

            // m_gameLock.unlock(); // likely the problem
        }
    }

    private void warnIfNoTransport()
    {
        if ( null != m_connTypes && alertOrderAt( StartAlertOrder.NO_MEANS ) ) {
            if ( m_connTypes.contains( CommsConnType.COMMS_CONN_SMS ) ) {
                if ( !XWPrefs.getNBSEnabled( m_activity ) ) {
                    makeConfirmThenBuilder( R.string.warn_sms_disabled,
                                            Action.ENABLE_NBS_ASK )
                        .setPosButton( R.string.button_enable_sms )
                        .setNegButton( R.string.button_later )
                        .show();
                }
            }
            if ( m_connTypes.contains( CommsConnType.COMMS_CONN_RELAY ) ) {
                if ( !XWPrefs.getRelayEnabled( m_activity ) ) {
                    m_dropRelayOnDismiss = false;
                    String msg = getString( R.string.warn_relay_disabled )
                        + "\n\n" + getString( R.string.warn_relay_remove );
                    makeConfirmThenBuilder( msg, Action.ENABLE_RELAY_DO_OR )
                        .setPosButton( R.string.button_enable_relay )
                        .setNegButton( R.string.newgame_drop_relay )
                        .show();
                }
            }

            if ( m_connTypes.isEmpty() ) {
                askNoAddrsDelete();
            } else {
                alertOrderIncrIfAt( StartAlertOrder.NO_MEANS );
            }
        }
    }

    private void tryInvites()
    {
        if ( 0 < m_mySIS.nMissing && m_summary.hasRematchInfo() ) {
            tryRematchInvites( false );
        } else if ( 0 < m_mySIS.nMissing && m_summary.hasInviteInfo() ) {
            tryOtherInvites();
        } else if ( null != m_missingDevs ) {
            Assert.assertNotNull( m_missingMeans );
            String gameName = GameUtils.getName( m_activity, m_rowid );
            for ( int ii = 0; ii < m_missingDevs.length; ++ii ) {
                String dev = m_missingDevs[ii];
                int nPlayers = m_missingCounts[ii];
                Assert.assertTrue( 0 <= m_mySIS.nGuestDevs );
                int forceChannel = ii + m_mySIS.nGuestDevs + 1;
                NetLaunchInfo nli = new NetLaunchInfo( m_activity, m_summary, m_gi,
                                                       nPlayers, forceChannel )
                    .setRemotesAreRobots( m_remotesAreRobots );

                if ( m_relayMissing ) {
                    nli.removeAddress( CommsConnType.COMMS_CONN_RELAY );
                }

                switch ( m_missingMeans ) {
                case BLUETOOTH:
                    if ( ! m_progressShown ) {
                        m_progressShown = true;
                        String progMsg = BTUtils.nameForAddr( dev );
                        progMsg = getString( R.string.invite_progress_bt_fmt, progMsg );
                        startProgress( R.string.invite_progress_title, progMsg,
                                       new OnCancelListener() {
                                           public void onCancel( DialogInterface dlg )
                                           {
                                               m_progressShown = false;
                                           }
                                       });
                    }
                    BTUtils.inviteRemote( m_activity, dev, nli );
                    break;
                case SMS_DATA:
                    sendNBSInviteIf( dev, nli, true );
                    dev = null; // don't record send a second time
                    break;
                case RELAY:
                    try {
                        int destDevID = Integer.parseInt( dev ); // failing
                        RelayService.inviteRemote( m_activity, m_jniGamePtr, destDevID,
                                                   null, nli );
                    } catch (NumberFormatException nfi) {
                        Log.ex( TAG, nfi );
                    }
                    break;
                case WIFIDIRECT:
                    WiDirService.inviteRemote( m_activity, dev, nli );
                    break;
                case MQTT:
                    MQTTUtils.inviteRemote( m_activity, dev, nli );
                    break;
                default:
                    Assert.failDbg();
                    break;
                }

                if ( null != dev ) {
                    recordInviteSent( m_missingMeans, dev );
                }
            }
            m_missingDevs = null;
            m_missingCounts = null;
            m_missingMeans = null;
        }
    }

    private boolean m_needsResize = false;
    private void updateToolbar()
    {
        if ( null != m_toolbar ) {
            m_toolbar.update( Buttons.BUTTON_FLIP, m_gsi.visTileCount >= 1 )
                .update( Buttons.BUTTON_VALUES, m_gsi.visTileCount >= 1 )
                .update( Buttons.BUTTON_JUGGLE, m_gsi.canShuffle )
                .update( Buttons.BUTTON_UNDO, m_gsi.canRedo )
                .update( Buttons.BUTTON_HINT_PREV, m_gsi.canHint )
                .update( Buttons.BUTTON_HINT_NEXT, m_gsi.canHint )
                .update( Buttons.BUTTON_CHAT, m_gsi.canChat )
                .update( Buttons.BUTTON_BROWSE_DICT,
                         null != m_gi.dictName( m_view.getCurPlayer() ) );

            int count = m_toolbar.enabledCount();
            if ( 0 == count ) {
                m_needsResize = true;
            } else if ( m_needsResize && 0 < count ) {
                m_needsResize = false;
                m_view.orientationChanged();
            }
        }
    }

    private void adjustTradeVisibility()
    {
        if ( null != m_toolbar ) {
            m_toolbar.setVisible( !m_mySIS.inTrade );
        }
        if ( null != m_tradeButtons ) {
            m_tradeButtons.setVisibility( m_mySIS.inTrade? View.VISIBLE : View.GONE );
        }
        if ( m_mySIS.inTrade && null != m_exchCommmitButton ) {
            m_exchCommmitButton.setEnabled( m_gsi.tradeTilesSelected );
        }
    }

    private void setBackgroundColor()
    {
        View view = findViewById( R.id.board_root );
        // Google's reported an NPE here, so test
        if ( null != view ) {
            int back = CommonPrefs.get( m_activity )
                .otherColors[CommonPrefs.COLOR_BACKGRND];
            view.setBackgroundColor( back );
        }
    }

    private void setKeepScreenOn()
    {
        boolean keepOn = CommonPrefs.getKeepScreenOn( m_activity );
        m_view.setKeepScreenOn( keepOn );

        if ( keepOn ) {
            if ( null == m_screenTimer ) {
                m_screenTimer = new Runnable() {
                        @Override
                        public void run() {
                            if ( null != m_view ) {
                                m_view.setKeepScreenOn( false );
                            }
                        }
                    };
            }
            removeCallbacks( m_screenTimer ); // needed?
            postDelayed( m_screenTimer, SCREEN_ON_TIME );
        }
    }

    @Override
    protected boolean post( Runnable runnable )
    {
        boolean canPost = null != m_handler;
        if ( canPost ) {
            m_handler.post( runnable );
        } else {
            Log.w( TAG, "post(): dropping b/c handler null" );
            DbgUtils.printStack( TAG );
        }
        return canPost;
    }

    private void postDelayed( Runnable runnable, int when )
    {
        if ( null != m_handler ) {
            m_handler.postDelayed( runnable, when );
        } else {
            Log.w( TAG, "postDelayed: dropping %d because handler null", when );
        }
    }

    private void removeCallbacks( Runnable which )
    {
        if ( null != m_handler ) {
            m_handler.removeCallbacks( which );
        } else {
            Log.w( TAG, "removeCallbacks: dropping %h because handler null",
                   which );
        }
    }

    private String[] wordsToArray( String words )
    {
        String[] tmp = TextUtils.split( words, "\n" );
        List<String> list = new ArrayList<>();
        for ( String one : tmp ) {
            if ( 0 < one.length() ) {
                list.add( one );
            }
        }
        return list.toArray( new String[list.size()] );
    }

    private boolean inArchiveGroup()
    {
        long archiveGroup = DBUtils.getArchiveGroup( m_activity );
        long curGroup = DBUtils.getGroupForGame( m_activity, m_rowid );
        return curGroup == archiveGroup;
    }

    private void showArchiveNA( boolean rematchAfter )
    {
        makeNotAgainBuilder( R.string.not_again_archive,
                             R.string.key_na_archive,
                             Action.ARCHIVE_ACTION )
            .setParams( rematchAfter )
            .show();
    }

    private void archiveGame( boolean closeAfter )
    {
        long gid = DBUtils.getArchiveGroup( m_activity );
        DBUtils.moveGame( m_activity, m_rowid, gid );
        if ( closeAfter ) {
            waitCloseGame( false );
            finish();
        }
    }

    // For now, supported if standalone or either BT or SMS used for transport
    private boolean rematchSupported( boolean showMulti )
    {
        return null != m_summary
            && rematchSupported( showMulti ? m_activity : null,
                                 m_summary );
    }

    public static boolean rematchSupported( Context context, long rowID )
    {
        GameSummary summary = GameUtils.getSummary( context, rowID, 1 );
        return null != summary && rematchSupported( null, summary );
    }

    private static boolean rematchSupported( Context context,
                                             GameSummary summary )

    {
        return rematchSupported( context, false, summary );
    }

    private static boolean rematchSupported( Context context, boolean supported,
                                             GameSummary summary )
    {
        // standalone games are easy to rematch
        supported = summary.serverRole == DeviceRole.SERVER_STANDALONE;

        if ( !supported ) {
            if ( 2 == summary.nPlayers ) {
                if ( !summary.anyMissing() ) {
                    CommsConnTypeSet connTypes = summary.conTypes;
                    supported = connTypes.contains( CommsConnType.COMMS_CONN_BT )
                        || connTypes.contains( CommsConnType.COMMS_CONN_SMS  )
                        || connTypes.contains( CommsConnType.COMMS_CONN_RELAY )
                        || connTypes.contains( CommsConnType.COMMS_CONN_MQTT )
                        || connTypes.contains( CommsConnType.COMMS_CONN_P2P );
                }
            } else if ( null != context ) {
                // show the button if people haven't dismissed the hint yet
                supported = ! XWPrefs
                    .getPrefsBoolean( context,
                                      R.string.key_na_rematch_two_only,
                                      false );
            }
        }
        return supported;
    }

    private void doRematchIf()
    {
        doRematchIf( DBUtils.GROUPID_UNSPEC );
    }

    private void doRematchIf( long groupID )
    {
        doRematchIf( m_activity, this, m_rowid, groupID, m_summary,
                     m_gi, m_jniGamePtr );
    }

    private static void doRematchIf( Activity activity, DelegateBase dlgt,
                                     long rowid, long groupID,
                                     GameSummary summary, CurGameInfo gi,
                                     GamePtr jniGamePtr )
    {
        boolean doIt = true;
        String phone = null;
        String btAddr = null;
        String relayID = null;
        String p2pMacAddress = null;
        String mqttDevID = null;
        if ( DeviceRole.SERVER_STANDALONE == gi.serverRole ) {
            // nothing to do??
        } else if ( 2 != gi.nPlayers ) {
            Assert.assertNotNull( dlgt );
            if ( null != dlgt ) {
                dlgt.makeNotAgainBuilder( R.string.not_again_rematch_two_only,
                                          R.string.key_na_rematch_two_only )
                    .show();
            }
            doIt = false;
        } else {
            CommsAddrRec[] addrs = XwJNI.comms_getAddrs( jniGamePtr );
            for ( int ii = 0; ii < addrs.length; ++ii ) {
                CommsAddrRec addr = addrs[ii];
                if ( addr.contains( CommsConnType.COMMS_CONN_BT ) ) {
                    Assert.assertNull( btAddr );
                    btAddr = addr.bt_btAddr;
                }
                if ( addr.contains( CommsConnType.COMMS_CONN_SMS ) ) {
                    Assert.assertNull( phone );
                    phone = addr.sms_phone;
                }
                if ( addr.contains( CommsConnType.COMMS_CONN_RELAY ) ) {
                    Assert.assertNull( relayID );
                    relayID = XwJNI.comms_formatRelayID( jniGamePtr, ii );
                }
                if ( addr.contains( CommsConnType.COMMS_CONN_P2P ) ) {
                    Assert.assertNull( p2pMacAddress );
                    p2pMacAddress = addr.p2p_addr;
                }
                if ( addr.contains( CommsConnType.COMMS_CONN_MQTT ) ) {
                    Assert.assertNull( mqttDevID );
                    mqttDevID = addr.mqtt_devID;
                }
            }
        }

        if ( doIt ) {
            CommsConnTypeSet connTypes = summary.conTypes;
            String newName = summary.getRematchName( activity );
            Intent intent = GamesListDelegate
                .makeRematchIntent( activity, rowid, groupID, gi, connTypes,
                                    btAddr, phone, relayID, p2pMacAddress,
                                    mqttDevID, newName );
            if ( null != intent ) {
                activity.startActivity( intent );
            }
        }
    }

    public static void setupRematchFor( Activity activity, long rowID )
    {
        GameSummary summary = null;
        CurGameInfo gi = null;

        try ( JNIThread thread = JNIThread.getRetained( rowID ) ) {
            if ( null != thread ) {
                try ( GamePtr gamePtr = thread.getGamePtr().retain() ) {
                    summary = thread.getSummary();
                    gi = thread.getGI();
                    setupRematchFor( activity, gamePtr, summary, gi );
                }
            } else {
                try ( GameLock lock = GameLock.tryLockRO( rowID ) ) {
                    if ( null != lock ) {
                        summary = DBUtils.getSummary( activity, lock );
                        gi = new CurGameInfo( activity );
                        try ( GamePtr gamePtr = GameUtils
                              .loadMakeGame( activity, gi, lock ) ) {
                            setupRematchFor( activity, gamePtr, summary, gi );
                        }
                    } else {
                        DbgUtils.toastNoLock( TAG, activity, rowID,
                                              "setupRematchFor(%d)", rowID );
                    }
                }
            }
        }
    }

    private static void setupRematchFor( Activity activity, GamePtr gamePtr,
                                         GameSummary summary, CurGameInfo gi )
    {
        if ( null != gamePtr ) {
            doRematchIf( activity, null, gamePtr.getRowid(),
                         DBUtils.GROUPID_UNSPEC, summary, gi, gamePtr );
        } else {
            Log.w( TAG, "setupRematchFor(): unable to lock game" );
        }
    }

    private NetLaunchInfo nliForMe()
    {
        int numHere = 1;
        int forceChannel = 1 + m_mySIS.nGuestDevs;
        NetLaunchInfo nli = new NetLaunchInfo( m_activity, m_summary, m_gi,
                                               numHere, forceChannel );
        return nli;
    }

    // Return true if anything sent
    private boolean tryRematchInvites( boolean force )
    {
        if ( !force ) {
            SentInvitesInfo info = DBUtils.getInvitesFor( m_activity, m_rowid );
            force = 0 == info.getMinPlayerCount();
        }

        if ( force ) {
            Assert.assertNotNull( m_summary );
            Assert.assertNotNull( m_gi );
            // only supports a single invite for now!
            NetLaunchInfo nli = nliForMe();

            String value;
            value = m_summary.getStringExtra( GameSummary.EXTRA_REMATCH_PHONE );
            if ( null != value ) {
                sendNBSInviteIf( value, nli, true );
            }
            value = m_summary.getStringExtra( GameSummary.EXTRA_REMATCH_BTADDR );
            if ( null != value ) {
                BTUtils.inviteRemote( m_activity, value, nli );
                recordInviteSent( InviteMeans.BLUETOOTH, value );
            }
            value = m_summary.getStringExtra( GameSummary.EXTRA_REMATCH_RELAY );
            if ( null != value ) {
                RelayService.inviteRemote( m_activity, m_jniGamePtr, 0, value, nli );
                recordInviteSent( InviteMeans.RELAY, value );
            }
            value = m_summary.getStringExtra( GameSummary.EXTRA_REMATCH_P2P );
            if ( null != value ) {
                WiDirService.inviteRemote( m_activity, value, nli );
                recordInviteSent( InviteMeans.WIFIDIRECT, value );
            }
            value = m_summary.getStringExtra( GameSummary.EXTRA_REMATCH_MQTT );
            if ( null != value ) {
                MQTTUtils.inviteRemote( m_activity, value, nli );
                recordInviteSent( InviteMeans.MQTT, value );
            }

            showToast( R.string.rematch_sent_toast );
        }
        return force;
    }

    private boolean tryOtherInvites()
    {
        boolean result = false;
        Assert.assertNotNull( m_summary );
        String str64 = m_summary.getStringExtra( GameSummary.EXTRA_REMATCH_ADDR );
        try {
            CommsAddrRec addr = (CommsAddrRec)Utils.string64ToSerializable( str64 );
            result = tryOtherInvites( addr );
        } catch ( Exception ex ) {
            Log.ex( TAG, ex );
            Assert.failDbg();
        }
        return result;
    }

    private boolean tryOtherInvites( CommsAddrRec addr )
    {
        boolean result = true;
        NetLaunchInfo nli = nliForMe();
        CommsConnTypeSet conTypes = addr.conTypes;
        for ( CommsConnType typ : conTypes ) {
            switch ( typ ) {
            case COMMS_CONN_MQTT:
                MQTTUtils.inviteRemote( m_activity, addr.mqtt_devID, nli );
                recordInviteSent( InviteMeans.MQTT, addr.mqtt_devID );
                break;
            case COMMS_CONN_BT:
                BTUtils.inviteRemote( m_activity, addr.bt_btAddr, nli );
                recordInviteSent( InviteMeans.BLUETOOTH, addr.bt_btAddr );
                break;
                // case COMMS_CONN_RELAY:
                //     RelayService.inviteRemote( m_activity, m_jniGamePtr, 0, value, nli );
                //     recordInviteSent( InviteMeans.RELAY );
                //     break;
            case COMMS_CONN_SMS:
                sendNBSInviteIf( addr.sms_phone, nli, true );
                recordInviteSent( InviteMeans.SMS_DATA, addr.sms_phone );
                break;

            default:
                Log.d( TAG, "not inviting using addr type %s", typ );
            }
        }
        return result;
    }

    private void sendNBSInviteIf( String phone, NetLaunchInfo nli,
                                  boolean askOk )
    {
        if ( XWPrefs.getNBSEnabled( m_activity ) ) {
            NBSProto.inviteRemote( m_activity, phone, nli );
            recordInviteSent( InviteMeans.SMS_DATA, phone );
        } else if ( askOk ) {
            makeConfirmThenBuilder( R.string.warn_sms_disabled,
                                    Action.ENABLE_NBS_ASK )
                .setPosButton( R.string.button_enable_sms )
                .setNegButton( R.string.button_later )
                .setParams( nli, phone )
                .show();
        }
    }

    private void retryNBSInvites( Object[] params )
    {
        if ( null != params && 2 == params.length
             && params[0] instanceof NetLaunchInfo
             && params[1] instanceof String ) {
            sendNBSInviteIf( (String)params[1], (NetLaunchInfo)params[0],
                             false );
        } else {
            Log.w( TAG, "retryNBSInvites: tests failed" );
        }
    }

    private void recordInviteSent( InviteMeans means, String dev )
    {
        boolean invitesSent = true;
        if ( !m_showedReInvite ) { // have we sent since?
            SentInvitesInfo sentInfo = DBUtils.getInvitesFor( m_activity, m_rowid );
            int nSent = sentInfo.getMinPlayerCount();
            invitesSent = nSent >= m_mySIS.nMissing;
        }

        DBUtils.recordInviteSent( m_activity, m_rowid, means, dev );

        if ( !invitesSent ) {
            Log.d( TAG, "recordInviteSent(): redoing invite alert" );
            showInviteAlertIf();
        }
    }

    private void handleViaThread( JNICmd cmd, Object... args )
    {
        if ( null == m_jniThread ) {
            Log.w( TAG, "m_jniThread null: not calling m_jniThread.handle(%s)",
                   cmd.toString() );
            DbgUtils.printStack( TAG );
        } else {
            m_jniThread.handle( cmd, args );
        }
    }
} // class BoardDelegate
