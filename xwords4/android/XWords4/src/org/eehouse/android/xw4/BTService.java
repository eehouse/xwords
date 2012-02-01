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
import android.os.Bundle;
import android.os.IBinder;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothClass;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothServerSocket;
import android.bluetooth.BluetoothSocket;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.CommsAddrRec;

import java.util.HashMap;
import java.util.Iterator;
import java.util.Set;

import java.io.DataInputStream;
import java.io.OutputStream;
import java.io.DataOutputStream;
import java.util.concurrent.LinkedBlockingQueue;

public class BTService extends Service {

    public enum BTEvent { SCAN_DONE
                        , HOST_PONGED
                        , NEWGAME_SUCCESS
                        , NEWGAME_FAILURE
                        , MESSAGE_ACCEPTED
                        , MESSAGE_REFUSED
                        , BT_ENABLED
                        , BT_DISABLED
            };

    public interface BTEventListener {
        public void eventOccurred( BTEvent event, Object ... args );
    }

    private static final int PING = 0;
    private static final int SCAN = 1;
    private static final int INVITE = 2;
    private static final int SEND = 3;

    private static final String CMD_STR = "CMD";
    private static final String MSG_STR = "MSG";
    private static final String TARGET_STR = "TRG";
    private static final String ADDR_STR = "ADR";

    private static final String GAMEID_STR = "GMI";

    private static final String LANG_STR = "LNG";
    private static final String NTO_STR = "TOT";
    private static final String NHE_STR = "HER";

    private static BTEventListener s_eventListener = null;
    private static Object s_syncObj = new Object();

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

    private class BTQueueElem {
        // These should perhaps be in some subclasses....
        BTCmd m_cmd;
        byte[] m_msg;
        String m_recipient;
        String m_addr;
        int m_gameID;
        int m_lang;
        int m_nPlayersT;
        int m_nPlayersH;

        public BTQueueElem( BTCmd cmd ) { m_cmd = cmd; }
        public BTQueueElem( BTCmd cmd, String targetName, String targetAddr,
                            int gameID, int lang, int nPlayersT, 
                            int nPlayersH ) {
            this( cmd, null, targetName, targetAddr, gameID );
            m_lang = lang; m_nPlayersT = nPlayersT; m_nPlayersH = nPlayersH;
        }
        public BTQueueElem( BTCmd cmd, byte[] buf, String targetName, 
                            String targetAddr, int gameID ) {
            m_cmd = cmd; m_msg = buf; m_recipient = targetName; 
            m_addr = targetAddr; m_gameID = gameID;
        }

    }

    private BluetoothAdapter m_adapter;
    private LinkedBlockingQueue<BTQueueElem> m_queue;
    private HashMap<String,String> m_names;
    private BTMsgSink m_btMsgSink;

    public static boolean BTEnabled()
    {
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        return null != adapter && adapter.isEnabled();
    }

    public static void startService( Context context )
    {
        context.startService( new Intent( context, BTService.class ) );
    }

    public static void setBTEventListener( BTEventListener li ) {
        synchronized( s_syncObj ) {
            s_eventListener = li;
        }
    }

    public static void rescan( Context context ){
        Intent intent = new Intent( context, BTService.class );
        intent.putExtra( CMD_STR, SCAN );
        context.startService( intent );
    }

    public static void ping( Context context )
    {
        Intent intent = new Intent( context, BTService.class );
        intent.putExtra( CMD_STR, PING );
        context.startService( intent );
    }

    public static void inviteRemote( Context context, String hostName, 
                                     int gameID, int lang, int nPlayersT, 
                                     int nPlayersH )
    {
        Intent intent = new Intent( context, BTService.class );
        intent.putExtra( CMD_STR, INVITE );
        intent.putExtra( GAMEID_STR, gameID );
        intent.putExtra( TARGET_STR, hostName );
        intent.putExtra( LANG_STR, lang );
        intent.putExtra( NTO_STR, nPlayersT );
        intent.putExtra( NHE_STR, nPlayersH );

        context.startService( intent );
    }

