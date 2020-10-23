/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright © 2009 - 2020 by Eric House (xwords@eehouse.org).  All rights
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

#include "utilwrapper.h"
#include "drawwrapper.h"
#include "xportwrapper.h"
#include "anddict.h"
#include "andutils.h"
#include "andglobals.h"
#include "jniutlswrapper.h"
#include "paths.h"

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
    pthread_mutex_t mtxThreads;
    int nEntries;
    EnvThreadEntry* entries;
    MPSLOT
};

#endif

/* Globals for the whole game */
typedef struct _JNIGlobalState {
#ifdef MAP_THREAD_TO_ENV
    EnvThreadInfo ti;
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

#ifdef MEM_DEBUG
static MemPoolCtx*
getMPoolImpl( JNIGlobalState* globalState, const char* user )
{
    if ( globalState->mpoolInUse ) {
        XP_LOGF( "%s(): mpoolInUse ALREADY SET!!!! (by %s)",
                 __func__, globalState->mpoolUser );
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
        XP_LOGF( "%s() line %d; ERROR ERROR ERROR mpoolInUse not set",
                 __func__, __LINE__ );
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
            XP_LOGF( "%s(): ii=%d; owner: %x", __func__, ii, (unsigned int)entry->owner );
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

    pthread_mutex_lock( &ti->mtxThreads );

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

    pthread_mutex_unlock( &ti->mtxThreads );
} /* map_thread_prv */

static void
map_init( MPFORMAL EnvThreadInfo* ti, JNIEnv* env )
{
    pthread_mutex_init( &ti->mtxThreads, NULL );
    MPASSIGN( ti->mpool, mpool );
    MAP_THREAD( ti, env );
}

#define MAP_REMOVE( ti, env ) map_remove_prv((ti), (env), __func__)
static void
map_remove_prv( EnvThreadInfo* ti, JNIEnv* env, const char* func )
{
    XP_Bool found = false;

    pthread_mutex_lock( &ti->mtxThreads );
    for ( int ii = 0; !found && ii < ti->nEntries; ++ii ) {
        EnvThreadEntry* entry = &ti->entries[ii];
        found = env == entry->env;
        if ( found ) {
            XP_ASSERT( pthread_self() == entry->owner );
#ifdef LOG_MAPPING
            RAW_LOG( "UNMAPPED env %p to thread %x (from %s; mapped by %s)",
                     entry->env, (int)entry->owner, func, entry->ownerFunc );
            RAW_LOG( "%d entries left", countUsed( ti ) );
            entry->ownerFunc = NULL;
#endif
            XP_ASSERT( 1 == entry->refcount );
            --entry->refcount;
            entry->env = NULL;
            entry->owner = 0;
        }
    }
    pthread_mutex_unlock( &ti->mtxThreads );

    XP_ASSERT( found );
}

static void
map_destroy( EnvThreadInfo* ti )
{
    pthread_mutex_destroy( &ti->mtxThreads );
}

static JNIEnv*
prvEnvForMe( EnvThreadInfo* ti )
{
    JNIEnv* result = NULL;
    pthread_t self = pthread_self();
    pthread_mutex_lock( &ti->mtxThreads );
    for ( int ii = 0; !result && ii < ti->nEntries; ++ii ) {
        if ( self == ti->entries[ii].owner ) {
            result = ti->entries[ii].env;
        }
    }
    pthread_mutex_unlock( &ti->mtxThreads );
    return result;
}

#else
# define MAP_THREAD( ti, env )
# define MAP_REMOVE( ti, env )
# define map_init( ... )
# define map_destroy( ti )
#endif // MAP_THREAD_TO_ENV

#ifdef MAP_THREAD_TO_ENV
static pthread_mutex_t g_globalStateLock = PTHREAD_MUTEX_INITIALIZER;
static JNIGlobalState* g_globalState = NULL;

void setGlobalState( JNIGlobalState* state )
{
    pthread_mutex_lock( &g_globalStateLock );
    g_globalState = state;
    pthread_mutex_unlock( &g_globalStateLock );
}

JNIEnv*
envForMe( EnvThreadInfo* ti, const char* caller )
{
    JNIEnv* result = prvEnvForMe( ti );
#ifdef DEBUG
    if( !result ) {
        pthread_t self = pthread_self();
        XP_LOGF( "no env for %s (thread %x)", caller, (int)self );
        XP_ASSERT(0);
    }
#endif
    return result;
}

#else
# define setGlobalState(s)
#endif

JNIEnv*
waitEnvFromGlobals()            /* hanging */
{
    JNIEnv* result = NULL;
#ifdef MAP_THREAD_TO_ENV
    pthread_mutex_lock( &g_globalStateLock );
    JNIGlobalState* state = g_globalState;
    if ( !!state ) {
        result = prvEnvForMe( &state->ti );
    }
    if ( !result ) {
        pthread_mutex_unlock( &g_globalStateLock );
    }
#endif
    return result;
}

void
releaseEnvFromGlobals( JNIEnv* env )
{
#ifdef MAP_THREAD_TO_ENV
    XP_ASSERT( !!env );
    JNIGlobalState* state = g_globalState;
    XP_ASSERT( !!state );
    XP_ASSERT( env == prvEnvForMe( &state->ti ) );
    pthread_mutex_unlock( &g_globalStateLock );
#endif
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
        XP_LOGF( "ERROR: getState() called from %s() with null gamePtr",
                 func );
    }
#endif
    XP_ASSERT( NULL != gamePtr ); /* fired */
    jmethodID mid = getMethodID( env, gamePtr, "ptr", "()J" );
    XP_ASSERT( !!mid );
    return (JNIState*)(*env)->CallLongMethod( env, gamePtr, mid );
}
#else
# define getState( env, gamePtr, func ) ((JNIState*)(gamePtr))
#endif

JNIEXPORT jlong JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_globalsInit
( JNIEnv* env, jclass C, jobject jdutil, jobject jniu, jlong jseed )
{
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = mpool_make( NULL );
    XP_LOGF( "%s(): ptr size: %zu", __func__, sizeof(mpool) );
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
    globalState->dictMgr = dmgr_make( MPPARM_NOCOMMA( mpool ) );
    globalState->smsProto = smsproto_init( MPPARM( mpool ) env, globalState->dutil );
    MPASSIGN( globalState->mpool, mpool );
    setGlobalState( globalState );
    // LOG_RETURNF( "%p", globalState );
    return (jlong)globalState;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_cleanGlobals
( JNIEnv* env, jclass C, jlong jniGlobalPtr )
{
    // LOG_FUNC();
    if ( 0 != jniGlobalPtr ) {
        setGlobalState( NULL );
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
    ,ARR_MEMBER( CurGameInfo, gameID )
    ,ARR_MEMBER( CurGameInfo, dictLang )
    ,ARR_MEMBER( CurGameInfo, forceChannel )
};

static const SetInfo gi_bools[] = {
    ARR_MEMBER( CurGameInfo, hintsNotAllowed )
    ,ARR_MEMBER( CurGameInfo, timerEnabled )
    ,ARR_MEMBER( CurGameInfo, allowPickTiles )
    ,ARR_MEMBER( CurGameInfo, allowHintRect )
    ,ARR_MEMBER( CurGameInfo, inDuplicateMode )
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
        while ( 0 == gi->gameID ) {
            gi->gameID = getCurSeconds( env );
        }
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

    XP_ASSERT( gi->nPlayers <= MAX_NUM_PLAYERS );

    jobject jplayers;
    if ( getObject( env, jgi, "players", "[L" PKG_PATH("jni/LocalPlayer") ";",
                    &jplayers ) ) {
        for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
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
    } else {
        XP_ASSERT(0);
    }

    return gi;
} /* makeGI */

static void
setJGI( JNIEnv* env, jobject jgi, const CurGameInfo* gi )
{
    // set fields

    setInts( env, jgi, (void*)gi, AANDS(gi_ints) );
    setBools( env, jgi, (void*)gi, AANDS(gi_bools) );

    setString( env, jgi, "dictName", gi->dictName );

    intToJenumField( env, jgi, gi->phoniesAction, "phoniesAction",
                     PKG_PATH("jni/CurGameInfo$XWPhoniesChoice") );
    intToJenumField( env, jgi, gi->serverRole, "serverRole",
                     PKG_PATH("jni/CurGameInfo$DeviceRole") );

    jobject jplayers;
    if ( getObject( env, jgi, "players", 
                    "[L" PKG_PATH("jni/LocalPlayer") ";",
                    &jplayers ) ) {
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
static const SetInfo bd_ints[] = {
    ARR_MEMBER( BoardDims, left )
    ,ARR_MEMBER( BoardDims, top )
    ,ARR_MEMBER( BoardDims, width )
    ,ARR_MEMBER( BoardDims, height )
    ,ARR_MEMBER( BoardDims, scoreLeft )
    ,ARR_MEMBER( BoardDims, scoreHt )
    ,ARR_MEMBER( BoardDims, scoreWidth )
    ,ARR_MEMBER( BoardDims, boardWidth )
    ,ARR_MEMBER( BoardDims, boardHt )
    ,ARR_MEMBER( BoardDims, trayLeft )
    ,ARR_MEMBER( BoardDims, trayTop )
    ,ARR_MEMBER( BoardDims, trayWidth )
    ,ARR_MEMBER( BoardDims, trayHt )
    ,ARR_MEMBER( BoardDims, cellSize )
    ,ARR_MEMBER( BoardDims, maxCellSize )
    ,ARR_MEMBER( BoardDims, timerWidth )
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
#ifdef XWFEATURE_CROSSHAIRS
    cp->hideCrosshairs = getBool( env, j_cp, "hideCrosshairs" );
#endif
}

static XWStreamCtxt*
streamFromJStream( MPFORMAL JNIEnv* env, VTableMgr* vtMgr, jbyteArray jstream )
{
    XP_ASSERT( !!jstream );
    int len = (*env)->GetArrayLength( env, jstream );
    XWStreamCtxt* stream = mem_stream_make_sized( MPPARM(mpool) vtMgr,
                                                  len, NULL, 0, NULL );
    jbyte* jelems = (*env)->GetByteArrayElements( env, jstream, NULL );
    stream_putBytes( stream, jelems, len );
    (*env)->ReleaseByteArrayElements( env, jstream, jelems, 0 );
    return stream;
} /* streamFromJStream */

/****************************************************
 * These methods are stateless: no gamePtr
 ****************************************************/

#define DVC_HEADER(PTR) {                                              \
    JNIGlobalState* globalState = (JNIGlobalState*)(PTR);              \

#define DVC_HEADER_END() }                      \


JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dvc_1getMQTTDevID
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jobjectArray jTopicOut )
{
    jstring result;
    DVC_HEADER(jniGlobalPtr);
    MQTTDevID devID;
    dvc_getMQTTDevID( globalState->dutil, env, &devID );

    XP_UCHAR buf[64];

    if ( !!jTopicOut ) {
        formatMQTTTopic( &devID, buf, VSIZE(buf) );
        jstring jtopic = (*env)->NewStringUTF( env, buf );
        XP_ASSERT( 1 == (*env)->GetArrayLength( env, jTopicOut ) ); /* fired */
        (*env)->SetObjectArrayElement( env, jTopicOut, 0, jtopic );
        deleteLocalRef( env, jtopic );
    }

    formatMQTTDevID( &devID, buf, VSIZE(buf) );
    result = (*env)->NewStringUTF( env, buf );
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

static void
addrToTopic( JNIEnv* env, jobjectArray jAddrToTopic )
{
    XP_ASSERT( 1 == (*env)->GetArrayLength( env, jAddrToTopic ) );
    jstring jaddr = (*env)->GetObjectArrayElement( env, jAddrToTopic, 0 );
    const char* addr = (*env)->GetStringUTFChars( env, jaddr, NULL );

    MQTTDevID devID;
#ifdef DEBUG
    XP_Bool success =
#endif
        strToMQTTCDevID( addr, &devID );
    XP_ASSERT( success );

    XP_UCHAR buf[64];
    formatMQTTTopic( &devID, buf, VSIZE(buf) );
    jstring jTopic = (*env)->NewStringUTF( env, buf );
    (*env)->SetObjectArrayElement( env, jAddrToTopic, 0, jTopic );

    (*env)->ReleaseStringUTFChars( env, jaddr, addr );
    deleteLocalRefs( env, jaddr, jTopic, DELETE_NO_REF );
}

JNIEXPORT jbyteArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dvc_1makeMQTTInvite
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jobject jnli,
  jobjectArray jAddrToTopic )
{
    jbyteArray result;
    DVC_HEADER(jniGlobalPtr);
    NetLaunchInfo nli = {0};
    loadNLI( env, &nli, jnli );
    LOGNLI( &nli );

    XWStreamCtxt* stream = mem_stream_make( MPPARM(globalState->mpool)
                                            globalState->vtMgr,
                                            NULL, 0, NULL );
    dvc_makeMQTTInvite( globalState->dutil, env, stream, &nli );

    result = streamToBArray( env, stream );
    stream_destroy( stream, env );

    addrToTopic( env, jAddrToTopic );

    DVC_HEADER_END();
    return result;
}

JNIEXPORT jbyteArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dvc_1makeMQTTMessage
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jint jGameID,
  jbyteArray jmsg, jobjectArray jAddrToTopic )
{
    jbyteArray result;
    LOG_FUNC();
    DVC_HEADER(jniGlobalPtr);

    XWStreamCtxt* stream = mem_stream_make( MPPARM(globalState->mpool)
                                            globalState->vtMgr,
                                            NULL, 0, NULL );

    XP_U16 len = (*env)->GetArrayLength( env, jmsg );
    jbyte* buf = (*env)->GetByteArrayElements( env, jmsg, NULL );
    dvc_makeMQTTMessage( globalState->dutil, env, stream, jGameID,
                         (const XP_U8*)buf, len );
    (*env)->ReleaseByteArrayElements( env, jmsg, buf, 0 );

    result = streamToBArray( env, stream );
    stream_destroy( stream, env );

    addrToTopic( env, jAddrToTopic );

    DVC_HEADER_END();
    return result;
}

JNIEXPORT jbyteArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dvc_1makeMQTTNoSuchGame
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jint jgameid, jobjectArray jAddrToTopic )
{
    jbyteArray result;
    DVC_HEADER(jniGlobalPtr);

    XWStreamCtxt* stream = mem_stream_make( MPPARM(globalState->mpool)
                                            globalState->vtMgr,
                                            NULL, 0, NULL );
    dvc_makeMQTTNoSuchGame( globalState->dutil, env, stream, jgameid );

    result = streamToBArray( env, stream );
    stream_destroy( stream, env );

    addrToTopic( env, jAddrToTopic );

    DVC_HEADER_END();
    LOG_RETURN_VOID();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dvc_1parseMQTTPacket
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jbyteArray jmsg )
{
    DVC_HEADER(jniGlobalPtr);

    XP_U16 len = (*env)->GetArrayLength( env, jmsg );
    jbyte* buf = (*env)->GetByteArrayElements( env, jmsg, NULL );

    dvc_parseMQTTPacket( globalState->dutil, env, (XP_U8*)buf, len );

    (*env)->ReleaseByteArrayElements( env, jmsg, buf, 0 );
    DVC_HEADER_END();
}

# ifdef XWFEATURE_KNOWNPLAYERS
JNIEXPORT jobjectArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_kplr_1getPlayers
( JNIEnv* env, jclass C, jlong jniGlobalPtr )
{
    jobjectArray jnames = NULL;
    DVC_HEADER(jniGlobalPtr);

    XP_U16 nFound = 0;
    kplr_getNames( globalState->dutil, env, NULL, &nFound );
    if ( 0 < nFound ) {
        const XP_UCHAR* names[nFound];
        kplr_getNames( globalState->dutil, env, names, &nFound );
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
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jstring jName )
{
    jobject jaddr = NULL;
    DVC_HEADER(jniGlobalPtr);

    CommsAddrRec addr;
    const char* name = (*env)->GetStringUTFChars( env, jName, NULL );
    kplr_getAddr( globalState->dutil, env, name, &addr );
    (*env)->ReleaseStringUTFChars( env, jName, name );
    jaddr = makeObjectEmptyConst( env, PKG_PATH("jni/CommsAddrRec") );
    setJAddrRec( env, jaddr, &addr );

    DVC_HEADER_END();
    return jaddr;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_kplr_1nameForMqttDev
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jstring jDevID )
{
    jstring result = NULL;
    DVC_HEADER(jniGlobalPtr);
    const char* devid = (*env)->GetStringUTFChars( env, jDevID, NULL );
    const XP_UCHAR* name = kplr_nameForMqttDev( globalState->dutil, env, devid );
    result = (*env)->NewStringUTF( env, name );
    (*env)->ReleaseStringUTFChars( env, jDevID, devid );
    DVC_HEADER_END();
    return result;
}
#endif

JNIEXPORT jbyteArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_gi_1to_1stream
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jobject jgi )
{
    jbyteArray result;
    JNIGlobalState* globalState = (JNIGlobalState*)jniGlobalPtr;
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = GETMPOOL( globalState );
#endif
    CurGameInfo* gi = makeGI( MPPARM(mpool) env, jgi );
    XWStreamCtxt* stream = mem_stream_make( MPPARM(mpool) globalState->vtMgr,
                                            NULL, 0, NULL );

    game_saveToStream( NULL, env, gi, stream, 0 );
    destroyGI( MPPARM(mpool) &gi );

    result = streamToBArray( env, stream );
    stream_destroy( stream, env );
    releaseMPool( globalState );
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_gi_1from_1stream
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jobject jgi, jbyteArray jstream )
{
    JNIGlobalState* globalState = (JNIGlobalState*)jniGlobalPtr;
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = GETMPOOL( globalState );
#endif
    XWStreamCtxt* stream = streamFromJStream( MPPARM(mpool) env,
                                              globalState->vtMgr, jstream );

    CurGameInfo gi = {0};
    // XP_MEMSET( &gi, 0, sizeof(gi) );
    if ( game_makeFromStream( MPPARM(mpool) env, stream, NULL,
                              &gi, NULL, NULL, NULL, NULL, NULL, NULL ) ) {
        setJGI( env, jgi, &gi );
    } else {
        XP_LOGF( "%s: game_makeFromStream failed", __func__ );
    }

    gi_disposePlayerInfo( MPPARM(mpool) &gi );

    stream_destroy( stream, env );
    releaseMPool( globalState );
}

JNIEXPORT jbyteArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_nli_1to_1stream
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jobject jnli )
{
    LOG_FUNC();
    JNIGlobalState* globalState = (JNIGlobalState*)jniGlobalPtr;
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = GETMPOOL( globalState );
#endif

    jbyteArray result;
    NetLaunchInfo nli = {0};
    loadNLI( env, &nli, jnli );
    XWStreamCtxt* stream = mem_stream_make( MPPARM(mpool) globalState->vtMgr,
                                            NULL, 0, NULL );

    nli_saveToStream( &nli, stream );

    result = streamToBArray( env, stream );
    stream_destroy( stream, env );
    releaseMPool( globalState );
    return result;
}

JNIEXPORT jobject JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_nli_1from_1stream
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jbyteArray jstream )
{
    jobject jnli = NULL;
    LOG_FUNC();
    JNIGlobalState* globalState = (JNIGlobalState*)jniGlobalPtr;
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = GETMPOOL( globalState );
#endif
    XWStreamCtxt* stream = streamFromJStream( MPPARM(mpool) env,
                                              globalState->vtMgr, jstream );

    NetLaunchInfo nli = {0};
    if ( nli_makeFromStream( &nli, stream ) ) {
        jnli = makeObjectEmptyConst( env, PKG_PATH("NetLaunchInfo") );
        setNLI( env, jnli, &nli );
    } else {
        XP_LOGF( "%s: game_makeFromStream failed", __func__ );
    }

    stream_destroy( stream, env );
    releaseMPool( globalState );
    return jnli;
}

