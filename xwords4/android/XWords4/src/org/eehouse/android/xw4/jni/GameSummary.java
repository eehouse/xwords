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
    public int nMoves;
    public int nPlayers;
    public int[] scores;
    public boolean gameOver;
    public String players;
    public CommsAddrRec.CommsConnType conType;
    public String smsPhone;
    // relay-related fields
    public String roomName;
    public String relayID;
    public int seed;
    public boolean msgsPending;

    public int dictLang;
    public String dictName;
    public CurGameInfo.DeviceRole serverRole;

    private CurGameInfo m_gi;

    public GameSummary(){}

    public GameSummary( CurGameInfo gi )
    {
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
        String vsString = context.getString( R.string.vs );
        for ( int ii = 0; ; ) {

            int score = 0;
            try {
                // scores can be null, but I've seen array OOB too.
                score = scores[ii];
            } catch ( Exception ex ){}

            sb.append( String.format( "%s(%d)", m_gi.players[ii].name, score ) );
            if ( ++ii >= nPlayers ) {
                break;
            }
            sb.append( String.format( " %s ", vsString ) );
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

}
