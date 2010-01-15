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
import android.os.Message;
import java.io.FileOutputStream;
import java.io.FileInputStream;
import android.content.res.Configuration;
import android.content.Intent;
import java.util.concurrent.Semaphore;
import android.net.Uri;

import org.eehouse.android.xw4.jni.*;

public class BoardActivity extends Activity implements XW_UtilCtxt, Runnable {

    private static final int PICK_TILE_REQUEST = 1;

    private BoardView m_view;
    private int m_jniGamePtr;
    private CurGameInfo m_gi;
    private CommonPrefs m_prefs;
    private Handler m_handler;
    private TimerRunnable[] m_timers;
    private String m_path;

    // call startActivityForResult synchronously
	private Semaphore m_forResultWait = new Semaphore(0);
    private int m_resultCode = 0;
    private Intent m_resultIntent = null;

    private JNIThread m_jniThread;

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
            m_jniThread.handle( JNIThread.JNICmd.CMD_TIMER_FIRED,
                                new Object[] { m_why, m_when, m_handle } );
        }
    }

    protected void onCreate( Bundle savedInstanceState ) {
        super.onCreate( savedInstanceState );

        setContentView( R.layout.board );
        m_handler = new Handler();
        m_timers = new TimerRunnable[4]; // needs to be in sync with
                                         // XWTimerReason
        m_prefs = new CommonPrefs();
        m_gi = new CurGameInfo();

        m_view = (BoardView)findViewById( R.id.board_view );

        Intent intent = getIntent();
        Uri uri = intent.getData();
        m_path = uri.getPath();
        if ( m_path.charAt(0) == '/' ) {
            m_path = m_path.substring( 1 );
        }

        byte[] stream = savedGame();
        XwJNI.gi_from_stream( m_gi, stream );

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
        
        m_jniGamePtr = XwJNI.initJNI();

        if ( null == stream ||
             ! XwJNI.game_makeFromStream( m_jniGamePtr, stream, 
                                          m_gi, dictBytes, this,
                                          m_view, m_prefs,
                                          null ) ) {
            XwJNI.game_makeNewGame( m_jniGamePtr, m_gi, this, m_view, 0, 
                                    m_prefs, null, dictBytes );
        }

        m_jniThread = new JNIThread( m_jniGamePtr, 
                                     new Handler() {
                                             public void handleMessage( Message msg ) {
                                                 Utils.logf( "handleMessage called" );
                                                 m_view.invalidate();
                                             }
                                     } );
        m_jniThread.start();
        m_view.startHandling( m_jniThread, m_jniGamePtr, m_gi );

        m_jniThread.handle( JNIThread.JNICmd.CMD_DO );

        Utils.logf( "BoardActivity::onCreate() done" );
    } // onCreate

    // protected void onPause() {
    //     // save state here
    //     saveGame();
    //     super.onPause();
    // }

    protected void onDestroy() 
    {
        m_jniThread.waitToStop();
        saveGame();
        XwJNI.game_dispose( m_jniGamePtr );
        m_jniGamePtr = 0;
        super.onDestroy();
        Utils.logf( "onDestroy done" );
    }
	
    protected void onActivityResult( int requestCode, int resultCode, 
                                     Intent result ) 
    {
        Utils.logf( "onActivityResult called" );
		this.m_resultCode = resultCode;
		this.m_resultIntent = result;
		this.m_forResultWait.release();
	}
	
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate( R.menu.board_menu, menu );
        return true;
    }

    public boolean onOptionsItemSelected(MenuItem item) {
        boolean handled = true;

        switch (item.getItemId()) {
        case R.id.board_menu_done:
            m_jniThread.handle( JNIThread.JNICmd.CMD_COMMIT );
            break;
        case R.id.board_menu_juggle:
            m_jniThread.handle( JNIThread.JNICmd.CMD_JUGGLE );
            break;
        case R.id.board_menu_flip:
            m_jniThread.handle( JNIThread.JNICmd.CMD_FLIP );
            break;
        case R.id.board_menu_trade:
            m_jniThread.handle( JNIThread.JNICmd.CMD_TOGGLE_TRADE );
            break;
        case R.id.board_menu_tray:
            m_jniThread.handle( JNIThread.JNICmd.CMD_TOGGLE_TRAY );
            break;
        case R.id.board_menu_undo_current:
            m_jniThread.handle( JNIThread.JNICmd.CMD_UNDO_CUR );
            break;
        case R.id.board_menu_undo_last:
            m_jniThread.handle( JNIThread.JNICmd.CMD_UNDO_LAST );
            break;
        case R.id.board_menu_hint:
            m_jniThread.handle( JNIThread.JNICmd.CMD_HINT );
            break;
        case R.id.board_menu_hint_next:
            m_jniThread.handle( JNIThread.JNICmd.CMD_NEXT_HINT );
            break;
        case R.id.board_menu_values:
            m_jniThread.handle( JNIThread.JNICmd.CMD_VALUES );
            break;

        case R.id.board_menu_game_counts:
        case R.id.board_menu_game_left:
        case R.id.board_menu_game_info:
        case R.id.board_menu_game_history:
        case R.id.board_menu_game_final:
        case R.id.board_menu_game_resend:
        case R.id.board_menu_file_prefs:
            Utils.notImpl(this);
            break;
        case R.id.board_menu_file_about:
            Utils.about(this);
            break;

        default:
            Utils.logf( "menuitem " + item.getItemId() + " not handled" );
            handled = false;
        }

        return handled;
    }

    private void saveGame()
    {
        byte[] state = XwJNI.game_saveToStream( m_jniGamePtr, m_gi );

        try {
            FileOutputStream out = openFileOutput( m_path, MODE_PRIVATE );
            out.write( state );
            out.close();
        } catch ( java.io.IOException ex ) {
            Utils.logf( "got IOException: " + ex.toString() );
        }
    }

    private byte[] savedGame()
    {
        byte[] stream = null;
        try {
            FileInputStream in = openFileInput( m_path );
            int len = in.available();
            Utils.logf( "savedGame: got " + len + " bytes." );
            stream = new byte[len];
            in.read( stream, 0, len );
            in.close();
        } catch ( java.io.FileNotFoundException fnf ) {
            Utils.logf( fnf.toString() );
            stream = null;
        } catch ( java.io.IOException io ) {
            Utils.logf( io.toString() );
            stream = null;
        }
        return stream;
    } // savedGame

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
        m_jniThread.handle( JNIThread.JNICmd.CMD_DO );
    }

    public void requestTime() {
        m_handler.post( this );
    }

    public void remSelected() {
        // Send a message to the main thread or follow the docs to add
        // a looper inside JNIThread::run()
        Utils.logf( "remSelected() can't call notImpl() as hasn't "
                    + "called Looper.prepare()" );
        // Utils.notImpl( this );
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

    // This is supposed to be called from the jni thread
    public int userPickTile( int playerNum, String[] texts )
    {
        int tile = -1;
        Utils.logf( "util_userPickTile called" );

        // Intent intent = new Intent( XWConstants.ACTION_PICK_TILE );
        // intent.setClassName( "org.eehouse.android.xw4",
        //                      "org.eehouse.android.xw4.TilePicker");
        Intent intent = new Intent( BoardActivity.this, TilePicker.class );
        intent.setAction( XWConstants.ACTION_PICK_TILE );

        Bundle bundle = new Bundle();
        bundle.putStringArray( XWConstants.PICK_TILE_TILES, texts );
        intent.putExtra( XWConstants.PICK_TILE_TILES, bundle );

        try {
            startActivityForResult( intent, PICK_TILE_REQUEST );
            m_forResultWait.acquire();
        } catch ( Exception ee ) {
            Utils.logf( "userPickTile got: " + ee.toString() );
        }

        if ( m_resultCode >= RESULT_FIRST_USER ) {
            tile = m_resultCode - RESULT_FIRST_USER;
        } else {
            Utils.logf( "unexpected result code: " + m_resultCode );
        }

        Utils.logf( "util_userPickTile => " + tile );
        return tile;
    }

    public boolean engineProgressCallback()
    {
        return !m_jniThread.busy();
    }
} // class BoardActivity
