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
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.preference.PreferenceManager;
import android.text.TextUtils;

import org.json.JSONException;
import org.json.JSONObject;


import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;

public class XWPrefs {
    private static final String TAG = XWPrefs.class.getSimpleName();

    // No reason to put this in xml if they're private to this file!
    private static final String key_checked_upgrades = "key_checked_upgrades";

    public static boolean getNBSEnabled( Context context )
    {
        boolean haveNative = Perms23.haveNativePerms();
        return haveNative || getPrefsBoolean( context, R.string.key_enable_nbs, false );
    }

    public static void setNBSEnabled( Context context, boolean enabled )
    {
        Assert.assertTrue( !Perms23.haveNativePerms() || !BuildConfig.DEBUG );
        setPrefsBoolean( context, R.string.key_enable_nbs, enabled );
    }

    public static boolean getDebugEnabled( Context context )
    {
        return getPrefsBoolean( context, R.string.key_enable_debug,
                                BuildConfig.DEBUG );
    }

    public static boolean moveCountEnabled( Context context )
    {
        return getPrefsBoolean( context, R.string.key_enable_pending_count,
                                BuildConfig.DEBUG );
    }

    public static boolean getIgnoreFCM( Context context )
    {
        String curValue =
            XWPrefs.getPrefsString( context, R.string.key_relay_poll );
        String noFCMString = context.getString(R.string.relay_poll_name_no_fcm);
        boolean result = noFCMString.equals( curValue );
        // Log.d( TAG, "getIgnoreFCM() => %b (%s vs %s)", result,
        //        curValue, noFCMString );
        return result;
    }

    // Not used yet
    public static boolean getFCMOnly( Context context )
    {
        String curValue =
            XWPrefs.getPrefsString( context, R.string.key_relay_poll );
        String FCMOnlyString = context
            .getString( R.string.relay_poll_name_no_polling );
        boolean result = FCMOnlyString.equals( curValue );
        Log.d( TAG, "getFCMOnly() => %b (%s vs %s)", result,
               curValue, FCMOnlyString );
        return result;
    }

    public static boolean getToastFCM( Context context )
    {
        return getPrefsBoolean( context, R.string.key_show_fcm, BuildConfig.DEBUG );
    }

    public static boolean getSMSToSelfEnabled( Context context )
    {
        return getPrefsBoolean( context, R.string.key_enable_sms_toself, false );
    }

    public static boolean getHideNewgameButtons( Context context )
    {
        return getPrefsBoolean( context, R.string.key_hide_newgames,
                                false );
    }

    public static boolean getPublicRoomsEnabled( Context context )
    {
        return getPrefsBoolean( context, R.string.key_enable_pubroom, false );
    }

    public static void setHideNewgameButtons( Context context, boolean set )
    {
        setPrefsBoolean( context, R.string.key_hide_newgames, set );
    }

    public static String getDefaultRelayHost( Context context )
    {
        String host = getPrefsString( context, R.string.key_relay_host );
        return NetUtils.forceHost( host );
    }

    public static int getDefaultRelayPort( Context context )
    {
        String val = getPrefsString( context, R.string.key_relay_port );
        int result = 0;
        try {
            result = Integer.parseInt( val );
        } catch ( Exception ex ) {
        }
        return result;
    }

    public static String getDefaultUpdateUrl( Context context )
    {
        return getPrefsString( context, R.string.key_update_url );
    }

    public static String getDefaultRelayUrl( Context context )
    {
        String result = getPrefsString( context, R.string.key_relay_url );
        if ( result == null || 0 == result.length() ) {
            result = context.getString( R.string.default_relay_url );
        }
        return result;
    }

    public static String getDefaultMQTTUrl( Context context )
    {
        String result = getPrefsString( context, R.string.key_mqtt_url );
        if ( result == null || 0 == result.length() ) {
            result = context.getString( R.string.default_mqtt_url );
        }
        return result;
    }

