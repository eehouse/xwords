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

import android.content.Context
import android.os.Handler
import android.os.Message

import java.util.concurrent.LinkedBlockingQueue

import org.eehouse.android.xw4.Assert
import org.eehouse.android.xw4.BuildConfig
// import org.eehouse.android.xw4.CommsTransport
import org.eehouse.android.xw4.ConnStatusHandler
import org.eehouse.android.xw4.DBUtils
import org.eehouse.android.xw4.DbgUtils
import org.eehouse.android.xw4.DictUtils.DictPairs
import org.eehouse.android.xw4.DictUtils.openDicts
import org.eehouse.android.xw4.DupeModeTimer
import org.eehouse.android.xw4.GameLock
import org.eehouse.android.xw4.GameUtils
import org.eehouse.android.xw4.Log
import org.eehouse.android.xw4.R
import org.eehouse.android.xw4.XWPrefs
import org.eehouse.android.xw4.jni.CommonPrefs
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole
import org.eehouse.android.xw4.jni.TransportProcs.TPMsgHandler
import org.eehouse.android.xw4.jni.XwJNI.GamePtr

class JNIThread private constructor(lockIn: GameLock) : Thread(), AutoCloseable {
    private var m_lock: GameLock? = lockIn.retain()
    private val m_rowid = lockIn.rowid
    private val m_queue = LinkedBlockingQueue<QueueElem>()

    enum class JNICmd {
        CMD_NONE,

        // CMD_RUN,
        CMD_DRAW,
        CMD_SETDRAW,
        CMD_INVALALL,
        CMD_LAYOUT,
        CMD_START,
        CMD_SAVE,
        CMD_DO,
        CMD_RECEIVE,

        // can I remove this? What if ordinal of enum's being saved somewhere
        _CMD_TRANSFAIL,
        CMD_PREFS_CHANGE,
        CMD_PEN_DOWN,
        CMD_PEN_MOVE,
        CMD_PEN_UP,
        CMD_KEYDOWN,
        CMD_KEYUP,
        CMD_TIMER_FIRED,
        CMD_COMMIT,
        CMD_TILES_PICKED,
        CMD_JUGGLE,
        CMD_FLIP,
        CMD_TOGGLE_TRAY,
        CMD_TRADE,
        CMD_CANCELTRADE,
        CMD_UNDO_CUR,
        CMD_UNDO_LAST,
        CMD_ZOOM,
        CMD_PREV_HINT,
        CMD_NEXT_HINT,
        CMD_COUNTS_VALUES,
        CMD_REMAINING,
        CMD_RESEND,

        // CMD_ACKANY,
        CMD_HISTORY,
        CMD_FINAL,
        CMD_ENDGAME,
        CMD_POST_OVER,
        CMD_SENDCHAT,
        CMD_NETSTATS,
        CMD_PASS_PASSWD,
        CMD_SET_BLANK,
        CMD_SETMQTTID,
        CMD_PAUSE,
        CMD_UNPAUSE,
        CMD_UNQUASH,  // CMD_DRAW_CONNS_STATUS,
        // CMD_DRAW_BT_STATUS,
        // CMD_DRAW_SMS_STATUS,
    }

    // private val mGsi = GameStateInfo()

    private var mStopped = false
    private var mSaveOnStop = false
    private var mJNIGamePtr: GamePtr? = null
    // You aren't meant to call wait() or notify() on Threads. So I'm creating
    // an object solely to be used for synchronization
    private var mSyncObj = Object()
    private var mLastSavedState = 0
    private var mContext: Context? = null
    private var mGi: CurGameInfo? = null
    private var mHandler: Handler? = null
    private var mNewDict: String? = null
    private var mRefCount = 0
    // private var mXport: CommsTransport? = null
    private var mSummary: GameSummary? = null

    private inner class QueueElem(
        val m_cmd: JNICmd,
        val m_isUIEvent: Boolean,
        val m_args: Array<Any>
    ) {
        override fun toString(): String
        {
            return "QueueElem{cmd: $m_cmd, args: ${DbgUtils.fmtAny(m_args)}}"
        }
    }

    // fun configure(
    //     context: Context, utils: UtilCtxtImpl?,
    //     xportHandler: TPMsgHandler, handler: Handler
    // ): Boolean {
    //     mContext = context
    //     mHandler = handler

