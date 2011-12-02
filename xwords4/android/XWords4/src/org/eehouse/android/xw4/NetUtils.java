/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
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

import javax.net.SocketFactory;
import java.net.InetAddress;
import java.net.Socket;
import android.content.Context;

import java.io.InputStream;
import java.io.DataInputStream;
import java.io.OutputStream;
import java.io.DataOutputStream;
import java.util.ArrayList;

import org.eehouse.android.xw4.jni.CommonPrefs;

public class NetUtils {

    public static final byte PROTOCOL_VERSION = 0;
    // from xwrelay.h
    public static byte PRX_PUB_ROOMS = 1;
    public static byte PRX_HAS_MSGS = 2;
    public static byte PRX_DEVICE_GONE = 3;
    public static byte PRX_GET_MSGS = 4;
    
    public static Socket MakeProxySocket( Context context, 
                                          int timeoutMillis )
    {
        Socket socket = null;
        try {
            int port = CommonPrefs.getDefaultProxyPort( context );
            String host = CommonPrefs.getDefaultRelayHost( context );

            SocketFactory factory = SocketFactory.getDefault();
            InetAddress addr = InetAddress.getByName( host );
            socket = factory.createSocket( addr, port );
            socket.setSoTimeout( timeoutMillis );

        } catch ( java.net.UnknownHostException uhe ) {
            DbgUtils.logf( uhe.toString() );
        } catch( java.io.IOException ioe ) {
            DbgUtils.logf( ioe.toString() );
        }
        return socket;
    }

    private static class InformThread extends Thread {
        Context m_context;
        DBUtils.Obit[] m_obits;
        public InformThread( Context context, DBUtils.Obit[] obits )
        {
            m_context = context;
            m_obits = obits;
        }

        public void run() {
            Socket socket = MakeProxySocket( m_context, 10000 );
            if ( null != socket ) {
                int strLens = 0;
                for ( int ii = 0; ii < m_obits.length; ++ii ) {
                    strLens += m_obits[ii].m_relayID.length() + 1; // 1 for /n
                }

                try {
                    DataOutputStream outStream = 
                        new DataOutputStream( socket.getOutputStream() );
                    outStream.writeShort( 2 + 2 + (2*m_obits.length) + strLens );
                    outStream.writeByte( NetUtils.PROTOCOL_VERSION );
                    outStream.writeByte( NetUtils.PRX_DEVICE_GONE );
                    outStream.writeShort( m_obits.length );

                    for ( int ii = 0; ii < m_obits.length; ++ii ) {
                        outStream.writeShort( m_obits[ii].m_seed );
                        outStream.writeBytes( m_obits[ii].m_relayID );
                        outStream.write( '\n' );
                    }

                    outStream.flush();

                    DataInputStream dis = 
                        new DataInputStream( socket.getInputStream() );
                    short resLen = dis.readShort();
                    socket.close();

                    if ( resLen == 0 ) {
                        DBUtils.clearObits( m_context, m_obits );
                    }
                } catch ( java.io.IOException ioe ) {
                    DbgUtils.logf( ioe.toString() );
                }
            }
        }
    }

    public static void informOfDeaths( Context context )
    {
        DBUtils.Obit[] obits = DBUtils.listObits( context );
        if ( null != obits && 0 < obits.length ) {
            InformThread thread = new InformThread( context, obits );
            thread.start();
        }
    }

    public static String[] QueryRelay( Context context )
    {
        String[] result = null;
        int[] nBytes = new int[1];
        String[] ids = collectIDs( context, nBytes );
        if ( null != ids && 0 < ids.length ) {
            try {
                Socket socket = MakeProxySocket( context, 8000 );
                DataOutputStream outStream = 
                    new DataOutputStream( socket.getOutputStream() );

                // total packet size
                outStream.writeShort( 2 + nBytes[0] + ids.length + 1 );

                outStream.writeByte( NetUtils.PROTOCOL_VERSION );
                outStream.writeByte( NetUtils.PRX_GET_MSGS );

                // number of ids
                outStream.writeShort( ids.length );

                for ( String id : ids ) {
                    outStream.writeBytes( id );
                    outStream.write( '\n' );
                }
                outStream.flush();

                DataInputStream dis = 
                    new DataInputStream(socket.getInputStream());
                short resLen = dis.readShort();
                short nameCount = dis.readShort();
                byte[][][] msgs = null;
                if ( nameCount == ids.length ) {
                    msgs = new byte[nameCount][][];
                    for ( int ii = 0; ii < nameCount; ++ii ) {
                        short countsThisGame = dis.readShort();
                        if ( countsThisGame > 0 ) {
                            msgs[ii] = new byte[countsThisGame][];
                            for ( int jj = 0; jj < countsThisGame; ++jj ) {
                                short len = dis.readShort();
                                if ( len > 0 ) {
                                    byte[] packet = new byte[len];
                                    dis.read( packet );
                                    msgs[ii][jj] = packet;
                                }
                            }
                        }
                    }
                }
                socket.close();

                if ( null != msgs ) {
                    ArrayList<String> idsWMsgs =
                        new ArrayList<String>( nameCount );
                    for ( int ii = 0; ii < nameCount; ++ii ) {
                        // if game has messages, open it and feed 'em
                        // to it.
                        if ( null != msgs[ii] ) {
                            if( GameUtils.feedMessages( context, ids[ii], 
                                                        msgs[ii] ) ) {
                                idsWMsgs.add( ids[ii] );
                            }
                        }
                    }
                    if ( 0 < idsWMsgs.size() ) {
                        ids = new String[idsWMsgs.size()];
                        result = idsWMsgs.toArray( ids );
                    }
                }

            } catch( java.net.UnknownHostException uhe ) {
                DbgUtils.logf( uhe.toString() );
            } catch( java.io.IOException ioe ) {
                DbgUtils.logf( ioe.toString() );
            } catch( NullPointerException npe ) {
                DbgUtils.logf( npe.toString() );
            }
        }
        return result;
    } // QueryRelay

    private static String[] collectIDs( Context context, int[] nBytes )
    {
        String[] ids = DBUtils.getRelayIDs( context, false );
        int len = 0;
        if ( null != ids ) {
            for ( String id : ids ) {
                len += id.length();
            }
        }
        nBytes[0] = len;
        return ids;
    }
}
