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
import android.content.Intent;
import android.net.Uri;
import android.text.TextUtils;

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
import java.net.InetAddress;
import java.net.Socket;
import java.net.URL;
import java.net.URLEncoder;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import javax.net.ssl.HttpsURLConnection;

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

        @Override
        public void run()
        {
            try {
                JSONArray params = new JSONArray();
                for ( int ii = 0; ii < m_obits.length; ++ii ) {
                    JSONObject one = new JSONObject();
                    one.put( "relayID", m_obits[ii].m_relayID );
                    one.put( "seed", m_obits[ii].m_seed );
                    params.put( one );
                }
                HttpsURLConnection conn = makeHttpsRelayConn( m_context, "kill" );
                String resStr = runConn( conn, params );
                Log.d( TAG, "runViaWeb(): kill(%s) => %s", params, resStr );

                if ( null != resStr ) {
                    JSONObject result = new JSONObject( resStr );
                    if ( 0 == result.optInt( "err", -1 ) ) {
                        DBUtils.clearObits( m_context, m_obits );
                    }
                }
            } catch ( JSONException ex ) {
                Assert.failDbg();
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

    static void showGamePage( Context context, int gameID )
    {
        // Requires a login, so only of use to me right now....
        String url = String.format( "https://eehouse.org/xw4/ui/games?gameid=%d",
                                    gameID );
        Intent intent = new Intent( Intent.ACTION_VIEW, Uri.parse( url ) );
        if ( null != intent.resolveActivity( context.getPackageManager() ) ) {
            context.startActivity( intent );
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

    public static String ensureHttps( String url )
    {
        String result = url.replaceFirst( "^http:", "https:" );
        if ( ! url.equals( result ) ) {
            Log.d( TAG, "ensureHttps(%s) => %s", url, result );
        }
        return result;
    }

    public static void launchWebBrowserWith( Context context, int uriResID )
    {
        String uri = context.getString( uriResID );
        launchWebBrowserWith( context, uri );
    }

    public static void launchWebBrowserWith( Context context, String uri )
    {
        Intent intent = new Intent( Intent.ACTION_VIEW, Uri.parse(uri) );
        context.startActivity( intent );
    }

    protected static HttpsURLConnection makeHttpsRelayConn( Context context,
                                                            String proc )
    {
        String url = XWPrefs.getDefaultRelayUrl( context );
        return makeHttpsConn( context, url, proc );
    }

    protected static HttpsURLConnection makeHttpsMQTTConn( Context context,
                                                           String proc )
    {
        String url = XWPrefs.getDefaultMQTTUrl( context );
        return makeHttpsConn( context, url, proc );
    }

    protected static HttpsURLConnection makeHttpsUpdateConn( Context context,
                                                             String proc )
    {
        String url = XWPrefs.getDefaultUpdateUrl( context );
        return makeHttpsConn( context, url, proc );
    }

    private static HttpsURLConnection makeHttpsConn( Context context,
                                                     String path, String proc )
    {
        HttpsURLConnection result = null;
        try {
            String url = String.format( "%s/%s", ensureHttps( path ), proc );
            result = (HttpsURLConnection)new URL(url).openConnection(); // class cast exception
        } catch ( java.net.MalformedURLException mue ) {
            Assert.assertNull( result );
            Log.ex( TAG, mue );
        } catch ( java.io.IOException ioe ) {
            Assert.assertNull( result );
            Log.ex( TAG, ioe );
        }
        return result;
    }

    protected static String runConn( HttpsURLConnection conn, JSONArray param )
    {
        return runConn( conn, param.toString(), false );
    }

    protected static String runConn( HttpsURLConnection conn, JSONObject param )
    {
        return runConn( conn, param.toString(), false );
    }

    protected static String runConn( HttpsURLConnection conn, JSONObject param,
                                     boolean directJson )
    {
        return runConn( conn, param.toString(), directJson );
    }

    private static String runConn( HttpsURLConnection conn, String param,
                                   boolean directJson )
    {
        String result = null;
        if ( ! directJson ) {
            Map<String, String> params = new HashMap<>();
            params.put( k_PARAMS, param );
            param = getPostDataString( params );
        }

        if ( null != conn && null != param ) {
            try {
                conn.setReadTimeout( 15000 );
                conn.setConnectTimeout( 15000 );
                conn.setRequestMethod( "POST" );
                if ( directJson ) {
                    conn.setRequestProperty("Content-Type", "application/json;charset=UTF-8");
                } else {
                    conn.setFixedLengthStreamingMode( param.length() );
                }
                conn.setDoInput( true );
                conn.setDoOutput( true );

                OutputStream os = conn.getOutputStream();
                BufferedWriter writer
                    = new BufferedWriter(new OutputStreamWriter(os, "UTF-8"));
                writer.write( param );
                writer.flush();
                writer.close();
                os.close();

                int responseCode = conn.getResponseCode();
                if ( HttpsURLConnection.HTTP_OK == responseCode ) {
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
                    Log.w( TAG, "runConn: responseCode: %d/%s for url: %s",
                           responseCode, conn.getResponseMessage(),
                           conn.getURL() );
                    logErrorStream( conn.getErrorStream() );
                }
            } catch ( java.net.ProtocolException pe ) {
                Log.ex( TAG, pe );
            } catch( java.io.IOException ioe ) {
                Log.ex( TAG, ioe );
            }
        } else {
            Log.e( TAG, "not running conn %s with params %s", conn, param );
        }

        return result;
    }

    private static void logErrorStream( InputStream is )
    {
        try {
            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            byte[] buffer = new byte[1024];
            for ( ; ; ) {
                int length = is.read( buffer );
                if ( length == -1 ) {
                    break;
                }
                baos.write( buffer, 0, length );
            }
            Log.e( TAG, baos.toString() );
        } catch (Exception ex) {
            Log.e( TAG, ex.getMessage() );
        }
    }

    // This handles multiple params but only every gets passed one!
    private static String getPostDataString( Map<String, String> params )
    {
        String result = null;
        try {
            ArrayList<String> pairs = new ArrayList<>();
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
