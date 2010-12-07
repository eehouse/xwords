/* -*-mode: C; compile-command: "../../scripts/ndkbuild.sh"; -*- */
/*
 * Copyright © 2009-2010 by Eric House (xwords@eehouse.org).  All
 * rights reserved.
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

#include <jni.h>
#include <android/log.h>

#include "comtypes.h"
#include "game.h"
#include "board.h"
#include "mempool.h"
#include "strutils.h"

#include "utilwrapper.h"
#include "drawwrapper.h"
#include "xportwrapper.h"
#include "anddict.h"
#include "andutils.h"
#include "jniutlswrapper.h"

static CurGameInfo*
makeGI( MPFORMAL JNIEnv* env, jobject j_gi )
{
    CurGameInfo* gi = (CurGameInfo*)XP_CALLOC( mpool, sizeof(*gi) );
    XP_UCHAR buf[256];          /* in case needs whole path */

    gi->nPlayers = getInt( env, j_gi, "nPlayers");
    gi->gameSeconds = getInt( env, j_gi, "gameSeconds");
    gi->boardSize = getInt( env, j_gi, "boardSize" );
    gi->gameID = getInt( env, j_gi, "gameID" );
    gi->dictLang = getInt( env, j_gi, "dictLang" );
    gi->robotSmartness = getInt( env, j_gi, "robotSmartness" );
    gi->hintsNotAllowed = getBool( env, j_gi, "hintsNotAllowed" );
    gi->timerEnabled =  getBool( env, j_gi, "timerEnabled" );
    gi->allowPickTiles = getBool( env, j_gi, "allowPickTiles" );
    gi->allowHintRect = getBool( env, j_gi, "allowHintRect" );

    gi->phoniesAction = jenumFieldToInt( env, j_gi, "phoniesAction",
                                         "org/eehouse/android/xw4/jni/"
                                         "CurGameInfo$XWPhoniesChoice");
    gi->serverRole = 
        jenumFieldToInt( env, j_gi, "serverRole",
                         "org/eehouse/android/xw4/jni/CurGameInfo$DeviceRole");

    getString( env, j_gi, "dictName", buf, VSIZE(buf) );
    gi->dictName = copyString( mpool, buf );

    XP_ASSERT( gi->nPlayers <= MAX_NUM_PLAYERS );

    jobject jplayers;
    if ( getObject( env, j_gi, "players", 
                    "[Lorg/eehouse/android/xw4/jni/LocalPlayer;",
                    &jplayers ) ) {
        int ii;
        for ( ii = 0; ii < gi->nPlayers; ++ii ) {
            LocalPlayer* lp = &gi->players[ii];

            jobject jlp = (*env)->GetObjectArrayElement( env, jplayers, ii );
            XP_ASSERT( !!jlp );

            lp->isRobot = getBool( env, jlp, "isRobot" );
            lp->isLocal = getBool( env, jlp, "isLocal" );

            getString( env, jlp, "name", buf, VSIZE(buf) );
            lp->name = copyString( mpool, buf );
            getString( env, jlp, "password", buf, VSIZE(buf) );
            lp->password = copyString( mpool, buf );

            lp->secondsUsed = 0;

            (*env)->DeleteLocalRef( env, jlp );
        }
        (*env)->DeleteLocalRef( env, jplayers );
    } else {
        XP_ASSERT(0);
    }

    return gi;
} /* makeGI */

