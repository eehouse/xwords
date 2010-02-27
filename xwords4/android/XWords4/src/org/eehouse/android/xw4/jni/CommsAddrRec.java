/* -*- compile-command: "cd ../../../../../../; ant install"; -*- */
package org.eehouse.android.xw4.jni;

import java.net.InetAddress;

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

    // sms case
    public String sms_phone;
    public int sms_port;                   // NBS port, if they still use those

    public CommsAddrRec() {
        Utils.logf( "CommsAddrRec() called " );
        conType = CommsConnType.COMMS_CONN_RELAY;
        ip_relay_invite = "Room 1";
        ip_relay_hostName = CommonPrefs.getDefaultRelayHost();
        ip_relay_port = 10999;
    }

    public CommsAddrRec( CommsAddrRec src ) {
        this.copyFrom(src );
    }

    private void copyFrom( CommsAddrRec src )
    {
        conType = src.conType;
        ip_relay_invite = src.ip_relay_invite;
        ip_relay_hostName = src.ip_relay_hostName;
        ip_relay_port = src.ip_relay_port;
    }

    private static CommsAddrRec s_car;
    public static final CommsAddrRec get() 
    { 
        if ( null == s_car ) {
            s_car = new CommsAddrRec();
        }
        return s_car;
    }
}
