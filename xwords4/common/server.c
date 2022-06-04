/* -*- compile-command: "cd ../linux && make -j3 MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 1997 - 2021 by Eric House (xwords@eehouse.org).  All rights
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

#include "comtypes.h"
#include "server.h"
#include "util.h"
#include "model.h"
#include "comms.h"
#include "memstream.h"
#include "game.h"
#include "states.h"
#include "util.h"
#include "pool.h"
#include "engine.h"
#include "device.h"
#include "strutils.h"
#include "dbgutil.h"

#include "LocalizedStrIncludes.h"

#ifdef CPLUS
extern "C" {
#endif

#define LOCAL_ADDR NULL

enum {
    END_REASON_USER_REQUEST,
    END_REASON_OUT_OF_TILES,
    END_REASON_TOO_MANY_PASSES
};
typedef XP_U8 GameEndReason;

typedef enum { DUPE_STUFF_TRADES_SERVER,
               DUPE_STUFF_MOVES_SERVER,
               DUPE_STUFF_MOVE_CLIENT,
               DUPE_STUFF_PAUSE,
} DUPE_STUFF;

typedef enum {
    XWPROTO_ERROR = 0 /* illegal value */
    ,XWPROTO_CHAT      /* broadcast text message for display */
    ,XWPROTO_DEVICE_REGISTRATION /* client's first message to server */
    ,XWPROTO_CLIENT_SETUP /* server's first message to client */
    ,XWPROTO_MOVEMADE_INFO_CLIENT /* client reports a move it made */
    ,XWPROTO_MOVEMADE_INFO_SERVER /* server tells all clients about a move
                                     made by it or another client */
    ,XWPROTO_UNDO_INFO_CLIENT    /* client reports undo[s] on the device */
    ,XWPROTO_UNDO_INFO_SERVER    /* server reports undos[s] happening
                                  elsewhere*/
    //XWPROTO_CLIENT_MOVE_INFO,  /* client says "I made this move" */
    //XWPROTO_SERVER_MOVE_INFO,  /* server says "Player X made this move" */
/*     XWPROTO_CLIENT_TRADE_INFO, */
/*     XWPROTO_TRADEMADE_INFO, */
    ,XWPROTO_BADWORD_INFO
    ,XWPROTO_MOVE_CONFIRM  /* server tells move sender that move was
                              legal */
    //XWPROTO_MOVEMADE_INFO,       /* info about tiles placed and received */
    ,XWPROTO_CLIENT_REQ_END_GAME   /* non-server wants to end the game */
    ,XWPROTO_END_GAME               /* server says to end game */

    ,XWPROTO_NEW_PROTO

    ,XWPROTO_DUPE_STUFF         /* used for all duplicate-mode messages */
} XW_Proto;

#define XWPROTO_NBITS 4

typedef struct ServerPlayer {
    EngineCtxt* engine; /* each needs his own so don't interfere each other */
    XP_S8 deviceIndex;  /* 0 means local, -1 means unknown */
} ServerPlayer;

#define UNKNOWN_DEVICE -1
#define SERVER_DEVICE 0

typedef struct RemoteAddress {
    XP_PlayerAddr channelNo;
#ifdef STREAM_VERS_BIGBOARD
    XP_U8 streamVersion;
#endif
} RemoteAddress;

/* These are the parts of the server's state that needs to be preserved
   across a reset/new game */
typedef struct ServerVolatiles {
    ModelCtxt* model;
    CommsCtxt* comms;
    XW_UtilCtxt* util;
    XW_DUtilCtxt* dutil;
    CurGameInfo* gi;
    TurnChangeListener turnChangeListener;
    void* turnChangeData;
    TimerChangeListener timerChangeListener;
    void* timerChangeData;
    GameOverListener gameOverListener;
    void* gameOverData;
    XP_U16 bitsPerTile;
    XP_Bool showPrevMove;
    XP_Bool pickTilesCalled[MAX_NUM_PLAYERS];
} ServerVolatiles;

typedef struct ServerNonvolatiles {
    XP_U32 lastMoveTime;    /* seconds of last turn change */
    XP_S32 dupTimerExpires;
    XP_U8 nDevices;
    XW_State gameState;
    XW_State stateAfterShow;
    XP_S8 currentTurn; /* invalid when game is over */
    XP_S8 quitter;     /* -1 unless somebody resigned */
    XP_U8 pendingRegistrations; /* server-case only */
    XP_Bool showRobotScores;
    XP_Bool sortNewTiles;
    XP_Bool skipMQTTAdd;
#ifdef STREAM_VERS_BIGBOARD
    XP_U8 streamVersion;
#endif
#ifdef XWFEATURE_SLOW_ROBOT
    XP_U16 robotThinkMin, robotThinkMax;   /* not saved (yet) */
    XP_U16 robotTradePct;
#endif
#ifdef XWFEATURE_ROBOTPHONIES
    XP_U16 makePhonyPct;
#endif

    RemoteAddress addresses[MAX_NUM_PLAYERS];
    XWStreamCtxt* prevMoveStream;     /* save it to print later */
    XWStreamCtxt* prevWordsStream;
    XP_Bool dupTurnsMade[MAX_NUM_PLAYERS];
    XP_Bool dupTurnsForced[MAX_NUM_PLAYERS];
    XP_Bool dupTurnsSent;       /* used on client only */
} ServerNonvolatiles;

struct ServerCtxt {
    ServerVolatiles vol;
    ServerNonvolatiles nv;

    PoolContext* pool;

    BadWordInfo illegalWordInfo;
    XP_U16 lastMoveSource;

    ServerPlayer players[MAX_NUM_PLAYERS];
    XP_Bool serverDoing;
#ifdef XWFEATURE_SLOW_ROBOT
    XP_Bool robotWaiting;
#endif
    MPSLOT
};

#ifdef XWFEATURE_SLOW_ROBOT
# define ROBOTWAITING(s) (s)->robotWaiting
#else
# define ROBOTWAITING(s) XP_FALSE
#endif

# define dupe_timerRunning()    server_canPause(server)


#define NPASSES_OK(s) model_recentPassCountOk((s)->vol.model)

/******************************* prototypes *******************************/
static XP_Bool assignTilesToAll( ServerCtxt* server, XWEnv xwe );
static void makePoolOnce( ServerCtxt* server );

static XP_S8 getIndexForDevice( const ServerCtxt* server,
                                XP_PlayerAddr channelNo );
static XP_S8 getIndexForStream( const ServerCtxt* server,
                                const XWStreamCtxt* stream );

static void nextTurn( ServerCtxt* server, XWEnv xwe, XP_S16 nxtTurn );

static void doEndGame( ServerCtxt* server, XWEnv xwe, XP_S16 quitter );
static void endGameInternal( ServerCtxt* server, XWEnv xwe,
                             GameEndReason why, XP_S16 quitter );
static void badWordMoveUndoAndTellUser( ServerCtxt* server, XWEnv xwe,
                                        BadWordInfo* bwi );
static XP_Bool tileCountsOk( const ServerCtxt* server );
static void setTurn( ServerCtxt* server, XWEnv xwe, XP_S16 turn );
static XWStreamCtxt* mkServerStream( ServerCtxt* server );
static void fetchTiles( ServerCtxt* server, XWEnv xwe, XP_U16 playerNum,
                        XP_U16 nToFetch, const TrayTileSet* tradedTiles,
                        TrayTileSet* resultTiles, XP_Bool forceCanPlay );
static void finishMove( ServerCtxt* server, XWEnv xwe,
                        TrayTileSet* newTiles, XP_U16 turn );
static XP_Bool dupe_checkTurns( ServerCtxt* server, XWEnv xwe );
static void dupe_forceCommits( ServerCtxt* server, XWEnv xwe );

static void dupe_clearState( ServerCtxt* server );
static XP_U16 dupe_nextTurn( const ServerCtxt* server );
static void dupe_commitAndReportMove( ServerCtxt* server, XWEnv xwe,
                                      XP_U16 winner, XP_U16 nPlayers,
                                      XP_U16* scores, XP_U16 nTiles );
static XP_Bool commitMoveImpl( ServerCtxt* server, XWEnv xwe, XP_U16 player,
                               TrayTileSet* newTilesP, XP_Bool forced );
static void dupe_makeAndReportTrade( ServerCtxt* server, XWEnv xwe );
static void dupe_transmitPause( ServerCtxt* server, XWEnv xwe, DupPauseType typ,
                                XP_U16 turn, const XP_UCHAR* msg,
                                XP_S16 skipDev );
static void dupe_resetTimer( ServerCtxt* server, XWEnv xwe );
static XP_Bool setDupCheckTimer( ServerCtxt* server, XWEnv xwe );
static void sortTilesIf( ServerCtxt* server, XP_S16 turn );

#ifndef XWFEATURE_STANDALONE_ONLY
static XWStreamCtxt* messageStreamWithHeader( ServerCtxt* server, XWEnv xwe,
                                              XP_U16 devIndex, XW_Proto code );
static XP_Bool handleRegistrationMsg( ServerCtxt* server, XWEnv xwe,
                                      XWStreamCtxt* stream );
static XP_S8 registerRemotePlayer( ServerCtxt* server, XWEnv xwe,
                                   XWStreamCtxt* stream );
static void sendInitialMessage( ServerCtxt* server, XWEnv xwe );
static void sendBadWordMsgs( ServerCtxt* server, XWEnv xwe );
static XP_Bool handleIllegalWord( ServerCtxt* server, XWEnv xwe,
                                  XWStreamCtxt* incoming );
static void tellMoveWasLegal( ServerCtxt* server, XWEnv xwe );
static void writeProto( const ServerCtxt* server, XWStreamCtxt* stream, 
                        XW_Proto proto );
#endif

#define PICK_NEXT -1
#define PICK_CUR -2

#define LOG_GAMEID()     XP_LOGFF("gameID: %d", server->vol.gi->gameID )

#if defined DEBUG && ! defined XWFEATURE_STANDALONE_ONLY
static char*
getStateStr( XW_State st )
{
#   define CASESTR(c) case c: return #c
    switch( st ) {
        CASESTR(XWSTATE_NONE);
        CASESTR(XWSTATE_BEGIN);
        CASESTR(XWSTATE_NEWCLIENT);
        CASESTR(XWSTATE_NEED_SHOWSCORE);
        CASESTR(XWSTATE_RECEIVED_ALL_REG);
        CASESTR(XWSTATE_NEEDSEND_BADWORD_INFO);
        CASESTR(XWSTATE_MOVE_CONFIRM_WAIT);
        CASESTR(XWSTATE_MOVE_CONFIRM_MUSTSEND);
        CASESTR(XWSTATE_NEEDSEND_ENDGAME);
        CASESTR(XWSTATE_INTURN);
        CASESTR(XWSTATE_GAMEOVER);
    default:
        XP_ASSERT(0);
        return "unknown";
    }
#   undef CASESTR
}
#endif

#ifdef DEBUG
static void
logNewState( XW_State old, XW_State newst, const char* caller )
{
    if ( old != newst ) {
        char* oldStr = getStateStr(old);
        char* newStr = getStateStr(newst);
        XP_LOGFF( "state transition %s => %s (from %s())", oldStr, newStr, caller );
    }
}
# define SETSTATE( server, st ) {                                   \
        XW_State old = (server)->nv.gameState;                      \
        (server)->nv.gameState = (st);                              \
        logNewState( old, st, __func__);                            \
    }
#else
# define SETSTATE( s, st ) (s)->nv.gameState = (st)
#endif

static XP_Bool
inDuplicateMode( const ServerCtxt* server )
{
    XP_Bool result = server->vol.gi->inDuplicateMode;
    // LOG_RETURNF( "%d", result );
    return result;
}

/*****************************************************************************
 *
 ****************************************************************************/
#ifndef XWFEATURE_STANDALONE_ONLY
static void
syncPlayers( ServerCtxt* server )
{
    XP_U16 ii;
    CurGameInfo* gi = server->vol.gi;
    LocalPlayer* lp = gi->players;
    ServerPlayer* player = server->players;
    for ( ii = 0; ii < gi->nPlayers; ++ii, ++lp, ++player ) {
        if ( !lp->isLocal/*  && !lp->name */ ) {
            ++server->nv.pendingRegistrations;
        }
        player->deviceIndex = lp->isLocal? SERVER_DEVICE : UNKNOWN_DEVICE;
    }
}
#else
# define syncPlayers( server )
#endif

static XP_Bool
amServer( const ServerCtxt* server )
{
    XP_Bool result = SERVER_ISSERVER == server->vol.gi->serverRole;
    // LOG_RETURNF( "%d (seed=%d)", result, comms_getChannelSeed( server->vol.comms ) );
    return result;
}

#ifdef DEBUG
XP_Bool server_getIsServer( const ServerCtxt* server ) { return amServer(server); }
#endif

static void
initServer( ServerCtxt* server, XWEnv xwe )
{
    LOG_GAMEID();
    setTurn( server, xwe, -1 ); /* game isn't under way yet */

    if ( 0 ) {
#ifndef XWFEATURE_STANDALONE_ONLY
    } else if ( server->vol.gi->serverRole == SERVER_ISCLIENT ) {
        SETSTATE( server, XWSTATE_NONE );
#endif
    } else {
        SETSTATE( server, XWSTATE_BEGIN );
    }

    syncPlayers( server );

    server->nv.nDevices = 1; /* local device (0) is always there */
#ifdef STREAM_VERS_BIGBOARD
    server->nv.streamVersion = STREAM_SAVE_PREVWORDS; /* default to old */
#endif
    server->nv.quitter = -1;
} /* initServer */

ServerCtxt* 
server_make( MPFORMAL XWEnv xwe, ModelCtxt* model, CommsCtxt* comms, XW_UtilCtxt* util )
{
    ServerCtxt* result = (ServerCtxt*)XP_MALLOC( mpool, sizeof(*result) );

    if ( result != NULL ) {
        XP_MEMSET( result, 0, sizeof(*result) );

        MPASSIGN(result->mpool, mpool);

        result->vol.model = model;
        result->vol.comms = comms;
        result->vol.util = util;
        result->vol.dutil = util_getDevUtilCtxt( util, xwe );
        result->vol.gi = util->gameInfo;

        initServer( result, xwe );
    }
    return result;
} /* server_make */

static void
getNV( XWStreamCtxt* stream, ServerNonvolatiles* nv, XP_U16 nPlayers )
{
    XP_U16 ii;
    XP_U16 version = stream_getVersion( stream );

    if ( STREAM_VERS_DICTNAME <= version ) {
        nv->lastMoveTime = stream_getU32( stream );
    }
    if ( STREAM_VERS_DUPLICATE <= version ) {
        nv->dupTimerExpires = stream_getU32( stream );
    }

    if ( version < STREAM_VERS_SERVER_SAVES_TOSHOW ) {
        /* no longer used */
        (void)stream_getBits( stream, 3 ); /* was npassesinrow */
    }

    nv->nDevices = (XP_U8)stream_getBits( stream, NDEVICES_NBITS );
    if ( version > STREAM_VERS_41B4 ) {
        ++nv->nDevices;
    }

    XP_ASSERT( XWSTATE_LAST <= 1<<4 );
    nv->gameState = (XW_State)stream_getBits( stream, XWSTATE_NBITS );
    if ( version >= STREAM_VERS_SERVER_SAVES_TOSHOW ) {
        nv->stateAfterShow = (XW_State)stream_getBits( stream, XWSTATE_NBITS );
    }

    nv->currentTurn = (XP_S8)stream_getBits( stream, NPLAYERS_NBITS ) - 1;
    if ( STREAM_VERS_DICTNAME <= version ) {
        nv->quitter = (XP_S8)stream_getBits( stream, NPLAYERS_NBITS ) - 1;
    }
    nv->pendingRegistrations = (XP_U8)stream_getBits( stream, NPLAYERS_NBITS );

    for ( ii = 0; ii < nPlayers; ++ii ) {
        nv->addresses[ii].channelNo =
            (XP_PlayerAddr)stream_getBits( stream, 16 );
#ifdef STREAM_VERS_BIGBOARD
        nv->addresses[ii].streamVersion = STREAM_VERS_BIGBOARD <= version ?
            stream_getBits( stream, 8 ) : STREAM_SAVE_PREVWORDS;
#endif
    }
#ifdef STREAM_VERS_BIGBOARD
    if ( STREAM_SAVE_PREVWORDS < version ) {
        nv->streamVersion = stream_getU8 ( stream );
    }
    /* XP_LOGF( "%s: read streamVersion: 0x%x", __func__, nv->streamVersion ); */
#endif

    if ( version >= STREAM_VERS_DUPLICATE ) {
        for ( ii = 0; ii < nPlayers; ++ii ) {
            nv->dupTurnsMade[ii] = stream_getBits( stream, 1 );
            XP_LOGF( "%s(): dupTurnsMade[%d]: %d", __func__, ii, nv->dupTurnsMade[ii] );
            nv->dupTurnsForced[ii] = stream_getBits( stream, 1 );
        }
        nv->dupTurnsSent = stream_getBits( stream, 1 );
    }
} /* getNV */

static void
putNV( XWStreamCtxt* stream, const ServerNonvolatiles* nv, XP_U16 nPlayers )
{
    XP_U16 ii;

    stream_putU32( stream, nv->lastMoveTime );
    stream_putU32( stream, nv->dupTimerExpires );

    /* number of players is upper limit on device count */
    stream_putBits( stream, NDEVICES_NBITS, nv->nDevices-1 );

    XP_ASSERT( XWSTATE_LAST <= 1<<4 );
    stream_putBits( stream, XWSTATE_NBITS, nv->gameState );
    stream_putBits( stream, XWSTATE_NBITS, nv->stateAfterShow );

    /* +1: make -1 (NOTURN) into a positive number */
    XP_ASSERT( -1 <= nv->currentTurn && nv->currentTurn < MAX_NUM_PLAYERS );
    stream_putBits( stream, NPLAYERS_NBITS, nv->currentTurn+1 );
    stream_putBits( stream, NPLAYERS_NBITS, nv->quitter+1 );
    stream_putBits( stream, NPLAYERS_NBITS, nv->pendingRegistrations );

    for ( ii = 0; ii < nPlayers; ++ii ) {
        stream_putBits( stream, 16, nv->addresses[ii].channelNo );
#ifdef STREAM_VERS_BIGBOARD
        stream_putBits( stream, 8, nv->addresses[ii].streamVersion );
#endif
    }
#ifdef STREAM_VERS_BIGBOARD
    stream_putU8( stream, nv->streamVersion );
    /* XP_LOGF( "%s: wrote streamVersion: 0x%x", __func__, nv->streamVersion ); */
#endif

    for ( ii = 0; ii < nPlayers; ++ii ) {
        stream_putBits( stream, 1, nv->dupTurnsMade[ii] );
        stream_putBits( stream, 1, nv->dupTurnsForced[ii] );
    }
    stream_putBits( stream, 1, nv->dupTurnsSent );
} /* putNV */

static XWStreamCtxt*
readStreamIf( ServerCtxt* server, XWStreamCtxt* in )
{
    XWStreamCtxt* result = NULL;
    XP_U16 len = stream_getU16( in );
    if ( 0 < len ) {
        result = mkServerStream( server );
        stream_getFromStream( result, in, len );
    }
    return result;
}

static void
writeStreamIf( XWStreamCtxt* dest, XWStreamCtxt* src )
{
    XP_U16 len = !!src ? stream_getSize( src ) : 0;
    stream_putU16( dest, len );
    if ( 0 < len ) {
        XWStreamPos pos = stream_getPos( src, POS_READ );
        stream_getFromStream( dest, src, len );
        (void)stream_setPos( src, POS_READ, pos );
    }
}

static void
informMissing( const ServerCtxt* server, XWEnv xwe )
{
    const XP_Bool isServer = amServer( server );
    const CommsCtxt* comms = server->vol.comms;
    const CurGameInfo* gi = server->vol.gi;
    CommsAddrRec addr;
    CommsAddrRec* addrP;
    if ( !comms ) {
        addrP = NULL;
    } else {
        addrP = &addr;
        comms_getAddr( comms, addrP );
    }

    XP_U16 nDevs = 0;
    XP_U16 nPending = 0;
    if ( XWSTATE_BEGIN < server->nv.gameState ) {
        /* do nothing */
    } else if ( isServer ) {
        nPending = server->nv.pendingRegistrations;
        nDevs = server->nv.nDevices - 1;
    } else if ( SERVER_ISCLIENT == gi->serverRole ) {
        nPending = gi->nPlayers - gi_countLocalPlayers( gi, XP_FALSE);
    }
    util_informMissing( server->vol.util, xwe, isServer, addrP, nDevs, nPending );
}

XP_U16
server_getPendingRegs( const ServerCtxt* server )
{
    XP_U16 nPending = amServer( server ) ? server->nv.pendingRegistrations : 0;
    return nPending;
}

ServerCtxt*
server_makeFromStream( MPFORMAL XWEnv xwe, XWStreamCtxt* stream, ModelCtxt* model,
                       CommsCtxt* comms, XW_UtilCtxt* util, XP_U16 nPlayers )
{
    ServerCtxt* server;
    XP_U16 version = stream_getVersion( stream );
    short ii;

    server = server_make( MPPARM(mpool) xwe, model, comms, util ); /* BAE */
    getNV( stream, &server->nv, nPlayers );
    
    if ( stream_getBits(stream, 1) != 0 ) {
        server->pool = pool_makeFromStream( MPPARM(mpool) stream );
    }

    for ( ii = 0; ii < nPlayers; ++ii ) {
        ServerPlayer* player = &server->players[ii];

        player->deviceIndex = stream_getU8( stream );

        if ( stream_getU8( stream ) != 0 ) {
            player->engine = engine_makeFromStream( MPPARM(mpool)
                                                    stream, util );
        }
    }

    if ( STREAM_VERS_ALWAYS_MULTI <= version
#ifndef PREV_WAS_STANDALONE_ONLY
         || XP_TRUE
#endif
         ) { 
        server->lastMoveSource = (XP_U16)stream_getBits( stream, 2 );
    }

    if ( version >= STREAM_SAVE_PREVMOVE ) {
        server->nv.prevMoveStream = readStreamIf( server, stream );
    }
    if ( version >= STREAM_SAVE_PREVWORDS ) {
        server->nv.prevWordsStream = readStreamIf( server, stream );
    }

    /* Hack alert: recovering from an apparent bug that leaves the game
       thinking it's a client but being in the host-only XWSTATE_BEGIN
       state. */
    if ( server->nv.gameState == XWSTATE_BEGIN &&
         server->vol.gi->serverRole == SERVER_ISCLIENT ) {
        XP_LOGFF( "server_makeFromStream(): fixing state" );
        SETSTATE( server, XWSTATE_NONE );
    }

    informMissing( server, xwe );
    return server;
} /* server_makeFromStream */

