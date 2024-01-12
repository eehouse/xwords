/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2012 by Eric House (xwords@eehouse.org).  All
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

package org.eehouse.android.xw4.jni;

import android.content.Context;
import android.text.TextUtils;

import java.io.Serializable;
import java.util.Arrays;

import org.json.JSONObject;

import org.eehouse.android.xw4.Assert;
import org.eehouse.android.xw4.BuildConfig;
import org.eehouse.android.xw4.DBUtils;
import org.eehouse.android.xw4.DbgUtils;
import org.eehouse.android.xw4.Log;
import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.Utils.ISOCode;
import org.eehouse.android.xw4.Utils;
import org.eehouse.android.xw4.XWApp;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;
import org.eehouse.android.xw4.jni.CurGameInfo;
import org.eehouse.android.xw4.loc.LocUtils;

/** Info we want to access when the game's closed that's not available
 * in CurGameInfo
 *
 * I assume it's Serializable so it can be passed as a parameter.
 */
public class GameSummary implements Serializable {
    private static final String TAG = GameSummary.class.getSimpleName();

    public static final int MSG_FLAGS_NONE = 0;
    public static final int MSG_FLAGS_TURN = 1;
    public static final int MSG_FLAGS_CHAT = 2;
    public static final int MSG_FLAGS_GAMEOVER = 4;
    public static final int MSG_FLAGS_ALL = 7;
    public static final int DUP_MODE_MASK = 1 << (CurGameInfo.MAX_NUM_PLAYERS * 2);
    public static final int FORCE_CHANNEL_OFFSET = (CurGameInfo.MAX_NUM_PLAYERS * 2) + 1;
    public static final int FORCE_CHANNEL_MASK = 0x03;

    public int lastMoveTime;  // set by jni's server.c on move receipt
    public int dupTimerExpires;
    public int nMoves;
    public int turn;
    public boolean turnIsLocal;
    public int nPlayers;
    public int missingPlayers;
    public int[] scores;
    public boolean gameOver;
    public boolean quashed;
    private String[] m_players;
    public CommsConnTypeSet conTypes;
    // relay-related fields
    public String roomName;     // PENDING remove me
    public String relayID;
    public int seed;
    public long modtime;
    public long created;
    public int gameID;
    public String[] remoteDevs; // BTAddrs and phone numbers

    public ISOCode isoCode;
    public DeviceRole serverRole;
    public int nPacketsPending;
    public boolean canRematch;

    private Integer m_giFlags;
    private String m_playersSummary;
    private CurGameInfo m_gi;
    private String[] m_remotePhones;
    private String m_extras;

    public GameSummary() {}

    public GameSummary( CurGameInfo gi )
    {
        nPlayers = gi.nPlayers;
        isoCode = gi.isoCode();
        serverRole = gi.serverRole;
        gameID = gi.gameID;
        m_gi = gi;
    }

    public boolean inRelayGame()
    {
        return null != relayID;
    }

    @Override
    public boolean equals( Object obj )
    {
        boolean result;
        if ( BuildConfig.DEBUG ) {
            result = null != obj && obj instanceof GameSummary;
            if ( result ) {
                GameSummary other = (GameSummary)obj;
                result = lastMoveTime == other.lastMoveTime
                    && nMoves == other.nMoves
                    && dupTimerExpires == other.dupTimerExpires
                    && turn == other.turn
                    && turnIsLocal == other.turnIsLocal
                    && nPlayers == other.nPlayers
                    && missingPlayers == other.missingPlayers
                    && gameOver == other.gameOver
                    && quashed == other.quashed
                    && seed == other.seed
                    && modtime == other.modtime
                    && created == other.created
                    && gameID == other.gameID
                    && ISOCode.safeEquals( isoCode, other.isoCode )
                    && nPacketsPending == other.nPacketsPending
                    && Arrays.equals( scores, other.scores )
                    && Arrays.equals( m_players, other.m_players )
                    && ((null == conTypes) ? (null == other.conTypes)
                        : conTypes.equals(other.conTypes))
                    // && TextUtils.equals( roomName, other.roomName )
                    && TextUtils.equals( relayID, other.relayID )
                    && Arrays.equals( remoteDevs, other.remoteDevs )
                    && ((null == serverRole) ? (null == other.serverRole)
                        : serverRole.equals(other.serverRole))
                    && ((null == m_giFlags) ? (null == other.m_giFlags)
                        : m_giFlags.equals(other.m_giFlags))
                    && TextUtils.equals( m_playersSummary, other.m_playersSummary )
                    && ((null == m_gi) ? (null == other.m_gi)
                        : m_gi.equals(other.m_gi))
                    && Arrays.equals( m_remotePhones, other.m_remotePhones )
                    && TextUtils.equals( m_extras, other.m_extras )
                    ;
            }
        } else {
            result = super.equals( obj );
        }
        return result;
    }

