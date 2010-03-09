/* -*- compile-command: "cd ../../../../../; ant install"; -*- */

package org.eehouse.android.xw4;

import android.app.Activity;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MenuInflater;
import android.view.KeyEvent;
import android.os.Handler;
import android.os.Message;
import android.content.Intent;
import java.util.concurrent.Semaphore;
import android.net.Uri;
import android.app.Dialog;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.widget.Toast;
import junit.framework.Assert;
import android.content.res.Configuration;
import android.content.pm.ActivityInfo;

import org.eehouse.android.xw4.jni.*;
import org.eehouse.android.xw4.jni.JNIThread.*;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;


public class BoardActivity extends Activity implements UtilCtxt {

    private static final int DLG_OKONLY = 1;
    private static final int DLG_BADWORDS = 2;
    private static final int QUERY_REQUEST_BLK = 3;
    private static final int PICK_TILE_REQUEST_BLK = 4;
    private static final int QUERY_ENDGAME = 5;

    private BoardView m_view;
    private int m_jniGamePtr;
    private CurGameInfo m_gi;
    CommsTransport m_xport;
    private Handler m_handler;
    private TimerRunnable[] m_timers;
    private String m_path;
    private int m_currentOrient;

    private String m_dlgBytes = null;
    private int m_dlgTitle;
    private String[] m_texts;
    private CommonPrefs m_cp;
    private JNIUtils m_jniu;

    // call startActivityForResult synchronously
	private Semaphore m_forResultWait = new Semaphore(0);
    private int m_resultCode;

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
            m_jniThread.handle( JNICmd.CMD_TIMER_FIRED,
                                m_why, m_when, m_handle );
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
            dialog = new AlertDialog.Builder( BoardActivity.this )
                //.setIcon( R.drawable.alert_dialog_icon )
                .setTitle( m_dlgTitle )
                .setMessage( m_dlgBytes )
                .setPositiveButton( R.string.button_ok, 
                                    new DialogInterface.OnClickListener() {
                                        public void onClick( DialogInterface dlg, 
                                                             int whichButton ) {
                                            Utils.logf( "Ok clicked" );
                                        }
                                    })
                .create();
            break;

        case QUERY_REQUEST_BLK:
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
            ab.setNegativeButton( R.string.button_no, lstnr );