static void
setJGI( JNIEnv* env, jobject jgi, const CurGameInfo* gi )
{
    // set fields
    setInt( env, jgi, "nPlayers", gi->nPlayers );
    setInt( env, jgi, "gameSeconds", gi->gameSeconds );
    setInt( env, jgi, "boardSize", gi->boardSize );
    setInt( env, jgi, "gameID", gi->gameID );
    setInt( env, jgi, "dictLang", gi->dictLang );
    setInt( env, jgi, "robotSmartness", gi->robotSmartness );
    setBool( env, jgi, "hintsNotAllowed", gi->hintsNotAllowed );
    setBool( env, jgi, "timerEnabled", gi->timerEnabled );
    setBool( env, jgi, "allowPickTiles", gi->allowPickTiles );
    setString( env, jgi, "dictName", gi->dictName );

    intToJenumField( env, jgi, gi->phoniesAction, "phoniesAction",
                     "org/eehouse/android/xw4/jni/CurGameInfo$XWPhoniesChoice" );
    intToJenumField( env, jgi, gi->serverRole, "serverRole",
                     "org/eehouse/android/xw4/jni/CurGameInfo$DeviceRole" );

    jobject jplayers;
    if ( getObject( env, jgi, "players", 
                    "[Lorg/eehouse/android/xw4/jni/LocalPlayer;",
                    &jplayers ) ) {
        int ii;
        for ( ii = 0; ii < gi->nPlayers; ++ii ) {
            const LocalPlayer* lp = &gi->players[ii];

            jobject jlp = (*env)->GetObjectArrayElement( env, jplayers, ii );
            XP_ASSERT( !!jlp );

            setBool( env, jlp, "isRobot", lp->isRobot );
            setBool( env, jlp, "isLocal", lp->isLocal );
            setString( env, jlp, "name", lp->name );
            setString( env, jlp, "password", lp->password );
            setInt( env, jlp, "secondsUsed", lp->secondsUsed );

            (*env)->DeleteLocalRef( env, jlp );
        }
        (*env)->DeleteLocalRef( env, jplayers );
    } else {
        XP_ASSERT(0);
    }
} /* setJGI */

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
}

