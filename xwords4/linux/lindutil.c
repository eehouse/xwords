/* 
 * Copyright 2018 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <curl/curl.h>

#include "dutil.h"
#include "device.h"
#include "mempool.h"
#include "knownplyr.h"
#include "lindutil.h"
#include "linuxutl.h"
#include "linuxmain.h"
#include "gamesdb.h"
#include "dbgutil.h"
#include "LocalizedStrIncludes.h"
#include "nli.h"
#include "strutils.h"
#include "xwmutex.h"
#include "gamemgr.h"

#include "linuxdict.h"
#include "cursesmain.h"
#include "gtkmain.h"
#include "gtkdraw.h"
#include "mqttcon.h"
#include "linuxsms.h"
#include "linuxbt.h"
#include "gsrcwrap.h"

typedef struct _LinDUtilCtxt {
    XW_DUtilCtxt super;
    MutexState timersMutex;
    GSList* timers;
} LinDUtilCtxt;

static DrawCtx* linux_dutil_getThumbDraw( XW_DUtilCtxt* duc, XWEnv xwe,
                                          GameRef gr );
static XP_U32 linux_dutil_getCurSeconds( XW_DUtilCtxt* duc, XWEnv xwe );
static const XP_UCHAR* linux_dutil_getUserString( XW_DUtilCtxt* duc, XWEnv xwe, XP_U16 code );
static const XP_UCHAR* linux_dutil_getUserQuantityString( XW_DUtilCtxt* duc, XWEnv xwe, XP_U16 code,
                                                          XP_U16 quantity );

static void linux_dutil_storePtr( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key,
                                  const void* data, XP_U32 len );
static void linux_dutil_loadPtr( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key,
                                 void* data, XP_U32* lenp );
static void linux_dutil_removeStored( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key );
static void linux_dutil_getKeysLike( XW_DUtilCtxt* duc, XWEnv xwe,
                                     const XP_UCHAR* pattern, OnGotKey proc,
                                     void* closure );
static void linux_dutil_forEach( XW_DUtilCtxt* duc, XWEnv xwe,
                                 const XP_UCHAR* keys[],
                                 OnOneProc proc, void* closure );
#ifdef XWFEATURE_SMS
static XP_Bool  linux_dutil_phoneNumbersSame( XW_DUtilCtxt* duc, XWEnv xwe,
                                              const XP_UCHAR* p1,
                                              const XP_UCHAR* p2 );
#endif

#if defined XWFEATURE_DEVID && defined XWFEATURE_RELAY
static const XP_UCHAR* linux_dutil_getDevID( XW_DUtilCtxt* duc, XWEnv xwe, DevIDType* typ );
static void linux_dutil_deviceRegistered( XW_DUtilCtxt* duc, XWEnv xwe, DevIDType typ,
                                          const XP_UCHAR* idRelay );
#endif

static void linux_dutil_md5sum( XW_DUtilCtxt* duc, XWEnv xwe, const XP_U8* ptr,
                                XP_U32 len, Md5SumBuf* sb );

static void
linux_dutil_getUsername( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                         XP_U16 num, XP_Bool XP_UNUSED(isLocal), XP_Bool isRobot,
                         XP_UCHAR* buf, XP_U16* len )
{
    LaunchParams* params = (LaunchParams*)duc->closure;
    if ( params->localName ) {
        *len = XP_SNPRINTF( buf, *len, "%s", params->localName );
        XP_LOGFF( "set using local name: %s", buf );
    } else {
        const char* fmt = isRobot ? "Robot %d" : "Player %d";
        *len = XP_SNPRINTF( buf, *len, fmt, num );
    }
}

static void
linux_dutil_getSelfAddr( XW_DUtilCtxt* duc, XWEnv xwe, CommsAddrRec* addr )
{
    LaunchParams* params = (LaunchParams*)duc->closure;

    if ( !params->skipMQTTAdd ) {
        MQTTDevID devID;
        dvc_getMQTTDevID( duc, xwe, &devID );
        addr_addMQTT( addr, &devID );
    }

    if ( !!params->connInfo.sms.myPhone ) {
        addr_addSMS( addr, params->connInfo.sms.myPhone,
                     params->connInfo.sms.port );
    }

    /* Some test for this? */
    if ( !params->disableBT ) {
        BTHostPair hp;
        lbt_setToSelf( params, &hp );
        addr_addBT( addr, hp.hostName, hp.btAddr.chars );
    }
}

