/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
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

    public GCMIntentService()
    {
        super( GCMConsts.SENDER_ID );
    }

    @Override
    protected void onError( Context context, String error ) 
    {
        DbgUtils.logf("GCMIntentService.onError(%s)", error );
    }

    @Override
    protected void onRegistered( Context context, String regId ) 
    {
        DbgUtils.logf( "GCMIntentService.onRegistered(%s)", regId );
        XWPrefs.setGCMDevID( context, regId );
        notifyRelayService( true );
    }

    @Override
    protected void onUnregistered( Context context, String regId ) 
    {
        DbgUtils.logf( "GCMIntentService.onUnregistered(%s)", regId );
        XWPrefs.clearGCMDevID( context );
        notifyRelayService( false );
    }

    @Override
    protected void onMessage( Context context, Intent intent ) 
    {
        DbgUtils.logf( "GCMIntentService.onMessage()" );
        notifyRelayService( true );

        String value;
        boolean ignoreIt = XWApp.GCM_IGNORED;
        if ( ignoreIt ) {
            DbgUtils.logf( "received GCM but ignoring it" );
        } else {
            value = intent.getStringExtra( "checkUpdates" );
            if ( null != value && Boolean.parseBoolean( value ) ) {
                UpdateCheckReceiver.checkVersions( context, true );
            }

            value = intent.getStringExtra( "getMoves" );
            if ( null != value && Boolean.parseBoolean( value ) ) {
                RelayService.timerFired( context );
            }

            value = intent.getStringExtra( "msgs64" );
            if ( null != value ) {
                String connname = intent.getStringExtra( "connname" );
                try {
                    JSONArray msgs64 = new JSONArray( value );
                    String[] strs64 = new String[msgs64.length()];
                    for ( int ii = 0; ii < strs64.length; ++ii ) {
                        strs64[ii] = msgs64.optString(ii);
                    }
                    if ( null == connname ) {
                        RelayService.processDevMsgs( context, strs64 );
                    } else {
                        RelayService.processGameMsgs( context, connname, strs64 );
                    }
                } catch (org.json.JSONException jse ) {
                    DbgUtils.loge( jse );
                }
            }

            value = intent.getStringExtra( "msg" );
            if ( null != value ) {
                String title = intent.getStringExtra( "title" );
                if ( null != title ) {
                    int code = value.hashCode() ^ title.hashCode();
                    Utils.postNotification( context, null, title, value, code );
                }
            }
        }
    }

    public static void init( Application app )
    {
        int sdkVersion = Integer.valueOf( android.os.Build.VERSION.SDK );
        if ( 8 <= sdkVersion && 0 < GCMConsts.SENDER_ID.length() ) {
            try {
                GCMRegistrar.checkDevice( app );
                // GCMRegistrar.checkManifest( app );
                String regId = XWPrefs.getGCMDevID( app );
                if (regId.equals("")) {
                    GCMRegistrar.register( app, GCMConsts.SENDER_ID );
                }
            } catch ( UnsupportedOperationException uoe ) {
                DbgUtils.logf( "Device can't do GCM." );
            } catch ( Exception whatever ) {
                // funky devices could do anything
                DbgUtils.loge( whatever );
            }
        }
    }

    private void notifyRelayService( boolean working )
    {
        if ( working && XWApp.GCM_IGNORED ) {
            working = false;
        }
        RelayService.gcmConfirmed( working );
    }

}
