/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 - 2022 by Eric House (xwords@eehouse.org).  All rights
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
import org.eehouse.android.xw4.Utils.ISOCode;

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

    private static ISOCode s_adaptedLang = null;
    private static LangsArrayAdapter s_langsAdapter;
    private static ArrayAdapter<String> s_dictsAdapter;
    private static String s_last;
    private static Handler s_handler;

    public static class LangsArrayAdapter extends ArrayAdapter<String> {
        private Context m_context;

        public LangsArrayAdapter( Context context, int itemLayout ) {
            super( context, itemLayout );
            m_context = context;
        }

        public void rebuild()
        {
            Set<String> langsSet = new HashSet<>();
            DictAndLoc[] dals = DictUtils.dictList( m_context );
            for ( DictAndLoc dal : dals ) {
                String lang = getDictLangName( m_context, dal.name );
                if ( null != lang && 0 != lang.length() ) {
                    langsSet.add( lang );
                }
            }

            // Now build the array data
            clear();
            for ( String str: langsSet ) {
                add( str );
            }
            if ( null != s_last ) {
                add( s_last );
            }
            sort( KeepLast );
        }

        public int getPosForLang( String langName )
        {
            int result = -1;
            for ( int ii = 0; ii < getCount(); ++ii ) {
                if ( langName.equals( getLangAtPosition( ii ) ) ) {
                    result = ii;
                    break;
                }
            }
            return result;
        }

        public String getLangAtPosition( int position )
        {
            return getItem( position );
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

            String langName = getDictLangName( context, dal.name );
            result = LocUtils.getString( context, R.string.dict_desc_fmt,
                                         dal.name, langName,
                                         wordCount );
        }
        return result;
    }

    // This populates the cache and will take significant time if it's mostly
    // empty and there are a lot of dicts.
    public static int getLangCount( Context context, ISOCode isoCode )
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

    private static DictInfo[] getInfosHaveLang( Context context, ISOCode isoCode )
    {
        List<DictInfo> al = new ArrayList<>();
        DictAndLoc[] dals = DictUtils.dictList( context );
        for ( DictAndLoc dal : dals ) {
            DictInfo info = getInfo( context, dal );
            if ( null != info && isoCode.equals( info.isoCode() ) ) {
                al.add( info );
            }
        }
        DictInfo[] result = al.toArray( new DictInfo[al.size()] );
        return result;
    }

    public static boolean haveDict( Context context, ISOCode isoCode, String dictName )
    {
        boolean found = false;
        DictInfo[] infos = getInfosHaveLang( context, isoCode );
        for ( DictInfo info : infos ) {
            if ( dictName.equals( info.name ) ) {
                found = true;
                break;
            }
        }
        return found;
    }

    private static String[] getHaveLang( Context context, ISOCode isoCode,
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

    public static String[] getHaveLang( Context context, ISOCode isoCode )
    {
        return getHaveLang( context, isoCode, null, false );
    }

    public static DictAndLoc[] getDALsHaveLang( Context context, ISOCode isoCode )
    {
        Assert.assertNotNull( isoCode );
        List<DictAndLoc> al = new ArrayList<>();
        DictAndLoc[] dals = DictUtils.dictList( context );

        for ( DictAndLoc dal : dals ) {
            DictInfo info = getInfo( context, dal );
            if ( null != info && isoCode.equals( info.isoCode() ) ) {
                al.add( dal );
            }
        }
        DictAndLoc[] result = al.toArray( new DictAndLoc[al.size()] );
        // Log.d( TAG, "getDALsHaveLang(%s) => %s", isoCode, result );
        return result;
    }

    private static Comparator<DictInfo> s_ByCount =
        new Comparator<DictInfo>() {
            public int compare( DictInfo di1, DictInfo di2 )
            {
                return di2.wordCount - di1.wordCount;
            }
        };

    public static String[] getHaveLangByCount( Context context, ISOCode isoCode )
    {
        return getHaveLang( context, isoCode, s_ByCount, false );
    }

    public static String[] getHaveLangCounts( Context context, ISOCode isoCode )
    {
        return getHaveLang( context, isoCode, null, true );
    }

    public static String stripCount( String nameWithCount )
    {
        int indx = nameWithCount.lastIndexOf( " (" );
        return nameWithCount.substring( 0, indx );
    }

    public static ISOCode getDictISOCode( Context context, DictAndLoc dal )
    {
        ISOCode result = getInfo( context, dal ).isoCode();
        Assert.assertTrueNR( null != result );
        return result;
    }

    public static ISOCode getDictISOCode( Context context, String dictName )
    {
        DictInfo info = getInfo( context, dictName );
        ISOCode result = info.isoCode();
        Assert.assertTrueNR( null != result );
        return result;
    }

    public static String getLangNameForISOCode( Context context, ISOCode isoCode )
    {
        String langName;
        try ( DLCache cache = DLCache.get( context ) ) {
            langName = cache.get( isoCode );

            if ( null == langName ) {
                // Any chance we have a installed dict providing this? How to
                // search given we can't read an ISOCode from a dict without
                // opening it.
            }
        }
        // Log.d( TAG, "getLangNameForISOCode(%s) => %s", isoCode, langName );
        return langName;
    }

    public static void setLangNameForISOCode( Context context, ISOCode isoCode,
                                              String langName )
    {
        // Log.d( TAG, "setLangNameForISOCode(%s=>%s)", isoCode, langName );
        try ( DLCache cache = DLCache.get( context ) ) {
            cache.put( isoCode, langName );
        }
    }

    public static ISOCode getLangIsoCode( Context context, String langName )
    {
        ISOCode result;
        try ( DLCache cache = DLCache.get( context ) ) {
            // Log.d( TAG, "looking for %s in %H", langName, cache );
            result = cache.get( langName );
        }

        if ( null == result ) {
            Assert.failDbg();
            // getinfo
        }

        // Log.d( TAG, "getLangIsoCode(%s) => %s", langName, result );
        // Assert.assertTrueNR( null != result );
        return result;
    }

    public static String getDictLangName( Context context, String dictName )
    {
        ISOCode isoCode = getDictISOCode( context, dictName );
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

    public static long getFileSize( Context context, DictAndLoc dal )
    {
        File path = dal.getPath( context );
        return path.length();
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
            String name = getDictLangName( context, dal.name );
            if ( null == name || 0 == name.length() ) {
                Log.w( TAG, "bad lang name for dal name %s", dal.name );

                DictInfo di = getInfo( context, dal );
                if ( null != di ) {
                    name = di.langName;
                    try ( DLCache cache = DLCache.get( context ) ) {
                        cache.put( di.isoCode(), name );
                    }
                }
            }
            if ( null != name && 0 < name.length() ) {
                langs.add( name );
            }
        }
        String[] result = new String[langs.size()];
        return langs.toArray( result );
    }

    public static String getBestDefault( Context context, ISOCode isoCode,
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
                                                        ISOCode isoCode )
    {
        if ( ! isoCode.equals( s_adaptedLang ) ) {
            s_dictsAdapter =
                new ArrayAdapter<>(context, android.R.layout.simple_spinner_item );
            rebuildAdapter( s_dictsAdapter, getHaveLang( context, isoCode ) );
            s_adaptedLang = isoCode;
        }
        return s_dictsAdapter;
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
        if ( null != info && null == info.isoCode() ) {
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
                // Log.d( TAG, "getInfo() => %s", info );
            } else {
                Log.i( TAG, "getInfo(): unable to open dict %s", dal.name );
            }
        }
        return info;
    }

    private static class DLCache implements AutoCloseable {
        private static final String CACHE_KEY_DATA = TAG + "/cache_data";
        private static final String CACHE_KEY_REV = TAG + "/cache_rev";
        private static DLCache[] sCache = {null};

        private HashMap<ISOCode, String> mLangNames = new HashMap<>();
        private int mCurRev;
        private transient boolean mDirty = false;
        private transient Context mContext;

        static DLCache get( Context context )
        {
            DLCache result;
            synchronized ( sCache ) {
                result = sCache[0];
                if ( null == result ) {
                    HashMap<ISOCode, String> data = (HashMap<ISOCode, String>)DBUtils
                        .getSerializableFor( context, CACHE_KEY_DATA );
                    if ( null != data ) {
                        int rev = DBUtils.getIntFor( context, CACHE_KEY_REV, 0 );
                        result = new DLCache( data, rev );
                        Log.d( TAG, "loaded cache: %s", result );
                    }
                }
                if ( null == result ) {
                    result = new DLCache();
                }
                result.update( context );
                sCache[0] = result;

                try {
                    while ( !result.tryLock( context ) ) {
                        sCache.wait();
                    }
                } catch ( InterruptedException ioe ) {
                    Log.ex( TAG, ioe );
                    Assert.failDbg();
                }
            }

            // Log.d( TAG, "getCache() => %H", sCache[0] );
            return sCache[0];
        }

        DLCache() {}

        DLCache( HashMap<ISOCode, String> data, int rev )
        {
            mLangNames = data;
            mCurRev = rev;
        }

        ISOCode get( String langName )
        {
            ISOCode result = null;
            for ( ISOCode code : mLangNames.keySet() ) {
                if ( langName.equals( mLangNames.get(code) ) ) {
                    result = code;
                    break;
                }
            }
            if ( null == result ) {
                Log.d( TAG, "langName '%s' not in %s", langName, this );
            }
            return result;
        }

        String get( ISOCode code )
        {
            String result = mLangNames.get(code);
            if ( null == result ) {
                Log.d( TAG, "code '%s' not in %s", code, this );
            }
            return result;
        }

        void put( ISOCode code, String langName )
        {
            if ( !langName.equals( mLangNames.get( code ) ) ) {
                mDirty = true;
                mLangNames.put( code, langName );
            }
        }

        // @Override
        // public String toString()
        // {
        //     ArrayList<String> pairs = new ArrayList<>();
        //     for ( ISOCode code : mLangNames.keySet() ) {
        //         pairs.add(String.format("%s<=>%s", code, mLangNames.get(code) ) );
        //     }
        //     return TextUtils.join( ", ", pairs );
        // }

        @Override
        public void close()
        {
            if ( mDirty ) {
                DBUtils.setSerializableFor( mContext, CACHE_KEY_DATA, mLangNames );
                DBUtils.setIntFor( mContext, CACHE_KEY_REV, mCurRev );
                Log.d( TAG, "saveCache(%H) stored %s", this, this );
                mDirty = false;
            }
            unlock();
            synchronized ( sCache ) {
                sCache.notifyAll();
            }
        }

        private void update( Context context )
        {
            if ( mCurRev < BuildConfig.VERSION_CODE ) {
                Resources res = context.getResources();
                String[] entries  = res.getStringArray( R.array.language_names );
                for ( int ii = 0; ii < entries.length; ii += 2 ) {
                    ISOCode isoCode = new ISOCode(entries[ii]);
                    String langName = entries[ii+1];
                    put( isoCode, langName );
                }
                mCurRev = BuildConfig.VERSION_CODE;
                if ( mDirty ) {
                    Log.d( TAG, "updated cache; now %s", this );
                }
            }
        }

        private boolean tryLock( Context context )
        {
            boolean canLock = null == mContext;
            if ( canLock ) {
                mContext = context;
            }
            return canLock;
        }

        private void unlock()
        {
            Assert.assertTrueNR( null != mContext );
            mContext = null;
        }
    }
}
