/* 
 * Copyright 2001 - 2022 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _COMMS_H_
#define _COMMS_H_

#include "comtypes.h"
#include "commstyp.h"
#include "mempool.h"
#include "xwrelay.h"
#include "contrlrp.h"


EXTERN_C_START

#define CHANNEL_NONE ((XP_PlayerAddr)0)
#define CONN_ID_NONE 0L

typedef XP_U32 MsgID;           /* this is too big!!! PENDING */
typedef XP_U8 XWHostID;

typedef enum {
    COMMS_RELAYSTATE_UNCONNECTED
    , COMMS_RELAYSTATE_DENIED   /* terminal; new game or reset required to
                                   fix */
    , COMMS_RELAYSTATE_CONNECT_PENDING
    , COMMS_RELAYSTATE_CONNECTED
    , COMMS_RELAYSTATE_RECONNECTED
    , COMMS_RELAYSTATE_ALLCONNECTED
#ifdef RELAY_VIA_HTTP
    , COMMS_RELAYSTATE_USING_HTTP /* connection state doesn't matter */
#endif
} CommsRelayState;

#ifdef XWFEATURE_BLUETOOTH
# define XW_BT_NAME "CrossWords"
#endif

#ifdef COMMS_HEARTBEAT
# define IF_CH(a) a,
#else
# define IF_CH(a)
#endif

#ifdef COMMS_HEARTBEAT
typedef void (*TransportReset)( XWEnv xwe, void* closure );
#endif

typedef enum {
    COMMS_XPORT_FLAGS_NONE = 0
    ,COMMS_XPORT_FLAGS_HASNOCONN = 1
} CommsTransportFlags;

#ifdef COMMS_XPORT_FLAGSPROC
typedef XP_U32 (*FlagsProc)( XWEnv xwe, void* closure );
#endif

CommsCtxt* comms_make( XWEnv xwe, XW_UtilCtxt** utilp, XP_Bool isServer,
                       const CommsAddrRec* selfAddr,
                       const CommsAddrRec* hostAddr,
#ifdef XWFEATURE_RELAY
                       XP_U16 nPlayersHere, XP_U16 nPlayersTotal,
                       RoleChangeProc rcp, void* rcClosure,
#endif
                       XP_U16 forceChannel );

void comms_destroy( CommsCtxt* comms, XWEnv xwe );

void comms_setConnID( CommsCtxt* comms, XP_U32 connID, XP_U16 streamVersion );

void comms_getSelfAddr( const CommsCtxt* comms, CommsAddrRec* selfAddr );
XP_Bool comms_getHostAddr( const CommsCtxt* comms, CommsAddrRec* hostAddr );
void comms_addMQTTDevID( CommsCtxt* comms, XP_PlayerAddr channelNo,
                         const MQTTDevID* devID );

void comms_getAddrs( const CommsCtxt* comms, CommsAddrRec addr[],
                     XP_U16* nRecs );
XP_Bool comms_formatRelayID( const CommsCtxt* comms, XP_U16 indx,
                             XP_UCHAR* buf, XP_U16* lenp );

XP_U16 comms_countPendingPackets( RELCONST CommsCtxt* comms, XP_Bool* quashed );

typedef void (*GotPacketProc)( const XP_U8* buf, XP_U16 len, void* closure );
void comms_getPendingPacketsFor( RELCONST CommsCtxt* comms, XWEnv xwe,
                                 const CommsAddrRec* addr,
                                 GotPacketProc proc, void* closure );

#ifdef XWFEATURE_RELAY
XP_Bool comms_getRelayID( const CommsCtxt* comms, XP_UCHAR* buf, XP_U16* len );
#endif

void comms_dropHostAddr( CommsCtxt* comms, CommsConnType typ );
XP_Bool comms_getIsHost( const CommsCtxt* comms );

CommsCtxt* comms_makeFromStream( XWEnv xwe, XWStreamCtxt* stream,
                                 XW_UtilCtxt** utilp, XP_Bool isServer,
#ifdef XWFEATURE_RELAY
                                 RoleChangeProc rcp, void* rcClosure,
#endif
                                 XP_U16 forceChannel );
void comms_start( CommsCtxt* comms, XWEnv xwe );
void comms_stop( CommsCtxt* comms
#ifdef XWFEATURE_RELAY
                 , XWEnv xwe
#endif
                 );
void comms_writeToStream( CommsCtxt* comms, XWStreamCtxt* stream,
                          XP_U16 saveToken );
void comms_saveSucceeded( CommsCtxt* comms, XWEnv xwe, XP_U16 saveToken );

