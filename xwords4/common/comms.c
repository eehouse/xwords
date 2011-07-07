/* -*- compile-command: "cd ../linux && make MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 2001-2009 by Eric House (xwords@eehouse.org).  All rights
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
#include "game.h"
#include "xwstream.h"
#include "memstream.h"
#include "xwrelay.h"
#include "strutils.h"

#define cEND 0x65454e44
#define HEARTBEAT_NONE 0

#ifndef XWFEATURE_STANDALONE_ONLY

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
    XP_U8* msg;
    XP_U16 len;
    XP_PlayerAddr channelNo;
    XP_U16 sendCount;           /* how many times sent? */
    MsgID msgID;                /* saved for ease of deletion */
#ifdef COMMS_CHECKSUM
    gchar* checksum;
#endif
} MsgQueueElem;

typedef struct AddressRecord {
    struct AddressRecord* next;
    CommsAddrRec addr;
#ifdef DEBUG
    XP_U16 lastACK;
    XP_U16 nUniqueBytes;
#endif
    MsgID nextMsgID;        /* on a per-channel basis */
    MsgID lastMsgRcd;        /* on a per-channel basis */
    /* only used if COMMS_HEARTBEAT set except for serialization (to_stream) */
    XP_PlayerAddr channelNo;
    struct {
        XWHostID hostID;            /* used for relay case */
    } r;
#ifdef COMMS_HEARTBEAT
    XP_Bool initialSeen;
#endif
} AddressRecord;

#define ADDRESSRECORD_SIZE_68K 20

struct CommsCtxt {
    XW_UtilCtxt* util;

    XP_U32 connID;             /* set from gameID: 0 means ignore; otherwise
                                  must match.  Set by server. */
    XP_PlayerAddr nextChannelNo;

    AddressRecord* recs;        /* return addresses */

    TransportProcs procs;
#ifdef COMMS_HEARTBEAT
    XP_U32  lastMsgRcd;
#endif
    void* sendClosure;

    MsgQueueElem* msgQueueHead;
    MsgQueueElem* msgQueueTail;
    XP_U16 queueLen;
    XP_U16 channelSeed;         /* tries to be unique per device to aid
                                   dupe elimination at start */
#ifdef COMMS_HEARTBEAT
    XP_Bool doHeartbeat;
    XP_U32 lastMsgRcvdTime;
#endif
#if defined XWFEATURE_RELAY || defined COMMS_HEARTBEAT
    XP_Bool hbTimerPending;
    XP_Bool reconTimerPending;
#endif

    /* The following fields, down to isServer, are only used if
       XWFEATURE_RELAY is defined, but I'm leaving them in here so apps built
       both ways can open each other's saved games files.*/
    CommsAddrRec addr;

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
    } r;

    XP_Bool isServer;
#ifdef DEBUG
    XP_U16 nUniqueBytes;
#endif
    MPSLOT
};

#if defined XWFEATURE_BLUETOOTH || defined XWFEATURE_IP_DIRECT
typedef enum {
    BTIPMSG_NONE = 0
    ,BTIPMSG_DATA
    ,BTIPMSG_RESET
    ,BTIPMSG_HB
} BTIPMsgType;
#endif

/****************************************************************************
 *                               prototypes 
 ****************************************************************************/
static AddressRecord* rememberChannelAddress( CommsCtxt* comms, 
                                              XP_PlayerAddr channelNo, 
                                              XWHostID id, 
                                              const CommsAddrRec* addr );
static void updateChannelAddress( AddressRecord* rec, const CommsAddrRec* addr );
static XP_Bool channelToAddress( CommsCtxt* comms, XP_PlayerAddr channelNo, 
                                 const CommsAddrRec** addr );
static AddressRecord* getRecordFor( CommsCtxt* comms, const CommsAddrRec* addr,
                                    XP_PlayerAddr channelNo, XP_Bool maskChnl );
static XP_S16 sendMsg( CommsCtxt* comms, MsgQueueElem* elem );
static void addToQueue( CommsCtxt* comms, MsgQueueElem* newMsgElem );
static void freeElem( const CommsCtxt* comms, MsgQueueElem* elem );

static XP_U16 countAddrRecs( const CommsCtxt* comms );
static void sendConnect( CommsCtxt* comms, XP_Bool breakExisting );

#ifdef XWFEATURE_RELAY
static XP_Bool relayConnect( CommsCtxt* comms );
static void relayDisconnect( CommsCtxt* comms );
static XP_Bool send_via_relay( CommsCtxt* comms, XWRELAY_Cmd cmd, 
                               XWHostID destID, void* data, int dlen );
static XWHostID getDestID( CommsCtxt* comms, XP_PlayerAddr channelNo );
static void set_reset_timer( CommsCtxt* comms );
# ifdef DEBUG
static const char* relayCmdToStr( XWRELAY_Cmd cmd );
# endif
#endif
#if defined XWFEATURE_RELAY || defined COMMS_HEARTBEAT
static void setHeartbeatTimer( CommsCtxt* comms );

#else
# define setHeartbeatTimer( comms )
#endif
#if defined XWFEATURE_BLUETOOTH || defined XWFEATURE_IP_DIRECT
static XP_S16 send_via_bt_or_ip( CommsCtxt* comms, BTIPMsgType typ, 
                                 XP_PlayerAddr channelNo,
                                 void* data, int dlen );
#endif

/****************************************************************************
 *                               implementation 
 ****************************************************************************/
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
set_relay_state( CommsCtxt* comms, CommsRelayState state )
{
    if ( comms->r.relayState != state ) {
        XP_LOGF( "%s: %s => %s", __func__,
                 CommsRelayState2Str(comms->r.relayState), 
                 CommsRelayState2Str(state) );
        comms->r.relayState = state;
        if ( !!comms->procs.rstatus ) {
            (*comms->procs.rstatus)( comms->procs.closure, state );
        }
    }
}

static void
init_relay( CommsCtxt* comms, XP_U16 nPlayersHere, XP_U16 nPlayersTotal )
{
    comms->r.myHostID = comms->isServer? HOST_ID_SERVER: HOST_ID_NONE;
    XP_LOGF( "%s: set hostid: %x", __func__, comms->r.myHostID );
    set_relay_state( comms, COMMS_RELAYSTATE_UNCONNECTED );
    comms->r.nPlayersHere = nPlayersHere;
    comms->r.nPlayersTotal = nPlayersTotal;
    comms->r.cookieID = COOKIE_ID_NONE;
    comms->r.connName[0] = '\0';
}
#endif

CommsCtxt* 
comms_make( MPFORMAL XW_UtilCtxt* util, XP_Bool isServer, 
            XP_U16 XP_UNUSED_RELAY(nPlayersHere), 
            XP_U16 XP_UNUSED_RELAY(nPlayersTotal),
            const TransportProcs* procs )
{
    CommsCtxt* result = (CommsCtxt*)XP_MALLOC( mpool, sizeof(*result) );
    XP_MEMSET( result, 0, sizeof(*result) );

    MPASSIGN(result->mpool, mpool);

    result->isServer = isServer;
    if ( !!procs ) {
        XP_MEMCPY( &result->procs, procs, sizeof(result->procs) );
    }
    result->util = util;

#ifdef XWFEATURE_RELAY
    init_relay( result, nPlayersHere, nPlayersTotal );
#endif
    return result;
} /* comms_make */

