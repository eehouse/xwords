/* 
 * Copyright 2024-2025 by Eric House (xwords@eehouse.org).  All rights
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

#include "gamemgrp.h"
#include "xwarray.h"
#include "dictmgrp.h"
#include "stats.h"
#include "device.h"
#include "nli.h"
#include "dbgutil.h"
#include "timers.h"
#include "strutils.h"

#include "LocalizedStrIncludes.h"

#define KEY_GAMEMGR "gmgr"
#define KEY_STATE "state"
#define KEY_GAMES "games"
#define KEY_DATA "data"
#define KEY_GRP "grp"
#define KEY_GROUPS "groups"
#define KEY_IDS "ids"
#define KEY_SUM "sum"
#define KEY_REFS "refs"
#define KEY_GI "gi"
#define MAX_KEYS 4
#define FLAG_HASCOMMS 0x01
#define MAX_GROUP_NAME 32

/* Highest possible bit that doesn't get messed with by signed/unsigned
   translations: hi java */
#define GROUP_BIT 0x4000000000000000

typedef struct _GameEntry {
    GameRef gr;
    GameData* gd;
} GameEntry;

typedef struct _GroupState {
    XW_DUtilCtxt* duc;          /* so GroupState can be useful as closure */
    union {
        XWArray* games;         /* used if collapsed is NOT set */
        XP_U16 nGames;          /* used if collapsed is set */
    } u;
    GroupRef grp;
    XP_UCHAR name[MAX_GROUP_NAME+1];
    SORT_ORDER sos[SO_NSOS];
    XP_UCHAR nSOs;
    XP_Bool collapsed;
} GroupState;

typedef struct _DeleteData {
    XW_DUtilCtxt* duc;
    XWEnv xwe;
    GameRef target;            /* might be null */
    GameEntry* ge;
} DeleteData;

static void insertGr( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr, GameData* gd );
static void gmgr_addGR( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr );
static void loadGamesOnce( XW_DUtilCtxt* duc, XWEnv xwe );
static XP_U32 gameIDFromGR( GameRef gr );
static GroupState* addGroup( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp,
                             const XP_UCHAR* name );
typedef void (*OnGameProc)( GameRef game, XWEnv xwe, void* closure );

typedef struct _KeyStore {
    const XP_UCHAR* keys[MAX_KEYS+1];
    XP_UCHAR storage[128];
} KeyStore;
static void mkKeys( GameRef gr, KeyStore* ksp, const XP_UCHAR* second );
static GameRef grFromKey( const XP_UCHAR* key );
static void storeGroups( XW_DUtilCtxt* duc, XWEnv xwe, XP_Bool deleteAfter );
static GroupState* findGroupByRef( XW_DUtilCtxt* duc, XWEnv xwe,
                                   GroupRef grp, XP_U32* indx );
static void dispGroupStateProc( void* elem, void* dd );
static void postOnGroupChanged( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp,
                                GroupChangeEvent grce );
static void scheduleSaveState( XW_DUtilCtxt* duc, XWEnv xwe );
static void checkDefault( XW_DUtilCtxt* duc, GroupRef* grpp );
static void checkMakeArchive( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef* grpp );
static void storeGroupRef( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr, GroupRef grp );
static void storeGroup( XW_DUtilCtxt* duc, XWEnv xwe, GroupState* grps );
static XP_U32 numGames(const GroupState* grps);
static XP_U32 countGroupGames( XW_DUtilCtxt* duc, XWEnv xwe, GroupState* grps );
static XWArray* makeGroupGamesArray( XW_DUtilCtxt* duc, XWEnv xwe, GroupState* grps );
static void onCollapsedChange( XW_DUtilCtxt* duc, XWEnv xwe,
                               GroupState* grps, XP_Bool newVal );

struct GameMgrState {
    XWArray* list;
    XWArray* deletedList;
    XWArray* pendingGroupEvents;
    XP_U16 nextSaveToken;

    XWArray* groups;
    XP_U8 defaultGrp;    /* the GroupRef of the current default */
    XP_U8 archiveGrp;    /* the GroupRef of the current archive group (may be 0) */

    XP_Bool loaded;
    XP_Bool saveStatePending;
};

static int
sortByGR(const void* dl1, const void* dl2,
         XWEnv XP_UNUSED(xwe), void* XP_UNUSED(closure))
{
    GameEntry* ge1 = (GameEntry*)dl1;
    GameEntry* ge2 = (GameEntry*)dl2;
    // GameRef is 64-bit, not an int!! So simple subtraction won't work.
    int result = 0;
    if ( ge1->gr > ge2->gr ) {
        result = 1;
    } else if ( ge1->gr < ge2->gr ) {
        result = -1;
    }
    return result;
}

void
gmgr_init( XW_DUtilCtxt* duc )
{
    XP_ASSERT( !duc->gameMgrState );
    GameMgrState* gs = XP_CALLOC( duc->mpool, sizeof(*gs) );
    duc->gameMgrState = gs;
    XWArray* list = arr_make( MPPARM(duc->mpool) sortByGR, NULL );
    gs->list = list;
    gs->deletedList = arr_make( MPPARM(duc->mpool) sortByGR, NULL );
    gs->pendingGroupEvents = arr_make( MPPARM(duc->mpool) NULL, NULL );
}

XP_Bool
gmgr_isGame(GLItemRef ir)
{
    return !(GROUP_BIT & ir);
}

GameRef
gmgr_toGame(GLItemRef ir)
{
    XP_ASSERT( gmgr_isGame(ir) );
    return (GameRef)ir;
}

GroupRef
gmgr_toGroup(GLItemRef ir)
{
    XP_ASSERT( !gmgr_isGame(ir) );
    return (ir & 0xFFFF);
}

GroupRef
gmgr_addGroup( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* name )
{
    LOG_FUNC();
    GroupRef grp;
    for ( grp = 1; ; ++grp ) {
        if ( !findGroupByRef( duc, xwe, grp, NULL ) ) {
            GroupState* grps = addGroup( duc, xwe, grp, name );
            onCollapsedChange( duc, xwe, grps, XP_FALSE );
            break;
        }
    }
    scheduleSaveState( duc, xwe );
    postOnGroupChanged( duc, xwe, grp, GRCE_ADDED );
    return grp;
}

#ifdef XWFEATURE_GAMEREF_CONVERT
typedef struct _FindGrpByNameState {
    const XP_UCHAR* name;
    GroupRef result;
} FindGrpByNameState;

static ForEachAct
findByName( void* elem, void* closure, XWEnv XP_UNUSED(xwe) )
{
    ForEachAct result = FEA_OK;
    FindGrpByNameState* fgs = (FindGrpByNameState*)closure;
    GroupState* gs = (GroupState*)elem;
    if ( 0 == XP_STRCMP( fgs->name, gs->name ) ) {
        fgs->result = gs->grp;
        result |= FEA_EXIT;
    }
    return result;
}

