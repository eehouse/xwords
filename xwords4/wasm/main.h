
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

    TimerState timers[NUM_TIMERS_PLUS_ONE];

    IdleProc idleProc;
    void* idleClosure;

    XP_U16 saveToken;

    MemPoolCtx* mpool;
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

#endif
