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

// Collection of native methods
public class XwJNI {


    // This needs to be called before the first attempt to use the
    // jni.  I figure this class has to be loaded before that cna
    // happen.  Doing this in GamesList isn't enough because sometimes
    // BoardActivity is the first Activity loaded.
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
    public static native boolean timerFired( int gamePtr, int why, 
                                             int when, int handle );

    // Stateless methods
    public static native byte[] gi_to_stream( CurGameInfo gi );
    public static native void gi_from_stream( CurGameInfo gi, byte[] stream );
    public static native void comms_getInitialAddr( CommsAddrRec addr,
                                                    String relayHost,
                                                    int relayPort );

    // Game methods
    public static native int initJNI();
    public static native void game_makeNewGame( int gamePtr,
                                                CurGameInfo gi, 
                                                UtilCtxt util,
                                                JNIUtils jniu,
                                                DrawCtx draw, CommonPrefs cp, 
                                                TransportProcs procs, 
                                                byte[][] dicts, 
                                                String[] dictNames,
                                                String langName );

    public static native boolean game_makeFromStream( int gamePtr,
                                                      byte[] stream, 
                                                      CurGameInfo gi, 
                                                      byte[][] dicts, 
                                                      String[] dictNames,
                                                      String langName,
                                                      UtilCtxt util, 
                                                      JNIUtils jniu,
                                                      DrawCtx draw,
                                                      CommonPrefs cp,
                                                      TransportProcs procs );

    // leave out options params for when game won't be rendered or
    // played
    public static void game_makeNewGame( int gamePtr, CurGameInfo gi,
                                         JNIUtils jniu, CommonPrefs cp, 
                                         byte[][] dicts, String[] dictNames,
                                         String langName ) {
        game_makeNewGame( gamePtr, gi, (UtilCtxt)null, jniu,
                          (DrawCtx)null, cp, (TransportProcs)null, 
                          dicts, dictNames, langName );
    }

    public static boolean game_makeFromStream( int gamePtr,
                                               byte[] stream, 
                                               JNIUtils jniu,
                                               CurGameInfo gi, 
                                               byte[][] dicts, 
                                               String[] dictNames,
                                               String langName,
                                               CommonPrefs cp ) {
        return game_makeFromStream( gamePtr, stream, gi, dicts, dictNames,
                                    langName, (UtilCtxt)null, jniu,
                                    (DrawCtx)null, cp, (TransportProcs)null );
    }

    public static boolean game_makeFromStream( int gamePtr,
                                               byte[] stream, 
                                               JNIUtils jniu,
                                               CurGameInfo gi, 
                                               byte[][] dicts, 
                                               String[] dictNames,
                                               String langName,
                                               UtilCtxt util,
                                               CommonPrefs cp ) {
        return game_makeFromStream( gamePtr, stream, gi, dicts, dictNames,
                                    langName, util, jniu, (DrawCtx)null, cp, 
                                    (TransportProcs)null );
    }

    public static native boolean game_receiveMessage( int gamePtr, 
                                                      byte[] stream );
    public static native void game_summarize( int gamePtr, GameSummary summary );
    public static native byte[] game_saveToStream( int gamePtr,
                                                   CurGameInfo gi  );
    public static native void game_getGi( int gamePtr, CurGameInfo gi );
    public static native void game_getState( int gamePtr, 
                                             JNIThread.GameStateInfo gsi );
    public static native boolean game_hasComms( int gamePtr );
    public static native void game_dispose( int gamePtr );

    // Board methods
    public static native void board_invalAll( int gamePtr );
    public static native boolean board_draw( int gamePtr );
    public static native void board_setPos( int gamePtr, int left, int top,
                                            int width, int height, 
                                            int maxCellHt, boolean lefty );
    public static native boolean board_zoom( int gamePtr, int zoomBy, 
                                             boolean[] canZoom );
    public static native void board_setScoreboardLoc( int gamePtr, int left, 
                                                      int top, int width, 
                                                      int height,
                                                      boolean divideHorizontally );
    public static native void board_setTrayLoc( int gamePtr, int left, 
                                                int top, int width, 
                                                int height, int minDividerWidth );
    public static native void board_setTimerLoc( int gamePtr,
                                                 int timerLeft, int timerTop,
                                                 int timerWidth, int timerHeight );

    public static native boolean board_handlePenDown( int gamePtr, 
                                                      int xx, int yy, 
                                                      boolean[] handled );
    public static native boolean board_handlePenMove( int gamePtr, 
                                                      int xx, int yy );
    public static native boolean board_handlePenUp( int gamePtr, 
                                                    int xx, int yy );

    public static native boolean board_juggleTray( int gamePtr );
    public static native int board_getTrayVisState( int gamePtr );
    public static native boolean board_hideTray( int gamePtr );
    public static native boolean board_showTray( int gamePtr );
    public static native boolean board_toggle_showValues( int gamePtr );
    public static native boolean board_commitTurn( int gamePtr );
    public static native boolean board_flip( int gamePtr );
    public static native boolean board_replaceTiles( int gamePtr );
    public static native boolean board_redoReplacedTiles( int gamePtr );
    public static native void board_resetEngine( int gamePtr );
    public static native boolean board_requestHint( int gamePtr, 
                                                    boolean useTileLimits,
                                                    boolean goBackwards,
                                                    boolean[] workRemains );
    public static native boolean board_beginTrade( int gamePtr );
    public static native boolean board_endTrade( int gamePtr );

    public static native String board_formatRemainingTiles( int gamePtr );

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
    public static native boolean board_handleKey( int gamePtr, XP_Key key, 
                                                  boolean up, boolean[] handled );
    // public static native boolean board_handleKeyDown( XP_Key key, 
    //                                                   boolean[] handled );
    // public static native boolean board_handleKeyRepeat( XP_Key key, 
    //                                                     boolean[] handled );

    // Model
    public static native String model_writeGameHistory( int gamePtr, 
                                                        boolean gameOver );
    public static native int model_getNMoves( int gamePtr );

    // Server
    public static native void server_reset( int gamePtr );
    public static native void server_handleUndo( int gamePtr );
    public static native boolean server_do( int gamePtr );
    public static native String server_formatDictCounts( int gamePtr, int nCols );
    public static native boolean server_getGameIsOver( int gamePtr );
    public static native String server_writeFinalScores( int gamePtr );
    public static native void server_initClientConnection( int gamePtr );
    public static native void server_endGame( int gamePtr );
    public static native void server_sendChat( int gamePtr, String msg );

    // hybrid to save work
    public static native boolean board_server_prefsChanged( int gamePtr, 
                                                            CommonPrefs cp );

    // Comms
    public static native void comms_start( int gamePtr );
    public static native void comms_resetSame( int gamePtr );
    public static native void comms_getAddr( int gamePtr, CommsAddrRec addr );
    public static native void comms_setAddr( int gamePtr, CommsAddrRec addr );
    public static native void comms_resendAll( int gamePtr );
    public static native void comms_transportFailed( int gamePtr );
    public static native boolean comms_isConnected( int gamePtr );

    // Dicts
    public static native boolean dict_tilesAreSame( int dictPtr1, int dictPtr2 );
    public static native String[] dict_getChars( int dictPtr );
    public static native void dict_getInfo( byte[] dict, JNIUtils jniu, 
                                            DictInfo info );
    public static native int dict_getTileValue( int dictPtr, int tile );
}