void
server_writeToStream( const ServerCtxt* server, XWStreamCtxt* stream )
{
    XP_U16 ii;
    XP_U16 nPlayers = server->vol.gi->nPlayers;

    putNV( stream, &server->nv, nPlayers );

    stream_putBits( stream, 1, server->pool != NULL );
    if ( server->pool != NULL ) {
        pool_writeToStream( server->pool, stream );
    }

    for ( ii = 0; ii < nPlayers; ++ii ) {
        const ServerPlayer* player = &server->players[ii];

        stream_putU8( stream, player->deviceIndex );

        stream_putU8( stream, (XP_U8)(player->engine != NULL) );
        if ( player->engine != NULL ) {
            engine_writeToStream( player->engine, stream );
        }
    }

    stream_putBits( stream, 2, server->lastMoveSource );

    writeStreamIf( stream, server->nv.prevMoveStream );
    writeStreamIf( stream, server->nv.prevWordsStream );
} /* server_writeToStream */

void
server_onRoleChanged( ServerCtxt* server, XWEnv xwe, XP_Bool amNowGuest )
{
    if ( amNowGuest == amServer(server) ) { /* do I need to change */
        XP_ASSERT ( amNowGuest );
        if ( amNowGuest ) {
            server->vol.gi->serverRole = SERVER_ISCLIENT;
            server_reset( server, xwe, server->vol.comms );

            SETSTATE( server, XWSTATE_NEWCLIENT );
            util_requestTime( server->vol.util, xwe );
        }
    }
}

static void
cleanupServer( ServerCtxt* server, XWEnv xwe )
{
    XP_U16 ii;
    for ( ii = 0; ii < VSIZE(server->players); ++ii ){
        ServerPlayer* player = &server->players[ii];
        if ( player->engine != NULL ) {
            engine_destroy( player->engine );
        }
    }
    XP_MEMSET( server->players, 0, sizeof(server->players) );

    if ( server->pool != NULL ) {
        pool_destroy( server->pool );
        server->pool = (PoolContext*)NULL;
    }

    if ( !!server->nv.prevMoveStream ) {
        stream_destroy( server->nv.prevMoveStream, xwe );
    }
    if ( !!server->nv.prevWordsStream ) {
        stream_destroy( server->nv.prevWordsStream, xwe );
    }

    XP_MEMSET( &server->nv, 0, sizeof(server->nv) );
} /* cleanupServer */

void
server_reset( ServerCtxt* server, XWEnv xwe, CommsCtxt* comms )
{
    LOG_GAMEID();
    ServerVolatiles vol = server->vol;

    cleanupServer( server, xwe );

    vol.comms = comms;
    server->vol = vol;

    initServer( server, xwe );
} /* server_reset */

void
server_destroy( ServerCtxt* server, XWEnv xwe )
{
    cleanupServer( server, xwe );

    XP_FREE( server->mpool, server );
} /* server_destroy */

#ifdef XWFEATURE_SLOW_ROBOT
static int
figureSleepTime( const ServerCtxt* server )
{
    int result = 0;
    XP_U16 min = server->nv.robotThinkMin;
    XP_U16 max = server->nv.robotThinkMax;
    if ( min < max ) {
        int diff = max - min + 1;
        result = XP_RANDOM() % diff;
    }
    result += min;

    return result;
}
#endif

void
server_prefsChanged( ServerCtxt* server, const CommonPrefs* cp )
{
    server->nv.showRobotScores = cp->showRobotScores;
    server->nv.sortNewTiles = cp->sortNewTiles;
    server->nv.skipMQTTAdd = cp->skipMQTTAdd;
#ifdef XWFEATURE_SLOW_ROBOT
    server->nv.robotThinkMin = cp->robotThinkMin;
    server->nv.robotThinkMax = cp->robotThinkMax;
    server->nv.robotTradePct = cp->robotTradePct;
#endif
#ifdef XWFEATURE_ROBOTPHONIES
    server->nv.makePhonyPct = cp->makePhonyPct;
#endif
} /* server_prefsChanged */

XP_S16
server_countTilesInPool( ServerCtxt* server )
{
    XP_S16 result = -1;
    PoolContext* pool = server->pool;
    if ( !!pool ) {
        result = pool_getNTilesLeft( pool );
    }
    return result;
} /* server_countTilesInPool */

/* I'm a client device.  It's my job to start the whole conversation by
 * contacting the server and telling him that I exist (and some other stuff,
 * including what the players here want to be called.)
 */ 
#define NAME_LEN_NBITS 6
#define MAX_NAME_LEN ((1<<(NAME_LEN_NBITS-1))-1)
#ifndef XWFEATURE_STANDALONE_ONLY

/* addMQTTDevID() and readMQTTDevID() exist to work around the case where
   folks start games using agreed-upon relay room names rather than
   invitations. In that case the MQTT devID hasn't been transmitted and so
   only old-style relay communication is possible. This hack sends the mqtt
   devIDs in the same host->guest message that sets the gameID. Guests will
   start using mqtt to transmit and in so doing transmit their own devIDs to
   the host.
*/
static void
addMQTTDevID( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* stream )
{
    MQTTDevID devID;
    dvc_getMQTTDevID( server->vol.dutil, xwe, &devID );

    XP_UCHAR buf[32];
    formatMQTTDevID( &devID, buf, VSIZE(buf) );
    stringToStream( stream, buf );
}

static void
readMQTTDevID( ServerCtxt* server, XWStreamCtxt* stream )
{
    if ( 0 < stream_getSize( stream ) ) {
        XP_UCHAR buf[32];
        stringFromStreamHere( stream, buf, VSIZE(buf) );

        MQTTDevID devID;
        if ( strToMQTTCDevID( buf, &devID ) ) {
            if ( server->nv.skipMQTTAdd ) {
                XP_LOGFF( "skipMQTTAdd: %s", boolToStr(server->nv.skipMQTTAdd) );
            } else {
                XP_PlayerAddr channelNo = stream_getAddress( stream );
                comms_addMQTTDevID( server->vol.comms, channelNo, &devID );
            }
        }
    }
}

XP_Bool
server_initClientConnection( ServerCtxt* server, XWEnv xwe )
{
    XP_Bool result;
    XP_LOGFF( "gameState: %s; gameID: %d", getStateStr(server->nv.gameState),
              server->vol.gi->gameID );
    CurGameInfo* gi = server->vol.gi;
    XP_U16 nPlayers;
    LocalPlayer* lp;
#ifdef DEBUG
    XP_U16 ii = 0;
#endif

    XP_ASSERT( gi->serverRole == SERVER_ISCLIENT );
    result = server->nv.gameState == XWSTATE_NONE;
    if ( result ) {
        XWStreamCtxt* stream = messageStreamWithHeader( server, xwe, SERVER_DEVICE,
                                                        XWPROTO_DEVICE_REGISTRATION );
        nPlayers = gi->nPlayers;
        XP_ASSERT( nPlayers > 0 );
        XP_U16 localPlayers = gi_countLocalPlayers( gi, XP_FALSE);
        XP_ASSERT( 0 < localPlayers );
        stream_putBits( stream, NPLAYERS_NBITS, localPlayers );

        for ( lp = gi->players; nPlayers-- > 0; ++lp ) {
            XP_UCHAR* name;
            XP_U8 len;

#ifdef DEBUG
            XP_ASSERT( ii < MAX_NUM_PLAYERS );
            ++ii;
#endif
            if ( !lp->isLocal ) {
                continue;
            }

            stream_putBits( stream, 1, LP_IS_ROBOT(lp) ); /* better not to
                                                             send this */
            /* The first nPlayers players are the ones we'll use.  The local flag
               doesn't matter when for SERVER_ISCLIENT. */
            name = emptyStringIfNull(lp->name);
            len = XP_STRLEN(name);
            if ( len > MAX_NAME_LEN ) {
                len = MAX_NAME_LEN;
            }
            stream_putBits( stream, NAME_LEN_NBITS, len );
            stream_putBytes( stream, name, len );
            XP_LOGF( "%s(): wrote local name %s", __func__, name );
        }
#ifdef STREAM_VERS_BIGBOARD
        stream_putU8( stream, CUR_STREAM_VERS );
#endif
        stream_destroy( stream, xwe );
    } else {
        XP_LOGFF( "wierd state: %s (expected XWSTATE_NONE); dropping message",
                  getStateStr(server->nv.gameState) );
    }
    return result;
} /* server_initClientConnection */
#endif

#ifdef XWFEATURE_CHAT
static void
sendChatTo( ServerCtxt* server, XWEnv xwe, XP_U16 devIndex, const XP_UCHAR* msg,
            XP_S8 from, XP_U32 timestamp )
{
    if ( comms_canChat( server->vol.comms ) ) {
        XWStreamCtxt* stream = messageStreamWithHeader( server, xwe, devIndex,
                                                        XWPROTO_CHAT );
        stringToStream( stream, msg );
        stream_putU8( stream, from );
        stream_putU32( stream, timestamp );
        stream_destroy( stream, xwe );
    } else {
        XP_LOGF( "%s: dropping chat %s; queue too full?", __func__, msg );
    }
}

static void
sendChatToClientsExcept( ServerCtxt* server, XWEnv xwe, XP_U16 skip,
                         const XP_UCHAR* msg, XP_S8 from, XP_U32 timestamp )
{
    for ( XP_U16 devIndex = 1; devIndex < server->nv.nDevices; ++devIndex ) {
        if ( devIndex != skip ) {
            sendChatTo( server, xwe, devIndex, msg, from, timestamp );
        }
    }
}

void
server_sendChat( ServerCtxt* server, XWEnv xwe, const XP_UCHAR* msg, XP_S16 from )
{
    XP_U32 timestamp = dutil_getCurSeconds( server->vol.dutil, xwe );
    if ( server->vol.gi->serverRole == SERVER_ISCLIENT ) {
        sendChatTo( server, xwe, SERVER_DEVICE, msg, from, timestamp );
    } else {
        sendChatToClientsExcept( server, xwe, SERVER_DEVICE, msg, from, timestamp );
    }
}

static XP_Bool
receiveChat( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* incoming )
{
    XP_UCHAR* msg = stringFromStream( server->mpool, incoming );
    XP_S16 from = 1 <= stream_getSize( incoming )
        ? stream_getU8( incoming ) : -1;
    XP_U32 timestamp = sizeof(timestamp) <= stream_getSize( incoming )
        ? stream_getU32( incoming ) : 0;
    if ( amServer( server ) ) {
        XP_U16 sourceClientIndex = getIndexForStream( server, incoming );
        sendChatToClientsExcept( server, xwe, sourceClientIndex, msg, from,
                                 timestamp );
    }
    util_showChat( server->vol.util, xwe, msg, from, timestamp );
    XP_FREE( server->mpool, msg );
    return XP_TRUE;
}
#endif

static void
callTurnChangeListener( const ServerCtxt* server, XWEnv xwe )
{
    if ( server->vol.turnChangeListener != NULL ) {
        (*server->vol.turnChangeListener)( xwe, server->vol.turnChangeData );
    }
} /* callTurnChangeListener */

static void
callDupTimerListener( const ServerCtxt* server, XWEnv xwe, XP_S32 oldVal, XP_S32 newVal )
{
    if ( server->vol.timerChangeListener != NULL ) {
        (*server->vol.timerChangeListener)( xwe, server->vol.timerChangeData,
                                            server->vol.gi->gameID, oldVal, newVal );
    } else {
        XP_LOGF( "%s(): no listener!!", __func__ );
    }
}

#ifndef XWFEATURE_STANDALONE_ONLY
# ifdef STREAM_VERS_BIGBOARD
static void
setStreamVersion( ServerCtxt* server )
{
    XP_U8 streamVersion = CUR_STREAM_VERS;
    for ( XP_U16 devIndex = 1; devIndex < server->nv.nDevices; ++devIndex ) {
        XP_U8 devVersion = server->nv.addresses[devIndex].streamVersion;
        if ( devVersion < streamVersion ) {
            streamVersion = devVersion;
        }
    }
    XP_LOGF( "%s: setting streamVersion: 0x%x", __func__, streamVersion );
    server->nv.streamVersion = streamVersion;

    CurGameInfo* gi = server->vol.gi;
    if ( STREAM_VERS_NINETILES > streamVersion ) {
        if ( 7 < gi->traySize ) {
            XP_LOGFF( "reducing tray size from %d to 7", gi->traySize );
            gi->traySize = gi->bingoMin = 7;
        }
        model_forceStack7Tiles( server->vol.model );
    }
}

static void
checkResizeBoard( ServerCtxt* server )
{
    CurGameInfo* gi = server->vol.gi;
    if ( STREAM_VERS_BIGBOARD > server->nv.streamVersion && gi->boardSize > 15) {
        XP_LOGF( "%s: dropping board size from %d to 15", __func__, gi->boardSize );
        gi->boardSize = 15;
        model_setSize( server->vol.model, 15 );
    }
}
# else
#  define setStreamVersion(s)
#  define checkResizeBoard(s)
# endif

static XP_Bool
handleRegistrationMsg( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* stream )
{
    XP_Bool success = XP_TRUE;
    XP_U16 playersInMsg;
    XP_S8 clientIndex = 0;      /* quiet compiler */
    XP_U16 ii = 0;
    LOG_FUNC();

    /* code will have already been consumed */
    playersInMsg = (XP_U16)stream_getBits( stream, NPLAYERS_NBITS );
    XP_ASSERT( playersInMsg > 0 );

    if ( server->nv.pendingRegistrations < playersInMsg ) {
        XP_LOGF( "%s: got %d players but missing only %d", __func__, 
                 playersInMsg, server->nv.pendingRegistrations );
        util_userError( server->vol.util, xwe, ERR_REG_UNEXPECTED_USER );
        success = XP_FALSE;
    } else {
#ifdef DEBUG
        XP_S8 prevIndex = -1;
#endif
        for ( ; ii < playersInMsg; ++ii ) {
            clientIndex = registerRemotePlayer( server, xwe, stream );
            if ( -1 == clientIndex ) {
                success = XP_FALSE;
                break;
            }

            /* This is abusing the semantics of turn change -- at least in the
               case where there is another device yet to register -- but we
               need to let the board know to redraw the scoreboard with more
               players there. */
            callTurnChangeListener( server, xwe );
#ifdef DEBUG
            XP_ASSERT( ii == 0 || prevIndex == clientIndex );
            prevIndex = clientIndex;
#endif
        }

    }

    if ( success ) {
#ifdef STREAM_VERS_BIGBOARD
        if ( 0 < stream_getSize(stream) ) {
            XP_U8 streamVersion = stream_getU8( stream );
            if ( streamVersion >= STREAM_VERS_BIGBOARD ) {
                XP_LOGF( "%s: upping device %d streamVersion to %d",
                         __func__, clientIndex, streamVersion );
                server->nv.addresses[clientIndex].streamVersion = streamVersion;
            }
        }
#endif

        if ( server->nv.pendingRegistrations == 0 ) {
            XP_ASSERT( ii == playersInMsg ); /* otherwise malformed */
            setStreamVersion( server );
            checkResizeBoard( server );
            (void)assignTilesToAll( server, xwe );
            SETSTATE( server, XWSTATE_RECEIVED_ALL_REG );
        }
        informMissing( server, xwe );
    }

    return success;
} /* handleRegistrationMsg */

static XP_U16
bitsPerTile( ServerCtxt* server )
{
    if ( 0 == server->vol.bitsPerTile ) {
        const DictionaryCtxt* dict = model_getDictionary( server->vol.model );
        XP_U16 nFaces = dict_numTileFaces( dict );
        server->vol.bitsPerTile = nFaces <= 32? 5 : 6;
    }
    return server->vol.bitsPerTile;
}

static void
dupe_setupShowTrade( ServerCtxt* server, XWEnv xwe, XP_U16 nTiles )
{
    XP_ASSERT( inDuplicateMode(server) );
    XP_ASSERT( !server->nv.prevMoveStream );

    XP_UCHAR buf[128];
    const XP_UCHAR* fmt = dutil_getUserString( server->vol.dutil, xwe, STRD_DUP_TRADED );
    XP_SNPRINTF( buf, VSIZE(buf), fmt, nTiles );

    XWStreamCtxt* stream = mkServerStream( server );
    stream_catString( stream, buf );

    server->nv.prevMoveStream = stream;
    server->vol.showPrevMove = XP_TRUE;
}

static void
dupe_setupShowMove( ServerCtxt* server, XWEnv xwe, XP_U16* scores )
{
    XP_ASSERT( inDuplicateMode(server) );
    XP_ASSERT( !server->nv.prevMoveStream ); /* firing */

    const CurGameInfo* gi = server->vol.gi;
    const XP_U16 nPlayers = gi->nPlayers;

    XWStreamCtxt* stream = mkServerStream( server );

    XP_U16 lastMax = 0x7FFF;
    for ( XP_U16 nDone = 0; nDone < nPlayers; ) {

        /* Find the largest score we haven't already done */
        XP_U16 thisMax = 0;
        for ( XP_U16 ii = 0; ii < nPlayers; ++ii ) {
            XP_U16 score = scores[ii];
            if ( score < lastMax && score > thisMax ) {
                thisMax = score;
            }
        }

        /* Process everybody with that score */
        const XP_UCHAR* fmt = dutil_getUserString( server->vol.dutil, xwe,
                                                   STRSD_DUP_ONESCORE );
        for ( XP_U16 ii = 0; ii < nPlayers; ++ii ) {
            if ( scores[ii] == thisMax ) {
                ++nDone;
                XP_UCHAR buf[128];
                XP_SNPRINTF( buf, VSIZE(buf), fmt, gi->players[ii].name, scores[ii] );
                stream_catString( stream, buf );
            }
        }
        lastMax = thisMax;
    }

    server->nv.prevMoveStream = stream;
    server->vol.showPrevMove = XP_TRUE;
}

static void
addDupeStuffMark( XWStreamCtxt* stream, DUPE_STUFF typ )
{
    stream_putBits( stream, 3, typ );
}

static DUPE_STUFF
getDupeStuffMark( XWStreamCtxt* stream )
{
    return (DUPE_STUFF)stream_getBits( stream, 3 );
}

/* Called on server when client has sent a message giving its local players'
   duplicate moves for a single turn. */
static XP_Bool
dupe_handleClientMoves( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* stream )
{
    LOG_FUNC();
    ModelCtxt* model = server->vol.model;
    XP_Bool success = XP_TRUE;

    XP_U16 movesInMsg = (XP_U16)stream_getBits( stream, NPLAYERS_NBITS );
    XP_LOGF( "%s(): reading %d moves", __func__, movesInMsg );
    for ( XP_U16 ii = 0; success && ii < movesInMsg; ++ii ) {
        XP_U16 turn = (XP_U16)stream_getBits( stream, PLAYERNUM_NBITS );
        XP_Bool forced = (XP_Bool)stream_getBits( stream, 1 );

        model_resetCurrentTurn( model, xwe, turn );
        success = model_makeTurnFromStream( model, xwe, turn, stream );
        XP_ASSERT( success );   /* shouldn't fail in duplicate case */
        if ( success ) {
            XP_ASSERT( !server->nv.dupTurnsMade[turn] ); /* firing */
            XP_ASSERT( !server->vol.gi->players[turn].isLocal );
            server->nv.dupTurnsMade[turn] = XP_TRUE;
            server->nv.dupTurnsForced[turn] = forced;
        }
    }

    if ( success ) {
        dupe_checkTurns( server, xwe );
        nextTurn( server, xwe, PICK_NEXT );
    }

    LOG_RETURNF( "%d", success );
    return success;
}

static void
updateOthersTiles( ServerCtxt* server, XWEnv xwe )
{
    sortTilesIf( server, DUP_PLAYER );
    model_cloneDupeTrays( server->vol.model, xwe );
}

static XP_Bool
checkDupTimerProc( void* closure, XWEnv xwe, XWTimerReason XP_UNUSED_DBG(XP_why) )
{
    XP_ASSERT( XP_why == TIMER_DUP_TIMERCHECK );
    ServerCtxt* server = (ServerCtxt*)closure;
    XP_ASSERT( inDuplicateMode( server ) );
    // Don't call server_do() if the timer hasn't fired yet
    return setDupCheckTimer( server, xwe ) || server_do( server, xwe );
}

static XP_Bool
setDupCheckTimer( ServerCtxt* server, XWEnv xwe )
{
    XP_Bool set = XP_FALSE;
    XP_U32 now = dutil_getCurSeconds( server->vol.dutil, xwe );
    if ( server->nv.dupTimerExpires > 0 && server->nv.dupTimerExpires > now ) {
        XP_U32 diff = server->nv.dupTimerExpires - now;
        XP_ASSERT( diff <= 0x7FFF );
        XP_U16 whenSeconds = (XP_U16) diff;
        util_setTimer( server->vol.util, xwe, TIMER_DUP_TIMERCHECK,
                       whenSeconds, checkDupTimerProc, server );
        set = XP_TRUE;
    }
    return set;
}

