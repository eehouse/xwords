/* -*- compile-command: "cd ../linux && make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 2020-2022 by Eric House (xwords@eehouse.org).  All rights
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

#ifdef XWFEATURE_KNOWNPLAYERS

typedef struct _KnownPlayer {
    struct _KnownPlayer* next;
    XP_U32 newestMod;
    XP_UCHAR* name;
    CommsAddrRec addr;
} KnownPlayer;

typedef struct _KPState {
    KnownPlayer* players;
    XP_U16 nPlayers;
    XP_Bool dirty;
    XP_Bool inUse;
} KPState;

static void addPlayer( XW_DUtilCtxt* dutil, KPState* state,
                       const XP_UCHAR* name, const CommsAddrRec* addr,
                       XP_U32 newestMod );
static void getPlayersImpl( const KPState* state, const XP_UCHAR** players,
                            XP_U16* nFound );

static void
loadFromStream( XW_DUtilCtxt* dutil, KPState* state, XWStreamCtxt* stream )
{
    while ( 0 < stream_getSize( stream ) ) {
        XP_U32 newestMod = stream_getU32( stream );
        XP_UCHAR buf[64];
        stringFromStreamHere( stream, buf, VSIZE(buf) );

        CommsAddrRec addr = {0};
        addrFromStream( &addr, stream );

        addPlayer( dutil, state, buf, &addr, newestMod );
    }
}

static KPState*
loadState( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    // LOG_FUNC();
    pthread_mutex_lock( &dutil->kpMutex );

    KPState* state = (KPState*)dutil->kpCtxt;
    if ( NULL == state ) {
        dutil->kpCtxt = state = XP_CALLOC( dutil->mpool, sizeof(*state) );
        XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(dutil->mpool)
                                                    dutil_getVTManager(dutil) );
        const XP_UCHAR* keys[] = { KNOWN_PLAYERS_KEY, NULL };
        dutil_loadStream( dutil, xwe, keys, stream );
        if ( 0 < stream_getSize( stream ) ) {
            XP_U8 vers = stream_getU8( stream );
            stream_setVersion( stream, vers );
            loadFromStream( dutil, state, stream );
        }

        stream_destroy( stream, xwe );
    }
    XP_ASSERT( !state->inUse );
    state->inUse = XP_TRUE;
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
            stream_putU32( stream, kp->newestMod );
            stringToStream( stream, kp->name );
            addrToStream( stream, &kp->addr );
        }
        const XP_UCHAR* keys[] = { KNOWN_PLAYERS_KEY, NULL };
        dutil_storeStream( dutil, xwe, keys, stream );
        stream_destroy( stream, xwe );
        state->dirty = XP_FALSE;
    }
}

static void
releaseState( XW_DUtilCtxt* dutil, XWEnv xwe, KPState* state )
{
    XP_ASSERT( state->inUse );
    saveState( dutil, xwe, state );
    state->inUse = XP_FALSE;

    pthread_mutex_unlock( &dutil->kpMutex );
}

static const XP_UCHAR*
figureNameFor( XP_U16 posn, const CurGameInfo* gi )
{
    const XP_UCHAR* result = NULL;
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
makeUniqueName( const KPState* state, const XP_UCHAR* name,
                XP_UCHAR newName[], XP_U16 len )
{
    XP_U16 nPlayers = state->nPlayers;
    const XP_UCHAR* names[nPlayers];
    getPlayersImpl( state, names, &nPlayers );
    XP_ASSERT( nPlayers == state->nPlayers );
    for ( int ii = 2; ; ++ii ) {
        XP_SNPRINTF( newName, len, "%s %d", name, ii );
        XP_Bool found = XP_FALSE;
        for ( int jj = 0; !found && jj < nPlayers; ++jj ) {
            found = 0 == XP_STRCMP( names[jj], newName );
        }
        if ( !found ) {
            break;
        }
    }
    XP_LOGFF( "created new name: %s", newName );
}

/* Adding players is the hard part. There will be a lot with the same name and
 * representing the same device. That's easy: skip adding a new entry, but if
 * there's a change or addition, make it. For changes, e.g. a different
 * BlueTooth device name, the newer one wins.
 *
 * If two different names have the same mqtt devID, they're the same
 * device!!. Change the name to be that of the newer of the two, making sure
 * it's not a duplicate.
 *
 * For early testing, however, just make a new name.
 */
static void
addPlayer( XW_DUtilCtxt* XP_UNUSED_DBG(dutil), KPState* state, const XP_UCHAR* name,
           const CommsAddrRec* addr, XP_U32 newestMod )
{
    XP_LOGFF( "(name=%s)", name );
    KnownPlayer* withSameDevID = NULL;
    KnownPlayer* withSameName = NULL;

    for ( KnownPlayer* kp = state->players;
          !!kp && (!withSameDevID || !withSameName);
          kp = kp->next ) {
        if ( 0 == XP_STRCMP( kp->name, name ) ) {
            withSameName = kp;
        }
        if ( addr->u.mqtt.devID == kp->addr.u.mqtt.devID ) {
            withSameDevID = kp;
        }
    }

    XP_UCHAR tmpName[64];
    if ( !!withSameDevID ) {    /* only one allowed */
        XP_Bool isNewer = newestMod > withSameDevID->newestMod;
        XP_Bool changed = augmentAddr( &withSameDevID->addr, addr, isNewer );
        if ( isNewer ) {
            withSameDevID->newestMod = newestMod;
            changed = XP_TRUE;
        }
        state->dirty = changed || state->dirty;
    } else {
        if ( !!withSameName ) {
        /* Same name but different devID? Create a unique name */
            makeUniqueName( state, name, tmpName, VSIZE(tmpName) );
            name = tmpName;
        }
        /* XP_LOGFF( "adding new player %s!", name ); */
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
               CommsAddrRec addrs[], XP_U16 nAddrs, XP_U32 modTime )
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
                addPlayer( dutil, state, name, &addrs[ii], modTime );
            } else {
                XP_LOGFF( "unable to find %dth name", ii );
            }
        }
        releaseState( dutil, xwe, state );
    }

    return canUse;
}

