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

    public interface HandleRelaysIface {
        void HandleRelaysIDs( final String[] relayIDs );
    }

    private static HashSet<Activity> s_running = new HashSet<Activity>();
    private static HandleRelaysIface s_handler;

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        Utils.logf( "DispatchNotify.onCreate()" );
        super.onCreate( savedInstanceState );

        Intent intent = getIntent();
        String[] relayIDs = 
            intent.getStringArrayExtra( getString(R.string.relayids_extra) );

        if ( null != s_handler ) {
            Utils.logf( "calling s_handler" );
            s_handler.HandleRelaysIDs( relayIDs );
        } else if ( s_running.isEmpty() ) {
            Utils.logf( "DispatchNotify: nothing running" );
            startActivity( new Intent( this, GamesList.class ) );
        } else {
            Utils.logf( "DispatchNotify: something running" );

            String ids = "new moves available; need to inval";
            for ( String id : relayIDs ) {
                ids += " " + id ;
            }

            // Toast.makeText( this, ids, Toast.LENGTH_SHORT).show();

            // for ( Activity activity : s_running ) {
            //     if ( activity instanceof DispatchNotify.HandleRelaysIface ) {
            //         DispatchNotify.HandleRelaysIface iface =
            //             (DispatchNotify.HandleRelaysIface)activity;
            //         iface.HandleRelaysIDs( relayIDs );
            //     }
            // }
        }

        finish();
    }

    @Override
    protected void onNewIntent( Intent intent )
    {
        Utils.logf( "DispatchNotify.onNewIntent() called" );
    }

    public static void SetRunning( Activity running )
    {
        s_running.add( running );
    }

    public static void ClearRunning( Activity running )
    {
        s_running.remove( running );
    }

    public static void SetRelayIDsHandler( HandleRelaysIface iface )
    {
        s_handler = iface;
    }
}
