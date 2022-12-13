/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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
import android.text.TextUtils;

import java.io.Serializable;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Random;
import org.json.JSONObject;

import org.eehouse.android.xw4.Assert;
import org.eehouse.android.xw4.BuildConfig;
import org.eehouse.android.xw4.DictLangCache;
import org.eehouse.android.xw4.DictUtils;
import org.eehouse.android.xw4.Log;
import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.Utils.ISOCode;
import org.eehouse.android.xw4.Utils;
import org.eehouse.android.xw4.XWApp;
import org.eehouse.android.xw4.XWPrefs;
import org.eehouse.android.xw4.loc.LocUtils;

public class CurGameInfo implements Serializable {
    private static final String TAG = CurGameInfo.class.getSimpleName();
    
    public static final int MAX_NUM_PLAYERS = 4;

    private static final String BOARD_SIZE = "BOARD_SIZE";
    private static final String TRAY_SIZE = "TRAY_SIZE";
    private static final String BINGO_MIN = "BINGO_MIN";
    private static final String NO_HINTS = "NO_HINTS";
    private static final String TIMER = "TIMER";
    private static final String ALLOW_PICK = "ALLOW_PICK";
    private static final String PHONIES = "PHONIES";
    private static final String DUP = "DUP";

    public enum XWPhoniesChoice { PHONIES_IGNORE, PHONIES_WARN, PHONIES_DISALLOW, PHONIES_BLOCK, };
    public enum DeviceRole { SERVER_STANDALONE, SERVER_ISSERVER, SERVER_ISCLIENT };

    public String dictName;
    public LocalPlayer[] players;
    public String isoCodeStr;    // public only for access from JNI; use isoCode() from java
    public int gameID;
    public int gameSeconds;
    public int nPlayers;
    public int boardSize;
    public int traySize;
    public int bingoMin;
    public int forceChannel;
    public DeviceRole serverRole;

    public boolean inDuplicateMode;
    public boolean hintsNotAllowed;
    public boolean timerEnabled;
    public boolean allowPickTiles;
    public boolean allowHintRect;
    public XWPhoniesChoice phoniesAction;

    // private int[] m_visiblePlayers;
    // private int m_nVisiblePlayers;
    private int m_smartness;
    private String m_name;      // not shared across the jni boundary

    public CurGameInfo( Context context )
    {
        this( context, (String)null );
    }

    public CurGameInfo( Context context, String inviteID )
    {
        boolean isNetworked = null != inviteID;
        nPlayers = 2;
        inDuplicateMode = CommonPrefs.getDefaultDupMode( context );
        gameSeconds = inDuplicateMode ? (5 * 60)
            : 60 * nPlayers * CommonPrefs.getDefaultPlayerMinutes( context );
        boardSize = CommonPrefs.getDefaultBoardSize( context );
        traySize = XWPrefs.getDefaultTraySize( context );
        bingoMin = XWApp.MIN_TRAY_TILES;
        players = new LocalPlayer[MAX_NUM_PLAYERS];
        serverRole = isNetworked ? DeviceRole.SERVER_ISCLIENT
            : DeviceRole.SERVER_STANDALONE;
        hintsNotAllowed = !CommonPrefs.getDefaultHintsAllowed( context,
                                                               isNetworked );
        phoniesAction = CommonPrefs.getDefaultPhonies( context );
        timerEnabled = CommonPrefs.getDefaultTimerEnabled( context );
        allowPickTiles = false;
        allowHintRect = false;
        m_smartness = 0;        // needs to be set from players

        try {
            gameID = (null == inviteID) ? 0 : Integer.parseInt( inviteID, 16 );
        } catch ( Exception ex ) {
        }

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
            if ( lp.isLocal ) {
                lp.name = lp.isRobot() ? CommonPrefs.getDefaultRobotName( context )
                    : CommonPrefs.getDefaultPlayerName( context, count++ );
            }
        }

        if ( CommonPrefs.getAutoJuggle( context ) ) {
            juggle();
        }

