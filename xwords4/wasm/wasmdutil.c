/* -*- compile-command: "cd ../wasm && make MEMDEBUG=TRUE install -j3"; -*- */
/*
 * Copyright 2021 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <time.h>

#include <emscripten.h>

#include "wasmdutil.h"
#include "main.h"
#include "dbgutil.h"
#include "LocalizedStrIncludes.h"
#include "wasmasm.h"

typedef struct _WasmDUtilCtxt {
    XW_DUtilCtxt super;
} WasmDUtilCtxt;

EM_JS(void, _get_stored_value, (const char* key,
                                StringProc proc, void* closure), {
          var result = null;
          var jsKey = UTF8ToString(key);
          // console.log('_get_stored_value(key:' + jsKey + ')');
          var val = localStorage.getItem(jsKey);
          ccallString(proc, closure, val);
      });

EM_JS(void, set_stored_value, (const char* key, const char* val), {
        var jsKey = UTF8ToString(key);
        var jsVal = UTF8ToString(val);
        // console.log('set_stored_value(key:' + jsKey + ', val:' + jsVal + ')');
        var jsString = localStorage.setItem(jsKey, jsVal);
    });

EM_JS(void, remove_stored_value, (const char* key), {
        var jsKey = UTF8ToString(key);
        var jsString = localStorage.removeItem(jsKey);
    });

EM_JS(bool, have_stored_value, (const char* key), {
        let jsKey = UTF8ToString(key);
        let jsVal = localStorage.getItem(jsKey);
        let result = null !== jsVal;
        return result;
    });

EM_JS(void, call_for_each_key, (StringProc proc, void* closure), {
        for (let ii = 0; ii < localStorage.length; ++ii ) {
            let key = localStorage.key(ii);
            Module.ccall('cbckString', null, ['number', 'number', 'string'],
                         [proc, closure, key]);
        }
    });

#define SEP_STR "\n"
#define PREFIX "v.2" SEP_STR
#define MAKE_PREFIX(BUF, KEY) \
    char BUF[128];            \
    sprintf( BUF, "%s%s", PREFIX, KEY )

#define MAKE_INDEX(BUF, KEY, IDX)                   \
    char BUF[128];                                  \
    size_t _len = snprintf( BUF, sizeof(BUF), "%s" SEP_STR "%s", KEY, IDX);    \
    XP_ASSERT( _len < sizeof(BUF) )

typedef struct _ValState {
    void* ptr;
    size_t* lenp;
    bool success;
} ValState;

static void
onGotVal( void* closure, const char* val )
{
    ValState* vs = (ValState*)closure;
    if ( !!val ) {
        size_t slen = 1 + strlen(val);
        if ( !!vs->ptr && slen <= *vs->lenp ) {
            memcpy( vs->ptr, val, slen );
            vs->success = true;
        }
        *vs->lenp = slen;
    } else {
        *vs->lenp = 0;
    }
}

static bool
get_stored_value( const char* key, void* out, size_t* lenp )
{
    ValState state = { .ptr = out, .lenp = lenp, .success = false, };
    _get_stored_value( key, onGotVal, &state );
    return state.success;
}

static XP_U32
wasm_dutil_getCurSeconds( XW_DUtilCtxt* XP_UNUSED(duc), XWEnv XP_UNUSED(xwe) )
{
    return (XP_U32)time(NULL);
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
    return wasm_dutil_getUserString( duc, xwe, code );
}

static void
base16Encode( const uint8_t* data, int dataLen, char* out, int outLen )
{
    int used = 0;
    for ( int ii = 0; ii < dataLen; ++ii ) {
        uint8_t byt = data[ii];
        out[used++] = 'A' + ((byt >> 4) & 0x0F);
        out[used++] = 'A' + (byt & 0x0F);
    }
    out[used] = '\0';
}

static void
base16Decode( uint8_t* decodeBuf, int len, const char* str )
{
    int offset = 0;
    for ( ; ; ) {
        char chr = *str++;
        if ( chr < 'A' || chr > ('A' + 16) ) {
            break;
        }
        uint8_t byt = (chr - 'A') << 4;
        chr = *str++;
        if ( chr < 'A' || chr > ('A' + 16) ) {
            break;
        }
        byt |= chr - 'A';
        decodeBuf[offset++] = byt;
    }
    // XP_LOGFF( "offset: %d; len: %d", offset, len );
    XP_ASSERT( offset == len );
}

static void
wasm_dutil_loadPtr( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key,
                    const XP_UCHAR* keySuffix, void* data, XP_U32* lenp )
{
    // XP_LOGFF( "(key: %s)", key );
    MAKE_PREFIX(fullKey, key);

    size_t len;
    get_stored_value(fullKey, NULL, &len);

    char val[len];
    if ( get_stored_value( fullKey, val, &len ) ) {
        // XP_LOGFF( "get_stored_value(%s) => %s", fullKey, val );
        len = XP_STRLEN(val);
        XP_ASSERT( (len % 2) == 0 );
        XP_U8 decodeBuf[len/2];
        len = VSIZE(decodeBuf);
        if ( len <= *lenp ) {
            base16Decode( decodeBuf, len, val );
            XP_MEMCPY( data, decodeBuf, len );
        }
        *lenp = len;
    } else {
        *lenp = 0;              /* signal failure */
    }
    // XP_LOGFF("(%s)=> len: %d", fullKey, *lenp );
}