static void
setDupTimerExpires( ServerCtxt* server, XWEnv xwe, XP_S32 newVal )
{
    XP_LOGF( "%s(%d)", __func__, newVal );
    if ( newVal != server->nv.dupTimerExpires ) {
        XP_S32 oldVal = server->nv.dupTimerExpires;
        server->nv.dupTimerExpires = newVal;
        callDupTimerListener( server, xwe, oldVal, newVal );
    }
}

static void
dupe_resetTimer( ServerCtxt* server, XWEnv xwe )
{
    XP_S32 newVal = 0;
    if ( server->vol.gi->timerEnabled && 0 < server->vol.gi->gameSeconds ) {
        XP_U32 now = dutil_getCurSeconds( server->vol.dutil, xwe );
        newVal = now + server->vol.gi->gameSeconds;
    } else {
        XP_LOGF( "%s(): doing nothing because timers disabled", __func__ );
    }

    if ( server_canUnpause( server ) ) {
        XP_U32 now = dutil_getCurSeconds( server->vol.dutil, xwe );
        newVal = -(newVal - now);
    }
    setDupTimerExpires( server, xwe, newVal );

    setDupCheckTimer( server, xwe );
}

XP_S32
server_getDupTimerExpires( const ServerCtxt* server )
{
    return server->nv.dupTimerExpires;
}

/* If we're in dup mode, this is 0 if no timer otherwise the number of seconds
   left. */
XP_S16
server_getTimerSeconds( const ServerCtxt* server, XWEnv xwe, XP_U16 turn )
{
    XP_S16 result;
    if ( inDuplicateMode( server ) ) {
        XP_S32 dupTimerExpires = server->nv.dupTimerExpires;
        if ( dupTimerExpires <= 0 ) {
            result = (XP_S16)-dupTimerExpires;
        } else {
            XP_U32 now = dutil_getCurSeconds( server->vol.dutil, xwe );
            result = dupTimerExpires > now ? dupTimerExpires - now : 0;
        }
        XP_ASSERT( result >= 0 ); /* should never go negative */
    } else {
        CurGameInfo* gi = server->vol.gi;
        XP_U16 secondsUsed = gi->players[turn].secondsUsed;
        XP_U16 secondsAvailable = gi->gameSeconds / gi->nPlayers;
        XP_ASSERT( gi->timerEnabled );
        result = secondsAvailable - secondsUsed;
    }
    return result;
}

XP_Bool
server_canPause( const ServerCtxt* server )
{
    XP_Bool result = inDuplicateMode( server )
        && 0 < server_getDupTimerExpires( server );
    /* LOG_RETURNF( "%d", result ); */
    return result;
}

XP_Bool
server_canUnpause( const ServerCtxt* server )
{
    XP_Bool result = inDuplicateMode( server )
        && 0 > server_getDupTimerExpires( server );
    /* LOG_RETURNF( "%d", result ); */
    return result;
}

static void
pauseImpl( ServerCtxt* server, XWEnv xwe )
{
    XP_ASSERT( server_canPause( server ) );
    /* Figure out how many seconds are left on the timer, and set timer to the
       negative of that (since negative is the flag) */
    XP_U32 now = dutil_getCurSeconds( server->vol.dutil, xwe );
    setDupTimerExpires( server, xwe, -(server->nv.dupTimerExpires - now) );
    XP_ASSERT( 0 > server->nv.dupTimerExpires );
    XP_ASSERT( server_canUnpause( server ) );
}

void
server_pause( ServerCtxt* server, XWEnv xwe, XP_S16 turn, const XP_UCHAR* msg )
{
    XP_LOGF( "%s(turn=%d)", __func__, turn );
    pauseImpl( server, xwe );
    /* Figure out how many seconds are left on the timer, and set timer to the
       negative of that (since negative is the flag) */
    dupe_transmitPause( server, xwe, PAUSED, turn, msg, -1 );
    model_noteDupePause( server->vol.model, xwe, PAUSED, turn, msg );
    LOG_RETURN_VOID();
}

static void
dupe_autoPause( ServerCtxt* server, XWEnv xwe )
{
    XP_LOGF( "%s()", __func__ );

    /* Reset timer: we're starting turn over */
    dupe_resetTimer( server, xwe );
    dupe_clearState( server );

    /* Then pause us */
    pauseImpl( server, xwe );

    dupe_transmitPause( server, xwe, AUTOPAUSED, 0, NULL, -1 );
    model_noteDupePause( server->vol.model, xwe, AUTOPAUSED, -1, NULL );
    LOG_RETURN_VOID();
}

void
server_unpause( ServerCtxt* server, XWEnv xwe, XP_S16 turn, const XP_UCHAR* msg )
{
    XP_LOGF( "%s(turn=%d)", __func__, turn );
    XP_ASSERT( server_canUnpause( server ) );
    XP_U32 now = dutil_getCurSeconds( server->vol.dutil, xwe );
    /* subtract because it's negative */
    setDupTimerExpires( server, xwe, now - server->nv.dupTimerExpires );
    XP_ASSERT( server_canPause( server ) );
    dupe_transmitPause( server, xwe, UNPAUSED, turn, msg, -1 );
    model_noteDupePause( server->vol.model, xwe, UNPAUSED, turn, msg );
    LOG_RETURN_VOID();
}

/* Called on client. Unpacks DUP move data and applies it. */
static XP_Bool
dupe_handleServerMoves( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* stream )
{
    LOG_FUNC();
    MoveInfo moveInfo;
    moveInfoFromStream( stream, &moveInfo, bitsPerTile(server) );
    TrayTileSet newTiles;
    traySetFromStream( stream, &newTiles );
    XP_ASSERT( newTiles.nTiles <= moveInfo.nTiles );
    XP_ASSERT( pool_containsTiles( server->pool, &newTiles ) );

    XP_U16 nScores = stream_getBits( stream, NPLAYERS_NBITS );
    XP_U16 scores[MAX_NUM_PLAYERS];
    XP_ASSERT( nScores <= MAX_NUM_PLAYERS );
    scoresFromStream( stream, nScores, scores );

    dupe_resetTimer( server, xwe );

    pool_removeTiles( server->pool, &newTiles );
    model_commitDupeTurn( server->vol.model, xwe, &moveInfo, nScores, scores,
                          &newTiles );

    /* Need to remove the played tiles from all local trays */
    updateOthersTiles( server, xwe );

    dupe_setupShowMove( server, xwe, scores );

    dupe_clearState( server );
    nextTurn( server, xwe, PICK_NEXT );
    LOG_RETURN_VOID();
    return XP_TRUE;
} /* dupe_handleServerMoves */

static XP_Bool
dupe_handleServerTrade( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* stream )
{
    TrayTileSet oldTiles, newTiles;
    traySetFromStream( stream, &oldTiles );
    traySetFromStream( stream, &newTiles );

    ModelCtxt* model = server->vol.model;
    model_resetCurrentTurn( model, xwe, DUP_PLAYER );
    model_removePlayerTiles( model, DUP_PLAYER, &oldTiles );
    pool_replaceTiles( server->pool, &oldTiles );
    pool_removeTiles( server->pool, &newTiles );

    model_commitDupeTrade( model, &oldTiles, &newTiles );

    model_addNewTiles( model, DUP_PLAYER, &newTiles );
    updateOthersTiles( server, xwe );

    dupe_resetTimer( server, xwe );
    dupe_setupShowTrade( server, xwe, newTiles.nTiles );

    dupe_clearState( server );
    nextTurn( server, xwe, PICK_NEXT );
    return XP_TRUE;
}

#endif

/* Just for grins....trade in all the tiles that weren't used in the
 * move the robot manage to make.  This is not meant to be strategy, but
 * rather to force me to make the trade-communication stuff work well.
 */
#if 0
static void
robotTradeTiles( ServerCtxt* server, MoveInfo* newMove )
{
    Tile tradeTiles[MAX_TRAY_TILES];
    XP_S16 turn = server->nv.currentTurn;
    Tile* curTiles = model_getPlayerTiles( server->model, turn );
    XP_U16 numInTray = model_getNumPlayerTiles( server->model, turn );
    XP_MEMCPY( tradeTiles, curTiles, numInTray );

    for ( ii = 0; ii < numInTray; ++ii ) { /* for each tile in tray */
        XP_Bool keep = XP_FALSE;
        for ( jj = 0; jj < newMove->numTiles; ++jj ) { /* for each in move */
            Tile movedTile = newMove->tiles[jj].tile;
            if ( newMove->tiles[jj].isBlank ) {
                movedTile |= TILE_BLANK_BIT;
            }
            if ( movedTile == curTiles[ii] ) { /* it's in the move */
                keep = XP_TRUE;
                break;
            }
        }
        if ( !keep ) {
            tradeTiles[numToTrade++] = curTiles[ii];
        }
    }

    
} /* robotTradeTiles */
#endif

static XWStreamCtxt*
mkServerStream( ServerCtxt* server )
{
    XWStreamCtxt* stream;
    stream = mem_stream_make_raw( MPPARM(server->mpool)
                                  dutil_getVTManager(server->vol.dutil) );
    XP_ASSERT( !!stream );
    return stream;
} /* mkServerStream */

static XP_Bool
makeRobotMove( ServerCtxt* server, XWEnv xwe )
{
    LOG_FUNC();
    XP_Bool result = XP_FALSE;
    XP_Bool searchComplete = XP_FALSE;
    XP_S16 turn;
    MoveInfo newMove = {0};
    ModelCtxt* model = server->vol.model;
    CurGameInfo* gi = server->vol.gi;
    XP_Bool timerEnabled = gi->timerEnabled;
    XP_Bool canMove;
    XP_U32 time = 0L; /* stupid compiler.... */
    XW_DUtilCtxt* dutil = server->vol.dutil;
    XP_Bool forceTrade = XP_FALSE;
    
    if ( timerEnabled ) {
        time = dutil_getCurSeconds( dutil, xwe );
    }

#ifdef XWFEATURE_SLOW_ROBOT
    if ( 0 != server->nv.robotTradePct ) {
        XP_ASSERT( ! inDuplicateMode( server ) );
        if ( server_countTilesInPool( server ) >= gi->traySize ) {
            XP_U16 pct = XP_RANDOM() % 100;
            forceTrade = pct < server->nv.robotTradePct ;
        }
    }
#endif

    turn = server->nv.currentTurn;
    XP_ASSERT( turn >= 0 );

    /* If the player's been recently turned into a robot while he had some
       pending tiles on the board we'll have problems.  It'd be best to detect
       this and put 'em back when that happens.  But for now we'll just be
       paranoid.  PENDING(ehouse) */
    model_resetCurrentTurn( model, xwe, turn );

    if ( !forceTrade ) {
        const TrayTileSet* tileSet = model_getPlayerTiles( model, turn );
#ifdef XWFEATURE_BONUSALL
        XP_U16 allTilesBonus = server_figureFinishBonus( server, turn );
#endif
        XP_ASSERT( !!server_getEngineFor( server, turn ) );
        searchComplete = engine_findMove( server_getEngineFor( server, turn ),
                                          xwe, model, turn, XP_FALSE, XP_FALSE,
                                          tileSet->tiles, tileSet->nTiles, XP_FALSE,
#ifdef XWFEATURE_BONUSALL
                                          allTilesBonus, 
#endif
#ifdef XWFEATURE_SEARCHLIMIT
                                          NULL, XP_FALSE,
#endif
                                          gi->players[turn].robotIQ,
                                          &canMove, &newMove, NULL );
    }
    if ( forceTrade || searchComplete ) {
        const XP_UCHAR* str;
        XWStreamCtxt* stream = NULL;

        XP_Bool trade = forceTrade || 
            ((newMove.nTiles == 0) && !canMove &&
             (server_countTilesInPool( server ) >= gi->traySize));

        server->vol.showPrevMove = XP_TRUE;
        if ( inDuplicateMode(server) || server->nv.showRobotScores ) {
            stream = mkServerStream( server );
        }

        /* trade if unable to find a move */
        if ( trade ) {
            TrayTileSet oldTiles = *model_getPlayerTiles( model, turn );
            XP_LOGFF( "robot trading %d tiles", oldTiles.nTiles );
            result = server_commitTrade( server, xwe, &oldTiles, NULL );

            /* Quick hack to fix gremlin bug where all-robot game seen none
               able to trade for tiles to move and blowing the undo stack.
               This will stop them, and should have no effect if there are any
               human players making real moves. */

            if ( !!stream ) {
                XP_UCHAR buf[64];
                XP_U16 nTrayTiles = gi->traySize;
                str = dutil_getUserQuantityString( dutil, xwe, STRD_ROBOT_TRADED,
                                                   nTrayTiles );
                XP_SNPRINTF( buf, sizeof(buf), str, nTrayTiles );

                stream_catString( stream, buf );
                XP_ASSERT( !server->nv.prevMoveStream );
                server->nv.prevMoveStream = stream;
            }
        } else { 
            /* if canMove is false, this is a fake move, a pass */

            if ( canMove || NPASSES_OK(server) ) {
#ifdef XWFEATURE_ROBOTPHONIES
                if ( server->nv.makePhonyPct > XP_RANDOM() % 100 ) {
                    reverseTiles( &newMove );
                }
#endif
                juggleMoveIfDebug( &newMove );
                model_makeTurnFromMoveInfo( model, xwe, turn, &newMove );
                XP_LOGFF( "robot making %d tile move for player %d", newMove.nTiles, turn );

                if ( !!stream ) {
                    XWStreamCtxt* wordsStream = mkServerStream( server );
                    WordNotifierInfo* ni = 
                        model_initWordCounter( model, wordsStream );
                    (void)model_checkMoveLegal( model, xwe, turn, stream, ni );
                    XP_ASSERT( !server->nv.prevMoveStream );
                    server->nv.prevMoveStream = stream;
                    server->nv.prevWordsStream = wordsStream;
                }
                result = server_commitMove( server, xwe, turn, NULL );
            } else {
                result = XP_FALSE;
            }
        }
    }

    if ( timerEnabled ) {
        gi->players[turn].secondsUsed += 
            (XP_U16)(dutil_getCurSeconds( dutil, xwe ) - time);
    } else {
        XP_ASSERT( gi->players[turn].secondsUsed == 0 );
    }

    LOG_RETURNF( "%s", boolToStr(result) );
    return result; /* always return TRUE after robot move? */
} /* makeRobotMove */

#ifdef XWFEATURE_SLOW_ROBOT
static XP_Bool 
wakeRobotProc( void* closure, XWEnv xwe, XWTimerReason XP_UNUSED_DBG(why) )
{
    XP_ASSERT( TIMER_SLOWROBOT == why );
    ServerCtxt* server = (ServerCtxt*)closure;
    XP_ASSERT( ROBOTWAITING(server) );
    server->robotWaiting = XP_FALSE;
    util_requestTime( server->vol.util, xwe );
    return XP_FALSE;
}
#endif

static XP_Bool
robotMovePending( const ServerCtxt* server )
{
    XP_Bool result = XP_FALSE;
    XP_S16 turn = server->nv.currentTurn;
    if ( turn >= 0 && tileCountsOk(server) && NPASSES_OK(server) ) {
        CurGameInfo* gi = server->vol.gi;
        LocalPlayer* player = &gi->players[turn];
        result = LP_IS_ROBOT(player) && LP_IS_LOCAL(player);
    }
    return result;
} /* robotMovePending */

#ifdef XWFEATURE_SLOW_ROBOT
static XP_Bool
postponeRobotMove( ServerCtxt* server, XWEnv xwe )
{
    XP_Bool result = XP_FALSE;
    XP_ASSERT( robotMovePending(server) );

    if ( !ROBOTWAITING(server) ) {
        XP_U16 sleepTime = figureSleepTime(server);
        if ( 0 != sleepTime ) {
            server->robotWaiting = XP_TRUE;
            util_setTimer( server->vol.util, xwe, TIMER_SLOWROBOT, sleepTime,
                           wakeRobotProc, server );
            result = XP_TRUE;
        }
    }
    return result;
}
# define POSTPONEROBOTMOVE(s, e) postponeRobotMove(s, e)
#else
# define POSTPONEROBOTMOVE(s, e) XP_FALSE
#endif

static void
showPrevScore( ServerCtxt* server, XWEnv xwe )
{
    /* showRobotScores can be changed between turns */
    if ( inDuplicateMode( server ) || server->nv.showRobotScores ) {
        XW_UtilCtxt* util = server->vol.util;
        XW_DUtilCtxt* dutil = server->vol.dutil;
        XWStreamCtxt* stream;
        XP_UCHAR buf[128];
        CurGameInfo* gi = server->vol.gi;
        XP_U16 nPlayers = gi->nPlayers;
        XP_U16 prevTurn;
        LocalPlayer* lp;

        prevTurn = (server->nv.currentTurn + nPlayers - 1) % nPlayers;
        lp = &gi->players[prevTurn];

        XP_U16 stringCode;
        if ( inDuplicateMode( server ) ) {
            stringCode = STR_DUP_MOVED;
        } else if ( LP_IS_LOCAL(lp) ) {
            stringCode = STR_ROBOT_MOVED;
        } else {
            stringCode = STRS_REMOTE_MOVED;
        }
        const XP_UCHAR* str = dutil_getUserString( dutil, xwe, stringCode );
        XP_SNPRINTF( buf, sizeof(buf), str, lp->name );
        str = buf;

        stream = mkServerStream( server );
        stream_catString( stream, str );

        XWStreamCtxt* prevStream = server->nv.prevMoveStream;
        if ( !!prevStream ) {
            server->nv.prevMoveStream = NULL;

            XP_U16 len = stream_getSize( prevStream );
            stream_putBytes( stream, stream_getPtr( prevStream ), len );
            stream_destroy( prevStream, xwe );
        }

        util_informMove( util, xwe, prevTurn, stream, server->nv.prevWordsStream );
        stream_destroy( stream, xwe );

        if ( !!server->nv.prevWordsStream ) {
            stream_destroy( server->nv.prevWordsStream, xwe );
            server->nv.prevWordsStream = NULL;
        }
    }
    SETSTATE( server, server->nv.stateAfterShow );
} /* showPrevScore */

void
server_tilesPicked( ServerCtxt* server, XWEnv xwe, XP_U16 player,
                    const TrayTileSet* newTilesP )
{
    XP_ASSERT( 0 == model_getNumTilesInTray( server->vol.model, player ) );
    XP_ASSERT( server->vol.pickTilesCalled[player] );
    server->vol.pickTilesCalled[player] = XP_FALSE;

    TrayTileSet newTiles = *newTilesP;
    pool_removeTiles( server->pool, &newTiles );

    fetchTiles( server, xwe, player, server->vol.gi->traySize,
                NULL, &newTiles, XP_FALSE );
    XP_ASSERT( !inDuplicateMode(server) );
    model_assignPlayerTiles( server->vol.model, player, &newTiles );

    util_requestTime( server->vol.util, xwe );
}

static XP_Bool
informNeedPickTiles( ServerCtxt* server, XWEnv xwe, XP_Bool initial,
                     XP_U16 turn, XP_U16 nToPick )
{
    ModelCtxt* model = server->vol.model;
    const DictionaryCtxt* dict = model_getDictionary(model);
    XP_U16 nFaces = dict_numTileFaces( dict );
    XP_U16 counts[MAX_UNIQUE_TILES];
    const XP_UCHAR* faces[MAX_UNIQUE_TILES];

    XP_U16 nLeft = pool_getNTilesLeft( server->pool );
    if ( nLeft < nToPick ) {
        nToPick = nLeft;
    }
    XP_Bool asking = nToPick > 0;

    if ( asking ) {
        /* We need to make sure we only call util_informNeedPickTiles once
           without it returning. Even if server_do() is called a lot. */
        if ( server->vol.pickTilesCalled[turn] ) {
            XP_LOGF( "%s(): already asking for %d", __func__, turn );
        } else {
            server->vol.pickTilesCalled[turn] = XP_TRUE;
            for ( Tile tile = 0; tile < nFaces; ++tile ) {
                faces[tile] = dict_getTileString( dict, tile );
                counts[tile] = pool_getNTilesLeftFor( server->pool, tile );
            }
            util_informNeedPickTiles( server->vol.util, xwe, initial, turn,
                                      nToPick, nFaces, faces, counts );
        }
    }
    return asking;
}