    public static boolean getRelayEnabled( Context context )
    {
        boolean enabled = ! getPrefsBoolean( context, R.string.key_disable_relay,
                                             false );
        // Log.d( TAG, "getRelayEnabled() => %b", enabled );
        return enabled;
    }

    public static boolean getSkipToWebAPI( Context context )
    {
        return getPrefsBoolean( context, R.string.key_relay_via_http_first, false );
    }

    public static int getDefaultProxyPort( Context context )
    {
        String val = getPrefsString( context, R.string.key_proxy_port );
        int result = 0;
        try {
            result = Integer.parseInt( val );
        } catch ( Exception ex ) {
        }
        // DbgUtils.logf( "getDefaultProxyPort=>%d", result );
        return result;
    }

    public static String getDefaultDictURL( Context context )
    {
        return getPrefsString( context, R.string.key_dict_host4 );
    }

    public static boolean getSquareTiles( Context context )
    {
        return getPrefsBoolean( context, R.string.key_square_tiles, false );
    }

    public static int getDefaultPlayerMinutes( Context context )
    {
        String value =
            getPrefsString( context, R.string.key_initial_player_minutes );
        int result;
        try {
            result = Integer.parseInt( value );
        } catch ( Exception ex ) {
            result = 25;
        }
        return result;
    }

    public static int getPrefsInt( Context context, int keyID, int defaultValue )
    {
        int result = defaultValue;
        if ( null != context ) {
            String key = context.getString( keyID );
            SharedPreferences sp = PreferenceManager
                .getDefaultSharedPreferences( context );
            try {
                result = sp.getInt( key, defaultValue );
                // If it's in a pref, it'll be a string (editable) So will get CCE
            } catch ( ClassCastException cce ) {
                String asStr = sp.getString( key, String.format( "%d", defaultValue ) );
                try {
                    result = Integer.parseInt( asStr );
                } catch ( Exception ex ) {
                }
            }
        }
        return result;
    }

    public static void setPrefsInt( Context context, int keyID, int newValue )
    {
        SharedPreferences sp = PreferenceManager
            .getDefaultSharedPreferences( context );
        SharedPreferences.Editor editor = sp.edit();
        String key = context.getString( keyID );
        editor.putInt( key, newValue );
        editor.commit();
    }

    public static boolean getPrefsBoolean( Context context, int keyID,
                                           boolean defaultValue )
    {
        String key = context.getString( keyID );
        return getPrefsBoolean( context, key, defaultValue );
    }

    private static boolean getPrefsBoolean( Context context, String key,
                                            boolean defaultValue )
    {
        SharedPreferences sp = PreferenceManager
            .getDefaultSharedPreferences( context );
        return sp.getBoolean( key, defaultValue );
    }

    public static void setPrefsBoolean( Context context, int keyID,
                                        boolean newValue )
    {
        String key = context.getString( keyID );
        setPrefsBoolean( context, key, newValue );
    }

    private static void setPrefsBoolean( Context context, String key,
                                         boolean newValue )
    {
        SharedPreferences sp = PreferenceManager
            .getDefaultSharedPreferences( context );
        SharedPreferences.Editor editor = sp.edit();
        editor.putBoolean( key, newValue );
        editor.commit();
    }

    public static long getPrefsLong( Context context, int keyID,
                                     long defaultValue )
    {
        String key = context.getString( keyID );
        SharedPreferences sp = PreferenceManager
            .getDefaultSharedPreferences( context );
        return sp.getLong( key, defaultValue );
    }

    public static void setPrefsLong( Context context, int keyID, long newVal )
    {
        SharedPreferences sp = PreferenceManager
            .getDefaultSharedPreferences( context );
        SharedPreferences.Editor editor = sp.edit();
        String key = context.getString( keyID );
        editor.putLong( key, newVal );
        editor.commit();
    }

    public static void setClosedLangs( Context context, String[] langs )
    {
        setPrefsString( context, R.string.key_closed_langs,
                        TextUtils.join( "\n", langs ) );
    }

