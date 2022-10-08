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

#ifdef USE_STDIO
# include <stdio.h>
#endif

#include "comms.h"

#include "util.h"
#include "dutil.h"
#include "game.h"
#include "xwstream.h"
#include "memstream.h"
#include "xwrelay.h"
#include "strutils.h"
#include "dbgutil.h"
#include "knownplyr.h"
#include "device.h"
#include "nli.h"
#include "device.h"

#define HEARTBEAT_NONE 0

#define HAS_VERSION_FLAG 0xBEEF
#define COMMS_VERSION 2

/* Flags: 7 bits of header len, 6 bits of flags (3 used so far), and 3 bits of
   comms version */
#define VERSION_MASK 0x0007
#define NO_CONNID_BIT 0x0008
#define IS_SERVER_BIT 0x0010
#define NO_MSGID_BIT 0x0020
#define HEADER_LEN_OFFSET 9

/* Low two bits treated as channel, third as short-term flag indicating
 * sender's role; rest can be random to aid detection of duplicate packets. */
#define CHANNEL_MASK 0x0003

#ifndef XWFEATURE_STANDALONE_ONLY

#ifndef INITIAL_CLIENT_VERS
# define INITIAL_CLIENT_VERS 2
#endif

#ifdef COMMS_HEARTBEAT
/* It might make sense for this to be a parameter or somehow tied to the
   platform and transport.  But in that case it'd have to be passed across
   since all devices must agree. */
# ifndef HB_INTERVAL
#  define HB_INTERVAL 5
# endif
#endif

EXTERN_C_START

typedef struct MsgQueueElem {
    struct MsgQueueElem* next;
    XP_U8* msg;                 /* ptr to NetLaunchInfo if isInvite is true */
    XP_U16 len;                 /* stored as 0 if msg is a nli */
    XP_PlayerAddr channelNo;
#ifdef DEBUG
    XP_U16 sendCount;           /* how many times sent? */
#endif
    MsgID msgID;                /* saved for ease of deletion */
#ifdef COMMS_CHECKSUM
    XP_UCHAR* checksum;
#endif
    XP_U32 createdStamp;
} MsgQueueElem;

typedef struct AddressRecord {
    struct AddressRecord* next;
    CommsAddrRec addr;
    MsgID nextMsgID;        /* on a per-channel basis */
    MsgID lastMsgAckd;      /* on a per-channel basis */

    /* lastMsgRcd is the numerically highest MsgID we've seen.  Because once
     * it's sent in message as an ACK the other side will delete messages
     * based on it, we don't send a number higher than has actually been
     * written out successfully. lastMsgSaved is that number.
     */
    MsgID lastMsgRcd;
    MsgID lastMsgSaved;
    /* only used if COMMS_HEARTBEAT set except for serialization (to_stream) */
    XP_PlayerAddr channelNo;
    XP_U16 flags;                   /* storing only COMMS_VERSION */
    struct {
        XWHostID hostID;            /* used for relay case */
    } rr;
#ifdef COMMS_HEARTBEAT
    XP_Bool initialSeen;
#endif
} AddressRecord;

#define ADDRESSRECORD_SIZE_68K 20

struct CommsCtxt {
    XW_UtilCtxt* util;
    XW_DUtilCtxt* dutil;

    XP_U32 connID;             /* set from gameID: 0 means ignore; otherwise
                                  must match.  Set by server. */
    XP_PlayerAddr nextChannelNo;

    AddressRecord* recs;        /* return addresses */

    TransportProcs procs;

    RoleChangeProc rcProc;
    void* rcClosure;

    XP_U32 xportFlags;
#ifdef COMMS_HEARTBEAT
    XP_U32 lastMsgRcd;
#endif
    void* sendClosure;

    MsgQueueElem* msgQueueHead;
    MsgQueueElem* msgQueueTail;
    XP_U16 queueLen;
    XP_U16 channelSeed;         /* tries to be unique per device to aid
                                   dupe elimination at start */
    XP_U32 nextResend;          /* timestamp */
    XP_U16 resendBackoff;

#ifdef COMMS_HEARTBEAT
    XP_Bool doHeartbeat;
    XP_U32 lastMsgRcvdTime;
#endif
#if defined XWFEATURE_RELAY || defined COMMS_HEARTBEAT
    XP_Bool hbTimerPending;
    XP_Bool reconTimerPending;
#endif
    XP_U16 lastSaveToken;
    XP_U16 forceChannel;

    /* The following fields, down to isServer, are only used if
       XWFEATURE_RELAY is defined, but I'm leaving them in here so apps built
       both ways can open each other's saved games files.*/
    CommsAddrRec selfAddr;

    /* Stuff for relays */
    struct {
        XWHostID myHostID;          /* 0 if unset, 1 if acting as server.
                                       Client's 0 replaced by id assigned by
                                       relay. Relay calls this "srcID". */
        CommsRelayState relayState; /* not saved: starts at UNCONNECTED */
        CookieID cookieID;          /* not saved; temp standin for cookie; set
                                       by relay */
        /* permanent globally unique name, set by relay and forever after
           associated with this game.  Used to reconnect. */
        XP_UCHAR connName[MAX_CONNNAME_LEN+1];

        /* heartbeat: for periodic pings if relay thinks the network the
           device is on requires them.  Not saved since only valid when
           connected, and we reconnect for every game and after restarting. */
        XP_U16 heartbeat;
        XP_U16 nPlayersHere;
        XP_U16 nPlayersTotal;
        XP_Bool connecting;
    } rr;

    XP_U8 flags;

    XP_Bool isServer;
    XP_Bool disableds[COMMS_CONN_NTYPES][2];
#ifdef DEBUG
    XP_Bool processingMsg;
    const XP_UCHAR* tag;
#endif

    MPSLOT
};

#define FLAG_HARVEST_DONE 1

#if defined XWFEATURE_IP_DIRECT || defined XWFEATURE_DIRECTIP
typedef enum {
    BTIPMSG_NONE = 0
    ,BTIPMSG_DATA
    ,BTIPMSG_RESET
    ,BTIPMSG_HB
} BTIPMsgType;
#endif

#define TAGFMT(...) "<%s> (" #__VA_ARGS__ "): "
#define TAGPRMS comms->tag

/****************************************************************************
 *                               prototypes 
 ****************************************************************************/
static AddressRecord* rememberChannelAddress( CommsCtxt* comms, XWEnv xwe,
                                              XP_PlayerAddr channelNo, XWHostID id,
                                              const CommsAddrRec* addr, XP_U16 flags );
static void augmentChannelAddr( CommsCtxt* comms, AddressRecord* rec,
                                const CommsAddrRec* addr, XWHostID hostID );
static XP_Bool augmentAddrIntrnl( CommsCtxt* comms, CommsAddrRec* dest,
                                  const CommsAddrRec* src, XP_Bool isNewer );
static XP_Bool channelToAddress( CommsCtxt* comms, XWEnv xwe, XP_PlayerAddr channelNo,
                                 const CommsAddrRec** addr );
static AddressRecord* getRecordFor( CommsCtxt* comms, XWEnv xwe,
                                    const CommsAddrRec* addr, XP_PlayerAddr channelNo );
static void augmentSelfAddr( CommsCtxt* comms, XWEnv xwe, const CommsAddrRec* addr );
static XP_S16 sendMsg( CommsCtxt* comms, XWEnv xwe, MsgQueueElem* elem,
                       CommsConnType filter );
static MsgQueueElem* addToQueue( CommsCtxt* comms, XWEnv xwe, MsgQueueElem* newElem );
static XP_Bool elems_same( const MsgQueueElem* e1, const MsgQueueElem* e2 ) ;
static void freeElem( MPFORMAL MsgQueueElem* elem );
static void removeFromQueue( CommsCtxt* comms, XWEnv xwe, XP_PlayerAddr channelNo,
                             MsgID msgID );
static XP_U16 countAddrRecs( const CommsCtxt* comms );
static void sendConnect( CommsCtxt* comms, XWEnv xwe
#ifdef XWFEATURE_RELAY
                         , XP_Bool breakExisting
#endif
                         );
static void notifyQueueChanged( const CommsCtxt* comms, XWEnv xwe );
static XP_U16 makeFlags( const CommsCtxt* comms, XP_U16 headerLen,
                         MsgID msgID );

static XP_Bool formatRelayID( const CommsCtxt* comms, XWHostID hostID,
                              XP_UCHAR* buf, XP_U16* lenp );

#ifdef XWFEATURE_RELAY
static void set_reset_timer( CommsCtxt* comms, XWEnv xwe );
static XP_Bool sendNoConn( CommsCtxt* comms, XWEnv xwe,
                           const MsgQueueElem* elem, XWHostID destID );
static XP_Bool relayConnect( CommsCtxt* comms, XWEnv xwe );
static void relayDisconnect( CommsCtxt* comms, XWEnv xwe );
static XP_Bool send_via_relay( CommsCtxt* comms, XWEnv xwe, XWRELAY_Cmd cmd,
                               XWHostID destID, void* data, int dlen,
                               const XP_UCHAR* msgNo );
static XWHostID getDestID( CommsCtxt* comms, XWEnv xwe, XP_PlayerAddr channelNo );
# ifdef XWFEATURE_DEVID
static void putDevID( const CommsCtxt* comms, XWEnv xwe, XWStreamCtxt* stream );


# else
#  define putDevID( comms, xwe, stream )
# endif
# else
# define relayDisconnect( comms, xwe )
#endif

#ifdef DEBUG
static void assertAddrOk( const CommsAddrRec* addr );
static void listRecs( const CommsCtxt* comms, const char* msg );
#define ASSERT_ADDR_OK(addr) assertAddrOk( addr )

# ifdef XWFEATURE_RELAY
static const char* relayCmdToStr( XWRELAY_Cmd cmd );
# endif
static void printQueue( const CommsCtxt* comms );
static void logAddr( const CommsCtxt* comms, XWEnv xwe,
                     const CommsAddrRec* addr, const char* caller );
/* static void logAddrs( const CommsCtxt* comms, XWEnv xwe, */
/*                       const char* caller ); */

#else
#define ASSERT_ADDR_OK(addr)
#define printQueue( comms )
#define logAddr( comms, xwe, addr, caller)
#define logAddrs( comms, caller )
#endif
#if defined RELAY_HEARTBEAT || defined COMMS_HEARTBEAT
static void setHeartbeatTimer( CommsCtxt* comms );

#else
# define setHeartbeatTimer( comms )
#endif
#if defined XWFEATURE_IP_DIRECT || defined XWFEATURE_DIRECTIP
static XP_S16 send_via_bt_or_ip( CommsCtxt* comms, XWEnv xwe, BTIPMsgType msgTyp,
                                 XP_PlayerAddr channelNo,
                                 CommsConnType typ,
                                 void* data, int dlen, const XP_UCHAR* msgNo );
#endif

#if defined COMMS_HEARTBEAT || defined XWFEATURE_COMMSACK
static void sendEmptyMsg( CommsCtxt* comms, XWEnv xwe, AddressRecord* rec );
#endif
static inline XP_Bool IS_INVITE(const MsgQueueElem* elem)
{
    return 0 == elem->len;
}

/****************************************************************************
 *                               implementation 
 ****************************************************************************/

#ifdef DEBUG
# define CNO_FMT(buf, cno)                                         \
    XP_UCHAR (buf)[64];                                            \
    XP_SNPRINTF( (buf), sizeof(buf), "cno: %.4X|%x",               \
                 (cno) & ~CHANNEL_MASK, (cno) & CHANNEL_MASK )
#else
# define CNO_FMT(buf, cno)
#endif

#ifdef XWFEATURE_RELAY

#ifdef DEBUG
const char*
CommsRelayState2Str( CommsRelayState state )
{
#define CASE_STR(s) case s: return #s
    switch( state ) {
        CASE_STR(COMMS_RELAYSTATE_UNCONNECTED);
        CASE_STR(COMMS_RELAYSTATE_DENIED);
        CASE_STR(COMMS_RELAYSTATE_CONNECT_PENDING);
        CASE_STR(COMMS_RELAYSTATE_CONNECTED);
        CASE_STR(COMMS_RELAYSTATE_RECONNECTED);
        CASE_STR(COMMS_RELAYSTATE_ALLCONNECTED);
#ifdef RELAY_VIA_HTTP
        CASE_STR(COMMS_RELAYSTATE_USING_HTTP);
#endif
    default:
        XP_ASSERT(0); 
    }
#undef CASE_STR
    return NULL;
}

const char*
XWREASON2Str( XWREASON reason )
{
#define CASE_STR(s) case s: return #s
    switch( reason ) {
        CASE_STR(XWRELAY_ERROR_NONE);
        CASE_STR(XWRELAY_ERROR_OLDFLAGS);
        CASE_STR(XWRELAY_ERROR_BADPROTO);
        CASE_STR(XWRELAY_ERROR_RELAYBUSY);
        CASE_STR(XWRELAY_ERROR_SHUTDOWN);
        CASE_STR(XWRELAY_ERROR_TIMEOUT);
        CASE_STR(XWRELAY_ERROR_HEART_YOU);
        CASE_STR(XWRELAY_ERROR_HEART_OTHER);
        CASE_STR(XWRELAY_ERROR_LOST_OTHER);
        CASE_STR(XWRELAY_ERROR_OTHER_DISCON);
        CASE_STR(XWRELAY_ERROR_NO_ROOM);
        CASE_STR(XWRELAY_ERROR_DUP_ROOM);
        CASE_STR(XWRELAY_ERROR_TOO_MANY);
        CASE_STR(XWRELAY_ERROR_DELETED);
        CASE_STR(XWRELAY_ERROR_NORECONN);
        CASE_STR(XWRELAY_ERROR_DEADGAME);
        CASE_STR(XWRELAY_ERROR_LASTERR);
    default:
        XP_ASSERT(0);
    }
#undef CASE_STR
    return NULL;
}
#endif

static void
set_relay_state( CommsCtxt* comms, XWEnv xwe, CommsRelayState state )
{
    if ( comms->rr.relayState != state ) {
        XP_LOGFF( TAGFMT() "%s => %s", TAGPRMS,
                  CommsRelayState2Str(comms->rr.relayState),
                  CommsRelayState2Str(state) );
        comms->rr.relayState = state;
        if ( !!comms->procs.rstatus ) {
            (*comms->procs.rstatus)( xwe, comms->procs.closure, state );
        }
    }
}

static void
init_relay( CommsCtxt* comms, XWEnv xwe, XP_U16 nPlayersHere, XP_U16 nPlayersTotal )
{
    comms->rr.myHostID = comms->isServer? HOST_ID_SERVER: HOST_ID_NONE;
    if ( HOST_ID_NONE != comms->rr.myHostID ) {
        XP_LOGFF( "set hostid: %x", comms->rr.myHostID );
    }
    set_relay_state( comms, xwe, COMMS_RELAYSTATE_UNCONNECTED );
    comms->rr.nPlayersHere = nPlayersHere;
    comms->rr.nPlayersTotal = nPlayersTotal;
    comms->rr.cookieID = COOKIE_ID_NONE;
    comms->rr.connName[0] = '\0';
}
#endif

CommsCtxt* 
comms_make( MPFORMAL XWEnv xwe, XW_UtilCtxt* util, XP_Bool isServer,
            const CommsAddrRec* selfAddr, const CommsAddrRec* hostAddr,
#ifdef XWFEATURE_RELAY
            XP_U16 nPlayersHere, XP_U16 nPlayersTotal,
#endif
            const TransportProcs* procs,
            RoleChangeProc rcp, void* rcClosure,
            XP_U16 forceChannel
#ifdef SET_GAMESEED
            , XP_U16 gameSeed
#endif
            )
{
    CommsCtxt* comms = (CommsCtxt*)XP_CALLOC( mpool, sizeof(*comms) );
#ifdef DEBUG
    comms->tag = mpool_getTag(mpool);
    XP_LOGFF( TAGFMT(isServer=%d; forceChannel=%d), TAGPRMS, isServer, forceChannel );
#endif
    MPASSIGN(comms->mpool, mpool);

    XP_ASSERT( 0 == (forceChannel & ~CHANNEL_MASK) );
    comms->isServer = isServer;
    comms->forceChannel = forceChannel;
    if ( !!procs ) {
        XP_MEMCPY( &comms->procs, procs, sizeof(comms->procs) );
#ifdef COMMS_XPORT_FLAGSPROC
        comms->xportFlags = (*comms->procs.getFlags)(xwe, comms->procs.closure);
#else
        comms->xportFlags = comms->procs.flags;
#endif
    }
    XP_ASSERT( rcp );
    comms->rcProc = rcp;
    comms->rcClosure = rcClosure;

    comms->util = util;
    comms->dutil = util_getDevUtilCtxt( util, xwe );

#ifdef XWFEATURE_RELAY
    init_relay( comms, xwe, nPlayersHere, nPlayersTotal );
# ifdef SET_GAMESEED
    comms->channelSeed = gameSeed;
# endif
#endif

    if ( !!selfAddr ) {
        logAddr( comms, xwe, selfAddr, __func__ );
        augmentSelfAddr( comms, xwe, selfAddr );
    }
    if ( !!hostAddr ) {
        XP_ASSERT( !isServer );
        logAddr( comms, xwe, hostAddr, __func__ );
        XP_PlayerAddr channelNo = comms_getChannelSeed( comms );
#ifdef DEBUG
        AddressRecord* rec = 
#endif
            rememberChannelAddress( comms, xwe, channelNo, 0, hostAddr,0 );
        XP_ASSERT( rec == getRecordFor( comms, xwe, hostAddr, channelNo ) );
    }

    return comms;
} /* comms_make */

static void
cleanupInternal( CommsCtxt* comms ) 
{
    MsgQueueElem* msg;
    MsgQueueElem* next;

    for ( msg = comms->msgQueueHead; !!msg; msg = next ) {
        next = msg->next;
        freeElem( MPPARM(comms->mpool) msg );
    }
    comms->queueLen = 0;
    comms->msgQueueHead = comms->msgQueueTail = (MsgQueueElem*)NULL;
} /* cleanupInternal */

static void
cleanupAddrRecs( CommsCtxt* comms )
{
    AddressRecord* recs;
    AddressRecord* next;

    for ( recs = comms->recs; !!recs; recs = next ) {
        next = recs->next;
        XP_FREE( comms->mpool, recs );
    }
    comms->recs = (AddressRecord*)NULL;
} /* cleanupAddrRecs */

/* static void */
/* removeAddrRec( CommsCtxt* comms, XWEnv XP_UNUSED_DBG(xwe), AddressRecord* rec ) */
/* { */
/*     XP_LOGFF( TAGFMT(%p), TAGPRMS, rec ); */
/* #ifdef DEBUG */
/*     logAddrs( comms, xwe, "BEFORE" ); */
/*     XP_U16 nBefore = countAddrRecs( comms ); */
/* #endif */
/*     AddressRecord** curp = &comms->recs; */
/*     while ( NULL != *curp ) { */
/*         if ( rec == *curp ) { */
/*             *curp = rec->next; */
/*             XP_FREE( comms->mpool, rec ); */
/*             break; */
/*         } */
/*         curp = &(*curp)->next; */
/*     } */
/* #ifdef DEBUG */
/*     XP_U16 nAfter = countAddrRecs( comms ); */
/*     XP_ASSERT( (nAfter + 1) == nBefore ); */
/*     logAddrs( comms, xwe, "AFTER" ); */
/* #endif */
/* } */

