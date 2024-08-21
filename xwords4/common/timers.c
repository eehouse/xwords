/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2024 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "timers.h"
#include "xwmutex.h"
#include "dllist.h"

#ifdef DUTIL_TIMERS
typedef struct _TimerState {
    DLHead links;
    TimerProc proc;
    void* closure;
    TimerKey key;
    XP_U32 inWhen;
} TimerState;

typedef struct _TimerMgrState {
    XW_DUtilCtxt* dutil;
    MutexState mutex;
    XP_U32 nextKey;
    TimerState* timers;
} TimerMgrState;

static void clearPendings( XW_DUtilCtxt* dutil, XWEnv xwe );

void
tmr_init( XW_DUtilCtxt* dutil )
{
    LOG_FUNC();
    XP_ASSERT( !dutil->timersState );
    TimerMgrState* tms = XP_CALLOC( dutil->mpool, sizeof(*tms) );
    tms->dutil = dutil;
    dutil->timersState = tms;
    MUTEX_INIT( &tms->mutex, XP_TRUE );
}

void
tmr_cleanup( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    TimerMgrState* timersState = (TimerMgrState*)dutil->timersState;
    XP_ASSERT( !!timersState );
    clearPendings( dutil, xwe );
    MUTEX_DESTROY( &timersState->mutex );
    XP_FREEP( dutil->mpool, &dutil->timersState );
}

TimerKey
tmr_set( XW_DUtilCtxt* dutil, XWEnv xwe, XP_U32 inWhen,
         TimerProc proc, void* closure )
{
    TimerKey key;
    TimerMgrState* timersState = (TimerMgrState*)dutil->timersState;
    XP_ASSERT( !!timersState );

    TimerState* ts = XP_CALLOC( dutil->mpool, sizeof(*ts) );
    XP_LOGFF( "allocated timer %p", ts );
    ts->proc = proc;
    ts->closure = closure;
    ts->inWhen = inWhen;
    
    WITH_MUTEX( &timersState->mutex );
    key = ts->key = ++timersState->nextKey;
    timersState->timers = (TimerState*)
        dll_insert( &timersState->timers->links, &ts->links, NULL );
    END_WITH_MUTEX();

    dutil_setTimer( dutil, xwe, inWhen, ts->key );
    return key;
}

static void
timerFired( XW_DUtilCtxt* XP_UNUSED_DBG(dutil), XWEnv xwe, TimerState* timer,
            XP_Bool fired )
{
    XP_LOGFF( "(timer=%p); key=%d", timer, timer->key );
    (*timer->proc)( timer->closure, xwe, fired );
    XP_FREE( dutil->mpool, timer );
}

typedef struct _FindByKeyState {
    TimerKey key;
    TimerState* found;
} FindByKeyState;

static ForEachAct
findByKeyProc( const DLHead* elem, void* closure )
{
    ForEachAct result = FEA_OK;
    FindByKeyState* fbksp = (FindByKeyState*)closure;
    TimerState* ts = (TimerState*)elem;
    if ( ts->key == fbksp->key ) {
        fbksp->found = ts;
        result = FEA_REMOVE | FEA_EXIT;
    }
    return result;
}

void
tmr_fired( XW_DUtilCtxt* dutil, XWEnv xwe, TimerKey key )
{
    XP_LOGFF( "(key=%d)", key );
    TimerMgrState* timersState = (TimerMgrState*)dutil->timersState;
    XP_ASSERT( !!timersState );

    FindByKeyState fbks = { .key = key };

    WITH_MUTEX( &timersState->mutex );
    timersState->timers = (TimerState*)
        dll_map( &timersState->timers->links, findByKeyProc, NULL, &fbks );
    END_WITH_MUTEX();

    if ( !!fbks.found ) {
        timerFired( dutil, xwe, fbks.found, XP_TRUE );
    } else {
        XP_LOGFF( "no timer found for key %d", key );
    }
}

typedef struct _FireAndDisposeState {
    XW_DUtilCtxt* dutil;
    XWEnv xwe;
} FireAndDisposeState;

static void
fireAndDispose( DLHead* elem, void* closure )
{
    TimerState* ts = (TimerState*)elem;
    FireAndDisposeState* fadsp = (FireAndDisposeState*)closure;
    dutil_clearTimer( fadsp->dutil, fadsp->xwe, ts->key );
    timerFired( fadsp->dutil, fadsp->xwe, ts, XP_FALSE ); /* disposes ts */
}

static void
clearPendings( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    TimerMgrState* timersState = (TimerMgrState*)dutil->timersState;
    TimerState* timers;

    /* Grab the list so we can run over it without a mutex */
    WITH_MUTEX( &timersState->mutex );
    timers = timersState->timers;
    timersState->timers = NULL;
    END_WITH_MUTEX();

    FireAndDisposeState fads = {
        .xwe = xwe,
        .dutil = dutil,
    };
    dll_removeAll( &timers->links, fireAndDispose, &fads );
}

#endif
