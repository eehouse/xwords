
#ifndef _MAIN_H_
#define _MAIN_H_

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "game.h"

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

#endif