XP_Bool
server_do( ServerCtxt* server, XWEnv xwe )
{
    XP_Bool result = XP_TRUE;

    if ( server->serverDoing ) {
        result = XP_FALSE;
    } else {
        XP_Bool moreToDo = XP_FALSE;
        server->serverDoing = XP_TRUE;
        XP_LOGFF( "gameState: %s; gameID: %d", getStateStr(server->nv.gameState),
                  server->vol.gi->gameID );
        switch( server->nv.gameState ) {
        case XWSTATE_BEGIN:
            if ( server->nv.pendingRegistrations == 0 ) { /* all players on
                                                             device */
                if ( assignTilesToAll( server, xwe ) ) {
                    SETSTATE( server, XWSTATE_INTURN );
                    setTurn( server, xwe, 0 );
                    if ( inDuplicateMode( server ) ) {
                        dupe_resetTimer( server, xwe );
                    }
                    moreToDo = XP_TRUE;
                }
            }
            break;

        case XWSTATE_NEWCLIENT:
            XP_ASSERT( !amServer( server ) );
            SETSTATE( server, XWSTATE_NONE ); /* server_initClientConnection expects this */
            server_initClientConnection( server, xwe );
            break;

        case XWSTATE_NEEDSEND_BADWORD_INFO:
            XP_ASSERT( server->vol.gi->serverRole == SERVER_ISSERVER );
            badWordMoveUndoAndTellUser( server, xwe, &server->illegalWordInfo );
#ifndef XWFEATURE_STANDALONE_ONLY
            sendBadWordMsgs( server, xwe );
#endif
            nextTurn( server, xwe, PICK_NEXT );
            //moreToDo = XP_TRUE;   /* why? */
            break;

#ifndef XWFEATURE_STANDALONE_ONLY
        case XWSTATE_RECEIVED_ALL_REG:
            sendInitialMessage( server, xwe );
            /* PENDING isn't INTURN_OFFDEVICE possible too?  Or just
               INTURN?  */
            SETSTATE( server, XWSTATE_INTURN );
            setTurn( server, xwe, 0 );
            moreToDo = XP_TRUE;
            break;

        case XWSTATE_MOVE_CONFIRM_MUSTSEND:
            XP_ASSERT( server->vol.gi->serverRole == SERVER_ISSERVER );
            tellMoveWasLegal( server, xwe ); /* sets state */
            nextTurn( server, xwe, PICK_NEXT );
            break;

#endif /* XWFEATURE_STANDALONE_ONLY */

        case XWSTATE_NEEDSEND_ENDGAME:
            endGameInternal( server, xwe, END_REASON_OUT_OF_TILES, -1 );
            break;

        case XWSTATE_NEED_SHOWSCORE:
            showPrevScore( server, xwe );
            /* state better have changed or we'll infinite loop... */
            XP_ASSERT( XWSTATE_NEED_SHOWSCORE != server->nv.gameState );
            /* either process turn or end game should come next... */
            moreToDo = XWSTATE_NEED_SHOWSCORE != server->nv.gameState;
            break;
        case XWSTATE_INTURN:
            if ( inDuplicateMode( server ) ) {
                /* For now, anyway; makes dev easier */
                dupe_forceCommits( server, xwe );
                dupe_checkTurns( server, xwe );
            }

            if ( robotMovePending( server ) && !ROBOTWAITING(server) ) {
                result = makeRobotMove( server, xwe );
                /* if robot was interrupted, we need to schedule again */
                moreToDo = !result || 
                    (robotMovePending( server ) && !POSTPONEROBOTMOVE(server, xwe));
            }
            break;

        default:
            result = XP_FALSE;
            break;
        } /* switch */

        if ( moreToDo ) {
            util_requestTime( server->vol.util, xwe );
        }

        server->serverDoing = XP_FALSE;
    }
    return result;
} /* server_do */

#ifndef XWFEATURE_STANDALONE_ONLY

static XP_S8
getIndexForStream( const ServerCtxt* server, const XWStreamCtxt* stream )
{
    XP_PlayerAddr channelNo = stream_getAddress( stream );
    return getIndexForDevice( server, channelNo );
}

static XP_S8
getIndexForDevice( const ServerCtxt* server, XP_PlayerAddr channelNo )
{
    short ii;
    XP_S8 result = -1;

    for ( ii = 0; ii < server->nv.nDevices; ++ii ) {
        const RemoteAddress* addr = &server->nv.addresses[ii];
        if ( addr->channelNo == channelNo ) {
            result = ii;
            break;
        }
    }

    XP_LOGF( "%s(%x)=>%d", __func__, channelNo, result );
    return result;
} /* getIndexForDevice */

static LocalPlayer* 
findFirstPending( ServerCtxt* server, ServerPlayer** playerP )
{
    LocalPlayer* lp;
    CurGameInfo* gi = server->vol.gi;
    XP_U16 nPlayers = gi->nPlayers;
    XP_U16 nPending = server->nv.pendingRegistrations;

    XP_ASSERT( nPlayers > 0 );
    lp = gi->players + nPlayers;

    while ( --lp >= gi->players ) {
        --nPlayers;
        if ( !lp->isLocal ) {
            if ( --nPending == 0 ) {
                break;
            }
        }
    }
    if ( lp < gi->players ) { /* did we find a slot? */
        XP_LOGF( "%s: no slot found for player; duplicate packet?", __func__ );
        lp = NULL;
    } else {
        *playerP = server->players + nPlayers;
    }
    return lp;
} /* findFirstPending */

static XP_S8
registerRemotePlayer( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* stream )
{
    XP_S8 deviceIndex = -1;
    XP_PlayerAddr channelNo;
    XP_U16 nameLen;
    LocalPlayer* lp;
    ServerPlayer* player = (ServerPlayer*)NULL;

    /* The player must already be there with a null name, or it's an error.
       Take the first empty slot. */
    XP_ASSERT( server->nv.pendingRegistrations > 0 );

    /* find the slot to use */
    lp = findFirstPending( server, &player );
    if ( NULL != lp ) {

        /* get data from stream */
        lp->robotIQ = 1 == stream_getBits( stream, 1 )? 1 : 0;
        nameLen = stream_getBits( stream, NAME_LEN_NBITS );
        XP_UCHAR name[nameLen + 1];
        stream_getBytes( stream, name, nameLen );
        name[nameLen] = '\0';
        XP_LOGFF( "read remote name: %s", name );

        replaceStringIfDifferent( server->mpool, &lp->name, name );

        channelNo = stream_getAddress( stream );
        deviceIndex = getIndexForDevice( server, channelNo );

        --server->nv.pendingRegistrations;

        if ( deviceIndex == -1 ) {
            RemoteAddress* addr = &server->nv.addresses[server->nv.nDevices];

            XP_ASSERT( channelNo != 0 );
            addr->channelNo = channelNo;
            XP_LOGFF( "set channelNo to %x for device %d",
                     channelNo, server->nv.nDevices );

            deviceIndex = server->nv.nDevices++;
#ifdef STREAM_VERS_BIGBOARD
            addr->streamVersion = STREAM_SAVE_PREVWORDS;
#endif
        } else {
            XP_LOGF( "%s: deviceIndex already set", __func__ );
        }

        player->deviceIndex = deviceIndex;

        informMissing( server, xwe );
    }
    return deviceIndex;
} /* registerRemotePlayer */
#endif

static void
sortTilesIf( ServerCtxt* server, XP_S16 turn )
{
    if ( server->nv.sortNewTiles ) {
        model_sortTiles( server->vol.model, turn );
    }
}

#ifndef XWFEATURE_STANDALONE_ONLY
/* Called in response to message from server listing all the names of
 * players in the game (in server-assigned order) and their initial
 * tray contents.
 */
static XP_Bool
client_readInitialMessage( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* stream )
{
    LOG_FUNC();
    XP_Bool accepted = 0 == server->nv.addresses[0].channelNo;

    /* We should never get this message a second time, but very rarely we do.
       Drop it in that case. */
    if ( accepted ) {
        ModelCtxt* model = server->vol.model;
        CurGameInfo* gi = server->vol.gi;
        CurGameInfo localGI;
        XP_U32 gameID;
        PoolContext* pool;
#ifdef STREAM_VERS_BIGBOARD
        XP_UCHAR rmtDictName[128];
        XP_UCHAR rmtDictSum[64];
#endif

        /* version; any dependencies here? */
        XP_U8 streamVersion = stream_getU8( stream );
        XP_LOGF( "%s: set streamVersion to %d", __func__, streamVersion );
        stream_setVersion( stream, streamVersion );
        if ( STREAM_VERS_NINETILES > streamVersion ) {
            model_forceStack7Tiles( server->vol.model );
        }
        // XP_ASSERT( streamVersion <= CUR_STREAM_VERS ); /* else do what? */

        gameID = stream_getU32( stream );
        XP_LOGFF( "read gameID of %x/%d; calling comms_setConnID (replacing %d)",
                  gameID, gameID, server->vol.gi->gameID );
        server->vol.gi->gameID = gameID;
        comms_setConnID( server->vol.comms, gameID );

        XP_MEMSET( &localGI, 0, sizeof(localGI) );
        gi_readFromStream( MPPARM(server->mpool) stream, &localGI );
        localGI.serverRole = SERVER_ISCLIENT;

        localGI.dictName = copyString( server->mpool, gi->dictName );
        gi_copy( MPPARM(server->mpool) gi, &localGI );

        if ( streamVersion < STREAM_VERS_NOEMPTYDICT ) {
            XP_LOGFF( "loading and dropping empty dict" );
            DictionaryCtxt* empty = util_makeEmptyDict( server->vol.util, xwe );
            dict_loadFromStream( empty, xwe, stream );
            dict_unref( empty, xwe );
        }

#ifdef STREAM_VERS_BIGBOARD
        if ( STREAM_VERS_DICTNAME <= streamVersion ) {
            stringFromStreamHere( stream, rmtDictName, VSIZE(rmtDictName) );
            stringFromStreamHere( stream, rmtDictSum, VSIZE(rmtDictSum) );
        } else {
            rmtDictName[0] = '\0';
        }
#endif

        XP_PlayerAddr channelNo = stream_getAddress( stream );
        XP_ASSERT( channelNo != 0 );
        server->nv.addresses[0].channelNo = channelNo;
        XP_LOGF( "%s: assigning channelNo %x for 0", __func__, channelNo );

        model_setSize( model, localGI.boardSize );

        XP_U16 nPlayers = localGI.nPlayers;
        XP_LOGF( "%s: reading in %d players", __func__, localGI.nPlayers );

        gi->nPlayers = nPlayers;
        model_setNPlayers( model, nPlayers );

        const DictionaryCtxt* curDict = model_getDictionary( model );

        if ( curDict == NULL ) {
            XP_ASSERT(0);
        } else {
            /* keep the dict the local user installed */
#ifdef STREAM_VERS_BIGBOARD
            if ( '\0' != rmtDictName[0] ) {
                const XP_UCHAR* ourName = dict_getShortName( curDict );
                util_informNetDict( server->vol.util, xwe,
                                    dict_getISOCode( curDict ),
                                    ourName, rmtDictName,
                                    rmtDictSum, localGI.phoniesAction );
            }
#endif
        }

        gi_disposePlayerInfo( MPPARM(server->mpool) &localGI );

        XP_ASSERT( !server->pool );
        makePoolOnce( server );
        pool = server->pool;

        /* now read the assigned tiles for each player from the stream, and
           remove them from the newly-created local pool. */
        TrayTileSet tiles;
        for ( XP_U16 ii = 0; ii < nPlayers; ++ii ) {

            /* Pull/remove only once if duplicate-mode game */
            if ( ii == 0 || !inDuplicateMode(server) ) {
                traySetFromStream( stream, &tiles );
                XP_ASSERT( tiles.nTiles <= MAX_TRAY_TILES );
                /* remove what the server's assigned so we won't conflict
                   later. */
                pool_removeTiles( pool, &tiles );
            }
            XP_LOGF( "%s: got %d tiles for player %d", __func__, tiles.nTiles, ii );

            if ( inDuplicateMode(server ) ) {
                model_assignDupeTiles( model, xwe, &tiles );
                break;
            } else {
                model_assignPlayerTiles( model, ii, &tiles );
            }

            sortTilesIf( server, ii );
        }

        readMQTTDevID( server, stream );

        syncPlayers( server );

        SETSTATE( server, XWSTATE_INTURN );

        /* Give board a chance to redraw self with the full compliment of known
           players */
        informMissing( server, xwe );
        setTurn( server, xwe, 0 );
        dupe_resetTimer( server, xwe );
    } else {
        XP_LOGF( "%s: wanted 0; got %d", __func__, 
                 server->nv.addresses[0].channelNo );
    }
    return accepted;
} /* client_readInitialMessage */
#endif

/* For each remote device, send a message containing the dictionary and the
 * names of all the players in the game (including those on the device itself,
 * since they might have been changed in the case of conflicts), in the order
 * that all must use for the game.  Then for each player on the device give
 * the starting tray.
 */
#ifndef XWFEATURE_STANDALONE_ONLY

static void
makeSendableGICopy( ServerCtxt* server, CurGameInfo* giCopy, 
                    XP_U16 deviceIndex )
{
    XP_U16 nPlayers;
    LocalPlayer* clientPl;
    XP_U16 ii;

    XP_MEMCPY( giCopy, server->vol.gi, sizeof(*giCopy) );

    nPlayers = giCopy->nPlayers;

    for ( clientPl = giCopy->players, ii = 0;
          ii < nPlayers; ++clientPl, ++ii ) {
        /* adjust isLocal to client's perspective */
        clientPl->isLocal = server->players[ii].deviceIndex == deviceIndex;
    }

    giCopy->forceChannel = deviceIndex;
    XP_LOGF( "%s: assigning forceChannel from deviceIndex: %d", __func__, 
             giCopy->forceChannel );

    giCopy->dictName = (XP_UCHAR*)NULL; /* so we don't sent the bytes */
} /* makeSendableGICopy */

static void
sendInitialMessage( ServerCtxt* server, XWEnv xwe )
{
    ModelCtxt* model = server->vol.model;
    XP_U16 nPlayers = server->vol.gi->nPlayers;
    XP_U32 gameID = server->vol.gi->gameID;
#ifdef STREAM_VERS_BIGBOARD
    XP_U8 streamVersion = server->nv.streamVersion;
#endif

    XP_ASSERT( server->nv.nDevices > 1 );
    for ( XP_U16 deviceIndex = 1; deviceIndex < server->nv.nDevices;
          ++deviceIndex ) {
        XWStreamCtxt* stream = messageStreamWithHeader( server, xwe, deviceIndex,
                                                        XWPROTO_CLIENT_SETUP );
        XP_ASSERT( !!stream );

#ifdef STREAM_VERS_BIGBOARD
        XP_ASSERT( 0 < streamVersion );
        stream_putU8( stream, streamVersion );
#else
        stream_putU8( stream, CUR_STREAM_VERS );
#endif

        XP_LOGF( "putting gameID %x into msg", gameID );
        stream_putU32( stream, gameID );

        CurGameInfo localGI;
        makeSendableGICopy( server, &localGI, deviceIndex );
        gi_writeToStream( stream, &localGI );

        const DictionaryCtxt* dict = model_getDictionary( model );
        if ( streamVersion < STREAM_VERS_NOEMPTYDICT ) {
            XP_LOGFF( "writing dict to stream" );
            dict_writeToStream( dict, stream );
        }
#ifdef STREAM_VERS_BIGBOARD
        if ( STREAM_VERS_DICTNAME <= streamVersion ) {
            stringToStream( stream, dict_getShortName(dict) );
            stringToStream( stream, dict_getMd5Sum(dict) );
        }
#endif
        /* send tiles currently in tray */
        for ( XP_U16 ii = 0; ii < nPlayers; ++ii ) {
            model_trayToStream( model, ii, stream );
            if ( inDuplicateMode(server) ) {
                break;
            }
        }

        addMQTTDevID( server, xwe, stream );

        stream_destroy( stream, xwe );
    }

    /* Set after messages are built so their connID will be 0, but all
       non-initial messages will have a non-0 connID. */
    comms_setConnID( server->vol.comms, gameID );

    dupe_resetTimer( server, xwe );
} /* sendInitialMessage */
#endif

static void
freeBWI( MPFORMAL BadWordInfo* bwi )
{
    XP_U16 nWords = bwi->nWords;

    XP_FREEP( mpool, &bwi->dictName );
    while ( nWords-- ) {
        XP_FREEP( mpool, &bwi->words[nWords] );
    }

    bwi->nWords = 0;
} /* freeBWI */

#ifndef XWFEATURE_STANDALONE_ONLY
static void
bwiToStream( XWStreamCtxt* stream, BadWordInfo* bwi )
{
    XP_U16 nWords = bwi->nWords;
    const XP_UCHAR** sp;

    stream_putBits( stream, 4, nWords );
    if ( STREAM_VERS_DICTNAME <= stream_getVersion( stream ) ) {
        stringToStream( stream, bwi->dictName );
    }
    for ( sp = bwi->words; nWords > 0; --nWords, ++sp ) {
        stringToStream( stream, *sp );
    }

} /* bwiToStream */

static void
bwiFromStream( MPFORMAL XWStreamCtxt* stream, BadWordInfo* bwi )
{
    XP_U16 nWords = stream_getBits( stream, 4 );
    XP_ASSERT( nWords < VSIZE(bwi->words) - 1 );

    bwi->nWords = nWords;
    bwi->dictName = ( STREAM_VERS_DICTNAME <= stream_getVersion( stream ) )
        ? stringFromStream( mpool, stream ) : NULL;
    for ( int ii = 0; ii < nWords; ++ii ) {
        bwi->words[ii] = (const XP_UCHAR*)stringFromStream( mpool, stream );
    }
    bwi->words[nWords] = NULL;
} /* bwiFromStream */

#ifdef DEBUG
#define caseStr(s) case s: str = #s; break;
static const char*
codeToStr( XW_Proto code )
{
    const char* str = (char*)NULL;

    switch ( code ) {
        caseStr( XWPROTO_ERROR );
        caseStr( XWPROTO_CHAT );
        caseStr( XWPROTO_DEVICE_REGISTRATION );
        caseStr( XWPROTO_CLIENT_SETUP );
        caseStr( XWPROTO_MOVEMADE_INFO_CLIENT );
        caseStr( XWPROTO_MOVEMADE_INFO_SERVER );
        caseStr( XWPROTO_UNDO_INFO_CLIENT );
        caseStr( XWPROTO_UNDO_INFO_SERVER );
        caseStr( XWPROTO_BADWORD_INFO );
        caseStr( XWPROTO_MOVE_CONFIRM );
        caseStr( XWPROTO_CLIENT_REQ_END_GAME );
        caseStr( XWPROTO_END_GAME );
        caseStr( XWPROTO_NEW_PROTO );
        caseStr( XWPROTO_DUPE_STUFF );
    }
    return str;
} /* codeToStr */

#define PRINTCODE( intro, code ) \
    XP_STATUSF( "\t%s(): %s for %s", __func__, intro, codeToStr(code) )

#undef caseStr
#else
#define PRINTCODE(intro, code)
#endif

static XWStreamCtxt*
messageStreamWithHeader( ServerCtxt* server, XWEnv xwe, XP_U16 devIndex, XW_Proto code )
{
    XWStreamCtxt* stream;
    XP_PlayerAddr channelNo = server->nv.addresses[devIndex].channelNo;

    PRINTCODE( "making", code );

    stream = util_makeStreamFromAddr( server->vol.util, xwe, channelNo );
    stream_open( stream );
    writeProto( server, stream, code );

    return stream;
} /* messageStreamWithHeader */

static void
sendBadWordMsgs( ServerCtxt* server, XWEnv xwe )
{
    XP_ASSERT( server->illegalWordInfo.nWords > 0 );

    if ( server->illegalWordInfo.nWords > 0 ) { /* fail gracefully */
        XWStreamCtxt* stream = 
            messageStreamWithHeader( server, xwe, server->lastMoveSource,
                                     XWPROTO_BADWORD_INFO );
        stream_putBits( stream, PLAYERNUM_NBITS, server->nv.currentTurn );

        bwiToStream( stream, &server->illegalWordInfo );

        /* XP_U32 hash = model_getHash( server->vol.model ); */
        /* stream_putU32( stream, hash ); */
        /* XP_LOGFF( "wrote hash: %X", hash ); */

        stream_destroy( stream, xwe );

        freeBWI( MPPARM(server->mpool) &server->illegalWordInfo );
    }
    SETSTATE( server, XWSTATE_INTURN );
} /* sendBadWordMsgs */
#endif

static void
badWordMoveUndoAndTellUser( ServerCtxt* server, XWEnv xwe, BadWordInfo* bwi )
{
    XP_U16 turn;
    ModelCtxt* model = server->vol.model;
    /* I'm the server.  I need to send a message to everybody else telling
       them the move's rejected.  Then undo it on this side, replacing it with
       model_commitRejectedPhony(); */

    model_rejectPreviousMove( model, xwe, server->pool, &turn );

    util_notifyIllegalWords( server->vol.util, xwe, bwi, turn, XP_TRUE );
} /* badWordMoveUndoAndTellUser */

EngineCtxt*
server_getEngineFor( ServerCtxt* server, XP_U16 playerNum )
{
    const CurGameInfo* gi = server->vol.gi;
    XP_ASSERT( playerNum < gi->nPlayers );

    ServerPlayer* player = &server->players[playerNum];
    EngineCtxt* engine = player->engine;
    if ( !engine &&
         (inDuplicateMode(server) || gi->players[playerNum].isLocal) ) {
        engine = engine_make( MPPARM(server->mpool)
                              server->vol.util );
        player->engine = engine;
    }

    return engine;
} /* server_getEngineFor */

void
server_resetEngine( ServerCtxt* server, XP_U16 playerNum )
{
    ServerPlayer* player = &server->players[playerNum];
    if ( !!player->engine ) {
        XP_ASSERT( player->deviceIndex == 0 || inDuplicateMode(server) );
        engine_reset( player->engine );
    }
} /* server_resetEngine */

void
server_resetEngines( ServerCtxt* server )
{
    XP_U16 nPlayers = server->vol.gi->nPlayers;
    for ( XP_U16 ii = 0; ii < nPlayers; ++ii ) {
        server_resetEngine( server, ii );
    }
} /* resetEngines */

#ifdef TEST_ROBOT_TRADE
static void
makeNotAVowel( ServerCtxt* server, Tile* newTile )
{
    char face[4];
    Tile tile = *newTile;
    PoolContext* pool = server->pool;
    TrayTileSet set;
    const DictionaryCtxt* dict = model_getDictionary( server->vol.model );
    XP_U8 numGot = 1;

    set.nTiles = 1;

    for ( ; ; ) {

        XP_U16 len = dict_tilesToString( dict, &tile, 1, face );

        if ( len == 1 ) {
            switch ( face[0] ) {
            case 'A':
            case 'E':
            case 'I':
            case 'O':
            case 'U':
            case '_':
                break;
            default:
                *newTile = tile;
                return;
            }
        }

        set.tiles[0] = tile;
        pool_replaceTiles( pool, &set );

        pool_requestTiles( pool, &tile, &numGot );
    }

} /* makeNotAVowel */
#endif

