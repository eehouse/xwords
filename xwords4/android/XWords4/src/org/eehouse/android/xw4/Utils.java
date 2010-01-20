/* -*- compile-command: "cd ../../../../../; ant reinstall"; -*- */

package org.eehouse.android.xw4;

import android.util.Log;
import java.lang.Thread;
import java.text.MessageFormat;
import android.widget.Toast;
import android.content.Context;
import java.io.FileOutputStream;
import java.io.FileInputStream;

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

    public static byte[] savedGame( Context context, String path )
    {
        byte[] stream = null;
        try {
            FileInputStream in = context.openFileInput( path );
            int len = in.available();
            Utils.logf( "savedGame: got " + len + " bytes." );
            stream = new byte[len];
            in.read( stream, 0, len );
            in.close();
        } catch ( java.io.FileNotFoundException fnf ) {
            Utils.logf( fnf.toString() );
            stream = null;
        } catch ( java.io.IOException io ) {
            Utils.logf( io.toString() );
            stream = null;
        }
        return stream;
    } // savedGame


    public static void saveGame( Context context, byte[] bytes, String path )
    {
        try {
            FileOutputStream out = context.openFileOutput( path, Context.MODE_PRIVATE );
            out.write( bytes );
            out.close();
        } catch ( java.io.IOException ex ) {
            Utils.logf( "got IOException: " + ex.toString() );
        }
    }


}
