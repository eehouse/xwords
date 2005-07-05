/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2001 by Eric House (fixin@peak.org).  All rights reserved.
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
    XWHostID hostID;            /* used for relay case */
} AddressRecord;

#define ADDRESSRECORD_SIZE_68K 20

typedef enum {
    COMMS_RELAYSTATE_UNCONNECTED
    , COMMS_RELAYSTATE_CONNECT_PENDING
    , COMMS_RELAYSTATE_CONNECTED
} CommsRelaystate;

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

    CommsAddrRec addr;

    /* Stuff for relays */
    XWHostID myHostID;        /* 0 if unset, 1 if acting as server, random for
                                 client */
    CommsRelaystate relayState; /* not saved: starts at UNCONNECTED */
    XP_U16 cookieID;         /* standin for cookie; set by relay */

    /* heartbeat: for periodic pings if relay thinks the network the device is
       on requires them.  Not saved since only valid when connected, and we
       reconnect for every game and after restarting. */
    XP_U16 heartbeat;
    XP_Bool isServer;
    XP_Bool connecting;
#ifdef DEBUG
    XP_U16 nUniqueBytes;
#endif
    MPSLOT
};

/****************************************************************************
 *                               prototypes 
 ****************************************************************************/
static AddressRecord* rememberChannelAddress( CommsCtxt* comms, 
                                              XP_PlayerAddr channelNo, 
                                              XWHostID id, CommsAddrRec* addr );
static XP_Bool channelToAddress( CommsCtxt* comms, XP_PlayerAddr channelNo, 
                                 CommsAddrRec** addr );
static AddressRecord* getRecordFor( CommsCtxt* comms, 
                                    XP_PlayerAddr channelNo );
static XP_S16 sendMsg( CommsCtxt* comms, MsgQueueElem* elem );
static void addToQueue( CommsCtxt* comms, MsgQueueElem* newMsgElem );
static XP_U16 countAddrRecs( CommsCtxt* comms );
static void relayConnect( CommsCtxt* comms );
static XP_Bool send_via_relay( CommsCtxt* comms, XWRELAY_Cmd cmd, 
                               XWHostID destID, void* data, int dlen );
static XWHostID getDestID( CommsCtxt* comms, XP_PlayerAddr channelNo );
static void setHeartbeatTimer( CommsCtxt* comms );

/****************************************************************************
 *                               implementation 
 ****************************************************************************/
