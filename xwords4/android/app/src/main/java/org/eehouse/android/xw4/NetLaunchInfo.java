/* -*- compile-command: "find-and-gradle.sh inXw4Deb"; -*- */
/*
 * Copyright 2009 - 2018 by Eric House (xwords@eehouse.org).  All rights
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

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.InputStream;
import java.io.Serializable;

import java.util.Iterator;
import java.util.List;

import org.json.JSONException;
import org.json.JSONObject;


import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.CurGameInfo;
import org.eehouse.android.xw4.jni.GameSummary;
import org.eehouse.android.xw4.loc.LocUtils;

public class NetLaunchInfo implements Serializable {
    private static final String TAG = NetLaunchInfo.class.getSimpleName();
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
    private static final String NAME_KEY = "nm";
    private static final String P2P_MAC_KEY = "p2";

    protected String gameName;
    protected String dict;
    protected int lang;
    protected int forceChannel;
    protected int nPlayersT;
    protected int nPlayersH;
    protected boolean remotesAreRobots;
    protected String room;      // relay
    protected String btName;
    protected String btAddress;
    protected String p2pMacAddress;
    // SMS
    protected String phone;
    protected boolean isGSM;
    protected int osVers;

    private int _conTypes;      // for syncing with the c version only!!
    private int gameID = 0;
    private CommsConnTypeSet m_addrs;
    private boolean m_valid;
    private String inviteID;

    public NetLaunchInfo()
    {
        m_addrs = new CommsConnTypeSet();
        // Give it a random number. It may be overwritten, but so what.
        inviteID = GameUtils.formatGameID( Utils.nextRandomInt() );
    }

    private NetLaunchInfo( Context context, String data ) throws JSONException
    {
        init( context, data );
    }

    private NetLaunchInfo( Bundle bundle )
    {
        lang = bundle.getInt( MultiService.LANG );
        room = bundle.getString( MultiService.ROOM );
        inviteID = bundle.getString( MultiService.INVITEID );
        forceChannel = bundle.getInt( MultiService.FORCECHANNEL );
        dict = bundle.getString( MultiService.DICT );
        gameName = bundle.getString( MultiService.GAMENAME );
        nPlayersT = bundle.getInt( MultiService.NPLAYERST );
        nPlayersH = bundle.getInt( MultiService.NPLAYERSH );
        remotesAreRobots = bundle.getBoolean( MultiService.REMOTES_ROBOTS );
        gameID = bundle.getInt( MultiService.GAMEID );
        btName = bundle.getString( MultiService.BT_NAME );
        btAddress = bundle.getString( MultiService.BT_ADDRESS );
        p2pMacAddress = bundle.getString( MultiService.P2P_MAC_ADDRESS );

        m_addrs = new CommsConnTypeSet( bundle.getInt( ADDRS_KEY ) );

        Utils.testSerialization( this );
    }

    public static NetLaunchInfo makeFrom( Bundle bundle )
    {
        NetLaunchInfo nli = null;
        if ( 0 != bundle.getInt( MultiService.LANG ) ) { // quick test: valid?
            nli = new NetLaunchInfo( bundle );
            nli.calcValid();
            if ( !nli.isValid() ) {
                nli = null;
            }
        }
        return nli;
    }

    public static NetLaunchInfo makeFrom( Context context, String data )
    {
        NetLaunchInfo nli = null;
        try {
            nli = new NetLaunchInfo( context, data );
        } catch ( JSONException jse ) {
            Log.ex( TAG, jse );
        }
        return nli;
    }

    public static NetLaunchInfo makeFrom( Context context, byte[] data )
    {
        NetLaunchInfo nli = null;
        try {
            ByteArrayInputStream bais = new ByteArrayInputStream( data );
            DataInputStream dis = new DataInputStream( bais );
            String nliData = dis.readUTF();
            nli = NetLaunchInfo.makeFrom( context, nliData );
        } catch ( java.io.IOException ex ) {
            Assert.assertFalse( BuildConfig.DEBUG );
        }
        return nli;
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
                    inviteID = json.getString( MultiService.INVITEID );
                } else {
                    String val = data.getQueryParameter( ADDRS_KEY );
                    boolean hasAddrs = null != val;
                    if ( hasAddrs ) {
                        m_addrs = new CommsConnTypeSet( Integer.decode( val ) );
                    } else {
                        m_addrs = new CommsConnTypeSet();
                    }

                    List<CommsConnType> supported = CommsConnTypeSet.getSupported( context );
                    for ( CommsConnType typ : supported ) {
                        if ( hasAddrs && !m_addrs.contains( typ ) ) {
                            continue;
                        }
                        boolean doAdd;
                        switch ( typ ) {
                        case COMMS_CONN_BT:
                            btAddress = data.getQueryParameter( BTADDR_KEY );
                            btName = data.getQueryParameter( BTNAME_KEY );
                            doAdd = !hasAddrs && null != btAddress;
                            break;
                        case COMMS_CONN_RELAY:
                            room = data.getQueryParameter( ROOM_KEY );
                            inviteID = data.getQueryParameter( ID_KEY );
                            doAdd = !hasAddrs && null != room;
                            break;
                        case COMMS_CONN_SMS:
                            phone = data.getQueryParameter( PHONE_KEY );
                            val = data.getQueryParameter( GSM_KEY );
                            isGSM = null != val && 1 == Integer.decode( val );
                            val = data.getQueryParameter( OSVERS_KEY );
                            if ( null != val ) {
                                osVers = Integer.decode( val );
                            }
                            doAdd = !hasAddrs && null != phone;
                            break;
                        case COMMS_CONN_P2P:
                            p2pMacAddress = data.getQueryParameter( P2P_MAC_KEY );
                            doAdd = !hasAddrs && null != p2pMacAddress;
                            break;
                        default:
                            doAdd = false;
                            Assert.fail();
                        }
                        if ( doAdd ) {
                            m_addrs.add( typ );
                        }
                    }

                    removeUnsupported( supported );

                    dict = data.getQueryParameter( WORDLIST_KEY );
                    String langStr = data.getQueryParameter( LANG_KEY );
                    lang = Integer.decode( langStr );
                    String np = data.getQueryParameter( TOTPLAYERS_KEY );
                    nPlayersT = Integer.decode( np );
                    String nh = data.getQueryParameter( HEREPLAYERS_KEY );
                    nPlayersH = nh == null ? 1 : Integer.decode( nh );
                    val = data.getQueryParameter( GID_KEY );
                    gameID = null == val ? 0 : Integer.decode( val );
                    val = data.getQueryParameter( FORCECHANNEL_KEY );
                    forceChannel = null == val ? 0 : Integer.decode( val );
                    gameName = data.getQueryParameter( NAME_KEY );
                }
                calcValid();
            } catch ( Exception e ) {
                Log.e( TAG, "unable to parse \"%s\"", data.toString() );
            }
        }
        calcValid();
    }

    private NetLaunchInfo( int gamID, String gamNam, int dictLang,
                           String dictName, int nPlayers )
    {
        this();
        gameName = gamNam;
        dict = dictName;
        lang = dictLang;
        nPlayersT = nPlayers;
        nPlayersH = 1;
        gameID = gamID;
    }

    public NetLaunchInfo( Context context, GameSummary summary, CurGameInfo gi,
                          int numHere, int fc )
    {
        this( context, summary, gi );
        nPlayersH = numHere;
        forceChannel = fc;
    }

    public NetLaunchInfo( CurGameInfo gi )
    {
        this( gi.gameID, gi.getName(), gi.dictLang, gi.dictName, gi.nPlayers );
    }

    public NetLaunchInfo( Context context, GameSummary summary, CurGameInfo gi )
    {
        this( gi );

        for ( CommsConnType typ : summary.conTypes.getTypes() ) {
            // DbgUtils.logf( "NetLaunchInfo(): got type %s", typ.toString() );
            switch( typ ) {
            case COMMS_CONN_BT:
                addBTInfo();
                break;
            case COMMS_CONN_RELAY:
                addRelayInfo( summary.roomName, summary.relayID );
                break;
            case COMMS_CONN_SMS:
                addSMSInfo( context );
                break;
            case COMMS_CONN_P2P:
                addP2PInfo( context );
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

    public void removeAddress( CommsConnType typ )
    {
        m_addrs.remove( typ );
    }

    public String inviteID()
    {
        String result = inviteID;
        if ( null == result ) {
            result = GameUtils.formatGameID( gameID );
            // DbgUtils.logf( "inviteID(): m_inviteID null so substituting %s", result );
        }
        return result;
    }

    public int gameID()
    {
        int result = gameID;
        if ( 0 == result ) {
            Assert.assertNotNull( inviteID );
            Log.i( TAG, "gameID(): looking at inviteID: %s", inviteID );
            result = Integer.parseInt( inviteID, 16 );
            // DbgUtils.logf( "gameID(): gameID -1 so substituting %d", result );
            gameID = result;
        }
        Assert.assertTrue( 0 != result );
        return result;
    }

    public void putSelf( Bundle bundle )
    {
        bundle.putString( MultiService.ROOM, room );
        bundle.putString( MultiService.INVITEID, inviteID );
        bundle.putInt( MultiService.LANG, lang );
        bundle.putString( MultiService.DICT, dict );
        bundle.putString( MultiService.GAMENAME, gameName );
        bundle.putInt( MultiService.NPLAYERST, nPlayersT );
        bundle.putInt( MultiService.NPLAYERSH, nPlayersH );
        bundle.putBoolean( MultiService.REMOTES_ROBOTS, remotesAreRobots );
        bundle.putInt( MultiService.GAMEID, gameID() );
        bundle.putString( MultiService.BT_NAME, btName );
        bundle.putString( MultiService.BT_ADDRESS, btAddress );
        bundle.putString( MultiService.P2P_MAC_ADDRESS, p2pMacAddress );
        bundle.putInt( MultiService.FORCECHANNEL, forceChannel );

        int flags = m_addrs.toInt();
        bundle.putInt( ADDRS_KEY, flags );
    }

    @Override
    public boolean equals( Object obj )
    {
        NetLaunchInfo other = null;
        boolean result = null != obj && obj instanceof NetLaunchInfo;
        if ( result ) {
            other = (NetLaunchInfo)obj;
            result = TextUtils.equals( gameName, other.gameName )
                && TextUtils.equals( dict, other.dict )
                && lang == other.lang
                && forceChannel == other.forceChannel
                && nPlayersT == other.nPlayersT
                && nPlayersH == other.nPlayersH
                && remotesAreRobots == other.remotesAreRobots
                && TextUtils.equals( room, other.room )
                && TextUtils.equals( btName, other.btName )
                && TextUtils.equals( btAddress, other.btAddress )
                && TextUtils.equals( p2pMacAddress, other.p2pMacAddress )
                && TextUtils.equals( phone, other.phone )
                && isGSM == other. isGSM
                && osVers == other.osVers
                && _conTypes == other._conTypes
                && gameID == other.gameID
                && ((null == m_addrs ? (null == other.m_addrs)
                     : m_addrs.equals(other.m_addrs)))
                && m_valid == other.m_valid
                && TextUtils.equals( inviteID, other.inviteID )
                ;
        }
        return result;
    }

    public String makeLaunchJSON()
    {
        String result = null;
        try {
            JSONObject obj = new JSONObject()
                .put( ADDRS_KEY, m_addrs.toInt() )
                .put( MultiService.LANG, lang )
                .put( MultiService.DICT, dict )
                .put( MultiService.GAMENAME, gameName )
                .put( MultiService.NPLAYERST, nPlayersT )
                .put( MultiService.NPLAYERSH, nPlayersH )
                .put( MultiService.REMOTES_ROBOTS, remotesAreRobots )
                .put( MultiService.GAMEID, gameID() )
                .put( MultiService.FORCECHANNEL, forceChannel );

            if ( m_addrs.contains( CommsConnType.COMMS_CONN_RELAY ) ) {
                obj.put( MultiService.ROOM, room )
                    .put( MultiService.INVITEID, inviteID );
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
            if ( m_addrs.contains( CommsConnType.COMMS_CONN_P2P ) ) {
                obj.put( P2P_MAC_KEY, p2pMacAddress );
            }
            result = obj.toString();

        } catch ( org.json.JSONException jse ) {
            Log.ex( TAG, jse );
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
            case COMMS_CONN_P2P:
                result.setP2PParams( p2pMacAddress );
                break;
            default:
                Assert.fail();
                break;
            }
        }

        return result;
    }

    private void init( Context context, String data ) throws JSONException
    {
        List<CommsConnType> supported = CommsConnTypeSet.getSupported( context );
        JSONObject json = new JSONObject( data );

        int flags = json.optInt(ADDRS_KEY, -1);
        boolean hasAddrs = -1 != flags;
        m_addrs = hasAddrs ?
            new CommsConnTypeSet( flags ) : new CommsConnTypeSet();

        lang = json.optInt( MultiService.LANG, -1 );
        forceChannel = json.optInt( MultiService.FORCECHANNEL, 0 );
        dict = json.optString( MultiService.DICT );
        gameName = json.optString( MultiService.GAMENAME );
        nPlayersT = json.optInt( MultiService.NPLAYERST, -1 );
        nPlayersH = json.optInt( MultiService.NPLAYERSH, 1 ); // absent ok
        remotesAreRobots = json.optBoolean( MultiService.REMOTES_ROBOTS, false );
        gameID = json.optInt( MultiService.GAMEID, 0 );

        // Try each type
        for ( CommsConnType typ : supported ) {
            if ( hasAddrs && !m_addrs.contains( typ ) ) {
                continue;
            }
            boolean doAdd;
            switch ( typ ) {
            case COMMS_CONN_BT:
                btAddress = json.optString( MultiService.BT_ADDRESS );
                btName = json.optString( MultiService.BT_NAME );
                doAdd = !hasAddrs && !btAddress.isEmpty();
                break;
            case COMMS_CONN_RELAY:
                room = json.getString( MultiService.ROOM );
                inviteID = json.optString( MultiService.INVITEID );
                doAdd = !hasAddrs && !room.isEmpty();
                break;
            case COMMS_CONN_SMS:
                phone = json.optString( PHONE_KEY );
                isGSM = json.optBoolean( GSM_KEY, false );
                osVers = json.optInt( OSVERS_KEY, 0 );
                doAdd = !hasAddrs && !phone.isEmpty();
                break;
            case COMMS_CONN_P2P:
                p2pMacAddress = json.optString( P2P_MAC_KEY );
                doAdd = !hasAddrs && null != p2pMacAddress;
                break;
            default:
                doAdd = false;
                Assert.fail();
            }
            if ( doAdd ) {
                m_addrs.add( typ );
            }
        }

        removeUnsupported( supported );

        calcValid();
    }

    private void appendInt( Uri.Builder ub, String key, int value )
    {
        ub.appendQueryParameter( key, String.format("%d", value ) );
    }

    public Uri makeLaunchUri( Context context )
    {
        int addrs = m_addrs.toInt();
        String host = LocUtils.getString( context, R.string.invite_host );
        host = NetUtils.forceHost( host );
        Uri.Builder ub = new Uri.Builder()
            .scheme( "http" )
            .path( String.format( "//%s%s", host,
                                  LocUtils.getString(context, R.string.invite_prefix) ) );
        appendInt( ub, LANG_KEY, lang );
        appendInt( ub, TOTPLAYERS_KEY, nPlayersT );
        appendInt( ub, HEREPLAYERS_KEY, nPlayersH );
        appendInt( ub, GID_KEY, gameID() );
        appendInt( ub, FORCECHANNEL_KEY, forceChannel );
        appendInt( ub, ADDRS_KEY, addrs );
        ub.appendQueryParameter( NAME_KEY, gameName );

        if ( null != dict ) {
            ub.appendQueryParameter( WORDLIST_KEY, dict );
        }

        if ( m_addrs.contains( CommsConnType.COMMS_CONN_RELAY ) ) {
            ub.appendQueryParameter( ROOM_KEY, room );
            ub.appendQueryParameter( ID_KEY, inviteID );
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
        if ( m_addrs.contains( CommsConnType.COMMS_CONN_P2P ) ) {
            ub.appendQueryParameter( P2P_MAC_KEY, p2pMacAddress );
        }
        Uri result = ub.build();

        if ( BuildConfig.DEBUG ) { // Test...
            Log.i( TAG, "testing %s...", result.toString() );
            NetLaunchInfo instance = new NetLaunchInfo( context, result );
            Assert.assertTrue( instance.isValid() );
        }

        return result;
    }

    public void addRelayInfo( String aRoom, String inviteID )
    {
        room = aRoom;
        inviteID = inviteID;
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
            Log.w( TAG, "addBTInfo(): no BT info available" );
        }
    }

    public void addSMSInfo( Context context )
    {
        NBSProto.SMSPhoneInfo pi = NBSProto.getPhoneInfo( context );
        if ( null != pi ) {
            phone = pi.number;
            isGSM = pi.isGSM;

            osVers = Integer.valueOf( android.os.Build.VERSION.SDK );

            m_addrs.add( CommsConnType.COMMS_CONN_SMS );
        }
    }

    public void addP2PInfo( Context context )
    {
        p2pMacAddress = WiDirService.getMyMacAddress( context );
        m_addrs.add( CommsConnType.COMMS_CONN_P2P );
    }

    public boolean isValid()
    {
        calcValid();            // this isn't always called. Likely should
                                // remove it as it's a stupid optimization
        // Log.d( TAG, "NetLaunchInfo(%s).isValid() => %b", this, m_valid );
        return m_valid;
    }

    public NetLaunchInfo setRemotesAreRobots( boolean val )
    {
        Assert.assertTrue( val == false || BuildConfig.DEBUG );
        remotesAreRobots = val;
        return this;
    }

    @Override
    public String toString()
    {
        return makeLaunchJSON();
    }

    public byte[] asByteArray()
    {
        byte[] result = null;
        try {
            ByteArrayOutputStream bas = new ByteArrayOutputStream();
            DataOutputStream das = new DataOutputStream( bas );
            das.writeUTF( makeLaunchJSON() );
            result = bas.toByteArray();
        } catch ( java.io.IOException ex ) {
            Assert.assertFalse( BuildConfig.DEBUG );
        }
        return result;
    }

    public static void putExtras( Intent intent, int gameID, String btAddr )
    {
        Assert.fail();
    }

    public void freezeAddrs()
    {
        _conTypes = m_addrs.toInt();
    }

    public void unfreezeAddrs()
    {
        m_addrs = new CommsConnTypeSet( _conTypes, true );
    }

    private boolean hasCommon()
    {
        return null != dict
            && 0 < lang
            && 0 < nPlayersT
            && 0 != gameID();
    }

    private void removeUnsupported( List<CommsConnType> supported )
    {
        for ( Iterator<CommsConnType> iter = m_addrs.iterator();
              iter.hasNext(); ) {
            if ( !supported.contains( iter.next() ) ) {
                iter.remove();
            }
        }
    }

    private void calcValid()
    {
        boolean valid = hasCommon() && null != m_addrs;
        // DbgUtils.logf( "calcValid(%s)", toString() );
        if ( valid ) {
            for ( Iterator<CommsConnType> iter = m_addrs.iterator();
                  valid && iter.hasNext(); ) {
                CommsConnType typ = iter.next();
                switch ( typ ) {
                case COMMS_CONN_RELAY:
                    valid = null != room && null != inviteID();
                    break;
                case COMMS_CONN_BT:
                    valid = null != btAddress;
                    break;
                case COMMS_CONN_SMS:
                    valid = null != phone && 0 < osVers;
                    break;
                }
            }
        }
        m_valid = valid;

        Utils.testSerialization( this );
    }
}
