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
import java.io.InputStream;
import java.net.URLEncoder;
import java.util.Iterator;
import org.json.JSONException;
import org.json.JSONObject;

import junit.framework.Assert;

import org.eehouse.android.xw4.loc.LocUtils;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;
import org.eehouse.android.xw4.jni.GameSummary;
import org.eehouse.android.xw4.jni.CurGameInfo;

public class NetLaunchInfo {
    private static final String ADDRS_KEY = "ADDRS";

    protected String gameName;
    protected String dict;
    protected int lang;
    protected int forceChannel;
    protected int nPlayersT;
    protected int nPlayersH;
    protected String room;      // relay
    protected String btName;
    protected String btAddress;
    protected String phone;     // SMS
    protected boolean isGSM;    // SMS
    protected String osVers;    // SMS

    protected int gameID;

    private CommsConnTypeSet m_addrs;
    private JSONObject m_json;
    private boolean m_valid;
    private String m_inviteID;

    public NetLaunchInfo()
    {
        m_addrs = new CommsConnTypeSet();
    }

    public NetLaunchInfo( String data )
    {
        try { 
            m_json = new JSONObject( data );

            int flags = m_json.getInt(ADDRS_KEY);
            m_addrs = DBUtils.intToConnTypeSet( flags );

            lang = m_json.optInt( MultiService.LANG, -1 );
            forceChannel = m_json.optInt( MultiService.FORCECHANNEL, 0 );
            dict = m_json.optString( MultiService.DICT );
            gameName = m_json.optString( MultiService.GAMENAME );
            nPlayersT = m_json.optInt( MultiService.NPLAYERST, -1 );
            nPlayersH = m_json.optInt( MultiService.NPLAYERSH, -1 );
            gameID = m_json.optInt( MultiService.GAMEID, -1 );

            for ( CommsConnType typ : m_addrs.getTypes() ) {
                switch ( typ ) {
                case COMMS_CONN_BT:
                    btAddress = m_json.optString( MultiService.BT_ADDRESS );
                    btName = m_json.optString( MultiService.BT_NAME );
                    break;
                case COMMS_CONN_RELAY:
                    room = m_json.optString( MultiService.ROOM );
                    m_inviteID = m_json.optString( MultiService.INVITEID );
                    break;
                case COMMS_CONN_SMS:
                    phone = m_json.optString( "phn" );
                    isGSM = m_json.optBoolean( "gsm", false );
                    osVers = m_json.optString( "os" );
                    break;
                default:
                    DbgUtils.logf( "Unexpected typ %s", typ.toString() );
                    break;
                }
            }

            calcValid();
        } catch ( JSONException jse ) {
            DbgUtils.loge( jse );
        }
    }

    public NetLaunchInfo( Bundle bundle )
    {
        room = bundle.getString( MultiService.ROOM );
        m_inviteID = bundle.getString( MultiService.INVITEID );
        lang = bundle.getInt( MultiService.LANG );
        forceChannel = bundle.getInt( MultiService.FORCECHANNEL );
        dict = bundle.getString( MultiService.DICT );
        gameName= bundle.getString( MultiService.GAMENAME );
        nPlayersT = bundle.getInt( MultiService.NPLAYERST );
        nPlayersH = bundle.getInt( MultiService.NPLAYERSH );
        gameID = bundle.getInt( MultiService.GAMEID );
        btName = bundle.getString( MultiService.BT_NAME );
        btAddress = bundle.getString( MultiService.BT_ADDRESS );

        m_addrs = DBUtils.intToConnTypeSet( bundle.getInt( ADDRS_KEY ) );
    }

