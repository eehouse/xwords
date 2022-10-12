/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2022 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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
import android.content.Intent;
import android.telephony.PhoneNumberUtils;

import java.util.Arrays;

import org.eehouse.android.xw4.Assert;
import org.eehouse.android.xw4.BuildConfig;
import org.eehouse.android.xw4.Channels;
import org.eehouse.android.xw4.DBUtils;
import org.eehouse.android.xw4.DevID;
import org.eehouse.android.xw4.DictUtils;
import org.eehouse.android.xw4.DupeModeTimer;
import org.eehouse.android.xw4.GameUtils;
import org.eehouse.android.xw4.GamesListDelegate;
import org.eehouse.android.xw4.Log;
import org.eehouse.android.xw4.MQTTUtils;
import org.eehouse.android.xw4.NetLaunchInfo;
import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.Utils;
import org.eehouse.android.xw4.XWApp;
import org.eehouse.android.xw4.loc.LocUtils;

public class DUtilCtxt {
    private static final String TAG = DUtilCtxt.class.getSimpleName();

    private Context m_context;

    public DUtilCtxt() {
        m_context = XWApp.getContext();
    }

    private static final int STRD_ROBOT_TRADED =                  1;
    private static final int STR_ROBOT_MOVED =                    2;
    private static final int STRS_VALUES_HEADER =                 3;
    private static final int STRD_REMAINING_TILES_ADD =           4;
    private static final int STRD_UNUSED_TILES_SUB =              5;
    private static final int STRS_REMOTE_MOVED =                  6;
    private static final int STRD_TIME_PENALTY_SUB =              7;
    private static final int STR_PASS =                           8;
    private static final int STRS_MOVE_ACROSS =                   9;
    private static final int STRS_MOVE_DOWN =                    10;
    private static final int STRS_TRAY_AT_START =                11;
    private static final int STRSS_TRADED_FOR =                  12;
    private static final int STR_PHONY_REJECTED =                13;
    private static final int STRD_CUMULATIVE_SCORE =             14;
    private static final int STRS_NEW_TILES =                    15;
    private static final int STR_COMMIT_CONFIRM =                16;
    private static final int STR_SUBMIT_CONFIRM =                17;
    private static final int STR_BONUS_ALL =                     18;
    private static final int STRD_TURN_SCORE =                   19;
    private static final int STRD_REMAINS_HEADER =               20;
    private static final int STRD_REMAINS_EXPL =                 21;
    private static final int STRSD_RESIGNED =                    22;
    private static final int STRSD_WINNER =                      23;
    private static final int STRDSD_PLACER  =                    24;
    private static final int STR_DUP_CLIENT_SENT =               25;
    private static final int STRDD_DUP_HOST_RECEIVED =           26;
    private static final int STR_DUP_MOVED =                     27;
    private static final int STRD_DUP_TRADED =                   28;
    private static final int STRSD_DUP_ONESCORE =                29;
    private static final int STR_PENDING_PLAYER =                30;
    private static final int STR_BONUS_ALL_SUB =                 31;
    private static final int STRS_DUP_ALLSCORES =                32;

    public String getUserString( final int stringCode )
    {
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
        case STR_SUBMIT_CONFIRM:
            id = R.string.str_submit_confirm;
            break;
        case STR_BONUS_ALL:
            id = R.string.str_bonus_all;
            break;
        case STR_BONUS_ALL_SUB:
            id = R.string.str_bonus_all_fmt;
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

        case STR_DUP_CLIENT_SENT:
            id = R.string.dup_client_sent;
            break;
        case STRDD_DUP_HOST_RECEIVED:
            id = R.string.dup_host_received_fmt;
            break;
        case STR_DUP_MOVED:
            id = R.string.dup_moved;
            break;
        case STRD_DUP_TRADED:
            id = R.string.dup_traded_fmt;
            break;
        case STRSD_DUP_ONESCORE:
            id = R.string.dup_onescore_fmt;
            break;
        case STRS_DUP_ALLSCORES:
            id = R.string.dup_allscores_fmt;
            break;

        case STR_PENDING_PLAYER:
            id = R.string.missing_player;
            break;

        default:
            Log.w( TAG, "no such stringCode: %d", stringCode );
        }

