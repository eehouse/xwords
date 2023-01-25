/* -*- compile-command: "make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 2020 - 2022 by Eric House (xwords@eehouse.org).  All rights
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
    int msgPipe[2];
    XP_Bool connected;
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
}

static void
onMessageReceived( struct mosquitto* XP_UNUSED_DBG(mosq), void *userdata,
                   const struct mosquitto_message* message )
{
    XP_LOGFF( "(len=%d)", message->payloadlen );
    MQTTConStorage* storage = (MQTTConStorage*)userdata;
    XP_ASSERT( storage->mosq == mosq );

    XP_ASSERT( message->payloadlen < 0x7FFF );

    const int msgPipe = storage->msgPipe[1];
    /* write topic, then message */
    const char* topic = message->topic;
    short msgLen = htons(1 + strlen(topic));
    write( msgPipe, &msgLen, sizeof(msgLen) );
    write( msgPipe, topic, 1 + strlen(topic) );

    short len = (short)message->payloadlen;
    len = htons( len );
    write( msgPipe, &len, sizeof(len) );
    write( msgPipe, message->payload, message->payloadlen );
}

static void
connect_callback( struct mosquitto* mosq, void* userdata,
                  int XP_UNUSED_DBG(connErr) )
{
    XP_LOGFF( "(err=%s)", mosquitto_strerror(connErr) );
    MQTTConStorage* storage = (MQTTConStorage*)userdata;
    storage->connected = XP_TRUE;

    XP_UCHAR topicStorage[256];
    XP_UCHAR* topics[4];
    XP_U16 nTopics = VSIZE(topics);
    dvc_getMQTTSubTopics( storage->params->dutil, NULL_XWE,
                          topicStorage, VSIZE(topicStorage),
                          &nTopics, topics );
    int mid;
    int err = mosquitto_subscribe_multiple( mosq, &mid, nTopics, topics,
                                            DEFAULT_QOS, 0, NULL );
    XP_LOGFF( "mosquitto_subscribe(topics[0]=%s, etc) => %s, mid=%d", topics[0],
              mosquitto_strerror(err), mid );
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

    short topicLen;
#ifdef DEBUG
    ssize_t nRead =
#endif
        read( pipe, &topicLen, sizeof(topicLen) );
    XP_ASSERT( nRead == sizeof(topicLen) );
    topicLen = ntohs(topicLen);
    XP_U8 topicBuf[topicLen];
#ifdef DEBUG
    nRead =
#endif
        read( pipe, topicBuf, topicLen );
    XP_ASSERT( nRead == topicLen);
    XP_ASSERT( '\0' == topicBuf[topicLen-1] );

    short msgLen;
#ifdef DEBUG
    nRead =
#endif
        read( pipe, &msgLen, sizeof(msgLen) );
    XP_ASSERT( nRead == sizeof(msgLen) );
    msgLen = ntohs(msgLen);
    XP_U8 msgBuf[msgLen];
#ifdef DEBUG
    nRead =
#endif
        read( pipe, msgBuf, msgLen );
    XP_ASSERT( nRead == msgLen );

    dvc_parseMQTTPacket( storage->params->dutil, NULL_XWE,
                         (XP_UCHAR*)topicBuf, msgBuf, msgLen );

    return TRUE;
} /* handle_gotmsg */

static bool
postOne( MQTTConStorage* storage, const XP_UCHAR* topic, const XP_U8* buf, XP_U16 len )
{
    int err = -1;
    if ( storage->connected ) {
        int mid;
        err = mosquitto_publish( storage->mosq, &mid, topic,
                                 len, buf, DEFAULT_QOS, true );
        XP_LOGFF( "mosquitto_publish(topic=%s, len=%d) => %s; mid=%d", topic,
                  len, mosquitto_strerror(err), mid );
    } else {
        XP_LOGFF( "not connected and so not sending" );
    }
    return 0 == err;
}

void
mqttc_init( LaunchParams* params )
{
    if ( types_hasType( params->conTypes, COMMS_CONN_MQTT ) ) {
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
            err = mosquitto_loop_start( mosq );
            XP_ASSERT( !err );
        } else {
            XP_LOGFF( "failed to connect so not proceeding" );
        }
    } else {
        XP_LOGFF( "MQTT disabled; doing nothing" );
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


static void
msgAndTopicProc( void* closure, const XP_UCHAR* topic, const XP_U8* buf, XP_U16 len )
{
    MQTTConStorage* storage = (MQTTConStorage*)closure;
    (void)postOne( storage, topic, buf, len );
}

void
mqttc_invite( LaunchParams* params, const NetLaunchInfo* nli,
              const MQTTDevID* invitee )
{
    MQTTConStorage* storage = getStorage( params );
#ifdef DEBUG
    gchar buf[32];
    XP_LOGFF( "need to send to %s", formatMQTTDevID(invitee, buf, sizeof(buf) ) );
    XP_ASSERT( 16 == strlen(buf) );
#endif

    dvc_makeMQTTInvites( params->dutil, NULL_XWE, msgAndTopicProc, storage,
                         invitee, nli );
}

XP_S16
mqttc_send( LaunchParams* params, XP_U32 gameID,
            const XP_U8* buf, XP_U16 len, const MQTTDevID* addressee )
{
    MQTTConStorage* storage = getStorage( params );

    dvc_makeMQTTMessages( params->dutil, NULL_XWE,
                          msgAndTopicProc, storage,
                          addressee, gameID, buf, len );
    return len;
}

void
mqttc_onInviteHandled( LaunchParams* params, const NetLaunchInfo* nli )
{
    LOG_FUNC();
    MQTTConStorage* storage = getStorage( params );
    dvc_makeMQTTNukeInvite( params->dutil, NULL_XWE,
                            msgAndTopicProc, storage, nli );
}

void
mqttc_notifyGameGone( LaunchParams* params, const MQTTDevID* addressee, XP_U32 gameID )
{
    MQTTConStorage* storage = getStorage( params );
    dvc_makeMQTTNoSuchGames( params->dutil, NULL_XWE,
                             msgAndTopicProc, storage,
                             addressee, gameID );
}
