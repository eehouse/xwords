/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright © 2009 - 2014 by Eric House (xwords@eehouse.org).  All rights
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
#include "dictnry.h"
#include "dictiter.h"
#include "dictmgr.h"

#include "utilwrapper.h"
#include "drawwrapper.h"
#include "xportwrapper.h"
#include "anddict.h"
#include "andutils.h"
#include "andglobals.h"
#include "jniutlswrapper.h"
#include "paths.h"

typedef struct _EnvThreadEntry {
    JNIEnv* env;
    pthread_t owner;
} EnvThreadEntry;

#define MAX_ENV_THREADS 10

struct _EnvThreadInfo {
    pthread_mutex_t mtxThreads;
    EnvThreadEntry entries[MAX_ENV_THREADS];
};

/* Globals for the whole game */
typedef struct _JNIGlobalState {
    EnvThreadInfo ti;
    DictMgrCtxt* dictMgr;
    MPSLOT
} JNIGlobalState;

static void
map_thread( EnvThreadInfo* ti, JNIEnv* env )
{
    pthread_t self = pthread_self();

    pthread_mutex_lock( &ti->mtxThreads );

    XP_Bool found = false;
    EnvThreadEntry* firstEmpty = NULL;
    for ( int ii = 0; !found && ii < VSIZE(ti->entries); ++ii ) {
        EnvThreadEntry* entry = &ti->entries[ii];
        if ( 0 == entry->owner ) {
            if ( NULL == firstEmpty ) {
                firstEmpty = entry;
            }
        } else if ( self == entry->owner ) {
            found = true;
            if ( env != entry->env ) {
                /* this DOES happen!!! */
                XP_LOGF( "%s (ti=%p): replacing env %p with env %p for thread %x",
                         __func__, ti, entry->env, env, (int)self );
                entry->env = env;
            }
        }
    }

    if ( !found ) {
        XP_ASSERT( !!firstEmpty );
        firstEmpty->owner = self;
        firstEmpty->env = env;
        int indx = firstEmpty - ti->entries;
        XP_LOGF( "%s: entry %d: mapped env %p to thread %x", __func__, indx,
                 env, (int)self );
    }

    pthread_mutex_unlock( &ti->mtxThreads );
}

static void
map_init( EnvThreadInfo* ti, JNIEnv* env )
{
    pthread_mutex_init( &ti->mtxThreads, NULL );
    map_thread( ti, env );
}

static void
map_remove( EnvThreadInfo* ti, JNIEnv* env )
{
    pthread_t self = pthread_self();
    XP_Bool found = false;
    for ( int ii = 0; !found && ii < VSIZE(ti->entries); ++ii ) {
        found = env == ti->entries[ii].env;
        if ( found ) {
            XP_LOGF( "%s: clearing out %dth entry (thread %x)", __func__, ii, 
                     (int)self );
            ti->entries[ii].env = NULL;
            XP_ASSERT( ti->entries[ii].owner = self );
            ti->entries[ii].owner = 0;
        }
    }
    XP_ASSERT( found );
}

static void
map_destroy( EnvThreadInfo* ti )
{
    pthread_mutex_destroy( &ti->mtxThreads );
}

