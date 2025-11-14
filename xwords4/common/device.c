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

#include "devicep.h"
#include "xwarray.h"
#include "dllist.h"
#include "comtypes.h"
#include "xwstream.h"
#include "strutils.h"
#include "nli.h"
#include "dbgutil.h"
#include "timersp.h"
#include "xwmutex.h"
#include "gamemgrp.h"
#include "stats.h"
#include "dictmgrp.h"
#include "dvcbtp.h"

#ifdef DEBUG
# define MAGIC_INITED 0x8283413F
# define ASSERT_MAGIC()  {                              \
        if ( dutil->magic != MAGIC_INITED ) {           \
            XP_LOGFF( "bad magic %X", dutil->magic );   \
            XP_ASSERT(0);                               \
        }                                               \
    }
#else
# define ASSERT_MAGIC()
#endif

#define LAST_REG_KEY FULL_KEY("device_last_reg")
#define KEY_GITREV FULL_KEY("device_gitrev")

#define PD_VERSION_1 2

#ifndef ACK_TIMER_INTERVAL_MS
# define ACK_TIMER_INTERVAL_MS 5000
#endif

#define SMS_WAIT_SECS 5
#define SMS_MAX_SIZE 130

static XP_S16
makeMQTTMessages( XW_DUtilCtxt* dutil, XWEnv xwe,
                  MsgAndTopicProc proc, void* closure,
                  const SendMsgsPacket* const msgs,
                  const MQTTDevID* addressee,
                  XP_U32 gameID, XP_U16 streamVersion );
static void makeMQTTInvites( XW_DUtilCtxt* dutil, XWEnv xwe,
                             MsgAndTopicProc proc, void* closure,
                             const MQTTDevID* addressee,
                             const NetLaunchInfo* nli );


XWStreamCtxt*
dvc_makeStream( XW_DUtilCtxt* dutil )
{
    XWStreamCtxt* stream = strm_make_raw( MPPARM_NOCOMMA(dutil->mpool) );
    return stream;
}

#define KEY_DELIM '/'

static void
formatKeys( const XP_UCHAR* keys[], XP_UCHAR buf[], XP_U16 bufLen,
            XP_Bool sepAfter )
{
    int offset = 0;

    for ( int ii = 0; ; ) {
        XP_ASSERT( NULL == strchr(keys[ii], KEY_DELIM) );
        offset += XP_SNPRINTF( &buf[offset], bufLen - offset, "%s", keys[ii] );
        if ( !keys[++ii] ) {
            break;
        }
        buf[offset++] = KEY_DELIM;
    }
    if ( sepAfter ) {
        buf[offset++] = KEY_DELIM;
        buf[offset++] = '%';
    }
    buf[offset] = '\0';
}

void
dvc_parseKey( XP_UCHAR* buf, XP_UCHAR* parts[], XP_U16* nParts )
{
    XP_UCHAR* ptr = buf;
    int nFound = 0;
    for ( int ii = 0; !!buf[ii]; ++ii ) {
        if ( KEY_DELIM == buf[ii] ) {
            buf[ii] = '\0';
            parts[nFound++] = ptr;
            ptr = &buf[ii+1];
            XP_ASSERT( nFound < *nParts );
        }
    }
    parts[nFound++] = ptr;
    XP_ASSERT( nFound < *nParts );

    /* for ( int ii = 0; ii < nFound; ++ii ) { */
    /*     XP_LOGFF( "found part[%d]: '%s'", ii, parts[ii] ); */
    /* } */
    
    *nParts = nFound;
}

void
dvc_storeStream( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* keys[],
                 XWStreamCtxt* stream )
{
    XP_UCHAR key[256];
    formatKeys( keys, key, VSIZE(key), XP_FALSE );

    XP_LOGFF( "using key: %s", key );

    dutil_storeStream( dutil, xwe, key, stream );
}

XWStreamCtxt*
dvc_loadStream( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* keys[] )
{
    XP_UCHAR key[256];
    formatKeys( keys, key, VSIZE(key), XP_FALSE );
    XP_LOGFF( "using key: %s", key );

    XWStreamCtxt* stream = dvc_makeStream( dutil );
    dutil_loadStream( dutil, xwe, key, stream );
    if ( 0 == strm_getSize(stream) ) { /* data found */
        strm_destroy( stream );
        stream = NULL;
    }
    return stream;
}

void
dvc_removeStream( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* keys[] )
{
    XP_UCHAR key[256];
    formatKeys( keys, key, VSIZE(key), XP_FALSE );
    XP_LOGFF( "using key: %s", key );

    dutil_removeStored( dutil, xwe, key );
}

void
dvc_getKeysLike( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* keys[],
                 OnGotKey proc, void* closure )
{
    XP_UCHAR key[256];
    formatKeys( keys, key, VSIZE(key), XP_FALSE );
    XP_LOGFF( "using key: %s", key );

    dutil_getKeysLike( dutil, xwe, key, proc, closure );
}

/* typedef struct _GotMsgState { */
/*     XW_DUtilCtxt* dutil; */
/*     XWStreamCtxt* stream; */
/*     XP_U32 gameID; */
/*     const CommsAddrRec* retAddr; */
/* } GotMsgState; */

/* static void */
/* onGameProc( GameRef gr, XWEnv xwe, void* closure ) */
/* { */
/*     GotMsgState* sp = (GotMsgState*)closure; */
/*     if ( sp->gameID == gr_getGameID(sp->duc, gr, xwe) ) { */
/*         XP_Bool success = */
/*             game_receiveMessage( sp->duc, gr, xwe, sp->stream, sp->retAddr ); */
/*         XP_LOGFF( "game_receiveMessage() => %s", boolToStr(success) ); */
/*     } */
/* } */

/* void */
/* dvc_onMessageReceived( XW_DUtilCtxt* dutil, XWEnv xwe, XP_U32 gameID, */
/*                        const CommsAddrRec* from, const XP_U8* buf, */
/*                        XP_U16 len ) */
/* { */
/*     XP_LOGFF( "(gameID: %X)", gameID ); */
/*     /\* XWStreamCtxt* stream = dvc_makeStream( dutil ); *\/ */
/*     /\* strm_putBytes( stream, buf, len ); *\/ */

/*     GameRef gr = gmgr_getFor( dutil, xwe, gameID ); */
/*     if ( gr ) { */
/*         gr_onMessageReceived(dutil, xwe, gr, from, buf, len ); */
/*     } */
/*         // dutil_onGameAdded( dutil, xwe, gr ); /\* in case it was created? *\/ */

/*     /\*     XP_Bool haveComms = gr_haveComms( duc, gr, xwe ); *\/ */
/*     /\*     if ( haveComms ) { *\/ */
/*     /\*         CommsMsgState commsState; *\/ */
/*     /\*         result = gr_checkIncomingStream( duc, gr, xwe, stream, retAddr, *\/ */
/*     /\*                                          &commsState ); *\/ */
/*     /\*         if ( result ) { *\/ */
/*     /\*             result = gr_receiveMessage( duc, gr, xwe, stream ); *\/ */
/*     /\*         } *\/ */
/*     /\*         gr_msgProcessed( duc, gr, xwe, &commsState, !result ); *\/ */
/*     /\*     } *\/ */

