/* 
 * Copyright 1997 - 2023 by Eric House (xwords@eehouse.org).  All rights
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
#include "knownplyr.h"
#include "stats.h"

#include "LocalizedStrIncludes.h"

#ifdef CPLUS
extern "C" {
#endif

#define LOCAL_ADDR NULL

/* These aren't really used */
enum {
    END_REASON_USER_REQUEST,
    END_REASON_OUT_OF_TILES,
    END_REASON_TOO_MANY_PASSES,
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

#define UNKNOWN_DEVICE -1
#define HOST_DEVICE 0

typedef struct _ServerPlayer {
    EngineCtxt* engine; /* each needs his own so don't interfere each other */
    XP_S8 deviceIndex;  /* 0 means local, -1 means unknown */
} ServerPlayer;

typedef struct _RemoteAddress {
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

#define MASK_IS_FROM_REMATCH (1<<0)
#define MASK_HAVE_RIP_INFO (1<<1)
#define FLAG_HARVEST_READY (1<<2)

typedef struct _ServerNonvolatiles {
    XP_U32 flags;           /*  */
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

    /* On guests only, stores addresses of other clients for rematch use*/
    struct {
        /* clients store this */
        XP_U16 addrsLen;
        XP_U8* addrs;

        /* rematch-created hosts store this */
        RematchInfo* order;  /* rematched host only */
    } rematch;

    XP_Bool dupTurnsMade[MAX_NUM_PLAYERS];
    XP_Bool dupTurnsForced[MAX_NUM_PLAYERS];
    XP_Bool dupTurnsSent;       /* used on guest only */

} ServerNonvolatiles;

typedef struct _BadWordsState {
    BadWordInfo bwi;
    XP_UCHAR* dictName;
} BadWordsState;

struct ServerCtxt {
    ServerVolatiles vol;
    ServerNonvolatiles nv;

    PoolContext* pool;

    BadWordsState bws;

    XP_U16 lastMoveSource;

    ServerPlayer srvPlyrs[MAX_NUM_PLAYERS];
    XP_Bool serverDoing;
#ifdef XWFEATURE_SLOW_ROBOT
    XP_Bool robotWaiting;
#endif
    MPSLOT
};

/* RematchInfo: used to store remote addresses and the order within an
   eventual game where players coming from those addresses are meant to
   live. Not all addresses (esp. the local host's) are meant to be here.
   Local players' indices are -1
*/

struct RematchInfo {
    XP_U16 nPlayers;            /* how many of users array are there */
    XP_S8 addrIndices[MAX_NUM_PLAYERS]; /* indices into addrs */
    XP_U16 nAddrs;              /* needn't be serialized; may not be needed */
    CommsAddrRec addrs[MAX_NUM_PLAYERS];
};

#define RIP_LOCAL_INDX -1

#ifdef XWFEATURE_SLOW_ROBOT
# define ROBOTWAITING(s) (s)->robotWaiting
#else
# define ROBOTWAITING(s) XP_FALSE
#endif

# define dupe_timerRunning()    server_canPause(server)

# ifdef ENABLE_LOGFFV
#  define SRVR_LOGFFV SRVR_LOGFF
# else
#  define SRVR_LOGFFV(...)
# endif

#ifdef DEBUG
# define SRVR_LOGFF( ... ) {                           \
        XP_U32 gameID = server->vol.gi->gameID;        \
        XP_GID_LOGFF( gameID, __VA_ARGS__ );           \
    }
#else
# define SRVR_LOGFF( ... )
#endif


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
                                        const BadWordsState* bws );
static XP_Bool tileCountsOk( const ServerCtxt* server );
static void setTurn( ServerCtxt* server, XWEnv xwe, XP_S16 turn );
static XWStreamCtxt* mkServerStream( const ServerCtxt* server, XP_U8 version );
static XWStreamCtxt* mkServerStream0( const ServerCtxt* server );
static void fetchTiles( ServerCtxt* server, XWEnv xwe, XP_U16 playerNum,
                        XP_U16 nToFetch, TrayTileSet* resultTiles,
                        XP_Bool forceCanPlay );
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
static void resetDupeTimerIf( ServerCtxt* server, XWEnv xwe );
static XP_Bool setDupCheckTimer( ServerCtxt* server, XWEnv xwe );
static void sortTilesIf( ServerCtxt* server, XP_S16 turn );

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
static void readGuestAddrs( ServerCtxt* server, XWStreamCtxt* stream,
                            XP_U8 streamVersion );
static XP_Bool getRematchInfoImpl( const ServerCtxt* server, XWEnv xwe,
                                   CurGameInfo* newGI, const NewOrder* nop,
                                   RematchInfo** ripp );

static void ri_fromStream( RematchInfo* rip, XWStreamCtxt* stream,
                           const ServerCtxt* server );
static void ri_toStream( XWStreamCtxt* stream, const RematchInfo* rip,
                         const ServerCtxt* server );
static void ri_addAddrAt( XW_DUtilCtxt* dutil, XWEnv xwe, RematchInfo* rip,
                          const CommsAddrRec* addr, XP_U16 playerIndex );
static void ri_addHostAddrs( RematchInfo* rip, const ServerCtxt* server );
static void ri_addLocal( RematchInfo* rip );

#ifdef DEBUG
static void assertRI( const RematchInfo* rip, const CurGameInfo* gi );
static void log_ri( const ServerCtxt* server, const RematchInfo* rip,
                    const char* caller, int line );
# define LOG_RI(RIP) log_ri(server, (RIP), __func__, __LINE__ )
#else
# define LOG_RI(rip)
# define assertRI(r, s)
#endif


#define PICK_NEXT -1
#define PICK_CUR -2

#ifdef DEBUG
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
    // LOG_RETURNF( "%s", boolToStr(result) );
    return result;
}

/*****************************************************************************
 *
 ****************************************************************************/
static void
syncPlayers( ServerCtxt* server )
{
    const CurGameInfo* gi = server->vol.gi;
    for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
        const LocalPlayer* lp = &gi->players[ii];
        if ( !lp->isLocal/*  && !lp->name */ ) {
            ++server->nv.pendingRegistrations;
        }
        ServerPlayer* player = &server->srvPlyrs[ii];
        player->deviceIndex = lp->isLocal? HOST_DEVICE : UNKNOWN_DEVICE;
    }
}

static XP_Bool
amHost( const ServerCtxt* server )
{
    XP_Bool result = SERVER_ISHOST == server->vol.gi->serverRole;
    // LOG_RETURNF( "%d (seed=%d)", result, comms_getChannelSeed( server->vol.comms ) );
    return result;
}

#ifdef DEBUG
XP_Bool server_getIsHost( const ServerCtxt* server ) { return amHost(server); }
#endif

