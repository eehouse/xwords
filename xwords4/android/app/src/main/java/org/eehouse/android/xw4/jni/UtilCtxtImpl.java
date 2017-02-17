/* -*- compile-command: "find-and-gradle.sh installXw4Debug"; -*- */
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
import android.telephony.PhoneNumberUtils;

import junit.framework.Assert;

import org.eehouse.android.xw4.DbgUtils;
import org.eehouse.android.xw4.DevID;
import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.XWApp;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;
import org.eehouse.android.xw4.loc.LocUtils;

public class UtilCtxtImpl implements UtilCtxt {
    private static final String TAG = UtilCtxtImpl.class.getSimpleName();
    private Context m_context;

    private UtilCtxtImpl() {}   // force subclasses to pass context

    public UtilCtxtImpl( Context context )
    {
        super();
        m_context = context;
    }

    public void requestTime() {
        subclassOverride( "requestTime" );
    }

    public int userPickTileBlank( int playerNum, String[] texts )
    {
        subclassOverride( "userPickTileBlank" );
        return 0;
    }

    public int userPickTileTray( int playerNum, String[] texts,
                                 String[] curTiles, int nPicked )
    {
        subclassOverride( "userPickTileTray" );
        return 0;
    }

    public void informNeedPassword( int player, String name )
    {
        subclassOverride( "informNeedPassword" );
    }

    public void turnChanged( int newTurn )
    {
        subclassOverride( "turnChanged" );
    }

    public boolean engineProgressCallback()
    {
        // subclassOverride( "engineProgressCallback" );
        return true;
    }

    public void setTimer( int why, int when, int handle )
    {
        subclassOverride( "setTimer" );
    }

    public void clearTimer( int why )
    {
        subclassOverride( "clearTimer" );
    }

    public void remSelected()
    {
        subclassOverride( "remSelected" );
    }

    public void setIsServer( boolean isServer )
    {
        subclassOverride( "setIsServer" );
    }

    public String getDevID( /*out*/ byte[] typa )
    {
        UtilCtxt.DevIDType typ = UtilCtxt.DevIDType.ID_TYPE_NONE;
        String result = DevID.getRelayDevID( m_context );
        if ( null != result ) {
            typ = UtilCtxt.DevIDType.ID_TYPE_RELAY;
        } else {
            result = DevID.getGCMDevID( m_context );
            if ( result.equals("") ) {
                result = null;
            } else {
                typ = UtilCtxt.DevIDType.ID_TYPE_ANDROID_GCM;
            }
        }
        typa[0] = (byte)typ.ordinal();
        return result;
    }

    public void deviceRegistered( UtilCtxt.DevIDType devIDType, String idRelay )
    {
        switch ( devIDType ) {
        case ID_TYPE_RELAY:
            DevID.setRelayDevID( m_context, idRelay );
            break;
        case ID_TYPE_NONE:
            DevID.clearRelayDevID( m_context );
            break;
        default:
            Assert.fail();
            break;
        }
    }

    public void bonusSquareHeld( int bonus )
    {
    }

    public void playerScoreHeld( int player )
    {
    }

    public void cellSquareHeld( String words )
    {
    }

