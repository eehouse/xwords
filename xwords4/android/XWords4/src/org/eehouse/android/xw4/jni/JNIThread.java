/* -*- compile-command: "cd ../../../../../../; ant reinstall"; -*- */


package org.eehouse.android.xw4.jni;

import android.view.View;
import org.eehouse.android.xw4.Utils;
import java.lang.InterruptedException;
import java.util.concurrent.LinkedBlockingQueue;
import android.os.Handler;
import android.os.Message;

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
            };

    private boolean m_stopped = false;
    private int m_jniGamePtr;
    private Handler m_handler;
    LinkedBlockingQueue<QueueElem> m_queue;

    private class QueueElem {
        protected QueueElem( JNICmd cmd, Object[] args )
        {
            m_cmd = cmd; m_args = args;
        }
        JNICmd m_cmd;
        Object[] m_args;
    }

    public JNIThread( int gamePtr, Handler handler ) {
        Utils.logf( "in JNIThread()" );
        m_jniGamePtr = gamePtr;
        m_handler = handler;

        m_queue = new LinkedBlockingQueue<QueueElem>();
    }

    public void waitToStop() {
        m_stopped = true;
        handle( JNICmd.CMD_NONE );     // tickle it
        try {
            join();
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
            case CMD_DO:
                draw = XwJNI.server_do( m_jniGamePtr );
                break;

            case CMD_PEN_DOWN:
                draw = XwJNI.board_handlePenDown( m_jniGamePtr, 
                                                  ((Integer)args[0]).intValue(),
                                                  ((Integer)args[1]).intValue(),
                                                  barr );
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
                draw = XwJNI.board_requestHint( m_jniGamePtr, false, barr );
                if ( barr[0] ) {
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
            }

            if ( draw ) {
                if ( !XwJNI.board_draw( m_jniGamePtr ) ) {
                    Utils.logf( "draw not complete" );
                }
                m_handler.post(null);
            }
        }
        Utils.logf( "run exiting" );
    } // run

    public void handle( JNICmd cmd, Object... args )
    {
        QueueElem elem = new QueueElem( cmd, args );
        m_queue.add( elem );
    }
}
