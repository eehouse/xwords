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
import org.eehouse.android.xw4.Utils.ISOCode;
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
    private static ISOCode s_curLang;
    private static Map<String, String> s_langMap = null;

    public static View inflate( Context context, int resID )
    {
        LayoutInflater factory = LayoutInflater.from( context );
        return factory.inflate( resID, null );
    }

    public static void xlateTitle( Activity activity )
    {
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
        Assert.assertVarargsNotNullNR(params);
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
        Assert.assertVarargsNotNullNR(params);
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

    public static ISOCode getCurLangCode( Context context )
    {
        if ( null == s_curLang ) {
            String lang = Locale.getDefault().getLanguage();

            // sometimes I get "en-us", i.e. the locale's there too. Strip it.
            if ( lang.contains( "-" ) ) {
                lang = TextUtils.split(lang, "-")[0];
            }
            // Sometimes getLanguage() returns "". Let's just fall back to
            // English for now.
            if ( TextUtils.isEmpty( lang ) ) {
                lang = "en";
            }
            s_curLang = new ISOCode( lang );
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