static void
linux_dutil_notifyPause( XW_DUtilCtxt* XP_UNUSED(duc), XWEnv XP_UNUSED(xwe),
                         GameRef XP_UNUSED_DBG(gr),
                         DupPauseType XP_UNUSED_DBG(pauseTyp),
                         XP_U16 XP_UNUSED_DBG(pauser),
                         const XP_UCHAR* XP_UNUSED_DBG(name),
                         const XP_UCHAR* XP_UNUSED_DBG(msg) )
{
    XP_LOGFF( "(gr=%lX, turn=%d, name=%s, typ=%d, %s)", gr, pauser,
              name, pauseTyp, msg );
}

static void
linux_dutil_informMove( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe), GameRef gr,
                        XP_S16 XP_UNUSED(turn), XWStreamCtxt* expl,
                        XWStreamCtxt* words )
{

    LaunchParams* params = (LaunchParams*)duc->closure;
    if ( params->useCurses ) {
        informMoveCurses( params, expl );
    } else {
        informMoveGTK( params, gr, expl, words );
    }
}

static void
linux_dutil_notifyGameOver( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                            GameRef gr, XP_S16 quitter )
{
    LaunchParams* params = (LaunchParams*)duc->closure;
    if ( params->useCurses ) {
        informGameOverCurses( params, gr, quitter );
    } else {
        informGameOverGTK( params, gr, quitter );
    }
}

/* static XP_Bool */
/* linux_dutil_haveGame( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe), */
/*                       XP_U32 gameID, XP_U8 channel ) */
/* { */
/*     LaunchParams* params = (LaunchParams*)duc->closure; */
/*     sqlite3* pDb = params->pDb; */
/*     sqlite3_int64 rowids[MAX_NUM_PLAYERS]; */
/*     int nRowIDs = VSIZE(rowids); */
/*     gdb_getRowsForGameID( pDb, gameID, rowids, &nRowIDs ); */
/*     XP_Bool result = XP_FALSE; */
/*     for ( int ii = 0; ii < nRowIDs; ++ii ) { */
/*         GameInfo gib; */
/*         if ( ! gdb_getGameInfoForRow( pDb, rowids[ii], &gib ) ) { */
/*             XP_ASSERT(0); */
/*         } */
/*         if ( gib.channelNo == channel ) { */
/*             result = XP_TRUE; */
/*         } */
/*     } */
/*     XP_LOGFF( "(gameID=%X, channel=%d) => %s", */
/*               gameID, channel, boolToStr(result) ); */
/*     return result; */
/* } */

static void
linux_dutil_onDupTimerChanged( XW_DUtilCtxt* XP_UNUSED(duc), XWEnv XP_UNUSED(xwe),
                               GameRef XP_UNUSED_DBG(gr),
                               XP_U32 XP_UNUSED_DBG(oldVal),
                               XP_U32 XP_UNUSED_DBG(newVal) )
{
    XP_LOGFF( "(gr=%lX, oldVal=%d, newVal=%d)", gr, oldVal, newVal );
}

static void
linux_dutil_onGroupChanged( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp,
                            GroupChangeEvents gces )
{
    LaunchParams* params = (LaunchParams*)duc->closure;
    if ( params->useCurses ) {
        XP_UCHAR buf[64];
        gmgr_getGroupName( duc, xwe, grp, buf, VSIZE(buf) );
        XP_LOGFF( "grp=%d, name=%s", grp, buf );
    } else {
        onGroupChangedGTK( params, grp, gces );
    }
}

static void
linux_dutil_onCtrlReceived( XW_DUtilCtxt* duc, XWEnv xwe,
                            const XP_U8* buf, XP_U16 len )
{
    XP_USE(duc);
    XP_USE(xwe);
    XP_USE(buf);
    XP_USE(len);
    XP_LOGFF( "got msg len %d", len );
}

