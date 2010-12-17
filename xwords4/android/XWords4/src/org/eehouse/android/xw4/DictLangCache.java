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
import java.util.ArrayList;
import java.util.HashMap;

import org.eehouse.android.xw4.jni.JNIUtilsImpl;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.jni.DictInfo;

public class DictLangCache {
    private static final HashMap<String,DictInfo> s_nameToLang = 
        new HashMap<String,DictInfo>();
    private static String[] s_langNames;

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
        String[] namesArray = getNamesArray( context );
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
            if ( code == getLangCode( context, dict ) ) {
                ++count;
            }
        }
        return count;
    }

    private static String[] getHaveLang( Context context, int code,
                                         boolean withCounts )
    {
        ArrayList<String> al = new ArrayList<String>();
        String[] dicts = GameUtils.dictList( context );
        String fmt = "%s (%d)"; // must match stripCount below
        for ( String dict : dicts ) {
            DictInfo info = getInfo( context, dict );
            if ( code == info.langCode ) {
                if ( withCounts ) {
                    dict = String.format( fmt, dict, info.wordCount );
                }
                al.add( dict );
            }
        }
        return al.toArray( new String[al.size()] );
    }

    public static String[] getHaveLang( Context context, int code )
    {
        return getHaveLang( context, code, false );
    }

    public static String[] getHaveLangCounts( Context context, int code )
    {
        return getHaveLang( context, code, true );
    }

    public static String stripCount( String nameWithCount )
    {
        int indx = nameWithCount.lastIndexOf( " (" );
        return nameWithCount.substring( 0, indx );
    }

    public static int getLangCode( Context context, String name )
    {
        return getInfo( context, name ).langCode;
    }

    public static String getLangName( Context context, String name )
    {
        int code = getLangCode( context, name );
        return getLangName( context, code );
    }

    public static void inval( String name )
    {
        s_nameToLang.remove( name );
    }

    private static String[] getNamesArray( Context context )
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
            s_nameToLang.put( name, info );
        }
        return info;
    }

}