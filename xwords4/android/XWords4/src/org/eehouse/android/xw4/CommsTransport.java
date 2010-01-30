/* -*- compile-command: "cd ../../../../../; ant reinstall"; -*- */

package org.eehouse.android.xw4;

import java.nio.channels.Selector;
import java.nio.channels.SocketChannel;

import org.eehouse.android.xw4.jni.*;

public class CommsTransport extends Thread implements TransportProcs {
    private Selector m_selector;
    private SocketChannel m_socketChannel;
    private int m_jniGamePtr;
    // private CommsAddrRec m_addr;

    public CommsTransport( int jniGamePtr )
    {
        m_jniGamePtr = jniGamePtr;
    }

    @Override
    public void run() 
    {
    }

    // TransportProcs interface
    public int transportSend( byte[] buf, final CommsAddrRec faddr )
    {
        Utils.logf( "CommsTransport::transportSend" );

        CommsAddrRec addr = faddr;
        if ( null == addr ) {
            addr = new CommsAddrRec();
            XwJNI.comms_getAddr( m_jniGamePtr, addr );
        }

        Utils.logf( "CommsTransport::transportSend(" + addr.ip_relay_hostName + 
                    ") called!!!" );
        return buf.length;
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
