/* Copyright 2001 - 2024 by Eric House (xwords@eehouse.org).  All rights
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

// #define ENABLE_LOGFFV

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
#include "dllist.h"
#include "xwmutex.h"

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


#ifndef INITIAL_CLIENT_VERS
# define INITIAL_CLIENT_VERS 2
#endif

#ifdef DEBUG
# ifdef ENABLE_LOGFFV
#  define COMMS_LOGFFV COMMS_LOGFF
# else
#  define COMMS_LOGFFV(...)
# endif
# define COMMS_LOGFF( FMT, ... ) {                        \
        XP_U32 gameID = comms->util->gameInfo->gameID;    \
        XP_GID_LOGFF( gameID, FMT, ##__VA_ARGS__ );         \
    }
#else
# define COMMS_LOGFF( FMT, ... )
# define COMMS_LOGFFV(...)
#endif

EXTERN_C_START

typedef struct MsgQueueElem {
    SendMsgsPacket smp;
    XP_PlayerAddr channelNo;
#ifdef DEBUG
    XP_U16 sendCount;           /* how many times sent? */
#endif
    MsgID msgID;                /* saved for ease of deletion */
    Md5SumBuf sb;
} MsgQueueElem;

typedef struct AddressRecord {
    struct AddressRecord* next;
    MsgQueueElem* _msgQueueHead;

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
    XP_PlayerAddr channelNo;
    XP_U16 flags;                   /* storing only COMMS_VERSION */
    struct {
        XWHostID hostID;            /* used for relay case */
    } rr;
} AddressRecord;

#define ADDRESSRECORD_SIZE_68K 20

struct CommsCtxt {
    XW_UtilCtxt* util;
    XW_DUtilCtxt* dutil;
    MutexState mutex;

#ifdef DEBUG
    pthread_t lockHolder;
#endif

    XP_U32 connID;             /* set from gameID: 0 means ignore; otherwise
                                  must match.  Set by server. */
    XP_U16 streamVersion;       /* negotiated by server  */
    XP_PlayerAddr nextChannelNo;

    AddressRecord* recs;        /* return addresses */

    TransportProcs procs;

    RoleChangeProc rcProc;
    void* rcClosure;

