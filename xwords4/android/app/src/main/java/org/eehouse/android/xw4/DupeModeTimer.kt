/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2019 - 2024 by Eric House (xwords@eehouse.org).  All rights
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
package org.eehouse.android.xw4

import android.app.AlarmManager
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.os.SystemClock

import java.text.DateFormat
import java.util.Date

import org.eehouse.android.xw4.DBUtils.DBChangeListener
import org.eehouse.android.xw4.DBUtils.GameChangeType
import org.eehouse.android.xw4.jni.CurGameInfo
import org.eehouse.android.xw4.jni.JNIThread
import org.eehouse.android.xw4.jni.XwJNI
import org.eehouse.android.xw4.loc.LocUtils

/**
 * This class owns the problem of timers in duplicate-mode games. Unlike the
 * existing timers that run only when a game is open and visible, they run
 * with the clock for any game where it's a local player's turn. So this
 * module needs to be aware of all games in that state and to be counting
 * their timers down at all times. For each game for which a timer's running
 * it's either 1) sending updates to the game (if it's open) OR 2) keeping an
 * unhideable notification open with the relevant time counting down.
 */
private val TAG = DupeModeTimer::class.java.simpleName

class DupeModeTimer : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        sCurTimer = Long.MAX_VALUE // clear so we'll set again
        sQueue.addAll()
    }

    private class RowidQueue : Thread() {
        private val mSet: MutableSet<Long> = HashSet()

        fun addAll() {
            addOne(0)
        }

        @Synchronized
        fun addOne(rowid: Long) {
            synchronized(mSet) {
                mSet.add(rowid)
                (mSet as Object).notify()
            }
        }

        override fun run() {
            var rowid = DBUtils.ROWID_NOTFOUND
            while (true) {
                synchronized(mSet) {
                    mSet.remove(rowid)
                    if (0 == mSet.size) {
                        try {
                            (mSet as Object).wait()
                            Assert.assertTrueNR(0 < mSet.size)
                        } catch (ie: InterruptedException) {
                            break
                        }
                    }
                    rowid = mSet.iterator().next()
                }
                inventoryGames(rowid)
            }
        }

        private fun inventoryGames(onerow: Long) {
            Log.d(TAG, "inventoryGames($onerow)")
            val context = XWApp.getContext()
            val dupeGames =
                if (onerow == 0L) DBUtils.getDupModeGames(context)
                else DBUtils.getDupModeGames(context, onerow)

            Log.d(TAG, "inventoryGames(%s)", dupeGames)
            val now = Utils.getCurSeconds()
            var minTimer = sCurTimer

            for (rowid in dupeGames.keys) {
                val timerFires = dupeGames[rowid]!!

                synchronized(sDirtyVals) {
                    if (sDirtyVals.containsKey(rowid) && timerFires == sDirtyVals[rowid]) {
                        sDirtyVals.remove(rowid)
                    }
                }

                if (timerFires > now) {
                    Log.d(
                        TAG, "found dupe game with %d seconds left",
                        timerFires - now
                    )
                    postNotification(context, rowid, timerFires.toLong())
                    if (timerFires < minTimer) {
                        minTimer = timerFires.toLong()
                    }
                } else {
                    cancelNotification(context, rowid)
                    Log.d(TAG, "found dupe game with expired or inactive timer")
                    if (timerFires > 0) {
                        giveGameTime(rowid)
                    }
                }
            }

            setTimer(context, minTimer)
        }

        private fun giveGameTime(rowid: Long) {
            Log.d(TAG, "giveGameTime(%d)() starting", rowid)
            GameLock.tryLock(rowid).use { lock ->
                if (null != lock) {
                    val context = XWApp.getContext()
                    val gi = CurGameInfo(context)
                    val sink = MultiMsgSink(context, rowid)
                    GameUtils.loadMakeGame(context, gi, sink, lock)
                        .use { gamePtr ->  // calls getJNI()
                        Log.d(TAG, "got gamePtr: %H", gamePtr)
                        if (null != gamePtr) {
                            var draw = false
                            for (ii in 0..2) {
                                draw = XwJNI.server_do(gamePtr) || draw
                            }

                            GameUtils.saveGame(context, gamePtr, gi, lock, false)

                            if (draw && XWPrefs.getThumbEnabled(context)) {
                                val bitmap = GameUtils.takeSnapshot(context, gamePtr, gi)
                                DBUtils.saveThumbnail(context, lock, bitmap)
                            }
                        }
                    }
                }
            }
            Log.d(TAG, "giveGameTime(%d)() DONE", rowid)
        }
    }

    companion object {
        private val sMyChannel = Channels.ID.DUP_TIMER_RUNNING

        private var sQueue = RowidQueue()
        private val sDirtyVals: MutableMap<Long, Int> = HashMap()

        private val s_df: DateFormat = DateFormat.getTimeInstance()
        private var sCurTimer = Long.MAX_VALUE

        init {
            sQueue.start()

            DBUtils.setDBChangeListener(object : DBChangeListener {
                override fun gameSaved(
                    context: Context, rowid: Long,
                    change: GameChangeType
                ) {
                    // Log.d( TAG, "gameSaved(rowid=%d,change=%s) called", rowid, change );
                    when (change) {
                        GameChangeType.GAME_CHANGED, GameChangeType.GAME_CREATED -> synchronized(
                            sDirtyVals
                        ) {
                            if (sDirtyVals.containsKey(rowid)) {
                                sQueue.addOne(rowid)
                            } else {
                                // Log.d( TAG, "skipping; not dirty" );
                            }
                        }

                        GameChangeType.GAME_DELETED -> cancelNotification(context, rowid)
                        else -> {Log.d(TAG, "gameSaved(): unexpected change $change")}
                    }
                }
            })
        }

        /**
         * Called when
         */
        fun init() {
            Log.d(TAG, "init()")
            sQueue.addAll()
        }

        fun gameOpened(context: Context, rowid: Long) {
            Log.d(TAG, "gameOpened(%s, %d)", context, rowid)
            sQueue.addOne(rowid)
        }

        fun gameClosed(rowid: Long) {
            Log.d(TAG, "gameClosed(%d)", rowid)
            sQueue.addOne(rowid)
        }

        // public static void timerPauseChanged( Context context, long rowid )
        // {
        //     sQueue.addOne( context, rowid );
        // }
        fun timerChanged(context: Context, gameID: Int, newVal: Int) {
            val rowids = DBUtils.getRowIDsFor(context, gameID)
            for (rowid in rowids) {
                Log.d(TAG, "timerChanged(rowid=%d, newVal=%d)", rowid, newVal)
                synchronized(sDirtyVals) {
                    sDirtyVals.put(rowid, newVal)
                }
            }
        }

        private fun postNotification(context: Context, rowid: Long, `when`: Long) {
            Log.d(TAG, "postNotification(rowid=%d)", rowid)
            if (!JNIThread.gameIsOpen(rowid)) {
                var title = LocUtils.getString(context, R.string.dup_notif_title)
                if (BuildConfig.DEBUG) {
                    title += " ($rowid)"
                }
                val body = context.getString(
                    R.string.dup_notif_title_fmt,
                    s_df.format(Date(1000 * `when`))
                )
                val intent = GamesListDelegate.makeRowidIntent(context, rowid)

                val pauseIntent = GamesListDelegate.makeRowidIntent(context, rowid)
                pauseIntent.putExtra(BoardDelegate.PAUSER_KEY, true)
                Utils.postOngoingNotification(
                    context, intent, title, body,
                    rowid, sMyChannel,
                    pauseIntent, R.string.board_menu_game_pause
                )
            } else {
                Log.d(TAG, "postOngoingNotification(%d): open, so skipping", rowid)
            }
        }

        private fun cancelNotification(context: Context, rowid: Long) {
            Log.d(TAG, "cancelNotification(rowid=%d)", rowid)
            Utils.cancelNotification(context, sMyChannel, rowid)
        }

        private fun setTimer(context: Context, whenSeconds: Long) {
            if (whenSeconds < sCurTimer) {
                sCurTimer = whenSeconds
                val intent = Intent(context, DupeModeTimer::class.java)
                val pi = PendingIntent.getBroadcast(
                    context, 0, intent,
                    PendingIntent.FLAG_IMMUTABLE
                )

                val now = Utils.getCurSeconds()
                val fire_millis = (SystemClock.elapsedRealtime()
                        + (1000 * (whenSeconds - now)))

                val am = context.getSystemService(Context.ALARM_SERVICE) as AlarmManager
                am[AlarmManager.ELAPSED_REALTIME, fire_millis] = pi
            }
        }
    }
}
