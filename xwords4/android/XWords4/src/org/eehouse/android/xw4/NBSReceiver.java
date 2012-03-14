/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2010 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.telephony.SmsMessage;
import android.telephony.SmsManager;

public class NBSReceiver extends BroadcastReceiver {

    @Override
    public void onReceive( Context context, Intent intent )
    {
        DbgUtils.logf( "NBSReceiver::onReceive(intent=%s)!!!!",
                    intent.toString() );

        Bundle bundle = intent.getExtras();
        if ( null != bundle ) {
            Object[] pdus = (Object[])bundle.get( "pdus" );
            SmsMessage[] nbses = new SmsMessage[pdus.length];

            for ( int ii = 0; ii < nbses.length; ++ii ) {
                nbses[ii] = SmsMessage.createFromPdu((byte[])pdus[ii]);

                byte[] data = nbses[ii].getUserData();
                char[] asChars = new char[data.length];
                for ( int jj = 0; jj < data.length; ++jj ) {
                    asChars[jj] = (char)data[jj];
                }
                String dataStr = new String( asChars );
                DbgUtils.logf( "got %d bytes from %s: %s", data.length,
                               nbses[ii].getOriginatingAddress(), dataStr );
            }
        }
    }

    static void tryNBSMessage( Context context, String phoneNo )
    {
        byte[] data = { 'a', 'b', 'c' };

        SmsManager mgr = SmsManager.getDefault();

        try {
            /* online comment says providing PendingIntents prevents 
               random crashes */
            PendingIntent sent = PendingIntent.getBroadcast( context,
                                                             0, new Intent(), 0 );
            PendingIntent dlvrd = PendingIntent.getBroadcast( context, 0, 
                                                              new Intent(), 0 );

            mgr.sendDataMessage( phoneNo, null, (short)50009, 
                                 data, sent, dlvrd );
            // PendingIntent sentIntent, 
            // PendingIntent deliveryIntent );
            DbgUtils.logf( "sendDataMessage finished" );
        } catch ( IllegalArgumentException iae ) {
            DbgUtils.logf( "%s", iae.toString() );
        }
    }

    public static void inviteRemote( Context context, String phone,
                                     int gameID, String gameName, 
                                     int lang, int nPlayersT, 
                                     int nPlayersH )
    {
        DbgUtils.logf( "NBSReceiver.inviteRemote(phone=%s)", phone );
    }

}
