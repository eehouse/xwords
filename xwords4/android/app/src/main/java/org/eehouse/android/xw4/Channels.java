/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 - 2014 by Eric House (xwords@eehouse.org).  All
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

package org.eehouse.android.xw4;

import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Context;
import android.os.Build;

import java.io.Serializable;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

public class Channels {
    private static final String TAG = Channels.class.getSimpleName();

    public enum ID {
        NBSPROXY( R.string.nbsproxy_channel_expl )
        // HIGH seems to be required for sound
        ,GAME_EVENT( R.string.gameevent_channel_expl,
                     NotificationManager.IMPORTANCE_HIGH )
        ,DUP_TIMER_RUNNING( R.string.dup_timer_expl )
        ,DUP_PAUSED( R.string.dup_paused_expl )
        ;

        private int mExpl;
        private int mImportance;
        private ID( int expl, int imp )
        {
            mExpl = expl;
            mImportance = imp;
        }

        private ID( int expl )
        {
            this( expl, NotificationManager.IMPORTANCE_LOW );
        }

        public int getDesc() { return mExpl; }
        public int idFor( long rowid ) {
            return notificationId( rowid, this );
        }
        private int getImportance() { return mImportance; }
    }

    private static Set<ID> sChannelsMade = new HashSet<>();
    public static String getChannelID( Context context, ID id )
    {
        final String name = id.toString();
        if ( ! sChannelsMade.contains( id ) ) {
            sChannelsMade.add( id );
            if ( Build.VERSION.SDK_INT >= Build.VERSION_CODES.O ) {
                NotificationManager notMgr = (NotificationManager)
                    context.getSystemService( Context.NOTIFICATION_SERVICE );

                NotificationChannel channel = notMgr.getNotificationChannel( name );
                if ( channel == null ) {
                    String channelDescription = context.getString( id.getDesc() );
                    channel = new NotificationChannel( name, channelDescription,
                                                       id.getImportance() );
                    channel.enableVibration( true );
                    notMgr.createNotificationChannel( channel );
                }
            }
        }
        return name;
    }

    private static final String IDS_KEY = TAG + "/ids_key";

    private static class IdsData implements Serializable {
        HashMap<ID, HashMap<Long, Integer>> mMap = new HashMap<>();
        HashSet<Integer> mInts = new HashSet<>();

        int newID()
        {
            int result;
            for ( ; ; ) {
                int one = Utils.nextRandomInt();
                if ( !mInts.contains( one ) ) {
                    mInts.add( one );
                    result = one;
                    break;
                }
            }
            return result;
        }
    }
    private static IdsData sData;

    // I want each rowid to be able to have a notification active for it for
    // each channel. So let's try generating and storing random ints.
    private static int notificationId( long rowid, ID channel )
    {
        Context context = XWApp.getContext();
        int result;
        synchronized ( Channels.class ) {
            if ( null == sData ) {
                sData = (IdsData)DBUtils.getSerializableFor( context, IDS_KEY );
                if ( null == sData ) {
                    sData = new IdsData();
                }
            }
        }

        synchronized ( sData ) {
            boolean dirty = false;
            if ( ! sData.mMap.containsKey( channel ) ) {
                sData.mMap.put( channel, new HashMap<Long, Integer>() );
                dirty = true;
            }
            Map<Long, Integer> map = sData.mMap.get( channel );
            if ( ! map.containsKey( rowid ) ) {
                map.put( rowid, sData.newID() );
                dirty = true;
            }

            if ( dirty ) {
                DBUtils.setSerializableFor( context, IDS_KEY, sData );
            }

            result = map.get( rowid );
        }
        Log.d( TAG, "notificationId(%s, %d) => %d", channel, rowid, result );
        return result;
    }
}
