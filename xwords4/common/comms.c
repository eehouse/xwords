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
#include "xwstream.h"
#include "memstream.h"

#define cEND 0x65454e44

#ifndef XWFEATURE_STANDALONE_ONLY

typedef struct MsgQueueElem {
    struct MsgQueueElem* next;
    XP_U8* msg;
    XP_U16 len;
    XP_U16 channelNo;
    MsgID msgID;
} MsgQueueElem;

typedef struct AddressRecord {
    struct AddressRecord* next;
    CommsAddrRec addr;
#ifdef DEBUG
    XP_U16 lastACK;
    XP_U16 nUniqueBytes;
#endif
    MsgID nextMsgID;		/* on a per-channel basis */
    MsgID lastMsgReceived;	/* on a per-channel basis */
    XP_PlayerAddr channelNo;
} AddressRecord;

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

    /* added to stream format.  Must deal with format change in saved
       games. PENDING */
    XP_U16 listenPort;
    CommsAddrRec addr;

    XP_Bool isServer;
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
                                              CommsAddrRec* addr );
static XP_Bool channelToAddress( CommsCtxt* comms, XP_PlayerAddr channelNo, 
                                 CommsAddrRec** addr );
static AddressRecord* getRecordFor( CommsCtxt* comms, 
                                    XP_PlayerAddr channelNo );
static XP_S16 sendMsg( CommsCtxt* comms, MsgQueueElem* elem );
static void addToQueue( CommsCtxt* comms, MsgQueueElem* newMsgElem );
static XP_U16 countAddrRecs( CommsCtxt* comms );

/****************************************************************************
 *                               implementation 
 ****************************************************************************/
CommsCtxt* 
comms_make( MPFORMAL XW_UtilCtxt* util, XP_Bool isServer, 
            TransportSend sendproc, void* closure )
{
    CommsCtxt* result = (CommsCtxt*)XP_MALLOC( mpool, sizeof(*result) );
    XP_MEMSET( result, 0, sizeof(*result) );

    MPASSIGN(result->mpool, mpool);

    result->isServer = isServer;
    result->sendproc = sendproc;
    result->sendClosure = closure;
    result->util = util;

#ifdef BEYOND_IR
    /* default values; default is still IR where there's a choice */
    result->addr.conType = COMMS_CONN_IR;
    result->addr.u.ip.ipAddr = 0L; /* force 'em to set it */
    result->addr.u.ip.port = 6000;
    result->listenPort = 6001;
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
        addr->conType = stream_getU8( stream );
        if ( addr->conType == COMMS_CONN_IP ) {
            addr->u.ip.ipAddr = stream_getU32( stream );
            addr->u.ip.port = stream_getU16( stream );
        }
	
        rec->nextMsgID = stream_getU16( stream );
        rec->lastMsgReceived = stream_getU16( stream );
        rec->channelNo = stream_getU16( stream );

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

#ifdef BEYOND_IR
    comms->addr.conType = stream_getBits( stream, 3 );
    comms->addr.u.ip.ipAddr = stream_getU32( stream );
    comms->addr.u.ip.port = stream_getU16( stream );
    comms->listenPort = stream_getU16( stream );
    /* tell client about the port */
    util_listenPortChange( util, comms->listenPort );
#endif

#ifdef DEBUG
    XP_ASSERT( stream_getU32( stream ) == cEND );
#endif
    return comms;
} /* comms_makeFromStream */

