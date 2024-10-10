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

#include "linuxdict.h"
#include "cursesmain.h"
#include "gtkmain.h"
#include "mqttcon.h"



typedef struct _LinDUtilCtxt {
    XW_DUtilCtxt super;
#ifdef DUTIL_TIMERS
    MutexState timersMutex;
    GSList* timers;
#endif
} LinDUtilCtxt;

static XP_U32 linux_dutil_getCurSeconds( XW_DUtilCtxt* duc, XWEnv xwe );
static const XP_UCHAR* linux_dutil_getUserString( XW_DUtilCtxt* duc, XWEnv xwe, XP_U16 code );
static const XP_UCHAR* linux_dutil_getUserQuantityString( XW_DUtilCtxt* duc, XWEnv xwe, XP_U16 code,
                                                          XP_U16 quantity );

static void linux_dutil_storePtr( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* keys[],
                                  const void* data, XP_U32 len );
static void linux_dutil_loadPtr( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* keys[],
                                 void* data, XP_U32* lenp );
static void linux_dutil_forEach( XW_DUtilCtxt* duc, XWEnv xwe,
                                 const XP_UCHAR* keys[],
                                 OnOneProc proc, void* closure );
static void linux_dutil_remove( XW_DUtilCtxt* duc, const XP_UCHAR* keys[] );

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
linux_dutil_notifyPause( XW_DUtilCtxt* XP_UNUSED(duc), XWEnv XP_UNUSED(xwe),
                         XP_U32 XP_UNUSED_DBG(gameID),
                         DupPauseType XP_UNUSED_DBG(pauseTyp),
                         XP_U16 XP_UNUSED_DBG(pauser),
                         const XP_UCHAR* XP_UNUSED_DBG(name),
                         const XP_UCHAR* XP_UNUSED_DBG(msg) )
{
    XP_LOGFF( "(id=%d, turn=%d, name=%s, typ=%d, %s)", gameID, pauser,
              name, pauseTyp, msg );
}

static XP_Bool
linux_dutil_haveGame( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                      XP_U32 gameID, XP_U8 channel )
{
    LaunchParams* params = (LaunchParams*)duc->closure;
    sqlite3* pDb = params->pDb;

    sqlite3_int64 rowids[MAX_NUM_PLAYERS];
    int nRowIDs = VSIZE(rowids);
    gdb_getRowsForGameID( pDb, gameID, rowids, &nRowIDs );
    XP_Bool result = 0 < nRowIDs;
    /* XP_Bool result = XP_FALSE; */
    /* for ( int ii = 0; ii < nRowIDs; ++ii ) { */
    /*     GameInfo gib; */
    /*     if ( ! gdb_getGameInfoForRow( pDb, rowids[ii], &gib ) ) { */
    /*         XP_ASSERT(0); */
    /*     } */
    /*     if ( gib.channelNo == channel ) { */
    /*         result = XP_TRUE; */
    /*     } */
    /* } */
    XP_LOGFF( "(gameID=%X, channel=%d) => %s",
              gameID, channel, boolToStr(result) );
    return result;
}

static void
linux_dutil_onDupTimerChanged( XW_DUtilCtxt* XP_UNUSED(duc), XWEnv XP_UNUSED(xwe),
                               XP_U32 XP_UNUSED_DBG(gameID),
                               XP_U32 XP_UNUSED_DBG(oldVal),
                               XP_U32 XP_UNUSED_DBG(newVal) )
{
    XP_LOGFF( "(id=%d, oldVal=%d, newVal=%d)", gameID, oldVal, newVal );
}

static void
linux_dutil_onInviteReceived( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                              const NetLaunchInfo* nli )
{
    LaunchParams* params = (LaunchParams*)duc->closure;

    gchar* sum = g_compute_checksum_for_data( G_CHECKSUM_MD5, (unsigned char*)nli,
                                              sizeof(*nli) );
    XP_LOGFF( "sum: %s", sum );
    g_free( sum );

    if ( params->useCurses ) {
        inviteReceivedCurses( params->appGlobals, nli );
    } else {
        inviteReceivedGTK( params->appGlobals, nli );
    }
    mqttc_onInviteHandled( params, nli );
}