GroupRef
gmgr_getGroup( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* name )
{
    GameMgrState* gs = duc->gameMgrState;
    FindGrpByNameState fgs = { .name = name, };
    arr_map( gs->groups, xwe, findByName, &fgs );
    return fgs.result;
}
#endif

static void
addToGroup( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr, GroupRef grp )
{
    XP_LOGFF( "(" GR_FMT ", %d)", gr, grp );
    GroupState* grps = findGroupByRef( duc, xwe, grp, NULL );
    gr_setGroup( duc, xwe, gr, grp ); /* should this call storeGroupRef */
    if ( !grps->collapsed ) {
        arr_insert( grps->u.games, xwe, (void*)gr );
    } else {
        ++grps->u.nGames;
    }
    postOnGroupChanged( duc, xwe, grp, GRCE_GAME_ADDED );
    storeGroupRef( duc, xwe, gr, grp );
}

static ForEachAct
removeGR( void* elem, void* closure, XWEnv XP_UNUSED(xwe) )
{
    ForEachAct result = FEA_OK;
    GameRef gr1 = (GameRef)elem;
    GameRef gr2 = (GameRef)closure;
    if ( gr1 == gr2 ) {
        result |= FEA_EXIT | FEA_REMOVE;
    }
    return result;
}

void
gmgr_rmFromGroup( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr, GroupRef grp )
{
    GroupState* grps = findGroupByRef( duc, xwe, grp, NULL );
    if ( !grps->collapsed ) {
        arr_map( grps->u.games, xwe, removeGR, (void*)gr );
    } else {
        --grps->u.nGames;
    }
    postOnGroupChanged( duc, xwe, grp, GRCE_GAME_REMOVED );
    storeGroup( duc, xwe, grps );
}

void
gmgr_addToGroup( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr, GroupRef grp )
{
    checkDefault( duc, &grp );
    addToGroup( duc, xwe, gr, grp );
}

void
gmgr_deleteGroup( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp )
{
    GameMgrState* gs = duc->gameMgrState;
    if ( gs->defaultGrp != grp ) {
        GroupState* grps = findGroupByRef( duc, xwe, grp, NULL );

        /* Uncollapse so we have the list to iterate over */
        onCollapsedChange( duc, xwe, grps, XP_FALSE );

        while ( 0 < arr_length(grps->u.games) ) {
            GameRef gr = (GameRef)arr_getNth( grps->u.games, 0 );
            gmgr_deleteGame( duc, xwe, gr );
        }

        arr_remove( gs->groups, xwe, grps );

        if ( grp == gs->archiveGrp ) {
            gs->archiveGrp = 0;
        }

        DeleteData dd = { .duc = duc, .xwe = xwe, };
        dispGroupStateProc( grps, &dd );
        scheduleSaveState( duc, xwe );
        postOnGroupChanged( duc, xwe, grp, GRCE_DELETED );
    }
}

static void
moveGroupBy( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp, int by )
{
    GameMgrState* gs = duc->gameMgrState;
    XP_U32 indx;
    GroupState* grps = findGroupByRef( duc, xwe, grp, &indx );
    XP_U32 newLoc = indx + by;
    if ( 0 <= newLoc && newLoc < arr_length(gs->groups) ) {
        arr_removeAt( gs->groups, xwe, indx );
        arr_insertAt( gs->groups, grps, newLoc );
        scheduleSaveState( duc, xwe );
        postOnGroupChanged( duc, xwe, grp, GRCE_MOVED );
    }
}

void
gmgr_raiseGroup( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp )
{
    moveGroupBy( duc, xwe, grp, -1 );
}

void
gmgr_lowerGroup( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp )
{
    moveGroupBy( duc, xwe, grp, 1 );
}

void
gmgr_getGroupName( XW_DUtilCtxt* duc, XWEnv xwe,
                   GroupRef grp, XP_UCHAR buf[], XP_U16 bufLen )
{
    GroupState* grps = findGroupByRef( duc, xwe, grp, NULL );
    XP_SNPRINTF( buf, bufLen, "%s", grps->name );
}

void
gmgr_setGroupName( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp,
                   const XP_UCHAR* name )
{
    GroupState* grps = findGroupByRef( duc, xwe, grp, NULL );
    XP_SNPRINTF( grps->name, VSIZE(grps->name), "%s", name );
    grps->name[VSIZE(grps->name)-1] = '\0';
    postOnGroupChanged( duc, xwe, grp, GRCE_RENAMED );
}

static void
onCollapsedChange( XW_DUtilCtxt* duc, XWEnv xwe, GroupState* grps, XP_Bool newVal )
{
    if ( newVal != grps->collapsed ) {
        grps->collapsed = newVal;
        if ( newVal ) {
            arr_destroy( grps->u.games );
            grps->u.nGames = countGroupGames( duc, xwe, grps );
        } else {
            grps->u.games = makeGroupGamesArray( duc, xwe, grps );
        }
    }
}

void
gmgr_setGroupCollapsed( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp,
                        XP_Bool collapsed )
{
    GroupState* grps = findGroupByRef( duc, xwe, grp, NULL );
    if ( grps->collapsed != collapsed ) {
        onCollapsedChange( duc, xwe, grps, collapsed );
        XP_LOGFF( "(collapsed: %s)", boolToStr(collapsed) );
        GroupChangeEvent grce = collapsed ? GRCE_COLLAPSED : GRCE_EXPANDED;
        postOnGroupChanged( duc, xwe, grp, grce );
    }
}

void
gmgr_makeGroupDefault( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp )
{
    GameMgrState* gs = duc->gameMgrState;
    if ( gs->defaultGrp != grp ) {
        gs->defaultGrp = grp;
        scheduleSaveState( duc, xwe );
    }
    XP_ASSERT( !!findGroupByRef( duc, xwe, grp, NULL ) );
}

GroupRef
gmgr_getDefaultGroup( XW_DUtilCtxt* duc )
{
    GameMgrState* gs = duc->gameMgrState;
    return gs->defaultGrp;
}

GroupRef
gmgr_getArchiveGroup( XW_DUtilCtxt* duc )
{
    GameMgrState* gs = duc->gameMgrState;
    return gs->archiveGrp;
}

void
gmgr_moveGames( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp,
                GameRef games[], XP_U16 nGames )
{
    checkMakeArchive( duc, xwe, &grp );
    XP_LOGFF( "(grp: %d, nGames: %d)", grp, nGames );

    for ( int ii = 0; ii < nGames; ++ii ) {
        GameRef gr = games[ii];
        GroupRef curGroup = gr_getGroup( duc, gr, xwe );
        if ( grp == curGroup ) {
            XP_LOGFF( "game " GR_FMT " already in grp %d", gr, grp );
        } else {
            XP_LOGFF( "moving " GR_FMT " from group %d to %d", gr,
                      curGroup, grp );
            gmgr_rmFromGroup( duc, xwe, gr, curGroup );
            addToGroup( duc, xwe, gr, grp );
        }
    }
    scheduleSaveState( duc, xwe );
}

XP_Bool
gmgr_getGroupCollapsed( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp )
{
    GroupState* grps = findGroupByRef( duc, xwe, grp, NULL );
    return grps->collapsed;
}

