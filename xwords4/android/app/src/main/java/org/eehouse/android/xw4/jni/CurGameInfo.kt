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
import org.eehouse.android.xw4.Assert
import org.eehouse.android.xw4.BuildConfig
import org.eehouse.android.xw4.DictLangCache
import org.eehouse.android.xw4.DictUtils.dictExists
import org.eehouse.android.xw4.Log
import org.eehouse.android.xw4.R
import org.eehouse.android.xw4.Utils
import org.eehouse.android.xw4.Utils.ISOCode
import org.eehouse.android.xw4.XWApp
import org.eehouse.android.xw4.XWPrefs
import org.eehouse.android.xw4.jni.CurGameInfo
import org.eehouse.android.xw4.loc.LocUtils
import org.json.JSONException
import org.json.JSONObject
import java.io.Serializable
import java.util.Arrays
import java.util.Random
import kotlin.math.abs

class CurGameInfo : Serializable {
    enum class XWPhoniesChoice {
        PHONIES_IGNORE, PHONIES_WARN, PHONIES_DISALLOW, PHONIES_BLOCK,
    }

    enum class DeviceRole {
        SERVER_STANDALONE, SERVER_ISSERVER, SERVER_ISCLIENT
    }

    @JvmField
    var dictName: String? = null
    @JvmField
    var players: Array<LocalPlayer?>
    var isoCodeStr: String? = null // public only for access from JNI; use isoCode() from java
    @JvmField
    var gameID: Int = 0
    var gameSeconds: Int
    @JvmField
    var nPlayers: Int
    var boardSize: Int
    var traySize: Int
    var bingoMin: Int
    @JvmField
    var forceChannel: Int = 0
    @JvmField
    var serverRole: DeviceRole?

    @JvmField
    var inDuplicateMode: Boolean
    var tradeSub7: Boolean
    var hintsNotAllowed: Boolean
    var timerEnabled: Boolean
    var allowPickTiles: Boolean
    var allowHintRect: Boolean
    var phoniesAction: XWPhoniesChoice?

    // private int[] m_visiblePlayers;
    // private int m_nVisiblePlayers;
    private var m_smartness = 0

    // Assert.assertNotNull( m_name );
    var name: String? = null // not shared across the jni boundary

    @JvmOverloads
    constructor(context: Context, inviteID: String? = null as String?) {
        val isNetworked = null != inviteID
        nPlayers = 2
        inDuplicateMode = CommonPrefs.getDefaultDupMode(context)
        gameSeconds = if (inDuplicateMode) (5 * 60)
        else 60 * nPlayers * XWPrefs.getDefaultPlayerMinutes(context)
        boardSize = CommonPrefs.getDefaultBoardSize(context)
        traySize = XWPrefs.getDefaultTraySize(context)
        bingoMin = XWApp.MIN_TRAY_TILES
        players = arrayOfNulls(MAX_NUM_PLAYERS)
        serverRole = if (isNetworked) DeviceRole.SERVER_ISCLIENT
        else DeviceRole.SERVER_STANDALONE
        hintsNotAllowed = !CommonPrefs.getDefaultHintsAllowed(
            context,
            isNetworked
        )
        tradeSub7 = CommonPrefs.getSub7TradeAllowed(context)
        phoniesAction = CommonPrefs.getDefaultPhonies(context)
        timerEnabled = CommonPrefs.getDefaultTimerEnabled(context)
        allowPickTiles = false
        allowHintRect = false
        m_smartness = 0 // needs to be set from players

        try {
            gameID = if ((null == inviteID)) 0 else inviteID.toInt(16)
        } catch (ex: Exception) {
        }

        // Always create MAX_NUM_PLAYERS so jni code doesn't ever have
        // to cons up a LocalPlayer instance.
        for (ii in 0 until MAX_NUM_PLAYERS) {
            players[ii] = LocalPlayer(context, ii)
        }
        if (isNetworked) {
            players[1]!!.isLocal = false
        } else {
            players[0]!!.setRobotSmartness(1)
        }

        // name the local humans now
        var count = 0
        for (ii in 0 until nPlayers) {
            val lp = players[ii]
            if (lp!!.isLocal) {
                lp.name =
                    if (lp.isRobot()) CommonPrefs.getDefaultRobotName(context)
                    else CommonPrefs.getDefaultPlayerName(context, count++)
            }
        }

        if (CommonPrefs.getAutoJuggle(context)) {
            juggle()
        }

        setLang(context, null)
    }

