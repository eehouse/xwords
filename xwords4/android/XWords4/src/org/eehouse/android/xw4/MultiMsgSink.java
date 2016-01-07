/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
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
import java.util.HashSet;
import java.util.Set;

import junit.framework.Assert;

import org.eehouse.android.xw4.jni.*;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;

public class MultiMsgSink implements TransportProcs {
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
    public void setRowID( long rowID ) { m_rowid = rowID; };

    // These will be overridden by e.g. BTService which for sendViaBluetooth()
    // can just insert a message into its queue
    public int sendViaRelay( byte[] buf, int gameID )
    {
        Assert.assertTrue( XWApp.UDP_ENABLED );
        return RelayService.sendPacket( m_context, getRowID(), buf );
    }

    public int sendViaBluetooth( byte[] buf, int gameID, CommsAddrRec addr )
    {
        return BTService.enqueueFor( m_context, buf, addr, gameID );
    }

    public int sendViaSMS( byte[] buf, int gameID, CommsAddrRec addr )
    {
        return SMSService.sendPacket( m_context, addr.sms_phone, gameID, buf );
    }

    public int numSent()
    {
        return m_sentSet.size();
    }

    /***** TransportProcs interface *****/

    public int getFlags() { return COMMS_XPORT_FLAGS_HASNOCONN; }

    public int transportSend( byte[] buf, String msgNo, CommsAddrRec addr, 
                              CommsConnType typ, int gameID )
    {
        int nSent = -1;
        switch ( typ ) {
        case COMMS_CONN_RELAY:
            nSent = sendViaRelay( buf, gameID );
            break;
        case COMMS_CONN_BT:
            nSent = sendViaBluetooth( buf, gameID, addr );
            break;
        case COMMS_CONN_SMS:
            nSent = sendViaSMS( buf, gameID, addr );
            break;
        default:
            Assert.fail();
            break;
        }
        DbgUtils.logf( "MultiMsgSink.transportSend(): sent %d via %s", 
                       nSent, typ.toString() );
        if ( 0 < nSent ) {
            DbgUtils.logdf( "MultiMsgSink.transportSend: adding %s", msgNo );
            m_sentSet.add( msgNo );
        }

        return nSent;
    }

    public void relayStatus( CommsRelayState newState )
    {
    }

    public void relayErrorProc( XWRELAY_ERROR relayErr )
    {
    }

    public void relayConnd( String room, int devOrder, boolean allHere, 
                            int nMissing )
    {
    }

    public boolean relayNoConnProc( byte[] buf, String msgNo, String relayID )
    {
        // Assert.fail();
        int nSent = RelayService.sendNoConnPacket( m_context, getRowID(), 
                                                   relayID, buf );
        boolean success = buf.length == nSent;
        if ( success ) {
            DbgUtils.logdf( "MultiMsgSink.relayNoConnProc: adding %s", msgNo );
            m_sentSet.add( msgNo );
        }
        return success;
    }
}
