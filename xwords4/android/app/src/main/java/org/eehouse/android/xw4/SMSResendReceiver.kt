/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2012 - 2024 by Eric House (xwords@eehouse.org).  All rights
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
import kotlin.math.min

import org.eehouse.android.xw4.GameUtils.ResendDoneProc
import org.eehouse.android.xw4.TimerReceiver.TimerCallback
import org.eehouse.android.xw4.jni.CommsAddrRec
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType

/*
 * SMS messages get dropped. We resend pending relay messages when we gain
 * network connectivity. There's no similar event for gaining the ability to
 * send SMS, so this class handles doing it on a timer. With backoff.
 */
object SMSResendReceiver {
    private val TAG: String = SMSResendReceiver::class.java.simpleName

    private val BACKOFF_KEY = TAG + "/backoff"
    private const val MIN_BACKOFF_SECONDS = 60 * 5
    private const val MAX_BACKOFF_SECONDS = 60 * 60 * 12

    private val sTimerCallbacks
            : TimerCallback = object : TimerCallback {
        override fun timerFired(context: Context) {
            GameUtils.resendAllIf(context,
                                  CommsConnType.COMMS_CONN_SMS,
                                  true,
                object : GameUtils.ResendDoneProc {
                    override fun onResendDone(
                        context: Context,
                        nSent: Int
                    ) {
                        var backoff = -1
                        if (0 < nSent) {
                            backoff = setTimer(context, true)
                        }
                        if (BuildConfig.NON_RELEASE) {
                            DbgUtils.showf(
                                context,
                                "%d SMS msgs resent;"
                                        + " backoff: %d",
                                nSent, backoff
                            )
                        }
                    }
                })
        }

        override fun incrementBackoff(prevBackoff: Long): Long {
            Assert.failDbg()
            return 0
        }
    }

    fun resetTimer(context: Context) {
        DBUtils.setIntFor(context, BACKOFF_KEY, MIN_BACKOFF_SECONDS)
        setTimer(context)
    }

    private fun setTimer(context: Context): Int {
        return setTimer(context, false)
    }

    private fun setTimer(context: Context, advance: Boolean): Int {
        var backoff = DBUtils.getIntFor(context, BACKOFF_KEY, MIN_BACKOFF_SECONDS)
        if (advance) {
            backoff = min(MAX_BACKOFF_SECONDS.toDouble(), (backoff * 2).toDouble())
                .toInt()
            DBUtils.setIntFor(context, BACKOFF_KEY, backoff)
        }

        val millis = 1000L * backoff
        TimerReceiver.setTimerRelative(context, sTimerCallbacks, millis)
        return backoff
    }
}
