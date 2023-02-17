/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2019 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

import android.content.Context;
import android.graphics.Bitmap;
import android.os.Handler;
import android.os.Message;

import org.eehouse.android.xw4.Assert;
import org.eehouse.android.xw4.BuildConfig;
import org.eehouse.android.xw4.CommsTransport;
import org.eehouse.android.xw4.ConnStatusHandler;
import org.eehouse.android.xw4.DBUtils;
import org.eehouse.android.xw4.DbgUtils;
import org.eehouse.android.xw4.DictUtils;
import org.eehouse.android.xw4.DupeModeTimer;
import org.eehouse.android.xw4.GameLock;
import org.eehouse.android.xw4.GameUtils;
import org.eehouse.android.xw4.Log;
import org.eehouse.android.xw4.Quarantine;
import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.Utils;
import org.eehouse.android.xw4.XWPrefs;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;
import org.eehouse.android.xw4.jni.XwJNI.GamePtr;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.concurrent.LinkedBlockingQueue;

public class JNIThread extends Thread implements AutoCloseable {
    private static final String TAG = JNIThread.class.getSimpleName();

    public enum JNICmd { CMD_NONE,
            // CMD_RUN,
            CMD_DRAW,
            CMD_SETDRAW,
            CMD_INVALALL,
            CMD_LAYOUT,
            CMD_START,
            CMD_SAVE,
            CMD_DO,
            CMD_RECEIVE,
            // can I remove this? What if ordinal of enum's being saved somewhere
            _CMD_TRANSFAIL,
            CMD_PREFS_CHANGE,
            CMD_PEN_DOWN,
            CMD_PEN_MOVE,
            CMD_PEN_UP,
            CMD_KEYDOWN,
            CMD_KEYUP,
            CMD_TIMER_FIRED,
            CMD_COMMIT,
            CMD_TILES_PICKED,
            CMD_JUGGLE,
            CMD_FLIP,
            CMD_TOGGLE_TRAY,
            CMD_TRADE,
            CMD_CANCELTRADE,
            CMD_UNDO_CUR,
            CMD_UNDO_LAST,
            CMD_ZOOM,
            CMD_PREV_HINT,
            CMD_NEXT_HINT,
            CMD_COUNTS_VALUES,
            CMD_REMAINING,
            CMD_RESEND,
            // CMD_ACKANY,
            CMD_HISTORY,
            CMD_FINAL,
            CMD_ENDGAME,
            CMD_POST_OVER,
            CMD_SENDCHAT,
            CMD_NETSTATS,
            CMD_PASS_PASSWD,
            CMD_SET_BLANK,
            CMD_SETMQTTID,
            CMD_PAUSE,
            CMD_UNPAUSE,
            // CMD_DRAW_CONNS_STATUS,
            // CMD_DRAW_BT_STATUS,
            // CMD_DRAW_SMS_STATUS,
            };

    public static final int RUNNING = 1;
    public static final int DIALOG = 2;
    public static final int QUERY_ENDGAME = 3;
    public static final int TOOLBAR_STATES = 4;
    public static final int GOT_PAUSE = 5;
    public static final int GAME_OVER = 6;
    public static final int MSGS_SENT = 7;

    public class GameStateInfo implements Cloneable {
        public int visTileCount;
        public int trayVisState;
        public int nPendingMessages;
        public boolean canHint;
        public boolean canUndo;
        public boolean canRedo;
        public boolean inTrade;
        public boolean tradeTilesSelected;
        public boolean canChat;
        public boolean canShuffle;
        public boolean curTurnSelected;
        public boolean canHideRack;
        public boolean canTrade;
        public boolean canPause;
        public boolean canUnpause;
        public boolean canRematch;
        @Override
        public GameStateInfo clone() {
            GameStateInfo obj;
            try {
                obj = (GameStateInfo)super.clone();
            } catch ( CloneNotSupportedException cnse ) {
                obj = null;
            }
            return obj;
        }
    }

