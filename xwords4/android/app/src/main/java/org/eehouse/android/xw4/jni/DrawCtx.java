/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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

import android.graphics.Rect;

public interface DrawCtx {
    // These must be kept in sync with the enum CellFlags in draw.h
    static final int CELL_NONE = 0x00;
    static final int CELL_ISBLANK = 0x01;
    static final int CELL_RECENT = 0x02;
    static final int CELL_ISSTAR = 0x04;
    static final int CELL_ISCURSOR = 0x08;
    static final int CELL_ISEMPTY = 0x10;       /* of a tray tile slot */
    // static final int CELL_VALHIDDEN = 0x20;     /* show letter only, not value */
    static final int CELL_DRAGSRC = 0x40;       /* where drag originated */
    static final int CELL_DRAGCUR = 0x80;       /* where drag is now */
    static final int CELL_CROSSVERT = 0x100;
    static final int CELL_CROSSHOR = 0x200;
    static final int CELL_PENDING = 0x400;
    static final int CELL_ALL = 0x7FF;

    /* BoardObjectType */
    static final int OBJ_NONE = 0;
    static final int OBJ_BOARD = 1;
    static final int OBJ_SCORE = 2;
    static final int OBJ_TRAY = 3;


    boolean scoreBegin( Rect rect, int numPlayers, int[] scores, int remCount );
    boolean measureRemText( Rect r, int nTilesLeft, int[] width, int[] height );
    void measureScoreText( Rect r, DrawScoreInfo dsi, int[] width, int[] height );
    void drawRemText( Rect rInner, Rect rOuter, int nTilesLeft, boolean focussed );
    void score_drawPlayer( Rect rInner, Rect rOuter, int gotPct,
                           DrawScoreInfo dsi );

    // New way of drawing
    // boolean drawRemText( int nTilesLeft, boolean focussed, Rect rect );
    // void score_drawPlayers( Rect scoreRect, DrawScoreInfo[] playerData,
    //                         Rect[] playerRects );

    void drawTimer( Rect rect, int player, int secondsLeft, boolean inDuplicateMode );

    boolean drawCell( Rect rect, String text, int tile, String value,
                      int owner, int bonus, int hintAtts, int flags );
    void drawBoardArrow ( Rect rect, int bonus, boolean vert, int hintAtts,
                          int flags );
    boolean trayBegin ( Rect rect, int owner, int score );
    boolean drawTile( Rect rect, String text, int val, int flags );
    boolean drawTileMidDrag ( Rect rect, String text, int val, int owner,
                              int flags );
    boolean drawTileBack( Rect rect, int flags );
    void drawTrayDivider( Rect rect, int flags );
    void score_pendingScore( Rect rect, int score, int playerNum,
                             boolean curTurn, int flags );

    public static final int BONUS_NONE = 0;
    public static final int BONUS_DOUBLE_LETTER = 1;
    public static final int BONUS_DOUBLE_WORD = 2;
    public static final int BONUS_TRIPLE_LETTER = 3;
    public static final int BONUS_TRIPLE_WORD = 4;
    public static final int INTRADE_MW_TEXT = 5;

    void objFinished( /*BoardObjectType*/int typ, Rect rect );

    void dictChanged( long dictPtr );

}