CommsCtxt* 
comms_make( MPFORMAL XW_UtilCtxt* util, XP_Bool isServer, 
            TransportSend sendproc, void* closure )
{
    XWHostID hostID;
    CommsCtxt* result = (CommsCtxt*)XP_MALLOC( mpool, sizeof(*result) );
    XP_MEMSET( result, 0, sizeof(*result) );

    MPASSIGN(result->mpool, mpool);

    result->isServer = isServer;
    result->sendproc = sendproc;
    result->sendClosure = closure;
    result->util = util;

    if ( isServer ) {
        hostID = HOST_ID_SERVER;
    } else {
        do {
            hostID = XP_RANDOM();
        } while ( hostID == HOST_ID_NONE || hostID == HOST_ID_SERVER );
    }
    result->myHostID = hostID;
    XP_LOGF( "set myHostID to %d", result->myHostID );

    result->relayState = COMMS_RELAYSTATE_UNCONNECTED;

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
comms_reset( CommsCtxt* comms, XP_Bool isServer )
{
    cleanupInternal( comms );
    comms->isServer = isServer;

    cleanupAddrRecs( comms );

    comms->nextChannelNo = 0;

    comms->connID = CONN_ID_NONE;
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

    addr.conType = stream_getBits( stream, 3 );

    switch( addr.conType ) {
    case COMMS_CONN_UNUSED:
        XP_ASSERT( 0 );
        break;
    case COMMS_CONN_BT:
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
    XP_U16 nAddrRecs;
    AddressRecord** prevsAddrNext;
    MsgQueueElem** prevsQueueNext;
    short i;

    isServer = stream_getU8( stream );
    comms = comms_make( MPPARM(mpool) util, isServer, sendproc, closure );

    comms->connID = stream_getU32( stream );
    comms->nextChannelNo = stream_getU16( stream );
    comms->myHostID = stream_getU16( stream );
    comms->cookieID = stream_getU16( stream );

#ifdef DEBUG
    comms->nUniqueBytes = stream_getU16( stream );
#endif
    comms->queueLen = stream_getU8( stream );

    nAddrRecs = stream_getU8( stream );
    prevsAddrNext = &comms->recs;
    for ( i = 0; i < nAddrRecs; ++i ) {
        AddressRecord* rec = (AddressRecord*)XP_MALLOC( mpool, sizeof(*rec));
        CommsAddrRec* addr;
        XP_MEMSET( rec, 0, sizeof(*rec) );

        addr = &rec->addr;
        addrFromStream( addr, stream );

        rec->nextMsgID = stream_getU16( stream );
        rec->lastMsgReceived = stream_getU16( stream );
        rec->channelNo = stream_getU16( stream );
        rec->hostID = stream_getU16( stream );

#ifdef DEBUG
        rec->lastACK = stream_getU16( stream );
        rec->nUniqueBytes = stream_getU16( stream );
#endif

        rec->next = (AddressRecord*)NULL;
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

    addrFromStream( &comms->addr, stream );

#ifdef DEBUG
    XP_ASSERT( stream_getU32( stream ) == cEND );
#endif

    return comms;
} /* comms_makeFromStream */

void
comms_init( CommsCtxt* comms )
{
    if ( comms->addr.conType == COMMS_CONN_RELAY ) {
        comms->relayState = COMMS_RELAYSTATE_UNCONNECTED;
        relayConnect( comms );
    }
}

static void
addrToStream( XWStreamCtxt* stream, CommsAddrRec* addrP )
{
    CommsAddrRec addr;
    XP_MEMCPY( &addr, addrP, sizeof(addr) );

    stream_putBits( stream, 3, addr.conType );

    switch( addr.conType ) {
#ifdef DEBUG
    case COMMS_CONN_UNUSED:
    case LAST_____FOO:
        XP_ASSERT( 0 );
        break;
#endif
    case COMMS_CONN_BT:
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
comms_writeToStream( CommsCtxt* comms, XWStreamCtxt* stream )
{
    XP_U16 nAddrRecs;
    AddressRecord* rec;
    MsgQueueElem* msg;

    stream_putU8( stream, (XP_U8)comms->isServer );

    stream_putU32( stream, comms->connID );
    stream_putU16( stream, comms->nextChannelNo );
    stream_putU16( stream, comms->myHostID );
    stream_putU16( stream, comms->cookieID );

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
        stream_putU16( stream, rec->hostID );
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

    addrToStream( stream, &comms->addr );

#ifdef DEBUG
    stream_putU32( stream, cEND );
#endif
} /* comms_writeToStream */

void
comms_getAddr( CommsCtxt* comms, CommsAddrRec* addr )
{
    XP_ASSERT( !!comms );
    XP_MEMCPY( addr, &comms->addr, sizeof(*addr) );
} /* comms_getAddr */

void
comms_setAddr( CommsCtxt* comms, CommsAddrRec* addr )
{
#ifdef BEYOND_IR
    util_addrChange( comms->util, &comms->addr, addr );
#endif
    XP_MEMCPY( &comms->addr, addr, sizeof(comms->addr) );

    /* We should now have a cookie so we can connect??? */
    relayConnect( comms );
} /* comms_setAddr */

#ifdef BEYOND_IR
void
comms_getInitialAddr( CommsAddrRec* addr )
{ 	 
    /* default values; default is still IR where there's a choice */ 	 
    addr->conType = COMMS_CONN_RELAY; 	 
    addr->u.ip_relay.ipAddr = 0L; /* force 'em to set it */ 	 
    addr->u.ip_relay.port = 10999; 	 
    { 	 
        char* name = "eehouse.org"; 	 
        XP_MEMCPY( addr->u.ip_relay.hostName, name, XP_STRLEN(name)+1 ); 	 
    } 	 
    addr->u.ip_relay.cookie[0] = '\0'; 	 
} /* comms_getInitialAddr */
#endif

CommsConnType 
comms_getConType( CommsCtxt* comms )
{
    return comms->addr.conType;
} /* comms_getConType */

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

    XP_DEBUGF( "assigning msgID of " XP_LD " on chnl %d", msgID, channelNo );

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
        XP_STATUSF( "\t%d: channel: %d; msgID: " XP_LD,
                    i+1, elem->channelNo, elem->msgID );
    }
}
#else
#define printQueue(foo)
#endif
/* We've received on some channel a message with a certain ID.  This means
 * that all messages sent on that channel with lower IDs have been received
 * and can be removed from our queue.
 */
static void
removeFromQueue( CommsCtxt* comms, XP_PlayerAddr channelNo, MsgID msgID )
{
    MsgQueueElem* elem;
    MsgQueueElem* prev;

    XP_STATUSF( "looking to remove msgs prior or equal to " XP_LD 
                " for channel %d (queue len now %d)",
		msgID, channelNo, comms->queueLen );

    for ( prev = (MsgQueueElem*)NULL, elem = comms->msgQueueHead; 
	  !!elem; prev = elem, elem = elem->next ) {

	/* remove the 0-channel message if we've established a channel number.
	   Only clients should have any 0-channel messages in the queue, and
	   receiving something from the server is an implicit ACK */

	if ( elem->channelNo == 0 && channelNo != 0 ) {
	    XP_ASSERT( !comms->isServer );      /* I've seen this fail once */
	    XP_ASSERT( elem->msgID == 0 );	/* will the check below pass? */
	} else if ( elem->channelNo != channelNo ) {
	    continue;
	}

	if ( elem->msgID <= msgID ) {

	    if ( !prev ) {	/* it's the first element */
		comms->msgQueueHead = elem->next;
		prev = comms->msgQueueHead; /* so elem=prev below will work */
	    } else {
		prev->next = elem->next;
	    }

	    if ( comms->msgQueueTail == elem ) {
		comms->msgQueueTail = prev;
	    }

	    XP_FREE( comms->mpool, elem->msg );
	    XP_FREE( comms->mpool, elem );
	    elem = prev;
	    --comms->queueLen;

	    if ( !elem ) {
		break;
	    }
	}
    }
    XP_STATUSF( "removeFromQueue: queueLen now %d", comms->queueLen );

    XP_ASSERT( comms->queueLen > 0 || comms->msgQueueHead == NULL );

    printQueue( comms );
} /* removeFromQueue */

static XP_S16
sendMsg( CommsCtxt* comms, MsgQueueElem* elem )
{
    XP_S16 result = 0;
    XP_PlayerAddr channelNo;
    CommsAddrRec* addr;

    channelNo = elem->channelNo;

    if ( comms_getConType( comms ) == COMMS_CONN_RELAY ) {
        if ( comms->relayState == COMMS_RELAYSTATE_CONNECTED ) {
            XWHostID destID = getDestID( comms, channelNo );
            result = send_via_relay( comms, XWRELAY_MSG_TORELAY, destID, 
                                     elem->msg, elem->len );
        } else {
            XP_LOGF( "skipping message: not connected" );
        }
    } else {

        if ( !channelToAddress( comms, channelNo, &addr ) ) {
            addr = NULL;
        }

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

    for ( msg = comms->msgQueueHead; !!msg; msg = msg->next ) {
        XP_S16 oneResult = sendMsg( comms, msg );
        if ( result == 0 && oneResult != 0 ) {
            result = oneResult;
        }
        XP_STATUSF( "resend: msgid=" XP_LD "; rslt=%d", msg->msgID, oneResult );
    }

    return result;
} /* comms_resend */

static XP_Bool
relayPreProcess( CommsCtxt* comms, XWStreamCtxt* stream, XWHostID* senderID )
{
    XP_Bool consumed;
    XWHostID destID, srcID;
    XP_U16 cookieID;

    if ( comms->addr.conType != COMMS_CONN_RELAY ) {
        consumed = XP_FALSE;         /* nothing for us to do here! */
    } else {
        XWRELAY_Cmd cmd = stream_getU8( stream );
        switch( cmd ) {
        case XWRELAY_CONNECTRESP:
            consumed = XP_TRUE;
            comms->relayState = COMMS_RELAYSTATE_CONNECTED;
            comms->heartbeat = stream_getU16( stream );
            comms->cookieID = stream_getU16( stream );
            XP_LOGF( "got XWRELAY_CONNECTRESP; set cookieID = %d",
                     comms->cookieID );
            /* We're connected now.  Send any pending messages.  This may need
               to be done later since we're inside the platform's socket read
               proc now. */
            comms_resendAll( comms );

            setHeartbeatTimer( comms );
            break;
        case XWRELAY_MSG_FROMRELAY:
            cookieID = stream_getU16( stream );
            srcID = stream_getU16( stream );
            destID = stream_getU16( stream );
            XP_LOGF( "cookieID: %d; srcID: %d; destID: %d",
                     cookieID, srcID, destID );
            /* If these values don't check out, drop it */
            consumed = cookieID != comms->cookieID
                && destID != comms->myHostID;
            if ( consumed ) {
                XP_LOGF( "rejecting data message" );
            } else {
                *senderID = srcID;
            }
            break;
        default:
            consumed = XP_TRUE; /* drop it */
            XP_LOGF( "dropping relay msg with cmd %d", (XP_U16)cmd );
        }
    
    }
    return consumed;
} /* checkForRelay */

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
                           CommsAddrRec* addr )
{
    XP_U16 channelNo;
    XP_U32 connID;
    MsgID msgID;
    MsgID lastMsgRcd;
    XP_Bool validMessage = XP_TRUE;
    AddressRecord* recs = (AddressRecord*)NULL;
    XWHostID senderID;

    XP_ASSERT( addr == NULL || comms->addr.conType == addr->conType );

    if ( relayPreProcess( comms, stream, &senderID ) ) {
        return XP_FALSE;
    }

    connID = stream_getU32( stream );
    XP_STATUSF( "read connID of %lx", connID );

    if ( comms->connID == connID || comms->connID == CONN_ID_NONE ) {

        channelNo = stream_getU16( stream );
        msgID = stream_getU32( stream );
        lastMsgRcd = stream_getU32( stream );

        XP_DEBUGF( "rcd: msg " XP_LD " on chnl %d", msgID, channelNo );

        removeFromQueue( comms, channelNo, lastMsgRcd );

        if ( channelNo == 0 ) {
            XP_ASSERT( comms->isServer );
            XP_ASSERT( msgID == 0 );
            channelNo = ++comms->nextChannelNo;
            XP_STATUSF( "assigning channelNo=%d", channelNo );
        } else {
            recs = getRecordFor( comms, channelNo );	
            /* messageID for an incomming message should be one greater than
             * the id most recently used for that channel. */
            if ( !!recs && (msgID != recs->lastMsgReceived + 1)  ) {
                XP_DEBUGF( "unex msgID " XP_LD " (expt " XP_LD ")",
                           msgID, recs->lastMsgReceived+1 );
                validMessage = XP_FALSE;
            }
#ifdef DEBUG
            if ( !!recs ) {
                XP_ASSERT( lastMsgRcd < 0x0000FFFF );
                recs->lastACK = (XP_U16)lastMsgRcd;
            }
#endif
        }
    
        if ( validMessage ) {
            XP_LOGF( "remembering senderID %x for channel %d",
                     senderID, channelNo );

            recs = rememberChannelAddress( comms, channelNo, senderID, addr );
            stream_setAddress( stream, channelNo );

            if ( !!recs ) {
                recs->lastMsgReceived = msgID;
            }
            XP_STATUSF( "set channel %d's lastMsgReceived to " XP_LD,
                        channelNo, msgID );
        }
    } else {
        validMessage = XP_FALSE;
        XP_STATUSF( "refusing non-matching connID; got %lx, wanted %lx",
                    connID, comms->connID );
    }
    return validMessage;
} /* comms_checkIncomingStream */

static void
p_comms_timerFired( void* closure, XWTimerReason why )
{
    CommsCtxt* comms = (CommsCtxt*)closure;
    XP_ASSERT( why == TIMER_HEARTBEAT );
    XP_LOGF( "comms_timerFired" );
    if ( comms->heartbeat != HEARTBEAT_NONE ) {
        send_via_relay( comms, XWRELAY_HEARTBEAT, HOST_ID_NONE, NULL, 0 );
        /* No need to reset timer.  send_via_relay does that. */
    }
} /* comms_timerFired */

static void
setHeartbeatTimer( CommsCtxt* comms )
{
    util_setTimer( comms->util, TIMER_HEARTBEAT, comms->heartbeat,
                   p_comms_timerFired, comms );
}

#ifdef DEBUG
void
comms_getStats( CommsCtxt* comms, XWStreamCtxt* stream )
{
    XP_UCHAR buf[100];
    AddressRecord* rec;

    XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf), (XP_UCHAR*)"msg queue len: %d\n", 
                 comms->queueLen );
    stream_putBytes( stream, buf, (XP_U16)XP_STRLEN( buf ) );

    XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf), 
                 (XP_UCHAR*)"channel-less bytes sent: %d\n", 
                 comms->nUniqueBytes );
    stream_putBytes( stream, buf, (XP_U16)XP_STRLEN( buf ) );

    for ( rec = comms->recs; !!rec; rec = rec->next ) {
        XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf), 
                     (XP_UCHAR*)"  Stats for channel: %d\n", 
                     rec->channelNo );
        stream_putBytes( stream, buf, (XP_U16)XP_STRLEN( buf ) );

        XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf), 
                     (XP_UCHAR*)"Last msg sent: " XP_LD "\n", 
                     rec->nextMsgID );
        stream_putBytes( stream, buf, (XP_U16)XP_STRLEN( buf ) );

        XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf), 
                     (XP_UCHAR*)"Unique bytes sent: %d\n", 
                     rec->nUniqueBytes );
        stream_putBytes( stream, buf, (XP_U16)XP_STRLEN( buf ) );

        XP_SNPRINTF( (XP_UCHAR*)buf, sizeof(buf), 
                     (XP_UCHAR*)"Last message acknowledged: %d\n", 
                     rec->lastACK);
        stream_putBytes( stream, buf, (XP_U16)XP_STRLEN( buf ) );
    }
} /* comms_getStats */
#endif