static void
cleanupInternal( CommsCtxt* comms ) 
{
    MsgQueueElem* msg;
    MsgQueueElem* next;

    for ( msg = comms->msgQueueHead; !!msg; msg = next ) {
        next = msg->next;
        freeElem( comms, msg );
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


void
comms_resetSame( CommsCtxt* comms )
{
    comms_reset( comms, comms->isServer, 
                 comms->r.nPlayersHere, comms->r.nPlayersTotal );
}

static void
reset_internal( CommsCtxt* comms, XP_Bool isServer, 
                XP_U16 XP_UNUSED_RELAY(nPlayersHere), 
                XP_U16 XP_UNUSED_RELAY(nPlayersTotal),
                XP_Bool XP_UNUSED_RELAY(resetRelay) )
{
    LOG_FUNC();
#ifdef XWFEATURE_RELAY
    if ( resetRelay ) {
        relayDisconnect( comms );
    }
#endif

    cleanupInternal( comms );
    comms->isServer = isServer;

    cleanupAddrRecs( comms );

    comms->nextChannelNo = 0;
    if ( resetRelay ) {
        comms->channelSeed = 0;
    }

    comms->connID = CONN_ID_NONE;
#ifdef XWFEATURE_RELAY
    if ( resetRelay ) {
        init_relay( comms, nPlayersHere, nPlayersTotal );
    }
#endif
    LOG_RETURN_VOID();
} /* comms_reset */

void
comms_reset( CommsCtxt* comms, XP_Bool isServer, 
             XP_U16 nPlayersHere, 
             XP_U16 nPlayersTotal )
{
    reset_internal( comms, isServer, nPlayersHere, nPlayersTotal, XP_TRUE );
}

#ifdef XWFEATURE_RELAY

static XP_Bool
p_comms_resetTimer( void* closure, XWTimerReason XP_UNUSED_DBG(why) )
{
    CommsCtxt* comms = (CommsCtxt*)closure;
    LOG_FUNC();
    XP_ASSERT( why == TIMER_COMMS );

    /* Once we're denied we don't try again.  A new game or save and re-open
       will reset comms and get us out of this state. */
    if ( comms->r.relayState != COMMS_RELAYSTATE_DENIED ) {
        XP_Bool success = comms->r.relayState >= COMMS_RELAYSTATE_CONNECTED
            || relayConnect( comms );

        if ( success ) {
            comms->reconTimerPending = XP_FALSE;
            setHeartbeatTimer( comms );  /* in case we killed it with this
                                            one.... */
        } else {
            set_reset_timer( comms );
        }
    }

    return XP_FALSE;            /* no redraw required */
} /* p_comms_resetTimer */

static void
set_reset_timer( CommsCtxt* comms )
{
    /* This timer is allowed to overwrite a heartbeat timer, but not
       vice-versa.  Make sure we can restart it. */
    comms->hbTimerPending = XP_FALSE;
    util_setTimer( comms->util, TIMER_COMMS, 15,
                   p_comms_resetTimer, comms );
    comms->reconTimerPending = XP_TRUE;
} /* set_reset_timer */

void
comms_transportFailed( CommsCtxt* comms  )
{
    LOG_FUNC();
    XP_ASSERT( !!comms );
    if ( COMMS_CONN_RELAY == comms->addr.conType
         && comms->r.relayState != COMMS_RELAYSTATE_DENIED ) {
        relayDisconnect( comms );

        set_reset_timer( comms );
    }
    LOG_RETURN_VOID();
}
#endif  /* XWFEATURE_RELAY */

void
comms_destroy( CommsCtxt* comms )
{
    CommsAddrRec aNew;
    aNew.conType = COMMS_CONN_NONE;
    util_addrChange( comms->util, &comms->addr, &aNew );

    cleanupInternal( comms );
    cleanupAddrRecs( comms );

    util_clearTimer( comms->util, TIMER_COMMS );

    XP_FREE( comms->mpool, comms );
} /* comms_destroy */

void
comms_setConnID( CommsCtxt* comms, XP_U32 connID )
{
    XP_ASSERT( CONN_ID_NONE != connID );
    comms->connID = connID;
    XP_STATUSF( "%s: set connID (gameID) to %lx", __func__, connID );
} /* comms_setConnID */

static void
addrFromStream( CommsAddrRec* addrP, XWStreamCtxt* stream )
{
    CommsAddrRec addr;

    addr.conType = stream_getU8( stream );

    switch( addr.conType ) {
    case COMMS_CONN_NONE:
        break;
    case COMMS_CONN_BT:
        stringFromStreamHere( stream, addr.u.bt.hostName,
                              sizeof(addr.u.bt.hostName) );
        stream_getBytes( stream, &addr.u.bt.btAddr.bits, 
                         sizeof(addr.u.bt.btAddr.bits) );
        break;
    case COMMS_CONN_IR:
        /* nothing to save */
        break;
    case COMMS_CONN_IP_DIRECT:
        stringFromStreamHere( stream, addr.u.ip.hostName_ip,
                              sizeof(addr.u.ip.hostName_ip) );
        addr.u.ip.ipAddr_ip = stream_getU32( stream );
        addr.u.ip.port_ip = stream_getU16( stream );
        break;
    case COMMS_CONN_RELAY:
        stringFromStreamHere( stream, addr.u.ip_relay.invite,
                              sizeof(addr.u.ip_relay.invite) );
        stringFromStreamHere( stream, addr.u.ip_relay.hostName,
                              sizeof(addr.u.ip_relay.hostName) );
        addr.u.ip_relay.ipAddr = stream_getU32( stream );
        addr.u.ip_relay.port = stream_getU16( stream );
        if ( stream_getVersion( stream ) >= STREAM_VERS_DICTLANG ) {
            addr.u.ip_relay.seeksPublicRoom = stream_getBits( stream, 1 );
            addr.u.ip_relay.advertiseRoom = stream_getBits( stream, 1 );
        }
        break;
    case COMMS_CONN_SMS:
        stringFromStreamHere( stream, addr.u.sms.phone, 
                              sizeof(addr.u.sms.phone) );
        addr.u.sms.port = stream_getU16( stream );
        break;
    default:
        /* shut up, compiler */
        break;
    }

    XP_MEMCPY( addrP, &addr, sizeof(*addrP) );
} /* addrFromStream */

CommsCtxt* 
comms_makeFromStream( MPFORMAL XWStreamCtxt* stream, XW_UtilCtxt* util,
                      const TransportProcs* procs )
{
    CommsCtxt* comms;
    XP_Bool isServer;
    XP_U16 nAddrRecs, nPlayersHere, nPlayersTotal;
    AddressRecord** prevsAddrNext;
    MsgQueueElem** prevsQueueNext;
    XP_U16 version = stream_getVersion( stream );
    CommsAddrRec addr;
    short i;

    isServer = stream_getU8( stream );
    if ( version < STREAM_VERS_RELAY ) {
        XP_MEMSET( &addr, 0, sizeof(addr) );
        addr.conType = COMMS_CONN_IR; /* all there was back then */
    } else {
        addrFromStream( &addr, stream );
    }

    if ( addr.conType == COMMS_CONN_RELAY ) {
        nPlayersHere = (XP_U16)stream_getBits( stream, 4 );
        nPlayersTotal = (XP_U16)stream_getBits( stream, 4 );
    } else {
        nPlayersHere = 0;
        nPlayersTotal = 0;
    }
    comms = comms_make( MPPARM(mpool) util, isServer, 
                        nPlayersHere, nPlayersTotal, procs );
    XP_MEMCPY( &comms->addr, &addr, sizeof(comms->addr) );

    comms->connID = stream_getU32( stream );
    comms->nextChannelNo = stream_getU16( stream );
    if ( version < STREAM_VERS_CHANNELSEED ) {
        comms->channelSeed = 0;
    } else {
        comms->channelSeed = stream_getU16( stream );
    }
    if ( addr.conType == COMMS_CONN_RELAY ) {
        comms->r.myHostID = stream_getU8( stream );
        stringFromStreamHere( stream, comms->r.connName, 
                              sizeof(comms->r.connName) );
    }

#ifdef DEBUG
    comms->nUniqueBytes = stream_getU16( stream );
#endif
    comms->queueLen = stream_getU8( stream );

    nAddrRecs = stream_getU8( stream );
    prevsAddrNext = &comms->recs;
    for ( i = 0; i < nAddrRecs; ++i ) {
        AddressRecord* rec = (AddressRecord*)XP_MALLOC( mpool, sizeof(*rec));
        XP_MEMSET( rec, 0, sizeof(*rec) );

        addrFromStream( &rec->addr, stream );

        rec->nextMsgID = stream_getU16( stream );
        rec->lastMsgRcd = stream_getU16( stream );
        rec->channelNo = stream_getU16( stream );
        if ( rec->addr.conType == COMMS_CONN_RELAY ) {
            rec->r.hostID = stream_getU8( stream );
        }

#ifdef DEBUG
        rec->lastACK = stream_getU16( stream );
        rec->nUniqueBytes = stream_getU16( stream );
#endif

        *prevsAddrNext = rec;
        prevsAddrNext = &rec->next;
    }

    prevsQueueNext = &comms->msgQueueHead;
    for ( i = 0; i < comms->queueLen; ++i ) {
        MsgQueueElem* msg = (MsgQueueElem*)XP_MALLOC( mpool, sizeof(*msg) );

        msg->channelNo = stream_getU16( stream );
        msg->msgID = stream_getU32( stream );
#ifdef COMMS_HEARTBEAT
        msg->sendCount = 0;
#endif
        msg->len = stream_getU16( stream );
        msg->msg = (XP_U8*)XP_MALLOC( mpool, msg->len );
        stream_getBytes( stream, msg->msg, msg->len );
#ifdef COMMS_CHECKSUM
        msg->checksum = g_compute_checksum_for_data( G_CHECKSUM_MD5,
                                                     msg->msg, msg->len );
#endif
        msg->next = (MsgQueueElem*)NULL;
        *prevsQueueNext = comms->msgQueueTail = msg;
        comms->msgQueueTail = msg;
        prevsQueueNext = &msg->next;
    }

#ifdef DEBUG
    XP_ASSERT( stream_getU32( stream ) == cEND );
#endif

    return comms;
} /* comms_makeFromStream */

#ifdef COMMS_HEARTBEAT
static void
setDoHeartbeat( CommsCtxt* comms )
{
    CommsConnType conType = comms->addr.conType;
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
comms_start( CommsCtxt* comms )
{
    XP_ASSERT( !!comms );
    setDoHeartbeat( comms );
    sendConnect( comms, XP_FALSE );
} /* comms_start */

static void
sendConnect( CommsCtxt* comms, XP_Bool breakExisting )
{
    switch( comms->addr.conType ) {
#ifdef XWFEATURE_RELAY
    case COMMS_CONN_RELAY:
        if ( breakExisting
             || COMMS_RELAYSTATE_UNCONNECTED == comms->r.relayState ) {
            set_relay_state( comms, COMMS_RELAYSTATE_UNCONNECTED );
            if ( !relayConnect( comms ) ) {
                XP_LOGF( "%s: relayConnect failed", __func__ );
                set_reset_timer( comms );
            }
        }
        break;
#endif
#if defined XWFEATURE_BLUETOOTH || defined XWFEATURE_IP_DIRECT
    case COMMS_CONN_BT:
    case COMMS_CONN_IP_DIRECT:
        /* This will only work on host side when there's a single guest! */
        (void)send_via_bt_or_ip( comms, BTIPMSG_RESET, CHANNEL_NONE, NULL, 0 );
        (void)comms_resendAll( comms );
        break;
#endif
    default:
        break;
    }

    setHeartbeatTimer( comms );
} /* comms_start */

static void
addrToStream( XWStreamCtxt* stream, const CommsAddrRec* addrP )
{
    CommsAddrRec addr;
    XP_MEMCPY( &addr, addrP, sizeof(addr) ); /* does this really speed things
                                                or reduce code size? */
    stream_putU8( stream, addr.conType );

    switch( addr.conType ) {
    case COMMS_CONN_NONE:
        /* nothing to write */
        break;
    case COMMS_CONN_BT:
        stringToStream( stream, addr.u.bt.hostName );
        /* sizeof(.bits) below defeats ARM's padding. */
        stream_putBytes( stream, &addr.u.bt.btAddr.bits, 
                         sizeof(addr.u.bt.btAddr.bits) );
        break;
    case COMMS_CONN_IR:
        /* nothing to save */
        break;
    case COMMS_CONN_IP_DIRECT:
        stringToStream( stream, addr.u.ip.hostName_ip );
        stream_putU32( stream, addr.u.ip.ipAddr_ip );
        stream_putU16( stream, addr.u.ip.port_ip );
        break;
    case COMMS_CONN_RELAY:
        stringToStream( stream, addr.u.ip_relay.invite );
        stringToStream( stream, addr.u.ip_relay.hostName );
        stream_putU32( stream, addr.u.ip_relay.ipAddr );
        stream_putU16( stream, addr.u.ip_relay.port );
        stream_putBits( stream, 1, addr.u.ip_relay.seeksPublicRoom );
        stream_putBits( stream, 1, addr.u.ip_relay.advertiseRoom );
        break;
    case COMMS_CONN_SMS:
        stringToStream( stream, addr.u.sms.phone );
        stream_putU16( stream, addr.u.sms.port );
        break;
    default:
        XP_ASSERT(0);
        break;
    }
} /* addrToStream */

void
comms_writeToStream( const CommsCtxt* comms, XWStreamCtxt* stream )
{
    XP_U16 nAddrRecs;
    AddressRecord* rec;
    MsgQueueElem* msg;

    stream_putU8( stream, (XP_U8)comms->isServer );
    addrToStream( stream, &comms->addr );
    if ( comms->addr.conType == COMMS_CONN_RELAY ) {
        stream_putBits( stream, 4, comms->r.nPlayersHere );
        stream_putBits( stream, 4, comms->r.nPlayersTotal );
    }

    stream_putU32( stream, comms->connID );
    stream_putU16( stream, comms->nextChannelNo );
    stream_putU16( stream, comms->channelSeed );
    if ( comms->addr.conType == COMMS_CONN_RELAY ) {
        stream_putU8( stream, comms->r.myHostID );
        stringToStream( stream, comms->r.connName );
    }

#ifdef DEBUG
    stream_putU16( stream, comms->nUniqueBytes );
#endif

    XP_ASSERT( comms->queueLen <= 255 );
    stream_putU8( stream, (XP_U8)comms->queueLen );

    nAddrRecs = countAddrRecs(comms);
    stream_putU8( stream, (XP_U8)nAddrRecs );

    for ( rec = comms->recs; !!rec; rec = rec->next ) {

        CommsAddrRec* addr = &rec->addr;
        addrToStream( stream, addr );

        stream_putU16( stream, (XP_U16)rec->nextMsgID );
        stream_putU16( stream, (XP_U16)rec->lastMsgRcd );
        stream_putU16( stream, rec->channelNo );
        if ( rec->addr.conType == COMMS_CONN_RELAY ) {
            stream_putU8( stream, rec->r.hostID ); /* unneeded unless RELAY */
        }
#ifdef DEBUG
        stream_putU16( stream, rec->lastACK );
        stream_putU16( stream, rec->nUniqueBytes );
#endif
    }

    for ( msg = comms->msgQueueHead; !!msg; msg = msg->next ) {
        stream_putU16( stream, msg->channelNo );
        stream_putU32( stream, msg->msgID );

        stream_putU16( stream, msg->len );
        stream_putBytes( stream, msg->msg, msg->len );
    }

#ifdef DEBUG
    stream_putU32( stream, cEND );
#endif
} /* comms_writeToStream */

void
comms_getAddr( const CommsCtxt* comms, CommsAddrRec* addr )
{
    XP_ASSERT( !!comms );
    XP_MEMCPY( addr, &comms->addr, sizeof(*addr) );
} /* comms_getAddr */

void
comms_setAddr( CommsCtxt* comms, const CommsAddrRec* addr )
{
    XP_ASSERT( comms != NULL );
    util_addrChange( comms->util, &comms->addr, addr );
    XP_MEMCPY( &comms->addr, addr, sizeof(comms->addr) );

#ifdef COMMS_HEARTBEAT
    setDoHeartbeat( comms );
#endif
    sendConnect( comms, XP_TRUE );

} /* comms_setAddr */

#ifdef XWFEATURE_RELAY
XP_Bool
comms_getRelayID( const CommsCtxt* comms, XP_UCHAR* buf, XP_U16* lenp )
{
    XP_Bool success = comms->r.connName[0] != 0;
    if ( success ) {
        XP_U16 len = sizeof( comms->r.connName ) + 16;
        XP_UCHAR local[len];
        XP_SNPRINTF( local, sizeof(local), "%s/%d", 
                     comms->r.connName, comms->r.myHostID );
        XP_U16 strln = XP_STRLEN(local);
        success = *lenp >= strln;
        if ( success ) {
            *lenp = strln;
            XP_MEMCPY( buf, local, strln );
            XP_LOGF( "%s: keysize=%d", __func__, strln );
        }
    }
    LOG_RETURNF( "%d", success );
    return success;
}
#endif

void
comms_getInitialAddr( CommsAddrRec* addr
#ifdef XWFEATURE_RELAY
                      , const XP_UCHAR* relayName
                      , XP_U16 relayPort
#endif
                      )
{
#if defined  XWFEATURE_RELAY
    addr->conType = COMMS_CONN_RELAY; /* for temporary ease in debugging */
    addr->u.ip_relay.ipAddr = 0L; /* force 'em to set it */
    addr->u.ip_relay.port = relayPort;
    {
        const char* name = relayName;
        char* room = RELAY_ROOM_DEFAULT;
        XP_MEMCPY( addr->u.ip_relay.hostName, name, XP_STRLEN(name)+1 );
        XP_MEMCPY( addr->u.ip_relay.invite, room, XP_STRLEN(room)+1 );
    }
    addr->u.ip_relay.seeksPublicRoom = XP_FALSE;
    addr->u.ip_relay.advertiseRoom = XP_FALSE;
#elif defined PLATFORM_PALM
    /* default values; default is still IR where there's a choice, at least on
       Palm... */
    addr->conType = COMMS_CONN_IR;
#else
    addr->conType = COMMS_CONN_SMS;
#endif
} /* comms_getInitialAddr */

XP_Bool
comms_checkAddr( DeviceRole role, const CommsAddrRec* addr, XW_UtilCtxt* util )
{
    XP_Bool ok = XP_TRUE;
    /* make sure the user's given us enough information to make a connection */
    if ( role == SERVER_ISCLIENT ) {
        if ( addr->conType == COMMS_CONN_BT ) {
            XP_U32 empty = 0L;      /* check four bytes to save some code */
            if ( !XP_MEMCMP( &empty, &addr->u.bt.btAddr, sizeof(empty) ) ) {
                ok = XP_FALSE;
                if ( !!util ) {
                    util_userError( util, STR_NEED_BT_HOST_ADDR );
                }
            }
        }
    }
    return ok;
} /* comms_checkAddr */

CommsConnType 
comms_getConType( const CommsCtxt* comms )
{
    XP_ASSERT( !!comms );       /* or: return COMMS_CONN_NONE */
    return comms->addr.conType;
} /* comms_getConType */

XP_Bool
comms_getIsServer( const CommsCtxt* comms )
{
    XP_ASSERT( !!comms );
    return comms->isServer;
}

static MsgQueueElem*
makeElemWithID( CommsCtxt* comms, MsgID msgID, AddressRecord* rec, 
                XP_PlayerAddr channelNo, XWStreamCtxt* stream )
{
    XP_U16 headerLen;
    XP_U16 streamSize = NULL == stream? 0 : stream_getSize( stream );
    MsgID lastMsgRcd = (!!rec)? rec->lastMsgRcd : 0;
    MsgQueueElem* newMsgElem;
    XWStreamCtxt* msgStream;

#ifdef DEBUG
    if ( !!rec ) {
        rec->nUniqueBytes += streamSize;
    } else {
        comms->nUniqueBytes += streamSize;
    }
#endif

    newMsgElem = (MsgQueueElem*)XP_MALLOC( comms->mpool, 
                                           sizeof( *newMsgElem ) );
    newMsgElem->channelNo = channelNo;
    newMsgElem->msgID = msgID;
#ifdef COMMS_HEARTBEAT
    newMsgElem->sendCount = 0;
#endif

    msgStream = mem_stream_make( MPPARM(comms->mpool) 
                                 util_getVTManager(comms->util),
                                 NULL, 0, 
                                 (MemStreamCloseCallback)NULL );
    stream_open( msgStream );
    XP_LOGF( "%s: putting connID %lx", __func__, comms->connID );
    stream_putU32( msgStream, comms->connID );

    stream_putU16( msgStream, channelNo );
    stream_putU32( msgStream, msgID );
    XP_LOGF( "put lastMsgRcd: %ld", lastMsgRcd );
    stream_putU32( msgStream, lastMsgRcd );

    headerLen = stream_getSize( msgStream );
    newMsgElem->len = streamSize + headerLen;
    newMsgElem->msg = (XP_U8*)XP_MALLOC( comms->mpool, newMsgElem->len );

    stream_getBytes( msgStream, newMsgElem->msg, headerLen );
    stream_destroy( msgStream );
    
    if ( 0 < streamSize ) {
        stream_getBytes( stream, newMsgElem->msg + headerLen, streamSize );
    }

#ifdef COMMS_CHECKSUM
    newMsgElem->checksum = g_compute_checksum_for_data( G_CHECKSUM_MD5,
                                                        newMsgElem->msg, 
                                                        newMsgElem->len );
#endif
    return newMsgElem;
} /* makeElemWithID */

XP_U16
comms_getChannelSeed( CommsCtxt* comms )
{
    while ( comms->channelSeed == 0 ) {
        comms->channelSeed = XP_RANDOM();
        XP_LOGF( "%s: channelSeed: %.4X", __func__, comms->channelSeed );
    }
    return comms->channelSeed;
}

/* Send a message using the sequentially next MsgID.  Save the message so
 * resend can work. */
XP_S16
comms_send( CommsCtxt* comms, XWStreamCtxt* stream )
{
    XP_PlayerAddr channelNo = stream_getAddress( stream );
    XP_LOGF( "%s: channelNo=%x", __func__, channelNo );
    AddressRecord* rec = getRecordFor( comms, NULL, channelNo, XP_FALSE );
    MsgID msgID = (!!rec)? ++rec->nextMsgID : 0;
    MsgQueueElem* elem;
    XP_S16 result = -1;

    if ( 0 == channelNo ) {
        channelNo = comms_getChannelSeed(comms) & ~CHANNEL_MASK;
    }

    XP_DEBUGF( "%s: assigning msgID=" XP_LD " on chnl %x", __func__, 
               msgID, channelNo );

    elem = makeElemWithID( comms, msgID, rec, channelNo, stream );
    if ( NULL != elem ) {
        addToQueue( comms, elem );
        result = sendMsg( comms, elem );
    }
    return result;
} /* comms_send */

/* Add new message to the end of the list.  The list needs to be kept in order
 * by ascending msgIDs within each channel since if there's a resend that's
 * the order in which they need to be sent.
 */
static void
addToQueue( CommsCtxt* comms, MsgQueueElem* newMsgElem )
{
    newMsgElem->next = (MsgQueueElem*)NULL;
    if ( !comms->msgQueueHead ) {
        comms->msgQueueHead = comms->msgQueueTail = newMsgElem;
        XP_ASSERT( comms->queueLen == 0 );
    } else {
        XP_ASSERT( !!comms->msgQueueTail );
        comms->msgQueueTail->next = newMsgElem;
        comms->msgQueueTail = newMsgElem;

        XP_ASSERT( comms->queueLen > 0 );
    }
    ++comms->queueLen;
    XP_STATUSF( "addToQueue: queueLen now %d", comms->queueLen );
} /* addToQueue */

#ifdef DEBUG
static void
printQueue( const CommsCtxt* comms )
{
    MsgQueueElem* elem;
    short i;

    for ( elem = comms->msgQueueHead, i = 0; i < comms->queueLen; 
          elem = elem->next, ++i ) {
        XP_STATUSF( "\t%d: channel: %x; msgID=" XP_LD 
#ifdef COMMS_CHECKSUM
                    "; check=%s"
#endif
                    ,i+1, elem->channelNo, elem->msgID
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
    XP_ASSERT( count < 10 );
}
#endif

static void
freeElem( const CommsCtxt* XP_UNUSED_DBG(comms), MsgQueueElem* elem )
{
    XP_FREE( comms->mpool, elem->msg );
#ifdef COMMS_CHECKSUM
    g_free( elem->checksum );
#endif
    XP_FREE( comms->mpool, elem );
}

/* We've received on some channel a message with a certain ID.  This means
 * that all messages sent on that channel with lower IDs have been received
 * and can be removed from our queue.  BUT: if this ID is higher than any
 * we've sent, don't remove.  We may be starting a new game but have a server
 * that's still on the old one.
 */
static void
removeFromQueue( CommsCtxt* comms, XP_PlayerAddr channelNo, MsgID msgID )
{
    XP_STATUSF( "%s: remove msgs <= " XP_LD " for channel %x (queueLen: %d)",
                __func__, msgID, channelNo, comms->queueLen );

    if ( (channelNo == 0) || !!getRecordFor( comms, NULL, channelNo, 
                                             XP_FALSE ) ) {

        MsgQueueElem* elem = comms->msgQueueHead;
        MsgQueueElem* next;

        /* empty the queue so we can add all back again */
        comms->msgQueueHead = comms->msgQueueTail = NULL;
        comms->queueLen = 0;

        for ( ; !!elem; elem = next ) {
            XP_Bool knownGood = XP_FALSE;
            next = elem->next;

            /* remove the 0-channel message if we've established a channel
               number.  Only clients should have any 0-channel messages in the
               queue, and receiving something from the server is an implicit
               ACK -- IFF it isn't left over from the last game. */

            if ( ((CHANNEL_MASK & elem->channelNo) == 0) && (channelNo!= 0) ) {
                XP_ASSERT( !comms->isServer );
                XP_ASSERT( elem->msgID == 0 );
            } else if ( elem->channelNo != channelNo ) {
                knownGood = XP_TRUE;
            }

            if ( !knownGood && (elem->msgID <= msgID) ) {
                freeElem( comms, elem );
            } else {
                addToQueue( comms, elem );
            }
        }
    }

    XP_STATUSF( "%s: queueLen now %d", __func__, comms->queueLen );

#ifdef DEBUG
    assertQueueOk( comms );
    printQueue( comms );
#endif
} /* removeFromQueue */

static XP_S16
sendMsg( CommsCtxt* comms, MsgQueueElem* elem )
{
    XP_S16 result = -1;
    XP_PlayerAddr channelNo;
#if defined XWFEATURE_RELAY || defined XWFEATURE_BLUETOOTH
    CommsConnType conType = comms_getConType( comms );
#endif

    channelNo = elem->channelNo;

    if ( 0 ) {
#ifdef XWFEATURE_RELAY
    } else if ( conType == COMMS_CONN_RELAY ) {
        if ( comms->r.relayState >= COMMS_RELAYSTATE_CONNECTED ) {
            XWHostID destID = getDestID( comms, channelNo );
            result = send_via_relay( comms, XWRELAY_MSG_TORELAY, destID, 
                                     elem->msg, elem->len );
        } else {
            XP_LOGF( "%s: skipping message: not connected", __func__ );
        }
#endif
#if defined XWFEATURE_BLUETOOTH || defined XWFEATURE_IP_DIRECT
    } else if ( conType == COMMS_CONN_BT || conType == COMMS_CONN_IP_DIRECT ) {
        result = send_via_bt_or_ip( comms, BTIPMSG_DATA, channelNo, 
                                    elem->msg, elem->len );
#ifdef COMMS_HEARTBEAT
        setHeartbeatTimer( comms );
#endif
#endif
    } else {
        const CommsAddrRec* addr;
        (void)channelToAddress( comms, channelNo, &addr );

        XP_ASSERT( !!comms->procs.send );
        result = (*comms->procs.send)( elem->msg, elem->len, addr,
                                       comms->procs.closure );
    }
    
    if ( result == elem->len ) {
        ++elem->sendCount;
        XP_LOGF( "%s: sendCount now %d", __func__, elem->sendCount );
    }

    return result;
} /* sendMsg */

static void
send_ack( CommsCtxt* comms )
{
    LOG_FUNC();
    send_via_relay( comms, XWRELAY_ACK, comms->r.myHostID, NULL, 0 );
}

XP_S16
comms_resendAll( CommsCtxt* comms )
{
    MsgQueueElem* msg;
    XP_S16 result = 0;

    XP_ASSERT( !!comms );

    for ( msg = comms->msgQueueHead; !!msg; msg = msg->next ) {
        XP_S16 oneResult = sendMsg( comms, msg );
        if ( result == 0 && oneResult != 0 ) {
            result = oneResult;
        }
        XP_STATUSF( "%s: msgID=" XP_LD "; rslt=%d", __func__,
                    msg->msgID, oneResult );
    }

    return result;
} /* comms_resend */

#ifdef XWFEATURE_RELAY
# ifdef DEBUG
# define CASESTR(s) case s: return #s
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
        CASESTR( XWRELAY_MSG_TORELAY );
        CASESTR( XWRELAY_MSG_STATUS );
    default: 
        XP_LOGF( "%s: unknown cmd: %d", __func__, cmd );
        XP_ASSERT( 0 );
        return "<unknown>";
    }
}
# endif 

static void
got_connect_cmd( CommsCtxt* comms, XWStreamCtxt* stream, 
                 XP_Bool reconnected )
{
    XP_U16 nHere, nSought;
    XP_Bool isServer;

    set_relay_state( comms, reconnected ? COMMS_RELAYSTATE_RECONNECTED
                     : COMMS_RELAYSTATE_CONNECTED );
    comms->r.myHostID = stream_getU8( stream );
    XP_LOGF( "%s: set hostid: %x", __func__, comms->r.myHostID );
    isServer = HOST_ID_SERVER == comms->r.myHostID;
    if ( isServer != comms->isServer ) {
        comms->isServer = isServer;
        util_setIsServer( comms->util, comms->isServer );

        reset_internal( comms, isServer, comms->r.nPlayersHere, 
                        comms->r.nPlayersTotal, XP_FALSE );
    }

    comms->r.cookieID = stream_getU16( stream );
    comms->r.heartbeat = stream_getU16( stream );
    nSought = (XP_U16)stream_getU8( stream );
    nHere = (XP_U16)stream_getU8( stream );
    if ( nSought == nHere ) {
        set_relay_state( comms, COMMS_RELAYSTATE_ALLCONNECTED );
    }

#ifdef DEBUG
    {
        XP_UCHAR connName[MAX_CONNNAME_LEN+1];
        stringFromStreamHere( stream, connName, sizeof(connName) );
        XP_ASSERT( comms->r.connName[0] == '\0' 
                   || 0 == XP_STRCMP( comms->r.connName, connName ) );
        XP_MEMCPY( comms->r.connName, connName, sizeof(comms->r.connName) );
        XP_LOGF( "%s: connName: \"%s\"", __func__, connName );
    }
#else
    stringFromStreamHere( stream, comms->r.connName, 
                          sizeof(comms->r.connName) );
#endif

    if ( ! reconnected ) {
        /* This may belong as an alert to user so knows has connected. */
        (*comms->procs.rconnd)( comms->procs.closure, 
                                comms->addr.u.ip_relay.invite,
                                comms->r.myHostID, XP_FALSE, nSought - nHere );
    }
    XP_LOGF( "%s: have %d of %d players", __func__, nHere, nSought );
    setHeartbeatTimer( comms );
} /* got_connect_cmd */

static XP_Bool
relayPreProcess( CommsCtxt* comms, XWStreamCtxt* stream, XWHostID* senderID )
{
    XP_Bool consumed = XP_TRUE;
    XWHostID destID, srcID;
    CookieID cookieID;
    XP_U8 relayErr;

    /* nothing for us to do here if not using relay */
    XWRELAY_Cmd cmd = stream_getU8( stream );
    XP_LOGF( "%s(%s)", __func__, relayCmdToStr( cmd ) );
    switch( cmd ) {

    case XWRELAY_CONNECT_RESP:
        got_connect_cmd( comms, stream, XP_FALSE );
        send_ack( comms );
        break;
    case XWRELAY_RECONNECT_RESP:
        got_connect_cmd( comms, stream, XP_TRUE );
        comms_resendAll( comms );
        break;

    case XWRELAY_ALLHERE:
        srcID = (XWHostID)stream_getU8( stream );
        XP_ASSERT( comms->r.myHostID == HOST_ID_NONE
                   || comms->r.myHostID == srcID );
        comms->r.myHostID = srcID;
        XP_LOGF( "%s: set hostid: %x", __func__, comms->r.myHostID );

#ifdef DEBUG
        {
            XP_UCHAR connName[MAX_CONNNAME_LEN+1];
            stringFromStreamHere( stream, connName, sizeof(connName) );
            XP_ASSERT( comms->r.connName[0] == '\0' 
                       || 0 == XP_STRCMP( comms->r.connName, connName ) );
            XP_MEMCPY( comms->r.connName, connName, sizeof(comms->r.connName) );
            XP_LOGF( "%s: connName: \"%s\"", __func__, connName );
        }
#else
        stringFromStreamHere( stream, comms->r.connName, 
                              sizeof(comms->r.connName) );
#endif

        /* We're [re-]connected now.  Send any pending messages.  This may
           need to be done later since we're inside the platform's socket read
           proc now.  But don't resend if we were previously REconnected, as
           we'll have sent then.  -- I don't see any send on RECONNECTED, so
           removing the test for now to fix recon problems on android. */
        /* if ( COMMS_RELAYSTATE_RECONNECTED != comms->r.relayState ) { */
        comms_resendAll( comms );
        /* } */
        if ( XWRELAY_ALLHERE == cmd ) { /* initial connect? */
            (*comms->procs.rconnd)( comms->procs.closure, 
                                    comms->addr.u.ip_relay.invite,
                                    comms->r.myHostID, XP_TRUE, 0 );
        }
        set_relay_state( comms, COMMS_RELAYSTATE_ALLCONNECTED );
        break;
    case XWRELAY_MSG_FROMRELAY:
        cookieID = stream_getU16( stream );
        srcID = stream_getU8( stream );
        destID = stream_getU8( stream );
        XP_LOGF( "cookieID: %d; srcID: %x; destID: %x",
                 cookieID, srcID, destID );
        /* If these values don't check out, drop it */

        /* When a message comes in via proxy (rather than a connection) state
           may not be as expected.  Just commenting these out is probably the
           wrong fix.  Maybe instead the constructor takes a flag that means
           "assume you're connected"  Revisit this. */
        /* XP_ASSERT( COMMS_RELAYSTATE_ALLCONNECTED == comms->r.relayState */
        /*            || COMMS_RELAYSTATE_CONNECTED == comms->r.relayState */
        /*            || COMMS_RELAYSTATE_RECONNECTED == comms->r.relayState ); */

        if ( destID == comms->r.myHostID ) { /* When would this not happen? */
            consumed = XP_FALSE;
        } else if ( cookieID == comms->r.cookieID ) {
            XP_LOGF( "%s: keeping message though hostID not what expected (%d vs %d)",
                     __func__, destID, comms->r.myHostID );
            consumed = XP_FALSE;
        }

        if ( consumed ) {
            XP_LOGF( "%s: rejecting data message", __func__ );
        } else {
            *senderID = srcID;
        }
        break;

    case XWRELAY_DISCONNECT_OTHER:
        relayErr = stream_getU8( stream );
        srcID = stream_getU8( stream );
        XP_LOGF( "%s: host id %x disconnected", __func__, srcID );
        /* if we don't have connName then RECONNECTED is the wrong state to
           change to. */
        XP_ASSERT( 0 != comms->r.connName[0] );
        set_relay_state( comms, COMMS_RELAYSTATE_RECONNECTED );
        /* we will eventually want to tell the user which player's gone */
        util_userError( comms->util, ERR_RELAY_BASE + relayErr );
        break;

    case XWRELAY_DISCONNECT_YOU:                /* Close socket for this? */
        relayErr = stream_getU8( stream );
        set_relay_state( comms, COMMS_RELAYSTATE_UNCONNECTED );
        util_userError( comms->util, ERR_RELAY_BASE + relayErr );
        break;

    case XWRELAY_MSG_STATUS:
        relayErr = stream_getU8( stream );
        (*comms->procs.rerror)( comms->procs.closure, relayErr );
        break;

    case XWRELAY_CONNECTDENIED: /* socket will get closed by relay */
        relayErr = stream_getU8( stream );
        set_relay_state( comms, COMMS_RELAYSTATE_DENIED );
        util_userError( comms->util, ERR_RELAY_BASE + relayErr );
        /* requires action, not just notification */
        (*comms->procs.rerror)( comms->procs.closure, relayErr );
        break;

        /* fallthru */
    default:
        XP_LOGF( "%s: dropping relay msg with cmd %d", __func__, (XP_U16)cmd );
    }
    
    return consumed;
} /* relayPreProcess */
#endif

#ifdef COMMS_HEARTBEAT
static void
noteHBReceived( CommsCtxt* comms/* , const CommsAddrRec* addr */ )
{
    comms->lastMsgRcvdTime = util_getCurSeconds( comms->util );
    setHeartbeatTimer( comms );
}
#else
# define noteHBReceived(a)
#endif

#if defined XWFEATURE_BLUETOOTH || defined XWFEATURE_IP_DIRECT
static XP_Bool
btIpPreProcess( CommsCtxt* comms, XWStreamCtxt* stream )
{
    BTIPMsgType typ = (BTIPMsgType)stream_getU8( stream );
    XP_Bool consumed = typ != BTIPMSG_DATA;

    if ( consumed ) {
        /* This  is all there is so far */
        if ( typ == BTIPMSG_RESET ) {
            (void)comms_resendAll( comms );
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
preProcess( CommsCtxt* comms, XWStreamCtxt* stream, 
            XP_Bool* XP_UNUSED_RELAY(usingRelay), 
            XWHostID* XP_UNUSED_RELAY(senderID) )
{
    XP_Bool consumed = XP_FALSE;
    switch ( comms->addr.conType ) {
#ifdef XWFEATURE_RELAY
    /* relayPreProcess returns true if consumes the message.  May just eat the
       header and leave a regular message to be processed below. */
    case COMMS_CONN_RELAY:
        consumed = relayPreProcess( comms, stream, senderID );
        if ( !consumed ) {
            *usingRelay = comms->addr.conType == COMMS_CONN_RELAY;
        }
        break;
#endif
#if defined XWFEATURE_BLUETOOTH || defined XWFEATURE_IP_DIRECT
    case COMMS_CONN_BT:
    case COMMS_CONN_IP_DIRECT:
        consumed = btIpPreProcess( comms, stream );
        break;
#endif
    default:
        break;
    }
    LOG_RETURNF( "%d", consumed );
    return consumed;
} /* preProcess */

static AddressRecord* 
getRecordFor( CommsCtxt* comms, const CommsAddrRec* addr, 
              XP_PlayerAddr channelNo, XP_Bool maskChannel )
{
    CommsConnType conType;
    AddressRecord* rec;
    XP_Bool matched = XP_FALSE;
    XP_U16 mask = maskChannel? ~CHANNEL_MASK : ~0;

    /* Use addr if we have it.  Otherwise use channelNo if non-0 */
    conType = !!addr? addr->conType : COMMS_CONN_NONE;

    for ( rec = comms->recs; !!rec; rec = rec->next ) {
        XP_ASSERT( !addr || (conType == rec->addr.conType) );
        switch( conType ) {
        case COMMS_CONN_RELAY:
            XP_ASSERT(0);       /* is this being used? */
            if ( (addr->u.ip_relay.ipAddr == rec->addr.u.ip_relay.ipAddr)
                 && (addr->u.ip_relay.port == rec->addr.u.ip_relay.port ) ) {
                matched = XP_TRUE;
            }
            break;
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
            break;
        case COMMS_CONN_SMS:
            if ( ( 0 == XP_MEMCMP( &addr->u.sms.phone, &rec->addr.u.sms.phone,
                                   sizeof(addr->u.sms.phone) ) )
                 && addr->u.sms.port == rec->addr.u.sms.port ) {
                matched = XP_TRUE;
            }
            break;
        case COMMS_CONN_NONE:
            matched = channelNo == (rec->channelNo & mask);
            break;
        default:
            XP_ASSERT(0);
            break;
        }
        if ( matched ) {
            break;
        }
    }
    return rec;
} /* getRecordFor */

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
validateInitialMessage( CommsCtxt* comms, 
                        XP_Bool XP_UNUSED_HEARTBEAT(hasPayload),
                        const CommsAddrRec* addr, XWHostID senderID, 
                        XP_PlayerAddr* channelNo )
{
    AddressRecord* rec = NULL;
    LOG_FUNC();
    if ( 0 ) {
#ifdef COMMS_HEARTBEAT
    } else if ( comms->doHeartbeat ) {
        XP_Bool addRec = XP_FALSE;
        /* This (with mask) is untested!!! */
        rec = getRecordFor( comms, addr, *channelNo, XP_TRUE );

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
                XP_LOGF( "%s: looking at channelNo: %x", __func__, *channelNo );
                XP_ASSERT( (*channelNo && CHANNEL_MASK) == 0 );
                *channelNo |= ++comms->nextChannelNo;
                XP_ASSERT( comms->nextChannelNo <= CHANNEL_MASK );
            }
            rec = rememberChannelAddress( comms, *channelNo, senderID, addr );
            if ( hasPayload ) {
                rec->initialSeen = XP_TRUE;
            } else {
                rec = NULL;
            }
        }
#endif
    } else {
        XP_LOGF( "%s: looking at channelNo: %x", __func__, *channelNo );
        rec = getRecordFor( comms, addr, *channelNo, XP_TRUE );
        if ( !!rec ) {
            /* reject: we've already seen init message on channel */
            rec = NULL;
        } else {
            if ( comms->isServer ) {
                XP_ASSERT( (*channelNo & CHANNEL_MASK) == 0 );
                *channelNo |= ++comms->nextChannelNo;
                XP_ASSERT( comms->nextChannelNo <= CHANNEL_MASK );
            }
            rec = rememberChannelAddress( comms, *channelNo, senderID, addr );
        }
    }
    LOG_RETURNF( XP_P, rec );
    return rec;
} /* validateInitialMessage */

/* Messages with established connIDs are valid only if they have the msgID
 * that's expected on that channel.  Their addresses need to match what we
 * have for that channel, and in fact we'll overwrite what we have in case a
 * reset has changed the address.  The danger is that somebody might sneak in
 * with a forged message, but this isn't internet banking.
 */
static AddressRecord* 
validateChannelMessage( CommsCtxt* comms, const CommsAddrRec* addr,
                        XP_PlayerAddr channelNo, MsgID msgID, MsgID lastMsgRcd )

{
    AddressRecord* rec;
    LOG_FUNC();

    rec = getRecordFor( comms, NULL, channelNo, XP_FALSE );
    if ( !!rec ) {
        removeFromQueue( comms, channelNo, lastMsgRcd );
        if ( msgID == rec->lastMsgRcd + 1 ) {
            updateChannelAddress( rec, addr );
#ifdef DEBUG
            rec->lastACK = (XP_U16)lastMsgRcd;
#endif
        } else {
            XP_LOGF( "%s: expected %ld, got %ld", __func__, 
                     rec->lastMsgRcd + 1, msgID );
            rec = NULL;
        }
    } else {
        XP_LOGF( "%s: no rec for channelNo %x", __func__, channelNo );
    }

    LOG_RETURNF( XP_P, rec );
    return rec;
} /* validateChannelMessage */

XP_Bool
comms_checkIncomingStream( CommsCtxt* comms, XWStreamCtxt* stream, 
                           const CommsAddrRec* retAddr )
{
    XP_Bool messageValid = XP_FALSE;
    XWHostID senderID = 0;      /* unset; default for non-relay cases */
    XP_Bool usingRelay = XP_FALSE;

    XP_ASSERT( retAddr == NULL || comms->addr.conType == retAddr->conType );

    if ( !preProcess( comms, stream, &usingRelay, &senderID ) ) {
        XP_U32 connID;
        XP_PlayerAddr channelNo;
        MsgID msgID;
        MsgID lastMsgRcd;

        /* reject too-small message */
        if ( stream_getSize( stream ) >=
             (sizeof(connID) + sizeof(channelNo) 
              + sizeof(msgID) + sizeof(lastMsgRcd)) ) {
            XP_U16 payloadSize;
            AddressRecord* rec = NULL;

            connID = stream_getU32( stream );
            XP_STATUSF( "%s: read connID (gameID) of %lx", __func__, connID );
            channelNo = stream_getU16( stream );
            msgID = stream_getU32( stream );
            lastMsgRcd = stream_getU32( stream );
            XP_DEBUGF( "rcd on channelNo %x: msgID=%ld,lastMsgRcd=%ld ", 
                       channelNo, msgID, lastMsgRcd );

            payloadSize = stream_getSize( stream ) > 0; /* anything left? */

            if ( connID == CONN_ID_NONE ) {
                /* special case: initial message from client or server */
                rec = validateInitialMessage( comms, payloadSize > 0, retAddr, 
                                              senderID, &channelNo );
            } else if ( comms->connID == connID ) {
                rec = validateChannelMessage( comms, retAddr, channelNo, msgID,
                                              lastMsgRcd );
            }

            messageValid = (NULL != rec)
                && (0 == rec->lastMsgRcd || rec->lastMsgRcd <= msgID);
            if ( messageValid ) {
                rec->lastMsgRcd = msgID;
                XP_LOGF( "%s: set channel %x's lastMsgRcd to " XP_LD,
                         __func__, channelNo, msgID );
                stream_setAddress( stream, channelNo );
                messageValid = payloadSize > 0;
            }
        } else {
            XP_LOGF( "%s: message too small", __func__ );
        }
    }

    /* Call after we've had a chance to create rec for addr */
    noteHBReceived( comms/* , addr */ );

    LOG_RETURNF( "%s", messageValid?"valid":"invalid" );
    return messageValid;
} /* comms_checkIncomingStream */

XP_Bool
comms_checkComplete( const CommsAddrRec* addr )
{
    XP_Bool result;

    switch ( addr->conType ) {
    case COMMS_CONN_RELAY:
        result = !!addr->u.ip_relay.invite[0]
            && !!addr->u.ip_relay.hostName[0]
            && !!addr->u.ip_relay.port > 0;
        break;
    default:
        result = XP_TRUE;
    }

    return result;
}

XP_Bool
comms_canChat( const CommsCtxt* const comms )
{
    XP_Bool canChat = comms_isConnected( comms )
        && comms->connID != 0;
    return canChat;
}

XP_Bool
comms_isConnected( const CommsCtxt* const comms )
{
    XP_Bool result = XP_FALSE;
    switch ( comms->addr.conType ) {
    case COMMS_CONN_RELAY:
        result = 0 != comms->r.connName[0];
        break;
    default:
        break;
    }
    return result;
}

#ifdef COMMS_HEARTBEAT
static void
sendEmptyMsg( CommsCtxt* comms, AddressRecord* rec )
{
    MsgQueueElem* elem = makeElemWithID( comms, 
                                         0 /*rec? rec->lastMsgRcd : 0*/,
                                         rec, 
                                         rec? rec->channelNo : 0, NULL );
    sendMsg( comms, elem );
    freeElem( comms, elem );
} /* sendEmptyMsg */

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
            XP_U32 now = util_getCurSeconds( comms->util );
            XP_U32 tooLongAgo = now - (HB_INTERVAL * 2);
            if ( comms->lastMsgRcvdTime < tooLongAgo ) {
                XP_LOGF( "%s: calling reset proc; last was %ld secs too long "
                         "ago", __func__, tooLongAgo - comms->lastMsgRcvdTime );
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

#if defined XWFEATURE_RELAY || defined COMMS_HEARTBEAT
static XP_Bool
p_comms_timerFired( void* closure, XWTimerReason XP_UNUSED_DBG(why) )
{
    CommsCtxt* comms = (CommsCtxt*)closure;
    XP_ASSERT( why == TIMER_COMMS );
    LOG_FUNC();
    comms->hbTimerPending = XP_FALSE;
    if (0 ) {
#if defined XWFEATURE_RELAY && defined RELAY_HEARTBEAT
    } else  if ( (comms->addr.conType == COMMS_CONN_RELAY ) 
         && (comms->r.heartbeat != HEARTBEAT_NONE) ) {
        send_via_relay( comms, XWRELAY_HEARTBEAT, HOST_ID_NONE, NULL, 0 );
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
    LOG_FUNC();
    XP_ASSERT( !!comms );

    if ( comms->hbTimerPending ) {
        XP_LOGF( "%s: skipping b/c hbTimerPending", __func__ );
    } else if ( comms->reconTimerPending ) {
        XP_LOGF( "%s: skipping b/c reconTimerPending", __func__ );
    } else {
        XP_U16 when = 0;
#ifdef XWFEATURE_RELAY
        if ( comms->addr.conType == COMMS_CONN_RELAY ) {
            when = comms->r.heartbeat;
        }
#endif
#ifdef COMMS_HEARTBEAT
        if ( comms->doHeartbeat ) {
            XP_LOGF( "%s: calling util_setTimer", __func__ );
            when = HB_INTERVAL;
        }
#endif
        if ( when != 0 ) {
            util_setTimer( comms->util, TIMER_COMMS, when,
                           p_comms_timerFired, comms );
            comms->hbTimerPending = XP_TRUE;
        }
    }
} /* setHeartbeatTimer */
#endif

#ifdef DEBUG
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
    default:
        XP_ASSERT(0);
    }
    return "<unknown>";
} /* ConnType2Str */

void
comms_getStats( CommsCtxt* comms, XWStreamCtxt* stream )
{
    XP_UCHAR buf[100];
    AddressRecord* rec;
    MsgQueueElem* elem;
    XP_U32 now;

    XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf), 
                 (XP_UCHAR*)"msg queue len: %d\n", comms->queueLen );
    stream_catString( stream, buf );

    for ( elem = comms->msgQueueHead; !!elem; elem = elem->next ) {
        XP_SNPRINTF( buf, sizeof(buf), 
                     " - channelNo=%x; msgID=" XP_LD "; len=%d\n", 
                     elem->channelNo, elem->msgID, elem->len );
        stream_catString( stream, buf );
    }

    XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf), 
                 (XP_UCHAR*)"channel-less bytes sent: %d\n", 
                 comms->nUniqueBytes );
    stream_catString( stream, buf );

    now = util_getCurSeconds( comms->util );
    for ( rec = comms->recs; !!rec; rec = rec->next ) {
        XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf), 
                     (XP_UCHAR*)"  Stats for channel: %x\n", 
                     rec->channelNo );
        stream_catString( stream, buf );

        XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf), 
                     (XP_UCHAR*)"Last msg sent: " XP_LD "\n", 
                     rec->nextMsgID );
        stream_catString( stream, buf );

        XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf), 
                     (XP_UCHAR*)"Unique bytes sent: %d\n", 
                     rec->nUniqueBytes );
        stream_catString( stream, buf );

        XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf), 
                     (XP_UCHAR*)"Last message received: %ld\n", 
                     rec->lastMsgRcd );
        stream_catString( stream, buf );
        XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf), 
                     (XP_UCHAR*)"Last message acknowledged: %d\n", 
                     rec->lastACK );
        stream_catString( stream, buf );
    }
} /* comms_getStats */
#endif