XP_U32
gmgr_getGroupGamesCount( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp )
{
    GroupState* grps = findGroupByRef( duc, xwe, grp, NULL );
    XP_U32 result = numGames(grps);
    return result;
}

typedef struct _FindGrpState {
    GroupRef grp;
    GroupState* result;
    XP_U16 indx;
} FindGrpState;

static ForEachAct
findByGRP( void* elem, void* closure, XWEnv XP_UNUSED(xwe) )
{
    ForEachAct result = FEA_OK;
    GroupState* gs = (GroupState*)elem;
    FindGrpState* fgs = (FindGrpState*)closure;
    if ( gs->grp == fgs->grp ) {
        fgs->result = gs;
        result |= FEA_EXIT;
    } else {
        ++fgs->indx;
    }
    return result;
}

static GroupState*
findGroupByRef( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp, XP_U32* indxp )
{
    XP_ASSERT( 0 < grp );
    GameMgrState* gs = duc->gameMgrState;
    FindGrpState fgs = { .grp = grp, };
    arr_map( gs->groups, xwe, findByGRP, &fgs );
    if ( !!indxp ) {
        *indxp = fgs.indx;
    }
    // XP_ASSERT( !!fgs.result );
    return fgs.result;
}

/* static GroupState* */
/* findGroupByName( XW_DUtilCtxt* duc, const XP_UCHAR* name ) */
/* { */
/*     XP_USE(duc); */
/*     XP_USE(name); */
/*     return NULL; */
/* } */

static ForEachAct
saveGameProc( void* elem, void* closure, XWEnv xwe )
{
    GameEntry* ge = (GameEntry*)elem;
    DeleteData* dd = (DeleteData*)closure;
    gmgr_saveGame( dd->duc, xwe, ge->gr );
    return FEA_OK;
}

static void
dispGameEntryProc( void* elem, void* closure )
{
    DeleteData* dd = (DeleteData*)closure;
    GameEntry* ge = (GameEntry*)elem;
    gr_freeData( dd->duc, ge->gr, dd->xwe, ge->gd );
    XP_FREE( dd->duc->mpool, ge );
}

static void
dispGroupStateProc( void* elem, void* closure )
{
    GroupState* grps = (GroupState*)elem;
    DeleteData* dd = (DeleteData*)closure;
    if ( !grps->collapsed ) {
        arr_destroy( grps->u.games );
        grps->u.games = NULL;
    }
    XP_FREE( dd->duc->mpool, elem );
}

static void
freeEntryProc( void* elem, void* closure )
{
    XW_DUtilCtxt* duc = (XW_DUtilCtxt*)closure;
    XP_FREE( duc->mpool, elem );
}

void
gmgr_cleanup( XW_DUtilCtxt* duc, XWEnv xwe )
{
    GameMgrState* gs = duc->gameMgrState;
    DeleteData dd = { .duc = duc, .xwe = xwe, };
    arr_map( gs->list, xwe, saveGameProc, &dd );

    storeGroups( duc, xwe, XP_TRUE );

    XWArray* lists[] = { gs->list, gs->deletedList };
    for ( int ii = 0; ii < VSIZE(lists); ++ii ) {
        arr_removeAll( lists[ii], dispGameEntryProc, &dd );
        arr_destroy( lists[ii] );
    }
    arr_removeAll( gs->pendingGroupEvents, freeEntryProc, duc );
    arr_destroy( gs->pendingGroupEvents );

    arr_removeAll( gs->groups, dispGroupStateProc, &dd );
    arr_destroy( gs->groups );

    XP_FREEP( duc->mpool, &gs );
}

typedef struct _CountItemsState {
    XP_U16 count;
} CountItemsState;

static ForEachAct
countVisiblsItemsProc( void* elem, void* closure, XWEnv XP_UNUSED(xwe) )
{
    CountItemsState* cis = (CountItemsState*)closure;
    GroupState* grps = (GroupState*)elem;
    cis->count += 1;
    if ( !grps->collapsed ) {
        cis->count += numGames(grps);
    }
    return FEA_OK;
}

XP_U16
gmgr_countItems( XW_DUtilCtxt* duc, XWEnv xwe )
{
    GameMgrState* gs = duc->gameMgrState;
    loadGamesOnce( duc, xwe );

    CountItemsState cis = {};
    arr_map( gs->groups, xwe, countVisiblsItemsProc, &cis );
    
    /* XP_U32 len = arr_length( gs->list ) + 1; /\* fake group, to start *\/ */
    /* XP_U16 count = (XP_U16)len; */
    LOG_RETURNF( "%d", cis.count );
    return cis.count;
}

static GameEntry*
findFor( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr, XP_Bool tryDeleted,
         XP_Bool* deletedP )
{
    GameEntry* result = NULL;
    GameMgrState* gs = duc->gameMgrState;
    XP_U32 indx;
    XP_Bool deleted = XP_FALSE;
    GameEntry dummy = { .gr = gr, };
    if ( arr_find( gs->list, xwe, &dummy, &indx ) ) {
        result = arr_getNth( gs->list, indx );
    } else if ( tryDeleted ) {
        if ( arr_find( gs->deletedList, xwe, &dummy, &indx ) ) {
            deleted = XP_TRUE;
            result = arr_getNth( gs->deletedList, indx );
        }
    }

    if ( result && deletedP ) {
        *deletedP = deleted;
    }

    return result;
}

XP_Bool
gmgr_haveGame( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 gameID, DeviceRole role )
{
    loadGamesOnce( duc, xwe );

    GameRef gr = formatGR( gameID, role );
    GameEntry* ge = findFor( duc, xwe, gr, XP_FALSE, NULL );
    XP_Bool result = !!ge;

    XP_LOGFF( "(%X, %d) => %s", gameID, role, boolToStr(result) );
    return result;
}

typedef struct _DictAddState {
    XW_DUtilCtxt* duc;
    const XP_UCHAR* dictName;
    const XP_UCHAR* isoCode;
    const DictionaryCtxt* dict;
} DictAddState;

static ForEachAct
checkNewDict( void* elem, void* closure, XWEnv xwe )
{
    DictAddState* das = (DictAddState*)closure;
    GameEntry* ge = (GameEntry*)elem;
    if ( !!ge->gd ) {
        gr_checkNewDict( das->duc, xwe, ge->gd, das->dict );
    }
    return FEA_OK;
}

/* Any game that's loaded and thinks it's missing this wordlist should be
   notified. */
void
gmgr_onDictAdded( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* dictName )
{
    const DictionaryCtxt* dict = dmgr_get( duc, xwe, dictName );
    XP_UCHAR* isoCode = dict->isoCode;
    GameMgrState* gs = duc->gameMgrState;
    DictAddState das = { .duc = duc, .dictName = dictName,
                         .isoCode = isoCode, .dict = dict,
    };
    arr_map( gs->list, xwe, checkNewDict, &das );
    dict_unref( dict, xwe );
}

typedef struct _DictGoneState {
    XW_DUtilCtxt* duc;
    const XP_UCHAR* dictName;
} DictGoneState;

