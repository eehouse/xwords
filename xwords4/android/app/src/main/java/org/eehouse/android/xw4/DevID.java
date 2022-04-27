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

/* The relay issues an identifier for a registered device. It's a string
 * representation of a 32-bit hex number. When devices register, they pass
 * what's meant to be a unique identifier of their own. GCM-aware devices (for
 * which this was originally conceived) pass their GCM IDs (which can change,
 * and require re-registration). Other devices generate an ID however they
 * choose, or can pass "", meaning "I'm anonymous; just give me an ID based on
 * nothing."
 */

import android.content.Context;

public class DevID {
    private static final String TAG = DevID.class.getSimpleName();

    private static final String DEVID_KEY = "DevID.devid_key";
    private static final String DEVID_ACK_KEY = "key_relay_regid_ackd2";
    private static final String FCM_REGVERS_KEY = "key_fcmvers_regid";
    private static final String NFC_DEVID_KEY = "key_nfc_devid";

    private static String s_relayDevID;
    private static int s_asInt;

    // Called, likely on DEBUG builds only, when the relay hostname is
    // changed. DevIDs are invalid at that point.
    public static void hostChanged( Context context )
    {
        RelayService.logGone( TAG, 1 );
    }

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

    public static String getRelayDevID( Context context, boolean insistAckd )
    {
        String result = getRelayDevID( context );
        if ( insistAckd && null != result && 0 < result.length()
             && ! DBUtils.getBoolFor( context, DEVID_ACK_KEY, false ) ) {
            result = null;
        }
        return result;
    }

    public static String getRelayDevID( Context context )
    {
        if ( null == s_relayDevID ) {
            String asStr = DBUtils.getStringFor( context, DEVID_KEY, "" );
            // TRANSITIONAL: If it's not there, see if it's stored the old way
            if ( 0 == asStr.length() ) {
                asStr = XWPrefs.getPrefsString( context, R.string.key_relay_regid );
            }

            if ( null != asStr && 0 != asStr.length() ) {
                s_relayDevID = asStr;
            }
        }
        // Log.d( TAG, "getRelayDevID() => %s", s_relayDevID );
        return s_relayDevID;
    }

    public static void setRelayDevID( Context context, String devID )
    {
        Log.d( TAG, "setRelayDevID()" );
        if ( BuildConfig.DEBUG ) {
            String oldID = getRelayDevID( context );
            if ( null != oldID && 0 < oldID.length()
                 && ! devID.equals( oldID ) ) {
                Log.d( TAG, "devID changing!!!: %s => %s", oldID, devID );
            }
        }
        DBUtils.setStringFor( context, DEVID_KEY, devID );
        s_relayDevID = devID;

        DBUtils.setBoolFor( context, DEVID_ACK_KEY, true );
        // DbgUtils.printStack();
    }

    public static void clearRelayDevID( Context context )
    {
        Log.i( TAG, "clearRelayDevID()" );
        DBUtils.setStringFor( context, DEVID_KEY, "" );
        // DbgUtils.printStack();
    }

    public static void setFCMDevID( Context context, String devID )
    {
        int curVers = Utils.getAppVersion( context );
        DBUtils.setIntFor( context, FCM_REGVERS_KEY, curVers );
        DBUtils.setBoolFor( context, DEVID_ACK_KEY, false );
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
            Log.d( TAG, "getNFCDevID() => %d", sNFCDevID[0] );
            return sNFCDevID[0];
        }
    }
}
