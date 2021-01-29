/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2021 by Eric House (xwords@eehouse.org).  All
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

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import androidx.fragment.app.Fragment;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat.OnPreferenceDisplayDialogCallback;
import androidx.preference.PreferenceFragmentCompat.OnPreferenceStartFragmentCallback;
import androidx.preference.PreferenceFragmentCompat.OnPreferenceStartScreenCallback;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;
import androidx.preference.PreferenceViewHolder;

import java.util.HashSet;
import java.util.Set;
    
import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.DlgDelegate.Builder;
import org.eehouse.android.xw4.jni.CommonPrefs;

public class PrefsActivity extends XWActivity
    implements Delegator, DlgDelegate.HasDlgDelegate,
               // OnPreferenceStartScreenCallback,
               OnPreferenceStartFragmentCallback,
               OnPreferenceDisplayDialogCallback {
    private final static String TAG = PrefsActivity.class.getSimpleName();

    private PrefsDelegate m_dlgt;

    interface DialogProc {
        XWDialogFragment makeDialogFrag();
    }

    @Override
    protected void onCreate( Bundle savedInstanceState )
    {
        m_dlgt = new PrefsDelegate( this, this, savedInstanceState );
        super.onCreate( savedInstanceState, m_dlgt );

        int layoutID = m_dlgt.getLayoutID();
        Assert.assertTrue( 0 < layoutID );
        m_dlgt.setContentView( layoutID );

        PreferenceFragmentCompat rootFrag = new FragPrefs();
        m_dlgt.setRootFragment( rootFrag );

        getSupportFragmentManager()
            .beginTransaction()
            .replace( R.id.main_container, rootFrag )
            .commit();
    }

    @Override
    public Builder makeOkOnlyBuilder( String msg )
    {
        return m_dlgt.makeOkOnlyBuilder( msg );
    }

    @Override
    public Builder makeNotAgainBuilder(int msgID, int key, Action action)
    {
        return m_dlgt.makeNotAgainBuilder( msgID, key, action );
    }

    @Override
    public boolean onPreferenceDisplayDialog( PreferenceFragmentCompat caller,
                                              Preference pref )
    {
        boolean success = false;
        if ( pref instanceof DialogProc ) {
            show( ((DialogProc)pref).makeDialogFrag() );
            success = true;
        } else {
            Log.e( TAG, "unexpected class: %s", pref.getClass().getSimpleName() );
        }
        return success;
    }

    @Override
    public boolean onPreferenceStartFragment( PreferenceFragmentCompat caller,
                                              Preference pref )
    {
        final Bundle args = pref.getExtras();
        final Fragment fragment = getSupportFragmentManager().getFragmentFactory()
            .instantiate( getClassLoader(), pref.getFragment());
        fragment.setArguments(args);
        fragment.setTargetFragment( caller, 0 );

        getSupportFragmentManager().beginTransaction()
            .replace( R.id.main_container, fragment)
            .addToBackStack(null)
            .commit();
        setTitle( pref.getTitle() );
        
        return true;
    }

    Builder makeConfirmThenBuilder( String msg, Action action )
    {
        return m_dlgt.makeConfirmThenBuilder( msg, action );
    }

    protected void showSMSEnableDialog( Action action )
    {
        m_dlgt.showSMSEnableDialog( action );
    }

    private static Set<String> sHideSet = null;
    private synchronized static Set<String> getHideSet( Context context )
    {
        if ( null == sHideSet ) {
            Set<Integer> tmp = new HashSet<>();
            if ( !Utils.isGSMPhone( context ) || Perms23.haveNativePerms() ) {
                tmp.add( R.string.key_enable_nbs );
            }
            if ( ABUtils.haveActionBar() ) {
                tmp.add( R.string.key_hide_title );
            }

            if ( ! BuildConfig.WIDIR_ENABLED ) {
                tmp.add( R.string.key_enable_p2p );
            }

            if ( null == FBMService.getFCMDevID( context ) ) {
                tmp.add( R.string.key_show_fcm );
            }

            if ( BuildConfig.DEBUG ) {
                tmp.add( R.string.key_logging_on );
                tmp.add( R.string.key_enable_debug );
            } else {
                tmp.add( R.string.key_unhide_dupmode );
            }

            if ( CommonPrefs.getDupModeHidden( context ) ) {
                tmp.add( R.string.key_init_dupmodeon );
            }

            if ( null == BuildConfig.KEY_FCMID ) {
                tmp.add( R.string.key_relay_poll );
            }

            sHideSet = new HashSet<>();
            for ( int key : tmp ) {
                sHideSet.add( context.getString( key ) );
            }
        }
        return sHideSet;
    }

    // Every subscreen in the prefs.xml heierarchy has to have a class
    // associated with it just to provide its xml-file ID. Stupid design; not
    // mine!

    private abstract static class BasePrefsFrag extends PreferenceFragmentCompat {
        @Override
        public void onCreatePreferences( Bundle savedInstanceState, String rootKey )
        {
            setPreferencesFromResource( getResID(), rootKey );
        }

        @Override
        public void onViewCreated( View view, Bundle savedInstanceState )
        {
            Context context = view.getContext();
            Set<String> hideSet = getHideSet( context );

            for ( String key : hideSet ) {
                Preference pref = findPreference( key );
                if ( null != pref ) {
                    Log.d( TAG, "in %s, found pref %s", getClass().getSimpleName(),
                           pref.getTitle() );
                    pref.setVisible( false );
                }
            }

            super.onViewCreated( view, savedInstanceState );
        }

        abstract int getResID();
    }

    public static class FragPrefs extends BasePrefsFrag {
        @Override
        int getResID() { return R.xml.prefs; }
    }
    public static class FragPrefsDflts extends BasePrefsFrag {
        @Override
        int getResID() { return R.xml.prefs_dflts; }
    }
    public static class FragPrefsDfltsNames extends BasePrefsFrag {
        @Override
        int getResID() { return R.xml.prefs_dflts_names; }
    }
    public static class FragPrefsDfltsDicts extends BasePrefsFrag {
        @Override
        int getResID() { return R.xml.prefs_dflts_dicts; }
    }
    public static class FragPrefsAppear extends BasePrefsFrag {
        @Override
        int getResID() { return R.xml.prefs_appear; }
    }
    public static class FragPrefsAppearColors extends BasePrefsFrag {
        @Override
        int getResID() { return R.xml.prefs_appear_colors; }
    }
    public static class FragPrefsBehave extends BasePrefsFrag {
        @Override
        int getResID() { return R.xml.prefs_behave; }
    }
    public static class FragPrefsBehaveNag extends BasePrefsFrag {
        @Override
        int getResID() { return R.xml.prefs_behave_nag; }
    }
    public static class FragPrefsNet extends BasePrefsFrag {
        @Override
        int getResID() { return R.xml.prefs_net; }
    }
    public static class FragPrefsNetAdv extends BasePrefsFrag {
        @Override
        int getResID() { return R.xml.prefs_net_adv; }
    }
    public static class FragPrefsDbg extends BasePrefsFrag {
        @Override
        int getResID() { return R.xml.prefs_dbg; }
    }
    public static class FragPrefsDbgNet extends BasePrefsFrag {
        @Override
        int getResID() { return R.xml.prefs_dbg_net; }
    }
    public static class FragPrefsDbgSms extends BasePrefsFrag {
        @Override
        int getResID() { return R.xml.prefs_dbg_sms; }
    }
    public static class FragPrefsDbgL10n extends BasePrefsFrag {
        @Override
        int getResID() { return R.xml.prefs_dbg_l10n; }
    }
}
