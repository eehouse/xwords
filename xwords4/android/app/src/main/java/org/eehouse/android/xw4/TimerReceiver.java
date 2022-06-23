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

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.SystemClock;
import android.text.TextUtils;

import java.io.Serializable;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;

public class TimerReceiver extends BroadcastReceiver {
    private static final String TAG = TimerReceiver.class.getSimpleName();
    private static final boolean VERBOSE = false;
    private static final String DATA_KEY = TAG + "/data";
    private static final String KEY_FIREWHEN = "FIREWHEN";
    private static final String KEY_BACKOFF = "BACKOFF";
    private static final String CLIENT_STATS = "stats";
    private static final String KEY_COUNT = "COUNT";
    private static final String KEY_NEXT_FIRE = "NEXTFIRE";
    private static final String KEY_NEXT_SPAN = "SPAN";
    private static final String KEY_WORST = "WORST";
    private static final String KEY_AVG_MISS = "AVG_MISS";
    private static final String KEY_AVG_SPAN = "AVG_SPAN";
    private static final long MIN_FUTURE = 2000L;
    private static final String KEY_TIMER_ID = "timerID";

    interface TimerCallback {
        public void timerFired( Context context );
        public long incrementBackoff( long prevBackoff );
    }

    private interface WithData {
        void withData( Data data );
    }

    private static class Data implements Serializable {
        private Map<String, Map<String, Long>> mFields;
        private transient boolean mDirty = false;
        private transient int mRefcount = 0;

        Data() { mFields = new HashMap<>(); }

        Data get()
        {
            ++mRefcount;
            // Log.d( TAG, "get(): refcount now %d", mRefcount );
            return this;
        }

        void put( Context context )
        {
            Assert.assertTrueNR( 0 <= mRefcount );
            --mRefcount;
            // Log.d( TAG, "put(): refcount now %d", mRefcount );
            if ( 0 == mRefcount && mDirty ) {
                store( context, this );
                mDirty = false;
            }
        }

        Set<String> clients() { return mFields.keySet(); }

        void remove( String client ) {
            mFields.remove( client );
            mDirty = true;
            Log.d( TAG, "remove(%s)", client );
        }

        void setFor( TimerCallback client, String key, long val )
        {
            setFor( className(client), key, val );
        }

        void setFor( String client, String key, long val )
        {
            if ( ! mFields.containsKey( client ) ) {
                mFields.put( client, new HashMap<String, Long>() );
            }
            Map<String, Long> map = mFields.get(client);
            if ( !map.containsKey( key ) || val != map.get( key ) ) {
                map.put( key, val );
                mDirty = true;
            }
        }

        long getFor( TimerCallback client, String key, long dflt )
        {
            return getFor( className(client), key, dflt );
        }

        long getFor( String client, String key, long dflt )
        {
            long result = dflt;
            if ( mFields.containsKey( client ) ) {
                Map<String, Long> map = mFields.get( client );
                if ( map.containsKey( key ) ) {
                    result = map.get( key );
                }
            }
            return result;
        }
    }

    // If it's a number > 30 years past Epoch, format it as a date
    private static final SimpleDateFormat sFmt = new SimpleDateFormat("MMM dd HH:mm:ss ");
    private static String fmtLong( long dateOrNum )
    {
        String result;
        if ( dateOrNum < 1000 * 60 * 60 * 24 * 365 * 30 ) {
            result = String.format( "%d", dateOrNum );
        } else {
            result = sFmt.format( new Date(dateOrNum) );
        }
        return result;
    }

    private static String toString( Data data )
    {
        List<String> all = new ArrayList<>();
        for ( String client : data.mFields.keySet() ) {
            List<String> ones = new ArrayList<>();
            Map<String, Long> map = data.mFields.get( client );
            for ( String key : map.keySet() ) {
                ones.add( String.format("%s: %s", key, fmtLong(map.get( key ))) );
            }
            String one = String.format("{%s}", TextUtils.join(", ", ones ) );
            all.add( String.format("%s: %s", getSimpleName(client), one ) );
        }
        return String.format("{%s}", TextUtils.join(", ", all ) );
    }

    @Override
    public void onReceive( final Context context, Intent intent )
    {
        long timerID = intent.getLongExtra( KEY_TIMER_ID, -1 );
        onReceiveImpl( context, timerID, TAG );
    }