void
comms_resetSame( CommsCtxt* comms, XWEnv xwe )
{
    comms_reset( comms, xwe, comms->isServer
#ifdef XWFEATURE_RELAY
                 , comms->rr.nPlayersHere, comms->rr.nPlayersTotal
#endif
                 );
}

static void
reset_internal( CommsCtxt* comms, XWEnv xwe, XP_Bool isServer
#ifdef XWFEATURE_RELAY
                , XP_U16 nPlayersHere, XP_U16 nPlayersTotal,
                XP_Bool resetRelay
#endif
                )
{
    LOG_FUNC();
#ifdef XWFEATURE_RELAY
    if ( resetRelay ) {
        relayDisconnect( comms, xwe );
    }
#else
    XP_USE(xwe);
#endif

    cleanupInternal( comms );
    comms->isServer = isServer;

    cleanupAddrRecs( comms );

    if ( 0 != comms->nextChannelNo ) {
        XP_LOGFF( "comms->nextChannelNo: %d", comms->nextChannelNo );
    }
    /* This tends to fire when games reconnect to the relay after the DB's
       been wiped and connect in a different order from that in which they did
       originally. So comment it out. */
    // XP_ASSERT( 0 == comms->nextChannelNo );
    // comms->nextChannelNo = 0;
#ifdef XWFEATURE_RELAY
    if ( resetRelay ) {
        comms->channelSeed = 0;
    }
#endif
    comms->connID = CONN_ID_NONE;
#ifdef XWFEATURE_RELAY
    if ( resetRelay ) {
        init_relay( comms, xwe, nPlayersHere, nPlayersTotal );
    }
#endif
    LOG_RETURN_VOID();
} /* reset_internal */

void
comms_reset( CommsCtxt* comms, XWEnv xwe, XP_Bool isServer
#ifdef XWFEATURE_RELAY
             , XP_U16 nPlayersHere, XP_U16 nPlayersTotal
#endif
             )
{
    reset_internal( comms, xwe, isServer
#ifdef XWFEATURE_RELAY
                    , nPlayersHere, nPlayersTotal, XP_TRUE
#endif
                    );
}

#ifdef XWFEATURE_RELAY

static XP_Bool
p_comms_resetTimer( void* closure, XWEnv xwe, XWTimerReason XP_UNUSED_DBG(why) )
{
    CommsCtxt* comms = (CommsCtxt*)closure;
    LOG_FUNC();
    XP_ASSERT( why == TIMER_COMMS );

    /* Once we're denied we don't try again.  A new game or save and re-open
       will reset comms and get us out of this state. */
    if ( comms->rr.relayState != COMMS_RELAYSTATE_DENIED ) {
        XP_Bool success = comms->rr.relayState >= COMMS_RELAYSTATE_CONNECTED
            || relayConnect( comms, xwe );

        if ( success ) {
            comms->reconTimerPending = XP_FALSE;
            setHeartbeatTimer( comms );  /* in case we killed it with this
                                            one.... */
        } else {
            set_reset_timer( comms, xwe );
        }
    }

    return XP_FALSE;            /* no redraw required */
} /* p_comms_resetTimer */

static void
set_reset_timer( CommsCtxt* comms, XWEnv xwe )
{
    /* This timer is allowed to overwrite a heartbeat timer, but not
       vice-versa.  Make sure we can restart it. */
    comms->hbTimerPending = XP_FALSE;
    util_setTimer( comms->util, xwe, TIMER_COMMS, 15,
                   p_comms_resetTimer, comms );
    comms->reconTimerPending = XP_TRUE;
} /* set_reset_timer */
#endif  /* XWFEATURE_RELAY */

void
comms_transportFailed( CommsCtxt* comms,
#ifdef XWFEATURE_RELAY
                       XWEnv xwe,
#endif
                       CommsConnType failed )
{
    XP_LOGFF( "(%s)", ConnType2Str(failed) );
    XP_ASSERT( !!comms );
    if ( COMMS_CONN_RELAY == failed && addr_hasType( &comms->selfAddr, COMMS_CONN_RELAY )
         && comms->rr.relayState != COMMS_RELAYSTATE_DENIED ) {
        relayDisconnect( comms, xwe );
#ifdef XWFEATURE_RELAY
        set_reset_timer( comms, xwe );
#endif  /* XWFEATURE_RELAY */
    }
    LOG_RETURN_VOID();
}

void
comms_destroy( CommsCtxt* comms, XWEnv xwe )
{
    /* did I call comms_stop()? */
    XP_ASSERT( ! addr_hasType( &comms->selfAddr, COMMS_CONN_RELAY )
               || COMMS_RELAYSTATE_UNCONNECTED == comms->rr.relayState );

    CommsAddrRec aNew = {0};
    util_addrChange( comms->util, xwe, &comms->selfAddr, &aNew );

    cleanupInternal( comms );
    cleanupAddrRecs( comms );

    util_clearTimer( comms->util, xwe, TIMER_COMMS );

    XP_FREE( comms->mpool, comms );
} /* comms_destroy */

void
comms_setConnID( CommsCtxt* comms, XP_U32 connID )
{
    XP_ASSERT( CONN_ID_NONE != connID );
    XP_ASSERT( 0 == comms->connID || connID == comms->connID );
    comms->connID = connID;
    XP_LOGFF( "set connID (gameID) to %x", connID );
} /* comms_setConnID */

static void
addrFromStreamOne( CommsAddrRec* addrP, XWStreamCtxt* stream, CommsConnType typ )
{
    XP_U16 version = stream_getVersion( stream );
    switch( typ ) {
    case COMMS_CONN_NONE:
        break;
    case COMMS_CONN_BT:
        stringFromStreamHere( stream, addrP->u.bt.hostName,
                              sizeof(addrP->u.bt.hostName) );
        stringFromStreamHere( stream, addrP->u.bt.btAddr.chars,
                              sizeof(addrP->u.bt.btAddr.chars) );
        break;
    case COMMS_CONN_IR:
        /* nothing to save */
        break;
    case COMMS_CONN_IP_DIRECT:
        stringFromStreamHere( stream, addrP->u.ip.hostName_ip,
                              sizeof(addrP->u.ip.hostName_ip) );
        addrP->u.ip.ipAddr_ip = stream_getU32( stream );
        addrP->u.ip.port_ip = stream_getU16( stream );
        break;
    case COMMS_CONN_RELAY: {
        IpRelay ip_relay = {{0}};
        stringFromStreamHere( stream, ip_relay.invite,
                              sizeof(ip_relay.invite) );
        stringFromStreamHere( stream, ip_relay.hostName,
                              sizeof(ip_relay.hostName) );
        ip_relay.ipAddr = stream_getU32( stream );
        ip_relay.port = stream_getU16( stream );
        if ( version >= STREAM_VERS_DICTLANG ) {
            ip_relay.seeksPublicRoom = stream_getBits( stream, 1 );
            ip_relay.advertiseRoom = stream_getBits( stream, 1 );
        }
#ifdef XWFEATURE_RELAY
        XP_MEMCPY( &addrP->u.ip_relay, &ip_relay, sizeof(ip_relay) );
#endif
    }
        break;
    case COMMS_CONN_SMS:
        stringFromStreamHere( stream, addrP->u.sms.phone, 
                              sizeof(addrP->u.sms.phone) );
        addrP->u.sms.port = stream_getU16( stream );
        break;
    case COMMS_CONN_P2P:
        stringFromStreamHere( stream, addrP->u.p2p.mac_addr,
                              sizeof(addrP->u.p2p.mac_addr) );
        break;
    case COMMS_CONN_NFC:
        break;
    case COMMS_CONN_MQTT:
        stream_getBytes( stream, &addrP->u.mqtt.devID, sizeof(addrP->u.mqtt.devID) );
        break;
    default:
        /* shut up, compiler */
        break;
    }
} /* addrFromStreamOne */

void
addrFromStream( CommsAddrRec* addrP, XWStreamCtxt* stream )
{
    XP_U8 tmp = stream_getU8( stream );
    if ( (STREAM_VERS_MULTIADDR > stream_getVersion( stream )) 
         && (COMMS_CONN_NONE != tmp) ) {
        tmp = 1 << (tmp - 1);
    }
    addrP->_conTypes = tmp;

    CommsConnType typ;
    for ( XP_U32 st = 0; addr_iter( addrP, &typ, &st ); ) {
        addrFromStreamOne( addrP, stream, typ );
    }
    // ASSERT_ADDR_OK( addrP );
}

/* Return TRUE if there are no addresses left that include relay */
static XP_Bool
removeRelayIf( CommsCtxt* comms, XWEnv xwe )
{
    XP_Bool allRemoved = XP_TRUE;
    XP_UCHAR bufs[MAX_NUM_PLAYERS+1][64];
    const XP_UCHAR* ptrs[MAX_NUM_PLAYERS+1];
    int nIds = 0;
    for ( AddressRecord* rec = comms->recs; !!rec; rec = rec->next ) {
        CommsAddrRec* addr = &rec->addr;
        if ( addr_hasType( addr, COMMS_CONN_RELAY ) ) {
            if ( addr_hasType( addr, COMMS_CONN_MQTT )
                 && 0 != addr->u.mqtt.devID ) {
                addr_rmType( addr, COMMS_CONN_RELAY );
            } else {
                XP_U16 len = VSIZE(bufs[nIds]);
                if ( formatRelayID( comms, rec->rr.hostID, bufs[nIds], &len ) ) {
                    ptrs[nIds] = &bufs[nIds][0];
                    ++nIds;
                }
                allRemoved = XP_FALSE;
            }
        }
    }
    if ( 0 < nIds ) {
        util_getMQTTIDsFor( comms->util, xwe, nIds, ptrs );
    }
    LOG_RETURNF( "%s", boolToStr(allRemoved) );
    return allRemoved;
} /* removeRelayIf */

CommsCtxt* 
comms_makeFromStream( MPFORMAL XWEnv xwe, XWStreamCtxt* stream,
                      XW_UtilCtxt* util, XP_Bool isServer,
                      const TransportProcs* procs,
                      RoleChangeProc rcp, void* rcClosure,
                      XP_U16 forceChannel )
{
    LOG_FUNC();
    XP_U16 version = stream_getVersion( stream );

    XP_U8 flags = stream_getU8( stream );
    if ( version < STREAM_VERS_GICREATED ) {
        flags = 0;
    }

    CommsAddrRec addr = {0};
    addrFromStream( &addr, stream );
    if ( addr_hasType( &addr, COMMS_CONN_MQTT ) && 0 == addr.u.mqtt.devID ) {
        XW_DUtilCtxt* dutil = util_getDevUtilCtxt( util, xwe );
        dvc_getMQTTDevID( dutil, xwe, &addr.u.mqtt.devID );
    }
    ASSERT_ADDR_OK( &addr );

    XP_U16 nPlayersHere, nPlayersTotal;
    if ( version >= STREAM_VERS_DEVIDS
         || addr_hasType( &addr, COMMS_CONN_RELAY ) ) {
        nPlayersHere = (XP_U16)stream_getBits( stream, 4 );
        nPlayersTotal = (XP_U16)stream_getBits( stream, 4 );
    } else {
        nPlayersHere = 0;
        nPlayersTotal = 0;
    }
    CommsCtxt* comms = comms_make( MPPARM(mpool) xwe, util, isServer, NULL, NULL,
#ifdef XWFEATURE_RELAY
                                   nPlayersHere, nPlayersTotal,
#endif
                                   procs, rcp, rcClosure, forceChannel
#ifdef SET_GAMESEED
                                   , 0
#endif
                                   );
#ifndef XWFEATURE_RELAY
    XP_USE( nPlayersHere );
    XP_USE( nPlayersTotal );
#endif
    XP_MEMCPY( &comms->selfAddr, &addr, sizeof(comms->selfAddr) );
    logAddr( comms, xwe, &addr, __func__ );
    comms->flags = flags;

    comms->connID = stream_getU32( stream );
    comms->nextChannelNo = stream_getU16( stream );
    if ( version < STREAM_VERS_CHANNELSEED ) {
        comms->channelSeed = 0;
    } else {
        comms->channelSeed = stream_getU16( stream );
        // CNO_FMT( cbuf, comms->channelSeed );
    }
    if ( STREAM_VERS_COMMSBACKOFF <= version ) {
        comms->resendBackoff = stream_getU16( stream );
        comms->nextResend = stream_getU32( stream );
    }
    if ( addr_hasType(&addr, COMMS_CONN_RELAY ) ) {
        comms->rr.myHostID = stream_getU8( stream );
        XP_LOGFF( "loaded myHostID: %d", comms->rr.myHostID );
        stringFromStreamHere( stream, comms->rr.connName, 
                              sizeof(comms->rr.connName) );
    }

    comms->queueLen = stream_getU8( stream );

    XP_U16 nAddrRecs = stream_getU8( stream );
    XP_LOGFF( "nAddrRecs: %d", nAddrRecs );
    AddressRecord** prevsAddrNext = &comms->recs;
    for ( int ii = 0; ii < nAddrRecs; ++ii ) {
        AddressRecord* rec = (AddressRecord*)XP_CALLOC( mpool, sizeof(*rec));

        addrFromStream( &rec->addr, stream );
        logAddr( comms, xwe, &rec->addr, __func__ );

        if ( STREAM_VERS_SMALLCOMMS <= version ) {
            rec->nextMsgID = stream_getU32VL( stream );
            rec->lastMsgSaved = rec->lastMsgRcd = stream_getU32VL( stream );
            rec->flags = stream_getU16( stream );
        } else {
            rec->nextMsgID = stream_getU16( stream );
            rec->lastMsgSaved = rec->lastMsgRcd = stream_getU16( stream );
        }
#ifdef LOG_COMMS_MSGNOS
        XP_LOGFF( "read lastMsgRcd of %d for addr %d", rec->lastMsgRcd, ii );
#endif
        if ( version >= STREAM_VERS_BLUETOOTH2 ) {
            rec->lastMsgAckd = stream_getU16( stream );
        }
        rec->channelNo = stream_getU16( stream );
        if ( addr_hasType( &rec->addr, COMMS_CONN_RELAY ) ) {
            rec->rr.hostID = stream_getU8( stream );
        }

        CNO_FMT( cbuf, rec->channelNo );
        XP_LOGFF( "loaded rec %d: %s", ii, cbuf );

        *prevsAddrNext = rec;
        prevsAddrNext = &rec->next;
    }

    MsgQueueElem** prevsQueueNext = &comms->msgQueueHead;
    for ( int ii = 0; ii < comms->queueLen; ++ii ) {
        MsgQueueElem* msg = (MsgQueueElem*)XP_CALLOC( mpool, sizeof(*msg) );

        msg->channelNo = stream_getU16( stream );
        if ( version >= STREAM_VERS_SMALLCOMMS ) {
            msg->msgID = stream_getU32VL( stream );
            msg->len = stream_getU32VL( stream );
        } else {
            msg->msgID = stream_getU32( stream );
            msg->len = stream_getU16( stream );
        }
        if ( version >= STREAM_VERS_MSGTIMESTAMP ) {
            msg->createdStamp = stream_getU32( stream );
        }
#ifdef DEBUG
        msg->sendCount = 0;
#endif
        XP_U16 len = msg->len;
        if ( 0 == len ) {
            XP_U32 nliLen = stream_getU32VL( stream );
            XWStreamCtxt* nliStream = mem_stream_make_raw( MPPARM(comms->mpool)
                                                           dutil_getVTManager(comms->dutil));
            stream_getFromStream( nliStream, stream, nliLen );
            NetLaunchInfo nli;
            if ( nli_makeFromStream( &nli, nliStream ) ) {
                msg->msg = (XP_U8*)XP_MALLOC( mpool, sizeof(nli) );
                XP_MEMCPY( msg->msg, &nli, sizeof(nli) );
                len = sizeof(nli); /* needed for checksum calc */
            } else {
                XP_ASSERT(0);
            }
            stream_destroy( nliStream, xwe );
        } else {
            msg->msg = (XP_U8*)XP_MALLOC( mpool, len );
            stream_getBytes( stream, msg->msg, len );
        }
#ifdef COMMS_CHECKSUM
        msg->checksum = dutil_md5sum( comms->dutil, xwe, msg->msg, len );
#endif
        msg->next = (MsgQueueElem*)NULL;
        *prevsQueueNext = comms->msgQueueTail = msg;
        comms->msgQueueTail = msg;
        prevsQueueNext = &msg->next;
    }

    if ( STREAM_VERS_DISABLEDS <= version ) {
        for ( CommsConnType typ = (CommsConnType)0; typ < VSIZE(comms->disableds); ++typ ) {
            if ( typ < COMMS_CONN_NFC || addr_hasType( &comms->selfAddr, typ ) ) {
                for ( int ii = 0; ii < VSIZE(comms->disableds[0]); ++ii ) {
                    comms->disableds[typ][ii] = 0 != stream_getBits( stream, 1 );
                }
            }
        }
    }

    notifyQueueChanged( comms, xwe );
    if ( addr_hasType( &comms->selfAddr, COMMS_CONN_RELAY )
         && removeRelayIf( comms, xwe ) ) {
        addr_rmType( &comms->selfAddr, COMMS_CONN_RELAY );
    }

    listRecs( comms, __func__ );

    LOG_RETURNF( "%p", comms );
    return comms;
} /* comms_makeFromStream */

#ifdef COMMS_HEARTBEAT
static void
setDoHeartbeat( CommsCtxt* comms )
{
    CommsConnType conType = comms->selfAddr.conType;
    comms->doHeartbeat = XP_FALSE
        || COMMS_CONN_IP_DIRECT == conType
        || COMMS_CONN_BT == conType
        ;
}
#else
# define setDoHeartbeat(c)
#endif

/* 
 * Currently this disconnects an open connection.  Don't do that.
 */
void
comms_start( CommsCtxt* comms, XWEnv xwe )
{
    XP_ASSERT( !!comms );
    setDoHeartbeat( comms );
    sendConnect( comms, xwe
#ifdef XWFEATURE_RELAY
                 , XP_FALSE
#endif
                 );
} /* comms_start */

void
comms_stop( CommsCtxt* comms
#ifdef XWFEATURE_RELAY
            , XWEnv xwe
#endif
            )
{
    if ( addr_hasType( &comms->selfAddr, COMMS_CONN_RELAY ) ) {
        relayDisconnect( comms, xwe );
    }
}

static void
sendConnect( CommsCtxt* comms, XWEnv xwe
#ifdef XWFEATURE_RELAY
             , XP_Bool breakExisting
#endif
             )
{
    // CommsAddrRec addr = comms->selfAddr;
    CommsConnType typ;
    for ( XP_U32 st = 0; addr_iter( &comms->selfAddr, &typ, &st ); ) {
        // addr._conTypes = typ;
        switch( typ ) {
#ifdef XWFEATURE_RELAY
        case COMMS_CONN_RELAY:
            if ( breakExisting
                 || COMMS_RELAYSTATE_UNCONNECTED == comms->rr.relayState ) {
                set_relay_state( comms, xwe, COMMS_RELAYSTATE_UNCONNECTED );
                if ( !relayConnect( comms, xwe ) ) {
                    XP_LOGFF( "relayConnect failed" );
                    set_reset_timer( comms, xwe );
                }
            }
            break;
#endif
#if defined XWFEATURE_IP_DIRECT || defined XWFEATURE_DIRECTIP
        case COMMS_CONN_BT:
        case COMMS_CONN_IP_DIRECT:
            /* This will only work on host side when there's a single guest! */
            (void)send_via_bt_or_ip( comms, xwe, BTIPMSG_RESET, CHANNEL_NONE, typ, NULL, 0, NULL );
            (void)comms_resendAll( comms, xwe, COMMS_CONN_NONE, XP_FALSE );
            break;
#endif
#if defined XWFEATURE_SMS
        case COMMS_CONN_SMS:
            (void)comms_resendAll( comms, xwe, COMMS_CONN_NONE, XP_FALSE );
            break;
#endif
        default:
            break;
        }
    }

    setHeartbeatTimer( comms );
} /* sendConnect */

