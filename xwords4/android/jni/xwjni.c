/*
 * Copyright © 2009 - 2023 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include <string.h>
#include <sys/time.h>
#include <pthread.h>

#include <jni.h>
#include <android/log.h>

#include "comtypes.h"
#include "game.h"
#include "board.h"
#include "mempool.h"
#include "strutils.h"
#include "dbgutil.h"
#include "dictnry.h"
#include "dictiter.h"
#include "nli.h"
#include "device.h"
#include "knownplyr.h"
#include "xwmutex.h"

#include "utilwrapper.h"
#include "drawwrapper.h"
#include "anddict.h"
#include "andutils.h"
#include "andglobals.h"
#include "jniutlswrapper.h"
#include "paths.h"
#include "stats.h"
#include "gamemgr.h"

#include "cJSON.h"

#ifdef MAP_THREAD_TO_ENV
# define LOG_MAPPING
#endif
// #define LOG_MAPPING_ALL

#ifdef MAP_THREAD_TO_ENV
typedef struct _EnvThreadEntry {
    JNIEnv* env;
    pthread_t owner;
    XP_U16 refcount;
#ifdef LOG_MAPPING
    const char* ownerFunc;
#endif
} EnvThreadEntry;

struct _EnvThreadInfo {
    MutexState mtxThreads;
    int nEntries;
    EnvThreadEntry* entries;
    MPSLOT
};

#endif

/* Globals for the whole game */
typedef struct _JNIGlobalState {
#ifdef MAP_THREAD_TO_ENV
    EnvThreadInfo ti;
    JavaVM* jvm;
#endif
    VTableMgr* vtMgr;
    XW_DUtilCtxt* dutil;
    JNIUtilCtxt* jniutil;
    XP_Bool mpoolInUse;
    const char* mpoolUser;
    MPSLOT
} JNIGlobalState;

#ifdef MAP_THREAD_TO_ENV
static MutexState g_globalStateMutex = { .mutex = PTHREAD_MUTEX_INITIALIZER, };
static JNIGlobalState* g_globalState = NULL;
#endif

#ifdef MEM_DEBUG
static MemPoolCtx*
getMPoolImpl( JNIGlobalState* globalState, const char* user )
{
    if ( globalState->mpoolInUse ) {
        XP_LOGFF( "mpoolInUse ALREADY SET!!!! (by %s)",
                  globalState->mpoolUser );
    }
    globalState->mpoolInUse = XP_TRUE;
    globalState->mpoolUser = user;
    return globalState->mpool;
}

#define GETMPOOL(gs) getMPoolImpl( (gs), __func__ )

#endif


/* #define GAMEPTR_IS_OBJECT */
/* #ifdef GAMEPTR_IS_OBJECT */
/* typedef jobject GamePtrType; */
/* #else */
/* typedef long GamePtrType; */
/* #endif */

#ifdef LOG_MAPPING
# ifdef DEBUG
static int 
countUsed( const EnvThreadInfo* ti )
{
    int count = 0;
    for ( int ii = 0; ii < ti->nEntries; ++ii ) {
        EnvThreadEntry* entry = &ti->entries[ii];
        if ( 0 != entry->owner ) {
#  ifdef LOG_MAPPING_ALL
            XP_LOGFF( "ii=%d; owner: %x", ii, (unsigned int)entry->owner );
#  endif
            ++count;
        }
    }
    return count;
}
# endif
#endif

#ifdef MAP_THREAD_TO_ENV
# define MAP_THREAD( ti, env ) map_thread_prv( (ti), (env), __func__ )

static void
map_thread_prv( EnvThreadInfo* ti, JNIEnv* env, const char* caller )
{
    pthread_t self = pthread_self();

    WITH_MUTEX( &ti->mtxThreads );

    XP_Bool found = false;
    int nEntries = ti->nEntries;
    EnvThreadEntry* firstEmpty = NULL;
    for ( int ii = 0; !found && ii < nEntries; ++ii ) {
        EnvThreadEntry* entry = &ti->entries[ii];
        if ( 0 == entry->owner ) {
            if ( NULL == firstEmpty ) {
                firstEmpty = entry;
            }
        } else if ( self == entry->owner ) {
            found = true;
            if ( env != entry->env ) {
                /* this DOES happen!!! */
                RAW_LOG( "(ti=%p): replacing env %p with env %p for thread %x",
                         ti, entry->env, env, (int)self );
                entry->env = env;
            }
        }
    }

    if ( !found ) {
        if ( !firstEmpty ) {    /* out of slots */
            if ( 0 == nEntries ) { /* first time */
                nEntries = 2;
                XP_ASSERT( !ti->entries );
            } else {
                nEntries *= 2;
            }
            EnvThreadEntry* entries = XP_CALLOC( ti->mpool, nEntries * sizeof(*entries) );
            if ( !!ti->entries ) {
                XP_MEMCPY( entries, ti->entries, ti->nEntries * sizeof(*ti->entries) );
            }
            firstEmpty = &entries[ti->nEntries]; /* first new entry */
            ti->entries = entries;
            ti->nEntries = nEntries;
#ifdef LOG_MAPPING
            RAW_LOG( "num env entries now %d", nEntries );
#endif
        }

        XP_ASSERT( !!firstEmpty );
        firstEmpty->owner = self;
        firstEmpty->env = env;
        XP_ASSERT( 0 == firstEmpty->refcount );
        ++firstEmpty->refcount;
#ifdef LOG_MAPPING
        firstEmpty->ownerFunc = caller;
        RAW_LOG( "entry %zu: mapped env %p to thread %x",
                 firstEmpty - ti->entries, env, (int)self );
        RAW_LOG( "num entries USED now %d", countUsed(ti) );
#endif
    }

    END_WITH_MUTEX();
} /* map_thread_prv */

static void
map_init( MPFORMAL EnvThreadInfo* ti, JNIEnv* env )
{
    MUTEX_INIT( &ti->mtxThreads, XP_TRUE );
    MPASSIGN( ti->mpool, mpool );
    MAP_THREAD( ti, env );
}

# define MAP_REMOVE( ti, env ) map_remove_prv((ti), (env), __func__)
static void
map_remove_prv( EnvThreadInfo* ti, JNIEnv* env, const char* caller )
{
    XP_Bool found = false;

    WITH_MUTEX( &ti->mtxThreads );
    for ( int ii = 0; !found && ii < ti->nEntries; ++ii ) {
        EnvThreadEntry* entry = &ti->entries[ii];
        found = env == entry->env;
        if ( found ) {
            if ( pthread_self() != entry->owner ) {
                XP_LOGFF( "mismatch; called from %s (but mapped by %s()?",
                          caller, entry->ownerFunc );
                /* XP_ASSERT(0);*/   /* <-- Is this causing ANRs? Does NOT
                   show up as an assert sometimes anyway, though it does kill
                   the thread. */
            }
# ifdef LOG_MAPPING
            RAW_LOG( "UNMAPPED env %p to thread %x (called from %s(); mapped by %s)",
                     entry->env, (int)entry->owner, caller, entry->ownerFunc );
            RAW_LOG( "%d entries left", countUsed( ti ) );
            entry->ownerFunc = NULL;
# endif
            if ( 1 != entry->refcount ) {
                XP_LOGFF( "ERROR: refcount %d, not 1", entry->refcount );
            }
            --entry->refcount;
            entry->env = NULL;
            entry->owner = 0;
        }
    }
    END_WITH_MUTEX();

    if ( !found ) {
        RAW_LOG( "ERROR: mapping for env %p not found when called from %s()",
                 env, caller );
        // XP_ASSERT( 0 );         /* firing, but may be fixed */
    }
}

static void
map_destroy( EnvThreadInfo* ti )
{
    MUTEX_DESTROY( &ti->mtxThreads );
}

static JNIEnv*
prvEnvForMe( EnvThreadInfo* ti )
{
    JNIEnv* result = NULL;
    pthread_t self = pthread_self();
    WITH_MUTEX( &ti->mtxThreads );
    for ( int ii = 0; !result && ii < ti->nEntries; ++ii ) {
        EnvThreadEntry* entry = &ti->entries[ii];
        if ( self == entry->owner ) {
            result = entry->env;
        }
    }
    END_WITH_MUTEX();

    if ( !result ) {
        // JNI_VERSION_1_6 works, whether right or not
        JavaVMAttachArgs args = { .version = JNI_VERSION_1_6, };
        (*g_globalState->jvm)->AttachCurrentThread( g_globalState->jvm,
                                                    &result, &args );
        RAW_LOG( "used AttachCurrentThread to get env %p for thread %x",
                 result, self );
    }

    return result;
}

#else
# define MAP_THREAD( ti, env )
# define MAP_REMOVE( ti, env )
# define map_init( ... )
# define map_destroy( ti )
#endif // MAP_THREAD_TO_ENV

#ifdef MAP_THREAD_TO_ENV

void setGlobalState( JNIEnv* env, JNIGlobalState* state )
{
#ifdef MAP_THREAD_TO_ENV
    if ( !!state ) {
        XP_ASSERT( !state->jvm );
        (*env)->GetJavaVM( env, &state->jvm );
    }
#endif
    WITH_MUTEX( &g_globalStateMutex );
    g_globalState = state;
    END_WITH_MUTEX();
}

JNIEnv*
envForMe( EnvThreadInfo* ti, const char* caller )
{
    JNIEnv* result = prvEnvForMe( ti );
#ifdef DEBUG
    if( !result ) {
        pthread_t self = pthread_self();
        XP_LOGFF( "no env for %s (thread %x)", caller, (int)self );
        // XP_ASSERT(0);           /* firing a lot */
    }
#endif
    return result;
}

#else
# define setGlobalState(e, s)
#endif