static void
initServer( ServerCtxt* server, XWEnv xwe )
{
    SRVR_LOGFF(" ");
    setTurn( server, xwe, -1 ); /* game isn't under way yet */

    if ( server->vol.gi->serverRole == SERVER_ISCLIENT ) {
        SETSTATE( server, XWSTATE_NONE );
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

    XP_ASSERT( 0 == nv->flags );
    if ( STREAM_VERS_REMATCHORDER <= version ) {
        nv->flags = stream_getU32VL( stream );
    }

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
    /* XP_LOGFF( "read streamVersion: 0x%x", nv->streamVersion ); */
#endif

    if ( version >= STREAM_VERS_DUPLICATE ) {
        for ( ii = 0; ii < nPlayers; ++ii ) {
            nv->dupTurnsMade[ii] = stream_getBits( stream, 1 );
            // XP_LOGFF( "dupTurnsMade[%d]: %d", ii, nv->dupTurnsMade[ii] );
            nv->dupTurnsForced[ii] = stream_getBits( stream, 1 );
        }
        nv->dupTurnsSent = stream_getBits( stream, 1 );
    }
} /* getNV */

static void
putNV( XWStreamCtxt* stream, const ServerNonvolatiles* nv, XP_U16 nPlayers )
{
    stream_putU32VL( stream, nv->flags );
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

    for ( int ii = 0; ii < nPlayers; ++ii ) {
        stream_putBits( stream, 16, nv->addresses[ii].channelNo );
#ifdef STREAM_VERS_BIGBOARD
        stream_putBits( stream, 8, nv->addresses[ii].streamVersion );
#endif
    }
#ifdef STREAM_VERS_BIGBOARD
    stream_putU8( stream, nv->streamVersion );
    /* XP_LOGFF( "wrote streamVersion: 0x%x", nv->streamVersion ); */
#endif

    for ( int ii = 0; ii < nPlayers; ++ii ) {
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
        result = mkServerStream0( server );
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
    const XP_Bool isHost = amHost( server );
    RELCONST CommsCtxt* comms = server->vol.comms;
    const CurGameInfo* gi = server->vol.gi;
    XP_U16 nInvited = 0;
    CommsAddrRec selfAddr;
    CommsAddrRec* selfAddrP = NULL;
    CommsAddrRec hostAddr;
    CommsAddrRec* hostAddrP = NULL;
    if ( !!comms ) {
        selfAddrP = &selfAddr;
        comms_getSelfAddr( comms, selfAddrP );
        if ( comms_getHostAddr( comms, &hostAddr ) ) {
            hostAddrP = &hostAddr;
        }
    }

    XP_U16 nDevs = 0;
    XP_U16 nPending = 0;
    XP_Bool fromRematch = XP_FALSE;
    if ( XWSTATE_BEGIN < server->nv.gameState ) {
        /* do nothing */
    } else if ( isHost ) {
        nPending = server->nv.pendingRegistrations;
        nDevs = server->nv.nDevices - 1;
        if ( 0 < nPending ) {
            comms_getInvited( comms, &nInvited );
            if ( nPending < nInvited ) {
                nInvited = nPending;
            }
        }
        fromRematch = server_isFromRematch( server );
    } else if ( SERVER_ISCLIENT == gi->serverRole ) {
        nPending = gi->nPlayers - gi_countLocalPlayers( gi, XP_FALSE);
    }
    util_informMissing( server->vol.util, xwe, isHost,
                        hostAddrP, selfAddrP, nDevs, nPending, nInvited,
                        fromRematch );
}

XP_U16
server_getPendingRegs( const ServerCtxt* server )
{
    XP_U16 nPending = amHost( server ) ? server->nv.pendingRegistrations : 0;
    return nPending;
}

ServerCtxt*
server_makeFromStream( MPFORMAL XWEnv xwe, XWStreamCtxt* stream, ModelCtxt* model,
                       CommsCtxt* comms, XW_UtilCtxt* util, XP_U16 nPlayers )
{
    ServerCtxt* server;
    XP_U16 version = stream_getVersion( stream );

    server = server_make( MPPARM(mpool) xwe, model, comms, util );
    getNV( stream, &server->nv, nPlayers );
    
    if ( stream_getBits(stream, 1) != 0 ) {
        server->pool = pool_makeFromStream( MPPARM(mpool) stream );
    }

    for ( int ii = 0; ii < nPlayers; ++ii ) {
        ServerPlayer* player = &server->srvPlyrs[ii];

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

    if ( server->vol.gi->serverRole == SERVER_ISCLIENT
         && 2 < nPlayers ) {
        readGuestAddrs( server, stream, server->nv.streamVersion );
    }

    if ( 0 != (server->nv.flags & MASK_HAVE_RIP_INFO) ) {
        struct RematchInfo ri;
        ri_fromStream( &ri, stream, server );
        server_setRematchOrder( server, &ri );
    }

    /* Hack alert: recovering from an apparent bug that leaves the game
       thinking it's a client but being in the host-only XWSTATE_BEGIN
       state. */
    if ( server->nv.gameState == XWSTATE_BEGIN &&
         server->vol.gi->serverRole == SERVER_ISCLIENT ) {
        SRVR_LOGFF( "fixing state" );
        SETSTATE( server, XWSTATE_NONE );
    }

    informMissing( server, xwe );
    return server;
} /* server_makeFromStream */

void
server_writeToStream( const ServerCtxt* server, XWStreamCtxt* stream )
{
    const XP_U16 nPlayers = server->vol.gi->nPlayers;

    putNV( stream, &server->nv, nPlayers );

    stream_putBits( stream, 1, server->pool != NULL );
    if ( server->pool != NULL ) {
        pool_writeToStream( server->pool, stream );
    }

    for ( int ii = 0; ii < nPlayers; ++ii ) {
        const ServerPlayer* player = &server->srvPlyrs[ii];

        stream_putU8( stream, player->deviceIndex );

        stream_putU8( stream, (XP_U8)(player->engine != NULL) );
        if ( player->engine != NULL ) {
            engine_writeToStream( player->engine, stream );
        }
    }

    stream_putBits( stream, 2, server->lastMoveSource );

    writeStreamIf( stream, server->nv.prevMoveStream );
    writeStreamIf( stream, server->nv.prevWordsStream );

    if ( server->vol.gi->serverRole == SERVER_ISCLIENT
         && 2 < nPlayers ) {
        XP_U16 len = server->nv.rematch.addrsLen;
        stream_putU32VL( stream, len );
        stream_putBytes( stream, server->nv.rematch.addrs, len );
    }

    if ( 0 != (server->nv.flags & MASK_HAVE_RIP_INFO) ) {
        XP_ASSERT( !!server->nv.rematch.order );
        ri_toStream( stream, server->nv.rematch.order, server );
    }
} /* server_writeToStream */

#ifdef XWFEATURE_RELAY
void
server_onRoleChanged( ServerCtxt* server, XWEnv xwe, XP_Bool amNowGuest )
{
    if ( amNowGuest == amHost(server) ) { /* do I need to change */
        XP_ASSERT ( amNowGuest );
        if ( amNowGuest ) {
            server->vol.gi->serverRole = SERVER_ISCLIENT;
            server_reset( server, xwe, server->vol.comms );

            SETSTATE( server, XWSTATE_NEWCLIENT );
            util_requestTime( server->vol.util, xwe );
        }
    }
}
#endif

static void
cleanupServer( ServerCtxt* server )
{
    for ( XP_U16 ii = 0; ii < VSIZE(server->srvPlyrs); ++ii ){
        ServerPlayer* player = &server->srvPlyrs[ii];
        if ( player->engine != NULL ) {
            engine_destroy( player->engine );
        }
    }
    XP_MEMSET( server->srvPlyrs, 0, sizeof(server->srvPlyrs) );

    if ( server->pool != NULL ) {
        pool_destroy( server->pool );
        server->pool = (PoolContext*)NULL;
    }

    if ( !!server->nv.prevMoveStream ) {
        stream_destroy( server->nv.prevMoveStream );
    }
    if ( !!server->nv.prevWordsStream ) {
        stream_destroy( server->nv.prevWordsStream );
    }

    XP_FREEP( server->mpool, &server->nv.rematch.addrs );
    XP_FREEP( server->mpool, &server->nv.rematch.order );

    XP_MEMSET( &server->nv, 0, sizeof(server->nv) );
} /* cleanupServer */

void
server_reset( ServerCtxt* server, XWEnv xwe, CommsCtxt* comms )
{
    SRVR_LOGFF(" ");
    ServerVolatiles vol = server->vol;

    cleanupServer( server );

    vol.comms = comms;
    server->vol = vol;

    initServer( server, xwe );
} /* server_reset */

void
server_destroy( ServerCtxt* server )
{
    cleanupServer( server );

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

/* addMQTTDevID() and readMQTTDevID() exist to work around the case where
   folks start games using agreed-upon relay room names rather than
   invitations. In that case the MQTT devID hasn't been transmitted and so
   only old-style relay communication is possible. This hack sends the mqtt
   devIDs in the same host->guest message that sets the gameID. Guests will
   start using mqtt to transmit and in so doing transmit their own devIDs to
   the host.
*/
static void
addMQTTDevIDIf( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* stream )
{
    CommsAddrRec selfAddr = {};
    comms_getSelfAddr( server->vol.comms, &selfAddr );
    if ( addr_hasType( &selfAddr, COMMS_CONN_MQTT ) ) {
        MQTTDevID devID;
        dvc_getMQTTDevID( server->vol.dutil, xwe, &devID );

        XP_UCHAR buf[32];
        formatMQTTDevID( &devID, buf, VSIZE(buf) );
        stringToStream( stream, buf );
    }
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
                SRVR_LOGFF( "skipMQTTAdd: %s", boolToStr(server->nv.skipMQTTAdd) );
            } else {
                XP_PlayerAddr channelNo = stream_getAddress( stream );
                comms_addMQTTDevID( server->vol.comms, channelNo, &devID );
            }
        }
    }
}

/* Build a RematchInfo from the perspective of the guest we're sending it to,
   with the addresses of all the devices not it. Rather than include my
   address, which the guest knows already, add an empty address as a
   placeholder. Guest will replace it if needed. */
static void
buildGuestRI( const ServerCtxt* server, XWEnv xwe, XP_U16 guestIndex, RematchInfo* rip )
{
    XP_MEMSET( rip, 0, sizeof(*rip) );

    const CurGameInfo* gi = server->vol.gi;
    for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
        const LocalPlayer* lp = &gi->players[ii];
        if ( lp->isLocal ) {    /* that's me, the host */
            CommsAddrRec addr = {};
            ri_addAddrAt( server->vol.dutil, xwe, rip, &addr, ii );
        } else {
            XP_S8 deviceIndex = server->srvPlyrs[ii].deviceIndex;
            if ( guestIndex == deviceIndex ) {
                ri_addLocal( rip );
            } else {
                XP_PlayerAddr channelNo
                    = server->nv.addresses[deviceIndex].channelNo;
                CommsAddrRec addr;
                comms_getChannelAddr( server->vol.comms, channelNo, &addr );
                ri_addAddrAt( server->vol.dutil, xwe, rip, &addr, ii );
            }
        }
    }
    LOG_RI(rip);
}

static void
loadRemoteRI( const ServerCtxt* server, const CurGameInfo* XP_UNUSED_DBG(gi),
              RematchInfo* rip )
{
    XWStreamCtxt* tmpStream = mkServerStream( server, server->nv.streamVersion );
    stream_putBytes( tmpStream, server->nv.rematch.addrs, server->nv.rematch.addrsLen );

    ri_fromStream( rip, tmpStream, server );
    stream_destroy( tmpStream );

    /* Now find the unaddressed host and add its address */
    XP_ASSERT( rip->nPlayers == gi->nPlayers );

    ri_addHostAddrs( rip, server );

    LOG_RI( rip );
}

static void
addGuestAddrsIf( const ServerCtxt* server, XWEnv xwe, XP_U16 sendee,
                 XWStreamCtxt* stream )
{
    SRVR_LOGFF("(sendee: %d)", sendee );
    XP_ASSERT( amHost( server ) );
    XP_U16 version = stream_getVersion( stream );
    XP_ASSERT( version == server->nv.streamVersion );
    if ( STREAM_VERS_REMATCHADDRS <= version
         /* Not needed for two-device games */
         && 2 < server->nv.nDevices ) {
        XWStreamCtxt* tmpStream = mkServerStream( server, version );
        XP_Bool skipIt = XP_FALSE;

        if ( STREAM_VERS_REMATCHORDER <= version ) {
            RematchInfo ri;
            buildGuestRI( server, xwe, sendee, &ri );
            ri_toStream( tmpStream, &ri, server );

            /* Old verion requires no two-player devices  */
        } else if ( server->nv.nDevices == server->vol.gi->nPlayers ) {
            for ( XP_U16 devIndex = 1; devIndex < server->nv.nDevices; ++devIndex ) {
                if ( devIndex == sendee ) {
                    continue;
                }
                XP_PlayerAddr channelNo
                    = server->nv.addresses[devIndex].channelNo;
                CommsAddrRec addr;
                comms_getChannelAddr( server->vol.comms, channelNo, &addr );
                addrToStream( tmpStream, &addr );
            }
        } else {
            skipIt = XP_TRUE;
        }
        if ( !skipIt ) {
            XP_U16 len = stream_getSize( tmpStream );
            stream_putU32VL( stream, len );
            stream_putBytes( stream, stream_getPtr(tmpStream), len );
        }
        stream_destroy( tmpStream );
    }
}

static void
readGuestAddrs( ServerCtxt* server, XWStreamCtxt* stream, XP_U8 streamVersion )
{
    SRVR_LOGFFV( "version: 0x%X", streamVersion );
    if ( STREAM_VERS_REMATCHADDRS <= streamVersion && 0 < stream_getSize(stream) ) {
        XP_U16 len = server->nv.rematch.addrsLen = stream_getU32VL( stream );
        SRVR_LOGFFV( "rematch.addrsLen: %d", server->nv.rematch.addrsLen );
        if ( 0 < len ) {
            XP_ASSERT( !server->nv.rematch.addrs );
            server->nv.rematch.addrs = XP_MALLOC( server->mpool, len );
            stream_getBytes( stream, server->nv.rematch.addrs, len );
            SRVR_LOGFFV( "loaded %d bytes of rematch.addrs", len );
#ifdef DEBUG
            XWStreamCtxt* tmpStream = mkServerStream( server, streamVersion );
            stream_putBytes( tmpStream, server->nv.rematch.addrs,
                             server->nv.rematch.addrsLen );

            if ( STREAM_VERS_REMATCHORDER <= streamVersion ) {
                RematchInfo ri;
                ri_fromStream( &ri, tmpStream, server );
                for ( int ii = 0; ii < ri.nAddrs; ++ii ) {
                    SRVR_LOGFFV( "got an address" );
                    logAddr( server->vol.dutil, &ri.addrs[ii], __func__ );
                }
            } else {
                while ( 0 < stream_getSize(tmpStream) ) {
                    CommsAddrRec addr = {};
                    addrFromStream( &addr, tmpStream );
                    SRVR_LOGFFV( "got an address" );
                    logAddr( server->vol.dutil, &addr, __func__ );
                }
            }
            stream_destroy( tmpStream );
#endif
        }
    }
    LOG_RETURN_VOID();
}

