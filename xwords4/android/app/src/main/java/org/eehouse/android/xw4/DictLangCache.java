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
import android.text.TextUtils;

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
import java.util.List;
import java.util.Map;
import java.util.Set;

public class DictLangCache {
    private static final String TAG = DictLangCache.class.getSimpleName();
    private static Map<String, String> s_langNames;
    private static Map<String, String> s_langCodes;

    private static String s_adaptedLang = null;
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

    // This populates the cache and will take significant time if it's mostly
    // empty and there are a lot of dicts.
    public static int getLangCount( Context context, String isoCode )
    {
        int count = 0;
        DictAndLoc[] dals = DictUtils.dictList( context );
        for ( DictAndLoc dal : dals ) {
            if ( isoCode.equals( getDictISOCode( context, dal ) ) ) {
                ++count;
            }
        }
        return count;
    }

    private static DictInfo[] getInfosHaveLang( Context context, String isoCode )
    {
        List<DictInfo> al = new ArrayList<>();
        DictAndLoc[] dals = DictUtils.dictList( context );
        for ( DictAndLoc dal : dals ) {
            DictInfo info = getInfo( context, dal );
            if ( null != info && isoCode.equals( info.isoCode ) ) {
                al.add( info );
            }
        }
        DictInfo[] result = al.toArray( new DictInfo[al.size()] );
        return result;
    }

    public static boolean haveDict( Context context, String isoCode, String name )
    {
        boolean found = false;
        DictInfo[] infos = getInfosHaveLang( context, isoCode );
        for ( DictInfo info : infos ) {
            if ( info.name.equals( name ) ) {
                found = true;
                break;
            }
        }
        return found;
    }

