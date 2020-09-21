/* -*- compile-command: "cd ../linux && make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 2001 - 2020 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

#include "knownplyr.h"
#include "strutils.h"
#include "comms.h"
#include "dbgutil.h"

typedef struct _KnownPlayer {
    struct _KnownPlayer* next;
    XP_UCHAR* name;
    CommsAddrRec addr;
} KnownPlayer;

typedef struct _KPState {
    KnownPlayer* players;
    XP_U16 nPlayers;
    XP_Bool dirty;
} KPState;

/* enum { STREAM_VERSION_KP_1,     /\* initial *\/ */
/* }; */

static void
addPlayer( XW_DUtilCtxt* dutil, KPState* state,
           const XP_UCHAR* name, const CommsAddrRec* addr );

static void
loadFromStream( XW_DUtilCtxt* dutil, KPState* state, XWStreamCtxt* stream )
{
    LOG_FUNC();
    while ( 0 < stream_getSize( stream ) ) {
        XP_UCHAR buf[64];
        stringFromStreamHere( stream, buf, VSIZE(buf) );

        CommsAddrRec addr = {0};
        addrFromStream( &addr, stream );

        addPlayer( dutil, state, buf, &addr );
    }
}

static KPState*
loadState( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    LOG_FUNC();
    KPState* state = (KPState*)dutil->kpCtxt;
    if ( NULL == state ) {
        dutil->kpCtxt = state = XP_CALLOC( dutil->mpool, sizeof(*state) );
        XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(dutil->mpool)
                                                    dutil_getVTManager(dutil) );
        dutil_loadStream( dutil, xwe, KNOWN_PLAYERS_KEY, NULL, stream );
        if ( 0 < stream_getSize( stream ) ) {
            XP_U8 vers = stream_getU8( stream );
            stream_setVersion( stream, vers );
            loadFromStream( dutil, state, stream );
        }

        stream_destroy( stream, xwe );
    }
    return state;
}

static void
saveState( XW_DUtilCtxt* dutil, XWEnv xwe, KPState* state )
{
    if ( state->dirty ) {
        XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(dutil->mpool)
                                                    dutil_getVTManager(dutil) );
        stream_putU8( stream, CUR_STREAM_VERS );
        for ( KnownPlayer* kp = state->players; !!kp; kp = kp->next ) {
            stringToStream( stream, kp->name );
            addrToStream( stream, &kp->addr );
        }
        dutil_storeStream( dutil, xwe, KNOWN_PLAYERS_KEY, stream );
        stream_destroy( stream, xwe );
        state->dirty = XP_FALSE;
    }
}

static const XP_UCHAR*
figureNameFor( XP_U16 posn, const CurGameInfo* gi )
{
    const XP_UCHAR* result = NULL;
    // int nthRemote = 0;
    for ( int ii = 0, nthRemote = 0;
          NULL == result && ii < gi->nPlayers;
          ++ii ) {
        const LocalPlayer* lp = &gi->players[ii];
        if ( lp->isLocal ) {
            continue;
        } else if ( nthRemote++ == posn ) {
            result = lp->name;
        }
    }
    return result;
}

static void
addPlayer( XW_DUtilCtxt* dutil, KPState* state,
           const XP_UCHAR* name, const CommsAddrRec* addr )
{
    XP_LOGFF( "(name=%s)", name );
    XP_Bool exists = XP_FALSE;
    for ( KnownPlayer* kp = state->players; !!kp && !exists; kp = kp->next ) {
        exists = 0 == XP_STRCMP( kp->name, name );
    }
    if ( !exists ) {
        XP_LOGFF( "adding new player!" );
        KnownPlayer* newPlayer = XP_CALLOC( dutil->mpool, sizeof(*newPlayer) );
        newPlayer->name = copyString( dutil->mpool, name );
        newPlayer->addr = *addr;
        newPlayer->next = state->players;
        state->players = newPlayer;
        state->dirty = XP_TRUE;
        ++state->nPlayers;
    }
    XP_LOGFF( "nPlayers now: %d", state->nPlayers );
}

XP_Bool
kplr_addAddrs( XW_DUtilCtxt* dutil, XWEnv xwe, const CurGameInfo* gi,
               CommsAddrRec addrs[], XP_U16 nAddrs )
{
    LOG_FUNC();
    XP_Bool canUse = XP_TRUE;
    for ( int ii = 0; ii < nAddrs && canUse; ++ii ) {
        canUse = addr_hasType( &addrs[ii], COMMS_CONN_MQTT );
        if ( !canUse ) {
            XP_LOGFF( "addr %d has no mqqt id", ii );
        }
    }

    if ( canUse ) {
        KPState* state = loadState( dutil, xwe );
        for ( int ii = 0; ii < nAddrs && canUse; ++ii ) {
            const XP_UCHAR* name = figureNameFor( ii, gi );
            if ( !!name ) {
                addPlayer( dutil, state, name, &addrs[ii] );
            } else {
                XP_LOGFF( "unable to find %dth name", ii );
            }
        }
        saveState( dutil, xwe, state );
    }

    return canUse;
}

XP_Bool
kplr_havePlayers( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    KPState* state = loadState( dutil, xwe );
    XP_Bool result = 0 < state->nPlayers;
    LOG_RETURNF( "%s", boolToStr(result) );
    return result;
}

void
kplr_getPlayers( XW_DUtilCtxt* dutil, XWEnv xwe,
                 const XP_UCHAR** players, XP_U16* nFound )
{
    KPState* state = loadState( dutil, xwe );
    if ( state->nPlayers <= *nFound && !!players ) {
        XP_U16 ii = 0;
        for ( KnownPlayer* kp = state->players; !!kp; kp = kp->next ) {
            players[ii++] = kp->name;
        }
    }
    *nFound = state->nPlayers;
}

void
kplr_cleanup( XW_DUtilCtxt* dutil )
{
    KPState** state = (KPState**)&dutil->kpCtxt;
    if ( !!*state ) {
        for ( KnownPlayer* kp = (*state)->players; !!kp; kp = kp->next ) {
            XP_FREEP( dutil->mpool, &kp->name );
            XP_FREE( dutil->mpool, kp );
        }
        XP_FREEP( dutil->mpool, state );
    }
}
