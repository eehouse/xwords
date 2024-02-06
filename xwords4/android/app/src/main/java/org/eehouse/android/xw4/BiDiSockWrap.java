/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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
import java.net.InetAddress;
import java.net.Socket;
import java.util.concurrent.LinkedBlockingQueue;
import org.json.JSONObject;


public class BiDiSockWrap {
    private static final String TAG = BiDiSockWrap.class.getSimpleName();
    interface Iface {
        void gotPacket( BiDiSockWrap wrap, byte[] bytes );
        void onWriteSuccess( BiDiSockWrap wrap );
        void connectStateChanged( BiDiSockWrap wrap, boolean nowConnected );
    }

    private Socket mSocket;
    private Iface mIface;
    private LinkedBlockingQueue<byte[]> mQueue;
    private Thread mReadThread;
    private Thread mWriteThread;
    private boolean mRunThreads;
    private boolean mActive;
    private InetAddress mAddress;
    private int mPort;

    // For sockets that came from accept() on a ServerSocket
    public BiDiSockWrap( Socket socket, Iface iface )
    {
        mIface = iface;
        init( socket );
    }

    // For creating sockets that will connect to a remote ServerSocket. Must
    // call connect() afterwards.
    public BiDiSockWrap( InetAddress address, int port, Iface iface )
    {
        mIface = iface;
        mAddress = address;
        mPort = port;
    }

    public BiDiSockWrap connect()
    {
        mActive = true;
        new Thread( new Runnable() {
                @Override
                public void run() {
                    long waitMillis = 1000;
                    while ( mActive ) {
                        try {
                            Thread.sleep( waitMillis );
                            Log.d( TAG, "trying to connect..." );
                            Socket socket = new Socket( mAddress, mPort );
                            Log.d( TAG, "connected!!!" );
                            init( socket );
                            mIface.connectStateChanged( BiDiSockWrap.this, true );
                            break;
                        } catch ( java.net.UnknownHostException uhe ) {
                            Log.ex( TAG, uhe );
                        } catch ( IOException ioe ) {
                            Log.ex( TAG, ioe );
                        } catch ( InterruptedException ie ) {
                            Log.ex( TAG, ie );
                        }
                        waitMillis = Math.min( waitMillis * 2, 1000 * 60 );
                    }
                }
            } ).start();

        return this;
    }

    public Socket getSocket() { return mSocket; }

    public boolean isConnected() { return null != getSocket(); }

    private void send( String packet )
    {
        try {
            send( packet.getBytes( "UTF-8" ) );
        } catch ( java.io.UnsupportedEncodingException uee ) {
            Log.ex( TAG, uee );
        }
    }

    public void send( JSONObject obj )
    {
        send( obj.toString() );
    }

    public void send( XWPacket packet )
    {
        send( packet.toString() );
    }

    public void send( byte[] packet )
    {
        Assert.assertNotNull( packet );
        Assert.assertTrue( packet.length > 0 );
        mQueue.add( packet );
    }

    private void init( Socket socket )
    {
        mSocket = socket;
        mQueue = new LinkedBlockingQueue<>();
        startThreads();
    }

    private void closeSocket()
    {
        Log.d( TAG, "closeSocket()" );
        mRunThreads = false;
        mActive = false;
        try {
            mSocket.close();
        } catch ( IOException ioe ) {
            Log.ex( TAG, ioe );
        }
        mIface.connectStateChanged( this, false );
        mQueue.add( new byte[0] );
    }

    private void startThreads()
    {
        mRunThreads = true;
        mWriteThread = new Thread( new Runnable() {
                @Override
                public void run() {
                    Log.d( TAG, "write thread starting" );
                    try {
                        DataOutputStream outStream
                            = new DataOutputStream( mSocket.getOutputStream() );
                        while ( mRunThreads ) {
                            byte[] packet = mQueue.take();
                            Log.d( TAG,
                                           "write thread got packet of len %d",
                                           packet.length );
                            Assert.assertNotNull( packet );
                            if ( 0 == packet.length ) {
                                closeSocket();
                                break;
                            }

                            outStream.writeShort( packet.length );
                            outStream.write( packet, 0, packet.length );
                            mIface.onWriteSuccess( BiDiSockWrap.this );
                        }
                    } catch ( IOException ioe ) {
                        Log.ex( TAG, ioe );
                        closeSocket();
                    } catch ( InterruptedException ie ) {
                        Assert.failDbg();
                    }
                    Log.d( TAG, "write thread exiting" );
                }
            } );
        mWriteThread.start();

        mReadThread = new Thread( new Runnable() {
                @Override
                public void run() {
                    Log.d( TAG, "read thread starting" );
                    try {
                        DataInputStream inStream
                            = new DataInputStream( mSocket.getInputStream() );
                        while ( mRunThreads ) {
                            byte[] packet = new byte[inStream.readShort()];
                            inStream.readFully( packet );
                            mIface.gotPacket( BiDiSockWrap.this, packet );
                        }
                    } catch( IOException ioe ) {
                        Log.ex( TAG, ioe );
                        closeSocket();
                    }
                    Log.d( TAG, "read thread exiting" );
                }
            } );
        mReadThread.start();
    }
}
