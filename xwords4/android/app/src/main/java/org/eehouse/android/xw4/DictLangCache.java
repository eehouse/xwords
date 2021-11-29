/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 by Eric House (xwords@eehouse.org).  All rights
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
import android.content.res.Resources;
import android.os.Handler;
import android.widget.ArrayAdapter;


import org.eehouse.android.xw4.DictUtils.DictAndLoc;
import org.eehouse.android.xw4.DictUtils.DictLoc;
import org.eehouse.android.xw4.jni.CommonPrefs;
import org.eehouse.android.xw4.jni.DictInfo;
import org.eehouse.android.xw4.jni.JNIUtilsImpl;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;

public class DictLangCache {
    private static final String TAG = DictLangCache.class.getSimpleName();
    private static Map<Integer, String> s_langNames;
    private static Map<String, Integer> s_langCodes;
    private static Map<Integer, String> s_langCodeStrs;

    private static int s_adaptedLang = -1;
    private static LangsArrayAdapter s_langsAdapter;
    private static ArrayAdapter<String> s_dictsAdapter;
    private static String s_last;
    private static Handler s_handler;


    public static class LangsArrayAdapter extends ArrayAdapter<String> {
        private Context m_context;
        private Map<String, String> m_map;

        public LangsArrayAdapter( Context context, int itemLayout ) {
            super( context, itemLayout );
            m_context = context;
        }

        public void rebuild()
        {
            m_map = new HashMap<>();
            DictAndLoc[] dals = DictUtils.dictList( m_context );
            for ( DictAndLoc dal : dals ) {
                String lang = getLangName( m_context, dal.name );
                if ( null != lang && 0 != lang.length() ) {
                    if ( ! m_map.containsValue( lang ) ) {
                        String locName = LocUtils.xlateLang( m_context, lang,
                                                             true );
                        m_map.put( locName, lang );
                    }
                }
            }

            // Now build the array data
            clear();
            for ( Iterator<String> iter = m_map.keySet().iterator();
                  iter.hasNext(); ) {
                String locName = iter.next();
                add( locName );
            }
            if ( null != s_last ) {
                add( s_last );
            }
            sort( KeepLast );
        }

        public int getPosForLang( String lang )
        {
            int result = -1;
            for ( int ii = 0; ii < getCount(); ++ii ) {
                String code = getLangAtPosition( ii );
                if ( code.equals( lang ) ) {
                    result = ii;
                    break;
                }
            }
            return result;
        }

        public String getLangAtPosition( int position )
        {
            String locName = getItem( position );
            String result = m_map.get( locName );
            return result;
        }
    }

    private static Comparator<String> KeepLast =
        new Comparator<String>() {
            public int compare( String str1, String str2 )
            {
                if ( s_last.equals( str1 ) ) {
                    return 1;
                } else if ( s_last.equals( str2 ) ) {
                    return -1;
                } else {
                    return str1.compareToIgnoreCase( str2 );
                }
            }
        };

    public static String annotatedDictName( Context context, DictAndLoc dal )
    {
        String result = null;
        DictInfo info = getInfo( context, dal );
        if ( null != info ) {
            int wordCount = info.wordCount;

            String langName = getLangName( context, dal.name );
            String locName = LocUtils.xlateLang( context, langName );
            result = LocUtils.getString( context, R.string.dict_desc_fmt,
                                         dal.name, locName,
                                         wordCount );
        }
        return result;
    }

    public static String annotatedDictName( Context context, String name,
                                            int lang )
    {
        return name + " (" + getLangName( context, lang ) + ")";
    }

    public static String getLangName( Context context, int code )
    {
        makeMaps( context );
        String name = s_langNames.get( code );
        if ( null == name ) {
            name = s_langNames.get( 0 );
        }
        return name;
    }

    // This populates the cache and will take significant time if it's
    // mostly empty and there are a lot of dicts.
    public static int getLangCount( Context context, int code )
    {
        int count = 0;
        DictAndLoc[] dals = DictUtils.dictList( context );
        for ( DictAndLoc dal : dals ) {
            if ( code == getDictLangCode( context, dal ) ) {
                ++count;
            }
        }
        return count;
    }

    private static DictInfo[] getInfosHaveLang( Context context, int code )
    {
        ArrayList<DictInfo> al = new ArrayList<>();
        DictAndLoc[] dals = DictUtils.dictList( context );
        for ( DictAndLoc dal : dals ) {
            DictInfo info = getInfo( context, dal );
            if ( null != info && code == info.langCode ) {
                al.add( info );
            }
        }
        DictInfo[] result = al.toArray( new DictInfo[al.size()] );
        return result;
    }

