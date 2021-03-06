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

typedef enum { CMD_INVITE, CMD_MSG, CMD_DEVGONE, } MQTTCmd;

// #define PROTO_0 0
#define PROTO_1 1        /* moves gameID into "header" relay2 knows about */

static void
addHeaderGameIDAndCmd( XW_DUtilCtxt* dutil, XWEnv xwe, MQTTCmd cmd,
                       XP_U32 gameID, XWStreamCtxt* stream )
{
    stream_putU8( stream, PROTO_1 );

    MQTTDevID myID;
    dvc_getMQTTDevID( dutil, xwe, &myID );
    myID = htobe64( myID );
    stream_putBytes( stream, &myID, sizeof(myID) );

    stream_putU32( stream, gameID );

    stream_putU8( stream, cmd );
}

void
dvc_makeMQTTInvite( XW_DUtilCtxt* dutil, XWEnv xwe, XWStreamCtxt* stream,
                    const NetLaunchInfo* nli )
{
    addHeaderGameIDAndCmd( dutil, xwe, CMD_INVITE, nli->gameID, stream );
    nli_saveToStream( nli, stream );
}

void
dvc_makeMQTTMessage( XW_DUtilCtxt* dutil, XWEnv xwe, XWStreamCtxt* stream,
                     XP_U32 gameID, const XP_U8* buf, XP_U16 len )
{
    addHeaderGameIDAndCmd( dutil, xwe, CMD_MSG, gameID, stream );
    stream_putBytes( stream, buf, len );
}

void
dvc_makeMQTTNoSuchGame( XW_DUtilCtxt* dutil, XWEnv xwe,
                        XWStreamCtxt* stream, XP_U32 gameID )
{
    addHeaderGameIDAndCmd( dutil, xwe, CMD_DEVGONE, gameID, stream );
}

void
dvc_parseMQTTPacket( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_U8* buf, XP_U16 len )
{
    LOG_FUNC();
    XWStreamCtxt* stream = mkStream( dutil );
    stream_putBytes( stream, buf, len );

    XP_U8 proto = stream_getU8( stream );
    if ( proto != PROTO_1 ) {
        XP_LOGFF( "read proto %d, expected %d; dropping packet",
                  proto, PROTO_1 );
    } else {
        MQTTDevID senderID;
        stream_getBytes( stream, &senderID, sizeof(senderID) );
        senderID = be64toh( senderID );
#ifdef DEBUG
        XP_UCHAR tmp[32];
        formatMQTTDevID( &senderID, tmp, VSIZE(tmp) );
        XP_LOGFF( "senderID: %s", tmp );
#endif

        MQTTCmd cmd;
        XP_U32 gameID = 0;

        gameID = stream_getU32( stream );
        cmd = stream_getU8( stream );

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
                dutil_onMessageReceived( dutil, xwe, gameID, &from, stream );
            } else if ( CMD_DEVGONE == cmd ) {
                dutil_onGameGoneReceived( dutil, xwe, gameID, &from );
            }
        }
            break;
        default:
            XP_LOGFF( "unknown command %d; dropping message", cmd );
            XP_ASSERT(0);
        }
    }
    stream_destroy( stream, xwe );
}
