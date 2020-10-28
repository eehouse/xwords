/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

public class OnBootReceiver extends BroadcastReceiver {
    private static final String TAG = OnBootReceiver.class.getSimpleName();

    @Override
    public void onReceive( Context context, Intent intent )
    {
        if ( null != intent ) {
            String action = intent.getAction();
            Log.d( TAG, "got %s", action );
            switch( action ) {
            case Intent.ACTION_MY_PACKAGE_REPLACED:
            case Intent.ACTION_BOOT_COMPLETED:
                startTimers( context );
                break;
            }
        }
    }

    protected static void startTimers( Context context )
    {
        NagTurnReceiver.restartTimer( context );
        TimerReceiver.setTimer( context, true );
        SMSResendReceiver.setTimer( context );
    }
}