static void
curTrayAsTexts( ServerCtxt* server, XP_U16 turn, const TrayTileSet* notInTray,
                XP_U16* nUsedP, const XP_UCHAR** curTrayText )
{
    const TrayTileSet* tileSet = model_getPlayerTiles( server->vol.model, turn );
    const DictionaryCtxt* dict = model_getDictionary( server->vol.model );
    XP_U16 ii, jj;
    XP_U16 size = tileSet->nTiles;
    const Tile* tp = tileSet->tiles;
    XP_U16 tradedTiles[MAX_TRAY_TILES];
    XP_U16 nNotInTray = 0;
    XP_U16 nUsed = 0;

    XP_MEMSET( tradedTiles, 0xFF, sizeof(tradedTiles) );
    if ( !!notInTray ) {
        const Tile* tp = notInTray->tiles;
        nNotInTray = notInTray->nTiles;
        for ( ii = 0; ii < nNotInTray; ++ii ) {
            tradedTiles[ii] = *tp++;
        }
    }

    for ( ii = 0; ii < size; ++ii ) {
        Tile tile = *tp++;
        XP_Bool toBeTraded = XP_FALSE;

        for ( jj = 0; jj < nNotInTray; ++jj ) {
            if ( tradedTiles[jj] == tile ) {
                tradedTiles[jj] = 0xFFFF;
                toBeTraded = XP_TRUE;
                break;
            }
        }

        if ( !toBeTraded ) {
            curTrayText[nUsed++] = dict_getTileString( dict, tile );
        }
    }
    *nUsedP = nUsed;
} /* curTrayAsTexts */

/**
 * Return true (after calling util_informPickTiles()) IFF allowPickTiles is
 * TRUE and the tile set passed in is NULL.  If it doesn't contain as many
 * tiles as are needed that's cool: server code will later interpret that as
 * meaning the remainder should be assigned randomly as usual.
 */
XP_Bool
server_askPickTiles( ServerCtxt* server, XWEnv xwe, XP_U16 turn,
                     TrayTileSet* newTiles, XP_U16 nToPick )
{
    /* May want to allow the host to pick tiles even in duplicate mode. Not
       sure how that'll work! PENDING */
    XP_Bool asked = newTiles == NULL && !inDuplicateMode(server)
        && server->vol.gi->allowPickTiles;
    if ( asked ) {
        asked = informNeedPickTiles( server, xwe, XP_FALSE, turn, nToPick );
    }
    return asked;
}

/* dupe_trayAllowsMoves()
 *
 * Assuming a model with a turn loaded (but maybe not committed), build the
 * tile set containing the current model tray tiles PLUS the new set we're
 * considering, and see if the engine can find moves.
 */
static XP_Bool
dupe_trayAllowsMoves( ServerCtxt* server, XWEnv xwe, XP_U16 turn,
                      const Tile* tiles, XP_U16 nTiles )
{
    ModelCtxt* model = server->vol.model;
    XP_U16 nInTray = model_getNumTilesInTray( model, turn );
    XP_LOGF( "%s(nTiles=%d): nInTray: %d", __func__, nTiles, nInTray );
    XP_ASSERT( nInTray + nTiles <= MAX_TRAY_TILES ); /* fired! */
    Tile tmpTiles[MAX_TRAY_TILES];
    const TrayTileSet* tray = model_getPlayerTiles( model, turn );
    XP_MEMCPY( tmpTiles, &tray->tiles[0], nInTray * sizeof(tmpTiles[0]) );
    XP_MEMCPY( &tmpTiles[nInTray], &tiles[0], nTiles * sizeof(tmpTiles[0]) );

    /* XP_LOGF( "%s(nTiles=%d)", __func__, nTiles ); */
    EngineCtxt* tmpEngine = NULL;
    EngineCtxt* engine = server_getEngineFor( server, turn );
    if ( !engine ) {
        tmpEngine = engine = engine_make( MPPARM(server->mpool) server->vol.util );
    }
    XP_Bool canMove;
    MoveInfo newMove = {0};
    XP_U16 score = 0;
    XP_Bool result = engine_findMove( engine, xwe, server->vol.model, turn,
                                      XP_TRUE, XP_TRUE,
                                      tmpTiles, nTiles + nInTray, XP_FALSE, 0,
#ifdef XWFEATURE_SEARCHLIMIT
                                      NULL, XP_FALSE,
#endif
                                      0, /* not a robot */
                                      &canMove, &newMove, &score )
        && canMove;

    if ( result ) {
        XP_LOGF( "%s(): first move found has score of %d", __func__, score );
    } else {
        XP_LOGF( "%s(): no moves found for tray!!!", __func__ );
    }

    if ( !!tmpEngine ) {
        engine_destroy( tmpEngine );
    } else {
        server_resetEngine( server, turn );
    }

    return result;
}

/* Get tiles for one user.  If picking is available, let user pick until
 * cancels.  Otherwise, and after cancel, pick for 'im.
 */
static void
fetchTiles( ServerCtxt* server, XWEnv xwe, XP_U16 playerNum, XP_U16 nToFetch,
            const TrayTileSet* tradedTiles, TrayTileSet* resultTiles,
            XP_Bool forceCanPlay /* First player shouldn't have unplayable rack*/ )
{
    XP_ASSERT( server->vol.gi->serverRole != SERVER_ISCLIENT || !inDuplicateMode(server) );
    XP_Bool ask;
    XP_U16 nSoFar = resultTiles->nTiles;
    PoolContext* pool = server->pool;
    const CurGameInfo* gi = server->vol.gi;
    const XP_UCHAR* curTray[gi->traySize];
#ifdef FEATURE_TRAY_EDIT
    const DictionaryCtxt* dict = model_getDictionary( server->vol.model );
#endif

    XP_ASSERT( !!pool );
#ifdef FEATURE_TRAY_EDIT
    ask = gi->allowPickTiles
        && !LP_IS_ROBOT(&gi->players[playerNum]);
#else
    ask = XP_FALSE;
#endif
    
    XP_U16 nLeftInPool = pool_getNTilesLeft( pool );
    if ( nLeftInPool < nToFetch ) {
        nToFetch = nLeftInPool;
    }

    TrayTileSet oneTile = {.nTiles = 1};
    PickInfo pi = { .nTotal = nToFetch,
                    .thisPick = 0,
                    .curTiles = curTray,
    };

    curTrayAsTexts( server, playerNum, tradedTiles, &pi.nCurTiles, curTray );

#ifdef FEATURE_TRAY_EDIT        /* good compiler would note ask==0, but... */
    /* First ask until cancelled */
    while ( ask && nSoFar < nToFetch ) {
        XP_ASSERT( !inDuplicateMode(server) );
        const XP_UCHAR* texts[MAX_UNIQUE_TILES];
        Tile tiles[MAX_UNIQUE_TILES];
        XP_S16 chosen;
        XP_U16 nUsed = MAX_UNIQUE_TILES;
        // XP_ASSERT(0);           /* should no longer happen!!! */
        model_packTilesUtil( server->vol.model, pool,
                             XP_TRUE, &nUsed, texts, tiles );

        chosen = PICKER_PICKALL; /*util_userPickTileTray( server->vol.util,
                                   &pi, playerNum, texts, nUsed );*/

        if ( chosen == PICKER_PICKALL ) {
            ask = XP_FALSE;
        } else if ( chosen == PICKER_BACKUP ) {
            if ( nSoFar > 0 ) {
                TrayTileSet tiles;
                tiles.nTiles = 1;
                tiles.tiles[0] = resultTiles->tiles[--nSoFar];
                pool_replaceTiles( pool, &tiles );
                --pi.nCurTiles;
                --pi.thisPick;
            }
        } else {
            Tile tile = tiles[chosen];
            oneTile.tiles[0] = tile;
            pool_removeTiles( pool, &oneTile );
            curTray[pi.nCurTiles++] = dict_getTileString( dict, tile );
            resultTiles->tiles[nSoFar++] = tile;
            ++pi.thisPick;
        }
    }
#endif

    /* Then fetch the rest without asking. But if we're in duplicate mode,
       make sure the tray allows some moves (e.g. isn't all consonants when
       the board's empty.) */
    XP_ASSERT( nToFetch >= nSoFar );
    XP_U16 nLeft = nToFetch - nSoFar;
    for ( XP_U16 nBadTrays = 0; 0 < nLeft; ) {
        pool_requestTiles( pool, &resultTiles->tiles[nSoFar], &nLeft );

        if ( !inDuplicateMode( server ) && !forceCanPlay ) {
            break;
        } else if ( dupe_trayAllowsMoves( server, xwe, playerNum, &resultTiles->tiles[0],
                                          nSoFar + nLeft )
                    || ++nBadTrays >= 5 ) {
            break;
        }
        pool_replaceTiles2( pool, nLeft, &resultTiles->tiles[nSoFar] );
    }

    nSoFar += nLeft;
   
    XP_ASSERT( nSoFar < 0x00FF );
    resultTiles->nTiles = (XP_U8)nSoFar;
} /* fetchTiles */

static void
makePoolOnce( ServerCtxt* server )
{
    ModelCtxt* model = server->vol.model;
    XP_ASSERT( model_getDictionary(model) != NULL );
    if ( server->pool == NULL ) {
        server->pool = pool_make( MPPARM_NOCOMMA(server->mpool) );
        XP_STATUSF( "%s(): initing pool", __func__ );
        pool_initFromDict( server->pool, model_getDictionary(model),
                           server->vol.gi->boardSize );
    }
}

static XP_Bool
assignTilesToAll( ServerCtxt* server, XWEnv xwe )
{
    LOG_FUNC();
    XP_Bool allDone = XP_TRUE;
    XP_U16 numAssigned;
    XP_U16 ii;
    ModelCtxt* model = server->vol.model;
    CurGameInfo* gi = server->vol.gi;
    XP_U16 nPlayers = gi->nPlayers;

    XP_ASSERT( gi->serverRole != SERVER_ISCLIENT );
    makePoolOnce( server );

    XP_STATUSF( "assignTilesToAll" );

    model_setNPlayers( model, nPlayers );

    numAssigned = pool_getNTilesLeft( server->pool ) / nPlayers;
    if ( numAssigned > gi->traySize ) {
        numAssigned = gi->traySize;
    }

    /* Loop through all the players. If picking tiles is on, stop for each
       local player that doesn't have tiles yet. Return TRUE if we get all the
       way through without doing that. */

    XP_Bool pickingTiles = gi->serverRole == SERVER_STANDALONE
        && gi->allowPickTiles;
    TrayTileSet newTiles;
    for ( ii = 0; ii < nPlayers; ++ii ) {
        if ( 0 == model_getNumTilesInTray( model, ii ) ) {
            if ( pickingTiles && !LP_IS_ROBOT(&gi->players[ii])
                 && informNeedPickTiles( server, xwe, XP_TRUE, ii,
                                         gi->traySize ) ) {
                allDone = XP_FALSE;
                break;
            }
            if ( 0 == ii || !gi->inDuplicateMode ) {
                newTiles.nTiles = 0;
                fetchTiles( server, xwe, ii, numAssigned, NULL, &newTiles, ii == 0 );
            }

            if ( gi->inDuplicateMode ) {
                XP_ASSERT( ii == DUP_PLAYER );
                model_assignDupeTiles( model, xwe, &newTiles );
                break;
            } else {
                model_assignPlayerTiles( model, ii, &newTiles );
            }
        }
        sortTilesIf( server, ii );
    }
    LOG_RETURNF( "%d", allDone );
    return allDone;
} /* assignTilesToAll */

#ifndef XWFEATURE_STANDALONE_ONLY
static void
getPlayerTime( ServerCtxt* server, XWStreamCtxt* stream, XP_U16 turn )
{
    CurGameInfo* gi = server->vol.gi;

    if ( gi->timerEnabled ) {
        XP_U16 secondsUsed = stream_getU16( stream );

        gi->players[turn].secondsUsed = secondsUsed;
    }
} /* getPlayerTime */
#endif

static void
nextTurn( ServerCtxt* server, XWEnv xwe, XP_S16 nxtTurn )
{
    XP_LOGFF( "(nxtTurn=%d)", nxtTurn );
    CurGameInfo* gi = server->vol.gi;
    XP_S16 currentTurn = server->nv.currentTurn;
    XP_Bool moreToDo = XP_FALSE;

    if ( nxtTurn == PICK_CUR ) {
        nxtTurn = model_getNextTurn( server->vol.model );
    } else if ( nxtTurn == PICK_NEXT ) {
        XP_ASSERT( server->nv.gameState == XWSTATE_INTURN );
        if ( server->nv.gameState != XWSTATE_INTURN ) {
            XP_LOGFF( "doing nothing; state %s != XWSTATE_INTURN",
                      getStateStr(server->nv.gameState) );
            XP_ASSERT( !moreToDo );
            goto exit;
        } else if ( currentTurn >= 0 ) {
            if ( inDuplicateMode(server) ) {
                nxtTurn = dupe_nextTurn( server );
            } else {
                nxtTurn = model_getNextTurn( server->vol.model );
            }
        } else {
            XP_LOGFF( "turn == -1 so dropping" );
        }
    } else {
        /* We're doing an undo, and so won't bother figuring out who the
           previous turn was or how many tiles he had: it's a sure thing he
           "has" enough to be allowed to take the turn just undone. */
        XP_ASSERT( nxtTurn == model_getNextTurn( server->vol.model ) );
    }
    XP_Bool playerTilesLeft = tileCountsOk( server );
    SETSTATE( server, XWSTATE_INTURN ); /* even if game over, if undoing */

    if ( playerTilesLeft && NPASSES_OK(server) ){
        setTurn( server, xwe, nxtTurn );
    } else {
        /* I discover that the game should end.  If I'm the client,
           though, should I wait for the server to deduce this and send
           out a message?  I think so.  Yes, but be sure not to compute
           another PASS move.  Just don't do anything! */
        if ( gi->serverRole != SERVER_ISCLIENT ) {
            SETSTATE( server, XWSTATE_NEEDSEND_ENDGAME ); /* this is it */
            moreToDo = XP_TRUE;
        } else if ( currentTurn >= 0 ) {
            XP_LOGFF( "Doing nothing; waiting for server to end game" );
            setTurn( server, xwe, -1 );
            /* I'm the client. Do ++nothing++. */
        }
    }

    if ( server->vol.showPrevMove ) {
        server->vol.showPrevMove = XP_FALSE;
        if ( inDuplicateMode(server) || server->nv.showRobotScores ) {
            server->nv.stateAfterShow = server->nv.gameState;
            SETSTATE( server, XWSTATE_NEED_SHOWSCORE );
            moreToDo = XP_TRUE;
        }
    }

    /* It's safer, if perhaps not always necessary, to do this here. */
    server_resetEngines( server );

    XP_ASSERT( server->nv.gameState != XWSTATE_GAMEOVER );
    callTurnChangeListener( server, xwe );
    util_turnChanged( server->vol.util, xwe, server->nv.currentTurn );

    if ( robotMovePending(server) && !POSTPONEROBOTMOVE(server, xwe) ) {
        moreToDo = XP_TRUE;
    }

 exit:
    if ( moreToDo ) {
        util_requestTime( server->vol.util, xwe );
    }
} /* nextTurn */

void
server_setTurnChangeListener( ServerCtxt* server, TurnChangeListener tl,
                              void* data )
{
    XP_ASSERT( !server->vol.turnChangeListener );
    server->vol.turnChangeListener = tl;
    server->vol.turnChangeData = data;
} /* server_setTurnChangeListener */

void
server_setTimerChangeListener( ServerCtxt* server, TimerChangeListener tl,
                               void* data )
{
    XP_ASSERT( !server->vol.timerChangeListener );
    server->vol.timerChangeListener = tl;
    server->vol.timerChangeData = data;
}

void
server_setGameOverListener( ServerCtxt* server, GameOverListener gol,
                            void* data )
{
    server->vol.gameOverListener = gol;
    server->vol.gameOverData = data;
} /* server_setGameOverListener */

static void
storeBadWords( const WNParams* wnp, void* closure )
{
    if ( !wnp->isLegal ) {
        ServerCtxt* server = (ServerCtxt*)closure;
        const XP_UCHAR* name = dict_getShortName( wnp->dict );

        XP_LOGF( "storeBadWords called with \"%s\" (name=%s)", wnp->word, name );
        if ( NULL == server->illegalWordInfo.dictName ) {
            server->illegalWordInfo.dictName = copyString( server->mpool, name );
        }
        server->illegalWordInfo.words[server->illegalWordInfo.nWords++]
            = copyString( server->mpool, wnp->word );
    }
} /* storeBadWords */

static XP_Bool
checkMoveAllowed( ServerCtxt* server, XWEnv xwe, XP_U16 playerNum )
{
    CurGameInfo* gi = server->vol.gi;
    XP_ASSERT( server->illegalWordInfo.nWords == 0 );

    if ( gi->phoniesAction == PHONIES_DISALLOW ) {
        WordNotifierInfo info;
        info.proc = storeBadWords;
        info.closure = server;
        (void)model_checkMoveLegal( server->vol.model, xwe, playerNum,
                                    (XWStreamCtxt*)NULL, &info );
    }

    return server->illegalWordInfo.nWords == 0;
} /* checkMoveAllowed */

#ifndef XWFEATURE_STANDALONE_ONLY
static void
sendMoveTo( ServerCtxt* server, XWEnv xwe, XP_U16 devIndex, XP_U16 turn,
            XP_Bool legal, TrayTileSet* newTiles, 
            const TrayTileSet* tradedTiles ) /* null if a move, set if a trade */
{
    XP_Bool isTrade = !!tradedTiles;
    CurGameInfo* gi = server->vol.gi;
    XW_Proto code = gi->serverRole == SERVER_ISCLIENT?
        XWPROTO_MOVEMADE_INFO_CLIENT : XWPROTO_MOVEMADE_INFO_SERVER;

    XWStreamCtxt* stream = messageStreamWithHeader( server, xwe, devIndex, code );

#ifdef STREAM_VERS_BIGBOARD
    XP_U16 version = stream_getVersion( stream );
    if ( STREAM_VERS_BIGBOARD <= version ) {
        XP_ASSERT( version == server->nv.streamVersion );
        XP_U32 hash = model_getHash( server->vol.model );
#ifdef DEBUG_HASHING
        XP_LOGFF( "adding hash %X", (unsigned int)hash );
#endif
        stream_putU32( stream, hash );
    }
#endif

    stream_putBits( stream, PLAYERNUM_NBITS, turn ); /* who made the move */

    traySetToStream( stream, newTiles );

    stream_putBits( stream, 1, isTrade );

    if ( isTrade ) {
        traySetToStream( stream, tradedTiles );
    } else {
        stream_putBits( stream, 1, legal );

        model_currentMoveToStream( server->vol.model, turn, stream );

        if ( gi->timerEnabled ) {
            stream_putU16( stream, gi->players[turn].secondsUsed );
            XP_LOGF("%s: wrote secondsUsed for player %d: %d", __func__,
                       turn, gi->players[turn].secondsUsed );
        } else {
            XP_ASSERT( gi->players[turn].secondsUsed == 0 );
        }

        if ( !legal ) {
            XP_ASSERT( server->illegalWordInfo.nWords > 0 );
            stream_putBits( stream, PLAYERNUM_NBITS, turn );
            bwiToStream( stream, &server->illegalWordInfo );
        }
    }

    stream_destroy( stream, xwe );
} /* sendMoveTo */

static XP_Bool
readMoveInfo( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* stream,
              XP_U16* whoMovedP, XP_Bool* isTradeP,
              TrayTileSet* newTiles, TrayTileSet* tradedTiles, 
              XP_Bool* legalP, XP_Bool* badStackP )
{
    LOG_FUNC();
    XP_Bool success = XP_TRUE;
    XP_Bool legalMove = XP_TRUE;
    XP_Bool isTrade;

#ifdef STREAM_VERS_BIGBOARD
    if ( STREAM_VERS_BIGBOARD <= stream_getVersion( stream ) ) {
        XP_U32 hashReceived = stream_getU32( stream );
        success = model_hashMatches( server->vol.model, hashReceived )
            || model_popToHash( server->vol.model, xwe, hashReceived, server->pool );

        if ( !success ) {
            XP_LOGFF( "hash mismatch: %X not found", hashReceived );
            *badStackP = XP_TRUE;
#ifdef DEBUG_HASHING
        } else {
            XP_LOGF( "%s: hash match: %X",__func__, hashReceived );
#endif
        }
    }
#endif
    if ( success ) {
        XP_U16 whoMoved = stream_getBits( stream, PLAYERNUM_NBITS );
        traySetFromStream( stream, newTiles );
        success = pool_containsTiles( server->pool, newTiles );
        XP_ASSERT( success );
        if ( success ) {
            isTrade = stream_getBits( stream, 1 );

            if ( isTrade ) {
                traySetFromStream( stream, tradedTiles );
                XP_LOGF( "%s: got trade of %d tiles", __func__, 
                         tradedTiles->nTiles );
            } else {
                legalMove = stream_getBits( stream, 1 );
                success = model_makeTurnFromStream( server->vol.model, 
                                                    xwe, whoMoved, stream );
                getPlayerTime( server, stream, whoMoved );
            }

            if ( success ) {
                pool_removeTiles( server->pool, newTiles );

                *whoMovedP = whoMoved;
                *isTradeP = isTrade;
                *legalP = legalMove;
            }
        }
    }
    LOG_RETURNF( "%s", boolToStr(success) );
    return success;
} /* readMoveInfo */

static void
sendMoveToClientsExcept( ServerCtxt* server, XWEnv xwe, XP_U16 whoMoved, XP_Bool legal,
                         TrayTileSet* newTiles, const TrayTileSet* tradedTiles,
                         XP_U16 skip )
{
    for ( XP_U16 devIndex = 1; devIndex < server->nv.nDevices; ++devIndex ) {
        if ( devIndex != skip ) {
            sendMoveTo( server, xwe, devIndex, whoMoved, legal,
                        newTiles, tradedTiles );
        }
    }
} /* sendMoveToClientsExcept */

