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

import android.content.Context
import android.text.TextUtils

import org.json.JSONException
import org.json.JSONObject
import java.io.Serializable

import org.eehouse.android.xw4.Assert
import org.eehouse.android.xw4.BuildConfig
import org.eehouse.android.xw4.DBUtils
import org.eehouse.android.xw4.GameUtils
import org.eehouse.android.xw4.Log
import org.eehouse.android.xw4.R
import org.eehouse.android.xw4.Utils
import org.eehouse.android.xw4.Utils.ISOCode
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole
import org.eehouse.android.xw4.loc.LocUtils
import org.eehouse.android.xw4.putAnd

/** Info we want to access when the game's closed that's not available
 * in CurGameInfo
 *
 * I assume it's Serializable so it can be passed as a parameter.
 */
class GameSummary : Serializable {
    @JvmField
    var lastMoveTime: Int = 0 // set by jni's server.c on move receipt
    var dupTimerExpires: Int = 0
    var nMoves: Int = 0
    var turn: Int = 0
    var turnIsLocal: Boolean = false
    @JvmField
    var nPlayers: Int = 0
    var missingPlayers: Int = 0
    @JvmField
    var scores: IntArray? = null
    var gameOver: Boolean = false
    var quashed: Boolean = false
    private var m_players: Array<String?>? = null
    @JvmField
    var conTypes: CommsConnTypeSet? = null

    // relay-related fields
    @JvmField
    var roomName: String? = null // PENDING remove me
    @JvmField
    var relayID: String? = null
    var seed: Int = 0
    var modtime: Long = 0
    @JvmField
    var created: Long = 0
    @JvmField
    var gameID: Int = 0
    var remoteDevs: Array<String>? = null // BTAddrs and phone numbers

    @JvmField
    var isoCode: ISOCode? = null
    var serverRole: DeviceRole? = null
    @JvmField
    var nPacketsPending: Int = 0
    @JvmField
    var canRematch: Boolean = false

    private var m_giFlags: Int? = null
    private var m_playersSummary: String? = null
    private var m_gi: CurGameInfo? = null
    private var m_remotePhones: Array<String?>? = null
    var extras: String? = null

    constructor()

    constructor(gi: CurGameInfo) {
        nPlayers = gi.nPlayers
        isoCode = gi.isoCode()
        serverRole = gi.serverRole
        gameID = gi.gameID
        m_gi = gi
    }

    fun inRelayGame(): Boolean {
        return null != relayID
    }

    override fun equals(obj: Any?): Boolean {
        var result: Boolean
        if (BuildConfig.DEBUG) {
            result = null != obj && obj is GameSummary
            if (result) {
                val other = obj as GameSummary?
                result =
                    (lastMoveTime == other!!.lastMoveTime
                         && nMoves == other.nMoves
                         && dupTimerExpires == other.dupTimerExpires
                         && turn == other.turn
                         && turnIsLocal == other.turnIsLocal
                         && nPlayers == other.nPlayers
                         && missingPlayers == other.missingPlayers
                         && gameOver == other.gameOver
                         && quashed == other.quashed
                         && seed == other.seed
                         && modtime == other.modtime
                         && created == other.created
                         && gameID == other.gameID
                         && ISOCode.safeEquals(isoCode,other.isoCode)
                         && nPacketsPending == other.nPacketsPending
                         && scores.contentEquals(other.scores)
                         && m_players.contentEquals(other.m_players)
                         && (if ((null == conTypes)) (null == other.conTypes)
                             else conTypes == other.conTypes)
                         && TextUtils.equals(relayID, other.relayID)
                         && remoteDevs.contentEquals(other.remoteDevs)
                         && (if ((null == serverRole)) (null == other.serverRole)
                             else serverRole == other.serverRole)
                         && (if ((null == m_giFlags)) (null == other.m_giFlags)
                             else m_giFlags == other.m_giFlags)
                         && TextUtils.equals(m_playersSummary, other.m_playersSummary)
                         && (if ((null == m_gi)) (null == other.m_gi)
                             else m_gi!!.equals(other.m_gi))
                         && m_remotePhones.contentEquals(other.m_remotePhones)
                         && TextUtils.equals(extras, other.extras)
                    )
            }
        } else {
            result = super.equals(obj)
        }
        return result
    }

    fun summarizePlayers(): String? {
        val result: String?
        if (null == m_gi) {
            result = m_playersSummary
        } else {
            val names = arrayOfNulls<String>(nPlayers)
            for (ii in 0 until nPlayers) {
                names[ii] = m_gi!!.players[ii]!!.name
            }
            result = TextUtils.join("\n", names)
            m_playersSummary = result
        }
        return result
    }

    fun summarizeDevs(): String? {
        var result: String? = null
        if (null != remoteDevs) {
            result = TextUtils.join("\n", remoteDevs!!)
        }
        return result
    }

    fun setRemoteDevs(context: Context, typ: CommsConnType, str: String?) {
        if (null != str && 0 < str.length) {
            remoteDevs = TextUtils.split(str, "\n")!!

            m_remotePhones = arrayOfNulls(remoteDevs!!.size)
            for (ii in remoteDevs!!.indices) {
                m_remotePhones!![ii] =
                    if (typ == CommsConnType.COMMS_CONN_SMS)
                        Utils.phoneToContact(context, remoteDevs!!.get(ii), true)
                    else remoteDevs!!.get(ii)
            }
        }
    }

