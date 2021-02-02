// Copyright 2011 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

#include <sys/time.h>
#include <stdio.h>
#include <stdarg.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <emscripten.h>
#include <unistd.h>
#include <stdlib.h>

#include "game.h"
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
#define BDWIDTH 330
#define BDHEIGHT 330

static void
initGlobals( Globals* globals )
{
    globals->cp.showBoardArrow = XP_TRUE;
    globals->cp.allowPeek = XP_TRUE;

    globals->gi.serverRole = SERVER_STANDALONE;
    globals->gi.nPlayers = 2;
    globals->gi.boardSize = 15;
    globals->gi.dictName = "myDict";
    globals->gi.players[0].name = "Eric";
    globals->gi.players[0].isLocal = XP_TRUE;
    globals->gi.players[1].name = "Kati";
    globals->gi.players[1].isLocal = XP_TRUE;

    globals->mpool = mpool_make( "wasm" );
    globals->vtMgr = make_vtablemgr( globals->mpool );
    globals->dutil = wasm_dutil_make( globals->mpool, globals->vtMgr, globals );
    globals->util = wasm_util_make( globals->mpool, &globals->gi,
                                    globals->dutil, globals );
    globals->dict = wasm_load_dict( globals->mpool );

    globals->draw = wasm_draw_make( MPPARM(globals->mpool)
                                    WINDOW_WIDTH, WINDOW_HEIGHT );
}

static void
makeAndDraw( Globals* globals )
{
    XP_LOGFF( "calling game_makeNewGame()" );
    game_makeNewGame( MPPARM(globals->mpool) NULL,
                      &globals->game, &globals->gi,
                      globals->util, globals->draw, 
                      &globals->cp, &globals->procs );

    XP_LOGFF( "calling board_figureLayout()" );
    BoardDims dims;
    board_figureLayout( globals->game.board, NULL, &globals->gi,
                        WASM_BOARD_LEFT, WASM_HOR_SCORE_TOP, BDWIDTH, BDHEIGHT,
                        110, 150, 200, BDWIDTH-25, 16, 16, XP_FALSE, &dims );
    XP_LOGFF( "calling board_applyLayout()" );
    board_applyLayout( globals->game.board, NULL, &dims );
    XP_LOGFF( "calling board_draw()" );

    model_setDictionary( globals->game.model, NULL, globals->dict );
    // model_setSquareBonuses( globals->game.model, XWBonusType* bonuses, XP_U16 nBonuses )

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

static void
checkForTimers( Globals* globals )
{
    time_t now = getCurMS();
    for ( XWTimerReason why = 0; why < NUM_TIMERS_PLUS_ONE; ++why ) {
        TimerState* timer = &globals->timers[why];
        XWTimerProc proc = timer->proc;
        if ( !!proc && now >= timer->when ) {
            timer->proc = NULL;
            XP_LOGFF( "timer fired (why=%d): calling proc", why );
            (*proc)( timer->closure, NULL, why );
            XP_LOGFF( "back from proc" );
        }
    }
}

void
main_set_timer( Globals* globals, XWTimerReason why, XP_U16 when,
                XWTimerProc proc, void* closure )
{
    TimerState* timer = &globals->timers[why];
    timer->proc = proc;
    timer->closure = closure;

    time_t now = getCurMS();
    timer->when = now + (1000 * when);
}

static void
checkForEvent( Globals* globals )
{
    XP_Bool handled;
    XP_Bool draw = XP_FALSE;
    SDL_Event event;
    if ( SDL_PollEvent(&event) ) {
        switch ( event.type ) {
        case SDL_MOUSEBUTTONDOWN:
            draw = event.button.button == SDL_BUTTON_LEFT
                && board_handlePenDown( globals->game.board, NULL,
                                        event.button.x, event.button.y,
                                        &handled );
            break;
        case SDL_MOUSEBUTTONUP:
            draw = event.button.button == SDL_BUTTON_LEFT
                && board_handlePenUp( globals->game.board, NULL,
                                      event.button.x, event.button.y );
            break;
            // SDL_MouseButtonEvent
        default:
            break;
        }
    }

    // XP_LOGFF( "draw: %d", draw );
    if ( draw ) {
        SDL_RenderClear( globals->renderer );
        board_draw( globals->game.board, NULL );
        wasm_draw_render( globals->draw, globals->renderer );
        SDL_RenderPresent( globals->renderer );
    }
}

static void
looper( void* closure )
{
    Globals* globals = (Globals*)closure;
    checkForTimers( globals );
    checkForEvent( globals );
}

int main( int argc, char** argv )
{
    LOG_FUNC();
    Globals globals = {0};
    SDL_Init( SDL_INIT_EVENTS );
    TTF_Init();
    int foo = SDL_SWSURFACE;

    // Do I want SDL_CreateWindow() plus something else?

    
    SDL_CreateWindowAndRenderer( WINDOW_WIDTH, WINDOW_HEIGHT, 0,
                                 &globals.window, &globals.renderer );

    /**
     * Set up a white background
     */
    SDL_SetRenderDrawColor(globals.renderer, 255, 255, 50, 50);
    SDL_RenderClear(globals.renderer);

    initGlobals( &globals );
    makeAndDraw( &globals );

    /**
     * Show what is in the renderer
     */
    wasm_draw_render( globals.draw, globals.renderer );
    SDL_RenderPresent( globals.renderer );

    emscripten_set_main_loop_arg( looper, &globals, -1, 1 );

    return 0;
}
