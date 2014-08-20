/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2010 by Eric House (xwords@eehouse.org).  All rights
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

package org.eehouse.android.xw4;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.SystemClock;
import java.util.Date;

import junit.framework.Assert;

import org.eehouse.android.xw4.DBUtils.NeedsNagInfo;
import org.eehouse.android.xw4.loc.LocUtils;
import org.eehouse.android.xw4.jni.GameSummary;

public class NagTurnReceiver extends BroadcastReceiver {

    private static final long INTERVAL_MILLIS = 1000 * 30; // every half minute for now
    private static final long[] NAG_INTERVAL_SECONDS = {// 2*60, // two minutes (for testing)
                                                        // 5*60,
                                                        // 10*60,
                                                        60*60*24*1, // one day
                                                        60*60*24*2, // two days
                                                        60*60*24*3, // three days
    };

    @Override
    public void onReceive( Context context, Intent intent )
    {
        // loop through all games testing who's been sitting on a turn
        NeedsNagInfo[] needNagging = DBUtils.getNeedNagging( context );
        if ( null != needNagging ) {
            long now = new Date().getTime(); // in milliseconds
            for ( NeedsNagInfo info : needNagging ) {
                Assert.assertTrue( info.m_nextNag < now );
                info.m_nextNag = figureNextNag( info.m_lastMoveMillis );
                boolean lastWarning = 0 == info.m_nextNag;

                long rowid = info.m_rowid;
                GameSummary summary = DBUtils.getSummary( context, rowid, 10 );
                String prevPlayer = null == summary 
                    ? LocUtils.getString(context, R.string.prev_player)
                    : summary.getPrevPlayer();

                Intent msgIntent = GamesListDelegate.makeRowidIntent( context, rowid );
                int nHours = (int)(now - info.m_lastMoveMillis) / (1000 * 60 * 60);
                String body = String.format( LocUtils.getString(context, R.string.nag_body_fmt),
                                             prevPlayer, nHours );
                if ( lastWarning ) {
                    body = LocUtils
                        .getString( context, R.string.nag_warn_last_fmt, body );
                }
                Utils.postNotification( context, msgIntent, R.string.nag_title, 
                                        body, (int)rowid );

            }
            DBUtils.updateNeedNagging( context, needNagging );

            setNagTimer( context );
        }
    }

    public static void restartTimer( Context context )
    {
        setNagTimer( context );
    }

    private static void restartTimer( Context context, long atMillis )
    {
        AlarmManager am =
            (AlarmManager)context.getSystemService( Context.ALARM_SERVICE );

        Intent intent = new Intent( context, NagTurnReceiver.class );
        PendingIntent pi = PendingIntent.getBroadcast( context, 0, intent, 0 );

        long now = new Date().getTime(); // in milliseconds
        am.set( AlarmManager.RTC, atMillis, pi );
    }

    public static void setNagTimer( Context context )
    {
        long nextNag = DBUtils.getNextNag( context );
        if ( 0 < nextNag ) {
            restartTimer( context, nextNag );
        }
    }

    public static long figureNextNag( long moveTimeMillis )
    {
        long result = 0;
        long now = new Date().getTime(); // in milliseconds
        Assert.assertTrue( now >= moveTimeMillis );
        for ( long nSecs : NAG_INTERVAL_SECONDS ) {
            long asMillis = moveTimeMillis + (nSecs * 1000);
            if ( asMillis >= now ) {
                result = asMillis;
                break;
            }
        }

        return result;
    }
}