static ForEachAct
checkGoneDict( void* elem, void* closure, XWEnv xwe )
{
    DictGoneState* dgs = (DictGoneState*)closure;
    GameEntry* ge = (GameEntry*)elem;
    if ( !!ge->gd ) {
        gr_checkGoneDict( dgs->duc, xwe, ge->gd, dgs->dictName );
    }
    return FEA_OK;
}

void
gmgr_onDictRemoved( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* dictName )
{
    GameMgrState* gs = duc->gameMgrState;
    DictGoneState dgs = { .duc = duc, .dictName = dictName, };
    arr_map( gs->list, xwe, checkGoneDict, &dgs );
}

static void
storeGroupRef( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr, GroupRef grp )
{
    KeyStore ks;
    mkKeys( gr, &ks, KEY_GRP );
    XWStreamCtxt* stream = dvc_makeStream( duc );
    stream_putU16( stream, grp );
    dvc_storeStream( duc, xwe, ks.keys, stream );
    stream_destroy( stream );
    XP_LOGFF( "stored group %d for game " GR_FMT, grp, gr );
}

typedef struct _GetNthState {
    XP_U16 sought;
    GLItemRef result;
} GetNthState;

static ForEachAct
getNthProc( void* elem, void* closure, XWEnv XP_UNUSED(xwe) )
{
    ForEachAct result = FEA_OK;
    GroupState* grps = (GroupState*)elem;
    GetNthState* gns = (GetNthState*)closure;
    XP_U16 here = 1;
    if ( !grps->collapsed ) {
        here += arr_length(grps->u.games);
    }
    if ( here <= gns->sought ) {
        gns->sought -= here;
        // XP_LOGFF( "sought now %d", gns->sought );
    } else {
        if ( 0 == gns->sought ) {
            gns->result = grps->grp | GROUP_BIT;
        } else {
            XP_ASSERT( !grps->collapsed );
            GameRef gr = (GameRef)arr_getNth( grps->u.games, gns->sought-1 );
            gns->result = gr;
        }
        result |= FEA_EXIT;
    }
    return result;
}

GLItemRef
gmgr_getNthItem( XW_DUtilCtxt* duc, XWEnv xwe, XP_U16 indx )
{
    /* Walk the groups, dropping indx by the number each represents. */
    loadGamesOnce( duc, xwe );

    GameMgrState* gs = duc->gameMgrState;
    GetNthState gns = { .sought = indx, };
    arr_map( gs->groups, xwe, getNthProc, &gns );
    LOG_RETURNF( GR_FMT, gns.result );
    return gns.result;
}

XP_U16
gmgr_countGroups(XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe))
{
    GameMgrState* gs = duc->gameMgrState;
    return arr_length(gs->groups);
}

GroupRef
gmgr_getNthGroup(XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe), XP_U16 indx)
{
    GameMgrState* gs = duc->gameMgrState;
    GroupState* grps = (GroupState*)arr_getNth( gs->groups, indx );
    return grps->grp;
}

#ifdef DEBUG
XP_U16
gmgr_countGames(XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe))
{
    GameMgrState* gs = duc->gameMgrState;
    return arr_length(gs->list);
}

GameRef
gmgr_getNthGame(XW_DUtilCtxt* duc, XWEnv XP_UNUSED(xwe), XP_U16 indx)
{
    GameMgrState* gs = duc->gameMgrState;
    GameEntry* ge = arr_getNth( gs->list, indx );
    return ge->gr;
}
#endif

/* Don't actually delete the game: it's not easy to guarantee no code still
   has a GameRef it might want to use. So "mark" as deleted, allowing gr_
   calls to safely fail.
*/
void
gmgr_deleteGame( XW_DUtilCtxt* duc, XWEnv xwe, const GameRef gr )
{
    XP_LOGFF( "(" GR_FMT ")", gr );
    // First, create the keys since that requires live gr
    const char* midKeys[] = { KEY_DATA, KEY_GI, KEY_GRP, KEY_SUM, };
    KeyStore ks[VSIZE(midKeys)];
    for ( int ii = 0; ii < VSIZE(midKeys); ++ii ) {
        mkKeys( gr, &ks[ii], midKeys[ii] );
    }

    GameRef grp = gr_getGroup( duc, gr, xwe );
    gmgr_rmFromGroup( duc, xwe, gr, grp );

    GameEntry* ge = findFor( duc, xwe, gr, XP_FALSE, NULL );
    XP_ASSERT( !!ge );

    GameMgrState* gs = duc->gameMgrState;
    arr_remove( gs->list, xwe, ge );
    arr_insert( gs->deletedList, xwe, ge );

    // Finally clear the data
    for ( int ii = 0; ii < VSIZE(midKeys); ++ii ) {
        dvc_removeStream( duc, xwe, ks[ii].keys );
    }

    dutil_onGameChanged( duc, xwe, gr, GCE_DELETED );
    XP_LOGFF( "(" GR_FMT ") DONE", gr );
}

static void
mkKeys( GameRef gr, KeyStore* ksp, const XP_UCHAR* third )
{
    int keyIndx = 0;
    int storeIndx = 0;
    ksp->keys[keyIndx++] = KEY_GAMES;

    ksp->keys[keyIndx++] = &ksp->storage[storeIndx];
    storeIndx += XP_SNPRINTF( &ksp->storage[storeIndx],
                              VSIZE(ksp->storage)-storeIndx,
                              GR_FMT, gr );

    ksp->keys[keyIndx++] = third;

    ++storeIndx;                /* skip null */

    ksp->keys[keyIndx++] = NULL;
}

static GameRef
grFromKey( const XP_UCHAR* key )
{
    XP_LOGFF( "(%s)", key );
    XP_UCHAR buf[256];
    XP_SNPRINTF( buf, VSIZE(buf), "%s", key );
    XP_UCHAR* parts[8] = {0};
    XP_U16 count = VSIZE(parts);
    dvc_parseKey( buf, parts, &count );
    XP_ASSERT( count == 3 );

    GameRef gr;
    sscanf( parts[1], GR_FMT, &gr );
    XP_LOGFF( "scanned gameRef " GR_FMT " from str %s", gr, parts[1] );
    return gr;
}

XWStreamCtxt*
gmgr_loadGI( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr )
{
    KeyStore ks;
    mkKeys( gr, &ks, KEY_GI );
    XWStreamCtxt* stream = dvc_loadStream( duc, xwe, ks.keys );
    return stream;
}

XWStreamCtxt*
gmgr_loadSum( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr )
{
    KeyStore ks;
    mkKeys( gr, &ks, KEY_SUM );
    XWStreamCtxt* stream = dvc_loadStream( duc, xwe, ks.keys );
    return stream;
}

void
gmgr_storeSum( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr, XWStreamCtxt* stream )
{
    KeyStore ks;
    mkKeys( gr, &ks, KEY_SUM );
    dvc_storeStream( duc, xwe, ks.keys, stream );
}

