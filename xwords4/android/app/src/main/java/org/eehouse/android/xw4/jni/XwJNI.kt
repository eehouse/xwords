/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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

import org.eehouse.android.xw4.Assert
import org.eehouse.android.xw4.BuildConfig
import org.eehouse.android.xw4.Log
import org.eehouse.android.xw4.NetLaunchInfo
import org.eehouse.android.xw4.Quarantine
import org.eehouse.android.xw4.R
import org.eehouse.android.xw4.Utils
import org.eehouse.android.xw4.Utils.ISOCode
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole
import org.eehouse.android.xw4.jni.JNIThread.GameStateInfo

// Collection of native methods and a bit of state
class XwJNI private constructor() {
    class GamePtr(ptr: Long, rowid: Long) : AutoCloseable {
        private var m_ptrGame: Long = 0
        private var m_refCount = 1
        val rowid: Long
        private val mStack: String

        init {
            m_ptrGame = ptr
            this.rowid = rowid
            mStack = android.util.Log.getStackTraceString(Exception())
            Quarantine.recordOpened(rowid)
        }

        @Synchronized
        fun ptr(): Long {
            Assert.assertTrue(0L != m_ptrGame)
            // Log.d( TAG, "ptr(): m_rowid: %d", m_rowid );
            return m_ptrGame
        }

        @Synchronized
        fun retain(): GamePtr {
            Assert.assertTrueNR(0 < m_refCount)
            ++m_refCount
            Log.d(TAG, "retain(this=%H, rowid=%d): refCount now %d",
                  this, rowid, m_refCount)
            return this
        }

        val isRetained: Boolean
            get() = 0 < m_refCount

        // Force (via an assert in finalize() below) that this is called. It's
        // better if jni stuff isn't being done on the finalizer thread
        @Synchronized
        fun release() {
            --m_refCount
            // Log.d( TAG, "%s.release(this=%H, rowid=%d): refCount now %d",
            //        getClass().getName(), this, m_rowid, m_refCount );
            if (0 == m_refCount) {
                if (0L != m_ptrGame) {
                    Quarantine.recordClosed(rowid)
                    if (haveEnv(jNI.m_ptrGlobals)) {
                        game_dispose(this) // will crash if haveEnv fails
                    } else {
                        Log.d(TAG, "release(): no ENV!!! (this=%H, rowid=%d)",
                              this, rowid)
                        Assert.failDbg() // seen on Play Store console; and now!!
                    }
                    m_ptrGame = 0
                }
            } else {
                Assert.assertTrue(m_refCount > 0 || !BuildConfig.DEBUG)
            }
        }

        override fun close() {
            release()
        }

        @Throws(Throwable::class)
        fun finalize() {
            if (BuildConfig.NON_RELEASE && (0 != m_refCount || 0L != m_ptrGame)) {
                org.eehouse.android.xw4.Log.e(
                    TAG, "finalize(): called prematurely: refCount: %d"
                            + "; ptr: %d; creator: %s", m_refCount, m_ptrGame, mStack
                )
            }
        }
    }

    private var m_ptrGlobals: Long

    class TopicsAndPackets {
        @JvmField
        var topics: Array<String>? = null
        @JvmField
        var packets: Array<ByteArray>? = null
    // default constructor is called from JNI world, so don't add another!
    }