JNIEnv*
waitEnvFromGlobals()            /* hanging */
{
    JNIEnv* result = NULL;
#ifdef MAP_THREAD_TO_ENV
    WITH_MUTEX( &g_globalStateMutex );
    JNIGlobalState* state = g_globalState;
    if ( !!state ) {
        result = prvEnvForMe( &state->ti );
    }
    END_WITH_MUTEX();
#endif
    return result;
}

static TrayTileSet*
tilesArrayToTileSet( JNIEnv* env, jintArray jtiles, TrayTileSet* tset )
{
    TrayTileSet* result = NULL;
    if ( jtiles != NULL ) {
        XP_ASSERT( !!jtiles );
        jsize nTiles = (*env)->GetArrayLength( env, jtiles );
        int tmp[MAX_TRAY_TILES];
        getIntsFromArray( env, tmp, jtiles, nTiles, XP_FALSE );

        tset->nTiles = nTiles;
        for ( int ii = 0; ii < nTiles; ++ii ) {
            tset->tiles[ii] = tmp[ii];
        }
        result = tset;
    }
    return result;
}

JNIEXPORT jlong JNICALL
Java_org_eehouse_android_xw4_jni_Device_initJNIState
( JNIEnv* env, jclass C, jobject jdutil, jobject jniu, jlong jseed )
{
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = mpool_make( NULL );
    XP_LOGFF( "ptr size: %zu", sizeof(mpool) );
#endif
    int seed = (int)jseed;
    XP_LOGFF( "calling srandom(seed %d)", seed );
    srandom( seed );

    JNIGlobalState* globalState = (JNIGlobalState*)XP_CALLOC( mpool,
                                                              sizeof(*globalState) );
    map_init( MPPARM(mpool) &globalState->ti, env );
    globalState->jniutil = makeJNIUtil( MPPARM(mpool) env, TI_IF(&globalState->ti) jniu );
    globalState->vtMgr = make_vtablemgr( MPPARM_NOCOMMA(mpool) );
    globalState->dutil = makeDUtil( MPPARM(mpool) env, TI_IF(&globalState->ti)
                                    jdutil, globalState->vtMgr,
                                    globalState->jniutil, NULL );
    MPASSIGN( globalState->mpool, mpool );
    setGlobalState( env, globalState );
    // LOG_RETURNF( "%p", globalState );
    return (jlong)globalState;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_Device_cleanupJNIState
( JNIEnv* env, jclass C, jlong jniGlobalPtr )
{
    // XP_LOGFF( "(jniGlobalPtr: %p)", jniGlobalPtr );
    if ( 0 != jniGlobalPtr ) {
        setGlobalState( env, NULL );
        JNIGlobalState* globalState = (JNIGlobalState*)jniGlobalPtr;
#ifdef MEM_DEBUG
        MemPoolCtx* mpool = GETMPOOL( globalState );
#endif
        ASSERT_ENV( &globalState->ti, env );
        vtmgr_destroy( MPPARM(mpool) globalState->vtMgr );
        destroyDUtil( &globalState->dutil, env );
        destroyJNIUtil( env, &globalState->jniutil );
        map_destroy( &globalState->ti );
        XP_FREE( mpool, globalState );
        mpool_destroy( mpool );
    }
}

static const SetInfo gi_ints[] = {
    ARR_MEMBER( CurGameInfo, nPlayers )
    ,ARR_MEMBER( CurGameInfo, gameSeconds )
    ,ARR_MEMBER( CurGameInfo, boardSize )
    ,ARR_MEMBER( CurGameInfo, traySize )
    ,ARR_MEMBER( CurGameInfo, bingoMin )
    ,ARR_MEMBER( CurGameInfo, gameID )
    ,ARR_MEMBER( CurGameInfo, forceChannel )
};

static const SetInfo gi_bools[] = {
    ARR_MEMBER( CurGameInfo, hintsNotAllowed )
    ,ARR_MEMBER( CurGameInfo, timerEnabled )
    ,ARR_MEMBER( CurGameInfo, allowPickTiles )
    ,ARR_MEMBER( CurGameInfo, allowHintRect )
    ,ARR_MEMBER( CurGameInfo, inDuplicateMode )
    ,ARR_MEMBER( CurGameInfo, tradeSub7 )
    ,ARR_MEMBER( CurGameInfo, fromRematch )
};

static const SetInfo pl_ints[] = {
    ARR_MEMBER( LocalPlayer, robotIQ )
};

static void
makeGI( JNIEnv* env, XW_DUtilCtxt* dutil, CurGameInfo* gi, jobject jgi )
{
    XP_UCHAR buf[256];          /* in case needs whole path */

    getInts( env, (void*)gi, jgi, AANDS(gi_ints) );
    getBools( env, (void*)gi, jgi, AANDS(gi_bools) );

    gi->phoniesAction = 
        jenumFieldToInt( env, jgi, "phoniesAction",
                         PKG_PATH("jni/CurGameInfo$XWPhoniesChoice") );
    gi->deviceRole =
        jenumFieldToInt( env, jgi, "deviceRole",
                         PKG_PATH("jni/CurGameInfo$DeviceRole"));

    getString( env, jgi, "gameName", AANDS(gi->gameName) );
    getString( env, jgi, "dictName", AANDS(gi->dictName) );
    getString( env, jgi, "isoCodeStr", AANDS(gi->isoCodeStr) );

    XP_ASSERT( gi->nPlayers <= MAX_NUM_PLAYERS );

    /* Convert CommsConnTypeSet to ConnTypeSetBits */
    XP_ASSERT( 0 == gi->conTypes );
    jobject jConTypes =
        getObjectField( env, jgi, "conTypes",
                        "L" PKG_PATH("jni/CommsAddrRec$CommsConnTypeSet") ";" );
    if ( !!jConTypes ) {
        gi->conTypes = getTypesFromSet( env, jConTypes );
        deleteLocalRef( env, jConTypes );
    }

    jobject jplayers
        = getObjectField( env, jgi, "players", "[L" PKG_PATH("jni/LocalPlayer") ";" );
    XP_ASSERT( !!jplayers );
    for ( int ii = 0; !!jplayers && ii < gi->nPlayers; ++ii ) {
        LocalPlayer* lp = &gi->players[ii];

        jobject jlp = (*env)->GetObjectArrayElement( env, jplayers, ii );
        XP_ASSERT( !!jlp );

        getInts( env, (void*)lp, jlp, AANDS(pl_ints) );

        lp->isLocal = getBool( env, jlp, "isLocal" );

        getString( env, jlp, "name", AANDS(lp->name) );
        getString( env, jlp, "password", AANDS(lp->password) );
        getString( env, jlp, "dictName", AANDS(lp->dictName) );

        deleteLocalRef( env, jlp );
    }
    deleteLocalRef( env, jplayers );
} /* makeGI */

static void
setJGI( JNIEnv* env, jobject jgi, const CurGameInfo* gi )
{
    // set fields

    setInts( env, jgi, (void*)gi, AANDS(gi_ints) );
    setBools( env, jgi, (void*)gi, AANDS(gi_bools) );

    setString( env, jgi, "gameName", gi->gameName );
    setString( env, jgi, "dictName", gi->dictName );
    setString( env, jgi, "isoCodeStr", gi->isoCodeStr );

    intToJenumField( env, jgi, gi->phoniesAction, "phoniesAction",
                     PKG_PATH("jni/CurGameInfo$XWPhoniesChoice") );
    intToJenumField( env, jgi, gi->deviceRole, "deviceRole",
                     PKG_PATH("jni/CurGameInfo$DeviceRole") );

    jobject jtypset = conTypesToJ( env, gi->conTypes );
    setObjectField( env, jgi, "conTypes",
                    "L" PKG_PATH("jni/CommsAddrRec$CommsConnTypeSet") ";",
                    jtypset );

    jobject jplayers = getObjectField( env, jgi, "players",
                                       "[L" PKG_PATH("jni/LocalPlayer") ";" );
    if ( !!jplayers ) {
        for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
            const LocalPlayer* lp = &gi->players[ii];

            jobject jlp = (*env)->GetObjectArrayElement( env, jplayers, ii );
            XP_ASSERT( !!jlp );

            setInts( env, jlp, (void*)lp, AANDS(pl_ints) );
            
            setBool( env, jlp, "isLocal", lp->isLocal );
            setString( env, jlp, "name", lp->name );
            setString( env, jlp, "password", lp->password );
            setString( env, jlp, "dictName", lp->dictName );

            deleteLocalRef( env, jlp );
        }
        deleteLocalRef( env, jplayers );
    } else {
        XP_ASSERT(0);
    }
} /* setJGI */

#ifdef COMMON_LAYOUT
static const SetInfo bd_ints[] = { ARR_MEMBER( BoardDims, left ),
                                   ARR_MEMBER( BoardDims, top ),
                                   ARR_MEMBER( BoardDims, width ),
                                   ARR_MEMBER( BoardDims, height ),
                                   ARR_MEMBER( BoardDims, scoreLeft ),
                                   ARR_MEMBER( BoardDims, scoreHt ),
                                   ARR_MEMBER( BoardDims, scoreWidth ),
                                   ARR_MEMBER( BoardDims, boardWidth ),
                                   ARR_MEMBER( BoardDims, boardHt ),
                                   ARR_MEMBER( BoardDims, trayLeft ),
                                   ARR_MEMBER( BoardDims, trayTop ),
                                   ARR_MEMBER( BoardDims, trayWidth ),
                                   ARR_MEMBER( BoardDims, trayHt ),
                                   ARR_MEMBER( BoardDims, traySize ),
                                   ARR_MEMBER( BoardDims, cellSize ),
                                   ARR_MEMBER( BoardDims, maxCellSize ),
                                   ARR_MEMBER( BoardDims, timerWidth ),
};

static void
dimsJToC( JNIEnv* env, BoardDims* out, jobject jdims )
{
    getInts( env, (void*)out, jdims, AANDS(bd_ints) );
}

