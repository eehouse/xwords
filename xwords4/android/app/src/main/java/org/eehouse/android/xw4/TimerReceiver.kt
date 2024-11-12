/*
 * Copyright 2010 - 2024 by Eric House (xwords@eehouse.org).  All rights
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
import android.os.Build
import android.text.TextUtils

import java.io.Serializable
import java.text.SimpleDateFormat
import java.util.Date
import java.util.TreeMap
import kotlin.math.abs

class TimerReceiver : BroadcastReceiver() {
    interface TimerCallback {
        fun timerFired(context: Context)
        fun incrementBackoff(prevBackoff: Long): Long
    }

    interface WithData {
        fun withData(data: Data)
    }

    class Data : Serializable {
        val mFields: MutableMap<String, MutableMap<String, Long>> =
            HashMap()

        @Transient
        private var mDirty = false

        @Transient
        private var mRefcount = 0

        fun get(): Data {
            ++mRefcount
            // Log.d( TAG, "get(): refcount now %d", mRefcount );
            return this
        }

        fun put(context: Context) {
            Assert.assertTrueNR(0 <= mRefcount)
            --mRefcount
            // Log.d( TAG, "put(): refcount now %d", mRefcount );
            if (0 == mRefcount && mDirty) {
                store(context, this)
                mDirty = false
            }
        }

        fun clients(): Set<String> {
            return mFields.keys
        }

        fun remove(client: String) {
            mFields.remove(client)
            mDirty = true
            Log.d(TAG, "remove(%s)", client)
        }

        fun setFor(client: TimerCallback, key: String, `val`: Long) {
            setFor(className(client), key, `val`)
        }

        fun setFor(client: String, key: String, `val`: Long) {
            if (!mFields.containsKey(client)) {
                mFields[client] = HashMap()
            }
            val map = mFields[client]!!
            if (!map.containsKey(key) || `val` != map[key]) {
                map[key] = `val`
                mDirty = true
            }
        }

        fun getFor(client: TimerCallback, key: String, dflt: Long): Long {
            return getFor(className(client), key, dflt)
        }

        fun getFor(client: String, key: String, dflt: Long): Long {
            var result = dflt
            if (mFields.containsKey(client)) {
                val map: Map<String, Long> = mFields[client]!!
                if (map.containsKey(key)) {
                    result = map[key]!!
                }
            }
            return result
        }
    }

    override fun onReceive(context: Context, intent: Intent) {
        val timerID = intent.getLongExtra(KEY_TIMER_ID, -1)
        onReceiveImpl(context, timerID, TAG)
    }

    companion object {
        private val TAG: String = TimerReceiver::class.java.simpleName
        private const val VERBOSE = false
        private val DATA_KEY = TAG + "/data"
        private const val KEY_FIREWHEN = "FIREWHEN"
        private const val KEY_BACKOFF = "BACKOFF"
        private const val CLIENT_STATS = "stats"
        private const val KEY_COUNT = "COUNT"
        private const val KEY_NEXT_FIRE = "NEXTFIRE"
        private const val KEY_NEXT_SPAN = "SPAN"
        private const val KEY_WORST = "WORST"
        private const val KEY_AVG_MISS = "AVG_MISS"
        private const val KEY_AVG_SPAN = "AVG_SPAN"
        private const val MIN_FUTURE = 2000L
        private const val KEY_TIMER_ID = "timerID"

        // If it's a number > 30 years past Epoch, format it as a date
        private val sFmt = SimpleDateFormat("MMM dd HH:mm:ss ")
        private fun fmtLong(dateOrNum: Long): String {
            val result =
                if (dateOrNum < 1000 * 60 * 60 * 24 * 365 * 30) {
                    String.format("%d", dateOrNum)
                } else {
                    sFmt.format(Date(dateOrNum))
                }
            return result
        }

        private fun toString(data: Data?): String {
            val all: MutableList<String?> = ArrayList()
            for (client in data!!.mFields.keys) {
                val ones: MutableList<String?> = ArrayList()
                val map: Map<String, Long> = data.mFields[client]!!
                for (key in map.keys) {
                    ones.add(String.format("%s: %s", key, fmtLong(map[key]!!)))
                }
                val one = String.format("{%s}", TextUtils.join(", ", ones))
                all.add(String.format("%s: %s", getSimpleName(client), one))
            }
            return String.format("{%s}", TextUtils.join(", ", all))
        }

        fun jobTimerFired(context: Context, timerID: Long, src: String) {
            onReceiveImpl(context, timerID, src)
        }

        private fun onReceiveImpl(
            context: Context,
            timerID: Long, src: String
        ) {
            Log.d(TAG, "onReceiveImpl(timerID=%d, src=%s)", timerID, src)
            load(context, object : WithData {
                override fun withData(data: Data) {
                    // updateStats(context, data)
                    data.setFor(CLIENT_STATS, KEY_NEXT_FIRE, 0)
                    val fired = fireExpiredTimers(context, data)
                    incrementBackoffs(data, fired)
                    setNextTimer(context, data)
                }
            })
            Log.d(TAG, "onReceiveImpl(timerID=%d, src=%s) DONE", timerID, src)
        }

        // fun statsStr(context: Context): String {
        //     val sb = StringBuffer()
        //     if (BuildConfig.NON_RELEASE || XWPrefs.getDebugEnabled(context)) {
        //         load(context, object : WithData {
        //             override fun withData(data: Data) {
        //                 // TreeMap to sort by timer fire time
        //                 val tmpMap = TreeMap<Long, String>()
        //                 for (client in data.clients()) {
        //                     val nextFire = data.getFor(client, KEY_FIREWHEN, 0)
        //                     if (0L != nextFire) {
        //                         tmpMap[nextFire] = client
        //                     }
        //                 }
        //                 sb.append("Next timers:\n")
        //                 for ((key, value) in tmpMap) {
        //                     sb.append(getSimpleName(value)).append(": ")
        //                         .append(fmtLong(key))
        //                         .append("\n")
        //                 }

        //                 val count = data.getFor(CLIENT_STATS, KEY_COUNT, 0)
        //                 sb.append("\nTimers fired: ").append(count).append("\n")
        //                 if (0 < count) {
        //                     val avgMiss = data.getFor(CLIENT_STATS, KEY_AVG_MISS, 0)
        //                     sb.append("Avg delay: ")
        //                         .append(String.format("%.1fs\n", avgMiss / 1000f))
        //                     val worst = data.getFor(CLIENT_STATS, KEY_WORST, 0)
        //                     sb.append("Worst delay: ")
        //                         .append(String.format("%.1fs\n", worst / 1000f))
        //                     val avgSpan = data.getFor(CLIENT_STATS, KEY_AVG_SPAN, 0)
        //                     sb.append("Avg interval: ").append((avgSpan + 500) / 1000).append("s\n")
        //                 }
        //             }
        //         })
        //     }
        //     return sb.toString()
        // }

        // fun clearStats(context: Context) {
        //     load(context, object : WithData {
        //         override fun withData(data: Data) {
        //             data.remove(CLIENT_STATS)
        //         }
        //     })
        // }

        fun setBackoff(
            context: Context, cback: TimerCallback,
            backoffMS: Long
        ) {
            Log.d(TAG, "setBackoff(client=%s, backoff=%ds)",
                  className(cback), backoffMS / 1000)
            load(context, object : WithData {
                override fun withData(data: Data) {
                    data.setFor(cback, KEY_BACKOFF, backoffMS)
                    setTimer(context, data, backoffMS, true, cback)
                }
            })
        }

        // This one's public. Sets a one-time timer. Any backoff or re-set the
        // client has to handle
        fun setTimerRelative(
            context: Context, cback: TimerCallback,
            waitMS: Long
        ) {
            val fireMS = waitMS + System.currentTimeMillis()
            setTimer(context, cback, fireMS)
        }

        fun setTimer(
            context: Context, cback: TimerCallback,
            fireMS: Long
        ) {
            load(context, object : WithData {
                override fun withData(data: Data) {
                    data.setFor(cback, KEY_FIREWHEN, fireMS)
                    setNextTimer(context, data)
                }
            })
        }

        fun allTimersFired(context: Context) {
            val callbacks = getCallbacks(context)
            for (callback in callbacks) {
                callback!!.timerFired(context)
            }
        }

        var sCallbacks: MutableMap<String, TimerCallback?> = HashMap()
        @Synchronized
        private fun getCallbacks(context: Context): Set<TimerCallback?> {
            val results: MutableSet<TimerCallback?> = HashSet()
            for (callback in sCallbacks.values) {
                results.add(callback)
            }
            return results
        }

        @Synchronized
        private fun getCallback(client: String): TimerCallback? {
            var callback: TimerCallback?
            try {
                callback = sCallbacks[client]
                if (null == callback) {
                    val clazz = Class.forName(client)
                    callback = clazz.newInstance() as TimerCallback
                    sCallbacks[client] = callback
                }
            } catch (ex: Exception) {
                callback = null
                Log.ex(TAG, ex)
            }

            return callback
        }

        private fun fireExpiredTimers(context: Context, data: Data):
            Set<TimerCallback>
        {
            Log.d(TAG, "fireExpiredTimers()")
            val clients: MutableSet<String> = HashSet()
            val now = System.currentTimeMillis()
            for (client in data.clients()) {
                val fireTime = data.getFor(client, KEY_FIREWHEN, 0)
                // Assert.assertTrueNR( fireTime < now ); <- firing
                if (0L != fireTime && fireTime <= now) {
                    clients.add(client)
                    Log.d(
                        TAG, "fireExpiredTimers(): firing %s %d ms late", client,
                        now - fireTime
                    )
                }
            }

            val callees: MutableSet<TimerCallback> = HashSet()
            for (client in clients) {
                val callback = getCallback(client)
                if (null == callback) { // class no longer exists
                    data.remove(client)
                } else {
                    data.setFor(client, KEY_FIREWHEN, 0)
                    callback.timerFired(context)
                    callees.add(callback)
                }
            }

            Log.d(TAG, "fireExpiredTimers() DONE")
            return callees
        }

        private fun incrementBackoffs(data: Data, callbacks: Set<TimerCallback>) {
            val now = System.currentTimeMillis()
            for (callback in callbacks) {
                var backoff = data.getFor(callback, KEY_BACKOFF, 0)
                if (0L != backoff) {
                    backoff = callback.incrementBackoff(backoff)
                    data.setFor(callback, KEY_BACKOFF, backoff)
                    data.setFor(callback, KEY_FIREWHEN, now + backoff)
                }
            }
        }

        private fun setNextTimer(context: Context, data: Data) {
            var firstFireTime = Long.MAX_VALUE
            var firstClient: String? = null
            val now = System.currentTimeMillis()
            for (client in data.clients()) {
                val fireTime = data.getFor(client, KEY_FIREWHEN, 0)
                if (0L != fireTime) {
                    if (fireTime < firstFireTime) {
                        firstFireTime = fireTime
                        firstClient = client
                    }
                }
            }

            if (null != firstClient) {
                val curNextFire = data.getFor(CLIENT_STATS, KEY_NEXT_FIRE, 0)
                if (1000L < abs((firstFireTime - curNextFire).toDouble())) {
                    if (firstFireTime - now < MIN_FUTURE) { // Less than a 2 seconds in the future?
                        Log.d(
                            TAG,
                            "setNextTimer(): moving firstFireTime (for %s) to the future: %s -> %s",
                            firstClient,
                            fmtLong(firstFireTime),
                            fmtLong(now + MIN_FUTURE)
                        )
                        firstFireTime = now + MIN_FUTURE
                        data.setFor(firstClient, KEY_FIREWHEN, firstFireTime)
                    }

                    val delayMS = firstFireTime - now
                    data.setFor(CLIENT_STATS, KEY_NEXT_FIRE, firstFireTime)
                    data.setFor(CLIENT_STATS, KEY_NEXT_SPAN, delayMS)
                    val timerID = 1 + data.getFor(CLIENT_STATS, KEY_TIMER_ID, 0)
                    data.setFor(CLIENT_STATS, KEY_TIMER_ID, timerID)

                    val am =
                        context.getSystemService(Context.ALARM_SERVICE) as AlarmManager
                    val intent = Intent(context, TimerReceiver::class.java)
                    intent.putExtra(KEY_TIMER_ID, timerID)
                    val pi = PendingIntent
                        .getBroadcast(
                            context, 0, intent, PendingIntent.FLAG_CANCEL_CURRENT
                                    or PendingIntent.FLAG_IMMUTABLE
                        )
                    am[AlarmManager.RTC_WAKEUP, firstFireTime] = pi

                    setJobTimerIf(context, delayMS, timerID)

                    if (VERBOSE) {
                        Log.d(
                            TAG, "setNextTimer(): SET id %d for %s at %s", timerID,
                            getSimpleName(firstClient), fmtLong(firstFireTime)
                        )
                    }
                    // } else {
                    //     Assert.assertTrueNR( 0 != curNextFire );
                    //     long diff = Math.abs( firstFireTime - curNextFire );
                    //     Log.d( TAG, "not setting timer for %s: firstFireTime: %d,"
                    //            + " curNextFire: %d; diff: %d",
                    //            getSimpleName(firstClient), firstFireTime, curNextFire, diff );
                }
            }
        }

        private fun setJobTimerIf(context: Context, delayMS: Long, timerID: Long) {
            if (Build.VERSION_CODES.LOLLIPOP <= Build.VERSION.SDK_INT) {
                TimerJobReceiver.setTimer(context, delayMS, timerID)
            }
        }

        private fun setTimer(
            context: Context, data: Data, backoff: Long,
            force: Boolean, cback: TimerCallback
        ) {
            var force = force
            val client = className(cback)
            if (!force) {
                val curBackoff = data.getFor(client, KEY_BACKOFF, backoff)
                val nextFire = data.getFor(client, KEY_FIREWHEN, 0)
                force = 0L == nextFire || backoff != curBackoff
            }
            if (VERBOSE) {
                Log.d(
                    TAG, "setTimer(clazz=%s, force=%b, curBackoff=%d)",
                    getSimpleName(client), force, backoff
                )
            }
            if (force) {
                val now = System.currentTimeMillis()
                val fireMillis = now + backoff
                data.setFor(client, KEY_FIREWHEN, fireMillis)
            }

            setNextTimer(context, data)
        }

        // What to measure? By how much are timer fires delayed? How's that as a
        // percentage of what we wanted?
        // private fun updateStats(context: Context, data: Data) {
        //     if (BuildConfig.NON_RELEASE || XWPrefs.getDebugEnabled(context)) {
        //         val target = data.getFor(CLIENT_STATS, KEY_NEXT_FIRE, 0)
        //         // Ignore for stats purposes if target not yet set
        //         if (0 < target) {
        //             val oldCount = data.getFor(CLIENT_STATS, KEY_COUNT, 0)
        //             data.setFor(CLIENT_STATS, KEY_COUNT, oldCount + 1)

        //             val now = System.currentTimeMillis()

        //             if (0 < target) {
        //                 val missedByMS = now - target
        //                 val worstMiss = data.getFor(CLIENT_STATS, KEY_WORST, 0)
        //                 if (worstMiss < missedByMS) {
        //                     data.setFor(CLIENT_STATS, KEY_WORST, missedByMS)
        //                 }

        //                 updateAverage(data, KEY_AVG_MISS, oldCount, missedByMS)
        //                 val targetSpan = data.getFor(CLIENT_STATS, KEY_NEXT_SPAN, 0)
        //                 updateAverage(data, KEY_AVG_SPAN, oldCount, targetSpan)
        //             }
        //         }
        //     }
        // }

        // private fun updateAverage(
        //     data: Data, key: String,
        //     oldCount: Long, newVal: Long
        // ) {
        //     var avg = data.getFor(CLIENT_STATS, key, 0)
        //     avg = ((avg * oldCount) + newVal) / (oldCount + 1)
        //     data.setFor(CLIENT_STATS, key, avg)
        // }

        private fun className(cbck: TimerCallback): String {
            return cbck.javaClass.name
        }

        private fun getSimpleName(client: String): String {
            val parts = TextUtils.split(client, "\\.")
            val end = parts[parts.size - 1]
            return TextUtils.split(end, "\\$")[0]
        }

        private val sDataWrapper = arrayOf<Data?>(null)
        fun load(context: Context, proc: WithData) {
            Log.d(TAG, "load() waiting on sDataWrapper")
            val now = System.currentTimeMillis()
            synchronized(sDataWrapper) {
                var data = sDataWrapper[0]
                if (null == data) {
                    try {
                        data = DBUtils.getSerializableFor(context, DATA_KEY) as Data?
                        if (VERBOSE) {
                            Log.d(TAG, "load(): loaded: %s", toString(data))
                        }
                    } catch (ex: Exception) {
                        data = null
                    }

                    if (null == data) {
                        data = Data()
                    }
                    sDataWrapper[0] = data
                }
                proc.withData(data.get())
                data.put(context)
            }
            val after = System.currentTimeMillis()
            Log.d(TAG, "load() took %d millis", after - now)
        }

        private fun store(context: Context, data: Data) {
            DBUtils.setSerializableFor(context, DATA_KEY, data)
            if (VERBOSE) {
                Log.d(TAG, "store(): saved: %s", toString(data))
            }
        }
    }
}
