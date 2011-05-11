/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
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
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Arrays;
import java.util.Comparator;

import org.eehouse.android.xw4.jni.JNIUtilsImpl;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.jni.DictInfo;
import org.eehouse.android.xw4.jni.CommonPrefs;

public class DictLangCache {
    private static final HashMap<String,DictInfo> s_nameToLang = 
        new HashMap<String,DictInfo>();
    private static String[] s_langNames;

    private static int m_adaptedLang = -1;
    private static ArrayAdapter<String> m_langsAdapter;
    private static ArrayAdapter<String> m_dictsAdapter;
    private static String s_last;
    private static Handler s_handler;
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

    public static String annotatedDictName( Context context, String name )
    {
        DictInfo info = getInfo( context, name );
        int wordCount = info.wordCount;
            
        String langName = getLangName( context, name );
        String result;
        if ( 0 == wordCount ) {
            result = String.format( "%s (%s)", name, langName );
        } else {
            result = String.format( "%s (%s/%d)", name, langName, wordCount );
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
        String[] namesArray = getLangNames( context );
        if ( code < 0 || code >= namesArray.length ) {
            code = 0;
        }
        return namesArray[code];
    }

    // This populates the cache and will take significant time if it's
    // mostly empty and there are a lot of dicts.
    public static int getLangCount( Context context, int code )
    {
        int count = 0;
        String[] dicts = GameUtils.dictList( context );
        for ( String dict : dicts ) {
            if ( code == getDictLangCode( context, dict ) ) {
                ++count;
            }
        }
        return count;
    }

    private static DictInfo[] getInfosHaveLang( Context context, int code )
    {
        ArrayList<DictInfo> al = new ArrayList<DictInfo>();
        String[] dicts = GameUtils.dictList( context );
        for ( String dict : dicts ) {
            DictInfo info = getInfo( context, dict );
            if ( code == info.langCode ) {
                al.add( info );
            }
        }
        DictInfo[] result = al.toArray( new DictInfo[al.size()] );
        return result;
    }

    private static String[] getHaveLang( Context context, int code,
                                         Comparator<DictInfo> comp,
                                         boolean withCounts )
    {
        DictInfo[] infos = getInfosHaveLang( context, code );

        if ( null != comp ) {
            Arrays.sort( infos, comp );
        }

        ArrayList<String> al = new ArrayList<String>();
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

    public static int getDictLangCode( Context context, String dict )
    {
        return getInfo( context, dict ).langCode;
    }

    public static int getLangLangCode( Context context, String lang )
    {
        int code = 0;
        String[] namesArray = getLangNames( context );
        for ( int ii = 0; ii < namesArray.length; ++ii ) {
            if ( namesArray[ii].equals( lang ) ) {
                code = ii;
                break;
            }
        }
        return code;
    }

    public static String getLangName( Context context, String name )
    {
        int code = getDictLangCode( context, name );
        return getLangName( context, code );
    }

    public static void inval( final Context context, String name, 
                              boolean added )
    {
        name = GameUtils.removeDictExtn( name );
        s_nameToLang.remove( name );
        if ( added ) {
            getInfo( context, name );
        }

        if ( null != s_handler ) {
            s_handler.post( new Runnable() {
                    public void run() {
                        if ( null != m_dictsAdapter ) {
                            rebuildAdapter( m_dictsAdapter, 
                                            DictLangCache.
                                            getHaveLang( context, 
                                                         m_adaptedLang ) );
                        }
                        if ( null != m_langsAdapter ) {
                            rebuildAdapter( m_langsAdapter, 
                                            DictLangCache.listLangs( context ) );
                        }
                    }
                } );
        }
    }

    public static String[] listLangs( Context context )
    {
        return listLangs( context, GameUtils.dictList( context ) );
    }

    public static String[] listLangs( Context context, final String[] names )
    {
        HashSet<String> langs = new HashSet<String>();
        for ( String name:names ) {
            langs.add( getLangName( context, name ) );
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

    public static ArrayAdapter<String> getLangsAdapter( Context context )
    {
        if ( null == m_langsAdapter ) {
            m_langsAdapter = 
                new ArrayAdapter<String>( context,
                                          android.R.layout.simple_spinner_item );
            rebuildAdapter( m_langsAdapter, listLangs( context ) );
        }
        return m_langsAdapter;
    }

    public static ArrayAdapter<String> getDictsAdapter( Context context, 
                                                        int lang )
    {
        if ( lang != m_adaptedLang ) {
            m_dictsAdapter = 
                new ArrayAdapter<String>( context,
                                          android.R.layout.simple_spinner_item );
            rebuildAdapter( m_dictsAdapter, getHaveLang( context, lang ) );
            m_adaptedLang = lang;
        }
        return m_dictsAdapter;
    }

    public static String[] getLangNames( Context context )
    {
        if ( null == s_langNames ) {
            Resources res = context.getResources();
            s_langNames = res.getStringArray( R.array.language_names );
        }
        return s_langNames;
    }

    private static DictInfo getInfo( Context context, String name )
    {
        DictInfo info;
        if ( s_nameToLang.containsKey( name ) ) {
            info = s_nameToLang.get( name );
        } else {
            byte[] dict = GameUtils.openDict( context, name );
            info = new DictInfo();
            XwJNI.dict_getInfo( dict, JNIUtilsImpl.get(), info );
            info.name = name;
            s_nameToLang.put( name, info );
        }
        return info;
    }

}