/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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
import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.LifecycleObserver;
import androidx.lifecycle.LifecycleOwner;
import androidx.lifecycle.OnLifecycleEvent;
import androidx.lifecycle.ProcessLifecycleOwner;
import android.content.Context;
import android.graphics.Color;
import android.os.Build;

import org.eehouse.android.nbsplib.NBSProxy;

import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.XwJNI;

import java.util.UUID;

import static androidx.lifecycle.Lifecycle.Event.ON_ANY;

public class XWApp extends Application
    implements LifecycleObserver, NBSProxy.Callbacks {
    private static final String TAG = XWApp.class.getSimpleName();

    public static final boolean DEBUG_EXP_TIMERS = false;
    public static final boolean LOCUTILS_ENABLED = false;
    public static final boolean CONTEXT_MENUS_ENABLED = true;
    public static final boolean OFFER_DUALPANE = false;

    public static final String SMS_PUBLIC_HEADER = "-XW4";
    public static final int MIN_TRAY_TILES = 7; // comtypes.h
    public static final int SEL_COLOR = Color.argb( 0xFF, 0x09, 0x70, 0x93 );

    public static final int GREEN = 0xFF00AF00;
    public static final int RED = 0xFFAF0000;

    private static UUID s_UUID = null;
    private static Boolean s_onEmulator = null;
    private static Context s_context = null;

    private short mPort;

    @Override
    public void onCreate()
    {
        s_context = this;
        Assert.assertTrue( s_context == s_context.getApplicationContext() );
        super.onCreate();

        Log.init( this );

        ProcessLifecycleOwner.get().getLifecycle().addObserver(this);

        android.util.Log.i( TAG, "onCreate(); git_rev="
                            + BuildConfig.GIT_REV );
        Log.enable( this );

        OnBootReceiver.startTimers( this );

        boolean mustCheck = Utils.firstBootThisVersion( this );
        PrefsDelegate.resetPrefs( this, mustCheck );
        if ( mustCheck ) {
            XWPrefs.setHaveCheckedUpgrades( this, false );
        } else {
            mustCheck = ! XWPrefs.getHaveCheckedUpgrades( this );
        }
        if ( mustCheck ) {
            UpdateCheckReceiver.checkVersions( this, false );
        }
        UpdateCheckReceiver.restartTimer( this );

        RelayService.startService( this );
        WiDirWrapper.init( this );

        mPort = Short.valueOf( getString( R.string.nbs_port ) );
        NBSProxy.register( this, mPort, BuildConfig.APPLICATION_ID, this );

        DupeModeTimer.init( this );

        MQTTUtils.init( this );
        BTUtils.init( this, getAppName(), getAppUUID() );
    }

    @OnLifecycleEvent(ON_ANY)
    public void onAny( LifecycleOwner source, Lifecycle.Event event )
    {
        Log.d( TAG, "onAny(%s)", event );
        switch( event ) {
        case ON_RESUME:
            MQTTUtils.onResume( this );
            BTUtils.onResume( this );
            GameUtils.resendAllIf( this, null );
            break;
        case ON_STOP:
            BTUtils.onStop( this );
            break;
        }
    }

    // This is called on emulator only, but good for ensuring no memory leaks
    // by forcing JNI cleanup
    @Override
    public void onTerminate()
    {
        Log.d( TAG, "onTerminate() called" );
        XwJNI.cleanGlobalsEmu();
        super.onTerminate();
    }

    // NBSProxy.Callbacks
    @Override
    public void onProxyAppLaunched()
    {
        Log.d( TAG, "onProxyAppLaunched()" );
    }

    @Override
    public void onPermissionsGranted()
    {
        Log.d( TAG, "onPermissionsGranted()" );
        GameUtils.resendAllIf( this, CommsConnType.COMMS_CONN_SMS );
    }

    @Override
    public void onDataReceived( short port, String fromPhone, byte[] data )
    {
        Assert.assertTrue( port == mPort || !BuildConfig.DEBUG );
        NBSProto.handleFrom( this, data, fromPhone, port );
    }

    @Override
    public void onRegResponse( boolean appReached, boolean needsInitialLaunch )
    {
        if ( needsInitialLaunch ) {
            String channelID = Channels.getChannelID( this, Channels.ID.NBSPROXY );
            NBSProxy.postLaunchNotification( this, channelID, R.drawable.notify );
        }
    }

    public static UUID getAppUUID()
    {
        if ( null == s_UUID ) {
            s_UUID = UUID.fromString( XwJNI.comms_getUUID() );
            Log.d( TAG, "s_UUID (for BT): %s", s_UUID );
        }
        return s_UUID;
    }

    public static String getAppName()
    {
        Context context = getContext();
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
