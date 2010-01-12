/* -*- compile-command: "cd ../../../../../; ant reinstall"; -*- */

package org.eehouse.android.xw4;

import android.util.Log;
import java.lang.Thread;
import java.text.MessageFormat;
import android.widget.Toast;
import android.content.Context;

public class Utils {
    static final String TAG = "EJAVA";

    private Utils() {}

    public static void logf( String format ) {
        long id = Thread.currentThread().getId();
        Log.d( TAG, id + ": " + format );
    } // logf

    public static void logf( String format, Object[] args ) {
        MessageFormat mfmt = new MessageFormat( format );
        logf( mfmt.format( args ) );
    } // logf

    public static void notImpl( Context context ) 
    {
        CharSequence text = "Feature coming soon";
        Toast.makeText( context, text, Toast.LENGTH_SHORT).show();
    }

    public static void about( Context context ) 
    {
        CharSequence text = "Version: pre-alpha; svn rev: " + SvnVersion.VERS;
        Toast.makeText( context, text, Toast.LENGTH_LONG ).show();
    }

}
