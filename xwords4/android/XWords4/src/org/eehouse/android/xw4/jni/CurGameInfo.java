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

import java.util.Random;
import android.content.Context;
import junit.framework.Assert;

import org.eehouse.android.xw4.Utils;
import org.eehouse.android.xw4.GameUtils;
import org.eehouse.android.xw4.R;

public class CurGameInfo {

    public static final int MAX_NUM_PLAYERS = 4;

    public enum XWPhoniesChoice { PHONIES_IGNORE, PHONIES_WARN, PHONIES_DISALLOW };
    public enum DeviceRole { SERVER_STANDALONE, SERVER_ISSERVER, SERVER_ISCLIENT };

    public String dictName;
    public LocalPlayer[] players;
    public int gameID;
    public int gameSeconds;
    public int nPlayers;
    public int boardSize;
    public DeviceRole serverRole;

    public boolean hintsNotAllowed;
    public boolean timerEnabled;
    public boolean allowPickTiles;
    public boolean allowHintRect;
    public int robotSmartness;
    public XWPhoniesChoice phoniesAction;
    public boolean confirmBTConnect;   /* only used for BT */

    private int[] m_visiblePlayers;
    private int m_nVisiblePlayers;
    private boolean m_inProgress;

    public CurGameInfo( Context context ) {
        m_inProgress = false;
        nPlayers = 2;
        gameSeconds = 60 * CommonPrefs.getDefaultGameMinutes( context );
        boardSize = CommonPrefs.getDefaultBoardSize( context );
        players = new LocalPlayer[MAX_NUM_PLAYERS];
        serverRole = DeviceRole.SERVER_STANDALONE;
        dictName = CommonPrefs.getDefaultDict( context );
        hintsNotAllowed = false;
        phoniesAction = CommonPrefs.getDefaultPhonies( context );
        timerEnabled = CommonPrefs.getDefaultTimerEnabled( context );
        allowPickTiles = false;
        allowHintRect = false;
        robotSmartness = 1;

        // Always create MAX_NUM_PLAYERS so jni code doesn't ever have
        // to cons up a LocalPlayer instance.
        int ii;
        for ( ii = 0; ii < MAX_NUM_PLAYERS; ++ii ) {
            players[ii] = new LocalPlayer( context, ii );
        }

        figureVisible();
    }

    public CurGameInfo( CurGameInfo src )
    {
        m_inProgress = src.m_inProgress;
        gameID = src.gameID;
        nPlayers = src.nPlayers;
        gameSeconds = src.gameSeconds;
        boardSize = src.boardSize;
        players = new LocalPlayer[MAX_NUM_PLAYERS];
        serverRole = src.serverRole;
        dictName = src.dictName;
        hintsNotAllowed = src.hintsNotAllowed;
        phoniesAction = src.phoniesAction;
        timerEnabled = src.timerEnabled;
        allowPickTiles = src.allowPickTiles;
        allowHintRect = src.allowHintRect;
        robotSmartness = src.robotSmartness;
        
        int ii;
        for ( ii = 0; ii < MAX_NUM_PLAYERS; ++ii ) {
            players[ii] = new LocalPlayer( src.players[ii] );
        }

        figureVisible();
    }

    public void setServerRole( DeviceRole newRole )
    {
        serverRole = newRole;
        figureVisible();
        if ( m_nVisiblePlayers == 0 ) { // must always be one visible player
            Assert.assertFalse( players[0].isLocal );
            players[0].isLocal = true;
            figureVisible();
        }
    }

    public void setInProgress( boolean inProgress )
    {
        m_inProgress = inProgress;
        figureVisible();
    }

