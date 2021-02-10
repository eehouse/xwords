/* -*- compile-command: "cd ../wasm && make main.html -j3"; -*- */
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

#include <sys/time.h>
#include <stdio.h>
#include <stdarg.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <emscripten.h>
#include <unistd.h>
#include <stdlib.h>

#include "game.h"
#include "device.h"
#include "mempool.h"
#include "nli.h"
#include "strutils.h"

#include "main.h"
#include "wasmdraw.h"
#include "wasmutil.h"
#include "wasmdutil.h"
#include "wasmdict.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define WASM_BOARD_LEFT 0
#define WASM_HOR_SCORE_TOP 0

#define WINDOW_WIDTH 400
#define WINDOW_HEIGHT 600
#define BDWIDTH WINDOW_WIDTH
#define BDHEIGHT WINDOW_HEIGHT

#define KEY_GAME "the_game"
#define DICTNAME "assets_dir/CollegeEng_2to8.xwd"

EM_JS(bool, call_confirm, (const char* str), {
        return confirm(UTF8ToString(str));
});
EM_JS(void, call_alert, (const char* str), {
        alert(UTF8ToString(str));
});
EM_JS(void, call_haveDevID, (void* closure, const char* devid), {
        onHaveDevID(closure, UTF8ToString(devid));
});

EM_JS(void, call_mqttSend, (const char* topic, const uint8_t* ptr, int len), {
        let topStr = UTF8ToString(topic);
        let buffer = new Uint8Array(Module.HEAPU8.buffer, ptr, len);
        mqttSend(topStr, buffer);
});

static void updateScreen( Globals* globals );

static Globals* sGlobals;

static XP_S16
send_msg( XWEnv xwe, const XP_U8* buf, XP_U16 len,
          const XP_UCHAR* msgNo, const CommsAddrRec* addr,
          CommsConnType conType, XP_U32 gameID, void* closure )
{
    XP_S16 nSent = -1;
    LOG_FUNC();
    Globals* globals = (Globals*)closure;
    XP_ASSERT( globals == sGlobals );

    if ( addr_hasType( addr, COMMS_CONN_MQTT ) ) {
        MQTTDevID devID = addr->u.mqtt.devID;

        XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(globals->mpool)
                                                    globals->vtMgr );
        dvc_makeMQTTMessage( globals->dutil, NULL, stream,
                             gameID, buf, len );

        XP_UCHAR topic[64];
        formatMQTTTopic( &devID, topic, sizeof(topic) );

        XP_U16 streamLen = stream_getSize( stream );

        XP_LOGFF( "calling call_mqttSend" );
        call_mqttSend( topic, stream_getPtr( stream ), streamLen );
        XP_LOGFF( "back from call_mqttSend" );
        stream_destroy( stream, NULL );
        nSent = len;
    }

    LOG_RETURNF( "%d", nSent );
    return nSent;
}

static void
initDeviceGlobals( Globals* globals )
{
    globals->cp.showBoardArrow = XP_TRUE;
    globals->cp.allowPeek = XP_TRUE;
    // globals->cp.showRobotScores = XP_TRUE;
    globals->cp.sortNewTiles = XP_TRUE;

    globals->procs.send = send_msg;
    globals->procs.closure = globals;

    globals->mpool = mpool_make( "wasm" );
    globals->vtMgr = make_vtablemgr( globals->mpool );
    globals->dutil = wasm_dutil_make( globals->mpool, globals->vtMgr, globals );
    globals->dictMgr = dmgr_make( MPPARM_NOCOMMA(globals->mpool) );
    globals->dict = wasm_dictionary_make( MPPARM(globals->mpool) NULL,
                                          globals, DICTNAME, true );

    dict_ref( globals->dict, NULL );

    globals->draw = wasm_draw_make( MPPARM(globals->mpool)
                                    WINDOW_WIDTH, WINDOW_HEIGHT );

    MQTTDevID devID;
    dvc_getMQTTDevID( globals->dutil, NULL, &devID );
    XP_UCHAR buf[32];
    XP_SNPRINTF( buf, VSIZE(buf), MQTTDevID_FMT, devID );
    XP_LOGFF( "got mqtt devID: %s", buf );
    call_haveDevID( globals, buf );
}

