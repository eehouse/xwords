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
import android.net.Uri;
import android.os.Environment;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.nio.channels.FileChannel;
import java.util.ArrayList;
import java.util.Arrays;
import android.content.res.AssetManager;
import java.util.concurrent.locks.ReentrantReadWriteLock;
import java.util.concurrent.locks.Lock;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Random;
import android.text.Html;

import junit.framework.Assert;

import org.eehouse.android.xw4.jni.*;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;

public class DictUtils {

    // keep in sync with loc_names string-array
    public enum DictLoc { UNKNOWN, BUILT_IN, INTERNAL, EXTERNAL, DOWNLOAD };
    public static final String INVITED = "invited";

    private static DictAndLoc[] s_dictListCache = null;

    static {
        MountEventReceiver.register( new MountEventReceiver.SDCardNotifiee() {
                public void cardMounted( boolean nowMounted )
                {
                    invalDictList();
                }
            } );
    }
 
    public static class DictPairs {
        public byte[][] m_bytes;
        public String[] m_paths;
        public DictPairs( byte[][] bytes, String[] paths ) {
            m_bytes = bytes; m_paths = paths;
        }

        public boolean anyMissing( final String[] names )
        {
            boolean missing = false;
            for ( int ii = 0; ii < m_paths.length; ++ii ) {
                if ( names[ii] != null ) {
                    // It's ok for there to be no dict IFF there's no
                    // name.  That's a player using the default dict.
                    if ( null == m_paths[ii] && null == m_bytes[ii] ) {
                        missing = true;
                        break;
                    }
                }
            }
            return missing;
        }
    } // DictPairs

    public static class DictAndLoc {
        public DictAndLoc( String pname, DictLoc ploc ) {
            name = removeDictExtn(pname); loc = ploc;
        }
        public String name;
        public DictLoc loc;

        @Override 
        public boolean equals( Object obj ) 
        {
            boolean result = false;
            if ( obj instanceof DictAndLoc ) {
                DictAndLoc other = (DictAndLoc)obj;
                
                result = name.equals( other.name )
                    && loc.equals( other.loc );
            }
            return result;
        }
    }
 
    public static void invalDictList()
    {
        s_dictListCache = null;
        // Should I have a list of folks who want to know when this
        // changes?
    }

    public static DictAndLoc[] dictList( Context context )
    {
        if ( null == s_dictListCache ) {
            ArrayList<DictAndLoc> al = new ArrayList<DictAndLoc>();

            for ( String file : getAssets( context ) ) {
                if ( isDict( file ) ) {
                    al.add( new DictAndLoc( removeDictExtn( file ), 
                                            DictLoc.BUILT_IN ) );
                }
            }

            for ( String file : context.fileList() ) {
                if ( isDict( file ) ) {
                    al.add( new DictAndLoc( removeDictExtn( file ),
                                            DictLoc.INTERNAL ) );
                }
            }

            File sdDir = getSDDir( context );
            if ( null != sdDir ) {
                for ( String file : sdDir.list() ) {
                    if ( isDict( file ) ) {
                        al.add( new DictAndLoc( removeDictExtn( file ),
                                                DictLoc.EXTERNAL ) );
                    }
                }
            }

            s_dictListCache = 
                al.toArray( new DictUtils.DictAndLoc[al.size()] );
        }
        return s_dictListCache;
    }

    public static DictLoc getDictLoc( Context context, String name )
    {
        DictLoc loc = null;
        name = addDictExtn( name );

        for ( String file : getAssets( context ) ) {
            if ( file.equals( name ) ) {
                loc = DictLoc.BUILT_IN;
                break;
            }
        }

        if ( null == loc ) {
            try {
                FileInputStream fis = context.openFileInput( name );
                fis.close();
                loc = DictLoc.INTERNAL;
            } catch ( java.io.FileNotFoundException fnf ) {
            } catch ( java.io.IOException ioe ) {
            }
        }

        if ( null == loc ) {
            File file = getSDPathFor( context, name );
            if ( null != file && file.exists() ) {
                loc = DictLoc.EXTERNAL;
            }
        }

        return loc;
    }

    public static boolean dictExists( Context context, String name )
    {
        return null != getDictLoc( context, name );
    }

