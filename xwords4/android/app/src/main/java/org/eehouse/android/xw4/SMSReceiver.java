/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
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

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.telephony.SmsMessage;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class SMSReceiver extends BroadcastReceiver {
    private static final String TAG = SMSReceiver.class.getSimpleName();
    private static final Pattern sPortPat = Pattern.compile("^sms://localhost:(\\d+)$");

    @Override
    public void onReceive( Context context, Intent intent )
    {
        String action = intent.getAction();
        // Log.d( TAG, "onReceive(): action=%s", action );
        if ( action.equals("android.intent.action.DATA_SMS_RECEIVED")
             && checkPort( context, intent ) ) {
            Bundle bundle = intent.getExtras();
            if ( null != bundle ) {
                Object[] pdus = (Object[])bundle.get( "pdus" );
                SmsMessage[] smses = new SmsMessage[pdus.length];

                for ( int ii = 0; ii < pdus.length; ++ii ) {
                    SmsMessage sms = SmsMessage.createFromPdu((byte[])pdus[ii]);
                    if ( null != sms ) {
                        try {
                            String phone = sms.getOriginatingAddress();
                            byte[] body = sms.getUserData();
                            SMSService.handleFrom( context, body, phone );
                        } catch ( NullPointerException npe ) {
                            Log.ex( TAG, npe );
                        }
                    }
                }
            }
        }
    }

    private boolean checkPort( Context context, Intent intent )
    {
        boolean portsMatch = true;
        Matcher matcher = sPortPat.matcher( intent.getDataString() );
        if ( matcher.find() ) {
            short port = Short.valueOf( matcher.group(1) );
            short myPort = getConfiguredPort( context );
            portsMatch = port == myPort;
            if ( !portsMatch ) {
                Log.i( TAG, "checkPort(): received msg on %d but expect %d",
                       port, myPort );
            }
        }
        return portsMatch;
    }

    private static Short sPort;
    private short getConfiguredPort( Context context )
    {
        if ( sPort == null ) {
            sPort = Short.valueOf( context.getString( R.string.nbs_port ) );
        }
        return sPort;
    }
}