    /** return true if any of the changes made would invalide a game
     * in progress, i.e. require that it be restarted with the new
     * params.  E.g. changing a player to a robot is harmless for a
     * local-only game but illegal for a connected one.
     */
    public boolean changesMatter( final CurGameInfo other )
    {
        boolean matter = nPlayers != other.nPlayers
            || serverRole != other.serverRole
            || !dictName.equals( other.dictName )
            || boardSize != other.boardSize
            || hintsNotAllowed != other.hintsNotAllowed
            || allowPickTiles != other.allowPickTiles
            || phoniesAction != other.phoniesAction;

        if ( !matter && DeviceRole.SERVER_STANDALONE != serverRole ) {
            for ( int ii = 0; ii < nPlayers; ++ii ) {
                LocalPlayer me = players[ii];
                LocalPlayer him = other.players[ii];
                matter = me.isRobot != him.isRobot
                    || me.isLocal != him.isLocal
                    || !me.name.equals( him.name );
                if ( matter ) {
                    break;
                }
            }
        }

        return matter;
    }

    public int remoteCount()
    {
        figureVisible();
        int count = 0;
        for ( int ii = 0; ii < m_nVisiblePlayers; ++ii ) {
            if ( !players[m_visiblePlayers[ii]].isLocal ) {
                ++count;
            }
        }
        return count;
    }

    /**
     * fixup: if we're pretending some players don't exist, move them
     * up and make externally (i.e. in the jni world) visible fields
     * consistent.
     */
    public void fixup()
    {
        if ( m_nVisiblePlayers < nPlayers ) {
            Assert.assertTrue( serverRole == DeviceRole.SERVER_ISCLIENT );
            
            for ( int ii = 0; ii < m_nVisiblePlayers; ++ii ) {
                Assert.assertTrue( m_visiblePlayers[ii] >= ii );
                if ( m_visiblePlayers[ii] != ii ) {
                    LocalPlayer tmp = players[ii];
                    players[ii] = players[m_visiblePlayers[ii]];
                    players[m_visiblePlayers[ii]] = tmp;
                    m_visiblePlayers[ii] = ii;
                }
            }

            nPlayers = m_nVisiblePlayers;
        }

        if ( !m_inProgress && serverRole != DeviceRole.SERVER_ISSERVER ) {
            for ( int ii = 0; ii < nPlayers; ++ii ) {
                players[ii].isLocal = true;
            }
        }
    }

    public String[] visibleNames( Context context )
    {
        String[] names = new String[m_nVisiblePlayers];
        for ( int ii = 0; ii < m_nVisiblePlayers; ++ii ) {
            LocalPlayer lp = players[m_visiblePlayers[ii]];
            if ( lp.isLocal || serverRole == DeviceRole.SERVER_STANDALONE ) {
                names[ii] = lp.name;
                if ( lp.isRobot ) {
                    names[ii] += context.getString( R.string.robot_name );
                }
            } else {
                names[ii] = context.getString( R.string.guest_name );
            }
        }
        return names;
    }

    public String summarizePlayers( Context context, GameSummary summary )
    {
        StringBuffer sb = new StringBuffer();
        String vsString = context.getString( R.string.vs );
        for ( int ii = 0; ; ) {

            int score = 0;
            try {
                // scores can be null, but I've seen array OOB too.
                score = summary.scores[ii];
            } catch ( Exception ex ){}

            sb.append( String.format( "%s(%d)", players[ii].name, score ) );
            if ( ++ii >= nPlayers ) {
                break;
            }
            sb.append( String.format( " %s ", vsString ) );
        }
        return sb.toString();
    }

