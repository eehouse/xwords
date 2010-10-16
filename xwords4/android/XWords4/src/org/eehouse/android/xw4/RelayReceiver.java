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
        int[] nBytes = new int[1];
        String[] ids = collectIDs( context, nBytes );
        if ( null != ids && 0 < ids.length ) {
            try {
                Socket socket = 
                    NetUtils.MakeProxySocket( context, 8000 );
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

                if ( null == msgCounts ) {
                    Utils.logf( "relay has no messages" );
                } else {
                    ArrayList<String> idsWMsgs =
                        new ArrayList<String>( nameCount );
                    for ( int ii = 0; ii < nameCount; ++ii ) {
                        if ( msgCounts[ii] > 0 ) {
                            String msg = 
                                String.format("%d messages for %s",
                                              msgCounts[ii], 
                                              ids[ii] );
                            Utils.logf( msg );
                            DBUtils.setHasMsgs( ids[ii] );
                            idsWMsgs.add( ids[ii] );
                        }
                    }
                    ids = new String[idsWMsgs.size()];
                    setupNotification( context, idsWMsgs.toArray( ids ) );
                }

            } catch( java.net.UnknownHostException uhe ) {
                Utils.logf( uhe.toString() );
            } catch( java.io.IOException ioe ) {
                Utils.logf( ioe.toString() );
            } catch( NullPointerException npe ) {
                Utils.logf( npe.toString() );
            }
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

    private String[] collectIDs( Context context, int[] nBytes )
    {
        String[] ids = DBUtils.getRelayIDNoMsgs( context );

        int len = 0;
        for ( String id : ids ) {
            Utils.logf( "got relayID: %s", id );
            len += id.length();
        }
        nBytes[0] = len;
        return ids;
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