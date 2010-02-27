/* -*- compile-command: "cd ../../../../../; ant reinstall"; -*- */


package org.eehouse.android.xw4;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

public class StatusReceiver extends BroadcastReceiver {

    @Override
    public void onReceive( Context context, Intent intent ) 
    {
        Utils.logf( "StatusReceiver::onReceive called: " + intent.toString() );
    }

}
