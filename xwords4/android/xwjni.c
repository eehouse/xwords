/* -*-mode: C; compile-command: "cd XWords4; ../scripts/ndkbuild.sh"; -*- */
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
#include "anddict.h"
#include "andutils.h"

static CurGameInfo*
makeGI( MPFORMAL JNIEnv* env, jobject j_gi )
{
    CurGameInfo* gi = (CurGameInfo*)XP_CALLOC( mpool, sizeof(*gi) );
    int nPlayers, robotSmartness, boardSize;
    XP_UCHAR buf[256];          /* in case needs whole path */

    bool success = getInt( env, j_gi, "nPlayers", &nPlayers )
        && getInt( env, j_gi, "boardSize", &boardSize )
        && getInt( env, j_gi, "robotSmartness", &robotSmartness )
        && getBool( env, j_gi, "hintsNotAllowed", &gi->hintsNotAllowed )
        && getBool( env, j_gi, "timerEnabled", &gi->timerEnabled )
        && getBool( env, j_gi, "allowPickTiles", &gi->allowPickTiles )
        && getBool( env, j_gi, "allowHintRect", &gi->allowHintRect )
        ;
    XP_ASSERT( success );

    gi->nPlayers = nPlayers;
    gi->robotSmartness = robotSmartness;
    gi->boardSize = boardSize;
    gi->serverRole = SERVER_STANDALONE; /* figure out enums later */

    getString( env, j_gi, "dictName", buf, VSIZE(buf) );
    gi->dictName = copyString( mpool, buf );
    XP_LOGF( "dict name: %s", gi->dictName );

    XP_ASSERT( nPlayers < MAX_NUM_PLAYERS );

    jobject jplayers;
    if ( getObject( env, j_gi, "players", 
                    "[Lorg/eehouse/android/xw4/jni/LocalPlayer;",
                    &jplayers ) ) {
        int ii;
        for ( ii = 0; ii < gi->nPlayers; ++ii ) {
            LocalPlayer* lp = &gi->players[ii];

            jobject jlp = (*env)->GetObjectArrayElement( env, jplayers, ii );

            getBool( env, jlp, "isRobot", &lp->isRobot );
            getBool( env, jlp, "isLocal", &lp->isLocal );

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
    bool success = setInt( env, jgi, "nPlayers", gi->nPlayers )
        && setInt( env, jgi, "boardSize", gi->boardSize )
        && setInt( env, jgi, "robotSmartness", gi->robotSmartness )
        && setBool( env, jgi, "hintsNotAllowed", gi->hintsNotAllowed )
        && setBool( env, jgi, "timerEnabled", gi->timerEnabled )
        && setBool( env, jgi, "allowPickTiles", gi->allowPickTiles )
        && setBool( env, jgi, "allowHintRect", gi->allowHintRect )
        && setString( env, jgi, "dictName", gi->dictName )
        ;
    XP_ASSERT( success );

    jobject jplayers;
    if ( getObject( env, jgi, "players", 
                    "[Lorg/eehouse/android/xw4/jni/LocalPlayer;",
                    &jplayers ) ) {
        int ii;
        for ( ii = 0; ii < gi->nPlayers; ++ii ) {
            const LocalPlayer* lp = &gi->players[ii];

            jobject jlp = (*env)->GetObjectArrayElement( env, jplayers, ii );

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
}

static void
destroyGI( MPFORMAL CurGameInfo* gi )
{
    gi_disposePlayerInfo( MPPARM(mpool) gi );
    XP_FREE( mpool, gi );
}

static bool
loadCommonPrefs( JNIEnv* env, CommonPrefs* cp, jobject j_cp )
{
    bool success = getBool( env, j_cp, "showBoardArrow", &cp->showBoardArrow )
        && getBool( env, j_cp, "showRobotScores", &cp->showRobotScores )
        && getBool( env, j_cp, "hideTileValues", &cp->hideTileValues )
        && getBool( env, j_cp, "skipCommitConfirm", &cp->skipCommitConfirm );
    return success;
}

static XWStreamCtxt*
and_empty_stream( MPFORMAL AndGlobals* globals )
{
    XWStreamCtxt* stream = mem_stream_make( MPPARM(mpool) globals->vtMgr,
                                            NULL, 0, NULL );
    return stream;
}

/****************************************************
 * These two methods are stateless: no gamePtr
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

    game_saveToStream( NULL, gi, stream );
    destroyGI( MPPARM(mpool) gi );

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
    LOG_FUNC();
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = mpool_make();
#endif
    VTableMgr* vtMgr = make_vtablemgr( MPPARM_NOCOMMA(mpool) );

    jbyte* jelems = (*env)->GetByteArrayElements( env, jstream, NULL );
    XWStreamCtxt* stream = mem_stream_make( MPPARM(mpool) vtMgr,
                                            NULL, 0, NULL );
    int len = (*env)->GetArrayLength( env, jstream );
    stream_putBytes( stream, jelems, len );
    (*env)->ReleaseByteArrayElements( env, jstream, jelems, 0 );

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
    LOG_RETURN_VOID();
}

typedef struct _JNIState {
    XWGame game;
    JNIEnv* env;
    AndGlobals globals;
    MPSLOT
} JNIState;

#define XWJNI_START() {                                 \
    XP_LOGF( "%s(env=%x)", __func__, env );             \
    XP_ASSERT( 0 != gamePtr );                          \
    JNIState* state = (JNIState*)gamePtr;               \
    MPSLOT;                                             \
    MPASSIGN( mpool, state->mpool);                     \
    AndGlobals* globals = &state->globals;              \
    /* if reentrant must be from same thread */         \
    XP_ASSERT( state->env == 0 || state->env == env );  \
    JNIEnv* _oldEnv = state->env;                       \
    state->env = env;

#define XWJNI_END()                             \
    state->env = _oldEnv;                       \
    LOG_RETURN_VOID();                          \
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
    MPASSIGN( state->mpool, mpool );
    globals->vtMgr = make_vtablemgr(MPPARM_NOCOMMA(mpool));

    return (jint) state;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1makeNewGame
( JNIEnv* env, jclass C, jint gamePtr, jobject j_gi, jobject j_util,
  jobject j_draw, jint gameID, jobject j_cp, jobject j_procs, 
  jbyteArray jDictBytes )
{
    XWJNI_START();
    CurGameInfo* gi = makeGI( MPPARM(mpool) env, j_gi );
    globals->gi = gi;
    XW_UtilCtxt* util = makeUtil( MPPARM(mpool) &state->env, j_util, gi, 
                                  globals );
    globals->util = util;
    DrawCtx* dctx = makeDraw( MPPARM(mpool) &state->env, j_draw );
    globals->dctx = dctx;
    CommonPrefs cp;
    (void)loadCommonPrefs( env, &cp, j_cp );

    XP_LOGF( "calling game_makeNewGame" );
    game_makeNewGame( MPPARM(mpool) &state->game, gi, util, dctx, gameID, 
                      &cp, NULL );

    DictionaryCtxt* dict = makeDict( MPPARM(mpool) env, jDictBytes );
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
    LOG_FUNC();
    JNIState* state = (JNIState*)gamePtr;
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = state->mpool;
#endif
    AndGlobals* globals = &state->globals;

    destroyGI( MPPARM(mpool) globals->gi );

    JNIEnv* oldEnv = state->env;
    state->env = env;
    game_dispose( &state->game );

    destroyDraw( globals->dctx );
    destroyUtil( globals->util );
    vtmgr_destroy( MPPARM(mpool) globals->vtMgr );

    state->env = oldEnv;
    XP_FREE( mpool, state );
    mpool_destroy( mpool );
    LOG_RETURN_VOID();
} /* game_dispose */

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1makeFromStream
( JNIEnv* env, jclass C, jint gamePtr, jbyteArray jstream, 
  jobject /*out*/jgi, jbyteArray jdict, jobject jutil, jobject jdraw, 
  jobject jcp, jobject jprocs )
{
    jboolean result;
    XWJNI_START();

    globals->gi = (CurGameInfo*)XP_CALLOC( mpool, sizeof(*globals->gi) );
    DictionaryCtxt* dict = makeDict( MPPARM(mpool) env, jdict );
    globals->util = makeUtil( MPPARM(mpool) &state->env, jutil, globals->gi, globals );
    globals->dctx = makeDraw( MPPARM(mpool) &state->env, jdraw );

    jbyte* jelems = (*env)->GetByteArrayElements( env, jstream, NULL );
    XWStreamCtxt* stream = mem_stream_make( MPPARM(mpool) globals->vtMgr,
                                            NULL, 0, NULL );
    int len = (*env)->GetArrayLength( env, jstream );
    stream_putBytes( stream, jelems, len );
    (*env)->ReleaseByteArrayElements( env, jstream, jelems, 0 );

    CommonPrefs cp;
    (void)loadCommonPrefs( env, &cp, jcp );
    result = game_makeFromStream( MPPARM(mpool) stream, &state->game, 
                                  globals->gi, dict, 
                                  globals->util, globals->dctx, &cp,
                                  NULL );
    stream_destroy( stream );

    if ( result ) {
        setJGI( env, jgi, globals->gi );
    } else {
        XP_LOGF( "%s: need to free stuff allocated above", __func__ );
    }

    XWJNI_END();
    return result;
} /* makeFromStream */

JNIEXPORT jbyteArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1saveToStream
( JNIEnv* env, jclass C, jint gamePtr, jobject jgi )
{
    jbyteArray result;
    XWJNI_START();

    CurGameInfo* gi = makeGI( MPPARM(mpool) env, jgi );
    XWStreamCtxt* stream = mem_stream_make( MPPARM(mpool) globals->vtMgr,
                                            NULL, 0, NULL );

    game_saveToStream( &state->game, gi, stream );
    destroyGI( MPPARM(mpool) gi );

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
(JNIEnv *env, jclass C, jint gamePtr, jint left, jint top, jboolean lefty )
{
    XWJNI_START();
    board_setPos( state->game.board, left, top, lefty );
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setScale
(JNIEnv *env, jclass C, jint gamePtr, jint hscale, jint vscale )
{
    XWJNI_START();
    board_setScale( state->game.board, hscale, vscale );
    XWJNI_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setShowColors
( JNIEnv *env, jclass C, jint gamePtr, jboolean on )
{
    XWJNI_START();
    board_setShowColors( state->game.board, on );
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
  jbooleanArray workRemains )
{
    jboolean result;
    XWJNI_START();
    XP_Bool tmpbool;
    result = board_requestHint( state->game.board, useLimits, &tmpbool );
    /* If passed need to do workRemains[0] = tmpbool */
    if ( workRemains ) {
        jboolean* jelems = (*env)->GetBooleanArrayElements(env, workRemains, NULL );
        *jelems = tmpbool;
        (*env)->ReleaseBooleanArrayElements( env, workRemains, jelems, 0 );
    }
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_timerFired
( JNIEnv* env, jclass C, jint gamePtr, jint why, jint when, jint handle )
{
    jboolean result;
    XWJNI_START();
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
    XWJNI_START();
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
    XWJNI_START();
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
    XWJNI_START();
    XWStreamCtxt* stream = and_empty_stream( MPPARM(mpool) globals );
    model_writeGameHistory( state->game.model, stream, state->game.server,
                            gameOver );
    result = streamToJString( MPPARM(mpool) env, stream );
    XWJNI_END();
    return result;
}
