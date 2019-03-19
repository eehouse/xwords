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

import android.os.Build;
import android.content.Context;
import java.util.HashSet;
import java.util.Set;
import android.app.NotificationChannel;
import android.app.NotificationManager;

public class Channels {

    enum ID {
        NBSPROXY(R.string.nbsproxy_channel_expl,
                 NotificationManager.IMPORTANCE_LOW),
        GAME_EVENT(R.string.gameevent_channel_expl,
                   NotificationManager.IMPORTANCE_LOW),
        SERVICE_STALL(R.string.servicestall_channel_expl,
                      NotificationManager.IMPORTANCE_LOW);

        private int mExpl;
        private int mImportance;
        private ID( int expl, int imp )
        {
            mExpl = expl;
            mImportance = imp;
        }

        public int getDesc() { return mExpl; }
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
}
