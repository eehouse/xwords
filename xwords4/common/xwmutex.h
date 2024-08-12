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

#include <pthread.h>
#include "xptypes.h"

/* Making this a struct in case I want to add e.g. a chain of holders */
typedef struct _MutexState {
    pthread_mutex_t mutex;
} MutexState;

#define WITH_MUTEX_LOCK_DEBUG(STATEP) {                                 \
    MutexState* _state = (STATEP);                                      \
    time_t startTime = time(NULL);                                      \
    pthread_mutex_lock(&_state->mutex);                                 \
    time_t gotItTime = time(NULL);                                      \
    time_t _elapsed = gotItTime-startTime;                              \
    if ( 0 < _elapsed ) {                                               \
        XP_LOGFF("took %lds to get mutex", _elapsed);                   \
    }                                                                   \

#define WITH_MUTEX_UNLOCK_DEBUG()                           \
    time_t unlockTime = time(NULL);                         \
    _elapsed = unlockTime-gotItTime;                        \
    if ( 0 < _elapsed ) {                                   \
        XP_LOGFF("held mutex for %lds", _elapsed);          \
    }                                                       \
    pthread_mutex_unlock(&_state->mutex);                   \
    }                                                       \

#define WITH_MUTEX_LOCK_RELEASE(COMMS) {       \
    CommsCtxt* _comms = COMMS;                  \
    pthread_mutex_lock(&_comms->mutex);         \

#define WITH_MUTEX_UNLOCK_RELEASE()            \
    pthread_mutex_unlock(&_comms->mutex);       \
    }                                           \

#ifdef DEBUG
#define WITH_MUTEX WITH_MUTEX_LOCK_DEBUG
#define END_WITH_MUTEX WITH_MUTEX_UNLOCK_DEBUG
#else
#define WITH_MUTEX WITH_MUTEX_LOCK_RELEASE
#define END_WITH_MUTEX WITH_MUTEX_UNLOCK_RELEASE
#endif

void initMutex( MutexState* mutex, XP_Bool recursive );

#endif
