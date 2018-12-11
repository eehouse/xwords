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

public class BTService extends XWService {
    private static final String TAG = BTService.class.getSimpleName();
    private static final String BOGUS_MARSHMALLOW_ADDR = "02:00:00:00:00:00";
    private static final String KEY_KEEPALIVE_UNTIL_SECS = "keep_secs";
    // half minute for testing; maybe 15 on ship? Or make it a debug config.
    private static int DEFAULT_KEEPALIVE_SECONDS = 15 * 60;

    private static final long RESEND_TIMEOUT = 5; // seconds
    private static final int MAX_SEND_FAIL = 3;

    private static final int BT_PROTO_ORIG = 0;
    private static final int BT_PROTO_JSONS = 1; // using jsons instead of lots of fields
    private static final int BT_PROTO_NLI = 2; // using binary/common form of NLI
    private static final int BT_PROTO = BT_PROTO_JSONS; // change in a release or two

    private enum BTAction { _NONE,
                            CANCEL_SERVICE,
                            START_FOREGROUND,
                            START_BACKGROUND,
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

    private static Boolean sInForeground;

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
    private BTSenderThread m_sender;
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

    private static void onAppStateChange( Context context, boolean inForeground )
    {
        Log.d( TAG, "onAppStateChange(inForeground=%b)", inForeground );
        if ( null == sInForeground || inForeground() != inForeground ) {
            sInForeground = inForeground;

            startService( context,
                          getIntentTo( context,
                                       inForeground ? BTAction.START_FOREGROUND
                                       : BTAction.START_BACKGROUND ) );
        }
    }

    private static boolean inForeground()
    {
        boolean result = sInForeground != null && sInForeground;
        // Log.d( TAG, "inForeground() => %b", result );
        return result;
    }

    static void onAppToForeground( Context context )
    {
        onAppStateChange( context, true );
    }

    static void onAppToBackground( Context context )
    {
        onAppStateChange( context, false );
    }

    public static void startService( Context context )
    {
        if ( XWApp.BTSUPPORTED ) {
            startService( context, new Intent( context, BTService.class ) );
        }
    }

    public static void onACLConnected( Context context )
    {
        Log.d( TAG, "onACLConnected()" );
        BTListenerThread.startYourself( context );
    }

    public static void stopBackground( Context context )
    {
        startService( context,
                      getIntentTo( context, BTAction.CANCEL_SERVICE ) );
    }

    public static void radioChanged( Context context, boolean cameOn )
    {
        Intent intent = getIntentTo( context, BTAction.RADIO );
        intent.putExtra( RADIO_KEY, cameOn );
        startService( context, intent );
    }

    public static void clearDevices( Context context, String[] btAddrs )
    {
        Intent intent = getIntentTo( context, BTAction.CLEAR );
        intent.putExtra( CLEAR_KEY, btAddrs );
        startService( context, intent );
    }

    public static void scan( Context context )
    {
        startService( context, getIntentTo( context, BTAction.SCAN ) );
    }

    public static void pingHost( Context context, String hostAddr, int gameID )
    {
        Assert.assertTrue( null != hostAddr && 0 < hostAddr.length() );
        Intent intent = getIntentTo( context, BTAction.PINGHOST );
        intent.putExtra( ADDR_KEY, hostAddr );
        intent.putExtra( GAMEID_KEY, gameID );
        startService( context, intent );
    }

    public static void inviteRemote( Context context, String btAddr,
                                     NetLaunchInfo nli )
    {
        Assert.assertTrue( null != btAddr && 0 < btAddr.length() );
        Intent intent = getIntentTo( context, BTAction.INVITE );
        String nliData = nli.toString();
        intent.putExtra( GAMEDATA_KEY, nliData );
        intent.putExtra( ADDR_KEY, btAddr );

        startService( context, intent );
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

        startService( context, intent );
    }

