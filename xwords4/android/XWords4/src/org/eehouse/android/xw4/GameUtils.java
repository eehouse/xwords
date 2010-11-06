/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
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

package org.eehouse.android.xw4;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import android.net.Uri;
import java.util.ArrayList;
import android.content.res.AssetManager;

import org.eehouse.android.xw4.jni.*;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;

public class GameUtils {

    private static Object s_syncObj = new Object();

    public static byte[] savedGame( Context context, String path )
    {
        byte[] stream = null;
        try {
            synchronized( s_syncObj ) {
                FileInputStream in = context.openFileInput( path );
                int len = in.available();
                stream = new byte[len];
                in.read( stream, 0, len );
                in.close();
            }
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

        // loadMakeGame, if makinga new game, will add comms as long
        // as DeviceRole.SERVER_STANDALONE != gi.serverRole
        loadMakeGame( context, gamePtr, gi, pathIn );
        byte[] dictBytes = GameUtils.openDict( context, gi.dictName );
        
        if ( XwJNI.game_hasComms( gamePtr ) ) {
            addr = new CommsAddrRec( context );
            XwJNI.comms_getAddr( gamePtr, addr );
            if ( CommsAddrRec.CommsConnType.COMMS_CONN_NONE == addr.conType ) {
                String relayName = CommonPrefs.getDefaultRelayHost( context );
                int relayPort = CommonPrefs.getDefaultRelayPort( context );
                XwJNI.comms_getInitialAddr( addr, relayName, relayPort );
            }
        }
        XwJNI.game_dispose( gamePtr );

        gi.setInProgress( false );
        gi.fixup();

        gamePtr = XwJNI.initJNI();
        XwJNI.game_makeNewGame( gamePtr, gi, JNIUtilsImpl.get(), 
                                CommonPrefs.get( context ), dictBytes, 
                                gi.dictName );
        if ( null != addr ) {
            XwJNI.comms_setAddr( gamePtr, addr );
        }
        saveGame( context, gamePtr, gi, pathOut );

        GameSummary summary = new GameSummary();
        XwJNI.game_summarize( gamePtr, gi.nPlayers, summary );
        DBUtils.saveSummary( context, pathOut, summary );

        XwJNI.game_dispose( gamePtr );
    } // resetGame

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

    public static String resetGame( Context context, String pathIn )
    {
        String newName = newName( context );
        resetGame( context, pathIn, newName );
        return newName;
    }

    public static void deleteGame( Context context, String path )
    {
        // does this need to be synchronized?
        context.deleteFile( path );
        DBUtils.saveSummary( context, path, null );
    }

    public static void loadMakeGame( Context context, int gamePtr, 
                                     CurGameInfo gi, String path )
    {
        byte[] stream = savedGame( context, path );
        XwJNI.gi_from_stream( gi, stream );
        byte[] dictBytes = GameUtils.openDict( context, gi.dictName );

        boolean madeGame = XwJNI.game_makeFromStream( gamePtr, stream, 
                                                      JNIUtilsImpl.get(), gi, 
                                                      dictBytes, gi.dictName,
                                                      CommonPrefs.get(context));
        if ( !madeGame ) {
            XwJNI.game_makeNewGame( gamePtr, gi, JNIUtilsImpl.get(), 
                                    CommonPrefs.get(context), dictBytes, 
                                    gi.dictName );
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
            synchronized( s_syncObj ) {
                FileOutputStream out =
                    context.openFileOutput( path, Context.MODE_PRIVATE );
                out.write( bytes );
                out.close();
            }
        } catch ( java.io.IOException ex ) {
            Utils.logf( "got IOException: " + ex.toString() );
        }
    }

    public static String saveGame( Context context, byte[] bytes )
    {
        String name = newName( context );
        saveGame( context, bytes, name );
        return name;
    }

    public static boolean gameDictHere( Context context, String path, 
                                        String[] missingName, 
                                        int[] missingLang )
    {
        byte[] stream = savedGame( context, path );
        CurGameInfo gi = new CurGameInfo( context );
        XwJNI.gi_from_stream( gi, stream );
        String dictName = removeExtn( gi.dictName );
        missingName[0] = dictName;
        missingLang[0] = gi.dictLang;

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
                                        String[] name, int[] lang )
    {
        String path = GameUtils.gamesList( context )[indx];
        return gameDictHere( context, path, name, lang );
    }

    public static String newName( Context context ) 
    {
        String name = null;
        Integer num = 1;
        int ii;
        String[] files = context.fileList();
        String fmt = context.getString( R.string.gamef );

        while ( name == null ) {
            name = String.format( fmt + XWConstants.GAME_EXTN, num );
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

        for ( String file : getAssets( context ) ) {
            if ( isDict( file ) ) {
                al.add( removeExtn( file ) );
            }
        }

        for ( String file : context.fileList() ) {
            if ( isDict( file ) ) {
                al.add( removeExtn( file ) );
            }
        }

        return al.toArray( new String[al.size()] );
    }

    public static boolean dictExists( Context context, String name )
    {
        boolean exists = dictIsBuiltin( context, name );
        if ( !exists ) {
            name = addDictExtn( name );
            try {
                FileInputStream fis = context.openFileInput( name );
                fis.close();
                exists = true;
            } catch ( java.io.FileNotFoundException fnf ) {
            } catch ( java.io.IOException ioe ) {
            }
        }
        return exists;
    }

    public static boolean dictIsBuiltin( Context context, String name )
    {
        boolean builtin = false;
        name = addDictExtn( name );

        for ( String file : getAssets( context ) ) {
            if ( file.equals( name ) ) {
                builtin = true;
                break;
            }
        }

        return builtin;
    }

    public static void deleteDict( Context context, String name )
    {
        context.deleteFile( addDictExtn( name ) );
    }

    public static byte[] openDict( Context context, String name )
    {
        byte[] bytes = null;
        InputStream dict = null;

        name = addDictExtn( name );

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

    private static boolean isGame( String file )
    {
        return file.endsWith( XWConstants.GAME_EXTN );
    }

    private static boolean isDict( String file )
    {
        return file.endsWith( XWConstants.DICT_EXTN );
    }

    public static String gameName( Context context, String path )
    {
        return path.substring( 0, path.lastIndexOf( XWConstants.GAME_EXTN ) );
    }

    public static void launchGame( Activity activity, String path )
    {
        File file = new File( path );
        Uri uri = Uri.fromFile( file );
        Intent intent = new Intent( Intent.ACTION_EDIT, uri,
                                    activity, BoardActivity.class );
        activity.startActivity( intent );
        activity.finish();
    }

    public static void applyChanges( Context context, CurGameInfo gi, 
                                     CommsAddrRec car, String path, 
                                     boolean forceNew )
    {
        // This should be a separate function, commitChanges() or
        // somesuch.  But: do we have a way to save changes to a gi
        // that don't reset the game, e.g. player name for standalone
        // games?
        byte[] dictBytes = GameUtils.openDict( context, gi.dictName );
        int gamePtr = XwJNI.initJNI();
        boolean madeGame = false;
        CommonPrefs cp = CommonPrefs.get( context );

        if ( !forceNew ) {
            byte[] stream = GameUtils.savedGame( context, path );
            // Will fail if there's nothing in the stream but a gi.
            madeGame = XwJNI.game_makeFromStream( gamePtr, stream, 
                                                  JNIUtilsImpl.get(),
                                                  new CurGameInfo(context), 
                                                  dictBytes, gi.dictName, cp );
        }

        if ( forceNew || !madeGame ) {
            gi.setInProgress( false );
            gi.fixup();
            XwJNI.game_makeNewGame( gamePtr, gi, JNIUtilsImpl.get(), 
                                    cp, dictBytes, gi.dictName );
        }

        if ( null != car ) {
            XwJNI.comms_setAddr( gamePtr, car );
        }

        GameUtils.saveGame( context, gamePtr, gi, path );

        GameSummary summary = new GameSummary();
        XwJNI.game_summarize( gamePtr, gi.nPlayers, summary );
        DBUtils.saveSummary( context, path, summary );

        XwJNI.game_dispose( gamePtr );
    }

    public static void doConfig( Activity activity, String path, Class clazz )
    {
        Uri uri = Uri.fromFile( new File(path) );
        Intent intent = new Intent( Intent.ACTION_EDIT, uri, activity, clazz );
        activity.startActivity( intent );
    }

    private static String removeExtn( String str )
    {
        if ( str.endsWith( XWConstants.DICT_EXTN ) ) {
            int indx = str.lastIndexOf( XWConstants.DICT_EXTN );
            str = str.substring( 0, indx );
        }
        return str;
    }

    private static String addDictExtn( String str ) 
    {
        if ( ! str.endsWith( XWConstants.DICT_EXTN ) ) {
            str += XWConstants.DICT_EXTN;
        }
        return str;
    }

    private static String[] getAssets( Context context )
    {
        try {
            AssetManager am = context.getAssets();
            return am.list("");
        } catch( java.io.IOException ioe ) {
            Utils.logf( ioe.toString() );
            return new String[0];
        }
    }
}
