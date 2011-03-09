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
    public int giFlags;
    public int nPlayers;
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
    public String dictName;
    public CurGameInfo.DeviceRole serverRole;


    private CurGameInfo m_gi;

    public GameSummary(){
        pendingMsgLevel = 0;
    }

    public GameSummary( CurGameInfo gi )
    {
        super();
        nPlayers = gi.nPlayers;
        dictLang = gi.dictLang;
        dictName = gi.dictName;
        serverRole = gi.serverRole;
        m_gi = gi;
    }

    public boolean inNetworkGame()
    {
        return null != relayID;
    }

    public String summarizePlayers( Context context )
    {
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
        return sb.toString();
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
        return 0 == (giFlags & flag);
    }

    private boolean isRobot( int indx ) {
        int flag = 1 << (indx * 2);
        boolean result = 0 != (giFlags & flag);
        return result;
    }

    public int giflags() {
        Assert.assertNotNull( m_gi );
        int result = 0;
        for ( int ii = 0; ii < m_gi.nPlayers; ++ii ) {
            if ( ! m_gi.players[ii].isLocal ) {
                result |= 2 << (ii * 2);
            }
            if ( m_gi.players[ii].isRobot() ) {
                result |= 1 << (ii * 2);
            }
        }
        return result;
    }

    public String summarizePlayer( Context context, int indx ) {
        String player = players[indx];
        if ( !isLocal(indx) ) {
            player = 
                String.format( context.getString( R.string.str_nonlocal_name),
                               player );
        } else if ( isRobot(indx) ) {
            String robot = context.getString( R.string.robot_name );
            player += robot;
        }
        return player;
    }

    public boolean isNextToPlay( int indx ) {
        return indx == turn && isLocal(indx);
    }

}
