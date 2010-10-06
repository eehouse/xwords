/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
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

import android.app.Activity;
import android.os.Bundle;
import android.content.Intent;
import android.content.Context;
import android.app.AlarmManager;
import android.app.PendingIntent;

import org.eehouse.android.xw4.jni.CommonPrefs;

public class RelayActivity extends Activity {
    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );

        Intent service = new Intent(this, RelayService.class );
        startService( service );

        finish();
    }

    public static void RestartTimer( Context context )
    {
        RestartTimer( context, 
                      1000 * CommonPrefs.getProxyInterval( context ) );
    }

    public static void RestartTimer( Context context, long interval_millis )
    {
        AlarmManager am =
            (AlarmManager)context.getSystemService( Context.ALARM_SERVICE );
        PendingIntent intent = 
            PendingIntent.getActivity( context, 0, 
                                       new Intent(context, 
                                                  RelayActivity.class), 0);

        if ( interval_millis > 0 ) {
            Utils.logf( "setting alarm for %d millis", interval_millis );
            am.setInexactRepeating( AlarmManager.ELAPSED_REALTIME_WAKEUP, 
                                    0, // first firing
                                    interval_millis, intent );
        } else {
            am.cancel( intent );
        }
    }
}
