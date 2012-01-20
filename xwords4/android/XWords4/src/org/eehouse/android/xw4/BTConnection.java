/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2012 by Eric House (xwords@eehouse.org).  All rights
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

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothServerSocket;
import android.bluetooth.BluetoothSocket;
import android.bluetooth.BluetoothDevice;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Set;
import java.util.UUID;

public class BTConnection extends BroadcastReceiver {
    public static final int GOT_PONG = 1;

    private static final byte PING = 1;
    private static final byte PONG = 2;

    private static BluetoothAdapter s_btAdapter = 
        BluetoothAdapter.getDefaultAdapter();
    private static BluetoothServerSocket s_serverSocket;

    private class BTListener extends Thread {

        @Override
        public void run() {
            byte[] buffer = new byte[1024];
            try {
                s_serverSocket = s_btAdapter.
                    listenUsingRfcommWithServiceRecord( XWApp.getAppName(),
                                                        XWApp.getAppUUID() );
            } catch ( java.io.IOException ioe ) {
                DbgUtils.logf( "listenUsingRfcommWithServiceRecord=>%s", 
                               ioe.toString() );
                s_serverSocket = null;
            }

            while ( null != s_serverSocket ) {
                BluetoothSocket socket = null;
                InputStream inStream = null;
                int nRead = 0;
                try {
                    DbgUtils.logf( "run: calling accept()" );
                    socket = s_serverSocket.accept(); // blocks
                    DbgUtils.logf( "run: accept() returned" );
                    inStream = socket.getInputStream();
                    nRead = inStream.read( buffer );
                    DbgUtils.logf( "read %d bytes from BT socket", nRead );

                    // now handle what's on the socket
                    if ( 0 < nRead && buffer[0] == PING ) {
                        DbgUtils.logf( "got PING!!!" );
                        OutputStream os = socket.getOutputStream();
                        os.write( PONG );
                        os.flush();
                    }

                    socket.close();
                } catch ( java.io.IOException ioe ) {
                    DbgUtils.logf( "accept=>%s", ioe.toString() );
                    DbgUtils.logf( "trying again..." );
                    continue;
                }

            }

            if ( null != s_serverSocket ) {
                try {
                    s_serverSocket.close();
                } catch ( java.io.IOException ioe ) {
                    DbgUtils.logf( "close()=>%s", ioe.toString() );
                }
                s_serverSocket = null;
            }
        }
    }

    private BTListener m_listener = null;

    @Override
    public void onReceive( Context context, Intent intent ) {
        int newState = intent.getIntExtra( BluetoothAdapter.EXTRA_STATE, -1 );
        String asString = null;
        switch ( newState ) {
        case -1:
            break;
        case BluetoothAdapter.STATE_OFF:
            asString = "STATE_OFF";
            break;
        case BluetoothAdapter.STATE_TURNING_ON:
            asString = "STATE_TURNING_ON";
            break;
        case BluetoothAdapter.STATE_ON:
            asString = "STATE_ON";
            m_listener = new BTListener();
            m_listener.start();
            break;
        case BluetoothAdapter.STATE_TURNING_OFF:
            asString = "STATE_TURNING_OFF";
            if ( null != m_listener ) {
                m_listener.stop();
                m_listener = null;
            }
            break;
        }

        if ( null != asString ) {
            DbgUtils.logf( "onReceive: new BT state = %s", asString );
        }

    } // onReceive

    // Test whether there's another device running Crosswords and it
    // can respond.
    private static class PingThread extends Thread {
        private Handler m_handler;

        public PingThread( Handler handler ) {
            m_handler = handler;
        }

        public void run() {
            BluetoothDevice xwdev = null;
            UUID myUUID = XWApp.getAppUUID();

            Set<BluetoothDevice> pairedDevs = s_btAdapter.getBondedDevices();
            DbgUtils.logf( "ping: got %d paired devices", pairedDevs.size() );
            for ( BluetoothDevice dev : pairedDevs ) {
                BluetoothSocket socket = null;
                try {
                    socket = dev.createRfcommSocketToServiceRecord( myUUID );
                    DbgUtils.logf( "PingThread: got socket to device %s", 
                                   dev.getName() );
                    socket.connect();
                    DbgUtils.logf( "PingThread: connected" );
                    OutputStream os = socket.getOutputStream();
                    os.write( PING );
                    DbgUtils.logf( "PingThread: wrote" );
                    os.flush();

                    InputStream is = socket.getInputStream();
                    byte[] buffer = new byte[128];
                    int nRead = is.read( buffer );
                    if ( 1 == nRead && buffer[0] == PONG ) {
                        m_handler.obtainMessage( GOT_PONG, dev.getName() )
                            .sendToTarget();
                    }

                    socket.close();
                } catch ( java.io.IOException ioe ) {
                    DbgUtils.logf( "PingThread: %s", ioe.toString() );
                }
            }
        }
    }

    public static void ping( Handler handler ) {
        PingThread pt = new PingThread( handler );
        pt.start();
    }

}
