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
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.content.res.Resources;
import android.preference.DialogPreference;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceGroup;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MenuItem.OnMenuItemClickListener;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.Spinner;
import android.widget.SpinnerAdapter;
import android.widget.TextView;


import org.eehouse.android.xw4.Assert;
import org.eehouse.android.xw4.DBUtils;
import org.eehouse.android.xw4.DbgUtils;
import org.eehouse.android.xw4.Log;
import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.Utils;
import org.eehouse.android.xw4.XWApp;
import org.eehouse.android.xw4.XWPrefs;
import org.json.JSONArray;
import org.json.JSONObject;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.HashSet;
import java.util.IllegalFormatConversionException;
import java.util.Iterator;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class LocUtils {
    private static final String TAG = LocUtils.class.getSimpleName();
    public static final String CONTEXT_NAME = "CONTEXT_NAME";
    protected static final String RES_FORMAT = "%[\\d]\\$[ds]";
    private static final int FMT_LEN = 4;
    private static final String k_LOCALE = "locale";
    private static final String k_XLATEVERS = "xlatevers";

    private static Map<String, String> s_xlationsLocal = null;
    private static Map<String, String> s_xlationsBlessed = null;
    private static HashMap<Integer, String> s_idsToKeys = null;
    private static Boolean s_enabled = null;
    private static String s_curLocale;
    private static String s_curLang;
    private static WeakReference<Menu> s_latestMenuRef;
    private static Map<WeakReference<Menu>, HashSet<String> > s_menuSets
        = new HashMap<WeakReference<Menu>, HashSet<String> >();
    private static Map<String, HashSet<String> > s_contextSets
        = new HashMap<String, HashSet<String> >();
    private static Map<String, String> s_langMap = null;

    public static void localeChanged( Context context, String newLocale )
    {
        saveLocalData( context );
        s_curLocale = newLocale;
        s_curLang = splitLocale( newLocale );
        s_xlationsLocal = null;
        s_xlationsBlessed = null;
        s_enabled = null;
    }

    public static View inflate( Context context, int resID )
    {
        LayoutInflater factory = LayoutInflater.from( context );
        View view = factory.inflate( resID, null );
        xlateView( context, view );
        return view;
    }

    public static void xlateTitle( Activity activity )
    {
        String title = activity.getTitle().toString();
        String xlated = xlateString( activity, title );
        if ( ! title.equals(xlated) ) {
            activity.setTitle( xlated );
        }
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
        xlateView( activity, Utils.getContentView( activity ) );
    }

    public static void xlatePreferences( PreferenceActivity activity )
    {
        if ( XWApp.LOCUTILS_ENABLED ) {
            xlatePreferences( activity, activity.getPreferenceScreen(), 0 );
        }
    }

    public static void xlateView( Context context, View view )
    {
        // DbgUtils.logf( "xlateView(%s, %s)", context.getClass().getName(),
        //                view.getClass().getName() );
        if ( XWApp.LOCUTILS_ENABLED ) {
            xlateView( context, context.getClass().getSimpleName(), view, 0 );
        }
    }

    public static void xlateMenu( Activity activity, Menu menu )
    {
        if ( XWApp.LOCUTILS_ENABLED ) {
            pareMenus();

            xlateMenu( activity, new WeakReference<>( menu ), menu, 0 );
        }
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
        if ( LocIDs.getS_MAP( context ).containsKey( str ) ) {
            if ( associate ) {
                associateContextString( context, str );
            }
            String xlation = getXlation( context, str, true );
            if ( null != xlation ) {
                str = xlation;
            }
        }
        return str;
    }

    public static String xlateString( Context context, String str )
    {
        if ( XWApp.LOCUTILS_ENABLED ) {
            str = xlateString( context, str, true );
        }
        return str;
    }

    public static CharSequence[] xlateStrings( Context context, CharSequence[] strs )
    {
        CharSequence[] result = new CharSequence[strs.length];
        for ( int ii = 0; ii < strs.length; ++ii ) {
            result[ii] = xlateString( context, strs[ii].toString() );
        }
        return result;
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
        return getString( context, true, id );
    }

    public static String getStringOrNull( int id )
    {
        String result = null;
        if ( 0 != id ) {
            Context context = XWApp.getContext();
            result = getString( context, true, id );
        }
        return result;
    }

    public static String getString( Context context, boolean canUseDB, int id )
    {
        String result = null;

        if ( XWApp.LOCUTILS_ENABLED ) {
            String key = keyForID( context, id );
            if ( null != key ) {
                associateContextString( context, key );
                result = getXlation( context, key, canUseDB );
            }
        }

        if ( null == result ) {
            result = context.getString( id );
        }

        return result;
    }

    public static String getString( Context context, int id, Object... params )
    {
        Assert.assertNotNull( params );
        String result;
        if ( XWApp.LOCUTILS_ENABLED ) {
            result = getString( context, id );
            if ( null != result ) {
                try {
                    result = String.format( result, params );
                } catch ( IllegalFormatConversionException fce ) {
                    dropXLations( context );
                    result = getString( context, id, params );
                }
            }
        } else {
            result = context.getString( id, params );
        }
        return result;
    }

    public static String getQuantityString( Context context, int id,
                                            int quantity )
    {
        if ( XWApp.LOCUTILS_ENABLED ) {
            Log.w( TAG, "getQuantityString(%d): punting on locutils stuff for"
                   + " now. FIXME", quantity );
        }
        String result = context.getResources().getQuantityString( id, quantity );
        return result;
    }

    public static String getQuantityString( Context context, int id,
                                            int quantity, Object... params )
    {
        if ( XWApp.LOCUTILS_ENABLED ) {
            Log.w( TAG, "getQuantityString(%d): punting on locutils stuff for"
                   + " now. FIXME", quantity );
        }
        String result = context.getResources()
            .getQuantityString( id, quantity, params );
        return result;
    }

    public static void setXlation( Context context, String key, CharSequence txt )
    {
        if ( XWApp.LOCUTILS_ENABLED ) {
            loadXlations( context );
            if ( null == txt || 0 == txt.length() ) {
                s_xlationsLocal.remove( key );
            } else {
                s_xlationsLocal.put( key, txt.toString() );
            }
        }
    }

    protected static String getLocalXlation( Context context, String key,
                                             boolean canUseDB )
    {
        String result = null;
        if ( XWApp.LOCUTILS_ENABLED ) {
            if ( canUseDB ) {
                loadXlations( context );
            }
            if ( null != s_xlationsLocal ) {
                result = s_xlationsLocal.get( key );
            }
        }
        return result;
    }

    protected static String getBlessedXlation( Context context, String key,
                                               boolean canUseDB )
    {
        String result = null;
        if ( XWApp.LOCUTILS_ENABLED ) {
            if ( canUseDB ) {
                loadXlations( context );
            }
            if ( null != s_xlationsBlessed ) {
                result = s_xlationsBlessed.get( key );
            }
        }
        return result;
    }

    protected static String getXlation( Context context, String key,
                                        boolean canUseDB )
    {
        String result = null;

        result = getLocalXlation( context, key, canUseDB );
        if ( null == result ) {
            result = getBlessedXlation( context, key, canUseDB );
        }

        return result;
    }

    public static void saveLocalData( Context context )
    {
        if ( XWApp.LOCUTILS_ENABLED ) {
            DBUtils.saveXlations( context, getCurLocale( context ),
                                  s_xlationsLocal, false );
        }
    }

    public static JSONArray makeForXlationUpdate( Context context )
    {
        JSONArray result = null;
        if ( XWApp.LOCUTILS_ENABLED ) {
            String locale = getCurLocale( context );
            String fake = XWPrefs.getFakeLocale( context );
            result = new JSONArray().put( entryForLocale( context, locale ) );
            if ( null != fake && 0 < fake.length() && ! fake.equals(locale) ) {
                result.put( entryForLocale( context, fake ) );
            }
        }
        return result;
    }

    private static JSONObject entryForLocale( Context context, String locale )
    {
        JSONObject result = null;
        if ( XWApp.LOCUTILS_ENABLED ) {
            try {
                String version =
                    DBUtils.getStringFor( context, localeKey(locale), "0" );
                result = new JSONObject()
                    .put( k_LOCALE, locale )
                    .put( k_XLATEVERS, version );
            } catch ( org.json.JSONException jse ) {
                Log.ex( TAG, jse );
            }
        }
        return result;
    }

    private static String localeKey( String locale )
    {
        return String.format( "%s:%s", k_XLATEVERS, locale );
    }

    private static final String k_OLD = "old";
    private static final String k_NEW = "new";
    private static final String k_PAIRS = "pairs";

    public static int addXlations( Context context, JSONArray data )
    {
        int nAdded = 0;
        if ( XWApp.LOCUTILS_ENABLED ) {
            try {
                int nLocales = data.length();
                for ( int ii = 0; ii < nLocales; ++ii ) {
                    JSONObject entry = data.getJSONObject( ii );
                    String locale = entry.getString( k_LOCALE );
                    String newVersion = entry.getString( k_NEW );
                    JSONArray pairs = entry.getJSONArray( k_PAIRS );
                    Log.i( TAG, "addXlations: locale %s: got pairs of len %d,"
                           + " version %s", locale,
                           pairs.length(), newVersion );

                    int len = pairs.length();
                    Map<String,String> newXlations = new HashMap<>( len );
                    for ( int jj = 0; jj < len; ++jj ) {
                        JSONObject pair = pairs.getJSONObject( jj );
                        int id = pair.getInt( "id" );
                        String key = context.getString( id );
                        Assert.assertNotNull( key );
                        String txt = pair.getString( "loc" );
                        txt = replaceEscaped( txt );
                        newXlations.put( key, txt );
                    }

                    DBUtils.saveXlations( context, locale, newXlations, true );
                    DBUtils.setStringFor( context, localeKey(locale), newVersion );
                    nAdded += len;
                }
                s_xlationsBlessed = null;
                loadXlations( context );
            } catch ( org.json.JSONException jse ) {
                Log.ex( TAG, jse );
            }
        }
        return nAdded;
    }

    protected static LocSearcher.Pair[] makePairs( Context context )
    {
        loadXlations( context );

        Map<String,Integer> map = LocIDs.getS_MAP( context );
        int siz = map.size();
        LocSearcher.Pair[] result = new LocSearcher.Pair[siz];
        Iterator<String> iter = map.keySet().iterator();

        for ( int ii = 0; iter.hasNext(); ++ii ) {
            String key = iter.next();
            String english = context.getString( map.get( key ) );
            Assert.assertTrue( english.equals( key ) );
            String xlation = getXlation( context, key, true );
            result[ii] = new LocSearcher.Pair( key, english, xlation );
        }
        return result;
    }

    private static void xlateMenu( final Activity activity,
                                   final WeakReference<Menu> rootRef,
                                   Menu menu, int depth )
    {
        int count = menu.size();
        for ( int ii = 0; ii < count; ++ii ) {
            MenuItem item = menu.getItem( ii );
            CharSequence ts = item.getTitle();
            if ( null != ts ) {
                String title = ts.toString();
                if ( LocIDs.getS_MAP( activity ).containsKey(title) ) {
                    associateMenuString( rootRef, title );

                    title = xlateString( activity, title, false );
                    item.setTitle( title );
                }
            }

            if ( item.hasSubMenu() ) {
                xlateMenu( activity, rootRef, item.getSubMenu(), 1 + depth );
            }
        }

        // The caller is loc-aware, so add our menu -- at the top level!
        if ( 0 == depth && XWPrefs.getXlationEnabled( activity ) ) {
            String title = activity.getString( R.string.loc_menu_xlate );
            associateMenuString( rootRef, title );

            String xlated = getXlation( activity, title, true );
            if ( null != xlated ) {
                title = xlated;
            }
            menu.add( title )
                .setOnMenuItemClickListener( new OnMenuItemClickListener() {
                        public boolean onMenuItemClick( MenuItem item ) {
                            s_latestMenuRef = rootRef;

                            Intent intent =
                                new Intent( activity, LocActivity.class );
                            intent.putExtra( CONTEXT_NAME,
                                             activity.getClass().getName() );
                            activity.startActivity( intent );
                            return true;
                        }
                    });
        }
    }

    private static void pareMenus()
    {
        DbgUtils.assertOnUIThread();

        Set<WeakReference<Menu>> keys = s_menuSets.keySet();
        Iterator<WeakReference<Menu>> iter = keys.iterator();
        while ( iter.hasNext() ) {
            WeakReference<Menu> ref = iter.next();
            if ( null == ref.get() ) {
                iter.remove();
            }
        }
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
            String lang = null;
            String locale = XWPrefs.getFakeLocale( context );
            if ( null != locale && 0 < locale.length() ) {
                lang = splitLocale( locale );
            }
            if ( null == lang ) {
                lang = Locale.getDefault().getLanguage();
                // sometimes I get "en-us" in this case, i.e. the locale's
                // there too. Strip it.
                if ( lang.contains( "-" ) ) {
                    lang = TextUtils.split(lang, "-")[0];
                }
            }
            s_curLang = lang;
        }
        return s_curLang;
    }

    public static String getCurLocale( Context context )
    {
        if ( null == s_curLocale ) {
            String locale = XWPrefs.getFakeLocale( context );
            if ( null == locale || 0 == locale.length() ) {
                locale = Locale.getDefault().toString();
            }
            s_curLocale = locale;
        }
        return s_curLocale;
    }

    private static void loadXlations( Context context )
    {
        if ( null == s_xlationsLocal || null == s_xlationsBlessed ) {
            Object[] asObjs =
                DBUtils.getXlations( context, getCurLocale( context ) );
            s_xlationsLocal = (Map<String,String>)asObjs[0];
            s_xlationsBlessed = (Map<String,String>)asObjs[1];
            Log.i( TAG, "loadXlations: got %d local strings, %d blessed strings",
                   s_xlationsLocal.size(), s_xlationsBlessed.size() );
        }
    }

    private static String keyForID( Context context, int id )
    {
        if ( null == s_idsToKeys ) {
            Map<String,Integer> map = LocIDs.getS_MAP( context );
            HashMap<Integer, String> idsToKeys = new HashMap<>( map.size() );

            Iterator<String> iter = map.keySet().iterator();
            while ( iter.hasNext() ) {
                String key = iter.next();
                idsToKeys.put( map.get( key ), key );
            }
            s_idsToKeys = idsToKeys;
        }

        return s_idsToKeys.get( id );
    }

    private static void xlateView( Context context, String contextName,
                                   View view, int depth )
    {
        // DbgUtils.logf( "xlateView(depth=%d, view=%s, canRecurse=%b)", depth,
        //                view.getClass().getName(), view instanceof ViewGroup );
        if ( view instanceof Button ) {
            Button button = (Button)view;
            String str = xlateAndStore( context, button.getText() );
            if ( null != str ) {
                button.setText( str );
            }
        } else if ( view instanceof TextView ) {
            TextView tv = (TextView)view;
            String str = xlateAndStore( context, tv.getText() );
            if ( null != str ) {
                tv.setText( str );
            }
        } else if ( view instanceof Spinner ) {
            Spinner sp = (Spinner)view;
            String str = xlateAndStore( context, sp.getPrompt() );
            if ( null != str ) {
                sp.setPrompt( str );
            }
            SpinnerAdapter adapter = sp.getAdapter();
            if ( null != adapter ) {
                sp.setAdapter( new XlatingSpinnerAdapter( context, adapter ) );
            }
        }

        // A Spinner, for instance, ISA ViewGroup, so this is a separate test.
        if ( view instanceof ViewGroup ) {
            ViewGroup asGroup = (ViewGroup)view;
            int count =	asGroup.getChildCount();
            for ( int ii = 0; ii < count; ++ii ) {
                View child = asGroup.getChildAt( ii );
                xlateView( context, contextName, child, depth + 1 );
            }
        }
    }

    public static void xlatePreferences( Context context, Preference pref,
                                         int depth )
    {
        // DbgUtils.logf( "xlatePreferences(depth=%d, view=%s, canRecurse=%b)", depth,
        //                pref.getClass().getName(), pref instanceof PreferenceGroup );

        String str = xlateString( context, pref.getSummary() );
        if ( null != str ) {
            pref.setSummary( str );
        }
        str = xlateString( context, pref.getTitle() );
        if ( null != str ) {
            pref.setTitle( str );
        }

        if ( pref instanceof DialogPreference ) {
            DialogPreference dp = (DialogPreference)pref;
            if ( null != (str = xlateString( context, dp.getDialogMessage()))) {
                dp.setDialogMessage( str );
            }
            if ( null != (str = xlateString( context, dp.getDialogTitle()))) {
                dp.setDialogTitle( str );
            }
            if ( null != (str = xlateString( context, dp.getNegativeButtonText()))) {
                dp.setNegativeButtonText( str );
            }
            if ( null != (str = xlateString( context, dp.getPositiveButtonText()))) {
                dp.setPositiveButtonText( str );
            }

            // ListPreference isa DialogPreference
            if ( dp instanceof ListPreference ) {
                ListPreference lp = (ListPreference) dp;
                CharSequence[] entries = lp.getEntries();
                if ( null != entries ) {
                    CharSequence[] newEntries = xlateStrings( context, entries );
                    lp.setEntries( newEntries );
                }
            }
        }

        if ( pref instanceof PreferenceGroup ) {
            PreferenceGroup group = (PreferenceGroup)pref;
            int count = group.getPreferenceCount();
            for ( int ii = 0; ii < count; ++ii ) {
                xlatePreferences( context, group.getPreference(ii), 1 + depth );
            }
        }
    }

    public static boolean inLatestMenu( String key )
    {
        boolean result = false;
        if ( null != s_latestMenuRef ) {
            HashSet<String> keys = s_menuSets.get( s_latestMenuRef );
            if ( null != keys ) {
                result = keys.contains( key );
            }
        }
        return result;
    }

    public static boolean inLatestScreen( String key, String contextName )
    {
        boolean result = false;
        HashSet<String> keys = s_contextSets.get( contextName );
        if ( null != keys ) {
            result = keys.contains( key );
        }
        return result;
    }

    private static String xlateAndStore( Context context, CharSequence cs )
    {
        String result = null;
        if ( null == cs ) {
            // DbgUtils.logf( "xlateAndStore: cs null" );
        } else if ( 0 == cs.length() ) {
            Assert.assertTrue( 0 == cs.toString().length() );
            // DbgUtils.logf( "xlateAndStore: cs 0 len" );
        } else {
            String key = cs.toString();
            // DbgUtils.logf( "xlateAndStore: key=%s", key );
            result = xlateString( context, key );
            if ( null != result ) {
                associateContextString( context, key );
            }
        }
        return result;
    }

    private static void dropXLations( Context context )
    {
        s_xlationsBlessed = null;
        s_idsToKeys = null;

        String locale = getCurLocale( context );
        String msg = String.format( "Dropping bad translations for %s", locale );
        Utils.showToast( context, msg );
        Log.w( TAG, msg );

        DBUtils.dropXLations( context, locale );
        DBUtils.setStringFor( context, localeKey(locale), "" );
    }

    // Add key (english string) to the hashset associated with this menu
    private static void associateMenuString( WeakReference<Menu> ref, String key )
    {
        HashSet<String> keys = s_menuSets.get( ref );
        if ( null == keys ) {
            keys = new HashSet<>();
            s_menuSets.put( ref, keys );
        }
        keys.add( key );
    }

    private static void associateContextString( Context context, final String key )
    {
        associateContextString( context.getClass().getName(), key );
    }

    private static void associateContextString( String contextName, final String key )
    {
        HashSet<String> keys = s_contextSets.get( contextName );
        if ( null == keys ) {
            keys = new HashSet<>();
            s_contextSets.put( contextName, keys );
            // DbgUtils.logf( "adding keys hash to %s", contextName );
        }
        keys.add( key );
        // DbgUtils.logf( "associated with context %s string '%s'", contextName, key );
    }

    private static Pattern s_patUnicode = Pattern.compile("(\\\\[Uu][0-9a-fA-F]{4})");
    private static Pattern s_patCr = Pattern.compile("\\\\n");

    private static String replaceEscaped( String txt )
    {
        // String orig = txt;

        // Swap unicode escapes for real chars
        Matcher matcher = s_patUnicode.matcher( txt );
        StringBuffer sb = new StringBuffer();
        while ( matcher.find() ) {
            int start = matcher.start();
            int end = matcher.end();
            String match = txt.substring( start, end );
            char ch = (char)Integer.parseInt( match.substring(2), 16 );
            matcher.appendReplacement( sb, String.valueOf(ch) );
        }
        matcher.appendTail(sb);
        txt = sb.toString();

        // Swap in real carriage returns
        txt = s_patCr.matcher( txt ).replaceAll( "\n" );

        // if ( ! orig.equals( txt ) ) {
        //     DbgUtils.logf( "replaceEscaped: <<%s>> -> <<%s>>", orig, txt );
        // }
        return txt;
    }

    private static String splitLocale( String locale )
    {
        String result = null;
        String[] tuple = TextUtils.split( locale, "_" );
        if ( null != tuple && 2 == tuple.length ) {
            result = tuple[0];
        }
        return result;
    }

    public static AlertDialog.Builder makeAlertBuilder( Context context )
    {
        return new AlertBuilder( context );
    }

    private static class AlertBuilder extends AlertDialog.Builder {
        Context m_context;

        private AlertBuilder( Context context )
        {
            super( context );
            m_context = context;
        }

        @Override
        public AlertDialog.Builder setTitle( int id )
        {
            if ( 0 != id ) {
                String str = getString( m_context, id );
                setTitle( str );
            }
            return this;
        }

        public AlertDialog.Builder setMessage( int textId )
        {
            String str = getString( m_context, textId );
            return setMessage( str );
        }

        @Override
        public AlertDialog.Builder setPositiveButton( int textId,
                                                      OnClickListener listener )
        {
            String str = getString( m_context, textId );
            return setPositiveButton( str, listener );
        }
        @Override
        public AlertDialog.Builder setNeutralButton( int textId,
                                                     OnClickListener listener )
        {
            String str = getString( m_context, textId );
            return setNeutralButton( str, listener );
        }

        @Override
        public AlertDialog.Builder setNegativeButton( int textId,
                                                      OnClickListener listener )
        {
            String str = getString( m_context, textId );
            return setNegativeButton( str, listener );
        }
    }

}
