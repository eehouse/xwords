/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2010 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.SystemClock;
import java.util.ArrayList;
import java.util.List;

import org.apache.http.HttpEntity;
import org.apache.http.HttpResponse;
import org.apache.http.NameValuePair;
import org.apache.http.client.ClientProtocolException;
import org.apache.http.client.HttpClient;
import org.apache.http.client.entity.UrlEncodedFormEntity;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.impl.client.DefaultHttpClient;
import org.apache.http.message.BasicNameValuePair;
import org.apache.http.util.EntityUtils;
import org.json.JSONObject;

public class UpdateCheckReceiver extends BroadcastReceiver {

    public static final String NEW_DICT_URL = "NEW_DICT_URL";
    public static final String NEW_DICT_LOC = "NEW_DICT_LOC";

    // every hourish for now; later should be more like weekly
    private static final long INTERVAL_MILLIS = 1000 * 60 * 60;
    
    @Override
    public void onReceive( Context context, Intent intent )
    {
        DbgUtils.logf( "UpdateCheckReceiver.onReceive()" );
        if ( null != intent && null != intent.getAction() 
             && intent.getAction().equals( Intent.ACTION_BOOT_COMPLETED ) ) {
            restartTimer( context );
        } else {
            checkVersions( context );
            restartTimer( context );
        }
    }

    public static void restartTimer( Context context )
    {
        AlarmManager am =
            (AlarmManager)context.getSystemService( Context.ALARM_SERVICE );

        Intent intent = new Intent( context, UpdateCheckReceiver.class );
        PendingIntent pi = PendingIntent.getBroadcast( context, 0, intent, 0 );
        am.cancel( pi );

        long interval_millis = (INTERVAL_MILLIS / 2)
            + Math.abs(Utils.nextRandomInt() % INTERVAL_MILLIS);
        DbgUtils.logf( "restartTimer: using interval %d (from %d)", interval_millis,
                       INTERVAL_MILLIS );
        long first_millis = SystemClock.elapsedRealtime() + interval_millis;
        am.setInexactRepeating( AlarmManager.ELAPSED_REALTIME_WAKEUP, 
                                first_millis, // first firing
                                interval_millis, pi );
    }

    public static void checkVersions( Context context ) 
    {
        PackageManager pm = context.getPackageManager();
        String packageName = context.getPackageName();
        String installer = pm.getInstallerPackageName( packageName );
        if ( "com.google.android.feedback".equals( installer ) 
             || "com.android.vending".equals( installer ) ) {
            DbgUtils.logf( "checkVersion; skipping market app" );
        } else {
            try { 
                int versionCode = pm.getPackageInfo( packageName, 0 ).versionCode;

                HttpPost post = makePost( context, "curVersion" );

                List<NameValuePair> nvp = new ArrayList<NameValuePair>();
                nvp.add(new BasicNameValuePair( "name", packageName ) );
                nvp.add( new BasicNameValuePair( "avers", 
                                                 String.format( "%d", 
                                                                versionCode ) ) );
                nvp.add( new BasicNameValuePair( "gvers", GitVersion.VERS ) );
                nvp.add( new BasicNameValuePair( "installer", installer ) );
                String json = runPost( post, nvp );
                String url = urlFromJson( json );
                if ( null != url ) {
                    ApplicationInfo ai = pm.getApplicationInfo( packageName, 0);
                    String label = pm.getApplicationLabel( ai ).toString();
                    Intent intent = 
                        new Intent( Intent.ACTION_VIEW, Uri.parse(url) );
                    String title = 
                        Utils.format( context, R.string.new_app_availf, label );
                    String body = context.getString( R.string.new_app_avail );
                    Utils.postNotification( context, intent, title, body,
                                            url.hashCode() );
                }
            } catch ( PackageManager.NameNotFoundException nnfe ) {
                DbgUtils.logf( "checkVersions: %s", nnfe.toString() );
            }
        }

        DictUtils.DictAndLoc[] list = DictUtils.dictList( context );
        for ( DictUtils.DictAndLoc dal : list ) {
            switch ( dal.loc ) {
                // case DOWNLOAD:
            case EXTERNAL:
            case INTERNAL:
                String sum = DictUtils.getMD5SumFor( context, dal );
                String url = checkDictVersion( context, dal, sum );
                if ( null != url ) {
                    Intent intent = new Intent( context, DictsActivity.class );
                    intent.putExtra( NEW_DICT_URL, url );
                    intent.putExtra( NEW_DICT_LOC, dal.loc.ordinal() );
                    String body = 
                        Utils.format( context, R.string.new_dict_availf,
                                      dal.name );
                    Utils.postNotification( context, intent, 
                                            R.string.new_dict_avail, 
                                            body, url.hashCode() );
                }
            }
        }
    }

    private static String urlFromJson( String json )
    {
        String result = null;
        if ( null != json ) {
            try {
                JSONObject jobj = new JSONObject( json );
                if ( null != jobj && jobj.has( "url" ) ) {
                    result = jobj.getString( "url" );
                }
            } catch ( org.json.JSONException jse ) {
                DbgUtils.loge( jse );
            }
        }
        return result;
    }

    private static HttpPost makePost( Context context, String proc )
    {
        String url = String.format( "%s/%s", 
                                    XWPrefs.getDefaultUpdateUrl( context ),
                                    proc );
        return new HttpPost( url );
    }

    private static String runPost( HttpPost post, List<NameValuePair> nvp )
    {
        String result = null;
        try {
            post.setEntity( new UrlEncodedFormEntity(nvp) );

            // Execute HTTP Post Request
            HttpClient httpclient = new DefaultHttpClient();
            HttpResponse response = httpclient.execute(post);
            HttpEntity entity = response.getEntity();
            if ( null != entity ) {
                result = EntityUtils.toString( entity );
                if ( 0 == result.length() ) {
                    result = null;
                }
            }
        } catch ( java.io.UnsupportedEncodingException uee ) {
            DbgUtils.logf( "runPost: %s", uee.toString() );
        } catch ( java.io.IOException ioe ) {
            DbgUtils.logf( "runPost: %s", ioe.toString() );
        }
        return result;
    }

    private static String checkDictVersion( Context context, 
                                            DictUtils.DictAndLoc dal, 
                                            String sum )
    {
        int lang = DictLangCache.getDictLangCode( context, dal );
        String langStr = DictLangCache.getLangName( context, lang );
        HttpPost post = makePost( context, "dictVersion" );
        List<NameValuePair> nvp = new ArrayList<NameValuePair>();
        nvp.add( new BasicNameValuePair( "name", dal.name ) );
        nvp.add( new BasicNameValuePair( "lang", langStr ) );
        nvp.add( new BasicNameValuePair( "md5sum", sum ) );
        String json = runPost( post, nvp );
        return urlFromJson( json );
    }

}
