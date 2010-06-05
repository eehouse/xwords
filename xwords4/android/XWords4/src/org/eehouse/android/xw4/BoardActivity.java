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
import android.view.Menu;
import android.view.MenuItem;
import android.view.MenuInflater;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.os.Handler;
import android.os.Message;
import android.content.Intent;
import java.util.concurrent.Semaphore;
import android.net.Uri;
import android.app.Dialog;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.widget.Toast;
import android.widget.EditText;
import android.widget.TextView;
import junit.framework.Assert;
import android.content.res.Configuration;
import android.content.pm.ActivityInfo;

import org.eehouse.android.xw4.jni.*;
import org.eehouse.android.xw4.jni.JNIThread.*;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;


public class BoardActivity extends Activity implements UtilCtxt {

    private static final int DLG_OKONLY = Utils.DIALOG_LAST + 1;
    private static final int DLG_BADWORDS = Utils.DIALOG_LAST + 2;
    private static final int QUERY_REQUEST_BLK = Utils.DIALOG_LAST + 3;
    private static final int QUERY_INFORM_BLK = Utils.DIALOG_LAST + 4;
    private static final int PICK_TILE_REQUEST_BLK = Utils.DIALOG_LAST + 5;
    private static final int QUERY_ENDGAME = Utils.DIALOG_LAST + 6;
    private static final int ASK_PASSWORD_BLK = Utils.DIALOG_LAST + 7;
    private static final int DLG_RETRY = Utils.DIALOG_LAST + 8;

    private BoardView m_view;
    private int m_jniGamePtr;
    private CurGameInfo m_gi;
    CommsTransport m_xport;
    private Handler m_handler;
    private TimerRunnable[] m_timers;
    private String m_path;
    private int m_currentOrient;

    private String m_dlgBytes = null;
    private EditText m_passwdEdit = null;
    private int m_dlgTitle;
    private String m_dlgTitleStr;
    private String[] m_texts;
    private boolean m_firingPrefs;
    private JNIUtils m_jniu;
    private boolean m_volKeysZoom;

    // call startActivityForResult synchronously
	private Semaphore m_forResultWait = new Semaphore(0);
    private int m_resultCode;

