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
#define BDWIDTH 400
#define BDHEIGHT 400

#define KEY_GAME "the_game"

EM_JS(bool, call_confirm, (const char* str), {
        return confirm(UTF8ToString(str));
});
EM_JS(void, call_alert, (const char* str), {
        alert(UTF8ToString(str));
});

static Globals* sGlobals;

static void
initDeviceGlobals( Globals* globals )
{
    globals->cp.showBoardArrow = XP_TRUE;
    globals->cp.allowPeek = XP_TRUE;
    // globals->cp.showRobotScores = XP_TRUE;
    globals->cp.sortNewTiles = XP_TRUE;

    globals->mpool = mpool_make( "wasm" );
    globals->vtMgr = make_vtablemgr( globals->mpool );
    globals->dutil = wasm_dutil_make( globals->mpool, globals->vtMgr, globals );
    globals->dict = wasm_load_dict( globals->mpool );

    globals->draw = wasm_draw_make( MPPARM(globals->mpool)
                                    WINDOW_WIDTH, WINDOW_HEIGHT );

    MQTTDevID devID;
    dvc_getMQTTDevID( globals->dutil, NULL, &devID );
    XP_LOGFF( "got devID: %X", devID );
}

static void
makeAndDraw( Globals* globals, bool p0robot, bool p1robot )
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
    globals->gi.dictName = "myDict";
    globals->gi.players[0].name = "Player 1";
    globals->gi.players[0].isLocal = XP_TRUE;
    if ( p0robot ) {
        globals->gi.players[0].robotIQ = 99;
    }
    globals->gi.players[1].name = "Player 2";
    globals->gi.players[1].isLocal = XP_TRUE;
    if ( p1robot ) {
        globals->gi.players[1].robotIQ = 99;
    }

    globals->util = wasm_util_make( globals->mpool, &globals->gi,
                                    globals->dutil, globals );

    XP_LOGFF( "calling game_makeNewGame()" );
    game_makeNewGame( MPPARM(globals->mpool) NULL,
                      &globals->game, &globals->gi,
                      globals->util, globals->draw, 
                      &globals->cp, &globals->procs );

    BoardDims dims;
    board_figureLayout( globals->game.board, NULL, &globals->gi,
                        WASM_BOARD_LEFT, WASM_HOR_SCORE_TOP, BDWIDTH, BDHEIGHT,
                        110, 150, 200, BDWIDTH-25, 16, 16, XP_FALSE, &dims );
    board_applyLayout( globals->game.board, NULL, &dims );

    model_setDictionary( globals->game.model, NULL, globals->dict );

    (void)server_do( globals->game.server, NULL ); /* assign tiles, etc. */
    board_draw( globals->game.board, NULL );
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
main_set_timer( Globals* globals, XWTimerReason why, XP_U16 when,
                XWTimerProc proc, void* closure )
{
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
    SDL_RenderClear( globals->renderer );
    board_draw( globals->game.board, NULL );
    wasm_draw_render( globals->draw, globals->renderer );
    SDL_RenderPresent( globals->renderer );

    /* Let's save state here too, though likely too often */
    XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(globals->mpool)
                                                globals->vtMgr );
    game_saveToStream( &globals->game, NULL, &globals->gi,
                        stream, ++globals->saveToken );
    dutil_storeStream( globals->dutil, NULL, KEY_GAME, stream );
    stream_destroy( stream, NULL );
    game_saveSucceeded( &globals->game, NULL, globals->saveToken );
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

#ifdef NAKED_MODE
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
#endif

static Globals*
initOnce()
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

    return globals;
}

#ifdef NAKED_MODE
void
newgame(bool p0, bool p1)
{
    XP_LOGFF( "(args: %d,%d)", p0, p1 );
    if ( !!sGlobals ) {
        makeAndDraw( sGlobals, p0, p1 );
    }
}

void
mainf()
{
    LOG_FUNC();
    Globals* globals = initOnce();
    emscripten_set_main_loop_arg( looper, globals, -1, 1 );
}
#else
int
main( int argc, char** argv )
{
    LOG_FUNC();
    Globals* globals = initOnce();
    makeAndDraw( globals, false, true );

    /**
     * Show what is in the renderer
     */
    /* wasm_draw_render( globals.draw, globals.renderer ); */
    /* SDL_RenderPresent( globals.renderer ); */

    emscripten_set_main_loop_arg( looper, globals, -1, 1 );

    return 0;
}
#endif