typedef struct _FetchData {
    char* payload;
    size_t size;
} FetchData;

typedef struct _SendViaData {
    LinDUtilCtxt* lduc;
    char* pstr;
    char* api;
    XP_U32 resultKey;
    XP_Bool succeeded;
    FetchData fd;
} SendViaData;

static size_t
curl_callback( void *contents, size_t size, size_t nmemb, void *userp )
{
    size_t realsize = size * nmemb;
    FetchData* fdp = (FetchData*)userp;
    XP_LOGFF( "(realsize: %zu)", realsize );

    fdp->payload = (char *) realloc(fdp->payload, fdp->size + realsize + 1);
    memcpy(&(fdp->payload[fdp->size]), contents, realsize);
    fdp->size += realsize;
    fdp->payload[fdp->size] = 0;
    return realsize;
}

static gint
onGotData( gpointer data )
{
    SendViaData* svdp = (SendViaData*)data;
    if ( svdp->resultKey ) {
        dvc_onWebSendResult( &svdp->lduc->super, NULL_XWE, svdp->resultKey,
                             svdp->succeeded, svdp->fd.payload );
    }

    free( svdp->fd.payload );
    free( svdp->pstr );
    g_free( svdp->api );
    g_free( svdp );

    return FALSE;
}

static void*
sendViaThreadProc( void* arg )
{
    SendViaData* svdp = (SendViaData*)arg;
    svdp->succeeded = XP_TRUE;

    const LaunchParams* params = (LaunchParams*)svdp->lduc->super.closure;

    XP_LOGFF( "(api: %s, json: %s)", svdp->api, svdp->pstr );

    CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT); /* ok to call multiple times */
    XP_ASSERT(res == CURLE_OK);

    CURL* curl = curl_easy_init();

    char url[128];
    /* Don't use https. Doing so triggers a race condition/crash in openssl as
       called by curl. See https://curl.se/libcurl/c/threaded-ssl.html (which
       has a fix to allow using https, but I'm to lazy to work it out now.) */
    const char* proto = "http";
    snprintf( url, sizeof(url), "%s://%s/xw4/api/v1/%s", proto,
              params->connInfo.mqtt.hostName, svdp->api );
    curl_easy_setopt( curl, CURLOPT_URL, url );
    XP_LOGFF( "url: %s", url );

    curl_easy_setopt( curl, CURLOPT_POSTFIELDS, svdp->pstr );
    curl_easy_setopt( curl, CURLOPT_POSTFIELDSIZE, -1L );

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Expect:");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);


    curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, curl_callback );
    curl_easy_setopt( curl, CURLOPT_WRITEDATA, (void *) &svdp->fd );

    res = curl_easy_perform( curl );
    if ( res != CURLE_OK ) {
        XP_LOGFF( "curl_easy_perform() failed: %s", curl_easy_strerror(res));
        svdp->succeeded = XP_FALSE;
    } else {
        XP_LOGFF( "got buffer: %s", svdp->fd.payload );
    }

    curl_slist_free_all( headers );
    curl_easy_cleanup( curl );

    ADD_ONETIME_IDLE( onGotData, svdp );
    return NULL;
}

static void
linux_dutil_sendViaWeb( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                        XP_U32 resultKey, const XP_UCHAR* api,
                        const cJSON* json )
{
    LinDUtilCtxt* lduc = (LinDUtilCtxt*)duc;

    SendViaData svd = {
        .lduc = lduc,
        .pstr = cJSON_PrintUnformatted( json ),
        .api = g_strdup(api),
        .resultKey = resultKey,
    };
    SendViaData* svdp = g_memdup2( &svd, sizeof(svd) );

    pthread_t thrd;
    (void)pthread_create( &thrd, NULL, sendViaThreadProc, svdp );
    pthread_detach( thrd );
}

static DictionaryCtxt*
linux_dutil_makeEmptyDict( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe) )
{
    XP_DEBUGF( "linux_util_makeEmptyDict called" );
    LaunchParams* params = (LaunchParams*)duc->closure;
    XP_USE(params);
    return linux_dictionary_make( MPPARM(params->mpool) NULL, NULL, XP_FALSE );
}

