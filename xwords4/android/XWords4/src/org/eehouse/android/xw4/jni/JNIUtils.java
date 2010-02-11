/* -*- compile-command: "cd ../../../../../../; ant install"; -*- */

package org.eehouse.android.xw4.jni;

import android.graphics.drawable.BitmapDrawable;

public interface JNIUtils {

    // Stuff I can't do in C....
    BitmapDrawable makeBitmap( int width, int height, boolean[] colors );
    String[] splitFaces( byte[] chars );
}