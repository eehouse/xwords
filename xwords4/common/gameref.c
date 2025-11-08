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

#include "gamerefp.h"
#include "gamemgrp.h"
#include "gamep.h"
#include "strutils.h"
#include "devicep.h"
#include "gamemgr.h"
#include "stats.h"
#include "timers.h"
#include "dictmgrp.h"
#include "dbgutil.h"

#define FLAG_HASCOMMS 0x01

typedef struct ProcState ProcState;
struct ProcState {
    ProcState* prev;
    pthread_t thread;
    const char* proc;
    const char* file;
    int line;
};

typedef struct _GameChangeEvtData {
    GameData* gd;
    GameChangeEvents gces;
} GameChangeEvtData;

struct GameData {
    GameRef gr;
    ProcState* ps;
    CurGameInfo gi;
    GameSummary sum;
    XP_Bool sumLoaded;
    XP_Bool commsLoaded;
    GroupRef grp;
    XW_UtilCtxt* util;
    BoardCtxt* board;
    ModelCtxt* model;
    CtrlrCtxt* ctrlr;
    CommsCtxt* comms;
    CommsAddrRec hostAddr; /* hack: store until can init game */
    XWStreamCtxt* thumbData;

    XP_Bool dictsSought;   /* don't keep looking for missing stuff */
    const DictionaryCtxt* dict;
    PlayerDicts playerDicts;

    GameChangeEvtData* gcedp;
    TimerKey saveTimerKey;
    TimerKey drawTimerKey;
    MPSLOT
};

/* Not persisted, so add/remove at will */
typedef enum { GD, GI, SUM, UTIL, COMMS, DICTS, MODEL, BOARD, } NeedsLevel;

static GameData* loadToLevel( XW_DUtilCtxt* duc, GameRef gr, XWEnv xwe,
                              NeedsLevel nl, XP_Bool* loaded,
                              XP_Bool* deleted );
static void makeData( XW_DUtilCtxt* duc, XWEnv xwe, GameData* gd );
static void loadData( XW_DUtilCtxt* duc, XWEnv xwe,
                      GameData* gd, XWStreamCtxt** streamp );
static void setGIImpl( XW_DUtilCtxt* duc, XWEnv xwe, GameData* gd,
                       const CurGameInfo* gip );

static void setSaveTimer(XW_DUtilCtxt* duc, XWEnv xwe, GameData* gd,
                         GameRef gr);
static void thumbChanged( XW_DUtilCtxt* duc, XWEnv xwe, GameData* gd );
static void unrefDicts( XWEnv xwe, GameData* gd );
static void summarize( XW_DUtilCtxt* duc, XWEnv xwe, GameData* gd );
static void updateSummary( XW_DUtilCtxt* duc, XWEnv xwe,
                           GameData* gd, const GameSummary* newSum );
static void saveSummary( XW_DUtilCtxt* duc, XWEnv xwe, GameData* gd );
static void sumToStream( XWStreamCtxt* stream, const GameSummary* sum,
                         XP_U16 nPlayers );
static XP_Bool gotSumFromStream( GameSummary* sum, XWStreamCtxt* stream );
static void postGameChangeEvent( XW_DUtilCtxt* duc, XWEnv xwe, GameData* gd,
                                 GameChangeEvent evt );
static XW_UtilCtxt* makeDummyUtil( XW_DUtilCtxt* duc, GameData* gd );

#define PROC_PUSH(GD) {                                                 \
    ProcState _ps = {};                                                 \
    GameData* _gd = (GD);                                               \
    if ( !!_gd ) {                                                      \
        pthread_t _self = pthread_self();                               \
        pthread_t _prev = (!!_gd->ps) ? _gd->ps->thread : 0;            \
        _ps.prev = _gd->ps;                                             \
        _ps.proc = __func__;                                            \
        _ps.line = __LINE__;                                            \
        _ps.file = __FILE__;                                            \
        _ps.thread = _self;                                             \
        if ( 0 != _prev && _self != _prev ) {                           \
            XP_LOGFF( "oops: new thead %lX on top of old %lX", _self, _prev ); \
            XP_LOGFF( "new proc: %s; old: %s", __func__, _ps.prev->proc ); \
            XP_ASSERT(0);                                               \
        }                                                               \
        _gd->ps = &_ps;                                                 \
    }                                                                   \

#define PROC_POP()                              \
    if ( !!_gd ) {                              \
        _gd->ps = _ps.prev;                     \
    }                                           \
    }                                           \

#define GR_HEADER_WITH(NL) {                                            \
    XP_ASSERT( gr );                                                    \
    /* XP_LOGFF("(gr=" GR_FMT ")", gr); */                              \
    XP_Bool loaded;                                                     \
    XP_Bool deleted;                                                    \
    GameData* gd = loadToLevel(duc, gr, xwe, NL, &loaded, &deleted);    \
    if ( loaded && !deleted ) {                                         \
    PROC_PUSH(gd);                                                      \

#define GR_HEADER() GR_HEADER_WITH(BOARD)

#define GR_HEADER_END()                                             \
    PROC_POP();                                                     \
    } else {                                                        \
        XP_LOGFF( "%s() failed: deleted or can't load", __func__ ); \
    }                                                               \
} /* LOG_RETURN_VOID() */

#define GR_HEADER_END_SAVE()                    \
    setSaveTimer(duc, xwe, gd, gr);             \
    GR_HEADER_END()                             \

static void
loadCommsOnce(XW_DUtilCtxt* duc, XWEnv xwe, GameData* gd )
{
    if ( !gd->commsLoaded ) {
        gd->commsLoaded = XP_TRUE;
        const CurGameInfo* gi = &gd->gi;
        if ( ROLE_STANDALONE != gi->deviceRole ) {
            XWStreamCtxt* stream = gmgr_loadComms( duc, xwe, gd->gr );
            if ( !!stream ) {
                XP_U8 strVersion = strm_getU8( stream );
                XP_LOGFF( "strVersion: 0x%X", strVersion );
                strm_setVersion( stream, strVersion );
                gd->comms = comms_makeFromStream( xwe, stream, &gd->util,
                                                  gi->deviceRole != ROLE_ISGUEST,
                                                  gi->forceChannel );
                strm_destroy( stream );
            } else {
                XP_Bool isClient = ROLE_ISGUEST == gi->deviceRole;
                const CommsAddrRec* hostAddr = isClient ? &gd->hostAddr : NULL;
                CommsAddrRec selfAddr = {0};
                dutil_getSelfAddr( duc, xwe, &selfAddr );
                XW_UtilCtxt** utilp = &gd->util;
                gd->comms = comms_make( xwe, utilp, gi->deviceRole != ROLE_ISGUEST,
                                        &selfAddr, hostAddr, gi->forceChannel );
            }
        } else {
            XP_ASSERT( !gd->comms );
        }
    }
}

static void
loadDictsOnce( XW_DUtilCtxt* dutil, XWEnv xwe, GameData* gd )
{
    if ( !gd->dictsSought ) {
        gd->dictsSought = XP_TRUE;
        const CurGameInfo* gi = &gd->gi;
        gd->dict = dmgr_get( dutil, xwe, gi->dictName );

        XP_MEMSET( &gd->playerDicts, 0, sizeof(gd->playerDicts) );
        if ( !!gd->dict ) {
            for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
                const LocalPlayer* lp = &gi->players[ii];
                if ( lp->isLocal && !!lp->dictName[0] ) {
                    gd->playerDicts.dicts[ii] = dmgr_get( dutil, xwe, lp->dictName );
                }
            }
        }
    }
}

static GameData*
makeGD( GameRef gr )
{
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = mpool_make( __func__ );
#endif
    GameData* gd = XP_CALLOC( mpool, sizeof(*gd) );
    MPASSIGN( gd->mpool, mpool );
    gd->gr = gr;
    return gd;
}

static GameData*
loadToLevel( DUTIL_GR_XWE, NeedsLevel target,
             XP_Bool* loadedP, XP_Bool* deletedP )
{
    GameData* gd = gmgr_getForRef( duc, xwe, gr, deletedP );
    XP_Bool loaded = XP_TRUE;
    for ( NeedsLevel nl = 0; nl <= target; ++nl ) {
        switch ( nl ) {
        case GD:
            if ( !gd ) {
                gd = makeGD( gr );
                gd->gr = gr;
                gd->grp = gmgr_loadGroupRef( duc, xwe, gr );
                XP_LOGFF( "set group %d for gr " GR_FMT, gd->grp, gr );
                gmgr_setGD( duc, xwe, gr, gd );
            }
            break;
        case GI: {
            CurGameInfo* gi = &gd->gi;
            if ( !gi_isValid(gi) ) {
                XWStreamCtxt* stream = gmgr_loadGI( duc, xwe, gr );
                XP_ASSERT( !!stream );
                XP_U8 strVersion = strm_getU8( stream );
                XP_LOGFF( "strVersion: 0x%X", strVersion );
                strm_setVersion( stream, strVersion );

                gi_readFromStream( stream, gi );
                XP_ASSERT( gi_isValid(gi) );
                XP_ASSERT( gi->gameID == (gr & 0xFFFFFFFF) );
                XP_ASSERT( gi->created );
                strm_destroy( stream );
                LOG_GI( gi, __func__ );
            }
        }
            break;
        case SUM: {
            if ( !gd->sumLoaded ) {
                gd->sumLoaded = XP_TRUE;
                XWStreamCtxt* stream = gmgr_loadSum( duc, xwe, gr );
                if ( !!stream ) {
                    if ( gotSumFromStream( &gd->sum, stream ) ) {
                    }
                    strm_destroy( stream );
                }
            }
        }
            break;
        case UTIL:
            if ( !gd->util ) {
                /* util comes back with refcount of 1. so we don't have to ref */
                gd->util = makeDummyUtil( duc, gd );
            }
            break;
        case COMMS:
            loadCommsOnce(duc, xwe, gd );
            // Eventually comms should be loaded first, maybe before dict, as
            // it doesn't need dict but DOES need to exist on guest in order
            // to preserve hostAddr of inviting device.
            break;
        case DICTS:
            loadDictsOnce( duc, xwe, gd );
            /* if ( !gd->dict ) { */
            /*     loaded = XP_FALSE; */
            /*     goto done; */
            /* } */
            break;
        case MODEL:
            if ( !gd->model ) {
                XWStreamCtxt* stream = gmgr_loadData( duc, xwe, gr );
                if ( !stream ) {
                    makeData( duc, xwe, gd );
                    if ( !!gd->model ) {
                        summarize( duc, xwe, gd );
                        setSaveTimer( duc, xwe, gd, gr );
                    }
                } else {
                    loadData( duc, xwe, gd, &stream );
                }
            }
            if ( !gd->model ) {
                loaded = XP_FALSE;
                goto done;
            }
            break;
        case BOARD:
            XP_ASSERT( !!gd->board );
            break;
        default:
            XP_LOGFF( "Unexpected NeedsLevel %d", nl );
            XP_ASSERT(0);
        }
    }
 done:
    *loadedP = loaded;
    return gd;
} /* loadToLevel */

typedef struct _SaveTimerData {
    GameRef gr;
    GameData* gd;
} SaveTimerData;

static void
saveTimerProc(XW_DUtilCtxt* duc, XWEnv xwe, void* closure, TimerKey XP_UNUSED(key),
              XP_Bool fired)
{
    SaveTimerData* std = (SaveTimerData*)closure;
    if ( fired ) {
        XP_LOGFF( "calling gr_save()");
        SaveTimerData* std = (SaveTimerData*)closure;

        gr_save( duc, std->gr, xwe );

    }
    std->gd->saveTimerKey = 0;
    XP_FREE( duc->mpool, closure );
}

