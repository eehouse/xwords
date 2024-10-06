/* 
 * Copyright 2013 - 2020 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

#ifdef XWFEATURE_RELAY

#include "main.h"
#include "nli.h"

typedef struct _Procs {
    void (*msgReceived)( void* closure, const CommsAddrRec* from, 
                         const XP_U8* buf, XP_U16 len );
    void (*msgForRow)( void* closure, const CommsAddrRec* from,
                       sqlite3_int64 rowid, const XP_U8* buf, XP_U16 len );
    void (*msgNoticeReceived)( void* closure );
    void (*devIDReceived)( void* closure, const XP_UCHAR* devID, 
                           XP_U16 maxInterval );
    void (*msgErrorMsg)( void* closure, const XP_UCHAR* msg );
    void (*inviteReceived)( void* closure, const NetLaunchInfo* invit );
} RelayConnProcs;

void relaycon_init( LaunchParams* params, const RelayConnProcs* procs, 
                    void* procsClosure, const char* host, int port );
void relaycon_reg( LaunchParams* params, const XP_UCHAR* rDevID, 
                   DevIDType typ, const XP_UCHAR* devID );
/* Need one of dest or relayID, with dest preferred. pass 0 for dest to use
   relayID (formatted per comms::formatRelayID()) */
void relaycon_invite( LaunchParams* params, XP_U32 dest, 
                      const XP_UCHAR* relayID, NetLaunchInfo* invite );
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

void relaycon_checkMsgs( LaunchParams* params );

# ifdef RELAY_VIA_HTTP
typedef void (*OnJoinedProc)( void* closure, const XP_UCHAR* connname, XWHostID hid );
void relaycon_join( LaunchParams* params, const XP_UCHAR* devID, const XP_UCHAR* room,
                    XP_U16 nPlayersHere, XP_U16 nPlayersTotal, XP_U16 seed,
                    XP_U16 lang, OnJoinedProc proc, void* closure );
# endif

#endif

#endif