XWStreamCtxt*
gmgr_loadData( XW_DUtilCtxt* duc, XWEnv xwe, const GameRef gr )
{
    KeyStore ks;
    mkKeys( gr, &ks, KEY_DATA );
    XWStreamCtxt* stream = dvc_loadStream( duc, xwe, ks.keys );
    return stream;
}

GroupRef
gmgr_loadGroupRef( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr )
{
    KeyStore ks;
    mkKeys( gr, &ks, KEY_GRP );
    XWStreamCtxt* stream = dvc_loadStream( duc, xwe, ks.keys );
    GroupRef grp = 0;
    if ( !!stream ) {
        grp = stream_getU16( stream );
        stream_destroy( stream );
    } else {
        XP_LOGFF( "nothing found for gr " GR_FMT, gr );
    }
    return grp;
}

void
gmgr_saveGI( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr )
{
    XWStreamCtxt* stream = dvc_makeStream( duc );
    gr_giToStream( duc, gr, xwe, stream );
    KeyStore ks;
    mkKeys( gr, &ks, KEY_GI );
    dvc_storeStream( duc, xwe, ks.keys, stream );
    stream_destroy( stream );
}

void
gmgr_saveGame( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr )
{
    XP_Bool deleted;
    GameData* gd = gmgr_getForRef( duc, xwe, gr, &deleted );
    if ( !deleted && !!gd ) {

        gmgr_saveGI(duc, xwe, gr );

        /* This is firing. Why? */
        // XP_ASSERT(gr_getGroup(duc, gr, xwe) == gmgr_loadGroupRef( duc, xwe, gr ));

        if ( gr_haveData( duc, gr, xwe ) ) {
            GameMgrState* gs = duc->gameMgrState;
            XP_U16 saveToken = ++gs->nextSaveToken;
            XWStreamCtxt* stream = dvc_makeStream( duc );
            gr_dataToStream( duc, gr, xwe, stream, saveToken );

            KeyStore ks;
            mkKeys( gr, &ks, KEY_DATA );
            dvc_storeStream( duc, xwe, ks.keys, stream );
            stream_destroy( stream );

            gr_saveSucceeded( duc, gr, xwe, saveToken );
        }
    }
}

static void
insertGr( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr, GameData* gd )
{
    XP_ASSERT( gr );
    XP_U32 gameID = gameIDFromGR( gr );
    XP_LOGFF( "gr: " GR_FMT "; gid: %X", gr, gameID );
    GameMgrState* gs = duc->gameMgrState;
    /* below leaking when game deleted */
    GameEntry* ge = XP_CALLOC( duc->mpool, sizeof(*ge) );
    XP_LOGFF( "gr: " GR_FMT "; gd: %p", gr, gd );
    ge->gr = gr;
    ge->gd = gd;
    arr_insert( gs->list, xwe, ge );
}

typedef struct _LoadGamesState {
    XW_DUtilCtxt* duc;
    XWEnv xwe;
} LoadGamesState;

static void
onGotGIKey( const XP_UCHAR* key, void* closure, XWEnv xwe )
{
    XP_LOGFF( "key: %s", key );
    LoadGamesState* lgs = (LoadGamesState*)closure;

    /* Let's just create an emtpy record. Later we'll load what we need */
    GameRef gr = grFromKey( key );
    if ( !findFor( lgs->duc, xwe, gr, XP_TRUE, NULL ) ) {
        gmgr_addGR( lgs->duc, xwe, gr );
    }

    /* XWStreamCtxt* stream = dvc_makeStream( lgs->dutil ); */
    /* dutil_loadStream( lgs->dutil, lgs->xwe, key, stream ); */
    /* // GameRef gr = */
    /* makeFromGIStream( lgs->dutil, lgs->xwe, stream ); */
    /* stream_destroy( stream ); */
    // insertGr( lgs->dutil, gr, NULL );
}

/* A game is awaiting  */
static int
stateCodeFor( const GameSummary* gs )
{
    int result;
    if ( 0 <= gs->turn ) {      /* game's in play */
        result = 1;
    } else if ( gs->gameOver ) {
        result = 2;
    } else {
        result = 0;
    }
    return result;
}

static int
cmpU32( XP_U32 first, XP_U32 second )
{
    int result = 0;
    if ( first < second ) {
        result = -1;
    } else if ( first > second ) {
        result = 1;
    }
    return result;
}

static int
sortOrderSort( const void* dl1, const void* dl2, XWEnv xwe, void* closure )
{
    GroupState* gs = (GroupState*)closure;
    XW_DUtilCtxt* duc = gs->duc;
    SORT_ORDER* sos = gs->sos;
    int result = 0;

    const CurGameInfo* gi1 = gr_getGI( duc, (GameRef)dl1, xwe );
    const CurGameInfo* gi2 = gr_getGI( duc, (GameRef)dl2, xwe );
    const GameSummary* gs1 = gr_getSummary( duc, (GameRef)dl1, xwe );
    const GameSummary* gs2 = gr_getSummary( duc, (GameRef)dl2, xwe );
    for ( int ii = 0; result == 0 && ii < gs->nSOs; ++ii ) {
        switch ( sos[ii] ) {
        case SO_GAMENAME:
            if ( !gi1->gameName && !gi2->gameName ) {
                /* they're equal */
            } else if ( !gi1->gameName ) {
                result = -1;
            } else if ( !gi2->gameName ) {
                result = 1;
            } else {
                result = XP_STRCMP( gi1->gameName, gi2->gameName );
            }
            break;
        case SO_CREATED:
            if ( gi1->created < gi2->created ) {
                result = -1;
            } else if ( gi1->created > gi2->created ) {
                result = 1;
            }
            break;
        case SO_GAMESTATE: {
            int sc1 = stateCodeFor( gs1 );
            int sc2 = stateCodeFor( gs2 );
            result = sc1 - sc2;
        }
            break;
        case SO_TURNLOCAL:
            /* reverse so local turns sort first  */
            result = ((int)gs2->turnIsLocal) - ((int)gs1->turnIsLocal);
            break;
        case SO_LASTMOVE_TS:
            result = cmpU32( gs1->lastMoveTime, gs2->lastMoveTime );
            break;

        default:
            XP_LOGFF( "so %d not handled", sos[ii] );
            XP_ASSERT(0);
        }
    }

    if ( !result ) {
        result = PtrCmpProc( dl1, dl2, xwe, NULL );
    }
    return result;
}

static GroupState*
addGroup( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp, const XP_UCHAR* name )
{
    GroupState* grps = XP_CALLOC( duc->mpool, sizeof(*grps) );
    grps->grp = grp;
    grps->duc = duc;
    grps->collapsed = XP_TRUE;
    SORT_ORDER sos[] = {
        SO_GAMESTATE,
        SO_TURNLOCAL,
        SO_LASTMOVE_TS,
    };
    grps->nSOs = VSIZE(sos);
    for ( int ii = 0; ii < VSIZE(sos); ++ii ) {
        grps->sos[ii] = sos[ii];
    }

    if ( !!name ) {
        XP_SNPRINTF( grps->name, VSIZE(grps->name), "%s", name );
    }

    GameMgrState* gs = duc->gameMgrState;
    arr_insert( gs->groups, xwe, grps );
    return grps;
}

