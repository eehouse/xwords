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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#include <emscripten.h>

#include "strutils.h"

#include "wasmdutil.h"
#include "main.h"
#include "dbgutil.h"
#include "LocalizedStrIncludes.h"
#include "wasmdict.h"

typedef struct _WasmDUtilCtxt {
    XW_DUtilCtxt super;
    int dirtyCount;
} WasmDUtilCtxt;

EM_JS( void, fsSyncOut, (StringProc proc, void* closure), {
        FS.syncfs(false, function (err) {
                if ( err ) {
                    console.log('sync done: ' + err);
                }
                if ( proc ) {
                    let str = !err ? "success" : err.toString();
                    ccall('cbckString', null, ['number', 'number', 'string'],
                          [proc, closure, str]);
                }
            });
    });

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
ensurePath(const char* keys[], char buf[], bool mkDirs)
{
    buf[0] = '\0';
    int offset = 0;
    offset += sprintf( buf+offset, "%s", ROOT_PATH );

    /* Iterate path elements. Last is a file. So if we're not at the end, make
       a directory */
    for ( int ii = 0; ; ++ii ) {
        const char* elem = keys[ii];
        // XP_LOGFF( "elem[%d]: %s", ii, elem );
        if ( !elem ) {
            break;
        }

        if ( mkDirs ) {
            struct stat statbuf;
            if ( 0 == stat(buf, &statbuf) ) {
                // XP_LOGFF( "%s already exists", buf );
            } else {
                /* XP_LOGFF( "calling mkdir(%s) before appending %s", buf, elem ); */
                int err = mkdir(buf, 0777);
                /* XP_LOGFF( "mkdir(%s) => %d", buf, err); */
                if ( err != 0 ) {
                    XP_LOGFF( "error from mkdir: %s", strerror(errno) );
                }
            }
        }
        offset += sprintf( buf+offset, "/%s", elem );
        // XP_LOGFF( "path now %s", buf );
    }
}

static void
wasm_dutil_storeStream( XW_DUtilCtxt* duc, XWEnv xwe, const char* keys[],
                        XWStreamCtxt* stream )
{
    /* char path[128]; */
    /* ensurePath(keys, path, true); */
    /* XP_LOGFF( "(path: %s)", path ); */

    const XP_U8* data = stream_getPtr(stream);
    XP_U32 len = stream_getSize(stream);
    dutil_storePtr( duc, xwe, keys, (void*)data, len );
    /* writeToPath( buf, data, len ); */
    LOG_RETURN_VOID();
}

static void
wasm_dutil_loadStream( XW_DUtilCtxt* duc, XWEnv xwe, const char* keys[],
                       XWStreamCtxt* inOut )
{
    XP_U32 len;
    dutil_loadPtr( duc, xwe, keys, NULL, &len );
    if ( 0 < len  ) {
        uint8_t* ptr = XP_MALLOC( duc->mpool, len );
        dutil_loadPtr( duc, xwe, keys, ptr, &len );
        stream_putBytes( inOut, ptr, len );
        XP_FREE( duc->mpool, ptr );
    }
    LOG_RETURN_VOID();
}

static void
wasm_dutil_storePtr( XW_DUtilCtxt* duc, XWEnv xwe,
                     const char* keys[],
                     const void* data, XP_U32 len )
{
    char path[128];
    ensurePath(keys, path, true);

    // XP_LOGFF( "opening %s", path );
    int fd = open(path, O_RDWR | O_CREAT, 0666);
    if ( -1 == fd ) {
        XP_LOGFF( "error from open(%s): %s", path, strerror(errno));
    }
    XP_ASSERT( fd != -1 );      /* firing */
    ssize_t nWritten = write(fd, data, len);
    // XP_LOGFF( "wrote %d bytes to path %s", nWritten, path );
    XP_ASSERT( nWritten == len );

    ++((WasmDUtilCtxt*)duc)->dirtyCount;
}

