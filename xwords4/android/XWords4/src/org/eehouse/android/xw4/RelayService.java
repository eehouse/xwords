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

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;

public class RelayService extends Service {

    private NotificationManager m_nm;

    @Override
    public void onCreate()
    {
        super.onCreate();
        Utils.logf( "RelayService::onCreate() called" );


        Thread thread = new Thread( null, m_task, getClass().getName() );
        thread.start();
    }

    @Override
    public void onDestroy()
    {
        Utils.logf( "RelayService::onDestroy() called" );
        super.onDestroy();

        // m_nm.cancel( R.string.running_notification );
    }

    @Override
    public IBinder onBind( Intent intent )
    {
        Utils.logf( "RelayService::onBind() called" );
        return null;
    }

    //@Override
    // protected int onStartCommand( Intent intent, int flags, int startId )
    // {
    //     Utils.logf( "RelayService::onStartCommand() called" );
    //     // return super.onStartCommand( intent, flags, startId );
    //     return 0;
    // }

    // private void setupNotification()
    // {
    //     m_nm = (NotificationManager)getSystemService( NOTIFICATION_SERVICE );

    //     Notification notification = 
    //         new Notification( R.drawable.icon48x48, "foo",
    //                           System.currentTimeMillis());

    //     PendingIntent intent = PendingIntent
    //         .getActivity( this, 0, new Intent(this, BoardActivity.class), 0);

    //     notification.setLatestEventInfo( this, "bazz", "bar", intent );
        
    //     m_nm.notify( R.string.running_notification, notification );
    // }

    // Thread that does the actual work of pinging the relay
    private Runnable m_task = new Runnable() {
            public void run() {

                RelayService.this.stopSelf();
            }
        };

}