static void
dimsCtoJ( JNIEnv* env, jobject jdims, const BoardDims* in )
{
    setInts( env, jdims, (void*)in, AANDS(bd_ints) );
}
#endif

static XWStreamCtxt*
streamFromJStream( MPFORMAL JNIEnv* env, VTableMgr* vtMgr, jbyteArray jstream )
{
    XP_ASSERT( !!jstream );
    int len = (*env)->GetArrayLength( env, jstream );
    XWStreamCtxt* stream = mem_stream_make_sized( MPPARM(mpool) vtMgr,
                                                  len, 0 );
    jbyte* jelems = (*env)->GetByteArrayElements( env, jstream, NULL );
    stream_putBytes( stream, jelems, len );
    (*env)->ReleaseByteArrayElements( env, jstream, jelems, 0 );
    return stream;
} /* streamFromJStream */

/****************************************************
 * These methods are stateless: no gamePtr
 ****************************************************/
#ifdef DEBUG
# define DVC_HEADER(PTR) {                                              \
    JNIGlobalState* globalState = (JNIGlobalState*)(PTR);              \
    XP_ASSERT( !!globalState );                                        \
    XP_ASSERT( !!globalState->dutil );                                 \
    MAP_THREAD( &globalState->ti, env );                               \
    /* LOG_FUNC();  <- NO LOGGING before MAP_THREAD()!! */             \

# define DVC_HEADER_END()                        \
    /*LOG_RETURN_VOID();*/                      \
    }                                           \

#else
# define DVC_HEADER(PTR) {                                  \
    JNIGlobalState* globalState = (JNIGlobalState*)(PTR);   \
    XP_USE(globalState)                                     \

# define DVC_HEADER_END() }
#endif

static XP_Bool
jstrToDevID( JNIEnv* env, jstring jstr, MQTTDevID* outDevID )
{
    const char* str = (*env)->GetStringUTFChars( env, jstr, NULL );
    XP_Bool success =
        strToMQTTCDevID( str, outDevID );
    // XP_ASSERT( success );
    (*env)->ReleaseStringUTFChars( env, jstr, str );
    return success;
}