    public static boolean haveDict( Context context, String lang, String name )
    {
        boolean result = false;
        makeMaps( context );
        Integer code = s_langCodes.get( lang );
        if ( null != code ) {
            result = haveDict( context, code, name );
        }
        return result;
    }

    public static boolean haveDict( Context context, int code, String name )
    {
        boolean found = false;
        DictInfo[] infos = getInfosHaveLang( context, code );
        for ( DictInfo info : infos ) {
            if ( info.name.equals( name ) ) {
                found = true;
                break;
            }
        }
        return found;
    }

    private static String[] getHaveLang( Context context, int code,
                                         Comparator<DictInfo> comp,
                                         boolean withCounts )
    {
        DictInfo[] infos = getInfosHaveLang( context, code );

        if ( null != comp ) {
            Arrays.sort( infos, comp );
        }

        ArrayList<String> al = new ArrayList<>();
        String fmt = "%s (%d)"; // must match stripCount below
        for ( DictInfo info : infos ) {
            String name = info.name;
            if ( withCounts ) {
                name = String.format( fmt, name, info.wordCount );
            }
            al.add( name );
        }
        String[] result = al.toArray( new String[al.size()] );
        if ( null == comp ) {
            Arrays.sort( result );
        }
        return result;
    }

    public static String[] getHaveLang( Context context, int code )
    {
        return getHaveLang( context, code, null, false );
    }

    public static DictAndLoc[] getDALsHaveLang( Context context, int code )
    {
        ArrayList<DictAndLoc> al = new ArrayList<>();
        DictAndLoc[] dals = DictUtils.dictList( context );

        makeMaps( context );

        for ( DictAndLoc dal : dals ) {
            DictInfo info = getInfo( context, dal );
            int langCode = info.langCode;
            if ( !s_langNames.containsKey( langCode ) ) {
                langCode = 0;
            }
            if ( null != info && code == langCode ) {
                al.add( dal );
            }
        }
        DictAndLoc[] result = al.toArray( new DictAndLoc[al.size()] );
        return result;
    }

    private static Comparator<DictInfo> s_ByCount =
        new Comparator<DictInfo>() {
            public int compare( DictInfo di1, DictInfo di2 )
            {
                return di2.wordCount - di1.wordCount;
            }
        };

    public static String[] getHaveLangByCount( Context context, int code )
    {
        return getHaveLang( context, code, s_ByCount, false );
    }

    public static String[] getHaveLangCounts( Context context, int code )
    {
        return getHaveLang( context, code, null, true );
    }

    public static String stripCount( String nameWithCount )
    {
        int indx = nameWithCount.lastIndexOf( " (" );
        return nameWithCount.substring( 0, indx );
    }

    public static int getDictLangCode( Context context, DictAndLoc dal )
    {
        return getInfo( context, dal ).langCode;
    }

    public static String getLangCodeStr( Context context, int code )
    {
        makeMaps( context );
        return s_langCodeStrs.get( code );
    }

    public static String getDictMD5Sum( Context context, String dict )
    {
        String result = null;
        DictInfo info = getInfo( context, dict );
        if ( null != info ) {
            result = info.md5Sum;
        }
        return result;
    }

    public static long getFileLen( Context context, DictAndLoc dal )
    {
        File path = dal.getPath( context );
        return path.length();
    }

    public static int getDictLangCode( Context context, String dict )
    {
        return getInfo( context, dict ).langCode;
    }

    public static int getLangLangCode( Context context, String lang )
    {
        makeMaps( context );

        Integer code = s_langCodes.get( lang );
        if ( null == code ) {
            code = 0;
        }
        return code;
    }

    public static String userLangForLc( Context context, String lc )
    {
        makeMaps( context );
        String result = null;

        for ( Integer code : s_langCodeStrs.keySet() ) {
            if ( s_langCodeStrs.get(code).equals(lc) ) {
                result = s_langNames.get(code);
                break;
            }
        }

        return result;
    }

    public static String getLangName( Context context, String dict )
    {
        int code = getDictLangCode( context, dict );
        return getLangName( context, code );
    }

    // May be called from background thread
    public static void inval( final Context context, String name,
                              DictLoc loc, boolean added )
    {
        DBUtils.dictsRemoveInfo( context, DictUtils.removeDictExtn( name ) );

        if ( added ) {
            DictAndLoc dal = new DictAndLoc( name, loc );
            getInfo( context, dal );
        }

        if ( null != s_handler ) {
            s_handler.post( new Runnable() {
                    public void run() {
                        if ( null != s_dictsAdapter ) {
                            rebuildAdapter( s_dictsAdapter,
                                            DictLangCache.
                                            getHaveLang( context,
                                                         s_adaptedLang ) );
                        }
                        if ( null != s_langsAdapter ) {
                            s_langsAdapter.rebuild();
                        }
                    }
                } );
        }
    }

