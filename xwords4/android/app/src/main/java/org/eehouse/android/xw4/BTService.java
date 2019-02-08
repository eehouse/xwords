/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
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
import android.app.Notification;
import android.app.PendingIntent;
import android.app.Service;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothClass.Device.Major;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothServerSocket;
import android.bluetooth.BluetoothSocket;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Handler;
import android.os.SystemClock;
import android.provider.Settings;
import android.support.v4.app.NotificationCompat;

import org.eehouse.android.xw4.MultiService.DictFetchOwner;
import org.eehouse.android.xw4.MultiService.MultiEvent;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.ListIterator;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

// Notes on running under Oreo
//
// The goal is to be running only when useful: when user wants to play via
// BT. Ideally we'd not run the foreground service when BT is turned off, but
// there's no way to be notified of its being turned on without installing a
// receiver. So *something* has to be running, whether it's listening on a BT
// socket or hosting the receiver. Might as well be the socket listener. Users
// who want the service not running when BT's off can turn it off via app
// preferences. When they try to start a BT game we can note the preference
// and suggest they change it. And when an invitation sent is not received the
// sender can be told to suggest to his opponent to turn on the preference.
//
// When there's no BT adapter at all (Emulator case) there's no point in
// starting the service. Let's catch that early.
//
// Note also that we need to be careful to NEVER call stopSelf() after calling
// startForegroundService() until AFTER calling startForeground(). Doing so
// will cause a crash.

public class BTService extends XWJIService {
    private static final String TAG = BTService.class.getSimpleName();
    private static final String BOGUS_MARSHMALLOW_ADDR = "02:00:00:00:00:00";
    private static final String KEY_KEEPALIVE_UNTIL_SECS = "keep_secs";
    private final static int sJobID = 218719979;
    // half minute for testing; maybe 15 on ship? Or make it a debug config.
    private static int DEFAULT_KEEPALIVE_SECONDS = 15 * 60;
    private static int CONNECT_SLEEP_MS = 1000;

    private static final long RESEND_TIMEOUT = 5; // seconds
    private static final int MAX_SEND_FAIL = 3;
    private static final int CONNECT_TIMEOUT_MS = 10000;

    private static final int BT_PROTO_ORIG = 0;
    private static final int BT_PROTO_JSONS = 1; // using jsons instead of lots of fields
    private static final int BT_PROTO_NLI = 2; // using binary/common form of NLI
    private static final int BT_PROTO = BT_PROTO_JSONS; // change in a release or two

    private enum BTAction implements XWJICmds { _NONE,
                                                ACL_CONN,
                                                START_BACKGROUND,
                                                SCAN,
                                                INVITE,
                                                SEND,
                                                RADIO,
                                                REMOVE,
                                                PINGHOST,

                                                // Pass to main work thread
                                                MAKE_OR_NOTIFY,
                                                RECEIVE_MSG,
                                                POST_GAME_GONE,
    };