    public static int enqueueFor( Context context, byte[] buf, 
                                  String targetName, String targetAddr,
                                  int gameID )
    {
        Intent intent = new Intent( context, BTService.class );
        intent.putExtra( CMD_STR, SEND );
        intent.putExtra( MSG_STR, buf );
        intent.putExtra( TARGET_STR, targetName );
        intent.putExtra( ADDR_STR, targetAddr );
        intent.putExtra( GAMEID_STR, gameID );
        context.startService( intent );
        DbgUtils.logf( "got %d bytes for %s (%s), gameID %d", buf.length, 
                       targetName, targetAddr, gameID );
        return buf.length;
    }

    @Override
    public void onCreate()
    {
        DbgUtils.logf( "BTService.onCreate()" );
        m_adapter = BluetoothAdapter.getDefaultAdapter();
        if ( null != m_adapter && m_adapter.isEnabled() ) {
            m_names = new HashMap<String, String>();
            m_queue = new LinkedBlockingQueue<BTQueueElem>();
            m_btMsgSink = new BTMsgSink();
            new BTListenerThread().start();
            new BTSenderThread().start();
        } else {
            DbgUtils.logf( "not starting threads: BT not available" );
            stopSelf();
        }
    }

    @Override
    public int onStartCommand( Intent intent, int flags, int startId )
    {
        if ( null != intent ) {
            int cmd = intent.getIntExtra( CMD_STR, -1 );
            DbgUtils.logf( "BTService.onStartCommand; cmd=%d", cmd );
            switch( cmd ) {
            case -1:
                break;
            case PING:
                m_queue.add( new BTQueueElem( BTCmd.PING ) );
                break;
            case SCAN:
                m_queue.add( new BTQueueElem( BTCmd.SCAN ) );
                break;
            case INVITE:
                int gameID = intent.getIntExtra( GAMEID_STR, -1 );
                String target = intent.getStringExtra( TARGET_STR );
                String addr = addrFor( target );
                int lang = intent.getIntExtra( LANG_STR, -1 );
                int nPlayersT = intent.getIntExtra( NTO_STR, -1 );
                int nPlayersH = intent.getIntExtra( NHE_STR, -1 );
                m_queue.add( new BTQueueElem( BTCmd.INVITE, target, addr, 
                                              gameID, lang, 
                                              nPlayersT, nPlayersH ) );
                break;
            case SEND:
                byte[] buf = intent.getByteArrayExtra( MSG_STR );
                target = intent.getStringExtra( TARGET_STR );
                addr = intent.getStringExtra( ADDR_STR );
                gameID = intent.getIntExtra( GAMEID_STR, -1 );
                if ( -1 != gameID ) {
                    m_queue.add( new BTQueueElem( BTCmd.MESG_SEND, buf, target, 
                                                  addr, gameID ) );
                }
                break;
            default:
                Assert.fail();
            }
        }
        return Service.START_STICKY;
    }

    @Override
    public IBinder onBind( Intent intent )
    {
        return null;
    }

    private class BTListenerThread extends Thread {

        @Override
        public void run() {
            BluetoothServerSocket serverSocket;
            for ( ; ; ) {
                try {
                    serverSocket = m_adapter.
                        listenUsingRfcommWithServiceRecord( XWApp.getAppName(),
                                                            XWApp.getAppUUID() );
                } catch ( java.io.IOException ioe ) {
                    DbgUtils.logf( "listenUsingRfcommWithServiceRecord=>%s", 
                                   ioe.toString() );
                    serverSocket = null;
                    continue;
                }

                BluetoothSocket socket = null;
                DataInputStream inStream = null;
                int nRead = 0;
                try {
                    DbgUtils.logf( "run: calling accept()" );
                    socket = serverSocket.accept(); // blocks
                    addAddr( socket );
                    DbgUtils.logf( "run: accept() returned" );
                    inStream = new DataInputStream( socket.getInputStream() );

                    byte msg = inStream.readByte();
                    BTCmd cmd = BTCmd.values()[msg];
                    switch( cmd ) {
                    case PING:
                        receivePing( socket );
                        break;
                    case INVITE:
                        receiveInvitation( BTService.this, inStream, socket );
                        break;
                    case MESG_SEND:
                        receiveMessage( inStream, socket );
                        break;

                    default:
                        DbgUtils.logf( "unexpected msg %d", msg );
                        break;
                    }
                } catch ( java.io.IOException ioe ) {
                    DbgUtils.logf( "accept=>%s", ioe.toString() );
                    DbgUtils.logf( "trying again..." );
                    continue;
                }

                if ( null != serverSocket ) {
                    try {
                        serverSocket.close();
                    } catch ( java.io.IOException ioe ) {
                        DbgUtils.logf( "close()=>%s", ioe.toString() );
                    }
                    serverSocket = null;
                }
            }
        }
    }

