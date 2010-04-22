/* -*- compile-command: "cd ../../../../../../; ant install"; -*- */
/*
 * Copyright 2009-2010 by Eric House (xwords@eehouse.org).  All
 * rights reserved.
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

package org.eehouse.android.xw4.jni;

import android.graphics.drawable.BitmapDrawable;
import android.graphics.Bitmap;
import java.util.ArrayList;
import java.io.ByteArrayInputStream;
import java.io.InputStreamReader;

import org.eehouse.android.xw4.*;

public class JNIUtilsImpl implements JNIUtils {

    private static JNIUtils s_impl = null;

    private JNIUtilsImpl(){}

    public static JNIUtils get()
    {
        if ( null == s_impl ) {
            s_impl = new JNIUtilsImpl();
        }
        return s_impl;
    }

    /** Working around lack of utf8 support on the JNI side: given a
     * utf-8 string with embedded small number vals starting with 0,
     * convert into individual strings.  The 0 is the problem: it's
     * not valid utf8.  So turn it and the other nums into strings and
     * catch them on the other side.
     */
    public String[] splitFaces( byte[] chars, boolean isUTF8 )
    {
        ArrayList<String> al = new ArrayList<String>();
        ByteArrayInputStream bais = new ByteArrayInputStream( chars );
        InputStreamReader isr;
        try {
            isr = new InputStreamReader( bais, isUTF8? "UTF8" : "ISO8859_1" );
        } catch( java.io.UnsupportedEncodingException uee ) {
            Utils.logf( "splitFaces: %s", uee.toString() );
            isr = new InputStreamReader( bais );
        }
        
        int[] codePoints = new int[1];

        for ( ; ; ) {
            int chr = -1;
            try {
                chr = isr.read();
            } catch ( java.io.IOException ioe ) {
                Utils.logf( ioe.toString() );
            }
            if ( -1 == chr ) {
                break;
            } else {
                String letter;
                if ( chr < 32 ) {
                    letter = String.format( "%d", chr );
                } else {
                    codePoints[0] = chr;
                    letter = new String( codePoints, 0, 1 );
                }
                al.add( letter );
            }
        }
        
        String[] result = al.toArray( new String[al.size()] );
        return result;
    }
}