    private GameStateInfo m_gsi = new GameStateInfo();

    private boolean m_stopped = false;
    private boolean m_saveOnStop = false;
    private GamePtr m_jniGamePtr;
    private int m_lastSavedState = 0;
    private GameLock m_lock;
    private Context m_context;
    private CurGameInfo m_gi;
    private Handler m_handler;
    private SyncedDraw m_drawer;
    private String m_newDict = null;
    private long m_rowid;
    private int m_refCount;
    private CommsTransport m_xport;
    private GameSummary m_summary;

    private LinkedBlockingQueue<QueueElem> m_queue;

    private class QueueElem {
        protected QueueElem( JNICmd cmd, boolean isUI, Object[] args )
        {
            m_cmd = cmd; m_isUIEvent = isUI; m_args = args;
        }
        boolean m_isUIEvent;
        JNICmd m_cmd;
        Object[] m_args;
    }

    private JNIThread( GameLock lock )
    {
        m_lock = lock.retain();
        m_rowid = lock.getRowid();
        m_queue = new LinkedBlockingQueue<>();
    }

    public boolean configure( Context context, SyncedDraw drawer,
                              UtilCtxtImpl utils,
                              TransportProcs.TPMsgHandler xportHandler,
                              Handler handler )
    {
        m_context = context;
        m_drawer = drawer;
        m_handler = handler;

        // If this isn't true then the queue has to be allowed to empty,
        // working on the old game state, before we can re-use any of this.
        if ( 0 < m_queue.size() ) {
            if ( BuildConfig.DEBUG ) {
                Iterator<QueueElem> iter = m_queue.iterator();
                while ( iter.hasNext() ) {
                    Log.i( TAG, "removing %s from queue",
                           iter.next().m_cmd.toString() );
                }
            }
            m_queue.clear();
        }

        boolean success = false;
        DictUtils.DictPairs pairs = null;
        String[] dictNames = GameUtils.dictNames( context, m_lock );
        if ( null != dictNames ) {
            pairs = DictUtils.openDicts( context, dictNames );
            success = !pairs.anyMissing( dictNames );
        }

        if ( success ) {
            byte[] stream = GameUtils.savedGame( context, m_lock );
            Assert.assertNotNull( stream );
            m_gi = new CurGameInfo( context );
            m_gi.setName( DBUtils.getName( context, m_rowid ) );
            XwJNI.gi_from_stream( m_gi, stream );

            m_summary = DBUtils.getSummary( context, m_lock );

            if ( m_gi.serverRole != DeviceRole.SERVER_STANDALONE ) {
                m_xport = new CommsTransport( context, xportHandler, m_rowid,
                                              m_gi.serverRole );
            }

            CommonPrefs cp = CommonPrefs.get( context );

            // Assert.assertNull( m_jniGamePtr ); // fired!!
            if ( null != m_jniGamePtr ) {
                Log.d( TAG, "configure(): m_jniGamePtr not null; that ok?" );
                m_jniGamePtr.release();
            }

            synchronized ( this ) {
                m_jniGamePtr = null;
                if ( null != stream ) {
                    m_jniGamePtr = XwJNI.initFromStream( m_rowid, stream, m_gi,
                                                         utils, null, cp, m_xport );
                }
                if ( null == m_jniGamePtr ) {
                    Quarantine.markBad( m_rowid );
                    success = false;
                } else {
                    notifyAll();

                    m_lastSavedState = Arrays.hashCode( stream );
                    DupeModeTimer.gameOpened( m_context, m_rowid );
                }
            }
        }
        Log.d( TAG, "configure() => %b", success );
        return success;
    }

    public GamePtr getGamePtr() { return m_jniGamePtr; }
    public CurGameInfo getGI() { return m_gi; }
    public GameSummary getSummary() { return m_summary; }
    public GameLock getLock() { Assert.assertNotNull(m_lock); return m_lock; }

