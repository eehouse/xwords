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
#include "dllist.h"
#include "comtypes.h"
#include "memstream.h"
#include "xwstream.h"
#include "strutils.h"
#include "nli.h"
#include "dbgutil.h"

#ifdef DEBUG
# define MAGIC_INITED 0x8283413F
# define ASSERT_MAGIC() XP_ASSERT(dutil->magic == MAGIC_INITED)
#else
# define ASSERT_MAGIC()
#endif

#define LAST_REG_KEY FULL_KEY("device_last_reg")
#define KEY_GITREV FULL_KEY("device_gitrev")

#define PD_VERSION_1 2

static XWStreamCtxt*
mkStream( XW_DUtilCtxt* dutil )
{
    XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(dutil->mpool)
                                                dutil_getVTManager(dutil) );
    return stream;
}

typedef enum { WSR_REGISTER, } WSR;

typedef struct WSData {
    DLHead links;
    XP_U32 resultKey;
    WSR code;
} WSData;

typedef struct _PhoniesDataStrs {
    DLHead links;
    XP_UCHAR* phony;
} PhoniesDataStrs;

typedef struct _PhoniesDataCodes {
    DLHead links;
    XP_UCHAR* isoCode;
    PhoniesDataStrs* head;
} PhoniesDataCodes;

typedef struct _DevCtxt {
    XP_U16 devCount;
    WSData* webSendData;
    XP_U32 mWebSendKey;
    pthread_mutex_t webSendMutex;

    PhoniesDataCodes* pd;

#ifdef DEBUG
    XP_U32 magic;
#endif

} DevCtxt;


static DevCtxt*
load( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    DevCtxt* state = (DevCtxt*)dutil->devCtxt;
    if ( NULL == state ) {
        XP_ASSERT(0);
#ifdef XWFEATURE_DEVICE
        XWStreamCtxt* stream = mkStream( dutil );
        const XP_UCHAR* keys[] = { KEY_DEVSTATE, NULL };
        dutil_loadStream( dutil, xwe, keys, stream );

        if ( 0 < stream_getSize( stream ) ) {
            state->devCount = stream_getU16( stream );
            ++state->devCount;  /* for testing until something's there */
            /* XP_LOGF( "%s(): read devCount: %d", __func__, state->devCount ); */
        } else {
            XP_LOGF( "%s(): empty stream!!", __func__ );
        }
        stream_destroy( stream );
#endif
    }

    return state;
}

#ifdef XWFEATURE_DEVICE
void
dvc_store( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    ASSERT_MAGIC();
    DevCtxt* state = load( dutil, xwe );
    XWStreamCtxt* stream = mkStream( dutil );
    stream_putU16( stream, state->devCount );
    const XP_UCHAR* keys[] = { KEY_DEVSTATE, NULL };
    dutil_storeStream( dutil, xwe, keys, stream );
    stream_destroy( stream );
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
    ASSERT_MAGIC();
    getMQTTDevID( dutil, xwe, XP_FALSE, devID );
}

void
dvc_setMQTTDevID( XW_DUtilCtxt* dutil, XWEnv xwe, const MQTTDevID* devID )
{
    const XP_UCHAR* keys[] = { MQTT_DEVID_KEY, NULL };
    dutil_storePtr( dutil, xwe, keys, devID, sizeof(*devID) );
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
    ASSERT_MAGIC();
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
        XP_LOGFFV( "AFTER: got %d: %s", ii, topics[ii] );
    }

    XP_ASSERT( count <= *nTopics );
    *nTopics = count;
    XP_ASSERT( offset < storageLen );

    logPtrs( __func__, *nTopics, topics );
}

typedef enum { CMD_INVITE, CMD_MSG, CMD_DEVGONE, } MQTTCmd;

// #define PROTO_0 0
#define PROTO_1 1        /* moves gameID into "header" relay2 knows about */
#define _PROTO_2 2       /* never used, and now deprecated (value 2 is) */
#define PROTO_3 3        /* adds multi-message, removes gameID and timestamp */
#ifndef MQTT_USE_PROTO
# define MQTT_USE_PROTO PROTO_1
#endif

