/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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
#include "dictmgr.h"
#include "nli.h"
#include "smsproto.h"
#include "device.h"
#include "knownplyr.h"
#include "xwmutex.h"

#include "utilwrapper.h"
#include "drawwrapper.h"
#include "xportwrapper.h"
#include "anddict.h"
#include "andutils.h"
#include "andglobals.h"
#include "jniutlswrapper.h"
#include "paths.h"
#include "stats.h"

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
    DictMgrCtxt* dictMgr;
    SMSProto* smsProto;
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

static void
releaseMPool( JNIGlobalState* globalState )
{
    // XP_ASSERT( globalState->mpoolInUse ); /* fired again!!! */
    if ( !globalState->mpoolInUse ) {
        XP_LOGFF( "line %d; ERROR ERROR ERROR mpoolInUse not set", __LINE__ );
    }
    globalState->mpoolInUse = XP_FALSE;
}
#else
# define releaseMPool(s)
#endif


#define GAMEPTR_IS_OBJECT
#ifdef GAMEPTR_IS_OBJECT
typedef jobject GamePtrType;
#else
typedef long GamePtrType;
#endif

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

static void
tilesArrayToTileSet( JNIEnv* env, jintArray jtiles, TrayTileSet* tset )
{
    if ( jtiles != NULL ) {
        XP_ASSERT( !!jtiles );
        jsize nTiles = (*env)->GetArrayLength( env, jtiles );
        int tmp[MAX_TRAY_TILES];
        getIntsFromArray( env, tmp, jtiles, nTiles, XP_FALSE );

        tset->nTiles = nTiles;
        for ( int ii = 0; ii < nTiles; ++ii ) {
            tset->tiles[ii] = tmp[ii];
        }
    }
}

#ifdef GAMEPTR_IS_OBJECT
static JNIState*
getState( JNIEnv* env, GamePtrType gamePtr, const char* func )
{
#ifdef DEBUG
    if ( NULL == gamePtr ) {
        XP_LOGFF( "ERROR: getState() called from %s() with null gamePtr",
                  func );
    }
#endif
    jmethodID mid = getMethodID( env, gamePtr, "ptr", "()J" );
    XP_ASSERT( !!mid );
    return (JNIState*)(*env)->CallLongMethod( env, gamePtr, mid );
}
#else
# define getState( env, gamePtr, func ) ((JNIState*)(gamePtr))
#endif

JNIEXPORT jlong JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_00024Companion_globalsInit
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
    globalState->dictMgr = dmgr_make( MPPARM_NOCOMMA( mpool ) );
    globalState->dutil = makeDUtil( MPPARM(mpool) env, TI_IF(&globalState->ti)
                                    jdutil, globalState->vtMgr, globalState->dictMgr,
                                    globalState->jniutil, NULL );
    globalState->smsProto = smsproto_init( MPPARM( mpool ) env, globalState->dutil );
    MPASSIGN( globalState->mpool, mpool );
    setGlobalState( env, globalState );
    // LOG_RETURNF( "%p", globalState );
    return (jlong)globalState;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_cleanGlobals
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
        smsproto_free( globalState->smsProto );
        vtmgr_destroy( MPPARM(mpool) globalState->vtMgr );
        dmgr_destroy( globalState->dictMgr, env );
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
};

static const SetInfo pl_ints[] = {
    ARR_MEMBER( LocalPlayer, robotIQ )
    ,ARR_MEMBER( LocalPlayer, secondsUsed )
};

static CurGameInfo*
makeGI( MPFORMAL JNIEnv* env, jobject jgi )
{
    CurGameInfo* gi = (CurGameInfo*)XP_CALLOC( mpool, sizeof(*gi) );
    XP_UCHAR buf[256];          /* in case needs whole path */

    getInts( env, (void*)gi, jgi, AANDS(gi_ints) );
    getBools( env, (void*)gi, jgi, AANDS(gi_bools) );

    /* Unlike on other platforms, gi is created without a call to
       game_makeNewGame, which sets gameID.  So check here if it's still unset
       and if necessary set it -- including back in the java world. */
    if ( 0 == gi->gameID ) {
        gi->gameID = game_makeGameID( 0 );
        setInt( env, jgi, "gameID", gi->gameID );
    }

    gi->phoniesAction = 
        jenumFieldToInt( env, jgi, "phoniesAction",
                         PKG_PATH("jni/CurGameInfo$XWPhoniesChoice") );
    gi->serverRole = 
        jenumFieldToInt( env, jgi, "serverRole", 
                         PKG_PATH("jni/CurGameInfo$DeviceRole"));

    getString( env, jgi, "dictName", AANDS(buf) );
    gi->dictName = copyString( mpool, buf );
    getString( env, jgi, "isoCodeStr", AANDS(buf) );
    XP_STRNCPY( gi->isoCodeStr, buf, VSIZE(gi->isoCodeStr) );

    XP_ASSERT( gi->nPlayers <= MAX_NUM_PLAYERS );

    jobject jplayers
        = getObjectField( env, jgi, "players", "[L" PKG_PATH("jni/LocalPlayer") ";" );
    XP_ASSERT( !!jplayers );
    for ( int ii = 0; !!jplayers && ii < gi->nPlayers; ++ii ) {
        LocalPlayer* lp = &gi->players[ii];

        jobject jlp = (*env)->GetObjectArrayElement( env, jplayers, ii );
        XP_ASSERT( !!jlp );

        getInts( env, (void*)lp, jlp, AANDS(pl_ints) );

        lp->isLocal = getBool( env, jlp, "isLocal" );

        getString( env, jlp, "name", AANDS(buf) );
        lp->name = copyString( mpool, buf );
        getString( env, jlp, "password", AANDS(buf) );
        lp->password = copyString( mpool, buf );
        getString( env, jlp, "dictName", AANDS(buf) );
        lp->dictName = copyString( mpool, buf );

        deleteLocalRef( env, jlp );
    }
    deleteLocalRef( env, jplayers );
    return gi;
} /* makeGI */