    private void waitToStop( boolean save )
    {
        synchronized ( this ) {
            m_stopped = true;
            m_saveOnStop = save;
        }
        handle( JNICmd.CMD_NONE );     // tickle it
        try {
            // Can't pass timeout to join.  There's no way to kill
            // this thread unless it's doing something interruptable
            // (like blocking on a socket) so might as well let it
            // take however log it takes.  If that's too long, fix it.
            interrupt();
            join();
            // Assert.assertFalse( isAlive() );
        } catch ( java.lang.InterruptedException ie ) {
            Log.ex( TAG, ie );
        }

        unlockOnce();
    }

    private synchronized void unlockOnce()
    {
        if ( null != m_lock ) {
            m_lock.release();
            m_lock = null;
        }
    }

    public boolean busy()
    {
        boolean result = false;

        // Docs: The returned iterator is a "weakly consistent" iterator that
        // will never throw ConcurrentModificationException, and guarantees to
        // traverse elements as they existed upon construction of the
        // iterator, and may (but is not guaranteed to) reflect any
        // modifications subsequent to construction.
        Iterator<QueueElem> iter = m_queue.iterator();
        while ( iter.hasNext() && !result ) {
            result = iter.next().m_isUIEvent;
        }

        return result;
    }

    public GameStateInfo getGameStateInfo()
    {
        synchronized( m_gsi ) {
            return m_gsi.clone();
        }
    }