static void
wasm_dutil_storePtr( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key,
                      const void* data, XP_U32 len )
{
    XP_UCHAR out[len*2+1];
    base16Encode( data, len, out, sizeof(out) );
    MAKE_PREFIX(fullKey, key);
    set_stored_value( fullKey, out );
}

static void
wasm_dutil_storeIndxStream( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key,
                            const XP_UCHAR* indx, XWStreamCtxt* data )
{
    MAKE_INDEX(ikey, key, indx);
    dutil_storeStream( duc, xwe, ikey, data );
}

static void
wasm_dutil_loadIndxStream(XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key,
                          const XP_UCHAR* fallbackKey,
                          const char* indx, XWStreamCtxt* inOut)
{
    MAKE_INDEX(ikey, key, indx);
    dutil_loadStream( duc, xwe, ikey, fallbackKey, inOut );
}

static void
wasm_dutil_storeIndxPtr(XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key,
                        const XP_UCHAR* indx, const void* data, XP_U32 len )
{
    // LOG_FUNC();
    MAKE_INDEX(ikey, key, indx);
    wasm_dutil_storePtr(duc, xwe, ikey, data, len );
}

static void
wasm_dutil_loadIndxPtr( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key,
                        const XP_UCHAR* indx, void* data, XP_U32* lenp )
{
    // LOG_FUNC();
    MAKE_INDEX(ikey, key, indx);

    wasm_dutil_loadPtr( duc, xwe, ikey, NULL, data, lenp );
}

static bool
splitFullKey( char key[], char indx[], const char* fullKey )
{
    bool matches = false;
    if ( 0 == strncmp( fullKey, PREFIX, strlen(PREFIX) ) ) {
        fullKey += strlen(PREFIX);
        char* breakLoc = strstr( fullKey, SEP_STR );
        if ( !!breakLoc ) {
            indx[0] = key[0] = '\0';
            XP_ASSERT( '\n' == *breakLoc );
            strncat( key, fullKey, breakLoc - fullKey );
            strcat( indx, 1 + breakLoc );
            matches = true;
        }
    }
    return matches;
}

typedef struct _ForEachStateKey {
    XW_DUtilCtxt* duc;
    XWEnv xwe;
    OnOneProc onOneProc;
    void* onOneClosure;
    const char* key;
} ForEachStateKey;

/* I'm called with a full key, PREFIX + KEY + INDEX. I'm interested in it IFF
   the KEY part matches, and in that case pass INDEX plus value to the
   callback. */
static void
withOneKey( void* closure, const char* fullKey )
{
    ForEachStateKey* fes = (ForEachStateKey*)closure;

    char key[128];
    char indx[128];
    if ( splitFullKey( key, indx, fullKey ) ) {
        if ( 0 == strcmp(key, fes->key) ) {

            XP_U32 len = 0;
            wasm_dutil_loadIndxPtr( fes->duc, fes->xwe, key, indx, NULL, &len );
            uint8_t val[len];
            wasm_dutil_loadIndxPtr( fes->duc, fes->xwe, key, indx, val, &len );
            (*fes->onOneProc)(fes->onOneClosure, indx, val, len);
        }
    }
}

