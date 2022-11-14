/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2019 by Eric House (xwords@eehouse.org).  All rights reserved.
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
import android.graphics.Bitmap;
import android.os.SystemClock;

import java.text.DateFormat;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;

import org.eehouse.android.xw4.DBUtils.GameChangeType;
import org.eehouse.android.xw4.jni.CurGameInfo;
import org.eehouse.android.xw4.jni.JNIThread;
import org.eehouse.android.xw4.jni.XwJNI.GamePtr;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;

/**
 * This class owns the problem of timers in duplicate-mode games. Unlike the
 * existing timers that run only when a game is open and visible, they run
 * with the clock for any game where it's a local player's turn. So this
 * module needs to be aware of all games in that state and to be counting
 * their timers down at all times. For each game for which a timer's running
 * it's either 1) sending updates to the game (if it's open) OR 2) keeping an
 * unhideable notification open with the relevant time counting down.
 */

public class DupeModeTimer extends BroadcastReceiver {
    private static final String TAG = DupeModeTimer.class.getSimpleName();

    private static final Channels.ID sMyChannel = Channels.ID.DUP_TIMER_RUNNING;

    private static RowidQueue sQueue;
    private static Map<Long, Integer> sDirtyVals = new HashMap<>();

    private static DateFormat s_df =
        DateFormat.getTimeInstance( /*DateFormat.SHORT*/ );
    private static long sCurTimer = Long.MAX_VALUE;

    static {
        sQueue = new RowidQueue();
        sQueue.start();

        DBUtils.setDBChangeListener( new DBUtils.DBChangeListener() {
                @Override
                public void gameSaved( Context context, long rowid,
                                       GameChangeType change )
                {
                    // Log.d( TAG, "gameSaved(rowid=%d,change=%s) called", rowid, change );
                    switch( change ) {
                    case GAME_CHANGED:
                    case GAME_CREATED:
                        synchronized ( sDirtyVals ) {
                            if ( sDirtyVals.containsKey( rowid ) ) {
                                sQueue.addOne( context, rowid );
                            } else {
                                // Log.d( TAG, "skipping; not dirty" );
                            }
                        }
                        break;
                    case GAME_DELETED:
                        cancelNotification( context, rowid );
                        break;
                    }
                }
            } );

    }

    @Override
    public void onReceive( Context context, Intent intent )
    {
        Log.d( TAG, "onReceive()" );
        sCurTimer = Long.MAX_VALUE; // clear so we'll set again
        sQueue.addAll( context );
    }

    /**
     * Called when
     */
    static void init( Context context )
    {
        Log.d( TAG, "init()" );
        sQueue.addAll( context );
    }

    public static void gameOpened( Context context, long rowid )
    {
        Log.d( TAG, "gameOpened(%s, %d)", context, rowid );
        sQueue.addOne( context, rowid );
    }

    public static void gameClosed( Context context, long rowid )
    {
        Log.d( TAG, "gameClosed(%s, %d)", context, rowid );
        sQueue.addOne( context, rowid );
    }

    // public static void timerPauseChanged( Context context, long rowid )
    // {
    //     sQueue.addOne( context, rowid );
    // }

    public static void timerChanged( Context context, int gameID, int newVal )
    {
        long[] rowids = DBUtils.getRowIDsFor( context, gameID );
        for ( long rowid : rowids ) {
            Log.d( TAG, "timerChanged(rowid=%d, newVal=%d)", rowid, newVal );
            synchronized ( sDirtyVals ) {
                sDirtyVals.put( rowid, newVal );
            }
        }
    }

    private static void postNotification( Context context, long rowid, long when )
    {
        Log.d( TAG, "postNotification(rowid=%d)", rowid );
        if ( !JNIThread.gameIsOpen( rowid ) ) {
            String title = LocUtils.getString( context, R.string.dup_notif_title );
            if ( BuildConfig.DEBUG ) {
                title += " (" + rowid + ")";
            }
            String body = context.getString( R.string.dup_notif_title_fmt,
                                             s_df.format( new Date( 1000 * when ) ) );
            Intent intent = GamesListDelegate.makeRowidIntent( context, rowid );

            Intent pauseIntent = GamesListDelegate.makeRowidIntent( context, rowid );
            pauseIntent.putExtra( BoardDelegate.PAUSER_KEY, true );
            Utils.postOngoingNotification( context, intent, title, body,
                                           rowid, sMyChannel,
                                           pauseIntent, R.string.board_menu_game_pause );
        } else {
            Log.d( TAG, "postOngoingNotification(%d): open, so skipping", rowid );
        }
    }

