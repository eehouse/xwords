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
# define KEY_DEVSTATE PERSIST_KEY("devState")

typedef struct _DevCtxt {
    XP_U16 devCount;
} DevCtxt;

static DevCtxt*
load( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    LOG_FUNC();
    DevCtxt* state = (DevCtxt*)dutil->devCtxt;
    if ( NULL == state ) {
        XWStreamCtxt* stream = mkStream( dutil );
        dutil_loadStream( dutil, xwe, KEY_DEVSTATE, stream );

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
    LOG_FUNC();
    DevCtxt* state = load( dutil, xwe );
    XWStreamCtxt* stream = mkStream( dutil );
    stream_putU16( stream, state->devCount );
    dutil_storeStream( dutil, xwe, KEY_DEVSTATE, stream );
    stream_destroy( stream, NULL );

    XP_FREEP( dutil->mpool, &dutil->devCtxt );
}

#endif

#define MQTT_DEVID_KEY "mqtt_devid_key"

void
dvc_getMQTTDevID( XW_DUtilCtxt* dutil, XWEnv xwe, MQTTDevID* devID )
{
    MQTTDevID tmp = 0;
    XP_U16 len = sizeof(tmp);
    dutil_loadPtr( dutil, xwe, MQTT_DEVID_KEY, &tmp, &len );
    // XP_LOGFF( "len: %d; sizeof(tmp): %d", len, sizeof(tmp) );
    if ( len != sizeof(tmp) ) { /* we have it!!! */
        tmp = XP_RANDOM();
        tmp <<= 32;
        tmp |= XP_RANDOM();
        dutil_storePtr( dutil, xwe, MQTT_DEVID_KEY, &tmp, sizeof(tmp) );
#ifdef DEBUG
        XP_UCHAR buf[32];
        formatMQTTDevID( &tmp, buf, VSIZE(buf) );
        /* This log statement is required by discon_ok2.py!!! (keep in sync) */
        XP_LOGFF( "generated id: %s", buf );
#endif
    }
    *devID = tmp;
    // LOG_RETURNF( MQTTDevID_FMT, *devID );
}

typedef enum { CMD_INVITE, CMD_MSG, CMD_DEVGONE, } MQTTCmd;

#define PROTO_0 0

static void
addHeaderCmdAndGameID( XW_DUtilCtxt* dutil, XWEnv xwe, MQTTCmd cmd,
                       XP_U32 gameID, XWStreamCtxt* stream )
{
    stream_putU8( stream, PROTO_0 );

    MQTTDevID myID;
    dvc_getMQTTDevID( dutil, xwe, &myID );
    myID = htobe64( myID );
    stream_putBytes( stream, &myID, sizeof(myID) );

    stream_putU8( stream, cmd );

    if ( gameID != 0 ) {
        stream_putU32( stream, gameID );
    }
}

void
dvc_makeMQTTInvite( XW_DUtilCtxt* dutil, XWEnv xwe, XWStreamCtxt* stream,
                    const NetLaunchInfo* nli )
{
    LOG_FUNC();
    addHeaderCmdAndGameID( dutil, xwe, CMD_INVITE, 0, stream );
    nli_saveToStream( nli, stream );
}

void
dvc_makeMQTTMessage( XW_DUtilCtxt* dutil, XWEnv xwe, XWStreamCtxt* stream,
                     XP_U32 gameID, const XP_U8* buf, XP_U16 len )
{
    LOG_FUNC();
    addHeaderCmdAndGameID( dutil, xwe, CMD_MSG, gameID, stream );
    stream_putBytes( stream, buf, len );
}

void
dvc_makeMQTTNoSuchGame( XW_DUtilCtxt* dutil, XWEnv xwe,
                        XWStreamCtxt* stream, XP_U32 gameID )
{
    addHeaderCmdAndGameID( dutil, xwe, CMD_DEVGONE, gameID, stream );
}

void
dvc_parseMQTTPacket( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_U8* buf, XP_U16 len )
{
    LOG_FUNC();
    XWStreamCtxt* stream = mkStream( dutil );
    stream_putBytes( stream, buf, len );

    XP_U8 proto = stream_getU8( stream );
    if ( PROTO_0 != proto ) {
        XP_LOGFF( "read proto %d, expected %d; dropping packet", proto, PROTO_0 );
    } else {
        MQTTDevID myID;
        stream_getBytes( stream, &myID, sizeof(myID) );
        myID = be64toh( myID );
        MQTTCmd cmd = stream_getU8( stream );
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
            from.u.mqtt.devID = myID;
            XP_U32 gameID = stream_getU32( stream );
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
