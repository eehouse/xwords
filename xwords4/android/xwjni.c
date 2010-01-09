/* -*-mode: C; compile-command: "cd XWords4; ../scripts/ndkbuild.sh"; -*- */
#include <string.h>
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


/* static JNIEnv* g_env = NULL; */

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

            getString( env, jlp, "name", &buf, VSIZE(buf) );
            lp->name = copyString( mpool, buf );
            getString( env, jlp, "password", &buf, VSIZE(buf) );
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
    LOG_FUNC();
    bool success = getBool( env, j_cp, "showBoardArrow", &cp->showBoardArrow )
        && getBool( env, j_cp, "showRobotScores", &cp->showRobotScores )
        && getBool( env, j_cp, "hideTileValues", &cp->hideTileValues )
        && getBool( env, j_cp, "skipCommitConfirm", &cp->skipCommitConfirm );
    return success;
}

typedef struct _GameAndMPool {
    XWGame game;
    AndGlobals* globals;
    MPSLOT
} GameAndMPool;

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_initJNI
( JNIEnv* env, jclass C )
{
    MemPoolCtx* mpool = mpool_make();
    GameAndMPool* game = (GameAndMPool*)XP_CALLOC( mpool, sizeof(*game) );
    AndGlobals* globals = XP_MALLOC( mpool, sizeof( *globals ) );
    game->mpool = mpool;
    game->globals = globals;
    globals->vtMgr = make_vtablemgr(MPPARM_NOCOMMA(mpool));

    return (jint) game;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1makeNewGame
( JNIEnv* env, jclass C, jint gamePtr, jobject j_gi, jobject j_util,
  jobject j_draw, jint gameID, jobject j_cp, jobject j_procs, 
  jbyteArray jDictBytes )
{
    LOG_FUNC();
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    MemPoolCtx* mpool = game->mpool;
    AndGlobals* globals = game->globals;

    CurGameInfo* gi = makeGI( MPPARM(mpool) env, j_gi );
    globals->gi = gi;
    XW_UtilCtxt* util = makeUtil( MPPARM(mpool) env, j_util, gi, globals );
    globals->util = util;
    DrawCtx* dctx = makeDraw( MPPARM(mpool) env, j_draw );
    globals->dctx = dctx;
    CommonPrefs cp;
    (void)loadCommonPrefs( env, &cp, j_cp );
#ifndef XWFEATURE_STANDALONE_ONLY
    /* TransportProcs proc; */
    /* loadTransportProcs( &procs, j_procs ); */
#endif

    XP_LOGF( "calling game_makeNewGame" );
    game_makeNewGame( MPPARM(mpool) &game->game, gi, util, dctx, gameID, 
                      &cp, NULL );

    DictionaryCtxt* dict = makeDict( MPPARM(mpool) env, jDictBytes );
#ifdef STUBBED_DICT
    if ( !dict ) {
        XP_LOGF( "falling back to stubbed dict" );
        dict = make_stubbed_dict( mpool );
    }
#endif
    XP_ASSERT( !!dict );
    model_setDictionary( game->game.model, dict );
} /* makeNewGame */

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1makeFromStream
( JNIEnv* env, jclass C, jint gamePtr, jbyteArray jstream, 
  jobject /*out*/jgi, jbyteArray jdict, jobject jutil, jobject jdraw, 
  jobject jcp, jobject jprocs )
{
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    MemPoolCtx* mpool = game->mpool;
    AndGlobals* globals = game->globals;

    globals->gi = (CurGameInfo*)XP_CALLOC( mpool, sizeof(*globals->gi) );
    DictionaryCtxt* dict = makeDict( MPPARM(mpool) env, jdict );
    globals->util = makeUtil( MPPARM(mpool) env, jutil, globals->gi, globals );
    globals->dctx = makeDraw( MPPARM(mpool) env, jdraw );

    jbyte* jelems = (*env)->GetByteArrayElements( env, jstream, NULL );
    XWStreamCtxt* stream = mem_stream_make( game->mpool, globals->vtMgr,
                                            NULL, 0, NULL );
    int len = (*env)->GetArrayLength( env, jstream );
    XP_LOGF( "putting %d bytes into stream", len );
    stream_putBytes( stream, jelems, len );
    (*env)->ReleaseByteArrayElements( env, jstream, jelems, 0 );

    CommonPrefs cp;
    (void)loadCommonPrefs( env, &cp, jcp );
    XP_Bool result = game_makeFromStream( mpool, stream, &game->game, 
                                          globals->gi, dict, 
                                          globals->util, globals->dctx, &cp,
                                          NULL );
    stream_destroy( stream );

    setJGI( env, jgi, globals->gi );

    LOG_RETURNF( "%s", result?"success":"failure" );
    return result;
} /* makeFromStream */

JNIEXPORT jbyteArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1saveToStream
( JNIEnv* env, jclass C, jint gamePtr, jobject jgi )
{
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    AndGlobals* globals = game->globals;

    CurGameInfo* gi = makeGI( MPPARM(game->mpool) env, jgi );
    XWStreamCtxt* stream = mem_stream_make( game->mpool, globals->vtMgr,
                                            NULL, 0, NULL );
    game_saveToStream( &game->game, gi, stream );
    destroyGI( MPPARM(game->mpool) gi );

    int nBytes = stream_getSize( stream );
    jbyteArray jarr = (*env)->NewByteArray( env, nBytes );
    jbyte* jelems = (*env)->GetByteArrayElements( env, jarr, NULL );
    stream_getBytes( stream, jelems, nBytes );
    (*env)->ReleaseByteArrayElements( env, jarr, jelems, 0 );
    stream_destroy( stream );

    (*env)->DeleteLocalRef( env, jarr );
    return jarr;
}

JNIEXPORT void JNICALL Java_org_eehouse_android_xw4_jni_XwJNI_game_1dispose
( JNIEnv * env, jclass claz, jint gamePtr )
{
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    MemPoolCtx* mpool = game->mpool;
    AndGlobals* globals = game->globals;

    destroyGI( mpool, globals->gi );

    game_dispose( &game->game );

    destroyDraw( globals->dctx );
    destroyUtil( globals->util );
    vtmgr_destroy( mpool, globals->vtMgr );

    XP_FREE( mpool, globals );
    XP_FREE( mpool, game );
    mpool_destroy( mpool );
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1invalAll
( JNIEnv *env, jclass C, jint gamePtr )
{
    LOG_FUNC();
    XP_ASSERT( 0 != gamePtr );
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    BoardCtxt* board = game->game.board;
    board_invalAll( board );
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1draw
( JNIEnv *env, jclass C, jint gamePtr )
{
    XP_ASSERT( 0 != gamePtr );
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    BoardCtxt* board = game->game.board;
    XP_Bool success = board_draw( board );
    return (jboolean)success;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setPos
(JNIEnv *env, jclass C, jint gamePtr, jint left, jint top, jboolean lefty )
{
    LOG_FUNC();
    XP_ASSERT( 0 != gamePtr );
    XP_LOGF( "calling setPos(%d,%d)", left, top );
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    BoardCtxt* board = game->game.board;
    board_setPos( board, left, top, lefty );
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setScale
(JNIEnv *env, jclass C, jint gamePtr, jint hscale, jint vscale )
{
    XP_LOGF( "calling setScale(%d,%d)", hscale, vscale );
    XP_ASSERT( 0 != gamePtr );
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    BoardCtxt* board = game->game.board;
    board_setScale( board, hscale, vscale );
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setShowColors
( JNIEnv *env, jclass C, jint gamePtr, jboolean on )
{
    LOG_FUNC();
    XP_ASSERT( 0 != gamePtr );
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    BoardCtxt* board = game->game.board;
    board_setShowColors( board, on );
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setScoreboardLoc
( JNIEnv *env, jclass C, jint gamePtr, jint left, jint top, 
  jint width, jint height, jboolean divideHorizontally )
{
    XP_ASSERT( 0 != gamePtr );
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    BoardCtxt* board = game->game.board;
    XP_LOGF( "calling setScoreboardLoc(%d,%d,%d,%d,%d)", left, top, 
             width, height, divideHorizontally );
    board_setScoreboardLoc( board, left, top, width, height, divideHorizontally );
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setTrayLoc
( JNIEnv *env, jclass C, jint gamePtr, jint left, jint top, 
  jint width, jint height, jint minDividerWidth )
{
    XP_LOGF( "calling setTrayLoc(%d,%d,%d,%d,%d)", left, top, 
             width, height, minDividerWidth );
    XP_ASSERT( 0 != gamePtr );
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    BoardCtxt* board = game->game.board;
    board_setTrayLoc( board, left, top, width, height, minDividerWidth );
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1handlePenDown
(JNIEnv *env, jclass C, jint gamePtr, jint xx, jint yy, jbooleanArray barray )
{
    XP_ASSERT( 0 != gamePtr );
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    BoardCtxt* board = game->game.board;
    XP_Bool bb;                 /* drop this for now */
    return board_handlePenDown( board, xx, yy, &bb );
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1handlePenMove
( JNIEnv *env, jclass C, jint gamePtr, jint xx, jint yy )
{
    XP_ASSERT( 0 != gamePtr );
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    BoardCtxt* board = game->game.board;
    return board_handlePenMove( board, xx, yy );
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1handlePenUp
( JNIEnv *env, jclass C, jint gamePtr, jint xx, jint yy )
{
    XP_ASSERT( 0 != gamePtr );
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    BoardCtxt* board = game->game.board;
    return board_handlePenUp( board, xx, yy );
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1juggleTray
(JNIEnv* env, jclass C, jint gamePtr )
{
    XP_ASSERT( 0 != gamePtr );
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    BoardCtxt* board = game->game.board;
    return board_juggleTray( board );
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1getTrayVisState
(JNIEnv* env, jclass C, jint gamePtr)
{
    XP_ASSERT( 0 != gamePtr );
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    BoardCtxt* board = game->game.board;
    return board_getTrayVisState( board );
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1hideTray
(JNIEnv* env, jclass C, jint gamePtr)
{
    XP_ASSERT( 0 != gamePtr );
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    BoardCtxt* board = game->game.board;
    return board_hideTray( board );
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1showTray
(JNIEnv* env, jclass C, jint gamePtr)
{
    XP_ASSERT( 0 != gamePtr );
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    BoardCtxt* board = game->game.board;
    return board_showTray( board );
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1commitTurn
(JNIEnv* env, jclass C, jint gamePtr)
{
    XP_ASSERT( 0 != gamePtr );
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    BoardCtxt* board = game->game.board;
    return board_commitTurn( board );
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1flip
(JNIEnv* env, jclass C, jint gamePtr)
{
    XP_ASSERT( 0 != gamePtr );
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    BoardCtxt* board = game->game.board;
    return board_flip( board );
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1replaceTiles
(JNIEnv* env, jclass C, jint gamePtr)
{
    XP_ASSERT( 0 != gamePtr );
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    BoardCtxt* board = game->game.board;
    return board_replaceTiles( board );
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1handleUndo
(JNIEnv* env, jclass C, jint gamePtr)
{
    XP_ASSERT( 0 != gamePtr );
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    ServerCtxt* server = game->game.server;
    server_handleUndo( server );
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1do
(JNIEnv* env, jclass C, jint gamePtr )
{
    XP_ASSERT( 0 != gamePtr );
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    ServerCtxt* server = game->game.server;
    return server_do( server );
}

JNIEXPORT void JNICALL
org_eehouse_android_xw4_jni_XwJNI_board_1resetEngine
(JNIEnv* env, jclass C, jint gamePtr )
{
    XP_ASSERT( 0 != gamePtr );
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    BoardCtxt* board = game->game.board;
    board_resetEngine( board );
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1requestHint
( JNIEnv* env, jclass C, jint gamePtr, jboolean useLimits, 
  jbooleanArray workRemains )
{
    XP_ASSERT( 0 != gamePtr );
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    BoardCtxt* board = game->game.board;

    XP_Bool tmpbool;
    jboolean result = board_requestHint( board, useLimits, &tmpbool );
 
    /* If passed need to do workRemains[0] = tmpbool */
    XP_ASSERT( !workRemains );
    
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_timerFired
( JNIEnv* env, jclass C, jint gamePtr, jint why, jint when, jint handle )
{
    XP_ASSERT( 0 != gamePtr );
    GameAndMPool* game = (GameAndMPool*)gamePtr;
    AndGlobals* globals = game->globals;
    XW_UtilCtxt* util = globals->util;
    return utilTimerFired( util, why, handle );
}
