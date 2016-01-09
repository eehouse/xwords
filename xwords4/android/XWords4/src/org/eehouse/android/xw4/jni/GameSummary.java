/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
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
import java.util.Iterator;

import junit.framework.Assert;
import org.json.JSONObject;

import org.eehouse.android.xw4.DBUtils;
import org.eehouse.android.xw4.DbgUtils;
import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.Utils;
import org.eehouse.android.xw4.XWApp;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;
import org.eehouse.android.xw4.loc.LocUtils;

/** Info we want to access when the game's closed that's not available
 * in CurGameInfo
 */
public class GameSummary {
    public static final String EXTRA_REMATCH_BTADDR = "rm_btaddr";
    public static final String EXTRA_REMATCH_PHONE = "rm_phone";
    public static final String EXTRA_REMATCH_RELAY = "rm_relay";

    public static final int MSG_FLAGS_NONE = 0;
    public static final int MSG_FLAGS_TURN = 1;
    public static final int MSG_FLAGS_CHAT = 2;
    public static final int MSG_FLAGS_GAMEOVER = 4;
    public static final int MSG_FLAGS_ALL = 7;

    public int lastMoveTime;
    public int nMoves;
    public int turn;
    public int nPlayers;
    public int missingPlayers;
    public int[] scores;
    public boolean gameOver;
    private String[] m_players;
    public CommsConnTypeSet conTypes;
    // relay-related fields
    public String roomName;
    public String relayID;
    public int seed;
    public long modtime;
    public int gameID;
    public String[] remoteDevs; // BTAddrs and phone numbers

    public int dictLang;
    public DeviceRole serverRole;
    public int nPacketsPending;

    private Integer m_giFlags;
    private String m_playersSummary;
    private CurGameInfo m_gi;
    private Context m_context;
    private String[] m_remotePhones;
    private String m_extras;

    private GameSummary() {}

    public GameSummary( Context context ) {
        m_context = context;
        gameID = 0;
    }

    public GameSummary( Context context, CurGameInfo gi )
    {
        this( context );
        nPlayers = gi.nPlayers;
        dictLang = gi.dictLang;
        serverRole = gi.serverRole;
        gameID = gi.gameID;
        m_gi = gi;
    }

    public Context getContext()
    {
        Assert.assertNotNull( m_context );
        return m_context; 
    }

