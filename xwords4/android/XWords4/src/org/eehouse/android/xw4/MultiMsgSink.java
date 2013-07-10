/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
/*
 * Copyright 2009 - 2012 by Eric House (xwords@eehouse.org).  All
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

import android.content.Context;
import java.util.HashMap;
import java.util.ArrayList;

import junit.framework.Assert;

import org.eehouse.android.xw4.jni.*;

public class MultiMsgSink implements TransportProcs {

    /***** TransportProcs interface *****/

    public int getFlags() { return COMMS_XPORT_FLAGS_HASNOCONN; }

    public int transportSend( byte[] buf, final CommsAddrRec addr, int gameID )
    {
        Assert.fail();          // implement if this is getting called!!!
        return -1;
    }

    public void relayStatus( CommsRelayState newState )
    {
    }

    public void relayErrorProc( XWRELAY_ERROR relayErr )
    {
    }

    public void relayConnd( String room, int devOrder, boolean allHere, 
                            int nMissing )
    {
    }

    public boolean relayNoConnProc( byte[] buf, String relayID )
    {
        Assert.fail();          // implement if this is getting called!!!
        return false; 
    }
}
