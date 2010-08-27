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
import java.util.HashMap;

import org.eehouse.android.xw4.jni.JNIUtilsImpl;
import org.eehouse.android.xw4.jni.XwJNI;

public class DictLangCache {
    private static HashMap<String,Integer> s_nameToLang;

    public static int getLangCode( Context context, String name )
    {
        if ( null == s_nameToLang ) {
            s_nameToLang = new HashMap<String,Integer>();
        }
        
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
}