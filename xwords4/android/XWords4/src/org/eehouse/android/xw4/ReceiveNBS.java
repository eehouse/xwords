/* -*- compile-command: "cd ../../../../../; ant reinstall"; -*- */

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
        Utils.logf( "onReceive called: " + intent.toString() );

        Bundle bundle = intent.getExtras();        
        SmsMessage[] smsarr = null;
        if (bundle != null) {
            Object[] pdus = (Object[]) bundle.get("pdus");
            smsarr = new SmsMessage[pdus.length];            
            for ( int ii = 0; ii < pdus.length; ii++){
                smsarr[ii] = SmsMessage.createFromPdu((byte[])pdus[ii]);
                Utils.logf( "from " + smsarr[ii].getOriginatingAddress() );
                // buf.append( smsarr[ii].getMessageBody() );
                // XwJni.handle( XwJni.JNICmd.CMD_RECEIVE, 
                //               smsarr[ii].getMessageBody() );
            }
        }
    } // onReceive

}
