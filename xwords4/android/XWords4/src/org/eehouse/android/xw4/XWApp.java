/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2010 - 2011 by Eric House (xwords@eehouse.org).  All
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

import android.app.Application;
import android.content.Context;
import android.os.Build;
import java.util.UUID;

import org.eehouse.android.xw4.jni.XwJNI;

public class XWApp extends Application {

    public static final boolean BTSUPPORTED = false;
    public static final boolean SMSSUPPORTED = false;

    private static UUID s_UUID = null;
    private static Boolean s_onEmulator = null;

    @Override
    public void onCreate()
    {
        super.onCreate();

        // This one line should always get logged even if logging is
        // off -- because logging is on by default until logEnable is
        // called.
        DbgUtils.logf( "XWApp.onCreate(); git_rev=%s", 
                       getString( R.string.git_rev ) );
        DbgUtils.logEnable( this );

        RelayReceiver.RestartTimer( this );
        BTService.startService( this );
    }

    public static UUID getAppUUID()
    {
        if ( null == s_UUID ) {
            s_UUID = UUID.fromString( XwJNI.comms_getUUID() );
        }
        return s_UUID;
    }

    public static String getAppName( Context context ) 
    {
        return context.getString( R.string.app_name );
    }

    public static boolean onEmulator()
    {
        if ( null == s_onEmulator ) {
            s_onEmulator = new Boolean( "google_sdk".equals(Build.MODEL) );
        }
        return s_onEmulator;
    }
}
