/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2018 by Eric House (xwords@eehouse.org).  All rights
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

import android.graphics.Rect;

import java.util.Arrays;

import org.eehouse.android.xw4.Assert;
import org.eehouse.android.xw4.BuildConfig;
import org.eehouse.android.xw4.Log;
import org.eehouse.android.xw4.NetLaunchInfo;
import org.eehouse.android.xw4.Quarantine;
import org.eehouse.android.xw4.Utils;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;

// Collection of native methods and a bit of state
public class XwJNI {
    private static final String TAG = XwJNI.class.getSimpleName();

    public static class GamePtr implements AutoCloseable {
        private long m_ptrGame = 0;
        private int m_refCount = 0;
        private long m_rowid;
        private String mStack;

        private GamePtr( long ptr, long rowid )
        {
            m_ptrGame = ptr;
            m_rowid = rowid;
            mStack = android.util.Log.getStackTraceString(new Exception());
            Quarantine.recordOpened( rowid );
        }

        public synchronized long ptr()
        {
            Assert.assertTrue( 0 != m_ptrGame );
            return m_ptrGame;
        }

        public synchronized GamePtr retain()
        {
            ++m_refCount;
            Log.d( TAG, "retain(this=%H, rowid=%d): refCount now %d",
                   this, m_rowid, m_refCount );
            return this;
        }

        public long getRowid() { return m_rowid; }

        // Force (via an assert in finalize() below) that this is called. It's
        // better if jni stuff isn't being done on the finalizer thread
        public synchronized void release()
        {
            --m_refCount;
            // Log.d( TAG, "%s.release(this=%H, rowid=%d): refCount now %d",
            //        getClass().getName(), this, m_rowid, m_refCount );
            if ( 0 == m_refCount ) {
                if ( 0 != m_ptrGame ) {
                    Quarantine.recordClosed( m_rowid );
                    if ( haveEnv( getJNI().m_ptrGlobals ) ) {
                        game_dispose( this ); // will crash if haveEnv fails
                    } else {
                        Log.d( TAG, "release(): no ENV!!! (this=%H, rowid=%d)",
                               this, m_rowid );
                        Assert.failDbg(); // seen on Play Store console
                    }
                    m_ptrGame = 0;
                }
            } else {
                Assert.assertTrue( m_refCount > 0 || !BuildConfig.DEBUG );
            }
        }

        @Override
        public void close()
        {
            release();
        }

        // @Override
        public void finalize() throws java.lang.Throwable
        {
            if ( BuildConfig.DEBUG && (0 != m_refCount || 0 != m_ptrGame) ) {
                Log.e( TAG, "finalize(): called prematurely: refCount: %d"
                       + "; ptr: %d; creator: %s", m_refCount, m_ptrGame, mStack );
            }
            super.finalize();
        }
    }

    private static XwJNI s_JNI = null;
    private static synchronized XwJNI getJNI()
    {
        if ( null == s_JNI ) {
            s_JNI = new XwJNI();
        }
        return s_JNI;
    }

    private long m_ptrGlobals;
    private XwJNI()
    {
        m_ptrGlobals = initGlobals( new DUtilCtxt(), JNIUtilsImpl.get() );
    }

    public static void cleanGlobalsEmu()
    {
        cleanGlobals();
    }

    private static void cleanGlobals()
    {
        synchronized( XwJNI.class ) { // let's be safe here
            XwJNI jni = getJNI();
            cleanGlobals( jni.m_ptrGlobals ); // tests for 0
            jni.m_ptrGlobals = 0;
        }
    }

    @Override
    public void finalize() throws java.lang.Throwable
    {
        cleanGlobals( m_ptrGlobals );
        super.finalize();
    }

    // This needs to be called before the first attempt to use the
    // jni.
    static {
        System.loadLibrary("xwjni");
    }

    /* XW_TrayVisState enum */
    public static final int TRAY_HIDDEN = 0;
    public static final int TRAY_REVERSED = 1;
    public static final int TRAY_REVEALED = 2;

    // Methods not part of the common interface but necessitated by
    // how java/jni work (or perhaps my limited understanding of it.)