static AddressRecord*
rememberChannelAddress( CommsCtxt* comms, XP_PlayerAddr channelNo, 
                        XWHostID hostID, const CommsAddrRec* addr )
{
    AddressRecord* recs = NULL;
    recs = getRecordFor( comms, NULL, channelNo, XP_FALSE );
    if ( !recs ) {
        /* not found; add a new entry */
        recs = (AddressRecord*)XP_MALLOC( comms->mpool, sizeof(*recs) );
        XP_MEMSET( recs, 0, sizeof(*recs) );

        recs->channelNo = channelNo;
        recs->r.hostID = hostID;
        recs->next = comms->recs;
        comms->recs = recs;
    }

    /* overwrite existing address with new one.  I assume that's the right
       move. */
    if ( !!recs ) {
        if ( !!addr ) {
            XP_MEMCPY( &recs->addr, addr, sizeof(recs->addr) );
            XP_ASSERT( recs->r.hostID == hostID );
        } else {
            XP_MEMSET( &recs->addr, 0, sizeof(recs->addr) );
            recs->addr.conType = comms->addr.conType;
        }
    }
    return recs;
} /* rememberChannelAddress */

static void
updateChannelAddress( AddressRecord* rec, const CommsAddrRec* addr )
{
    XP_ASSERT( !!rec );
    if ( !!addr ) {
        XP_MEMCPY( &rec->addr, addr, sizeof(rec->addr) );
    }
} /* updateChannelAddress */

