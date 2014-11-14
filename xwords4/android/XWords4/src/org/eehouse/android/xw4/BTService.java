/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2010 - 2014 by Eric House (xwords@eehouse.org).  All
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

import android.app.Activity;
import android.app.Service;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothClass;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothServerSocket;
import android.bluetooth.BluetoothSocket;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.ListIterator;
import java.util.Map.Entry;
import java.util.Set;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

import org.eehouse.android.xw4.MultiService.MultiEvent;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.LastMoveInfo;
import org.eehouse.android.xw4.loc.LocUtils;

import junit.framework.Assert;

public class BTService extends XWService {

    private static final long RESEND_TIMEOUT = 5; // seconds
    private static final int MAX_SEND_FAIL = 3;

    private static final int BT_PROTO = 0;

    private static final int SCAN = 1;
    private static final int INVITE = 2;
    private static final int SEND = 3;
    private static final int RADIO = 4;
    private static final int CLEAR = 5;
    private static final int REMOVE = 6;
    private static final int NFCINVITE = 7;
    private static final int PINGHOST = 8;

    private static final String CMD_STR = "CMD";
    private static final String MSG_STR = "MSG";
    private static final String GAMENAME_STR = "NAM";
    private static final String ADDR_STR = "ADR";
    private static final String RADIO_STR = "RDO";
    private static final String CLEAR_STR = "CLR";

    private static final String GAMEID_STR = "GMI";
    private static final String GAMEDATA_STR = "GD";

    private static final String LANG_STR = "LNG";
    private static final String DICT_STR = "DCT";
    private static final String NTO_STR = "TOT";
    private static final String NHE_STR = "HER";
    private static final String BT_NAME_STR = "BT_NAME_STR";
    private static final String BT_ADDRESS_STR = "BT_ADDRESS_STR";

    private enum BTCmd { 
        BAD_PROTO,
        PING,
        PONG,
        SCAN,
        INVITE,
        INVITE_ACCPT,
        INVITE_DECL,
        INVITE_DUPID,
        INVITE_FAILED,      // generic error
        MESG_SEND,
        MESG_ACCPT,
        MESG_DECL,
        MESG_GAMEGONE,
        REMOVE_FOR,
    };

    private class BTQueueElem {
        int m_failCount;
        // These should perhaps be in some subclasses....
        BTCmd m_cmd;
        byte[] m_msg;
        String m_btAddr;
        String m_gameName;
        int m_gameID;
        int m_lang;
        String m_dict;
        int m_nPlayersT;
        int m_nPlayersH;
        NetLaunchInfo m_nli;

        public BTQueueElem( BTCmd cmd ) { m_cmd = cmd; m_failCount = 0; }
        // public BTQueueElem( BTCmd cmd, String btAddr, 
        //                     int gameID, String gameName, int lang, 
        //                     String dict, int nPlayersT, int nPlayersH ) {
        //     this( cmd, null, btAddr, gameID );
        //     m_lang = lang; m_dict = dict; m_nPlayersT = nPlayersT; 
        //     m_nPlayersH = nPlayersH; m_gameName = gameName;
        // }
        public BTQueueElem( BTCmd cmd, byte[] buf, String btAddr, int gameID ) {
            this( cmd );
            Assert.assertTrue( null != btAddr && 0 < btAddr.length() );
            m_msg = buf; m_btAddr = btAddr; 
            m_gameID = gameID;
        }
        public BTQueueElem( BTCmd cmd, String btAddr, int gameID ) {
            this( cmd );
            Assert.assertTrue( null != btAddr && 0 < btAddr.length() );
            m_btAddr = btAddr;
            m_gameID = gameID;
        }

        public BTQueueElem( BTCmd cmd, NetLaunchInfo nli, String btAddr ) {
            this( cmd );
            m_nli = nli;
            m_btAddr = btAddr;
        }

        public int incrFailCount() { return ++m_failCount; }
        public boolean failCountExceeded() { return m_failCount >= MAX_SEND_FAIL; }
    }

    private BluetoothAdapter m_adapter;
    private Set<String> m_addrs;
    private MultiMsgSink m_btMsgSink;
    private BTListenerThread m_listener;
    private BTSenderThread m_sender;
    private static int s_errCount = 0;

    public static boolean BTAvailable()
    {
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        return null != adapter;
    }

    public static boolean BTEnabled()
    {
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        return null != adapter && adapter.isEnabled();
    }

    public static String[] getBTNameAndAddress()
    {
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        return null == adapter ? null 
            : new String[] { adapter.getName(), adapter.getAddress() };
    }