    // Gross hack.  This is the easiest way to set the dict without
    // rewriting game loading code or running into cross-threading
    // issues.
    public void setSaveDict( String newDict )
    {
        m_newDict = newDict;
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

    private void doLayout( int width, int height, int fontWidth,
                           int fontHeight )
    {
        BoardDims dims = new BoardDims();

        boolean squareTiles = XWPrefs.getSquareTiles( m_context );
        XwJNI.board_figureLayout( m_jniGamePtr, m_gi, 0, 0, width, height,
                                  150 /*scorePct*/, 200 /*trayPct*/,
                                  width, fontWidth, fontHeight, squareTiles,
                                  dims /* out param */ );

        // Make space for net status icon if appropriate
        if ( m_gi.serverRole != DeviceRole.SERVER_STANDALONE ) {
            int statusWidth = dims.boardWidth / 15;
            dims.scoreWidth -= statusWidth;
            int left = dims.scoreLeft + dims.scoreWidth + dims.timerWidth;
            ConnStatusHandler.setRect( left, dims.top, left + statusWidth,
                                       dims.top + dims.scoreHt );
        } else {
            ConnStatusHandler.clearRect();
        }

        XwJNI.board_applyLayout( m_jniGamePtr, dims );

        m_drawer.dimsChanged( dims );
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
        synchronized( m_gsi ) {
            XwJNI.game_getState( m_jniGamePtr, m_gsi );
        }
        Message.obtain( m_handler, TOOLBAR_STATES ).sendToTarget();
    }

    private void save_jni()
    {
        // If server has any work to do, e.g. clean up after showing a
        // remote- or robot-moved dialog, let it do so before saving
        // state.  In some cases it'll otherwise drop the move.
        XwJNI.server_do( m_jniGamePtr );

        // And let it tell the relay (if any) it's leaving
        // XwJNI.comms_stop( m_jniGamePtr );

        XwJNI.game_getGi( m_jniGamePtr, m_gi );
        if ( null != m_newDict ) {
            m_gi.dictName = m_newDict;
        }
        byte[] state = XwJNI.game_saveToStream( m_jniGamePtr, m_gi );
        int newHash = Arrays.hashCode( state );
        boolean hashesEqual = m_lastSavedState == newHash;
        // PENDING: once certain this is true, stop saving the full array and
        // instead save the hash. Also, update it after each save.
        if ( hashesEqual ) {
            // Log.d( TAG, "save_jni(): no change in game; can skip saving" );
        } else {
            // Don't need this!!!! this only runs on the run() thread
            synchronized( this ) {
                Assert.assertNotNull( m_lock );
                GameSummary summary = new GameSummary( m_gi );
                XwJNI.game_summarize( m_jniGamePtr, summary );
                DBUtils.saveGame( m_context, m_lock, state, false );
                DBUtils.saveSummary( m_context, m_lock, summary );

                // There'd better be no way for saveGame above to fail!
                XwJNI.game_saveSucceeded( m_jniGamePtr );
                m_lastSavedState = newHash;

                Bitmap thumb
                    = GameUtils.takeSnapshot( m_context, m_jniGamePtr, m_gi );
                DBUtils.saveThumbnail( m_context, m_lock, thumb );
            }
        }
    }

    boolean m_running = false;
    public void startOnce()
    {
        synchronized ( this ) {
            if ( !m_running ) {
                m_running = true;
                start();
            }
        }
    }

    public void setDaemonOnce( boolean val )
    {
        if ( !m_running ) {
            setDaemon( val );
        }
    }

    @SuppressWarnings("fallthrough")
    @Override
    public void run()
    {
        Log.d( TAG, "run() starting" );
        boolean[] barr = new boolean[2]; // scratch boolean
        for ( ; ; ) {
            synchronized ( this ) {
                if ( m_stopped ) {
                    break;
                } else if ( null == m_jniGamePtr ) {
                    try {
                        wait();
                    } catch ( InterruptedException iex ) {
                        Log.d( TAG, "exiting run() on interrupt: %s",
                               iex.getMessage() );
                        break;
                    }
                    continue;
                }
            }

            final QueueElem elem;
            Object[] args;
            try {
                elem = m_queue.take();
            } catch ( InterruptedException ie ) {
                Log.w( TAG, "interrupted; killing thread" );
                break;
            }
            boolean draw = false;
            args = elem.m_args;
            switch( elem.m_cmd ) {

            case CMD_SAVE:
                if ( nextSame( JNICmd.CMD_SAVE ) ) {
                    continue;
                }
                save_jni();
                break;

            case CMD_DRAW:
                if ( nextSame( JNICmd.CMD_DRAW ) ) {
                    continue;
                }
                draw = true;
                break;

            case CMD_SETDRAW:
                XwJNI.board_setDraw( m_jniGamePtr, (DrawCtx)args[0] );
                XwJNI.board_invalAll( m_jniGamePtr );
                break;

            case CMD_INVALALL:
                XwJNI.board_invalAll( m_jniGamePtr );
                draw = true;
                break;

            case CMD_LAYOUT:
                Object args0 = args[0];
                BoardDims dims = null;
                if ( args0 instanceof BoardDims ) {
                    dims = (BoardDims)args0;
                    XwJNI.board_applyLayout( m_jniGamePtr, dims );
                } else {
                    doLayout( (Integer)args0, (Integer)args[1],
                              (Integer)args[2], (Integer)args[3] );
                }
                draw = true;
                // check and disable zoom button at limit
                handle( JNICmd.CMD_ZOOM, 0 );
                break;

            case CMD_START:
                draw = tryConnect( m_jniGamePtr, m_gi );
                break;

            case CMD_DO:
                if ( nextSame( JNICmd.CMD_DO ) ) {
                    continue;
                }
                draw = XwJNI.server_do( m_jniGamePtr );
                break;

            case CMD_RECEIVE:
                CommsAddrRec ret = (CommsAddrRec)args[1];
                Assert.assertNotNull( ret );
                draw = XwJNI.game_receiveMessage( m_jniGamePtr,
                                                  (byte[])args[0], ret );
                handle( JNICmd.CMD_DO );
                if ( draw ) {
                    handle( JNICmd.CMD_SAVE );
                }
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
                boolean phoniesConfirmed = args.length < 1
                    ? false : (Boolean)args[0];
                boolean turnConfirmed = args.length < 2
                    ? false : (Boolean)args[1];
                int[] newTiles = args.length < 3 ? null : (int[])args[2];
                draw = XwJNI.board_commitTurn( m_jniGamePtr, phoniesConfirmed,
                                               turnConfirmed, newTiles );
                break;

            case CMD_TILES_PICKED:
                int playerNum = (Integer)args[0];
                int[] tiles = (int[])args[1];
                XwJNI.server_tilesPicked( m_jniGamePtr, playerNum, tiles );
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
            case CMD_CANCELTRADE:
                draw = XwJNI.board_endTrade( m_jniGamePtr );
                break;
            case CMD_UNDO_CUR:
                draw = XwJNI.board_replaceTiles( m_jniGamePtr )
                    || XwJNI.board_redoReplacedTiles( m_jniGamePtr );
                break;
            case CMD_UNDO_LAST:
                XwJNI.server_handleUndo( m_jniGamePtr );
                draw = true;
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

            case CMD_ZOOM:
                draw = XwJNI.board_zoom( m_jniGamePtr,
                                         ((Integer)args[0]).intValue(),
                                         barr );
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
                int nSent =
                    XwJNI.comms_resendAll( m_jniGamePtr,
                                           ((Boolean)args[0]).booleanValue(),
                                           ((Boolean)args[1]).booleanValue() );
                if ( ((Boolean)args[2]).booleanValue() ) {
                    Message.obtain(m_handler, MSGS_SENT, nSent).sendToTarget();
                }
                break;
            // case CMD_ACKANY:
            //     XwJNI.comms_ackAny( m_jniGamePtr );
            //     break;

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
                    boolean auto = 0 < args.length &&
                        ((Boolean)args[0]).booleanValue();
                    int titleID = auto? R.string.summary_gameover
                        : R.string.finalscores_title;

                    String text = XwJNI.server_writeFinalScores( m_jniGamePtr );
                    Message.obtain( m_handler, GAME_OVER, titleID, 0, text )
                        .sendToTarget();
                }
                break;

            case CMD_SENDCHAT:
                XwJNI.board_sendChat( m_jniGamePtr, (String)args[0] );
                break;

            case CMD_NETSTATS:
                sendForDialog( ((Integer)args[0]).intValue(),
                               XwJNI.comms_getStats( m_jniGamePtr )
                               );
                break;

            case CMD_PASS_PASSWD:
                int player = ((Integer)args[0]).intValue();
                String pwd = (String)args[1];
                draw = XwJNI.board_passwordProvided( m_jniGamePtr, player, pwd );
                break;

            case CMD_SET_BLANK:
                draw = XwJNI.board_setBlankValue( m_jniGamePtr,
                                                  ((Integer)args[0]).intValue(),
                                                  ((Integer)args[1]).intValue(),
                                                  ((Integer)args[2]).intValue(),
                                                  ((Integer)args[3]).intValue() );
                break;

            case CMD_SETMQTTID:
                draw = false;
                XwJNI.comms_addMQTTDevID( m_jniGamePtr, (Integer)args[0], (String)args[1] );
                break;

            case CMD_TIMER_FIRED:
                draw = XwJNI.timerFired( m_jniGamePtr,
                                         ((Integer)args[0]).intValue(),
                                         ((Integer)args[1]).intValue(),
                                         ((Integer)args[2]).intValue() );
                break;

            case CMD_PAUSE:
                XwJNI.board_pause( m_jniGamePtr, ((String)args[0]) );
                break;
            case CMD_UNPAUSE:
                XwJNI.board_unpause( m_jniGamePtr, ((String)args[0]) );
                break;
            case CMD_NONE:      // ignored
                break;
            default:
                Log.w( TAG, "dropping cmd: %s", elem.m_cmd.toString() );
                Assert.failDbg();
            }

            if ( draw ) {
                // do the drawing in this thread but in BoardView
                // where it can be synchronized with that class's use
                // of the same bitmap for blitting.
                m_drawer.doJNIDraw();

                checkButtons();
            }
        } // for

        if ( null != m_jniGamePtr ) {
            if ( m_saveOnStop ) {
                XwJNI.comms_stop( m_jniGamePtr );
                save_jni();
            } else {
                Log.w( TAG, "run(): exiting without saving" );
            }
            m_jniGamePtr.release();
            m_jniGamePtr = null;
        }

        unlockOnce();
        Log.d( TAG, "run() finished" );
    } // run

