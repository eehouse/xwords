/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 - 2020 by Eric House (xwords@eehouse.org).  All rights
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

package org.eehouse.android.xw4;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.SystemClock;

public class TimerReceiver extends BroadcastReceiver {
    private static final String TAG = TimerReceiver.class.getSimpleName();
    private static final String KEY_BACKOFF = TAG + "/backoff";
    private static final String KEY_NEXT_BACKOFF = TAG + "/next_backoff";
    private static final long MIN_BACKOFF = 1000 * 10; // 10 seconds
    private static final long MAX_BACKOFF = 1000 * 60 * 60 * 23; // 23 hours

    @Override
    public void onReceive( Context context, Intent intent )
    {
        Log.d( TAG, "onReceive(intent=%s)", intent );
        RelayService.timerFired( context );
        MQTTUtils.timerFired( context );
        BTUtils.timerFired( context );

        long nextBackoff = DBUtils.getLongFor( context, KEY_BACKOFF, MIN_BACKOFF );
        if ( nextBackoff == MAX_BACKOFF ) {
            // at max, so no change and nothing to save
        } else {
            nextBackoff *= 2;
            if ( nextBackoff > MAX_BACKOFF ) {
                nextBackoff = MAX_BACKOFF;
            }
            DBUtils.setLongFor( context, KEY_BACKOFF, nextBackoff );
        }
        setTimer( context, nextBackoff, true );
    }

    static void restartBackoff( Context context )
    {
        DBUtils.setLongFor( context, KEY_BACKOFF, MIN_BACKOFF );
        setTimer( context, MIN_BACKOFF, false );
    }

    static void setTimer( Context context, boolean force )
    {
        long backoff = DBUtils.getLongFor( context, KEY_BACKOFF, MIN_BACKOFF );
        setTimer( context, backoff, force );
    }

    private synchronized static void setTimer( Context context, long backoff, boolean force )
    {
        if ( !force ) {
            long curBackoff = DBUtils.getLongFor( context, KEY_NEXT_BACKOFF, MIN_BACKOFF );
            force = backoff != curBackoff;
        }
        if ( force ) {
            long now = SystemClock.elapsedRealtime();
            long fireMillis = now + backoff;

            AlarmManager am =
                (AlarmManager)context.getSystemService( Context.ALARM_SERVICE );

            Intent intent = new Intent( context, TimerReceiver.class );
            PendingIntent pi = PendingIntent.getBroadcast( context, 0, intent, 0 );
            am.set( AlarmManager.ELAPSED_REALTIME_WAKEUP, fireMillis, pi );
            Log.d( TAG, "setTimer() set for %d seconds from now (%d)", backoff / 1000, now / 1000 );
            DBUtils.setLongFor( context, KEY_NEXT_BACKOFF, backoff );
        }
    }
}
