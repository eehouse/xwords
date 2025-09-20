/*
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
import android.content.Intent
import android.graphics.Bitmap
import android.os.Build
import android.telephony.PhoneNumberUtils

import kotlin.concurrent.thread
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

import org.json.JSONException
import org.json.JSONObject
import java.lang.ref.WeakReference

import org.eehouse.android.xw4.Assert
import org.eehouse.android.xw4.BTUtils
import org.eehouse.android.xw4.BoardDelegate
import org.eehouse.android.xw4.BuildConfig
import org.eehouse.android.xw4.Channels
import org.eehouse.android.xw4.DBUtils
import org.eehouse.android.xw4.DictUtils
import org.eehouse.android.xw4.DupeModeTimer
import org.eehouse.android.xw4.GameUtils
import org.eehouse.android.xw4.GamesListDelegate
import org.eehouse.android.xw4.Log
import org.eehouse.android.xw4.MQTTUtils
import org.eehouse.android.xw4.NBSProto
import org.eehouse.android.xw4.NetLaunchInfo
import org.eehouse.android.xw4.NetUtils
import org.eehouse.android.xw4.R
import org.eehouse.android.xw4.ThumbCanvas
import org.eehouse.android.xw4.Utils
import org.eehouse.android.xw4.XWApp
import org.eehouse.android.xw4.jni.GameMgr.GroupRef
import org.eehouse.android.xw4.loc.LocUtils
import org.eehouse.android.xw4.putAnd

class DUtilCtxt() {

    interface Listeners {
        fun onKnownPlayersChange() {}
        fun onGameChanged(gr: GameRef, flags: GameChangeEvents) {}
        fun onDictRemoved(gr: GameRef, name: String) {}
        fun missingDictAdded(gr: GameRef, name: String) {}
    }

    // No idea why DrawCtx doesn't work here. Code in drawwrapper.c can't find
    // the method.
    fun getThumbDraw(nCols: Int): /* DrawCtx? */ Any? {
        val context = XWApp.getContext()
        val size = GameUtils.getThumbSize(context, nCols)
        val thumb = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888)
        return ThumbCanvas(context, thumb, size)
    }
    
    private val mContext: Context
    fun getUserString(stringCode: Int): String {
        val id = when (stringCode) {
            STR_ROBOT_MOVED -> R.string.str_robot_moved_fmt
            STRS_VALUES_HEADER -> R.string.strs_values_header_fmt
            STRD_REMAINING_TILES_ADD -> R.string.strd_remaining_tiles_add_fmt
            STRD_UNUSED_TILES_SUB -> R.string.strd_unused_tiles_sub_fmt
            STRS_REMOTE_MOVED -> R.string.str_remote_moved_fmt
            STRD_TIME_PENALTY_SUB -> R.string.strd_time_penalty_sub_fmt
            STR_PASS -> R.string.str_pass
            STRS_MOVE_ACROSS -> R.string.strs_move_across_fmt
            STRS_MOVE_DOWN -> R.string.strs_move_down_fmt
            STRS_TRAY_AT_START -> R.string.strs_tray_at_start_fmt
            STRSS_TRADED_FOR -> R.string.strss_traded_for_fmt
            STR_PHONY_REJECTED -> R.string.str_phony_rejected
            STRD_CUMULATIVE_SCORE -> R.string.strd_cumulative_score_fmt
            STRS_NEW_TILES -> R.string.strs_new_tiles_fmt
            STR_COMMIT_CONFIRM -> R.string.str_commit_confirm
            STR_SUBMIT_CONFIRM -> R.string.str_submit_confirm
            STR_BONUS_ALL -> R.string.str_bonus_all
            STR_BONUS_ALL_SUB -> R.string.str_bonus_all_fmt
            STRD_TURN_SCORE -> R.string.strd_turn_score_fmt
            STRSD_RESIGNED -> R.string.str_resigned_fmt
            STRSD_WINNER -> R.string.str_winner_fmt
            STRDSD_PLACER -> R.string.str_placer_fmt
            STR_DUP_CLIENT_SENT -> R.string.dup_client_sent
            STRDD_DUP_HOST_RECEIVED -> R.string.dup_host_received_fmt
            STR_DUP_MOVED -> R.string.dup_moved
            STRD_DUP_TRADED -> R.string.dup_traded_fmt
            STRSD_DUP_ONESCORE -> R.string.dup_onescore_fmt
            STRS_DUP_ALLSCORES -> R.string.dup_allscores_fmt
            STRS_GROUPS_DEFAULT -> R.string.group_new_games
            STRS_GROUPS_ARCHIVE -> R.string.group_name_archive
            STR_PENDING_PLAYER -> R.string.missing_player
            else -> { Log.w( TAG, "no such stringCode: %d", stringCode)
					  0
			}
        }
        // Log.d( TAG, "getUserString(%d) => %s", stringCode, result );
        return if (0 == id) "" else LocUtils.getString(mContext, id)
    }

    fun getUserQuantityString(stringCode: Int, quantity: Int): String {
        val pluralsId = when (stringCode) {
            STRD_ROBOT_TRADED -> R.plurals.strd_robot_traded_fmt
            STRD_REMAINS_HEADER -> R.plurals.strd_remains_header_fmt
            STRD_REMAINS_EXPL -> R.plurals.strd_remains_expl_fmt
			else -> 0
        }
        val result =
			if (0 != pluralsId) {
				LocUtils.getQuantityString(mContext, pluralsId, quantity)
			} else {
				""
			}
        return result
    }

    fun phoneNumbersSame(num1: String?, num2: String?): Boolean {
        return PhoneNumberUtils.compare(mContext, num1, num2)
    }

    fun store(key: String, data: ByteArray?) {
        Log.d( TAG, "store(key=%s)", key );
        data?.let {
            DBUtils.setBytesFor(mContext, key, it)
            if (BuildConfig.DEBUG) {
                val tmp = load(key)!!
                if (! tmp.contentEquals(it)) {
                    Log.d(TAG, "store($key): race or bad content?; lens"
                          + " are ${tmp.size} and ${it.size}")
                    Assert.failDbg()
                }
            }
        }
    }

    fun load(key: String): ByteArray? {
        // Log.d( TAG, "load(%s, %s)", key, keySuffix );

        // Log.d( TAG, "load(%s, %s) returning %d bytes", key, keySuffix,
        //        null == result ? 0 : result.length );
        return DBUtils.getBytesFor(mContext, key)
    }

    init {
        mContext = XWApp.getContext()
    }

    private val sCleared = HashSet<Int>()
    fun setTimer(inMS: Int, key: Int)
    {
        val startMS = if (BuildConfig.DEBUG) System.currentTimeMillis() else 0
        synchronized(sCleared) {sCleared.add(key)}

        GlobalScope.launch(Dispatchers.Default) {
            delay(inMS.toLong())
            if (BuildConfig.DEBUG) {
                val wakeMS = System.currentTimeMillis()
                // Log.d(TAG, "setTimer(): firing; set for $inMS, "
                //       + "took ${wakeMS - startMS}")
            }

            if (synchronized(sCleared) {sCleared.remove(key)}) {
                Device.onTimerFired(key)
            }
        }
    }

    fun clearTimer(key: Int)
    {
        Log.d(TAG, "clearTimer($key)")
        synchronized(sCleared) {sCleared.add(key)}
    }

    // PENDING use prefs for this
    fun getUsername(
        posn: Int,
        isLocal: Boolean,
        isRobot: Boolean
    ): String {
        return if (isRobot) {
			CommonPrefs.getDefaultRobotName(mContext)
		} else{
			CommonPrefs.getDefaultPlayerName( mContext, posn )
		}
    }

    fun getSelfAddr(): CommsAddrRec {
        Log.d(TAG, "getSelfAddr()")
        val result = CommsAddrRec.getSelfAddr(mContext)
        return result
    }

    // A pause can come in when a game's open or when it's not. If it's open,
    // we want to post an alert. If it's not, we want to post a notification,
    // or at least kick off DupeModeTimer to cancel or start the timer-running
    // notification.
    fun notifyPause(
        gr: Long, pauseType: Int, pauser: Int,
        pauserName: String, expl: String?
    ) {
        Assert.failDbg()
        // val rowids: LongArray = DBUtils.getRowIDsFor(mContext, gameID)
        // // Log.d( TAG, "got %d games with gameid", rowids.length );
        // val isPause = UNPAUSED != pauseType
        // for (rowid in rowids) {
        //     val msg = msgForPause(rowid, pauseType, pauserName, expl)
        //     JNIThread.getRetained(rowid).use { thread ->
        //         if (null != thread) {
        //             thread.notifyPause(pauser, isPause, msg)
        //         } else {
        //             val intent: Intent = GamesListDelegate
        //                 .makeRowidIntent(mContext, rowid)
        //             val titleID: Int =
        //                 if (isPause) R.string.game_paused_title else R.string.game_unpaused_title
        //             val channelID = Channels.ID.DUP_PAUSED
        //             Utils.postNotification(
        //                 mContext, intent, titleID, msg,
        //                 rowid, channelID
        //             )

        //             // DupeModeTimer.timerPauseChanged( mContext, rowid );
        //         }
        //     }
        // }
    }

    fun informMove(grval: Long, turn: Int, expl: String, words: String?) {
        val gr = GameRef(grval)
        BoardDelegate.getIfOpen(gr)?.informMove(turn, expl, words)
            ?: run {
                val bmr = GameUtils.BackMoveResult()
                Utils.launch {
                    bmr.m_lmi = gr.getPlayersLastScore(turn)
                    val gi = gr.getGI()!!
                    val locals = gi.playersLocal()
                    GameUtils.postMoveNotification(
                        mContext, gr,
                        bmr, locals[turn],
                        gi.gameName!!
                    )
                }
            }
    }

    fun notifyGameOver(grval: Long) {
        val gr = GameRef(grval)
        BoardDelegate.getIfOpen(gr)?.notifyGameOver()
            ?: run {
                Utils.showToast(mContext, "game over but not open")
            }
    }

    // PENDING: channel is ignored here, meaning there can't be two ends of a
    // game in the same app.
    fun haveGame(gameID: Int, channel: Int): Boolean {
        return GameUtils.haveWithGameID(mContext, gameID, channel)
    }

    private fun msgForPause(
        rowid: Long,
        pauseType: Int,
        pauserName: String,
        expl: String?
    ): String {
        val msg: String
        val gameName = GameUtils.getName(mContext, rowid)
        msg = if (AUTOPAUSED == pauseType) {
            LocUtils.getString(
                mContext, R.string.autopause_expl_fmt,
                gameName
            )
        } else {
            val isPause = PAUSED == pauseType
            if (null != expl && 0 < expl.length) {
                LocUtils.getString(
                    mContext,
                    if (isPause) R.string.pause_notify_expl_fmt else R.string.unpause_notify_expl_fmt,
                    pauserName, expl
                )
            } else {
                LocUtils.getString(
                    mContext,
                    if (isPause) R.string.pause_notify_fmt else R.string.unpause_notify_fmt,
                    pauserName
                )
            }
        }
        return msg
    }

    fun getDictPath(name: String, path: Array<String?>, bytes: Array<ByteArray?>) {
        Log.d(TAG, "getDictPath(name='%s')", name)
        val pairs: DictUtils.DictPairs = DictUtils.openDicts(mContext, arrayOf(name as String?))
        // Log.d( TAG, "openDicts() => %s", pairs );
        path[0] = pairs.m_paths.get(0)
        bytes[0] = pairs.m_bytes.get(0)
        // Log.d( TAG, "getDictPath(%s): have path: %s; bytes: %s", name, path[0], bytes[0] );
    }

    fun onDupTimerChanged(gr: Long, oldVal: Int, newVal: Int) {
        DupeModeTimer.timerChanged(mContext, GameRef(gr), newVal)
    }

    fun sendViaWeb(resultKey: Int, api: String?, jsonParams: String) {
        NetUtils.sendViaWeb(mContext, resultKey, api, jsonParams)
    }

    val regValues: String
        get() {
            val result: String
            result = try {
                val params = JSONObject()
					.putAnd("os", Build.MODEL)
					.putAnd("vers", Build.VERSION.RELEASE)
					.putAnd("versI", Build.VERSION.SDK_INT)
					.putAnd("vrntCode", BuildConfig.VARIANT_CODE)
					.putAnd("vrntName", BuildConfig.VARIANT_NAME)
					.putAnd("loc", LocUtils.getCurLocale(mContext))
                params.toString()
            } catch (je: JSONException) {
                Log.e(TAG, "getRegValues() ex: %s", je)
                "{}"
            }
            return result
        }

    fun dictGone(grval: Long, dictName: String) {
        val gr = GameRef(grval)
        Log.d(TAG, "dictGone($gr, $dictName)")
        pruned().map{ it.onDictRemoved(gr, dictName) }
    }

    fun missingDictAdded(grval: Long, dictName: String) {
        val gr = GameRef(grval)
        Log.d(TAG, "dictGone($gr, $dictName)")
        pruned().map{ it.missingDictAdded(gr, dictName) }
    }

    fun startMQTTListener( devID: String, topics: Array<String>, qos: Int ) {
        MQTTUtils.startListener(mContext, devID, topics, qos);
    }

    fun sendViaMQTT(topic: String, msg: ByteArray, qos: Int) {
        MQTTUtils.send(mContext, topic, msg, qos)
    }

    fun sendViaBT(msg: ByteArray, hostName: String, hostAddr: String?) {
        BTUtils.sendMessage(mContext, msg, hostName, hostAddr)
    }

    fun sendViaNBS(msg: ByteArray, phone: String, port: Int) {
        Log.d(TAG, "sendViaNBS($msg, $phone)")
        NBSProto.sendPacket(mContext, msg, phone, port)
    }

    fun onGameGoneReceived(gameID: Int, from: CommsAddrRec) {
        GameUtils.onGameGone(mContext, gameID)
        Assert.assertTrueNR(from.contains(CommsAddrRec.CommsConnType.COMMS_CONN_MQTT))
        MQTTUtils.handleGameGone(mContext, from, gameID)
    }

    fun onGroupChanged(grpval: Int, flags: Int) {
        val grp = GroupRef(grpval)
        val events = GroupChangeEvents(flags)
        GamesListDelegate.onGroupChanged(mContext, grp, events)
    }

    fun onCtrlReceived(msg: ByteArray) {
        MQTTUtils.handleCtrlReceived(mContext, msg)
    }

    fun removeStored(key: String) {
        Log.d(TAG, "removeStored($key) called")
        DBUtils.delKVPair(mContext, key)
    }

    fun getKeysLike(pattern: String): Array<String> {
        Log.d(TAG, "getKeysLike()")
        return DBUtils.getKeysLike(mContext, pattern)
    }

    fun onKnownPlayersChange() {
        pruned().map{ it.onKnownPlayersChange() }
    }

    fun onGameChanged(grVal: Long, bits: Int) {
        val gr = GameRef(grVal)
        val flags = GameChangeEvents(bits)
        // Log.d(TAG, "onGameChanged($gr, 0x%x)".format(flags))
        pruned().map{ it.onGameChanged(gr, flags) }
    }

    fun getCommonPrefs(): CommonPrefs {
        return CommonPrefs.get(mContext)
    }

    // must match enum GameChangeEvent in dutil.h
    enum class GameChangeEvent(val bit: Int) {
        GCE_PLAYER_JOINED(0x01),
        GCE_CONFIG_CHANGED(0x02),
        GCE_SUMMARY_CHANGED(0x04),
        GCE_TURN_CHANGED(0x08),
        GCE_BOARD_CHANGED(0x10),
        GCE_CHAT_ARRIVED(0x20),
        ;
    }
    class GameChangeEvents(flags: Int): HashSet<GameChangeEvent>() {
        init {
            GameChangeEvent.entries.map {
                if (0 != (it.bit and flags)) add(it)
            }
        }
    }

    // must match enum GroupChangeEvent in dutil.h
    enum class GroupChangeEvent(val bit: Int) {
        GRCE_ADDED(0x01),
        GRCE_DELETED(0x02),
        GRCE_MOVED(0x04),
        GRCE_RENAMED(0x08),
        GRCE_COLLAPSED(0x10),
        GRCE_EXPANDED(0x20),
        GRCE_GAMES_REORDERED(0x40),
        GRCE_GAME_ADDED(0x80),
        GRCE_GAME_REMOVED(0x100),
        ;
    }

    class GroupChangeEvents(flags: Int): HashSet<GroupChangeEvent>() {
        init {
            GroupChangeEvent.entries.map {
                if (0 != (it.bit and flags)) add(it)
            }
        }
    }

    companion object {
        private val TAG = DUtilCtxt::class.java.getSimpleName()
        private const val STRD_ROBOT_TRADED = 1
        private const val STR_ROBOT_MOVED = 2
        private const val STRS_VALUES_HEADER = 3
        private const val STRD_REMAINING_TILES_ADD = 4
        private const val STRD_UNUSED_TILES_SUB = 5
        private const val STRS_REMOTE_MOVED = 6
        private const val STRD_TIME_PENALTY_SUB = 7
        private const val STR_PASS = 8
        private const val STRS_MOVE_ACROSS = 9
        private const val STRS_MOVE_DOWN = 10
        private const val STRS_TRAY_AT_START = 11
        private const val STRSS_TRADED_FOR = 12
        private const val STR_PHONY_REJECTED = 13
        private const val STRD_CUMULATIVE_SCORE = 14
        private const val STRS_NEW_TILES = 15
        private const val STR_COMMIT_CONFIRM = 16
        private const val STR_SUBMIT_CONFIRM = 17
        private const val STR_BONUS_ALL = 18
        private const val STRD_TURN_SCORE = 19
        private const val STRD_REMAINS_HEADER = 20
        private const val STRD_REMAINS_EXPL = 21
        private const val STRSD_RESIGNED = 22
        private const val STRSD_WINNER = 23
        private const val STRDSD_PLACER = 24
        private const val STR_DUP_CLIENT_SENT = 25
        private const val STRDD_DUP_HOST_RECEIVED = 26
        private const val STR_DUP_MOVED = 27
        private const val STRD_DUP_TRADED = 28
        private const val STRSD_DUP_ONESCORE = 29
        private const val STR_PENDING_PLAYER = 30
        private const val STR_BONUS_ALL_SUB = 31
        private const val STRS_DUP_ALLSCORES = 32
        private const val STRS_GROUPS_DEFAULT = 33
        private const val STRS_GROUPS_ARCHIVE = 34

        // Must match enum DupPauseType
        const val UNPAUSED = 0
        const val PAUSED = 1
        const val AUTOPAUSED = 2

        val sListeners: MutableSet<WeakReference<Listeners>> =
            HashSet<WeakReference<Listeners>>()

        fun registerListener(proc: Listeners) {
            synchronized(sListeners) {
                sListeners.add(WeakReference<Listeners>(proc))
            }
        }

        private fun pruned(): List<Listeners> {
            val procs =
                synchronized(sListeners) {
                    val iter = sListeners.iterator()
                    while (iter.hasNext()) {
                        if (null == iter.next().get()) {
                            iter.remove();
                        }
                    }
                    sListeners.mapNotNull{it.get()} // Don't assert non-null: could have changed!!
                }
            return procs
        }

    }
}
