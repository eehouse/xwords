/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
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

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import java.net.URLEncoder;
import java.io.InputStream;
import org.json.JSONObject;
import org.json.JSONException;

import junit.framework.Assert;

import org.eehouse.android.xw4.loc.LocUtils;

public class NetLaunchInfo extends AbsLaunchInfo {
    protected String room;
    protected String inviteID;

    public NetLaunchInfo( String data )
    {
        try { 
            JSONObject json = init( data );
            room = json.getString( MultiService.ROOM );
            inviteID = json.getString( MultiService.INVITEID );
            setValid( true );
        } catch ( JSONException jse ) {
            // Don't bother logging; it's just not a valid object of this type
        }
    }

    public void putSelf( Bundle bundle )
    {
        super.putSelf( bundle );
        bundle.putString( MultiService.ROOM, room );
        bundle.putString( MultiService.INVITEID, inviteID );
    }

    public NetLaunchInfo( Bundle bundle )
    {
        init( bundle );
        room = bundle.getString( MultiService.ROOM );
        inviteID = bundle.getString( MultiService.INVITEID );
    }

    public NetLaunchInfo( Context context, Uri data )
    {
        setValid( false );
        if ( null != data ) {
            String scheme = data.getScheme();
            try {
                if ( "content".equals(scheme) || "file".equals(scheme) ) {
                    Assert.assertNotNull( context );
                    ContentResolver resolver = context.getContentResolver();
                    InputStream is = resolver.openInputStream( data );
                    int len = is.available();
                    byte[] buf = new byte[len];
                    is.read( buf );

                    JSONObject json = init( new String( buf ) );
                    room = json.getString( MultiService.ROOM );
                    inviteID = json.getString( MultiService.INVITEID );
                } else {
                    room = data.getQueryParameter( "room" );
                    inviteID = data.getQueryParameter( "id" );
                    dict = data.getQueryParameter( "wl" );
                    String langStr = data.getQueryParameter( "lang" );
                    lang = Integer.decode( langStr );
                    String np = data.getQueryParameter( "np" );
                    nPlayersT = Integer.decode( np );
                }
                setValid( true );
            } catch ( Exception e ) {
                DbgUtils.logf( "unable to parse \"%s\"", data.toString() );
            }
        }
    }

    public NetLaunchInfo( Intent intent )
    {
        init( intent );
        room = intent.getStringExtra( MultiService.ROOM );
        inviteID = intent.getStringExtra( MultiService.INVITEID );
        boolean valid = null != room
            && -1 != lang
            && -1 != nPlayersT;
        setValid( valid );
    }

    public static Uri makeLaunchUri( Context context, String room,
                                     String inviteID, int lang, 
                                     String dict, int nPlayersT )
    {
        Uri.Builder ub = new Uri.Builder()
            .scheme( "http" )
            .path( String.format( "//%s%s", 
                                  LocUtils.getString(context, R.string.invite_host),
                                  LocUtils.getString(context, R.string.invite_prefix) ) )
            .appendQueryParameter( "lang", String.format("%d", lang ) )
            .appendQueryParameter( "np", String.format( "%d", nPlayersT ) )
            .appendQueryParameter( "room", room )
            .appendQueryParameter( "id", inviteID );
        if ( null != dict ) {
            ub.appendQueryParameter( "wl", dict );
        }
        return ub.build();
    }

    public static String makeLaunchJSON( String room, String inviteID, int lang, 
                                         String dict, int nPlayersT )
    {
        String result = null;
        try {
            result = makeLaunchJSONObject( lang, dict, nPlayersT )
                .put( MultiService.ROOM, room )
                .put( MultiService.INVITEID, inviteID )
                .toString();
        } catch ( org.json.JSONException jse ) {
            DbgUtils.loge( jse );
        }
        return result;
    }
}