static void
addrToStreamOne( XWStreamCtxt* stream, CommsConnType typ,
                 const CommsAddrRec* addrP )
{
    switch( typ ) {
    case COMMS_CONN_NONE:
        /* nothing to write */
        break;
    case COMMS_CONN_BT:
        stringToStream( stream, addrP->u.bt.hostName );
        /* sizeof(.bits) below defeats ARM's padding. */
        stringToStream( stream, addrP->u.bt.btAddr.chars );
        break;
    case COMMS_CONN_IR:
        /* nothing to save */
        break;
    case COMMS_CONN_IP_DIRECT:
        stringToStream( stream, addrP->u.ip.hostName_ip );
        stream_putU32( stream, addrP->u.ip.ipAddr_ip );
        stream_putU16( stream, addrP->u.ip.port_ip );
        break;
    case COMMS_CONN_RELAY:
#ifdef XWFEATURE_RELAY
        stringToStream( stream, addrP->u.ip_relay.invite );
        stringToStream( stream, addrP->u.ip_relay.hostName );
        stream_putU32( stream, addrP->u.ip_relay.ipAddr );
        stream_putU16( stream, addrP->u.ip_relay.port );
        stream_putBits( stream, 1, addrP->u.ip_relay.seeksPublicRoom );
        stream_putBits( stream, 1, addrP->u.ip_relay.advertiseRoom );
#endif
        break;
    case COMMS_CONN_SMS:
        stringToStream( stream, addrP->u.sms.phone );
        stream_putU16( stream, addrP->u.sms.port );
        break;
    case COMMS_CONN_P2P:
        stringToStream( stream, addrP->u.p2p.mac_addr );
        break;
    case COMMS_CONN_NFC:
        break;
    case COMMS_CONN_MQTT:
        XP_ASSERT( 0 != addrP->u.mqtt.devID );
        stream_putBytes( stream, &addrP->u.mqtt.devID, sizeof(addrP->u.mqtt.devID) );
        break;
    default:
        XP_LOGFF( "unexpected typ: %s", ConnType2Str(typ) );
        XP_ASSERT(0);           /* firing */
        break;
    }
} /* addrToStreamOne */

void
addrToStream( XWStreamCtxt* stream, const CommsAddrRec* addrP )
{
    stream_setVersion( stream, CUR_STREAM_VERS );
    stream_putU8( stream, addrP->_conTypes );

    CommsConnType typ;
    for ( XP_U32 st = 0; addr_iter( addrP, &typ, &st ); ) {
        addrToStreamOne( stream, typ, addrP );
    }
}

void
comms_writeToStream( CommsCtxt* comms, XWEnv xwe,
                     XWStreamCtxt* stream, XP_U16 saveToken )
{
    XP_U16 nAddrRecs;
    AddressRecord* rec;
    MsgQueueElem* msg;

    listRecs( comms, __func__ );

    stream_setVersion( stream, CUR_STREAM_VERS );

    stream_putU8( stream, comms->flags );
    logAddr( comms, xwe, &comms->selfAddr, __func__ );
    addrToStream( stream, &comms->selfAddr );
    stream_putBits( stream, 4, comms->rr.nPlayersHere );
    stream_putBits( stream, 4, comms->rr.nPlayersTotal );

    stream_putU32( stream, comms->connID );
    stream_putU16( stream, comms->nextChannelNo );
    XP_U16 channelSeed = comms_getChannelSeed( comms ); /* force creation */
    stream_putU16( stream, channelSeed );
    stream_putU16( stream, comms->resendBackoff );
    stream_putU32( stream, comms->nextResend );
    if ( addr_hasType( &comms->selfAddr, COMMS_CONN_RELAY ) ) {
        stream_putU8( stream, comms->rr.myHostID );
        XP_LOGFF( "stored myHostID: %d", comms->rr.myHostID );
        stringToStream( stream, comms->rr.connName );
    }

    XP_ASSERT( comms->queueLen <= 255 );
    stream_putU8( stream, (XP_U8)comms->queueLen );

    nAddrRecs = countAddrRecs(comms);
    stream_putU8( stream, (XP_U8)nAddrRecs );

#ifdef LOG_COMMS_MSGNOS
    int ii = 0;
#endif
    for ( rec = comms->recs; !!rec; rec = rec->next ) {

        const CommsAddrRec* addr = &rec->addr;
        logAddr( comms, xwe, addr, __func__ );
        addrToStream( stream, addr );

        stream_putU32VL( stream, rec->nextMsgID );
        stream_putU32VL( stream, rec->lastMsgRcd );
        stream_putU16( stream, rec->flags );
#ifdef LOG_COMMS_MSGNOS
        XP_LOGFF( "wrote lastMsgRcd of %d for addr %d", rec->lastMsgRcd, ii++ );
#endif
        stream_putU16( stream, (XP_U16)rec->lastMsgAckd );
        stream_putU16( stream, rec->channelNo );
        if ( addr_hasType( addr, COMMS_CONN_RELAY ) ) {
            XP_ASSERT(0);
            stream_putU8( stream, rec->rr.hostID ); /* unneeded unless RELAY */
        }
    }

    for ( msg = comms->msgQueueHead; !!msg; msg = msg->next ) {
        stream_putU16( stream, msg->channelNo );
        stream_putU32VL( stream, msg->msgID );

        stream_putU32VL( stream, msg->len );
        stream_putU32( stream, msg->createdStamp );
        if ( 0 == msg->len ) {
            XWStreamCtxt* nliStream = mem_stream_make_raw( MPPARM(comms->mpool)
                                                           dutil_getVTManager(comms->dutil));
            NetLaunchInfo* nli = (NetLaunchInfo*)msg->msg;
            nli_saveToStream( nli, nliStream );
            XP_U16 nliLen = stream_getSize( nliStream );
            stream_putU32VL( stream, nliLen );
            stream_getFromStream( stream, nliStream, nliLen );
            stream_destroy( nliStream, xwe );
        } else {
            stream_putBytes( stream, msg->msg, msg->len );
        }
    }

    /* This writes 2 bytes instead of 1 if it were smarter. Not worth the work
     * to fix. */
    for ( CommsConnType typ = (CommsConnType)0; typ < VSIZE(comms->disableds); ++typ ) {
        if ( typ < COMMS_CONN_NFC || addr_hasType( &comms->selfAddr, typ ) ) {
            for ( int ii = 0; ii < VSIZE(comms->disableds[0]); ++ii ) {
                stream_putBits( stream, 1, comms->disableds[typ][ii] ? 1 : 0 );
            }
        }
    }

    comms->lastSaveToken = saveToken;
} /* comms_writeToStream */

static void
resetBackoff( CommsCtxt* comms )
{
    XP_LOGFF( "resetting backoff" );
    comms->resendBackoff = 0;
    comms->nextResend = 0;
}

void
comms_saveSucceeded( CommsCtxt* comms, XWEnv xwe, XP_U16 saveToken )
{
    XP_LOGFF( "(saveToken=%d)", saveToken );
    XP_ASSERT( !!comms );
    if ( saveToken == comms->lastSaveToken ) {
        AddressRecord* rec;
        for ( rec = comms->recs; !!rec; rec = rec->next ) {
            XP_LOGFF( "lastSave matches; updating lastMsgSaved (%d) to "
                     "lastMsgRcd (%d)", rec->lastMsgSaved, rec->lastMsgRcd );
            rec->lastMsgSaved = rec->lastMsgRcd;
        }
#ifdef XWFEATURE_COMMSACK
        comms_ackAny( comms, xwe );  /* might not want this for all transports */
#endif
    }
}

void
comms_getSelfAddr( const CommsCtxt* comms, CommsAddrRec* addr )
{
    XP_ASSERT( !!comms );
    XP_MEMCPY( addr, &comms->selfAddr, sizeof(*addr) );
} /* comms_getAddr */

XP_Bool
comms_getHostAddr( const CommsCtxt* comms, CommsAddrRec* addr )
{
    XP_ASSERT( !!comms );
    XP_Bool haveAddr = !comms->isServer
        && !!comms->recs
        && !comms->recs->next
        ;
    if ( haveAddr ) {
        XP_MEMCPY( addr, &comms->recs->addr, sizeof(*addr) );
    }
    return haveAddr;
} /* comms_getAddr */

static void
augmentSelfAddr( CommsCtxt* comms, XWEnv xwe, const CommsAddrRec* addr )
{
    logAddr( comms, xwe, addr, __func__ );
    XP_ASSERT( comms != NULL );

    XP_Bool addingRelay = addr_hasType( addr, COMMS_CONN_RELAY )
        && ! addr_hasType( &comms->selfAddr, COMMS_CONN_RELAY );

    CommsAddrRec tmp = comms->selfAddr;
    augmentAddrIntrnl( comms, &tmp, addr, XP_TRUE );
    util_addrChange( comms->util, xwe, &comms->selfAddr, &tmp );
    comms->selfAddr = tmp;

    logAddr( comms, xwe, &comms->selfAddr, "after" );

#ifdef COMMS_HEARTBEAT
    setDoHeartbeat( comms );
#endif
    if ( addingRelay ) {
        sendConnect( comms, xwe
#ifdef XWFEATURE_RELAY
                     , XP_TRUE
#endif
                     );
    }
} /* comms_setHostAddr */

void
comms_addMQTTDevID( CommsCtxt* comms, XP_PlayerAddr channelNo,
                    const MQTTDevID* devID )
{
#ifdef NO_ADD_MQTT_TO_ALL       /* set for (usually) BT testing on Android */
    XP_LOGFF("ifdef'd out");
    XP_USE( comms );
    XP_USE( channelNo );
    XP_USE( devID );
#else
    CNO_FMT( cbuf, channelNo );
    XP_LOGFF( "(channelNo: %s, devID: " MQTTDevID_FMT ")", cbuf, *devID );
    XP_Bool found = XP_FALSE;
    for ( AddressRecord* rec = comms->recs; !!rec && !found; rec = rec->next ) {
        found = (rec->channelNo & ~CHANNEL_MASK) == (channelNo & ~CHANNEL_MASK);
        if ( found ) {
            if ( addr_hasType( &rec->addr, COMMS_CONN_MQTT ) ) {
                XP_ASSERT( *devID == rec->addr.u.mqtt.devID );
            } else {
                CommsAddrRec tmp = {0};
                addr_setType( &tmp, COMMS_CONN_MQTT );
                tmp.u.mqtt.devID = *devID;
                ASSERT_ADDR_OK( &tmp );

                augmentAddrIntrnl( comms, &rec->addr, &tmp, XP_TRUE );
                ASSERT_ADDR_OK( &rec->addr );
                CNO_FMT( cbuf, channelNo );
                XP_LOGFF( "added for channel %s", cbuf );
            }
        }
    }
    if ( !found ) {
        XP_LOGFF( "unable to augment address!!" );
        XP_ASSERT(0);
    }
#endif
}

void
comms_getAddrs( const CommsCtxt* comms, XWEnv XP_UNUSED_DBG(xwe),
                CommsAddrRec addr[], XP_U16* nRecs )
{
    AddressRecord* recs;
    XP_U16 count;
    for ( count = 0, recs = comms->recs;
          count < *nRecs && !!recs;
          ++count, recs = recs->next ) {
        XP_MEMCPY( &addr[count], &recs->addr, sizeof(addr[count]) );
        logAddr( comms, xwe, &addr[count], __func__ );
    }
    *nRecs = count;
}

XP_U16
comms_countPendingPackets( const CommsCtxt* comms )
{
    // LOG_RETURNF( "%d", comms->queueLen );
    return comms->queueLen;
}

static XP_Bool
formatRelayID( const CommsCtxt* comms, XWHostID hostID,
               XP_UCHAR* buf, XP_U16* lenp )
{
    XP_U16 strln = 1 + XP_SNPRINTF( buf, *lenp, "%s/%d", 
                                    comms->rr.connName, hostID );
    XP_ASSERT( *lenp >= strln );
    *lenp = strln;
    return XP_TRUE;
}

#ifdef XWFEATURE_RELAY
static XP_Bool
haveRelayID( const CommsCtxt* comms )
{
    XP_Bool result = 0 != comms->rr.connName[0]
        && comms->rr.myHostID != HOST_ID_NONE;
    return result;
}

XP_Bool
comms_formatRelayID( const CommsCtxt* comms, XP_U16 indx,
                     XP_UCHAR* buf, XP_U16* lenp )
{
    XP_LOGFF( "(indx=%d)", indx );
    XWHostID hostID = HOST_ID_SERVER;
    if ( comms->isServer ) {
        hostID += 1 + indx;
    }
    XP_Bool success = formatRelayID( comms, hostID, buf, lenp );
    XP_LOGFF( "(%d) => %s", indx, buf );
    return success;
}

/* Get *my* "relayID", a combo of connname and host id */
XP_Bool
comms_getRelayID( const CommsCtxt* comms, XP_UCHAR* buf, XP_U16* lenp )
{
    XP_Bool result = haveRelayID( comms )
        && formatRelayID( comms, comms->rr.myHostID, buf, lenp );
    return result;
}
#endif

static void
formatMsgNo( const CommsCtxt* comms, const MsgQueueElem* elem,
             XP_UCHAR* buf, XP_U16 len )
{
    XP_SNPRINTF( buf, len, "%d:%d", comms->rr.myHostID, elem->msgID );
}

CommsConnTypes
comms_getConTypes( const CommsCtxt* comms )
{
    CommsConnType typ;
    if ( !!comms ) {
        typ = comms->selfAddr._conTypes;
    } else {
        typ = COMMS_CONN_NONE;
        XP_LOGFF( "returning COMMS_CONN_NONE for null comms" );
    }
    return typ;
} /* comms_getConTypes */

void
comms_dropHostAddr( CommsCtxt* comms, CommsConnType typ )
{
    addr_rmType( &comms->selfAddr, typ );
    ASSERT_ADDR_OK( &comms->selfAddr );
}

XP_Bool
comms_getIsServer( const CommsCtxt* comms )
{
    XP_ASSERT( !!comms );
    return comms->isServer;
}

static MsgQueueElem*
makeNewElem( const CommsCtxt* comms, XWEnv xwe, MsgID msgID,
             XP_PlayerAddr channelNo )
{
    MsgQueueElem* newElem = (MsgQueueElem*)XP_CALLOC( comms->mpool,
                                                      sizeof( *newElem ) );
    newElem->createdStamp = dutil_getCurSeconds( comms->dutil, xwe );
    newElem->channelNo = channelNo;
    newElem->msgID = msgID;
    return newElem;
}

static MsgQueueElem*
makeElemWithID( CommsCtxt* comms, XWEnv xwe, MsgID msgID, AddressRecord* rec,
                XP_PlayerAddr channelNo, XWStreamCtxt* stream )
{
    CNO_FMT( cbuf, channelNo );
    XP_LOGFF( TAGFMT(%s), TAGPRMS, cbuf );
    XP_U16 streamSize = NULL == stream? 0 : stream_getSize( stream );
    MsgID lastMsgSaved = (!!rec)? rec->lastMsgSaved : 0;
    MsgQueueElem* newElem = makeNewElem( comms, xwe, msgID, channelNo );

    XP_Bool useSmallHeader = !!rec && (COMMS_VERSION == rec->flags);
    XWStreamCtxt* hdrStream = mem_stream_make_raw( MPPARM(comms->mpool)
                                                   dutil_getVTManager(comms->dutil));
    XP_ASSERT( 0L == comms->connID || comms->connID == comms->util->gameInfo->gameID );
    if ( !useSmallHeader ) {
        XP_LOGFF( TAGFMT() "putting connID %x", TAGPRMS, comms->connID );
        stream_putU32( hdrStream, comms->connID );
    }

    stream_putU16( hdrStream, channelNo );

    if ( useSmallHeader ) {
        if ( 0 != msgID ) {     /* bit in flags says not there */
            stream_putU32VL( hdrStream, msgID );
        }
        stream_putU32VL( hdrStream, lastMsgSaved );
#if 0 && defined DEBUG
        /* Test receiver's ability to skip unexpected header fields */
        stream_putU8( hdrStream, 0x01 );
        stream_putU8( hdrStream, 0x02 );
        stream_putU8( hdrStream, 0x03 );
#endif
    } else {
        stream_putU32( hdrStream, msgID );
        stream_putU32( hdrStream, lastMsgSaved );
    }
    XP_LOGFF( TAGFMT() "put lastMsgSaved: %d", TAGPRMS, lastMsgSaved );
    if ( !!rec ) {
        rec->lastMsgAckd = lastMsgSaved;
    }

    /* Now we'll use a third stream to combine them all */
    XP_U16 headerLen = stream_getSize( hdrStream );
    XP_U16 flags = makeFlags( comms, headerLen, msgID );
    XWStreamCtxt* msgStream = mem_stream_make_raw( MPPARM(comms->mpool)
                                                   dutil_getVTManager(comms->dutil));
    if ( useSmallHeader ) {
        XP_ASSERT( HAS_VERSION_FLAG != flags );
    } else {
        stream_putU16( msgStream, HAS_VERSION_FLAG );
    }
    stream_putU16( msgStream, flags );

    stream_getFromStream( msgStream, hdrStream, stream_getSize(hdrStream) );
    stream_destroy( hdrStream, xwe );

    if ( 0 < streamSize ) {
        stream_getFromStream( msgStream, stream, streamSize );
    }

    newElem->len = stream_getSize( msgStream );
    XP_ASSERT( 0 < newElem->len );
    newElem->msg = (XP_U8*)XP_MALLOC( comms->mpool, newElem->len );
    stream_getBytes( msgStream, newElem->msg, newElem->len );
    stream_destroy( msgStream, xwe );

#ifdef COMMS_CHECKSUM
    newElem->checksum = dutil_md5sum( comms->dutil, xwe, newElem->msg,
                                      newElem->len );
#endif
    XP_ASSERT( 0 < newElem->len ); /* else NLI assumptions fail */
    return newElem;
} /* makeElemWithID */

#ifdef XWFEATURE_COMMS_INVITE
static MsgQueueElem*
makeInviteElem( CommsCtxt* comms, XWEnv xwe,
                XP_PlayerAddr channelNo, const NetLaunchInfo* nli )
{
    MsgQueueElem* newElem = makeNewElem( comms, xwe, 0, channelNo );

    XP_ASSERT( 0 == newElem->len );           /* len == 0 signals is NLI */
    newElem->msg = XP_MALLOC( comms->mpool, sizeof(*nli) );
    XP_MEMCPY( newElem->msg, nli, sizeof(*nli) );
# ifdef COMMS_CHECKSUM
    newElem->checksum = dutil_md5sum( comms->dutil, xwe, newElem->msg,
                                      sizeof(*nli) );
# endif
    return newElem;
}
#endif

XP_U16
comms_getChannelSeed( CommsCtxt* comms )
{
    XP_U16 result = !!comms ? comms->channelSeed : 0;
    while ( !!comms && 0 == (result & ~CHANNEL_MASK) ) {
        result = XP_RANDOM() & ~CHANNEL_MASK;
        result |= comms->forceChannel;
        CNO_FMT( cbuf, result );
        XP_LOGFF( "made seed: %s(%d)", cbuf, result );
        comms->channelSeed = result;
    }
    return result;
}

