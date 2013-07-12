/* -*- compile-command: "make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 2013 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _RELAYCON_H_
#define _RELAYCON_H_

#include "main.h"

typedef struct _Procs {
    void (*msgReceived)( void* closure, const XP_U8* buf, XP_U16 len );
    void (*msgNoticeReceived)( void* closure );
    void (*devIDChanged)( void* closure, const XP_UCHAR* devID );
    void (*msgErrorMsg)( void* closure, const XP_UCHAR* msg );
    void (*socketChanged)( void* closure, int newSock, int oldSock, 
                           SockReceiver proc, void* procClosure );

} RelayConnProcs;

void relaycon_init( LaunchParams* params, const RelayConnProcs* procs, 
                    void* procsClosure, const char* host, int port );
void relaycon_reg( LaunchParams* params, const XP_UCHAR* devID, DevIDType typ );
XP_S16 relaycon_send( LaunchParams* params, const XP_U8* buf, XP_U16 buflen, 
                      XP_U32 gameToken, const CommsAddrRec* addrRec );
XP_S16 relaycon_sendnoconn( LaunchParams* params, const XP_U8* buf, 
                            XP_U16 buflen, const XP_UCHAR* relayID, 
                            XP_U32 gameToken );
void relaycon_requestMsgs( LaunchParams* params, const XP_UCHAR* devID );
void relaycon_deleted( LaunchParams* params, const XP_UCHAR* devID, 
                       XP_U32 gameToken );

void relaycon_cleanup( LaunchParams* params );

XP_U32 makeClientToken( sqlite3_int64 rowid, XP_U16 seed );
void rowidFromToken( XP_U32 clientToken, sqlite3_int64* rowid, XP_U16* seed );
#endif
