/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2001-2005 by Eric House (xwords@eehouse.org).  All rights reserved.
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

EXTERN_C_START

typedef struct MsgQueueElem {
    struct MsgQueueElem* next;
    XP_U8* msg;
    XP_U16 len;
    XP_U16 channelNo;
    MsgID msgID;                /* saved for ease of deletion */
} MsgQueueElem;

typedef struct AddressRecord {
    struct AddressRecord* next;
    CommsAddrRec addr;
#ifdef DEBUG
    XP_U16 lastACK;
    XP_U16 nUniqueBytes;
#endif
    MsgID nextMsgID;		    /* on a per-channel basis */
    MsgID lastMsgReceived;	    /* on a per-channel basis */
    XP_PlayerAddr channelNo;
    struct {
        XWHostID hostID;            /* used for relay case */
    } r;
} AddressRecord;

#define ADDRESSRECORD_SIZE_68K 20

typedef enum {
    COMMS_RELAYSTATE_UNCONNECTED
    , COMMS_RELAYSTATE_CONNECT_PENDING
    , COMMS_RELAYSTATE_CONNECTED
    , COMMS_RELAYSTATE_ALLCONNECTED
} CommsRelayState;

struct CommsCtxt {
    XW_UtilCtxt* util;

    XP_U32 connID;		        /* 0 means ignore; otherwise must match */
    XP_U16 nextChannelNo;

    AddressRecord* recs;        /* return addresses */

    TransportSend sendproc;
    void* sendClosure;

    MsgQueueElem* msgQueueHead;
    MsgQueueElem* msgQueueTail;
    XP_U16 queueLen;

    /* The following fields, down to isServer, are only used if
       XWFEATURE_RELAY is defined, but I'm leaving them in here so apps built
       both ways can open each other's saved games files.*/
    CommsAddrRec addr;

    /* Stuff for relays */
    struct {
        XWHostID myHostID;          /* 0 if unset, 1 if acting as server,
                                       random for client */
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

#ifdef XWFEATURE_BLUETOOTH
typedef enum {
    BTMSG_DATA = 0
    ,BTMSG_RESET
} BTMsgType;
#endif

/****************************************************************************
 *                               prototypes 
 ****************************************************************************/
static AddressRecord* rememberChannelAddress( CommsCtxt* comms, 
                                              XP_PlayerAddr channelNo, 
                                              XWHostID id, 
                                              const CommsAddrRec* addr );
static XP_Bool channelToAddress( CommsCtxt* comms, XP_PlayerAddr channelNo, 
                                 const CommsAddrRec** addr );
static AddressRecord* getRecordFor( CommsCtxt* comms, 
                                    XP_PlayerAddr channelNo );
static XP_S16 sendMsg( CommsCtxt* comms, MsgQueueElem* elem );
static void addToQueue( CommsCtxt* comms, MsgQueueElem* newMsgElem );
static XP_U16 countAddrRecs( const CommsCtxt* comms );
#ifdef XWFEATURE_RELAY
static void relayConnect( CommsCtxt* comms );
static void relayDisconnect( CommsCtxt* comms );
static XP_Bool send_via_relay( CommsCtxt* comms, XWRELAY_Cmd cmd, 
                               XWHostID destID, void* data, int dlen );
static XWHostID getDestID( CommsCtxt* comms, XP_PlayerAddr channelNo );
static void setHeartbeatTimer( CommsCtxt* comms );
#endif
#ifdef XWFEATURE_BLUETOOTH
static XP_S16 send_via_bt( CommsCtxt* comms, BTMsgType typ, 
                           XP_PlayerAddr channelNo,
                           void* data, int dlen );
static void btConnect( CommsCtxt* comms );
#endif

/****************************************************************************
 *                               implementation 
 ****************************************************************************/
CommsCtxt* 
comms_make( MPFORMAL XW_UtilCtxt* util, XP_Bool isServer, 
            XP_U16 XP_UNUSED_RELAY(nPlayersHere), 
            XP_U16 XP_UNUSED_RELAY(nPlayersTotal),
            TransportSend sendproc, void* closure )
{
    CommsCtxt* result = (CommsCtxt*)XP_MALLOC( mpool, sizeof(*result) );
    XP_MEMSET( result, 0, sizeof(*result) );

    MPASSIGN(result->mpool, mpool);

    result->isServer = isServer;
    result->sendproc = sendproc;
    result->sendClosure = closure;
    result->util = util;

#ifdef XWFEATURE_RELAY
    result->r.myHostID = isServer? HOST_ID_SERVER: HOST_ID_NONE;
    XP_LOGF( "set myHostID to %d", result->r.myHostID );

    result->r.relayState = COMMS_RELAYSTATE_UNCONNECTED;
    result->r.nPlayersHere = nPlayersHere;
    result->r.nPlayersTotal = nPlayersTotal;
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
        XP_FREE( comms->mpool, msg->msg );
        XP_FREE( comms->mpool, msg );
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
comms_reset( CommsCtxt* comms, XP_Bool isServer, 
             XP_U16 XP_UNUSED_RELAY(nPlayersHere), 
             XP_U16 XP_UNUSED_RELAY(nPlayersTotal) )
{
    LOG_FUNC();
#ifdef XWFEATURE_RELAY
    relayDisconnect( comms );
#endif

    cleanupInternal( comms );
    comms->isServer = isServer;

    cleanupAddrRecs( comms );

    comms->nextChannelNo = 0;

    comms->connID = CONN_ID_NONE;
#ifdef XWFEATURE_RELAY
    comms->r.cookieID = COOKIE_ID_NONE;
    comms->r.nPlayersHere = nPlayersHere;
    comms->r.nPlayersTotal = nPlayersTotal;
    relayConnect( comms );
#endif
    LOG_RETURN_VOID();
} /* comms_reset */

void
comms_destroy( CommsCtxt* comms )
{
    cleanupInternal( comms );
    cleanupAddrRecs( comms );

    XP_FREE( comms->mpool, comms );
} /* comms_destroy */

void
comms_setConnID( CommsCtxt* comms, XP_U32 connID )
{
    comms->connID = connID;
    XP_STATUSF( "set connID to %lx", connID );
} /* comms_setConnID */

static void
addrFromStream( CommsAddrRec* addrP, XWStreamCtxt* stream )
{
    CommsAddrRec addr;

    addr.conType = stream_getU8( stream );

    switch( addr.conType ) {
    case COMMS_CONN_UNUSED:
/*         XP_ASSERT( 0 ); */
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
    case COMMS_CONN_IP_NOUSE:
        stringFromStreamHere( stream, addr.u.ip.hostName_ip,
                              sizeof(addr.u.ip.hostName_ip) );
        addr.u.ip.ipAddr_ip = stream_getU32( stream );
        addr.u.ip.port_ip = stream_getU16( stream );
        break;
    case COMMS_CONN_RELAY:
        stringFromStreamHere( stream, addr.u.ip_relay.cookie,
                              sizeof(addr.u.ip_relay.cookie) );
        stringFromStreamHere( stream, addr.u.ip_relay.hostName,
                              sizeof(addr.u.ip_relay.hostName) );
        addr.u.ip_relay.ipAddr = stream_getU32( stream );
        addr.u.ip_relay.port = stream_getU16( stream );
        break;
    default:
        /* shut up, compiler */
        break;
    }

    XP_MEMCPY( addrP, &addr, sizeof(*addrP) );
} /* addrFromStream */

CommsCtxt* 
comms_makeFromStream( MPFORMAL XWStreamCtxt* stream, XW_UtilCtxt* util,
                      TransportSend sendproc, void* closure )
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
                        nPlayersHere, nPlayersTotal,
                        sendproc, closure );
    XP_MEMCPY( &comms->addr, &addr, sizeof(comms->addr) );

