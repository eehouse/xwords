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

#include "stats.h"
#include "xwmutex.h"
#include "device.h"
#include "xwstream.h"
#include "strutils.h"

typedef struct StatsState {
    XP_U32* statsVals;
    pthread_mutex_t mutex;
} StatsState;

static const XP_UCHAR* STATtoStr(STAT stat);
static XP_U32* loadCounts( XW_DUtilCtxt* dutil, XWEnv xwe );
static void storeCounts( XW_DUtilCtxt* dutil, XWEnv xwe );

void
sts_init( XW_DUtilCtxt* dutil )
{
    StatsState* ss = XP_CALLOC( dutil->mpool, sizeof(*ss) );
    initMutex( &ss->mutex, XP_TRUE );
    dutil->statsState = ss;
}

void
sts_cleanup( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    StatsState* ss = dutil->statsState;
    storeCounts( dutil, xwe );
    XP_ASSERT( !!ss );
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
            ss->statsVals = loadCounts( dutil, xwe );
        }
        ++ss->statsVals[stat];
        END_WITH_MUTEX();

        XP_LOGFF( "bad: storing after every change" );
        storeCounts( dutil, xwe );
    }
}

cJSON*
sts_export( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    StatsState* ss = dutil->statsState;
    XP_ASSERT( !!ss );
    cJSON* result = cJSON_CreateObject();

    WITH_MUTEX( &ss->mutex );
    if ( !ss->statsVals ) {
        ss->statsVals = loadCounts( dutil, xwe );
    }
    for ( int ii = 0; ii < STAT_NSTATS; ++ii ) {
        XP_U32 val = ss->statsVals[ii];
        if ( 0 != val ) {
            const XP_UCHAR* nam = STATtoStr(ii);
            cJSON_AddNumberToObject(result, nam, val);
        }
    }
    END_WITH_MUTEX();
    return result;
}

/* void */
/* sts_increment( XW_DUtilCtxt* dutil, XWEnv XP_UNUSED(xwe), STS_KEY key ) */
/* { */
/*     StatsState* ss = dutil->statsState; */
/*     ++ss->stats[key]; */
/* } */

void
sts_clearAll( XW_DUtilCtxt* XP_UNUSED(dutil), XWEnv XP_UNUSED(xwe) )
{
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
        CASESTR(STAT_SMS_SENT);
        CASESTR(STAT_SMS_RCVD);
    default:
        XP_ASSERT(0);
    }
#undef CASESTR
    result += 5;
    return result;
}

static void
storeCounts( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    StatsState* ss = dutil->statsState;
    XP_ASSERT( !!ss );

    XWStreamCtxt* stream = mkStream( dutil );
    stream_putU8( stream, 0 );  /* version */

    WITH_MUTEX( &ss->mutex );
    if ( !!ss->statsVals ) {
        for ( int ii = 0; ii < STAT_NSTATS; ++ii ) {
            XP_U32 val = ss->statsVals[ii];
            if ( 0 != val ) {
                stream_putU8( stream, ii );
                stream_putU32VL( stream, val );
            }
        }
    }
    END_WITH_MUTEX();

    const XP_UCHAR* keys[] = { STATS_KEY, NULL };
    dutil_storeStream( dutil, xwe, keys, stream );
    stream_destroy( stream );
}

static XP_U32*
loadCounts( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    XWStreamCtxt* stream = mkStream( dutil );
    const XP_UCHAR* keys[] = { STATS_KEY, NULL };
    dutil_loadStream( dutil, xwe, keys, stream );

    XP_U32* statsVals
        = XP_CALLOC( dutil->mpool, sizeof(*statsVals) * STAT_NSTATS );

    XP_U8 version;
    if ( stream_gotU8( stream, &version ) ) {
        XP_U8 stat;
        while ( stream_gotU8( stream, &stat ) ) {
            XP_U32 value = stream_getU32VL( stream );
            statsVals[stat] = value;
        }
    }
    stream_destroy( stream );
    return statsVals;
}
