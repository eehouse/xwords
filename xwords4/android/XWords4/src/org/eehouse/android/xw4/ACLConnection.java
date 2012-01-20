/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2012 by Eric House (xwords@eehouse.org).  All rights
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
import android.bluetooth.BluetoothDevice;

public class ACLConnection extends BroadcastReceiver {

    @Override
    public void onReceive( Context context, Intent intent ) {
        String action = intent.getAction();
        if ( action.equals( BluetoothDevice.ACTION_ACL_CONNECTED ) ) {
            BluetoothDevice device = 
                intent.getParcelableExtra( BluetoothDevice.EXTRA_DEVICE );
            DbgUtils.logf( "ACLConnection.onReceive: ACL_CONNECTED: device: %s",
                           device.getName() );
        } else if ( action.equals( BluetoothDevice.
                                   ACTION_ACL_DISCONNECT_REQUESTED ) ) {
            DbgUtils.logf( "ACLConnection.onReceive: ACL_DISCONNECT_REQUESTED" );
        } else if ( action.equals( BluetoothDevice.ACTION_ACL_DISCONNECTED ) ) {
            DbgUtils.logf( "ACLConnection.onReceive: ACL_DISCONNECTED" );
        }
    }

}
