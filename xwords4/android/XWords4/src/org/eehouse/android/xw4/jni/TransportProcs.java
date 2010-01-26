
package org.eehouse.android.xw4.jni;

public interface TransportProcs {
    int transportSend( byte[] buf, int len, final CommsAddrRec addr );

    public static final int COMMS_RELAYSTATE_UNCONNECTED = 0;
    public static final int COMMS_RELAYSTATE_DENIED = 1;
    public static final int COMMS_RELAYSTATE_CONNECT_PENDING = 2;
    public static final int COMMS_RELAYSTATE_CONNECTED = 3;
    public static final int COMMS_RELAYSTATE_RECONNECTED = 4;
    public static final int COMMS_RELAYSTATE_ALLCONNECTED = 5;
    void relayStatus( int newState );

    void relayConnd( boolean allHere, int nMissing );

    public static enum XWRELAY_ERROR { NONE
            ,OLDFLAGS 
            ,BADPROTO
            ,RELAYBUSY
            ,SHUTDOWN
            ,TIMEOUT 
            ,HEART_YOU
            ,HEART_OTHER
            ,LOST_OTHER
            ,OTHER_DISCON
            ,NO_ROOM
            ,DUP_ROOM
            ,TOO_MANY
    };
    void relayErrorProc( XWRELAY_ERROR relayErr );
}
