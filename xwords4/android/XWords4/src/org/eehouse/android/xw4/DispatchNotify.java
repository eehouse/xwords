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
        String id = getString( R.string.relayids_extra );
        String[] relayIDs = intent.getStringArrayExtra( id );

        if ( !tryHandle( this, relayIDs ) ) {
            Utils.logf( "DispatchNotify: nothing running" );
            intent = new Intent( this, GamesList.class );
            intent.setFlags( Intent.FLAG_ACTIVITY_CLEAR_TOP );
            intent.putExtra( id, relayIDs );
            startActivity( intent );
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

    public static boolean tryHandle( Context context, String[] relayIDs )
    {
        Utils.logf( "tryHandle()" );
        boolean handled = false;
        if ( null != s_handler ) {
            // This means the GamesList activity is frontmost
            Utils.logf( "calling m_handler" );
            s_handler.HandleRelaysIDs( relayIDs );
            handled = true;
        } else {
            for ( Activity activity : s_running ) {
                if ( activity instanceof DispatchNotify.HandleRelaysIface ) {
                    DispatchNotify.HandleRelaysIface iface =
                        (DispatchNotify.HandleRelaysIface)activity;
                    iface.HandleRelaysIDs( relayIDs );
                    handled = true;
                }
            }
        }
        Utils.logf( "DispatchNotify.tryHandle()=>%s", handled?"true":"false" );
        return handled;
    }
}