#ifdef XWFEATURE_COMMS_INVITE
/* We're adding invites to comms so they'll be persisted and resent etc. can
   work in common code. Rule will be there's only one invitation present per
   channel (remote device.) We'll add a channel if necessary. Then if there is
   an invitation already there we'll replace it.

   Interesting case is where I send two invitations only one of which can be
   accepted (as e.g. when I invite one friend, then on not receiving a
   response decide to invite a different friend instead. In the two-player
   game case I can detect that there are too many invitations and delete/reuse
   the previous channel when the new invitation is received. But what about
   the two-remotes game? Would I delete the oldest that hadn't been responded
   to? Or maybe keep them all, and when an invitation is responded to, meaning
   that there's incoming traffic on that channel, I promote it somehow, and
   when a game is complete (all players present) delete channels that have
   only outgoing invitations pending.

   Let's use channel 1 for invites. And be prepared to nuke their records once
   real traffic develops on that channel.
 */

/* Remove any AddressRecord created for invitations, and any MsgQueueElems for it */
static void
nukeInvites( CommsCtxt* comms, XWEnv xwe, XP_PlayerAddr channelNo )
{
    channelNo &= CHANNEL_MASK;
    XP_LOGFF( "(channelNo=0x%X)", channelNo );

    listRecs( comms, __func__ );

    AddressRecord* deadRec;

    AddressRecord* prevRec = NULL;
    for ( deadRec = comms->recs; !!deadRec; deadRec = deadRec->next ) {
        if ( channelNo == deadRec->channelNo ) {
            // XP_ASSERT( forceChannel == rec->channelNo ); /* should not have high bits */
            if ( NULL == prevRec ) {
                comms->recs = deadRec->next;
            } else {
                prevRec->next = deadRec->next;
            }
            break;
        }
        prevRec = deadRec;
    }

    if ( !!deadRec ) {
        removeFromQueue( comms, xwe, channelNo, 0 );
        CNO_FMT( cbuf, deadRec->channelNo );
        XP_LOGFF( "removing rec for %s", cbuf );
        XP_FREEP( comms->mpool, &deadRec );
    }

    listRecs( comms, "end of nukeInvites" );
} /* nukeInvites */

static XP_Bool
haveRealChannel( const CommsCtxt* comms, XP_PlayerAddr channelNo )
{
    XP_ASSERT( (channelNo & CHANNEL_MASK) == channelNo );
    XP_Bool found = XP_FALSE;

    for ( AddressRecord* rec = comms->recs; !!rec && !found; rec = rec->next ) {
        found = (channelNo == (CHANNEL_MASK & rec->channelNo))
            && (0 != (rec->channelNo & ~CHANNEL_MASK));
    }

    CNO_FMT( cbuf, channelNo );
    XP_LOGFF( "(%s) => %s", cbuf, boolToStr(found) );
    return found;
}

void
comms_invite( CommsCtxt* comms, XWEnv xwe, const NetLaunchInfo* nli,
              const CommsAddrRec* destAddr )
{
    LOG_FUNC();
    XP_PlayerAddr forceChannel = nli->forceChannel;
    if ( !haveRealChannel( comms, forceChannel ) ) {
        /* See if we have a channel for this address. Then see if we have an
           invite matching this one, and if not add one. Then trigger a send of
           it. */

        /* remove the old rec, if found */
        nukeInvites( comms, xwe, forceChannel );

        /*AddressRecord* rec = */rememberChannelAddress( comms, xwe, forceChannel,
                                                         0, destAddr, 0 );
        MsgQueueElem* elem = makeInviteElem( comms, xwe, forceChannel, nli );

        elem = addToQueue( comms, xwe, elem );
        sendMsg( comms, xwe, elem, COMMS_CONN_NONE );
    }
    LOG_RETURN_VOID();
}
#endif

/* Send a message using the sequentially next MsgID.  Save the message so
 * resend can work. */
XP_S16
comms_send( CommsCtxt* comms, XWEnv xwe, XWStreamCtxt* stream )
{
    XP_S16 result = -1;
    if ( 0 == stream_getSize(stream) ) {
        XP_LOGFF( "dropping 0-len message" );
    } else {
        XP_PlayerAddr channelNo = stream_getAddress( stream );
        CNO_FMT( cbuf, channelNo );
        XP_LOGFF( "%s", cbuf );
        AddressRecord* rec = getRecordFor( comms, xwe, NULL, channelNo );
        MsgID msgID = (!!rec)? ++rec->nextMsgID : 0;
        MsgQueueElem* elem;

        if ( 0 == channelNo ) {
            channelNo = comms_getChannelSeed(comms) & ~CHANNEL_MASK;
        }

        XP_LOGFF( TAGFMT() "assigning msgID=" XP_LD " on %s", TAGPRMS, msgID, cbuf );

        elem = makeElemWithID( comms, xwe, msgID, rec, channelNo, stream );
        if ( NULL != elem ) {
            elem = addToQueue( comms, xwe, elem );
            printQueue( comms );
            result = sendMsg( comms, xwe, elem, COMMS_CONN_NONE );
        }
    }
    return result;
} /* comms_send */

static void
notifyQueueChanged( const CommsCtxt* comms, XWEnv xwe )
{
    XP_U16 count = comms->queueLen;
    if ( !!comms->procs.countChanged ) {
        (*comms->procs.countChanged)( xwe, comms->procs.closure, count );
    }
}

/* Add new message to the end of the list.  The list needs to be kept in order
 * by ascending msgIDs within each channel since if there's a resend that's
 * the order in which they need to be sent.
 */
static MsgQueueElem*
addToQueue( CommsCtxt* comms, XWEnv xwe, MsgQueueElem* newElem )
{
    MsgQueueElem* asAdded = newElem;
    newElem->next = (MsgQueueElem*)NULL;
    if ( !comms->msgQueueHead ) {
        comms->msgQueueHead = comms->msgQueueTail = newElem;
        XP_ASSERT( comms->queueLen == 0 );
    } else {
        XP_ASSERT( !!comms->msgQueueTail );
        XP_ASSERT( !comms->msgQueueTail->next );
        if ( elems_same( comms->msgQueueTail, newElem ) ) {
            freeElem( MPPARM(comms->mpool) newElem );
            asAdded = comms->msgQueueTail;
        } else {
            comms->msgQueueTail->next = newElem;
            comms->msgQueueTail = newElem;
        }

        XP_ASSERT( comms->queueLen > 0 );
    }

    if ( newElem == asAdded ) {
        ++comms->queueLen;
        notifyQueueChanged( comms, xwe );
    }
    XP_ASSERT( comms->queueLen <= 128 ); /* reasonable limit in testing */
    return asAdded;
} /* addToQueue */

#ifdef DEBUG
static void
printQueue( const CommsCtxt* comms )
{
    MsgQueueElem* elem;
    short ii;

    for ( elem = comms->msgQueueHead, ii = 0; ii < comms->queueLen; 
          elem = elem->next, ++ii ) {
        CNO_FMT( cbuf, elem->channelNo );
        XP_LOGFF( "%d: %s; msgID=" XP_LD 
#ifdef COMMS_CHECKSUM
                    "; sum=%s"
#endif
                 , ii+1, cbuf, elem->msgID
#ifdef COMMS_CHECKSUM
                    , elem->checksum 
#endif
);
    }
}

static void
assertQueueOk( const CommsCtxt* comms )
{
    XP_U16 count = 0;
    MsgQueueElem* elem;

    for ( elem = comms->msgQueueHead; !!elem; elem = elem->next ) {
        ++count;
        if ( elem == comms->msgQueueTail ) {
            XP_ASSERT( !elem->next );
            break;
        }
    }
    XP_ASSERT( count == comms->queueLen );
    if ( count >= 10 ) {
        XP_LOGFF( "queueLen unexpectedly high: %d", count );
    }
}

static void
assertAddrOk( const CommsAddrRec* addr )
{
    CommsConnType typ;
    for ( XP_U32 st = 0; addr_iter( addr, &typ, &st ); ) {
        switch ( typ ) {
        case COMMS_CONN_MQTT:
            XP_ASSERT( 0 != addr->u.mqtt.devID );
            break;
        case COMMS_CONN_SMS:
            XP_ASSERT( 0 != addr->u.sms.phone[0] );
            break;
        case COMMS_CONN_P2P:
            XP_ASSERT( 0 != addr->u.p2p.mac_addr[0] );
            break;
        case COMMS_CONN_BT:
            /* XP_ASSERT( 0 != addr->u.bt.btAddr.chars[0] */
            /*            || 0 != addr->u.bt.hostName[0] ); */
            break;
        case COMMS_CONN_RELAY:
        case COMMS_CONN_NFC:
            break;
        default:
            XP_LOGFF( "no case for %s", ConnType2Str(typ) );
            XP_ASSERT(0);
            break;
        }
    }
}
#endif

static XP_Bool
elems_same( const MsgQueueElem* elem1, const MsgQueueElem* elem2 ) 
{
    XP_Bool same = elem1->msgID == elem2->msgID
        && elem1->channelNo == elem2->channelNo
        && elem1->len == elem2->len
        && 0 == XP_MEMCMP( elem1->msg, elem2->msg, elem1->len );
    return same;
}

static void
freeElem( MPFORMAL MsgQueueElem* elem )
{
    XP_FREE( mpool, elem->msg );
#ifdef COMMS_CHECKSUM
    XP_LOGFF( "freeing msg with sum %s", elem->checksum );
    XP_FREE( mpool, elem->checksum );
#endif
    XP_FREE( mpool, elem );
}

/* We've received on some channel a message with a certain ID.  This means
 * that all messages sent on that channel with lower IDs have been received
 * and can be removed from our queue.  BUT: if this ID is higher than any
 * we've sent, don't remove.  We may be starting a new game but have a server
 * that's still on the old one.
 */
static void
removeFromQueue( CommsCtxt* comms, XWEnv xwe, XP_PlayerAddr channelNo, MsgID msgID )
{
    CNO_FMT( cbuf, channelNo );
    XP_LOGFF( "(channelNo=%d): remove msgs <= " XP_LD " for %s (queueLen: %d)",
             channelNo, msgID, cbuf, comms->queueLen );

    if ((channelNo == 0) || !!getRecordFor( comms, xwe, NULL, channelNo)) {

        MsgQueueElem* elem = comms->msgQueueHead;
        MsgQueueElem* next;

        /* empty the queue so we can add all back again */
        comms->msgQueueHead = comms->msgQueueTail = NULL;
        comms->queueLen = 0;

        XP_PlayerAddr maskedChannelNo = ~CHANNEL_MASK & channelNo;
        for ( ; !!elem; elem = next ) {
            XP_Bool knownGood = XP_FALSE;
            next = elem->next;

            /* remove the 0-channel message if we've established a channel
               number.  Only clients should have any 0-channel messages in the
               queue, and receiving something from the server is an implicit
               ACK -- IFF it isn't left over from the last game. */

            XP_PlayerAddr maskedElemChannelNo = ~CHANNEL_MASK & elem->channelNo;
            if ( (maskedElemChannelNo == 0) && (channelNo != 0) ) {
                XP_ASSERT( !comms->isServer || IS_INVITE(elem) );
                XP_ASSERT( elem->msgID == 0 );
            } else if ( maskedElemChannelNo != maskedChannelNo ) {
                knownGood = XP_TRUE;
            }

            if ( !knownGood && (elem->msgID <= msgID) ) {
                freeElem( MPPARM(comms->mpool) elem );
            } else {
                MsgQueueElem* asAdded = addToQueue( comms, xwe, elem );
                XP_ASSERT( asAdded == elem );
                elem = asAdded; /* for non-assert case */
            }
        }
        notifyQueueChanged( comms, xwe );
    }

    XP_LOGFF( "queueLen now %d", comms->queueLen );

#ifdef DEBUG
    assertQueueOk( comms );
    printQueue( comms );
#endif
} /* removeFromQueue */

static XP_U32
gameID( const CommsCtxt* comms )
{
    XP_U32 gameID = comms->connID;
    if ( 0 == gameID ) {
        gameID = comms->util->gameInfo->gameID;
    }

    /* Most of the time these will be the same, but early in a game they won't
       be.  Would be nice not to have to use gameID. */
    if ( 0 == gameID ) {
        XP_LOGFF( "gameID STILL 0" );
    } else if ( 0 == comms->util->gameInfo->gameID ) {
        XP_LOGFF( "setting gi's gameID to 0X%X", gameID );
        comms->util->gameInfo->gameID = gameID;
    }

    return gameID;
}

static XP_S16
sendMsg( CommsCtxt* comms, XWEnv xwe, MsgQueueElem* elem, const CommsConnType filter )
{
    XP_S16 result = -1;
    XP_PlayerAddr channelNo = elem->channelNo;
    CNO_FMT( cbuf, channelNo );

    XP_Bool isInvite = IS_INVITE(elem);
#ifdef COMMS_CHECKSUM
    XP_LOGFF( TAGFMT() "sending message on %s: id: %d; len: %d; sum: %s; isInvite: %s",
              TAGPRMS, cbuf, elem->msgID, elem->len, elem->checksum, boolToStr(isInvite) );
#endif

    const CommsAddrRec* addrP = NULL;
    if ( comms->isServer ) {
        (void)channelToAddress( comms, xwe, channelNo, &addrP );
    } else {
        /* guest has only one peer, but old code might save several */
        if ( !!comms->recs ) {
            XP_ASSERT( !comms->recs->next ); // firing during upgrade test
            addrP = &comms->recs->addr;
        }
    }
    if ( NULL == addrP ) {
        XP_LOGFF( TAGFMT() "no addr for channel %x; dropping!'", TAGPRMS, channelNo );
        // XP_ASSERT(0);           /* firing */
    } else {
        CommsAddrRec addr = *addrP;
        if ( addr_hasType( &comms->selfAddr, COMMS_CONN_NFC ) ) {
            addr_addType( &addr, COMMS_CONN_NFC );
        }

        CommsConnType typ;
        for ( XP_U32 st = 0; addr_iter( &addr, &typ, &st ); ) {
            XP_S16 nSent = -1;
            if ( comms_getAddrDisabled( comms, typ, XP_TRUE ) ) {
                XP_LOGFF( "dropping message because %s disabled",
                          ConnType2Str( typ ) );
            } else if ( COMMS_CONN_NONE != filter && filter != typ ) {
                XP_LOGFF( "dropping message because not of type %s",
                          ConnType2Str( filter ) );
            } else {
#ifdef COMMS_CHECKSUM
                XP_LOGFF( TAGFMT() "sending msg with sum %s using typ %s", TAGPRMS,
                          elem->checksum, ConnType2Str(typ) );
#endif
                switch ( typ ) {
#ifdef XWFEATURE_RELAY
                case COMMS_CONN_RELAY: {
                    XWHostID destID = getDestID( comms, xwe, channelNo );
                    if ( HOST_ID_NONE == destID ) {
                        XP_LOGFF( TAGFMT() "skipping message via relay: no destID yet", TAGPRMS );
                    } else if ( haveRelayID( comms ) && sendNoConn( comms, xwe, elem, destID ) ) {
                        /* do nothing */
                        nSent = elem->len;
                    } else if ( comms->rr.relayState >= COMMS_RELAYSTATE_CONNECTED ) {
                        XP_UCHAR msgNo[16];
                        formatMsgNo( comms, elem, msgNo, sizeof(msgNo) );
                        if ( send_via_relay( comms, xwe, XWRELAY_MSG_TORELAY, destID,
                                             elem->msg, elem->len, msgNo ) ) {
                            nSent = elem->len;
                        }
                    } else {
                        XP_LOGFF( "skipping message: not connected to relay" );
                    }
                    break;
                }
#endif
#if defined XWFEATURE_IP_DIRECT
                case COMMS_CONN_BT:
                case COMMS_CONN_IP_DIRECT:
                    nSent = send_via_ip( comms, BTIPMSG_DATA, channelNo, 
                                         elem->msg, elem->len );
#ifdef COMMS_HEARTBEAT
                    setHeartbeatTimer( comms );
#endif
                    break;
#endif
                default:
                    XP_ASSERT( addr_hasType( &addr, typ ) );

                    /* A more general check that the address type has the settings
                       it needs would be better here.... */
                    if ( typ == COMMS_CONN_MQTT && 0 == addr.u.mqtt.devID ) {
                        XP_LOGFF( "not sending: MQTT address NULL" );
                        XP_ASSERT(0);
                        break;
                    }

                    if ( 0 ) {
#ifdef XWFEATURE_COMMS_INVITE
                    } else if ( isInvite ) {
                        if ( !!comms->procs.sendInvt ) {
                            NetLaunchInfo* nli = (NetLaunchInfo*)elem->msg;
                            nSent = (*comms->procs.sendInvt)( xwe, nli, elem->createdStamp,
                                                              &addr, comms->procs.closure );
                        }
#endif
                    } else {
                        XP_ASSERT( !!comms->procs.sendMsg );
                        XP_U32 gameid = gameID( comms );
                        logAddr( comms, xwe, &addr, __func__ );
                        XP_UCHAR msgNo[16];
                        formatMsgNo( comms, elem, msgNo, sizeof(msgNo) );
                        nSent = (*comms->procs.sendMsg)( xwe, elem->msg, elem->len, msgNo,
                                                         elem->createdStamp, &addr,
                                                         typ, gameid,
                                                         comms->procs.closure );
                    }
                    break;

                } /* switch */
            }
            if ( nSent > result ) {
                result = nSent;
            }
        } /* for */
    
        if ( result == elem->len ) {
#ifdef DEBUG
            ++elem->sendCount;
#endif
            XP_LOGFF( "elem's sendCount since load: %d",
                      elem->sendCount );
        }
        CNO_FMT( cbuf1, elem->channelNo );
        XP_LOGFF( "(%s; msgID=" XP_LD ", len=%d)=>%d", cbuf1, elem->msgID,
                  elem->len, result );
    }
    XP_ASSERT( result < 0 || elem->len == result );
    return result;
} /* sendMsg */

#ifdef XWFEATURE_RELAY
static void
send_relay_ack( CommsCtxt* comms, XWEnv xwe )
{
    LOG_FUNC();
    (void)send_via_relay( comms, xwe, XWRELAY_ACK, comms->rr.myHostID,
                          NULL, 0, NULL );
}
#endif

typedef XP_S16 (*MsgProc)( CommsCtxt* comms, XWEnv xwe, MsgQueueElem* msg,
                           CommsConnType filter, void* closure );

static XP_S16
resendImpl( CommsCtxt* comms, XWEnv xwe, CommsConnType filter, XP_Bool force,
            MsgProc proc, void* closure )
{
    XP_S16 count = 0;
    XP_Bool success = XP_TRUE;
    XP_ASSERT( !!comms );

    XP_U32 now = dutil_getCurSeconds( comms->dutil, xwe );
    if ( !force && (now < comms->nextResend) ) {
        XP_LOGFF( "aborting: %d seconds left in backoff",
                 comms->nextResend - now );
        success = XP_FALSE;

    } else if ( !!comms->msgQueueHead ) {
        for ( MsgQueueElem* msg = comms->msgQueueHead; !!msg; msg = msg->next ) {
            XP_S16 len = (*proc)( comms, xwe, msg, filter, closure );
            if ( 0 > len ) {
                success = XP_FALSE;
                /* might want to remove break! Otherwise one bad channel (old
                   invite?) spoils all */
                break;
            } else {
                XP_ASSERT( 0 < len );
                ++count;
            }
        }

        /* Now set resend values */
        if ( success && !force ) {
            comms->resendBackoff = 2 * (1 + comms->resendBackoff);
            XP_LOGFF( "backoff now %d", comms->resendBackoff );
            comms->nextResend = now + comms->resendBackoff;
        }
    }
    XP_LOGFF( TAGFMT() "=> %d", TAGPRMS, count );
    return count;
} /* resendImpl */

