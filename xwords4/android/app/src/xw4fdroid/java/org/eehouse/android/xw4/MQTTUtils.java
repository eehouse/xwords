/* -*- compile-command: "find-and-gradle.sh -PuseCrashlytics insXw4dDeb"; -*- */
/*
 * Copyright 2020 by Eric House (xwords@eehouse.org).  All rights reserved.
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

import android.content.Context;

import org.eehouse.android.xw4.jni.CommsAddrRec;

public class MQTTUtils {
    private static final String TAG = MQTTUtils.class.getSimpleName();

    public static void init( Context context )
    {
        logUnimpl( "init" );
    }

    public static void onResume( Context context )
    {
        logUnimpl( "onResume" );
    }

    public static int send( Context context, String addressee, int gameID, byte[] buf )
    {
        logUnimpl( "send" );
        return -1;
    }

    public static void handleMessage( Context context, CommsAddrRec from,
                                      int gameID, byte[] data )
    {
        logUnimpl( "handleMessage" );
    }

    public static void handleGameGone( Context context, CommsAddrRec from, int gameID )
    {
        logUnimpl( "handleGameGone" );
    }

    public static void gameDied( String devID, int gameID )
    {
        logUnimpl( "gameDied" );
    }

    public static void timerFired( Context context )
    {
        logUnimpl( "timerFired" );
    }

    static void onConfigChanged( Context context )
    {
        logUnimpl( "onConfigChanged" );
    }

    public static void inviteRemote( Context context, String invitee, NetLaunchInfo nli )
    {
        logUnimpl( "inviteRemote" );
    }

    private static void logUnimpl( String name )
    {
        Log.d( TAG, "%s(): UNIMPLEMENTED", name );
    }

}
