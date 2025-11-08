/*
 * Copyright 2025 by Eric House (xwords@eehouse.org).  All rights
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
package org.eehouse.android.xw4.jni

import android.content.Context
import android.net.Uri
import android.os.Parcel
import android.os.Parcelable
import java.io.Serializable

import org.eehouse.android.xw4.Assert
import org.eehouse.android.xw4.Log
import org.eehouse.android.xw4.NetLaunchInfo
import org.eehouse.android.xw4.NetUtils
import org.eehouse.android.xw4.R
import org.eehouse.android.xw4.jni.GameMgr.GroupRef
import org.eehouse.android.xw4.loc.LocUtils

private val TAG: String = GameRef::class.java.simpleName
class GameRef(val gr: Long): Parcelable, Serializable {
    private val jniState: Long

    init {
        jniState = Device.ptrGlobals()
    }

    override fun equals(other: Any?): Boolean {
        val result =
            other != null
            && other is GameRef
            && (other as GameRef).gr == gr
        return result
    }

    override fun hashCode(): Int {
        return gr.hashCode()
    }

    override fun toString(): String {
        return String.format("%X", gr)
    }

    override fun describeContents(): Int {
        return 0
    }

    override fun writeToParcel(dest: Parcel, flags: Int) {
        dest.writeLong(gr)
    }

    suspend fun getGI(): CurGameInfo? {
        val gi = Device.await {
            gr_getGI(jniState, gr)
        } as CurGameInfo?
        return gi
    }

    fun setGI(gi: CurGameInfo) {
        Device.post {
            gr_setGI(jniState, gr, gi)
        }
    }

    suspend fun getGroup(): GroupRef {
        val grp = Device.await {
            gr_getGroup(jniState, gr)
        } as Int
        return GroupRef(grp)
    }

    fun setGameName(newName: String) {
        Device.post {
            gr_setGameName(jniState, gr, newName)
        }
    }

    // These must be synchronous because the caller needs the result to return
    // to the OS.
    fun containsPt(xx: Int, yy: Int): Boolean {
        return Device.blockFor {
            gr_containsPt(jniState, gr, xx, yy)
        } as Boolean
    }

    fun handlePenDown(xx: Int, yy: Int) {
        Device.post {
            gr_handlePenDown(jniState, gr, xx, yy)
        }
    }

    fun handlePenMove(xx: Int, yy: Int) {
        return Device.post {
            gr_handlePenMove(jniState, gr, xx, yy)
        }
    }

    fun handlePenUp(xx: Int, yy: Int) {
        Device.post {
            gr_handlePenUp(jniState, gr, xx, yy)
        }
    }

    fun zoom(zoomBy: Int) {
        Device.post {
            gr_zoom(jniState, gr, zoomBy)
        }
    }

    fun setDraw(draw: DrawCtx? = null, util: UtilCtxt? = null) {
        Device.post {
            gr_setDraw(jniState, gr, draw, util)
        }
    }

    fun draw() {
        Device.post {
            gr_draw(jniState, gr)
        }
    }

    fun invalAll() {
        Device.post {
            gr_invalAll(jniState, gr)
        }
    }

    suspend fun figureLayout( left: Int, top: Int, width: Int,
                              height: Int, scorePct: Int,
                              trayPct: Int, scoreWidth: Int,
                              fontWidth: Int, fontHt: Int,
                              squareTiles: Boolean ) : BoardDims
    {
        return Device.await {
            val dims = BoardDims()
            gr_figureLayout(jniState, gr, left, top, width,
                            height, scorePct,
                            trayPct, scoreWidth,
                            fontWidth, fontHt,
                            squareTiles, dims)
            dims
        } as BoardDims
    }

    fun applyLayout(dims: BoardDims) {
        Device.post {
            gr_applyLayout(jniState, gr, dims)
        }
    }

    suspend fun setQuashed(b: Boolean): Boolean {
        return Device.await {
            gr_setQuashed(jniState, gr, b)
        } as Boolean
    }

    suspend fun getSummary(): GameSummary? {
        return Device.await {
            gr_getSummary(jniState, gr)
        } as GameSummary?
    }

    fun start() {
        Device.post {
            gr_start(jniState, gr)
        }
    }

    suspend fun getGameIsOver(): Boolean {
        return Device.await {
            gr_getGameIsOver(jniState, gr)
        } as Boolean
    }

    fun endGame() {
        Device.post {
            gr_endGame(jniState, gr)
        }
    }

    suspend fun formatDictCounts(): String {
        return Device.await {
            gr_formatDictCounts(jniState, gr)
        } as String
    }

    suspend fun getGameIsConnected(): Boolean {
        return Device.await {
            gr_getGameIsConnected(jniState, gr)
        } as Boolean
    }

    suspend fun writeFinalScores(): String {
        return Device.await {
            gr_writeFinalScores(jniState, gr)
        } as String
    }

    suspend fun countTilesInPool(): Int {
        return Device.await {
            gr_countTilesInPool(jniState, gr)
        } as Int
    }

    fun save() {
        Device.post {
            gr_save(jniState, gr)
        }
    }

    suspend fun getStats(): String {
        return Device.await {
            gr_getStats(jniState, gr)
        } as String
    }

    suspend fun getAddrs(): Array<CommsAddrRec>? {
        return Device.await {
            gr_getAddrs(jniState, gr)
        } as Array<CommsAddrRec>?
    }

    suspend fun getSelfAddr(): CommsAddrRec? {
        return Device.await {
            gr_getSelfAddr(jniState, gr)
        } as CommsAddrRec?
    }

    suspend fun getState(): GameStateInfo {
        return Device.await {
            gr_getState(jniState, gr)
        } as GameStateInfo
    }

    suspend fun getPlayersLastScore(player: Int): LastMoveInfo {
        return Device.await {
            gr_getPlayersLastScore(jniState, gr, player)
        } as LastMoveInfo
    }

    suspend fun requestHint(getNext: Boolean, workRemains: BooleanArray?): Boolean {
        return Device.await {
            gr_requestHint(jniState, gr, getNext, workRemains)
        } as Boolean
    }

    fun flip() {
        Device.post {
            gr_flip( jniState, gr )
        }
    }

    fun juggleTray() {
        Device.post {
            gr_juggleTray( jniState, gr )
        }
    }

    fun toggleTray() {
        Device.post {
            gr_toggleTray( jniState, gr )
        }
    }

    fun beginTrade() {
        Device.post {
            gr_beginTrade( jniState, gr )
        }
    }
    fun endTrade() {
        Device.post {
            gr_endTrade( jniState, gr )
        }
    }

    fun replaceTiles() {
        Device.post {
            gr_replaceTiles( jniState, gr )
        }
    }

    fun setBlankValue( player: Int, col: Int, row: Int, tile: Int) {
        Device.post {
            gr_setBlankValue(jniState, gr, player, col, row, tile)
        }
    }

    fun tilesPicked(player: Int, tiles: IntArray?) {
        Device.post {
            gr_tilesPicked(jniState, gr, player, tiles)
        }
    }

    suspend fun getNumTilesInTray(player: Int): Int {
        return Device.await {
            gr_getNumTilesInTray(jniState, gr, player)
        } as Int
    }

    fun commitTurn(phoniesConfirmed: Boolean = false,
                   badWordsKey: Int = 0,
                   turnConfirmed: Boolean = false,
                   newTiles: IntArray? = null) {
        Device.post {
            gr_commitTurn(jniState, gr, phoniesConfirmed, badWordsKey,
                          turnConfirmed, newTiles)
        }
    }

    suspend fun getThumbData(): ByteArray? {
        return Device.await {
            gr_getThumbData(jniState, gr)
        } as ByteArray?
    }

    fun invite(nli: NetLaunchInfo, addr: CommsAddrRec, sendNow: Boolean)  {
        Device.post {
            gr_invite(jniState, gr, nli, addr, sendNow)
        }
    }

    suspend fun inviteUrl(context: Context): String {
        Log.d(TAG, "inviteUrl()")
        val (host, prefix) = NetUtils.getHostAndPrefix(context)

        val result = Device.await {
            gr_inviteUrl(jniState, gr, host!!, prefix)
        } as String
        return result
    }

    suspend fun dropHostAddr(typ: CommsAddrRec.CommsConnType) {
        Assert.failDbg()
     }

    suspend fun getLikelyChatter(): Int {
        return Device.await {
            gr_getLikelyChatter(jniState, gr)
        } as Int
    }

    suspend fun getChatCount(): Int {
        return Device.await {
            gr_getChatCount(jniState, gr)
        } as Int
    }

    suspend fun getNthChat(indx: Int, from: IntArray, ts: IntArray,
                           markShown: Boolean = true): String {
        return Device.await {
            gr_getNthChat(jniState, gr, indx, from, ts, markShown)
        } as String
    }

    fun sendChat(msg: String) {
        Device.post {
            gr_sendChat(jniState, gr, msg)
        }
    }

    fun deleteChats() {
        Device.post {
            gr_deleteChats(jniState, gr)
        }
    }

    suspend fun countPendingPackets(): Int {
        return Device.await {
            gr_countPendingPackets(jniState, gr)
        } as Int
    }

    suspend fun formatRemainingTiles(): String {
        return Device.await {
            gr_formatRemainingTiles(jniState, gr)
        } as String
    }

    suspend fun writeGameHistory(gameOver: Boolean): String {
        return Device.await {
            gr_writeGameHistory(jniState, gr, gameOver)
        } as String
    }

    suspend fun makeRematch(
        newName: String?, ro: RematchOrder,
        archiveAfter: Boolean, deleteAfter: Boolean
    ) : GameRef {
        val grv = Device.await {
            gr_makeRematch(jniState, gr, newName, ro, archiveAfter, deleteAfter)
        } as Long
        return GameRef(grv)
    }

    suspend fun figureOrder(ro: RematchOrder): Array<Int> {
        val tmp = Device.await {
            gr_figureOrder(jniState, gr, ro)
        } as IntArray
        val result = ArrayList<Int>()
        tmp.map{result.add(it)}
        return result.toTypedArray()
    }

    suspend fun canOfferRematch(): BooleanArray {
        val results: BooleanArray = BooleanArray(2)
        Device.await {
            gr_canOfferRematch(jniState, gr, results)
        }
        return results
    }

    suspend fun missingDicts(): Array<String>? {
        return Device.await {
            gr_missingDicts(jniState, gr)
        } as Array<String>?
    }

    suspend fun haveDicts(): Boolean {
        return null == missingDicts()
    }

    suspend fun replaceDicts(oldDict: String, newDict: String ): Boolean {
        Log.d(TAG, "replaceDicts($oldDict, $newDict)")
        Device.await {
            gr_replaceDicts(jniState, gr, oldDict, newDict)
        }
        return false
    }

    suspend fun getNMoves(): Int {
        return Device.await {
            gr_getNMoves(jniState, gr)
        } as Int
    }

    suspend fun getAddrDisabled(typ: CommsAddrRec.CommsConnType, send: Boolean): Boolean {
        return Device.await {
            gr_getAddrDisabled(jniState, gr, typ, send)
        } as Boolean
    }

    fun prefsChanged(cp: CommonPrefs?) {
        Device.post {
            gr_prefsChanged(jniState, gr, cp)
        }
    }

    suspend fun isArchived(): Boolean {
        return Device.await {
            gr_isArchived(jniState, gr)
        } as Boolean
    }

    fun setCollapsed(collapsed: Boolean) {
        Device.post {
            gr_setCollapsed(jniState, gr, collapsed)
        }
    }

    suspend fun getPendingPacketsFor(context: Context, addr: CommsAddrRec)
        : String? {
        val (host, prefix) = NetUtils.getHostAndPrefix(context)
        val result = Device.await {
            gr_getPendingPacketsFor(jniState, gr, addr, host, prefix)
        } as String?
        return result
    }

    suspend fun safeToOpen(): Boolean {
        return 0 == failedOpenCount()
    }

    suspend fun failedOpenCount(): Int {
        return Device.await {
            gr_failedOpenCount(jniState, gr)
        } as Int
    }

    fun setOpenCount(count: Int) {
        Device.post {
            gr_setOpenCount(jniState, gr, count)
        }
    }

    class GameStateInfo() {
        var visTileCount: Int = 0
        var trayVisState: Int = 0
        var canHint: Boolean = false
        var canUndo: Boolean = false
        var canRedo: Boolean = false
        var inTrade: Boolean = false
        var tradeTilesSelected: Boolean = false
        var canChat: Boolean = false
        var canShuffle: Boolean = false
        var curTurnSelected: Boolean = false
        var canHideRack: Boolean = false
        var canTrade: Boolean = false
        var canPause: Boolean = false
        var canUnpause: Boolean = false
    }

    // Keep in sync with server.h
    enum class RematchOrder(val strID: Int) {
        RO_NONE(0),
        RO_SAME(R.string.ro_same),
        RO_LOW_SCORE_FIRST(R.string.ro_low_score_first),
        RO_HIGH_SCORE_FIRST(R.string.ro_high_score_first),
        RO_JUGGLE(R.string.ro_juggle);
		// fun getStrID(): Int = strID
    }

    companion object {
        val CREATOR
            : Parcelable.Creator<GameRef> = object : Parcelable.Creator<GameRef> {
                override fun createFromParcel(parcel: Parcel): GameRef? {
                    val gr = parcel.readLong()
                    val obj = GameRef(gr)
                    return obj
                }
                override fun newArray(size: Int): Array<GameRef?> {
                    return arrayOfNulls(size)
                }
            }
        
        @JvmStatic
        private external fun gr_getGI(jniState: Long, gr: Long): CurGameInfo?
        @JvmStatic
        private external fun gr_setGI(jniState: Long, gr: Long, gi: CurGameInfo)
        @JvmStatic
        private external fun gr_getGroup(jniState: Long, gr: Long): Int
        @JvmStatic
        private external fun gr_setGameName(jniState: Long, gr: Long, newName: String)
        @JvmStatic
        private external fun gr_containsPt(jniState: Long, gr: Long, xx: Int,
                                           yy: Int): Boolean
        @JvmStatic
        private external fun gr_handlePenDown(jniState: Long, gr: Long, xx: Int,
                                              yy: Int)
        @JvmStatic
        private external fun gr_handlePenMove(jniState: Long, gr: Long, xx: Int,
                                              yy: Int)
        @JvmStatic
        private external fun gr_handlePenUp(jniState: Long, gr: Long,
                                            xx: Int, yy: Int)
        @JvmStatic
        private external fun gr_zoom(jniState: Long, gr: Long, zoomBy: Int)

        @JvmStatic
        private external fun gr_setDraw(jniState: Long, gr: Long,
                                        draw: DrawCtx?, util: UtilCtxt? )
        @JvmStatic
        private external fun gr_draw(jniState: Long, gr: Long)
        @JvmStatic
        private external fun gr_invalAll(jniState: Long, gr: Long)
        @JvmStatic
        private external fun gr_figureLayout(jniState: Long, gr: Long,
                                             left: Int, top: Int, width: Int, height: Int,
                                             scorePct: Int,
                                             trayPct: Int,
                                             scoreWidth: Int,
                                             fontWidth: Int,
                                             fontHt: Int,
                                             squareTiles: Boolean,
                                             dims: BoardDims)
        @JvmStatic
        private external fun gr_applyLayout( jniState: Long, gr: Long,  dims: BoardDims)
        @JvmStatic
        private external fun gr_getSummary(jniState: Long, gr: Long): GameSummary?
        @JvmStatic
        private external fun gr_getGameIsConnected(jniState: Long, gr: Long): Boolean
        @JvmStatic
        private external fun gr_getGameIsOver(jniState: Long, gr: Long): Boolean
        @JvmStatic
        private external fun gr_endGame(jniState: Long, gr: Long)
        @JvmStatic
        private external fun gr_formatDictCounts(jniState: Long, gr: Long): String
        @JvmStatic
        private external fun gr_writeFinalScores(jniState: Long, gr: Long): String
        @JvmStatic
        private external fun gr_countTilesInPool(jniState: Long, gr: Long): Int
        @JvmStatic
        private external fun gr_getAddrs(jniState: Long, gr: Long): Array<CommsAddrRec>?
        @JvmStatic
        private external fun gr_getSelfAddr(jniState: Long, gr: Long): CommsAddrRec?
        @JvmStatic
        private external fun gr_setQuashed(jniState: Long, gr: Long, quashed: Boolean):
            Boolean
        @JvmStatic
        private external fun gr_start(jniState: Long, gr: Long)
        @JvmStatic
        private external fun gr_save(jniState: Long, gr: Long)
        @JvmStatic
        private external fun gr_getStats(jniState: Long, gr: Long): String
        @JvmStatic
        private external fun gr_getState(jniState: Long, gr: Long): GameStateInfo
        @JvmStatic
        private external fun gr_getPlayersLastScore(jniState: Long, gr: Long,
                                                    player: Int): LastMoveInfo
        @JvmStatic
        private external fun gr_requestHint(jniState: Long, gr: Long, getNext: Boolean,
                                            workLeft: BooleanArray?): Boolean
        @JvmStatic
        private external fun gr_flip( jniState: Long, gr: Long )
        @JvmStatic
        private external fun gr_juggleTray( jniState: Long, gr: Long )
        @JvmStatic
        private external fun gr_toggleTray( jniState: Long, gr: Long )
        @JvmStatic
        private external fun gr_beginTrade( jniState: Long, gr: Long )
        @JvmStatic
        private external fun gr_endTrade( jniState: Long, gr: Long )
        @JvmStatic
        private external fun gr_replaceTiles( jniState: Long, gr: Long )
        @JvmStatic
        private external fun gr_setBlankValue(jniState: Long, gr: Long,
                                              player: Int, col: Int, row: Int, tile: Int)
        @JvmStatic
        private external fun gr_tilesPicked(jniState: Long, gr: Long,
                                            player: Int, tiles: IntArray?)
        @JvmStatic
        private external fun gr_getNumTilesInTray(jniState: Long, gr: Long, player: Int): Int
        @JvmStatic
        private external fun gr_commitTurn(jniState: Long, gr: Long,
                                           phoniesConfirmed: Boolean,
                                           badWordsKey: Int, turnConfirmed: Boolean,
                                           newTiles: IntArray?)
        @JvmStatic
        private external fun gr_getThumbData(jniState: Long, gr: Long): ByteArray?

        @JvmStatic
        private external fun gr_invite(jniState: Long, gr: Long, nli: NetLaunchInfo,
                                       addr: CommsAddrRec, sendNow: Boolean)
        @JvmStatic
        private external fun gr_inviteUrl(jniState: Long, gr: Long,
                                          host: String, prefix: String): String
        @JvmStatic
        private external fun gr_getLikelyChatter(jniState: Long, gr: Long): Int
        @JvmStatic
        private external fun gr_getChatCount(jniState: Long, gr: Long): Int
        @JvmStatic
        private external fun gr_getNthChat(jniState: Long, gr: Long, indx: Int,
                                           from: IntArray, ts: IntArray,
                                           markShown: Boolean): String
        @JvmStatic
        private external fun gr_sendChat(jniState: Long, gr: Long, msg: String)
        @JvmStatic
        private external fun gr_deleteChats(jniState: Long, gr: Long)
        @JvmStatic
        private external fun gr_countPendingPackets(jniState: Long, gr: Long): Int
        @JvmStatic
        private external fun gr_formatRemainingTiles( jniState: Long, gr: Long): String
        @JvmStatic
        private external fun gr_writeGameHistory(jniState: Long, gr: Long,
                                                 gameOver: Boolean): String
        @JvmStatic
        private external fun gr_figureOrder(jniState: Long, gr: Long, ro: RematchOrder): IntArray
        @JvmStatic
        private external fun gr_makeRematch(jniState: Long, gr: Long, name: String?,
                                            ro: RematchOrder, archiveAfter: Boolean,
                                            deleteAfter: Boolean): Long
        @JvmStatic
        private external fun gr_canOfferRematch(jniState: Long, gr: Long,
                                                results: BooleanArray)
        @JvmStatic
        private external fun gr_missingDicts(jniState: Long, gr: Long): Array<String>?
        @JvmStatic
        private external fun gr_replaceDicts(jniState: Long, gr: Long,
                                             oldDict: String, newDict: String)
        @JvmStatic
        private external fun gr_getNMoves(jniState: Long, gr: Long): Int
        @JvmStatic
        private external fun gr_getAddrDisabled(jniState: Long, gr: Long,
                                                typ: CommsAddrRec.CommsConnType,
                                                send: Boolean): Boolean
        @JvmStatic
        private external fun gr_prefsChanged(jniState: Long, gr: Long,
                                             cp: CommonPrefs?)
        @JvmStatic
        private external fun gr_isArchived(jniState: Long, gr: Long): Boolean
        @JvmStatic
        private external fun gr_setCollapsed(jniState: Long, gr: Long, collapsed: Boolean)
        @JvmStatic
        private external fun gr_getPendingPacketsFor(jniState: Long, gr: Long,
                                                     addr: CommsAddrRec, host: String,
                                                     prefix: String): String?
        @JvmStatic
        private external fun gr_failedOpenCount(jniState: Long, gr: Long): Int
        @JvmStatic
        private external fun gr_setOpenCount(jniState: Long, gr: Long, count: Int)
    }

}
