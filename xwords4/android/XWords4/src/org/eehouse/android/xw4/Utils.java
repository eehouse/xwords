/* -*- compile-command: "cd ../../../../../; ant install"; -*- */

package org.eehouse.android.xw4;

import android.util.Log;
import java.lang.Thread;
import android.widget.Toast;
import android.content.Context;
import android.content.Intent;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileInputStream;
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.util.ArrayList;
import android.content.res.AssetManager;
import android.os.Environment;
import java.io.InputStream;
import android.widget.CheckBox;
import android.app.Activity;
import android.app.Dialog;
import android.app.AlertDialog;
import android.widget.EditText;
import android.widget.TextView;
import android.view.View;
import android.text.format.Time;
import java.util.Formatter;
import android.view.LayoutInflater;
import android.net.Uri;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.*;

public class Utils {
    static final String TAG = "EJAVA";

    static final int DIALOG_ABOUT = 1;
    static final int DIALOG_LAST = DIALOG_ABOUT;

    private static Time s_time = new Time();

    private Utils() {}

    public static void logf( String format ) {
        s_time.setToNow();
        String time = s_time.format("%M:%S");
        long id = Thread.currentThread().getId();
        Log.d( TAG, time + "-" + id + "-" + format );
    } // logf

    public static void logf( String format, Object... args ) {
        Formatter formatter = new Formatter();
        logf( formatter.format( format, args ).toString() );
    } // logf

    public static void notImpl( Context context ) 
    {
        CharSequence text = "Feature coming soon";
        Toast.makeText( context, text, Toast.LENGTH_SHORT).show();
    }

