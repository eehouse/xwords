/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2012 by Eric House (xwords@eehouse.org).  All rights
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
import java.io.File;
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
import org.json.JSONArray;
import org.json.JSONObject;

public class UpdateCheckReceiver extends BroadcastReceiver {

    public static final String NEW_DICT_URL = "NEW_DICT_URL";
    public static final String NEW_DICT_LOC = "NEW_DICT_LOC";

    // weekly
    private static final long INTERVAL_ONEDAY = 1000 * 60 * 60 * 24;
    private static final long INTERVAL_NDAYS = 7;

    // constants that are also used in info.py
    private static final String k_NAME = "name";
    private static final String k_AVERS = "avers";
    private static final String k_GVERS = "gvers";
    private static final String k_INSTALLER = "installer";
    private static final String k_DEVOK = "devOK";
    private static final String k_APP = "app";
    private static final String k_DICTS = "dicts";
    private static final String k_LANG = "lang";
    private static final String k_MD5SUM = "md5sum";
    private static final String k_INDEX = "index";
    private static final String k_URL = "url";
    private static final String k_PARAMS = "params";
    private static final String k_DEVID = "did";

    @Override
    public void onReceive( Context context, Intent intent )
    {
        if ( null != intent && null != intent.getAction() 
             && intent.getAction().equals( Intent.ACTION_BOOT_COMPLETED ) ) {
            restartTimer( context );
        } else {
            checkVersions( context, false );
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

        long interval_millis = INTERVAL_ONEDAY;
        if ( !devOK( context ) ) {
            interval_millis *= INTERVAL_NDAYS;
        }
        interval_millis = (interval_millis / 2)
            + Math.abs(Utils.nextRandomInt() % interval_millis);
        am.setInexactRepeating( AlarmManager.ELAPSED_REALTIME_WAKEUP, 
                                SystemClock.elapsedRealtime() + interval_millis,
                                interval_millis, pi );
    }

    public static void checkVersions( Context context, boolean fromUI ) 
    {
        JSONObject params = new JSONObject();
        PackageManager pm = context.getPackageManager();
        String packageName = context.getPackageName();
        String installer = pm.getInstallerPackageName( packageName );

        if ( "com.google.android.feedback".equals( installer ) 
             || "com.android.vending".equals( installer ) ) { 
            // Do nothing; it's a Market app
        } else {
            try { 
                int versionCode = pm.getPackageInfo( packageName, 0 ).versionCode;

                JSONObject appParams = new JSONObject();

                appParams.put( k_NAME, packageName );
                appParams.put( k_AVERS, versionCode );
                appParams.put( k_GVERS, GitVersion.VERS );
                appParams.put( k_INSTALLER, installer );
                if ( devOK( context ) ) {
                    appParams.put( k_DEVOK, true );
                }
                params.put( k_APP, appParams );
                params.put( k_DEVID, XWPrefs.getDevID( context ) );
            } catch ( PackageManager.NameNotFoundException nnfe ) {
                DbgUtils.loge( nnfe );
            } catch ( org.json.JSONException jse ) {
                DbgUtils.loge( jse );
            }
        }
        JSONArray dictParams = new JSONArray();
        DictUtils.DictAndLoc[] dals = DictUtils.dictList( context );
        for ( int ii = 0; ii < dals.length; ++ii ) {
            DictUtils.DictAndLoc dal = dals[ii];
            switch ( dal.loc ) {
                // case DOWNLOAD:
            case EXTERNAL:
            case INTERNAL:
                dictParams.put( makeDictParams( context, dal, ii ) );
            }
        }
        if ( 0 < dictParams.length() ) {
            try {
                params.put( k_DICTS, dictParams );
                params.put( k_DEVID, XWPrefs.getDevID( context ) );
            } catch ( org.json.JSONException jse ) {
                DbgUtils.loge( jse );
            }
        }

        if ( 0 < params.length() ) {
            HttpPost post = makePost( context, "getUpdates" );
            String json = runPost( post, params );
            if ( null != json ) {
                makeNotificationsIf( context, fromUI, json, pm, packageName, 
                                     dals );
            }
        }
    }

    private static void makeNotificationsIf( Context context, boolean fromUI, 
                                             String jstr, PackageManager pm,
                                             String packageName, 
                                             DictUtils.DictAndLoc[] dals )
    {
        boolean gotOne = false;
        try {
            JSONObject jobj = new JSONObject( jstr );
            if ( null != jobj ) {
                if ( jobj.has( k_APP ) ) {
                    JSONObject app = jobj.getJSONObject( k_APP );
                    if ( app.has( k_URL ) ) {
                        ApplicationInfo ai = pm.getApplicationInfo( packageName, 0);
                        String label = pm.getApplicationLabel( ai ).toString();

                        // If there's a download dir AND an installer
                        // app, handle this ourselves.  Otherwise just
                        // launch the browser
                        boolean useBrowser;
                        File downloads = DictUtils.getDownloadDir( context );
                        if ( null == downloads ) {
                            useBrowser = true;
                        } else {
                            File tmp = new File( downloads, 
                                                 "xx" + XWConstants.APK_EXTN );
                            useBrowser = !Utils.canInstall( context, tmp );
                        }

                        Intent intent;
                        String url = app.getString( k_URL );
                        if ( useBrowser ) {
                            intent = new Intent( Intent.ACTION_VIEW, 
                                                 Uri.parse(url) );
                        } else {
                            intent = new Intent( context, 
                                                 DictImportActivity.class );
                            intent.putExtra( DictImportActivity.APK_EXTRA, url );
                        }

                        String title = 
                            Utils.format( context, R.string.new_app_availf, label );
                        String body = context.getString( R.string.new_app_avail );
                        Utils.postNotification( context, intent, title, body,
                                                url.hashCode() );
                        gotOne = true;
                    }
                }
                if ( jobj.has( k_DICTS ) ) {
                    JSONArray dicts = jobj.getJSONArray( k_DICTS );
                    for ( int ii = 0; ii < dicts.length(); ++ii ) {
                        JSONObject dict = dicts.getJSONObject( ii );
                        if ( dict.has( k_URL ) && dict.has( k_INDEX ) ) {
                            String url = dict.getString( k_URL );
                            int index = dict.getInt( k_INDEX );
                            DictUtils.DictAndLoc dal = dals[index];
                            Intent intent = 
                                new Intent( context, DictsActivity.class );
                            intent.putExtra( NEW_DICT_URL, url );
                            intent.putExtra( NEW_DICT_LOC, dal.loc.ordinal() );
                            String body = 
                                Utils.format( context, R.string.new_dict_availf,
                                              dal.name );
                            Utils.postNotification( context, intent, 
                                                    R.string.new_dict_avail, 
                                                    body, url.hashCode() );
                            gotOne = true;
                        }
                    }
                }
            }
        } catch ( org.json.JSONException jse ) {
            DbgUtils.loge( jse );
        } catch ( PackageManager.NameNotFoundException nnfe ) {
            DbgUtils.loge( nnfe );
        }

        if ( !gotOne && fromUI ) {
            Utils.showToast( context, R.string.checkupdates_none_found );
        }
    }

    private static HttpPost makePost( Context context, String proc )
    {
        String url = String.format( "%s/%s", 
                                    XWPrefs.getDefaultUpdateUrl( context ),
                                    proc );
        return new HttpPost( url );
    }

    private static String runPost( HttpPost post, JSONObject params )
    {
        String result = null;
        try {
            String jsonStr = params.toString();
            List<NameValuePair> nvp = new ArrayList<NameValuePair>();
            nvp.add( new BasicNameValuePair( k_PARAMS, jsonStr ) );
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
        } catch( java.io.UnsupportedEncodingException uee ) {
            DbgUtils.loge( uee );
        } catch( java.io.IOException ioe ) {
            DbgUtils.loge( ioe );
        }
        return result;
    }

    private static JSONObject makeDictParams( Context context, 
                                              DictUtils.DictAndLoc dal, 
                                              int index )
    {
        JSONObject params = new JSONObject();
        int lang = DictLangCache.getDictLangCode( context, dal );
        String langStr = DictLangCache.getLangName( context, lang );
        String sum = DictLangCache.getDictMD5Sum( context, dal.name );
        try {
            params.put( k_NAME, dal.name );
            params.put( k_LANG, langStr );
            params.put( k_MD5SUM, sum );
            params.put( k_INDEX, index );
        } catch( org.json.JSONException jse ) {
            DbgUtils.loge( jse );
        }
        return params;
    }

    private static boolean devOK( Context context )
    {
        return XWPrefs.getPrefsBoolean( context, R.string.key_update_prerel,
                                        false );
    }

}
