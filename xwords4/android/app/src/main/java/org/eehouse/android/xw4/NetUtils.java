/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
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
import android.text.TextUtils;

import junit.framework.Assert;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedInputStream;
import java.io.BufferedWriter;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.net.HttpURLConnection;
import java.net.InetAddress;
import java.net.Socket;
import java.net.URL;
import java.net.URLEncoder;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

import javax.net.SocketFactory;

public class NetUtils {
    private static final String TAG = NetUtils.class.getSimpleName();

    public static final String k_PARAMS = "params";
    public static final byte PROTOCOL_VERSION = 0;
    // from xwrelay.h
    public static byte PRX_PUB_ROOMS = 1;
    public static byte PRX_HAS_MSGS = 2;
    public static byte PRX_DEVICE_GONE = 3;
    public static byte PRX_GET_MSGS = 4;
    public static byte PRX_PUT_MSGS = 5;

    public static Socket makeProxySocket( Context context,
                                          int timeoutMillis )
    {
        Socket socket = null;
        try {
            int port = XWPrefs.getDefaultProxyPort( context );
            String host = XWPrefs.getDefaultRelayHost( context );

            SocketFactory factory = SocketFactory.getDefault();
            InetAddress addr = InetAddress.getByName( host );
            socket = factory.createSocket( addr, port );
            socket.setSoTimeout( timeoutMillis );

        } catch ( java.net.UnknownHostException uhe ) {
            Log.ex( TAG, uhe );
        } catch( java.io.IOException ioe ) {
            Log.ex( TAG, ioe );
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

        public void run()
        {
            if ( XWPrefs.getPreferWebAPI( m_context ) ) {
                runViaWeb();
            } else {
                runWithProxySocket();
            }
        }

        private void runViaWeb()
        {
            try {
                JSONArray params = new JSONArray();
                for ( int ii = 0; ii < m_obits.length; ++ii ) {
                    JSONObject one = new JSONObject();
                    one.put( "relayID", m_obits[ii].m_relayID );
                    one.put( "seed", m_obits[ii].m_seed );
                    params.put( one );
                }
                HttpURLConnection conn = makeHttpRelayConn( m_context, "kill" );
                Log.d( TAG, "runViaWeb(): kill params: %s", params.toString() );
                String resStr = runConn( conn, params );
                Log.d( TAG, "runViaWeb(): kill => %s", resStr );

                if ( null != resStr ) {
                    JSONObject result = new JSONObject( resStr );
                    if ( 0 == result.optInt( "err", -1 ) ) {
                        DBUtils.clearObits( m_context, m_obits );
                    }
                } else {
                    Log.e( TAG, "runViaWeb(): KILL => null" );
                }
            } catch ( JSONException ex ) {
                Assert.assertFalse( BuildConfig.DEBUG );
            }
        }

        private void runWithProxySocket()
        {
            Socket socket = makeProxySocket( m_context, 10000 );
            if ( null != socket ) {
                int strLens = 0;
                int nObits = 0;
                for ( int ii = 0; ii < m_obits.length; ++ii ) {
                    String relayID = m_obits[ii].m_relayID;
                    if ( null != relayID ) {
                        ++nObits;
                        strLens += relayID.length() + 1; // 1 for /n
                    }
                }

                try {
                    DataOutputStream outStream =
                        new DataOutputStream( socket.getOutputStream() );
                    outStream.writeShort( 2 + 2 + (2*nObits) + strLens );
                    outStream.writeByte( NetUtils.PROTOCOL_VERSION );
                    outStream.writeByte( NetUtils.PRX_DEVICE_GONE );
                    outStream.writeShort( m_obits.length );

                    for ( int ii = 0; ii < m_obits.length; ++ii ) {
                        String relayID = m_obits[ii].m_relayID;
                        if ( null != relayID ) {
                            outStream.writeShort( m_obits[ii].m_seed );
                            outStream.writeBytes( relayID );
                            outStream.write( '\n' );
                        }
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
                    Log.ex( TAG, ioe );
                }
            }
        }
    }

    public static void informOfDeaths( Context context )
    {
        DBUtils.Obit[] obits = DBUtils.listObits( context );
        if ( null != obits && 0 < obits.length ) {
            new InformThread( context, obits ).start();
        }
    }

    public static byte[][][] queryRelay( Context context, String[] ids )
    {
        byte[][][] msgs = null;
        try {
            Socket socket = makeProxySocket( context, 8000 );
            if ( null != socket ) {
                DataOutputStream outStream =
                    new DataOutputStream( socket.getOutputStream() );

                // total packet size
                int nBytes = sumStrings( ids );
                outStream.writeShort( 2 + nBytes + ids.length + 1 );

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
                short resLen = dis.readShort();          // total message length
                short nameCount = dis.readShort();

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
                                    dis.readFully( packet );
                                    msgs[ii][jj] = packet;
                                }
                            }
                        }
                    }
                }
                if ( 0 != dis.available() ) {
                    msgs = null;
                    Log.e( TAG, "format error: bytes left over in stream" );
                }
                socket.close();
            }

        } catch( Exception npe ) {
            Log.ex( TAG, npe );
        }
        return msgs;
    } // queryRelay

    private static final String FORCE_RELAY_HOST = null;
    // private static final String FORCE_RELAY_HOST = "eehouse.org";
    public static String forceHost( String host )
    {
        if ( null != FORCE_RELAY_HOST ) {
            host = FORCE_RELAY_HOST;
        }
        return host;
    }

    protected static HttpURLConnection makeHttpRelayConn( Context context,
                                                           String proc )
    {
        String url = XWPrefs.getDefaultRelayUrl( context );
        return makeHttpConn( context, url, proc );
    }

    protected static HttpURLConnection makeHttpUpdateConn( Context context,
                                                           String proc )
    {
        String url = XWPrefs.getDefaultUpdateUrl( context );
        return makeHttpConn( context, url, proc );
    }

    private static HttpURLConnection makeHttpConn( Context context,
                                                   String path, String proc )
    {
        HttpURLConnection result = null;
        try {
            String url = String.format( "%s/%s", path, proc );
            result = (HttpURLConnection)new URL(url).openConnection();
        } catch ( java.net.MalformedURLException mue ) {
            Assert.assertNull( result );
            Log.ex( TAG, mue );
        } catch ( java.io.IOException ioe ) {
            Assert.assertNull( result );
            Log.ex( TAG, ioe );
        }
        return result;
    }

    protected static String runConn( HttpURLConnection conn, JSONArray param )
    {
        return runConn( conn, param.toString() );
    }

    protected static String runConn( HttpURLConnection conn, JSONObject param )
    {
        return runConn( conn, param.toString() );
    }

    private static String runConn( HttpURLConnection conn, String param )
    {
        String result = null;
        Map<String, String> params = new HashMap<String, String>();
        params.put( k_PARAMS, param );
        String paramsString = getPostDataString( params );

        if ( null != paramsString ) {
            try {
                conn.setReadTimeout( 15000 );
                conn.setConnectTimeout( 15000 );
                conn.setRequestMethod( "POST" );
                conn.setDoInput( true );
                conn.setDoOutput( true );
                conn.setFixedLengthStreamingMode( paramsString.length() );

                OutputStream os = conn.getOutputStream();
                BufferedWriter writer
                    = new BufferedWriter(new OutputStreamWriter(os, "UTF-8"));
                writer.write( paramsString );
                writer.flush();
                writer.close();
                os.close();

                int responseCode = conn.getResponseCode();
                if ( HttpURLConnection.HTTP_OK == responseCode ) {
                    InputStream is = conn.getInputStream();
                    BufferedInputStream bis = new BufferedInputStream( is );

                    ByteArrayOutputStream bas = new ByteArrayOutputStream();
                    byte[] buffer = new byte[1024];
                    for ( ; ; ) {
                        int nRead = bis.read( buffer );
                        if ( 0 > nRead ) {
                            break;
                        }
                        bas.write( buffer, 0, nRead );
                    }
                    result = new String( bas.toByteArray() );
                } else {
                    Log.w( TAG, "runConn: responseCode: %d", responseCode );
                }
            } catch ( java.net.ProtocolException pe ) {
                Log.ex( TAG, pe );
            } catch( java.io.IOException ioe ) {
                Log.ex( TAG, ioe );
            }
        }

        return result;
    }

    // This handles multiple params but only every gets passed one!
    private static String getPostDataString( Map<String, String> params )
    {
        String result = null;
        try {
            ArrayList<String> pairs = new ArrayList<String>();
            // StringBuilder sb = new StringBuilder();
            // String[] pair = { null, null };
            for ( Map.Entry<String, String> entry : params.entrySet() ){
                pairs.add( URLEncoder.encode( entry.getKey(), "UTF-8" )
                           + "="
                           + URLEncoder.encode( entry.getValue(), "UTF-8" ) );
            }
            result = TextUtils.join( "&", pairs );
        } catch ( java.io.UnsupportedEncodingException uee ) {
            Log.ex( TAG, uee );
        }

        return result;
    }

    private static int sumStrings( final String[] strs )
    {
        int len = 0;
        if ( null != strs ) {
            for ( String str : strs ) {
                len += str.length();
            }
        }
        return len;
    }

}