static XP_S16
sendMsgWrapper( CommsCtxt* comms, XWEnv xwe, MsgQueueElem* msg, CommsConnType filter,
                void* XP_UNUSED(closure) )
{
    return sendMsg( comms, xwe, msg, filter );
}

XP_S16
comms_resendAll( CommsCtxt* comms, XWEnv xwe, CommsConnType filter, XP_Bool force )
{
    XP_S16 result = resendImpl( comms, xwe, filter, force, sendMsgWrapper, NULL );
    // LOG_RETURNF( "%d", result );
    return result;
}

typedef struct _GetAllClosure{
    PendingMsgProc proc;
    void* closure;
} GetAllClosure;

static XP_S16
gatherMsgs( CommsCtxt* XP_UNUSED(comms), XWEnv xwe, MsgQueueElem* msg,
            CommsConnType XP_UNUSED(filter), void* closure )
{
    GetAllClosure* gac = (GetAllClosure*)closure;
    (*gac->proc)( gac->closure, xwe, msg->msg, msg->len, msg->msgID );
    return 1;                   /* 0 gets an assert */
}

void
comms_getPending( CommsCtxt* comms, XWEnv xwe, PendingMsgProc proc, void* closure )
{
    GetAllClosure gac = { .proc = proc, .closure = closure };
    (void)resendImpl( comms, xwe, COMMS_CONN_NONE, XP_TRUE, gatherMsgs, &gac );
}

#ifdef XWFEATURE_COMMSACK
static void
ackAnyImpl( CommsCtxt* comms, XWEnv xwe, XP_Bool force )
{
    if ( CONN_ID_NONE == comms->connID ) {
        XP_LOGFF( "doing nothing because connID still unset" );
    } else {
#ifdef DEBUG
        int nSent = 0;
        int nSeen = 0;
#endif
        AddressRecord* rec;
        for ( rec = comms->recs; !!rec; rec = rec->next ) {
#ifdef DEBUG
            ++nSeen;
#endif
            if ( force || rec->lastMsgAckd < rec->lastMsgRcd ) {
#ifdef DEBUG
                ++nSent;
                CNO_FMT( cbuf, rec->channelNo );
                XP_LOGFF( "%s; %d < %d (or force: %s): rec getting ack",
                          cbuf, rec->lastMsgAckd, rec->lastMsgRcd,
                          boolToStr(force) );
#endif
                sendEmptyMsg( comms, xwe, rec );
            }
        }
        XP_LOGFF( "sent for %d channels (of %d)", nSent, nSent );
    } 
}

void
comms_ackAny( CommsCtxt* comms, XWEnv xwe )
{
    ackAnyImpl( comms, xwe, XP_FALSE );
}
#else
# define ackAnyImpl( comms, xwe, force )
#endif

# define CASESTR(s) case s: return #s

#ifdef XWFEATURE_RELAY
# ifdef DEBUG
static const char*
relayCmdToStr( XWRELAY_Cmd cmd )
{
    switch( cmd ) {
        CASESTR( XWRELAY_NONE );
        CASESTR( XWRELAY_GAME_CONNECT );
        CASESTR( XWRELAY_GAME_RECONNECT );
        CASESTR( XWRELAY_GAME_DISCONNECT );
        CASESTR( XWRELAY_CONNECT_RESP );
        CASESTR( XWRELAY_RECONNECT_RESP );
        CASESTR( XWRELAY_ALLHERE );
        CASESTR( XWRELAY_DISCONNECT_YOU );
        CASESTR( XWRELAY_DISCONNECT_OTHER );
        CASESTR( XWRELAY_CONNECTDENIED );
#ifdef RELAY_HEARTBEAT
        CASESTR( XWRELAY_HEARTBEAT );
#endif
        CASESTR( XWRELAY_MSG_FROMRELAY );
        CASESTR( XWRELAY_MSG_FROMRELAY_NOCONN );
        CASESTR( XWRELAY_MSG_TORELAY );
        CASESTR( XWRELAY_MSG_TORELAY_NOCONN );
        CASESTR( XWRELAY_MSG_STATUS );
        CASESTR( XWRELAY_ACK );
    default: 
        XP_LOGFF( "unknown cmd: %d", cmd );
        XP_ASSERT( 0 );
        return "<unknown>";
    }
}
# endif 

static void
got_connect_cmd( CommsCtxt* comms, XWEnv xwe, XWStreamCtxt* stream,
                 XP_Bool reconnected )
{
    LOG_FUNC();
    XP_U16 nHere, nSought;
    XP_Bool isServer;

    set_relay_state( comms, xwe, reconnected ? COMMS_RELAYSTATE_RECONNECTED
                     : COMMS_RELAYSTATE_CONNECTED );
    XWHostID myHostID = stream_getU8( stream );
    XP_LOGFF( "myHostID: %d", myHostID );
    if ( comms->rr.myHostID != myHostID ) {
        XP_LOGFF( "changing rr.myHostID from %x to %x",
                 comms->rr.myHostID, myHostID );
        comms->rr.myHostID = myHostID;
    }

    isServer = HOST_ID_SERVER == comms->rr.myHostID;

    if ( isServer != comms->isServer ) {
        XP_LOGFF( "becoming%s a server", isServer ? "" : " NOT" );
        comms->isServer = isServer;
#ifdef DEBUG
        XP_U16 queueLen = comms->queueLen;
#endif
        (*comms->rcProc)( xwe, comms->rcClosure, !isServer );
        XP_ASSERT( queueLen == comms->queueLen ); /* callback should not send!!! */
        reset_internal( comms, xwe, isServer, comms->rr.nPlayersHere,
                        comms->rr.nPlayersTotal, XP_FALSE );
    }

    comms->rr.cookieID = stream_getU16( stream );
    XP_ASSERT( COOKIE_ID_NONE != comms->rr.cookieID );
    comms->rr.heartbeat = stream_getU16( stream );
    nSought = (XP_U16)stream_getU8( stream );
    nHere = (XP_U16)stream_getU8( stream );
    if ( nSought == nHere ) {
        set_relay_state( comms, xwe, COMMS_RELAYSTATE_ALLCONNECTED );
    }

#ifdef DEBUG
    {
        XP_UCHAR connName[MAX_CONNNAME_LEN+1];
        stringFromStreamHere( stream, connName, sizeof(connName) );
        if ( comms->rr.connName[0] != '\0' 
             && 0 != XP_STRCMP( comms->rr.connName, connName ) ) {
            XP_LOGFF( "we're replacing connNames: %s overwritten by %s",
                      comms->rr.connName, connName );
        }
        XP_MEMCPY( comms->rr.connName, connName, sizeof(comms->rr.connName) );
        XP_LOGFF( "connName: \"%s\" (reconnect=%d)", connName, 
                 reconnected );
    }
#else
    stringFromStreamHere( stream, comms->rr.connName, 
                          sizeof(comms->rr.connName) );
#endif

#ifdef XWFEATURE_DEVID
    DevIDType typ = stream_getU8( stream );
    XP_UCHAR devID[MAX_DEVID_LEN + 1] = {0};
    if ( ID_TYPE_NONE != typ ) {
        stringFromStreamHere( stream, devID, sizeof(devID) );
    }
    if ( ID_TYPE_NONE == typ    /* error case */
         || '\0' != devID[0] ) /* new info case */ {
        dutil_deviceRegistered( comms->dutil, xwe, typ, devID );
    }
#endif

    /* Don't bother notifying if the game's already in play on some other
       transport */
    if ( CONN_ID_NONE == comms->connID ) {
        if ( !!comms->procs.rconnd ) {
            (*comms->procs.rconnd)( xwe, comms->procs.closure,
                                    comms->selfAddr.u.ip_relay.invite, reconnected,
                                    comms->rr.myHostID, XP_FALSE, nSought - nHere );
            XP_LOGFF( "have %d of %d players", nHere, nSought );
        }
    }
    setHeartbeatTimer( comms );
} /* got_connect_cmd */

static XP_Bool
relayPreProcess( CommsCtxt* comms, XWEnv xwe, XWStreamCtxt* stream, XWHostID* senderID )
{
    XP_Bool consumed = XP_TRUE;
    XWHostID destID, srcID;
    CookieID cookieID = comms->rr.cookieID;
    XWREASON relayErr;

    /* nothing for us to do here if not using relay */
    XWRELAY_Cmd cmd = stream_getU8( stream );
    XP_LOGFF( "(%s)", relayCmdToStr( cmd ) );
    switch( cmd ) {

    case XWRELAY_CONNECT_RESP:
        got_connect_cmd( comms, xwe, stream, XP_FALSE );
        send_relay_ack( comms, xwe );
        break;
    case XWRELAY_RECONNECT_RESP:
        got_connect_cmd( comms, xwe, stream, XP_TRUE );
        comms_resendAll( comms, xwe, COMMS_CONN_NONE, XP_FALSE );
        break;

    case XWRELAY_ALLHERE:
        srcID = (XWHostID)stream_getU8( stream );
        if ( comms->rr.myHostID != HOST_ID_NONE
             && comms->rr.myHostID != srcID ) {
            XP_LOGFF( "changing hostid from %d to %d",
                     comms->rr.myHostID, srcID );
        }

        if ( COOKIE_ID_NONE == comms->rr.cookieID ) {
            XP_LOGFF( "cookieID still 0; background send?" );
        }

        if ( srcID != comms->rr.myHostID ) {
            XP_LOGFF( "set hostID: %x (was %x)", srcID, comms->rr.myHostID );
        }
        comms->rr.myHostID = srcID;

#ifdef DEBUG
        {
            XP_UCHAR connName[MAX_CONNNAME_LEN+1];
            stringFromStreamHere( stream, connName, sizeof(connName) );
            if ( comms->rr.connName[0] != '\0' 
                 && 0 != XP_STRCMP( comms->rr.connName, connName ) ) {
                XP_LOGFF( "we're replacing connNames: %s overwritten by %s",
                          comms->rr.connName, connName );
            }
            XP_MEMCPY( comms->rr.connName, connName, 
                       sizeof(comms->rr.connName) );
            XP_LOGFF( "connName: \"%s\"", connName );
        }
#else
        stringFromStreamHere( stream, comms->rr.connName, 
                              sizeof(comms->rr.connName) );
#endif

        /* We're [re-]connected now.  Send any pending messages.  This may
           need to be done later since we're inside the platform's socket
           read proc now.  But don't resend if we were previously
           REconnected, as we'll have sent then.  -- I don't see any send
           on RECONNECTED, so removing the test for now to fix recon
           problems on android. */
        /* if ( COMMS_RELAYSTATE_RECONNECTED != comms->rr.relayState ) { */
        comms_resendAll( comms, xwe, COMMS_CONN_NONE, XP_FALSE );
        /* } */
        if ( XWRELAY_ALLHERE == cmd ) { /* initial connect? */
            (*comms->procs.rconnd)( xwe, comms->procs.closure,
                                    comms->selfAddr.u.ip_relay.invite, XP_FALSE,
                                    comms->rr.myHostID, XP_TRUE, 0 );
        }
        set_relay_state( comms, xwe, COMMS_RELAYSTATE_ALLCONNECTED );
        break;
    case XWRELAY_MSG_FROMRELAY:
        cookieID = stream_getU16( stream );
    case XWRELAY_MSG_FROMRELAY_NOCONN:
        srcID = stream_getU8( stream );
        destID = stream_getU8( stream );
        XP_LOGFF( "cookieID: %d; srcID: %x; destID: %x",
                  cookieID, srcID, destID );
        /* If these values don't check out, drop it */

        /* When a message comes in via proxy (rather than a connection) state
           may not be as expected.  Just commenting these out is probably the
           wrong fix.  Maybe instead the constructor takes a flag that means
           "assume you're connected"  Revisit this. */
        /* XP_ASSERT( COMMS_RELAYSTATE_ALLCONNECTED == comms->rr.relayState */
        /*            || COMMS_RELAYSTATE_CONNECTED == comms->rr.relayState */
        /*            || COMMS_RELAYSTATE_RECONNECTED == comms->rr.relayState ); */

        if ( destID == comms->rr.myHostID ) { /* When would this not happen? */
            consumed = XP_FALSE;
        } else if ( cookieID == comms->rr.cookieID ) {
            XP_LOGFF( "keeping message though hostID not what "
                     "expected (%d vs %d)", destID, comms->rr.myHostID );
            consumed = XP_FALSE;
        }

        if ( consumed ) {
            XP_LOGFF( "rejecting data message (consumed)" );
        } else {
            *senderID = srcID;
        }
        break;

    case XWRELAY_DISCONNECT_OTHER:
        relayErr = stream_getU8( stream );
        srcID = stream_getU8( stream );
        XP_LOGFF( "host id %x disconnected", srcID );
        /* if we don't have connName then RECONNECTED is the wrong state to
           change to. */
        if ( COMMS_RELAYSTATE_RECONNECTED < comms->rr.relayState ) {
            XP_ASSERT( 0 != comms->rr.connName[0] );
            // XP_ASSERT( COOKIE_ID_NONE != comms->rr.cookieID ); /* firing!! */
            if ( COOKIE_ID_NONE == comms->rr.cookieID ) { /* firing!! */
                XP_LOGFF( "cookieID still COOKIE_ID_NONE; dropping!" );
            } else {
                set_relay_state( comms, xwe, COMMS_RELAYSTATE_RECONNECTED );
            /* we will eventually want to tell the user which player's gone */
                util_userError( comms->util, xwe, ERR_RELAY_BASE + relayErr );
            }
        }
        break;

    case XWRELAY_DISCONNECT_YOU:                /* Close socket for this? */
        relayErr = stream_getU8( stream );
        set_relay_state( comms, xwe, COMMS_RELAYSTATE_UNCONNECTED );
        util_userError( comms->util, xwe, ERR_RELAY_BASE + relayErr );
        break;

    case XWRELAY_MSG_STATUS:
        relayErr = stream_getU8( stream );
        (*comms->procs.rerror)( xwe, comms->procs.closure, relayErr );
        break;

    case XWRELAY_CONNECTDENIED: /* socket will get closed by relay */
        relayErr = stream_getU8( stream );
        XP_LOGFF( "got reason: %s", XWREASON2Str( relayErr ) );
        set_relay_state( comms, xwe, COMMS_RELAYSTATE_DENIED );

        if ( XWRELAY_ERROR_NORECONN == relayErr ) {
            init_relay( comms, xwe, comms->rr.nPlayersHere, comms->rr.nPlayersTotal );
        } else {
            util_userError( comms->util, xwe, ERR_RELAY_BASE + relayErr );
            /* requires action, not just notification */
            (*comms->procs.rerror)( xwe, comms->procs.closure, relayErr );
        }
        break;

        /* fallthru */
    default:
        XP_ASSERT( 0 );         /* while debugging multi-addr, this needs a fix! */
        XP_LOGFF( "dropping relay msg with cmd %d", (XP_U16)cmd );
    }
    
    LOG_RETURNF( "consumed=%s", boolToStr(consumed) );
    return consumed;
} /* relayPreProcess */
#endif

#ifdef COMMS_HEARTBEAT
static void
noteHBReceived( CommsCtxt* comms/* , const CommsAddrRec* addr */ )
{
    comms->lastMsgRcvdTime = dutil_getCurSeconds( comms->dutil, xwe );
    setHeartbeatTimer( comms );
}
#else
# define noteHBReceived(a)
#endif

#if defined XWFEATURE_IP_DIRECT
static XP_Bool
btIpPreProcess( CommsCtxt* comms, XWStreamCtxt* stream )
{
    BTIPMsgType typ = (BTIPMsgType)stream_getU8( stream );
    XP_Bool consumed = typ != BTIPMSG_DATA;

    if ( consumed ) {
        /* This  is all there is so far */
        if ( typ == BTIPMSG_RESET ) {
            (void)comms_resendAll( comms, XP_FALSE );
        } else if ( typ == BTIPMSG_HB ) {
/*             noteHBReceived( comms, addr ); */
        } else {
            XP_ASSERT( 0 );
        }
    }

    return consumed;
} /* btIpPreProcess */
#endif

static XP_Bool
preProcess(
#ifdef XWFEATURE_RELAY
           CommsCtxt* comms, XWEnv xwe, XWStreamCtxt* stream,
           XP_Bool* usingRelay, XWHostID* senderID,
#endif
           const CommsAddrRec* useAddr
           )
{
    XP_Bool consumed = XP_FALSE;

    /* There should be exactly one type associated with an incoming message */
    CommsConnType typ = addr_getType( useAddr );
    XP_LOGFF( "(typ=%s)", ConnType2Str(typ) );

    switch ( typ ) {
#ifdef XWFEATURE_RELAY
        /* relayPreProcess returns true if consumes the message.  May just eat the
           header and leave a regular message to be processed below. */
    case COMMS_CONN_RELAY:
        consumed = relayPreProcess( comms, xwe, stream, senderID );
        if ( !consumed ) {
            *usingRelay = XP_TRUE;
        }
        break;
#endif
#if defined XWFEATURE_IP_DIRECT
    case COMMS_CONN_BT:
    case COMMS_CONN_IP_DIRECT:
        consumed = btIpPreProcess( comms, stream );
        break;
#endif
#if defined XWFEATURE_SMS
    case COMMS_CONN_SMS:
        break;    /* nothing to grab */
#endif
#ifdef XWFEATURE_BLUETOOTH
    case COMMS_CONN_BT:
        break;    /* nothing to grab */
#endif
    case COMMS_CONN_P2P:
        break;    /* nothing to grab?? */
    case COMMS_CONN_NFC:
    case COMMS_CONN_MQTT:
        break;    /* nothing to grab?? */
    default:
        XP_ASSERT(0);
        break;
    }
    LOG_RETURNF( "%s", boolToStr(consumed) );
    return consumed;
} /* preProcess */

