/* -*- compile-command: "cd ../linux && make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 2020 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <endian.h>
#include <inttypes.h>

#include "device.h"
#include "comtypes.h"
#include "memstream.h"
#include "xwstream.h"
#include "strutils.h"
#include "nli.h"
#include "dbgutil.h"

static XWStreamCtxt*
mkStream( XW_DUtilCtxt* dutil )
{
    XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(dutil->mpool)
                                                dutil_getVTManager(dutil) );
    return stream;
}

#ifdef XWFEATURE_DEVICE

typedef struct _DevCtxt {
    XP_U16 devCount;
} DevCtxt;

static DevCtxt*
load( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    DevCtxt* state = (DevCtxt*)dutil->devCtxt;
    if ( NULL == state ) {
        XWStreamCtxt* stream = mkStream( dutil );
        const XP_UCHAR* keys[] = { KEY_DEVSTATE, NULL };
        dutil_loadStream( dutil, xwe, keys, stream );

        state = XP_CALLOC( dutil->mpool, sizeof(*state) );
        dutil->devCtxt = state;

        if ( 0 < stream_getSize( stream ) ) {
            state->devCount = stream_getU16( stream );
            ++state->devCount;  /* for testing until something's there */
            /* XP_LOGF( "%s(): read devCount: %d", __func__, state->devCount ); */
        } else {
            XP_LOGF( "%s(): empty stream!!", __func__ );
        }
        stream_destroy( stream, NULL );
    }

    return state;
}

void
dvc_store( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    DevCtxt* state = load( dutil, xwe );
    XWStreamCtxt* stream = mkStream( dutil );
    stream_putU16( stream, state->devCount );
    const XP_UCHAR* keys[] = { KEY_DEVSTATE, NULL };
    dutil_storeStream( dutil, xwe, keys, stream );
    stream_destroy( stream, NULL );

    XP_FREEP( dutil->mpool, &dutil->devCtxt );
}

#endif

// #define BOGUS_ALL_SAME_DEVID
#define NUM_RUNS 1              /* up this to see how random things look */

static void
getMQTTDevID( XW_DUtilCtxt* dutil, XWEnv xwe, XP_Bool forceNew, MQTTDevID* devID )
{
    const XP_UCHAR* keys[] = { MQTT_DEVID_KEY, NULL };
#ifdef BOGUS_ALL_SAME_DEVID
    XP_USE(forceNew);
    MQTTDevID bogusID = 0;
    XP_UCHAR* str = "ABCDEF0123456789";
    XP_Bool ok = strToMQTTCDevID( str, &bogusID );
    XP_ASSERT( ok );

    MQTTDevID tmp = 0;
    XP_U32 len = sizeof(tmp);
    dutil_loadPtr( dutil, xwe, keys, &tmp, &len );
    if ( len != sizeof(tmp) || 0 != XP_MEMCMP( &bogusID, &tmp, sizeof(tmp) ) ) {
        dutil_storePtr( dutil, xwe, keys, &bogusID, sizeof(bogusID) );
    }
    *devID = bogusID;

#else

    MQTTDevID tmp = 0;
    XP_U32 len = sizeof(tmp);
    if ( !forceNew ) {
        dutil_loadPtr( dutil, xwe, keys, &tmp, &len );
    }

    /* XP_LOGFF( "len: %d; sizeof(tmp): %zu", len, sizeof(tmp) ); */
    if ( forceNew || len != sizeof(tmp) ) { /* not found, or bogus somehow */
        int total = 0;
        for ( int ii = 0; ii < NUM_RUNS; ++ii ) {
            tmp = XP_RANDOM();
            tmp <<= 27;
            tmp ^= XP_RANDOM();
            tmp <<= 27;
            tmp ^= XP_RANDOM();

            int count = 0;
            MQTTDevID tmp2 = tmp;
            while ( 0 != tmp2 ) {
                if ( 0 != (1 & tmp2) ) {
                    ++count;
                    ++total;
                }
                tmp2 >>= 1;
            }
            XP_LOGFF( "got: %" PRIX64 " (set: %d/%zd)", tmp, count, sizeof(tmp2)*8 );
        }
        XP_LOGFF( "average bits set: %d", total / NUM_RUNS );

        dutil_storePtr( dutil, xwe, keys, &tmp, sizeof(tmp) );

# ifdef DEBUG
        XP_UCHAR buf[32];
        formatMQTTDevID( &tmp, buf, VSIZE(buf) );
        /* This log statement is required by discon_ok2.py!!! (keep in sync) */
        XP_LOGFF( "generated id: %s; key: %s", buf, MQTT_DEVID_KEY );
# endif
    }
    *devID = tmp;
#endif
    // LOG_RETURNF( MQTTDevID_FMT " key: %s", *devID, MQTT_DEVID_KEY );
}