static void
wasm_dutil_loadPtr( XW_DUtilCtxt* duc, XWEnv xwe,
                    const char* keys[],
                    void* data, XP_U32* lenp )
{
    char path[128];
    ensurePath(keys, path, false);
    // XP_LOGFF( "(path: %s)", path );

    struct stat statbuf;
    int err = stat(path, &statbuf);
    if ( 0 == err ) {
        if ( NULL != data && statbuf.st_size <= *lenp ) {
            int fd = open(path, O_RDONLY);
            ssize_t nRead = read(fd, data, statbuf.st_size);
            // XP_LOGFF( "read %d bytes from file %s", nRead, path );
            close( fd );
        }
        *lenp = statbuf.st_size;
    } else {
        XP_LOGFF( "no file at %s", path );
        *lenp = 0;              /* does not exist */
    }
    // XP_LOGFF( "(path: %s)=> (len: %d)", path, *lenp );
}

/* Iterate over every child of the provided path. This isn't a recursive
 * operation since it's only at one level - UNLESS there are wildcards.
 *
 * We only call the callback when we've reached the end!
 */
static bool
callWithKeys( XW_DUtilCtxt* duc, char path[], int depth,
              const char* keysIn[], const char* keysOut[],
              OnOneProc proc, void* closure )
{
    const char* elem = keysIn[depth];
    // XP_LOGFF( "(path:%s; depth: %d; elem: %s)", path, depth, elem );

    bool goOn = true;
    struct stat statbuf;
    int err = stat(path, &statbuf);
    if ( 0 == err ) {
        char* curBase = path + strlen(path);
        if ( NULL == elem ) {
            goOn = (*proc)(closure, keysOut);
        } else if ( 0 == strcmp( KEY_WILDCARD, elem ) ) {
            DIR* dir = opendir(path);
            if ( !!dir ) {
                struct dirent* dent; // for the directory entries
                while (goOn && (dent = readdir(dir)) != NULL) {
                    if ( '.' != dent->d_name[0] ) {
                        keysOut[depth] = dent->d_name;
                        sprintf( curBase, "/%s", dent->d_name );
                        goOn = callWithKeys( duc, path, depth + 1, keysIn,
                                             keysOut, proc, closure );
                    }
                }
                closedir(dir);
            } else {
                XP_LOGFF( "error from opendir(%s): %s", path, strerror(errno));
            }
        } else {
            keysOut[depth] = elem;
            sprintf( curBase, "/%s", elem );
            goOn = callWithKeys( duc, path, depth + 1, keysIn, keysOut, proc, closure );
        }
        keysOut[depth] = NULL;
    }
    return goOn;
}

static void
wasm_dutil_forEach( XW_DUtilCtxt* duc, XWEnv xwe,
                    const char* keysIn[],
                    OnOneProc proc, void* closure )
{
    char path[256];
    sprintf( path, "%s", ROOT_PATH );
    const char* keysOut[10] = {0};
    callWithKeys( duc, path, 0, keysIn, keysOut, proc, closure );
}

static void
deleteAll( char path[] )
{
    DIR* dir = opendir(path);
    if ( !!dir ) {
        char* base = path + strlen(path);
        struct dirent* dent; // for the directory entries
        while ((dent = readdir(dir)) != NULL) {
            if ( '.' != dent->d_name[0] ) {
                sprintf( base, "/%s", dent->d_name );
                deleteAll( path );
            }
        }
        closedir(dir);
        *base = '\0';
        rmdir(path);
        XP_LOGFF( "removed directory %s", path );
    } else {
        unlink( path );
        XP_LOGFF( "removed file %s", path );
    }
}

static void
wasm_dutil_remove( XW_DUtilCtxt* duc, const XP_UCHAR* keys[] )
{
    char path[256];
    ensurePath(keys, path, false);
    XP_LOGFF( "(path: %s)", path );

    deleteAll(path);
    ++((WasmDUtilCtxt*)duc)->dirtyCount;
}

#ifdef XWFEATURE_DEVID
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
#endif