JNIEXPORT jlong JNICALL
Java_org_eehouse_android_xw4_jni_TmpDict_dict_1make
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jbyteArray jDictBytes,
  jstring jname, jstring jpath )
{
    DictionaryCtxt* dictPtr;
    DVC_HEADER(jniGlobalPtr);
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = GETMPOOL( globalState );
#endif

    /* makeDict calls dict_ref() */
    dictPtr = makeDict( MPPARM(mpool) env, TI_IF(&globalState->ti)
                        globalState->jniutil, jname, jDictBytes, jpath, NULL, false );
    DVC_HEADER_END();
    return (jlong)dictPtr;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_TmpDict_dict_1ref
( JNIEnv* env, jclass C, jlong dictPtr )
{
    if ( 0 != dictPtr ) {
        DictionaryCtxt* dict = (DictionaryCtxt*)dictPtr;
        dict_ref( dict );
    }
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_TmpDict_dict_1unref
( JNIEnv* env, jclass C, jlong dictPtr )
{
    if ( 0 != dictPtr ) {
        DictionaryCtxt* dict = (DictionaryCtxt*)dictPtr;
        dict_unref( dict, env );
    }
}

typedef struct _FTData {
    JNIEnv* env;
    jbyteArray arrays[16];
    int nArrays;
} FTData;

static XP_Bool
onFoundTiles( void* closure, const Tile* tiles, int nTiles )
{
    FTData* ftd = (FTData*)closure;
    ftd->arrays[ftd->nArrays++] = makeByteArray( ftd->env, nTiles,
                                                 (const jbyte*)tiles );
    return ftd->nArrays < VSIZE(ftd->arrays); /* still have room? */
}

JNIEXPORT jobjectArray JNICALL
Java_org_eehouse_android_xw4_jni_TmpDict_dict_1strToTiles
( JNIEnv* env, jclass C, jlong dictPtr, jstring jstr )
{
    jobjectArray result = NULL;
    DictionaryCtxt* dict = (DictionaryCtxt*)dictPtr;
    const char* str = (*env)->GetStringUTFChars( env, jstr, NULL );

    FTData ftd = { .env = env, };
    dict_tilesForString( dict, str, 0, onFoundTiles, &ftd );

    if ( ftd.nArrays > 0 ) {
        result = makeByteArrayArray( env, ftd.nArrays );
        for ( int ii = 0; ii < ftd.nArrays; ++ii ) {
            (*env)->SetObjectArrayElement( env, result, ii, ftd.arrays[ii] );
            deleteLocalRef( env, ftd.arrays[ii] );
        }
    }

    (*env)->ReleaseStringUTFChars( env, jstr, str );
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_TmpDict_dict_1hasDuplicates
( JNIEnv* env, jclass C, jlong dictPtr )
{
    jboolean result;
    DictionaryCtxt* dict = (DictionaryCtxt*)dictPtr;
    result = dict_hasDuplicates( dict );
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_TmpDict_dict_1getTilesInfo
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong dictPtr )
{
    jstring result;
    DVC_HEADER(jniGlobalPtr);
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = GETMPOOL( globalState );
#endif
    DictionaryCtxt* dict = (DictionaryCtxt*)dictPtr;
    XWStreamCtxt* stream = mem_stream_make( MPPARM(mpool) globalState->vtMgr, 0 );
    dict_writeTilesInfo( dict, 15, stream );
    result = streamToJString( env, stream );
    stream_destroy( stream );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_TmpDict_dict_1tilesToStr
( JNIEnv* env, jclass C, jlong dictPtr, jbyteArray jtiles, jstring jdelim )
{
    jstring result = NULL;
    DictionaryCtxt* dict = (DictionaryCtxt*)dictPtr;
    if ( !!jtiles && !!dict ) {
        XP_UCHAR buf[64];
        const XP_UCHAR* delim = NULL;
        if ( !!jdelim ) {
            delim = (*env)->GetStringUTFChars( env, jdelim, NULL );
        }

        XP_U16 nTiles = (*env)->GetArrayLength( env, jtiles );
        jbyte* tiles = (*env)->GetByteArrayElements( env, jtiles, NULL );

        XP_U16 strLen = dict_tilesToString( dict, (Tile*)tiles, nTiles,
                                            buf, VSIZE(buf), delim );
        if ( 0 < strLen ) {
            buf[strLen] = '\0';
            result = (*env)->NewStringUTF( env, buf );
        }

        if ( !!jdelim ) {
            (*env)->ReleaseStringUTFChars( env, jdelim, delim );
        }
        (*env)->ReleaseByteArrayElements( env, jtiles, tiles, 0 );
    } else {
        XP_LOGFF( "null jtiles array" );
    }
    return result;
}

JNIEXPORT jobject JNICALL
Java_org_eehouse_android_xw4_jni_TmpDict_dict_1getInfo
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong dictPtr,
  jboolean check )
{
    jobject jinfo;
    DVC_HEADER(jniGlobalPtr);
    DictionaryCtxt* dict = (DictionaryCtxt*)dictPtr;
    XP_ASSERT( !!dict );
    jinfo = makeObjectEmptyConstr( env, PKG_PATH("jni/DictInfo") );
    setInt( env, jinfo, "wordCount", dict_getWordCount( dict, env ) );
    setString( env, jinfo, "md5Sum", dict_getMd5Sum( dict ) );
    setString( env, jinfo, "isoCodeStr", dict_getISOCode( dict ) );
    setString( env, jinfo, "langName", dict_getLangName( dict ) );
    DVC_HEADER_END();
    return jinfo;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_TmpDict_dict_1getDesc
( JNIEnv* env, jclass C, jlong dictPtr )
{
    jstring result = NULL;
    DictionaryCtxt* dict = (DictionaryCtxt*)dictPtr;
    const XP_UCHAR* disc = dict_getDesc( dict );
    if ( NULL != disc && '\0' != disc[0] ) {
        result = (*env)->NewStringUTF( env, disc );
    }
    return result;
}

/* Dictionary methods: don't use gamePtr */
JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_TmpDict_dict_1tilesAreSame
( JNIEnv* env, jclass C, jlong dictPtr1, jlong dictPtr2 )
{
    jboolean result;
    const DictionaryCtxt* dict1 = (DictionaryCtxt*)dictPtr1;
    XP_ASSERT( !!dict1 );
    const DictionaryCtxt* dict2 = (DictionaryCtxt*)dictPtr2;
    XP_ASSERT( !!dict2 );
    result = dict_tilesAreSame( dict1, dict2 );
    return result;
}

JNIEXPORT jobjectArray JNICALL
Java_org_eehouse_android_xw4_jni_TmpDict_dict_1getChars
( JNIEnv* env, jclass C, jlong dictPtr )
{
    jobject result = NULL;
    result = and_dictionary_getChars( env, (DictionaryCtxt*)dictPtr );
    return result;
}

struct _JNIState {
    XWGame game;
    JNIGlobalState* globalJNI;
    AndGameGlobals globals;
    XP_U16 curSaveCount;
    XP_U16 lastSavedSize;
#ifdef DEBUG
    const char* envSetterFunc;
    XP_U32 guard;
#endif
    MPSLOT
};

#define GAME_GUARD 0x453627
#if 1
# define LOG_FUNC_IF() LOG_FUNC()
# define LOG_RETURN_VOID_IF() LOG_RETURN_VOID()
#else
# define LOG_FUNC_IF()
# define LOG_RETURN_VOID_IF()
#endif

static const SetInfo gsi_ints[] = {
    ARR_MEMBER( GameStateInfo, visTileCount ),
    ARR_MEMBER( GameStateInfo, nPendingMessages ),
    ARR_MEMBER( GameStateInfo, trayVisState ),
};
static const SetInfo gsi_bools[] = {
    ARR_MEMBER( GameStateInfo,canHint ),
    ARR_MEMBER( GameStateInfo, canUndo ),
    ARR_MEMBER( GameStateInfo, canRedo ),
    ARR_MEMBER( GameStateInfo, inTrade ),
    ARR_MEMBER( GameStateInfo, tradeTilesSelected ),
    ARR_MEMBER( GameStateInfo, canChat ),
    ARR_MEMBER( GameStateInfo, canShuffle ),
    ARR_MEMBER( GameStateInfo, curTurnSelected ),
    ARR_MEMBER( GameStateInfo, canHideRack ),
    ARR_MEMBER( GameStateInfo, canTrade ),
    ARR_MEMBER( GameStateInfo, canPause ),
    ARR_MEMBER( GameStateInfo, canUnpause ),
};

#ifdef XWFEATURE_WALKDICT
////////////////////////////////////////////////////////////
// Dict iterator
////////////////////////////////////////////////////////////

typedef struct _DictIterData {
    JNIGlobalState* globalState;
    const DictionaryCtxt* dict;
    DictIter* iter;
    IndexData idata;
    XP_U16 depth;
#ifdef DEBUG
    pthread_t lastUser;
    XP_U32 guard;
#endif
} DictIterData;

static void makeIndex( DictIterData* data );
static void freeIndices( DictIterData* data );
#define GI_GUARD 0x89ab72

JNIEXPORT jlong JNICALL
Java_org_eehouse_android_xw4_jni_TmpDict_di_1init
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jDictPtr,
  jobjectArray jPatsArr, jint minLen, jint maxLen )
{
    jlong closure = 0;
    DVC_HEADER(jniGlobalPtr);

    if ( !!jDictPtr ) {
        DictionaryCtxt* dict = (DictionaryCtxt*)jDictPtr;
        PatDesc patDescs[3];
        XP_MEMSET( patDescs, 0, VSIZE(patDescs) * sizeof(patDescs[0]) );

        int len = 0;
        bool formatOK = true;
        if ( !!jPatsArr ) {
            len = (*env)->GetArrayLength( env, jPatsArr );
            XP_ASSERT( len == 3 );
            for ( int ii = 0; formatOK && ii < len ; ++ii ) {
                jobject jdesc = (*env)->GetObjectArrayElement( env, jPatsArr, ii );
                if ( !!jdesc ) {
                    jbyteArray jtiles = getObjectField( env, jdesc, "tilePat", "[B" );
                    if ( !!jtiles ) {
                        int nTiles = (*env)->GetArrayLength( env, jtiles );
                        if ( 0 < nTiles ) {
                            PatDesc* pd = &patDescs[ii];
                            /* If user adds too many tiles, we'll see it here */
                            if ( nTiles <= VSIZE(pd->tiles) ) {
                                pd->nTiles = nTiles;
                                jbyte* tiles = (*env)->GetByteArrayElements( env, jtiles, NULL );
                                XP_MEMCPY( &pd->tiles[0], tiles, nTiles * sizeof(pd->tiles[0]) );
                                (*env)->ReleaseByteArrayElements( env, jtiles, tiles, 0 );
                                pd->anyOrderOk = getBool( env, jdesc, "anyOrderOk" );
                            } else {
                                formatOK = false;
                            }
                        }
                    }
                    deleteLocalRefs( env, jtiles, jdesc, DELETE_NO_REF );
                }
            }
        }

        DictIter* iter = NULL;
        if ( formatOK ) {
            DIMinMax mm = { .min = minLen, .max = maxLen };
            iter = di_makeIter( dict, &mm, NULL, 0, !!jPatsArr ? patDescs : NULL,
                                VSIZE(patDescs) );
        }

        if ( !!iter ) {
            DictIterData* data = XP_CALLOC( globalState->mpool, sizeof(*data) );
            data->iter = iter;
            data->globalState = globalState;
            data->dict = dict_ref( dict );
            data->depth = 2;
#ifdef DEBUG
            data->guard = GI_GUARD;
#endif
            makeIndex( data );
            (void)di_firstWord( data->iter );

            closure = (jlong)data;
        }
    }
    DVC_HEADER_END();
    return closure;
}

#ifdef DEBUG
# define DI_HEADER(THREAD_CHECK) {                                      \
    DictIterData* data = (DictIterData*)closure;                        \
    XP_ASSERT( NULL == data || data->guard == GI_GUARD );               \
    if ( THREAD_CHECK && !!data ) {                                     \
        if ( 0 == data->lastUser ) {                                    \
            data->lastUser = pthread_self();                            \
        } else {                                                        \
            XP_ASSERT( data->lastUser == pthread_self() );              \
        }                                                               \
    }                                                                   \

#else
# define DI_HEADER(THREAD_CHECK) {                                      \
    DictIterData* data = (DictIterData*)closure;                        \

#endif

#define DI_HEADER_END()                         \
    }

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_TmpDict_di_1destroy
( JNIEnv* env, jclass C, jlong closure )
{
    DI_HEADER(XP_FALSE);
    if ( NULL != data ) {
#ifdef MEM_DEBUG
        MemPoolCtx* mpool = data->globalState->mpool;
#endif

        freeIndices( data );

        MAP_REMOVE( &data->globalState->ti, env );

        di_freeIter( data->iter, env);
        dict_unref( data->dict, env );
        XP_FREE( mpool, data );
    }
    DI_HEADER_END();
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_TmpDict_di_1wordCount
(JNIEnv* env, jclass C, jlong closure )
{
    jint result = 0;
    DI_HEADER(XP_TRUE);
    if ( NULL != data ) {
        result = di_getNWords( data->iter );
    }
    DI_HEADER_END();
    return result;
}

JNIEXPORT jintArray JNICALL
Java_org_eehouse_android_xw4_jni_TmpDict_di_1getMinMax
( JNIEnv* env, jclass C, jlong closure )
{
    jintArray result;
    DI_HEADER(XP_TRUE);
    XP_U16 vals[2];
    di_getMinMax( data->iter, &vals[0], &vals[1] );

    result = makeIntArray( env, VSIZE(vals), vals, sizeof(vals[0]) );

    DI_HEADER_END();
    return result;
}

JNIEXPORT jobjectArray JNICALL
Java_org_eehouse_android_xw4_jni_TmpDict_di_1getPrefixes
( JNIEnv* env, jclass C, jlong closure )
{
    jobjectArray result = NULL;
    DI_HEADER(XP_TRUE);
    if ( NULL != data && NULL != data->idata.prefixes ) {
        result = makeStringArray( env, data->idata.count, NULL );

        XP_U16 depth = data->depth;
        for ( int ii = 0; ii < data->idata.count; ++ii ) {
            XP_UCHAR buf[16];
            (void)dict_tilesToString( data->dict, 
                                      &data->idata.prefixes[depth*ii], 
                                      depth, buf, VSIZE(buf), NULL );
            jstring jstr = (*env)->NewStringUTF( env, buf );
            (*env)->SetObjectArrayElement( env, result, ii, jstr );
            deleteLocalRef( env, jstr );
        }
    }
    DI_HEADER_END();
    return result;
}

JNIEXPORT jintArray JNICALL
Java_org_eehouse_android_xw4_jni_TmpDict_di_1getIndices
( JNIEnv* env, jclass C, jlong closure )
{
    jintArray jindices = NULL;
    DI_HEADER(XP_TRUE);
    if ( NULL != data ) {
        if ( !!data->idata.indices ) { /* filters-block-all case */
            XP_ASSERT( sizeof(jint) == sizeof(data->idata.indices[0]) );
            jindices = makeIntArray( env, data->idata.count,
                                     (jint*)data->idata.indices,
                                     sizeof(data->idata.indices[0]) );
        }
    }
    DI_HEADER_END();
    return jindices;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_TmpDict_di_1nthWord
( JNIEnv* env, jclass C, jlong closure, jint jposn, jstring jdelim )
{
    jstring result = NULL;
    DI_HEADER(XP_TRUE);
    if ( NULL != data
         && di_getNthWord( data->iter, env, jposn, data->depth,
                           &data->idata ) ) {
        XP_UCHAR buf[64];
        const XP_UCHAR* delim = NULL == jdelim ? NULL
            : (*env)->GetStringUTFChars( env, jdelim, NULL );
        di_wordToString( data->iter, buf, VSIZE(buf), delim );
        result = (*env)->NewStringUTF( env, buf );
        if ( !!delim ) {
            (*env)->ReleaseStringUTFChars( env, jdelim, delim );
        }
    } else {
        XP_LOGFF( "failed to get %dth word", jposn );
    }
    DI_HEADER_END();
    return result;
}

static void
freeIndices( DictIterData* data )
{
    if ( !!data ) {
        IndexData* idata = &data->idata;
        if ( !!idata->prefixes ) {
            XP_FREE( data->globalState->mpool, idata->prefixes );
            idata->prefixes = NULL;
        }
        if( !!idata->indices ) {
            XP_FREE( data->globalState->mpool, idata->indices );
            idata->indices = NULL;
        }
    }
}

static void
makeIndex( DictIterData* data )
{
    XP_U16 nFaces = dict_numTileFaces( data->dict );
    XP_U16 ii;
    XP_U16 count;
    for ( count = 1, ii = 0; ii < data->depth; ++ii ) {
        count *= nFaces;
    }

    freeIndices( data );

    IndexData* idata = &data->idata;
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = data->globalState->mpool;
#endif
    idata->prefixes = XP_MALLOC( mpool, count * data->depth
                                 * sizeof(*idata->prefixes) );
    idata->indices = XP_MALLOC( mpool, count * sizeof(*idata->indices) );
    idata->count = count;

    di_makeIndex( data->iter, data->depth, idata );
    if ( 0 < idata->count ) {
        idata->prefixes = XP_REALLOC( mpool, idata->prefixes,
                                      idata->count * data->depth *
                                      sizeof(*idata->prefixes) );
        idata->indices = XP_REALLOC( mpool, idata->indices,
                                     idata->count * sizeof(*idata->indices) );
    } else {
        freeIndices( data );
    }
} /* makeIndex */

JNIEXPORT jlongArray JNICALL
Java_org_eehouse_android_xw4_jni_GameMgr_gmgr_1getPositions
( JNIEnv* env, jclass C, jlong jniGlobalPtr )
{
    jlongArray result = NULL;
    DVC_HEADER(jniGlobalPtr);
    XWArray* positions = gmgr_getPositions( globalState->dutil, env );
    int count = arr_length( positions );
    XP_LOGFF( "positions array len: %d", count );
    result = makeLongArray( env, count, arr_getData(positions) );
    arr_destroy( positions );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameMgr_gmgr_1deleteGame
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr )
{
    DVC_HEADER(jniGlobalPtr);
    gmgr_deleteGame( globalState->dutil, env, (GameRef)jgr );
    DVC_HEADER_END();
}

JNIEXPORT jlong JNICALL
Java_org_eehouse_android_xw4_jni_GameMgr_gmgr_1newFor
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jobject jgi, jobject jinvitee )
{
    jlong result;
    DVC_HEADER(jniGlobalPtr);
    XW_DUtilCtxt* dutil = globalState->dutil;
    CurGameInfo gi = {};
    makeGI( env, dutil, &gi, jgi );

    CommsAddrRec* addrp = NULL;
    CommsAddrRec invitee = {};
    if ( !!jinvitee ) {
        getJAddrRec( env, &invitee, jinvitee );
        addrp = &invitee;
    }

    result = gmgr_newFor( dutil, env, GROUP_DEFAULT, &gi, addrp );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT jlong JNICALL
Java_org_eehouse_android_xw4_jni_GameMgr_gmgr_1addForInvite
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jobject jnli )
{
    jlong result;
    DVC_HEADER(jniGlobalPtr);
    NetLaunchInfo nli;
    loadNLI( env, &nli, jnli );
    result = gmgr_addForInvite( globalState->dutil, env, GROUP_DEFAULT, &nli );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameMgr_gmgr_1clearThumbnails
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jobject jnli )
{
    DVC_HEADER(jniGlobalPtr);
    gmgr_clearThumbnails( globalState->dutil, env );
    DVC_HEADER_END();
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_GameMgr_gmgr_1addGroup
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jstring jname )
{
    jint result;
    DVC_HEADER(jniGlobalPtr);
    const XP_UCHAR* name = (*env)->GetStringUTFChars( env, jname, NULL );
    result = gmgr_addGroup( globalState->dutil, env, name );
    (*env)->ReleaseStringUTFChars( env, jname, name );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameMgr_gmgr_1getGroupsMap
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jobjectArray outRefs, jobjectArray outNames )
{
    DVC_HEADER(jniGlobalPtr);
    XP_U16 nGroups = gmgr_countGroups( globalState->dutil, env );
    jint refs[nGroups];
    XP_UCHAR names[nGroups][64];
    const XP_UCHAR* ptrs[nGroups];
    for ( int ii = 0; ii < nGroups; ++ii ) {
        GroupRef grp = gmgr_getNthGroup( globalState->dutil, env, ii );
        gmgr_getGroupName( globalState->dutil, env, grp, names[ii], VSIZE(names[ii]) );
        ptrs[ii] = names[ii];
        refs[ii] = grp;
    }

    jobjectArray jRefs = makeIntArray( env, nGroups, refs, sizeof(refs[0]) );
    (*env)->SetObjectArrayElement( env, outRefs, 0, jRefs );
    jobjectArray jNames = makeStringArray( env, nGroups, ptrs );
    (*env)->SetObjectArrayElement( env, outNames, 0, jNames );
    deleteLocalRefs( env, jRefs, jNames, DELETE_NO_REF );

    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameMgr_gmgr_1moveGames
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jint jgrp, jlongArray jgrs )
{
    DVC_HEADER(jniGlobalPtr);
    int nGames = (*env)->GetArrayLength( env, jgrs );

    GameRef games[nGames];
    jlong* grs = (*env)->GetLongArrayElements(env, jgrs, 0);
    for ( int ii = 0; ii < nGames; ++ii ) {
        games[ii] = grs[ii];
    }

    (*env)->ReleaseLongArrayElements( env, jgrs, grs, 0 );
    gmgr_moveGames( globalState->dutil, env, jgrp, games, nGames );
    DVC_HEADER_END();
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_GameMgr_gmgr_1getDefaultGroup
( JNIEnv* env, jclass C, jlong jniGlobalPtr )
{
    jint result;
    DVC_HEADER(jniGlobalPtr);
    result = gmgr_getDefaultGroup( globalState->dutil );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameMgr_gmgr_1makeGroupDefault
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jint grp)
{
    DVC_HEADER(jniGlobalPtr);
    gmgr_makeGroupDefault( globalState->dutil, env, grp );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameMgr_gmgr_1raiseGroup
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jint grp)
{
    DVC_HEADER(jniGlobalPtr);
    gmgr_raiseGroup( globalState->dutil, env, grp );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameMgr_gmgr_1lowerGroup
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jint grp)
{
    DVC_HEADER(jniGlobalPtr);
    gmgr_lowerGroup( globalState->dutil, env, grp );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameMgr_gmgr_1setGroupName
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jint grp, jstring jname)
{
    DVC_HEADER(jniGlobalPtr);
    const XP_UCHAR* name = (*env)->GetStringUTFChars( env, jname, NULL );
    gmgr_setGroupName( globalState->dutil, env, grp, name );
    (*env)->ReleaseStringUTFChars( env, jname, name );
    DVC_HEADER_END();
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_GameMgr_gmgr_1getGroupName
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jint grp )
{
    jstring result;
    DVC_HEADER(jniGlobalPtr);
    XP_UCHAR buf[64];
    gmgr_getGroupName( globalState->dutil, env, grp, buf, VSIZE(buf) );
    result = (*env)->NewStringUTF( env, buf );
    DVC_HEADER_END();
    return result;
}

#ifdef XWFEATURE_GAMEREF_CONVERT
JNIEXPORT jlong JNICALL
Java_org_eehouse_android_xw4_jni_GameMgr_gmgr_1figureGR
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jbyteArray jstream )
{
    jlong result = 0;
    DVC_HEADER(jniGlobalPtr);
    XWStreamCtxt* stream =
        streamFromJStream( MPPARM(globalState->dutil->mpool)
                           env, globalState->vtMgr, jstream );
    result = gmgr_figureGR( stream );
    stream_destroy( stream );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_GameMgr_gmgr_1gameExists
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr )
{
    jboolean result;
    DVC_HEADER(jniGlobalPtr);
    result = gmgr_gameExists(globalState->dutil, env, (GameRef)jgr );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_GameMgr_gmgr_1getGroup
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jstring jname )
{
    jint result = 0;
    DVC_HEADER(jniGlobalPtr);
    const XP_UCHAR* name = (*env)->GetStringUTFChars( env, jname, NULL );
    result = gmgr_getGroup( globalState->dutil, env, name );
    (*env)->ReleaseStringUTFChars( env, jname, name );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT jlong JNICALL
Java_org_eehouse_android_xw4_jni_GameMgr_gmgr_1convertGame
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jstring jname, jint jgrp,
 jbyteArray jstream)
{
    jlong result = 0;
    DVC_HEADER(jniGlobalPtr);
    XWStreamCtxt* stream =
        streamFromJStream( MPPARM(globalState->dutil->mpool)
                           env, globalState->vtMgr, jstream );
    const XP_UCHAR* name = (*env)->GetStringUTFChars( env, jname, NULL );
    GroupRef grp = (GroupRef)jgrp;
    GameRef gr = gmgr_convertGame( globalState->dutil, env, grp, name, stream );
    result = gr;
    (*env)->ReleaseStringUTFChars( env, jname, name );
    stream_destroy( stream );
    DVC_HEADER_END();
    return result;
}
#endif

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameMgr_gmgr_1deleteGroup
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jint grp )
{
    DVC_HEADER(jniGlobalPtr);
    gmgr_deleteGroup( globalState->dutil, env, grp );
    DVC_HEADER_END();
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_GameMgr_gmgr_1getGroupGamesCount
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jint grp )
{
    jint result;
    DVC_HEADER(jniGlobalPtr);
    result = gmgr_getGroupGamesCount( globalState->dutil, env, grp );
    DVC_HEADER_END();
    LOG_RETURNF( "%d", result );
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_GameMgr_gmgr_1getGroupCollapsed
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jint grp )
{
    jboolean result;
    DVC_HEADER(jniGlobalPtr);
    result = gmgr_getGroupCollapsed( globalState->dutil, env, grp );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameMgr_gmgr_1setGroupCollapsed
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jint grp, jboolean jcollapsed )
{
    DVC_HEADER(jniGlobalPtr);
    gmgr_setGroupCollapsed( globalState->dutil, env, grp, jcollapsed );
    DVC_HEADER_END();
}

#define DUTIL_GR_ENV globalState->dutil, (GameRef)jgr, env

JNIEXPORT jobject JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1getGI
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr )
{
    jobject result = NULL;
    DVC_HEADER(jniGlobalPtr);
    const CurGameInfo* gi = gr_getGI( DUTIL_GR_ENV );
    if ( !!gi ) {
        result = makeObjectEmptyConstr( env, PKG_PATH("jni/CurGameInfo") );
        setJGI( env, result, gi );
    }
    DVC_HEADER_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1setGI
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr, jobject jgi )
{
    DVC_HEADER(jniGlobalPtr);
    CurGameInfo gi = {};
    makeGI( env, globalState->dutil, &gi, jgi );
    gr_setGI( DUTIL_GR_ENV, &gi );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1setGameName
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr, jstring jname)
{
    DVC_HEADER(jniGlobalPtr);
    const XP_UCHAR* name = (*env)->GetStringUTFChars( env, jname, NULL );
    gr_setGameName( DUTIL_GR_ENV, name );
    (*env)->ReleaseStringUTFChars( env, jname, name );
    DVC_HEADER_END();
}

JNIEXPORT jobject JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1getSummary
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr )
{
    jobject jsum = NULL;
    DVC_HEADER(jniGlobalPtr);

    const GameSummary* gs = gr_getSummary( DUTIL_GR_ENV );
    if ( !!gs ) {
        const CurGameInfo* gi = gr_getGI( DUTIL_GR_ENV );
        if ( !!gi ) {
            jsum = makeJSummary( env, gs, gi );
        }
    }
    DVC_HEADER_END();
    return jsum;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1start
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr)
{
    DVC_HEADER(jniGlobalPtr);
    gr_start( DUTIL_GR_ENV );
    DVC_HEADER_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1getGameIsOver
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr)
{
    jboolean result;
    DVC_HEADER(jniGlobalPtr);
    result = gr_getGameIsOver( DUTIL_GR_ENV );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1endGame
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr)
{
    DVC_HEADER(jniGlobalPtr);
    gr_endGame( DUTIL_GR_ENV );
    DVC_HEADER_END();
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1formatDictCounts
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr)
{
    jstring result;
    DVC_HEADER(jniGlobalPtr);
    XWStreamCtxt* stream = and_tmp_stream( globalState->dutil );
    gr_formatDictCounts( DUTIL_GR_ENV, stream, 3, XP_FALSE );
    result = streamToJString( env, stream );
    stream_destroy( stream );

    DVC_HEADER_END();
    return result;
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1countTilesInPool
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr)
{
    jint result;
    DVC_HEADER(jniGlobalPtr);
    result = gr_countTilesInPool( DUTIL_GR_ENV );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1figureLayout
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr,
  jint left, jint top, jint width, jint height,
  jint scorePct, jint trayPct, jint scoreWidth,jint fontWidth,
  jint fontHt, jboolean squareTiles, jobject jdims )
{
    DVC_HEADER(jniGlobalPtr);
    BoardDims dims = {};
    gr_figureLayout( DUTIL_GR_ENV,
                     left, top, width, height,
                     115, scorePct, trayPct, scoreWidth,
                     fontWidth, fontHt, squareTiles, &dims );
    dimsCtoJ( env, jdims, &dims );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1save
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr)
{
    DVC_HEADER(jniGlobalPtr);
    gr_save( DUTIL_GR_ENV );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1setDraw
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr, jobject jdraw,
 jobject jutil)
{
    DVC_HEADER(jniGlobalPtr);

    GameRef gr = (GameRef)jgr;
    XW_DUtilCtxt* dutil = globalState->dutil;
    DrawCtx* newDraw = !!jdraw
        ? makeDraw( env, jdraw, DT_SCREEN )
        : NULL;
    const CurGameInfo* gi = gr_getGI( DUTIL_GR_ENV );
    XW_UtilCtxt* newUtil = !!jutil
        ? makeUtil( MPPARM(dutil->mpool) env, jutil, gi, dutil, gr )
        : NULL;
    gr_setDraw( DUTIL_GR_ENV, newDraw, newUtil );
    draw_unref( newDraw, env );
    util_unref( newUtil, env );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1invalAll
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr )
{
    XP_LOGFF("entering");
    DVC_HEADER(jniGlobalPtr);
    gr_invalAll( DUTIL_GR_ENV );
    DVC_HEADER_END();
    XP_LOGFF("exiting");
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1applyLayout
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr, jobject jdims)
{
    BoardDims dims = {};
    DVC_HEADER(jniGlobalPtr);
    dimsJToC( env, &dims, jdims );
    gr_applyLayout( DUTIL_GR_ENV, &dims );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1draw
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr)
{
    DVC_HEADER(jniGlobalPtr);
    gr_draw( DUTIL_GR_ENV );
    DVC_HEADER_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1containsPt
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr, jint xx, jint yy)
{
    jboolean result;
    DVC_HEADER(jniGlobalPtr);
    result = gr_containsPt( DUTIL_GR_ENV, xx, yy );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1handlePenDown
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr, jint xx, jint yy)
{
    DVC_HEADER(jniGlobalPtr);
    XP_Bool bb;                 /* drop this for now */
    gr_handlePenDown( DUTIL_GR_ENV, xx, yy, &bb );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1handlePenUp
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr, jint xx, jint yy)
{
    DVC_HEADER(jniGlobalPtr);
    gr_handlePenUp( DUTIL_GR_ENV, xx, yy );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1handlePenMove
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr, jint xx, jint yy)
{
    DVC_HEADER(jniGlobalPtr);
    gr_handlePenMove( DUTIL_GR_ENV, xx, yy );
    DVC_HEADER_END();
}

