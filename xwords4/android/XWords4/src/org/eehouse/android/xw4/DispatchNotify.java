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
import android.content.Intent;
import android.content.Context;
import android.app.AlarmManager;
import android.app.PendingIntent;
import android.widget.Toast;
import android.os.Bundle;
import java.util.HashSet;

import org.eehouse.android.xw4.jni.CommonPrefs;

public class DispatchNotify extends Activity {
    private static HashSet<Activity> s_running = new HashSet<Activity>();

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );

        if ( s_running.isEmpty() ) {
            Utils.logf( "DispatchNotify: nothing running" );
            startActivity( new Intent( this, GamesList.class ) );
        } else {
            Utils.logf( "DispatchNotify: something running" );
            Intent intent = getIntent();
            String[] relayIDs = 
                intent.getStringArrayExtra(getString(R.string.relayids_extra));
            String ids = "new moves available; need to inval";
            for ( String id : relayIDs ) {
                ids += " " + id ;
            }

            Toast.makeText( this, ids, Toast.LENGTH_SHORT).show();
        }

        finish();
    }

    public static void SetRunning( Activity running )
    {
        s_running.add( running );
    }

    public static void ClearRunning( Activity running )
    {
        s_running.remove( running );
    }

}
