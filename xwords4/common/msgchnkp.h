/* 
 * Copyright 2018 by Eric House (xwords@eehouse.org). All rights reserved.
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

#ifndef _MSGCHNKP_H_
#define _MSGCHNKP_H_

// This "class" breaks up or combines messages for NBS (sms data) or BLE
// transport. Both have a fairly small max packet size, and NBS takes enough
// time that combining small messages is a win.

#include "xptypes.h"
#include "mempool.h" /* debug only */
#include "nli.h"

typedef struct MsgChunker MsgChunker;

typedef enum { NONE, INVITE, DATA, DEATH, ACK_INVITE, } CHUNK_CMD;

/* Unpacked/local format with relevant fields exposed */
typedef struct _ChunkMsgLoc {
    // XP_U16 msgID;
    CHUNK_CMD cmd;
    XP_U32 gameID;
    XP_U16 len;
    XP_U8* data; // will be NetLaunchInfo* if cmd == INVITE
} ChunkMsgLoc;

/* Flattened format suitable for transmission over wire. Encapsulates Loc
   format data */
typedef struct _ChunkMsgNet {
    // XP_U16 msgID;
    XP_U16 len;
    XP_U8* data;
} ChunkMsgNet;

typedef enum { FORMAT_NONE, FORMAT_LOC, FORMAT_NET } CHUNK_FORMAT;

typedef struct _ChunkMsgArray {
    XP_U16 nMsgs;
    CHUNK_FORMAT format;
    union {
        ChunkMsgNet* msgsNet;
        ChunkMsgLoc* msgsLoc;
    } u;
} ChunkMsgArray;

MsgChunker* cnk_init( XW_DUtilCtxt* dutil, XWEnv xwe, XP_U32 waitSecs,
                      XP_U16 defaultMaxSize );
void cnk_free( MsgChunker* state );

void cnk_maxSizeChangedFor( MsgChunker* state, XWEnv xwe,
                            const XP_UCHAR* phone, XP_U16 mtu );

/* Return ptr to structure if one's ready to be sent, otherwise null. Caller *
 * should interpret null as meaning it's meant to call again. To support that,
 * null buf is legit.
 *
 * When send() returns a non-null value, that value must be passed to
 * freeMsgArray() or there will be leakage.
*/

ChunkMsgArray* cnk_prepOutbound( MsgChunker* state, XWEnv xwe, CHUNK_CMD cmd,
                                 XP_U32 gameID, const void* buf, XP_U16 buflen,
                                 const XP_UCHAR* toPhone, int port,
                                 XP_Bool forceOld, XP_U16* waitSecs );

/* When a message is received, pass it in for reassambly. Non-null return
   means one or more messages is ready for consumption. */
ChunkMsgArray* cnk_prepInbound( MsgChunker* state, XWEnv xwe, const XP_UCHAR* fromPhone,
                                XP_U16 wantPort, const XP_U8* data, XP_U16 len );

void cnk_freeMsgArray( MsgChunker* state, ChunkMsgArray* arr );

# ifdef DEBUG
void cnk_runTests( XW_DUtilCtxt* dutil, XWEnv xwe );
# else
#  define cnk_runTests(p1,p2 )
# endif
#endif
