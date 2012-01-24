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

import android.app.ProgressDialog;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.AsyncTask;
import android.os.Handler;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Set;
import java.util.UUID;

public class BTConnection extends BroadcastReceiver {
    public static final int GOT_PONG = 1;
    public static final int CONNECT_ACCEPTED = 2;
    public static final int CONNECT_REFUSED = 3;
    public static final int CONNECT_FAILED = 4;
    public static final byte SCAN_DONE = 5;

    public interface BTStateChangeListener {
        public void stateChanged( boolean nowEnabled );
    }
    private static BTStateChangeListener s_stateChangeListener = null;

    private static final byte PING = 1;
    private static final byte PONG = 2;
    private static final byte INVITE = 3;
    private static final byte INVITE_ACCPT = 4;
    private static final byte INVITE_DECL = 5;
    private static final byte MESG_SEND = 6;
    private static final byte MESG_ACCPT = 7;
    private static final byte MESG_DECL = 8;

    private static BluetoothAdapter s_btAdapter = 
        BluetoothAdapter.getDefaultAdapter();
    private static BluetoothServerSocket s_serverSocket;
    private static HashMap<String,String> s_names = 
        new HashMap<String, String>();

    private class BTListener extends Thread {
        private Context m_context;

        public BTListener( Context context )
        {
            m_context = context;
        }

