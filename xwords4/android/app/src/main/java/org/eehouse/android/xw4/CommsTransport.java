/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2012 by Eric House (xwords@eehouse.org).  All
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

import android.content.Context;


import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;
import org.eehouse.android.xw4.jni.JNIThread;
import org.eehouse.android.xw4.jni.JNIThread.JNICmd;
import org.eehouse.android.xw4.jni.TransportProcs;

import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.channels.ClosedChannelException;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.SocketChannel;
import java.nio.channels.UnresolvedAddressException;
import java.util.Iterator;
import java.util.Vector;

public class CommsTransport implements TransportProcs,
                                       NetStateCache.StateChangedIf {
    private static final String TAG = CommsTransport.class.getSimpleName();
    private Selector m_selector;
    private SocketChannel m_socketChannel;
    private CommsAddrRec m_relayAddr;
    private String m_useHost;
    private JNIThread m_jniThread;
    private CommsThread m_thread;
    private TransportProcs.TPMsgHandler m_tpHandler;
    private boolean m_done = false;

    private Vector<ByteBuffer> m_buffersOut;
    private ByteBuffer m_bytesOut;
    private ByteBuffer m_bytesIn;

    private Context m_context;
    private long m_rowid;

    // assembling inbound packet
    private byte[] m_packetIn;
    private int m_haveLen = -1;

    public CommsTransport( Context context, TransportProcs.TPMsgHandler handler,
                           long rowid, DeviceRole role )
    {
        m_context = context;
        m_tpHandler = handler;
        m_rowid = rowid;
        m_buffersOut = new Vector<>();
        m_bytesIn = ByteBuffer.allocate( 2048 );

        NetStateCache.register( context, this );
    }

    public class CommsThread extends Thread {

        @Override
        public void run()
        {
            if ( !BuildConfig.UDP_ENABLED ) {
                m_done = false;
                boolean failed = true;
                try {
                    if ( XWApp.onEmulator() ) {
                        System.setProperty("java.net.preferIPv6Addresses", "false");
                    }

                    m_selector = Selector.open();

                    failed = loop();

                    closeSocket();
                } catch ( java.io.IOException ioe ) {
                    Log.ex( TAG, ioe );
                } catch ( UnresolvedAddressException uae ) {
                    Log.w( TAG, "bad address: name: %s; port: %s; exception: %s",
                           m_useHost, m_relayAddr.ip_relay_port, uae.toString() );
                }

                m_thread = null;
                if ( failed ) {
                    m_jniThread.handle( JNICmd.CMD_TRANSFAIL,
                                        CommsConnType.COMMS_CONN_RELAY );
                }
            }
        }

        private boolean loop()
        {
            boolean failed = false;
            if ( !BuildConfig.UDP_ENABLED ) {
                outer_loop:
                while ( !m_done ) {
                    try {
                        synchronized( this ) {

                            // if we have data and no socket, try to connect.
                            if ( null == m_socketChannel
                                 && 0 < m_buffersOut.size() ) {
                                try {
                                    m_socketChannel = SocketChannel.open();
                                    m_socketChannel.configureBlocking( false );
                                    Log.i( TAG, "connecting to %s:%d",
                                           m_useHost, m_relayAddr.ip_relay_port );
                                    InetSocketAddress isa = new
                                        InetSocketAddress(m_useHost,
                                                          m_relayAddr.ip_relay_port );
                                    m_socketChannel.connect( isa );
                                } catch ( java.io.IOException ioe ) {
                                    Log.ex( TAG, ioe );
                                    failed = true;
                                    break outer_loop;
                                }
                            }

                            if ( null != m_socketChannel ) {
                                int ops = figureOps();
                                // DbgUtils.logf( "calling with ops=%x", ops );
                                m_socketChannel.register( m_selector, ops );
                            }
                        }
                        m_selector.select();
                    } catch ( ClosedChannelException cce ) {
                        // we get this when relay goes down.  Need to notify!
                        failed = true;
                        closeSocket();
                        Log.w( TAG, "exiting: %s", cce.toString() );
                        break;          // don't try again
                    } catch ( java.io.IOException ioe ) {
                        closeSocket();
                        Log.w( TAG, "exiting: %s", ioe.toString() );
                        Log.w( TAG, ioe.toString() );
                    } catch ( java.nio.channels.NoConnectionPendingException ncp ) {
                        Log.ex( TAG, ncp );
                        closeSocket();
                        break;
                    }

                    Iterator<SelectionKey> iter = m_selector.selectedKeys().iterator();
                    while ( iter.hasNext() ) {
                        SelectionKey key = iter.next();
                        SocketChannel channel = (SocketChannel)key.channel();
                        iter.remove();
                        try {
                            if (key.isValid() && key.isConnectable()) {
                                if ( !channel.finishConnect() ) {
                                    key.cancel();
                                }
                            }
                            if (key.isValid() && key.isReadable()) {
                                m_bytesIn.clear(); // will wipe any pending data!
                                // DbgUtils.logf( "socket is readable; buffer has space for "
                                //             + m_bytesIn.remaining() );
                                int nRead = channel.read( m_bytesIn );
                                if ( nRead == -1 ) {
                                    channel.close();
                                } else {
                                    addIncoming();
                                }
                                ConnStatusHandler.
                                    updateStatusIn( m_context,
                                                    CommsConnType.COMMS_CONN_RELAY,
                                                    0 <= nRead );
                            }
                            if (key.isValid() && key.isWritable()) {
                                getOut();
                                if ( null != m_bytesOut ) {
                                    int nWritten = channel.write( m_bytesOut );
                                    ConnStatusHandler.
                                        updateStatusOut( m_context,
                                                         CommsConnType.COMMS_CONN_RELAY,
                                                         0 < nWritten );
                                }
                            }
                        } catch ( java.io.IOException ioe ) {
                            Log.w( TAG, "%s: cancelling key", ioe.toString() );
                            key.cancel();
                            failed = true;
                            break outer_loop;
                        } catch ( java.nio.channels.
                                  NoConnectionPendingException ncp ) {
                            Log.ex( TAG, ncp );
                            break outer_loop;
                        }
                    }
                }
            }
            return failed;
        } // loop
    }

    public void setReceiver( JNIThread jnit )
    {
        m_jniThread = jnit;
    }

    public void waitToStop()
    {
        waitToStopImpl();
        NetStateCache.unregister( m_context, this );
    }

    //////////////////////////////////////////////////////////////////////
    // NetStateCache.StateChangedIf interface
    //////////////////////////////////////////////////////////////////////
    public void onNetAvail( boolean nowAvailable )
    {
        if ( !nowAvailable ) {
            waitToStopImpl();
            m_jniThread.handle( JNICmd.CMD_TRANSFAIL,
                                CommsConnType.COMMS_CONN_RELAY );
        }
    }

    private synchronized void putOut( final byte[] buf )
    {
        if ( !BuildConfig.UDP_ENABLED ) {
            int len = buf.length;
            ByteBuffer netbuf = ByteBuffer.allocate( len + 2 );
            netbuf.putShort( (short)len );
            netbuf.put( buf );
            m_buffersOut.add( netbuf );
            Assert.assertEquals( netbuf.remaining(), 0 );

            if ( null != m_selector ) {
                // tell it it's got some writing to do
                m_selector.wakeup(); // getting NPE inside here -- see below
            }
        }
    }

    private synchronized void closeSocket()
    {
        if ( !BuildConfig.UDP_ENABLED && null != m_socketChannel ) {
            try {
                m_socketChannel.close();
            } catch ( Exception e ) {
                Log.w( TAG, "closing socket: %s", e.toString() );
            }
            m_socketChannel = null;
        }
    }

    private synchronized void getOut()
    {
        if ( null != m_bytesOut && m_bytesOut.remaining() == 0 ) {
            m_bytesOut = null;
        }

        if ( null == m_bytesOut && m_buffersOut.size() > 0 ) {
            m_bytesOut = m_buffersOut.remove(0);
            m_bytesOut.flip();
        }
    }

    private synchronized int figureOps() {
        int ops;
        if ( null == m_socketChannel ) {
            ops = 0;
        } else if ( m_socketChannel.isConnected() ) {
            ops = SelectionKey.OP_READ;
            if ( (null != m_bytesOut && m_bytesOut.hasRemaining())
                 || m_buffersOut.size() > 0 ) {
                ops |= SelectionKey.OP_WRITE;
            }
        } else {
            ops = SelectionKey.OP_CONNECT;
        }
        return ops;
    }

    private void addIncoming( )
    {
        if ( !BuildConfig.UDP_ENABLED ) {
            m_bytesIn.flip();

            for ( ; ; ) {
                int len = m_bytesIn.remaining();
                if ( len <= 0 ) {
                    break;
                }

                if ( null == m_packetIn ) { // we're not mid-packet
                    Assert.assertTrue( len > 1 ); // tell me if I see this case
                    if ( len == 1 ) {       // half a length byte...
                        break;              // can I leave it in the buffer?
                    } else {
                        m_packetIn = new byte[m_bytesIn.getShort()];
                        m_haveLen = 0;
                    }
                } else {                    // we're mid-packet
                    int wantLen = m_packetIn.length - m_haveLen;
                    if ( wantLen > len ) {
                        wantLen = len;
                    }
                    m_bytesIn.get( m_packetIn, m_haveLen, wantLen );
                    m_haveLen += wantLen;
                    if ( m_haveLen == m_packetIn.length ) {
                        // send completed packet
                        m_jniThread.handle( JNICmd.CMD_RECEIVE, m_packetIn, null );
                        m_packetIn = null;
                    }
                }
            }
        }
    }

    private void waitToStopImpl()
    {
        if ( !BuildConfig.UDP_ENABLED ) {
            m_done = true;          // this is in a race!
            if ( null != m_selector ) {
                m_selector.wakeup(); // getting NPE inside here -- see below
            }
            if ( null != m_thread ) {     // synchronized this?  Or use Thread method
                try {
                    m_thread.join(100);   // wait up to 1/10 second
                } catch ( java.lang.InterruptedException ie ) {
                    Log.ex( TAG, ie );
                }
                m_thread = null;
            }
        }
    }

    // TransportProcs interface
    private static final boolean TRANSPORT_DOES_NOCONN = true;
    @Override
    public int getFlags() {
        return TRANSPORT_DOES_NOCONN ? COMMS_XPORT_FLAGS_HASNOCONN : COMMS_XPORT_FLAGS_NONE;
    }

    @Override
    public int transportSend( byte[] buf, String msgID, CommsAddrRec addr,
                              CommsConnType conType, int gameID )
    {
        Log.d( TAG, "transportSend(len=%d, typ=%s)", buf.length,
               conType.toString() );
        int nSent = -1;
        Assert.assertNotNull( addr );
        Assert.assertTrue( addr.contains( conType ) );

        if ( !BuildConfig.UDP_ENABLED && conType == CommsConnType.COMMS_CONN_RELAY
             && null == m_relayAddr ) {
            m_relayAddr = new CommsAddrRec( addr );
            m_useHost = NetUtils.forceHost( m_relayAddr.ip_relay_hostName );
        }

        if ( !BuildConfig.UDP_ENABLED && conType == CommsConnType.COMMS_CONN_RELAY ) {
            if ( NetStateCache.netAvail( m_context ) ) {
                putOut( buf );      // add to queue
                if ( null == m_thread ) {
                    m_thread = new CommsThread();
                    m_thread.start();
                }
                nSent = buf.length;
            }
        } else {
            nSent = sendForAddr( m_context, addr, conType, m_rowid, gameID,
                                 buf, msgID );
        }

        // Keep this while debugging why the resend_all that gets
        // fired on reconnect doesn't unstall a game but a manual
        // resend does.
        Log.d( TAG, "transportSend(len=%d, typ=%s) => %d", buf.length,
               conType, nSent );
        return nSent;
    }

    @Override
    public void relayConnd( String room, int devOrder, boolean allHere,
                            int nMissing )
    {
        // m_tpHandler.tpmRelayConnd( room, devOrder, allHere, nMissing );
    }

    @Override
    public void relayErrorProc( XWRELAY_ERROR relayErr )
    {
        m_tpHandler.tpmRelayErrorProc( relayErr );
    }

    @Override
    public boolean relayNoConnProc( byte[] buf, String msgID, String relayID )
    {
        Assert.assertTrue( TRANSPORT_DOES_NOCONN );
        int nSent = RelayService.sendNoConnPacket( m_context, m_rowid,
                                                   relayID, buf, msgID );
        boolean success = buf.length == nSent;
        Log.d( TAG, "relayNoConnProc(msgID=%s, len=%d) => %b", msgID,
               buf.length, success );
        return success;
    }

    @Override
    public void countChanged( int newCount )
    {
        m_tpHandler.tpmCountChanged( newCount );
    }

    private int sendForAddr( Context context, CommsAddrRec addr,
                             CommsConnType conType, long rowID,
                             int gameID, byte[] buf, String msgID )
    {
        int nSent = -1;
        switch ( conType ) {
        case COMMS_CONN_RELAY:
            Assert.assertTrue( BuildConfig.UDP_ENABLED );
            nSent = RelayService.sendPacket( context, rowID, buf, msgID );
            break;
        case COMMS_CONN_SMS:
            nSent = NBSProto.sendPacket( context, addr.sms_phone,
                                         gameID, buf, msgID );
            break;
        case COMMS_CONN_BT:
            nSent = BTUtils.sendPacket( context, buf, msgID, addr, gameID );
            break;
        case COMMS_CONN_P2P:
            nSent = WiDirService
                .sendPacket( context, addr.p2p_addr, gameID, buf );
            break;
        case COMMS_CONN_NFC:
            nSent = NFCUtils.addMsgFor( buf, gameID );
            break;
        case COMMS_CONN_MQTT:
            nSent = MQTTUtils.send( context, addr.mqtt_devID, gameID, buf );
            break;
        default:
            Assert.failDbg();
            break;
        }
        Log.d( TAG, "sendForAddr(typ=%s, len=%d) => %d", conType,
               buf.length, nSent );
        return nSent;
    }

    /* NPEs in m_selector calls: sometimes my Moment gets into a state
     * where after 15 or so seconds of Crosswords trying to connect to
     * the relay I get a crash.  Logs show it's inside one or both of
     * these Selector methods.  Rebooting the device gets it out of
     * that state, so I suspect it's a but in 2.1 or Samsung's build
     * of it.  Should watch crash reports at developer.android.com and
     * perhaps catch NPEs here just to be safe.  But then do what?
     * Tell user to restart device?
     */
}
