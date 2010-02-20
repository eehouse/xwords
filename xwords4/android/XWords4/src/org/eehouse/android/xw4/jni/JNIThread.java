/* -*- compile-command: "cd ../../../../../../; ant install"; -*- */


package org.eehouse.android.xw4.jni;

import android.view.View;
import org.eehouse.android.xw4.Utils;
import java.lang.InterruptedException;
import java.util.concurrent.LinkedBlockingQueue;
import android.os.Handler;
import android.os.Message;
import android.graphics.Paint;
import android.graphics.Rect;

import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;

public class JNIThread extends Thread {

    public enum JNICmd { CMD_NONE,
            CMD_DRAW,
            CMD_LAYOUT,
            CMD_START,
            CMD_DO,
            CMD_RECEIVE,
            CMD_PREFS_CHANGE,
            CMD_PEN_DOWN,
            CMD_PEN_MOVE,
            CMD_PEN_UP,
            CMD_TIMER_FIRED,
            CMD_COMMIT,
            CMD_JUGGLE,
            CMD_FLIP,
            CMD_TOGGLE_TRAY,
            CMD_TOGGLE_TRADE,
            CMD_UNDO_CUR,
            CMD_UNDO_LAST,
            CMD_HINT,
            CMD_NEXT_HINT,
            CMD_VALUES,
            CMD_COUNTS_VALUES,
            CMD_REMAINING,
            CMD_HISTORY,
            CMD_POST_OVER,
            };

    public static final int RUNNING = 1;
    public static final int DRAW = 2;
    public static final int DIALOG = 3;

    private boolean m_stopped = false;
    private int m_jniGamePtr;
    private CurGameInfo m_gi;
    private Handler m_handler;
    private SyncedDraw m_drawer;

    LinkedBlockingQueue<QueueElem> m_queue;

    private class QueueElem {
        protected QueueElem( JNICmd cmd, Object[] args )
        {
            m_cmd = cmd; m_args = args;
        }
        JNICmd m_cmd;
        Object[] m_args;
    }

    public JNIThread( int gamePtr, CurGameInfo gi, SyncedDraw drawer, 
                      Handler handler ) 
    {
        m_jniGamePtr = gamePtr;
        m_gi = gi;
        m_handler = handler;
        m_drawer = drawer;

        m_queue = new LinkedBlockingQueue<QueueElem>();
    }

    public void waitToStop() {
        m_stopped = true;
        handle( JNICmd.CMD_NONE );     // tickle it
        try {
            join(100);          // wait up to 1/10 second
        } catch ( java.lang.InterruptedException ie ) {
            Utils.logf( "got InterruptedException: " + ie.toString() );
        }
    }

