/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
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
import android.preference.PreferenceActivity;
import android.app.Dialog;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.DialogInterface;
import android.os.Bundle;
import android.content.SharedPreferences;
import android.preference.Preference;
import android.preference.PreferenceManager;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MenuInflater;

public class PrefsActivity extends PreferenceActivity 
    implements SharedPreferences.OnSharedPreferenceChangeListener {

    private static final int REVERT_COLORS = 1;

    private String[] m_keys;

    @Override
    protected Dialog onCreateDialog( int id )
    {
        Dialog dialog = null;
        switch( id ) {
        case REVERT_COLORS:
            DialogInterface.OnClickListener lstnr = 
                new DialogInterface.OnClickListener() {
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
                            R.string.key_focus,
                            R.string.key_empty,
                        };
                        for ( int colorKey : colorKeys ) {
                            editor.remove( getString(colorKey) );
                        }
                        editor.commit();

                        PreferenceManager.setDefaultValues( PrefsActivity.this, 
                                                            R.xml.xwprefs,
                                                            false );

                        // Now replace this activity with a new copy
                        // so the new values get loaded.
                        startActivity( new Intent( PrefsActivity.this,
                                                   PrefsActivity.class ) );
                        finish();
                    }
                };

            dialog = new AlertDialog.Builder( this )
                .setTitle( R.string.query_title )
                .setMessage( R.string.confirm_revert_colors )
                .setPositiveButton( R.string.button_ok, lstnr )
                .setNegativeButton( R.string.button_cancel, null )
                .create();
            break;
        }
        return dialog;
    }

    @Override
    protected void onCreate( Bundle savedInstanceState )
    {
        super.onCreate(savedInstanceState);

        // Load the preferences from an XML resource
        addPreferencesFromResource( R.xml.xwprefs );

        int[] textKeyIds = { R.string.key_relay_host,
                             R.string.key_relay_port,
                             // R.string.key_sms_port,
                             R.string.key_dict_host,
                             R.string.key_board_size,
                             R.string.key_initial_game_minutes,
                             R.string.key_default_dict,
                             R.string.key_default_phonies,
        };

        SharedPreferences sp
            = PreferenceManager.getDefaultSharedPreferences( this );
        m_keys = new String[ textKeyIds.length ];
        for ( int ii = 0; ii < textKeyIds.length; ++ii ) {
            int id  = textKeyIds[ii];
            String key = getString( id );
            setSummary( sp, key );
            m_keys[ii] = key;
        }
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
        for ( String akey : m_keys ) {
            if ( akey.equals( key ) ) {
                setSummary( sp, key );
                break;
            }
        }
    }

    @Override
    public boolean onCreateOptionsMenu( Menu menu )
    {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate( R.menu.prefs_menu, menu );
        return true;
    }

    @Override
    public boolean onOptionsItemSelected( MenuItem item )
    {
        boolean handled = false;
        if ( R.id.menu_revert_colors == item.getItemId()) {
            showDialog( REVERT_COLORS );
            handled = true;
        }
        return handled;
    }

    private void setSummary( SharedPreferences sp, String key )
    {
        Preference pref = getPreferenceScreen().findPreference( key );
        String value = sp.getString( key, "" );
        if ( ! value.equals("") ) {
            pref.setSummary( value );
        }
    }

}