        setLang( context, null );
    }

    public CurGameInfo( CurGameInfo src )
    {
        m_name = src.m_name;
        gameID = src.gameID;
        nPlayers = src.nPlayers;
        gameSeconds = src.gameSeconds;
        boardSize = src.boardSize;
        traySize = src.traySize;
        bingoMin = src.bingoMin;
        players = new LocalPlayer[MAX_NUM_PLAYERS];
        serverRole = src.serverRole;
        dictName = src.dictName;
        isoCodeStr = src.isoCodeStr;
        hintsNotAllowed = src.hintsNotAllowed;
        inDuplicateMode = src.inDuplicateMode;
        phoniesAction = src.phoniesAction;
        timerEnabled = src.timerEnabled;
        allowPickTiles = src.allowPickTiles;
        allowHintRect = src.allowHintRect;

        int ii;
        for ( ii = 0; ii < MAX_NUM_PLAYERS; ++ii ) {
            players[ii] = new LocalPlayer( src.players[ii] );
        }

        Utils.testSerialization( this );
    }

    public ISOCode isoCode()
    {
        return ISOCode.newIf( isoCodeStr );
    }

    @Override
    public String toString()
    {
        String result = null;
        if ( BuildConfig.DEBUG ) {
            StringBuilder sb = new StringBuilder(TAG)
                .append( ": {nPlayers: ").append( nPlayers )
                .append(", players: [");
            for ( int ii = 0; ii < nPlayers; ++ii ) {
                sb.append( players[ii] )
                    .append( ", " );
            }
            sb.append( "], gameID: ").append( gameID )
                .append( ", role: ").append( serverRole )
                .append( ", hashCode: ").append( hashCode() )
                .append( ", timerEnabled: ").append( timerEnabled )
                .append( ", gameSeconds: ").append( gameSeconds )
                .append('}');

            result = sb.toString();
        } else {
            result = super.toString();
        }
        return result;
    }

    public String getJSONData()
    {
        String jsonData = null;
        try {
            JSONObject obj = new JSONObject()
                .put( BOARD_SIZE, boardSize )
                .put( TRAY_SIZE, traySize )
                .put( BINGO_MIN, bingoMin )
                .put( NO_HINTS, hintsNotAllowed )
                .put( DUP, inDuplicateMode )
                .put( TIMER, timerEnabled )
                .put( ALLOW_PICK, allowPickTiles )
                .put( PHONIES, phoniesAction.ordinal() )
                ;
            jsonData = obj.toString();
        } catch ( org.json.JSONException jse ) {
            Log.ex( TAG, jse );
        }

        return jsonData;
    }

    public void setFrom( String jsonData )
    {
        if ( null != jsonData ) {
            try {
                JSONObject obj = new JSONObject( jsonData );
                boardSize = obj.optInt( BOARD_SIZE, boardSize );
                traySize = obj.optInt( TRAY_SIZE, traySize );
                bingoMin = obj.optInt( BINGO_MIN, bingoMin );
                hintsNotAllowed = obj.optBoolean( NO_HINTS, hintsNotAllowed );
                inDuplicateMode = obj.optBoolean( DUP, inDuplicateMode );
                timerEnabled = obj.optBoolean( TIMER, timerEnabled );
                allowPickTiles = obj.optBoolean( ALLOW_PICK, allowPickTiles );
                int tmp = obj.optInt( PHONIES, phoniesAction.ordinal() );
                phoniesAction = XWPhoniesChoice.values()[tmp];
            } catch ( org.json.JSONException jse ) {
                Log.ex( TAG, jse );
            }
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

    public void setLang( Context context, ISOCode isoCode, String dict )
    {
        if ( null != dict ) {
            dictName = dict;
        }
        setLang( context, isoCode );
    }

    public void setLang( Context context, ISOCode isoCodeNew )
    {
        if ( null == isoCodeNew ) {
            String dictName = CommonPrefs.getDefaultHumanDict( context );
            isoCodeNew = DictLangCache.getDictISOCode( context, dictName );
        }
        Assert.assertTrueNR( null != isoCodeNew );

        if ( ! TextUtils.equals( isoCodeNew.toString(), this.isoCodeStr ) ) {
            isoCodeStr = isoCodeNew.toString();
            assignDicts( context );
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

    public CurGameInfo addDefaults( Context context, boolean standalone )
    {
        setLang( context, null );
        nPlayers = 2;
        players[0] = new LocalPlayer( context, 0 );
        players[1] = new LocalPlayer( context, 1 );
        if ( standalone ) {
            players[1].setIsRobot( true );
            players[1].name = CommonPrefs.getDefaultRobotName( context );
        } else {
            players[1].isLocal = false;
        }
        setServerRole( standalone ?
                       DeviceRole.SERVER_STANDALONE : DeviceRole.SERVER_ISSERVER );
        return this;
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
            || ! TextUtils.equals(isoCodeStr, other.isoCodeStr)
            || boardSize != other.boardSize
            || traySize != other.traySize
            || bingoMin != other.bingoMin
            || hintsNotAllowed != other.hintsNotAllowed
            || inDuplicateMode != other.inDuplicateMode
            || allowPickTiles != other.allowPickTiles
            || phoniesAction != other.phoniesAction;

        if ( !matter ) {
            matter = !dictName.equals( other.dictName );
            for ( int ii = 0; !matter && ii < nPlayers; ++ii ) {
                LocalPlayer me = players[ii];
                LocalPlayer him = other.players[ii];
                matter = me.isRobot() != him.isRobot()
                    || me.isLocal != him.isLocal
                    || !me.name.equals( him.name );
            }
        }

        return matter;
    }

    @Override
    public boolean equals( Object obj )
    {
        boolean result;
        if ( BuildConfig.DEBUG ) {
            CurGameInfo other = null;
            result = null != obj && obj instanceof CurGameInfo;
            if ( result ) {
                other = (CurGameInfo)obj;
                result = TextUtils.equals( isoCodeStr, other.isoCodeStr)
                    && gameID == other.gameID
                    && gameSeconds == other.gameSeconds
                    && nPlayers == other.nPlayers
                    && boardSize == other.boardSize
                    && traySize == other.traySize
                    && bingoMin == other.bingoMin
                    && forceChannel == other.forceChannel
                    && hintsNotAllowed == other.hintsNotAllowed
                    && inDuplicateMode == other.inDuplicateMode
                    && timerEnabled == other.timerEnabled
                    && allowPickTiles == other.allowPickTiles
                    && allowHintRect == other.allowHintRect
                    && m_smartness == other.m_smartness
                    && Arrays.deepEquals( players, other.players )
                    && TextUtils.equals( dictName, other.dictName )
                    && ((null == serverRole) ? (null == other.serverRole)
                        : serverRole.equals(other.serverRole))
                    && ((null == phoniesAction) ? (null == other.phoniesAction)
                        : phoniesAction.equals(other.phoniesAction))
                    && TextUtils.equals( m_name, other.m_name )
                    ;
            }
        } else {
            result = super.equals( obj );
        }
        return result;
    }

    public int remoteCount()
    {
        int count = 0;
        for ( int ii = 0; ii < nPlayers; ++ii ) {
            if ( !players[ii].isLocal ) {
                ++count;
            }
        }
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

    public String[] playerNames()
    {
        String[] names = new String[nPlayers];
        for ( int ii = 0; ii < nPlayers; ++ii ) {
            names[ii] = players[ii].name;
        }
        return names;
    }

    public boolean[] playersLocal()
    {
        boolean[] locs = new boolean[nPlayers];
        for ( int ii = 0; ii < nPlayers; ++ii ) {
            locs[ii] = players[ii].isLocal;
        }
        return locs;
    }

    public String[] visibleNames( Context context, boolean withDicts )
    {
        String nameFmt = withDicts?
            LocUtils.getString( context, R.string.name_dict_fmt )
            : "%s";
        String[] names = new String[nPlayers];
        for ( int ii = 0; ii < nPlayers; ++ii ) {
            LocalPlayer lp = players[ii];
            if ( lp.isLocal || serverRole == DeviceRole.SERVER_STANDALONE ) {
                String name;
                if ( lp.isRobot() ) {
                    String format = LocUtils.getString( context, R.string.robot_name_fmt );
                    name = String.format( format, lp.name );
                } else {
                    name = lp.name;
                }
                names[ii] = String.format( nameFmt, name, dictName(lp) );
            } else {
                names[ii] = LocUtils.getString( context, R.string.guest_name );
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
    public void replaceDicts( Context context, String newDict )
    {
        String[] dicts =
            DictLangCache.getHaveLang( context, isoCode() );
        HashSet<String> installed = new HashSet<>( Arrays.asList(dicts) );

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

    public String langName( Context context )
    {
        return DictLangCache.getLangNameForISOCode( context, isoCode() );
    }

    public String dictName( final LocalPlayer lp )
    {
        String dname = lp.dictName;
        if ( null == dname || 0 == dname.length() ) {
            dname = dictName;
        }
        return dname;
    }

    public String dictName( int indx )
    {
        String dname = null;
        if ( 0 <= indx && indx < nPlayers ) {
            dname = dictName( players[indx] );
        }
        return dname;
    }

    public String getName()
    {
        // Assert.assertNotNull( m_name );
        return m_name;
    }

    public void setName( String name )
    {
        m_name = name;
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

    public void setNPlayers( int nPlayersTotal, int nPlayersHere,
                             boolean localsAreRobots )
    {
        assert( nPlayersTotal < MAX_NUM_PLAYERS );
        assert( nPlayersHere < nPlayersTotal );

        nPlayers = nPlayersTotal;

        for ( int ii = 0; ii < nPlayersTotal; ++ii ) {
            boolean isLocal = ii < nPlayersHere;
            LocalPlayer player = players[ii];
            player.isLocal = isLocal;
            if ( isLocal && localsAreRobots ) {
                player.setIsRobot( true );
            } else {
                assert( !player.isRobot() );
            }
        }
    }

    private boolean moveUp( int which )
    {
        boolean canMove = which > 0 && which < nPlayers;
        if ( canMove ) {
            LocalPlayer tmp = players[which-1];
            players[which-1] = players[which];
            players[which] = tmp;
        }
        return canMove;
    }

    private boolean moveDown( int which )
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

    private void assignDicts( Context context )
    {
        // For each player's dict, if non-null and language matches
        // leave it alone.  Otherwise replace with default if that
        // matches langauge.  Otherwise pick an arbitrary dict in the
        // right language.

        String humanDict =
            DictLangCache.getBestDefault( context, isoCode(), true );
        String robotDict =
            DictLangCache.getBestDefault( context, isoCode(), false );

        if ( null == dictName
             || ! DictUtils.dictExists( context, dictName )
             || ! DictLangCache.getDictISOCode( context, dictName ).equals( isoCode() ) ) {
            dictName = humanDict;
        }

        for ( int ii = 0; ii < nPlayers; ++ii ) {
            LocalPlayer lp = players[ii];

            if ( null != lp.dictName &&
                 !ISOCode.safeEquals( DictLangCache.getDictISOCode(context, lp.dictName),
                                      isoCode() ) ) {
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
