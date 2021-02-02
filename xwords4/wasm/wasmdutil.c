#include <time.h>

#include "wasmdutil.h"
#include "dbgutil.h"
#include "LocalizedStrIncludes.h"

static XP_U32
wasm_dutil_getCurSeconds( XW_DUtilCtxt* XP_UNUSED(duc), XWEnv XP_UNUSED(xwe) )
{
    LOG_FUNC();
    return (XP_U32)time(NULL);//tv.tv_sec;
}

static const XP_UCHAR*
wasm_dutil_getUserString( XW_DUtilCtxt* duc, XWEnv xwe, XP_U16 code )
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
}

static const XP_UCHAR*
wasm_dutil_getUserQuantityString( XW_DUtilCtxt* duc, XWEnv xwe, XP_U16 code,
                                  XP_U16 quantity )
{
    LOG_FUNC();
    return NULL;
}

static void
wasm_dutil_storeStream( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key,
                        XWStreamCtxt* data )
{
    LOG_FUNC();
}

static void
wasm_dutil_loadStream( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key,
                       const XP_UCHAR* keySuffix, XWStreamCtxt* inOut )
{
    LOG_FUNC();
}

static void
wasm_dutil_storePtr( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key,
                      const void* data, XP_U16 len )
{
    LOG_FUNC();
}

static void
wasm_dutil_loadPtr( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key,
                    const XP_UCHAR* keySuffix, void* data, XP_U16* lenp )
{
    LOG_FUNC();
}

static const XP_UCHAR*
wasm_dutil_getDevID( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe), DevIDType* typ )
{
    LOG_FUNC();
    return NULL;
}

static void
wasm_dutil_deviceRegistered( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe), DevIDType typ,
                             const XP_UCHAR* idRelay )
{
    LOG_FUNC();
}

static XP_UCHAR*
wasm_dutil_md5sum( XW_DUtilCtxt* duc, XWEnv xwe, const XP_U8* ptr,
                   XP_U16 len )
{
    LOG_FUNC();
    return NULL;
}

static void
wasm_dutil_notifyPause( XW_DUtilCtxt* XP_UNUSED(duc), XWEnv XP_UNUSED(xwe),
                         XP_U32 XP_UNUSED_DBG(gameID),
                         DupPauseType XP_UNUSED_DBG(pauseTyp),
                         XP_U16 XP_UNUSED_DBG(pauser),
                         const XP_UCHAR* XP_UNUSED_DBG(name),
                         const XP_UCHAR* XP_UNUSED_DBG(msg) )
{
    LOG_FUNC();
}

static void
wasm_dutil_onDupTimerChanged( XW_DUtilCtxt* XP_UNUSED(duc), XWEnv XP_UNUSED(xwe),
                              XP_U32 XP_UNUSED_DBG(gameID),
                              XP_U32 XP_UNUSED_DBG(oldVal),
                              XP_U32 XP_UNUSED_DBG(newVal) )
{
    LOG_FUNC();
}

static void
wasm_dutil_onInviteReceived( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                             const NetLaunchInfo* nli )
{
    LOG_FUNC();
}

static void
wasm_dutil_onMessageReceived( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                               XP_U32 gameID, const CommsAddrRec* from,
                               XWStreamCtxt* stream )
{
    LOG_FUNC();
}

static void
wasm_dutil_onGameGoneReceived( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                               XP_U32 gameID, const CommsAddrRec* from )
{
    LOG_FUNC();
}

XW_DUtilCtxt*
wasm_dutil_make( MPFORMAL VTableMgr* vtMgr, void* closure )
{
    XW_DUtilCtxt* result = XP_CALLOC( mpool, sizeof(*result) );

    dutil_super_init( MPPARM(mpool) result );

    result->vtMgr = vtMgr;
    result->closure = closure;

# define SET_PROC(nam) \
    result->vtable.m_dutil_ ## nam = wasm_dutil_ ## nam;

    SET_PROC(getCurSeconds);
    SET_PROC(getUserString);
    SET_PROC(getUserQuantityString);
    SET_PROC(storeStream);
    SET_PROC(loadStream);
    SET_PROC(storePtr);
    SET_PROC(loadPtr);

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

# undef SET_PROC

    assertTableFull( &result->vtable, sizeof(result->vtable), "wasmutil" );

    return result;
}

void
wasm_dutil_destroy( XW_DUtilCtxt* dutil )
{
}
