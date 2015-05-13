/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2010 by Eric House (xwords@eehouse.org).  All rights
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
import android.widget.Toast;

public class RelayReceiver extends BroadcastReceiver {

    @Override
    public void onReceive( Context context, Intent intent )
    {
        RelayService.timerFired( context );
    }

    public static void setTimer( Context context )
    {
        setTimer( context, 1000 * XWPrefs.getProxyInterval( context ) );
    }

    public static void setTimer( Context context, long interval_millis )
    {
        DbgUtils.logf( "RelayReceiver.restartTimer(%d)", interval_millis );
        AlarmManager am =
            (AlarmManager)context.getSystemService( Context.ALARM_SERVICE );

        Intent intent = new Intent( context, RelayReceiver.class );
        PendingIntent pi = PendingIntent.getBroadcast( context, 0, intent, 0 );

        // Check if we have any relay IDs, since we'll be using them to
        // identify connected games for which we can fetch messages
        if ( interval_millis > 0 && DBUtils.haveRelayIDs( context ) ) {
            long fire_millis = SystemClock.elapsedRealtime() + interval_millis;
            am.set( AlarmManager.ELAPSED_REALTIME_WAKEUP, fire_millis, pi );
        } else {
            DbgUtils.logf( "RelayReceiver.restartTimer(): cancelling" );
            // will happen if user's set getProxyInterval to return 0
            am.cancel( pi );
        }
    }

}
