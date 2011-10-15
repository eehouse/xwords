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
import java.util.HashSet;
import java.util.Arrays;
import junit.framework.Assert;

import org.eehouse.android.xw4.Utils;
import org.eehouse.android.xw4.DictUtils;
import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.DictLangCache;

public class CurGameInfo {

    public static final int MAX_NUM_PLAYERS = 4;

    public enum XWPhoniesChoice { PHONIES_IGNORE, PHONIES_WARN, PHONIES_DISALLOW };
    public enum DeviceRole { SERVER_STANDALONE, SERVER_ISSERVER, SERVER_ISCLIENT };

    public String dictName;
    public LocalPlayer[] players;
    public int dictLang;
    public int gameID;
    public int gameSeconds;
    public int nPlayers;
    public int boardSize;
    public DeviceRole serverRole;

    public boolean hintsNotAllowed;
    public boolean timerEnabled;
    public boolean allowPickTiles;
    public boolean allowHintRect;
    public XWPhoniesChoice phoniesAction;
    public boolean confirmBTConnect;   /* only used for BT */

    // private int[] m_visiblePlayers;
    // private int m_nVisiblePlayers;
    private int m_smartness;
    private Context m_context;

    public CurGameInfo( Context context )
    {
        this( context, false );
    }

    public CurGameInfo( Context context, boolean isNetworked )
    {
        m_context = context;
        nPlayers = 2;
        gameSeconds = 60 * nPlayers *
            CommonPrefs.getDefaultPlayerMinutes( context );
        boardSize = CommonPrefs.getDefaultBoardSize( context );
        players = new LocalPlayer[MAX_NUM_PLAYERS];
        serverRole = isNetworked ? DeviceRole.SERVER_ISCLIENT
            : DeviceRole.SERVER_STANDALONE;
        hintsNotAllowed = !CommonPrefs.getDefaultHintsAllowed( context );
        phoniesAction = CommonPrefs.getDefaultPhonies( context );
        timerEnabled = CommonPrefs.getDefaultTimerEnabled( context );
        allowPickTiles = false;
        allowHintRect = false;
        m_smartness = 0;        // needs to be set from players

        // Always create MAX_NUM_PLAYERS so jni code doesn't ever have
        // to cons up a LocalPlayer instance.
        for ( int ii = 0; ii < MAX_NUM_PLAYERS; ++ii ) {
            players[ii] = new LocalPlayer( context, ii );
        }
        if ( isNetworked ) {
            players[1].isLocal = false;
        } else {
            players[0].setRobotSmartness( 1 );
        }

        // name the local humans now
        int count = 0;
        for ( int ii = 0; ii < nPlayers; ++ii ) {
            LocalPlayer lp = players[ii];
            if ( lp.isLocal && !lp.isRobot() ) {
                lp.name = CommonPrefs.getDefaultPlayerName( context, count++ );
            }
        }

        if ( CommonPrefs.getAutoJuggle( context ) ) {
            juggle();
        }

        setLang( 0 );
    }

    public CurGameInfo( Context context, CurGameInfo src )
    {
        m_context = context;
        gameID = src.gameID;
        nPlayers = src.nPlayers;
        gameSeconds = src.gameSeconds;
        boardSize = src.boardSize;
        players = new LocalPlayer[MAX_NUM_PLAYERS];
        serverRole = src.serverRole;
        dictName = src.dictName;
        dictLang = src.dictLang;
        hintsNotAllowed = src.hintsNotAllowed;
        phoniesAction = src.phoniesAction;
        timerEnabled = src.timerEnabled;
        allowPickTiles = src.allowPickTiles;
        allowHintRect = src.allowHintRect;
        
        int ii;
        for ( ii = 0; ii < MAX_NUM_PLAYERS; ++ii ) {
            players[ii] = new LocalPlayer( src.players[ii] );
        }
    }

