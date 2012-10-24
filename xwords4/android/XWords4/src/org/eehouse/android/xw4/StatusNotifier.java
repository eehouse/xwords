/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2012 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;

public class StatusNotifier {
    private int m_id;
    private NotificationManager m_mgr;
    private Context m_context;

    public StatusNotifier( Context context, String msg, int id )
    {
        m_context = context;
        m_id = id;

        Notification notification = 
            new Notification( R.drawable.icon48x48, msg, 
                              System.currentTimeMillis() );
        notification.flags = notification.flags |= Notification.FLAG_AUTO_CANCEL;
        PendingIntent pi = PendingIntent.getActivity( context, 0, 
                                                      new Intent(), 0 );
        notification.setLatestEventInfo( context, "", "", pi );

        m_mgr = (NotificationManager) 
            context.getSystemService( Context.NOTIFICATION_SERVICE );
        m_mgr.notify( id, notification );
    }

    // Will likely be called from background thread
    public void close()
    {
        m_mgr.cancel( m_id );
    }

}