static void
setSaveTimer( XW_DUtilCtxt* duc, XWEnv xwe, GameData* gd, GameRef gr )
{
    if ( !gd->saveTimerKey ) {
        XP_LOGFF( "setting timer");
        XP_U32 inWhenMS = 5 * 1000;
        SaveTimerData* std = XP_CALLOC( duc->mpool, sizeof(*std) );
        std->gr = gr;
        std->gd = gd;
        gd->saveTimerKey = tmr_set( duc, xwe, inWhenMS,
                                    saveTimerProc, std );
    }
}

static void
drawTimerProc( XW_DUtilCtxt* XP_UNUSED(duc), XWEnv xwe, void* closure,
               TimerKey XP_UNUSED(key), XP_Bool fired )
{
    GameData* gd = (GameData*)closure;
    gd->drawTimerKey = 0;
    if ( fired && gd->board ) {
        board_draw( gd->board, xwe );
    }
}

static void
schedule_draw( XW_DUtilCtxt* duc, XWEnv xwe, GameData* gd )
{
    if ( !gd->drawTimerKey ) {
        XP_U32 inWhenMS = 10;
        gd->drawTimerKey = tmr_set( duc, xwe, inWhenMS, drawTimerProc, gd );
    } else {
        XP_LOGFF( "timer already set for " GR_FMT , gd->gr );
    }
}

void
gr_giToStream( XW_DUtilCtxt* duc, GameRef gr, XWEnv xwe, XWStreamCtxt* stream )
{
    strm_putU8( stream, CUR_STREAM_VERS );
    strm_setVersion( stream, CUR_STREAM_VERS );
    XP_Bool deleted;
    GameData* gd = gmgr_getForRef(duc, xwe, gr, &deleted);
    XP_ASSERT( !deleted );
    XP_ASSERT( !!gd );
    const CurGameInfo* gi = &gd->gi;
    XP_ASSERT( gi->gameID );
    XP_ASSERT( gi->gameID == (0xFFFFFFFF & gr) );
    gi_writeToStream( stream, gi );
}

void
gr_dataToStream( DUTIL_GR_XWE, XWStreamCtxt* commsStream,
                 XWStreamCtxt* stream, XP_U16 saveToken )
{
    GR_HEADER();
    if ( !!gd->comms ) {
        XP_ASSERT( START_OF_STREAM == strm_getPos( commsStream, POS_READ ) );
        strm_putU8( commsStream, CUR_STREAM_VERS );
        strm_setVersion( commsStream, CUR_STREAM_VERS );
        comms_writeToStream( gd->comms, commsStream, saveToken );
    } else {
        XP_ASSERT( !commsStream );
    }

    XP_ASSERT( START_OF_STREAM == strm_getPos( stream, POS_READ ) );
    strm_putU8( stream, CUR_STREAM_VERS );
    strm_setVersion( stream, CUR_STREAM_VERS );

    /* bad? having only comms set is now a thing!!! */
    XP_ASSERT( !!gd->model );

    model_writeToStream( gd->model, xwe, stream );
    ctrl_writeToStream( gd->ctrlr, stream );
    board_writeToStream( gd->board, stream );
    GR_HEADER_END();
}

static void
unloadComms( GameData* gd, XWEnv xwe )
{
    if ( !!gd->comms ) {
        comms_stop( gd->comms );
        comms_destroy( gd->comms, xwe );
        gd->comms = NULL;
        gd->commsLoaded = XP_FALSE;
    }
}

static void
unloadData( GameData* gd, XWEnv xwe )
{
    if ( !!gd->board ) {
        board_destroy( gd->board, xwe, XP_FALSE );
        gd->board = NULL;
        if ( !!gd->comms ) {
            comms_stop( gd->comms );
            comms_destroy( gd->comms, xwe );
            gd->comms = NULL;
            gd->commsLoaded = XP_FALSE;
        }
        model_destroy( gd->model, xwe );
        ctrl_destroy( gd->ctrlr );
        gd->model = NULL;
        gd->ctrlr = NULL;
    }
}

static void
destroyData( XW_DUtilCtxt* XP_UNUSED(duc), XWEnv xwe,
             GameData* gd )
{
    if ( !!gd ) {
        unloadData( gd, xwe );
        destroyStreamIf( &gd->thumbData );

        unloadComms( gd, xwe );

        unrefDicts( xwe, gd );

        XW_UtilCtxt* util = gd->util;
#ifdef MEM_DEBUG
        MemPoolCtx* mpool = gd->mpool;
        gd->mpool = NULL;
#endif
        XP_FREE( mpool, gd );
        if ( !!util ) {
            XP_ASSERT( 1 == util->refCount );
            util_unref( util, xwe );
        }
        XP_LOGFF( "calling mpool_destroy()" );
        mpool_destroy( mpool );
    }
}

static void
timerChangeListener( XWEnv xwe, void* closure, GameRef gr,
                     XP_S32 oldVal, XP_S32 newVal )
{
    XP_ASSERT(0);
    XP_USE(xwe);
    XP_USE(closure);
    XP_USE(gr);
    XP_USE(oldVal);
    XP_USE(newVal);
    /* GameRef gr = (GameRef)data; */
    /* XP_ASSERT( gd->util->gameInfo->gameID == gameID ); */
    /* XP_LOGFF( "(oldVal=%d, newVal=%d, id=%d)", oldVal, newVal, gameID ); */
    /* dutil_onDupTimerChanged( util_getDevUtilCtxt( gd->util, xwe ), xwe, */
    /*                          gameID, oldVal, newVal ); */
}

static void
setListeners( XW_DUtilCtxt* dutil, XWEnv xwe, GameData* gd )
{
    CommonPrefs cp = {};
    dutil_getCommonPrefs( dutil, xwe, &cp );
    ctrl_prefsChanged( gd->ctrlr, &cp );
    board_prefsChanged( gd->board, xwe, &cp );
    ctrl_setTimerChangeListener( gd->ctrlr, timerChangeListener,
                                   (void*)gd );
}

static void
unrefDicts( XWEnv xwe, GameData* gd )
{
    dict_unref( gd->dict, xwe );
    for ( int ii = 0; ii < VSIZE(gd->playerDicts.dicts); ++ii ) {
        const DictionaryCtxt* dict = gd->playerDicts.dicts[ii];
        dict_unref( dict, xwe );
    }
}

static void
initClientProc( XW_DUtilCtxt* duc, XWEnv xwe, void* closure,
               TimerKey XP_UNUSED(key), XP_Bool fired )
{
    if ( fired ) {
        GameData* gd = (GameData*)closure;
        if ( !!gd->ctrlr ) {
            ctrl_initClientConnection( gd->ctrlr, xwe );
            summarize( duc, xwe, gd );
        }
    }
}

static void
finishSetup( XW_DUtilCtxt* duc, XWEnv xwe, GameData* gd )
{
    board_setCallbacks( gd->board, xwe );
    setListeners( duc, xwe, gd );
    ctrl_addIdle( gd->ctrlr, xwe );

    if ( gd->gi.deviceRole == ROLE_ISGUEST
         && !ctrl_getGameIsConnected( gd->ctrlr ) ) {
        tmr_setIdle( duc, xwe, initClientProc, gd );
    }
}

static void
makeData( XW_DUtilCtxt* duc, XWEnv xwe, GameData* gd )
{
    LOG_FUNC();
    XP_ASSERT( gd->util );
    XW_UtilCtxt** utilp = &gd->util;
    const CurGameInfo* gi = &gd->gi;
    XP_ASSERT( !!gi->dictName[0] );
    XP_ASSERT( gi_isValid( gi ) );
    LOG_GI(gi, __func__);
    XP_ASSERT( gi->gameID );

    XP_Bool success = !!gd->dict;

    if ( success ) {
        const XP_UCHAR* iso = dict_getISOCode( gd->dict );
        if ( !!iso ) {
            CurGameInfo tmp = {};
            gi_copy( &tmp, gi );
            XP_STRNCPY( tmp.isoCodeStr, iso, VSIZE(tmp.isoCodeStr) );
            XP_ASSERT( !!gi->isoCodeStr[0] );
            setGIImpl( duc, xwe, gd, &tmp );
        }
        gd->model = model_make( xwe, (DictionaryCtxt*)NULL, NULL, utilp,
                                gi->boardSize );
        XP_LOGFF( "set model: %p", gd->model );
        if ( !!iso ) {
            XP_ASSERT( !!gd->dict );
            model_setDictionary( gd->model, xwe, gd->dict );
            model_setPlayerDicts( gd->model, xwe, &gd->playerDicts );
        } else {
            XP_ASSERT(0);
        }

        gd->ctrlr = ctrl_make( xwe, gd->model, gd->comms, utilp );
        gd->board = board_make( xwe, gd->model, gd->ctrlr, NULL, utilp );

        finishSetup( duc, xwe, gd );

        STAT stat = STAT_NONE;
        if ( !gd->comms ) {
            stat = STAT_NEW_SOLO;
        } else switch ( gi->nPlayers ) {
            case 2: stat = STAT_NEW_TWO; break;
            case 3: stat = STAT_NEW_THREE; break;
            case 4: stat = STAT_NEW_FOUR; break;
        }
        sts_increment( duc, xwe, stat );
    }

    LOG_RETURN_VOID();
} /* makeData */

static void
resendAllProc( XW_DUtilCtxt* XP_UNUSED(duc), XWEnv xwe, void* closure,
               TimerKey XP_UNUSED(key), XP_Bool fired )
{
    if ( fired ) {
        GameData* gd = (GameData*)closure;
        if ( gd->comms ) {
            comms_resendAll( gd->comms, xwe, COMMS_CONN_NONE, XP_TRUE );
        }
    }
}

static void
loadData( XW_DUtilCtxt* duc, XWEnv xwe, GameData* gd, XWStreamCtxt** streamp )
{
    const CurGameInfo* gi = &gd->gi;
    XP_ASSERT( gi_isValid(gi) );
    XP_ASSERT( START_OF_STREAM == strm_getPos( *streamp, POS_READ ) );
    XP_U8 strVersion = strm_getU8( *streamp );
    XP_LOGFF( "strVersion: 0x%X", strVersion );
    strm_setVersion( *streamp, strVersion );

    XP_ASSERT( gi->gameID );
    XP_ASSERT( gi->created );
    if ( !gd->dict ) {
        goto exit;
    }

    gd->model = model_makeFromStream( xwe, *streamp, gd->dict,
                                      &gd->playerDicts, &gd->util );
    gd->ctrlr = ctrl_makeFromStream( xwe, *streamp,
                                        gd->model, gd->comms,
                                        &gd->util, gi->nPlayers );
    gd->board = board_makeFromStream( xwe, *streamp, gd->model,
                                      gd->ctrlr, NULL, &gd->util,
                                      gi->nPlayers );
    finishSetup( duc, xwe, gd );

    if ( gd->comms && gd->ctrlr ) {
        XP_ASSERT( comms_getIsHost(gd->comms) == ctrl_getIsHost(gd->ctrlr) );
#ifdef XWFEATURE_KNOWNPLAYERS
        const XP_U32 created = gi->created;
        if ( 0 != created
             && ctrl_getGameIsConnected( gd->ctrlr ) ) {
            ctrl_gatherPlayers( gd->ctrlr, xwe, created );
        }
#endif
        XP_Bool quashed;
        if ( 0 < comms_countPendingPackets( gd->comms, &quashed )
             && !quashed ) {
            tmr_setIdle( duc, xwe, resendAllProc, gd );
        }
    }

 exit:
    strm_destroy( *streamp );
    *streamp = NULL;
    return;
} /* loadData */

