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

#include "dllist.h"

// #define WAIT_ALL_SECS 3
/* #define WAIT_ALL_SECS to enable checking for ALL mutexes. Better to define
   it in a platform's Makefile than here.
*/

/* Take 2: A single thread runs forever, sleeping 1 second at a time. On
   waking it walks a list of pending mutexes, and asserts that none has
   expired. */

static pthread_t sCheckThread = 0;
static pthread_mutex_t sCheckMutex = PTHREAD_MUTEX_INITIALIZER;
static DLHead* sCheckList = NULL;

typedef struct _CheckListData {
    XP_U32 currentTime;
    int count;                  /* for dev/logging only */
} CheckListData;

typedef struct _CheckThreadData {
    DLHead links;
    XP_U32 expiryTime;
    const char* file;
    const char* caller;
    int lineNo;
} CheckThreadData;

static ForEachAct
checkListProcLocked( const DLHead* elem, void* closure )
{
    CheckListData* cld = (CheckListData*)closure;
    CheckThreadData* ctd = (CheckThreadData*)elem;
    if ( cld->currentTime > ctd->expiryTime ) {
        XP_LOGFF( "FAIL: %s() on line %d in %s unable to lock mutex",
                  ctd->caller, ctd->lineNo, ctd->file );
        XP_ASSERT(0);
    }
    ++cld->count;
    return FEA_OK;
}

static void*
checkProc( void* XP_UNUSED(closure) )
{
    for ( int ii = 0; ; ++ii ) {
        sleep(1);
        pthread_mutex_lock( &sCheckMutex );
        CheckListData cld = { .currentTime = (XP_U32)time(NULL), };
        (void)dll_map( sCheckList, checkListProcLocked, NULL, &cld );
        pthread_mutex_unlock( &sCheckMutex );
        /* Don't log from this thread on Android. 
           PENDING what's the #ifdef to check? Add one?
           XP_LOGFF( "pass %d: checked %d pending", ii, cld.count );
        */
    }
    return NULL;
}

static void
addCheckee( CheckThreadData* ctd )
{
    pthread_mutex_lock( &sCheckMutex );
    if ( 0 == sCheckThread ) {
        (void)pthread_create( &sCheckThread, NULL, checkProc, NULL );
    }
    sCheckList = dll_insert( sCheckList, &ctd->links, NULL );
    pthread_mutex_unlock( &sCheckMutex );
}

static void
removeCheckee( CheckThreadData* ctd )
{
    pthread_mutex_lock( &sCheckMutex );
    XP_ASSERT( !!sCheckList );
    XP_ASSERT( 0 != sCheckThread );
    sCheckList = dll_remove( sCheckList, &ctd->links );
    pthread_mutex_unlock( &sCheckMutex );
}

void
mtx_lock_prv( MutexState* state, XP_U16 waitSecs,
              const char* file, const char* caller, int lineNo )
{
    if ( 0 == waitSecs ) {
        waitSecs = state->waitSecs;
    }

    CheckThreadData ctd = {};
    if ( 0 < waitSecs ) {
        ctd.expiryTime = waitSecs + (XP_U32)time(NULL),
        ctd.file = file;
        ctd.caller = caller;
        ctd.lineNo = lineNo;
        addCheckee( &ctd );
    }

    pthread_mutex_lock( &state->mutex );

    if ( 0 < waitSecs ) {
        removeCheckee( &ctd );
    }
}

void
mtx_unlock_prv( MutexState* state )
{
    pthread_mutex_unlock( &state->mutex );
}
#endif

void
mtx_init_prv( MutexState* mutex, XP_Bool recursive
#ifdef DEBUG
              , XP_U16 waitSecs, const char* caller
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
    XP_LOGFF( "set waitSecs: %d (called by %s())", mutex->waitSecs, caller );
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