    constructor(src: CurGameInfo) {
        name = src.name
        gameID = src.gameID
        nPlayers = src.nPlayers
        gameSeconds = src.gameSeconds
        boardSize = src.boardSize
        traySize = src.traySize
        bingoMin = src.bingoMin
        players = arrayOfNulls(MAX_NUM_PLAYERS)
        serverRole = src.serverRole
        dictName = src.dictName
        isoCodeStr = src.isoCodeStr
        hintsNotAllowed = src.hintsNotAllowed
        inDuplicateMode = src.inDuplicateMode
        tradeSub7 = src.tradeSub7
        phoniesAction = src.phoniesAction
        timerEnabled = src.timerEnabled
        allowPickTiles = src.allowPickTiles
        allowHintRect = src.allowHintRect
        forceChannel = src.forceChannel
        var ii = 0
        while (ii < MAX_NUM_PLAYERS) {
            players[ii] = LocalPlayer(src.players[ii]!!)
            ++ii
        }

        Utils.testSerialization(this)
    }

    fun isoCode(): ISOCode? {
        return ISOCode.newIf(isoCodeStr)
    }

    override fun toString(): String {
        var result: String? = null
        if (BuildConfig.DEBUG) {
            val sb = StringBuilder(TAG)
                .append(": {nPlayers: ").append(nPlayers)
                .append(", players: [")
            for (ii in 0 until nPlayers) {
                sb.append(players[ii])
                    .append(", ")
            }
            sb.append("], gameID: ").append(gameID)
                .append(", role: ").append(serverRole)
                .append(", hashCode: ").append(hashCode())
                .append(", timerEnabled: ").append(timerEnabled)
                .append(", gameSeconds: ").append(gameSeconds)
                .append('}')

            result = sb.toString()
        } else {
            result = super.toString()
        }
        return result
    }

    val jSONData: String?
        get() {
            var jsonData: String? = null
            try {
                val obj = JSONObject()
                    .put(BOARD_SIZE, boardSize)
                    .put(TRAY_SIZE, traySize)
                    .put(BINGO_MIN, bingoMin)
                    .put(NO_HINTS, hintsNotAllowed)
                    .put(DUP, inDuplicateMode)
                    .put(SUB7, tradeSub7)
                    .put(TIMER, timerEnabled)
                    .put(ALLOW_PICK, allowPickTiles)
                    .put(PHONIES, phoniesAction!!.ordinal)

                jsonData = obj.toString()
            } catch (jse: JSONException) {
                Log.ex(TAG, jse)
            }

            return jsonData
        }

    fun setFrom(jsonData: String?) {
        if (null != jsonData) {
            try {
                val obj = JSONObject(jsonData)
                boardSize = obj.optInt(BOARD_SIZE, boardSize)
                traySize = obj.optInt(TRAY_SIZE, traySize)
                bingoMin = obj.optInt(BINGO_MIN, bingoMin)
                hintsNotAllowed = obj.optBoolean(NO_HINTS, hintsNotAllowed)
                inDuplicateMode = obj.optBoolean(DUP, inDuplicateMode)
                tradeSub7 = obj.optBoolean(SUB7, tradeSub7)
                timerEnabled = obj.optBoolean(TIMER, timerEnabled)
                allowPickTiles = obj.optBoolean(ALLOW_PICK, allowPickTiles)
                val tmp = obj.optInt(PHONIES, phoniesAction!!.ordinal)
                phoniesAction = XWPhoniesChoice.entries[tmp]
            } catch (jse: JSONException) {
                Log.ex(TAG, jse)
            }
        }
    }

