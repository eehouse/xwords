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
import android.os.Bundle;
import android.content.SharedPreferences;
import android.preference.Preference;
import android.preference.PreferenceManager;

public class PrefsActivity extends PreferenceActivity 
    implements SharedPreferences.OnSharedPreferenceChangeListener {

    private String[] m_keys;

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

    private void setSummary( SharedPreferences sp, String key )
    {
        Preference pref = getPreferenceScreen().findPreference( key );
        String value = sp.getString( key, "" );
        if ( ! value.equals("") ) {
            pref.setSummary( value );
        }
    }

}
