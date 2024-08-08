/* -*- compile-command: "make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 2020 by Eric House (xwords@eehouse.org).  All rights reserved.
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


#include <glib.h>
#include <time.h>
#include <stdbool.h>
#include "gsrcwrap.h"
#include "xptypes.h"
#include "comtypes.h"

#define TAG __FILE__ ": "

typedef struct _WrapperState {
    union {
        GSourceFunc srcProc;
        GIOFunc ioProc;
    } proc;
    void* data;
    const char* caller;
    const char* procName;
    struct timespec spec;
} WrapperState;

static GSList* s_idleProcs = NULL;
static pthread_mutex_t s_idleProcsMutex = PTHREAD_MUTEX_INITIALIZER;

static void
printElapsedFor( struct timespec* first, struct timespec* second,
                 const char* proc, const char* action )
{
    XP_USE( proc );
    XP_USE( action );
    time_t secs = second->tv_sec - first->tv_sec;
    long nsecs = second->tv_nsec - first->tv_nsec;
    while ( nsecs < 0 ) {
        ++secs;
        nsecs += 1000000000;
    }
    
    /* float firstSecs = (float)first->tv_sec + (((float)first->tv_nsec)/1000000000.0f); */
    /* float secondSecs = (float)second->tv_sec + (((float)second->tv_nsec)/1000000000.0f); */
    /* XP_LOGF( TAG "elapsed %s %s(): %ld.%ld", action, proc, secs, nsecs ); */
}


static gint
idle_wrapper( gpointer data )
{
    WrapperState* state = (WrapperState*)data;
    /* XP_LOGF( TAG "%s(): CALLED for %s", __func__, state->procName ); */

    struct timespec callTime;
    clock_gettime(CLOCK_REALTIME, &callTime);
    printElapsedFor( &state->spec, &callTime, state->procName, "scheduling" );

    gint result = (*state->proc.srcProc)(state->data);

    struct timespec returnTime;
    clock_gettime( CLOCK_REALTIME, &returnTime );

    printElapsedFor( &callTime, &returnTime, state->procName, "running" );

#ifdef DEBUG
    const char* procName = state->procName;
    XP_USE( procName );
#endif
    if ( 0 == result ) {        /* won't be getting called again */
        pthread_mutex_lock( &s_idleProcsMutex );
        s_idleProcs = g_slist_remove( s_idleProcs, state );
        pthread_mutex_unlock( &s_idleProcsMutex );
        g_free( state );
    }

    /* XP_LOGF( TAG "%s(): DONE for %s; now have %d", __func__, procName, */
    /*          g_slist_length(s_idleProcs) ); */

    return result;
}

guint
_wrapIdle( GSourceFunc function, gpointer data,
           const char* procName, const char* caller )
{
    /* XP_LOGF( TAG "%s(): installing proc %s from caller %s", __func__, */
    /*          procName, caller ); */
    WrapperState* state = g_malloc0( sizeof(*state) );
    pthread_mutex_lock( &s_idleProcsMutex );
    s_idleProcs = g_slist_append( s_idleProcs, state );
    pthread_mutex_unlock( &s_idleProcsMutex );
    state->proc.srcProc = function;
    state->data = data;
    state->caller = caller;
    state->procName = procName;
    clock_gettime(CLOCK_REALTIME, &state->spec);
    
    guint res = g_idle_add( idle_wrapper, state );
    /* XP_LOGF( TAG "%s(): added idle for %s; now have %d", __func__, */
    /*          procName, g_slist_length(s_idleProcs) ); */
    return res;
}

#define TRY_ONE(FLAG)                                                   \
    if ((condition & FLAG) == FLAG) {                                   \
        offset += snprintf( &buf[offset], len - offset, #FLAG " " );    \
    }
static void
formatFlags( char* buf, size_t len, GIOCondition condition )
{
    int offset = 0;
    TRY_ONE(G_IO_IN);
    TRY_ONE(G_IO_HUP);
    TRY_ONE(G_IO_ERR);
    TRY_ONE(G_IO_PRI);
}

static gboolean
watch_wrapper( GIOChannel* source, GIOCondition condition, gpointer data )
{
    WrapperState* state = (WrapperState*)data;

    char buf[128] = {};
    formatFlags( buf, VSIZE(buf), condition );
    /* XP_LOGF( TAG "%s(%s): CALLED; flags: %s", __func__, state->procName, buf ); */

    struct timespec callTime;
    clock_gettime(CLOCK_REALTIME, &callTime);

    bool keep = (*state->proc.ioProc)(source, condition, state->data);

    struct timespec returnTime;
    clock_gettime( CLOCK_REALTIME, &returnTime );

    printElapsedFor( &callTime, &returnTime, state->procName, "running" );

    /* XP_LOGF( TAG "%s(%s): DONE", __func__, state->procName ); */
    
    if ( 0 == keep ) {        /* won't be getting called again */
        // g_source_destroy( source );
        g_free( state );
    }
    return keep;
}

guint
_wrapWatch( gpointer data, int socket, GIOFunc ioProc,
            const char* procName, const char* caller )
{
    /* XP_LOGF( TAG "%s(): installing proc %s from caller %s", __func__, */
    /*          procName, caller ); */

    WrapperState* state = g_malloc0( sizeof(*state) );
    state->proc.ioProc = ioProc;
    state->data = data;
    state->caller = caller;
    state->procName = procName;

    GIOChannel* channel = g_io_channel_unix_new( socket );
    guint watch = g_io_add_watch( channel,
                                  G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI,
                                  watch_wrapper, state );
    g_io_channel_unref( channel ); /* only main loop holds it now */

    /* XP_LOGF( TAG "%s(): added watch for %s", __func__, procName ); */
    return watch;
}

void
gsw_logIdles()
{
    XP_LOGF( TAG "%s(): %d idles pending", __func__, g_slist_length(s_idleProcs) );
}
