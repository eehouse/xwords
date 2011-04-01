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

import org.eehouse.android.xw4.Utils;
import org.eehouse.android.xw4.R;

public class UtilCtxtImpl implements UtilCtxt {
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

    public int userPickTile( int playerNum, String[] texts )
    {
        subclassOverride( "userPickTile" );
        return 0;
    }

    public String askPassword( String name )
    {
        subclassOverride( "askPassword" );
        return null;
    }

    public void turnChanged()
    {
        subclassOverride( "turnChanged" );
    }

    public boolean engineProgressCallback()
    {
        subclassOverride( "engineProgressCallback" );
        return true;
    }

    public void engineStarting( int nBlanks )
    {
        subclassOverride( "engineStarting" );
    }

    public void engineStopping()
    {
        subclassOverride( "engineStopping" );
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

    public String getUserString( int stringCode )
    {
        int id = 0;
        switch( stringCode ) {
        case UtilCtxt.STRD_ROBOT_TRADED:
            id = R.string.strd_robot_traded;
            break;
        case UtilCtxt.STR_ROBOT_MOVED:
            id = R.string.str_robot_moved;
            break;
        case UtilCtxt.STRS_VALUES_HEADER:
            id = R.string.strs_values_header;
            break;
        case UtilCtxt.STRD_REMAINING_TILES_ADD:
            id = R.string.strd_remaining_tiles_add;
            break;
        case UtilCtxt.STRD_UNUSED_TILES_SUB:
            id = R.string.strd_unused_tiles_sub;
            break;
        case UtilCtxt.STR_REMOTE_MOVED:
            id = R.string.str_remote_moved;
            break;
        case UtilCtxt.STRD_TIME_PENALTY_SUB:
            id = R.string.strd_time_penalty_sub;
            break;
        case UtilCtxt.STR_PASS:
            id = R.string.str_pass;
            break;
        case UtilCtxt.STRS_MOVE_ACROSS:
            id = R.string.strs_move_across;
            break;
        case UtilCtxt.STRS_MOVE_DOWN:
            id = R.string.strs_move_down;
            break;
        case UtilCtxt.STRS_TRAY_AT_START:
            id = R.string.strs_tray_at_start;
            break;
        case UtilCtxt.STRSS_TRADED_FOR:
            id = R.string.strss_traded_for;
            break;
        case UtilCtxt.STR_PHONY_REJECTED:
            id = R.string.str_phony_rejected;
            break;
        case UtilCtxt.STRD_CUMULATIVE_SCORE:
            id = R.string.strd_cumulative_score;
            break;
        case UtilCtxt.STRS_NEW_TILES:
            id = R.string.strs_new_tiles;
            break;
        case UtilCtxt.STR_PASSED:
            id = R.string.str_passed;
            break;
        case UtilCtxt.STRSD_SUMMARYSCORED:
            id = R.string.strsd_summaryscored;
            break;
        case UtilCtxt.STRD_TRADED:
            id = R.string.strd_traded;
            break;
        case UtilCtxt.STR_LOSTTURN:
            id = R.string.str_lostturn;
            break;
        case UtilCtxt.STR_COMMIT_CONFIRM:
            id = R.string.str_commit_confirm;
            break;
        case UtilCtxt.STR_LOCAL_NAME:
            id = R.string.str_local_name;
            break;
        case UtilCtxt.STR_NONLOCAL_NAME:
            id = R.string.str_nonlocal_name;
            break;
        case UtilCtxt.STR_BONUS_ALL:
            id = R.string.str_bonus_all;
            break;
        case UtilCtxt.STRD_TURN_SCORE:
            id = R.string.strd_turn_score;
            break;
        default:
            Utils.logf( "no such stringCode: " + stringCode );
        }

        String result;
        if ( 0 == id ) {
            result = "";
        } else {
            result = m_context.getString( id );
        }
        return result;
    }

    public boolean userQuery( int id, String query )
    {
        subclassOverride( "userQuery" );
        return false;
    }

    public void userError( int id )
    {
        subclassOverride( "userError" );
    }

    // Probably want to cache the fact that the game over notification
    // showed up and then display it next time game's opened.
    public void notifyGameOver()
    {
        subclassOverride( "notifyGameOver" );
    }

    public boolean warnIllegalWord( String[] words, int turn, boolean turnLost )
    {
        subclassOverride( "warnIllegalWord" );
        return false;
    }

    // These need to go into some sort of chat DB, not dropped.
    public void showChat( String msg )
    {
        subclassOverride( "showChat" );
    }

    private void subclassOverride( String name ) {
        Utils.logf( "%s::%s() called", getClass().getName(), name );
    }

}
