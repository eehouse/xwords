/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
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
        if ( null != intent && null != intent.getAction() 
             && intent.getAction().equals( Intent.ACTION_BOOT_COMPLETED ) ) {
            DbgUtils.logf("RelayReceiver.onReceive: launching timer on boot");
            RestartTimer( context );
        } else {
            // DbgUtils.logf( "RelayReceiver::onReceive()" );
            // Toast.makeText( context, "RelayReceiver: timer fired", 
            //                 Toast.LENGTH_SHORT).show();
            RelayService.timerFired( context );
        }
    }

    public static void RestartTimer( Context context )
    {
        RestartTimer( context, 
                      1000 * XWPrefs.getProxyInterval( context ) );
    }

    public static void RestartTimer( Context context, long interval_millis )
    {
        AlarmManager am =
            (AlarmManager)context.getSystemService( Context.ALARM_SERVICE );

        Intent intent = new Intent( context, RelayReceiver.class );
        PendingIntent pi = PendingIntent.getBroadcast( context, 0, intent, 0 );

        if ( interval_millis > 0 ) {
            long first_millis = SystemClock.elapsedRealtime() + interval_millis;
            am.setInexactRepeating( AlarmManager.ELAPSED_REALTIME_WAKEUP, 
                                    first_millis, // first firing
                                    interval_millis, pi );
        } else {
            // will happen if user's set getProxyInterval to return 0
            am.cancel( pi );
        }
    }

}
