/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2024 by Eric House (xwords@eehouse.org).  All
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

import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet
import org.eehouse.android.xw4.jni.CurGameInfo.XWPhoniesChoice

interface UtilCtxt {
    fun notifyPickTileBlank(playerNum: Int, col: Int, row: Int, texts: Array<String>) {}
    fun informNeedPickTiles(
        isInitial: Boolean, playerNum: Int, nToPick: Int,
        texts: Array<String>, counts: IntArray
    ) {}

    fun informNeedPassword(player: Int, name: String?) {}
    fun turnChanged(newTurn: Int) {}
    fun engineProgressCallback(): Boolean = false
    fun setTimer(why: Int, `when`: Int, handle: Int) {}
    fun clearTimer(why: Int) {}
    fun requestTime() {}
    fun remSelected() {}
    fun timerSelected(inDuplicateMode: Boolean, canPause: Boolean) {}
    fun informWordsBlocked(nWords: Int, words: String?, dict: String?) {}
    fun getInviteeName(index: Int): String? = null
    fun bonusSquareHeld(bonus: Int) {}
    fun playerScoreHeld(player: Int) {}
    fun cellSquareHeld(words: String) {}
    fun notifyMove(query: String?) {}
    fun notifyTrade(tiles: Array<String?>?) {}
    fun notifyDupStatus(amHost: Boolean, msg: String?) {}
    fun userError(id: Int) {}
    fun informMove(turn: Int, expl: String?, words: String?) {}
    fun informUndo() {}
    fun informNetDict(
        isoCodeStr: String?, oldName: String?, newName: String?,
        newSum: String?, phonies: XWPhoniesChoice?
    ) {}

    fun informMissing(
        isServer: Boolean, hostAddr: CommsAddrRec?,
        connTypes: CommsConnTypeSet?, nDevs: Int,
        nMissingPlayers: Int, nInvited: Int, fromRematch: Boolean
    ) {}

    fun notifyGameOver() {}

    // Don't need this unless we have a scroll thumb to indicate position
    //void yOffsetChange( int maxOffset, int oldOffset, int newOffset );
    fun notifyIllegalWords(
        dict: String?, words: Array<String?>?, turn: Int,
        turnLost: Boolean, badWordsKey: Int
    ) {}

    fun showChat(msg: String?, fromPlayer: Int, tsSeconds: Int) {}
    fun formatPauseHistory(
        pauseTyp: Int, player: Int, whenPrev: Int,
        whenCur: Int, msg: String?
    ): String? = null

    companion object {
        const val BONUS_NONE = 0
        const val BONUS_DOUBLE_LETTER = 1
        const val BONUS_DOUBLE_WORD = 2
        const val BONUS_TRIPLE_LETTER = 3
        const val BONUS_TRIPLE_WORD = 4
        const val BONUS_QUAD_LETTER = 5
        const val BONUS_QUAD_WORD = 6
        const val TRAY_HIDDEN = 0
        const val TRAY_REVERSED = 1
        const val TRAY_REVEALED = 2

        // must match defns in util.h
        const val PICKER_PICKALL = -1
        const val PICKER_BACKUP = -2

        // Values for why; should be enums
        const val TIMER_PENDOWN = 1
        const val TIMER_TIMERTICK = 2
        const val TIMER_COMMS = 3
        const val TIMER_SLOWROBOT = 4
        const val TIMER_DUP_TIMERCHECK = 5
        const val NUM_TIMERS_PLUS_ONE = 6

        // These can't be an ENUM! The set is open-ended, with arbitrary values
        // added to ERR_RELAY_BASE, so no way to create an enum from an int in the
        // jni world. int has to be passed into userError(). Trust me: I
        // tried. :-)
        const val ERR_NONE = 0
        const val ERR_TILES_NOT_IN_LINE = 1
        const val ERR_NO_EMPTIES_IN_TURN = 2
        const val ERR_TWO_TILES_FIRST_MOVE = 3
        const val ERR_TILES_MUST_CONTACT = 4
        const val ERR_TOO_FEW_TILES_LEFT_TO_TRADE = 5
        const val ERR_NOT_YOUR_TURN = 6
        const val ERR_NO_PEEK_ROBOT_TILES = 7
        const val ERR_SERVER_DICT_WINS = 8
        const val ERR_NO_PEEK_REMOTE_TILES = 9
        const val ERR_REG_UNEXPECTED_USER = 10
        const val ERR_REG_SERVER_SANS_REMOTE = 11
        const val STR_NEED_BT_HOST_ADDR = 12
        const val ERR_NO_EMPTY_TRADE = 13
        const val ERR_TOO_MANY_TRADE = 14
        const val ERR_CANT_UNDO_TILEASSIGN = 15
        const val ERR_CANT_HINT_WHILE_DISABLED = 16
        const val ERR_NO_HINT_FOUND = 17
        const val ERR_RELAY_BASE = 18
    }
}
