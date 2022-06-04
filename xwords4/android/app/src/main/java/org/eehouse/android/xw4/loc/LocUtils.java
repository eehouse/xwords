/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2014 by Eric House (xwords@eehouse.org).  All rights reserved.
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

package org.eehouse.android.xw4.loc;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.View;

import org.eehouse.android.xw4.Assert;
import org.eehouse.android.xw4.BuildConfig;
import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.Utils;
import org.eehouse.android.xw4.XWApp;
import org.eehouse.android.xw4.XWPrefs;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

public class LocUtils {
    protected static final String RES_FORMAT = "%[\\d]\\$[ds]";
    private static String s_curLocale;
    private static String s_curLang;
    private static Map<String, String> s_langMap = null;

    public static View inflate( Context context, int resID )
    {
        LayoutInflater factory = LayoutInflater.from( context );
        return factory.inflate( resID, null );
    }

    public static void xlateTitle( Activity activity )
    {
    }

    public static String xlateLang( Context context, String lang )
    {
        return xlateLang( context, lang, false );
    }

    public static String xlateLang( Context context, String lang, boolean caps )
    {
        if ( null == s_langMap ) {
            s_langMap = new HashMap<>();
            s_langMap.put( "English", context.getString( R.string.lang_name_english ) );
            s_langMap.put( "French", context.getString( R.string.lang_name_french ) );
            s_langMap.put( "German", context.getString( R.string.lang_name_german ) );
            s_langMap.put( "Turkish", context.getString( R.string.lang_name_turkish ) );
            s_langMap.put( "Arabic", context.getString( R.string.lang_name_arabic ) );
            s_langMap.put( "Spanish", context.getString( R.string.lang_name_spanish ) );
            s_langMap.put( "Swedish", context.getString( R.string.lang_name_swedish ) );
            s_langMap.put( "Polish", context.getString( R.string.lang_name_polish ) );
            s_langMap.put( "Danish", context.getString( R.string.lang_name_danish ) );
            s_langMap.put( "Italian", context.getString( R.string.lang_name_italian ) );
            s_langMap.put( "Dutch", context.getString( R.string.lang_name_dutch ) );
            s_langMap.put( "Catalan", context.getString( R.string.lang_name_catalan ) );
            s_langMap.put( "Portuguese", context.getString( R.string.lang_name_portuguese ) );
            s_langMap.put( "Russian", context.getString( R.string.lang_name_russian ) );
            s_langMap.put( "Czech", context.getString( R.string.lang_name_czech ) );
            s_langMap.put( "Greek", context.getString( R.string.lang_name_greek ) );
            s_langMap.put( "Slovak", context.getString( R.string.lang_name_slovak ) );
        }

        String xlated = s_langMap.get( lang );
        if ( null == xlated ) {
            xlated = lang;
        }
        if ( caps ) {
            xlated = Utils.capitalize( xlated );
        }
        return xlated;
    }

    public static void xlateView( Activity activity )
    {
    }

    public static void xlateView( Context context, View view )
    {
    }

    public static void xlateMenu( Activity activity, Menu menu )
    {
    }

    private static String xlateString( Context context, CharSequence str )
    {
        String result = null;
        if ( null != str ) {
            result = xlateString( context, str.toString() );
        }
        return result;
    }

    private static String xlateString( Context context, String str,
                                       boolean associate )
    {
        return str;
    }

    public static String xlateString( Context context, String str )
    {
        if ( BuildConfig.LOCUTILS_ENABLED ) {
            str = xlateString( context, str, true );
        }
        return str;
    }

    public static String[] getStringArray( Context context, int resID )
    {
        Resources res = context.getResources();
        String[] arr = res.getStringArray( resID );
        return xlateStrings( context, arr );
    }

    public static String[] xlateStrings( Context context, String[] strs )
    {
        String[] result = new String[strs.length];
        for ( int ii = 0; ii < strs.length; ++ii ) {
            result[ii] = xlateString( context, strs[ii].toString() );
        }
        return result;
    }

    public static String getString( Context context, int id )
    {
        return context.getString( id );
    }

    public static String getStringOrNull( int id )
    {
        String result = null;
        if ( 0 != id ) {
            result = getString( XWApp.getContext(), true, id );
        }
        return result;
    }

    public static String getString( Context context, boolean canUseDB, int id )
    {
        return getString( context, id );
    }

    public static String getString( Context context, int id, Object... params )
    {

        return context.getString( id, params );
    }

    public static String getQuantityString( Context context, int id,
                                            int quantity )
    {
        String result = context.getResources().getQuantityString( id, quantity );
        return result;
    }

    public static String getQuantityString( Context context, int id,
                                            int quantity, Object... params )
    {
        String result = context.getResources()
            .getQuantityString( id, quantity, params );
        return result;
    }

    protected static String getCurLocaleName( Context context )
    {
        String locale_code = getCurLocale( context );
        Locale locale = new Locale( locale_code );
        String name = locale.getDisplayLanguage( locale );
        return name;
    }

    public static String getCurLangCode( Context context )
    {
        if ( null == s_curLang ) {
            String lang = Locale.getDefault().getLanguage();
            // sometimes I get "en-us" in this case, i.e. the locale's there
            // too. Strip it.
            if ( lang.contains( "-" ) ) {
                lang = TextUtils.split(lang, "-")[0];
            }
            Assert.assertTrueNR( 2 == lang.length() );
            s_curLang = lang;
        }
        return s_curLang;
    }

    public static String getCurLocale( Context context )
    {
        if ( null == s_curLocale ) {
            s_curLocale = Locale.getDefault().toString();
        }
        return s_curLocale;
    }

    private static void xlateView( Context context, String contextName,
                                   View view, int depth )
    {
    }

    public static AlertDialog.Builder makeAlertBuilder( Context context )
    {
        return new AlertDialog.Builder( context );
    }
}