JNIEXPORT jobject JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1getState
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr )
{
    jobject jgsi;
    DVC_HEADER(jniGlobalPtr);
    GameStateInfo info = {};
    gr_getState( DUTIL_GR_ENV, &info );

    jgsi = makeObjectEmptyConstr( env, PKG_PATH("jni/GameRef$GameStateInfo") );

    setInts( env, jgsi, (void*)&info, AANDS(gsi_ints) );
    setBools( env, jgsi, (void*)&info, AANDS(gsi_bools) );
    DVC_HEADER_END();
    return jgsi;
}

JNIEXPORT jobject JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1getPlayersLastScore
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr, jint player)
{
    jobject result;
    DVC_HEADER(jniGlobalPtr);

    LastMoveInfo lmi = {};
    XP_Bool valid = gr_getPlayersLastScore( DUTIL_GR_ENV,
                                            player, &lmi );

    result = makeObjectEmptyConstr( env, PKG_PATH("jni/LastMoveInfo") );
    setBool( env, result, "isValid", valid );
    if ( valid ) {
        setBool( env, result, "inDuplicateMode", lmi.inDuplicateMode );
        setInt( env, result, "score", lmi.score );
        setInt( env, result, "nTiles", lmi.nTiles );
        setInt( env, result, "moveType", lmi.moveType );
        setStringArray( env, result, "names", lmi.nWinners, lmi.names );
        setString( env, result, "word", lmi.word );
    }
    DVC_HEADER_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1requestHint
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr, jboolean getNext,
 jbooleanArray workRemains)
{
    jboolean result;
    DVC_HEADER(jniGlobalPtr);
    XP_Bool tmpbool;
    result = gr_requestHint( DUTIL_GR_ENV, !getNext, &tmpbool );
    if ( workRemains ) {
        jboolean jbool = tmpbool;
        setBoolArray( env, workRemains, 1, &jbool );
    }
    DVC_HEADER_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1flip
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr)
{
    DVC_HEADER(jniGlobalPtr);
    gr_flip( DUTIL_GR_ENV );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1juggleTray
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr)
{
    DVC_HEADER(jniGlobalPtr);
    gr_juggleTray( DUTIL_GR_ENV );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1toggleTray
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr)
{
    DVC_HEADER(jniGlobalPtr);
    gr_toggleTray( DUTIL_GR_ENV );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1beginTrade
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr)
{
    DVC_HEADER(jniGlobalPtr);
    gr_beginTrade( DUTIL_GR_ENV );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1endTrade
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr)
{
    DVC_HEADER(jniGlobalPtr);
    gr_endTrade( DUTIL_GR_ENV );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1replaceTiles
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr)
{
    DVC_HEADER(jniGlobalPtr);
    gr_replaceTiles( DUTIL_GR_ENV );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1commitTurn
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr,
 jboolean phoniesConfirmed, jint badWordsKey,
 jboolean turnConfirmed, jintArray jNewTiles)
{
    DVC_HEADER(jniGlobalPtr);
    TrayTileSet newTiles = {};
    TrayTileSet* newTilesP = tilesArrayToTileSet( env, jNewTiles, &newTiles );
    PhoniesConf pc = {.confirmed = phoniesConfirmed,
                      .key = badWordsKey,
    };
    gr_commitTurn( DUTIL_GR_ENV, &pc, turnConfirmed,
                   newTilesP );
    DVC_HEADER_END();
}

