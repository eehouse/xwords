/* -*- compile-command: "cd ../../../../../../; ant install"; -*- */
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

package org.eehouse.android.xw4.jni;

import android.content.Context;
import junit.framework.Assert;
import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.Utils;


/** Info we want to access when the game's closed that's not available
 * in CurGameInfo
 */
public class GameSummary {

    public static final int MSG_FLAGS_NONE = 0;
    public static final int MSG_FLAGS_TURN = 1;
    public static final int MSG_FLAGS_CHAT = 2;
    public static final int MSG_FLAGS_GAMEOVER = 4;
    public static final int MSG_FLAGS_ALL = 7;

    public int nMoves;
    public int turn;
    public int nPlayers;
    public int missingPlayers;
    public int[] scores;
    public boolean gameOver;
    public String[] players;
    public CommsAddrRec.CommsConnType conType;
    public String smsPhone;
    // relay-related fields
    public String roomName;
    public String relayID;
    public int seed;
    public int pendingMsgLevel;
    public long modtime;

    public int dictLang;
    public CurGameInfo.DeviceRole serverRole;

    private int m_giFlags;
    private String m_playersSummary;
    private CurGameInfo m_gi;

    public GameSummary(){
        pendingMsgLevel = 0;
    }

    public GameSummary( CurGameInfo gi )
    {
        this();
        nPlayers = gi.nPlayers;
        dictLang = gi.dictLang;
        serverRole = gi.serverRole;
        m_gi = gi;
    }

    public boolean inNetworkGame()
    {
        return null != relayID;
    }

    public String summarizePlayers( Context context )
    {
        String result;
        if ( null == m_gi ) {
            result = m_playersSummary;
        } else {
            StringBuffer sb = new StringBuffer();
            for ( int ii = 0; ; ) {

                int score = 0;
                try {
                    // scores can be null, but I've seen array OOB too.
                    score = scores[ii];
                } catch ( Exception ex ){}

                sb.append( m_gi.players[ii].name );
                if ( ++ii >= nPlayers ) {
                    break;
                }
                sb.append( "\n" );
            }
            result = sb.toString();
            m_playersSummary = result;
        }
        return result;
    }

    public void setPlayerSummary( String summary ) 
    {
        m_playersSummary = summary;
    }

    public String summarizeState( Context context )
    {
        String result = null;
        if ( gameOver ) {
            result = context.getString( R.string.gameOver );
        } else {
            result = String.format( context.getString(R.string.movesf),
                                    nMoves );
        }
        return result;
    }

    public String summarizeRole( Context context )
    {
        String result = null;
        if ( null != conType 
             && serverRole != CurGameInfo.DeviceRole.SERVER_STANDALONE ) {
            Assert.assertTrue( CommsAddrRec.CommsConnType.COMMS_CONN_RELAY
                               == conType );
            String fmt = context.getString( R.string.summary_fmt_relay );
            result = String.format( fmt, roomName );
        }
        return result;
    }

    private boolean isLocal( int indx ) {
        int flag = 2 << (indx * 2);
        return 0 == (m_giFlags & flag);
    }

    private boolean isRobot( int indx ) {
        int flag = 1 << (indx * 2);
        boolean result = 0 != (m_giFlags & flag);
        return result;
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
        m_giFlags = flags;
    }

    public String summarizePlayer( Context context, int indx ) 
    {
        String player = players[indx];
        int formatID = 0;
        if ( !isLocal(indx) ) {
            boolean isMissing = 0 != ((1 << indx) & missingPlayers);
            if ( isMissing ) {
                player = context.getString( R.string.missing_player );
            } else {
                formatID = R.string.str_nonlocal_namef;
            }
        } else if ( isRobot(indx) ) {
            formatID = R.string.robot_namef;
        }

        if ( 0 != formatID ) {
            String format = context.getString( formatID );
            player = String.format( format, player );
        }
        return player;
    }

    public boolean isNextToPlay( int indx ) {
        return indx == turn && isLocal(indx);
    }

}