JNIEXPORT jobject JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1getInitialAddr
( JNIEnv* env, jclass C, jstring jname, jint port )
{
    CommsAddrRec addr;

    const char* chars = (*env)->GetStringUTFChars( env, jname, NULL );
    comms_getInitialAddr( &addr, chars, port );
    (*env)->ReleaseStringUTFChars( env, jname, chars );
    jobject jaddr = makeObjectEmptyConst( env, PKG_PATH("jni/CommsAddrRec") );
    setJAddrRec( env, jaddr, &addr );
    return jaddr;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1getUUID
( JNIEnv* env, jclass C )
{
    jstring jstr = 
#ifdef XWFEATURE_BLUETOOTH
        (*env)->NewStringUTF( env, XW_BT_UUID )
#else
        NULL
#endif
        ;
    return jstr;
}

JNIEXPORT jlong JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1make
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jbyteArray jDictBytes,
  jstring jname, jstring jpath )
{
    JNIGlobalState* globalState = (JNIGlobalState*)jniGlobalPtr;
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = GETMPOOL( globalState );
#endif

    /* makeDict calls dict_ref() */
    DictionaryCtxt* dictPtr = makeDict( MPPARM(mpool) env, TI_IF(&globalState->ti)
                                        globalState->dictMgr,globalState->jniutil,
                                        jname, jDictBytes, jpath, NULL, false );
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
    jstring result = NULL;
    JNIGlobalState* globalState = (JNIGlobalState*)jniGlobalPtr;
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = GETMPOOL( globalState );
#endif
    DictionaryCtxt* dict = (DictionaryCtxt*)dictPtr;
    XWStreamCtxt* stream = mem_stream_make( MPPARM(mpool) globalState->vtMgr,
                                            NULL, 0, NULL );
    dict_writeTilesInfo( dict, stream );
    result = streamToJString( env, stream );
    stream_destroy( stream, env );

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
    jobject jinfo = NULL;
#ifdef MAP_THREAD_TO_ENV
    JNIGlobalState* globalState = (JNIGlobalState*)jniGlobalPtr;
    MAP_THREAD( &globalState->ti, env );
#endif
    DictionaryCtxt* dict = (DictionaryCtxt*)dictPtr;
    if ( NULL != dict ) {
        jinfo = makeObjectEmptyConst( env, PKG_PATH("jni/DictInfo") );
        XP_LangCode code = dict_getLangCode( dict );
        setInt( env, jinfo, "langCode", code );
        setInt( env, jinfo, "wordCount", dict_getWordCount( dict, env ) );
        setString( env, jinfo, "md5Sum", dict_getMd5Sum( dict ) );
    }

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
        setObject( env, jmsg, "data", "[B", arr );
        deleteLocalRef( env, arr );
        
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
    JNIGlobalState* globalState = (JNIGlobalState*)jniGlobalPtr;
    MAP_THREAD( &globalState->ti, env );

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

    return result;
}