    XP_U32 xportFlags;
    void* sendClosure;
    XP_U16 queueLen;
    XP_U16 channelSeed;         /* tries to be unique per device to aid
                                   dupe elimination at start */
    XP_U32 nextResend;          /* timestamp */
    XP_U16 resendBackoff;

#if defined XWFEATURE_RELAY
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

#define _FLAG_HARVEST_DONE 1    /* no longer used */
#define FLAG_QUASHED 2

#define QUASHED(COMMS) (0 != ((COMMS)->flags & FLAG_QUASHED))

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
static AddressRecord* rememberChannelAddress( CommsCtxt* comms,
                                              XP_PlayerAddr channelNo,
                                              XWHostID id,
                                              const CommsAddrRec* addr,
                                              XP_U16 flags );
static void augmentChannelAddr( CommsCtxt* comms, AddressRecord* rec,
                                const CommsAddrRec* addr, XWHostID hostID );
static XP_Bool augmentAddrIntrnl( CommsCtxt* comms, CommsAddrRec* dest,
                                  const CommsAddrRec* src, XP_Bool isNewer );
static XP_Bool channelToAddress( const CommsCtxt* comms, XP_PlayerAddr channelNo,
                                 const CommsAddrRec** addr );
static AddressRecord* getRecordFor( const CommsCtxt* comms, XP_PlayerAddr channelNo );
static XP_S16 sendMsg( const CommsCtxt* comms, XWEnv xwe, MsgQueueElem* elem,
                       CommsConnType filter );
static MsgQueueElem* addToQueue( CommsCtxt* comms, XWEnv xwe,
                                 MsgQueueElem* newElem, XP_Bool notify );
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

typedef ForEachAct (*EachMsgProc)( MsgQueueElem* elem, void* closure );
static void forEachElem( CommsCtxt* comms, EachMsgProc proc, void* closure );

static MsgQueueElem* makeNewElem( const CommsCtxt* comms, XWEnv xwe, MsgID msgID,
                                  XP_PlayerAddr channelNo );
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
static void _assertQueueOk( const CommsCtxt* comms, const char* func );
# define assertQueueOk(COM) _assertQueueOk((COM), __func__)

#define ASSERT_ADDR_OK(addr) assertAddrOk( addr )

# ifdef XWFEATURE_RELAY
static const char* relayCmdToStr( XWRELAY_Cmd cmd );
# endif
static void printQueue( const CommsCtxt* comms );
static void logAddrComms( const CommsCtxt* comms, const CommsAddrRec* addr,
                          const char* caller );
#else
# define ASSERT_ADDR_OK(addr)
# define assertQueueOk(comms)
# define printQueue( comms )
# define logAddr( comms, addr, caller)
# define listRecs( comms, caller )
# define logAddrComms(comms, addr, caller)
#endif  /* def DEBUG */

#if defined RELAY_HEARTBEAT
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

#if defined XWFEATURE_COMMSACK
static void sendEmptyMsg( CommsCtxt* comms, XWEnv xwe, AddressRecord* rec,
                          const CommsConnType filter );
#endif
static inline XP_Bool IS_INVITE(const MsgQueueElem* elem)
{
    return 0 == elem->smp.len;
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
        COMMS_LOGFF( TAGFMT() "%s => %s", TAGPRMS,
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
        COMMS_LOGFF( "set hostid: %x", comms->rr.myHostID );
    }
    set_relay_state( comms, xwe, COMMS_RELAYSTATE_UNCONNECTED );
    comms->rr.nPlayersHere = nPlayersHere;
    comms->rr.nPlayersTotal = nPlayersTotal;
    comms->rr.cookieID = COOKIE_ID_NONE;
    comms->rr.connName[0] = '\0';
}
#endif

CommsCtxt* 
comms_make( XWEnv xwe, XW_UtilCtxt* util, XP_Bool isServer,
            const CommsAddrRec* selfAddr, const CommsAddrRec* hostAddr,
            const TransportProcs* procs,
#ifdef XWFEATURE_RELAY
            XP_U16 nPlayersHere, XP_U16 nPlayersTotal,
            RoleChangeProc rcp, void* rcClosure,
#endif
            XP_U16 forceChannel )
{
    CommsCtxt* comms = (CommsCtxt*)XP_CALLOC( util->mpool, sizeof(*comms) );
    MUTEX_INIT_CHECKED( &comms->mutex, XP_TRUE, 3 );
    comms->util = util;
    comms->dutil = util_getDevUtilCtxt( util, xwe );
#ifdef DEBUG
    comms->tag = mpool_getTag(util->mpool);
    COMMS_LOGFF( TAGFMT(isServer=%d; forceChannel=%d), TAGPRMS, isServer, forceChannel );
#endif
    MPASSIGN(comms->mpool, util->mpool);

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

#ifdef XWFEATURE_RELAY
    XP_ASSERT( rcp );
    comms->rcProc = rcp;
    comms->rcClosure = rcClosure;

    init_relay( comms, xwe, nPlayersHere, nPlayersTotal );
#endif

    if ( !!selfAddr ) {
        ASSERT_ADDR_OK(selfAddr);
        logAddrComms( comms, &comms->selfAddr, "before selfAddr" );
        comms->selfAddr = *selfAddr;
        logAddrComms( comms, &comms->selfAddr, "after selfAddr" );
    }
    if ( !!hostAddr ) {
        XP_ASSERT( !isServer );
        logAddrComms( comms, hostAddr, __func__ );
        XP_PlayerAddr channelNo = comms_getChannelSeed( comms );
#ifdef DEBUG
        AddressRecord* rec = 
#endif
            rememberChannelAddress( comms, channelNo, 0, hostAddr,
                                    COMMS_VERSION );
        XP_ASSERT( rec == getRecordFor( comms, channelNo ) );
#ifdef DEBUG
        /* Anything in hostAddr should be supported -- in selfAddr */
        CommsConnType typ;
        for ( XP_U32 st = 0; addr_iter( hostAddr, &typ, &st ); ) {
            if ( !addr_hasType( &comms->selfAddr, typ ) ) {
                COMMS_LOGFF( "%s not in selfAddr", ConnType2Str(typ) );
                /* PENDING: fix this */
                // XP_ASSERT(0); <-- happening a lot (NFC missing)
            }
        }
#endif
    }

    return comms;
} /* comms_make */

static void
forEachElem( CommsCtxt* comms, EachMsgProc proc, void* closure )

{
    WITH_MUTEX(&comms->mutex);
    for ( AddressRecord* recs = comms->recs; !!recs; recs = recs->next ) {
        for ( MsgQueueElem** home = &recs->_msgQueueHead; !!*home; ) {
            MsgQueueElem* elem = *home;
            ForEachAct fea = (*proc)( elem, closure );
            if ( 0 != (FEA_REMOVE & fea) ) {
                *home = (MsgQueueElem*)elem->smp.next;
#ifdef DEBUG
                elem->smp.next = NULL;
#endif
                freeElem( MPPARM(comms->mpool) elem );
                XP_ASSERT( 1 <= comms->queueLen );
                --comms->queueLen;
            } else {
                home = (MsgQueueElem**)&elem->smp.next;
            }
            if ( 0 != (FEA_EXIT & fea) ) {
                goto done;
            }
        }
    }
 done:
    assertQueueOk( comms );
    END_WITH_MUTEX();
}

static ForEachAct
freeElemProc( MsgQueueElem* XP_UNUSED(elem), void* XP_UNUSED(closure) )
{
    return FEA_REMOVE;
}

static void
cleanupInternal( CommsCtxt* comms )
{
    forEachElem( comms, freeElemProc, NULL );
    XP_ASSERT( 0 == comms->queueLen );
} /* cleanupInternal */

static void
cleanupAddrRecs( CommsCtxt* comms )
{
    AddressRecord* recs;
    AddressRecord* next;

    for ( recs = comms->recs; !!recs; recs = next ) {
        next = recs->next;
        XP_ASSERT( !recs->_msgQueueHead );
        XP_FREE( comms->mpool, recs );
    }
    comms->recs = (AddressRecord*)NULL;
} /* cleanupAddrRecs */

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
comms_destroy( CommsCtxt* comms, XWEnv xwe )
{
    WITH_MUTEX(&comms->mutex);
    /* did I call comms_stop()? */
    XP_ASSERT( ! addr_hasType( &comms->selfAddr, COMMS_CONN_RELAY )
               || COMMS_RELAYSTATE_UNCONNECTED == comms->rr.relayState );

    cleanupInternal( comms );
    cleanupAddrRecs( comms );

    util_clearTimer( comms->util, xwe, TIMER_COMMS );

    END_WITH_MUTEX();
    MUTEX_DESTROY( &comms->mutex );
    XP_FREE( comms->mpool, comms );
} /* comms_destroy */

void
comms_setConnID( CommsCtxt* comms, XP_U32 connID, XP_U16 streamVersion )
{
    WITH_MUTEX(&comms->mutex);
    XP_ASSERT( CONN_ID_NONE != connID );
    XP_ASSERT( 0 == comms->connID || connID == comms->connID );
    comms->connID = connID;
    XP_ASSERT( 0 == comms->streamVersion
               || streamVersion == comms->streamVersion );
    comms->streamVersion = streamVersion;
    COMMS_LOGFF( "set connID (gameID) to %08X, streamVersion to 0x%X",
                 connID, streamVersion );
    END_WITH_MUTEX();
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
        IpRelay ip_relay = {};
        if ( version < STREAM_VERS_NORELAY ) {
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
    XP_MEMSET( addrP, 0, sizeof(*addrP) );
    XP_U8 tmp = stream_getU8( stream );
    XP_U16 version = stream_getVersion( stream );
    XP_ASSERT( 0 < version );
    if ( STREAM_VERS_MULTIADDR > version && (COMMS_CONN_NONE != tmp) ) {
        tmp = 1 << (tmp - 1);
    }
    addrP->_conTypes = tmp;

    CommsConnType typ;
    for ( XP_U32 st = 0; addr_iter( addrP, &typ, &st ); ) {
        addrFromStreamOne( addrP, stream, typ );
    }
    // ASSERT_ADDR_OK( addrP );
}

static void
getMQTTIDsFor( CommsCtxt* comms, XWEnv xwe, XP_U16 nRelayIDs,
               const XP_UCHAR* relayIDs[] )
{
    XP_USE(comms);
    XP_USE(xwe);
    XP_USE(nRelayIDs);
    XP_USE(relayIDs);
    // XP_ASSERT(0);        /* I see this on dev with many old games */
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
        getMQTTIDsFor( comms, xwe, nIds, ptrs );
    }
    LOG_RETURNF( "%s", boolToStr(allRemoved) );
    return allRemoved;
} /* removeRelayIf */

/* Looking toward a time when we store only the first couple of bits of
   channelNo. Not possible yet, though. */
static XP_U16
readChannelNo( XWStreamCtxt* stream )
{
    XP_U16 tmp = stream_getU16( stream );
    return tmp; //  & CHANNEL_MASK;
}

static void
writeChannelNo( XWStreamCtxt* stream, XP_PlayerAddr channelNo )
{
    // stream_putU16( stream, channelNo & CHANNEL_MASK );
    stream_putU16( stream, channelNo );
}

CommsCtxt* 
comms_makeFromStream( XWEnv xwe, XWStreamCtxt* stream,
                      XW_UtilCtxt* util, XP_Bool isServer,
                      const TransportProcs* procs,
#ifdef XWFEATURE_RELAY
                      RoleChangeProc rcp, void* rcClosure,
#endif
                      XP_U16 forceChannel )
{
    LOG_FUNC();
    XP_U16 version = stream_getVersion( stream );

    XP_U8 flags = stream_getU8( stream );
    if ( version < STREAM_VERS_GICREATED ) {
        flags = 0;
    }

    CommsAddrRec selfAddr = {};
    addrFromStream( &selfAddr, stream );
    if ( addr_hasType( &selfAddr, COMMS_CONN_MQTT )
         && 0 == selfAddr.u.mqtt.devID ) {
        XW_DUtilCtxt* dutil = util_getDevUtilCtxt( util, xwe );
        dvc_getMQTTDevID( dutil, xwe, &selfAddr.u.mqtt.devID );
    }
    ASSERT_ADDR_OK( &selfAddr );

    XP_U16 nPlayersHere, nPlayersTotal;
    if ( version >= STREAM_VERS_DEVIDS
         || addr_hasType( &selfAddr, COMMS_CONN_RELAY ) ) {
        nPlayersHere = (XP_U16)stream_getBits( stream, 4 );
        nPlayersTotal = (XP_U16)stream_getBits( stream, 4 );
    } else {
        nPlayersHere = 0;
        nPlayersTotal = 0;
    }
    CommsCtxt* comms = comms_make( xwe, util, isServer,
                                   NULL, NULL, procs,
#ifdef XWFEATURE_RELAY
                                   nPlayersHere, nPlayersTotal, rcp, rcClosure,
#endif
                                   forceChannel );
#ifndef XWFEATURE_RELAY
    XP_USE( nPlayersHere );
    XP_USE( nPlayersTotal );
#endif
    XP_MEMCPY( &comms->selfAddr, &selfAddr, sizeof(comms->selfAddr) );
    logAddrComms( comms, &selfAddr, __func__ );
    comms->flags = flags;

    comms->connID = stream_getU32( stream );
    XP_ASSERT( comms->streamVersion == 0 );
    if ( version >= STREAM_VERS_MSGSTREAMVERS ) {
        comms->streamVersion = stream_getU16( stream );
    }

    comms->nextChannelNo = readChannelNo( stream );
    if ( version < STREAM_VERS_CHANNELSEED ) {
        comms->channelSeed = 0;
    } else {
        comms->channelSeed = stream_getU16( stream );
    }
    if ( STREAM_VERS_COMMSBACKOFF <= version ) {
        comms->resendBackoff = stream_getU16( stream );
        comms->nextResend = stream_getU32( stream );
    }
    if ( addr_hasType( &selfAddr, COMMS_CONN_RELAY ) ) {
        comms->rr.myHostID = stream_getU8( stream );
        COMMS_LOGFFV( "loaded myHostID: %d", comms->rr.myHostID );
        stringFromStreamHere( stream, comms->rr.connName, 
                              sizeof(comms->rr.connName) );
    }

    XP_U16 queueLen = stream_getU8( stream );

    XP_U16 nAddrRecs = stream_getU8( stream );
    COMMS_LOGFFV( "nAddrRecs: %d", nAddrRecs );
    AddressRecord** prevsAddrNext = &comms->recs;
    for ( int ii = 0; ii < nAddrRecs; ++ii ) {
        AddressRecord* rec = (AddressRecord*)XP_CALLOC( util->mpool, sizeof(*rec));

        addrFromStream( &rec->addr, stream );
        logAddrComms( comms, &rec->addr, __func__ );

        if ( STREAM_VERS_SMALLCOMMS <= version ) {
            rec->nextMsgID = stream_getU32VL( stream );
            rec->lastMsgSaved = rec->lastMsgRcd = stream_getU32VL( stream );
            rec->flags = stream_getU16( stream );
        } else {
            rec->nextMsgID = stream_getU16( stream );
            rec->lastMsgSaved = rec->lastMsgRcd = stream_getU16( stream );
        }
#ifdef LOG_COMMS_MSGNOS
        COMMS_LOGFFV( "read lastMsgRcd of %d for addr %d", rec->lastMsgRcd, ii );
#endif
        if ( version >= STREAM_VERS_BLUETOOTH2 ) {
            rec->lastMsgAckd = stream_getU16( stream );
        }
        rec->channelNo = readChannelNo( stream );
        if ( addr_hasType( &rec->addr, COMMS_CONN_RELAY ) ) {
            rec->rr.hostID = stream_getU8( stream );
        }

        CNO_FMT( cbuf, rec->channelNo );
        COMMS_LOGFFV( "loaded rec %d: %s", ii, cbuf );

        *prevsAddrNext = rec;
        prevsAddrNext = &rec->next;
    }

    for ( int ii = 0; ii < queueLen; ++ii ) {
        MsgQueueElem* msg = (MsgQueueElem*)XP_CALLOC( util->mpool, sizeof(*msg) );

        msg->channelNo = readChannelNo( stream );
        if ( version >= STREAM_VERS_SMALLCOMMS ) {
            msg->msgID = stream_getU32VL( stream );
            msg->smp.len = stream_getU32VL( stream );
        } else {
            msg->msgID = stream_getU32( stream );
            msg->smp.len = stream_getU16( stream );
        }
        if ( version >= STREAM_VERS_MSGTIMESTAMP ) {
            msg->smp.createdStamp = stream_getU32( stream );
        }
        if ( 0 == msg->smp.createdStamp ) {
            msg->smp.createdStamp = dutil_getCurSeconds( comms->dutil, xwe );
        }
#ifdef DEBUG
        XP_ASSERT( 0 == msg->sendCount );
#endif
        XP_U16 len = msg->smp.len;
        if ( 0 == len ) {
            XP_ASSERT( isServer );
            XP_U32 nliLen = stream_getU32VL( stream );
            XWStreamCtxt* nliStream = mem_stream_make_raw( MPPARM(comms->mpool)
                                                           dutil_getVTManager(comms->dutil));
            stream_getFromStream( nliStream, stream, nliLen );
            NetLaunchInfo nli;
            if ( nli_makeFromStream( &nli, nliStream ) ) {
                msg->smp.buf = (XP_U8*)XP_MALLOC( util->mpool, sizeof(nli) );
                XP_MEMCPY( (void*)msg->smp.buf, &nli, sizeof(nli) );
                len = sizeof(nli); /* needed for checksum calc */
            } else {
                XP_ASSERT(0);
            }
            stream_destroy( nliStream );
        } else {
            msg->smp.buf = (XP_U8*)XP_MALLOC( util->mpool, len );
            stream_getBytes( stream, (XP_U8*)msg->smp.buf, len );
        }
        dutil_md5sum( comms->dutil, xwe, msg->smp.buf, len, &msg->sb );
        XP_ASSERT( NULL == msg->smp.next );
        if ( !addToQueue( comms, xwe, msg, XP_FALSE ) ) {
            --queueLen;         /* was dropped */
        }
    }
#ifdef DEBUG
    if ( queueLen != comms->queueLen ) {
        COMMS_LOGFF( "Error: queueLen %d != comms->queueLen %d",
                     queueLen, comms->queueLen );
        XP_ASSERT(0);
    }
#endif

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

    COMMS_LOGFF( "=>%p", comms );
    return comms;
} /* comms_makeFromStream */

/* 
 * Currently this disconnects an open connection.  Don't do that.
 */
void
comms_start( CommsCtxt* comms, XWEnv xwe )
{
    XP_ASSERT( !!comms );
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
    CommsConnType typ;
    for ( XP_U32 st = 0; addr_iter( &comms->selfAddr, &typ, &st ); ) {
        switch( typ ) {
#ifdef XWFEATURE_RELAY
        case COMMS_CONN_RELAY:
            if ( breakExisting
                 || COMMS_RELAYSTATE_UNCONNECTED == comms->rr.relayState ) {
                set_relay_state( comms, xwe, COMMS_RELAYSTATE_UNCONNECTED );
                if ( !relayConnect( comms, xwe ) ) {
                    COMMS_LOGFF( "relayConnect failed" );
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
        if ( stream_getVersion( stream ) < STREAM_VERS_NORELAY ) {
            IpRelay ip_relay = {};
#ifdef XWFEATURE_RELAY
            ip_relay = addrP->u.ip_relay;
#endif
            stringToStream( stream, ip_relay.invite );
            stringToStream( stream, ip_relay.hostName );
            stream_putU32( stream, ip_relay.ipAddr );
            stream_putU16( stream, ip_relay.port );
            stream_putBits( stream, 1, ip_relay.seeksPublicRoom );
            stream_putBits( stream, 1, ip_relay.advertiseRoom );
        }
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
        XP_ASSERT(0);
        break;
    }
} /* addrToStreamOne */

void
addrToStream( XWStreamCtxt* stream, const CommsAddrRec* addrP )
{
    XP_ASSERT( 0 < stream_getVersion(stream) ); /* must be set already */
    XP_U16 conTypes = addrP->_conTypes;
    types_rmType( &conTypes, COMMS_CONN_RELAY );
    stream_putU8( stream, conTypes );

    CommsConnType typ;
    for ( XP_U32 st = 0; types_iter( conTypes, &typ, &st ); ) {
        addrToStreamOne( stream, typ, addrP );
    }
}

typedef struct _E2SData {
    CommsCtxt* comms;
    XWStreamCtxt* stream;
    XP_U16 queueLen;
} E2SData;

static ForEachAct
elemToStream( MsgQueueElem* elem, void* closure )
{
    if ( 0 == elem->msgID && 0 < elem->smp.len ) {
        /* Maybe don't wrte non-invites with msgID of 0 */
        XP_LOGFF( "skipping apparent ACK elem" );
    } else {
        E2SData* e2sp = (E2SData*)closure;
        CommsCtxt* comms = e2sp->comms;

        ++e2sp->queueLen;

        XWStreamCtxt* stream = e2sp->stream;
        writeChannelNo( stream, elem->channelNo );
        stream_putU32VL( stream, elem->msgID );

        stream_putU32VL( stream, elem->smp.len );
        stream_putU32( stream, elem->smp.createdStamp );
        COMMS_LOGFFV( "writing msg elem with sum: %s", elem->sb.buf );
        if ( 0 == elem->smp.len ) {
            XP_ASSERT( 0 == elem->msgID );
            XWStreamCtxt* nliStream = mem_stream_make_raw( MPPARM(comms->mpool)
                                                           dutil_getVTManager(comms->dutil));
            NetLaunchInfo nli;
            XP_MEMCPY( &nli, elem->smp.buf, sizeof(nli) );
            nli_saveToStream( &nli, nliStream );
            XP_U16 nliLen = stream_getSize( nliStream );
            stream_putU32VL( stream, nliLen );
            stream_getFromStream( stream, nliStream, nliLen );
            stream_destroy( nliStream );
        } else {
            stream_putBytes( stream, elem->smp.buf, elem->smp.len );
        }
    }
    return FEA_OK;
}

void
comms_writeToStream( CommsCtxt* comms, XWStreamCtxt* stream, XP_U16 saveToken )
{
    WITH_MUTEX(&comms->mutex);
    XP_U16 nAddrRecs;
    AddressRecord* rec;

    listRecs( comms, __func__ );

    stream_setVersion( stream, CUR_STREAM_VERS );

    stream_putU8( stream, comms->flags );
    logAddrComms( comms, &comms->selfAddr, __func__ );
    addrToStream( stream, &comms->selfAddr );
    stream_putBits( stream, 4, comms->rr.nPlayersHere );
    stream_putBits( stream, 4, comms->rr.nPlayersTotal );

    stream_putU32( stream, comms->connID );
    stream_putU16( stream, comms->streamVersion );
    writeChannelNo( stream, comms->nextChannelNo );
    XP_U16 channelSeed = comms_getChannelSeed( comms ); /* force creation */
    stream_putU16( stream, channelSeed );
    stream_putU16( stream, comms->resendBackoff );
    stream_putU32( stream, comms->nextResend );
    if ( addr_hasType( &comms->selfAddr, COMMS_CONN_RELAY ) ) {
        stream_putU8( stream, comms->rr.myHostID );
        COMMS_LOGFF( "stored myHostID: %d", comms->rr.myHostID );
        stringToStream( stream, comms->rr.connName );
    }

    /* Next field is queueLen, but we don't know that until after we write the
       queue, since ACKs are not persisted. */
    XWStreamCtxt* tmpStream = mem_stream_make_raw( MPPARM(comms->mpool)
                                                   dutil_getVTManager(comms->dutil));
    stream_setVersion( tmpStream, CUR_STREAM_VERS );

    nAddrRecs = countAddrRecs(comms);
    stream_putU8( tmpStream, (XP_U8)nAddrRecs );

#ifdef LOG_COMMS_MSGNOS
    int ii = 0;
#endif
    for ( rec = comms->recs; !!rec; rec = rec->next ) {

        const CommsAddrRec* addr = &rec->addr;
        logAddrComms( comms, addr, __func__ );
        addrToStream( tmpStream, addr );

        stream_putU32VL( tmpStream, rec->nextMsgID );
        stream_putU32VL( tmpStream, rec->lastMsgRcd );
        stream_putU16( tmpStream, rec->flags );
#ifdef LOG_COMMS_MSGNOS
        COMMS_LOGFF( "wrote lastMsgRcd of %d for addr %d", rec->lastMsgRcd, ii++ );
#endif
        stream_putU16( tmpStream, (XP_U16)rec->lastMsgAckd );
        writeChannelNo( tmpStream, rec->channelNo );
        if ( addr_hasType( addr, COMMS_CONN_RELAY ) ) {
            stream_putU8( tmpStream, rec->rr.hostID ); /* unneeded unless RELAY */
        }
    }

    E2SData e2sd = { .comms = comms, .stream = tmpStream, };
    forEachElem( comms, elemToStream, &e2sd );
    COMMS_LOGFF( "wrote %d msg elems", e2sd.queueLen );

    XP_ASSERT( e2sd.queueLen <= 255 );
    XP_ASSERT( e2sd.queueLen <= comms->queueLen );
    stream_putU8( stream, (XP_U8)e2sd.queueLen );
    stream_getFromStream( stream, tmpStream, stream_getSize(tmpStream) );
    stream_destroy( tmpStream );

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
    END_WITH_MUTEX();
} /* comms_writeToStream */

static void
resetBackoff( CommsCtxt* comms )
{
    COMMS_LOGFF( "resetting backoff" );
    comms->resendBackoff = 0;
    comms->nextResend = 0;
}

void
comms_saveSucceeded( CommsCtxt* comms, XWEnv xwe, XP_U16 saveToken )
{
    WITH_MUTEX(&comms->mutex);
    COMMS_LOGFF( "(saveToken=%d)", saveToken );
    XP_ASSERT( !!comms );
    if ( saveToken == comms->lastSaveToken ) {
        AddressRecord* rec;
        for ( rec = comms->recs; !!rec; rec = rec->next ) {
            COMMS_LOGFF( "lastSave matches; updating lastMsgSaved (%d) to "
                         "lastMsgRcd (%d)", rec->lastMsgSaved, rec->lastMsgRcd );
            rec->lastMsgSaved = rec->lastMsgRcd;
        }
#ifdef XWFEATURE_COMMSACK
        comms_ackAny( comms, xwe );  /* might not want this for all transports */
#endif
    }
    END_WITH_MUTEX();
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

void
comms_addMQTTDevID( CommsCtxt* comms, XP_PlayerAddr channelNo,
                    const MQTTDevID* devID )
{
    WITH_MUTEX(&comms->mutex);
#ifdef NO_ADD_MQTT_TO_ALL       /* set for (usually) BT testing on Android */
    COMMS_LOGFF("ifdef'd out");
    XP_USE( comms );
    XP_USE( channelNo );
    XP_USE( devID );
#else
    CNO_FMT( cbuf, channelNo );
    COMMS_LOGFF( "(channelNo: %s, devID: " MQTTDevID_FMT ")", cbuf, *devID );
    XP_Bool found = XP_FALSE;
    for ( AddressRecord* rec = comms->recs; !!rec && !found; rec = rec->next ) {
        found = (rec->channelNo & ~CHANNEL_MASK) == (channelNo & ~CHANNEL_MASK);
        if ( found ) {
            if ( !addr_hasType( &comms->selfAddr, COMMS_CONN_MQTT ) ) {
                COMMS_LOGFF( "not adding mqtt because game doesn't allow it" );
            } else if ( addr_hasType( &rec->addr, COMMS_CONN_MQTT ) ) {
                XP_ASSERT( *devID == rec->addr.u.mqtt.devID );
            } else {
                CommsAddrRec tmp = {};
                addr_setType( &tmp, COMMS_CONN_MQTT );
                tmp.u.mqtt.devID = *devID;
                ASSERT_ADDR_OK( &tmp );

                augmentAddrIntrnl( comms, &rec->addr, &tmp, XP_TRUE );
                ASSERT_ADDR_OK( &rec->addr );
                CNO_FMT( cbuf, channelNo );
                COMMS_LOGFF( "added for channel %s", cbuf );
            }
        }
    }
    if ( !found ) {
        COMMS_LOGFF( "unable to augment address!!" );
        XP_ASSERT(0);
    }
#endif
    END_WITH_MUTEX();
}

void
comms_getAddrs( const CommsCtxt* comms, CommsAddrRec addrs[],
                XP_U16* nAddrs )
{
    XP_U16 count = 0;
    for ( AddressRecord* recs = comms->recs; !!recs; recs = recs->next ) {
        if ( count < *nAddrs ) {
            XP_MEMCPY( &addrs[count], &recs->addr, sizeof(addrs[count]) );
            logAddrComms( comms, &addrs[count], __func__ );
        }
        ++count;
    }
    *nAddrs = count;
}

void
comms_getChannelAddr( const CommsCtxt* comms, XP_PlayerAddr channelNo,
                      CommsAddrRec* addr )
{
    XP_U16 masked = channelNo & CHANNEL_MASK;
    XP_Bool found = XP_FALSE;
    for ( const AddressRecord* rec = comms->recs;
          !found && !!rec; rec = rec->next ) {
        found = (rec->channelNo & CHANNEL_MASK) == masked;
        if ( found ) {
            COMMS_LOGFF( "writing addr for channel %X", channelNo );
            *addr = rec->addr;
            logAddrComms( comms, addr, __func__ );
        }
    }
    XP_ASSERT( found );
}

XP_Bool
addrsAreSame( XW_DUtilCtxt* dutil, XWEnv xwe, const CommsAddrRec* addr1,
              const CommsAddrRec* addr2 )
{
    /* Empty addresses are the same only if both are empty */
    XP_Bool same = addr1->_conTypes == 0 && addr2->_conTypes == 0;

    CommsConnType typ;
    for ( XP_U32 st = 0; !same && addr_iter( addr1, &typ, &st ); ) {
        if ( addr_hasType( addr2, typ ) ) {
            switch ( typ ) {
            case COMMS_CONN_MQTT:
                same = addr1->u.mqtt.devID == addr2->u.mqtt.devID;
                break;
            case COMMS_CONN_SMS:
                same = addr1->u.sms.port == addr2->u.sms.port
                    && dutil_phoneNumbersSame( dutil, xwe, addr1->u.sms.phone, addr2->u.sms.phone );
                break;
            case COMMS_CONN_BT:
                same = 0 == XP_STRCMP( addr1->u.bt.hostName, addr2->u.bt.hostName );
                break;
            default:
                XP_LOGFF( "ignoring %s", ConnType2Str(typ) );
            }
        }
    }

    return same;
}

typedef struct _NonAcks {
    int count;
} NonAcks;

static ForEachAct
countNonAcks( MsgQueueElem* elem, void* closure )
{
    if ( IS_INVITE(elem) || 0 != elem->msgID ) {
        NonAcks* nap = (NonAcks*)closure;
        ++nap->count;
    }
    return FEA_OK;
}

XP_U16
comms_countPendingPackets( RELCONST CommsCtxt* comms, XP_Bool* quashed )
{
    NonAcks na = {};
    WITH_MUTEX(&comms->mutex);
    if ( !!quashed ) {
        *quashed = QUASHED(comms);
    }

    forEachElem( (CommsCtxt*)comms, countNonAcks, &na );

    // COMMS_LOGFF( "=> %d (queueLen = %d)", na.count, comms->queueLen );
    END_WITH_MUTEX();
    return na.count;
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
    COMMS_LOGFF( "(indx=%d)", indx );
    XWHostID hostID = HOST_ID_SERVER;
    if ( comms->isServer ) {
        hostID += 1 + indx;
    }
    XP_Bool success = formatRelayID( comms, hostID, buf, lenp );
    COMMS_LOGFF( "(%d) => %s", indx, buf );
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

void
comms_dropHostAddr( CommsCtxt* comms, CommsConnType typ )
{
    WITH_MUTEX(&comms->mutex);
    addr_rmType( &comms->selfAddr, typ );
    ASSERT_ADDR_OK( &comms->selfAddr );
    END_WITH_MUTEX();
}

XP_Bool
comms_getIsHost( const CommsCtxt* comms )
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
    newElem->smp.createdStamp = dutil_getCurSeconds( comms->dutil, xwe );
    newElem->channelNo = channelNo;
    newElem->msgID = msgID;
    return newElem;
}

static MsgQueueElem*
makeElemWithID( const CommsCtxt* comms, XWEnv xwe, MsgID msgID, AddressRecord* rec,
                XP_PlayerAddr channelNo, XWStreamCtxt* stream )
{
    CNO_FMT( cbuf, channelNo );
    COMMS_LOGFF( TAGFMT(%s), TAGPRMS, cbuf );
    XP_U16 streamSize = NULL == stream? 0 : stream_getSize( stream );
    MsgID lastMsgSaved = (!!rec)? rec->lastMsgSaved : 0;
    MsgQueueElem* newElem = makeNewElem( comms, xwe, msgID, channelNo );

    XP_Bool useSmallHeader = !!rec && (COMMS_VERSION == rec->flags);
    XWStreamCtxt* hdrStream = mem_stream_make_raw( MPPARM(comms->mpool)
                                                   dutil_getVTManager(comms->dutil));
    XP_ASSERT( 0L == comms->connID || comms->connID == comms->util->gameInfo->gameID );
    if ( !useSmallHeader ) {
        COMMS_LOGFF( TAGFMT() "putting connID %x", TAGPRMS, comms->connID );
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
    COMMS_LOGFF( TAGFMT() "put lastMsgSaved: %d", TAGPRMS, lastMsgSaved );
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
    stream_destroy( hdrStream );

    if ( 0 < streamSize ) {
        stream_getFromStream( msgStream, stream, streamSize );
    }

    newElem->smp.len = stream_getSize( msgStream );
    XP_ASSERT( 0 < newElem->smp.len );
    newElem->smp.buf = (XP_U8*)XP_MALLOC( comms->mpool, newElem->smp.len );
    stream_getBytes( msgStream, (XP_U8*)newElem->smp.buf, newElem->smp.len );
    stream_destroy( msgStream );

    dutil_md5sum( comms->dutil, xwe, newElem->smp.buf, newElem->smp.len,
                  &newElem->sb );
    XP_ASSERT( 0 < newElem->smp.len ); /* else NLI assumptions fail */
    return newElem;
} /* makeElemWithID */

#ifdef XWFEATURE_COMMS_INVITE
static MsgQueueElem*
makeInviteElem( CommsCtxt* comms, XWEnv xwe,
                XP_PlayerAddr channelNo, const NetLaunchInfo* nli )
{
    MsgQueueElem* newElem = makeNewElem( comms, xwe, 0, channelNo );

    XP_ASSERT( 0 == newElem->smp.len );           /* len == 0 signals is NLI */
    newElem->smp.buf = XP_MALLOC( comms->mpool, sizeof(*nli) );
    XP_MEMCPY( (XP_U8*)newElem->smp.buf, nli, sizeof(*nli) );
    dutil_md5sum( comms->dutil, xwe, newElem->smp.buf, sizeof(*nli), &newElem->sb );
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
        COMMS_LOGFF( "made seed: %s(%d)", cbuf, result );
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
    COMMS_LOGFF( "(channelNo=0x%X)", channelNo );
    assertQueueOk( comms );
    channelNo &= CHANNEL_MASK;

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
        XP_ASSERT( !!deadRec->_msgQueueHead ); /* otherwise we'll leak */
        freeElem( MPPARM(comms->mpool) deadRec->_msgQueueHead );
        deadRec->_msgQueueHead = NULL;
        --comms->queueLen;
        removeFromQueue( comms, xwe, channelNo, 0 );
        CNO_FMT( cbuf, deadRec->channelNo );
        COMMS_LOGFF( "removing rec for %s", cbuf );
        XP_ASSERT( !deadRec->_msgQueueHead );
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
    COMMS_LOGFF( "(%s) => %s", cbuf, boolToStr(found) );
    return found;
}

typedef struct _GetInviteChannelsData {
    XP_U16 hasInvitesMask;
    XP_U16 hasNonInvitesMask;
} GetInviteChannelsData;

static ForEachAct
getInviteChannels( MsgQueueElem* elem, void* closure )
{
    GetInviteChannelsData* gicdp = (GetInviteChannelsData*)closure;
    if ( IS_INVITE(elem) ) {
        XP_ASSERT( 0 == (gicdp->hasInvitesMask & (1 << elem->channelNo)) );
        gicdp->hasInvitesMask |= 1 << elem->channelNo;
    } else {
        gicdp->hasNonInvitesMask |= 1 << elem->channelNo;
    }
    return FEA_OK;
}

/* Choose a channel IFF nli doesn't already specify one. */
static XP_PlayerAddr
pickChannel( const CommsCtxt* comms, XWEnv xwe, const NetLaunchInfo* nli,
             const CommsAddrRec* destAddr )
{
    XP_PlayerAddr result = nli->forceChannel;

    if ( 0 == result ) {
        /* First, do we already have an invitation for this address */
        for ( AddressRecord* rec = comms->recs; !!rec; rec = rec->next ) {
            if ( addrsAreSame( comms->dutil, xwe, destAddr, &rec->addr ) ) {
                result = rec->channelNo & CHANNEL_MASK;
                XP_LOGFF( "addrs match; reusing channel %d", result );
                break;
            }
        }
    }

    if ( 0 == result ) {
        /* Data useful for next two steps: unused channel, then invites-only
           channel */
        GetInviteChannelsData gicd = {};
        forEachElem( (CommsCtxt*)comms, getInviteChannels, &gicd );

        /* Now find the first channelNo that doesn't have an invitation on it
           already */
        // const XP_U16 nPlayers = comms->util->gameInfo->nPlayers;
        for ( XP_PlayerAddr chan = 1; chan <= CHANNEL_MASK; ++chan ) {
            if ( 0 == (gicd.hasInvitesMask & (1 << chan)) ) {
                result = chan;
                XP_LOGFF( "using unused channel %d", result );
                break;
            }
        }

        if ( 0 == result ) {
            /* We need to find a channel to recycle. It should be one that has
               only an invite on it.*/
            for ( XP_PlayerAddr chan = 1; chan <= CHANNEL_MASK; ++chan ) {
                if ( 0 == (gicd.hasNonInvitesMask & (1 << chan)) ) {
                    result = chan;
                    XP_LOGFF( "recycling channel: %d", result );
                    break;
                }
            }
        }
    }

    COMMS_LOGFF( "=> 0X%X", result );
    return result;
}

void
comms_invite( CommsCtxt* comms, XWEnv xwe, const NetLaunchInfo* nli,
              const CommsAddrRec* destAddr, XP_Bool sendNow )
{
    COMMS_LOGFF("(sendNow=%s)", boolToStr(sendNow));
    LOGNLI(nli);
    WITH_MUTEX(&comms->mutex);
    XP_PlayerAddr forceChannel = pickChannel( comms, xwe, nli, destAddr );
    XP_LOGFF( "forceChannel: %d", forceChannel );
    XP_ASSERT( 0 < forceChannel );
    if ( 0 < forceChannel ) {
        XP_ASSERT( (forceChannel & CHANNEL_MASK) == forceChannel );
        if ( !haveRealChannel( comms, forceChannel ) ) {
            /* See if we have a channel for this address. Then see if we have an
               invite matching this one, and if not add one. Then trigger a send of
               it. */

            /* remove the old rec, if found */
            nukeInvites( comms, xwe, forceChannel );

            XP_U16 flags = COMMS_VERSION;
            /*AddressRecord* rec = */rememberChannelAddress( comms, forceChannel,
                                                             0, destAddr, flags );
            MsgQueueElem* elem = makeInviteElem( comms, xwe, forceChannel, nli );

            elem = addToQueue( comms, xwe, elem, XP_TRUE );
            if ( !!elem ) {
                XP_ASSERT( !elem->smp.next );
                COMMS_LOGFF( "added invite with sum %s on channel %d", elem->sb.buf,
                             elem->channelNo & CHANNEL_MASK );
                /* Let's let platform code decide whether to call sendMsg() . On
                   Android creating a game with an invitation in its queue is always
                   followed by opening the game, which results in comms_resendAll()
                   getting called leading to a second send immediately after this. So
                   let Android drop it. Linux, though, needs it for now. */
                if ( sendNow && !!comms->procs.sendInvt ) {
                    sendMsg( comms, xwe, elem, COMMS_CONN_NONE );
                }
            }
        }
    } else {
        XP_LOGFF( "dropping invite; no open channel found" );
    }
    END_WITH_MUTEX();
    LOG_RETURN_VOID();
}

typedef struct _GetInvitedData {
    XP_U16 allBits;
    XP_U16 count;
} GetInvitedData;

static ForEachAct
getInvitedProc( MsgQueueElem* elem, void* closure )
{
    if ( IS_INVITE( elem ) ) {
        GetInvitedData* gidp = (GetInvitedData*)closure;
        XP_PlayerAddr channelNo = elem->channelNo & CHANNEL_MASK;
        XP_LOGFF( "found invite on channel %d", channelNo );
        XP_U16 thisBit = 1 << channelNo;
        XP_ASSERT( 0 == (thisBit & gidp->allBits) ); /* should be no dupes */
        if ( 0 == (thisBit & gidp->allBits) ) {
            ++gidp->count;
        }
        gidp->allBits |= thisBit;
    }
    return FEA_OK;
}

void
comms_getInvited( RELCONST CommsCtxt* comms, XP_U16* nInvites )
{
    WITH_MUTEX(&comms->mutex);
    GetInvitedData gid = {};
    forEachElem( (CommsCtxt*)comms, getInvitedProc, &gid );
    *nInvites = gid.count;
    // LOG_RETURNF( "%d", *nInvites );
    END_WITH_MUTEX();
}

typedef struct _GetNamesData {
    const CommsCtxt* comms;
    XWEnv xwe;
    InviteeNames* names;
} GetNamesData;

static ForEachAct
getNamesProc( MsgQueueElem* elem, void* closure )
{
    LOG_FUNC();
    if ( IS_INVITE( elem ) ) {
        GetNamesData* gndp = (GetNamesData*)closure;
        XP_PlayerAddr channelNo = elem->channelNo & CHANNEL_MASK;
        XP_LOGFF( "channelNo: %d", channelNo );

        const AddressRecord* rec = getRecordFor( gndp->comms, channelNo );
        XP_ASSERT( !!rec );
        const CommsAddrRec* addr = &rec->addr;
        const XP_UCHAR* name =
            kplr_nameForAddress( gndp->comms->dutil, gndp->xwe, addr );
        InviteeNames* names = gndp->names;
        if ( !!name ) {
            XP_STRCAT( names->name[names->nNames], name );
            XP_LOGFF( "copied name %s to pos %d (pos %d)", name,
                      channelNo, names->nNames );
        }
        ++names->nNames;
    }
    return FEA_OK;
}

void
comms_inviteeNames( CommsCtxt* comms, XWEnv xwe,
                    InviteeNames* names )
{
    WITH_MUTEX(&comms->mutex);
    GetNamesData gnd = {
        .comms = comms,
        .xwe = xwe,
        .names = names,
    };
    forEachElem( (CommsCtxt*)comms, getNamesProc, &gnd );
    END_WITH_MUTEX();
}
#endif

/* Send a message using the sequentially next MsgID.  Save the message so
 * resend can work. */
XP_S16
comms_send( CommsCtxt* comms, XWEnv xwe, XWStreamCtxt* stream )
{
    XP_S16 result = -1;
    WITH_MUTEX(&comms->mutex);
    if ( 0 == stream_getSize(stream) ) {
        COMMS_LOGFF( "dropping 0-len message" );
    } else {
        XP_PlayerAddr channelNo = stream_getAddress( stream );
        CNO_FMT( cbuf, channelNo );
        COMMS_LOGFF( "%s", cbuf );
        AddressRecord* rec = getRecordFor( comms, channelNo );
        MsgID msgID = (!!rec)? ++rec->nextMsgID : 0;
        MsgQueueElem* elem;

        if ( 0 == channelNo ) {
            channelNo = comms_getChannelSeed(comms) & ~CHANNEL_MASK;
            COMMS_LOGFF( "new channelNo: %X", channelNo );
        }

        COMMS_LOGFF( TAGFMT() "assigning msgID=" XP_LD " on %s", TAGPRMS, msgID, cbuf );

        elem = makeElemWithID( comms, xwe, msgID, rec, channelNo, stream );
        if ( NULL != elem ) {
            elem = addToQueue( comms, xwe, elem, XP_TRUE );
            if ( !!elem ) {
                printQueue( comms );
                result = sendMsg( comms, xwe, elem, COMMS_CONN_NONE );
            }
        }
    }
    END_WITH_MUTEX();
    return result;
} /* comms_send */

static void
notifyQueueChanged( const CommsCtxt* comms, XWEnv xwe )
{
    if ( !!comms->procs.countChanged ) {
        XP_U16 count = comms->queueLen;
        XP_Bool quashed = QUASHED(comms);
        (*comms->procs.countChanged)( xwe, comms->procs.closure, count, quashed );
    }
}

/* Add new message to the end of the list.  The list needs to be kept in order
 * by ascending msgIDs within each channel since if there's a resend that's
 * the order in which they need to be sent.
 */
static MsgQueueElem*
addToQueue( CommsCtxt* comms, XWEnv xwe, MsgQueueElem* newElem, XP_Bool notify )
{
    MsgQueueElem* asAdded = newElem;
    WITH_MUTEX( &comms->mutex );
    newElem->smp.next = NULL;

    MsgQueueElem** head;
    AddressRecord* rec = getRecordFor( comms, newElem->channelNo );
    if ( !rec ) {
        freeElem( MPPARM(comms->mpool) newElem );
        asAdded = NULL;
        goto dropPacket;
    }
    head = &rec->_msgQueueHead;

    if ( !*head ) {
        *head = newElem;
    } else {
        while ( !!(*head)->smp.next ) {
            head = (MsgQueueElem**)&(*head)->smp.next;
        }
        if ( elems_same( *head, newElem ) ) {
            /* This does still happen! Not sure why. */
            freeElem( MPPARM(comms->mpool) newElem );
            asAdded = *head;
        } else {
            (*head)->smp.next = &newElem->smp;
        }

        XP_ASSERT( comms->queueLen > 0 );
    }

    if ( newElem == asAdded ) {
        ++comms->queueLen;
        /* Do I need this? PENDING */
        formatMsgNo( comms, newElem, (XP_UCHAR*)newElem->smp.msgNo,
                     sizeof(newElem->smp.msgNo) );
        if ( notify ) {
            notifyQueueChanged( comms, xwe );
        }
    }
 dropPacket:
    XP_ASSERT( comms->queueLen <= 128 ); /* reasonable limit in testing */
    END_WITH_MUTEX();
    return asAdded;
} /* addToQueue */

#ifdef DEBUG
static ForEachAct
printElem( MsgQueueElem* elem, void* closure )
{
    int* iip = (int*)closure;
    CNO_FMT( cbuf, elem->channelNo );
    XP_LOGFFV( "%d: %s; msgID=" XP_LD "; sum=%s",
               *iip, cbuf, elem->msgID, elem->sb.buf );
    ++*iip;
    return FEA_OK;
}

static void
printQueue( const CommsCtxt* comms )
{
    int ii = 0;
    forEachElem( (CommsCtxt*)comms, printElem, &ii );
}

static void
_assertQueueOk( const CommsCtxt* comms, const char* XP_UNUSED(func) )
{
    XP_U16 count = 0;

    for ( AddressRecord* recs = comms->recs; !!recs; recs = recs->next ) {
        for ( MsgQueueElem* elem = recs->_msgQueueHead; !!elem;
              elem = (MsgQueueElem*)elem->smp.next ) {
            ++count;
        }
    }
    if ( count != comms->queueLen ) {
        COMMS_LOGFF( "count(%d) != comms->queueLen(%d)", count, comms->queueLen );
        XP_ASSERT(0);
    }
    if ( count >= 10 ) {
        COMMS_LOGFFV( "queueLen unexpectedly high: %d", count );
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
        && elem1->smp.len == elem2->smp.len
        && 0 == XP_MEMCMP( elem1->smp.buf, elem2->smp.buf, elem1->smp.len );
    return same;
}

static void
freeElem( MPFORMAL MsgQueueElem* elem )
{
    XP_ASSERT( !elem->smp.next );
    XP_FREEP( mpool, &elem->smp.buf );
    XP_FREE( mpool, elem );
}

/* We've received on some channel a message with a certain ID.  This means
 * that all messages sent on that channel with lower IDs have been received
 * and can be removed from our queue.  BUT: if this ID is higher than any
 * we've sent, don't remove.  We may be starting a new game but have a server
 * that's still on the old one.
 */

typedef struct _RemoveData {
    const CommsCtxt* comms;
    XP_PlayerAddr channelNo;
    MsgID msgID;
} RemoveData;

static ForEachAct
removeProc( MsgQueueElem* elem, void* closure )
{
    ForEachAct result = FEA_OK;
    RemoveData* rdp = (RemoveData*)closure;

    XP_PlayerAddr maskedChannelNo = ~CHANNEL_MASK & rdp->channelNo;
    XP_Bool knownGood = XP_FALSE;
    /* remove the 0-channel message if we've established a channel number.
       Only clients should have any 0-channel messages in the queue, and
       receiving something from the server is an implicit ACK -- IFF it isn't
       left over from the last game. */

    XP_PlayerAddr maskedElemChannelNo = ~CHANNEL_MASK & elem->channelNo;
    if ( (maskedElemChannelNo == 0) && (rdp->channelNo != 0) ) {
        // not sure what this was doing....
        // XP_ASSERT( !rdp->comms->isServer || IS_INVITE(elem) );
        XP_ASSERT( elem->msgID == 0 );
    } else if ( maskedElemChannelNo != maskedChannelNo ) {
        knownGood = XP_TRUE;
    }

    if ( !knownGood && (elem->msgID <= rdp->msgID) ) {
        result = FEA_REMOVE;
    }
    return result;
}

static void
removeFromQueue( CommsCtxt* comms, XWEnv xwe, XP_PlayerAddr channelNo, MsgID msgID )
{
    WITH_MUTEX( &comms->mutex );
    assertQueueOk( comms );
    CNO_FMT( cbuf, channelNo );
    COMMS_LOGFFV( "(channelNo=%d): remove msgs <= " XP_LD " for %s (queueLen: %d)",
                  channelNo, msgID, cbuf, comms->queueLen );
#ifdef DEBUG
    XP_U16 prevLen = comms->queueLen;
#endif

    if ((channelNo == 0) || !!getRecordFor( comms, channelNo)) {

        RemoveData rd = {
            .comms = comms,
            .msgID = msgID,
            .channelNo = channelNo,
        };
        forEachElem( comms, removeProc, &rd );

        notifyQueueChanged( comms, xwe );
    }

    XP_ASSERT( comms->queueLen <= prevLen );
    COMMS_LOGFFV( "queueLen now %d (was %d)", comms->queueLen, prevLen );

#ifdef DEBUG
    assertQueueOk( comms );
    printQueue( comms );
#endif
    END_WITH_MUTEX();
} /* removeFromQueue */

static XP_U32
gameID( const CommsCtxt* comms )
{
    XP_U32 gameID = comms->connID;
    CurGameInfo* gi = comms->util->gameInfo;
    if ( 0 == gameID ) {
        gameID = gi->gameID;
    }

    /* Most of the time these will be the same, but early in a game they won't
       be.  Would be nice not to have to use gameID. */
    if ( 0 == gameID ) {
        COMMS_LOGFF( "gameID STILL 0" );
    } else if ( 0 == gi->gameID ) {
        COMMS_LOGFF( "setting gi's gameID to 0X%X", gameID );
        gi->gameID = gameID;
    }

    return gameID;
}

#ifdef DEBUG
typedef struct _CheckPrevState {
    const CommsCtxt* comms;
    const MsgQueueElem* elem;
    int count;
} CheckPrevState;

static ForEachAct
checkPrevProc( MsgQueueElem* elem, void* closure )
{
    CheckPrevState* cpsp = (CheckPrevState*)closure;
    if ( elem != cpsp->elem
         && (cpsp->elem->channelNo & CHANNEL_MASK) == (elem->channelNo & CHANNEL_MASK) ) {
        if ( 0 == cpsp->elem->msgID || elem->msgID < cpsp->elem->msgID ) {
            ++cpsp->count;
            XP_LOGFFV( "found one! their id: %d; my id: %d", elem->msgID,
                       cpsp->elem->msgID );
        }
    }
    return FEA_OK;
}

static void
checkForPrev( const CommsCtxt* comms, MsgQueueElem* elem, CommsConnType typ )
{
    if ( COMMS_CONN_MQTT == typ ) {
        CheckPrevState cps = { .comms = comms,
                               .elem = elem,
        };
        forEachElem( (CommsCtxt*)comms, checkPrevProc, &cps );
    }
}
#else
# define checkForPrev( comms, elem, typ )
#endif

static XP_S16
sendMsg( const CommsCtxt* comms, XWEnv xwe, MsgQueueElem* elem,
         const CommsConnType filter )
{
    XP_S16 result = -1;
    XP_PlayerAddr channelNo = elem->channelNo;
    CNO_FMT( cbuf, channelNo );

    XP_Bool isInvite = IS_INVITE(elem);
    COMMS_LOGFF( TAGFMT() "sending message on %s: id: %d; len: %d; sum: %s; isInvite: %s",
                 TAGPRMS, cbuf, elem->msgID, elem->smp.len, elem->sb.buf,
                 boolToStr(isInvite) );

    const CommsAddrRec* addrP = NULL;
    if ( comms->isServer ) {
        (void)channelToAddress( comms, channelNo, &addrP );
    } else {
        /* guest has only one peer, but old code might save several */
        if ( !!comms->recs ) {
            XP_ASSERT( !comms->recs->next ); // firing during upgrade test
            addrP = &comms->recs->addr;
        }
    }
    if ( QUASHED(comms) ) {
        // COMMS_LOGFF( "not sending; comms is quashed" );
    } else if ( NULL == addrP ) {
        COMMS_LOGFF( TAGFMT() "no addr for channel %x; dropping!'", TAGPRMS, channelNo );
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
                COMMS_LOGFF( "dropping message because %s disabled",
                             ConnType2Str( typ ) );
            } else if ( COMMS_CONN_NONE != filter && filter != typ ) {
                /* dropping it. But don't log, as it happens a lot with acks */
            } else {
                if ( !isInvite && !addr_hasType( &comms->selfAddr, typ ) ) {
                    COMMS_LOGFF( "self addr doesn't have msg type %s", ConnType2Str(typ) );
                    /* PENDING: fix this */
                    // XP_ASSERT( 0 ); <-- happens a lot
                }
                COMMS_LOGFF( TAGFMT() "sending msg with sum %s using typ %s", TAGPRMS,
                             elem->sb.buf, ConnType2Str(typ) );
                switch ( typ ) {
#ifdef XWFEATURE_RELAY
                case COMMS_CONN_RELAY: {
                    XWHostID destID = getDestID( comms, xwe, channelNo );
                    if ( HOST_ID_NONE == destID ) {
                        COMMS_LOGFF( TAGFMT() "skipping message via relay: no destID yet", TAGPRMS );
                    } else if ( haveRelayID( comms ) && sendNoConn( comms, xwe, elem, destID ) ) {
                        /* do nothing */
                        nSent = elem->smp.len;
                    } else if ( comms->rr.relayState >= COMMS_RELAYSTATE_CONNECTED ) {
                        XP_UCHAR msgNo[16];
                        formatMsgNo( comms, elem, msgNo, sizeof(msgNo) );
                        if ( send_via_relay( comms, xwe, XWRELAY_MSG_TORELAY, destID,
                                             elem->smp.buf, elem->smp.len, msgNo ) ) {
                            nSent = elem->smp.len;
                        }
                    } else {
                        COMMS_LOGFF( "skipping message: not connected to relay" );
                    }
                    break;
                }
#endif
#if defined XWFEATURE_IP_DIRECT
                case COMMS_CONN_BT:
                case COMMS_CONN_IP_DIRECT:
                    nSent = send_via_ip( comms, BTIPMSG_DATA, channelNo, 
                                         elem->smp.buf, elem->smp.len );
                    break;
#endif
                default:
                    XP_ASSERT( addr_hasType( &addr, typ ) );

                    /* A more general check that the address type has the settings
                       it needs would be better here.... */
                    if ( typ == COMMS_CONN_MQTT && 0 == addr.u.mqtt.devID ) {
                        COMMS_LOGFF( "not sending: MQTT address NULL" );
                        XP_ASSERT(0);
                        break;
                    }

                    if ( 0 ) {
#ifdef XWFEATURE_COMMS_INVITE
                    } else if ( isInvite ) {
                        if ( !!comms->procs.sendInvt ) {
                            NetLaunchInfo nli;
                            XP_MEMCPY( &nli, elem->smp.buf, sizeof(nli) );
                            XP_ASSERT( 0 != elem->smp.createdStamp );
                            nSent = (*comms->procs.sendInvt)( xwe, &nli,
                                                              elem->smp.createdStamp,
                                                              &addr, typ,
                                                              comms->procs.closure );
                        }
#endif
                    } else {
                        SendMsgsPacket* head = NULL;
                        if ( COMMS_CONN_MQTT == typ ) {
                            AddressRecord* rec = getRecordFor( comms, channelNo);
                            head = &rec->_msgQueueHead->smp;
#ifdef DEBUG
                            /* Make sure our message is in there!!! */
                            XP_Bool found = XP_FALSE;
                            for ( SendMsgsPacket* tmp = head; !found && !!tmp; tmp = tmp->next ) {
                                found = tmp == &elem->smp;
                            }
                            XP_ASSERT( found );
#endif
                        } else {
                            XP_ASSERT( !elem->smp.next );
                        }
                        if ( !head ) {
                            head = &elem->smp;
                        }
                        XP_ASSERT( !!head );
                        XP_ASSERT( !!comms->procs.sendMsgs );
                        XP_U32 gameid = gameID( comms );
                        logAddrComms( comms, &addr, __func__ );
                        XP_ASSERT( 0 != elem->smp.createdStamp );
                        nSent = (*comms->procs.sendMsgs)( xwe, head, comms->streamVersion,
                                                          &addr, typ, gameid,
                                                          comms->procs.closure );
                        checkForPrev( comms, elem, typ );
                    }
                    break;

                } /* switch */
            }
            if ( nSent > result ) {
                result = nSent;
            }
        } /* for */
    
        if ( result == elem->smp.len ) {
#ifdef DEBUG
            ++elem->sendCount;
#endif
            COMMS_LOGFF( "elem's sendCount since load: %d", elem->sendCount );
        }
        CNO_FMT( cbuf1, elem->channelNo );
        COMMS_LOGFF( "(%s; msgID=" XP_LD ", len=%d)=>%d", cbuf1, elem->msgID,
                     elem->smp.len, result );
    }
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

XP_S16
comms_resendAll( CommsCtxt* comms, XWEnv xwe, CommsConnType filter, XP_Bool force )
{
    XP_S16 count = 0;
    XP_ASSERT( !!comms );

    XP_U32 now = dutil_getCurSeconds( comms->dutil, xwe );
    if ( QUASHED(comms) ) {
        // COMMS_LOGFF( "not sending; comms is quashed" );
    } else if ( !force && (now < comms->nextResend) ) {
        COMMS_LOGFF( "aborting: %d seconds left in backoff",
                     comms->nextResend - now );
    } else {
        XP_U32 gameid = gameID( comms );
        for ( AddressRecord* rec = comms->recs; !!rec; rec = rec->next ) {
            const MsgQueueElem* const elem = rec->_msgQueueHead;
            const SendMsgsPacket* const head = &elem->smp;
            if ( !!head ) {
                CommsConnType typ;
                for ( XP_U32 st = 0; addr_iter( &rec->addr, &typ, &st ); ) {
                    if ( COMMS_CONN_NONE == filter || typ == filter ) {
                        if ( IS_INVITE(elem) ) {
                            NetLaunchInfo nli;
                            XP_MEMCPY( &nli, head->buf, sizeof(nli) );
                            (void)(*comms->procs.sendInvt)( xwe, &nli,
                                                            head->createdStamp,
                                                            &rec->addr, typ,
                                                            comms->procs.closure );
                            COMMS_LOGFF( "resent invite with sum %s", elem->sb.buf );
                            ++count;
                            XP_ASSERT( !head->next );
                        } else {
                            count += (*comms->procs.sendMsgs)( xwe, head, comms->streamVersion,
                                                               &rec->addr, typ, gameid,
                                                               comms->procs.closure );
                            COMMS_LOGFF( "resent msg with sum %s", elem->sb.buf );
                        }
                    }
                }
            }
        }

        /* Now set resend values */
        comms->resendBackoff = 2 * (1 + comms->resendBackoff);
        COMMS_LOGFF( "backoff now %d", comms->resendBackoff );
        comms->nextResend = now + comms->resendBackoff;
    }
    COMMS_LOGFF( TAGFMT() "(force=%s) => %d", TAGPRMS, boolToStr(force), count );
    return count;
}

#ifdef XWFEATURE_COMMSACK
static void
ackAnyImpl( CommsCtxt* comms, XWEnv xwe, XP_Bool force,
            const CommsConnType filter )
{
    WITH_MUTEX(&comms->mutex);
    if ( CONN_ID_NONE == comms->connID ) {
        COMMS_LOGFF( "doing nothing because connID still unset" );
    } else {
#ifdef DEBUG
        int nSent = 0;
        int nSeen = 0;
#endif
        for ( AddressRecord* rec = comms->recs; !!rec; rec = rec->next ) {
#ifdef DEBUG
            ++nSeen;
#endif
            if ( force || rec->lastMsgAckd < rec->lastMsgRcd ) {
#ifdef DEBUG
                ++nSent;
                CNO_FMT( cbuf, rec->channelNo );
                COMMS_LOGFF( "%s; %d < %d (or force: %s): rec getting ack",
                             cbuf, rec->lastMsgAckd, rec->lastMsgRcd,
                             boolToStr(force) );
#endif
                sendEmptyMsg( comms, xwe, rec, filter );
            }
        }
        COMMS_LOGFF( "sent for %d channels (of %d)", nSent, nSeen );
    }
    END_WITH_MUTEX();
}

void
comms_ackAny( CommsCtxt* comms, XWEnv xwe )
{
    ackAnyImpl( comms, xwe, XP_FALSE, COMMS_CONN_NONE );
}
#else
# define ackAnyImpl( comms, xwe, force, filter )
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
        COMMS_LOGFF( "unknown cmd: %d", cmd );
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
    COMMS_LOGFF( "myHostID: %d", myHostID );
    if ( comms->rr.myHostID != myHostID ) {
        COMMS_LOGFF( "changing rr.myHostID from %x to %x",
                     comms->rr.myHostID, myHostID );
        comms->rr.myHostID = myHostID;
    }

    isServer = HOST_ID_SERVER == comms->rr.myHostID;

    if ( isServer != comms->isServer ) {
        COMMS_LOGFF( "becoming%s a server", isServer ? "" : " NOT" );
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
            COMMS_LOGFF( "we're replacing connNames: %s overwritten by %s",
                         comms->rr.connName, connName );
        }
        XP_MEMCPY( comms->rr.connName, connName, sizeof(comms->rr.connName) );
        COMMS_LOGFF( "connName: \"%s\" (reconnect=%d)", connName,
                     reconnected );
    }
#else
    stringFromStreamHere( stream, comms->rr.connName, 
                          sizeof(comms->rr.connName) );
#endif

#ifdef XWFEATURE_DEVID
    DevIDType typ = stream_getU8( stream );
    XP_UCHAR devID[MAX_DEVID_LEN + 1] = {};
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
            COMMS_LOGFF( "have %d of %d players", nHere, nSought );
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
    COMMS_LOGFF( "(%s)", relayCmdToStr( cmd ) );
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
            COMMS_LOGFF( "changing hostid from %d to %d",
                         comms->rr.myHostID, srcID );
        }

        if ( COOKIE_ID_NONE == comms->rr.cookieID ) {
            COMMS_LOGFF( "cookieID still 0; background send?" );
        }

        if ( srcID != comms->rr.myHostID ) {
            COMMS_LOGFF( "set hostID: %x (was %x)", srcID, comms->rr.myHostID );
        }
        comms->rr.myHostID = srcID;

#ifdef DEBUG
        {
            XP_UCHAR connName[MAX_CONNNAME_LEN+1];
            stringFromStreamHere( stream, connName, sizeof(connName) );
            if ( comms->rr.connName[0] != '\0' 
                 && 0 != XP_STRCMP( comms->rr.connName, connName ) ) {
                COMMS_LOGFF( "we're replacing connNames: %s overwritten by %s",
                             comms->rr.connName, connName );
            }
            XP_MEMCPY( comms->rr.connName, connName, 
                       sizeof(comms->rr.connName) );
            COMMS_LOGFF( "connName: \"%s\"", connName );
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
        COMMS_LOGFF( "cookieID: %d; srcID: %x; destID: %x",
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
            COMMS_LOGFF( "keeping message though hostID not what "
                         "expected (%d vs %d)", destID, comms->rr.myHostID );
            consumed = XP_FALSE;
        }

        if ( consumed ) {
            COMMS_LOGFF( "rejecting data message (consumed)" );
        } else {
            *senderID = srcID;
        }
        break;

    case XWRELAY_DISCONNECT_OTHER:
        relayErr = stream_getU8( stream );
        srcID = stream_getU8( stream );
        COMMS_LOGFF( "host id %x disconnected", srcID );
        /* if we don't have connName then RECONNECTED is the wrong state to
           change to. */
        if ( COMMS_RELAYSTATE_RECONNECTED < comms->rr.relayState ) {
            XP_ASSERT( 0 != comms->rr.connName[0] );
            // XP_ASSERT( COOKIE_ID_NONE != comms->rr.cookieID ); /* firing!! */
            if ( COOKIE_ID_NONE == comms->rr.cookieID ) { /* firing!! */
                COMMS_LOGFF( "cookieID still COOKIE_ID_NONE; dropping!" );
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
        COMMS_LOGFF( "got reason: %s", XWREASON2Str( relayErr ) );
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
        COMMS_LOGFF( "dropping relay msg with cmd %d", (XP_U16)cmd );
    }
    
    LOG_RETURNF( "consumed=%s", boolToStr(consumed) );
    return consumed;
} /* relayPreProcess */
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
    return consumed;
} /* preProcess */

/* If this message is coming in in response to an invitation, we already have
   the channel set up. Be sure to use it or we may wind up nuking undelievered
   invitations */
static XP_Bool
getChannelFromInvite( const CommsCtxt* comms, XWEnv xwe,
                      const CommsAddrRec* retAddr, XP_PlayerAddr* channelNoP )
{
    XP_Bool found = XP_FALSE;
    for ( const AddressRecord* rec = comms->recs; !!rec && !found;
          rec = rec->next ) {
        found = addrsAreSame( comms->dutil, xwe, retAddr, &rec->addr );
        if ( found ) {
            COMMS_LOGFF( "channelNo before: %x", *channelNoP );
            *channelNoP |= rec->channelNo;
            COMMS_LOGFF( "channelNo after: %x", *channelNoP );
        }
    }
    COMMS_LOGFF( "=> %s", boolToStr(found) );
    return found;
}

static AddressRecord* 
getRecordFor( const CommsCtxt* comms, const XP_PlayerAddr channelNo )
{
    AddressRecord* rec;

    /* Use addr if we have it.  Otherwise use channelNo if non-0 */
    CNO_FMT( cbuf, channelNo );
    for ( rec = comms->recs; !!rec; rec = rec->next ) {
        /* guest should have only one rec max */
        XP_ASSERT( comms->isServer || !rec->next );

        CNO_FMT( cbuf1, rec->channelNo );
        COMMS_LOGFFV( "comparing rec channel %s with addr channel %s",
                      cbuf1, cbuf );

        /* Invite case: base on channelNo bits if the rest is 0 */
        if ( (0 == (rec->channelNo & ~CHANNEL_MASK)) && (0 == (channelNo & ~CHANNEL_MASK)) ) {
            if ( rec->channelNo == channelNo ) {
                break;
            }
        } else if ( (rec->channelNo & ~CHANNEL_MASK) == (channelNo & ~CHANNEL_MASK) ) {
            COMMS_LOGFFV( "match based on channels!!!" );
            /* This is so wrong for addresses coming from invites. Why works
               with GTK? */
            break;
        }
    }

    COMMS_LOGFFV( "(%s) => %p", cbuf, rec );
    return rec;
} /* getRecordFor */

/* It should be possible to find the next channel from what's already in use,
   and so to get rid of nextChannelNo, but this doesn't work yet. Saving the
   code for later... */
static XP_PlayerAddr
getNextChannelNo( CommsCtxt* comms )
{
    XP_PlayerAddr result = ++comms->nextChannelNo;
#if 0
    XP_U16 mask = 0;
    for ( AddressRecord* rec = comms->recs; !!rec; rec = rec->next ) {
        XP_U16 forceChannel = rec->channelNo & CHANNEL_MASK;
        XP_ASSERT( forceChannel <= CHANNEL_MASK );
        COMMS_LOGFF( "forceChannel: %d", forceChannel );
        mask |= 1 << forceChannel;
        COMMS_LOGFF( "mask now: %x", mask );
    }

    XP_PlayerAddr candidate;
    for ( candidate = 1; ; ++candidate ) {
        XP_ASSERT( candidate <= CHANNEL_MASK );
        if ( 0 == ((1 << candidate) & mask) ) {
            break;
        }
    }
    if ( candidate != result ) {
        COMMS_LOGFF( "ERROR: candidate: %d; result: %d", candidate, result );
    }
    // XP_ASSERT( candidate == result );
#endif
    LOG_RETURNF( "%d", result );
    return result;
}

static XP_Bool
checkChannelNo( CommsCtxt* comms, XWEnv xwe, const CommsAddrRec* retAddr,
                XP_PlayerAddr* channelNoP )
{
    XP_Bool success = XP_TRUE;
    XP_PlayerAddr channelNo = *channelNoP;
    if ( 0 == (channelNo & CHANNEL_MASK) ) {
        XP_ASSERT( comms->isServer );
        if ( getChannelFromInvite( comms, xwe, retAddr, &channelNo ) ) {
            success = XP_TRUE;
        } else {
            success = comms->nextChannelNo < CHANNEL_MASK;
            if ( success ) {
                channelNo |= getNextChannelNo( comms );
                CNO_FMT( cbuf, channelNo );
                COMMS_LOGFF( "assigned channelNo: %s", cbuf );
            }
        }
        // XP_ASSERT( comms->nextChannelNo <= CHANNEL_MASK );
    } else {
        /* Let's make sure we don't assign it later */
        XP_ASSERT( 0 );         /* does this happen */
        comms->nextChannelNo = channelNo;
    }
    *channelNoP = channelNo;
    COMMS_LOGFF( "=> %s", boolToStr(success) );
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
                        const CommsAddrRec* retAddr, XWHostID senderID,
                        XP_PlayerAddr* channelNoP, XP_U16 flags, MsgID msgID )
{
    CNO_FMT( cbuf, *channelNoP );
    COMMS_LOGFF( TAGFMT() "looking at %s", TAGPRMS, cbuf );

    AddressRecord* rec = getRecordFor( comms, *channelNoP );
    if ( !!rec ) {
        augmentChannelAddr( comms, rec, retAddr, senderID );

        /* Used to be that the initial message was where the channel
           record got created, but now the client creates an address for
           the host on startup (comms_make()) */
        if ( comms->isServer || 1 != msgID ) {
            COMMS_LOGFF( TAGFMT() "rejecting duplicate INIT message", TAGPRMS );
            rec = NULL;
        } else {
            COMMS_LOGFF( "accepting duplicate (?) msg" );
        }
    } else {
        if ( comms->isServer ) {
            if ( checkChannelNo( comms, xwe, retAddr, channelNoP ) ) {
                CNO_FMT( cbuf, *channelNoP );
                COMMS_LOGFF( TAGFMT() "augmented channel: %s", TAGPRMS, cbuf );
            } else {
                /* Why do I sometimes see these in the middle of a game
                   with lots of messages already sent?  connID of 0 should
                   only happen at the start! */
                COMMS_LOGFF( TAGFMT() "dropping msg because channel already set",
                             TAGPRMS );
                goto errExit;
            }
        }
        rec = rememberChannelAddress( comms, *channelNoP, senderID,
                                      retAddr, flags );
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
validateChannelMessage( CommsCtxt* comms, XWEnv xwe,
                        const CommsAddrRec* retAddr,
                        XP_PlayerAddr channelNo, XWHostID senderID,
                        MsgID msgID, MsgID lastMsgRcd )

{
    AddressRecord* rec;
    WITH_MUTEX(&comms->mutex);
    LOG_FUNC();

    rec = getRecordFor( comms, channelNo );
    if ( !!rec ) {
        removeFromQueue( comms, xwe, channelNo, lastMsgRcd );

        augmentChannelAddr( comms, rec, retAddr, senderID );

        if ( msgID == 0 ) {
            /* an ACK; do nothing */
            rec = NULL;
        } else if ( msgID == rec->lastMsgRcd + 1 ) {
            COMMS_LOGFFV( TAGFMT() "expected %d AND got %d", TAGPRMS,
                         msgID, msgID );
        } else {
            COMMS_LOGFF( TAGFMT() "expected %d, got %d", TAGPRMS,
                         rec->lastMsgRcd + 1, msgID );
            ackAnyImpl( comms, xwe, XP_TRUE, addr_getType( retAddr ) );
            rec = NULL;
        }
    } else {
        CNO_FMT( cbuf, channelNo );
        COMMS_LOGFF( TAGFMT() "no rec for %s", TAGPRMS, cbuf );
    }

    LOG_RETURNF( XP_P, rec );
    END_WITH_MUTEX();
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
    XP_Bool messageValid;
    WITH_MUTEX(&comms->mutex);
    messageValid = stream_gotU16( stream, &stuff->channelNo );
    if ( messageValid ) {
        XP_U16 channelSeed = comms_getChannelSeed( comms );
        XP_U16 flags = stuff->flags;

        /* First test isn't valid if we haven't passed the bit explicitly */
        if ( 0 != flags && (comms->isServer == (0 != (flags & IS_SERVER_BIT))) ) {
            COMMS_LOGFF( TAGFMT() "server bits mismatch; isServer: %d; flags: %x",
                         TAGPRMS, comms->isServer, flags );
            messageValid = XP_FALSE;
        } else if ( comms->isServer ) {
            /* channelNo comparison invalid */
        } else if ( 0 == stuff->channelNo || 0 == channelSeed ) {
            COMMS_LOGFF( TAGFMT() "one of channelNos still 0", TAGPRMS );
            XP_ASSERT(0);
        } else if ( (stuff->channelNo & ~CHANNEL_MASK)
                    != (channelSeed & ~CHANNEL_MASK) ) {
            COMMS_LOGFF( "channelNos test fails: %x vs %x", stuff->channelNo,
                         channelSeed );
            messageValid = XP_FALSE;
        }
    }
    LOG_RETURNF( "%s", boolToStr(messageValid) );
    END_WITH_MUTEX();
    return messageValid;
}

static XP_Bool
parseBeefHeader( CommsCtxt* comms, XWStreamCtxt* stream, HeaderStuff* stuff )
{
    XP_Bool messageValid =
        stream_gotU16( stream, &stuff->flags ) /* flags are the next short */
        && stream_gotU32( stream, &stuff->connID );
    COMMS_LOGFF( TAGFMT() "read connID (gameID) of %x", TAGPRMS, stuff->connID );

    messageValid = messageValid
        && getCheckChannelSeed( comms, stream, stuff )
        && stream_gotU32( stream, &stuff->msgID )
        && stream_gotU32( stream, &stuff->lastMsgRcd );

    // LOG_RETURNF( "%s", boolToStr(messageValid) );
    return messageValid;
}

static XP_Bool
parseSmallHeader( CommsCtxt* comms, XWStreamCtxt* msgStream,
                  HeaderStuff* stuff )
{
    XP_Bool messageValid = XP_FALSE;
    WITH_MUTEX(&comms->mutex);
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
        stream_destroy( hdrStream );
    }

    // LOG_RETURNF( "%s", boolToStr(messageValid) );
    END_WITH_MUTEX();
    return messageValid;
}

XP_Bool
comms_checkIncomingStream( CommsCtxt* comms, XWEnv xwe, XWStreamCtxt* stream,
                           const CommsAddrRec* retAddr, CommsMsgState* state )
{
    XP_Bool messageValid = XP_FALSE;
    WITH_MUTEX(&comms->mutex);
    XP_ASSERT( !!retAddr );     /* for now */
    XP_MEMSET( state, 0, sizeof(*state) );
#ifdef DEBUG
    state->comms = comms;
    if ( comms->processingMsg ) {
        COMMS_LOGFF( "processingMsg SET, so dropping message" );
        return XP_FALSE;
    }
    XP_ASSERT( !comms->processingMsg );
    comms->processingMsg = XP_TRUE;
    CommsConnType addrType = addr_getType( retAddr );
#endif

    COMMS_LOGFF( TAGFMT(retAddr.typ=%s), TAGPRMS, ConnType2Str(addrType ) );
    if ( comms_getAddrDisabled( comms, addrType, XP_FALSE ) ) {
        COMMS_LOGFF( "dropping message because %s disabled",
                     ConnType2Str( addrType ) );
    /* } else if (0 == (comms->selfAddr._conTypes & retAddr->_conTypes)) { */
    /*     /\* we don't expect messages with that address type; drop it *\/ */
    /*     XP_LOGF( "%s: not expecting %s messages", __func__,  */
    /*              ConnType2Str( addrType ) ); */
    } else {
#ifdef DEBUG
        if (0 == (comms->selfAddr._conTypes & retAddr->_conTypes)) {
            COMMS_LOGFF( "not expecting %s messages (but proceeding)",
                         ConnType2Str( addrType ) );
        }
#endif
        XWHostID senderID = 0;      /* unset; default for non-relay cases */
#ifdef XWFEATURE_RELAY
        XP_Bool usingRelay = XP_FALSE;
#endif

        XP_U16 initialLen = stream_getSize( stream );

        if ( !preProcess(
#ifdef XWFEATURE_RELAY
                         comms, xwe , stream, &usingRelay, &senderID,
#endif
                         retAddr ) ) {
            state->len = stream_getSize( stream );
            // stream_getPtr pts at base, but sum excludes relay header
            const XP_U8* ptr = initialLen - state->len + stream_getPtr( stream );
            Md5SumBuf sb;
            dutil_md5sum( comms->dutil, xwe, ptr, state->len, &sb );
            XP_STRCAT( state->sum, sb.buf );
            COMMS_LOGFF( TAGFMT() "got message of len %d with sum %s",
                         TAGPRMS, state->len, state->sum );

            HeaderStuff stuff = {};
            messageValid = stream_gotU16( stream, &stuff.flags );

            if ( messageValid ) {
                /* If BEEF is next sender is using old format. Otherwise
                   assume the bits are flags and BEEF is skipped by newer
                   code. Should work with anything newer then six years
                   ago. */
                if ( HAS_VERSION_FLAG == stuff.flags ) {
                    messageValid = parseBeefHeader( comms, stream, &stuff );
                } else if ( COMMS_VERSION == (stuff.flags & VERSION_MASK) ) {
                    messageValid = parseSmallHeader( comms, stream, &stuff );
                }
            }

            if ( messageValid ) {
                state->msgID = stuff.msgID;
                CNO_FMT( cbuf, stuff.channelNo );
                COMMS_LOGFF( TAGFMT() "rcd on %s: msgID=%d, lastMsgRcd=%d ",
                             TAGPRMS, cbuf, stuff.msgID, stuff.lastMsgRcd );
            } else {
                COMMS_LOGFF( TAGFMT() "got message to self?", TAGPRMS ); /* firing */
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
                    COMMS_LOGFF( TAGFMT() "unexpected connID (%x vs %x) ; "
                                 "dropping message", TAGPRMS, comms->connID,
                                 stuff.connID );
                }
            }

            messageValid = messageValid && (NULL != rec)
                && (0 == rec->lastMsgRcd || rec->lastMsgRcd <= stuff.msgID);
            if ( messageValid ) {
                CNO_FMT( cbuf, stuff.channelNo );
                COMMS_LOGFF( TAGFMT() "got %s; msgID=%d; len=%d", TAGPRMS, cbuf,
                             stuff.msgID, streamSize );
                state->channelNo = stuff.channelNo;
                comms->lastSaveToken = 0; /* lastMsgRcd no longer valid */
                stream_setAddress( stream, stuff.channelNo );
                messageValid = streamSize > 0;
                resetBackoff( comms );
            }
        }

    }
    LOG_RETURNF( "%s (len: %d; sum: %s)", boolToStr(messageValid), state->len, state->sum );
    END_WITH_MUTEX();
    return messageValid;
} /* comms_checkIncomingStream */

void
comms_msgProcessed( CommsCtxt* comms, XWEnv xwe,
                    CommsMsgState* state, XP_Bool rejected )
{
    WITH_MUTEX(&comms->mutex);
    assertQueueOk( comms );

    COMMS_LOGFF( "rec: %p; len: %d; sum: %s; id: %d; rejected: %s", state->rec,
                 state->len, state->sum, state->msgID, boolToStr(rejected) );

    XP_ASSERT( comms == state->comms );
    XP_ASSERT( comms->processingMsg );

    if ( rejected ) {
        if ( !!state->rec ) {
            COMMS_LOGFF( "should I remove rec???; msgID: %d", state->msgID );
            XP_ASSERT( 1 >= state->msgID );
            /* this is likely a mistake!!! Why remove it??? */
            // removeAddrRec( comms, xwe, state->rec );
        }
#ifdef LOG_COMMS_MSGNOS
        COMMS_LOGFF( "msg rejected; NOT upping lastMsgRcd to %d", state->msgID );
#endif
    } else {
        AddressRecord* rec = getRecordFor( comms, state->channelNo );
        XP_ASSERT( !!rec );
        if ( !!rec && rec->lastMsgRcd < state->msgID ) {
#ifdef LOG_COMMS_MSGNOS
            COMMS_LOGFF( "upping lastMsgRcd from %d to %d", rec->lastMsgRcd, state->msgID );
#endif
            rec->lastMsgRcd = state->msgID;
        }
        // COMMS_LOGFF( "CALLING nukeInvites(); might be wrong" );
        nukeInvites( comms, xwe, state->channelNo );
    }

#ifdef DEBUG
    comms->processingMsg = XP_FALSE;
#endif
    END_WITH_MUTEX();
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
            COMMS_LOGFF( "unexpected type %s", ConnType2Str(typ) );
        }
    }
    return result;
}

XP_Bool
comms_setQuashed( CommsCtxt* comms, XWEnv xwe, XP_Bool quashed )
{
    XP_U8 flags = comms->flags;
    if ( quashed ) {
        flags |= FLAG_QUASHED;
    } else {
        flags &= ~FLAG_QUASHED;
    }
    XP_Bool changed = flags != comms->flags;
    if ( changed ) {
        comms->flags = flags;
        COMMS_LOGFF( "(quashed=%s): changing state", boolToStr(quashed) );
        notifyQueueChanged( comms, xwe );
    }
    return changed;
}

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

#ifdef XWFEATURE_COMMSACK
static void
sendEmptyMsg( CommsCtxt* comms, XWEnv xwe, AddressRecord* rec,
              const CommsConnType filter )
{
    WITH_MUTEX(&comms->mutex);
    MsgQueueElem* elem = makeElemWithID( comms, xwe, 0 /* msgID */, 
                                         rec, rec? rec->channelNo : 0, NULL );
    XP_ASSERT( !!elem );
    elem = addToQueue( comms, xwe, elem, XP_FALSE );
    if ( !!elem ) {
        sendMsg( comms, xwe, elem, filter );
    }
    END_WITH_MUTEX();
} /* sendEmptyMsg */
#endif

#ifdef RELAY_HEARTBEAT
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
    }
    return XP_FALSE;            /* no need for redraw */
} /* p_comms_timerFired */