static XWStreamCtxt*
makeTradeReportIf( ServerCtxt* server, XWEnv xwe, const TrayTileSet* tradedTiles )
{
    XWStreamCtxt* stream = NULL;
    if ( inDuplicateMode(server) || server->nv.showRobotScores ) {
        XP_UCHAR tradeBuf[64];
        const XP_UCHAR* tradeStr = 
            dutil_getUserQuantityString( server->vol.dutil, xwe, STRD_ROBOT_TRADED,
                                         tradedTiles->nTiles );
        XP_SNPRINTF( tradeBuf, sizeof(tradeBuf), tradeStr, 
                     tradedTiles->nTiles );
        stream = mkServerStream( server );
        stream_catString( stream, tradeBuf );
    }
    return stream;
} /* makeTradeReportIf */

static XWStreamCtxt*
makeMoveReportIf( ServerCtxt* server, XWEnv xwe, XWStreamCtxt** wordsStream )
{
    XWStreamCtxt* stream = NULL;
    if ( inDuplicateMode(server) || server->nv.showRobotScores ) {
        ModelCtxt* model = server->vol.model;
        stream = mkServerStream( server );
        *wordsStream = mkServerStream( server );
        WordNotifierInfo* ni = model_initWordCounter( model, *wordsStream );
        (void)model_checkMoveLegal( model, xwe, server->nv.currentTurn, stream, ni );
    }
    return stream;
} /* makeMoveReportIf */

/* Client is reporting a move made, complete with new tiles and time taken by
 * the player.  Update the model with that information as a tentative move,
 * then sent info about it to all the clients, and finally commit the move
 * here.
 */
static XP_Bool
reflectMoveAndInform( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* stream )
{
    XP_Bool success;
    ModelCtxt* model = server->vol.model;
    XP_U16 whoMoved;
    XP_U16 nTilesMoved = 0; /* trade case */
    XP_Bool isTrade;
    XP_Bool isLegalMove;
    XP_Bool doRequest = XP_FALSE;
    TrayTileSet newTiles;
    TrayTileSet tradedTiles;
    CurGameInfo* gi = server->vol.gi;
    XP_U16 sourceClientIndex = getIndexForStream( server, stream );
    XWStreamCtxt* mvStream = NULL;
    XWStreamCtxt* wordsStream = NULL;

    XP_ASSERT( gi->serverRole == SERVER_ISSERVER );

    XP_Bool badStack = XP_FALSE;
    success = readMoveInfo( server, xwe, stream, &whoMoved, &isTrade, &newTiles,
                            &tradedTiles, &isLegalMove, &badStack ); /* modifies model */
    XP_ASSERT( !success || isLegalMove ); /* client should always report as true */
    isLegalMove = XP_TRUE;

    if ( success ) {
        if ( isTrade ) {
            sendMoveToClientsExcept( server, xwe, whoMoved, XP_TRUE, &newTiles,
                                     &tradedTiles, sourceClientIndex );

            model_makeTileTrade( model, whoMoved,
                                 &tradedTiles, &newTiles );
            pool_replaceTiles( server->pool, &tradedTiles );

            server->vol.showPrevMove = XP_TRUE;
            mvStream = makeTradeReportIf( server, xwe, &tradedTiles );

        } else {
            nTilesMoved = model_getCurrentMoveCount( model, whoMoved );
            isLegalMove = (nTilesMoved == 0)
                || checkMoveAllowed( server, xwe, whoMoved );

            /* I don't think this will work if there are more than two devices in
               a palm game; need to change state and get out of here before
               returning to send additional messages.  PENDING(ehouse) */
            sendMoveToClientsExcept( server, xwe, whoMoved, isLegalMove, &newTiles,
                                     (TrayTileSet*)NULL, sourceClientIndex );

            server->vol.showPrevMove = XP_TRUE;
            if ( isLegalMove ) {
                mvStream = makeMoveReportIf( server, xwe, &wordsStream );
            }

            success = model_commitTurn( model, xwe, whoMoved, &newTiles );
            server_resetEngines( server );
        }

        if ( success && isLegalMove ) {
            XP_U16 nTilesLeft = model_getNumTilesTotal( model, whoMoved );

            if ( (gi->phoniesAction == PHONIES_DISALLOW) && (nTilesMoved > 0) ) {
                server->lastMoveSource = sourceClientIndex;
                SETSTATE( server, XWSTATE_MOVE_CONFIRM_MUSTSEND );
                doRequest = XP_TRUE;
            } else if ( nTilesLeft > 0 ) {
                nextTurn( server, xwe, PICK_NEXT );
            } else {
                SETSTATE(server, XWSTATE_NEEDSEND_ENDGAME );
                doRequest = XP_TRUE;
            }

            if ( !!mvStream ) {
                XP_ASSERT( !server->nv.prevMoveStream );
                server->nv.prevMoveStream = mvStream;
                XP_ASSERT( !server->nv.prevWordsStream );
                server->nv.prevWordsStream = wordsStream;
            }
        } else {
            /* The client from which the move came still needs to be told.  But we
               can't send a message now since we're burried in a message handler.
               (Palm, at least, won't manage.)  So set up state to tell that
               client again in a minute. */
            SETSTATE( server, XWSTATE_NEEDSEND_BADWORD_INFO );
            server->lastMoveSource = sourceClientIndex;
            doRequest = XP_TRUE;
        }

        if ( doRequest ) {
            util_requestTime( server->vol.util, xwe );
        }
    } else if ( badStack ) {
        success = XP_TRUE;      /* so we don't reject the move forever */
    }
    return success;
} /* reflectMoveAndInform */

static XP_Bool
reflectMove( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* stream )
{
    XP_Bool isTrade;
    XP_Bool isLegal;
    XP_U16 whoMoved;
    TrayTileSet newTiles;
    TrayTileSet tradedTiles;
    ModelCtxt* model = server->vol.model;
    XWStreamCtxt* mvStream = NULL;
    XWStreamCtxt* wordsStream = NULL;

    XP_Bool badStack = XP_FALSE;
    XP_Bool moveOk = XP_FALSE;

    if ( XWSTATE_INTURN != server->nv.gameState ) {
        XP_LOGFF( "BAD: game state: %s, not XWSTATE_INTURN", getStateStr(server->nv.gameState ) );
    } else if ( server->nv.currentTurn < 0 ) {
        XP_LOGFF( "BAD: currentTurn %d < 0", server->nv.currentTurn );
    } else if ( ! readMoveInfo( server, xwe, stream, &whoMoved, &isTrade,
                                &newTiles, &tradedTiles, &isLegal, &badStack ) ) { /* modifies model */
        XP_LOGFF( "BAD: readMoveInfo() failed" );
    } else {
        moveOk = XP_TRUE;
    }

    if ( moveOk ) {
        if ( isTrade ) {
            model_makeTileTrade( model, whoMoved, &tradedTiles, &newTiles );
            pool_replaceTiles( server->pool, &tradedTiles );

            server->vol.showPrevMove = XP_TRUE;
            mvStream = makeTradeReportIf( server, xwe, &tradedTiles );
        } else {
            server->vol.showPrevMove = XP_TRUE;
            mvStream = makeMoveReportIf( server, xwe, &wordsStream );
            model_commitTurn( model, xwe, whoMoved, &newTiles );
        }

        if ( !!mvStream ) {
            XP_ASSERT( !server->nv.prevMoveStream );
            server->nv.prevMoveStream = mvStream;
            XP_ASSERT( !server->nv.prevWordsStream );
            server->nv.prevWordsStream = wordsStream;
        }

        server_resetEngines( server );

        if ( !isLegal ) {
            XP_ASSERT( server->vol.gi->serverRole == SERVER_ISCLIENT );
            handleIllegalWord( server, xwe, stream );
        }
    } else if ( badStack ) {
        moveOk = XP_TRUE;
    }
    return moveOk;
} /* reflectMove */
#endif /* XWFEATURE_STANDALONE_ONLY */

static void
dupe_chooseMove( const ServerCtxt* server, XWEnv xwe, XP_U16 nPlayers,
                 XP_U16 scores[], XP_U16* winner, XP_U16* winningNTiles )
{
    ModelCtxt* model = server->vol.model;
    struct {
        XP_U16 score;
        XP_U16 nTiles;
        XP_U16 player;
    } moveData[MAX_NUM_PLAYERS];
    XP_U16 nWinners = 0;

    /* Pick the best move. "Best" means highest scoring, or in case of a score
       tie the largest number of tiles used. If there's still a tie, pick at
       random. :-) */
    for ( XP_U16 player = 0; player < nPlayers; ++player ) {
        XP_S16 score;
        if ( !getCurrentMoveScoreIfLegal( model, xwe, player, NULL, NULL, &score ) ) {
            score = 0;
        }
        scores[player] = score;

        XP_U16 nTiles = score == 0 ? 0 : model_getCurrentMoveCount( model, player );

        XP_Bool saveIt = nWinners == 0;
        if ( !saveIt ) {     /* not our first time through  */
            if ( score > moveData[nWinners-1].score ) { /* score wins? Keep it! */
                saveIt = XP_TRUE;
                nWinners = 0;
            } else if ( score < moveData[nWinners-1].score ) { /* score too low? */
                // score lower than best; drop it!
            } else if ( nTiles > moveData[nWinners-1].nTiles ) {
                saveIt = XP_TRUE;
                nWinners = 0;
            } else if ( nTiles < moveData[nWinners-1].nTiles ) {
                // number of tiles lower than best; drop it!
            } else {
                saveIt = XP_TRUE;
            }
        }

        if ( saveIt ) {
            moveData[nWinners].score = score;
            moveData[nWinners].nTiles = nTiles;
            moveData[nWinners].player = player;
            ++nWinners;
        }

    }

    const XP_U16 winnerIndx = 1 == nWinners ? 0 : XP_RANDOM() % nWinners;
    *winner = moveData[winnerIndx].player;
    *winningNTiles = moveData[winnerIndx].nTiles;
    /* This fires: I need the reassign-no-moves thing */
    if ( *winningNTiles == 0 ) {
        XP_LOGF( "%s(): no scoring move found", __func__ );
    } else {
        XP_LOGF( "%s(): %d wins with %d points", __func__, *winner,
                 scores[*winner] );
    }
}

static XP_Bool
dupe_allForced( const ServerCtxt* server )
{
    XP_Bool result = XP_TRUE;
    for ( XP_U16 ii = 0; result && ii < server->vol.gi->nPlayers; ++ii ) {
        result = server->nv.dupTurnsForced[ii];
    }
    LOG_RETURNF( "%d", result );
    return result;
}

/* Called for host or standalone case when all moves for the turn are
   present. Pick the best one and commit locally. In server case, transmit to
   each guest device as well. */
static void
dupe_commitAndReport( ServerCtxt* server, XWEnv xwe )
{
    const XP_U16 nPlayers = server->vol.gi->nPlayers;
    XP_U16 scores[nPlayers];

    XP_U16 winner;
    XP_U16 nTiles;
    dupe_chooseMove( server, xwe, nPlayers, scores, &winner, &nTiles );

    /* If nobody can move AND there are tiles left, trade instead of recording
       a 0. Unless we're running a timer, in which case it's most likely
       noboby's playing, so pause the game instead. */
    if ( 0 == scores[winner] && 0 < pool_getNTilesLeft(server->pool) ) {
        if ( dupe_timerRunning() && dupe_allForced( server ) ) {
            dupe_autoPause( server, xwe );
        } else {
            dupe_makeAndReportTrade( server, xwe );
        }
    } else {
        dupe_commitAndReportMove( server, xwe, winner, nPlayers, scores, nTiles );
    }
    dupe_clearState( server );
} /* dupe_commitAndReport */

static void
sendStreamToDev( ServerCtxt* server, XWEnv xwe, XP_U16 dev, XW_Proto code,
                 XWStreamCtxt* data )
{
    XWStreamCtxt* stream = messageStreamWithHeader( server, xwe, dev, code );
    const XP_U16 dataLen = stream_getSize( data );
    const XP_U8* dataPtr = stream_getPtr( data );
    stream_putBytes( stream, dataPtr, dataLen );
    stream_destroy( stream, xwe );
}

/* Called in the case where nobody was able to move, does a trade. The goal is
   to make it more likely that folks will be able to move with the next set of
   tiles. For now I'll put them back first so there's a chance of getting some
   of the same back.
*/
static void
dupe_makeAndReportTrade( ServerCtxt* server, XWEnv xwe )
{
    LOG_FUNC();
    PoolContext* pool = server->pool;
    ModelCtxt* model = server->vol.model;

    model_resetCurrentTurn( model, xwe, DUP_PLAYER );

    TrayTileSet oldTiles = *model_getPlayerTiles( model, DUP_PLAYER );
    model_removePlayerTiles( model, DUP_PLAYER, &oldTiles );
    pool_replaceTiles( pool, &oldTiles );

    TrayTileSet newTiles = {0};
    fetchTiles( server, xwe, DUP_PLAYER, oldTiles.nTiles, NULL, &newTiles, XP_FALSE );

    model_commitDupeTrade( model, &oldTiles, &newTiles );

    model_addNewTiles( model, DUP_PLAYER, &newTiles );
    updateOthersTiles( server, xwe );

    if ( server->vol.gi->serverRole == SERVER_ISSERVER ) {
        XWStreamCtxt* tmpStream =
            mem_stream_make_raw( MPPARM(server->mpool)
                                 dutil_getVTManager(server->vol.dutil) );

        addDupeStuffMark( tmpStream, DUPE_STUFF_TRADES_SERVER );

        traySetToStream( tmpStream, &oldTiles );
        traySetToStream( tmpStream, &newTiles );

        /* Send it to each one */
        for ( XP_U16 dev = 1; dev < server->nv.nDevices; ++dev ) {
            sendStreamToDev( server, xwe, dev, XWPROTO_DUPE_STUFF, tmpStream );
        }

        stream_destroy( tmpStream, xwe );
    }

    dupe_resetTimer( server, xwe );

    dupe_setupShowTrade( server, xwe, newTiles.nTiles );
    LOG_RETURN_VOID();
} /* dupe_makeAndReportTrade */

static void
dupe_transmitPause( ServerCtxt* server, XWEnv xwe, DupPauseType typ, XP_U16 turn,
                    const XP_UCHAR* msg, XP_S16 skipDev )
{
    XP_LOGF( "%s(type=%d, msg=%s)", __func__, typ, msg );
    CurGameInfo* gi = server->vol.gi;
    if ( gi->serverRole != SERVER_STANDALONE ) {
        XP_Bool amClient = SERVER_ISCLIENT == gi->serverRole;
        XWStreamCtxt* tmpStream =
            mem_stream_make_raw( MPPARM(server->mpool)
                             dutil_getVTManager(server->vol.dutil) );

        addDupeStuffMark( tmpStream, DUPE_STUFF_PAUSE );

        stream_putBits( tmpStream, 1, amClient );
        stream_putBits( tmpStream, 2, typ );
        if ( AUTOPAUSED != typ ) {
            stream_putBits( tmpStream, PLAYERNUM_NBITS, turn );
        }
        stream_putU32( tmpStream, server->nv.dupTimerExpires );
        if ( AUTOPAUSED != typ ) {
            stringToStream( tmpStream, msg );
        }

        if ( amClient ) {
            sendStreamToDev( server, xwe, SERVER_DEVICE, XWPROTO_DUPE_STUFF, tmpStream );
        } else {
            for ( XP_U16 dev = 1; dev < server->nv.nDevices; ++dev ) {
                if ( dev != skipDev ) {
                    sendStreamToDev( server, xwe, dev, XWPROTO_DUPE_STUFF, tmpStream );
                }
            }
        }
        stream_destroy( tmpStream, xwe );
    }
}

static XP_Bool
dupe_receivePause( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* stream )
{
    LOG_FUNC();
    XP_Bool isClient = (XP_Bool)stream_getBits( stream, 1 );
    XP_Bool accept = isClient == amServer( server );
    if ( accept ) {
        const CurGameInfo* gi = server->vol.gi;
        DupPauseType pauseType = (DupPauseType)stream_getBits( stream, 2 );
        XP_S16 turn = -1;
        if ( AUTOPAUSED != pauseType ) {
            turn = (XP_S16)stream_getBits( stream, PLAYERNUM_NBITS );
            XP_ASSERT( 0 <= turn );
        } else {
            dupe_clearState( server );
        }

        setDupTimerExpires( server, xwe, (XP_S32)stream_getU32( stream ) );

        XP_UCHAR* msg = NULL;
        if ( AUTOPAUSED != pauseType ) {
            msg = stringFromStream( server->mpool, stream );
            XP_LOGF( "%s(): pauseType: %d; guiltyParty: %d; msg: %s", __func__,
                     pauseType, turn, msg );
        }

        if ( amServer( server ) ) {
            XP_U16 senderDev = getIndexForStream( server, stream );
            dupe_transmitPause( server, xwe, pauseType, turn, msg, senderDev );
        }

        model_noteDupePause( server->vol.model, xwe, pauseType, turn, msg );
        callTurnChangeListener( server, xwe );

        const XP_UCHAR* name = NULL;
        if ( AUTOPAUSED != pauseType ) {
            name = gi->players[turn].name;
        }
        dutil_notifyPause( server->vol.dutil, xwe, gi->gameID, pauseType, turn,
                           name, msg );

        XP_FREEP( server->mpool, &msg );
    }
    LOG_RETURNF( "%d", accept );
    return accept;
}

static XP_Bool
dupe_handleStuff( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* stream )
{
    XP_Bool accepted;
    XP_Bool isServer = amServer( server );
    DUPE_STUFF typ = getDupeStuffMark( stream );
    switch ( typ ) {
    case DUPE_STUFF_MOVE_CLIENT:
        accepted = isServer && dupe_handleClientMoves( server, xwe, stream );
        break;
    case DUPE_STUFF_MOVES_SERVER:
        accepted = !isServer && dupe_handleServerMoves( server, xwe, stream );
        break;
    case DUPE_STUFF_TRADES_SERVER:
        accepted = !isServer && dupe_handleServerTrade( server, xwe, stream );
        break;
    case DUPE_STUFF_PAUSE:
        accepted = dupe_receivePause( server, xwe, stream );
        break;
    default:
        XP_ASSERT(0);
        accepted = XP_FALSE;
    }
    return accepted;
}

static void
dupe_commitAndReportMove( ServerCtxt* server, XWEnv xwe, XP_U16 winner,
                          XP_U16 nPlayers, XP_U16* scores,
                          XP_U16 nTiles )
{
    ModelCtxt* model = server->vol.model;

    /* The winning move is the one we'll commit everywhere. Get it. Reset
       everybody else then commit it there. */
    MoveInfo moveInfo = {0};
    model_currentMoveToMoveInfo( model, winner, &moveInfo );

    TrayTileSet newTiles = {0};
    fetchTiles( server, xwe, winner, nTiles, NULL, &newTiles, XP_FALSE );

    for ( XP_U16 player = 0; player < nPlayers; ++player ) {
        model_resetCurrentTurn( model, xwe, player );
    }

    model_commitDupeTurn( model, xwe, &moveInfo, nPlayers,
                          scores, &newTiles );

    updateOthersTiles( server, xwe );

    if ( server->vol.gi->serverRole == SERVER_ISSERVER ) {
        XWStreamCtxt* tmpStream =
            mem_stream_make_raw( MPPARM(server->mpool)
                                 dutil_getVTManager(server->vol.dutil) );

        addDupeStuffMark( tmpStream, DUPE_STUFF_MOVES_SERVER );

        moveInfoToStream( tmpStream, &moveInfo, bitsPerTile(server) );
        traySetToStream( tmpStream, &newTiles );

        /* Now write all the scores */
        stream_putBits( tmpStream, NPLAYERS_NBITS, nPlayers );
        scoresToStream( tmpStream, nPlayers, scores );

        /* Send it to each one */
        for ( XP_U16 dev = 1; dev < server->nv.nDevices; ++dev ) {
            sendStreamToDev( server, xwe, dev, XWPROTO_DUPE_STUFF, tmpStream );
        }

        stream_destroy( tmpStream, xwe );
    }

    dupe_resetTimer( server, xwe );

    dupe_setupShowMove( server, xwe, scores );
} /* dupe_commitAndReportMove */

static void
dupe_forceCommits( ServerCtxt* server, XWEnv xwe )
{
    if ( dupe_timerRunning() ) {
        XP_U32 now = dutil_getCurSeconds( server->vol.dutil, xwe );
        if ( server->nv.dupTimerExpires <= now  ) {

            ModelCtxt* model = server->vol.model;
            for ( XP_U16 ii = 0; ii < server->vol.gi->nPlayers; ++ii ) {
                if ( server->vol.gi->players[ii].isLocal
                     && !server->nv.dupTurnsMade[ii] ) {
                    if ( !model_checkMoveLegal( model, xwe, ii, (XWStreamCtxt*)NULL,
                                                (WordNotifierInfo*)NULL ) ) {
                        model_resetCurrentTurn( model, xwe, ii );
                    }
                    commitMoveImpl( server, xwe, ii, NULL, XP_TRUE );
                }
            }
        }
    }
}

/* Figure out whether everything we care about is done for this turn. If I'm a
   guest, I care only about local players. If I'm a host or standalone, I care
   about everything.  */
