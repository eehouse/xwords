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
import javax.net.SocketFactory;
import java.net.InetAddress;
import java.net.Socket;
import java.io.InputStream;
import java.io.OutputStream;

public class RelayService extends Service {

    private static final String proxy_addr = "10.0.2.2";
    private static final int proxy_port = 10998;
    
    private NotificationManager m_nm;

    @Override
    public void onCreate()
    {
        super.onCreate();
        Utils.logf( "RelayService::onCreate() called" );

        Thread thread = new Thread( null, new Runnable() {
                public void run() {
                    try {
                        SocketFactory factory = SocketFactory.getDefault();
                        InetAddress addr = InetAddress.getByName( proxy_addr );
                        Socket socket = factory.createSocket( addr, proxy_port );
                        socket.setSoTimeout( 3000 );
                        Utils.logf( "writing to proxy socket" );
                        OutputStream outStream = socket.getOutputStream();
                        outStream.write( 0 );
                        InputStream inStream = socket.getInputStream();
                        int result = inStream.read();
                        socket.close();
                        Utils.logf( "read %d and closed proxy socket", result );
                    } catch( java.net.UnknownHostException uhe ) {
                        Utils.logf( uhe.toString() );
                    } catch( java.io.IOException ioe ) {
                        Utils.logf( ioe.toString() );
                    }

                    RelayService.this.stopSelf();
                }
            }, getClass().getName() );
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

}

