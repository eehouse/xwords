/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 - 2019 by Eric House (xwords@eehouse.org).  All
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
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothServerSocket;
import android.bluetooth.BluetoothSocket;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Handler;
import android.provider.Settings;
import android.text.TextUtils;

import org.eehouse.android.xw4.DbgUtils.DeadlockWatch;
import org.eehouse.android.xw4.MultiService.DictFetchOwner;
import org.eehouse.android.xw4.MultiService.MultiEvent;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

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
    static {
        XWJIService.register( BTService.class, sJobID,
                              CommsConnType.COMMS_CONN_BT );
    }

    // half minute for testing; maybe 15 on ship? Or make it a debug config.
    private static int DEFAULT_KEEPALIVE_SECONDS = 15 * 60;
    private static int CONNECT_SLEEP_MS = 2500;

    private static final long RESEND_TIMEOUT = 5; // seconds
    private static final int MAX_SEND_FAIL = 3;
    private static final int CONNECT_TIMEOUT_MS = 10000;
    private static final int MAX_PACKET_LEN = 4 * 1024;

    private static final int BT_PROTO_JSONS = 1; // using jsons instead of lots of fields
    private static final int BT_PROTO_BATCH = 2;
    // Move to BT_PROTO_BATCH after everybody's upgraded.
    private static final int BT_PROTO = BT_PROTO_JSONS; /* BT_PROTO_BATCH */

    private static boolean IS_BATCH_PROTO() { return BT_PROTO_BATCH == BT_PROTO; }

    private enum BTAction implements XWJICmds { _NONE,
                                                ACL_CONN,
                                                _START_BACKGROUND, // unused
                                                RESEND,
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
                                                POST_PING_REPLY,
    };

    private static final String MSG_KEY = "MSG";
    private static final String MSGID_KEY = "MSGID";
    private static final String GAMENAME_KEY = "NAM";
    private static final String ADDR_KEY = "ADR";
    private static final String SCAN_TIMEOUT_KEY = "SCAN_TIMEOUT";
    private static final String RADIO_KEY = "RDO";
    private static final String DEL_KEY = "DEL";
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
        INVITE_DECL,            // unused
        INVITE_DUPID,
        INVITE_FAILED,      // generic error
        MESG_SEND,
        MESG_ACCPT,
        MESG_DECL,              // unused
        MESG_GAMEGONE,
        REMOVE_FOR,             // unused
        INVITE_DUP_INVITE,
    };

    private BluetoothAdapter m_adapter;
    private BTMsgSink m_btMsgSink;
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
        if ( BTEnabled() ) {
            enqueueWork( context, BTService.class, intent );
            // Log.d( TAG, "enqueueWork(%s)", cmdFrom( intent, BTAction.values() ) );
        } else {
            Log.d( TAG, "enqueueWork(): BT disabled so doing nothing" );
        }
    }

    public static void onACLConnected( Context context )
    {
        Log.d( TAG, "onACLConnected(); enqueuing work" );
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

    public static int sendPacket( Context context, byte[] buf, String msgID,
                                  CommsAddrRec targetAddr, int gameID )
    {
        int nSent = -1;
        Assert.assertNotNull( targetAddr );
        String btAddr = getSafeAddr( targetAddr );
        if ( null != btAddr && 0 < btAddr.length() ) {
            Intent intent = getIntentTo( context, BTAction.SEND )
                .putExtra( MSG_KEY, buf )
                .putExtra( MSGID_KEY, msgID )
                .putExtra( ADDR_KEY, btAddr )
                .putExtra( GAMEID_KEY, gameID );
            enqueueWork( context, intent );
            nSent = buf.length;
        }

        if ( -1 == nSent ) {
            Log.i( TAG, "sendPacket(gameID=%d(0x%x)): can't send to %s",
                   gameID, gameID, targetAddr.bt_hostName );
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
    }

    @Override
    XWJICmds[] getCmds() { return BTAction.values(); }

    @Override
    void onHandleWorkImpl( Intent intent, XWJICmds jicmd, long timestamp )
    {
        if ( BTEnabled() ) {
            BTAction cmd = (BTAction)jicmd;
            switch( cmd ) {
            case ACL_CONN:          // just forces onCreate to run
                startListener();
                break;
            case SCAN:
                int timeout = intent.getIntExtra( SCAN_TIMEOUT_KEY, -1 );
                startScanThread( timeout );
                break;
            case INVITE:
                String btAddr = intent.getStringExtra( ADDR_KEY );
                String jsonData = intent.getStringExtra( GAMEDATA_KEY );
                NetLaunchInfo nli = NetLaunchInfo.makeFrom( this, jsonData );
                // Log.i( TAG, "onHandleWorkImpl(): nli: %s", nli );
                getPA( btAddr ).addInvite( nli );
                break;
            case PINGHOST:
                btAddr = intent.getStringExtra( ADDR_KEY );
                int gameID = intent.getIntExtra( GAMEID_KEY, 0 );
                getPA( btAddr ).addPing( gameID );
                break;

            case SEND:
                byte[] buf = intent.getByteArrayExtra( MSG_KEY );
                btAddr = intent.getStringExtra( ADDR_KEY );
                String msgID = intent.getStringExtra( MSGID_KEY );
                gameID = intent.getIntExtra( GAMEID_KEY, -1 );
                if ( -1 != gameID ) {
                    getPA( btAddr ).addMsg( gameID, buf, msgID );
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
                getPA( btAddr ).addDied( gameID );
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
                        = mHelper.receiveMessage( gameID, m_btMsgSink, buf, addr );

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

            case POST_PING_REPLY:
                socketRef = intent.getIntExtra( SOCKET_REF, -1 );
                socket = socketForRef( socketRef  );
                if ( null != socket ) {
                    try {
                        boolean deleted = intent.getBooleanExtra( DEL_KEY, false );
                        DataOutputStream os = new DataOutputStream( socket.getOutputStream() );
                        os.writeByte( BTCmd.PONG.ordinal() );
                        os.writeBoolean( deleted );
                        os.flush();
                    } catch ( IOException ex ) {
                        Log.ex( TAG, ex );
                    }
                    closeForRef( socketRef );
                }
                break;

            default:
                Assert.failDbg();
            }
        } else {
            Log.d( TAG, "onHandleWorkImpl(): BT disabled so doing nothing" );
        }
    } // onHandleWorkImpl()

    private void startScanThread( final int timeoutMS )
    {
        new Thread( new Runnable() {
                @Override
                public void run() {
                    Log.d( TAG, "scan thread starting (timeout=%dms)", timeoutMS );
                    sendPings( MultiEvent.HOST_PONGED, timeoutMS );
                    mHelper.postEvent( MultiEvent.SCAN_DONE );
                    Log.d( TAG, "scan thread done" );
                }
            } ).start();
    }

    private static class BTListenerThread extends Thread {
        // Wrap so we can synchronize on the container
        private static BTListenerThread[] s_listener = {null};
        private BluetoothServerSocket m_serverSocket;
        private volatile Thread mTimerThread;

        private BTListenerThread() {}

        static void startYourself()
        {
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
            } catch ( SecurityException ex ) {
                // Got this with a message saying not allowed to call
                // listenUsingRfcommWithServiceRecord() in background (on
                // Android 9)
                m_serverSocket = null;
                // I'm seeing too much of this on two-user systems.
                // Log.ex( TAG, ex );
            }

            int nBadCount = 0;
            while ( null != m_serverSocket && adapter.isEnabled() ) {
                BluetoothSocket socket = null;
                try {
                    Log.d( TAG, "%s.run() calling accept()", this );
                    socket = m_serverSocket.accept(); // blocks
                    Assert.assertTrue( socket.isConnected() || !BuildConfig.DEBUG );
                    DataInputStream inStream =
                        new DataInputStream( socket.getInputStream() );

                    byte proto = inStream.readByte();
                    if ( proto == BT_PROTO_BATCH || proto == BT_PROTO_JSONS ) {
                        resetSenderFor( socket );         // still looks good here?
                        BTInviteDelegate.onHeardFromDev( XWApp.getContext(),
                                                         socket.getRemoteDevice() );

                        new PacketParser( proto )
                            .dispatchAll( inStream, socket, BTListenerThread.this );
                        // hack: will close if nobody ref'd it inside dispatchAll()
                        closeForRef( makeRefFor( socket ) );
                        socket = null;
                        updateStatusIn( true );
                    } else {
                        writeBack( socket, BTCmd.BAD_PROTO );
                        socket.close();
                    }
                    nBadCount = 0;
                } catch ( IOException ioe ) {
                    ++nBadCount;
                    Log.w( TAG, "BTListenerThread.run(): trying again (%dth time)", nBadCount );
                    // logIOE( ioe);

                } catch ( NullPointerException npe ) {
                    // continue;   // m_serverSocket probably null
                } finally {
                    if ( null != socket ) {
                        try {
                            socket.close();
                        } catch ( Exception ex ) {
                            Log.ex( TAG, ex );
                        }
                    }
                }
            }

            closeServerSocket();

            stopYourself( this ); // need to clear the ref so can restart
            onACLConnected( XWApp.getContext() ); // make sure we'll start again

            Log.d( TAG, "BTListenerThread.run() exiting" );
        } // run()

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

        void receivePing( int gameID, BluetoothSocket socket )
            throws IOException
        {
            boolean deleted = 0 != gameID && !DBUtils
                .haveGame( XWApp.getContext(), gameID );

            enqueueWork( getIntentTo( BTAction.POST_PING_REPLY )
                         .putExtra( SOCKET_REF, makeRefFor( socket ) )
                         .putExtra( DEL_KEY, deleted ) );
        }

        void receiveInvitation( NetLaunchInfo nli, BluetoothSocket socket )
            throws IOException
        {
            Intent intent = getIntentTo( BTAction.MAKE_OR_NOTIFY )
                .putExtra( SOCKET_REF, makeRefFor( socket ) )
                .putExtra( NLI_KEY, nli )
                ;
            enqueueWork( intent );
        } // receiveInvitation

        void receiveMessage( int gameID, byte[] buffer, BluetoothSocket socket )
        {
            enqueueWork( getIntentTo( BTAction.RECEIVE_MSG )
                         .putExtra( SOCKET_REF, makeRefFor( socket ) )
                         .putExtra( GAMEID_KEY, gameID )
                         .putExtra( MSG_KEY, buffer ) );
        } // receiveMessage

        void receiveGameGone( int gameID, BluetoothSocket socket )
        {
            enqueueWork( getIntentTo( BTAction.POST_GAME_GONE )
                         .putExtra( SOCKET_REF, makeRefFor( socket ) )
                         .putExtra( GAMEID_KEY, gameID ) );
        }
    } // class BTListenerThread

    private static Map<String, String> s_namesToAddrs;
    private static String getSafeAddr( CommsAddrRec addr )
    {
        String btAddr = addr.bt_btAddr;
        if ( BOGUS_MARSHMALLOW_ADDR.equals( btAddr ) ) {
            String btName = addr.bt_hostName;
            if ( null == s_namesToAddrs ) {
                s_namesToAddrs = new HashMap<>();
            }

            if ( s_namesToAddrs.containsKey( btName ) ) {
                btAddr = s_namesToAddrs.get( btName );
            } else {
                btAddr = null;
            }
            if ( null == btAddr ) {
                BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
                if ( null != adapter ) {
                    for ( BluetoothDevice dev : adapter.getBondedDevices() ) {
                        // Log.d( TAG, "%s => %s", dev.getName(), dev.getAddress() );
                        if ( btName.equals( dev.getName() ) ) {
                            btAddr = dev.getAddress();
                            s_namesToAddrs.put( btName, btAddr );
                            break;
                        }
                    }
                }
            }
        }
        return btAddr;
    }

    private void sendPings( MultiEvent event, int timeoutMS )
    {
        Set<BluetoothDevice> pairedDevs = m_adapter.getBondedDevices();
        Map<BluetoothDevice, PacketAccumulator> pas = new HashMap<>();
        for ( BluetoothDevice dev : pairedDevs ) {
            // Skip things that can't host an Android app
            int clazz = dev.getBluetoothClass().getMajorDeviceClass();
            if ( Major.PHONE == clazz || Major.COMPUTER == clazz ) {
                PacketAccumulator pa =
                    new PacketAccumulator( dev.getAddress(), timeoutMS )
                    .addPing( 0 )
                    .setExitWhenEmpty()
                    .setLifetimeMS(timeoutMS)
                    .setService( this )
                    ;
                pas.put( dev, pa );
            } else {
                Log.d( TAG, "skipping %s (clazz=%d); not an android device!",
                       dev.getName(), clazz );
            }
        }

        for ( BluetoothDevice dev : pas.keySet() ) {
            PacketAccumulator pa = pas.get( dev );
            try {
                pa.join();
                if ( 0 < pa.getResponseCount() ) {
                    mHelper.postEvent( event, dev );
                }
            } catch ( InterruptedException ex ) {
                Assert.failDbg();
            }
        }
    }

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
        if ( mHelper.handleInvitation( nli, btName, DictFetchOwner.OWNER_BT ) ) {
            result = BTCmd.INVITE_ACCPT;
        } else {
            result = BTCmd.INVITE_DUP_INVITE; // dupe of rematch
        }
        return result;
    }

    private static void logIOE( IOException ioe )
    {
        Log.ex( TAG, ioe );
        ++s_errCount;
    }

    private static void updateStatusOut( boolean success )
    {
        Context context = XWApp.getContext();
        ConnStatusHandler
            .updateStatusOut( context, CommsConnType.COMMS_CONN_BT, success );
    }

    private static void updateStatusIn( boolean success )
    {
        Context context = XWApp.getContext();
        ConnStatusHandler
            .updateStatusIn( context, CommsConnType.COMMS_CONN_BT, success );
    }

    private static class KillerIn extends Thread implements AutoCloseable {
        private int mSeconds;
        private java.io.Closeable mSocket;

        KillerIn( final java.io.Closeable socket, final int seconds )
        {
            mSeconds = seconds;
            mSocket = socket;
            start();
        }

        @Override
        public void run()
        {
            try {
                Thread.sleep( 1000 * mSeconds );
                Log.d( TAG, "KillerIn(): time's up; closing socket" );
                mSocket.close();
            } catch ( InterruptedException ie ) {
                // Log.d( TAG, "KillerIn: killed by owner" );
            } catch( IOException ioe ) {
                Log.ex( TAG, ioe );
            }
        }

        @Override
        public void close() { interrupt(); }
    }

    private class BTMsgSink extends MultiMsgSink {

        public BTMsgSink() { super( BTService.this ); }

        @Override
        public int sendViaBluetooth( byte[] buf, String msgID, int gameID,
                                     CommsAddrRec addr )
        {
            int nSent = -1;
            String btAddr = getSafeAddr( addr );
            if ( null != btAddr && 0 < btAddr.length() ) {
                getPA( btAddr ).addMsg( gameID, buf, msgID );
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

    private static class PacketAccumulator extends Thread {

        private static class MsgElem {
            BTCmd mCmd;
            String mMsgID;
            int mGameID;
            long mStamp;
            byte[] mData;
            int mLocalID;

            MsgElem( BTCmd cmd, int gameID, String msgID, OutputPair op )
            {
                mCmd = cmd;
                mMsgID = msgID;
                mGameID = gameID;
                mStamp = System.currentTimeMillis();

                OutputPair tmpOp = new OutputPair();
                try {
                    tmpOp.dos.writeByte( cmd.ordinal() );
                    byte[] data = op.bos.toByteArray();
                    if ( IS_BATCH_PROTO() ) {
                        tmpOp.dos.writeShort( data.length );
                    }
                    tmpOp.dos.write( data, 0, data.length );
                    mData = tmpOp.bos.toByteArray();
                } catch (IOException ioe ) {
                    // With memory-backed IO this should be impossible
                    Log.e( TAG, "MsgElem.__init(): got ioe!: %s",
                           ioe.getMessage() );
                }
            }

            void setLocalID( int id ) { mLocalID = id; }

            boolean isSameAs( MsgElem other )
            {
                boolean result = mCmd == other.mCmd
                    && mGameID == other.mGameID
                    && Arrays.equals( mData, other.mData );
                if ( result ) {
                    if ( null != mMsgID && !mMsgID.equals( other.mMsgID ) ) {
                        Log.d( TAG, "hmmm: identical but msgIDs differ: new %s vs old %s",
                               mMsgID, other.mMsgID );
                        // new 0:0 vs old 2:0 is ok!! since 0: is replaced by
                        // 2 or more when device becomes a client
                        // Assert.assertFalse( BuildConfig.DEBUG ); // fired!!!
                    }
                }
                return result;
            }

            int size() { return mData.length; }
            @Override
            public String toString()
            {
                return String.format("{cmd: %s, msgID: %s}", mCmd, mMsgID );
            }
        }

        private String mAddr;
        private String mName;
        private List<MsgElem> mElems;
        private long mLastFailTime;
        private int mFailCount;
        private int mLength;
        private int mCounter;
        private BTService mService;
        private boolean mShouldExit = false;
        private long mDieTimeMS = Long.MAX_VALUE;
        private int mResponseCount;
        private int mTimeoutMS;
        private volatile boolean mExitWhenEmpty = false;

        PacketAccumulator( String addr ) { this(addr, 20000); }

        PacketAccumulator( String addr, int timeoutMS )
        {
            mAddr = addr;
            mName = getName( addr );
            mElems = new ArrayList<>();
            mFailCount = 0;
            mLength = 0;
            mTimeoutMS = timeoutMS;
            start();
        }

        synchronized PacketAccumulator setService( BTService service )
        {
            Assert.assertNotNull( service );
            mService = service;

            notifyAll();

            return this;
        }

        PacketAccumulator setExitWhenEmpty()
        {
            mExitWhenEmpty = true;
            return this;
        }

        PacketAccumulator setLifetimeMS( long msToLive )
        {
            mDieTimeMS = System.currentTimeMillis() + msToLive;
            return this;
        }

        int getResponseCount()
        {
            return mResponseCount;
        }

        @Override
        public void run()
        {
            Log.d( TAG, "run starting for %s", this );
            // Run as long as I have something to send. Sleep for as long as
            // appropriate based on backoff logic, and be awakened when
            // something new comes in or there's reason to hope a send try
            // will succeed.
            while ( BTEnabled() && ! mShouldExit ) {
                synchronized ( this ) {
                    if ( mExitWhenEmpty && 0 == mElems.size() ) {
                        break;
                    } else if ( System.currentTimeMillis() >= mDieTimeMS ) {
                        break;
                    }

                    long waitTimeMS = null == mService
                        ? Long.MAX_VALUE : figureWait();
                    if ( waitTimeMS > 0 ) {
                        Log.d( TAG, "%s: waiting %dms", this, waitTimeMS );
                        try {
                            wait( waitTimeMS );
                            Log.d( TAG, "%s: done waiting", this );
                            continue; // restart in case state's changed
                        } catch ( InterruptedException ie ) {
                            Log.d( TAG, "ie inside wait: %s", ie.getMessage() );
                        }
                    }
                }
                mResponseCount += trySend();
            }
            Log.d( TAG, "run finishing for %s after sending %d packets",
                   this, mResponseCount );

            // A hack: mExitWhenEmpty only set in the ping case
            if ( !mExitWhenEmpty ) {
                removeSenderFor( this );
            }
        }

        String getBTAddr() { return mAddr; }
        String getBTName() { return mName; }

        private int trySend()
        {
            int nDone = 0;
            BluetoothSocket socket = null;
            try {
                socket = mService.m_adapter.getRemoteDevice( getBTAddr() )
                    .createRfcommSocketToServiceRecord( XWApp.getAppUUID() );
                DataOutputStream dos = connect( socket, mTimeoutMS );
                if ( null == dos ) {
                    setNoHost();
                    updateStatusOut( false );
                } else {
                    Log.d( TAG, "PacketAccumulator.run(): connect(%s) => %s",
                           getBTName(), dos );
                    nDone += writeAndCheck( socket, dos, mService.mHelper );
                    updateStatusOut( true );
                }
            } catch ( IOException ioe ) {
                Log.e( TAG, "PacketAccumulator.run(): ioe: %s",
                       ioe.getMessage() );
            } finally {
                if ( null != socket ) {
                    try { socket.close(); }
                    catch (Exception ex) {}
                }
            }
            return nDone;
        }

        private long figureWait()
        {
            long waitFromNow;
            try ( DeadlockWatch dw = new DeadlockWatch( this ) ) {
                synchronized ( this ) {
                    if ( 0 == mElems.size() ) { // nothing to send
                        waitFromNow = Long.MAX_VALUE;
                    } else if ( 0 == mFailCount ) {
                        waitFromNow = 0;
                    } else {
                        // If we're failing, use a backoff.
                        long wait = 1000 * (long)Math.pow( mFailCount, 2 );
                        waitFromNow = wait - (System.currentTimeMillis() - mLastFailTime);
                    }
                }
            }
            Log.d( TAG, "%s.figureWait() => %dms", this, waitFromNow );
            return waitFromNow;
        }

        void setNoHost()
        {
            try ( DeadlockWatch dw = new DeadlockWatch( this ) ) {
                synchronized ( this ) {
                    mLastFailTime = System.currentTimeMillis();
                    ++mFailCount;
                }
            }
        }

        @Override
        public synchronized String toString()
        {
            StringBuilder sb = new StringBuilder("{")
                .append("name: ").append( mName )
                .append( ", addr: ").append( mAddr )
                .append( ", failCount: " ).append( mFailCount )
                .append( ", len: " ).append( mLength )
                ;

            if ( 0 < mElems.size() ) {
                long age = System.currentTimeMillis() - mElems.get(0).mStamp;
                int lowID = mElems.get(0).mLocalID;
                int highID = mElems.get(mElems.size() - 1).mLocalID;
                List<BTCmd> cmds = new ArrayList<>();
                for ( MsgElem elem : mElems ) {
                    cmds.add( elem.mCmd );
                }
                sb.append( ", age: " ).append( age )
                    .append( ", ids: ").append(lowID).append('-').append(highID)
                    .append( ", cmds: " ).append( TextUtils.join(",", cmds) )
                    ;
            }

            return sb.append('}').toString();
        }

        private int writeAndCheck( BluetoothSocket socket, DataOutputStream dos,
                           BTServiceHelper helper )
            throws IOException
        {
            Log.d( TAG, "%s.writeAndCheck() IN", this );
            Assert.assertNotNull( dos );

            List<MsgElem> localElems = new ArrayList<>();
            try ( DeadlockWatch dw = new DeadlockWatch( this ) ) {
                synchronized ( this ) {
                    if ( 0 < mLength ) {
                        try {
                            // Format is <proto><len-of-rest><msgCount><msg1>..<msgN> To
                            // insert len-of-rest at the beginning we have to create a
                            // tmp byte array then append it after writing its length.

                            OutputPair tmpOP = new OutputPair();
                            int msgCount = IS_BATCH_PROTO() ? mElems.size() : 1;
                            if ( IS_BATCH_PROTO() ) {
                                tmpOP.dos.writeByte( msgCount );
                            }

                            for ( int ii = 0; ii < msgCount; ++ii ) {
                                MsgElem elem = mElems.get(ii);
                                byte[] elemData = elem.mData;
                                tmpOP.dos.write( elemData, 0, elemData.length );
                                localElems.add( elem );
                            }
                            byte[] data = tmpOP.bos.toByteArray();

                            // now write to the socket. Note that connect()
                            // writes BT_PROTO as the first byte.
                            if ( IS_BATCH_PROTO() ) {
                                dos.writeShort( data.length );
                            }
                            dos.write( data, 0, data.length );
                            dos.flush();
                            Log.d( TAG, "writeAndCheck(): wrote %d msgs as"
                                   + " %d-byte payload with sum %s (for %s)",
                                   msgCount, data.length, Utils.getMD5SumFor( data ),
                                   this );
                        } catch ( IOException ioe ) {
                            Log.e( TAG, "writeAndCheck(): ioe: %s", ioe.getMessage() );
                            localElems = null;
                        }
                    }
                } // synchronized
            }

            int nDone = 0;
            if ( null != localElems ) {
                Log.d( TAG, "writeAndCheck(): reading %d replies", localElems.size() );
                try ( KillerIn ki = new KillerIn( socket, 30 ) ) {
                    DataInputStream inStream =
                        new DataInputStream( socket.getInputStream() );
                    for ( int ii = 0; ii < localElems.size(); ++ii ) {
                        MsgElem elem = localElems.get(ii);
                        BTCmd cmd = elem.mCmd;
                        int gameID = elem.mGameID;
                        byte cmdOrd = inStream.readByte();
                        if ( cmdOrd >= BTCmd.values().length ) {
                            break; // SNAFU!!!
                        }
                        BTCmd reply = BTCmd.values()[cmdOrd];
                        Log.d( TAG, "writeAndCheck() %s: got response %s to cmd[%d] %s",
                               this, reply, ii, cmd );

                        if ( reply == BTCmd.BAD_PROTO ) {
                            helper.postEvent( MultiEvent.BAD_PROTO_BT,
                                              socket.getRemoteDevice().getName() );
                        } else {
                            handleReply( helper, inStream, cmd, gameID, reply );
                        }
                        ++nDone;
                    }
                } catch ( IOException ioe ) {
                    Log.d( TAG, "failed reading replies for %s: %s", this, ioe.getMessage() );
                }
            }
            unappend( nDone );
            Log.d( TAG, "writeAndCheck() => %d", nDone );
            if ( nDone > 0 ) {
                updateStatusOut( true );
            }
            return nDone;
        }

        private void handleReply( BTServiceHelper helper, DataInputStream inStream,
                                  BTCmd cmd, int gameID, BTCmd reply ) throws IOException
        {
            MultiEvent evt = null;
            switch ( cmd ) {
            case MESG_SEND:
            case MESG_GAMEGONE:
                switch ( reply ) {
                case MESG_ACCPT:
                    evt = MultiEvent.MESSAGE_ACCEPTED;
                    break;
                case MESG_GAMEGONE:
                    evt = MultiEvent.MESSAGE_NOGAME;
                    break;
                }
                break;
                    
            case INVITE:
                switch ( reply ) {
                case INVITE_ACCPT:
                    helper.postEvent( MultiEvent.NEWGAME_SUCCESS, gameID );
                    break;
                case INVITE_DUPID:
                    helper.postEvent( MultiEvent.NEWGAME_DUP_REJECTED, mName );
                    break;
                default:
                    helper.postEvent( MultiEvent.NEWGAME_FAILURE, gameID );
                    break;
                }
                break;
            case PING:
                if ( BTCmd.PONG == reply && inStream.readBoolean() ) {
                    helper.postEvent( MultiEvent.MESSAGE_NOGAME, gameID );
                }
                break;

            default:
                Log.e( TAG, "handleReply(cmd=%s) case not handled", cmd );
                Assert.failDbg(); // fired
            }

            if ( null != evt ) {
                helper.postEvent( evt, gameID, 0, mName );
            }
        }

        private DataOutputStream connect( BluetoothSocket socket, int timeout )
        {
            String name = socket.getRemoteDevice().getName();
            String addr = socket.getRemoteDevice().getAddress();
            Log.w( TAG, "connect(%s/%s, timeout=%d) starting", name, addr, timeout );
            // DbgUtils.logf( "connecting to %s to send cmd %s", name, cmd.toString() );
            // Docs say always call cancelDiscovery before trying to connect
            mService.m_adapter.cancelDiscovery();

            DataOutputStream dos = null;

            // Retry for some time. Some devices take a long time to generate and
            // broadcast ACL conn ACTION
            int nTries = 0;
            for ( long end = timeout + System.currentTimeMillis(); ; ) {
                try {
                    // Log.d( TAG, "trying connect(%s/%s) (check accept() logs)", name, addr );
                    ++nTries;
                    socket.connect();
                    Log.i( TAG, "connect(%s/%s) succeeded after %d tries",
                           name, addr, nTries );
                    dos = new DataOutputStream( socket.getOutputStream() );
                    dos.writeByte( BT_PROTO );
                    break;          // success!!!
                } catch (IOException ioe) {
                    // Log.d( TAG, "connect(): %s", ioe.getMessage() );
                    long msLeft = end - System.currentTimeMillis();
                    if ( msLeft <= 0 ) {
                        break;
                    }
                    try {
                        Thread.sleep( Math.min( CONNECT_SLEEP_MS, msLeft ) );
                    } catch ( InterruptedException ex ) {
                        break;
                    }
                }
            }
            Log.w( TAG, "connect(%s/%s) => %s", name, addr, dos );
            return dos;
        }

        PacketAccumulator addPing( int gameID )
        {
            try {
                OutputPair op = new OutputPair();
                op.dos.writeInt( gameID );
                append( BTCmd.PING, gameID, op );
            } catch ( IOException ioe ) {
                Assert.failDbg();
            }
            return this;
        }

        void addInvite( NetLaunchInfo nli )
        {
            try {
                OutputPair op = new OutputPair();
                if ( IS_BATCH_PROTO() ) {
                    byte[] nliData = XwJNI.nliToStream( nli );
                    op.dos.writeShort( nliData.length );
                    op.dos.write( nliData, 0, nliData.length );
                } else {
                    op.dos.writeUTF( nli.toString() );
                }
                append( BTCmd.INVITE, op );
            } catch ( IOException ioe ) {
                Assert.failDbg();
            }
        }

        void addMsg( int gameID, byte[] buf, String msgID )
        {
            try {
                OutputPair op = new OutputPair();
                op.dos.writeInt( gameID );
                op.dos.writeShort( buf.length );
                op.dos.write( buf, 0, buf.length );
                append( BTCmd.MESG_SEND, gameID, msgID, op );
            } catch ( IOException ioe ) {
                Assert.failDbg();
            }
        }
        
        void addDied( int gameID )
        {
            try {
                OutputPair op = new OutputPair();
                op.dos.writeInt( gameID );
                append( BTCmd.MESG_GAMEGONE, gameID, op );
            } catch ( IOException ioe ) {
                Assert.failDbg();
            }
        }

        private void append( BTCmd cmd, OutputPair op ) throws IOException
        {
            append( cmd, 0, null, op );
        }

        private void append( BTCmd cmd, int gameID, OutputPair op ) throws IOException
        {
            append( cmd, gameID, null, op );
        }
        
        private boolean append( BTCmd cmd, int gameID, String msgID,
                                OutputPair op ) throws IOException
        {
            boolean haveSpace;
            try ( DeadlockWatch dw = new DeadlockWatch( this ) ) {
                synchronized ( this ) {
                    MsgElem newElem = new MsgElem( cmd, gameID, msgID, op );
                    haveSpace = mLength + newElem.size() < MAX_PACKET_LEN;
                    if ( haveSpace ) {
                        // Let's check for duplicates....
                        boolean dupFound = false;
                        for ( MsgElem elem : mElems ) {
                            if ( elem.isSameAs( newElem ) ) {
                                dupFound = true;
                                break;
                            }
                        }

                        if ( dupFound ) {
                            Log.d( TAG, "append(): dropping dupe: %s", newElem );
                        } else {
                            newElem.setLocalID( mCounter++ );
                            mElems.add( newElem );
                            mLength += newElem.size();
                        }
                        // for now, we restart timer on new data, even if a dupe
                        mFailCount = 0;
                        notifyAll();
                    }
                }
            }
            // Log.d( TAG, "append(%s): now %s", cmd, this );
            return haveSpace;
        }

        private void unappend( int nToRemove )
        {
            Assert.assertTrue( nToRemove <= mElems.size() );
            try ( DeadlockWatch dw = new DeadlockWatch( this ) ) {
                synchronized ( this ) {
                    for ( int ii = 0; ii < nToRemove; ++ii ) {
                        MsgElem elem = mElems.remove(0);
                        mLength -= elem.size();
                    }
                    Log.d( TAG, "unappend(): after removing %d, have %d left for size %d",
                           nToRemove, mElems.size(), mLength );

                    resetBackoff(); // we were successful sending, so should retry immediately
                }
            }
        }

        void resetBackoff()
        {
            // Log.d( TAG, "resetBackoff() IN" );
            try ( DeadlockWatch dw = new DeadlockWatch( this ) ) {
                synchronized ( this ) {
                    mFailCount = 0;
                }
            }
            // Log.d( TAG, "resetBackoff() OUT" );
        }

        private static class OutputPair {
            ByteArrayOutputStream bos;
            DataOutputStream dos;
            OutputPair() {
                bos = new ByteArrayOutputStream();
                dos = new DataOutputStream( bos );
            }

            int length() { return bos.toByteArray().length; }
        }
                             
        private String getName( String addr )
        {
            Assert.assertFalse( BOGUS_MARSHMALLOW_ADDR.equals( addr ) );
            String result = "<unknown>";
            BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
            if ( null != adapter ) {
                Set<BluetoothDevice> devs = adapter.getBondedDevices();
                Iterator<BluetoothDevice> iter = devs.iterator();
                while ( iter.hasNext() ) {
                    BluetoothDevice dev = iter.next();
                    String devAddr = dev.getAddress();
                    Assert.assertFalse( BOGUS_MARSHMALLOW_ADDR.equals( devAddr ) );
                    if ( devAddr.equals( addr ) ) {
                        result = dev.getName();
                        break;
                    }
                }
            }
            return result;
        }
    }

    private static class PacketParser {
        private int mProto;
        PacketParser( int proto ) { mProto = proto; }

        void dispatchAll( DataInputStream inStream, BluetoothSocket socket,
                          BTListenerThread processor )
        {
            try {
                boolean isOldProto = mProto == BT_PROTO_JSONS;
                short isLen = isOldProto
                    ? (short)inStream.available() : inStream.readShort();
                if ( isLen >= MAX_PACKET_LEN ) {
                    Log.e( TAG, "packet too big; dropping!!!" );
                    Assert.failDbg();
                } else {
                    byte[] data = new byte[isLen];
                    inStream.readFully( data );

                    ByteArrayInputStream bis = new ByteArrayInputStream( data );
                    DataInputStream dis = new DataInputStream( bis );
                    int nMessages = isOldProto ? 1 : dis.readByte();

                    Log.d( TAG, "dispatchAll(): read %d-byte payload with sum %s containing %d messages",
                           data.length, Utils.getMD5SumFor( data ), nMessages );

                    for ( int ii = 0; ii < nMessages; ++ii ) {
                        byte cmdOrd = dis.readByte();
                        short oneLen = isOldProto ? 0 : dis.readShort(); // used only to skip
                        int availableBefore = dis.available();
                        if ( cmdOrd < BTCmd.values().length ) {
                            BTCmd cmd = BTCmd.values()[cmdOrd];
                            Log.d( TAG, "dispatchAll(): reading msg %d: %s, len=%d",
                                   ii, cmd, oneLen );
                            switch ( cmd ) {
                            case PING:
                                int gameID = dis.readInt();
                                processor.receivePing( gameID, socket );
                                break;
                            case INVITE:
                                NetLaunchInfo nli;
                                if ( isOldProto ) {
                                    nli = NetLaunchInfo.makeFrom( XWApp.getContext(),
                                                                  dis.readUTF() );
                                } else {
                                    data = new byte[dis.readShort()];
                                    dis.readFully( data );
                                    nli = XwJNI.nliFromStream( data );
                                }
                                processor.receiveInvitation( nli, socket );
                                break;
                            case MESG_SEND:
                                gameID = dis.readInt();
                                data = new byte[dis.readShort()];
                                dis.readFully( data );
                                processor.receiveMessage( gameID, data, socket );
                                break;
                            case MESG_GAMEGONE:
                                gameID = dis.readInt();
                                processor.receiveGameGone( gameID, socket );
                                break;
                            default:
                                Assert.failDbg();
                                break;
                            }
                        } else {
                            Log.e( TAG, "unexpected command (ord: %d);"
                                   + " skipping %d bytes", cmdOrd, oneLen );
                            if ( oneLen <= dis.available() ) {
                                dis.readFully( new byte[oneLen] );
                                Assert.failDbg();
                            }
                        }

                        // sanity-check based on packet length
                        int availableAfter = dis.available();
                        Assert.assertTrue( 0 == oneLen
                                           || oneLen == availableBefore - availableAfter
                                           || !BuildConfig.DEBUG );
                    }
                }
            } catch ( IOException ioe ) {
                Log.e( TAG, "dispatchAll() got ioe: %s", ioe );
                Log.ex( TAG, ioe );
                // Assert.assertFalse( BuildConfig.DEBUG ); // fired
            } catch ( Exception ex ) {
                Log.e( TAG, "dispatchAll() got ex: %s", ex );
                Log.ex( TAG, ex );
                Assert.failDbg();
            }
            Log.d( TAG, "dispatchAll() done" );
        }
    } // class PacketParser

    private PacketAccumulator getPA( String addr )
    {
        PacketAccumulator pa = getSenderFor( addr );
        return pa.setService( this );
    }

    private static Map<String, PacketAccumulator> sSenders = new HashMap<>();
    private static PacketAccumulator getSenderFor( String addr )
    {
        return getSenderFor( addr, true );
    }

    private static PacketAccumulator getSenderFor( String addr, boolean create )
    {
        PacketAccumulator result;
        try ( DeadlockWatch dw = new DeadlockWatch( sSenders ) ) {
            synchronized ( sSenders ) {
                if ( create && !sSenders.containsKey( addr ) ) {
                    sSenders.put( addr, new PacketAccumulator( addr ) );
                }
                result = sSenders.get( addr );
            }
        }
        return result;
    }

    private static void removeSenderFor( PacketAccumulator pa )
    {
        try ( DeadlockWatch dw = new DeadlockWatch( sSenders ) ) {
            synchronized ( sSenders ) {
                if ( pa == sSenders.get( pa.getBTAddr() ) ) {
                    sSenders.remove( pa );
                } else {
                    Log.e( TAG, "race? There's a different PA for %s", pa.getBTAddr() );
                }
            }
        }
    }

    private static void resetSenderFor( BluetoothSocket socket )
    {
        // Log.d( TAG, "resetSenderFor(%s)", socket );
        String addr = socket.getRemoteDevice().getAddress();
        PacketAccumulator pa = getSenderFor( addr, false );
        if ( null != pa ) {
            pa.resetBackoff();
        } else {
            Log.d( TAG, "resetSenderFor(): not creating for addr %s", addr );
        }
        // Log.d( TAG, "resetSenderFor(): reset backoff for %s", pa );
    }

    private static class KeptSocket {
        BluetoothSocket mSocket;
        int mCount = 0;
        KeptSocket(BluetoothSocket socket) { mSocket = socket; }
    }
    private static Map<Integer, KeptSocket> s_sockets = new HashMap<>();
    private static int makeRefFor( BluetoothSocket socket )
    {
        int code = socket.hashCode();
        synchronized ( s_sockets ) {
            if ( !s_sockets.containsKey( code ) ) {
                s_sockets.put( code, new KeptSocket(socket) );
            }
            ++s_sockets.get( code ).mCount;
            // Log.d( TAG, "makeRefFor(%s) => %d (map size: %d, ref count: %d)", socket,
            //        code, s_sockets.size(), s_sockets.get( code ).mCount );
        }
        return code;
    }

    private static BluetoothSocket socketForRef( int ref  )
    {
        BluetoothSocket result = null;
        synchronized ( s_sockets ) {
            if ( s_sockets.containsKey( ref ) ) {
                result = s_sockets.get( ref ).mSocket;
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
            KeptSocket ks = s_sockets.get( ref );
            Assert.assertNotNull( ks );
            if ( null != ks ) {
                --ks.mCount;
                // Log.d( TAG, "closeForRef(%d): refCount now %d", ref, ks.mCount );
                if ( 0 == ks.mCount ) {
                    try {
                        ks.mSocket.close();
                    } catch ( IOException ex ) {
                        Log.ex( TAG, ex );
                    }
                    s_sockets.remove( ref );
                }
            }
        }
        // Log.d( TAG, "closeForRef(%d) (map size: %d)", ref, s_sockets.size() );
    }
}