static const DictionaryCtxt*
linux_dutil_makeDict( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                      const XP_UCHAR* dictName )
{
    LaunchParams* params = (LaunchParams*)duc->closure;
    DictionaryCtxt* result = linux_dictionary_make( MPPARM(params->mpool)
                                                    params, dictName, XP_TRUE );
    return result;
}

static void
linux_dutil_missingDictAdded( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe), GameRef gr,
                              const XP_UCHAR* dictName )
{
    XP_LOGFF( "(gr=" GR_FMT ", name=%s)", gr, dictName );
    LaunchParams* params = (LaunchParams*)duc->closure;
    if ( params->useCurses ) {
    } else {
        onGTKMissingDictAdded( params, gr, dictName );
    }
    XP_USE( params );
}

static void
linux_dutil_dictGone( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe), GameRef gr,
                      const XP_UCHAR* dictName )
{
    XP_LOGFF( "(gr=" GR_FMT ", name=%s)", gr, dictName );
    LaunchParams* params = (LaunchParams*)duc->closure;
    if ( params->useCurses ) {
    } else {
        onGTKDictGone( params, gr, dictName );
    }
    XP_USE( params );
}

typedef struct _MQTTParams {
    LaunchParams* params;
    const XP_UCHAR* topics[8];
    MQTTDevID devID;
    XP_U8 qos;
} MQTTParams;

static gint
startMQTT( gpointer data )
{
    MQTTParams* mqttp = (MQTTParams*)data;
    mqttc_init( mqttp->params, &mqttp->devID, mqttp->topics, mqttp->qos );

    for ( int ii = 0; ; ++ii ) {
        XP_UCHAR* topic = (XP_UCHAR*)mqttp->topics[ii];
        if ( !topic ) { break; }
        g_free( topic );
    }
    g_free( mqttp );
    return FALSE;
}

static void
linux_dutil_startMQTTListener( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                               const MQTTDevID* devID, const XP_UCHAR** topics,
                               XP_U8 qos )
{
    MQTTParams* mqttp = g_malloc0( sizeof(*mqttp) );
    mqttp->params = (LaunchParams*)duc->closure;
    mqttp->devID = *devID;
    mqttp->qos = qos;
    for ( int ii = 0; ; ++ii ) {
        const XP_UCHAR* topic = topics[ii];
        if ( !topic ) {
            break;
        }
        mqttp->topics[ii] = g_strdup(topic);
    }
    ADD_ONETIME_IDLE( startMQTT, mqttp );
}

static XP_S16
linux_dutil_sendViaMQTT( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                         const XP_UCHAR* topic, const XP_U8* buf,
                         XP_U16 len, XP_U8 qos )
{
    LaunchParams* params = (LaunchParams*)duc->closure;
    mqttc_enqueue( params, topic, buf, len, qos );
    return -1;
}

static XP_S16
linux_dutil_sendViaBT( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                       const XP_U8* buf, XP_U16 len,
                       const XP_UCHAR* hostName,
                       const XP_BtAddrStr* btAddr )
{
    XP_LOGFF( "sending %d bytes to %s", len, hostName );
    LaunchParams* params = (LaunchParams*)duc->closure;
    return lbt_send( params, buf, len, hostName, btAddr );
}

static XP_S16
linux_dutil_sendViaNFC( XW_DUtilCtxt* duc, XWEnv xwe,
                        const XP_U8* buf, XP_U16 len,
                        XP_U32 gameID )
{
    XP_ASSERT(0);   /* This should never get called...  */
    XP_USE(duc);
    XP_USE(xwe);
    XP_USE(buf);
    XP_USE(len);
    XP_USE(gameID);
    return -1;
}

static XP_S16
linux_dutil_sendViaNBS( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                        const XP_U8* buf, XP_U16 len,
                        const XP_UCHAR* phone, XP_U16 port )
{
    LaunchParams* params = (LaunchParams*)duc->closure;
    linux_sms_enqueue( params, buf, len, phone, port );
    return -1;
}

static void
linux_dutil_onKnownPlayersChange( XW_DUtilCtxt* XP_UNUSED(duc),
                                  XWEnv XP_UNUSED(xwe) )
{
    // LOG_FUNC();
}