static XP_U32
makeGameID( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 curID )
{
    XP_U32 newID = curID;
    while ( 0 == newID ) {
#ifdef SEQUENTIAL_GIDS
        const XP_UCHAR* key = __FILE__ "/nextGID";
        XP_U32 len = sizeof(newID);
        dutil_loadPtr( duc, xwe, key, &newID, &len );
        if ( !len ) {
            newID = 0;
        }
        ++newID;
        dutil_storePtr( duc, xwe, key, &newID, sizeof(newID) );
#else
        XP_USE(duc);
        XP_USE(xwe);
        /* High bit never set by XP_RANDOM() alone */
        newID = (XP_RANDOM() << 16) ^ XP_RANDOM();
#endif
        /* But let's clear it -- set high-bit causes problems for existing
           postgres DB where INTEGER is apparently a signed 32-bit. Recently
           confirmed that working around that in the code that moves between
           incoming web api calls and postgres would be much harder than using
           8-char hex strings instead. But I'll leave the test change in
           place. */
#ifdef HIGH_GAMEID_BITS
        newID |= 0x80000000;
#else
        newID &= ~0x80000000;
#endif
    }
    XP_LOGFF( "(%08X) => %08X", curID, newID );
    return newID;
}

GameRef
gr_makeForGI( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef* grp,
              const CurGameInfo* gip, const CommsAddrRec* hostAddr )
{
    GameRef gr = 0;
    CurGameInfo gi = *gip;
    /* Game coming from an invitation received will have a gameID already */
    gi.gameID = makeGameID( duc, xwe, gi.gameID );
    if ( !gi.created ) {
        gi.created = dutil_getCurSeconds( duc, xwe );
    }

    if ( gi_isValid(&gi) ) {
        if ( !gi.isoCodeStr[0] ) {
            const DictionaryCtxt* dict = dmgr_get( duc, xwe, gi.dictName );
            XP_STRNCPY( gi.isoCodeStr, dict_getISOCode( dict ),
                        VSIZE(gi.isoCodeStr) );
            dict_unref( dict, xwe );
        }

        gr = gi_formatGR( &gi );

        GameData* gd = gmgr_getForRef( duc, xwe, gr, NULL );
        if ( !!gd ) {
            XP_LOGFF( "gr " GR_FMT " already exists; doing nothing!", gr );
            gr = 0;
        } else {
            GameData* gd = makeGD( gr );
            if ( *grp == gmgr_getArchiveGroup(duc) || *grp == GROUP_DEFAULT ) {
                *grp = gmgr_getDefaultGroup(duc);
            }
            gd->grp = *grp;
            gi_copy( &gd->gi, &gi );

            gd->util = makeDummyUtil( duc, gd );
            XP_LOGFF( "created game with id %X", gd->gi.gameID );

            if ( !!hostAddr ) {
                gd->hostAddr = *hostAddr;
            }

            gmgr_addGame( duc, xwe, gd, gr );
            gmgr_addToGroup( duc, xwe, gr, gd->grp );
        }
    } else {
        LOG_GI( gip, __func__ );
        XP_LOGFF( "failing: bad gi" );
    }
    return gr;
}

#ifdef XWFEATURE_GAMEREF_CONVERT
GameRef
gr_convertGame( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef* grpp,
                const XP_UCHAR* gameName, XWStreamCtxt* stream )
{
    LOG_FUNC();
    XP_U8 strVersion = strm_getU8( stream );
    XP_LOGFF( "got strVersion: 0x%x", strVersion );
    strm_setVersion( stream, strVersion );
    GameRef gr;
    {
        CurGameInfo gi = gi_readFromStream2( stream );
        LOG_GI( &gi, __func__ );

        XP_U32 created = strVersion < STREAM_VERS_GICREATED
            ? 0 : strm_getU32( stream );
        if ( created && !gi.created ) {
            gi.created = created;
        }
        
        if ( STREAM_VERS_BIGGERGI > strVersion ) {
            str2ChrArray( gi.gameName, gameName );
        }
        LOG_GI( &gi, __func__ );

        if ( ROLE_STANDALONE != gi.deviceRole ) {
            /* Set something so test in gr_makeForGI() won't fail. We'll
               overwrite conTypes below anyway */
            types_addType( &gi.conTypes, COMMS_CONN_NFC );
        }
        gr = gr_makeForGI( duc, xwe, grpp, &gi, NULL );
    }
    
    if ( !!gr ) {
        GameData* gd = gmgr_getForRef( duc, xwe, gr, NULL );
        XP_ASSERT( !!gd );
        const CurGameInfo* gi = &gd->gi;
        XW_UtilCtxt* util = makeDummyUtil( duc, gd );

        XP_Bool hasComms;
        if ( strVersion < STREAM_VERS_GICREATED ) {
            hasComms = strm_getU8( stream );
        } else {
            XP_U8 flags = strm_getU8( stream );
            hasComms = flags & FLAG_HASCOMMS;
        }

        /* If we're loading old code, we need to figure out the conTypes field of
           the gi. If the game's in play, we can get it from addresses. But if it
           was newly created, our own default hostAddress is it??? */
        if ( hasComms ) {
            XP_ASSERT( !gd->comms );
            gd->comms = comms_makeFromStream( xwe, stream, &util,
                                              gi->deviceRole != ROLE_ISGUEST,
                                              gi->forceChannel );
            gd->commsLoaded = XP_TRUE;
            ConnTypeSetBits conTypes = 0;

            CommsAddrRec addrs[gi->nPlayers];
            XP_U16 nRecs = VSIZE(addrs);
            comms_getAddrs( gd->comms, addrs, &nRecs );
            for ( int ii = 0; ii < nRecs; ++ii ) {
                conTypes |= addrs[ii]._conTypes;
                XP_LOGFF( "conTypes now 0x%x", conTypes );
            }
            if ( !conTypes ) {
                CommsAddrRec selfAddr;
                comms_getSelfAddr( gd->comms, &selfAddr );
                conTypes |= selfAddr._conTypes;
            }
            XP_LOGFF( "conTypes now 0x%x", conTypes );

            /* Now add it to gi */
            CurGameInfo tmpGI = *gi;
            tmpGI.conTypes = conTypes;
            LOG_GI( &tmpGI, __func__ );
            setGIImpl( duc, xwe, gd, &tmpGI );
        }

        /* Now let's write the rest of the stream out so we can save it in the
           correct format */
        {
            loadDictsOnce( duc, xwe, gd );
            XP_ASSERT( !!gd->dict );
            gd->model = model_makeFromStream( xwe, stream, gd->dict,
                                              &gd->playerDicts, &gd->util );
            gd->ctrlr = ctrl_makeFromStream( xwe, stream,
                                             gd->model, gd->comms,
                                             &gd->util, gd->gi.nPlayers );
            gd->board = board_makeFromStream( xwe, stream, gd->model,
                                              gd->ctrlr, NULL, &gd->util,
                                              gd->gi.nPlayers );

            XWStreamCtxt* commsStream = !!gd->comms ? dvc_makeStream(duc) : NULL;
            XWStreamCtxt* dataStream = dvc_makeStream(duc);
            gr_dataToStream( duc, gr, xwe, commsStream, dataStream, 0 );
            gmgr_saveStreams( duc, xwe, gr, &commsStream, &dataStream, 0 );

            unloadData( gd, xwe );
            // unloadComms( gd, xwe );

            /* board_destroy( gd->board, xwe, XP_FALSE ); */
            /* gd->board = NULL; */
            /* ctrl_destroy( gd->ctrlr ); */
            /* gd->ctrlr = NULL; */
            /* model_destroy( gd->model, xwe ); */
            /* gd->model = NULL; */
            /* if ( !!gd->comms ) { */
            /*     comms_destroy( gd->comms, xwe ); */
            /*     gd->comms = NULL; */
            /*     gd->commsLoaded = XP_FALSE; */
            /* } */
        }

        XP_Bool loaded;
#ifdef DEBUG
        GameData* gd1 =
#endif
            loadToLevel( duc, gr, xwe, MODEL, &loaded, NULL );
        XP_ASSERT( gd1 == gd );
        XP_ASSERT( loaded );

        util_unref( util, xwe );
    }
    LOG_RETURNF( GR_FMT, gr );
    return gr;
} /* gr_convertGame */
#endif

static NetLaunchInfo
makeSelfNLI( GameData* gd, const XP_UCHAR* name,
             XP_U16 nPlayersH, XP_U16 forceChannel )
{
    NetLaunchInfo nli = {};

    CommsAddrRec selfAddr;
    comms_getSelfAddr( gd->comms, &selfAddr );
    nli_init( &nli, &gd->gi, &selfAddr, nPlayersH, forceChannel );
    if ( !!name ) {
        nli_setGameName( &nli, name );
    }
    return nli;
}

GameRef
gr_makeRematch( DUTIL_GR_XWE, const XP_UCHAR* newName, RematchOrder ro,
                XP_Bool archiveAfter, XP_Bool deleteAfter )
{
    GameRef newGR = 0;
    GR_HEADER();

    CurGameInfo tmpGI = gd->gi;
    tmpGI.gameID = 0;
    str2ChrArray( tmpGI.gameName, newName );
    if ( ROLE_ISGUEST == tmpGI.deviceRole ) {
        tmpGI.deviceRole = ROLE_ISHOST;
    }
    GroupRef grp = gd->grp;
    newGR = gr_makeForGI( duc, xwe, &grp, &tmpGI, NULL );
    if ( !!newGR ) {
        XP_LOGFF( "made new gr: " GR_FMT, newGR );
        XP_Bool deleted;
        GameData* newGd = gmgr_getForRef( duc, xwe, newGR, &deleted );
        XP_ASSERT( !!newGd );
        XP_ASSERT( !deleted );

        /* Now create a new gi to be modified but whose mempool is the new
           game's */
        CurGameInfo newGI = newGd->gi;

        RematchInfo* rip;
        if ( ctrl_getRematchInfo( gd->ctrlr, xwe, ro, &newGI, &rip ) ) {
            setGIImpl( duc, xwe, newGd, &newGI );

            XP_Bool loaded;
#ifdef DEBUG
            GameData* newGD2 =
#endif
                loadToLevel( duc, newGR, xwe, MODEL, &loaded, NULL );
            XP_ASSERT( loaded );
            XP_ASSERT( newGD2 == newGd );

            if ( !!newGd->comms ) {
                XP_ASSERT( 0 != newGd->gi.conTypes );

                ctrl_setRematchOrder( newGd->ctrlr, rip );

                for ( int ii = 0; ; ++ii ) {
                    CommsAddrRec guestAddr;
                    XP_U16 nPlayersH;
                    if ( !ctrl_ri_getAddr( rip, ii, &guestAddr, &nPlayersH )){
                        break;
                    }
                    NetLaunchInfo nli = makeSelfNLI( newGd, newName, nPlayersH, ii+1 );
                    LOGNLI( &nli );
                    comms_invite( newGd->comms, xwe, &nli, &guestAddr, XP_TRUE );
                }
            }
            ctrl_disposeRematchInfo( gd->ctrlr, rip );
        } else {
            XP_ASSERT(0);
            gmgr_deleteGame( duc, xwe, newGR );
            newGR = 0;
        }
    }

    GR_HEADER_END();

    if ( !!newGR ) {
        if ( archiveAfter ) {
            gmgr_moveGames( duc, xwe, GROUP_ARCHIVE, &gr, 1  );
        } else if ( deleteAfter ) {
            gmgr_deleteGame( duc, xwe, gr );
        }
    }
    return newGR;
}

