/* -*- compile-command: "find-and-gradle.sh -PuseCrashlytics insXw4dDeb"; -*- */
/*
 * Copyright 2019 - 2021 by Eric House (xwords@eehouse.org).  All rights
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

import android.content.Context;
import android.content.Intent;

import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.Task;
import com.google.firebase.iid.FirebaseInstanceId;
import com.google.firebase.iid.InstanceIdResult;
import com.google.firebase.messaging.FirebaseMessagingService;
import com.google.firebase.messaging.RemoteMessage;

import org.json.JSONArray;

import java.util.Map;

import org.eehouse.android.xw4.loc.LocUtils;

public class FBMService extends FirebaseMessagingService {
    private static final String TAG = FBMService.class.getSimpleName();

    public static void init( Context context )
    {
        Log.d( TAG, "init()" );
        Assert.assertTrueNR( null != BuildConfig.KEY_FCMID );
        getTokenAsync( context );
    }

    @Override
    public void onNewToken( String token )
    {
        Log.d( TAG, "onNewToken(%s)", token);
        onGotToken( this, token );
    }

    @Override
    public void onMessageReceived( RemoteMessage message )
    {
        if ( XWPrefs.getIgnoreFCM( this ) ) {
            Log.d( TAG, "onMessageReceived(): ignoring" );
        } else {
            callFcmConfirmed( this, true );

            Map<String, String>	data = message.getData();
            Log.d( TAG, "onMessageReceived(data=\"%s\")", data );
            boolean toastFCM = XWPrefs.getToastFCM( this );

            String value = data.get( "msgs64" );
            if ( null != value ) {
                String connname = data.get( "connname" );
                try {
                    JSONArray msgs64 = new JSONArray( value );
                    String[] strs64 = new String[msgs64.length()];
                    if ( toastFCM ) {
                        DbgUtils.showf( this, "%s-%s.onMessageReceived(): got %d msgs",
                                        BuildConfig.FLAVOR, TAG, strs64.length );
                    }

                    for ( int ii = 0; ii < strs64.length; ++ii ) {
                        strs64[ii] = msgs64.optString(ii);
                    }
                    if ( null == connname ) {
                        RelayService.processDevMsgs( this, strs64 );
                    } else {
                        RelayService.processGameMsgs( this, connname, strs64 );
                    }
                } catch (org.json.JSONException jse ) {
                    Log.ex( TAG, jse );
                    Assert.assertFalse( BuildConfig.DEBUG );
                }
            }

            value = data.get( "checkUpdates" );
            if ( null != value && Boolean.parseBoolean( value ) ) {
                UpdateCheckReceiver.checkVersions( this, true );
            }

            value = data.get( "getMoves" );
            if ( null != value && Boolean.parseBoolean( value ) ) {
                RelayService.timerFired( this );
                if ( toastFCM ) {
                    DbgUtils.showf( this, "%s.onMessageReceived(): got 'getMoves'",
                                    TAG );
                }
            }

            value = data.get( "getMQTT" );
            if ( null != value && Boolean.parseBoolean( value ) ) {
                MQTTUtils.onFCMReceived( this );
                if ( toastFCM ) {
                    DbgUtils.showf( this, "%s.onMessageReceived(): got 'getMQTT'",
                                    TAG );
                }
            }

            value = data.get( "msg" );
            if ( null != value ) {
                String title = data.get( "title" );
                if ( null == title ) {
                    title = LocUtils.getString( this, R.string.remote_msg_title );
                }
                String teaser = data.get( "teaser" );
                if ( null == teaser ) {
                    teaser = value;
                }
                Intent alertIntent = GamesListDelegate
                    .makeAlertIntent( this, value );
                int code = value.hashCode() ^ title.hashCode();
                Utils.postNotification( this, alertIntent, title,
                                        teaser, code );
            }
        }
    }

    public static String getFCMDevID( Context context )
    {
        Assert.assertTrueNR( null != BuildConfig.KEY_FCMID );
        String result = DBUtils.getStringFor( context, BuildConfig.KEY_FCMID );

        if ( null == result ) {
            getTokenAsync( context );
        }
        return result;
    }

    private static void getTokenAsync( final Context context )
    {
        FirebaseInstanceId.getInstance().getInstanceId()
            .addOnCompleteListener(new OnCompleteListener<InstanceIdResult>() {
                    @Override
                    public void onComplete(Task<InstanceIdResult> task) {
                        if (!task.isSuccessful()) {
                            Log.w(TAG, "getInstanceId failed: %s", task.getException());
                            if ( !XWPrefs.getIgnoreFCM( context ) ) {
                                callFcmConfirmed( context, false );
                            }
                        } else {

                            // Get new Instance ID token
                            String token = task.getResult().getToken();

                            // Log and toast
                            Log.d(TAG, "got token!: %s", token );
                            onGotToken( context, token );
                        }
                    }
                });
    }

    private static void onGotToken( Context context, String token )
    {
        // Don't call this with empty tokens!!!
        Assert.assertTrue( token.length() > 0 || !BuildConfig.DEBUG );

        DBUtils.setStringFor( context, BuildConfig.KEY_FCMID, token );
        DevID.setFCMDevID( context, token );

        callFcmConfirmed( context, true );
    }

    private static void callFcmConfirmed( Context context, boolean working )
    {
        RelayService.fcmConfirmed( context, working );
        MQTTUtils.fcmConfirmed( context, working );
    }
}
