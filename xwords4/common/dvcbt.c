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
#include "gamemgr.h"

#define BT_DFLT_MAX_LEN 20

#ifdef XWFEATURE_BLUETOOTH

static MsgChunker*
initBTChunkerOnce( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    if ( !dutil->btChunkerState ) {
        dutil->btChunkerState = cnk_init( dutil, xwe, 0, BT_DFLT_MAX_LEN );
    }
    return dutil->btChunkerState;
}

static void
sendMsgs( XW_DUtilCtxt* dutil, XWEnv xwe, ChunkMsgArray* arr, XP_U16 waitSecs,
          const XP_UCHAR* hostName, const XP_BtAddrStr* btAddr )
{
    XP_ASSERT( 0 == waitSecs );
    if ( !!arr ) {
        for ( int ii = 0; ii < arr->nMsgs; ++ii ) {
            const ChunkMsgNet* msg = &arr->u.msgsNet[ii];
            // dutil_sendViaNBS( dutil, xwe, msg->data, msg->len, phone, port );
            dutil_sendViaBT( dutil, xwe, msg->data, msg->len, hostName, btAddr );
        }
        cnk_freeMsgArray( dutil->btChunkerState, arr );
    }
}

void
sendInviteViaBT( XW_DUtilCtxt* dutil, XWEnv xwe, const NetLaunchInfo* nli,
                 const XP_UCHAR* hostName, const XP_BtAddrStr* btAddr )
{
    MsgChunker* chunker = initBTChunkerOnce( dutil, xwe );

    XWStreamCtxt* stream = dvc_makeStream( dutil );
    nli_saveToStream( nli, stream );

    const XP_U8* ptr = strm_getPtr( stream );
    XP_U16 len = strm_getSize( stream );
    XP_U16 waitSecs;
    const XP_Bool forceOld = XP_FALSE;
    ChunkMsgArray* arr
        = cnk_prepOutbound( chunker, xwe, INVITE, nli->gameID,
                                 ptr, len, btAddr->chars, 0,
                                 forceOld, &waitSecs );
    XP_ASSERT( !!arr || !forceOld );
    sendMsgs( dutil, xwe, arr, waitSecs, hostName, btAddr );
    strm_destroy( stream );
}

void
sendMsgsViaBT( XW_DUtilCtxt* dutil, XWEnv xwe,
               const SendMsgsPacket* const packets,
               const CommsAddrRec* addr, XP_U32 gameID )
{
    MsgChunker* chunker = initBTChunkerOnce( dutil, xwe );

    const XP_UCHAR* hostName = addr->u.bt.hostName;
    const XP_BtAddrStr* btAddr = &addr->u.bt.btAddr;
    for ( const SendMsgsPacket* next = packets; !!next; next = next->next ) {
        XP_U16 waitSecs;
        ChunkMsgArray* arr
            = cnk_prepOutbound( chunker, xwe, DATA, gameID,
                                next->buf, next->len,
                                btAddr->chars, 0,
                                XP_TRUE, &waitSecs );
        sendMsgs( dutil, xwe, arr, waitSecs, hostName, btAddr );
    }
}

static void
handleMsg( XW_DUtilCtxt* dutil, XWEnv xwe, ChunkMsgLoc* msg, const CommsAddrRec* from )
{
    switch ( msg->cmd ) {
    case DATA:
        gmgr_onMessageReceived( dutil, xwe, msg->gameID,
                                from, msg->data, msg->len,
                                NULL );
        break;
    case INVITE: {
        XWStreamCtxt* stream = dvc_makeStream( dutil );
        strm_putBytes( stream, msg->data, msg->len );
        NetLaunchInfo nli = {};
        if ( nli_makeFromStream( &nli, stream ) ) {
            gmgr_addForInvite( dutil, xwe, GROUP_DEFAULT, &nli );
        } else {
            XP_ASSERT(0);
        }
        strm_destroy( stream );
    }
        break;
    default:
        XP_ASSERT(0);
        break;
    }
}

void
parseBTPacket( XW_DUtilCtxt* dutil, XWEnv xwe,
               const XP_U8* buf, XP_U16 len,
               const XP_UCHAR* fromName, const XP_UCHAR* fromAddr )
{
    MsgChunker* chunker = initBTChunkerOnce( dutil, xwe );
    ChunkMsgArray* msgArr = cnk_prepInbound( chunker, xwe, fromAddr, 0,
                                             buf, len );
    if ( NULL != msgArr ) {
        CommsAddrRec from = {};
        addr_addBT( &from, fromName, fromAddr );
        XP_ASSERT( msgArr->format == FORMAT_LOC );
        for ( int ii = 0; ii < msgArr->nMsgs; ++ii ) {
            ChunkMsgLoc* msg = &msgArr->u.msgsLoc[ii];
            handleMsg( dutil, xwe, msg, &from );
        }
        cnk_freeMsgArray( chunker, msgArr );
    }
}

void
onBLEMtuChangedFor( XW_DUtilCtxt* dutil, XWEnv xwe,
                    const XP_UCHAR* phone, XP_U16 mtu )
{
    MsgChunker* chunker = initBTChunkerOnce( dutil, xwe );
    cnk_maxSizeChangedFor( chunker, xwe, phone, mtu );
}

void
cleanupBT( XW_DUtilCtxt* dutil )
{
    if ( !!dutil->btChunkerState ) {
        cnk_free(dutil->btChunkerState);
        dutil->btChunkerState = NULL;
    }
}

#endif