    //     // If this isn't true then the queue has to be allowed to empty,
    //     // working on the old game state, before we can re-use any of this.
    //     if (0 < m_queue.size) {
    //         if (BuildConfig.DEBUG) {
    //             val iter: Iterator<QueueElem> = m_queue.iterator()
    //             while (iter.hasNext()) {
    //                 Log.i(
    //                     TAG, "removing %s from queue",
    //                     iter.next().m_cmd.toString()
    //                 )
    //             }
    //         }
    //         m_queue.clear()
    //     }

    //     var success = false
    //     var pairs: DictPairs? = null
    //     val dictNames = null // GameUtils.dictNames(context, m_lock!!)
    //     if (null != dictNames) {
    //         pairs = openDicts(context, dictNames)
    //         success = !pairs.anyMissing(dictNames)
    //     }

    //     if (success) {
    //         val lock = m_lock!!
    //         val stream = GameUtils.savedGame(context, lock)!!
    //         mGi = CurGameInfo(context)
    //         mGi!!.name = DBUtils.getName(context, m_rowid)
    //         XwJNI.giFromStream(mGi!!, stream!!)

    //         mSummary = DBUtils.getSummary(context, lock)

    //         // if (mGi!!.deviceRole != DeviceRole.ROLE_STANDALONE) {
    //         //     mXport = CommsTransport(
    //         //         context, xportHandler!!, m_rowid,
    //         //         mGi!!.deviceRole
    //         //     )
    //         // }

    //         val cp = CommonPrefs.get(context)

    //         // Assert.assertNull( m_jniGamePtr ); // fired!!
    //         mJNIGamePtr?.let {
    //             Log.d(TAG, "configure(): m_jniGamePtr not null; that ok?")
    //             it.release()
    //         }

    //         synchronized(mSyncObj) {
    //             mJNIGamePtr = XwJNI.initFromStream(
    //                 m_rowid, stream, mGi!!,
    //                 utils, null, cp, mXport
    //             )

    //             if (null == mJNIGamePtr) {
    //                 // Quarantine.markBad(m_rowid)
    //                 success = false
    //             } else {
    //                 mSyncObj.notifyAll()

    //                 mLastSavedState = stream.contentHashCode()
    //                 DupeModeTimer.gameOpened(context, m_rowid)
    //             }
    //         }
    //     }
    //     Log.d(TAG, "configure() => %b", success)
    //     return success
    // } // configure()

    fun getGamePtr() : GamePtr? { return mJNIGamePtr }
    fun getGI(): CurGameInfo { return mGi!! }
    fun getSummary(): GameSummary { return mSummary!! }
    fun getLock(): GameLock { return m_lock!! }

    private fun waitToStop(save: Boolean) {
        synchronized(mSyncObj) {
            mStopped = true
            mSaveOnStop = save
        }
        handle(JNICmd.CMD_NONE) // tickle it
        try {
            // Can't pass timeout to join.  There's no way to kill
            // this thread unless it's doing something interruptable
            // (like blocking on a socket) so might as well let it
            // take however log it takes.  If that's too long, fix it.
            interrupt()
            Log.d(TAG, "trying to join; currently handling $mCurElem")
            join()              // getting ANRs here when this doesn't return
            Log.d(TAG, "join() done")
            // Assert.assertFalse( isAlive() );
        } catch (ie: InterruptedException) {
            Log.ex(TAG, ie)
        }

        unlockOnce()
    }

    private fun unlockOnce() {
        synchronized(mSyncObj) {
            m_lock?.let {
                it.release()
                m_lock = null
            }
        }
    }

    fun busy(): Boolean {
        var result = false

        // Docs: The returned iterator is a "weakly consistent" iterator that
        // will never throw ConcurrentModificationException, and guarantees to
        // traverse elements as they existed upon construction of the
        // iterator, and may (but is not guaranteed to) reflect any
        // modifications subsequent to construction.
        val iter: Iterator<QueueElem> = m_queue.iterator()
        while (iter.hasNext() && !result) {
            result = iter.next().m_isUIEvent
        }

        return result
    }

    // fun getGameStateInfo(): GameStateInfo
    // {
    //     synchronized( mGsi ) {
    //         return GameStateInfo(mGsi)
    //     }
    // }