    comms->connID = stream_getU32( stream );
    comms->nextChannelNo = stream_getU16( stream );
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
        rec->lastMsgReceived = stream_getU16( stream );
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

        msg->len = stream_getU16( stream );
        msg->msg = (XP_U8*)XP_MALLOC( mpool, msg->len );
        stream_getBytes( stream, msg->msg, msg->len );

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

void
comms_start( CommsCtxt* comms )
{
    LOG_FUNC();
    if ( 0 ) {
#ifdef XWFEATURE_RELAY
    } else if ( comms->addr.conType == COMMS_CONN_RELAY ) {
        comms->r.relayState = COMMS_RELAYSTATE_UNCONNECTED;
        relayConnect( comms );
#endif
#ifdef XWFEATURE_BLUETOOTH
    } else if ( comms->addr.conType == COMMS_CONN_BT ) {
        btConnect( comms );
#endif
    }
    LOG_RETURN_VOID();
} /* comms_start */

static void
addrToStream( XWStreamCtxt* stream, const CommsAddrRec* addrP )
{
    CommsAddrRec addr;
    XP_MEMCPY( &addr, addrP, sizeof(addr) );

    stream_putU8( stream, addr.conType );

    switch( addr.conType ) {
#ifdef DEBUG
    case COMMS_CONN_UNUSED:
    case LAST_____FOO:
/*         XP_ASSERT( 0 ); */
        break;
#endif
    case COMMS_CONN_BT:
        stringToStream( stream, addr.u.bt.hostName );
        /* sizeof(.bits) below defeats ARM's padding. */
        stream_putBytes( stream, &addr.u.bt.btAddr.bits, 
                         sizeof(addr.u.bt.btAddr.bits) );
        break;
    case COMMS_CONN_IR:
        /* nothing to save */
        break;
    case COMMS_CONN_IP_NOUSE:
        stringToStream( stream, addr.u.ip.hostName_ip );
        stream_putU32( stream, addr.u.ip.ipAddr_ip );
        stream_putU16( stream, addr.u.ip.port_ip );
        break;
    case COMMS_CONN_RELAY:
        stringToStream( stream, addr.u.ip_relay.cookie );
        stringToStream( stream, addr.u.ip_relay.hostName );
        stream_putU32( stream, addr.u.ip_relay.ipAddr );
        stream_putU16( stream, addr.u.ip_relay.port );
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
        stream_putU16( stream, (XP_U16)rec->lastMsgReceived );
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
#if defined XWFEATURE_RELAY || defined XWFEATURE_BLUETOOTH
    util_addrChange( comms->util, &comms->addr, addr );
#endif
    XP_MEMCPY( &comms->addr, addr, sizeof(comms->addr) );

    if ( 0 ) {
#ifdef XWFEATURE_RELAY
    /* We should now have a cookie so we can connect??? */
    } else if ( addr->conType == COMMS_CONN_RELAY ) {
        relayConnect( comms );
#endif
#ifdef XWFEATURE_BLUETOOTH
    } else if ( addr->conType == COMMS_CONN_BT ) {
        btConnect( comms );
#endif
    }
} /* comms_setAddr */

void
comms_getInitialAddr( CommsAddrRec* addr )
{ 	 
#if defined  XWFEATURE_RELAY
    addr->conType = COMMS_CONN_RELAY; /* for temporary ease in debugging */
    addr->u.ip_relay.ipAddr = 0L; /* force 'em to set it */
    addr->u.ip_relay.port = 10999;
    {
        char* name = "eehouse.org";
        XP_MEMCPY( addr->u.ip_relay.hostName, name, XP_STRLEN(name)+1 );
    }
    addr->u.ip_relay.cookie[0] = '\0';
#elif defined PLATFORM_PALM
    /* default values; default is still IR where there's a choice, at least on
       Palm... */
    addr->conType = COMMS_CONN_IR;
#else
    addr->conType = COMMS_CONN_BT;
#endif
} /* comms_getInitialAddr */

CommsConnType 
comms_getConType( const CommsCtxt* comms )
{
    return comms->addr.conType;
} /* comms_getConType */

XP_Bool
comms_getIsServer( const CommsCtxt* comms )
{
    return comms->isServer;
}

/* Send a message using the sequentially next MsgID.  Save the message so
 * resend can work. */
XP_S16
comms_send( CommsCtxt* comms, XWStreamCtxt* stream )
{
    XP_U16 streamSize = stream_getSize( stream );
    XP_U16 headerLen;
    XP_PlayerAddr channelNo = stream_getAddress( stream );
    AddressRecord* rec = getRecordFor( comms, channelNo );
    MsgID msgID = (!!rec)? ++rec->nextMsgID : 0;
    MsgID lastMsgRcd = (!!rec)? rec->lastMsgReceived : 0;
    MsgQueueElem* newMsgElem;
    XWStreamCtxt* msgStream;

    XP_DEBUGF( "assigning msgID=" XP_LD " on chnl %d", msgID, channelNo );

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

    msgStream = mem_stream_make( MPPARM(comms->mpool) 
                                 util_getVTManager(comms->util),
                                 NULL, 0, 
                                 (MemStreamCloseCallback)NULL );
    stream_open( msgStream );
    stream_putU32( msgStream, comms->connID );

    stream_putU16( msgStream, channelNo );
    stream_putU32( msgStream, msgID );
    stream_putU32( msgStream, lastMsgRcd );

    headerLen = stream_getSize( msgStream );
    newMsgElem->len = streamSize + headerLen;
    newMsgElem->msg = (XP_U8*)XP_MALLOC( comms->mpool, newMsgElem->len );

    stream_getBytes( msgStream, newMsgElem->msg, headerLen );
    stream_destroy( msgStream );
    
    stream_getBytes( stream, newMsgElem->msg + headerLen, streamSize );

    addToQueue( comms, newMsgElem );

    return sendMsg( comms, newMsgElem );
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
        comms->queueLen = 1;
    } else {
        XP_ASSERT( !!comms->msgQueueTail );
        comms->msgQueueTail->next = newMsgElem;
        comms->msgQueueTail = newMsgElem;

        XP_ASSERT( comms->queueLen > 0 );
        ++comms->queueLen;
    }
    XP_STATUSF( "addToQueue: queueLen now %d", comms->queueLen );
} /* addToQueue */

#ifdef DEBUG
static void
printQueue( CommsCtxt* comms )
{
    MsgQueueElem* elem;
    short i;

    for ( elem = comms->msgQueueHead, i = 0; i < comms->queueLen; 
          elem = elem->next, ++i ) {
        XP_STATUSF( "\t%d: channel: %d; msgID=" XP_LD,
                    i+1, elem->channelNo, elem->msgID );
    }
}
#endif

/* We've received on some channel a message with a certain ID.  This means
 * that all messages sent on that channel with lower IDs have been received
 * and can be removed from our queue.  BUT: if this ID is higher than any
 * we've sent, don't remove.  We may be starting a new game but have a server
 * that's still on the old one.
 */
static void
removeFromQueue( CommsCtxt* comms, XP_PlayerAddr channelNo, MsgID msgID )
{
    XP_STATUSF( "%s: remove msgs <= " XP_LD " for channel %d (queueLen: %d)",
                __func__, msgID, channelNo, comms->queueLen );

    if ( (channelNo == 0) || !!getRecordFor(comms, channelNo) ) {
        MsgQueueElem dummy;
        MsgQueueElem* keep = &dummy;
        MsgQueueElem* elem;
        MsgQueueElem* next;

        for ( elem = comms->msgQueueHead; !!elem; elem = next ) {
            XP_Bool knownGood = XP_FALSE;
            next = elem->next;

            /* remove the 0-channel message if we've established a channel
               number.  Only clients should have any 0-channel messages in the
               queue, and receiving something from the server is an implicit
               ACK -- IFF it isn't left over from the last game. */

            if ( (elem->channelNo == 0) && (channelNo != 0) ) {
                XP_ASSERT( !comms->isServer );
                XP_ASSERT( elem->msgID == 0 );
            } else if ( elem->channelNo != channelNo ) {
                knownGood = XP_TRUE;
            }

            if ( !knownGood && (elem->msgID <= msgID) ) {
                XP_FREE( comms->mpool, elem->msg );
                XP_FREE( comms->mpool, elem );
                --comms->queueLen;
            } else {
                keep->next = elem;
                keep = elem;
            }
        }

        keep->next = NULL;
        comms->msgQueueHead = dummy.next;
    }

    XP_STATUSF( "%s: queueLen now %d", __func__, comms->queueLen );

    XP_ASSERT( comms->queueLen > 0 || comms->msgQueueHead == NULL );

#ifdef DEBUG
    printQueue( comms );
#endif
} /* removeFromQueue */

static XP_S16
sendMsg( CommsCtxt* comms, MsgQueueElem* elem )
{
    XP_S16 result = 0;
    XP_PlayerAddr channelNo;
#if defined XWFEATURE_RELAY || defined XWFEATURE_BLUETOOTH
    CommsConnType conType = comms_getConType( comms );
#endif

    channelNo = elem->channelNo;

    if ( 0 ) {
#ifdef XWFEATURE_RELAY
    } else if ( conType == COMMS_CONN_RELAY ) {
        if ( comms->r.relayState == COMMS_RELAYSTATE_ALLCONNECTED ) {
            XWHostID destID = getDestID( comms, channelNo );
            result = send_via_relay( comms, XWRELAY_MSG_TORELAY, destID, 
                                     elem->msg, elem->len );
        } else {
            XP_LOGF( "%s: skipping message: not connected", __func__ );
        }
#endif
#ifdef XWFEATURE_BLUETOOTH
    } else if ( conType == COMMS_CONN_BT ) {
        result = send_via_bt( comms, BTMSG_DATA, channelNo, 
                              elem->msg, elem->len );
#endif
    } else {
        const CommsAddrRec* addr;
        (void)channelToAddress( comms, channelNo, &addr );

        XP_ASSERT( !!comms->sendproc );
        result = (*comms->sendproc)( elem->msg, elem->len, addr,
                                     comms->sendClosure );
    }
    return result;
} /* sendMsg */

XP_S16
comms_resendAll( CommsCtxt* comms )
{
    MsgQueueElem* msg;
    XP_S16 result = 0;
    LOG_FUNC();
    XP_ASSERT( !!comms );

    for ( msg = comms->msgQueueHead; !!msg; msg = msg->next ) {
        XP_S16 oneResult = sendMsg( comms, msg );
        if ( result == 0 && oneResult != 0 ) {
            result = oneResult;
        }
        XP_STATUSF( "resend: msgID=" XP_LD "; rslt=%d", 
                    msg->msgID, oneResult );
    }

    LOG_RETURNF( "%d", result );
    return result;
} /* comms_resend */

#ifdef XWFEATURE_RELAY
static XP_Bool
relayPreProcess( CommsCtxt* comms, XWStreamCtxt* stream, XWHostID* senderID )
{
    XP_Bool consumed = XP_TRUE;
    XWHostID destID, srcID;
    CookieID cookieID;
    XP_U8 relayErr;
    XP_U8 hasName;

    /* nothing for us to do here if not using relay */
    XWRELAY_Cmd cmd = stream_getU8( stream );
    switch( cmd ) {

    case XWRELAY_CONNECT_RESP:
    case XWRELAY_RECONNECT_RESP:
        comms->r.relayState = COMMS_RELAYSTATE_CONNECTED;
        comms->r.heartbeat = stream_getU16( stream );
        comms->r.cookieID = stream_getU16( stream );
        comms->r.myHostID = (XWHostID)stream_getU8( stream );
        XP_LOGF( "got XWRELAY_CONNECTRESP; set cookieID = %d; "
                 "set hostid: %x",
                 comms->r.cookieID, comms->r.myHostID );
        setHeartbeatTimer( comms );
        break;

    case XWRELAY_ALLHERE:
        comms->r.relayState = COMMS_RELAYSTATE_ALLCONNECTED;
        hasName = stream_getU8( stream );
        if ( hasName ) {
            stringFromStreamHere( stream, comms->r.connName, 
                                  sizeof(comms->r.connName) );
            XP_LOGF( "read connName: %s", comms->r.connName );
        } else {
            XP_ASSERT( comms->r.connName[0] != '\0' );
        }

        /* We're [re-]connected now.  Send any pending messages.  This may
           need to be done later since we're inside the platform's socket
           read proc now. */
        comms_resendAll( comms );
        break;
    case XWRELAY_MSG_FROMRELAY:
        cookieID = stream_getU16( stream );
        srcID = stream_getU8( stream );
        destID = stream_getU8( stream );
        XP_LOGF( "cookieID: %d; srcID: %x; destID: %x",
                 cookieID, srcID, destID );
        /* If these values don't check out, drop it */
        consumed = cookieID != comms->r.cookieID
            || destID != comms->r.myHostID;
        if ( consumed ) {
            XP_LOGF( "rejecting data message" );
        } else {
            *senderID = srcID;
        }
        break;

    case XWRELAY_DISCONNECT_OTHER:
        relayErr = stream_getU8( stream );
        srcID = stream_getU8( stream );
        XP_LOGF( "host id %x disconnected", srcID );
        /* we will eventually want to tell the user which player's gone */
        util_userError( comms->util, ERR_RELAY_BASE + relayErr );
        break;

    case XWRELAY_DISCONNECT_YOU:                /* Close socket for this? */
    case XWRELAY_CONNECTDENIED:                 /* Close socket for this? */
        XP_LOGF( "XWRELAY_DISCONNECT_YOU|XWRELAY_CONNECTDENIED" );
        relayErr = stream_getU8( stream );
        util_userError( comms->util, ERR_RELAY_BASE + relayErr );
        comms->r.relayState = COMMS_RELAYSTATE_UNCONNECTED;
        /* fallthru */
    default:
        XP_LOGF( "dropping relay msg with cmd %d", (XP_U16)cmd );
    }
    
    return consumed;
} /* relayPreProcess */
#endif

#ifdef XWFEATURE_BLUETOOTH
static XP_Bool
btPreProcess( CommsCtxt* comms, XWStreamCtxt* stream )
{
    BTMsgType typ = (BTMsgType)stream_getU8( stream );
    XP_Bool consumed = typ != BTMSG_DATA;

    if ( consumed ) {
        XP_ASSERT( typ == BTMSG_RESET );
        (void)comms_resendAll( comms );
    }

    return consumed;
} /* btPreProcess */
#endif

static XP_Bool
addressUnknown( CommsCtxt* comms, const CommsAddrRec* addr )
{
    XP_Bool unknown = XP_TRUE;
    if ( !!addr ) {
        CommsConnType conType = addr->conType;
        const AddressRecord* rec;
        for ( rec = comms->recs; !!rec && unknown ; rec = rec->next ) {
            XP_ASSERT( conType == rec->addr.conType );
            switch( conType ) {
            case COMMS_CONN_RELAY:
                if ( (addr->u.ip_relay.ipAddr == rec->addr.u.ip_relay.ipAddr)
                     && (addr->u.ip_relay.port == rec->addr.u.ip_relay.port ) ) {
                    unknown = XP_FALSE;
                }
                break;
            case COMMS_CONN_BT:
                if ( 0 == XP_MEMCMP( &addr->u.bt.btAddr, &rec->addr.u.bt.btAddr,
                                     sizeof(addr->u.bt.btAddr) ) ) {
                    unknown = XP_FALSE;
                }
                break;
            case COMMS_CONN_IR:              /* no way to test */
            default:
                break;
            }
        }
    }

    return unknown;
} /* addressUnknown */

/* read a raw buffer into a stream, stripping off the headers and keeping
 * any necessary stats.
 *
 * Keep track of return addresses by channel.  If the message's channel number
 * is 0, assign a new channel number and associate an address with it.
 * Otherwise update the address, which may have changed since we last heard
 * from this channel.
 *
 * There may be messages that are only about the comms 'protocol', that
 * contain nothing to be passed to the server.  In that case, return false
 * indicating the caller that all processing is finished.
 *
 * conType tells both how to interpret the addr and whether to expect any
 * special fields in the message itself.  In the IP case, for example, the
 * port component of a return address is in the message but the IP address
 * component will be passed in.
 */
XP_Bool
comms_checkIncomingStream( CommsCtxt* comms, XWStreamCtxt* stream, 
                           const CommsAddrRec* addr )
{
    XP_U16 channelNo;
    XP_U32 connID;
    MsgID msgID;
    MsgID lastMsgRcd;
    XP_Bool validMessage = XP_FALSE;
    AddressRecord* recs = (AddressRecord*)NULL;
    XWHostID senderID = 0;      /* unset; default for non-relay cases */
    XP_Bool usingRelay = XP_FALSE;
    XP_Bool channelWas0 = XP_FALSE;
    XP_Bool done = XP_FALSE;

    /* Firing when switch from bt to solo in mid game and other device
       sends.  BT should be shut down!!  */
    XP_ASSERT( addr == NULL || comms->addr.conType == addr->conType );

    if ( 0 ) {
#ifdef XWFEATURE_RELAY
    /* relayPreProcess returns true if consumes the message.  May just eat the
       header and leave a regular message to be processed below. */
    } else if ( comms->addr.conType == COMMS_CONN_RELAY ) {
        done = relayPreProcess( comms, stream, &senderID );
        if ( !done ) {
            usingRelay = comms->addr.conType == COMMS_CONN_RELAY;
        }
#endif
#ifdef XWFEATURE_BLUETOOTH
    } else if ( comms->addr.conType == COMMS_CONN_BT ) {
        done = btPreProcess( comms, stream );
#endif
    }

    if ( !done ) {
        if ( stream_getSize( stream ) >= sizeof(connID) ) {
            connID = stream_getU32( stream );
            XP_STATUSF( "%s: read connID of %lx", __func__, connID );

            if ( comms->connID == connID || comms->connID == CONN_ID_NONE ) {
                if ( stream_getSize( stream ) >= sizeof(channelNo) 
                     + sizeof(msgID) + sizeof(lastMsgRcd) ) {

                    channelNo = stream_getU16( stream );
                    XP_STATUSF( "read channelNo %d", channelNo );

                    msgID = stream_getU32( stream );
                    lastMsgRcd = stream_getU32( stream );

                    XP_DEBUGF( "rcd: msgID=" XP_LD " on chnl %d", msgID, 
                               channelNo );

                    removeFromQueue( comms, channelNo, lastMsgRcd );

                    /* Problem: need to detect duplicate messages even before
                       the server's had a chance to assign channels.
                       Solution, which is a hack: since hostID does the same
                       thing, use it in the relay case.  But in the relay-less
                       case, which still needs to work, do assign channels.
                       The dup message problem is far less common there.  */

                    if ( channelNo != 0 ) {
                        validMessage = XP_TRUE;
                    } else {
                        /* If we've seen a channel0 msg from same addr, drop
                           it. It's most likely a dupe, and server can't
                           handle e.g. duplicate client reg messages.. */
                        validMessage = addressUnknown( comms, addr );

                        if ( validMessage ) {
                            XP_ASSERT( comms->isServer );
                            if ( usingRelay ) {
                                XP_ASSERT( senderID != 0 );
                                channelNo = senderID;
                            } else {
                                XP_ASSERT( msgID == 0 );
                                /* check that address isn't already associated
                                   with an existing channel. */
                                channelNo = ++comms->nextChannelNo;
                                XP_LOGF( "%s: incrementled nextChannelNo "
                                         "to %d", __FUNCTION__, 
                                         comms->nextChannelNo );
                                channelWas0 = XP_TRUE;
                            }
                            XP_STATUSF( "assigning channelNo=%d", channelNo );
                        }
                    }

                    if ( validMessage && (usingRelay || !channelWas0) ) {
                        recs = getRecordFor( comms, channelNo );
                        /* messageID for an incoming message should be one
                         * greater than the id most recently used for that
                         * channel. */
                        if ( !!recs ) {
                            if ( msgID != recs->lastMsgReceived + 1 ) {
                                XP_DEBUGF( "on channel %d, msgID=" XP_LD 
                                           " (next should be " XP_LD ")", 
                                           channelNo, msgID, 
                                           recs->lastMsgReceived+1 );
                                validMessage = XP_FALSE;
                            }
                        } else if ( msgID > 1 ) {
                            validMessage = XP_FALSE;
                        }
#ifdef DEBUG
                        if ( !!recs ) {
                            XP_ASSERT( lastMsgRcd <= recs->nextMsgID );
                            XP_ASSERT( lastMsgRcd < 0x0000FFFF );
                            recs->lastACK = (XP_U16)lastMsgRcd;
                        }
#endif
                    }
    
                    if ( validMessage ) {
                        XP_LOGF( "remembering senderID %x for channel %d",
                                 senderID, channelNo );

                        recs = rememberChannelAddress( comms, channelNo, 
                                                       senderID, addr );
                        stream_setAddress( stream, channelNo );

                        if ( !!recs ) {
                            recs->lastMsgReceived = msgID;
                        }
                        XP_STATUSF( "set channel %d's lastMsgReceived to " 
                                    XP_LD, channelNo, msgID );
                    }
                } else {
                    XP_LOGF( "%s: message too small", __FUNCTION__ );
                }
            } else {
                XP_STATUSF( "refusing non-matching connID; got %lx, "
                            "wanted %lx", connID, comms->connID );
            }
        } else {
            XP_LOGF( "%s: message too small", __FUNCTION__ );
        }
    }
    LOG_RETURNF( "%d", (XP_U16)validMessage );
    return validMessage;
} /* comms_checkIncomingStream */

#ifdef XWFEATURE_RELAY
static void
p_comms_timerFired( void* closure, XWTimerReason XP_UNUSED_DBG(why) )
{
    CommsCtxt* comms = (CommsCtxt*)closure;
    XP_ASSERT( why == TIMER_HEARTBEAT );
    XP_LOGF( "comms_timerFired" );
    if ( comms->r.heartbeat != HEARTBEAT_NONE ) {
        send_via_relay( comms, XWRELAY_HEARTBEAT, HOST_ID_NONE, NULL, 0 );
        /* No need to reset timer.  send_via_relay does that. */
    }
} /* comms_timerFired */

static void
setHeartbeatTimer( CommsCtxt* comms )
{
    util_setTimer( comms->util, TIMER_HEARTBEAT, comms->r.heartbeat,
                   p_comms_timerFired, comms );
}
#endif

#ifdef DEBUG
void
comms_getStats( CommsCtxt* comms, XWStreamCtxt* stream )
{
    XP_UCHAR buf[100];
    AddressRecord* rec;
    MsgQueueElem* elem;

    XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf), 
                 (XP_UCHAR*)"msg queue len: %d\n", comms->queueLen );
    stream_putString( stream, buf );