JNIEnv*
envForMe( EnvThreadInfo* ti, const char* caller )
{
    JNIEnv* result = NULL;
    pthread_t self = pthread_self();
    pthread_mutex_lock( &ti->mtxThreads );
    for ( int ii = 0; ii < VSIZE(ti->entries); ++ii ) {
        if ( self == ti->entries[ii].owner ) {
            result = ti->entries[ii].env;
            break;
        }
    }
    pthread_mutex_unlock( &ti->mtxThreads );
    if( !result ) {
        XP_LOGF( "no env for %s (thread %x)", caller, (int)self );
        XP_ASSERT(0);
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_initGlobals
( JNIEnv* env, jclass C )
{
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = mpool_make();
#endif
    JNIGlobalState* state = (JNIGlobalState*)XP_CALLOC( mpool, sizeof(*state) );
    map_init( &state->ti, env );
    state->dictMgr = dmgr_make( MPPARM_NOCOMMA( mpool ) );
    MPASSIGN( state->mpool, mpool );
    LOG_RETURNF( "%p", state );
    return (jint)state;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_cleanGlobals
( JNIEnv* env, jclass C, jint ptr )
{
    LOG_FUNC();
    if ( 0 != ptr ) {
        JNIGlobalState* state = (JNIGlobalState*)ptr;
        XP_ASSERT( ENVFORME(&state->ti) == env );
        dmgr_destroy( state->dictMgr );
#ifdef MEM_DEBUG
        MemPoolCtx* mpool = state->mpool;
#endif
        map_destroy( &state->ti );
        XP_FREE( mpool, state );
        mpool_destroy( mpool );
    }
}

static const SetInfo gi_ints[] = {
    ARR_MEMBER( CurGameInfo, nPlayers )
    ,ARR_MEMBER( CurGameInfo, gameSeconds )
    ,ARR_MEMBER( CurGameInfo, boardSize )
    ,ARR_MEMBER( CurGameInfo, gameID )
    ,ARR_MEMBER( CurGameInfo, dictLang )
};

static const SetInfo gi_bools[] = {
    ARR_MEMBER( CurGameInfo, hintsNotAllowed )
    ,ARR_MEMBER( CurGameInfo, timerEnabled )
    ,ARR_MEMBER( CurGameInfo, allowPickTiles )
    ,ARR_MEMBER( CurGameInfo, allowHintRect )
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

    getInts( env, (void*)gi, jgi, gi_ints, VSIZE(gi_ints) );
    getBools( env, (void*)gi, jgi, gi_bools, VSIZE(gi_bools) );

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

    getString( env, jgi, "dictName", buf, VSIZE(buf) );
    gi->dictName = copyString( mpool, buf );

    XP_ASSERT( gi->nPlayers <= MAX_NUM_PLAYERS );

    jobject jplayers;
    if ( getObject( env, jgi, "players", "[L" PKG_PATH("jni/LocalPlayer") ";",
                    &jplayers ) ) {
        int ii;
        for ( ii = 0; ii < gi->nPlayers; ++ii ) {
            LocalPlayer* lp = &gi->players[ii];

            jobject jlp = (*env)->GetObjectArrayElement( env, jplayers, ii );
            XP_ASSERT( !!jlp );

            getInts( env, (void*)lp, jlp, pl_ints, VSIZE(pl_ints) );

            lp->isLocal = getBool( env, jlp, "isLocal" );

            getString( env, jlp, "name", buf, VSIZE(buf) );
            lp->name = copyString( mpool, buf );
            getString( env, jlp, "password", buf, VSIZE(buf) );
            lp->password = copyString( mpool, buf );
            getString( env, jlp, "dictName", buf, VSIZE(buf) );
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

    setInts( env, jgi, (void*)gi, gi_ints, VSIZE(gi_ints) );
    setBools( env, jgi, (void*)gi, gi_bools, VSIZE(gi_bools) );

    setString( env, jgi, "dictName", gi->dictName );

    intToJenumField( env, jgi, gi->phoniesAction, "phoniesAction",
                     PKG_PATH("jni/CurGameInfo$XWPhoniesChoice") );
    intToJenumField( env, jgi, gi->serverRole, "serverRole",
                     PKG_PATH("jni/CurGameInfo$DeviceRole") );

    jobject jplayers;
    if ( getObject( env, jgi, "players", 
                    "[L" PKG_PATH("jni/LocalPlayer") ";",
                    &jplayers ) ) {
        int ii;
        for ( ii = 0; ii < gi->nPlayers; ++ii ) {
            const LocalPlayer* lp = &gi->players[ii];

            jobject jlp = (*env)->GetObjectArrayElement( env, jplayers, ii );
            XP_ASSERT( !!jlp );

            setInts( env, jlp, (void*)lp, pl_ints, VSIZE(pl_ints) );
            
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
    getInts( env, (void*)out, jdims, bd_ints, VSIZE(bd_ints) );
}

static void
dimsCtoJ( JNIEnv* env, jobject jdims, const BoardDims* in )
{
    setInts( env, jdims, (void*)in, bd_ints, VSIZE(bd_ints) );
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
    int len = (*env)->GetArrayLength( env, jstream );
    XWStreamCtxt* stream = mem_stream_make_sized( MPPARM(mpool) vtMgr,
                                                  len, NULL, 0, NULL );
    jbyte* jelems = (*env)->GetByteArrayElements( env, jstream, NULL );
    stream_putBytes( stream, jelems, len );
    (*env)->ReleaseByteArrayElements( env, jstream, jelems, 0 );
    return stream;
} /* streamFromJStream */

/****************************************************
 * These three methods are stateless: no gamePtr
 ****************************************************/
JNIEXPORT jbyteArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_gi_1to_1stream
(JNIEnv* env, jclass C, jobject jgi )
{
    LOG_FUNC();
    jbyteArray result;
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = mpool_make();
#endif
    CurGameInfo* gi = makeGI( MPPARM(mpool) env, jgi );
    VTableMgr* vtMgr = make_vtablemgr( MPPARM_NOCOMMA(mpool) );
    XWStreamCtxt* stream = mem_stream_make( MPPARM(mpool) vtMgr,
                                            NULL, 0, NULL );

    game_saveToStream( NULL, gi, stream, 0 );
    destroyGI( MPPARM(mpool) &gi );

    result = streamToBArray( env, stream );
    stream_destroy( stream );

    vtmgr_destroy( MPPARM(mpool) vtMgr );
#ifdef MEM_DEBUG
    mpool_destroy( mpool );
#endif
    LOG_RETURN_VOID();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_gi_1from_1stream
( JNIEnv* env, jclass C, jobject jgi, jbyteArray jstream )
{
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = mpool_make();
#endif
    VTableMgr* vtMgr = make_vtablemgr( MPPARM_NOCOMMA(mpool) );

    XWStreamCtxt* stream = streamFromJStream( MPPARM(mpool) env, vtMgr, jstream );

    CurGameInfo gi;
    XP_MEMSET( &gi, 0, sizeof(gi) );
    if ( game_makeFromStream( MPPARM(mpool) stream, NULL, 
                              &gi, NULL, NULL, NULL, NULL, NULL, NULL ) ) {
        setJGI( env, jgi, &gi );
    } else {
        XP_LOGF( "%s: game_makeFromStream failed", __func__ );
    }

    gi_disposePlayerInfo( MPPARM(mpool) &gi );

    stream_destroy( stream );
    vtmgr_destroy( MPPARM(mpool) vtMgr );
#ifdef MEM_DEBUG
    mpool_destroy( mpool );
#endif
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1getInitialAddr
( JNIEnv* env, jclass C, jobject jaddr, jstring jname, jint port )
{
    CommsAddrRec addr;

    const char* chars = (*env)->GetStringUTFChars( env, jname, NULL );
    comms_getInitialAddr( &addr, chars, port );
    (*env)->ReleaseStringUTFChars( env, jname, chars );
    setJAddrRec( env, jaddr, &addr );
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1getUUID
( JNIEnv* env, jclass C )
{
    jstring jstr = NULL;
#ifdef XWFEATURE_BLUETOOTH
    const char* uuid = XW_BT_UUID;
    XP_LOGF( "uuid: %s", uuid );
    jstr = (*env)->NewStringUTF( env, uuid );
#endif
    return jstr;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1ref
( JNIEnv* env, jclass C, jint dictPtr )
{
    if ( 0 != dictPtr ) {
        DictionaryCtxt* dict = (DictionaryCtxt*)dictPtr;
        dict_ref( dict );
    }
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1unref
( JNIEnv* env, jclass C, jint dictPtr )
{
    if ( 0 != dictPtr ) {
        DictionaryCtxt* dict = (DictionaryCtxt*)dictPtr;
        dict_unref( dict );
    }
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1getInfo
( JNIEnv* env, jclass C, jint jniGlobalPtr, jbyteArray jDictBytes, 
  jstring jname, jstring jpath, jobject jniu, jboolean check, jobject jinfo )
{
    jboolean result = false;
    JNIGlobalState* state = (JNIGlobalState*)jniGlobalPtr;
    map_thread( &state->ti, env );
    JNIUtilCtxt* jniutil = makeJNIUtil( MPPARM(state->mpool) &state->ti, jniu );
    DictionaryCtxt* dict = makeDict( MPPARM(state->mpool) env, state->dictMgr, 
                                     jniutil, jname, jDictBytes, jpath, 
                                     NULL, check );
    if ( NULL != dict ) {
        if ( NULL != jinfo ) {
            XP_LangCode code = dict_getLangCode( dict );
            XP_ASSERT( 0 < code );
            setInt( env, jinfo, "langCode", code );
            setInt( env, jinfo, "wordCount", dict_getWordCount( dict ) );
            setString( env, jinfo, "md5Sum", dict_getMd5Sum( dict ) );
        }
        dict_unref( dict );
        result = true;
    }
    destroyJNIUtil( &jniutil );

    return result;
}

/* Dictionary methods: don't use gamePtr */
JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1tilesAreSame
( JNIEnv* env, jclass C, jint dictPtr1, jint dictPtr2 )
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
( JNIEnv* env, jclass C, jint dictPtr )
{
    jobject result = NULL;
    result = and_dictionary_getChars( env, (DictionaryCtxt*)dictPtr );
    return result;
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1getTileValue
( JNIEnv* env, jclass C, jint dictPtr, jint tile )
{
    return dict_getTileValue( (DictionaryCtxt*)dictPtr, tile );
}

struct _JNIState {
    XWGame game;
    JNIGlobalState* globalJNI;
    AndGlobals globals;
    XP_U16 curSaveCount;
    XP_U16 lastSavedSize;
#ifdef DEBUG
    const char* envSetterFunc;
#endif
    MPSLOT
};

#define XWJNI_START() {                                 \
    XP_ASSERT( 0 != gamePtr );                          \
    JNIState* state = (JNIState*)gamePtr;               \
    MPSLOT;                                             \
    MPASSIGN( mpool, state->mpool);                     \
    XP_ASSERT( !!state->globalJNI );                    \
    map_thread( &state->globalJNI->ti, env );           \

#define XWJNI_START_GLOBALS()                           \
    XWJNI_START()                                       \
    AndGlobals* globals = &state->globals;              \

#define XWJNI_END()                                   \
    }                                                 \

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_initJNI
( JNIEnv* env, jclass C, int jniGlobalPtr, jint seed )
{
    /* Why am I doing this twice? */
    /* struct timeval tv; */
    /* gettimeofday( &tv, NULL ); */
    /* srandom( tv.tv_sec ); */
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = mpool_make();
#endif
    JNIState* state = (JNIState*)XP_CALLOC( mpool, sizeof(*state) );
    state->globalJNI = (JNIGlobalState*)jniGlobalPtr;
    AndGlobals* globals = &state->globals;
    globals->state = (JNIState*)state;
    MPASSIGN( state->mpool, mpool );
    globals->vtMgr = make_vtablemgr(MPPARM_NOCOMMA(mpool));

    XP_LOGF( "%s: initing srand with %d", __func__, seed );
    srandom( seed );

    return (jint) state;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_envDone
( JNIEnv* env, jclass C, int jniGlobalPtr )
{
    JNIGlobalState* globalJNI = (JNIGlobalState*)jniGlobalPtr;
    map_remove( &globalJNI->ti, env );
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1makeNewGame
( JNIEnv* env, jclass C, jint gamePtr, jobject j_gi, jobject j_util, 
  jobject jniu, jobject j_draw, jobject j_cp, jobject j_procs, 
  jobjectArray j_names, jobjectArray j_dicts, jobjectArray j_paths,
  jstring j_lang )
{
    XWJNI_START_GLOBALS();
    EnvThreadInfo* ti = &state->globalJNI->ti;
    CurGameInfo* gi = makeGI( MPPARM(mpool) env, j_gi );
    globals->gi = gi;
    globals->util = makeUtil( MPPARM(mpool) ti, j_util, gi, 
                              globals );
    globals->jniutil = makeJNIUtil( MPPARM(mpool) ti, jniu );
    DrawCtx* dctx = NULL;
    if ( !!j_draw ) {
        dctx = makeDraw( MPPARM(mpool) ti, j_draw );
    }
    globals->dctx = dctx;
    globals->xportProcs = makeXportProcs( MPPARM(mpool) ti, j_procs );
    CommonPrefs cp;
    loadCommonPrefs( env, &cp, j_cp );

    XP_LOGF( "calling game_makeNewGame" );
    game_makeNewGame( MPPARM(mpool) &state->game, gi, globals->util, dctx, &cp,
                      globals->xportProcs );

    DictionaryCtxt* dict;
    PlayerDicts dicts;

    makeDicts( MPPARM(state->globalJNI->mpool) env, state->globalJNI->dictMgr, 
               globals->jniutil, &dict, &dicts, j_names, j_dicts, 
               j_paths, j_lang );
#ifdef STUBBED_DICT
    if ( !dict ) {
        XP_LOGF( "falling back to stubbed dict" );
        dict = make_stubbed_dict( MPPARM_NOCOMMA(mpool) );
    }
#endif
    model_setDictionary( state->game.model, dict );
    dict_unref( dict );         /* game owns it now */
    model_setPlayerDicts( state->game.model, &dicts );
    dict_unref_all( &dicts );
    XWJNI_END();
} /* makeNewGame */

JNIEXPORT void JNICALL Java_org_eehouse_android_xw4_jni_XwJNI_game_1dispose
( JNIEnv* env, jclass claz, jint gamePtr )
{
    JNIState* state = (JNIState*)gamePtr;
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = state->mpool;
#endif
    AndGlobals* globals = &state->globals;

    destroyGI( MPPARM(mpool) &globals->gi );

    game_dispose( &state->game );

    destroyDraw( &globals->dctx );
    destroyXportProcs( &globals->xportProcs );
    destroyUtil( &globals->util );
    destroyJNIUtil( &globals->jniutil );
    vtmgr_destroy( MPPARM(mpool) globals->vtMgr );

    map_remove( &state->globalJNI->ti, env );

    XP_FREE( mpool, state );
    mpool_destroy( mpool );
} /* game_dispose */

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1makeFromStream
( JNIEnv* env, jclass C, jint gamePtr, jbyteArray jstream, jobject /*out*/jgi,
  jobjectArray jdictNames, jobjectArray jdicts, jobjectArray jpaths,
  jstring jlang, jobject jutil, jobject jniu, jobject jdraw, jobject jcp,
  jobject jprocs )
{
    jboolean result;
    DictionaryCtxt* dict;
    PlayerDicts dicts;
    XWJNI_START_GLOBALS();
    EnvThreadInfo* ti = &state->globalJNI->ti;

    globals->gi = (CurGameInfo*)XP_CALLOC( mpool, sizeof(*globals->gi) );
    globals->util = makeUtil( MPPARM(mpool) ti, jutil, globals->gi, globals);
    globals->jniutil = makeJNIUtil( MPPARM(mpool) ti, jniu );
    makeDicts( MPPARM(state->globalJNI->mpool) env, state->globalJNI->dictMgr, 
               globals->jniutil, &dict, &dicts, jdictNames, jdicts, jpaths, 
               jlang );
    if ( !!jdraw ) {
        globals->dctx = makeDraw( MPPARM(mpool) ti, jdraw );
    }
    globals->xportProcs = makeXportProcs( MPPARM(mpool) ti, jprocs );

    XWStreamCtxt* stream = streamFromJStream( MPPARM(mpool) env, 
                                              globals->vtMgr, jstream );

    CommonPrefs cp;
    loadCommonPrefs( env, &cp, jcp );
    result = game_makeFromStream( MPPARM(mpool) stream, &state->game, 
                                  globals->gi, dict, &dicts,
                                  globals->util, globals->dctx, &cp,
                                  globals->xportProcs );
    stream_destroy( stream );
    dict_unref( dict );         /* game owns it now */
    dict_unref_all( &dicts );

    if ( result ) {
        XP_ASSERT( 0 != globals->gi->gameID );
        if ( !!jgi ) {
            setJGI( env, jgi, globals->gi );
        }
    } else {
        destroyDraw( &globals->dctx );
        destroyXportProcs( &globals->xportProcs );
        destroyUtil( &globals->util );
        destroyJNIUtil( &globals->jniutil );
        destroyGI( MPPARM(mpool) &globals->gi );
    }

    XWJNI_END();
    return result;
} /* makeFromStream */

JNIEXPORT jbyteArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1saveToStream
( JNIEnv* env, jclass C, jint gamePtr, jobject jgi )
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
( JNIEnv* env, jclass C, jint gamePtr )
{
    XWJNI_START();
    game_saveSucceeded( &state->game, state->curSaveCount );
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setDraw
( JNIEnv* env, jclass C, jint gamePtr, jobject jdraw )
{
    XWJNI_START_GLOBALS();

    DrawCtx* newDraw = makeDraw( MPPARM(mpool) &state->globalJNI->ti, jdraw );
    board_setDraw( state->game.board, newDraw );

    destroyDraw( &globals->dctx );
    globals->dctx = newDraw;

    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1invalAll
( JNIEnv *env, jclass C, jint gamePtr )
{
    XWJNI_START();
    board_invalAll( state->game.board );
    XWJNI_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1draw
( JNIEnv *env, jclass C, jint gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = board_draw( state->game.board );
    XWJNI_END();
    return result;
}

#ifdef COMMON_LAYOUT
JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1figureLayout
( JNIEnv* env, jclass C, jint gamePtr, jobject jgi, jint left, jint top, 
  jint width, jint height, jint scorePct, jint trayPct, jint scoreWidth,
  jint fontWidth, jint fontHt, jboolean squareTiles, jobject jdims )
{
    XWJNI_START();
    CurGameInfo* gi = makeGI( MPPARM(mpool) env, jgi );

    BoardDims dims;
    board_figureLayout( state->game.board, gi, left, top, width, height, 
                        scorePct, trayPct, scoreWidth,
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
( JNIEnv* env, jclass C, jint gamePtr, jobject jdims )
{
    XWJNI_START();
    BoardDims dims;
    dimsJToC( env, &dims, jdims );
    board_applyLayout( state->game.board, &dims );
    XWJNI_END();
}

#else

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setPos
(JNIEnv *env, jclass C, jint gamePtr, jint left, jint top, jint width, 
 jint height, jint maxCellSize, jboolean lefty )
{
    XWJNI_START();
    board_setPos( state->game.board, left, top, width, height, maxCellSize, 
                  lefty );
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setScoreboardLoc
( JNIEnv *env, jclass C, jint gamePtr, jint left, jint top, 
  jint width, jint height, jboolean divideHorizontally )
{
    XWJNI_START();
    board_setScoreboardLoc( state->game.board, left, top, width, 
                            height, divideHorizontally );
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setTimerLoc
( JNIEnv* env, jclass C, jint gamePtr, jint timerLeft, jint timerTop,
  jint timerWidth, jint timerHeight )
{
    XWJNI_START();
    XP_LOGF( "%s(%d,%d,%d,%d)", __func__, timerLeft, timerTop,
             timerWidth, timerHeight );
    board_setTimerLoc( state->game.board, timerLeft, timerTop, 
                       timerWidth, timerHeight );
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setTrayLoc
( JNIEnv *env, jclass C, jint gamePtr, jint left, jint top, 
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
( JNIEnv* env, jclass C, jint gamePtr, jint zoomBy, jbooleanArray jCanZoom )
{
    jboolean result;
    XWJNI_START();
    XP_Bool canInOut[2];
    result = board_zoom( state->game.board, zoomBy, canInOut );
    jboolean canZoom[2] = { canInOut[0], canInOut[1] };
    setBoolArray( env, jCanZoom, VSIZE(canZoom), canZoom );
    XWJNI_END();
    return result;
}

#ifdef XWFEATURE_ACTIVERECT
JNIEXPORT jboolean JNICALL 
Java_org_eehouse_android_xw4_jni_XwJNI_board_1getActiveRect
( JNIEnv* env, jclass C, jint gamePtr, jobject jrect, jintArray dims )
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

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1handlePenDown
(JNIEnv *env, jclass C, jint gamePtr, jint xx, jint yy, jbooleanArray barray )
{
    jboolean result;
    XWJNI_START();
    XP_Bool bb;                 /* drop this for now */
    result = board_handlePenDown( state->game.board, xx, yy, &bb );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1handlePenMove
( JNIEnv *env, jclass C, jint gamePtr, jint xx, jint yy )
{
    jboolean result;
    XWJNI_START();
    result = board_handlePenMove( state->game.board, xx, yy );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1handlePenUp
( JNIEnv *env, jclass C, jint gamePtr, jint xx, jint yy )
{
    jboolean result;
    XWJNI_START();
    result = board_handlePenUp( state->game.board, xx, yy );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1juggleTray
(JNIEnv* env, jclass C, jint gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = board_juggleTray( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1getTrayVisState
(JNIEnv* env, jclass C, jint gamePtr)
{
    jboolean result;
    XWJNI_START();
    result = board_getTrayVisState( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1hideTray
(JNIEnv* env, jclass C, jint gamePtr)
{
    jboolean result;
    XWJNI_START();
    result = board_hideTray( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1showTray
(JNIEnv* env, jclass C, jint gamePtr)
{
    jboolean result;
    XWJNI_START();
    result = board_showTray( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1beginTrade
( JNIEnv* env, jclass C, jint gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = board_beginTrade( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1endTrade
( JNIEnv* env, jclass C, jint gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = board_endTrade( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1toggle_1showValues
( JNIEnv* env, jclass C, jint gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = board_toggle_showValues( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1commitTurn
(JNIEnv* env, jclass C, jint gamePtr)
{
    jboolean result;
    XWJNI_START();
    result = board_commitTurn( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1flip
(JNIEnv* env, jclass C, jint gamePtr)
{
    jboolean result;
    XWJNI_START();
    result = board_flip( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1replaceTiles
(JNIEnv* env, jclass C, jint gamePtr)
{
    jboolean result;
    XWJNI_START();
    result = board_replaceTiles( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL 
Java_org_eehouse_android_xw4_jni_XwJNI_board_1redoReplacedTiles
( JNIEnv* env, jclass C, jint gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = board_redoReplacedTiles( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1reset
(JNIEnv* env, jclass C, jint gamePtr )
{
    XWJNI_START();
    server_reset( state->game.server, state->game.comms );
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1handleUndo
(JNIEnv* env, jclass C, jint gamePtr)
{
    XWJNI_START();
    server_handleUndo( state->game.server, 0 );
    XWJNI_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1do
(JNIEnv* env, jclass C, jint gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = server_do( state->game.server );
    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1resetEngine
(JNIEnv* env, jclass C, jint gamePtr )
{
    XWJNI_START();
    board_resetEngine( state->game.board );
    XWJNI_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1requestHint
( JNIEnv* env, jclass C, jint gamePtr, jboolean useLimits, 
  jboolean goBack, jbooleanArray workRemains )
{
    jboolean result;
    XWJNI_START();
    XP_Bool tmpbool;
    result = board_requestHint( state->game.board, 
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
( JNIEnv* env, jclass C, jint gamePtr, jint why, jint when, jint handle )
{
    jboolean result;
    XWJNI_START_GLOBALS();
    XW_UtilCtxt* util = globals->util;
    result = utilTimerFired( util, why, handle );
    XWJNI_END();
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1formatRemainingTiles
(JNIEnv* env, jclass C, jint gamePtr )
{
    jstring result;
    XWJNI_START_GLOBALS();
    XWStreamCtxt* stream = mem_stream_make( MPPARM(mpool) globals->vtMgr,
                                            NULL, 0, NULL );
    board_formatRemainingTiles( state->game.board, stream );
    result = streamToJString( env, stream );
    stream_destroy( stream );

    XWJNI_END();
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1formatDictCounts
( JNIEnv* env, jclass C, jint gamePtr, jint nCols )
{
    jstring result;
    XWJNI_START_GLOBALS();
    XWStreamCtxt* stream = and_empty_stream( MPPARM(mpool) globals );
    server_formatDictCounts( state->game.server, stream, nCols );
    result = streamToJString( env, stream );
    stream_destroy( stream );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1getGameIsOver
( JNIEnv* env, jclass C, jint gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = server_getGameIsOver( state->game.server );
    XWJNI_END();
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_model_1writeGameHistory
( JNIEnv* env, jclass C, jint gamePtr, jboolean gameOver )
{
    jstring result;
    XWJNI_START_GLOBALS();
    XWStreamCtxt* stream = and_empty_stream( MPPARM(mpool) globals );
    model_writeGameHistory( state->game.model, stream, state->game.server,
                            gameOver );
    result = streamToJString( env, stream );
    stream_destroy( stream );
    XWJNI_END();
    return result;
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_model_1getNMoves
( JNIEnv* env, jclass C, jint gamePtr )
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
( JNIEnv* env, jclass C, jint gamePtr, jint player )
{
    jint result;
    XWJNI_START();
    XP_ASSERT( !!state->game.model );
    result = model_getNumTilesInTray( state->game.model, player );
    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_model_1getPlayersLastScore
( JNIEnv* env, jclass C, jint gamePtr, jint player, jobject jlmi )
{
    XWJNI_START();
    XP_ASSERT( !!state->game.model );
    LastMoveInfo lmi;
    XP_Bool valid = model_getPlayersLastScore( state->game.model, 
                                               player, &lmi );
    setBool( env, jlmi, "isValid", valid );
    if ( valid ) {
        setInt( env, jlmi, "score", lmi.score );
        setInt( env, jlmi, "nTiles", lmi.nTiles );
        setInt( env, jlmi, "moveType", lmi.moveType );
        setString( env, jlmi, "name", lmi.name );
        setString( env, jlmi, "word", lmi.word );
    }
    XWJNI_END();
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1writeFinalScores
( JNIEnv* env, jclass C, jint gamePtr )
{
    jstring result;
    XWJNI_START_GLOBALS();
    XWStreamCtxt* stream = and_empty_stream( MPPARM(mpool) globals );
    server_writeFinalScores( state->game.server, stream );
    result = streamToJString( env, stream );
    stream_destroy( stream );
    XWJNI_END();
    return result;
}

void
and_send_on_close( XWStreamCtxt* stream, void* closure )
{
    AndGlobals* globals = (AndGlobals*)closure;
    JNIState* state = (JNIState*)globals->state;

    XP_ASSERT( !!state->game.comms );
    comms_send( state->game.comms, stream );
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1initClientConnection
( JNIEnv* env, jclass C, jint gamePtr )
{
    jboolean result;
    LOG_FUNC();
    XWJNI_START_GLOBALS();
    XWStreamCtxt* stream = and_empty_stream( MPPARM(mpool) globals );
    stream_setOnCloseProc( stream, and_send_on_close );
    result = server_initClientConnection( state->game.server, stream );
    XWJNI_END();
    LOG_RETURNF( "%d", result );
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1start
( JNIEnv* env, jclass C, jint gamePtr )
{
    XWJNI_START();
    CommsCtxt* comms = state->game.comms;
    if ( !!comms ) {
        comms_start( comms );
    }
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1stop
( JNIEnv* env, jclass C, jint gamePtr )
{
    XWJNI_START();
    CommsCtxt* comms = state->game.comms;
    if ( !!comms ) {
        comms_stop( comms );
    }
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1resetSame
( JNIEnv* env, jclass C, jint gamePtr )
{
    XWJNI_START();
    if ( !!state->game.comms ) {
        comms_resetSame( state->game.comms );
    }
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1getAddr
(JNIEnv* env, jclass C, jint gamePtr, jobject jaddr )
{
    XWJNI_START();
    XP_ASSERT( state->game.comms );
    CommsAddrRec addr;
    comms_getAddr( state->game.comms, &addr );
    setJAddrRec( env, jaddr, &addr );
    XWJNI_END();
}

JNIEXPORT jobjectArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1getAddrs
( JNIEnv* env, jclass C, jint gamePtr )
{
    jobjectArray result = NULL;
    XP_U16 ii;
    XWJNI_START();
    XP_ASSERT( state->game.comms );
    CommsAddrRec addrs[MAX_NUM_PLAYERS];
    XP_U16 count = VSIZE(addrs);
    comms_getAddrs( state->game.comms, addrs, &count );

    jclass clas = (*env)->FindClass( env, PKG_PATH("jni/CommsAddrRec") );
    result = (*env)->NewObjectArray( env, count, clas, NULL );

    jmethodID initId = (*env)->GetMethodID( env, clas, "<init>", "()V" );
    for ( ii = 0; ii < count; ++ii ) {
        jobject jaddr = (*env)->NewObject( env, clas, initId );
        setJAddrRec( env, jaddr, &addrs[ii] );
        (*env)->SetObjectArrayElement( env, result, ii, jaddr );
        deleteLocalRef( env, jaddr );
    }
    deleteLocalRef( env, clas );

    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1setAddr
( JNIEnv* env, jclass C, jint gamePtr, jobject jaddr )
{
    XWJNI_START();
    if ( state->game.comms ) {
        CommsAddrRec addr = {0};
        getJAddrRec( env, &addr, jaddr );
        comms_setAddr( state->game.comms, &addr );
    } else {
        XP_LOGF( "%s: no comms this game", __func__ );
    }
    XWJNI_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1receiveMessage
( JNIEnv* env, jclass C, jint gamePtr, jbyteArray jstream, jobject jaddr )
{
    jboolean result;
    XWJNI_START_GLOBALS();
    XP_ASSERT( state->game.comms );
    XP_ASSERT( state->game.server );

    XWStreamCtxt* stream = streamFromJStream( MPPARM(mpool) env, globals->vtMgr,
                                              jstream );
    CommsAddrRec* addrp = NULL;
    CommsAddrRec addr = {0};
    XP_ASSERT( !!jaddr );
    if ( NULL != jaddr ) {
        getJAddrRec( env, &addr, jaddr );
        addrp = &addr;
    }
    result = comms_checkIncomingStream( state->game.comms, stream, addrp );
    if ( result ) {
        ServerCtxt* server = state->game.server;
        (void)server_do( server );
        (void)server_receiveMessage( server, stream );
        /* in case MORE work's pending.  Multiple calls are required in at
           least one case, where I'm a host handling client registration *AND*
           I'm a robot.  Only one server_do and I'll never make that first
           robot move.  That's because comms can't detect a duplicate initial
           packet (in validateInitialMessage()). */
        int ii;
        for ( ii = 0; ii < 5; ++ii ) {
            (void)server_do( server );
        }
    }

    stream_destroy( stream );

    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1summarize
( JNIEnv* env, jclass C, jint gamePtr, jobject jsummary )
{
    XWJNI_START();
    ModelCtxt* model = state->game.model;
    XP_S16 nMoves = model_getNMoves( model );
    setInt( env, jsummary, "nMoves", nMoves );
    XP_Bool gameOver = server_getGameIsOver( state->game.server );
    setBool( env, jsummary, "gameOver", gameOver );
    setInt( env, jsummary, "turn", 
            server_getCurrentTurn( state->game.server ) );
    setInt( env, jsummary, "lastMoveTime", 
            server_getLastMoveTime(state->game.server) );
    
    if ( !!state->game.comms ) {
        CommsAddrRec addr;
        CommsCtxt* comms = state->game.comms;
        comms_getAddr( comms, &addr );
        setInt( env, jsummary, "seed", comms_getChannelSeed( comms ) );
        setInt( env, jsummary, "missingPlayers", 
                server_getMissingPlayers( state->game.server ) );
        setInt( env, jsummary, "nPacketsPending", 
                comms_countPendingPackets( state->game.comms ) );

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
#if defined XWFEATURE_BLUETOOTH || defined XWFEATURE_SMS
            case COMMS_CONN_BT:
            case COMMS_CONN_SMS: {
                XP_Bool isBT = COMMS_CONN_BT == typ;
                CommsAddrRec addrs[MAX_NUM_PLAYERS];
                XP_U16 count = VSIZE(addrs);
                comms_getAddrs( comms, addrs, &count );
            
                int ii;
                const XP_UCHAR* addrps[count];
                for ( ii = 0; ii < count; ++ii ) {
                    addrps[ii] = isBT ? (XP_UCHAR*)&addrs[ii].u.bt.btAddr : 
                        (XP_UCHAR*)&addrs[ii].u.sms.phone;
                    XP_LOGF( "%s: adding btaddr/phone %s", __func__, addrps[ii] );
                }
                jobjectArray jaddrs = makeStringArray( env, count, addrps );
                setObject( env, jsummary, "remoteDevs", "[Ljava/lang/String;", 
                           jaddrs );
                deleteLocalRef( env, jaddrs );
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
    int ii;
    if ( gameOver ) {
        ScoresArray scores;
        model_figureFinalScores( model, &scores, NULL );
        for ( ii = 0; ii < nPlayers; ++ii ) {
            jvals[ii] = scores.arr[ii];
        }
    } else {
        for ( ii = 0; ii < nPlayers; ++ii ) {
            jvals[ii] = model_getPlayerScore( model, ii );
        }
    }
    jintArray jarr = makeIntArray( env, nPlayers, jvals );
    setObject( env, jsummary, "scores", "[I", jarr );
    deleteLocalRef( env, jarr );

    XWJNI_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1server_1prefsChanged
( JNIEnv* env, jclass C, jint gamePtr, jobject jcp )
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
( JNIEnv* env, jclass C, jint gamePtr )
{
    jint result;
    XWJNI_START();
    result = board_getFocusOwner( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1focusChanged
( JNIEnv* env, jclass C, jint gamePtr, jint typ )
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
( JNIEnv* env, jclass C, jint gamePtr, jobject jkey, jboolean jup, 
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
( JNIEnv* env, jclass C, jint gamePtr, jobject jgi )
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
};

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1getState
( JNIEnv* env, jclass C, jint gamePtr, jobject jgsi )
{
    XWJNI_START();
    GameStateInfo info;
    game_getState( &state->game, &info );

    setInts( env, jgsi, (void*)&info, gsi_ints, VSIZE(gsi_ints) );
    setBools( env, jgsi, (void*)&info, gsi_bools, VSIZE(gsi_bools) );

    XWJNI_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1hasComms
( JNIEnv* env, jclass C, jint gamePtr )
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
( JNIEnv* env, jclass C, jint gamePtr, jobject jgi, jstring jname, 
  jbyteArray jDictBytes, jstring jpath )
{
    XWJNI_START_GLOBALS();
    DictionaryCtxt* dict = makeDict( MPPARM(state->globalJNI->mpool) env, 
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

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1resendAll
( JNIEnv* env, jclass C, jint gamePtr, jboolean force, jboolean thenAck )
{
    XWJNI_START();
    CommsCtxt* comms = state->game.comms;
    XP_ASSERT( !!comms );
    (void)comms_resendAll( comms, force );
    if ( thenAck ) {
#ifdef XWFEATURE_COMMSACK
        comms_ackAny( comms );
#endif
    }
    XWJNI_END();
}

#ifdef XWFEATURE_COMMSACK
JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1ackAny
( JNIEnv* env, jclass C, jint gamePtr )
{
    XWJNI_START();
    XP_ASSERT( !!state->game.comms );
    (void)comms_ackAny( state->game.comms );
    XWJNI_END();
}
#endif

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1transportFailed
( JNIEnv* env, jclass C, jint gamePtr )
{
    XWJNI_START();
    XP_ASSERT( !!state->game.comms );
    (void)comms_transportFailed( state->game.comms );
    XWJNI_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1isConnected
( JNIEnv* env, jclass C, jint gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = NULL != state->game.comms && comms_isConnected( state->game.comms );
    XWJNI_END();
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1getStats
( JNIEnv* env, jclass C, jint gamePtr )
{
    jstring result = NULL;
#ifdef DEBUG
    XWJNI_START_GLOBALS();
    if ( NULL != state->game.comms ) {
        XWStreamCtxt* stream = mem_stream_make( MPPARM(mpool) globals->vtMgr,
                                                NULL, 0, NULL );
        comms_getStats( state->game.comms, stream );
        result = streamToJString( env, stream );
        stream_destroy( stream );
    }
    XWJNI_END();
#endif
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1endGame
( JNIEnv* env, jclass C, jint gamePtr )
{
    XWJNI_START();
    XP_ASSERT( !!state->game.server );
    server_endGame( state->game.server );
    XWJNI_END();
}

#ifdef XWFEATURE_CHAT
JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1sendChat
( JNIEnv* env, jclass C, jint gamePtr, jstring jmsg )
{
    XWJNI_START();
    XP_ASSERT( !!state->game.server );
    const char* msg = (*env)->GetStringUTFChars( env, jmsg, NULL );
    server_sendChat( state->game.server, msg );
    (*env)->ReleaseStringUTFChars( env, jmsg, msg );
    XWJNI_END();
}
#endif

#ifdef XWFEATURE_WALKDICT
////////////////////////////////////////////////////////////
// Dict iterator
////////////////////////////////////////////////////////////

typedef struct _DictIterData {
    JNIGlobalState* state;
    JNIEnv* env;
    JNIUtilCtxt* jniutil;
    VTableMgr* vtMgr;
    DictionaryCtxt* dict;
    DictIter iter;
    IndexData idata;
    XP_U16 depth;
#ifdef MEM_DEBUG
    MemPoolCtx* mpool;
#endif
} DictIterData;

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1iter_1init
( JNIEnv* env, jclass C, jint jniGlobalPtr, jbyteArray jDictBytes, jstring jname, 
  jstring jpath, jobject jniu )
{
    jint closure = 0;
    JNIGlobalState* state = (JNIGlobalState*)jniGlobalPtr;
    map_thread( &state->ti, env );
    DictIterData* data = XP_CALLOC( state->mpool, sizeof(*data) );
    data->state = state;
    data->env = env;
    JNIUtilCtxt* jniutil = makeJNIUtil( MPPARM(state->mpool) &state->ti, jniu );
    DictionaryCtxt* dict = makeDict( MPPARM(state->mpool) env, state->dictMgr, 
                                     jniutil, jname, jDictBytes, jpath, NULL, 
                                     false );
    if ( !!dict ) {
        data->vtMgr = make_vtablemgr( MPPARM_NOCOMMA(state->mpool) );
        data->jniutil = jniutil;
        data->dict = dict;
        data->depth = 2;
#ifdef MEM_DEBUG
        data->mpool = state->mpool;
#endif
        closure = (int)data;
    } else {
        destroyJNIUtil( &jniutil );
        XP_FREE( state->mpool, data );
    }
    return closure;
}

static void
freeIndices( DictIterData* data )
{
    IndexData* idata = &data->idata;
    if ( !!idata->prefixes ) {
        XP_FREE( data->mpool, idata->prefixes );
        idata->prefixes = NULL;
    }
    if( !!idata->indices ) {
        XP_FREE( data->mpool, idata->indices );
        idata->indices = NULL;
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
    idata->prefixes = XP_MALLOC( data->mpool, count * data->depth 
                                 * sizeof(*idata->prefixes) );
    idata->indices = XP_MALLOC( data->mpool, 
                                count * sizeof(*idata->indices) );
    idata->count = count;

    dict_makeIndex( &data->iter, data->depth, idata );
    if ( 0 < idata->count ) {
        idata->prefixes = XP_REALLOC( data->mpool, idata->prefixes,
                                      idata->count * data->depth *
                                      sizeof(*idata->prefixes) );
        idata->indices = XP_REALLOC( data->mpool, idata->indices,
                                     idata->count * sizeof(*idata->indices) );
    } else {
        freeIndices( data );
    }
} /* makeIndex */

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1iter_1setMinMax
( JNIEnv* env, jclass C, jint closure, jint min, jint max )
{
    DictIterData* data = (DictIterData*)closure;
    if ( NULL != data ) {
        dict_initIter( &data->iter, data->dict, min, max );
        makeIndex( data );
        (void)dict_firstWord( &data->iter );
    }
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1iter_1destroy
( JNIEnv* env, jclass C, jint closure )
{
    DictIterData* data = (DictIterData*)closure;
    if ( NULL != data ) {
#ifdef MEM_DEBUG
        MemPoolCtx* mpool = data->mpool;
#endif
        dict_unref( data->dict );
        destroyJNIUtil( &data->jniutil );
        freeIndices( data );
        vtmgr_destroy( MPPARM(mpool) data->vtMgr );
        map_remove( &data->state->ti, env );
        XP_FREE( mpool, data );
    }
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1iter_1wordCount
(JNIEnv* env, jclass C, jint closure )
{
    jint result = 0;
    DictIterData* data = (DictIterData*)closure;
    if ( NULL != data ) {
        result = data->iter.nWords;
    }
    return result;
}

JNIEXPORT jintArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1iter_1getCounts
(JNIEnv* env, jclass C, jint closure )
{
    jintArray result = NULL;
    DictIterData* data = (DictIterData*)closure;
    if ( NULL != data ) {
        DictIter iter;
        dict_initIter( &iter, data->dict, 0, MAX_COLS_DICT );

        LengthsArray lens;
        if ( 0 < dict_countWords( &iter, &lens ) ) {
            XP_ASSERT( sizeof(jint) == sizeof(lens.lens[0]) );
            result = makeIntArray( env, VSIZE(lens.lens), (jint*)&lens.lens );
        }
    }
    return result;
}

JNIEXPORT jobjectArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1iter_1getPrefixes
( JNIEnv* env, jclass C, jint closure )
{
    jobjectArray result = NULL;
    DictIterData* data = (DictIterData*)closure;
    if ( NULL != data && NULL != data->idata.prefixes ) {
        result = makeStringArray( env, data->idata.count, NULL );

        int ii;
        XP_U16 depth = data->depth;
        for ( ii = 0; ii < data->idata.count; ++ii ) {
            XP_UCHAR buf[16];
            (void)dict_tilesToString( data->dict, 
                                      &data->idata.prefixes[depth*ii], 
                                      depth, buf, VSIZE(buf) );
            jstring jstr = (*env)->NewStringUTF( env, buf );
            (*env)->SetObjectArrayElement( env, result, ii, jstr );
            deleteLocalRef( env, jstr );
        }
    }
    return result;
}

JNIEXPORT jintArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1iter_1getIndices
( JNIEnv* env, jclass C , jint closure )
{
    jintArray jindices = NULL;
    DictIterData* data = (DictIterData*)closure;
    if ( NULL != data ) {
        XP_ASSERT( !!data->idata.indices );
        XP_ASSERT( sizeof(jint) == sizeof(data->idata.indices[0]) );
        jindices = makeIntArray( env, data->idata.count, 
                                 (jint*)data->idata.indices );
    }
    return jindices;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1iter_1nthWord
( JNIEnv* env, jclass C, jint closure, jint nn)
{
    jstring result = NULL;
    DictIterData* data = (DictIterData*)closure;
    if ( NULL != data ) {
        if ( dict_getNthWord( &data->iter, nn, data->depth, &data->idata ) ) {
            XP_UCHAR buf[64];
            dict_wordToString( &data->iter, buf, VSIZE(buf) );
            result = (*env)->NewStringUTF( env, buf );
        }
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1iter_1getStartsWith
( JNIEnv* env, jclass C, jint closure, jstring jprefix )
{
    jint result = -1;
    DictIterData* data = (DictIterData*)closure;
    if ( NULL != data ) {
        const char* prefix = (*env)->GetStringUTFChars( env, jprefix, NULL );
        if ( 0 <= dict_findStartsWith( &data->iter, prefix ) ) {
            result = dict_getPosition( &data->iter );
        }
        (*env)->ReleaseStringUTFChars( env, jprefix, prefix );
    }
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1iter_1getDesc
( JNIEnv* env, jclass C, jint closure )
{
    jstring result = NULL;
    DictIterData* data = (DictIterData*)closure;
    if ( NULL != data ) {
        const XP_UCHAR* disc = dict_getDesc( data->dict );
        if ( NULL != disc && '\0' != disc[0] ) {
            result = (*env)->NewStringUTF( env, disc );
        }
    }
    return result;
}

#ifdef XWFEATURE_BASE64
JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_base64Encode
( JNIEnv* env, jclass C, jbyteArray jbytes )
{
    int inlen = (*env)->GetArrayLength( env, jbytes );
    jbyte* elems = (*env)->GetByteArrayElements( env, jbytes, NULL );
    XP_ASSERT( !!elems );

    XP_UCHAR out[4+(inlen*4/3)];
    XP_U16 outlen = VSIZE( out );
    binToSms( out, &outlen, (const XP_U8*)elems, inlen );

    (*env)->ReleaseByteArrayElements( env, jbytes, elems, 0 );

    jstring result = (*env)->NewStringUTF( env, out );
    return result;
}

JNIEXPORT jbyteArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_base64Decode
( JNIEnv* env, jclass C, jstring jstr )
{
    jbyteArray result = NULL;
    const char* instr = (*env)->GetStringUTFChars( env, jstr, NULL );
    XP_U16 inlen = (*env)->GetStringUTFLength( env, jstr );
    XP_U8 out[inlen];
    XP_U16 outlen = VSIZE(out);
    if ( smsToBin( out, &outlen, instr, inlen ) ) {
        result = makeByteArray( env, outlen, (jbyte*)out );
    } else {
        XP_ASSERT(0);
    }
    (*env)->ReleaseStringUTFChars( env, jstr, instr );
    return result;
}
#endif

#endif  /* XWFEATURE_BOARDWORDS */
