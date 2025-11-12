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

struct MQTTConStorage {
    LaunchParams* params;
    struct mosquitto* mosq;
    struct  {
        MQTTDevID clientID;
        gchar clientIDStr[32];
        gchar* topics[5];
        int nTopics;
        XP_U8 qos;
    } config;
    XP_Bool connected;
    GSList* queue;
};

#define STORAGE_FREED ((MQTTConStorage*)-1)

typedef struct _QElem {
    gchar* topic;
    gchar* sum;
    uint8_t* buf;
    uint16_t len;
    XP_U8 qos;
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
                                       elem->len, elem->buf, elem->qos, true );
                gchar* sum = g_compute_checksum_for_data( G_CHECKSUM_MD5,
                                                          elem->buf, elem->len );
                XP_LOGFF( "mosquitto_publish(topic=%s, msgLen=%d, sum=%s) => %s; mid=%d",
                          elem->topic, elem->len, sum, mosquitto_strerror(err), elem->mid );
                g_free(sum);
                /* Remove this so all are resent together? */
                sts_increment( storage->params->dutil, NULL_XWE, STAT_MQTT_SENT );
                break;
            }
        }
    } else {
        XP_LOGFF( "not connected to broker" );
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
         const XP_U8* buf, XP_U16 len, XP_U8 qos )
{
    FindState fs = {
        .elem.buf = (uint8_t*)buf,
        .elem.len = len,
        .elem.topic = (gchar*)topic,
        .elem.qos = qos,
    };
    g_slist_foreach( storage->queue, findMsg, &fs );

    if ( fs.found ) {
        XP_LOGFF( "dropping duplicate message" );
    } else {
        QElem* elem = g_malloc0( sizeof(*elem) );
        elem->topic = g_strdup( topic );
        elem->buf = G_MEMDUP( buf, len );
        elem->len = len;
        elem->qos = qos;
        elem->sum = g_compute_checksum_for_data( G_CHECKSUM_MD5, buf, len );
        storage->queue = g_slist_append( storage->queue, elem );
        XP_LOGFF( "added elem with sum %s; len now %d", elem->sum,
                  g_slist_length(storage->queue) );

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
        g_free( qe->sum );
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
makeStorage( LaunchParams* params )
{
    XP_ASSERT( !params->mqttConStorage );
    MQTTConStorage* storage = XP_CALLOC( params->mpool, sizeof(*storage) );
    storage->params = params;
    params->mqttConStorage = storage;
    XP_LOGFF( "allocated %p (params: %p, addr: %p, mpool: %p)",
              storage, params, &params->mqttConStorage, params->mpool );
    return storage;
}

static MQTTConStorage* 
getStorage( LaunchParams* params )
{
    MQTTConStorage* storage = params->mqttConStorage;
    if ( storage == STORAGE_FREED ) {
        storage = NULL;
    } else if ( !storage ) {
        storage = params->mqttConStorage = makeStorage(params);
    }
    return storage;
}

static void
loadClientID( MQTTConStorage* storage, const MQTTDevID* devID )
{
    storage->config.clientID = *devID;
    formatMQTTDevID( &storage->config.clientID, storage->config.clientIDStr,
                     VSIZE(storage->config.clientIDStr) );
    XP_ASSERT( 16 == strlen(storage->config.clientIDStr) );
}

typedef struct _MessageIdleData {
    MQTTConStorage* storage;
    gchar* topic;
    gchar* sum;
    short payloadlen;
    unsigned char* payload;
} MessageIdleData;

static gint
handleMessageIdle( gpointer data )
{
    LOG_FUNC();
    MessageIdleData* mid = (MessageIdleData*)data;
    XW_DUtilCtxt* dutil = mid->storage->params->dutil;
    sts_increment( dutil, NULL_XWE, STAT_MQTT_RCVD );

    XP_LOGFF( "processing message of len %d with sum %s",
              mid->payloadlen, mid->sum );
    dvc_parseMQTTPacket( dutil, NULL_XWE, (XP_UCHAR*)mid->topic,
                         mid->payload, mid->payloadlen );

    g_free( mid->payload );
    g_free( mid->topic );
    g_free( mid->sum );
    g_free( mid );

    return FALSE;
}

static void
onMessageReceived( struct mosquitto* XP_UNUSED_DBG(mosq), void *userdata,
                   const struct mosquitto_message* message )
{
    XP_LOGFF( "(len=%d)", message->payloadlen );
    MQTTConStorage* storage = (MQTTConStorage*)userdata;
    XP_ASSERT( storage->mosq == mosq );

    XP_ASSERT( message->payloadlen < 0x7FFF );

    MessageIdleData* mid = g_malloc0(sizeof(*mid));
    mid->storage = storage;
    mid->topic = g_strdup(message->topic);
    mid->payloadlen = message->payloadlen;
    mid->payload = g_memdup2( message->payload, message->payloadlen );

    mid->sum = g_compute_checksum_for_data( G_CHECKSUM_MD5,
                                            mid->payload, mid->payloadlen );
    XP_LOGFF( "got msg of len %d with sum %s", mid->payloadlen, mid->sum );

    g_idle_add( handleMessageIdle, mid );
}

static void
connect_callback( struct mosquitto* mosq, void* userdata,
                  int XP_UNUSED_DBG(connErr) )
{
    XP_LOGFF( "(err=%s)", mosquitto_strerror(connErr) );
    MQTTConStorage* storage = (MQTTConStorage*)userdata;
    storage->connected = XP_TRUE;

    int mid;
    int err = mosquitto_subscribe_multiple( mosq, &mid,
                                            storage->config.nTopics,
                                            storage->config.topics,
                                            storage->config.qos, 0, NULL );
    XP_LOGFF( "mosquitto_subscribe(topics[0]=%s, etc) => %s, mid=%d",
              storage->config.topics[0],
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

void
mqttc_init( LaunchParams* params, const MQTTDevID* devID, const XP_UCHAR** topics,
            XP_U8 qos )
{
    LOG_FUNC();
    if ( types_hasType( params->conTypes, COMMS_CONN_MQTT ) ) {
        MQTTConStorage* storage = getStorage( params );

        loadClientID( storage, devID );

        for ( int ii = 0; ii < VSIZE(storage->config.topics); ++ii ) {
            const XP_UCHAR* topic = topics[ii];
            if (!topic) {
                break;
            }
            storage->config.topics[ii] = g_strdup( topics[ii] );
            ++storage->config.nTopics;
        }

        storage->config.qos = qos;

        int err = mosquitto_lib_init();
        XP_LOGFF( "mosquitto_lib_init() => %d", err );
        XP_ASSERT( 0 == err );

        bool cleanSession = false;
        struct mosquitto* mosq = storage->mosq =
            mosquitto_new( storage->config.clientIDStr, cleanSession, storage );

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
    XP_FREEP( params->mpool, &params->mqttConStorage );
    params->mqttConStorage = STORAGE_FREED;
}

const MQTTDevID*
mqttc_getDevID( LaunchParams* params )
{
    MQTTConStorage* storage = getStorage( params );
    return &storage->config.clientID;
}

const gchar*
mqttc_getDevIDStr( LaunchParams* params )
{
    MQTTConStorage* storage = getStorage( params );
    return storage->config.clientIDStr;
}

static void
msgAndTopicProc( void* closure, const XP_UCHAR* topic, const XP_U8* buf,
                 XP_U16 len, XP_U8 qos )
{
    MQTTConStorage* storage = (MQTTConStorage*)closure;
    (void)enqueue( storage, topic, buf, len, qos );
}

void
mqttc_enqueue( LaunchParams* params, const XP_UCHAR* topic, const XP_U8* buf,
               XP_U16 len, XP_U8 qos )
{
    MQTTConStorage* storage = getStorage( params );
    if ( !!storage ) {
        (void)enqueue( storage, topic, buf, len, qos );
    } else {
        XP_LOGFF( "not sending; storage not yet available or already freed" );
    }
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
