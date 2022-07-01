/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2022 by Eric House (xwords@eehouse.org).  All
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

import android.content.Context;


import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;
import org.eehouse.android.xw4.jni.TransportProcs;

public class CommsTransport implements TransportProcs {
    private static final String TAG = CommsTransport.class.getSimpleName();
    private TransportProcs.TPMsgHandler m_tpHandler;
    private boolean m_done = false;

    private Context m_context;
    private long m_rowid;

    // assembling inbound packet
    private byte[] m_packetIn;
    private int m_haveLen = -1;

    public CommsTransport( Context context, TransportProcs.TPMsgHandler handler,
                           long rowid, DeviceRole role )
    {
        m_context = context;
        m_tpHandler = handler;
        m_rowid = rowid;
    }

    // TransportProcs interface
    private static final boolean TRANSPORT_DOES_NOCONN = true;
    @Override
    public int getFlags() {
        return TRANSPORT_DOES_NOCONN ? COMMS_XPORT_FLAGS_HASNOCONN : COMMS_XPORT_FLAGS_NONE;
    }

    @Override
    public int transportSend( byte[] buf, String msgID, CommsAddrRec addr,
                              CommsConnType conType, int gameID, int timestamp )
    {
        Log.d( TAG, "transportSend(len=%d, typ=%s, ts=%d)", buf.length,
               conType.toString(), timestamp );
        int nSent = -1;
        Assert.assertNotNull( addr );
        Assert.assertTrueNR( addr.contains( conType ) ); // fired per google

        if ( !BuildConfig.UDP_ENABLED && conType == CommsConnType.COMMS_CONN_RELAY ) {
            Assert.failDbg();
        }

        if ( !BuildConfig.UDP_ENABLED && conType == CommsConnType.COMMS_CONN_RELAY ) {
            if ( NetStateCache.netAvail( m_context ) ) {
                Assert.failDbg();
                nSent = -1;
            }
        } else {
            nSent = sendForAddr( m_context, addr, conType, m_rowid, gameID,
                                 timestamp, buf, msgID );
        }

        // Keep this while debugging why the resend_all that gets
        // fired on reconnect doesn't unstall a game but a manual
        // resend does.
        Log.d( TAG, "transportSend(len=%d, typ=%s) => %d", buf.length,
               conType, nSent );
        return nSent;
    }

    @Override
    public void countChanged( int newCount )
    {
        m_tpHandler.tpmCountChanged( newCount );
    }

    private int sendForAddr( Context context, CommsAddrRec addr,
                             CommsConnType conType, long rowID,
                             int gameID, int timestamp,
                             byte[] buf, String msgID )
    {
        int nSent = -1;
        switch ( conType ) {
        case COMMS_CONN_RELAY:
            Log.e( TAG, "sendForAddr();still sending via RELAY" );
            break;
        case COMMS_CONN_SMS:
            nSent = NBSProto.sendPacket( context, addr.sms_phone,
                                         gameID, buf, msgID );
            break;
        case COMMS_CONN_BT:
            nSent = BTUtils.sendPacket( context, buf, msgID, addr, gameID );
            break;
        case COMMS_CONN_P2P:
            nSent = WiDirService
                .sendPacket( context, addr.p2p_addr, gameID, buf );
            break;
        case COMMS_CONN_NFC:
            nSent = NFCUtils.addMsgFor( buf, gameID );
            break;
        case COMMS_CONN_MQTT:
            nSent = MQTTUtils.send( context, addr.mqtt_devID, gameID, timestamp, buf );
            break;
        default:
            Assert.failDbg();
            break;
        }
        Log.d( TAG, "sendForAddr(typ=%s, len=%d) => %d", conType,
               buf.length, nSent );
        return nSent;
    }

    /* NPEs in m_selector calls: sometimes my Moment gets into a state
     * where after 15 or so seconds of Crosswords trying to connect to
     * the relay I get a crash.  Logs show it's inside one or both of
     * these Selector methods.  Rebooting the device gets it out of
     * that state, so I suspect it's a but in 2.1 or Samsung's build
     * of it.  Should watch crash reports at developer.android.com and
     * perhaps catch NPEs here just to be safe.  But then do what?
     * Tell user to restart device?
     */
}