    public String summarizeRole( Context context, GameSummary summary )
    {
        String result = null;
        if ( null != summary ) {
            DeviceRole role = serverRole;
            if ( role != DeviceRole.SERVER_STANDALONE ) {
                if ( null != summary.conType ) {
                    boolean isHost = role == DeviceRole.SERVER_ISSERVER;
                    boolean justListening = false;
                    int roleID = isHost ? R.string.role_host : R.string.role_guest;
                    String via;
                    int summaryID;
                    switch ( summary.conType ) {
                    case COMMS_CONN_RELAY:
                        via = summary.roomName;
                        summaryID = R.string.summary_fmt_relay;
                        break;
                    case COMMS_CONN_SMS:
                        via = summary.smsPhone;
                        summaryID = R.string.summary_fmt_sms;
                        justListening = isHost;
                        break;
                    default:
                        summaryID = 0;
                        via = null;
                        Assert.fail();
                    }
                    String fmt = context.getString( justListening?
                                                    R.string.summary_fmt_listening
                                                    : summaryID );
                    String roleStr = context.getString( roleID );
                    if ( justListening ) {
                        result = String.format( fmt, roleStr );
                    } else {
                        result = String.format( fmt, roleStr, via );
                    }
                }
            }
        }
        return result;
    }

    public String summarizeState( Context context, GameSummary summary )
    {
        String result = null;
        if ( summary.gameOver ) {
            result = context.getString( R.string.gameOver );
        } else {
            result = String.format( context.getString(R.string.movesf),
                                    summary.nMoves );
        }
        return result;
    }

    public String summarizeDict( Context context )
    {
        String label = context.getString( R.string.dictionary );
        return label + " " + dictName;
    }

    public boolean addPlayer() 
    {
        boolean added = false;
        // We can add either by adding a player, if nPlayers <
        // MAX_NUM_PLAYERS, or by making an unusable player usable.
        if ( nPlayers < MAX_NUM_PLAYERS ) {
            ++nPlayers;
            added = true;
        } else if ( serverRole == DeviceRole.SERVER_ISCLIENT ) {
            for ( int ii = 0; ii < players.length; ++ii ) {
                if ( !players[ii].isLocal ) {
                    players[ii].isLocal = true;
                    added = true;
                    break;
                }
            }
        }
        if ( added ) {
            figureVisible();
        }
        return added;
    }

    public boolean moveUp( int which )
    {
        boolean canMove = which > 0 && which < nPlayers;
        if ( canMove ) {
            LocalPlayer tmp = players[which-1];
            players[which-1] = players[which];
            players[which] = tmp;
        }
        return canMove;
    }

    public boolean moveDown( int which )
    {
        return moveUp( which + 1 );
    }

    public boolean delete( int which )
    {
        boolean canDelete = m_nVisiblePlayers > 1;
        if ( canDelete ) {
            which = m_visiblePlayers[which]; // translate
            LocalPlayer tmp = players[which];
            for ( int ii = which; ii < nPlayers - 1; ++ii ) {
                moveDown( ii );
            }
            --nPlayers;
            players[nPlayers] = tmp;
            figureVisible();
        }
        return canDelete;
    }

    public boolean juggle()
    {
        boolean canJuggle = m_nVisiblePlayers > 1;
        if ( canJuggle ) {
            // for each element, exchange with randomly chocsen from
            // range <= to self.
            Random rgen = new Random();

            for ( int ii = m_nVisiblePlayers - 1; ii > 0; --ii ) {
                // Contrary to docs, nextInt() comes back negative!
                int rand = Math.abs(rgen.nextInt()); 
                int indx = rand % (ii+1);
                if ( indx != ii ) {
                    LocalPlayer tmp = players[m_visiblePlayers[ii]];
                    players[m_visiblePlayers[ii]] 
                        = players[m_visiblePlayers[indx]];
                    players[m_visiblePlayers[indx]] = tmp;
                }
            }
        }
        return canJuggle;
    }

    private void figureVisible()
    {
        if ( null == m_visiblePlayers ) {
            m_visiblePlayers = new int[MAX_NUM_PLAYERS];
        }

        m_nVisiblePlayers = 0;
        for ( int ii = 0; ii < nPlayers; ++ii ) {
            if ( m_inProgress
                 || serverRole != DeviceRole.SERVER_ISCLIENT
                 || players[ii].isLocal ) {
                m_visiblePlayers[m_nVisiblePlayers++] = ii;
            }
        }
    }
}
