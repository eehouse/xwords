/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2016 by Eric House (xwords@eehouse.org).  All rights reserved.
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

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.net.Socket;
import java.util.concurrent.LinkedBlockingQueue;
import junit.framework.Assert;

public class BiDiSockWrap {
    interface Iface {
        void gotPacket( BiDiSockWrap socket, byte[] bytes );
        void onSocketClosed( BiDiSockWrap socket );
    }
    private Socket mSocket;
    private Iface mIface;
    private LinkedBlockingQueue<byte[]> mQueue;
    private Thread mReadThread;
    private Thread mWriteThread;
    private boolean mRunThreads;

    // For sockets that came from accept() on a ServerSocket
    public BiDiSockWrap( Socket socket, Iface iface )
    {
        init( socket, iface );
    }

    // For creating sockets that will connect to a remote ServerSocket
    public BiDiSockWrap( String address, int port, Iface iface )
    {
        Socket socket = null;
        try {
            socket = new Socket( address, port );
        } catch ( java.net.UnknownHostException uhe ) {
            Assert.fail();
        } catch ( IOException uhe ) {
            Assert.fail();
        }
        if ( null != socket ) {
            init( socket, iface );
        }
    }

    public Socket getSocket() { return mSocket; }

    public void send( byte[] packet )
    {
        Assert.assertNotNull( packet );
        mQueue.add(packet);
    }

    private void init( Socket socket, Iface iface )
    {
        mSocket = socket;
        mIface = iface;
        mQueue = new LinkedBlockingQueue<byte[]>();
        startThreads();
    }

    private void closeSocket()
    {
        mRunThreads = false;
        try {
            mSocket.close();
        } catch ( IOException ioe ) {
            DbgUtils.logex( ioe );
        }
        mIface.onSocketClosed( this );
        send( new byte[0] );
    }

    private void startThreads()
    {
        mRunThreads = true;
        mWriteThread = new Thread( new Runnable() {
                @Override
                public void run() {
                    try {
                        DataOutputStream outStream
                            = new DataOutputStream( mSocket.getOutputStream() );
                        while ( mRunThreads ) {
                            byte[] packet = mQueue.take();
                            DbgUtils.logd( getClass(), "write thread got packet of len %d",
                                           packet.length );
                            if ( null == packet ) {
                                closeSocket();
                                break;
                            }

                            outStream.writeShort( packet.length );
                            outStream.write( packet, 0, packet.length );
                        }
                    } catch ( IOException ioe ) {
                        Assert.fail();
                    } catch ( InterruptedException ie ) {
                        Assert.fail();
                    }
                }
            } );
        mWriteThread.start();

        mReadThread = new Thread( new Runnable() {
                @Override
                public void run() {
                    try {
                        DataInputStream inStream
                            = new DataInputStream( mSocket.getInputStream() );
                        while ( mRunThreads ) {
                            short len = inStream.readShort();
                            byte[] packet = new byte[len];
                            inStream.read( packet );
                        }
                    } catch( IOException ioe ) {
                        Assert.fail();
                    }
                }
            } );
        mReadThread.start();
    }
}

