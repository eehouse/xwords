/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2022 by Eric House (xwords@eehouse.org).  All
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

package org.eehouse.android.xw4.jni;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Build;
import android.preference.PreferenceManager;

import org.eehouse.android.xw4.Assert;
import org.eehouse.android.xw4.DictUtils;
import org.eehouse.android.xw4.Log;
import org.eehouse.android.xw4.NetUtils;
import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.Utils;
import org.eehouse.android.xw4.XWPrefs;
import org.eehouse.android.xw4.XWSumListPreference;
import org.eehouse.android.xw4.loc.LocUtils;

public class CommonPrefs extends XWPrefs {
    private static final String TAG = CommonPrefs.class.getSimpleName();

    // Keep in sync with TileValueType enum in comtypes.h
    public enum TileValueType {
        TVT_FACES(R.string.values_faces),
        TVT_VALUES(R.string.values_values),
        TVT_BOTH(R.string.values_both);

        private int mExplID;
        private TileValueType(int explID) { mExplID = explID ;}
        public int getExpl() { return mExplID; }
    };

    public static final int COLOR_TILE_BACK = 0;
    public static final int COLOR_NOTILE = 1;
    public static final int COLOR_FOCUS = 2;
    public static final int COLOR_BACKGRND = 3;
    public static final int COLOR_BONUSHINT = 4;
    public static final int COLOR_CELLLINE = 5;
    public static final int COLOR_LAST = 6;

    private static CommonPrefs s_cp = null;

    public boolean showBoardArrow;
    public boolean showRobotScores;
    public boolean hideTileValues;
    public boolean skipCommitConfirm;
    public boolean showColors;
    public boolean sortNewTiles;
    public boolean allowPeek;
    public boolean hideCrosshairs;
    public TileValueType tvType;

    public int[] playerColors;
    public int[] bonusColors;
    public int[] otherColors;

    private CommonPrefs()
    {
        playerColors = new int[4];
        bonusColors = new int[5];
        bonusColors[0] = 0xF0F0F0F0; // garbage
        otherColors = new int[COLOR_LAST];
    }

    private CommonPrefs refresh( Context context )
    {
        String key;
        SharedPreferences sp = PreferenceManager
            .getDefaultSharedPreferences( context );

        showBoardArrow = getBoolean( context, sp, R.string.key_show_arrow,
                                     true );
        showRobotScores = getBoolean( context, sp, R.string.key_explain_robot,
                                      false );
        hideTileValues = getBoolean( context, sp, R.string.key_hide_values,
                                     false );
        skipCommitConfirm = getBoolean( context, sp,
                                        R.string.key_skip_confirm, false );
        showColors = getBoolean( context, sp, R.string.key_color_tiles, true );
        sortNewTiles = getBoolean( context, sp, R.string.key_sort_tiles, true );
        allowPeek = getBoolean( context, sp, R.string.key_peek_other, false );
        hideCrosshairs = getBoolean( context, sp, R.string.key_hide_crosshairs, false );

        int ord = getInt(context, sp, R.string.key_tile_valuetype, 0);
        tvType = TileValueType.values()[ord];

        ColorTheme theme = getTheme( context, null );
        String[] colorStrIds = context.getResources().getStringArray( theme.getArrayID() );
        int offset = copyColors( sp, colorStrIds, 0, playerColors, 0 );
        offset += copyColors( sp, colorStrIds, offset, bonusColors, 1 );
        offset += copyColors( sp, colorStrIds, offset, otherColors, 0 );

        return this;
    }

    private int copyColors( SharedPreferences sp, String[] colorStrIds,
                            int idsStart, int[] colors, int colorsStart )
    {
        int nUsed = 0;
        while ( colorsStart < colors.length ) {
            String key = colorStrIds[idsStart + nUsed++];
            int color = 0xFF000000 | sp.getInt( key, 0 );
            colors[colorsStart++] = color;
        }

        return nUsed;
    }

    private boolean getBoolean( Context context, SharedPreferences sp,
                                int id, boolean dflt )
    {
        String key = LocUtils.getString( context, id );
        return sp.getBoolean( key, dflt );
    }

    private int getInt( Context context, SharedPreferences sp,
                        int id, int dflt )
    {
        String key = LocUtils.getString( context, id );
        return sp.getInt( key, dflt );
    }

