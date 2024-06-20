/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2022 - 2024 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.job.JobInfo
import android.app.job.JobParameters
import android.app.job.JobScheduler
import android.app.job.JobService
import android.content.ComponentName
import android.content.Context
import android.os.PersistableBundle

class TimerJobReceiver : JobService() {
    override fun onStartJob(params: JobParameters): Boolean {
        val bundle = params.extras
        val timerID = bundle.getLong(KEY_TIMER_ID)
        Log.d(TAG, "onStartJob(%s)", params)
        TimerReceiver.jobTimerFired(this, timerID, TAG)

        return false // job is finished
    }

    override fun onStopJob(params: JobParameters): Boolean {
        Assert.failDbg()
        return true
    }

    companion object {
        private val TAG: String = TimerJobReceiver::class.java.simpleName
        private const val KEY_TIMER_ID = "timerID"

        private val sJobId = BuildConfig.APPLICATION_ID.hashCode()
        fun setTimer(context: Context, delayMS: Long, timerID: Long) {
            Assert.assertTrueNR(0 < delayMS)
            val jobService = ComponentName(context, TimerJobReceiver::class.java)
            val bundle = PersistableBundle()
            bundle.putLong(KEY_TIMER_ID, timerID)
            val job = JobInfo.Builder(sJobId, jobService)
                .setPersisted(true)
                .setMinimumLatency(delayMS)
                .setExtras(bundle)
                .build()
            val scheduler = context
                .getSystemService(JOB_SCHEDULER_SERVICE) as JobScheduler
            scheduler.schedule(job)
            Log.d(TAG, "setTimer(delayMS=%d, id=%d): SET", delayMS, timerID)
        }
    }
}