void
comms_writeToStream( CommsCtxt* comms, XWStreamCtxt* stream )
{
    XP_U16 nAddrRecs;
    AddressRecord* rec;
    MsgQueueElem* msg;

    stream_putU8( stream, (XP_U8)comms->isServer );
    stream_putU32( stream, comms->connID );
    stream_putU16( stream, comms->nextChannelNo );
#ifdef DEBUG
    stream_putU16( stream, comms->nUniqueBytes );
#endif

    XP_ASSERT( comms->queueLen <= 255 );
    stream_putU8( stream, (XP_U8)comms->queueLen );

    nAddrRecs = countAddrRecs(comms);
    stream_putU8( stream, (XP_U8)nAddrRecs );

    for ( rec = comms->recs; !!rec; rec = rec->next ) {
        CommsAddrRec* addr = &rec->addr;
        stream_putU8( stream, addr->conType );
        if ( rec->addr.conType == COMMS_CONN_IP ) {
            stream_putU32( stream, addr->u.ip.ipAddr );
            stream_putU16( stream, addr->u.ip.port );
        }

        stream_putU16( stream, (XP_U16)rec->nextMsgID );
        stream_putU16( stream, (XP_U16)rec->lastMsgReceived );
        stream_putU16( stream, rec->channelNo );
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

#ifdef BEYOND_IR
    stream_putBits( stream, 3, comms->addr.conType );
    stream_putU32( stream, comms->addr.u.ip.ipAddr );
    stream_putU16( stream, comms->addr.u.ip.port );
    stream_putU16( stream, comms->listenPort );
#endif

#ifdef DEBUG
    stream_putU32( stream, cEND );
#endif
} /* comms_writeToStream */

void
comms_getAddr( CommsCtxt* comms, CommsAddrRec* addr, XP_U16* listenPort )
{
    XP_MEMCPY( addr, &comms->addr, sizeof(*addr) );
    *listenPort = comms->listenPort;
} /* comms_getAddr */

void
comms_setAddr( CommsCtxt* comms, CommsAddrRec* addr, XP_U16 listenPort )
{
    XP_MEMCPY( &comms->addr, addr, sizeof(comms->addr) );
    comms->listenPort = listenPort;
#ifdef BEYOND_IR
    util_listenPortChange( comms->util, listenPort );
#endif
} /* comms_setAddr */

CommsConnType 
comms_getConType( CommsCtxt* comms )
{
    return comms->addr.conType;
} /* comms_getConType */

/* Send a message using the sequentially next MsgID.  Save the message so
 * resend can work. */
XP_S16
comms_send( CommsCtxt* comms, CommsConnType conType, XWStreamCtxt* stream )
{
    XP_U16 streamSize = stream_getSize( stream );
    XP_U16 headerLen;
    XP_PlayerAddr channelNo = stream_getAddress( stream );
    AddressRecord* rec = getRecordFor( comms, channelNo );
    MsgID msgID = (!!rec)? ++rec->nextMsgID : 0;
    MsgID lastMsgRcd = (!!rec)? rec->lastMsgReceived : 0;
    MsgQueueElem* newMsgElem;
    XWStreamCtxt* msgStream;

    XP_DEBUGF( "assigning msgID of %ld on chnl %d", msgID, channelNo );

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

#ifdef BEYOND_IR
    if ( conType == COMMS_CONN_IP ) {
        stream_putU16( msgStream, comms->listenPort );
        XP_LOGF( "wrote return port to stream: %d", comms->listenPort );
    }
#endif

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
        XP_STATUSF( "\t%d: channel: %d; msgID: %ld",
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

    XP_STATUSF( "looking to remove msgs prior or equal to %ld for channel %d "
		"(queue len now %d)",
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
    XP_S16 result;
    XP_PlayerAddr channelNo;
    CommsAddrRec* addr;

    channelNo = elem->channelNo;

    if ( !channelToAddress( comms, channelNo, &addr ) ) {
        addr = NULL;
    }

    XP_ASSERT( !!comms->sendproc );
    result = (*comms->sendproc)( elem->msg, elem->len, addr,
                                 comms->sendClosure );
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
        XP_STATUSF( "resend: msgid=%ld; rslt=%d", msg->msgID, oneResult );
    }

    return result;
} /* comms_resend */

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
comms_checkIncommingStream( CommsCtxt* comms, XWStreamCtxt* stream, 
                            CommsAddrRec* addr )
{
    XP_U16 channelNo;
    XP_U32 connID;
    MsgID msgID;
    MsgID lastMsgRcd;
    XP_Bool validMessage = XP_TRUE;
    AddressRecord* recs = (AddressRecord*)NULL;

    connID = stream_getU32( stream );
    XP_STATUSF( "read connID of %lx", connID );

#ifdef BEYOND_IR
    if ( addr->conType == COMMS_CONN_IP ) {
        addr->u.ip.port = stream_getU16( stream );
        XP_LOGF( "read return port from stream: %d", addr->u.ip.port );
    }
#endif

    if ( comms->connID == connID || comms->connID == CONN_ID_NONE ) {

        channelNo = stream_getU16( stream );
        msgID = stream_getU32( stream );
        lastMsgRcd = stream_getU32( stream );

        XP_DEBUGF( "rcd: msg %ld on chnl %d", msgID, channelNo );

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
                XP_DEBUGF( "unex msgID %ld (expt %ld)",
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
            recs = rememberChannelAddress( comms, channelNo, addr );

            stream_setAddress( stream, channelNo );

            if ( !!recs ) {
                recs->lastMsgReceived = msgID;
            }
            XP_STATUSF( "set channel %d's lastMsgReceived to %ld",
                        channelNo, msgID );
        }
    } else {
        validMessage = XP_FALSE;
        XP_STATUSF( "refusing non-matching connID; got %lx, wanted %lx",
                    connID, comms->connID );
    }
    return validMessage;
} /* comms_checkIncommingStream */

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
                     (XP_UCHAR*)"Last msg sent: %ld\n", 
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
                        CommsAddrRec* addr )
{
    AddressRecord* recs = NULL;
    if ( addr != NULL ) {
        recs = getRecordFor( comms, channelNo );
        if ( !!recs ) {
            /* Looks as if this will overwrite the address each time a new
               message comes in.  I *guess* that's right... */
            XP_MEMCPY( &recs->addr, addr, sizeof(recs->addr) );
        } else {
            /* not found; add a new entry */
            recs = (AddressRecord*)XP_MALLOC( comms->mpool, sizeof(*recs) );

            recs->nextMsgID = 0;
            recs->channelNo = channelNo;
            XP_MEMCPY( &recs->addr, addr, sizeof(recs->addr) );
#ifdef DEBUG
            recs->nUniqueBytes = 0;
#endif
            recs->next = comms->recs;
            comms->recs = recs;
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

#endif /* #ifndef XWFEATURE_STANDALONE_ONLY */