typedef struct _GetGroupData {
    XW_DUtilCtxt* duc;
    GroupState* grps;
    union {
        XWArray* refs;
        int count;
    } u;
    XP_Bool collapsed;
} GetGroupData;

static void
onGotGroupKey( const XP_UCHAR* key, void* closure, XWEnv xwe )
{
    XP_LOGFF( "(key: %s)", key );
    GetGroupData* ggd = (GetGroupData*)closure;

    GameRef gr = grFromKey( key );
    GroupRef grp = gmgr_loadGroupRef( ggd->duc, xwe, gr );
    XP_LOGFF( "loaded grp %d from key %s (gr: " GR_FMT ")", grp, key, gr );

    XP_LOGFF( "comparing grp %d with %d", grp, ggd->grps->grp );
    if ( grp == ggd->grps->grp ) {
        GameRef gr = grFromKey( key );
        XP_LOGFF( "got a match for group %d: " GR_FMT, grp, gr );

        if ( ggd->collapsed ) {
            ++ggd->u.count;
        } else {
            arr_insert( ggd->u.refs, xwe, (void*)gr );
            XP_LOGFF( "added " GR_FMT " to games array for group %d", gr, grp );
        }
    }
}

static XP_U32
countGroupGames( XW_DUtilCtxt* duc, XWEnv xwe, GroupState* grps )
{
    LOG_FUNC();
    const XP_UCHAR* keys[] = { KEY_GAMES, "%", KEY_GRP, NULL };
    GetGroupData ggd = { .duc = duc,
                         .collapsed = XP_TRUE,
                         .grps = grps,
    };
    dvc_getKeysLike( duc, xwe, keys, onGotGroupKey, &ggd );
    LOG_RETURNF( "%d", ggd.u.count );
    return ggd.u.count;
}

/* Called whenever the collapsed flag is toggled off. Each game has stored its
   group as a field. We need to check them all and store a gameref for those
   whose group matches this one. */
static XWArray*
makeGroupGamesArray( XW_DUtilCtxt* duc, XWEnv xwe, GroupState* grps )
{
    LOG_FUNC();
    const XP_UCHAR* keys[] = { KEY_GAMES, "%", KEY_GRP, NULL };
    GetGroupData ggd = {
        .duc = duc,
        .grps = grps,
        .u.refs = arr_make( MPPARM(duc->mpool) sortOrderSort, grps ),
    };
    dvc_getKeysLike( duc, xwe, keys, onGotGroupKey, &ggd );
    XP_LOGFF( "got %d refs", arr_length(ggd.u.refs) );
    return ggd.u.refs;
}

static void
loadGroupData( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp )
{
    XP_LOGFF( "(grp: %d)", grp );
    XP_Bool collapsed;
    GroupState* grps;
    {
        XP_UCHAR buf[8];
        XP_SNPRINTF( buf, VSIZE(buf), "%d", grp );
        const XP_UCHAR* keys[] = { KEY_GROUPS, buf, KEY_SUM, NULL };
        XWStreamCtxt* stream = dvc_loadStream( duc, xwe, keys );
        XP_UCHAR name[MAX_GROUP_NAME+1];
        stringFromStreamHere( stream, name, VSIZE(name) );
        grps = addGroup( duc, xwe, grp, name );

        collapsed = stream_getU8( stream );
        stream_destroy( stream );
    }

    onCollapsedChange( duc, xwe, grps, collapsed );
}

static ForEachAct
enableSortProc( void* elem, void* XP_UNUSED(closure), XWEnv xwe )
{
    GroupState* grps = (GroupState*)elem;
    XP_ASSERT( !grps->collapsed );
    arr_setSort( grps->u.games, xwe, sortOrderSort, grps );
    return FEA_OK;
}

static void
loadGroups( XW_DUtilCtxt* duc, XWEnv xwe, GameMgrState* gs )
{
    LOG_FUNC();
    XP_ASSERT( !gs->groups );
    gs->groups = arr_make(MPPARM(duc->mpool) NULL, NULL );

    const XP_UCHAR* keys[] = { KEY_GROUPS, KEY_IDS, NULL };
    XWStreamCtxt* stream = dvc_loadStream( duc, xwe, keys );
    if ( !!stream ) {
        XP_U32 count = stream_getU32VL( stream );
        XP_LOGFF( "loading %d groups", count );
        for ( int ii = 0; ii < count; ++ii ) {
            XP_U8 gid = stream_getU8( stream );
            XP_ASSERT( 0 < gid );
            loadGroupData( duc, xwe, gid );
        }
        stream_destroy( stream );
    } else {
        GroupRef grp = 1;
        const XP_UCHAR* groupName =
            dutil_getUserString( duc, xwe, STRS_GROUPS_DEFAULT );
        GroupState* grps = addGroup( duc, xwe, grp, groupName );
        onCollapsedChange( duc, xwe, grps, XP_FALSE );

        gmgr_makeGroupDefault( duc, xwe, grp );
    }
}

static void
storeGroup( XW_DUtilCtxt* duc, XWEnv xwe, GroupState* grps )
{
    {
        XWStreamCtxt* stream = dvc_makeStream( duc );
        stringToStream( stream, grps->name );
        stream_putU8( stream, grps->collapsed );
        XP_U32 nGames = numGames( grps );
        stream_putU32VL( stream, nGames );

        XP_UCHAR buf[8];
        XP_SNPRINTF( buf, VSIZE(buf), "%d", grps->grp );
        const XP_UCHAR* keys[] = { KEY_GROUPS, buf, KEY_SUM, NULL };
        dvc_storeStream( duc, xwe, keys, stream );
        stream_destroy( stream );
    }
    XP_LOGFF( "stored group with id %d", grps->grp );
}

typedef struct  _SumStoreState {
    XW_DUtilCtxt* duc;
    XP_Bool deleteAfter;
} SumStoreState;

static ForEachAct
storeGroupProc( void* elem, void* closure, XWEnv xwe )
{
    SumStoreState* sss = (SumStoreState*)closure;
    GroupState* grps = (GroupState*)elem;

    storeGroup( sss->duc, xwe, grps );

    return FEA_OK;
}

static ForEachAct
storeGroupID( void* elem, void* closure, XWEnv XP_UNUSED(xwe) )
{
    XWStreamCtxt* stream = (XWStreamCtxt*)closure;
    GroupState* grps = (GroupState*)elem;
    XP_ASSERT( (0xFF & grps->grp) == grps->grp );
    XP_ASSERT( 0 < grps->grp );
    stream_putU8( stream, grps->grp );
    XP_LOGFF( "stored group with id %d", grps->grp );
    return FEA_OK;
}