static AddressRecord*
rememberChannelAddress( CommsCtxt* comms, XP_PlayerAddr channelNo, 
                        XWHostID hostID, CommsAddrRec* addr )
{
    AddressRecord* recs = NULL;
    recs = getRecordFor( comms, channelNo );
    if ( !recs ) {
        /* not found; add a new entry */
        recs = (AddressRecord*)XP_MALLOC( comms->mpool, sizeof(*recs) );

        recs->nextMsgID = 0;
        recs->channelNo = channelNo;
        recs->hostID = hostID;
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
            XP_ASSERT( recs->hostID == hostID );
        } else {
            XP_MEMSET( &recs->addr, 0, sizeof(recs->addr) );
        }
    }
    return recs;
} /* rememberChannelAddress */

static XP_Bool
channelToAddress( CommsCtxt* comms, XP_PlayerAddr channelNo, 
                  CommsAddrRec** addr )
{
    AddressRecord* recs = getRecordFor( comms, channelNo );

    if ( !!recs ) {
        *addr = &recs->addr;
        return XP_TRUE;
    } else {
        return XP_FALSE;
    }
} /* channelToAddress */

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
                id = recs->hostID;
            }
        }
    }
    XP_LOGF( "getDestID(%d) => %x", channelNo, id );
    return id;
} /* getDestID */