    private static final String MSG_KEY = "MSG";
    private static final String GAMENAME_KEY = "NAM";
    private static final String ADDR_KEY = "ADR";
    private static final String SCAN_TIMEOUT_KEY = "SCAN_TIMEOUT";
    private static final String RADIO_KEY = "RDO";
    private static final String SOCKET_REF = "SOCKET";
    private static final String NLI_KEY = "NLI";

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
        INVITE_DUP_INVITE,
    };

    private class BTQueueElem {
        int m_failCount;
        int m_timeout;
        // These should perhaps be in some subclasses....
        BTCmd m_cmd;
        byte[] m_msg;
        String m_btAddr;
        String m_gameName;
        int m_gameID;
        int m_lang;
        String m_dict;
        int m_nPlayersT;
        NetLaunchInfo m_nli;

        public BTQueueElem( BTCmd cmd ) { m_cmd = cmd; m_failCount = 0; }
        public BTQueueElem( BTCmd cmd, int timeout )
        {
            this(cmd);
            m_timeout = timeout;
        }

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
    private BTMsgSink m_btMsgSink;
    private Notification m_notification; // make once use many
    private Handler mHandler;
    private BTServiceHelper mHelper;

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
        intent.setAction( Settings.ACTION_BLUETOOTH_SETTINGS );
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

    private static void writeBack( BluetoothSocket socket, BTCmd result )
    {
        try {
            DataOutputStream os = new DataOutputStream( socket.getOutputStream() );
            os.writeByte( result.ordinal() );
            os.flush();
        } catch ( IOException ex ) {
            Log.ex( TAG, ex );
        }
    }

    private static void enqueueWork( Intent intent )
    {
        Context context = XWApp.getContext();
        enqueueWork( context, intent );
    }

    private static void enqueueWork( Context context, Intent intent )
    {
        enqueueWork( context, BTService.class, sJobID, intent );
        Log.d( TAG, "called enqueueWork(cmd=%s)",
               cmdFrom( intent, BTAction.values() ) );
    }

    public static void onACLConnected( Context context )
    {
        Log.d( TAG, "onACLConnected()" );
        enqueueWork( context,
                     getIntentTo( context, BTAction.ACL_CONN ) );

    }

    public static void radioChanged( Context context, boolean cameOn )
    {
        Intent intent = getIntentTo( context, BTAction.RADIO )
            .putExtra( RADIO_KEY, cameOn );
        enqueueWork( context, intent );
    }

    public static void scan( Context context, int timeoutMS )
    {
        Intent intenet = getIntentTo( context, BTAction.SCAN )
            .putExtra( SCAN_TIMEOUT_KEY, timeoutMS );
        enqueueWork( context, intenet );
    }

    public static void pingHost( Context context, String hostAddr, int gameID )
    {
        Assert.assertTrue( null != hostAddr && 0 < hostAddr.length() );
        Intent intent = getIntentTo( context, BTAction.PINGHOST )
            .putExtra( ADDR_KEY, hostAddr )
            .putExtra( GAMEID_KEY, gameID );
        enqueueWork( context, intent );
    }

    public static void inviteRemote( Context context, String btAddr,
                                     NetLaunchInfo nli )
    {
        Assert.assertTrue( null != btAddr && 0 < btAddr.length() );
        Intent intent = getIntentTo( context, BTAction.INVITE )
            .putExtra( GAMEDATA_KEY, nli.toString() )
            .putExtra( ADDR_KEY, btAddr );
        enqueueWork( context, intent );
    }

    public static int enqueueFor( Context context, byte[] buf,
                                  CommsAddrRec targetAddr, int gameID )
    {
        int nSent = -1;
        Assert.assertNotNull( targetAddr );
        String btAddr = getSafeAddr( targetAddr );
        if ( null != btAddr && 0 < btAddr.length() ) {
            Intent intent = getIntentTo( context, BTAction.SEND )
                .putExtra( MSG_KEY, buf )
                .putExtra( ADDR_KEY, btAddr )
                .putExtra( GAMEID_KEY, gameID );
            enqueueWork( context, intent );
            nSent = buf.length;
        }

        if ( -1 == nSent ) {
            Log.i( TAG, "enqueueFor(): can't send to %s",
                   targetAddr.bt_hostName );
        }
        return nSent;
    }

    public static void gameDied( Context context, String btAddr, int gameID )
    {
        Assert.assertNotNull( btAddr );
        Intent intent = getIntentTo( context, BTAction.REMOVE )
            .putExtra( GAMEID_KEY, gameID )
            .putExtra( ADDR_KEY, btAddr );
        enqueueWork( context, intent );
    }

    private static Intent getIntentTo( Context context, BTAction cmd )
    {
        return getIntentTo( context, BTService.class, cmd );
    }

    private static Intent getIntentTo( BTAction cmd )
    {
        Context context = XWApp.getContext();
        return getIntentTo( context, cmd );
    }

    @Override
    public void onCreate()
    {
        Log.d( TAG, "%s.onCreate()", this );
        super.onCreate();

        mHelper = new BTServiceHelper( this );

        m_btMsgSink = new BTMsgSink();
        mHandler = new Handler();

        BluetoothAdapter adapter = XWApp.BTSUPPORTED
            ? BluetoothAdapter.getDefaultAdapter() : null;
        if ( null != adapter && adapter.isEnabled() ) {
            m_adapter = adapter;
            Log.i( TAG, "onCreate(); bt name = %s; bt addr = %s",
                   adapter.getName(), adapter.getAddress() );
            startListener();
        } else {
            Log.w( TAG, "not starting threads: BT not available" );
            stopSelf();
        }
    }

    @Override
    public void onDestroy()
    {
        super.onDestroy();
        releaseSender( this );
    }

    @Override
    XWJICmds[] getCmds() { return BTAction.values(); }

    @Override
    void onHandleWorkImpl( Intent intent, XWJICmds jicmd, long timestamp )
    {
        BTAction cmd = (BTAction)jicmd;
        switch( cmd ) {
        case ACL_CONN:          // just forces onCreate to run
            break;

        case START_BACKGROUND:
            noteLastUsed( this );   // prevent timer from killing immediately
            setTimeoutTimer();
            break;

        case SCAN:
            int timeout = intent.getIntExtra( SCAN_TIMEOUT_KEY, -1 );
            add( new BTQueueElem( BTCmd.SCAN, timeout ) );
            break;
        case INVITE:
            String jsonData = intent.getStringExtra( GAMEDATA_KEY );
            NetLaunchInfo nli = NetLaunchInfo.makeFrom( this, jsonData );
            Log.i( TAG, "handleCommand: nli: %s", nli );
            String btAddr = intent.getStringExtra( ADDR_KEY );
            add( new BTQueueElem( BTCmd.INVITE, nli, btAddr ) );
            break;

        case PINGHOST:
            btAddr = intent.getStringExtra( ADDR_KEY );
            int gameID = intent.getIntExtra( GAMEID_KEY, 0 );
            add( new BTQueueElem( BTCmd.PING, btAddr, gameID ) );
            break;

        case SEND:
            byte[] buf = intent.getByteArrayExtra( MSG_KEY );
            btAddr = intent.getStringExtra( ADDR_KEY );
            gameID = intent.getIntExtra( GAMEID_KEY, -1 );
            if ( -1 != gameID ) {
                add( new BTQueueElem( BTCmd.MESG_SEND, buf,
                                               btAddr, gameID ) );
            }
            break;
        case RADIO:
            boolean cameOn = intent.getBooleanExtra( RADIO_KEY, false );
            MultiEvent evt = cameOn? MultiEvent.BT_ENABLED
                : MultiEvent.BT_DISABLED;
            mHelper.postEvent( evt );
            if ( cameOn ) {
                GameUtils.resendAllIf( this, CommsConnType.COMMS_CONN_BT );
            } else {
                ConnStatusHandler.updateStatus( this, null,
                                                CommsConnType.COMMS_CONN_BT,
                                                false );
                stopListener();
                // stopSender();
                stopSelf();
            }
            break;
        case REMOVE:
            gameID = intent.getIntExtra( GAMEID_KEY, -1 );
            btAddr = intent.getStringExtra( ADDR_KEY );
            add( new BTQueueElem( BTCmd.MESG_GAMEGONE, btAddr, gameID ) );
            break;

        case MAKE_OR_NOTIFY:
            int socketRef = intent.getIntExtra( SOCKET_REF, -1 );
            BluetoothSocket socket = socketForRef( socketRef  );
            if ( null == socket ) {
                Log.e( TAG, "socket didn't survive into onHandleWork()" );
            } else {
                nli = (NetLaunchInfo)intent.getSerializableExtra( NLI_KEY );
                BluetoothDevice host = socket.getRemoteDevice();
                BTCmd response = makeOrNotify( nli, host.getName(), host.getAddress() );

                writeBack( socket, response );

                closeForRef( socketRef );
            }
            break;

        case RECEIVE_MSG:
            socketRef = intent.getIntExtra( SOCKET_REF, -1 );
            socket = socketForRef( socketRef  );
            if ( null != socket ) {
                gameID = intent.getIntExtra( GAMEID_KEY, -1 );
                buf = intent.getByteArrayExtra( MSG_KEY );
                BluetoothDevice host = socket.getRemoteDevice();
                CommsAddrRec addr = new CommsAddrRec( host.getName(),
                                                      host.getAddress() );
                XWServiceHelper.ReceiveResult rslt
                    = mHelper.receiveMessage( this, gameID,
                                              m_btMsgSink,
                                              buf, addr );

                BTCmd response = rslt == XWServiceHelper.ReceiveResult.GAME_GONE ?
                    BTCmd.MESG_GAMEGONE : BTCmd.MESG_ACCPT;
                writeBack( socket, response );
                closeForRef( socketRef );
            }
            break;

        case POST_GAME_GONE:
            socketRef = intent.getIntExtra( SOCKET_REF, -1 );
            socket = socketForRef( socketRef  );
            if ( null != socket ) {
                gameID = intent.getIntExtra( GAMEID_KEY, -1 );
                mHelper.postEvent( MultiEvent.MESSAGE_NOGAME, gameID );
                writeBack( socket, BTCmd.MESG_ACCPT );
                closeForRef( socketRef );
            }
            break;
                
        default:
            Assert.fail();
        }
    } // onHandleWorkImpl()

    private void add( BTQueueElem elem )
    {
        senderFor( this ).add( elem );
    }

    private static class BTListenerThread extends Thread {
        // Wrap so we can synchronize on the container
        private static BTListenerThread[] s_listener = {null};
        private BluetoothServerSocket m_serverSocket;
        private volatile Thread mTimerThread;

        private BTListenerThread() {}

        static void startYourself()
        {
            Log.d( TAG, "startYourself()" );
            synchronized ( s_listener ) {
                if ( s_listener[0] == null ) {
                    s_listener[0] = new BTListenerThread();
                    s_listener[0].start();
                }
            }
        }

        static void stopYourself( BTListenerThread self )
        {
            Log.d( TAG, "stopYourself()" );
            BTListenerThread curListener;
            synchronized ( s_listener ) {
                curListener = s_listener[0];
                s_listener[0] = null;
            }
            if ( null != curListener ) {
                Assert.assertTrue( self == null || self == curListener );
                curListener.stopListening();
            }
        }

        @Override
        public void run() {     // receive thread
            BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
            String appName = XWApp.getAppName( XWApp.getContext() );

            try {
                m_serverSocket = adapter.
                    listenUsingRfcommWithServiceRecord( appName,
                                                        XWApp.getAppUUID() );
            } catch ( IOException ioe ) {
                m_serverSocket = null;
                logIOE( ioe );
            }

            while ( null != m_serverSocket && adapter.isEnabled() ) {
                try {
                    Log.d( TAG, "%s.run() calling accept()", this );
                    BluetoothSocket socket = m_serverSocket.accept(); // blocks
                    Log.d( TAG, "accept() => %s", socket );
                    DataInputStream inStream =
                        new DataInputStream( socket.getInputStream() );

                    byte proto = inStream.readByte();
                    BTCmd cmd = BTCmd.values()[inStream.readByte()];
                    Log.d( TAG, "BTListenerThread() got %s", cmd );
                    if ( protoOK( proto, cmd ) ) {
                        process( socket, proto, inStream, cmd );
                    } else {
                        writeBack( socket, BTCmd.BAD_PROTO );
                    }
                } catch ( IOException ioe ) {
                    Log.w( TAG, "trying again..." );
                    logIOE( ioe);
                    continue;
                } catch ( NullPointerException npe ) {
                    continue;   // m_serverSocket probably null
                }
            }

            closeServerSocket();
            stopYourself( this ); // need to clear the ref so can restart
            Log.d( TAG, "BTListenerThread.run() exiting" );
        } // run()

        private void process( BluetoothSocket socket, byte proto,
                              DataInputStream inStream, BTCmd cmd )
            throws IOException
        {
            switch( cmd ) {
            case PING:
                receivePing( socket );
                break;
            case INVITE:
                receiveInvitation( proto, inStream, socket );
                break;
            case MESG_SEND:
                receiveMessage( cmd, inStream, socket );
                break;

            case MESG_GAMEGONE:
                receiveMessage( cmd, inStream, socket );
                break;

            default:
                Log.e( TAG, "unexpected msg %s", cmd.toString());
                break;
            }
        }

        public void stopListening()
        {
            closeServerSocket();
            interrupt();
        }

        private synchronized void closeServerSocket()
        {
            if ( null != m_serverSocket ) {
                try {
                    m_serverSocket.close();
                } catch ( IOException ioe ) {
                    logIOE( ioe );
                }
                m_serverSocket = null;
            }
        }

        private boolean protoOK( byte proto, BTCmd cmd )
        {
            boolean ok = proto == BT_PROTO_NLI || proto == BT_PROTO_JSONS;
            return ok;
        }

        private void receivePing( BluetoothSocket socket ) throws IOException
        {
            DataInputStream inStream = new DataInputStream( socket.getInputStream() );
            int gameID = inStream.readInt();
            boolean deleted = 0 != gameID && !DBUtils
                .haveGame( XWApp.getContext(), gameID );

            DataOutputStream os = new DataOutputStream( socket.getOutputStream() );
            os.writeByte( BTCmd.PONG.ordinal() );
            os.writeBoolean( deleted );
            os.flush();

            socket.close();
            // service.updateStatusOut( true );
        }

        private void receiveInvitation( byte proto, DataInputStream is,
                                        BluetoothSocket socket )
            throws IOException
        {
            BTCmd result;
            NetLaunchInfo nli;
            if ( BT_PROTO_JSONS == proto ) {
                String asJson = is.readUTF();
                nli = NetLaunchInfo.makeFrom( XWApp.getContext(), asJson );
            } else {
                short len = is.readShort();
                byte[] nliData = new byte[len];
                is.readFully( nliData );
                nli = XwJNI.nliFromStream( nliData );
            }

            Intent intent = getIntentTo( BTAction.MAKE_OR_NOTIFY )
                .putExtra( SOCKET_REF, makeRefFor( socket ) )
                .putExtra( NLI_KEY, nli )
                ;
            enqueueWork( intent );
        } // receiveInvitation

        private void receiveMessage( BTCmd cmd, DataInputStream dis,
                                     BluetoothSocket socket )
        {
            try {
                BTCmd result = null;
                int gameID = dis.readInt();
                switch ( cmd ) {
                case MESG_SEND:
                    byte[] buffer = new byte[dis.readShort()];
                    dis.readFully( buffer );

                    enqueueWork( getIntentTo( BTAction.RECEIVE_MSG )
                                 .putExtra( SOCKET_REF, makeRefFor( socket ) )
                                 .putExtra( GAMEID_KEY, gameID )
                                 .putExtra( MSG_KEY, buffer ) );
                    socket = null;
                    break;
                case MESG_GAMEGONE:
                    enqueueWork( getIntentTo( BTAction.POST_GAME_GONE )
                                 .putExtra( SOCKET_REF, makeRefFor( socket ) )
                                 .putExtra( GAMEID_KEY, gameID ) );
                    socket = null;
                    break;
                default:
                    result = BTCmd.BAD_PROTO;
                    break;
                }

                if ( null != socket ) {
                    writeBack( socket, result );
                    socket.close();
                }
            } catch ( IOException ioe ) {
                logIOE( ioe );
            }
        } // receiveMessage
    } // class BTListenerThread

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

    // sender thread
    //
    // Thing wants to outlive an instance of BTService, but not live
    // forever. Ideally it exists long enough to send the elems posted by one
    // instance then dies.

    private static Map<BTService, BTSenderThread> sMap = new HashMap<>();
    private synchronized BTSenderThread senderFor( BTService bts )
    {
        BTSenderThread result = sMap.get( bts );
        if ( null == result ) {
            result = new BTSenderThread();
            result.start();
            sMap.put( bts, result );
        }
        return result;
    }

    private synchronized void releaseSender( BTService bts )
    {
        BTSenderThread self = senderFor( bts );
        self.mFinishing = true;
        sMap.remove( bts );
    }

    private class BTSenderThread extends Thread {
        private LinkedBlockingQueue<BTQueueElem> m_queue;
        private HashMap<String,LinkedList<BTQueueElem> > m_resends;
        private volatile boolean mFinishing = false;

        private BTSenderThread()
        {
            m_queue = new LinkedBlockingQueue<BTQueueElem>();
            m_resends = new HashMap<String,LinkedList<BTQueueElem> >();
        }

        public void add( BTQueueElem elem )
        {
            Assert.assertFalse( mFinishing );
            m_queue.add( elem );
        }

        @Override
        public void run()
        {
            String className = getClass().getSimpleName();
            Log.d( TAG, "%s.run() starting", className );
            for ( ; ; ) {
                BTQueueElem elem;
                // onTheWayOut: mFinishing can change while we're in poll()
                boolean onTheWayOut = mFinishing;
                try {
                    elem = m_queue.poll( RESEND_TIMEOUT, TimeUnit.SECONDS );
                } catch ( InterruptedException ie ) {
                    Log.w( TAG, "interrupted; killing thread" );
                    break;
                }

                if ( null == elem ) { // timed out
                    if ( doAnyResends() && onTheWayOut ) {
                        // nothing to send AND nothing to resend: outta here!
                        break;
                    }
                } else {
                    // DbgUtils.logf( "run: got %s from queue", elem.m_cmd.toString() );

                    switch( elem.m_cmd ) {
                    case PING:
                        if ( null == elem.m_btAddr ) {
                            sendPings( MultiEvent.HOST_PONGED, CONNECT_TIMEOUT_MS );
                        } else {
                            sendPing( elem.m_btAddr, elem.m_gameID, CONNECT_TIMEOUT_MS );
                        }
                        break;
                    case SCAN:
                        sendPings( MultiEvent.HOST_PONGED, elem.m_timeout );
                        mHelper.postEvent( MultiEvent.SCAN_DONE );
                        break;
                    case INVITE:
                        sendInvite( elem );
                        break;
                    case MESG_SEND:
                        boolean success = doAnyResends( elem.m_btAddr )
                            && sendElem( elem );
                        if ( !success ) {
                            addToResends( elem );
                        }
                        updateStatusOut( success );
                        break;

                    case MESG_GAMEGONE:
                        sendElem( elem );
                        break;

                    default:
                        Assert.fail();
                        break;
                    }
                }
            }
            Log.d( TAG, "%s.run() exiting (owner was %s)", className,
                   BTService.this );
        } // run

        private void sendPings( MultiEvent event, int timeout )
        {
            Set<BluetoothDevice> pairedDevs = m_adapter.getBondedDevices();
            Map<BluetoothDevice, PingThread> threads = new HashMap<>();
            for ( BluetoothDevice dev : pairedDevs ) {
                // Skip things that can't host an Android app
                int clazz = dev.getBluetoothClass().getMajorDeviceClass();
                if ( Major.PHONE == clazz || Major.COMPUTER == clazz ) {
                    PingThread thread = new PingThread( dev, timeout, event );
                    threads.put( dev, thread );
                    thread.start();
                } else {
                    Log.d( TAG, "skipping %s; not an android device!",
                           dev.getName() );
                }
            }

            for ( BluetoothDevice dev : threads.keySet() ) {
                PingThread thread = threads.get( dev );
                try {
                    thread.join();
                } catch ( InterruptedException ex ) {
                    Assert.assertFalse( BuildConfig.DEBUG );
                }
            }
        }

        private class PingThread extends Thread {
            private boolean mGotResponse;
            private BluetoothDevice mDev;
            private int mTimeout;
            private MultiEvent mEvent;

            PingThread(BluetoothDevice dev, int timeout, MultiEvent event)
            {
                mDev = dev; mTimeout = timeout; mEvent = event;
            }

            @Override
            public void run() {
                mGotResponse = sendPing( mDev, 0, mTimeout );
                if ( mGotResponse && null != mEvent) {
                    mHelper.postEvent( mEvent, mDev );
                }
            }

            boolean gotResponse() { return mGotResponse; }
        }

        private boolean sendPing( BluetoothDevice dev, int gameID, int timeout )
        {
            boolean gotReply = false;
            boolean sendWorking = false;
            boolean receiveWorking = false;
            try {
                BluetoothSocket socket =
                    dev.createRfcommSocketToServiceRecord( XWApp.getAppUUID() );
                if ( null != socket ) {
                    DataOutputStream os = connect( socket, BTCmd.PING, timeout );
                    if ( null != os ) {
                        os.writeInt( gameID );
                        os.flush();
                        try ( KillerIn killer = new KillerIn( socket, 5 ) ) {
                            DataInputStream is =
                                new DataInputStream( socket.getInputStream() );
                            BTCmd reply = BTCmd.values()[is.readByte()];
                            if ( BTCmd.BAD_PROTO == reply ) {
                                sendBadProto( socket );
                            } else {
                                gotReply = BTCmd.PONG == reply;
                                if ( gotReply && is.readBoolean() ) {
                                    mHelper.postEvent( MultiEvent.MESSAGE_NOGAME, gameID );
                                }
                            }
                        }
                        receiveWorking = true;
                        sendWorking = true;
                    }
                    socket.close();
                }
            } catch ( IOException ioe ) {
                Log.e( TAG, "sendPing() failure; %s", ioe.getMessage() );
                DbgUtils.printStack( TAG, ioe );
            }
            updateStatusOut( sendWorking );
            updateStatusIn( receiveWorking );
            Log.d( TAG, "sendPing(%s) => %b", dev.getName(), gotReply );
            return gotReply;
        } // sendPing

        private boolean sendPing( String btAddr, int gameID, int timeout )
        {
            boolean success = false;
            BluetoothDevice dev = m_adapter.getRemoteDevice( btAddr );
            success = sendPing( dev, gameID, timeout );
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
                        if ( BT_PROTO == BT_PROTO_JSONS ) {
                            outStream.writeUTF( elem.m_nli.toString() );
                        } else {
                            byte[] nliData = XwJNI.nliToStream( elem.m_nli );
                            outStream.writeShort( nliData.length );
                            outStream.write( nliData, 0, nliData.length );
                        }
                        Log.i( TAG, "sending invite" );
                        outStream.flush();

                        DataInputStream inStream =
                            new DataInputStream( socket.getInputStream() );
                        reply = BTCmd.values()[inStream.readByte()];
                        Log.i( TAG, "got invite reply: %s", reply );
                    }

                    if ( null == reply ) {
                        mHelper.postEvent( MultiEvent.APP_NOT_FOUND_BT,
                                           dev.getName() );
                    } else {
                        switch ( reply ) {
                        case BAD_PROTO:
                            sendBadProto( socket );
                            break;
                        case INVITE_ACCPT:
                            mHelper.postEvent( MultiEvent.NEWGAME_SUCCESS,
                                               elem.m_gameID );
                            break;
                        case INVITE_DUPID:
                            mHelper.postEvent( MultiEvent.NEWGAME_DUP_REJECTED,
                                               dev.getName() );
                            break;
                        default:
                            mHelper.postEvent( MultiEvent.NEWGAME_FAILURE,
                                               elem.m_gameID );
                            break;
                        }
                    }

                    socket.close();
                }
            } catch ( IOException ioe ) {
                logIOE( ioe );
            }
        } // sendInvite

        private boolean sendElem( BTQueueElem elem )
        {
            boolean success = false;
            // synchronized( m_deadGames ) {
            //     success = m_deadGames.contains( elem.m_gameID );
            // }
            MultiEvent evt;
            if ( success ) {
                evt = MultiEvent.MESSAGE_DROPPED;
                Log.w( TAG, "sendElem: dropping message %s because game %X dead",
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
                            connect( socket, elem.m_cmd );
                        if ( null != outStream ) {
                            outStream.writeInt( elem.m_gameID );

                            switch ( elem.m_cmd ) {
                            case MESG_SEND:
                                short len = (short)elem.m_msg.length;
                                outStream.writeShort( len );
                                outStream.write( elem.m_msg, 0, elem.m_msg.length );
                                break;
                            case MESG_GAMEGONE:
                                // gameID's all we need
                                break;
                            default:
                                Assert.fail();
                            }

                            outStream.flush();

                            BTCmd reply;
                            try ( KillerIn killer = new KillerIn( socket, 10 ) ) {
                                DataInputStream inStream =
                                    new DataInputStream( socket.getInputStream() );
                                reply = BTCmd.values()[inStream.readByte()];
                            }
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
                mHelper.postEvent( evt, elem.m_gameID, 0, btName );
                if ( ! success ) {
                    int failCount = elem.incrFailCount();
                    mHelper.postEvent( MultiEvent.MESSAGE_RESEND, btName,
                                                RESEND_TIMEOUT, failCount );
                }
            }
            return success;
        } // sendElem

        private boolean doAnyResends( LinkedList<BTQueueElem> resends )
        {
            int count = 0;
            boolean success = null == resends || 0 < resends.size();
            if ( !success ) {
                count = resends.size();
                success = true;
                ListIterator<BTQueueElem> iter = resends.listIterator();
                while ( iter.hasNext() && success ) {
                    BTQueueElem elem = iter.next();
                    success = sendElem( elem );
                    if ( success ) {
                        iter.remove();
                    } else if ( elem.failCountExceeded() ) {
                        String btName = nameForAddr( m_adapter, elem.m_btAddr );
                        mHelper.postEvent( MultiEvent.MESSAGE_FAILOUT, btName );
                        iter.remove();
                    }
                }

            }
            if ( 0 < count ) {
                Log.d( TAG, "doAnyResends(size=%d) => %b", count, success );
            }
            return success;
        }

        private boolean doAnyResends( String btAddr )
        {
            return doAnyResends( m_resends.get( btAddr ) );
        }

        private boolean doAnyResends()
        {
            boolean success = true;
            Iterator<LinkedList<BTQueueElem>> iter =
                m_resends.values().iterator();
            while ( iter.hasNext() ) {
                LinkedList<BTQueueElem> list = iter.next();
                success = doAnyResends( list ) && success;
            }
            return success;
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

        private DataOutputStream connect( BluetoothSocket socket, BTCmd cmd )
        {
            return connect( socket, cmd, 20000 );
        }

        private DataOutputStream connect( BluetoothSocket socket, BTCmd cmd,
                                          int timeout )
        {
            String name = socket.getRemoteDevice().getName();
            String addr = socket.getRemoteDevice().getAddress();
            Log.w( TAG, "connect(%s/%s, timeout=%d) starting", name, addr, timeout );
            // DbgUtils.logf( "connecting to %s to send cmd %s", name, cmd.toString() );
            // Docs say always call cancelDiscovery before trying to connect
            m_adapter.cancelDiscovery();

            DataOutputStream dos = null;

            // Retry for some time. Some devices take a long time to generate and
            // broadcast ACL conn ACTION
            for ( long end = timeout + System.currentTimeMillis(); ; ) {
                try {
                    Log.d( TAG, "trying connect(%s/%s) ", name, addr );
                    socket.connect();
                    Log.i( TAG, "connect(%s/%s) succeeded", name, addr );
                    dos = new DataOutputStream( socket.getOutputStream() );
                    dos.writeByte( BT_PROTO );
                    dos.writeByte( cmd.ordinal() );
                    break;          // success!!!
                } catch (IOException ioe) {
                    if ( CONNECT_SLEEP_MS + System.currentTimeMillis() > end ) {
                        break;
                    }
                    try {
                        Thread.sleep( CONNECT_SLEEP_MS );
                    } catch ( InterruptedException ex ) {
                        break;
                    }
                }
            }
            return dos;
        }
    } // class BTSenderThread

    private void startListener()
    {
        Assert.assertNotNull( mHelper );
        BTListenerThread.startYourself();
    }

    private void stopListener()
    {
        Log.d( TAG, "stopListener()" );
        BTListenerThread.stopYourself( null );
    }

    private BTCmd makeOrNotify( NetLaunchInfo nli, String btName,
                                String btAddr )
    {
        BTCmd result;
        if ( mHelper.handleInvitation( this, nli, btName,
                                       DictFetchOwner.OWNER_BT ) ) {
            result = BTCmd.INVITE_ACCPT;
        } else {
            result = BTCmd.INVITE_DUP_INVITE; // dupe of rematch
        }
        return result;
    }

    private static void noteLastUsed( Context context )
    {
        Log.d( TAG, "noteLastUsed(" + context + ")" );
        // synchronized (BTService.class) {
        //     int nowSecs = (int)(SystemClock.uptimeMillis() / 1000);
        //     int newKeepSecs = nowSecs + DEFAULT_KEEPALIVE_SECONDS;
        //     DBUtils.setIntFor( context, KEY_KEEPALIVE_UNTIL_SECS, newKeepSecs );
        // }
    }

    private void setTimeoutTimer()
    {
        // // DbgUtils.assertOnUIThread();

        // long dieTimeMillis;
        // synchronized (BTService.class) {
        //     dieTimeMillis = 1000 * DBUtils.getIntFor( this, KEY_KEEPALIVE_UNTIL_SECS, 0 );
        // }
        // long nowMillis = SystemClock.uptimeMillis();

        // if ( dieTimeMillis <= nowMillis ) {
        //     Log.d( TAG, "setTimeoutTimer(): killing the thing" );
        //     stopListener();
        //     stopForeground(true);
        // } else {
        //     mHandler.removeCallbacksAndMessages( this );
        //     mHandler.postAtTime( new Runnable() {
        //             @Override
        //             public void run() {
        //                 setTimeoutTimer();
        //             }
        //         }, this, dieTimeMillis );
        //     Log.d( TAG, "setTimeoutTimer(): set for %dms from now", dieTimeMillis - nowMillis );
        // }
    }

    private static void logIOE( IOException ioe )
    {
        Log.ex( TAG, ioe );
        ++s_errCount;
    }

    private void sendBadProto( BluetoothSocket socket )
    {
        mHelper.postEvent( MultiEvent.BAD_PROTO_BT,
                           socket.getRemoteDevice().getName() );
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

    private class KillerIn implements AutoCloseable {
        private final Thread mThread;
        KillerIn( final BluetoothSocket socket, final int seconds )
        {
            mThread = new Thread( new Runnable() {
                public void run() {
                    try {
                        Thread.sleep( 1000 * seconds );
                    } catch ( InterruptedException ie ) {
                        // Log.d( TAG, "KillerIn: killed by owner" );
                    }
                    try {
                        socket.close();
                    } catch( IOException ioe ) {
                        Log.ex( TAG, ioe );
                    }
                }
                });
            mThread.start();
        }

        @Override
        public void close()
        {
            mThread.interrupt();
        }
    }

    private class BTMsgSink extends MultiMsgSink {

        public BTMsgSink() { super( BTService.this ); }

        @Override
        public int sendViaBluetooth( byte[] buf, int gameID, CommsAddrRec addr )
        {
            int nSent = -1;
            String btAddr = getSafeAddr( addr );
            if ( null != btAddr && 0 < btAddr.length() ) {
                add( new BTQueueElem( BTCmd.MESG_SEND, buf, btAddr,
                                      gameID ) );
                nSent = buf.length;
            } else {
                Log.i( TAG, "sendViaBluetooth(): no addr for dev %s",
                       addr.bt_hostName );
            }
            return nSent;
        }
    }

    private class BTServiceHelper extends XWServiceHelper {
        private Service mService;

        BTServiceHelper( Service service ) {
            super( service );
            mService = service;
        }

        @Override
        MultiMsgSink getSink( long rowid )
        {
            return m_btMsgSink;
        }

        @Override
        void postNotification( String device, int gameID, long rowid )
        {
            String body = LocUtils.getString( mService, R.string.new_bt_body_fmt,
                                              device );

            GameUtils.postInvitedNotification( mService, gameID, body, rowid );

            postEvent( MultiEvent.BT_GAME_CREATED, rowid );
        }
    }

    private static Map<Integer, BluetoothSocket> s_sockets = new HashMap<>();
    private static int makeRefFor( BluetoothSocket socket )
    {
        int code = socket.hashCode();
        synchronized ( s_sockets ) {
            Assert.assertTrue( !s_sockets.containsKey(code) || !BuildConfig.DEBUG );
            s_sockets.put( code, socket );
        }
        Log.d( TAG, "makeRefFor(%s) => %d (size: %d)", socket, code, s_sockets.size() );
        return code;
    }

    private static BluetoothSocket socketForRef( int ref  )
    {
        BluetoothSocket result = null;
        synchronized ( s_sockets ) {
            if ( s_sockets.containsKey( ref ) ) {
                result = s_sockets.get( ref );
                Assert.assertTrue( null != result || !BuildConfig.DEBUG );
            }
        }
        // Log.d( TAG, "socketForRef(%d) => %s", ref, result );
        return result;
    }

    private static void closeForRef( int ref )
    {
        synchronized ( s_sockets ) {
            BluetoothSocket socket = socketForRef( ref );
            if ( null != socket ) {
                try {
                    socket.close();
                } catch ( IOException ex ) {
                    Log.ex( TAG, ex );
                }
            }
        }
        Log.d( TAG, "closeForRef(%d) (size: %d)", ref, s_sockets.size() );
    }
}