JNIEXPORT jbyteArray JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1getThumbData
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr)
{
    LOG_FUNC();
    jbyteArray result = NULL;
    DVC_HEADER(jniGlobalPtr);
    XWStreamCtxt* stream = and_tmp_stream( globalState->dutil );
    XP_LOGFF( "calling gr_getThumbData");
    if ( gr_getThumbData( DUTIL_GR_ENV, stream ) ) {
        result = streamToBArray( env, stream );
    }
    stream_destroy( stream );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT jobjectArray JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1getAddrs
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr)
{
    jobjectArray result;
    DVC_HEADER(jniGlobalPtr);
    CommsAddrRec addrs[MAX_NUM_PLAYERS];
    XP_U16 count = VSIZE(addrs);
    gr_getAddrs(DUTIL_GR_ENV, addrs, &count );
    result = makeAddrArray( env, count, addrs );
    DVC_HEADER_END();
    return result;
}

#ifdef DEBUG
JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1getStats
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr)
{
    jstring result;
    DVC_HEADER(jniGlobalPtr);
    XWStreamCtxt* stream = and_tmp_stream( globalState->dutil );
    gr_getStats( DUTIL_GR_ENV, stream );
    result = streamToJString( env, stream );
    stream_destroy( stream );
    DVC_HEADER_END();
    return result;
}
#endif

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1invite
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr,
 jobject jnli, jobject jaddr, jboolean jSendNow )
{
    LOG_FUNC();
    DVC_HEADER(jniGlobalPtr);
    CommsAddrRec destAddr;
    getJAddrRec( env, &destAddr, jaddr );
    NetLaunchInfo nli;
    loadNLI( env, &nli, jnli );

    gr_invite( DUTIL_GR_ENV, &nli, &destAddr, jSendNow );

    DVC_HEADER_END();
    LOG_RETURN_VOID();
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1getLikelyChatter
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr )
{
    jint result;
    DVC_HEADER(jniGlobalPtr);
    result = gr_getLikelyChatter( DUTIL_GR_ENV );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1getChatCount
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr )
{
    jint result;
    DVC_HEADER(jniGlobalPtr);
    result = gr_getChatCount( DUTIL_GR_ENV );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1getNthChat
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr, jint indx,
  jintArray jfrom, jintArray jts, jboolean markShown )
{
    jstring result;
    DVC_HEADER(jniGlobalPtr);
    XP_S16 from;
    XP_U32 timestamp;
    XP_UCHAR buf[256];
    XP_U16 bufLen = VSIZE(buf);
    gr_getNthChat(DUTIL_GR_ENV, indx, buf, &bufLen, &from, &timestamp, markShown );
    result = (*env)->NewStringUTF( env, buf );
    setIntInArray( env, jfrom, 0, from );
    setIntInArray( env, jts, 0, timestamp );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1sendChat
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr, jstring jmsg )
{
    DVC_HEADER(jniGlobalPtr);
    const char* msg = (*env)->GetStringUTFChars( env, jmsg, NULL );
    gr_sendChat( DUTIL_GR_ENV, msg );
    (*env)->ReleaseStringUTFChars( env, jmsg, msg );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1deleteChats
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr )
{
    DVC_HEADER(jniGlobalPtr);
    gr_deleteChats( DUTIL_GR_ENV );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1setBlankValue
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr,
  jint player, jint col, jint row, jint tile )
{
    DVC_HEADER(jniGlobalPtr);
    gr_setBlankValue( DUTIL_GR_ENV, player, col, row, tile );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1tilesPicked
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr,
  jint player, jintArray jNewTiles )
{
    DVC_HEADER(jniGlobalPtr);
    TrayTileSet newTiles = {};
    tilesArrayToTileSet( env, jNewTiles, &newTiles );
    gr_tilesPicked( DUTIL_GR_ENV, player, &newTiles );
    DVC_HEADER_END();
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1getNumTilesInTray
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr, jint player )
{
    jint result;
    DVC_HEADER(jniGlobalPtr);
    result = gr_getNumTilesInTray( DUTIL_GR_ENV, player );
    DVC_HEADER_END()
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1zoom
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr, jint zoomBy )
{
    DVC_HEADER(jniGlobalPtr);
    gr_zoom( DUTIL_GR_ENV, zoomBy, NULL );
    DVC_HEADER_END();
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1countPendingPackets
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr )
{
    jint result;
    DVC_HEADER(jniGlobalPtr);
    XP_Bool quashed;
    result = gr_countPendingPackets( DUTIL_GR_ENV, &quashed );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1formatRemainingTiles
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr )
{
    jstring result;
    DVC_HEADER(jniGlobalPtr);
    XWStreamCtxt* stream = and_tmp_stream( globalState->dutil );
    gr_formatRemainingTiles( DUTIL_GR_ENV, stream );
    result = streamToJString( env, stream );
    stream_destroy( stream );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1writeGameHistory
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr, jboolean gameOver )
{
    jstring result;
    DVC_HEADER(jniGlobalPtr);
    XWStreamCtxt* stream = and_tmp_stream( globalState->dutil );
    gr_writeGameHistory( DUTIL_GR_ENV, stream, gameOver );
    result = streamToJString( env, stream );
    stream_destroy( stream );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1writeFinalScores
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr )
{
    jstring result;
    DVC_HEADER(jniGlobalPtr);
    XWStreamCtxt* stream = and_tmp_stream( globalState->dutil );
    gr_writeFinalScores( DUTIL_GR_ENV, stream );
    result = streamToJString( env, stream );
    stream_destroy( stream );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1canOfferRematch
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr, jbooleanArray results )
{
    LOG_FUNC();
    jboolean result = false;
    DVC_HEADER(jniGlobalPtr);
    XP_Bool bools[2];
    bools[0] = gr_canRematch( DUTIL_GR_ENV, &bools[1] );
    setBoolArray( env, results, VSIZE(bools), (jboolean*)bools );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT jobjectArray JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1missingDicts
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr )
{
    jobjectArray result = NULL;
    DVC_HEADER(jniGlobalPtr);
    const XP_UCHAR* missingNames[4];
    XP_U16 len = VSIZE(missingNames);
    gr_missingDicts( DUTIL_GR_ENV, missingNames, &len );
    XP_LOGFF( "len: %d", len );

    if ( len ) {
        result = makeStringArray( env, len, missingNames );
    }

    DVC_HEADER_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1replaceDicts
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr,
  jstring jOldName, jstring jNewName )
{
    DVC_HEADER(jniGlobalPtr);
    const char* oldName = (*env)->GetStringUTFChars( env, jOldName, NULL );
    const char* newName = (*env)->GetStringUTFChars( env, jNewName, NULL );
    gr_replaceDicts( DUTIL_GR_ENV, oldName, newName );
    (*env)->ReleaseStringUTFChars( env, jOldName, oldName );
    (*env)->ReleaseStringUTFChars( env, jNewName, newName );
    DVC_HEADER_END();
}

