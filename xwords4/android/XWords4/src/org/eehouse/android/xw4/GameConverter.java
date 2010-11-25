/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
/*
 * Copyright 2010 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

import android.content.Context;
import java.io.FileInputStream;
import java.util.ArrayList;

// Rip this out a month or so after releasing...
public class GameConverter {

    public static void convert( Context context )
    {
        String[] games = gamesList( context );
        if ( null == games ) {
            Utils.logf( "GameConverter::convert() no old games found" );
        } else {
            for ( String game : games ) {
                Utils.logf( "GameConverter::convert() converting %s",
                            game );
                byte[] bytes = savedGame( context, game );
                DBUtils.saveGame( context, game, bytes );
                context.deleteFile( game );
            }
        }
    }

    private static byte[] savedGame( Context context, String path )
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
    }

    private static String[] gamesList( Context context )
    {
        ArrayList<String> al = new ArrayList<String>();
        for ( String file : context.fileList() ) {
            if ( file.endsWith( XWConstants.GAME_EXTN ) ) {
                al.add( file );
            }
        }

        int siz = al.size();
        String[] result = null;
        if ( siz > 0 ) {
            result = al.toArray( new String[siz] );
        }
        return result;
    }
}
