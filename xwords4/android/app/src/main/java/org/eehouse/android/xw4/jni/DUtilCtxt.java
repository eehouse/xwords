/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
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
import android.telephony.PhoneNumberUtils;

import org.eehouse.android.xw4.Assert;
import org.eehouse.android.xw4.DBUtils;
import org.eehouse.android.xw4.DevID;
import org.eehouse.android.xw4.FBMService;
import org.eehouse.android.xw4.Log;
import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.XWApp;
import org.eehouse.android.xw4.loc.LocUtils;

public class DUtilCtxt {
    private static final String TAG = DUtilCtxt.class.getSimpleName();

    private Context m_context;

    public DUtilCtxt() {
        m_context = XWApp.getContext();
    }

    // Possible values for typ[0], these must match enum in xwrelay.h
    public enum DevIDType { ID_TYPE_NONE
                            , ID_TYPE_RELAY
                            , ID_TYPE_LINUX
                            , ID_TYPE_ANDROID_GCM_UNUSED // 3
                            , ID_TYPE_ANDROID_OTHER
                            , ID_TYPE_ANON
                            , ID_TYPE_ANDROID_FCM // 6
            }

    public String getDevID( /*out*/ byte[] typa )
    {
        DevIDType typ = DevIDType.ID_TYPE_NONE;
        String result = DevID.getRelayDevID( m_context );
        if ( null != result ) {
            typ = DevIDType.ID_TYPE_RELAY;
        } else {
            result = FBMService.getFCMDevID( m_context );
            if ( null == result ) {
                // do nothing
            } else if ( result.equals("") ) {
                result = null;
            } else {
                typ = DevIDType.ID_TYPE_ANDROID_FCM;
            }
        }
        typa[0] = (byte)typ.ordinal();
        return result;
    }

    public void deviceRegistered( DevIDType devIDType, String idRelay )
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

    static final int STRD_ROBOT_TRADED =                  1;
    static final int STR_ROBOT_MOVED =                    2;
    static final int STRS_VALUES_HEADER =                 3;
    static final int STRD_REMAINING_TILES_ADD =           4;
    static final int STRD_UNUSED_TILES_SUB =              5;
    static final int STRS_REMOTE_MOVED =                  6;
    static final int STRD_TIME_PENALTY_SUB =              7;
    static final int STR_PASS =                           8;
    static final int STRS_MOVE_ACROSS =                   9;
    static final int STRS_MOVE_DOWN =                    10;
    static final int STRS_TRAY_AT_START =                11;
    static final int STRSS_TRADED_FOR =                  12;
    static final int STR_PHONY_REJECTED =                13;
    static final int STRD_CUMULATIVE_SCORE =             14;
    static final int STRS_NEW_TILES =                    15;
    static final int STR_COMMIT_CONFIRM =                16;
    static final int STR_BONUS_ALL =                     17;
    static final int STRD_TURN_SCORE =                   18;
    static final int STRD_REMAINS_HEADER =               19;
    static final int STRD_REMAINS_EXPL =                 20;
    static final int STRSD_RESIGNED =                    21;
    static final int STRSD_WINNER =                      22;
    static final int STRDSD_PLACER  =                    23;

    
    public String getUserString( int stringCode )
    {
        Log.d( TAG, "getUserString(%d)", stringCode );
        int id = 0;
        switch( stringCode ) {
        case STR_ROBOT_MOVED:
            id = R.string.str_robot_moved_fmt;
            break;
        case STRS_VALUES_HEADER:
            id = R.string.strs_values_header_fmt;
            break;
        case STRD_REMAINING_TILES_ADD:
            id = R.string.strd_remaining_tiles_add_fmt;
            break;
        case STRD_UNUSED_TILES_SUB:
            id = R.string.strd_unused_tiles_sub_fmt;
            break;
        case STRS_REMOTE_MOVED:
            id = R.string.str_remote_moved_fmt;
            break;
        case STRD_TIME_PENALTY_SUB:
            id = R.string.strd_time_penalty_sub_fmt;
            break;
        case STR_PASS:
            id = R.string.str_pass;
            break;
        case STRS_MOVE_ACROSS:
            id = R.string.strs_move_across_fmt;
            break;
        case STRS_MOVE_DOWN:
            id = R.string.strs_move_down_fmt;
            break;
        case STRS_TRAY_AT_START:
            id = R.string.strs_tray_at_start_fmt;
            break;
        case STRSS_TRADED_FOR:
            id = R.string.strss_traded_for_fmt;
            break;
        case STR_PHONY_REJECTED:
            id = R.string.str_phony_rejected;
            break;
        case STRD_CUMULATIVE_SCORE:
            id = R.string.strd_cumulative_score_fmt;
            break;
        case STRS_NEW_TILES:
            id = R.string.strs_new_tiles_fmt;
            break;
        case STR_COMMIT_CONFIRM:
            id = R.string.str_commit_confirm;
            break;
        case STR_BONUS_ALL:
            id = R.string.str_bonus_all;
            break;
        case STRD_TURN_SCORE:
            id = R.string.strd_turn_score_fmt;
            break;
        case STRSD_RESIGNED:
            id = R.string.str_resigned_fmt;
            break;
        case STRSD_WINNER:
            id = R.string.str_winner_fmt;
            break;
        case STRDSD_PLACER:
            id = R.string.str_placer_fmt;
            break;

        default:
            Log.w( TAG, "no such stringCode: %d", stringCode );
        }

        String result = (0 == id) ? "" : LocUtils.getString( m_context, id );
        Log.d( TAG, "getUserString() => %s", result );
        return result;
    }

    public String getUserQuantityString( int stringCode, int quantity )
    {
        int pluralsId = 0;
        switch ( stringCode ) {
        case STRD_ROBOT_TRADED:
            pluralsId = R.plurals.strd_robot_traded_fmt;
            break;
        case STRD_REMAINS_HEADER:
            pluralsId = R.plurals.strd_remains_header_fmt;
            break;
        case STRD_REMAINS_EXPL:
            pluralsId = R.plurals.strd_remains_expl_fmt;
            break;
        }

        String result = "";
        if ( 0 != pluralsId ) {
            result = LocUtils.getQuantityString( m_context, pluralsId, quantity );
        }
        return result;
    }

    public boolean phoneNumbersSame( String num1, String num2 )
    {
        boolean same = PhoneNumberUtils.compare( m_context, num1, num2 );
        return same;
    }

    public void store( String key, byte[] data )
    {
        // Log.d( TAG, "store(key=%s)", key );
        if ( null != data ) {
            DBUtils.setBytesFor( m_context, key, data );
        }
    }

    public byte[] load( String key )
    {
        byte[] result = null;
        int resultLen = 0;
        Log.d( TAG, "load(key=%s)", key );

        result = DBUtils.getBytesFor( m_context, key );
        if ( result != null ) {
            resultLen = result.length;
        }

        Log.d( TAG, "load(%s) returning %d bytes", key, resultLen );
        return result;
    }
}
