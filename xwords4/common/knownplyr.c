/* -*- compile-command: "cd ../linux && make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 2020-2024 by Eric House (xwords@eehouse.org).  All rights
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

#ifdef XWFEATURE_KNOWNPLAYERS

#include "knownplyr.h"
#include "strutils.h"
#include "comms.h"
#include "dbgutil.h"
#include "dllist.h"
#include "xwmutex.h"

typedef struct _KnownPlayer {
    DLHead links;
    XP_U32 newestMod;
    XP_UCHAR* name;
    CommsAddrRec addr;
} KnownPlayer;

typedef struct _KPState {
    KnownPlayer* players;
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

        CommsAddrRec addr = {};
        addrFromStream( &addr, stream );

        addPlayer( dutil, state, buf, &addr, newestMod );
    }
}

static KPState*
loadStateLocked( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    // LOG_FUNC();
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

        stream_destroy( stream );
    }
    XP_ASSERT( !state->inUse );
    state->inUse = XP_TRUE;
    return state;
}

static ForEachAct
saveProc( const DLHead* dl, void* closure )
{
    XWStreamCtxt* stream = (XWStreamCtxt*)closure;
    KnownPlayer* kp = (KnownPlayer*)dl;
    stream_putU32( stream, kp->newestMod );
    stringToStream( stream, kp->name );
    addrToStream( stream, &kp->addr );
    return FEA_OK;
}

static void
saveState( XW_DUtilCtxt* dutil, XWEnv xwe, KPState* state )
{
    if ( state->dirty ) {
        XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(dutil->mpool)
                                                    dutil_getVTManager(dutil) );
        stream_setVersion( stream, CUR_STREAM_VERS );
        stream_putU8( stream, CUR_STREAM_VERS );

        dll_map( &state->players->links, saveProc, NULL, stream );

        const XP_UCHAR* keys[] = { KNOWN_PLAYERS_KEY, NULL };
        dutil_storeStream( dutil, xwe, keys, stream );
        stream_destroy( stream );
        state->dirty = XP_FALSE;
    }
}

static void
releaseStateLocked( XW_DUtilCtxt* dutil, XWEnv xwe, KPState* state )
{
    XP_ASSERT( state->inUse );
    saveState( dutil, xwe, state );
    state->inUse = XP_FALSE;
}

static void
makeUniqueName( const KPState* state, const XP_UCHAR* name,
                XP_UCHAR newName[], XP_U16 len )
{
    XP_U16 nPlayers = dll_length( &state->players->links );
    const XP_UCHAR* names[nPlayers];
    getPlayersImpl( state, names, &nPlayers );
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
    XP_LOGFFV( "created new name: %s", newName );
}

static int
compByName(const DLHead* dl1, const DLHead* dl2)
{
    const KnownPlayer* kp1 = (const KnownPlayer*)dl1;
    const KnownPlayer* kp2 = (const KnownPlayer*)dl2;
    return XP_STRCMP( kp1->name, kp2->name );
}

static int
compByDate(const DLHead* dl1, const DLHead* dl2)
{
    const KnownPlayer* kp1 = (const KnownPlayer*)dl1;
    const KnownPlayer* kp2 = (const KnownPlayer*)dl2;
    int result = 0;
    if ( kp1->newestMod < kp2->newestMod ) {
        result = 1;
    } else if ( kp1->newestMod > kp2->newestMod ) {
        result = -1;
    }
    return result;
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

typedef struct _AddData {
    const XP_UCHAR* name;
    const CommsAddrRec* addr;
    KnownPlayer* withSameDevID;
    KnownPlayer* withSameName;
} AddData;

static ForEachAct
addProc( const DLHead* dl, void* closure )
{
    AddData* adp = (AddData*)closure;
    KnownPlayer* kp = (KnownPlayer*)dl;
    if ( 0 == XP_STRCMP( kp->name, adp->name ) ) {
        adp->withSameName = kp;
    }
    if ( adp->addr->u.mqtt.devID == kp->addr.u.mqtt.devID ) {
        adp->withSameDevID = kp;
    }
    ForEachAct result = !!adp->withSameName && !!adp->withSameDevID
        ? FEA_EXIT : FEA_OK;
    return result;
}

static void
addPlayer( XW_DUtilCtxt* XP_UNUSED_DBG(dutil), KPState* state,
           const XP_UCHAR* name, const CommsAddrRec* addr, XP_U32 newestMod )
{
    XP_LOGFFV( "(name=%s, newestMod: %d)", name, newestMod );
    AddData ad = {.name = name, .addr = addr, };
    dll_map( &state->players->links, addProc, NULL, &ad );

    XP_UCHAR tmpName[64];
    if ( !!ad.withSameDevID ) {    /* only one allowed */
        XP_Bool isNewer = newestMod > ad.withSameDevID->newestMod;
        XP_Bool changed = augmentAddr( &ad.withSameDevID->addr,
                                       addr, isNewer );
        if ( isNewer ) {
            XP_LOGFF( "updating newestMod from %d to %d",
                      ad.withSameDevID->newestMod, newestMod );
            ad.withSameDevID->newestMod = newestMod;
            changed = XP_TRUE;
        }
        state->dirty = changed || state->dirty;
    } else {
        if ( !!ad.withSameName ) {
        /* Same name but different devID? Create a unique name */
            makeUniqueName( state, name, tmpName, VSIZE(tmpName) );
            name = tmpName;
        }
        /* XP_LOGFF( "adding new player %s!", name ); */
        KnownPlayer* newPlayer = XP_CALLOC( dutil->mpool, sizeof(*newPlayer) );
        newPlayer->name = copyString( dutil->mpool, name );
        newPlayer->addr = *addr;
        newPlayer->newestMod = newestMod;
        state->players
            = (KnownPlayer*)dll_insert( &state->players->links,
                                        &newPlayer->links, compByName );
        state->dirty = XP_TRUE;
    }
    XP_LOGFFV( "nPlayers now: %d", dll_length( &state->players->links ) );
}