XP_Bool
kplr_havePlayers( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    KPState* state = loadState( dutil, xwe );
    XP_Bool result = 0 < state->nPlayers;
    releaseState( dutil, xwe, state );
    LOG_RETURNF( "%s", boolToStr(result) );
    return result;
}

static void
getPlayersImpl( const KPState* state, const XP_UCHAR** players, XP_U16* nFound )
{
    if ( state->nPlayers <= *nFound && !!players ) {
        XP_U16 ii = 0;
        for ( KnownPlayer* kp = state->players; !!kp; kp = kp->next ) {
            players[ii++] = kp->name;
        }
    }
    *nFound = state->nPlayers;
}

void
kplr_getNames( XW_DUtilCtxt* dutil, XWEnv xwe,
               const XP_UCHAR** players, XP_U16* nFound )
{
    KPState* state = loadState( dutil, xwe );
    getPlayersImpl( state, players, nFound );
    releaseState( dutil, xwe, state );
}

static KnownPlayer*
findByName( KPState* state, const XP_UCHAR* name )
{
    KnownPlayer* result = NULL;
    for ( KnownPlayer* kp = state->players; !!kp && !result; kp = kp->next ) {
        if ( 0 == XP_STRCMP( kp->name, name ) ) {
            result = kp;
        }
    }
    return result;
}

XP_Bool
kplr_getAddr( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* name,
              CommsAddrRec* addr, XP_U32* lastMod )
{
    KPState* state = loadState( dutil, xwe );
    XP_Bool found = XP_FALSE;
    KnownPlayer* kp = findByName( state, name );
    found = NULL != kp;
    if ( found ) {
        *addr = kp->addr;
        if ( !!lastMod ) {
            *lastMod = kp->newestMod;
        }
    }
    releaseState( dutil, xwe, state );
    LOG_RETURNF( "%s", boolToStr(found) );
    return found;
}

const XP_UCHAR*
kplr_nameForMqttDev( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* mqttDevID )
{
    const XP_UCHAR* name = NULL;
    MQTTDevID devID;
    if ( strToMQTTCDevID( mqttDevID, &devID ) ) {
        KPState* state = loadState( dutil, xwe );
        for ( KnownPlayer* kp = state->players; !!kp && !name; kp = kp->next ) {
            const CommsAddrRec* addr = &kp->addr;
            if ( addr_hasType( addr, COMMS_CONN_MQTT ) ) {
                if ( 0 == XP_MEMCMP( &addr->u.mqtt.devID, &devID, sizeof(devID) ) ) {
                    name = kp->name;
                }
            }
        }
        releaseState( dutil, xwe, state );
    }
    LOG_RETURNF( "%s", name );
    return name;
}

static void
freeKP( XW_DUtilCtxt* XP_UNUSED_DBG(dutil), KnownPlayer* kp )
{
    XP_FREEP( dutil->mpool, &kp->name );
    XP_FREE( dutil->mpool, kp );
}

KP_Rslt
kplr_renamePlayer( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* oldName,
                   const XP_UCHAR* newName )
{
    KP_Rslt result;
    KPState* state = loadState( dutil, xwe );

    KnownPlayer* kp = findByName( state, oldName );
    if ( !kp ) {
        result = KP_NAME_NOT_FOUND;
    } else if ( NULL != findByName( state, newName ) ) {
        result = KP_NAME_IN_USE;
    } else {
        XP_FREEP( dutil->mpool, &kp->name );
        kp->name = copyString( dutil->mpool, newName );
        state->dirty = XP_TRUE;
        result = KP_OK;
    }

    releaseState( dutil, xwe, state );
    return result;
}

KP_Rslt
kplr_deletePlayer( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* name )
{
    KP_Rslt result = KP_NAME_NOT_FOUND;
    KnownPlayer* doomed = NULL;
    KPState* state = loadState( dutil, xwe );

    KnownPlayer* prev = NULL;
    for ( KnownPlayer* kp = state->players; !!kp && !doomed; kp = kp->next ) {
        if ( 0 == XP_STRCMP( kp->name, name ) ) {
            doomed = kp;
            if ( NULL == prev ) { /* first time through? */
                state->players = kp->next;
            } else {
                prev->next = kp->next;
            }
            --state->nPlayers;
            state->dirty = XP_TRUE;
            result = KP_OK;
        }
        prev = kp;
    }
    releaseState( dutil, xwe, state );

    XP_ASSERT( !!doomed );
    if ( !!doomed ) {
        freeKP( dutil, doomed );
    }
    return result;
}

void
kplr_cleanup( XW_DUtilCtxt* dutil )
{
    KPState** state = (KPState**)&dutil->kpCtxt;
    if ( !!*state ) {
        XP_ASSERT( !(*state)->inUse );
        KnownPlayer* next = NULL;
        for ( KnownPlayer* kp = (*state)->players; !!kp; kp = next ) {
            next = kp->next;
            freeKP( dutil, kp );
        }
        XP_FREEP( dutil->mpool, state );
    }
}
#endif
