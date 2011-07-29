/* -*- compile-command: "cd ../../../../../; ant reinstall"; -*- */
/*
 * Copyright 2009-2010 by Eric House (xwords@eehouse.org).  All
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

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.telephony.SmsMessage;

public class ReceiveNBS extends BroadcastReceiver {

    @Override
    public void onReceive( Context context, Intent intent ) 
    {
        Utils.logf( "onReceive called: %s", intent.toString() );

        Bundle bundle = intent.getExtras();        
        SmsMessage[] smsarr = null;
        if (bundle != null) {
            Object[] pdus = (Object[]) bundle.get("pdus");
            smsarr = new SmsMessage[pdus.length];            
            for ( int ii = 0; ii < pdus.length; ii++){
                smsarr[ii] = SmsMessage.createFromPdu((byte[])pdus[ii]);
                Utils.logf( "from %s",  smsarr[ii].getOriginatingAddress() );
                // buf.append( smsarr[ii].getMessageBody() );
                // XwJni.handle( XwJni.JNICmd.CMD_RECEIVE, 
                //               smsarr[ii].getMessageBody() );
            }
        }
    } // onReceive

}
