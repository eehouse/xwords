/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2021 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.os.Bundle;
import android.view.MenuItem;
import android.view.View;
import android.widget.PopupMenu;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceManager;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.gen.PrefsWrappers;
import org.eehouse.android.xw4.jni.CommonPrefs;
import org.eehouse.android.xw4.loc.LocUtils;

import java.io.File;
import java.util.HashMap;
import java.util.Map;

public class PrefsDelegate extends DelegateBase
    implements SharedPreferences.OnSharedPreferenceChangeListener,
               View.OnClickListener, PopupMenu.OnMenuItemClickListener {
    private static final String TAG = PrefsDelegate.class.getSimpleName();

    private XWActivity mActivity;
    private PreferenceFragmentCompat mFragment;
    private static int[] s_keys = {
        R.string.key_logging_on,
        R.string.key_show_sms,
        R.string.key_enable_nbs,
        R.string.key_download_path,
        R.string.key_thumbsize,
        R.string.key_default_language,
        R.string.key_force_radio,
        R.string.key_disable_nag,
        R.string.key_disable_nag_solo,
        R.string.key_disable_relay,
        R.string.key_disable_bt,
        R.string.key_force_tablet,
        R.string.key_mqtt_host,
        R.string.key_mqtt_port,
        R.string.key_mqtt_qos,
    };
    private static Map<String, Integer> s_keysHash = null;

    public PrefsDelegate( XWActivity activity, Delegator delegator,
                          Bundle savedInstanceState )
    {
        super( delegator, savedInstanceState, R.layout.prefs );
        mActivity = activity;
    }

    @Override
    protected Dialog makeDialog( final DBAlert alert, Object[] params )
    {
        final DlgID dlgID = alert.getDlgID();
        DialogInterface.OnClickListener lstnr = null;
        int confirmID = 0;

        switch( dlgID ) {
        case REVERT_COLORS:
            confirmID = R.string.confirm_revert_colors;
            lstnr = new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick( DialogInterface dlg, int item ) {
                        PrefsDelegate self = (PrefsDelegate)curThis();
                        Resources res = alert.getContext().getResources();
                        SharedPreferences.Editor editor =
                            self.getSharedPreferences().edit();
                        int[] themeKeys = { R.array.color_ids_light,
                                            R.array.color_ids_dark,
                        };
                        for ( int themeKey : themeKeys ) {
                            String[] colorKeys = res.getStringArray( themeKey );
                            for ( String colorKey : colorKeys ) {
                                editor.remove( colorKey );
                            }
                        }
                        editor.commit();
                        self.relaunch();
                    }
                };
            break;
        case REVERT_ALL:
            confirmID = R.string.confirm_revert_all;
            lstnr = new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick( DialogInterface dlg, int item ) {
                        PrefsDelegate self = (PrefsDelegate)curThis();
                        SharedPreferences sp = self.getSharedPreferences();
                        SharedPreferences.Editor editor = sp.edit();
                        editor.clear();
                        editor.commit();
                        self.relaunch();
                    }
                };
            break;
        }

        Dialog dialog = null;
        if (  null != lstnr ) {
            dialog = makeAlertBuilder()
                .setTitle( R.string.query_title )
                .setMessage( confirmID )
                .setPositiveButton( android.R.string.ok, lstnr )
                .setNegativeButton( android.R.string.cancel, null )
                .create();
        }
        return dialog;
    }

    @Override
    protected void init( Bundle savedInstanceState )
    {
        // Assert.assertNotNull( m_fragment );
        if ( null == s_keysHash ) {
            s_keysHash = new HashMap<>();
            for ( int key : s_keys ) {
                String str = getString( key );
                s_keysHash.put( str, key );
            }
        }
    }

    void setRootFragment( PreferenceFragmentCompat fragment )
    {
        Assert.assertNotNull( fragment );
        mFragment = fragment;
    }

    @Override
    protected void onResume()
    {
        super.onResume();
        getSharedPreferences().registerOnSharedPreferenceChangeListener( this );

        // It's too early somehow to do this in init() above
        findViewById( R.id.prefs_menu ).setOnClickListener(this);
    }

    @Override
    protected void onPause()
    {
        getSharedPreferences().unregisterOnSharedPreferenceChangeListener(this);
        super.onPause();
    }

    // interface View.OnClickListener
    @Override
    public void onClick( View view )
    {
        DlgID dlgID = null;
        int id = view.getId();
        switch ( id ) {
        case R.id.prefs_menu:
            PopupMenu popup = new PopupMenu( mActivity, view );
            popup.inflate( R.menu.prefs_popup );
            popup.setOnMenuItemClickListener( this );
            popup.show();
            break;
        default:
            Assert.failDbg();
        }

        if ( null != dlgID ) {
            showDialogFragment( dlgID );
        }
    }

    @Override
    public boolean onMenuItemClick( MenuItem item )
    {
        boolean handled = true;
        DlgID dlgID = null;
        CommonPrefs.ColorTheme theme = null;
        switch ( item.getItemId() ) {
        case R.id.prefs_revert_colors:
            dlgID = DlgID.REVERT_COLORS;
            break;
        case R.id.prefs_revert_all:
            dlgID = DlgID.REVERT_ALL;
            break;
        case R.id.prefs_copy_light:
            theme = CommonPrefs.ColorTheme.LIGHT;
            break;
        case R.id.prefs_copy_dark:
            theme = CommonPrefs.ColorTheme.DARK;
            break;
        default:
            Assert.failDbg();
            handled = false;
        }

        if ( null != dlgID ) {
            showDialogFragment( dlgID );
        } else if ( null != theme ) {
            makeNotAgainBuilder( R.string.not_again_copytheme,
                                 R.string.key_na_copytheme,
                                 Action.EXPORT_THEME )
                .setParams( theme )
                .show();
        }

        return handled;
    }

    // interface SharedPreferences.OnSharedPreferenceChangeListener
    @Override
    public void onSharedPreferenceChanged( SharedPreferences sp, String key )
    {
        if ( s_keysHash.containsKey( key ) ) {
            switch( s_keysHash.get( key ) ) {
            case R.string.key_logging_on:
                Log.enable( sp.getBoolean( key, false ) );
                break;
            case R.string.key_show_sms:
                NBSProto.smsToastEnable( sp.getBoolean( key, false ) );
                break;
            case R.string.key_enable_nbs:
                if ( ! sp.getBoolean( key, true ) ) {
                    NBSProto.stopThreads();
                }
                break;
            case R.string.key_download_path:
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
                break;
            case R.string.key_thumbsize:
                DBUtils.clearThumbnails( mActivity );
                break;
            case R.string.key_default_language:
                // forceDictsMatch( sp.getString( key, null ) );
                break;
            case R.string.key_force_radio:
                SMSPhoneInfo.reset();
                break;
            case R.string.key_disable_nag:
            case R.string.key_disable_nag_solo:
                NagTurnReceiver.resetNagsDisabled( mActivity );
                break;
            case R.string.key_disable_relay:
                RelayService.enabledChanged( mActivity );
                break;
            case R.string.key_disable_bt:
                BTUtils.disabledChanged( mActivity );
                break;
            case R.string.key_force_tablet:
                makeOkOnlyBuilder( R.string.after_restart ).show();
                break;
            case R.string.key_mqtt_host:
            case R.string.key_mqtt_port:
            case R.string.key_mqtt_qos:
                MQTTUtils.onConfigChanged( mActivity );
                break;
            default:
                Assert.failDbg();
                break;
            }
        }
    }

    @Override
    public boolean onPosButton( Action action, Object[] params )
    {
        boolean handled = true;
        switch ( action ) {
        case ENABLE_NBS_DO:
            XWPrefs.setNBSEnabled( mActivity, true );
            SMSCheckBoxPreference.setChecked();
            break;
        case DISABLE_RELAY_DO:
            RelayService.setEnabled( mActivity, false );
            RelayCheckBoxPreference.setChecked();
            break;
        case DISABLE_BT_DO:
            BTUtils.setEnabled( mActivity, false );
            BTCheckBoxPreference.setChecked();
            break;
        case EXPORT_THEME:
            CommonPrefs.ColorTheme theme = (CommonPrefs.ColorTheme)params[0];
            CommonPrefs.colorPrefsToClip( mActivity, theme );
            DbgUtils.showf( mActivity, R.string.theme_data_success );
            break;
        default:
            handled = super.onPosButton( action, params );
        }
        return handled;
    }

    private void relaunch()
    {
        resetPrefs( mActivity, true );

        // Now replace this activity with a new copy
        // so the new values get loaded.
        launch( mActivity );
        finish();
    }

    private SharedPreferences getSharedPreferences()
    {
        return mFragment.getPreferenceScreen().getSharedPreferences();
    }

    public static void launch( Context context )
    {
        launch( context, PrefsWrappers.prefs.class );
    }

    public static void launch( Context context, Class root )
    {
        Bundle bundle = null;
        Intent intent = new Intent( context, PrefsActivity.class );
        if ( null != root ) {
            PrefsActivity.bundleRoot( root, intent );
        }
        context.startActivity( intent );
    }

    public static void resetPrefs( Context context, boolean mustCheck )
    {
        int[] prefIDs = PrefsWrappers.getPrefsResIDs();
        for ( int id : prefIDs ) {
            PreferenceManager.setDefaultValues( context, id, mustCheck );
        }
    }
}