    public NetLaunchInfo( Context context, Uri data )
    {
        this();
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
                    m_inviteID = json.getString( MultiService.INVITEID );
                } else {
                    room = data.getQueryParameter( "room" );
                    m_inviteID = data.getQueryParameter( "id" );
                    dict = data.getQueryParameter( "wl" );
                    String langStr = data.getQueryParameter( "lang" );
                    lang = Integer.decode( langStr );
                    String np = data.getQueryParameter( "np" );
                    nPlayersT = Integer.decode( np );
                }
                calcValid();
            } catch ( Exception e ) {
                DbgUtils.logf( "unable to parse \"%s\"", data.toString() );
            }
        }
        calcValid();
    }

    public NetLaunchInfo( Intent intent )
    {
        room = intent.getStringExtra( MultiService.ROOM );
        m_inviteID = intent.getStringExtra( MultiService.INVITEID );
        lang = intent.getIntExtra( MultiService.LANG, -1 );
        forceChannel = intent.getIntExtra( MultiService.FORCECHANNEL, -1 );
        dict = intent.getStringExtra( MultiService.DICT );
        gameName = intent.getStringExtra( MultiService.GAMENAME );
        nPlayersT = intent.getIntExtra( MultiService.NPLAYERST, -1 );
        nPlayersH = intent.getIntExtra( MultiService.NPLAYERSH, -1 );
        gameID = intent.getIntExtra( MultiService.GAMEID, -1 );
        btName = intent.getStringExtra( MultiService.BT_NAME );
        btAddress = intent.getStringExtra( MultiService.BT_ADDRESS );
        m_addrs = DBUtils.intToConnTypeSet( intent.getIntExtra( ADDRS_KEY, -1 ) );

        calcValid();
    }

    public NetLaunchInfo( int gamID, int dictLang, String dictName, int nPlayers )
    {
        this();
        dict = dictName;
        lang = dictLang;
        nPlayersT = nPlayers;
        nPlayersH = 1;
        gameID = gamID;
    }

    public NetLaunchInfo( GameSummary summary, CurGameInfo gi, int numHere )
    {
        this( summary, gi );
        nPlayersH = numHere;
    }

    public NetLaunchInfo( GameSummary summary, CurGameInfo gi )
    {
        this( gi.gameID, gi.dictLang, gi.dictName, gi.nPlayers );

        for ( CommsConnType typ : summary.conTypes.getTypes() ) {
            DbgUtils.logf( "NetLaunchInfo(): got type %s", typ.toString() );
            switch( typ ) {
            case COMMS_CONN_BT:
                addBTInfo();
                break;
            case COMMS_CONN_RELAY:
                addRelayInfo( summary.roomName, summary.relayID );
                break;
            case COMMS_CONN_SMS:
                addSMSInfo();
                break;
            default:
                Assert.fail();
                break;
            }
        }
    }

    public String inviteID()
    { 
        String result = m_inviteID;
        if ( null == result ) {
            result = GameUtils.formatGameID( gameID );
            DbgUtils.logf( "inviteID(): m_inviteID null so substituting %s", result );
        }
        return result;
    }

    public void putSelf( Bundle bundle )
    {
        bundle.putString( MultiService.ROOM, room );
        bundle.putString( MultiService.INVITEID, m_inviteID );
        bundle.putInt( MultiService.LANG, lang );
        bundle.putString( MultiService.DICT, dict );
        bundle.putString( MultiService.GAMENAME, gameName );
        bundle.putInt( MultiService.NPLAYERST, nPlayersT );
        bundle.putInt( MultiService.NPLAYERSH, nPlayersH );
        bundle.putInt( MultiService.GAMEID, gameID );
        bundle.putString( MultiService.BT_NAME, btName );
        bundle.putString( MultiService.BT_ADDRESS, btAddress );

        int flags = DBUtils.connTypeSetToInt( m_addrs );
        bundle.putInt( ADDRS_KEY, flags );
    }

    public String makeLaunchJSON()
    {
        String result = null;
        try {
            result = new JSONObject()
                .put( MultiService.LANG, lang )
                .put( MultiService.DICT, dict )
                .put( MultiService.GAMENAME, gameName )
                .put( MultiService.NPLAYERST, nPlayersT )
                .put( MultiService.NPLAYERSH, nPlayersH )
                .put( MultiService.ROOM, room )
                .put( MultiService.INVITEID, m_inviteID )
                .put( MultiService.GAMEID, gameID )
                .put( MultiService.BT_NAME, btName )
                .put( MultiService.BT_ADDRESS, btAddress )
                .put( ADDRS_KEY, DBUtils.connTypeSetToInt( m_addrs ) )
                .toString();
        } catch ( org.json.JSONException jse ) {
            DbgUtils.loge( jse );
        }
        // DbgUtils.logf( "makeLaunchJSON() => %s", result );
        return result;
    }

    public CommsAddrRec makeAddrRec( Context context )
    {
        CommsAddrRec result = new CommsAddrRec();
        for ( CommsConnType typ : m_addrs.getTypes() ) {
            result.conTypes.add( typ );
            switch( typ ) {
            case COMMS_CONN_RELAY:
                String relayName = XWPrefs.getDefaultRelayHost( context );
                int relayPort = XWPrefs.getDefaultRelayPort( context );
                result.setRelayParams( relayName, relayPort, room );
                break;
            case COMMS_CONN_BT:
                result.setBTParams( btAddress, btName );
                break;
            case COMMS_CONN_SMS:
            default:
                Assert.fail();
                break;
            }
        }

        return result;
    }

    public Uri makeLaunchUri( Context context )
    {
        Uri.Builder ub = new Uri.Builder()
            .scheme( "http" )
            .path( String.format( "//%s%s", 
                                  LocUtils.getString(context, R.string.invite_host),
                                  LocUtils.getString(context, R.string.invite_prefix) ) )
            .appendQueryParameter( "lang", String.format("%d", lang ) )
            .appendQueryParameter( "np", String.format( "%d", nPlayersT ) )
            .appendQueryParameter( "nh", String.format( "%d", nPlayersH ) )
            .appendQueryParameter( "gid", String.format( "%d", nPlayersT ) );
        if ( null != dict ) {
            ub.appendQueryParameter( "wl", dict );
        }

        if ( m_addrs.contains( CommsConnType.COMMS_CONN_RELAY ) ) {
            ub.appendQueryParameter( "room", room );
            ub.appendQueryParameter( "id", m_inviteID );
        }
        if ( m_addrs.contains( CommsConnType.COMMS_CONN_BT ) ) {
            ub.appendQueryParameter( "bta", btAddress );
            ub.appendQueryParameter( "btn", btName );
        }
        if ( m_addrs.contains( CommsConnType.COMMS_CONN_SMS ) ) {
            ub.appendQueryParameter( "phn", phone );
        }
        return ub.build();
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
    
    public void addRelayInfo( String aRoom, String inviteID )
    {
        room = aRoom;
        m_inviteID = inviteID;
        m_addrs.add( CommsConnType.COMMS_CONN_RELAY );
    }

    public void addBTInfo()
    {
        String[] got = BTService.getBTNameAndAddress();
        if ( null != got ) {
            btName = got[0];
            btAddress = got[1];
            m_addrs.add( CommsConnType.COMMS_CONN_BT );
        } else {
            DbgUtils.logf( "addBTInfo(): no BT info available" );
        }
    }

    public void addSMSInfo()
    {
        // look up own phone number, which will require new permission
        Assert.fail();
        phone = "123-456-7890";
        m_addrs.add( CommsConnType.COMMS_CONN_SMS );
    }

    public boolean isValid()
    {
        DbgUtils.logf( "NetLaunchInfo(%s).isValid() => %b", toString(), m_valid );
        return m_valid;
    }

    @Override
    public String toString()
    {
        return makeLaunchJSON();
    }

    public static void putExtras( Intent intent, int gameID, String btAddr )
    {
        Assert.fail();
    }

    private boolean hasCommon()
    {
        return null != dict
            && 0 < lang
            && 0 < nPlayersT
            && 0 != gameID;
    }

    private void calcValid()
    {
        boolean valid = hasCommon();
        for ( Iterator<CommsConnType> iter = m_addrs.iterator();
              valid && iter.hasNext(); ) {
            switch ( iter.next() ) {
            case COMMS_CONN_RELAY:
                valid = null != room && null != m_inviteID;
                break;
            case COMMS_CONN_BT:
                valid = null != btAddress && 0 != gameID;
                break;
            case COMMS_CONN_SMS:
                Assert.fail();
                valid = false;
                break;
            }
        }
        m_valid = valid;
    }
}