    public static int getPairedCount( Activity activity )
    {
        int result = 0;
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        if ( null != adapter ) {
            Set<BluetoothDevice> pairedDevs = adapter.getBondedDevices();
            result = pairedDevs.size();
        }
        return result;
    }

    public static void openBTSettings( Activity activity )
    {
        Intent intent = new Intent();
        intent.setAction( android.provider.Settings.ACTION_BLUETOOTH_SETTINGS );
        activity.startActivity( intent ); 
    }

    public static String nameForAddr( String btAddr )
    {
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        return nameForAddr( adapter, btAddr );
    }

    private static String nameForAddr( BluetoothAdapter adapter, String btAddr )
    {
        String result = null;
        if ( null != adapter ) {
            result = adapter.getRemoteDevice( btAddr ).getName();
        }
        return result;
    }

    public static void startService( Context context )
    {
        if ( XWApp.BTSUPPORTED ) {
            context.startService( new Intent( context, BTService.class ) );
        }
    }

    public static void radioChanged( Context context, boolean cameOn )
    {
        Intent intent = getIntentTo( context, RADIO );
        intent.putExtra( RADIO_STR, cameOn );
        context.startService( intent );
    }

    public static void clearDevices( Context context, String[] btAddrs )
    {
        Intent intent = getIntentTo( context, CLEAR );
        intent.putExtra( CLEAR_STR, btAddrs );
        context.startService( intent );
    }

    public static void scan( Context context )
    {
        Intent intent = getIntentTo( context, SCAN );
        context.startService( intent );
    }

    public static void pingHost( Context context, String hostAddr, int gameID )
    {
        Intent intent = getIntentTo( context, PINGHOST );
        intent.putExtra( ADDR_STR, hostAddr );
        intent.putExtra( GAMEID_STR, gameID );
        context.startService( intent );
    } 

    // public static void inviteRemote( Context context, String btAddr, 
    //                                  int gameID, String initialName, int lang, 
    //                                  String dict, int nPlayersT, int nPlayersH )
    // {
    //     Intent intent = getIntentTo( context, INVITE );
    //     intent.putExtra( GAMEID_STR, gameID );
    //     intent.putExtra( ADDR_STR, btAddr );
    //     Assert.assertNotNull( initialName );
    //     intent.putExtra( GAMENAME_STR, initialName );
    //     intent.putExtra( LANG_STR, lang );
    //     intent.putExtra( DICT_STR, dict );
    //     intent.putExtra( NTO_STR, nPlayersT );
    //     intent.putExtra( NHE_STR, nPlayersH );

    //     context.startService( intent );
    // }

    public static void inviteRemote( Context context, String btAddr, 
                                     NetLaunchInfo nli )
    {
        Intent intent = getIntentTo( context, INVITE );
        String nliData = nli.toString();
        intent.putExtra( GAMEDATA_STR, nliData );
        intent.putExtra( ADDR_STR, btAddr );

        context.startService( intent );
    }

    public static void gotGameViaNFC( Context context, NetLaunchInfo bli )
    {
        Intent intent = getIntentTo( context, NFCINVITE );
        intent.putExtra( GAMEID_STR, bli.gameID );
        intent.putExtra( DICT_STR, bli.dict );
        intent.putExtra( LANG_STR, bli.lang );
        intent.putExtra( NTO_STR, bli.nPlayersT );
        intent.putExtra( BT_NAME_STR, bli.btName );
        intent.putExtra( BT_ADDRESS_STR, bli.btAddress );

        context.startService( intent );
    }

    public static int enqueueFor( Context context, byte[] buf, 
                                  String targetAddr, int gameID )
    {
        int nSent = -1;
        if ( null != targetAddr && 0 < targetAddr.length() ) {
            Intent intent = getIntentTo( context, SEND );
            intent.putExtra( MSG_STR, buf );
            intent.putExtra( ADDR_STR, targetAddr );
            intent.putExtra( GAMEID_STR, gameID );
            context.startService( intent );
            nSent = buf.length;
        } else {
            DbgUtils.logf( "BTService.enqueueFor(): targetAddr is null" );
        }
        return nSent;
    }
    
    public static void gameDied( Context context, int gameID )
    {
        Intent intent = getIntentTo( context, REMOVE );
        intent.putExtra( GAMEID_STR, gameID );
        context.startService( intent );
    }

    private static Intent getIntentTo( Context context, int cmd )
    {
        Intent intent = new Intent( context, BTService.class );
        intent.putExtra( CMD_STR, cmd );
        return intent;
    }