    @Override
    public void finalize() throws java.lang.Throwable
    {
        Assert.assertTrue( null == m_lock || !BuildConfig.DEBUG );
        super.finalize();
    }

    public void handleBkgrnd( JNICmd cmd, Object... args )
    {
        // DbgUtils.logf( "adding: %s", cmd.toString() );
        m_queue.add( new QueueElem( cmd, false, args ) );
    }

    public JNIThread receive( byte[] msg, CommsAddrRec addr )
    {
        handle( JNICmd.CMD_RECEIVE, msg, addr );
        return this;
    }

    public void sendChat( String chat )
    {
        handle( JNICmd.CMD_SENDCHAT, chat );
    }

    public void notifyPause( int pauser, boolean isPause, String msg )
    {
        Message.obtain( m_handler, GOT_PAUSE, msg )
                        .sendToTarget();
    }

    public void handle( JNICmd cmd, Object... args )
    {
        if ( m_stopped && ! JNICmd.CMD_NONE.equals(cmd) ) {
            Log.w( TAG, "handle(%s): NOT adding to stopped thread!!!", cmd );
            // DbgUtils.printStack( TAG );
        } else {
            m_queue.add( new QueueElem( cmd, true, args ) );
        }
    }

    public static boolean tryConnect( GamePtr gamePtr, CurGameInfo gi )
    {
        Log.d( TAG, "tryConnect(rowid=%d)", gamePtr.getRowid() );
        XwJNI.comms_start( gamePtr );
        if ( gi.serverRole == DeviceRole.SERVER_ISCLIENT ) {
            XwJNI.server_initClientConnection( gamePtr );
        }
        return XwJNI.server_do( gamePtr );
    }