    public String summarizePlayers()
    {
        String result;
        if ( null == m_gi ) {
            result = m_playersSummary;
        } else {
            String[] names = new String[nPlayers];
            for ( int ii = 0; ii < nPlayers; ++ii ) {
                names[ii] = m_gi.players[ii].name;
            }
            result = TextUtils.join( "\n", names );
            m_playersSummary = result;
        }
        return result;
    }

    public String summarizeDevs()
    {
        String result = null;
        if ( null != remoteDevs ) {
            result = TextUtils.join( "\n", remoteDevs );
        }
        return result;
    }

    public String getRematchName( Context context )
    {
        return LocUtils.getString( context, R.string.rematch_name_fmt,
                                   playerNames( context ) );
    }

    public void setRemoteDevs( Context context, CommsConnType typ, String str )
    {
        if ( null != str && 0 < str.length() ) {
            remoteDevs = TextUtils.split( str, "\n" );

            m_remotePhones = new String[remoteDevs.length];
            for ( int ii = 0; ii < remoteDevs.length; ++ii ) {
                m_remotePhones[ii] = (typ == CommsConnType.COMMS_CONN_SMS)
                    ? Utils.phoneToContact( context, remoteDevs[ii], true )
                    : remoteDevs[ii];
            }
        }
    }

    public void readPlayers( Context context, String playersStr )
    {
        if ( null != playersStr ) {
            m_players = new String[nPlayers];
            String sep;
            if ( playersStr.contains("\n") ) {
                sep = "\n";
            } else {
                sep = LocUtils.getString( context, R.string.vs_join );
            }

            int ii, nxt;
            for ( ii = 0, nxt = 0; ; ++ii ) {
                int prev = nxt;
                nxt = playersStr.indexOf( sep, nxt );
                String name = -1 == nxt ?
                    playersStr.substring( prev ) :
                    playersStr.substring( prev, nxt );
                m_players[ii] = name;
                if ( -1 == nxt ) {
                    break;
                }
                nxt += sep.length();
            }
        }
    }

    public void setPlayerSummary( String summary )
    {
        m_playersSummary = summary;
    }

    public String summarizeState( Context context )
    {
        String result = null;
        if ( gameOver ) {
            result = LocUtils.getString( context, R.string.gameOver );
        } else {
            result = LocUtils.getQuantityString( context, R.plurals.moves_fmt,
                                                 nMoves, nMoves );
        }
        return result;
    }