static void
setHeartbeatTimer( CommsCtxt* comms )
{
    XP_ASSERT( !!comms );

    if ( comms->hbTimerPending ) {
        COMMS_LOGFF( "skipping b/c hbTimerPending" );
    } else if ( comms->reconTimerPending ) {
        COMMS_LOGFF( "skipping b/c reconTimerPending" );
    } else {
        XP_U16 when = 0;
#ifdef XWFEATURE_RELAY
        if ( comms->selfAddr.conType == COMMS_CONN_RELAY ) {
            when = comms->rr.heartbeat;
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

static ForEachAct
statsProc( MsgQueueElem* elem, void* closure )
{
    XWStreamCtxt* stream = (XWStreamCtxt*)closure;
    XP_UCHAR buf[100];
    XP_SNPRINTF( buf, sizeof(buf),
                 "msgID: " XP_LD ": channelNo=%.4X; len=%d\n",
                 elem->msgID, elem->channelNo, elem->smp.len );
    stream_catString( stream, buf );
    return FEA_OK;
}

void
comms_getStats( RELCONST CommsCtxt* comms, XWStreamCtxt* stream )
{
    WITH_MUTEX(&comms->mutex);
    XP_UCHAR buf[100];

    XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf), 
                 (XP_UCHAR*)"role: %s; msg queue len: %d; quashed: %s;\n",
                 comms->isServer ? "host" : "guest",
                 comms->queueLen, boolToStr(QUASHED(comms)) );
    stream_catString( stream, buf );

    forEachElem( (CommsCtxt*)comms, statsProc, stream );

    for ( AddressRecord* rec = comms->recs; !!rec; rec = rec->next ) {
        XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf),
                     (XP_UCHAR*)"Stats for channel %.4X msgs\n",
                     rec->channelNo );
        stream_catString( stream, buf );

        XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf),
                     (XP_UCHAR*)"  Last sent: " XP_LD "; ",
                     rec->nextMsgID );
        stream_catString( stream, buf );

        XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf),
                     (XP_UCHAR*)"last rcvd: %d\n",
                     rec->lastMsgRcd );
        stream_catString( stream, buf );
    }
    END_WITH_MUTEX();
} /* comms_getStats */