void
dvc_getMQTTDevID( XW_DUtilCtxt* dutil, XWEnv xwe, MQTTDevID* devID )
{
    getMQTTDevID( dutil, xwe, XP_FALSE, devID );
}

void
dvc_resetMQTTDevID( XW_DUtilCtxt* dutil, XWEnv xwe )
{
#ifdef BOGUS_ALL_SAME_DEVID
    XP_LOGFF( "doing nothing" );
    XP_USE( dutil );
    XP_USE( xwe );
#else
    MQTTDevID ignored;
    getMQTTDevID( dutil, xwe, XP_TRUE, &ignored );
#endif
}

static XP_UCHAR*
appendToStorage( XP_UCHAR* storage, int* offset,
                 const XP_UCHAR* str )
{
    XP_UCHAR* start = &storage[*offset];
    start[0] = '\0';
    XP_STRCAT( start, str );
    *offset += 1 + XP_STRLEN(str);
    return start;
}

#ifdef DEBUG
static void
logPtrs( const char* func, int nTopics, char* topics[] )
{
    for ( int ii = 0; ii < nTopics; ++ii ) {
        XP_LOGFF( "from %s; topics[%d] = %s", func, ii, topics[ii] );
    }
}
#else
# define logPtrs(func, nTopics, topics)
#endif

void
dvc_getMQTTSubTopics( XW_DUtilCtxt* dutil, XWEnv xwe,
                      XP_UCHAR* storage, XP_U16 XP_UNUSED_DBG(storageLen),
                      XP_U16* nTopics, XP_UCHAR* topics[] )
{
    LOG_FUNC();
    int offset = 0;
    XP_U16 count = 0;
    storage[0] = '\0';

    MQTTDevID devid;
    getMQTTDevID( dutil, xwe, XP_FALSE, &devid );
    XP_UCHAR buf[64];
    formatMQTTDevTopic( &devid, buf, VSIZE(buf) );

#ifdef MQTT_DEV_TOPICS
    /* First, the main device topic */
    topics[count++] = appendToStorage( storage, &offset, buf );
#endif

#ifdef MQTT_GAMEID_TOPICS
    /* Then the pattern that includes gameIDs */
    XP_UCHAR buf2[64];
    size_t siz = XP_SNPRINTF( buf2, VSIZE(buf2), "%s/+", buf );
    XP_ASSERT( siz < VSIZE(buf) );
    XP_USE(siz);
    topics[count++] = appendToStorage( storage, &offset, buf2 );
#endif

    /* Finally, the control pattern */
    formatMQTTCtrlTopic( &devid, buf, VSIZE(buf) );
    topics[count++] = appendToStorage( storage, &offset, buf );

    for ( int ii = 0; ii < count; ++ii ) {
        XP_LOGFF( "AFTER: got %d: %s", ii, topics[ii] );
    }

    XP_ASSERT( count <= *nTopics );
    *nTopics = count;
    XP_ASSERT( offset < storageLen );

    logPtrs( __func__, *nTopics, topics );

    LOG_RETURN_VOID();
}

