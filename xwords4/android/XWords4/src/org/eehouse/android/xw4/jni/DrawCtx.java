

package org.eehouse.android.xw4.jni;

import android.graphics.Rect;

public interface DrawCtx {
    static final int CELL_NONE = 0x00;
    static final int CELL_ISBLANK = 0x01;
    static final int CELL_HIGHLIGHT = 0x02;
    static final int CELL_ISSTAR = 0x04;
    static final int CELL_ISCURSOR = 0x08;
    static final int CELL_ISEMPTY = 0x10;       /* of a tray tile slot */
    static final int CELL_VALHIDDEN = 0x20;     /* show letter only, not value */
    static final int CELL_DRAGSRC = 0x40;       /* where drag originated */
    static final int CELL_DRAGCUR = 0x80;       /* where drag is now */
    static final int CELL_ALL = 0xFF;

    /* BoardObjectType */
    static final int OBJ_NONE = 0;
    static final int OBJ_BOARD = 1;
    static final int OBJ_SCORE = 2;
    static final int OBJ_TRAY = 3;


    void scoreBegin( Rect rect, int numPlayers, int[] scores, int remCount,
                     int dfs );
    void measureRemText( Rect r, int nTilesLeft, int[] width, int[] height );
    void measureScoreText( Rect r, DrawScoreInfo dsi, int[] width, int[] height );
    void drawRemText( Rect rInner,Rect rOuter, int nTilesLeft, boolean focussed );
    void score_drawPlayer( Rect rInner, Rect rOuter, DrawScoreInfo dsi );

    boolean drawCell( Rect rect, String text, Object[] bitmaps, int tile, 
                      int owner, int bonus, int hintAtts, int flags );
    void drawBoardArrow ( Rect rect, int bonus, boolean vert, int hintAtts,
                          int flags );
    boolean vertScrollBoard( Rect /*out*/ rect, int dist, int dfs );

    boolean trayBegin ( Rect rect, int owner, int dfs );
    void drawTile( Rect rect, String text, Object[] bitmaps, int val, int flags );
    void drawTileMidDrag ( Rect rect, String text, Object[] bitmaps,
                           int val, int owner, int flags );
    void drawTileBack( Rect rect, int flags );
    void drawTrayDivider( Rect rect, int flags );
    void score_pendingScore( Rect rect, int score, int playerNum, int flags );

    void objFinished( /*BoardObjectType*/int typ, Rect rect, int dfs );

}
