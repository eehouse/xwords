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
import android.bluetooth.BluetoothClass.Device.Major;
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
import java.util.Map;
import java.util.Set;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

import org.eehouse.android.xw4.MultiService.DictFetchOwner;
import org.eehouse.android.xw4.MultiService.MultiEvent;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.LastMoveInfo;
import org.eehouse.android.xw4.loc.LocUtils;

import junit.framework.Assert;

public class BTService extends XWService {
    private static final String BOGUS_MARSHMALLOW_ADDR = "02:00:00:00:00:00";

    private static final long RESEND_TIMEOUT = 5; // seconds
    private static final int MAX_SEND_FAIL = 3;

    private static final int BT_PROTO = 1; // using jsons instead of lots of fields

    private enum BTAction { _NONE,
                            SCAN,
                            INVITE,
                            SEND,
                            RADIO,
                            CLEAR,
                            REMOVE,
                            NFCINVITE,
                            PINGHOST,
    };

    private static final String CMD_KEY = "CMD";
    private static final String MSG_KEY = "MSG";
    private static final String GAMENAME_KEY = "NAM";
    private static final String ADDR_KEY = "ADR";
    private static final String RADIO_KEY = "RDO";
    private static final String CLEAR_KEY = "CLR";

    private static final String GAMEID_KEY = "GMI";
    private static final String GAMEDATA_KEY = "GD";