typedef enum { CMD_INVITE, CMD_MSG, CMD_DEVGONE, } MQTTCmd;

// #define PROTO_0 0
#define PROTO_1 1        /* moves gameID into "header" relay2 knows about */
#define PROTO_2 2        /* adds timestamp to header */
#define PROTO_3 3        /* adds multi-message, removes gameID */
#ifndef MQTT_USE_PROTO
# define MQTT_USE_PROTO PROTO_1
#endif

static void
addHeaderGameIDAndCmd( XW_DUtilCtxt* dutil, XWEnv xwe, MQTTCmd cmd,
                       XP_U32 gameID, XP_U32 timestamp, XWStreamCtxt* stream )
{
    stream_putU8( stream, MQTT_USE_PROTO );

    MQTTDevID myID;
    dvc_getMQTTDevID( dutil, xwe, &myID );
    myID = htobe64( myID );
    stream_putBytes( stream, &myID, sizeof(myID) );

    stream_putU32( stream, gameID );
    if ( PROTO_2 <= MQTT_USE_PROTO ) {
        XP_ASSERT(0 != timestamp);
        stream_putU32( stream, timestamp );
    }

    stream_putU8( stream, cmd );
}

#ifdef MQTT_GAMEID_TOPICS
static void
addProto3HeaderCmd( XW_DUtilCtxt* dutil, XWEnv xwe, MQTTCmd cmd,
                    XP_U32 timestamp, XWStreamCtxt* stream )
{
    XP_ASSERT( 0 != timestamp );
    stream_putU8( stream, PROTO_3 );

    MQTTDevID myID;
    dvc_getMQTTDevID( dutil, xwe, &myID );
    myID = htobe64( myID );
    stream_putBytes( stream, &myID, sizeof(myID) );

    stream_putU32( stream, timestamp );

    stream_putU8( stream, cmd );
}
#endif

static void
callProc( MsgAndTopicProc proc, void* closure, const XP_UCHAR* topic, XWStreamCtxt* stream )
{
    (*proc)( closure, topic, stream_getPtr(stream), stream_getSize(stream) );
}

void
dvc_makeMQTTInvites( XW_DUtilCtxt* dutil, XWEnv xwe,
                     MsgAndTopicProc proc, void* closure,
                     const MQTTDevID* addressee,
                     const NetLaunchInfo* nli,
                     XP_U32 timestamp )
{
    XP_UCHAR devTopic[64];      /* used by two below */
    formatMQTTDevTopic( addressee, devTopic, VSIZE(devTopic) );
    /* Stream format is identical for both topics */
    XWStreamCtxt* stream = mkStream( dutil );
    addHeaderGameIDAndCmd( dutil, xwe, CMD_INVITE, nli->gameID,
                           timestamp, stream );
    nli_saveToStream( nli, stream );

#ifdef MQTT_DEV_TOPICS
    callProc( proc, closure, devTopic, stream );
#endif

#ifdef MQTT_GAMEID_TOPICS
    XP_UCHAR gameTopic[64];
    size_t siz = XP_SNPRINTF( gameTopic, VSIZE(gameTopic),
                              "%s/%X", devTopic, nli->gameID );
    XP_ASSERT( siz < VSIZE(gameTopic) );
    XP_USE(siz);
    callProc( proc, closure, devTopic, stream );
#endif

    stream_destroy( stream, xwe );
}

/* Ship with == 1, but increase to test */
#define MULTI_MSG_COUNT 1