/*     /\* if ( result ) { *\/ */
/*     /\*     gr_addIdle( duc, xwe, gr ); *\/ */
/*     /\* } *\/ */

/*     /\* LOG_RETURNF( "%s", boolToStr(result) ); *\/ */
/*     /\* return result; *\/ */
/* } */
     
        
/* #ifdef DEBUG */
/*         XP_Bool success = */
/* #endif */
/*             game_receiveMessage( dutil, gr, xwe, stream, from ); */
/*         XP_LOGFF( "game_receiveMessage() => %s", boolToStr(success) ); */
/*     } */
/*     strm_destroy( stream ); */
/* } */

void
dvc_onGameGoneReceived( XW_DUtilCtxt* dutil, XWEnv xwe, XP_U32 gameID,
                        const CommsAddrRec* from )
{
    XP_LOGFF( "(gameID=%X)", gameID );
    XP_USE(gameID);
    XP_USE(dutil);
    XP_USE(xwe);
    XP_USE(from);
}

void
dvc_onDictAdded( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* dictName )
{
    gmgr_onDictAdded( duc, xwe, dictName );
}

void
dvc_onDictRemoved( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* dictName )
{
    LOG_FUNC();
    dmgr_remove( duc, xwe, dictName );
    gmgr_onDictRemoved( duc, xwe, dictName );
    LOG_RETURN_VOID();
}

typedef enum { WSR_REGISTER, } WSR;

typedef struct WSData {
    DLHead links;
    XP_U32 resultKey;
    WSR code;
} WSData;

typedef struct _PhoniesDataCodes {
    XP_UCHAR* isoCode;
    XWArray* phonies;
} PhoniesDataCodes;

typedef struct _DevCtxt {

    XP_U16 devCount;
    XP_U8 mqttQOS;
    XP_Bool dirty;
    MutexState mutex;

    struct {
        WSData* data;
        XP_U32 key;
        MutexState mutex;
    } webSend;

    struct {
        MutexState mutex;
        cJSON* msgs;             /* pending acks saved here */
        TimerKey key;
    } ackTimer;

    XWArray* pd;

#ifdef DEBUG
    XP_U32 magic;
#endif

} DevCtxt;

// PENDING: Actually use a timer, or rename this function
static void setSaveDCTimer( XW_DUtilCtxt* dutil, XWEnv xwe,
                            DevCtxt* dc );

static DevCtxt*
load( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    DevCtxt* state = (DevCtxt*)dutil->devCtxt;
    if ( NULL == state ) {
        dutil->devCtxt = state = XP_CALLOC( dutil->mpool, sizeof(*state) );

        XWStreamCtxt* stream = dvc_makeStream( dutil );
        dutil_loadStream( dutil, xwe, KEY_DEVSTATE, stream );

        if ( 0 < strm_getSize( stream ) ) {
            state->devCount = strm_getU16( stream );
            ++state->devCount;  /* for testing until something's there */
            /* XP_LOGFF( "read devCount: %d", state->devCount ); */
            if ( strm_gotU8( stream, &state->mqttQOS ) ) {
                XP_LOGFF( "read qos: %d", state->mqttQOS );
            } else {
                state->mqttQOS = 1;
                setSaveDCTimer( dutil, xwe, state );
            }
        } else {
            XP_LOGFF( "empty stream!!" );
        }
        strm_destroy( stream );
    }

    // LOG_RETURNF( "%p", state );
    return state;
}

XP_U8
dvc_getQOS( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    DevCtxt* state = load( dutil, xwe );
    // LOG_RETURNF("%d", state->mqttQOS);
    return state->mqttQOS;
}

static void
dvcStoreLocked( XW_DUtilCtxt* dutil, XWEnv xwe, DevCtxt* state )
{
    XWStreamCtxt* stream = dvc_makeStream( dutil );
    strm_putU16( stream, state->devCount );
    strm_putU8( stream, state->mqttQOS );
    dutil_storeStream( dutil, xwe, KEY_DEVSTATE, stream );
    strm_destroy( stream );
}

void
dvc_store( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    ASSERT_MAGIC();
    DevCtxt* state = load( dutil, xwe );
    WITH_MUTEX( &state->mutex );
    dvcStoreLocked( dutil, xwe, state );
    END_WITH_MUTEX();
}

typedef struct _MsgTopicData {
    XW_DUtilCtxt* dutil;
    XWEnv xwe;
} MsgTopicData;

static void
msgAndTopicProc( void* closure, const XP_UCHAR* topic, const XP_U8* buf,
                 XP_U16 len, XP_U8 qos )
{
    MsgTopicData* mtd = (MsgTopicData*)closure;
    dutil_sendViaMQTT( mtd->dutil, mtd->xwe, topic, buf, len, qos );
}

static void
sendInviteViaMQTT( XW_DUtilCtxt* dutil, XWEnv xwe, const NetLaunchInfo* nli,
                   const MQTTDevID* invitee )
{
    MsgTopicData mtd = { .dutil = dutil, .xwe = xwe, };
    makeMQTTInvites( dutil, xwe, msgAndTopicProc, &mtd,
                     invitee, nli );
}

#ifdef XWFEATURE_SMS
typedef struct _RetryClosure {
    CHUNK_CMD cmd;
    XP_U16 port;
    XP_U32 gameID;
    XP_UCHAR msgNo[32];
    XP_UCHAR phone[32];
} RetryClosure;

static void sendOrRetry( XW_DUtilCtxt* dutil, XWEnv xwe, ChunkMsgArray* arr,
                         CHUNK_CMD cmd, XP_U16 waitSecs, const XP_UCHAR* phone,
                         XP_U16 port, XP_U32 gameID, const XP_UCHAR* msgNo );

static void
retryProc( XW_DUtilCtxt* dutil, XWEnv xwe, void* closure,
           TimerKey XP_UNUSED_DBG(key), XP_Bool fired )
{
    XP_LOGFF( "(key: %d, fired: %s)", key, boolToStr(fired) );
    if ( fired ) {
        RetryClosure* rc = (RetryClosure*)closure;

        XP_U16 waitSecs;
        ChunkMsgArray* arr = cnk_prepOutbound( dutil->smsChunkerState, xwe,
                                               rc->cmd,
                                               rc->gameID, NULL, 0,
                                               rc->phone, rc->port,
                                               XP_TRUE, &waitSecs );
        sendOrRetry( dutil, xwe, arr, rc->cmd, waitSecs, rc->phone,
                     rc->port, rc->gameID, rc->msgNo );
    }
    XP_FREEP( dutil->mpool, &closure );
}

static void
sendOrRetry( XW_DUtilCtxt* dutil, XWEnv xwe, ChunkMsgArray* arr, CHUNK_CMD cmd,
             XP_U16 waitSecs, const XP_UCHAR* phone, XP_U16 port,
             XP_U32 gameID, const XP_UCHAR* msgNo )
{
    if ( !!arr ) {
        for ( int ii = 0; ii < arr->nMsgs; ++ii ) {
            const ChunkMsgNet* msg = &arr->u.msgsNet[ii];
            XP_LOGFF( "msg->len: %d", msg->len );
            XP_ASSERT( msg->len <= SMS_MAX_SIZE );
            dutil_sendViaNBS( dutil, xwe, msg->data, msg->len, phone, port );
        }
        cnk_freeMsgArray( dutil->smsChunkerState, arr );
    } else if ( waitSecs > 0 ) {
        RetryClosure* rc = (RetryClosure*)XP_CALLOC( dutil->mpool, sizeof(*rc) );
        XP_STRCAT( rc->phone, phone );
        XP_STRCAT( rc->msgNo, msgNo );
        rc->port = port;
        rc->gameID = gameID;
        rc->cmd = cmd;

        XP_U32 inWhenMS = waitSecs * 1000;
#ifdef DEBUG
        TimerKey key =
#endif
            tmr_set( dutil, xwe, inWhenMS, retryProc, rc );
        XP_LOGFF( "got key %d", key );
    }
}

