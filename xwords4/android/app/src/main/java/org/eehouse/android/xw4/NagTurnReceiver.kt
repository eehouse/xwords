/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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

import android.content.Context
import android.text.TextUtils

import org.eehouse.android.xw4.TimerReceiver.TimerCallback
import org.eehouse.android.xw4.loc.LocUtils

object NagTurnReceiver {
    private val TAG: String = NagTurnReceiver::class.java.simpleName

    private val NAG_INTERVAL_SECONDS = longArrayOf(
        // 2*60, // two minutes (for testing)
        // 5*60,
        // 10*60,
        (60 * 60 * 24 * 1).toLong(),  // one day
        (60 * 60 * 24 * 2).toLong(),  // two days
        (60 * 60 * 24 * 3).toLong(),  // three days
    )

    private val s_fmtData = arrayOf(
        intArrayOf(60 * 60 * 24, R.plurals.nag_days_fmt),
        intArrayOf(60 * 60, R.plurals.nag_hours_fmt),
        intArrayOf(60, R.plurals.nag_minutes_fmt),
    )

    private var s_nagsDisabledNet: Boolean? = null
    private var s_nagsDisabledSolo: Boolean? = null

    private val sTimerCallbacks
            : TimerCallback = object : TimerCallback {
        override fun timerFired(context: Context) {
            NagTurnReceiver.timerFired(context)
        }

        override fun incrementBackoff(prevBackoff: Long): Long {
            Assert.failDbg()
            return 0
        }
    }

    private fun timerFired(context: Context) {
        // loop through all games testing who's been sitting on a turn
        if (!getNagsDisabled(context)) {
            val needNagging = DBUtils.getNeedNagging(context)
            if (null != needNagging) {
                val now = System.currentTimeMillis()
                for (info in needNagging) {
                    Assert.assertTrueNR(info!!.m_nextNag < now)

                    info.m_nextNag = figureNextNag(
                        context,
                        info.m_lastMoveMillis
                    )

                    // Skip display of notifications disabled for this type
                    // of game
                    if (s_nagsDisabledSolo!! && info.isSolo) {
                        // do nothing
                    } else if (s_nagsDisabledNet!! && !info.isSolo) {
                        // do nothing
                    } else {
                        val lastWarning = 0L == info.m_nextNag
                        val rowid = info.m_rowid
                        val summary = GameUtils.getSummary(
                            context, rowid,
                            10
                        )
                        val prevPlayer =
                            if (null == summary) {
                                LocUtils.getString(context, R.string.prev_player)
                            } else summary.prevPlayer!!

                        val msgIntent =
                            GamesListDelegate.makeRowidIntent(context, rowid)
                        val millis = formatMillis(
                            context,
                            now - info.m_lastMoveMillis
                        )
                        var body = String.format(
                            LocUtils.getString(context,R.string.nag_body_fmt),
                            prevPlayer, millis
                        )
                        if (lastWarning) {
                            body = LocUtils.getString(context, R.string.nag_warn_last_fmt, body)
                        }
                        Utils.postNotification(
                            context, msgIntent,
                            R.string.nag_title, body,
                            rowid
                        )
                    }
                }
                DBUtils.updateNeedNagging(context, needNagging!!)

                setNagTimer(context)
            }
        }
    }

    fun restartTimer(context: Context) {
        setNagTimer(context)
    }

    private fun restartTimer(context: Context, fireTimeMS: Long) {
        if (!getNagsDisabled(context)) {
            TimerReceiver.setTimer(context, sTimerCallbacks, fireTimeMS)
        }
    }

    fun setNagTimer(context: Context) {
        if (!getNagsDisabled(context)) {
            val nextNag = DBUtils.getNextNag(context)
            if (0 < nextNag) {
                restartTimer(context, nextNag)
            }
        }
    }

    fun figureNextNag(context: Context, moveTimeMillis: Long): Long {
        var result: Long = 0
        val now = System.currentTimeMillis()
        if (now >= moveTimeMillis) {
            val intervals = getIntervals(context)
            for (nSecs in intervals!!) {
                val asMillis = moveTimeMillis + (nSecs * 1000)
                if (asMillis >= now) {
                    result = asMillis
                    break
                }
            }
        } else {
            Assert.failDbg()
        }
        return result
    }

    private var s_lastIntervals: LongArray? = null
    private var s_lastStr: String? = null
    private fun getIntervals(context: Context): LongArray? {
        var result: LongArray? = null
        val pref =
            XWPrefs.getPrefsString(context, R.string.key_nag_intervals)
        if (null != pref && 0 < pref.length) {
            if (pref == s_lastStr) {
                result = s_lastIntervals
            } else {
                val strs = TextUtils.split(pref, ",")
                val al = ArrayList<Long>()
                for (str in strs) {
                    try {
                        val value = str.toLong()
                        if (0 < value) {
                            al.add(value)
                        }
                    } catch (ex: Exception) {
                        Log.ex(TAG, ex)
                    }
                }
                if (0 < al.size) {
                    result = LongArray(al.size)
                    val iter: Iterator<Long> = al.iterator()
                    var ii = 0
                    while (iter.hasNext()) {
                        result[ii] = 60 * iter.next()
                        ++ii
                    }
                }
                s_lastStr = pref
                s_lastIntervals = result
            }
        }

        if (null == result) {
            result = NAG_INTERVAL_SECONDS
        }
        return result
    }

    private fun formatMillis(context: Context, millis: Long): String {
        var seconds = millis / 1000
        val results = ArrayList<String?>()
        for (datum in s_fmtData) {
            val `val` = seconds / datum[0]
            if (1 <= `val`) {
                results.add(
                    LocUtils.getQuantityString(
                        context, datum[1],
                        `val`.toInt(), `val`
                    )
                )
                seconds %= datum[0].toLong()
            }
        }
        val result = TextUtils.join(", ", results)
        return result
    }

    private fun getNagsDisabled(context: Context): Boolean {
        if (null == s_nagsDisabledNet) {
            val nagsDisabled =
                XWPrefs.getPrefsBoolean(
                    context, R.string.key_disable_nag,
                    false
                )
            s_nagsDisabledNet = nagsDisabled
        }
        if (null == s_nagsDisabledSolo) {
            val nagsDisabled =
                XWPrefs.getPrefsBoolean(
                    context, R.string.key_disable_nag_solo,
                    true
                )
            s_nagsDisabledSolo = nagsDisabled
        }
        return s_nagsDisabledNet!! && s_nagsDisabledSolo!!
    }

    fun resetNagsDisabled(context: Context) {
        s_nagsDisabledSolo = null
        s_nagsDisabledNet = s_nagsDisabledSolo
        restartTimer(context)
    }
}
