/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
/*
 * Copyright 2010 - 2015 by Eric House (xwords@eehouse.org).  All
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
import android.os.AsyncTask;
import android.os.Build;
import android.os.Handler;
import android.text.TextUtils;

import junit.framework.Assert;

import org.eehouse.android.xw4.GameUtils.BackMoveResult;
import org.eehouse.android.xw4.MultiService.DictFetchOwner;
import org.eehouse.android.xw4.MultiService.MultiEvent;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.UtilCtxt.DevIDType;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.HttpURLConnection;
import java.net.InetAddress;
import java.net.Socket;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.atomic.AtomicInteger;

public class RelayService extends XWService
    implements NetStateCache.StateChangedIf {
    private static final String TAG = RelayService.class.getSimpleName();
    private static final int MAX_SEND = 1024;
    private static final int MAX_BUF = MAX_SEND - 2;
    private static final int REG_WAIT_INTERVAL = 10;
    private static final int INITIAL_BACKOFF = 5;

    // One day, in seconds.  Probably should be configurable.
    private static final long MAX_KEEPALIVE_SECS = 24 * 60 * 60;

    private static final String CMD_STR = "CMD";

    private static enum MsgCmds { INVALID,
                                  PROCESS_GAME_MSGS,
                                  PROCESS_DEV_MSGS,
                                  UDP_CHANGED,
                                  SEND,
                                  SENDNOCONN,
                                  RECEIVE,
                                  TIMER_FIRED,
                                  RESET,
                                  UPGRADE,
                                  INVITE,
                                  GOT_INVITE,
                                  STOP,
    }

    private static final String MSGS_ARR = "MSGS_ARR";
    private static final String RELAY_ID = "RELAY_ID";
    private static final String DEV_ID_SRC = "DEV_ID_SRC";
    private static final String DEV_ID_DEST = "DEV_ID_DEST";
    private static final String NLI_DATA = "NLI_DATA";
    private static final String INVITE_FROM = "INVITE_FROM";
    private static final String ROWID = "ROWID";
    private static final String BINBUFFER = "BINBUFFER";

    private static Map<Integer, PacketData> s_packetsSent = new HashMap<>();
    private static AtomicInteger s_nextPacketID = new AtomicInteger();
    private static boolean s_gcmWorking = false;
    private static boolean s_registered = false;
    private static CommsAddrRec s_addr =
        new CommsAddrRec( CommsConnType.COMMS_CONN_RELAY );
    private static int s_curBackoff;
    private static long s_curNextTimer;
    static { resetBackoffTimer(); }

    private Thread m_fetchThread = null;
    private Thread m_UDPReadThread = null;
    private Thread m_UDPWriteThread = null;
    private DatagramSocket m_UDPSocket;
    private LinkedBlockingQueue<PacketData> m_queue =
        new LinkedBlockingQueue<PacketData>();
    private Handler m_handler;
    private Runnable m_onInactivity;
    private int m_maxIntervalSeconds = 0;
    private long m_lastGamePacketReceived;
    private static DevIDType s_curType = DevIDType.ID_TYPE_NONE;
    private static long s_regStartTime = 0;

    // These must match the enum XWPDevProto in xwrelay.h
    private static enum XWPDevProto { XWPDEV_PROTO_VERSION_INVALID
            ,XWPDEV_PROTO_VERSION_1
            };

    // private static final int XWPDEV_NONE = 0;

    // Must be kept in sync with eponymous enum in xwrelay.h
    private enum XWRelayReg { XWPDEV_NONE,
                              XWPDEV_UNAVAIL,
                              XWPDEV_REG,
                              XWPDEV_REGRSP,
                              XWPDEV_KEEPALIVE,
                              XWPDEV_HAVEMSGS,
                              XWPDEV_RQSTMSGS,
                              XWPDEV_MSG,
                              XWPDEV_MSGNOCONN,
                              XWPDEV_MSGRSP,
                              XWPDEV_BADREG,
                              XWPDEV_ACK,
                              XWPDEV_DELGAME,
                              XWPDEV_ALERT,
                              XWPDEV_UPGRADE,
                              XWPDEV_INVITE,
                              XWPDEV_GOTINVITE, // test without this!!!
    };

    public static void gcmConfirmed( Context context, boolean confirmed )
    {
        if ( s_gcmWorking != confirmed ) {
            Log.i( TAG, "gcmConfirmed(): changing s_gcmWorking to %b",
                   confirmed );
            s_gcmWorking = confirmed;
        }

        // If we've gotten a GCM id and haven't registered it, do so!
        if ( confirmed && !s_curType.equals( DevIDType.ID_TYPE_ANDROID_GCM ) ) {
            s_regStartTime = 0;      // so we're sure to register
            devIDChanged();
            timerFired( context );
        }
    }

    public static boolean relayEnabled( Context context )
    {
        boolean enabled = ! XWPrefs
            .getPrefsBoolean( context, R.string.key_disable_relay, false );
        return enabled;
    }

    public static void enabledChanged( Context context ) {
        boolean enabled = relayEnabled( context );
        if ( enabled ) {
            startService( context );
        } else {
            stopService( context );
        }
    }

    public static void setEnabled( Context context, boolean enabled ) {
        XWPrefs.setPrefsBoolean( context, R.string.key_disable_relay, !enabled );
        enabledChanged( context );
    }

    public static void startService( Context context )
    {
        Log.i( TAG, "startService()" );
        Intent intent = getIntentTo( context, MsgCmds.UDP_CHANGED );
        context.startService( intent );
    }

    private static void stopService( Context context )
    {
        Intent intent = getIntentTo( context, MsgCmds.STOP );
        context.startService( intent );
    }

    public static void inviteRemote( Context context, int destDevID,
                                     String relayID, NetLaunchInfo nli )
    {
        int myDevID = DevID.getRelayDevIDInt( context );
        if ( 0 != myDevID ) {
            context.startService( getIntentTo( context, MsgCmds.INVITE )
                                  .putExtra( DEV_ID_SRC, myDevID )
                                  .putExtra( DEV_ID_DEST, destDevID )
                                  .putExtra( RELAY_ID, relayID )
                                  .putExtra( NLI_DATA, nli.toString() ) );
        }
    }

    public static void reset( Context context )
    {
        Intent intent = getIntentTo( context, MsgCmds.RESET );
        context.startService( intent );
    }

    public static void timerFired( Context context )
    {
        Intent intent = getIntentTo( context, MsgCmds.TIMER_FIRED );
        context.startService( intent );
    }

    public static int sendPacket( Context context, long rowid, byte[] msg )
    {
        Log.d( TAG, "sendPacket(len=%d)", msg.length );
        int result = -1;
        if ( NetStateCache.netAvail( context ) ) {
            Intent intent = getIntentTo( context, MsgCmds.SEND )
                .putExtra( ROWID, rowid )
                .putExtra( BINBUFFER, msg );
            context.startService( intent );
            result = msg.length;
        } else {
            Log.w( TAG, "sendPacket: network down" );
        }
        return result;
    }

    public static int sendNoConnPacket( Context context, long rowid, String relayID,
                                        byte[] msg )
    {
        int result = -1;
        if ( NetStateCache.netAvail( context ) ) {
            Intent intent = getIntentTo( context, MsgCmds.SENDNOCONN )
                .putExtra( ROWID, rowid )
                .putExtra( RELAY_ID, relayID )
                .putExtra( BINBUFFER, msg );
            context.startService( intent );
            result = msg.length;
        }
        return result;
    }

    public static void devIDChanged()
    {
        s_registered = false;
    }

    private void receiveInvitation( int srcDevID, NetLaunchInfo nli )
    {
        Log.d( TAG, "receiveInvitation: got nli from %d: %s", srcDevID,
               nli.toString() );
        if ( checkNotDupe( nli ) ) {
            makeOrNotify( nli );
        }
    }

    private void makeOrNotify( NetLaunchInfo nli )
    {
        if ( DictLangCache.haveDict( this, nli.lang, nli.dict ) ) {
            makeGame( nli );
        } else {
            Intent intent = MultiService
                .makeMissingDictIntent( this, nli,
                                        DictFetchOwner.OWNER_RELAY );
            MultiService.postMissingDictNotification( this, intent,
                                                      nli.gameID() );
        }
    }

    private void makeGame( NetLaunchInfo nli )
    {
        long[] rowids = DBUtils.getRowIDsFor( this, nli.gameID() );
        if ( (null == rowids || 0 == rowids.length)
             || XWPrefs.getRelayInviteToSelfEnabled( this )) {

            if ( DictLangCache.haveDict( this, nli.lang, nli.dict ) ) {
                long rowid = GameUtils.makeNewMultiGame( this, nli,
                                                         new RelayMsgSink(),
                                                         getUtilCtxt() );
                if ( DBUtils.ROWID_NOTFOUND != rowid ) {
                    if ( null != nli.gameName && 0 < nli.gameName.length() ) {
                        DBUtils.setName( this, rowid, nli.gameName );
                    }
                    String body = LocUtils.getString( this,
                                                      R.string.new_relay_body );
                    GameUtils.postInvitedNotification( this, nli.gameID(), body,
                                                       rowid );
                }
            } else {
                Intent intent = MultiService
                    .makeMissingDictIntent( this, nli,
                                            DictFetchOwner.OWNER_RELAY );
                MultiService.postMissingDictNotification( this, intent,
                                                          nli.gameID() );
            }
        }
    }

    // Exists to get incoming data onto the main thread
    private static void postData( Context context, long rowid, byte[] msg )
    {
        Log.d( TAG, "postData(): packet of length %d for token %d",
               msg.length, rowid );
        if ( DBUtils.haveGame( context, rowid ) ) {
            Intent intent = getIntentTo( context, MsgCmds.RECEIVE )
                .putExtra( ROWID, rowid )
                .putExtra( BINBUFFER, msg );
            context.startService( intent );
        } else {
            Log.w( TAG, "postData(): Dropping message for rowid %d:"
                   + " not on device", rowid );
        }
    }

    public static void udpChanged( Context context )
    {
        startService( context );
    }

    public static void processGameMsgs( Context context, String relayId,
                                        String[] msgs64 )
    {
        Intent intent = getIntentTo( context, MsgCmds.PROCESS_GAME_MSGS )
            .putExtra( MSGS_ARR, msgs64 )
            .putExtra( RELAY_ID, relayId );
        context.startService( intent );
    }

    public static void processDevMsgs( Context context, String[] msgs64 )
    {
        Intent intent = getIntentTo( context, MsgCmds.PROCESS_DEV_MSGS )
            .putExtra( MSGS_ARR, msgs64 );
        context.startService( intent );
    }

    private static Intent getIntentTo( Context context, MsgCmds cmd )
    {
        Intent intent = new Intent( context, RelayService.class );
        intent.putExtra( CMD_STR, cmd.ordinal() );
        return intent;
    }

    @Override
    protected MultiMsgSink getSink( long rowid )
    {
        return new RelayMsgSink().setRowID( rowid );
    }

    @Override
    public void onCreate()
    {
        super.onCreate();
        m_lastGamePacketReceived =
            XWPrefs.getPrefsLong( this, R.string.key_last_packet,
                                  Utils.getCurSeconds() );

        m_handler = new Handler();
        m_onInactivity = new Runnable() {
                public void run() {
                    Log.d( TAG, "m_onInactivity fired" );
                    if ( !shouldMaintainConnection() ) {
                        NetStateCache.unregister( RelayService.this,
                                                  RelayService.this );
                        stopSelf();
                    } else {
                        timerFired( RelayService.this );
                    }
                }
            };
    }

    @Override
    public int onStartCommand( Intent intent, int flags, int startId )
    {
        Integer result = null;
        if ( null != intent ) {
            MsgCmds cmd;
            try {
                cmd = MsgCmds.values()[intent.getIntExtra( CMD_STR, -1 )];
            } catch (Exception ex) { // OOB most likely
                cmd = null;
            }
            if ( null != cmd ) {
                Log.d( TAG, "onStartCommand(): cmd=%s", cmd.toString() );
                switch( cmd ) {
                case PROCESS_GAME_MSGS:
                    String[] relayIDs = new String[1];
                    relayIDs[0] = intent.getStringExtra( RELAY_ID );
                    long[] rowIDs = DBUtils.getRowIDsFor( this, relayIDs[0] );
                    if ( 0 < rowIDs.length ) {
                        byte[][][] msgs = expandMsgsArray( intent );
                        process( msgs, rowIDs, relayIDs );
                    }
                    break;
                case PROCESS_DEV_MSGS:
                    byte[][][] msgss = expandMsgsArray( intent );
                    for ( byte[][] msgs : msgss ) {
                        for ( byte[] msg : msgs ) {
                            gotPacket( msg, true );
                        }
                    }
                    break;
                case UDP_CHANGED:
                    startThreads();
                    break;
                case RESET:
                    stopThreads();
                    startThreads();
                    break;
                case UPGRADE:
                    UpdateCheckReceiver.checkVersions( this, false );
                    break;
                case GOT_INVITE:
                    int srcDevID = intent.getIntExtra( INVITE_FROM, 0 );
                    NetLaunchInfo nli
                        = new NetLaunchInfo( this, intent.getStringExtra(NLI_DATA) );
                    receiveInvitation( srcDevID, nli );
                    break;
                case SEND:
                case RECEIVE:
                case SENDNOCONN:
                    startUDPThreadsIfNot();
                    long rowid = intent.getLongExtra( ROWID, -1 );
                    byte[] msg = intent.getByteArrayExtra( BINBUFFER );
                    if ( MsgCmds.SEND == cmd ) {
                        sendMessage( rowid, msg );
                    } else if ( MsgCmds.SENDNOCONN == cmd ) {
                        String relayID = intent.getStringExtra( RELAY_ID );
                        sendNoConnMessage( rowid, relayID, msg );
                    } else {
                        receiveMessage( this, rowid, null, msg, s_addr );
                    }
                    break;
                case INVITE:
                    startUDPThreadsIfNot();
                    srcDevID = intent.getIntExtra( DEV_ID_SRC, 0 );
                    int destDevID = intent.getIntExtra( DEV_ID_DEST, 0 );
                    String relayID = intent.getStringExtra( RELAY_ID );
                    String nliData = intent.getStringExtra( NLI_DATA );
                    sendInvitation( srcDevID, destDevID, relayID, nliData );
                    break;
                case TIMER_FIRED:
                    if ( !NetStateCache.netAvail( this ) ) {
                        Log.w( TAG, "not connecting: no network" );
                    } else if ( startFetchThreadIfNotUDP() ) {
                        // do nothing
                    } else if ( registerWithRelayIfNot() ) {
                        requestMessages();
                    }
                    RelayReceiver.setTimer( this );
                    break;
                case STOP:
                    stopThreads();
                    stopSelf();
                    break;
                default:
                    Assert.fail();
                }

                result = Service.START_STICKY;
            }
        }

        if ( null == result ) {
            result = Service.START_STICKY_COMPATIBILITY;
        }

        NetStateCache.register( this, this );
        resetExitTimer();
        return result;
    }

    @Override
    public void onDestroy()
    {
        if ( shouldMaintainConnection() ) {
            long interval_millis = getMaxIntervalSeconds() * 1000;
            RelayReceiver.setTimer( this, interval_millis );
        }
        stopThreads();
        super.onDestroy();
    }

    // NetStateCache.StateChangedIf interface
    public void onNetAvail( boolean nowAvailable )
    {
        startService( this ); // bad name: will *stop* threads too
    }

    private void setupNotifications( String[] relayIDs, BackMoveResult[] bmrs,
                                     ArrayList<Boolean> locals )
    {
        for ( int ii = 0; ii < relayIDs.length; ++ii ) {
            String relayID = relayIDs[ii];
            BackMoveResult bmr = bmrs[ii];
            long[] rowids = DBUtils.getRowIDsFor( this, relayID );
            if ( null != rowids ) {
                for ( long rowid : rowids ) {
                    GameUtils.postMoveNotification( this, rowid, bmr,
                                                    locals.get(ii) );
                }
            }
        }
    }

    private boolean startFetchThreadIfNotUDP()
    {
        // DbgUtils.logf( "startFetchThreadIfNotUDP()" );
        boolean handled = relayEnabled( this ) && !XWApp.UDP_ENABLED;
        if ( handled && null == m_fetchThread ) {
            m_fetchThread = new Thread( null, new Runnable() {
                    public void run() {
                        fetchAndProcess();
                        m_fetchThread = null;
                        RelayService.this.stopSelf();
                    }
                }, getClass().getName() );
            m_fetchThread.start();
        }
        return handled;
    }

    private void stopFetchThreadIf()
    {
        while ( null != m_fetchThread ) {
            Log.w( TAG, "2: m_fetchThread NOT NULL; WHAT TO DO???" );
            try {
                Thread.sleep( 20 );
            } catch( java.lang.InterruptedException ie ) {
                Log.ex( TAG, ie );
            }
        }
    }

    private void startUDPThreadsIfNot()
    {
        if ( XWApp.UDP_ENABLED && relayEnabled( this ) ) {
            if ( null == m_UDPReadThread ) {
                m_UDPReadThread = new Thread( null, new Runnable() {
                        public void run() {

                            connectSocket(); // block until this is done
                            startWriteThread();

                            Log.i( TAG, "read thread running" );
                            byte[] buf = new byte[1024];
                            for ( ; ; ) {
                                DatagramPacket packet =
                                    new DatagramPacket( buf, buf.length );
                                try {
                                    m_UDPSocket.receive( packet );
                                    resetExitTimer();
                                    gotPacket( packet );
                                } catch ( java.io.InterruptedIOException iioe ) {
                                    // DbgUtils.logf( "FYI: udp receive timeout" );
                                } catch( java.io.IOException ioe ) {
                                    break;
                                }
                            }
                            Log.i( TAG, "read thread exiting" );
                        }
                    }, getClass().getName() );
                m_UDPReadThread.start();
            } else {
                Log.i( TAG, "m_UDPReadThread not null and assumed to be running" );
            }
        } else {
            Log.i( TAG, "startUDPThreadsIfNot(): UDP disabled" );
        }
    } // startUDPThreadsIfNot

    private void connectSocket()
    {
        if ( null == m_UDPSocket ) {
            int port = XWPrefs.getDefaultRelayPort( this );
            String host = XWPrefs.getDefaultRelayHost( this );
            try {
                m_UDPSocket = new DatagramSocket();
                m_UDPSocket.setSoTimeout(30 * 1000); // timeout so we can log

                InetAddress addr = InetAddress.getByName( host );
                m_UDPSocket.connect( addr, port ); // remember this address
                Log.d( TAG, "connectSocket(%s:%d): m_UDPSocket now %H",
                       host, port, m_UDPSocket );
            } catch( java.net.SocketException se ) {
                Log.ex( TAG, se );
                Assert.fail();
            } catch( java.net.UnknownHostException uhe ) {
                Log.ex( TAG, uhe );
            }
        } else {
            Assert.assertTrue( m_UDPSocket.isConnected() );
            Log.i( TAG, "m_UDPSocket not null" );
        }
    }

    private void startWriteThread()
    {
        if ( null == m_UDPWriteThread ) {
            m_UDPWriteThread = new Thread( null, new Runnable() {
                    public void run() {
                        Log.i( TAG, "write thread starting" );
                        for ( ; ; ) {
                            List<PacketData> dataList = null;
                            try {
                                dataList = new ArrayList<>();
                                for ( PacketData outData = m_queue.take(); // blocks
                                      null != outData;
                                      outData = m_queue.poll() ) {         // doesn't block
                                    if ( outData.isEOQ() ) {
                                        dataList = null;
                                        break;
                                    }
                                    dataList.add(outData);
                                    Log.d( TAG, "got %d packets; %d more left", dataList.size(),
                                           m_queue.size());
                                }
                            } catch ( InterruptedException ie ) {
                                Log.w( TAG, "write thread killed" );
                                break;
                            }
                            if ( null == dataList ) {
                                Log.i( TAG, "stopping write thread" );
                                break;
                            }

                            int sentLen;
                            if ( XWPrefs.getPreferWebAPI( RelayService.this ) ) {
                                sentLen = sendViaWeb( dataList );
                            } else {
                                sentLen = sendViaUDP( dataList );
                            }

                            resetExitTimer();
                            ConnStatusHandler.showSuccessOut();
                        }
                        Log.i( TAG, "write thread exiting" );
                    }
                }, getClass().getName() );
            m_UDPWriteThread.start();
        } else {
            Log.i( TAG, "m_UDPWriteThread not null and assumed to "
                   + "be running" );
        }
    }

    private int sendViaWeb( List<PacketData> packets )
    {
        Log.d( TAG, "sendViaWeb(): sending %d at once", packets.size() );
        int sentLen = 0;
        HttpURLConnection conn = NetUtils.makeHttpRelayConn( this, "post" );
        if ( null == conn ) {
            Log.e( TAG, "sendViaWeb(): null conn for POST" );
        } else {
            try {
                JSONArray dataArray = new JSONArray();
                for ( PacketData packet : packets ) {
                    Assert.assertFalse( packet.isEOQ() );
                    byte[] datum = packet.assemble();
                    dataArray.put( Utils.base64Encode(datum) );
                    sentLen += datum.length;
                }
                JSONObject params = new JSONObject();
                params.put( "data", dataArray );

                String result = NetUtils.runConn(conn, params);
                if ( null != result ) {
                    Log.d( TAG, "sendViaWeb(): POST(%s) => %s", params, result );
                    JSONObject resultObj = new JSONObject( result );
                    JSONArray resData = resultObj.getJSONArray( "data" );
                    int nReplies = resData.length();
                    Log.d( TAG, "sendViaWeb(): got %d replies", nReplies );

                    noteSent( packets ); // before we process the acks below :-)

                    if ( nReplies > 0 ) {
                        resetExitTimer();
                    }
                    for ( int ii = 0; ii < nReplies; ++ii ) {
                        byte[] datum = Utils.base64Decode( resData.getString( ii ) );
                        // PENDING: skip ack or not
                        gotPacket( datum, false );
                    }
                } else {
                    Log.e( TAG, "sendViaWeb(): failed result for POST" );
                }
            } catch ( JSONException ex ) {
                Assert.assertFalse( BuildConfig.DEBUG );
            }
        }
        return sentLen;
    }

    private int sendViaUDP( List<PacketData> packets )
    {
        int sentLen = 0;
        for ( PacketData packet : packets ) {
            boolean getOut = true;
            byte[] data = packet.assemble();
            try {
                DatagramPacket udpPacket = new DatagramPacket( data, data.length );
                m_UDPSocket.send( udpPacket );
                if ( BuildConfig.DEBUG ) {
                    Assert.assertFalse( XWPrefs.getPreferWebAPI( this ) );
                }
                sentLen += udpPacket.getLength();
                noteSent( packet );
                getOut = false;
            } catch ( java.net.SocketException se ) {
                Log.ex( TAG, se );
                Log.i( TAG, "Restarting threads to force"
                       + " new socket" );
                m_handler.post( new Runnable() {
                        public void run() {
                            stopUDPThreadsIf();
                        }
                    } );
            } catch ( java.io.IOException ioe ) {
                Log.ex( TAG, ioe );
            } catch ( NullPointerException npe ) {
                Log.w( TAG, "network problem; dropping packet" );
            }
            if ( getOut ) {
                break;
            }
        }
        return sentLen;
    }

    private void noteSent( PacketData packet )
    {
        int pid = packet.m_packetID;
        Log.d( TAG, "Sent [udp?] packet: cmd=%s, id=%d",
               packet.m_cmd.toString(), pid);
        if ( packet.m_cmd != XWRelayReg.XWPDEV_ACK ) {
            synchronized( s_packetsSent ) {
                s_packetsSent.put( pid, packet );
            }
        }
    }

    private void noteSent( List<PacketData> packets )
    {
        for ( PacketData packet : packets ) {
            noteSent( packet );
        }
    }

    private void stopUDPThreadsIf()
    {
        if ( null != m_UDPWriteThread ) {
            // can't add null
            m_queue.add( new PacketData() );
            try {
                Log.d( TAG, "joining m_UDPWriteThread" );
                m_UDPWriteThread.join();
                Log.d( TAG, "SUCCESSFULLY joined m_UDPWriteThread" );
            } catch( java.lang.InterruptedException ie ) {
                Log.ex( TAG, ie );
            }
            m_UDPWriteThread = null;
            m_queue.clear();
        }
        if ( null != m_UDPSocket && null != m_UDPReadThread ) {
            m_UDPSocket.close();
            try {
                m_UDPReadThread.join();
            } catch( java.lang.InterruptedException ie ) {
                Log.ex( TAG, ie );
            }
            m_UDPReadThread = null;
            m_UDPSocket = null;
        }
    }

    // MIGHT BE Running on reader thread
    private void gotPacket( byte[] data, boolean skipAck )
    {
        boolean resetBackoff = false;
        ByteArrayInputStream bis = new ByteArrayInputStream( data );
        DataInputStream dis = new DataInputStream( bis );
        try {
            PacketHeader header = readHeader( dis );
            if ( null != header ) {
                if ( !skipAck ) {
                    sendAckIf( header );
                }
                Log.d( TAG, "gotPacket(): cmd=%s", header.m_cmd.toString() );
                switch ( header.m_cmd ) {
                case XWPDEV_UNAVAIL:
                    int unavail = dis.readInt();
                    Log.i( TAG, "relay unvailable for another %d seconds",
                           unavail );
                    String str = getVLIString( dis );
                    postEvent( MultiEvent.RELAY_ALERT, str );
                    break;
                case XWPDEV_ALERT:
                    str = getVLIString( dis );
                    Intent intent = GamesListDelegate.makeAlertIntent( this, str );
                    Utils.postNotification( this, intent,
                                            R.string.relay_alert_title,
                                            str, str.hashCode() );
                    break;
                case XWPDEV_BADREG:
                    str = getVLIString( dis );
                    Log.i( TAG, "bad relayID \"%s\" reported", str );
                    DevID.clearRelayDevID( this );
                    s_registered = false;
                    registerWithRelay();
                    break;
                case XWPDEV_REGRSP:
                    str = getVLIString( dis );
                    short maxIntervalSeconds = dis.readShort();
                    Log.d( TAG, "got relayid %s (%d), maxInterval %d", str,
                           Integer.parseInt( str, 16 ), maxIntervalSeconds );
                    setMaxIntervalSeconds( maxIntervalSeconds );
                    DevID.setRelayDevID( this, str );
                    s_registered = true;
                    break;
                case XWPDEV_HAVEMSGS:
                    requestMessages();
                    break;
                case XWPDEV_MSG:
                    int token = dis.readInt();
                    byte[] msg = new byte[dis.available()];
                    dis.readFully( msg );
                    postData( this, token, msg );

                    // game-related packets only count
                    long lastGamePacketReceived = Utils.getCurSeconds();
                    if ( lastGamePacketReceived != m_lastGamePacketReceived ) {
                        XWPrefs.setPrefsLong( this, R.string.key_last_packet,
                                              lastGamePacketReceived );
                        m_lastGamePacketReceived = lastGamePacketReceived;
                    }
                    resetBackoff = true;
                    break;
                case XWPDEV_UPGRADE:
                    intent = getIntentTo( this, MsgCmds.UPGRADE );
                    startService( intent );
                    break;
                case XWPDEV_GOTINVITE:
                    resetBackoff = true;
                    intent = getIntentTo( this, MsgCmds.GOT_INVITE );
                    int srcDevID = dis.readInt();
                    byte[] nliData = new byte[dis.readShort()];
                    dis.readFully( nliData );
                    NetLaunchInfo nli = XwJNI.nliFromStream( nliData );
                    intent.putExtra( INVITE_FROM, srcDevID );
                    String asStr = nli.toString();
                    Log.d( TAG, "got invitation: %s", asStr );
                    intent.putExtra( NLI_DATA, asStr );
                    startService( intent );
                    break;
                case XWPDEV_ACK:
                    noteAck( vli2un( dis ) );
                    break;
                // case XWPDEV_MSGFWDOTHERS:
                //     Assert.assertTrue( 0 == dis.readByte() ); // protocol; means "invite", I guess.
                //     String nliData = dis.readUTF();
                //     DbgUtils.logf( "RelayService: got invite: %s", nliData );
                //     break;
                default:
                    Log.w( TAG, "gotPacket(): Unhandled cmd" );
                    break;
                }
            }
        } catch ( java.io.IOException ioe ) {
            Log.ex( TAG, ioe );
        }

        if ( resetBackoff ) {
            resetBackoffTimer();
        }
    }

    private void gotPacket( DatagramPacket packet )
    {
        ConnStatusHandler.showSuccessIn();

        int packetLen = packet.getLength();
        byte[] data = new byte[packetLen];
        System.arraycopy( packet.getData(), 0, data, 0, packetLen );
        // DbgUtils.logf( "RelayService::gotPacket: %d bytes of data", packetLen );
        gotPacket( data, false );
    } // gotPacket

    private boolean shouldRegister()
    {
        boolean should = relayEnabled( this );
        if ( should ) {
            String relayID = DevID.getRelayDevID( this, true );
            boolean registered = null != relayID;
            should = !registered;
        }
        Log.d( TAG, "shouldRegister()=>%b", should );
        return should;
    }

    // Register: pass both the relay-assigned relayID (or empty string
    // if none has been assigned yet) and the deviceID IFF it's
    // changed since we last registered (Otherwise just ID_TYPE_NONE
    // and no string)
    //
    // How do we know if we need to register?  We keep a timestamp
    // indicating when we last got a reg-response.  When the GCM id
    // changes, that timestamp is cleared.
    private void registerWithRelay()
    {
        long now = Utils.getCurSeconds();
        long interval = now - s_regStartTime;
        if ( interval < REG_WAIT_INTERVAL ) {
            Log.i( TAG, "registerWithRelay: skipping because only %d "
                   + "seconds since last start", interval );
        } else {
            String relayID = DevID.getRelayDevID( this );
            DevIDType[] typa = new DevIDType[1];
            String devid = getDevID( typa );
            DevIDType typ = typa[0];
            s_curType = typ;

            ByteArrayOutputStream bas = new ByteArrayOutputStream();
            try {
                DataOutputStream out = new DataOutputStream( bas );

                writeVLIString( out, relayID );             // may be empty
                if ( DevIDType.ID_TYPE_RELAY == typ ) {     // all's well
                    out.writeByte( DevIDType.ID_TYPE_NONE.ordinal() );
                } else {
                    out.writeByte( typ.ordinal() );
                    writeVLIString( out, devid );
                }

                Log.d( TAG, "registering devID \"%s\" (type=%s)", devid,
                       typ.toString() );

                out.writeShort( BuildConfig.CLIENT_VERS_RELAY );
                writeVLIString( out, BuildConfig.GIT_REV );
                // writeVLIString( out, String.format( "€%s", Build.MODEL) );
                writeVLIString( out, Build.MODEL );
                writeVLIString( out, Build.VERSION.RELEASE );

                postPacket( bas, XWRelayReg.XWPDEV_REG );
                s_regStartTime = now;
            } catch ( java.io.IOException ioe ) {
                Log.ex( TAG, ioe );
            }
        }
    }

    private boolean registerWithRelayIfNot()
    {
        if ( !s_registered && shouldRegister() ) {
            registerWithRelay();
        }
        return s_registered;
    }

    private void requestMessagesImpl( XWRelayReg reg )
    {
        ByteArrayOutputStream bas = new ByteArrayOutputStream();
        try {
            DevIDType[] typp = new DevIDType[1];
            String devid = getDevID( typp );
            if ( null != devid ) {
                DataOutputStream out = new DataOutputStream( bas );
                writeVLIString( out, devid );
                Log.d(TAG, "requestMessagesImpl(): devid: %s; type: " + typp[0], devid );
                postPacket( bas, reg );
            } else {
                Log.d(TAG, "requestMessagesImpl(): devid is null" );
            }
        } catch ( java.io.IOException ioe ) {
            Log.ex( TAG, ioe );
        }
    }

    private void requestMessages()
    {
        requestMessagesImpl( XWRelayReg.XWPDEV_RQSTMSGS );
    }

    // private void sendKeepAlive()
    // {
    //     requestMessagesImpl( XWRelayReg.XWPDEV_KEEPALIVE );
    // }

    private void sendMessage( long rowid, byte[] msg )
    {
        ByteArrayOutputStream bas = new ByteArrayOutputStream();
        try {
            DataOutputStream out = new DataOutputStream( bas );
            Assert.assertTrue( rowid < Integer.MAX_VALUE );
            out.writeInt( (int)rowid );
            out.write( msg, 0, msg.length );
            postPacket( bas, XWRelayReg.XWPDEV_MSG );
        } catch ( java.io.IOException ioe ) {
            Log.ex( TAG, ioe );
        }
    }

    private void sendNoConnMessage( long rowid, String relayID, byte[] msg )
    {
        ByteArrayOutputStream bas = new ByteArrayOutputStream();
        try {
            DataOutputStream out = new DataOutputStream( bas );
            Assert.assertTrue( rowid < Integer.MAX_VALUE ); // ???
            out.writeInt( (int)rowid );
            out.writeBytes( relayID );
            out.write( '\n' );
            out.write( msg, 0, msg.length );
            postPacket( bas, XWRelayReg.XWPDEV_MSGNOCONN );
        } catch ( java.io.IOException ioe ) {
            Log.ex( TAG, ioe );
        }
    }

    private void sendInvitation( int srcDevID, int destDevID, String relayID,
                                 String nliStr )
    {
        Log.d( TAG, "sendInvitation(%d->%d/%s [%s])", srcDevID, destDevID,
               relayID, nliStr );

        NetLaunchInfo nli = new NetLaunchInfo( this, nliStr );
        byte[] nliData = XwJNI.nliToStream( nli );
        if ( BuildConfig.DEBUG ) {
            NetLaunchInfo tmp = XwJNI.nliFromStream( nliData );
            Log.d( TAG, "sendInvitation: compare these: %s vs %s",
                   nli.toString(), tmp.toString() );
        }

        ByteArrayOutputStream bas = new ByteArrayOutputStream();
        try {
            DataOutputStream out = new DataOutputStream( bas );
            out.writeInt( srcDevID );
            if ( 0 == destDevID ) {
                Assert.assertTrue( null != relayID && 0 < relayID.length() );
                out.writeBytes( relayID );
                out.writeByte( 0 ); // null terminator
            } else {
                out.writeByte( 0 ); // empty dev
                out.writeInt( destDevID );
            }
            out.writeShort( nliData.length );
            out.write( nliData, 0, nliData.length );
            postPacket( bas, XWRelayReg.XWPDEV_INVITE );
        } catch ( java.io.IOException ioe ) {
            Log.ex( TAG, ioe );
        }
    }

    private void sendAckIf( PacketHeader header )
    {
        if ( 0 != header.m_packetID ) {
            ByteArrayOutputStream bas = new ByteArrayOutputStream();
            try {
                DataOutputStream out = new DataOutputStream( bas );
                un2vli( header.m_packetID, out );
                postPacket( bas, XWRelayReg.XWPDEV_ACK );
            } catch ( java.io.IOException ioe ) {
                Log.ex( TAG, ioe );
            }
        }
    }

    private PacketHeader readHeader( DataInputStream dis )
        throws java.io.IOException
    {
        PacketHeader result = null;
        byte proto = dis.readByte();
        if ( XWPDevProto.XWPDEV_PROTO_VERSION_1.ordinal() == proto ) {
            int packetID = vli2un( dis );
            if ( 0 != packetID ) {
                Log.d( TAG, "readHeader(): got packetID %d", packetID );
            }
            byte ordinal = dis.readByte();
            XWRelayReg cmd = XWRelayReg.values()[ordinal];
            result = new PacketHeader( cmd, packetID );
        } else {
            Log.w( TAG, "bad proto: %d", proto );
        }
        return result;
    }

    private String getVLIString( DataInputStream dis )
        throws java.io.IOException
    {
        byte[] tmp = new byte[vli2un( dis )];
        dis.readFully( tmp );
        String result = new String( tmp );
        return result;
    }

    private void postPacket( ByteArrayOutputStream bas, XWRelayReg cmd )
    {
        m_queue.add( new PacketData( bas, cmd ) );
        // 0 ok; thread will often have sent already!
        // DbgUtils.logf( "postPacket() done; %d in queue", m_queue.size() );
    }

    private String getDevID( DevIDType[] typp )
    {
        DevIDType typ;
        String devid = DevID.getRelayDevID( this, true );

        if ( null != devid && 0 < devid.length() ) {
            typ = DevIDType.ID_TYPE_RELAY;
        } else {
            devid = DevID.getGCMDevID( this );
            if ( null != devid && 0 < devid.length() ) {
                typ = DevIDType.ID_TYPE_ANDROID_GCM;
            } else {
                devid = "";
                typ = DevIDType.ID_TYPE_ANON;
            }
        }
        if ( null != typp ) {
            typp[0] = typ;
        } else if ( typ != DevIDType.ID_TYPE_RELAY ) {
            devid = null;
        }
        return devid;
    }

    private void fetchAndProcess()
    {
        long[][] rowIDss = new long[1][];
        String[] relayIDs = DBUtils.getRelayIDs( this, rowIDss );
        if ( null != relayIDs && 0 < relayIDs.length ) {
            byte[][][] msgs = NetUtils.queryRelay( this, relayIDs );
            process( msgs, rowIDss[0], relayIDs );
        }
    }

    private void process( byte[][][] msgs, long[] rowIDs, String[] relayIDs )
    {
        if ( null != msgs ) {
            RelayMsgSink sink = new RelayMsgSink();
            int nameCount = relayIDs.length;
            ArrayList<String> idsWMsgs = new ArrayList<String>( nameCount );
            ArrayList<Boolean> isLocals = new ArrayList<Boolean>( nameCount );
            ArrayList<BackMoveResult> bmrs =
                new ArrayList<BackMoveResult>( nameCount );

            boolean[] isLocalP = new boolean[1];
            for ( int ii = 0; ii < nameCount; ++ii ) {
                byte[][] forOne = msgs[ii];
                if ( null != forOne ) {
                    long rowid = rowIDs[ii];
                    sink.setRowID( rowid );
                    for ( byte[] msg : forOne ) {
                        receiveMessage( this, rowid, sink, msg, s_addr );
                    }
                }
            }
            sink.send( this );
        }
    }

    private static class AsyncSender extends AsyncTask<Void, Void, Void> {
        private Context m_context;
        private HashMap<String,ArrayList<byte[]>> m_msgHash;

        public AsyncSender( Context context,
                            HashMap<String,ArrayList<byte[]>> msgHash )
        {
            m_context = context;
            m_msgHash = msgHash;
        }

        @Override
        protected Void doInBackground( Void... ignored )
        {
            Assert.assertFalse( XWPrefs.getPreferWebAPI( m_context ) );
            // format: total msg lenth: 2
            //         number-of-relayIDs: 2
            //         for-each-relayid: relayid + '\n': varies
            //                           message count: 1
            //                           for-each-message: length: 2
            //                                             message: varies

            // Build up a buffer containing everything but the total
            // message length and number of relayIDs in the message.
            try {
                ByteArrayOutputStream store =
                    new ByteArrayOutputStream( MAX_BUF ); // mem
                DataOutputStream outBuf = new DataOutputStream( store );
                int msgLen = 4;          // relayID count + protocol stuff
                int nRelayIDs = 0;

                Iterator<String> iter = m_msgHash.keySet().iterator();
                while ( iter.hasNext() ) {
                    String relayID = iter.next();
                    int thisLen = 1 + relayID.length(); // string and '\n'
                    thisLen += 2;                        // message count

                    ArrayList<byte[]> msgs = m_msgHash.get( relayID );
                    for ( byte[] msg : msgs ) {
                        thisLen += 2 + msg.length;
                    }

                    if ( msgLen + thisLen > MAX_BUF ) {
                        // Need to deal with this case by sending multiple
                        // packets.  It WILL happen.
                        Log.w( TAG, "dropping send for lack of space; FIX ME!!" );
                        Assert.fail();
                        break;
                    }
                    // got space; now write it
                    ++nRelayIDs;
                    outBuf.writeBytes( relayID );
                    outBuf.write( '\n' );
                    outBuf.writeShort( msgs.size() );
                    for ( byte[] msg : msgs ) {
                        outBuf.writeShort( msg.length );
                        outBuf.write( msg );
                    }
                    msgLen += thisLen;
                }
                // Now open a real socket, write size and proto, and
                // copy in the formatted buffer

                Assert.assertFalse( XWPrefs.getPreferWebAPI( m_context ) );
                Socket socket = NetUtils.makeProxySocket( m_context, 8000 );
                if ( null != socket ) {
                    DataOutputStream outStream =
                        new DataOutputStream( socket.getOutputStream() );
                    outStream.writeShort( msgLen );
                    outStream.writeByte( NetUtils.PROTOCOL_VERSION );
                    outStream.writeByte( NetUtils.PRX_PUT_MSGS );
                    outStream.writeShort( nRelayIDs );
                    outStream.write( store.toByteArray() );
                    outStream.flush();
                    socket.close();
                }
            } catch ( java.io.IOException ioe ) {
                Log.ex( TAG, ioe );
            }
            return null;
        } // doInBackground
    }

    private static void sendToRelay( Context context,
                                     HashMap<String,ArrayList<byte[]>> msgHash )
    {
        if ( null != msgHash ) {
            new AsyncSender( context, msgHash ).execute();
        } else {
            Log.w( TAG, "sendToRelay: null msgs" );
        }
    } // sendToRelay

    private class RelayMsgSink extends MultiMsgSink {
        private HashMap<String,ArrayList<byte[]>> m_msgLists = null;

        public RelayMsgSink() { super( RelayService.this ); }

        public void send( Context context )
        {
            if ( -1 == getRowID() ) {
                sendToRelay( context, m_msgLists );
            } else {
                Assert.assertNull( m_msgLists );
            }
        }

        @Override
        public int sendViaRelay( byte[] buf, int gameID )
        {
            Assert.assertTrue( -1 != getRowID() );
            sendPacket( RelayService.this, getRowID(), buf );
            return buf.length;
        }

        public boolean relayNoConnProc( byte[] buf, String relayID )
        {
            long rowID = getRowID();
            if ( -1 != rowID ) {
                sendNoConnMessage( rowID, relayID, buf );
            } else {
                if ( null == m_msgLists ) {
                    m_msgLists = new HashMap<String,ArrayList<byte[]>>();
                }

                ArrayList<byte[]> list = m_msgLists.get( relayID );
                if ( list == null ) {
                    list = new ArrayList<byte[]>();
                    m_msgLists.put( relayID, list );
                }
                list.add( buf );
            }
            return true;
        }
    }

    private static int nextPacketID( XWRelayReg cmd )
    {
        int nextPacketID = 0;
        if ( XWRelayReg.XWPDEV_ACK != cmd ) {
            nextPacketID = s_nextPacketID.incrementAndGet();
        }
        return nextPacketID;
    }

    private static void noteAck( int packetID )
    {
        PacketData packet;
        synchronized( s_packetsSent ) {
            packet = s_packetsSent.remove( packetID );
            if ( packet != null ) {
                Log.d( TAG, "noteAck(): removed for id %d: %s", packetID, packet );
            } else {
                Log.w( TAG, "Weird: got ack %d but never sent", packetID );
            }
            if ( BuildConfig.DEBUG ) {
                ArrayList<String> pstrs = new ArrayList<>();
                for ( Integer pkid : s_packetsSent.keySet() ) {
                    pstrs.add( s_packetsSent.get(pkid).toString() );
                }
                Log.d( TAG, "noteAck(): Got ack for %d; there are %d unacked packets: %s",
                       packetID, s_packetsSent.size(), TextUtils.join( ",", pstrs ) );
            }
        }
    }

    // Called from any thread
    private void resetExitTimer()
    {
        m_handler.removeCallbacks( m_onInactivity );

        // UDP socket's no good as a return address after several
        // minutes of inactivity, so do something after that time.
        m_handler.postDelayed( m_onInactivity,
                               getMaxIntervalSeconds() * 1000 );
    }

    private void startThreads()
    {
        Log.d( TAG, "startThreads()" );
        if ( !relayEnabled( this ) || !NetStateCache.netAvail( this ) ) {
            stopThreads();
        } else if ( XWApp.UDP_ENABLED ) {
            stopFetchThreadIf();
            startUDPThreadsIfNot();
            registerWithRelay();
        } else {
            stopUDPThreadsIf();
            startFetchThreadIfNotUDP();
        }
    }

    private void stopThreads()
    {
        Log.d( TAG, "stopThreads()" );
        stopFetchThreadIf();
        stopUDPThreadsIf();
    }

    private static void un2vli( int nn, OutputStream os )
        throws java.io.IOException
    {
        int indx = 0;
        boolean done = false;
        do {
            byte byt = (byte)(nn & 0x7F);
            nn >>= 7;
            done = 0 == nn;
            if ( done ) {
                byt |= 0x80;
            }
            os.write( byt );
        } while ( !done );
    }

    private static int vli2un( InputStream is ) throws java.io.IOException
    {
        int result = 0;
        byte[] buf = new byte[1];
        int nRead = 0;

        boolean done = false;
        for ( int count = 0; !done; ++count ) {
            nRead = is.read( buf );
            if ( 1 != nRead ) {
                Log.w( TAG, "vli2un: unable to read from stream" );
                break;
            }
            int byt = buf[0];
            done = 0 != (byt & 0x80);
            if ( done ) {
                byt &= 0x7F;
            }
            result |= byt << (7 * count);
        }

        return result;
    }

    private static void writeVLIString( DataOutputStream os, String str )
        throws java.io.IOException
    {
        if ( null == str ) {
            str = "";
        }
        byte[] bytes = str.getBytes( "UTF-8" );
        int len = bytes.length;
        un2vli( len, os );
        os.write( bytes, 0, len );
    }

    private void setMaxIntervalSeconds( int maxIntervalSeconds )
    {
        Log.d( TAG, "IGNORED: setMaxIntervalSeconds(%d); "
                       + "using -1 instead", maxIntervalSeconds );
        maxIntervalSeconds = -1;
        if ( m_maxIntervalSeconds != maxIntervalSeconds ) {
            m_maxIntervalSeconds = maxIntervalSeconds;
            XWPrefs.setPrefsInt( this, R.string.key_udp_interval,
                                 maxIntervalSeconds );
        }
    }

    private int getMaxIntervalSeconds()
    {
        if ( 0 == m_maxIntervalSeconds ) {
            m_maxIntervalSeconds = -1;
            // XWPrefs.getPrefsInt( this, R.string.key_udp_interval, 60 );
        }

        int result = m_maxIntervalSeconds;
        if ( -1 == result ) {
            result = figureBackoffSeconds();
        }

        Log.d( TAG, "getMaxIntervalSeconds() => %d", result );
        return result;
    }

    private byte[][][] expandMsgsArray( Intent intent )
    {
        String[] msgs64 = intent.getStringArrayExtra( MSGS_ARR );
        int count = msgs64.length;

        byte[][][] msgs = new byte[1][count][];
        for ( int ii = 0; ii < count; ++ii ) {
            msgs[0][ii] = Utils.base64Decode( msgs64[ii] );
        }
        return msgs;
    }

    /* Timers:
     *
     * Two goals: simulate the GCM experience for those who don't have
     * it (e.g. Kindle); and stop this service when it's not needed.
     * For now, we'll try to be immediately reachable from the relay
     * if: 1) the device doesn't have GCM; and 2) it's been less than
     * a week since we last received a packet from the relay.  We'll
     * do this even if there are no relay games left on the device in
     * order to support the rematch feature.
     *
     * Goal: maintain connection by keeping this service alive with
     * its periodic pings to relay.  When it dies or is killed,
     * notice, and use RelayReceiver's timer to get it restarted a bit
     * later.  But note: s_gcmWorking will not be set when the app is
     * relaunched.
     */

    private boolean shouldMaintainConnection()
    {
        boolean result = relayEnabled( this )
            && (XWApp.GCM_IGNORED || !s_gcmWorking);
        if ( result ) {
            long interval = Utils.getCurSeconds() - m_lastGamePacketReceived;
            result = interval < MAX_KEEPALIVE_SECS;
        }
        Log.d( TAG, "shouldMaintainConnection=>%b", result );
        return result;
    }

    private static void resetBackoffTimer()
    {
        synchronized( RelayService.class ) {
            s_curBackoff = 0;
            s_curNextTimer = Utils.getCurSeconds();
        }
    }

    private int figureBackoffSeconds() {
        // DbgUtils.printStack();
        int result = 60 * 60;   // default if no games
        if ( 0 < DBUtils.countOpenGamesUsingRelay( this ) ) {
            long diff;
            synchronized ( RelayService.class ) {
                long now = Utils.getCurSeconds();
                if ( s_curNextTimer <= now ) {
                    if ( 0 == s_curBackoff ) {
                        s_curBackoff = INITIAL_BACKOFF;
                    } else {
                        s_curBackoff = Math.min( 2 * s_curBackoff, result );
                    }
                    s_curNextTimer += s_curBackoff;
                }

                diff = s_curNextTimer - now;
            }
            Assert.assertTrue( diff < Integer.MAX_VALUE );
            result = (int)diff;
        }
        Log.d( TAG, "figureBackoffSeconds() => %d", result );
        return result;
    }

    private class PacketHeader {
        public int m_packetID;
        public XWRelayReg m_cmd;
        public PacketHeader( XWRelayReg cmd, int packetID ) {
            m_packetID = packetID;
            m_cmd = cmd;
        }
    }

    private class PacketData {
        public ByteArrayOutputStream m_bas;
        public XWRelayReg m_cmd;
        public byte[] m_header;
        public int m_packetID;
        private long m_created;

        public PacketData() {
            m_bas = null;
            m_created = System.currentTimeMillis();
        }

        public PacketData( ByteArrayOutputStream bas, XWRelayReg cmd )
        {
            this();
            m_bas = bas;
            m_cmd = cmd;
        }

        @Override
        public String toString()
        {
            return String.format( "{cmd: %s; age: %d ms}", m_cmd,
                                  System.currentTimeMillis() - m_created );
        }

        public boolean isEOQ() { return 0 == getLength(); }

        public int getLength()
        {
            int result = 0;
            if ( null != m_bas ) { // empty case?
                if ( null == m_header ) {
                    makeHeader();
                }
                result = m_header.length + m_bas.size();
            }
            return result;
        }

        public byte[] assemble()
        {
            byte[] data = new byte[getLength()];
            System.arraycopy( m_header, 0, data, 0, m_header.length );
            byte[] basData = m_bas.toByteArray();
            System.arraycopy( basData, 0, data, m_header.length, basData.length );
            return data;
        }

        private void makeHeader()
        {
            ByteArrayOutputStream bas = new ByteArrayOutputStream();
            try {
                m_packetID = nextPacketID( m_cmd );
                DataOutputStream out = new DataOutputStream( bas );
                Log.d( TAG, "makeHeader(): building packet with cmd %s",
                       m_cmd.toString() );
                out.writeByte( XWPDevProto.XWPDEV_PROTO_VERSION_1.ordinal() );
                un2vli( m_packetID, out );
                out.writeByte( m_cmd.ordinal() );
                m_header = bas.toByteArray();
            } catch ( java.io.IOException ioe ) {
                Log.ex( TAG, ioe );
            }
        }
    }
}
