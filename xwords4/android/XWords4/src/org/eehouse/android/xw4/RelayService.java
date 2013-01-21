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
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.Socket;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.concurrent.LinkedBlockingQueue;

import junit.framework.Assert;

import org.eehouse.android.xw4.jni.GameSummary;
import org.eehouse.android.xw4.jni.UtilCtxt;
import org.eehouse.android.xw4.MultiService.MultiEvent;

public class RelayService extends XWService {
    private static final int MAX_SEND = 1024;
    private static final int MAX_BUF = MAX_SEND - 2;

    private static final String CMD_STR = "CMD";
    private static final int UDP_CHANGED = 1;

    private Thread m_fetchThread = null;
    private Thread m_UDPReadThread = null;
    private Thread m_UDPWriteThread = null;
    private DatagramSocket m_UDPSocket;
    private LinkedBlockingQueue<DatagramPacket> m_queue = null;

    // These must match the enum XWRelayReg in xwrelay.h
    private static final int XWPDEV_PROTO_VERSION = 0;
    // private static final int XWPDEV_NONE = 0;
    private static final int XWPDEV_ALERT = 1;
    private static final int XWPDEV_REG = 2;

    public static void startService( Context context )
    {
        Intent intent = getIntentTo( context, UDP_CHANGED );
        context.startService( intent );
    }

    public static void udpChanged( Context context )
    {
        startService( context );
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
        startFetchThreadIf();
    }