void
gr_getSelfAddr( DUTIL_GR_XWE, CommsAddrRec* addr )
{
    GR_HEADER_WITH(COMMS);
    comms_getSelfAddr( gd->comms, addr );
    GR_HEADER_END();
}

XP_S16
gr_resendAll(DUTIL_GR_XWE, CommsConnType filter, XP_Bool force )
{
    XP_S16 result = -1;
    GR_HEADER_WITH(COMMS);
    result = comms_resendAll( gd->comms, xwe, filter, force );
    GR_HEADER_END();
    return result;
}

/* XP_Bool */
/* gr_getRematchInfo( DUTIL_GR_XWE, XW_UtilCtxt* newUtil, */
/*                    XP_U32 gameID, const NewOrder* nop, RematchInfo** ripp ) */
/* { */
/*     XP_Bool result = XP_FALSE; */
/*     GR_HEADER(); */
/*     XP_ASSERT(0); */
/*     /\* result = ctrl_getRematchInfo( gd->ctrlr, xwe, newUtil, *\/ */
/*     /\*                                 gameID, nop, ripp ); *\/ */
/*     GR_HEADER_END(); */
/*     return result; */
/* } */

XP_Bool
gr_haveData( DUTIL_GR_XWE )
{
    XP_Bool deleted;
    GameData* gd = gmgr_getForRef( duc, xwe, gr, &deleted );
    XP_ASSERT( !deleted );
    return !deleted && NULL != gd->model;
}

XP_Bool
gr_haveComms( DUTIL_GR_XWE )
{
    XP_Bool result = XP_FALSE;
    GR_HEADER();
    result = NULL != gd->comms;
    GR_HEADER_END();
    return result;
}

void
gr_setRematchOrder( DUTIL_GR_XWE, RematchInfo* rip )
{
    GR_HEADER();
    ctrl_setRematchOrder( gd->ctrlr, rip );
    GR_HEADER_END();
}

void
gr_invite( DUTIL_GR_XWE, const NetLaunchInfo* nli,
           const CommsAddrRec* destAddr, XP_Bool sendNow )
{
    GR_HEADER_WITH(COMMS);
    comms_invite( gd->comms,xwe, nli, destAddr, sendNow );
    GR_HEADER_END();
}

XP_U32
gr_getGameID( GameRef gr )
{
    return (XP_U32)(gr & 0xFFFFFFFF);
}

const CurGameInfo*
gr_getGI( DUTIL_GR_XWE )
{
    const CurGameInfo* result = NULL;
    GR_HEADER_WITH(GI);
    result = &gd->gi;
    GR_HEADER_END();
    return result;
}

static void
setGIImpl( XW_DUtilCtxt* duc, XWEnv xwe, GameData* gd,
           const CurGameInfo* gip )
{
    XP_ASSERT( gi_isValid(gip) );
    if ( !gi_equal( gip, &gd->gi ) ) {
        /* If the gi's different, then it might be impacting sort order within
           groups. So we remove the game from its group, make the change, then
           re-insert it. */
        GroupRef grp = gd->grp;
        gmgr_rmFromGroup( duc, xwe, gd->gr, grp );

        gd->gi = *gip;
        gmgr_saveGI( duc, xwe, gd->gr );

        gmgr_addToGroup( duc, xwe, gd->gr, grp );
        postGameChangeEvent( duc, xwe, gd, GCE_CONFIG_CHANGED );
    }
}

void
gr_setGI( DUTIL_GR_XWE, const CurGameInfo* gip )
{
    GR_HEADER_WITH(GI);
    XP_ASSERT( gr == gi_formatGR(gip) );
    XP_ASSERT( !!gd );
    if ( !!gd ) {
        setGIImpl( duc, xwe, gd, gip );
    }
    GR_HEADER_END();
}

void
gr_setGameName( DUTIL_GR_XWE, const XP_UCHAR* newName )
{
    GR_HEADER_WITH(GI);
    XP_ASSERT( !!gd );
    const CurGameInfo* gip = &gd->gi;
    CurGameInfo gi = *gip;
    str2ChrArray( gi.gameName, newName );
    setGIImpl( duc, xwe, gd, &gi );
    GR_HEADER_END();
}

XP_Bool
gr_checkIncomingStream( DUTIL_GR_XWE, XWStreamCtxt* stream,
                        const CommsAddrRec* addr, CommsMsgState* state )
{
    XP_ASSERT(0);                  /* am I using this */
    XP_Bool result = XP_FALSE;
    GR_HEADER();
    result = comms_checkIncomingStream( gd->comms, xwe, stream, addr, state, NULL );
    GR_HEADER_END();
    return result;
}

static void
checkMessageCount(XW_DUtilCtxt* duc, XWEnv xwe, GameData* gd )
{
    XP_Bool quashed;
    XP_U16 count = comms_countPendingPackets( gd->comms, &quashed );
    if ( !quashed && gd->sum.nPacketsPending != count ) {
        gd->sum.nPacketsPending = count;
        saveSummary( duc, xwe, gd );
        postGameChangeEvent( duc, xwe, gd,
                             GCE_MSGCOUNT_CHANGED|GCE_SUMMARY_CHANGED );
    }
}

void
gr_onMessageReceived( DUTIL_GR_XWE, const CommsAddrRec* from,
                      const XP_U8* msgBuf, XP_U16 msgLen,
                      const MsgCountState* mcs )
{
    GR_HEADER_WITH(COMMS);
    XWStreamCtxt* stream = dvc_makeStream( duc );
    strm_putBytes( stream, msgBuf, msgLen );

    CommsMsgState commsState;
    XP_Bool result = comms_checkIncomingStream( gd->comms, xwe, stream, from,
                                                &commsState, mcs );
    if ( result ) {
        XP_Bool haveCtrlr = !!gd->ctrlr;
        if ( !haveCtrlr ) {
            loadToLevel( duc, gr, xwe, MODEL, &haveCtrlr, NULL );
        }
        if ( haveCtrlr ) {
            XP_Bool needsChatNotify = XP_FALSE;;
            result = ctrl_receiveMessage( gd->ctrlr, xwe, stream,
                                          &needsChatNotify );
            if ( needsChatNotify ) {
                postGameChangeEvent( duc, xwe, gd, GCE_CHAT_ARRIVED );
                if ( !gd->sum.hasChat ) {
                    GameSummary newSum = gd->sum;
                    newSum.hasChat = XP_TRUE;
                    updateSummary( duc, xwe, gd, &newSum );
                }
            }
        }
    }
    comms_msgProcessed( gd->comms, &commsState, !result );
    if ( result ) {
        ctrl_addIdle( gd->ctrlr, xwe );
    }

    strm_destroy( stream );

    checkMessageCount( duc, xwe, gd );

    if ( result ) {
        if ( !!gd->board ) {
            schedule_draw( duc, xwe, gd );
        }
        thumbChanged( duc, xwe, gd );
    }

    GR_HEADER_END();
}

void
gr_setQuashed( DUTIL_GR_XWE, XP_Bool set )
{
    GR_HEADER_WITH(COMMS);
    comms_setQuashed( gd->comms, set );
    GR_HEADER_END();
}

void
gr_setDraw( DUTIL_GR_XWE, DrawCtx* dctx, XW_UtilCtxt* util )
{
    GR_HEADER();
    util_unref( gd->util, xwe );
    gd->util = util_ref( util );
    if ( !gd->util ) {
        gd->util = makeDummyUtil( duc, gd );
    }
    board_setDraw( gd->board, xwe, dctx );
    GR_HEADER_END();
}

XP_Bool
gr_isArchived( DUTIL_GR_XWE )
{
    XP_Bool result = XP_FALSE;
    GR_HEADER_WITH(GD);
    GroupRef dflt = gmgr_getArchiveGroup( duc );
    result = gd->grp == dflt;
    GR_HEADER_END();
    return result;
}

static void
loadThumbData( XW_DUtilCtxt* duc, XWEnv xwe, GameData* gd )
{
    DrawCtx* thumbDraw = dutil_getThumbDraw( duc, xwe, gd->gr );
    if ( !!thumbDraw ) {
        board_drawThumb( gd->board, xwe, thumbDraw );

        XP_ASSERT( !gd->thumbData );
        gd->thumbData = dvc_makeStream( duc );
        draw_getThumbData( thumbDraw, xwe, gd->thumbData );
        if ( 0 == strm_getSize(gd->thumbData) ) {
            XP_LOGFF( "got nothing from draw_getThumbData" );
            strm_destroy( gd->thumbData );
            gd->thumbData = NULL;
        }
        draw_unref( thumbDraw, xwe );
    }
}

XP_Bool
gr_getThumbData( DUTIL_GR_XWE, XWStreamCtxt* stream )
{
    XP_Bool result = XP_FALSE;
    GR_HEADER();
    // XP_LOGFF( "looking at gd->thumbData; gd: %p", gd );
    result = !!gd;
    if ( result ) {
        if ( !gd->thumbData ) {
            // XP_LOGFF( "no data; calling loadThumbData()");
            loadThumbData( duc, xwe, gd );
        } else {
            // XP_LOGFF( "have data, so NOT calling loadThumbData()");
        }
        result = !!gd->thumbData;
        if ( result ) {
            strm_setPos( gd->thumbData, POS_READ, START_OF_STREAM );
            strm_getFromStream( stream, gd->thumbData, strm_getSize(gd->thumbData) );
        }
    }
    GR_HEADER_END();
    // LOG_RETURNF( "%s", boolToStr(result) );
    return result;
}

XP_S16
gr_getNMoves( DUTIL_GR_XWE )
{
    XP_S16 result = -1;
    GR_HEADER();
    result = model_getNMoves( gd->model );
    GR_HEADER_END();
    return result;
}

XP_U16
gr_getNumTilesInTray( DUTIL_GR_XWE, XP_S16 turn )
{
    XP_S16 result = 0;
    GR_HEADER();
    result = model_getNumTilesInTray( gd->model, turn );
    GR_HEADER_END();
    return result;
}

XP_Bool
gr_getGameIsOver( DUTIL_GR_XWE )
{
    XP_Bool result = XP_FALSE;
    GR_HEADER();
    result = ctrl_getGameIsOver( gd->ctrlr );
    GR_HEADER_END();
    return result;
}

XP_S16
gr_getCurrentTurn( DUTIL_GR_XWE, XP_Bool* isLocal )
{
    XP_S16 result = -1;
    GR_HEADER();
    result = ctrl_getCurrentTurn( gd->ctrlr, isLocal );
    GR_HEADER_END();
    return result;
}

XP_U32
gr_getLastMoveTime( DUTIL_GR_XWE )
{
    XP_U32 result = 0;
    GR_HEADER();
    result = ctrl_getLastMoveTime( gd->ctrlr );
    GR_HEADER_END();
    return result;
}

XP_U32
gr_getDupTimerExpires( DUTIL_GR_XWE )
{
    XP_U32 result = 0;
    GR_HEADER();
    result = ctrl_getDupTimerExpires( gd->ctrlr );
    GR_HEADER_END();
    return result;
}

XP_U16
gr_getMissingPlayers( DUTIL_GR_XWE )
{
    XP_U16 result = 0;
    GR_HEADER();
    result = ctrl_getMissingPlayers( gd->ctrlr );
    GR_HEADER_END();
    return result;
}

XP_S16
gr_countTilesInPool( DUTIL_GR_XWE )
{
    XP_S16 result = -1;
    GR_HEADER();
    XP_ASSERT( !!gd->ctrlr );
    result = ctrl_countTilesInPool( gd->ctrlr );
    GR_HEADER_END();
    return result;
}

