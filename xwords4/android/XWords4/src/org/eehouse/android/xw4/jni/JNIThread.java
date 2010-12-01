/* -*- compile-command: "cd ../../../../../../; ant install"; -*- */
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


package org.eehouse.android.xw4.jni;

import org.eehouse.android.xw4.Utils;
import android.content.Context;
import java.lang.InterruptedException;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.Iterator;
import android.os.Handler;
import android.os.Message;
import android.graphics.Paint;
import android.graphics.Rect;

import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.BoardDims;
import org.eehouse.android.xw4.GameUtils;
import org.eehouse.android.xw4.DBUtils;
import org.eehouse.android.xw4.Toolbar;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;

public class JNIThread extends Thread {

    public enum JNICmd { CMD_NONE,
            CMD_DRAW,
            CMD_LAYOUT,
            CMD_START,
            CMD_SWITCHCLIENT,
            CMD_RESET,
            CMD_SAVE,
            CMD_DO,
            CMD_RECEIVE,
            CMD_TRANSFAIL,
            CMD_PREFS_CHANGE,
            CMD_PEN_DOWN,
            CMD_PEN_MOVE,
            CMD_PEN_UP,
            CMD_KEYDOWN,
            CMD_KEYUP,
            CMD_TIMER_FIRED,
            CMD_COMMIT,
            CMD_JUGGLE,
            CMD_FLIP,
            CMD_TOGGLE_TRAY,
            CMD_TRADE,
            CMD_UNDO_CUR,
            CMD_UNDO_LAST,
            CMD_HINT,
            CMD_ZOOM,
            CMD_TOGGLEZOOM,
            CMD_PREV_HINT,
            CMD_NEXT_HINT,
            CMD_VALUES,
            CMD_COUNTS_VALUES,
            CMD_REMAINING,
            CMD_RESEND,
            CMD_HISTORY,
            CMD_FINAL,
            CMD_ENDGAME,
            CMD_POST_OVER,
            CMD_SENDCHAT,
            CMD_DRAW_CONNS_STATUS,
            };

    public static final int RUNNING = 1;
    public static final int DRAW = 2;
    public static final int DIALOG = 3;
    public static final int QUERY_ENDGAME = 4;
    public static final int TOOLBAR_STATES = 5;

    private boolean m_stopped = false;
    private int m_jniGamePtr;
    private String m_path;
    private Context m_context;
    private CurGameInfo m_gi;
    private Handler m_handler;
    private SyncedDraw m_drawer;
    private static final int kMinDivWidth = 10;
    private Rect m_connsIconRect;
    private int m_connsIconID = 0;
    private boolean m_inBack = false;

    LinkedBlockingQueue<QueueElem> m_queue;

    private class QueueElem {
        protected QueueElem( JNICmd cmd, boolean isUI, Object[] args )
        {
            m_cmd = cmd; m_isUIEvent = isUI; m_args = args;
        }
        boolean m_isUIEvent;
        JNICmd m_cmd;
        Object[] m_args;
    }

    public JNIThread( int gamePtr, CurGameInfo gi, SyncedDraw drawer, 
                      String path, Context context, Handler handler ) 
    {
        m_jniGamePtr = gamePtr;
        m_gi = gi;
        m_drawer = drawer;
        m_path = path;
        m_context = context;
        m_handler = handler;

        m_queue = new LinkedBlockingQueue<QueueElem>();
    }

    public void waitToStop() {
        m_stopped = true;
        handle( JNICmd.CMD_NONE );     // tickle it
        try {
            join(200);          // wait up to 2/10 second
        } catch ( java.lang.InterruptedException ie ) {
            Utils.logf( "got InterruptedException: " + ie.toString() );
        }
    }

    public boolean busy()
    {                           // synchronize this!!!
        boolean result = false;
        Iterator<QueueElem> iter = m_queue.iterator();
        while ( iter.hasNext() ) {
            if ( iter.next().m_isUIEvent ) {
                result = true;
                break;
            }
        }
        return result;
    }

