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
        Utils.logf( "RelayReceiver::onReceive()" );
        Toast.makeText(context, "RelayReceiver: fired", 
                       Toast.LENGTH_SHORT).show();

        // Do the actual background work.  Could do it here, but only
        // if we're sure to finish in 10 seconds and if it'll always
        // result in posting a notification.  Some scenarios

        query_relay( context );

        // Intent service = new Intent( context, RelayService.class );
        // context.startService( service );
    }

    private void query_relay( Context context ) 
    {
        String[] relayIDs = NetUtils.QueryRelay( context );
        if ( null != relayIDs ) {
            setupNotification( context, relayIDs );
        }
    }

    private void setupNotification( Context context, String[] relayIDs )
    {
        Intent intent = new Intent( context, DispatchNotify.class );
        //intent.addFlags( Intent.FLAG_ACTIVITY_CLEAR_TOP );
        intent.putExtra( context.getString(R.string.relayids_extra), 
                         relayIDs );

        PendingIntent pi = PendingIntent.
            getActivity( context, 0, intent, 
                         PendingIntent.FLAG_UPDATE_CURRENT );
        String title = context.getString(R.string.notify_title);
        Notification notification = 
            new Notification( R.drawable.icon48x48, title,
                              System.currentTimeMillis() );
        notification.flags |= Notification.FLAG_AUTO_CANCEL;

        notification.
            setLatestEventInfo( context, title, 
                                context.getString(R.string.notify_body), pi );

        NotificationManager nm = (NotificationManager)
            context.getSystemService( Context.NOTIFICATION_SERVICE );
        nm.notify( R.string.relayids_extra, notification );
    }

    public static void RestartTimer( Context context )
    {
        RestartTimer( context, 
                      1000 * CommonPrefs.getProxyInterval( context ) );
    }

    public static void RestartTimer( Context context, long interval_millis )
    {
        AlarmManager am =
            (AlarmManager)context.getSystemService( Context.ALARM_SERVICE );

        Intent intent = new Intent( context, RelayReceiver.class );
        PendingIntent pi = PendingIntent.getBroadcast( context, 0, intent, 0 );

        if ( interval_millis > 0 ) {
            Utils.logf( "setting alarm for %d millis", interval_millis );
            am.setInexactRepeating( AlarmManager.ELAPSED_REALTIME_WAKEUP, 
                                    0, // first firing
                                    interval_millis, pi );
        } else {
            am.cancel( pi );
        }
    }
}