    @Override
    public void onCreate()
    {
        BluetoothAdapter adapter = XWApp.BTSUPPORTED
            ? BluetoothAdapter.getDefaultAdapter() : null;
        if ( null != adapter && adapter.isEnabled() ) {
            m_adapter = adapter;
            DbgUtils.logf( "BTService.onCreate(); bt name = %s; bt addr = %s", 
                           adapter.getName(), adapter.getAddress() );
            initAddrs();
            startListener();
            startSender();
        } else {
            DbgUtils.logf( "not starting threads: BT not available" );
            stopSelf();
        }
    }

    @Override
    public int onStartCommand( Intent intent, int flags, int startId )
    {
        int result;
        if ( XWApp.BTSUPPORTED && null != intent ) {
            int cmd = intent.getIntExtra( CMD_STR, -1 );
            DbgUtils.logf( "BTService.onStartCommand; cmd=%d", cmd );
            if ( -1 == cmd ) {
            } else if ( null == m_sender ) {
                DbgUtils.logf( "exiting: m_queue is null" );
                stopSelf();
            } else {
                switch( cmd ) {
                case -1:
                    break;
                case CLEAR:
                    String[] btAddrs = intent.getStringArrayExtra( CLEAR_STR );
                    clearDevs( btAddrs );
                    sendNames();
                    break;
                case SCAN:
                    m_sender.add( new BTQueueElem( BTCmd.SCAN ) );
                    break;
                case INVITE:
                    String jsonData = intent.getStringExtra( GAMEDATA_STR );
                    NetLaunchInfo nli = new NetLaunchInfo( jsonData );
                    DbgUtils.logf( "onStartCommand: nli: %s", nli.toString() );
                    // int gameID = intent.getIntExtra( GAMEID_STR, -1 );
                    // String btAddr = intent.getStringExtra( ADDR_STR );
                    // String gameName = intent.getStringExtra( GAMENAME_STR );
                    // int lang = intent.getIntExtra( LANG_STR, -1 );
                    // String dict = intent.getStringExtra( DICT_STR );
                    // int nPlayersT = intent.getIntExtra( NTO_STR, -1 );
                    String btAddr = intent.getStringExtra( ADDR_STR );
                    m_sender.add( new BTQueueElem( BTCmd.INVITE, nli, btAddr ) );
                    break;

                case PINGHOST:
                    btAddr = intent.getStringExtra( ADDR_STR );
                    int gameID = intent.getIntExtra( GAMEID_STR, 0 );
                    m_sender.add( new BTQueueElem( BTCmd.PING, btAddr, gameID ) );
                    break;

                case NFCINVITE:
                    gameID = intent.getIntExtra( GAMEID_STR, -1 );
                    int lang = intent.getIntExtra( LANG_STR, -1 );
                    String dict = intent.getStringExtra( DICT_STR );
                    int nPlayersT = intent.getIntExtra( NTO_STR, -1 );
                    String btName = intent.getStringExtra( BT_NAME_STR );
                    btAddr = intent.getStringExtra( BT_ADDRESS_STR );
                    // /*(void)*/makeOrNotify( this, gameID, null, lang, dict, 
                    //                         nPlayersT, 1, btName, btAddr );
                    Assert.fail();
                    break;

                case SEND:
                    byte[] buf = intent.getByteArrayExtra( MSG_STR );
                    btAddr = intent.getStringExtra( ADDR_STR );
                    gameID = intent.getIntExtra( GAMEID_STR, -1 );
                    if ( -1 != gameID ) {
                        m_sender.add( new BTQueueElem( BTCmd.MESG_SEND, buf, 
                                                       btAddr, gameID ) );
                    }
                    break;
                case RADIO:
                    boolean cameOn = intent.getBooleanExtra( RADIO_STR, false );
                    MultiEvent evt = cameOn? MultiEvent.BT_ENABLED
                        : MultiEvent.BT_DISABLED;
                    sendResult( evt );
                    if ( cameOn ) {
                        GameUtils.resendAllIf( this, CommsConnType.COMMS_CONN_BT,
                                               false );
                    } else {
                        ConnStatusHandler.updateStatus( this, null,
                                                        CommsConnType.COMMS_CONN_BT, 
                                                        false );
                        stopListener();
                        stopSender();
                        stopSelf();
                    }
                    break;
                case REMOVE:
                    gameID = intent.getIntExtra( GAMEID_STR, -1 );
                    break;
                default:
                    Assert.fail();
                }
            }
            result = Service.START_STICKY;
        } else {
            result = Service.START_STICKY_COMPATIBILITY;
        }
        return result;
    } // onStartCommand()