XP_Bool
kplr_addAddr( XW_DUtilCtxt* dutil, XWEnv xwe, const CommsAddrRec* addr,
              const XP_UCHAR* name, XP_U32 modTime )
{
    XP_ASSERT( modTime );
    XP_ASSERT( !!name );
    XP_Bool canUse = addr_hasType( addr, COMMS_CONN_MQTT );
    if ( canUse ) {
        WITH_MUTEX( &dutil->kpMutex );
        KPState* state = loadStateLocked( dutil, xwe );
        addPlayer( dutil, state, name, addr, modTime );
        releaseStateLocked( dutil, xwe, state );
        END_WITH_MUTEX();
    }

    XP_LOGFFV( "=>%s", boolToStr(canUse) );
    return canUse;
}

XP_Bool
kplr_havePlayers( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    XP_Bool result;
    WITH_MUTEX( &dutil->kpMutex );
    KPState* state = loadStateLocked( dutil, xwe );
    result = 0 < dll_length( &state->players->links );
    releaseStateLocked( dutil, xwe, state );
    END_WITH_MUTEX();
    LOG_RETURNF( "%s", boolToStr(result) );
    return result;
}

typedef struct _GetState {
    const XP_UCHAR** players;
    int indx;
} GetState;


static ForEachAct
getProc( const DLHead* dl, void* closure )
{
    GetState* gsp = (GetState*)closure;
    const KnownPlayer* kp = (KnownPlayer*)dl;
    gsp->players[gsp->indx++] = kp->name;
    return FEA_OK;
}

static void
getPlayersImpl( const KPState* state, const XP_UCHAR** players,
                XP_U16* nFound )
{
    XP_U16 nPlayers = dll_length( &state->players->links );
    if ( nPlayers <= *nFound && !!players ) {
        GetState gs = { .players = players, .indx = 0, };
#ifdef DEBUG
        DLHead* head =
#endif
            dll_map( &state->players->links, getProc, NULL, &gs );
        XP_ASSERT( head == &state->players->links );
    }
    *nFound = nPlayers;
}

void
kplr_getNames( XW_DUtilCtxt* dutil, XWEnv xwe, XP_Bool byDate,
               const XP_UCHAR** players, XP_U16* nFound )
{
    WITH_MUTEX( &dutil->kpMutex );
    KPState* state = loadStateLocked( dutil, xwe );
    if ( byDate ) {
        state->players = (KnownPlayer*)dll_sort( &state->players->links,
                                                 compByDate );
    }
    getPlayersImpl( state, players, nFound );
    if ( byDate ) {
        state->players = (KnownPlayer*)dll_sort( &state->players->links,
                                                 compByName );
    }
    releaseStateLocked( dutil, xwe, state );
    END_WITH_MUTEX();
}

typedef struct _FindState {
    const XP_UCHAR* name;
    const KnownPlayer* result;
} FindState;

static ForEachAct
findProc( const DLHead* dl, void* closure )
{
    ForEachAct result = FEA_OK;
    FindState* fsp = (FindState*)closure;
    const KnownPlayer* kp = (KnownPlayer*)dl;
    if ( 0 == XP_STRCMP( kp->name, fsp->name ) ) {
        fsp->result = kp;
        result = FEA_EXIT;
    }
    return result;
}