static AddressRecord* 
getRecordFor( CommsCtxt* comms, XWEnv xwe, const CommsAddrRec* addr,
              const XP_PlayerAddr channelNo )
{
    AddressRecord* rec;
    XP_Bool matched = XP_FALSE;

    /* Use addr if we have it.  Otherwise use channelNo if non-0 */
    CNO_FMT( cbuf, channelNo );
    for ( rec = comms->recs; !!rec; rec = rec->next ) {
        /* server should have only one rec max -- but relay has bugs right now
           that send to the wrong device randomly. */
        // XP_ASSERT( comms->isServer || !rec->next );

        CNO_FMT( cbuf1, rec->channelNo );
        XP_LOGFF( "comparing rec channel %s with addr channel %s",
                 cbuf1, cbuf );

        if ( (rec->channelNo & ~CHANNEL_MASK) == (channelNo & ~CHANNEL_MASK) ) {
            XP_LOGFF( "match based on channels!!!" );
            matched = XP_TRUE;
        } else {
            continue;
            /* if ( (rec->channelNo & ~CHANNEL_MASK) == (channelNo & ~CHANNEL_MASK) ) { */
            /*     XP_ASSERT(0);   /\* figure out why this would make sense *\/ */
            /*     XP_LOGF( "%s: figure out why this would make sense ", __func__ ); */
            /* } */
            CommsConnType conType = !!addr ?
                addr_getType( addr ) : COMMS_CONN_NONE;
            switch( conType ) {
#ifdef XWFEATURE_RELAY
            case COMMS_CONN_RELAY:
                if ( (addr->u.ip_relay.ipAddr == rec->addr.u.ip_relay.ipAddr)
                     && (addr->u.ip_relay.port == rec->addr.u.ip_relay.port ) ) {
                    matched = XP_TRUE;
                }
                break;
#endif
            case COMMS_CONN_BT:
                if ( 0 == XP_MEMCMP( &addr->u.bt.btAddr, &rec->addr.u.bt.btAddr,
                                     sizeof(addr->u.bt.btAddr) ) ) {
                    matched = XP_TRUE;
                }
                break;
            case COMMS_CONN_IP_DIRECT:
                if ( (addr->u.ip.ipAddr_ip == rec->addr.u.ip.ipAddr_ip)
                     && (addr->u.ip.port_ip == rec->addr.u.ip.port_ip) ) {
                    matched = XP_TRUE;
                }
                break;
            case COMMS_CONN_IR:              /* no way to test */
            case COMMS_CONN_P2P:              /* no way to test??? */
                break;
            case COMMS_CONN_SMS:
#ifdef XWFEATURE_SMS
                {
                    XW_DUtilCtxt* duc = util_getDevUtilCtxt( comms->util, xwe );
                    matched = dutil_phoneNumbersSame( duc, xwe, addr->u.sms.phone,
                                                      rec->addr.u.sms.phone )
                        && addr->u.sms.port == rec->addr.u.sms.port;
                }
#endif
                break;
            case COMMS_CONN_MQTT:
                matched = addr->u.mqtt.devID == rec->addr.u.mqtt.devID;
                break;
            case COMMS_CONN_NONE:
                matched = channelNo == (rec->channelNo & ~CHANNEL_MASK);
                break;
            default:
                XP_ASSERT(0);
                break;
            }
        }
        if ( matched ) {
            break;
        }
    }

    XP_LOGFF( "(%s) => %p", cbuf, rec );
    return rec;
} /* getRecordFor */

static XP_Bool
checkChannelNo( CommsCtxt* comms, XP_PlayerAddr* channelNoP )
{
    XP_Bool success = XP_TRUE;
    XP_PlayerAddr channelNo = *channelNoP;
    if ( 0 == (channelNo & CHANNEL_MASK) ) {
        success = comms->nextChannelNo < CHANNEL_MASK;
        if ( success ) {
            channelNo |= ++comms->nextChannelNo;
            CNO_FMT( cbuf, channelNo );
            XP_LOGFF( "assigned channelNo: %s", cbuf );
        }
        // XP_ASSERT( comms->nextChannelNo <= CHANNEL_MASK );
    } else {
        /* Let's make sure we don't assign it later */
        comms->nextChannelNo = channelNo;
    }
    *channelNoP = channelNo;
    LOG_RETURNF( "%s", boolToStr(success) );
    return success;
}

/* An initial message comes only from a client to a server, and from the
 * server in response to that initial message.  Once the inital messages are
 * exchanged there's a connID associated.  The greatest danger is that it's a
 * dup, resent for whatever reason.  To detect that we check that the address
 * is unknown.  But addresses can change, e.g. if a reset of a socket-based
 * transport causes the local socket to change.  How to deal with this?
 * Likely a boolean set when we call comms->resetproc that causes us to accept
 * changed addresses.
 *
 * But: before we're connected heartbeats will also come here, but with
 * hasPayload false.  We want to remember their address, but not give them a
 * channel ID.  So if we have a payload we insist that it's the first we've
 * seen on this channel.
 *
 * If it's a HB, then we want to add a rec/channel if there's none, but mark
 * it invalid
 */
static AddressRecord*
validateInitialMessage( CommsCtxt* comms, XWEnv xwe,
                        XP_Bool XP_UNUSED_HEARTBEAT(hasPayload),
                        const CommsAddrRec* addr, XWHostID senderID, 
                        XP_PlayerAddr* channelNo, XP_U16 flags, MsgID msgID )
{
    CNO_FMT( cbuf, *channelNo );
    XP_LOGFF( TAGFMT(%s), TAGPRMS, cbuf );

    AddressRecord* rec = NULL;
    if ( 0 ) {
#ifdef COMMS_HEARTBEAT
    } else if ( comms->doHeartbeat ) {
        XP_Bool addRec = XP_FALSE;
        /* This (with mask) is untested!!! */
        rec = getRecordFor( comms, xwe, addr, *channelNo );

        if ( hasPayload ) {
            if ( rec ) {
                if ( rec->initialSeen ) {
                    rec = NULL;     /* reject it! */
                }
            } else {
                addRec = XP_TRUE;
            }
        } else {
            /* This is a heartbeat */
            if ( !rec && comms->isServer ) {
                addRec = XP_TRUE;
            }
        }

        if ( addRec ) {
            if ( comms->isServer ) {
                CNO_FMT( cbuf, *channelNo );
                XP_LOGFF( TAGFMT() "looking at %s", TAGPRMS, cbuf );
                XP_ASSERT( (*channelNo & CHANNEL_MASK) == 0 );
                *channelNo |= ++comms->nextChannelNo;
                CNO_FMT( cbuf1, *channelNo );
                XP_LOGFF( TAGFMT() "ORd channel onto channelNo: now %s", TAGPRMS,
                          cbuf1 );
                XP_ASSERT( comms->nextChannelNo <= CHANNEL_ID_MASK );
            }
            rec = rememberChannelAddress( comms, xwe, *channelNo, senderID, addr,
                                          flags );
            if ( hasPayload ) {
                rec->initialSeen = XP_TRUE;
            } else {
                rec = NULL;
            }
        }
#endif
    } else {
        CNO_FMT( cbuf, *channelNo );
        XP_LOGFF( TAGFMT() "looking at %s", TAGPRMS, cbuf );
        rec = getRecordFor( comms, xwe, addr, *channelNo );
        if ( !!rec ) {
            augmentChannelAddr( comms, rec, addr, senderID );

            /* Used to be that the initial message was where the channel
               record got created, but now the client creates an address for
               the host on startup (comms_make()) */
            if ( comms->isServer || 1 != msgID ) {
                XP_LOGFF( TAGFMT() "rejecting duplicate INIT message", TAGPRMS );
                rec = NULL;
            } else {
                XP_LOGFF( "accepting duplicate (?) msg" );
            }
        } else {
            if ( comms->isServer ) {
                if ( checkChannelNo( comms, channelNo ) ) {
                    CNO_FMT( cbuf, *channelNo );
                    XP_LOGFF( TAGFMT() "augmented channel: %s", TAGPRMS, cbuf );
                } else {
                    /* Why do I sometimes see these in the middle of a game
                       with lots of messages already sent?  connID of 0 should
                       only happen at the start! */
                    XP_LOGFF( TAGFMT() "dropping msg because channel already set",
                             TAGPRMS );
                    goto errExit;
                }
            }
            rec = rememberChannelAddress( comms, xwe, *channelNo, senderID, /* here */
                                          addr, flags );
        }
    }
 errExit:
    LOG_RETURNF( XP_P, rec );
    return rec;
} /* validateInitialMessage */

static XP_U16
makeFlags( const CommsCtxt* comms, XP_U16 headerLen, MsgID msgID )
{
    XP_U16 flags = COMMS_VERSION;
    if ( comms->isServer ) {
        flags |= IS_SERVER_BIT;
    }
    if ( CONN_ID_NONE == comms->connID ) {
        flags |= NO_CONNID_BIT;
    }
    if ( 0 == msgID ) {
        flags |= NO_MSGID_BIT;
    }

    XP_ASSERT( headerLen == ((headerLen << HEADER_LEN_OFFSET) >> HEADER_LEN_OFFSET) );
    flags |= headerLen << HEADER_LEN_OFFSET;

    return flags;
}

/* Messages with established connIDs are valid only if they have the msgID
 * that's expected on that channel.  Their addresses need to match what we
 * have for that channel, and in fact we'll overwrite what we have in case a
 * reset has changed the address.  The danger is that somebody might sneak in
 * with a forged message, but this isn't internet banking.
 */
static AddressRecord* 
validateChannelMessage( CommsCtxt* comms, XWEnv xwe, const CommsAddrRec* addr,
                        XP_PlayerAddr channelNo, XWHostID senderID,
                        MsgID msgID, MsgID lastMsgRcd )

{
    AddressRecord* rec;
    LOG_FUNC();

    rec = getRecordFor( comms, xwe, NULL, channelNo );
    if ( !!rec ) {
        removeFromQueue( comms, xwe, channelNo, lastMsgRcd );

        augmentChannelAddr( comms, rec, addr, senderID );

        if ( msgID == rec->lastMsgRcd + 1 ) {
            XP_LOGFF( TAGFMT() "expected %d AND got %d", TAGPRMS,
                     msgID, msgID );
        } else if ( msgID != rec->lastMsgRcd + 1 ) {
            XP_LOGFF( TAGFMT() "expected %d, got %d", TAGPRMS,
                     rec->lastMsgRcd + 1, msgID );
            /* Let's not do this yet. I need to understand why there are
               messages with msgID==0 and how to get rid of them. */
            // ackAnyImpl( comms, xwe, XP_TRUE );
            rec = NULL;
        }
    } else {
        CNO_FMT( cbuf, channelNo );
        XP_LOGFF( TAGFMT() "no rec for %s", TAGPRMS, cbuf );
    }

    LOG_RETURNF( XP_P, rec );
    return rec;
} /* validateChannelMessage */

typedef struct _HeaderStuff {
    XP_U16 flags;
    XP_U32 connID;
    XP_PlayerAddr channelNo;
    MsgID msgID;
    MsgID lastMsgRcd;
} HeaderStuff;

static XP_Bool
getCheckChannelSeed( CommsCtxt* comms, XWStreamCtxt* stream, HeaderStuff* stuff )
{
    XP_Bool messageValid = stream_gotU16( stream, &stuff->channelNo );
    if ( messageValid ) {
        XP_U16 channelSeed = comms_getChannelSeed( comms );
        XP_U16 flags = stuff->flags;

        /* First test isn't valid if we haven't passed the bit explicitly */
        if ( 0 != flags && (comms->isServer == (0 != (flags & IS_SERVER_BIT))) ) {
            XP_LOGFF( TAGFMT() "server bits mismatch; isServer: %d; flags: %x",
                      TAGPRMS, comms->isServer, flags );
            messageValid = XP_FALSE;
        } else if ( comms->isServer ) {
            /* channelNo comparison invalid */
        } else if ( 0 == stuff->channelNo || 0 == channelSeed ) {
            XP_LOGFF( TAGFMT() "one of channelNos still 0", TAGPRMS );
            XP_ASSERT(0);
        } else if ( (stuff->channelNo & ~CHANNEL_MASK)
                    != (channelSeed & ~CHANNEL_MASK) ) {
            XP_LOGFF( "channelNos test fails: %x vs %x", stuff->channelNo,
                      channelSeed );
            messageValid = XP_FALSE;
        }
    }
    LOG_RETURNF( "%s", boolToStr(messageValid) );
    return messageValid;
}

static XP_Bool
parseBeefHeader( CommsCtxt* comms, XWStreamCtxt* stream, HeaderStuff* stuff )
{
    XP_Bool messageValid =
        stream_gotU16( stream, &stuff->flags ) /* flags are the next short */
        && stream_gotU32( stream, &stuff->connID );
    XP_LOGFF( TAGFMT() "read connID (gameID) of %x", TAGPRMS, stuff->connID );

    messageValid = messageValid
        && getCheckChannelSeed( comms, stream, stuff )
        && stream_gotU32( stream, &stuff->msgID )
        && stream_gotU32( stream, &stuff->lastMsgRcd );

    LOG_RETURNF( "%s", boolToStr(messageValid) );
    return messageValid;
}

static XP_Bool
parseSmallHeader( CommsCtxt* comms, XWEnv xwe, XWStreamCtxt* msgStream,
                  HeaderStuff* stuff )
{
    XP_Bool messageValid = XP_FALSE;
    XP_U16 headerLen = stuff->flags >> HEADER_LEN_OFFSET;
    XP_ASSERT( 0 < headerLen );
    XP_ASSERT( headerLen <= stream_getSize( msgStream ) );
    if ( headerLen <= stream_getSize( msgStream ) ) {
        XWStreamCtxt* hdrStream =
            mem_stream_make_raw( MPPARM(comms->mpool)
                                 dutil_getVTManager(comms->dutil));
        stream_getFromStream( hdrStream, msgStream, headerLen );
        stuff->connID = 0 == (stuff->flags & NO_CONNID_BIT)
            ? comms->util->gameInfo->gameID : CONN_ID_NONE;

        if ( getCheckChannelSeed( comms, hdrStream, stuff ) ) {
            XP_ASSERT( stuff->msgID == 0 );
            if ( 0 == (stuff->flags & NO_MSGID_BIT) ) {
                stuff->msgID = stream_getU32VL( hdrStream );
            }
            stuff->lastMsgRcd = stream_getU32VL( hdrStream );
            messageValid = XP_TRUE;
        }
        stream_destroy( hdrStream, xwe );
    }

    LOG_RETURNF( "%s", boolToStr(messageValid) );
    return messageValid;
}

XP_Bool
comms_checkIncomingStream( CommsCtxt* comms, XWEnv xwe, XWStreamCtxt* stream,
                           const CommsAddrRec* retAddr, CommsMsgState* state )
{
    XP_ASSERT( !!retAddr );     /* for now */
    XP_MEMSET( state, 0, sizeof(*state) );
#ifdef DEBUG
    state->comms = comms;
    if ( comms->processingMsg ) {
        XP_LOGFF( "processingMsg SET, so dropping message" );
        return XP_FALSE;
    }
    XP_ASSERT( !comms->processingMsg );
    comms->processingMsg = XP_TRUE;
    CommsConnType addrType = addr_getType( retAddr );
#endif

    XP_Bool messageValid = XP_FALSE;
    XP_LOGFF( TAGFMT(retAddr.typ=%s), TAGPRMS, ConnType2Str(addrType ) );
    if ( comms_getAddrDisabled( comms, addrType, XP_FALSE ) ) {
        XP_LOGFF( "dropping message because %s disabled",
                 ConnType2Str( addrType ) );
    /* } else if (0 == (comms->selfAddr._conTypes & retAddr->_conTypes)) { */
    /*     /\* we don't expect messages with that address type; drop it *\/ */
    /*     XP_LOGF( "%s: not expecting %s messages", __func__,  */
    /*              ConnType2Str( addrType ) ); */
    } else {
#ifdef DEBUG
        if (0 == (comms->selfAddr._conTypes & retAddr->_conTypes)) {
            XP_LOGFF( "not expecting %s messages (but proceeding)",
                      ConnType2Str( addrType ) );
        }
#endif
        XWHostID senderID = 0;      /* unset; default for non-relay cases */
#ifdef XWFEATURE_RELAY
        XP_Bool usingRelay = XP_FALSE;
#endif

#ifdef COMMS_CHECKSUM
        XP_U16 initialLen = stream_getSize( stream );
#endif

        const CommsAddrRec* useAddr = !!retAddr ? retAddr : &comms->selfAddr;
        if ( !preProcess(
#ifdef XWFEATURE_RELAY
                         comms, xwe , stream, &usingRelay, &senderID,
#endif
                         useAddr ) ) {
#ifdef COMMS_CHECKSUM
            state->len = stream_getSize( stream );
            // stream_getPtr pts at base, but sum excludes relay header
            const XP_U8* ptr = initialLen - state->len + stream_getPtr( stream );
            XP_UCHAR* tmpsum = dutil_md5sum( comms->dutil, xwe, ptr, state->len );
            XP_STRCAT( state->sum, tmpsum );
            XP_LOGFF( TAGFMT() "got message of len %d with sum %s",
                      TAGPRMS, state->len, state->sum );
            XP_FREE( comms->mpool, tmpsum );
#endif
            HeaderStuff stuff = {0};
            messageValid = stream_gotU16( stream, &stuff.flags );

            if ( messageValid ) {
                /* If BEEF is next sender is using old format. Otherwise
                   assume the bits are flags and BEEF is skipped by newer
                   code. Should work with anything newer then six years
                   ago. */
                if ( HAS_VERSION_FLAG == stuff.flags ) {
                    messageValid = parseBeefHeader( comms, stream, &stuff );
                } else if ( COMMS_VERSION == (stuff.flags & VERSION_MASK) ) {
                    messageValid = parseSmallHeader( comms, xwe, stream, &stuff );
                }
            }

            if ( messageValid ) {
                state->msgID = stuff.msgID;
                CNO_FMT( cbuf, stuff.channelNo );
                XP_LOGFF( TAGFMT() "rcd on %s: msgID=%d, lastMsgRcd=%d ",
                          TAGPRMS, cbuf, stuff.msgID, stuff.lastMsgRcd );
            } else {
                XP_LOGFF( TAGFMT() "got message to self?", TAGPRMS ); /* firing */
            }

            AddressRecord* rec = NULL;
            XP_U16 streamSize = stream_getSize( stream );  /* anything left? */
            if ( messageValid ) {
                if ( stuff.connID == CONN_ID_NONE ) {
                    /* special case: initial message from client or server */
                    rec = validateInitialMessage( comms, xwe, streamSize > 0, retAddr,
                                                  senderID, &stuff.channelNo,
                                                  stuff.flags, stuff.msgID );
                    state->rec = rec;
                } else if ( comms->connID == stuff.connID ) {
                    rec = validateChannelMessage( comms, xwe, retAddr,
                                                  stuff.channelNo, senderID,
                                                  stuff.msgID, stuff.lastMsgRcd );
                } else {
                    XP_LOGFF( TAGFMT() "unexpected connID (%x vs %x) ; "
                              "dropping message", TAGPRMS, comms->connID,
                              stuff.connID );
                }
            }

            messageValid = messageValid && (NULL != rec)
                && (0 == rec->lastMsgRcd || rec->lastMsgRcd <= stuff.msgID);
            if ( messageValid ) {
                CNO_FMT( cbuf, stuff.channelNo );
                XP_LOGFF( TAGFMT() "got %s; msgID=%d; len=%d", TAGPRMS, cbuf,
                          stuff.msgID, streamSize );
                state->channelNo = stuff.channelNo;
                comms->lastSaveToken = 0; /* lastMsgRcd no longer valid */
                stream_setAddress( stream, stuff.channelNo );
                messageValid = streamSize > 0;
                resetBackoff( comms );
            }
        }

        /* Call after we've had a chance to create rec for addr */
        noteHBReceived( comms/* , addr */ );

    }
#ifdef COMMS_CHECKSUM
    LOG_RETURNF( "%s (len: %d; sum: %s)", boolToStr(messageValid), state->len, state->sum );
#else
    LOG_RETURNF( "%s (len: %d)", boolToStr(messageValid), state->len );
#endif
    return messageValid;
} /* comms_checkIncomingStream */