    // Gross hack.  This is the easiest way to set the dict without
    // rewriting game loading code or running into cross-threading
    // issues.
    fun setSaveDict(newDict: String?) {
        mNewDict = newDict
    }

    // private fun toggleTray(): Boolean {
    //     val draw: Boolean
    //     val state = XwJNI.board_getTrayVisState(mJNIGamePtr)
    //     draw = if (state == XwJNI.TRAY_REVEALED) {
    //         XwJNI.board_hideTray(mJNIGamePtr)
    //     } else {
    //         XwJNI.board_showTray(mJNIGamePtr)
    //     }
    //     return draw
    // }

    private fun sendForDialog(titleArg: Int, text: String?) {
        Message.obtain(mHandler, DIALOG, titleArg, 0, text).sendToTarget()
    }

    private fun doLayout(
        width: Int, height: Int, fontWidth: Int,
        fontHeight: Int
    ) {
        Assert.failDbg()
        // val dims = BoardDims()

        // val squareTiles = XWPrefs.getSquareTiles(mContext!!)
        // XwJNI.board_figureLayout(
        //     mJNIGamePtr, mGi, 0, 0, width, height,
        //     150,  /*scorePct*/200,  /*trayPct*/
        //     width, fontWidth, fontHeight, squareTiles,
        //     dims /* out param */
        // )

        // // Make space for net status icon if appropriate
        // if (mGi!!.deviceRole != DeviceRole.ROLE_STANDALONE) {
        //     val statusWidth = dims.boardWidth / 15
        //     dims.scoreWidth -= statusWidth
        //     val left = dims.scoreLeft + dims.scoreWidth + dims.timerWidth
        //     ConnStatusHandler.setRect(
        //         left, dims.top, left + statusWidth,
        //         dims.top + dims.scoreHt
        //     )
        // } else {
        //     ConnStatusHandler.clearRect()
        // }

        // XwJNI.board_applyLayout(mJNIGamePtr, dims)

        // Message.obtain(mHandler, DIMMS_CHANGED, dims)
        //     .sendToTarget()
    }

    // private fun nextSame(cmd: JNICmd): Boolean {
    //     val nextElem = m_queue.peek()
    //     return null != nextElem && nextElem.m_cmd == cmd
    // }

    // private fun processKeyEvent(
    //     cmd: JNICmd, xpKey: XP_Key,
    //     barr: BooleanArray
    // ): Boolean {
    //     val draw = false
    //     return draw
    // } // processKeyEvent

    // private fun checkButtons() {
    // synchronized(mGsi) {
    //     XwJNI.game_getState(mJNIGamePtr!!, mGsi)
    // }
    // Message.obtain(mHandler, TOOLBAR_STATES).sendToTarget()
    //     }

    // private fun save_jni() {
    //     // If server has any work to do, e.g. clean up after showing a
    //     // remote- or robot-moved dialog, let it do so before saving
    //     // state.  In some cases it'll otherwise drop the move.
    //     XwJNI.server_do(mJNIGamePtr)

    //     // And let it tell the relay (if any) it's leaving
    //     // XwJNI.comms_stop( m_jniGamePtr );
    //     XwJNI.game_getGi(mJNIGamePtr, mGi)
    //     mNewDict?.let {
    //         mGi!!.dictName = it
    //     }
    //     val state = XwJNI.game_saveToStream(mJNIGamePtr, mGi)
    //     val newHash = state.contentHashCode()
    //     val hashesEqual = mLastSavedState == newHash
    //     // PENDING: once certain this is true, stop saving the full array and
    //     // instead save the hash. Also, update it after each save.
    //     if (hashesEqual) {
    //         // Log.d( TAG, "save_jni(): no change in game; can skip saving" );
    //     } else {
    //         val context = mContext!!
    //         val lock = m_lock!!
    //         val summary = GameSummary(mGi!!)
    //         XwJNI.game_summarize(mJNIGamePtr, summary)
    //         DBUtils.saveGame(context, lock, state, false)
    //         DBUtils.saveSummary(context, lock, summary)

    //         // There'd better be no way for saveGame above to fail!
    //         XwJNI.game_saveSucceeded(mJNIGamePtr)
    //         mLastSavedState = newHash

    //         Assert.fail()
    //         // val thumb = GameUtils.takeSnapshot(context, mJNIGamePtr!!, mGi)
    //         // DBUtils.saveThumbnail(context, lock, thumb)
    //     }
    // }

