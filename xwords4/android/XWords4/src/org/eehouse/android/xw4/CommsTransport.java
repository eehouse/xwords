/* -*- compile-command: "cd ../../../../../; ant reinstall"; -*- */


package org.eehouse.android.xw4;

import org.eehouse.android.xw4.jni.*;

public class CommsTransport implements TransportProcs {

    public int transportSend( byte[] buf, final CommsAddrRec addr )
    {
        Utils.logf( "CommsTransport::transportSend() called!!!" );
        return -1;
    }

    public void relayStatus( int newState )
    {
    }

    public void relayConnd( boolean allHere, int nMissing )
    {
    }

    public void relayErrorProc( XWRELAY_ERROR relayErr )
    {
    }

}