    private Thread m_blockingThread;
    private JNIThread m_jniThread;

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
                m_jniThread.handle( JNICmd.CMD_TIMER_FIRED,
                                    m_why, m_when, m_handle );
            }
        }
    } 

    @Override
    protected Dialog onCreateDialog( int id )
    {
        Dialog dialog = null;
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
            break;

        case QUERY_REQUEST_BLK:
        case QUERY_INFORM_BLK:
            ab = new AlertDialog.Builder( this )
                .setTitle( R.string.query_title )
                .setMessage( m_dlgBytes );
            lstnr = new DialogInterface.OnClickListener() {
                    public void onClick( DialogInterface dialog, 
                                         int whichButton ) {
                        m_resultCode = 1;
                    }
                };
            ab.setPositiveButton( R.string.button_yes, lstnr );
            lstnr = new DialogInterface.OnClickListener() {
                    public void onClick( DialogInterface dialog, 
                                         int whichButton ) {
                        m_resultCode = 0;
                    }
                };
            if ( QUERY_INFORM_BLK != id ) {
                ab.setNegativeButton( R.string.button_no, lstnr );
            }

            dialog = ab.create();
            dialog.setOnDismissListener( makeODLforBlocking() );
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
            dialog.setOnDismissListener( makeODLforBlocking() );
            break;

        case ASK_PASSWORD_BLK:
            ab = new AlertDialog.Builder( this )
                .setTitle( m_dlgTitleStr )
                .setView( m_passwdEdit )
                .setPositiveButton( R.string.button_ok,
                                    new DialogInterface.OnClickListener() {
                                        public void onClick( DialogInterface dlg,
                                                             int whichButton ) {
                                            m_resultCode = 1;
                                        }
                                    });
            dialog = ab.create();
            dialog.setOnDismissListener( makeODLforBlocking() );
            break;

        case QUERY_ENDGAME:
            dialog = new AlertDialog.Builder( this )
                .setTitle( R.string.query_title )
                .setMessage( R.string.ids_endnow )
                .setPositiveButton( R.string.button_yes,
                                    new DialogInterface.OnClickListener() {
                                        public void onClick( DialogInterface dlg, 
                                                             int item ) {
                                            m_jniThread.handle(JNICmd.CMD_ENDGAME);
                                        }
                                    })
                .setNegativeButton( R.string.button_no,
                                    new DialogInterface.OnClickListener() {
                                        public void onClick( DialogInterface dlg, 
                                                             int item ) {
                                            // do nothing
                                        }
                                    })
                .create();
            break;
        default:
            dialog = Utils.onCreateDialog( this, id );
            Assert.assertTrue( null != dialog );
        }
        return dialog;
    } // onCreateDialog

    @Override
    protected void onPrepareDialog( int id, Dialog dialog )
    {
        Utils.logf( "onPrepareDialog(id=" + id + ")" );
        switch( id ) {
        case DLG_OKONLY:
            dialog.setTitle( m_dlgTitle );
            // FALLTHRU
        case DLG_BADWORDS:
        case QUERY_REQUEST_BLK:
        case QUERY_INFORM_BLK:
            ((AlertDialog)dialog).setMessage( m_dlgBytes );
            break;
        case ASK_PASSWORD_BLK:
            m_passwdEdit.setText( "", TextView.BufferType.EDITABLE );
            dialog.setTitle( m_dlgTitleStr );
            break;
        }
        super.onPrepareDialog( id, dialog );
    }

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        Utils.logf( "BoardActivity::onCreate()" );
        super.onCreate( savedInstanceState );

        m_jniu = JNIUtilsImpl.get();
        setContentView( R.layout.board );
        m_handler = new Handler();
        m_timers = new TimerRunnable[4]; // needs to be in sync with
                                         // XWTimerReason
        m_gi = new CurGameInfo( this );

        m_view = (BoardView)findViewById( R.id.board_view );
        m_volKeysZoom = CommonPrefs.getVolKeysZoom( this );
        m_view.setUseZoomControl( !m_volKeysZoom );

        Intent intent = getIntent();
        Uri uri = intent.getData();
        m_path = uri.getPath();
        if ( m_path.charAt(0) == '/' ) {
            m_path = m_path.substring( 1 );
        }

    } // onCreate

    @Override
    protected void onStart()
    {
        Utils.logf( "BoardActivity::onStart" );
        loadGame();
        super.onStart();
    }

    @Override
    protected void onRestart()
    {
        Utils.logf( "BoardActivity::onRestart" );
        super.onRestart();
    }

    @Override
    protected void onPause()
    {
        Utils.logf( "BoardActivity::onPause()" );
        if ( null != m_jniThread ) {
            m_jniThread.setInBackground( true );
        }
        super.onPause();
    }

    @Override
    protected void onResume()
    {
        Utils.logf( "BoardActivity::onResume()" );
        if ( null != m_jniThread ) {
            m_jniThread.setInBackground( false );
        }
        super.onResume();
    }

    @Override
    protected void onStop()
    {
        Utils.logf( "BoardActivity::onStop()" );
        super.onStop();
    }

    @Override
    protected void onDestroy()
    {
        Utils.logf( "BoardActivity::onDestroy()" );
        if ( 0 != m_jniGamePtr ) {
            if ( null != m_xport ) {
                m_xport.waitToStop();
                m_xport = null;
            }

            interruptBlockingThread();

            if ( null != m_jniThread ) {
                // one last command
                m_jniThread.handle( JNIThread.JNICmd.CMD_SAVE );
                m_jniThread.waitToStop();
                m_jniThread = null;
            }

            XwJNI.game_dispose( m_jniGamePtr );
            m_jniGamePtr = 0;
        }
        super.onDestroy();
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
            }
            m_view.setUseZoomControl( !m_volKeysZoom );
        }
    }

    @Override
    public void onConfigurationChanged( Configuration newConfig )
    {
        m_currentOrient = newConfig.orientation;
        super.onConfigurationChanged( newConfig );
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
                            ? -1 : 1;
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

    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate( R.menu.board_menu, menu );
        return true;
    }

    public boolean onOptionsItemSelected( MenuItem item ) 
    {
        boolean handled = true;
        JNIThread.JNICmd cmd = JNIThread.JNICmd.CMD_NONE;

        int id = item.getItemId();
        switch ( id ) {
        case R.id.board_menu_done:
            cmd = JNIThread.JNICmd.CMD_COMMIT;
            break;
        case R.id.board_menu_juggle:
            cmd = JNIThread.JNICmd.CMD_JUGGLE;
            break;
        case R.id.board_menu_flip:
            cmd = JNIThread.JNICmd.CMD_FLIP;
            break;
        case R.id.board_menu_trade:
            cmd = JNIThread.JNICmd.CMD_TOGGLE_TRADE;
            break;
        case R.id.board_menu_tray:
            cmd = JNIThread.JNICmd.CMD_TOGGLE_TRAY;
            break;
        case R.id.board_menu_undo_current:
            cmd = JNIThread.JNICmd.CMD_UNDO_CUR;
            break;
        case R.id.board_menu_undo_last:
            cmd = JNIThread.JNICmd.CMD_UNDO_LAST;
            break;
        case R.id.board_menu_hint:
            cmd = JNIThread.JNICmd.CMD_HINT;
            break;
        case R.id.board_menu_hint_next:
            cmd = JNIThread.JNICmd.CMD_NEXT_HINT;
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
        case R.id.board_menu_file_prefs:
            m_firingPrefs = true;
            startActivity( new Intent( this, PrefsActivity.class ) );
            break;
        case R.id.board_menu_file_about:
            showDialog( Utils.DIALOG_ABOUT );
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

    //////////////////////////////////////////
    // XW_UtilCtxt interface implementation //
    //////////////////////////////////////////
    static final int[][] s_buttsBoard = { 
        { BONUS_TRIPLE_WORD,  BONUS_NONE,         BONUS_NONE,BONUS_DOUBLE_LETTER,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_TRIPLE_WORD },
        { BONUS_NONE,         BONUS_DOUBLE_WORD,  BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_TRIPLE_LETTER,BONUS_NONE,BONUS_NONE },

        { BONUS_NONE,         BONUS_NONE,         BONUS_DOUBLE_WORD,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_DOUBLE_LETTER,BONUS_NONE },
        { BONUS_DOUBLE_LETTER,BONUS_NONE,         BONUS_NONE,BONUS_DOUBLE_WORD,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_DOUBLE_LETTER },
                            
        { BONUS_NONE,         BONUS_NONE,         BONUS_NONE,BONUS_NONE,BONUS_DOUBLE_WORD,BONUS_NONE,BONUS_NONE,BONUS_NONE },
        { BONUS_NONE,         BONUS_TRIPLE_LETTER,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_TRIPLE_LETTER,BONUS_NONE,BONUS_NONE },
                            
        { BONUS_NONE,         BONUS_NONE,         BONUS_DOUBLE_LETTER,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_DOUBLE_LETTER,BONUS_NONE },
        { BONUS_TRIPLE_WORD,  BONUS_NONE,         BONUS_NONE,BONUS_DOUBLE_LETTER,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_DOUBLE_WORD },
    }; /* buttsBoard */

    public int getSquareBonus( int col, int row ) 
    {
        int half = m_gi.boardSize / 2;
        if ( col > half ) { col = (half*2) - col; }
        if ( row > half ) { row = (half*2) - row; }
        return s_buttsBoard[row][col];
    }

    public void requestTime() 
    {
        m_handler.post( new Runnable() {
                public void run() {
                    if ( null != m_jniThread ) {
                        m_jniThread.handle( JNIThread.JNICmd.CMD_DO );
                    }
                }
            } );
    }

    public void remSelected() 
    {
        m_jniThread.handle( JNIThread.JNICmd.CMD_REMAINING,
                            R.string.tiles_left_title );
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

    private void loadGame()
    {
        if ( 0 == m_jniGamePtr ) {
            byte[] stream = GameUtils.savedGame( this, m_path );
            XwJNI.gi_from_stream( m_gi, stream );

            Utils.logf( "loadGame: dict name: %s", m_gi.dictName );
            byte[] dictBytes = GameUtils.openDict( this, m_gi.dictName );
            if ( null == dictBytes ) {
                Assert.fail();
                finish();
            } else {
                m_jniGamePtr = XwJNI.initJNI();

                if ( m_gi.serverRole != DeviceRole.SERVER_STANDALONE ) {
                    Handler handler = new Handler() {
                            public void handleMessage( Message msg ) {
                                switch( msg.what ) {
                                case CommsTransport.DIALOG:
                                case CommsTransport.DIALOG_RETRY:
                                    m_dlgBytes = (String)msg.obj;
                                    m_dlgTitle = msg.arg1;
                                    showDialog( CommsTransport.DIALOG==msg.what
                                                ? DLG_OKONLY : DLG_RETRY );
                                    break;
                                case CommsTransport.TOAST:
                                    Toast.makeText( BoardActivity.this,
                                                    (CharSequence)(msg.obj),
                                                    Toast.LENGTH_SHORT).show();
                                    break;
                                }
                            }
                        };
                    m_xport = new CommsTransport( m_jniGamePtr, this, handler, 
                                                  m_gi.serverRole );
                }

                CommonPrefs cp = CommonPrefs.get( this );
                if ( null == stream ||
                     ! XwJNI.game_makeFromStream( m_jniGamePtr, stream, 
                                                  m_gi, dictBytes, 
                                                  m_gi.dictName,this, m_jniu, 
                                                  m_view, cp, m_xport ) ) {
                    XwJNI.game_makeNewGame( m_jniGamePtr, m_gi, this, m_jniu, 
                                            m_view, cp, m_xport, 
                                            dictBytes, m_gi.dictName );
                }

                m_jniThread = new 
                    JNIThread( m_jniGamePtr, m_gi, m_view, m_path, this,
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
                                       }
                                   }
                               } );
                m_jniThread.start();

                m_view.startHandling( m_jniThread, m_jniGamePtr, m_gi );
                if ( null != m_xport ) {
                    m_xport.setReceiver( m_jniThread );
                }
                m_jniThread.handle( JNICmd.CMD_START );

                setTitle( GameUtils.gameName( this, m_path ) );
            }
        }
    } // loadGame

    private DialogInterface.OnDismissListener makeODLforBlocking()
    {
        return new DialogInterface.OnDismissListener() {
            public void onDismiss( DialogInterface di ) {
                setRequestedOrientation( ActivityInfo.SCREEN_ORIENTATION_SENSOR );
                releaseIfBlocking();
            }
        };
    }

    private int waitBlockingDialog( final int dlgID, int cancelResult )
    {
        setBlockingThread();
        int orient = m_currentOrient == Configuration.ORIENTATION_LANDSCAPE
            ? ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
            : ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
        setRequestedOrientation( orient );

        m_handler.post( new Runnable() {
                public void run() {
                    showDialog( dlgID );
                }
            } );

        try {
            m_forResultWait.acquire();
        } catch ( java.lang.InterruptedException ie ) {
            m_resultCode = cancelResult;
            dismissDialog( dlgID );
            Utils.logf( "waitBlockingDialog: got " + ie.toString() );
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
            LayoutInflater factory = LayoutInflater.from( this );
            m_passwdEdit = (EditText)factory.inflate( R.layout.passwd_view, null );
        }
        waitBlockingDialog( ASK_PASSWORD_BLK, 0 );

        String result = null;      // means cancelled
        if ( 0 != m_resultCode ) {
            result = m_passwdEdit.getText().toString();
        }
        return result;
    }

    public boolean engineProgressCallback()
    {
        return ! m_jniThread.busy();
    }

    public String getUserString( int stringCode )
    {
        int id = 0;
        switch( stringCode ) {
        case UtilCtxt.STRD_ROBOT_TRADED:
            id = R.string.strd_robot_traded;
            break;
        case UtilCtxt.STR_ROBOT_MOVED:
            id = R.string.str_robot_moved;
            break;
        case UtilCtxt.STRS_VALUES_HEADER:
            id = R.string.strs_values_header;
            break;
        case UtilCtxt.STRD_REMAINING_TILES_ADD:
            id = R.string.strd_remaining_tiles_add;
            break;
        case UtilCtxt.STRD_UNUSED_TILES_SUB:
            id = R.string.strd_unused_tiles_sub;
            break;
        case UtilCtxt.STR_REMOTE_MOVED:
            id = R.string.str_remote_moved;
            break;
        case UtilCtxt.STRD_TIME_PENALTY_SUB:
            id = R.string.strd_time_penalty_sub;
            break;
        case UtilCtxt.STR_PASS:
            id = R.string.str_pass;
            break;
        case UtilCtxt.STRS_MOVE_ACROSS:
            id = R.string.strs_move_across;
            break;
        case UtilCtxt.STRS_MOVE_DOWN:
            id = R.string.strs_move_down;
            break;
        case UtilCtxt.STRS_TRAY_AT_START:
            id = R.string.strs_tray_at_start;
            break;
        case UtilCtxt.STRSS_TRADED_FOR:
            id = R.string.strss_traded_for;
            break;
        case UtilCtxt.STR_PHONY_REJECTED:
            id = R.string.str_phony_rejected;
            break;
        case UtilCtxt.STRD_CUMULATIVE_SCORE:
            id = R.string.strd_cumulative_score;
            break;
        case UtilCtxt.STRS_NEW_TILES:
            id = R.string.strs_new_tiles;
            break;
        case UtilCtxt.STR_PASSED:
            id = R.string.str_passed;
            break;
        case UtilCtxt.STRSD_SUMMARYSCORED:
            id = R.string.strsd_summaryscored;
            break;
        case UtilCtxt.STRD_TRADED:
            id = R.string.strd_traded;
            break;
        case UtilCtxt.STR_LOSTTURN:
            id = R.string.str_lostturn;
            break;
        case UtilCtxt.STR_COMMIT_CONFIRM:
            id = R.string.str_commit_confirm;
            break;
        case UtilCtxt.STR_LOCAL_NAME:
            id = R.string.str_local_name;
            break;
        case UtilCtxt.STR_NONLOCAL_NAME:
            id = R.string.str_nonlocal_name;
            break;
        case UtilCtxt.STR_BONUS_ALL:
            id = R.string.str_bonus_all;
            break;
        case UtilCtxt.STRD_TURN_SCORE:
            id = R.string.strd_turn_score;
            break;
        default:
            Utils.logf( "no such stringCode: " + stringCode );
        }

        String result;
        if ( 0 == id ) {
            result = "";
        } else {
            result = getString( id );
        }
        return result;
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
        case ERR_CANT_TRADE_MID_MOVE:
            resid = R.string.str_cant_trade_mid_move;
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
            accept = 0 != waitBlockingDialog( QUERY_REQUEST_BLK, 0 );
        }

        Utils.logf( "warnIllegalWord=>" + accept );
        return accept;
    }

} // class BoardActivity