    var m_running: Boolean = false
    fun startOnce() {
        synchronized(mSyncObj) {
            if (!m_running) {
                m_running = true
                start()
            }
        }
    }

    fun setDaemonOnce(`val`: Boolean) {
        if (!m_running) {
            isDaemon = `val`
        }
    }

    private var mCurElem: QueueElem? = null
    private enum class CMD {CONTINUE, BREAK, PROCEED,}
    override fun run() {
        // Log.d(TAG, "run() starting")
        // val barr = BooleanArray(2) // scratch boolean
        // while (true) {
        //     // Ok, so Kotlin has a bug/limitation preventing continue or break
        //     // from inside a synchronized block. So I save a string inside and
        //     // use it after to break or continue.
        //     when (synchronized(mSyncObj) {
        //               if (mStopped) CMD.BREAK
        //               else if (null == mJNIGamePtr) {
        //                   try {
        //                       Log.d(TAG, "run(): waiting on non-null mJNIGamePtr")
        //                       mSyncObj.wait()
        //                       CMD.CONTINUE
        //                   } catch (iex: InterruptedException) {
        //                       Log.d(TAG, "exiting run() on interrupt: %s",
        //                             iex.message)
        //                       CMD.BREAK
        //                   }
        //               } else CMD.PROCEED
        //           } )
        //     {
        //         CMD.CONTINUE -> continue
        //         CMD.BREAK -> break
        //         CMD.PROCEED -> {}
        //         else -> Assert.failDbg()
        //     }

        //     val elem: QueueElem
        //     try {
        //         elem = m_queue.take()
        //         mCurElem = elem
        //     } catch (ie: InterruptedException) {
        //         Log.w(TAG, "interrupted; killing thread")
        //         break
        //     }
        //     var draw = false
        //     val args = elem.m_args
        //     when (elem.m_cmd) {
        //         JNICmd.CMD_SAVE -> {
        //             if (nextSame(JNICmd.CMD_SAVE)) {
        //                 continue
        //             }
        //             // save_jni()
        //         }

        //         JNICmd.CMD_DRAW -> {
        //             if (nextSame(JNICmd.CMD_DRAW)) {
        //                 continue
        //             }
        //             draw = true
        //         }

        //         JNICmd.CMD_SETDRAW -> {
        //             XwJNI.board_setDraw(mJNIGamePtr, args[0] as DrawCtx)
        //             XwJNI.board_invalAll(mJNIGamePtr)
        //         }

        //         JNICmd.CMD_INVALALL -> {
        //             XwJNI.board_invalAll(mJNIGamePtr)
        //             draw = true
        //         }

        //         JNICmd.CMD_LAYOUT -> {
        //             val args0 = args[0]
        //             var dims: BoardDims? = null
        //             if (args0 is BoardDims) {
        //                 dims = args0
        //                 XwJNI.board_applyLayout(mJNIGamePtr, dims)
        //             } else {
        //                 doLayout(
        //                     args0 as Int, args[1] as Int,
        //                     args[2] as Int, args[3] as Int
        //                 )
        //             }
        //             draw = true
        //             // check and disable zoom button at limit
        //             handle(JNICmd.CMD_ZOOM, 0)
        //         }

        //         JNICmd.CMD_START -> draw = tryConnect(mJNIGamePtr!!, mGi!!)
        //         JNICmd.CMD_DO -> {
        //             if (nextSame(JNICmd.CMD_DO)) {
        //                 continue
        //             }
        //             // draw = XwJNI.server_do(mJNIGamePtr)
        //         }

        //         JNICmd.CMD_RECEIVE -> {
        //             val ret = args[1] as CommsAddrRec
        //             Assert.assertNotNull(ret)
        //             Assert.assertNotNull(mJNIGamePtr)
        //             // draw = XwJNI.game_receiveMessage(mJNIGamePtr,
        //             //                                  args[0] as ByteArray, ret)
        //             handle(JNICmd.CMD_DO)
        //             if (draw) {
        //                 handle(JNICmd.CMD_SAVE)
        //             }
        //         }

        //         JNICmd.CMD_PREFS_CHANGE -> {
        //             // need to inval all because some of prefs,
        //             // e.g. colors, aren't known by common code so
        //             // board_prefsChanged's return value isn't enough.
        //             XwJNI.board_invalAll(mJNIGamePtr)
        //             XwJNI.board_server_prefsChanged(mJNIGamePtr,
        //                                             CommonPrefs.get(mContext!!))
        //             draw = true
        //         }

        //         JNICmd.CMD_PEN_DOWN ->
        //             draw = XwJNI.board_handlePenDown( mJNIGamePtr,
        //                                               args[0] as Int,
        //                                               args[1] as Int,
        //                                               barr)

        //         JNICmd.CMD_PEN_MOVE -> {
        //             if (nextSame(JNICmd.CMD_PEN_MOVE)) {
        //                 continue
        //             }
        //             draw = XwJNI.board_handlePenMove(
        //                 mJNIGamePtr, args[0] as Int, args[1] as Int)
        //         }

        //         JNICmd.CMD_PEN_UP ->
        //             draw = XwJNI.board_handlePenUp(
        //                 mJNIGamePtr, args[0] as Int, args[1] as Int)

        //         JNICmd.CMD_KEYDOWN, JNICmd.CMD_KEYUP -> draw =
        //             processKeyEvent(elem.m_cmd, args[0] as XP_Key, barr)

        //         JNICmd.CMD_COMMIT -> {
        //             val phoniesConfirmed =
        //                 if (args.size >= 1) args[0] as Boolean else false
        //             val turnConfirmed =
        //                 if (args.size >= 2) args[1] as Boolean else false
        //             var newTiles: IntArray? = null
        //             var badWordsKey = 0
        //             if (args.size >= 3) {
        //                 val obj = args[2]
        //                 if (obj is Int) {
        //                     badWordsKey = obj
        //                 } else if (obj is IntArray) {
        //                     newTiles = obj
        //                 }
        //             }
        //             draw = XwJNI.board_commitTurn(
        //                 mJNIGamePtr, phoniesConfirmed,
        //                 badWordsKey, turnConfirmed, newTiles
        //             )
        //         }

        //         JNICmd.CMD_TILES_PICKED -> {
        //             val playerNum = args[0] as Int
        //             val tiles = args[1] as IntArray
        //             // XwJNI.server_tilesPicked(mJNIGamePtr, playerNum, tiles)
        //             Assert.failDbg()
        //         }

        //         JNICmd.CMD_JUGGLE -> draw = XwJNI.board_juggleTray(mJNIGamePtr)
        //         JNICmd.CMD_FLIP -> draw = XwJNI.board_flip(mJNIGamePtr)
        //         JNICmd.CMD_TOGGLE_TRAY -> draw = toggleTray()
        //         JNICmd.CMD_TRADE -> draw = XwJNI.board_beginTrade(mJNIGamePtr)
        //         JNICmd.CMD_CANCELTRADE -> draw = XwJNI.board_endTrade(mJNIGamePtr)
        //         JNICmd.CMD_UNDO_CUR -> draw = (XwJNI.board_replaceTiles(mJNIGamePtr)
        //                 || XwJNI.board_redoReplacedTiles(mJNIGamePtr))

        //         JNICmd.CMD_UNDO_LAST -> {
        //             // XwJNI.server_handleUndo(mJNIGamePtr)
        //             draw = true
        //         }

        //         JNICmd.CMD_NEXT_HINT, JNICmd.CMD_PREV_HINT -> {
        //             if (nextSame(elem.m_cmd)) {
        //                 continue
        //             }
        //             draw = XwJNI.board_requestHint(
        //                 mJNIGamePtr, false,
        //                 JNICmd.CMD_PREV_HINT == elem.m_cmd,
        //                 barr
        //             )
        //             if (barr[0]) {
        //                 handle(elem.m_cmd)
        //                 draw = false
        //             }
        //         }

        //         JNICmd.CMD_ZOOM -> draw = XwJNI.board_zoom(
        //             mJNIGamePtr,
        //             ((args[0] as Int)),
        //             barr
        //         )

        //         // JNICmd.CMD_COUNTS_VALUES -> sendForDialog(
        //             // (args[0] as Int),
        //             // XwJNI.server_formatDictCounts(mJNIGamePtr, 3)
        //             // )

        //         JNICmd.CMD_REMAINING -> sendForDialog(
        //             (args[0] as Int),
        //             XwJNI.board_formatRemainingTiles(mJNIGamePtr)
        //         )

        //         JNICmd.CMD_RESEND -> {
        //             val nSent =
        //                 XwJNI.comms_resendAll(
        //                     mJNIGamePtr,
        //                     ((args[0] as Boolean)),
        //                     ((args[1] as Boolean))
        //                 )
        //             if ((args[2] as Boolean)) {
        //                 Message.obtain(mHandler, MSGS_SENT, nSent).sendToTarget()
        //             }
        //         }

        //         JNICmd.CMD_HISTORY -> {
        //             val gameOver = XwJNI.server_getGameIsOver(mJNIGamePtr)
        //             sendForDialog(
        //                 (args[0] as Int),
        //                 XwJNI.model_writeGameHistory(
        //                     mJNIGamePtr,
        //                     gameOver
        //                 )
        //             )
        //         }

        //         JNICmd.CMD_FINAL ->
        //             if (XwJNI.server_getGameIsOver(mJNIGamePtr)) {
        //                 handle(JNICmd.CMD_POST_OVER)
        //             } else {
        //                 Message.obtain(mHandler, QUERY_ENDGAME).sendToTarget()
        //             }

        //         JNICmd.CMD_ENDGAME -> {
        //             XwJNI.server_endGame(mJNIGamePtr)
        //             draw = true
        //         }

        //         JNICmd.CMD_POST_OVER -> if (XwJNI.server_getGameIsOver(mJNIGamePtr)) {
        //             val auto = 0 < args.size &&
        //                     (args[0] as Boolean)
        //             val titleID = if (auto) R.string.summary_gameover
        //             else R.string.finalscores_title

        //             val text = XwJNI.server_writeFinalScores(mJNIGamePtr)
        //             Message.obtain(mHandler, GAME_OVER, titleID, 0, text)
        //                 .sendToTarget()
        //         }

        //         JNICmd.CMD_SENDCHAT -> XwJNI.board_sendChat(mJNIGamePtr, args[0] as String)
        //         JNICmd.CMD_NETSTATS -> sendForDialog(
        //             (args[0] as Int),
        //             XwJNI.comms_getStats(mJNIGamePtr)
        //         )

        //         JNICmd.CMD_PASS_PASSWD -> {
        //             val player = (args[0] as Int)
        //             val pwd = args[1] as String
        //             draw = XwJNI.board_passwordProvided(mJNIGamePtr, player, pwd)
        //         }

        //         JNICmd.CMD_SET_BLANK -> draw = XwJNI.board_setBlankValue(
        //             mJNIGamePtr,
        //             ((args[0] as Int)),
        //             ((args[1] as Int)),
        //             ((args[2] as Int)),
        //             ((args[3] as Int))
        //         )

        //         JNICmd.CMD_SETMQTTID -> {
        //             draw = false
        //             XwJNI.comms_addMQTTDevID(mJNIGamePtr, (args[0] as Int), args[1] as String)
        //         }

        //         JNICmd.CMD_TIMER_FIRED ->
        //             Assert.failDbg()
        //         // draw = XwJNI.timerFired(mJNIGamePtr, args[0] as Int,
        //         // args[1] as Int, args[2] as Int)

        //         JNICmd.CMD_PAUSE -> XwJNI.board_pause(mJNIGamePtr, (args[0] as String))
        //         JNICmd.CMD_UNPAUSE -> XwJNI.board_unpause(mJNIGamePtr, (args[0] as String))
        //         JNICmd.CMD_UNQUASH -> XwJNI.comms_setQuashed(mJNIGamePtr, false)
        //         JNICmd.CMD_NONE -> {}
        //         else -> {
        //             Log.w(TAG, "dropping cmd: %s", elem.m_cmd.toString())
        //             Assert.failDbg()
        //         }
        //     }
        //     if (draw) {
        //         // do the drawing in this thread but in BoardView
        //         // where it can be synchronized with that class's use
        //         // of the same bitmap for blitting.
        //         Message.obtain(mHandler, DO_DRAW).sendToTarget()
        //         checkButtons()
        //     }
        //     mCurElem = null
        // }

        // mJNIGamePtr?.let {
        //     if (mSaveOnStop) {
        //         // XwJNI.comms_stop(it)
        //         // save_jni()
        //     } else {
        //         Log.w(TAG, "run(): exiting without saving")
        //     }
        //     it.release()
        //     mJNIGamePtr = null
        // }

        // unlockOnce()
        // Log.d(TAG, "run() finished")
    } // run