static void
setJGI( JNIEnv* env, jobject jgi, const CurGameInfo* gi )
{
    // set fields

    setInts( env, jgi, (void*)gi, AANDS(gi_ints) );
    setBools( env, jgi, (void*)gi, AANDS(gi_bools) );

    setString( env, jgi, "dictName", gi->dictName );
    setString( env, jgi, "isoCodeStr", gi->isoCodeStr );

    intToJenumField( env, jgi, gi->phoniesAction, "phoniesAction",
                     PKG_PATH("jni/CurGameInfo$XWPhoniesChoice") );
    intToJenumField( env, jgi, gi->serverRole, "serverRole",
                     PKG_PATH("jni/CurGameInfo$DeviceRole") );

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

static void
destroyGI( MPFORMAL CurGameInfo** gip )
{
    CurGameInfo* gi = *gip;
    if ( !!gi ) {
        gi_disposePlayerInfo( MPPARM(mpool) gi );
        XP_FREE( mpool, gi );
        *gip = NULL;
    }
}

static void
loadCommonPrefs( JNIEnv* env, CommonPrefs* cp, jobject j_cp )
{
    XP_ASSERT( !!j_cp );
    cp->showBoardArrow = getBool( env, j_cp, "showBoardArrow" );
    cp->showRobotScores = getBool( env, j_cp, "showRobotScores" );
    cp->hideTileValues = getBool( env, j_cp, "hideTileValues" );
    cp->skipCommitConfirm = getBool( env, j_cp, "skipCommitConfirm" );
    cp->showColors = getBool( env, j_cp, "showColors" );
    cp->sortNewTiles = getBool( env, j_cp, "sortNewTiles" );
    cp->allowPeek = getBool( env, j_cp, "allowPeek" );
    cp->skipMQTTAdd = getBool( env, j_cp, "skipMQTTAdd" );
#ifdef XWFEATURE_CROSSHAIRS
    cp->hideCrosshairs = getBool( env, j_cp, "hideCrosshairs" );
#endif
    cp->tvType = jenumFieldToInt( env, j_cp, "tvType",
                                  PKG_PATH("jni/CommonPrefs$TileValueType"));
}

static XWStreamCtxt*
streamFromJStream( MPFORMAL JNIEnv* env, VTableMgr* vtMgr, jbyteArray jstream )
{
    XP_ASSERT( !!jstream );
    int len = (*env)->GetArrayLength( env, jstream );
    XWStreamCtxt* stream = mem_stream_make_sized( MPPARM(mpool) vtMgr,
                                                  len, NULL, 0, NULL, NULL );
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

/* This signature's different to not require JvmStatic on the .kt end */
JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_00024Companion_dvc_1getMQTTDevID
( JNIEnv* env, jclass C, jlong jniGlobalPtr )
{
    jstring result;
    DVC_HEADER(jniGlobalPtr);
    MQTTDevID devID;
    dvc_getMQTTDevID( globalState->dutil, env, &devID );

    XP_UCHAR buf[64];
    formatMQTTDevID( &devID, buf, VSIZE(buf) );
    result = (*env)->NewStringUTF( env, buf );

    DVC_HEADER_END();
    return result;
}

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

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dvc_1setMQTTDevID
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

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dvc_1resetMQTTDevID
( JNIEnv* env, jclass C, jlong jniGlobalPtr )
{
    DVC_HEADER(jniGlobalPtr);
    dvc_resetMQTTDevID( globalState->dutil, env );
    DVC_HEADER_END();
}

JNIEXPORT jobjectArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dvc_1getMQTTSubTopics
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jintArray jQOSOut )
{
    jobjectArray result;
    DVC_HEADER(jniGlobalPtr);


    XP_UCHAR storage[256];
    XP_UCHAR* topics[4];
    XP_U16 nTopics = VSIZE(topics);
    XP_U8 qos;
    dvc_getMQTTSubTopics( globalState->dutil, env,
                          storage, VSIZE(storage),
                          &nTopics, topics, &qos );

    result = makeStringArray( env, nTopics, (const XP_UCHAR* const*)topics );

    XP_ASSERT( !!result );
    setIntInArray( env, jQOSOut, 0, qos );

    DVC_HEADER_END();
    return result;
}

JNIEXPORT jobject JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dvc_1makeMQTTNukeInvite
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jobject jnli )
{
    jobject result;
    DVC_HEADER(jniGlobalPtr);

    NetLaunchInfo nli;
    loadNLI( env, &nli, jnli );

    MTPData mtp = { .env = env, };

    dvc_makeMQTTNukeInvite( globalState->dutil, env,
                            msgAndTopicProc, &mtp, &nli );
    result = wrapResults( &mtp );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT jobject JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dvc_1makeMQTTNoSuchGames
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jstring jAddressee,
  jint jgameid )
{
    jobject result;
    DVC_HEADER(jniGlobalPtr);

    MTPData mtp = { .env = env, };
    MQTTDevID addressee;
    jstrToDevID( env, jAddressee, &addressee );

    dvc_makeMQTTNoSuchGames( globalState->dutil, env, msgAndTopicProc, &mtp,
                             &addressee, jgameid );

    result = wrapResults( &mtp );

    DVC_HEADER_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dvc_1parseMQTTPacket
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jstring jtopic, jbyteArray jmsg )
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
Java_org_eehouse_android_xw4_jni_XwJNI_dvc_1onWebSendResult
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
Java_org_eehouse_android_xw4_jni_XwJNI_dvc_1getLegalPhonyCodes
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
Java_org_eehouse_android_xw4_jni_XwJNI_dvc_1getLegalPhoniesFor
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
Java_org_eehouse_android_xw4_jni_XwJNI_dvc_1clearLegalPhony
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

# ifdef XWFEATURE_KNOWNPLAYERS
JNIEXPORT jobjectArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_kplr_1getPlayers
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jboolean byDate )
{
    jobjectArray jnames = NULL;
    DVC_HEADER(jniGlobalPtr);

    XP_U16 nFound = 0;
    kplr_getNames( globalState->dutil, env, byDate, NULL, &nFound );
    if ( 0 < nFound ) {
        const XP_UCHAR* names[nFound];
        kplr_getNames( globalState->dutil, env, byDate, names, &nFound );
        jnames = makeStringArray( env, nFound, names );
    }
    DVC_HEADER_END();
    return jnames;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_kplr_1renamePlayer
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
Java_org_eehouse_android_xw4_jni_XwJNI_kplr_1deletePlayer
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jstring jName )
{
    DVC_HEADER(jniGlobalPtr);
    const char* name = (*env)->GetStringUTFChars( env, jName, NULL );
    kplr_deletePlayer( globalState->dutil, env, name );
    (*env)->ReleaseStringUTFChars( env, jName, name );
    DVC_HEADER_END();
}

JNIEXPORT jobject JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_kplr_1getAddr
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jstring jName, jintArray jLastMod )
{
    jobject jaddr;
    DVC_HEADER(jniGlobalPtr);

    CommsAddrRec addr;
    const char* name = (*env)->GetStringUTFChars( env, jName, NULL );
    XP_U32 lastMod;
    kplr_getAddr( globalState->dutil, env, name, &addr, &lastMod );
    (*env)->ReleaseStringUTFChars( env, jName, name );
    jaddr = makeObjectEmptyConstr( env, PKG_PATH("jni/CommsAddrRec") );
    setJAddrRec( env, jaddr, &addr );

    if ( !!jLastMod ) {
        setIntInArray( env, jLastMod, 0, lastMod );
    }

    DVC_HEADER_END();
    return jaddr;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_kplr_1nameForMqttDev
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jstring jDevID )
{
    jstring result;
    DVC_HEADER(jniGlobalPtr);

    MQTTDevID devID;
    jstrToDevID( env, jDevID, &devID );
    const XP_UCHAR* name = kplr_nameForMqttDev( globalState->dutil,
                                                env, &devID );

    result = (*env)->NewStringUTF( env, name );
    DVC_HEADER_END();
    return result;
}
#endif

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_gi_1from_1stream
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jobject jgi, jbyteArray jstream )
{
    DVC_HEADER(jniGlobalPtr);
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = GETMPOOL( globalState );
#endif
    XWStreamCtxt* stream = streamFromJStream( MPPARM(mpool) env,
                                              globalState->vtMgr, jstream );

    CurGameInfo gi = {};
    if ( game_makeFromStream( MPPARM(mpool) env, stream, NULL,
                              &gi, NULL, NULL, NULL, NULL ) ) {
        setJGI( env, jgi, &gi );
    } else {
        XP_LOGFF( "game_makeFromStream failed" );
    }

    gi_disposePlayerInfo( MPPARM(mpool) &gi );

    stream_destroy( stream );
    releaseMPool( globalState );
    DVC_HEADER_END();
}