    static void jobTimerFired( Context context, long timerID, String src )
    {
        onReceiveImpl( context, timerID, src );
    }

    private static void onReceiveImpl( final Context context,
                                       long timerID, String src )
    {
        Log.d( TAG, "onReceiveImpl(timerID=%d, src=%s)", timerID, src );
        load( context, new WithData() {
                @Override
                public void withData( Data data ) {
                    updateStats( context, data );
                    data.setFor( CLIENT_STATS, KEY_NEXT_FIRE, 0 );
                    Set<TimerCallback> fired = fireExpiredTimers( context, data );
                    incrementBackoffs( data, fired );
                    setNextTimer( context, data );
                }
            } );
        Log.d( TAG, "onReceiveImpl(timerID=%d, src=%s) DONE", timerID, src );
    }

    static String statsStr( Context context )
    {
        final StringBuffer sb = new StringBuffer();
        if ( BuildConfig.NON_RELEASE || XWPrefs.getDebugEnabled( context ) ) {
            load( context, new WithData() {
                    @Override
                    public void withData( Data data ) {
                        // TreeMap to sort by timer fire time
                        TreeMap<Long, String> tmpMap = new TreeMap<>();
                        for ( String client: data.clients() ) {
                            long nextFire = data.getFor( client, KEY_FIREWHEN, 0 );
                            if ( 0 != nextFire ) {
                                tmpMap.put( nextFire, client );
                            }
                        }
                        sb.append("Next timers:\n");
                        for ( Map.Entry<Long, String> entry: tmpMap.entrySet() ) {
                            sb.append( getSimpleName(entry.getValue())).append(": ")
                                .append(fmtLong(entry.getKey()))
                                .append("\n");
                        }

                        long count = data.getFor( CLIENT_STATS, KEY_COUNT, 0 );
                        sb.append( "\nTimers fired: ").append(count).append("\n");
                        if ( 0 < count ) {
                            long avgMiss = data.getFor( CLIENT_STATS, KEY_AVG_MISS, 0 );
                            sb.append("Avg delay: ")
                                .append(String.format( "%.1fs\n", avgMiss/1000f));
                            long worst = data.getFor( CLIENT_STATS, KEY_WORST, 0 );
                            sb.append("Worst delay: ")
                                .append(String.format( "%.1fs\n", worst/1000f));
                            long avgSpan = data.getFor( CLIENT_STATS, KEY_AVG_SPAN, 0 );
                            sb.append("Avg interval: ").append((avgSpan+500)/1000).append("s\n");
                        }
                    }
                } );
        }
        return sb.toString();
    }

    static void clearStats( Context context )
    {
        load( context, new WithData() {
                @Override
                public void withData( Data data ) {
                    data.remove( CLIENT_STATS );
                }
            } );
    }

    static void setBackoff( final Context context, final TimerCallback cback,
                            final long backoffMS )
    {
        Log.d( TAG, "setBackoff(client=%s, backoff=%ds)", className(cback), backoffMS/1000 );
        load( context, new WithData() {
                @Override
                public void withData( Data data ) {
                    data.setFor( cback, KEY_BACKOFF, backoffMS );
                    setTimer( context, data, backoffMS, true, cback );
                }
            } );
    }

    // This one's public. Sets a one-time timer. Any backoff or re-set the
    // client has to handle
    static void setTimerRelative( final Context context, final TimerCallback cback,
                                  final long waitMS )
    {
        long fireMS = waitMS + System.currentTimeMillis();
        setTimer( context, cback, fireMS );
    }

    static void setTimer( final Context context, final TimerCallback cback,
                          final long fireMS )
    {
        load( context, new WithData() {
                @Override
                public void withData( Data data ) {
                    data.setFor( cback, KEY_FIREWHEN, fireMS );
                    setNextTimer( context, data );
                }
            } );
    }

    static void allTimersFired( Context context )
    {
        Set<TimerCallback> callbacks = getCallbacks( context );
        for ( TimerCallback callback : callbacks ) {
            callback.timerFired( context );
        }
    }

    static Map<String, TimerCallback> sCallbacks = new HashMap<>();
    private synchronized static Set<TimerCallback> getCallbacks( Context context )
    {
        Set<TimerCallback> results = new HashSet<>();
        for ( TimerCallback callback : sCallbacks.values() ) {
            results.add( callback );
        }
        return results;
    }

