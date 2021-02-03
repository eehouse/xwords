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
#define BDWIDTH 400
#define BDHEIGHT 400

EM_JS(bool, call_confirm, (const char* str), {
        return confirm(UTF8ToString(str));
});
EM_JS(void, call_alert, (const char* str), {
        alert(UTF8ToString(str));
});

static Globals* sGlobals;

static void
initGlobals( Globals* globals )
{
    globals->cp.showBoardArrow = XP_TRUE;
    globals->cp.allowPeek = XP_TRUE;
    // globals->cp.showRobotScores = XP_TRUE;
    globals->cp.sortNewTiles = XP_TRUE;

    globals->gi.serverRole = SERVER_STANDALONE;
    globals->gi.phoniesAction = PHONIES_WARN;

    globals->gi.nPlayers = 2;
    globals->gi.boardSize = 15;
    globals->gi.dictName = "myDict";
    globals->gi.players[0].name = "You";
    globals->gi.players[0].isLocal = XP_TRUE;
    globals->gi.players[1].name = "Robot";
    globals->gi.players[1].isLocal = XP_TRUE;
    globals->gi.players[1].robotIQ = 99;

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
    XP_ASSERT( !globals->idleProc );
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

#ifdef NAKED_MODE
void mainf()
#else
int main( int argc, char** argv )
#endif
{
    LOG_FUNC();
    Globals globals = {0};
    sGlobals = &globals;
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

#ifndef NAKED_MODE
    return 0;
#endif
}
