/* -*- compile-command: "cd ../../../../../../; ant debug install"; -*- */
/*
 * Copyright 2009 - 2011 by Eric House (xwords@eehouse.org).  All
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
import android.preference.PreferenceManager;
import android.graphics.Paint;
import android.content.res.Resources;
import junit.framework.Assert;

import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.DictUtils;

public class CommonPrefs {
    public static final int COLOR_TILE_BACK = 0;
    public static final int COLOR_NOTILE = 1;
    public static final int COLOR_FOCUS = 2;
    public static final int COLOR_BACKGRND = 3;
    public static final int COLOR_BONUSHINT = 4;
    public static final int COLOR_LAST = 5;

    private static CommonPrefs s_cp = null;

    public boolean showBoardArrow;
    public boolean showRobotScores;
    public boolean hideTileValues; 
    public boolean skipCommitConfirm;
    public boolean showColors;
    public boolean sortNewTiles;
    public boolean allowPeek;
    public boolean hideCrosshairs;

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

        int ids[] = { R.string.key_player0,
                      R.string.key_player1,
                      R.string.key_player2,
                      R.string.key_player3,
        };

        for ( int ii = 0; ii < ids.length; ++ii ) {
            playerColors[ii] = prefToColor( context, sp, ids[ii] );
        }

        int ids2[] = { R.string.key_bonus_l2x,
                       R.string.key_bonus_w2x,
                       R.string.key_bonus_l3x,
                       R.string.key_bonus_w3x,
        };
        for ( int ii = 0; ii < ids2.length; ++ii ) {
            bonusColors[ii+1] = prefToColor( context, sp, ids2[ii] );
        }

        int idsOther[] = { R.string.key_tile_back,
                           R.string.key_empty,
                           R.string.key_clr_crosshairs,
                           R.string.key_background,
                           R.string.key_clr_bonushint,
        };
        for ( int ii = 0; ii < idsOther.length; ++ii ) {
            otherColors[ii] = prefToColor( context, sp, idsOther[ii] );
        }

        return this;
    }

    private boolean getBoolean( Context context, SharedPreferences sp, 
                                int id, boolean dflt )
    {
        String key = context.getString( id );
        return sp.getBoolean( key, dflt );
    }

    private int prefToColor( Context context, SharedPreferences sp, int id )
    {
        String key = context.getString( id );
        return 0xFF000000 | sp.getInt( key, 0 );
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

    public static String getDefaultRelayHost( Context context )
    {
        return getString( context, R.string.key_relay_host );
    }

    public static String getDefaultRedirHost( Context context )
    {
        return getString( context, R.string.key_redir_host );
    }

    public static int getDefaultRelayPort( Context context )
    {
        String val = getString( context, R.string.key_relay_port );
        int result = 0;
        try {
            result = Integer.parseInt( val );
        } catch ( Exception ex ) {
        } 
        return result;
    }

    public static int getDefaultProxyPort( Context context )
    {
        String val = getString( context, R.string.key_proxy_port );
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
        return getString( context, R.string.key_dict_host );
    }

    public static boolean getVolKeysZoom( Context context )
    {
        return getPrefsBoolean( context, R.string.key_ringer_zoom, false );
    }

    public static int getDefaultBoardSize( Context context )
    {
        String value = getString( context, R.string.key_board_size );
        int result;
        try {
            result = Integer.parseInt( value.substring( 0, 2 ) );
        } catch ( Exception ex ) {
            result = 15;
        } 
        return result;
    }

    public static int getDefaultPlayerMinutes( Context context )
    {
        String value = getString( context, R.string.key_initial_player_minutes );
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
        String value = getString( context, R.string.key_connect_frequency );
        long result;
        try {
            result = Long.parseLong( value );
        } catch ( Exception ex ) {
            result = -1;
        }
        return result;
    }

    public static String getDefaultHumanDict( Context context )
    {
        String value = getString( context, R.string.key_default_dict );
        if ( value.equals("") || !DictUtils.dictExists( context, value ) ) {
            value = DictUtils.dictList( context )[0].name;
        }
        return value;
    }

    public static String getDefaultRobotDict( Context context )
    {
        String value = getString( context, R.string.key_default_robodict );
        if ( value.equals("") || !DictUtils.dictExists( context, value ) ) {
            value = getDefaultHumanDict( context );
        }
        return value;
    }
    
    public static String getDefaultPlayerName( Context context, int num,
                                               boolean force )
    {
        int id = 0;
        switch( num ) {
        case 0: id = R.string.key_player1_name; break;
        case 1: id = R.string.key_player2_name; break;
        case 2: id = R.string.key_player3_name; break;
        case 3: id = R.string.key_player4_name; break;
        }
        String result = getString( context, id );
        if ( null != result && 0 == result.length() ) {
            result = null;      // be consistent
        }
        if ( force && null == result ) {
            String fmt = context.getString( R.string.playerf );
            result = String.format( fmt, num + 1 );
        }
        return result;
    }

    public static String getDefaultPlayerName( Context context, int num )
    {
        return getDefaultPlayerName( context, num, true );
    }

    public static void setDefaultPlayerName( Context context, String value )
    {
        setPrefsString( context, R.string.key_player1_name, value );
    }

    public static CurGameInfo.XWPhoniesChoice 
        getDefaultPhonies( Context context )
    {
        String value = getString( context, R.string.key_default_phonies );

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

    public static boolean getDefaultHintsAllowed( Context context )
    {
        return getPrefsBoolean( context, R.string.key_init_hintsallowed, 
                                true );
    }

    public static boolean getAutoJuggle( Context context )
    {
        return getPrefsBoolean( context, R.string.key_init_autojuggle, false );
    }

    public static boolean getHideTitleBar( Context context )
    {
        return getPrefsBoolean( context, R.string.key_hide_title, true );
    }

    public static boolean getSoundNotify( Context context )
    {
        return getPrefsBoolean( context, R.string.key_notify_sound, true );
    }

    public static boolean getVibrateNotify( Context context )
    {
        return getPrefsBoolean( context, R.string.key_notify_vibrate, false );
    }

    public static boolean getHideIntro( Context context )
    {
        return getPrefsBoolean( context, R.string.key_hide_intro, false );
    }

    public static boolean getKeepScreenOn( Context context )
    {
        return getPrefsBoolean( context, R.string.key_keep_screenon, false );
    }

    public static String getSummaryField( Context context )
    {
        return getString( context, R.string.key_summary_field );
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

    private static String getString( Context context, int keyID )
    {
        String key = context.getString( keyID );
        SharedPreferences sp = PreferenceManager
            .getDefaultSharedPreferences( context );
        return sp.getString( key, "" );
    }

    private static void setPrefsString( Context context, int keyID, 
                                        String newValue )
    {
        SharedPreferences sp = PreferenceManager
            .getDefaultSharedPreferences( context );
        SharedPreferences.Editor editor = sp.edit();
        String key = context.getString( keyID );
        editor.putString( key, newValue );
        editor.commit();
    }

}