JNIEXPORT jintArray JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1figureOrder
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr, jobject jRo )
{
    jintArray result;
    DVC_HEADER(jniGlobalPtr);
    RematchOrder ro = jEnumToInt( env, jRo );
    XP_LOGFF( "(ro=%s)", RO2Str(ro) );

    NewOrder no;
    gr_figureOrder( DUTIL_GR_ENV, ro, &no );

    const CurGameInfo* gi = gr_getGI(DUTIL_GR_ENV);
    result = makeIntArray( env, gi->nPlayers, no.order,
                           sizeof(no.order[0]) );

    DVC_HEADER_END();
    return result;
}

JNIEXPORT jlong JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1makeRematch
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr, jstring jName,
  jobject jRo, jboolean archive, jboolean delete )
{
    jlong result;
    DVC_HEADER(jniGlobalPtr);
    RematchOrder ro = jEnumToInt( env, jRo );
    const char* name = (*env)->GetStringUTFChars( env, jName, NULL );

    result = gr_makeRematch(DUTIL_GR_ENV, name, ro, archive, delete );

    (*env)->ReleaseStringUTFChars( env, jName, name );

    DVC_HEADER_END();
    return result;
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1getNMoves
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr )
{
    jint result;
    DVC_HEADER(jniGlobalPtr);
    result = gr_getNMoves( DUTIL_GR_ENV );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1getGameIsConnected
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr)
{
    jboolean result;
    DVC_HEADER(jniGlobalPtr);
    result = gr_getGameIsConnected( DUTIL_GR_ENV );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT jobject JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1getSelfAddr
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr)
{
    jobject result;
    DVC_HEADER(jniGlobalPtr);
    CommsAddrRec addr = {};
    gr_getSelfAddr( DUTIL_GR_ENV, &addr );
    result = makeJAddr( env, &addr );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1getAddrDisabled
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr,
 jobject jConnTyp, jboolean forSend )
{
    jboolean result;
    DVC_HEADER(jniGlobalPtr);
    CommsConnType connType = jEnumToInt( env, jConnTyp );
    result = gr_getAddrDisabled( DUTIL_GR_ENV, connType, forSend );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1prefsChanged
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr, jobject jcp)
{
    DVC_HEADER(jniGlobalPtr);
    CommonPrefs cp;
    loadCommonPrefs( env, &cp, jcp );
    gr_prefsChanged( DUTIL_GR_ENV, &cp );
    DVC_HEADER_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1isArchived
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr)
{
    jboolean result;
    DVC_HEADER(jniGlobalPtr);
    result = gr_isArchived( DUTIL_GR_ENV );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1getGroup
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr)
{
    jint result;
    DVC_HEADER(jniGlobalPtr);
    result = gr_getGroup( DUTIL_GR_ENV );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1setCollapsed
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr, jboolean collapsed)
{
    DVC_HEADER(jniGlobalPtr);
    gr_setCollapsed( DUTIL_GR_ENV, collapsed );
    DVC_HEADER_END();
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1failedOpenCount
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr)
{
    jint result;
    DVC_HEADER(jniGlobalPtr);
    result = 0;                 /* FIXME */
    DVC_HEADER_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_GameRef_gr_1setOpenCount
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong jgr, jint newval)
{
    DVC_HEADER(jniGlobalPtr);
    XP_LOGFF( "not using value %d", newval);
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_Device_dvc_1parseMQTTPacket
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jstring jtopic, jbyteArray jmsg )
{
    DVC_HEADER(jniGlobalPtr);

    XP_U16 len = (*env)->GetArrayLength( env, jmsg );
    jbyte* buf = (*env)->GetByteArrayElements( env, jmsg, NULL );
    const char* topic = (*env)->GetStringUTFChars( env, jtopic, NULL );

    dvc_parseMQTTPacket( globalState->dutil, env, topic, (XP_U8*)buf, len );

    (*env)->ReleaseStringUTFChars( env, jtopic, topic );
    (*env)->ReleaseByteArrayElements( env, jmsg, buf, 0 );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_Device_dvc_1parseBTPacket
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jstring jname, jstring jaddr,
  jbyteArray jmsg )
{
    LOG_FUNC();
    DVC_HEADER(jniGlobalPtr);

    const char* name = (*env)->GetStringUTFChars( env, jname, NULL );
    const char* addr = (*env)->GetStringUTFChars( env, jaddr, NULL );
    XP_U16 len = (*env)->GetArrayLength( env, jmsg );
    jbyte* buf = (*env)->GetByteArrayElements( env, jmsg, NULL );

    dvc_parseBTPacket( globalState->dutil, env, buf, len, name, addr );

    (*env)->ReleaseStringUTFChars( env, jname, name );
    (*env)->ReleaseStringUTFChars( env, jaddr, addr );
    (*env)->ReleaseByteArrayElements( env, jmsg, buf, 0 );

    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_Device_dvc_1onBLEMtuChanged
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jstring jname, jint mtu )
{
    DVC_HEADER(jniGlobalPtr);
    const char* name = (*env)->GetStringUTFChars( env, jname, NULL );
    dvc_onBLEMtuChangedFor( globalState->dutil, env, name, mtu );
    (*env)->ReleaseStringUTFChars( env, jname, name );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_Device_dvc_1parseSMSPacket
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jstring jphone, jbyteArray jmsg )
{
    LOG_FUNC();
    DVC_HEADER(jniGlobalPtr);
    XP_U16 len = (*env)->GetArrayLength( env, jmsg );
    jbyte* msg = (*env)->GetByteArrayElements( env, jmsg, NULL );

    const char* phone = (*env)->GetStringUTFChars( env, jphone, NULL );
    CommsAddrRec fromAddr = {};
    addr_addType( &fromAddr, COMMS_CONN_SMS );
    XP_SNPRINTF( fromAddr.u.sms.phone, sizeof(fromAddr.u.sms.phone), "%s", phone );
    (*env)->ReleaseStringUTFChars( env, jphone, phone );

    dvc_parseSMSPacket( globalState->dutil, env, &fromAddr, (XP_U8*)msg, len );
    (*env)->ReleaseByteArrayElements( env, jmsg, msg, 0 );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_Device_dvc_1onTimerFired
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jint jkey )
{
    DVC_HEADER(jniGlobalPtr);
    dvc_onTimerFired( globalState->dutil, env, jkey );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_Device_dvc_1onWebSendResult
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jint jResultKey,
  jboolean jSucceeded, jstring jResult )
{
    XP_ASSERT( 0 != jResultKey );
    DVC_HEADER(jniGlobalPtr);

    const char* resultStr = NULL;
    if ( !!jResult ) {
        resultStr = (*env)->GetStringUTFChars( env, jResult, NULL );
    }
    dvc_onWebSendResult( globalState->dutil, env, jResultKey, jSucceeded,
                         resultStr );
    if ( !!jResult ) {
        (*env)->ReleaseStringUTFChars( env, jResult, resultStr );
    }
    DVC_HEADER_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_Knowns_kplr_1havePlayers
( JNIEnv* env, jclass C, jlong jniGlobalPtr )
{
    jboolean result;
    DVC_HEADER(jniGlobalPtr);
    result = kplr_havePlayers( globalState->dutil, env );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT jobjectArray JNICALL
Java_org_eehouse_android_xw4_jni_Knowns_kplr_1getPlayers
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jboolean byDate )
{
    jobjectArray result = NULL;
    DVC_HEADER(jniGlobalPtr);
    XP_U16 nFound = 0;
    kplr_getNames( globalState->dutil, env, byDate, NULL, &nFound );
    if ( 0 < nFound ) {
        const XP_UCHAR* names[nFound];
        kplr_getNames( globalState->dutil, env, byDate, names, &nFound );
        result = makeStringArray( env, nFound, names );
    }
    DVC_HEADER_END();
    return result;
}

JNIEXPORT jobject JNICALL
Java_org_eehouse_android_xw4_jni_Knowns_kplr_1getAddr
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jstring jName, jintArray jLastMod )
{
    jobject jaddr;
    DVC_HEADER(jniGlobalPtr);

    CommsAddrRec addr;
    const char* name = (*env)->GetStringUTFChars( env, jName, NULL );
    XP_U32 lastMod;
    kplr_getAddr( globalState->dutil, env, name, &addr, &lastMod );
    (*env)->ReleaseStringUTFChars( env, jName, name );

    jaddr = makeJAddr( env, &addr );

    if ( !!jLastMod ) {
        setIntInArray( env, jLastMod, 0, lastMod );
    }

    DVC_HEADER_END();
    return jaddr;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_Knowns_kplr_1renamePlayer
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jstring jOldName, jstring jNewName )
{
    jboolean result;
    DVC_HEADER(jniGlobalPtr);
    const char* oldName = (*env)->GetStringUTFChars( env, jOldName, NULL );
    const char* newName = (*env)->GetStringUTFChars( env, jNewName, NULL );
    result = KP_OK == kplr_renamePlayer( globalState->dutil, env, oldName, newName );
    (*env)->ReleaseStringUTFChars( env, jOldName, oldName );
    (*env)->ReleaseStringUTFChars( env, jNewName, newName );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_Knowns_kplr_1deletePlayer
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jstring jName )
{
    DVC_HEADER(jniGlobalPtr);
    const char* name = (*env)->GetStringUTFChars( env, jName, NULL );
    kplr_deletePlayer( globalState->dutil, env, name );
    (*env)->ReleaseStringUTFChars( env, jName, name );
    DVC_HEADER_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_Device_dvc_1setMQTTDevID
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jstring jNewid )
{
    jboolean result = false;
    DVC_HEADER(jniGlobalPtr);
    MQTTDevID devID;
    if ( jstrToDevID( env, jNewid, &devID ) ) {
        dvc_setMQTTDevID( globalState->dutil, env, &devID );
        result = true;
    }
    DVC_HEADER_END();
    return result;
}