XP_Bool
gr_handleUndo( DUTIL_GR_XWE, XP_U16 limit )
{
    XP_Bool result = XP_FALSE;
    GR_HEADER();
    result = ctrl_handleUndo( gd->ctrlr, xwe, limit );
    GR_HEADER_END();
    return result;
}

void
gr_endGame( DUTIL_GR_XWE )
{
    GR_HEADER();
    ctrl_endGame( gd->ctrlr, xwe );
    schedule_draw( duc, xwe, gd );
    GR_HEADER_END();
}

void
gr_writeFinalScores( DUTIL_GR_XWE, XWStreamCtxt* stream )
{
    GR_HEADER();
    ctrl_writeFinalScores( gd->ctrlr, xwe, stream );
    GR_HEADER_END();
}

void
gr_figureFinalScores( DUTIL_GR_XWE,
                      ScoresArray* scores, ScoresArray* tilePenalties )
{
    GR_HEADER();
    model_figureFinalScores( gd->model, scores, tilePenalties );
    GR_HEADER_END();
}

XP_S16
gr_getPlayerScore( DUTIL_GR_XWE, XP_S16 player )
{
    XP_S16 result = -1;
    GR_HEADER();
    result = model_getPlayerScore( gd->model, player );
    GR_HEADER_END();
    return result;
}

XP_U16
gr_getChannelSeed( DUTIL_GR_XWE )
{
    XP_U16 result = 0;
    GR_HEADER_WITH(COMMS);
    result = comms_getChannelSeed( gd->comms );
    GR_HEADER_END();
    return result;
}

XP_U16
gr_countPendingPackets( DUTIL_GR_XWE, XP_Bool* quashed )
{
    XP_U16 result = 0;
    GR_HEADER_WITH(COMMS);
    result = !!gd->comms ? comms_countPendingPackets( gd->comms, quashed ) : 0;
    if ( result != gd->sum.nPacketsPending ) {
        XP_LOGFF( "result: %d and nPacketsPending: %d out of sync",
                  result, gd->sum.nPacketsPending );
    }
    GR_HEADER_END();
    return result;
}

typedef struct _GotPacketState {
    XW_DUtilCtxt* duc;
    XWStreamCtxt* stream;
    int count;
} GotPacketState;

static void
gotPacketProc( const XP_U8* buf, XP_U16 len, void* closure )
{
    GotPacketState* gps = (GotPacketState*)closure;
    if ( !gps->stream ) {
        gps->stream = dvc_makeStream( gps->duc );
    }
    strm_putU32VL( gps->stream, len );
    strm_putBytes( gps->stream, buf, len );
    ++gps->count;
}

XWStreamCtxt*
gr_getPendingPacketsFor( DUTIL_GR_XWE, const CommsAddrRec* addr,
                         const XP_UCHAR* host, const XP_UCHAR* prefix)
{
    XWStreamCtxt* result = NULL;
    GR_HEADER_WITH(COMMS);
    if ( !!gd->comms ) {
        GotPacketState gps = { .duc = duc, };
        comms_getPendingPacketsFor( gd->comms, xwe, addr, gotPacketProc, &gps );

        if ( !!gps.stream ) {
            XWStreamCtxt* msgs = dvc_makeStream( duc );
            strm_putU32VL( msgs, gps.count );
            strm_getFromStream( msgs, gps.stream, strm_getSize(gps.stream) );
            strm_destroy( gps.stream );

            result = dvc_beginUrl( duc, host, prefix );
            XP_U32 gid = gr_getGameID( gr );
            UrlParamState state = {};
            urlParamToStream( result, &state, "gid", UPT_U32, gid );

            XWStreamCtxt* b64Stream = dvc_makeStream( duc );
            binToB64Streams( b64Stream, msgs );
            urlParamToStream( result, &state, "msgs", UPT_STREAM, b64Stream );
            strm_destroy( b64Stream );
            strm_destroy( msgs );
        }
    }
    GR_HEADER_END();
    return result;
}

void
gr_parsePendingPackets( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr,
                        XWStreamCtxt* stream )
{
    XWStreamCtxt* msgs = dvc_makeStream( duc );
    b64ToBinStreams( msgs, stream );
    XP_U32 count = strm_getU32VL( msgs );
    for ( int ii = 0; ii < count; ++ii ) {
        XP_U32 len = strm_getU32VL( msgs );
        XP_U8 msg[len];
        strm_getBytes( msgs, msg, len );
        gr_onMessageReceived( duc, gr, xwe, NULL, msg, len, NULL );
    }
    strm_destroy( msgs );
}

XWStreamCtxt*
gr_inviteUrl( DUTIL_GR_XWE, const XP_UCHAR* host, const XP_UCHAR* prefix )
{
    XWStreamCtxt* result = NULL;
    GR_HEADER_WITH(COMMS);

    XP_U16 channel;
    if ( !!gd->comms && ctrl_getOpenChannel( gd->ctrlr, &channel )) {
        result = dvc_beginUrl( duc, host, prefix );
        NetLaunchInfo nli = makeSelfNLI( gd, gd->gi.gameName, 1, channel );
        nli_makeInviteData( &nli, result );
    }
    GR_HEADER_END();
    return result;
}

XW_UtilCtxt*
gr_getUtil( DUTIL_GR_XWE )
{
    XW_UtilCtxt* result = NULL;
    GR_HEADER();
    result = util_ref( gd->util );
    GR_HEADER_END();
    return result;
}

XP_U32
gr_getCreated( DUTIL_GR_XWE )
{
    XP_U32 result = 0;
    GR_HEADER();
    result = util_getGI(gd->util)->created;
    GR_HEADER_END();
    return result;
}

XP_Bool
gr_getGameIsConnected( DUTIL_GR_XWE )
{
    XP_Bool result = XP_FALSE;
    GR_HEADER();
    result = ctrl_getGameIsConnected( gd->ctrlr );
    GR_HEADER_END();
    return result;
}

XP_Bool
gr_getIsHost( DUTIL_GR_XWE )
{
    XP_Bool result = XP_FALSE;
    GR_HEADER_WITH(COMMS);
    result = comms_getIsHost( gd->comms );
    GR_HEADER_END();
    return result;
}

static void
invalAll(XW_DUtilCtxt* duc, XWEnv xwe, GameData* gd)
{
    board_invalAll( gd->board );
    schedule_draw( duc, xwe, gd );
}

void
gr_invalAll( DUTIL_GR_XWE )
{
    GR_HEADER();
    invalAll( duc, xwe, gd );
    GR_HEADER_END();
}

static void
addOnce( const XP_UCHAR* missingNames[], XP_U16* count, const XP_UCHAR* dictName )
{
    if ( dictName[0] ) {        /* sometimes dict name's an empty string
                                   instead of not there */
        XP_Bool found = XP_FALSE;
        for ( int ii = 0; ii < *count && !found; ++ii ) {
            found = 0 == XP_STRCMP( missingNames[ii], dictName );
        }
        if ( !found ) {
            XP_LOGFF( "adding '%s' at %d", dictName, *count );
            missingNames[*count] = dictName;
            ++*count;
        }
    }
}

static void
missingDictsImpl( const GameData* gd, const XP_UCHAR* missingNames[], XP_U16* countP )
{
    const CurGameInfo* gi = &gd->gi;
    XP_U16 count = 0;
    if ( !!gi->dictName[0] && !gd->dict ) {
        addOnce( missingNames, &count, gi->dictName );
    }

    const PlayerDicts* playerDicts = &gd->playerDicts;
    for ( int ii = 0; ii < gi->nPlayers && count < *countP; ++ii ) {
        const LocalPlayer* lp = &gi->players[ii];
        if ( !!lp->dictName[0] && !playerDicts->dicts[ii] ) {
            addOnce( missingNames, &count, lp->dictName );
        }
    }

    *countP = count;
}

void
gr_missingDicts( DUTIL_GR_XWE, const XP_UCHAR* missingNames[], XP_U16* countP )
{
    GR_HEADER_WITH( DICTS );
    missingDictsImpl( gd, missingNames, countP );
    GR_HEADER_END();
}

static XP_Bool
tryReplOne( GameData* XP_UNUSED(gd), XP_UCHAR* loc, XP_U16 len,
            const XP_UCHAR* oldName, const XP_UCHAR* newName )
{
    XP_Bool changed = 0 == XP_STRCMP( loc, oldName );
    if ( changed ) {
        XP_SNPRINTF( loc, len, "%s", newName );
    }
    return changed;
}

void
gr_replaceDicts( DUTIL_GR_XWE, const XP_UCHAR* oldName, const XP_UCHAR* newName )
{
    GR_HEADER_WITH(GI);

    /* For now, assume model ain't loaded. If it is, it's probably easiest to
       save and unload it then let later gr_ calls load it again with the new
       dicts. */
    XP_ASSERT( !gd->model );

    unrefDicts( xwe, gd );
    gd->dictsSought = XP_FALSE; /* so we'll load again */
    
    CurGameInfo* gi = &gd->gi;
    XP_Bool changed = tryReplOne( gd, gi->dictName, VSIZE(gi->dictName),
                                  oldName, newName );
    for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
        LocalPlayer* lp = &gi->players[ii];
        changed = tryReplOne( gd, lp->dictName, VSIZE(lp->dictName),
                              oldName, newName ) || changed;
    }

    if ( changed ) {
        postGameChangeEvent( duc, xwe, gd, GCE_CONFIG_CHANGED );
    }

    GR_HEADER_END();
}

/* XP_Bool */
/* gr_haveDicts( DUTIL_GR_XWE ) */
/* { */
/*     XP_Bool result = XP_FALSE; */
/*     GR_HEADER(); */
/*     result = XP_TRUE; */
/*     GR_HEADER_END(); */
/*     return result; */
/* } */

const DictionaryCtxt*
gr_getDictionary( DUTIL_GR_XWE )
{
    const DictionaryCtxt* result = NULL;
    GR_HEADER();
    result = model_getDictionary( gd->model );
    GR_HEADER_END();
    return result;
}

void
gr_draw( DUTIL_GR_XWE )
{
    GR_HEADER();
    board_draw( gd->board, xwe );
    GR_HEADER_END();
}

void
gr_figureLayout( DUTIL_GR_XWE, XP_U16 bLeft, XP_U16 bTop,
                 XP_U16 bWidth, XP_U16 bHeight, XP_U16 colPctMax,
                 XP_U16 scorePct, XP_U16 trayPct, XP_U16 scoreWidth,
                 XP_U16 fontWidth, XP_U16 fontHt, XP_Bool squareTiles,
                 BoardDims* dimsp )
{
    GR_HEADER();
    const CurGameInfo* gi = &gd->gi;
    board_figureLayout( gd->board, xwe, gi, bLeft, bTop,
                        bWidth, bHeight, colPctMax, scorePct, trayPct,
                        scoreWidth, fontWidth, fontHt, squareTiles, dimsp );
    GR_HEADER_END();
}

void
gr_applyLayout( DUTIL_GR_XWE, const BoardDims* dimsp )
{
    GR_HEADER();
    board_applyLayout( gd->board, xwe, dimsp );
    GR_HEADER_END();
}

void
gr_sendChat( DUTIL_GR_XWE, const XP_UCHAR* msg )
{
    GR_HEADER();
    board_sendChat( gd->board, xwe, msg );
    GR_HEADER_END_SAVE();
}

XP_U16
gr_getChatCount( DUTIL_GR_XWE )
{
    XP_U16 result = 0;
    GR_HEADER();
    result = model_countChats( gd->model );
    GR_HEADER_END();
    return result;
}