    fun readPlayers(context: Context, playersStr: String?) {
        if (null != playersStr) {
            m_players = arrayOfNulls(nPlayers)
            val sep = if (playersStr.contains("\n")) {
                "\n"
            } else {
                LocUtils.getString(context, R.string.vs_join)
            }
            var nxt: Int
            var ii = 0
            nxt = 0
            while (true) {
                val prev = nxt
                nxt = playersStr.indexOf(sep, nxt)
                val name =
                    if (-1 == nxt) playersStr.substring(prev)
                    else playersStr.substring(prev, nxt)
                m_players!![ii] = name
                if (-1 == nxt) {
                    break
                }
                nxt += sep.length
                ++ii
            }
        }
    }

    fun setPlayerSummary(summary: String?) {
        m_playersSummary = summary
    }

    fun summarizeState(context: Context): String {
        var result: String? = null
        result = if (gameOver) {
            LocUtils.getString(context, R.string.gameOver)
        } else {
            LocUtils.getQuantityString(
                context, R.plurals.moves_fmt,
                nMoves, nMoves
            )
        }
        return result
    }

    // FIXME: should report based on whatever conType is giving us a
    // successful connection.
    fun summarizeRole(context: Context, rowid: Long): String? {
        var result: String? = null
        if (isMultiGame) {

            val missing = countMissing()
            if (0 < missing) {
                val si = DBUtils.getInvitesFor(context, rowid)
                if (si.minPlayerCount >= missing) {
                    result = if ((null != roomName))
                        LocUtils.getString(context,R.string.summary_invites_out_fmt,
                                           roomName)
                    else
                        LocUtils.getString(context,R.string.summary_invites_out)
                }
            }

            // Otherwise, use BT or SMS
            if (null == result) {
                if (conTypes!!.contains(CommsConnType.COMMS_CONN_BT)
                    || conTypes!!.contains(CommsConnType.COMMS_CONN_SMS)
                    || conTypes!!.contains(CommsConnType.COMMS_CONN_MQTT)
                ) {
                    val fmtID =
                        if (0 < missing) {
                            if (DeviceRole.SERVER_ISSERVER == serverRole) {
                                R.string.summary_wait_host
                            } else {
                                R.string.summary_wait_guest
                            }
                    } else if (gameOver) {
                        R.string.summary_gameover
                    } else if (quashed) {
                        R.string.summary_game_gone
                    } else if (null != remoteDevs
                        && conTypes!!.contains(CommsConnType.COMMS_CONN_SMS)) {
                        result =
                            LocUtils.getString(context, R.string.summary_conn_sms_fmt,
                                               TextUtils.join(", ", m_remotePhones!!))
                            0
                    } else {
                        R.string.summary_conn
                    }
                    if (null == result) {
                        result = LocUtils.getString(context, fmtID)
                    }
                }
            }
        }
        return result
    }

    fun relayConnectPending(): Boolean {
        var result = (conTypes!!.contains(CommsConnType.COMMS_CONN_RELAY)
                && (null == relayID || 0 == relayID!!.length))
        if (result) {
            // Don't report it as unconnected if a game's happening
            // anyway, e.g. via BT.
            result = 0 > turn && !gameOver
        }
        // DbgUtils.logf( "relayConnectPending()=>%b (turn=%d)", result,
        //                turn );
        return result
    }

    val isMultiGame: Boolean
        get() = (serverRole != DeviceRole.SERVER_STANDALONE)

    private fun isLocal(indx: Int): Boolean {
        return localTurnNextImpl(m_giFlags!!, indx)
    }

    private fun isRobot(indx: Int): Boolean {
        val flag = 1 shl (indx * 2)
        val result = 0 != (m_giFlags!! and flag)
        return result
    }

    private fun countMissing(): Int {
        var result = 0
        for (ii in 0 until nPlayers) {
            if (!isLocal(ii) && (0 != ((1 shl ii) and missingPlayers))) {
                ++result
            }
        }
        return result
    }

    fun anyMissing(): Boolean {
        return 0 < countMissing()
    }

    fun giflags(): Int {
        var result: Int
        if (null == m_gi) {
            result = m_giFlags!!
        } else {
            result = 0
            for (ii in 0 until m_gi!!.nPlayers) {
                if (!m_gi!!.players[ii]!!.isLocal) {
                    result = result or (2 shl (ii * 2))
                }
                if (m_gi!!.players[ii]!!.isRobot()) {
                    result = result or (1 shl (ii * 2))
                }
            }

            Assert.assertTrue((result and DUP_MODE_MASK) == 0)
            if (m_gi!!.inDuplicateMode) {
                result = result or DUP_MODE_MASK
            }

            Assert.assertTrue((result and (FORCE_CHANNEL_MASK shl FORCE_CHANNEL_OFFSET)) == 0)
            // Make sure it's big enough
            Assert.assertTrue(0 == (FORCE_CHANNEL_MASK.inv() and m_gi!!.forceChannel))
            result = result or (m_gi!!.forceChannel shl FORCE_CHANNEL_OFFSET)
            // Log.d( TAG, "giflags(): adding forceChannel %d", m_gi.forceChannel );
        }
        return result
    }

