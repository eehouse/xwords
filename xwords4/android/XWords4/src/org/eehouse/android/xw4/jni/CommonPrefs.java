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
import junit.framework.Assert;

import org.eehouse.android.xw4.Utils;
import org.eehouse.android.xw4.R;

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

        key = context.getString( R.string.key_show_arrow );
        showBoardArrow = sp.getBoolean( key, true );

        key = context.getString( R.string.key_explain_robot );
        showRobotScores = sp.getBoolean( key, false );

        key = context.getString( R.string.key_hide_values );
        hideTileValues = sp.getBoolean( key, false );

        key = context.getString( R.string.key_skip_confirm );
        skipCommitConfirm = sp.getBoolean( key, false );

        key = context.getString( R.string.key_color_tiles );
        showColors = sp.getBoolean( key, true );

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
        String key = context.getString( R.string.key_relay_host );
        SharedPreferences sp = PreferenceManager
            .getDefaultSharedPreferences( context );
        return sp.getString( key, "" );
    }

    public static int getDefaultRelayPort( Context context )
    {
        String key = context.getString( R.string.key_relay_port );
        SharedPreferences sp = PreferenceManager
            .getDefaultSharedPreferences( context );
        String val = sp.getString( key, "" );
        int result = 0;
        result = Integer.decode( val );
        return result;
    }

    public static String getDefaultDictURL( Context context )
    {
        String key = context.getString( R.string.key_dict_host );
        SharedPreferences sp = PreferenceManager
            .getDefaultSharedPreferences( context );
        return sp.getString( key, "" );
    }

    public static boolean getVolKeysZoom( Context context )
    {
        String key = context.getString( R.string.key_ringer_zoom );
        SharedPreferences sp = PreferenceManager
            .getDefaultSharedPreferences( context );
        return sp.getBoolean( key, false );
    }

    public static int getDefaultBoardSize( Context context )
    {
        String key = context.getString( R.string.key_board_size );
        SharedPreferences sp = PreferenceManager
            .getDefaultSharedPreferences( context );
        String value = sp.getString( key, "15" );
        return Integer.parseInt( value.substring( 0, 2 ) );
    }

    public static int getDefaultGameMinutes( Context context )
    {
        String key = context.getString( R.string.key_initial_game_minutes );
        SharedPreferences sp = PreferenceManager
            .getDefaultSharedPreferences( context );
        String value = sp.getString( key, "25" );
        Utils.logf( "value for key_initial_game_minutes: %s", value );
        return Integer.parseInt( value );
    }
}