void
gr_getNthChat( DUTIL_GR_XWE, XP_U16 nn,
               XP_UCHAR* buf, XP_U16* bufLen, XP_S16* from,
               XP_U32* timestamp, XP_Bool markShown )
{
    GR_HEADER();
    model_getChat( gd->model, nn, buf, bufLen, from, timestamp );
    if ( markShown && gd->sum.hasChat ) {
        gd->sum.hasChat = XP_FALSE;
        saveSummary( duc, xwe, gd );
        postGameChangeEvent( duc, xwe, gd, GCE_SUMMARY_CHANGED );
    }
    GR_HEADER_END();
}

void
gr_deleteChats( DUTIL_GR_XWE )
{
    GR_HEADER();
    model_deleteChats( gd->model );
    GR_HEADER_END_SAVE();
}

void
gr_getPlayerName( DUTIL_GR_XWE, XP_U16 nn,
                  XP_UCHAR* buf, XP_U16* bufLen )
{
    GR_HEADER();
    const CurGameInfo* gi = &gd->gi;
    XP_ASSERT( nn < gi->nPlayers );
    *bufLen = XP_SNPRINTF( buf, *bufLen - 1, "%s", gi->players[nn].name );
    GR_HEADER_END();
}

void
gr_commitTurn( DUTIL_GR_XWE, const PhoniesConf* pc,
               XP_Bool turnConfirmed, TrayTileSet* newTiles )
{
    GR_HEADER();
    if ( board_commitTurn( gd->board, xwe, pc, turnConfirmed, newTiles ) ) {
        schedule_draw( duc, xwe, gd );
        thumbChanged( duc, xwe, gd );
    }
    GR_HEADER_END_SAVE();
}

void
gr_figureOrder( DUTIL_GR_XWE,
                RematchOrder ro, NewOrder* nop )
{
    GR_HEADER();
    ctrl_figureOrder( gd->ctrlr, ro, nop );
    GR_HEADER_END();
}

void
gr_formatRemainingTiles( DUTIL_GR_XWE, XWStreamCtxt* stream )
{
    GR_HEADER();
    board_formatRemainingTiles( gd->board, xwe, stream );
    GR_HEADER_END();
}

void
gr_setBlankValue( DUTIL_GR_XWE, XP_U16 player,
                  XP_U16 col, XP_U16 row, XP_U16 tileIndex )
{
    GR_HEADER();
    if ( board_setBlankValue( gd->board, player, col, row, tileIndex ) ) {
        schedule_draw( duc, xwe, gd );
    }
    GR_HEADER_END();
}

XP_Bool
gr_getPlayersLastScore( DUTIL_GR_XWE, XP_S16 player,
                        LastMoveInfo* info )
{
    XP_Bool result = XP_FALSE;
    GR_HEADER();
    result = model_getPlayersLastScore( gd->model, xwe, player, info );
    GR_HEADER_END();
    return result;
}

void
gr_flip( DUTIL_GR_XWE )
{
    GR_HEADER();
    if ( board_flip( gd->board ) ) {
        schedule_draw( duc, xwe, gd );
    }
    GR_HEADER_END();
}

#ifdef KEY_SUPPORT
void
gr_handleKey( DUTIL_GR_XWE, XP_Key key, XP_Bool* handled )
{
    GR_HEADER();
    if ( board_handleKey( gd->board, xwe, key, handled ) ) {
        schedule_draw( duc, xwe, gd );
    }
    GR_HEADER_END();
}

void
gr_handleKeyUp( DUTIL_GR_XWE, XP_Key key, XP_Bool* handled )
{
    GR_HEADER();
    if ( board_handleKeyUp( gd->board, xwe, key, handled ) ) {
        schedule_draw( duc, xwe, gd );
    }
    GR_HEADER_END();
}

void
gr_handleKeyDown( DUTIL_GR_XWE, XP_Key key, XP_Bool* handled )
{
    GR_HEADER();
    if ( board_handleKeyDown( gd->board, xwe, key, handled ) ) {
        schedule_draw( duc, xwe, gd );
    }
    GR_HEADER_END();
}

# ifdef KEYBOARD_NAV
void
gr_handleKeyRepeat( DUTIL_GR_XWE, XP_Key key, XP_Bool* handled )
{
    GR_HEADER();
    if ( board_handleKeyRepeat( gd->board, xwe, key, handled ) ) {
        schedule_draw( duc, xwe, gd );
    }
    GR_HEADER_END();
}

XP_Bool
gr_focusChanged( DUTIL_GR_XWE, BoardObjectType typ, XP_Bool gained )
{
    XP_Bool result = XP_FALSE;
    GR_HEADER();
    result = board_focusChanged( gd->board, xwe, typ, gained );
    if ( result ) {
        schedule_draw( duc, xwe, gd );
    }
    GR_HEADER_END();
    return result;
}
# endif
#endif

void
gr_replaceTiles( DUTIL_GR_XWE )
{
    GR_HEADER();
    if ( board_replaceTiles( gd->board, xwe )
         || board_redoReplacedTiles(gd->board, xwe ) ) {
        schedule_draw( duc, xwe, gd );
    }
    GR_HEADER_END_SAVE();
}

void
gr_juggleTray( DUTIL_GR_XWE )
{
    GR_HEADER();
    if ( board_juggleTray( gd->board, xwe ) ) {
        schedule_draw( duc, xwe, gd );
    }
    GR_HEADER_END_SAVE();
}

void
gr_beginTrade( DUTIL_GR_XWE )
{
    GR_HEADER();
    if ( board_beginTrade( gd->board, xwe ) ) {
        schedule_draw( duc, xwe, gd );
    }
    GR_HEADER_END_SAVE();
}

void
gr_endTrade( DUTIL_GR_XWE )
{
    GR_HEADER();
    if ( board_endTrade( gd->board ) ) {
        schedule_draw( duc, xwe, gd );
    }
    GR_HEADER_END_SAVE();
}

XP_Bool
gr_hideTray( DUTIL_GR_XWE )
{
    XP_Bool result = XP_FALSE;
    GR_HEADER();
    result = board_hideTray( gd->board, xwe );
    GR_HEADER_END_SAVE();
    return result;
}

XP_Bool
gr_showTray( DUTIL_GR_XWE )
{
    XP_Bool result = XP_FALSE;
    GR_HEADER();
    result = board_showTray( gd->board, xwe );
    GR_HEADER_END_SAVE();
    return result;
}

void
gr_toggleTray( DUTIL_GR_XWE )
{
    GR_HEADER();
    XP_Bool draw = XP_FALSE;
    XW_TrayVisState state = board_getTrayVisState( gd->board );
    if ( TRAY_REVEALED == state ) {
        draw = board_hideTray( gd->board, xwe );
    } else {
        draw = board_showTray( gd->board, xwe );
    }
    if ( draw ) {
        schedule_draw( duc, xwe, gd );
    }
    GR_HEADER_END_SAVE();
}

XP_Bool
gr_requestHint( DUTIL_GR_XWE,
#ifdef XWFEATURE_SEARCHLIMIT
                XP_Bool useTileLimits,
#endif
                XP_Bool usePrev, XP_Bool* workRemainsP )
{
    XP_Bool result = XP_FALSE;
    GR_HEADER();
    result = board_requestHint( gd->board, xwe,
#ifdef XWFEATURE_SEARCHLIMIT
                                useTileLimits,
#endif
                                usePrev, workRemainsP );
    if ( result ) {
        schedule_draw( duc, xwe, gd );
    }
    GR_HEADER_END();
    return result;
}

XW_TrayVisState
gr_getTrayVisState( DUTIL_GR_XWE )
{
    XW_TrayVisState result = 0;
    GR_HEADER();
    result = board_getTrayVisState( gd->board );
    GR_HEADER_END();
    return result;
}

#ifdef KEYBOARD_NAV
BoardObjectType
gr_getFocusOwner( DUTIL_GR_XWE )
{
    BoardObjectType result = 0;
    GR_HEADER();
    result = board_getFocusOwner( gd->board );
    GR_HEADER_END();
    return result;
}
#endif

void
gr_formatDictCounts( DUTIL_GR_XWE, XWStreamCtxt* stream,
                     XP_U16 nCols, XP_Bool allFaces )
{
    GR_HEADER();
    ctrl_formatDictCounts( gd->ctrlr, xwe, stream, nCols, allFaces );
    GR_HEADER_END();
}

XP_Bool
gr_canRematch( DUTIL_GR_XWE, XP_Bool* canOrder )
{
    XP_Bool result = XP_FALSE;
    GR_HEADER();
    result = ctrl_canRematch( gd->ctrlr, canOrder );
    GR_HEADER_END();
    return result;
}

#ifdef POINTER_SUPPORT
void
gr_handlePenDown( DUTIL_GR_XWE, XP_U16 xx,
                  XP_U16 yy, XP_Bool* handled )
{
    GR_HEADER();
    if ( board_handlePenDown( gd->board, xwe, xx, yy, handled ) ) {
        schedule_draw( duc, xwe, gd );
        thumbChanged( duc, xwe, gd );
    }
    GR_HEADER_END_SAVE();
}

void
gr_handlePenMove( DUTIL_GR_XWE, XP_U16 x, XP_U16 y )
{
    GR_HEADER();
    if ( board_handlePenMove( gd->board, xwe, x, y ) ) {
        schedule_draw( duc, xwe, gd );
    }
    GR_HEADER_END_SAVE();
}

void
gr_handlePenUp( DUTIL_GR_XWE, XP_U16 x, XP_U16 y )
{
    GR_HEADER();
    if ( board_handlePenUp( gd->board, xwe, x, y ) ) {
        schedule_draw( duc, xwe, gd );
        thumbChanged( duc, xwe, gd );
    }
    GR_HEADER_END_SAVE();
}

XP_Bool
gr_containsPt( DUTIL_GR_XWE, XP_U16 xx, XP_U16 yy )
{
    XP_Bool result = XP_FALSE;
    GR_HEADER();
    result = board_containsPt( gd->board, xx, yy );
    GR_HEADER_END();
    return result;
}
#endif

#ifdef DEBUG
void
gr_setAddrDisabled( DUTIL_GR_XWE, CommsConnType typ,
                    XP_Bool send, XP_Bool disabled )
{
    GR_HEADER_WITH(COMMS);
    comms_setAddrDisabled( gd->comms, typ, send, disabled );
    GR_HEADER_END();
}
#endif
 
void
gr_zoom( DUTIL_GR_XWE, XP_S16 zoomBy, XP_Bool* canInOut )
{
    GR_HEADER();
    if ( board_zoom( gd->board, xwe, zoomBy, canInOut ) ) {
        schedule_draw( duc, xwe, gd );
    }
    GR_HEADER_END();
}

XP_U16
gr_getLikelyChatter( DUTIL_GR_XWE )
{
    XP_U16 result = 0;
    GR_HEADER();
    result = board_getLikelyChatter( gd->board );
    GR_HEADER_END();
    return result;
}

void
gr_start( DUTIL_GR_XWE )
{
    GR_HEADER_WITH(COMMS);
    if ( !!gd->comms ) {
        comms_start( gd->comms, xwe );
    }
    ctrl_addIdle( gd->ctrlr, xwe );
    GR_HEADER_END();
}

void
gr_stop( DUTIL_GR_XWE )
{
    GR_HEADER_WITH(COMMS);
    comms_stop( gd->comms );
    GR_HEADER_END();
}

#ifdef TEXT_MODEL
void
gr_writeToTextStream( DUTIL_GR_XWE, XWStreamCtxt* stream )
{
    GR_HEADER();
    model_writeToTextStream( gd->model, stream );
    GR_HEADER_END();
}
#endif

