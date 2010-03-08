/* -*- compile-command: "cd ../../../../../../; ant install"; -*- */


package org.eehouse.android.xw4.jni;

public interface TransportProcs {
    int transportSend( byte[] buf, final CommsAddrRec addr );

    enum CommsRelayState { COMMS_RELAYSTATE_UNCONNECTED
            , COMMS_RELAYSTATE_DENIED
            , COMMS_RELAYSTATE_CONNECT_PENDING
            , COMMS_RELAYSTATE_CONNECTED
            , COMMS_RELAYSTATE_RECONNECTED
            , COMMS_RELAYSTATE_ALLCONNECTED
    };
    void relayStatus( CommsRelayState newState );

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