    public static String[] getClosedLangs( Context context )
    {
        return getPrefsStringArray( context, R.string.key_closed_langs );
    }

    public static void setSMSPhones( Context context, JSONObject phones )
    {
        setPrefsString( context, R.string.key_sms_phones, phones.toString() );
    }

    public static JSONObject getSMSPhones( Context context )
    {
        String asStr = getPrefsString( context, R.string.key_sms_phones );
        JSONObject obj = null;

        if ( null != asStr ) {
            try {
                obj = new JSONObject( asStr );
            } catch ( JSONException ex ) {
                obj = null;
            }
        }

        if ( null == obj ) {
            obj = new JSONObject();
            if ( null != asStr ) {
                String[] numbers = TextUtils.split( asStr, "\n" );
                for ( String number : numbers ) {
                    try {
                        obj.put( number, "" ); // null removes any entry
                    } catch ( JSONException ex ) {
                        Log.ex( TAG, ex );
                    }
                }
            }
        }

        // Log.d( TAG, "getSMSPhones() => %s", obj.toString() );
        return obj;
    }

    public static void setBTAddresses( Context context, String[] addrs )
    {
        setPrefsStringArray( context, R.string.key_bt_addrs, addrs );
    }

    public static String[] getBTAddresses( Context context )
    {
        return getPrefsStringArray( context, R.string.key_bt_addrs );
    }

    public static String getDevID( Context context )
    {
        String id = getPrefsString( context, R.string.key_dev_id );
        if ( null == id || 0 == id.length() ) {
            id = String.format( "%08X-%08X", Utils.nextRandomInt(),
                                Utils.nextRandomInt() );
            setPrefsString( context, R.string.key_dev_id, id );
        }
        return id;
    }

    public static DictUtils.DictLoc getDefaultLoc( Context context )
    {
        boolean internal = getDefaultLocInternal( context );
        DictUtils.DictLoc result = internal ? DictUtils.DictLoc.INTERNAL
            : DictUtils.DictLoc.EXTERNAL;
        return result;
    }

    public static String getMyDownloadDir( Context context )
    {
        return getPrefsString( context, R.string.key_download_path );
    }

    public static boolean getDefaultLocInternal( Context context )
    {
        return getPrefsBoolean( context, R.string.key_default_loc, true );
    }

    public static long getDefaultNewGameGroup( Context context )
    {
        long groupID = getPrefsLong( context, R.string.key_default_group,
                                     DBUtils.GROUPID_UNSPEC );
        if ( DBUtils.GROUPID_UNSPEC == groupID ) {
            groupID = DBUtils.getAnyGroup( context );
            setPrefsLong( context, R.string.key_default_group, groupID );
        }
        Assert.assertTrue( DBUtils.GROUPID_UNSPEC != groupID );
        return groupID;
    }

    public static void setDefaultNewGameGroup( Context context, long val )
    {
        Assert.assertTrue( DBUtils.GROUPID_UNSPEC != val );
        setPrefsLong( context, R.string.key_default_group, val );
    }

    public static boolean getThumbEnabled( Context context )
    {
        return 0 < getThumbPct( context );
    }

    public static int getThumbPct( Context context )
    {
        String pct = getPrefsString( context, R.string.key_thumbsize );
        int result;
        if ( context.getString( R.string.thumb_off ).equals( pct ) ) {
            result = 0;
        } else {
            try {
                String suffix = context.getString( R.string.pct_suffix );
                result = Integer.parseInt( pct.substring( 0, pct.length()
                                                          - suffix.length() ) );
            } catch (Exception ex ) {
                result = 30;
            }
        }
        return result;
    }

    public static boolean getStudyEnabled( Context context )
    {
        return getPrefsBoolean( context, R.string.key_studyon, true );
    }

    protected static String getPrefsString( Context context, int keyID )
    {
        String key = context.getString( keyID );
        SharedPreferences sp = PreferenceManager
            .getDefaultSharedPreferences( context );
        return sp.getString( key, "" );
    }

