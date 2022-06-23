/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 - 2022 by Eric House (xwords@eehouse.org).  All rights
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

import android.content.Context;
import android.content.Intent;
import android.text.TextUtils;


import org.eehouse.android.xw4.DBUtils.NeedsNagInfo;
import org.eehouse.android.xw4.jni.GameSummary;
import org.eehouse.android.xw4.loc.LocUtils;

import java.util.ArrayList;
import java.util.Date;
import java.util.Iterator;

public class NagTurnReceiver {
    private static final String TAG = NagTurnReceiver.class.getSimpleName();

    private static final long[] NAG_INTERVAL_SECONDS = {// 2*60, // two minutes (for testing)
                                                        // 5*60,
                                                        // 10*60,
                                                        60*60*24*1, // one day
                                                        60*60*24*2, // two days
                                                        60*60*24*3, // three days
    };

    private static final int[][] s_fmtData = {
        { 60*60*24, R.plurals.nag_days_fmt },
        { 60*60, R.plurals.nag_hours_fmt },
        { 60, R.plurals.nag_minutes_fmt },
    };

    private static Boolean s_nagsDisabledNet = null;
    private static Boolean s_nagsDisabledSolo = null;

    private static TimerReceiver.TimerCallback sTimerCallbacks
        = new TimerReceiver.TimerCallback() {
                @Override
                public void timerFired( Context context )
                {
                    NagTurnReceiver.timerFired( context );
                }

                @Override
                public long incrementBackoff( long prevBackoff )
                {
                    Assert.failDbg();
                    return 0;
                }
            };

    private static void timerFired( Context context )
    {
        // loop through all games testing who's been sitting on a turn
        if ( !getNagsDisabled( context ) ) {
            NeedsNagInfo[] needNagging = DBUtils.getNeedNagging( context );
            if ( null != needNagging ) {
                long now = System.currentTimeMillis();
                for ( NeedsNagInfo info : needNagging ) {
                    Assert.assertTrueNR( info.m_nextNag < now );

                    info.m_nextNag = figureNextNag( context,
                                                    info.m_lastMoveMillis );

                    // Skip display of notifications disabled for this type
                    // of game
                    if ( s_nagsDisabledSolo && info.isSolo() ) {
                        // do nothing
                    } else if ( s_nagsDisabledNet && !info.isSolo() ) {
                        // do nothing
                    } else {
                        boolean lastWarning = 0 == info.m_nextNag;
                        long rowid = info.m_rowid;
                        GameSummary summary = GameUtils.getSummary( context, rowid,
                                                                    10 );
                        String prevPlayer = null == summary
                            ? LocUtils.getString(context, R.string.prev_player)
                            : summary.getPrevPlayer();

                        Intent msgIntent =
                            GamesListDelegate.makeRowidIntent( context, rowid );
                        String millis = formatMillis( context,
                                                      now - info.m_lastMoveMillis);
                        String body =
                            String.format( LocUtils.getString(context,
                                                              R.string.nag_body_fmt),
                                           prevPlayer, millis );
                        if ( lastWarning ) {
                            body = LocUtils
                                .getString( context, R.string.nag_warn_last_fmt, body );
                        }
                        Utils.postNotification( context, msgIntent,
                                                R.string.nag_title, body,
                                                rowid );
                    }
                }
                DBUtils.updateNeedNagging( context, needNagging );

                setNagTimer( context );
            }
        }
    }

    public static void restartTimer( Context context )
    {
        setNagTimer( context );
    }

    private static void restartTimer( Context context, long fireTimeMS )
    {
        if ( !getNagsDisabled( context ) ) {
            TimerReceiver.setTimer( context, sTimerCallbacks, fireTimeMS );
        }
    }

    public static void setNagTimer( Context context )
    {
        if ( !getNagsDisabled( context ) ) {
            long nextNag = DBUtils.getNextNag( context );
            if ( 0 < nextNag ) {
                restartTimer( context, nextNag );
            }
        }
    }

    public static long figureNextNag( Context context, long moveTimeMillis )
    {
        long result = 0;
        long now = System.currentTimeMillis();
        if ( now >= moveTimeMillis ) {
            long[] intervals = getIntervals( context );
            for ( long nSecs : intervals ) {
                long asMillis = moveTimeMillis + (nSecs * 1000);
                if ( asMillis >= now ) {
                    result = asMillis;
                    break;
                }
            }
        } else {
            Assert.failDbg();
        }
        return result;
    }

    private static long[] s_lastIntervals = null;
    private static String s_lastStr = null;
    private static long[] getIntervals( Context context )
    {
        long[] result = null;
        String pref =
            XWPrefs.getPrefsString( context, R.string.key_nag_intervals );
        if ( null != pref && 0 < pref.length() ) {
            if ( pref.equals( s_lastStr ) ) {
                result = s_lastIntervals;
            } else {
                String[] strs = TextUtils.split( pref, "," );
                ArrayList<Long> al = new ArrayList<>();
                for ( String str : strs ) {
                    try {
                        long value = Long.parseLong(str);
                        if ( 0 < value ) {
                            al.add(value);
                        }
                    } catch ( Exception ex ) {
                        Log.ex( TAG, ex );
                    }
                }
                if ( 0 < al.size() ) {
                    result = new long[al.size()];
                    Iterator<Long> iter = al.iterator();
                    for ( int ii = 0; iter.hasNext(); ++ii ) {
                        result[ii] = 60 * iter.next();
                    }
                }
                s_lastStr = pref;
                s_lastIntervals = result;
            }
        }

        if ( null == result ) {
            result = NAG_INTERVAL_SECONDS;
        }
        return result;
    }

    private static String formatMillis( Context context, long millis )
    {
        long seconds = millis / 1000;
        ArrayList<String> results = new ArrayList<>();
        for ( int[] datum : s_fmtData ) {
            long val = seconds / datum[0];
            if ( 1 <= val ) {
                results.add( LocUtils.getQuantityString( context, datum[1],
                                                         (int)val, val ) );
                seconds %= datum[0];
            }
        }
        String result = TextUtils.join( ", ", results );
        return result;
    }

    private static boolean getNagsDisabled( Context context )
    {
        if ( null == s_nagsDisabledNet ) {
            boolean nagsDisabled =
                XWPrefs.getPrefsBoolean( context, R.string.key_disable_nag,
                                         false );
            s_nagsDisabledNet = new Boolean( nagsDisabled );
        }
        if ( null == s_nagsDisabledSolo ) {
            boolean nagsDisabled =
                XWPrefs.getPrefsBoolean( context, R.string.key_disable_nag_solo,
                                         true );
            s_nagsDisabledSolo = new Boolean( nagsDisabled );
        }
        return s_nagsDisabledNet && s_nagsDisabledSolo;
    }

    public static void resetNagsDisabled( Context context )
    {
        s_nagsDisabledNet = s_nagsDisabledSolo = null;
        restartTimer( context );
    }
}