    private static void cancelNotification( Context context, long rowid )
    {
        Log.d( TAG, "cancelNotification(rowid=%d)", rowid );
        Utils.cancelNotification( context, sMyChannel, rowid );
    }

    private static void setTimer( Context context, long whenSeconds )
    {
        if ( whenSeconds < sCurTimer ) {
            sCurTimer = whenSeconds;
            Intent intent = new Intent( context, DupeModeTimer.class );
            PendingIntent pi = PendingIntent.getBroadcast( context, 0, intent,
                                                           PendingIntent.FLAG_IMMUTABLE );

            long now = Utils.getCurSeconds();
            long fire_millis = SystemClock.elapsedRealtime()
                + (1000 * (whenSeconds - now));

            ((AlarmManager)context.getSystemService( Context.ALARM_SERVICE ))
                .set( AlarmManager.ELAPSED_REALTIME, fire_millis, pi );
        }
    }

    private static class RowidQueue extends Thread {
        private Set<Long> mSet = new HashSet<>();
        private Context mContext;

        void addAll( Context context )
        {
            addOne( context, 0 );
        }

        synchronized void addOne( Context context, long rowid )
        {
            mContext = context;
            synchronized ( mSet ) {
                mSet.add( rowid );
                mSet.notify();
            }
        }

        @Override
        public void run()
        {
            long rowid = DBUtils.ROWID_NOTFOUND;
            for ( ; ; ) {
                synchronized( mSet ) {
                    mSet.remove( rowid );
                    if ( 0 == mSet.size() ) {
                        try {
                            mSet.wait();
                            Assert.assertTrue( 0 < mSet.size() );
                        } catch ( InterruptedException ie ) {
                            break;
                        }
                    }
                    rowid = mSet.iterator().next();
                }
                inventoryGames( rowid );
            }
        }

        private void inventoryGames( long onerow )
        {
            Log.d( TAG, "inventoryGames(%d)", onerow );
            Map<Long, Integer> dupeGames = onerow == 0
                ? DBUtils.getDupModeGames( mContext )
                : DBUtils.getDupModeGames( mContext, onerow );

            Log.d( TAG, "inventoryGames(%s)", dupeGames );
            long now = Utils.getCurSeconds();
            long minTimer = sCurTimer;

            for ( long rowid : dupeGames.keySet() ) {
                int timerFires = dupeGames.get( rowid );

                synchronized ( sDirtyVals ) {
                    if ( sDirtyVals.containsKey(rowid) && timerFires == sDirtyVals.get(rowid) ) {
                        sDirtyVals.remove(rowid);
                    }
                }

                if ( timerFires > now ) {
                    Log.d( TAG, "found dupe game with %d seconds left",
                           timerFires - now );
                    postNotification( mContext, rowid, timerFires );
                    if ( timerFires < minTimer ) {
                        minTimer = timerFires;
                    }
                } else {
                    cancelNotification( mContext, rowid );
                    Log.d( TAG, "found dupe game with expired or inactive timer" );
                    if ( timerFires > 0 ) {
                        giveGameTime( rowid );
                    }
                }
            }

            setTimer( mContext, minTimer );
        }

        private void giveGameTime( long rowid )
        {
            Log.d( TAG, "giveGameTime(%d)() starting", rowid );
            try ( GameLock lock = GameLock.tryLock( rowid ) ) {
                if ( null != lock ) {
                    CurGameInfo gi = new CurGameInfo( mContext );
                    MultiMsgSink sink = new MultiMsgSink( mContext, rowid );
                    try ( final XwJNI.GamePtr gamePtr = GameUtils
                          .loadMakeGame( mContext, gi, sink, lock ) ) { // calls getJNI()
                        Log.d( TAG, "got gamePtr: %H", gamePtr );
                        if ( null != gamePtr ) {
                            boolean draw = false;
                            for ( int ii = 0; ii < 3; ++ii ) {
                                draw = XwJNI.server_do( gamePtr ) || draw;
                            }

                            GameUtils.saveGame( mContext, gamePtr, gi, lock, false );

                            if ( draw && XWPrefs.getThumbEnabled( mContext ) ) {
                                Bitmap bitmap = GameUtils
                                    .takeSnapshot( mContext, gamePtr, gi );
                                DBUtils.saveThumbnail( mContext, lock, bitmap );
                            }
                        }
                    }
                }
            }
            Log.d( TAG, "giveGameTime(%d)() DONE", rowid );
        }
    }
}