    public void setServerRole( DeviceRole newRole )
    {
        serverRole = newRole;
        Assert.assertTrue( nPlayers > 0 );
        if ( nPlayers == 0 ) { // must always be one visible player
            Assert.assertFalse( players[0].isLocal );
            players[0].isLocal = true;
        }
    }

    public void setLang( int lang )
    {
        if ( 0 == lang ) {
            String dictName = CommonPrefs.getDefaultHumanDict( m_context );
            lang = DictLangCache.getDictLangCode( m_context, dictName );
        }
        if ( dictLang != lang ) {
            dictLang = lang;
            assignDicts();
        }
    }

    public int getRobotSmartness()
    {
        if ( m_smartness == 0 ) {
            m_smartness = 1;    // default if no robots
            for ( int ii = 0; ii < nPlayers; ++ii ) {
                if ( players[ii].isRobot() ) {
                    m_smartness = players[ii].robotIQ;
                    break;      // should all be the same
                }
            }
        }
        return m_smartness;
    }

    public void setRobotSmartness( int smartness )
    {
        m_smartness = smartness;
        for ( int ii = 0; ii < nPlayers; ++ii ) {
            if ( players[ii].isRobot() ) {
                players[ii].robotIQ = smartness;
            }
        }
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
            || dictLang != other.dictLang
            || boardSize != other.boardSize
            || hintsNotAllowed != other.hintsNotAllowed
            || allowPickTiles != other.allowPickTiles
            || phoniesAction != other.phoniesAction;

        if ( !matter && DeviceRole.SERVER_STANDALONE != serverRole ) {
            for ( int ii = 0; ii < nPlayers; ++ii ) {
                LocalPlayer me = players[ii];
                LocalPlayer him = other.players[ii];
                matter = me.isRobot() != him.isRobot()
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
        int count = 0;
        for ( int ii = 0; ii < nPlayers; ++ii ) {
            if ( !players[ii].isLocal ) {
                ++count;
            }
        }
        Utils.logf( "remoteCount()=>%d", count );
        return count;
    }

    public boolean forceRemoteConsistent()
    {
        boolean consistent = serverRole == DeviceRole.SERVER_STANDALONE;
        if ( !consistent ) {
            if ( remoteCount() == 0 ) {
                players[0].isLocal = false;
            } else if ( remoteCount() == nPlayers ) {
                players[0].isLocal = true;
            } else {
                consistent = true; // nothing changed
            }
        }
        return !consistent;
    }

    public String[] visibleNames( boolean withDicts )
    {
        String nameFmt = withDicts? m_context.getString( R.string.name_dict_fmt )
            : "%s";
        String[] names = new String[nPlayers];
        for ( int ii = 0; ii < nPlayers; ++ii ) {
            LocalPlayer lp = players[ii];
            if ( lp.isLocal || serverRole == DeviceRole.SERVER_STANDALONE ) {
                String name;
                if ( lp.isRobot() ) {
                    String format = m_context.getString( R.string.robot_namef );
                    name = String.format( format, lp.name );
                } else {
                    name = lp.name;
                }
                names[ii] = String.format( nameFmt, name, dictName(lp) );
            } else {
                names[ii] = m_context.getString( R.string.guest_name );
            }
        }
        return names;
    }

    public String[] dictNames()
    {
        String[] result = new String[nPlayers+1];
        result[0] = dictName;
        for ( int ii = 0; ii < nPlayers; ++ii ) {
            result[ii+1] = players[ii].dictName;
        }
        return result;
    }

    // Replace any dict that doesn't exist with newDict
    public void replaceDicts( String newDict ) 
    {
        String[] dicts = 
            DictLangCache.getHaveLang( m_context, dictLang );
        HashSet<String> installed = new HashSet<String>( Arrays.asList(dicts) );

        if ( !installed.contains( dictName ) ) {
            dictName = newDict;
        }

        for ( int ii = 0; ii < nPlayers; ++ii ) {
            LocalPlayer lp = players[ii];
            if ( null == lp.dictName ) {
                // continue to inherit
            } else if ( !installed.contains( players[ii].dictName ) ) {
                players[ii].dictName = newDict;
            }
        }
    }

    public String langName()
    {
        return DictLangCache.getLangName( m_context, dictLang );
    }

    public String dictName( final LocalPlayer lp )
    {
        String dname = lp.dictName;
        if ( null == dname ) {
            dname = dictName;
        }
        return dname;
    }

    public String dictName( int indx )
    {
        LocalPlayer lp = players[indx];
        return dictName( lp );
    }

    public boolean addPlayer() 
    {
        boolean added = nPlayers < MAX_NUM_PLAYERS;
        // We can add either by adding a player, if nPlayers <
        // MAX_NUM_PLAYERS, or by making an unusable player usable.
        if ( added ) {
            players[nPlayers].isLocal = 
                serverRole == DeviceRole.SERVER_STANDALONE;
            ++nPlayers;
        }
        return added;
    }

    public void setNPlayers( int nPlayersTotal, int nPlayersHere )
    {
        assert( nPlayersTotal < MAX_NUM_PLAYERS );
        assert( nPlayersHere < nPlayersTotal );

        nPlayers = nPlayersTotal;

        for ( int ii = 0; ii < nPlayersTotal; ++ii ) {
            players[ii].isLocal = ii < nPlayersHere;
            assert( !players[ii].isRobot() );
        }
    }

    public void setFirstLocalName( String name ) {
        for ( int ii = 0; ii < nPlayers; ++ii ) {
            if ( players[ii].isLocal ) {
                players[ii].name = name;
                break;
            }
        }
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
        boolean canDelete = nPlayers > 0;
        if ( canDelete ) {
            LocalPlayer tmp = players[which];
            for ( int ii = which; ii < nPlayers - 1; ++ii ) {
                moveDown( ii );
            }
            --nPlayers;
            players[nPlayers] = tmp;
        }
        return canDelete;
    }

    public boolean juggle()
    {
        boolean canJuggle = nPlayers > 1;
        if ( canJuggle ) {
            // for each element, exchange with randomly chocsen from
            // range <= to self.
            Random rgen = new Random();

            for ( int ii = nPlayers - 1; ii > 0; --ii ) {
                // Contrary to docs, nextInt() comes back negative!
                int rand = Math.abs(rgen.nextInt()); 
                int indx = rand % (ii+1);
                if ( indx != ii ) {
                    LocalPlayer tmp = players[ii];
                    players[ii] = players[indx];
                    players[indx] = tmp;
                }
            }
        }
        return canJuggle;
    }

    private void assignDicts()
    {
        // For each player's dict, if non-null and language matches
        // leave it alone.  Otherwise replace with default if that
        // matches langauge.  Otherwise pick an arbitrary dict in the
        // right language.

        String humanDict = 
            DictLangCache.getBestDefault( m_context, dictLang, true );
        String robotDict = 
            DictLangCache.getBestDefault( m_context, dictLang, false );

        if ( null == dictName 
             || ! DictUtils.dictExists( m_context, dictName ) 
             || dictLang != DictLangCache.getDictLangCode( m_context, 
                                                           dictName ) ) {
            dictName = humanDict;
        }

        for ( int ii = 0; ii < nPlayers; ++ii ) {
            LocalPlayer lp = players[ii];

            if ( null != lp.dictName &&
                 dictLang != DictLangCache.getDictLangCode( m_context, 
                                                            lp.dictName ) ) {
                lp.dictName = null;
            }

            if ( null == lp.dictName ) {
                if ( lp.isRobot() ) {
                    if ( robotDict != dictName ) {
                        lp.dictName = robotDict;
                    } else if ( humanDict != dictName ) {
                        lp.dictName = humanDict;
                    }
                }
            }
        }
    }

}
