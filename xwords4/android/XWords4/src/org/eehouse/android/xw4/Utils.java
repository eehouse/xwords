/* -*- compile-command: "cd ../../../../../; ant reinstall"; -*- */

package org.eehouse.android.xw4;

import android.util.Log;
import java.lang.Thread;
import java.text.MessageFormat;
import android.widget.Toast;
import android.content.Context;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileInputStream;
import java.util.ArrayList;
import android.content.res.AssetManager;
import android.os.Environment;
import java.io.InputStream;
import android.widget.CheckBox;
import android.app.Activity;
import android.app.Dialog;
import android.widget.EditText;
import android.widget.TextView;

import org.eehouse.android.xw4.jni.*;

public class Utils {
    static final String TAG = "EJAVA";
    private static CommonPrefs m_cp;

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

    public static void saveGame( Context context, byte[] bytes )
    {
        saveGame( context, bytes, newName( context ) );
    }
    
    public static String newName( Context context ) 
    {
        String name = null;
        Integer num = 0;
        int ii;
        String[] files = context.fileList();

        while ( name == null ) {
            name = "game " + num.toString();
            for ( ii = 0; ii < files.length; ++ii ) {
                if ( files[ii].equals(name) ) {
                    ++num;
                    name = null;
                }
            }
        }
        return name;
    }

    private static void tryFile( ArrayList<String> al, String name )
    {
        if ( name.endsWith( ".xwd" ) ) {
            al.add( name );
        }
    }

    private static void tryDir( ArrayList<String> al, File dir )
    {
        for ( File file: dir.listFiles() ) {
            tryFile( al, file.getAbsolutePath() );
        }
    }

    public static String[] listDicts( Context context )
    {
	return listDicts( context, Integer.MAX_VALUE );
    }

    public static String[] listDicts( Context context, int enough )
    {
        ArrayList<String> al = new ArrayList<String>();

        try {
            AssetManager am = context.getAssets();
            String[] files = am.list("");
            for ( String file : files ) {
                tryFile( al, file );
            }
        } catch( java.io.IOException ioe ) {
            Utils.logf( ioe.toString() );
        }

        File files[] = Environment.getExternalStorageDirectory().listFiles();
        for ( File file : files ) {
	    if ( al.size() >= enough ) {
		break;
	    }
            if ( file.isDirectory() ) { // go down one level
                tryDir( al, file );
            } else {
                tryFile( al, file.getAbsolutePath() );
            }
        }

        return al.toArray( new String[al.size()] );
    }

    public static byte[] openDict( Context context, String name )
    {
        byte[] bytes = null;
        InputStream dict = null;
        AssetManager am = context.getAssets();
        try {
            dict = am.open( name, 
                            android.content.res.AssetManager.ACCESS_RANDOM );

            int len = dict.available();
            bytes = new byte[len];
            int nRead = dict.read( bytes, 0, len );
            if ( nRead != len ) {
                Utils.logf( "**** warning ****; read only " + nRead + " of " 
                            + len + " bytes." );
            }
        } catch ( java.io.IOException ee ){
            Utils.logf( "failed to open" );
        }

        // not an asset?  Try storage
        if ( null == bytes ) {
            try {
                FileInputStream fis = new FileInputStream( new File(name) );
                int len = fis.available();
                bytes = new byte[len];
                fis.read( bytes, 0, len );
                fis.close();
            } catch ( java.io.FileNotFoundException fnf ) {
                Utils.logf( fnf.toString() );
            } catch ( java.io.IOException ioe ) {
                Utils.logf( ioe.toString() );
            }
        }
        

        return bytes;
    }

    public static void setChecked( Activity activity, int id, boolean value )
    {
        CheckBox cbx = (CheckBox)activity.findViewById( id );
        cbx.setChecked( value );
    }

    public static void setChecked( Dialog dialog, int id, boolean value )
    {
        CheckBox cbx = (CheckBox)dialog.findViewById( id );
        cbx.setChecked( value );
    }

    public static void setText( Dialog dialog, int id, String value )
    {
        EditText editText = (EditText)dialog.findViewById( id );
        if ( null != editText ) {
            editText.setText( value, TextView.BufferType.EDITABLE   );
        }
    }

    public static void setInt( Dialog dialog, int id, int value )
    {
        String str = Integer.toString(value);
        setText( dialog, id, str );
    }

    public static boolean getChecked( Activity activity, int id )
    {
        CheckBox cbx = (CheckBox)activity.findViewById( id );
        return cbx.isChecked();
    }

    public static boolean getChecked( Dialog dialog, int id )
    {
        CheckBox cbx = (CheckBox)dialog.findViewById( id );
        return cbx.isChecked();
    }

    public static String getText( Dialog dialog, int id )
    {
        EditText editText = (EditText)dialog.findViewById( id );
        return editText.getText().toString();
    }

    public static int getInt( Dialog dialog, int id )
    {
        String str = getText( dialog, id );
        return Integer.parseInt( str );
    }

    public static CommonPrefs getCP()
    {
        if ( null == m_cp ) {
            m_cp = new CommonPrefs();
        }
        return m_cp;
    }

    public static void setCP( CommonPrefs cp )
    {
        if ( null == m_cp ) {
            m_cp = new CommonPrefs( cp );
        } else {
            m_cp.copyFrom( cp );
        }
    }

}