    public boolean busy()
    {                           // synchronize this!!!
        int siz = m_queue.size();
        return siz > 0;
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

    private void sendForDialog( int titleArg, String text )
    {
        Message.obtain( m_handler, DIALOG, titleArg, 0, text ).sendToTarget();
    }

    private void doLayout( int width, int height, int nCells )
    {
        int cellSize = width / nCells;

        // If we're vertical, we can likely fit all the board and
        // have a tall tray too.  If horizontal, let's assume
        // that's so things will be big, and rather than make 'em
        // small assume some scrolling.  So make the tray 1.5 to
        // 2.5x a cell width in height and then scroll however
        // many.

        int trayHt = cellSize * 3;
        int scoreHt = cellSize; // scoreboard ht same as cells for
        // proportion
        int wantHt = trayHt + scoreHt + (cellSize * nCells);
        int nToScroll = 0;
        if ( wantHt <= height ) {
            
        } else {
            // 
            nToScroll = nCells - ((height - (cellSize*3)) / cellSize);
            Utils.logf( "nToScroll: " + nToScroll );
            trayHt = height - (cellSize * (1 + (nCells-nToScroll)));
        }

        int scoreWidth = nCells * cellSize;
        if ( m_gi.timerEnabled ) {
            Paint paint = new Paint();
            paint.setTextSize( scoreHt );
            Rect rect = new Rect();
            paint.getTextBounds( "-00:00", 0, 6, rect );
            int timerWidth = rect.right;
            scoreWidth -= timerWidth;
            XwJNI.board_setTimerLoc( m_jniGamePtr, scoreWidth, 0, timerWidth, 
                                     scoreHt );
        } 
        XwJNI.board_setScoreboardLoc( m_jniGamePtr, 0, 0, scoreWidth, 
                                      scoreHt, true );

        XwJNI.board_setPos( m_jniGamePtr, 0, scoreHt, false );
        XwJNI.board_setScale( m_jniGamePtr, cellSize, cellSize );

        XwJNI.board_setTrayLoc( m_jniGamePtr, 0,
                                scoreHt + ((nCells-nToScroll) * cellSize),
                                nCells * cellSize, // width
                                trayHt,      // height
                                4 );

        XwJNI.board_invalAll( m_jniGamePtr );
    }

    private boolean nextSame( JNICmd cmd ) 
    {
        QueueElem nextElem = m_queue.peek();
        return null != nextElem && nextElem.m_cmd == cmd;
    }

    public void run() 
    {
        boolean[] barr = new boolean[1]; // scratch boolean
        while ( !m_stopped ) {
            QueueElem elem;
            Object[] args;
            try {
                elem = m_queue.take();
            } catch ( InterruptedException ie ) {
                Utils.logf( "interrupted; killing thread" );
                break;
            }
            boolean draw = false;
            args = elem.m_args;
            switch( elem.m_cmd ) {

            case CMD_DRAW:
                draw = true;
                break;

            case CMD_LAYOUT:
                doLayout( ((Integer)args[0]).intValue(),
                          ((Integer)args[1]).intValue(),
                          ((Integer)args[2]).intValue() );
                draw = true;
                break;

            case CMD_START:
                XwJNI.comms_start( m_jniGamePtr );
                if ( m_gi.serverRole == DeviceRole.SERVER_ISCLIENT ) {
                    XwJNI.server_initClientConnection( m_jniGamePtr );
                }
                /* FALLTHRU; works in java? */ 
                draw = XwJNI.server_do( m_jniGamePtr );
                break;
            case CMD_DO:
                if ( nextSame( JNICmd.CMD_DO ) ) {
                    continue;
                }
                draw = XwJNI.server_do( m_jniGamePtr );
                break;

            case CMD_RECEIVE:
                draw = XwJNI.game_receiveMessage( m_jniGamePtr, 
                                                  (byte[])args[0] );
                handle( JNICmd.CMD_DO );
                break;

            case CMD_PREFS_CHANGE:
                XwJNI.board_invalAll( m_jniGamePtr );
                XwJNI.board_prefsChanged( m_jniGamePtr, CommonPrefs.get() );
                draw = true;
                break;

            case CMD_PEN_DOWN:
                draw = XwJNI.board_handlePenDown( m_jniGamePtr, 
                                                  ((Integer)args[0]).intValue(),
                                                  ((Integer)args[1]).intValue(),
                                                  barr );
                break;
            case CMD_PEN_MOVE:
                if ( nextSame( JNICmd.CMD_PEN_MOVE ) ) {
                    continue;
                }
                draw = XwJNI.board_handlePenMove( m_jniGamePtr, 
                                                  ((Integer)args[0]).intValue(),
                                                  ((Integer)args[1]).intValue() );
                break;
            case CMD_PEN_UP:
                draw = XwJNI.board_handlePenUp( m_jniGamePtr, 
                                                ((Integer)args[0]).intValue(),
                                                ((Integer)args[1]).intValue() );
                break;

            case CMD_COMMIT:
                draw = XwJNI.board_commitTurn( m_jniGamePtr );
                break;

            case CMD_JUGGLE:
                draw = XwJNI.board_juggleTray( m_jniGamePtr );
                break;
            case CMD_FLIP:
                draw = XwJNI.board_flip( m_jniGamePtr );
                break;
            case CMD_TOGGLE_TRAY:
                draw = toggleTray();
                break;
            case CMD_TOGGLE_TRADE:
                draw = XwJNI.board_beginTrade( m_jniGamePtr );
                break;
            case CMD_UNDO_CUR:
                draw = XwJNI.board_replaceTiles( m_jniGamePtr );
                break;
            case CMD_UNDO_LAST:
                XwJNI.server_handleUndo( m_jniGamePtr );
                draw = true;
                break;

            case CMD_HINT:
                XwJNI.board_resetEngine( m_jniGamePtr );
                // fallthru
            case CMD_NEXT_HINT:
                draw = XwJNI.board_requestHint( m_jniGamePtr, false, barr );
                if ( barr[0] ) {
                    handle( JNICmd.CMD_NEXT_HINT );
                }
                break;

            case CMD_VALUES:
                draw = XwJNI.board_toggle_showValues( m_jniGamePtr );
                break;

            case CMD_COUNTS_VALUES:
                sendForDialog( ((Integer)args[0]).intValue(),
                               XwJNI.server_formatDictCounts( m_jniGamePtr, 3 )
                               );
                break;
            case CMD_REMAINING:
                sendForDialog( ((Integer)args[0]).intValue(),
                               XwJNI.board_formatRemainingTiles( m_jniGamePtr )
                               );
                break;
            case CMD_HISTORY:
                boolean gameOver = XwJNI.server_getGameIsOver( m_jniGamePtr );
                sendForDialog( ((Integer)args[0]).intValue(),
                               XwJNI.model_writeGameHistory( m_jniGamePtr, 
                                                             gameOver )
                               );
                break;

            case CMD_POST_OVER:
                sendForDialog( ((Integer)args[0]).intValue(),
                               XwJNI.server_writeFinalScores( m_jniGamePtr ) );
                break;

            case CMD_TIMER_FIRED:
                draw = XwJNI.timerFired( m_jniGamePtr, 
                                         ((Integer)args[0]).intValue(),
                                         ((Integer)args[1]).intValue(),
                                         ((Integer)args[2]).intValue() );
                break;
            }

            if ( draw ) {
                // do the drawing in this thread but in BoardView
                // where it can be synchronized with that class's use
                // of the same bitmap for blitting.
                m_drawer.doJNIDraw();

                // main UI thread has to invalidate view as it created
                // it.
                Message.obtain( m_handler, DRAW ).sendToTarget();
            }
        }
        Utils.logf( "run exiting" );
    } // run

    public void handle( JNICmd cmd, Object... args )
    {
        QueueElem elem = new QueueElem( cmd, args );
        // Utils.logf( "adding: " + cmd.toString() );
        m_queue.add( elem );
    }
}
