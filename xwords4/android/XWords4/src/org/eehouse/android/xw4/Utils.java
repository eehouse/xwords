
package org.eehouse.android.xw4;

import android.util.Log;
import java.text.MessageFormat;

public class Utils {
    static final String TAG = "EJAVA";

    private Utils() {}

    public static void logf( String format ) {
        Log.d( TAG, format );
    } // logf

    public static void logf( String format, Object[] args ) {
        MessageFormat mfmt = new MessageFormat( format );
        logf( mfmt.format( args ) );
    } // logf
}