void addrFromStream( CommsAddrRec* addr, XWStreamCtxt* stream );
void addrToStream( XWStreamCtxt* stream, const CommsAddrRec* addr );
#ifdef XWFEATURE_COMMS_INVITE
void comms_invite( CommsCtxt* comms, XWEnv xwe, const NetLaunchInfo* nli,
                   const CommsAddrRec* destAddr, XP_Bool sendNow );
void comms_getInvited( RELCONST CommsCtxt* comms, XP_U16* nInvites );
typedef struct _InviteeNames {
    XP_UCHAR name[4][32];
    XP_U16 nNames;
} InviteeNames;
void comms_inviteeNames( CommsCtxt* comms, XWEnv xwe, InviteeNames* names );

#endif
XP_S16 comms_send( CommsCtxt* comms, XWEnv xwe, XWStreamCtxt* stream );
XP_S16 comms_resendAll( CommsCtxt* comms, XWEnv xwe, CommsConnType filter,
                        XP_Bool force );

XP_U16 comms_getChannelSeed( CommsCtxt* comms );
void comms_getChannelAddr( const CommsCtxt* comms, XP_PlayerAddr channelNo,
                           CommsAddrRec* addr );
XP_Bool addrsAreSame( XW_DUtilCtxt* dutil, XWEnv xwe, const CommsAddrRec* addr1,
                      const CommsAddrRec* addr2 );

#ifdef XWFEATURE_COMMSACK
void comms_ackAny( CommsCtxt* comms, XWEnv xwe );
#endif


XP_Bool comms_checkIncomingStream( CommsCtxt* comms, XWEnv xwe,
                                   XWStreamCtxt* stream,
                                   const CommsAddrRec* addr, 
                                   CommsMsgState* state,
                                   const MsgCountState* mcs );
void comms_msgProcessed( CommsCtxt* comms, CommsMsgState* state,
                         XP_Bool rejected );
XP_Bool comms_checkComplete( const CommsAddrRec* const addr );

XP_Bool comms_canChat( const CommsCtxt* comms );
XP_Bool comms_isConnected( const CommsCtxt* const comms );
XP_Bool comms_setQuashed( CommsCtxt* comms, XP_Bool quashed );

#ifdef RELAY_VIA_HTTP
void comms_gameJoined( CommsCtxt* comms, const XP_UCHAR* connname, XWHostID hid );
#endif

XP_Bool augmentAddr( CommsAddrRec* addr, const CommsAddrRec* newer,
                     XP_Bool isNewer );

XP_Bool addr_isEmpty( const CommsAddrRec* addr );
CommsConnType addr_getType( const CommsAddrRec* addr );
void addr_setType( CommsAddrRec* addr, CommsConnType type );
void addr_addType( CommsAddrRec* addr, CommsConnType type );
void addr_rmType( CommsAddrRec* addr, CommsConnType type );
XP_Bool addr_hasType( const CommsAddrRec* addr, CommsConnType type );
XP_Bool addr_iter( const CommsAddrRec* addr, CommsConnType* typp, 
                   XP_U32* state );
void addr_addMQTT( CommsAddrRec* addr, const MQTTDevID* devID );
void addr_addBT( CommsAddrRec* addr, const XP_UCHAR* btName,
                 const XP_UCHAR* btAddr );
void addr_addSMS( CommsAddrRec* addr, const XP_UCHAR* phone,
                 XP_U16 port );

void types_addType( ConnTypeSetBits* conTypes, CommsConnType type );
void types_rmType( ConnTypeSetBits* conTypes, CommsConnType type );
XP_Bool types_hasType( ConnTypeSetBits conTypes, CommsConnType type );
XP_Bool types_iter( ConnTypeSetBits conTypes, CommsConnType* typp, XP_U32* state );

const char* ConnType2Str( CommsConnType typ );

# ifdef DEBUG
void comms_getStats( RELCONST CommsCtxt* comms, XWStreamCtxt* stream );
const char* CommsRelayState2Str( CommsRelayState state );
const char* XWREASON2Str( XWREASON reason );

void comms_setAddrDisabled( CommsCtxt* comms, CommsConnType typ, 
                            XP_Bool send, XP_Bool enabled );
XP_Bool comms_getAddrDisabled( const CommsCtxt* comms, CommsConnType typ, 
                               XP_Bool send );
void logAddr( XW_DUtilCtxt* dutil, const CommsAddrRec* addr,
              const char* caller );
void logTypeSet( ConnTypeSetBits conTypes, XP_UCHAR buf[], XP_U16 bufLen );

# else
#  define comms_setAddrDisabled( comms, typ, send, enabled )
#  define comms_getAddrDisabled( comms, typ, send ) XP_FALSE
# endif

EXTERN_C_END

#endif /* _COMMS_H_ */