static void
addHeaderGameIDAndCmd( XW_DUtilCtxt* dutil, XWEnv xwe, MQTTCmd cmd,
                       XP_U32 gameID, XWStreamCtxt* stream )
{
    stream_putU8( stream, MQTT_USE_PROTO );

    MQTTDevID myID;
    dvc_getMQTTDevID( dutil, xwe, &myID );
    myID = htobe64( myID );
    stream_putBytes( stream, &myID, sizeof(myID) );

    stream_putU32( stream, gameID );

    stream_putU8( stream, cmd );
}

#ifdef MQTT_GAMEID_TOPICS
static void
addProto3HeaderCmd( XW_DUtilCtxt* dutil, XWEnv xwe, MQTTCmd cmd,
                    XWStreamCtxt* stream )
{
    stream_putU8( stream, PROTO_3 );

    MQTTDevID myID;
    dvc_getMQTTDevID( dutil, xwe, &myID );
    myID = htobe64( myID );
    stream_putBytes( stream, &myID, sizeof(myID) );

    stream_putU8( stream, cmd );
}
#endif

static void
callProc( MsgAndTopicProc proc, void* closure, const XP_UCHAR* topic,
          XWStreamCtxt* stream )
{
    const XP_U8* msgBuf = !!stream ? stream_getPtr(stream) : NULL;
    XP_U16 msgLen = !!stream ? stream_getSize(stream) : 0;
    (*proc)( closure, topic, msgBuf, msgLen );
}

void
dvc_makeMQTTInvites( XW_DUtilCtxt* dutil, XWEnv xwe,
                     MsgAndTopicProc proc, void* closure,
                     const MQTTDevID* addressee,
                     const NetLaunchInfo* nli )
{
    ASSERT_MAGIC();
    XP_UCHAR devTopic[64];      /* used by two below */
    formatMQTTDevTopic( addressee, devTopic, VSIZE(devTopic) );
    /* Stream format is identical for both topics */
    XWStreamCtxt* stream = mkStream( dutil );
    addHeaderGameIDAndCmd( dutil, xwe, CMD_INVITE, nli->gameID, stream );
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
    callProc( proc, closure, gameTopic, stream );
#endif

    stream_destroy( stream );
}

void
dvc_makeMQTTNukeInvite( XW_DUtilCtxt* dutil, XWEnv xwe,
                        MsgAndTopicProc proc, void* closure,
                        const NetLaunchInfo* nli )
{
    ASSERT_MAGIC();
#ifdef MQTT_GAMEID_TOPICS
    MQTTDevID myID;
    dvc_getMQTTDevID( dutil, xwe, &myID );
    XP_UCHAR devTopic[32];
    formatMQTTDevTopic( &myID, devTopic, VSIZE(devTopic) );
    XP_UCHAR gameTopic[64];
    size_t siz = XP_SNPRINTF( gameTopic, VSIZE(gameTopic),
                              "%s/%X", devTopic, nli->gameID );
    XP_ASSERT( siz < VSIZE(gameTopic) );
    XP_USE(siz);
    callProc( proc, closure, gameTopic, NULL );
#endif
}