    private static Map<Long, JNIThread> s_instances = new HashMap<>();
    private void retain_sync()
    {
        ++m_refCount;
        // Log.i( TAG, "retain_sync(rowid=%d): m_refCount raised to %d",
        //        m_rowid, m_refCount );
    }

    public JNIThread retain()
    {
        synchronized( s_instances ) {
            retain_sync();
        }
        return this;
    }

    public void release() { release( true ); }

    public void release( boolean save )
    {
        boolean stop = false;
        synchronized( s_instances ) {
            if ( 0 == --m_refCount ) {
                s_instances.remove( m_rowid );
                stop = true;
            }
        }
        // Log.i( TAG, "release(rowid=%d): m_refCount dropped to %d",
        //        m_rowid, m_refCount );

        if ( stop ) {
            waitToStop( true );
            DupeModeTimer.gameClosed( m_context, m_rowid );
        } else if ( save && 0 != m_lastSavedState ) { // has configure() run?
            handle( JNICmd.CMD_SAVE );         // in case releaser has made changes
        }
    }

    @Override
    public void close()
    {
        release();
    }

    public static JNIThread getRetained( long rowid )
    {
        return getRetained( rowid, null );
    }

    public static JNIThread getRetained( GameLock lock )
    {
        return getRetained( lock.getRowid(), lock );
    }

    private static JNIThread getRetained( long rowid, GameLock lock )
    {
        JNIThread result = null;
        synchronized( s_instances ) {
            result = s_instances.get( rowid );
            if ( null == result && null != lock ) {
                result = new JNIThread( lock );
                s_instances.put( rowid, result );
            }
            if ( null != result ) {
                result.retain_sync();
            }
        }
        return result;
    }

    public static boolean gameIsOpen( long rowid )
    {
        boolean result = false;
        try ( JNIThread thread = JNIThread.getRetained( rowid ) ) {
            result = null != thread;
        }
        Log.d( TAG, "gameIsOpen(%d) => %b", rowid, result );
        return result;
    }
}
