/* -*- compile-command: "cd ../linux && make MEMDEBUG=TRUE -j3"; -*- */
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
#include "server.h"

EXTERN_C_START

#define CHANNEL_NONE ((XP_PlayerAddr)0)
#define CONN_ID_NONE 0L

typedef XP_U32 MsgID;           /* this is too big!!! PENDING */
typedef XP_U8 XWHostID;

typedef enum {
    COMMS_CONN_NONE           /* I want errors on uninited case */
    ,COMMS_CONN_IR            /* 1 */
    ,COMMS_CONN_IP_DIRECT       /* 2 */
    ,COMMS_CONN_RELAY           /* 3 */
    ,COMMS_CONN_BT              /* 4 */
    ,COMMS_CONN_SMS             /* 5 */
    ,COMMS_CONN_P2P             /* a.k.a. Wifi direct */
    ,COMMS_CONN_NFC             /* 7 */
    ,COMMS_CONN_MQTT            /* 8 */

    ,COMMS_CONN_NTYPES
} CommsConnType;

typedef XP_U8 CommsConnTypes;

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

#ifdef XWFEATURE_COMMS_INVITE
typedef XP_S16 (*TransportSendInvt)( XWEnv xwe, const NetLaunchInfo* nli,
                                     XP_U32 createdStamp,
                                     const CommsAddrRec* addr, void* closure );
#endif
typedef XP_S16 (*TransportSendMsg)( XWEnv xwe, const XP_U8* buf, XP_U16 len,
                                    const XP_UCHAR* msgNo, XP_U32 createdStamp,
                                    const CommsAddrRec* addr,
                                    CommsConnType conType, XP_U32 gameID,
                                    void* closure );

#ifdef COMMS_HEARTBEAT
typedef void (*TransportReset)( XWEnv xwe, void* closure );
#endif

#ifdef XWFEATURE_RELAY
typedef void (*RelayStatusProc)( XWEnv xwe, void* closure, CommsRelayState newState );
typedef void (*RelayConndProc)( XWEnv xwe, void* closure, XP_UCHAR* const room,
                                XP_Bool reconnect,
                                XP_U16 devOrder, /* 1 means created room, etc. */
                                XP_Bool allHere, XP_U16 nMissing );
typedef void (*RelayErrorProc)( XWEnv xwe, void* closure, XWREASON relayErr );
typedef XP_Bool (*RelayNoConnProc)( XWEnv xwe, const XP_U8* buf, XP_U16 len,
                                    const XP_UCHAR* msgNo,
                                    const XP_UCHAR* relayID, void* closure );
# ifdef RELAY_VIA_HTTP
typedef void (*RelayRequestJoinProc)( XWEnv xwe, void* closure, const XP_UCHAR* devID,
                                      const XP_UCHAR* room, XP_U16 nPlayersHere,
                                      XP_U16 nPlayersTotal, XP_U16 seed,
                                      XP_U16 lang );
# endif
#endif

typedef void (*MsgCountChange)( XWEnv xwe, void* closure, XP_U16 msgCount );
typedef void (*RoleChangeProc)( XWEnv xwe, void* closure, XP_Bool amNowGuest );

typedef enum {
    COMMS_XPORT_FLAGS_NONE = 0
    ,COMMS_XPORT_FLAGS_HASNOCONN = 1
} CommsTransportFlags;

#ifdef COMMS_XPORT_FLAGSPROC
typedef XP_U32 (*FlagsProc)( XWEnv xwe, void* closure );
#endif

typedef struct _TransportProcs {
# ifdef COMMS_XPORT_FLAGSPROC
    FlagsProc getFlags;
#else
    XP_U32 flags;
#endif
    TransportSendMsg sendMsg;
#ifdef XWFEATURE_COMMS_INVITE
    TransportSendInvt sendInvt;
#endif
    MsgCountChange countChanged;
#ifdef COMMS_HEARTBEAT
    TransportReset reset;
#endif
#ifdef XWFEATURE_RELAY
    RelayStatusProc rstatus;
    RelayConndProc rconnd;
    RelayErrorProc rerror;
    RelayNoConnProc sendNoConn;
# ifdef RELAY_VIA_HTTP
    RelayRequestJoinProc requestJoin;
# endif
#endif
    void* closure;
} TransportProcs;

CommsCtxt* comms_make( MPFORMAL XWEnv xwe, XW_UtilCtxt* util,
                       XP_Bool isServer,
                       const CommsAddrRec* selfAddr,
                       const CommsAddrRec* hostAddr,
#ifdef XWFEATURE_RELAY
                       XP_U16 nPlayersHere, XP_U16 nPlayersTotal,
#endif
                       const TransportProcs* procs,
                       RoleChangeProc rcp, void* rcClosure,
                       XP_U16 forceChannel
#ifdef SET_GAMESEED
                       ,XP_U16 gameSeed
#endif
                       );

void comms_reset( CommsCtxt* comms, XWEnv xwe, XP_Bool isServer
#ifdef XWFEATURE_RELAY
                  , XP_U16 nPlayersHere, XP_U16 nPlayersTotal
#endif
                  );
void comms_resetSame( CommsCtxt* comms, XWEnv xwe );

void comms_destroy( CommsCtxt* comms, XWEnv xwe );

void comms_setConnID( CommsCtxt* comms, XP_U32 connID );

void comms_getSelfAddr( const CommsCtxt* comms, CommsAddrRec* selfAddr );
XP_Bool comms_getHostAddr( const CommsCtxt* comms, CommsAddrRec* hostAddr );
void comms_addMQTTDevID( CommsCtxt* comms, XP_PlayerAddr channelNo,
                         const MQTTDevID* devID );

void comms_getAddrs( const CommsCtxt* comms, XWEnv xwe,
                     CommsAddrRec addr[], XP_U16* nRecs );
