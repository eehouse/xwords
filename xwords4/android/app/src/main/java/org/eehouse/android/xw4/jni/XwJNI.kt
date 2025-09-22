/*
 * Copyright 2009 - 2024 by Eric House (xwords@eehouse.org).  All rights
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

import java.io.Serializable
import org.json.JSONObject
import kotlin.concurrent.thread

import org.eehouse.android.xw4.Assert
import org.eehouse.android.xw4.BuildConfig
import org.eehouse.android.xw4.Log
import org.eehouse.android.xw4.NetLaunchInfo
import org.eehouse.android.xw4.R
import org.eehouse.android.xw4.Utils
import org.eehouse.android.xw4.Utils.ISOCode
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole

// Collection of native methods and a bit of state
class XwJNI private constructor() {

    // This class is dead. For now it just asserts if anybody tries to use
    // it. Remove when convenient.
    class GamePtr(ptr: Long, rowid: Long) : AutoCloseable {
        val rowid: Long

        init {
            Assert.failDbg()
            this.rowid = rowid
        }

        override fun close() {
            Assert.failDbg()
        }
    }


    // Ok, so for now I can't figure out how to call the real constructor from
    // jni (javac -h and kotlin files are beyond me still) so I'm putting back
    // the nullable fields and an empty constructor. PENDING()...
    class TopicsAndPackets(val topics: Array<String>?,
                           val packets: Array<ByteArray>?,
                           val qos: Int)
    {
        constructor(topic: String, packet: ByteArray, qos: Int):
            this(arrayOf(topic), arrayOf(packet), qos)
        constructor(): this(null, null, 0) // PENDING Remove me later

        fun iterator(): Iterator<Pair<String, ByteArray>>
        {
            val lst = ArrayList<Pair<String, ByteArray>>()
            for (ii in 0 ..< topics!!.size) {
                lst.add(Pair<String, ByteArray>(topics[ii], packets!![ii]))
            }
            return lst.iterator()
        }

        fun qosInt(): Int { return this.qos }

        override fun hashCode(): Int {
            val topCode = topics.contentDeepHashCode()
            // Log.d(TAG, "hashCode(): topic: ${topics!!.get(0)}, code: $topCode")
            val packCode = packets.contentDeepHashCode()
            // Log.d(TAG, "hashCode(): buffer: ${packets!!.get(0).size}, code: $packCode")
            return topCode xor packCode
        }

        override fun equals(other: Any?): Boolean {
            val tmp = other as? TopicsAndPackets
            val result = this === tmp
                || (tmp?.packets.contentDeepEquals(packets)
                        && tmp?.topics.contentDeepEquals(topics))
            // Log.d(TAG, "equals($this, $tmp) => $result")
            return result
        }

        // override fun toString(): String {
        //     val builder = StringBuilder().append("{code: ${hashCode()}, data: [")
        //     if (null != topics) {
        //         val len = topics?.size ?: 0
        //         for (ii in 0..<len) {
        //             val topic = topics!!.get(ii)
        //             builder.append("{topic: $topic, ")
        //                 .append("len: ${packets!!.get(ii).size}},")
        //         }
        //     }
        //     return builder.append("]}").toString()
        // }
    }

    // enum class XP_Key {
    //     XP_KEY_NONE,
    //     XP_CURSOR_KEY_DOWN,
    //     XP_CURSOR_KEY_ALTDOWN,
    //     XP_CURSOR_KEY_RIGHT,
    //     XP_CURSOR_KEY_ALTRIGHT,
    //     XP_CURSOR_KEY_UP,
    //     XP_CURSOR_KEY_ALTUP,
    //     XP_CURSOR_KEY_LEFT,
    //     XP_CURSOR_KEY_ALTLEFT,
    //     XP_CURSOR_KEY_DEL,
    //     XP_RAISEFOCUS_KEY,
    //     XP_RETURN_KEY,
    //     XP_KEY_LAST
    // }

    // enum class SMS_CMD {
    //     NONE,
    //     INVITE,
    //     DATA,
    //     DEATH,
    //     ACK_INVITE
    // }

    companion object {
        private val TAG = XwJNI::class.java.getSimpleName()
        private var s_JNI: XwJNI? = null

        fun dvc_makeMQTTNukeInvite(nli: NetLaunchInfo): TopicsAndPackets {
            return dvc_makeMQTTNukeInvite(Device.ptrGlobals(), nli)
        }

        fun dvc_makeMQTTNoSuchGames(addressee: String, gameID: Int): TopicsAndPackets {
            Log.d(TAG, "dvc_makeMQTTNoSuchGames(to: %s, gameID: %X)",
                  addressee, gameID)
            // DbgUtils.printStack( TAG );
            return dvc_makeMQTTNoSuchGames(Device.ptrGlobals(), addressee, gameID)
        }

        /* XW_TrayVisState enum */
        const val TRAY_HIDDEN = 0
        const val TRAY_REVERSED = 1
        const val TRAY_REVEALED = 2

        fun giFromStream(gi: CurGameInfo, stream: ByteArray) {
            Assert.assertNotNull(stream)
            gi_from_stream(Device.ptrGlobals(), gi, stream) // called here
        }

        fun nliFromStream(stream: ByteArray): NetLaunchInfo? {
            return nli_from_stream(Device.ptrGlobals(), stream)
        }

        // @JvmStatic
        // external fun comms_getUUID(): String

        // @JvmStatic
        // external fun haveLocaleToLc(isoCodeStr: String?, lc: IntArray?): Boolean

        // Game methods
        // private fun initGameJNI(rowid: Long): GamePtr? {
        //     val ptr = gameJNIInit(Device.ptrGlobals())
        //     Assert.assertTrueNR(0L != ptr) // should be impossible
        //     return if (0L == ptr) null else GamePtr(ptr, rowid)
        // }

        @Synchronized
        fun initFromStream(
            rowid: Long, stream: ByteArray, gi: CurGameInfo,
            util: UtilCtxt?, draw: DrawCtx?,
            cp: CommonPrefs, procs: TransportProcs?
        ): GamePtr? {
            Assert.failDbg()
            return null
            // var gamePtr = initGameJNI(rowid)
            // if (!game_makeFromStream(
            //         gamePtr, stream, gi, util, draw,
            //         cp, procs
            //     )
            // ) {
            //     // gamePtr!!.release()
            //     gamePtr = null
            // }
            // return gamePtr
        }

        @Synchronized
        fun initNew(
            gi: CurGameInfo, selfAddr: CommsAddrRec?, hostAddr: CommsAddrRec?,
            util: UtilCtxt?, draw: DrawCtx?, cp: CommonPrefs, procs: TransportProcs?
        ): GamePtr? {
            Assert.failDbg()
            return null
            // Only standalone doesn't provide self address
            // Assert.assertTrueNR(null != selfAddr || gi.deviceRole == DeviceRole.ROLE_STANDALONE)
            // Only client should be providing host addr
            // Assert.assertTrueNR(null == hostAddr || gi.deviceRole == DeviceRole.ROLE_ISGUEST)
            // val gamePtr = initGameJNI(0)
            // game_makeNewGame(gamePtr, gi, selfAddr, hostAddr, util, draw, cp, procs)
            // return gamePtr
        }

        fun game_makeRematch(
            gamePtr: GamePtr, util: UtilCtxt,
            cp: CommonPrefs, gameName: String?,
            newOrder: Array<Int>
        ): GamePtr? {
            val noInts = IntArray(newOrder.size)
            for (ii in newOrder.indices) {
                noInts[ii] = newOrder[ii]
            }
            Assert.failDbg()
            // var gamePtrNew = initGameJNI(0)
            // if (!game_makeRematch(gamePtr, gamePtrNew, util, cp, gameName, noInts)) {
            //     // gamePtrNew!!.release()
            //     gamePtrNew = null
            // }
            return null
        }

        fun game_makeFromInvite(
            nli: NetLaunchInfo, util: UtilCtxt,
            selfAddr: CommsAddrRec,
            cp: CommonPrefs, procs: TransportProcs
        ): GamePtr? {
            Assert.failDbg()
            // var gamePtrNew = initGameJNI(0)
            // if (!game_makeFromInvite(gamePtrNew, nli, util, selfAddr, cp, procs)) {
            //     // gamePtrNew!!.release()
            //     gamePtrNew = null
            // }
            // return gamePtrNew
            return null
        }

        // hack to allow cleanup of env owned by thread that doesn't open game
        fun threadDone() {
            envDone(Device.ptrGlobals())
        }

        @JvmStatic
        private external fun game_makeNewGame(
            gamePtr: GamePtr?,
            gi: CurGameInfo,
            selfAddr: CommsAddrRec?,
            hostAddr: CommsAddrRec?,
            util: UtilCtxt?,
            draw: DrawCtx?, cp: CommonPrefs,
            procs: TransportProcs?
        )

        @JvmStatic
        private external fun game_makeFromStream(
            gamePtr: GamePtr?,
            stream: ByteArray,
            gi: CurGameInfo,
            util: UtilCtxt?,
            draw: DrawCtx?,
            cp: CommonPrefs,
            procs: TransportProcs?
        ): Boolean

        @JvmStatic
        private external fun game_makeRematch(
            gamePtr: GamePtr,
            gamePtrNew: GamePtr?,
            util: UtilCtxt, cp: CommonPrefs,
            gameName: String?, newOrder: IntArray
        ): Boolean

        @JvmStatic
        private external fun game_makeFromInvite(
            gamePtr: GamePtr?, nli: NetLaunchInfo,
            util: UtilCtxt,
            selfAddr: CommsAddrRec,
            cp: CommonPrefs,
            procs: TransportProcs
        ): Boolean

        // @JvmStatic
        // external fun game_receiveMessage(
        //     gamePtr: GamePtr?,
        //     stream: ByteArray?,
        //     retAddr: CommsAddrRec?
        // ): Boolean

        @JvmStatic
        external fun game_summarize(gamePtr: GamePtr?, summary: GameSummary?)
        @JvmStatic
        external fun game_saveToStream(
            gamePtr: GamePtr?,
            gi: CurGameInfo?
        ): ByteArray

        @JvmStatic
        external fun game_saveSucceeded(gamePtr: GamePtr?)
        @JvmStatic

        external fun game_getGi(gamePtr: GamePtr?, gi: CurGameInfo?)
        // @JvmStatic
        // external fun game_getState(
        //     gamePtr: GamePtr,
        //     gsi: GameStateInfo
        // )

        @JvmStatic
        external fun game_hasComms(gamePtr: GamePtr?): Boolean

        // Keep for historical purposes.  But threading issues make it
        // impossible to implement this without a ton of work.
        // public static native boolean game_changeDict( int gamePtr, CurGameInfo gi,
        //                                               String dictName,
        //                                               byte[] dictBytes,
        //                                               String dictPath );
        @JvmStatic
        private external fun game_dispose(gamePtr: GamePtr)

        // Board methods
        @JvmStatic
        external fun board_setDraw(gamePtr: GamePtr?, draw: DrawCtx?)
        @JvmStatic
        external fun board_invalAll(gamePtr: GamePtr?)
        @JvmStatic
        external fun board_draw(gamePtr: GamePtr?): Boolean

        fun board_drawSnapshot(
            gamePtr: GamePtr, draw: DrawCtx?,
            width: Int, height: Int
        ) = Assert.failDbg()

        // Only if COMMON_LAYOUT defined
        @JvmStatic
        external fun board_figureLayout(
            gamePtr: GamePtr?, gi: CurGameInfo?,
            left: Int, top: Int, width: Int,
            height: Int, scorePct: Int,
            trayPct: Int, scoreWidth: Int,
            fontWidth: Int, fontHt: Int,
            squareTiles: Boolean,
            dims: BoardDims?
        )

        // Only if COMMON_LAYOUT defined
        @JvmStatic
        external fun board_applyLayout(gamePtr: GamePtr?, dims: BoardDims?)
        @JvmStatic
        external fun board_zoom(
            gamePtr: GamePtr?, zoomBy: Int,
            canZoom: BooleanArray?
        ): Boolean

        // Not available if XWFEATURE_ACTIVERECT not #defined in C
        // public static native boolean board_getActiveRect( GamePtr gamePtr, Rect rect,
        //                                                   int[] dims );
        @JvmStatic
        external fun board_handlePenDown(
            gamePtr: GamePtr?,
            xx: Int, yy: Int,
            handled: BooleanArray?
        ): Boolean

        @JvmStatic
        external fun board_handlePenMove(
            gamePtr: GamePtr?,
            xx: Int, yy: Int
        ): Boolean

        @JvmStatic
        external fun board_handlePenUp(
            gamePtr: GamePtr?,
            xx: Int, yy: Int
        ): Boolean

        @JvmStatic
        external fun board_containsPt(
            gamePtr: GamePtr?,
            xx: Int, yy: Int
        ): Boolean

        @JvmStatic
        external fun board_juggleTray(gamePtr: GamePtr?): Boolean
        @JvmStatic
        external fun board_getTrayVisState(gamePtr: GamePtr?): Int
        @JvmStatic
        external fun board_hideTray(gamePtr: GamePtr?): Boolean
        @JvmStatic
        external fun board_showTray(gamePtr: GamePtr?): Boolean
        @JvmStatic
        external fun board_commitTurn(
            gamePtr: GamePtr?,
            phoniesConfirmed: Boolean,
            badWordsKey: Int,
            turnConfirmed: Boolean,
            newTiles: IntArray?
        ): Boolean

        @JvmStatic
        external fun board_flip(gamePtr: GamePtr?): Boolean
        @JvmStatic
        external fun board_replaceTiles(gamePtr: GamePtr?): Boolean
        @JvmStatic
        external fun board_getLikelyChatter(gamePtr: GamePtr?): Int
        @JvmStatic
        external fun board_passwordProvided(
            gamePtr: GamePtr?, player: Int,
            pass: String?
        ): Boolean

        @JvmStatic
        external fun board_redoReplacedTiles(gamePtr: GamePtr?): Boolean
        @JvmStatic
        external fun board_requestHint(
            gamePtr: GamePtr?,
            useTileLimits: Boolean,
            goBackwards: Boolean,
            workRemains: BooleanArray?
        ): Boolean

        @JvmStatic
        external fun board_beginTrade(gamePtr: GamePtr?): Boolean
        @JvmStatic
        external fun board_endTrade(gamePtr: GamePtr?): Boolean
        @JvmStatic
        external fun board_setBlankValue(
            gamePtr: GamePtr?, player: Int,
            col: Int, row: Int, tile: Int
        ): Boolean

        @JvmStatic
        external fun board_formatRemainingTiles(gamePtr: GamePtr?): String
        @JvmStatic
        external fun board_sendChat(gamePtr: GamePtr?, msg: String?)

        // Duplicate mode to start and stop timer
        @JvmStatic
        external fun board_pause(gamePtr: GamePtr?, msg: String?)
        @JvmStatic
        external fun board_unpause(gamePtr: GamePtr?, msg: String?)

        // public static native boolean board_handleKey( GamePtr gamePtr, XP_Key key,
        //                                               boolean up, boolean[] handled );
        // public static native boolean board_handleKeyDown( XP_Key key,
        //                                                   boolean[] handled );
        // public static native boolean board_handleKeyRepeat( XP_Key key,
        //                                                     boolean[] handled );
        // Model
        @JvmStatic
        external fun model_writeGameHistory(
            gamePtr: GamePtr?,
            gameOver: Boolean
        ): String

        @JvmStatic
        external fun model_getNMoves(gamePtr: GamePtr?): Int
        @JvmStatic
        external fun model_getNumTilesInTray(gamePtr: GamePtr?, player: Int): Int
        // @JvmStatic
        // external fun model_getPlayersLastScore(
        //     gamePtr: GamePtr?,
        //     player: Int
        // ): LastMoveInfo

        // Server
        // external fun server_reset(gamePtr: GamePtr?)
        // @JvmStatic
        // external fun server_handleUndo(gamePtr: GamePtr?)
        // @JvmStatic
        // external fun server_do(gamePtr: GamePtr?): Boolean
        // @JvmStatic
        // external fun server_tilesPicked(gamePtr: GamePtr?, player: Int, tiles: IntArray?)
        // @JvmStatic
        // external fun server_countTilesInPool(gamePtr: GamePtr?): Int
        // @JvmStatic
        // external fun server_formatDictCounts(gamePtr: GamePtr?, nCols: Int): String
        // @JvmStatic
        // external fun server_getGameIsOver(gamePtr: GamePtr?): Boolean
        // @JvmStatic
        // external fun server_getGameIsConnected(gamePtr: GamePtr?): Boolean
        // @JvmStatic
        // external fun server_writeFinalScores(gamePtr: GamePtr?): String

        @JvmStatic
        external fun server_initClientConnection(gamePtr: GamePtr?): Boolean

        fun server_canOfferRematch(gamePtr: GamePtr): BooleanArray {
            val results: BooleanArray = BooleanArray(2)
            server_canOfferRematch(gamePtr, results)
            return results
        }

        @JvmStatic
        external fun server_canOfferRematch(gamePtr: GamePtr,
                                            results: BooleanArray)

        // fun server_figureOrderKT(gamePtr: GamePtr, ro: RematchOrder): Array<Int> {
        //     val noInts = server_figureOrder(gamePtr, ro)
        //     val result = ArrayList<Int>()
        //     noInts.map{result.add(it)}
        //     return result.toTypedArray()
        // }

        // @JvmStatic
        // private external fun server_figureOrder(gamePtr: GamePtr,
        //                                         ro: RematchOrder): IntArray
        @JvmStatic
        external fun server_endGame(gamePtr: GamePtr?)

        @JvmStatic
        external fun server_inviteeName(gamePtr: GamePtr?, channelNo: Int): String?

        // hybrid to save work
        @JvmStatic
        external fun board_server_prefsChanged(
            gamePtr: GamePtr?,
            cp: CommonPrefs?
        ): Boolean

        // Comms
        // @JvmStatic
        // external fun comms_start(gamePtr: GamePtr?)
        // @JvmStatic
        // external fun comms_stop(gamePtr: GamePtr?)
        // @JvmStatic
        // external fun comms_getSelfAddr(gamePtr: GamePtr?): CommsAddrRec
        // @JvmStatic
        // external fun comms_getHostAddr(gamePtr: GamePtr?): CommsAddrRec?
        // @JvmStatic
        // external fun comms_getAddrs(gamePtr: GamePtr?): Array<CommsAddrRec>?
        // @JvmStatic
        // external fun comms_dropHostAddr(gamePtr: GamePtr?, typ: CommsConnType?)
        // @JvmStatic
        // external fun comms_setQuashed(gamePtr: GamePtr?, quashed: Boolean): Boolean
        // @JvmStatic
        // external fun comms_resendAll(
        //     gamePtr: GamePtr?, force: Boolean,
        //     filter: CommsConnType?,
        //     andAck: Boolean
        // ): Int

        // fun comms_resendAll(
        //     gamePtr: GamePtr?, force: Boolean,
        //     andAck: Boolean
        // ): Int {
        //     return comms_resendAll(gamePtr, force, null, andAck)
        // }

        // @JvmStatic
        // external fun comms_countPendingPackets(gamePtr: GamePtr?): Int
        // @JvmStatic
        // external fun comms_ackAny(gamePtr: GamePtr?)
        // @JvmStatic
        // external fun comms_isConnected(gamePtr: GamePtr?): Boolean
        // @JvmStatic
        // external fun comms_getStats(gamePtr: GamePtr?): String?
        // @JvmStatic
        // external fun comms_addMQTTDevID(
        //     gamePtr: GamePtr?, channelNo: Int,
        //     devID: String?
        // )

        // @JvmStatic
        // external fun comms_invite(
        //     gamePtr: GamePtr?, nli: NetLaunchInfo?,
        //     destAddr: CommsAddrRec?, sendNow: Boolean
        // )

        // Used/defined (in C) for DEBUG only
        // @JvmStatic
        // external fun comms_setAddrDisabled(
        //     gamePtr: GamePtr?, typ: CommsConnType?,
        //     send: Boolean, enabled: Boolean
        // )

        // @JvmStatic
        // external fun comms_getAddrDisabled(
        //     gamePtr: GamePtr?, typ: CommsConnType?,
        //     send: Boolean
        // ): Boolean


        // Private methods -- called only here
        //
        // Some don't need JvmStatic because I used newer form of jni function
        // name.
        private external fun globalsInit(dutil: DUtilCtxt, jniu: JNIUtils, seed: Long): Long
		// private external fun dvc_getMQTTDevID(jniState: Long): String
		// @JvmStatic
		// private external fun dvc_setMQTTDevID(jniState: Long, newid: String): Boolean
		// @JvmStatic
        // private external fun dvc_resetMQTTDevID(jniState: Long)
		@JvmStatic
        private external fun dvc_makeMQTTNukeInvite(
            jniState: Long,
            nli: NetLaunchInfo
        ): TopicsAndPackets

		@JvmStatic
        private external fun dvc_makeMQTTNoSuchGames(
            jniState: Long,
            addressee: String,
            gameID: Int
        ): TopicsAndPackets

		// @JvmStatic
        // private external fun dvc_parseMQTTPacket(
        //     jniState: Long, topic: String,
        //     buf: ByteArray
        // )

		// @JvmStatic
        // private external fun dvc_onWebSendResult(
        //     jniState: Long, resultKey: Int,
        //     succeeded: Boolean,
        //     result: String?
        // )

		// @JvmStatic
        // private external fun kplr_getPlayers(jniState: Long, byDate: Boolean): Array<String>?
		// @JvmStatic
        // private external fun kplr_renamePlayer(
        //     jniState: Long, oldName: String,
        //     newName: String
        // ): Boolean

		// @JvmStatic
        // private external fun kplr_deletePlayer(jniState: Long, player: String)
		// @JvmStatic
        // private external fun kplr_getAddr(
        //     jniState: Long, name: String,
        //     lastMod: IntArray?
        // ): CommsAddrRec

		// @JvmStatic
        // external fun kplr_nameForMqttDev(jniState: Long, mqttID: String?): String
		@JvmStatic
        private external fun cleanGlobals(jniState: Long)
		@JvmStatic
        private external fun gi_from_stream(
            jniState: Long, gi: CurGameInfo,
            stream: ByteArray
        )

		@JvmStatic
        private external fun nli_from_stream(jniState: Long, stream: ByteArray): NetLaunchInfo
		@JvmStatic
        private external fun gameJNIInit(jniState: Long): Long
		@JvmStatic
        private external fun envDone(globals: Long)
        // This always returns true on release builds now.
		@JvmStatic
        private external fun haveEnv(jniState: Long): Boolean
    }
}