static void
storeGroups( XW_DUtilCtxt* duc, XWEnv xwe, XP_Bool deleteAfter )
{
    /* First, the array of group IDs */
    XWStreamCtxt* stream = dvc_makeStream( duc );
    GameMgrState* gs = duc->gameMgrState;
    stream_putU32VL( stream, arr_length(gs->groups) );
    XP_LOGFF( "storing %d groups", arr_length(gs->groups) );
    arr_map( gs->groups, xwe, storeGroupID, stream );
    const XP_UCHAR* keys[] = { KEY_GROUPS, KEY_IDS, NULL };
    dvc_storeStream( duc, xwe, keys, stream );

    stream_destroy( stream );

    /* now let each write its summary data */
    SumStoreState sss = { .duc = duc, .deleteAfter = deleteAfter};
    arr_map( gs->groups, xwe, storeGroupProc, &sss );
}

static void
loadGamesOnce( XW_DUtilCtxt* duc, XWEnv xwe )
{
    GameMgrState* gs = duc->gameMgrState;
    if ( !gs->loaded ) {
        gs->loaded = XP_TRUE;
        {
            const XP_UCHAR* keys[] = { KEY_GAMEMGR, KEY_STATE, NULL };
            XWStreamCtxt* stream = dvc_loadStream( duc, xwe, keys );
            if ( !!stream ) {
                gs->defaultGrp = stream_getU8(stream);
                stream_gotU8( stream, &gs->archiveGrp );
                stream_destroy( stream );
            }
        }
        
        loadGroups( duc, xwe, gs );
        LoadGamesState lgs = { .duc = duc, .xwe = xwe, };
        const XP_UCHAR* keys[] = { KEY_GAMES, "%", KEY_GI, NULL };
        dvc_getKeysLike( duc, xwe, keys, onGotGIKey, &lgs );

        if ( 0 ) {
            /* enable this after populating the thing */
            arr_map( gs->groups, xwe, enableSortProc, NULL );
        }
    }
}

GameRef
gmgr_newFor( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp, const CurGameInfo* gip,
             const CommsAddrRec* invitee )
{
    XP_ASSERT( !gip->gameID );
    checkDefault( duc, &grp );
    GameRef gr = gr_makeForGI( duc, xwe, &grp, gip, NULL );
    addToGroup( duc, xwe, gr, grp );
    if ( !!invitee ) {
        NetLaunchInfo nli = {};
        const CurGameInfo* gi = gr_getGI(duc, gr, xwe);
        nli_init( &nli, gi, invitee, 1, 0 );
        gr_invite( duc, gr, xwe, &nli, invitee, XP_TRUE );
    }
    scheduleOnGameAdded( duc, xwe, gr );
    return gr;
}

GameRef
gmgr_addForInvite( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp,
                   const NetLaunchInfo* nli )
{
    GameRef gr = 0;
    if ( !gmgr_haveGame( duc, xwe, nli->gameID, SERVER_ISCLIENT ) ) {
        CurGameInfo gi = {};
        nliToGI( MPPARM(duc->mpool) duc, xwe, nli, &gi );
        if ( !gi.created ) {
            gi.created = dutil_getCurSeconds( duc, xwe );
        }

        CommsAddrRec hostAddr = {};
        nli_makeAddrRec( nli, &hostAddr );

        checkDefault( duc, &grp );
        gr = gr_makeForGI( duc, xwe, &grp, &gi, &hostAddr );

        addToGroup( duc, xwe, gr, grp );

        gi_disposePlayerInfo( MPPARM(duc->mpool) &gi );
        scheduleOnGameAdded( duc, xwe, gr );
    }
    return gr;
}

typedef struct _GetGameState {
    XW_DUtilCtxt* duc;
    XWEnv xwe;
    XP_U32 gameID;
    GameRef found;
} GetGameState;

static void
getForGIKey( const XP_UCHAR* key, void* closure, XWEnv XP_UNUSED(xwe) )
{
    GetGameState* ggs = (GetGameState*)closure;
    if ( !ggs->found ) {
        GameRef gr = grFromKey( key );
        XP_U32 gameID = gameIDFromGR( gr );
        if ( gameID == ggs->gameID ) {
            ggs->found = gr;
        }
    }
}

typedef struct _FindGIDState {
    XP_U32 gameID;
    XW_DUtilCtxt* duc;
    GameRef* refs;
    XP_U16 maxRefs;
    XP_U16 nFound;
} FindGIDState;

static ForEachAct
findWithGID( void* elem, void* closure, XWEnv xwe )
{
    ForEachAct result = FEA_OK;
    GameEntry* ge = (GameEntry*)elem;
    FindGIDState* fgrs = (FindGIDState*)closure;
    XP_USE(fgrs);
    GameRef gr = ge->gr;
    if ( gr ) {
        const CurGameInfo* gi = gr_getGI( fgrs->duc, gr, xwe );
        if ( gi->gameID == fgrs->gameID ) {
            XP_ASSERT( fgrs->nFound < fgrs->maxRefs );
            fgrs->refs[fgrs->nFound++] = gr;
        }
    }
    return result;
}

void
gmgr_getForGID( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 gameID,
                GameRef refs[], XP_U16* nRefs )
{
    loadGamesOnce( duc, xwe );
    /* first check the in-memory array. Everything should be there now. */
    FindGIDState fgs = { .gameID = gameID, .duc = duc,
                         .refs = refs, .maxRefs = *nRefs };
    GameMgrState* gs = duc->gameMgrState;
    arr_map( gs->list, xwe, findWithGID, &fgs );

    if ( 0 == fgs.nFound ) {
        XP_LOGFF( "not found in memory; going after key" );
        GetGameState ggs = { .duc = duc, .xwe = xwe, .gameID = gameID, };
        const XP_UCHAR* keys[] = { KEY_GAMES, KEY_GI, NULL };
        dvc_getKeysLike( duc, xwe, keys, getForGIKey, &ggs );
        if ( ggs.found ) {
            fgs.refs[fgs.nFound++] = ggs.found;
        }
    }
    *nRefs = fgs.nFound;
    XP_LOGFF( "(%X) => " GR_FMT ", count=%d", gameID, refs[0], *nRefs );
}

GameData*
gmgr_getForRef( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr, XP_Bool* deletedP )
{
    loadGamesOnce( duc, xwe );
    XP_Bool deleted = XP_FALSE;
    GameEntry* ge = findFor( duc, xwe, gr, XP_TRUE, &deleted );

    GameData* result = NULL;
    if ( !!ge ) {
        result = ge->gd;
        if ( deletedP ) {
            *deletedP = deleted;
        }
    }
    // XP_LOGFF( "(" GR_FMT ") => %p", gr, result );
    return result;
}

void
gmgr_setGD( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr, GameData* gd )
{
    XP_ASSERT( gd );

    GameEntry* ge = findFor( duc, xwe, gr, XP_FALSE, NULL );
    if ( !!ge ) {
        XP_ASSERT( !ge->gd );
        ge->gd = gd;
    } else {
        insertGr( duc, xwe, gr, gd );
    }
}

static void
gmgr_addGR( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr )
{
    insertGr( duc, xwe, gr, NULL );
}