XP_Bool comms_formatRelayID( const CommsCtxt* comms, XP_U16 indx,
                             XP_UCHAR* buf, XP_U16* lenp );

XP_U16 comms_countPendingPackets( const CommsCtxt* comms );


#ifdef XWFEATURE_RELAY
XP_Bool comms_getRelayID( const CommsCtxt* comms, XP_UCHAR* buf, XP_U16* len );
#endif

CommsConnTypes comms_getConTypes( const CommsCtxt* comms );
void comms_dropHostAddr( CommsCtxt* comms, CommsConnType typ );
XP_Bool comms_getIsServer( const CommsCtxt* comms );

CommsCtxt* comms_makeFromStream( MPFORMAL XWEnv xwe, XWStreamCtxt* stream,
                                 XW_UtilCtxt* util, XP_Bool isServer,
                                 const TransportProcs* procs,
                                 RoleChangeProc rcp, void* rcClosure,
                                 XP_U16 forceChannel );
void comms_start( CommsCtxt* comms, XWEnv xwe );
void comms_stop( CommsCtxt* comms
#ifdef XWFEATURE_RELAY
                 , XWEnv xwe
#endif
                 );
void comms_writeToStream( CommsCtxt* comms, XWEnv xwe, XWStreamCtxt* stream,
                          XP_U16 saveToken );
void comms_saveSucceeded( CommsCtxt* comms, XWEnv xwe, XP_U16 saveToken );

void addrFromStream( CommsAddrRec* addr, XWStreamCtxt* stream );
void addrToStream( XWStreamCtxt* stream, const CommsAddrRec* addr );
#ifdef XWFEATURE_COMMS_INVITE
void comms_invite( CommsCtxt* comms, XWEnv xwe, const NetLaunchInfo* nli,
                   const CommsAddrRec* destAddr );
void comms_getInvited( const CommsCtxt* comms, /*XWEnv xwe, */
                       XP_U16* nInvites, CommsAddrRec* inviteRecs );
#endif
XP_S16 comms_send( CommsCtxt* comms, XWEnv xwe, XWStreamCtxt* stream );
XP_S16 comms_resendAll( CommsCtxt* comms, XWEnv xwe, CommsConnType filter,
                        XP_Bool force );

typedef void (*PendingMsgProc)( void* closure, XWEnv xwe, XP_U8* msg,
                                XP_U16 len, MsgID msgID );
void comms_getPending( CommsCtxt* comms, XWEnv xwe, PendingMsgProc proc, void* closure );
XP_U16 comms_getChannelSeed( CommsCtxt* comms );

#ifdef XWFEATURE_COMMSACK
void comms_ackAny( CommsCtxt* comms, XWEnv xwe );
#endif

typedef struct _CommsMsgState {
    struct AddressRecord* rec;
    XP_U32 msgID;
    XP_PlayerAddr channelNo;
    XP_U16 len;
#ifdef DEBUG
    const CommsCtxt* comms;
#endif
#ifdef COMMS_CHECKSUM
    XP_UCHAR sum[36];
#endif
} CommsMsgState;

XP_Bool comms_checkIncomingStream( CommsCtxt* comms, XWEnv xwe,
                                   XWStreamCtxt* stream,
                                   const CommsAddrRec* addr, 
                                   CommsMsgState* state );
void comms_msgProcessed( CommsCtxt* comms, XWEnv xwe,
                         CommsMsgState* state, XP_Bool rejected );
XP_Bool comms_checkComplete( const CommsAddrRec* const addr );

XP_Bool comms_canChat( const CommsCtxt* comms );
XP_Bool comms_isConnected( const CommsCtxt* const comms );

#ifdef RELAY_VIA_HTTP
void comms_gameJoined( CommsCtxt* comms, const XP_UCHAR* connname, XWHostID hid );
#endif

XP_Bool augmentAddr( CommsAddrRec* addr, const CommsAddrRec* newer,
                     XP_Bool isNewer );

CommsConnType addr_getType( const CommsAddrRec* addr );
void addr_setType( CommsAddrRec* addr, CommsConnType type );
void addr_addType( CommsAddrRec* addr, CommsConnType type );
void addr_rmType( CommsAddrRec* addr, CommsConnType type );
XP_Bool addr_hasType( const CommsAddrRec* addr, CommsConnType type );
XP_Bool addr_iter( const CommsAddrRec* addr, CommsConnType* typp, 
                   XP_U32* state );
void types_addType( XP_U16* conTypes, CommsConnType type );
void types_rmType( XP_U16* conTypes, CommsConnType type );
XP_Bool types_hasType( XP_U16 conTypes, CommsConnType type );
XP_Bool types_iter( XP_U32 conTypes, CommsConnType* typp, XP_U32* state );

#ifdef XWFEATURE_KNOWNPLAYERS
void comms_gatherPlayers( CommsCtxt* comms, XWEnv xwe, XP_U32 created );
#endif

const char* ConnType2Str( CommsConnType typ );

# ifdef DEBUG
void comms_getStats( CommsCtxt* comms, XWStreamCtxt* stream );
const char* CommsRelayState2Str( CommsRelayState state );
const char* XWREASON2Str( XWREASON reason );

void comms_setAddrDisabled( CommsCtxt* comms, CommsConnType typ, 
                            XP_Bool send, XP_Bool enabled );
XP_Bool comms_getAddrDisabled( const CommsCtxt* comms, CommsConnType typ, 
                               XP_Bool send );

# else
#  define comms_setAddrDisabled( comms, typ, send, enabled )
#  define comms_getAddrDisabled( comms, typ, send ) XP_FALSE
# endif

EXTERN_C_END

#endif /* _COMMS_H_ */