    private synchronized static TimerCallback getCallback( String client )
    {
        TimerCallback callback;
        try {
            callback = sCallbacks.get( client );
            if ( null == callback ) {
                Class<?> clazz = Class.forName( client );
                callback = (TimerCallback)clazz.newInstance();
                sCallbacks.put( client, callback );
            }
        } catch ( Exception ex ) {
            callback = null;
            Log.ex( TAG, ex );
        }

        return callback;
    }

    private static Set<TimerCallback> fireExpiredTimers( Context context, Data data )
    {
        Set<String> clients = new HashSet<>();
        long now = System.currentTimeMillis();
        for ( String client: data.clients() ) {
            long fireTime = data.getFor( client, KEY_FIREWHEN, 0 );
            // Assert.assertTrueNR( fireTime < now ); <- firing
            if ( 0 != fireTime && fireTime <= now ) {
                clients.add( client );
                Log.d( TAG, "fireExpiredTimers(): firing %s %d ms late", client,
                       now - fireTime );
            }
        }

        Set<TimerCallback> callees = new HashSet<>();
        for ( String client : clients ) {
            TimerCallback callback = getCallback( client );
            if ( null == callback ) { // class no longer exists
                data.remove( client );
            } else {
                data.setFor( client, KEY_FIREWHEN, 0 );
                callback.timerFired( context );
                callees.add( callback );
            }
        }

        return callees;
    }

    private static void incrementBackoffs( Data data, Set<TimerCallback> callbacks )
    {
        long now = System.currentTimeMillis();
        for ( TimerCallback callback: callbacks ) {
            long backoff = data.getFor( callback, KEY_BACKOFF, 0 );
            if ( 0 != backoff ) {
                backoff = callback.incrementBackoff( backoff );
                data.setFor( callback, KEY_BACKOFF, backoff );
                data.setFor( callback, KEY_FIREWHEN, now + backoff );
            }
        }
    }

    private static void setNextTimer( Context context, Data data )
    {
        long firstFireTime = Long.MAX_VALUE;
        String firstClient = null;
        long now = System.currentTimeMillis();
        for ( String client: data.clients() ) {
            long fireTime = data.getFor( client, KEY_FIREWHEN, 0 );
            if ( 0 != fireTime ) {
                // Log.d( TAG, "setNextTimer(): %s: %ds from now", getSimpleName(client),
                //        (fireTime - now)/1000 );
                if ( fireTime < firstFireTime ) {
                    firstFireTime = fireTime;
                    firstClient = client;
                }
            }
        }

        if ( null != firstClient ) {
            final long curNextFire = data.getFor( CLIENT_STATS, KEY_NEXT_FIRE, 0 );
            if ( 1000L < Math.abs( firstFireTime - curNextFire ) ) {
                if ( firstFireTime - now < MIN_FUTURE ) { // Less than a 2 seconds in the future?
                    Log.d( TAG, "setNextTimer(): moving firstFireTime (for %s) to the future: %s -> %s",
                           firstClient, fmtLong(firstFireTime), fmtLong(now + MIN_FUTURE) );
                    firstFireTime = now + MIN_FUTURE;
                    data.setFor( firstClient, KEY_FIREWHEN, firstFireTime );
                }

                long delayMS = firstFireTime - now;
                data.setFor( CLIENT_STATS, KEY_NEXT_FIRE, firstFireTime );
                data.setFor( CLIENT_STATS, KEY_NEXT_SPAN, delayMS );
                long timerID = 1 + data.getFor( CLIENT_STATS, KEY_TIMER_ID, 0 );
                data.setFor( CLIENT_STATS, KEY_TIMER_ID, timerID );

                AlarmManager am =
                    (AlarmManager)context.getSystemService( Context.ALARM_SERVICE );
                Intent intent = new Intent( context, TimerReceiver.class );
                intent.putExtra( KEY_TIMER_ID, timerID );
                PendingIntent pi = PendingIntent.getBroadcast( context, 0, intent,
                                                               PendingIntent.FLAG_CANCEL_CURRENT );
                am.set( AlarmManager.RTC_WAKEUP, firstFireTime, pi );

                setJobTimerIf( context, delayMS, timerID );

                if ( VERBOSE ) {
                    Log.d( TAG, "setNextTimer(): SET id %d for %s at %s", timerID,
                           getSimpleName(firstClient), fmtLong(firstFireTime) );
                }
            // } else {
            //     Assert.assertTrueNR( 0 != curNextFire );
            //     long diff = Math.abs( firstFireTime - curNextFire );
            //     Log.d( TAG, "not setting timer for %s: firstFireTime: %d,"
            //            + " curNextFire: %d; diff: %d",
            //            getSimpleName(firstClient), firstFireTime, curNextFire, diff );
            }
        }
    }