    // callback into jni from java when timer set here fires.
    public static native boolean timerFired( GamePtr gamePtr, int why,
                                             int when, int handle );

    public static byte[] gi_to_stream( CurGameInfo gi )
    {
        return gi_to_stream( getJNI().m_ptrGlobals, gi );
    }

    public static void gi_from_stream( CurGameInfo gi, byte[] stream )
    {
        Assert.assertNotNull( stream );
        gi_from_stream( getJNI().m_ptrGlobals, gi, stream ); // called here
    }

    public static byte[] nliToStream( NetLaunchInfo nli )
    {
        nli.freezeAddrs();
        return nli_to_stream( getJNI().m_ptrGlobals, nli );
    }

    public static NetLaunchInfo nliFromStream( byte[] stream )
    {
        NetLaunchInfo nli = new NetLaunchInfo();
        nli_from_stream( getJNI().m_ptrGlobals, nli, stream );
        nli.unfreezeAddrs();
        return nli;
    }

    public static native void comms_getInitialAddr( CommsAddrRec addr,
                                                    String relayHost,
                                                    int relayPort );
    public static native String comms_getUUID();

    // Game methods
    private static GamePtr initGameJNI( long rowid )
    {
        int seed = Utils.nextRandomInt();
        long ptr = initGameJNI( getJNI().m_ptrGlobals, seed );
        GamePtr result = 0 == ptr ? null : new GamePtr( ptr, rowid );
        return result;
    }

    public static synchronized GamePtr
        initFromStream( long rowid, byte[] stream, CurGameInfo gi,
                        String[] dictNames, byte[][] dictBytes,
                        String[] dictPaths, String langName,
                        UtilCtxt util, DrawCtx draw,
                        CommonPrefs cp, TransportProcs procs )

    {
        GamePtr gamePtr = initGameJNI( rowid ).retain();
        if ( ! game_makeFromStream( gamePtr, stream, gi, dictNames, dictBytes,
                                    dictPaths, langName, util, draw,
                                    cp, procs ) ) {
            gamePtr.release();
            gamePtr = null;
        }

        return gamePtr;
    }

    public static synchronized GamePtr
        initNew( CurGameInfo gi, String[] dictNames, byte[][] dictBytes,
                 String[] dictPaths, String langName, UtilCtxt util,
                 DrawCtx draw, CommonPrefs cp, TransportProcs procs )
    {
        GamePtr gamePtr = initGameJNI( 0 );
        game_makeNewGame( gamePtr, gi, dictNames, dictBytes, dictPaths,
                          langName, util, draw, cp, procs );
        return gamePtr.retain();
    }

    // hack to allow cleanup of env owned by thread that doesn't open game
    public static void threadDone()
    {
        envDone( getJNI().m_ptrGlobals );
    }

    private static native void game_makeNewGame( GamePtr gamePtr,
                                                 CurGameInfo gi,
                                                 String[] dictNames,
                                                 byte[][] dictBytes,
                                                 String[] dictPaths,
                                                 String langName,
                                                 UtilCtxt util,
                                                 DrawCtx draw, CommonPrefs cp,
                                                 TransportProcs procs );

    private static native boolean game_makeFromStream( GamePtr gamePtr,
                                                       byte[] stream,
                                                       CurGameInfo gi,
                                                       String[] dictNames,
                                                       byte[][] dictBytes,
                                                       String[] dictPaths,
                                                       String langName,
                                                       UtilCtxt util,
                                                       DrawCtx draw,
                                                       CommonPrefs cp,
                                                       TransportProcs procs );

    public static native boolean game_receiveMessage( GamePtr gamePtr,
                                                      byte[] stream,
                                                      CommsAddrRec retAddr );
    public static native void game_summarize( GamePtr gamePtr, GameSummary summary );
    public static native byte[] game_saveToStream( GamePtr gamePtr,
                                                   CurGameInfo gi  );
    public static native void game_saveSucceeded( GamePtr gamePtr );
    public static native void game_getGi( GamePtr gamePtr, CurGameInfo gi );
    public static native void game_getState( GamePtr gamePtr,
                                             JNIThread.GameStateInfo gsi );
    public static native boolean game_hasComms( GamePtr gamePtr );