    private class BTListenerThread extends Thread {
        private BluetoothServerSocket m_serverSocket;

        @Override
        public void run() {     // receive thread
            try {
                String appName = XWApp.getAppName( BTService.this );
                m_serverSocket = m_adapter.
                    listenUsingRfcommWithServiceRecord( appName,
                                                        XWApp.getAppUUID() );
            } catch ( IOException ioe ) {
                m_serverSocket = null;
                logIOE( ioe );
            }

            while ( null != m_serverSocket && m_adapter.isEnabled() ) {

                BluetoothSocket socket = null;
                DataInputStream inStream = null;
                int nRead = 0;
                try {
                    socket = m_serverSocket.accept(); // blocks
                    addAddr( socket );
                    inStream = new DataInputStream( socket.getInputStream() );

                    byte proto = inStream.readByte();
                    if ( proto != BT_PROTO ) {
                        DataOutputStream os = new DataOutputStream( socket.getOutputStream() );
                        os.writeByte( BTCmd.BAD_PROTO.ordinal() );
                        os.flush();
                        socket.close();

                        sendBadProto( socket );
                    } else {
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
                        updateStatusIn( true );
                    }
                } catch ( IOException ioe ) {
                    DbgUtils.logf( "trying again..." );
                    logIOE( ioe);
                    continue;
                }
            }

            if ( null != m_serverSocket ) {
                try {
                    m_serverSocket.close();
                } catch ( IOException ioe ) {
                    logIOE( ioe );
                }
                m_serverSocket = null;
            }
        } // run()

        public void stopListening()
        {
            if ( null != m_serverSocket ) {
                try {
                    m_serverSocket.close();
                } catch ( IOException ioe ) {
                    logIOE( ioe );
                }
                m_serverSocket = null;
            }
            interrupt();
        }

        private void receivePing( BluetoothSocket socket ) throws IOException
        {
            DataInputStream inStream = new DataInputStream( socket.getInputStream() );
            int gameID = inStream.readInt();
            boolean deleted = 0 != gameID && !DBUtils.haveGame( BTService.this, gameID );

            DataOutputStream os = new DataOutputStream( socket.getOutputStream() );
            os.writeByte( BTCmd.PONG.ordinal() );
            os.writeBoolean( deleted );
            os.flush();

            socket.close();
            updateStatusOut( true );
        }

        private void receiveInvitation( Context context,
                                        DataInputStream is,
                                        BluetoothSocket socket )
            throws IOException
        {
            BTCmd result;
            String asJson = is.readUTF();
            NetLaunchInfo nli = new NetLaunchInfo( asJson );
            DbgUtils.logf( "receiveInvitation: nli: %s", nli.toString() );
            // int gameID = is.readInt();
            // String gameName = is.readUTF();
            // int lang = is.readInt();
            // String dict = is.readUTF();
            // int nPlayersT = is.readInt();
            // int nPlayersH = is.readInt();

            BluetoothDevice host = socket.getRemoteDevice();
            addAddr( host );

            result = makeOrNotify( context, nli, host.getName(), host.getAddress() );

            DataOutputStream os = new DataOutputStream( socket.getOutputStream() );
            os.writeByte( result.ordinal() );
            os.flush();

            socket.close();
        } // receiveInvitation

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

                    // check if still here
                    long[] rowids = DBUtils.getRowIDsFor( BTService.this, 
                                                          gameID );
                    boolean haveGame = null != rowids && 0 < rowids.length;
                    BTCmd result = haveGame ? 
                        BTCmd.MESG_ACCPT : BTCmd.MESG_GAMEGONE;

                    DataOutputStream os = 
                        new DataOutputStream( socket.getOutputStream() );
                    os.writeByte( result.ordinal() );
                    os.flush();
                    socket.close();

                    CommsAddrRec addr = new CommsAddrRec( host.getName(), 
                                                          host.getAddress() );