XP_Bool
server_initClientConnection( ServerCtxt* server, XWEnv xwe )
{
    XP_Bool result;
    SRVR_LOGFFV( "gameState: %s; ", getStateStr(server->nv.gameState) );
    CurGameInfo* gi = server->vol.gi;
    XP_U16 nPlayers;
    LocalPlayer* lp;
#ifdef DEBUG
    XP_U16 ii = 0;
#endif

    XP_ASSERT( gi->serverRole == SERVER_ISCLIENT );
    result = server->nv.gameState == XWSTATE_NONE;
    if ( result ) {
        XWStreamCtxt* stream = messageStreamWithHeader( server, xwe, HOST_DEVICE,
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
            SRVR_LOGFFV( "wrote local name %s", name );
        }
#ifdef STREAM_VERS_BIGBOARD
        stream_putU8( stream, CUR_STREAM_VERS );
#endif
        stream_destroy( stream );
    } else {
        SRVR_LOGFF( "wierd state: %s (expected XWSTATE_NONE); doing nothing",
                    getStateStr(server->nv.gameState) );
    }
    SRVR_LOGFF( "=> %s", boolToStr(result) );
    return result;
} /* server_initClientConnection */

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
        stream_destroy( stream );
    } else {
        SRVR_LOGFF( "dropping chat %s; queue too full?", msg );
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
server_sendChat( ServerCtxt* server, XWEnv xwe, const XP_UCHAR* msg,
                 XP_S16 fromHint )
{
    const CurGameInfo* gi = server->vol.gi;
    /* The player sending must be local. Caller (likely board) tells us what
       player is selected, which is who the sender should be IFF it's a local
       player, but once the game's over it might not be. */
    fromHint = gi_getLocalPlayer( gi, fromHint );

    XP_ASSERT( gi->players[fromHint].isLocal );
    XP_U32 timestamp = dutil_getCurSeconds( server->vol.dutil, xwe );
    if ( gi->serverRole == SERVER_ISCLIENT ) {
        sendChatTo( server, xwe, HOST_DEVICE, msg, fromHint, timestamp );
    } else {
        sendChatToClientsExcept( server, xwe, HOST_DEVICE, msg, fromHint,
                                 timestamp );
    }
}

static XP_Bool
receiveChat( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* incoming )
{
    XP_UCHAR* msg = stringFromStream( server->mpool, incoming );
    XP_S16 from = -1;
    if ( 1 <= stream_getSize( incoming ) ) {
        from = stream_getU8( incoming );
        if ( server->vol.gi->players[from].isLocal ) { /* fail.... */
            from = -1;          /* means it's wrong/unknown */
        }
    }

    XP_U32 timestamp = sizeof(timestamp) <= stream_getSize( incoming )
        ? stream_getU32( incoming ) : 0;
    if ( amHost( server ) ) {
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
        SRVR_LOGFF( "no listener!!" );
    }
}

# ifdef STREAM_VERS_BIGBOARD
static void
setStreamVersion( ServerCtxt* server )
{
    XP_ASSERT( amHost( server ) );
    XP_U8 streamVersion = CUR_STREAM_VERS;
    for ( XP_U16 devIndex = 1; devIndex < server->nv.nDevices; ++devIndex ) {
        XP_U8 devVersion = server->nv.addresses[devIndex].streamVersion;
        if ( devVersion < streamVersion ) {
            streamVersion = devVersion;
        }
    }
    SRVR_LOGFFV( "setting streamVersion: 0x%x", streamVersion );
    server->nv.streamVersion = streamVersion;

    /* If we're downgrading stream to accomodate an older guest, we need to
       re-write gi in that version in case there are newer features the game
       can't support, e.g. allowing to trade with less than a full tray left
       in the pool. */
    if ( CUR_STREAM_VERS != streamVersion ) {
        CurGameInfo* gi = server->vol.gi;
        XP_U16 oldTraySize = gi->traySize;
        XP_U16 oldBingoMin = gi->bingoMin;

        XWStreamCtxt* tmp = mkServerStream( server, streamVersion );
        gi_writeToStream( tmp, gi );
        gi_disposePlayerInfo( MPPARM(server->mpool) gi );
        gi_readFromStream( MPPARM(server->mpool) tmp, gi );
        stream_destroy( tmp );
        /* If downgrading forced tray size change, model needs to know. BUT:
           the guest would have to be >two years old now for this to happen. */
        if ( oldTraySize != gi->traySize || oldBingoMin != gi->bingoMin ) {
            XP_ASSERT( 7 == gi->traySize && 7 == gi->bingoMin );
            if ( 7 == gi->traySize && 7 == gi->bingoMin ) {
                model_forceStack7Tiles( server->vol.model );
            }
        }
    }
}

static void
checkResizeBoard( ServerCtxt* server )
{
    CurGameInfo* gi = server->vol.gi;
    if ( STREAM_VERS_BIGBOARD > server->nv.streamVersion && gi->boardSize > 15) {
        SRVR_LOGFF( "dropping board size from %d to 15", gi->boardSize );
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
        SRVR_LOGFF( "got %d players but missing only %d",
                    playersInMsg, server->nv.pendingRegistrations );
        util_userError( server->vol.util, xwe, ERR_REG_UNEXPECTED_USER );
        sts_increment( server->vol.dutil, xwe, STAT_REG_NOROOM );
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
                SRVR_LOGFF( "setting addresses[%d] streamVersion to 0x%x "
                            "(CUR_STREAM_VERS is 0x%x)",
                            clientIndex, streamVersion, CUR_STREAM_VERS );
                server->nv.addresses[clientIndex].streamVersion = streamVersion;
            }
        }
#endif

        if ( server->nv.pendingRegistrations == 0 ) {
            XP_ASSERT( ii == playersInMsg ); /* otherwise malformed */
            setStreamVersion( server );
            checkResizeBoard( server );
            (void)assignTilesToAll( server, xwe );
            /* We won't need this any more */
            XP_FREEP( server->mpool, &server->nv.rematch.order );
            server->nv.flags &= ~MASK_HAVE_RIP_INFO;
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

    XWStreamCtxt* stream = mkServerStream0( server );
    stream_catString( stream, buf );

    server->nv.prevMoveStream = stream;
    server->vol.showPrevMove = XP_TRUE;
}

static void
dupe_setupShowMove( ServerCtxt* server, XWEnv xwe, XP_U16* scores )
{
    XP_ASSERT( inDuplicateMode(server) );
    // XP_ASSERT( !server->nv.prevMoveStream ); /* firing */

    const CurGameInfo* gi = server->vol.gi;
    const XP_U16 nPlayers = gi->nPlayers;

    XWStreamCtxt* stream = mkServerStream0( server );

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
    SRVR_LOGFF( "reading %d moves", movesInMsg );
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
    SRVR_LOGFF( "(%d)", newVal );
    if ( newVal != server->nv.dupTimerExpires ) {
        XP_S32 oldVal = server->nv.dupTimerExpires;
        server->nv.dupTimerExpires = newVal;
        callDupTimerListener( server, xwe, oldVal, newVal );
    }
}

static void
resetDupeTimerIf( ServerCtxt* server, XWEnv xwe )
{
    if ( inDuplicateMode( server ) ) {
        dupe_resetTimer( server, xwe );
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
        SRVR_LOGFF( "doing nothing because timers disabled" );
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
    SRVR_LOGFF( "(turn=%d)", turn );
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
    SRVR_LOGFF( "(turn=%d)", turn );
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
mkServerStream0( const ServerCtxt* server )
{
    return mkServerStream( server, 0 );
}

static XWStreamCtxt*
mkServerStream( const ServerCtxt* server, XP_U8 version )
{
    XWStreamCtxt* stream =
        mem_stream_make_raw( MPPARM(server->mpool)
                             dutil_getVTManager(server->vol.dutil) );
    XP_ASSERT( !!stream );
    stream_setVersion( stream, version );
    return stream;
} /* mkServerStream */

static XP_Bool
makeRobotMove( ServerCtxt* server, XWEnv xwe )
{
    LOG_FUNC();
    XP_Bool result = XP_FALSE;
    XP_Bool searchComplete = XP_FALSE;
    XP_S16 turn;
    MoveInfo newMove = {};
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
            stream = mkServerStream0( server );
        }

        /* trade if unable to find a move */
        if ( trade ) {
            TrayTileSet oldTiles = *model_getPlayerTiles( model, turn );
            SRVR_LOGFF( "robot trading %d tiles", oldTiles.nTiles );
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
                // XP_ASSERT( !server->nv.prevMoveStream );
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
                SRVR_LOGFF( "robot making %d tile move for player %d",
                            newMove.nTiles, turn );

                if ( !!stream ) {
                    XWStreamCtxt* wordsStream = mkServerStream0( server );
                    WordNotifierInfo* ni = 
                        model_initWordCounter( model, wordsStream );
                    (void)model_checkMoveLegal( model, xwe, turn, stream, ni );
                    // XP_ASSERT( !server->nv.prevMoveStream );
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

        stream = mkServerStream0( server );
        stream_catString( stream, str );

        XWStreamCtxt* prevStream = server->nv.prevMoveStream;
        if ( !!prevStream ) {
            server->nv.prevMoveStream = NULL;

            XP_U16 len = stream_getSize( prevStream );
            stream_putBytes( stream, stream_getPtr( prevStream ), len );
            stream_destroy( prevStream );
        }

        util_informMove( server->vol.util, xwe, prevTurn, stream,
                         server->nv.prevWordsStream );
        stream_destroy( stream );

        if ( !!server->nv.prevWordsStream ) {
            stream_destroy( server->nv.prevWordsStream );
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
                &newTiles, XP_FALSE );
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
            SRVR_LOGFF( "already asking for %d", turn );
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
        SRVR_LOGFFV( "gameState: %s", getStateStr(server->nv.gameState) );
        switch( server->nv.gameState ) {
        case XWSTATE_BEGIN:
            if ( server->nv.pendingRegistrations == 0 ) { /* all players on
                                                             device */
                if ( assignTilesToAll( server, xwe ) ) {
                    SETSTATE( server, XWSTATE_INTURN );
                    setTurn( server, xwe, 0 );
                    resetDupeTimerIf( server, xwe );
                    moreToDo = XP_TRUE;
                }
            }
            break;

        case XWSTATE_NEWCLIENT:
            XP_ASSERT( !amHost( server ) );
            SETSTATE( server, XWSTATE_NONE ); /* server_initClientConnection expects this */
            server_initClientConnection( server, xwe );
            break;

        case XWSTATE_NEEDSEND_BADWORD_INFO:
            XP_ASSERT( server->vol.gi->serverRole == SERVER_ISHOST );
            badWordMoveUndoAndTellUser( server, xwe, &server->bws );
            sendBadWordMsgs( server, xwe );
            nextTurn( server, xwe, PICK_NEXT );
            //moreToDo = XP_TRUE;   /* why? */
            break;

        case XWSTATE_RECEIVED_ALL_REG:
            sendInitialMessage( server, xwe );
            /* PENDING isn't INTURN_OFFDEVICE possible too?  Or just
               INTURN?  */
            SETSTATE( server, XWSTATE_INTURN );
            setTurn( server, xwe, 0 );
            moreToDo = XP_TRUE;
            server->nv.flags |= FLAG_HARVEST_READY;
            break;

        case XWSTATE_MOVE_CONFIRM_MUSTSEND:
            XP_ASSERT( server->vol.gi->serverRole == SERVER_ISHOST );
            tellMoveWasLegal( server, xwe ); /* sets state */
            nextTurn( server, xwe, PICK_NEXT );
            break;

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

static XP_S8
getIndexForStream( const ServerCtxt* server, const XWStreamCtxt* stream )
{
    XP_PlayerAddr channelNo = stream_getAddress( stream );
    return getIndexForDevice( server, channelNo );
}

static XP_S8
getIndexForDevice( const ServerCtxt* server, XP_PlayerAddr channelNo )
{
    XP_S8 result = -1;

    for ( int ii = 0; ii < server->nv.nDevices; ++ii ) {
        const RemoteAddress* addr = &server->nv.addresses[ii];
        if ( addr->channelNo == channelNo ) {
            result = (XP_S8)ii;
            break;
        }
    }

    SRVR_LOGFFV( "(%x)=>%d", channelNo, result );
    return result;
} /* getIndexForDevice */

static XP_Bool
findFirstPending( ServerCtxt* server, ServerPlayer** spp,
                   LocalPlayer** lpp )
{
    /* We want to find the local player and srvPlyrs slot for this
       connection. There's a srvPlyrs slot for each client that will
       register. For each we find in use, skip a non-local slot in the gi. */
    CurGameInfo* gi = server->vol.gi;
    XP_Bool success = XP_FALSE;
    for ( int ii = 0; !success && ii < gi->nPlayers; ++ii ) {
        LocalPlayer* lp = &gi->players[ii];
        if ( !lp->isLocal ) {
            ServerPlayer* sp = &server->srvPlyrs[ii];
            XP_ASSERT( HOST_DEVICE != sp->deviceIndex );
            if ( UNKNOWN_DEVICE == sp->deviceIndex ) {
                success = XP_TRUE;
                *lpp = lp;
                *spp = sp;
            }
        }
    }
    return success;
} /* findFirstPending */

static XP_Bool
findOrderedSlot( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* stream,
                 ServerPlayer** spp, LocalPlayer** lpp )
{
    LOG_FUNC();
    CurGameInfo* gi = server->vol.gi;
    XP_PlayerAddr channelNo = stream_getAddress( stream );
    CommsAddrRec guestAddr;
    const CommsCtxt* comms = server->vol.comms;
    comms_getChannelAddr( comms, channelNo, &guestAddr );

    const RematchInfo* rip = server->nv.rematch.order;
    LOG_RI(rip);

    XP_Bool success = XP_FALSE;

    /* We have an incoming player with an address. We want to find the first
       open slot (a srvPlyrs entry where deviceIndex is -1) where the
       corresponding entry in the RematchInfo points to the same address.
    */

    for ( int ii = 0; !success && ii < gi->nPlayers; ++ii ) {
        ServerPlayer* sp = &server->srvPlyrs[ii];
        SRVR_LOGFFV( "ii: %d; deviceIndex: %d", ii, sp->deviceIndex );
        if ( UNKNOWN_DEVICE == sp->deviceIndex ) {
            int addrIndx = rip->addrIndices[ii];
            if ( addrsAreSame( server->vol.dutil, xwe, &guestAddr,
                               &rip->addrs[addrIndx] ) ) {
                *spp = sp;
                *lpp = &gi->players[ii];
                XP_ASSERT( !(*lpp)->isLocal );
                success = XP_TRUE;
            }
        }
    }

    SRVR_LOGFFV( "()=>%s", boolToStr(success) );
    return success;
}

static XP_S8
registerRemotePlayer( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* stream )
{
    XP_S8 deviceIndex = -1;

    /* The player must already be there with a null name, or it's an error.
       Take the first empty slot. */
    XP_ASSERT( server->nv.pendingRegistrations > 0 );

    /* find the slot to use */
    ServerPlayer* sp;
    LocalPlayer* lp;
    XP_Bool success;
    if ( server_isFromRematch( server ) ) {
        success = findOrderedSlot( server, xwe, stream, &sp, &lp );
    } else {
        success = findFirstPending( server, &sp, &lp );
    }

    if ( success ) {
        /* get data from stream */
        lp->robotIQ = 1 == stream_getBits( stream, 1 )? 1 : 0;
        XP_U16 nameLen = stream_getBits( stream, NAME_LEN_NBITS );
        XP_UCHAR name[nameLen + 1];
        stream_getBytes( stream, name, nameLen );
        name[nameLen] = '\0';
        SRVR_LOGFF( "read remote name: %s", name );

        replaceStringIfDifferent( server->mpool, &lp->name, name );

        XP_PlayerAddr channelNo = stream_getAddress( stream );
        deviceIndex = getIndexForDevice( server, channelNo );

        --server->nv.pendingRegistrations;

        if ( deviceIndex == -1 ) {
            RemoteAddress* addr = &server->nv.addresses[server->nv.nDevices];

            XP_ASSERT( channelNo != 0 );
            addr->channelNo = channelNo;
            SRVR_LOGFF( "set channelNo to %x for device %d",
                        channelNo, server->nv.nDevices );

            deviceIndex = server->nv.nDevices++;
#ifdef STREAM_VERS_BIGBOARD
            addr->streamVersion = STREAM_SAVE_PREVWORDS;
#endif
        } else {
            SRVR_LOGFF( "deviceIndex already set" );
        }

        sp->deviceIndex = deviceIndex;

        informMissing( server, xwe );
    }
    return deviceIndex;
} /* registerRemotePlayer */

static void
sortTilesIf( ServerCtxt* server, XP_S16 turn )
{
    if ( server->nv.sortNewTiles ) {
        model_sortTiles( server->vol.model, turn );
    }
}

/* Called in response to message from server listing all the names of
 * players in the game (in server-assigned order) and their initial
 * tray contents.
 */
static XP_Bool
client_readInitialMessage( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* stream )
{
    LOG_FUNC();
    XP_Bool accepted = 0 == server->nv.addresses[0].channelNo;
    XP_ASSERT( accepted );

    /* We should never get this message a second time, but very rarely we do.
       Drop it in that case. */
    if ( accepted ) {
        ModelCtxt* model = server->vol.model;
        CommsCtxt* comms = server->vol.comms;
        CurGameInfo* gi = server->vol.gi; /* we'll overwrite this */
        XP_U32 gameID = 0;
        PoolContext* pool;
#ifdef STREAM_VERS_BIGBOARD
        XP_UCHAR rmtDictName[128];
        XP_UCHAR rmtDictSum[64];
#endif

        /* version; any dependencies here? */
        XP_U8 streamVersion = stream_getU8( stream );
        SRVR_LOGFF( "set streamVersion to 0x%X", streamVersion );
        stream_setVersion( stream, streamVersion );
        if ( STREAM_VERS_NINETILES > streamVersion ) {
            model_forceStack7Tiles( server->vol.model );
        }
        // XP_ASSERT( streamVersion <= CUR_STREAM_VERS ); /* else do what? */

        gameID = streamVersion < STREAM_VERS_REMATCHORDER
            ? stream_getU32( stream ) : 0;
        CurGameInfo localGI = {};
        gi_readFromStream( MPPARM(server->mpool) stream, &localGI );
        XP_ASSERT( gameID == 0 || gameID == localGI.gameID );
        gameID = localGI.gameID;
        localGI.serverRole = SERVER_ISCLIENT;

        /* never seems to replace anything -- gi is already correct on guests
           apparently. How? Will have come in with invitation, of course. */
        SRVR_LOGFFV( "read gameID of %08X; calling comms_setConnID (replacing %08X)",
                    gameID, gi->gameID );
        XP_ASSERT( gi->gameID == gameID );
        gi->gameID = gameID;
        comms_setConnID( comms, gameID, streamVersion );

        XP_ASSERT( !localGI.dictName );
        localGI.dictName = copyString( server->mpool, gi->dictName );
        gi_copy( MPPARM(server->mpool) gi, &localGI );
        server->nv.flags |= FLAG_HARVEST_READY;

        if ( streamVersion < STREAM_VERS_NOEMPTYDICT ) {
            SRVR_LOGFF( "loading and dropping empty dict" );
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
        SRVR_LOGFF( "assigning channelNo %x for 0", channelNo );

        model_setSize( model, localGI.boardSize );

        XP_U16 nPlayers = localGI.nPlayers;
        SRVR_LOGFFV( "reading in %d players", localGI.nPlayers );

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
            SRVR_LOGFFV( "got %d tiles for player %d", tiles.nTiles, ii );

            if ( inDuplicateMode(server ) ) {
                model_assignDupeTiles( model, xwe, &tiles );
                break;
            } else {
                model_assignPlayerTiles( model, ii, &tiles );
            }

            sortTilesIf( server, ii );
        }

        readMQTTDevID( server, stream );
        readGuestAddrs( server, stream, stream_getVersion( stream ) );

        syncPlayers( server );

        SETSTATE( server, XWSTATE_INTURN );

        /* Give board a chance to redraw self with the full compliment of known
           players */
        informMissing( server, xwe );
        setTurn( server, xwe, 0 );
        resetDupeTimerIf( server, xwe );
    }
    return accepted;
} /* client_readInitialMessage */

/* For each remote device, send a message containing the dictionary and the
 * names of all the players in the game (including those on the device itself,
 * since they might have been changed in the case of conflicts), in the order
 * that all must use for the game.  Then for each player on the device give
 * the starting tray.
 */
static void
makeSendableGICopy( ServerCtxt* server, CurGameInfo* giCopy, 
                    XP_U16 deviceIndex )
{
    XP_MEMCPY( giCopy, server->vol.gi, sizeof(*giCopy) );

    for ( int ii = 0; ii < giCopy->nPlayers; ++ii ) {
        LocalPlayer* lp = &giCopy->players[ii];

        /* adjust isLocal to client's perspective */
        lp->isLocal = server->srvPlyrs[ii].deviceIndex == deviceIndex;
    }

    giCopy->forceChannel = deviceIndex;
    SRVR_LOGFFV( "assigning forceChannel from deviceIndex: %d",
                giCopy->forceChannel );

    giCopy->dictName = (XP_UCHAR*)NULL; /* so we don't sent the bytes */
    LOGGI( giCopy, "after" );
} /* makeSendableGICopy */

static void
sendInitialMessage( ServerCtxt* server, XWEnv xwe )
{
    ModelCtxt* model = server->vol.model;
    XP_U16 nPlayers = server->vol.gi->nPlayers;
    XP_U32 gameID = server->vol.gi->gameID;
#ifdef STREAM_VERS_BIGBOARD
    XP_U8 streamVersion = server->nv.streamVersion;
    SRVR_LOGFF( "streamVersion: 0x%X", streamVersion );
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

        if ( streamVersion < STREAM_VERS_REMATCHORDER ) {
            stream_putU32( stream, gameID );
        }

        CurGameInfo localGI;
        makeSendableGICopy( server, &localGI, deviceIndex );
        gi_writeToStream( stream, &localGI );

        const DictionaryCtxt* dict = model_getDictionary( model );
        if ( streamVersion < STREAM_VERS_NOEMPTYDICT ) {
            SRVR_LOGFFV( "writing dict to stream" );
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

        addMQTTDevIDIf( server, xwe, stream );
        addGuestAddrsIf( server, xwe, deviceIndex, stream );

        stream_destroy( stream );
    }

    /* Set after messages are built so their connID will be 0, but all
       non-initial messages will have a non-0 connID. */
    comms_setConnID( server->vol.comms, gameID, streamVersion );

    resetDupeTimerIf( server, xwe );
} /* sendInitialMessage */

static void
freeBWS( MPFORMAL BadWordsState* bws )
{
    BadWordInfo* bwi = &bws->bwi;
    XP_U16 nWords = bwi->nWords;

    XP_FREEP( mpool, &bws->dictName );
    while ( nWords-- ) {
        XP_FREEP( mpool, &bwi->words[nWords] );
    }

    bwi->nWords = 0;
} /* freeBWI */

static void
bwsToStream( XWStreamCtxt* stream, const BadWordsState* bws )
{
    const XP_U16 nWords = bws->bwi.nWords;

    stream_putBits( stream, 4, nWords );
    if ( STREAM_VERS_DICTNAME <= stream_getVersion( stream ) ) {
        stringToStream( stream, bws->dictName );
    }
    for ( int ii = 0; ii < nWords; ++ii ) {
        stringToStream( stream, bws->bwi.words[ii] );
    }

} /* bwsToStream */

static void
bwsFromStream( MPFORMAL XWStreamCtxt* stream, BadWordsState* bws )
{
    XP_U16 nWords = stream_getBits( stream, 4 );
    XP_ASSERT( nWords < VSIZE(bws->bwi.words) - 1 );

    bws->bwi.nWords = nWords;
    if ( STREAM_VERS_DICTNAME <= stream_getVersion( stream ) ) {
        bws->dictName = stringFromStream( mpool, stream );
    }
    for ( int ii = 0; ii < nWords; ++ii ) {
        bws->bwi.words[ii] = (const XP_UCHAR*)stringFromStream( mpool, stream );
    }
    bws->bwi.words[nWords] = NULL;
} /* bwsFromStream */

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

const XP_UCHAR*
RO2Str( RematchOrder ro )
{
    const char* str = (char*)NULL;
    switch( ro ) {
        caseStr(RO_NONE);
        caseStr(RO_SAME);
        caseStr(RO_LOW_SCORE_FIRST);
        caseStr(RO_HIGH_SCORE_FIRST);
        caseStr(RO_JUGGLE);
#ifdef XWFEATURE_RO_BYNAME
        caseStr(RO_BY_NAME);
#endif
        caseStr(RO_NUM_ROS);    /* should never print!!! */
    }
    return str;
}

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
    writeProto( server, stream, code );

    return stream;
} /* messageStreamWithHeader */

static void
sendBadWordMsgs( ServerCtxt* server, XWEnv xwe )
{
    XP_ASSERT( server->bws.bwi.nWords > 0 );

    if ( server->bws.bwi.nWords > 0 ) { /* fail gracefully */
        XWStreamCtxt* stream = 
            messageStreamWithHeader( server, xwe, server->lastMoveSource,
                                     XWPROTO_BADWORD_INFO );
        stream_putBits( stream, PLAYERNUM_NBITS, server->nv.currentTurn );

        bwsToStream( stream, &server->bws );

        /* XP_U32 hash = model_getHash( server->vol.model ); */
        /* stream_putU32( stream, hash ); */
        /* XP_LOGFF( "wrote hash: %X", hash ); */

        stream_destroy( stream );

        freeBWS( MPPARM(server->mpool) &server->bws );
    }
    SETSTATE( server, XWSTATE_INTURN );
} /* sendBadWordMsgs */

static void
badWordMoveUndoAndTellUser( ServerCtxt* server, XWEnv xwe,
                            const BadWordsState* bws )
{
    XP_U16 turn;
    ModelCtxt* model = server->vol.model;
    /* I'm the server.  I need to send a message to everybody else telling
       them the move's rejected.  Then undo it on this side, replacing it with
       model_commitRejectedPhony(); */

    model_rejectPreviousMove( model, xwe, server->pool, &turn );

    util_notifyIllegalWords( server->vol.util, xwe, &bws->bwi,
                             bws->dictName, turn, XP_TRUE, 0 );
} /* badWordMoveUndoAndTellUser */

EngineCtxt*
server_getEngineFor( ServerCtxt* server, XP_U16 playerNum )
{
    const CurGameInfo* gi = server->vol.gi;
    XP_ASSERT( playerNum < gi->nPlayers );

    ServerPlayer* player = &server->srvPlyrs[playerNum];
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
    ServerPlayer* player = &server->srvPlyrs[playerNum];
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

/* trayAllowsMoves()
 *
 * Assuming a model with a turn loaded (but maybe not committed), build the
 * tile set containing the current model tray tiles PLUS the new set we're
 * considering, and see if the engine can find moves.
 */
static XP_Bool
trayAllowsMoves( ServerCtxt* server, XWEnv xwe, XP_U16 turn,
                      const Tile* tiles, XP_U16 nTiles )
{
    ModelCtxt* model = server->vol.model;
    XP_U16 nInTray = model_getNumTilesInTray( model, turn );
    SRVR_LOGFF( "(nTiles=%d): nInTray: %d", nTiles, nInTray );
    XP_ASSERT( nInTray + nTiles <= MAX_TRAY_TILES ); /* fired again! */
    Tile tmpTiles[MAX_TRAY_TILES];
    const TrayTileSet* tray = model_getPlayerTiles( model, turn );
    XP_MEMCPY( tmpTiles, &tray->tiles[0], nInTray * sizeof(tmpTiles[0]) );
    XP_MEMCPY( &tmpTiles[nInTray], &tiles[0], nTiles * sizeof(tmpTiles[0]) );

    /* XP_LOGFF( "%s(nTiles=%d)", __func__, nTiles ); */
    EngineCtxt* tmpEngine = NULL;
    EngineCtxt* engine = server_getEngineFor( server, turn );
    if ( !engine ) {
        tmpEngine = engine = engine_make( MPPARM(server->mpool) server->vol.util );
    }
    XP_Bool canMove;
    MoveInfo newMove = {};
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
        SRVR_LOGFFV( "first move found has score of %d", score );
    } else {
        SRVR_LOGFF( "no moves found for tray!!!" );
    }

    if ( !!tmpEngine ) {
        engine_destroy( tmpEngine );
    } else {
        server_resetEngine( server, turn );
    }

    return result;
}

static void
fetchTiles( ServerCtxt* server, XWEnv xwe, XP_U16 playerNum, XP_U16 nToFetch,
            TrayTileSet* resultTiles, XP_Bool forceCanPlay /* First player shouldn't have unplayable rack*/ )
{
    XP_ASSERT( server->vol.gi->serverRole != SERVER_ISCLIENT || !inDuplicateMode(server) );
    XP_U16 nSoFar = resultTiles->nTiles;
    PoolContext* pool = server->pool;

    XP_ASSERT( !!pool );
    
    XP_U16 nLeftInPool = pool_getNTilesLeft( pool );
    if ( nLeftInPool < nToFetch ) {
        XP_LOGFF( "dropping nToFetch from %d to %d", nToFetch, nLeftInPool );
        nToFetch = nLeftInPool;
    }

    /* Then fetch the rest without asking. But if we're in duplicate mode,
       make sure the tray allows some moves (e.g. isn't all consonants when
       the board's empty.) */
    if ( nToFetch >= nSoFar ) {
        XP_U16 nLeft = nToFetch - nSoFar;
        for ( XP_U16 nBadTrays = 0; 0 < nLeft; ) {
            pool_requestTiles( pool, &resultTiles->tiles[nSoFar], &nLeft );

            if ( !inDuplicateMode( server ) && !forceCanPlay ) {
                break;
            } else if ( trayAllowsMoves( server, xwe, playerNum, &resultTiles->tiles[0],
                                         nSoFar + nLeft )
                        || ++nBadTrays >= 5 ) {
                break;
            }
            pool_replaceTiles2( pool, nLeft, &resultTiles->tiles[nSoFar] );
        }

        nSoFar += nLeft;
    }
   
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
                fetchTiles( server, xwe, ii, numAssigned, &newTiles, ii == 0 );
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

static void
getPlayerTime( ServerCtxt* server, XWStreamCtxt* stream, XP_U16 turn )
{
    CurGameInfo* gi = server->vol.gi;

    if ( gi->timerEnabled ) {
        XP_U16 secondsUsed = stream_getU16( stream );

        gi->players[turn].secondsUsed = secondsUsed;
    }
} /* getPlayerTime */

static void
nextTurn( ServerCtxt* server, XWEnv xwe, XP_S16 nxtTurn )
{
    SRVR_LOGFFV( "(nxtTurn=%d)", nxtTurn );
    CurGameInfo* gi = server->vol.gi;
    XP_S16 currentTurn = server->nv.currentTurn;
    XP_Bool moreToDo = XP_FALSE;

    if ( nxtTurn == PICK_CUR ) {
        nxtTurn = model_getNextTurn( server->vol.model );
    } else if ( nxtTurn == PICK_NEXT ) {
        XP_ASSERT( server->nv.gameState == XWSTATE_INTURN );
        if ( server->nv.gameState != XWSTATE_INTURN ) {
            SRVR_LOGFF( "doing nothing; state %s != XWSTATE_INTURN",
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
            SRVR_LOGFF( "turn == -1 so dropping" );
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
            SRVR_LOGFF( "Doing nothing; waiting for server to end game" );
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
        const XP_UCHAR* dictName = dict_getShortName( wnp->dict );

        XP_LOGFF( "storeBadWords called with \"%s\" (name=%s)", wnp->word,
                  dictName );
        if ( NULL == server->bws.dictName ) {
            server->bws.dictName = copyString( server->mpool, dictName );
        }
        BadWordInfo* bwi = &server->bws.bwi;
        bwi->words[bwi->nWords++] = copyString( server->mpool, wnp->word );
    }
} /* storeBadWords */

static XP_Bool
checkMoveAllowed( ServerCtxt* server, XWEnv xwe, XP_U16 playerNum )
{
    CurGameInfo* gi = server->vol.gi;
    XP_ASSERT( server->bws.bwi.nWords == 0 );

    if ( gi->phoniesAction == PHONIES_DISALLOW ) {
        WordNotifierInfo info;
        info.proc = storeBadWords;
        info.closure = server;
        (void)model_checkMoveLegal( server->vol.model, xwe, playerNum,
                                    (XWStreamCtxt*)NULL, &info );
    }

    return server->bws.bwi.nWords == 0;
} /* checkMoveAllowed */

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
        SRVR_LOGFF( "adding hash %X", (unsigned int)hash );
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
            SRVR_LOGFFV( "wrote secondsUsed for player %d: %d",
                         turn, gi->players[turn].secondsUsed );
        } else {
            XP_ASSERT( gi->players[turn].secondsUsed == 0 );
        }

        if ( !legal ) {
            XP_ASSERT( server->bws.bwi.nWords > 0 );
            stream_putBits( stream, PLAYERNUM_NBITS, turn );
            bwsToStream( stream, &server->bws );
        }
    }

    stream_destroy( stream );
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
            SRVR_LOGFF( "hash mismatch: %X not found", hashReceived );
            *badStackP = XP_TRUE;
#ifdef DEBUG_HASHING
        } else {
            SRVR_LOGFF( "hash match: %X", hashReceived );
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
                SRVR_LOGFFV( "got trade of %d tiles", tradedTiles->nTiles );
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
        stream = mkServerStream0( server );
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
        stream = mkServerStream0( server );
        *wordsStream = mkServerStream0( server );
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

    XP_ASSERT( gi->serverRole == SERVER_ISHOST );

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
                // XP_ASSERT( !server->nv.prevMoveStream );
                server->nv.prevMoveStream = mvStream;
                // XP_ASSERT( !server->nv.prevWordsStream );
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
        SRVR_LOGFF( "BAD: game state: %s, not XWSTATE_INTURN", getStateStr(server->nv.gameState ) );
    } else if ( server->nv.currentTurn < 0 ) {
        SRVR_LOGFF( "BAD: currentTurn %d < 0", server->nv.currentTurn );
    } else if ( ! readMoveInfo( server, xwe, stream, &whoMoved, &isTrade,
                                &newTiles, &tradedTiles, &isLegal, &badStack ) ) { /* modifies model */
        SRVR_LOGFF( "BAD: readMoveInfo() failed" );
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
            // XP_ASSERT( !server->nv.prevMoveStream );
            server->nv.prevMoveStream = mvStream;
            // XP_ASSERT( !server->nv.prevWordsStream );
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
        SRVR_LOGFF( "no scoring move found" );
    } else {
        SRVR_LOGFF( "%d wins with %d points", *winner, scores[*winner] );
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
    stream_destroy( stream );
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

    TrayTileSet newTiles = {};
    fetchTiles( server, xwe, DUP_PLAYER, oldTiles.nTiles, &newTiles, XP_FALSE );

    model_commitDupeTrade( model, &oldTiles, &newTiles );

    model_addNewTiles( model, DUP_PLAYER, &newTiles );
    updateOthersTiles( server, xwe );

    if ( server->vol.gi->serverRole == SERVER_ISHOST ) {
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

        stream_destroy( tmpStream );
    }

    dupe_resetTimer( server, xwe );

    dupe_setupShowTrade( server, xwe, newTiles.nTiles );
    LOG_RETURN_VOID();
} /* dupe_makeAndReportTrade */

static void
dupe_transmitPause( ServerCtxt* server, XWEnv xwe, DupPauseType typ, XP_U16 turn,
                    const XP_UCHAR* msg, XP_S16 skipDev )
{
    SRVR_LOGFF( "(type=%d, msg=%s)", typ, msg );
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
            sendStreamToDev( server, xwe, HOST_DEVICE, XWPROTO_DUPE_STUFF, tmpStream );
        } else {
            for ( XP_U16 dev = 1; dev < server->nv.nDevices; ++dev ) {
                if ( dev != skipDev ) {
                    sendStreamToDev( server, xwe, dev, XWPROTO_DUPE_STUFF, tmpStream );
                }
            }
        }
        stream_destroy( tmpStream );
    }
}

static XP_Bool
dupe_receivePause( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* stream )
{
    LOG_FUNC();
    XP_Bool isClient = (XP_Bool)stream_getBits( stream, 1 );
    XP_Bool accept = isClient == amHost( server );
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
            SRVR_LOGFF( "pauseType: %d; guiltyParty: %d; msg: %s",
                        pauseType, turn, msg );
        }

        if ( amHost( server ) ) {
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
    XP_Bool isHost = amHost( server );
    DUPE_STUFF typ = getDupeStuffMark( stream );
    switch ( typ ) {
    case DUPE_STUFF_MOVE_CLIENT:
        accepted = isHost && dupe_handleClientMoves( server, xwe, stream );
        break;
    case DUPE_STUFF_MOVES_SERVER:
        accepted = !isHost && dupe_handleServerMoves( server, xwe, stream );
        break;
    case DUPE_STUFF_TRADES_SERVER:
        accepted = !isHost && dupe_handleServerTrade( server, xwe, stream );
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
    MoveInfo moveInfo = {};
    model_currentMoveToMoveInfo( model, winner, &moveInfo );

    TrayTileSet newTiles = {};
    fetchTiles( server, xwe, winner, nTiles, &newTiles, XP_FALSE );

    for ( XP_U16 player = 0; player < nPlayers; ++player ) {
        model_resetCurrentTurn( model, xwe, player );
    }

    model_commitDupeTurn( model, xwe, &moveInfo, nPlayers,
                          scores, &newTiles );

    updateOthersTiles( server, xwe );

    if ( server->vol.gi->serverRole == SERVER_ISHOST ) {
        XWStreamCtxt* tmpStream =
            mem_stream_make_raw( MPPARM(server->mpool)
                                 dutil_getVTManager(server->vol.dutil) );
        /* tilesNBits, in moveInfoToStream(), needs version */
        stream_setVersion( tmpStream, server->nv.streamVersion );

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

        stream_destroy( tmpStream );
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
dupe_checkWhatsDone( const ServerCtxt* server, XP_Bool amHost,
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
        if ( amHost || isLocal ) {
            allDone = allDone && done;
        }
    }

    // XP_LOGFF( "allDone: %d; allLocalsDone: %d", allDone, allLocalsDone );
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
    XP_Bool amHost = server->vol.gi->serverRole == SERVER_ISHOST
        || server->vol.gi->serverRole == SERVER_STANDALONE;
    dupe_checkWhatsDone( server, amHost, &allDone, &allLocalsDone );

    SRVR_LOGFF( "allDone: %d", allDone );

    if ( allDone ) {            /* Yep: commit time */
        if ( amHost ) {       /* I now have everything I need to move the
                                   game foreward */
            dupe_commitAndReport( server, xwe );
        } else if ( ! server->nv.dupTurnsSent ) { /* I need to send info for
                                                     local players to host */
            XWStreamCtxt* stream =
                messageStreamWithHeader( server, xwe, HOST_DEVICE,
                                         XWPROTO_DUPE_STUFF );

            addDupeStuffMark( stream, DUPE_STUFF_MOVE_CLIENT );

            /* XP_U32 hash = model_getHash( server->vol.model ); */
            /* stream_putU32( stream, hash ); */

            XP_U16 localCount = gi_countLocalPlayers( server->vol.gi, XP_FALSE );
            SRVR_LOGFF( "writing %d moves", localCount );
            stream_putBits( stream, NPLAYERS_NBITS, localCount );
            for ( XP_U16 ii = 0; ii < server->vol.gi->nPlayers; ++ii ) {
                if ( server->vol.gi->players[ii].isLocal ) {
                    stream_putBits( stream, PLAYERNUM_NBITS, ii );
                    stream_putBits( stream, 1, server->nv.dupTurnsForced[ii] );
                    model_currentMoveToStream( server->vol.model, ii, stream );
                    SRVR_LOGFF( "wrote move %d ", ii );
                }
            }

            stream_destroy( stream ); /* sends it */
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

    XP_UCHAR buf[256] = {};
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
    case SERVER_ISHOST:
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
        SRVR_LOGFF( "msg=%s", buf );
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

    SRVR_LOGFF( "player %d now has %d tiles", turn,
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
    TrayTileSet newTiles = {};

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
    fetchTiles( server, xwe, turn, nTilesMoved, newTiles, XP_FALSE );

    XP_Bool isClient = gi->serverRole == SERVER_ISCLIENT;
    XP_Bool isLegalMove = XP_TRUE;
    if ( isClient ) {
        /* just send to host */
        sendMoveTo( server, xwe, HOST_DEVICE, turn, XP_TRUE, newTiles,
                    (TrayTileSet*)NULL );
    } else {
        isLegalMove = checkMoveAllowed( server, xwe, turn );
        sendMoveToClientsExcept( server, xwe, turn, isLegalMove, newTiles,
                                 (TrayTileSet*)NULL, HOST_DEVICE );
    }

    model_commitTurn( model, xwe, turn, newTiles );
    sortTilesIf( server, turn );

    if ( !isLegalMove && !isClient ) {
        badWordMoveUndoAndTellUser( server, xwe, &server->bws );
        /* It's ok to free these guys.  I'm the server, and the move was made
           here, so I've notified all clients already by setting the flag (and
           passing the word) in sendMoveToClientsExcept. */
        freeBWS( MPPARM(server->mpool) &server->bws );
    }

    if (isClient && (gi->phoniesAction == PHONIES_DISALLOW)
               && nTilesMoved > 0 ) {
        SETSTATE( server, XWSTATE_MOVE_CONFIRM_WAIT );
        setTurn( server, xwe, -1 );
    } else {
        nextTurn( server, xwe, PICK_NEXT );
    }
    /* SRVR_LOGFF( "player %d now has %d tiles", turn, */
    /*           model_getNumTilesInTray( model, turn ) ); */
} /* finishMove */
    
XP_Bool
server_commitTrade( ServerCtxt* server, XWEnv xwe, const TrayTileSet* oldTiles,
                    TrayTileSet* newTilesP )
{
    TrayTileSet newTiles = {};
    if ( !!newTilesP ) {
        newTiles = *newTilesP;
    }
    XP_U16 turn = server->nv.currentTurn;

    fetchTiles( server, xwe, turn, oldTiles->nTiles, &newTiles, XP_FALSE );

    if ( server->vol.gi->serverRole == SERVER_ISCLIENT ) {
        /* just send to server */
        sendMoveTo(server, xwe, HOST_DEVICE, turn, XP_TRUE, &newTiles, oldTiles);
    } else if ( server->vol.gi->serverRole == SERVER_ISHOST ) {
        sendMoveToClientsExcept( server, xwe, turn, XP_TRUE, &newTiles, oldTiles,
                                 HOST_DEVICE );
    }

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

    // XP_LOGFF( "(%d) => %d", turn, result );
    return result;
}

XP_Bool
server_getGameIsOver( const ServerCtxt* server )
{
    return server->nv.gameState == XWSTATE_GAMEOVER;
} /* server_getGameIsOver */

/* This is completely wrong */
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
    case SERVER_ISHOST:
        if ( 0 < server->nv.pendingRegistrations ) {
            XP_U16 nPlayers = server->vol.gi->nPlayers;
            const ServerPlayer* players = server->srvPlyrs;
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

XP_Bool
server_getOpenChannel( const ServerCtxt* server, XP_U16* channel )
{
    XP_Bool result = XP_FALSE;
    XP_ASSERT( amHost( server ) );
    if ( amHost( server ) && 0 < server->nv.pendingRegistrations ) {
        XP_PlayerAddr channelNo = 1;
        const XP_U16 nPlayers = server->vol.gi->nPlayers;
        const ServerPlayer* players = server->srvPlyrs;
        for ( int ii = 0; ii < nPlayers && !result; ++ii ) {
            XP_S8 deviceIndex = players->deviceIndex;
            if ( UNKNOWN_DEVICE == deviceIndex ) {
                *channel = channelNo;
                result = XP_TRUE;
            } else if ( HOST_DEVICE < deviceIndex ) {
                /* a slot's been taken */
                ++channelNo;
            }
            ++players;
        }
    }
    SRVR_LOGFF( "channel = %d, found: %s", *channel, boolToStr(result) );
    return result;
}

XP_Bool
server_canRematch( const ServerCtxt* server, XP_Bool* canOrderP )
{
    SRVR_LOGFFV( "nDevices: %d; nPlayers: %d",
                 server->nv.nDevices, server->vol.gi->nPlayers );
    const CurGameInfo* gi = server->vol.gi;
    XP_Bool result;
    XP_Bool canOrder = XP_TRUE;
    switch ( gi->serverRole ) {
    case SERVER_STANDALONE:
        result = XP_TRUE;       /* can always rematch a local game */
        break;
    case SERVER_ISHOST:
        /* have all expected clients connected? */
        result = XWSTATE_RECEIVED_ALL_REG <= server->nv.gameState
            && server->nv.nDevices == server->vol.gi->nPlayers;
        break;
    case SERVER_ISCLIENT:
        if ( 2 == gi->nPlayers ) {
            result = XP_TRUE;
        } else {
            result = 0 < server->nv.rematch.addrsLen;
            canOrder = STREAM_VERS_REMATCHORDER <= server->nv.streamVersion;
        }
        break;
    }

    if ( !!canOrderP ) {
        *canOrderP = canOrder;
    }

    /* LOG_RETURNF( "%s", boolToStr(result) ); */
    return result;
}

/* Modify the RematchInfo data to be consistent with the order we'll enforce
   as invitees join the new game.
 */
static void
sortBySame( const ServerCtxt* server, NewOrder* nop )
{
    const CurGameInfo* gi = server->vol.gi;
    for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
        nop->order[ii] = ii;
    }
}

static void
sortByScoreLow( const ServerCtxt* server, NewOrder* nop )
{
    const CurGameInfo* gi = server->vol.gi;

    ScoresArray scores = {};
    model_getCurScores( server->vol.model, &scores, server_getGameIsOver(server) );

    int mask = 0; /* mark values already consumed */
    for ( int resultIndx = 0; resultIndx < gi->nPlayers; ++resultIndx ) {
        int lowest = 10000;
        int newPosn = -1;
        for ( int jj = 0; jj < gi->nPlayers; ++jj ) {
            if ( 0 != (mask & (1 << jj)) ) {
                continue;
            } else if ( scores.arr[jj] < lowest ) {
                lowest = scores.arr[jj];
                newPosn = jj;
            }
        }
        if ( newPosn == -1 ) {
            break;
        } else {
            mask |= 1 << newPosn;
            nop->order[resultIndx] = newPosn;
            /* SRVR_LOGFF( "result[%d] = %d (for score %d)", resultIndx, newPosn, */
            /*           lowest ); */
        }
    }
}

static void
sortByScoreHigh( const ServerCtxt* server, NewOrder* nop )
{
    sortByScoreLow( server, nop );

    const CurGameInfo* gi = server->vol.gi;
    for ( int ii = 0, jj = gi->nPlayers - 1; ii < jj; ++ii, --jj ) {
        int tmp = nop->order[ii];
        nop->order[ii] = nop->order[jj];
        nop->order[jj] = tmp;
    }
}

static void
sortByRandom( const ServerCtxt* server, NewOrder* nop )
{
    const CurGameInfo* gi = server->vol.gi;
    int src[gi->nPlayers];
    for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
        src[ii] = ii;
    }
    for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
        int nLeft = gi->nPlayers - ii;
        int indx = XP_RANDOM() % nLeft;
        nop->order[ii] = src[indx];
        SRVR_LOGFFV( "set result[%d] to %d", ii, nop->order[ii] );
        /* now swap the last down */
        src[indx] = src[nLeft-1];
    }
}

#ifdef XWFEATURE_RO_BYNAME
static void
sortByName( const ServerCtxt* server, NewOrder* nop )
{
    const CurGameInfo* gi = server->vol.gi;
    int mask = 0; /* mark values already consumed */
    for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
        /* find the lowest not already used */
        int lowest = -1;
        for ( int jj = 0; jj < gi->nPlayers; ++jj ) {
            if ( 0 != (mask & (1 << jj)) ) {
                continue;
            } else if ( lowest == -1 ) {
                lowest = jj;
            } else if ( 0 < XP_STRCMP( gi->players[lowest].name,
                                       gi->players[jj].name ) ) {
                lowest = jj;
            }
        }
        XP_ASSERT( lowest != -1 );
        mask |= 1 << lowest;
        nop->order[ii] = lowest;
    }
}
#endif

static XP_Bool
setPlayerOrder( const ServerCtxt* XP_UNUSED_DBG(server), const NewOrder* nop,
                CurGameInfo* gi, RematchInfo* rip )
{
    CurGameInfo srcGi = *gi;
    RematchInfo srcRi;
    if ( !!rip ) {
        srcRi = *rip;
    }

    for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
        gi->players[ii] = srcGi.players[nop->order[ii]];
        if ( !!rip ) {
            rip->addrIndices[ii] = srcRi.addrIndices[nop->order[ii]];
        }
    }

    LOGGI( gi, "end" );
    if ( !!rip ) {
        LOG_RI( rip );
    }

    return XP_TRUE;
} /* setPlayerOrder */

XP_Bool
server_getRematchInfo( const ServerCtxt* server, XWEnv xwe, XW_UtilCtxt* newUtil,
                       XP_U32 gameID, const NewOrder* nop, RematchInfo** ripp )
{
    XP_Bool success = server_canRematch( server, NULL );
    if ( success ) {
        CurGameInfo* newGI = newUtil->gameInfo;
        gi_disposePlayerInfo( MPPARM(newUtil->mpool) newGI );

        gi_copy( MPPARM(newUtil->mpool) newGI, server->vol.gi );
        newGI->gameID = gameID;
        if ( SERVER_ISCLIENT == newGI->serverRole ) {
            newGI->serverRole = SERVER_ISHOST; /* we'll be inviting */
            newGI->forceChannel = 0;
        }
        LOGGI( newUtil->gameInfo, "ready to invite" );

        success = getRematchInfoImpl( server, xwe, newGI, nop, ripp );
    }
    return success;
}

static XP_Bool
getRematchInfoImpl( const ServerCtxt* server, XWEnv xwe, CurGameInfo* newGI,
                    const NewOrder* nop, RematchInfo** ripp )
{
    XP_Bool success = XP_TRUE;
    RematchInfo ri = {};
    const CommsCtxt* comms = server->vol.comms;
    /* Now build the address list. Simple cases are STANDALONE, when I'm
       the host, or when there are only two devices/players. If I'm guest
       and there is another guest, I count on the host having sent rematch
       info, and *that* info has an old and a new format. Sheesh. */
    XP_Bool canOrder = XP_TRUE;
    if ( !comms ) {
        /* no addressing to do!! */
    } else if ( amHost( server ) || 2 == newGI->nPlayers ) {
        for ( int ii = 0; ii < newGI->nPlayers; ++ii ) {
            if ( newGI->players[ii].isLocal ) {
                ri_addLocal( &ri );
            } else {
                CommsAddrRec addr;
                if ( amHost(server) ) {
                    XP_S8 deviceIndex = server->srvPlyrs[ii].deviceIndex;
                    XP_ASSERT( deviceIndex != RIP_LOCAL_INDX );
                    XP_PlayerAddr channelNo =
                        server->nv.addresses[deviceIndex].channelNo;
                    comms_getChannelAddr( comms, channelNo, &addr );
                } else {
                    comms_getHostAddr( comms, &addr );
                }
                ri_addAddrAt( server->vol.dutil, xwe, &ri, &addr, ii );
            }
        }
    } else if ( !!server->nv.rematch.addrs ) {
        XP_U16 streamVersion = server->nv.streamVersion;
        if ( STREAM_VERS_REMATCHORDER <= streamVersion ) {
            loadRemoteRI( server, newGI, &ri );

        } else {
            /* I don't have complete info yet. So let's go through the gi,
               assigning an address to all non-local players. We'll use
               the host address first, then the rest we have. If we don't
               have the right number of everything, we fail. Note: we
               should not have given the user a choice in rematch ordering
               here!!!*/
            canOrder = newGI->nPlayers <= 2;
            XP_ASSERT( !canOrder );
            canOrder = XP_FALSE;

            CommsAddrRec addrs[MAX_NUM_PLAYERS];
            int nAddrs = 0;
            comms_getHostAddr( comms, &addrs[nAddrs++] );

            XWStreamCtxt* stream = mkServerStream( server,
                                                   server->nv.streamVersion );
            stream_putBytes( stream, server->nv.rematch.addrs,
                             server->nv.rematch.addrsLen );
            while ( 0 < stream_getSize( stream ) ) {
                XP_ASSERT( nAddrs < VSIZE(addrs) );
                addrFromStream( &addrs[nAddrs++], stream );
            }
            stream_destroy( stream );

            int nextRemote = 0;
            for ( int ii = 0; success && ii < newGI->nPlayers; ++ii ) {
                if ( newGI->players[ii].isLocal ) {
                    ri_addLocal( &ri );
                } else if ( nextRemote < nAddrs ) {
                    ri_addAddrAt( server->vol.dutil, xwe, &ri, &addrs[nextRemote++], ii );
                } else {
                    SRVR_LOGFF( "ERROR: not enough addresses for all"
                                " remote players" );
                    success = XP_FALSE;
                }
            }
            if ( success ) {
                success = nextRemote == nAddrs;
            }
        }
    } else {
        success = XP_FALSE;
    }

    if ( success && canOrder ) {
        if ( !!comms ) {
            assertRI( &ri, newGI );
        }
        success = setPlayerOrder( server, nop, newGI, !!comms ? &ri : NULL );
    }

    if ( success && !!comms ) {
        LOG_RI( &ri );
        assertRI( &ri, newGI );
        XP_ASSERT( success );
        *ripp = XP_MALLOC(server->mpool, sizeof(**ripp));
        **ripp = ri;
    } else {
        *ripp = NULL;
    }
    XP_ASSERT( success );

    LOG_RETURNF( "%s", boolToStr(success)  );
    return success;
} /* getRematchInfoImpl */

void
server_disposeRematchInfo( ServerCtxt* XP_UNUSED_DBG(server), RematchInfo** ripp )
{
    SRVR_LOGFFV( "(%p)", *ripp );
    if ( !!*ripp ) {
        LOG_RI( *ripp );
    }
    XP_FREEP( server->mpool, ripp );
}

XP_Bool
server_ri_getAddr( const RematchInfo* rip, XP_U16 nth,
                   CommsAddrRec* addr, XP_U16* nPlayersH )
{
    const CommsAddrRec* rec = &rip->addrs[nth];
    XP_Bool success = !addr_isEmpty( rec );

    if ( success ) {
        XP_U16 count = 0;
        for ( int ii = 0; ii < rip->nPlayers; ++ii ) {
            if ( rip->addrIndices[ii] == nth ) {
                ++count;
            }
        }
        success = 0 < count;
        if ( success ) {
            *nPlayersH = count;
            *addr = *rec;
        }
    }

    return success;
}

void
server_figureOrder( const ServerCtxt* server, RematchOrder ro, NewOrder* nop )
{
    XP_MEMSET( nop, 0, sizeof(*nop) );

    void (*proc)(const ServerCtxt*, NewOrder*) = NULL;
    switch ( ro ) {
    case RO_NONE:
    case RO_SAME:
        proc = sortBySame;
        break;
    case RO_LOW_SCORE_FIRST:
        proc = sortByScoreLow;
        break;
    case RO_HIGH_SCORE_FIRST:
        proc = sortByScoreHigh;
        break;
    case RO_JUGGLE:
        proc = sortByRandom;
        break;
#ifdef XWFEATURE_RO_BYNAME
    case RO_BY_NAME:
        proc = sortByName;
        break;
#endif
    case RO_NUM_ROS:
    default:
        XP_ASSERT(0); break;
    }

    (*proc)( server, nop );
}

/* Record the desired order, which is already set in the RematchInfo passed
   in, so we can enforce it as clients register. */
void
server_setRematchOrder( ServerCtxt* server, const RematchInfo* rip )
{
    if ( amHost( server ) ) {   /* standalones can call without harm.... */
        XP_ASSERT( !!rip );
        XP_ASSERT( !server->nv.rematch.order );
        server->nv.rematch.order = XP_MALLOC( server->mpool, sizeof(*rip) );
        *server->nv.rematch.order = *rip;
        server->nv.flags |= MASK_HAVE_RIP_INFO + MASK_IS_FROM_REMATCH;
    }
}

XP_Bool
server_isFromRematch( const ServerCtxt* server )
{
    return 0 != (server->nv.flags & MASK_IS_FROM_REMATCH);
}

#ifdef XWFEATURE_KNOWNPLAYERS
void
server_gatherPlayers( ServerCtxt* server, XWEnv xwe, XP_U32 created )
{
    XP_Bool flagSet = 0 != (server->nv.flags & FLAG_HARVEST_READY);
    if ( flagSet ) {
        const CurGameInfo* gi = server->vol.gi;
        XW_DUtilCtxt* dutil = server->vol.dutil;

        NewOrder no;
        server_figureOrder( server, RO_SAME, &no );

        CurGameInfo tmpGi = *gi;
        RematchInfo* ripp;
        if ( getRematchInfoImpl( server, xwe, &tmpGi, &no, &ripp ) ) {
            for ( int ii = 0, nRemotes = 0; ii < gi->nPlayers; ++ii ) {
                const LocalPlayer* lp = &gi->players[ii];
                /* order unchanged? */
                XP_ASSERT( lp->name == gi->players[ii].name );
                if ( !lp->isLocal ) {
                    CommsAddrRec addr;
                    XP_U16 nPlayersH;
                    if ( !server_ri_getAddr( ripp, nRemotes++, &addr, &nPlayersH ) ) {
                        break;
                    }
                    XP_ASSERT( 1 == nPlayersH ); /* else fixme... */
                    kplr_addAddr( dutil, xwe, &addr, lp->name, created );
                }
            }

            server_disposeRematchInfo( server, &ripp );
        }
    }
}
#endif

#ifdef DEBUG
static void
log_ri( const ServerCtxt* server, const RematchInfo* rip,
        const char* caller, int line )
{
    XP_USE(line);
    SRVR_LOGFFV( "called from line %d of %s() with ptr %p", line, caller, rip );
    if ( !!rip ) {
        char buf[64] = {};
        int offset = 0;
        int maxIndx = -1;
        for ( int ii = 0; ii < rip->nPlayers; ++ii ) {
            XP_S8 indx = rip->addrIndices[ii];
            offset += XP_SNPRINTF( buf+offset, VSIZE(buf)-offset, "%d, ", indx );
            if ( indx > maxIndx ) {
                maxIndx = indx;
            }
        }
        SRVR_LOGFFV( "%d players (and %d addrs): [%s]", rip->nPlayers,
                     rip->nAddrs, buf );

        for ( int ii = 0; ii < rip->nAddrs; ++ii ) {
            XP_SNPRINTF( buf, VSIZE(buf), "[%d of %d]: %s from %s",
                         ii, rip->nAddrs, __func__, caller );
            logAddr( server->vol.dutil, &rip->addrs[ii], __func__ );
        }
    }
}
#endif

static void
ri_toStream( XWStreamCtxt* stream, const RematchInfo* rip,
             const ServerCtxt* XP_UNUSED_DBG(server) )
{
    LOG_RI(rip);
    XP_U16 nPlayers = !!rip ? rip->nPlayers : 0;
    for ( int ii = 0; ii < nPlayers; ++ii ) {
        XP_S8 indx = rip->addrIndices[ii];
        if ( RIP_LOCAL_INDX != indx ) {
            stream_putBits( stream, PLAYERNUM_NBITS, indx );
        }
    }

    for ( int ii = 0; ii < rip->nAddrs; ++ii ) {
        addrToStream( stream, &rip->addrs[ii] );
    }
}

static void
ri_fromStream( RematchInfo* rip, XWStreamCtxt* stream,
               const ServerCtxt* server )
{
    const CurGameInfo* gi = server->vol.gi;
    XP_MEMSET( rip, 0, sizeof(*rip) );
    rip->nPlayers = gi->nPlayers;

    for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
        if ( gi->players[ii].isLocal ) {
            rip->addrIndices[ii] = RIP_LOCAL_INDX;
        } else {
            XP_U16 indx = stream_getBits( stream, PLAYERNUM_NBITS );
            rip->addrIndices[ii] = indx;
            if ( indx > rip->nAddrs ) {
                rip->nAddrs = indx;
            }
        }
    }

    ++rip->nAddrs;       /* it's count now, not index */
    for ( int ii = 0; ii < rip->nAddrs; ++ii ) {
        addrFromStream( &rip->addrs[ii], stream );
    }

    LOG_RI(rip);
    XP_ASSERT( 0 < rip->nPlayers );
}

/* Given an address, insert it if it's new, or point to an existing copy
   otherwise */
static void
ri_addAddrAt( XW_DUtilCtxt* dutil, XWEnv xwe, RematchInfo* rip,
              const CommsAddrRec* addr, const XP_U16 player )
{
    XP_S8 newIndex = RIP_LOCAL_INDX;
    for ( int ii = 0; ii < player; ++ii ) {
        int index = rip->addrIndices[ii];
        if ( index != RIP_LOCAL_INDX &&
             addrsAreSame( dutil, xwe, addr, &rip->addrs[index] ) ) {
            newIndex = index;
            break;
        }
    }

    // didn't find it?
    if ( RIP_LOCAL_INDX == newIndex ) {
        newIndex = rip->nAddrs;
        rip->addrs[newIndex] = *addr;
        ++rip->nAddrs;
    }

    rip->addrIndices[player] = newIndex;
    XP_ASSERT( rip->nPlayers == player );
    ++rip->nPlayers;
}

static void
ri_addHostAddrs( RematchInfo* rip, const ServerCtxt* server )
{
    for ( int ii = 0; ii < rip->nAddrs; ++ii ) {
        if ( addr_isEmpty( &rip->addrs[ii] ) ) {
            comms_getHostAddr( server->vol.comms, &rip->addrs[ii] );
        }
    }
}

static void
ri_addLocal( RematchInfo* rip )
{
    rip->addrIndices[rip->nPlayers++] = RIP_LOCAL_INDX;
}

#ifdef DEBUG
static void
assertRI( const RematchInfo* rip, const CurGameInfo* gi )
{
    /* Local players should not be represented */
    XP_ASSERT( gi && rip );
    XP_ASSERT( gi->nPlayers == rip->nPlayers );
    XP_U16 mask = 0;
    for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
        XP_Bool isLocal = gi->players[ii].isLocal;
        XP_ASSERT( isLocal == (rip->addrIndices[ii] == RIP_LOCAL_INDX) );
        if ( !isLocal ) {
            mask |= 1 << rip->addrIndices[ii];
        }
    }
    XP_ASSERT( countBits(mask) == rip->nAddrs );

    for ( int ii = 0; ii < rip->nAddrs; ++ii ) {
        XP_ASSERT( !addr_isEmpty( &rip->addrs[ii] ) );
    }
}
#endif

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

        XP_U16 devIndex;
        for ( devIndex = 1; devIndex < server->nv.nDevices; ++devIndex ) {
            XWStreamCtxt* stream;
            stream = messageStreamWithHeader( server, xwe, devIndex,
                                              XWPROTO_END_GAME );
            putQuitter( server, stream, quitter );
            stream_destroy( stream );
        }
        doEndGame( server, xwe, quitter );

    } else {
        XWStreamCtxt* stream;
        stream = messageStreamWithHeader( server, xwe, HOST_DEVICE,
                                          XWPROTO_CLIENT_REQ_END_GAME );
        putQuitter( server, stream, quitter );
        stream_destroy( stream );

        /* Do I want to change the state I'm in?  I don't think so.... */
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

void
server_inviteeName( const ServerCtxt* server,
                    XWEnv xwe, XP_U16 playerPosn,
                    XP_UCHAR* buf, XP_U16* bufLen )
{
    int nameIndx = 0;
    for ( int ii = 0; ii <= playerPosn; ++ii ) {
        const ServerPlayer* sp = &server->srvPlyrs[ii];
        if ( -1 == sp->deviceIndex ) { /* not yet claimed */
            if ( playerPosn == ii ) {

                CommsCtxt* comms = server->vol.comms;
                InviteeNames names = {};
                comms_inviteeNames( comms, xwe, &names );

                if ( nameIndx < names.nNames ) {
                    XP_LOGFF( "got a match: player %d for channel %d; name: \"%s\"",
                              playerPosn, nameIndx, names.name[nameIndx] );
                    *bufLen = XP_SNPRINTF( buf, *bufLen, names.name[nameIndx], playerPosn );
                } else {
                    XP_LOGFF( "expected %dth name but found only %d",
                              nameIndx, names.nNames );
                }
                break;
            }
            ++nameIndx;
        }
    }

    XP_LOGFF( "(%d) => %s", playerPosn, buf );
}

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
               || (!amHost(server) || (0 == server->nv.pendingRegistrations)));
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

static void
tellMoveWasLegal( ServerCtxt* server, XWEnv xwe )
{
    XWStreamCtxt* stream =
        messageStreamWithHeader( server, xwe, server->lastMoveSource,
                                 XWPROTO_MOVE_CONFIRM );

    stream_destroy( stream );

    SETSTATE( server, XWSTATE_INTURN );
} /* tellMoveWasLegal */

static XP_Bool
handleIllegalWord( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* incoming )
{
    BadWordsState bws = {{}};

    (void)stream_getBits( incoming, PLAYERNUM_NBITS );
    bwsFromStream( MPPARM(server->mpool) incoming, &bws );

    badWordMoveUndoAndTellUser( server, xwe, &bws );

    freeBWS( MPPARM(server->mpool) &bws );

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

    stream_destroy( stream );
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
        SRVR_LOGFF( "popped down to %X", model_getHash( model ) );
        sortTilesIf( server, turn );

        if ( code == XWPROTO_UNDO_INFO_CLIENT ) { /* need to inform */
            XP_U16 sourceClientIndex = getIndexForStream( server, stream );

            sendUndoToClientsExcept( server, xwe, sourceClientIndex, nUndone,
                                     lastUndone, newHash );
        }

        util_informUndo( server->vol.util, xwe );
        nextTurn( server, xwe, turn );
    } else {
        SRVR_LOGFF( "unable to pop to hash %X; dropping", newHash );
        // XP_ASSERT(0);
        success = XP_TRUE;      /* Otherwise we'll stall */
    }

    LOG_RETURNF( "%s", boolToStr(success) );
    return success;
} /* reflectUndos */

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
        XP_ASSERT( lastUndone != 0xFFFF );
        SRVR_LOGFF( "popped to hash %X", newHash );
        if ( server->vol.gi->serverRole == SERVER_ISCLIENT ) {
            sendUndoTo( server, xwe, HOST_DEVICE, nUndone, lastUndone, newHash );
        } else {
            sendUndoToClientsExcept( server, xwe, HOST_DEVICE, nUndone,
                                     lastUndone, newHash );
        }
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
    XP_Bool isHost = amHost( server );
    const XW_Proto code = readProto( server, incoming );
    SRVR_LOGFFV( "code=%s", codeToStr(code) );

    switch ( code ) {
    case XWPROTO_DEVICE_REGISTRATION:
        accepted = isHost;
        if ( accepted ) {
        /* This message is special: doesn't have the header that's possible
           once the game's in progress and communication's been
           established. */
            SRVR_LOGFF( "somebody's registering!!!" );
            accepted = handleRegistrationMsg( server, xwe, incoming );
        } else {
            SRVR_LOGFF( "WTF: I'm not a server!!" );
        }
        break;
    case XWPROTO_CLIENT_SETUP:
        accepted = !isHost
            && XWSTATE_NONE == server->nv.gameState
            && client_readInitialMessage( server, xwe, incoming );
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
            SRVR_LOGFF( "bad state: %s; dropping", getStateStr( server->nv.gameState ) );
            accepted = XP_TRUE;
        }
        break;

    case XWPROTO_MOVEMADE_INFO_SERVER: /* server telling me about a move */
        if ( isHost ) {
            SRVR_LOGFFV( "%s received by server!", codeToStr(code) );
            accepted = XP_FALSE;
        } else {
            accepted = reflectMove( server, xwe, incoming );
        }
        if ( accepted ) {
            nextTurn( server, xwe, PICK_NEXT );
        } else {
            accepted = XP_TRUE; /* don't stall.... */
            SRVR_LOGFF( "dropping move: state=%s", getStateStr(server->nv.gameState ) );
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

    XP_ASSERT( isHost == amHost( server ) ); /* caching value is ok? */
    stream_close( incoming );

    SRVR_LOGFF( "=> %s (code=%s)", boolToStr(accepted), codeToStr(code) );
    // XP_ASSERT( accepted );      /* do not commit!!! */
    return accepted;
} /* server_receiveMessage */

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
            XP_UCHAR faces[48] = {};
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

        XP_UCHAR tmpbuf[48] = {};
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
            XP_UCHAR buf2[128];
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