    fun setServerRole(newRole: DeviceRole?) {
        serverRole = newRole
        Assert.assertTrue(nPlayers > 0)
        if (nPlayers == 0) { // must always be one visible player
            Assert.assertFalse(players[0]!!.isLocal)
            players[0]!!.isLocal = true
        }
    }

    fun setLang(context: Context, isoCode: ISOCode?, dict: String?) {
        if (null != dict) {
            dictName = dict
        }
        setLang(context, isoCode)
    }

    fun setLang(context: Context, isoCodeNew: ISOCode?) {
        var isoCodeNew = isoCodeNew
        if (null == isoCodeNew) {
            val dictName = CommonPrefs.getDefaultHumanDict(context)
            isoCodeNew = DictLangCache.getDictISOCode(context, dictName)
        }

        if (!TextUtils.equals(isoCodeNew.toString(), this.isoCodeStr)) {
            isoCodeStr = isoCodeNew.toString()
            assignDicts(context)
        }
    }

    var robotSmartness: Int
        get() {
            if (m_smartness == 0) {
                m_smartness = 1 // default if no robots
                for (ii in 0 until nPlayers) {
                    if (players[ii]!!.isRobot()) {
                        m_smartness = players[ii]!!.robotIQ
                        break // should all be the same
                    }
                }
            }
            return m_smartness
        }
        set(smartness) {
            m_smartness = smartness
            for (ii in 0 until nPlayers) {
                if (players[ii]!!.isRobot()) {
                    players[ii]!!.robotIQ = smartness
                }
            }
        }

    fun addDefaults(context: Context, standalone: Boolean): CurGameInfo {
        setLang(context, null)
        nPlayers = 2
        players[0] = LocalPlayer(context, 0)
        players[1] = LocalPlayer(context, 1)
        if (standalone) {
            players[1]!!.setIsRobot(true)
            players[1]!!.name = CommonPrefs.getDefaultRobotName(context)
        } else {
            players[1]!!.isLocal = false
        }
        setServerRole(if (standalone) DeviceRole.SERVER_STANDALONE else DeviceRole.SERVER_ISSERVER)
        return this
    }

    /** return true if any of the changes made would invalide a game
     * in progress, i.e. require that it be restarted with the new
     * params.  E.g. changing a player to a robot is harmless for a
     * local-only game but illegal for a connected one.
     */
    fun changesMatter(other: CurGameInfo): Boolean {
        var matter =
            nPlayers != other.nPlayers || serverRole != other.serverRole || !TextUtils.equals(
                isoCodeStr,
                other.isoCodeStr
            ) || boardSize != other.boardSize || traySize != other.traySize || bingoMin != other.bingoMin || hintsNotAllowed != other.hintsNotAllowed || inDuplicateMode != other.inDuplicateMode || tradeSub7 != other.tradeSub7 || allowPickTiles != other.allowPickTiles || phoniesAction != other.phoniesAction

        if (!matter) {
            matter = dictName != other.dictName
            var ii = 0
            while (!matter && ii < nPlayers) {
                val me = players[ii]
                val him = other.players[ii]
                matter =
                    me!!.isRobot() != him!!.isRobot() || me.isLocal != him.isLocal || me.name != him.name
                ++ii
            }
        }

        return matter
    }

    override fun equals(obj: Any?): Boolean {
        var result: Boolean
        if (BuildConfig.DEBUG) {
            var other: CurGameInfo? = null
            result = null != obj && obj is CurGameInfo
            if (result) {
                other = obj as CurGameInfo?
                result = (TextUtils.equals(
                    isoCodeStr,
                    other!!.isoCodeStr
                ) && gameID == other.gameID && gameSeconds == other.gameSeconds && nPlayers == other.nPlayers && boardSize == other.boardSize && traySize == other.traySize && bingoMin == other.bingoMin && forceChannel == other.forceChannel && hintsNotAllowed == other.hintsNotAllowed && inDuplicateMode == other.inDuplicateMode && tradeSub7 == other.tradeSub7 && timerEnabled == other.timerEnabled && allowPickTiles == other.allowPickTiles && allowHintRect == other.allowHintRect && m_smartness == other.m_smartness && players.contentDeepEquals(
                    other.players
                ) && TextUtils.equals(dictName, other.dictName)
                        && (if ((null == serverRole)) (null == other.serverRole)
                else serverRole == other.serverRole)
                        && (if ((null == phoniesAction)) (null == other.phoniesAction)
                else phoniesAction == other.phoniesAction)
                        && TextUtils.equals(name, other.name))
            }
        } else {
            result = super.equals(obj)
        }
        return result
    }

