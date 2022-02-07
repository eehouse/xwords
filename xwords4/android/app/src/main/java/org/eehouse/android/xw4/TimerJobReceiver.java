/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2022 by Eric House (xwords@eehouse.org).  All rights reserved.
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

package org.eehouse.android.xw4;

import android.app.job.JobInfo;
import android.app.job.JobParameters;
import android.app.job.JobScheduler;
import android.app.job.JobService;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.PersistableBundle;

public class TimerJobReceiver extends JobService {
    private static final String TAG = TimerJobReceiver.class.getSimpleName();
    private static final String KEY_TIMER_ID = "timerID";

    @Override
    public boolean onStartJob( JobParameters params )
    {
        PersistableBundle bundle = params.getExtras();
        long timerID = bundle.getLong( KEY_TIMER_ID );
        Log.d( TAG, "onStartJob(%s)", params );
        TimerReceiver.jobTimerFired( this, timerID, TAG );
        
        return false;           // job is finished
    }

    @Override
    public boolean onStopJob(JobParameters params)
    {
        Assert.failDbg();
        return true;
    }

    private static final int sJobId = BuildConfig.APPLICATION_ID.hashCode();
    static void setTimer( Context context, long delayMS, long timerID )
    {
        Assert.assertTrueNR( 0 < delayMS );
        ComponentName jobService = new ComponentName( context, TimerJobReceiver.class );
        PersistableBundle bundle = new PersistableBundle();
        bundle.putLong( KEY_TIMER_ID, timerID );
        JobInfo job = new JobInfo.Builder( sJobId, jobService )
            .setPersisted( true )
            .setMinimumLatency( delayMS )
            .setExtras( bundle )
            .build();
        JobScheduler scheduler = (JobScheduler)context
            .getSystemService(Context.JOB_SCHEDULER_SERVICE);
        scheduler.schedule( job );
        Log.d( TAG, "setTimer(delayMS=%d, id=%d): SET", delayMS, timerID );
    }
}