            dialog = ab.create();
            dialog.setOnDismissListener( makeODL() );
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
            dialog.setOnDismissListener( makeODL() );
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
        case DLG_BADWORDS:
        case QUERY_REQUEST_BLK:
            ((AlertDialog)dialog).setMessage( m_dlgBytes );
            break;
        }
        super.onPrepareDialog( id, dialog );
    }

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );

        m_cp = CommonPrefs.get();
        m_jniu = JNIUtilsImpl.get();

        setContentView( R.layout.board );
        m_handler = new Handler();
        m_timers = new TimerRunnable[4]; // needs to be in sync with
                                         // XWTimerReason
        m_gi = new CurGameInfo( this );

        m_view = (BoardView)findViewById( R.id.board_view );

        Intent intent = getIntent();
        Uri uri = intent.getData();
        m_path = uri.getPath();
        if ( m_path.charAt(0) == '/' ) {
            m_path = m_path.substring( 1 );
        }

        byte[] stream = Utils.savedGame( this, m_path );
        XwJNI.gi_from_stream( m_gi, stream );

        Utils.logf( "dict name: " + m_gi.dictName );
        byte[] dictBytes = Utils.openDict( this, m_gi.dictName );
        if ( null == dictBytes ) {
            Utils.logf( "**** unable to open dict; warn user! ****" );
            finish();
        } else {
            m_jniGamePtr = XwJNI.initJNI();

            if ( m_gi.serverRole != DeviceRole.SERVER_STANDALONE ) {
                Handler handler = new Handler() {
                        public void handleMessage( Message msg ) {
                            switch( msg.what ) {
                            case CommsTransport.DIALOG:
                                m_dlgBytes = (String)msg.obj;
                                m_dlgTitle = msg.arg1;
                                showDialog( DLG_OKONLY );
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

            if ( null == stream ||
                 ! XwJNI.game_makeFromStream( m_jniGamePtr, stream, 
                                              m_gi, dictBytes, this,
                                              m_jniu, m_view, m_cp, m_xport ) ) {
                XwJNI.game_makeNewGame( m_jniGamePtr, m_gi, this, m_jniu, 
                                        m_view, m_cp, m_xport, dictBytes );
            }

            m_jniThread = new 
                JNIThread( m_jniGamePtr, m_gi, m_view,
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
            m_jniThread.handle( JNICmd.CMD_START );
            if ( null != m_xport ) {
                m_xport.setReceiver( m_jniThread );
            }
        }
    } // onCreate

    @Override
    public void onWindowFocusChanged( boolean hasFocus )
    {
        super.onWindowFocusChanged( hasFocus );
        if ( hasFocus ) {
            if ( null == m_cp ) {
                m_view.prefsChanged();
                m_jniThread.handle( JNIThread.JNICmd.CMD_PREFS_CHANGE );
            }
            // onContentChanged();
        }
    }

    @Override
    public void  onConfigurationChanged( Configuration newConfig )
    {
        m_currentOrient = newConfig.orientation;
        super.onConfigurationChanged( newConfig );
    }

    @Override
    public boolean onKeyDown( int keyCode, KeyEvent event )
    {
        XwJNI.XP_Key xpKey = keyCodeToXPKey( keyCode );
        if ( XwJNI.XP_Key.XP_KEY_NONE != xpKey ) {
            m_jniThread.handle( JNIThread.JNICmd.CMD_KEYDOWN, xpKey );
        }
        return super.onKeyDown( keyCode, event );
    }

    @Override
    public boolean onKeyUp( int keyCode, KeyEvent event )
    {
        XwJNI.XP_Key xpKey = keyCodeToXPKey( keyCode );
        if ( XwJNI.XP_Key.XP_KEY_NONE != xpKey ) {
            m_jniThread.handle( JNIThread.JNICmd.CMD_KEYUP, xpKey );
        }
        return super.onKeyUp( keyCode, event );
    }

    protected void onDestroy() 
    {
        byte[] state = XwJNI.game_saveToStream( m_jniGamePtr, null );
        Utils.saveGame( this, state, m_path );

        if ( null != m_xport ) {
            m_xport.waitToStop();
        }

        if ( null != m_jniThread ) {
            m_jniThread.waitToStop();
            Utils.logf( "onDestroy(): waitToStop() returned" );

            XwJNI.game_dispose( m_jniGamePtr );
            m_jniGamePtr = 0;
        }

        super.onDestroy();
        Utils.logf( "onDestroy done" );
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

        switch (item.getItemId()) {
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
            m_cp = null;        // mark so we'll reset it later
            startActivity( new Intent( this, PrefsActivity.class ) );
            break;
        case R.id.board_menu_file_about:
            Utils.about(this);
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

    //////////////////////////////////////////
    // XW_UtilCtxt interface implementation //
    //////////////////////////////////////////
    static final int[] s_buttsBoard = { 
        BONUS_TRIPLE_WORD,  BONUS_NONE,         BONUS_NONE,BONUS_DOUBLE_LETTER,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_TRIPLE_WORD,
        BONUS_NONE,         BONUS_DOUBLE_WORD,  BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_TRIPLE_LETTER,BONUS_NONE,BONUS_NONE,

        BONUS_NONE,         BONUS_NONE,         BONUS_DOUBLE_WORD,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_DOUBLE_LETTER,BONUS_NONE,
        BONUS_DOUBLE_LETTER,BONUS_NONE,         BONUS_NONE,BONUS_DOUBLE_WORD,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_DOUBLE_LETTER,
                            
        BONUS_NONE,         BONUS_NONE,         BONUS_NONE,BONUS_NONE,BONUS_DOUBLE_WORD,BONUS_NONE,BONUS_NONE,BONUS_NONE,
        BONUS_NONE,         BONUS_TRIPLE_LETTER,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_TRIPLE_LETTER,BONUS_NONE,BONUS_NONE,
                            
        BONUS_NONE,         BONUS_NONE,         BONUS_DOUBLE_LETTER,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_DOUBLE_LETTER,BONUS_NONE,
        BONUS_TRIPLE_WORD,  BONUS_NONE,         BONUS_NONE,BONUS_DOUBLE_LETTER,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_DOUBLE_WORD,
    }; /* buttsBoard */

    public int getSquareBonus( int col, int row ) {
        int bonus = BONUS_NONE;
        if ( col > 7 ) { col = 14 - col; }
        if ( row > 7 ) { row = 14 - row; }
        int index = (row*8) + col;
        if ( index < 8*8 ) {
            bonus = s_buttsBoard[index];
        }
        return bonus;
    }

    public void requestTime() 
    {
        m_handler.post( new Runnable() {
                public void run() {
                    m_jniThread.handle( JNIThread.JNICmd.CMD_DO );
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

    private DialogInterface.OnDismissListener makeODL()
    {
        return new DialogInterface.OnDismissListener() {
            public void onDismiss( DialogInterface di ) {
                setRequestedOrientation( ActivityInfo.SCREEN_ORIENTATION_SENSOR );
                m_forResultWait.release();
            }
        };
    }

    private int waitBlockingDialog( final int dlgID )
    {
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
            Utils.logf( "got " + ie.toString() );
        }

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

    // This is supposed to be called from the jni thread
    public int userPickTile( int playerNum, String[] texts )
    {
        m_texts = texts;
        waitBlockingDialog( PICK_TILE_REQUEST_BLK );
        return m_resultCode;
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
            // these two are not blocking; post showDialog and move on
        case UtilCtxt.QUERY_ROBOT_MOVE:
        case UtilCtxt.QUERY_ROBOT_TRADE:
            nonBlockingDialog( DLG_OKONLY, query );
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
            result = 0 != waitBlockingDialog( QUERY_REQUEST_BLK );
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
            accept = 0 != waitBlockingDialog( QUERY_REQUEST_BLK );
        }

        Utils.logf( "warnIllegalWord=>" + accept );
        return accept;
    }

} // class BoardActivity
