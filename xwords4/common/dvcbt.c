/*
 * Copyright 2020-2025 by Eric House (xwords@eehouse.org).  All rights
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


#include "dvcbtp.h"
#include "devicep.h"

typedef enum {
    BTCMD_BAD_PROTO,
    BTCMD_PING,
    BTCMD_PONG,
    BTCMD_SCAN,
    BTCMD_INVITE,
    BTCMD_INVITE_ACCPT,
    BTCMD__INVITE_DECL,  // unused
    BTCMD_INVITE_DUPID,
    BTCMD__INVITE_FAILED,  // generic error, and unused
    BTCMD_MESG_SEND,
    BTCMD_MESG_ACCPT,
    BTCMD__MESG_DECL,  // unused
    BTCMD_MESG_GAMEGONE,
    BTCMD__REMOVE_FOR,  // unused
    BTCMD_INVITE_DUP_INVITE,
    BTCMD_MAC_ASK,  // ask peer what my mac address is
    BTCMD_MAC_REPLY,  // reply to above
} BTCmd;

#define BT_PROTO_BATCH 2
#define BT_PROTO BT_PROTO_BATCH

void
sendInviteViaBT( XW_DUtilCtxt* dutil, XWEnv xwe, const NetLaunchInfo* nli,
                 const XP_UCHAR* hostName, const XP_BtAddrStr* btAddr )
{
    XWStreamCtxt* stream = dvc_makeStream( dutil );
    stream_putU8( stream, BT_PROTO );
    stream_putU8( stream, 1 );  /* one message in this packet */

    /* tmp stream lets us put message together then get length */
    XWStreamCtxt* tmpStream = dvc_makeStream( dutil );
    stream_putU8( tmpStream, BTCMD_INVITE );
    nli_saveToStream( nli, tmpStream );
    XP_U16 size = stream_getSize(tmpStream);
    stream_putU16( stream, size );
    stream_getFromStream( stream, tmpStream, size );
    stream_destroy( tmpStream );

    const XP_U8* ptr = stream_getPtr( stream );
    XP_U16 len = stream_getSize( stream );
    dutil_sendViaBT( dutil, xwe, ptr, len, hostName, btAddr );

    stream_destroy( stream );
}

void
sendMsgsViaBT( XW_DUtilCtxt* dutil, XWEnv xwe,
               const SendMsgsPacket* const XP_UNUSED(packets),
               const CommsAddrRec* addr, XP_U32 gameID )
{
    XP_USE(dutil);
    XP_USE(xwe);
    XP_USE(addr);
    XP_USE(gameID);
    // XP_ASSERT(0);
    XP_LOGFF( "NOT IMPLEMENTED" );
}

void
parseBTPacket( XW_DUtilCtxt* dutil, XWEnv xwe,
               const XP_U8* buf, XP_U16 len,
               const XP_UCHAR* fromName, const XP_UCHAR* fromAddr )
{
    XP_USE(dutil);
    XP_USE(xwe);
    XP_USE(buf);
    XP_USE(len);
    XP_USE(fromName);
    XP_USE(fromAddr);
}