                    for ( long rowid : rowids ) {
                        boolean consumed = 
                            BoardDelegate.feedMessage( gameID, buffer, addr );
                        if ( !consumed && haveGame ) {
                            LastMoveInfo lmi = new LastMoveInfo();
                            if ( GameUtils.feedMessage( BTService.this, rowid, 
                                                        buffer, addr, 
                                                        m_btMsgSink, lmi ) ) {
                                consumed = true;
                                GameUtils.postMoveNotification( BTService.this,
                                                                rowid, lmi );
                            }
                        }
                        if ( !consumed ) {
                            DbgUtils.logf( "nobody took msg for gameID %X", 
                                           gameID );
                        }
                    }
                } else {
                    DbgUtils.logf( "receiveMessages: read only %d of %d bytes",
                                   nRead, len );
                }
            } catch ( IOException ioe ) {
                logIOE( ioe );
            }
        } // receiveMessage
    } // class BTListenerThread

    private void addAddr( BluetoothSocket socket )
    {
        addAddr( socket.getRemoteDevice() );
    }

    private void addAddr( BluetoothDevice dev )
    {
        addAddr( dev.getAddress(), dev.getName() );
    }

    private void addAddr( String btAddr, String btName )
    {
        boolean save = false;
        synchronized( m_addrs ) {
            save = !m_addrs.contains( btAddr );
            if ( save ) {
                m_addrs.add( btAddr );
            }
        }
        if ( save ) {
            saveAddrs();
        }
    }

    private boolean haveAddr( String btAddr )
    {
        return m_addrs.contains( btAddr );
    }

    private void clearDevs( String[] btAddrs )
    {
        if ( null != btAddrs ) {
            synchronized( m_addrs ) {
                for ( String btAddr : btAddrs ) {
                    m_addrs.remove( btAddr );
                }
            }
        }
        saveAddrs();
    }

    private class BTSenderThread extends Thread {
        private LinkedBlockingQueue<BTQueueElem> m_queue;
        private HashMap<String,LinkedList<BTQueueElem> > m_resends;
        // private HashSet<Integer> m_deadGames;

        public BTSenderThread()
        {
            m_queue = new LinkedBlockingQueue<BTQueueElem>();
            m_resends = new HashMap<String,LinkedList<BTQueueElem> >();
            // m_deadGames = new HashSet<Integer>();
        }

        public void add( BTQueueElem elem )
        {
            m_queue.add( elem );
        }

        @Override
        public void run()       // send thread
        {
            for ( ; ; ) {
                BTQueueElem elem;
                long timeout = haveResends() ? RESEND_TIMEOUT : Long.MAX_VALUE;
                try {
                    elem = m_queue.poll( timeout, TimeUnit.SECONDS );
                } catch ( InterruptedException ie ) {
                    DbgUtils.logf( "BTService: interrupted; killing thread" );
                    break;
                }

                if ( null == elem ) {
                    doAnyResends();
                } else {
                    // DbgUtils.logf( "run: got %s from queue", elem.m_cmd.toString() );

                    switch( elem.m_cmd ) {
                    case PING:
                        if ( null == elem.m_btAddr ) {
                            sendPings( MultiEvent.HOST_PONGED );
                        } else {
                            sendPing( elem.m_btAddr, elem.m_gameID );
                        }
                        break;
                    case SCAN:
                        addAllToNames();
                        sendNames();
                        saveAddrs();
                        break;
                    case INVITE:
                        sendInvite( elem );
                        break;
                    case MESG_SEND:
                        boolean success = doAnyResends( elem.m_btAddr )
                            && sendMsg( elem );
                        if ( !success ) {
                            addToResends( elem );
                        }
                        updateStatusOut( success );
                        break;
                    default:
                        Assert.fail();
                        break;
                    }
                }
            }
        } // run

        private void sendPings( MultiEvent event )
        {
            Set<BluetoothDevice> pairedDevs = m_adapter.getBondedDevices();
            // DbgUtils.logf( "ping: got %d paired devices", pairedDevs.size() );
            for ( BluetoothDevice dev : pairedDevs ) {
                String btAddr = dev.getAddress();
                if ( haveAddr( btAddr ) ) {
                    continue;
                }

                if ( sendPing( dev, 0 ) ) { // did we get a reply?
                    addAddr( dev );
                    if ( null != event ) {
                        sendResult( event, dev.getName() );
                    }
                }
            }
        }

        private boolean sendPing( BluetoothDevice dev, int gameID )
        {
            boolean gotReply = false;
            boolean sendWorking = false;
            boolean receiveWorking = false;
            try {
                BluetoothSocket socket =
                    dev.createRfcommSocketToServiceRecord( XWApp.getAppUUID() );
                if ( null != socket ) {
                    DataOutputStream os = connect( socket, BTCmd.PING );
                    if ( null != os ) {
                        os.writeInt( gameID );
                        os.flush();
                        Thread killer = killSocketIn( socket, 5 );

                        DataInputStream is = 
                            new DataInputStream( socket.getInputStream() );
                        BTCmd reply = BTCmd.values()[is.readByte()];
                        if ( BTCmd.BAD_PROTO == reply ) {
                            sendBadProto( socket );
                        } else {
                            gotReply = BTCmd.PONG == reply;
                            if ( gotReply && is.readBoolean() ) {
                                sendResult( MultiEvent.MESSAGE_NOGAME, gameID );
                            }
                        }

                        receiveWorking = true;
                        killer.interrupt();
                        sendWorking = true;
                    }
                    socket.close();
                }
            } catch ( IOException ioe ) {
                logIOE( ioe );
            }
            updateStatusOut( sendWorking );
            updateStatusIn( receiveWorking );
            return gotReply;
        } // sendPing

        private boolean sendPing( String btAddr, int gameID )
        {
            boolean success = false;
            BluetoothDevice dev = m_adapter.getRemoteDevice( btAddr );
            success = sendPing( dev, gameID );
            return success;
        }

        private void sendInvite( BTQueueElem elem )
        {
            try {
                BluetoothDevice dev = 
                    m_adapter.getRemoteDevice( elem.m_btAddr );
                BluetoothSocket socket = 
                    dev.createRfcommSocketToServiceRecord( XWApp.getAppUUID() );
                if ( null != socket ) {
                    BTCmd reply = null;
                    DataOutputStream outStream = connect( socket, BTCmd.INVITE );
                    if ( null != outStream ) {
                        outStream.writeUTF( elem.m_nli.toString() );
                        // outStream.writeInt( elem.m_gameID );
                        // outStream.writeUTF( elem.m_gameName );
                        // outStream.writeInt( elem.m_lang );
                        // outStream.writeUTF( elem.m_dict );
                        // outStream.writeInt( elem.m_nPlayersT );
                        DbgUtils.logf( "<eeh>sending invite for %d players", elem.m_nPlayersH );
                        outStream.flush();

                        DataInputStream inStream = 
                            new DataInputStream( socket.getInputStream() );
                        reply = BTCmd.values()[inStream.readByte()];
                    }
                    socket.close();

                    if ( null == reply ) {
                        sendResult( MultiEvent.APP_NOT_FOUND, dev.getName() );
                    } else if ( BTCmd.BAD_PROTO == reply ) {
                        sendBadProto( socket );
                    } else if ( BTCmd.INVITE_ACCPT == reply ) {
                        sendResult( MultiEvent.NEWGAME_SUCCESS, elem.m_gameID );
                    } else {                        
                        sendResult( MultiEvent.NEWGAME_FAILURE, elem.m_gameID );
                    }
                }
            } catch ( IOException ioe ) {
                logIOE( ioe );
            }
        } // sendInvite

        private boolean sendMsg( BTQueueElem elem )
        {
            boolean success = false;
            // synchronized( m_deadGames ) {
            //     success = m_deadGames.contains( elem.m_gameID );
            // }
            MultiEvent evt;
            if ( success ) {
                evt = MultiEvent.MESSAGE_DROPPED;
                DbgUtils.logf( "BTService.sendMsg: dropping message %s because game %X dead", 
                               elem.m_cmd, elem.m_gameID );
            } else {
                evt = MultiEvent.MESSAGE_REFUSED;
            }
            if ( !success ) {
                try {
                    BluetoothDevice dev = 
                        m_adapter.getRemoteDevice( elem.m_btAddr );
                    BluetoothSocket socket = dev.
                        createRfcommSocketToServiceRecord( XWApp.getAppUUID() );
                    if ( null != socket ) {
                        DataOutputStream outStream = 
                            connect( socket, BTCmd.MESG_SEND );
                        if ( null != outStream ) {
                            outStream.writeInt( elem.m_gameID );

                            short len = (short)elem.m_msg.length;
                            outStream.writeShort( len );
                            outStream.write( elem.m_msg, 0, elem.m_msg.length );

                            outStream.flush();
                            Thread killer = killSocketIn( socket );

                            DataInputStream inStream = 
                                new DataInputStream( socket.getInputStream() );
                            BTCmd reply = BTCmd.values()[inStream.readByte()];
                            killer.interrupt();
                            success = true;

                            switch ( reply ) {
                            case BAD_PROTO:
                                sendBadProto( socket );
                                evt = null;
                                break;
                            case MESG_ACCPT:
                                evt = MultiEvent.MESSAGE_ACCEPTED;
                                break;
                            case MESG_GAMEGONE:
                                evt = MultiEvent.MESSAGE_NOGAME;
                                break;
                            }
                        }
                        socket.close();
                    }
                } catch ( IOException ioe ) {
                    success = false;
                    logIOE( ioe );
                }
            }

            if ( null != evt ) {
                String btName = nameForAddr( m_adapter, elem.m_btAddr );
                sendResult( evt, elem.m_gameID, 0, btName );
                if ( ! success ) {
                    int failCount = elem.incrFailCount();
                    sendResult( MultiEvent.MESSAGE_RESEND, btName,
                                RESEND_TIMEOUT, failCount );
                }
            }
            return success;
        } // sendMsg

        private boolean doAnyResends( LinkedList<BTQueueElem> resends )
        {
            boolean success = null == resends || 0 == resends.size();
            if ( !success ) {
                success = true;
                ListIterator<BTQueueElem> iter = resends.listIterator();
                while ( iter.hasNext() && success ) {
                    BTQueueElem elem = iter.next();
                    success = sendMsg( elem );
                    if ( success ) {
                        iter.remove();
                    } else if ( elem.failCountExceeded() ) {
                        String btName = nameForAddr( m_adapter, elem.m_btAddr );
                        sendResult( MultiEvent.MESSAGE_FAILOUT, btName );
                        iter.remove();
                    }
                }
                
            }
            return success;
        }

        private boolean doAnyResends( String btAddr )
        {
            return doAnyResends( m_resends.get( btAddr ) );
        }

        private void doAnyResends()
        {
            Iterator<LinkedList<BTQueueElem>> iter =
                m_resends.values().iterator();
            while ( iter.hasNext() ) {
                LinkedList<BTQueueElem> list = iter.next();
                doAnyResends( list );
            }
        }

        private void addToResends( BTQueueElem elem )
        {
            String addr = elem.m_btAddr;
            LinkedList<BTQueueElem> resends = m_resends.get( addr );
            if ( null == resends ) {
                resends = new LinkedList<BTQueueElem>();
                m_resends.put( addr, resends );
            }
            resends.add( elem );
        }

        private boolean haveResends()
        {
            boolean found = false;
            Iterator<LinkedList<BTQueueElem>> iter =
                m_resends.values().iterator();
            while ( !found && iter.hasNext() ) {
                LinkedList<BTQueueElem> list = iter.next();
                found = 0 < list.size();
            }
            return found;
        }

    } // class BTSenderThread

    private void addAllToNames()
    {
        Set<BluetoothDevice> pairedDevs = m_adapter.getBondedDevices();
        synchronized( m_addrs ) {
            for ( BluetoothDevice dev : pairedDevs ) {
                m_addrs.add( dev.getAddress() );
            }
        }
    }

    private void sendNames()
    {
        String[] btAddrs = getAddrs();
        int size = btAddrs.length;
        String[] btNames = new String[size];
        for ( int ii = 0; ii < size; ++ii ) {
            btNames[ii] = nameForAddr( m_adapter, btAddrs[ii] );
        }
        sendResult( MultiEvent.SCAN_DONE, (Object)btAddrs, (Object)btNames );
    }

    private void initAddrs()
    {
        m_addrs = new HashSet<String>();

        String[] addrs = XWPrefs.getBTAddresses( this );
        if ( null != addrs ) {
            for ( String btAddr : addrs ) {
                m_addrs.add( btAddr );
            }
        }
    }

    private String[] getAddrs()
    {
        String[] addrs;
        synchronized( m_addrs ) {
            addrs = m_addrs.toArray( new String[m_addrs.size()] );
        }
        return addrs;
    }

    private void saveAddrs()
    {
        XWPrefs.setBTAddresses( this, getAddrs() );
    }

    private void startListener()
    {
        m_btMsgSink = new BTMsgSink();
        m_listener = new BTListenerThread();
        m_listener.start();
    }

    private void startSender()
    {
        m_sender = new BTSenderThread();
        m_sender.start();
    }

    private void stopListener()
    {
        m_listener.stopListening();
        try {
            m_listener.join( 100 );
        } catch ( InterruptedException ie ) {
            DbgUtils.loge( ie );
        }
        m_listener = null;
    }

    private void stopSender()
    {
        m_sender.interrupt();
        try {
            m_sender.join( 100 );
        } catch ( InterruptedException ie ) {
            DbgUtils.loge( ie );
        }
        m_sender = null;
    }

    private BTCmd makeOrNotify( Context context, NetLaunchInfo nli, 
                                String btName, String btAddr )
    {
        BTCmd result;
        if ( DictLangCache.haveDict( context, nli.lang, nli.dict ) ) {
            result = makeGame( context, nli, btName, btAddr );
        } else {
            Intent intent = MultiService.makeMissingDictIntent( context, nli );
            Assert.fail();
            // NetLaunchInfo.putExtras( intent, gameID, btName, btAddr );
            MultiService.postMissingDictNotification( context, intent, 
                                                      nli.gameID );
            result = BTCmd.INVITE_ACCPT; // ???
        }
        return result;
    }

    private BTCmd makeGame( Context context, NetLaunchInfo nli, 
                            String sender, String senderAddress )
    {
        BTCmd result;
        long[] rowids = DBUtils.getRowIDsFor( BTService.this, nli.gameID );
        if ( null == rowids || 0 == rowids.length ) {
            CommsAddrRec addr = new CommsAddrRec( sender, senderAddress );
            long rowid = GameUtils.makeNewGame( context, m_btMsgSink, nli.gameID, 
                                                addr, nli.lang, nli.dict, nli.nPlayersT, 
                                                nli.nPlayersH );
            if ( DBUtils.ROWID_NOTFOUND == rowid ) {
                result = BTCmd.INVITE_FAILED;
            } else {
                if ( null != nli.gameName && 0 < nli.gameName.length() ) {
                    DBUtils.setName( context, rowid, nli.gameName );
                }
                result = BTCmd.INVITE_ACCPT;
                String body = LocUtils.getString( BTService.this, 
                                                  R.string.new_bt_body_fmt, 
                                                  sender );
                postNotification( nli.gameID, R.string.new_bt_title, body, rowid );
                sendResult( MultiEvent.BT_GAME_CREATED, rowid );
            }
        } else {
            result = BTCmd.INVITE_DUPID;
        }
        return result;
    }

    private DataOutputStream connect( BluetoothSocket socket, BTCmd cmd )
    {
        DbgUtils.logf( "connecting to %s to send %s", 
                       socket.getRemoteDevice().getName(), cmd.toString() );
        // Docs say always call cancelDiscovery before trying to connect
        m_adapter.cancelDiscovery();
        
        DataOutputStream dos;
        try { 
            socket.connect();
            dos = new DataOutputStream( socket.getOutputStream() );
            dos.writeByte( BT_PROTO );
            dos.writeByte( cmd.ordinal() );
            DbgUtils.logf( "connect successful" );
        } catch ( IOException ioe ) {
            dos = null;
            logIOE( ioe );
        }
        return dos;
    }

    private void logIOE( IOException ioe )
    {
        DbgUtils.loge( ioe );
        ++s_errCount;
        // if ( 0 == s_errCount % 10 ) {
            sendResult( MultiEvent.BT_ERR_COUNT, s_errCount );
            // }
    }

    private void sendBadProto( BluetoothSocket socket )
    {
        sendResult( MultiEvent.BAD_PROTO, socket.getRemoteDevice().getName() );
    }

    private void postNotification( int gameID, int title, String body, 
                                   long rowid )
    {
        Intent intent = GamesListDelegate.makeGameIDIntent( this, gameID );
        Utils.postNotification( this, intent, R.string.new_btmove_title, 
                                body, (int)rowid );
    }

    private void updateStatusOut( boolean success )
    {
        ConnStatusHandler
            .updateStatusOut( this, null, CommsConnType.COMMS_CONN_BT, success );
    }

    private void updateStatusIn( boolean success )
    {
        ConnStatusHandler
            .updateStatusIn( this, null, CommsConnType.COMMS_CONN_BT, success );
    }

    private Thread killSocketIn( final BluetoothSocket socket )
    {
        return killSocketIn( socket, 10 );
    }

    private Thread killSocketIn( final BluetoothSocket socket, int seconds )
    {
        final int millis = seconds * 1000;
        Thread thread = new Thread( new Runnable() {
                public void run() {
                    try {
                        Thread.sleep( millis );
                    } catch ( InterruptedException ie ) {
                        DbgUtils.logf( "killSocketIn: killed by owner" );
                        return;
                    }
                    try {
                        socket.close();
                    } catch( IOException ioe ) {
                        DbgUtils.loge( ioe );
                    }
                }
            } );
        thread.start();
        return thread;
    }

    private class BTMsgSink extends MultiMsgSink {

        /***** TransportProcs interface *****/

        public int transportSend( byte[] buf, final CommsAddrRec addr, int gameID )
        {
            int sent = -1;
            if ( null != addr ) {
                m_sender.add( new BTQueueElem( BTCmd.MESG_SEND, buf, 
                                               addr.bt_btAddr, gameID ) );
                sent = buf.length;
            } else {
                DbgUtils.logf( "BTMsgSink.transportSend: "
                               + "addr null so not sending" );
            }
            return sent;
        }
    }

}
