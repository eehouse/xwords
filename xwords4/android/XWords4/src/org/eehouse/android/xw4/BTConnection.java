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
import java.util.concurrent.LinkedBlockingQueue;

import junit.framework.Assert;

public class BTConnection extends BroadcastReceiver {
    public static final int GOT_PONG = 1;
    public static final int CONNECT_ACCEPTED = 2;
    public static final int CONNECT_REFUSED = 3;
    public static final int CONNECT_FAILED = 4;
    public static final int MESSAGE_ACCEPTED = 5;
    public static final int MESSAGE_REFUSED = 6;
    public static final byte SCAN_DONE = 7;

    public interface BTStateChangeListener {
        public void stateChanged( boolean nowEnabled );
    }
    private static BTStateChangeListener s_stateChangeListener = null;

    private enum BTCmd { 
            PING,
            PONG,
            SCAN,
            INVITE,
            INVITE_ACCPT,
            INVITE_DECL,
            MESG_SEND,
            MESG_ACCPT,
            MESG__DECL,
            };

    private static BluetoothAdapter s_btAdapter;
    private static BluetoothServerSocket s_serverSocket;
    private static HashMap<String,String> s_names = 
        new HashMap<String, String>();

    private static class BTQueueElem {
        // These should perhaps be in some subclasses....
        BTCmd m_cmd;
        byte[] m_msg;
        String m_recipient;
        Handler m_handler;
        int m_gameID;

        public BTQueueElem( BTCmd cmd ) { m_cmd = cmd; }
        public BTQueueElem( BTCmd cmd, Handler handler ) { 
            m_cmd = cmd; m_handler = handler; 
        }
        public BTQueueElem( BTCmd cmd, byte[] buf, String target, int gameID ) {
            m_cmd = cmd; m_msg = buf; m_recipient = target; m_gameID = gameID;
        }
        public BTQueueElem( BTCmd cmd, String dev, int gameID, 
                            Handler handler ) {
            m_cmd = cmd; m_recipient = dev; m_gameID = gameID; m_handler = handler;
        }
    }
    private static LinkedBlockingQueue<BTQueueElem> s_queue;

    private static class BTSendThread extends Thread {
        @Override
        public void run()
        {
            for ( ; ; ) {
                BTQueueElem elem;
                try {
                    elem = s_queue.take();
                } catch ( InterruptedException ie ) {
                    DbgUtils.logf( "interrupted; killing thread" );
                    break;
                }
                
                DbgUtils.logf( "run: got %s from queue", elem.m_cmd.toString() );
                switch( elem.m_cmd ) {
                case PING:
                    sendPings( elem.m_handler );
                    break;
                case SCAN:
                    doScan( elem );
                    break;
                case INVITE:
                    sendInvite( elem );
                    break;
                case MESG_SEND:
                    sendMsg( elem );
                    break;
                }
            }
        }
    }

    // Static initializers
    static {
        s_btAdapter = BluetoothAdapter.getDefaultAdapter();
        Assert.assertNotNull( s_btAdapter );
        if ( null != s_btAdapter ) {
            s_queue = new LinkedBlockingQueue<BTQueueElem>();
            new BTSendThread().start();
        }
    }

    private class BTListener extends Thread {
        private Context m_context;

        public BTListener( Context context )
        {
            m_context = context;
        }

