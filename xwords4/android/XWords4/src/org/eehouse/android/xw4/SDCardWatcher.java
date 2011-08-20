/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
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

package org.eehouse.android.xw4;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

public class SDCardWatcher {
    public interface SDCardNotifiee {
        void cardMounted();
    }

    private UmountReceiver m_rcvr;
    private Context m_context;
    private SDCardNotifiee m_notifiee;

    private class UmountReceiver extends BroadcastReceiver {
        @Override
        public void onReceive( Context context, Intent intent ) 
        {
            if ( intent.getAction().
                 equals( Intent.ACTION_MEDIA_MOUNTED ) ) {
                m_notifiee.cardMounted();
            }
        }
    }

    public SDCardWatcher( Context context, SDCardNotifiee notifiee )
    {
        m_context = context;
        m_rcvr = new UmountReceiver();
        m_notifiee = notifiee;

        IntentFilter filter = 
            new IntentFilter( Intent.ACTION_MEDIA_MOUNTED );
        // filter.addAction( Intent.ACTION_MEDIA_UNMOUNTED );
        // filter.addAction( Intent.ACTION_MEDIA_EJECT );
        filter.addDataScheme( "file" );

        /*Intent intent = */context.getApplicationContext().
            registerReceiver( m_rcvr, filter );
    }

    public void close()
    {
        m_context.getApplicationContext().unregisterReceiver( m_rcvr );
    }
}
