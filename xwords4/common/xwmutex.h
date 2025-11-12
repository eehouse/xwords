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

#ifndef _XWMUTEX_H_
#define _XWMUTEX_H_

#include "xptypes.h"
#include "comtypes.h"

#ifdef USE_MUTEXES
#ifdef DEBUG

void mtx_lock_prv(MutexState* state, XP_U16 waitSecs, const char* file,
                  const char* caller, int lineNo);
void mtx_unlock_prv(MutexState* state);
# define WITH_MUTEX_CHECKED(STATE, SECS) {        \
    MutexState* _state = (STATE);                \
    XP_U16 _waitSecs = (SECS);                   \
    mtx_lock_prv(_state, _waitSecs, __FILE__, __func__, __LINE__)
# define WITH_MUTEX(STATE) WITH_MUTEX_CHECKED(STATE, 0)
# define END_WITH_MUTEX() mtx_unlock_prv(_state);   \
    }
#else
# define WITH_MUTEX(STATE) {                            \
    const pthread_mutex_t* _mutex = &(STATE)->mutex;    \
    pthread_mutex_lock((pthread_mutex_t*)_mutex)
# define WITH_MUTEX_CHECKED(STATE, IGNORE) WITH_MUTEX(STATE)
# define END_WITH_MUTEX() pthread_mutex_unlock((pthread_mutex_t*)_mutex); \
    }
#endif

void mtx_init_prv( MutexState* mutex, XP_Bool recursive
# ifdef DEBUG
                   , XP_U16 waitSecs, const char* caller
# endif
                       );
void mtx_destroy_prv( MutexState* mutex );

#ifdef DEBUG
void mtx_crashToTest();

# define MUTEX_INIT_CHECKED(STATE, RECURSIVE, WS)                 \
                   mtx_init_prv((STATE), (RECURSIVE), (WS), __func__)
# ifndef MUTEX_CHECK_SECS
#  define MUTEX_CHECK_SECS 3
# endif
# define MUTEX_INIT(STATE, RECURSIVE) MUTEX_INIT_CHECKED(STATE, RECURSIVE, MUTEX_CHECK_SECS)
#else
# define mtx_crashToTest()
# define MUTEX_INIT(STATE, RECURSIVE) mtx_init_prv((STATE), (RECURSIVE))
# define MUTEX_INIT_CHECKED(STATE, RECURSIVE, WS) MUTEX_INIT((STATE), (RECURSIVE))
#endif

#define MUTEX_DESTROY(STATE) mtx_destroy_prv((STATE))

#else   /* USE_MUTEXES */
# define WITH_MUTEX_CHECKED(M,C) WITH_MUTEX(M)
# define MUTEX_INIT_CHECKED(M,B,C) MUTEX_INIT(M,B)
# define MUTEX_INIT(M,B)
# define MUTEX_DESTROY(M)
# ifdef DEBUG
# define WITH_MUTEX(M) {                        \
    MutexState* _ms = (M);                      \
    pthread_t _self = pthread_self();           \
    if ( 0 == _ms->counter ) {                  \
        XP_ASSERT( !_ms->owner );               \
        _ms->owner = _self;                     \
    } else {                                    \
        XP_ASSERT( _ms->owner == _self );       \
    }                                           \
    ++_ms->counter;                             \

# define END_WITH_MUTEX()                       \
    XP_ASSERT( 0 < _ms->counter );              \
    --_ms->counter;                             \
    if ( 0 == _ms->counter ) {                  \
        _ms->owner = 0;                         \
    }                                           \
    }                                           \

# else
# define WITH_MUTEX(M)
# define END_WITH_MUTEX()
# endif

#endif  /* USE_MUTEXES */

#endif  /* #ifndef _XWMUTEX_H_ */