void
comms_setAddrDisabled( CommsCtxt* comms, CommsConnType typ, 
                       XP_Bool send, XP_Bool disabled )
{
    XP_ASSERT( !!comms );
    COMMS_LOGFF( "(typ=%s, send=%d, disabled=%d)",
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
rememberChannelAddress( CommsCtxt* comms, XP_PlayerAddr channelNo,
                        XWHostID hostID, const CommsAddrRec* addr, XP_U16 flags )
{
    AddressRecord* rec = NULL;
    WITH_MUTEX( &comms->mutex);
    CNO_FMT( cbuf, channelNo );
    COMMS_LOGFF( "(%s)", cbuf );
    listRecs( comms, "entering rememberChannelAddress" );

    logAddrComms( comms, addr, __func__ );
    rec = getRecordFor( comms, channelNo );
    if ( !rec ) {
        /* not found; add a new entry */
        rec = (AddressRecord*)XP_CALLOC( comms->mpool, sizeof(*rec) );

        rec->channelNo = channelNo;
        rec->rr.hostID = hostID;
        rec->flags = flags & VERSION_MASK;

        rec->next = comms->recs;
        comms->recs = rec;
        COMMS_LOGFF( "creating rec %p for %s, hostID = %d, flags=0x%x",
                     rec, cbuf, hostID, flags );
    }

    /* overwrite existing address with new one.  I assume that's the right
       move. */
    if ( !!rec ) {
        if ( !!addr ) {
            COMMS_LOGFF( "replacing/adding addr with _conTypes %x with %x",
                         rec->addr._conTypes, addr->_conTypes );
            XP_MEMCPY( &rec->addr, addr, sizeof(rec->addr) );
            XP_ASSERT( rec->rr.hostID == hostID );
        } else {
            COMMS_LOGFF( "storing addr with _conTypes %x",
                         addr->_conTypes );
            XP_MEMSET( &rec->addr, 0, sizeof(rec->addr) );
            rec->addr._conTypes = comms->selfAddr._conTypes;
            // addr_setTypes( &recs->addr, addr_getTypes( &comms->selfAddr ) );
        }
    }
    listRecs( comms, "leaving rememberChannelAddress()" );
    END_WITH_MUTEX();
    return rec;
} /* rememberChannelAddress */

#ifdef DEBUG

static void
logAddrComms( const CommsCtxt* comms, const CommsAddrRec* addr,
              const char* caller )
{
    logAddr( comms->dutil, addr, caller );
}

void
logAddr( XW_DUtilCtxt* dutil, const CommsAddrRec* addr,
         const char* caller )
{
    if ( !!addr ) {
        char buf[128];
        XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(dutil->mpool)
                                                    dutil_getVTManager(dutil));
        if ( !!caller ) {
            snprintf( buf, sizeof(buf), "called on %p from %s:\n",
                      addr, caller );
            stream_catString( stream, buf );
        }

        CommsConnType typ;
        XP_Bool first = XP_TRUE;
        for ( XP_U32 st = 0; addr_iter( addr, &typ, &st ); ) {
            if ( !first ) {
                stream_catString( stream, "\n" );
            }

            snprintf( buf, sizeof(buf), "* %s: ", ConnType2Str(typ) );
            stream_catString( stream, buf );

            switch( typ ) {
            case COMMS_CONN_RELAY:
#ifdef XWFEATURE_RELAY
                stream_catString( stream, "room: " );
                stream_catString( stream, addr->u.ip_relay.invite );
                stream_catString( stream, "; host: " );
                stream_catString( stream, addr->u.ip_relay.hostName );
#endif
                break;
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
        stream_destroy( stream );
    }
}
#endif

static void
augmentChannelAddr( CommsCtxt* comms, AddressRecord* const rec,
                    const CommsAddrRec* addr, XWHostID hostID )
{
    WITH_MUTEX( &comms->mutex);
    augmentAddrIntrnl( comms, &rec->addr, addr, XP_TRUE );
#ifdef XWFEATURE_RELAY
    if ( addr_hasType( &rec->addr, COMMS_CONN_RELAY ) ) {
        if ( 0 != hostID ) {
            rec->rr.hostID = hostID;
            COMMS_LOGFF( "set hostID for rec %p to %d", rec, hostID );
        }
    }
#else
    XP_USE(hostID);
#endif

#ifdef DEBUG
    CommsConnType typ;
    for ( XP_U32 st = 0; addr_iter( addr, &typ, &st ); ) {
        if ( !addr_hasType( &comms->selfAddr, typ ) ) {
            COMMS_LOGFF( "main addr missing type %s", ConnType2Str(typ) );
        }
    }
#endif
    END_WITH_MUTEX();
}

static XP_Bool
augmentAddrIntrnl( CommsCtxt* comms, CommsAddrRec* destAddr,
                   const CommsAddrRec* srcAddr, XP_Bool isNewer )
{
    XP_Bool changed = XP_FALSE;
    ASSERT_ADDR_OK( srcAddr );
    const CommsAddrRec empty = {};
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
channelToAddress( const CommsCtxt* comms, XP_PlayerAddr channelNo,
                  const CommsAddrRec** addr )
{
    AddressRecord* recs = getRecordFor( comms, channelNo );
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

XP_Bool
addr_isEmpty( const CommsAddrRec* addr )
{
    CommsConnType typ;
    XP_U32 st = 0;
    return !addr_iter( addr, &typ, &st );
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
            COMMS_LOGFF( "rec %p has %s, hostID %d", recs,
                         cbuf, recs->rr.hostID );
            if ( (recs->channelNo & ~CHANNEL_MASK) != masked ) {
                COMMS_LOGFF( "rejecting record %p; channelNo doesn't match", recs );
                logAddr( comms, &recs->addr, __func__ );
            } else if ( !addr_hasType( &recs->addr, COMMS_CONN_RELAY ) ) {
                COMMS_LOGFF( "rejecting record %p; no relay address", recs );
                logAddr( comms, &recs->addr, __func__ );
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
        COMMS_LOGFF( "special casing channel missing relay address" );
        id = HOST_ID_SERVER;
    }

    CNO_FMT( cbuf, channelNo );
    COMMS_LOGFF( "(%s) => %x", cbuf, id );
    return id;
} /* getDestID */

static XWStreamCtxt* 
relay_msg_to_stream( CommsCtxt* comms, XWEnv xwe, XWRELAY_Cmd cmd, XWHostID destID,
                     void* data, int datalen )
{
    COMMS_LOGFF( "(cmd=%s, destID=%x)", relayCmdToStr(cmd), destID );
    XWStreamCtxt* stream;
    stream = mem_stream_make_raw( MPPARM(comms->mpool)
                                  dutil_getVTManager(comms->dutil) );
    if ( stream != NULL ) {
        CommsAddrRec addr;
        stream_putU8( stream, cmd );

        comms_getAddr( comms, &addr );

        switch ( cmd ) {
        case XWRELAY_MSG_TORELAY:
            if ( COOKIE_ID_NONE == comms->rr.cookieID ) {
                COMMS_LOGFF( "cookieID still 0; background send?" );
            }
            stream_putU16( stream, comms->rr.cookieID );
        case XWRELAY_MSG_TORELAY_NOCONN:
            XP_ASSERT( 0 < comms->rr.myHostID );
            stream_putU8( stream, comms->rr.myHostID );
            XP_ASSERT( 0 < destID );
            stream_putU8( stream, destID );
            COMMS_LOGFF( "wrote ids src %d, dest %d", comms->rr.myHostID, destID );
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
            COMMS_LOGFF( "writing nPlayersHere: %d; nPlayersTotal: %d",
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
            COMMS_LOGFF( "writing nPlayersHere: %d; nPlayersTotal: %d",
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
        COMMS_LOGFF( "dropping message because %s disabled",
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
                COMMS_LOGFF( "passing %d bytes to sendproc", len );
                result = (*comms->procs.send)( xwe, stream_getPtr(tmpStream), len,
                                               msgNo, &addr, COMMS_CONN_RELAY, 
                                               gameID(comms), 
                                               comms->procs.closure );
                success = result == len;
                if ( success ) {
                    setHeartbeatTimer( comms );
                }
            }
            stream_destroy( tmpStream );
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
                                 destID, elem->smp.buf, elem->smp.len );
        if ( NULL != stream ) {
            XP_U16 len = stream_getSize( stream );
            if ( 0 < len ) {
                XP_UCHAR msgNo[16];
                formatMsgNo( comms, elem, msgNo, sizeof(msgNo) );
                success = (*comms->procs.sendNoConn)( xwe, stream_getPtr( stream ),
                                                      len, msgNo, relayID,
                                                      comms->procs.closure );
            }
            stream_destroy( stream );
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
    XP_USE(msg);
    COMMS_LOGFFV( "nrecs: %d", countAddrRecs( comms ) );
    int ii = 0;
    for ( AddressRecord* rec = comms->recs; !!rec; rec = rec->next ) {
        CNO_FMT( cbuf, rec->channelNo );
        COMMS_LOGFFV( "%s: rec[%d]: %s", msg, ii, cbuf );
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
        (void)channelToAddress( comms, channelNo, &addr );

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