static XP_Bool
channelToAddress( CommsCtxt* comms, XP_PlayerAddr channelNo, 
                  const CommsAddrRec** addr )
{
    AddressRecord* recs = getRecordFor( comms, NULL, channelNo, XP_FALSE );
    XP_Bool found = !!recs;
    *addr = found? &recs->addr : NULL;
    return found;
} /* channelToAddress */

static XP_U16
countAddrRecs( const CommsCtxt* comms )
{
    short count = 0;
    AddressRecord* recs;
    for ( recs = comms->recs; !!recs; recs = recs->next ) {
        ++count;
    } 
    return count;
} /* countAddrRecs */

#ifdef XWFEATURE_RELAY
static XWHostID
getDestID( CommsCtxt* comms, XP_PlayerAddr channelNo )
{
    XWHostID id = HOST_ID_NONE;
    if ( (channelNo & CHANNEL_MASK) == CHANNEL_NONE ) {
        id = HOST_ID_SERVER;
    } else {
        AddressRecord* recs;
        for ( recs = comms->recs; !!recs; recs = recs->next ) {
            if ( recs->channelNo == channelNo ) {
                id = recs->r.hostID;
            }
        }
    }
    XP_LOGF( "%s(%x) => %x", __func__, channelNo, id );
    return id;
} /* getDestID */

