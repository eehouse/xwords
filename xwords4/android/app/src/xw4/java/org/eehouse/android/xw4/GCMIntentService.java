/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
/*
 * Copyright 2012 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.Application;
import android.content.Context;
import android.content.Intent;

import com.google.android.gcm.GCMBaseIntentService;
import com.google.android.gcm.GCMRegistrar;

import org.json.JSONArray;


public class GCMIntentService extends GCMBaseIntentService {
    private static final String TAG = GCMIntentService.class.getSimpleName();

    private Boolean m_toastGCM;

    public GCMIntentService()
    {
        super( BuildConfig.GCM_SENDER_ID );
        Assert.assertTrue( BuildConfig.GCM_SENDER_ID.length() > 0 );
    }

    @Override
    protected void onError( Context context, String error )
    {
        Log.d( TAG, "onError(%s)", error );
    }

    @Override
    protected void onRegistered( Context context, String regId )
    {
        Log.d( TAG, "onRegistered(%s)", regId );
        DevID.setGCMDevID( context, regId );
        notifyRelayService( context, true );
    }

    @Override
    protected void onUnregistered( Context context, String regId )
    {
        Log.d( TAG, "onUnregistered(%s)", regId );
        DevID.clearGCMDevID( context );
        RelayService.devIDChanged();
        notifyRelayService( context, false );
    }

    @Override
    protected void onMessage( Context context, Intent intent )
    {
        Log.d( TAG, "onMessage()" );

        if ( null == m_toastGCM ) {
            m_toastGCM = new Boolean( XWPrefs.getToastGCM( context ) );
        }

        if ( XWPrefs.getIgnoreGCM( context ) ) {
            String logMsg = "received GCM but ignoring it";
            Log.d( TAG, logMsg );
            DbgUtils.showf( context, logMsg );
        } else {
            notifyRelayService( context, true );

            String value = intent.getStringExtra( "checkUpdates" );
            if ( null != value && Boolean.parseBoolean( value ) ) {
                UpdateCheckReceiver.checkVersions( context, true );
            }

            value = intent.getStringExtra( "getMoves" );
            if ( null != value && Boolean.parseBoolean( value ) ) {
                RelayService.timerFired( context );
                if ( m_toastGCM ) {
                    DbgUtils.showf( context, "onMessage(): got 'getMoves'" );
                }
            }

            value = intent.getStringExtra( "msgs64" );
            if ( null != value ) {
                String connname = intent.getStringExtra( "connname" );
                try {
                    JSONArray msgs64 = new JSONArray( value );
                    String[] strs64 = new String[msgs64.length()];
                    if ( m_toastGCM ) {
                        DbgUtils.showf( context, "onMessage(): got %d msgs",
                                        strs64.length );
                    }

                    for ( int ii = 0; ii < strs64.length; ++ii ) {
                        strs64[ii] = msgs64.optString(ii);
                    }
                    if ( null == connname ) {
                        RelayService.processDevMsgs( context, strs64 );
                    } else {
                        RelayService.processGameMsgs( context, connname, strs64 );
                    }
                } catch (org.json.JSONException jse ) {
                    Log.ex( TAG, jse );
                    Assert.assertFalse( BuildConfig.DEBUG );
                }
            }

            value = intent.getStringExtra( "msg" );
            if ( null != value ) {
                String title = intent.getStringExtra( "title" );
                if ( null != title ) {
                    String teaser = intent.getStringExtra( "teaser" );
                    if ( null == teaser ) {
                        teaser = value;
                    }
                    Intent alertIntent = GamesListDelegate
                        .makeAlertIntent( this, value );
                    int code = value.hashCode() ^ title.hashCode();
                    Utils.postNotification( context, alertIntent, title,
                                            teaser, code );
                }
            }
        }
    }

    public static void init( Application app )
    {
        if ( 0 < BuildConfig.GCM_SENDER_ID.length() ) {
            int sdkVersion = Integer.valueOf( android.os.Build.VERSION.SDK );
            if ( 8 <= sdkVersion ) {
                try {
                    GCMRegistrar.checkDevice( app );
                    // GCMRegistrar.checkManifest( app );
                    String regId = DevID.getGCMDevID( app );
                    if ( regId.equals("") ) {
                        GCMRegistrar.register( app, BuildConfig.GCM_SENDER_ID );
                    }
                } catch ( UnsupportedOperationException uoe ) {
                    Log.w( TAG, "Device can't do GCM." );
                } catch ( Exception whatever ) {
                    // funky devices could do anything
                    Log.ex( TAG, whatever );
                }
            }
        }
    }

    private void notifyRelayService( Context context, boolean working )
    {
        if ( !XWPrefs.getIgnoreGCM( context ) ) {
            RelayService.gcmConfirmed( context, working );
        }
    }
}