        String result = (0 == id) ? "" : LocUtils.getString( m_context, id );
        // Log.d( TAG, "getUserString(%d) => %s", stringCode, result );
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
            if ( BuildConfig.DEBUG ) {
                byte[] tmp = load( key );
                Assert.assertTrue( Arrays.equals( tmp, data ) );
            }
        }
    }

    public byte[] load( String key )
    {
        // Log.d( TAG, "load(%s, %s)", key, keySuffix );
        byte[] result = DBUtils.getBytesFor( m_context, key );

        // Log.d( TAG, "load(%s, %s) returning %d bytes", key, keySuffix,
        //        null == result ? 0 : result.length );
        return result;
    }

    // Must match enum DupPauseType
    public static final int UNPAUSED = 0;
    public static final int PAUSED = 1;
    public static final int AUTOPAUSED = 2;

    // PENDING use prefs for this
    public String getUsername( int posn, boolean isLocal, boolean isRobot )
    {
        Log.d( TAG, "getUsername(posn=%d; isLocal=%b, isRobot=%b)",
               posn, isLocal, isRobot );
        String fmt = isLocal ? "Lcl" : "Rmt";
        fmt += isRobot ? "Rbt" : "Hum";
        fmt += " %d";
        return String.format( fmt, posn + 1 );
    }

    // A pause can come in when a game's open or when it's not. If it's open,
    // we want to post an alert. If it's not, we want to post a notification,
    // or at least kick off DupeModeTimer to cancel or start the timer-running
    // notification.
    public void notifyPause( int gameID, int pauseType, int pauser,
                             String pauserName, String expl )
    {
        long[] rowids = DBUtils.getRowIDsFor( m_context, gameID );
        // Log.d( TAG, "got %d games with gameid", rowids.length );

        final boolean isPause = UNPAUSED != pauseType;

        for ( long rowid : rowids ) {
            String msg = msgForPause( rowid, pauseType, pauserName, expl );
            try ( JNIThread thread = JNIThread.getRetained( rowid ) ) {
                if ( null != thread ) {
                    thread.notifyPause( pauser, isPause, msg );
                } else {
                    Intent intent = GamesListDelegate
                        .makeRowidIntent( m_context, rowid );
                    int titleID = isPause ? R.string.game_paused_title
                        : R.string.game_unpaused_title;
                    Channels.ID channelID = Channels.ID.DUP_PAUSED;
                    Utils.postNotification( m_context, intent, titleID, msg,
                                            rowid, channelID );

                    // DupeModeTimer.timerPauseChanged( m_context, rowid );
                }
            }
        }
    }

    // PENDING: channel is ignored here, meaning there can't be two ends of a
    // game in the same app.
    public boolean haveGame( int gameID, int channel )
    {
        boolean result = DBUtils.haveGame( m_context, gameID );
        Log.d( TAG, "haveGame(%d, %d) => %b", gameID, channel, result );
        return result;
    }

    private String msgForPause( long rowid, int pauseType, String pauserName, String expl )
    {
        String msg;
        final String gameName = GameUtils.getName( m_context, rowid );
        if ( AUTOPAUSED == pauseType ) {
            msg = LocUtils.getString( m_context, R.string.autopause_expl_fmt,
                                      gameName );
        } else {
            boolean isPause = PAUSED == pauseType;
            if ( null != expl && 0 < expl.length() ) {
                msg = LocUtils.getString( m_context,
                                          isPause ? R.string.pause_notify_expl_fmt
                                          : R.string.unpause_notify_expl_fmt,
                                          pauserName, expl );
            } else {
                msg = LocUtils.getString( m_context,
                                          isPause ? R.string.pause_notify_fmt
                                          : R.string.unpause_notify_fmt,
                                          pauserName );
            }
        }
        return msg;
    }

    public void getDictPath( String name, String[] path, byte[][] bytes )
    {
        Log.d( TAG, "getDictPath(name='%s')", name );
        String[] names = { name };
        DictUtils.DictPairs pairs = DictUtils.openDicts( m_context, names );
        // Log.d( TAG, "openDicts() => %s", pairs );
        path[0] = pairs.m_paths[0];
        bytes[0] = pairs.m_bytes[0];
        // Log.d( TAG, "getDictPath(%s): have path: %s; bytes: %s", name, path[0], bytes[0] );
        Assert.assertTrueNR( path[0] != null || bytes[0] != null );
    }

    public void onDupTimerChanged( int gameID, int oldVal, int newVal )
    {
        DupeModeTimer.timerChanged( m_context, gameID, newVal );
    }

    public void onInviteReceived( NetLaunchInfo nli )
    {
        // Log.d( TAG, "onInviteReceived(%s)", nli );
        MQTTUtils.makeOrNotify( m_context, nli );
    }

    public void onMessageReceived( int gameID, CommsAddrRec from, byte[] msg )
    {
        // Log.d( TAG, "onMessageReceived()" );
        Assert.assertTrueNR( from.contains( CommsAddrRec.CommsConnType.COMMS_CONN_MQTT ) );
        MQTTUtils.handleMessage( m_context, from, gameID, msg );
    }

    public void onGameGoneReceived( int gameID, CommsAddrRec from )
    {
        Assert.assertTrueNR( from.contains( CommsAddrRec.CommsConnType.COMMS_CONN_MQTT ) );
        MQTTUtils.handleGameGone( m_context, from, gameID );
    }

    public void ackMQTTMsg( int gameID, String senderID, byte[] msg )
    {
        MQTTUtils.ackMessage( m_context, gameID, senderID, msg );
    }
}