#ifdef XWFEATURE_CHANGEDICT
static void
setDict( CurGameInfo* gi, const DictionaryCtxt* dict )
{
    XP_U16 ii;
    const XP_UCHAR* name = dict_getName( dict );
    str2ChrArray( gi->dictName, name );
    for ( ii = 0; ii < gi->nPlayers; ++ii ) {
        LocalPlayer* pl = &gi->players[ii];
        pl->dictName[0] = '\0';
    }    
}

void
gr_changeDict( DUTIL_GR_XWE,
               DictionaryCtxt* dict )
{
    GR_HEADER();
    model_setDictionary( gd->model, xwe, dict );
    setDict( &gd->gi, dict );
    ctrl_resetEngines( gd->ctrlr );
    GR_HEADER_END();
}
#endif

void
gr_ackAny( DUTIL_GR_XWE )
{
    GR_HEADER_WITH(COMMS);
    if ( !!gd->comms ) {
        comms_ackAny( gd->comms, xwe );
    }
    GR_HEADER_END();
}

void
gr_getAddrs( DUTIL_GR_XWE,
             CommsAddrRec addr[], XP_U16* nRecs )
{
    GR_HEADER_WITH(COMMS);
    if ( !!gd->comms ) {
        comms_getAddrs( gd->comms, addr, nRecs );
    } else {
        *nRecs = 0;
    }
    GR_HEADER_END();
}

#ifdef DEBUG
XWStreamCtxt*
gr_getStats( DUTIL_GR_XWE )
{
    XWStreamCtxt* result = NULL;
    GR_HEADER_WITH(COMMS);
    result = comms_getStats( gd->comms );
    GR_HEADER_END();
    return result;
}
#endif

#ifdef MEM_DEBUG
MemPoolCtx*
gr_getMemPool( DUTIL_GR_XWE )
{
    XP_ASSERT( !!gr );
    GameData* gd = gmgr_getForRef(duc, xwe, gr, NULL);
    MemPoolCtx* result = gd->mpool;
    XP_ASSERT( !!result );
    return result;
}
#endif


XP_U16
gr_getPendingRegs( DUTIL_GR_XWE )
{
    XP_U16 result = 0;
    GR_HEADER();
    result = ctrl_getPendingRegs( gd->ctrlr );
    GR_HEADER_END();
    return result;
}

XP_Bool
gr_isFromRematch( DUTIL_GR_XWE )
{
    XP_Bool result = XP_FALSE;
    GR_HEADER();
    result = ctrl_isFromRematch( gd->ctrlr );
    GR_HEADER_END();
    return result;
}

void
gr_getState( DUTIL_GR_XWE, GameStateInfo* gsi )
{
    GR_HEADER();
    const CtrlrCtxt* ctrlr = gd->ctrlr;
    BoardCtxt* board = gd->board;

    XP_Bool gameOver = ctrl_getGameIsOver( ctrlr );
    gsi->curTurnSelected = board_curTurnSelected( board );
    gsi->trayVisState = board_getTrayVisState( board );
    gsi->visTileCount = board_visTileCount( board );
    gsi->canHint = !gameOver && board_canHint( board );
    gsi->canUndo = model_canUndo( gd->model );
    gsi->canRedo = board_canTogglePending( board );
    gsi->inTrade = board_inTrade( board, &gsi->tradeTilesSelected );
    gsi->canChat = !!gd->comms && comms_canChat( gd->comms );
    gsi->canShuffle = board_canShuffle( board );
    gsi->canHideRack = board_canHideRack( board );
    gsi->canTrade = board_canTrade( board, xwe );
    gsi->nPendingMessages = !!gd->comms ? 
        comms_countPendingPackets(gd->comms, NULL) : 0;
    gsi->canPause = ctrl_canPause( ctrlr );
    gsi->canUnpause = ctrl_canUnpause( ctrlr );
    GR_HEADER_END();
}

const GameSummary*
gr_getSummary( DUTIL_GR_XWE )
{
    const GameSummary* result = NULL;
    GR_HEADER_WITH(SUM);
    result = &gd->sum;
    GR_HEADER_END();
    return result;
}

void
gr_setCollapsed( DUTIL_GR_XWE, XP_Bool collapsed )
{
    // XP_LOGFF( "collapsed: %s", boolToStr(collapsed) );
    GR_HEADER_WITH(SUM);
    if ( gd->sum.collapsed != collapsed ) {
        gd->sum.collapsed = collapsed;
        saveSummary( duc, xwe, gd );
        postGameChangeEvent( duc, xwe, gd, GCE_SUMMARY_CHANGED );
    }
    GR_HEADER_END();
}

static void
saveSummary( XW_DUtilCtxt* duc, XWEnv xwe, GameData* gd )
{
    XWStreamCtxt* stream = dvc_makeStream( duc );
    sumToStream( stream, &gd->sum, gd->gi.nPlayers );
    gmgr_storeSum( duc, xwe, gd->gr, stream );
    strm_destroy( stream );
}

static void
updateSummary( XW_DUtilCtxt* duc, XWEnv xwe,
               GameData* gd, const GameSummary* newSum )
{
    if ( 0 != XP_MEMCMP( &gd->sum, newSum, sizeof(*newSum) ) ) {
        GroupRef grp = gd->grp;
        gmgr_rmFromGroup( duc, xwe, gd->gr, grp );
        gd->sum = *newSum;
        gmgr_addToGroup( duc, xwe, gd->gr, grp );
    }
}

static void
summarize( XW_DUtilCtxt* duc, XWEnv xwe, GameData* gd )
{
    const CurGameInfo* gi = &gd->gi;
    GameSummary sum = {};
    CtrlrCtxt* ctrlr = gd->ctrlr;
    sum.turn = ctrl_getCurrentTurn( ctrlr, &sum.turnIsLocal );
    XP_LOGFF( "turn now %d", sum.turn );
    sum.lastMoveTime = ctrl_getLastMoveTime(ctrlr);
    sum.gameOver = ctrl_getGameIsOver( ctrlr );
    sum.nMoves = model_getNMoves( gd->model );
    sum.dupTimerExpires = ctrl_getDupTimerExpires( ctrlr );
    sum.canRematch = ctrl_canRematch( ctrlr, &sum.canOfferRO );

    /* Copied from our storage, for now */
    sum.collapsed = gd->sum.collapsed;
    sum.hasChat = gd->sum.hasChat;

    model_getCurScores( gd->model, &sum.scores, XP_TRUE );

    for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
        const LocalPlayer* lp  = &gi->players[ii];
        if ( LP_IS_ROBOT(lp) || !LP_IS_LOCAL(lp) ) {
            if ( '\0' != sum.opponents[0] ) {
                XP_STRCAT( sum.opponents, ", " );
            }
            const XP_UCHAR* name = lp->name;
            if ( !!name[0] ) {
                XP_STRCAT( sum.opponents, name );
            }
        }
    }
    if ( !!gd->comms ) {
        sum.missingPlayers = ctrl_getMissingPlayers( ctrlr );
        sum.nPacketsPending =
            comms_countPendingPackets( gd->comms, &sum.quashed );
        ctrl_setReMissing( ctrlr, &sum );
    }

    updateSummary( duc, xwe, gd, &sum );
    gd->sumLoaded = XP_TRUE;
}

static void
sumToStream( XWStreamCtxt* stream, const GameSummary* sum, XP_U16 nPlayers )
{
    strm_putU8( stream, CUR_STREAM_VERS );

    XP_ASSERT( 0 < nPlayers && nPlayers <= 4 );
    strm_putBits( stream, 2, nPlayers - 1 );
    strm_putBits( stream, 1, sum->turnIsLocal );
    strm_putBits( stream, 1, sum->gameOver );
    strm_putBits( stream, 1, sum->quashed );
    strm_putBits( stream, 1, sum->canRematch );
    strm_putBits( stream, 1, sum->canOfferRO );

    strm_putBits( stream, 3, 1 + sum->turn );
    strm_putBits( stream, 4, sum->missingPlayers ); /* it's a bit vector */
    strm_putBits( stream, 2, sum->nMissing );
    strm_putBits( stream, 2, sum->nInvited );
    strm_putBits( stream, 1, sum->collapsed );
    strm_putBits( stream, 1, sum->hasChat );

    strm_putU32VL( stream, sum->nPacketsPending );
    strm_putU32( stream, sum->lastMoveTime );
    strm_putU32( stream, sum->dupTimerExpires );
    XP_ASSERT( -4 < sum->nMoves );
    strm_putU32VL( stream, 4 + sum->nMoves );

    for ( int ii = 0; ii < nPlayers; ++ii ) {
        strm_putU16( stream, sum->scores.arr[ii] );
    }

    stringToStream( stream, sum->opponents );
 }

static XP_Bool
gotSumFromStream( GameSummary* sump, XWStreamCtxt* stream )
{
    XP_ASSERT( START_OF_STREAM == strm_getPos( stream, POS_READ ) );
    strm_setVersion( stream, strm_getU8(stream) );

    GameSummary sum = {};
    XP_U16 nPlayers = 1 + strm_getBits( stream, 2 );
    sum.turnIsLocal = strm_getBits( stream, 1 );
    sum.gameOver = strm_getBits( stream, 1 );
    sum.quashed = strm_getBits( stream, 1 );
    sum.canRematch = strm_getBits( stream, 1 );
    sum.canOfferRO = strm_getBits( stream, 1 );

    sum.turn = strm_getBits( stream, 3 ) - 1;
    sum.missingPlayers = strm_getBits( stream, 4 );
    sum.nMissing = strm_getBits( stream, 2 );
    sum.nInvited = strm_getBits( stream, 2 );
    sum.collapsed = strm_getBits( stream, 1 );
    sum.hasChat = strm_getBits( stream, 1 );

    sum.nPacketsPending = strm_getU32VL( stream );
    sum.lastMoveTime = strm_getU32( stream );
    sum.dupTimerExpires = strm_getU32( stream );
    sum.nMoves = strm_getU32VL( stream ) - 4;

    for ( int ii = 0; ii < nPlayers; ++ii ) {
        sum.scores.arr[ii] = strm_getU16( stream );
    }

    stringFromStreamHere( stream, sum.opponents, VSIZE(sum.opponents) );

    *sump = sum;
    return XP_TRUE;
}

XP_U16
gr_visTileCount( DUTIL_GR_XWE )
{
    XP_U16 result = 0;
    GR_HEADER();
    result = board_visTileCount( gd->board );
    GR_HEADER_END();
    return result;
}

XP_Bool
gr_canTogglePending( DUTIL_GR_XWE )
{
    XP_Bool result = XP_FALSE;
    GR_HEADER();
    result = board_canTogglePending( gd->board );
    GR_HEADER_END();
    return result;
}

#ifdef DEBUG
XP_Bool
gr_getAddrDisabled( DUTIL_GR_XWE,
                    CommsConnType typ, XP_Bool send )
{
    XP_Bool result = XP_FALSE;
    GR_HEADER_WITH(COMMS);
    result = comms_getAddrDisabled( gd->comms, typ, send );
    GR_HEADER_END();
    return result;
}
#endif

XP_Bool
gr_prefsChanged( DUTIL_GR_XWE, const CommonPrefs* cp )
{
    XP_Bool result = XP_FALSE;
    GR_HEADER();
    result = board_prefsChanged( gd->board, xwe, cp );
    if ( result ) {
        invalAll( duc, xwe, gd );
    }
    GR_HEADER_END();
    return result;
}

void
gr_resetEngine( DUTIL_GR_XWE )
{
    GR_HEADER();
    board_resetEngine( gd->board );
    GR_HEADER_END();
}

