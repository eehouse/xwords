/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */ /*
 * Copyright 2009 - 2022 by Eric House (xwords@eehouse.org).  All rights
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
import org.eehouse.android.xw4.Log
import org.eehouse.android.xw4.NetUtils
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet
import org.eehouse.android.xw4.jni.CurGameInfo.XWPhoniesChoice
import org.eehouse.android.xw4.jni.JNIThread.JNICmd
import org.json.JSONArray
import org.json.JSONObject
import java.net.HttpURLConnection

open class UtilCtxtImpl : UtilCtxt {
    private var m_context: Context? = null

    private constructor() // force subclasses to pass context
    constructor(context: Context?) : super() {
        m_context = context
    }

    override fun requestTime() {
        subclassOverride("requestTime")
    }

    override fun notifyPickTileBlank(
        playerNum: Int, col: Int, row: Int,
        texts: Array<String?>?
    ) {
        subclassOverride("userPickTileBlank")
    }

    override fun informNeedPickTiles(
        isInitial: Boolean, playerNum: Int, nToPick: Int,
        texts: Array<String?>?, counts: IntArray?
    ) {
        subclassOverride("informNeedPickTiles")
    }

    override fun informNeedPassword(player: Int, name: String?) {
        subclassOverride("informNeedPassword")
    }

    override fun turnChanged(newTurn: Int) {
        subclassOverride("turnChanged")
    }

    override fun engineProgressCallback(): Boolean {
        // subclassOverride( "engineProgressCallback" );
        return true
    }

    override fun setTimer(why: Int, `when`: Int, handle: Int) {
        Log.e(TAG, "setTimer(%d) not doing anything...", why)
        subclassOverride("setTimer")
    }

    override fun clearTimer(why: Int) {
        Log.e(TAG, "setTimer(%d) not doing anything...", why)
        subclassOverride("clearTimer")
    }

    override fun remSelected() {
        subclassOverride("remSelected")
    }

    open val rowID: Long
        get() = 0 // to be overridden

    override fun getMQTTIDsFor(relayIDs: Array<String?>?) {
        val rowid = rowID
        if (0L == rowid) {
            Log.d(TAG, "getMQTTIDsFor() no rowid available so dropping")
        } else {
            Thread(Runnable {
                val params = JSONObject()
                val array = JSONArray()
                try {
                    JNIThread.getRetained(rowid).use { thread ->
                        params.put("rids", array)
                        for (rid: String? in relayIDs!!) {
                            array.put(rid)
                        }
                        val conn: HttpURLConnection = NetUtils
                            .makeHttpMQTTConn(m_context, "mids4rids")
                        val resStr: String = NetUtils.runConn(conn, params, true)
                        Log.d(TAG, "mids4rids => %s", resStr)
                        val obj: JSONObject = JSONObject(resStr)
                        val keys: Iterator<String> = obj.keys()
                        while (keys.hasNext()) {
                            val key: String = keys.next()
                            val hid: Int = key.toInt()
                            thread.handle(JNICmd.CMD_SETMQTTID, hid, obj.getString(key))
                        }
                    }
                } catch (ex: Exception) {
                    Log.ex(TAG, ex)
                }
            }).start()
        }
    }

    override fun timerSelected(inDuplicateMode: Boolean, canPause: Boolean) {
        subclassOverride("timerSelected")
    }

    override fun informWordsBlocked(nWords: Int, words: String?, dict: String?) {
        subclassOverride("informWordsBlocked")
    }

    override fun getInviteeName(plyrNum: Int): String? {
        subclassOverride("getInviteeName")
        return null
    }

    override fun bonusSquareHeld(bonus: Int) {}
    override fun playerScoreHeld(player: Int) {}
    override fun cellSquareHeld(words: String?) {}
    override fun notifyMove(query: String?) {
        subclassOverride("notifyMove")
    }

    override fun notifyTrade(tiles: Array<String?>?) {
        subclassOverride("notifyTrade")
    }

    override fun notifyDupStatus(amHost: Boolean, msg: String?) {
        subclassOverride("notifyDupStatus")
    }

    override fun userError(id: Int) {
        subclassOverride("userError")
    }

    override fun informMove(turn: Int, expl: String?, words: String?) {
        subclassOverride("informMove")
    }

    override fun informUndo() {
        subclassOverride("informUndo")
    }

    override fun informNetDict(
        isoCodeStr: String?, oldName: String?,
        newName: String?, newSum: String?,
        phonies: XWPhoniesChoice?
    ) {
        subclassOverride("informNetDict")
    }

    override fun informMissing(
        isServer: Boolean, hostAddr: CommsAddrRec?,
        connTypes: CommsConnTypeSet?, nDevices: Int,
        nMissingPlayers: Int, nInvited: Int,
        fromRematch: Boolean
    ) {
        subclassOverride("informMissing")
    }

    // Probably want to cache the fact that the game over notification
    // showed up and then display it next time game's opened.
    override fun notifyGameOver() {
        subclassOverride("notifyGameOver")
    }

    override fun notifyIllegalWords(
        dict: String?, words: Array<String?>?, turn: Int,
        turnLost: Boolean, badWordsKey: Int
    ) {
        subclassOverride("notifyIllegalWords")
    }

    // These need to go into some sort of chat DB, not dropped.
    override fun showChat(msg: String?, fromPlayer: Int, tsSeconds: Int) {
        subclassOverride("showChat")
    }

    override fun formatPauseHistory(
        pauseTyp: Int, player: Int, whenPrev: Int,
        whenCur: Int, msg: String?
    ): String? {
        subclassOverride("formatPauseHistory")
        return null
    }

    private fun subclassOverride(name: String) {
        // DbgUtils.logf( "%s::%s() called", getClass().getName(), name );
    }

    companion object {
        private val TAG = UtilCtxtImpl::class.java.getSimpleName()
    }
}
