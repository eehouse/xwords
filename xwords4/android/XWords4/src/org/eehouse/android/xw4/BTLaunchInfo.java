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
import org.json.JSONObject;
import org.json.JSONException;
import android.content.Context;

import org.eehouse.android.xw4.jni.CommsAddrRec;

public class BTLaunchInfo extends AbsLaunchInfo {
    private static final String INVITE_GAMEID = "INVITE_GAMEID";
    private static final String INVITE_BT_NAME = "INVITE_BT_NAME";
    private static final String INVITE_BT_ADDRESS = "INVITE_BT_ADDRESS";

    protected String btName;
    protected String btAddress;
    protected int gameID;

    public BTLaunchInfo( String data )
    {
        try {
            JSONObject json = init( data );
            gameID = json.getInt( INVITE_GAMEID );
            btName = json.getString( INVITE_BT_NAME );
            btAddress = json.getString( INVITE_BT_ADDRESS );
            setValid( true );
        } catch ( JSONException ex ) {
            DbgUtils.loge( ex );
        }
    }

    public static String makeLaunchJSON( int gameID, int lang, 
                                         String dict, int nPlayersT )
    {
        String result = null;
        BluetoothAdapter adapter = XWApp.BTSUPPORTED
            ? BluetoothAdapter.getDefaultAdapter() : null;
        if ( null != adapter ) {
            String name = adapter.getName();
            String address = adapter.getAddress();

            try {
                result = makeLaunchJSONObject( lang, dict, nPlayersT )
                    .put( INVITE_GAMEID, gameID )
                    .put( INVITE_BT_NAME, name )
                    .put( INVITE_BT_ADDRESS, address )
                    .toString();
            } catch ( org.json.JSONException jse ) {
                DbgUtils.loge( jse );
            }
        }
        return result;
    }

    public CommsAddrRec getAddrRec()
    {
        return new CommsAddrRec( btName, btAddress );
    }
}