void
comms_msgProcessed( CommsCtxt* comms, XWEnv xwe,
                    CommsMsgState* state, XP_Bool rejected )
{
#ifdef COMMS_CHECKSUM
    XP_LOGFF( "rec: %p; len: %d; sum: %s; id: %d; rejected: %s", state->rec,
              state->len, state->sum, state->msgID, boolToStr(rejected) );
#endif
    XP_ASSERT( comms == state->comms );
    XP_ASSERT( comms->processingMsg );

    if ( rejected ) {
        if ( !!state->rec ) {
            XP_LOGFF( "should I remove rec???; msgID: %d", state->msgID );
            XP_ASSERT( 1 >= state->msgID );
            /* this is likely a mistake!!! Why remove it??? */
            // removeAddrRec( comms, xwe, state->rec );
        }
#ifdef LOG_COMMS_MSGNOS
        XP_LOGFF( "msg rejected; NOT upping lastMsgRcd to %d", state->msgID );
#endif
    } else {
        AddressRecord* rec = getRecordFor( comms, xwe, NULL, state->channelNo );
        XP_ASSERT( !!rec );
        if ( !!rec && rec->lastMsgRcd < state->msgID ) {
#ifdef LOG_COMMS_MSGNOS
            XP_LOGFF( "upping lastMsgRcd from %d to %d", rec->lastMsgRcd, state->msgID );
#endif
            rec->lastMsgRcd = state->msgID;
        }
        // XP_LOGFF( "CALLING nukeInvites(); might be wrong" );
        nukeInvites( comms, xwe, state->channelNo );
    }

#ifdef DEBUG
    comms->processingMsg = XP_FALSE;
#endif
}

XP_Bool
comms_checkComplete( const CommsAddrRec* addr )
{
    XP_Bool result;

    switch ( addr_getType( addr ) ) {
#ifdef XWFEATURE_RELAY
    case COMMS_CONN_RELAY:
        result = !!addr->u.ip_relay.invite[0]
            && !!addr->u.ip_relay.hostName[0]
            && !!addr->u.ip_relay.port > 0;
        break;
#endif
    default:
        result = XP_TRUE;
    }

    return result;
}

XP_Bool
comms_canChat( const CommsCtxt* const comms )
{
    XP_Bool canChat = comms_isConnected( comms )
        && comms->connID != CONN_ID_NONE
        && 64 > comms->queueLen;
    return canChat;
}

XP_Bool
comms_isConnected( const CommsCtxt* const comms )
{
    XP_Bool result = XP_FALSE;
    CommsConnType typ;
    for ( XP_U32 st = 0; !result && addr_iter( &comms->selfAddr, &typ, &st ); ) {
        XP_Bool expected = XP_FALSE;
        switch ( typ ) {
        case COMMS_CONN_RELAY:
            result = 0 != comms->rr.connName[0];
            expected = XP_TRUE;
            break;
        case COMMS_CONN_SMS:
        case COMMS_CONN_BT:
        case COMMS_CONN_P2P:
        case COMMS_CONN_MQTT:
            expected = XP_TRUE;
        default:
            result = comms->connID != CONN_ID_NONE;
            break;
        }
        if ( ! expected ) {
            XP_LOGFF( "unexpected type %s", ConnType2Str(typ) );
        }
    }
    return result;
}

#ifdef XWFEATURE_KNOWNPLAYERS
void
comms_gatherPlayers( CommsCtxt* comms, XWEnv xwe, XP_U32 created )
{
    LOG_FUNC();
    if ( 0 == (comms->flags & FLAG_HARVEST_DONE) ) {
        CommsAddrRec addrs[4] = {{0}};
        XP_U16 nRecs = VSIZE(addrs);
        comms_getAddrs( comms, NULL, addrs, &nRecs );

        const CurGameInfo* gi = comms->util->gameInfo;
        XP_ASSERT( 0 < gi->nPlayers );
        if ( kplr_addAddrs( comms->dutil, xwe, gi, addrs, nRecs, created ) ) {
            if ( 1 ) {
                XP_LOGFF( "not setting flag :-)" );
            } else {
                /* Need a way to force/override this manually? */
                comms->flags |= FLAG_HARVEST_DONE;
            }
        }
    }
}
#endif

#ifdef RELAY_VIA_HTTP
void
comms_gameJoined( CommsCtxt* comms, XWEnv xwe, const XP_UCHAR* connname, XWHostID hid )
{
    LOG_FUNC();
    XP_ASSERT( XP_STRLEN( connname ) + 1 < sizeof(comms->rr.connName) );
    XP_STRNCPY( comms->rr.connName, connname, sizeof(comms->rr.connName) );
    comms->rr.myHostID = hid;
    comms->forceChannel = hid;
    set_relay_state( comms, xwe, COMMS_RELAYSTATE_USING_HTTP );
}
#endif

#if defined COMMS_HEARTBEAT || defined XWFEATURE_COMMSACK
static void
sendEmptyMsg( CommsCtxt* comms, XWEnv xwe, AddressRecord* rec )
{
    MsgQueueElem* elem = makeElemWithID( comms, xwe, 0 /* msgID */, 
                                         rec, rec? rec->channelNo : 0, NULL );
    (void)sendMsg( comms, xwe, elem, COMMS_CONN_NONE );
    freeElem( MPPARM(comms->mpool) elem );
} /* sendEmptyMsg */
#endif

#ifdef COMMS_HEARTBEAT
/* Heartbeat.
 *
 * Goal is to allow all participants to detect when another is gone quickly.
 * Assumption is that transport is cheap: sending extra packets doesn't cost
 * much money or bother (meaning: don't do this over IR! :-).  
 *
 * Keep track of last time we heard from each channel and of when we last sent
 * a packet.  Run a timer, and when it fires: 1) check if we haven't heard
 * since 2x the timer interval.  If so, call alert function and reset the
 * underlying (ip, bt) channel.  If not, check how long since we last sent a
 * packet on each channel.  If it's been longer than since the last timer, and
 * if there are not already packets in the queue on that channel, fire a HB
 * packet.
 *
 * A HB packet is one whose msg ID is lower than the most recent ACK'd so that
 * it's sure to be dropped on the other end and not to interfere with packets
 * that might be resent.
 */
static void
heartbeat_checks( CommsCtxt* comms )
{
    LOG_FUNC();

    do {
        if ( comms->lastMsgRcvdTime > 0 ) {
            XP_U32 now = dutil_getCurSeconds( comms->dutil, xwe );
            XP_U32 tooLongAgo = now - (HB_INTERVAL * 2);
            if ( comms->lastMsgRcvdTime < tooLongAgo ) {
                XP_LOGFF( "calling reset proc; last was %ld secs too long "
                         "ago", tooLongAgo - comms->lastMsgRcvdTime );
                (*comms->procs.reset)(comms->procs.closure);
                comms->lastMsgRcvdTime = 0;
                break;          /* outta here */
            }
        }

        if ( comms->recs ) {
            AddressRecord* rec;
            for ( rec = comms->recs; !!rec; rec = rec->next ) {
                sendEmptyMsg( comms, rec );
            }
        } else if ( !comms->isServer ) {
            /* Client still waiting for inital ALL_REG message */
            sendEmptyMsg( comms, NULL );
        }
    } while ( XP_FALSE );

    setHeartbeatTimer( comms );
} /* heartbeat_checks */
#endif

#if defined RELAY_HEARTBEAT || defined COMMS_HEARTBEAT
static XP_Bool
p_comms_timerFired( void* closure, XWEnv xwe, XWTimerReason XP_UNUSED_DBG(why) )
{
    CommsCtxt* comms = (CommsCtxt*)closure;
    XP_ASSERT( why == TIMER_COMMS );
    LOG_FUNC();
    comms->hbTimerPending = XP_FALSE;
    if (0 ) {
#if defined XWFEATURE_RELAY && defined RELAY_HEARTBEAT
    } else  if ( (comms->selfAddr.conType == COMMS_CONN_RELAY )
         && (comms->rr.heartbeat != HEARTBEAT_NONE) ) {
        (void)send_via_relay( comms, xwe, XWRELAY_HEARTBEAT, HOST_ID_NONE,
                              NULL, 0, NULL );
        /* No need to reset timer.  send_via_relay does that. */
#endif
#ifdef COMMS_HEARTBEAT
    } else {
        XP_ASSERT( comms->doHeartbeat );
        heartbeat_checks( comms );
#endif
    }
    return XP_FALSE;            /* no need for redraw */
} /* p_comms_timerFired */

static void
setHeartbeatTimer( CommsCtxt* comms )
{
    XP_ASSERT( !!comms );

    if ( comms->hbTimerPending ) {
        XP_LOGFF( "skipping b/c hbTimerPending" );
    } else if ( comms->reconTimerPending ) {
        XP_LOGFF( "skipping b/c reconTimerPending" );
    } else {
        XP_U16 when = 0;
#ifdef XWFEATURE_RELAY
        if ( comms->selfAddr.conType == COMMS_CONN_RELAY ) {
            when = comms->rr.heartbeat;
        }
#endif
#ifdef COMMS_HEARTBEAT
        if ( comms->doHeartbeat ) {
            XP_LOGFF( "calling util_setTimer" );
            when = HB_INTERVAL;
        }
#endif
        if ( when != 0 ) {
            util_setTimer( comms->util, xwe, TIMER_COMMS, when,
                           p_comms_timerFired, comms );
            comms->hbTimerPending = XP_TRUE;
        }
    }
} /* setHeartbeatTimer */
#endif

const char*
ConnType2Str( CommsConnType typ )
{
    switch( typ ) {
        CASESTR(COMMS_CONN_NONE);
        CASESTR( COMMS_CONN_IR );
        CASESTR( COMMS_CONN_IP_DIRECT );
        CASESTR( COMMS_CONN_RELAY );
        CASESTR( COMMS_CONN_BT );
        CASESTR( COMMS_CONN_SMS );
        CASESTR( COMMS_CONN_P2P );
        CASESTR( COMMS_CONN_NTYPES );
        CASESTR( COMMS_CONN_NFC );
        CASESTR( COMMS_CONN_MQTT );
    default:
        XP_ASSERT(0);
    }
    return "<unknown>";
} /* ConnType2Str */

#ifdef DEBUG
void
comms_getStats( CommsCtxt* comms, XWStreamCtxt* stream )
{
    XP_UCHAR buf[100];

    int nChannels = 0;
    for ( AddressRecord* rec = comms->recs; !!rec; rec = rec->next ) {
        ++nChannels;
    }

    XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf), 
                 (XP_UCHAR*)"role: %s; msg queue len: %d; have %d channels\n",
                 comms->isServer ? "host" : "guest",
                 comms->queueLen, nChannels );
    stream_catString( stream, buf );

    XP_U16 indx = 0;
    for ( MsgQueueElem* elem = comms->msgQueueHead; !!elem; elem = elem->next ) {
        XP_SNPRINTF( buf, sizeof(buf), 
                     "%d: - channelNo=%.4X; msgID=" XP_LD "; len=%d\n",
                     indx++, elem->channelNo, elem->msgID, elem->len );
        stream_catString( stream, buf );
    }

    for ( AddressRecord* rec = comms->recs; !!rec; rec = rec->next ) {
        XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf),
                     (XP_UCHAR*)"Stats for channel %.4X\n",
                     rec->channelNo );
        stream_catString( stream, buf );

        XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf),
                     (XP_UCHAR*)"  Last msg sent: " XP_LD "; ",
                     rec->nextMsgID );
        stream_catString( stream, buf );

        XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf),
                     (XP_UCHAR*)"last msg received: %d\n",
                     rec->lastMsgRcd );
        stream_catString( stream, buf );
    }
} /* comms_getStats */

void
comms_setAddrDisabled( CommsCtxt* comms, CommsConnType typ, 
                       XP_Bool send, XP_Bool disabled )
{
    XP_ASSERT( !!comms );
    XP_LOGFF( "(typ=%s, send=%d, disabled=%d)",
        ConnType2Str(typ), send, disabled );
    comms->disableds[typ][send?0:1] = disabled;
}

XP_Bool
comms_getAddrDisabled( const CommsCtxt* comms, CommsConnType typ, 
                       XP_Bool send )
{
    XP_ASSERT( !!comms );
    return comms->disableds[typ][send?0:1];
}
#endif

static AddressRecord*
rememberChannelAddress( CommsCtxt* comms, XWEnv xwe, XP_PlayerAddr channelNo,
                        XWHostID hostID, const CommsAddrRec* addr, XP_U16 flags )
{
    CNO_FMT( cbuf, channelNo );
    XP_LOGFF( "(%s)", cbuf );
    listRecs( comms, "entering rememberChannelAddress" );

    logAddr( comms, xwe, addr, __func__ );
    AddressRecord* rec = NULL;
    rec = getRecordFor( comms, xwe, NULL, channelNo );
    if ( !rec ) {
        /* not found; add a new entry */
        rec = (AddressRecord*)XP_CALLOC( comms->mpool, sizeof(*rec) );

        rec->channelNo = channelNo;
        rec->rr.hostID = hostID;
        rec->flags = flags & VERSION_MASK;

        rec->next = comms->recs;
        comms->recs = rec;
        XP_LOGFF( "creating rec %p for %s, hostID = %d, flags=0x%x",
                  rec, cbuf, hostID, flags );
    }

    /* overwrite existing address with new one.  I assume that's the right
       move. */
    if ( !!rec ) {
        if ( !!addr ) {
            XP_LOGFF( "replacing/adding addr with _conTypes %x with %x",
                     rec->addr._conTypes, addr->_conTypes );
            XP_MEMCPY( &rec->addr, addr, sizeof(rec->addr) );
            XP_ASSERT( rec->rr.hostID == hostID );
        } else {
            XP_LOGFF( "storing addr with _conTypes %x",
                     addr->_conTypes );
            XP_MEMSET( &rec->addr, 0, sizeof(rec->addr) );
            rec->addr._conTypes = comms->selfAddr._conTypes;
            // addr_setTypes( &recs->addr, addr_getTypes( &comms->selfAddr ) );
        }
    }
    listRecs( comms, "leaving rememberChannelAddress" );
    return rec;
} /* rememberChannelAddress */

#ifdef DEBUG
static void 
logAddr( const CommsCtxt* comms, XWEnv xwe,
         const CommsAddrRec* addr, const char* caller )
{
    if ( !!addr ) {
        char buf[128];
        XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(comms->mpool)
                                                    dutil_getVTManager(comms->dutil));
        snprintf( buf, sizeof(buf), TAGFMT() "called on %p from %s:\n", TAGPRMS,
                  addr, caller );
        stream_catString( stream, buf );

        CommsConnType typ;
        XP_Bool first = XP_TRUE;
        for ( XP_U32 st = 0; addr_iter( addr, &typ, &st ); ) {
            if ( !first ) {
                stream_catString( stream, "\n" );
            }

            snprintf( buf, sizeof(buf), "* %s: ", ConnType2Str(typ) );
            stream_catString( stream, buf );

            switch( typ ) {
#ifdef XWFEATURE_RELAY
            case COMMS_CONN_RELAY:
                stream_catString( stream, "room: " );
                stream_catString( stream, addr->u.ip_relay.invite );
                stream_catString( stream, "; host: " );
                stream_catString( stream, addr->u.ip_relay.hostName );
                break;
#endif
            case COMMS_CONN_SMS:
                stream_catString( stream, "phone: " );
                stream_catString( stream, addr->u.sms.phone );
                stream_catString( stream, "; port: " );
                snprintf( buf, sizeof(buf), "%d", addr->u.sms.port );
                stream_catString( stream, buf );
                break;
            case COMMS_CONN_BT:
                stream_catString( stream, "host: " );
                stream_catString( stream, addr->u.bt.hostName );
                stream_catString( stream, "; addr: " );
                stream_catString( stream, addr->u.bt.btAddr.chars );
                break;
            case COMMS_CONN_P2P:
                stream_catString( stream, "mac addr: " );
                stream_catString( stream, addr->u.p2p.mac_addr );
                break;
            case COMMS_CONN_NFC:
                break;
            case COMMS_CONN_MQTT: {
                stream_catString( stream, "mqtt devID: " );
                XP_UCHAR buf[32];
                XP_SNPRINTF( buf, VSIZE(buf), MQTTDevID_FMT, addr->u.mqtt.devID );
                stream_catString( stream, buf );
            }
                break;
            default:
                XP_ASSERT(0);
            }
            first = XP_FALSE;
        }
        stream_putU8( stream, '\0' );
        XP_LOGFF( "%s", stream_getPtr( stream ) );
        stream_destroy( stream, xwe );
    }
}

/* static void  */
/* logAddrs( const CommsCtxt* comms, XWEnv xwe, const char* caller ) */
/* { */
/*     const AddressRecord* rec = comms->recs; */
/*     while ( !!rec ) { */
/*         CNO_FMT( cbuf, rec->channelNo ); */
/*         XP_LOGFF( TAGFMT() "%s", TAGPRMS, cbuf ); */
/*         logAddr( comms, xwe, &rec->addr, caller ); */
/*         rec = rec->next; */
/*     } */
/* } */
#endif

static void
augmentChannelAddr( CommsCtxt* comms, AddressRecord* const rec,
                    const CommsAddrRec* addr, XWHostID hostID )
{
    augmentAddrIntrnl( comms, &rec->addr, addr, XP_TRUE );
    if ( addr_hasType( &rec->addr, COMMS_CONN_RELAY ) ) {
        if ( 0 != hostID ) {
            rec->rr.hostID = hostID;
            XP_LOGFF( "set hostID for rec %p to %d", rec, hostID );
        }
    }

#ifdef DEBUG
    CommsConnType typ;
    for ( XP_U32 st = 0; addr_iter( addr, &typ, &st ); ) {
        if ( !addr_hasType( &comms->selfAddr, typ ) ) {
            XP_LOGFF( "main addr missing type %s", ConnType2Str(typ) );
        }
    }
#endif
}

static XP_Bool
augmentAddrIntrnl( CommsCtxt* comms, CommsAddrRec* destAddr,
                   const CommsAddrRec* srcAddr, XP_Bool isNewer )
{
    ASSERT_ADDR_OK( srcAddr );
    XP_Bool changed = XP_FALSE;
    const CommsAddrRec empty = {0};
    if ( !!srcAddr ) {
        CommsConnType typ;
        for ( XP_U32 st = 0; addr_iter( srcAddr, &typ, &st ); ) {
            XP_Bool newType = !addr_hasType( destAddr, typ );
            if ( newType ) {
                XP_LOGFF( "adding new type %s to rec", ConnType2Str(typ) );
                addr_addType( destAddr, typ );

                /* If an address is getting added to a channel, the top-level
                   address should also include the type. The specifics of the
                   address don't make sense to copy, however.
                   NO -- not any more. I have the addresses my user gives me
                */
                if ( !!comms && ! addr_hasType( &comms->selfAddr, typ ) ) {
                    /* we just added it, so can't be comms->selfAddr */
                    XP_ASSERT( destAddr != &comms->selfAddr );
                    XP_LOGFF( "NOT adding %s to comms->selfAddr", ConnType2Str(typ) );
                    // addr_addType( &comms->selfAddr, typ );
                }
            }

            const void* src = NULL;
            void* dest = NULL;
            size_t siz;

            switch( typ ) {
#ifdef XWFEATURE_RELAY
            case COMMS_CONN_RELAY:
                dest = &destAddr->u.ip_relay;
                src = &srcAddr->u.ip_relay;
                siz = sizeof( destAddr->u.ip_relay );
                break;
#endif
            case COMMS_CONN_SMS:
                XP_ASSERT( 0 != srcAddr->u.sms.port );
                XP_ASSERT( '\0' != srcAddr->u.sms.phone[0] );
                dest = &destAddr->u.sms;
                src = &srcAddr->u.sms;
                siz = sizeof(destAddr->u.sms);
                break;
            case COMMS_CONN_P2P:
                XP_ASSERT( '\0' != srcAddr->u.p2p.mac_addr[0] );
                dest = &destAddr->u.p2p;
                src = &srcAddr->u.p2p;
                siz = sizeof(destAddr->u.p2p);
                break;
            case COMMS_CONN_BT:
#ifdef XWFEATURE_BLUETOOTH
                dest = &destAddr->u.bt;
                src = &srcAddr->u.bt;
                siz = sizeof(destAddr->u.bt);
#endif
                break;
            case COMMS_CONN_NFC:
                break;
            case COMMS_CONN_MQTT:
                XP_ASSERT( 0 != srcAddr->u.mqtt.devID );
                dest = &destAddr->u.mqtt;
                src = &srcAddr->u.mqtt;
                siz = sizeof(destAddr->u.mqtt);
                break;
            default:
                XP_ASSERT(0);
                break;
            }
            if ( !!dest ) {
                XP_ASSERT( !newType || 0 == XP_MEMCMP( &empty, dest, siz ) );

                XP_Bool different = 0 != XP_MEMCMP( dest, src, siz );
                if ( different ) {
                    /* If the dest is non-empty AND the src is older, don't do
                       anything: don't replace newer info with older. Note
                       that this assumes unset values are empty!!! */
                    if ( !isNewer && !newType
                         && 0 != XP_MEMCMP( &empty, dest, siz ) ) {
                        XP_LOGFF( "%s: not replacing new info with old",
                                  ConnType2Str(typ) );
                    } else {
                        XP_MEMCPY( dest, src, siz );
                        changed = XP_TRUE;
                    }
                }
            }
        }
    }
    return changed;
}