static MsgChunker*
initSMSProtoOnce( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    if ( !dutil->smsChunkerState ) {
        dutil->smsChunkerState = cnk_init( dutil, xwe, SMS_WAIT_SECS, SMS_MAX_SIZE );
    }
    return dutil->smsChunkerState;
}

static void
sendInviteViaNBS( XW_DUtilCtxt* dutil, XWEnv xwe, const NetLaunchInfo* nli,
                  const XP_UCHAR* phone, XP_U16 port )
{
    initSMSProtoOnce( dutil, xwe );
    XWStreamCtxt* stream = dvc_makeStream( dutil );
    nli_saveToStream( nli, stream );
    const XP_U8* ptr = strm_getPtr( stream );
    XP_U16 len = strm_getSize( stream );

    XP_U16 waitSecs;
    const XP_Bool forceOld = XP_TRUE; /* Send NOW in case test app kills us */
    ChunkMsgArray* arr
        = cnk_prepOutbound( dutil->smsChunkerState, xwe, INVITE, nli->gameID,
                            ptr, len, phone, port, forceOld, &waitSecs );
    XP_ASSERT( !!arr || !forceOld );
    sendOrRetry( dutil, xwe, arr, INVITE, waitSecs, phone, port,
                 nli->gameID, "invite" );
    strm_destroy( stream );
}
#endif

/* static ForEachAct */
/* addBTMsgProc( void* elem, void* closure, XWEnv xwe ) */
/* { */
/*     /\* tmp stream lets us put message together then get length *\/ */
/*     XWStreamCtxt* msgStream = (XWStreamCtxt*)elem; */
/*     XWStreamCtxt* tmpStream = dvc_makeStream( dutil ); */
/*     strm_putU8( tmpStream, BTCMD_MESG_SEND ); */
/*     XP_U16 size = strm_getSize(msgStream) */
/*     strm_putU16( tmpStream, size ); */
/*     strm_getFromStream( tmpStream, msgStream, size ); */

/*     XWStreamCtxt* outStream = (XWStreamCtxt*)closure; */
/*     strm_getFromStream( outStream, tmpStream, strm_getSize(tmpStream) ); */
/*     strm_destroy( tmpStream ); */
/*     return FEA_OK; */
/* } */

XP_S16
dvc_sendInvite( XW_DUtilCtxt* dutil, XWEnv xwe, const NetLaunchInfo* nli,
                XP_U32 XP_UNUSED(createdStamp), const CommsAddrRec* addr,
                CommsConnType typ )
{
    XP_S16 nSent = -1;
    switch ( typ ) {
    case COMMS_CONN_MQTT:
        sendInviteViaMQTT( dutil, xwe, nli, &addr->u.mqtt.devID );
        break;
#ifdef XWFEATURE_SMS
    case COMMS_CONN_SMS:
        sendInviteViaNBS( dutil, xwe, nli, addr->u.sms.phone,
                          addr->u.sms.port );
        break;
#endif
    case COMMS_CONN_BT:
        sendInviteViaBT( dutil, xwe, nli, addr->u.bt.hostName,
                         &addr->u.bt.btAddr );
        break;
    case COMMS_CONN_NFC:
        break;
    default:
        XP_LOGFF( "unexpected typ %s", ConnType2Str(typ) );
        XP_ASSERT(0);           /* fired */
    }
    return nSent;
}

static void
sendMsgViaMQTT(XW_DUtilCtxt* dutil, XWEnv xwe, const MQTTDevID* addressee,
               const SendMsgsPacket* const packets, XP_U32 gameID,
               XP_U16 streamVersion )
{
    MsgTopicData mtd = { .dutil = dutil, .xwe = xwe, };
    (void)makeMQTTMessages( dutil, xwe,
                            msgAndTopicProc, &mtd,
                            packets, addressee,
                            gameID, streamVersion );
}

#ifdef XWFEATURE_SMS
static void
sendMsgViaNBS( XW_DUtilCtxt* dutil, XWEnv xwe,
               const SendMsgsPacket* const packets,
               const CommsAddrRec* addr, XP_U32 gameID )
{
    initSMSProtoOnce( dutil, xwe );

    const XP_UCHAR* phone = addr->u.sms.phone;
    XP_U16 port = addr->u.sms.port;
    for ( SendMsgsPacket* packet = (SendMsgsPacket*)packets;
          !!packet; packet = (SendMsgsPacket* const)packet->next ) {
        XP_U16 waitSecs;
        ChunkMsgArray* arr
            = cnk_prepOutbound( dutil->smsChunkerState, xwe, DATA, gameID,
                                packet->buf, packet->len, phone, port,
                                XP_TRUE, &waitSecs );
        sendOrRetry( dutil, xwe, arr, DATA, waitSecs, phone, port, gameID,
                     packet->msgNo );
    }
}
#endif

static void
sendMsgsViaNFC( XW_DUtilCtxt* dutil, XWEnv xwe,
               const SendMsgsPacket* const packets, XP_U32 gameID )
{
    for ( SendMsgsPacket* packet = (SendMsgsPacket*)packets;
          !!packet; packet = (SendMsgsPacket* const)packet->next ) {
        dutil_sendViaNFC( dutil, xwe, packet->buf, packet->len, gameID );
    }
}

XP_S16
dvc_sendMsgs( XW_DUtilCtxt* dutil, XWEnv xwe,
              const SendMsgsPacket* const packets,
              XP_U16 streamVersion,
              const CommsAddrRec* addr, CommsConnType typ,
              GameRef gr )
{
    XP_S16 nSent = -1;
    XP_U32 gameID = gr_getGameID( gr );
    switch ( typ ) {
    case COMMS_CONN_MQTT:
        sendMsgViaMQTT( dutil, xwe, &addr->u.mqtt.devID, packets,
                        gameID, streamVersion );
        break;
#ifdef XWFEATURE_SMS
    case COMMS_CONN_SMS:
        sendMsgViaNBS( dutil, xwe, packets, addr, gameID );
        break;
#endif
    case COMMS_CONN_NFC:
        sendMsgsViaNFC( dutil, xwe, packets, gameID );
        break;
    case COMMS_CONN_BT:
        sendMsgsViaBT( dutil, xwe, packets, addr, gameID );
        break;
    default:
        XP_LOGFF( "don't handle %s", ConnType2Str( typ ) );
        XP_ASSERT(0);           /* fired */
    }
    return nSent;
}

// #define BOGUS_ALL_SAME_DEVID
#define NUM_RUNS 1              /* up this to see how random things look */

