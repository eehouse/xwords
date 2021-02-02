#include <time.h>

#include "wasmdutil.h"
#include "dbgutil.h"

static XP_U32
wasm_dutil_getCurSeconds( XW_DUtilCtxt* XP_UNUSED(duc), XWEnv XP_UNUSED(xwe) )
{
    LOG_FUNC();
    return (XP_U32)time(NULL);//tv.tv_sec;
}

static const XP_UCHAR*
wasm_dutil_getUserString( XW_DUtilCtxt* duc, XWEnv xwe, XP_U16 code )
{
    LOG_FUNC();
    return "a string";
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
