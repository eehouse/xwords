/* -*- compile-command: "make MEMDEBUG=TRUE -j3"; -*- */
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

#include <mosquitto.h>

#include "mqttcon.h"
#include "gsrcwrap.h"
#include "device.h"
#include "strutils.h"

typedef struct _MQTTConStorage {
    LaunchParams* params;
    struct mosquitto* mosq;
    MQTTDevID clientID;
    gchar clientIDStr[32];
    gchar topic[64];
    int msgPipe[2];
} MQTTConStorage;

#define DEFAULT_QOS 2

static MQTTConStorage* 
getStorage( LaunchParams* params )
{
    MQTTConStorage* storage = (MQTTConStorage*)params->mqttConStorage;
    if ( NULL == storage ) {
        storage = XP_CALLOC( params->mpool, sizeof(*storage) );
        params->mqttConStorage = storage;
    }
    return storage;
}

static void
loadClientID( LaunchParams* params, MQTTConStorage* storage )
{
    dvc_getMQTTDevID( params->dutil, NULL_XWE, &storage->clientID );
    formatMQTTDevID( &storage->clientID, storage->clientIDStr,
                     VSIZE(storage->clientIDStr) );
    XP_ASSERT( 16 == strlen(storage->clientIDStr) );
    formatMQTTTopic( &storage->clientID, storage->topic, VSIZE(storage->topic) );
}

static void
onMessageReceived( struct mosquitto* XP_UNUSED_DBG(mosq), void *userdata,
                  const struct mosquitto_message* message )
{
    XP_LOGFF( "(len=%d)", message->payloadlen );
    MQTTConStorage* storage = (MQTTConStorage*)userdata;
    XP_ASSERT( storage->mosq == mosq );
    // XP_ASSERT( 0 == XP_STRCMP( message->topic, storage->clientID ) );

    XP_ASSERT( message->payloadlen < 0x7FFF );
    short len = (short)message->payloadlen;
    len = htons( len );

    write( storage->msgPipe[1], &len, sizeof(len) );
    write( storage->msgPipe[1], message->payload, message->payloadlen );
}

static void
connect_callback( struct mosquitto *mosq, void *userdata, int XP_UNUSED_DBG(err) )
{
    XP_LOGFF( "(err=%s)", mosquitto_strerror(err) );
    XP_USE(mosq);
    XP_USE(userdata);
	/* int i; */
	/* if(!result){ */
	/* 	/\* Subscribe to broker information topics on successful connect. *\/ */
	/* 	mosquitto_subscribe(mosq, NULL, "$SYS/#", 2); */
	/* }else{ */
	/* 	fprintf(stderr, "Connect failed\n"); */
	/* } */
}

static void
subscribe_callback( struct mosquitto *mosq, void *userdata, int mid,
                    int qos_count, const int *granted_qos)
{
    XP_USE(mosq);
    XP_USE(userdata);
    XP_USE(mid);
    XP_USE(qos_count);
    XP_USE(granted_qos);
	XP_LOGFF ("Subscribed (mid: %d): %d", mid, granted_qos[0]);
	for ( int ii = 1; ii < qos_count; ii++ ) {
		XP_LOGFF(", %d", granted_qos[ii]);
	}
}

static void
log_callback( struct mosquitto *mosq, void *userdata, int level,
              const char* str )
{
    XP_USE(mosq);
    XP_USE(userdata);
    XP_USE(level);
    XP_USE(str);
    /* XP_LOGFF( "msg: %s", str ); */
}

static gboolean
handle_gotmsg( GIOChannel* source, GIOCondition XP_UNUSED(condition), gpointer data )
{
    // XP_LOGFF( "(len=%d)", message->payloadlen );
    LOG_FUNC();
    MQTTConStorage* storage = (MQTTConStorage*)data;

    int pipe = g_io_channel_unix_get_fd( source );
    XP_ASSERT( pipe == storage->msgPipe[0] );
    short len;
#ifdef DEBUG
    ssize_t nRead =
#endif
        read( pipe, &len, sizeof(len) );
    XP_ASSERT( nRead == sizeof(len) );
    len = ntohs(len);
    XP_U8 buf[len];
#ifdef DEBUG
    nRead =
#endif
        read( pipe, buf, len );
    XP_ASSERT( nRead == len );

    dvc_parseMQTTPacket( storage->params->dutil, NULL_XWE, buf, len );

    return TRUE;
} /* handle_gotmsg */

static bool
postMsg( MQTTConStorage* storage, XWStreamCtxt* stream, const MQTTDevID* invitee )
{
    const XP_U8* bytes = stream_getPtr( stream );
    XP_U16 len = stream_getSize( stream );

    int mid;

#ifdef DEBUG
    XP_UCHAR* sum = dutil_md5sum( storage->params->dutil, NULL_XWE, bytes, len );
    XP_LOGFF( "sending %d bytes with sum %s", len, sum );
    XP_FREEP( storage->params->mpool, &sum );
#endif

    gchar buf[32];
    int err = mosquitto_publish( storage->mosq, &mid,
                                 formatMQTTTopic( invitee, buf, sizeof(buf) ),
                                 len, bytes, DEFAULT_QOS, false );
    XP_LOGFF( "mosquitto_publish(topic=%s) => %s; mid=%d", buf, mosquitto_strerror(err), mid );

    stream_destroy( stream, NULL_XWE );
    return err == 0;
}