    public void setInBackground( boolean inBack )
    {
        m_inBack = inBack;
        if ( inBack ) {
            handle( JNICmd.CMD_SAVE );
        }
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

    private void doLayout( BoardDims dims )
    {
        int scoreWidth = dims.width;

        if ( DeviceRole.SERVER_STANDALONE != m_gi.serverRole ) {
            scoreWidth -= dims.cellSize;
            m_connsIconRect = 
                new Rect( scoreWidth, 0, scoreWidth + dims.cellSize, 
                          Math.min( dims.scoreHt, dims.cellSize ) );
        }

        if ( m_gi.timerEnabled ) {
            scoreWidth -= dims.timerWidth;
            XwJNI.board_setTimerLoc( m_jniGamePtr, scoreWidth, 0, 
                                     dims.timerWidth, dims.scoreHt );
        } 
        XwJNI.board_setScoreboardLoc( m_jniGamePtr, 0, 0, scoreWidth, 
                                      dims.scoreHt, true );

        XwJNI.board_setPos( m_jniGamePtr, 0, dims.scoreHt, 
                            dims.width-1, dims.boardHt, dims.maxCellSize, 
                            false );

        XwJNI.board_setTrayLoc( m_jniGamePtr, 0, dims.trayTop,
                                dims.width-1, dims.trayHt, kMinDivWidth );

        XwJNI.board_invalAll( m_jniGamePtr );
    }

    private boolean nextSame( JNICmd cmd ) 
    {
        QueueElem nextElem = m_queue.peek();
        return null != nextElem && nextElem.m_cmd == cmd;
    }

    private boolean processKeyEvent( JNICmd cmd, XwJNI.XP_Key xpKey,
                                     boolean[] barr )
    {
        boolean draw = false;
        return draw;
    } // processKeyEvent

    private void checkButtons()
    {
        int visTileCount = XwJNI.board_visTileCount( m_jniGamePtr );
        int canFlip = visTileCount > 1 ? 1 : 0;
        Message.obtain( m_handler, TOOLBAR_STATES, Toolbar.BUTTON_FLIP,
                        canFlip ).sendToTarget();

        int canShuffle = XwJNI.board_canShuffle( m_jniGamePtr ) ? 1 : 0;
        Message.obtain( m_handler, TOOLBAR_STATES, Toolbar.BUTTON_JUGGLE,
                        canShuffle ).sendToTarget();

        int canRedo = XwJNI.board_canTogglePending( m_jniGamePtr ) ? 1 : 0;
        Message.obtain( m_handler, TOOLBAR_STATES, Toolbar.BUTTON_UNDO,
                        canRedo ).sendToTarget();

        int canHint = XwJNI.board_canHint( m_jniGamePtr ) ? 1 : 0;
        Message.obtain( m_handler, TOOLBAR_STATES, Toolbar.BUTTON_HINT_PREV,
                        canHint ).sendToTarget();
        Message.obtain( m_handler, TOOLBAR_STATES, Toolbar.BUTTON_HINT_NEXT,
                        canHint ).sendToTarget();

        int canMsg = XwJNI.comms_canChat( m_jniGamePtr ) ? 1 : 0;
        Message.obtain( m_handler, TOOLBAR_STATES, Toolbar.BUTTON_CHAT,
                        canMsg ).sendToTarget();
    }

    public void run() 
    {
        boolean[] barr = new boolean[2]; // scratch boolean
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

            case CMD_SAVE:
                if ( nextSame( JNICmd.CMD_SAVE ) ) {
                    continue;
                }
                GameSummary summary = new GameSummary( m_gi );
                XwJNI.game_summarize( m_jniGamePtr, summary );
                byte[] state = XwJNI.game_saveToStream( m_jniGamePtr, null );
                GameUtils.saveGame( m_context, state, m_path );
                DBUtils.saveSummary( m_context, m_path, summary );
                break;

            case CMD_DRAW:
                if ( nextSame( JNICmd.CMD_DRAW ) ) {
                    continue;
                }
                draw = true;
                break;

            case CMD_LAYOUT:
                doLayout( (BoardDims)args[0] );
                draw = true;
                // check and disable zoom button at limit
                handle( JNICmd.CMD_ZOOM, 0 );
                break;

            case CMD_RESET:
                XwJNI.comms_resetSame( m_jniGamePtr );
                // FALLTHRU
            case CMD_START:
                XwJNI.comms_start( m_jniGamePtr );
                if ( m_gi.serverRole == DeviceRole.SERVER_ISCLIENT ) {
                    XwJNI.server_initClientConnection( m_jniGamePtr );
                }
                draw = XwJNI.server_do( m_jniGamePtr );
                break;

            case CMD_SWITCHCLIENT:
                XwJNI.server_reset( m_jniGamePtr );
                XwJNI.server_initClientConnection( m_jniGamePtr );
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
                if ( m_inBack ) {
                    handle( JNICmd.CMD_SAVE );
                }
                break;

            case CMD_TRANSFAIL:
                XwJNI.comms_transportFailed( m_jniGamePtr );
                break;

            case CMD_PREFS_CHANGE:
                // need to inval all because some of prefs,
                // e.g. colors, aren't known by common code so
                // board_prefsChanged's return value isn't enough.
                XwJNI.board_invalAll( m_jniGamePtr );
                XwJNI.board_server_prefsChanged( m_jniGamePtr, 
                                                 CommonPrefs.get( m_context ) );
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

            case CMD_KEYDOWN:
            case CMD_KEYUP:
                draw = processKeyEvent( elem.m_cmd, (XwJNI.XP_Key)args[0], barr );
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
            case CMD_TRADE:
                draw = XwJNI.board_beginTrade( m_jniGamePtr );
                break;
            case CMD_UNDO_CUR:
                draw = XwJNI.board_replaceTiles( m_jniGamePtr )
                    || XwJNI.board_redoReplacedTiles( m_jniGamePtr );
                break;
            case CMD_UNDO_LAST:
                XwJNI.server_handleUndo( m_jniGamePtr );
                draw = true;
                break;

            case CMD_HINT:
                XwJNI.board_resetEngine( m_jniGamePtr );
                handle( JNICmd.CMD_NEXT_HINT );
                break;

            case CMD_NEXT_HINT:
            case CMD_PREV_HINT:
                if ( nextSame( elem.m_cmd ) ) {
                    continue;
                }
                draw = XwJNI.board_requestHint( m_jniGamePtr, false, 
                                                JNICmd.CMD_PREV_HINT==elem.m_cmd,
                                                barr );
                if ( barr[0] ) {
                    handle( elem.m_cmd );
                    draw = false;
                }
                break;

            case CMD_TOGGLEZOOM:
                XwJNI.board_zoom( m_jniGamePtr, 0 , barr );
                int zoomBy = 0;
                if ( barr[1] ) { // always go out if possible
                    zoomBy = -4;
                } else if ( barr[0] ) {
                    zoomBy = 4;
                }
                draw = XwJNI.board_zoom( m_jniGamePtr, zoomBy, barr );
                break;
            case CMD_ZOOM:
                draw = XwJNI.board_zoom( m_jniGamePtr, 
                                         ((Integer)args[0]).intValue(),
                                         barr );
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

            case CMD_RESEND:
                XwJNI.comms_resendAll( m_jniGamePtr );
                break;

            case CMD_HISTORY:
                boolean gameOver = XwJNI.server_getGameIsOver( m_jniGamePtr );
                sendForDialog( ((Integer)args[0]).intValue(),
                               XwJNI.model_writeGameHistory( m_jniGamePtr, 
                                                             gameOver ) );
                break;

            case CMD_FINAL:
                if ( XwJNI.server_getGameIsOver( m_jniGamePtr ) ) {
                    handle( JNICmd.CMD_POST_OVER );
                } else {
                    Message.obtain( m_handler, QUERY_ENDGAME ).sendToTarget();
                }
                break;

            case CMD_ENDGAME:
                XwJNI.server_endGame( m_jniGamePtr );
                draw = true;
                break;

            case CMD_POST_OVER:
                if ( XwJNI.server_getGameIsOver( m_jniGamePtr ) ) {
                    sendForDialog( R.string.finalscores_title,
                                   XwJNI.server_writeFinalScores( m_jniGamePtr ) );
                }
                break;

            case CMD_SENDCHAT:
                XwJNI.server_sendChat( m_jniGamePtr, (String)args[0] );
                break;

            case CMD_DRAW_CONNS_STATUS:
                int newID = 0;
                switch( (TransportProcs.CommsRelayState)(args[0]) ) {
                case COMMS_RELAYSTATE_UNCONNECTED:
                case COMMS_RELAYSTATE_DENIED:
                case COMMS_RELAYSTATE_CONNECT_PENDING:
                    newID = R.drawable.netarrow_unconn;
                    break;
                case COMMS_RELAYSTATE_CONNECTED: 
                case COMMS_RELAYSTATE_RECONNECTED: 
                    newID = R.drawable.netarrow_someconn;
                    break;
                case COMMS_RELAYSTATE_ALLCONNECTED:
                    newID = R.drawable.netarrow_allconn;
                    break;
                default:
                    newID = 0;
                }
                if ( m_connsIconID != newID ) {
                    draw = true;
                    m_connsIconID = newID;
                }
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
                if ( null != m_connsIconRect ) {
                    m_drawer.doIconDraw( m_connsIconID, m_connsIconRect );
                }

                // main UI thread has to invalidate view as it created
                // it.
                Message.obtain( m_handler, DRAW ).sendToTarget();

                checkButtons();
            }
        }
        Utils.logf( "run exiting" );
    } // run

    public void handle( JNICmd cmd, boolean isUI, Object... args )
    {
        QueueElem elem = new QueueElem( cmd, isUI, args );
        // Utils.logf( "adding: " + cmd.toString() );
        m_queue.add( elem );
    }

    public void handle( JNICmd cmd, Object... args )
    {
        handle( cmd, true, args );
    }

}
