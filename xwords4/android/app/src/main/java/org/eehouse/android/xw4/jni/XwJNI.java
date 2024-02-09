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

import java.io.Serializable;
import java.util.Arrays;

import org.eehouse.android.xw4.Assert;
import org.eehouse.android.xw4.BuildConfig;
import org.eehouse.android.xw4.DbgUtils;
import org.eehouse.android.xw4.Log;
import org.eehouse.android.xw4.NetLaunchInfo;
import org.eehouse.android.xw4.Quarantine;
import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.Utils.ISOCode;
import org.eehouse.android.xw4.Utils;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;

// Collection of native methods and a bit of state
public class XwJNI {
    private static final String TAG = XwJNI.class.getSimpleName();

    public static class GamePtr implements AutoCloseable {
        private long m_ptrGame = 0;
        private int m_refCount = 1;
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
            // Log.d( TAG, "ptr(): m_rowid: %d", m_rowid );
            return m_ptrGame;
        }

        public synchronized GamePtr retain()
        {
            Assert.assertTrueNR( 0 < m_refCount );
            ++m_refCount;
            Log.d( TAG, "retain(this=%H, rowid=%d): refCount now %d",
                   this, m_rowid, m_refCount );
            return this;
        }

        public long getRowid() { return m_rowid; }

        public boolean isRetained() { return 0 < m_refCount; }

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
                        Assert.failDbg(); // seen on Play Store console; and now!!
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