    // FIXME: should report based on whatever conType is giving us a
    // successful connection.
    public String summarizeRole( Context context, long rowid )
    {
        String result = null;
        if ( isMultiGame() ) {
            int fmtID = 0;

            int missing = countMissing();
            if ( 0 < missing ) {
                DBUtils.SentInvitesInfo si = DBUtils.getInvitesFor( context,
                                                                    rowid );
                if ( si.getMinPlayerCount() >= missing ) {
                    result = ( null != roomName )
                        ? LocUtils.getString( context,
                                              R.string.summary_invites_out_fmt,
                                              roomName )
                        : LocUtils.getString( context,
                                              R.string.summary_invites_out );
                }
            }

            // Otherwise, use BT or SMS
            if ( null == result ) {
                if ( conTypes.contains( CommsConnType.COMMS_CONN_BT )
                     || conTypes.contains( CommsConnType.COMMS_CONN_SMS)
                     || conTypes.contains( CommsConnType.COMMS_CONN_MQTT )) {
                    if ( 0 < missing ) {
                        if ( DeviceRole.SERVER_ISSERVER == serverRole ) {
                            fmtID = R.string.summary_wait_host;
                        } else {
                            fmtID = R.string.summary_wait_guest;
                        }
                    } else if ( gameOver ) {
                        fmtID = R.string.summary_gameover;
                    } else if ( quashed ) {
                        fmtID = R.string.summary_game_gone;
                    } else if ( null != remoteDevs
                                && conTypes.contains( CommsConnType.COMMS_CONN_SMS)){
                        result =
                            LocUtils.getString( context, R.string.summary_conn_sms_fmt,
                                                TextUtils.join(", ", m_remotePhones) );
                    } else {
                        fmtID = R.string.summary_conn;
                    }
                    if ( null == result ) {
                        result = LocUtils.getString( context, fmtID );
                    }
                }
            }
        }
        return result;
    }

    public boolean relayConnectPending()
    {
        boolean result = conTypes.contains( CommsConnType.COMMS_CONN_RELAY )
            && (null == relayID || 0 == relayID.length());
        if ( result ) {
            // Don't report it as unconnected if a game's happening
            // anyway, e.g. via BT.
            result = 0 > turn && !gameOver;
        }
        // DbgUtils.logf( "relayConnectPending()=>%b (turn=%d)", result,
        //                turn );
        return result;
    }

    public boolean isMultiGame()
    {
        return ( serverRole != DeviceRole.SERVER_STANDALONE );
    }

    private boolean isLocal( int indx )
    {
        return localTurnNextImpl( m_giFlags, indx );
    }

    private boolean isRobot( int indx ) {
        int flag = 1 << (indx * 2);
        boolean result = 0 != (m_giFlags & flag);
        return result;
    }

    private int countMissing()
    {
        int result = 0;
        for ( int ii = 0; ii < nPlayers; ++ii ) {
            if ( !isLocal(ii) && (0 != ((1 << ii) & missingPlayers) ) ) {
                ++result;
            }
        }
        return result;
    }

    public boolean anyMissing()
    {
        return 0 < countMissing();
    }

    public int giflags() {
        int result;
        if ( null == m_gi ) {
            result = m_giFlags;
        } else {
            result = 0;
            for ( int ii = 0; ii < m_gi.nPlayers; ++ii ) {
                if ( ! m_gi.players[ii].isLocal ) {
                    result |= 2 << (ii * 2);
                }
                if ( m_gi.players[ii].isRobot() ) {
                    result |= 1 << (ii * 2);
                }
            }

            Assert.assertTrue( (result & DUP_MODE_MASK) == 0 );
            if ( m_gi.inDuplicateMode ) {
                result |= DUP_MODE_MASK;
            }

            Assert.assertTrue( (result & (FORCE_CHANNEL_MASK<<FORCE_CHANNEL_OFFSET)) == 0 );
            // Make sure it's big enough
            Assert.assertTrue( 0 == (~FORCE_CHANNEL_MASK & m_gi.forceChannel) );
            result |= m_gi.forceChannel << FORCE_CHANNEL_OFFSET;
            // Log.d( TAG, "giflags(): adding forceChannel %d", m_gi.forceChannel );
        }
        return result;
    }

    public boolean inDuplicateMode()
    {
        int flags = giflags();
        return (flags & DUP_MODE_MASK) != 0;
    }

    public void setGiFlags( int flags )
    {
        m_giFlags = new Integer( flags );
    }

    public int getChannel()
    {
        int flags = giflags();
        int channel = (flags >> FORCE_CHANNEL_OFFSET) & FORCE_CHANNEL_MASK;
        // Log.d( TAG, "getChannel(id: %X) => %d", gameID, channel );
        return channel;
    }

