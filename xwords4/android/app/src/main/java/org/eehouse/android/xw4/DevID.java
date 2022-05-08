/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 - 2015 by Eric House (xwords@eehouse.org).  All rights
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

/* This class is left over from the proprietary relay days and doesn't do much
 * now. Can go away soon, wihth getNFCDevID() moving elsewhere.
 */

import android.content.Context;

public class DevID {
    private static final String TAG = DevID.class.getSimpleName();

    private static final String DEVID_KEY = "DevID.devid_key";
    private static final String NFC_DEVID_KEY = "key_nfc_devid";

    private static String s_relayDevID;
    private static int s_asInt;

    public static int getRelayDevIDInt( Context context )
    {
        if ( 0 == s_asInt ) {
            String asStr = getRelayDevID( context );
            if ( null != asStr && 0 < asStr.length() ) {
                s_asInt = Integer.valueOf( asStr, 16 );
            }
        }
        return s_asInt;
    }

    public static String getRelayDevID( Context context )
    {
        if ( null == s_relayDevID ) {
            String asStr = DBUtils.getStringFor( context, DEVID_KEY, "00000000" );
            if ( null != asStr && 0 != asStr.length() ) {
                s_relayDevID = asStr;
            }
        }
        // Log.d( TAG, "getRelayDevID() => %s", s_relayDevID );
        return s_relayDevID;
    }

    // Just a random number I hang onto as long as possible
    private static int[] sNFCDevID = {0};
    public static int getNFCDevID( Context context )
    {
        synchronized ( sNFCDevID ) {
            if ( 0 == sNFCDevID[0] ) {
                int devid = DBUtils.getIntFor( context, NFC_DEVID_KEY, 0 );
                while ( 0 == devid ) {
                    devid = Utils.nextRandomInt();
                    DBUtils.setIntFor( context, NFC_DEVID_KEY, devid );
                }
                sNFCDevID[0] = devid;
            }
            // Log.d( TAG, "getNFCDevID() => %d", sNFCDevID[0] );
            return sNFCDevID[0];
        }
    }
}