    @Throws(Throwable::class)
    fun finalize() {
        Assert.assertTrue(null == m_lock || !BuildConfig.DEBUG)
    }

    fun handleBkgrnd(cmd: JNICmd, vararg args: Any) {
        // DbgUtils.logf( "adding: %s", cmd.toString() );
        m_queue.add(QueueElem(cmd, false, arrayOf(*args)))
    }

    fun receive(msg: ByteArray, addr: CommsAddrRec): JNIThread {
        handle(JNICmd.CMD_RECEIVE, msg, addr)
        return this
    }

    fun sendChat(chat: String) {
        handle(JNICmd.CMD_SENDCHAT, chat)
    }

    fun notifyPause(pauser: Int, isPause: Boolean, msg: String?) {
        Message.obtain(mHandler, GOT_PAUSE, msg)
            .sendToTarget()
    }

    fun handle(cmd: JNICmd, vararg args: Any) {
        if (mStopped && JNICmd.CMD_NONE != cmd) {
            Log.w(TAG, "handle(%s): NOT adding to stopped thread!!!", cmd)
            // DbgUtils.printStack( TAG );
        } else {
            m_queue.add(QueueElem(cmd, true, arrayOf(*args)))
        }
    }
    private fun retain_sync() {
        ++mRefCount
        // Log.i( TAG, "retain_sync(rowid=%d): mRefCount raised to %d",
        //        m_rowid, mRefCount );
    }

