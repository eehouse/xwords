/* -*- compile-command: "make MEMDEBUG=TRUE -j3"; -*- */
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

#include "dutil.h"
#include "mempool.h"
#include "knownplyr.h"
#include "lindutil.h"
#include "linuxutl.h"
#include "linuxmain.h"
#include "gamesdb.h"
#include "dbgutil.h"
#include "LocalizedStrIncludes.h"
#include "nli.h"

#include "linuxdict.h"
#include "cursesmain.h"
#include "gtkmain.h"

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

#ifdef XWFEATURE_DEVID
static const XP_UCHAR* linux_dutil_getDevID( XW_DUtilCtxt* duc, XWEnv xwe, DevIDType* typ );
static void linux_dutil_deviceRegistered( XW_DUtilCtxt* duc, XWEnv xwe, DevIDType typ,
                                          const XP_UCHAR* idRelay );
#endif

#ifdef COMMS_CHECKSUM
static XP_UCHAR* linux_dutil_md5sum( XW_DUtilCtxt* duc, XWEnv xwe, const XP_U8* ptr,
                                     XP_U32 len );
#endif

static void
linux_dutil_notifyPause( XW_DUtilCtxt* XP_UNUSED(duc), XWEnv XP_UNUSED(xwe),
                         XP_U32 XP_UNUSED_DBG(gameID),
                         DupPauseType XP_UNUSED_DBG(pauseTyp),
                         XP_U16 XP_UNUSED_DBG(pauser),
                         const XP_UCHAR* XP_UNUSED_DBG(name),
                         const XP_UCHAR* XP_UNUSED_DBG(msg) )
{
    XP_LOGF( "%s(id=%d, turn=%d, name=%s, typ=%d, %s)", __func__, gameID, pauser,
             name, pauseTyp, msg );
}

static void
linux_dutil_onDupTimerChanged( XW_DUtilCtxt* XP_UNUSED(duc), XWEnv XP_UNUSED(xwe),
                               XP_U32 XP_UNUSED_DBG(gameID),
                               XP_U32 XP_UNUSED_DBG(oldVal),
                               XP_U32 XP_UNUSED_DBG(newVal) )
{
    XP_LOGF( "%s(id=%d, oldVal=%d, newVal=%d)", __func__, gameID, oldVal, newVal );
}

static void
linux_dutil_onInviteReceived( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                              const NetLaunchInfo* nli )
{
    LaunchParams* params = (LaunchParams*)duc->closure;

    if ( params->useCurses ) {
        CommsAddrRec addr = {0};
        nli_makeAddrRec( nli, &addr );
        inviteReceivedCurses( params->appGlobals, nli, &addr );
    } else {
        inviteReceivedGTK( params->appGlobals, nli );
    }
}

static void
linux_dutil_onMessageReceived( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                               XP_U32 gameID, const CommsAddrRec* from,
                               XWStreamCtxt* stream )
{
    XP_LOGFF( "(gameID=%d)", gameID );
    LaunchParams* params = (LaunchParams*)duc->closure;

    XP_U16 len = stream_getSize( stream );
    XP_U8 buf[len];
    stream_getBytes( stream, buf, len );

    if ( params->useCurses ) {
        mqttMsgReceivedCurses( params->appGlobals, from, gameID, buf, len );
    } else {
        msgReceivedGTK( params->appGlobals, from, gameID, buf, len );
    }
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

static void
linux_dutil_ackMQTTMsg( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 gameID,
                        const MQTTDevID* senderID, const XP_U8* msg, XP_U16 len )
{
    XP_USE(duc);
    XP_USE(xwe);
    XP_USE(gameID);
    XP_USE(senderID);
    XP_USE(msg);
    XP_USE(len);
    XP_LOGFF( "doing nothing" );
}

XW_DUtilCtxt*
linux_dutils_init( MPFORMAL VTableMgr* vtMgr, void* closure )
{
    XW_DUtilCtxt* result = XP_CALLOC( mpool, sizeof(*result) );

    dutil_super_init( MPPARM(mpool) result );

    result->vtMgr = vtMgr;
    result->closure = closure;

# define SET_PROC(nam) \
    result->vtable.m_dutil_ ## nam = linux_dutil_ ## nam;

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

#ifdef XWFEATURE_DEVID
    SET_PROC(getDevID);
    SET_PROC(deviceRegistered);
#endif

#ifdef COMMS_CHECKSUM
    SET_PROC(md5sum);
#endif
    SET_PROC(notifyPause);
    SET_PROC(onDupTimerChanged);
    SET_PROC(onInviteReceived);
    SET_PROC(onMessageReceived);
    SET_PROC(onGameGoneReceived);
    SET_PROC(ackMQTTMsg);

# undef SET_PROC

    assertTableFull( &result->vtable, sizeof(result->vtable), "lindutil" );

    return result;
}

void
linux_dutils_free( XW_DUtilCtxt** dutil )
{
    kplr_cleanup( *dutil );
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
#ifndef XWFEATURE_STANDALONE_ONLY
    case STR_LOCALPLAYERS:
        return (XP_UCHAR*)"Local players";
    case STR_REMOTE:
        return (XP_UCHAR*)"Remote";
#endif
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
        return "%d of %d players have reported their moves.";
    case STRD_DUP_TRADED:
        return "No moves made; traded %d tiles";
    case STRSD_DUP_ONESCORE:
        return "%s: %d points\n";

    default:
        XP_LOGF( "%s(code=%d)", __func__, code );
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

    XP_LOGF( "%s(key=%s) => len: %d", __func__, keys[0], *lenp );
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
    XP_LOGF( "%s(%s, %s) => %d", __func__, p1, p2, result );
    return result;
}
#endif

#ifdef XWFEATURE_DEVID
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
        XP_LOGF( "%s: id rejected", __func__ );
        params->lDevID = NULL;
        break;
    case ID_TYPE_RELAY:
        if ( !!params->pDb && 0 < strlen( idRelay ) ) {
            XP_LOGF( "%s: new id: %s", __func__, idRelay );
            gdb_store( params->pDb, KEY_RDEVID, idRelay );
        }
        break;
    default:
        XP_ASSERT(0);
        break;
    }
}
#endif

#ifdef COMMS_CHECKSUM
static XP_UCHAR*
linux_dutil_md5sum( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                    const XP_U8* ptr, XP_U32 len )
{
    gchar* sum = g_compute_checksum_for_data( G_CHECKSUM_MD5, ptr, len );
    XP_U32 sumlen = 1 + strlen( sum );
    XP_UCHAR* result = XP_MALLOC( duc->mpool, sumlen );
    XP_MEMCPY( result, sum, sumlen );
    g_free( sum );
    return result;
}
#endif