    public static int enqueueFor( Context context, byte[] buf,
                                  CommsAddrRec targetAddr, int gameID )
    {
        int nSent = -1;
        Assert.assertNotNull( targetAddr );
        String btAddr = getSafeAddr( targetAddr );
        if ( null != btAddr && 0 < btAddr.length() ) {
            Intent intent = getIntentTo( context, BTAction.SEND );
            intent.putExtra( MSG_KEY, buf );
            intent.putExtra( ADDR_KEY, btAddr );
            intent.putExtra( GAMEID_KEY, gameID );
            startService( context, intent );
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
        Intent intent = getIntentTo( context, BTAction.REMOVE );
        intent.putExtra( GAMEID_KEY, gameID );
        intent.putExtra( ADDR_KEY, btAddr );
        startService( context, intent );
    }

    private static void startService( Context context, Intent intent )
    {
        boolean inForeground = inForeground();
        Log.d( TAG, "startService(%s); inForeground = %b", intent, inForeground );

        if ( ! inForeground && canRunForegroundService() ) {
            context.startForegroundService( intent );
        } else if ( inForeground || Build.VERSION.SDK_INT < Build.VERSION_CODES.O ) {
            context.startService( intent );
        } else {
            Log.d( TAG, "startService(); not starting" );
        }
    }

    // We can run a foreground service IIF the OS version is recent enough AND
    // user hasn't said not to do it.
    private static boolean canRunForegroundService()
    {
        // added in API level 26
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
            // && Prefs.runForegroundServiceEnabled( context, true )
            ;
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
        Log.d( TAG, "%s.onCreate()", this );
        super.onCreate();

        mHelper = new BTServiceHelper( this );

        m_btMsgSink = new BTMsgSink();
        mHandler = new Handler();
        startForegroundIf();

        BluetoothAdapter adapter = XWApp.BTSUPPORTED
            ? BluetoothAdapter.getDefaultAdapter() : null;
        if ( null != adapter && adapter.isEnabled() ) {
            m_adapter = adapter;
            Log.i( TAG, "onCreate(); bt name = %s; bt addr = %s",
                   adapter.getName(), adapter.getAddress() );
            startListener();
            startSender();
        } else {
            Log.w( TAG, "not starting threads: BT not available" );
            stopSelf();
        }
    }

    @Override
    public int onStartCommand( Intent intent, int flags, int startId )
    {
        startForegroundIf();

        int result = handleCommand( intent );

        return result;
    }

    @Override
    public void onDestroy()
    {
        Log.d( TAG, "%s.onDestroy()", this );
        super.onDestroy();
    }

    private int handleCommand( Intent intent )
    {
        int result;
        if ( XWApp.BTSUPPORTED && null != intent ) {
            int ordinal = intent.getIntExtra( CMD_KEY, -1 );
            if ( -1 == ordinal ) {
                Log.d( TAG, "handleCommand(): no ordinal!" );
                // Drop it
            } else if ( null == m_sender ) {
                Log.w( TAG, "exiting: m_queue is null" );
                stopSelf();
            } else {
                BTAction cmd = BTAction.values()[ordinal];
                Log.i( TAG, "handleCommand; cmd=%s", cmd.toString() );
                switch( cmd ) {
                case CANCEL_SERVICE:
                case START_FOREGROUND:
                    stopForeground( true ); // Kill the notification
                    break;
                case START_BACKGROUND:
                    startListener();
                    noteLastUsed( this );   // prevent timer from killing immediately
                    setTimeoutTimer();
                    break;

                case CLEAR:
                    String[] btAddrs = intent.getStringArrayExtra( CLEAR_KEY );
                    break;
                case SCAN:
                    m_sender.add( new BTQueueElem( BTCmd.SCAN ) );
                    break;
                case INVITE:
                    String jsonData = intent.getStringExtra( GAMEDATA_KEY );
                    NetLaunchInfo nli = NetLaunchInfo.makeFrom( this, jsonData );
                    Log.i( TAG, "handleCommand: nli: %s", nli.toString() );
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
                    mHelper.postEvent( evt );
                    if ( cameOn ) {
                        GameUtils.resendAllIf( this, CommsConnType.COMMS_CONN_BT );
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
                    btAddr = intent.getStringExtra( ADDR_KEY );
                    m_sender.add( new BTQueueElem( BTCmd.MESG_GAMEGONE, btAddr, gameID ) );
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
    } // handleCommand()

    private void startForegroundIf()
    {
        if ( ! inForeground() ) {
            Log.d( TAG, "startForegroundIf(): starting" );

            if ( null == m_notification ) {
                Intent notifIntent = GamesListDelegate.makeBackgroundIntent( this );
                PendingIntent pendIntent = PendingIntent
                    .getActivity( this, Utils.nextRandomInt(), notifIntent,
                                  PendingIntent.FLAG_ONE_SHOT );

                Intent stopIntent = new Intent(this, BTReceiver.class);
                stopIntent.setAction( BTReceiver.ACTION_STOP_BT );
                PendingIntent stopPI =
                    PendingIntent.getBroadcast(this, 0, stopIntent, 0);

                m_notification =
                    new NotificationCompat.Builder( this, Utils.getChannelId(this) )
                    .setSmallIcon( R.drawable.notify_btservice )
                    .setContentText( getString(R.string.bkng_notify_text) )
                    .setContentIntent( pendIntent )
                    .addAction( R.drawable.notify_btservice,
                                getString(R.string.bkng_stop_text), stopPI )
                    .build();
            }

            Log.d( TAG, "calling service.startForeground()" );
            startForeground( R.string.app_name, m_notification );
        } else {
            Log.d( TAG, "startForegroundIf(): NOT starting" );
        }
    }

    private static class BTListenerThread extends Thread {
        // Wrap so we can synchronize on the container
        private static BTListenerThread[] s_listener = {null};
        private BluetoothServerSocket m_serverSocket;
        private Context mContext;
        private BTService[] mServiceHolder = { null };
        private volatile Thread mTimerThread;

        private BTListenerThread( Context context, BTService service )
        {
            mContext = context;
            mServiceHolder[0] = service;

            // Started without a BTService instance? Run for only a little
            // while
            if ( null == service ) {
                startKillTimer();
            }
        }

        static void startYourself( Context context, BTService service )
        {
            Log.d( TAG, "startYourself(%s, %s)", context, service );
            DbgUtils.assertOnUIThread();
            synchronized ( s_listener ) {
                if ( s_listener[0] == null ) {
                    s_listener[0] = new BTListenerThread( context, service );
                    s_listener[0].start();
                } else if ( null != service ) {
                    s_listener[0].setService( context, service );
                }
            }
        }

        static void startYourself( Context context )
        {
            DbgUtils.assertOnUIThread();
            startYourself( context, null );
        }

        static void stopYourself( BTListenerThread self )
        {
            Log.d( TAG, "stopYourself()" );
            BTListenerThread listener;
            synchronized ( s_listener ) {
                listener = s_listener[0];
                s_listener[0] = null;
            }
            if ( null != listener ) {
                Assert.assertTrue( self == null || self == listener );
                listener.stopListening();
                try {
                    listener.join( 100 );
                } catch ( InterruptedException ie ) {
                    Log.ex( TAG, ie );
                }
            }
        }

        void setService( Context context, BTService service )
        {
            synchronized ( mServiceHolder ) {
                if ( null == mServiceHolder[0] ) {
                    Log.d( TAG, "setService(): setting. notifying all!!!" );
                    Assert.assertNotNull( context );
                    mContext = context; // Use Service instead of Receiver (possibly)
                    mServiceHolder[0] = service;
                    if ( service != null ) {
                        mServiceHolder.notifyAll();
                    }
                } else {
                    Assert.assertTrue( service == mServiceHolder[0] );
                }
            }
        }

        private void startTheService()
        {
            stopKillTimer();

            startService( mContext,
                          getIntentTo( mContext,
                                       BTAction.START_BACKGROUND ) );
        }

        private BTService blockForService() throws InterruptedException
        {
            synchronized( mServiceHolder ) {
                while ( mServiceHolder[0] == null ) {
                    mServiceHolder.wait();
                }
                return mServiceHolder[0];
            }
        }

        private synchronized void startKillTimer()
        {
            Assert.assertNull( mTimerThread );
            mTimerThread = new Thread( new Runnable() {
                    @Override
                    public void run() {
                        try {
                            // Give legitimate caller 10 seconds to retry with
                            // a packet that we'll recognize.
                            Thread.sleep( 10 * 1000 );
                            stopYourself( BTListenerThread.this );
                        } catch (InterruptedException ie) {
                            Log.e( TAG, "kill timer thread exiting; we're live!" );
                        }
                    }
                } );
            mTimerThread.start();
        }

        private synchronized void stopKillTimer()
        {
            if ( mTimerThread != null ) {
                mTimerThread.interrupt();
                mTimerThread = null;
            }
        }

        @Override
        public void run() {     // receive thread
            BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
            String appName = XWApp.getAppName( mContext );

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
                    BluetoothSocket socket = m_serverSocket.accept(); // blocks
                    DataInputStream inStream =
                        new DataInputStream( socket.getInputStream() );

                    byte proto = inStream.readByte();
                    BTCmd cmd = BTCmd.values()[inStream.readByte()];
                    Log.d( TAG, "BTListenerThread() got %s", cmd );
                    if ( protoOK( proto, cmd ) ) {
                        if ( null == mServiceHolder[0] ) {
                            startTheService();
                        }
                        processWhenReady( socket, proto, inStream, cmd );
                    } else {
                        DataOutputStream os =
                            new DataOutputStream( socket.getOutputStream() );
                        os.writeByte( BTCmd.BAD_PROTO.ordinal() );
                        os.flush();
                        socket.close();
                    }
                } catch ( IOException ioe ) {
                    Log.w( TAG, "trying again..." );
                    logIOE( ioe);
                    continue;
                } catch ( NullPointerException npe ) {
                    continue;   // m_serverSocket probably null
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

        private static class InPacket {
            BluetoothSocket mSocket;
            byte mProto;
            DataInputStream mInStream;
            BTCmd mCmd;

            InPacket( BluetoothSocket socket, byte proto,
                      DataInputStream inStream, BTCmd cmd )
            {
                mSocket = socket;
                mProto = proto;
                mInStream = inStream;
                mCmd = cmd;
            }
        }

        private LinkedBlockingQueue<InPacket> mInQueue = new LinkedBlockingQueue<>();

        private Thread mIgnored;
        private void processWhenReady( BluetoothSocket socket, byte proto,
                                       DataInputStream inStream, BTCmd cmd )
        {
            if ( mIgnored == null ) {
                mIgnored = new Thread( new Runnable() {
                        @Override
                        public void run() {
                            for ( ; ; ) {
                                try {
                                    InPacket elem = mInQueue.take();
                                    BTService service = blockForService();
                                    BTCmd cmd = elem.mCmd; // fired
                                    switch( cmd ) {
                                    case PING:
                                        receivePing( service, elem.mSocket );
                                        break;
                                    case INVITE:
                                        receiveInvitation( service, elem.mProto,
                                                           elem.mInStream, elem.mSocket );
                                        break;
                                    case MESG_SEND:
                                        receiveMessage( service, cmd, elem.mInStream,
                                                        elem.mSocket );
                                        break;

                                    case MESG_GAMEGONE:
                                        receiveMessage( service, cmd, elem.mInStream,
                                                        elem.mSocket );
                                        break;

                                    default:
                                        Log.e( TAG, "unexpected msg %s", cmd.toString());
                                        break;
                                    }

                                    service.updateStatusIn( true );
                                    noteLastUsed( service );
                                } catch (IOException | InterruptedException ex) {
                                    DbgUtils.printStack( TAG, ex );
                                    break;
                                }
                            }
                        }
                    } );
                mIgnored.start();
            }

            mInQueue.add( new InPacket( socket, proto, inStream, cmd ) );
        }

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

        private boolean protoOK( byte proto, BTCmd cmd )
        {
            boolean ok = proto == BT_PROTO_NLI || proto == BT_PROTO_JSONS;
            return ok;
        }

        private void receivePing( BTService service, BluetoothSocket socket ) throws IOException
        {
            DataInputStream inStream = new DataInputStream( socket.getInputStream() );
            int gameID = inStream.readInt();
            boolean deleted = 0 != gameID && !DBUtils.haveGame( mContext, gameID );

            DataOutputStream os = new DataOutputStream( socket.getOutputStream() );
            os.writeByte( BTCmd.PONG.ordinal() );
            os.writeBoolean( deleted );
            os.flush();

            socket.close();
            service.updateStatusOut( true );
        }

        private void receiveInvitation( BTService service, byte proto,
                                        DataInputStream is, BluetoothSocket socket )
            throws IOException
        {
            BTCmd result;
            NetLaunchInfo nli;
            if ( BT_PROTO_JSONS == proto ) {
                String asJson = is.readUTF();
                nli = NetLaunchInfo.makeFrom( mContext, asJson );
            } else {
                short len = is.readShort();
                byte[] nliData = new byte[len];
                is.readFully( nliData );
                nli = XwJNI.nliFromStream( nliData );
            }

            BluetoothDevice host = socket.getRemoteDevice();
            result = service
                .makeOrNotify( nli, host.getName(), host.getAddress() );

            DataOutputStream os = new DataOutputStream( socket.getOutputStream() );
            os.writeByte( result.ordinal() );
            os.flush();

            socket.close();
        } // receiveInvitation

        private void receiveMessage( BTService service, BTCmd cmd,
                                     DataInputStream dis, BluetoothSocket socket )
        {
            try {
                BTCmd result = null;
                int gameID = dis.readInt();
                switch ( cmd ) {
                case MESG_SEND:
                    byte[] buffer = new byte[dis.readShort()];
                    dis.readFully( buffer );
                    BluetoothDevice host = socket.getRemoteDevice();

                    CommsAddrRec addr = new CommsAddrRec( host.getName(),
                                                          host.getAddress() );
                    XWServiceHelper.ReceiveResult rslt
                        = service.mHelper.receiveMessage( mContext, gameID,
                                                          service.m_btMsgSink,
                                                          buffer, addr );

                    result = rslt == XWServiceHelper.ReceiveResult.GAME_GONE ?
                        BTCmd.MESG_GAMEGONE : BTCmd.MESG_ACCPT;
                    break;
                case MESG_GAMEGONE:
                    service.mHelper.postEvent( MultiEvent.MESSAGE_NOGAME, gameID );
                    result = BTCmd.MESG_ACCPT;
                    break;
                default:
                    result = BTCmd.BAD_PROTO;
                    break;
                }

                DataOutputStream os =
                    new DataOutputStream( socket.getOutputStream() );
                os.writeByte( result.ordinal() );
                os.flush();
                socket.close();
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
                    Log.w( TAG, "interrupted; killing thread" );
                    break;
                }

                if ( null == elem ) {
                    doAnyResends();
                } else {
                    // DbgUtils.logf( "run: got %s from queue", elem.m_cmd.toString() );

                    switch( elem.m_cmd ) {
                    case PING:
                        if ( null == elem.m_btAddr ) {
                            sendPings( MultiEvent.HOST_PONGED, null );
                        } else {
                            sendPing( elem.m_btAddr, elem.m_gameID );
                        }
                        break;
                    case SCAN:
                        Set<BluetoothDevice> devs = new HashSet<>();
                        sendPings( null, devs );
                        sendNames( devs );
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
        } // run

        private void sendPings( MultiEvent event, Set<BluetoothDevice> addrs )
        {
            Set<BluetoothDevice> pairedDevs = m_adapter.getBondedDevices();
            // DbgUtils.logf( "ping: got %d paired devices", pairedDevs.size() );
            for ( BluetoothDevice dev : pairedDevs ) {
                // Skip things that can't host an Android app
                int clazz = dev.getBluetoothClass().getMajorDeviceClass();
                if ( Major.PHONE == clazz || Major.COMPUTER == clazz ) {
                    if ( sendPing( dev, 0 ) ) { // did we get a reply?
                        if ( null != addrs ) {
                            addrs.add( dev );
                        }
                        if ( null != event ) {
                            mHelper.postEvent( event, dev.getName() );
                        }
                    }
                } else {
                    Log.d( TAG, "skipping %s; not an android device!",
                           dev.getName() );
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
                                mHelper.postEvent( MultiEvent.MESSAGE_NOGAME, gameID );
                            }
                        }

                        receiveWorking = true;
                        killer.interrupt();
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
                        if ( BT_PROTO == BT_PROTO_JSONS ) {
                            outStream.writeUTF( elem.m_nli.toString() );
                        } else {
                            byte[] nliData = XwJNI.nliToStream( elem.m_nli );
                            outStream.writeShort( nliData.length );
                            outStream.write( nliData, 0, nliData.length );
                        }
                        Log.i( TAG, "<eeh>sending invite" );
                        outStream.flush();

                        DataInputStream inStream =
                            new DataInputStream( socket.getInputStream() );
                        reply = BTCmd.values()[inStream.readByte()];
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
            boolean success = null == resends || 0 == resends.size();
            if ( !success ) {
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

    private void sendNames( Set<BluetoothDevice> devs )
    {
        String[] btNames = new String[devs.size()];
        String[] btAddrs = new String[devs.size()];
        int ii = 0;
        for ( BluetoothDevice dev : devs ) {
            btAddrs[ii] = dev.getAddress();
            btNames[ii] = nameForAddr( m_adapter, btAddrs[ii] );
            ++ii;
        }
        mHelper.postEvent( MultiEvent.SCAN_DONE, (Object)btAddrs, (Object)btNames );
    }

    private void startListener()
    {
        Assert.assertNotNull( mHelper );
        BTListenerThread.startYourself( this, this );
    }

    private void startSender()
    {
        m_sender = new BTSenderThread();
        m_sender.start();
    }

    private void stopListener()
    {
        BTListenerThread.stopYourself( null );
    }

    private void stopSender()
    {
        m_sender.interrupt();
        try {
            m_sender.join( 100 );
        } catch ( InterruptedException ie ) {
            Log.ex( TAG, ie );
        }
        m_sender = null;
    }

    private BTCmd makeOrNotify( NetLaunchInfo nli, String btName,
                                String btAddr )
    {
        BTCmd result;
        if ( mHelper.handleInvitation( nli, btName,
                                       DictFetchOwner.OWNER_BT ) ) {
            result = BTCmd.INVITE_ACCPT;
        } else {
            result = BTCmd.INVITE_DUP_INVITE; // dupe of rematch
        }
        return result;
    }

    private DataOutputStream connect( BluetoothSocket socket, BTCmd cmd )
    {
        String name = socket.getRemoteDevice().getName();
        Log.w( TAG, "connect(%s) starting", name );
        // DbgUtils.logf( "connecting to %s to send cmd %s", name, cmd.toString() );
        // Docs say always call cancelDiscovery before trying to connect
        m_adapter.cancelDiscovery();

        DataOutputStream dos = null;

        // Try for 8 seconds. Some devices take a long time to get ACL conn
        // ACTION
        for ( long end = 10000 + System.currentTimeMillis(); ; ) {
            try {
                socket.connect();
                Log.i( TAG, "connect(%s) succeeded", name );
                dos = new DataOutputStream( socket.getOutputStream() );
                dos.writeByte( BT_PROTO );
                dos.writeByte( cmd.ordinal() );
                break;          // success!!!
            } catch (IOException ioe) {
                if ( System.currentTimeMillis() > end ) {
                    break;
                }
                Log.d( TAG, "connect(%s) trying again", name );
                try {
                    Thread.sleep( 1000 );
                } catch ( InterruptedException ex ) {
                    break;
                }
            }
        }
        return dos;
    }

    private static void noteLastUsed( Context context )
    {
        Log.d( TAG, "noteLastUsed(" + context + ")" );
        synchronized (BTService.class) {
            int nowSecs = (int)(SystemClock.uptimeMillis() / 1000);
            int newKeepSecs = nowSecs + DEFAULT_KEEPALIVE_SECONDS;
            DBUtils.setIntFor( context, KEY_KEEPALIVE_UNTIL_SECS, newKeepSecs );
        }
    }

    private void setTimeoutTimer()
    {
        DbgUtils.assertOnUIThread();

        if ( inForeground() ) {
            // Let the timer die if we're in the foreground!
            Log.d( TAG, "setTimeoutTimer(): in foreground; not resetting timer" );
        } else {
            long dieTimeMillis;
            synchronized (BTService.class) {
                dieTimeMillis = 1000 * DBUtils.getIntFor( this, KEY_KEEPALIVE_UNTIL_SECS, 0 );
            }
            long nowMillis = SystemClock.uptimeMillis();

            if ( dieTimeMillis <= nowMillis ) {
                Log.d( TAG, "setTimeoutTimer(): killing the thing" );
                stopListener();
                stopForeground(true);
            } else {
                mHandler.removeCallbacksAndMessages( this );
                mHandler.postAtTime( new Runnable() {
                        @Override
                        public void run() {
                            setTimeoutTimer();
                        }
                    }, this, dieTimeMillis );
                Log.d( TAG, "setTimeoutTimer(): set for %dms from now", dieTimeMillis - nowMillis );
            }
        }
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
                        Log.w( TAG, "killSocketIn: killed by owner" );
                        return;
                    }
                    try {
                        socket.close();
                    } catch( IOException ioe ) {
                        Log.ex( TAG, ioe );
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
            int nSent = -1;
            if ( null == m_sender ) {
                Log.w( TAG, "sendViaBluetooth(): no send thread" );
            } else {
                String btAddr = getSafeAddr( addr );
                if ( null != btAddr && 0 < btAddr.length() ) {
                    m_sender.add( new BTQueueElem( BTCmd.MESG_SEND, buf, btAddr,
                                                   gameID ) );
                    nSent = buf.length;
                } else {
                    Log.i( TAG, "sendViaBluetooth(): no addr for dev %s",
                           addr.bt_hostName );
                }
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
}