static XWStreamCtxt*
streamFromJStream( MPFORMAL JNIEnv* env, VTableMgr* vtMgr, jbyteArray jstream )
{
    XWStreamCtxt* stream = mem_stream_make( MPPARM(mpool) vtMgr,
                                            NULL, 0, NULL );
    int len = (*env)->GetArrayLength( env, jstream );
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

    /* Unlike on other platforms, gi is created without a call to
       game_makeNewGame, which sets gameID.  So check here if it's still unset
       and if necessary set it. */
    while ( 0 == gi->gameID ) {
        gi->gameID = and_util_getCurSeconds( NULL );
    }

    game_saveToStream( NULL, gi, stream );
    destroyGI( MPPARM(mpool) &gi );

    int nBytes = stream_getSize( stream );
    result = (*env)->NewByteArray( env, nBytes );
    jbyte* jelems = (*env)->GetByteArrayElements( env, result, NULL );
    stream_getBytes( stream, jelems, nBytes );
    (*env)->ReleaseByteArrayElements( env, result, jelems, 0 );
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
                              &gi, NULL, NULL, NULL, NULL, NULL ) ) {
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

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1getInfo
(JNIEnv* env, jclass C, jbyteArray jDictBytes, jobject jniu, jobject jinfo )
{
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = mpool_make();
#endif
    JNIUtilCtxt* jniutil = makeJNIUtil( MPPARM(mpool) &env, jniu );
    DictionaryCtxt* dict = makeDict( MPPARM(mpool) env, jniutil,
                                     jDictBytes, NULL );
    jint code = dict_getLangCode( dict );
    jint nWords = dict_getWordCount( dict );
    dict_destroy( dict );
    destroyJNIUtil( &jniutil );

    setInt( env, jinfo, "langCode", code );
    setInt( env, jinfo, "wordCount", nWords );
#ifdef MEM_DEBUG
    mpool_destroy( mpool );
#endif
}

/* Dictionary methods: don't use gamePtr */
JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1tilesAreSame
( JNIEnv* env, jclass C, jint dictPtr1, jint dictPtr2 )
{
    jboolean result;
    const DictionaryCtxt* dict1 = (DictionaryCtxt*)dictPtr1;
    const DictionaryCtxt* dict2 = (DictionaryCtxt*)dictPtr2;
    result = dict_tilesAreSame( dict1, dict2 );
    return result;
}

JNIEXPORT jobjectArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1getChars
( JNIEnv* env, jclass C, jint dictPtr )
{
    jobject result = NULL;
    result = and_dictionary_getChars( env, (DictionaryCtxt*)dictPtr );
    (*env)->DeleteLocalRef( env, result );
    return result;
}

typedef struct _JNIState {
    XWGame game;
    JNIEnv* env;
    AndGlobals globals;
#ifdef DEBUG
    const char* envSetterFunc;
#endif
    MPSLOT
} JNIState;

#ifdef DEBUG
# define CHECK_ENV()                                                    \
    if ( state->env != 0 && state->env != env ) {                       \
        XP_LOGF( "ERROR: %s trying to set env when %s still has it",    \
                 __func__, state->envSetterFunc );                      \
        XP_ASSERT( state->env == 0 || state->env == env );              \
    }                                                                   \
    state->envSetterFunc = __func__;                                    \

#else
# define CHECK_ENV()
#endif

#define XWJNI_START() {                                 \
    XP_ASSERT( 0 != gamePtr );                          \
    JNIState* state = (JNIState*)gamePtr;               \
    MPSLOT;                                             \
    MPASSIGN( mpool, state->mpool);                     \
    /* if reentrant must be from same thread */         \
    CHECK_ENV();                                        \
    JNIEnv* _oldEnv = state->env;                       \
    state->env = env;

#define XWJNI_START_GLOBALS()                           \
    XWJNI_START()                                       \
    AndGlobals* globals = &state->globals;              \

#define XWJNI_END()                             \
    state->env = _oldEnv;                       \
    }

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_initJNI
( JNIEnv* env, jclass C )
{
    struct timeval tv;
    gettimeofday( &tv, NULL );
    srandom( tv.tv_sec );
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = mpool_make();
#endif
    JNIState* state = (JNIState*)XP_CALLOC( mpool, sizeof(*state) );
    AndGlobals* globals = &state->globals;
    globals->state = (struct JNIState*)state;
    MPASSIGN( state->mpool, mpool );
    globals->vtMgr = make_vtablemgr(MPPARM_NOCOMMA(mpool));

    XP_U32 secs = and_util_getCurSeconds( NULL );
    XP_LOGF( "initing srand with %ld", secs );
    srandom( secs );

    return (jint) state;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1makeNewGame
( JNIEnv* env, jclass C, jint gamePtr, jobject j_gi, jobject j_util, 
  jobject jniu, jobject j_draw, jobject j_cp, jobject j_procs, 
  jbyteArray jDictBytes, jstring jDictName )
{
    XWJNI_START_GLOBALS();
    CurGameInfo* gi = makeGI( MPPARM(mpool) env, j_gi );
    globals->gi = gi;
    globals->util = makeUtil( MPPARM(mpool) &state->env, j_util, gi, 
                              globals );
    globals->jniutil = makeJNIUtil( MPPARM(mpool) &state->env, jniu );
    DrawCtx* dctx = makeDraw( MPPARM(mpool) &state->env, j_draw );
    globals->dctx = dctx;
    globals->xportProcs = makeXportProcs( MPPARM(mpool) &state->env, j_procs );
    CommonPrefs cp;
    loadCommonPrefs( env, &cp, j_cp );

    XP_LOGF( "calling game_makeNewGame" );
    game_makeNewGame( MPPARM(mpool) &state->game, gi, globals->util, dctx, &cp,
                      globals->xportProcs );

    DictionaryCtxt* dict = makeDict( MPPARM(mpool) env, globals->jniutil,
                                     jDictBytes, jDictName );
#ifdef STUBBED_DICT
    if ( !dict ) {
        XP_LOGF( "falling back to stubbed dict" );
        dict = make_stubbed_dict( MPPARM_NOCOMMA(mpool) );
    }
#endif
    XP_ASSERT( !!dict );
    model_setDictionary( state->game.model, dict );
    XWJNI_END();
} /* makeNewGame */

JNIEXPORT void JNICALL Java_org_eehouse_android_xw4_jni_XwJNI_game_1dispose
( JNIEnv * env, jclass claz, jint gamePtr )
{
    JNIState* state = (JNIState*)gamePtr;
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = state->mpool;
#endif
    AndGlobals* globals = &state->globals;

    destroyGI( MPPARM(mpool) &globals->gi );

    JNIEnv* oldEnv = state->env;
    state->env = env;
    game_dispose( &state->game );

    destroyDraw( &globals->dctx );
    destroyXportProcs( &globals->xportProcs );
    destroyUtil( &globals->util );
    destroyJNIUtil( &globals->jniutil );
    vtmgr_destroy( MPPARM(mpool) globals->vtMgr );

    state->env = oldEnv;
    XP_FREE( mpool, state );
    mpool_destroy( mpool );
} /* game_dispose */

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1makeFromStream
( JNIEnv* env, jclass C, jint gamePtr, jbyteArray jstream, 
  jobject /*out*/jgi, jbyteArray jdict, jstring jdictName, jobject jutil, 
  jobject jniu, jobject jdraw, jobject jcp, jobject jprocs )
{
    jboolean result;
    XWJNI_START_GLOBALS();

    globals->gi = (CurGameInfo*)XP_CALLOC( mpool, sizeof(*globals->gi) );
    globals->util = makeUtil( MPPARM(mpool) &state->env, 
                              jutil, globals->gi, globals );
    globals->jniutil = makeJNIUtil( MPPARM(mpool) &state->env, jniu );
    DictionaryCtxt* dict = makeDict( MPPARM(mpool) env, globals->jniutil, 
                                     jdict, jdictName );
    globals->dctx = makeDraw( MPPARM(mpool) &state->env, jdraw );
    globals->xportProcs = makeXportProcs( MPPARM(mpool) &state->env, jprocs );

    XWStreamCtxt* stream = streamFromJStream( MPPARM(mpool) env, 
                                              globals->vtMgr, jstream );

    CommonPrefs cp;
    loadCommonPrefs( env, &cp, jcp );
    result = game_makeFromStream( MPPARM(mpool) stream, &state->game, 
                                  globals->gi, dict, 
                                  globals->util, globals->dctx, &cp,
                                  globals->xportProcs );
    stream_destroy( stream );

    if ( result ) {
        XP_ASSERT( 0 != globals->gi->gameID );
        if ( !!jgi ) {
            setJGI( env, jgi, globals->gi );
        }
    } else {
        destroyDraw( &globals->dctx );
        destroyXportProcs( &globals->xportProcs );
        dict_destroy( dict );
        dict = NULL;
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
    XWStreamCtxt* stream = mem_stream_make( MPPARM(mpool) globals->vtMgr,
                                            NULL, 0, NULL );

    game_saveToStream( &state->game, gi, stream );

    if ( NULL != jgi ) {
        destroyGI( MPPARM(mpool) &gi );
    }

    int nBytes = stream_getSize( stream );
    result = (*env)->NewByteArray( env, nBytes );
    jbyte* jelems = (*env)->GetByteArrayElements( env, result, NULL );
    stream_getBytes( stream, jelems, nBytes );
    (*env)->ReleaseByteArrayElements( env, result, jelems, 0 );
    stream_destroy( stream );

    (*env)->DeleteLocalRef( env, result );
    XWJNI_END();
    return result;
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
    server_handleUndo( state->game.server );
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
    result = streamToJString( MPPARM(mpool) env, stream );
    stream_destroy( stream );
    (*env)->DeleteLocalRef( env, result );

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
    result = streamToJString( MPPARM(mpool) env, stream );
    stream_destroy( stream );
    (*env)->DeleteLocalRef( env, result );
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
    result = streamToJString( MPPARM(mpool) env, stream );
    (*env)->DeleteLocalRef( env, result );
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

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1writeFinalScores
( JNIEnv* env, jclass C, jint gamePtr )
{
    jstring result;
    XWJNI_START_GLOBALS();
    XWStreamCtxt* stream = and_empty_stream( MPPARM(mpool) globals );
    server_writeFinalScores( state->game.server, stream );
    result = streamToJString( MPPARM(mpool) env, stream );
    (*env)->DeleteLocalRef( env, result );
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
    if ( stream_getSize( stream ) > 0 ) {
        comms_send( state->game.comms, stream );
    }
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1initClientConnection
( JNIEnv* env, jclass C, jint gamePtr )
{
    LOG_FUNC();
    XWJNI_START_GLOBALS();
    XWStreamCtxt* stream = and_empty_stream( MPPARM(mpool) globals );
    stream_setOnCloseProc( stream, and_send_on_close );
    server_initClientConnection( state->game.server, stream );
    XWJNI_END();
    LOG_RETURN_VOID();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1start
( JNIEnv* env, jclass C, jint gamePtr )
{
    XWJNI_START();
    if ( !!state->game.comms ) {
        comms_start( state->game.comms );
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
    LOG_FUNC();
    XP_ASSERT( state->game.comms );
    CommsAddrRec addr;
    comms_getAddr( state->game.comms, &addr );
    setJAddrRec( env, jaddr, &addr );
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1setAddr
( JNIEnv* env, jclass C, jint gamePtr, jobject jaddr )
{
    XWJNI_START();
    if ( state->game.comms ) {
        CommsAddrRec addr;
        getJAddrRec( env, &addr, jaddr );
        comms_setAddr( state->game.comms, &addr );
    } else {
        XP_LOGF( "%s: no comms this game", __func__ );
    }
    XWJNI_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1receiveMessage
( JNIEnv* env, jclass C, jint gamePtr, jbyteArray jstream )
{
    jboolean result;
    XWJNI_START_GLOBALS();
    XP_ASSERT( state->game.comms );
    XP_ASSERT( state->game.server );

    XWStreamCtxt* stream = streamFromJStream( MPPARM(mpool) env, globals->vtMgr,
                                              jstream );
    result = comms_checkIncomingStream( state->game.comms, stream, NULL )
        && server_receiveMessage( state->game.server, stream );

    stream_destroy( stream );

    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1summarize
( JNIEnv* env, jclass C, jint gamePtr, jobject jsummary )
{
    LOG_FUNC();
    XWJNI_START();
    ModelCtxt* model = state->game.model;
    XP_S16 nMoves = model_getNMoves( model );
    setInt( env, jsummary, "nMoves", nMoves );
    XP_Bool gameOver = server_getGameIsOver( state->game.server );
    setBool( env, jsummary, "gameOver", gameOver );
    
    if ( !!state->game.comms ) {
        CommsAddrRec addr;
        CommsCtxt* comms = state->game.comms;
        comms_getAddr( comms, &addr );
        intToJenumField( env, jsummary, addr.conType, "conType",
                         "org/eehouse/android/xw4/jni/"
                         "CommsAddrRec$CommsConnType" );
        if ( COMMS_CONN_RELAY == addr.conType ) {
            XP_UCHAR buf[128];
            XP_U16 len = VSIZE(buf);
            if ( comms_getRelayID( comms, buf, &len ) ) {
                buf[len] = '\0';
                setString( env, jsummary, "relayID", buf );
            }
            setString( env, jsummary, "roomName", addr.u.ip_relay.invite );
            setInt( env, jsummary, "seed", comms_getChannelSeed( comms ) );
        } else if ( COMMS_CONN_SMS == addr.conType ) {
            setString( env, jsummary, "smsPhone", addr.u.sms.phone );
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
    (*env)->DeleteLocalRef( env, jarr );

    XWJNI_END();
    LOG_RETURN_VOID();
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

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1visTileCount
( JNIEnv* env, jclass C, jint gamePtr )
{
    jint result;
    XWJNI_START();
    result = board_visTileCount( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1canHint
( JNIEnv* env, jclass C, jint gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = board_canHint( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1canShuffle
( JNIEnv* env, jclass C, jint gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = board_canShuffle( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1canTogglePending
( JNIEnv* env, jclass C, jint gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = board_canTogglePending( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1canChat
( JNIEnv* env, jclass C, jint gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = NULL != state->game.comms && comms_canChat( state->game.comms );
    XWJNI_END();
    return result;
}

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

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1resendAll
( JNIEnv* env, jclass C, jint gamePtr )
{
    XWJNI_START();
    XP_ASSERT( !!state->game.comms );
    (void)comms_resendAll( state->game.comms );
    XWJNI_END();
}

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

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1endGame
( JNIEnv* env, jclass C, jint gamePtr )
{
    XWJNI_START();
    XP_ASSERT( !!state->game.server );
    server_endGame( state->game.server );
    XWJNI_END();
}

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