    for ( elem = comms->msgQueueHead; !!elem; elem = elem->next ) {
        XP_SNPRINTF( buf, sizeof(buf), 
                     " - channelNo=%d; msgID=" XP_LD "; len=%d\n", 
                     elem->channelNo, elem->msgID, elem->len );
        stream_putString( stream, buf );
    }

    XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf), 
                 (XP_UCHAR*)"channel-less bytes sent: %d\n", 
                 comms->nUniqueBytes );
    stream_putString( stream, buf );

    for ( rec = comms->recs; !!rec; rec = rec->next ) {
        XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf), 
                     (XP_UCHAR*)"  Stats for channel: %d\n", 
                     rec->channelNo );
        stream_putString( stream, buf );

        XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf), 
                     (XP_UCHAR*)"Last msg sent: " XP_LD "\n", 
                     rec->nextMsgID );
        stream_putString( stream, buf );

        XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf), 
                     (XP_UCHAR*)"Unique bytes sent: %d\n", 
                     rec->nUniqueBytes );
        stream_putString( stream, buf );

        XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf), 
                     (XP_UCHAR*)"Last message acknowledged: %d\n", 
                     rec->lastACK);
        stream_putString( stream, buf );
    }
} /* comms_getStats */
#endif

static AddressRecord*
rememberChannelAddress( CommsCtxt* comms, XP_PlayerAddr channelNo, 
                        XWHostID hostID, const CommsAddrRec* addr )
{
    AddressRecord* recs = NULL;
    recs = getRecordFor( comms, channelNo );
    if ( !recs ) {
        /* not found; add a new entry */
        recs = (AddressRecord*)XP_MALLOC( comms->mpool, sizeof(*recs) );

        recs->nextMsgID = 0;
        recs->channelNo = channelNo;
        recs->r.hostID = hostID;
#ifdef DEBUG
        recs->nUniqueBytes = 0;
#endif
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

static XP_Bool
channelToAddress( CommsCtxt* comms, XP_PlayerAddr channelNo, 
                  const CommsAddrRec** addr )
{
    AddressRecord* recs = getRecordFor( comms, channelNo );
    XP_Bool found = !!recs;
    *addr = found? &recs->addr : NULL;
    return found;
} /* channelToAddress */

static AddressRecord* 
getRecordFor( CommsCtxt* comms, XP_PlayerAddr channelNo )
{
    AddressRecord* recs = NULL;

    if ( channelNo != CHANNEL_NONE ) {
        for ( recs = comms->recs; !!recs; recs = recs->next ) {
            if ( recs->channelNo == channelNo ) {
                break;
            }
        }
    }
    return recs;
} /* getRecordFor */

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
    if ( channelNo == CHANNEL_NONE ) {
        id = HOST_ID_SERVER;
    } else {
        AddressRecord* recs;
        for ( recs = comms->recs; !!recs; recs = recs->next ) {
            if ( recs->channelNo == channelNo ) {
                id = recs->r.hostID;
            }
        }
    }
    XP_LOGF( "getDestID(%d) => %x", channelNo, id );
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
            stream_putU16( tmpStream, comms->r.cookieID );
            stream_putU8( tmpStream, comms->r.myHostID );
            stream_putU8( tmpStream, destID );
            if ( data != NULL && dlen > 0 ) {
                stream_putBytes( tmpStream, data, dlen );
            }
            break;
        case XWRELAY_GAME_CONNECT:
            stream_putU8( tmpStream, XWRELAY_PROTO_VERSION );
            stringToStream( tmpStream, addr.u.ip_relay.cookie );
            stream_putU8( tmpStream, comms->r.myHostID );
            stream_putU8( tmpStream, comms->r.nPlayersHere );
            stream_putU8( tmpStream, comms->r.nPlayersTotal );

            comms->r.relayState = COMMS_RELAYSTATE_CONNECT_PENDING;
            break;

        case XWRELAY_GAME_RECONNECT:
            stream_putU8( tmpStream, XWRELAY_PROTO_VERSION );
            stream_putU8( tmpStream, comms->r.myHostID );
            stream_putU8( tmpStream, comms->r.nPlayersHere );
            stream_putU8( tmpStream, comms->r.nPlayersTotal );
            stringToStream( tmpStream, comms->r.connName );

            comms->r.relayState = COMMS_RELAYSTATE_CONNECT_PENDING;
            break;

        case XWRELAY_GAME_DISCONNECT:
            stream_putU16( tmpStream, comms->r.cookieID );
            stream_putU8( tmpStream, comms->r.myHostID );
            break;

        case XWRELAY_HEARTBEAT:
            /* Add these for grins.  Server can assert they match the IP
               address it expects 'em on. */
            stream_putU16( tmpStream, comms->r.cookieID );
            stream_putU8( tmpStream, comms->r.myHostID );
            break;

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
            XP_LOGF( "passing %d bytes to sendproc", len );
            result = (*comms->sendproc)( buf, len, &addr, comms->sendClosure );
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
static void
relayConnect( CommsCtxt* comms )
{
    LOG_FUNC();
    if ( comms->addr.conType == COMMS_CONN_RELAY && !comms->r.connecting ) {
        comms->r.connecting = XP_TRUE;
        send_via_relay( comms, 
                        comms->r.connName[0] == '\0' ?
                        XWRELAY_GAME_CONNECT:XWRELAY_GAME_RECONNECT,
                        comms->r.myHostID, NULL, 0 );
        comms->r.connecting = XP_FALSE;
    }
} /* relayConnect */
#endif

#ifdef XWFEATURE_BLUETOOTH
static void
btConnect( CommsCtxt* comms )
{
    XP_ASSERT( !!comms );
    /* Ping the bt layer so it'll get sockets set up.  PENDING: if I'm server
       need to do this once per guest record with non-null address.  Might as
       well use real messages if we have 'em.  Otherwise a fake size-0 msg. */

    send_via_bt( comms, BTMSG_RESET, CHANNEL_NONE, NULL, 0 );
    (void)( comms_resendAll( comms ) );
} /* btConnect */

static XP_S16
send_via_bt( CommsCtxt* comms, BTMsgType typ, XP_PlayerAddr channelNo,
             void* data, int dlen )
{
    XP_U8* buf;
    XP_S16 nSent = -1;

    buf = XP_MALLOC( comms->mpool, dlen + 1 );
    if ( !!buf ) {
        const CommsAddrRec* addr;
        (void)channelToAddress( comms, channelNo, &addr );

        buf[0] = typ;
        if ( dlen > 0 ) {
            XP_MEMCPY( &buf[1], data, dlen );
        }

        nSent = (*comms->sendproc)( buf, dlen+1, addr, comms->sendClosure );
        XP_FREE( comms->mpool, buf );
    }
    return nSent;
} /* send_via_bt */

#endif

#ifdef XWFEATURE_RELAY
static void
relayDisconnect( CommsCtxt* comms )
{
    XP_LOGF( "relayDisconnect called" );
    if ( comms->addr.conType == COMMS_CONN_RELAY ) {
        if ( comms->r.relayState != COMMS_RELAYSTATE_UNCONNECTED ) {
            comms->r.relayState = COMMS_RELAYSTATE_UNCONNECTED;
            send_via_relay( comms, XWRELAY_GAME_DISCONNECT, HOST_ID_NONE, 
                            NULL, 0 );
        }
    }
} /* relayDisconnect */
#endif

EXTERN_C_END

#endif /* #ifndef XWFEATURE_STANDALONE_ONLY */
