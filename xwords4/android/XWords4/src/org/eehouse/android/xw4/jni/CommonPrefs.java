/* -*- compile-command: "cd ../../../../../../; ant install"; -*- */

package org.eehouse.android.xw4.jni;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import org.eehouse.android.xw4.R;
import junit.framework.Assert;

public class CommonPrefs {
    private static Context s_context = null;
    private static CommonPrefs s_cp = null;

    public boolean showBoardArrow;
    public boolean showRobotScores;
    public boolean hideTileValues; 
    public boolean skipCommitConfirm;
    public boolean showColors;

    public int[] playerColors;
    public int[] bonusColors;

    private CommonPrefs()
    {
        playerColors = new int[4];
        bonusColors = new int[5];
        bonusColors[0] = 0xFFFFFFFF; // white
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
                       R.string.key_bonus_l3x,
                       R.string.key_bonus_w2x,
                       R.string.key_bonus_w3x,
        };
        for ( int ii = 0; ii < ids2.length; ++ii ) {
            bonusColors[ii+1] = prefToColor( sp, ids2[ii] );
        }

        

        return this;
    }

    private int prefToColor( SharedPreferences sp, int id )
    {
        String key = s_context.getString( id );
        String val = sp.getString( key, "" );
        return 0xFF000000 | Integer.decode( val );
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
}