static XP_Bool
send_via_relay( CommsCtxt* comms, XWRELAY_Cmd cmd, XWHostID destID, 
                void* data, int dlen )
{
    XP_Bool success = XP_FALSE;
    XP_U16 len = 0;
    CommsAddrRec addr;
    XWStreamCtxt* tmpStream;
    XP_U8* buf;

    comms_getAddr( comms, &addr );
    tmpStream = mem_stream_make( MPPARM(comms->mpool) 
                                 util_getVTManager(comms->util),
                                 NULL, 0, 
                                 (MemStreamCloseCallback)NULL );
    if ( tmpStream != NULL ) {
        stream_open( tmpStream );
        stream_putU8( tmpStream, cmd );

        switch ( cmd ) {
        case XWRELAY_MSG_TORELAY:
            XP_ASSERT( 0 != comms->r.cookieID );
            stream_putU16( tmpStream, comms->r.cookieID );
            stream_putU8( tmpStream, comms->r.myHostID );
            stream_putU8( tmpStream, destID );
            if ( data != NULL && dlen > 0 ) {
                stream_putBytes( tmpStream, data, dlen );
            }
            break;
        case XWRELAY_GAME_CONNECT:
            stream_putU8( tmpStream, XWRELAY_PROTO_VERSION );
            stringToStream( tmpStream, addr.u.ip_relay.invite );
            stream_putU8( tmpStream, addr.u.ip_relay.seeksPublicRoom );
            stream_putU8( tmpStream, addr.u.ip_relay.advertiseRoom );
            /* XP_ASSERT( cmd == XWRELAY_GAME_RECONNECT */
            /*            || comms->r.myHostID == HOST_ID_NONE */
            /*            || comms->r.myHostID == HOST_ID_SERVER ); */
            XP_LOGF( "%s: writing nPlayersHere: %d; nPlayersTotal: %d",
                     __func__, comms->r.nPlayersHere, 
                     comms->r.nPlayersTotal );
            stream_putU8( tmpStream, comms->r.nPlayersHere );
            stream_putU8( tmpStream, comms->r.nPlayersTotal );
            stream_putU16( tmpStream, comms_getChannelSeed(comms) );
            stream_putU8( tmpStream, comms->util->gameInfo->dictLang );
            set_relay_state( comms, COMMS_RELAYSTATE_CONNECT_PENDING );
            break;

        case XWRELAY_GAME_RECONNECT:
            stream_putU8( tmpStream, XWRELAY_PROTO_VERSION );
            stringToStream( tmpStream, addr.u.ip_relay.invite );
            stream_putU8( tmpStream, addr.u.ip_relay.seeksPublicRoom );
            stream_putU8( tmpStream, addr.u.ip_relay.advertiseRoom );
            stream_putU8( tmpStream, comms->r.myHostID );
            XP_ASSERT( cmd == XWRELAY_GAME_RECONNECT
                       || comms->r.myHostID == HOST_ID_NONE
                       || comms->r.myHostID == HOST_ID_SERVER );
            XP_LOGF( "%s: writing nPlayersHere: %d; nPlayersTotal: %d",
                     __func__, comms->r.nPlayersHere, 
                     comms->r.nPlayersTotal );
            stream_putU8( tmpStream, comms->r.nPlayersHere );
            stream_putU8( tmpStream, comms->r.nPlayersTotal );
            stream_putU16( tmpStream, comms_getChannelSeed(comms) );
            stream_putU8( tmpStream, comms->util->gameInfo->dictLang );
            stringToStream( tmpStream, comms->r.connName );
            set_relay_state( comms, COMMS_RELAYSTATE_CONNECT_PENDING );
            break;

        case XWRELAY_ACK:
            stream_putU8( tmpStream, destID );
            break;

        case XWRELAY_GAME_DISCONNECT:
            stream_putU16( tmpStream, comms->r.cookieID );
            stream_putU8( tmpStream, comms->r.myHostID );
            break;

#if defined XWFEATURE_RELAY && defined RELAY_HEARTBEAT
        case XWRELAY_HEARTBEAT:
            /* Add these for grins.  Server can assert they match the IP
               address it expects 'em on. */
            stream_putU16( tmpStream, comms->r.cookieID );
            stream_putU8( tmpStream, comms->r.myHostID );
            break;
#endif
        default:
            XP_ASSERT(0); 
        }

        len = stream_getSize( tmpStream );
        buf = XP_MALLOC( comms->mpool, len );
        if ( buf != NULL ) {
            stream_getBytes( tmpStream, buf, len );
        }
        stream_destroy( tmpStream );
        if ( buf != NULL ) {
            XP_U16 result;
            XP_LOGF( "%s: passing %d bytes to sendproc", __func__, len );
            result = (*comms->procs.send)( buf, len, &addr, 
                                           comms->procs.closure );
            success = result == len;
            if ( success ) {
                setHeartbeatTimer( comms );
            }
        }
        XP_FREE( comms->mpool, buf );
    }
    return success;
} /* send_via_relay */

