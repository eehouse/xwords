/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2022 by Eric House (xwords@eehouse.org).  All rights reserved.
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

import android.content.ContentResolver;
import android.content.Context;
import android.net.Uri;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.Serializable;
import java.util.ArrayList;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

public class ZipUtils {
    private static final String TAG = ZipUtils.class.getSimpleName();

    public static enum SaveWhat { // COLORS,
                                  SETTINGS,
                                  GAMES,
                                  ;

                                  String entryName() { return toString(); }
    };

    static String getMimeType() {
        return "application/x-zip";
        // return "application/octet-stream";
    }

    private interface EntryIter {
        boolean withEntry( ZipInputStream zis, SaveWhat what ) throws FileNotFoundException, IOException;
    }

    public static List<SaveWhat> getHasWhats( Context context, Uri uri )
    {
        final List<SaveWhat> result = new ArrayList<>();
        try {
            iterate( context, uri, new EntryIter() {
                    @Override
                    public boolean withEntry( ZipInputStream zis, SaveWhat what ) {
                        result.add( what );
                        return true;
                    }
                } );
        } catch ( IOException ioe ) {
            Log.ex( TAG, ioe );
        }
        Log.d( TAG, "getHasWhats() => %s", result );
        return result;
    }
    
    public static boolean load( Context context, Uri uri,
                                final List<SaveWhat> whats )
    {
        Log.d( TAG, "load(%s)", whats );
        boolean result = false;
        try {
            result = iterate( context, uri, new EntryIter() {
                @Override
                public boolean withEntry( ZipInputStream zis, SaveWhat what )
                    throws FileNotFoundException, IOException {
                    boolean success = false;
                    if ( whats.contains( what ) ) {
                        switch ( what ) {
                        case SETTINGS:
                            success = loadSettings( context, zis );
                            break;
                        case GAMES:
                            success = loadGames( context, zis );
                            break;
                        default:
                            Assert.failDbg();
                            break;
                        }
                    }
                    return success;
                }
            } );
        } catch ( Exception ex ) {
            Log.ex( TAG, ex );
            result = false;
        }
        
        return result;
    }

    private static boolean iterate( Context context, Uri uri, EntryIter iter )
        throws IOException, FileNotFoundException
    {
        boolean success = true;
        try ( InputStream is = context
              .getContentResolver().openInputStream( uri ) ) {
            ZipInputStream zis = new ZipInputStream( is );
            while ( success ) {
                ZipEntry ze = zis.getNextEntry();
                if ( null == ze ) {
                    break;
                }
                String name = ze.getName();
                Log.d( TAG, "next entry name: %s", name );
                SaveWhat what = SaveWhat.valueOf( name );
                success = iter.withEntry( zis, what );
            }
            zis.close();
        }
        return success;
    }

    public static boolean save( Context context, Uri uri,
                                List<SaveWhat> whats )
    {
        Log.d( TAG, "save(%s)", whats );
        boolean success = false;
        ContentResolver resolver = context.getContentResolver();
        // resolver.delete( uri, null, null ); // nuke the file if exists
        try ( OutputStream os = resolver.openOutputStream( uri ) ) {
            ZipOutputStream zos = new ZipOutputStream( os ) ;

            for ( SaveWhat what : whats ) {
                zos.putNextEntry( new ZipEntry( what.entryName() ) );
                switch ( what ) {
                // case SAVE_COLORS:
                //     success = saveColors( zos, ze );
                //     break;
                case SETTINGS:
                    success = saveSettings( context, zos );
                    break;
                case GAMES:
                    success = saveGames( context, zos );
                    break;
                default:
                    Assert.failDbg();
                }
                if ( success ) {
                    zos.closeEntry();
                } else {
                    break;
                }
            }
            zos.close();
            os.close();
        } catch ( Exception ex ) {
            Log.ex( TAG, ex );
        }
        Log.d( TAG, "save(%s) DONE", whats );
        return success;
    }

    private static boolean saveGames( Context context, ZipOutputStream zos )
        throws FileNotFoundException, IOException
    {
        String name = DBHelper.getDBName();
        File gamesDB = context.getDatabasePath( name );
        FileInputStream fis = new FileInputStream( gamesDB );
        boolean success = DBUtils.copyStream( zos, fis );
        return success;
    }

    private static boolean loadGames( Context context, ZipInputStream zis )
        throws FileNotFoundException, IOException
    {
        String name = DBHelper.getDBName();
        File gamesDB = context.getDatabasePath( name );
        FileOutputStream fos = new FileOutputStream( gamesDB );
        boolean success = DBUtils.copyStream( fos, zis );
        return success;
    }

    private static boolean saveSettings( Context context, ZipOutputStream zos )
        throws IOException
    {
        Serializable map = PrefsDelegate.getPrefs( context );
        byte[] asBytes = Utils.serializableToBytes( map );
        ByteArrayInputStream bis = new ByteArrayInputStream( asBytes );
        boolean success = DBUtils.copyStream( zos, bis );
        return success;
    }

    private static boolean loadSettings( Context context, ZipInputStream zis )
    {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        boolean success = DBUtils.copyStream( bos, zis );
        if ( success ) {
            Serializable map = Utils.bytesToSerializable( bos.toByteArray() );
            PrefsDelegate.loadPrefs( context, map );
        }
        return success;
    }
}
