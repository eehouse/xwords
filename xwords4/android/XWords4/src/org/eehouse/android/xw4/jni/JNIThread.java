/* -*- compile-command: "cd ../../../../../../; ant reinstall"; -*- */


package org.eehouse.android.xw4.jni;

import android.view.View;
import org.eehouse.android.xw4.Utils;
import java.lang.InterruptedException;
import java.util.concurrent.LinkedBlockingQueue;
import android.os.Handler;
import android.os.Message;
import android.os.Looper;

public class JNIThread extends Thread {

    public enum JNICmd { CMD_NONE,
            CMD_DRAW,
            CMD_DO,
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

            CMD_STOP,
            };

    public static final int RUNNING = 1;
    public static final int DRAW = 2;

    private int m_jniGamePtr;
    private Handler m_parentHandler;
    private Handler m_loopHandler;
    private boolean[] m_barr = new boolean[1]; // scratch boolean
    
    public JNIThread( int gamePtr, Handler handler ) {
        Utils.logf( "in JNIThread()" );
        m_jniGamePtr = gamePtr;
        m_parentHandler = handler;
    }

    public void waitToStop() {
        handle( JNICmd.CMD_STOP );
        try {
            join();
        } catch ( java.lang.InterruptedException ie ) {
            Utils.logf( "got InterruptedException: " + ie.toString() );
        }
    }

    public boolean busy()
    {
        // HTF to I tell if my queue has anything in it.  Do I have to
        // keep a counter?  Which means synchronizing...
        return false;
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

    public void run() 
    {
        Looper.prepare();
          
        m_loopHandler = new Handler() {
                public void handleMessage( Message msg ) {
                    boolean draw = false;
                    Object[] args = (Object[])msg.obj;
                    switch( JNICmd.values()[msg.what] ) {

                    case CMD_DRAW:
                        draw = true;
                        break;
                    case CMD_DO:
                        draw = XwJNI.server_do( m_jniGamePtr );
                        break;

                    case CMD_PEN_DOWN:
                        draw = XwJNI.board_handlePenDown( m_jniGamePtr, 
                                                          ((Integer)args[0]).intValue(),
                                                          ((Integer)args[1]).intValue(),
                                                          m_barr );
                        break;
                    case CMD_PEN_MOVE:
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
                        draw = XwJNI.board_requestHint( m_jniGamePtr, false, m_barr );
                        if ( m_barr[0] ) {
                            handle( JNICmd.CMD_NEXT_HINT );
                        }
                        break;

                    case CMD_VALUES:
                        draw = XwJNI.board_toggle_showValues( m_jniGamePtr );
                        break;

                    case CMD_TIMER_FIRED:
                        draw = XwJNI.timerFired( m_jniGamePtr, 
                                                 ((Integer)args[0]).intValue(),
                                                 ((Integer)args[1]).intValue(),
                                                 ((Integer)args[2]).intValue() );
                        break;

                    case CMD_STOP:
                        Looper.myLooper().quit();
                        break;
                    }

                    if ( draw ) {
                        if ( !XwJNI.board_draw( m_jniGamePtr ) ) {
                            Utils.logf( "draw not complete" );
                        }
                        Message.obtain( m_parentHandler, DRAW ).sendToTarget();
                    }
                }
            };

        // Safe to use us now
        Message.obtain( m_parentHandler, RUNNING ).sendToTarget();
          
        Looper.loop();

        Utils.logf( "run exiting" );
    } // run


    /** Post a cmd to be handled by the JNI thread
     */
    public void handle( JNICmd cmd, Object... args )
    {
        Message message = Message.obtain( m_loopHandler, cmd.ordinal(), args );
        message.sendToTarget();
    }
}