/* Send a CONNECT message to the relay.  This opens up a connection to the
 * relay, and tells it our hostID and cookie so that it can associatate it
 * with a socket.  In the CONNECT_RESP we should get back what?
 */
static XP_Bool
relayConnect( CommsCtxt* comms )
{
    XP_Bool success = XP_TRUE;
    LOG_FUNC();
    if ( comms->addr.conType == COMMS_CONN_RELAY && !comms->r.connecting ) {
        comms->r.connecting = XP_TRUE;
        success = send_via_relay( comms, comms->r.connName[0]?
                                  XWRELAY_GAME_RECONNECT : XWRELAY_GAME_CONNECT,
                                  comms->r.myHostID, NULL, 0 );
        comms->r.connecting = XP_FALSE;
    }
    return success;
} /* relayConnect */
#endif

#if defined XWFEATURE_BLUETOOTH || defined XWFEATURE_IP_DIRECT
static XP_S16
send_via_bt_or_ip( CommsCtxt* comms, BTIPMsgType typ, XP_PlayerAddr channelNo,
                   void* data, int dlen )
{
    XP_S16 nSent;
    XP_U8* buf;
    LOG_FUNC();
    nSent = -1;
    buf = XP_MALLOC( comms->mpool, dlen + 1 );
    if ( !!buf ) {
        const CommsAddrRec* addr;
        (void)channelToAddress( comms, channelNo, &addr );

        buf[0] = typ;
        if ( dlen > 0 ) {
            XP_MEMCPY( &buf[1], data, dlen );
        }

        nSent = (*comms->procs.send)( buf, dlen+1, addr, comms->procs.closure );
        XP_FREE( comms->mpool, buf );

        setHeartbeatTimer( comms );
    }
    LOG_RETURNF( "%d", nSent );
    return nSent;
} /* send_via_bt_or_ip */

#endif

#ifdef XWFEATURE_RELAY
static void
relayDisconnect( CommsCtxt* comms )
{
    LOG_FUNC();
    if ( comms->addr.conType == COMMS_CONN_RELAY ) {
        if ( comms->r.relayState > COMMS_RELAYSTATE_CONNECT_PENDING ) {
            send_via_relay( comms, XWRELAY_GAME_DISCONNECT, HOST_ID_NONE, 
                            NULL, 0 );
        }
        set_relay_state( comms, COMMS_RELAYSTATE_UNCONNECTED );
    }
} /* relayDisconnect */
#endif

EXTERN_C_END

#endif /* #ifndef XWFEATURE_STANDALONE_ONLY */
