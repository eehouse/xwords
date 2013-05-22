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
import android.os.AsyncTask;
import android.os.IBinder;
import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.net.Socket;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;

import org.eehouse.android.xw4.jni.GameSummary;
import org.eehouse.android.xw4.jni.XwJNI;

public class RelayService extends Service {
    private static final int MAX_SEND = 1024;
    private static final int MAX_BUF = MAX_SEND - 2;

    private static final String CMD_STR = "CMD";
    private static final int PROCESS_MSG = 1;
    private static final String MSG = "MSG";
    private static final String RELAY_ID = "RELAY_ID";

    public static void processMsg( Context context, String relayId, 
                                   String msg64 )
    {
        byte[] msg = XwJNI.base64Decode( msg64 );
        Intent intent = getIntentTo( context, PROCESS_MSG )
            .putExtra( MSG, msg )
            .putExtra( RELAY_ID, relayId );
        context.startService( intent );
    }

    private static Intent getIntentTo( Context context, int cmd )
    {
        Intent intent = new Intent( context, RelayService.class );
        intent.putExtra( CMD_STR, cmd );
        return intent;
    }

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

    @Override
    public int onStartCommand( Intent intent, int flags, int startId )
    {
        int cmd = intent.getIntExtra( CMD_STR, -1 );
        switch( cmd ) {
        case PROCESS_MSG:
            String[] relayIDs = new String[1];
            relayIDs[0] = intent.getStringExtra( RELAY_ID );
            long[] rowIDs = DBUtils.getRowIDsFor( this, relayIDs[0] );
            if ( 0 < rowIDs.length ) {
                byte[][][] msgs = new byte[1][1][1];
                msgs[0][0] = intent.getByteArrayExtra( MSG );
                process( msgs, rowIDs, relayIDs );
            }
            break;
        }
        stopSelf( startId );
        return Service.START_NOT_STICKY;
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
            byte[][][] msgs = NetUtils.queryRelay( this, relayIDs );
            process( msgs, rowIDss[0], relayIDs );
        }
    }

    private void process( byte[][][] msgs, long[] rowIDs, String[] relayIDs )
    {
        if ( null != msgs ) {
            RelayMsgSink sink = new RelayMsgSink();
            int nameCount = relayIDs.length;
            ArrayList<String> idsWMsgs = new ArrayList<String>( nameCount );

            for ( int ii = 0; ii < nameCount; ++ii ) {
                byte[][] forOne = msgs[ii];

                // if game has messages, open it and feed 'em to it.
                if ( null == forOne ) {
                    // Nothing for this relayID
                } else if ( BoardActivity.feedMessages( rowIDs[ii], forOne )
                            || GameUtils.feedMessages( this, rowIDs[ii],
                                                       forOne, null,
                                                       sink ) ) {
                    idsWMsgs.add( relayIDs[ii] );
                } else {
                    DbgUtils.logf( "message for %s (rowid %d) not consumed",
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

    private static class AsyncSender extends AsyncTask<Void, Void, Void> {
        private Context m_context;
        private HashMap<String,ArrayList<byte[]>> m_msgHash;

        public AsyncSender( Context context, 
                            HashMap<String,ArrayList<byte[]>> msgHash )
        {
            m_context = context;
            m_msgHash = msgHash;
        }

        @Override
        protected Void doInBackground( Void... ignored )
        {
            // format: total msg lenth: 2
            //         number-of-relayIDs: 2
            //         for-each-relayid: relayid + '\n': varies
            //                           message count: 1
            //                           for-each-message: length: 2
            //                                             message: varies

            // Build up a buffer containing everything but the total
            // message length and number of relayIDs in the message.
            try {
                ByteArrayOutputStream store = 
                    new ByteArrayOutputStream( MAX_BUF ); // mem
                DataOutputStream outBuf = new DataOutputStream( store );
                int msgLen = 4;          // relayID count + protocol stuff
                int nRelayIDs = 0;
        
                Iterator<String> iter = m_msgHash.keySet().iterator();
                while ( iter.hasNext() ) {
                    String relayID = iter.next();
                    int thisLen = 1 + relayID.length(); // string and '\n'
                    thisLen += 2;                        // message count

                    ArrayList<byte[]> msgs = m_msgHash.get( relayID );
                    for ( byte[] msg : msgs ) {
                        thisLen += 2 + msg.length;
                    }

                    if ( msgLen + thisLen > MAX_BUF ) {
                        // Need to deal with this case by sending multiple
                        // packets.  It WILL happen.
                        break;
                    }
                    // got space; now write it
                    ++nRelayIDs;
                    outBuf.writeBytes( relayID );
                    outBuf.write( '\n' );
                    outBuf.writeShort( msgs.size() );
                    for ( byte[] msg : msgs ) {
                        outBuf.writeShort( msg.length );
                        outBuf.write( msg );
                    }
                    msgLen += thisLen;
                }
                // Now open a real socket, write size and proto, and
                // copy in the formatted buffer
                Socket socket = NetUtils.makeProxySocket( m_context, 8000 );
                if ( null != socket ) {
                    DataOutputStream outStream = 
                        new DataOutputStream( socket.getOutputStream() );
                    outStream.writeShort( msgLen );
                    outStream.writeByte( NetUtils.PROTOCOL_VERSION );
                    outStream.writeByte( NetUtils.PRX_PUT_MSGS );
                    outStream.writeShort( nRelayIDs );
                    outStream.write( store.toByteArray() );
                    outStream.flush();
                    socket.close();
                }
            } catch ( java.io.IOException ioe ) {
                DbgUtils.loge( ioe );
            }
            return null;
        } // doInBackground
    }

    private static void sendToRelay( Context context,
                                     HashMap<String,ArrayList<byte[]>> msgHash )
    {
        if ( null != msgHash ) {
            new AsyncSender( context, msgHash ).execute();
        } else {
            DbgUtils.logf( "sendToRelay: null msgs" );
        }
    } // sendToRelay

    private class RelayMsgSink extends MultiMsgSink {

        private HashMap<String,ArrayList<byte[]>> m_msgLists = null;

        public void send( Context context )
        {
            sendToRelay( context, m_msgLists );
        }

        /***** TransportProcs interface *****/

        public boolean relayNoConnProc( byte[] buf, String relayID )
        {
            if ( null == m_msgLists ) {
                m_msgLists = new HashMap<String,ArrayList<byte[]>>();
            }

            ArrayList<byte[]> list = m_msgLists.get( relayID );
            if ( list == null ) {
                list = new ArrayList<byte[]>();
                m_msgLists.put( relayID, list );
            }
            list.add( buf );

            return true;
        }
    }

}
