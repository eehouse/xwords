/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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

import android.content.Context;

import org.eehouse.android.xw4.Assert;
import org.eehouse.android.xw4.DBUtils;
import org.eehouse.android.xw4.XWApp;
import org.eehouse.android.xw4.Log;
import org.eehouse.android.xw4.Utils;

import java.io.ByteArrayInputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;

public class JNIUtilsImpl implements JNIUtils {
    private static final String TAG = JNIUtilsImpl.class.getSimpleName();

    private static final char SYNONYM_DELIM = ' ';

    private static JNIUtilsImpl s_impl = null;
    private Context m_context;

    private JNIUtilsImpl(Context context) { m_context = context; }

    public static synchronized JNIUtils get()
    {
        if ( null == s_impl ) {
            s_impl = new JNIUtilsImpl( XWApp.getContext() );
        }
        return s_impl;
    }

    /** Working around lack of utf8 support on the JNI side: given a
     * utf-8 string with embedded small number vals starting with 0,
     * convert into individual strings.  The 0 is the problem: it's
     * not valid utf8.  So turn it and the other nums into strings and
     * catch them on the other side.
     *
     * Changes for "synonyms" (A and a being the same tile): return an
     * array of Strings for each face.  Each face is
     * <letter>[<delim><letter]*, so for each loop until the delim
     * isn't found.
     */
    @Override
    public String[][] splitFaces( byte[] chars, boolean isUTF8 )
    {
        ArrayList<String[]> faces = new ArrayList<String[]>();
        ByteArrayInputStream bais = new ByteArrayInputStream( chars );
        InputStreamReader isr;
        try {
            isr = new InputStreamReader( bais, isUTF8? "UTF8" : "ISO8859_1" );
        } catch( java.io.UnsupportedEncodingException uee ) {
            Log.i( TAG, "splitFaces: %s", uee.toString() );
            isr = new InputStreamReader( bais );
        }

        int[] codePoints = new int[1];

        // "A aB bC c"
        boolean lastWasDelim = false;
        ArrayList<String> face = null;
        for ( ; ; ) {
            int chr = -1;
            try {
                chr = isr.read();
            } catch ( java.io.IOException ioe ) {
                Log.w( TAG, ioe.toString() );
            }
            if ( -1 == chr ) {
                addFace( faces, face );
                break;
            } else if ( SYNONYM_DELIM == chr ) {
                Assert.assertNotNull( face );
                lastWasDelim = true;
                continue;
            } else {
                String letter;
                if ( chr < 32 ) {
                    letter = String.format( "%d", chr );
                } else {
                    codePoints[0] = chr;
                    letter = new String( codePoints, 0, 1 );
                }
                // Ok, we have a letter.  Is it part of an existing
                // one or the start of a new?  If the latter, insert
                // what we have before starting over.
                if ( null == face ) { // start of a new, clearly
                    // do nothing
                } else {
                    Assert.assertTrue( 0 < face.size() );
                    if ( !lastWasDelim ) {
                        addFace( faces, face );
                        face = null;
                    }
                }
                lastWasDelim = false;
                if ( null == face ) {
                    face = new ArrayList<String>();
                }
                face.add( letter );
            }
        }

        String[][] result = faces.toArray( new String[faces.size()][] );
        return result;
    }

    private void addFace( ArrayList<String[]> faces, ArrayList<String> face )
    {
        faces.add( face.toArray( new String[face.size()] ) );
    }

    @Override
    public String getMD5SumFor( byte[] bytes )
    {
        return Utils.getMD5SumFor( bytes );
    }

    @Override
    public String getMD5SumFor( String dictName, byte[] bytes )
    {
        String result = null;
        if ( null == bytes ) {
            result = DBUtils.dictsGetMD5Sum( m_context, dictName );
        } else {
            result = getMD5SumFor( bytes );
            // Is this needed?  Caller might be doing it anyway.
            DBUtils.dictsSetMD5Sum( m_context, dictName, result );
        }
        return result;
    }
}