static void
getMQTTDevID( XW_DUtilCtxt* dutil, XWEnv xwe, XP_Bool forceNew, MQTTDevID* devID )
{
#ifdef BOGUS_ALL_SAME_DEVID
    XP_USE(forceNew);
    MQTTDevID bogusID = 0;
    XP_UCHAR* str = "ABCDEF0123456789";
    XP_Bool ok = strToMQTTCDevID( str, &bogusID );
    XP_ASSERT( ok );

    MQTTDevID tmp = 0;
    XP_U32 len = sizeof(tmp);
    dutil_loadPtr( dutil, xwe, MQTT_DEVID_KEY, &tmp, &len );
    if ( len != sizeof(tmp) || 0 != XP_MEMCMP( &bogusID, &tmp, sizeof(tmp) ) ) {
        dutil_storePtr( dutil, xwe, MQTT_DEVID_KEY, &bogusID, sizeof(bogusID) );
    }
    *devID = bogusID;

#else
    /* Use the cached value if present and if we're not forcing new */
    if ( !dutil->devID || forceNew ) {
        XP_U32 len = sizeof(dutil->devID);
        if ( !forceNew ) {
            dutil_loadPtr( dutil, xwe, MQTT_DEVID_KEY, &dutil->devID, &len );
        }

        /* XP_LOGFF( "len: %d; sizeof(tmp): %zu", len, sizeof(tmp) ); */
        if ( forceNew || len != sizeof(dutil->devID) ) { /* not found, or bogus somehow */
            int total = 0;
            MQTTDevID tmp;
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

            dutil->devID = tmp;
            dutil_storePtr( dutil, xwe, MQTT_DEVID_KEY, &dutil->devID, sizeof(dutil->devID) );
        }

# ifdef DEBUG
        XP_UCHAR buf[32];
        formatMQTTDevID( &dutil->devID, buf, VSIZE(buf) );
        /* This log statement is required by discon_ok2.py!!! (keep in sync) */
        XP_LOGFF( "generated id: %s; key: %s", buf, MQTT_DEVID_KEY );
# endif
    }
    XP_ASSERT( dutil->devID );
    *devID = dutil->devID;
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
    dutil->devID = *devID;
    dutil_storePtr( dutil, xwe, MQTT_DEVID_KEY, devID, sizeof(*devID) );
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

static void
mqttInit( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    MQTTDevID devid;
    getMQTTDevID( dutil, xwe, XP_FALSE, &devid );
    XP_UCHAR buf0[64];
    formatMQTTDevTopic( &devid, buf0, VSIZE(buf0) );

    const XP_UCHAR* topics[4] = {};

    int count = 0;
#ifdef MQTT_DEV_TOPICS
    /* First, the main device topic */
    topics[count++] = buf0;
#endif

#ifdef MQTT_GAMEID_TOPICS
    /* Then the pattern that includes gameIDs */
    XP_UCHAR buf1[64];
    size_t siz = XP_SNPRINTF( buf1, VSIZE(buf1), "%s/+", buf0 );
    XP_ASSERT( siz < VSIZE(buf1) );
    XP_USE(siz);
    topics[count++] = buf1;
#endif

    /* Finally, the control pattern */
    XP_UCHAR buf2[64];
    formatMQTTCtrlTopic( &devid, buf2, VSIZE(buf2) );
    topics[count++] = buf2;

    for ( int ii = 0; ii < count; ++ii ) {
        XP_LOGFFV( "AFTER: got %d: %s", ii, topics[ii] );
    }

    XP_U8 qos = dvc_getQOS( dutil, xwe );

    dutil_startMQTTListener( dutil, xwe, &devid, topics, qos );
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
    strm_putU8( stream, MQTT_USE_PROTO );

    MQTTDevID myID;
    dvc_getMQTTDevID( dutil, xwe, &myID );
    myID = htobe64( myID );
    strm_putBytes( stream, &myID, sizeof(myID) );

    strm_putU32( stream, gameID );

    strm_putU8( stream, cmd );
}

#ifdef MQTT_GAMEID_TOPICS
static void
addProto3HeaderCmd( XW_DUtilCtxt* dutil, XWEnv xwe, MQTTCmd cmd,
                    XWStreamCtxt* stream )
{
    strm_putU8( stream, PROTO_3 );

    MQTTDevID myID;
    dvc_getMQTTDevID( dutil, xwe, &myID );
    myID = htobe64( myID );
    strm_putBytes( stream, &myID, sizeof(myID) );

    strm_putU8( stream, cmd );
}
#endif

static void
callProc( MsgAndTopicProc proc, void* closure, const XP_UCHAR* topic,
          XWStreamCtxt* stream, XP_U8 qos )
{
    const XP_U8* msgBuf = !!stream ? strm_getPtr(stream) : NULL;
    XP_U16 msgLen = !!stream ? strm_getSize(stream) : 0;
    (*proc)( closure, topic, msgBuf, msgLen, qos );
}

static void
makeMQTTInvites( XW_DUtilCtxt* dutil, XWEnv xwe,
                 MsgAndTopicProc proc, void* closure,
                 const MQTTDevID* addressee,
                 const NetLaunchInfo* nli )
{
    ASSERT_MAGIC();
    XP_UCHAR devTopic[64];      /* used by two below */
    formatMQTTDevTopic( addressee, devTopic, VSIZE(devTopic) );
    /* Stream format is identical for both topics */
    XWStreamCtxt* stream = dvc_makeStream( dutil );
    addHeaderGameIDAndCmd( dutil, xwe, CMD_INVITE, nli->gameID, stream );
    nli_saveToStream( nli, stream );

    XP_U8 qos = dvc_getQOS( dutil, xwe );
#ifdef MQTT_DEV_TOPICS
    callProc( proc, closure, devTopic, stream, qos );
#endif

#ifdef MQTT_GAMEID_TOPICS
    XP_UCHAR gameTopic[64];
    size_t siz = XP_SNPRINTF( gameTopic, VSIZE(gameTopic),
                              "%s/%X", devTopic, nli->gameID );
    XP_ASSERT( siz < VSIZE(gameTopic) );
    XP_USE(siz);
    callProc( proc, closure, gameTopic, stream, qos );
#endif

    strm_destroy( stream );
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
    callProc( proc, closure, gameTopic, NULL, dvc_getQOS(dutil, xwe) );
#endif
}

static XP_S16
makeMQTTMessages( XW_DUtilCtxt* dutil, XWEnv xwe,
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

    XP_U8 qos = dvc_getQOS( dutil, xwe );
    for ( SendMsgsPacket* packet = (SendMsgsPacket*)msgs;
          !!packet; packet = (SendMsgsPacket* const)packet->next ) {
        ++nBufs;
        if ( 0 == streamVersion || STREAM_VERS_NORELAY > streamVersion ) {
            XWStreamCtxt* stream = dvc_makeStream( dutil );
            addHeaderGameIDAndCmd( dutil, xwe, CMD_MSG, gameID, stream );
            strm_putBytes( stream, packet->buf, packet->len );
            callProc( proc, closure, devTopic, stream, qos );
            strm_destroy( stream );
            nSent0 += packet->len;
        }
    }

    if ( 0 == streamVersion || STREAM_VERS_NORELAY <= streamVersion ) {
        XWStreamCtxt* stream = dvc_makeStream( dutil );
        addProto3HeaderCmd( dutil, xwe, CMD_MSG, stream );

        /* For now, we ship one message per packet. But the receiving code
           should be ready */
        strm_putU8( stream, nBufs );
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
            strm_putU32VL( stream, len );
            strm_putBytes( stream, packet->buf, len );
            nSent1 += len;
        }

        XP_ASSERT( nSent0 == nSent1 || nSent0 == 0 || nSent1 == 0 );

        XP_UCHAR gameTopic[64];
        size_t siz = XP_SNPRINTF( gameTopic, VSIZE(gameTopic),
                                  "%s/%X", devTopic, gameID );
        XP_ASSERT( siz < VSIZE(gameTopic) );
        XP_USE(siz);

        callProc( proc, closure, gameTopic, stream, qos );
        strm_destroy( stream );
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

    XP_U8 qos = dvc_getQOS( dutil, xwe );
    XWStreamCtxt* stream = dvc_makeStream( dutil );
    addHeaderGameIDAndCmd( dutil, xwe, CMD_DEVGONE, gameID, stream );
#ifdef MQTT_DEV_TOPICS
    callProc( proc, closure, devTopic, stream, qos );
#endif

#ifdef MQTT_GAMEID_TOPICS
    XP_UCHAR gameTopic[64];
    size_t siz = XP_SNPRINTF( gameTopic, VSIZE(gameTopic),
                              "%s/%X", devTopic, gameID );
    XP_ASSERT( siz < VSIZE(gameTopic) );
    XP_USE(siz);
    callProc( proc, closure, gameTopic, stream, qos );
#endif

    strm_destroy( stream );
}

static XP_Bool
isDevMsg( const MQTTDevID* myID, const XP_UCHAR* topic, XP_U32* gameIDP )
{
    XP_UCHAR buf[64];
    formatMQTTDevTopic( myID, buf, VSIZE(buf) );
    size_t topicLen = XP_STRLEN(buf);
    XP_ASSERT( topicLen < VSIZE(buf)-1 );
    /* Does topic match xw4/device/<devid> at least  */
    XP_Bool success = 0 == strncmp( buf, topic, topicLen );
    if ( success ) {
        /* Now get the gameID if it's there */
        const XP_UCHAR* gameIDPart = topic + topicLen;
#ifdef DEBUG
        int count =
#endif
            sscanf( gameIDPart, "/%X", gameIDP );
        XP_ASSERT( 1 == count || 0 == *gameIDP ); /* firing */
    }
    XP_LOGFF( "(%s) => %s (gameID=%X)", topic, boolToStr(success), *gameIDP );
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
dispatchMsgs( XW_DUtilCtxt* dutil, XWEnv xwe, XP_U8 proto,
              XWStreamCtxt* stream, XP_U32 gameID,
              const CommsAddrRec* from )
{
    int msgCount = proto >= PROTO_3 ? strm_getU8( stream ) : 1;
    if ( 1 < msgCount ) {
        XP_LOGFF( "nBufs > 1: %d", msgCount );
    }
    MsgCountState mcs = { .last = msgCount-1, };
    for ( int ii = 0; ii < msgCount; ++ii ) {
        XP_U32 msgLen = PROTO_1 == proto
            ? strm_getSize( stream )
            : strm_getU32VL( stream );
        if ( msgLen > strm_getSize( stream ) ) {
            XP_LOGFF( "msglen %d too large", msgLen );
            msgLen = 0;
            XP_ASSERT(0);
        }
        if ( 0 < msgLen ) {
            XP_U8 msgBuf[msgLen];
            strm_getBytes( stream, msgBuf, msgLen );
            mcs.cur = ii;
            gmgr_onMessageReceived( dutil, xwe, gameID,
                                    from, msgBuf, msgLen,
                                    &mcs );
        }
    }
}

static void
onAckSendTimer( XW_DUtilCtxt* dutil, XWEnv xwe, void* XP_UNUSED(closure),
                TimerKey XP_UNUSED(key), XP_Bool fired )
{
    XP_LOGFF( "(fired: %s)", boolToStr(fired) );
    DevCtxt* dc = load( dutil, xwe );
    cJSON* ackMsgs;
    WITH_MUTEX( &dc->ackTimer.mutex );
    ackMsgs = dc->ackTimer.msgs;
    dc->ackTimer.msgs = NULL;
    dc->ackTimer.key = 0;
    END_WITH_MUTEX();

    XP_ASSERT( 0 < cJSON_GetArraySize( ackMsgs ) );

    if ( fired ) {
        cJSON* params = cJSON_CreateObject();
        cJSON_AddItemToObject( params, "msgs", ackMsgs );
        dutil_sendViaWeb( dutil, xwe, 0, "ack2", params );
        cJSON_Delete( params );
    } else {
        XP_LOGFF( "Dropping ack messages -- but should store them!" );
        cJSON_Delete( ackMsgs );
    }
}

static void
setAckSendTimerLocked( XW_DUtilCtxt* dutil, XWEnv xwe, DevCtxt* dc )
{
    if ( 0 == dc->ackTimer.key ) {
        dc->ackTimer.key = tmr_set( dutil, xwe,
                                    ACK_TIMER_INTERVAL_MS,
                                    onAckSendTimer, NULL );
        XP_ASSERT( 0 != dc->ackTimer.key );
    }
}

static void
ackMQTTMsg( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* topic,
            XP_U32 gameID, const XP_U8* buf, XP_U16 len )
{
    cJSON* msg = cJSON_CreateObject();
    cJSON_AddStringToObject( msg, "topic", topic );

    Md5SumBuf sb;
    dutil_md5sum( dutil, xwe, buf, len, &sb );
    cJSON_AddStringToObject( msg, "sum", sb.buf );

    cJSON_AddNumberToObject( msg, "gid", gameID );

    DevCtxt* dc = load( dutil, xwe );
    WITH_MUTEX( &dc->ackTimer.mutex );
    if ( !dc->ackTimer.msgs ) {
        dc->ackTimer.msgs = cJSON_CreateArray();
    }
    cJSON_AddItemToArray( dc->ackTimer.msgs, msg );
    setAckSendTimerLocked( dutil, xwe, dc );
    END_WITH_MUTEX();
}

void
dvc_parseMQTTPacket( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* topic,
                     const XP_U8* buf, XP_U16 len )
{
    XP_LOGFF( "(topic=%s, len=%d)", topic, len );
    ASSERT_MAGIC();

    MQTTDevID myID;
    dvc_getMQTTDevID( dutil, xwe, &myID );

    // XP_LOGFF( "got myID" );     /* gets here */

    XP_U32 gameID = 0;
    if ( isDevMsg( &myID, topic, &gameID ) ) {
        XP_LOGFF( "is msg; gameID: %X", gameID );
        XWStreamCtxt* stream = dvc_makeStream( dutil );
        strm_putBytes( stream, buf, len );

        XP_U8 proto = 0;
        if ( !strm_gotU8( stream, &proto ) ) {
            XP_LOGFF( "bad message: too short" );
        } else if ( proto == PROTO_1 || proto == PROTO_3 ) {
            MQTTDevID senderID;
            if ( strm_gotBytes( stream, &senderID, sizeof(senderID) ) ) {
                senderID = be64toh( senderID );
#ifdef DEBUG
                XP_UCHAR tmp[32];
                formatMQTTDevID( &senderID, tmp, VSIZE(tmp) );
                XP_LOGFF( "senderID: %s", tmp );
#endif
                if ( proto < PROTO_3 ) {
                    gameID = strm_getU32( stream );
                } else {
                    XP_ASSERT( 0 != gameID );
                }

                MQTTCmd cmd = strm_getU8( stream );

                /* Need to ack even if discarded/malformed */
                ackMQTTMsg( dutil, xwe, topic, gameID, buf, len );

                switch ( cmd ) {
                case CMD_INVITE: {
                    NetLaunchInfo nli = {};
                    if ( nli_makeFromStream( &nli, stream ) ) {
                        gmgr_addForInvite( dutil, xwe, GROUP_DEFAULT, &nli );
                    }
                }
                    break;
                case CMD_DEVGONE:
                case CMD_MSG: {
                    CommsAddrRec from = {};
                    addr_addType( &from, COMMS_CONN_MQTT );
                    from.u.mqtt.devID = senderID;
                    if ( CMD_MSG == cmd ) {
                        dispatchMsgs( dutil, xwe, proto, stream, gameID, &from );
                    } else if ( CMD_DEVGONE == cmd ) {
                        dvc_onGameGoneReceived( dutil, xwe, gameID, &from );
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
        strm_destroy( stream );
    } else if ( isCtrlMsg( &myID, topic ) ) {
        dutil_onCtrlReceived( dutil, xwe, buf, len );
    } else {
        XP_LOGFF( "OTHER" );
    }
    LOG_RETURN_VOID();
} /* dvc_parseMQTTPacket */

#ifdef XWFEATURE_SMS
void
dvc_parseSMSPacket( XW_DUtilCtxt* dutil, XWEnv xwe,
                    const CommsAddrRec* fromAddr,
                    const XP_U8* buf, XP_U16 len )
{
    XP_LOGFF( "(len: %d)", len );
    XP_ASSERT( len <= SMS_MAX_SIZE );
    MsgChunker* state = initSMSProtoOnce( dutil, xwe );
    ChunkMsgArray* msgArr = cnk_prepInbound( state, xwe,
                                                fromAddr->u.sms.phone,
                                                fromAddr->u.sms.port,
                                                buf, len );
    sts_increment( dutil, xwe, STAT_NBS_RCVD );
    if ( NULL != msgArr ) {
        XP_ASSERT( msgArr->format == FORMAT_LOC );
        for ( int ii = 0; ii < msgArr->nMsgs; ++ii ) {
            ChunkMsgLoc* msg = &msgArr->u.msgsLoc[ii];
            switch ( msg->cmd ) {
            case DATA:
                gmgr_onMessageReceived( dutil, xwe, msg->gameID,
                                        fromAddr, msg->data, msg->len,
                                        NULL );
                break;
            case INVITE: {
                XWStreamCtxt* stream = dvc_makeStream( dutil );
                strm_putBytes( stream, msg->data, msg->len );
                NetLaunchInfo nli = {};
                if ( nli_makeFromStream( &nli, stream ) ) {
                    gmgr_addForInvite( dutil, xwe, GROUP_DEFAULT, &nli );
                } else {
                    XP_ASSERT(0);
                }
                strm_destroy( stream );
            }
                break;
            default:
                XP_ASSERT(0);   /* implement me!! */
                break;
            }
        }
        cnk_freeMsgArray( state, msgArr );
    }
} /* dvc_parseSMSPacket */
#endif

void
dvc_parseBTPacket( XW_DUtilCtxt* dutil, XWEnv xwe,
                   const XP_U8* buf, XP_U16 len,
                   const XP_UCHAR* fromName, const XP_UCHAR* fromAddr )
{
    parseBTPacket( dutil, xwe, buf, len, fromName, fromAddr );
}

XP_Bool
dvc_parseUrl( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* buf, XP_U16 len,
              const XP_UCHAR* host, const XP_UCHAR* prefix)
{
    XWStreamCtxt* stream = dvc_makeStream( duc );
    strm_putBytes( stream, buf, len );

    XP_UCHAR start[64];
    dvc_formatUrl( start, VSIZE(start), host, prefix );
    XP_Bool good = matchFromStream( stream, (XP_U8*)start, XP_STRLEN(start) );
    if ( good ) {
        XP_U8 got;
        good = strm_gotU8( stream, &got ) && got == '?';
    }

    /* Now look at params to see what it is */
    if ( good ) {
        NetLaunchInfo nli;
        if ( nli_fromInviteData( duc, stream, &nli ) ) {
            GameRef gr = gmgr_addForInvite( duc, xwe, GROUP_DEFAULT, &nli );
            XP_LOGFF( "created " GR_FMT " for:", gr );
            LOGNLI( &nli );
        } else {
            good = XP_FALSE;
            XP_LOGFF( "looking beyond '?'" );
            XP_U32 gameID;
            UrlDecodeIter gidIter = {.key = "gid"};
            UrlDecodeIter msgsIter = {.key = "msgs"};
            while ( urlDecodeFromStream( duc, stream, &gidIter, UPT_U32, &gameID ) ) {
                XWStreamCtxt* msgs = NULL;
                if ( urlDecodeFromStream( duc, stream, &msgsIter, UPT_STREAM, &msgs ) ) {
                    good = XP_TRUE;
                    XP_ASSERT( !!msgs );

                    GameRef grs[4];
                    XP_U16 nRefs = VSIZE(grs);
                    gmgr_getForGID( duc, xwe, gameID, grs, &nRefs );
                    for ( int ii = 0; ii < nRefs; ++ii ) {
                        XWStreamPos pos = strm_getPos( msgs, POS_READ );
                        gr_parsePendingPackets( duc, xwe, grs[ii], msgs );
                        strm_setPos( msgs, POS_READ, pos );
                    }
                    strm_destroy( msgs );
                }
            }
        }
    } else {
        XP_LOGFF( "bad start to URL" );
    }

    strm_destroy( stream );
    return good;
}

void
dvc_parsePacketFor( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 gameID,
                    const XP_U8* msg, XP_U16 len, const CommsAddrRec* from )
{
    GameRef grs[4];
    XP_U16 nRefs = VSIZE(grs);
    gmgr_getForGID( duc, xwe, gameID, grs, &nRefs );
    for ( int ii = 0; ii < nRefs; ++ii ) {
        gr_onMessageReceived( duc, grs[ii], xwe, from, msg, len, NULL );
    }
}

void
dvc_onBLEMtuChangedFor( XW_DUtilCtxt* dutil, XWEnv xwe,
                        const XP_UCHAR* phone, XP_U16 mtu )
{
    onBLEMtuChangedFor( dutil, xwe, phone, mtu );
}

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

    WITH_MUTEX(&dc->webSend.mutex);

    GetByKeyData gbkd = { .resultKey = key, };
    dc->webSend.data = (WSData*)dll_map( &dc->webSend.data->links,
                                         getByKeyProc, NULL, &gbkd );
    item = gbkd.found;

    END_WITH_MUTEX();
    XP_LOGFFV( "(key: %d) => %p", key, item );
    return item;
}

static XP_U32
addWithKey( XW_DUtilCtxt* dutil, XWEnv xwe, WSData* wsdp )
{
    DevCtxt* dc = load( dutil, xwe );
    WITH_MUTEX(&dc->webSend.mutex);
    wsdp->resultKey = ++dc->webSend.key;
    dc->webSend.data = (WSData*)
        dll_insert( &dc->webSend.data->links, &wsdp->links, NULL );
    END_WITH_MUTEX();
    XP_LOGFFV( "(%p) => %d", wsdp, wsdp->resultKey );
    return wsdp->resultKey;
}

static void
delWSDatum( DLHead* elem, void* closure )
{
    XW_DUtilCtxt* dutil = (XW_DUtilCtxt*)closure;
    XP_USE(dutil);              /* for release builds */
    XP_FREEP( dutil->mpool, &elem );
}

static void
setSaveDCTimerLocked( XW_DUtilCtxt* dutil, XWEnv xwe, DevCtxt* dc )
{
    dvcStoreLocked( dutil, xwe, dc );
}

static void
setSaveDCTimer( XW_DUtilCtxt* dutil, XWEnv xwe, DevCtxt* dc )
{
    WITH_MUTEX( &dc->mutex );
    setSaveDCTimerLocked( dutil, xwe, dc );
    END_WITH_MUTEX();
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
                            dutil_storePtr( dutil, xwe, LAST_REG_KEY, &atNext,
                                            sizeof(atNext) );
                            dutil_storePtr( dutil, xwe, KEY_GITREV, GITREV,
                                            XP_STRLEN(GITREV) );
                        }

                        tmp = cJSON_GetObjectItem( result, "qos" );
                        if ( !!tmp ) {
                            XP_U8 qos = (XP_U8)tmp->valueint;
                            XP_ASSERT( 0 <= qos && qos <= 2 );
                            if ( 0 <= qos && qos <= 2 ) {
                                DevCtxt* dc = load( dutil, xwe );
                                WITH_MUTEX( &dc->mutex );
                                if ( dc->mqttQOS != qos ) {
                                    dc->dirty = XP_TRUE;
                                    dc->mqttQOS = qos;
                                    setSaveDCTimerLocked( dutil, xwe, dc );
                                }
                                END_WITH_MUTEX();
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
    WITH_MUTEX( &dc->webSend.mutex );
    dll_removeAll( &dc->webSend.data->links, delWSDatum, dutil );
    END_WITH_MUTEX();
}

typedef struct _PhoniesMapState {
#ifdef MEM_DEBUG
    XW_DUtilCtxt* dutil;
#endif
    const XP_UCHAR* isoCode;
    const XP_UCHAR* phony;
    const void* found;
} PhoniesMapState;

static ForEachAct
findIsoProc( void* elem, void* closure, XWEnv XP_UNUSED(xwe) )
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

static int
strCmpProc( const void* dl1, const void* dl2,
            XWEnv XP_UNUSED(xwe), void* XP_UNUSED(closure) )
{
    return XP_STRCMP( (XP_UCHAR*)dl1, (XP_UCHAR*)dl2 );
}

static PhoniesDataCodes*
findForIso( XW_DUtilCtxt* XP_UNUSED_DBG(dutil), XWEnv xwe, DevCtxt* dc,
            const XP_UCHAR* isoCode )
{
    PhoniesMapState ms = {
        .isoCode = isoCode,
    };
    arr_map( dc->pd, xwe, findIsoProc, &ms );
    PhoniesDataCodes* pdc = (PhoniesDataCodes*)ms.found;

    if ( !pdc ) {
        pdc = XP_CALLOC( dutil->mpool, sizeof(*pdc) );
        pdc->isoCode = copyString( dutil->mpool, isoCode );
        pdc->phonies = arr_make( dutil->mpool, strCmpProc, NULL );
        arr_insert( dc->pd, xwe, pdc );
    }

    return pdc;
}

static void
addPhony( XW_DUtilCtxt* dutil, XWEnv xwe, DevCtxt* dc, const XP_UCHAR* isoCode,
          const XP_UCHAR* phony )
{
    PhoniesDataCodes* pdc = findForIso( dutil, xwe, dc, isoCode );
    XP_ASSERT( !!pdc );

    XP_UCHAR* datum = copyString( dutil->mpool, phony );
    arr_insert( pdc->phonies, xwe, datum );
}

static ForEachAct
storeStrs( void* elem, void* closure, XWEnv XP_UNUSED(xwe) )
{
    XP_UCHAR* phony = (XP_UCHAR*)elem;
    XWStreamCtxt* stream = (XWStreamCtxt*)closure;
    stringToStream( stream, phony );
    return FEA_OK;
}

static ForEachAct
storeIso( void* elem, void* closure, XWEnv xwe )
{
    const PhoniesDataCodes* pdc = (PhoniesDataCodes*)elem;
    XWStreamCtxt* stream = (XWStreamCtxt*)closure;
    stringToStream( stream, pdc->isoCode );

    XWArray* phonies = pdc->phonies;
    XP_U16 numStrs = arr_length( phonies );
    XP_ASSERT( 0 < numStrs );
    strm_putU32VL( stream, numStrs );

    arr_map( phonies, xwe, storeStrs, stream );

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
    XWStreamCtxt* stream = dvc_makeStream( dutil );
    if ( !!dc->pd ) {
        strm_putU8( stream, PD_VERSION_1 );

        XWArray* pdc = dc->pd;
        XP_U16 numIsos = arr_length( pdc );
        strm_putU8( stream, numIsos );

        arr_map( pdc, xwe, storeIso, stream );
    }

    dutil_storeStream( dutil, xwe, KEY_LEGAL_PHONIES, stream );
    strm_destroy( stream );
}

static void
loadPhoniesData( XW_DUtilCtxt* dutil, XWEnv xwe, DevCtxt* dc )
{
    LOG_FUNC();
    XP_ASSERT ( !dc->pd );
    dc->pd = arr_make( dutil->mpool, PtrCmpProc, NULL );

    XWStreamCtxt* stream = dvc_makeStream( dutil );
    dutil_loadStream( dutil, xwe, KEY_LEGAL_PHONIES, stream );

    XP_U8 flags;
    if ( strm_gotU8( stream, &flags ) && PD_VERSION_1 == flags ) {
        XP_U8 numIsos;
        if ( strm_gotU8( stream, &numIsos ) ) {
            for ( int ii = 0; ii < numIsos; ++ii ) {
                XP_UCHAR isoCode[32];
                stringFromStreamHere( stream, isoCode, VSIZE(isoCode) );
                XP_U32 numStrs = strm_getU32VL( stream );
                for ( int jj = 0; jj < numStrs; ++jj ) {
                    XP_UCHAR phony[32];
                    stringFromStreamHere( stream, phony, VSIZE(phony) );
                    addPhony( dutil, xwe, dc, isoCode, phony );
                }
            }
        }
    } else {
        XP_LOGFF( "nothing there???" );
    }

    strm_destroy( stream );
}

void
dvc_addLegalPhony( XW_DUtilCtxt* dutil, XWEnv xwe,
                   const XP_UCHAR* isoCode,
                   const XP_UCHAR* phony )
{
    if ( ! dvc_isLegalPhony( dutil, xwe, isoCode, phony ) ) {
        DevCtxt* dc = load( dutil, xwe );
        addPhony( dutil, xwe, dc, isoCode, phony );
        storePhoniesData( dutil, xwe, dc );
    }
}

XP_Bool
dvc_haveLegalPhonies( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    DevCtxt* dc = load( dutil, xwe );
    XP_Bool result = 0 < arr_length( dc->pd );
    LOG_RETURNF( "%s", boolToStr(result) );
    return result;
}

static void
freeOnePhony( void* elem, void* XP_UNUSED_DBG(closure) )
{
#ifdef DEBUG
    PhoniesMapState* ms = (PhoniesMapState*)closure;
#endif
    XP_FREE( ms->dutil->mpool, elem );
}

static void
freeOneCode( void* elem, void* closure )
{
    const PhoniesDataCodes* pdc = (PhoniesDataCodes*)elem;

    arr_removeAll( pdc->phonies, freeOnePhony, closure );
    arr_destroy( pdc->phonies );

#ifdef MEM_DEBUG
    PhoniesMapState* ms = (PhoniesMapState*)closure;
#endif
    XP_FREE( ms->dutil->mpool, pdc->isoCode );
    XP_FREE( ms->dutil->mpool, elem );
}

static DevCtxt*
freePhonyState( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    PhoniesMapState ms = {
#ifdef MEM_DEBUG
        .dutil = dutil,
#endif
    };
    DevCtxt* dc = load( dutil, xwe );
    arr_removeAll( dc->pd, freeOneCode, &ms );
    XP_ASSERT( !dvc_haveLegalPhonies( dutil, xwe ) );
    arr_destroy( dc->pd );
    return dc;
}

void
dvc_clearLegalPhony( XW_DUtilCtxt* dutil, XWEnv xwe,
                     const XP_UCHAR* isoCode,
                     const XP_UCHAR* phony )
{
    DevCtxt* dc = load( dutil, xwe );
    PhoniesDataCodes* pdc = findForIso( dutil, xwe, dc, isoCode );

    PhoniesMapState ms = {

#ifdef MEM_DEBUG
        .dutil = dutil,
#endif
        .phony = phony, };
    void* removed = arr_remove( pdc->phonies, xwe, (void*)phony );
    XP_FREE( dutil->mpool, removed );
    if ( 0 == arr_length(pdc->phonies) ) {
        arr_remove( dc->pd, xwe, pdc );
        freeOneCode( pdc, &ms );
    }
    storePhoniesData( dutil, xwe, dc );
}

typedef struct _GetWordsState {
    WordCollector proc;
    void* closure;
} GetWordsState;

static ForEachAct
getIsosProc( void* elem, void* closure, XWEnv XP_UNUSED(xwe) )
{
    const PhoniesDataCodes* pdc = (PhoniesDataCodes*)elem;
    GetWordsState* gws = (GetWordsState*)closure;
    (*gws->proc)( pdc->isoCode, gws->closure );
    return FEA_OK;
}

static ForEachAct
getWordsProc( void* elem, void* closure, XWEnv XP_UNUSED(xwe) )
{
    XP_UCHAR* phony = (XP_UCHAR*)elem;
    GetWordsState* gws = (GetWordsState*)closure;
    (*gws->proc)( phony, gws->closure );
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
    arr_map( dc->pd, xwe, getIsosProc, &gws );
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
    PhoniesDataCodes* pdc = findForIso( dutil, xwe, dc, code );
    arr_map( pdc->phonies, xwe, getWordsProc, &gws );
}

void
dvc_onTimerFired( XW_DUtilCtxt* dutil, XWEnv xwe, TimerKey key )
{
    // XP_LOGFF( "(key: %d)", key );
    tmr_fired( dutil, xwe, key );
}

typedef struct _FindState {
    const XP_UCHAR* isoCode;
    const XP_UCHAR* phony;
    XP_Bool foundIt;
} FindState;

static ForEachAct
findPhonyProc( void* elem, void* closure, XWEnv xwe )
{
    ForEachAct result = FEA_OK;
    FindState* fs = (FindState*)closure;
    const PhoniesDataCodes* pdc = (PhoniesDataCodes*)elem;
    if ( 0 == XP_STRCMP( fs->isoCode, pdc->isoCode ) ) {
        fs->foundIt = arr_find( pdc->phonies, xwe, fs->phony, NULL );
        result |= FEA_EXIT;
    }
    return result;
}

XP_Bool
dvc_isLegalPhony( XW_DUtilCtxt* dutil, XWEnv xwe,
                  const XP_UCHAR* isoCode, const XP_UCHAR* phony )
{
    DevCtxt* dc = load( dutil, xwe );

    FindState fs = {
        .isoCode = isoCode,
        .phony = phony,
    };
    arr_map( dc->pd, xwe, findPhonyProc, &fs );
    XP_LOGFF( "(%s, %s) => %s", isoCode, phony, boolToStr(fs.foundIt) );
    return fs.foundIt;
}

static void
registerIf( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    XP_U32 atNext = 0;
    XP_U32 len = sizeof(atNext);
    dutil_loadPtr( dutil, xwe, LAST_REG_KEY, &atNext, &len );
    XP_UCHAR gitrev[128];
    len = VSIZE(gitrev);
    dutil_loadPtr( dutil, xwe, KEY_GITREV, gitrev, &len );
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
    DevCtxt* dc = dutil->devCtxt = load( dutil, xwe );
    dc->webSend.data = NULL;
    dc->webSend.key = 0;

    MUTEX_INIT( &dc->mutex, XP_FALSE );
    MUTEX_INIT( &dc->webSend.mutex, XP_FALSE );
    MUTEX_INIT( &dc->ackTimer.mutex, XP_FALSE );

    loadPhoniesData( dutil, xwe, dc );

    mqttInit( dutil, xwe );

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

    cnk_free( dutil->smsChunkerState );
    cleanupBT( dutil );

    MUTEX_DESTROY( &dc->webSend.mutex );
    MUTEX_DESTROY( &dc->ackTimer.mutex );
    MUTEX_DESTROY( &dc->mutex );

    XP_FREEP( dutil->mpool, &dc );
}

GameRef
dvc_makeFromStream( XW_DUtilCtxt* dutil, XWEnv xwe, XWStreamCtxt* stream,
                    const CurGameInfo* gi, XW_UtilCtxt* util, DrawCtx* draw,
                    CommonPrefs* cp )
{
    XP_ASSERT(0);
    XP_USE(dutil);
    XP_USE(xwe);
    XP_USE(stream);
    XP_USE(gi);
    XP_USE(util);
    XP_USE(draw);
    XP_USE(cp);
    return 0;
}

XWStreamCtxt*
dvc_beginUrl( XW_DUtilCtxt* dutil, const XP_UCHAR* host, const XP_UCHAR* prefix )
{
    XWStreamCtxt* stream = dvc_makeStream(dutil);
    XP_UCHAR buf[64];
    dvc_formatUrl( buf, VSIZE(buf), host, prefix );
    strm_catString( stream, buf );
    return stream;
}

void
dvc_formatUrl( XP_UCHAR buf[], XP_U16 bufLen,
               const XP_UCHAR* host, const XP_UCHAR* prefix )
{
    if ( !host ) {
        host = "eehouse.org";
    }
    if ( !prefix ) {
        prefix = "/andd/";
    }
    XP_U16 len = XP_SNPRINTF( buf, bufLen-1, "https://%s%s", host, prefix );
    XP_ASSERT( len < bufLen );
}

