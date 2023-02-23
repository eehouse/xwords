/* -*- compile-command: "cd ../wasm && make MEMDEBUG=TRUE install -j3"; -*- */
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

#ifndef _MAIN_H_
#define _MAIN_H_

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "game.h"
#include "dictmgr.h"

typedef struct _TimerState {
    void* closure;
    XWTimerReason why;
    XWTimerProc proc;
    time_t when;
} TimerState;

typedef XP_Bool (*IdleProc)(void* closure);
typedef void (*BinProc)(void* closure, const uint8_t* data, int len);
typedef void (*BoolProc)(void* closure, bool result);
typedef void (*StringProc)(void* closure, const char* str);
typedef void (*MsgProc)(void* closure, const char* topic,
                        const uint8_t* data, int len);

typedef struct GameState {
#ifdef DEBUG
    int _GUARD;
#endif
    struct GameState* next;
    struct Globals* globals;

    CurGameInfo gi;
    XWGame game;
    XW_UtilCtxt* util;
    XP_U16 saveToken;
    char gameName[32];
} GameState;

typedef struct Globals {
#ifdef DEBUG
    int _GUARD;
#endif
    SDL_Window* window;
    SDL_Renderer* renderer;
    GameState* games;
    GameState* curGame;         /* if non-null, ptr to the one of games that's visible */
    VTableMgr* vtMgr;
    XW_DUtilCtxt* dutil;
    DrawCtx* draw;
    TransportProcs transportProcs;
    CommonPrefs cp;
    DictMgrCtxt* dictMgr;

    int useWidth, useHeight;

    bool focussed;              /* window is in foreground */

    char playerName[32];

#ifdef MEM_DEBUG
    MPStatsBuf mpstats;
    MemPoolCtx* mpool;
#endif
} Globals;

#define GUARD_GLOB 0x76AB98CD
#define GUARD_GS 0x12347890

#define CAST_GLOB(typ, var, ptr) XP_ASSERT(((typ)(ptr))->_GUARD == GUARD_GLOB); typ var = (typ)(ptr)
#define CAST_GS(typ, var, ptr) XP_ASSERT(((typ)(ptr))->_GUARD == GUARD_GS); typ var = (typ)(ptr)

#define KEY_DICTS "dicts"

#define NULL_XWE ((XWEnv)-1)

void main_set_timer( GameState* gs, XWTimerReason why, XP_U16 when,
                     XWTimerProc proc, void* closure );
void main_clear_timer( GameState* gs, XWTimerReason why );

typedef void (*QueryProc)(void* closure, XP_Bool confirmed);
void main_query( GameState* gs, const XP_UCHAR* query,
                 QueryProc proc, void* closure );

void main_set_idle( GameState* gs, IdleProc proc, void* closure );

void main_alert( GameState* gs, const XP_UCHAR* msg );

void main_gameFromInvite( Globals* globals, const NetLaunchInfo* invite );
void main_onGameMessage( Globals* globals, XP_U32 gameID,
                         const CommsAddrRec* from, const XP_U8* buf,
                         XP_U16 len );
void main_onCtrlReceived( Globals* globals, const XP_U8* buf, XP_U16 len );
void main_onGameGone( Globals* globals, XP_U32 gameID );
XP_Bool main_haveGame( Globals* globals, XP_U32 gameID, XP_U8 channel );
void main_sendOnClose( XWStreamCtxt* stream, XWEnv env, void* closure );
void main_playerScoreHeld( GameState* gs, XP_U16 player );
void main_showGameOver( GameState* gs );
void main_showRemaining( GameState* gs );
void main_turnChanged(GameState* gs, int newTurn);
void main_chatReceived( GameState* gs, const char* msg );
void main_pickBlank( GameState* gs, int playerNum, int col, int row,
                     const char** tileFaces, int nTiles );
void main_updateScreen( GameState* gs );
void main_needDictForGame(GameState* gs, const char* lc, const XP_UCHAR* dictName);
bool main_getLocalName( Globals* globals, char* playerName, size_t buflen );

#endif
