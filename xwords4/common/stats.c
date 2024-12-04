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

#include "stats.h"
#include "device.h"
#include "xwstream.h"
#include "strutils.h"
#include "dbgutil.h"
#include "timers.h"

#include "xwmutex.h"

typedef struct StatsState {
    XP_U32* statsVals;
    XP_U32 startTime;
    MutexState mutex;
    XP_Bool timerSet;
} StatsState;

enum { VERSION_0,
    VERSION_1,                  /* adds timestamp at start */
};

static const XP_UCHAR* STATtoStr(STAT stat);
static void loadCountsLocked( XW_DUtilCtxt* dutil, XWEnv xwe );
static void storeCountsLocked( XW_DUtilCtxt* dutil, XWEnv xwe );
static void setStoreTimerLocked( XW_DUtilCtxt* dutil, XWEnv xwe );

void
sts_init( XW_DUtilCtxt* dutil )
{
    StatsState* ss = XP_CALLOC( dutil->mpool, sizeof(*ss) );
    MUTEX_INIT( &ss->mutex, XP_TRUE );
    dutil->statsState = ss;
}

void
sts_cleanup( XW_DUtilCtxt* dutil, XWEnv XP_UNUSED(xwe) )
{
    StatsState* ss = dutil->statsState;
    XP_ASSERT( !!ss );
    MUTEX_DESTROY( &ss->mutex );
    XP_FREEP( dutil->mpool, &ss->statsVals );
    XP_FREEP( dutil->mpool, &ss );
}

void
sts_increment( XW_DUtilCtxt* dutil, XWEnv xwe, STAT stat )
{
    if ( STAT_NONE < stat && stat < STAT_NSTATS ) {
        StatsState* ss = dutil->statsState;
        XP_ASSERT( !!ss );
        WITH_MUTEX( &ss->mutex );
        if ( !ss->statsVals ) {
            loadCountsLocked( dutil, xwe );
        }
        ++ss->statsVals[stat];

        setStoreTimerLocked( dutil, xwe );
        END_WITH_MUTEX();
    }
}

cJSON*
sts_export( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    StatsState* ss = dutil->statsState;
    XP_ASSERT( !!ss );
    cJSON* result = cJSON_CreateObject();
    cJSON* stats = cJSON_CreateObject();
    cJSON_AddItemToObject(result, "stats", stats );

    WITH_MUTEX( &ss->mutex );
    if ( !ss->statsVals ) {
        loadCountsLocked( dutil, xwe );
    }

    for ( int ii = 0; ii < STAT_NSTATS; ++ii ) {
        XP_U32 val = ss->statsVals[ii];
        if ( 0 != val ) {
            const XP_UCHAR* nam = STATtoStr(ii);
            cJSON_AddNumberToObject( stats, nam, val );
        }
    }

    cJSON_AddNumberToObject( result, "since", ss->startTime );

    END_WITH_MUTEX();
    return result;
}

void
sts_clearAll( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    StatsState* ss = dutil->statsState;
    XP_ASSERT( !!ss );

    /* grab outside the mutex */
    XP_U32 startTime = dutil_getCurSeconds( dutil, xwe );
    XP_U32* statsVals = XP_CALLOC( dutil->mpool,
                                   sizeof(*ss->statsVals) * STAT_NSTATS );


    WITH_MUTEX( &ss->mutex );
    XP_FREEP( dutil->mpool, &ss->statsVals );

    ss->statsVals = statsVals;
    ss->startTime = startTime;
    storeCountsLocked( dutil, xwe );
    END_WITH_MUTEX();
}

static const XP_UCHAR*
STATtoStr(STAT stat)
{
    const XP_UCHAR* result = NULL;
#define CASESTR(s) case (s): result = #s; break
    switch (stat) {
        CASESTR(STAT_MQTT_RCVD);
        CASESTR(STAT_MQTT_SENT);
        CASESTR(STAT_REG_NOROOM);
        CASESTR(STAT_NEW_SOLO);
        CASESTR(STAT_NEW_TWO);
        CASESTR(STAT_NEW_THREE);
        CASESTR(STAT_NEW_FOUR);
        CASESTR(STAT_NEW_REMATCH);
        CASESTR(STAT_NBS_SENT);
        CASESTR(STAT_NBS_RCVD);
        CASESTR(STAT_BT_SENT);
        CASESTR(STAT_BT_RCVD);
    default:
        XP_ASSERT(0);
    }
#undef CASESTR
    result += 5;
    return result;
}

static void
storeCountsLocked( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    StatsState* ss = dutil->statsState;
    XP_ASSERT( !!ss );

    XWStreamCtxt* stream = mkStream( dutil );
    stream_putU8( stream, VERSION_1 );
    stream_putU32( stream, ss->startTime );

    if ( !!ss->statsVals ) {
        for ( int ii = 0; ii < STAT_NSTATS; ++ii ) {
            XP_U32 val = ss->statsVals[ii];
            if ( 0 != val ) {
                stream_putU8( stream, ii );
                stream_putU32VL( stream, val );
            }
        }
    }

    dutil_storeStream( dutil, xwe, STATS_KEY, stream );
    stream_destroy( stream );
}

static void
loadCountsLocked( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    StatsState* ss = dutil->statsState;
    XP_ASSERT( !!ss );
    XP_U32* statsVals
        = XP_CALLOC( dutil->mpool, sizeof(*statsVals) * STAT_NSTATS );
    ss->statsVals = statsVals;

    XWStreamCtxt* stream = mkStream( dutil );
    dutil_loadStream( dutil, xwe, STATS_KEY, stream );

    XP_U8 version;
    if ( stream_gotU8( stream, &version ) ) {
        if ( VERSION_1 <= version ) {
            ss->startTime = stream_getU32(stream);
        }

        XP_U8 stat;
        while ( stream_gotU8( stream, &stat ) ) {
            XP_U32 value = stream_getU32VL( stream );
            statsVals[stat] = value;
        }
    }
    stream_destroy( stream );

    if ( 0 == ss->startTime ) {
        ss->startTime = dutil_getCurSeconds( dutil, xwe );
        setStoreTimerLocked( dutil, xwe ); /* something to save */
    }
}

#ifdef DUTIL_TIMERS
static void
onStoreTimer( void* closure, XWEnv xwe, XP_Bool XP_UNUSED_DBG(fired) )
{
    XP_LOGFF( "(fired: %s)", boolToStr(fired) );
    XW_DUtilCtxt* dutil = (XW_DUtilCtxt*)closure;
    StatsState* ss = dutil->statsState;
    XP_ASSERT( !!ss );

    WITH_MUTEX( &ss->mutex );
    storeCountsLocked( dutil, xwe );
    ss->timerSet = XP_FALSE;
    END_WITH_MUTEX();
    LOG_RETURN_VOID();
}
#endif

static void
setStoreTimerLocked( XW_DUtilCtxt* dutil, XWEnv xwe )
{
#ifdef DUTIL_TIMERS
    StatsState* ss = dutil->statsState;
    XP_ASSERT( !!ss );
    if ( !ss->timerSet ) {
        ss->timerSet = XP_TRUE;
        XP_U32 inWhenMS = 5 * 1000;
#ifdef DEBUG
        TimerKey key =
#endif
            tmr_set( dutil, xwe, inWhenMS, onStoreTimer, dutil );
        XP_LOGFF( "tmr_set() => %d", key );
    } else {
        // XP_LOGFF( "timer already set" );
    }
#else
    XP_USE(dutil);
    XP_USE(xwe);
#endif
}
