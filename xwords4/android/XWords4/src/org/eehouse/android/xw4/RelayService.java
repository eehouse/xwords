/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
/*
 * Copyright 2010 - 2011 by Eric House (xwords@eehouse.org).  All
 * rights reserved.
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
import java.io.DataInputStream;
import java.io.OutputStream;
import java.io.DataOutputStream;
import java.util.ArrayList;

import org.eehouse.android.xw4.jni.GameSummary;
import org.eehouse.android.xw4.jni.CommonPrefs;

public class RelayService extends Service {

    @Override
    public void onCreate()
    {
        super.onCreate();
        
        Thread thread = new Thread( null, new Runnable() {
                public void run() {

                    String[] relayIDs = NetUtils.QueryRelay( RelayService.this );
                    if ( null != relayIDs ) {
                        if ( !DispatchNotify.tryHandle( relayIDs ) ) {
                            setupNotification( relayIDs );
                        }
                    }
                    RelayService.this.stopSelf();
                }
            }, getClass().getName() );
        thread.start();
    }

    @Override
    public IBinder onBind( Intent intent )
    {
        return null;
    }

    private void setupNotification( String[] relayIDs )
    {
        Intent intent = new Intent( this, DispatchNotify.class );
        intent.putExtra( DispatchNotify.RELAYIDS_EXTRA, relayIDs );

        PendingIntent pi = PendingIntent.
            getActivity( this, 0, intent, 
                         PendingIntent.FLAG_UPDATE_CURRENT );
        String title = getString(R.string.notify_title);
        Notification notification = 
            new Notification( R.drawable.icon48x48, title,
                              System.currentTimeMillis() );

        notification.flags |= Notification.FLAG_AUTO_CANCEL;
        if ( CommonPrefs.getSoundNotify( this ) ) {
            notification.defaults |= Notification.DEFAULT_SOUND;
        }
        if ( CommonPrefs.getVibrateNotify( this ) ) {
            notification.defaults |= Notification.DEFAULT_VIBRATE;
        }

        notification.
            setLatestEventInfo( this, title, 
                                getString(R.string.notify_body), pi );

        NotificationManager nm = (NotificationManager)
            getSystemService( Context.NOTIFICATION_SERVICE );
        nm.notify( R.string.notify_body, // unique id; any will do
                   notification );
    }
}