XP_S16
dvc_makeMQTTMessages( XW_DUtilCtxt* dutil, XWEnv xwe,
                      MsgAndTopicProc proc, void* closure,
                      const SendMsgsPacket* const msgs,
                      const MQTTDevID* addressee,
                      XP_U32 gameID, XP_U16 streamVersion )
{
    ASSERT_MAGIC();
    XP_S16 nSent0 = 0;
    XP_S16 nSent1 = 0;
    XP_U8 nBufs = 0;
    // XP_LOGFF( "(streamVersion: %X)", streamVersion );
    XP_UCHAR devTopic[64];      /* used by two below */
    formatMQTTDevTopic( addressee, devTopic, VSIZE(devTopic) );

    /* If streamVersion is >= STREAM_VERS_NORELAY then we know that the remote
       supports PROTO_3. If it's 0, it *might*, and as people upgrade it'll be
       more likely we just aren't in that point in the game, but send both. If
       it's > 0 but < STREAM_VERS_NORELAY, no point sending PROTO_3 */

    for ( SendMsgsPacket* packet = (SendMsgsPacket*)msgs;
          !!packet; packet = (SendMsgsPacket* const)packet->next ) {
        ++nBufs;
        if ( 0 == streamVersion || STREAM_VERS_NORELAY > streamVersion ) {
            XWStreamCtxt* stream = mkStream( dutil );
            addHeaderGameIDAndCmd( dutil, xwe, CMD_MSG, gameID, stream );
            stream_putBytes( stream, packet->buf, packet->len );
            callProc( proc, closure, devTopic, stream );
            stream_destroy( stream );
            nSent0 += packet->len;
        }
    }

    if ( 0 == streamVersion || STREAM_VERS_NORELAY <= streamVersion ) {
        XWStreamCtxt* stream = mkStream( dutil );
        addProto3HeaderCmd( dutil, xwe, CMD_MSG, stream );

        /* For now, we ship one message per packet. But the receiving code
           should be ready */
        stream_putU8( stream, nBufs );
        if ( 1 < nBufs ) {
            XP_LOGFF( "nBufs > 1: %d", nBufs );
        }
        for ( SendMsgsPacket* packet = (SendMsgsPacket*)msgs;
              !!packet; packet = (SendMsgsPacket* const)packet->next ) {
            XP_U32 len = packet->len;
#ifdef DEBUG
            if ( 0 == len ) {
                XP_LOGFF( "ERROR: msg len 0" );
            }
#endif
            stream_putU32VL( stream, len );
            stream_putBytes( stream, packet->buf, len );
            nSent1 += len;
        }

        XP_ASSERT( nSent0 == nSent1 || nSent0 == 0 || nSent1 == 0 );

        XP_UCHAR gameTopic[64];
        size_t siz = XP_SNPRINTF( gameTopic, VSIZE(gameTopic),
                                  "%s/%X", devTopic, gameID );
        XP_ASSERT( siz < VSIZE(gameTopic) );
        XP_USE(siz);

        callProc( proc, closure, gameTopic, stream );
        stream_destroy( stream );
    }
    return XP_MAX( nSent0, nSent1 );
}