    private static String[] getHaveLang( Context context, String isoCode,
                                         Comparator<DictInfo> comp,
                                         boolean withCounts )
    {
        DictInfo[] infos = getInfosHaveLang( context, isoCode );

        if ( null != comp ) {
            Arrays.sort( infos, comp );
        }

        List<String> al = new ArrayList<>();
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

    public static String[] getHaveLang( Context context, String isoCode )
    {
        return getHaveLang( context, isoCode, null, false );
    }

    public static DictAndLoc[] getDALsHaveLang( Context context, String isoCode )
    {
        Assert.assertNotNull( isoCode );
        List<DictAndLoc> al = new ArrayList<>();
        DictAndLoc[] dals = DictUtils.dictList( context );

        makeMaps( context );

        for ( DictAndLoc dal : dals ) {
            DictInfo info = getInfo( context, dal );
            if ( null != info ) {
                Assert.assertTrueNR( s_langNames.containsKey( info.isoCode ) );
                if ( isoCode.equals( info.isoCode ) ) {
                    al.add( dal );
                }
            }
        }
        DictAndLoc[] result = al.toArray( new DictAndLoc[al.size()] );
        Log.d( TAG, "getDALsHaveLang(%s) => %s", isoCode, result );
        return result;
    }

    private static Comparator<DictInfo> s_ByCount =
        new Comparator<DictInfo>() {
            public int compare( DictInfo di1, DictInfo di2 )
            {
                return di2.wordCount - di1.wordCount;
            }
        };

    public static String[] getHaveLangByCount( Context context, String isoCode )
    {
        return getHaveLang( context, isoCode, s_ByCount, false );
    }

    public static String[] getHaveLangCounts( Context context, String isoCode )
    {
        return getHaveLang( context, isoCode, null, true );
    }

    public static String stripCount( String nameWithCount )
    {
        int indx = nameWithCount.lastIndexOf( " (" );
        return nameWithCount.substring( 0, indx );
    }

    public static String getDictISOCode( Context context, DictAndLoc dal )
    {
        return getInfo( context, dal ).isoCode;
    }

    public static String getDictISOCode( Context context, String dictName )
    {
        DictInfo info = getInfo( context, dictName );
        return info.isoCode;
    }

    public static String getLangNameForISOCode( Context context, String isoCode )
    {
        makeMaps( context );
        return s_langNames.get( isoCode );
    }

    public static void setLangNameForISOCode( Context context, String isoCode,
                                              String langName )
    {
        makeMaps( context );
        putTwo( isoCode, langName );
    }

    public static String getLangIsoCode( Context context, String langName )
    {
        makeMaps( context );
        String result = s_langCodes.get( langName );
        Log.d( TAG, "getLangIsoCode(%s) => %s", langName, result );
        return result;
    }

    public static String getDictLangName( Context context, String dictName )
    {
        String isoCode = getDictISOCode( context, dictName );
        return getLangNameForISOCode( context, isoCode );
    }

    public static String[] getDictMD5Sums( Context context, String dict )
    {
        String[] result = {null, null};
        DictInfo info = getInfo( context, dict );
        if ( null != info ) {
            result[0] = info.md5Sum;
            result[1] = info.fullSum;
        }
        return result;
    }

    public static long getFileLen( Context context, DictAndLoc dal )
    {
        File path = dal.getPath( context );
        return path.length();
    }

    public static String getLangName( Context context, String dictName )
    {
        String isoCode = getDictISOCode( context, dictName );
        return getLangNameForISOCode( context, isoCode );
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

    public static String getBestDefault( Context context, String isoCode,
                                         boolean human )
    {
        String dictName = human? CommonPrefs.getDefaultHumanDict( context )
            : CommonPrefs.getDefaultRobotDict( context );
        if ( ! isoCode.equals( getDictISOCode( context, dictName ) ) ) {
            String dicts[] = getHaveLangByCount( context, isoCode );
            if ( dicts.length > 0 ) {
                // Human gets biggest; robot gets smallest
                dictName = dicts[ human ? 0 : dicts.length-1 ];
            } else {
                dictName = null;
            }
        }
        return dictName;
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
                                                        String isoCode )
    {
        if ( ! isoCode.equals( s_adaptedLang ) ) {
            s_dictsAdapter =
                new ArrayAdapter<>(context, android.R.layout.simple_spinner_item);
            rebuildAdapter( s_dictsAdapter, getHaveLang( context, isoCode ) );
            s_adaptedLang = isoCode;
        }
        return s_dictsAdapter;
    }

    private static void putTwo( String isoCode, String langName )
    {
        Log.d( TAG, "putTwo(): adding %s => %s", langName, isoCode );
        Assert.assertTrueNR( !TextUtils.isEmpty(isoCode)
                             && !TextUtils.isEmpty(langName) );
        s_langCodes.put( langName, isoCode );
        s_langNames.put( isoCode, langName );
    }

    private static void makeMaps( Context context )
    {
        if ( null == s_langNames ) {
            s_langCodes = new HashMap<>();
            s_langNames = new HashMap<>();

            Resources res = context.getResources();
            String[] entries  = res.getStringArray( R.array.language_names );
            for ( int ii = 0; ii < entries.length; ii += 2 ) {
                String isoCode = entries[ii];
                String langName = entries[ii+1];
                putTwo( isoCode, langName );
            }

            // Now deal with any dicts too new for their isoCodes to be in
            // language_names
            DictAndLoc[] dals = DictUtils.dictList( context ) ;
            for ( DictAndLoc dal : dals ) {
                DictInfo info = getInfo( context, dal );
                String isoCode = info.isoCode;
                Assert.assertTrueNR( null != isoCode );
                if ( !s_langNames.containsKey( isoCode ) ) {
                    Log.d( TAG, "looking at info %s", info );
                    Assert.assertTrueNR( null != info.langName );
                    putTwo( isoCode, info.langName );
                }
            }
        }
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
        if ( null != info && null == info.isoCode ) {
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
                info.fullSum = Utils.getMD5SumFor( context, dal );
                Assert.assertTrueNR( null != info.fullSum );

                DBUtils.dictsSetInfo( context, dal, info );
                Log.d( TAG, "getInfo() => %s", info );
            } else {
                Log.i( TAG, "getInfo(): unable to open dict %s", dal.name );
            }
        }
        return info;
    }
}
