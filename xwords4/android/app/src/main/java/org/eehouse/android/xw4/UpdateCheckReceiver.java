/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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
import android.os.AsyncTask;
import android.os.SystemClock;

import java.io.File;
import java.util.List;
import javax.net.ssl.HttpsURLConnection;
import org.json.JSONArray;
import org.json.JSONObject;

import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;

public class UpdateCheckReceiver extends BroadcastReceiver {
    private static final String TAG = UpdateCheckReceiver.class.getSimpleName();
    private static final boolean LOG_QUERIES = false;

    public static final String NEW_DICT_URL = "NEW_DICT_URL";
    public static final String NEW_DICT_LOC = "NEW_DICT_LOC";
    public static final String NEW_DICT_NAME = "NEW_DICT_NAME";
    public static final String NEW_XLATION_CBK = "NEW_XLATION_CBK";

    private static final long INTERVAL_ONEHOUR = 1000 * 60 * 60;
    private static final long INTERVAL_ONEDAY = INTERVAL_ONEHOUR * 24;
    private static final long INTERVAL_NDAYS = 1;
    private static final String KEY_PREV_URL = "PREV_URL";

    // constants that are also used in info.py
    private static final String k_NAME = "name";
    private static final String k_AVERS = "avers";
    private static final String k_VARIANT = "variant";
    private static final String k_GVERS = "gvers";
    private static final String k_INSTALLER = "installer";
    private static final String k_DEVOK = "devOK";
    private static final String k_APP = "app";
    private static final String k_DICTS = "dicts";
    private static final String k_LANG = "lang";
    private static final String k_LANGCODE = "lc";
    private static final String k_MD5SUM = "md5sum";
    private static final String k_FULLSUM = "fullsum";
    private static final String k_INDEX = "index";
    private static final String k_LEN = "len";
    private static final String k_URL = "url";
    private static final String k_MQTTDEVID = "devid";
    private static final String k_DEBUG = "dbg";
    private static final String k_XLATEINFO = "xlatinfo";
    private static final String k_UPGRADE_TITLE = "title";
    private static final String k_UPGRADE_BODY = "body";

