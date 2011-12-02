/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
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

package org.eehouse.android.xw4;
import android.app.AlertDialog;
import android.app.Dialog;
import android.preference.PreferenceActivity;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.view.View;
import android.widget.Button;

public class PrefsActivity extends PreferenceActivity 
    implements SharedPreferences.OnSharedPreferenceChangeListener {

    private static final int REVERT_COLORS = 1;
    private static final int REVERT_ALL = 2;

    private String m_keyLogging;

    @Override
    protected Dialog onCreateDialog( int id )
    {
        DialogInterface.OnClickListener lstnr = null;
        int confirmID = 0;

        switch( id ) {
        case REVERT_COLORS:
            confirmID = R.string.confirm_revert_colors;
            lstnr = new DialogInterface.OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        SharedPreferences sp =
                            getPreferenceScreen().getSharedPreferences();
                        SharedPreferences.Editor editor = sp.edit();
                        int[] colorKeys = {
                            R.string.key_player0,
                            R.string.key_player1,
                            R.string.key_player2,
                            R.string.key_player3,
                            R.string.key_bonus_l2x,
                            R.string.key_bonus_l3x,
                            R.string.key_bonus_w2x,
                            R.string.key_bonus_w3x,
                            R.string.key_tile_back,
                            R.string.key_clr_crosshairs,
                            R.string.key_empty,
                            R.string.key_background,
                            R.string.key_clr_bonushint,
                        };
                        for ( int colorKey : colorKeys ) {
                            editor.remove( getString(colorKey) );
                        }
                        editor.commit();
                        relaunch();
                    }
                };
            break;
        case REVERT_ALL:
            confirmID = R.string.confirm_revert_all;
            lstnr = new DialogInterface.OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        SharedPreferences sp =
                            getPreferenceScreen().getSharedPreferences();
                        SharedPreferences.Editor editor = sp.edit();
                        editor.clear();
                        editor.commit();
                        relaunch();
                    }
                };
            break;
        }

        Dialog dialog = null;
        if ( null != lstnr ) {
            dialog = new AlertDialog.Builder( this )
                .setTitle( R.string.query_title )
                .setMessage( confirmID )
                .setPositiveButton( R.string.button_ok, lstnr )
                .setNegativeButton( R.string.button_cancel, null )
                .create();
        }
        return dialog;
    }

    @Override
    protected void onCreate( Bundle savedInstanceState )
    {
        super.onCreate(savedInstanceState);

        // Load the preferences from an XML resource
        addPreferencesFromResource( R.xml.xwprefs );
        setContentView( R.layout.prefs_w_buttons );

        m_keyLogging = getString( R.string.key_logging_on );

        Button button = (Button)findViewById( R.id.revert_colors );
        button.setOnClickListener( new View.OnClickListener() {
                public void onClick( View v ) {
                    showDialog( REVERT_COLORS );
                }
            } );
        button = (Button)findViewById( R.id.revert_all );
        button.setOnClickListener(new View.OnClickListener() {
                public void onClick( View v ) {
                    showDialog( REVERT_ALL );
                }
            } );
    }
    
    @Override
    protected void onResume() 
    {
        super.onResume();
        getPreferenceScreen().getSharedPreferences().
            registerOnSharedPreferenceChangeListener(this);   
   }

    @Override
    protected void onPause() 
    {
        super.onPause();
        getPreferenceScreen().getSharedPreferences().
            unregisterOnSharedPreferenceChangeListener(this);
    }

    public void onSharedPreferenceChanged( SharedPreferences sp, String key ) 
    {
        if ( key.equals( m_keyLogging ) ) {
            DbgUtils.logEnable( sp.getBoolean( key, false ) );
        }
    }

    private void relaunch()
    {
        PreferenceManager.setDefaultValues( this, R.xml.xwprefs,
                                            false );

        // Now replace this activity with a new copy
        // so the new values get loaded.
        startActivity( new Intent( this, PrefsActivity.class ) );
        finish();
    }

}
