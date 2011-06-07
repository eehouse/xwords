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
import android.os.Bundle;
import android.view.View;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MenuInflater;
import android.view.KeyEvent;
import android.view.LayoutInflater;
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
import android.widget.ImageButton;
import android.widget.Toast;
import android.widget.EditText;
import android.widget.TextView;
import junit.framework.Assert;
import android.content.res.Configuration;
import android.content.pm.ActivityInfo;

import org.eehouse.android.xw4.jni.*;
import org.eehouse.android.xw4.jni.JNIThread.*;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;


public class BoardActivity extends XWActivity 
    implements TransportProcs.TPMsgHandler {

    public static final String INTENT_KEY_NAME = "name";
    public static final String INTENT_KEY_CHAT = "chat";

    private static final int DLG_OKONLY = DlgDelegate.DIALOG_LAST + 1;
    private static final int DLG_BADWORDS = DLG_OKONLY + 1;
    private static final int QUERY_REQUEST_BLK = DLG_OKONLY + 2;
    private static final int QUERY_INFORM_BLK = DLG_OKONLY + 3;
    private static final int PICK_TILE_REQUEST_BLK = DLG_OKONLY + 4;
    private static final int ASK_PASSWORD_BLK = DLG_OKONLY + 5;
    private static final int DLG_RETRY = DLG_OKONLY + 6;
    private static final int QUERY_ENDGAME = DLG_OKONLY + 7;
    private static final int DLG_DELETED = DLG_OKONLY + 8;

    private static final int CHAT_REQUEST = 1;

    private BoardView m_view;
    private int m_jniGamePtr;
    private GameUtils.GameLock m_gameLock;
    private CurGameInfo m_gi;
    CommsTransport m_xport;
    private Handler m_handler;
    private TimerRunnable[] m_timers;
    private String m_name;
    private Toolbar m_toolbar;
    private ArrayList<String> m_pendingChats = new ArrayList<String>();

    private String m_dlgBytes = null;
    private EditText m_passwdEdit = null;
    private int m_dlgTitle;
    private String m_dlgTitleStr;
    private String[] m_texts;
    private boolean m_firingPrefs;
    private JNIUtils m_jniu;
    private boolean m_volKeysZoom;
    private BoardUtilCtxt m_utils;

    // call startActivityForResult synchronously
	private Semaphore m_forResultWait = new Semaphore(0);
    private int m_resultCode;

    private Thread m_blockingThread;
    private JNIThread m_jniThread;
    private JNIThread.GameStateInfo m_gsi;

    private ProgressDialog m_progress;
    private boolean m_isVisible;

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
                m_jniThread.handle( JNICmd.CMD_TIMER_FIRED, false,
                                    m_why, m_when, m_handle );
            }
        }
    } 

    @Override
    protected Dialog onCreateDialog( int id )
    {
        Dialog dialog = super.onCreateDialog( id );
        if ( null == dialog ) {
            DialogInterface.OnClickListener lstnr;
            AlertDialog.Builder ab;

            switch ( id ) {
            case DLG_OKONLY:
            case DLG_BADWORDS:
            case DLG_RETRY:
                ab = new AlertDialog.Builder( BoardActivity.this )
                    //.setIcon( R.drawable.alert_dialog_icon )
                    .setTitle( m_dlgTitle )
                    .setMessage( m_dlgBytes )
                    .setPositiveButton( R.string.button_ok, null );
                if ( DLG_RETRY == id ) {
                    lstnr = new DialogInterface.OnClickListener() {
                            public void onClick( DialogInterface dlg, 
                                                 int whichButton ) {
                                m_jniThread.handle( JNIThread.JNICmd.CMD_RESET );
                            }
                        };
                    ab.setNegativeButton( R.string.button_retry, lstnr );
                }
                dialog = ab.create();
                setRemoveOnDismiss( dialog, id );
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
                                                  m_name, false );
                            // Intent intent = new Intent();
                            // intent.putExtra( "delete", true );
                            // intent.putExtra( "path", m_path );
                            // setResult( Activity.RESULT_OK, intent );
                            finish();
                        }
                    };
                ab.setNegativeButton( R.string.button_discard, lstnr );
                dialog = ab.create();
                break;

            case QUERY_REQUEST_BLK:
            case QUERY_INFORM_BLK:
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
                }

                dialog = ab.create();
                dialog.setOnDismissListener( makeODLforBlocking( id ) );
                break;

            case PICK_TILE_REQUEST_BLK:
                ab = new AlertDialog.Builder( this )
                    .setTitle( R.string.title_tile_picker );
                lstnr = new DialogInterface.OnClickListener() {
                        public void onClick( DialogInterface dialog, 
                                             int item ) {
                            m_resultCode = item;
                        }
                    };
                ab.setItems( m_texts, lstnr );

                dialog = ab.create();
                dialog.setOnDismissListener( makeODLforBlocking( id ) );
                break;

            case ASK_PASSWORD_BLK:
                m_passwdEdit.setText( "", TextView.BufferType.EDITABLE );
                ab = new AlertDialog.Builder( this )
                    .setTitle( m_dlgTitleStr )
                    .setView( m_passwdEdit )
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
                    .setNegativeButton( R.string.button_no,
                                        new DialogInterface.OnClickListener() {
                                            public void 
                                                onClick( DialogInterface dlg, 
                                                         int item ) {
                                                // do nothing
                                            }
                                        })
                    .create();
                break;

            default:
                // just drop it; super.onCreateDialog likely failed
                break;
            }
        }
        return dialog;
    } // onCreateDialog

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        Utils.logf( "BoardActivity::onCreate()" );
        super.onCreate( savedInstanceState );

        if ( CommonPrefs.getHideTitleBar( this ) ) {
            requestWindowFeature( Window.FEATURE_NO_TITLE );
        }

        m_utils = new BoardUtilCtxt();
        m_jniu = JNIUtilsImpl.get();
        setContentView( R.layout.board );
        m_handler = new Handler();
        m_timers = new TimerRunnable[4]; // needs to be in sync with
                                         // XWTimerReason
        m_gi = new CurGameInfo( this );

        m_view = (BoardView)findViewById( R.id.board_view );
        m_volKeysZoom = CommonPrefs.getVolKeysZoom( this );

        m_name = getIntent().getStringExtra( INTENT_KEY_NAME );

        setBackgroundColor();
    } // onCreate

    @Override
    protected void onPause()
    {
        waitCloseGame( true );
        super.onPause();
    }

    @Override
    protected void onResume()
    {
        super.onResume();
        loadGame();
    }

    @Override
    protected void onActivityResult( int requestCode, int resultCode, Intent data )
    {
        if ( Activity.RESULT_CANCELED != resultCode ) {
            if ( CHAT_REQUEST == requestCode ) {
                String msg = data.getStringExtra( INTENT_KEY_CHAT );
                if ( null != msg && msg.length() > 0 ) {
                    m_pendingChats.add( msg );
                    trySendChats();
                }
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
                m_volKeysZoom = CommonPrefs.getVolKeysZoom( this );
                if ( null != m_jniThread ) {
                    m_jniThread.handle( JNIThread.JNICmd.CMD_PREFS_CHANGE );
                }
                setBackgroundColor();
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
                m_jniThread.handle( JNIThread.JNICmd.CMD_KEYDOWN, xpKey );
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
                m_jniThread.handle( JNIThread.JNICmd.CMD_KEYUP, xpKey );
            }
        }
        return super.onKeyUp( keyCode, event );
    }

    @Override
    public boolean onCreateOptionsMenu( Menu menu ) 
    {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate( R.menu.board_menu, menu );

        // For now undo-last can crash the app or break a game in
        // networked case.  Disable until this is fixed.
        if ( m_gi.serverRole != DeviceRole.SERVER_STANDALONE ) {
            menu.removeItem( R.id.board_menu_undo_last );
        }
        return true;
    }

    @Override
    public boolean onPrepareOptionsMenu( Menu menu ) 
    {
        super.onPrepareOptionsMenu( menu );

        if ( null != m_gsi ) {
            boolean inTrade = m_gsi.inTrade;
            menu.setGroupVisible( R.id.group_intrade, inTrade ) ;
            menu.setGroupVisible( R.id.group_done, !inTrade );
        }

        return true;
    }

    public boolean onOptionsItemSelected( MenuItem item ) 
    {
        boolean handled = true;
        JNIThread.JNICmd cmd = JNIThread.JNICmd.CMD_NONE;
        Runnable proc = null;

        int id = item.getItemId();
        switch ( id ) {
        case R.id.board_menu_done:
            proc = new Runnable() {
                    public void run() {
                        m_jniThread.handle( JNIThread.JNICmd.CMD_COMMIT );
                    }
                };
            showNotAgainDlgThen( R.string.not_again_done, 
                                 R.string.key_notagain_done, proc );
            break;
        // case R.id.board_menu_juggle:
        //     cmd = JNIThread.JNICmd.CMD_JUGGLE;
        //     break;
        // case R.id.board_menu_flip:
        //     cmd = JNIThread.JNICmd.CMD_FLIP;
        //     break;
        case R.id.board_menu_trade:
            cmd = JNIThread.JNICmd.CMD_TRADE;
            break;
        case R.id.board_menu_trade_commit:
            cmd = JNIThread.JNICmd.CMD_COMMIT;
            break;
        case R.id.board_menu_trade_cancel:
            cmd = JNIThread.JNICmd.CMD_CANCELTRADE;
            break;

        case R.id.board_menu_tray:
            cmd = JNIThread.JNICmd.CMD_TOGGLE_TRAY;
            break;
        // case R.id.board_menu_undo_current:
        //     cmd = JNIThread.JNICmd.CMD_UNDO_CUR;
        //     break;
        case R.id.board_menu_undo_last:
            showConfirmThen( R.string.confirm_undo_last,
                             new DialogInterface.OnClickListener() {
                                 public void onClick( DialogInterface dlg, 
                                                      int whichButton ) {
                                     m_jniThread.handle( JNIThread.JNICmd.
                                                         CMD_UNDO_LAST );
                                 }
                             } );
            break;
        case R.id.board_menu_values:
            cmd = JNIThread.JNICmd.CMD_VALUES;
            break;

        case R.id.board_menu_game_counts:
            m_jniThread.handle( JNIThread.JNICmd.CMD_COUNTS_VALUES,
                                R.string.counts_values_title );
            break;
        case R.id.board_menu_game_left:
            m_jniThread.handle( JNIThread.JNICmd.CMD_REMAINING,
                                R.string.tiles_left_title );
            break;
        case R.id.board_menu_game_history:
            m_jniThread.handle( JNIThread.JNICmd.CMD_HISTORY,
                                R.string.history_title );
            break;

        case R.id.board_menu_game_final:
            m_jniThread.handle( JNIThread.JNICmd.CMD_FINAL,
                                R.string.history_title );
            break;

        case R.id.board_menu_game_resend:
            m_jniThread.handle( JNIThread.JNICmd.CMD_RESEND );
            break;

        case R.id.gamel_menu_checkmoves:
            doSyncMenuitem();
            break;

        case R.id.board_menu_file_prefs:
            m_firingPrefs = true;
            startActivity( new Intent( this, PrefsActivity.class ) );
            break;

        case R.id.board_menu_file_about:
            showAboutDialog();
            break;

        default:
            Utils.logf( "menuitem " + item.getItemId() + " not handled" );
            handled = false;
        }

        if ( handled && cmd != JNIThread.JNICmd.CMD_NONE ) {
            m_jniThread.handle( cmd );
        }
        return handled;
    }


    //////////////////////////////////////////////////
    // TransportProcs.TPMsgHandler interface
    //////////////////////////////////////////////////

    public void tpmRelayConnd( final String room, final int devOrder,
                               final boolean allHere, final int nMissing )
    {
        m_handler.post( new Runnable() {
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
            Toast.makeText( this, getString( strID ), 
                            Toast.LENGTH_SHORT).show();
        } else if ( dlgID >= 0 ) {
            final int strIDf = strID;
            final int dlgIDf = dlgID;
            m_handler.post( new Runnable() {
                    public void run() {
                        m_dlgBytes = getString( strIDf );
                        m_dlgTitle = R.string.relay_alert;
                        showDialog( dlgIDf );
                    }
                });
        }
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

    private void handleConndMessage( String room, int devOrder, boolean allHere, 
                                     int nMissing )
    {
        int naMsg = 0;
        int naKey = 0;
        String str = null;
        if ( allHere ) {
            // All players have now joined the game.  The device that
            // created the room will assign tiles.  Then it will be
            // the first player's turn
            String fmt = getString( R.string.msg_relay_all_heref );
            str = String.format( fmt, room );
            if ( devOrder > 1 ) {
                naMsg = R.string.not_again_conndall;
                naKey = R.string.key_notagain_conndall;
            }
        } else if ( nMissing > 0 ) {
            String fmt = getString( R.string.msg_relay_waiting );
            str = String.format( fmt, devOrder,
                                 room, nMissing );
            if ( devOrder == 1 ) {
                naMsg = R.string.not_again_conndfirst;
                naKey = R.string.key_notagain_conndfirst;
            } else {
                naMsg = R.string.not_again_conndmid;
                naKey = R.string.key_notagain_conndmid;
            }
        }

        if ( null != str ) {
            final String fstr = str;
            Runnable proc = new Runnable() {
                    public void run() {
                        Toast.makeText( BoardActivity.this, fstr,
                                        Toast.LENGTH_SHORT).show();
                    }
                };
            if ( naMsg == 0 ) {
                proc.run();
            } else {
                showNotAgainDlgThen( naMsg, naKey, proc );
            }
        }
    } // handleConndMessage

    private class BoardUtilCtxt extends UtilCtxtImpl {

        public BoardUtilCtxt()
        {
            super( BoardActivity.this );
        }

        public void requestTime() 
        {
            m_handler.post( new Runnable() {
                    public void run() {
                        if ( null != m_jniThread ) {
                            m_jniThread.handle( JNIThread.JNICmd.CMD_DO, false );
                        }
                    }
                } );
        }

        public void remSelected() 
        {
            m_jniThread.handle( JNIThread.JNICmd.CMD_REMAINING,
                                R.string.tiles_left_title );
        }

        public void setIsServer( boolean isServer )
        {
            Utils.logf( "setIsServer(%s)", isServer?"true":"false" );
            DeviceRole newRole = isServer? DeviceRole.SERVER_ISSERVER
                : DeviceRole.SERVER_ISCLIENT;
            if ( newRole != m_gi.serverRole ) {
                Utils.logf( "new role: %s; old role: %s", 
                            newRole.toString(), m_gi.serverRole.toString() );
                m_gi.serverRole = newRole;
                if ( !isServer ) {
                    m_jniThread.handle( JNIThread.JNICmd.CMD_SWITCHCLIENT );
                }
            }
        }

        public void setTimer( int why, int when, int handle )
        {
            if ( null != m_timers[why] ) {
                m_handler.removeCallbacks( m_timers[why] );
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
            m_handler.postDelayed( m_timers[why], inHowLong );
        }

        public void clearTimer( int why ) 
        {
            Utils.logf( "clearTimer called" );
            if ( null != m_timers[why] ) {
                m_handler.removeCallbacks( m_timers[why] );
                m_timers[why] = null;
            }
        }
        // This is supposed to be called from the jni thread
        public int userPickTile( int playerNum, String[] texts )
        {
            m_texts = texts;
            waitBlockingDialog( PICK_TILE_REQUEST_BLK, 0 );
            return m_resultCode;
        }

        public String askPassword( String name )
        {
            String fmt = getString( R.string.msg_ask_password );
            m_dlgTitleStr = String.format( fmt, name );

            if ( null == m_passwdEdit ) {
                LayoutInflater factory = LayoutInflater.from( BoardActivity.this );
                m_passwdEdit = (EditText)factory.inflate( R.layout.passwd_view, null );
            }
            waitBlockingDialog( ASK_PASSWORD_BLK, 0 );

            String result = null;      // means cancelled
            if ( 0 != m_resultCode ) {
                result = m_passwdEdit.getText().toString();
            }
            return result;
        }

        public void turnChanged()
        {
            m_handler.post( new Runnable() {
                    public void run() {
                        showNotAgainDlgThen( R.string.not_again_turnchanged, 
                                             R.string.key_notagain_turnchanged,
                                             null );
                    }
                } );
            m_jniThread.handle( JNIThread.JNICmd. CMD_ZOOM, -8 );
        }

        public boolean engineProgressCallback()
        {
            return ! m_jniThread.busy();
        }

        public void engineStarting( int nBlanks )
        {
            if ( nBlanks > 0 ) {
                m_handler.post( new Runnable() {
                        // Need to keep this from running after activity dies!!
                        public void run() {
                            if ( m_isVisible ) {
                                String title = 
                                    getString( R.string.progress_title );
                                m_progress = 
                                    ProgressDialog.show( BoardActivity.this,
                                                         title, null, true, 
                                                         true );
                            }
                        }
                    } );
            }
        }

        public void engineStopping()
        {
            m_handler.post( new Runnable() {
                    public void run() {
                        if ( null != m_progress ) {
                            m_progress.cancel();
                            m_progress = null;
                        }
                    }
                } );
        }

        public boolean userQuery( int id, String query )
        {
            boolean result;

            switch( id ) {
                // Though robot-move dialogs don't normally need to block,
                // if the player after this one is also a robot and we
                // don't block then a second dialog will replace this one.
                // So block.  Yuck.
            case UtilCtxt.QUERY_ROBOT_MOVE:
            case UtilCtxt.QUERY_ROBOT_TRADE:
                m_dlgBytes = query;
                m_dlgTitle = R.string.info_title;
                waitBlockingDialog( QUERY_INFORM_BLK, 0 );
                result = true;
                break;

                // These *are* blocking dialogs
            case UtilCtxt.QUERY_COMMIT_TRADE:
            case UtilCtxt.QUERY_COMMIT_TURN:
                if ( UtilCtxt.QUERY_COMMIT_TRADE == id ) {
                    m_dlgBytes = getString( R.string.query_trade );
                } else {
                    m_dlgBytes = query;
                }
                m_dlgTitle = R.string.query_title;
                result = 0 != waitBlockingDialog( QUERY_REQUEST_BLK, 0 );
                break;
            default:
                Assert.fail();
                result = false;
            }

            return result;
        }

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
            case UtilCtxt.ERR_CANT_TRADE_MID_MOVE:
                resid = R.string.str_cant_trade_mid_move;
                break;
            case UtilCtxt.ERR_NO_EMPTY_TRADE:
                resid = R.string.str_no_empty_trade;
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

        public void notifyGameOver()
        {
            m_jniThread.handle( JNIThread.JNICmd.CMD_POST_OVER );
        }

        // public void yOffsetChange( int maxOffset, int oldOffset, int newOffset )
        // {
        //     Utils.logf( "yOffsetChange(maxOffset=%d)", maxOffset );
        //     m_view.setVerticalScrollBarEnabled( maxOffset > 0 );
        // }

        public boolean warnIllegalWord( String[] words, int turn, boolean turnLost )
        {
            Utils.logf( "warnIllegalWord" );
            boolean accept = turnLost;

            StringBuffer sb = new StringBuffer();
            for ( int ii = 0; ; ) {
                sb.append( words[ii] );
                if ( ++ii == words.length ) {
                    break;
                }
                sb.append( "; " );
            }
        
            String format = getString( R.string.ids_badwords );
            String message = String.format( format, sb.toString() );

            if ( turnLost ) {
                nonBlockingDialog( DLG_BADWORDS, 
                                   message + getString(R.string.badwords_lost) );
            } else {
                m_dlgBytes = message + getString( R.string.badwords_accept );
                m_dlgTitle = R.string.query_title;
                accept = 0 != waitBlockingDialog( QUERY_REQUEST_BLK, 0 );
            }

            Utils.logf( "warnIllegalWord=>" + accept );
            return accept;
        }

        // Let's have this block in case there are multiple messages.  If
        // we don't block the jni thread will continue processing messages
        // and may stack dialogs on top of this one.  Including later
        // chat-messages.
        public void showChat( final String msg )
        {
            m_handler.post( new Runnable() {
                    public void run() {
                        DBUtils.appendChatHistory( BoardActivity.this, 
                                                   m_name, msg, false );
                        startChatActivity();
                    }
                } );
        }
    } // class BoardUtilCtxt 

    private void loadGame()
    {
        if ( 0 == m_jniGamePtr ) {
            Assert.assertNull( m_gameLock );
            m_gameLock = new GameUtils.GameLock( m_name, true ).lock();

            byte[] stream = GameUtils.savedGame( this, m_gameLock );
            XwJNI.gi_from_stream( m_gi, stream );

            String[] dictNames = m_gi.dictNames();
            byte[][] dictBytes = GameUtils.openDicts( this, dictNames );
            String langName = m_gi.langName();
            m_jniGamePtr = XwJNI.initJNI();

            if ( m_gi.serverRole != DeviceRole.SERVER_STANDALONE ) {
                m_xport = new CommsTransport( m_jniGamePtr, this, this, 
                                              m_gi.serverRole );
            }

            CommonPrefs cp = CommonPrefs.get( this );
            if ( null == stream ||
                 ! XwJNI.game_makeFromStream( m_jniGamePtr, stream, 
                                              m_gi, dictBytes, dictNames, 
                                              langName, m_utils, m_jniu, 
                                              m_view, cp, m_xport ) ) {
                XwJNI.game_makeNewGame( m_jniGamePtr, m_gi, m_utils, m_jniu, 
                                        m_view, cp, m_xport, 
                                        dictBytes, dictNames, langName );
            }

            m_jniThread = new 
                JNIThread( m_jniGamePtr, m_gi, m_view, m_gameLock, this,
                           new Handler() {
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
                                           m_gsi = m_jniThread.getGameStateInfo();
                                           updateToolbar();
                                       }
                                       break;
                                   }
                               }
                           } );
            // see http://stackoverflow.com/questions/680180/where-to-stop-\
            // destroy-threads-in-android-service-class
            m_jniThread.setDaemon( true );
            m_jniThread.start();

            m_view.startHandling( this, m_jniThread, m_jniGamePtr, m_gi );
            if ( null != m_xport ) {
                m_xport.setReceiver( m_jniThread );
            }
            m_jniThread.handle( JNICmd.CMD_START );

            if ( !CommonPrefs.getHideTitleBar( this ) ) {
                setTitle( GameUtils.gameName( this, m_name ) );
            }
            m_toolbar = new Toolbar( this );

            populateToolbar();

            int flags = DBUtils.getMsgFlags( m_name );
            if ( 0 != (GameSummary.MSG_FLAGS_CHAT & flags) ) {
                startChatActivity();
            }
            if ( 0 != (GameSummary.MSG_FLAGS_GAMEOVER & flags) ) {
                m_jniThread.handle( JNIThread.JNICmd.CMD_POST_OVER );
            }
            if ( 0 != flags ) {
                DBUtils.setMsgFlags( m_name, GameSummary.MSG_FLAGS_NONE );
            }

            trySendChats();
        }
    } // loadGame

    private void populateToolbar()
    {
        m_toolbar.setListener( Toolbar.BUTTON_HINT_PREV, 
                               R.string.not_again_hintprev,
                               R.string.key_notagain_hintprev,
                               new Runnable() {
                                   public void run() {
                                       m_jniThread.handle( JNIThread.JNICmd
                                                           .CMD_PREV_HINT );
                                   }
                               } );
        m_toolbar.setListener( Toolbar.BUTTON_HINT_NEXT,
                               R.string.not_again_hintnext,
                               R.string.key_notagain_hintnext,
                               new Runnable() {
                                   @Override
                                   public void run() {
                                       m_jniThread.handle( JNIThread.JNICmd
                                                           .CMD_NEXT_HINT );
                                   }
                               } );
        m_toolbar.setListener( Toolbar.BUTTON_JUGGLE,
                               R.string.not_again_juggle,
                               R.string.key_notagain_juggle,
                               new Runnable() {
                                   @Override
                                   public void run() {
                                       m_jniThread.handle( JNIThread.JNICmd
                                                           .CMD_JUGGLE );
                                   }
                               } );
        m_toolbar.setListener( Toolbar.BUTTON_FLIP,
                               R.string.not_again_flip,
                               R.string.key_notagain_flip,
                               new Runnable() {
                                   @Override
                                   public void run() {
                                       m_jniThread.handle( JNIThread.JNICmd
                                                           .CMD_FLIP );
                                   }
                               } );
        m_toolbar.setListener( Toolbar.BUTTON_ZOOM,
                               R.string.not_again_zoom,
                               R.string.key_notagain_zoom,
                               new Runnable() {
                                   @Override
                                   public void run() {
                                       m_jniThread.handle( JNIThread.JNICmd
                                                           .CMD_TOGGLEZOOM );
                                   }
                               } );
        m_toolbar.setListener( Toolbar.BUTTON_UNDO,
                               R.string.not_again_undo,
                               R.string.key_notagain_undo,
                               new Runnable() {
                                   @Override
                                   public void run() {
                                       m_jniThread.handle( JNIThread.JNICmd
                                                           .CMD_UNDO_CUR );
                                   }
                               });
        m_toolbar.setListener( Toolbar.BUTTON_CHAT,
                               R.string.not_again_chat, 
                               R.string.key_notagain_chat,
                               new Runnable() {
                                   @Override
                                   public void run() {
                                       startChatActivity();
                                   }
                               });
    } // populateToolbar

    private DialogInterface.OnDismissListener makeODLforBlocking( final int id )
    {
        return new DialogInterface.OnDismissListener() {
            public void onDismiss( DialogInterface di ) {
                releaseIfBlocking();
                removeDialog( id );
            }
        };
    }

    /* m_blockingDlgPosted: very rarely m_forResultWait will get
     * interrupted before the runnable gets run.  In that case there's
     * no dialog to dismiss.  So track it.
     */
    private boolean m_blockingDlgPosted = false;

    private int waitBlockingDialog( final int dlgID, int cancelResult )
    {
        Assert.assertFalse( m_blockingDlgPosted );
        setBlockingThread();

        m_handler.post( new Runnable() {
                public void run() {
                    showDialog( dlgID ); // crash
                    m_blockingDlgPosted = true;
                }
            } );

        try {
            m_forResultWait.acquire();
            m_blockingDlgPosted = false;
        } catch ( java.lang.InterruptedException ie ) {
            Utils.logf( "waitBlockingDialog: got " + ie.toString() );
            m_resultCode = cancelResult;
            if ( m_blockingDlgPosted ) {
                dismissDialog( dlgID );
                m_blockingDlgPosted = false;
            }
        }
        clearBlockingThread();
        return m_resultCode;
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
        default:
            Assert.fail();
        }

        m_dlgBytes = txt;
        m_handler.post( new Runnable() {
                public void run() {
                    showDialog( dlgID );
                }
            } );
    }

    private boolean doZoom( int zoomBy )
    {
        boolean handled = null != m_jniThread;
        if ( handled ) {
            m_jniThread.handle( JNIThread.JNICmd.CMD_ZOOM, zoomBy );
        }
        return handled;
    }

    private void startChatActivity()
    {
        Intent intent = new Intent( this, ChatActivity.class );
        intent.putExtra( BoardActivity.INTENT_KEY_NAME, m_name );
        startActivityForResult( intent, CHAT_REQUEST );
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
                if ( save ) {
                    m_jniThread.handle( JNIThread.JNICmd.CMD_SAVE );
                }
                m_jniThread.waitToStop();
                m_jniThread = null;
            }

            XwJNI.game_dispose( m_jniGamePtr );
            m_jniGamePtr = 0;

            m_gameLock.unlock();
            m_gameLock = null;
        }
    }

    private void trySendChats()
    {
        if ( null != m_jniThread ) {
            Iterator<String> iter = m_pendingChats.iterator();
            while ( iter.hasNext() ) {
                m_jniThread.handle( JNICmd.CMD_SENDCHAT, iter.next() );
            }
            m_pendingChats.clear();
        }
    }

    private void updateToolbar()
    {
        m_toolbar.update( Toolbar.BUTTON_FLIP, m_gsi.visTileCount > 1 );
        m_toolbar.update( Toolbar.BUTTON_JUGGLE, m_gsi.canShuffle );
        m_toolbar.update( Toolbar.BUTTON_UNDO, m_gsi.canRedo );
        m_toolbar.update( Toolbar.BUTTON_HINT_PREV, m_gsi.canHint );
        m_toolbar.update( Toolbar.BUTTON_HINT_NEXT, m_gsi.canHint );
        m_toolbar.update( Toolbar.BUTTON_CHAT, m_gsi.gameIsConnected );
    }

    private void setBackgroundColor()
    {
        int back = CommonPrefs.get(this)
            .otherColors[CommonPrefs.COLOR_BACKGRND];
        m_view.getRootView().setBackgroundColor( back );
    }

} // class BoardActivity