    // Keep for historical purposes.  But threading issues make it
    // impossible to implement this without a ton of work.
    // public static native boolean game_changeDict( int gamePtr, CurGameInfo gi,
    //                                               String dictName,
    //                                               byte[] dictBytes,
    //                                               String dictPath );
    private static native void game_dispose( GamePtr gamePtr );

    // Board methods
    public static native void board_setDraw( GamePtr gamePtr, DrawCtx draw );
    public static native void board_invalAll( GamePtr gamePtr );
    public static native boolean board_draw( GamePtr gamePtr );
    public static native void board_drawSnapshot( GamePtr gamePtr, DrawCtx draw,
                                                  int width, int height );

    // Only if COMMON_LAYOUT defined
    public static native void board_figureLayout( GamePtr gamePtr, CurGameInfo gi,
                                                  int left, int top, int width,
                                                  int height, int scorePct,
                                                  int trayPct, int scoreWidth,
                                                  int fontWidth, int fontHt,
                                                  boolean squareTiles,
                                                  BoardDims dims );
    // Only if COMMON_LAYOUT defined
    public static native void board_applyLayout( GamePtr gamePtr, BoardDims dims );

    // public static native void board_setPos( int gamePtr, int left, int top,
                                            // int width, int height,
                                            // int maxCellHt, boolean lefty );
    // public static native void board_setScoreboardLoc( int gamePtr, int left,
    //                                                   int top, int width,
    //                                                   int height,
    //                                                   boolean divideHorizontally );
    // public static native void board_setTrayLoc( int gamePtr, int left,
    //                                             int top, int width,
    //                                             int height, int minDividerWidth );
    // public static native void board_setTimerLoc( int gamePtr,
    //                                              int timerLeft, int timerTop,
    //                                              int timerWidth,
    //                                              int timerHeight );
    public static native boolean board_zoom( GamePtr gamePtr, int zoomBy,
                                             boolean[] canZoom );

    // Not available if XWFEATURE_ACTIVERECT not #defined in C
    // public static native boolean board_getActiveRect( GamePtr gamePtr, Rect rect,
    //                                                   int[] dims );

    public static native boolean board_handlePenDown( GamePtr gamePtr,
                                                      int xx, int yy,
                                                      boolean[] handled );
    public static native boolean board_handlePenMove( GamePtr gamePtr,
                                                      int xx, int yy );
    public static native boolean board_handlePenUp( GamePtr gamePtr,
                                                    int xx, int yy );
    public static native boolean board_containsPt( GamePtr gamePtr,
                                                   int xx, int yy );

    public static native boolean board_juggleTray( GamePtr gamePtr );
    public static native int board_getTrayVisState( GamePtr gamePtr );
    public static native boolean board_hideTray( GamePtr gamePtr );
    public static native boolean board_showTray( GamePtr gamePtr );
    public static native boolean board_toggle_showValues( GamePtr gamePtr );
    public static native boolean board_commitTurn( GamePtr gamePtr,
                                                   boolean phoniesConfirmed,
                                                   boolean turnConfirmed,
                                                   int[] newTiles );

    public static native boolean board_flip( GamePtr gamePtr );
    public static native boolean board_replaceTiles( GamePtr gamePtr );
    public static native int board_getSelPlayer( GamePtr gamePtr );
    public static native boolean board_passwordProvided( GamePtr gamePtr, int player,
                                                         String pass );
    public static native boolean board_redoReplacedTiles( GamePtr gamePtr );
    public static native void board_resetEngine( GamePtr gamePtr );
    public static native boolean board_requestHint( GamePtr gamePtr,
                                                    boolean useTileLimits,
                                                    boolean goBackwards,
                                                    boolean[] workRemains );
    public static native boolean board_beginTrade( GamePtr gamePtr );
    public static native boolean board_endTrade( GamePtr gamePtr );

    public static native boolean board_setBlankValue( GamePtr gamePtr, int player,
                                                      int col, int row, int tile );

    public static native String board_formatRemainingTiles( GamePtr gamePtr );
    public static native void board_sendChat( GamePtr gamePtr, String msg );
    // Duplicate mode to start and stop timer
    public static native void board_pause( GamePtr gamePtr, String msg );
    public static native void board_unpause( GamePtr gamePtr, String msg );