    public String summarizePlayer( Context context, long rowid, int indx )
    {
        String player = m_players[indx];
        int formatID = 0;
        if ( !isLocal(indx) ) {
            boolean isMissing = 0 != ((1 << indx) & missingPlayers);
            if ( isMissing ) {
                DBUtils.SentInvitesInfo si = DBUtils.getInvitesFor( context, rowid );
                String kp = null;
                if ( null != si ) {
                    kp = si.getKPName( context );
                }
                if ( null == kp ) {
                    player = LocUtils.getString( context, R.string.missing_player );
                } else {
                    player = LocUtils.getString( context, R.string.invitee_fmt, kp );
                }
            } else {
                formatID = R.string.str_nonlocal_name_fmt;
            }
        } else if ( isRobot(indx) ) {
            formatID = R.string.robot_name_fmt;
        }

        if ( 0 != formatID ) {
            player = LocUtils.getString( context, formatID, player );
        }
        return player;
    }

    public String playerNames( Context context )
    {
        String[] names = null;
        if ( null != m_gi ) {
            names = m_gi.visibleNames( context, false );
        } else if ( null != m_playersSummary ) {
            names = TextUtils.split( m_playersSummary, "\n" );
        }

        String result = null;
        if ( null != names && 0 < names.length ) {
            String joiner = LocUtils.getString( context, R.string.vs_join );
            result = TextUtils.join( joiner, names );
        }

        return result;
    }

    public boolean isNextToPlay( int indx, boolean[] isLocal )
    {
        boolean isNext = indx == turn;
        if ( isNext ) {
            isLocal[0] = isLocal(indx);
        }
        return isNext;
    }

    public boolean nextTurnIsLocal()
    {
        boolean result = false;
        if ( !gameOver && 0 <= turn ) {
            Assert.assertTrue( null != m_gi || null != m_giFlags );
            result = localTurnNextImpl( giflags(), turn );
        }
        return result;
    }

    public String getPrevPlayer()
    {
        int prevTurn = (turn + nPlayers - 1) % nPlayers;
        return m_players[prevTurn];
    }

    public String dictNames( String separator )
    {
        String list = null;
        if ( null != m_gi ) {
            String[] names = m_gi.dictNames();
            list = TextUtils.join( separator, names );
        }
        return String.format( "%s%s%s", separator, list, separator );
    }

    public String getExtras()
    {
        return m_extras;
    }

    public void setExtras( String data )
    {
        m_extras = data;
    }

    public GameSummary putStringExtra( String key, String value )
    {
        if ( null != value ) {
            String extras = (null == m_extras) ? "{}" : m_extras;
            try {
                JSONObject asObj = new JSONObject( extras );
                if ( null == value ) {
                    asObj.remove( key );
                } else {
                    asObj.put( key, value );
                }
                m_extras = asObj.toString();
            } catch( org.json.JSONException ex ) {
                Log.ex( TAG, ex );
            }
            Log.i( TAG, "putStringExtra(%s,%s) => %s", key, value, m_extras );
        }
        return this;
    }

    public String getStringExtra( String key )
    {
        String result = null;
        if ( null != m_extras ) {
            try {
                JSONObject asObj = new JSONObject( m_extras );
                result = asObj.optString( key );
                if ( 0 == result.length() ) {
                    result = null;
                }
            } catch( org.json.JSONException ex ) {
                Log.ex( TAG, ex );
            }
        }
        // Log.i( TAG, "getStringExtra(%s) => %s", key, result );
        return result;
    }

    private static boolean localTurnNextImpl( int flags, int turn )
    {
        int flag = 2 << (turn * 2);
        return 0 == (flags & flag);
    }

    public static Boolean localTurnNext( int flags, int turn )
    {
        Boolean result = null;
        if ( 0 <= turn ) {
            result = new Boolean( localTurnNextImpl( flags, turn ) );
        }
        return result;
    }

    @Override
    public String toString()
    {
        String result;
        if ( BuildConfig.NON_RELEASE ) {
            StringBuffer sb = new StringBuffer("{")
                .append("nPlayers: ").append(nPlayers).append(',')
                .append("}");
            result = sb.toString();
        } else {
            result = super.toString();
        }
        return result;
    }
}