    fun inDuplicateMode(): Boolean {
        val flags = giflags()
        return (flags and DUP_MODE_MASK) != 0
    }

    fun setGiFlags(flags: Int) {
        m_giFlags = flags
    }

    val channel: Int
        get() {
            val flags = giflags()
            val channel = (flags shr FORCE_CHANNEL_OFFSET) and FORCE_CHANNEL_MASK
            // Log.d( TAG, "getChannel(id: %X) => %d", gameID, channel );
            return channel
        }

    fun summarizePlayer(context: Context, rowid: Long, indx: Int): String? {
        var player = m_players!![indx]
        var formatID = 0
        if (!isLocal(indx)) {
            val isMissing = 0 != ((1 shl indx) and missingPlayers)
            if (isMissing) {
                val kp = GameUtils.inviteeName(context, rowid, indx)
                player =
                    if (TextUtils.isEmpty(kp))
                        LocUtils.getString(context, R.string.missing_player)
                    else
                        LocUtils.getString(context, R.string.invitee_fmt, kp)
            } else {
                formatID = R.string.str_nonlocal_name_fmt
            }
        } else if (isRobot(indx)) {
            formatID = R.string.robot_name_fmt
        }

        if (0 != formatID) {
            player = LocUtils.getString(context, formatID, player)
        }
        return player
    }

    fun playerNames(context: Context): String? {
        var names: Array<String?>? = null
        if (null != m_gi) {
            names = m_gi!!.visibleNames(context, false)
        } else if (null != m_playersSummary) {
            names = TextUtils.split(m_playersSummary, "\n")
        }

        var result: String? = null
        if (null != names && 0 < names.size) {
            val joiner = LocUtils.getString(context, R.string.vs_join)
            result = TextUtils.join(joiner, names)
        }

        return result
    }

    fun isNextToPlay(indx: Int, isLocal: BooleanArray): Boolean {
        val isNext = indx == turn
        if (isNext) {
            isLocal[0] = isLocal(indx)
        }
        return isNext
    }

    fun nextTurnIsLocal(): Boolean {
        var result = false
        if (!gameOver && 0 <= turn) {
            Assert.assertTrue(null != m_gi || null != m_giFlags)
            result = localTurnNextImpl(giflags(), turn)
        }
        return result
    }

    val prevPlayer: String?
        get() {
            val prevTurn = (turn + nPlayers - 1) % nPlayers
            return m_players!![prevTurn]
        }

    fun dictNames(separator: String?): String {
        var list: String? = null
        if (null != m_gi) {
            val names = m_gi!!.dictNames()
            list = TextUtils.join(separator!!, names)
        }
        return String.format("%s%s%s", separator, list, separator)
    }

    fun putStringExtra(key: String, value: String?): GameSummary {
        value?.let {
            val extras = if ((null == extras)) "{}" else extras!!
            this.extras = JSONObject(extras)
                .putAnd(key, it)
                .toString()
            Log.i(TAG, "putStringExtra(%s,%s) => %s", key, it, this.extras)
        }
        return this
    }

    fun getStringExtra(key: String): String? {
        var result: String? = null
        if (null != extras) {
            try {
                val asObj = JSONObject(extras)
                result = asObj.optString(key)
                if (0 == result.length) {
                    result = null
                }
            } catch (ex: JSONException) {
                Log.ex(TAG, ex)
            }
        }
        // Log.i( TAG, "getStringExtra(%s) => %s", key, result );
        return result
    }

    override fun toString(): String {
        val result =
            if (BuildConfig.NON_RELEASE) {
                StringBuffer("{")
                    .append("nPlayers: ").append(nPlayers).append(',')
                    .append("}")
                    .toString()
            } else {
                super.toString()
            }
        return result
    }

    companion object {
        private val TAG: String = GameSummary::class.java.simpleName

        const val MSG_FLAGS_NONE: Int = 0
        const val MSG_FLAGS_TURN: Int = 1
        const val MSG_FLAGS_CHAT: Int = 2
        const val MSG_FLAGS_GAMEOVER: Int = 4
        const val MSG_FLAGS_ALL: Int = 7
        const val DUP_MODE_MASK: Int = 1 shl (CurGameInfo.MAX_NUM_PLAYERS * 2)
        const val FORCE_CHANNEL_OFFSET: Int = (CurGameInfo.MAX_NUM_PLAYERS * 2) + 1
        const val FORCE_CHANNEL_MASK: Int = 0x03

        private fun localTurnNextImpl(flags: Int, turn: Int): Boolean {
            val flag = 2 shl (turn * 2)
            return 0 == (flags and flag)
        }

        fun localTurnNext(flags: Int, turn: Int): Boolean? {
            var result: Boolean? = null
            if (0 <= turn) {
                result = localTurnNextImpl(flags, turn)
            }
            return result
        }
    }
}