    fun remoteCount(): Int {
        var count = 0
        for (ii in 0 until nPlayers) {
            if (!players[ii]!!.isLocal) {
                ++count
            }
        }
        return count
    }

    fun forceRemoteConsistent(): Boolean {
        var consistent = serverRole == DeviceRole.SERVER_STANDALONE
        if (!consistent) {
            if (remoteCount() == 0) {
                players[0]!!.isLocal = false
            } else if (remoteCount() == nPlayers) {
                players[0]!!.isLocal = true
            } else {
                consistent = true // nothing changed
            }
        }
        return !consistent
    }

    fun playerNames(newOrder: Array<Int>): Array<String> {
        val noInts = IntArray(newOrder.size)
        for (ii in newOrder.indices) {
            noInts[ii] = newOrder[ii]
        }
        return playerNames(noInts)
    }

    @JvmOverloads
    fun playerNames(newOrder: IntArray? = null as IntArray?): Array<String> {
        val names = arrayOfNulls<String>(nPlayers)
        for (ii in 0 until nPlayers) {
            val indx = newOrder?.get(ii) ?: ii
            names[ii] = players[indx]!!.name
        }
        return names as Array<String>
    }

    fun playerName(indx: Int): String? {
        var result: String? = null
        if (0 <= indx && indx < nPlayers) {
            result = players[indx]!!.name
        }
        return result
    }

    fun playersLocal(): BooleanArray {
        val locs = BooleanArray(nPlayers)
        for (ii in 0 until nPlayers) {
            locs[ii] = players[ii]!!.isLocal
        }
        return locs
    }

    fun visibleNames(context: Context, withDicts: Boolean): Array<String?> {
        val nameFmt = if (withDicts) LocUtils.getString(context, R.string.name_dict_fmt)
        else "%s"
        val names = arrayOfNulls<String>(nPlayers)
        for (ii in 0 until nPlayers) {
            val lp = players[ii]
            if (lp!!.isLocal || serverRole == DeviceRole.SERVER_STANDALONE) {
                var name: String?
                if (lp.isRobot()) {
                    val format = LocUtils.getString(context, R.string.robot_name_fmt)
                    name = String.format(format, lp.name)
                } else {
                    name = lp.name
                }
                names[ii] = String.format(nameFmt, name, dictName(lp))
            } else {
                names[ii] = LocUtils.getString(context, R.string.guest_name)
            }
        }
        return names
    }

    fun dictNames(): Array<String?> {
        val result = arrayOfNulls<String>(nPlayers + 1)
        result[0] = dictName
        for (ii in 0 until nPlayers) {
            result[ii + 1] = players[ii]!!.dictName
        }
        return result
    }

    // Replace any dict that doesn't exist with newDict
    fun replaceDicts(context: Context, newDict: String?) {
        val dicts =
            DictLangCache.getHaveLang(context, isoCode())
        val installed = HashSet(Arrays.asList(*dicts))

        if (!installed.contains(dictName)) {
            dictName = newDict
        }

        for (ii in 0 until nPlayers) {
            val lp = players[ii]
            if (null == lp!!.dictName) {
                // continue to inherit
            } else if (!installed.contains(players[ii]!!.dictName)) {
                players[ii]!!.dictName = newDict
            }
        }
    }

    fun langName(context: Context): String {
        return DictLangCache.getLangNameForISOCode(context, isoCode()!!)!!
    }

    fun dictName(lp: LocalPlayer): String {
        var dname = lp.dictName
        if (null == dname || 0 == dname.length) {
            dname = dictName
        }
        return dname!!
    }

    fun dictName(indx: Int): String? {
        val dname =
            if (0 <= indx && indx < nPlayers) {
                dictName(players[indx]!!)
            } else null
        return dname
    }