JNIEXPORT jobjectArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_smsproto_1prepInbound
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jbyteArray jData,
  jstring jFromPhone, jint jWantPort )
{
    jobjectArray result = NULL;

    if ( !!jData ) {
        JNIGlobalState* globalState = (JNIGlobalState*)jniGlobalPtr;
        MAP_THREAD( &globalState->ti, env );

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
        XP_LOGF( "%s() => null (null input)", __func__ );
    }
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
#define XWJNI_START() {                                     \
    JNIState* state = getState( env, gamePtr, __func__ );   \
    XP_ASSERT( state->guard == GAME_GUARD );          \
    MPSLOT;                                                 \
    MPASSIGN( mpool, state->mpool );                        \
    XP_ASSERT( !!state->globalJNI );                        \
    MAP_THREAD( &state->globalJNI->ti, env );               \

#define XWJNI_START_GLOBALS()                           \
    XWJNI_START()                                       \
    AndGameGlobals* globals = &state->globals;          \
    XP_USE(globals); /*no warnings */                   \

#define XWJNI_END()                                   \
    }                                                 \

JNIEXPORT jlong JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_gameJNIInit
( JNIEnv* env, jclass C, jlong jniGlobalPtr )
{
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = ((JNIGlobalState*)jniGlobalPtr)->mpool;
#endif
    JNIState* state = (JNIState*)XP_CALLOC( mpool, sizeof(*state) );
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
  jobjectArray j_names, jobjectArray j_dicts, jobjectArray j_paths,
  jstring j_lang, jobject j_util, jobject j_draw, jobject j_cp,
  jobject j_procs )
{
    XWJNI_START_GLOBALS();
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
    globals->xportProcs = makeXportProcs( MPPARM(mpool) env,
                                          TI_IF(&state->globalJNI->ti)
                                          j_procs );
    CommonPrefs cp = {0};
    loadCommonPrefs( env, &cp, j_cp );

    game_makeNewGame( MPPARM(mpool) env, &state->game, gi,
                      globals->util, dctx, &cp, globals->xportProcs );

    DictionaryCtxt* dict;
    PlayerDicts dicts;

    makeDicts( MPPARM(state->globalJNI->mpool) env,
               TI_IF(&state->globalJNI->ti)
               state->globalJNI->dictMgr,
               globals->jniutil, &dict, &dicts, j_names, j_dicts, 
               j_paths, j_lang );
#ifdef STUBBED_DICT
    if ( !dict ) {
        XP_LOGF( "falling back to stubbed dict" );
        dict = make_stubbed_dict( MPPARM_NOCOMMA(mpool) );
    }
#endif
    model_setDictionary( state->game.model, env, dict );
    dict_unref( dict, env );         /* game owns it now */
    model_setPlayerDicts( state->game.model, env, &dicts );
    dict_unref_all( &dicts, env );
    XWJNI_END();
} /* makeNewGame */

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
  jobjectArray jdictNames, jobjectArray jdicts, jobjectArray jpaths,
  jstring jlang, jobject jutil, jobject jdraw, jobject jcp, jobject jprocs )
{
    jboolean result;
    DictionaryCtxt* dict;
    PlayerDicts dicts;
    XWJNI_START_GLOBALS();

    globals->gi = (CurGameInfo*)XP_CALLOC( mpool, sizeof(*globals->gi) );
    globals->util = makeUtil( MPPARM(mpool) env,
                              TI_IF(&state->globalJNI->ti)
                              jutil, globals->gi, globals);
    globals->jniutil = state->globalJNI->jniutil;
    makeDicts( MPPARM(state->globalJNI->mpool) env,
               TI_IF(&state->globalJNI->ti)
               state->globalJNI->dictMgr,
               globals->jniutil, &dict, &dicts, jdictNames, jdicts, jpaths, 
               jlang );
    if ( !!jdraw ) {
        globals->dctx = makeDraw( MPPARM(mpool) env,
                                  TI_IF(&state->globalJNI->ti)
                                  jdraw );
    }
    globals->xportProcs = makeXportProcs( MPPARM(mpool) env,
                                          TI_IF(&state->globalJNI->ti)
                                          jprocs );

    XWStreamCtxt* stream = streamFromJStream( MPPARM(mpool) env, 
                                              globals->vtMgr, jstream );

    CommonPrefs cp;
    loadCommonPrefs( env, &cp, jcp );
    result = game_makeFromStream( MPPARM(mpool) env, stream, &state->game,
                                  globals->gi, dict, &dicts,
                                  globals->util, globals->dctx, &cp,
                                  globals->xportProcs );
    stream_destroy( stream, env );
    dict_unref( dict, env );         /* game owns it now */
    dict_unref_all( &dicts, env );

    /* If game_makeFromStream() fails, the platform-side caller still needs to
       call game_dispose. That requirement's better than having cleanup code
       in two places. */
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
    XWJNI_START_GLOBALS();

    /* Use our copy of gi if none's provided.  That's because only the caller
       knows if its gi should win -- user has changed game config -- or if
       ours should -- changes like remote players being added. */
    CurGameInfo* gi = 
        (NULL == jgi) ? globals->gi : makeGI( MPPARM(mpool) env, jgi );
    XWStreamCtxt* stream = mem_stream_make_sized( MPPARM(mpool) globals->vtMgr,
                                                  state->lastSavedSize,
                                                  NULL, 0, NULL );

    game_saveToStream( &state->game, env, gi, stream, ++state->curSaveCount );

    if ( NULL != jgi ) {
        destroyGI( MPPARM(mpool) &gi );
    }

    state->lastSavedSize = stream_getSize( stream );
    result = streamToBArray( env, stream );
    stream_destroy( stream, env );

    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1saveSucceeded
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    XWJNI_START();
    game_saveSucceeded( &state->game, env, state->curSaveCount );
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setDraw
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jdraw )
{
    XWJNI_START_GLOBALS();

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
    XWJNI_START();
    board_invalAll( state->game.board );
    XWJNI_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1draw
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = board_draw( state->game.board, env );
    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1drawSnapshot
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jdraw, jint width,
  jint height )
{
    XWJNI_START();
    DrawCtx* newDraw = makeDraw( MPPARM(mpool) env, TI_IF(&state->globalJNI->ti) jdraw );
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
    XWJNI_START();
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
    XWJNI_START();
    BoardDims dims;
    dimsJToC( env, &dims, jdims );
    board_applyLayout( state->game.board, env, &dims );
    XWJNI_END();
}

