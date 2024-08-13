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
#include "stats.h"

typedef struct _MQTTConStorage {
    LaunchParams* params;
    struct mosquitto* mosq;
    MQTTDevID clientID;
    gchar clientIDStr[32];
    int msgPipe[2];
    XP_Bool connected;
    GSList* queue;
} MQTTConStorage;

#define DEFAULT_QOS 2

typedef struct _QElem {
    gchar* topic;
    uint8_t* buf;
    uint16_t len;
    int mid;
} QElem;

static void
sendQueueHead( MQTTConStorage* storage )
{
    XP_LOGFF( "queue len: %d", g_slist_length(storage->queue) );
    if ( storage->connected ) {
        for ( GSList* iter = storage->queue; !!iter; iter = iter->next ) {
            QElem* elem = (QElem*)iter->data;
            if ( 0 == elem->mid ) {
#ifdef DEBUG
                int err =
#endif
                    mosquitto_publish( storage->mosq, &elem->mid, elem->topic,
                                       elem->len, elem->buf, DEFAULT_QOS, true );
                XP_LOGFF( "mosquitto_publish(topic=%s, msgLen=%d) => %s; mid=%d", elem->topic,
                          elem->len, mosquitto_strerror(err), elem->mid );
                /* Remove this so all are resent together? */
                sts_increment( storage->params->dutil, NULL_XWE, STAT_MQTT_SENT );
                break;
            }
        }
    }
    LOG_RETURN_VOID();
} /* sendQueueHead */

typedef struct _FindState {
    QElem elem;
    XP_Bool found;
} FindState;

static bool
elemsEqual( QElem* qe1, QElem* qe2 )
{
    return qe1->len == qe2->len
        && 0 == strcmp( qe1->topic, qe2->topic )
        && 0 == memcmp( qe1->buf, qe2->buf, qe1->len );
}

static void
findMsg( gpointer data, gpointer user_data )
{
    QElem* qe = (QElem*)data;
    FindState* fsp = (FindState*)user_data;
    if ( !fsp->found && elemsEqual( qe, &fsp->elem ) ) {
        fsp->found = XP_TRUE;
    }
}

static gint
queueIdle( gpointer data )
{
    MQTTConStorage* storage = (MQTTConStorage*)data;
    sendQueueHead( storage );
    return FALSE;
}

static void
tickleQueue( MQTTConStorage* storage )
{
    ADD_ONETIME_IDLE( queueIdle, storage );
}

// Ubuntu 22.4 is at 2.72.4 right now. Debian bookworm at 2.74.6. Ubuntu 20.4
// is at 2.64.6
#if GLIB_CHECK_VERSION(2,68,0)
# define G_MEMDUP(a,b) g_memdup2((a), (b))
#else
# define G_MEMDUP(a,b) g_memdup((a), (b))
#endif

/* Add to queue if not already there */
static void
enqueue( MQTTConStorage* storage, const char* topic,
         const XP_U8* buf, XP_U16 len )
{
    FindState fs = {
        .elem.buf = (uint8_t*)buf,
        .elem.len = len,
        .elem.topic = (gchar*)topic,
    };
    g_slist_foreach( storage->queue, findMsg, &fs );

    if ( fs.found ) {
        XP_LOGFF( "dropping duplicate message" );
    } else {
        QElem* elem = g_malloc0( sizeof(*elem) );
        elem->topic = g_strdup( topic );
        elem->buf = G_MEMDUP( buf, len );
        elem->len = len;
        storage->queue = g_slist_append( storage->queue, elem );
        XP_LOGFF( "added elem; len now %d", g_slist_length(storage->queue) );

        tickleQueue( storage );
    }
} /* enqueue */

typedef struct _RemoveState {
    MQTTConStorage* storage;
    int mid;
    XP_Bool found;
} RemoveState;

static void
removeWithMid( gpointer data, gpointer user_data )
{
    QElem* qe = (QElem*)data;
    RemoveState* rsp = (RemoveState*)user_data;
    if ( qe->mid == rsp->mid ) {
        XP_ASSERT( !rsp->found );
        rsp->found = XP_TRUE;
        MQTTConStorage* storage = rsp->storage;
        storage->queue = g_slist_remove( storage->queue, qe );
        XP_LOGFF( "removed elem with mid %d; len now %d", rsp->mid,
                  g_slist_length(storage->queue) );

        g_free( qe->topic );
        g_free( qe->buf );
        g_free( qe );
    }
}

static gint
dequeueIdle( gpointer data )
{
    LOG_FUNC();
    RemoveState* rsp = (RemoveState*)data;
    XP_ASSERT( !rsp->found );

    g_slist_foreach( rsp->storage->queue, removeWithMid, rsp );
    if ( !rsp->found ) {
        XP_LOGFF( "failed to find mid %d", rsp->mid );
    }
    g_free( rsp );
    return FALSE;
}

static void
dequeue_on_idle( MQTTConStorage* storage, int mid )
{
    RemoveState* rsp = g_malloc0( sizeof(*rsp) );
    rsp->storage = storage;
    rsp->mid = mid;
    // ADD_ONETIME_IDLE() has trouble with multiple instance with same idle
    // proc, so:
    /*guint res = */g_idle_add( dequeueIdle, rsp );
}

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
    XP_USE(err);

    tickleQueue( storage );
} /* connect_callback */

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
publish_callback( struct mosquitto* XP_UNUSED(mosq), void* userdata, int mid )
{
    XP_LOGFF( "publish of mid %d successful", mid );
    MQTTConStorage* storage = (MQTTConStorage*)userdata;
    dequeue_on_idle( storage, mid );
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
    MQTTConStorage* storage = (MQTTConStorage*)data;
    // XP_LOGFF( "(len=%d)", message->payloadlen );
    LOG_FUNC();
    XW_DUtilCtxt* dutil = storage->params->dutil;
    sts_increment( dutil, NULL_XWE, STAT_MQTT_RCVD );

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
    LOG_RETURN_VOID();
    return TRUE;
} /* handle_gotmsg */

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
        mosquitto_publish_callback_set( mosq, publish_callback );

        int keepalive = 60;
        err = mosquitto_connect( mosq, params->connInfo.mqtt.hostName,
                                 params->connInfo.mqtt.port, keepalive );
        XP_LOGFF( "mosquitto_connect(host=%s, port=%d) => %s",
                  params->connInfo.mqtt.hostName,
                  params->connInfo.mqtt.port,
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
    (void)mosquitto_loop_stop( storage->mosq, true ); /* blocks until thread dies */
    mosquitto_destroy( storage->mosq );
    storage->mosq = NULL;
	mosquitto_lib_cleanup();

    XP_LOGFF( "quitting with %d undelivered messages",
              g_slist_length(storage->queue) );

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
    (void)enqueue( storage, topic, buf, len );
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
            const SendMsgsPacket* const msgs,
            XP_U16 streamVersion, const MQTTDevID* addressee  )
{
    MQTTConStorage* storage = getStorage( params );
    XP_S16 nSent = dvc_makeMQTTMessages( params->dutil, NULL_XWE,
                                         msgAndTopicProc, storage,
                                         msgs, addressee,
                                         gameID, streamVersion );
    return nSent;
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