static void
startGame( Globals* globals )
{
    LOG_FUNC();
    BoardDims dims;
    board_figureLayout( globals->game.board, NULL, &globals->gi,
                        WASM_BOARD_LEFT, WASM_HOR_SCORE_TOP, BDWIDTH, BDHEIGHT,
                        110, 150, 200, BDWIDTH-25, BDWIDTH/15, BDHEIGHT/15,
                        XP_FALSE, &dims );
    XP_LOGFF( "calling board_applyLayout" );
    board_applyLayout( globals->game.board, NULL, &dims );
    XP_LOGFF( "calling model_setDictionary" );
    model_setDictionary( globals->game.model, NULL, globals->dict );

    if ( SERVER_ISCLIENT == globals->gi.serverRole ) {
        server_initClientConnection( globals->game.server, NULL );
    }
    
    (void)server_do( globals->game.server, NULL ); /* assign tiles, etc. */
    if ( !!globals->game.comms ) {
        comms_resendAll( globals->game.comms, NULL, COMMS_CONN_MQTT, XP_TRUE );
    }

    updateScreen( globals );
    LOG_RETURN_VOID();
}

static void
makeAndDraw( Globals* globals, bool forceNew, bool p0robot, bool p1robot )
{
    if ( !!globals->util ) {
        game_dispose( &globals->game, NULL );
        wasm_util_destroy( globals->util );
        globals->util = NULL;
    }

    globals->gi.serverRole = SERVER_STANDALONE;
    globals->gi.phoniesAction = PHONIES_WARN;

    globals->gi.nPlayers = 2;
    globals->gi.boardSize = 15;
    // globals->gi.dictName = "myDict";
    globals->gi.players[0].name = copyString( globals->mpool, "Player 1" );
    globals->gi.players[0].isLocal = XP_TRUE;
    globals->gi.players[0].robotIQ = p0robot ? 99 : 0;

    globals->gi.players[1].name = copyString( globals->mpool, "Player 2" );
    globals->gi.players[1].isLocal = XP_TRUE;
    globals->gi.players[1].robotIQ = p1robot ? 99 : 0;

    globals->util = wasm_util_make( globals->mpool, &globals->gi,
                                    globals->dutil, globals );

    XP_Bool loaded = XP_FALSE;
    if ( ! forceNew ) {
        /* Let's see if there's a saved game */
        XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(globals->mpool)
                                                    globals->vtMgr );
        dutil_loadStream( globals->dutil, NULL, KEY_GAME, NULL, stream );
        if ( 0 < stream_getSize( stream ) ) {
            XP_LOGFF( "there's a saved game!!" );
            loaded = game_makeFromStream( MPPARM(globals->mpool) NULL, stream,
                                          &globals->game, &globals->gi,
                                          globals->dict, NULL,
                                          globals->util, globals->draw,
                                          &globals->cp, &globals->procs );

        }
        stream_destroy( stream, NULL );
    }

    if ( !loaded ) {
        XP_LOGFF( "calling game_makeNewGame()" );
        game_makeNewGame( MPPARM(globals->mpool) NULL,
                          &globals->game, &globals->gi,
                          globals->util, globals->draw,
                          &globals->cp, &globals->procs );
    }

    startGame( globals );
}

void
main_gameFromInvite( Globals* globals, const NetLaunchInfo* invite )
{
    LOG_FUNC();
    if ( ! call_confirm( "Invitation received; replace current game?" ) ) {
    } else if ( invite->lang != 1 ) {
        call_alert( "Invitation received, but only English supported now" );
    } else {
        if ( !!globals->util ) {
            XP_LOGFF( "calling game_dispose" );
            game_dispose( &globals->game, NULL );
            XP_LOGFF( "calling wasm_util_destroy" );
            wasm_util_destroy( globals->util );
            globals->util = NULL;
        }
        XP_LOGFF( "done with cleanup" ); /* not reaching this */
        
        XP_MEMSET( &globals->gi, 0, sizeof(globals->gi) );

        gi_setNPlayers( &globals->gi, invite->nPlayersT, invite->nPlayersH );
        // ensureLocalPlayerNames( params, &gi );

        globals->gi.boardSize = 15;
        globals->gi.gameID = invite->gameID;
        globals->gi.dictLang = invite->lang;
        globals->gi.forceChannel = invite->forceChannel;
        globals->gi.inDuplicateMode = invite->inDuplicateMode;
        globals->gi.serverRole = SERVER_ISCLIENT; /* recipient of invitation is client */
        replaceStringIfDifferent( globals->mpool, &globals->gi.dictName,
                                  invite->dict );

        globals->util = wasm_util_make( globals->mpool, &globals->gi,
                                        globals->dutil, globals );
        
        game_makeNewGame( MPPARM(globals->mpool) NULL,
                          &globals->game, &globals->gi,
                          globals->util, globals->draw,
                          &globals->cp, &globals->procs );

        CommsAddrRec returnAddr;
        nli_makeAddrRec( invite, &returnAddr );
        addr_setType( &returnAddr, COMMS_CONN_MQTT ); /* nuke everything else */
        comms_augmentHostAddr( globals->game.comms, NULL, &returnAddr );

        startGame( globals );
    }
    LOG_RETURN_VOID();
}

