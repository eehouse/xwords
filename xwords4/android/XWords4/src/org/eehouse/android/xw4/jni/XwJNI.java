/* -*- compile-command: "cd ../../../../../../; ant reinstall"; -*- */

package org.eehouse.android.xw4.jni;


// Collection of native methods
public class XwJNI {
    
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

    // Game methods
    public static native int initJNI();
    public static native void game_makeNewGame( int gamePtr,
                                                CurGameInfo gi, 
                                                XW_UtilCtxt util,
                                                DrawCtx draw, int gameID, 
                                                CommonPrefs cp, 
                                                TransportProcs procs, 
                                                byte[] dict );
    public static native boolean game_makeFromStream( int gamePtr,
                                                      byte[] stream, 
                                                      CurGameInfo gi, 
                                                      byte[] dict, 
                                                      XW_UtilCtxt util, 
                                                      DrawCtx draw,
                                                      CommonPrefs cp,
                                                      TransportProcs procs );

    public static native byte[] game_saveToStream( int gamePtr,
                                                   CurGameInfo gi );
    public static native void game_dispose( int gamePtr );

    // Board methods

    public static native void board_invalAll( int gamePtr );
    public static native boolean board_draw( int gamePtr );
    public static native void board_setPos( int gamePtr, int left, int top,
                                            boolean lefty );
    public static native void board_setScale( int gamePtr, int hscale, int vscale );
    public static native boolean board_setShowColors( int gamePtr, boolean on );
    public static native void board_setScoreboardLoc( int gamePtr, int left, 
                                                      int top, int width, 
                                                      int height,
                                                      boolean divideHorizontally );
    public static native void board_setTrayLoc( int gamePtr, int left, 
                                                int top, int width, 
                                                int height, int minDividerWidth );
    public static native boolean board_handlePenDown( int gamePtr, int xx, int yy, 
                                                          boolean[] handled );
    public static native boolean board_handlePenMove( int gamePtr, int xx, int yy );
    public static native boolean board_handlePenUp( int gamePtr, int xx, int yy );

    public static native boolean board_juggleTray( int gamePtr );
    public static native int board_getTrayVisState( int gamePtr );
    public static native boolean board_hideTray( int gamePtr );
    public static native boolean board_showTray( int gamePtr );
    public static native boolean board_toggle_showValues( int gamePtr );
    public static native boolean board_commitTurn( int gamePtr );
    public static native boolean board_flip( int gamePtr );
    public static native boolean board_replaceTiles( int gamePtr );
    public static native void board_resetEngine( int gamePtr );
    public static native boolean board_requestHint( int gamePtr, boolean useTileLimits,
                                                    boolean[] workRemains );
    public static native boolean board_beginTrade( int gamePtr );

    public static native String board_formatRemainingTiles( int gamePtr );

    public static native void server_handleUndo( int gamePtr );
    public static native boolean server_do( int gamePtr );
    public static native String server_formatDictCounts( int gamePtr, int nCols );
    public static native boolean server_getGameIsOver( int gamePtr );

    public static native String model_writeGameHistory( int gamePtr, boolean gameOver );

    public static native String server_writeFinalScores( int gamePtr );
}
