/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2010 - 2012 by Eric House (xwords@eehouse.org).  All
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

    @Override
    public void onCreate()
    {
        super.onCreate();
        
        Thread thread = new Thread( null, new Runnable() {
                public void run() {
                    fetchAndProcess();
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
        for ( String relayID : relayIDs ) {
            long[] rowids = DBUtils.getRowIDsFor( this, relayID );
            if ( null != rowids ) {
                for ( long rowid : rowids ) {
                    Intent intent = new Intent( this, DispatchNotify.class );
                    intent.putExtra( DispatchNotify.RELAYIDS_EXTRA, 
                                     new String[] {relayID} );
                    String msg = Utils.format( this, R.string.notify_bodyf, 
                                               GameUtils.getName( this, rowid ) );
                    Utils.postNotification( this, intent, R.string.notify_title,
                                            msg, relayID.hashCode() );
                }
            }
        }
    }

    private String[] collectIDs( int[] nBytes )
    {
        String[] ids = DBUtils.getRelayIDs( this, false );
        int len = 0;
        if ( null != ids ) {
            for ( String id : ids ) {
                len += id.length();
            }
        }
        nBytes[0] = len;
        return ids;
    }
    
    private void fetchAndProcess()
    {
        int[] nBytes = new int[1];
        String[] ids = collectIDs( nBytes );
        if ( null != ids && 0 < ids.length ) {
            RelayMsgSink sink = new RelayMsgSink();
            byte[][][] msgs =
                NetUtils.queryRelay( this, ids, nBytes[0] );

            if ( null != msgs ) {
                int nameCount = ids.length;
                ArrayList<String> idsWMsgs =
                    new ArrayList<String>( nameCount );
                for ( int ii = 0; ii < nameCount; ++ii ) {
                    // if game has messages, open it and feed 'em
                    // to it.
                    if ( GameUtils.feedMessages( this, ids[ii], 
                                                 msgs[ii], sink ) ) {
                        idsWMsgs.add( ids[ii] );
                    }
                }
                if ( 0 < idsWMsgs.size() ) {
                    String[] relayIDs = new String[idsWMsgs.size()];
                    idsWMsgs.toArray( relayIDs );
                    if ( !DispatchNotify.tryHandle( relayIDs ) ) {
                        setupNotification( relayIDs );
                    }
                }
                sink.send( this );
            }
        }
    }

}