    static Dialog onCreateDialog( Context context, int id )
    {
        Assert.assertTrue( DIALOG_ABOUT == id );
        LayoutInflater factory = LayoutInflater.from( context );
        final View view = factory.inflate( R.layout.about_dlg, null );
        TextView vers = (TextView)view.findViewById( R.id.version_string );
        vers.setText( String.format( context.getString(R.string.about_versf), 
                                     SvnVersion.VERS ) );

        TextView xlator = (TextView)view.findViewById( R.id.about_xlator );
        String str = context.getString( R.string.xlator );
        if ( str.length() > 0 ) {
            xlator.setText( str );
        } else {
            xlator.setVisibility( View.GONE );
        }

        return new AlertDialog.Builder( context )
            .setIcon( R.drawable.icon48x48 )
            .setTitle( R.string.app_name )
            .setView( view )
            .create();
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

    /**
     * Open an existing game, and use its gi and comms addr as the
     * basis for a new one.
     */
    public static void resetGame( Context context, String pathIn, 
                                  String pathOut )
    {
        int gamePtr = XwJNI.initJNI();
        CurGameInfo gi = new CurGameInfo( context );
        CommsAddrRec addr = null;

        loadMakeGame( context, gamePtr, gi, pathIn );
        byte[] dictBytes = Utils.openDict( context, gi.dictName );
        
        if ( XwJNI.game_hasComms( gamePtr ) ) {
            addr = new CommsAddrRec();
            XwJNI.comms_getAddr( gamePtr, addr );
        }
        XwJNI.game_dispose( gamePtr );

        gi.setInProgress( false );
        gi.fixup();

        gamePtr = XwJNI.initJNI();
        XwJNI.game_makeNewGame( gamePtr, gi, JNIUtilsImpl.get(), 
                                CommonPrefs.get(), dictBytes );
        if ( null != addr ) {
            XwJNI.comms_setAddr( gamePtr, addr );
        }
        saveGame( context, gamePtr, gi, pathOut );
        XwJNI.game_dispose( gamePtr );
    }

    public static String[] gamesList( Context context )
    {
        ArrayList<String> al = new ArrayList<String>();
        for ( String file : context.fileList() ) {
            if ( isGame( file ) ){
                al.add( file );
            }
        }
        return al.toArray( new String[al.size()] );
    }

    public static void resetGame( Context context, String pathIn )
    {
        resetGame( context, pathIn, newName( context ) );
    }

    public static void loadMakeGame( Context context, int gamePtr, 
                                     CurGameInfo gi, String path )
    {
        byte[] stream = savedGame( context, path );
        XwJNI.gi_from_stream( gi, stream );
        byte[] dictBytes = Utils.openDict( context, gi.dictName );

        boolean madeGame = XwJNI.game_makeFromStream( gamePtr, stream, 
                                                      JNIUtilsImpl.get(),
                                                      gi, dictBytes, 
                                                      CommonPrefs.get() );
        if ( !madeGame ) {
            XwJNI.game_makeNewGame( gamePtr, gi, JNIUtilsImpl.get(), 
                                    CommonPrefs.get(), dictBytes );
        }
    }

    public static void saveGame( Context context, int gamePtr, 
                                 CurGameInfo gi, String path )
    {
        byte[] stream = XwJNI.game_saveToStream( gamePtr, gi );
        saveGame( context, stream, path );
    }

    public static void saveGame( Context context, int gamePtr, 
                                 CurGameInfo gi )
    {
        saveGame( context, gamePtr, gi, newName( context ) );
    }

    public static void saveGame( Context context, byte[] bytes, String path )
    {
        try {
            FileOutputStream out = context.openFileOutput( path, 
                                                           Context.MODE_PRIVATE );
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

    public static boolean gameDictHere( Context context, String path, 
                                        String[] missingName )
    {
        byte[] stream = savedGame( context, path );
        CurGameInfo gi = new CurGameInfo( context );
        XwJNI.gi_from_stream( gi, stream );
        String dictName = gi.dictName;
        missingName[0] = dictName;

        boolean exists = false;
        for ( String name : dictList( context ) ) {
            if ( name.equals( dictName ) ){
                exists = true;
                break;
            }
        }
        return exists;
    }

    public static boolean gameDictHere( Context context, int indx, 
                                        String[] name )
    {
        String path = Utils.gamesList( context )[indx];
        return gameDictHere( context, path, name );
    }

    public static String newName( Context context ) 
    {
        String name = null;
        Integer num = 0;
        int ii;
        String[] files = context.fileList();

        while ( name == null ) {
            name = "game " + num.toString() + XWConstants.GAME_EXTN;
            for ( ii = 0; ii < files.length; ++ii ) {
                if ( files[ii].equals(name) ) {
                    ++num;
                    name = null;
                }
            }
        }
        return name;
    }

    public static String[] dictList( Context context )
    {
        ArrayList<String> al = new ArrayList<String>();

        try {
            AssetManager am = context.getAssets();
            String[] files = am.list("");
            for ( String file : files ) {
                if ( isDict( file ) ) {
                    al.add( file );
                }
            }
        } catch( java.io.IOException ioe ) {
            Utils.logf( ioe.toString() );
        }

        for ( String file : context.fileList() ) {
            if ( isDict( file ) ) {
                al.add( file );
            }
        }

        return al.toArray( new String[al.size()] );
    }

    public static byte[] openDict( Context context, final String name )
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
            Utils.logf( "%s failed to open; likely not built-in", name );
        }

        // not an asset?  Try storage
        if ( null == bytes ) {
            try {
                FileInputStream fis = context.openFileInput( name );
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

    public static void saveDict( Context context, String name, InputStream in )
    {
        int totalRead = 0;
        try {
            FileOutputStream fos = context.openFileOutput( name,
                                                           Context.MODE_PRIVATE );
            byte[] buf = new byte[1024];
            int nRead;
            while( 0 <= (nRead = in.read( buf, 0, buf.length )) ) {
                fos.write( buf, 0, nRead );
                totalRead += nRead;
            }
            fos.close();
        } catch ( java.io.FileNotFoundException fnf ) {
            Utils.logf( "saveDict: FileNotFoundException: %s", fnf.toString() );
        } catch ( java.io.IOException ioe ) {
            Utils.logf( "saveDict: IOException: %s", ioe.toString() );
        }
    } 

    public static Intent mkDownloadActivity( Context context )
    {
        Uri uri = Uri.parse( context.getString( R.string.dict_url ));
        Intent intent = new Intent( Intent.ACTION_VIEW, uri );
        intent.setFlags( Intent.FLAG_ACTIVITY_NEW_TASK );
        return intent;
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

    public static void setText( Activity activity, int id, String value )
    {
        EditText editText = (EditText)activity.findViewById( id );
        if ( null != editText ) {
            editText.setText( value, TextView.BufferType.EDITABLE   );
        }
    }

    public static void setInt( Dialog dialog, int id, int value )
    {
        String str = Integer.toString(value);
        setText( dialog, id, str );
    }

    public static void setInt( Activity activity, int id, int value )
    {
        String str = Integer.toString(value);
        setText( activity, id, str );
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

    public static String getText( Activity activity, int id )
    {
        EditText editText = (EditText)activity.findViewById( id );
        return editText.getText().toString();
    }

    public static int getInt( Dialog dialog, int id )
    {
        String str = getText( dialog, id );
        try {
            return Integer.parseInt( str );
        } catch ( NumberFormatException nfe ) {
            return 0;
        }
    }

    public static int getInt( Activity activity, int id )
    {
        String str = getText( activity, id );
        try {
            return Integer.parseInt( str );
        } catch ( NumberFormatException nfe ) {
            return 0;
        }
    }

    private static boolean isGame( String file )
    {
        return file.endsWith( XWConstants.GAME_EXTN );
    }

    private static boolean isDict( String file )
    {
        return file.endsWith( XWConstants.DICT_EXTN );
    }

}