    fun addPlayer(): Boolean {
        val added = nPlayers < MAX_NUM_PLAYERS
        // We can add either by adding a player, if nPlayers <
        // MAX_NUM_PLAYERS, or by making an unusable player usable.
        if (added) {
            players[nPlayers]!!.isLocal =
                serverRole == DeviceRole.SERVER_STANDALONE
            ++nPlayers
        }
        return added
    }

    fun setNPlayers(
        nPlayersTotal: Int, nPlayersHere: Int,
        localsAreRobots: Boolean
    ) {
        assert(nPlayersTotal < MAX_NUM_PLAYERS)
        assert(nPlayersHere < nPlayersTotal)
        nPlayers = nPlayersTotal

        for (ii in 0 until nPlayersTotal) {
            val isLocal = ii < nPlayersHere
            val player = players[ii]
            player!!.isLocal = isLocal
            if (isLocal && localsAreRobots) {
                player.setIsRobot(true)
            } else {
                assert(!player.isRobot())
            }
        }
    }

    private fun moveUp(which: Int): Boolean {
        val canMove = which > 0 && which < nPlayers
        if (canMove) {
            val tmp = players[which - 1]
            players[which - 1] = players[which]
            players[which] = tmp
        }
        return canMove
    }

    private fun moveDown(which: Int): Boolean {
        return moveUp(which + 1)
    }

    fun delete(which: Int): Boolean {
        val canDelete = nPlayers > 0
        if (canDelete) {
            val tmp = players[which]
            for (ii in which until nPlayers - 1) {
                moveDown(ii)
            }
            --nPlayers
            players[nPlayers] = tmp
        }
        return canDelete
    }

    fun juggle(): Boolean {
        val canJuggle = nPlayers > 1
        if (canJuggle) {
            // for each element, exchange with randomly chocsen from
            // range <= to self.
            val rgen = Random()

            for (ii in nPlayers - 1 downTo 1) {
                // Contrary to docs, nextInt() comes back negative!
                val rand = abs(rgen.nextInt().toDouble()).toInt()
                val indx = rand % (ii + 1)
                if (indx != ii) {
                    val tmp = players[ii]
                    players[ii] = players[indx]
                    players[indx] = tmp
                }
            }
        }
        return canJuggle
    }

    private fun assignDicts(context: Context) {
        // For each player's dict, if non-null and language matches
        // leave it alone.  Otherwise replace with default if that
        // matches langauge.  Otherwise pick an arbitrary dict in the
        // right language.

        val humanDict =
            DictLangCache.getBestDefault(context, isoCode()!!, true)
        val robotDict =
            DictLangCache.getBestDefault(context, isoCode()!!, false)

        if (null == dictName || !dictExists(context, dictName!!)
            || !DictLangCache.getDictISOCode(context, dictName).equals(isoCode())
        ) {
            dictName = humanDict
        }

        for (ii in 0 until nPlayers) {
            val lp = players[ii]

            if (null != lp!!.dictName &&
                !ISOCode.safeEquals(
                    DictLangCache.getDictISOCode(context, lp.dictName),
                    isoCode()
                )
            ) {
                lp.dictName = null
            }

            if (null == lp.dictName) {
                if (lp.isRobot()) {
                    if (robotDict !== dictName) {
                        lp.dictName = robotDict
                    } else if (humanDict !== dictName) {
                        lp.dictName = humanDict
                    }
                }
            }
        }
    }

    companion object {
        private val TAG: String = CurGameInfo::class.java.simpleName

        const val MAX_NUM_PLAYERS: Int = 4

        private const val BOARD_SIZE = "BOARD_SIZE"
        private const val TRAY_SIZE = "TRAY_SIZE"
        private const val BINGO_MIN = "BINGO_MIN"
        private const val NO_HINTS = "NO_HINTS"
        private const val TIMER = "TIMER"
        private const val ALLOW_PICK = "ALLOW_PICK"
        private const val PHONIES = "PHONIES"
        private const val DUP = "DUP"
        private const val SUB7 = "SUB7"
    }
}
