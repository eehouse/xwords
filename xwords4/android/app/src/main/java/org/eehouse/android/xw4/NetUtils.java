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
import android.os.Build;
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
import java.net.HttpURLConnection;
import java.net.InetAddress;
import java.net.Socket;
import java.net.URL;
import java.net.URLEncoder;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

import javax.net.SocketFactory;

import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;

public class NetUtils {
    private static final String TAG = NetUtils.class.getSimpleName();

    public static final String k_PARAMS = "params";

    public static Socket makeProxySocket( Context context,
                                          int timeoutMillis )
    {
        Socket socket = null;
        try {
            int port = XWPrefs.getDefaultProxyPort( context );
            String host = XWPrefs.getHostName( context );

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

    private static String urlForGameID( Context context, int gameID )
    {
        String host = XWPrefs.getPrefsString( context, R.string.key_mqtt_host );
        String myID = XwJNI.dvc_getMQTTDevID();
        // Use the route that doesn't require login
        String url = String.format( "https://%s/xw4/ui/gameinfo?gid16=%X&devid=%s",
                                    host, gameID, myID );
        return url;
    }

    static void gameURLToClip( Context context, int gameID )
    {
        String url = urlForGameID( context, gameID );
        Utils.stringToClip( context, url );
        Utils.showToast( context, R.string.relaypage_url_copied );
    }

    static void copyAndLaunchGamePage( Context context, int gameID )
    {
        // Requires a login, so only of use to me right now....
        String url = urlForGameID( context, gameID );
        Intent intent = new Intent( Intent.ACTION_VIEW, Uri.parse( url ) );
        context.startActivity( intent );
    }

    private static final String FORCE_HOST = null
        // "eehouse.org"
        ;
    public static String forceHost( String host )
    {
        if ( null != FORCE_HOST ) {
            host = FORCE_HOST;
        }
        return host;
    }

    // Pick http or https. SSL is broken on KitKat, and it's well after that
    // that https starts being required. So use http on and before KitKat,
    // just to be safe.
    public static String ensureProto( Context context, String url )
    {
        boolean useHTTPs;
        String dflt = LocUtils.getString( context, R.string.url_scheme_default );
        String pref = XWPrefs.getPrefsString( context, R.string.key_url_scheme, dflt );

        if ( dflt.equals(pref) ) {
            useHTTPs = Build.VERSION.SDK_INT > Build.VERSION_CODES.LOLLIPOP;
        } else if ( LocUtils.getString( context, R.string.url_scheme_http )
                    .equals(pref) ) {
            useHTTPs = false;
        } else {
            Assert.assertTrueNR(LocUtils.getString( context, R.string.url_scheme_https )
                                .equals(pref) );
            useHTTPs = true;
        }

        String result = useHTTPs
            ? url.replaceFirst( "^http:", "https:" )
            : url.replaceFirst( "^https:", "http:" );
        if ( ! url.equals( result ) ) {
            Log.d( TAG, "ensureProto(%s) => %s", url, result );
        }
        return result;
    }

    public static Uri ensureProto( Context context, Uri uri )
    {
        String uriString = ensureProto( context, uri.toString() );
        return Uri.parse( uriString ) ;
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

    public static void sendViaWeb( final Context context, final int resultKey,
                                   final String api, final String jsonParams )
    {
        new Thread( new Runnable() {
                @Override
                public void run() {
                    HttpURLConnection conn = makeHttpMQTTConn( context, api );
                    boolean directJson = true;
                    String result = runConn( conn, jsonParams, directJson );
                    if ( 0 != resultKey ) {
                        XwJNI.dvc_onWebSendResult( resultKey, true, result );
                    }
                }
            } ).start();
    }

    public static HttpURLConnection makeHttpMQTTConn( Context context,
                                                      String proc )
    {
        String url = XWPrefs.getDefaultMQTTUrl( context );
        return makeHttpConn( context, url, proc );
    }

    protected static HttpURLConnection makeHttpUpdateConn( Context context,
                                                           String proc )
    {
        String url = XWPrefs.getDefaultUpdateUrl( context );
        return makeHttpConn( context, url, proc );
    }

    private static HttpURLConnection makeHttpConn( Context context, String path,
                                                   String proc )
    {
        HttpURLConnection result = null;
        try {
            String url = String.format( "%s/%s", ensureProto( context, path ), proc );
            result = (HttpURLConnection)new URL(url).openConnection(); // class cast exception
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
        return runConn( conn, param.toString(), false );
    }

    protected static String runConn( HttpURLConnection conn, JSONObject param )
    {
        return runConn( conn, param.toString(), false );
    }

    public static String runConn( HttpURLConnection conn, JSONObject param,
                                  boolean directJson )
    {
        return runConn( conn, param.toString(), directJson );
    }

    private static String runConn( HttpURLConnection conn, String param,
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