void
gmgr_addGame( XW_DUtilCtxt* duc, XWEnv xwe, GameData* gd, GameRef gr )
{
    XP_LOGFF( "(gr: " GR_FMT ")", gr);

    XP_ASSERT( !gmgr_getForRef( duc, xwe, gr, NULL ) );

    insertGr( duc, xwe, gr, gd );
    gmgr_saveGame( duc, xwe, gr );
}

void
gmgr_onMessageReceived(XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 gameID,
                       const CommsAddrRec* from, XP_U8* msgBuf,
                       XP_U16 msgLen, const MsgCountState* mcs )
{
    GameRef grs[3];
    XP_U16 nRefs = VSIZE(grs);
    gmgr_getForGID( duc, xwe, gameID, grs, &nRefs );
    for ( int ii = 0; ii < nRefs; ++ii ) {
        gr_onMessageReceived( duc, grs[ii], xwe, from, msgBuf, msgLen, mcs );
    }
}

static ForEachAct
clearThumbProc( void* elem, void* XP_UNUSED(closure), XWEnv XP_UNUSED(xwe))
{
    ForEachAct result = FEA_OK;
    GameEntry* ge = (GameEntry*)elem;
    if ( !!ge->gd ) {
        XP_ASSERT( ge->gr );
        gr_clearThumb( ge->gd );
    }
    return result;
}

void
gmgr_clearThumbnails( XW_DUtilCtxt* duc, XWEnv xwe )
{
    GameMgrState* gs = duc->gameMgrState;
    arr_map( gs->list, xwe, clearThumbProc, NULL );
}

static XP_U32
gameIDFromGR( GameRef gr )
{
    XP_ASSERT( gr == (gr & 0x00000003FFFFFFFF));
    return gr & 0x00000000FFFFFFFF;
}


static void
callOnGameAddedProc( XW_DUtilCtxt* duc, XWEnv xwe, void* closure,
                     TimerKey XP_UNUSED(key), XP_Bool fired )
{
    if ( fired ) {
        GameRef gr = (GameRef)closure;
        dutil_onGameChanged( duc, xwe, gr, GCE_ADDED );
    }
}

void
scheduleOnGameAdded( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr )
{
    tmr_setIdle( duc, xwe, callOnGameAddedProc, (void*)gr );
}

static void
checkMakeArchive( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef* grpp )
{
    GroupRef grp = *grpp;
    if ( grp == GROUP_ARCHIVE ) {
        GameMgrState* gs = duc->gameMgrState;
        if ( !gs->archiveGrp ) {
            const XP_UCHAR* groupName =
                dutil_getUserString( duc, xwe, STRS_GROUPS_ARCHIVE );
            GroupRef agroup = gmgr_addGroup( duc, xwe, groupName );
            gs->archiveGrp = agroup;
        }
        *grpp = gs->archiveGrp;
    }
}

static void
checkDefault( XW_DUtilCtxt* duc, GroupRef* grpp )
{
    if ( GROUP_DEFAULT == *grpp ) {
        GameMgrState* gs = duc->gameMgrState;
        *grpp = gs->defaultGrp;
    }
}

typedef struct _GroupEventState {
    GroupRef grp;
    GroupChangeEvents pendingEvents;
} GroupEventState;

typedef struct _FindPGEState {
    GroupRef sought;
    GroupEventState* ges;
} FindPGEState;

static ForEachAct
findPGEProc( void* elem, void* closure, XWEnv XP_UNUSED(xwe))
{
    ForEachAct result = FEA_OK;
    GroupEventState* ges = (GroupEventState*)elem;
    FindPGEState* fps = (FindPGEState*)closure;
    if ( fps->sought == ges->grp ) {
        fps->ges = ges;
        result |= FEA_EXIT;
    }
    return result;
}

static GroupEventState*
getGES( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp, XP_Bool makeMissing )
{
    GameMgrState* gs = duc->gameMgrState;
    XWArray* pendingGroupEvents = gs->pendingGroupEvents;
    FindPGEState findPGEState = { .sought = grp, };
    arr_map( pendingGroupEvents, xwe, findPGEProc, &findPGEState );
    GroupEventState* ges = findPGEState.ges;
    if ( !ges && makeMissing ) {
        ges = XP_CALLOC( duc->mpool, sizeof(*ges) );
        ges->grp = grp;
        arr_insert( pendingGroupEvents, xwe, ges );
    }
    return ges;
}

static void
removeGES( XW_DUtilCtxt* duc, XWEnv xwe, GroupEventState* ges )
{
    GameMgrState* gs = duc->gameMgrState;
    XWArray* pendingGroupEvents = gs->pendingGroupEvents;
    arr_remove( pendingGroupEvents, xwe, ges );
    XP_FREEP( duc->mpool, &ges );
}

static void
sendGroupEventProc( XW_DUtilCtxt* duc, XWEnv xwe, void* closure,
                    TimerKey XP_UNUSED(key), XP_Bool fired )
{
    GroupRef grp = (GroupRef)(long)closure;
    GroupEventState* ges = getGES( duc, xwe, grp, XP_FALSE );
    XP_ASSERT( !!ges );
    if ( fired ) {
        GroupChangeEvents pendingEvents = ges->pendingEvents;
        if ( pendingEvents ) {
            dutil_onGroupChanged( duc, xwe, ges->grp, pendingEvents );
        }
    }
    removeGES( duc, xwe, ges );
}

static void
postOnGroupChanged( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp,
                    GroupChangeEvent grce )
{
    GroupEventState* ges = getGES( duc, xwe, grp, XP_TRUE );
    if ( !ges->pendingEvents ) { /* is it already set? */
        tmr_setIdle( duc, xwe, sendGroupEventProc, (void*)(long)grp );
    }
    ges->pendingEvents |= grce;
}

static void
saveGMStateProc( XW_DUtilCtxt* duc, XWEnv xwe, void* XP_UNUSED(closure),
                 TimerKey XP_UNUSED(key), XP_Bool fired )
{
    if ( fired ) {
        GameMgrState* gs = duc->gameMgrState;
        if ( gs->saveStatePending ) {
            gs->saveStatePending = XP_FALSE;
            XWStreamCtxt* stream = dvc_makeStream( duc );
            stream_putU8( stream, gs->defaultGrp );
            stream_putU8( stream, gs->archiveGrp );
            const XP_UCHAR* keys[] = { KEY_GAMEMGR, KEY_STATE, NULL };
            dvc_storeStream( duc, xwe, keys, stream );
            stream_destroy( stream );

            storeGroups( duc, xwe, XP_FALSE );
        }
    }
}

static void
scheduleSaveState( XW_DUtilCtxt* duc, XWEnv xwe )
{
    GameMgrState* gs = duc->gameMgrState;
    if ( !gs->saveStatePending ) {
        gs->saveStatePending = XP_TRUE;
        tmr_setIdle( duc, xwe, saveGMStateProc, NULL );
    }
}

static XP_U32
numGames( const GroupState* grps )
{
    XP_U32 result = grps->collapsed ? grps->u.nGames : arr_length(grps->u.games);
    return result;
}
