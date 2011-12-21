/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
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
import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Intent;
import android.net.Uri;
import android.widget.Toast;
import android.os.Bundle;
import java.util.HashSet;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.CommonPrefs;

public class DispatchNotify extends Activity {

    public static final String RELAYIDS_EXTRA = "relayids";

    public interface HandleRelaysIface {
        void HandleRelaysIDs( final String[] relayIDs );
        void HandleInvite( final Uri invite );
    }

    private static HashSet<HandleRelaysIface> s_running =
        new HashSet<HandleRelaysIface>();
    private static HandleRelaysIface s_handler;

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        boolean mustLaunch = false;
        super.onCreate( savedInstanceState );

        String[] relayIDs = getIntent().getStringArrayExtra( RELAYIDS_EXTRA );
        Uri data = getIntent().getData();

        if ( null != relayIDs ) {
            if ( !tryHandle( relayIDs ) ) {
                mustLaunch = true;
            }
        } else if ( null != data ) {
            if ( !tryHandle( data ) ) {
                mustLaunch = true;
            }
        }

        if ( mustLaunch ) {
            DbgUtils.logf( "DispatchNotify: nothing running" );
            Intent intent = new Intent( this, GamesList.class );

            // This combination of flags will bring an existing
            // GamesList instance to the front, killing any children
            // it has, or create a new one if none exists.  Coupled
            // with a "standard" launchMode it seems to work, meaning
            // both that the app preserves its stack in normal use
            // (you can go to Home with a stack of activities and
            // return to the top activity on that stack if you
            // relaunch the app) and that when I launch from here the
            // stack gets nuked and we don't get a second GamesList
            // instance.

            intent.setFlags( Intent.FLAG_ACTIVITY_CLEAR_TOP
                             | Intent.FLAG_ACTIVITY_NEW_TASK );
            if ( null != relayIDs ) {
                intent.putExtra( RELAYIDS_EXTRA, relayIDs );
            } else if ( null != data ) {
                intent.setData( data );
            } else {
                Assert.fail();
            }
            startActivity( intent );
        }

        finish();
    }

    public static void SetRunning( Activity running )
    {
        if ( running instanceof HandleRelaysIface ) {
            s_running.add( (HandleRelaysIface)running );
        }
    }

    public static void ClearRunning( Activity running )
    {
        if ( running instanceof HandleRelaysIface ) {
            s_running.remove( (HandleRelaysIface)running );
        }
    }

    public static void SetRelayIDsHandler( HandleRelaysIface iface )
    {
        s_handler = iface;
    }

    private static boolean tryHandle( Uri data )
    {
        boolean handled = false;
        if ( null != s_handler ) {
            // This means the GamesList activity is frontmost
            s_handler.HandleInvite( data );
            handled = true;
        } else {
            for ( HandleRelaysIface iface : s_running ) {
                iface.HandleInvite( data );
                handled = true;
            }
        }
        return handled;
    }

    public static boolean tryHandle( String[] relayIDs )
    {
        boolean handled = false;
        if ( null != s_handler ) {
            // This means the GamesList activity is frontmost
            s_handler.HandleRelaysIDs( relayIDs );
            handled = true;
        } else {
            for ( HandleRelaysIface iface : s_running ) {
                iface.HandleRelaysIDs( relayIDs );
                handled = true;
            }
        }
        return handled;
    }
}