    public String getUserString( int stringCode )
    {
        int id = 0;
        switch( stringCode ) {
        case UtilCtxt.STR_ROBOT_MOVED:
            id = R.string.str_robot_moved_fmt;
            break;
        case UtilCtxt.STRS_VALUES_HEADER:
            id = R.string.strs_values_header_fmt;
            break;
        case UtilCtxt.STRD_REMAINING_TILES_ADD:
            id = R.string.strd_remaining_tiles_add_fmt;
            break;
        case UtilCtxt.STRD_UNUSED_TILES_SUB:
            id = R.string.strd_unused_tiles_sub_fmt;
            break;
        case UtilCtxt.STRS_REMOTE_MOVED:
            id = R.string.str_remote_moved_fmt;
            break;
        case UtilCtxt.STRD_TIME_PENALTY_SUB:
            id = R.string.strd_time_penalty_sub_fmt;
            break;
        case UtilCtxt.STR_PASS:
            id = R.string.str_pass;
            break;
        case UtilCtxt.STRS_MOVE_ACROSS:
            id = R.string.strs_move_across_fmt;
            break;
        case UtilCtxt.STRS_MOVE_DOWN:
            id = R.string.strs_move_down_fmt;
            break;
        case UtilCtxt.STRS_TRAY_AT_START:
            id = R.string.strs_tray_at_start_fmt;
            break;
        case UtilCtxt.STRSS_TRADED_FOR:
            id = R.string.strss_traded_for_fmt;
            break;
        case UtilCtxt.STR_PHONY_REJECTED:
            id = R.string.str_phony_rejected;
            break;
        case UtilCtxt.STRD_CUMULATIVE_SCORE:
            id = R.string.strd_cumulative_score_fmt;
            break;
        case UtilCtxt.STRS_NEW_TILES:
            id = R.string.strs_new_tiles_fmt;
            break;
        case UtilCtxt.STR_COMMIT_CONFIRM:
            id = R.string.str_commit_confirm;
            break;
        case UtilCtxt.STR_BONUS_ALL:
            id = R.string.str_bonus_all;
            break;
        case UtilCtxt.STRD_TURN_SCORE:
            id = R.string.strd_turn_score_fmt;
            break;
        case UtilCtxt.STRSD_RESIGNED:
            id = R.string.str_resigned_fmt;
            break;
        case UtilCtxt.STRSD_WINNER:
            id = R.string.str_winner_fmt;
            break;
        case UtilCtxt.STRDSD_PLACER:
            id = R.string.str_placer_fmt;
            break;

        default:
            DbgUtils.logw( TAG, "no such stringCode: %d", stringCode );
        }

        String result = (0 == id) ? "" : LocUtils.getString( m_context, id );
        return result;
    }

    public String getUserQuantityString( int stringCode, int quantity )
    {
        int pluralsId = 0;
        switch ( stringCode ) {
        case UtilCtxt.STRD_ROBOT_TRADED:
            pluralsId = R.plurals.strd_robot_traded_fmt;
            break;
        case UtilCtxt.STRD_REMAINS_HEADER:
            pluralsId = R.plurals.strd_remains_header_fmt;
            break;
        case UtilCtxt.STRD_REMAINS_EXPL:
            pluralsId = R.plurals.strd_remains_expl_fmt;
            break;
        }

        String result = "";
        if ( 0 != pluralsId ) {
            result = LocUtils.getQuantityString( m_context, pluralsId, quantity );
        }
        return result;
    }

    public boolean userQuery( int id, String query )
    {
        subclassOverride( "userQuery" );
        return false;
    }

    public boolean confirmTrade( String[] tiles )
    {
        subclassOverride( "confirmTrade" );
        return false;
    }

    public void userError( int id )
    {
        subclassOverride( "userError" );
    }

    public void informMove( int turn, String expl, String words )
    {
        subclassOverride( "informMove" );
    }

    public void informUndo()
    {
        subclassOverride( "informUndo" );
    }

    public void informNetDict( int lang, String oldName,
                               String newName, String newSum,
                               CurGameInfo.XWPhoniesChoice phonies )
    {
        subclassOverride( "informNetDict" );
    }

    public void informMissing( boolean isServer,
                               CommsConnTypeSet connTypes,
                               int nDevices, int nMissingPlayers )
    {
        subclassOverride( "informMissing" );
    }

    // Probably want to cache the fact that the game over notification
    // showed up and then display it next time game's opened.
    public void notifyGameOver()
    {
        subclassOverride( "notifyGameOver" );
    }

    public boolean warnIllegalWord( String dict, String[] words, int turn,
                                    boolean turnLost )
    {
        subclassOverride( "warnIllegalWord" );
        return false;
    }

    // These need to go into some sort of chat DB, not dropped.
    public void showChat( String msg, int fromIndx, String fromName )
    {
        subclassOverride( "showChat" );
    }

    public boolean phoneNumbersSame( String num1, String num2 )
    {
        boolean same = PhoneNumberUtils.compare( m_context, num1, num2 );
        return same;
    }

    private void subclassOverride( String name ) {
        // DbgUtils.logf( "%s::%s() called", getClass().getName(), name );
    }

}