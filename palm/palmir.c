/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2001 by Eric House (xwords@eehouse.org).  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef XWFEATURE_IR

#include <TimeMgr.h>

#include "palmir.h"

#include "callback.h"
#include "xwords4defines.h"
#include "comms.h"
#include "memstream.h"
#include "palmutil.h"
#include "LocalizedStrIncludes.h"

/* We're passed an address as we've previously defined it and a buffer
 * containing a message to send.  Prepend any palm/ir specific headers to the
 * message, save the buffer somewhere, and fire up the state machine that
 * will eventually get it sent to the address.
 *
 * Note that the caller will queue the message for possible resend, but
 * won't automatically schedule that resend whatever results we return.
 *
 * NOTE also that simply stuffing the buf ptr into the packet won't work
 * if there's any ir-specific packet header I need to prepend to what's
 * outgoing.
 */
XP_S16
palm_ir_send( const XP_U8* buf, XP_U16 len, PalmAppGlobals* globals )
{
    UInt32 sent = 0;
    Err err;
    ExgSocketType exgSocket;
    XP_MEMSET( &exgSocket, 0, sizeof(exgSocket) );
    exgSocket.description = "Crosswords data"; 
    exgSocket.length = len;
    exgSocket.target = APPID;

    if ( globals->romVersion >= 40 ) {
        exgSocket.name = exgBeamPrefix;
    }

    err = ExgPut( &exgSocket );
    while ( !err && sent < len ) {
        sent += ExgSend( &exgSocket, buf+sent, len-sent, &err );
        XP_ASSERT( sent < 0x7FFF );
    }
    err = ExgDisconnect( &exgSocket, err );

    return err==0? sent : 0;
} /* palm_ir_send */

void
palm_ir_receiveMove( PalmAppGlobals* globals, ExgSocketPtr socket )
{
    UInt32 nBytesReceived = -1;
    Err err;

    err = ExgAccept( socket );
    if ( err == 0 ) {
        XWStreamCtxt* instream;

        instream = mem_stream_make( MEMPOOL globals->vtMgr, globals, 
                                    CHANNEL_NONE, NULL );
        stream_open( instream );

        for ( ; ; ) {
            UInt8 buf[128];
            nBytesReceived = ExgReceive( socket, buf, sizeof(buf), &err );
            if ( nBytesReceived == 0 || err != 0 ) {
                break;
            }

            stream_putBytes( instream, buf, nBytesReceived );
        }
        (void)ExgDisconnect( socket, err );

        if ( nBytesReceived == 0 ) { /* successful loop exit */
            checkAndDeliver( globals, NULL, instream, COMMS_CONN_IR );
        }
    }
} /* palm_ir_receiveMove */

#endif /* XWFEATURE_IR */