static void
linux_dutil_onGameChanged( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe), GameRef gr,
                           GameChangeEvents gces )
{
    XP_LOGFF( "(gces=0X%X)", gces );
    LaunchParams* params = (LaunchParams*)duc->closure;
    if ( params->useCurses ) {
        onGameChangedCurses( params->cag, gr, gces );
    } else {
        onGameChangedGTK( params, gr, gces );
    }
}

static void
linux_dutil_getCommonPrefs( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe), CommonPrefs* cp )
{
    LaunchParams* params = (LaunchParams*)duc->closure;
    cpFromLP( cp, params );
}

static cJSON*
linux_dutil_getRegValues( XW_DUtilCtxt* duc, XWEnv xwe )
{
    XP_USE(xwe);

    LaunchParams* params = (LaunchParams*)duc->closure;
    const char* localName = params->localName;
    if ( !localName ) {
        localName = "Linux";
    }

    cJSON* results = cJSON_CreateObject();
    cJSON_AddStringToObject( results, "os", localName );
    cJSON_AddStringToObject( results, "vers", "DEBUG" );

    return results;
}

typedef struct _TimerClosure {
    XW_DUtilCtxt* duc;
    TimerKey key;
    guint src;
} TimerClosure;

static gint
findByProc( gconstpointer elemData, gconstpointer keyp )
{
    TimerClosure* tc = (TimerClosure*)elemData;
    TimerKey* key = (TimerKey*)keyp;
    return (tc->key == *key) ? 0 : 1; /* return 0 on success */
}

static void
linux_dutil_clearTimer( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe), TimerKey key )
{
    LinDUtilCtxt* lduc = (LinDUtilCtxt*)duc;
    WITH_MUTEX( &lduc->timersMutex );
    XP_ASSERT( !!lduc->timers ); /* should be at least one */
    if ( !!lduc->timers ) {
        GSList* elem = g_slist_find_custom( lduc->timers, &key, findByProc );
        XP_ASSERT( !!elem );

        TimerClosure* tc = (TimerClosure*)elem->data;
        XP_ASSERT( tc->key == key );
        lduc->timers = g_slist_remove( lduc->timers, tc );

        g_source_remove( tc->src );
        g_free( tc );
    }
    END_WITH_MUTEX();
}

static gint
timer_proc( gpointer data )
{
    TimerClosure* tc = (TimerClosure*)data;
    dvc_onTimerFired( tc->duc, NULL_XWE, tc->key );

    linux_dutil_clearTimer( tc->duc, NULL_XWE, tc->key );

    return G_SOURCE_REMOVE;
}

static void
linux_dutil_setTimer( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 inWhenMS, TimerKey key )
{
    XP_USE(xwe);        /* I assume I'll need this on the Android side */

    TimerClosure* tc = g_malloc(sizeof(*tc));
    tc->duc = duc;
    tc->key = key;

    LinDUtilCtxt* lduc = (LinDUtilCtxt*)duc;
    WITH_MUTEX( &lduc->timersMutex );
    lduc->timers = g_slist_append( lduc->timers, tc );
    END_WITH_MUTEX();

    /* XP_LOGFF( "key: %d, inWhenMS: %d", key, inWhenMS ); */
    tc->src = g_timeout_add( inWhenMS, timer_proc, tc );
    /* XP_LOGFF( "after setting, length %d", g_slist_length(lduc->timers) ); */
}

