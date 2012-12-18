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
                    Intent intent = 
                        GamesList.makeRelayIdsIntent( this,
                                                      new String[] {relayID} );
                    String msg = Utils.format( this, R.string.notify_bodyf, 
                                               GameUtils.getName( this, rowid ) );
                    Utils.postNotification( this, intent, R.string.notify_title,
                                            msg, (int)rowid );
                }
            }
        }
    }
    
    private void fetchAndProcess()
    {
        long[][] rowIDss = new long[1][];
        String[] relayIDs = DBUtils.getRelayIDs( this, rowIDss );
        if ( null != relayIDs && 0 < relayIDs.length ) {
            long[] rowIDs = rowIDss[0];
            byte[][][] msgs = NetUtils.queryRelay( this, relayIDs );

            if ( null != msgs ) {
                RelayMsgSink sink = new RelayMsgSink();
                int nameCount = relayIDs.length;
                ArrayList<String> idsWMsgs =
                    new ArrayList<String>( nameCount );
                for ( int ii = 0; ii < nameCount; ++ii ) {
                    byte[][] forOne = msgs[ii];
                    // if game has messages, open it and feed 'em
                    // to it.
                    if ( null == forOne ) {
                        // Nothing for this relayID
                    } else if ( BoardActivity.feedMessages( rowIDs[ii], forOne )
                                || GameUtils.feedMessages( this, rowIDs[ii],
                                                           forOne, null,
                                                           sink ) ) {
                        idsWMsgs.add( relayIDs[ii] );
                    } else {
                        DbgUtils.logf( "dropping message for %s (rowid %d)",
                                       relayIDs[ii], rowIDs[ii] );
                    }
                }
                if ( 0 < idsWMsgs.size() ) {
                    String[] tmp = new String[idsWMsgs.size()];
                    idsWMsgs.toArray( tmp );
                    setupNotification( tmp );
                }
                sink.send( this );
            }
        }
    }

}
