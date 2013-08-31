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
import android.os.AsyncTask;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.Socket;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.concurrent.LinkedBlockingQueue;

import junit.framework.Assert;

import org.eehouse.android.xw4.MultiService.MultiEvent;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.GameSummary;
import org.eehouse.android.xw4.jni.UtilCtxt;
import org.eehouse.android.xw4.jni.UtilCtxt.DevIDType;
import org.eehouse.android.xw4.jni.XwJNI;

public class RelayService extends XWService 
    implements NetStateCache.StateChangedIf {
    private static final int MAX_SEND = 1024;
    private static final int MAX_BUF = MAX_SEND - 2;

    // One week, in seconds.  Probably should be configurable.
    private static final long MAX_KEEPALIVE_SECS = 7 * 24 * 60 * 60;

    private static final String CMD_STR = "CMD";

    // These should be enums
    private static enum MsgCmds { INVALID
            , PROCESS_GAME_MSGS
            , PROCESS_DEV_MSGS
            , UDP_CHANGED
            , SEND
            , RECEIVE
            , TIMER_FIRED
            , RESET
    }

    private static final String MSGS = "MSGS";
    private static final String RELAY_ID = "RELAY_ID";
    private static final String ROWID = "ROWID";
    private static final String BINBUFFER = "BINBUFFER";

    private static HashSet<Integer> s_packetsSent = new HashSet<Integer>();
    private static int s_nextPacketID = 1;
    private static boolean s_gcmWorking = false;

    private Thread m_fetchThread = null;
    private Thread m_UDPReadThread = null;
    private Thread m_UDPWriteThread = null;
    private DatagramSocket m_UDPSocket;
    private LinkedBlockingQueue<DatagramPacket> m_queue = 
        new LinkedBlockingQueue<DatagramPacket>();
    private Handler m_handler;
    private Runnable m_onInactivity;
    private int m_maxIntervalSeconds = 0;
    private long m_lastGamePacketReceived;

    // These must match the enum XWPDevProto in xwrelay.h
    private static enum XWPDevProto { XWPDEV_PROTO_VERSION_INVALID
            ,XWPDEV_PROTO_VERSION_1
            };

    // private static final int XWPDEV_NONE = 0;

    // Must be kept in sync with eponymous enum in xwrelay.h
    private enum XWRelayReg {
             XWPDEV_NONE
            ,XWPDEV_UNAVAIL
            ,XWPDEV_REG
            ,XWPDEV_REGRSP
            ,XWPDEV_KEEPALIVE
            ,XWPDEV_HAVEMSGS
            ,XWPDEV_RQSTMSGS
            ,XWPDEV_MSG
            ,XWPDEV_MSGNOCONN
            ,XWPDEV_MSGRSP
            ,XWPDEV_BADREG
            ,XWPDEV_ACK
            ,XWPDEV_DELGAME
            ,XWPDEV_ALERT
            };

    public static void gcmConfirmed( boolean confirmed )
    {
        s_gcmWorking = confirmed;
    }

    public static void startService( Context context )
    {
        DbgUtils.logf( "RelayService.startService()" );
        Intent intent = getIntentTo( context, MsgCmds.UDP_CHANGED );
        context.startService( intent );
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
        int result = -1;
        if ( NetStateCache.netAvail( context ) ) {
            Intent intent = getIntentTo( context, MsgCmds.SEND )
                .putExtra( ROWID, rowid )
                .putExtra( BINBUFFER, msg );
            context.startService( intent );
            result = msg.length;
        } else {
            DbgUtils.logf( "RelayService.sendPacket: network down" );
        }
        return result;
    }

    // Exists to get incoming data onto the main thread
    private static void postData( Context context, long rowid, byte[] msg )
    {
        DbgUtils.logf( "RelayService::postData: packet of length %d for token %d", 
                       msg.length, rowid );
        if ( DBUtils.haveGame( context, rowid ) ) {
            Intent intent = getIntentTo( context, MsgCmds.RECEIVE )
                .putExtra( ROWID, rowid )
                .putExtra( BINBUFFER, msg );
            context.startService( intent );
        } else {
            DbgUtils.logf( "RelayService.postData(): Dropping message for "
                           + "rowid %d: not on device", rowid );
        }
    }

    public static void udpChanged( Context context )
    {
        startService( context );
    }

    public static void processGameMsgs( Context context, String relayId, 
                                        String[] msgs64 )
    {
        DbgUtils.logf( "RelayService.processGameMsgs" );
        Intent intent = getIntentTo( context, MsgCmds.PROCESS_GAME_MSGS )
            .putExtra( MSGS, msgs64 )
            .putExtra( RELAY_ID, relayId );
        context.startService( intent );
    }

    public static void processDevMsgs( Context context, String[] msgs64 )
    {
        Intent intent = getIntentTo( context, MsgCmds.PROCESS_DEV_MSGS )
            .putExtra( MSGS, msgs64 );
        context.startService( intent );
    }

    private static Intent getIntentTo( Context context, MsgCmds cmd )
    {
        Intent intent = new Intent( context, RelayService.class );
        intent.putExtra( CMD_STR, cmd.ordinal() );
        return intent;
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
                    DbgUtils.logf( "RelayService: m_onInactivity fired" );
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
                DbgUtils.logf( "RelayService::onStartCommand: cmd=%s", 
                               cmd.toString() );
                switch( cmd ) {
                case PROCESS_GAME_MSGS:
                    String[] relayIDs = new String[1];
                    relayIDs[0] = intent.getStringExtra( RELAY_ID );
                    long[] rowIDs = DBUtils.getRowIDsFor( this, relayIDs[0] );
                    if ( 0 < rowIDs.length ) {
                        String[] msgs64 = intent.getStringArrayExtra( MSGS );
                        int count = msgs64.length;

                        byte[][][] msgs = new byte[1][count][];
                        for ( int ii = 0; ii < count; ++ii ) {
                            msgs[0][ii] = XwJNI.base64Decode( msgs64[ii] );
                        }
                        process( msgs, rowIDs, relayIDs );
                    }
                    break;
                case PROCESS_DEV_MSGS:
                    DbgUtils.logf( "dropping dev msg for now" );
                    break;
                case UDP_CHANGED:
                    startThreads();
                    break;
                case RESET:
                    stopThreads();
                    startThreads();
                    break;
                case SEND:
                case RECEIVE:
                    startUDPThreadsIfNot();
                    long rowid = intent.getLongExtra( ROWID, -1 );
                    byte[] msg = intent.getByteArrayExtra( BINBUFFER );
                    if ( MsgCmds.SEND.equals( cmd ) ) {
                        sendMessage( rowid, msg );
                    } else {
                        feedMessage( rowid, msg );
                    }
                    break;
                case TIMER_FIRED:
                    if ( !startFetchThreadIf() ) {
                        sendKeepAlive();
                    }
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
        DbgUtils.logf( "RelayService.onDestroy() called" );

        if ( shouldMaintainConnection() ) {
            long interval_millis = getMaxIntervalSeconds() * 1000;
            RelayReceiver.RestartTimer( this, interval_millis );
        }
        stopThreads();
        super.onDestroy();
    }

    // NetStateCache.StateChangedIf interface
    public void netAvail( boolean nowAvailable )
    {
        startService( this ); // bad name: will *stop* threads too
    }

    private void setupNotification( String[] relayIDs )
    {
        for ( String relayID : relayIDs ) {
            long[] rowids = DBUtils.getRowIDsFor( this, relayID );
            if ( null != rowids ) {
                for ( long rowid : rowids ) {
                    setupNotification( rowid );
                }
            }
        }
    }

    private void setupNotification( long rowid )
    {
        Intent intent = GamesList.makeRowidIntent( this, rowid );
        String msg = Utils.format( this, R.string.notify_bodyf, 
                                   GameUtils.getName( this, rowid ) );
        Utils.postNotification( this, intent, R.string.notify_title,
                                msg, (int)rowid );
    }
    
    private boolean startFetchThreadIf()
    {
        DbgUtils.logf( "startFetchThreadIf()" );
        boolean handled = !XWPrefs.getUDPEnabled( this );
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
            DbgUtils.logf( "2: m_fetchThread NOT NULL; WHAT TO DO???" );
            try {
                Thread.sleep( 20 );
            } catch( java.lang.InterruptedException ie ) {
                DbgUtils.loge( ie );
            }
        }
    }

    private void startUDPThreadsIfNot()
    {
        DbgUtils.logf( "RelayService.startUDPThreadsIfNot()" );
        if ( XWPrefs.getUDPEnabled( this ) ) {

            if ( null == m_UDPReadThread ) {
                m_UDPReadThread = new Thread( null, new Runnable() {
                        public void run() {

                            connectSocket(); // block until this is done
                            startWriteThread();

                            DbgUtils.logf( "RelayService:read thread running" );
                            byte[] buf = new byte[1024];
                            for ( ; ; ) {
                                DatagramPacket packet = 
                                    new DatagramPacket( buf, buf.length );
                                try {
                                    m_UDPSocket.receive( packet );
                                    resetExitTimer();
                                    gotPacket( packet );
                                } catch ( java.io.InterruptedIOException iioe ) {
                                    DbgUtils.logf( "FYI: udp receive timeout" );
                                } catch( java.io.IOException ioe ) {
                                    DbgUtils.loge( ioe );
                                    break; // ???
                                }
                            }
                            DbgUtils.logf( "RelayService:read thread exiting" );
                        }
                    }, getClass().getName() );
                m_UDPReadThread.start();
            } else {
                DbgUtils.logf( "m_UDPReadThread not null and assumed to "
                               + "be running" );
            }
        } else {
            DbgUtils.logf( "RelayService.startUDPThreadsIfNot(): UDP disabled" );
        }
    } // startUDPThreadsIfNot

    // Some of this must not be done on main (UI) thread
    private void connectSocket()
    {
        if ( null == m_UDPSocket ) {
            int port = XWPrefs.getDefaultRelayPort( this );
            String host = XWPrefs.getDefaultRelayHost( this );
            try { 
                m_UDPSocket = new DatagramSocket();
                m_UDPSocket.setSoTimeout(30 * 1000); // timeout so we can log
                // put on background thread!!
                InetAddress addr = InetAddress.getByName( host );
                m_UDPSocket.connect( addr, port ); // remember this address
            } catch( java.net.SocketException se ) {
                DbgUtils.loge( se );
                Assert.fail();
            } catch( java.net.UnknownHostException uhe ) {
                DbgUtils.loge( uhe );
            }
        } else {
            Assert.assertTrue( m_UDPSocket.isConnected() );
            DbgUtils.logf( "m_UDPSocket not null" );
        }
    }

    private void startWriteThread()
    {
        if ( null == m_UDPWriteThread ) {
            m_UDPWriteThread = new Thread( null, new Runnable() {
                    public void run() {
                        DbgUtils.logf( "RelayService: write thread running" );
                        for ( ; ; ) {
                            DatagramPacket outPacket;
                            try {
                                outPacket = m_queue.take();
                            } catch ( InterruptedException ie ) {
                                DbgUtils.logf( "RelayService; write thread "
                                               + "killed" );
                                break;
                            }
                            if ( null == outPacket 
                                 || 0 == outPacket.getLength() ) {
                                DbgUtils.logf( "stopping write thread" );
                                break;
                            }

                            try {
                                m_UDPSocket.send( outPacket );
                                DbgUtils.logf( "Sent udp packet of length %d", 
                                           outPacket.getLength() );
                                resetExitTimer();
                                ConnStatusHandler.showSuccessOut();
                            } catch ( java.io.IOException ioe ) {
                                DbgUtils.loge( ioe );
                            } catch ( NullPointerException npe ) {
                                DbgUtils.logf( "network problem; dropping packet" );
                            }
                        }
                        DbgUtils.logf( "RelayService: write thread exiting" );
                    }
                }, getClass().getName() );
            m_UDPWriteThread.start();
        } else {
            DbgUtils.logf( "m_UDPWriteThread not null and assumed to "
                           + "be running" );
        }
    }

    private void stopUDPThreadsIf()
    {
        DbgUtils.logf( "stopUDPThreadsIf" );
        if ( null != m_UDPWriteThread ) {
            // can't add null
            m_queue.add( new DatagramPacket( new byte[0], 0 ) );
            try {
                DbgUtils.logf( "joining m_UDPWriteThread" );
                m_UDPWriteThread.join();
                DbgUtils.logf( "SUCCESSFULLY joined m_UDPWriteThread" );
            } catch( java.lang.InterruptedException ie ) {
                DbgUtils.loge( ie );
            }
            m_UDPWriteThread = null;
            m_queue.clear();
        }
        if ( null != m_UDPSocket && null != m_UDPReadThread ) {
            m_UDPSocket.close();
            DbgUtils.logf( "waiting for read thread to exit" );
            try {
                m_UDPReadThread.join();
            } catch( java.lang.InterruptedException ie ) {
                DbgUtils.loge( ie );
            }
            DbgUtils.logf( "read thread exited" );
            m_UDPReadThread = null;
            m_UDPSocket = null;
        }
        DbgUtils.logf( "stopUDPThreadsIf DONE" );
    }

    // MIGHT BE Running on reader thread
    private void gotPacket( byte[] data, boolean skipAck )
    {
        ByteArrayInputStream bis = new ByteArrayInputStream( data );
        DataInputStream dis = new DataInputStream( bis );
        try {
            PacketHeader header = readHeader( dis );
            if ( null != header ) {
                if ( !skipAck ) {
                    sendAckIf( header );
                }
                DbgUtils.logf( "gotPacket: cmd=%s", header.m_cmd.toString() );
                switch ( header.m_cmd ) { 
                case XWPDEV_UNAVAIL:
                    int unavail = dis.readInt();
                    DbgUtils.logf( "relay unvailable for another %d seconds", 
                                   unavail );
                    String str = getVLIString( dis );
                    sendResult( MultiEvent.RELAY_ALERT, str );
                    break;
                case XWPDEV_ALERT:
                    str = getVLIString( dis );
                    Intent intent = GamesList.makeAlertIntent( this, str );
                    Utils.postNotification( this, intent, 
                                            R.string.relay_alert_title,
                                            str, str.hashCode() );
                    break;
                case XWPDEV_BADREG:
                    str = getVLIString( dis );
                    DbgUtils.logf( "bad relayID \"%s\" reported", str );
                    XWPrefs.clearRelayDevID( this );
                    registerWithRelay();
                    break;
                case XWPDEV_REGRSP:
                    str = getVLIString( dis );
                    short maxIntervalSeconds = dis.readShort();
                    DbgUtils.logf( "got relayid %s, maxInterval %d", str, 
                                   maxIntervalSeconds );
                    setMaxIntervalSeconds( maxIntervalSeconds );
                    XWPrefs.setRelayDevID( this, str );
                    break;
                case XWPDEV_HAVEMSGS:
                    requestMessages();
                    break;
                case XWPDEV_MSG:
                    int token = dis.readInt();
                    byte[] msg = new byte[dis.available()];
                    dis.read( msg );
                    postData( this, token, msg );

                    // game-related packets only count
                    long lastGamePacketReceived = Utils.getCurSeconds();
                    if ( lastGamePacketReceived != m_lastGamePacketReceived ) {
                        XWPrefs.setPrefsLong( this, R.string.key_last_packet,
                                              lastGamePacketReceived );
                        m_lastGamePacketReceived = lastGamePacketReceived;
                    }
                    break;
                case XWPDEV_ACK:
                    noteAck( vli2un( dis ) );
                    break;
                default:
                    DbgUtils.logf( "RelayService.gotPacket(): Unhandled cmd" );
                    break;
                }
            }
        } catch ( java.io.IOException ioe ) {
            DbgUtils.loge( ioe );
        }
    }

    private void gotPacket( DatagramPacket packet )
    {
        ConnStatusHandler.showSuccessIn();

        int packetLen = packet.getLength();
        byte[] data = new byte[packetLen];
        System.arraycopy( packet.getData(), 0, data, 0, packetLen );
        DbgUtils.logf( "RelayService::gotPacket: %d bytes of data", packetLen );
        gotPacket( data, false );
    } // gotPacket

    private void registerWithRelay()
    {
        DbgUtils.logf( "registerWithRelay" );
        DevIDType[] typ = new DevIDType[1];
        String devid = getDevID( typ );

        ByteArrayOutputStream bas = new ByteArrayOutputStream();
        try {
            DataOutputStream out = addProtoAndCmd( bas, XWRelayReg.XWPDEV_REG );
            out.writeByte( typ[0].ordinal() );
            writeVLIString( out, devid );
            DbgUtils.logf( "registering devID \"%s\" (type=%s)", devid, 
                           typ[0].toString() );

            out.writeShort( GitVersion.CLIENT_VERS_RELAY );
            writeVLIString( out, GitVersion.VERS );
            writeVLIString( out, Build.MODEL );

            postPacket( bas );
        } catch ( java.io.IOException ioe ) {
            DbgUtils.loge( ioe );
        }
    }

    private void requestMessagesImpl( XWRelayReg reg )
    {
        ByteArrayOutputStream bas = new ByteArrayOutputStream();
        try {
            String devid = getDevID( null );
            if ( null != devid ) {
                DataOutputStream out = addProtoAndCmd( bas, reg );
                writeVLIString( out, devid );
                postPacket( bas );
            }
        } catch ( java.io.IOException ioe ) {
            DbgUtils.loge( ioe );
        }
    }

    private void requestMessages()
    {
        requestMessagesImpl( XWRelayReg.XWPDEV_RQSTMSGS );
    }

    private void sendKeepAlive()
    {
        requestMessagesImpl( XWRelayReg.XWPDEV_KEEPALIVE );
    }

    private void sendMessage( long rowid, byte[] msg )
    {
        DbgUtils.logf( "RelayService.sendMessage()" );
        ByteArrayOutputStream bas = new ByteArrayOutputStream();
        try {
            DataOutputStream out = addProtoAndCmd( bas, XWRelayReg.XWPDEV_MSG );
            Assert.assertTrue( rowid < Integer.MAX_VALUE );
            out.writeInt( (int)rowid );
            out.write( msg, 0, msg.length );
            postPacket( bas );
        } catch ( java.io.IOException ioe ) {
            DbgUtils.loge( ioe );
        } 
    }

    private void sendNoConnMessage( long rowid, String relayID, byte[] msg )
    {
        ByteArrayOutputStream bas = new ByteArrayOutputStream();
        try {
            DataOutputStream out = 
                addProtoAndCmd( bas, XWRelayReg.XWPDEV_MSGNOCONN );
            Assert.assertTrue( rowid < Integer.MAX_VALUE );
            out.writeInt( (int)rowid );
            out.writeBytes( relayID );
            out.write( '\n' );
            out.write( msg, 0, msg.length );
            postPacket( bas );
        } catch ( java.io.IOException ioe ) {
            DbgUtils.loge( ioe );
        } 
    }

    private void sendAckIf( PacketHeader header )
    {
        DbgUtils.logf( "sendAckIf" );
        if ( 0 != header.m_packetID ) {
            ByteArrayOutputStream bas = new ByteArrayOutputStream();
            try {
                DataOutputStream out = 
                    addProtoAndCmd( bas, XWRelayReg.XWPDEV_ACK );
                un2vli( header.m_packetID, out );
                postPacket( bas );
            } catch ( java.io.IOException ioe ) {
                DbgUtils.loge( ioe );
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
                DbgUtils.logf( "readHeader: got packetID %d", packetID );
            }
            byte ordinal = dis.readByte();
            XWRelayReg cmd = XWRelayReg.values()[ordinal];
            result = new PacketHeader( cmd, packetID );
        } else {
            DbgUtils.logf( "bad proto: %d", proto );
        }
        return result;
    }

    private String getVLIString( DataInputStream dis )
        throws java.io.IOException
    {
        int len = vli2un( dis );
        byte[] tmp = new byte[len];
        dis.read( tmp );
        String result = new String( tmp );
        return result;
    }

    private DataOutputStream addProtoAndCmd( ByteArrayOutputStream bas, 
                                             XWRelayReg cmd )
        throws java.io.IOException
    {
        DataOutputStream out = new DataOutputStream( bas );
        DbgUtils.logf( "Building packet with cmd %s", cmd.toString() );
        out.writeByte( XWPDevProto.XWPDEV_PROTO_VERSION_1.ordinal() );
        un2vli( nextPacketID( cmd ), out );
        out.writeByte( cmd.ordinal() );
        return out;
    }

    private void postPacket( ByteArrayOutputStream bas )
    {
        byte[] data = bas.toByteArray();
        m_queue.add( new DatagramPacket( data, data.length ) );
        DbgUtils.logf( "postPacket() done; %d in queue", m_queue.size() );
    }

    private String getDevID( DevIDType[] typp )
    {
        DevIDType typ;
        String devid = XWPrefs.getRelayDevID( this );
        if ( null != devid && 0 < devid.length() ) {
            typ = DevIDType.ID_TYPE_RELAY;
        } else {
            devid = XWPrefs.getGCMDevID( this );
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

    private void feedMessage( long rowid, byte[] msg )
    {
        DbgUtils.logf( "RelayService::feedMessage: %d bytes for rowid %d", 
                       msg.length, rowid );
        if ( BoardActivity.feedMessage( rowid, msg ) ) {
            DbgUtils.logf( "feedMessage: board ate it" );
            // do nothing
        } else {
            RelayMsgSink sink = new RelayMsgSink();
            sink.setRowID( rowid );
            if ( GameUtils.feedMessage( this, rowid, msg, null, 
                                        sink ) ) {
                setupNotification( rowid );
            } else {
                DbgUtils.logf( "feedMessage(): background dropped it" );
            }
        }
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
        DbgUtils.logf( "RelayService.process()" );
        if ( null != msgs ) {
            RelayMsgSink sink = new RelayMsgSink();
            int nameCount = relayIDs.length;
            DbgUtils.logf( "RelayService.process(): nameCount: %d", nameCount );
            ArrayList<String> idsWMsgs = new ArrayList<String>( nameCount );

            for ( int ii = 0; ii < nameCount; ++ii ) {
                byte[][] forOne = msgs[ii];

                // if game has messages, open it and feed 'em to it.
                if ( null != forOne ) {
                    sink.setRowID( rowIDs[ii] );
                    if ( BoardActivity.feedMessages( rowIDs[ii], forOne )
                         || GameUtils.feedMessages( this, rowIDs[ii],
                                                    forOne, null,
                                                    sink ) ) {
                        idsWMsgs.add( relayIDs[ii] );
                    } else {
                        DbgUtils.logf( "message for %s (rowid %d) not consumed",
                                       relayIDs[ii], rowIDs[ii] );
                    }
                }
            }
            if ( 0 < idsWMsgs.size() ) {
                String[] tmp = new String[idsWMsgs.size()];
                idsWMsgs.toArray( tmp );
                setupNotification( tmp );
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
            } catch ( java.io.IOException ioe ) {
                DbgUtils.loge( ioe );
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
            DbgUtils.logf( "sendToRelay: null msgs" );
        }
    } // sendToRelay

    private class RelayMsgSink extends MultiMsgSink {

        private HashMap<String,ArrayList<byte[]>> m_msgLists = null;
        private long m_rowid = -1;

        public void setRowID( long rowid ) { m_rowid = rowid; }

        public void send( Context context )
        {
            if ( -1 == m_rowid ) {
                sendToRelay( context, m_msgLists );
            } else {
                Assert.assertNull( m_msgLists );
            }
        }

        /***** TransportProcs interface *****/

        public int transportSend( byte[] buf, final CommsAddrRec addr, 
                                  int gameID )
        {
            Assert.assertTrue( -1 != m_rowid );
            sendPacket( RelayService.this, m_rowid, buf );
            return buf.length;
        }

        public boolean relayNoConnProc( byte[] buf, String relayID )
        {
            if ( -1 != m_rowid ) {
                sendNoConnMessage( m_rowid, relayID, buf );
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
        synchronized( s_packetsSent ) {
            if ( XWRelayReg.XWPDEV_ACK != cmd ) {
                nextPacketID = ++s_nextPacketID;
                s_packetsSent.add( nextPacketID );
            }
        }
        return nextPacketID;
    }

    private static void noteAck( int packetID )
    {
        synchronized( s_packetsSent ) {
            if ( s_packetsSent.contains( packetID ) ) {
                s_packetsSent.remove( packetID );
            } else {
                DbgUtils.logf( "Weird: got ack %d but never sent", packetID );
            }
            DbgUtils.logf( "RelayService.noteAck(): Got ack for %d; "
                           + "there are %d unacked packets", 
                           packetID, s_packetsSent.size() );
        }
    }

    // Called from any thread
    private void resetExitTimer()
    {
        // DbgUtils.logf( "RelayService.resetExitTimer()" );
        m_handler.removeCallbacks( m_onInactivity );

        // UDP socket's no good as a return address after several
        // minutes of inactivity, so do something after that time.
        m_handler.postDelayed( m_onInactivity, 
                               getMaxIntervalSeconds() * 1000 );
    }

    private void startThreads()
    {
        DbgUtils.logf( "RelayService.startThreads()" );
        if ( !NetStateCache.netAvail( this ) ) {
            stopThreads();
        } else if ( XWPrefs.getUDPEnabled( this ) ) {
            stopFetchThreadIf();
            startUDPThreadsIfNot();
            registerWithRelay();
        } else {
            stopUDPThreadsIf();
            startFetchThreadIf();
        }
    }

    private void stopThreads()
    {
        DbgUtils.logf( "RelayService.stopThreads()" );
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
                DbgUtils.logf( "vli2un: unable to read from stream" );
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
        int len = str.length();
        un2vli( len, os );
        os.writeBytes( str );
    }

    private void setMaxIntervalSeconds( int maxIntervalSeconds )
    {
        if ( m_maxIntervalSeconds != maxIntervalSeconds ) {
            m_maxIntervalSeconds = maxIntervalSeconds;
            XWPrefs.setPrefsInt( this, R.string.key_udp_interval, 
                                 maxIntervalSeconds );
        }
    }

    private int getMaxIntervalSeconds()
    {
        if ( 0 == m_maxIntervalSeconds ) {
            m_maxIntervalSeconds = 
                XWPrefs.getPrefsInt( this, R.string.key_udp_interval, 60 );
        }
        return m_maxIntervalSeconds;
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
        boolean result = XWPrefs.getGCMIgnored( this ) || !s_gcmWorking;
        if ( result ) {
            long interval = Utils.getCurSeconds() - m_lastGamePacketReceived;
            result = interval < MAX_KEEPALIVE_SECS;
        }
        DbgUtils.logf( "RelayService.shouldMaintainConnection=>%b", result );
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

}