JNIEXPORT jbyteArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_nli_1to_1stream
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jobject jnli )
{
    jbyteArray result;
    DVC_HEADER(jniGlobalPtr);
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = GETMPOOL( globalState );
#endif

    NetLaunchInfo nli;
    loadNLI( env, &nli, jnli );
    XWStreamCtxt* stream = mem_stream_make( MPPARM(mpool) globalState->vtMgr,
                                            NULL, 0, NULL, NULL);

    nli_saveToStream( &nli, stream );

    result = streamToBArray( env, stream );
    stream_destroy( stream );
    releaseMPool( globalState );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT jobject JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_nli_1from_1stream
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jbyteArray jstream )
{
    jobject jnli = NULL;
    DVC_HEADER(jniGlobalPtr);
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = GETMPOOL( globalState );
#endif
    XWStreamCtxt* stream = streamFromJStream( MPPARM(mpool) env,
                                              globalState->vtMgr, jstream );

    NetLaunchInfo nli = {};
    if ( nli_makeFromStream( &nli, stream ) ) {
        jnli = makeObjectEmptyConstr( env, PKG_PATH("NetLaunchInfo") );
        setNLI( env, jnli, &nli );
    } else {
        XP_LOGFF( "nli_makeFromStream failed" );
    }

    stream_destroy( stream );
    releaseMPool( globalState );
    DVC_HEADER_END();
    return jnli;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1getUUID
( JNIEnv* env, jclass C )
{
    return (*env)->NewStringUTF( env, XW_BT_UUID );
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_lcToLocale
( JNIEnv* env, jclass C, jint lc )
{
    jstring result = NULL;
    const XP_UCHAR* locale = lcToLocale( lc );
    if ( !!locale ) {
        result = (*env)->NewStringUTF( env, locale );
    }
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_haveLocaleToLc
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

JNIEXPORT jlong JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1make
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
                        globalState->dictMgr,globalState->jniutil,
                        jname, jDictBytes, jpath, NULL, false );
    DVC_HEADER_END();
    return (jlong)dictPtr;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1ref
( JNIEnv* env, jclass C, jlong dictPtr )
{
    if ( 0 != dictPtr ) {
        DictionaryCtxt* dict = (DictionaryCtxt*)dictPtr;
        dict_ref( dict, env );
    }
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1unref
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
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1strToTiles
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
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1hasDuplicates
( JNIEnv* env, jclass C, jlong dictPtr )
{
    jboolean result;
    DictionaryCtxt* dict = (DictionaryCtxt*)dictPtr;
    result = dict_hasDuplicates( dict );
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1getTilesInfo
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong dictPtr )
{
    jstring result;
    DVC_HEADER(jniGlobalPtr);
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = GETMPOOL( globalState );
#endif
    DictionaryCtxt* dict = (DictionaryCtxt*)dictPtr;
    XWStreamCtxt* stream = mem_stream_make( MPPARM(mpool) globalState->vtMgr,
                                            NULL, 0, NULL, NULL );
    dict_writeTilesInfo( dict, 15, stream );
    result = streamToJString( env, stream );
    stream_destroy( stream );
    DVC_HEADER_END();
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1tilesToStr
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
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1getInfo
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
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1getDesc
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

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_sts_1export
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
Java_org_eehouse_android_xw4_jni_XwJNI_sts_1clearAll
( JNIEnv* env, jclass C, jlong jniGlobalPtr )
{
    DVC_HEADER(jniGlobalPtr);
    sts_clearAll( globalState->dutil, env );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_sts_1increment
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jobject jstat )
{
    DVC_HEADER(jniGlobalPtr);
    STAT stat = (STAT)jEnumToInt( env, jstat );
    sts_increment( globalState->dutil, env, stat );
    DVC_HEADER_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dvc_1onTimerFired
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jint jkey )
{
    DVC_HEADER(jniGlobalPtr);
    dvc_onTimerFired( globalState->dutil, env, jkey );
    DVC_HEADER_END();
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dvc_1getQOS
( JNIEnv* env, jclass C, jlong jniGlobalPtr )
{
    jint result;
    DVC_HEADER(jniGlobalPtr);
    result = dvc_getQOS( globalState->dutil, env );
    DVC_HEADER_END();
    return result;
}

/* Dictionary methods: don't use gamePtr */
JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1tilesAreSame
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
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1getChars
( JNIEnv* env, jclass C, jlong dictPtr )
{
    jobject result = NULL;
    result = and_dictionary_getChars( env, (DictionaryCtxt*)dictPtr );
    return result;
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1getTileValue
( JNIEnv* env, jclass C, jlong dictPtr, jint tile )
{
    return dict_getTileValue( (DictionaryCtxt*)dictPtr, tile );
}

static jobjectArray
msgArrayToByteArrays( JNIEnv* env, const SMSMsgArray* arr )
{
    XP_ASSERT( arr->format == FORMAT_NET );

    jobjectArray result = makeByteArrayArray( env, arr->nMsgs );
    for ( int ii = 0; ii < arr->nMsgs; ++ii ) {
        SMSMsgNet* msg = &arr->u.msgsNet[ii];
        jbyteArray arr = makeByteArray( env, msg->len, (const jbyte*)msg->data );
        (*env)->SetObjectArrayElement( env, result, ii, arr );
        deleteLocalRef( env, arr );
    }
    return result;
}

static jobjectArray
msgArrayToJMsgArray( JNIEnv* env, const SMSMsgArray* arr )
{
    XP_ASSERT( arr->format == FORMAT_LOC );
    jclass clas = (*env)->FindClass( env, PKG_PATH("jni/XwJNI$SMSProtoMsg") );
    jobjectArray result = (*env)->NewObjectArray( env, arr->nMsgs, clas, NULL );

    jmethodID initId = (*env)->GetMethodID( env, clas, "<init>", "()V" );
    for ( int ii = 0; ii < arr->nMsgs; ++ii ) {
        jobject jmsg = (*env)->NewObject( env, clas, initId );

        const SMSMsgLoc* msgsLoc = &arr->u.msgsLoc[ii];
        intToJenumField( env, jmsg, msgsLoc->cmd, "cmd", PKG_PATH("jni/XwJNI$SMS_CMD") );
        setInt( env, jmsg, "gameID", msgsLoc->gameID );

        jbyteArray arr = makeByteArray( env, msgsLoc->len,
                                        (const jbyte*)msgsLoc->data );
        setObjectField( env, jmsg, "data", "[B", arr );
        
        (*env)->SetObjectArrayElement( env, result, ii, jmsg );
        deleteLocalRef( env, jmsg );
    }
    deleteLocalRef( env, clas );
    return result;
}

JNIEXPORT jobjectArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_smsproto_1prepOutbound
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jobject jCmd, jint jGameID,
  jbyteArray jData, jstring jToPhone, jint jPort, jintArray jWaitSecsArr )
{
    jobjectArray result = NULL;
    DVC_HEADER(jniGlobalPtr);

    SMS_CMD cmd = jEnumToInt( env, jCmd );
    jbyte* data = NULL;
    int len = 0;
    if ( NULL != jData ) {
        len = (*env)->GetArrayLength( env, jData );
        data = (*env)->GetByteArrayElements( env, jData, NULL );
    }
    const char* toPhone = (*env)->GetStringUTFChars( env, jToPhone, NULL );

    XP_U16 waitSecs;
    SMSMsgArray* arr = smsproto_prepOutbound( globalState->smsProto, env, cmd,
                                              jGameID, (const XP_U8*)data, len,
                                              toPhone, jPort, XP_FALSE,
                                              &waitSecs );
    if ( !!arr ) {
        result = msgArrayToByteArrays( env, arr );
        smsproto_freeMsgArray( globalState->smsProto, arr );
    }

    setIntInArray( env, jWaitSecsArr, 0, waitSecs );

    (*env)->ReleaseStringUTFChars( env, jToPhone, toPhone );
    if ( NULL != jData ) {
        (*env)->ReleaseByteArrayElements( env, jData, data, 0 );
    }
    DVC_HEADER_END();
    return result;
}

JNIEXPORT jobjectArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_smsproto_1prepInbound
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jbyteArray jData,
  jstring jFromPhone, jint jWantPort )
{
    jobjectArray result = NULL;
    DVC_HEADER(jniGlobalPtr);
    if ( !!jData ) {

        int len = (*env)->GetArrayLength( env, jData );
        jbyte* data = (*env)->GetByteArrayElements( env, jData, NULL );
        const char* fromPhone = (*env)->GetStringUTFChars( env, jFromPhone, NULL );

        SMSMsgArray* arr = smsproto_prepInbound( globalState->smsProto, env, fromPhone,
                                                 jWantPort, (XP_U8*)data, len );
        if ( !!arr ) {
            result = msgArrayToJMsgArray( env, arr );
            smsproto_freeMsgArray( globalState->smsProto, arr );
        }

        (*env)->ReleaseStringUTFChars( env, jFromPhone, fromPhone );
        (*env)->ReleaseByteArrayElements( env, jData, data, 0 );
    } else {
        XP_LOGFF( " => null (null input)" );
    }
    DVC_HEADER_END();
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
#if 0
# define LOG_FUNC_IF() LOG_FUNC()
# define LOG_RETURN_VOID_IF() LOG_RETURN_VOID()
#else
# define LOG_FUNC_IF()
# define LOG_RETURN_VOID_IF()
#endif

#define XWJNI_START(GP) {                                   \
    XP_ASSERT( NULL != (GP) );                              \
    JNIState* state = getState( env, (GP), __func__ );      \
    LOG_FUNC_IF();                                          \
    XP_ASSERT( state->guard == GAME_GUARD );                \
    MPSLOT;                                                 \
    MPASSIGN( mpool, state->mpool );                        \
    XP_ASSERT( !!state->globalJNI );                        \
    MAP_THREAD( &state->globalJNI->ti, env );               \

#define XWJNI_START_GLOBALS(GP)                         \
    XWJNI_START(GP);                                    \
    AndGameGlobals* globals = &state->globals;          \
    XP_USE(globals); /*no warnings */                   \

#define XWJNI_END()                                          \
    LOG_RETURN_VOID_IF();                                    \
    }                                                        \

JNIEXPORT jlong JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_gameJNIInit
( JNIEnv* env, jclass C, jlong jniGlobalPtr )
{
    JNIState* state;
    DVC_HEADER(jniGlobalPtr);
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = ((JNIGlobalState*)jniGlobalPtr)->mpool;
#endif
    state = (JNIState*)XP_CALLOC( mpool, sizeof(*state) );
#ifdef DEBUG
    state->guard = GAME_GUARD;
#endif
    state->globalJNI = (JNIGlobalState*)jniGlobalPtr;
    MAP_THREAD( &state->globalJNI->ti, env );
    AndGameGlobals* globals = &state->globals;
    globals->dutil = state->globalJNI->dutil;
    globals->state = (JNIState*)state;
    MPASSIGN( state->mpool, mpool );
    globals->vtMgr = make_vtablemgr(MPPARM_NOCOMMA(mpool));
    DVC_HEADER_END();
    /* LOG_RETURNF( "%p", state ); */
    return (jlong) state;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_envDone
( JNIEnv* env, jclass C, jlong jniGlobalPtr )
{
#ifdef MAP_THREAD_TO_ENV
    JNIGlobalState* globalJNI = (JNIGlobalState*)jniGlobalPtr;
    MAP_REMOVE( &globalJNI->ti, env );
#endif
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1makeNewGame
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject j_gi,
  jobject j_selfAddr, jobject j_hostAddr, jobject j_util, jobject j_draw,
  jobject j_cp, jobject j_procs )
{
    XWJNI_START_GLOBALS(gamePtr);
    CurGameInfo* gi = makeGI( MPPARM(mpool) env, j_gi );
    globals->gi = gi;
    globals->util = makeUtil( MPPARM(mpool) env,
                              TI_IF(&state->globalJNI->ti)
                              j_util, gi, globals );
    globals->jniutil = state->globalJNI->jniutil;
    DrawCtx* dctx = NULL;
    if ( !!j_draw ) {
        dctx = makeDraw( MPPARM(mpool) env,
                         TI_IF(&state->globalJNI->ti) j_draw );
    }
    globals->dctx = dctx;
    if ( !!j_procs ) {
        globals->xportProcs = makeXportProcs( MPPARM(mpool) env, globals->util,
                                              TI_IF(&state->globalJNI->ti)
                                              j_procs );
    }
    CommonPrefs cp = {};
    loadCommonPrefs( env, &cp, j_cp );

    CommsAddrRec selfAddr;
    CommsAddrRec* selfAddrP = NULL;
    if ( !!j_selfAddr ) {
        getJAddrRec( env, &selfAddr, j_selfAddr );
        selfAddrP = &selfAddr;
    }

    CommsAddrRec hostAddr;
    CommsAddrRec* hostAddrP = NULL;
    if ( !!j_hostAddr ) {
        XP_ASSERT( gi->serverRole == SERVER_ISCLIENT );
        getJAddrRec( env, &hostAddr, j_hostAddr );
        hostAddrP = &hostAddr;
    } else {
        XP_ASSERT( gi->serverRole != SERVER_ISCLIENT );
    }

    game_makeNewGame( MPPARM(mpool) env, &state->game, gi, selfAddrP,
                      hostAddrP, globals->util, dctx, &cp, globals->xportProcs );
    XWJNI_END();
} /* makeNewGame */

static void
initGameGlobals( JNIEnv* env, JNIState* state, jobject jutil, jobject jprocs )
{
    AndGameGlobals* globals = &state->globals;
    globals->gi = (CurGameInfo*)XP_CALLOC( state->mpool, sizeof(*globals->gi) );
    if ( !!jutil ) {
        XP_ASSERT( !globals->util );
        globals->util = makeUtil( MPPARM(state->mpool) env,
                                  TI_IF(&state->globalJNI->ti)
                                  jutil, globals->gi, globals );
    }
    if ( !!jprocs ) {
        globals->xportProcs = makeXportProcs( MPPARM(state->mpool) env, globals->util,
                                              TI_IF(&state->globalJNI->ti)
                                              jprocs );
    }
    globals->jniutil = state->globalJNI->jniutil;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1makeRematch
( JNIEnv* env, jclass C, GamePtrType gamePtr, GamePtrType gamePtrNew,
  jobject jutil, jobject jcp, jstring jGameName, jintArray jNO )
{
    jboolean success = false;
    XWJNI_START_GLOBALS(gamePtr);

    JNIState* oldState = state; /* state about to go out-of-scope */
    XWJNI_START_GLOBALS(gamePtrNew);

    initGameGlobals( env, state, jutil, NULL );

    CommonPrefs cp;
    loadCommonPrefs( env, &cp, jcp );

    const char* gameName = (*env)->GetStringUTFChars( env, jGameName, NULL );

    NewOrder no;
    int tmp[VSIZE(no.order)];
    int count = getIntsFromArray( env, tmp, jNO, VSIZE(tmp), XP_FALSE );
    for ( int ii = 0; ii < count; ++ii ) {
        no.order[ii] = tmp[ii];
    }

    success = game_makeRematch( &oldState->game, env, globals->util, &cp,
                                (TransportProcs*)NULL, &state->game,
                                gameName, &no );
    (*env)->ReleaseStringUTFChars( env, jGameName, gameName );

    if ( success ) {
        /* increase the ref count */
        MAP_THREAD( &state->globalJNI->ti, env );
    }

    XWJNI_END();                /* matches second XWJNI_START_GLOBALS! */
    XWJNI_END();
    return success;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1makeFromInvite
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jnli, jobject jutil,
  jobject jSelfAddr, jobject jcp, jobject jprocs )
{
    jboolean result;
    XWJNI_START_GLOBALS(gamePtr);

    initGameGlobals( env, state, jutil, jprocs );

    NetLaunchInfo nli;
    loadNLI( env, &nli, jnli );
    LOGNLI( &nli );

    XP_ASSERT( !!jSelfAddr );
    CommsAddrRec* selfAddrP = NULL;
    CommsAddrRec selfAddr;
    if ( !!jSelfAddr ) {
        selfAddrP = &selfAddr;
        getJAddrRec( env, selfAddrP, jSelfAddr );
    }
    CommonPrefs cp;
    loadCommonPrefs( env, &cp, jcp );

    result = game_makeFromInvite( &state->game, env, &nli, selfAddrP, globals->util,
                                  globals->dctx, &cp, globals->xportProcs );
    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL Java_org_eehouse_android_xw4_jni_XwJNI_game_1dispose
( JNIEnv* env, jclass claz, GamePtrType gamePtr )
{
    JNIState* state = getState( env, gamePtr, __func__ );
    XP_ASSERT( state->guard == GAME_GUARD );

#ifdef MEM_DEBUG
    MemPoolCtx* mpool = state->mpool;
#endif
    AndGameGlobals* globals = &state->globals;

    game_dispose( &state->game, env );
    /* Must happen after game_dispose, which uses it */
    destroyGI( MPPARM(mpool) &globals->gi );

    destroyDraw( &globals->dctx, env );
    destroyXportProcs( &globals->xportProcs, env );
    destroyUtil( &globals->util, env );
    vtmgr_destroy( MPPARM(mpool) globals->vtMgr );

    MAP_REMOVE( &state->globalJNI->ti, env );

#ifdef DEBUG
    XP_MEMSET( state, -1, sizeof(*state) );
#endif
    XP_FREE( mpool, state );
} /* game_dispose */

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1makeFromStream
( JNIEnv* env, jclass C, GamePtrType gamePtr, jbyteArray jstream, jobject /*out*/jgi,
  jobject jutil, jobject jdraw, jobject jcp, jobject jprocs )
{
    jboolean result;
    XWJNI_START_GLOBALS(gamePtr);

    globals->gi = (CurGameInfo*)XP_CALLOC( mpool, sizeof(*globals->gi) );
    globals->util = makeUtil( MPPARM(mpool) env,
                              TI_IF(&state->globalJNI->ti)
                              jutil, globals->gi, globals);
    globals->jniutil = state->globalJNI->jniutil;
    if ( !!jdraw ) {
        globals->dctx = makeDraw( MPPARM(mpool) env,
                                  TI_IF(&state->globalJNI->ti)
                                  jdraw );
    }
    globals->xportProcs = makeXportProcs( MPPARM(mpool) env, globals->util,
                                          TI_IF(&state->globalJNI->ti)
                                          jprocs );

    XWStreamCtxt* stream = streamFromJStream( MPPARM(mpool) env, 
                                              globals->vtMgr, jstream );

    CommonPrefs cp;
    loadCommonPrefs( env, &cp, jcp );
    result = game_makeFromStream( MPPARM(mpool) env, stream, &state->game,
                                  globals->gi, globals->util, globals->dctx,
                                  &cp, globals->xportProcs );
    stream_destroy( stream );

    if ( result ) {
        XP_ASSERT( 0 != globals->gi->gameID );
        if ( !!jgi ) {
            setJGI( env, jgi, globals->gi );
        }
    }

    XWJNI_END();
    return result;
} /* makeFromStream */

JNIEXPORT jbyteArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1saveToStream
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jgi )
{
    jbyteArray result;
    XWJNI_START_GLOBALS(gamePtr);

    /* Use our copy of gi if none's provided.  That's because only the caller
       knows if its gi should win -- user has changed game config -- or if
       ours should -- changes like remote players being added. */
    CurGameInfo* gi = 
        (NULL == jgi) ? globals->gi : makeGI( MPPARM(mpool) env, jgi );
    XWStreamCtxt* stream = mem_stream_make_sized( MPPARM(mpool) globals->vtMgr,
                                                  state->lastSavedSize,
                                                  NULL, 0, NULL, NULL );
    XP_ASSERT( gi_equal( gi, globals->util->gameInfo ) );
    game_saveToStream( &state->game, gi, stream, ++state->curSaveCount );

    if ( NULL != jgi ) {
        destroyGI( MPPARM(mpool) &gi );
    }

    state->lastSavedSize = stream_getSize( stream );
    result = streamToBArray( env, stream );
    stream_destroy( stream );

    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1saveSucceeded
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    XWJNI_START(gamePtr);
    game_saveSucceeded( &state->game, env, state->curSaveCount );
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setDraw
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jdraw )
{
    XWJNI_START_GLOBALS(gamePtr);

    DrawCtx* newDraw = makeDraw( MPPARM(mpool) env, TI_IF(&state->globalJNI->ti) jdraw );
    board_setDraw( state->game.board, env, newDraw );

    destroyDraw( &globals->dctx, env );
    globals->dctx = newDraw;

    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1invalAll
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    XWJNI_START(gamePtr);
    board_invalAll( state->game.board );
    XWJNI_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1draw
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START(gamePtr);
    result = board_draw( state->game.board, env );
    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1drawSnapshot
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jdraw, jint width,
  jint height )
{
    XWJNI_START(gamePtr);
    DrawCtx* newDraw = makeDraw( MPPARM(mpool) env,
                                 TI_IF(&state->globalJNI->ti) jdraw );
    board_drawSnapshot( state->game.board, env, newDraw, width, height );
    destroyDraw( &newDraw, env );
    XWJNI_END();
}

#ifdef COMMON_LAYOUT
JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1figureLayout
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jgi, jint left, jint top,
  jint width, jint height, jint scorePct, jint trayPct, jint scoreWidth,
  jint fontWidth, jint fontHt, jboolean squareTiles, jobject jdims )
{
    XWJNI_START(gamePtr);
    CurGameInfo* gi = makeGI( MPPARM(mpool) env, jgi );

    BoardDims dims;
    board_figureLayout( state->game.board, env, gi, left, top, width, height,
                        115, scorePct, trayPct, scoreWidth,
                        fontWidth, fontHt, squareTiles,
                        ((!!jdims) ? &dims : NULL) );

    destroyGI( MPPARM(mpool) &gi );

    if ( !!jdims ) {
        dimsCtoJ( env, jdims, &dims );
    }
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1applyLayout
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jdims )
{
    XWJNI_START(gamePtr);
    BoardDims dims;
    dimsJToC( env, &dims, jdims );
    board_applyLayout( state->game.board, env, &dims );
    XWJNI_END();
}

#else

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setScoreboardLoc
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint left, jint top,
  jint width, jint height, jboolean divideHorizontally )
{
    XWJNI_START(gamePtr);
    board_setScoreboardLoc( state->game.board, left, top, width, 
                            height, divideHorizontally );
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setTrayLoc
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint left, jint top,
  jint width, jint height, jint minDividerWidth )
{
    XWJNI_START(gamePtr);
    board_setTrayLoc( state->game.board, left, top, width, height, 
                      minDividerWidth );
    XWJNI_END();
}
#endif

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1zoom
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint zoomBy, jbooleanArray jCanZoom )
{
    jboolean result;
    XWJNI_START(gamePtr);
    XP_Bool canInOut[2];
    result = board_zoom( state->game.board, env, zoomBy, canInOut );
    jboolean canZoom[2] = { canInOut[0], canInOut[1] };
    setBoolArray( env, jCanZoom, VSIZE(canZoom), canZoom );
    XWJNI_END();
    return result;
}

#ifdef XWFEATURE_ACTIVERECT
JNIEXPORT jboolean JNICALL 
Java_org_eehouse_android_xw4_jni_XwJNI_board_1getActiveRect
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jrect, jintArray dims )
{
    jboolean result;
    XWJNI_START(gamePtr);
    XP_Rect rect;
    XP_U16 nCols, nRows;
    result = board_getActiveRect( state->game.board, &rect, &nCols, &nRows );
    if ( result ) {
        setInt( env, jrect, "left", rect.left );
        setInt( env, jrect, "top", rect.top );
        setInt( env, jrect, "right", rect.left + rect.width );
        setInt( env, jrect, "bottom", rect.top + rect.height );
        if ( !!dims ) {
            setIntInArray( env, dims, 0, nCols );
            setIntInArray( env, dims, 1, nRows );
        }
    }
    XWJNI_END();
    return result;
}
#endif

#ifdef POINTER_SUPPORT
JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1handlePenDown
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint xx, jint yy, jbooleanArray barray )
{
    jboolean result;
    XWJNI_START(gamePtr);
    XP_Bool bb;                 /* drop this for now */
    result = board_handlePenDown( state->game.board, env, xx, yy, &bb );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1handlePenMove
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint xx, jint yy )
{
    jboolean result;
    XWJNI_START(gamePtr);
    result = board_handlePenMove( state->game.board, env, xx, yy );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1handlePenUp
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint xx, jint yy )
{
    jboolean result;
    XWJNI_START(gamePtr);
    result = board_handlePenUp( state->game.board, env, xx, yy );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1containsPt
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint xx, jint yy )
{
    jboolean result;
    XWJNI_START(gamePtr);
    result = board_containsPt( state->game.board, xx, yy );
    XWJNI_END();
    return result;
}
#endif

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1juggleTray
(JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START(gamePtr);
    result = board_juggleTray( state->game.board, env );
    XWJNI_END();
    return result;
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1getTrayVisState
(JNIEnv* env, jclass C, GamePtrType gamePtr)
{
    jboolean result;
    XWJNI_START(gamePtr);
    result = board_getTrayVisState( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1getLikelyChatter
(JNIEnv* env, jclass C, GamePtrType gamePtr)
{
    jint result;
    XWJNI_START(gamePtr);
    result = board_getLikelyChatter( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1passwordProvided
(JNIEnv* env, jclass C, GamePtrType gamePtr, jint player, jstring jpasswd )
{
    jboolean result;
    XWJNI_START(gamePtr);
    const char* passwd = (*env)->GetStringUTFChars( env, jpasswd, NULL );
    result = board_passwordProvided( state->game.board, env, player, passwd );
    (*env)->ReleaseStringUTFChars( env, jpasswd, passwd );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1hideTray
(JNIEnv* env, jclass C, GamePtrType gamePtr)
{
    jboolean result;
    XWJNI_START(gamePtr);
    result = board_hideTray( state->game.board, env);
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1showTray
(JNIEnv* env, jclass C, GamePtrType gamePtr)
{
    jboolean result;
    XWJNI_START(gamePtr);
    result = board_showTray( state->game.board, env );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1beginTrade
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START(gamePtr);
    result = board_beginTrade( state->game.board, env );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1endTrade
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START(gamePtr);
    result = board_endTrade( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setBlankValue
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint player,
  jint col, jint row, jint tile )
{
    jboolean result;
    XWJNI_START(gamePtr);
    result = board_setBlankValue( state->game.board, player, col, row, tile );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1commitTurn
( JNIEnv* env, jclass C, GamePtrType gamePtr, jboolean phoniesConfirmed,
  jint badWordsKey, jboolean turnConfirmed, jintArray jNewTiles )
{
    jboolean result;
    XWJNI_START(gamePtr);
    TrayTileSet* newTilesP = NULL;
    TrayTileSet newTiles;

    if ( jNewTiles != NULL ) {
        tilesArrayToTileSet( env, jNewTiles, &newTiles );
        newTilesP = &newTiles;
    }
    PhoniesConf pc = {.confirmed = phoniesConfirmed,
        .key = badWordsKey,
    };
    result = board_commitTurn( state->game.board, env,
                               phoniesConfirmed ? &pc : NULL,
                               turnConfirmed, newTilesP );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1flip
(JNIEnv* env, jclass C, GamePtrType gamePtr)
{
    jboolean result;
    XWJNI_START(gamePtr);
    result = board_flip( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1replaceTiles
(JNIEnv* env, jclass C, GamePtrType gamePtr)
{
    jboolean result;
    XWJNI_START(gamePtr);
    result = board_replaceTiles( state->game.board, env );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL 
Java_org_eehouse_android_xw4_jni_XwJNI_board_1redoReplacedTiles
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START(gamePtr);
    result = board_redoReplacedTiles( state->game.board, env );
    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1reset
(JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    XWJNI_START(gamePtr);
    server_reset( state->game.server, env, state->game.comms );
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1handleUndo
(JNIEnv* env, jclass C, GamePtrType gamePtr)
{
    XWJNI_START(gamePtr);
    server_handleUndo( state->game.server, env, 0 );
    XWJNI_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1do
(JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START(gamePtr);
    XP_ASSERT( !!state->game.server );
    result = server_do( state->game.server, env );
    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1tilesPicked
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint player, jintArray jNewTiles )
{
    XWJNI_START(gamePtr);
    TrayTileSet newTiles;
    tilesArrayToTileSet( env, jNewTiles, &newTiles );
    server_tilesPicked( state->game.server, env, player, &newTiles );
    XWJNI_END();
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1countTilesInPool
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jint result;
    XWJNI_START(gamePtr);
    result = server_countTilesInPool( state->game.server );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1requestHint
( JNIEnv* env, jclass C, GamePtrType gamePtr, jboolean useLimits,
  jboolean goBack, jbooleanArray workRemains )
{
    jboolean result;
    XWJNI_START(gamePtr);
    XP_Bool tmpbool;
    result = board_requestHint( state->game.board, env,
#ifdef XWFEATURE_SEARCHLIMIT
                                useLimits, 
#endif
                                goBack, &tmpbool );
    /* If passed need to do workRemains[0] = tmpbool */
    if ( workRemains ) {
        jboolean jbool = tmpbool;
        setBoolArray( env, workRemains, 1, &jbool );
    }
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_timerFired
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint why, jint when, jint handle )
{
    jboolean result;
    XWJNI_START_GLOBALS(gamePtr);
    XW_UtilCtxt* util = globals->util;
    result = utilTimerFired( util, env, why, handle );
    XWJNI_END();
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1formatRemainingTiles
(JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jstring result;
    XWJNI_START_GLOBALS(gamePtr);
    XWStreamCtxt* stream = mem_stream_make( MPPARM(mpool) globals->vtMgr,
                                            NULL, 0, NULL, NULL );
    board_formatRemainingTiles( state->game.board, env, stream );
    result = streamToJString( env, stream );
    stream_destroy( stream );

    XWJNI_END();
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1formatDictCounts
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint nCols )
{
    jstring result;
    XWJNI_START_GLOBALS(gamePtr);
    XWStreamCtxt* stream = and_empty_stream( MPPARM(mpool) globals );
    server_formatDictCounts( state->game.server, env, stream, nCols, XP_FALSE );
    result = streamToJString( env, stream );
    stream_destroy( stream );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1getGameIsOver
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START(gamePtr);
    result = server_getGameIsOver( state->game.server );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1getGameIsConnected
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START(gamePtr);
    result = server_getGameIsConnected( state->game.server );
    XWJNI_END();
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_model_1writeGameHistory
( JNIEnv* env, jclass C, GamePtrType gamePtr, jboolean gameOver )
{
    jstring result;
    XWJNI_START_GLOBALS(gamePtr);
    XWStreamCtxt* stream = and_empty_stream( MPPARM(mpool) globals );
    model_writeGameHistory( state->game.model, env, stream,
                            state->game.server, gameOver );
    result = streamToJString( env, stream );
    stream_destroy( stream );
    XWJNI_END();
    return result;
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_model_1getNMoves
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jint result;
    XWJNI_START(gamePtr);
    XP_ASSERT( !!state->game.model );
    result = model_getNMoves( state->game.model );
    XWJNI_END();
    return result;
}


JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_model_1getNumTilesInTray
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint player )
{
    jint result;
    XWJNI_START(gamePtr);
    XP_ASSERT( !!state->game.model );
    result = model_getNumTilesInTray( state->game.model, player );
    XWJNI_END();
    return result;
}

JNIEXPORT jobject JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_model_1getPlayersLastScore
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint player )
{
    jobject jlmi;
    XWJNI_START(gamePtr);
    XP_ASSERT( !!state->game.model );
    LastMoveInfo lmi;
    XP_Bool valid = model_getPlayersLastScore( state->game.model, env,
                                               player, &lmi );

    jlmi = makeObjectEmptyConstr( env, PKG_PATH("jni/LastMoveInfo") );
    setBool( env, jlmi, "isValid", valid );
    if ( valid ) {
        setBool( env, jlmi, "inDuplicateMode", lmi.inDuplicateMode );
        setInt( env, jlmi, "score", lmi.score );
        setInt( env, jlmi, "nTiles", lmi.nTiles );
        setInt( env, jlmi, "moveType", lmi.moveType );
        setStringArray( env, jlmi, "names", lmi.nWinners, lmi.names );
        setString( env, jlmi, "word", lmi.word );
    }
    XWJNI_END();
    return jlmi;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1writeFinalScores
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jstring result;
    XWJNI_START_GLOBALS(gamePtr);
    XWStreamCtxt* stream = and_empty_stream( MPPARM(mpool) globals );
    server_writeFinalScores( state->game.server, env, stream );
    result = streamToJString( env, stream );
    stream_destroy( stream );
    XWJNI_END();
    return result;
}

void
and_send_on_close( XWStreamCtxt* stream, XWEnv xwe, void* closure )
{
    AndGameGlobals* globals = (AndGameGlobals*)closure;
    JNIState* state = (JNIState*)globals->state;

    XP_ASSERT( !!state->game.comms );
    comms_send( state->game.comms, xwe, stream );
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1initClientConnection
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    LOG_FUNC();
    XWJNI_START_GLOBALS(gamePtr);
    result = server_initClientConnection( state->game.server, env );
    XWJNI_END();
    LOG_RETURNF( "%s", boolToStr(result) );
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1canOfferRematch
( JNIEnv* env, jclass C, GamePtrType gamePtr, jbooleanArray results  )
{
    XWJNI_START_GLOBALS(gamePtr);
    XP_Bool bools[2];
    bools[0] = server_canRematch( state->game.server, &bools[1] );
    setBoolArray( env, results, VSIZE(bools), (jboolean*)bools );

    XWJNI_END();
}

JNIEXPORT jintArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1figureOrder
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jRo )
{
    jintArray result;
    XWJNI_START_GLOBALS(gamePtr);
    RematchOrder ro = jEnumToInt( env, jRo );
    XP_LOGFF( "(ro=%s)", RO2Str(ro) );

    NewOrder no;
    server_figureOrder( state->game.server, ro, &no );

    result = makeIntArray( env, globals->gi->nPlayers, no.order,
                           sizeof(no.order[0]) );

    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1start
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    XWJNI_START(gamePtr);
    CommsCtxt* comms = state->game.comms;
    if ( !!comms ) {
        comms_start( comms, env );
    }
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1stop
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    XWJNI_START(gamePtr);
    CommsCtxt* comms = state->game.comms;
    if ( !!comms ) {
        comms_stop( comms
#ifdef XWFEATURE_RELAY
                    , env
#endif
                    );
    }
    XWJNI_END();
}

JNIEXPORT jobject JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1getSelfAddr
(JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jobject jaddr;
    XWJNI_START(gamePtr);
    XP_ASSERT( state->game.comms );
    CommsAddrRec addr;
    comms_getSelfAddr( state->game.comms, &addr );
    jaddr = makeObjectEmptyConstr( env, PKG_PATH("jni/CommsAddrRec") );
    setJAddrRec( env, jaddr, &addr );
    XWJNI_END();
    return jaddr;
}

JNIEXPORT jobject JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1getHostAddr
(JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jobject jaddr = NULL;
    XWJNI_START(gamePtr);
    XP_ASSERT( state->game.comms );
    CommsAddrRec addr;
    if ( comms_getHostAddr( state->game.comms, &addr ) ) {
        jaddr = makeObjectEmptyConstr( env, PKG_PATH("jni/CommsAddrRec") );
        setJAddrRec( env, jaddr, &addr );
    }
    XWJNI_END();
    return jaddr;
}

JNIEXPORT jobjectArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1getAddrs
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jobjectArray result = NULL;
    XWJNI_START(gamePtr);
    XP_ASSERT( state->game.comms );
    if ( !!state->game.comms ) {
        CommsAddrRec addrs[MAX_NUM_PLAYERS];
        XP_U16 count = VSIZE(addrs);
        comms_getAddrs( state->game.comms, addrs, &count );
        result = makeAddrArray( env, count, addrs );
    }
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1receiveMessage
( JNIEnv* env, jclass C, GamePtrType gamePtr, jbyteArray jstream, jobject jaddr )
{
    jboolean result;
    XWJNI_START_GLOBALS(gamePtr);

    XWStreamCtxt* stream = streamFromJStream( MPPARM(mpool) env, globals->vtMgr,
                                              jstream );
    CommsAddrRec* addrp = NULL;
    CommsAddrRec addr = {};
    XP_ASSERT( !!jaddr );
    if ( NULL != jaddr ) {
        getJAddrRec( env, &addr, jaddr );
        addrp = &addr;
    }

    result = game_receiveMessage( &state->game, env, stream, addrp );

    stream_destroy( stream );

    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1summarize
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jsummary )
{
    XWJNI_START_GLOBALS(gamePtr);
    GameSummary summary = {};
    game_summarize( &state->game, globals->gi, &summary );

    setInt( env, jsummary, "nMoves", summary.nMoves );
    setBool( env, jsummary, "gameOver", summary.gameOver );
    setBool( env, jsummary, "quashed", summary.quashed );
    setInt( env, jsummary, "turn", summary.turn );
    setBool( env, jsummary, "turnIsLocal", summary.turnIsLocal );
    setBool( env, jsummary, "canRematch", summary.canRematch );
    setInt( env, jsummary, "lastMoveTime", summary.lastMoveTime );
    setInt( env, jsummary, "dupTimerExpires", summary.dupTimerExpires );
    
    if ( !!state->game.comms ) {
        CommsCtxt* comms = state->game.comms;
        setInt( env, jsummary, "seed", comms_getChannelSeed( comms ) );
        setInt( env, jsummary, "missingPlayers", summary.missingPlayers );
        setInt( env, jsummary, "nPacketsPending", summary.nPacketsPending );

        CommsAddrRec addr;
        comms_getSelfAddr( comms, &addr );
        setTypeSetFieldIn( env, &addr, jsummary, "conTypes" );

        CommsConnType typ;
        for ( XP_U32 st = 0; addr_iter( &addr, &typ, &st ); ) {
            switch( typ ) {
            case COMMS_CONN_RELAY: {
#ifdef XWFEATURE_RELAY
                XP_UCHAR buf[128];
                XP_U16 len = VSIZE(buf);
                if ( comms_getRelayID( comms, buf, &len ) ) {
                    XP_ASSERT( '\0' == buf[len-1] ); /* failed! */
                    setString( env, jsummary, "relayID", buf );
                }
                setString( env, jsummary, "roomName", addr.u.ip_relay.invite );
#endif
            }
                break;
            case COMMS_CONN_NFC:
            case COMMS_CONN_MQTT:
                break;
#if defined XWFEATURE_BLUETOOTH || defined XWFEATURE_SMS || defined XWFEATURE_P2P
            case COMMS_CONN_BT:
            case COMMS_CONN_P2P:
            case COMMS_CONN_SMS: {
                CommsAddrRec addrs[MAX_NUM_PLAYERS];
                XP_U16 count = VSIZE(addrs);
                comms_getAddrs( comms, addrs, &count );
            
                const XP_UCHAR* addrps[count];
                for ( int ii = 0; ii < count; ++ii ) {
                    switch ( typ ) {
                    case COMMS_CONN_BT: addrps[ii] = (XP_UCHAR*)&addrs[ii].u.bt.btAddr; break;
                    case COMMS_CONN_P2P: addrps[ii] = (XP_UCHAR*)&addrs[ii].u.p2p.mac_addr; break;
                    case COMMS_CONN_SMS: addrps[ii] = (XP_UCHAR*)&addrs[ii].u.sms.phone; break;
                    default: XP_ASSERT(0); break;
                    }
                    XP_LOGFF( "adding btaddr/phone/mac %s", addrps[ii] );
                }
                setStringArray( env, jsummary, "remoteDevs", count, addrps );
            }
                break;
#endif
            default:
                XP_ASSERT(0);
            }
        }
    }

    ModelCtxt* model = state->game.model;
    XP_U16 nPlayers = model_getNPlayers( model );
    jint jvals[nPlayers];
    if ( summary.gameOver ) {
        ScoresArray scores;
        model_figureFinalScores( model, &scores, NULL );
        for ( int ii = 0; ii < nPlayers; ++ii ) {
            jvals[ii] = scores.arr[ii];
        }
    } else {
        for ( int ii = 0; ii < nPlayers; ++ii ) {
            jvals[ii] = model_getPlayerScore( model, ii );
        }
    }

    setIntArray( env, jsummary, "scores", nPlayers, jvals, sizeof(jvals[0]) );

    XWJNI_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1server_1prefsChanged
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jcp )
{
    jboolean result;
    XWJNI_START(gamePtr);

    CommonPrefs cp;
    loadCommonPrefs( env, &cp, jcp );

    result = board_prefsChanged( state->game.board, &cp );
    server_prefsChanged( state->game.server, &cp );

    XWJNI_END();
    return result;
}

#ifdef KEYBOARD_NAV
JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1getFocusOwner
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jint result;
    XWJNI_START(gamePtr);
    result = board_getFocusOwner( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1focusChanged
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint typ )
{
    jboolean result;
    XWJNI_START(gamePtr);
    result = board_focusChanged( state->game.board, typ, XP_TRUE );
    XWJNI_END();
    return result;
}
#endif

#ifdef KEYBOARD_NAV
JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1handleKey
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jkey, jboolean jup,
  jbooleanArray jhandled )
{
    jboolean result;
    XWJNI_START(gamePtr);

    XP_Bool tmpbool;
    XP_Key key = jEnumToInt( env, jkey );
    if ( jup ) {
        result = board_handleKeyUp( state->game.board, key, &tmpbool );
    } else {
        result = board_handleKeyDown( state->game.board, key, &tmpbool );
    }
    jboolean jbool = tmpbool;
    setBoolArray( env, jhandled, 1, &jbool );
    XWJNI_END();
    return result;
}
#endif

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1getGi
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jgi )
{
    XWJNI_START_GLOBALS(gamePtr);
    setJGI( env, jgi, globals->gi );
    XWJNI_END();
}

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

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1getState
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jgsi )
{
    XWJNI_START(gamePtr);
    GameStateInfo info;
    game_getState( &state->game, env, &info );

    setInts( env, jgsi, (void*)&info, AANDS(gsi_ints) );
    setBools( env, jgsi, (void*)&info, AANDS(gsi_bools) );

    XWJNI_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1hasComms
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START(gamePtr);
    result = NULL != state->game.comms;
    XWJNI_END();
    return result;
}

#ifdef XWFEATURE_CHANGEDICT
JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1changeDict
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jgi, jstring jname,
  jbyteArray jDictBytes, jstring jpath )
{
    XWJNI_START_GLOBALS(gamePtr);
    DictionaryCtxt* dict = makeDict( MPPARM(state->globalJNI->mpool) env,
                                     TI_IF(&globalState->ti)
                                     state->globalJNI->dictMgr, 
                                     globals->jniutil, jname, jDictBytes, 
                                     jpath, NULL, false );
    game_changeDict( MPPARM(mpool) &state->game, globals->gi, dict );
    dict_unref( dict );
    setJGI( env, jgi, globals->gi );
    XWJNI_END();
    return XP_FALSE;            /* no need to redraw */
}
#endif

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1resendAll
( JNIEnv* env, jclass C, GamePtrType gamePtr, jboolean force, jobject jFilter,
  jboolean thenAck )
{
    jint result = 0;
    XWJNI_START(gamePtr);
    CommsCtxt* comms = state->game.comms;
    XP_ASSERT( !!comms );
    if ( !!comms ) {
        CommsConnType filter =
            NULL == jFilter ? COMMS_CONN_NONE : jEnumToInt( env, jFilter );
        result = comms_resendAll( comms, env, filter, force );
        if ( thenAck ) {
#ifdef XWFEATURE_COMMSACK
            comms_ackAny( comms, env );
#endif
        }
    } else {
        /* I've seen this once, but wasn't reproducible */
        XP_LOGFF( "ERROR: called with null comms" );
    }
    XWJNI_END();
    return result;
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1countPendingPackets
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jint result = 0;
    XWJNI_START(gamePtr);
    RELCONST CommsCtxt* comms = state->game.comms;
    if ( !!comms ) {
        result = comms_countPendingPackets( comms, NULL );
    }
    XWJNI_END();
    return result;
}

#ifdef XWFEATURE_COMMSACK
JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1ackAny
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    XWJNI_START(gamePtr);
    XP_ASSERT( !!state->game.comms );
    (void)comms_ackAny( state->game.comms, env );
    XWJNI_END();
}
#endif

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1isConnected
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START(gamePtr);
    result = NULL != state->game.comms && comms_isConnected( state->game.comms );
    XWJNI_END();
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1getStats
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jstring result = NULL;
#ifdef DEBUG
    XWJNI_START_GLOBALS(gamePtr);
    if ( NULL != state->game.comms ) {
        XWStreamCtxt* stream = mem_stream_make( MPPARM(mpool) globals->vtMgr,
                                                NULL, 0, NULL, NULL );
        comms_getStats( state->game.comms, stream );
        result = streamToJString( env, stream );
        stream_destroy( stream );
    }
    XWJNI_END();
#endif
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1addMQTTDevID
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint channel, jstring jdevid )
{
    XWJNI_START_GLOBALS(gamePtr);
    if ( NULL != state->game.comms ) {
        const char* str = (*env)->GetStringUTFChars( env, jdevid, NULL );
        MQTTDevID devID;
        if ( strToMQTTCDevID( str, &devID ) ) {
            comms_addMQTTDevID( state->game.comms, channel, &devID );
        }
        (*env)->ReleaseStringUTFChars( env, jdevid, str );
    }
    XWJNI_END();
}

#ifdef XWFEATURE_COMMS_INVITE
JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1invite
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jnli, jobject jaddr,
  jboolean jSendNow )
{
    XWJNI_START_GLOBALS(gamePtr);
    CommsCtxt* comms = state->game.comms;
    XP_ASSERT( NULL != comms );
    if ( NULL != comms ) {
        CommsAddrRec destAddr;
        getJAddrRec( env, &destAddr, jaddr );
        NetLaunchInfo nli;
        loadNLI( env, &nli, jnli );

        comms_invite( comms, env, &nli, &destAddr, jSendNow );
    }
    XWJNI_END();
}
#endif

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1dropHostAddr
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jConnTyp )
{
    LOG_FUNC();
    XWJNI_START(gamePtr);
    if ( NULL != state->game.comms ) {
        CommsConnType connType = jEnumToInt( env, jConnTyp );
        comms_dropHostAddr( state->game.comms, connType );
    }
    XWJNI_END();
    LOG_RETURN_VOID();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1setQuashed
( JNIEnv* env, jclass C, GamePtrType gamePtr, jboolean jQuashed )
{
    jboolean result = false;
    XWJNI_START(gamePtr);
    XP_ASSERT( !!state->game.comms );
    if ( NULL != state->game.comms ) {
        result = comms_setQuashed( state->game.comms, env, jQuashed );
    }
    XWJNI_END();
    return result;
}

#ifdef DEBUG
JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1setAddrDisabled
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jConnTyp,
  jboolean forSend, jboolean val )
{
    XWJNI_START(gamePtr);
    if ( NULL != state->game.comms ) {
        CommsConnType connType = jEnumToInt( env, jConnTyp );
        comms_setAddrDisabled( state->game.comms, connType, forSend, val );
    }
    XWJNI_END();
}
#endif

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1getAddrDisabled
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jConnTyp,
  jboolean forSend )
{
    jboolean result = XP_FALSE;
#ifdef DEBUG
    XWJNI_START(gamePtr);
    if ( NULL != state->game.comms ) {
        CommsConnType connType = jEnumToInt( env, jConnTyp );
        result = comms_getAddrDisabled( state->game.comms, connType, forSend );
    }
    XWJNI_END();
#endif
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_haveEnv
( JNIEnv* env, jclass C, jlong jniGlobalPtr )
{
    jboolean result = XP_TRUE;
#ifdef MAP_THREAD_TO_ENV
    JNIGlobalState* globalState = (JNIGlobalState*)jniGlobalPtr;
    result = NULL != prvEnvForMe(&globalState->ti);
#endif
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1endGame
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    XWJNI_START(gamePtr);
    XP_ASSERT( !!state->game.server );
    server_endGame( state->game.server, env );
    XWJNI_END();
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1inviteeName
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint channel )
{
    jstring result = NULL;
    XWJNI_START(gamePtr);
    XP_ASSERT( !!state->game.server );
    XP_UCHAR buf[32] = {};
    XP_U16 len = VSIZE(buf);
    server_inviteeName( state->game.server, env, channel, buf, &len );
    if ( !!buf[0] ) {
        result = (*env)->NewStringUTF( env, buf );
    }
    XWJNI_END();
    return result;
}


JNIEXPORT void JNICALL Java_org_eehouse_android_xw4_jni_XwJNI_board_1pause
( JNIEnv* env, jclass C, GamePtrType gamePtr, jstring jmsg )
{
    XWJNI_START(gamePtr);
    XP_ASSERT( !!state->game.board );

    const char* msg = (*env)->GetStringUTFChars( env, jmsg, NULL );
    board_pause( state->game.board, env, msg );
    (*env)->ReleaseStringUTFChars( env, jmsg, msg );

    XWJNI_END();
}

JNIEXPORT void JNICALL Java_org_eehouse_android_xw4_jni_XwJNI_board_1unpause
( JNIEnv* env, jclass C, GamePtrType gamePtr, jstring jmsg )
{
    XWJNI_START(gamePtr);
    XP_ASSERT( !!state->game.board );
    const char* msg = (*env)->GetStringUTFChars( env, jmsg, NULL );
    board_unpause( state->game.board, env, msg );
    (*env)->ReleaseStringUTFChars( env, jmsg, msg );
    XWJNI_END();
}

#ifdef XWFEATURE_CHAT
JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1sendChat
( JNIEnv* env, jclass C, GamePtrType gamePtr, jstring jmsg )
{
    XWJNI_START(gamePtr);
    XP_ASSERT( !!state->game.server );
    const char* msg = (*env)->GetStringUTFChars( env, jmsg, NULL );
    board_sendChat( state->game.board, env, msg );
    (*env)->ReleaseStringUTFChars( env, jmsg, msg );
    XWJNI_END();
}
#endif

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
Java_org_eehouse_android_xw4_jni_XwJNI_di_1init
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
            iter = di_makeIter( dict, env, &mm, NULL, 0,
                                !!jPatsArr ? patDescs : NULL, VSIZE(patDescs) );
        }

        if ( !!iter ) {
            DictIterData* data = XP_CALLOC( globalState->mpool, sizeof(*data) );
            data->iter = iter;
            data->globalState = globalState;
            data->dict = dict_ref( dict, env );
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
Java_org_eehouse_android_xw4_jni_XwJNI_di_1destroy
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
Java_org_eehouse_android_xw4_jni_XwJNI_di_1wordCount
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
Java_org_eehouse_android_xw4_jni_XwJNI_di_1getMinMax
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
Java_org_eehouse_android_xw4_jni_XwJNI_di_1getPrefixes
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
Java_org_eehouse_android_xw4_jni_XwJNI_di_1getIndices
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
Java_org_eehouse_android_xw4_jni_XwJNI_di_1nthWord
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

#endif  /* XWFEATURE_BOARDWORDS */