    public enum XP_Key {
        XP_KEY_NONE,
        XP_CURSOR_KEY_DOWN,
        XP_CURSOR_KEY_ALTDOWN,
        XP_CURSOR_KEY_RIGHT,
        XP_CURSOR_KEY_ALTRIGHT,
        XP_CURSOR_KEY_UP,
        XP_CURSOR_KEY_ALTUP,
        XP_CURSOR_KEY_LEFT,
        XP_CURSOR_KEY_ALTLEFT,

        XP_CURSOR_KEY_DEL,
        XP_RAISEFOCUS_KEY,
        XP_RETURN_KEY,

        XP_KEY_LAST
    };
    public static native boolean board_handleKey( GamePtr gamePtr, XP_Key key,
                                                  boolean up, boolean[] handled );
    // public static native boolean board_handleKeyDown( XP_Key key,
    //                                                   boolean[] handled );
    // public static native boolean board_handleKeyRepeat( XP_Key key,
    //                                                     boolean[] handled );

    // Model
    public static native String model_writeGameHistory( GamePtr gamePtr,
                                                        boolean gameOver );
    public static native int model_getNMoves( GamePtr gamePtr );
    public static native int model_getNumTilesInTray( GamePtr gamePtr, int player );
    public static native void model_getPlayersLastScore( GamePtr gamePtr,
                                                         int player,
                                                         LastMoveInfo lmi );
    // Server
    public static native void server_reset( GamePtr gamePtr );
    public static native void server_handleUndo( GamePtr gamePtr );
    public static native boolean server_do( GamePtr gamePtr );
    public static native void server_tilesPicked( GamePtr gamePtr, int player, int[] tiles );
    public static native int server_countTilesInPool( GamePtr gamePtr );

    public static native String server_formatDictCounts( GamePtr gamePtr, int nCols );
    public static native boolean server_getGameIsOver( GamePtr gamePtr );
    public static native String server_writeFinalScores( GamePtr gamePtr );
    public static native boolean server_initClientConnection( GamePtr gamePtr );
    public static native void server_endGame( GamePtr gamePtr );

    // hybrid to save work
    public static native boolean board_server_prefsChanged( GamePtr gamePtr,
                                                            CommonPrefs cp );

    // Comms
    public static native void comms_start( GamePtr gamePtr );
    public static native void comms_stop( GamePtr gamePtr );
    public static native void comms_resetSame( GamePtr gamePtr );
    public static native void comms_getAddr( GamePtr gamePtr, CommsAddrRec addr );
    public static native CommsAddrRec[] comms_getAddrs( GamePtr gamePtr );
    public static native void comms_augmentHostAddr( GamePtr gamePtr, CommsAddrRec addr );
    public static native void comms_dropHostAddr( GamePtr gamePtr, CommsConnType typ );
    public static native int comms_resendAll( GamePtr gamePtr, boolean force,
                                              CommsConnType filter,
                                              boolean andAck );
    public static int comms_resendAll( GamePtr gamePtr, boolean force,
                                       boolean andAck ) {
        return comms_resendAll( gamePtr, force, null, andAck );
    }
    public static native byte[][] comms_getPending( GamePtr gamePtr );

    public static native void comms_ackAny( GamePtr gamePtr );
    public static native void comms_transportFailed( GamePtr gamePtr,
                                                     CommsConnType failed );
    public static native boolean comms_isConnected( GamePtr gamePtr );
    public static native String comms_formatRelayID( GamePtr gamePtr, int indx );
    public static native String comms_getStats( GamePtr gamePtr );

    // Used/defined (in C) for DEBUG only
    public static native void comms_setAddrDisabled( GamePtr gamePtr, CommsConnType typ,
                                                     boolean send, boolean enabled );
    public static native boolean comms_getAddrDisabled( GamePtr gamePtr, CommsConnType typ,
                                                        boolean send );

    public enum SMS_CMD { NONE, INVITE, DATA, DEATH, ACK_INVITE, };
    public static class SMSProtoMsg {
        public SMS_CMD cmd;
        public int gameID;
        public byte[] data;            // other cases
    }