XW_DUtilCtxt*
linux_dutils_init( MPFORMAL void* closure )
{
    LinDUtilCtxt* lduc = XP_CALLOC( mpool, sizeof(*lduc) );

    XW_DUtilCtxt* super = &lduc->super;

    MUTEX_INIT( &lduc->timersMutex, XP_TRUE );

    super->closure = closure;

# define SET_PROC(nam) \
    super->vtable.m_dutil_ ## nam = linux_dutil_ ## nam;

    SET_PROC(getThumbDraw);
    SET_PROC(getCurSeconds);
    SET_PROC(getUserString);
    SET_PROC(getUserQuantityString);
    SET_PROC(storePtr);
    SET_PROC(loadPtr);
    SET_PROC(removeStored);
    SET_PROC(getKeysLike);
    SET_PROC(forEach);

#ifdef XWFEATURE_SMS
    SET_PROC(phoneNumbersSame);
#endif

#if defined XWFEATURE_DEVID && defined XWFEATURE_RELAY
    SET_PROC(getDevID);
    SET_PROC(deviceRegistered);
#endif

    SET_PROC(md5sum);
    SET_PROC(getUsername);
    SET_PROC(getSelfAddr);
    SET_PROC(notifyPause);
    SET_PROC(informMove);
    SET_PROC(notifyGameOver);
    /* SET_PROC(haveGame); */
    SET_PROC(onDupTimerChanged);
    SET_PROC(onGroupChanged);
#ifndef XWFEATURE_DEVICE_STORES
    SET_PROC(onInviteReceived);
    SET_PROC(onMessageReceived);
    SET_PROC(onGameGoneReceived);
#endif
    SET_PROC(onCtrlReceived);
    SET_PROC(sendViaWeb);
    SET_PROC(makeEmptyDict);
    SET_PROC(makeDict);
    SET_PROC(missingDictAdded);
    SET_PROC(dictGone);
    SET_PROC(startMQTTListener);
    SET_PROC(sendViaMQTT);
    SET_PROC(sendViaBT);
    SET_PROC(sendViaNFC);
    SET_PROC(sendViaNBS);
    SET_PROC(onKnownPlayersChange);
    SET_PROC(onGameChanged);
    SET_PROC(getCommonPrefs);

    SET_PROC(getRegValues);
    SET_PROC(setTimer);
    SET_PROC(clearTimer);

# undef SET_PROC

    dutil_super_init( MPPARM(mpool) super, NULL_XWE );

    assertTableFull( &super->vtable, sizeof(super->vtable), "lindutil" );

    return super;
}

void
linux_dutils_free( XW_DUtilCtxt** dutil )
{
    dutil_super_cleanup( *dutil, NULL_XWE );
    MUTEX_DESTROY(&((LinDUtilCtxt*)*dutil)->timersMutex);
# ifdef MEM_DEBUG
    XP_FREEP( (*dutil)->mpool, dutil );
# endif
}

static DrawCtx*
linux_dutil_getThumbDraw( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                          GameRef XP_UNUSED(gr) )
{
    LaunchParams* params = (LaunchParams*)duc->closure;
    DrawCtx* dctx = NULL;
    if ( params->useCurses ) {
    } else {
        dctx = gtkDrawCtxtMake( NULL, NULL, DT_THUMB );
        XP_LOGFF( "allocated thumb: %p", dctx );
        addSurface( dctx, THUMB_WIDTH, THUMB_HEIGHT );
    }
    return dctx;
}

static XP_U32
linux_dutil_getCurSeconds( XW_DUtilCtxt* XP_UNUSED(duc), XWEnv XP_UNUSED(xwe) )
{
    return linux_getCurSeconds();
}