    private static final String LANG_KEY = "LNG";
    private static final String DICT_KEY = "DCT";
    private static final String NTO_KEY = "TOT";
    private static final String NHE_KEY = "HER";
    private static final String BT_NAME_KEY = "BT_NAME";
    private static final String BT_ADDRESS_KEY = "BT_ADDRESS";

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
            checkAddr();
        }
        public BTQueueElem( BTCmd cmd, String btAddr, int gameID ) {
            this( cmd );
            Assert.assertTrue( null != btAddr && 0 < btAddr.length() );
            m_btAddr = btAddr;
            m_gameID = gameID;
            checkAddr();
        }

        public BTQueueElem( BTCmd cmd, NetLaunchInfo nli, String btAddr ) {
            this( cmd );
            m_nli = nli;
            m_btAddr = btAddr;
            checkAddr();
        }

        public int incrFailCount() { return ++m_failCount; }
        public boolean failCountExceeded() { return m_failCount >= MAX_SEND_FAIL; }

        private void checkAddr()
        {
            Assert.assertFalse( BOGUS_MARSHMALLOW_ADDR.equals( m_btAddr ) );
        }
    }

    private BluetoothAdapter m_adapter;
    private Set<String> m_addrs;
    private BTMsgSink m_btMsgSink;
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

    public static void enable()
    {
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        if ( null != adapter ) {
            // Only do this after explicit action from user -- Android guidelines
            adapter.enable();
        }
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
        Intent intent = getIntentTo( context, BTAction.RADIO );
        intent.putExtra( RADIO_KEY, cameOn );
        context.startService( intent );
    }

    public static void clearDevices( Context context, String[] btAddrs )
    {
        Intent intent = getIntentTo( context, BTAction.CLEAR );
        intent.putExtra( CLEAR_KEY, btAddrs );
        context.startService( intent );
    }

    public static void scan( Context context )
    {
        Intent intent = getIntentTo( context, BTAction.SCAN );
        context.startService( intent );
    }

    public static void pingHost( Context context, String hostAddr, int gameID )
    {
        Assert.assertTrue( null != hostAddr && 0 < hostAddr.length() );
        Intent intent = getIntentTo( context, BTAction.PINGHOST );
        intent.putExtra( ADDR_KEY, hostAddr );
        intent.putExtra( GAMEID_KEY, gameID );
        context.startService( intent );
    } 

    // public static void inviteRemote( Context context, String btAddr, 
    //                                  int gameID, String initialName, int lang, 
    //                                  String dict, int nPlayersT, int nPlayersH )
    // {
    //     Intent intent = getIntentTo( context, INVITE );
    //     intent.putExtra( GAMEID_KEY, gameID );
    //     intent.putExtra( ADDR_KEY, btAddr );
    //     Assert.assertNotNull( initialName );
    //     intent.putExtra( GAMENAME_KEY, initialName );
    //     intent.putExtra( LANG_KEY, lang );
    //     intent.putExtra( DICT_KEY, dict );
    //     intent.putExtra( NTO_KEY, nPlayersT );
    //     intent.putExtra( NHE_KEY, nPlayersH );

    //     context.startService( intent );
    // }

    public static void inviteRemote( Context context, String btAddr, 
                                     NetLaunchInfo nli )
    {
        Intent intent = getIntentTo( context, BTAction.INVITE );
        String nliData = nli.toString();
        intent.putExtra( GAMEDATA_KEY, nliData );
        intent.putExtra( ADDR_KEY, btAddr );

        context.startService( intent );
    }

    public static void gotGameViaNFC( Context context, NetLaunchInfo bli )
    {
        Intent intent = getIntentTo( context, BTAction.NFCINVITE );
        intent.putExtra( GAMEID_KEY, bli.gameID() );
        intent.putExtra( DICT_KEY, bli.dict );
        intent.putExtra( LANG_KEY, bli.lang );
        intent.putExtra( NTO_KEY, bli.nPlayersT );
        intent.putExtra( BT_NAME_KEY, bli.btName );
        intent.putExtra( BT_ADDRESS_KEY, bli.btAddress );

        context.startService( intent );
    }

    public static int enqueueFor( Context context, byte[] buf, 
                                  CommsAddrRec targetAddr, int gameID )
    {
        int nSent = -1;
        if ( null != targetAddr ) {
            String btAddr = getSafeAddr( targetAddr );
            Intent intent = getIntentTo( context, BTAction.SEND );
            intent.putExtra( MSG_KEY, buf );
            intent.putExtra( ADDR_KEY, btAddr );
            intent.putExtra( GAMEID_KEY, gameID );
            context.startService( intent );
            nSent = buf.length;
        } else {
            DbgUtils.logf( "BTService.enqueueFor(): targetAddr is null" );
        }
        return nSent;
    }
    
    public static void gameDied( Context context, int gameID )
    {
        Intent intent = getIntentTo( context, BTAction.REMOVE );
        intent.putExtra( GAMEID_KEY, gameID );
        context.startService( intent );
    }

    private static Intent getIntentTo( Context context, BTAction cmd )
    {
        Intent intent = new Intent( context, BTService.class );
        intent.putExtra( CMD_KEY, cmd.ordinal() );
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
            int ordinal = intent.getIntExtra( CMD_KEY, -1 );
            if ( -1 == ordinal ) {
                // Drop it
            } else if ( null == m_sender ) {
                DbgUtils.logf( "exiting: m_queue is null" );
                stopSelf();
            } else {
                BTAction cmd = BTAction.values()[ordinal];
                DbgUtils.logf( "BTService.onStartCommand; cmd=%s", cmd.toString() );
                switch( cmd ) {
                case CLEAR:
                    String[] btAddrs = intent.getStringArrayExtra( CLEAR_KEY );
                    clearDevs( btAddrs );
                    sendNames();
                    break;
                case SCAN:
                    m_sender.add( new BTQueueElem( BTCmd.SCAN ) );
                    break;
                case INVITE:
                    String jsonData = intent.getStringExtra( GAMEDATA_KEY );
                    NetLaunchInfo nli = new NetLaunchInfo( this, jsonData );
                    DbgUtils.logf( "onStartCommand: nli: %s", nli.toString() );
                    // int gameID = intent.getIntExtra( GAMEID_KEY, -1 );
                    // String btAddr = intent.getStringExtra( ADDR_KEY );
                    // String gameName = intent.getStringExtra( GAMENAME_KEY );
                    // int lang = intent.getIntExtra( LANG_KEY, -1 );
                    // String dict = intent.getStringExtra( DICT_KEY );
                    // int nPlayersT = intent.getIntExtra( NTO_KEY, -1 );
                    String btAddr = intent.getStringExtra( ADDR_KEY );
                    m_sender.add( new BTQueueElem( BTCmd.INVITE, nli, btAddr ) );
                    break;

                case PINGHOST:
                    btAddr = intent.getStringExtra( ADDR_KEY );
                    int gameID = intent.getIntExtra( GAMEID_KEY, 0 );
                    m_sender.add( new BTQueueElem( BTCmd.PING, btAddr, gameID ) );
                    break;

                case NFCINVITE:
                    gameID = intent.getIntExtra( GAMEID_KEY, -1 );
                    int lang = intent.getIntExtra( LANG_KEY, -1 );
                    String dict = intent.getStringExtra( DICT_KEY );
                    int nPlayersT = intent.getIntExtra( NTO_KEY, -1 );
                    String btName = intent.getStringExtra( BT_NAME_KEY );
                    btAddr = intent.getStringExtra( BT_ADDRESS_KEY );
                    // /*(void)*/makeOrNotify( this, gameID, null, lang, dict, 
                    //                         nPlayersT, 1, btName, btAddr );
                    Assert.fail();
                    break;

                case SEND:
                    byte[] buf = intent.getByteArrayExtra( MSG_KEY );
                    btAddr = intent.getStringExtra( ADDR_KEY );
                    gameID = intent.getIntExtra( GAMEID_KEY, -1 );
                    if ( -1 != gameID ) {
                        m_sender.add( new BTQueueElem( BTCmd.MESG_SEND, buf, 
                                                       btAddr, gameID ) );
                    }
                    break;
                case RADIO:
                    boolean cameOn = intent.getBooleanExtra( RADIO_KEY, false );
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
                    gameID = intent.getIntExtra( GAMEID_KEY, -1 );
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
                            receiveInvitation( inStream, socket );
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

        private void receiveInvitation( DataInputStream is,
                                        BluetoothSocket socket )
            throws IOException
        {
            BTCmd result;
            String asJson = is.readUTF();
            NetLaunchInfo nli = new NetLaunchInfo( BTService.this, asJson );

            BluetoothDevice host = socket.getRemoteDevice();
            addAddr( host );

            result = makeOrNotify( nli, host.getName(), host.getAddress() );

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
                            BoardDelegate.feedMessage( rowid, buffer, addr );
                        if ( !consumed && haveGame ) {
                            GameUtils.BackMoveResult bmr = 
                                new GameUtils.BackMoveResult();
                            if ( GameUtils.feedMessage( BTService.this, rowid, 
                                                        buffer, addr, 
                                                        m_btMsgSink, bmr ) ) {
                                consumed = true;
                                GameUtils.postMoveNotification( BTService.this,
                                                                rowid, bmr );
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

    private static Map<String, String> s_namesToAddrs;
    private static String getSafeAddr( CommsAddrRec addr )
    {
        String btAddr = addr.bt_btAddr;
        if ( BOGUS_MARSHMALLOW_ADDR.equals( btAddr ) ) {
            String btName = addr.bt_hostName;
            if ( null == s_namesToAddrs ) {
                s_namesToAddrs = new HashMap<String, String>();
            }
            if ( ! s_namesToAddrs.containsKey( btName ) ) {
                BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
                if ( null != adapter ) {
                    Set<BluetoothDevice> devs = adapter.getBondedDevices();
                    Iterator<BluetoothDevice> iter = devs.iterator();
                    while ( iter.hasNext() ) {
                        BluetoothDevice dev = iter.next();
                        s_namesToAddrs.put( dev.getName(), dev.getAddress() );
                    }
                }
            }

            btAddr = s_namesToAddrs.get( btName );
        }
        return btAddr;
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
                        DbgUtils.logf( "<eeh>sending invite for %d players", elem.m_nPlayersH );
                        outStream.flush();

                        DataInputStream inStream = 
                            new DataInputStream( socket.getInputStream() );
                        reply = BTCmd.values()[inStream.readByte()];
                    }

                    if ( null == reply ) {
                        sendResult( MultiEvent.APP_NOT_FOUND, dev.getName() );
                    } else {
                        switch ( reply ) {
                        case BAD_PROTO:
                            sendBadProto( socket );
                            break;
                        case INVITE_ACCPT:
                            sendResult( MultiEvent.NEWGAME_SUCCESS, elem.m_gameID );
                            break;
                        case INVITE_DUPID:
                            sendResult( MultiEvent.NEWGAME_DUP_REJECTED, dev.getName() );
                            break;
                        default:
                            sendResult( MultiEvent.NEWGAME_FAILURE, elem.m_gameID );
                            break;
                        }
                    }

                    socket.close();
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
                int clazz = dev.getBluetoothClass().getMajorDeviceClass();
                if ( Major.PHONE == clazz || Major.COMPUTER == clazz ) {
                    m_addrs.add( dev.getAddress() );
                }
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

    private BTCmd makeOrNotify( NetLaunchInfo nli, String btName, 
                                String btAddr )
    {
        BTCmd result;
        if ( DictLangCache.haveDict( this, nli.lang, nli.dict ) ) {
            result = makeGame( nli, btName, btAddr );
        } else {
            Intent intent = MultiService
                .makeMissingDictIntent( this, nli, 
                                        DictFetchOwner.OWNER_BT );
            // NetLaunchInfo.putExtras( intent, gameID, btName, btAddr );
            MultiService.postMissingDictNotification( this, intent, 
                                                      nli.gameID() );
            result = BTCmd.INVITE_ACCPT; // ???
        }
        return result;
    }

    private BTCmd makeGame( NetLaunchInfo nli, String sender, 
                            String senderAddress )
    {
        BTCmd result;
        long[] rowids = DBUtils.getRowIDsFor( BTService.this, nli.gameID() );
        if ( null == rowids || 0 == rowids.length ) {
            CommsAddrRec addr = nli.makeAddrRec( BTService.this );
            long rowid = GameUtils.makeNewMultiGame( BTService.this, nli, m_btMsgSink );
            if ( DBUtils.ROWID_NOTFOUND == rowid ) {
                result = BTCmd.INVITE_FAILED;
            } else {
                if ( null != nli.gameName && 0 < nli.gameName.length() ) {
                    DBUtils.setName( BTService.this, rowid, nli.gameName );
                }
                result = BTCmd.INVITE_ACCPT;
                String body = LocUtils.getString( BTService.this, 
                                                  R.string.new_bt_body_fmt, 
                                                  sender );
                postNotification( nli.gameID(), R.string.new_bt_title, body, rowid );
                sendResult( MultiEvent.BT_GAME_CREATED, rowid );
            }
        } else {
            result = BTCmd.INVITE_DUPID;
        }
        return result;
    }

    private DataOutputStream connect( BluetoothSocket socket, BTCmd cmd )
    {
        String name = socket.getRemoteDevice().getName();
        // DbgUtils.logf( "connecting to %s to send cmd %s", name, cmd.toString() );
        // Docs say always call cancelDiscovery before trying to connect
        m_adapter.cancelDiscovery();
        
        DataOutputStream dos;
        try { 
            socket.connect();
            dos = new DataOutputStream( socket.getOutputStream() );
            dos.writeByte( BT_PROTO );
            dos.writeByte( cmd.ordinal() );
            DbgUtils.logf( "connect() to %s successful", name );
        } catch ( IOException ioe ) {
            dos = null;
            DbgUtils.logf( "BTService.connect() to %s failed", name );
            // logIOE( ioe );
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
        sendResult( MultiEvent.BAD_PROTO_BT, socket.getRemoteDevice().getName() );
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

        public BTMsgSink() { super( BTService.this ); }

        @Override
        public int sendViaBluetooth( byte[] buf, int gameID, CommsAddrRec addr )
        {
            String btAddr = getSafeAddr( addr );
            
            Assert.assertTrue( addr.contains( CommsConnType.COMMS_CONN_BT ) );
            m_sender.add( new BTQueueElem( BTCmd.MESG_SEND, buf,
                                           btAddr, gameID ) );
            return buf.length;
        }
    }

}