    @Override
    public void onReceive( Context context, Intent intent )
    {
        if ( null != intent
             && null != intent.getAction()
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

        long interval_millis;
        if ( BuildConfig.NON_RELEASE ) {
            interval_millis = INTERVAL_ONEHOUR;
        } else {
            interval_millis = INTERVAL_ONEDAY;
            if ( !devOK( context ) ) {
                interval_millis *= INTERVAL_NDAYS;
            }
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
        String packageName = BuildConfig.APPLICATION_ID;
        int versionCode;
        try {
            versionCode = pm.getPackageInfo( packageName, 0 ).versionCode;
        } catch ( PackageManager.NameNotFoundException nnfe ) {
            Log.ex( TAG, nnfe );
            versionCode = 0;
        }

        // App update
        if ( BuildConfig.FOR_FDROID ) {
            // Do nothing; can't upgrade app
        } else {
            String installer = pm.getInstallerPackageName( packageName );
            if ( null == installer ) {
                installer = "none";
            }

            try {
                JSONObject appParams = new JSONObject();

                appParams.put( k_VARIANT, BuildConfig.VARIANT_CODE );
                appParams.put( k_AVERS, versionCode );
                appParams.put( k_GVERS, BuildConfig.GIT_REV );
                appParams.put( k_INSTALLER, installer );
                if ( devOK( context ) ) {
                    appParams.put( k_DEVOK, true );
                }
                appParams.put( k_DEBUG, BuildConfig.DEBUG );
                params.put( k_APP, appParams );

                String devID = XwJNI.dvc_getMQTTDevID( null );
                params.put( k_MQTTDEVID, devID );
            } catch ( org.json.JSONException jse ) {
                Log.ex( TAG, jse );
            }
        }

        // Dict update
        DictUtils.DictAndLoc[] dals = getDownloadedDicts( context );
        if ( null != dals ) {
            JSONArray dictParams = new JSONArray();
            for ( int ii = 0; ii < dals.length; ++ii ) {
                dictParams.put( makeDictParams( context, dals[ii], ii ) );
            }
            try {
                params.put( k_DICTS, dictParams );
            } catch ( org.json.JSONException jse ) {
                Log.ex( TAG, jse );
            }
        }

        if ( 0 < params.length() ) {
            try {
                params.put( k_NAME, packageName );
                params.put( k_AVERS, versionCode );
                if ( LOG_QUERIES ) {
                    Log.d( TAG, "checkVersions(): sending: %s", params );
                }
                new UpdateQueryTask( context, params, fromUI, pm,
                                     packageName, dals ).execute();
            } catch ( org.json.JSONException jse ) {
                Log.ex( TAG, jse );
            }
        }
    }

    private static DictUtils.DictAndLoc[] getDownloadedDicts( Context context )
    {
        DictUtils.DictAndLoc[] result = null;
        DictUtils.DictAndLoc[] dals = DictUtils.dictList( context );
        DictUtils.DictAndLoc[] tmp = new DictUtils.DictAndLoc[dals.length];
        int indx = 0;
        for ( int ii = 0; ii < dals.length; ++ii ) {
            DictUtils.DictAndLoc dal = dals[ii];
            switch ( dal.loc ) {
                // case DOWNLOAD:
            case EXTERNAL:
            case INTERNAL:
                tmp[indx++] = dal;
                break;
            }
        }

        if ( 0 < indx ) {
            result = new DictUtils.DictAndLoc[indx];
            System.arraycopy( tmp, 0, result, 0, indx );
        }
        return result;
    }

    private static JSONObject makeDictParams( Context context,
                                              DictUtils.DictAndLoc dal,
                                              int index )
    {
        JSONObject params = new JSONObject();
        int lang = DictLangCache.getDictLangCode( context, dal );
        String langCode = DictLangCache.getLangCodeStr( context, lang );
        String langStr = DictLangCache.getLangName( context, lang );
        String sum = DictLangCache.getDictMD5Sum( context, dal.name );
        String fullSum = DictLangCache.getDictFullSum( context, dal.name );
        Assert.assertTrueNR( null != fullSum );
        long len = DictLangCache.getFileLen( context, dal );
        try {
            params.put( k_NAME, dal.name );
            params.put( k_LANG, langStr );
            params.put( k_LANGCODE, langCode );
            params.put( k_MD5SUM, sum );
            params.put( k_FULLSUM, fullSum );
            params.put( k_INDEX, index );
            params.put( k_LEN, len );
        } catch( org.json.JSONException jse ) {
            Log.ex( TAG, jse );
        }
        return params;
    }

    private static boolean devOK( Context context )
    {
        return XWPrefs.getPrefsBoolean( context, R.string.key_update_prerel,
                                        false );
    }

    private static class UpdateQueryTask extends AsyncTask<Void, Void, String> {
        private Context m_context;
        private JSONObject m_params;
        private boolean m_fromUI;
        private PackageManager m_pm;
        private String m_packageName;
        private DictUtils.DictAndLoc[] m_dals;

        public UpdateQueryTask( Context context, JSONObject params,
                                boolean fromUI, PackageManager pm,
                                String packageName,
                                DictUtils.DictAndLoc[] dals )
        {
            m_context = context;
            m_params = params;
            m_fromUI = fromUI;
            m_pm = pm;
            m_packageName = packageName;
            m_dals = dals;
        }

        @Override
        protected String doInBackground( Void... unused )
        {
            HttpsURLConnection conn
                = NetUtils.makeHttpsUpdateConn( m_context, "getUpdates" );
            String json = null;
            if ( null != conn ) {
                json = NetUtils.runConn( conn, m_params );
            }
            return json;
        }

        @Override
        protected void onPostExecute( String json )
        {
            if ( null != json ) {
                if ( LOG_QUERIES ) {
                    Log.d( TAG, "onPostExecute(): received: %s", json );
                }
                makeNotificationsIf( json );
                XWPrefs.setHaveCheckedUpgrades( m_context, true );
            }
        }

        private void makeNotificationsIf( String jstr )
        {
            boolean gotOne = false;
            try {
                // Log.d( TAG, "makeNotificationsIf(response=%s)", jstr );
                JSONObject jobj = new JSONObject( jstr );
                if ( null != jobj ) {

                    // Add upgrade
                    if ( jobj.has( k_APP ) ) {
                        JSONObject app = jobj.getJSONObject( k_APP );
                        if ( app.has( k_URL ) ) {
                            ApplicationInfo ai =
                                m_pm.getApplicationInfo( m_packageName, 0);
                            String label = m_pm.getApplicationLabel( ai ).toString();

                            // If there's a download dir AND an installer
                            // app, handle this ourselves.  Otherwise just
                            // launch the browser
                            boolean useBrowser;
                            File downloads = DictUtils.getDownloadDir( m_context );
                            if ( null == downloads ) {
                                useBrowser = true;
                            } else {
                                File tmp = new File( downloads,
                                                     "xx" + XWConstants.APK_EXTN );
                                useBrowser = !Utils.canInstall( m_context, tmp );
                            }

                            String urlParm = app.getString( k_URL );

                            // Debug builds check frequently on a timer and
                            // when it's something we don't want it's annoying
                            // to get a lot of offers. So track the URL used,
                            // and only offer once per URL unless the request
                            // was manual.
                            boolean skipIt = false;
                            if ( BuildConfig.NON_RELEASE && !m_fromUI ) {
                                String prevURL = DBUtils.getStringFor( m_context, KEY_PREV_URL );
                                if ( urlParm.equals( prevURL ) ) {
                                    skipIt = true;
                                } else {
                                    DBUtils.setStringFor( m_context, KEY_PREV_URL, urlParm );
                                }
                            }
                            if ( !skipIt ) {
                                gotOne = true;
                                String url = NetUtils.ensureHttps( urlParm );
                                Intent intent;
                                if ( useBrowser ) {
                                    intent = new Intent( Intent.ACTION_VIEW,
                                                         Uri.parse(url) );
                                } else {
                                    intent = DwnldDelegate
                                        .makeAppDownloadIntent( m_context, url );
                                }

                                // If I asked explicitly, let's download now.
                                if ( m_fromUI && !useBrowser ) {
                                    m_context.startActivity( intent );
                                } else {
                                    // title and/or body might be in the reply
                                    String title = app.optString( k_UPGRADE_TITLE, null );
                                    if ( null == title ) {
                                        title = LocUtils
                                            .getString( m_context, R.string.new_app_avail_fmt, label );
                                    }
                                    String body = app.optString( k_UPGRADE_BODY, null );
                                    if ( null == body ) {
                                        body = LocUtils
                                            .getString( m_context, R.string.new_app_avail );
                                    }
                                    Utils.postNotification( m_context, intent, title,
                                                            body, title.hashCode() );
                                }
                            }
                        }
                    }

                    // dictionaries upgrade
                    if ( jobj.has( k_DICTS ) ) {
                        JSONArray dicts = jobj.getJSONArray( k_DICTS );
                        for ( int ii = 0; ii < dicts.length(); ++ii ) {
                            JSONObject dict = dicts.getJSONObject( ii );
                            if ( dict.has( k_URL ) && dict.has( k_INDEX ) ) {
                                String url = dict.getString( k_URL );
                                int index = dict.getInt( k_INDEX );
                                DictUtils.DictAndLoc dal = m_dals[index];
                                postDictNotification( m_context, url,
                                                      dal.name, dal.loc, true );
                                gotOne = true;
                            }
                        }
                    }
                }
            } catch ( org.json.JSONException jse ) {
                Log.ex( TAG, jse );
                Log.w( TAG, "sent: \"%s\"", m_params.toString() );
                Log.w( TAG, "received: \"%s\"", jstr );
            } catch ( PackageManager.NameNotFoundException nnfe ) {
                Log.ex( TAG, nnfe );
            }

            if ( !gotOne && m_fromUI ) {
                Utils.showToast( m_context, R.string.checkupdates_none_found );
            }
        }
    }

    private static void postDictNotification( Context context, String url,
                                              String name, DictUtils.DictLoc loc,
                                              boolean isUpdate )
    {
        Intent intent = new Intent( context, MainActivity.class ); // PENDING TEST THIS!!!
        intent.putExtra( NEW_DICT_URL, url );
        intent.putExtra( NEW_DICT_NAME, name );
        intent.putExtra( NEW_DICT_LOC, loc.ordinal() );

        int strID = isUpdate ? R.string.new_dict_avail_fmt
            : R.string.dict_avail_fmt;
        String body = LocUtils.getString( context, strID, name );

        Utils.postNotification( context, intent,
                                R.string.new_dict_avail,
                                body, url.hashCode() );
    }

    static boolean postedForDictDownload( Context context, final Uri uri,
                                          DwnldDelegate.DownloadFinishedListener dfl )
    {
        String durl = uri.getQueryParameter( "durl" );
        boolean isDownloadURI = null != durl;
        if ( isDownloadURI ) {
            String name = uri.getQueryParameter( "name" );
            Assert.assertTrueNR( null != name ); // derive from uri?
            Uri dictUri = Uri.parse( durl );
            List<String> segs = dictUri.getPathSegments();
            if ( "byod".equals( segs.get(0) ) ) {
                DwnldDelegate
                    .downloadDictInBack( context, dictUri, name, dfl );
            } else {
                postDictNotification( context, durl, name,
                                      DictUtils.DictLoc.INTERNAL, false );
            }
        }
        // Log.d( TAG, "postDictNotification(%s) => %b", uri, isDownloadURI );
        return isDownloadURI;
    }
}