void
dvc_makeMQTTNoSuchGames( XW_DUtilCtxt* dutil, XWEnv xwe,
                         MsgAndTopicProc proc, void* closure,
                         const MQTTDevID* addressee,
                         XP_U32 gameID )
{
    ASSERT_MAGIC();
    XP_LOGFF( "(gameID: %X)", gameID );
    XP_UCHAR devTopic[64];      /* used by two below */
    formatMQTTDevTopic( addressee, devTopic, VSIZE(devTopic) );

    XWStreamCtxt* stream = mkStream( dutil );
    addHeaderGameIDAndCmd( dutil, xwe, CMD_DEVGONE, gameID, stream );
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

    stream_destroy( stream );
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
    if ( 1 < msgCount ) {
        XP_LOGFF( "nBufs > 1: %d", msgCount );
    }
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

static void
ackMQTTMsg( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* topic,
            XP_U32 gameID, const XP_U8* buf, XP_U16 len )
{
    cJSON* params = cJSON_CreateObject();

    cJSON_AddStringToObject( params, "topic", topic );

    Md5SumBuf sb;
    dutil_md5sum( dutil, xwe, buf, len, &sb );
    cJSON_AddStringToObject( params, "sum", sb.buf );

    cJSON_AddNumberToObject( params, "gid", gameID );

    dutil_sendViaWeb( dutil, xwe, 0, "ack", params );
    cJSON_Delete( params );
}

void
dvc_parseMQTTPacket( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* topic,
                     const XP_U8* buf, XP_U16 len )
{
    XP_LOGFF( "(topic=%s, len=%d)", topic, len );
    ASSERT_MAGIC();

    MQTTDevID myID;
    dvc_getMQTTDevID( dutil, xwe, &myID );

    XP_U32 gameID = 0;
    if ( isDevMsg( &myID, topic, &gameID ) ) {
        XWStreamCtxt* stream = mkStream( dutil );
        stream_putBytes( stream, buf, len );

        XP_U8 proto = 0;
        if ( !stream_gotU8( stream, &proto ) ) {
            XP_LOGFF( "bad message: too short" );
        } else if ( proto == PROTO_1 || proto == PROTO_3 ) {
            MQTTDevID senderID;
            if ( stream_gotBytes( stream, &senderID, sizeof(senderID) ) ) {
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

                MQTTCmd cmd = stream_getU8( stream );

                /* Need to ack even if discarded/malformed */
                ackMQTTMsg( dutil, xwe, topic, gameID, buf, len );

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
                XP_LOGFF( "no senderID found; bailing" );
            }
        } else {
            XP_LOGFF( "bad proto %d; dropping packet", proto );
        }
        stream_destroy( stream );
    } else if ( isCtrlMsg( &myID, topic ) ) {
        dutil_onCtrlReceived( dutil, xwe, buf, len );
    }
    LOG_RETURN_VOID();
} /* dvc_parseMQTTPacket */

typedef struct _GetByKeyData {
    XP_U32 resultKey;
    WSData* found;
} GetByKeyData;

static ForEachAct
getByKeyProc(const DLHead* elem, void* closure)
{
    ForEachAct result = FEA_OK;
    GetByKeyData* gbkdp = (GetByKeyData*)closure;
    WSData* wsdp = (WSData*)elem;
    if ( wsdp->resultKey == gbkdp->resultKey ) {
        gbkdp->found = wsdp;
        result = FEA_REMOVE | FEA_EXIT;
    }
    return result;
}

static WSData*
popForKey( XW_DUtilCtxt* dutil, XWEnv xwe, XP_U32 key )
{
    WSData* item = NULL;
    DevCtxt* dc = load( dutil, xwe );

    pthread_mutex_lock( &dc->webSendMutex );

    GetByKeyData gbkd = { .resultKey = key, };
    dc->webSendData = (WSData*)dll_map( &dc->webSendData->links,
                                        getByKeyProc, NULL, &gbkd );
    item = gbkd.found;

    pthread_mutex_unlock( &dc->webSendMutex );
    XP_LOGFFV( "(key: %d) => %p", key, item );
    return item;
}

static XP_U32
addWithKey( XW_DUtilCtxt* dutil, XWEnv xwe, WSData* wsdp )
{
    DevCtxt* dc = load( dutil, xwe );
    pthread_mutex_lock( &dc->webSendMutex );
    wsdp->resultKey = ++dc->mWebSendKey;
    dc->webSendData = (WSData*)
        dll_insert( &dc->webSendData->links, &wsdp->links, NULL );
    pthread_mutex_unlock( &dc->webSendMutex );
    XP_LOGFFV( "(%p) => %d", wsdp, wsdp->resultKey );
    return wsdp->resultKey;
}

static void
delWSDatum( DLHead* elem, void* closure )
{
    XW_DUtilCtxt* dutil = (XW_DUtilCtxt*)closure;
    XP_FREEP( dutil->mpool, &elem );
}

void
dvc_onWebSendResult( XW_DUtilCtxt* dutil, XWEnv xwe, XP_U32 resultKey,
                     XP_Bool succeeded, const XP_UCHAR* resultJson )
{
    XP_ASSERT( 0 != resultKey );
    if ( 0 != resultKey ) {
        WSData* wsdp = popForKey( dutil, xwe, resultKey );
        XP_ASSERT( !!wsdp );
        if ( !!wsdp ) {
            cJSON* result = cJSON_Parse( resultJson ); /* ok if resultJson is NULL... */
            switch ( wsdp->code ) {
            case WSR_REGISTER:
                if ( succeeded ) {
                    cJSON* tmp = cJSON_GetObjectItem( result, "success" ); /* returns null if result is null */
                    if ( !!tmp && cJSON_IsTrue( tmp ) ) {
                        tmp = cJSON_GetObjectItem( result, "atNext" );
                        if ( !!tmp ) {
                            XP_U32 atNext = tmp->valueint;
                            {
                                const XP_UCHAR* keys1[] = { LAST_REG_KEY, NULL };
                                dutil_storePtr( dutil, xwe, keys1, &atNext,
                                                sizeof(atNext) );
                            }
                            {
                                const XP_UCHAR* keys2[] = { KEY_GITREV, NULL };
                                dutil_storePtr( dutil, xwe, keys2, GITREV,
                                                XP_STRLEN(GITREV) );
                            }
                        }
                    }
                }
                break;
            default:
                XP_ASSERT(0);
                break;
            }

            cJSON_Delete( result );
            delWSDatum( &wsdp->links, dutil );
        }
    }
}

static void
freeWSState( XW_DUtilCtxt* dutil, DevCtxt* dc )
{
    pthread_mutex_lock( &dc->webSendMutex );
    dll_removeAll( &dc->webSendData->links, delWSDatum, dutil );
    pthread_mutex_unlock( &dc->webSendMutex );
}

typedef struct _PhoniesMapState {
    XW_DUtilCtxt* dutil;
    const XP_UCHAR* isoCode;
    const XP_UCHAR* phony;
    const DLHead* found;
} PhoniesMapState;

static ForEachAct
findIsoProc( const DLHead* elem, void* closure )
{
    ForEachAct result = FEA_OK;
    PhoniesDataCodes* pdc = (PhoniesDataCodes*)elem;

    PhoniesMapState* pms = (PhoniesMapState*)closure;
    if ( 0 == XP_STRCMP( pms->isoCode, pdc->isoCode ) ) {
        pms->found = elem;
        result = FEA_EXIT;
    }
    return result;
}

static PhoniesDataCodes*
findForIso( XW_DUtilCtxt* XP_UNUSED_DBG(dutil), DevCtxt* dc, const XP_UCHAR* isoCode )
{
    PhoniesMapState ms = {
        .isoCode = isoCode,
    };
    dll_map( &dc->pd->links, findIsoProc, NULL, &ms );
    PhoniesDataCodes* pdc = (PhoniesDataCodes*)ms.found;

    if ( !pdc ) {
        pdc = XP_CALLOC( dutil->mpool, sizeof(*pdc) );
        pdc->isoCode = copyString( dutil->mpool, isoCode );
        dc->pd = (PhoniesDataCodes*)dll_insert( &dc->pd->links, &pdc->links, NULL );
        XP_ASSERT( pdc == dc->pd );
    }

    return pdc;
}

static void
addPhony( XW_DUtilCtxt* dutil, DevCtxt* dc, const XP_UCHAR* isoCode,
          const XP_UCHAR* phony )
{
    PhoniesDataCodes* pdc = findForIso( dutil, dc, isoCode );
    XP_ASSERT( !!pdc );

    PhoniesDataStrs* pd = XP_CALLOC( dutil->mpool, sizeof(*pd) );
    pd->phony = copyString( dutil->mpool, phony );

    pdc->head = (PhoniesDataStrs*)dll_insert( &pdc->head->links, &pd->links, NULL );
}

static ForEachAct
storeStrs( const DLHead* elem, void* closure )
{
    const PhoniesDataStrs* pds = (PhoniesDataStrs*)elem;
    XWStreamCtxt* stream = (XWStreamCtxt*)closure;
    stringToStream( stream, pds->phony );
    return FEA_OK;
}

static ForEachAct
storeIso( const DLHead* elem, void* closure )
{
    const PhoniesDataCodes* pdc = (PhoniesDataCodes*)elem;
    XWStreamCtxt* stream = (XWStreamCtxt*)closure;
    stringToStream( stream, pdc->isoCode );

    PhoniesDataStrs* pds = pdc->head;
    XP_U16 numStrs = dll_length( &pds->links );
    XP_ASSERT( 0 < numStrs );
    stream_putU32VL( stream, numStrs );

    dll_map( &pds->links, storeStrs, NULL, stream );

    return FEA_OK;
}

/* Storage format for PD_VERSION_1:
 * version (byte)
 * isoCount (byte)
 ** (repeats isoCount times)
 ** isoCode (string)
 ** strCount (var len XP_U32)
 *** (repeats strCount times)
 *** phony (string)
 */
static void
storePhoniesData( XW_DUtilCtxt* dutil, XWEnv xwe, DevCtxt* dc )
{
    XWStreamCtxt* stream = mkStream( dutil );
    if ( !!dc->pd ) {
        stream_putU8( stream, PD_VERSION_1 );

        PhoniesDataCodes* pdc = dc->pd;
        XP_U16 numIsos = dll_length( &pdc->links );
        XP_ASSERT( 0 < numIsos );
        stream_putU8( stream, numIsos );

#ifdef DEBUG
        PhoniesDataCodes* pdc1 = (PhoniesDataCodes*)
#endif
            dll_map( &pdc->links, storeIso, NULL, stream );
        XP_ASSERT( pdc1 == pdc );
    }

    const XP_UCHAR* keys[] = { KEY_LEGAL_PHONIES, NULL };
    dutil_storeStream( dutil, xwe, keys, stream );
    stream_destroy( stream );
}

static void
loadPhoniesData( XW_DUtilCtxt* dutil, XWEnv xwe, DevCtxt* dc )
{
    LOG_FUNC();
    XP_ASSERT ( !dc->pd );

    XWStreamCtxt* stream = mkStream( dutil );
    const XP_UCHAR* keys[] = { KEY_LEGAL_PHONIES, NULL };
    dutil_loadStream( dutil, xwe, keys, stream );

    XP_U8 flags;
    if ( stream_gotU8( stream, &flags ) && PD_VERSION_1 == flags ) {
        XP_U8 numIsos;
        if ( stream_gotU8( stream, &numIsos ) ) {
            for ( int ii = 0; ii < numIsos; ++ii ) {
                XP_UCHAR isoCode[32];
                stringFromStreamHere( stream, isoCode, VSIZE(isoCode) );
                XP_U32 numStrs = stream_getU32VL( stream );
                for ( int jj = 0; jj < numStrs; ++jj ) {
                    XP_UCHAR phony[32];
                    stringFromStreamHere( stream, phony, VSIZE(phony) );
                    addPhony( dutil, dc, isoCode, phony );
                }
            }
        }
    } else {
        XP_LOGFF( "nothing there???" );
    }

    stream_destroy( stream );
}

void
dvc_addLegalPhony( XW_DUtilCtxt* dutil, XWEnv xwe,
                   const XP_UCHAR* isoCode,
                   const XP_UCHAR* phony )
{
    if ( ! dvc_isLegalPhony( dutil, xwe, isoCode, phony ) ) {
        DevCtxt* dc = load( dutil, xwe );
        addPhony( dutil, dc, isoCode, phony );
        storePhoniesData( dutil, xwe, dc );
    }
}

XP_Bool
dvc_haveLegalPhonies( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    DevCtxt* dc = load( dutil, xwe );
    XP_Bool result = 0 < dll_length( &dc->pd->links );
    LOG_RETURNF( "%s", boolToStr(result) );
    return result;
}

static void
freeOnePhony( DLHead* elem, void* XP_UNUSED_DBG(closure) )
{
#ifdef DEBUG
    PhoniesMapState* ms = (PhoniesMapState*)closure;
#endif
    const PhoniesDataStrs* pds = (PhoniesDataStrs*)elem;
    XP_FREE( ms->dutil->mpool, pds->phony );
    XP_FREE( ms->dutil->mpool, elem );
}

static void
freeOneCode( DLHead* elem, void* closure)
{
    const PhoniesDataCodes* pdc = (PhoniesDataCodes*)elem;

    dll_removeAll( &pdc->head->links, freeOnePhony, closure );

#ifdef MEM_DEBUG
    PhoniesMapState* ms = (PhoniesMapState*)closure;
#endif
    XP_FREE( ms->dutil->mpool, pdc->isoCode );
    XP_FREE( ms->dutil->mpool, elem );
}

static DevCtxt*
freePhonyState( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    PhoniesMapState ms = { .dutil = dutil, };
    DevCtxt* dc = load( dutil, xwe );
    dll_removeAll( &dc->pd->links, freeOneCode, &ms );
    dc->pd = NULL;
    return dc;
}

static ForEachAct
clearPhonyProc( const DLHead* elem, void* closure )
{
    ForEachAct result = FEA_OK;
    const PhoniesDataStrs* pds = (PhoniesDataStrs*)elem;
    PhoniesMapState* ms = (PhoniesMapState*)closure;
    if ( 0 == XP_STRCMP( ms->phony, pds->phony ) ) {
        result = FEA_EXIT | FEA_REMOVE;
    }
    return result;
}

void
dvc_clearLegalPhony( XW_DUtilCtxt* dutil, XWEnv xwe,
                     const XP_UCHAR* isoCode,
                     const XP_UCHAR* phony )
{
    DevCtxt* dc = load( dutil, xwe );
    PhoniesDataCodes* pdc = findForIso( dutil, dc, isoCode );

    PhoniesMapState ms = { .dutil = dutil, .phony = phony, };
    pdc->head = (PhoniesDataStrs*)
        dll_map( &pdc->head->links, clearPhonyProc, freeOnePhony, &ms );
    if ( !pdc->head ) {
        dc->pd = (PhoniesDataCodes*)dll_remove( &dc->pd->links, &pdc->links );
        freeOneCode( &pdc->links, &ms );
    }
    storePhoniesData( dutil, xwe, dc );
}

typedef struct _GetWordsState {
    WordCollector proc;
    void* closure;
} GetWordsState;

static ForEachAct
getIsosProc( const DLHead* elem, void* closure )
{
    const PhoniesDataCodes* pdc = (PhoniesDataCodes*)elem;
    GetWordsState* gws = (GetWordsState*)closure;
    (*gws->proc)( pdc->isoCode, gws->closure );
    return FEA_OK;
}

static ForEachAct
getWordsProc( const DLHead* elem, void* closure )
{
    const PhoniesDataStrs* pds = (PhoniesDataStrs*)elem;
    GetWordsState* gws = (GetWordsState*)closure;
    (*gws->proc)( pds->phony, gws->closure );
    return FEA_OK;
}

void
dvc_getIsoCodes( XW_DUtilCtxt* dutil, XWEnv xwe,
                 WordCollector proc, void* closure )
{
    GetWordsState gws = {
        .proc = proc,
        .closure = closure,
    };

    DevCtxt* dc = load( dutil, xwe );
    dll_map( &dc->pd->links, getIsosProc, NULL, &gws );
}

void
dvc_getPhoniesFor( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* code,
                   WordCollector proc, void* closure )
{
    GetWordsState gws = {
        .proc = proc,
        .closure = closure,
    };

    DevCtxt* dc = load( dutil, xwe );
    PhoniesDataCodes* pdc = findForIso( dutil, dc, code );
    dll_map( &pdc->head->links, getWordsProc, NULL, &gws );
}

static ForEachAct
findPhonyProc2( const DLHead* elem, void* closure )
{
    ForEachAct result = FEA_OK;
    PhoniesMapState* ms = (PhoniesMapState*)closure;
    const PhoniesDataStrs* pds = (PhoniesDataStrs*)elem;
    if ( 0 == XP_STRCMP( ms->phony, pds->phony ) ) {
        ms->found = elem;
        result |= FEA_EXIT;
    }
    return result;
}

static ForEachAct
findPhonyProc1( const DLHead* elem, void* closure )
{
    ForEachAct result = FEA_OK;
    PhoniesMapState* ms = (PhoniesMapState*)closure;
    const PhoniesDataCodes* pdc = (PhoniesDataCodes*)elem;
    if ( 0 == XP_STRCMP( ms->isoCode, pdc->isoCode ) ) {
        dll_map( &pdc->head->links, findPhonyProc2, NULL, closure );
        result |= FEA_EXIT;
    }
    return result;
}

XP_Bool
dvc_isLegalPhony( XW_DUtilCtxt* dutil, XWEnv xwe,
                  const XP_UCHAR* isoCode, const XP_UCHAR* phony )
{
    DevCtxt* dc = load( dutil, xwe );

    PhoniesMapState ms = {
        .isoCode = isoCode,
        .phony = phony,
    };
    dll_map( &dc->pd->links, findPhonyProc1, NULL, &ms );
    
    return NULL != ms.found;
}

static void
registerIf( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    XP_U32 atNext = 0;
    XP_U32 len = sizeof(atNext);
    {
        const XP_UCHAR* keys1[] = { LAST_REG_KEY, NULL };
        dutil_loadPtr( dutil, xwe, keys1, &atNext, &len );
    }
    XP_UCHAR gitrev[128];
    len = VSIZE(gitrev);
    {
        const XP_UCHAR* keys2[] = { KEY_GITREV, NULL };
        dutil_loadPtr( dutil, xwe, keys2, gitrev, &len );
    }
    if ( len >= VSIZE(gitrev) ) {
        len = VSIZE(gitrev) - 1;
    }
    gitrev[len] = '\0';

    XP_U32 now = dutil_getCurSeconds( dutil, xwe );
    if ( atNext < now || 0 != XP_STRCMP( gitrev, GITREV ) ) {

        /* Start with the platform's values */
        cJSON* params = dutil_getRegValues( dutil, xwe );

        MQTTDevID myID;
        dvc_getMQTTDevID( dutil, xwe, &myID );
        XP_UCHAR tmp[32];
        formatMQTTDevID( &myID, tmp, VSIZE(tmp) );
        cJSON_AddStringToObject( params, "devid", tmp );

        cJSON_AddStringToObject( params, "gitrev", GITREV );
#ifdef DEBUG
        cJSON_AddBoolToObject( params, "dbg", XP_TRUE );
#endif
        char* loc = getenv("LANG");
        cJSON_AddStringToObject( params, "loc", loc );

        cJSON_AddNumberToObject( params, "myNow", now );

        WSData* wsdp = XP_CALLOC( dutil->mpool, sizeof(*wsdp) );
        wsdp->code = WSR_REGISTER;
        XP_U32 resultKey = addWithKey( dutil, xwe, wsdp );

        dutil_sendViaWeb( dutil, xwe, resultKey, "register", params );
        cJSON_Delete( params );
    }
} /* registerIf */

void
dvc_init( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    LOG_FUNC();
    XP_ASSERT( 0 == dutil->magic );

    XP_ASSERT( !dutil->devCtxt );
    DevCtxt* dc = dutil->devCtxt = XP_CALLOC( dutil->mpool, sizeof(*dc) );
    dc->webSendData = NULL;
    dc->mWebSendKey = 0;
    pthread_mutex_init( &dc->webSendMutex, NULL );

    loadPhoniesData( dutil, xwe, dc );

#ifdef DEBUG
    dutil->magic = MAGIC_INITED;
#endif
    registerIf( dutil, xwe );
}

void
dvc_cleanup( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    DevCtxt* dc = freePhonyState( dutil, xwe );

    freeWSState( dutil, dc );

    pthread_mutex_destroy( &dc->webSendMutex );

    XP_FREEP( dutil->mpool, &dc );
}