    @Override
    public int onStartCommand( Intent intent, int flags, int startId )
    {
        DbgUtils.logf( "RelayService::onStartCommand" );
        int result;
        if ( null != intent ) {
            int cmd = intent.getIntExtra( CMD_STR, -1 );
            switch( cmd ) {
            case UDP_CHANGED:
                DbgUtils.logf( "RelayService::onStartCommand::UDP_CHANGED" );
                if ( XWPrefs.getUDPEnabled( this ) ) {
                    stopFetchThreadIf();
                    startUDPThreads();
                    registerWithRelay();
                } else {
                    stopUDPThreadsIf();
                    startFetchThreadIf();
                }
                break;
            default:
                Assert.fail();
            }

            result = Service.START_STICKY;
        } else {
            result = Service.START_STICKY_COMPATIBILITY;
        }
        return result;
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
    
    private void startFetchThreadIf()
    {
        DbgUtils.logf( "startFetchThreadIf()" );
        if ( !XWPrefs.getUDPEnabled( this ) && null == m_fetchThread ) {
            m_fetchThread = new Thread( null, new Runnable() {
                    public void run() {
                        fetchAndProcess();
                        m_fetchThread = null;
                        RelayService.this.stopSelf();
                    }
                }, getClass().getName() );
            m_fetchThread.start();
        }
    }

    private void stopFetchThreadIf()
    {
        if ( null != m_fetchThread ) {
            DbgUtils.logf( "2: m_fetchThread NOT NULL; WHAT TO DO???" );
        }
    }

    private void startUDPThreads()
    {
        DbgUtils.logf( "startUDPThreads" );
        Assert.assertNull( m_UDPWriteThread );
        Assert.assertNull( m_UDPReadThread );
        Assert.assertTrue( XWPrefs.getUDPEnabled( this ) );

        int port = XWPrefs.getDefaultRelayPort( RelayService.this );
        String host = XWPrefs.getDefaultRelayHost( RelayService.this );
        try { 
            m_UDPSocket = new DatagramSocket();
            InetAddress addr = InetAddress.getByName( host );
            m_UDPSocket.connect( addr, port );
        } catch( java.net.SocketException se ) {
            DbgUtils.loge( se );
            Assert.fail();
        } catch( java.net.UnknownHostException uhe ) {
            DbgUtils.loge( uhe );
        }

        m_UDPReadThread = new Thread( null, new Runnable() {
                public void run() {
                    byte[] buf = new byte[1024];
                    for ( ; ; ) {
                        DatagramPacket packet = 
                            new DatagramPacket( buf, buf.length );
                        try {
                            DbgUtils.logf( "UPD read thread blocking on receive" );
                            m_UDPSocket.receive( packet );
                            DbgUtils.logf( "UPD read thread: receive returned" );
                        } catch( java.io.IOException ioe ) {
                            DbgUtils.loge( ioe );
                            break; // ???
                        }
                        DbgUtils.logf( "received %d bytes", packet.getLength() );
                        gotPacket( packet );
                    }
                }
            }, getClass().getName() );
        m_UDPReadThread.start();

        m_queue = new LinkedBlockingQueue<DatagramPacket>();
        m_UDPWriteThread = new Thread( null, new Runnable() {
                public void run() {
                    for ( ; ; ) {
                        DatagramPacket outPacket;
                        try {
                            outPacket = m_queue.take();
                        } catch ( InterruptedException ie ) {
                            DbgUtils.logf( "RelayService; write thread killed" );
                            break;
                        }
                        if ( null == outPacket || 0 == outPacket.getLength() ) {
                            DbgUtils.logf( "stopping write thread" );
                            break;
                        }
                        DbgUtils.logf( "Sending packet of length %d", 
                                       outPacket.getLength() );
                        try {
                            m_UDPSocket.send( outPacket );
                        } catch ( java.io.IOException ioe ) {
                            DbgUtils.loge( ioe );
                        }
                    }
                }
            }, getClass().getName() );
        m_UDPWriteThread.start();
    }

    private void stopUDPThreadsIf()
    {
        DbgUtils.logf( "stopUDPThreadsIf" );
        if ( null != m_queue && null != m_UDPWriteThread ) {
            // can't add null
            m_queue.add( new DatagramPacket( new byte[0], 0 ) );
            try {
                DbgUtils.logf( "joining m_UDPWriteThread" );
                m_UDPWriteThread.join();
                DbgUtils.logf( "SUCCESSFULLY joined m_UDPWriteThread" );
            } catch( java.lang.InterruptedException ie ) {
                DbgUtils.loge( ie );
            }
            m_UDPWriteThread = null;
            m_queue = null;
        }
        if ( null != m_UDPSocket && null != m_UDPReadThread ) {
            m_UDPSocket.close();
            DbgUtils.logf( "waiting for read thread to exit" );
            try {
                m_UDPReadThread.join();
            } catch( java.lang.InterruptedException ie ) {
                DbgUtils.loge( ie );
            }
            DbgUtils.logf( "read thread exited" );
            m_UDPReadThread = null;
            m_UDPSocket = null;
        }
        DbgUtils.logf( "stopUDPThreadsIf DONE" );
    }

    private void gotPacket( DatagramPacket packet )
    {
        DbgUtils.logf( "gotPacket" );
        ByteArrayInputStream bis = new ByteArrayInputStream( packet.getData() );
        DataInputStream dis = new DataInputStream( bis );
        try {
            byte proto = dis.readByte();
            if ( XWPDEV_PROTO_VERSION == proto ) {
                byte cmd = dis.readByte();
                switch ( cmd ) { 
                case XWPDEV_ALERT:
                    short len = dis.readShort();
                    byte[] tmp = new byte[len];
                    dis.read( tmp );
                    sendResult( MultiEvent.RELAY_ALERT, new String( tmp ) );
                    break;
                default:
                    DbgUtils.logf( "RelayService: Unhandled cmd: %d", cmd );
                    break;
                }
            } else {
                DbgUtils.logf( "bad proto %d", proto );
            }
        } catch ( java.io.IOException ioe ) {
            DbgUtils.loge( ioe );
        }
    }

    private void registerWithRelay()
    {
        byte typ;
        String devid = XWPrefs.getRelayDevID( this );
        if ( null != devid && 0 < devid.length() ) {
            typ = UtilCtxt.ID_TYPE_RELAY;
        } else {
            devid = XWPrefs.getGCMDevID( this );
            if ( null != devid && 0 < devid.length() ) {
                typ = UtilCtxt.ID_TYPE_ANDROID_GCM;
            } else {
                devid = "DO NOT SHIP WITH ME";
                typ = UtilCtxt.ID_TYPE_ANDROID_OTHER;
            }
        }

        ByteArrayOutputStream bas = new ByteArrayOutputStream();
        DataOutputStream outBuf = new DataOutputStream( bas );
        try {
            outBuf.writeByte( XWPDEV_PROTO_VERSION );
            outBuf.writeByte( XWPDEV_REG );
            outBuf.writeByte( typ );
            outBuf.writeShort( devid.length() );
            outBuf.writeBytes( devid );

            byte[] data = bas.toByteArray();
            m_queue.add( new DatagramPacket( data, data.length ) );
        } catch ( java.io.IOException ioe ) {
            DbgUtils.loge( ioe );
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

    private static void sendToRelay( Context context,
                                     HashMap<String,ArrayList<byte[]>> msgHash )
    {
        // format: total msg lenth: 2
        //         number-of-relayIDs: 2
        //         for-each-relayid: relayid + '\n': varies
        //                           message count: 1
        //                           for-each-message: length: 2
        //                                             message: varies

        if ( null != msgHash ) {
            try {
                // Build up a buffer containing everything but the total
                // message length and number of relayIDs in the message.
                ByteArrayOutputStream store = 
                    new ByteArrayOutputStream( MAX_BUF ); // mem
                DataOutputStream outBuf = new DataOutputStream( store );
                int msgLen = 4;          // relayID count + protocol stuff
                int nRelayIDs = 0;
        
                Iterator<String> iter = msgHash.keySet().iterator();
                while ( iter.hasNext() ) {
                    String relayID = iter.next();
                    int thisLen = 1 + relayID.length(); // string and '\n'
                    thisLen += 2;                        // message count

                    ArrayList<byte[]> msgs = msgHash.get( relayID );
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
                Socket socket = NetUtils.makeProxySocket( context, 8000 );
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