    public static String[] listLangs( Context context )
    {
        return listLangs( context, DictUtils.dictList( context ) );
    }

    public static String[] listLangs( Context context, DictAndLoc[] dals )
    {
        Set<String> langs = new HashSet<>();
        for ( DictAndLoc dal : dals ) {
            String name = getLangName( context, dal.name );
            if ( null == name || 0 == name.length() ) {
                Log.w( TAG, "bad lang name for dal name %s", dal.name );
                // Assert.fail();
            }
            langs.add( name );
        }
        String[] result = new String[langs.size()];
        return langs.toArray( result );
    }

    public static String getBestDefault( Context context, int lang,
                                         boolean human )
    {
        String dict = human? CommonPrefs.getDefaultHumanDict( context )
            : CommonPrefs.getDefaultRobotDict( context );
        if ( lang != DictLangCache.getDictLangCode( context, dict ) ) {
            String dicts[] = getHaveLangByCount( context, lang );
            if ( dicts.length > 0 ) {
                // Human gets biggest; robot gets smallest
                dict = dicts[ human ? 0 : dicts.length-1 ];
            } else {
                dict = null;
            }
        }
        return dict;
    }

    private static void rebuildAdapter( ArrayAdapter<String> adapter,
                                        String[] items )
    {
        adapter.clear();

        for ( String item : items ) {
            adapter.add( item );
        }
        if ( null != s_last ) {
            adapter.add( s_last );
        }
        adapter.sort( KeepLast );
    }

    public static void setLast( String lastItem )
    {
        s_last = lastItem;
        s_handler = new Handler();
    }

    public static LangsArrayAdapter getLangsAdapter( Context context )
    {
        if ( null == s_langsAdapter ) {
            s_langsAdapter =
                new LangsArrayAdapter( context,
                                       android.R.layout.simple_spinner_item );
            s_langsAdapter.rebuild();
        }
        return s_langsAdapter;
    }

    public static ArrayAdapter<String> getDictsAdapter( Context context,
                                                        int lang )
    {
        if ( lang != s_adaptedLang ) {
            s_dictsAdapter =
                new ArrayAdapter<>(context, android.R.layout.simple_spinner_item);
            rebuildAdapter( s_dictsAdapter, getHaveLang( context, lang ) );
            s_adaptedLang = lang;
        }
        return s_dictsAdapter;
    }

    private static void makeMaps( Context context )
    {
        if ( null == s_langNames ) {
            s_langCodes = new HashMap<>();
            s_langNames = new HashMap<>();
            s_langCodeStrs = new HashMap<>();

            Resources res = context.getResources();
            String[] entries  = res.getStringArray( R.array.languages_map );
            for ( int ii = 0; ii < entries.length; ii += 3 ) {
                Integer code = Integer.parseInt(entries[ii]);
                String name = entries[ii+1];
                String lc = entries[ii+2];
                s_langCodes.put( name, code );
                s_langNames.put( code, name );
                s_langCodeStrs.put( code, lc );
            }
        }
    }

    public static int getDictCount( Context context, String name )
    {
        int result = 0;
        for ( DictAndLoc dal : DictUtils.dictList( context ) ) {
            if ( name.equals( dal.name ) ) {
                ++result;
            }
        }
        Assert.assertTrue( result > 0 );
        return result;
    }

    private static DictInfo getInfo( Context context, String name )
    {
        DictInfo result = DBUtils.dictsGetInfo( context, name );
        if ( null == result ) {
            DictLoc loc = DictUtils.getDictLoc( context, name );
            result = getInfo( context, new DictAndLoc( name, loc ) );
        }
        return result;
    }

    private static DictInfo getInfo( Context context, DictAndLoc dal )
    {
        DictInfo info = DBUtils.dictsGetInfo( context, dal.name );

        // Tmp test that recovers from problem with new background download code
        if ( null != info && 0 == info.langCode ) {
            Log.w( TAG, "getInfo: dropping info for %s b/c lang code wrong",
                   dal.name );
            info = null;
        }

        if ( null == info ) {
            String[] names = { dal.name };
            DictUtils.DictPairs pairs = DictUtils.openDicts( context, names );

            info = XwJNI.dict_getInfo( pairs.m_bytes[0], dal.name,
                                       pairs.m_paths[0],
                                       DictLoc.DOWNLOAD == dal.loc );
            if ( null != info ) {
                info.name = dal.name;
                DBUtils.dictsSetInfo( context, dal, info );
            } else {
                Log.i( TAG, "getInfo(): unable to open dict %s", dal.name );
            }
        }
        return info;
    }
}