static void
dupe_checkWhatsDone( const ServerCtxt* server, XP_Bool amServer,
                XP_Bool* allDoneP, XP_Bool* allLocalsDoneP )
{
    XP_Bool allDone = XP_TRUE;
    XP_Bool allLocalsDone = XP_TRUE;
    for ( XP_U16 ii = 0;
          (allLocalsDone || allDone) && ii < server->vol.gi->nPlayers;
          ++ii ) {
        XP_Bool done = server->nv.dupTurnsMade[ii];
        XP_Bool isLocal = server->vol.gi->players[ii].isLocal;
        if ( isLocal ) {
            allLocalsDone = allLocalsDone & done;
        }
        if ( amServer || isLocal ) {
            allDone = allDone && done;
        }
    }

    // XP_LOGF( "%s(): allDone: %d; allLocalsDone: %d", __func__, allDone, allLocalsDone );
    *allDoneP = allDone;
    *allLocalsDoneP = allLocalsDone;
}

XP_Bool
server_dupTurnDone( const ServerCtxt* server, XP_U16 turn )
{
    return server->vol.gi->players[turn].isLocal
        && server->nv.dupTurnsMade[turn];
}

static XP_Bool
dupe_checkTurns( ServerCtxt* server, XWEnv xwe )
{
    /* If all local players have made moves, it's time to commit the moves
       locally or notifiy the host */
    XP_Bool allDone = XP_TRUE;
    XP_Bool allLocalsDone = XP_TRUE;
    XP_Bool amServer = server->vol.gi->serverRole == SERVER_ISSERVER
        || server->vol.gi->serverRole == SERVER_STANDALONE;
    dupe_checkWhatsDone( server, amServer, &allDone, &allLocalsDone );

    XP_LOGF( "%s(): allDone: %d", __func__, allDone );

    if ( allDone ) {            /* Yep: commit time */
        if ( amServer ) {       /* I now have everything I need to move the
                                   game foreward */
            dupe_commitAndReport( server, xwe );
        } else if ( ! server->nv.dupTurnsSent ) { /* I need to send info for
                                                     local players to host */
            XWStreamCtxt* stream =
                messageStreamWithHeader( server, xwe, SERVER_DEVICE,
                                         XWPROTO_DUPE_STUFF );

            addDupeStuffMark( stream, DUPE_STUFF_MOVE_CLIENT );

            /* XP_U32 hash = model_getHash( server->vol.model ); */
            /* stream_putU32( stream, hash ); */

            XP_U16 localCount = gi_countLocalPlayers( server->vol.gi, XP_FALSE );
            XP_LOGF( "%s(): writing %d moves", __func__, localCount );
            stream_putBits( stream, NPLAYERS_NBITS, localCount );
            for ( XP_U16 ii = 0; ii < server->vol.gi->nPlayers; ++ii ) {
                if ( server->vol.gi->players[ii].isLocal ) {
                    stream_putBits( stream, PLAYERNUM_NBITS, ii );
                    stream_putBits( stream, 1, server->nv.dupTurnsForced[ii] );
                    model_currentMoveToStream( server->vol.model, ii, stream );
                    XP_LOGF( "%s(): wrote move %d ", __func__, ii );
                }
            }

            stream_destroy( stream, xwe ); /* sends it */
            server->nv.dupTurnsSent = XP_TRUE;
        }
    }
    return allDone;
} /* dupe_checkTurns */

static void
dupe_postStatus( const ServerCtxt* server, XWEnv xwe, XP_Bool allDone )
{
    /* Standalone case: say nothing here. Should be self evident what's
       up.*/
    /* If I'm a client and it's NOT a local turn, tell user that his
       turn's been sent off and he has to wait.
       *
       * If I'm a server, tell user how many of the expected moves have not
       * yet been received. If all have been, say nothing.
       */

    XP_UCHAR buf[256] = {0};
    XP_Bool amHost = XP_FALSE;
    switch ( server->vol.gi->serverRole ) {
    case SERVER_STANDALONE:
        /* do nothing */
        break;
    case SERVER_ISCLIENT:
        if ( allDone ) {
            const XP_UCHAR* fmt = dutil_getUserString( server->vol.dutil, xwe,
                                                       STR_DUP_CLIENT_SENT );
            XP_SNPRINTF( buf, VSIZE(buf), "%s", fmt );
        }
        break;
    case SERVER_ISSERVER:
        amHost = XP_TRUE;
        if ( !allDone ) {
            XP_U16 nHere = 0;
            for ( XP_U16 ii = 0; ii < server->vol.gi->nPlayers; ++ii ) {
                if ( server->nv.dupTurnsMade[ii] ) {
                    ++nHere;
                }
            }
            const XP_UCHAR* fmt = dutil_getUserString( server->vol.dutil, xwe,
                                                       STRDD_DUP_HOST_RECEIVED );
            XP_SNPRINTF( buf, VSIZE(buf), fmt, nHere, server->vol.gi->nPlayers );
        }
    }

    if ( !!buf[0] ) {
        XP_LOGF( "%s(): msg=%s", __func__, buf );
        util_notifyDupStatus( server->vol.util, xwe, amHost, buf );
    }
}

/* Called on client only? */
static void
dupe_storeTurn( ServerCtxt* server, XWEnv xwe, XP_U16 turn, XP_Bool forced )
{
    XP_ASSERT( !server->nv.dupTurnsMade[turn] );
    XP_ASSERT( server->vol.gi->players[turn].isLocal ); /* not if I'm the host! */
    server->nv.dupTurnsMade[turn] = XP_TRUE;
    server->nv.dupTurnsForced[turn] = forced;

    XP_Bool allDone = dupe_checkTurns( server, xwe );
    dupe_postStatus( server, xwe, allDone );
    nextTurn( server, xwe, PICK_NEXT );

    XP_LOGF( "%s(): player %d now has %d tiles", __func__, turn,
             model_getNumTilesInTray( server->vol.model, turn ) );
}

static void
dupe_clearState( ServerCtxt* server )
{
    for ( XP_U16 ii = 0; ii < server->vol.gi->nPlayers; ++ii ) {
        server->nv.dupTurnsMade[ii] = XP_FALSE;
        server->nv.dupTurnsForced[ii] = XP_FALSE;
    }
    server->nv.dupTurnsSent = XP_FALSE;
}

/* Make it the "turn" of the next local player who hasn't yet submitted a
   turn. If all have, make it a non-local player's turn. */
static XP_U16
dupe_nextTurn( const ServerCtxt* server )
{
    CurGameInfo* gi = server->vol.gi;
    XP_S16 result = -1;
    XP_U16 nextNonLocal = DUP_PLAYER;
    for ( XP_U16 ii = 0; ii < gi->nPlayers; ++ii ) {
        if ( !server->nv.dupTurnsMade[ii] ) {
            if ( gi->players[ii].isLocal ) {
                result = ii;
                break;
            } else {
                nextNonLocal = ii;
            }
        }
    }

    if ( -1 == result ) {
        result = nextNonLocal;
    }

    return result;
}

/* A local player is done with his turn.  If a client device, broadcast
 * the move to the server (after which a response should be coming soon.)
 * If the server, then that step can be skipped: go straight to doing what
 * the server does after learning of a move on a remote device.
 *
 * Second cut.  Whether server or client, be responsible for checking the
 * basic legality of the move and taking new tiles out of the pool.  If
 * client, send the move and new tiles to the server.  If the server, fall
 * back to what will do after hearing from client: tell everybody who doesn't
 * already know what's happened: move and new tiles together.
 *
 * What about phonies when DISALLOW is set?  The server's ultimately
 * responsible, since it has the dictionary, so the client can't check.  So
 * when server, check and send move together with a flag indicating legality.
 * Client is responsible for first posting the move to the model and then
 * undoing it.  When client, send the move and go into a state waiting to hear
 * if it was legal -- but only if DISALLOW is set.
 */
static XP_Bool
commitMoveImpl( ServerCtxt* server, XWEnv xwe, XP_U16 player,
                TrayTileSet* newTilesP, XP_Bool forced )
{
    XP_Bool inDupeMode = inDuplicateMode(server);
    XP_ASSERT( server->nv.currentTurn == player || inDupeMode );
    XP_S16 turn = player;
    TrayTileSet newTiles = {0};

    if ( !!newTilesP ) {
        newTiles = *newTilesP;
    }

#ifdef DEBUG
    CurGameInfo* gi = server->vol.gi;
    if ( LP_IS_ROBOT( &gi->players[turn] ) ) {
        ModelCtxt* model = server->vol.model;
        XP_ASSERT( model_checkMoveLegal( model, xwe, turn, (XWStreamCtxt*)NULL,
                                         (WordNotifierInfo*)NULL ) );
    }
#endif

    /* commit the move.  get new tiles.  if server, send to everybody.
       if client, send to server.  */
    XP_ASSERT( turn >= 0 );

    if ( inDupeMode ) {
        dupe_storeTurn( server, xwe, turn, forced );
    } else {
        finishMove( server, xwe, &newTiles, turn );
    }

    return XP_TRUE;
}

XP_Bool
server_commitMove( ServerCtxt* server, XWEnv xwe, XP_U16 player, TrayTileSet* newTilesP )
{
    return commitMoveImpl( server, xwe, player, newTilesP, XP_FALSE );
}

static void
finishMove( ServerCtxt* server, XWEnv xwe, TrayTileSet* newTiles, XP_U16 turn )
{
    ModelCtxt* model = server->vol.model;
    CurGameInfo* gi = server->vol.gi;

    pool_removeTiles( server->pool, newTiles );
    server->vol.pickTilesCalled[turn] = XP_FALSE;

    XP_U16 nTilesMoved = model_getCurrentMoveCount( model, turn );
    fetchTiles( server, xwe, turn, nTilesMoved, NULL, newTiles, XP_FALSE );

    XP_Bool isClient = gi->serverRole == SERVER_ISCLIENT;
    XP_Bool isLegalMove = XP_TRUE;
#ifndef XWFEATURE_STANDALONE_ONLY
    if ( isClient ) {
        /* just send to server */
        sendMoveTo( server, xwe, SERVER_DEVICE, turn, XP_TRUE, newTiles,
                    (TrayTileSet*)NULL );
    } else {
        isLegalMove = checkMoveAllowed( server, xwe, turn );
        sendMoveToClientsExcept( server, xwe, turn, isLegalMove, newTiles,
                                 (TrayTileSet*)NULL, SERVER_DEVICE );
    }
#else
    isLegalMove = checkMoveAllowed( server, xwe, turn );
#endif

    model_commitTurn( model, xwe, turn, newTiles );
    sortTilesIf( server, turn );

    if ( !isLegalMove && !isClient ) {
        badWordMoveUndoAndTellUser( server, xwe, &server->illegalWordInfo );
        /* It's ok to free these guys.  I'm the server, and the move was made
           here, so I've notified all clients already by setting the flag (and
           passing the word) in sendMoveToClientsExcept. */
        freeBWI( MPPARM(server->mpool) &server->illegalWordInfo );
    }

    if ( 0 ) {
#ifndef XWFEATURE_STANDALONE_ONLY
    } else if (isClient && (gi->phoniesAction == PHONIES_DISALLOW)
               && nTilesMoved > 0 ) {
        SETSTATE( server, XWSTATE_MOVE_CONFIRM_WAIT );
        setTurn( server, xwe, -1 );
#endif
    } else {
        nextTurn( server, xwe, PICK_NEXT );
    }
    /* XP_LOGFF( "player %d now has %d tiles", turn, */
    /*           model_getNumTilesInTray( model, turn ) ); */
} /* finishMove */
    
XP_Bool
server_commitTrade( ServerCtxt* server, XWEnv xwe, const TrayTileSet* oldTiles,
                    TrayTileSet* newTilesP )
{
    TrayTileSet newTiles = {0};
    if ( !!newTilesP ) {
        newTiles = *newTilesP;
    }
    XP_U16 turn = server->nv.currentTurn;

    fetchTiles( server, xwe, turn, oldTiles->nTiles, oldTiles, &newTiles, XP_FALSE );

#ifndef XWFEATURE_STANDALONE_ONLY
    if ( server->vol.gi->serverRole == SERVER_ISCLIENT ) {
        /* just send to server */
        sendMoveTo(server, xwe, SERVER_DEVICE, turn, XP_TRUE, &newTiles, oldTiles);
    } else {
        sendMoveToClientsExcept( server, xwe, turn, XP_TRUE, &newTiles, oldTiles,
                                 SERVER_DEVICE );
    }
#endif

    pool_replaceTiles( server->pool, oldTiles );
    XP_ASSERT( turn == server->nv.currentTurn );
    model_makeTileTrade( server->vol.model, turn, oldTiles, &newTiles );
    sortTilesIf( server, turn );

    nextTurn( server, xwe, PICK_NEXT );
    return XP_TRUE;
} /* server_commitTrade */

XP_S16
server_getCurrentTurn( const ServerCtxt* server, XP_Bool* isLocal )
{
    XP_S16 turn = server->nv.currentTurn;
    if ( NULL != isLocal && turn >= 0 ) {
        *isLocal = server->vol.gi->players[turn].isLocal;
    }
    return turn;
} /* server_getCurrentTurn */

XP_Bool
server_isPlayersTurn( const ServerCtxt* server, XP_U16 turn )
{
    XP_Bool result = XP_FALSE;

    if ( inDuplicateMode(server) ) {
        if ( server->vol.gi->players[turn].isLocal
             && ! server->nv.dupTurnsMade[turn] ) {
            result = XP_TRUE;
        }
    } else {
        result = turn == server_getCurrentTurn( server, NULL );
    }

    // XP_LOGF( "%s(%d) => %d", __func__, turn, result );
    return result;
}

XP_Bool
server_getGameIsOver( const ServerCtxt* server )
{
    return server->nv.gameState == XWSTATE_GAMEOVER;
} /* server_getGameIsOver */

XP_Bool
server_getGameIsConnected( const ServerCtxt* server )
{
    return server->nv.gameState >= XWSTATE_NEWCLIENT;
} /* server_getGameIsConnected */

XP_U16
server_getMissingPlayers( const ServerCtxt* server )
{
    /* list players for which we're reserving slots that haven't shown up yet.
     * If I'm a guest and haven't received the registration message and set
     * server->nv.addresses[0].channelNo, all non-local players are missing.
     * If I'm a host, players whose deviceIndex is -1 are missing.
    */

    XP_U16 result = 0;
    XP_U16 ii;
    switch( server->vol.gi->serverRole ) {
    case SERVER_ISCLIENT:
        if ( 0 == server->nv.addresses[0].channelNo ) {
            CurGameInfo* gi = server->vol.gi;
            const LocalPlayer* lp = gi->players;
            for ( ii = 0; ii < gi->nPlayers; ++ii ) {
                if ( !lp->isLocal ) {
                    result |= 1 << ii;
                }
                ++lp;
            }
        }
        break;
    case SERVER_ISSERVER:
        if ( 0 < server->nv.pendingRegistrations ) {
            XP_U16 nPlayers = server->vol.gi->nPlayers;
            const ServerPlayer* players = server->players;
            for ( ii = 0; ii < nPlayers; ++ii ) {
                if ( players->deviceIndex == UNKNOWN_DEVICE ) {
                    result |= 1 << ii;
                }
                ++players;
            }
        }
        break;
    }

    return result;
}

XP_U32
server_getLastMoveTime( const ServerCtxt* server )
{
    return server->nv.lastMoveTime;
}

static void
doEndGame( ServerCtxt* server, XWEnv xwe, XP_S16 quitter )
{
    XP_ASSERT( quitter < server->vol.gi->nPlayers );
    SETSTATE( server, XWSTATE_GAMEOVER );
    setTurn( server, xwe, -1 );
    server->nv.quitter = quitter;

    (*server->vol.gameOverListener)( xwe, server->vol.gameOverData, quitter );
} /* doEndGame */

static void 
putQuitter( const ServerCtxt* server, XWStreamCtxt* stream, XP_S16 quitter )
{
    if ( STREAM_VERS_DICTNAME <= server->nv.streamVersion ) {
        stream_putU8( stream, quitter );
    }
}

static void
getQuitter( const ServerCtxt* server, XWStreamCtxt* stream, XP_S8* quitter )
{
    *quitter = STREAM_VERS_DICTNAME <= server->nv.streamVersion
            ? stream_getU8( stream ) : -1;
}

/* Somebody wants to end the game.
 *
 * If I'm the server, I send a END_GAME message to all clients.  If I'm a
 * client, I send the GAME_OVER_REQUEST message to the server.  If I'm the
 * server and this is called in response to a GAME_OVER_REQUEST, send the
 * GAME_OVER message to all clients including the one that requested it.
 */
static void
endGameInternal( ServerCtxt* server, XWEnv xwe, GameEndReason XP_UNUSED(why),
                 XP_S16 quitter )
{
    XP_ASSERT( server->nv.gameState != XWSTATE_GAMEOVER );
    XP_ASSERT( quitter < server->vol.gi->nPlayers );

    if ( server->vol.gi->serverRole != SERVER_ISCLIENT ) {

#ifndef XWFEATURE_STANDALONE_ONLY
        XP_U16 devIndex;
        for ( devIndex = 1; devIndex < server->nv.nDevices; ++devIndex ) {
            XWStreamCtxt* stream;
            stream = messageStreamWithHeader( server, xwe, devIndex,
                                              XWPROTO_END_GAME );
            putQuitter( server, stream, quitter );
            stream_destroy( stream, xwe );
        }
#endif
        doEndGame( server, xwe, quitter );

#ifndef XWFEATURE_STANDALONE_ONLY
    } else {
        XWStreamCtxt* stream;
        stream = messageStreamWithHeader( server, xwe, SERVER_DEVICE,
                                          XWPROTO_CLIENT_REQ_END_GAME );
        putQuitter( server, stream, quitter );
        stream_destroy( stream, xwe );

        /* Do I want to change the state I'm in?  I don't think so.... */
#endif
    }
} /* endGameInternal */

void
server_endGame( ServerCtxt* server, XWEnv xwe )
{
    XW_State gameState = server->nv.gameState;
    if ( gameState < XWSTATE_GAMEOVER && gameState >= XWSTATE_INTURN ) {
        endGameInternal( server, xwe, END_REASON_USER_REQUEST, server->nv.currentTurn );
    }
} /* server_endGame */

/* If game is about to end because one player's out of tiles, we don't want to
 * keep trying to move. Note that in duplicate mode if ANY player has tiles
 * the answer's yes. */
static XP_Bool
tileCountsOk( const ServerCtxt* server )
{
    XP_Bool maybeOver = 0 == pool_getNTilesLeft( server->pool );
    if ( maybeOver ) {
        ModelCtxt* model = server->vol.model;
        XP_U16 nPlayers = server->vol.gi->nPlayers;
        XP_Bool inDupMode = inDuplicateMode( server );
        XP_Bool zeroFound = inDupMode;

        for ( XP_U16 player = 0; player < nPlayers; ++player ) {
            XP_U16 count = model_getNumTilesTotal( model, player );
            if ( inDupMode && count > 0 ) {
                zeroFound = XP_FALSE;
                break;
            } else if ( !inDupMode && count == 0 ) {
                zeroFound = XP_TRUE;
                break;
            }
        }
        maybeOver = zeroFound;
    }
    XP_Bool result = !maybeOver;
    // LOG_RETURNF( "%d", result );
    return result;
} /* tileCountsOk */

static void
setTurn( ServerCtxt* server, XWEnv xwe, XP_S16 turn )
{
    XP_ASSERT( -1 == turn
               || (!amServer(server) || (0 == server->nv.pendingRegistrations)));
    XP_Bool inDupMode = inDuplicateMode( server );
    if ( inDupMode || server->nv.currentTurn != turn || 1 == server->vol.gi->nPlayers ) {
        if ( DUP_PLAYER == turn && inDupMode ) {
            turn = dupe_nextTurn( server );
        } else if ( PICK_CUR == turn ) {
            XP_ASSERT(0);       /* this should never happen */
        } else if ( 0 <= turn && !inDupMode ) {
            XP_ASSERT( turn == model_getNextTurn( server->vol.model ) );
        }
        server->nv.currentTurn = turn;
        server->nv.lastMoveTime = dutil_getCurSeconds( server->vol.dutil, xwe );
        callTurnChangeListener( server, xwe );
    }
}

#ifndef XWFEATURE_STANDALONE_ONLY
static void
tellMoveWasLegal( ServerCtxt* server, XWEnv xwe )
{
    XWStreamCtxt* stream =
        messageStreamWithHeader( server, xwe, server->lastMoveSource,
                                 XWPROTO_MOVE_CONFIRM );

    stream_destroy( stream, xwe );

    SETSTATE( server, XWSTATE_INTURN );
} /* tellMoveWasLegal */

static XP_Bool
handleIllegalWord( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* incoming )
{
    BadWordInfo bwi;

    (void)stream_getBits( incoming, PLAYERNUM_NBITS );
    bwiFromStream( MPPARM(server->mpool) incoming, &bwi );

    badWordMoveUndoAndTellUser( server, xwe, &bwi );

    freeBWI( MPPARM(server->mpool) &bwi );

    SETSTATE( server, XWSTATE_INTURN );
    return XP_TRUE;
} /* handleIllegalWord */

static XP_Bool
handleMoveOk( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* XP_UNUSED(incoming) )
{
    XP_ASSERT( server->vol.gi->serverRole == SERVER_ISCLIENT );
    XP_Bool accepted = server->nv.gameState == XWSTATE_MOVE_CONFIRM_WAIT;
    if ( accepted ) {
        SETSTATE( server, XWSTATE_INTURN );
        nextTurn( server, xwe, PICK_CUR );
    }
    return accepted;
} /* handleMoveOk */

static void
sendUndoTo( ServerCtxt* server, XWEnv xwe, XP_U16 devIndex, XP_U16 nUndone,
            XP_U16 lastUndone, XP_U32 newHash )
{
    XWStreamCtxt* stream;
    CurGameInfo* gi = server->vol.gi;
    XW_Proto code = gi->serverRole == SERVER_ISCLIENT?
        XWPROTO_UNDO_INFO_CLIENT : XWPROTO_UNDO_INFO_SERVER;

    stream = messageStreamWithHeader( server, xwe, devIndex, code );

    stream_putU16( stream, nUndone );
    stream_putU16( stream, lastUndone );
    stream_putU32( stream, newHash );

    stream_destroy( stream, xwe );
} /* sendUndoTo */