static AddressRecord* 
getRecordFor( CommsCtxt* comms, XP_PlayerAddr channelNo )
{
    AddressRecord* recs;

    if ( channelNo == CHANNEL_NONE ) {
        return (AddressRecord*)NULL;
    }

    for ( recs = comms->recs; !!recs; recs = recs->next ) {
        if ( recs->channelNo == channelNo ) {
            return recs;
        }
    }
    return (AddressRecord*)NULL;
} /* getRecordFor */

static XP_U16
countAddrRecs( CommsCtxt* comms )
{
    short count = 0;
    AddressRecord* recs;
    for ( recs = comms->recs; !!recs; recs = recs->next ) {
        ++count;
    } 
    return count;
} /* countAddrRecs */

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

        if ( cmd == XWRELAY_MSG_TORELAY ) {
            stream_putU16( tmpStream, comms->cookieID );
            stream_putU16( tmpStream, comms->myHostID );
            stream_putU16( tmpStream, destID );
            if ( data != NULL && dlen > 0 ) {
                stream_putBytes( tmpStream, data, dlen );
            }
        } else if ( cmd == XWRELAY_CONNECT ) {
            XP_U8 clen;
            clen = XP_STRLEN( addr.u.ip_relay.cookie );
            stream_putU8( tmpStream, clen );
            stream_putBytes( tmpStream, addr.u.ip_relay.cookie, clen );
            stream_putU16( tmpStream, comms->myHostID );
            XP_LOGF( "writing cookieID of %d", comms->cookieID );
            stream_putU16( tmpStream, comms->cookieID );

            comms->relayState = COMMS_RELAYSTATE_CONNECT_PENDING;
        } else if ( cmd == XWRELAY_HEARTBEAT ) {
            /* Add these for grins.  Server can assert they match the IP
               address it expects 'em on. */
            stream_putU16( tmpStream, comms->cookieID );
            stream_putU16( tmpStream, comms->myHostID );
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
    XP_LOGF( "relayConnect called" );
    if ( !comms->connecting ) {
        comms->connecting = XP_TRUE;
        send_via_relay( comms, XWRELAY_CONNECT, HOST_ID_NONE, NULL, 0 );
        comms->connecting = XP_FALSE;
    }
} /* relayConnect */


EXTERN_C_END

#endif /* #ifndef XWFEATURE_STANDALONE_ONLY */
