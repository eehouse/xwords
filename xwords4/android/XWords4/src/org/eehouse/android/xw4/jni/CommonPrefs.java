/* -*- compile-command: "cd ../../../../../../; ant install"; -*- */
/*
 * Copyright 2009-2010 by Eric House (xwords@eehouse.org).  All
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

import org.eehouse.android.xw4.Utils;
import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.GameUtils;

public class CommonPrefs {
    public static final int COLOR_TILE_BACK = 0;
    public static final int COLOR_BKGND = 1;
    public static final int COLOR_FOCUS = 2;
    public static final int COLOR_LAST = 3;

    private static CommonPrefs s_cp = null;

    public boolean showBoardArrow;
    public boolean showRobotScores;
    public boolean hideTileValues; 
    public boolean skipCommitConfirm;
    public boolean showColors;
    public boolean sortNewTiles;

    public int[] playerColors;
    public int[] bonusColors;
    public int[] otherColors;

    static {
        Utils.logf( "CommonPrefs class initialized" );
    }

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
                           R.string.key_focus,
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

    public static int getDefaultRelayPort( Context context )
    {
        String val = getString( context, R.string.key_relay_port );
        int result = 0;
        try {
            return Integer.parseInt( val );
        } catch ( Exception ex ) {
            return 0;
        } 
    }

    public static String getDefaultDictURL( Context context )
    {
        return getString( context, R.string.key_dict_host );
    }

    public static boolean getVolKeysZoom( Context context )
    {
        return getBoolean( context, R.string.key_ringer_zoom, false );
    }

    public static int getDefaultBoardSize( Context context )
    {
        String value = getString( context, R.string.key_board_size );
        try {
            return Integer.parseInt( value.substring( 0, 2 ) );
        } catch ( Exception ex ) {
            return 15;
        } 
    }

    public static int getDefaultPlayerMinutes( Context context )
    {
        String value = getString( context, R.string.key_initial_player_minutes );
        try {
            return Integer.parseInt( value );
        } catch ( Exception ex ) {
            return 25;
        }
    }

    public static String getDefaultDict( Context context )
    {
        String value = getString( context, R.string.key_default_dict );
        if ( value.equals("") ) {
            value = GameUtils.dictList( context )[0];
        }
        return value;
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
        return getBoolean( context, R.string.key_default_timerenabled, false );
    }

    public static boolean getHideTitleBar( Context context )
    {
        return getBoolean( context, R.string.key_hide_title, true );
    }

    public static boolean getClickLaunches( Context context )
    {
        return getBoolean( context, R.string.key_click_launches, false );
    }

    private static boolean getBoolean( Context context, int keyID,
                                       boolean defaultValue )
    {
        String key = context.getString( keyID );
        SharedPreferences sp = PreferenceManager
            .getDefaultSharedPreferences( context );
        return sp.getBoolean( key, defaultValue );
    }

    private static String getString( Context context, int keyID )
    {
        String key = context.getString( keyID );
        SharedPreferences sp = PreferenceManager
            .getDefaultSharedPreferences( context );
        return sp.getString( key, "" );
    }

}