static const XP_UCHAR*
linux_dutil_getUserString( XW_DUtilCtxt* XP_UNUSED(uc),
                           XWEnv XP_UNUSED(xwe), XP_U16 code )
{
    switch( code ) {
    case STRD_REMAINING_TILES_ADD:
        return (XP_UCHAR*)"+ %d [all remaining tiles]";
    case STRD_UNUSED_TILES_SUB:
        return (XP_UCHAR*)"- %d [unused tiles]";
    case STR_COMMIT_CONFIRM:
        return (XP_UCHAR*)"Are you sure you want to commit the current move?\n";
    case STR_SUBMIT_CONFIRM:
        return (XP_UCHAR*)"Submit the current move?\n";
    case STRD_TURN_SCORE:
        return (XP_UCHAR*)"Score for turn: %d\n";
    case STR_BONUS_ALL:
        return (XP_UCHAR*)"Bonus for using all tiles: 50\n";
    case STR_BONUS_ALL_SUB:
        return (XP_UCHAR*)"Bonus for using at least %d tiles: 50\n";
    case STR_PENDING_PLAYER:
        return (XP_UCHAR*)"(remote)";
    case STRD_TIME_PENALTY_SUB:
        return (XP_UCHAR*)" - %d [time]";
        /* added.... */
    case STRD_CUMULATIVE_SCORE:
        return (XP_UCHAR*)"Cumulative score: %d\n";
    case STRS_TRAY_AT_START:
        return (XP_UCHAR*)"Tray at start: %s\n";
    case STRS_MOVE_DOWN:
        return (XP_UCHAR*)"move (from %s down)\n";
    case STRS_MOVE_ACROSS:
        return (XP_UCHAR*)"move (from %s across)\n";
    case STRS_NEW_TILES:
        return (XP_UCHAR*)"New tiles: %s\n";
    case STRSS_TRADED_FOR:
        return (XP_UCHAR*)"Traded %s for %s.";
    case STR_PASS:
        return (XP_UCHAR*)"pass\n";
    case STR_PHONY_REJECTED:
        return (XP_UCHAR*)"Illegal word in move; turn lost!\n";

    case STRD_ROBOT_TRADED:
        return (XP_UCHAR*)"%d tiles traded this turn.";
    case STR_ROBOT_MOVED:
        return (XP_UCHAR*)"The robot \"%s\" moved:\n";
    case STRS_REMOTE_MOVED:
        return (XP_UCHAR*)"Remote player \"%s\" moved:\n";
    case STR_LOCALPLAYERS:
        return (XP_UCHAR*)"Local players";
    case STR_REMOTE:
        return (XP_UCHAR*)"Remote";
    case STR_TOTALPLAYERS:
        return (XP_UCHAR*)"Total players";

    case STRS_VALUES_HEADER:
        return (XP_UCHAR*)"%s counts/values:\n";

    case STRD_REMAINS_HEADER:
        return (XP_UCHAR*)"%d tiles left in pool.";
    case STRD_REMAINS_EXPL:
        return (XP_UCHAR*)"%d tiles left in pool and hidden trays:\n";

    case STRSD_RESIGNED:
        return "[Resigned] %s: %d";
    case STRSD_WINNER:
        return "[Winner] %s: %d";
    case STRDSD_PLACER:
        return "[#%d] %s: %d";
    case STR_DUP_MOVED:
        return (XP_UCHAR*)"Duplicate turn complete. Scores:\n";
    case STR_DUP_CLIENT_SENT:
        return "This device has sent its moves to the host. When all players "
            "have sent their moves it will be your turn again.";
    case STRDD_DUP_HOST_RECEIVED:
        return "%d of %d players have reported their moves. When all moves have "
            "been received it will be your turn again.";
    case STRD_DUP_TRADED:
        return "No moves made; traded %d tiles";
    case STRSD_DUP_ONESCORE:
        return "%s: %d points\n";

    case STRS_GROUPS_DEFAULT:
        return "New games";

    case STRS_GROUPS_ARCHIVE:
        return "Archive";

    default:
        XP_LOGFF( "(code=%d)", code );
        return (XP_UCHAR*)"unknown code";
    }
} /* linux_dutil_getUserString */

static const XP_UCHAR*
linux_dutil_getUserQuantityString( XW_DUtilCtxt* duc, XWEnv xwe, XP_U16 code,
                                   XP_U16 XP_UNUSED(quantity) )
{
    return linux_dutil_getUserString( duc, xwe, code );
}

static void
linux_dutil_storePtr( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                      const XP_UCHAR* key,
                      const void* data, const XP_U32 len )
{
    LaunchParams* params = (LaunchParams*)duc->closure;
    sqlite3* pDb = params->pDb;

    gchar* b64 = g_base64_encode( data, len);
    gdb_store( pDb, key, b64 );
    g_free( b64 );
}