    public static byte[][]
        smsproto_prepOutbound( SMS_CMD cmd, int gameID, byte[] buf, String phone,
                               int port, /*out*/ int[] waitSecs )
    {
        return smsproto_prepOutbound( getJNI().m_ptrGlobals, cmd, gameID, buf,
                                      phone, port, waitSecs );
    }

    public static byte[][]
        smsproto_prepOutbound( String phone, int port, int[] waitSecs )
    {
        return smsproto_prepOutbound( SMS_CMD.NONE, 0, null, phone, port, waitSecs );
    }
    
    public static SMSProtoMsg[] smsproto_prepInbound( byte[] data,
                                                      String fromPhone, int wantPort )
    {
        return smsproto_prepInbound( getJNI().m_ptrGlobals, data, fromPhone, wantPort );
    }

    // Dicts
    public static class DictWrapper {
        private long m_dictPtr;

        public DictWrapper()
        {
            m_dictPtr = 0L;
        }

        public DictWrapper( long dictPtr )
        {
            m_dictPtr = dictPtr;
            dict_ref( dictPtr );
        }

        public void release()
        {
            if ( 0 != m_dictPtr ) {
                dict_unref( m_dictPtr );
                m_dictPtr = 0;
            }
        }

        public long getDictPtr()
        {
            return m_dictPtr;
        }

        // @Override
        public void finalize() throws java.lang.Throwable
        {
            release();
            super.finalize();
        }
    }

    public static native boolean dict_tilesAreSame( long dict1, long dict2 );
    public static native String[] dict_getChars( long dict );
    public static boolean dict_getInfo( byte[] dict, String name, String path,
                                        boolean check, DictInfo info )
    {
        return dict_getInfo( getJNI().m_ptrGlobals, dict, name, path, check, info );
    }

    public static native int dict_getTileValue( long dictPtr, int tile );

    // Dict iterator
    public final static int MAX_COLS_DICT = 15; // from dictiter.h
    public static long di_init( byte[] dict, String name, String path )
    {
        return di_init( getJNI().m_ptrGlobals, dict, name, path );
    }
    public static native void di_setMinMax( long closure, int min, int max );
    public static native void di_destroy( long closure );
    public static native int di_wordCount( long closure );
    public static native int[] di_getCounts( long closure );
    public static native String di_nthWord( long closure, int nn, String delim );
    public static native String[] di_getPrefixes( long closure );
    public static native int[] di_getIndices( long closure );
    public static native byte[][] di_strToTiles( long closure, String str );
    public static native int di_getStartsWith( long closure, byte[] prefix );
    public static native String di_getDesc( long closure );
    public static native String di_tilesToStr( long closure, byte[] tiles, String delim );

    // Private methods -- called only here
    private static native long initGlobals( DUtilCtxt dutil, JNIUtils jniu );
    private static native void cleanGlobals( long jniState );
    private static native byte[] gi_to_stream( long jniState, CurGameInfo gi );
    private static native void gi_from_stream( long jniState, CurGameInfo gi,
                                               byte[] stream );
    private static native byte[] nli_to_stream( long jniState, NetLaunchInfo nli );
    private static native void nli_from_stream( long jniState, NetLaunchInfo nli,
                                                byte[] stream );
    private static native long initGameJNI( long jniState, int seed );
    private static native void envDone( long globals );
    private static native void dict_ref( long dictPtr );
    private static native void dict_unref( long dictPtr );
    private static native boolean dict_getInfo( long jniState, byte[] dict,
                                                String name, String path,
                                                boolean check,
                                                DictInfo info );
    private static native long di_init( long jniState, byte[] dict,
                                        String name, String path );

    private static native byte[][]
        smsproto_prepOutbound( long jniState, SMS_CMD cmd, int gameID, byte[] buf,
                               String phone, int port, /*out*/int[] waitSecs );

    private static native SMSProtoMsg[] smsproto_prepInbound( long jniState,
                                                              byte[] data,
                                                              String fromPhone,
                                                              int wantPort);

    private static native boolean haveEnv( long jniState );
}