static void
linux_dutil_onMessageReceived( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                               XP_U32 gameID, const CommsAddrRec* from,
                               const XP_U8* buf, XP_U16 len )
{
    XP_LOGFF( "(gameID=%X)", gameID );
    LaunchParams* params = (LaunchParams*)duc->closure;

    if ( params->useCurses ) {
        mqttMsgReceivedCurses( params->appGlobals, from, gameID, buf, len );
    } else {
        msgReceivedGTK( params->appGlobals, from, gameID, buf, len );
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

static void
linux_dutil_onGameGoneReceived( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                                XP_U32 gameID, const CommsAddrRec* from )
{
    LaunchParams* params = (LaunchParams*)duc->closure;
    if ( params->useCurses ) {
        gameGoneCurses( params->appGlobals, from, gameID );
    } else {
        gameGoneGTK( params->appGlobals, from, gameID );
    }
}

typedef struct _SendViaData {
    LinDUtilCtxt* lduc;
    char* pstr;
    char* api;
    XP_U32 resultKey;
} SendViaData;

typedef struct _FetchData {
    char* payload;
    size_t size;
} FetchData;

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

static void*
sendViaThreadProc( void* arg )
{
    XP_Bool succeeded = XP_TRUE;
    SendViaData* svdp = (SendViaData*)arg;

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

    FetchData fd = {};

    curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, curl_callback );
    curl_easy_setopt( curl, CURLOPT_WRITEDATA, (void *) &fd );

    res = curl_easy_perform( curl );
    if ( res != CURLE_OK ) {
        XP_LOGFF( "curl_easy_perform() failed: %s", curl_easy_strerror(res));
        succeeded = XP_FALSE;
    } else {
        XP_LOGFF( "got buffer: %s", fd.payload );
    }

    curl_slist_free_all( headers );
    curl_easy_cleanup( curl );

    if ( svdp->resultKey ) {
        dvc_onWebSendResult( &svdp->lduc->super, NULL_XWE, svdp->resultKey,
                             succeeded, fd.payload );
    }

    free( fd.payload );
    free( svdp->pstr );
    g_free( svdp->api );
    g_free( svdp );

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

#ifdef DUTIL_TIMERS
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

    XP_LOGFF( "key: %d, inWhenMS: %d", key, inWhenMS );
    tc->src = g_timeout_add( inWhenMS, timer_proc, tc );

    LinDUtilCtxt* lduc = (LinDUtilCtxt*)duc;
    WITH_MUTEX( &lduc->timersMutex );
    lduc->timers = g_slist_append( lduc->timers, tc );
    END_WITH_MUTEX();

    XP_LOGFF( "after setting, length %d", g_slist_length(lduc->timers) );
}


#endif

XW_DUtilCtxt*
linux_dutils_init( MPFORMAL VTableMgr* vtMgr, void* closure )
{
    LinDUtilCtxt* lduc = XP_CALLOC( mpool, sizeof(*lduc) );

    XW_DUtilCtxt* super = &lduc->super;

    MUTEX_INIT( &lduc->timersMutex, XP_TRUE );

    super->vtMgr = vtMgr;
    super->closure = closure;

# define SET_PROC(nam) \
    super->vtable.m_dutil_ ## nam = linux_dutil_ ## nam;

    SET_PROC(getCurSeconds);
    SET_PROC(getUserString);
    SET_PROC(getUserQuantityString);
    SET_PROC(storePtr);
    SET_PROC(loadPtr);
    SET_PROC(forEach);
    SET_PROC(remove);

#ifdef XWFEATURE_SMS
    SET_PROC(phoneNumbersSame);
#endif

#if defined XWFEATURE_DEVID && defined XWFEATURE_RELAY
    SET_PROC(getDevID);
    SET_PROC(deviceRegistered);
#endif

    SET_PROC(md5sum);
    SET_PROC(getUsername);
    SET_PROC(notifyPause);
    SET_PROC(haveGame);
    SET_PROC(onDupTimerChanged);
    SET_PROC(onInviteReceived);
    SET_PROC(onMessageReceived);
    SET_PROC(onCtrlReceived);
    SET_PROC(onGameGoneReceived);
    SET_PROC(sendViaWeb);
    SET_PROC(getRegValues);
#ifdef DUTIL_TIMERS
    SET_PROC(setTimer);
    SET_PROC(clearTimer);
#endif

# undef SET_PROC

    dutil_super_init( MPPARM(mpool) super );

    assertTableFull( &super->vtable, sizeof(super->vtable), "lindutil" );

    return super;
}

void
linux_dutils_free( XW_DUtilCtxt** dutil )
{
    dutil_super_cleanup( *dutil, NULL_XWE );
    LinDUtilCtxt* lduc = (LinDUtilCtxt*)*dutil;
    MUTEX_DESTROY(&lduc->timersMutex);
# ifdef MEM_DEBUG
    XP_FREEP( (*dutil)->mpool, dutil );
# endif
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
                      const XP_UCHAR* keys[],
                      const void* data, const XP_U32 len )
{
    XP_ASSERT( keys[1] == NULL );
    LaunchParams* params = (LaunchParams*)duc->closure;
    sqlite3* pDb = params->pDb;

    gchar* b64 = g_base64_encode( data, len);
    gdb_store( pDb, keys[0], b64 );
    g_free( b64 );
}

static void
linux_dutil_loadPtr( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                     const XP_UCHAR* keys[],
                     void* data, XP_U32* lenp )
{
    XP_ASSERT( NULL == keys[1] );
    LaunchParams* params = (LaunchParams*)duc->closure;
    sqlite3* pDb = params->pDb;

    gint buflen = 0;
    FetchResult res = gdb_fetch( pDb, keys[0], NULL, NULL, &buflen );
    if ( res == BUFFER_TOO_SMALL ) { /* expected: I passed 0 */
        if ( 0 == *lenp ) {
            *lenp = buflen;
        } else {
            gchar* tmp = XP_MALLOC( duc->mpool, buflen );
            gint tmpLen = buflen;
            res = gdb_fetch( pDb, keys[0], NULL, tmp, &tmpLen );
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

    XP_LOGFF( "(key=%s) => len: %d", keys[0], *lenp );
}

static void
linux_dutil_forEach( XW_DUtilCtxt* XP_UNUSED(duc),
                     XWEnv XP_UNUSED(xwe),
                     const XP_UCHAR* XP_UNUSED(keys[]),
                     OnOneProc XP_UNUSED(proc), void* XP_UNUSED(closure) )
{
    XP_ASSERT(0);
}

static void
linux_dutil_remove( XW_DUtilCtxt* XP_UNUSED(duc), const XP_UCHAR* XP_UNUSED(keys[]) )
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