#else

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setPos
(JNIEnv* env, jclass C, GamePtrType gamePtr, jint left, jint top, jint width,
 jint height, jint maxCellSize, jboolean lefty )
{
    XWJNI_START();
    board_setPos( state->game.board, left, top, width, height, maxCellSize, 
                  lefty );
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setScoreboardLoc
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint left, jint top,
  jint width, jint height, jboolean divideHorizontally )
{
    XWJNI_START();
    board_setScoreboardLoc( state->game.board, left, top, width, 
                            height, divideHorizontally );
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setTrayLoc
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint left, jint top,
  jint width, jint height, jint minDividerWidth )
{
    XWJNI_START();
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
    XWJNI_START();
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
    XWJNI_START();
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
    XWJNI_START();
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
    XWJNI_START();
    result = board_handlePenMove( state->game.board, env, xx, yy );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1handlePenUp
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint xx, jint yy )
{
    jboolean result;
    XWJNI_START();
    result = board_handlePenUp( state->game.board, env, xx, yy );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1containsPt
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint xx, jint yy )
{
    jboolean result;
    XWJNI_START();
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
    XWJNI_START();
    result = board_juggleTray( state->game.board, env );
    XWJNI_END();
    return result;
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1getTrayVisState
(JNIEnv* env, jclass C, GamePtrType gamePtr)
{
    jboolean result;
    XWJNI_START();
    result = board_getTrayVisState( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1getSelPlayer
(JNIEnv* env, jclass C, GamePtrType gamePtr)
{
    jint result;
    XWJNI_START();
    result = board_getSelPlayer( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1passwordProvided
(JNIEnv* env, jclass C, GamePtrType gamePtr, jint player, jstring jpasswd )
{
    jboolean result;
    XWJNI_START();
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
    XWJNI_START();
    result = board_hideTray( state->game.board, env);
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1showTray
(JNIEnv* env, jclass C, GamePtrType gamePtr)
{
    jboolean result;
    XWJNI_START();
    result = board_showTray( state->game.board, env );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1beginTrade
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = board_beginTrade( state->game.board, env );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1endTrade
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START();
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
    XWJNI_START();
    result = board_setBlankValue( state->game.board, player, col, row, tile );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1toggle_1showValues
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = board_toggle_showValues( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1commitTurn
( JNIEnv* env, jclass C, GamePtrType gamePtr, jboolean phoniesConfirmed,
  jboolean turnConfirmed, jintArray jNewTiles )
{
    jboolean result;
    XWJNI_START();
    TrayTileSet* newTilesP = NULL;
    TrayTileSet newTiles;

    if ( jNewTiles != NULL ) {
        tilesArrayToTileSet( env, jNewTiles, &newTiles );
        newTilesP = &newTiles;
    }

    result = board_commitTurn( state->game.board, env, phoniesConfirmed,
                               turnConfirmed, newTilesP );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1flip
(JNIEnv* env, jclass C, GamePtrType gamePtr)
{
    jboolean result;
    XWJNI_START();
    result = board_flip( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1replaceTiles
(JNIEnv* env, jclass C, GamePtrType gamePtr)
{
    jboolean result;
    XWJNI_START();
    result = board_replaceTiles( state->game.board, env );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL 
Java_org_eehouse_android_xw4_jni_XwJNI_board_1redoReplacedTiles
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = board_redoReplacedTiles( state->game.board, env );
    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1reset
(JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    XWJNI_START();
    server_reset( state->game.server, env, state->game.comms );
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1handleUndo
(JNIEnv* env, jclass C, GamePtrType gamePtr)
{
    XWJNI_START();
    server_handleUndo( state->game.server, env, 0 );
    XWJNI_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1do
(JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START();
    XP_ASSERT( !!state->game.server );
    result = server_do( state->game.server, env );
    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1tilesPicked
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint player, jintArray jNewTiles )
{
    XWJNI_START();
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
    XWJNI_START();
    result = server_countTilesInPool( state->game.server );
    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1resetEngine
(JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    XWJNI_START();
    board_resetEngine( state->game.board );
    XWJNI_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1requestHint
( JNIEnv* env, jclass C, GamePtrType gamePtr, jboolean useLimits,
  jboolean goBack, jbooleanArray workRemains )
{
    jboolean result;
    XWJNI_START();
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
    XWJNI_START_GLOBALS();
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
    XWJNI_START_GLOBALS();
    XWStreamCtxt* stream = mem_stream_make( MPPARM(mpool) globals->vtMgr,
                                            NULL, 0, NULL );
    board_formatRemainingTiles( state->game.board, env, stream );
    result = streamToJString( env, stream );
    stream_destroy( stream, env );

    XWJNI_END();
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1formatDictCounts
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint nCols )
{
    jstring result;
    XWJNI_START_GLOBALS();
    XWStreamCtxt* stream = and_empty_stream( MPPARM(mpool) globals );
    server_formatDictCounts( state->game.server, env, stream, nCols, XP_FALSE );
    result = streamToJString( env, stream );
    stream_destroy( stream, env );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1getGameIsOver
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = server_getGameIsOver( state->game.server );
    XWJNI_END();
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_model_1writeGameHistory
( JNIEnv* env, jclass C, GamePtrType gamePtr, jboolean gameOver )
{
    jstring result;
    XWJNI_START_GLOBALS();
    XWStreamCtxt* stream = and_empty_stream( MPPARM(mpool) globals );
    model_writeGameHistory( state->game.model, env, stream,
                            state->game.server, gameOver );
    result = streamToJString( env, stream );
    stream_destroy( stream, env );
    XWJNI_END();
    return result;
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_model_1getNMoves
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jint result;
    XWJNI_START();
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
    XWJNI_START();
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
    XWJNI_START();
    XP_ASSERT( !!state->game.model );
    LastMoveInfo lmi;
    XP_Bool valid = model_getPlayersLastScore( state->game.model, env,
                                               player, &lmi );

    jlmi = makeObjectEmptyConst( env, PKG_PATH("jni/LastMoveInfo") );
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
    XWJNI_START_GLOBALS();
    XWStreamCtxt* stream = and_empty_stream( MPPARM(mpool) globals );
    server_writeFinalScores( state->game.server, env, stream );
    result = streamToJString( env, stream );
    stream_destroy( stream, env );
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
    XWJNI_START_GLOBALS();
    result = server_initClientConnection( state->game.server, env );
    XWJNI_END();
    LOG_RETURNF( "%s", boolToStr(result) );
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1start
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    XWJNI_START();
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
    XWJNI_START();
    CommsCtxt* comms = state->game.comms;
    if ( !!comms ) {
        comms_stop( comms, env );
    }
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1resetSame
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    XWJNI_START();
    if ( !!state->game.comms ) {
        comms_resetSame( state->game.comms, env );
    }
    XWJNI_END();
}

JNIEXPORT jobject JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1getAddr
(JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jobject jaddr;
    XWJNI_START();
    XP_ASSERT( state->game.comms );
    CommsAddrRec addr;
    comms_getAddr( state->game.comms, &addr );
    jaddr = makeObjectEmptyConst( env, PKG_PATH("jni/CommsAddrRec") );
    setJAddrRec( env, jaddr, &addr );
    XWJNI_END();
    return jaddr;
}

JNIEXPORT jobjectArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1getAddrs
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jobjectArray result = NULL;
    XWJNI_START();
    XP_ASSERT( state->game.comms );
    if ( !!state->game.comms ) {
        CommsAddrRec addrs[MAX_NUM_PLAYERS];
        XP_U16 count = VSIZE(addrs);
        comms_getAddrs( state->game.comms, env, addrs, &count );

        jclass clas = (*env)->FindClass( env, PKG_PATH("jni/CommsAddrRec") );
        result = (*env)->NewObjectArray( env, count, clas, NULL );

        for ( int ii = 0; ii < count; ++ii ) {
            jobject jaddr = makeJAddr( env, &addrs[ii] );
            (*env)->SetObjectArrayElement( env, result, ii, jaddr );
            deleteLocalRef( env, jaddr );
        }
        deleteLocalRef( env, clas );
    }
    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1augmentHostAddr
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jaddr )
{
    XWJNI_START();
    if ( state->game.comms ) {
        CommsAddrRec addr = {0};
        getJAddrRec( env, &addr, jaddr );
        comms_augmentHostAddr( state->game.comms, env, &addr );
    } else {
        XP_LOGF( "%s: no comms this game", __func__ );
    }
    XWJNI_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1receiveMessage
( JNIEnv* env, jclass C, GamePtrType gamePtr, jbyteArray jstream, jobject jaddr )
{
    jboolean result;
    XWJNI_START_GLOBALS();

    XWStreamCtxt* stream = streamFromJStream( MPPARM(mpool) env, globals->vtMgr,
                                              jstream );
    CommsAddrRec* addrp = NULL;
    CommsAddrRec addr = {0};
    XP_ASSERT( !!jaddr );
    if ( NULL != jaddr ) {
        getJAddrRec( env, &addr, jaddr );
        addrp = &addr;
    }

    result = game_receiveMessage( &state->game, env, stream, addrp );

    stream_destroy( stream, env );

    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1summarize
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jsummary )
{
    XWJNI_START();
    ModelCtxt* model = state->game.model;
    ServerCtxt* server = state->game.server;
    XP_S16 nMoves = model_getNMoves( model );
    setInt( env, jsummary, "nMoves", nMoves );
    XP_Bool gameOver = server_getGameIsOver( server );
    setBool( env, jsummary, "gameOver", gameOver );
    XP_Bool isLocal = XP_FALSE;
    setInt( env, jsummary, "turn", 
            server_getCurrentTurn( server, &isLocal ) );
    setBool( env, jsummary, "turnIsLocal", isLocal );
    setInt( env, jsummary, "lastMoveTime", 
            server_getLastMoveTime(server) );
    setInt( env, jsummary, "dupTimerExpires",
            server_getDupTimerExpires(server) );
    
    if ( !!state->game.comms ) {
        CommsCtxt* comms = state->game.comms;
        setInt( env, jsummary, "seed", comms_getChannelSeed( comms ) );
        setInt( env, jsummary, "missingPlayers", 
                server_getMissingPlayers( server ) );
        setInt( env, jsummary, "nPacketsPending", 
                comms_countPendingPackets( state->game.comms ) );

        CommsAddrRec addr;
        comms_getAddr( comms, &addr );
        setTypeSetFieldIn( env, &addr, jsummary, "conTypes" );

        CommsConnType typ;
        for ( XP_U32 st = 0; addr_iter( &addr, &typ, &st ); ) {
            switch( typ ) {
            case COMMS_CONN_RELAY: {
                XP_UCHAR buf[128];
                XP_U16 len = VSIZE(buf);
                if ( comms_getRelayID( comms, buf, &len ) ) {
                    XP_ASSERT( '\0' == buf[len-1] ); /* failed! */
                    setString( env, jsummary, "relayID", buf );
                }
                setString( env, jsummary, "roomName", addr.u.ip_relay.invite );
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
                comms_getAddrs( comms, env, addrs, &count );
            
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

    XP_U16 nPlayers = model_getNPlayers( model );
    jint jvals[nPlayers];
    if ( gameOver ) {
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
    XWJNI_START();

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
    XWJNI_START();
    result = board_getFocusOwner( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1focusChanged
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint typ )
{
    jboolean result;
    XWJNI_START();
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
    XWJNI_START();

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
    XWJNI_START_GLOBALS();
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
    XWJNI_START();
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
    XWJNI_START();
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
    XWJNI_START_GLOBALS();
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
    XWJNI_START();
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

typedef struct _GotOneClosure {
    JNIEnv* env;
    jbyteArray msgs[16];
    int count;
} GotOneClosure;

static void
onGotOne( void* closure, XWEnv xwe, XP_U8* msg, XP_U16 len, MsgID XP_UNUSED(msgID) )
{
    GotOneClosure* goc = (GotOneClosure*)closure;
    XP_ASSERT( goc->env == xwe );
    if ( goc->count < VSIZE(goc->msgs) ) {
        jbyteArray arr = makeByteArray( xwe, len, (const jbyte*)msg );
        goc->msgs[goc->count++] = arr;
    } else {
        XP_ASSERT( 0 );
    }
}

JNIEXPORT jobjectArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1getPending
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jobjectArray result = NULL;
    XWJNI_START();
    GotOneClosure goc = { .env = env, .count = 0 };
    XP_ASSERT( !!state->game.comms );
    comms_getPending( state->game.comms, env, onGotOne, &goc );

    result = makeByteArrayArray( env, goc.count );
    for ( int ii = 0; ii < goc.count; ++ii ) {
        (*env)->SetObjectArrayElement( env, result, ii, goc.msgs[ii] );
        deleteLocalRef( env, goc.msgs[ii] );
    }

    XWJNI_END();
    return result;
}

#ifdef XWFEATURE_COMMSACK
JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1ackAny
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    XWJNI_START();
    XP_ASSERT( !!state->game.comms );
    (void)comms_ackAny( state->game.comms, env );
    XWJNI_END();
}
#endif

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1transportFailed
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject failedTyp )
{
    XWJNI_START();
    XP_ASSERT( !!state->game.comms );

    CommsConnType typ = jEnumToInt( env, failedTyp );
    (void)comms_transportFailed( state->game.comms, env, typ );
    XWJNI_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1isConnected
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = NULL != state->game.comms && comms_isConnected( state->game.comms );
    XWJNI_END();
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1formatRelayID
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint indx )
{
    jstring result = NULL;
    XWJNI_START();

    XP_UCHAR buf[64];
    XP_U16 len = sizeof(buf);
    if ( comms_formatRelayID( state->game.comms, indx, buf, &len ) ) {
        XP_ASSERT( len < sizeof(buf) );
        LOG_RETURNF( "%s", buf );
        result = (*env)->NewStringUTF( env, buf );
    }

    XWJNI_END();
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1getStats
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jstring result = NULL;
#ifdef DEBUG
    XWJNI_START_GLOBALS();
    if ( NULL != state->game.comms ) {
        XWStreamCtxt* stream = mem_stream_make( MPPARM(mpool) globals->vtMgr,
                                                NULL, 0, NULL );
        comms_getStats( state->game.comms, stream );
        result = streamToJString( env, stream );
        stream_destroy( stream, env );
    }
    XWJNI_END();
#endif
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1dropHostAddr
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jConnTyp )
{
    LOG_FUNC();
    XWJNI_START();
    if ( NULL != state->game.comms ) {
        CommsConnType connType = jEnumToInt( env, jConnTyp );
        comms_dropHostAddr( state->game.comms, connType );
    }
    XWJNI_END();
    LOG_RETURN_VOID();
}

#ifdef DEBUG
JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1setAddrDisabled
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jConnTyp,
  jboolean forSend, jboolean val )
{
    XWJNI_START();
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
    XWJNI_START();
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
    XWJNI_START();
    XP_ASSERT( !!state->game.server );
    server_endGame( state->game.server, env );
    XWJNI_END();
}

JNIEXPORT void JNICALL Java_org_eehouse_android_xw4_jni_XwJNI_board_1pause
( JNIEnv* env, jclass C, GamePtrType gamePtr, jstring jmsg )
{
    XWJNI_START();
    XP_ASSERT( !!state->game.board );

    const char* msg = (*env)->GetStringUTFChars( env, jmsg, NULL );
    board_pause( state->game.board, env, msg );
    (*env)->ReleaseStringUTFChars( env, jmsg, msg );

    XWJNI_END();
}

JNIEXPORT void JNICALL Java_org_eehouse_android_xw4_jni_XwJNI_board_1unpause
( JNIEnv* env, jclass C, GamePtrType gamePtr, jstring jmsg )
{
    XWJNI_START();
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
    XWJNI_START();
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
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jlong dictPtr,
  jobjectArray jPatsArr, jint minLen, jint maxLen )
{
    jlong closure = 0;
    JNIGlobalState* globalState = (JNIGlobalState*)jniGlobalPtr;
    MAP_THREAD( &globalState->ti, env );

    DictionaryCtxt* dict = (DictionaryCtxt*)dictPtr;
    if ( !!dict ) {
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
                    jbyteArray jtiles;
                    if ( getObject( env, jdesc, "tilePat", "[B", &jtiles ) ) {
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
                        deleteLocalRef( env, jtiles );
                    }
                    deleteLocalRef( env, jdesc );
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
    jintArray result = NULL;
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
    if ( NULL != data ) {
        if ( di_getNthWord( data->iter, env, jposn, data->depth, &data->idata ) ) {
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