XP_Bool
augmentAddr( CommsAddrRec* addr, const CommsAddrRec* newer, XP_Bool isNewer )
{
    return augmentAddrIntrnl( NULL, addr, newer, isNewer );
}

static XP_Bool
channelToAddress( CommsCtxt* comms, XWEnv xwe, XP_PlayerAddr channelNo,
                  const CommsAddrRec** addr )
{
    AddressRecord* recs = getRecordFor( comms, xwe, NULL, channelNo );
    XP_Bool found = !!recs;
    *addr = found? &recs->addr : NULL;
    return found;
} /* channelToAddress */

static XP_U16
countAddrRecs( const CommsCtxt* comms )
{
    XP_U16 count = 0;
    for ( AddressRecord* recs = comms->recs; !!recs; recs = recs->next ) {
        ++count;
    } 
    return count;
} /* countAddrRecs */

XP_Bool
addr_iter( const CommsAddrRec* addr, CommsConnType* typp, XP_U32* state )
{
    XP_Bool result = types_iter( addr->_conTypes, typp, state );
    return result;
}

XP_Bool
types_iter( XP_U32 conTypes, CommsConnType* typp, XP_U32* state )
{
    CommsConnType typ = *state;
    XP_ASSERT( typ < COMMS_CONN_NTYPES );
    while ( ++typ < COMMS_CONN_NTYPES ) {
        *state = typ;
        XP_U16 mask = 1 << (typ - 1);
        if ( mask == (conTypes & mask) ) {
            break;
        }
    }
    XP_Bool found = typ < COMMS_CONN_NTYPES;
    if ( found ) {
        *typp = typ;
    }
    // XP_LOGF( "%s(flag=%x)=>%d (typ=%s)", __func__, conTypes, found, ConnType2Str( typ ) );
    return found;
}

XP_Bool
addr_hasType( const CommsAddrRec* addr, CommsConnType typ )
{
    return types_hasType( addr->_conTypes, typ );
}

XP_Bool
types_hasType( XP_U16 conTypes, CommsConnType typ )
{
    /* Any address has NONE */
    XP_Bool hasType = COMMS_CONN_NONE == typ;
    if ( !hasType ) {
        hasType = 0 != (conTypes & (1 << (typ - 1)));
    }
    // XP_LOGF( "%s(%s) => %d", __func__, ConnType2Str(typ), hasType );
    return hasType;
}

CommsConnType 
addr_getType( const CommsAddrRec* addr )
{
    CommsConnType typ;
    XP_U32 st = 0;
    if ( !addr_iter( addr, &typ, &st ) ) {
        typ = COMMS_CONN_NONE;
    }
    XP_ASSERT( !addr_iter( addr, &typ, &st ) ); /* shouldn't be a second */
    // XP_LOGF( "%s(%p) => %s", __func__, addr, ConnType2Str( typ ) );
    return typ;
}

void
types_addType( XP_U16* conTypes, CommsConnType type )
{
    XP_ASSERT( COMMS_CONN_NONE != type );
    // XP_LOGF( "%s(%s)", __func__, ConnType2Str(type) );
    *conTypes |= 1 << (type - 1);
}

void
types_rmType( XP_U16* conTypes, CommsConnType type )
{
    XP_ASSERT( COMMS_CONN_NONE != type );
    *conTypes &= ~(1 << (type - 1));
}

void
addr_addType( CommsAddrRec* addr, CommsConnType type )
{
    types_addType( &addr->_conTypes, type );
}

void
addr_rmType( CommsAddrRec* addr, CommsConnType type )
{
    XP_ASSERT( COMMS_CONN_NONE != type );
    // XP_LOGF( "%s(%s)", __func__, ConnType2Str(type) );
    types_rmType( &addr->_conTypes, type );
}

/* Overwrites anything that might already be there. Use addr_addType() to add
   to the set */
void
addr_setType( CommsAddrRec* addr, CommsConnType type )
{
    XP_LOGFF( "(%p, %s)", addr, ConnType2Str(type) );
    XP_U16 flags = 0;
    if ( COMMS_CONN_NONE != type ) {
        flags = 1 << (type - 1);
    }
    addr->_conTypes = flags;
    XP_ASSERT( type == addr_getType( addr ) );
}

#ifdef XWFEATURE_RELAY
static XWHostID
getDestID( CommsCtxt* comms, XWEnv XP_UNUSED_DBG(xwe), XP_PlayerAddr channelNo )
{
    XWHostID id = HOST_ID_NONE;
    XP_Bool missingRelay = XP_FALSE;
    if ( (channelNo & CHANNEL_MASK) == CHANNEL_NONE ) {
        id = HOST_ID_SERVER;
    } else {
        XP_PlayerAddr masked = channelNo & ~CHANNEL_MASK;
        for ( AddressRecord* recs = comms->recs; !!recs; recs = recs->next ) {
            CNO_FMT( cbuf, recs->channelNo );
            XP_LOGFF( "rec %p has %s, hostID %d", recs, 
                     cbuf, recs->rr.hostID );
            if ( (recs->channelNo & ~CHANNEL_MASK) != masked ) {
                XP_LOGFF( "rejecting record %p; channelNo doesn't match", recs );
                logAddr( comms, xwe, &recs->addr, __func__ );
            } else if ( !addr_hasType( &recs->addr, COMMS_CONN_RELAY ) ) {
                XP_LOGFF( "rejecting record %p; no relay address", recs );
                logAddr( comms, xwe, &recs->addr, __func__ );
                missingRelay = XP_TRUE;
            } else {
                XP_ASSERT( HOST_ID_NONE == id ); /* no duplicates */
                id = recs->rr.hostID;
                // break;
            }
        }
    }

    /* If we get here AND we're a client, it may be the case that the server
       channel is what we want because though we haven't connected via relay
       yet we have a channel working via another transport. */
    if ( HOST_ID_NONE == id && missingRelay && !comms->isServer ) {
        XP_LOGFF( "special casing channel missing relay address" );
        id = HOST_ID_SERVER;
    }

    CNO_FMT( cbuf, channelNo );
    XP_LOGFF( "(%s) => %x", cbuf, id );
    return id;
} /* getDestID */

static XWStreamCtxt* 
relay_msg_to_stream( CommsCtxt* comms, XWEnv xwe, XWRELAY_Cmd cmd, XWHostID destID,
                     void* data, int datalen )
{
    XP_LOGFF( "(cmd=%s, destID=%x)", relayCmdToStr(cmd), destID );
    XWStreamCtxt* stream;
    stream = mem_stream_make_raw( MPPARM(comms->mpool)
                                  dutil_getVTManager(comms->dutil) );
    if ( stream != NULL ) {
        CommsAddrRec addr;
        stream_open( stream );
        stream_putU8( stream, cmd );

        comms_getAddr( comms, &addr );

        switch ( cmd ) {
        case XWRELAY_MSG_TORELAY:
            if ( COOKIE_ID_NONE == comms->rr.cookieID ) {
                XP_LOGFF( "cookieID still 0; background send?" );
            }
            stream_putU16( stream, comms->rr.cookieID );
        case XWRELAY_MSG_TORELAY_NOCONN:
            XP_ASSERT( 0 < comms->rr.myHostID );
            stream_putU8( stream, comms->rr.myHostID );
            XP_ASSERT( 0 < destID );
            stream_putU8( stream, destID );
            XP_LOGFF( "wrote ids src %d, dest %d", comms->rr.myHostID, destID );
            if ( data != NULL && datalen > 0 ) {
                stream_putBytes( stream, data, datalen );
            }
            break;
        case XWRELAY_GAME_CONNECT:
            stream_putU8( stream, XWRELAY_PROTO_VERSION );
            stream_putU16( stream, INITIAL_CLIENT_VERS );
            stringToStream( stream, addr.u.ip_relay.invite );
            stream_putU8( stream, addr.u.ip_relay.seeksPublicRoom );
            stream_putU8( stream, addr.u.ip_relay.advertiseRoom );
            /* XP_ASSERT( cmd == XWRELAY_GAME_RECONNECT */
            /*            || comms->rr.myHostID == HOST_ID_NONE */
            /*            || comms->rr.myHostID == HOST_ID_SERVER ); */
            XP_LOGFF( "writing nPlayersHere: %d; nPlayersTotal: %d",
                      comms->rr.nPlayersHere, comms->rr.nPlayersTotal );
            stream_putU8( stream, comms->rr.nPlayersHere );
            stream_putU8( stream, comms->rr.nPlayersTotal );
            stream_putU16( stream, comms_getChannelSeed(comms) );
            stream_putU8( stream, comms->util->gameInfo->dictLang );
            putDevID( comms, xwe, stream );
            stream_putU8( stream, comms->forceChannel ); /* "clientIndx" on relay */

            set_relay_state( comms, xwe, COMMS_RELAYSTATE_CONNECT_PENDING );
            break;

        case XWRELAY_GAME_RECONNECT:
            stream_putU8( stream, XWRELAY_PROTO_VERSION );
            stream_putU16( stream, INITIAL_CLIENT_VERS );
            stringToStream( stream, addr.u.ip_relay.invite );
            stream_putU8( stream, addr.u.ip_relay.seeksPublicRoom );
            stream_putU8( stream, addr.u.ip_relay.advertiseRoom );
            stream_putU8( stream, comms->rr.myHostID );
            XP_ASSERT( cmd == XWRELAY_GAME_RECONNECT
                       || comms->rr.myHostID == HOST_ID_NONE
                       || comms->rr.myHostID == HOST_ID_SERVER );
            XP_LOGFF( "writing nPlayersHere: %d; nPlayersTotal: %d",
                     comms->rr.nPlayersHere, comms->rr.nPlayersTotal );
            stream_putU8( stream, comms->rr.nPlayersHere );
            stream_putU8( stream, comms->rr.nPlayersTotal );
            stream_putU16( stream, comms_getChannelSeed(comms) );
            stream_putU8( stream, comms->util->gameInfo->dictLang );
            stringToStream( stream, comms->rr.connName );
            putDevID( comms, xwe, stream );
            set_relay_state( comms, xwe, COMMS_RELAYSTATE_CONNECT_PENDING );
            break;

        case XWRELAY_ACK:
            stream_putU8( stream, destID );
            break;

        case XWRELAY_GAME_DISCONNECT:
            stream_putU16( stream, comms->rr.cookieID );
            stream_putU8( stream, comms->rr.myHostID );
            break;

#if defined XWFEATURE_RELAY && defined RELAY_HEARTBEAT
        case XWRELAY_HEARTBEAT:
            /* Add these for grins.  Server can assert they match the IP
               address it expects 'em on. */
            stream_putU16( stream, comms->rr.cookieID );
            stream_putU8( stream, comms->rr.myHostID );
            break;
#endif
        default:
            XP_ASSERT(0); 
        }
    }
    return stream;
} /* relay_msg_to_stream */

static XP_Bool
send_via_relay( CommsCtxt* comms, XWEnv xwe, XWRELAY_Cmd cmd, XWHostID destID,
                void* data, int dlen, const XP_UCHAR* msgNo )
{
    XP_Bool success = XP_FALSE;
    if ( comms_getAddrDisabled( comms, COMMS_CONN_RELAY, XP_TRUE ) ) {
        XP_LOGFF( "dropping message because %s disabled",
                 ConnType2Str( COMMS_CONN_RELAY ) );
    } else {
        XWStreamCtxt* tmpStream = 
            relay_msg_to_stream( comms, xwe, cmd, destID, data, dlen );

        if ( tmpStream != NULL ) {
            XP_U16 len = 0;

            len = stream_getSize( tmpStream );
            if ( 0 < len ) {
                XP_U16 result;
                CommsAddrRec addr;

                comms_getAddr( comms, &addr );
                XP_LOGFF( "passing %d bytes to sendproc", len );
                result = (*comms->procs.send)( xwe, stream_getPtr(tmpStream), len,
                                               msgNo, &addr, COMMS_CONN_RELAY, 
                                               gameID(comms), 
                                               comms->procs.closure );
                success = result == len;
                if ( success ) {
                    setHeartbeatTimer( comms );
                }
            }
            stream_destroy( tmpStream, xwe );
        }
    }
    return success;
} /* send_via_relay */

static XP_Bool
sendNoConn( CommsCtxt* comms, XWEnv xwe, const MsgQueueElem* elem, XWHostID destID )
{
    LOG_FUNC();
    XP_Bool success = XP_FALSE;

    XP_UCHAR relayID[64];
    XP_U16 len = sizeof(relayID);
    success = NULL != comms->procs.sendNoConn
        && (0 != (comms->xportFlags & COMMS_XPORT_FLAGS_HASNOCONN))
        && formatRelayID( comms, destID, relayID, &len );
    if ( success ) {
        XWStreamCtxt* stream = 
            relay_msg_to_stream( comms, xwe, XWRELAY_MSG_TORELAY_NOCONN,
                                 destID, elem->msg, elem->len );
        if ( NULL != stream ) {
            XP_U16 len = stream_getSize( stream );
            if ( 0 < len ) {
                XP_UCHAR msgNo[16];
                formatMsgNo( comms, elem, msgNo, sizeof(msgNo) );
                success = (*comms->procs.sendNoConn)( xwe, stream_getPtr( stream ),
                                                      len, msgNo, relayID,
                                                      comms->procs.closure );
            }
            stream_destroy( stream, xwe);
        }
    }

    LOG_RETURNF( "%s", success?"TRUE":"FALSE" );
    return success;
}

/* Send a CONNECT message to the relay.  This opens up a connection to the
 * relay, and tells it our hostID and cookie so that it can associatate it
 * with a socket.  In the CONNECT_RESP we should get back what?
 */
static XP_Bool
relayConnect( CommsCtxt* comms, XWEnv xwe )
{
    LOG_FUNC();
    XP_Bool success = XP_TRUE;
    if ( addr_hasType( &comms->selfAddr, COMMS_CONN_RELAY ) ) {
        if ( 0 ) {
#ifdef RELAY_VIA_HTTP
        } else if ( comms->rr.connName[0] ) {
            set_relay_state( comms, xwe, COMMS_RELAYSTATE_USING_HTTP );
        } else {
            CommsAddrRec addr;
            comms_getAddr( comms, &addr );
            DevIDType ignored;  /*  but should it be? */
            (*comms->procs.requestJoin)( comms->procs.closure,
                                         util_getDevID( comms->util, xwe, &ignored ),
                                         addr.u.ip_relay.invite, /* room */
                                         comms->rr.nPlayersHere,
                                         comms->rr.nPlayersTotal,
                                         comms_getChannelSeed(comms),
                                         comms->util->gameInfo->dictLang );
            success = XP_FALSE;
#else
        } else if ( !comms->rr.connecting ) {
            comms->rr.connecting = XP_TRUE;
            success = send_via_relay( comms, xwe, comms->rr.connName[0]?
                                      XWRELAY_GAME_RECONNECT : XWRELAY_GAME_CONNECT,
                                      comms->rr.myHostID, NULL, 0, NULL );
            comms->rr.connecting = XP_FALSE;
#endif
        }
    }
    return success;
} /* relayConnect */
#endif

#ifdef DEBUG
static void
listRecs( const CommsCtxt* comms, const char* msg )
{
    XP_LOGFF( "nrecs: %d", countAddrRecs( comms ) );
    int ii = 0;
    for ( AddressRecord* rec = comms->recs; !!rec; rec = rec->next ) {
        CNO_FMT( cbuf, rec->channelNo );
        XP_LOGFF( "%s: rec[%d]: %s", msg, ii, cbuf );
        ++ii;
    }
}
#endif

#if defined XWFEATURE_IP_DIRECT || defined XWFEATURE_DIRECTIP
static XP_S16
send_via_bt_or_ip( CommsCtxt* comms, XWEnv xwe, BTIPMsgType msgTyp, XP_PlayerAddr channelNo,
                   CommsConnType typ, void* data, int dlen, const XP_UCHAR* msgNo )
{
    XP_S16 nSent;
    XP_U8* buf;
    LOG_FUNC();
    nSent = -1;
    buf = XP_MALLOC( comms->mpool, dlen + 1 );
    if ( !!buf ) {
        const CommsAddrRec* addr;
        (void)channelToAddress( comms, xwe, channelNo, &addr );

        buf[0] = msgTyp;
        if ( dlen > 0 ) {
            XP_MEMCPY( &buf[1], data, dlen );
        }

        nSent = (*comms->procs.sendMsg)( xwe, buf, dlen+1, msgNo, 0, addr, typ,
                                         gameID(comms), comms->procs.closure );
        XP_FREE( comms->mpool, buf );

        setHeartbeatTimer( comms );
    }
    LOG_RETURNF( "%d", nSent );
    return nSent;
} /* send_via_bt_or_ip */

#endif

#ifdef XWFEATURE_RELAY
static void
relayDisconnect( CommsCtxt* comms, XWEnv xwe )
{
    LOG_FUNC();
    if ( addr_hasType( &comms->selfAddr, COMMS_CONN_RELAY ) ) {
        if ( comms->rr.relayState > COMMS_RELAYSTATE_CONNECT_PENDING ) {
            (void)send_via_relay( comms, xwe, XWRELAY_GAME_DISCONNECT, HOST_ID_NONE,
                                  NULL, 0, NULL );
        }
        set_relay_state( comms, xwe, COMMS_RELAYSTATE_UNCONNECTED );
    }
} /* relayDisconnect */

#ifdef XWFEATURE_DEVID
static void
putDevID( const CommsCtxt* comms, XWEnv xwe, XWStreamCtxt* stream )
{
# if XWRELAY_PROTO_VERSION >= XWRELAY_PROTO_VERSION_CLIENTID
    DevIDType typ;
    const XP_UCHAR* devID = dutil_getDevID( comms->dutil, xwe, &typ );
    XP_ASSERT( ID_TYPE_NONE <= typ && typ < ID_TYPE_NTYPES );
    stream_putU8( stream, typ );
    if ( ID_TYPE_NONE != typ ) {
        stream_catString( stream, devID );
        stream_putU8( stream, '\0' );
    }
# else
    XP_ASSERT(0);
    XP_USE(comms);
    XP_USE(stream);
# endif
}
#endif

#endif

EXTERN_C_END

#endif /* #ifndef XWFEATURE_STANDALONE_ONLY */