typedef struct _CollectState {
    JNIEnv* env;
    jobject list;
} CollectState;

static void
wordCollector( const XP_UCHAR* str, void* closure )
{
    CollectState* cs = (CollectState*)closure;
    addStrToList( cs->env, cs->list, str );
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_Device_dvc_1getLegalPhonyCodes
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jobject list)
{
    DVC_HEADER(jniGlobalPtr);
    CollectState cs = {
        .env = env,
        .list = list,
    };
    dvc_getIsoCodes( globalState->dutil, env, wordCollector, &cs );

    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_Device_dvc_1getLegalPhoniesFor
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jstring jcode, jobject list)
{
    DVC_HEADER(jniGlobalPtr);

    const char* code = (*env)->GetStringUTFChars( env, jcode, NULL );

    CollectState cs = {
        .env = env,
        .list = list,
    };
    dvc_getPhoniesFor( globalState->dutil, env, code, wordCollector, &cs );
    (*env)->ReleaseStringUTFChars( env, jcode, code );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_Device_dvc_1clearLegalPhony
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jstring jcode, jstring jphony)
{
    DVC_HEADER(jniGlobalPtr);
    const char* code = (*env)->GetStringUTFChars( env, jcode, NULL );
    const char* phony = (*env)->GetStringUTFChars( env, jphony, NULL );
    dvc_clearLegalPhony( globalState->dutil, env, code, phony );
    (*env)->ReleaseStringUTFChars( env, jcode, code );
    (*env)->ReleaseStringUTFChars( env, jphony, phony );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_Device_dvc_1onDictAdded
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jstring jName)
{
     DVC_HEADER(jniGlobalPtr);
    const char* name = (*env)->GetStringUTFChars( env, jName, NULL );
    dvc_onDictAdded( globalState->dutil, env, name );
    (*env)->ReleaseStringUTFChars( env, jName, name );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_Device_dvc_1onDictRemoved
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jstring jName)
{
    DVC_HEADER(jniGlobalPtr);
    const char* name = (*env)->GetStringUTFChars( env, jName, NULL );
    dvc_onDictRemoved( globalState->dutil, env, name );
    (*env)->ReleaseStringUTFChars( env, jName, name );
    DVC_HEADER_END();
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_Device_dvc_1lcToLocale
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jint jlc)
{
    jstring result = NULL;
    const XP_UCHAR* locale = lcToLocale( jlc );
    if ( !!locale ) {
        result = (*env)->NewStringUTF( env, locale );
    }
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_Device_dvc_1haveLocaleToLc
( JNIEnv* env, jclass C, jstring jIsoCode, jintArray jOutArray )
{
    XP_ASSERT( !!jIsoCode );
    XP_LangCode lc;
    const char* isoCode = (*env)->GetStringUTFChars( env, jIsoCode, NULL );
    jboolean result = haveLocaleToLc( isoCode, &lc );
    if ( result ) {
        setIntInArray( env, jOutArray, 0, lc );
    }
    (*env)->ReleaseStringUTFChars( env, jIsoCode, isoCode );
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_Stats_sts_1increment
(JNIEnv* env, jclass C, jlong jniGlobalPtr, jobject jstat)
{
    DVC_HEADER(jniGlobalPtr);
    STAT stat = (STAT)jEnumToInt( env, jstat );
    sts_increment( globalState->dutil, env, stat );
    DVC_HEADER_END();
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_Stats_sts_1export
( JNIEnv* env, jclass C, jlong jniGlobalPtr )
{
    jstring result = NULL;
    DVC_HEADER(jniGlobalPtr);
    cJSON* stats = sts_export( globalState->dutil, env );

    char* replyStr = cJSON_PrintUnformatted( stats );
    result = (*env)->NewStringUTF( env, replyStr );
    free( replyStr );
    cJSON_Delete( stats );

    DVC_HEADER_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_Stats_sts_1clearAll
(JNIEnv* env, jclass C, jlong jniGlobalPtr)
{
    DVC_HEADER(jniGlobalPtr);
    sts_clearAll( globalState->dutil, env );
    DVC_HEADER_END();
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_Device_dvc_1getUUID
(JNIEnv* env, jclass C)
{
    return (*env)->NewStringUTF( env, XW_BT_UUID );
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_GameMgr_gmgr_1isGame
(JNIEnv* env, jclass C, jlong jval)
{
    return gmgr_isGame(jval);
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_GameMgr_gmgr_1toGroup
(JNIEnv* env, jclass C, jlong jval)
{
    return gmgr_toGroup(jval);
}

JNIEXPORT jlong JNICALL
Java_org_eehouse_android_xw4_jni_GameMgr_gmgr_1toGame
(JNIEnv* env, jclass C, jlong jval)
{
    return gmgr_toGame(jval);
}
#endif  /* XWFEATURE_BOARDWORDS */
