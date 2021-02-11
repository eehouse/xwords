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
    DictMgrCtxt* dictMgr;

    XP_U16 saveToken;

#ifdef MEM_DEBUG
    MemPoolCtx* mpool;
#endif
} Globals;

void main_set_timer( Globals* globals, XWTimerReason why, XP_U16 when,
                     XWTimerProc proc, void* closure );
void main_clear_timer( Globals* globals, XWTimerReason why );

typedef void (*QueryProc)(void* closure, XP_Bool confirmed);
void main_query( Globals* globals, const XP_UCHAR* query,
                 QueryProc proc, void* closure );

void main_set_idle( Globals* globals, IdleProc proc, void* closure );

void main_alert( Globals* globals, const XP_UCHAR* msg );

void main_gameFromInvite( Globals* globals, const NetLaunchInfo* invite );
void main_onGameMessage( Globals* globals, XP_U32 gameID,
                         const CommsAddrRec* from, XWStreamCtxt* stream );

void main_sendOnClose( XWStreamCtxt* stream, XWEnv env, void* closure );
void main_playerScoreHeld( Globals* globals, XP_U16 player );

#endif
