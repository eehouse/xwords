/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009-2014 by Eric House (xwords@eehouse.org).  All
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
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.CheckBoxPreference;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceManager;
import android.preference.PreferenceScreen;
import android.view.View;
import android.widget.Button;
import java.io.File;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.loc.LocUtils;

public class PrefsDelegate extends DelegateBase
    implements SharedPreferences.OnSharedPreferenceChangeListener {

    private PreferenceActivity m_activity;

    private String m_keyLogging;
    private String m_smsToasting;
    private String m_smsEnable;
    private String m_downloadPath;
    private String m_thumbSize;
    private String m_keyLocale;
    private String m_keyLangs;
    private String m_keyFakeRadio;
    private String m_keyNagsDisabled;

    public PrefsDelegate( PreferenceActivity activity, Delegator delegator,
                          Bundle savedInstanceState )
    {
        super( delegator, savedInstanceState, R.layout.prefs_w_buttons );
        m_activity = activity;
    }

    protected Dialog onCreateDialog( int id )
    {
        Dialog dialog = super.onCreateDialog( id );
        if ( null == dialog ) {
            DialogInterface.OnClickListener lstnr = null;
            int confirmID = 0;

            switch( DlgID.values()[id] ) {
            case REVERT_COLORS:
                confirmID = R.string.confirm_revert_colors;
                lstnr = new DialogInterface.OnClickListener() {
                        public void onClick( DialogInterface dlg, int item ) {
                            SharedPreferences sp = getSharedPreferences();
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
                            SharedPreferences sp = getSharedPreferences();
                            SharedPreferences.Editor editor = sp.edit();
                            editor.clear();
                            editor.commit();
                            relaunch();
                        }
                    };
                break;
            }

            if ( null == dialog && null != lstnr ) {
                dialog = makeAlertBuilder()
                    .setTitle( R.string.query_title )
                    .setMessage( confirmID )
                    .setPositiveButton( R.string.button_ok, lstnr )
                    .setNegativeButton( R.string.button_cancel, null )
                    .create();
            }
        }
        return dialog;
    }

    protected void init( Bundle savedInstanceState )
    {
        // Load the preferences from an XML resource
        m_activity.addPreferencesFromResource( R.xml.xwprefs );

        m_keyLogging = getString( R.string.key_logging_on );
        m_smsToasting = getString( R.string.key_show_sms );
        m_smsEnable = getString( R.string.key_enable_sms );
        m_downloadPath = getString( R.string.key_download_path );
        m_thumbSize = getString( R.string.key_thumbsize );
        m_keyLocale = getString( R.string.key_xlations_locale );
        m_keyLangs = getString( R.string.key_default_language );
        m_keyFakeRadio = getString( R.string.key_force_radio );
        m_keyNagsDisabled = getString( R.string.key_disable_nag );

        Button button = (Button)findViewById( R.id.revert_colors );
        button.setOnClickListener( new View.OnClickListener() {
                public void onClick( View v ) {
                    showDialog( DlgID.REVERT_COLORS );
                }
            } );
        button = (Button)findViewById( R.id.revert_all );
        button.setOnClickListener(new View.OnClickListener() {
                public void onClick( View v ) {
                    showDialog( DlgID.REVERT_ALL );
                }
            } );

        setupLangPref();

        hideStuff();
    }
    
    @Override
    protected void onResume() 
    {
        super.onResume();
        getSharedPreferences().registerOnSharedPreferenceChangeListener(this);   
    }

    protected void onPause() 
    {
        getSharedPreferences().unregisterOnSharedPreferenceChangeListener(this);
        super.onPause();
    }

    @Override
    public void onSharedPreferenceChanged( SharedPreferences sp, String key ) 
    {
        if ( key.equals( m_keyLogging ) ) {
            DbgUtils.logEnable( sp.getBoolean( key, false ) );
        } else if ( key.equals( m_smsToasting ) ) {
            SMSService.smsToastEnable( sp.getBoolean( key, false ) );
        } else if ( key.equals( m_smsEnable ) ) {
            if ( ! sp.getBoolean( key, true ) ) {
                SMSService.stopService( m_activity );
            }
        } else if ( key.equals( m_downloadPath ) ) {
            String value = sp.getString( key, null );
            if ( null != value ) {
                File dir = new File( value );
                String msg = null;
                if ( !dir.exists() ) {
                    msg = String.format( "%s does not exist", value );
                } else if ( !dir.isDirectory() ) {
                    msg = String.format( "%s is not a directory", value );
                } else if ( !dir.canWrite() ) {
                    msg = String.format( "Cannot write to %s", value );
                }
                if ( null != msg ) {
                    showToast( msg );
                }
            }
            DictUtils.invalDictList();
        } else if ( key.equals( m_thumbSize ) ) {
            DBUtils.clearThumbnails( m_activity );
        } else if ( key.equals( m_keyLocale ) ) {
            LocUtils.localeChanged( m_activity, sp.getString( key, null ) );
        } else if ( key.equals( m_keyLangs ) ) {
            forceDictsMatch( sp.getString( key, null ) );
        } else if ( key.equals( m_keyFakeRadio ) ) {
            SMSService.resetPhoneInfo();
        } else if ( key.equals( m_keyNagsDisabled ) ) {
            NagTurnReceiver.resetNagsDisabled( m_activity );
        }
    }

    @Override
    public void dlgButtonClicked( Action action, int button, Object[] params )
    {
        if ( AlertDialog.BUTTON_POSITIVE == button
             && action == Action.ENABLE_SMS_DO ) {
            boolean enabled = (Boolean)params[0];
            if ( enabled ) {
                XWPrefs.setSMSEnabled( m_activity, true );
                SMSCheckBoxPreference.setChecked();
            }
        } else { 
            super.dlgButtonClicked( action, button, params );
        }
    }

    private void relaunch()
    {
        PreferenceManager.setDefaultValues( m_activity, R.xml.xwprefs,
                                            false );

        // Now replace this activity with a new copy
        // so the new values get loaded.
        Utils.launchSettings( m_activity );
        finish();
    }

    private SharedPreferences getSharedPreferences()
    {
        return m_activity.getPreferenceScreen().getSharedPreferences();
    }

    private void setupLangPref()
    {
        ListPreference lp = (ListPreference)
            m_activity.findPreference( m_keyLangs );
        String curLang = lp.getValue().toString();
        boolean haveDictForLang = false;

        String[] langs = DictLangCache.listLangs( m_activity );
        String[] langsLoc = new String[langs.length];
        for ( int ii = 0; ii < langs.length; ++ii ) {
            String lang = langs[ii];
            haveDictForLang = haveDictForLang
                || lang.equals( curLang );
            langsLoc[ii] = xlateLang( lang, true );
        }

        if ( !haveDictForLang ) {
            curLang = DictLangCache.getLangName( m_activity, 1 ); // English, unlocalized
            lp.setValue( curLang );
        }
        forceDictsMatch( curLang );

        lp.setEntries( langsLoc );
        lp.setDefaultValue( xlateLang( curLang, true ) );
        lp.setEntryValues( langs );
    }

    private void forceDictsMatch( String newLang ) 
    {
        int code = DictLangCache.getLangLangCode( m_activity, newLang );
        int[] keyIds = { R.string.key_default_dict, 
                         R.string.key_default_robodict };
        for ( int id : keyIds ) {
            String key = getString( id );
            DictListPreference pref = (DictListPreference)m_activity.findPreference( key );
            String curDict = pref.getValue().toString();
            if ( ! DictUtils.dictExists( m_activity, curDict )
                 || code != DictLangCache.getDictLangCode( m_activity, 
                                                           curDict ) ) {
                pref.invalidate();
            }
        }
    }

    private void hideOne( int prefID, int screenID )
    {
        Preference pref = m_activity.findPreference( getString( prefID ) );
        String key = getString( screenID );
        ((PreferenceScreen)m_activity.findPreference( key ))
            .removePreference( pref );
    }

    private void hideStuff()
    {
        if ( !XWApp.SMSSUPPORTED || !Utils.isGSMPhone( m_activity ) ) {
            hideOne( R.string.key_enable_sms, R.string.key_network_behavior );
        }

        if ( ABUtils.haveActionBar() ) {
            hideOne( R.string.key_hide_title, R.string.prefs_appearance );
        }
    }

}