        @Override
        public void finalize() throws java.lang.Throwable
        {
            if ( BuildConfig.NON_RELEASE && (0 != m_refCount || 0 != m_ptrGame) ) {
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
        long seed = Utils.nextRandomInt();
        seed <<= 32;
        seed |= Utils.nextRandomInt();
        seed ^= System.currentTimeMillis();
        m_ptrGlobals = globalsInit( new DUtilCtxt(), JNIUtilsImpl.get(), seed );
    }

    public static void cleanGlobalsEmu()
    {
        cleanGlobals();
    }

    public static String dvc_getMQTTDevID()
    {
        return dvc_getMQTTDevID( getJNI().m_ptrGlobals );
    }

    public static boolean dvc_setMQTTDevID( String newID )
    {
        return dvc_setMQTTDevID( getJNI().m_ptrGlobals, newID );
    }

    public static void dvc_resetMQTTDevID()
    {
        dvc_resetMQTTDevID( getJNI().m_ptrGlobals );
    }

    public static String[] dvc_getMQTTSubTopics()
    {
        return dvc_getMQTTSubTopics( getJNI().m_ptrGlobals );
    }

    public static class TopicsAndPackets {
        public String[] topics;
        public byte[][] packets;
        // default constructor is called from JNI world, so don't add another!
    }

    public static TopicsAndPackets dvc_makeMQTTNukeInvite( NetLaunchInfo nli )
    {
        return dvc_makeMQTTNukeInvite( getJNI().m_ptrGlobals, nli );
    }

    public static TopicsAndPackets dvc_makeMQTTNoSuchGames( String addressee, int gameID )
    {
        Log.d( TAG, "dvc_makeMQTTNoSuchGames(to: %s, gameID: %X)", addressee, gameID );
        // DbgUtils.printStack( TAG );
        return dvc_makeMQTTNoSuchGames( getJNI().m_ptrGlobals, addressee, gameID );
    }

    public static void dvc_parseMQTTPacket( String topic, byte[] buf )
    {
        dvc_parseMQTTPacket( getJNI().m_ptrGlobals, topic, buf );
    }

    public static boolean hasKnownPlayers()
    {
        String[] players = kplr_getPlayers();
        return null != players && 0 < players.length;
    }

    public static String[] kplr_getPlayers()
    {
        return kplr_getPlayers( false );
    }

    public static String[] kplr_getPlayers( boolean byDate )
    {
        String[] result = null;
        if ( BuildConfig.HAVE_KNOWN_PLAYERS ) {
            result = kplr_getPlayers( getJNI().m_ptrGlobals, byDate );
        }
        return result;
    }

    public static boolean kplr_renamePlayer( String oldName, String newName )
    {
        return BuildConfig.HAVE_KNOWN_PLAYERS
            ? kplr_renamePlayer( getJNI().m_ptrGlobals, oldName, newName )
            : true;
    }

    public static void kplr_deletePlayer( String player )
    {
        if ( BuildConfig.HAVE_KNOWN_PLAYERS ) {
            kplr_deletePlayer( getJNI().m_ptrGlobals, player );
        }
    }

    public static CommsAddrRec kplr_getAddr( String name )
    {
        return kplr_getAddr( name, null );
    }

    public static CommsAddrRec kplr_getAddr( String name, int[] lastMod )
    {
        return BuildConfig.HAVE_KNOWN_PLAYERS
            ? kplr_getAddr( getJNI().m_ptrGlobals, name, lastMod )
            : null;
    }

    public static String kplr_nameForMqttDev( String mqttID )
    {
        return BuildConfig.HAVE_KNOWN_PLAYERS
            ? kplr_nameForMqttDev( getJNI().m_ptrGlobals, mqttID )
            : null;
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
        System.loadLibrary( BuildConfig.JNI_LIB_NAME );
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

    public static void gi_from_stream( CurGameInfo gi, byte[] stream )
    {
        Assert.assertNotNull( stream );
        gi_from_stream( getJNI().m_ptrGlobals, gi, stream ); // called here
    }

    public static byte[] nliToStream( NetLaunchInfo nli )
    {
        return nli_to_stream( getJNI().m_ptrGlobals, nli );
    }

    public static NetLaunchInfo nliFromStream( byte[] stream )
    {
        return nli_from_stream( getJNI().m_ptrGlobals, stream );
    }

    public static ISOCode lcToLocaleJ( int lc )
    {
        String code = lcToLocale( lc );
        return ISOCode.newIf( code );
    }

    public static boolean haveLocaleToLc( ISOCode isoCode, int[] lc )
    {
        return haveLocaleToLc( isoCode.toString(), lc );
    }

    public static native String comms_getUUID();
    public static native String lcToLocale( int lc );
    public static native boolean haveLocaleToLc( String isoCodeStr, int[] lc );

    // Game methods
    private static GamePtr initGameJNI( long rowid )
    {
        long ptr = gameJNIInit( getJNI().m_ptrGlobals );
        Assert.assertTrueNR( 0 != ptr ); // should be impossible
        GamePtr result = 0 == ptr ? null : new GamePtr( ptr, rowid );
        return result;
    }

    public static synchronized GamePtr
        initFromStream( long rowid, byte[] stream, CurGameInfo gi,
                        UtilCtxt util, DrawCtx draw,
                        CommonPrefs cp, TransportProcs procs )

    {
        GamePtr gamePtr = initGameJNI( rowid );
        if ( ! game_makeFromStream( gamePtr, stream, gi, util, draw,
                                    cp, procs ) ) {
            gamePtr.release();
            gamePtr = null;
        }

        return gamePtr;
    }

    public static synchronized GamePtr
        initNew( CurGameInfo gi, CommsAddrRec selfAddr, CommsAddrRec hostAddr,
                 UtilCtxt util, DrawCtx draw, CommonPrefs cp, TransportProcs procs )
    {
        // Only standalone doesn't provide self address
        Assert.assertTrueNR( null != selfAddr || gi.serverRole == DeviceRole.SERVER_STANDALONE );
        // Only client should be providing host addr
        Assert.assertTrueNR( null == hostAddr || gi.serverRole == DeviceRole.SERVER_ISCLIENT );
        GamePtr gamePtr = initGameJNI( 0 );
        game_makeNewGame( gamePtr, gi, selfAddr, hostAddr, util, draw, cp, procs );
        return gamePtr;
    }

    // Keep in sync with server.h
    public enum RematchOrder {
        RO_NONE(0),
        RO_SAME(R.string.ro_same),
        RO_LOW_SCORE_FIRST(R.string.ro_low_score_first),
        RO_HIGH_SCORE_FIRST(R.string.ro_high_score_first),
        RO_JUGGLE(R.string.ro_juggle),
        ;
        private int mStrID;
        private RematchOrder(int str) { mStrID = str; }
        public int getStrID() { return mStrID; }
    };

    public static GamePtr game_makeRematch( GamePtr gamePtr, UtilCtxt util,
                                            CommonPrefs cp, String gameName,
                                            int[] newOrder )
    {
        GamePtr gamePtrNew = initGameJNI( 0 );
        if ( !game_makeRematch( gamePtr, gamePtrNew, util, cp, gameName, newOrder ) ) {
            gamePtrNew.release();
            gamePtrNew = null;
        }
        return gamePtrNew;
    }

    public static GamePtr game_makeFromInvite( NetLaunchInfo nli, UtilCtxt util,
                                               CommsAddrRec selfAddr,
                                               CommonPrefs cp, TransportProcs procs )
    {
        GamePtr gamePtrNew = initGameJNI( 0 );
        if ( !game_makeFromInvite( gamePtrNew, nli, util, selfAddr, cp, procs ) ) {
            gamePtrNew.release();
            gamePtrNew = null;
        }
        return gamePtrNew;
    }

    // hack to allow cleanup of env owned by thread that doesn't open game
    public static void threadDone()
    {
        envDone( getJNI().m_ptrGlobals );
    }

    private static native void game_makeNewGame( GamePtr gamePtr,
                                                 CurGameInfo gi,
                                                 CommsAddrRec selfAddr,
                                                 CommsAddrRec hostAddr,
                                                 UtilCtxt util,
                                                 DrawCtx draw, CommonPrefs cp,
                                                 TransportProcs procs );

    private static native boolean game_makeFromStream( GamePtr gamePtr,
                                                       byte[] stream,
                                                       CurGameInfo gi,
                                                       UtilCtxt util,
                                                       DrawCtx draw,
                                                       CommonPrefs cp,
                                                       TransportProcs procs );

    private static native boolean game_makeRematch( GamePtr gamePtr,
                                                    GamePtr gamePtrNew,
                                                    UtilCtxt util, CommonPrefs cp,
                                                    String gameName, int[] newOrder );

    private static native boolean game_makeFromInvite( GamePtr gamePtr, NetLaunchInfo nli,
                                                       UtilCtxt util,
                                                       CommsAddrRec selfAddr,
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
    public static native LastMoveInfo model_getPlayersLastScore( GamePtr gamePtr,
                                                                 int player );
    // Server
    public static native void server_reset( GamePtr gamePtr );
    public static native void server_handleUndo( GamePtr gamePtr );
    public static native boolean server_do( GamePtr gamePtr );
    public static native void server_tilesPicked( GamePtr gamePtr, int player, int[] tiles );
    public static native int server_countTilesInPool( GamePtr gamePtr );

    public static native String server_formatDictCounts( GamePtr gamePtr, int nCols );
    public static native boolean server_getGameIsOver( GamePtr gamePtr );
    public static native boolean server_getGameIsConnected( GamePtr gamePtr );
    public static native String server_writeFinalScores( GamePtr gamePtr );
    public static native boolean server_initClientConnection( GamePtr gamePtr );
    public static boolean[] server_canOfferRematch( GamePtr gamePtr )
    {
        boolean[] results = {false, false};
        server_canOfferRematch( gamePtr, results );
        return results;
    }
    private static native void server_canOfferRematch( GamePtr gamePtr, boolean[] results );
    public static native int[] server_figureOrder( GamePtr gamePtr, RematchOrder ro );
    public static native void server_endGame( GamePtr gamePtr );

    // hybrid to save work
    public static native boolean board_server_prefsChanged( GamePtr gamePtr,
                                                            CommonPrefs cp );

    // Comms
    public static native void comms_start( GamePtr gamePtr );
    public static native void comms_stop( GamePtr gamePtr );
    public static native CommsAddrRec comms_getSelfAddr( GamePtr gamePtr );
    public static native CommsAddrRec comms_getHostAddr( GamePtr gamePtr );
    public static native CommsAddrRec[] comms_getAddrs( GamePtr gamePtr );
    public static native void comms_dropHostAddr( GamePtr gamePtr, CommsConnType typ );
    public static native boolean comms_setQuashed( GamePtr gamePtr );
    public static native int comms_resendAll( GamePtr gamePtr, boolean force,
                                              CommsConnType filter,
                                              boolean andAck );
    public static int comms_resendAll( GamePtr gamePtr, boolean force,
                                       boolean andAck ) {
        return comms_resendAll( gamePtr, force, null, andAck );
    }
    public static native int comms_countPendingPackets( GamePtr gamePtr );

    public static native void comms_ackAny( GamePtr gamePtr );
    public static native boolean comms_isConnected( GamePtr gamePtr );
    public static native String comms_getStats( GamePtr gamePtr );
    public static native void comms_addMQTTDevID( GamePtr gamePtr, int channelNo,
                                                  String devID );
    public static native void comms_invite( GamePtr gamePtr, NetLaunchInfo nli,
                                            CommsAddrRec destAddr, boolean sendNow );

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
    public static DictInfo dict_getInfo( byte[] dict, String name, String path,
                                         boolean check )
    {
        DictWrapper wrapper = makeDict( dict, name, path );
        return dict_getInfo( wrapper, check );
    }

    public static DictInfo dict_getInfo( DictWrapper dict, boolean check )
    {
        return dict_getInfo( getJNI().m_ptrGlobals, dict.getDictPtr(),
                             check );
    }

    public static String dict_getDesc( DictWrapper dict )
    {
        return dict_getDesc( dict.getDictPtr() );
    }

    public static String dict_tilesToStr( DictWrapper dict, byte[] tiles, String delim )
    {
        return dict_tilesToStr( dict.getDictPtr(), tiles, delim );
    }

    public static byte[][] dict_strToTiles( DictWrapper dict, String str )
    {
        return dict_strToTiles( dict.getDictPtr(), str );
    }

    public static boolean dict_hasDuplicates( DictWrapper dict )
    {
        return dict_hasDuplicates( dict.getDictPtr() );
    }

    public static String getTilesInfo( DictWrapper dict )
    {
        return dict_getTilesInfo( getJNI().m_ptrGlobals, dict.getDictPtr() );
    }

    public static native int dict_getTileValue( long dictPtr, int tile );

    // Dict iterator
    public final static int MAX_COLS_DICT = 15; // from dictiter.h
    public static DictWrapper makeDict( byte[] bytes, String name, String path )
    {
        long dict = dict_make( getJNI().m_ptrGlobals, bytes, name, path );
        return new DictWrapper( dict );
    }

    public static class PatDesc implements Serializable {
        public String strPat;
        public byte[] tilePat;
        public boolean anyOrderOk;

        @Override
        public String toString()
        {
            return String.format( "{str: %s; nTiles: %d; anyOrderOk: %b}",
                                  strPat, null == tilePat ? 0 : tilePat.length,
                                  anyOrderOk );
        }
    }

    public static class IterWrapper {
        private long iterRef;

        private IterWrapper(long ref) { this.iterRef = ref; }

        private long getRef() { return this.iterRef; }

        @Override
        public void finalize() throws java.lang.Throwable
        {
            di_destroy( iterRef );
            super.finalize();
        }
    }

    public interface DictIterProcs {
        void onIterReady( IterWrapper iterRef );
    }

    public static void di_init( DictWrapper dict, final PatDesc[] pats,
                                final int minLen, final int maxLen,
                                final DictIterProcs callback )
    {
        final long jniState = getJNI().m_ptrGlobals;
        final long dictPtr = dict.getDictPtr();
        new Thread( new Runnable() {
                @Override
                public void run() {
                    IterWrapper wrapper = null;
                    long iterPtr = di_init( jniState, dictPtr, pats,
                                            minLen, maxLen );
                    if ( 0 != iterPtr ) {
                        wrapper = new IterWrapper(iterPtr);
                    }
                    callback.onIterReady( wrapper );
                }
            } ).start();
    }

    public static int di_wordCount( IterWrapper iter )
    {
        return di_wordCount( iter.getRef() );
    }

    public static String di_nthWord( IterWrapper iter, int nn, String delim )
    {
        return di_nthWord( iter.getRef(), nn, delim );
    }

    public static int[] di_getMinMax( IterWrapper iter ) {
        return di_getMinMax( iter.getRef() );
    }

    public static String[] di_getPrefixes( IterWrapper iter )
    {
        return di_getPrefixes( iter.getRef() );
    }

    public static int[] di_getIndices( IterWrapper iter )
    {
        return di_getIndices( iter.getRef() );
    }

    private static native void di_destroy( long closure );
    private static native int di_wordCount( long closure );
    private static native String di_nthWord( long closure, int nn, String delim );
    private static native int[] di_getMinMax( long closure );
    private static native String[] di_getPrefixes( long closure );
    private static native int[] di_getIndices( long closure );

    // Private methods -- called only here
    private static native long globalsInit( DUtilCtxt dutil, JNIUtils jniu, long seed );
    private static native String dvc_getMQTTDevID( long jniState );
    private static native boolean dvc_setMQTTDevID( long jniState, String newid );
    private static native void dvc_resetMQTTDevID( long jniState );
    private static native String[] dvc_getMQTTSubTopics( long jniState );
    private static native TopicsAndPackets dvc_makeMQTTNukeInvite( long jniState,
                                                                   NetLaunchInfo nli );
    private static native TopicsAndPackets
        dvc_makeMQTTNoSuchGames( long jniState, String addressee, int gameID );
    private static native void dvc_parseMQTTPacket( long jniState, String topic,
                                                    byte[] buf );
    private static native String[] kplr_getPlayers( long jniState, boolean byDate );
    private static native boolean kplr_renamePlayer( long jniState, String oldName,
                                                     String newName );
    private static native void kplr_deletePlayer( long jniState, String player );
    private static native CommsAddrRec kplr_getAddr( long jniState, String name,
                                                     int[] lastMod );
    public static native String kplr_nameForMqttDev( long jniState, String mqttID );

    private static native void cleanGlobals( long jniState );
    private static native void gi_from_stream( long jniState, CurGameInfo gi,
                                               byte[] stream );
    private static native byte[] nli_to_stream( long jniState, NetLaunchInfo nli );
    private static native NetLaunchInfo nli_from_stream( long jniState, byte[] stream );
    private static native long gameJNIInit( long jniState );
    private static native void envDone( long globals );
    private static native long dict_make( long jniState, byte[] dict, String name, String path );
    private static native void dict_ref( long dictPtr );
    private static native void dict_unref( long dictPtr );
    private static native byte[][] dict_strToTiles( long dictPtr, String str );
    private static native String dict_tilesToStr( long dictPtr, byte[] tiles, String delim );
    private static native boolean dict_hasDuplicates( long dictPtr );
    private static native String dict_getTilesInfo( long jniState, long dictPtr );
    private static native DictInfo dict_getInfo( long jniState, long dictPtr,
                                                 boolean check );
    private static native String dict_getDesc( long dictPtr );
    private static native long di_init( long jniState, long dictPtr,
                                        PatDesc[] pats, int minLen, int maxLen );

    private static native byte[][]
        smsproto_prepOutbound( long jniState, SMS_CMD cmd, int gameID, byte[] buf,
                               String phone, int port, /*out*/int[] waitSecs );

    private static native SMSProtoMsg[] smsproto_prepInbound( long jniState,
                                                              byte[] data,
                                                              String fromPhone,
                                                              int wantPort);

    // This always returns true on release builds now.
    private static native boolean haveEnv( long jniState );
}