void
dvc_makeMQTTMessages( XW_DUtilCtxt* dutil, XWEnv xwe,
                      MsgAndTopicProc proc, void* closure,
                      const MQTTDevID* addressee,
                      XP_U32 gameID, XP_U32 timestamp,
                      const XP_U8* buf, XP_U16 len )
{
    XP_UCHAR devTopic[64];      /* used by two below */
    formatMQTTDevTopic( addressee, devTopic, VSIZE(devTopic) );

#ifdef MQTT_DEV_TOPICS
    {
        XWStreamCtxt* stream = mkStream( dutil );
        addHeaderGameIDAndCmd( dutil, xwe, CMD_MSG, gameID, timestamp, stream );
        if ( PROTO_2 <= MQTT_USE_PROTO ) {
            stream_putU32VL( stream, len );
        }
        stream_putBytes( stream, buf, len );
        callProc( proc, closure, devTopic, stream );
        stream_destroy( stream, xwe );
    }
#endif

#ifdef MQTT_GAMEID_TOPICS
    {
        XWStreamCtxt* stream = mkStream( dutil );
        addProto3HeaderCmd( dutil, xwe, CMD_MSG, timestamp, stream );

        /* For now, we ship one message per packet. But the receiving code
           should be ready */
        stream_putU8( stream, MULTI_MSG_COUNT );
        for ( int ii = 0; ii < MULTI_MSG_COUNT; ++ii ) {
            stream_putU32VL( stream, len );
            stream_putBytes( stream, buf, len );
        }

        XP_UCHAR gameTopic[64];
        size_t siz = XP_SNPRINTF( gameTopic, VSIZE(gameTopic),
                                  "%s/%X", devTopic, gameID );
        XP_ASSERT( siz < VSIZE(gameTopic) );
        XP_USE(siz);

        callProc( proc, closure, gameTopic, stream );
        stream_destroy( stream, xwe );
    }
#endif
}

void
dvc_makeMQTTNoSuchGames( XW_DUtilCtxt* dutil, XWEnv xwe,
                         MsgAndTopicProc proc, void* closure,
                         const MQTTDevID* addressee,
                         XP_U32 gameID, XP_U32 timestamp )
{
    XP_UCHAR devTopic[64];      /* used by two below */
    formatMQTTDevTopic( addressee, devTopic, VSIZE(devTopic) );

    if ( 0 == timestamp ) {
        timestamp = dutil_getCurSeconds( dutil, xwe );
        XP_LOGFF( "replacing timestamp of 0" );
    }

    XWStreamCtxt* stream = mkStream( dutil );
    addHeaderGameIDAndCmd( dutil, xwe, CMD_DEVGONE, gameID,
                           timestamp, stream );
#ifdef MQTT_DEV_TOPICS
    callProc( proc, closure, devTopic, stream );
#endif

#ifdef MQTT_GAMEID_TOPICS
    XP_UCHAR gameTopic[64];
    size_t siz = XP_SNPRINTF( gameTopic, VSIZE(gameTopic),
                              "%s/%X", devTopic, gameID );
    XP_ASSERT( siz < VSIZE(gameTopic) );
    XP_USE(siz);
    callProc( proc, closure, gameTopic, stream );
#endif

    stream_destroy( stream, xwe );
}

static XP_Bool
isDevMsg( const MQTTDevID* myID, const XP_UCHAR* topic, XP_U32* gameID )
{
    XP_UCHAR buf[64];
    formatMQTTDevTopic( myID, buf, VSIZE(buf) );
    size_t topicLen = XP_STRLEN(buf);
    XP_Bool success = 0 == strncmp( buf, topic, topicLen );
    if ( success ) {
        const XP_UCHAR* gameIDPart = topic + topicLen;
        sscanf( gameIDPart, "/%X", gameID );
    }
    // XP_LOGFF( "(%s) => %s (gameID=%X)", topic, boolToStr(success), *gameID );
    return success;
}

static XP_Bool
isCtrlMsg( const MQTTDevID* myID, const XP_UCHAR* topic )
{
    XP_UCHAR buf[64];
    formatMQTTCtrlTopic( myID, buf, VSIZE(buf) );
    XP_Bool success = 0 == strncmp( buf, topic, XP_STRLEN(buf) );
    XP_LOGFF( "(%s) => %s", topic, boolToStr(success) );
    return success;
}

