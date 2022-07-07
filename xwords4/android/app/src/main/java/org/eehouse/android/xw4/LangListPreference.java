/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 - 2015 by Eric House (xwords@eehouse.org).  All rights
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

import android.content.Context;
import android.os.Handler;
import android.util.AttributeSet;
import androidx.preference.Preference;
import androidx.preference.PreferenceManager;

import org.eehouse.android.xw4.loc.LocUtils;
import org.eehouse.android.xw4.Utils.ISOCode;

public class LangListPreference extends XWListPreference
    implements Preference.OnPreferenceChangeListener {
    private static final String TAG = LangListPreference.class.getSimpleName();
    private Context mContext;
    private String mKey;

    public LangListPreference( Context context, AttributeSet attrs )
    {
        super( context, attrs );
        mContext = context;
        mKey = context.getString( R.string.key_default_language );
    }

    @Override
    public void onAttached()
    {
        super.onAttached();
        setOnPreferenceChangeListener( this );
        setupLangPref();
    }

    @Override
    public boolean onPreferenceChange( Preference preference, Object newValue )
    {
        final String newLang = (String)newValue;
        new Handler().post( new Runnable() {
                @Override
                public void run() {
                    forceDictsMatch( newLang );
                }
            } );
        return true;
    }

    private void setupLangPref()
    {
        String keyLangs = mContext.getString( R.string.key_default_language );
        String value = getValue();
        String curLang = null == value ? null : value.toString();
        boolean haveDictForLang = false;

        String[] langs = DictLangCache.listLangs( m_context );
        String[] langsLoc = new String[langs.length];
        for ( int ii = 0; ii < langs.length; ++ii ) {
            String lang = langs[ii];
            haveDictForLang = haveDictForLang || lang.equals( curLang );
            langsLoc[ii] = lang;
        }

        if ( !haveDictForLang ) {
            curLang = DictLangCache.getLangNameForISOCode( mContext, Utils.ISO_EN );
            setValue( curLang );
        }
        forceDictsMatch( curLang );

        setEntries( langsLoc );
        setDefaultValue( curLang );
        setEntryValues( langs );
    }

    private void forceDictsMatch( String newLang )
    {
        if ( null != newLang ) {
            ISOCode isoCode = DictLangCache.getLangIsoCode( mContext, newLang );
            int[] keyIds = { R.string.key_default_dict,
                             R.string.key_default_robodict };
            for ( int id : keyIds ) {
                String key = mContext.getString( id );

                PreferenceManager mgr = getPreferenceManager();
                Assert.assertNotNull( mgr );

                DictListPreference pref = (DictListPreference)mgr.findPreference( key );
                Assert.assertNotNull( pref );

                String curDict = pref.getValue().toString();
                if ( ! DictUtils.dictExists( mContext, curDict )
                     || ! isoCode.equals( DictLangCache.getDictISOCode( mContext,
                                                                        curDict ) ) ) {
                         pref.invalidate();
                }
            }
        }
    }
}