    @Throws(Throwable::class)
    fun finalize() {
        cleanGlobals(m_ptrGlobals)
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

    enum class XP_Key {
        XP_KEY_NONE,
        XP_CURSOR_KEY_DOWN,
        XP_CURSOR_KEY_ALTDOWN,
        XP_CURSOR_KEY_RIGHT,
        XP_CURSOR_KEY_ALTRIGHT,
        XP_CURSOR_KEY_UP,
        XP_CURSOR_KEY_ALTUP,
        XP_CURSOR_KEY_LEFT,
        XP_CURSOR_KEY_ALTLEFT,
        XP_CURSOR_KEY_DEL,
        XP_RAISEFOCUS_KEY,
        XP_RETURN_KEY,
        XP_KEY_LAST
    }

    enum class SMS_CMD {
        NONE,
        INVITE,
        DATA,
        DEATH,
        ACK_INVITE
    }

    class SMSProtoMsg {
        @JvmField
        var cmd: SMS_CMD? = null
        @JvmField
        var gameID = 0
        @JvmField
        var data // other cases
                : ByteArray? = null
    }

    // Dicts
    class DictWrapper {
        var dictPtr: Long
            private set

        constructor() {
            dictPtr = 0L
        }

        constructor(dictPtr: Long) {
            this.dictPtr = dictPtr
            dict_ref(dictPtr)
        }

        fun release() {
            if (0L != dictPtr) {
                dict_unref(dictPtr)
                dictPtr = 0
            }
        }

        // @Override
        @Throws(Throwable::class)
        fun finalize() {
            release()
        }
    }

    init {
        var seed = Utils.nextRandomInt().toLong()
        seed = seed shl 32
        seed = seed or Utils.nextRandomInt().toLong()
        seed = seed xor System.currentTimeMillis()
        m_ptrGlobals = globalsInit(DUtilCtxt(), JNIUtilsImpl.get(), seed)
    }

    class PatDesc : Serializable {
        @JvmField
        var strPat: String? = null
        @JvmField
        var tilePat: ByteArray? = null
        @JvmField
        var anyOrderOk = false
        override fun toString(): String {
            return String.format(
                "{str: %s; nTiles: %d; anyOrderOk: %b}",
                strPat, tilePat?.size ?: 0, anyOrderOk )
        }
    }

    class IterWrapper(private val ref: Long) {

        @Throws(Throwable::class)
        fun finalize() {
            di_destroy(ref)
        }

		fun getRef(): Long = ref

    }

    interface DictIterProcs {
        fun onIterReady(iterRef: IterWrapper?)
    }

    companion object {
        private val TAG = XwJNI::class.java.getSimpleName()
        private var s_JNI: XwJNI? = null

        @get:Synchronized
        private val jNI: XwJNI
            private get() {
                if (null == s_JNI) {
                    s_JNI = XwJNI()
                }
                return s_JNI!!
            }

        @JvmStatic
        fun cleanGlobalsEmu() {
            cleanGlobals()
        }

        @JvmStatic
        fun dvc_getMQTTDevID(): String {
            return dvc_getMQTTDevID(jNI.m_ptrGlobals)
        }

        @JvmStatic
        fun dvc_setMQTTDevID(newID: String): Boolean {
            return dvc_setMQTTDevID(jNI.m_ptrGlobals, newID)
        }

        fun dvc_resetMQTTDevID() {
            dvc_resetMQTTDevID(jNI.m_ptrGlobals)
        }

        @JvmStatic
        fun dvc_getMQTTSubTopics(): Array<String> {
            return dvc_getMQTTSubTopics(jNI.m_ptrGlobals)
        }

        @JvmStatic
        fun dvc_makeMQTTNukeInvite(nli: NetLaunchInfo): TopicsAndPackets {
            return dvc_makeMQTTNukeInvite(jNI.m_ptrGlobals, nli)
        }

        @JvmStatic
        fun dvc_makeMQTTNoSuchGames(addressee: String, gameID: Int): TopicsAndPackets {
            Log.d(TAG, "dvc_makeMQTTNoSuchGames(to: %s, gameID: %X)",
                  addressee, gameID)
            // DbgUtils.printStack( TAG );
            return dvc_makeMQTTNoSuchGames(jNI.m_ptrGlobals, addressee, gameID)
        }

        @JvmStatic
        fun dvc_parseMQTTPacket(topic: String, buf: ByteArray) {
            dvc_parseMQTTPacket(jNI.m_ptrGlobals, topic, buf)
        }

        @JvmStatic
        fun dvc_onWebSendResult(
            resultKey: Int, succeeded: Boolean,
            result: String?
        ) {
            dvc_onWebSendResult(jNI.m_ptrGlobals, resultKey, succeeded, result)
        }

        fun dvc_getLegalPhonyCodes(): Array<ISOCode> {
            val codes = ArrayList<String>()
            dvc_getLegalPhonyCodes(jNI.m_ptrGlobals, codes)

			val result = ArrayList<ISOCode>()
			for ( code in codes ) {
				result.add( ISOCode(code) )
			}
            return result.toTypedArray()
        }

        fun dvc_getLegalPhoniesFor(code: ISOCode): Array<String> {
            val list = ArrayList<String>()
            dvc_getLegalPhoniesFor(jNI.m_ptrGlobals, code.toString(), list)
            return list.toTypedArray<String>()
        }

        fun dvc_clearLegalPhony(code: ISOCode, phony: String) {
            dvc_clearLegalPhony(jNI.m_ptrGlobals, code.toString(), phony)
        }

        @JvmStatic
        fun hasKnownPlayers(): Boolean {
            val players = kplr_getPlayers()
            return null != players && 0 < players.size
        }

        @JvmStatic
        @JvmOverloads
        fun kplr_getPlayers(byDate: Boolean = false): Array<String>? {
            var result: Array<String>? = null
            if (BuildConfig.HAVE_KNOWN_PLAYERS) {
                result = kplr_getPlayers(jNI.m_ptrGlobals, byDate)
            }
            return result
        }

        @JvmStatic
        fun kplr_renamePlayer(oldName: String, newName: String): Boolean {
            return if (BuildConfig.HAVE_KNOWN_PLAYERS) kplr_renamePlayer(
                jNI.m_ptrGlobals, oldName, newName
            ) else true
        }

        @JvmStatic
        fun kplr_deletePlayer(player: String) {
            if (BuildConfig.HAVE_KNOWN_PLAYERS) {
                kplr_deletePlayer(jNI.m_ptrGlobals, player)
            }
        }

        @JvmStatic
        @JvmOverloads
        fun kplr_getAddr(name: String, lastMod: IntArray? = null): CommsAddrRec? {
            return if (BuildConfig.HAVE_KNOWN_PLAYERS) kplr_getAddr(
                jNI.m_ptrGlobals, name, lastMod
            ) else null
        }

        @JvmStatic
        fun kplr_nameForMqttDev(mqttID: String?): String? {
            return if (BuildConfig.HAVE_KNOWN_PLAYERS) kplr_nameForMqttDev(
                jNI.m_ptrGlobals, mqttID
            ) else null
        }

        private fun cleanGlobals() {
            synchronized(XwJNI::class.java) {
                // let's be safe here
                val jni = jNI
                cleanGlobals(jni!!.m_ptrGlobals) // tests for 0
                jni.m_ptrGlobals = 0
            }
        }

        // This needs to be called before the first attempt to use the
        // jni.
        init {
            System.loadLibrary(BuildConfig.JNI_LIB_NAME)
        }

        /* XW_TrayVisState enum */
        const val TRAY_HIDDEN = 0
        const val TRAY_REVERSED = 1
        const val TRAY_REVEALED = 2

        // Methods not part of the common interface but necessitated by
        // how java/jni work (or perhaps my limited understanding of it.)
        // callback into jni from java when timer set here fires.
        @JvmStatic
        external fun timerFired(
            gamePtr: GamePtr?, why: Int,
            `when`: Int, handle: Int
        ): Boolean

        @JvmStatic
        fun giFromStream(gi: CurGameInfo, stream: ByteArray) {
            Assert.assertNotNull(stream)
            gi_from_stream(jNI.m_ptrGlobals, gi, stream) // called here
        }

        @JvmStatic
        fun nliToStream(nli: NetLaunchInfo): ByteArray {
            return nli_to_stream(jNI.m_ptrGlobals, nli)
        }

        @JvmStatic
        fun nliFromStream(stream: ByteArray): NetLaunchInfo? {
            return nli_from_stream(jNI.m_ptrGlobals, stream)
        }

        @JvmStatic
        fun lcToLocaleJ(lc: Int): ISOCode {
            val code = lcToLocale(lc)
            return ISOCode.newIf(code)
        }

        fun haveLocaleToLc(isoCode: ISOCode, lc: IntArray?): Boolean {
            return haveLocaleToLc(isoCode.toString(), lc)
        }

        @JvmStatic
        external fun comms_getUUID(): String
        @JvmStatic
        external fun lcToLocale(lc: Int): String?
        @JvmStatic
        external fun haveLocaleToLc(isoCodeStr: String?, lc: IntArray?): Boolean

        // Game methods
        private fun initGameJNI(rowid: Long): GamePtr? {
            val ptr = gameJNIInit(jNI.m_ptrGlobals)
            Assert.assertTrueNR(0L != ptr) // should be impossible
            return if (0L == ptr) null else GamePtr(ptr, rowid)
        }

        @JvmStatic
        @Synchronized
        fun initFromStream(
            rowid: Long, stream: ByteArray, gi: CurGameInfo,
            util: UtilCtxt?, draw: DrawCtx?,
            cp: CommonPrefs, procs: TransportProcs?
        ): GamePtr? {
            var gamePtr = initGameJNI(rowid)
            if (!game_makeFromStream(
                    gamePtr, stream, gi, util, draw,
                    cp, procs
                )
            ) {
                gamePtr!!.release()
                gamePtr = null
            }
            return gamePtr
        }

        @JvmStatic
        @Synchronized
        fun initNew(
            gi: CurGameInfo, selfAddr: CommsAddrRec?, hostAddr: CommsAddrRec?,
            util: UtilCtxt?, draw: DrawCtx?, cp: CommonPrefs, procs: TransportProcs?
        ): GamePtr? {
            // Only standalone doesn't provide self address
            Assert.assertTrueNR(null != selfAddr || gi.serverRole == DeviceRole.SERVER_STANDALONE)
            // Only client should be providing host addr
            Assert.assertTrueNR(null == hostAddr || gi.serverRole == DeviceRole.SERVER_ISCLIENT)
            val gamePtr = initGameJNI(0)
            game_makeNewGame(gamePtr, gi, selfAddr, hostAddr, util, draw, cp, procs)
            return gamePtr
        }

        @JvmStatic
        fun game_makeRematch(
            gamePtr: GamePtr, util: UtilCtxt,
            cp: CommonPrefs, gameName: String,
            newOrder: Array<Int>
        ): GamePtr? {
            val noInts = IntArray(newOrder.size)
            for (ii in newOrder.indices) {
                noInts[ii] = newOrder[ii]
            }
            var gamePtrNew = initGameJNI(0)
            if (!game_makeRematch(gamePtr, gamePtrNew, util, cp, gameName, noInts)) {
                gamePtrNew!!.release()
                gamePtrNew = null
            }
            return gamePtrNew
        }

        @JvmStatic
        fun game_makeFromInvite(
            nli: NetLaunchInfo, util: UtilCtxt,
            selfAddr: CommsAddrRec,
            cp: CommonPrefs, procs: TransportProcs
        ): GamePtr? {
            var gamePtrNew = initGameJNI(0)
            if (!game_makeFromInvite(gamePtrNew, nli, util, selfAddr, cp, procs)) {
                gamePtrNew!!.release()
                gamePtrNew = null
            }
            return gamePtrNew
        }

        // hack to allow cleanup of env owned by thread that doesn't open game
        fun threadDone() {
            envDone(jNI.m_ptrGlobals)
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
            gameName: String, newOrder: IntArray
        ): Boolean

        @JvmStatic
        private external fun game_makeFromInvite(
            gamePtr: GamePtr?, nli: NetLaunchInfo,
            util: UtilCtxt,
            selfAddr: CommsAddrRec,
            cp: CommonPrefs,
            procs: TransportProcs
        ): Boolean

        @JvmStatic
        external fun game_receiveMessage(
            gamePtr: GamePtr?,
            stream: ByteArray?,
            retAddr: CommsAddrRec?
        ): Boolean

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
        @JvmStatic
        external fun game_getState(
            gamePtr: GamePtr?,
            gsi: GameStateInfo?
        )

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
        @JvmStatic
        external fun board_drawSnapshot(
            gamePtr: GamePtr?, draw: DrawCtx?,
            width: Int, height: Int
        )

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
        external fun board_resetEngine(gamePtr: GamePtr?)
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
        @JvmStatic
        external fun model_getPlayersLastScore(
            gamePtr: GamePtr?,
            player: Int
        ): LastMoveInfo

        // Server
        external fun server_reset(gamePtr: GamePtr?)
        @JvmStatic
        external fun server_handleUndo(gamePtr: GamePtr?)
        @JvmStatic
        external fun server_do(gamePtr: GamePtr?): Boolean
        @JvmStatic
        external fun server_tilesPicked(gamePtr: GamePtr?, player: Int, tiles: IntArray?)
        @JvmStatic
        external fun server_countTilesInPool(gamePtr: GamePtr?): Int
        @JvmStatic
        external fun server_formatDictCounts(gamePtr: GamePtr?, nCols: Int): String
        @JvmStatic
        external fun server_getGameIsOver(gamePtr: GamePtr?): Boolean
        @JvmStatic
        external fun server_getGameIsConnected(gamePtr: GamePtr?): Boolean
        @JvmStatic
        external fun server_writeFinalScores(gamePtr: GamePtr?): String

        @JvmStatic
        external fun server_initClientConnection(gamePtr: GamePtr?): Boolean

        fun server_canOfferRematch(gamePtr: GamePtr): BooleanArray {
            val results: BooleanArray = BooleanArray(2)
            server_canOfferRematch(gamePtr, results)
            return results
        }

        @JvmStatic
        external fun server_canOfferRematch(gamePtr: GamePtr, results: BooleanArray)

        fun server_figureOrderKT(gamePtr: GamePtr, ro: RematchOrder): Array<Int> {
            val noInts = server_figureOrder(gamePtr, ro)
            val result = ArrayList<Int>()
            noInts.map{result.add(it)}
            return result.toTypedArray()
        }

        @JvmStatic
        private external fun server_figureOrder(gamePtr: GamePtr,
                                                ro: RematchOrder): IntArray
        @JvmStatic
        external fun server_endGame(gamePtr: GamePtr?)

        // hybrid to save work
        @JvmStatic
        external fun board_server_prefsChanged(
            gamePtr: GamePtr?,
            cp: CommonPrefs?
        ): Boolean

        // Comms
        @JvmStatic
        external fun comms_start(gamePtr: GamePtr?)
        @JvmStatic
        external fun comms_stop(gamePtr: GamePtr?)
        @JvmStatic
        external fun comms_getSelfAddr(gamePtr: GamePtr?): CommsAddrRec
        @JvmStatic
        external fun comms_getHostAddr(gamePtr: GamePtr?): CommsAddrRec?
        @JvmStatic
        external fun comms_getAddrs(gamePtr: GamePtr?): Array<CommsAddrRec?>?
        @JvmStatic
        external fun comms_dropHostAddr(gamePtr: GamePtr?, typ: CommsConnType?)
        @JvmStatic
        external fun comms_setQuashed(gamePtr: GamePtr?, quashed: Boolean): Boolean
        @JvmStatic
        external fun comms_resendAll(
            gamePtr: GamePtr?, force: Boolean,
            filter: CommsConnType?,
            andAck: Boolean
        ): Int

        @JvmStatic
        fun comms_resendAll(
            gamePtr: GamePtr?, force: Boolean,
            andAck: Boolean
        ): Int {
            return comms_resendAll(gamePtr, force, null, andAck)
        }

        @JvmStatic
        external fun comms_countPendingPackets(gamePtr: GamePtr?): Int
        @JvmStatic
        external fun comms_ackAny(gamePtr: GamePtr?)
        @JvmStatic
        external fun comms_isConnected(gamePtr: GamePtr?): Boolean
        @JvmStatic
        external fun comms_getStats(gamePtr: GamePtr?): String?
        @JvmStatic
        external fun comms_addMQTTDevID(
            gamePtr: GamePtr?, channelNo: Int,
            devID: String?
        )

        @JvmStatic
        external fun comms_invite(
            gamePtr: GamePtr?, nli: NetLaunchInfo?,
            destAddr: CommsAddrRec?, sendNow: Boolean
        )

        // Used/defined (in C) for DEBUG only
        @JvmStatic
        external fun comms_setAddrDisabled(
            gamePtr: GamePtr?, typ: CommsConnType?,
            send: Boolean, enabled: Boolean
        )

        @JvmStatic
        external fun comms_getAddrDisabled(
            gamePtr: GamePtr?, typ: CommsConnType?,
            send: Boolean
        ): Boolean

        @JvmStatic
        fun smsproto_prepOutbound(
            cmd: SMS_CMD, gameID: Int, buf: ByteArray?, phone: String,
            port: Int,  /*out*/
            waitSecs: IntArray
        ): Array<ByteArray>? {
            return smsproto_prepOutbound(
                jNI.m_ptrGlobals, cmd, gameID, buf,
                phone, port, waitSecs
            )
        }

        @JvmStatic
        fun smsproto_prepOutbound(phone: String,
                                  port: Int,
                                  waitSecs: IntArray): Array<ByteArray>?
        {
            return smsproto_prepOutbound(SMS_CMD.NONE, 0, null, phone,
                                         port, waitSecs)
        }

        @JvmStatic
        fun smsproto_prepInbound(
            data: ByteArray,
            fromPhone: String, wantPort: Int
        ): Array<SMSProtoMsg>? {
            return smsproto_prepInbound(jNI.m_ptrGlobals, data, fromPhone, wantPort)
        }

        @JvmStatic
        external fun dict_tilesAreSame(dict1: Long, dict2: Long): Boolean
        @JvmStatic
        external fun dict_getChars(dict: Long): Array<String>?
        @JvmStatic
        fun dict_getInfo(
            dict: ByteArray?, name: String?, path: String?,
            check: Boolean
        ): DictInfo {
            val wrapper = makeDict(dict, name, path)
            return dict_getInfo(wrapper, check)
        }

        @JvmStatic
        fun dict_getInfo(dict: DictWrapper, check: Boolean): DictInfo {
            return dict_getInfo(
                jNI.m_ptrGlobals, dict.dictPtr,
                check
            )
        }

        @JvmStatic
        fun dict_getDesc(dict: DictWrapper): String? {
            return dict_getDesc(dict.dictPtr)
        }

        @JvmStatic
        fun dict_tilesToStr(dict: DictWrapper, tiles: ByteArray?,
                            delim: String?): String?
        {
            return dict_tilesToStr(dict.dictPtr, tiles, delim)
        }

        @JvmStatic
        fun dict_strToTiles(dict: DictWrapper, str: String):
            Array<ByteArray>?
        {
            return dict_strToTiles(dict.dictPtr, str)
        }

        @JvmStatic
        fun dict_hasDuplicates(dict: DictWrapper): Boolean {
            return dict_hasDuplicates(dict.dictPtr)
        }

        @JvmStatic
        fun getTilesInfo(dict: DictWrapper): String {
            return dict_getTilesInfo(jNI.m_ptrGlobals, dict.dictPtr)
        }

		@JvmStatic
        external fun dict_getTileValue(dictPtr: Long, tile: Int): Int

        // Dict iterator
        const val MAX_COLS_DICT = 15 // from dictiter.h
        @JvmStatic
        fun makeDict(bytes: ByteArray?, name: String?, path: String?): DictWrapper {
            val dict = dict_make(jNI.m_ptrGlobals, bytes, name, path)
            return DictWrapper(dict)
        }

        @JvmStatic
        fun di_init(
            dict: DictWrapper, pats: Array<PatDesc>,
            minLen: Int, maxLen: Int,
            callback: DictIterProcs
        ) {
            val jniState = jNI.m_ptrGlobals
            val dictPtr = dict.dictPtr
            Thread {
                var wrapper: IterWrapper? = null
                val iterPtr = di_init(
                    jniState, dictPtr, pats,
                    minLen, maxLen
                )
                if (0L != iterPtr) {
                    wrapper = IterWrapper(iterPtr)
                }
                callback.onIterReady(wrapper)
            }.start()
        }

        @JvmStatic
        fun di_wordCount(iw: IterWrapper): Int {
            return di_wordCount(iw.getRef())
        }

        @JvmStatic
        fun di_nthWord(iw: IterWrapper, nn: Int, delim: String?): String? {
            return di_nthWord(iw.getRef(), nn, delim)
        }

        @JvmStatic
        fun di_getMinMax(iw: IterWrapper): IntArray {
            return di_getMinMax(iw.getRef())
        }

        @JvmStatic
        fun di_getPrefixes(iw: IterWrapper): Array<String>? {
            return di_getPrefixes(iw.getRef())
        }

        @JvmStatic
        fun di_getIndices(iw: IterWrapper): IntArray? {
            return di_getIndices(iw.getRef())
        }

		@JvmStatic
        private external fun di_destroy(closure: Long)
		@JvmStatic
        private external fun di_wordCount(closure: Long): Int
		@JvmStatic
        private external fun di_nthWord(closure: Long, nn: Int, delim: String?): String?
		@JvmStatic
        private external fun di_getMinMax(closure: Long): IntArray
		@JvmStatic
        private external fun di_getPrefixes(closure: Long): Array<String>?
		@JvmStatic
        private external fun di_getIndices(closure: Long): IntArray?

        // Private methods -- called only here
		@JvmStatic
        private external fun globalsInit(dutil: DUtilCtxt, jniu: JNIUtils, seed: Long): Long
		@JvmStatic
		private external fun dvc_getMQTTDevID(jniState: Long): String
		@JvmStatic
		private external fun dvc_setMQTTDevID(jniState: Long, newid: String): Boolean
		@JvmStatic
        private external fun dvc_resetMQTTDevID(jniState: Long)
		@JvmStatic
        private external fun dvc_getMQTTSubTopics(jniState: Long): Array<String>
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

		@JvmStatic
        private external fun dvc_parseMQTTPacket(
            jniState: Long, topic: String,
            buf: ByteArray
        )

		@JvmStatic
        private external fun dvc_onWebSendResult(
            jniState: Long, resultKey: Int,
            succeeded: Boolean,
            result: String?
        )

		@JvmStatic
        private external fun dvc_getLegalPhonyCodes(
            jniState: Long,
            list: ArrayList<String>
        )

		@JvmStatic
        private external fun dvc_getLegalPhoniesFor(
            jniState: Long, code: String,
            list: ArrayList<String>
        )

		@JvmStatic
        private external fun dvc_clearLegalPhony(jniState: Long, code: String, phony: String)
		@JvmStatic
        private external fun kplr_getPlayers(jniState: Long, byDate: Boolean): Array<String>?
		@JvmStatic
        private external fun kplr_renamePlayer(
            jniState: Long, oldName: String,
            newName: String
        ): Boolean

		@JvmStatic
        private external fun kplr_deletePlayer(jniState: Long, player: String)
		@JvmStatic
        private external fun kplr_getAddr(
            jniState: Long, name: String,
            lastMod: IntArray?
        ): CommsAddrRec

		@JvmStatic
        external fun kplr_nameForMqttDev(jniState: Long, mqttID: String?): String
		@JvmStatic
        private external fun cleanGlobals(jniState: Long)
		@JvmStatic
        private external fun gi_from_stream(
            jniState: Long, gi: CurGameInfo,
            stream: ByteArray
        )

		@JvmStatic
        private external fun nli_to_stream(jniState: Long, nli: NetLaunchInfo): ByteArray
		@JvmStatic
        private external fun nli_from_stream(jniState: Long, stream: ByteArray): NetLaunchInfo
		@JvmStatic
        private external fun gameJNIInit(jniState: Long): Long
		@JvmStatic
        private external fun envDone(globals: Long)
		@JvmStatic
        private external fun dict_make(
            jniState: Long,
            dict: ByteArray?,
            name: String?,
            path: String?
        ): Long

		@JvmStatic
        private external fun dict_ref(dictPtr: Long)
		@JvmStatic
        private external fun dict_unref(dictPtr: Long)
		@JvmStatic
        private external fun dict_strToTiles(dictPtr: Long,
                                             str: String):
            Array<ByteArray>?
		@JvmStatic
        private external fun dict_tilesToStr(dictPtr: Long, tiles: ByteArray?, delim: String?): String?
		@JvmStatic
        private external fun dict_hasDuplicates(dictPtr: Long): Boolean
		@JvmStatic
        private external fun dict_getTilesInfo(jniState: Long, dictPtr: Long): String
		@JvmStatic
        private external fun dict_getInfo(
            jniState: Long, dictPtr: Long,
            check: Boolean
        ): DictInfo

		@JvmStatic
        private external fun dict_getDesc(dictPtr: Long): String?
		@JvmStatic
        private external fun di_init(
            jniState: Long, dictPtr: Long,
            pats: Array<PatDesc>, minLen: Int, maxLen: Int
        ): Long

		@JvmStatic
        private external fun smsproto_prepOutbound(
            jniState: Long, cmd: SMS_CMD, gameID: Int, buf: ByteArray?,
            phone: String, port: Int,  /*out*/
            waitSecs: IntArray
        ): Array<ByteArray>?

		@JvmStatic
        private external fun smsproto_prepInbound(
            jniState: Long,
            data: ByteArray,
            fromPhone: String,
            wantPort: Int
        ): Array<SMSProtoMsg>?

        // This always returns true on release builds now.
		@JvmStatic
        private external fun haveEnv(jniState: Long): Boolean
    }
}