void
mqttc_init( LaunchParams* params )
{

    XP_ASSERT( !params->mqttConStorage );
    MQTTConStorage* storage = getStorage( params );
    storage->params = params;

    loadClientID( params, storage );
#ifdef DEBUG
    int res =
#endif
        pipe( storage->msgPipe );
    XP_ASSERT( !res );
    ADD_SOCKET( storage, storage->msgPipe[0], handle_gotmsg );
    
    int err = mosquitto_lib_init();
    XP_LOGFF( "mosquitto_lib_init() => %d", err );
    XP_ASSERT( 0 == err );

    bool cleanSession = false;
    struct mosquitto* mosq = storage->mosq =
        mosquitto_new( storage->clientIDStr, cleanSession, storage );

    err = mosquitto_username_pw_set( mosq, "xwuser", "xw4r0cks" );
    XP_LOGFF( "mosquitto_username_pw_set() => %s", mosquitto_strerror(err) );

    mosquitto_log_callback_set( mosq, log_callback );
	mosquitto_connect_callback_set( mosq, connect_callback );
	mosquitto_message_callback_set( mosq, onMessageReceived );
	mosquitto_subscribe_callback_set( mosq, subscribe_callback );

	int keepalive = 60;
	err = mosquitto_connect( mosq, params->connInfo.mqtt.hostName,
                             params->connInfo.mqtt.port, keepalive );
    XP_LOGFF( "mosquitto_connect(host=%s) => %s", params->connInfo.mqtt.hostName,
              mosquitto_strerror(err) );
    if ( MOSQ_ERR_SUCCESS == err ) {
        int mid;
        err = mosquitto_subscribe( mosq, &mid, storage->topic, DEFAULT_QOS );
        XP_LOGFF( "mosquitto_subscribe(topic=%s) => %s, mid=%d", storage->topic,
                  mosquitto_strerror(err), mid );

        err = mosquitto_loop_start( mosq );
        XP_ASSERT( !err );
    } else {
        XP_LOGFF( "failed to connect so not proceeding" );
    }
}

void
mqttc_cleanup( LaunchParams* params )
{
    MQTTConStorage* storage = getStorage( params );
#ifdef DEBUG
    int err =
#endif
        mosquitto_loop_stop( storage->mosq, true ); /* blocks until thread dies */
    XP_LOGFF( "mosquitto_loop_stop() => %s", mosquitto_strerror(err) );
    mosquitto_destroy( storage->mosq );
    storage->mosq = NULL;
	mosquitto_lib_cleanup();

    XP_ASSERT( params->mqttConStorage == storage ); /* cheat */
    XP_FREEP( params->mpool, &storage );
    params->mqttConStorage = NULL;
}

const MQTTDevID*
mqttc_getDevID( LaunchParams* params )
{
    MQTTConStorage* storage = getStorage( params );
    return &storage->clientID;
}

const gchar*
mqttc_getDevIDStr( LaunchParams* params )
{
    MQTTConStorage* storage = getStorage( params );
    return storage->clientIDStr;
}

void
mqttc_invite( LaunchParams* params, XP_U32 timestamp, const NetLaunchInfo* nli,
              const MQTTDevID* invitee )
{
    MQTTConStorage* storage = getStorage( params );
#ifdef DEBUG
    gchar buf[32];
    XP_LOGFF( "need to send to %s", formatMQTTDevID(invitee, buf, sizeof(buf) ) );
    XP_ASSERT( 16 == strlen(buf) );
#endif
    XP_USE( timestamp );

    XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(params->mpool)
                                                params->vtMgr );

    dvc_makeMQTTInvite( params->dutil, NULL_XWE, stream, nli, 0 );

    postMsg( storage, stream, invitee );
}

XP_S16
mqttc_send( LaunchParams* params, XP_U32 gameID, XP_U32 timestamp,
            const XP_U8* buf, XP_U16 len, const MQTTDevID* addressee )
{
    XP_S16 result = -1;
    MQTTConStorage* storage = getStorage( params );
    XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(params->mpool)
                                                params->vtMgr );

    dvc_makeMQTTMessage( params->dutil, NULL_XWE, stream,
                         gameID, timestamp, buf, len );
    if ( postMsg( storage, stream, addressee ) ) {
        result = len;
    }
    return result;
}

void
mqttc_notifyGameGone( LaunchParams* params, const MQTTDevID* addressee, XP_U32 gameID )
{
    MQTTConStorage* storage = getStorage( params );
    XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(params->mpool)
                                                params->vtMgr );
    dvc_makeMQTTNoSuchGame( params->dutil, NULL_XWE, stream, gameID, 0 );
    postMsg( storage, stream, addressee );
}
