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

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import java.net.URLEncoder;
import java.io.InputStream;
import org.json.JSONObject;
import junit.framework.Assert;


public class NetLaunchInfo {
    public String room;
    public String inviteID;
    public String dict;
    public int lang;
    public int nPlayersT;

    private static final String LANG = "netlaunchinfo_lang";
    private static final String ROOM = "netlaunchinfo_room";
    private static final String DICT = "netlaunchinfo_dict";
    private static final String INVITEID = "netlaunchinfo_inviteid";
    private static final String NPLAYERS = "netlaunchinfo_nplayers";
    private static final String VALID = "netlaunchinfo_valid";

    private boolean m_valid;

    public void putSelf( Bundle bundle )
    {
        bundle.putInt( LANG, lang );
        bundle.putString( ROOM, room );
        bundle.putString( INVITEID, inviteID );
        bundle.putString( DICT, dict );
        bundle.putInt( NPLAYERS, nPlayersT );
        bundle.putBoolean( VALID, m_valid );
    }

    public NetLaunchInfo( String data )
    {
        try { 
            JSONObject json = new JSONObject( data );
            room = json.getString( MultiService.ROOM );
            inviteID = json.getString( MultiService.INVITEID );
            lang = json.getInt( MultiService.LANG );
            dict = json.getString( MultiService.DICT );
            nPlayersT = json.getInt( MultiService.NPLAYERST );
            m_valid = true;
        } catch ( org.json.JSONException jse ) {
            m_valid = false;
        }
    }

    public NetLaunchInfo( Bundle bundle )
    {
        lang = bundle.getInt( LANG );
        room = bundle.getString( ROOM );
        dict = bundle.getString( DICT );
        inviteID = bundle.getString( INVITEID );
        nPlayersT = bundle.getInt( NPLAYERS );
        m_valid = bundle.getBoolean( VALID );
    }

    public NetLaunchInfo( Context context, Uri data )
    {
        m_valid = false;
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

                    JSONObject json = new JSONObject( new String( buf ) );
                    room = json.getString( MultiService.ROOM );
                    inviteID = json.getString( MultiService.INVITEID );
                    lang = json.getInt( MultiService.LANG );
                    dict = json.getString( MultiService.DICT );
                    nPlayersT = json.getInt( MultiService.NPLAYERST );
                } else {
                    room = data.getQueryParameter( "room" );
                    inviteID = data.getQueryParameter( "id" );
                    dict = data.getQueryParameter( "wl" );
                    String langStr = data.getQueryParameter( "lang" );
                    lang = Integer.decode( langStr );
                    String np = data.getQueryParameter( "np" );
                    nPlayersT = Integer.decode( np );
                }
                m_valid = true;
            } catch ( Exception e ) {
                DbgUtils.logf( "unable to parse \"%s\"", data.toString() );
            }
        }
    }

    public NetLaunchInfo( Intent intent )
    {
        room = intent.getStringExtra( MultiService.ROOM );
        inviteID = intent.getStringExtra( MultiService.INVITEID );
        lang = intent.getIntExtra( MultiService.LANG, -1 );
        dict = intent.getStringExtra( MultiService.DICT );
        nPlayersT = intent.getIntExtra( MultiService.NPLAYERST, -1 );
        m_valid = null != room
            && -1 != lang
            && -1 != nPlayersT;
    }

    public static Uri makeLaunchUri( Context context, String room,
                                     String inviteID, int lang, 
                                     String dict, int nPlayersT )
    {
        Uri.Builder ub = new Uri.Builder()
            .scheme( "http" )
            .path( String.format( "//%s%s", 
                                  context.getString(R.string.invite_host),
                                  context.getString(R.string.invite_prefix) ) )
            .appendQueryParameter( "lang", String.format("%d", lang ) )
            .appendQueryParameter( "np", String.format( "%d", nPlayersT ) )
            .appendQueryParameter( "room", room )
            .appendQueryParameter( "id", inviteID );
        if ( null != dict ) {
            ub.appendQueryParameter( "wl", dict );
        }
        return ub.build();
    }

    public static String makeLaunchJSON( Context context, String room,
                                         String inviteID, int lang, 
                                         String dict, int nPlayersT )
    {
        String result = null;
        try {
            result = new JSONObject()
                .put( MultiService.ROOM, room )
                .put( MultiService.INVITEID, inviteID )
                .put( MultiService.LANG, lang )
                .put( MultiService.DICT, dict )
                .put( MultiService.NPLAYERST, nPlayersT )
                .toString();
        } catch ( org.json.JSONException jse ) {
            DbgUtils.loge( jse );
        }
        return result;
    }
    
    public boolean isValid()
    {
        return m_valid;
    }
}