        @Override
        public void run() {
            for ( ; ; ) {
                try {
                    s_serverSocket = s_btAdapter.
                        listenUsingRfcommWithServiceRecord( XWApp.getAppName(),
                                                            XWApp.getAppUUID() );
                } catch ( java.io.IOException ioe ) {
                    DbgUtils.logf( "listenUsingRfcommWithServiceRecord=>%s", 
                                   ioe.toString() );
                    s_serverSocket = null;
                    continue;
                }

                BluetoothSocket socket = null;
                DataInputStream inStream = null;
                int nRead = 0;
                try {
                    DbgUtils.logf( "run: calling accept()" );
                    socket = s_serverSocket.accept(); // blocks
                    DbgUtils.logf( "run: accept() returned" );
                    inStream = new DataInputStream( socket.getInputStream() );

                    byte msg = inStream.readByte();
                    BTCmd cmd = BTCmd.values()[msg];
                    switch( cmd ) {
                    case PING:
                        receivePing( socket );
                        break;
                    case INVITE:
                        receiveInvitation( m_context, inStream, socket );
                        break;
                    case MESG_SEND:
                        receiveMessages( m_context, inStream, socket );
                        break;
                    default:
                        DbgUtils.logf( "unexpected msg %d", msg );
                        break;
                    }

                    socket.close();
                } catch ( java.io.IOException ioe ) {
                    DbgUtils.logf( "accept=>%s", ioe.toString() );
                    DbgUtils.logf( "trying again..." );
                    continue;
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

    public static void ping( Handler handler ) 
    {
        s_queue.add( new BTQueueElem( BTCmd.PING, handler ) );
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

    public static int enqueueFor( byte[] buf, String target, int gameID )
    {
        DbgUtils.logf( "got %d bytes for %s, gameID %d", buf.length, target, 
                       gameID );
        s_queue.add( new BTQueueElem( BTCmd.MESG_SEND, buf, target, gameID ) );
        return buf.length;
    }

    public static void inviteRemote( String devName, int gameID, 
                                     Handler handler )
    {
        s_queue.add( new BTQueueElem( BTCmd.INVITE, devName, gameID, handler ) );
    }

    public static void rescan( Context context, Handler handler )
    {
        s_queue.add( new BTQueueElem( BTCmd.SCAN, handler ) );
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

    private static void sendInvite( BTQueueElem elem )
    {
        try {
            BluetoothDevice dev = 
                s_btAdapter.getRemoteDevice( addrFor( elem.m_recipient ) );
            BluetoothSocket socket = 
                dev.createRfcommSocketToServiceRecord( XWApp.getAppUUID() );
            if ( null != socket ) {
                socket.connect();
                DataOutputStream outStream = 
                    new DataOutputStream( socket.getOutputStream() );
                outStream.writeByte( BTCmd.INVITE.ordinal() );
                outStream.writeInt( elem.m_gameID );
                outStream.flush();

                DataInputStream inStream = 
                    new DataInputStream( socket.getInputStream() );
                boolean success = 
                    BTCmd.INVITE_ACCPT == BTCmd.values()[inStream.readByte()];
                socket.close();

                if ( null != elem.m_handler ) {
                    int result = success ? CONNECT_ACCEPTED : CONNECT_FAILED;
                    elem.m_handler.obtainMessage( result, elem.m_gameID, 0 )
                        .sendToTarget();
                }
            }
        } catch ( java.io.IOException ioe ) {
            DbgUtils.logf( "sendInvites: ioe: %s", ioe.toString() );
        }
    }

    private static void receiveInvitation( Context context,
                                           DataInputStream is,
                                           BluetoothSocket socket )
        throws java.io.IOException
    {
        int gameID = is.readInt();
        DbgUtils.logf( "receiveInvitation: got gameID of %d", gameID );

        BluetoothDevice host = socket.getRemoteDevice();
        GameUtils.makeNewBTGame( context, gameID, host.getName() );

        addAddr( host );

        // Post notification that, when selected, will create a game
        // -- or ask if user wants to create one.

        DataOutputStream os = new DataOutputStream( socket.getOutputStream() );
        os.writeByte( BTCmd.INVITE_ACCPT.ordinal() );
        os.flush();
    }

    private static void sendMsg( BTQueueElem elem )
    {
        try {
            BluetoothDevice dev = 
                s_btAdapter.getRemoteDevice( addrFor( elem.m_recipient ) );
            BluetoothSocket socket = 
                dev.createRfcommSocketToServiceRecord( XWApp.getAppUUID() );
            if ( null != socket ) {
                socket.connect();
                DataOutputStream outStream = 
                    new DataOutputStream( socket.getOutputStream() );
                outStream.writeByte( BTCmd.MESG_SEND.ordinal() );
                outStream.writeInt( elem.m_gameID );

                short len = (short)elem.m_msg.length;
                outStream.writeShort( len );
                outStream.write( elem.m_msg, 0, elem.m_msg.length );

                outStream.flush();

                DataInputStream inStream = 
                    new DataInputStream( socket.getInputStream() );
                boolean success = 
                    BTCmd.MESG_ACCPT == BTCmd.values()[inStream.readByte()];
                socket.close();

                if ( null != elem.m_handler ) {
                    int result = success ? MESSAGE_ACCEPTED : MESSAGE_REFUSED;
                    elem.m_handler.obtainMessage( result, elem.m_gameID, 0, 
                                                  elem.m_recipient )
                        .sendToTarget();
                }
            }
        } catch ( java.io.IOException ioe ) {
            DbgUtils.logf( "sendInvites: ioe: %s", ioe.toString() );
        }
    }

    private static void receiveMessages( Context context, DataInputStream dis,
                                         BluetoothSocket socket )
    {
        try {
            int gameID = dis.readInt();
            short len = dis.readShort();
            byte[] buffer = new byte[len];
            if ( dis.read( buffer, 0, len ) == len ) {
                BluetoothDevice host = socket.getRemoteDevice();
                addAddr( host );

                DbgUtils.logf( "receiveMessages: got %d bytes from %s for "
                               + "gameID of %d", 
                               len, host.getName(), gameID );

                DataOutputStream os = 
                    new DataOutputStream( socket.getOutputStream() );
                os.writeByte( BTCmd.MESG_ACCPT.ordinal() );
                os.flush();
            } else {
                DbgUtils.logf( "receiveMessages: failed to read %d bytes",
                               len );
            }
        } catch ( java.io.IOException ioe ) {
            DbgUtils.logf( "sendInvites: ioe: %s", ioe.toString() );
        }
    }

    private static void sendPings( Handler handler )
    {
        Set<BluetoothDevice> pairedDevs = s_btAdapter.getBondedDevices();
        DbgUtils.logf( "ping: got %d paired devices", pairedDevs.size() );
        for ( BluetoothDevice dev : pairedDevs ) {
            try {
                DbgUtils.logf( "PingThread: got socket to device %s", 
                               dev.getName() );
                BluetoothSocket socket =
                    dev.createRfcommSocketToServiceRecord( XWApp.getAppUUID() );
                if ( null != socket ) {
                    socket.connect();

                    DbgUtils.logf( "sendPings: connected" );
                    DataOutputStream os = 
                        new DataOutputStream( socket.getOutputStream() );
                    os.writeByte( BTCmd.PING.ordinal() );
                    DbgUtils.logf( "sendPings: wrote" );
                    os.flush();

                    DataInputStream is = 
                        new DataInputStream( socket.getInputStream() );
                    boolean success = BTCmd.PONG == BTCmd.values()[is.readByte()];
                    socket.close();

                    if ( success ) {
                        DbgUtils.logf( "got PONG from %s", dev.getName() );
                        addAddr( dev );
                        if ( null != handler ) {
                            handler.obtainMessage( GOT_PONG, dev.getName() )
                                .sendToTarget();
                        }
                    }
                }
            } catch ( java.io.IOException ioe ) {
                DbgUtils.logf( "sendPings: ioe: %s", ioe.toString() );
            }
        }
    }

    private static void receivePing( BluetoothSocket socket )
        throws java.io.IOException
    {
        DbgUtils.logf( "got PING!!!" );
        DataOutputStream os = new DataOutputStream( socket.getOutputStream() );
        os.writeByte( BTCmd.PONG.ordinal() );
        os.flush();
    }

    private static void doScan( BTQueueElem elem )
    {
        sendPings( null );
        elem.m_handler.obtainMessage( SCAN_DONE ).sendToTarget();
    }

    private static void addAddr( BluetoothDevice dev )
    {
        synchronized( s_names ) {
            s_names.put( dev.getName(), dev.getAddress() );
        }
    }

    private static String addrFor( String name )
    {
        String addr;
        synchronized( s_names ) {
            addr = s_names.get( name );
        }
        Assert.assertNotNull( addr );

        DbgUtils.logf( "addrFor(%s)=>%s", name, addr );
        return addr;
    }
}
