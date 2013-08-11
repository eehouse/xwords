/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
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

import java.nio.channels.Selector;
import java.nio.channels.SocketChannel;
import java.nio.channels.SelectionKey;
import java.nio.channels.ClosedChannelException;
import java.nio.channels.UnresolvedAddressException;
import java.nio.ByteBuffer;
import java.net.InetSocketAddress;
import java.util.Vector;
import java.util.Iterator;
import junit.framework.Assert;
import android.content.Context;
import android.os.Build;
import android.os.Handler;
import android.os.Message;

import org.eehouse.android.xw4.jni.*;
import org.eehouse.android.xw4.jni.JNIThread.*;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;

public class CommsTransport implements TransportProcs, 
                                       NetStateCache.StateChangedIf {
    private Selector m_selector;
    private SocketChannel m_socketChannel;
    private int m_jniGamePtr;
    private CommsAddrRec m_relayAddr;
    private JNIThread m_jniThread;
    private CommsThread m_thread;
    private TransportProcs.TPMsgHandler m_tpHandler;
    private Handler m_handler;
    private boolean m_done = false;

    private Vector<ByteBuffer> m_buffersOut;
    private ByteBuffer m_bytesOut;
    private ByteBuffer m_bytesIn;

    private Context m_context;
    private long m_rowid;

    // assembling inbound packet
    private byte[] m_packetIn;
    private int m_haveLen = -1;

    public CommsTransport( int jniGamePtr, Context context, 
                           TransportProcs.TPMsgHandler handler,
                           long rowid, DeviceRole role )
    {
        m_jniGamePtr = jniGamePtr;
        m_context = context;
        m_tpHandler = handler;
        m_rowid = rowid;
        m_buffersOut = new Vector<ByteBuffer>();
        m_bytesIn = ByteBuffer.allocate( 2048 );

        NetStateCache.register( context, this );
    }

    public class CommsThread extends Thread {

        @Override
        public void run()
        {
            m_done = false;
            boolean failed = true;
            try {   
                if ( Build.PRODUCT.contains("sdk") ) {
                    System.setProperty("java.net.preferIPv6Addresses", "false");
                }

                m_selector = Selector.open();

                failed = loop();

                closeSocket();
            } catch ( java.io.IOException ioe ) {
                DbgUtils.loge( ioe );
            } catch ( UnresolvedAddressException uae ) {
                DbgUtils.logf( "bad address: name: %s; port: %s; exception: %s",
                               m_relayAddr.ip_relay_hostName, 
                               m_relayAddr.ip_relay_port, 
                               uae.toString() );
            }

            m_thread = null;
            if ( failed ) {
                m_jniThread.handle( JNICmd.CMD_TRANSFAIL );
            }
        }

        private boolean loop()
        {
            boolean failed = false;
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
                                DbgUtils.logf( "connecting to %s:%d",
                                               m_relayAddr.ip_relay_hostName, 
                                               m_relayAddr.ip_relay_port );
                                InetSocketAddress isa = new 
                                    InetSocketAddress(m_relayAddr.ip_relay_hostName,
                                                      m_relayAddr.ip_relay_port );
                                m_socketChannel.connect( isa );
                            } catch ( java.io.IOException ioe ) {
                                DbgUtils.loge( ioe );
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
                    DbgUtils.logf( "exiting: %s", cce.toString() );
                    break;          // don't try again
                } catch ( java.io.IOException ioe ) {
                    closeSocket();
                    DbgUtils.logf( "exiting: %s", ioe.toString() );
                    DbgUtils.logf( ioe.toString() );
                } catch ( java.nio.channels.NoConnectionPendingException ncp ) {
                    DbgUtils.loge( ncp );
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
                                updateStatusIn( m_context, null,
                                                CommsConnType.COMMS_CONN_RELAY, 
                                                0 <= nRead );
                        }
                        if (key.isValid() && key.isWritable()) {
                            getOut();
                            if ( null != m_bytesOut ) {
                                int nWritten = channel.write( m_bytesOut );
                                ConnStatusHandler.
                                    updateStatusOut( m_context, null,
                                                     CommsConnType.COMMS_CONN_RELAY,
                                                     0 < nWritten );
                            }
                        }
                    } catch ( java.io.IOException ioe ) {
                        DbgUtils.logf( "%s: cancelling key", ioe.toString() );
                        key.cancel(); 
                        failed = true;
                        break outer_loop;
                    } catch ( java.nio.channels.
                              NoConnectionPendingException ncp ) {
                        DbgUtils.loge( ncp );
                        break outer_loop;
                    }
                }
            }
            return failed;
        } // loop
    }
    
    public void setReceiver( JNIThread jnit, Handler handler )
    {
        m_jniThread = jnit;
        m_handler = handler;
    }

    public void waitToStop()
    {
        waitToStopImpl();
        NetStateCache.unregister( m_context, this );
    }

    // NetStateCache.StateChangedIf interface
    public void netAvail( boolean nowAvailable )
    {
        if ( !nowAvailable ) {
            waitToStopImpl();
            m_jniThread.handle( JNICmd.CMD_TRANSFAIL );
        }
    }

    public void tickle( CommsConnType connType )
    {
        switch( connType ) {
        case COMMS_CONN_RELAY:
            // do nothing
            // break;     // Try skipping the resend -- later
        case COMMS_CONN_BT:
        case COMMS_CONN_SMS:
            // Let other know I'm here
            DbgUtils.logf( "tickle calling comms_resendAll" );
            m_jniThread.handle( JNIThread.JNICmd.CMD_RESEND, false, true );
            break;
        default:
            DbgUtils.logf( "tickle: unexpected type %s", 
                           connType.toString() );
            Assert.fail();
        }
    }

    private synchronized void putOut( final byte[] buf )
    {
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

    private synchronized void closeSocket()
    {
        if ( null != m_socketChannel ) {
            try {
                m_socketChannel.close();
            } catch ( Exception e ) {
                DbgUtils.logf( "closing socket: %s", e.toString() );
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

    private void waitToStopImpl()
    {
        m_done = true;          // this is in a race!
        if ( null != m_selector ) {
            m_selector.wakeup(); // getting NPE inside here -- see below
        }
        if ( null != m_thread ) {     // synchronized this?  Or use Thread method
            try {
                m_thread.join(100);   // wait up to 1/10 second
            } catch ( java.lang.InterruptedException ie ) {
                DbgUtils.loge( ie );
            }
            m_thread = null;
        }
    }

    // TransportProcs interface

    public int getFlags() { return COMMS_XPORT_FLAGS_NONE; }

    public int transportSend( byte[] buf, final CommsAddrRec faddr, int gameID )
    {
        int nSent = -1;
        CommsAddrRec addr;
        if ( null == faddr ) {
            DbgUtils.logf( "Do this in the JNI!!" );
            addr = new CommsAddrRec();
            XwJNI.comms_getAddr( m_jniGamePtr, addr );
        } else {
            addr = faddr;
        }

        if ( null == m_relayAddr ) {
            m_relayAddr = new CommsAddrRec( addr );
        }

        switch ( addr.conType ) {
        case COMMS_CONN_RELAY:
            if ( XWPrefs.getUDPEnabled( m_context ) ) {
                nSent = RelayService.sendPacket( m_context, m_rowid, buf );
            } else {
                if ( NetStateCache.netAvail( m_context ) ) {
                    putOut( buf );      // add to queue
                    if ( null == m_thread ) {
                        m_thread = new CommsThread();
                        m_thread.start();
                    }
                    nSent = buf.length;
                }
            }
            break;
        case COMMS_CONN_SMS:
            nSent = SMSService.sendPacket( m_context, addr.sms_phone, 
                                           gameID, buf );
            break;
        case COMMS_CONN_BT:
            nSent = BTService.enqueueFor( m_context, buf, addr.bt_hostName, 
                                          addr.bt_btAddr, gameID );
            break;
        default:
            Assert.fail();
            break;
        }

        // Keep this while debugging why the resend_all that gets
        // fired on reconnect doesn't unstall a game but a manual
        // resend does.
        DbgUtils.logf( "transportSend(%d)=>%d", buf.length, nSent );
        return nSent;
    }

    public void relayStatus( CommsRelayState newState )
    {
        DbgUtils.logf( "relayStatus called; state=%s", newState.toString() );
        
        switch( newState ) {
        case COMMS_RELAYSTATE_UNCONNECTED:
        case COMMS_RELAYSTATE_DENIED:
        case COMMS_RELAYSTATE_CONNECT_PENDING:
            ConnStatusHandler.updateStatusOut( m_context, null,
                                               CommsConnType.COMMS_CONN_RELAY, 
                                               false );
            ConnStatusHandler.updateStatusIn( m_context, null,
                                              CommsConnType.COMMS_CONN_RELAY, 
                                              false );
            break;
        case COMMS_RELAYSTATE_CONNECTED: 
        case COMMS_RELAYSTATE_RECONNECTED: 
            ConnStatusHandler.updateStatusOut( m_context, null,
                                               CommsConnType.COMMS_CONN_RELAY, 
                                               true );
            break;
        case COMMS_RELAYSTATE_ALLCONNECTED:
            ConnStatusHandler.updateStatusIn( m_context, null,
                                              CommsConnType.COMMS_CONN_RELAY, 
                                              true );
            break;
        }
    }

    public void relayConnd( String room, int devOrder, boolean allHere, 
                            int nMissing )
    {
        m_tpHandler.tpmRelayConnd( room, devOrder, allHere, nMissing );
    }

    public void relayErrorProc( XWRELAY_ERROR relayErr )
    {
        m_tpHandler.tpmRelayErrorProc( relayErr );
    }

    public boolean relayNoConnProc( byte[] buf, String relayID )
    {
        Assert.fail();
        return false;
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