void
main_onGameMessage( Globals* globals, XP_U32 gameID,
                    const CommsAddrRec* from, XWStreamCtxt* stream )
{
    XP_Bool draw = game_receiveMessage( &globals->game, NULL, stream, from );
    if ( draw ) {
        updateScreen( globals );
    }
}

void
main_sendOnClose( XWStreamCtxt* stream, XWEnv env, void* closure )
{
    Globals* globals = (Globals*)closure;
    XP_LOGFF( "called with msg of len %d", stream_getSize(stream) );
    (void)comms_send( globals->game.comms, NULL, stream );
}

static time_t
getCurMS()
{
    struct timeval tv;
    gettimeofday( &tv, NULL );
    time_t result = tv.tv_sec * 1000; /* convert to millis */
    result += tv.tv_usec / 1000;         /* convert to millis too */
    // LOG_RETURNF( "%x", result );
    return result;
}

static XP_Bool
checkForTimers( Globals* globals )
{
    XP_Bool draw = XP_FALSE;
    time_t now = getCurMS();
    for ( XWTimerReason why = 0; why < NUM_TIMERS_PLUS_ONE; ++why ) {
        TimerState* timer = &globals->timers[why];
        XWTimerProc proc = timer->proc;
        if ( !!proc && now >= timer->when ) {
            timer->proc = NULL;
            (*proc)( timer->closure, NULL, why );
            draw = XP_TRUE;     /* just in case */
        }
    }
    return draw;
}

static XP_Bool
checkForIdle( Globals* globals )
{
    XP_Bool draw = XP_FALSE;
    IdleProc proc = globals->idleProc;
    if ( !!proc ) {
        globals->idleProc = NULL;
        draw = (*proc)(globals->idleClosure);
    }
    return draw;
}

void
main_clear_timer( Globals* globals, XWTimerReason why )
{
    XP_LOGFF( "why: %d" );
}

void
main_set_timer( Globals* globals, XWTimerReason why, XP_U16 when,
                XWTimerProc proc, void* closure )
{
    XP_LOGFF( "why: %d" );
    /* TimerState* timer = &globals->timers[why]; */
    /* timer->proc = proc; */
    /* timer->closure = closure; */

    /* time_t now = getCurMS(); */
    /* timer->when = now + (1000 * when); */
}

void
main_query( Globals* globals, const XP_UCHAR* query, QueryProc proc, void* closure )
{
    bool ok = call_confirm( query );
    (*proc)( closure, ok );
}

void
main_alert( Globals* globals, const XP_UCHAR* msg )
{
    call_alert( msg );
}

void
main_set_idle( Globals* globals, IdleProc proc, void* closure )
{
    XP_ASSERT( !globals->idleProc || globals->idleProc == proc );
    globals->idleProc = proc;
    globals->idleClosure = closure;
}

static XP_Bool
checkForEvent( Globals* globals )
{
    XP_Bool handled;
    XP_Bool draw = XP_FALSE;
    BoardCtxt* board = globals->game.board;

    SDL_Event event;
    if ( SDL_PollEvent(&event) ) {
        switch ( event.type ) {
        case SDL_MOUSEBUTTONDOWN:
            draw = event.button.button == SDL_BUTTON_LEFT
                && board_handlePenDown( board, NULL,
                                        event.button.x, event.button.y,
                                        &handled );
            break;
        case SDL_MOUSEBUTTONUP:
            draw = event.button.button == SDL_BUTTON_LEFT
                && board_handlePenUp( board, NULL,
                                      event.button.x, event.button.y );
            break;
        case SDL_MOUSEMOTION:
            draw = board_handlePenMove( board, NULL,
                                        event.motion.x, event.motion.y );
            break;
        default:
            break;
        }
    }

    // XP_LOGFF( "draw: %d", draw );
    return draw;
}

