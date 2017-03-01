/* -*- compile-command: "find-and-gradle.sh installXw4Debug"; -*- */
/*
 * Copyright 2009 - 2017 by Eric House (xwords@eehouse.org).  All rights
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
import android.content.DialogInterface;
import android.content.DialogInterface.OnCancelListener;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface.OnDismissListener;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.Map;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Semaphore;

import junit.framework.Assert;

import org.eehouse.android.xw4.DBUtils.SentInvitesInfo;
import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.DlgDelegate.ActionPair;
import org.eehouse.android.xw4.Perms23.Perm;
import org.eehouse.android.xw4.Toolbar.Buttons;
import org.eehouse.android.xw4.jni.CommonPrefs;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;
import org.eehouse.android.xw4.jni.CurGameInfo;
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

public class BoardDelegate extends DelegateBase
    implements TransportProcs.TPMsgHandler, View.OnClickListener,
               DwnldDelegate.DownloadFinishedListener,
               ConnStatusHandler.ConnStatusCBacks,
               NFCUtils.NFCActor {
    private static final String TAG = BoardDelegate.class.getSimpleName();

    public static final String INTENT_KEY_CHAT = "chat";

    private static final int SCREEN_ON_TIME = 10 * 60 * 1000; // 10 mins

    private static final String DLG_TITLE = "DLG_TITLE";
    private static final String DLG_TITLESTR = "DLG_TITLESTR";
    private static final String DLG_BYTES = "DLG_BYTES";
    private static final String ROOM = "ROOM";
    private static final String PWDNAME = "PWDNAME";
    private static final String TOASTSTR = "TOASTSTR";
    private static final String WORDS = "WORDS";
    private static final String GETDICT = "GETDICT";

    private Activity m_activity;
    private BoardView m_view;
    private GamePtr m_jniGamePtr;
    private GameLock m_gameLock;
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
    private ArrayList<String> m_pendingChats;

    private CommsConnTypeSet m_connTypes = null;
    private String[] m_missingDevs;
    private int[] m_missingCounts;
    private InviteMeans m_missingMeans = null;
    private boolean m_progressShown = false;
    private boolean m_firingPrefs;
    private JNIUtils m_jniu;
    private boolean m_inTrade;  // save this in bundle?
    private BoardUtilCtxt m_utils;
    private boolean m_gameOver = false;

    // call startActivityForResult synchronously
    private Semaphore m_forResultWait = new Semaphore(0);
    private int m_resultCode;

    private Thread m_blockingThread;
    private JNIThread m_jniThread;
    private JNIThread m_jniThreadRef;
    private JNIThread.GameStateInfo m_gsi;
    private DlgID m_blockingDlgID = DlgID.NONE;

    private String m_toastStr;
    private String[] m_words;
    private String m_getDict;

    private int m_nMissing = -1;
    private int m_nGuestDevs = -1;
    private boolean m_haveInvited = false;
    private boolean m_overNotShown;
    private boolean m_dropOnDismiss;

    public class TimerRunnable implements Runnable {
        private int m_why;
        private int m_when;
        private int m_handle;
        private TimerRunnable( int why, int when, int handle ) {
            m_why = why;
            m_when = when;
            m_handle = handle;
        }
        public void run() {
            m_timers[m_why] = null;
            if ( null != m_jniThread ) {
                m_jniThread.handleBkgrnd( JNICmd.CMD_TIMER_FIRED,
                                          m_why, m_when, m_handle );
            }
        }
    }

    private DBAlert.OnDismissListener m_blockingODL =
        new DBAlert.OnDismissListener() {
            public void onDismissed() {
                releaseIfBlocking();
            }
        };

    @Override
    protected Dialog makeDialog( DBAlert alert, Object[] params )
    {
        final DlgID dlgID = alert.getDlgID();
        DbgUtils.logd( TAG, "makeDialog(%s)", dlgID.toString() );
        OnClickListener lstnr;
        AlertDialog.Builder ab = makeAlertBuilder();

        Dialog dialog;
        switch ( dlgID ) {
        case DLG_OKONLY:
        case DLG_RETRY:
        case GAME_OVER:
        case DLG_CONNSTAT: {
            GameSummary summary = (GameSummary)params[0];
            int title = (Integer)params[1];
            String msg = (String)params[2];
            ab.setTitle( title )
                .setMessage( msg )
                .setPositiveButton( android.R.string.ok, null );
            if ( DlgID.DLG_RETRY == dlgID ) {
                lstnr = new OnClickListener() {
                        public void onClick( DialogInterface dlg,
                                             int whichButton ) {
                            handleViaThread( JNICmd.CMD_RESET );
                        }
                    };
                ab.setNegativeButton( R.string.button_retry, lstnr );
            } else if ( DlgID.GAME_OVER == dlgID
                        && rematchSupported( m_activity, true, summary ) ) {
                lstnr = new OnClickListener() {
                        public void onClick( DialogInterface dlg,
                                             int whichButton ) {
                            doRematchIf();
                        }
                    };
                ab.setNegativeButton( R.string.button_rematch, lstnr );
            } else if ( DlgID.DLG_CONNSTAT == dlgID
                        && BuildConfig.DEBUG && null != m_connTypes
                        && (m_connTypes.contains( CommsConnType.COMMS_CONN_RELAY )
                            || m_connTypes.contains( CommsConnType.COMMS_CONN_P2P )) ) {

                lstnr = new OnClickListener() {
                        public void onClick( DialogInterface dlg,
                                             int whichButton ) {
                            NetStateCache.reset( m_activity );
                            if ( m_connTypes.contains( CommsConnType.COMMS_CONN_RELAY ) ) {
                                RelayService.reset( m_activity );
                            }
                            if ( m_connTypes.contains( CommsConnType.COMMS_CONN_P2P ) ) {
                                WiDirService.reset( m_activity );
                            }
                        }
                    };
                ab.setNegativeButton( R.string.button_reconnect, lstnr );
            }
            dialog = ab.create();
        }
            break;

        case DLG_USEDICT:
        case DLG_GETDICT: {
            int title = (Integer)params[0];
            String msg = (String)params[1];
            lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dlg,
                                         int whichButton ) {
                        if ( DlgID.DLG_USEDICT == dlgID ) {
                            setGotGameDict( m_getDict );
                        } else {
                            DwnldDelegate
                                .downloadDictInBack( m_activity,
                                                     m_gi.dictLang,
                                                     m_getDict, BoardDelegate.this );
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

        case DLG_DELETED:
            ab = ab.setTitle( R.string.query_title )
                .setMessage( R.string.msg_dev_deleted )
                .setPositiveButton( android.R.string.ok, null );
            lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dlg,
                                         int whichButton ) {
                        deleteAndClose();
                    }
                };
            ab.setNegativeButton( R.string.button_delete, lstnr );
            dialog = ab.create();
            break;

        case QUERY_TRADE:
        case QUERY_MOVE: {
            String msg = (String)params[0];
            lstnr = new OnClickListener() {
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
            lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dialog,
                                         int whichButton ) {
                        m_resultCode = 1;
                    }
                };
            ab.setPositiveButton( android.R.string.ok, lstnr );
            if ( DlgID.DLG_SCORES == dlgID ) {
                if ( null != m_words && m_words.length > 0 ) {
                    String buttonTxt;
                    boolean studyOn = XWPrefs.getStudyEnabled( m_activity );
                    if ( m_words.length == 1 ) {
                        int resID = studyOn
                            ? R.string.button_lookup_study_fmt
                            : R.string.button_lookup_fmt;
                        buttonTxt = getString( resID, m_words[0] );
                    } else {
                        int resID = studyOn ? R.string.button_lookup_study
                            : R.string.button_lookup;
                        buttonTxt = getString( resID );
                    }
                    lstnr = new OnClickListener() {
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
            alert.setOnDismissListener( m_blockingODL );
        }
            break;

        case PICK_TILE_REQUESTBLANK: {
            final int turn = (Integer)params[0];
            final int col = (Integer)params[1];
            final int row  = (Integer)params[2];
            String[] texts = (String[])params[3];
            dialog = ab.setItems( texts, new OnClickListener() {
                    public void onClick( DialogInterface dialog,
                                         int item ) {
                        handleViaThread( JNICmd.CMD_SET_BLANK, turn, col,
                                         row, item );
                    }
                })
                .setNegativeButton( android.R.string.cancel, null )
                .setTitle( R.string.title_tile_picker )
                .create();
        }
            break;

        case PICK_TILE_REQUESTTRAY_BLK: {
            String[] texts = (String[])params[0];
            checkBlocking();
            lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dialog,
                                         int item ) {
                        m_resultCode = item;
                    }
                };
            ab.setItems( texts, lstnr );

            String curTiles = (String)params[1];
            boolean canUndoTiles = (Boolean)params[2];

            ab.setTitle( getString( R.string.cur_tiles_fmt, curTiles ) );
            if ( canUndoTiles ) {
                OnClickListener undoClicked = new OnClickListener() {
                        public void onClick( DialogInterface dialog,
                                             int whichButton ) {
                            m_resultCode = UtilCtxt.PICKER_BACKUP;
                            removeDialog( dlgID );
                        }
                    };
                ab.setPositiveButton( R.string.tilepick_undo,
                                      undoClicked );
            }
            OnClickListener doAllClicked = new OnClickListener() {
                    public void onClick( DialogInterface dialog,
                                         int whichButton ) {
                        m_resultCode = UtilCtxt.PICKER_PICKALL;
                        removeDialog( dlgID );
                    }
                };
            ab.setNegativeButton( R.string.tilepick_all, doAllClicked );
                
            dialog = ab.create();
            alert.setOnDismissListener( m_blockingODL );
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
            dialog = makeAlertDialog();
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

    private Dialog makeAlertDialog()
    {
        String message;
        int titleID;
        boolean showInviteButton = true;
        boolean showNeutButton = false;

        int buttonTxt = R.string.newgame_invite;

        if ( m_relayMissing ) {
            titleID = R.string.seeking_relay;
            // If relay is only means, don't allow at all
            boolean relayOnly = 1 >= m_connTypes.size();
            showInviteButton = !relayOnly;
            message = getString( R.string.no_relay_conn );
            if ( NetStateCache.netAvail( m_activity )
                 && NetStateCache.onWifi() ) {
                message += getString( R.string.wifi_warning );
            }
            if ( !relayOnly ) {
                CommsConnTypeSet without = (CommsConnTypeSet)
                    m_connTypes.clone();
                without.remove( CommsConnType.COMMS_CONN_RELAY );
                message += "\n\n"
                    + getString( R.string.drop_relay_warning_fmt,
                                 without.toString( m_activity, true ) );
                buttonTxt = R.string.newgame_drop_relay;
            }
        } else {
            m_sentInfo = DBUtils.getInvitesFor( m_activity, m_rowid );
            int nSent = m_sentInfo.getMinPlayerCount();
            boolean invitesSent = nSent >= m_nMissing;
            if ( invitesSent ) {
                if ( m_summary.hasRematchInfo() ) {
                    titleID = R.string.waiting_rematch_title;
                    message = getString( R.string.rematch_msg );
                } else {
                    titleID = R.string.waiting_invite_title;
                    message = getQuantityString( R.plurals.invite_sent_fmt,
                                                 nSent, nSent, m_nMissing );
                }
                buttonTxt = R.string.button_reinvite;
                showNeutButton = true;
            } else if ( DeviceRole.SERVER_ISCLIENT == m_gi.serverRole ) {
                Assert.assertFalse( m_summary.hasRematchInfo() );
                message = getString( R.string.invited_msg );
                titleID = R.string.waiting_title;
                showInviteButton = false;
            } else {
                titleID = R.string.waiting_title;
                message = getQuantityString( R.plurals.invite_msg_fmt,
                                             m_nMissing, m_nMissing );
            }

            if ( ! invitesSent && showInviteButton ) {
                String ps = null;
                if ( m_nMissing > 1 ) {
                    ps = getString( R.string.invite_multiple );
                } else {
                    boolean[] avail = NFCUtils.nfcAvail( m_activity );
                    if ( avail[1] ) {
                        ps = getString( R.string.invite_if_nfc );
                    }
                }
                if ( null != ps ) {
                    message += "\n\n" + ps;
                }
            }

            message += "\n\n" + getString( R.string.invite_stays );
        }

        // Button button = ad.getButton( AlertDialog.BUTTON_POSITIVE );
        // button.setVisibility( nukeInviteButton ? View.GONE : View.VISIBLE );
        // if ( !nukeInviteButton ) {
        //     button.setText( buttonTxt );
        // }
        // button = ad.getButton( AlertDialog.BUTTON_NEUTRAL );
        // button.setVisibility( nukeNeutButton ? View.GONE : View.VISIBLE );

        OnClickListener lstnr = new OnClickListener() {
                public void onClick( DialogInterface dialog, int item ){
                    if ( !m_relayMissing ||
                         ! m_connTypes.contains(CommsConnType.COMMS_CONN_RELAY) ) {
                        Assert.assertTrue( 0 < m_nMissing );
                        if ( m_summary.hasRematchInfo() ) {
                            tryRematchInvites( true );
                        } else {
                            callInviteChoices( m_sentInfo );
                        }
                    } else {
                        askDropRelay();
                    }
                }
            };

        AlertDialog.Builder ab = makeAlertBuilder()
            .setTitle( titleID )
            .setMessage( message )
            .setPositiveButton( buttonTxt, lstnr )
            .setOnCancelListener( new OnCancelListener() {
                    public void onCancel( DialogInterface dialog ) {
                        finish();
                    }
                } );

        if ( showNeutButton ) {
            OnClickListener lstnrMore = new OnClickListener() {
                    public void onClick( DialogInterface dialog,
                                         int item ) {
                        String msg = m_sentInfo
                            .getAsText( m_activity );
                        makeOkOnlyBuilder( msg ).show();
                    }
                };
            ab.setNeutralButton( R.string.newgame_invite_more, lstnrMore );
        }
        if ( showInviteButton ) {
            OnClickListener lstnrWait = new OnClickListener() {
                    public void onClick( DialogInterface dialog,
                                         int item ) {
                        finish();
                    }
                };
            ab.setNegativeButton( R.string.button_wait, lstnrWait );
        }

        Dialog dialog = ab.create();
        return dialog;
    }

    public BoardDelegate( Delegator delegator, Bundle savedInstanceState )
    {
        super( delegator, savedInstanceState, R.layout.board, R.menu.board_menu );
        m_activity = delegator.getActivity();
    }

    protected void init( Bundle savedInstanceState )
    {
        getBundledData( savedInstanceState );

        m_pendingChats = new ArrayList<String>();

        m_utils = new BoardUtilCtxt();
        m_jniu = JNIUtilsImpl.get( m_activity );
        m_timers = new TimerRunnable[4]; // needs to be in sync with
                                         // XWTimerReason
        m_view = (BoardView)findViewById( R.id.board_view );
        m_view.setBoardDelegate( this );
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
        DbgUtils.logi( TAG, "opening rowid %d", m_rowid );
        m_haveInvited = args.getBoolean( GameUtils.INVITED, false );
        m_overNotShown = true;

        m_jniThreadRef = JNIThread.getRetained( m_rowid, true );

        // see http://stackoverflow.com/questions/680180/where-to-stop- \
        // destroy-threads-in-android-service-class
        m_jniThreadRef.setDaemonOnce( true ); // firing
        m_jniThreadRef.startOnce();

        NFCUtils.register( m_activity, this ); // Don't seem to need to unregister...

        setBackgroundColor();
        setKeepScreenOn();
    } // init

    @Override
    protected void onStart()
    {
        super.onStart();
        doResume( true );
    }

    @Override
    protected void onResume()
    {
        super.onResume();
        doResume( false );
    }

    protected void onPause()
    {
        closeIfFinishing( false );
        m_handler = null;
        ConnStatusHandler.setHandler( null );
        waitCloseGame( true );
        pauseGame();
        super.onPause();
    }

    @Override
    protected void onStop()
    {
        if ( isFinishing() ) {
            m_jniThreadRef.release();
            m_jniThreadRef = null;
        }
        super.onStop();
    }

    @Override
    protected void onDestroy()
    {
        closeIfFinishing( true );
        if ( null != m_jniThreadRef ) {
            m_jniThreadRef.release();
            m_jniThreadRef = null;
            // Assert.assertNull( m_jniThreadRef ); // firing
        }
        GamesListDelegate.boardDestroyed( m_rowid );
        super.onDestroy();
    }

    protected void onSaveInstanceState( Bundle outState )
    {
        outState.putString( TOASTSTR, m_toastStr );
        outState.putStringArray( WORDS, m_words );
        outState.putString( GETDICT, m_getDict );
    }

    private void getBundledData( Bundle bundle )
    {
        if ( null != bundle ) {
            m_toastStr = bundle.getString( TOASTSTR );
            m_words = bundle.getStringArray( WORDS );
            m_getDict = bundle.getString( GETDICT );
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
            case SMS_INVITE_RESULT:
                missingMeans = InviteMeans.SMS;
                break;
            case RELAY_INVITE_RESULT:
                missingMeans = InviteMeans.RELAY;
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
                m_missingMeans = missingMeans;
            }
        }
    }

    protected void onWindowFocusChanged( boolean hasFocus )
    {
        if ( hasFocus ) {
            if ( m_firingPrefs ) {
                m_firingPrefs = false;
                if ( null != m_jniThread ) {
                    handleViaThread( JNICmd.CMD_PREFS_CHANGE );
                }
                // in case of change...
                setBackgroundColor();
                setKeepScreenOn();
            } else if ( 0 < m_nMissing ) {
                showDialogFragment( DlgID.DLG_INVITE );
            }
        }
    }

    // Invitations need to check phone state to decide whether to offer SMS
    // invitation. Complexity (showRationale) boolean is to prevent infinite
    // loop of showing the rationale over and over. Android will always tell
    // us to show the rationale, but if we've done it already we need to go
    // straight to asking for the permission.
    private void callInviteChoices( final SentInvitesInfo info )
    {
        Perms23.tryGetPerms( this, Perm.READ_PHONE_STATE,
                             R.string.phone_state_rationale,
                             Action.ASKED_PHONE_STATE, this, info );
    }

    private void showInviteChoicesThen( Object[] params )
    {
        SentInvitesInfo info = (SentInvitesInfo)params[0];
        showInviteChoicesThen( Action.LAUNCH_INVITE_ACTION, info );
    }

    @Override
    public void orientationChanged()
    {
        DbgUtils.logd( TAG, "BoardDelegate.orientationChanged()" );
        initToolbar();
        m_view.orientationChanged();
    }

    @Override
    protected void setTitle()
    {
        setTitle( GameUtils.getName( m_activity, m_rowid ) );
    }

    private void initToolbar()
    {
        // Wait until we're attached....
        if ( null != findViewById( R.id.tbar_parent_hor ) ) {
            if ( null == m_toolbar ) {
                m_toolbar = new Toolbar( m_activity, this );
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
            Utils.setItemVisible( menu, R.id.board_menu_toggle,
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
        Utils.setItemVisible( menu, R.id.games_menu_study, enable );

        return true;
    } // onPrepareOptionsMenu

    @Override
    public boolean onOptionsItemSelected( MenuItem item )
    {
        boolean handled = true;
        JNICmd cmd = JNICmd.CMD_NONE;
        Runnable proc = null;

        int id = item.getItemId();
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
        case R.id.board_menu_toggle:
            cmd = JNICmd.CMD_VALUES;
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
        case R.id.games_menu_study:
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
            DbgUtils.logw( TAG, "menuitem %d not handled", id );
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
        boolean handled = true;
        JNICmd cmd = JNICmd.CMD_NONE;
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
            showToast( m_toastStr );
            m_toastStr = null;
            break;
        case BUTTON_BROWSEALL_ACTION:
        case BUTTON_BROWSE_ACTION:
            String curDict = m_gi.dictName( m_view.getCurPlayer() );
            View button = m_toolbar.getButtonFor( Buttons.BUTTON_BROWSE_DICT );
            if ( Action.BUTTON_BROWSEALL_ACTION == action &&
                 DictsDelegate.handleDictsPopup( getDelegator(), button,
                                                 curDict, m_gi.dictLang ) ){
                break;
            }
            DictBrowseDelegate.launch( getDelegator(), curDict );
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
            cmd = JNICmd.CMD_VALUES;
            break;
        case CHAT_ACTION:
            startChatActivity();
            break;
        case START_TRADE_ACTION:
            showToast( R.string.entering_trade );
            cmd = JNICmd.CMD_TRADE;
            break;
        case LOOKUP_ACTION:
            launchLookup( m_words, m_gi.dictLang );
            break;
        case NFC_TO_SELF:
            GamesListDelegate.sendNFCToSelf( m_activity, makeNFCMessage() );
            break;
        case DROP_RELAY_ACTION:
            dropConViaAndRestart(CommsConnType.COMMS_CONN_RELAY);
            break;
        case DELETE_AND_EXIT:
            deleteAndClose();
            break;
        case DROP_SMS_ACTION:   // do nothing; work done in onNegButton case
            break;

        case INVITE_SMS:
            int nMissing = (Integer)params[0];
            SentInvitesInfo info = (SentInvitesInfo)params[1];
            SMSInviteDelegate.launchForResult( m_activity, nMissing, info,
                                               RequestCode.SMS_INVITE_RESULT );
            break;

        case ASKED_PHONE_STATE:
            showInviteChoicesThen( params );
            break;

        case ENABLE_SMS_DO:
            post( new Runnable() {
                    public void run() {
                        retrySMSInvites( params );
                    }
                } );
            // FALLTHRU: so super gets called, before
        default:
            handled = super.onPosButton( action, params );
        }

        if ( JNICmd.CMD_NONE != cmd ) {
            handleViaThread( cmd );
        }

        return handled;
    }

    @Override
    public boolean onNegButton( Action action, Object[] params )
    {
        boolean handled = true;
        switch ( action ) {
        case ENABLE_RELAY_DO_OR:
            m_dropOnDismiss = true;
            break;
        case DROP_SMS_ACTION:
            dropConViaAndRestart(CommsConnType.COMMS_CONN_SMS);
            break;
        case DELETE_AND_EXIT:
            finish();
            break;
        case ASKED_PHONE_STATE:
            showInviteChoicesThen( params );
            break;
        default:
            handled = super.onNegButton( action, params );
        }
        return handled;
    }

    @Override
    public boolean onDismissed( Action action, Object[] params )
    {
        boolean handled = true;
        switch ( action ) {
        case ENABLE_RELAY_DO_OR:
            if ( m_dropOnDismiss ) {
                postDelayed( new Runnable() {
                        public void run() {
                            askDropRelay();
                        }
                    }, 10 );
            }
            break;
        case DELETE_AND_EXIT:
            finish();
            break;
        default:
            handled = false;
        }
        return handled;
    }

    @Override
    public void inviteChoiceMade( Action action, InviteMeans means,
                                  Object[] params )
    {
        if ( action == Action.LAUNCH_INVITE_ACTION ) {
            SentInvitesInfo info = params[0] instanceof SentInvitesInfo
                ? (SentInvitesInfo)params[0] : null;
            switch( means ) {
            case NFC:
                if ( XWPrefs.getNFCToSelfEnabled( m_activity ) ) {
                    makeConfirmThenBuilder( R.string.nfc_to_self, Action.NFC_TO_SELF )
                        .show();
                } else if ( ! NFCUtils.nfcAvail( m_activity )[1] ) {
                    showDialogFragment( DlgID.ENABLE_NFC );
                } else {
                    makeOkOnlyBuilder( R.string.nfc_just_tap ).show();
                }
                break;
            case BLUETOOTH:
                BTInviteDelegate.launchForResult( m_activity, m_nMissing, info,
                                                  RequestCode.BT_INVITE_RESULT );
                break;
            case SMS:
                Perms23.tryGetPerms( this, Perm.SEND_SMS, R.string.sms_invite_rationale,
                                     Action.INVITE_SMS, this, m_nMissing, info );
                break;
            case RELAY:
                RelayInviteDelegate.launchForResult( m_activity, m_nMissing,
                                                     RequestCode.RELAY_INVITE_RESULT );
                break;
            case WIFIDIRECT:
                WiDirInviteDelegate.launchForResult( m_activity,
                                                     m_nMissing,
                                                     RequestCode.P2P_INVITE_RESULT );
                break;
            case EMAIL:
            case CLIPBOARD:
                NetLaunchInfo nli = new NetLaunchInfo( m_summary, m_gi, 1,
                                                       1 + m_nGuestDevs );
                if ( m_relayMissing ) {
                    nli.removeAddress( CommsConnType.COMMS_CONN_RELAY );
                }
                if ( InviteMeans.EMAIL == means ) {
                    GameUtils.launchEmailInviteActivity( m_activity, nli );
                } else if ( InviteMeans.CLIPBOARD == means ) {
                    GameUtils.inviteURLToClip( m_activity, nli );
                }
                recordInviteSent( means, null );

                break;
            default:
                Assert.fail();
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
        switch( event ) {
        case MESSAGE_ACCEPTED:
        case MESSAGE_REFUSED:
            ConnStatusHandler.
                updateStatusIn( m_activity, this, CommsConnType.COMMS_CONN_BT,
                                MultiService.MultiEvent.MESSAGE_ACCEPTED == event);
            break;
        case MESSAGE_NOGAME:
            int gameID = (Integer)args[0];
            if ( gameID == m_gi.gameID ) {
                post( new Runnable() {
                        public void run() {
                            showDialogFragment( DlgID.DLG_DELETED );
                        }
                    } );
            }
            break;

        case BT_ENABLED:
            pingBTRemotes();
            break;

            // This can be BT or SMS.  In BT case there's a progress
            // thing going.  Not in SMS case.
        case NEWGAME_FAILURE:
            DbgUtils.logw( TAG, "failed to create game" );
            break;
        case NEWGAME_DUP_REJECTED:
            if ( m_progressShown ) {
                m_progressShown = false;
                stopProgress();     // in case it's a BT invite
            }
            final String msg =
                getString( R.string.err_dup_invite_fmt, (String)args[0] );
            post( new Runnable() {
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
            DbgUtils.showf( m_activity, R.string.sms_send_failed );
            break;

        default:
            if ( m_progressShown ) {
                m_progressShown = false;
                stopProgress();     // in case it's a BT invite
            }
            super.eventOccurred( event, args );
            break;
        }
    }

    //////////////////////////////////////////////////
    // TransportProcs.TPMsgHandler interface
    //////////////////////////////////////////////////

    public void tpmRelayConnd( final String room, final int devOrder,
                               final boolean allHere, final int nMissing )
    {
        runOnUiThread( new Runnable() {
                public void run() {
                    handleConndMessage( room, devOrder, allHere, nMissing );
                }
            } );
    }

    public void tpmRelayErrorProc( TransportProcs.XWRELAY_ERROR relayErr )
    {
        int strID = -1;
        DlgID dlgID = DlgID.NONE;
        boolean doToast = false;

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
            strID = R.string.msg_dev_deleted;
            dlgID = DlgID.DLG_DELETED;
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
            final int strIDf = strID;
            final DlgID dlgIDf = dlgID;
            post( new Runnable() {
                    public void run() {
                        showDialogFragment( dlgIDf, R.string.relay_alert,
                                            getString( strIDf ) );
                    }
                });
        }
    }

    //////////////////////////////////////////////////
    // DwnldActivity.DownloadFinishedListener interface
    //////////////////////////////////////////////////
    public void downloadFinished( String lang, final String name,
                                  boolean success )
    {
        if ( success ) {
            post( new Runnable() {
                    public void run() {
                        setGotGameDict( name );
                    }
                } );
        }
    }

    //////////////////////////////////////////////////
    // NFCUtils.NFCActor
    //////////////////////////////////////////////////
    public String makeNFCMessage()
    {
        String data = null;
        if ( 0 < m_nMissing ) {  // Isn't there a better test??
            NetLaunchInfo nli = new NetLaunchInfo( m_gi );
            Assert.assertTrue( 0 <= m_nGuestDevs );
            nli.forceChannel = 1 + m_nGuestDevs;
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
                default:
                    DbgUtils.logw( TAG, "Not doing NFC join for conn type %s",
                                   typ.toString() );
                }
            }
            data = nli.makeLaunchJSON();
        }
        if ( null != data ) {
            removeDialog( DlgID.CONFIRM_THEN );

            recordInviteSent( InviteMeans.NFC, null );
        }
        return data;
    }

    //////////////////////////////////////////////////
    // ConnStatusHandler.ConnStatusCBacks
    //////////////////////////////////////////////////
    public void invalidateParent()
    {
        runOnUiThread(new Runnable() {
                public void run() {
                    m_view.invalidate();
                }
            });
    }

    public void onStatusClicked()
    {
        final String msg = ConnStatusHandler.getStatusText( m_activity, m_connTypes );
        post( new Runnable() {
                public void run() {
                    if ( null == msg ) {
                        askNoAddrsDelete();
                    } else {
                        showDialogFragment( DlgID.DLG_CONNSTAT, null,
                                            R.string.info_title, msg );
                    }
                }
            } );
    }

    public Handler getHandler()
    {
        return m_handler;
    }

    private void deleteAndClose()
    {
        GameUtils.deleteGame( m_activity, m_gameLock, false );
        waitCloseGame( false );
        finish();
    }

    private void askNoAddrsDelete()
    {
        makeConfirmThenBuilder( R.string.connstat_net_noaddr,
                                Action.DELETE_AND_EXIT )
            .setPosButton( R.string.list_item_delete )
            .setNegButton( R.string.button_close_game )
            .show();
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
        CommsAddrRec addr = new CommsAddrRec();
        XwJNI.comms_getAddr( m_jniGamePtr, addr );
        addr.remove( typ );
        XwJNI.comms_setAddr( m_jniGamePtr, addr );

        finish();

        GameUtils.launchGame( getDelegator(), m_rowid, m_haveInvited );
    }

    private void setGotGameDict( String getDict )
    {
        m_jniThread.setSaveDict( getDict );

        String msg = getString( R.string.reload_new_dict_fmt, getDict );
        showToast( msg );
        finish();
        GameUtils.launchGame( getDelegator(), m_rowid, false );
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

    // Blocking thread stuff: The problem this is solving occurs when
    // you have a blocking dialog up, meaning the jni thread is
    // blocked, and you hit the home button.  onPause() gets called
    // which wants to use jni calls to e.g. summarize.  For those to
    // succeed (the jni being non-reentrant and set up to assert if it
    // is reentered) the jni thread must first be unblocked and
    // allowed to return back through the jni.  We unblock using
    // Thread.interrupt method, the exception from which winds up
    // caught in waitBlockingDialog.  The catch dismisses the dialog
    // with the default/cancel value, but that takes us into the
    // onDismissListener which normally releases the semaphore.  But
    // if we've interrupted then we can't release it or blocking won't
    // work for as long as this activity lives.  Hence
    // releaseIfBlocking().  This feels really fragile but it does
    // work.

    private void checkBlocking()
    {
        if ( null == m_blockingThread ) {
            DbgUtils.logd( TAG, "no blocking thread!!!" );
        }
    }

    private void setBlockingThread()
    {
        synchronized( this ) {
            Assert.assertTrue( null == m_blockingThread );
            m_blockingThread = Thread.currentThread();
        }
    }

    private void clearBlockingThread()
    {
        synchronized( this ) {
            Assert.assertTrue( null != m_blockingThread );
            m_blockingThread = null;
        }
    }

    private void interruptBlockingThread()
    {
        synchronized( this ) {
            if ( null != m_blockingThread ) {
                m_blockingThread.interrupt();
            }
        }
    }

    private void releaseIfBlocking()
    {
        synchronized( this ) {
            if ( null != m_blockingThread ) {
                m_forResultWait.release();
            }
        }
    }

    private void handleConndMessage( String room, int devOrder, // <- hostID
                                     boolean allHere, int nMissing )
    {
        boolean skipDismiss = false;

        int naMsg = 0;
        int naKey = 0;
        String toastStr = null;
        m_nMissing = nMissing;
        if ( allHere ) {
            // All players have now joined the game.  The device that
            // created the room will assign tiles.  Then it will be
            // the first player's turn

            // Skip this until there's a way to show it only once per game
            if ( false ) {
                toastStr = getString( R.string.msg_relay_all_here_fmt, room );
                if ( devOrder > 1 ) {
                    naMsg = R.string.not_again_conndall;
                    naKey = R.string.key_notagain_conndall;
                }
            }
        } else if ( nMissing > 0 ) {
            if ( m_summary.hasRematchInfo() ) {
                skipDismiss = !tryRematchInvites( false );
            } else if ( !m_haveInvited ) {
                m_haveInvited = true;
                showDialogFragment( DlgID.DLG_INVITE );
                invalidateOptionsMenuIf();
                skipDismiss = true;
            } else {
                toastStr = getQuantityString( R.plurals.msg_relay_waiting_fmt, nMissing,
                                              devOrder, room, nMissing );
                if ( devOrder == 1 ) {
                    naMsg = R.string.not_again_conndfirst;
                    naKey = R.string.key_notagain_conndfirst;
                } else {
                    naMsg = R.string.not_again_conndmid;
                    naKey = R.string.key_notagain_conndmid;
                }
            }
        }

        if ( null != toastStr ) {
            DbgUtils.logi( TAG, "handleConndMessage(): toastStr: %s", toastStr );
            m_toastStr = toastStr;
            if ( naMsg == 0 ) {
                onPosButton( Action.SHOW_EXPL_ACTION, null );
            } else {
                makeNotAgainBuilder( naMsg, naKey, Action.SHOW_EXPL_ACTION )
                    .show();
            }
        }

        if ( !skipDismiss ) {
            dismissInviteAlert( nMissing, true ); // NO!!!
        }

        invalidateOptionsMenuIf();
    } // handleConndMessage

    private class BoardUtilCtxt extends UtilCtxtImpl {

        public BoardUtilCtxt()
        {
            super( m_activity );
        }

        @Override
        public void requestTime()
        {
            runOnUiThread( new Runnable() {
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
        public void setIsServer( boolean isServer )
        {
            DeviceRole newRole = isServer? DeviceRole.SERVER_ISSERVER
                : DeviceRole.SERVER_ISCLIENT;
            if ( newRole != m_gi.serverRole ) {
                m_gi.serverRole = newRole;
                if ( !isServer ) {
                    handleViaThread( JNICmd.CMD_SWITCHCLIENT );
                }
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
                Assert.fail();
            }

            if ( 0 != id ) {
                final String bonusStr = getString( id );
                post( new Runnable() {
                        public void run() {
                            showToast( bonusStr );
                        }
                    } );
            }
        }

        @Override
        public void playerScoreHeld( int player )
        {
            LastMoveInfo lmi = new LastMoveInfo();
            XwJNI.model_getPlayersLastScore( m_jniGamePtr, player, lmi );
            String expl = lmi.format( m_activity );
            if ( null == expl || 0 == expl.length() ) {
                expl = getString( R.string.no_moves_made );
            }
            final String text = expl;
            post( new Runnable() {
                    public void run() {
                        showToast( text );
                    }
                } );
        }

        @Override
        public void cellSquareHeld( final String words )
        {
            post( new Runnable() {
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

        // This is supposed to be called from the jni thread
        @Override
        public void notifyPickTileBlank( int playerNum, int col, int row, String[] texts )
        {
            showDialogFragment( DlgID.PICK_TILE_REQUESTBLANK, playerNum, col,
                                row, texts );
        }

        @Override
        public int userPickTileTray( int playerNum, String[] texts,
                                     String[] curTiles, int nPicked )
        {
            String curTilesStr = TextUtils.join( ", ", curTiles );
            boolean canUndoTiles = 0 < nPicked;
            waitBlockingDialog( DlgID.PICK_TILE_REQUESTTRAY_BLK,
                                UtilCtxt.PICKER_PICKALL, texts, curTilesStr,
                                canUndoTiles );
            return m_resultCode;
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
                m_nMissing = 0;
                post( new Runnable() {
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
            return ! m_jniThread.busy();
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
        public void userError( int code )
        {
            int resid = 0;
            boolean asToast = false;
            switch( code ) {
            case UtilCtxt.ERR_TILES_NOT_IN_LINE:
                resid = R.string.str_tiles_not_in_line;
                break;
            case UtilCtxt.ERR_NO_EMPTIES_IN_TURN:
                resid = R.string.str_no_empties_in_turn;
                break;
            case UtilCtxt.ERR_TWO_TILES_FIRST_MOVE:
                resid = R.string.str_two_tiles_first_move;
                break;
            case UtilCtxt.ERR_TILES_MUST_CONTACT:
                resid = R.string.str_tiles_must_contact;
                break;
            case UtilCtxt.ERR_NOT_YOUR_TURN:
                resid = R.string.str_not_your_turn;
                break;
            case UtilCtxt.ERR_NO_PEEK_ROBOT_TILES:
                resid = R.string.str_no_peek_robot_tiles;
                break;
            case UtilCtxt.ERR_NO_EMPTY_TRADE:
                // This should not be possible as the button's
                // disabled when no tiles selected.
                Assert.fail();
                break;
            case UtilCtxt.ERR_TOO_FEW_TILES_LEFT_TO_TRADE:
                resid = R.string.str_too_few_tiles_left_to_trade;
                break;
            case UtilCtxt.ERR_CANT_UNDO_TILEASSIGN:
                resid = R.string.str_cant_undo_tileassign;
                break;
            case UtilCtxt.ERR_CANT_HINT_WHILE_DISABLED:
                resid = R.string.str_cant_hint_while_disabled;
                break;
            case UtilCtxt.ERR_NO_PEEK_REMOTE_TILES:
                resid = R.string.str_no_peek_remote_tiles;
                break;
            case UtilCtxt.ERR_REG_UNEXPECTED_USER:
                resid = R.string.str_reg_unexpected_user;
                break;
            case UtilCtxt.ERR_SERVER_DICT_WINS:
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

            if ( resid != 0 ) {
                if ( asToast ) {
                    final int residf = resid;
                    runOnUiThread( new Runnable() {
                            public void run() {
                                showToast( residf );
                            }
                        } );
                } else {
                    nonBlockingDialog( DlgID.DLG_OKONLY, getString( resid ) );
                }
            }
        } // userError

        // Called from server_makeFromStream, whether there's something
        // missing or not.
        @Override
        public void informMissing( boolean isServer, CommsConnTypeSet connTypes,
                                   int nDevs, final int nMissing )
        {
            boolean doDismiss = true;
            m_connTypes = connTypes;
            Assert.assertTrue( isServer || 0 == nMissing );
            // DbgUtils.logf( "BoardDelegate.informMissing(isServer=%b, nDevs=%d, nMissing=%d)",
            //                isServer, nDevs, nMissing );
            m_nGuestDevs = nDevs;

            m_nMissing = nMissing; // will be 0 unless isServer is true

            if ( null != connTypes && 0 == connTypes.size() ) {
                askNoAddrsDelete();
            } else if ( 0 < nMissing && isServer && !m_haveInvited ) {
                doDismiss = false;
                post( new Runnable() {
                        public void run() {
                            showDialogFragment( DlgID.DLG_INVITE );
                        }
                    } );
            }

            // If we might have put up an alert earlier, take it down
            if ( doDismiss ) {
                dismissInviteAlert( nMissing, !m_relayMissing );
            }
        }

        @Override
        public void informMove( int turn, String expl, String words )
        {
            m_words = null == words? null : wordsToArray( words );
            nonBlockingDialog( DlgID.DLG_SCORES, expl );
            if ( isVisible() ) {
                Utils.playNotificationSound( m_activity );
            } else {
                LastMoveInfo lmi = new LastMoveInfo();
                XwJNI.model_getPlayersLastScore( m_jniGamePtr, turn, lmi );
                GameUtils.BackMoveResult bmr = new GameUtils.BackMoveResult();
                bmr.m_lmi = lmi;
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
                m_getDict = newName;
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
                              String fromPlayer )
        {
            runOnUiThread( new Runnable() {
                    public void run() {
                        DBUtils.appendChatHistory( m_activity, m_rowid, msg,
                                                   fromIndx );
                        if ( ! ChatDelegate.append( m_rowid, msg,
                                                    fromIndx ) ) {
                            startChatActivity();
                        }
                    }
                } );
        }
    } // class BoardUtilCtxt

    private void doResume( boolean isStart )
    {
        boolean success = true;
        boolean firstStart = null == m_handler;
        if ( firstStart ) {
            m_handler = new Handler();
            m_blockingDlgID = DlgID.NONE;

            success = m_jniThreadRef.configure( m_activity, m_view, m_utils, this,
                                                makeJNIHandler() );
            if ( success ) {
                m_jniGamePtr = m_jniThreadRef.getGamePtr();
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
            } catch ( GameUtils.NoSuchGameException nsge ) {
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
                public void handleMessage( Message msg ) {
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
                            if ( m_inTrade != m_gsi.inTrade ) {
                                m_inTrade = m_gsi.inTrade;
                            }
                            m_view.setInTrade( m_inTrade );
                            adjustTradeVisibility();
                            invalidateOptionsMenuIf();
                        }
                        break;
                    case JNIThread.GOT_WORDS:
                        CurGameInfo gi = m_jniThreadRef.getGI();
                        launchLookup( wordsToArray((String)msg.obj),
                                      gi.dictLang );
                        break;
                    case JNIThread.GAME_OVER:
                        showDialogFragment( DlgID.GAME_OVER, m_summary, msg.arg1,
                                            (String)msg.obj );
                        break;
                    case JNIThread.MSGS_SENT:
                        int nSent = (Integer)msg.obj;
                        showToast( getQuantityString( R.plurals.resent_msgs_fmt,
                                                      nSent, nSent ) );
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
            m_gi = m_jniThread.getGI();
            m_summary = m_jniThread.getSummary();
            m_gameLock = m_jniThread.getLock();

            m_view.startHandling( m_activity, m_jniThread, m_connTypes );

            handleViaThread( JNICmd.CMD_START );

            if ( !CommonPrefs.getHideTitleBar( m_activity ) ) {
                setTitle();
            }

            initToolbar();
            populateToolbar();
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
                DBUtils.setMsgFlags( m_rowid, GameSummary.MSG_FLAGS_NONE );
            }

            Utils.cancelNotification( m_activity, (int)m_rowid );

            askPermissions();

            if ( m_gi.serverRole != DeviceRole.SERVER_STANDALONE ) {
                warnIfNoTransport();
                trySendChats();
                tickle( isStart );
                tryInvites();
            }
        }
    } // resumeGame

    private void askPermissions()
    {
        if ( m_summary.conTypes.contains( CommsConnType.COMMS_CONN_SMS )
             && null == m_permCbck ) { // already asked?
            m_permCbck = new Perms23.PermCbck() {
                    @Override
                    public void onPermissionResult( Map<Perm, Boolean> perms ) {
                        if ( ! perms.get(Perm.SEND_SMS) ) {
                            makeConfirmThenBuilder( R.string.missing_perms,
                                                    Action.DROP_SMS_ACTION )
                                .setNegButton(R.string.remove_sms)
                                .show();
                        }
                    }
                };
            new Perms23.Builder(Perm.SEND_SMS)
                .asyncQuery( m_activity, m_permCbck );
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
                break;
            default:
                DbgUtils.logw( TAG, "tickle: unexpected type %s",
                               typ.toString() );
                Assert.fail();
            }
        }

        if ( 0 < m_connTypes.size() ) {
            handleViaThread( JNIThread.JNICmd.CMD_RESEND, force, true, false );
        }
    }

    private void dismissInviteAlert( final int nMissing, final boolean connected )
    {
        runOnUiThread( new Runnable() {
                public void run() {
                    if ( m_relayMissing && connected ) {
                        m_relayMissing = false;
                    }
                    if ( 0 == nMissing || !m_relayMissing ) {
                        dismissDialog( DlgID.DLG_INVITE );
                    }
                }
            } );
    }

    private void pingBTRemotes()
    {
        if ( null != m_connTypes
             && m_connTypes.contains( CommsConnType.COMMS_CONN_BT ) ) {
            CommsAddrRec[] addrs = XwJNI.comms_getAddrs( m_jniGamePtr );
            for ( CommsAddrRec addr : addrs ) {
                if ( addr.contains( CommsConnType.COMMS_CONN_BT ) ) {
                    BTService.pingHost( m_activity, addr.bt_btAddr,
                                        m_gi.gameID );
                }
            }
        }
    }

    private void populateToolbar()
    {
        if ( null != m_toolbar ) {
            m_toolbar.setListener( Buttons.BUTTON_BROWSE_DICT,
                                   R.string.not_again_browseall,
                                   R.string.key_na_browseall,
                                   Action.BUTTON_BROWSEALL_ACTION );
            m_toolbar.setLongClickListener( Buttons.BUTTON_BROWSE_DICT,
                                            R.string.not_again_browse,
                                            R.string.key_na_browse,
                                            Action.BUTTON_BROWSE_ACTION );
            m_toolbar.setListener( Buttons.BUTTON_HINT_PREV,
                                   R.string.not_again_hintprev,
                                   R.string.key_notagain_hintprev,
                                   Action.PREV_HINT_ACTION );
            m_toolbar.setListener( Buttons.BUTTON_HINT_NEXT,
                                   R.string.not_again_hintnext,
                                   R.string.key_notagain_hintnext,
                                   Action.NEXT_HINT_ACTION );
            m_toolbar.setListener( Buttons.BUTTON_JUGGLE,
                                   R.string.not_again_juggle,
                                   R.string.key_notagain_juggle,
                                   Action.JUGGLE_ACTION );
            m_toolbar.setListener( Buttons.BUTTON_FLIP,
                                   R.string.not_again_flip,
                                   R.string.key_notagain_flip,
                                   Action.FLIP_ACTION );
            m_toolbar.setListener( Buttons.BUTTON_VALUES,
                                   R.string.not_again_values,
                                   R.string.key_na_values,
                                   Action.VALUES_ACTION );
            m_toolbar.setListener( Buttons.BUTTON_UNDO,
                                   R.string.not_again_undo,
                                   R.string.key_notagain_undo,
                                   Action.UNDO_ACTION );
            m_toolbar.setListener( Buttons.BUTTON_CHAT,
                                   R.string.not_again_chat,
                                   R.string.key_notagain_chat,
                                   Action.CHAT_ACTION );
        }
    } // populateToolbar

    private int waitBlockingDialog( final DlgID dlgID, int cancelResult,
                                    final Object... params )
    {
        int result = cancelResult;
        // this has been true; dunno why
        if ( DlgID.NONE != m_blockingDlgID ) {
            DbgUtils.logw( TAG, "waitBlockingDialog: dropping dlgID %d b/c %d set",
                           dlgID, m_blockingDlgID );
        } else {
            setBlockingThread();
            m_resultCode = cancelResult;

            if ( post( new Runnable() {
                    public void run() {
                        m_blockingDlgID = dlgID;
                        showDialogFragment( dlgID, params );
                    }
                } ) ) {

                try {
                    m_forResultWait.acquire();
                } catch ( java.lang.InterruptedException ie ) {
                    DbgUtils.logex( TAG, ie );
                    if ( DlgID.NONE != m_blockingDlgID ) {
                        try {
                            dismissDialog( m_blockingDlgID );
                        } catch ( java.lang.IllegalArgumentException iae ) {
                            DbgUtils.logex( TAG, iae );
                        }
                    }
                }
                m_blockingDlgID = DlgID.NONE;
            }

            clearBlockingThread();
            result = m_resultCode;
        }
        return result;
    }

    private void nonBlockingDialog( final DlgID dlgID, final String txt )
    {
        int dlgTitle = 0;
        switch ( dlgID ) {
        case DLG_OKONLY:
        case DLG_SCORES:
            dlgTitle = R.string.info_title;
            break;
        case DLG_USEDICT:
        case DLG_GETDICT:
            dlgTitle = R.string.inform_dict_title;
            break;

        default:
            Assert.fail();
        }

        final int fTitle = dlgTitle;
        runOnUiThread( new Runnable() {
                public void run() {
                    showDialogFragment( dlgID, fTitle, txt );
                }
            } );
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
            interruptBlockingThread();

            m_jniThread.release();
            m_jniThread = null;

            m_view.stopHandling();

            m_gameLock = null;
        }
    }

    private void waitCloseGame( boolean save )
    {
        pauseGame();
        if ( null != m_jniThread ) {
            // m_jniGamePtr.release();
            // m_jniGamePtr = null;

            // m_gameLock.unlock(); // likely the problem
            m_gameLock = null;
        }
    }

    private void warnIfNoTransport()
    {
        if ( m_connTypes.contains( CommsConnType.COMMS_CONN_SMS ) ) {
            if ( !XWPrefs.getSMSEnabled( m_activity ) ) {
                makeConfirmThenBuilder( R.string.warn_sms_disabled,
                                        Action.ENABLE_SMS_ASK )
                    .setPosButton( R.string.button_enable_sms )
                    .setNegButton( R.string.button_later )
                    .show();
            }
        }
        if ( m_connTypes.contains( CommsConnType.COMMS_CONN_RELAY ) ) {
            if ( !RelayService.relayEnabled( m_activity ) ) {
                m_dropOnDismiss = false;
                String msg = getString( R.string.warn_relay_disabled )
                    + "\n\n" + getString( R.string.warn_relay_remove );
                makeConfirmThenBuilder( msg, Action.ENABLE_RELAY_DO_OR )
                    .setPosButton( R.string.button_enable_relay )
                    .setNegButton( R.string.newgame_drop_relay )
                    .show();
            }
        }
    }

    private void trySendChats()
    {
        Iterator<String> iter = m_pendingChats.iterator();
        while ( iter.hasNext() ) {
            handleViaThread( JNICmd.CMD_SENDCHAT, iter.next() );
        }
        m_pendingChats.clear();
    }

    private void tryInvites()
    {
        if ( 0 < m_nMissing && m_summary.hasRematchInfo() ) {
            tryRematchInvites( false );
        } else if ( null != m_missingDevs ) {
            Assert.assertNotNull( m_missingMeans );
            String gameName = GameUtils.getName( m_activity, m_rowid );
            for ( int ii = 0; ii < m_missingDevs.length; ++ii ) {
                String dev = m_missingDevs[ii];
                int nPlayers = m_missingCounts[ii];
                Assert.assertTrue( 0 <= m_nGuestDevs );
                int forceChannel = ii + m_nGuestDevs + 1;
                NetLaunchInfo nli = new NetLaunchInfo( m_summary, m_gi,
                                                       nPlayers, forceChannel );
                if ( m_relayMissing ) {
                    nli.removeAddress( CommsConnType.COMMS_CONN_RELAY );
                }

                switch ( m_missingMeans ) {
                case BLUETOOTH:
                    if ( ! m_progressShown ) {
                        m_progressShown = true;
                        String progMsg = BTService.nameForAddr( dev );
                        progMsg = getString( R.string.invite_progress_fmt, progMsg );
                        startProgress( R.string.invite_progress_title, progMsg,
                                       new OnCancelListener() {
                                           public void onCancel( DialogInterface dlg )
                                           {
                                               m_progressShown = false;
                                           }
                                       });
                    }
                    BTService.inviteRemote( m_activity, dev, nli );
                    break;
                case SMS:
                    sendSMSInviteIf( dev, nli, true );
                    dev = null; // don't record send a second time
                    break;
                case RELAY:
                    try {
                        int destDevID = Integer.parseInt( dev ); // failing
                        RelayService.inviteRemote( m_activity, destDevID,
                                                   null, nli );
                    } catch (NumberFormatException nfi) {
                        DbgUtils.logex( TAG, nfi );
                    }
                    break;
                case WIFIDIRECT:
                    WiDirService.inviteRemote( m_activity, dev, nli );
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
            m_toolbar.update( Buttons.BUTTON_FLIP, m_gsi.visTileCount >= 1 );
            m_toolbar.update( Buttons.BUTTON_VALUES, m_gsi.visTileCount >= 1 );
            m_toolbar.update( Buttons.BUTTON_JUGGLE, m_gsi.canShuffle );
            m_toolbar.update( Buttons.BUTTON_UNDO, m_gsi.canRedo );
            m_toolbar.update( Buttons.BUTTON_HINT_PREV, m_gsi.canHint );
            m_toolbar.update( Buttons.BUTTON_HINT_NEXT, m_gsi.canHint );
            m_toolbar.update( Buttons.BUTTON_CHAT, m_gsi.canChat );
            m_toolbar.update( Buttons.BUTTON_BROWSE_DICT,
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
            m_toolbar.setVisible( !m_inTrade );
        }
        if ( null != m_tradeButtons ) {
            m_tradeButtons.setVisibility( m_inTrade? View.VISIBLE : View.GONE );
        }
        if ( m_inTrade && null != m_exchCommmitButton ) {
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
            DbgUtils.logw( TAG, "post(): dropping b/c handler null" );
            DbgUtils.printStack( TAG );
        }
        return canPost;
    }

    private void postDelayed( Runnable runnable, int when )
    {
        if ( null != m_handler ) {
            m_handler.postDelayed( runnable, when );
        } else {
            DbgUtils.logw( TAG, "postDelayed: dropping %d because handler null", when );
        }
    }

    private void removeCallbacks( Runnable which )
    {
        if ( null != m_handler ) {
            m_handler.removeCallbacks( which );
        } else {
            DbgUtils.logw( TAG, "removeCallbacks: dropping %h because handler null",
                           which );
        }
    }

    private String[] wordsToArray( String words )
    {
        String[] tmp = TextUtils.split( words, "\n" );
        String[] wordsArray = new String[tmp.length];
        for ( int ii = 0, jj = tmp.length; ii < tmp.length; ++ii, --jj ) {
            wordsArray[ii] = tmp[jj-1];
        }
        return wordsArray;
    }

    // For now, supported if standalone or either BT or SMS used for transport
    private boolean rematchSupported( boolean showMulti )
    {
        return rematchSupported( showMulti ? m_activity : null,
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
        doRematchIf( m_activity, this, m_rowid, m_summary, m_gi, m_jniGamePtr );
    }

    private static void doRematchIf( Activity activity, DelegateBase dlgt,
                                     long rowid, GameSummary summary,
                                     CurGameInfo gi, GamePtr jniGamePtr )
    {
        boolean doIt = true;
        String phone = null;
        String btAddr = null;
        String relayID = null;
        String p2pMacAddress = null;
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
            }
        }

        if ( doIt ) {
            CommsConnTypeSet connTypes = summary.conTypes;
            String newName = summary.getRematchName();
            Intent intent = GamesListDelegate
                .makeRematchIntent( activity, rowid, gi, connTypes, btAddr,
                                    phone, relayID, p2pMacAddress, newName );
            if ( null != intent ) {
                activity.startActivity( intent );
            }
        }
    }

    public static void setupRematchFor( Activity activity, long rowID )
    {
        GamePtr gamePtr = null;
        GameSummary summary = null;
        CurGameInfo gi = null;
        JNIThread thread = JNIThread.getRetained( rowID );
        if ( null != thread ) {
            gamePtr = thread.getGamePtr().retain();
            summary = thread.getSummary();
            gi = thread.getGI();
        } else {
            GameLock lock = new GameLock( rowID, false );
            if ( lock.tryLock() ) {
                summary = DBUtils.getSummary( activity, lock );
                gi = new CurGameInfo( activity );
                gamePtr = GameUtils.loadMakeGame( activity, gi, lock );
                lock.unlock();
            }
        }

        if ( null != gamePtr ) {
            doRematchIf( activity, null, rowID, summary, gi, gamePtr );
            gamePtr.release();
        } else {
            DbgUtils.logw( TAG, "setupRematchFor(): unable to lock game" );
        }

        if ( null != thread ) {
            thread.release();
        }
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
            int numHere = 1;
            int forceChannel = 1;
            NetLaunchInfo nli = new NetLaunchInfo( m_summary, m_gi, numHere,
                                                   forceChannel );

            String value;
            value = m_summary.getStringExtra( GameSummary.EXTRA_REMATCH_PHONE );
            if ( null != value ) {
                sendSMSInviteIf( value, nli, true );
            }
            value = m_summary.getStringExtra( GameSummary.EXTRA_REMATCH_BTADDR );
            if ( null != value ) {
                BTService.inviteRemote( m_activity, value, nli );
                recordInviteSent( InviteMeans.BLUETOOTH, value );
            }
            value = m_summary.getStringExtra( GameSummary.EXTRA_REMATCH_RELAY );
            if ( null != value ) {
                RelayService.inviteRemote( m_activity, 0, value, nli );
                recordInviteSent( InviteMeans.RELAY, value );
            }
            value = m_summary.getStringExtra( GameSummary.EXTRA_REMATCH_P2P );
            if ( null != value ) {
                WiDirService.inviteRemote( m_activity, value, nli );
                recordInviteSent( InviteMeans.WIFIDIRECT, value );
            }

            showToast( R.string.rematch_sent_toast );
        }
        return force;
    }

    private void sendSMSInviteIf( String phone, NetLaunchInfo nli,
                                  boolean askOk )
    {
        if ( XWPrefs.getSMSEnabled( m_activity ) ) {
            SMSService.inviteRemote( m_activity, phone, nli );
            recordInviteSent( InviteMeans.SMS, phone );
        } else if ( askOk ) {
            makeConfirmThenBuilder( R.string.warn_sms_disabled,
                                    Action.ENABLE_SMS_ASK )
                .setPosButton( R.string.button_enable_sms )
                .setNegButton( R.string.button_later )
                .setParams( nli, phone )
                .show();
        }
    }

    private void retrySMSInvites( Object[] params )
    {
        if ( null != params && 2 == params.length
             && params[0] instanceof NetLaunchInfo
             && params[1] instanceof String ) {
            sendSMSInviteIf( (String)params[1], (NetLaunchInfo)params[0],
                             false );
        } else {
            DbgUtils.logw( TAG, "retrySMSInvites: tests failed" );
        }
    }

    private void recordInviteSent( InviteMeans means, String dev )
    {
        DBUtils.recordInviteSent( m_activity, m_rowid, means, dev );
    }

    private void handleViaThread( JNICmd cmd, Object... args )
    {
        if ( null == m_jniThread ) {
            DbgUtils.logw( TAG, "not calling handle(%s)", cmd.toString() );
            DbgUtils.printStack( TAG );
        } else {
            m_jniThread.handle( cmd, args );
        }
    }
} // class BoardDelegate