    private void sendPings( BTEvent event )
    {
        Set<BluetoothDevice> pairedDevs = m_adapter.getBondedDevices();
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
                        if ( null != event ) {
                            sendResult( event, dev.getName() );
                        }
                    }
                }
            } catch ( java.io.IOException ioe ) {
                DbgUtils.logf( "sendPings: ioe: %s", ioe.toString() );
            }
        }
    }

    private void sendInvite( BTQueueElem elem )
    {
        try {
            BluetoothDevice dev = 
                m_adapter.getRemoteDevice( addrFor( elem.m_recipient ) );
            BluetoothSocket socket = 
                dev.createRfcommSocketToServiceRecord( XWApp.getAppUUID() );
            if ( null != socket ) {
                socket.connect();
                DataOutputStream outStream = 
                    new DataOutputStream( socket.getOutputStream() );
                outStream.writeByte( BTCmd.INVITE.ordinal() );
                outStream.writeInt( elem.m_gameID );
                outStream.writeInt( elem.m_lang );
                outStream.writeInt( elem.m_nPlayersT );
                outStream.writeInt( elem.m_nPlayersH );
                outStream.flush();

                DataInputStream inStream = 
                    new DataInputStream( socket.getInputStream() );
                boolean success = 
                    BTCmd.INVITE_ACCPT == BTCmd.values()[inStream.readByte()];
                DbgUtils.logf( "sendInvite(): invite done: success=%b", 
                               success );
                socket.close();

                BTEvent evt = success ? BTEvent.NEWGAME_SUCCESS
                    : BTEvent.NEWGAME_FAILURE;
                sendResult( evt, elem.m_gameID );
            }
        } catch ( java.io.IOException ioe ) {
            DbgUtils.logf( "sendInvites: ioe: %s", ioe.toString() );
        }
    }

    private void sendMsg( BTQueueElem elem )
    {
        try {
            BluetoothDevice dev = m_adapter.getRemoteDevice( elem.m_addr );
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

                BTEvent evt = success ? BTEvent.MESSAGE_ACCEPTED
                    : BTEvent.MESSAGE_REFUSED;
                sendResult( evt, elem.m_gameID, 0, elem.m_recipient );
            }
        } catch ( java.io.IOException ioe ) {
            DbgUtils.logf( "sendInvites: ioe: %s", ioe.toString() );
        }
    }

    private void addAddr( BluetoothSocket socket )
    {
        addAddr( socket.getRemoteDevice() );
    }

    private void addAddr( BluetoothDevice dev )
    {
        synchronized( m_names ) {
            m_names.put( dev.getName(), dev.getAddress() );
        }
    }

    private String addrFor( String name )
    {
        String addr;
        synchronized( m_names ) {
            addr = m_names.get( name );
        }
        DbgUtils.logf( "addrFor(%s)=>%s", name, addr );
        Assert.assertNotNull( addr );

        return addr;
    }

    private String[] names()
    {
        Set<String> names = null;
        synchronized( m_names ) {
            names = m_names.keySet();
        }

        String[] result = new String[names.size()];
        Iterator<String> iter = names.iterator();
        for ( int ii = 0; iter.hasNext(); ++ii ) {
            result[ii] = iter.next();
        }

        return result;
    }

    private class BTSenderThread extends Thread {
        @Override
        public void run()
        {
            for ( ; ; ) {
                BTQueueElem elem;
                try {
                    elem = m_queue.take();
                } catch ( InterruptedException ie ) {
                    DbgUtils.logf( "interrupted; killing thread" );
                    break;
                }
                DbgUtils.logf( "run: got %s from queue", elem.m_cmd.toString() );

                switch( elem.m_cmd ) {
                case PING:
                    sendPings( BTEvent.HOST_PONGED );
                    break;
                case SCAN:
                    synchronized ( m_names ) {
                        m_names.clear();
                    }
                    sendPings( null );
                    sendResult( BTEvent.SCAN_DONE, (Object)(names()) );
                    break;
                case INVITE:
                    sendInvite( elem );
                    break;
                case MESG_SEND:
                    sendMsg( elem );
                    break;
                default:
                    Assert.fail();
                    break;
                }
            }
        }
    }

    private void receivePing( BluetoothSocket socket )
        throws java.io.IOException
    {
        DbgUtils.logf( "got PING!!!" );
        DataOutputStream os = new DataOutputStream( socket.getOutputStream() );
        os.writeByte( BTCmd.PONG.ordinal() );
        os.flush();

        socket.close();
    }

    private void receiveInvitation( Context context,
                                    DataInputStream is,
                                    BluetoothSocket socket )
        throws java.io.IOException
    {
        int gameID = is.readInt();
        DbgUtils.logf( "receiveInvitation: got gameID of %d", gameID );
        int lang = is.readInt();
        int nPlayersT = is.readInt();
        int nPlayersH = is.readInt();

        BluetoothDevice host = socket.getRemoteDevice();
        CommsAddrRec addr = new CommsAddrRec( context, host.getName(), 
                                              host.getAddress() );
        GameUtils.makeNewBTGame( context, gameID, addr,
                                 lang, nPlayersT, nPlayersH );

        addAddr( host );

        // Post notification that, when selected, will create a game
        // -- or ask if user wants to create one.

        DataOutputStream os = new DataOutputStream( socket.getOutputStream() );
        os.writeByte( BTCmd.INVITE_ACCPT.ordinal() );
        os.flush();

        socket.close();
        DbgUtils.logf( "receiveInvitation done", gameID );
    }

    private void receiveMessage( DataInputStream dis, BluetoothSocket socket )
    {
        try {
            int gameID = dis.readInt();
            short len = dis.readShort();
            byte[] buffer = new byte[len];
            int nRead = dis.read( buffer, 0, len );
            if ( nRead == len ) {
                BluetoothDevice host = socket.getRemoteDevice();
                addAddr( host );

                DbgUtils.logf( "receiveMessages: got %d bytes from %s for "
                               + "gameID of %d", 
                               len, host.getName(), gameID );

                DataOutputStream os = 
                    new DataOutputStream( socket.getOutputStream() );
                os.writeByte( BTCmd.MESG_ACCPT.ordinal() );
                os.flush();
                socket.close();

                CommsAddrRec addr = new CommsAddrRec( this, host.getName(), 
                                                      host.getAddress() );

                if ( BoardActivity.feedMessage( gameID, buffer, addr ) ) {
                    DbgUtils.logf( "BoardActivity.feedMessage took it" );
                    // do nothing
                } else if ( GameUtils.feedMessage( this, gameID, buffer, 
                                                   addr, m_btMsgSink ) ) {
                    DbgUtils.logf( "GameUtils.feedMessage took it" );
                    // do nothing
                } else {
                    DbgUtils.logf( "nobody to take message for gameID", gameID );
                }
            } else {
                DbgUtils.logf( "receiveMessages: read only %d of %d bytes",
                               nRead, len );
            }
        } catch ( java.io.IOException ioe ) {
            DbgUtils.logf( "receiveMessages: ioe: %s", ioe.toString() );
        }
    }

    private void sendResult( BTEvent event, Object ... args )
    {
        Assert.assertNotNull( event );
        synchronized( s_syncObj ) {
            if ( null != s_eventListener ) {
                s_eventListener.eventOccurred( event, args );
            }
        }
    }

    private class BTMsgSink extends MultiMsgSink {

        /***** TransportProcs interface *****/

        public int transportSend( byte[] buf, final CommsAddrRec addr, int gameID )
        {
            m_queue.add( new BTQueueElem( BTCmd.MESG_SEND, buf, 
                                          addr.bt_hostName, addr.bt_btAddr,
                                          gameID ) );
            return buf.length;
        }

        public boolean relayNoConnProc( byte[] buf, String relayID )
        {
            Assert.fail();
            return false;
        }
    }

}
