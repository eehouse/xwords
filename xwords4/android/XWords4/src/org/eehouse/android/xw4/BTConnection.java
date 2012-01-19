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

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import java.io.InputStream;

public class BTConnection extends BroadcastReceiver {
    private static BluetoothAdapter s_btAdapter = BluetoothAdapter.getDefaultAdapter();
    private static BluetoothServerSocket s_serverSocket;

    private class BTListener extends Thread {
        @Override
        public void run() {
            byte[] buffer = new byte[1024];
            for ( ; ; ) {
                try {
                    s_serverSocket = s_btAdapter.
                        listenUsingRfcommWithServiceRecord( XWApp.getAppName(),
                                                            XWApp.getAppUUID() );
                } catch ( java.io.IOException ioe ) {
                    DbgUtils.logf( "listenUsingRfcommWithServiceRecord=>%s", ioe.toString() );
                    break;
                }

                BluetoothSocket socket = null;
                InputStream inStream = null;
                try {
                    socket = s_serverSocket.accept(); // blocks
                    inStream = socket.getInputStream();
                    int nRead = inStream.read( buffer );
                    DbgUtils.logf( "read %d bytes from BT socket", nRead );
                    socket.close();
                } catch ( java.io.IOException ioe ) {
                    DbgUtils.logf( "accept=>%s", ioe.toString() );
                    break;
                }

                // now handle what's on the socket

            } // for ( ; ; )

            if ( null != s_serverSocket ) {
                try {
                    s_serverSocket.close();
                } catch ( java.io.IOException ioe ) {
                    DbgUtils.logf( "close()=>%s", ioe.toString() );
                }
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

}