    /*
     * static methods
     */
    public static CommonPrefs get( Context context )
    {
        if ( null == s_cp ) {
            s_cp = new CommonPrefs();
        }
        return s_cp.refresh( context );
    }

    // Is the OS-level setting on?
    public static boolean darkThemeEnabled( Context context )
    {
        boolean[] fromOS = {false};
        ColorTheme theme = getTheme( context, fromOS );
        boolean result = theme == ColorTheme.DARK && fromOS[0];
        return result;
    }

    private static ColorTheme getTheme( Context context, boolean[] fromOSOut )
    {
        ColorTheme theme = ColorTheme.LIGHT;
        SharedPreferences sp = PreferenceManager
            .getDefaultSharedPreferences( context );
        String which = LocUtils.getString( context, R.string.key_theme_which );
        which = sp.getString( which, null );
        if ( null != which ) {
            try {
                switch ( Integer.parseInt( which ) ) {
                case 0:
                    // do nothing
                    break;
                case 1:
                    theme = ColorTheme.DARK;
                    break;
                case 2:
                    Assert.assertTrueNR( Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q );
                    Resources res = context.getResources();
                    int uiMode = res.getConfiguration().uiMode;
                    if ( Configuration.UI_MODE_NIGHT_YES
                         == (uiMode & Configuration.UI_MODE_NIGHT_MASK) ) {
                        theme = ColorTheme.DARK;
                        if ( null != fromOSOut ) {
                            fromOSOut[0] = true;
                        }
                    }
                    break;
                default:
                    Assert.failDbg();
                }
            } catch ( Exception ex ) {
                // Will happen with old not-an-int saved value
                Log.ex( TAG, ex );
            }
        }
        return theme;
    }

    public static int getDefaultBoardSize( Context context )
    {
        String value = getPrefsString( context, R.string.key_board_size );
        int result;
        try {
            result = Integer.parseInt( value.substring( 0, 2 ) );
        } catch ( Exception ex ) {
            result = 15;
        }
        return result;
    }

    public static String getDefaultHumanDict( Context context )
    {
        String value = getPrefsString( context, R.string.key_default_dict );
        if ( value.equals("") || !DictUtils.dictExists( context, value ) ) {
            value = DictUtils.dictList( context )[0].name;
        }
        return value;
    }

    public static String getDefaultRobotDict( Context context )
    {
        String value = getPrefsString( context, R.string.key_default_robodict );
        if ( value.equals("") || !DictUtils.dictExists( context, value ) ) {
            value = getDefaultHumanDict( context );
        }
        return value;
    }

    public static String getDefaultOriginalPlayerName( Context context,
                                                       int num )
    {
        return LocUtils.getString( context, R.string.player_fmt, num + 1 );
    }

    public static String getDefaultPlayerName( Context context, int num,
                                               boolean force )
    {
        String result = getPrefsString( context, R.string.key_player1_name );
        if ( null != result && 0 == result.length() ) {
            result = null;      // be consistent
        }
        if ( force && null == result ) {
            result = getDefaultOriginalPlayerName( context, num );
        }
        return result;
    }

    public static String getDefaultPlayerName( Context context, int num )
    {
        return getDefaultPlayerName( context, num, true );
    }

    public static String getDefaultRobotName( Context context )
    {
        return getPrefsString( context, R.string.key_robot_name );
    }

    public static void setDefaultPlayerName( Context context, String value )
    {
        setPrefsString( context, R.string.key_player1_name, value );
    }

    public static CurGameInfo.XWPhoniesChoice
        getDefaultPhonies( Context context )
    {
        String value = getPrefsString( context, R.string.key_default_phonies );

        CurGameInfo.XWPhoniesChoice result =
            CurGameInfo.XWPhoniesChoice.PHONIES_IGNORE;
        Resources res = context.getResources();
        String[] names = res.getStringArray( R.array.phony_names );
        for ( int ii = 0; ii < names.length; ++ii ) {
            String name = names[ii];
            if ( name.equals( value ) ) {
                result = CurGameInfo.XWPhoniesChoice.values()[ii];
                break;
            }
        }
        return result;
    }

    public static boolean getDefaultTimerEnabled( Context context )
    {
        return getPrefsBoolean( context, R.string.key_default_timerenabled,
                                false );
    }

    public static boolean getDefaultHintsAllowed( Context context,
                                                  boolean networked )
    {
        int key = networked ?
            R.string.key_init_nethintsallowed : R.string.key_init_hintsallowed;
        return getPrefsBoolean( context, key, true );
    }

