/* -*- compile-command: "find-and-gradle.sh installXw4Debug"; -*- */
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
import android.graphics.Color;
import android.os.Build;
import android.preference.PreferenceManager;

import org.eehouse.android.xw4.jni.XwJNI;

import java.util.UUID;

import junit.framework.Assert;

public class XWApp extends Application {
    private static final String TAG = XWApp.class.getSimpleName();

    public static final boolean BTSUPPORTED = true;
    public static final boolean GCMSUPPORTED = true;
    public static final boolean RELAYINVITE_SUPPORTED = false;
    public static final boolean ATTACH_SUPPORTED = false;
    public static final boolean LOG_LIFECYLE = false;
    public static final boolean DEBUG_EXP_TIMERS = false;
    public static final boolean GCM_IGNORED = false;
    public static final boolean UDP_ENABLED = true;
    public static final boolean SMS_INVITE_ENABLED = true;
    public static final boolean LOCUTILS_ENABLED = false;
    public static final boolean CONTEXT_MENUS_ENABLED = true;
    public static final boolean OFFER_DUALPANE = false;
    // BT class "COMPUTERS" includes tablets like the Nexus 9
    public static final boolean BT_SCAN_COMPUTERS = true;

    public static final String SMS_PUBLIC_HEADER = "-XW4";
    public static final int MAX_TRAY_TILES = 7; // comtypes.h
    public static final int SEL_COLOR = Color.argb( 0xFF, 0x09, 0x70, 0x93 );

    private static UUID s_UUID = null;
    private static Boolean s_onEmulator = null;
    private static Context s_context = null;

    @Override
    public void onCreate()
    {
        s_context = this;
        Assert.assertTrue( s_context == s_context.getApplicationContext() );
        super.onCreate();

        // This one line should always get logged even if logging is
        // off -- because logging is on by default until logEnable is
        // called.
        DbgUtils.logi( TAG, "onCreate(); git_rev=%s",
                       getString( R.string.git_rev ) );
        DbgUtils.logEnable( this );

        ConnStatusHandler.loadState( this );

        OnBootReceiver.startTimers( this );

        boolean mustCheck = Utils.firstBootThisVersion( this );
        PreferenceManager.setDefaultValues( this, R.xml.xwprefs, mustCheck );
        if ( mustCheck ) {
            XWPrefs.setHaveCheckedUpgrades( this, false );
        } else {
            mustCheck = ! XWPrefs.getHaveCheckedUpgrades( this );
        }
        if ( mustCheck ) {
            UpdateCheckReceiver.checkVersions( this, false );
        }
        UpdateCheckReceiver.restartTimer( this );

        BTService.startService( this );
        RelayService.startService( this );
        GCMIntentService.init( this );
        WiDirService.init( this );
    }

    // This is called on emulator only, but good for ensuring no memory leaks
    // by forcing JNI cleanup
    @Override
    public void onTerminate()
    {
        DbgUtils.logi( TAG, "onTerminate() called" );
        XwJNI.cleanGlobals();
        super.onTerminate();
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
            s_onEmulator = new Boolean( "google_sdk".equals(Build.MODEL)
                                        || Build.PRODUCT.startsWith("vbox") );
        }
        return s_onEmulator;
    }

    public static Context getContext()
    {
        Assert.assertNotNull( s_context );
        return s_context;
    }
}