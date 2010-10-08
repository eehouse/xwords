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
import java.io.DataInputStream;
import java.io.OutputStream;
import java.io.DataOutputStream;
import java.util.ArrayList;

import org.eehouse.android.xw4.jni.GameSummary;

public class RelayService extends Service {
    
    private NotificationManager m_nm;

    @Override
    public void onCreate()
    {
        super.onCreate();

        m_nm = (NotificationManager)getSystemService( NOTIFICATION_SERVICE );

        Thread thread = new Thread( null, new Runnable() {
                public void run() {

                    int[] nBytes = new int[1];
                    String[] ids = collectIDs( nBytes );
                    if ( null != ids && 0 < ids.length ) {
                        try {
                            Socket socket = 
                                NetUtils.MakeProxySocket( RelayService.this, 
                                                          3000 );
                            DataOutputStream outStream = 
                                new DataOutputStream( socket.getOutputStream() );

                            // total packet size
                            outStream.writeShort( 2 + nBytes[0] + ids.length + 1 );
                            Utils.logf( "total packet size: %d",
                                        2 + nBytes[0] + ids.length );

                            outStream.writeByte( NetUtils.PROTOCOL_VERSION );
                            outStream.writeByte( NetUtils.PRX_HAS_MSGS );

                            // number of ids
                            outStream.writeShort( ids.length );
                            Utils.logf( "wrote count %d to proxy socket",
                                        ids.length );

                            for ( String id : ids ) {
                                outStream.writeBytes( id );
                                outStream.write( '\n' );
                            }
                            outStream.flush();

                            DataInputStream dis = 
                                new DataInputStream(socket.getInputStream());
                            Utils.logf( "reading from proxy socket" );
                            short result = dis.readShort();
                            short nameCount = dis.readShort();
                            short[] msgCounts = null;
                            if ( nameCount == ids.length ) {
                                msgCounts = new short[nameCount];
                                for ( int ii = 0; ii < nameCount; ++ii ) {
                                    msgCounts[ii] = dis.readShort();
                                    Utils.logf( "msgCounts[%d]=%d", ii, 
                                                msgCounts[ii] );
                                }
                            }
                            socket.close();
                            Utils.logf( "closed proxy socket" );

                            if ( null != msgCounts ) {
                                for ( int ii = 0; ii < nameCount; ++ii ) {
                                    if ( msgCounts[ii] > 0 ) {
                                        String msg = 
                                            String.format("%d messages for %s",
                                                          msgCounts[ii], 
                                                          ids[ii] );
                                        Utils.logf( msg );
                                        DBUtils.setHasMsgs( ids[ii] );
                                    }
                                }
                                setupNotification();
                            }

                        } catch( java.net.UnknownHostException uhe ) {
                            Utils.logf( uhe.toString() );
                        } catch( java.io.IOException ioe ) {
                            Utils.logf( ioe.toString() );
                        } catch( NullPointerException npe ) {
                            Utils.logf( npe.toString() );
                        }
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

    private void setupNotification()
    {
        Notification notification = 
            new Notification( R.drawable.icon48x48, 
                              getString(R.string.notify_title),
                              System.currentTimeMillis());

        Intent intent = new Intent(this, GamesList.class);
        PendingIntent pi = PendingIntent.getActivity( this, 0, intent, 0);

        notification.setLatestEventInfo( this, "bazz", "bar", pi );
        
        m_nm.notify( R.string.running_notification, notification );
    }

    private String[] collectIDs( int[] nBytes )
    {
        Utils.logf( "collectIDs" );
        String[] ids = DBUtils.getRelayIDNoMsgs( this );

        int len = 0;
        for ( String id : ids ) {
            Utils.logf( "got relayID: %s", id );
            len += id.length();
        }
        nBytes[0] = len;
        return ids;
    }

}

