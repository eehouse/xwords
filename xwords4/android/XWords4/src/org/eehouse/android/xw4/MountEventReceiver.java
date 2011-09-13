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
import java.util.HashSet;
import java.util.Iterator;

public class MountEventReceiver extends BroadcastReceiver {

    public interface SDCardNotifiee {
        void cardMounted( boolean nowMounted );
    }

    private static HashSet<SDCardNotifiee> s_procs = new HashSet<SDCardNotifiee>();

    @Override
    public void onReceive( Context context, Intent intent )
    {
        Utils.logf( "MountEventReceiver.onReceive(%s)", intent.getAction() );
        synchronized( s_procs ) {
            do {
                if ( s_procs.isEmpty() ) {
                    break;
                }

                boolean mounted;
                String action = intent.getAction();
                if ( action.equals( Intent.ACTION_MEDIA_MOUNTED ) ) {
                    mounted = true;
                } else if ( action.equals( Intent.ACTION_MEDIA_EJECT ) ) {
                    mounted = false;
                } else {
                    break;
                }
                Iterator<SDCardNotifiee> iter = s_procs.iterator();
                while ( iter.hasNext() ) {
                    iter.next().cardMounted( mounted );
                }
            } while ( false );
        }
    }

    public static void register( SDCardNotifiee proc )
    {
        synchronized( s_procs ) {
            s_procs.add( proc );
        }
    }

    public static void unregister( SDCardNotifiee proc )
    {
        synchronized( s_procs ) {
            s_procs.remove( proc );
        }
    }
}
