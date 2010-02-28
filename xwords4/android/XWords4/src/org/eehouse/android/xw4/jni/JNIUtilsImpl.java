/* -*- compile-command: "cd ../../../../../../; ant install"; -*- */

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

    public BitmapDrawable makeBitmap( int width, int height, boolean[] colors )
    {
        Bitmap bitmap = Bitmap.createBitmap( width, height, 
                                             Bitmap.Config.ARGB_8888 );

        int indx = 0;
        for ( int yy = 0; yy < height; ++yy ) {
            for ( int xx = 0; xx < width; ++xx ) {
                boolean pixelSet = colors[indx++];
                bitmap.setPixel( xx, yy, pixelSet? 0xFF000000 : 0x00 );
            }
        }

        // Doesn't compile if pass getResources().  Maybe the
        // "deprecated" API is really the only one?
        return new BitmapDrawable( /*getResources(), */bitmap );
    }

    /** Working around lack of utf8 support on the JNI side: given a
     * utf-8 string with embedded small number vals starting with 0,
     * convert into individual strings.  The 0 is the problem: it's
     * not valid utf8.  So turn it and the other nums into strings and
     * catch them on the other side.
     */
    public String[] splitFaces( byte[] chars )
    {
        ArrayList<String> al = new ArrayList<String>();
        ByteArrayInputStream bais = new ByteArrayInputStream( chars );
        InputStreamReader isr = new InputStreamReader( bais );

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