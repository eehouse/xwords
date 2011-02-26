/* -*- compile-command: "cd ../../../../../../; ant install"; -*- */
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

package org.eehouse.android.xw4.jni;

public interface TransportProcs {
    int transportSend( byte[] buf, final CommsAddrRec addr );

    enum CommsRelayState { COMMS_RELAYSTATE_UNCONNECTED
            , COMMS_RELAYSTATE_DENIED
            , COMMS_RELAYSTATE_CONNECT_PENDING
            , COMMS_RELAYSTATE_CONNECTED
            , COMMS_RELAYSTATE_RECONNECTED
            , COMMS_RELAYSTATE_ALLCONNECTED
    };
    void relayStatus( CommsRelayState newState );

    void relayConnd( String room, int devOrder, boolean allHere, int nMissing );

    public static enum XWRELAY_ERROR { NONE
            ,OLDFLAGS 
            ,BADPROTO
            ,RELAYBUSY
            ,SHUTDOWN
            ,TIMEOUT 
            ,HEART_YOU
            ,HEART_OTHER
            ,LOST_OTHER
            ,OTHER_DISCON
            ,NO_ROOM
            ,DUP_ROOM
            ,TOO_MANY
            ,DELETED
    };
    void relayErrorProc( XWRELAY_ERROR relayErr );

    public interface TPMsgHandler {
        public void tpmRelayConnd( String room, int devOrder, boolean allHere, 
                                   int nMissing );
        public void tpmRelayErrorProc( XWRELAY_ERROR relayErr );
    }
}
