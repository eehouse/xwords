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
package org.eehouse.android.xw4.jni

import android.graphics.Rect
import org.eehouse.android.xw4.Log
import org.eehouse.android.xw4.jni.CommonPrefs.TileValueType

interface DrawCtx {
    class DrawScoreInfo {
        var name: String? = null
        var playerNum: Int = 0
        var totalScore: Int = 0
        var nTilesLeft: Int = 0 /* < 0 means don't use */
        var flags: Int = 0 // was CellFlags; use CELL_ constants above
        var isTurn: Boolean = false
        var selected: Boolean = false
        var isRemote: Boolean = false
        var isRobot: Boolean = false
    }

    fun beginDraw(): Boolean {
        Log.d("DrawCtx", "beginDraw() called")
        return false
    }

    fun endDraw() {}

    fun scoreBegin(rect: Rect, numPlayers: Int, scores: IntArray, remCount: Int): Boolean
    fun measureRemText(rect: Rect, nTilesLeft: Int, width: IntArray, height: IntArray): Boolean
    fun measureScoreText(rect: Rect, dsi: DrawScoreInfo, width: IntArray, height: IntArray)
    fun drawRemText(rInner: Rect, rOuter: Rect, nTilesLeft: Int, focussed: Boolean)
    fun score_drawPlayer(rInner: Rect, rOuter: Rect, gotPct: Int, dsi: DrawScoreInfo)

    // New way of drawing
    // boolean drawRemText( int nTilesLeft, boolean focussed, Rect rect );
    // void score_drawPlayers( Rect scoreRect, DrawScoreInfo[] playerData,
    //                         Rect[] playerRects );
    fun drawTimer(rect: Rect, player: Int, secondsLeft: Int, inDuplicateMode: Boolean)

    fun drawCell(
        rect: Rect, text: String?, tile: Int, value: Int,
        owner: Int, bonus: Int, flags: Int, tvType: TileValueType
    ): Boolean

    fun drawBoardArrow(
        rect: Rect, bonus: Int, vert: Boolean, hintAtts: Int,
        flags: Int
    )

    fun trayBegin(rect: Rect, owner: Int, score: Int): Boolean
    fun drawTile(rect: Rect, text: String?, value: Int, flags: Int): Boolean
    fun drawTileMidDrag(
        rect: Rect, text: String?, value: Int, owner: Int,
        flags: Int
    ): Boolean

    fun drawTileBack(rect: Rect, flags: Int): Boolean
    fun drawTrayDivider(rect: Rect, flags: Int)
    fun score_pendingScore(
        rect: Rect, score: Int, playerNum: Int,
        curTurn: Boolean, flags: Int
    )

    // typ possibilities are defined in UtilCtxt.java
    fun objFinished( typ: Int, /*BoardObjectType*/
                     rect: Rect
    )

    fun getThumbData(): ByteArray
    // getDims: Should return width and height, but we're assuming here it's a
    // square
    fun getThumbSize(): Int

    fun dictChanged(dictPtr: Long)

    companion object {
        // These must be kept in sync with the enum CellFlags in draw.h
        const val CELL_NONE: Int = 0x00
        const val CELL_ISBLANK: Int = 0x01
        const val CELL_RECENT: Int = 0x02
        const val CELL_ISSTAR: Int = 0x04
        const val CELL_ISCURSOR: Int = 0x08
        const val CELL_ISEMPTY: Int = 0x10 /* of a tray tile slot */

        // static final int CELL_VALHIDDEN = 0x20;     /* show letter only, not value */
        const val CELL_DRAGSRC: Int = 0x40 /* where drag originated */
        const val CELL_DRAGCUR: Int = 0x80 /* where drag is now */
        const val CELL_CROSSVERT: Int = 0x100
        const val CELL_CROSSHOR: Int = 0x200
        const val CELL_PENDING: Int = 0x400
        const val CELL_ALL: Int = 0x7FF

        /* BoardObjectType */
        const val OBJ_NONE: Int = 0
        const val OBJ_BOARD: Int = 1
        const val OBJ_SCORE: Int = 2
        const val OBJ_TRAY: Int = 3

        // DrawTarget: keep in sync with eponymous enum in common/ world
        const val DT_SCREEN: Int = 1
        const val DT_THUMB: Int = 2
    }
}
