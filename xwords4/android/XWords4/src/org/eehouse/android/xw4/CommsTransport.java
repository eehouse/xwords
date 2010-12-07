/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
/*
 * Copyright 2009-2010 by Eric House (xwords@eehouse.org).  All
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

import java.net.InetSocketAddress;
import java.net.Socket;
import java.util.Vector;
import java.util.Iterator;
import junit.framework.Assert;
import android.telephony.SmsManager;
import android.content.BroadcastReceiver;
import android.content.Intent;
import android.content.IntentFilter;
import android.app.PendingIntent;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Build;
import android.os.Handler;
import android.os.Message;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.io.DataInputStream;
import java.io.DataOutputStream;


import org.eehouse.android.xw4.jni.*;
import org.eehouse.android.xw4.jni.JNIThread.*;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;

public class CommsTransport implements TransportProcs {

    public static final int DIALOG = 0;
    public static final int DIALOG_RETRY = 1;
    public static final int TOAST = 2;
    public static final int RELAY_COND = 3;

    public static final int RELAY_CONNND_ALLHERE = 0;
    public static final int RELAY_CONNND_MISSING = 1;

    public class ConndMsg {
        ConndMsg( String room, int devOrder, boolean allHere, int nMissing )
        {
            m_room = room;
            m_devOrder = devOrder;
            m_allHere = allHere;
            m_nMissing = nMissing;
        }
        public String m_room;
        public int m_devOrder;
        public boolean m_allHere;
        public int m_nMissing;
    }

    private int m_jniGamePtr;
    private CommsAddrRec m_addr;
    private JNIThread m_jniThread;
    private Handler m_handler;

    private Socket m_socket;
    private ReaderThread m_reader;
    private WriterThread m_writer;
    BlockingQueue<byte[]> m_queue;

    private Context m_context;
    private ConnectivityManager m_connMgr;
    private BroadcastReceiver m_receiver;
    private boolean m_netAvail = false;

    public CommsTransport( int jniGamePtr, Context context, Handler handler,
                           DeviceRole role )
    {
        m_jniGamePtr = jniGamePtr;
        m_context = context;
        m_handler = handler;

        buildNetAvailReceiver();
        m_queue = new ArrayBlockingQueue<byte[]>(16);
    }

    public class WriterThread extends Thread {
        public void run()
        {
            DataOutputStream os;
            try {
                os = new DataOutputStream( m_socket.getOutputStream() );
            } catch ( java.io.IOException ioe ) {
                Utils.logf( "%s", ioe.toString() );
                return;
            }
            
            for ( ; ; ) {
                try {
                byte[] buf = m_queue.take(); // blocks

                os.writeShort( buf.length );
                os.write( buf );
                Utils.logf( "wrote %d bytes to socket", buf.length );
                } catch ( InterruptedException inte ) {
                    Utils.logf( "%s", inte.toString() );
                    break;
                } catch( java.io.IOException ioe ) {
                    Utils.logf( "%s", ioe.toString() );
                    break;
                }
            }
        }
    }

    public class ReaderThread extends Thread {
        public void run()
        {
            DataInputStream dis;
            try {
                dis = new DataInputStream( m_socket.getInputStream() );
            } catch ( java.io.IOException ioe ) {
                Utils.logf( "%s", ioe.toString() );
                return;
            }

            for ( ; ; ) {
                try { 
                    Utils.logf( "ReaderThread: blocking inside readShort();" );
                    short len = dis.readShort();
                    Utils.logf( "ReaderThread: read length short: %d", len );
                    byte[] buf = new byte[len];
                    dis.readFully( buf );
                    Utils.logf( "returned from readFully()" );
                    m_jniThread.handle( JNICmd.CMD_RECEIVE, buf );
                } catch( java.io.IOException ioe ) {
                    Utils.logf( "%s", ioe.toString() );
                    break;
                }
            }
        }
    }

    public void setReceiver( JNIThread jnit )
    {
        m_jniThread = jnit;
    }

    public void waitToStop()
    {
        if ( null != m_socket ) {
            try {
                m_socket.close();
                m_socket = null;
            } catch ( java.io.IOException ioe ) {
                Utils.logf( "%s", ioe.toString() );
            }
        }
        // destroyNetAvailReceiver();
    }

    private void startThreadsIf()
    {
        if ( null == m_socket && m_netAvail ) {
            try {
                m_socket = new Socket( m_addr.ip_relay_hostName, 
                                       m_addr.ip_relay_port );
                if ( null != m_socket ) {
                    m_reader = new ReaderThread();
                    m_reader.start();
                    m_writer = new WriterThread();
                    m_writer.start();
                }
            } catch ( java.net.UnknownHostException uhe ) {
                Utils.logf( "%s", uhe.toString() );
                m_socket = null;
            } catch ( java.io.IOException ioe ) {
                Utils.logf( "%s", ioe.toString() );
                m_socket = null; // need to notify user on some of these
            }
        }
    }

    private class CommsBroadcastReceiver extends BroadcastReceiver {
        @Override
        public void onReceive( Context context, Intent intent ) 
        {
            Utils.logf( "CommsBroadcastReceiver::onReceive()" );
            if ( intent.getAction().
                 equals( ConnectivityManager.CONNECTIVITY_ACTION)) {

                NetworkInfo ni = (NetworkInfo)intent.
                    getParcelableExtra(ConnectivityManager.EXTRA_NETWORK_INFO);
                boolean netAvail = NetworkInfo.State.CONNECTED == ni.getState();
                if ( m_netAvail != netAvail ) {
                    // Do something; it's a change
                    m_netAvail = netAvail;
                    if ( netAvail ) {
                        startThreadsIf();
                    // } else {
                    //     waitToStop();
                    }
                }

                Utils.logf( "CommsTransport::onReceive: m_netAvail=%s",
                            m_netAvail?"true":"false" );
            }
        }
    }

    private void buildNetAvailReceiver()
    {
        m_connMgr = (ConnectivityManager)
            m_context.getSystemService( Context.CONNECTIVITY_SERVICE );
        NetworkInfo ni = m_connMgr.getActiveNetworkInfo();
        m_netAvail = null != ni && 
            NetworkInfo.State.CONNECTED == ni.getState();
        Utils.logf( "CommsTransport::buildNetAvailReceiver: m_netAvail=%s",
                    m_netAvail?"true":"false" );

        m_receiver = new CommsBroadcastReceiver();
        IntentFilter filter = new IntentFilter();
        filter.addAction( ConnectivityManager.CONNECTIVITY_ACTION );
        Intent intent = m_context.registerReceiver( m_receiver, filter );
        Utils.logf( "CommsTransport::registerReceiver->%s", 
                    intent==null?"null" : intent.toString() );
    }

    private void destroyNetAvailReceiver()
    {
        if ( null != m_receiver ) {
            m_context.unregisterReceiver( m_receiver );
            m_receiver = null;
        }
    }

    // TransportProcs interface
    public int transportSend( byte[] buf, final CommsAddrRec faddr )
    {
        Utils.logf( "CommsTransport::transportSend(nbytes=%d)", buf.length );
        int nSent = -1;

        if ( null == m_addr ) {
            if ( null == faddr ) {
                m_addr = new CommsAddrRec( m_context );
                XwJNI.comms_getAddr( m_jniGamePtr, m_addr );
            } else {
                m_addr = new CommsAddrRec( faddr );
            }
        }

        switch ( m_addr.conType ) {
        case COMMS_CONN_RELAY:
            startThreadsIf();
            try {
                // add(), not put(): don't block thread in comms if full
                m_queue.add( buf );
                nSent = buf.length;
            } catch ( IllegalStateException ise ) {
                Utils.logf( "%s", ise.toString() );
            }
            break;
        case COMMS_CONN_SMS:
            Assert.fail();
            // This code can't be here, even if unreachable, unless
            // app has permission to use SMS.  So put it in a separate
            // module and catch the error that'll come when it fails
            // to verify.  IFF the plan's to ship a version that
            // doesn't do SMS.

            // Utils.logf( "sending via sms to  %s:%d", 
            //             m_addr.sms_phone, m_addr.sms_port );
            // try {
            //     Intent intent = new Intent( m_context, StatusReceiver.class);
            //     PendingIntent pi
            //         = PendingIntent.getBroadcast( m_context, 0,
            //                                       intent, 0 );
            //     if ( 0 == m_addr.sms_port ) {
            //          SmsManager.getDefault().sendTextMessage( m_addr.sms_phone,
            //                                                   null, "Hello world",
            //                                                   pi, pi );
            //         Utils.logf( "called sendTextMessage" );
            //     } else {
            //         SmsManager.getDefault().
            //             sendDataMessage( m_addr.sms_phone, (String)null,
            //                              (short)m_addr.sms_port, 
            //                              buf, pi, pi );
            //         Utils.logf( "called sendDataMessage" );
            //     }
            //     nSent = buf.length;
            // } catch ( java.lang.IllegalArgumentException iae ) {
            //     Utils.logf( iae.toString() );
            // }
            break;
        case COMMS_CONN_BT:
        default:
            Assert.fail();
            break;
        }

        return nSent;
    } 

    public void relayStatus( CommsRelayState newState )
    {
        //Utils.logf( "relayStatus called; state=%s", newState.toString() );
        if ( null != m_jniThread ) {
            m_jniThread.handle( JNICmd.CMD_DRAW_CONNS_STATUS, newState );
        } else {
            Utils.logf( "can't draw status yet" );
        }
    }

    public void relayConnd( String room, int devOrder, boolean allHere, 
                            int nMissing )
    {
        ConndMsg cndmsg = new ConndMsg( room, devOrder, allHere, nMissing );
        Message.obtain( m_handler, RELAY_COND, cndmsg ).sendToTarget();
    }

    public void relayErrorProc( XWRELAY_ERROR relayErr )
    {
        //Utils.logf( "relayErrorProc called; got " + relayErr.toString() );

        int strID = 0;
        int how = TOAST;

        switch ( relayErr ) {
        case TOO_MANY: 
            strID = R.string.msg_too_many;
            how = DIALOG;
            break;
        case NO_ROOM:
            strID = R.string.msg_no_room;
            how = DIALOG_RETRY;
            break;
        case DUP_ROOM:
            strID = R.string.msg_dup_room;
            how = DIALOG;
            break;
        case LOST_OTHER:
        case OTHER_DISCON:
            strID = R.string.msg_lost_other;
            break;

        case DELETED:
            strID = R.string.msg_dev_deleted;
            how = DIALOG;
            break;

        case OLDFLAGS:
        case BADPROTO:
        case RELAYBUSY:
        case SHUTDOWN:
        case TIMEOUT:
        case HEART_YOU:
        case HEART_OTHER:
            break;
        }

        if ( 0 != strID ) {
            String str = m_context.getString( strID );
            Message.obtain( m_handler, how, R.string.relay_alert, 
                            0, str ).sendToTarget();
        }
    }
}