void
gr_pause( DUTIL_GR_XWE, const XP_UCHAR* msg )
{
    GR_HEADER();
    board_pause( gd->board, xwe, msg );
    GR_HEADER_END();
}

void
gr_unpause( DUTIL_GR_XWE, const XP_UCHAR* msg )
{
    GR_HEADER();
    board_unpause( gd->board, xwe, msg );
    GR_HEADER_END();
}

XP_Bool
gr_setYOffset( DUTIL_GR_XWE, XP_U16 newOffset )
{
    XP_Bool result = XP_FALSE;
    GR_HEADER();
    result = board_setYOffset( gd->board, xwe, newOffset );
    GR_HEADER_END();
    return result;
}

XP_U16
gr_getYOffset( DUTIL_GR_XWE )
{
    XP_U16 result = 0;
    GR_HEADER();
    result = board_getYOffset( gd->board );
    GR_HEADER_END();
    return result;
}

XP_Bool
gr_getOpenChannel( DUTIL_GR_XWE, XP_U16* channel )
{
    XP_Bool result = XP_FALSE;
    GR_HEADER();
    result = ctrl_getOpenChannel( gd->ctrlr, channel );
    GR_HEADER_END();
    return result;
}

void
gr_tilesPicked( DUTIL_GR_XWE, XP_U16 player,
                const TrayTileSet* newTiles )
{
    GR_HEADER();
    ctrl_tilesPicked( gd->ctrlr, xwe, player, newTiles );
    GR_HEADER_END();
}

XP_Bool
gr_passwordProvided( DUTIL_GR_XWE, XP_U16 player, const XP_UCHAR* pass )
{
    XP_Bool result = XP_FALSE;
    GR_HEADER();
    result = board_passwordProvided( gd->board, xwe, player, pass );
    GR_HEADER_END();
    return result;
}

void
gr_hiliteCellAt( DUTIL_GR_XWE, XP_U16 col, XP_U16 row )
{
    GR_HEADER();
    board_hiliteCellAt( gd->board, xwe, col, row );
    GR_HEADER_END();
}

void
gr_selectPlayer( DUTIL_GR_XWE, XP_U16 newPlayer,
                 XP_Bool canPeek )
{
    GR_HEADER();
    board_selectPlayer( gd->board, xwe, newPlayer, canPeek );
    GR_HEADER_END();
}

XP_Bool
gr_canTrade( DUTIL_GR_XWE )
{
    XP_Bool result = XP_FALSE;
    GR_HEADER();
    result = board_canTrade( gd->board, xwe );
    GR_HEADER_END();
    return result;
}

XP_Bool
gr_canHint( DUTIL_GR_XWE )
{
    XP_Bool result = XP_FALSE;
    GR_HEADER();
    result = board_canHint( gd->board );
    GR_HEADER_END();
    return result;
}

const TrayTileSet*
gr_getPlayerTiles( DUTIL_GR_XWE, XP_S16 turn )
{
    const TrayTileSet* result = NULL;
    GR_HEADER();
    result = model_getPlayerTiles( gd->model, turn );
    GR_HEADER_END();
    return result;
}

XP_Bool
gr_commitTrade( DUTIL_GR_XWE, const TrayTileSet* oldTiles,
                TrayTileSet* newTiles )
{
    XP_Bool result = XP_FALSE;
    GR_HEADER();
    result = ctrl_commitTrade( gd->ctrlr, xwe, oldTiles, newTiles );
    GR_HEADER_END_SAVE();
    return result;
}

void
gr_writeGameHistory( DUTIL_GR_XWE, XWStreamCtxt* stream,
                     XP_Bool gameOver )
{
    GR_HEADER();
    model_writeGameHistory( gd->model, xwe, stream, gd->ctrlr, gameOver );
    GR_HEADER_END();
}

void
gr_saveSucceeded( DUTIL_GR_XWE, XP_U16 saveToken )
{
    GR_HEADER_WITH(COMMS);
    if ( !!gd->comms ) {
        comms_saveSucceeded( gd->comms, xwe, saveToken );
    }
    GR_HEADER_END();
}

void
gr_save( DUTIL_GR_XWE )
{
    GR_HEADER();
    gmgr_saveGame( duc, xwe, gr );
    GR_HEADER_END();
}

static XP_Bool
dummyProc( XW_UtilCtxt* XP_UNUSED(uc) )
{
    // LOG_FUNC();
    return XP_FALSE;
}

typedef XP_Bool (*ProcPtr)();

typedef struct _DummyUtilCtxt {
    XW_UtilCtxt super;
    GameData* gd;
} DummyUtilCtxt;

static void
dummyDestroy( XW_UtilCtxt* uc, XWEnv xwe )
{
#ifdef MEM_DEBUG
    XW_DUtilCtxt* duc = util_getDevUtilCtxt(uc);
    MemPoolCtx* mpool = duc->mpool;
#endif
    util_super_cleanup( uc, xwe );
    XP_FREE( mpool, uc->vtable );
    XP_FREE( mpool, uc );
}

/* static void */
/* dummyInformMove( XW_UtilCtxt* uc, XWEnv xwe, XP_S16 turn,  */
/*                  XWStreamCtxt* expl, XWStreamCtxt* words ) */
/* { */
/*     XP_USE(words); */
/*     XP_USE(turn); */
/*     XP_USE(expl); */

/*     XW_DUtilCtxt* duc = util_getDevUtilCtxt( uc ); */
/*     DummyUtilCtxt* dummy = (DummyUtilCtxt*)uc; */
/*     postGameChangeEvent( duc, xwe, dummy->gd, GCE_MOVE_MADE ); */
/* } */

/* static void */
/* dummyTurnChanged( XW_UtilCtxt* uc, XWEnv xwe, XP_S16 newTurn ) */
/* { */
/*     XP_USE(uc); */
/*     XP_USE(xwe); */
/*     XP_LOGFF( "(newTurn: %d)", newTurn ); */
/* } */

static XW_UtilCtxt*
makeDummyUtil( XW_DUtilCtxt* duc, GameData* gd )
{
    LOG_FUNC();
    DummyUtilCtxt* dummy = XP_CALLOC( duc->mpool, sizeof(*dummy) );
    dummy->gd = gd;
    XW_UtilCtxt* super = &dummy->super;
    super->vtable = XP_CALLOC( duc->mpool, sizeof(*super->vtable) );

    ProcPtr* start = (ProcPtr*)super->vtable;
    ProcPtr* end = start + (sizeof(*super->vtable) / sizeof(ProcPtr));
    for ( ProcPtr* ptr = (ProcPtr*)super->vtable; ptr < end; ++ptr ) {
        *ptr = dummyProc;
    }
    /* super->vtable->m_util_informMove = dummyInformMove; */
    /* super->vtable->m_util_turnChanged = dummyTurnChanged; */

    util_super_init( MPPARM(duc->mpool) super, &gd->gi, duc, gd->gr, dummyDestroy );

    return super;
}

static void
sendGameEventProc( XW_DUtilCtxt* duc, XWEnv xwe, void* closure,
                   TimerKey XP_UNUSED(key), XP_Bool fired )
{
    LOG_FUNC();
    if ( fired ) {
        GameChangeEvtData* gcedp = (GameChangeEvtData*)closure;
        GameData* gd = gcedp->gd;
        XP_ASSERT( gd->gcedp == gcedp );
        gd->gcedp = NULL;

        summarize( duc, xwe, gd );
        saveSummary( duc, xwe, gd );

        dutil_onGameChanged( duc, xwe, gd->gr, gcedp->gces );
    }
    XP_FREE( duc->mpool, closure );
}

static void
postGameChangeEvent( XW_DUtilCtxt* duc, XWEnv xwe, GameData* gd,
                     GameChangeEvent evt )
{
    GameChangeEvtData* gcedp = gd->gcedp;
    if ( !gcedp ) {
        gcedp = XP_CALLOC( duc->mpool, sizeof(*gcedp) );
        gcedp->gd = gd;
        gd->gcedp = gcedp;
        tmr_set( duc, xwe, 250, sendGameEventProc, gcedp );
    }
    XP_ASSERT( gcedp->gd == gd );
    gcedp->gces |= evt;
}

static void
thumbChanged( XW_DUtilCtxt* duc, XWEnv xwe, GameData* gd )
{
    destroyStreamIf( &gd->thumbData );
    postGameChangeEvent( duc, xwe, gd, GCE_BOARD_CHANGED );
}

/* Private functions */
void
gr_clearThumb( GameData* gd )
{
    destroyStreamIf( &gd->thumbData );
}

GroupRef
gr_getGroup( XW_DUtilCtxt* duc, GameRef gr, XWEnv xwe )
{
    GameData* gd = gmgr_getForRef(duc, xwe, gr, NULL);
    return gd->grp;
}

void
gr_setGroup( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr, GroupRef grp )
{
    GameData* gd = gmgr_getForRef(duc, xwe, gr, NULL);
    gd->grp = grp;
}

void
gr_freeData( DUTIL_GR_XWE, GameData* gd )
{
    XP_USE(gr);
    destroyData( duc, xwe, gd );
}

void
gr_checkNewDict( XW_DUtilCtxt* duc, XWEnv xwe, GameData* gd,
                 const DictionaryCtxt* dict )
{
    const XP_UCHAR* name = dict_getShortName(dict);
    XP_LOGFF( "(gr=" GR_FMT ", dict=%s)", gd->gr, name );
    /* Is this dict one that was missing for this game? */
    const CurGameInfo* gi = &gd->gi;
    if ( gi_isValid( gi )
         && gd->dictsSought
         && !gd->model
         && 0 == XP_STRCMP( gi->isoCodeStr, dict->isoCode ) ) {
        const XP_UCHAR* missingNames[5];
        XP_U16 count = VSIZE(missingNames);
        missingDictsImpl( gd, missingNames, &count );
        for ( int ii = 0; ii < count; ++ii ) {
            XP_LOGFF( "comparing %s %s", name, missingNames[ii] );
            if ( 0 == XP_STRCMP( name, missingNames[ii] ) ) {
                gd->dictsSought = XP_FALSE;
                dutil_missingDictAdded( duc, xwe, gd->gr, name );
            }
        }
    }
}

void
gr_checkGoneDict( XW_DUtilCtxt* duc, XWEnv xwe, GameData* gd,
                  const XP_UCHAR* dictName )
{
    XP_LOGFF( "(gr=" GR_FMT ", dict=%s)", gd->gr, dictName );
    /* IFF we're loaded and using this dict, unload it, and let the utilctxt
       know it's gone. The way most file systems work it'll still be available
       if we've loaded it, but let's assume the user wants to see immediate
       feedback from deleting the thing, e.g. an open game closing */
    const CurGameInfo* gi = &gd->gi;
    if ( gi_isValid( gi ) && gd->dictsSought ) {
        /* First, are we using this dict? */
        XP_Bool inUse = XP_FALSE;
        if ( !!gd->dict && 0 == XP_STRCMP( dict_getShortName(gd->dict), dictName ) ) {
            inUse = XP_TRUE;
        }
        for ( int ii = 0; !inUse && ii < gi->nPlayers; ++ii ) {
            const DictionaryCtxt* dict = gd->playerDicts.dicts[ii];
            if ( !!dict && 0 == XP_STRCMP( dict_getShortName(dict), dictName ) ) {
                inUse = XP_TRUE;
            }
        }

        if ( inUse ) {
            gd->dictsSought = XP_FALSE;
            unloadData( gd, xwe );
            if ( !!gd->util ) {
                util_dictGone( gd->util, xwe, dictName );
            }
            dutil_dictGone( duc, xwe, gd->gr, dictName );
        }
    }
}
