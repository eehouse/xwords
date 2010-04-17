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

    private static Context s_context = null;
    private static CommonPrefs s_cp = null;

    public boolean showBoardArrow;
    public boolean showRobotScores;
    public boolean hideTileValues; 
    public boolean skipCommitConfirm;
    public boolean showColors;

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

    private CommonPrefs refresh()
    {
        SharedPreferences sp = 
            PreferenceManager.getDefaultSharedPreferences( s_context );
        String key;

        key = s_context.getString( R.string.key_show_arrow );
        showBoardArrow = sp.getBoolean( key, true );

        key = s_context.getString( R.string.key_explain_robot );
        showRobotScores = sp.getBoolean( key, false );

        key = s_context.getString( R.string.key_hide_values );
        hideTileValues = sp.getBoolean( key, false );

        key = s_context.getString( R.string.key_skip_confirm );
        skipCommitConfirm = sp.getBoolean( key, false );

        key = s_context.getString( R.string.key_color_tiles );
        showColors = sp.getBoolean( key, true );

        int ids[] = { R.string.key_player0,
                      R.string.key_player1,
                      R.string.key_player2,
                      R.string.key_player3,
        };

        for ( int ii = 0; ii < ids.length; ++ii ) {
            playerColors[ii] = prefToColor( sp, ids[ii] );
        }

        int ids2[] = { R.string.key_bonus_l2x,
                       R.string.key_bonus_w2x,
                       R.string.key_bonus_l3x,
                       R.string.key_bonus_w3x,
        };
        for ( int ii = 0; ii < ids2.length; ++ii ) {
            bonusColors[ii+1] = prefToColor( sp, ids2[ii] );
        }

        int idsOther[] = { R.string.key_tile_back,
                           R.string.key_empty,
                           R.string.key_focus,
        };
        for ( int ii = 0; ii < idsOther.length; ++ii ) {
            otherColors[ii] = prefToColor( sp, idsOther[ii] );
        }

        return this;
    }

    private int prefToColor( SharedPreferences sp, int id )
    {
        String key = s_context.getString( id );
        return 0xFF000000 | sp.getInt( key, 0 );
    }

    /*
     * static methods
     */
    public static void setContext( Context context )
    {
        s_context = context;
    }

    public static CommonPrefs get()
    {
        Assert.assertNotNull( s_context );
        if ( null == s_cp ) {
            s_cp = new CommonPrefs();
        }
        return s_cp.refresh();
    }

    public static String getDefaultRelayHost()
    {
        SharedPreferences sp = 
            PreferenceManager.getDefaultSharedPreferences( s_context );
        String key = s_context.getString( R.string.key_relay_host );
        return sp.getString( key, "" );
    }

    public static int getDefaultRelayPort()
    {
        SharedPreferences sp = 
            PreferenceManager.getDefaultSharedPreferences( s_context );
        String key = s_context.getString( R.string.key_relay_port );
        String val = sp.getString( key, "" );
        int result = 0;
        result = Integer.decode( val );
        return result;
    }

    public static String getDefaultDictURL()
    {
        SharedPreferences sp = 
            PreferenceManager.getDefaultSharedPreferences( s_context );
        String key = s_context.getString( R.string.key_dict_host );
        return sp.getString( key, "" );
    }

    public static boolean getVolKeysZoom()
    {
        SharedPreferences sp = 
            PreferenceManager.getDefaultSharedPreferences( s_context );
        String key = s_context.getString( R.string.key_ringer_zoom );
        return sp.getBoolean( key, false );
    }

}
