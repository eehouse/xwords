/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
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

import android.content.Context;
import android.content.Intent;
import android.content.BroadcastReceiver;
import android.widget.Toast;
import android.app.AlarmManager;
import android.app.PendingIntent;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import java.net.Socket;
import java.io.InputStream;
import java.io.DataInputStream;
import java.io.OutputStream;
import java.io.DataOutputStream;
import java.util.ArrayList;

import org.eehouse.android.xw4.jni.CommonPrefs;

public class RelayReceiver extends BroadcastReceiver {

    @Override
    public void onReceive( Context context, Intent intent )
    {
        if ( null != intent && null != intent.getAction() 
             && intent.getAction().equals( Intent.ACTION_BOOT_COMPLETED ) ) {
            Utils.logf( "launching timer on boot" );
            RestartTimer( context );
        } else {
            // Utils.logf( "RelayReceiver::onReceive()" );
            // if ( XWConstants.s_showProxyToast ) {
            //     Toast.makeText(context, "RelayReceiver: fired", 
            //                    Toast.LENGTH_SHORT).show();
            // }
            Intent service = new Intent( context, RelayService.class );
            context.startService( service );
        }
    }

    public static void RestartTimer( Context context, boolean force )
    {
        RestartTimer( context, 
                      1000 * CommonPrefs.getProxyInterval( context ), force );
    }

    public static void RestartTimer( Context context )
    {
        RestartTimer( context, false );
    }

    public static void RestartTimer( Context context, long interval_millis, 
                                     boolean force )
    {
        AlarmManager am =
            (AlarmManager)context.getSystemService( Context.ALARM_SERVICE );

        Intent intent = new Intent( context, RelayReceiver.class );
        PendingIntent pi = PendingIntent.getBroadcast( context, 0, intent, 0 );

        if ( interval_millis > 0 || force ) {
            // Utils.logf( "setting alarm for %d millis", interval_millis );
            am.setInexactRepeating( AlarmManager.ELAPSED_REALTIME_WAKEUP, 
                                    0, // first firing
                                    interval_millis, pi );
        } else {
            am.cancel( pi );
        }
    }

    public static void RestartTimer( Context context, long interval_millis )
    {
        RestartTimer( context, interval_millis, false );
    }

}