    public static boolean dictIsBuiltin( Context context, String name )
    {
        return DictLoc.BUILT_IN == getDictLoc( context, name );
    }

    public static boolean moveDict( Context context, String name,
                                    DictLoc from, DictLoc to )
    {
        boolean success;
        name = addDictExtn( name );

        File toPath = getDictFile( context, name, to );
        if ( null != toPath && toPath.exists() ) {
            success = false;
        } else {
            success = copyDict( context, name, from, to );
            if ( success ) {
                deleteDict( context, name, from );
                invalDictList();
            }
        }
        return success;
    }

    private static boolean copyDict( Context context, String name,
                                     DictLoc from, DictLoc to )
    {
        boolean success = false;

        File file = getSDPathFor( context, name );
        if ( null != file ) {
            FileChannel channelIn = null;
            FileChannel channelOut = null;

            try {
                FileInputStream fis;
                FileOutputStream fos;
                if ( DictLoc.INTERNAL == from ) {
                    fis = context.openFileInput( name );
                    fos = new FileOutputStream( file );
                } else {
                    fis = new FileInputStream( file );
                    fos = context.openFileOutput( name, Context.MODE_PRIVATE );
                }

                channelIn = fis.getChannel();
                channelOut = fos.getChannel();

                channelIn.transferTo( 0, channelIn.size(), channelOut );
                success = true;

            } catch ( java.io.FileNotFoundException fnfe ) {
                Utils.logf( "%s", fnfe.toString() );
            } catch ( java.io.IOException ioe ) {
                Utils.logf( "%s", ioe.toString() );
            } finally {
                try {
                    // Order should match assignment order to above in
                    // case one or both null
                    channelIn.close();
                    channelOut.close();
                } catch ( Exception e ) {
                    Utils.logf( "%s", e.toString() );
                }
            }
        }
        return success;
    } // copyDict

    public static void deleteDict( Context context, String name, DictLoc loc )
    {
        name = addDictExtn( name );
        if ( DictLoc.EXTERNAL == loc ) {
            File onSD = getSDPathFor( context, name );
            if ( null != onSD ) {
                onSD.delete();
            } // otherwise what?
        } else {
            Assert.assertTrue( DictLoc.INTERNAL == loc );
            context.deleteFile( name );
        }
        invalDictList();
    }

    public static void deleteDict( Context context, String name )
    {
        deleteDict( context, name, getDictLoc( context, name ) );
    }

    private static byte[] openDict( Context context, String name, DictLoc loc )
    {
        byte[] bytes = null;

        name = addDictExtn( name );

        if ( loc == DictLoc.UNKNOWN || loc == DictLoc.BUILT_IN ) {
            try {
                AssetManager am = context.getAssets();
                InputStream dict = am.open( name, android.content.res.
                                            AssetManager.ACCESS_RANDOM );

                int len = dict.available(); // this may not be the
                                            // full length!
                bytes = new byte[len];
                int nRead = dict.read( bytes, 0, len );
                if ( nRead != len ) {
                    Utils.logf( "**** warning ****; read only %d of %d bytes.",
                                nRead, len );
                }
                // check that with len bytes we've read the whole file
                Assert.assertTrue( -1 == dict.read() );
            } catch ( java.io.IOException ee ){
                Utils.logf( "%s failed to open; likely not built-in", name );
            }
        }

        // not an asset?  Try external and internal storage
        if ( null == bytes ) {
            try {
                FileInputStream fis = null;
                if ( loc == DictLoc.UNKNOWN || loc == DictLoc.EXTERNAL ) {
                    File sdFile = getSDPathFor( context, name );
                    if ( null != sdFile && sdFile.exists() ) {
                        Utils.logf( "loading %s from SD", name );
                        fis = new FileInputStream( sdFile );
                    }
                }
                if ( null == fis ) {
                    if ( loc == DictLoc.UNKNOWN || loc == DictLoc.INTERNAL ) {
                        Utils.logf( "loading %s from private storage", name );
                        fis = context.openFileInput( name );
                    }
                }
                int len = (int)fis.getChannel().size();
                bytes = new byte[len];
                fis.read( bytes, 0, len );
                fis.close();
                Utils.logf( "Successfully loaded %s", name );
            } catch ( java.io.FileNotFoundException fnf ) {
                Utils.logf( fnf.toString() );
            } catch ( java.io.IOException ioe ) {
                Utils.logf( ioe.toString() );
            }
        }
        
        return bytes;
    } // openDict