static void
linux_dutil_loadPtr( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                     const XP_UCHAR* key,
                     void* data, XP_U32* lenp )
{
    LaunchParams* params = (LaunchParams*)duc->closure;
    sqlite3* pDb = params->pDb;

    gint buflen = 0;
    FetchResult res = gdb_fetch( pDb, key, NULL, NULL, &buflen );
    if ( res == BUFFER_TOO_SMALL ) { /* expected: I passed 0 */
        if ( 0 == *lenp ) {
            *lenp = buflen;
        } else {
            gchar* tmp = XP_MALLOC( duc->mpool, buflen );
            gint tmpLen = buflen;
            res = gdb_fetch( pDb, key, NULL, tmp, &tmpLen );
            XP_ASSERT( buflen == tmpLen );
            XP_ASSERT( res == SUCCESS );
            XP_ASSERT( tmp[buflen-1] == '\0' );

            gsize out_len;
            guchar* binp = g_base64_decode( tmp, &out_len );
            if ( out_len <= *lenp ) {
                XP_MEMCPY( data, binp, out_len );
                *lenp = out_len;
            }
            XP_FREEP( duc->mpool, &tmp );
            g_free( binp );
        }
    } else {
        *lenp = 0;              /* doesn't exist */
    }

    XP_LOGFF( "(key=%s) => len: %d", key, *lenp );
}

static void
linux_dutil_removeStored( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                          const XP_UCHAR* key )
{
    LaunchParams* params = (LaunchParams*)duc->closure;
    sqlite3* pDb = params->pDb;
    gdb_remove( pDb, key );
}

static void
linux_dutil_getKeysLike( XW_DUtilCtxt* duc, XWEnv xwe,
                         const XP_UCHAR* pattern, OnGotKey proc,
                         void* closure )
{
    XP_LOGFF( "(pattern: %s)", pattern );
    LaunchParams* params = (LaunchParams*)duc->closure;
    GSList* keys = gdb_keysLike( params->pDb, pattern );
    for ( GSList* iter = keys; !!iter; iter = iter->next ) {
        XP_UCHAR* key = iter->data;
        (*proc)(key, closure, xwe);
    }
    gdb_freeKeysList( keys );
}

static void
linux_dutil_forEach( XW_DUtilCtxt* XP_UNUSED(duc),
                     XWEnv XP_UNUSED(xwe),
                     const XP_UCHAR* XP_UNUSED(keys[]),
                     OnOneProc XP_UNUSED(proc), void* XP_UNUSED(closure) )
{
    XP_ASSERT(0);
}

#ifdef XWFEATURE_SMS
static XP_Bool
linux_dutil_phoneNumbersSame( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                              const XP_UCHAR* p1, const XP_UCHAR* p2 )
{
    LOG_FUNC();
    XP_USE( duc );
    XP_Bool result = 0 == strcmp( p1, p2 );
    XP_LOGFF( "(%s, %s) => %d", p1, p2, result );
    return result;
}
#endif

#if defined XWFEATURE_DEVID && defined XWFEATURE_RELAY
static const XP_UCHAR*
linux_dutil_getDevID( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe), DevIDType* typ )
{
    LaunchParams* params = (LaunchParams*)duc->closure;
    return linux_getDevID( params, typ );
}

static void
linux_dutil_deviceRegistered( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe), DevIDType typ,
                             const XP_UCHAR* idRelay )
{
    /* Script discon_ok2.sh is grepping for these strings in logs, so don't
       change them! */
    LaunchParams* params = (LaunchParams*)duc->closure;
    switch( typ ) {
    case ID_TYPE_NONE: /* error case */
        XP_LOGFF( "id rejected" );
        params->lDevID = NULL;
        break;
    case ID_TYPE_RELAY:
        if ( !!params->pDb && 0 < strlen( idRelay ) ) {
            XP_LOGFF( "new id: %s", idRelay );
            gdb_store( params->pDb, KEY_RDEVID, idRelay );
        }
        break;
    default:
        XP_ASSERT(0);
        break;
    }
}
#endif

static void
linux_dutil_md5sum( XW_DUtilCtxt* XP_UNUSED(duc), XWEnv XP_UNUSED(xwe),
                    const XP_U8* ptr, XP_U32 len, Md5SumBuf* sb )
{
    gchar* sum = g_compute_checksum_for_data( G_CHECKSUM_MD5, ptr, len );
    XP_U32 sumlen = 1 + strlen( sum );
    // XP_UCHAR* result = XP_MALLOC( duc->mpool, sumlen );
    XP_MEMCPY( sb->buf, sum, sumlen );
    g_free( sum );
}

