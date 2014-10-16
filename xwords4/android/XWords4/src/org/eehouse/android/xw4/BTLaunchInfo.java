/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009-2011 by Eric House (xwords@eehouse.org).  All
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

import android.bluetooth.BluetoothAdapter;
import android.content.Context;
import android.content.Intent;

import org.json.JSONException;
import org.json.JSONObject;

import junit.framework.Assert;

import org.eehouse.android.xw4.jni.CommsAddrRec;

public class BTLaunchInfo extends AbsLaunchInfo {

    protected String btName;
    protected String btAddress;
    protected int gameID;

    public BTLaunchInfo( String data )
    {
        try {
            JSONObject json = init( data );
            gameID = json.getInt( MultiService.GAMEID );
            btName = json.getString( MultiService.BT_NAME );
            btAddress = json.getString( MultiService.BT_ADDRESS );
            setValid( true );
        } catch ( JSONException ex ) {
            DbgUtils.loge( ex );
        }
    }

    public static String makeLaunchJSON( String curJson, int gameID, int lang, 
                                         String dict, int nPlayersT )
    {
        Assert.assertNull( curJson );
        String result = null;
        BluetoothAdapter adapter = XWApp.BTSUPPORTED
            ? BluetoothAdapter.getDefaultAdapter() : null;
        if ( null != adapter ) {
            String name = adapter.getName();
            String address = adapter.getAddress();

            try {
                result = makeLaunchJSONObject( lang, dict, nPlayersT )
                    .put( MultiService.GAMEID, gameID )
                    .put( MultiService.BT_NAME, name )
                    .put( MultiService.BT_ADDRESS, address )
                    .toString();
            } catch ( org.json.JSONException jse ) {
                DbgUtils.loge( jse );
            }
        }
        return result;
    }

    public BTLaunchInfo( Intent intent )
    {
        init( intent );
        btName = intent.getStringExtra( MultiService.BT_NAME );
        btAddress = intent.getStringExtra( MultiService.BT_ADDRESS );
        gameID = intent.getIntExtra( MultiService.GAMEID, 0 );
        setValid( null != btAddress && 0 != gameID );
    }


    public static void putExtras( Intent intent, int gameID, 
                                  String btName, String btAddr )
    {
        intent.putExtra( MultiService.GAMEID, gameID );
        intent.putExtra( MultiService.BT_NAME, btName );
        intent.putExtra( MultiService.BT_ADDRESS, btAddr );

        intent.putExtra( MultiService.OWNER, MultiService.OWNER_BT );
    }

    public CommsAddrRec getAddrRec()
    {
        return new CommsAddrRec( btName, btAddress );
    }
}
