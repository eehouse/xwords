/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009 - 2016 by Eric House (xwords@eehouse.org).  All
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
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface.OnCancelListener;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface.OnDismissListener;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.Window;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;
import java.util.concurrent.Semaphore;

import junit.framework.Assert;

import org.eehouse.android.xw4.DBUtils.SentInvitesInfo;
import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify.InviteMeans;
import org.eehouse.android.xw4.jni.*;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;
import org.eehouse.android.xw4.jni.JNIThread.*;
import org.eehouse.android.xw4.jni.XwJNI.GamePtr;

public class BoardDelegate extends DelegateBase
    implements TransportProcs.TPMsgHandler, View.OnClickListener,
               DwnldDelegate.DownloadFinishedListener, 
               ConnStatusHandler.ConnStatusCBacks,
               NFCUtils.NFCActor {

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

    private ArrayList<String> m_pendingChats;

    private String m_dlgBytes = null;
    private EditText m_passwdEdit;
    private int m_dlgTitle;
    private String m_dlgTitleStr;
    private String[] m_texts;
    private CommsConnTypeSet m_connTypes = null;
    private String[] m_missingDevs;
    private int[] m_missingCounts;
    private InviteMeans m_missingMeans = null;
    private boolean m_progressShown = false;
    private String m_curTiles;
    private boolean m_canUndoTiles;
    private boolean m_firingPrefs;
    private JNIUtils m_jniu;
    private boolean m_volKeysZoom;
    private boolean m_inTrade;  // save this in bundle?
    private BoardUtilCtxt m_utils;
    private int m_invitesPending;
    private boolean m_gameOver = false;

    // call startActivityForResult synchronously
    private Semaphore m_forResultWait = new Semaphore(0);
    private int m_resultCode;

    private Thread m_blockingThread;
    private JNIThread m_jniThread;
    private JNIThread m_jniThreadRef;
    private JNIThread.GameStateInfo m_gsi;
    private DlgID m_blockingDlgID = DlgID.NONE;

    private String m_room;
    private String m_toastStr;
    private String[] m_words;
    private String m_pwdName;
    private String m_getDict;

    private int m_nMissing = -1;
    private int m_nGuestDevs = -1;
    private boolean m_haveInvited = false;
    private boolean m_overNotShown;

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

    protected Dialog onCreateDialog( int id )
    {
        Dialog dialog = super.onCreateDialog( id );
        if ( null == dialog ) {
            OnClickListener lstnr;
            AlertDialog.Builder ab = makeAlertBuilder();

            final DlgID dlgID = DlgID.values()[id];
            switch ( dlgID ) {
            case DLG_OKONLY:
            case DLG_RETRY:
            case GAME_OVER:
            case DLG_CONNSTAT:
                ab.setTitle( m_dlgTitle )
                    .setMessage( m_dlgBytes )
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
                            && rematchSupported( true ) ) {
                    lstnr = new OnClickListener() {
                            public void onClick( DialogInterface dlg, 
                                                 int whichButton ) {
                                doRematchIf();
                            }
                        };
                    ab.setNegativeButton( R.string.button_rematch, lstnr );
                } else if ( DlgID.DLG_CONNSTAT == dlgID
                            && BuildConfig.DEBUG && null != m_connTypes
                            && m_connTypes.contains( CommsConnType.COMMS_CONN_RELAY ) ) {
                    lstnr = new OnClickListener() {
                            public void onClick( DialogInterface dlg, 
                                                 int whichButton ) {
                                NetStateCache.reset( m_activity );
                                RelayService.reset( m_activity );
                            }
                        };
                    ab.setNegativeButton( R.string.button_reconnect, lstnr );
                }
                dialog = ab.create();
                setRemoveOnDismiss( dialog, dlgID );
                break;

            case DLG_USEDICT:
            case DLG_GETDICT:
                lstnr = new OnClickListener() {
                        public void onClick( DialogInterface dlg, 
                                             int whichButton ) {
                            if ( DlgID.DLG_USEDICT == dlgID ) {
                                setGotGameDict( m_getDict );
                            } else {
                                DwnldDelegate
                                    .downloadDictInBack( m_activity,
                                                         m_gi.dictLang,
                                                         m_getDict,
                                                         BoardDelegate.this );
                            }
                        }
                    };
                dialog = ab.setTitle( m_dlgTitle )
                    .setMessage( m_dlgBytes )
                    .setPositiveButton( R.string.button_yes, lstnr )
                    .setNegativeButton( R.string.button_no, null )
                    .create();
                setRemoveOnDismiss( dialog, dlgID );
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

            case QUERY_REQUEST_BLK:
            case QUERY_INFORM_BLK:
            case DLG_SCORES:
            case DLG_BADWORDS_BLK: 
                ab = ab.setMessage( m_dlgBytes );
                if ( 0 != m_dlgTitle ) {
                    ab.setTitle( m_dlgTitle );
                }
                lstnr = new OnClickListener() {
                        public void onClick( DialogInterface dialog, 
                                             int whichButton ) {
                            m_resultCode = 1;
                        }
                    };
                ab.setPositiveButton( DlgID.QUERY_REQUEST_BLK == dlgID ?
                                      R.string.button_yes : android.R.string.ok,
                                      lstnr );
                if ( DlgID.QUERY_REQUEST_BLK == dlgID ) {
                    lstnr = new OnClickListener() {
                            public void onClick( DialogInterface dialog, 
                                                 int whichButton ) {
                                m_resultCode = 0;
                            }
                        };
                    ab.setNegativeButton( R.string.button_no, lstnr );
                } else if ( DlgID.DLG_SCORES == dlgID ) {
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
                                    showNotAgainDlgThen( R.string.not_again_lookup, 
                                                         R.string.
                                                         key_na_lookup, 
                                                         Action.LOOKUP_ACTION );
                                }
                            };
                        ab.setNegativeButton( buttonTxt, lstnr );
                    }
                }

                dialog = ab.create();
                dialog.setOnDismissListener( makeODLforBlocking( id ) );
                break;

            case PICK_TILE_REQUESTBLANK_BLK:
            case PICK_TILE_REQUESTTRAY_BLK:
                lstnr = new OnClickListener() {
                        public void onClick( DialogInterface dialog, 
                                             int item ) {
                            m_resultCode = item;
                        }
                    };
                ab.setItems( m_texts, lstnr );

                if ( DlgID.PICK_TILE_REQUESTBLANK_BLK == dlgID ) {
                    ab.setTitle( R.string.title_tile_picker );
                } else {
                    ab.setTitle( getString( R.string.cur_tiles_fmt, m_curTiles ) );
                    if ( m_canUndoTiles ) {
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
                }

                dialog = ab.create();
                dialog.setOnDismissListener( makeODLforBlocking( id ) );
                break;

            case ASK_PASSWORD_BLK:
                m_dlgTitleStr = getString( R.string.msg_ask_password_fmt, m_pwdName );
                LinearLayout pwdLayout = 
                    (LinearLayout)inflate( R.layout.passwd_view );
                m_passwdEdit = (EditText)pwdLayout.findViewById( R.id.edit );
                m_passwdEdit.setText( "", TextView.BufferType.EDITABLE );
                ab.setTitle( m_dlgTitleStr )
                    .setView( pwdLayout )
                    .setPositiveButton( android.R.string.ok,
                                        new OnClickListener() {
                                            public void 
                                                onClick( DialogInterface dlg,
                                                         int whichButton ) {
                                                m_resultCode = 1;
                                            }
                                        });
                dialog = ab.create();
                dialog.setOnDismissListener( makeODLforBlocking( id ) );
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
                lstnr = new OnClickListener() {
                        public void onClick( DialogInterface dialog, 
                                             int item ) {
                            if ( !m_relayMissing ||
                                 ! m_connTypes.contains(CommsConnType.COMMS_CONN_RELAY) ) {
                                Assert.assertTrue( 0 < m_nMissing );
                                if ( m_summary.hasRematchInfo() ) {
                                    tryRematchInvites( true );
                                } else {                                
                                    showInviteChoicesThen( Action.LAUNCH_INVITE_ACTION,
                                                           m_sentInfo );
                                }
                            } else {
                                askDropRelay();
                            }
                        }
                    };
                OnClickListener lstnrWait = new OnClickListener() {
                        public void onClick( DialogInterface dialog, 
                                             int item ) {
                            finish();
                        }
                    };
                OnClickListener lstnrMore = new OnClickListener() {
                        public void onClick( DialogInterface dialog, 
                                             int item ) {
                            showOKOnlyDialog( m_sentInfo.getAsText( m_activity ) );
                        }
                    };
                
                dialog = ab.setTitle( "foo" )
                    .setMessage( "" )
                    .setPositiveButton( "", lstnr )
                    .setNegativeButton( R.string.button_wait, lstnrWait )
                    .setNeutralButton( R.string.newgame_invite_more, lstnrMore )
                    .setOnCancelListener( new OnCancelListener() {
                            public void onCancel( DialogInterface dialog ) {
                                finish();
                            }
                        } )
                    .create();
                break;

            case ENABLE_NFC:
                dialog = NFCUtils.makeEnableNFCDialog( m_activity );
                break;

            default:
                // just drop it; super.onCreateDialog likely failed
                break;
            }
        }
        return dialog;
    } // onCreateDialog

    @Override
    protected void prepareDialog( DlgID dlgID, Dialog dialog )
    {
        switch( dlgID ) {
        case DLG_INVITE:
            AlertDialog ad = (AlertDialog)dialog;
            String message;
            int titleID;
            boolean nukeInviteButton = false;
            boolean nukeNeutButton = true;
            int buttonTxt = R.string.newgame_invite;
            if ( m_relayMissing ) {
                titleID = R.string.seeking_relay;
                // If relay is only means, don't allow at all
                boolean relayOnly = 1 >= m_connTypes.size();
                nukeInviteButton = relayOnly;
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
                                     without.toString( m_activity ) );
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
                    nukeNeutButton = false;
                } else if ( DeviceRole.SERVER_ISCLIENT == m_gi.serverRole ) {
                    Assert.assertFalse( m_summary.hasRematchInfo() );
                    message = getString( R.string.invited_msg );
                    titleID = R.string.waiting_title;
                    nukeInviteButton = true;
                } else {
                    titleID = R.string.waiting_title;
                    message = getQuantityString( R.plurals.invite_msg_fmt, 
                                                 m_nMissing, m_nMissing );
                }

                if ( ! invitesSent && ! nukeInviteButton ) {
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

            ad.setMessage( message );
            ad.setTitle( titleID );

            Button button = ad.getButton( AlertDialog.BUTTON_POSITIVE );
            button.setVisibility( nukeInviteButton ? View.GONE : View.VISIBLE );
            if ( !nukeInviteButton ) {
                button.setText( buttonTxt );
            }
            button = ad.getButton( AlertDialog.BUTTON_NEUTRAL );
            button.setVisibility( nukeNeutButton ? View.GONE : View.VISIBLE );
            
            break;
        default:
            super.prepareDialog( dlgID, dialog );
            break;
        }
    }

    public BoardDelegate( Delegator delegator, Bundle savedInstanceState )
    {
        super( delegator, savedInstanceState, R.layout.board, R.menu.board_menu );
        m_activity = delegator.getActivity();
    }

    protected void init( Bundle savedInstanceState ) 
    {
        getBundledData( savedInstanceState );

        if ( BuildConstants.CHAT_SUPPORTED ) {
            m_pendingChats = new ArrayList<String>();
        }

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
        m_volKeysZoom = XWPrefs.getVolKeysZoom( m_activity );

        Bundle args = getArguments();
        m_rowid = args.getLong( GameUtils.INTENT_KEY_ROWID, -1 );
        DbgUtils.logf( "BoardActivity: opening rowid %d", m_rowid );
        m_haveInvited = args.getBoolean( GameUtils.INVITED, false );
        m_overNotShown = true;

        Handler handler = new Handler() {
                public void handleMessage( Message msg ) {
                    switch( msg.what ) {
                    case JNIThread.DIALOG:
                        m_dlgBytes = (String)msg.obj;
                        m_dlgTitle = msg.arg1;
                        showDialog( DlgID.DLG_OKONLY );
                        break;
                    case JNIThread.QUERY_ENDGAME:
                        showDialog( DlgID.QUERY_ENDGAME );
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
                        m_dlgBytes = (String)msg.obj;
                        m_dlgTitle = msg.arg1;
                        showDialog( DlgID.GAME_OVER );
                        break;
                    case JNIThread.MSGS_SENT:
                        int nSent = (Integer)msg.obj;
                        showToast( getQuantityString( R.plurals.resent_msgs_fmt, 
                                                      nSent, nSent ) );
                        break;
                    }
                }
            };

        m_jniThreadRef = JNIThread.getRetained( m_rowid, true ).
            configure( m_activity, m_view, m_utils, this, handler );
        m_jniGamePtr = m_jniThreadRef.getGamePtr();
        Assert.assertNotNull( m_jniGamePtr );

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
        outState.putInt( DLG_TITLE, m_dlgTitle );
        outState.putString( DLG_TITLESTR, m_dlgTitleStr );
        outState.putString( DLG_BYTES, m_dlgBytes );
        outState.putString( ROOM, m_room );
        outState.putString( TOASTSTR, m_toastStr );
        outState.putStringArray( WORDS, m_words );
        outState.putString( PWDNAME, m_pwdName );
        outState.putString( GETDICT, m_getDict );
    }

    private void getBundledData( Bundle bundle )
    {
        if ( null != bundle ) {
            m_dlgTitleStr = bundle.getString( DLG_TITLESTR  );
            m_dlgTitle = bundle.getInt( DLG_TITLE  );
            m_dlgBytes = bundle.getString( DLG_BYTES );
            m_room = bundle.getString( ROOM );
            m_toastStr = bundle.getString( TOASTSTR );
            m_words = bundle.getStringArray( WORDS );
            m_pwdName = bundle.getString( PWDNAME );
            m_getDict = bundle.getString( GETDICT );
        }
    }

    @Override
    protected void onActivityResult( RequestCode requestCode, int resultCode, Intent data )
    {
        if ( Activity.RESULT_CANCELED != resultCode ) {
            InviteMeans missingMeans = null;
            switch ( requestCode ) {
            case CHAT_REQUEST:
                if ( BuildConstants.CHAT_SUPPORTED ) {
                    String msg = data.getStringExtra( INTENT_KEY_CHAT );
                    if ( null != msg && msg.length() > 0 ) {
                        m_pendingChats.add( msg );
                        trySendChats();
                    }
                }
                break;
            case BT_INVITE_RESULT:
                missingMeans = InviteMeans.BLUETOOTH;
                break;
            case SMS_INVITE_RESULT:
                missingMeans = InviteMeans.SMS;
                break;
            case RELAY_INVITE_RESULT:
                missingMeans = InviteMeans.RELAY;
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
                m_volKeysZoom = XWPrefs.getVolKeysZoom( m_activity );
                if ( null != m_jniThread ) {
                    handleViaThread( JNICmd.CMD_PREFS_CHANGE );
                }
                // in case of change...
                setBackgroundColor();
                setKeepScreenOn();
            } else if ( ! isFinishing() ) {
                if ( m_relayMissing || 0 < m_nMissing ) {
                    showDialog( DlgID.DLG_INVITE );
                }
            }
        }
    }

    @Override
    public void orientationChanged()
    {
        boolean isPortrait = isPortrait();
        DbgUtils.logdf( "BoardDelegate.orientationChanged(isPortrait=%b)",
                        isPortrait );
        positionToolbar( isPortrait );
        m_view.orientationChanged();
    }

    private void positionToolbar( boolean isPortrait )
    {
        if ( null != findViewById( R.id.tbar_parent_hor ) ) {
            if ( null == m_toolbar ) {
                m_toolbar = new Toolbar( m_activity, this );
            }
            m_toolbar.setIsPortrait( isPortrait );
        }
    }

    protected boolean onKeyDown( int keyCode, KeyEvent event )
    {
        boolean handled = false;
        if ( null != m_jniThread ) {
            XwJNI.XP_Key xpKey = keyCodeToXPKey( keyCode );
            if ( XwJNI.XP_Key.XP_KEY_NONE != xpKey ) {
                handleViaThread( JNICmd.CMD_KEYDOWN, xpKey );
            } else {
                switch( keyCode ) {
                case KeyEvent.KEYCODE_VOLUME_DOWN:
                case KeyEvent.KEYCODE_VOLUME_UP:
                    if ( m_volKeysZoom ) {
                        int zoomBy = KeyEvent.KEYCODE_VOLUME_DOWN == keyCode
                            ? -2 : 2;
                        handled = doZoom( zoomBy );
                    }
                    break;
                }
            }
        }
        return handled;
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
                                  BuildConstants.CHAT_SUPPORTED
                                  && m_gsi.canChat );
            Utils.setItemVisible( menu, R.id.board_menu_tray, 
                                  !inTrade && m_gsi.canHideRack );
            Utils.setItemVisible( menu, R.id.board_menu_trade, 
                                  m_gsi.canTrade );
            Utils.setItemVisible( menu, R.id.board_menu_undo_last, 
                                  m_gsi.canUndo );
        }

        Utils.setItemVisible( menu, R.id.board_menu_trade_cancel, inTrade );
        Utils.setItemVisible( menu, R.id.board_menu_trade_commit, 
                              inTrade && m_gsi.tradeTilesSelected
                              && m_gsi.curTurnSelected );
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
                showNotAgainDlgThen( R.string.not_again_done, 
                                     R.string.key_notagain_done, 
                                     Action.COMMIT_ACTION );
            } else {
                dlgButtonClicked( Action.COMMIT_ACTION, AlertDialog.BUTTON_POSITIVE, null );
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
        case R.id.board_menu_zoom:
            cmd = JNICmd.CMD_TOGGLEZOOM;
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
            showNotAgainDlgThen( msg, R.string.key_notagain_trading,
                                 Action.START_TRADE_ACTION );
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
            showOKOnlyDialog( sentInfo.getAsText( m_activity ) );
            break;
        case R.id.board_menu_undo_current:
            cmd = JNICmd.CMD_UNDO_CUR;
            break;
        case R.id.board_menu_undo_last:
            showConfirmThen( R.string.confirm_undo_last, Action.UNDO_LAST_ACTION );
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
            showNotAgainDlgThen( R.string.not_again_sync,
                                 R.string.key_notagain_sync,
                                 Action.SYNC_ACTION );
            break;

        case R.id.board_menu_file_prefs:
            m_firingPrefs = true;
            Utils.launchSettings( m_activity );
            break;

        default:
            DbgUtils.logf( "menuitem %d not handled", id );
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
    public void dlgButtonClicked( Action action, int which,
                                  final Object[] params )
    {
        boolean handled = false;
        if ( AlertDialog.BUTTON_POSITIVE == which ) {
            handled = true;
            JNICmd cmd = JNICmd.CMD_NONE;
            switch ( action ) {
            case UNDO_LAST_ACTION:
                cmd = JNICmd.CMD_UNDO_LAST;
                break;
            case SYNC_ACTION:
                doSyncMenuitem();
                break;
            case SMS_CONFIG_ACTION:
                Utils.launchSettings( m_activity );
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
                View button = m_toolbar.getViewFor( Toolbar.BUTTON_BROWSE_DICT );
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
            case ZOOM_ACTION:
                cmd = JNICmd.CMD_TOGGLEZOOM;
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
                dropRelayAndRestart();
                break;
            case DELETE_AND_EXIT:
                deleteAndClose();
                break;

            case ENABLE_SMS_DO:
                handled = false; // so super gets called, before
                                 // retrySMSInvites
                post( new Runnable() {
                        public void run() {
                            retrySMSInvites( params );
                        }
                    } );
                break;
                
            default:
                handled = false;
            }

            if ( JNICmd.CMD_NONE != cmd ) {
                handleViaThread( cmd );
            }
        }

        if ( !handled ) {
            super.dlgButtonClicked( action, which, params );
        }
    } // dlgButtonClicked

    public void inviteChoiceMade( Action action, InviteMeans means, 
                                  Object[] params )
    {
        if ( action == Action.LAUNCH_INVITE_ACTION ) {
            SentInvitesInfo info = params[0] instanceof SentInvitesInfo
                ? (SentInvitesInfo)params[0] : null;
            switch( means ) {
            case NFC:
                if ( XWPrefs.getNFCToSelfEnabled( m_activity ) ) {
                    showConfirmThen( R.string.nfc_to_self, Action.NFC_TO_SELF );
                } else if ( ! NFCUtils.nfcAvail( m_activity )[1] ) {
                    showDialog( DlgID.ENABLE_NFC );
                } else {
                    showOKOnlyDialog( R.string.nfc_just_tap );
                }
                break;
            case BLUETOOTH:
                BTInviteDelegate.launchForResult( m_activity, m_nMissing, info,
                                                  RequestCode.BT_INVITE_RESULT );
                break;
            case SMS:
                SMSInviteDelegate.launchForResult( m_activity, m_nMissing, info,
                                                   RequestCode.SMS_INVITE_RESULT );
                break;
            case RELAY:
                RelayInviteDelegate.launchForResult( m_activity, m_nMissing, 
                                                     RequestCode.RELAY_INVITE_RESULT );
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
                            showDialog( DlgID.DLG_DELETED );
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
            DbgUtils.logf( "failed to create game" );
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
                        showOKOnlyDialog( msg );
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

            // if ( null != m_jniThread ) {
            //     boolean accepted = 
            //         MultiService.MultiEvent.SMS_RECEIVE_OK == event
            //         || MultiService.MultiEvent.SMS_SEND_OK == event;
            //     m_jniThread.handle( JNICmd.CMD_DRAW_SMS_STATUS, accepted );
            // }
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
                        m_dlgBytes = getString( strIDf );
                        m_dlgTitle = R.string.relay_alert;
                        showDialog( dlgIDf );
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
                default:
                    DbgUtils.logf( "Not doing NFC join for conn type %s",
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
                        m_dlgBytes = msg;
                        m_dlgTitle = R.string.info_title;
                        showDialog( DlgID.DLG_CONNSTAT );
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
        showConfirmThen( R.string.connstat_net_noaddr, 
                         R.string.list_item_delete,
                         Action.DELETE_AND_EXIT );
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
        showConfirmThen( msg, Action.DROP_RELAY_ACTION );
    }

    private void dropRelayAndRestart() {
        CommsAddrRec addr = new CommsAddrRec();
        XwJNI.comms_getAddr( m_jniGamePtr, addr );
        addr.remove( CommsConnType.COMMS_CONN_RELAY );
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
        dismissInviteAlert( nMissing, true );

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
                tryRematchInvites( false );
            } else {
                if ( !m_haveInvited ) {
                    m_haveInvited = true;
                    m_room = room;
                    showDialog( DlgID.DLG_INVITE );
                    invalidateOptionsMenuIf();
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
        }

        if ( null != toastStr ) {
            m_toastStr = toastStr;
            if ( naMsg == 0 ) {
                dlgButtonClicked( Action.SHOW_EXPL_ACTION, 
                                  AlertDialog.BUTTON_POSITIVE, null );
            } else {
                showNotAgainDlgThen( naMsg, naKey, Action.SHOW_EXPL_ACTION );
            }
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
        public int userPickTileBlank( int playerNum, String[] texts)
        {
            m_texts = texts;
            waitBlockingDialog( DlgID.PICK_TILE_REQUESTBLANK_BLK, 0 );
            return m_resultCode;
        }

        @Override
        public int userPickTileTray( int playerNum, String[] texts, 
                                     String[] curTiles, int nPicked )
        {
            m_texts = texts;
            m_curTiles = TextUtils.join( ", ", curTiles );
            m_canUndoTiles = 0 < nPicked;
            waitBlockingDialog( DlgID.PICK_TILE_REQUESTTRAY_BLK, 
                                UtilCtxt.PICKER_PICKALL );
            return m_resultCode;
        }

        @Override
        public String askPassword( String name )
        {
            m_pwdName = name;

            waitBlockingDialog( DlgID.ASK_PASSWORD_BLK, 0 );

            String result = 0 == m_resultCode
                ? null      // means cancelled
                : m_passwdEdit.getText().toString();
            m_passwdEdit = null;
            return result;
        }

        @Override
        public void turnChanged( int newTurn )
        {
            if ( 0 <= newTurn ) {
                m_nMissing = 0;
                post( new Runnable() {
                        public void run() {
                            showNotAgainDlg( R.string.not_again_turnchanged, 
                                             R.string.key_notagain_turnchanged );
                        }
                    } );
                handleViaThread( JNICmd. CMD_ZOOM, -8 );
            }
        }

        @Override
        public boolean engineProgressCallback()
        {
            return ! m_jniThread.busy();
        }

        @Override
        public boolean userQuery( int id, String query )
        {
            boolean result;

            switch( id ) {
                // Though robot-move dialogs don't normally need to block,
                // if the player after this one is also a robot and we
                // don't block then a second dialog will replace this one.
                // So block.  Yuck.
            case UtilCtxt.QUERY_ROBOT_TRADE:
                m_dlgBytes = query;
                m_dlgTitle = R.string.info_title;
                waitBlockingDialog( DlgID.QUERY_INFORM_BLK, 0 );
                result = true;
                break;

                // These *are* blocking dialogs
            case UtilCtxt.QUERY_COMMIT_TURN:
                m_dlgBytes = query;
                m_dlgTitle = R.string.query_title;
                result = 0 != waitBlockingDialog( DlgID.QUERY_REQUEST_BLK, 0 );
                break;
            default:
                Assert.fail();
                result = false;
            }

            return result;
        }

        @Override
        public boolean confirmTrade( String[] tiles )
        {
            m_dlgTitle = R.string.info_title;
            m_dlgBytes = 
                getQuantityString( R.plurals.query_trade_fmt, tiles.length,
                                   tiles.length, TextUtils.join( ", ", tiles ));
            return 0 != waitBlockingDialog( DlgID.QUERY_REQUEST_BLK, 0 );
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
            m_connTypes = connTypes;
            Assert.assertTrue( isServer || 0 == nMissing );
            // DbgUtils.logf( "BoardDelegate.informMissing(isServer=%b, nDevs=%d, nMissing=%d)", 
            //                isServer, nDevs, nMissing );
            m_nGuestDevs = nDevs;

            // If we might have put up an alert earlier, take it down
            dismissInviteAlert( nMissing, !m_relayMissing );

            m_nMissing = nMissing; // will be 0 unless isServer is true

            if ( null != connTypes && 0 == connTypes.size() ) {
                askNoAddrsDelete();
            } else if ( 0 < nMissing && isServer && !m_haveInvited ) {
                post( new Runnable() {
                        public void run() {
                            showDialog( DlgID.DLG_INVITE );
                        }
                    } );
            }
        }

        @Override
        public void informMove( String expl, String words )
        {
            m_words = null == words? null : wordsToArray( words );
            nonBlockingDialog( DlgID.DLG_SCORES, expl );
            Utils.playNotificationSound( m_activity );
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

        // public void yOffsetChange( int maxOffset, int oldOffset, int newOffset )
        // {
        //     DbgUtils.logf( "yOffsetChange(maxOffset=%d)", maxOffset );
        //     m_view.setVerticalScrollBarEnabled( maxOffset > 0 );
        // }
        @Override
        public boolean warnIllegalWord( String dict, String[] words, int turn, 
                                        boolean turnLost )
        {
            boolean accept = turnLost;

            String wordsString = TextUtils.join( ", ", words );
            String message = 
                getString( R.string.ids_badwords_fmt, wordsString, dict );

            if ( turnLost ) {
                m_dlgBytes = message + getString( R.string.badwords_lost );
                m_dlgTitle = R.string.badwords_title;
                waitBlockingDialog( DlgID.DLG_BADWORDS_BLK, 0 );
            } else {
                m_dlgBytes = message + getString( R.string.badwords_accept );
                m_dlgTitle = R.string.query_title;
                accept = 0 != waitBlockingDialog( DlgID.QUERY_REQUEST_BLK, 0 );
            }

            return accept;
        }

        // Let's have this block in case there are multiple messages.  If
        // we don't block the jni thread will continue processing messages
        // and may stack dialogs on top of this one.  Including later
        // chat-messages.
        @Override
        public void showChat( final String msg, final int fromIndx,
                              String fromPlayer )
        {
            if ( BuildConstants.CHAT_SUPPORTED ) {
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
        }
    } // class BoardUtilCtxt 

    private void doResume( boolean isStart )
    {
        boolean firstStart = null == m_handler;
        if ( firstStart ) {
            m_handler = new Handler();
            m_blockingDlgID = DlgID.NONE;
        }

        try {
            resumeGame( isStart );
            if ( !isStart ) {
                setKeepScreenOn();
                ConnStatusHandler.setHandler( this );
            }
        } catch ( GameUtils.NoSuchGameException nsge ) {
            finish();
        }
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
                setTitle( GameUtils.getName( m_activity, m_rowid ) );
            }

            positionToolbar( isPortrait() );
            populateToolbar();
            adjustTradeVisibility();

            int flags = DBUtils.getMsgFlags( m_activity, m_rowid );
            if ( 0 != (GameSummary.MSG_FLAGS_CHAT & flags) ) {
                startChatActivity();
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

            if ( m_gi.serverRole != DeviceRole.SERVER_STANDALONE ) {
                warnIfNoTransport();
                trySendChats();
                tickle( isStart );
                tryInvites();
            }
        }
    } // resumeGame

    // private void loadGame( boolean isStart )
    // {
    //     if ( null == m_jniGamePtr ) {
    //         try {
    //             String gameName = DBUtils.getName( m_activity, m_rowid );
    //             String[] dictNames;

    //             Assert.assertNull( m_gameLock );
    //             m_gameLock = m_jniThreadRef.getLock();
    //             if ( null == m_gameLock ) {
    //                 dictNames = GameUtils.dictNames( m_activity, m_rowid );
    //             } else {
    //                 dictNames = GameUtils.dictNames( m_activity, m_gameLock );
    //             }
    //             DictUtils.DictPairs pairs = DictUtils.openDicts( m_activity, dictNames );

    //             if ( pairs.anyMissing( dictNames ) ) {
    //                 showDictGoneFinish();
    //             } else {
    //                 if ( null == m_gameLock ) {
    //                     m_gameLock = new GameLock( m_rowid, true ).lock();
    //                 }

    //                 // PENDING: there's no point in re-opening the game if
    //                 // it's already open!
    //                 byte[] stream = GameUtils.savedGame( m_activity, m_gameLock );
    //                 m_gi = new CurGameInfo( m_activity );
    //                 m_gi.setName( gameName );
    //                 XwJNI.gi_from_stream( m_gi, stream );
    //                 String langName = m_gi.langName();

    //                 m_summary = DBUtils.getSummary( m_activity, m_gameLock );
    //                 m_relayMissing = m_summary.relayConnectPending();

    //                 setThis( this );

    //                 m_jniGamePtr = XwJNI.initJNI( m_rowid );

    //                 if ( m_gi.serverRole != DeviceRole.SERVER_STANDALONE ) {
    //                     m_xport = new CommsTransport( m_activity, this, m_rowid,
    //                                                   m_gi.serverRole );
    //                 }

    //                 CommonPrefs cp = CommonPrefs.get( m_activity );
    //                 if ( null == stream ||
    //                      ! XwJNI.game_makeFromStream( m_jniGamePtr, stream, 
    //                                                   m_gi, dictNames, 
    //                                                   pairs.m_bytes, 
    //                                                   pairs.m_paths, langName, 
    //                                                   m_utils, m_jniu, 
    //                                                   null, cp, m_xport ) ) {
    //                     XwJNI.game_makeNewGame( m_jniGamePtr, m_gi, m_utils, 
    //                                             m_jniu, null, cp, m_xport, 
    //                                             dictNames, pairs.m_bytes, 
    //                                             pairs.m_paths, langName );
    //                 }


    //                 m_jniThread = m_jniThreadRef.retain();

    //                 // see http://stackoverflow.com/questions/680180/where-to-stop-\
    //                 // destroy-threads-in-android-service-class
    //                 // m_jniThread.setDaemonOnce( true ); // firing
    //                 // m_jniThread.startOnce();

    //                 m_view.startHandling( m_activity, m_jniThread, m_jniGamePtr, m_gi,
    //                                       m_connTypes );
    //                 if ( null != m_xport ) {
    //                     // informMissing should have been called by now
    //                     Assert.assertNotNull( m_connTypes );
    //                     m_xport.setReceiver( m_jniThread, m_handler );
    //                 }
    //                 handleViaThread( JNICmd.CMD_START );

    //                 if ( !CommonPrefs.getHideTitleBar( m_activity ) ) {
    //                     setTitle( GameUtils.getName( m_activity, m_rowid ) );
    //                 }

    //                 positionToolbar( isPortrait() );
    //                 populateToolbar();
    //                 adjustTradeVisibility();

    //                 int flags = DBUtils.getMsgFlags( m_activity, m_rowid );
    //                 if ( 0 != (GameSummary.MSG_FLAGS_CHAT & flags) ) {
    //                     startChatActivity();
    //                 }
    //                 if ( m_overNotShown ) {
    //                     boolean auto = false;
    //                     if ( 0 != (GameSummary.MSG_FLAGS_GAMEOVER & flags) ) {
    //                         m_gameOver = true;
    //                     } else if ( DBUtils.gameOver( m_activity, m_rowid ) ) {
    //                         m_gameOver = true;
    //                         auto = true;
    //                     }
    //                     if ( m_gameOver ) {
    //                         m_overNotShown = false;
    //                         handleViaThread( JNICmd.CMD_POST_OVER, auto );
    //                     }
    //                 }
    //                 if ( 0 != flags ) {
    //                     DBUtils.setMsgFlags( m_rowid, GameSummary.MSG_FLAGS_NONE );
    //                 }

    //                 Utils.cancelNotification( m_activity, (int)m_rowid );

    //                 if ( null != m_xport ) {
    //                     warnIfNoTransport();
    //                     trySendChats();
    //                     tickle( isStart );
    //                     tryInvites();
    //                 }
    //             }
    //        } catch ( GameUtils.NoSuchGameException nsge ) {
    //             DbgUtils.loge( nsge );
    //             finish();
    //         }
    //     }
    // } // loadGame

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
                break;
            default:
                DbgUtils.logf( "tickle: unexpected type %s", 
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
            m_toolbar.setListener( Toolbar.BUTTON_BROWSE_DICT,
                                   R.string.not_again_browseall,
                                   R.string.key_na_browseall,
                                   Action.BUTTON_BROWSEALL_ACTION );
            m_toolbar.setLongClickListener( Toolbar.BUTTON_BROWSE_DICT,
                                            R.string.not_again_browse,
                                            R.string.key_na_browse,
                                            Action.BUTTON_BROWSE_ACTION );
            m_toolbar.setListener( Toolbar.BUTTON_HINT_PREV, 
                                   R.string.not_again_hintprev,
                                   R.string.key_notagain_hintprev,
                                   Action.PREV_HINT_ACTION );
            m_toolbar.setListener( Toolbar.BUTTON_HINT_NEXT,
                                   R.string.not_again_hintnext,
                                   R.string.key_notagain_hintnext,
                                   Action.NEXT_HINT_ACTION );
            m_toolbar.setListener( Toolbar.BUTTON_JUGGLE,
                                   R.string.not_again_juggle,
                                   R.string.key_notagain_juggle,
                                   Action.JUGGLE_ACTION );
            m_toolbar.setListener( Toolbar.BUTTON_FLIP,
                                   R.string.not_again_flip,
                                   R.string.key_notagain_flip,
                                   Action.FLIP_ACTION );
            m_toolbar.setListener( Toolbar.BUTTON_ZOOM,
                                   R.string.not_again_zoom,
                                   R.string.key_notagain_zoom,
                                   Action.ZOOM_ACTION );
            m_toolbar.setListener( Toolbar.BUTTON_VALUES,
                                   R.string.not_again_values,
                                   R.string.key_na_values,
                                   Action.VALUES_ACTION );
            m_toolbar.setListener( Toolbar.BUTTON_UNDO,
                                   R.string.not_again_undo,
                                   R.string.key_notagain_undo,
                                   Action.UNDO_ACTION );
            if ( BuildConstants.CHAT_SUPPORTED ) {
                m_toolbar.setListener( Toolbar.BUTTON_CHAT,
                                       R.string.not_again_chat, 
                                       R.string.key_notagain_chat,
                                       Action.CHAT_ACTION );
            }
        }
    } // populateToolbar

    private OnDismissListener makeODLforBlocking( final int id )
    {
        return new OnDismissListener() {
            public void onDismiss( DialogInterface di ) {
                releaseIfBlocking();
                removeDialog( id );
            }
        };
    }

    private int waitBlockingDialog( final DlgID dlgID, int cancelResult )
    {
        int result = cancelResult;
        // this has been true; dunno why
        if ( DlgID.NONE != m_blockingDlgID ) {
            DbgUtils.logf( "waitBlockingDialog: dropping dlgID %d b/c %d set",
                           dlgID, m_blockingDlgID );
        } else {
            setBlockingThread();
            m_resultCode = cancelResult;

            if ( post( new Runnable() {
                    public void run() {
                        m_blockingDlgID = dlgID;
                        showDialog( dlgID );
                    }
                } ) ) {

                try {
                    m_forResultWait.acquire();
                } catch ( java.lang.InterruptedException ie ) {
                    DbgUtils.loge( ie );
                    if ( DlgID.NONE != m_blockingDlgID ) {
                        try {
                            dismissDialog( m_blockingDlgID );
                        } catch ( java.lang.IllegalArgumentException iae ) {
                            DbgUtils.loge( iae );
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

    private void nonBlockingDialog( final DlgID dlgID, String txt ) 
    {
        switch ( dlgID ) {
        case DLG_OKONLY:
        case DLG_SCORES:
            m_dlgTitle = R.string.info_title;
            break;
        case DLG_USEDICT:
        case DLG_GETDICT:
            m_dlgTitle = R.string.inform_dict_title;
            break;

        default:
            Assert.fail();
        }

        m_dlgBytes = txt;
        runOnUiThread( new Runnable() {
                public void run() {
                    showDialog( dlgID );
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
        if ( BuildConstants.CHAT_SUPPORTED ) {
            int curPlayer = XwJNI.board_getSelPlayer( m_jniGamePtr );
            String[] names = m_gi.playerNames();
            boolean[] locs = m_gi.playersLocal(); // to convert old histories
            ChatDelegate.startForResult( getDelegator(), RequestCode.CHAT_REQUEST,
                                         m_rowid, curPlayer, names, locs );
        }
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
            m_summary = null;

            m_view.stopHandling();

            if ( XWPrefs.getThumbEnabled( m_activity ) ) {
                // Before we dispose, and after JNIThread has
                // relinquished interest, redraw on smaller scale.
                Bitmap thumb = 
                    GameUtils.takeSnapshot( m_activity, m_jniGamePtr, m_gi );
                DBUtils.saveThumbnail( m_activity, m_gameLock, thumb );
            }

            m_gi = null;
            m_gameLock = null;
        }
    }
    
    private void waitCloseGame( boolean save )
    {
        pauseGame();
        if ( null != m_jniThread ) {
            // m_jniGamePtr.release();
            // m_jniGamePtr = null;
            m_gi = null;

            // m_gameLock.unlock(); // likely the problem
            m_gameLock = null;
        }
    }

    private void warnIfNoTransport()
    {
        if ( m_connTypes.contains( CommsConnType.COMMS_CONN_SMS ) ) {
            if ( XWApp.SMSSUPPORTED && !XWPrefs.getSMSEnabled( m_activity ) ) {
                showConfirmThen( R.string.warn_sms_disabled, 
                                 R.string.button_enable_sms,
                                 R.string.button_later,
                                 Action.ENABLE_SMS_ASK );
            }
        }
    }
    
    private void trySendChats()
    {
        if ( BuildConstants.CHAT_SUPPORTED && null != m_jniThread ) {
            Iterator<String> iter = m_pendingChats.iterator();
            while ( iter.hasNext() ) {
                handleViaThread( JNICmd.CMD_SENDCHAT, iter.next() );
            }
            m_pendingChats.clear();
        }
    }

    private void tryInvites()
    {
        if ( 0 < m_nMissing && m_summary.hasRematchInfo() ) {
            tryRematchInvites( false );
        } else if ( null != m_missingDevs ) {
            Assert.assertNotNull( m_missingMeans );
            String gameName = GameUtils.getName( m_activity, m_rowid );
            m_invitesPending = m_missingDevs.length;
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
                        DbgUtils.loge( nfi );
                    }
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

    private void updateToolbar()
    {
        if ( null != m_toolbar ) {
            m_toolbar.update( Toolbar.BUTTON_FLIP, m_gsi.visTileCount >= 1 );
            m_toolbar.update( Toolbar.BUTTON_VALUES, m_gsi.visTileCount >= 1 );
            m_toolbar.update( Toolbar.BUTTON_JUGGLE, m_gsi.canShuffle );
            m_toolbar.update( Toolbar.BUTTON_UNDO, m_gsi.canRedo );
            m_toolbar.update( Toolbar.BUTTON_HINT_PREV, m_gsi.canHint );
            m_toolbar.update( Toolbar.BUTTON_HINT_NEXT, m_gsi.canHint );
            m_toolbar.update( Toolbar.BUTTON_CHAT, 
                              BuildConstants.CHAT_SUPPORTED && m_gsi.canChat );
            m_toolbar.update( Toolbar.BUTTON_BROWSE_DICT, 
                              null != m_gi.dictName( m_view.getCurPlayer() ) );
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
            DbgUtils.logf( "BoardDelegate.post(): dropping b/c handler null" );
            DbgUtils.printStack();
        }
        return canPost;
    }

    private void postDelayed( Runnable runnable, int when )
    {
        if ( null != m_handler ) {
            m_handler.postDelayed( runnable, when );
        } else {
            DbgUtils.logf( "postDelayed: dropping %d because handler null", when );
        }
    }

    private void removeCallbacks( Runnable which )
    {
        if ( null != m_handler ) {
            m_handler.removeCallbacks( which );
        } else {
            DbgUtils.logf( "removeCallbacks: dropping %h because handler null", 
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
        GameSummary summary = DBUtils.getSummary( context, rowID, 1 );
        return null != summary && rematchSupported( null, summary );
    }

    private static boolean rematchSupported( Context context, 
                                             GameSummary summary )
    {
        boolean supported = false;
        if ( XWApp.REMATCH_SUPPORTED ) {
            // standalone games are easy to rematch
            supported = summary.serverRole == DeviceRole.SERVER_STANDALONE;

            if ( !supported ) {
                if ( 2 == summary.nPlayers ) {
                    if ( !summary.anyMissing() ) {
                        CommsConnTypeSet connTypes = summary.conTypes;
                        supported = connTypes.contains( CommsConnType.COMMS_CONN_BT )
                            || connTypes.contains( CommsConnType.COMMS_CONN_SMS  )
                            || connTypes.contains( CommsConnType.COMMS_CONN_RELAY );
                    }
                } else if ( null != context ) {
                    // show the button if people haven't dismissed the hint yet
                    supported = ! XWPrefs
                        .getPrefsBoolean( context, 
                                          R.string.key_na_rematch_two_only, 
                                          false );
                }
            }
        }
        return supported;
    }

    private void doRematchIf()
    {
        if ( doRematchIf( m_activity, this, m_rowid, m_summary, m_gi, 
                          m_jniGamePtr ) ) {
            finish();
        }
    }

    private static boolean doRematchIf( Activity activity, DelegateBase dlgt, 
                                        long rowid, GameSummary summary, 
                                        CurGameInfo gi, GamePtr jniGamePtr )
    {
        boolean success = false;
        if ( XWApp.REMATCH_SUPPORTED ) {
            boolean doIt = true;
            String phone = null;
            String btAddr = null;
            String relayID = null;
            if ( DeviceRole.SERVER_STANDALONE == gi.serverRole ) {
                // nothing to do??
            } else if ( 2 != gi.nPlayers ) {
                Assert.assertNotNull( dlgt );
                if ( null != dlgt ) {
                    dlgt.showNotAgainDlg( R.string.not_again_rematch_two_only, 
                                          R.string.key_na_rematch_two_only );
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
                }
            }

            if ( doIt ) {
                CommsConnTypeSet connTypes = summary.conTypes;
                String newName = summary.getRematchName();
                Intent intent = GamesListDelegate
                    .makeRematchIntent( activity, rowid, gi, connTypes, btAddr, 
                                        phone, relayID, newName );
                if ( null != intent ) {
                    activity.startActivity( intent );
                    success = true;
                }
            }
        }
        return success;
    }

    public static void setupRematchFor( Activity activity, long rowID )
    {
        GameLock lock = new GameLock( rowID, false );
        if ( lock.tryLock() ) {
            GameSummary summary = DBUtils.getSummary( activity, lock );
            CurGameInfo gi = new CurGameInfo( activity );
            GamePtr gamePtr = GameUtils.loadMakeGame( activity, gi, lock );

            doRematchIf( activity, null, rowID, summary, gi, gamePtr );

            gamePtr.release();
            lock.unlock();
        } else {
            DbgUtils.logf( "setupRematchFor(): unable to lock game" );
        }
    }

    private void tryRematchInvites( boolean force )
    {
        if ( XWApp.REMATCH_SUPPORTED ) {
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

                showToast( R.string.rematch_sent_toast );
            }
        }
    }

    private void sendSMSInviteIf( String phone, NetLaunchInfo nli,
                                  boolean askOk )
    {
        if ( XWPrefs.getSMSEnabled( m_activity ) ) {
            SMSService.inviteRemote( m_activity, phone, nli );
            recordInviteSent( InviteMeans.SMS, phone );
        } else if ( askOk ) {
            showConfirmThen( R.string.warn_sms_disabled,
                             R.string.button_enable_sms,
                             R.string.button_later,
                             Action.ENABLE_SMS_ASK, nli, phone );
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
            DbgUtils.logf( "retrySMSInvites: tests failed" );
        }
    }
    
    private void recordInviteSent( InviteMeans means, String dev )
    {
        DBUtils.recordInviteSent( m_activity, m_rowid, means, dev );
    }

    private void handleViaThread( JNICmd cmd, Object... args )
    {
        if ( null == m_jniThread ) {
            DbgUtils.logf( "BoardDelegate: not calling handle(%s)", cmd.toString() );
            DbgUtils.printStack();
        } else {
            m_jniThread.handle( cmd, args );
        }
    }
} // class BoardDelegate