    private static byte[] openDict( Context context, String name )
    {
        return openDict( context, name, DictLoc.UNKNOWN );
    }

    private static String getDictPath( Context context, String name )
    {
        name = addDictExtn( name );

        File file = context.getFileStreamPath( name );
        if ( !file.exists() ) {
            file = getSDPathFor( context, name );
            if ( null != file && !file.exists() ) {
                file = null;
            }
        }
        String path = null == file? null : file.getPath();
        return path;
    }

    private static File getDictFile( Context context, String name, DictLoc to )
    {
        File path;
        switch ( to ) {
        case EXTERNAL:
            path = getSDPathFor( context, name );
            break;
        case INTERNAL:
            path = context.getFileStreamPath( name );
            break;
        default:
            Assert.fail();
            path = null;
        }
        return path;
    }

    public static DictPairs openDicts( Context context, String[] names )
    {
        byte[][] dictBytes = new byte[names.length][];
        String[] dictPaths = new String[names.length];

        HashMap<String,byte[]> seen = new HashMap<String,byte[]>();
        for ( int ii = 0; ii < names.length; ++ii ) {
            byte[] bytes = null;
            String path = null;
            String name = names[ii];
            if ( null != name ) {
                path = getDictPath( context, name );
                if ( null == path ) {
                    bytes = seen.get( name );
                    if ( null == bytes ) {
                        bytes = openDict( context, name );
                        seen.put( name, bytes );
                    }
                }
            }
            dictBytes[ii] = bytes;
            dictPaths[ii] = path;
        }
        return new DictPairs( dictBytes, dictPaths );
    }

    public static boolean saveDict( Context context, InputStream in,
                                    String name, DictLoc loc )
    {
        boolean success = false;
        File sdFile = null;
        boolean useSD = DictLoc.EXTERNAL == loc;
        if ( useSD ) {
            sdFile = getSDPathFor( context, name );
        }

        if ( null != sdFile || !useSD ) {
            try {
                FileOutputStream fos;
                if ( null != sdFile ) {
                    fos = new FileOutputStream( sdFile );
                } else {
                    fos = context.openFileOutput( name, Context.MODE_PRIVATE );
                }
                byte[] buf = new byte[1024];
                int nRead;
                while( 0 <= (nRead = in.read( buf, 0, buf.length )) ) {
                    fos.write( buf, 0, nRead );
                }
                fos.close();
                invalDictList();
                success = true;
            } catch ( java.io.FileNotFoundException fnf ) {
                Utils.logf( "saveDict: FileNotFoundException: %s", 
                            fnf.toString() );
            } catch ( java.io.IOException ioe ) {
                Utils.logf( "saveDict: IOException: %s", ioe.toString() );
                deleteDict( context, name );
            }
        }
        return success;
    } 

    private static boolean isGame( String file )
    {
        return file.endsWith( XWConstants.GAME_EXTN );
    }
 
    private static boolean isDict( String file )
    {
        return file.endsWith( XWConstants.DICT_EXTN );
    }

    public static String removeDictExtn( String str )
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
    
    public static boolean haveWriteableSD()
    {
        String state = Environment.getExternalStorageState();

        return state.equals( Environment.MEDIA_MOUNTED );
        // want this later? Environment.MEDIA_MOUNTED_READ_ONLY
    }

    private static File getSDDir( Context context )
    {
        File result = null;
        if ( haveWriteableSD() ) {
            File storage = Environment.getExternalStorageDirectory();
            if ( null != storage ) {
                String packdir = String.format( "Android/data/%s/files/",
                                                context.getPackageName() );
                result = new File( storage.getPath(), packdir );
                if ( !result.exists() ) {
                    result.mkdirs();
                    if ( !result.exists() ) {
                        Utils.logf( "unable to create sd dir %s", packdir );
                        result = null;
                    }
                }
            }
        }
        return result;
    }

    private static File getSDPathFor( Context context, String name )
    {
        File result = null;
        File dir = getSDDir( context );
        if ( dir != null ) {
            result = new File( dir, name );
        }
        return result;
    }
}
