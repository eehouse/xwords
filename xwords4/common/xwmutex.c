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

void
initMutex( MutexState* mutex, XP_Bool recursive )
{
    pthread_mutexattr_t attr;
    int ret = pthread_mutexattr_init(&attr);
    XP_ASSERT(0 == ret);
    if ( recursive ) {
        ret = pthread_mutexattr_settype(&attr,
                                        PTHREAD_MUTEX_RECURSIVE);
        XP_ASSERT(0 == ret);
    }
    pthread_mutex_init( &mutex->mutex, &attr );
    ret = pthread_mutexattr_destroy(&attr);
    XP_ASSERT(0 == ret);

#ifdef DEBUG
    /* if ( recursive ) { */
    /*     XP_LOGFF( "testing recursive call..." ); */
    /*     WITH_MUTEX( mutex ); */
    /*     WITH_MUTEX( mutex ); */
    /*     END_WITH_MUTEX(); */
    /*     END_WITH_MUTEX(); */
    /* } */
#endif
}