    protected static void setPrefsString( Context context, int keyID,
                                          String newValue )
    {
        SharedPreferences sp = PreferenceManager
            .getDefaultSharedPreferences( context );
        SharedPreferences.Editor editor = sp.edit();
        String key = context.getString( keyID );
        editor.putString( key, newValue );
        editor.commit();
    }

    protected static void clearPrefsKey( Context context, int keyID )
    {
        SharedPreferences sp = PreferenceManager
            .getDefaultSharedPreferences( context );
        SharedPreferences.Editor editor = sp.edit();
        String key = context.getString( keyID );
        editor.remove( key );
        editor.commit();
    }

    protected static String[] getPrefsStringArray( Context context, int keyID )
    {
        String asStr = getPrefsString( context, keyID );
        String[] result = null == asStr ? null : TextUtils.split( asStr, "\n" );
        return result;
    }

    protected static void setPrefsStringArray( Context context, int keyID,
                                               String[] value )
    {
        setPrefsString( context, keyID, TextUtils.join( "\n", value ) );
    }

    public static String getFakeLocale( Context context )
    {
        return getPrefsString( context, R.string.key_xlations_locale );
    }

    public static boolean getXlationEnabled( Context context )
    {
        return getPrefsBoolean( context, R.string.key_xlations_enabled, false );
    }

    public static void setHaveCheckedUpgrades( Context context, boolean haveChecked )
    {
        setPrefsBoolean( context, key_checked_upgrades, haveChecked );
    }

    public static boolean getHaveCheckedUpgrades( Context context )
    {
        return getPrefsBoolean( context, key_checked_upgrades, false );
    }

    public static boolean getCanInviteMulti( Context context )
    {
        return getPrefsBoolean( context, R.string.key_invite_multi, false );
    }

    public static boolean getIsTablet( Context context )
    {
        boolean result = isTablet( context );
        String setting = getPrefsString( context, R.string.key_force_tablet );
        if ( setting.equals( context.getString(R.string.force_tablet_default) ) ) {
            // Leave it alone
        } else if ( setting.equals( context.getString(R.string.force_tablet_tablet) ) ) {
            result = true;
        } else if ( setting.equals( context.getString(R.string.force_tablet_phone) ) ) {
            result = false;
        }

        // Log.d( TAG, "getIsTablet() => %b (got %s)", result, setting );
        return result;
    }

    public static CommsConnTypeSet getAddrTypes( Context context )
    {
        CommsConnTypeSet result;
        int flags = getPrefsInt( context, R.string.key_addrs_pref, -1 );
        if ( -1 == flags ) {
            result = new CommsConnTypeSet();
            if ( getRelayEnabled( context ) ) {
                result.addWithCheck( CommsConnType.COMMS_CONN_RELAY );
            }
            if ( BTUtils.BTEnabled() ) {
                result.add( CommsConnType.COMMS_CONN_BT );
            }
        } else {
            result = new CommsConnTypeSet( flags );
        }

        // Save it if changed
        int siz = result.size();
        CommsConnTypeSet.removeUnsupported( context, result );
        if ( result.size() != siz ) {
            setAddrTypes( context, result );
        }

        // Log.d( TAG, "getAddrTypes() => %s", result.toString( context, false) );
        return result;
    }

    public static void setAddrTypes( Context context, CommsConnTypeSet set )
    {
        int flags = set.toInt();
        setPrefsInt( context, R.string.key_addrs_pref, flags );
    }

    private static Boolean s_isTablet = null;
    private static boolean isTablet( Context context )
    {
        if ( null == s_isTablet ) {
            int screenLayout =
                context.getResources().getConfiguration().screenLayout;
            int size = screenLayout & Configuration.SCREENLAYOUT_SIZE_MASK;
            s_isTablet =
                new Boolean(Configuration.SCREENLAYOUT_SIZE_LARGE <= size);
        }
        return s_isTablet;
    }
}
