/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2012 by Eric House (xwords@eehouse.org).  All
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
import org.eehouse.android.xw4.jni.TransportProcs;

import java.util.HashSet;
import java.util.Set;

public class MultiMsgSink implements TransportProcs {
    private static final String TAG = MultiMsgSink.class.getSimpleName();
    private long m_rowid;
    private Context m_context;
    // Use set to count so message sent over both BT and Relay is counted only
    // once.
    private Set<String> m_sentSet = new HashSet<String>();

    public MultiMsgSink( Context context, long rowid )
    {
        m_context = context;
        m_rowid = rowid;
    }

    public MultiMsgSink( Context context )
    {
        this( context, 0 );
    }

    // rowID is used as token to identify game on relay.  Anything that
    // uniquely identifies a game on a device would work
    public long getRowID() { return m_rowid; };
    public MultiMsgSink setRowID( long rowID ) { m_rowid = rowID; return this; };

    // These will be overridden by e.g. BTService which for sendViaBluetooth()
    // can just insert a message into its queue
    public int sendViaRelay( byte[] buf, String msgID, int gameID )
    {
        Assert.assertTrue( BuildConfig.UDP_ENABLED );
        return RelayService.sendPacket( m_context, getRowID(), buf, msgID );
    }

    public int sendViaBluetooth( byte[] buf, String msgID, int gameID,
                                 CommsAddrRec addr )
    {
        return BTService.sendPacket( m_context, buf, msgID, addr, gameID );
    }

    public int sendViaSMS( byte[] buf, int gameID, CommsAddrRec addr )
    {
        return NBSProto.sendPacket( m_context, addr.sms_phone, gameID, buf );
    }

    public int sendViaP2P( byte[] buf, int gameID, CommsAddrRec addr )
    {
        return WiDirService
            .sendPacket( m_context, addr.p2p_addr, gameID, buf );
    }

    public int numSent()
    {
        return m_sentSet.size();
    }

    /***** TransportProcs interface *****/
    @Override
    public int getFlags() { return COMMS_XPORT_FLAGS_HASNOCONN; }

    @Override
    public int transportSend( byte[] buf, String msgID, CommsAddrRec addr,
                              CommsConnType typ, int gameID )
    {
        int nSent = -1;
        switch ( typ ) {
        case COMMS_CONN_RELAY:
            nSent = sendViaRelay( buf, msgID, gameID );
            break;
        case COMMS_CONN_BT:
            nSent = sendViaBluetooth( buf, msgID, gameID, addr );
            break;
        case COMMS_CONN_SMS:
            nSent = sendViaSMS( buf, gameID, addr );
            break;
        case COMMS_CONN_P2P:
            nSent = sendViaP2P( buf, gameID, addr );
            break;
        default:
            Assert.fail();
            break;
        }
        Log.i( TAG, "transportSend(): sent %d msgs for game %d/%x via %s",
               nSent, gameID, gameID, typ.toString() );
        if ( 0 < nSent ) {
            Log.d( TAG, "transportSend: adding %s", msgID );
            m_sentSet.add( msgID );
        }

        Log.d( TAG, "transportSend(len=%d, typ=%s) => %d", buf.length,
               typ, nSent );
        return nSent;
    }

    @Override
    public void relayErrorProc( XWRELAY_ERROR relayErr )
    {
    }

    @Override
    public void relayConnd( String room, int devOrder, boolean allHere,
                            int nMissing )
    {
    }

    @Override
    public boolean relayNoConnProc( byte[] buf, String msgNo, String relayID )
    {
        // Assert.fail();
        int nSent = RelayService.sendNoConnPacket( m_context, getRowID(),
                                                   relayID, buf, msgNo );
        boolean success = buf.length == nSent;
        if ( success ) {
            Log.d( TAG, "relayNoConnProc: adding %s", msgNo );
            m_sentSet.add( msgNo );
        }
        return success;
    }
}
