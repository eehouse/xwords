/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2024 by Eric House (xwords@eehouse.org).  All rights reserved.
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
import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.net.HttpURLConnection;
import java.util.ArrayList;
import java.util.List;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;

public class PeerStatusView extends LinearLayout {
    private static final String TAG = PeerStatusView.class.getSimpleName();

    private Context mContext;
    private boolean mFinished;
    private int mGameID;
    private String mSelfDevID;

    public PeerStatusView( Context cx, AttributeSet as )
    {
        super( cx, as );
        mContext = cx;
    }

    public void configure( int gameID, String devID )
    {
        mGameID = gameID;
        mSelfDevID = devID;
        startThreadOnce();
    }
    
    @Override
    protected void onFinishInflate()
    {
        mFinished = true;
        startThreadOnce();
    }

    private void startThreadOnce()
    {
        if ( mFinished && null != mSelfDevID ) {
            new Thread( new Runnable() {
                @Override
                public void run() {
                    fetchAndDisplay();
                }
            } ).start();
        }
    }

    private void fetchAndDisplay()
    {
        String userStr = null;
        JSONObject params = new JSONObject();
        try {
            params.put( "gid16", String.format("%X", mGameID) );
            params.put( "devid", mSelfDevID );

            HttpURLConnection conn = NetUtils
                .makeHttpMQTTConn( mContext, "peers" );
            final String resStr = NetUtils.runConn( conn, params, true );
            Log.d( TAG, "runConn(ack) => %s", resStr );

            JSONObject result = new JSONObject( resStr );
            JSONArray results = result.optJSONArray( "results" );
            if ( null != results ) {
                List<String> lines = new ArrayList<>();
                for ( int ii = 0; ii < results.length(); ++ii ) {
                    JSONObject line = results.getJSONObject(ii);
                    String mqttID = line.getString("devid");
                    String age = line.getString( "age" );
                    String name = XwJNI.kplr_nameForMqttDev( mqttID );
                    if ( null == name ) {
                        if ( mSelfDevID.equals(mqttID) ) {
                            name = LocUtils.getString( mContext, R.string.selfName );
                        } else {
                            name = mqttID;
                        }
                    }
                    lines.add( String.format( "%s: %s", name, age ) );
                }
                userStr = TextUtils.join( "\n", lines );
            }
        } catch ( JSONException je ) {
            Log.ex( TAG, je );
        } catch ( NullPointerException npe ) {
            Log.ex( TAG, npe );
        }

        Activity activity = DelegateBase.getHasLooper();
        if ( null != activity ) {
            final String finalUserStr = userStr != null
                ? userStr : LocUtils.getString(mContext, R.string.no_peers_info);
            activity.runOnUiThread( new Runnable() {
                    @Override
                    public void run() {
                        TextView tv = (TextView)findViewById( R.id.status );
                        tv.setText( finalUserStr );
                    }
                } );
        } else {
            Log.d( TAG, "no activity found" );
        }
    }
}
