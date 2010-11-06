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
import java.util.HashMap;

import org.eehouse.android.xw4.jni.JNIUtilsImpl;
import org.eehouse.android.xw4.jni.XwJNI;

public class DictLangCache {
    private static final HashMap<String,Integer> s_nameToLang = 
        new HashMap<String,Integer>();
    private static String[] s_langNames;

    public static String annotatedDictName( Context context,
                                            String name )
    {
        return name + " (" + getLangName( context, name ) + ")";
    }

    public static String annotatedDictName( Context context, String name,
                                            int lang )
    {
        return name + " (" + getLangName( context, lang ) + ")";
    }

    public static String getLangName( Context context, int code )
    {
        return getNamesArray(context)[code];
    }

    public static int getLangCode( Context context, String name )
    {
        int code;
        if ( s_nameToLang.containsKey( name ) ) {
            code = s_nameToLang.get( name );
        } else {
            byte[] dict = GameUtils.openDict( context, name );
            code = XwJNI.dict_getLanguageCode( dict, JNIUtilsImpl.get() );
            s_nameToLang.put( name, new Integer(code) );
        }
        return code;
    }

    public static String getLangName( Context context, String name )
    {
        int code = getLangCode( context, name );
        return getNamesArray(context)[code];
    }

    private static String[] getNamesArray( Context context )
    {
        if ( null == s_langNames ) {
            Resources res = context.getResources();
            s_langNames = res.getStringArray( R.array.language_names );
        }
        return s_langNames;
    }

}