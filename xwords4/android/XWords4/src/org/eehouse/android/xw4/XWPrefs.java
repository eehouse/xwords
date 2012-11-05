/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
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
import android.preference.PreferenceManager;
import android.text.TextUtils;
import java.util.ArrayList;
import java.util.ArrayList;

public class XWPrefs {

    public static boolean getSMSEnabled( Context context )
    {
        return getPrefsBoolean( context, R.string.key_enable_sms, false );
    }

    public static boolean getDebugEnabled( Context context )
    {
        return getPrefsBoolean( context, R.string.key_enable_debug, false );
    }

    public static String getDefaultRelayHost( Context context )
    {
        return getPrefsString( context, R.string.key_relay_host );
    }

    public static String getDefaultRedirHost( Context context )
    {
        return getPrefsString( context, R.string.key_redir_host );
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
        return getPrefsString( context, R.string.key_dict_host );
    }

    public static boolean getVolKeysZoom( Context context )
    {
        return getPrefsBoolean( context, R.string.key_ringer_zoom, false );
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

    public static long getProxyInterval( Context context )
    {
        String value = getPrefsString( context, R.string.key_connect_frequency );
        long result;
        try {
            result = Long.parseLong( value );
        } catch ( Exception ex ) {
            result = -1;
        }
        return result;
    }

    public static boolean getPrefsBoolean( Context context, int keyID,
                                           boolean defaultValue )
    {
        String key = context.getString( keyID );
        SharedPreferences sp = PreferenceManager
            .getDefaultSharedPreferences( context );
        return sp.getBoolean( key, defaultValue );
    }

    public static void setPrefsBoolean( Context context, int keyID, 
                                        boolean newValue )
    {
        SharedPreferences sp = PreferenceManager
            .getDefaultSharedPreferences( context );
        SharedPreferences.Editor editor = sp.edit();
        String key = context.getString( keyID );
        editor.putBoolean( key, newValue );
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

    public static void setBTNames( Context context, String[] names )
    {
        setPrefsStringArray( context, R.string.key_bt_names, names );
    }

    public static void setSMSPhones( Context context, String[] names )
    {
        setPrefsStringArray( context, R.string.key_sms_phones, names );
    }

    public static String[] getBTNames( Context context )
    {
        return getPrefsStringArray( context, R.string.key_bt_names );
    }

    public static String[] getSMSPhones( Context context )
    {
        return getPrefsStringArray( context, R.string.key_sms_phones );
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

    public static boolean getDefaultLocInternal( Context context )
    {
        return getPrefsBoolean( context, R.string.key_default_loc, true );
    }

    public static boolean getHaveCheckedSMS( Context context )
    {
        return getPrefsBoolean( context, R.string.key_checked_sms, false );
    }

    public static void setHaveCheckedSMS( Context context, boolean newValue )
    {
        setPrefsBoolean( context, R.string.key_checked_sms, newValue );
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

    protected static String[] getPrefsStringArray( Context context, int keyID )
    {
        String asStr = getPrefsString( context, keyID );
        String[] result = null == asStr ? null : TextUtils.split( asStr, "\n" );
        return result;
    }

    protected static ArrayList<String> getPrefsStringArrayList( Context context, 
                                                                int keyID )
    {
        ArrayList<String> list = new ArrayList<String>();
        String[] strs = getPrefsStringArray( context, keyID );
        if ( null != strs ) {
            for ( int ii = 0; ii < strs.length; ++ii ) {
                list.add( strs[ii] );
            }
        }
        return list;
    }

    protected static void setPrefsStringArray( Context context, int keyID, 
                                               String[] value )
    {
        setPrefsString( context, keyID, TextUtils.join( "\n", value ) );
    }
}