/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2009 - 2013 by Eric House (xwords@eehouse.org).  All
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
import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MenuInflater;
import android.view.KeyEvent;
import android.view.Window;
import android.os.Handler;
import android.os.Message;
import android.content.Intent;
import java.util.concurrent.Semaphore;
import java.util.ArrayList;
import java.util.Iterator;
import android.app.Dialog;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.content.DialogInterface.OnDismissListener;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;
import junit.framework.Assert;
import android.content.res.Configuration;
import android.content.pm.ActivityInfo;

import org.eehouse.android.xw4.jni.*;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.JNIThread.*;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;


public class BoardActivity extends XWActivity 
    implements TransportProcs.TPMsgHandler, View.OnClickListener,
               DictImportActivity.DownloadFinishedListener, 
               ConnStatusHandler.ConnStatusCBacks {

    public static final String INTENT_KEY_CHAT = "chat";

    private static final int DLG_OKONLY = DlgDelegate.DIALOG_LAST + 1;
    private static final int DLG_BADWORDS = DLG_OKONLY + 1;
    private static final int QUERY_REQUEST_BLK = DLG_OKONLY + 2;
    private static final int QUERY_INFORM_BLK = DLG_OKONLY + 3;
    private static final int PICK_TILE_REQUESTBLANK_BLK = DLG_OKONLY + 4;
    private static final int ASK_PASSWORD_BLK = DLG_OKONLY + 5;
    private static final int DLG_RETRY = DLG_OKONLY + 6;
    private static final int QUERY_ENDGAME = DLG_OKONLY + 7;
    private static final int DLG_DELETED = DLG_OKONLY + 8;
    private static final int DLG_INVITE = DLG_OKONLY + 9;
    private static final int DLG_SCORES_BLK = DLG_OKONLY + 10;
    private static final int PICK_TILE_REQUESTTRAY_BLK = DLG_OKONLY + 11;
    private static final int DLG_USEDICT = DLG_OKONLY + 12;
    private static final int DLG_GETDICT = DLG_OKONLY + 13;
    private static final int GAME_OVER = DLG_OKONLY + 14;
    private static final int DLG_CONNSTAT = DLG_OKONLY + 15;

    private static final int CHAT_REQUEST = 1;
    private static final int BT_INVITE_RESULT = 2;
    private static final int SMS_INVITE_RESULT = 3;

    private static final int SCREEN_ON_TIME = 10 * 60 * 1000; // 10 mins

    private static final int UNDO_LAST_ACTION = 1;
    private static final int LAUNCH_INVITE_ACTION = 2;
    private static final int SYNC_ACTION = 3;
    private static final int COMMIT_ACTION = 4;
    private static final int SHOW_EXPL_ACTION = 5;
    private static final int PREV_HINT_ACTION = 6;
    private static final int NEXT_HINT_ACTION = 7;
    private static final int JUGGLE_ACTION = 8;
    private static final int FLIP_ACTION = 9;
    private static final int ZOOM_ACTION = 10;
    private static final int UNDO_ACTION = 11;
    private static final int CHAT_ACTION = 12;
    private static final int START_TRADE_ACTION = 13;
    private static final int LOOKUP_ACTION = 14;
    private static final int BUTTON_BROWSE_ACTION = 15;
    private static final int VALUES_ACTION = 16;
    private static final int BT_PICK_ACTION = 17;
    private static final int SMS_PICK_ACTION = 18;
    private static final int SMS_CONFIG_ACTION = 19;
    private static final int BUTTON_BROWSEALL_ACTION = 20;

    private static final String DLG_TITLE = "DLG_TITLE";
    private static final String DLG_TITLESTR = "DLG_TITLESTR";
    private static final String DLG_BYTES = "DLG_BYTES";
    private static final String ROOM = "ROOM";
    private static final String PWDNAME = "PWDNAME";
    private static final String TOASTSTR = "TOASTSTR";
    private static final String WORDS = "WORDS";
    private static final String GETDICT = "GETDICT";

    private BoardView m_view;
    private int m_jniGamePtr;
    private GameLock m_gameLock;
    private CurGameInfo m_gi;
    private CommsTransport m_xport;
    private Handler m_handler = null;
    private TimerRunnable[] m_timers;
    private Runnable m_screenTimer;
    private long m_rowid;
    private Toolbar m_toolbar;
    private View m_tradeButtons;
    private Button m_exchCommmitButton;
    private Button m_exchCancelButton;

    private ArrayList<String> m_pendingChats;

    private String m_dlgBytes = null;
    private EditText m_passwdEdit = null;
    private LinearLayout m_passwdLyt = null;
    private int m_dlgTitle;
    private String m_dlgTitleStr;
    private String[] m_texts;
    private CommsConnType m_connType = CommsConnType.COMMS_CONN_NONE;
    private String[] m_missingDevs;
    private String m_curTiles;
    private boolean m_canUndoTiles;
    private boolean m_firingPrefs;
    private JNIUtils m_jniu;
    private boolean m_volKeysZoom;
    private boolean m_inTrade;  // save this in bundle?
    private BoardUtilCtxt m_utils;
    private int m_nMissingPlayers = -1;
    private int m_invitesPending;
    private boolean m_gameOver = false;

    // call startActivityForResult synchronously
    private Semaphore m_forResultWait = new Semaphore(0);
    private int m_resultCode;

    private Thread m_blockingThread;
    private JNIThread m_jniThread;
    private JNIThread.GameStateInfo m_gsi;
    private boolean m_blockingDlgPosted = false;

    private String m_room;
    private String m_toastStr;
    private String[] m_words;
    private String m_pwdName;
    private String m_getDict;

    private int m_missing;
    private boolean m_haveInvited = false;
    private boolean m_overNotShown;

    private static BoardActivity s_this = null;
    private static Class s_thisLocker = BoardActivity.class;

    public static boolean feedMessage( int gameID, byte[] msg, 
                                       CommsAddrRec retAddr )
    {
        boolean delivered = false;
        synchronized( s_thisLocker ) {
            if ( null != s_this ) {
                Assert.assertNotNull( s_this.m_gi );
                Assert.assertNotNull( s_this.m_gameLock );
                Assert.assertNotNull( s_this.m_jniThread );
                if ( gameID == s_this.m_gi.gameID ) {
                    s_this.m_jniThread.handle( JNICmd.CMD_RECEIVE, msg,
                                               retAddr );
                    delivered = true;
                }
            }
        }
        return delivered;
    }

    public static boolean feedMessage( long rowid, byte[] msg )
    {
        boolean delivered = false;
        synchronized( s_thisLocker ) {
            if ( null != s_this ) {
                Assert.assertNotNull( s_this.m_gi );
                Assert.assertNotNull( s_this.m_gameLock );
                Assert.assertNotNull( s_this.m_jniThread );
                if ( rowid == s_this.m_rowid ) {
                    s_this.m_jniThread.handle( JNICmd.CMD_RECEIVE, msg, null );
                    delivered = true;
                }
            }
        }
        return delivered;
    }

    public static boolean feedMessages( long rowid, byte[][] msgs )
    {
        boolean delivered = false;
        Assert.assertNotNull( msgs );
        synchronized( s_thisLocker ) {
            if ( null != s_this ) {
                Assert.assertNotNull( s_this.m_gi );
                Assert.assertNotNull( s_this.m_gameLock );
                Assert.assertNotNull( s_this.m_jniThread );
                if ( rowid == s_this.m_rowid ) {
                    delivered = true; // even if no messages!
                    for ( byte[] msg : msgs ) {
                        s_this.m_jniThread.handle( JNICmd.CMD_RECEIVE, msg,
                                                   null );
                    }
                }
            }
        }
        return delivered;
    }

    private static void setThis( BoardActivity self )
    {
        synchronized( s_thisLocker ) {
            Assert.assertNull( s_this );
            s_this = self;
        }
    }

    private static void clearThis()
    {
        synchronized( s_thisLocker ) {
            Assert.assertNotNull( s_this );
            s_this = null;
        }
    }

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

    @Override
    protected Dialog onCreateDialog( final int id )
    {
        Dialog dialog = super.onCreateDialog( id );
        if ( null == dialog ) {
            DialogInterface.OnClickListener lstnr;
            AlertDialog.Builder ab;

            switch ( id ) {
            case DLG_OKONLY:
            case DLG_BADWORDS:
            case DLG_RETRY:
            case GAME_OVER:
            case DLG_CONNSTAT:
                ab = new AlertDialog.Builder( this )
                    .setTitle( m_dlgTitle )
                    .setMessage( m_dlgBytes )
                    .setPositiveButton( R.string.button_ok, null );
                if ( DLG_RETRY == id ) {
                    lstnr = new DialogInterface.OnClickListener() {
                            public void onClick( DialogInterface dlg, 
                                                 int whichButton ) {
                                m_jniThread.handle( JNICmd.CMD_RESET );
                            }
                        };
                    ab.setNegativeButton( R.string.button_retry, lstnr );
                } else if ( XWApp.REMATCH_SUPPORTED && GAME_OVER == id ) {
                    lstnr = new DialogInterface.OnClickListener() {
                            public void onClick( DialogInterface dlg, 
                                                 int whichButton ) {
                                doRematch();
                            }
                        };
                    ab.setNegativeButton( R.string.button_rematch, lstnr );
                } else if ( DLG_CONNSTAT == id &&
                            CommsConnType.COMMS_CONN_RELAY == m_connType ) {
                    lstnr = new DialogInterface.OnClickListener() {
                            public void onClick( DialogInterface dlg, 
                                                 int whichButton ) {
                                RelayService.reset( BoardActivity.this );
                            }
                        };
                    ab.setNegativeButton( R.string.button_reconnect, lstnr );
                }
                dialog = ab.create();
                Utils.setRemoveOnDismiss( this, dialog, id );
                break;

            case DLG_USEDICT:
            case DLG_GETDICT:
                lstnr = new DialogInterface.OnClickListener() {
                        public void onClick( DialogInterface dlg, 
                                             int whichButton ) {
                            if ( DLG_USEDICT == id ) {
                                setGotGameDict( m_getDict );
                            } else {
                                DictImportActivity
                                    .downloadDictInBack( BoardActivity.this,
                                                         m_gi.dictLang,
                                                         m_getDict,
                                                         BoardActivity.this );
                            }
                        }
                    };
                dialog = new AlertDialog.Builder( this )
                    .setTitle( m_dlgTitle )
                    .setMessage( m_dlgBytes )
                    .setPositiveButton( R.string.button_yes, lstnr )
                    .setNegativeButton( R.string.button_no, null )
                    .create();
                Utils.setRemoveOnDismiss( this, dialog, id );
                break;

            case DLG_DELETED:
                ab = new AlertDialog.Builder( BoardActivity.this )
                    .setTitle( R.string.query_title )
                    .setMessage( R.string.msg_dev_deleted )
                    .setPositiveButton( R.string.button_ok, null );
                lstnr = new DialogInterface.OnClickListener() {
                        public void onClick( DialogInterface dlg, 
                                             int whichButton ) {

                            waitCloseGame( false );
                            GameUtils.deleteGame( BoardActivity.this,
                                                  m_rowid, false );
                            finish();
                        }
                    };
                ab.setNegativeButton( R.string.button_discard, lstnr );
                dialog = ab.create();
                break;

            case QUERY_REQUEST_BLK:
            case QUERY_INFORM_BLK:
            case DLG_SCORES_BLK:
                ab = new AlertDialog.Builder( this )
                    .setMessage( m_dlgBytes );
                if ( 0 != m_dlgTitle ) {
                    ab.setTitle( m_dlgTitle );
                }
                lstnr = new DialogInterface.OnClickListener() {
                        public void onClick( DialogInterface dialog, 
                                             int whichButton ) {
                            m_resultCode = 1;
                        }
                    };
                ab.setPositiveButton( QUERY_REQUEST_BLK == id ?
                                      R.string.button_yes : R.string.button_ok,
                                      lstnr );
                if ( QUERY_REQUEST_BLK == id ) {
                    lstnr = new DialogInterface.OnClickListener() {
                            public void onClick( DialogInterface dialog, 
                                                 int whichButton ) {
                                m_resultCode = 0;
                            }
                        };
                    ab.setNegativeButton( R.string.button_no, lstnr );
                } else if ( DLG_SCORES_BLK == id ) {
                    if ( null != m_words && m_words.length > 0 ) {
                        String buttonTxt;
                        if ( m_words.length == 1 ) {
                            buttonTxt = Utils.format( this, 
                                                      R.string.button_lookupf,
                                                      m_words[0] );
                        } else {
                            buttonTxt = getString( R.string.button_lookup );
                        }
                        lstnr = new DialogInterface.OnClickListener() {
                                public void onClick( DialogInterface dialog, 
                                                     int whichButton ) {
                                    showNotAgainDlgThen( R.string.
                                                         not_again_lookup, 
                                                         R.string.
                                                         key_na_lookup, 
                                                         LOOKUP_ACTION );
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
                ab = new AlertDialog.Builder( this );
                lstnr = new DialogInterface.OnClickListener() {
                        public void onClick( DialogInterface dialog, 
                                             int item ) {
                            m_resultCode = item;
                        }
                    };
                ab.setItems( m_texts, lstnr );

                if ( PICK_TILE_REQUESTBLANK_BLK == id ) {
                    ab.setTitle( R.string.title_tile_picker );
                } else {
                    ab.setTitle( Utils.format( this, R.string.cur_tilesf,
                                               m_curTiles ) );
                    if ( m_canUndoTiles ) {
                        DialogInterface.OnClickListener undoClicked =
                            new DialogInterface.OnClickListener() {
                                public void onClick( DialogInterface dialog, 
                                                     int whichButton ) {
                                    m_resultCode = UtilCtxt.PICKER_BACKUP;
                                    removeDialog( id );
                                }
                            };
                        ab.setPositiveButton( R.string.tilepick_undo, 
                                              undoClicked );
                    }
                    DialogInterface.OnClickListener doAllClicked =
                        new DialogInterface.OnClickListener() {
                            public void onClick( DialogInterface dialog, 
                                                 int whichButton ) {
                                m_resultCode = UtilCtxt.PICKER_PICKALL;
                                removeDialog( id );
                            }
                        };
                    ab.setNegativeButton( R.string.tilepick_all, doAllClicked );
                }

                dialog = ab.create();
                dialog.setOnDismissListener( makeODLforBlocking( id ) );
                break;

            case ASK_PASSWORD_BLK:
                if ( null == m_passwdLyt ) {
                    setupPasswdVars();
                }
                m_passwdEdit.setText( "", TextView.BufferType.EDITABLE );
                ab = new AlertDialog.Builder( this )
                    .setTitle( m_dlgTitleStr )
                    .setView( m_passwdLyt )
                    .setPositiveButton( R.string.button_ok,
                                        new DialogInterface.OnClickListener() {
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
                dialog = new AlertDialog.Builder( this )
                    .setTitle( R.string.query_title )
                    .setMessage( R.string.ids_endnow )
                    .setPositiveButton( R.string.button_yes,
                                        new DialogInterface.OnClickListener() {
                                            public void 
                                                onClick( DialogInterface dlg, 
                                                         int item ) {
                                                m_jniThread.
                                                    handle(JNICmd.CMD_ENDGAME);
                                            }
                                        })
                    .setNegativeButton( R.string.button_no, null )
                    .create();
                break;
            case DLG_INVITE:
                if ( null != m_room ) {
                    lstnr = new DialogInterface.OnClickListener() {
                            public void onClick( DialogInterface dialog, 
                                                 int item ) {
                                showEmailOrSMSThen( LAUNCH_INVITE_ACTION );
                            }
                        };
                    dialog = new AlertDialog.Builder( this )
                        .setTitle( R.string.query_title )
                        .setMessage( "" )
                        .setPositiveButton( R.string.button_yes, lstnr )
                        .setNegativeButton( R.string.button_no, null )
                        .create();
                }
                break;

            default:
                // just drop it; super.onCreateDialog likely failed
                break;
            }
        }
        return dialog;
    } // onCreateDialog

    @Override
    public void onPrepareDialog( int id, Dialog dialog )
    {
        switch( id ) {
        case DLG_INVITE:
            AlertDialog ad = (AlertDialog)dialog;
            String message = getString( R.string.invite_msgf, m_missing );
            if ( m_missing > 1 ) {
                message += getString( R.string.invite_multiple );
            }
            ad.setMessage( message );
            break;
        default:
            super.onPrepareDialog( id, dialog );
            break;
        }
    }

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );
        getBundledData( savedInstanceState );

        if ( CommonPrefs.getHideTitleBar( this ) ) {
            requestWindowFeature( Window.FEATURE_NO_TITLE );
        }

        if ( GitVersion.CHAT_SUPPORTED ) {
            m_pendingChats = new ArrayList<String>();
        }

        m_utils = new BoardUtilCtxt();
        m_jniu = JNIUtilsImpl.get( this );
        setContentView( R.layout.board );
        m_timers = new TimerRunnable[4]; // needs to be in sync with
                                         // XWTimerReason
        m_view = (BoardView)findViewById( R.id.board_view );
        m_tradeButtons = findViewById( R.id.exchange_buttons );
        m_exchCommmitButton = (Button)findViewById( R.id.exchange_commit );
        if ( null != m_exchCommmitButton ) {
            m_exchCommmitButton.setOnClickListener( this );
        }
        m_exchCancelButton = (Button)findViewById( R.id.exchange_cancel );
        if ( null != m_exchCancelButton ) {
            m_exchCancelButton.setOnClickListener( this );
        }
        m_volKeysZoom = XWPrefs.getVolKeysZoom( this );

        Intent intent = getIntent();
        m_rowid = intent.getLongExtra( GameUtils.INTENT_KEY_ROWID, -1 );
        DbgUtils.logf( "BoardActivity: opening rowid %d", m_rowid );
        m_haveInvited = intent.getBooleanExtra( GameUtils.INVITED, false );
        m_overNotShown = true;

        setBackgroundColor();
        setKeepScreenOn();
    } // onCreate

    @Override
    protected void onPause()
    {
        m_handler = null;
        ConnStatusHandler.setHandler( null );
        waitCloseGame( true );
        super.onPause();
    }

    @Override
    protected void onResume()
    {
        super.onResume();
        m_handler = new Handler();
        m_blockingDlgPosted = false;

        setKeepScreenOn();

        loadGame();

        ConnStatusHandler.setHandler( this );
    }

    @Override
    protected void onSaveInstanceState( Bundle outState ) 
    {
        super.onSaveInstanceState( outState );
        outState.putInt( DLG_TITLESTR, m_dlgTitle );
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
    protected void onActivityResult( int requestCode, int resultCode, Intent data )
    {
        if ( Activity.RESULT_CANCELED != resultCode ) {
            switch ( requestCode ) {
            case CHAT_REQUEST:
                if ( GitVersion.CHAT_SUPPORTED ) {
                    String msg = data.getStringExtra( INTENT_KEY_CHAT );
                    if ( null != msg && msg.length() > 0 ) {
                        m_pendingChats.add( msg );
                        trySendChats();
                    }
                }
                break;
            case BT_INVITE_RESULT:
                // onActivityResult is called immediately *before*
                // onResume -- meaning m_gi etc are still null.
                m_missingDevs = data.getStringArrayExtra( BTInviteActivity.DEVS );
                break;
            case SMS_INVITE_RESULT:
                // onActivityResult is called immediately *before*
                // onResume -- meaning m_gi etc are still null.
                m_missingDevs = data.getStringArrayExtra( SMSInviteActivity.DEVS );
                break;
            }
        }
    }

    @Override
    public void onWindowFocusChanged( boolean hasFocus )
    {
        super.onWindowFocusChanged( hasFocus );
        if ( hasFocus ) {
            if ( m_firingPrefs ) {
                m_firingPrefs = false;
                m_volKeysZoom = XWPrefs.getVolKeysZoom( this );
                if ( null != m_jniThread ) {
                    m_jniThread.handle( JNICmd.CMD_PREFS_CHANGE );
                }
                // in case of change...
                setBackgroundColor();
                setKeepScreenOn();
            }
        }
    }

    @Override
    public boolean onKeyDown( int keyCode, KeyEvent event )
    {
        boolean handled = false;
        if ( null != m_jniThread ) {
            XwJNI.XP_Key xpKey = keyCodeToXPKey( keyCode );
            if ( XwJNI.XP_Key.XP_KEY_NONE != xpKey ) {
                m_jniThread.handle( JNICmd.CMD_KEYDOWN, xpKey );
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
        return handled || super.onKeyDown( keyCode, event );
    }

    @Override
    public boolean onKeyUp( int keyCode, KeyEvent event )
    {
        if ( null != m_jniThread ) {
            XwJNI.XP_Key xpKey = keyCodeToXPKey( keyCode );
            if ( XwJNI.XP_Key.XP_KEY_NONE != xpKey ) {
                m_jniThread.handle( JNICmd.CMD_KEYUP, xpKey );
            }
        }
        return super.onKeyUp( keyCode, event );
    }

    @Override
    public boolean onCreateOptionsMenu( Menu menu ) 
    {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate( R.menu.board_menu, menu );

        return true;
    }

    @Override
    public boolean onPrepareOptionsMenu( Menu menu ) 
    {
        super.onPrepareOptionsMenu( menu );
        boolean inTrade = false;
        MenuItem item;
        int strId;

        updateMenus( menu );

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
            item.setTitle( strId );
        }

        Utils.setItemVisible( menu, R.id.board_menu_undo_last, !inTrade );
        Utils.setItemVisible( menu, R.id.board_menu_tray, !inTrade );

        if ( !inTrade ) {
            boolean enabled = null == m_gsi || m_gsi.curTurnSelected;
            item = menu.findItem( R.id.board_menu_done );
            item.setVisible( enabled );
            if ( enabled ) {
                if ( 0 >= m_view.curPending() ) {
                    strId = R.string.board_menu_pass;
                } else {
                    strId = R.string.board_menu_done;
                }
                item.setTitle( strId );
            }
        }

        if ( m_gameOver || DBUtils.gameOver( this, m_rowid ) ) {
            m_gameOver = true;
            item = menu.findItem( R.id.board_menu_game_resign );
            item.setTitle( R.string.board_menu_game_final );
        }

        if ( DeviceRole.SERVER_STANDALONE == m_gi.serverRole ) {
            Utils.setItemVisible( menu, R.id.board_menu_game_resend, false );
            Utils.setItemVisible( menu, R.id.gamel_menu_checkmoves, false );
        }

        return true;
    } // onPrepareOptionsMenu

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
                                     R.string.key_notagain_done, COMMIT_ACTION );
            } else {
                dlgButtonClicked( COMMIT_ACTION, AlertDialog.BUTTON_POSITIVE );
            }
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
            showNotAgainDlgThen( R.string.not_again_trading, 
                                 R.string.key_notagain_trading,
                                 START_TRADE_ACTION );
            break;

        case R.id.board_menu_tray:
            cmd = JNICmd.CMD_TOGGLE_TRAY;
            break;
        case R.id.board_menu_undo_current:
            cmd = JNICmd.CMD_UNDO_CUR;
            break;
        case R.id.board_menu_undo_last:
            showConfirmThen( R.string.confirm_undo_last, UNDO_LAST_ACTION );
            break;

            // small devices only
        case R.id.board_menu_dict:
            String dictName = m_gi.dictName( m_view.getCurPlayer() );
            DictBrowseActivity.launch( this, dictName );
            break;

        case R.id.board_menu_game_counts:
            m_jniThread.handle( JNICmd.CMD_COUNTS_VALUES,
                                R.string.counts_values_title );
            break;
        case R.id.board_menu_game_left:
            m_jniThread.handle( JNICmd.CMD_REMAINING,
                                R.string.tiles_left_title );
            break;

        case R.id.board_menu_game_history:
            m_jniThread.handle( JNICmd.CMD_HISTORY, R.string.history_title );
            break;

        case R.id.board_menu_game_resign:
            m_jniThread.handle( JNICmd.CMD_FINAL, R.string.history_title );
            break;

        case R.id.board_menu_game_resend:
            m_jniThread.handle( JNICmd.CMD_RESEND, true, false );
            break;

        case R.id.gamel_menu_checkmoves:
            showNotAgainDlgThen( R.string.not_again_sync,
                                 R.string.key_notagain_sync,
                                 SYNC_ACTION );
            break;

        case R.id.board_menu_file_prefs:
            m_firingPrefs = true;
            Utils.launchSettings( this );
            break;

        case R.id.board_menu_file_about:
            showAboutDialog();
            break;

        default:
            DbgUtils.logf( "menuitem %d not handled", id );
            handled = false;
        }

        if ( handled && cmd != JNICmd.CMD_NONE ) {
            m_jniThread.handle( cmd );
        }
        return handled;
    }

    //////////////////////////////////////////////////
    // DlgDelegate.DlgClickNotify interface
    //////////////////////////////////////////////////
    @Override
    public void dlgButtonClicked( int id, int which )
    {
        if ( LAUNCH_INVITE_ACTION == id ) {
            if ( DlgDelegate.DISMISS_BUTTON != which ) {
                GameUtils.launchInviteActivity( BoardActivity.this,
                                                DlgDelegate.EMAIL_BTN == which,
                                                m_room, null, m_gi.dictLang, 
                                                m_gi.dictName, m_gi.nPlayers );
            }
        } else if ( AlertDialog.BUTTON_POSITIVE == which ) {
            JNICmd cmd = JNICmd.CMD_NONE;
            switch ( id ) {
            case UNDO_LAST_ACTION:
                cmd = JNICmd.CMD_UNDO_LAST;
                break;
            case SYNC_ACTION:
                doSyncMenuitem();
                break;
            case BT_PICK_ACTION:
                BTInviteActivity.launchForResult( this, m_nMissingPlayers, 
                                                  BT_INVITE_RESULT );
                break;
            case SMS_PICK_ACTION:
                SMSInviteActivity.launchForResult( this, m_nMissingPlayers, 
                                                   SMS_INVITE_RESULT );
                break;
            case SMS_CONFIG_ACTION:
                Utils.launchSettings( this );
                break;

            case COMMIT_ACTION:
                cmd = JNICmd.CMD_COMMIT;
                break;
            case SHOW_EXPL_ACTION:
                Utils.showToast( BoardActivity.this, m_toastStr );
                m_toastStr = null;
                break;
            case BUTTON_BROWSEALL_ACTION:
            case BUTTON_BROWSE_ACTION:
                String curDict = m_gi.dictName( m_view.getCurPlayer() );
                View button = m_toolbar.getViewFor( Toolbar.BUTTON_BROWSE_DICT );
                if ( BUTTON_BROWSEALL_ACTION == id &&
                     DictsActivity.handleDictsPopup( this, button, curDict ) ) {
                    break;
                }
                DictBrowseActivity.launch( this, curDict );
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
                Utils.showToast( this, R.string.entering_trade );
                cmd = JNICmd.CMD_TRADE;
                break;
            case LOOKUP_ACTION:
                launchLookup( m_words, m_gi.dictLang );
                break;
            default:
                Assert.fail();
            }

            if ( JNICmd.CMD_NONE != cmd ) {
                checkAndHandle( cmd );
            }
        }
    } // dlgButtonClicked

    //////////////////////////////////////////////////
    // View.OnClickListener interface
    //////////////////////////////////////////////////
    @Override
    public void onClick( View view ) 
    {
        if ( view == m_exchCommmitButton ) {
            m_jniThread.handle( JNICmd.CMD_COMMIT );
        } else if ( view == m_exchCancelButton ) {
            m_jniThread.handle( JNICmd.CMD_CANCELTRADE );
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
                updateStatusIn( this, this, CommsConnType.COMMS_CONN_BT, 
                                MultiService.MultiEvent.MESSAGE_ACCEPTED == event);
            break;
        case MESSAGE_NOGAME:
            int gameID = (Integer)args[0];
            if ( gameID == m_gi.gameID ) {
                post( new Runnable() {
                        public void run() {
                            showDialog( DLG_DELETED );
                        }
                    } );
            }
            break;

            // This can be BT or SMS.  In BT case there's a progress
            // thing going.  Not in SMS case.
        case NEWGAME_FAILURE:
            DbgUtils.logf( "failed to create game" );
        case NEWGAME_SUCCESS:
            final boolean success = 
                MultiService.MultiEvent.NEWGAME_SUCCESS == event;
            final boolean allHere = 0 == --m_invitesPending;
            m_handler.post( new Runnable() {
                    public void run() {
                        if ( allHere ) {
                            stopProgress();
                        }
                        if ( success ) {
                            DbgUtils.showf( BoardActivity.this, 
                                            R.string.invite_success );
                        }
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
        post( new Runnable() {
                public void run() {
                    handleConndMessage( room, devOrder, allHere, nMissing );
                }
            } );
    }

    public void tpmRelayErrorProc( TransportProcs.XWRELAY_ERROR relayErr )
    {
        int strID = -1;
        int dlgID = -1;
        boolean doToast = false;

        switch ( relayErr ) {
        case TOO_MANY:
            strID = R.string.msg_too_many;
            dlgID = DLG_OKONLY;
            break;
        case NO_ROOM:
            strID = R.string.msg_no_room;
            dlgID = DLG_RETRY;
            break;
        case DUP_ROOM:
            strID = R.string.msg_dup_room;
            dlgID = DLG_OKONLY;
            break;
        case LOST_OTHER:
        case OTHER_DISCON:
            strID = R.string.msg_lost_other;
            doToast = true;
            break;

        case DEADGAME:
        case DELETED:
            strID = R.string.msg_dev_deleted;
            dlgID = DLG_DELETED;
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
            Utils.showToast( this, strID );
        } else if ( dlgID >= 0 ) {
            final int strIDf = strID;
            final int dlgIDf = dlgID;
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
    // DictImportActivity.DownloadFinishedListener interface
    //////////////////////////////////////////////////
    public void downloadFinished( final String name, final boolean success )
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
        final String msg = ConnStatusHandler.getStatusText( this, m_connType );
        post( new Runnable() {
                public void run() {
                    m_dlgBytes = msg;
                    m_dlgTitle = R.string.info_title;
                    showDialog( DLG_CONNSTAT );
                }
            } );
    }

    public Handler getHandler()
    {
        return m_handler;
    }

    private void setGotGameDict( String getDict )
    {
        m_jniThread.setSaveDict( getDict );

        String msg = getString( R.string.reload_new_dict, getDict );
        Utils.showToast( this, msg );
        finish();
        GameUtils.launchGame( this, m_rowid, false );
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
        int naMsg = 0;
        int naKey = 0;
        String toastStr = null;
        if ( allHere ) {
            // All players have now joined the game.  The device that
            // created the room will assign tiles.  Then it will be
            // the first player's turn

            // Skip this until there's a way to show it only once per game
            if ( false ) {
                toastStr = getString( R.string.msg_relay_all_heref, room );
                if ( devOrder > 1 ) {
                    naMsg = R.string.not_again_conndall;
                    naKey = R.string.key_notagain_conndall;
                }
            }
        } else if ( nMissing > 0 ) {
            if ( !m_haveInvited ) {
                m_haveInvited = true;
                m_room = room;
                m_missing = nMissing;
                showDialog( DLG_INVITE );
            } else {
                toastStr = getString( R.string.msg_relay_waiting, devOrder,
                                      room, nMissing );
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
            m_toastStr = toastStr;
            if ( naMsg == 0 ) {
                dlgButtonClicked( SHOW_EXPL_ACTION, 
                                  AlertDialog.BUTTON_POSITIVE );
            } else {
                showNotAgainDlgThen( naMsg, naKey, SHOW_EXPL_ACTION );
            }
        }
    } // handleConndMessage

    private class BoardUtilCtxt extends UtilCtxtImpl {

        public BoardUtilCtxt()
        {
            super( BoardActivity.this );
        }

        @Override
        public void requestTime() 
        {
            post( new Runnable() {
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
            m_jniThread.handle( JNICmd.CMD_REMAINING,
                                R.string.tiles_left_title );
        }

        @Override
        public void setIsServer( boolean isServer )
        {
            DeviceRole newRole = isServer? DeviceRole.SERVER_ISSERVER
                : DeviceRole.SERVER_ISCLIENT;
            if ( newRole != m_gi.serverRole ) {
                m_gi.serverRole = newRole;
                if ( !isServer ) {
                    m_jniThread.handle( JNICmd.CMD_SWITCHCLIENT );
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
                            Utils.showToast( BoardActivity.this, bonusStr );
                        }
                    } );
            }
        }

        @Override
        public void playerScoreHeld( int player )
        {
            String expl = XwJNI.model_getPlayersLastScore( m_jniGamePtr, 
                                                           player );
            if ( expl.length() == 0 ) {
                expl = getString( R.string.no_moves_made );
            }
            String name = m_gi.players[player].name;
            final String text = String.format( "%s\n%s", name, expl );
            post( new Runnable() {
                    public void run() {
                        Utils.showToast( BoardActivity.this, text );
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
            waitBlockingDialog( PICK_TILE_REQUESTBLANK_BLK, 0 );
            return m_resultCode;
        }

        @Override
        public int userPickTileTray( int playerNum, String[] texts, 
                                     String[] curTiles, int nPicked )
        {
            m_texts = texts;
            m_curTiles = TextUtils.join( ", ", curTiles );
            m_canUndoTiles = 0 < nPicked;
            waitBlockingDialog( PICK_TILE_REQUESTTRAY_BLK, 
                                UtilCtxt.PICKER_PICKALL );
            return m_resultCode;
        }

        @Override
        public String askPassword( String name )
        {
            // call this each time dlg created or will get exception
            // for reusing m_passwdLyt
            m_pwdName = name;
            setupPasswdVars();  

            waitBlockingDialog( ASK_PASSWORD_BLK, 0 );

            String result = null;      // means cancelled
            if ( 0 != m_resultCode ) {
                result = m_passwdEdit.getText().toString();
            }
            return result;
        }

        @Override
        public void turnChanged( int newTurn )
        {
            if ( 0 <= newTurn ) {
                post( new Runnable() {
                        public void run() {
                            showNotAgainDlgThen( R.string.not_again_turnchanged, 
                                                 R.string.key_notagain_turnchanged );
                        }
                    } );
                m_jniThread.handle( JNICmd. CMD_ZOOM, -8 );
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
                waitBlockingDialog( QUERY_INFORM_BLK, 0 );
                result = true;
                break;

                // These *are* blocking dialogs
            case UtilCtxt.QUERY_COMMIT_TURN:
                m_dlgBytes = query;
                m_dlgTitle = R.string.query_title;
                result = 0 != waitBlockingDialog( QUERY_REQUEST_BLK, 0 );
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
                Utils.format( BoardActivity.this, R.string.query_tradef, 
                              TextUtils.join( ", ", tiles ) );
            return 0 != waitBlockingDialog( QUERY_REQUEST_BLK, 0 );
        }

        @Override
        public void userError( int code )
        {
            int resid = 0;
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
            }

            if ( resid != 0 ) {
                nonBlockingDialog( DLG_OKONLY, getString( resid ) );
            }
        } // userError

        @Override
        public void informMissing( boolean isServer, CommsConnType connType,
                                   final int nMissingPlayers )
        {
            m_connType = connType;

            int action = 0;
            if ( 0 < nMissingPlayers && isServer && !m_haveInvited ) {
                switch( connType ) {
                case COMMS_CONN_BT:
                    action = BT_PICK_ACTION;
                    break;
                case COMMS_CONN_SMS:
                    action = SMS_PICK_ACTION;
                    break;
                }
            }
            if ( 0 != action ) {
                m_haveInvited = true;
                final int faction = action;
                final String fmsg = getString( R.string.invite_msgf,
                                               nMissingPlayers );
                post( new Runnable() {
                        public void run() {
                            DbgUtils.showf( BoardActivity.this, 
                                            getString( R.string.players_missf,
                                                       nMissingPlayers ) );
                            m_nMissingPlayers = nMissingPlayers;
                            showConfirmThen( fmsg, faction );
                        }
                    } );
            }
        }

        @Override
        public void informMove( String expl, String words )
        {
            m_dlgBytes = expl;
            m_dlgTitle = R.string.info_title;
            m_words = null == words? null : wordsToArray( words );
            waitBlockingDialog( DLG_SCORES_BLK, 0 );
        }

        @Override
        public void informUndo()
        {
            nonBlockingDialog( DLG_OKONLY, getString( R.string.remote_undone ) );
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
                String oldSum = DictLangCache.getDictMD5Sum( BoardActivity.this,
                                                             oldName );
                if ( !oldSum.equals( newSum ) ) {
                    // Same dict, different versions
                    msg = getString( R.string.inform_dict_diffversionf,
                                     oldName );
                }
            } else {
                // Different dict!  If we have the other one, switch
                // to it.  Otherwise offer to download
                int dlgID;
                msg = getString( R.string.inform_dict_diffdictf,
                                 oldName, newName, newName );
                if ( DictLangCache.haveDict( BoardActivity.this, code, 
                                             newName ) ) {
                    dlgID = DLG_USEDICT;
                } else {
                    dlgID = DLG_GETDICT;
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
            m_jniThread.handle( JNICmd.CMD_POST_OVER );
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

            StringBuffer sb = new StringBuffer();
            for ( int ii = 0; ; ) {
                sb.append( words[ii] );
                if ( ++ii == words.length ) {
                    break;
                }
                sb.append( "; " );
            }
        
            String message = 
                getString( R.string.ids_badwords, sb.toString(), dict );

            if ( turnLost ) {
                nonBlockingDialog( DLG_BADWORDS, 
                                   message + getString(R.string.badwords_lost) );
            } else {
                m_dlgBytes = message + getString( R.string.badwords_accept );
                m_dlgTitle = R.string.query_title;
                accept = 0 != waitBlockingDialog( QUERY_REQUEST_BLK, 0 );
            }

            return accept;
        }

        // Let's have this block in case there are multiple messages.  If
        // we don't block the jni thread will continue processing messages
        // and may stack dialogs on top of this one.  Including later
        // chat-messages.
        @Override
        public void showChat( final String msg )
        {
            if ( GitVersion.CHAT_SUPPORTED ) {
                post( new Runnable() {
                        public void run() {
                            DBUtils.appendChatHistory( BoardActivity.this, 
                                                       m_rowid, msg, false );
                            startChatActivity();
                        }
                    } );
            }
        }
    } // class BoardUtilCtxt 

    private void loadGame()
    {
        if ( 0 == m_jniGamePtr ) {
            try {
                String[] dictNames = GameUtils.dictNames( this, m_rowid );
                DictUtils.DictPairs pairs = DictUtils.openDicts( this, dictNames );

                if ( pairs.anyMissing( dictNames ) ) {
                    showDictGoneFinish();
                } else {
                    Assert.assertNull( m_gameLock );
                    m_gameLock = new GameLock( m_rowid, true ).lock();

                    byte[] stream = GameUtils.savedGame( this, m_gameLock );
                    m_gi = new CurGameInfo( this );
                    XwJNI.gi_from_stream( m_gi, stream );
                    String langName = m_gi.langName();

                    setThis( this );

                    m_jniGamePtr = XwJNI.initJNI();

                    if ( m_gi.serverRole != DeviceRole.SERVER_STANDALONE ) {
                        m_xport = new CommsTransport( m_jniGamePtr, this, this, 
                                                      m_rowid, m_gi.serverRole );
                    }

                    CommonPrefs cp = CommonPrefs.get( this );
                    if ( null == stream ||
                         ! XwJNI.game_makeFromStream( m_jniGamePtr, stream, 
                                                      m_gi, dictNames, pairs.m_bytes, 
                                                      pairs.m_paths, langName, m_utils,
                                                      m_jniu, m_view, cp, m_xport ) ) {
                        XwJNI.game_makeNewGame( m_jniGamePtr, m_gi, m_utils, m_jniu, 
                                                m_view, cp, m_xport, dictNames, 
                                                pairs.m_bytes, pairs.m_paths,
                                                langName );
                    }

                    Handler handler = new Handler() {
                            public void handleMessage( Message msg ) {
                                switch( msg.what ) {
                                case JNIThread.DRAW:
                                    m_view.invalidate();
                                    break;
                                case JNIThread.DIALOG:
                                    m_dlgBytes = (String)msg.obj;
                                    m_dlgTitle = msg.arg1;
                                    showDialog( DLG_OKONLY );
                                    break;
                                case JNIThread.QUERY_ENDGAME:
                                    showDialog( QUERY_ENDGAME );
                                    break;
                                case JNIThread.TOOLBAR_STATES:
                                    if ( null != m_jniThread ) {
                                        m_gsi = 
                                            m_jniThread.getGameStateInfo();
                                        updateToolbar();
                                        if ( m_inTrade != m_gsi.inTrade ) {
                                            m_inTrade = m_gsi.inTrade;
                                            m_view.setInTrade( m_inTrade );
                                        }
                                        adjustTradeVisibility();
                                        Activity self = BoardActivity.this;
                                        Utils.invalidateOptionsMenuIf( self );
                                    }
                                    break;
                                case JNIThread.GOT_WORDS:
                                    launchLookup( wordsToArray((String)msg.obj), 
                                                  m_gi.dictLang );
                                    break;
                                case JNIThread.GAME_OVER:
                                    m_dlgBytes = (String)msg.obj;
                                    m_dlgTitle = msg.arg1;
                                    showDialog( GAME_OVER );
                                    break;
                                }
                            }
                        };
                    m_jniThread = 
                        new JNIThread( m_jniGamePtr, stream, m_gi, 
                                       m_view, m_gameLock, this, handler );
                    // see http://stackoverflow.com/questions/680180/where-to-stop-\
                    // destroy-threads-in-android-service-class
                    m_jniThread.setDaemon( true );
                    m_jniThread.start();

                    m_view.startHandling( this, m_jniThread, m_jniGamePtr, m_gi,
                                          m_connType );
                    if ( null != m_xport ) {
                        m_xport.setReceiver( m_jniThread, m_handler );
                    }
                    m_jniThread.handle( JNICmd.CMD_START );

                    if ( !CommonPrefs.getHideTitleBar( this ) ) {
                        setTitle( GameUtils.getName( this, m_rowid ) );
                    }
                    m_toolbar = new Toolbar( this, R.id.toolbar_horizontal );

                    populateToolbar();
                    adjustTradeVisibility();

                    int flags = DBUtils.getMsgFlags( this, m_rowid );
                    if ( 0 != (GameSummary.MSG_FLAGS_CHAT & flags) ) {
                        startChatActivity();
                    }
                    if ( m_overNotShown ) {
                        boolean auto = false;
                        if ( 0 != (GameSummary.MSG_FLAGS_GAMEOVER & flags) ) {
                            m_gameOver = true;
                        } else if ( DBUtils.gameOver( this, m_rowid ) ) {
                            m_gameOver = true;
                            auto = true;
                        }
                        if ( m_gameOver ) {
                            m_overNotShown = false;
                            m_jniThread.handle( JNICmd.CMD_POST_OVER, auto );
                        }
                    }
                    if ( 0 != flags ) {
                        DBUtils.setMsgFlags( m_rowid, GameSummary.MSG_FLAGS_NONE );
                    }

                    if ( null != m_xport ) {
                        warnIfNoTransport();
                        trySendChats();
                        Utils.cancelNotification( this, (int)m_rowid );
                        m_xport.tickle( m_connType );
                        tryInvites();
                    }
                }
           } catch ( GameUtils.NoSuchGameException nsge ) {
                DbgUtils.loge( nsge );
                finish();
            }
        }
    } // loadGame

    private void checkAndHandle( JNICmd cmd )
    {
        if ( null != m_jniThread ) {
            m_jniThread.handle( cmd );
        }
    }

    private void populateToolbar()
    {
        m_toolbar.setListener( Toolbar.BUTTON_BROWSE_DICT,
                               R.string.not_again_browseall,
                               R.string.key_na_browseall,
                               BUTTON_BROWSEALL_ACTION );
        m_toolbar.setLongClickListener( Toolbar.BUTTON_BROWSE_DICT,
                                        R.string.not_again_browse,
                                        R.string.key_na_browse,
                                        BUTTON_BROWSE_ACTION );
        m_toolbar.setListener( Toolbar.BUTTON_HINT_PREV, 
                               R.string.not_again_hintprev,
                               R.string.key_notagain_hintprev,
                               PREV_HINT_ACTION );
        m_toolbar.setListener( Toolbar.BUTTON_HINT_NEXT,
                               R.string.not_again_hintnext,
                               R.string.key_notagain_hintnext,
                               NEXT_HINT_ACTION );
        m_toolbar.setListener( Toolbar.BUTTON_JUGGLE,
                               R.string.not_again_juggle,
                               R.string.key_notagain_juggle,
                               JUGGLE_ACTION );
        m_toolbar.setListener( Toolbar.BUTTON_FLIP,
                               R.string.not_again_flip,
                               R.string.key_notagain_flip,
                               FLIP_ACTION );
        m_toolbar.setListener( Toolbar.BUTTON_ZOOM,
                               R.string.not_again_zoom,
                               R.string.key_notagain_zoom,
                               ZOOM_ACTION );
        m_toolbar.setListener( Toolbar.BUTTON_VALUES,
                               R.string.not_again_values,
                               R.string.key_na_values,
                               VALUES_ACTION );
        m_toolbar.setListener( Toolbar.BUTTON_UNDO,
                               R.string.not_again_undo,
                               R.string.key_notagain_undo,
                               UNDO_ACTION );
        if ( GitVersion.CHAT_SUPPORTED ) {
            m_toolbar.setListener( Toolbar.BUTTON_CHAT,
                                   R.string.not_again_chat, 
                                   R.string.key_notagain_chat,
                                   CHAT_ACTION );
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

    private int waitBlockingDialog( final int dlgID, int cancelResult )
    {
        int result = cancelResult;
        if ( m_blockingDlgPosted ) { // this has been true; dunno why
            DbgUtils.logf( "waitBlockingDialog: dropping dlgID %d", dlgID );
        } else {
            setBlockingThread();
            m_resultCode = cancelResult;

            if ( post( new Runnable() {
                    public void run() {
                        showDialog( dlgID );
                        m_blockingDlgPosted = true;
                    }
                } ) ) {

                try {
                    m_forResultWait.acquire();
                    m_blockingDlgPosted = false;
                } catch ( java.lang.InterruptedException ie ) {
                    DbgUtils.loge( ie );
                    if ( m_blockingDlgPosted ) {
                        dismissDialog( dlgID );
                        m_blockingDlgPosted = false;
                    }
                }
            }

            clearBlockingThread();
            result = m_resultCode;
        }
        return result;
    }

    private void nonBlockingDialog( final int dlgID, String txt ) 
    {
        switch ( dlgID ) {
        case DLG_OKONLY:
            m_dlgTitle = R.string.info_title;
            break;
        case DLG_BADWORDS:
            m_dlgTitle = R.string.badwords_title;
            break;
        case DLG_USEDICT:
        case DLG_GETDICT:
            m_dlgTitle = R.string.inform_dict_title;
            break;

        default:
            Assert.fail();
        }

        m_dlgBytes = txt;
        post( new Runnable() {
                public void run() {
                    showDialog( dlgID );
                }
            } );
    }

    private boolean doZoom( int zoomBy )
    {
        boolean handled = null != m_jniThread;
        if ( handled ) {
            m_jniThread.handle( JNICmd.CMD_ZOOM, zoomBy );
        }
        return handled;
    }

    private void startChatActivity()
    {
        if ( GitVersion.CHAT_SUPPORTED ) {
            Intent intent = new Intent( this, ChatActivity.class );
            intent.putExtra( GameUtils.INTENT_KEY_ROWID, m_rowid );
            startActivityForResult( intent, CHAT_REQUEST );
        }
    }

    private void waitCloseGame( boolean save ) 
    {
        if ( 0 != m_jniGamePtr ) {
            if ( null != m_xport ) {
                m_xport.waitToStop();
                m_xport = null;
            }

            interruptBlockingThread();

            if ( null != m_jniThread ) {
                m_jniThread.waitToStop( save );
                m_jniThread = null;
            }

            clearThis();

            XwJNI.game_dispose( m_jniGamePtr );
            m_jniGamePtr = 0;
            m_gi = null;

            m_gameLock.unlock();
            m_gameLock = null;
        }
    }

    private void warnIfNoTransport()
    {
        switch( m_connType ) {
        case COMMS_CONN_SMS:
            if ( XWApp.SMSSUPPORTED && !XWPrefs.getSMSEnabled( this ) ) {
                showConfirmThen( R.string.warn_sms_disabled, 
                                 R.string.newgame_enable_sms, 
                                 SMS_CONFIG_ACTION );
            }
            break;
        }
    }
    
    private void trySendChats()
    {
        if ( GitVersion.CHAT_SUPPORTED && null != m_jniThread ) {
            Iterator<String> iter = m_pendingChats.iterator();
            while ( iter.hasNext() ) {
                m_jniThread.handle( JNICmd.CMD_SENDCHAT, iter.next() );
            }
            m_pendingChats.clear();
        }
    }

    private void tryInvites()
    {
        if ( XWApp.BTSUPPORTED || XWApp.SMSSUPPORTED ) {
            if ( null != m_missingDevs ) {
                String gameName = GameUtils.getName( this, m_rowid );
                boolean doProgress = false;
                m_invitesPending = m_missingDevs.length;
                for ( String dev : m_missingDevs ) {
                    switch( m_connType ) {
                    case COMMS_CONN_BT:
                        BTService.inviteRemote( this, dev, m_gi.gameID, 
                                                gameName, m_gi.dictLang, 
                                                m_gi.nPlayers, 1 );
                        break;
                    case COMMS_CONN_SMS:
                        SMSService.inviteRemote( this, dev, m_gi.gameID, 
                                                 gameName, m_gi.dictLang, 
                                                 m_gi.dictName, m_gi.nPlayers,
                                                 1 );
                        break;
                    }
                }
                if ( doProgress ) {
                    startProgress( R.string.invite_progress );
                }
                m_missingDevs = null;
            }
        }
    }

    private void updateToolbar()
    {
        m_toolbar.update( Toolbar.BUTTON_FLIP, m_gsi.visTileCount >= 1 );
        m_toolbar.update( Toolbar.BUTTON_VALUES, m_gsi.visTileCount >= 1 );
        m_toolbar.update( Toolbar.BUTTON_JUGGLE, m_gsi.canShuffle );
        m_toolbar.update( Toolbar.BUTTON_UNDO, m_gsi.canRedo );
        m_toolbar.update( Toolbar.BUTTON_HINT_PREV, m_gsi.canHint );
        m_toolbar.update( Toolbar.BUTTON_HINT_NEXT, m_gsi.canHint );
        m_toolbar.update( Toolbar.BUTTON_CHAT, 
                          GitVersion.CHAT_SUPPORTED && m_gsi.canChat );
        m_toolbar.update( Toolbar.BUTTON_BROWSE_DICT, 
                          null != m_gi.dictName( m_view.getCurPlayer() ) );
    }

    private void hideShowItem( Menu menu, int id, boolean visible )
    {
        MenuItem item = menu.findItem( id );
        if ( null != item ) {
            item.setVisible( visible );
        }
    }

    private void updateMenus( Menu menu )
    {
        if ( null != m_gsi ) {
            hideShowItem( menu, R.id.board_menu_flip, m_gsi.visTileCount >= 1 );
            hideShowItem( menu, R.id.board_menu_toggle, m_gsi.visTileCount >= 1 );
            hideShowItem( menu, R.id.board_menu_juggle, m_gsi.canShuffle );
            hideShowItem( menu, R.id.board_menu_undo_current, m_gsi.canRedo );
            hideShowItem( menu, R.id.board_menu_hint_prev, m_gsi.canHint );
            hideShowItem( menu, R.id.board_menu_hint_next, m_gsi.canHint );
            hideShowItem( menu, R.id.board_menu_chat, 
                          GitVersion.CHAT_SUPPORTED && m_gsi.canChat );
        }
    }

    private void adjustTradeVisibility()
    {
        m_toolbar.setVisibility( m_inTrade? View.GONE : View.VISIBLE );
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
            int back = CommonPrefs.get( this )
                .otherColors[CommonPrefs.COLOR_BACKGRND];
            view.setBackgroundColor( back );
        }
    }

    private void setKeepScreenOn()
    {
        boolean keepOn = CommonPrefs.getKeepScreenOn( this );
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
            DbgUtils.logf( "post: dropping because handler null" );
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

    private void setupPasswdVars()
    {
        m_dlgTitleStr = getString( R.string.msg_ask_password, m_pwdName );
        m_passwdLyt = (LinearLayout)Utils.inflate( BoardActivity.this,
                                                   R.layout.passwd_view );
        m_passwdEdit = (EditText)m_passwdLyt.findViewById( R.id.edit );
    }

    private void doRematch()
    {
        Intent intent = GamesList.makeRematchIntent( this, m_gi, m_rowid );
        startActivity( intent );
        finish();
    }

} // class BoardActivity
