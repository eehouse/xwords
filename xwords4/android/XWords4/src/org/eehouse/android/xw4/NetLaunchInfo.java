/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2009-2011 by Eric House (xwords@eehouse.org).  All
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

import android.content.Context;
import android.net.Uri;
import android.net.Uri.Builder;
import android.os.Bundle;
import java.net.URLEncoder;

import org.eehouse.android.xw4.jni.CommonPrefs;


public class NetLaunchInfo {
    public String room;
    public int lang;
    public int nPlayers;

    private static final String LANG = "netlaunchinfo_lang";
    private static final String ROOM = "netlaunchinfo_room";
    private static final String NPLAYERS = "netlaunchinfo_nplayers";
    private static final String VALID = "netlaunchinfo_valid";

    private boolean m_valid;

    public void putSelf( Bundle bundle )
    {
        bundle.putInt( LANG, lang );
        bundle.putString( ROOM, room );
        bundle.putInt( NPLAYERS, nPlayers );
        bundle.putBoolean( VALID, m_valid );
    }

    public NetLaunchInfo( Bundle bundle )
    {
        lang = bundle.getInt( LANG  );
        room = bundle.getString( ROOM );
        nPlayers = bundle.getInt( NPLAYERS  );
        m_valid = bundle.getBoolean( VALID  );
    }

    public static Uri makeLaunchUri( Context context, String room,
                                        int lang, int nPlayers )
    {
        Builder ub = new Builder();
        ub.scheme( "http" );
        String format = context.getString( R.string.game_url_pathf );
        ub.path( String.format( format,
                                CommonPrefs.getDefaultRedirHost( context ) ) );
        
        ub.appendQueryParameter( "lang", String.format("%d", lang ) );
        ub.appendQueryParameter( "np", String.format( "%d", nPlayers ) );
        ub.appendQueryParameter( "room", room );
        return ub.build();
    }

    public NetLaunchInfo( Uri data )
    {
        m_valid = false;
        if ( null != data ) {
            try {
                room = data.getQueryParameter( "room" );
                String langStr = data.getQueryParameter( "lang" );
                lang = Integer.decode( langStr );
                String np = data.getQueryParameter( "np" );
                nPlayers = Integer.decode( np );
                m_valid = true;
            } catch ( Exception e ) {
                DbgUtils.logf( "unable to parse \"%s\"", data.toString() );
            }
        }
    }

    public boolean isValid()
    {
        return m_valid;
    }
}