static void
dispatchMsgs( XW_DUtilCtxt* dutil, XWEnv xwe, XP_U8 proto, XWStreamCtxt* stream,
              XP_U32 gameID, const CommsAddrRec* from )
{
    int msgCount = proto >= PROTO_3 ? stream_getU8( stream ) : 1;
    for ( int ii = 0; ii < msgCount; ++ii ) {
        XP_U32 msgLen;
        if ( PROTO_1 == proto ) {
            msgLen = stream_getSize( stream );
        } else {
            msgLen = stream_getU32VL( stream );
        }
        if ( msgLen > stream_getSize( stream ) ) {
            XP_LOGFF( "msglen %d too large", msgLen );
            msgLen = 0;
            XP_ASSERT(0);
        }
        if ( 0 < msgLen ) {
            XP_U8 msgBuf[msgLen];
            stream_getBytes( stream, msgBuf, msgLen );
            dutil_onMessageReceived( dutil, xwe, gameID,
                                     from, msgBuf, msgLen );
        }
    }
}

void
dvc_parseMQTTPacket( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* topic,
                     const XP_U8* buf, XP_U16 len )
{
    XP_LOGFF( "(topic=%s, len=%d)", topic, len );

    MQTTDevID myID;
    dvc_getMQTTDevID( dutil, xwe, &myID );

    XP_U32 gameID = 0;
    if ( isDevMsg( &myID, topic, &gameID ) ) {
        XWStreamCtxt* stream = mkStream( dutil );
        stream_putBytes( stream, buf, len );

        XP_U8 proto = 0;
        if ( stream_gotU8( stream, &proto )
             && (proto == PROTO_1 || proto == PROTO_2 || proto == PROTO_3 ) ) {
            MQTTDevID senderID;
            stream_getBytes( stream, &senderID, sizeof(senderID) );
            senderID = be64toh( senderID );
#ifdef DEBUG
            XP_UCHAR tmp[32];
            formatMQTTDevID( &senderID, tmp, VSIZE(tmp) );
            XP_LOGFF( "senderID: %s", tmp );
#endif
            if ( proto < PROTO_3 ) {
                gameID = stream_getU32( stream );
            } else {
                XP_ASSERT( 0 != gameID );
            }

            XP_U32 timestamp = 0;
            if ( PROTO_2 <= proto ) {
                timestamp = stream_getU32( stream );
#ifdef DEBUG
                if ( 0 < timestamp ) {
                    XP_U32 now = dutil_getCurSeconds( dutil, xwe );
                    XP_LOGFF( "delivery took %ds", now - timestamp );
                }
#else
                XP_USE( timestamp );
#endif
            }
            MQTTCmd cmd = stream_getU8( stream );

            /* Need to ack even if discarded/malformed */
            dutil_ackMQTTMsg( dutil, xwe, gameID, &senderID, buf, len );

            switch ( cmd ) {
            case CMD_INVITE: {
                NetLaunchInfo nli = {0};
                if ( nli_makeFromStream( &nli, stream ) ) {
                    dutil_onInviteReceived( dutil, xwe, &nli );
                }
            }
                break;
            case CMD_DEVGONE:
            case CMD_MSG: {
                CommsAddrRec from = {0};
                addr_addType( &from, COMMS_CONN_MQTT );
                from.u.mqtt.devID = senderID;
                if ( CMD_MSG == cmd ) {
                    dispatchMsgs( dutil, xwe, proto, stream, gameID, &from );
                } else if ( CMD_DEVGONE == cmd ) {
                    dutil_onGameGoneReceived( dutil, xwe, gameID, &from );
                }
            }
                break;
            default:
                XP_LOGFF( "unknown command %d; dropping message", cmd );
                // XP_ASSERT(0);
            }
        } else {
            XP_LOGFF( "bad proto %d; dropping packet", proto );
        }
        stream_destroy( stream, xwe );
    } else if ( isCtrlMsg( &myID, topic ) ) {
        dutil_onCtrlReceived( dutil, xwe, buf, len );
    }
}