static void
wasm_dutil_forEachIndx( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key,
                        OnOneProc proc, void* closure )
{
    ForEachStateKey fes = { .duc = duc,
                         .xwe = xwe,
                         .onOneProc = proc,
                         .onOneClosure = closure,
                         .key = key,
    };
    call_for_each_key( withOneKey, &fes );
}

typedef struct _ForEachStateIndx {
    XW_DUtilCtxt* duc;
    const XP_UCHAR* indx;
    char** keys;
    int nKeys;
} ForEachStateIndx;

static void
deleteWithIndx( void* closure, const char* fullKey )
{
    char key[128];
    char indx[128];
    if ( splitFullKey( key, indx, fullKey ) ) {
        ForEachStateIndx* fesi = (ForEachStateIndx*)closure;
        if ( 0 == strcmp( indx, fesi->indx ) ) {
            int cur = fesi->nKeys++;
            // XP_LOGFF( "adding key[%d]: %s", cur, fullKey );
            fesi->keys = XP_REALLOC( fesi->duc->mpool, fesi->keys,
                                     (fesi->nKeys) * sizeof(fesi->keys[0]) );
            fesi->keys[cur] = XP_MALLOC(fesi->duc->mpool, 1 + strlen(fullKey));
            strcpy( fesi->keys[cur], fullKey );
        }
    }
}

static void
wasm_dutil_removeAllIndx( XW_DUtilCtxt* duc, const XP_UCHAR* indx )
{
    ForEachStateIndx fesi = { .duc = duc,
                             .indx = indx,
    };

    call_for_each_key( deleteWithIndx, &fesi );
    for ( int ii = 0; ii < fesi.nKeys; ++ii ) {
        // XP_LOGFF( "removing key %s", fesi.keys[ii] );
        remove_stored_value( fesi.keys[ii] );
        XP_FREE( duc->mpool, fesi.keys[ii] );
    }
    XP_FREEP( duc->mpool, &fesi.keys );
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
    Globals* globals = (Globals*)duc->closure;
    main_gameFromInvite( globals, nli );
}

static void
wasm_dutil_onMessageReceived( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                              XP_U32 gameID, const CommsAddrRec* from,
                              XWStreamCtxt* stream )
{
    Globals* globals = (Globals*)duc->closure;
    main_onGameMessage( globals, gameID, from, stream );
}

static void
wasm_dutil_onGameGoneReceived( XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe),
                               XP_U32 gameID, const CommsAddrRec* from )
{
    Globals* globals = (Globals*)duc->closure;
    main_onGameGone( globals, gameID );
}

XW_DUtilCtxt*
wasm_dutil_make( MPFORMAL VTableMgr* vtMgr, void* closure )
{
    WasmDUtilCtxt* result = XP_CALLOC( mpool, sizeof(*result) );

    dutil_super_init( MPPARM(mpool) &result->super );

    result->super.vtMgr = vtMgr;
    result->super.closure = closure;

# define SET_PROC(nam) \
    result->super.vtable.m_dutil_ ## nam = wasm_dutil_ ## nam;

    SET_PROC(getCurSeconds);
    SET_PROC(getUserString);
    SET_PROC(getUserQuantityString);
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

    SET_PROC(storeIndxStream);
    SET_PROC(loadIndxStream);
    SET_PROC(storeIndxPtr);
    SET_PROC(loadIndxPtr);
    SET_PROC(forEachIndx);
    SET_PROC(removeAllIndx);

# undef SET_PROC

    assertTableFull( &result->super.vtable, sizeof(result->super.vtable), "wasmutil" );

    /* clear_stored(); */
    /* testBase16(); */

    LOG_RETURNF( "%p", &result->super );
    return &result->super;
}

void
wasm_dutil_destroy( XW_DUtilCtxt* dutil )
{
    XP_ASSERT(0);
}
