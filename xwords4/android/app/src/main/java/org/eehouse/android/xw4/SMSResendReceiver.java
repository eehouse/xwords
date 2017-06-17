/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
/*
 * Copyright 2012 by Eric House (xwords@eehouse.org).  All rights
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

import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;

/*
 * SMS messages get dropped. We resend pending relay messages when we gain
 * network connectivity. There's no similar event for gaining the ability to
 * send SMS, so this class handles doing it on a timer. With backoff.
 */

public class SMSResendReceiver extends BroadcastReceiver {
    private static final String TAG = SMSResendReceiver.class.getSimpleName();

    private static final String BACKOFF_KEY = TAG + "/backoff";
    private static final int MIN_BACKOFF_SECONDS
        = BuildConfig.DEBUG ? 10 : 60 * 5;
    private static final int MAX_BACKOFF_SECONDS
        = BuildConfig.DEBUG ? 60 * 5 : 60 * 60 * 12;

    @Override
    public void onReceive( Context context, Intent intent )
    {
        GameUtils.resendAllIf( context, CommsConnType.COMMS_CONN_SMS, true );
        setTimer( context, true );
    }

    static void resetTimer( Context context )
    {
        DBUtils.setIntFor( context, BACKOFF_KEY, MIN_BACKOFF_SECONDS );
        setTimer( context );
    }

    static void setTimer( Context context )
    {
        setTimer( context, false );
    }
    
    private static void setTimer( Context context, boolean advance )
    {
        AlarmManager am =
            (AlarmManager)context.getSystemService( Context.ALARM_SERVICE );

        Intent intent = new Intent( context, SMSResendReceiver.class );
        PendingIntent pi = PendingIntent.getBroadcast( context, 0, intent, 0 );
        am.cancel( pi );

        int backoff = DBUtils.getIntFor( context, BACKOFF_KEY, MIN_BACKOFF_SECONDS );
        if ( advance ) {
            backoff = Math.min( MAX_BACKOFF_SECONDS, backoff * 2 );
            DBUtils.setIntFor( context, BACKOFF_KEY, backoff );
        }

        long millis = 1000L * backoff;
        Log.d( TAG, "set for %d seconds from now", millis / 1000 );
        millis += SystemClock.elapsedRealtime();
        am.set( AlarmManager.ELAPSED_REALTIME,  millis, pi );
    }
}
