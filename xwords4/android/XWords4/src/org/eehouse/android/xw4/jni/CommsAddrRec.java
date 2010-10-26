/* -*- compile-command: "cd ../../../../../../; ant install"; -*- */
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

package org.eehouse.android.xw4.jni;

import java.net.InetAddress;
import android.content.Context;

import org.eehouse.android.xw4.Utils;

public class CommsAddrRec {

    public enum CommsConnType { COMMS_CONN_NONE,
            COMMS_CONN_IR,
            COMMS_CONN_IP_DIRECT,
            COMMS_CONN_RELAY,
            COMMS_CONN_BT,
            COMMS_CONN_SMS,
    };

    // The C equivalent of this struct uses a union for the various
    // data sets below.  So don't assume that any fields will be valid
    // except those for the current conType.
    public CommsConnType conType;

    // relay case
    public String ip_relay_invite;
    public String ip_relay_hostName;
    public InetAddress ip_relay_ipAddr;    // a cache, maybe unused in java
    public int ip_relay_port;
    public boolean ip_relay_seeksPublicRoom;
    public boolean ip_relay_advertiseRoom;

    // sms case
    public String sms_phone;
    public int sms_port;                   // NBS port, if they still use those

    public CommsAddrRec( Context context ) 
    {
        conType = CommsConnType.COMMS_CONN_RELAY;
        ip_relay_hostName = CommonPrefs.getDefaultRelayHost( context );
        ip_relay_port = CommonPrefs.getDefaultRelayPort( context );
        ip_relay_seeksPublicRoom = false;
        ip_relay_advertiseRoom = false;
    }

    public CommsAddrRec( final CommsAddrRec src ) 
    {
        this.copyFrom( src );
    }

    public boolean changesMatter( final CommsAddrRec other )
    {
        boolean matter = conType != other.conType;
        if ( !matter ) {
            matter = ! ip_relay_invite.equals( other.ip_relay_invite )
                || ! ip_relay_hostName.equals( other.ip_relay_hostName )
                || ip_relay_port != other.ip_relay_port;
        }
        return matter;
    }

    private void copyFrom( CommsAddrRec src )
    {
        conType = src.conType;
        ip_relay_invite = src.ip_relay_invite;
        ip_relay_hostName = src.ip_relay_hostName;
        ip_relay_port = src.ip_relay_port;
        ip_relay_seeksPublicRoom = src.ip_relay_seeksPublicRoom;
        ip_relay_advertiseRoom = src.ip_relay_advertiseRoom;
    }
}