    public boolean inRelayGame()
    {
        return null != relayID;
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

    public String getRematchName()
    {
        return LocUtils.getString( m_context, R.string.rematch_name_fmt, 
                                   playerNames() );
    }

    public void setRemoteDevs( Context context, String asString )
    {
        if ( null != asString ) {
            remoteDevs = TextUtils.split( asString, "\n" );

            m_remotePhones = new String[remoteDevs.length];
            for ( int ii = 0; ii < remoteDevs.length; ++ii ) {
                m_remotePhones[ii] = 
                    Utils.phoneToContact( context, remoteDevs[ii], true );
            }
        }
    }

    public void readPlayers( String playersStr ) 
    {
        if ( null != playersStr ) {
            m_players = new String[nPlayers];
            String sep;
            if ( playersStr.contains("\n") ) {
                sep = "\n";
            } else {
                sep = LocUtils.getString( m_context, R.string.vs_join );
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

    public String summarizeState()
    {
        String result = null;
        if ( gameOver ) {
            result = LocUtils.getString( m_context, R.string.gameOver );
        } else {
            result = LocUtils.getQuantityString( m_context, R.plurals.moves_fmt,
                                                 nMoves, nMoves );
        }
        return result;
    }

    // FIXME: should report based on whatever conType is giving us a
    // successful connection.
    public String summarizeRole( long rowid )
    {
        String result = null;
        if ( isMultiGame() ) {
            int fmtID = 0;

            int missing = countMissing();
            if ( 0 < missing ) {
                DBUtils.SentInvitesInfo si = DBUtils.getInvitesFor( m_context,
                                                                    rowid );
                if ( si.getPlayerCount() >= missing ) {
                    result = LocUtils.getString( m_context,
                                                 R.string.summary_invites_out );
                }
            }
            
            // If we're using relay to connect, get status from that
            if ( null == result
                 && conTypes.contains( CommsConnType.COMMS_CONN_RELAY ) ) {
                if ( 0 < missing ) {
                    if ( null == relayID || 0 == relayID.length() ) {
                        fmtID = R.string.summary_relay_conf_fmt;
                    } else {
                        fmtID = R.string.summary_relay_wait_fmt;
                    }
                } else if ( gameOver ) {
                    fmtID = R.string.summary_relay_gameover_fmt;
                } else {
                    fmtID = R.string.summary_relay_conn_fmt;
                }
                result = LocUtils.getString( m_context, fmtID, roomName );
            }
                
            // Otherwise, use BT or SMS
            if ( null == result ) {
                if ( conTypes.contains( CommsConnType.COMMS_CONN_BT )
                     || ( conTypes.contains( CommsConnType.COMMS_CONN_SMS))){
                    if ( 0 < missing ) {
                        if ( DeviceRole.SERVER_ISSERVER == serverRole ) {
                            fmtID = R.string.summary_wait_host;
                        } else {
                            fmtID = R.string.summary_wait_guest;
                        }
                    } else if ( gameOver ) {
                        fmtID = R.string.summary_gameover;
                    } else if ( null != remoteDevs 
                                && conTypes.contains( CommsConnType.COMMS_CONN_SMS)){
                        result = 
                            LocUtils.getString( m_context, R.string.summary_conn_sms_fmt,
                                                TextUtils.join(", ", m_remotePhones) );
                    } else {
                        fmtID = R.string.summary_conn;
                    }
                    if ( null == result ) {
                        result = LocUtils.getString( m_context, fmtID );
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
        }
        return result;
    }

    public void setGiFlags( int flags ) 
    {
        m_giFlags = new Integer( flags );
    }

    public String summarizePlayer( int indx ) 
    {
        String player = m_players[indx];
        int formatID = 0;
        if ( !isLocal(indx) ) {
            boolean isMissing = 0 != ((1 << indx) & missingPlayers);
            if ( isMissing ) {
                player = LocUtils.getString( m_context, R.string.missing_player );
            } else {
                formatID = R.string.str_nonlocal_name_fmt;
            }
        } else if ( isRobot(indx) ) {
            formatID = R.string.robot_name_fmt;
        }

        if ( 0 != formatID ) {
            player = LocUtils.getString( m_context, formatID, player );
        }
        return player;
    }

    public String playerNames()
    {
        String[] names = null;
        if ( null != m_gi ) {
            names = m_gi.visibleNames( false );
        } else if ( null != m_playersSummary ) {
            names = TextUtils.split( m_playersSummary, "\n" );
        }

        String result = null;
        if ( null != names && 0 < names.length ) {
            String joiner = LocUtils.getString( m_context, R.string.vs_join );
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

    public void putStringExtra( String key, String value )
    {
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
            DbgUtils.loge( ex );
        }
        DbgUtils.logf( "putStringExtra(%s,%s) => %s", key, value, m_extras );
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
                DbgUtils.loge( ex );
            }
        }
        DbgUtils.logf( "getStringExtra(%s) => %s", key, result );
        return result;
    }

    public boolean hasRematchInfo()
    {
        boolean found = false;
        if ( XWApp.REMATCH_SUPPORTED ) {
            String[] keys = { EXTRA_REMATCH_BTADDR,
                              EXTRA_REMATCH_PHONE,
                              EXTRA_REMATCH_RELAY,
            };
            for ( String key : keys ) {
                found = null != getStringExtra( key );
                if ( found ) {
                    break;
                }
            }
            DbgUtils.logf( "hasRematchInfo() => %b", found );
        }
        return found;
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

}
