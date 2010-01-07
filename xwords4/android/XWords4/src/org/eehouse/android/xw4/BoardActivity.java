/* -*- compile-command: "cd ../../../../../; ant reinstall"; -*- */


package org.eehouse.android.xw4;

import android.app.Activity;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MenuInflater;
import android.content.res.AssetManager;
import java.io.InputStream;
import android.os.Handler;
import android.content.res.Configuration;

import org.eehouse.android.xw4.jni.*;

public class BoardActivity extends Activity implements XW_UtilCtxt, Runnable {

    private BoardView m_view;
    private int m_jniGamePtr;
    private CurGameInfo m_gi;
    private CommonPrefs m_prefs;
    private Handler m_handler;
    private TimerRunnable[] m_timers;

    public class TimerRunnable implements Runnable {
        private int m_gamePtr;
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
            XwJNI.timerFired( m_jniGamePtr, m_why, m_when, m_handle );
        }
    }

    protected void onCreate( Bundle savedInstanceState ) {
        super.onCreate( savedInstanceState );

        setContentView( R.layout.board );
        m_handler = new Handler();
        m_timers = new TimerRunnable[4]; // needs to be in sync with XWTimerReason

        m_prefs = new CommonPrefs();
        m_gi = new CurGameInfo();

        m_view = (BoardView)findViewById( R.id.board_view );

        byte[] dictBytes = null;
        InputStream dict = null;
        AssetManager am = getAssets();
        try {
            dict = am.open( m_gi.dictName, 
                            android.content.res.AssetManager.ACCESS_RANDOM );
            Utils.logf( "opened dict" );

            int len = dict.available();
            Utils.logf( "dict size: " + len );
            dictBytes = new byte[len];
            int nRead = dict.read( dictBytes, 0, len );
            if ( nRead != len ) {
                Utils.logf( "**** warning ****; read only " + nRead + " of " 
                            + len + " bytes." );
            }
        } catch ( java.io.IOException ee ){
            Utils.logf( "failed to open" );
        }
        // am.close(); don't close! won't work subsequently
        
        Utils.logf( "calling game_makeNewGame; passing bytes: " + dictBytes.length );
        m_jniGamePtr = XwJNI.game_makeNewGame( m_gi, this, m_view, 0, 
                                               m_prefs, null, dictBytes );
        m_view.startHandling( this, m_jniGamePtr, m_gi );

        XwJNI.server_do( m_jniGamePtr );
    }

    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate( R.menu.board_menu, menu );
        return true;
    }

    private boolean toggleTray() {
        boolean draw;
        int state = XwJNI.board_getTrayVisState( m_jniGamePtr );
        if ( state == XwJNI.TRAY_REVEALED ) {
            draw = XwJNI.board_hideTray( m_jniGamePtr );
        } else {
            draw = XwJNI.board_showTray( m_jniGamePtr );
        }
        return draw;
    }


    public boolean onOptionsItemSelected(MenuItem item) {
        boolean draw = false;
        boolean handled = true;

        switch (item.getItemId()) {
        case R.id.board_menu_done:
            draw = XwJNI.board_commitTurn( m_jniGamePtr );
            break;
        case R.id.board_menu_juggle:
            draw = XwJNI.board_juggleTray( m_jniGamePtr );
            break;
        case R.id.board_menu_flip:
            draw = XwJNI.board_flip( m_jniGamePtr );
            break;
        case R.id.board_menu_tray:
            draw = toggleTray();
            break;

        case R.id.board_menu_undo_current:
            draw = XwJNI.board_replaceTiles( m_jniGamePtr );
            break;
        case R.id.board_menu_undo_last:
            XwJNI.server_handleUndo( m_jniGamePtr );
            draw = true;
            break;

        case R.id.board_menu_hint:
            XwJNI.board_resetEngine( m_jniGamePtr );
            // fallthru
        case R.id.board_menu_hint_next:
            draw = XwJNI.board_requestHint( m_jniGamePtr, false, null );
            break;

        default:
            Utils.logf( "menuitem " + item.getItemId() + " not handled" );
            handled = false;
        }

        if ( draw ) {
            m_view.invalidate();
        }

        return handled;
    }

    // gets called for orientation changes only if
    // android:configChanges="orientation" set in AndroidManifest.xml

    // public void onConfigurationChanged( Configuration newConfig )
    // {
    //     super.onConfigurationChanged( newConfig );
    // }

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

    public void run() {
        if ( XwJNI.server_do( m_jniGamePtr ) ) {
            m_view.invalidate();
        }
    }

    public void requestTime() {
        m_handler.post( this );
    }

    public void setTimer( int why, int when, int handle )
    {
        if ( null != m_timers[why] ) {
            m_handler.removeCallbacks( m_timers[why] );
        }

        m_timers[why] = new TimerRunnable( why, when, handle );
        m_handler.postDelayed( m_timers[why], 500 );
    }

    public void clearTimer( int why ) 
    {
        Utils.logf( "clearTimer called" );
        if ( null != m_timers[why] ) {
            m_handler.removeCallbacks( m_timers[why] );
            m_timers[why] = null;
        }
    }

    // Don't need this unless we have a scroll thumb to indicate position
    // public void yOffsetChange( int oldOffset, int newOffset )
    // {
    //     Utils.logf( "yOffsetChange(" + oldOffset + "," + newOffset + ")" );
    // }
}