static KnownPlayer*
findByName( KPState* state, const XP_UCHAR* name )
{
    FindState fs = { .name = name, };
#ifdef DEBUG
    DLHead* head =
#endif
        dll_map( &state->players->links, findProc, NULL, &fs );
    XP_ASSERT( head == &state->players->links );
    return (KnownPlayer*)fs.result;
}

XP_Bool
kplr_getAddr( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* name,
              CommsAddrRec* addr, XP_U32* lastMod )
{
    XP_Bool found = XP_FALSE;
    WITH_MUTEX( &dutil->kpMutex );
    KPState* state = loadStateLocked( dutil, xwe );
    KnownPlayer* kp = findByName( state, name );
    found = NULL != kp;
    if ( found ) {
        *addr = kp->addr;
        if ( !!lastMod ) {
            *lastMod = kp->newestMod;
        }
    }
    releaseStateLocked( dutil, xwe, state );
    END_WITH_MUTEX();
    XP_LOGFF( "(%s) => %s", name, boolToStr(found) );
    return found;
}

const XP_UCHAR*
kplr_nameForMqttDev( XW_DUtilCtxt* dutil, XWEnv xwe,
                     const MQTTDevID* devID )
{
    CommsAddrRec addr = {
        .u.mqtt.devID = *devID,
    };
    addr_setType( &addr, COMMS_CONN_MQTT );

    return kplr_nameForAddress( dutil, xwe, &addr );
}

typedef struct _MDevState {
    XW_DUtilCtxt* dutil;
    XWEnv xwe;
    const CommsAddrRec* addr;
    const XP_UCHAR* name;
} MDevState;

static ForEachAct
addrProc( const DLHead* dl, void* closure )
{
    ForEachAct result = FEA_OK;
    const KnownPlayer* kp = (KnownPlayer*)dl;
    MDevState* msp = (MDevState*)closure;

    if ( addrsAreSame( msp->dutil, msp->xwe, &kp->addr, msp->addr ) ) {
        msp->name = kp->name;
        result = FEA_EXIT;
    }
    return result;
}

const XP_UCHAR*
kplr_nameForAddress( XW_DUtilCtxt* dutil, XWEnv xwe,
                     const CommsAddrRec* addr )
{
    MDevState ms = {.addr = addr, .dutil = dutil, .xwe = xwe, };

    WITH_MUTEX( &dutil->kpMutex );
    KPState* state = loadStateLocked( dutil, xwe );
#ifdef DEBUG
    DLHead* head =
#endif
        dll_map( &state->players->links, addrProc, NULL, &ms );
    XP_ASSERT( head == &state->players->links );
    releaseStateLocked( dutil, xwe, state );
    END_WITH_MUTEX();

    XP_LOGFF( "=> %s", ms.name );
    return ms.name;
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
    WITH_MUTEX( &dutil->kpMutex );
    KPState* state = loadStateLocked( dutil, xwe );

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

    releaseStateLocked( dutil, xwe, state );
    END_WITH_MUTEX();
    return result;
}

typedef struct _DelState {
    XW_DUtilCtxt* dutil;
    const XP_UCHAR* name;
    KPState* state;
} DelState;

static ForEachAct
delMapProc( const DLHead* dl, void* closure )
{
    ForEachAct result = FEA_OK;

    const KnownPlayer* kp = (KnownPlayer*)dl;
    DelState* dsp = (DelState*)closure;
    if ( 0 == XP_STRCMP( kp->name, dsp->name ) ) {
        result = FEA_REMOVE | FEA_EXIT;
        dsp->state->dirty = XP_TRUE;
    }
    return result;
}

static void
delFreeProc( DLHead* elem, void* closure )
{
    DelState* dsp = (DelState*)closure;
    freeKP( dsp->dutil, (KnownPlayer*)elem );
}

KP_Rslt
kplr_deletePlayer( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* name )
{
    KP_Rslt result = KP_OK;
    WITH_MUTEX( &dutil->kpMutex );
    KPState* state = loadStateLocked( dutil, xwe );

    DelState ds = { .name = name, .state = state, .dutil = dutil,};
    state->players = (KnownPlayer*)
        dll_map( &state->players->links, delMapProc, delFreeProc, &ds );
    releaseStateLocked( dutil, xwe, state );
    END_WITH_MUTEX();

    return result;
}

void
kplr_cleanup( XW_DUtilCtxt* dutil )
{
    KPState** statep = (KPState**)&dutil->kpCtxt;
    KPState* state = *statep;
    if ( !!state ) {
        XP_ASSERT( !state->inUse );
        XP_LOGFF( "removing %d elems", dll_length(&state->players->links) );
        DelState ds = { .dutil = dutil,};
        dll_removeAll( &state->players->links, delFreeProc, &ds );
        XP_FREEP( dutil->mpool, statep );
    }
}
#endif