    fun retain(): JNIThread {
        synchronized(s_instances) {
            retain_sync()
        }
        return this
    }

    @JvmOverloads
    fun release(save: Boolean = true) {
        var stop = false
        synchronized(s_instances) {
            if (0 == --mRefCount) {
                s_instances.remove(m_rowid)
                stop = true
            }
        }

        // Log.i( TAG, "release(rowid=%d): mRefCount dropped to %d",
        //        m_rowid, mRefCount );
        if (stop) {
            waitToStop(true)
            DupeModeTimer.gameClosed(m_rowid)
        } else if (save && 0 != mLastSavedState) { // has configure() run?
            handle(JNICmd.CMD_SAVE) // in case releaser has made changes
        }
    }

    override fun close() {
        release()
    }

    companion object {
        private val TAG: String = JNIThread::class.java.simpleName

        const val RUNNING: Int = 1
        const val DIALOG: Int = 2
        const val QUERY_ENDGAME: Int = 3
        const val TOOLBAR_STATES: Int = 4
        const val GOT_PAUSE: Int = 5
        const val GAME_OVER: Int = 6
        const val MSGS_SENT: Int = 7
        const val DO_DRAW: Int = 8
        const val DIMMS_CHANGED: Int = 9

        fun tryConnect(gamePtr: GamePtr, gi: CurGameInfo): Boolean {
            Log.d(TAG, "tryConnect(rowid=%d)", gamePtr.rowid)
            // XwJNI.comms_start(gamePtr)
            if (gi.deviceRole == DeviceRole.ROLE_ISGUEST) {
                // XwJNI.server_initClientConnection(gamePtr)
            }
            return false // XwJNI.server_do(gamePtr)
        }

        private val s_instances: MutableMap<Long, JNIThread> = HashMap()
        fun getRetained(rowid: Long): JNIThread? {
            return getRetained(rowid, null)
        }

        fun getRetained(lock: GameLock): JNIThread? {
            return getRetained(lock.rowid, lock)
        }

        private fun getRetained(rowid: Long, lock: GameLock?): JNIThread? {
            var result: JNIThread? = null
            synchronized(s_instances) {
                result = s_instances[rowid]
                if (null == result && null != lock) {
                    result = JNIThread(lock)
                    s_instances[rowid] = result!!
                }
                if (null != result) {
                    result!!.retain_sync()
                }
            }
            return result
        }

        fun gameIsOpen(rowid: Long): Boolean {
            var result = false
            getRetained(rowid).use { thread ->
                result = null != thread
            }
            Log.d(TAG, "gameIsOpen(%d) => %b", rowid, result)
            return result
        }
    }
}