    private static void setJobTimerIf(Context context, long delayMS, long timerID)
    {
        if ( Build.VERSION_CODES.LOLLIPOP <= Build.VERSION.SDK_INT ) {
            TimerJobReceiver.setTimer( context, delayMS, timerID );
        }
    }

    private static void setTimer( Context context, Data data, long backoff,
                                  boolean force, TimerCallback cback )
    {
        String client = className( cback );
        if ( !force ) {
            long curBackoff = data.getFor( client, KEY_BACKOFF, backoff );
            long nextFire = data.getFor( client, KEY_FIREWHEN, 0 );
            force = 0 == nextFire || backoff != curBackoff;
        }
        if ( VERBOSE ) {
            Log.d( TAG, "setTimer(clazz=%s, force=%b, curBackoff=%d)",
                   getSimpleName(client), force, backoff );
        }
        if ( force ) {
            long now = System.currentTimeMillis();
            long fireMillis = now + backoff;
            data.setFor( client, KEY_FIREWHEN, fireMillis );
        }

        setNextTimer( context, data );
    }

    // What to measure? By how much are timer fires delayed? How's that as a
    // percentage of what we wanted?
    private static void updateStats( Context context, Data data )
    {
        if ( BuildConfig.NON_RELEASE || XWPrefs.getDebugEnabled( context ) ) {
            final long target = data.getFor( CLIENT_STATS, KEY_NEXT_FIRE, 0 );
            // Ignore for stats purposes if target not yet set
            if ( 0 < target ) {
                final long oldCount = data.getFor( CLIENT_STATS, KEY_COUNT, 0 );
                data.setFor( CLIENT_STATS, KEY_COUNT, oldCount + 1 );

                final long now = System.currentTimeMillis();

                if ( 0 < target ) {
                    long missedByMS = now - target;
                    long worstMiss = data.getFor( CLIENT_STATS, KEY_WORST, 0 );
                    if ( worstMiss < missedByMS ) {
                        data.setFor( CLIENT_STATS, KEY_WORST, missedByMS );
                    }

                    updateAverage( data, KEY_AVG_MISS, oldCount, missedByMS );
                    final long targetSpan = data.getFor( CLIENT_STATS, KEY_NEXT_SPAN, 0 );
                    updateAverage( data, KEY_AVG_SPAN, oldCount, targetSpan );
                }
            }
        }
    }

    private static void updateAverage( Data data, String key,
                                       long oldCount, long newVal )
    {
        long avg = data.getFor( CLIENT_STATS, key, 0 );
        avg = ((avg * oldCount) + newVal) / (oldCount + 1);
        data.setFor( CLIENT_STATS, key, avg );
    }

    private static String className( TimerCallback cbck )
    {
        return cbck.getClass().getName();
    }

    private static String getSimpleName( String client )
    {
        String[] parts = TextUtils.split( client, "\\." );
        String end = parts[parts.length-1];
        return TextUtils.split( end, "\\$" )[0];
    }

    private static Data[] sDataWrapper = {null};
    static void load( Context context, WithData proc )
    {
        synchronized ( sDataWrapper ) {
            Data data = sDataWrapper[0];
            if ( null == data ) {
                try {
                    data = (Data)DBUtils.getSerializableFor( context, DATA_KEY );
                    if ( VERBOSE ) {
                        Log.d( TAG, "load(): loaded: %s", toString(data) );
                    }
                } catch ( Exception ex ) {
                    data = null;
                }

                if ( null == data ) {
                    data = new Data();
                }
                sDataWrapper[0] = data;
            }
            proc.withData( data.get() );
            data.put( context );
        }
    }

    private static void store( Context context, Data data )
    {
        DBUtils.setSerializableFor( context, DATA_KEY, data );
        if ( VERBOSE ) {
            Log.d( TAG, "store(): saved: %s", toString(data) );
        }
    }
}
