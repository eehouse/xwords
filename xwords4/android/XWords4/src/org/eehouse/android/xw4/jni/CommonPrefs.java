/* -*- compile-command: "cd ../../../../../../; ant reinstall"; -*- */

package org.eehouse.android.xw4.jni;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import org.eehouse.android.xw4.R;
import junit.framework.Assert;

public class CommonPrefs {
    private static Context s_context = null;

    public boolean showBoardArrow;
    public boolean showRobotScores;
    public boolean hideTileValues; 
    public boolean skipCommitConfirm;
    public boolean showColors;

    public CommonPrefs( Context context )
    {
        if ( s_context == null ) {
            s_context = context;
        }

        SharedPreferences sp = 
            PreferenceManager.getDefaultSharedPreferences( s_context );
        String str;

        str = s_context.getString( R.string.key_show_arrow );
        showBoardArrow = sp.getBoolean( str, true );

        str = s_context.getString( R.string.key_explain_robot );
        showRobotScores = sp.getBoolean( str, false );

        str = s_context.getString( R.string.key_hide_values );
        hideTileValues = sp.getBoolean( str, false );

        str = s_context.getString( R.string.key_skip_confirm );
        skipCommitConfirm = sp.getBoolean( str, false );

        str = s_context.getString( R.string.key_color_tiles );
        showColors = sp.getBoolean( str, true );
    }

    public CommonPrefs( Context context, CommonPrefs src ) {
        this( context );
        copyFrom( src );
    }

    public void copyFrom( CommonPrefs src )
    {
        showBoardArrow = src.showBoardArrow;
        showRobotScores = src.showRobotScores;
        hideTileValues = src.hideTileValues;
        skipCommitConfirm = src.skipCommitConfirm;
        showColors = src.showColors;
    }

    public static void setContext( Context context )
    {
        Assert.assertTrue( s_context == null );
        s_context = context;
    }
}
