// Copyright 2011 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdarg.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <emscripten.h>
#include <unistd.h>
#include <stdlib.h>

#include "game.h"
#include "mempool.h"

#include "wasmdraw.h"
#include "wasmutil.h"
#include "wasmdutil.h"
#include "wasmdict.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define WASM_BOARD_LEFT 0
#define WASM_HOR_SCORE_TOP 0
#define BDWIDTH 330
#define BDHEIGHT 330

typedef struct _Globals {
    SDL_Window* window;
    SDL_Renderer* renderer;
    XWGame game;
    CurGameInfo gi;
    VTableMgr* vtMgr;
    XW_DUtilCtxt* dutil;
    XW_UtilCtxt* util;
    DrawCtx* draw;
    DictionaryCtxt* dict;
    TransportProcs procs;
    CommonPrefs cp;

    MemPoolCtx* mpool;
} Globals;

static void
initGlobals( Globals* globals )
{
    globals->cp.showBoardArrow = XP_TRUE;

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
    globals->util = wasm_util_make( globals->mpool, &globals->gi, globals->dutil );
    globals->dict = wasm_load_dict( globals->mpool );

    globals->draw = wasm_draw_make( MPPARM(globals->mpool) globals->renderer );
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

int main( int argc, char** argv )
{
    LOG_FUNC();
    Globals globals = {0};
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    SDL_CreateWindowAndRenderer(600, 400, 0,
                                &globals.window, &globals.renderer);

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
    SDL_RenderPresent(globals.renderer);

    printf("you should see an image.\n");

    return 0;
}
