/* -*- compile-command: "cd ../linux && make MEMDEBUG=TRUE -j3"; -*- */
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
#include <unistd.h>

#define WAIT_ALL_SECS 3

/* What is this? It's my attempt to, on debug builds and for all or only
   chosen mutexes, get an assertion when the app doesn't get a lock in the
   specified number of seconds. It's complicated because I want it to run on
   Android, which doesn't have pthread_cancel().

   It works by spawining a thread immediately before calling
   pthread_mutex_lock(). If the lock succeeds, a flag is set so that the
   thread won't raise an alarm. If the thread wakes from its sleep to find
   that that flag isn't yet set, assertion failure...

   The tricky part is how to share state between the checker thread and its
   parent (that's calling pthread_mutex_lock().) Normally the parent might
   pass a pointer to something on the stack to the thread proc, but normally
   the stack frame will be long gone while the thread proc still wants to use
   it. So: the flag that the checker thread will check lives in the thread's
   own frame. The closure it's passed by the parent lets it set a pointer to
   that flag so the parent can see it (the closure struct being in the
   caller's scope.) So that the parent doesn't get out of pthread_mutex_lock()
   before that pointer is set up, it busy-waits on its being set. Sue me: it's
   debug code, and only active when set up for a particular mutex.
 */

typedef struct _WaitState {
    const char* caller;
    const char* file;
    int lineNo;
    XP_U16 waitSecs;
    XP_Bool* flagLoc;
} WaitState;

static void*
checkLockProc( void* closure )
{
    WaitState* wsp = (WaitState*)closure;
    const XP_UCHAR* file = wsp->file;
    const XP_UCHAR* caller = wsp->caller;
    XP_U16 waitSecs = wsp->waitSecs;
    int lineNo = wsp->lineNo;
    XP_Bool setMe = XP_FALSE;   /* caller will change on success */
    XP_ASSERT( !wsp->flagLoc );
    wsp->flagLoc = &setMe;      /* tells busy-waiting caller to run */

    sleep( waitSecs );
    if ( !setMe ) {
        XP_LOGFF( "failed to get mutex in %d secs (caller: %s(), line %d in %s)",
                  waitSecs, caller, lineNo, file );
        XP_ASSERT(0);
    }
    return NULL;
}

void
mtx_lock_prv( MutexState* state, XP_U16 waitSecs,
              const char* file, const char* caller, int lineNo )
{
    if ( 0 == waitSecs ) {
        waitSecs  =state->waitSecs;
    }

    WaitState ws = {
        .waitSecs = waitSecs,
        .file = file,
        .caller = caller,
        .lineNo = lineNo,
    };
    if ( 0 < waitSecs ) {
        XP_ASSERT( !ws.flagLoc );
        pthread_t waitThread;
        (void)pthread_create( &waitThread, NULL, checkLockProc, &ws );
        int count = 0;
        while ( !ws.flagLoc ) {
            usleep(50); /* wait for thread to start */
            if ( 0 == (++count%10) ) {
                XP_LOGFF( "count %d; flagLoc still null", count );
            }
        }
    }
    pthread_mutex_lock( &state->mutex );
    if ( 0 < ws.waitSecs ) {
        *ws.flagLoc = XP_TRUE;
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
    XP_LOGFF( "set waitSecs: %d", mutex->waitSecs );
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

#ifdef DEBUG
static void*
grabProc( void* closure )
{
    LOG_FUNC();
    MutexState* ms = (MutexState*)closure;
    WITH_MUTEX(ms);
    sleep(20);
    END_WITH_MUTEX();
    LOG_RETURN_VOID();
    return NULL;
}

void
mtx_crashToTest()
{
    LOG_FUNC();
    MutexState state1;
    MUTEX_INIT_CHECKED( &state1, XP_FALSE, 5 );

    /* One thread to grab it for 20 seconds */
    pthread_t thread1;
    (void)pthread_create( &thread1, NULL, grabProc, &state1 );

    /* Another to try to grab it */
    pthread_t thread2;
    (void)pthread_create( &thread2, NULL, grabProc, &state1 );

    pthread_join( thread2, NULL );
}
#endif