        @Override
        public void run() {
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
                DataInputStream inStream = null;
                int nRead = 0;
                try {
                    DbgUtils.logf( "run: calling accept()" );
                    socket = s_serverSocket.accept(); // blocks
                    DbgUtils.logf( "run: accept() returned" );
                    inStream = new DataInputStream( socket.getInputStream() );

                    short len = inStream.readShort();
                    if ( 1 <= len ) {
                        byte msg = inStream.readByte();
                        switch( msg ) {
                        case PING:
                            receivePing( socket );
                            break;
                        case INVITE:
                            receiveInvitation( m_context, inStream, socket );
                            break;
                        default:
                            DbgUtils.logf( "unexpected msg %d", msg );
                            break;
                        }
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
            tellStateChanged( false );
            break;
        case BluetoothAdapter.STATE_TURNING_ON:
            asString = "STATE_TURNING_ON";
            break;
        case BluetoothAdapter.STATE_ON:
            asString = "STATE_ON";
            m_listener = new BTListener( context );
            m_listener.start();
            tellStateChanged( true );
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
                    DbgUtils.logf( "PingThread: got socket to device %s", 
                                   dev.getName() );
                    if ( sendPing( dev ) ) {
                        synchronized( s_names ) {
                            s_names.put( dev.getName(), dev.getAddress() );
                        }
                        if ( null != m_handler ) {
                            m_handler.obtainMessage( GOT_PONG, dev.getName() )
                                .sendToTarget();
                        }
                    }
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

    public static boolean BTEnabled()
    {
        boolean enabled = null != s_btAdapter && s_btAdapter.isEnabled();
        DbgUtils.logf( "BTEnabled=>%b", enabled );
        return enabled;
    }

    public static void setBTStateChangeListener( BTStateChangeListener li ) {
        s_stateChangeListener = li;
    }

    // 
    public static void enqueueFor( byte[] buf )
    {
    }

    private static class InviteThread extends Thread {
        String m_name;
        int m_gameID;
        Handler m_handler;
        public InviteThread( String name, int gameID, Handler handler ) {
            m_name = name;
            m_gameID = gameID;
            m_handler = handler;
        }

        public void run() {
            String addr;
            int result = CONNECT_FAILED;
            synchronized( s_names ) {
                addr = s_names.get( m_name );
            }
            if ( null != addr ) {
                try { 
                    BluetoothDevice remote = s_btAdapter.getRemoteDevice( addr );
                    if ( null != remote && sendInvitation( remote, m_gameID ) ) {
                        result = CONNECT_ACCEPTED;
                    }
                } catch( java.io.IOException ioe ) {
                    DbgUtils.logf( "ioe: %s", ioe.toString() );
                }

            }
            m_handler.obtainMessage( result, m_gameID ).sendToTarget();
        }
    }

    public static void inviteRemote( String devName, int gameID, 
                                     Handler handler )
    {
        InviteThread thread = new InviteThread( devName, gameID, handler );
        thread.start();
    }

    private static class BTScanner extends AsyncTask<Void, Void, Void> {
        private Handler m_handler;
        private ProgressDialog m_progress;

        public BTScanner( Context context, Handler handler ) {
            super();
            m_handler = handler;

            String msg = context.getString( R.string.scan_progress );
            m_progress = ProgressDialog.show( context, msg, null, true, true );
        }

        @Override
        protected Void doInBackground( Void... unused )
        {
            synchronized( s_names ) {
                s_names.clear();
            }
            new PingThread( null ).run(); // same thread
            return null;
        }

        @Override
        protected void onPostExecute( Void unused )
        {
            m_progress.cancel();
            m_handler.obtainMessage( SCAN_DONE ).sendToTarget();
        }
    }

    public static void rescan( Context context, Handler handler )
    {
        new BTScanner( context, handler ).execute();
    }

    public static String[] listPairedWithXwords()
    {
        Set<String> names = null;
        synchronized( s_names ) {
            names = s_names.keySet();
        }

        String[] result = new String[names.size()];
        Iterator<String> iter = names.iterator();
        for ( int ii = 0; iter.hasNext(); ++ii ) {
            result[ii] = iter.next();
        }

        return result;
    }

    private void tellStateChanged( boolean nowOn ) {
        if ( null != s_stateChangeListener ) {
            s_stateChangeListener.stateChanged( nowOn );
        }
    }

    private static boolean sendInvitation( BluetoothDevice dev, int gameID ) 
        throws java.io.IOException
    {
        boolean success = false;
        BluetoothSocket socket = 
            dev.createRfcommSocketToServiceRecord( XWApp.getAppUUID() );
        if ( null != socket ) {
            socket.connect();
            DataOutputStream outStream = 
                new DataOutputStream( socket.getOutputStream() );
            short len = 1           // INVITE
                + 4                 // gameID
                ;
            writeHeader( outStream, len, INVITE );
            // outStream.writeShort( len );
            // outStream.writeByte( INVITE );
            outStream.writeInt( gameID );
            outStream.flush();

            DataInputStream inStream = 
                new DataInputStream( socket.getInputStream() );
            success = INVITE_ACCPT == inStream.readByte();
            socket.close();
        }
        return success;
    }

    private static void receiveInvitation( Context context,
                                           DataInputStream is,
                                           BluetoothSocket socket )
        throws java.io.IOException
    {
        int gameID = is.readInt();
        DbgUtils.logf( "receiveInvitation: got gameID of %d", gameID );

        GameUtils.makeNewBTGame( context, gameID );

        // Post notification that, when selected, will create a game
        // -- or ask if user wants to create one.

        OutputStream os = socket.getOutputStream();
        os.write( INVITE_ACCPT );
        os.flush();
    }

    private static boolean sendPing( BluetoothDevice dev )
        throws java.io.IOException
    {
        boolean success = false;
        BluetoothSocket socket = 
            dev.createRfcommSocketToServiceRecord( XWApp.getAppUUID() );
        if ( null != socket ) {
            socket.connect();

            DbgUtils.logf( "PingThread: connected" );
            DataOutputStream os = new DataOutputStream( socket.getOutputStream() );
            writeHeader( os, (short)1, PING );
            DbgUtils.logf( "PingThread: wrote" );
            os.flush();

            DataInputStream is = 
                new DataInputStream( socket.getInputStream() );
            if ( PONG == is.readByte() ) {
                success = true;
            }
            socket.close();
        }
        return success;
    }

    private static void receivePing( BluetoothSocket socket )
        throws java.io.IOException
    {
        DbgUtils.logf( "got PING!!!" );
        OutputStream os = socket.getOutputStream();
        os.write( PONG );
        os.flush();
    }

    private static void writeHeader( DataOutputStream outStream, short len, 
                                     byte msg )
        throws java.io.IOException
    {
        outStream.writeShort( len );
        outStream.writeByte( msg );
    }
}
