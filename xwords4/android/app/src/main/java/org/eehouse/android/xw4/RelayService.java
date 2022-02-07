/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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
import android.os.Build;
import android.os.Handler;
import androidx.annotation.Nullable;
import android.text.TextUtils;

import org.eehouse.android.xw4.GameUtils.BackMoveResult;
import org.eehouse.android.xw4.MultiService.DictFetchOwner;
import org.eehouse.android.xw4.MultiService.MultiEvent;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.DUtilCtxt.DevIDType;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.Socket;
import java.net.SocketException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import javax.net.ssl.HttpsURLConnection;

public class RelayService extends XWJIService
    implements NetStateCache.StateChangedIf {
    private static final String TAG = RelayService.class.getSimpleName();
    private static final int MAX_SEND = 1024;
    private static final int MAX_BUF = MAX_SEND - 2;
    private static final int REG_WAIT_INTERVAL = 10;
    private static final int INITIAL_BACKOFF = 5;
    private static final int UDP_FAIL_LIMIT = 5;

    private static final long MIN_BACKOFF = 1000 * 60 * 2; // 2 minutes
    private static final long MAX_BACKOFF = 1000 * 60 * 60 * 23; // 23 hours

    // Must use the same jobID for all work enqueued for the same class. I
    // used to use the class's hashCode(), but that's different each time the
    // app runs. I think I was getting failures when a new instance launched
    // and found older jobs in the JobIntentService's work queue.
    private final static int sJobID = 218719978;
    static {
        XWJIService.register( RelayService.class, sJobID,
                              CommsConnType.COMMS_CONN_RELAY );
    }

    // One day, in seconds.  Probably should be configurable.
    private static final long MAX_KEEPALIVE_SECS = 24 * 60 * 60;

    private static enum MsgCmds implements XWJICmds { INVALID,
                                                      DO_WORK,
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
                                                      GOT_PACKET,
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
    private static final String MSGNUM = "MSGNUM";

    private static LinkedBlockingQueue<PacketData> s_queue =
        new LinkedBlockingQueue<>();
    private static List<PacketData> s_packetsSentUDP = new ArrayList<>();
    private static List<PacketData> s_packetsSentWeb = new ArrayList<>();
    private static final PacketData sEOQPacket = new PacketData();
    private static AtomicInteger s_nextPacketID = new AtomicInteger();
    private static long s_lastFCM = 0L;
    private static boolean s_registered = false;
    private static CommsAddrRec s_addr =
        new CommsAddrRec( CommsConnType.COMMS_CONN_RELAY );
    private static int s_curBackoff;
    private static long s_curNextTimer;
    private static DatagramSocket s_UDPSocket;

    static { resetBackoffTimer(); }

    private Thread m_fetchThread = null; // no longer used
    private static final AtomicReference<UDPReadThread> sUDPReadThreadRef
        = new AtomicReference<>();
    private Handler m_handler;
    private UDPReadThread mReadThread;
    private Runnable m_onInactivity;
    private int m_maxIntervalSeconds = 0;
    private long m_lastGamePacketReceived;
    private static AtomicInteger sNativeFailScore = new AtomicInteger();;
    private static boolean sSkipUPDSet;
    private RelayServiceHelper mHelper;
    private static DevIDType s_curType = DevIDType.ID_TYPE_NONE;
    private static long s_regStartTime = 0;

    // These must match the enum XWPDevProto in xwrelay.h
    private static enum XWPDevProto { XWPDEV_PROTO_VERSION_INVALID
            ,XWPDEV_PROTO_VERSION_1
            };

    private static TimerReceiver.TimerCallback sTimerCallbacks
        = new TimerReceiver.TimerCallback() {
                @Override
                public void timerFired( Context context )
                {
                    Log.d( TAG, "timerFired()" );
                    RelayService.timerFired( context );
                }

                @Override
                public long incrementBackoff( long backoff )
                {
                    if ( backoff < MIN_BACKOFF ) {
                        backoff = MIN_BACKOFF;
                    } else {
                        backoff *= 2;
                    }
                    if ( MAX_BACKOFF <= backoff ) {
                        backoff = MAX_BACKOFF;
                    }
                    return backoff;
                }
            };

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

    public static void fcmConfirmed( Context context, boolean working )
    {
        Log.d( TAG, "fcmConfirmed(working=%b)", working );
        long newVal = working ? System.currentTimeMillis() : 0L;
        if ( (s_lastFCM == 0) != working ) {
            Log.i( TAG, "fcmConfirmed(): changing s_lastFCM to %d",
                   newVal );
        }
        s_lastFCM = newVal;

        // If we've gotten a GCM id and haven't registered it, do so!
        if ( working && !s_curType.equals( DevIDType.ID_TYPE_ANDROID_FCM ) ) {
            s_regStartTime = 0;      // so we're sure to register
            devIDChanged();
            timerFired( context );
        }
    }

    public static long getLastFCMMillis()
    {
        Log.d( TAG, "getLastFCMMillis() => %d", s_lastFCM );
        return s_lastFCM;
    }

    public static void enabledChanged( Context context ) {
        boolean enabled = XWPrefs.getRelayEnabled( context );
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
        Intent intent = getIntentTo( context, MsgCmds.UDP_CHANGED );
        enqueueWork( context, intent );
    }

    private static void enqueueWork( Context context, Intent intent )
    {
        enqueueWork( context, RelayService.class, intent );
        // Log.d( TAG, "called enqueueWork(cmd=%s)", cmdFrom( intent, MsgCmds.values() ) );
    }

    private static void stopService( Context context )
    {
        Intent intent = getIntentTo( context, MsgCmds.STOP );
        enqueueWork( context, intent );
    }

    public static void inviteRemote( Context context, XwJNI.GamePtr gamePtr,
                                     int destDevID, String relayID, NetLaunchInfo nli )
    {
        if ( XwJNI.comms_getAddrDisabled( gamePtr, CommsConnType.COMMS_CONN_RELAY,
                                          true ) ) {
            Log.d( TAG, "inviteRemote() dropping because relay sends disabled" );
        } else {
            int myDevID = DevID.getRelayDevIDInt( context );
            if ( 0 != myDevID ) {
                enqueueWork( context, getIntentTo( context, MsgCmds.INVITE )
                             .putExtra( DEV_ID_SRC, myDevID )
                             .putExtra( DEV_ID_DEST, destDevID )
                             .putExtra( RELAY_ID, relayID )
                             .putExtra( NLI_DATA, nli.toString() ) );
            }
        }
    }

    public static void reset( Context context )
    {
        Intent intent = getIntentTo( context, MsgCmds.RESET );
        enqueueWork( context, intent );
    }

    private static void timerFired( Context context )
    {
        Intent intent = getIntentTo( context, MsgCmds.TIMER_FIRED );
        enqueueWork( context, intent );
    }

    public static int sendPacket( Context context, long rowid, byte[] msg,
                                  String msgID )
    {
        Log.d( TAG, "sendPacket(len=%d, msgID=%s)", msg.length, msgID );
        int result = -1;
        if ( NetStateCache.netAvail( context ) ) {
            Intent intent = getIntentTo( context, MsgCmds.SEND )
                .putExtra( ROWID, rowid )
                .putExtra( BINBUFFER, msg );
            enqueueWork( context, intent );
            result = msg.length;
        } else {
            Log.w( TAG, "sendPacket: network down" );
        }
        return result;
    }

    public static int sendNoConnPacket( Context context, long rowid, String relayID,
                                        byte[] msg, String msgNo )
    {
        int result = -1;
        if ( NetStateCache.netAvail( context ) ) {
            Intent intent = getIntentTo( context, MsgCmds.SENDNOCONN )
                .putExtra( ROWID, rowid )
                .putExtra( RELAY_ID, relayID )
                .putExtra( BINBUFFER, msg )
                .putExtra( MSGNUM, msgNo ); // not used yet
            enqueueWork( context, intent );
            result = msg.length;
        }
        return result;
    }

    private static void devIDChanged()
    {
        s_registered = false;
    }

    private void receiveInvitation( int srcDevID, NetLaunchInfo nli )
    {
        Log.d( TAG, "receiveInvitation: got nli from %d: %s", srcDevID,
               nli.toString() );
        if ( !mHelper.handleInvitation( nli, null,
                                        DictFetchOwner.OWNER_RELAY ) ) {
            Log.d( TAG, "handleInvitation() failed" );
        }
    }

    // Exists to get incoming data onto the main thread
    private static void postData( Context context, long rowid, byte[] msg )
    {
        Log.d( TAG, "postData(): packet of length %d for token (rowid) %d",
               msg.length, rowid );
        if ( DBUtils.haveGame( context, rowid ) ) {
            Intent intent = getIntentTo( context, MsgCmds.RECEIVE )
                .putExtra( ROWID, rowid )
                .putExtra( BINBUFFER, msg );
            enqueueWork( context, intent );
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
        enqueueWork( context, intent );
    }

    public static void processDevMsgs( Context context, String[] msgs64 )
    {
        Intent intent = getIntentTo( context, MsgCmds.PROCESS_DEV_MSGS )
            .putExtra( MSGS_ARR, msgs64 );
        enqueueWork( context, intent );
    }

    private static Intent getIntentTo( Context context, MsgCmds cmd )
    {
        return getIntentTo( context, RelayService.class, cmd );
    }

    @Override
    public void onCreate()
    {
        // Log.d( TAG, "%s.onCreate()", this );
        super.onCreate();

        mHelper = new RelayServiceHelper( this );
        m_lastGamePacketReceived =
            XWPrefs.getPrefsLong( this, R.string.key_last_packet,
                                  Utils.getCurSeconds() );

        m_handler = new Handler();
        m_onInactivity = new Runnable() {
                @Override
                public void run() {
                    Log.d( TAG, "%H.m_onInactivity fired", this );
                    if ( !shouldMaintainConnection() ) {
                        NetStateCache.unregister( RelayService.this,
                                                  RelayService.this );
                        stopSelf();
                    } else {
                        timerFired( RelayService.this );
                    }
                }
            };
        mReadThread = startUDPReadThreadOnce();
        if ( null == mReadThread ) {
            stopSelf();
        }
        sSkipUPDSet = XWPrefs.getSkipToWebAPI( this );
    }

    @Override
    void onHandleWorkImpl( Intent intent, XWJICmds jicmd, long timestamp )
    {
        DbgUtils.assertOnUIThread( false );
        // Log.d( TAG, "%s.onHandleWork(cmd=%s)", this, cmdFrom( intent ) );

        try {
            connectSocketOnce();    // must not be on UI thread

            handleCommand( intent, jicmd, timestamp );

            boolean goOn = serviceQueue();
            if ( !goOn ) {
                Log.e( TAG, "onHandleWork(): need to exit... HELP!!!!" );
                mReadThread.interrupt();
            }
        } catch (InterruptedException ie ) {
            Log.e( TAG, "InterruptedException in onHandleWork(): %s",
                   ie.getMessage() );
        }

        resetExitTimer();
        // Log.d( TAG, "%s.onHandleWork(cmd=%s) DONE", this, cmdFrom( intent ) );
    }

    @Override
    public void onDestroy()
    {
        // Log.d( TAG, "onDestroy() called" );

        if ( null != mReadThread ) {
            mReadThread.unsetService();
            if ( 0 < s_queue.size() ) {
                enqueueWork( getIntentTo( this, MsgCmds.DO_WORK ) );
            }
        }

        super.onDestroy();
        // Log.d( TAG, "%s.onDestroy() DONE", this );
    }

    @Override
    XWJICmds[] getCmds() { return MsgCmds.values(); }
    
    // NetStateCache.StateChangedIf interface
    @Override
    public void onNetAvail( boolean nowAvailable )
    {
        startService( this ); // bad name: will *stop* threads too
    }

    private void handleCommand( Intent intent, XWJICmds jicmd, long timestamp )
    {
        MsgCmds cmd = (MsgCmds)jicmd;
        switch( cmd ) {
        case DO_WORK:       // exists only to launch service
            break;
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
                    gotPacket( msg, true, false, timestamp );
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
                = NetLaunchInfo.makeFrom( this, intent.getStringExtra(NLI_DATA) );
            receiveInvitation( srcDevID, nli );
            break;
        case GOT_PACKET:
            byte[] msg = intent.getByteArrayExtra( BINBUFFER );
            gotPacket( msg, false, true );
            break;
        case SEND:
        case RECEIVE:
        case SENDNOCONN:
            startUDPReadThreadOnce();
            long rowid = intent.getLongExtra( ROWID, -1 );
            msg = intent.getByteArrayExtra( BINBUFFER );
            if ( MsgCmds.SEND == cmd ) {
                sendMessage( rowid, msg, timestamp );
            } else if ( MsgCmds.SENDNOCONN == cmd ) {
                String relayID = intent.getStringExtra( RELAY_ID );
                String msgNo = intent.getStringExtra( MSGNUM );
                sendNoConnMessage( rowid, relayID, msg, msgNo, timestamp );
            } else {
                mHelper.receiveMessage( rowid, null, msg, s_addr );
            }
            break;
        case INVITE:
            startUDPReadThreadOnce();
            srcDevID = intent.getIntExtra( DEV_ID_SRC, 0 );
            int destDevID = intent.getIntExtra( DEV_ID_DEST, 0 );
            String relayID = intent.getStringExtra( RELAY_ID );
            String nliData = intent.getStringExtra( NLI_DATA );
            sendInvitation( srcDevID, destDevID, relayID, nliData, timestamp );
            break;
        case TIMER_FIRED:
            if ( !NetStateCache.netAvail( this ) ) {
                Log.w( TAG, "not connecting: no network" );
            } else if ( startFetchThreadIfNotUDP() ) {
                // do nothing
            } else if ( registerWithRelayIfNot( timestamp ) ) {
                requestMessages( timestamp );
            }
            break;
        case STOP:
            stopThreads();
            stopSelf();
            break;
        default:
            Assert.failDbg();
        }
    }

    private DatagramSocket connectSocketOnce() throws InterruptedException
    {
        DatagramSocket result = null;
        synchronized ( RelayService.class ) {
            if ( null != s_UDPSocket && ! s_UDPSocket.isConnected() ) {
                closeUDPSocket( s_UDPSocket );
            }

            if ( null == s_UDPSocket ) {
                final RelayService service = this;
                int port = XWPrefs.getDefaultRelayPort( service );
                String host = XWPrefs.getDefaultRelayHost( service );

                try {
                    DatagramSocket udpSocket = new DatagramSocket();
                    udpSocket.setSoTimeout(30 * 1000); // timeout so we can log

                    InetAddress addr = InetAddress.getByName( host );
                    udpSocket.connect( addr, port ); // remember this address
                    Log.d( TAG, "connectSocket(%s:%d): udpSocket now %H",
                           host, port, udpSocket );
                    s_UDPSocket = udpSocket;
                } catch( SocketException se ) {
                    Log.ex( TAG, se );
                    Assert.failDbg();
                } catch( java.net.UnknownHostException uhe ) {
                    Log.ex( TAG, uhe );
                    Log.e( TAG, "connectSocketOnce(): %s", uhe.getMessage() );
                    // Assert.assertFalse( BuildConfig.DEBUG );
                }
            }
            result = s_UDPSocket;
        }
        return result;
    }

    private boolean serviceQueue()
    {
        boolean shouldGoOn = true;
        List<PacketData> dataListUDP = new ArrayList<>();
        List<PacketData> dataListWeb = new ArrayList<>();
        PacketData outData;
        try {
            long ts = s_packetsSentUDP.size() > 0 ? 10 : 1000;
            // Log.d( TAG, "blocking %dms on poll()", ts );
            for ( outData = s_queue.poll( ts, TimeUnit.MILLISECONDS );
                  null != outData;
                  outData = s_queue.poll() ) {         // doesn't block
                // Log.d( TAG, "removed packet from queue (%d left): %s",
                //        s_queue.size(), outData );
                if ( outData == sEOQPacket ) {
                    shouldGoOn = false;
                    break;
                } else if ( skipNativeSend() || outData.getForWeb() ) {
                    dataListWeb.add( outData );
                } else {
                    dataListUDP.add( outData );
                }
            }

            sendViaWeb( dataListWeb );
            sendViaUDP( dataListUDP );

            resetExitTimer();

            runUDPAckTimer();

            ConnStatusHandler.showSuccessOut();
        } catch ( InterruptedException ie ) {
            Log.w( TAG, "write thread killed" );
            shouldGoOn = false;
        }
        return shouldGoOn;
    }

    private long m_lastRunMS = 0;
    private void runUDPAckTimer()
    {
        long nowMS = System.currentTimeMillis();
        if ( 0 == m_lastRunMS ) {
            m_lastRunMS = nowMS;
        }
        long age = nowMS - m_lastRunMS;
        if ( age < 3000 ) { // never more frequently than 3 sec.
            // Log.d( TAG, "runUDPAckTimer(): too soon, so skipping" );
        } else {
            m_lastRunMS = nowMS;

            long minSentMS = nowMS - 10000; // 10 seconds ago
            long prevSentMS = 0;
            List<PacketData> forResend = new ArrayList<>();
            boolean foundNonAck = false;
            synchronized ( s_packetsSentUDP ) {
                for ( Iterator<PacketData> iter = s_packetsSentUDP.iterator();
                      iter.hasNext(); ) {
                    PacketData packet = iter.next();
                    long sentMS = packet.getSentMS();
                    if ( prevSentMS > sentMS ) {
                        Log.e( TAG, "error: prevSentMS: %d > sentMS: %d", prevSentMS, sentMS );
                        Assert.failDbg();
                        continue;
                    }

                    prevSentMS = sentMS;
                    if ( sentMS > minSentMS ) {
                        break;
                    }

                    forResend.add( packet );
                    if ( packet.m_cmd != XWRelayReg.XWPDEV_ACK ) {
                        foundNonAck = true;
                        sNativeFailScore.incrementAndGet();
                    }
                    iter.remove();
                }
                Log.d( TAG, "runUDPAckTimer(): %d too-new packets remaining",
                       s_packetsSentUDP.size() );
            }
            if ( foundNonAck ) {
                Log.d( TAG, "runUDPAckTimer(): reposting %d packets",
                       forResend.size() );
                s_queue.addAll( forResend );
            }
        }
    }

    private int sendViaWeb( List<PacketData> packets ) throws InterruptedException
    {
        int sentLen = 0;
        if ( packets.size() > 0 ) {
            Log.d( TAG, "sendViaWeb(): sending %d at once", packets.size() );

            final RelayService service = this;
            HttpsURLConnection conn = NetUtils
                .makeHttpsRelayConn( service, "post" );
            if ( null == conn ) {
                Log.e( TAG, "sendViaWeb(): null conn for POST" );
            } else {
                try {
                    JSONArray dataArray = new JSONArray();
                    for ( PacketData packet : packets ) {
                        Assert.assertFalse( packet == sEOQPacket );
                        byte[] datum = packet.assemble();
                        dataArray.put( Utils.base64Encode(datum) );
                        sentLen += datum.length;
                    }
                    JSONObject params = new JSONObject();
                    params.put( "data", dataArray );

                    String result = NetUtils.runConn( conn, params );
                    boolean succeeded = null != result;
                    if ( succeeded ) {
                        Log.d( TAG, "sendViaWeb(): POST(%s) => %s", params, result );
                        JSONObject resultObj = new JSONObject( result );
                        JSONArray resData = resultObj.getJSONArray( "data" );
                        int nReplies = resData.length();
                        // Log.d( TAG, "sendViaWeb(): got %d replies", nReplies );

                        service
                            .noteSent( packets, false ); // before we process the acks below :-)

                        for ( int ii = 0; ii < nReplies; ++ii ) {
                            byte[] datum = Utils.base64Decode( resData.getString( ii ) );
                            // PENDING: skip ack or not
                            service.gotPacket( datum, false, false );
                        }
                    } else {
                        Log.e( TAG, "sendViaWeb(): failed result for POST" );

                    }

                    ConnStatusHandler.updateStatus( service, null,
                                                    CommsConnType.COMMS_CONN_RELAY,
                                                    succeeded );
                } catch ( JSONException ex ) {
                    // this will happen if e.g. there's no 'data' in the result
                    Log.ex( TAG, ex );
                    sentLen = 0;
                }
            }
        }
        return sentLen;
    }

    private int sendViaUDP( List<PacketData> packets ) throws InterruptedException
    {
        int sentLen = 0;

        DatagramSocket udpSocket = s_UDPSocket;
        if ( null != udpSocket && packets.size() > 0 ) {
            boolean skipBackoffReset = true;
            // Log.d( TAG, "sendViaUDP(): sending %d at once", packets.size() );
            final RelayService service = this;
            service.noteSent( packets, true );
            for ( PacketData packet : packets ) {
                boolean breakNow = true;
                byte[] data = packet.assemble();
                try {
                    DatagramPacket udpPacket = new DatagramPacket( data, data.length );
                    udpSocket.send( udpPacket );

                    sentLen += udpPacket.getLength();
                    packet.setSentMS(); // this was commented out. Why?
                    breakNow = false;
                } catch ( IOException ex ) {
                    Log.e( TAG, "fail sending to %s", udpSocket );
                    Log.ex( TAG, ex );
                    Log.i( TAG, "Restarting threads to force new socket" );
                    ConnStatusHandler
                        .updateStatusOut( service, CommsConnType.COMMS_CONN_RELAY,
                                          true );
                    closeUDPSocket( udpSocket );

                    service.m_handler.post( new Runnable() {
                            @Override
                            public void run() {
                                service.stopUDPReadThread();
                            }
                        } );
                } catch ( NullPointerException npe ) {
                    Log.w( TAG, "network problem; dropping packet" );
                }
                skipBackoffReset = skipBackoffReset && packet.mSkipBackoffReset;
                if ( breakNow ) {
                    break;
                }
            }

            ConnStatusHandler.updateStatus( service, null,
                                            CommsConnType.COMMS_CONN_RELAY,
                                            sentLen > 0 );
            Log.d( TAG, "%s.sendViaUDP(): sent %d bytes (%d packets)",
                   this, sentLen, packets.size() );
            if ( ! skipBackoffReset ) {
                TimerReceiver.setBackoff( this, sTimerCallbacks, MIN_BACKOFF );
            }
        }

        return sentLen;
    }

    private void setupNotifications( String[] relayIDs, BackMoveResult[] bmrs,
                                     ArrayList<Boolean> locals )
    {
        for ( int ii = 0; ii < relayIDs.length; ++ii ) {
            String relayID = relayIDs[ii];
            BackMoveResult bmr = bmrs[ii];
            long[] rowids = DBUtils.getRowIDsFor( this, relayID );
            for ( long rowid : rowids ) {
                GameUtils.postMoveNotification( this, rowid, bmr,
                                                locals.get(ii) );
            }
        }
    }

    private boolean startFetchThreadIfNotUDP()
    {
        boolean handled = XWPrefs.getRelayEnabled( this ) && !BuildConfig.UDP_ENABLED;
        if ( handled && null == m_fetchThread ) {
            Assert.failDbg(); // NOT using this now!

            m_fetchThread = new Thread( null, new Runnable() {
                    @Override
                    public void run() {
                        fetchAndProcess();
                        m_fetchThread = null;
                        RelayService.this.stopSelf();
                    }
                }, getClass().getName() );
            m_fetchThread.start();
        }
        // Log.d( TAG, "startFetchThreadIfNotUDP() => %b", handled );
        return handled;
    }

    private void stopFetchThreadIf()
    {
        while ( null != m_fetchThread ) {
            Log.w( TAG, "2: m_fetchThread NOT NULL; WHAT TO DO???" );
            Assert.failDbg();
            try {
                Thread.sleep( 20 );
            } catch( java.lang.InterruptedException ie ) {
                Log.ex( TAG, ie );
            }
        }
    }

    private UDPReadThread startUDPReadThreadOnce()
    {
        UDPReadThread thread = null;
        if ( BuildConfig.UDP_ENABLED && XWPrefs.getRelayEnabled( this ) ) {
            synchronized ( sUDPReadThreadRef ) {
                thread = sUDPReadThreadRef.get();
                if ( null == thread ) {
                    thread = new UDPReadThread( this );
                    sUDPReadThreadRef.set( thread );
                    thread.start();
                } else if ( null != s_UDPSocket ) {
                    thread.setService( this );
                } else {
                    Log.e( TAG, "startUDPReadThreadOnce(): s_UDPSocket NULL" );
                    stopUDPReadThread();
                    thread = null;
                }
            }
        } else {
            Log.i( TAG, "startUDPReadThreadOnce(): UDP disabled" );
        }
        return thread;
    } // startUDPReadThreadOnce

    private void stopUDPReadThread()
    {
        synchronized ( sUDPReadThreadRef ) {
            UDPReadThread thread = sUDPReadThreadRef.getAndSet( null );
            if ( null != thread ) {
                thread.interrupt();
            }
        }
    }

    private static boolean skipNativeSend()
    {
        boolean skip = sNativeFailScore.get() > UDP_FAIL_LIMIT || sSkipUPDSet;
        // Log.d( TAG, "skipNativeSend(score=%d)) => %b", sNativeFailScore.get(), skip );
        return skip;
    }

    // So it's a map. The timer iterates over the whole map, which should
    // never be *that* big, and pulls everything older than 10 seconds. If
    // anything in that list isn't an ACK (since ACKs will always be there
    // because they're not ACK'd) then the whole thing gets resent.
    
    private void noteSent( PacketData packet, boolean fromUDP )
    {
        // Log.d( TAG, "noteSent(packet=%s, fromUDP=%b)", packet, fromUDP );
        if ( fromUDP ) {
            packet.setSentMS();
        }
        if ( fromUDP || packet.m_cmd != XWRelayReg.XWPDEV_ACK ) {
            List<PacketData> list = fromUDP ? s_packetsSentUDP : s_packetsSentWeb;
            synchronized( list ) {
                list.add(packet );
            }
        }
    }

    private void noteSent( List<PacketData> packets, boolean fromUDP )
    {
        List<PacketData> map = fromUDP ? s_packetsSentUDP : s_packetsSentWeb;
        // Log.d( TAG, "noteSent(fromUDP=%b): adding %d; size before: %d",
        //        fromUDP, packets.size(), map.size() );
        for ( PacketData packet : packets ) {
            noteSent( packet, fromUDP );
        }
        // Log.d( TAG, "noteSent(fromUDP=%b): size after: %d", fromUDP, map.size() );
    }

    // MIGHT BE Running on reader thread
    private void gotPacket( byte[] data, boolean skipAck, boolean fromUDP,
                            long timestamp )
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
                // Log.d( TAG, "%s.gotPacket(): cmd=%s", this, header.m_cmd.toString() );
                switch ( header.m_cmd ) {
                case XWPDEV_UNAVAIL:
                    int unavail = dis.readInt();
                    Log.i( TAG, "relay unvailable for another %d seconds",
                           unavail );
                    String str = getVLIString( dis );
                    mHelper.postEvent( MultiEvent.RELAY_ALERT, str );
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
                    registerWithRelay( timestamp );
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
                    requestMessages( timestamp );
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
                    enqueueWork( intent );
                    break;
                case XWPDEV_GOTINVITE:
                    resetBackoff = true;
                    int srcDevID = dis.readInt();
                    byte[] nliData = new byte[dis.readShort()];
                    dis.readFully( nliData );
                    NetLaunchInfo nli = XwJNI.nliFromStream( nliData );
                    String asStr = nli.toString();
                    Log.d( TAG, "got invitation: %s", asStr );
                    intent = getIntentTo( this, MsgCmds.GOT_INVITE )
                        .putExtra( INVITE_FROM, srcDevID )
                        .putExtra( NLI_DATA, asStr );
                    enqueueWork( intent );
                    break;
                case XWPDEV_ACK:
                    noteAck( vli2un( dis ), fromUDP );
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
        } catch ( IOException ioe ) {
            Log.ex( TAG, ioe );
        }

        if ( resetBackoff ) {
            resetBackoffTimer();
        }
    } // gotPacket()

    private void enqueueWork( Intent intent )
    {
        enqueueWork( this, intent );
    }

    private void gotPacket( byte[] data, boolean skipAck, boolean fromUDP )
    {
        gotPacket( data, skipAck, fromUDP, -1 );
    }

    private void gotPacket( DatagramPacket packet )
    {
        ConnStatusHandler.showSuccessIn();
        TimerReceiver.setBackoff( this, sTimerCallbacks, MIN_BACKOFF );

        int packetLen = packet.getLength();
        byte[] data = new byte[packetLen];
        System.arraycopy( packet.getData(), 0, data, 0, packetLen );
        // DbgUtils.logf( "RelayService::gotPacket: %d bytes of data", packetLen );
        gotPacket( data, false, true );
    } // gotPacket

    private boolean shouldRegister()
    {
        boolean should = XWPrefs.getRelayEnabled( this );
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
    private void registerWithRelay( long timestamp )
    {
        long now = Utils.getCurSeconds();
        long interval = now - s_regStartTime;
        if ( interval < REG_WAIT_INTERVAL ) {
            Log.i( TAG, "registerWithRelay(): skipping because only %d "
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

                Log.d( TAG, "registerWithRelay(): registering devID \"%s\" (type=%s)",
                       devid, typ.toString() );

                out.writeShort( BuildConfig.CLIENT_VERS_RELAY );
                writeVLIString( out, BuildConfig.GIT_REV );
                writeVLIString( out, Build.MODEL );
                writeVLIString( out, Build.VERSION.RELEASE );
                out.writeShort( BuildConfig.VARIANT_CODE );

                postPacket( bas, XWRelayReg.XWPDEV_REG, timestamp, true );
                s_regStartTime = now;
            } catch ( IOException ioe ) {
                Log.ex( TAG, ioe );
            }
        }
    }

    private boolean registerWithRelayIfNot( long timestamp )
    {
        if ( !s_registered && shouldRegister() ) {
            registerWithRelay( timestamp );
        }
        return s_registered;
    }

    private void requestMessages( long timestamp )
    {
        try {
            DevIDType[] typp = new DevIDType[1];
            String devid = getDevID( typp );
            if ( null != devid ) {
                ByteArrayOutputStream bas = new ByteArrayOutputStream();
                DataOutputStream out = new DataOutputStream( bas );
                writeVLIString( out, devid );
                postPacket( bas, XWRelayReg.XWPDEV_RQSTMSGS, timestamp, true );
            } else {
                Log.d(TAG, "requestMessages(): devid is null" );
            }
        } catch ( IOException ioe ) {
            Log.ex( TAG, ioe );
        }
    }

    private void sendMessage( long rowid, byte[] msg, long timestamp )
    {
        ByteArrayOutputStream bas = new ByteArrayOutputStream();
        try {
            DataOutputStream out = new DataOutputStream( bas );
            Assert.assertTrue( rowid < Integer.MAX_VALUE );
            out.writeInt( (int)rowid );
            out.write( msg, 0, msg.length );
            postPacket( bas, XWRelayReg.XWPDEV_MSG, timestamp );
        } catch ( IOException ioe ) {
            Log.ex( TAG, ioe );
        }
    }

    private void sendNoConnMessage( long rowid, String relayID,
                                    byte[] msg, String msgNo, // not used yet
                                    long timestamp )
    {
        Log.d( TAG, "sendNoConnMessage(rowid=%d, msgNo=%s, len=%d)", rowid,
               msgNo, msg.length );
        ByteArrayOutputStream bas = new ByteArrayOutputStream();
        try {
            DataOutputStream out = new DataOutputStream( bas );
            Assert.assertTrue( rowid < Integer.MAX_VALUE ); // ???
            out.writeInt( (int)rowid );
            out.writeBytes( relayID );
            out.write( '\n' );
            out.write( msg, 0, msg.length );
            postPacket( bas, XWRelayReg.XWPDEV_MSGNOCONN, timestamp );
        } catch ( IOException ioe ) {
            Log.ex( TAG, ioe );
        }
    }

    private void sendInvitation( int srcDevID, int destDevID, String relayID,
                                 String nliStr, long timestamp )
    {
        Log.d( TAG, "sendInvitation(%d->%d/%s [%s])", srcDevID, destDevID,
               relayID, nliStr );

        NetLaunchInfo nli = NetLaunchInfo.makeFrom( this, nliStr );
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
            postPacket( bas, XWRelayReg.XWPDEV_INVITE, timestamp );
        } catch ( IOException ioe ) {
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
                postPacket( bas, XWRelayReg.XWPDEV_ACK, -1, true );
            } catch ( IOException ioe ) {
                Log.ex( TAG, ioe );
            }
        }
    }

    private PacketHeader readHeader( DataInputStream dis )
        throws IOException
    {
        PacketHeader result = null;
        byte proto = dis.readByte();
        if ( XWPDevProto.XWPDEV_PROTO_VERSION_1.ordinal() == proto ) {
            int packetID = vli2un( dis );
            // if ( 0 != packetID ) {
            //     Log.d( TAG, "readHeader(): got packetID %d", packetID );
            // }
            byte ordinal = dis.readByte();
            XWRelayReg cmd = XWRelayReg.values()[ordinal];
            result = new PacketHeader( cmd, packetID );
        } else {
            Log.w( TAG, "bad proto: %d", proto );
        }
        return result;
    }

    private String getVLIString( DataInputStream dis ) throws IOException
    {
        byte[] tmp = new byte[vli2un( dis )];
        dis.readFully( tmp );
        String result = new String( tmp );
        return result;
    }

    private void postPacket( ByteArrayOutputStream bas, XWRelayReg cmd,
                             long timestamp )
    {
        postPacket( bas, cmd, timestamp, false );
    }

    private void postPacket( ByteArrayOutputStream bas, XWRelayReg cmd,
                             long timestamp, boolean skipBackoffReset )
    {
        PacketData packet = new PacketData( bas, cmd, timestamp, skipBackoffReset );
        s_queue.add( packet );
        // Log.d( TAG, "postPacket(%s); (now %d in queue)", packet,
        //        s_queue.size() );
    }

    private String getDevID( DevIDType[] typp )
    {
        DevIDType typ;
        String devid = DevID.getRelayDevID( this, true );

        if ( null != devid && 0 < devid.length() ) {
            typ = DevIDType.ID_TYPE_RELAY;
        } else {
            devid = FBMService.getFCMDevID( this );
            if ( null != devid && 0 < devid.length() ) {
                typ = DevIDType.ID_TYPE_ANDROID_FCM;
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
        Assert.failDbg();
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
            ArrayList<String> idsWMsgs = new ArrayList<>( nameCount );
            ArrayList<Boolean> isLocals = new ArrayList<>( nameCount );
            ArrayList<BackMoveResult> bmrs = new ArrayList<>( nameCount );

            boolean[] isLocalP = new boolean[1];
            for ( int ii = 0; ii < nameCount; ++ii ) {
                byte[][] forOne = msgs[ii];
                if ( null != forOne ) {
                    long rowid = rowIDs[ii];
                    sink.setRowID( rowid );
                    for ( byte[] msg : forOne ) {
                        mHelper.receiveMessage( rowid, sink, msg, s_addr );
                    }
                }
            }
            sink.send( this );
        }
    }

    private static class UDPReadThread extends Thread {
        private RelayService[] mServiceHolder = {null};

        UDPReadThread( RelayService service ) {
            super( "UDPReadThread" );
            setService( service );
        }

        void setService( RelayService service )
        {
            Assert.assertNotNull( service );
            synchronized ( mServiceHolder ) {
                mServiceHolder[0] = service;
                // unblock waiters for non-null Service
                mServiceHolder.notifyAll();
            }
        }

        // It will be a few milliseconds before all threads using the current
        // Service instance are done. Blocking the UI thread here until that
        // happened make for a laggy UI, so I'm going to see if we can
        // continue to use the instance for a short while after returning.
        void unsetService()
        {
            // RelayService oldService;
            synchronized ( mServiceHolder ) {
                // oldService = mServiceHolder[0];
                mServiceHolder[0] = null;
            }
            // Log.d( TAG, "unsetService() DONE (was %s)", oldService );
        }

        private RelayService getService() throws InterruptedException
        {
            synchronized ( mServiceHolder ) {
                long startMS = System.currentTimeMillis();
                while ( null == mServiceHolder[0] ) {
                    mServiceHolder.wait();
                }
                long tookMS = System.currentTimeMillis() - startMS;
                if ( tookMS > 10 ) {
                    Log.d( TAG, "getService(): blocked for %s ms", tookMS );
                }
                return mServiceHolder[0];
            }
        }

        @Override
        public void run() {
            Log.i( TAG, "%s.run() starting", this );
            Context context = XWApp.getContext();
            try {
                DatagramSocket udpSocket = s_UDPSocket;
                if ( null == udpSocket ) {
                    // will be null if e.g. device or emulator doesn't have network
                    udpSocket = getService().connectSocketOnce(); // block until this is done
                    // Assert.assertTrue( null != udpSocket || !BuildConfig.DEBUG ); // firing
                    if ( null == udpSocket ) {
                        Log.e( TAG, "connectSocketOnce() failed; no socket" );
                    }
                }

                byte[] buf = new byte[1024];
                DatagramPacket packet = new DatagramPacket( buf, buf.length );
                while ( null != udpSocket ) {
                    if ( interrupted() ) {
                        Log.d( TAG, "%s.run() interrupted; outta here", this );
                        break;
                    }
                    try {
                        udpSocket.receive( packet );
                        postGotPacket( context, packet );
                        // final RelayService service = getService();
                        // service.resetExitTimer();
                        // service.gotPacket( packet );
                    } catch ( java.io.InterruptedIOException iioe ) {
                        // poll timing out, typically
                        // Log.d( TAG, "iioe from receive(): %s", iioe.getMessage() );
                    } catch( IOException ioe ) {
                        Log.d( TAG, "ioe from receive(): %s", ioe.getMessage() );
                        closeUDPSocket( udpSocket );
                        break;
                    }
                }

            } catch ( InterruptedException ie ) {
                Log.d( TAG, "exiting on interrupt: %s",
                       ie.getMessage() );
            }
            Log.i( TAG, "%s.run() exiting", this );
        }

        private void postGotPacket( Context context, DatagramPacket packet )
        {
            int packetLen = packet.getLength();
            byte[] data = new byte[packetLen];
            System.arraycopy( packet.getData(), 0, data, 0, packetLen );

            Intent intent = getIntentTo( context, MsgCmds.GOT_PACKET )
                .putExtra( BINBUFFER, data );
            enqueueWork( context, intent );
        }
    }

    private static void closeUDPSocket( DatagramSocket udpSocket )
    {
        synchronized ( RelayService.class ) {
            if ( udpSocket == s_UDPSocket ) {
                s_UDPSocket.close();
                s_UDPSocket = null;
            }
        }
    }

    private static class AsyncSender extends Thread {
        private Context m_context;
        private HashMap<String,ArrayList<byte[]>> m_msgHash;

        AsyncSender( Context context, HashMap<String, ArrayList<byte[]>> msgHash )
        {
            m_context = context;
            m_msgHash = msgHash;
        }

        @Override
        public void run()
        {
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
                        Assert.failDbg();
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
            } catch ( IOException ioe ) {
                Log.ex( TAG, ioe );
            }
        } // run
    }

    private static void sendToRelay( Context context,
                                     HashMap<String,ArrayList<byte[]>> msgHash )
    {
        if ( null != msgHash ) {
            new AsyncSender( context, msgHash ).start();
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
        public int sendViaRelay( byte[] buf, String msgID, int gameID )
        {
            Assert.assertTrue( -1 != getRowID() );
            sendPacket( RelayService.this, getRowID(), buf, msgID );
            return buf.length;
        }

        @Override
        public boolean relayNoConnProc( byte[] buf, String msgNo, String relayID )
        {
            long rowID = getRowID();
            if ( -1 != rowID ) {
                sendNoConnMessage( rowID, relayID, buf, msgNo, -1 );
            } else {
                if ( null == m_msgLists ) {
                    m_msgLists = new HashMap<String,ArrayList<byte[]>>();
                }

                ArrayList<byte[]> list = m_msgLists.get( relayID );
                if ( list == null ) {
                    list = new ArrayList<>();
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

    private void noteAck( int packetID, boolean fromUDP )
    {
        Assert.assertTrue( packetID != 0 );
        List<PacketData> map = fromUDP ? s_packetsSentUDP : s_packetsSentWeb;
        synchronized( map ) {
            PacketData packet = null;
            Iterator<PacketData> iter = map.iterator();
            for ( iter = map.iterator(); iter.hasNext(); ) {
                PacketData next = iter.next();
                if ( next.m_packetID == packetID ) {
                    packet = next;
                    iter.remove();
                    break;
                }
            }

            if ( packet != null ) {
                // Log.d( TAG, "noteAck(fromUDP=%b): removed for id %d: %s",
                //        fromUDP, packetID, packet );
                if ( fromUDP ) {
                    sNativeFailScore.decrementAndGet();
                }
            } else {
                Log.w( TAG, "Weird: got ack %d but never sent", packetID );
            }

            if ( false && BuildConfig.DEBUG ) {
                ArrayList<String> pstrs = new ArrayList<>();
                for ( PacketData datum : map ) {
                    if ( 0 != datum.m_packetID ) {
                        pstrs.add( String.format("%d", datum.m_packetID ) );
                    }
                }
                Log.d( TAG, "noteAck(fromUDP=%b): Got ack for %d; there are %d unacked packets: %s",
                       fromUDP, packetID, pstrs.size(), TextUtils.join( ",", pstrs ) );
            }
        }

        // If we get an ACK, things are working, even if it's not found above
        // (which would be the case for an ACK sent via web, which we don't
        // save.)
        ConnStatusHandler.updateStatus( this, null,
                                        CommsConnType.COMMS_CONN_RELAY,
                                        true );
    }

    // Called from any thread
    private void resetExitTimer()
    {
        // Log.d( TAG, "resetExitTimer()" );
        m_handler.removeCallbacks( m_onInactivity );

        // UDP socket's no good as a return address after several
        // minutes of inactivity, so do something after that time.
        m_handler.postDelayed( m_onInactivity,
                               getMaxIntervalSeconds() * 1000 );
    }

    private void startThreads()
    {
        Log.d( TAG, "startThreads()" );
        if ( !XWPrefs.getRelayEnabled( this ) || !NetStateCache.netAvail( this ) ) {
            stopThreads();
        } else if ( BuildConfig.UDP_ENABLED ) {
            stopFetchThreadIf();
            startUDPReadThreadOnce();
            registerWithRelay( -1 );
        } else {
            Assert.failDbg();
            stopUDPReadThread();
            startFetchThreadIfNotUDP();
        }
    }

    private void stopThreads()
    {
        Log.d( TAG, "stopThreads()" );
        stopFetchThreadIf();
        stopUDPReadThread();
    }

    private static void un2vli( int nn, OutputStream os )
        throws IOException
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

    private static int vli2un( InputStream is ) throws IOException
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
        throws IOException
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

        // WFT? went from 40 to 1000
        // Log.d( TAG, "getMaxIntervalSeconds() => %d", result );
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
     * notice, and use TimerReceiver's timer to get it restarted a bit
     * later.  But note: s_lastFCM will not be set when the app is
     * relaunched.
     */

    private boolean shouldMaintainConnection()
    {
        boolean result = XWPrefs.getRelayEnabled( this )
            && (0 == s_lastFCM || XWPrefs.getIgnoreFCM( this ));

        if ( result ) {
            long interval = Utils.getCurSeconds() - m_lastGamePacketReceived;
            result = interval < MAX_KEEPALIVE_SECS;
        }
        // Log.d( TAG, "shouldMaintainConnection=>%b", result );
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
        // Log.d( TAG, "figureBackoffSeconds() => %d", result );
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

    private static class PacketData {
        public ByteArrayOutputStream m_bas;
        public XWRelayReg m_cmd;
        public byte[] m_header;
        public int m_packetID;
        private long m_requested; // when the request came into the static API
        private long m_created;   // when this packet was created (to service request)
        private long m_sentUDP;
        private boolean mSkipBackoffReset;

        private PacketData() {}

        public PacketData( ByteArrayOutputStream bas, XWRelayReg cmd,
                           long requestTS, boolean skipBackoffReset )
        {
            m_bas = bas;
            m_cmd = cmd;
            m_requested = requestTS;
            m_created = System.currentTimeMillis();
            mSkipBackoffReset = skipBackoffReset;

            makeHeader();
        }

        @Override
        public String toString()
        {
            long now = System.currentTimeMillis();
            StringBuilder sb = new StringBuilder()
                .append( "{cmd: " )
                .append( m_cmd )
                .append( "; id: " )
                .append( m_packetID );
            if ( m_requested > 0 ) {
                sb.append( "; requestAge: " )
                    .append( now - m_requested )
                    .append( "ms" );
            }
            sb.append( "; packetAge: " )
                .append( now - m_created )
                .append( "ms}" );
            return sb.toString();
            // return String.format( "{cmd: %s; id: %d; packetAge: %d ms; requestAge: %d}",
            //                       m_cmd, m_packetID, now - m_created,  );
        }

        void setSentMS() { m_sentUDP = System.currentTimeMillis(); }
        long getSentMS() { return m_sentUDP; }
        boolean getForWeb() { return m_sentUDP != 0; }

        public int getLength()
        {
            int result = 0;
            if ( null != m_bas ) { // empty case?
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
                // Log.d( TAG, "makeHeader(): building packet with cmd %s",
                //        m_cmd.toString() );
                out.writeByte( XWPDevProto.XWPDEV_PROTO_VERSION_1.ordinal() );
                un2vli( m_packetID, out );
                out.writeByte( m_cmd.ordinal() );
                m_header = bas.toByteArray();
            } catch ( IOException ioe ) {
                Log.ex( TAG, ioe );
            }
        }
    }

    private class RelayServiceHelper extends XWServiceHelper {
        RelayServiceHelper( Context context )
        {
            super( context );
        }

        @Override
        protected MultiMsgSink getSink( long rowid )
        {
            return new RelayMsgSink().setRowID( rowid );
        }
    }
}