static void
updateScreen( Globals* globals )
{
    LOG_FUNC();
    SDL_RenderClear( globals->renderer );
    XP_LOGFF( "calling board_draw" );
    board_draw( globals->game.board, NULL );
    XP_LOGFF( "calling wasm_draw_render" );
    wasm_draw_render( globals->draw, globals->renderer );
    XP_LOGFF( "calling SDL_RenderPresent" );
    SDL_RenderPresent( globals->renderer );

    /* Let's save state here too, though likely too often */
    XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(globals->mpool)
                                                globals->vtMgr );
    XP_LOGFF( "calling game_saveToStream" );
    game_saveToStream( &globals->game, NULL, &globals->gi,
                        stream, ++globals->saveToken );
    XP_LOGFF( "calling dutil_storeStream" );
    dutil_storeStream( globals->dutil, NULL, KEY_GAME, stream );
    stream_destroy( stream, NULL );
    XP_LOGFF( "calling game_saveSucceeded" );
    game_saveSucceeded( &globals->game, NULL, globals->saveToken );
    LOG_RETURN_VOID();
}

static void
looper( void* closure )
{
    Globals* globals = (Globals*)closure;
    XP_Bool draw = checkForTimers( globals );
    draw = checkForIdle( globals ) || draw;
    draw = checkForEvent( globals ) || draw;

    if ( draw ) {
        updateScreen( globals );
    }
}

void
button( const char* msg )
{
    XP_Bool draw = XP_FALSE;
    Globals* globals = sGlobals;
    BoardCtxt* board = globals->game.board;
    XP_Bool redo;

    if ( 0 == strcmp(msg, "hintdown") ) {
        draw = board_requestHint( board, NULL, XP_TRUE, &redo );
    } else if ( 0 == strcmp(msg, "hintup") ) {
        draw = board_requestHint( board, NULL, XP_FALSE, &redo );
    } else if ( 0 == strcmp(msg, "flip") ) {
        draw = board_flip( board );
    } else if ( 0 == strcmp(msg, "redo") ) {
        draw = board_redoReplacedTiles( board, NULL )
            || board_replaceTiles( board, NULL );
    } else if ( 0 == strcmp(msg, "vals") ) {
        draw = board_toggle_showValues( board );
    }

    if ( draw ) {
        updateScreen( globals );
    }
}

static void
initNoReturn()
{
    time_t now = getCurMS();
    srandom( now );
    XP_LOGFF( "called(srandom( %x )", now );

    Globals* globals = calloc(1, sizeof(*globals));
    sGlobals = globals;

    SDL_Init( SDL_INIT_EVENTS );
    TTF_Init();

    SDL_CreateWindowAndRenderer( WINDOW_WIDTH, WINDOW_HEIGHT, 0,
                                 &globals->window, &globals->renderer );

    /* whip the canvas to background */
    SDL_SetRenderDrawColor( globals->renderer, 155, 155, 155, 255 );
    SDL_RenderClear( globals->renderer );

    initDeviceGlobals( globals );

    makeAndDraw( globals, false, false, true );

    emscripten_set_main_loop_arg( looper, globals, -1, 1 );
}

void
newgame(bool p0, bool p1)
{
    XP_LOGFF( "(args: %d,%d)", p0, p1 );
    XP_ASSERT( !!sGlobals );
    makeAndDraw( sGlobals, true, p0, p1 );
}

void
gotMQTTMsg( void* closure, int len, const uint8_t* msg )
{
    XP_LOGFF( "got msg of len %d (%p vs %p)", len, closure, sGlobals );
    Globals* globals = (Globals*)closure;
    dvc_parseMQTTPacket( globals->dutil, NULL, msg, len );
}

int
main( int argc, char** argv )
{
    XP_LOGFF( "(argc=%d)", argc );
    for ( int ii = 0; ii < argc; ++ii ) {
        XP_LOGFF( "arg[%d]: %s", ii, argv[ii] );
    }
    initNoReturn();
    return 0;
}
