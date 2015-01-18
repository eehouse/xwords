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
import android.telephony.TelephonyManager;
import java.io.InputStream;
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
    private static final String ADDRS_KEY = "ad";
    private static final String PHONE_KEY = "phn";
    private static final String GSM_KEY = "gsm";
    private static final String OSVERS_KEY = "osv";
    private static final String BTADDR_KEY = "bta";
    private static final String BTNAME_KEY = "btn";
    private static final String ROOM_KEY = "room";
    private static final String ID_KEY = "id";
    private static final String WORDLIST_KEY = "wl";
    private static final String LANG_KEY = "lang";
    private static final String TOTPLAYERS_KEY = "np";
    private static final String HEREPLAYERS_KEY = "nh";
    private static final String GID_KEY = "gid";
    private static final String FORCECHANNEL_KEY = "fc";

    protected String gameName;
    protected String dict;
    protected int lang;
    protected int forceChannel;
    protected int nPlayersT;
    protected int nPlayersH;
    protected String room;      // relay
    protected String btName;
    protected String btAddress;
    // SMS
    protected String phone;
    protected boolean isGSM;
    protected int osVers;

    protected int gameID;

    private CommsConnTypeSet m_addrs;
    private boolean m_valid;
    private String m_inviteID;

    public NetLaunchInfo()
    {
        m_addrs = new CommsConnTypeSet();
    }

    public NetLaunchInfo( String data )
    {
        try { 
            JSONObject json = new JSONObject( data );

            int flags = json.getInt(ADDRS_KEY);
            m_addrs = DBUtils.intToConnTypeSet( flags );

            lang = json.optInt( MultiService.LANG, -1 );
            forceChannel = json.optInt( MultiService.FORCECHANNEL, 0 );
            dict = json.optString( MultiService.DICT );
            gameName = json.optString( MultiService.GAMENAME );
            nPlayersT = json.optInt( MultiService.NPLAYERST, -1 );
            nPlayersH = json.optInt( MultiService.NPLAYERSH, -1 );
            gameID = json.optInt( MultiService.GAMEID, -1 );

            for ( CommsConnType typ : m_addrs.getTypes() ) {
                switch ( typ ) {
                case COMMS_CONN_BT:
                    btAddress = json.optString( MultiService.BT_ADDRESS );
                    btName = json.optString( MultiService.BT_NAME );
                    break;
                case COMMS_CONN_RELAY:
                    room = json.optString( MultiService.ROOM );
                    m_inviteID = json.optString( MultiService.INVITEID );
                    break;
                case COMMS_CONN_SMS:
                    phone = json.optString( PHONE_KEY );
                    isGSM = json.optBoolean( GSM_KEY, false );
                    osVers = json.optInt( OSVERS_KEY );
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
                    int addrs = Integer.decode( data.getQueryParameter( ADDRS_KEY ) );
                    m_addrs = DBUtils.intToConnTypeSet( addrs );

                    if ( m_addrs.contains( CommsConnType.COMMS_CONN_RELAY ) ) {
                        room = data.getQueryParameter( ROOM_KEY );
                        m_inviteID = data.getQueryParameter( ID_KEY );
                    }
                    if ( m_addrs.contains( CommsConnType.COMMS_CONN_BT ) ) {
                        btAddress = data.getQueryParameter( BTADDR_KEY );
                        btName = data.getQueryParameter( BTNAME_KEY );
                    }
                    if ( m_addrs.contains( CommsConnType.COMMS_CONN_SMS ) ) {
                        phone = data.getQueryParameter( PHONE_KEY );
                        isGSM = 1 == Integer
                            .decode(data.getQueryParameter( GSM_KEY ) );
                        osVers = Integer.decode(data.getQueryParameter(OSVERS_KEY));
                    }

                    dict = data.getQueryParameter( WORDLIST_KEY );
                    String langStr = data.getQueryParameter( LANG_KEY );
                    lang = Integer.decode( langStr );
                    String np = data.getQueryParameter( TOTPLAYERS_KEY );
                    nPlayersT = Integer.decode( np );
                    String nh = data.getQueryParameter( HEREPLAYERS_KEY );
                    nPlayersH = Integer.decode( nh );
                    gameID = Integer.decode( data.getQueryParameter( GID_KEY ) );
                    forceChannel = Integer.decode( data.getQueryParameter( FORCECHANNEL_KEY ) );
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

    public NetLaunchInfo( GameSummary summary, CurGameInfo gi, int numHere, int fc )
    {
        this( summary, gi );
        nPlayersH = numHere;
        forceChannel = fc;
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
                addSMSInfo( summary.getContext() );
                break;
            default:
                Assert.fail();
                break;
            }
        }
    }

    public boolean contains( CommsConnType typ )
    {
        return m_addrs.contains( typ );
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
        bundle.putInt( MultiService.FORCECHANNEL, forceChannel );

        int flags = DBUtils.connTypeSetToInt( m_addrs );
        bundle.putInt( ADDRS_KEY, flags );
    }

    public String makeLaunchJSON()
    {
        String result = null;
        try {
            JSONObject obj = new JSONObject()
                .put( ADDRS_KEY, DBUtils.connTypeSetToInt( m_addrs ) )
                .put( MultiService.LANG, lang )
                .put( MultiService.DICT, dict )
                .put( MultiService.GAMENAME, gameName )
                .put( MultiService.NPLAYERST, nPlayersT )
                .put( MultiService.NPLAYERSH, nPlayersH )
                .put( MultiService.GAMEID, gameID )
                .put( MultiService.FORCECHANNEL, forceChannel );

            if ( m_addrs.contains( CommsConnType.COMMS_CONN_RELAY ) ) {
                obj.put( MultiService.ROOM, room )
                    .put( MultiService.INVITEID, m_inviteID );
            }

            if ( m_addrs.contains( CommsConnType.COMMS_CONN_BT ) ) {
                obj.put( MultiService.BT_NAME, btName )
                    .put( MultiService.BT_ADDRESS, btAddress );
            }
            if ( m_addrs.contains( CommsConnType.COMMS_CONN_SMS ) ) {
                obj.put( PHONE_KEY, phone )
                    .put( GSM_KEY, isGSM )
                    .put( OSVERS_KEY, osVers );
            }
            result = obj.toString();

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
                result.setSMSParams( phone );
                break;
            default:
                Assert.fail();
                break;
            }
        }

        return result;
    }

    private void appendInt( Uri.Builder ub, String key, int value )
    {
        ub.appendQueryParameter( key, String.format("%d", value ) );
    }

    public Uri makeLaunchUri( Context context )
    {
        int addrs = DBUtils.connTypeSetToInt( m_addrs );
        Uri.Builder ub = new Uri.Builder()
            .scheme( "http" )
            .path( String.format( "//%s%s", 
                                  LocUtils.getString(context, R.string.invite_host),
                                  LocUtils.getString(context, R.string.invite_prefix) ) );
        appendInt( ub, LANG_KEY, lang );
        appendInt( ub, TOTPLAYERS_KEY, nPlayersT );
        appendInt( ub, HEREPLAYERS_KEY, nPlayersH );
        appendInt( ub, GID_KEY, gameID );
        appendInt( ub, FORCECHANNEL_KEY, forceChannel );
        appendInt( ub, ADDRS_KEY, addrs );

        if ( null != dict ) {
            ub.appendQueryParameter( WORDLIST_KEY, dict );
        }

        if ( m_addrs.contains( CommsConnType.COMMS_CONN_RELAY ) ) {
            ub.appendQueryParameter( ROOM_KEY, room );
            ub.appendQueryParameter( ID_KEY, m_inviteID );
        }
        if ( m_addrs.contains( CommsConnType.COMMS_CONN_BT ) ) {
            ub.appendQueryParameter( BTADDR_KEY, btAddress );
            ub.appendQueryParameter( BTNAME_KEY, btName );
        }
        if ( m_addrs.contains( CommsConnType.COMMS_CONN_SMS ) ) {
            ub.appendQueryParameter( PHONE_KEY, phone );
            appendInt( ub, GSM_KEY, (isGSM? 1 : 0) );
            appendInt( ub, OSVERS_KEY, osVers );
        }
        Uri result = ub.build();

        // Now test
        DbgUtils.logf( "testing %s...", result.toString() );
        NetLaunchInfo instance = new NetLaunchInfo( context, result );
        Assert.assertTrue( instance.isValid() );

        return result;
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

    public void addSMSInfo( Context context )
    {
        // look up own phone number, which will require new permission
        TelephonyManager mgr = (TelephonyManager)
            context.getSystemService(Context.TELEPHONY_SERVICE);
        phone = mgr.getLine1Number();
        DbgUtils.logf( "addSMSInfo(): got phone: %s", phone );

        int type = mgr.getPhoneType();
        isGSM = TelephonyManager.PHONE_TYPE_GSM == type;
        osVers = Integer.valueOf( android.os.Build.VERSION.SDK );

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
                valid = null != room && null != inviteID();
                break;
            case COMMS_CONN_BT:
                valid = null != btAddress && 0 != gameID;
                break;
            case COMMS_CONN_SMS:
                valid = null != phone && 0 < osVers;
                break;
            }
        }
        m_valid = valid;
    }
}
