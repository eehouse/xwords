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

#include "xwmutex.h"

#ifdef DEBUG

#define WAIT_ALL_SECS 3

typedef struct _WaitState {
    const char* caller;
    XP_U16 waitSecs;
} WaitState;

static void*
checkLockProc(void* closure)
{
    WaitState* wsp = (WaitState*)closure;
    sleep( wsp->waitSecs );
    XP_LOGFF( "failed to get mutex in %d secs (caller: %s())",
              wsp->waitSecs, wsp->caller );
    XP_ASSERT(0);
    return NULL;
}

void
mtx_lock_prv( MutexState* state, XP_U16 waitSecs, const char* caller )
{
    // XP_LOGFF( "(caller=%s, waitSecs=%d)", caller, waitSecs );
    pthread_t waitThread;
    WaitState ws = {
        .waitSecs = 0 < waitSecs ? waitSecs : state->waitSecs,
        .caller = caller,
    };
    if ( 0 < waitSecs ) {
        (void)pthread_create( &waitThread, NULL, checkLockProc, &ws );
    }
    pthread_mutex_lock( &state->mutex );
    if ( 0 < waitSecs ) {
        (void)pthread_cancel(waitThread);
    }
}

void
mtx_unlock_prv( MutexState* state, XP_U16 XP_UNUSED(waitSecs),
                const char* XP_UNUSED(caller) )
{
    // XP_LOGFF( "(caller=%s, waitSecs=%d)", caller, waitSecs );
    pthread_mutex_unlock( &state->mutex );
}
#endif

void
mtx_init_prv( MutexState* mutex, XP_Bool recursive
#ifdef DEBUG
          , XP_U16 waitSecs
#endif
          )
{
    pthread_mutexattr_t attr;
#ifdef DEBUG
    int ret =
#endif
        pthread_mutexattr_init(&attr);
    XP_ASSERT(0 == ret);
    if ( recursive ) {
#ifdef DEBUG
        ret =
#endif
            pthread_mutexattr_settype(&attr,
                                        PTHREAD_MUTEX_RECURSIVE);
        XP_ASSERT(0 == ret);
    }
#ifdef DEBUG
# ifdef WAIT_ALL_SECS
    if ( waitSecs < WAIT_ALL_SECS ) {
        waitSecs = WAIT_ALL_SECS;
    }
# endif
    mutex->waitSecs = waitSecs;
#endif
    pthread_mutex_init( &mutex->mutex, &attr );
#ifdef DEBUG
    ret =
#endif
        pthread_mutexattr_destroy(&attr);
    XP_ASSERT(0 == ret);

/* #ifdef DEBUG */
/*     if ( recursive ) { */
/*         XP_LOGFF( "testing recursive call..." ); */
/*         WITH_MUTEX( mutex ); */
/*         WITH_MUTEX( mutex ); */
/*         END_WITH_MUTEX(); */
/*         END_WITH_MUTEX(); */
/*     } */
/* #endif */
}

void
mtx_destroy_prv( MutexState* mutex )
{
    pthread_mutex_destroy( &mutex->mutex );
}