    public static boolean getDefaultDupMode( Context context )
    {
        return getPrefsBoolean( context, R.string.key_init_dupmodeon, false );
    }

    public static boolean getDupModeHidden( Context context )
    {
        return !getPrefsBoolean( context, R.string.key_unhide_dupmode, false );
    }

    public static boolean getAutoJuggle( Context context )
    {
        return getPrefsBoolean( context, R.string.key_init_autojuggle, false );
    }

    public static boolean getHideTitleBar( Context context )
    {
        boolean hideByDefault = 11 > Integer.valueOf( Build.VERSION.SDK );
        return getPrefsBoolean( context, R.string.key_hide_title,
                                hideByDefault );
    }

    public static boolean getSoundNotify( Context context )
    {
        return getPrefsBoolean( context, R.string.key_notify_sound, true );
    }

    public static boolean getVibrateNotify( Context context )
    {
        return getPrefsBoolean( context, R.string.key_notify_vibrate, false );
    }

    public static boolean getKeepScreenOn( Context context )
    {
        return getPrefsBoolean( context, R.string.key_keep_screenon, false );
    }

    public static String getSummaryField( Context context )
    {
        return getPrefsString( context, R.string.key_summary_field );
    }

    public static int getSummaryFieldId( Context context )
    {
        int result = 0;
        String str = getSummaryField( context );
        int[] ids = XWSumListPreference.getFieldIDs( context );
        for ( int id : ids ) {
            if ( LocUtils.getString( context, id ).equals( str )){
                result = id;
                break;
            }
        }
        return result;
    }

    public enum ColorTheme {
        LIGHT(R.array.color_ids_light),
        DARK(R.array.color_ids_dark);

        private int mArrayID;
        private ColorTheme(int arrayID) {
            mArrayID = arrayID;
        }
        int getArrayID() { return mArrayID; }
    };
    private static final String THEME_KEY = "theme";

    public static void colorPrefsToClip( Context context, ColorTheme theme )
    {
        String host = LocUtils.getString( context, R.string.invite_host );
        Uri.Builder ub = new Uri.Builder()
            .scheme( "http" )   // PENDING: should be https soon
            .path( String.format( "//%s%s", NetUtils.forceHost( host ),
                                  LocUtils.getString(context, R.string.conf_prefix) ) )
            .appendQueryParameter( THEME_KEY, theme.toString() );

        Resources res = context.getResources();
        String[] urlKeys = res.getStringArray( R.array.color_url_keys );
        String[] dataKeys = res.getStringArray( theme.getArrayID() );
        Assert.assertTrue( urlKeys.length == dataKeys.length );
        SharedPreferences sp = PreferenceManager
            .getDefaultSharedPreferences( context );

        for ( int ii = 0; ii < urlKeys.length; ++ii ) {
            int val = sp.getInt( dataKeys[ii], 0 );
            ub.appendQueryParameter( urlKeys[ii], String.format("%X", val ) );
        }
        String data = ub.build().toString();

        Utils.stringToClip( context, data );
    }

    public static void loadColorPrefs( Context context, Uri uri )
    {
        String themeName = uri.getQueryParameter( THEME_KEY );
        int arrayID = 0;
        for ( ColorTheme theme : ColorTheme.values() ) {
            if ( theme.toString().equals(themeName) ) {
                arrayID = theme.getArrayID();
                break;
            }
        }
        Assert.assertTrueNR( 0 != arrayID );
        if ( 0 != arrayID ) {
            Resources res = context.getResources();
            String[] urlKeys = res.getStringArray( R.array.color_url_keys );
            String[] dataKeys = res.getStringArray( arrayID );
            SharedPreferences sp = PreferenceManager
                .getDefaultSharedPreferences( context );
            SharedPreferences.Editor editor = sp.edit();

            for ( int ii = 0; ii < urlKeys.length; ++ii ) {
                String urlKey = urlKeys[ii];
                try {
                    String val = uri.getQueryParameter( urlKey );
                    editor.putInt( dataKeys[ii], Integer.parseInt( val, 16 ) );
                    Log.d( TAG, "set %s => %s", dataKeys[ii], val ); // here
                } catch ( Exception ex ) {
                    Log.ex( TAG, ex );
                    Log.d( TAG, "bad/missing data for url key: %s", urlKey );
                }
            }
            editor.commit();
        }
    }
}