static void
sendUndoToClientsExcept( ServerCtxt* server, XWEnv xwe, XP_U16 skip, XP_U16 nUndone,
                         XP_U16 lastUndone, XP_U32 newHash )
{
    XP_U16 devIndex;

    for ( devIndex = 1; devIndex < server->nv.nDevices; ++devIndex ) {
        if ( devIndex != skip ) {
            sendUndoTo( server, xwe, devIndex, nUndone, lastUndone, newHash );
        }
    }
} /* sendUndoToClientsExcept */

static XP_Bool
reflectUndos( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* stream, XW_Proto code )
{
    LOG_FUNC();
    XP_U16 nUndone;
    XP_S16 lastUndone;
    XP_U16 turn;
    ModelCtxt* model = server->vol.model;
    XP_Bool success = XP_TRUE;

    nUndone = stream_getU16( stream );
    lastUndone = stream_getU16( stream );
    XP_U32 newHash = 0;
    if ( 0 < stream_getSize( stream ) ) {
        newHash = stream_getU32( stream );
    }
    XP_ASSERT( 0 == stream_getSize( stream ) );

    if ( 0 == newHash ) {
        success = model_undoLatestMoves( model, xwe, server->pool, nUndone, &turn,
                                         &lastUndone );
        XP_ASSERT( turn == model_getNextTurn( model ) );
    } else {
        success = model_popToHash( model, xwe, newHash, server->pool );
        turn = model_getNextTurn( model );
    }

    if ( success ) {
        XP_LOGFF( "popped down to %X", model_getHash( model ) );
        sortTilesIf( server, turn );

        if ( code == XWPROTO_UNDO_INFO_CLIENT ) { /* need to inform */
            XP_U16 sourceClientIndex = getIndexForStream( server, stream );

            sendUndoToClientsExcept( server, xwe, sourceClientIndex, nUndone,
                                     lastUndone, newHash );
        }

        util_informUndo( server->vol.util, xwe );
        nextTurn( server, xwe, turn );
    } else {
        XP_LOGFF( "unable to pop to hash %X; dropping", newHash );
        // XP_ASSERT(0);
        success = XP_TRUE;      /* Otherwise we'll stall */
    }

    LOG_RETURNF( "%s", boolToStr(success) );
    return success;
} /* reflectUndos */
#endif

XP_Bool
server_handleUndo( ServerCtxt* server, XWEnv xwe, XP_U16 limit )
{
    LOG_FUNC();
    XP_Bool result = XP_FALSE;
    XP_U16 lastTurnUndone = 0; /* quiet compiler good */
    XP_U16 nUndone = 0;
    ModelCtxt* model;
    CurGameInfo* gi;
    XP_U16 lastUndone = 0xFFFF;

    model = server->vol.model;
    gi = server->vol.gi;
    XP_ASSERT( !!model );

    /* Undo until we find we've just undone a non-robot move.  The point is
       not to stop with a robot about to act (since that's a bit pointless.)
       The exception is that if the first move was a robot move we'll stop
       there, and it will immediately move again. */
    for ( ; ; ) {
        XP_S16 moveNum = -1; /* don't need it checked */
        if ( !model_undoLatestMoves( model, xwe, server->pool, 1, &lastTurnUndone,
                                     &moveNum ) ) {
            break;
        }
        ++nUndone;
        XP_ASSERT( moveNum >= 0 );
        lastUndone = moveNum;
        if ( !LP_IS_ROBOT(&gi->players[lastTurnUndone]) ) {
            break;
        } else if ( 0 != limit && nUndone >= limit ) {
            break;
        }
    }

    result = nUndone > 0 ;
    if ( result ) {
        XP_U32 newHash = model_getHash( model );
#ifndef XWFEATURE_STANDALONE_ONLY
        XP_ASSERT( lastUndone != 0xFFFF );
        XP_LOGFF( "popped to hash %X", newHash );
        if ( server->vol.gi->serverRole == SERVER_ISCLIENT ) {
            sendUndoTo( server, xwe, SERVER_DEVICE, nUndone, lastUndone, newHash );
        } else {
            sendUndoToClientsExcept( server, xwe, SERVER_DEVICE, nUndone,
                                     lastUndone, newHash );
        }
#endif
        sortTilesIf( server, lastTurnUndone );
        nextTurn( server, xwe, lastTurnUndone );
    } else {
        /* I'm a bit nervous about this.  Is this the ONLY thing that cause
           nUndone to come back 0? */
        util_userError( server->vol.util, xwe, ERR_CANT_UNDO_TILEASSIGN );
    }

    LOG_RETURNF( "%s", boolToStr(result) );
    return result;
} /* server_handleUndo */

#ifndef XWFEATURE_STANDALONE_ONLY
static void
writeProto( const ServerCtxt* server, XWStreamCtxt* stream, XW_Proto proto )
{
#ifdef STREAM_VERS_BIGBOARD
    XP_ASSERT( server->nv.streamVersion > 0 );
    if ( STREAM_SAVE_PREVWORDS < server->nv.streamVersion ) {
        stream_putBits( stream, XWPROTO_NBITS, XWPROTO_NEW_PROTO );
        stream_putBits( stream, 8, server->nv.streamVersion );
    }
    stream_setVersion( stream, server->nv.streamVersion );
#else
    XP_USE(server);
#endif
    stream_putBits( stream, XWPROTO_NBITS, proto );
}

static XW_Proto
readProto( ServerCtxt* server, XWStreamCtxt* stream )
{
    XW_Proto proto = (XW_Proto)stream_getBits( stream, XWPROTO_NBITS );
    XP_U8 version = STREAM_SAVE_PREVWORDS; /* version prior to fmt change */
#ifdef STREAM_VERS_BIGBOARD
    if ( XWPROTO_NEW_PROTO == proto ) {
        version = stream_getBits( stream, 8 );
        proto = (XW_Proto)stream_getBits( stream, XWPROTO_NBITS );
    }
    server->nv.streamVersion = version;
#else
    XP_USE(server);
#endif
    stream_setVersion( stream, version );
    return proto;
}

XP_Bool
server_receiveMessage( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* incoming )
{
    XP_Bool accepted = XP_FALSE;
    XP_Bool isServer = amServer( server );
    const XW_Proto code = readProto( server, incoming );
    XP_LOGFF( "(code=%s)", codeToStr(code) );

    switch ( code ) {
    case XWPROTO_DEVICE_REGISTRATION:
        accepted = isServer;
        if ( accepted ) {
        /* This message is special: doesn't have the header that's possible
           once the game's in progress and communication's been
           established. */
            XP_LOGF( "%s: somebody's registering!!!", __func__ );
            accepted = handleRegistrationMsg( server, xwe, incoming );
        } else {
            XP_LOGFF( "WTF: I'm not a server!!" );
        }
        break;
    case XWPROTO_CLIENT_SETUP:
        accepted = !isServer;
        if ( accepted ) {
            XP_STATUSF( "client got XWPROTO_CLIENT_SETUP" );
            accepted = client_readInitialMessage( server, xwe, incoming );
        }
        break;
#ifdef XWFEATURE_CHAT
    case XWPROTO_CHAT:
        accepted = receiveChat( server, xwe, incoming );
        break;
#endif
    case XWPROTO_MOVEMADE_INFO_CLIENT: /* client is reporting a move */
        if ( XWSTATE_INTURN == server->nv.gameState ) {
            accepted = reflectMoveAndInform( server, xwe, incoming );
        } else {
            XP_LOGFF( "bad state: %s; dropping", getStateStr( server->nv.gameState ) );
            accepted = XP_TRUE;
        }
        break;

    case XWPROTO_MOVEMADE_INFO_SERVER: /* server telling me about a move */
        if ( isServer ) {
            XP_LOGF( "%s(): %s received by server!", __func__, codeToStr(code) );
            accepted = XP_FALSE;
        } else {
            accepted = reflectMove( server, xwe, incoming );
        }
        if ( accepted ) {
            nextTurn( server, xwe, PICK_NEXT );
        } else {
            accepted = XP_TRUE; /* don't stall.... */
            XP_LOGFF( "dropping move: state=%s", getStateStr(server->nv.gameState ) );
        }
        break;

    case XWPROTO_UNDO_INFO_CLIENT:
    case XWPROTO_UNDO_INFO_SERVER:
        accepted = reflectUndos( server, xwe, incoming, code );
        /* nextTurn is called by reflectUndos */
        break;

    case XWPROTO_BADWORD_INFO:
        accepted = handleIllegalWord( server, xwe, incoming );
        if ( accepted && server->nv.gameState != XWSTATE_GAMEOVER ) {
            nextTurn( server, xwe, PICK_CUR );
        }
        break;

    case XWPROTO_MOVE_CONFIRM:
        accepted = handleMoveOk( server, xwe, incoming );
        break;

    case XWPROTO_CLIENT_REQ_END_GAME: {
        XP_S8 quitter;
        getQuitter( server, incoming, &quitter );
        endGameInternal( server, xwe, END_REASON_USER_REQUEST, quitter );
        accepted = XP_TRUE;
    }
        break;
    case XWPROTO_END_GAME: {
        XP_S8 quitter;
        getQuitter( server, incoming, &quitter );
        doEndGame( server, xwe, quitter );
        accepted = XP_TRUE;
    }
        break;
    case XWPROTO_DUPE_STUFF:
        accepted = dupe_handleStuff( server, xwe, incoming );
        break;
    default:
        XP_WARNF( "%s: Unknown code on incoming message: %d\n",
                  __func__, code );
        // will happen e.g. if we don't support chat and remote sends. Is ok.
        break;
    } /* switch */

    XP_ASSERT( isServer == amServer( server ) ); /* caching value is ok? */
    stream_close( incoming, xwe );

    XP_LOGFF( "=> %d (code=%s)", accepted, codeToStr(code) );
    // XP_ASSERT( accepted );      /* do not commit!!! */
    return accepted;
} /* server_receiveMessage */
#endif

void 
server_formatDictCounts( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* stream,
                         XP_U16 nCols, XP_Bool allFaces )
{
    const DictionaryCtxt* dict;
    Tile tile;
    XP_U16 nChars, nPrinted;
    XP_UCHAR buf[48];
    const XP_UCHAR* fmt = dutil_getUserString( server->vol.dutil, xwe,
                                               STRS_VALUES_HEADER );
    const XP_UCHAR* langName;

    XP_ASSERT( !!server->vol.model );

    dict = model_getDictionary( server->vol.model );
    langName = dict_getLangName( dict );
    XP_SNPRINTF( buf, sizeof(buf), fmt, langName );
    stream_catString( stream, buf );

    nChars = dict_numTileFaces( dict );
    XP_U16 boardSize = server->vol.gi->boardSize;
    for ( tile = 0, nPrinted = 0; ; ) {
        XP_UCHAR buf[128];
        XP_U16 count, value;

        count = dict_numTilesForSize( dict, tile, boardSize );

        if ( count > 0 ) {
            const XP_UCHAR* face = NULL;
            XP_UCHAR faces[48] = {0};
            XP_U16 len = 0;
            do {
                face = dict_getNextTileString( dict, tile, face );
                if ( !face ) {
                    break;
                }
                const XP_UCHAR* fmt = len == 0? "%s" : ",%s";
                len += XP_SNPRINTF( faces + len, sizeof(faces) - len, fmt, face );
            } while ( allFaces );
            value = dict_getTileValue( dict, tile );

            XP_SNPRINTF( buf, sizeof(buf), (XP_UCHAR*)"%s: %d/%d", 
                         faces, count, value );
            stream_catString( stream, buf );
        }

        if ( ++tile >= nChars ) {
            break;
        } else if ( count > 0 ) {
            if ( ++nPrinted % nCols == 0 ) {
                stream_catString( stream, XP_CR );
            } else {
                stream_catString( stream, (void*)"   " );
            }
        }
    }
} /* server_formatDictCounts */

/* Print the faces of all tiles left in the pool, including those currently in
 * trays !unless! player is >= 0, in which case his tiles get removed from the
 * pool.  The idea is to show him what tiles are left in play.
 */
void
server_formatRemainingTiles( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* stream,
                             XP_S16 player )
{
    PoolContext* pool = server->pool;
    if ( !!pool ) {
        XP_UCHAR buf[128];
        const DictionaryCtxt* dict = model_getDictionary( server->vol.model );
        Tile tile;
        XP_U16 nChars = dict_numTileFaces( dict );
        XP_U16 offset;
        XP_U16 counts[MAX_UNIQUE_TILES+1]; /* 1 for the blank */
        XP_U16 nLeft = pool_getNTilesLeft( pool );
        XP_UCHAR cntsBuf[512];

        XP_ASSERT( !!server->vol.model );

        const XP_UCHAR* fmt = dutil_getUserQuantityString( server->vol.dutil, xwe,
                                                           STRD_REMAINS_HEADER,
                                                           nLeft );
        XP_SNPRINTF( buf, sizeof(buf), fmt, nLeft );
        stream_catString( stream, buf );
        stream_catString( stream, "\n\n" );

        XP_MEMSET( counts, 0, sizeof(counts) );
        model_countAllTrayTiles( server->vol.model, counts, player );

        for ( cntsBuf[0] = '\0', offset = 0, tile = 0; 
              offset < sizeof(cntsBuf); ) {
            XP_U16 count = pool_getNTilesLeftFor( pool, tile ) + counts[tile];
            XP_Bool hasCount = count > 0;
            nLeft += counts[tile];

            if ( hasCount ) {
                const XP_UCHAR* face = dict_getTileString( dict, tile );

                for ( ; ; ) {
                    offset += XP_SNPRINTF( &cntsBuf[offset], 
                                           sizeof(cntsBuf) - offset, "%s", 
                                           face );
                    if ( --count == 0 ) {
                        break;
                    }
                    offset += XP_SNPRINTF( &cntsBuf[offset], 
                                           sizeof(cntsBuf) - offset, "." );
                }
            }

            if ( ++tile >= nChars ) {
                break;
            } else if ( hasCount ) {
                offset += XP_SNPRINTF( &cntsBuf[offset], 
                                       sizeof(cntsBuf) - offset, "   " );
            }
            XP_ASSERT( offset < sizeof(cntsBuf) );
        }

        fmt = dutil_getUserQuantityString( server->vol.dutil, xwe, STRD_REMAINS_EXPL,
                                           nLeft );
        XP_SNPRINTF( buf, sizeof(buf), fmt, nLeft );
        stream_catString( stream, buf );

        stream_catString( stream, cntsBuf );
    }
} /* server_formatRemainingTiles */

#ifdef XWFEATURE_BONUSALL
XP_U16
server_figureFinishBonus( const ServerCtxt* server, XP_U16 turn )
{
    XP_U16 result = 0;
    if ( 0 == pool_getNTilesLeft( server->pool ) ) {
        XP_U16 nOthers = server->vol.gi->nPlayers - 1;
        if ( 0 < nOthers ) {
            Tile tile;
            const DictionaryCtxt* dict = model_getDictionary( server->vol.model );
            XP_U16 counts[dict_numTileFaces( dict )];
            XP_MEMSET( counts, 0, sizeof(counts) );
            model_countAllTrayTiles( server->vol.model, counts, turn );
            for ( tile = 0; tile < VSIZE(counts); ++tile ) {
                XP_U16 count = counts[tile];
                if ( 0 < count ) {
                    result += count * dict_getTileValue( dict, tile );
                }
            }
            /* Check this... */
            result += result / nOthers;
        }
    }
    // LOG_RETURNF( "%d", result );
    return result;
}
#endif

#define IMPOSSIBLY_LOW_SCORE -1000
#if 0
static void
printPlayer( const ServerCtxt* server, XWStreamCtxt* stream, XP_U16 index, 
             const XP_UCHAR* placeBuf, ScoresArray* scores, 
             ScoresArray* tilePenalties, XP_U16 place )
{
    XP_UCHAR buf[128];
    CurGameInfo* gi = server->vol.gi;
    ModelCtxt* model = server->vol.model;
    XP_Bool firstDone = model_getNumTilesTotal( model, index ) == 0;
    XP_UCHAR tmpbuf[48];
    XP_U16 addSubKey = firstDone? STRD_REMAINING_TILES_ADD : STRD_UNUSED_TILES_SUB;
    const XP_UCHAR* addSubString = util_getUserString( server->vol.util, xwe, addSubKey );
    XP_UCHAR* timeStr = (XP_UCHAR*)"";
    XP_UCHAR timeBuf[16];
    if ( gi->timerEnabled ) {
        XP_U16 penalty = player_timePenalty( gi, index );
        if ( penalty > 0 ) {
            XP_SNPRINTF( timeBuf, sizeof(timeBuf), 
                         util_getUserString( server->vol.util, xwe,
                                             STRD_TIME_PENALTY_SUB ),
                         penalty ); /* positive for formatting */
            timeStr = timeBuf;
        }
    }

    XP_SNPRINTF( tmpbuf, sizeof(tmpbuf), addSubString,
                 firstDone? 
                 tilePenalties->arr[index]:
                 -tilePenalties->arr[index] );

    XP_SNPRINTF( buf, sizeof(buf), 
                 (XP_UCHAR*)"[%s] %s: %d" XP_CR "  (%d %s%s)",
                 placeBuf, emptyStringIfNull(gi->players[index].name),
                 scores->arr[index], model_getPlayerScore( model, index ),
                 tmpbuf, timeStr );
    if ( 0 < place ) {
        stream_catString( stream, XP_CR );
    }
    stream_catString( stream, buf );
} /* printPlayer */
#endif

void
server_writeFinalScores( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* stream )
{
    ScoresArray scores;
    ScoresArray tilePenalties;
    XP_U16 place;
    XP_S16 quitter = server->nv.quitter;
    XP_Bool quitterDone = XP_FALSE;
    ModelCtxt* model = server->vol.model;
    const XP_UCHAR* addString = dutil_getUserString( server->vol.dutil, xwe,
                                                     STRD_REMAINING_TILES_ADD );
    const XP_UCHAR* subString = dutil_getUserString( server->vol.dutil, xwe,
                                                     STRD_UNUSED_TILES_SUB );
    XP_UCHAR* timeStr;
    CurGameInfo* gi = server->vol.gi;
    const XP_U16 nPlayers = gi->nPlayers;

    XP_ASSERT( server->nv.gameState == XWSTATE_GAMEOVER );

    model_figureFinalScores( model, &scores, &tilePenalties );

    XP_S16 winningScore = IMPOSSIBLY_LOW_SCORE;
    for ( place = 1; !quitterDone; ++place ) {
        XP_UCHAR timeBuf[16];
        XP_UCHAR buf[128]; 
        XP_S16 thisScore = IMPOSSIBLY_LOW_SCORE;
        XP_S16 thisIndex = -1;
        XP_U16 ii, placeKey = 0;
        XP_Bool firstDone;

        /* Find the next player we should print */
        for ( ii = 0; ii < nPlayers; ++ii ) {
            if ( quitter != ii && scores.arr[ii] > thisScore ) {
                thisIndex = ii;
                thisScore = scores.arr[ii];
            }
        }

        /* save top score overall to test for winner, including tie case */
        if ( 1 == place ) {
            winningScore = thisScore;
        }

        if ( thisIndex == -1 ) {
            if ( quitter >= 0 ) {
                XP_ASSERT( !quitterDone );
                thisIndex = quitter;
                quitterDone = XP_TRUE;
                placeKey = STRSD_RESIGNED;
            } else {
                break; /* we're done */
            }
        } else if ( thisScore == winningScore ) {
            placeKey = STRSD_WINNER;
        }

        timeStr = (XP_UCHAR*)"";
        if ( gi->timerEnabled ) {
            XP_U16 penalty = player_timePenalty( gi, thisIndex );
            if ( penalty > 0 ) {
                XP_SNPRINTF( timeBuf, sizeof(timeBuf), 
                             dutil_getUserString( server->vol.dutil, xwe,
                                                  STRD_TIME_PENALTY_SUB ),
                             penalty ); /* positive for formatting */
                timeStr = timeBuf;
            }
        }

        XP_UCHAR tmpbuf[48] = {0};
        if ( !inDuplicateMode( server ) ) {
            firstDone = model_getNumTilesTotal( model, thisIndex) == 0;
            XP_SNPRINTF( tmpbuf, sizeof(tmpbuf),
                         (firstDone? addString:subString),
                         firstDone?
                         tilePenalties.arr[thisIndex]:
                         -tilePenalties.arr[thisIndex] );
        }

        const XP_UCHAR* name = emptyStringIfNull(gi->players[thisIndex].name);
        if ( 0 == placeKey ) {
            const XP_UCHAR* fmt = dutil_getUserString( server->vol.dutil, xwe,
                                                      STRDSD_PLACER );
            XP_SNPRINTF( buf, sizeof(buf), fmt, place,
                         name, scores.arr[thisIndex] );
        } else {
            const XP_UCHAR* fmt = dutil_getUserString( server->vol.dutil, xwe,
                                                       placeKey );
            XP_SNPRINTF( buf, sizeof(buf), fmt, name,
                         scores.arr[thisIndex] );
        }

        if ( !inDuplicateMode( server ) ) {
            XP_UCHAR buf2[64];
            XP_SNPRINTF( buf2, sizeof(buf2), XP_CR "  (%d %s%s)",
                         model_getPlayerScore( model, thisIndex ),
                         tmpbuf, timeStr );
            XP_STRCAT( buf, buf2 );
        }

        if ( 1 < place ) {
            stream_catString( stream, XP_CR );
        }
        stream_catString( stream, buf );

        /* Don't consider this one next time around */
        scores.arr[thisIndex] = IMPOSSIBLY_LOW_SCORE;
    }
} /* server_writeFinalScores */

#ifdef CPLUS
}
#endif