static XP_UCHAR*
wasm_dutil_md5sum( XW_DUtilCtxt* duc, XWEnv xwe, const XP_U8* ptr,
                   XP_U16 len )
{
    LOG_FUNC();
    return NULL;
}

typedef struct _ForLangState {
    XW_DUtilCtxt* duc;
    XWEnv xwe;
    uint8_t* ptr;
    XP_U32 len;
} ForLangState;

static XP_Bool
gotForLang( void* closure, const XP_UCHAR* keys[] )
{
    XP_LOGFF("name: %s", keys[2]);
    ForLangState* fls = (ForLangState*)closure;
    dutil_loadPtr( fls->duc, fls->xwe, keys, NULL, &fls->len );
    if ( 0 < fls->len ) {
        fls->ptr = XP_MALLOC( fls->duc->mpool, fls->len );
        dutil_loadPtr( fls->duc, fls->xwe, keys, fls->ptr, &fls->len );
    } else {
        XP_LOGFF( "nothing for %s/%s", keys[1], keys[2] );
    }
    return NULL != fls->ptr;
}

static const DictionaryCtxt*
wasm_dutil_getDict( XW_DUtilCtxt* duc, XWEnv xwe,
                    XP_LangCode lang, const XP_UCHAR* dictName )
{
    XP_LOGFF( "(dictName: %s)", dictName );

    const char* lc = lcToLocale( lang );

    CAST_GLOB( Globals*, globals, duc->closure );
    const DictionaryCtxt* result = dmgr_get( globals->dictMgr, xwe, dictName );
    if ( !result ) {
        XP_U32 len = 0;
        const char* keys[] = {KEY_DICTS, lc, dictName, NULL };
        dutil_loadPtr( duc, xwe, keys, NULL, &len );
        if ( 0 < len ) {
            uint8_t* ptr = XP_MALLOC( duc->mpool, len );
            dutil_loadPtr( duc, xwe, keys, ptr, &len );
            result = wasm_dictionary_make( globals, xwe, dictName, ptr, len );
            dmgr_put( globals->dictMgr, xwe, dictName, result );
        } else {
            /* Try another dict in same language */
            XP_LOGFF( "trying for another %s dict", lc );
            ForLangState fls = { .duc = duc,
                                 .xwe = xwe,
            };
            const char* langKeys[] = {KEY_DICTS, lc, KEY_WILDCARD, NULL};
            dutil_forEach( duc, xwe, langKeys, gotForLang, &fls );
            if ( !!fls.ptr ) {
                result = wasm_dictionary_make( globals, xwe, dictName, fls.ptr, fls.len );
                dmgr_put( globals->dictMgr, xwe, dictName, result );
            }
        }
    }

    XP_LOGFF("(%s, %s)=>%p", lc, dictName, result );
    return result;
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

static void
onSynced(void* closure, const char* str)
{
    XW_DUtilCtxt* duc = (XW_DUtilCtxt*)closure;
    XP_LOGFF( "(str: %s)", str );
    if ( 0 == strcmp( "success", str ) ) {
        WasmDUtilCtxt* wduc = (WasmDUtilCtxt*)closure;
        wduc->dirtyCount = 0;
    }
}

void
wasm_dutil_syncIf( XW_DUtilCtxt* duc )
{
    WasmDUtilCtxt* wduc = (WasmDUtilCtxt*)duc;
    if ( 0 < wduc->dirtyCount ) {
        wduc->dirtyCount = 0;
        StringProc proc = NULL; // onSynced;
        fsSyncOut(proc, duc);
    }
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

    SET_PROC(getDict);
    SET_PROC(notifyPause);
    SET_PROC(onDupTimerChanged);
    SET_PROC(onInviteReceived);
    SET_PROC(onMessageReceived);
    SET_PROC(onGameGoneReceived);

# undef SET_PROC

    assertTableFull( &result->super.vtable, sizeof(result->super.vtable), "wasmutil" );

    LOG_RETURNF( "%p", &result->super );
    return &result->super;
}

void
wasm_dutil_destroy( XW_DUtilCtxt* dutil )
{
    XP_ASSERT(0);
}
