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
#include "contrlrp.h"
#include "util.h"
#include "model.h"
#include "comms.h"
#include "gamep.h"
#include "states.h"
#include "util.h"
#include "pool.h"
#include "enginep.h"
#include "device.h"
#include "gamerefp.h"
#include "strutils.h"
#include "dbgutil.h"
#include "knownplyr.h"
#include "stats.h"
#include "timers.h"

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

typedef enum { DUPE_STUFF_TRADES_CTRLR,
               DUPE_STUFF_MOVES_CTRLR,
               DUPE_STUFF_MOVE_CLIENT,
               DUPE_STUFF_PAUSE,
} DUPE_STUFF;

typedef enum {
    XWPROTO_ERROR = 0 /* illegal value */
    ,XWPROTO_CHAT      /* broadcast text message for display */
    ,XWPROTO_DEVICE_REGISTRATION /* client's first message to ctrlr */
    ,XWPROTO_CLIENT_SETUP /* ctrlr's first message to client */
    ,XWPROTO_MOVEMADE_INFO_GUEST /* client reports a move it made */
    ,XWPROTO_MOVEMADE_INFO_HOST /* host tells all clients about a move
                                     made by it or another client */
    ,XWPROTO_UNDO_INFO_CLIENT    /* client reports undo[s] on the device */
    ,XWPROTO_UNDO_INFO_CTRLR    /* ctrlr reports undos[s] happening
                                  elsewhere*/
    //XWPROTO_CLIENT_MOVE_INFO,  /* client says "I made this move" */
    //XWPROTO_CTRLR_MOVE_INFO,  /* ctrlr says "Player X made this move" */
/*     XWPROTO_CLIENT_TRADE_INFO, */
/*     XWPROTO_TRADEMADE_INFO, */
    ,XWPROTO_BADWORD_INFO
    ,XWPROTO_MOVE_CONFIRM  /* ctrlr tells move sender that move was
                              legal */
    //XWPROTO_MOVEMADE_INFO,       /* info about tiles placed and received */
    ,XWPROTO_CLIENT_REQ_END_GAME   /* non-ctrlr wants to end the game */
    ,XWPROTO_END_GAME               /* ctrlr says to end game */

    ,XWPROTO_NEW_PROTO

    ,XWPROTO_DUPE_STUFF         /* used for all duplicate-mode messages */
} XW_Proto;

#define XWPROTO_NBITS 4

#define UNKNOWN_DEVICE -1
#define HOST_DEVICE 0

typedef struct _CtrlrPlayer {
    EngineCtxt* engine; /* each needs his own so don't interfere each other */
    XP_S8 deviceIndex;  /* 0 means local, -1 means unknown */
} CtrlrPlayer;

typedef struct _RemoteAddress {
    XP_PlayerAddr channelNo;
#ifdef STREAM_VERS_BIGBOARD
    XP_U8 streamVersion;
#endif
} RemoteAddress;

/* These are the parts of the ctrlr's state that needs to be preserved
   across a reset/new game */
typedef struct CtrlrVolatiles {
    ModelCtxt* model;
    CommsCtxt* comms;
    XW_UtilCtxt** utilp;
    XW_DUtilCtxt* dutil;
    const CurGameInfo* gi;
    GameRef gr;
    TurnChangeListener turnChangeListener;
    void* turnChangeData;
    TimerChangeListener timerChangeListener;
    void* timerChangeData;
    GameOverListener gameOverListener;
    void* gameOverData;
    XP_U16 bitsPerTile;
    XP_Bool showPrevMove;
    XP_Bool pickTilesCalled[MAX_NUM_PLAYERS];
} CtrlrVolatiles;

#define MASK_IS_FROM_REMATCH (1<<0)
#define MASK_HAVE_RIP_INFO (1<<1)
#define FLAG_HARVEST_READY (1<<2)

typedef struct _CtrlrNonvolatiles {
    XP_U32 flags;           /*  */
    XP_U32 lastMoveTime;    /* seconds of last turn change */
    XP_S32 dupTimerExpires;
    XP_U8 nDevices;
    XW_State gameState;
    XP_S8 currentTurn; /* invalid when game is over */
    XP_S8 quitter;     /* -1 unless somebody resigned */
    XP_U8 pendingRegistrations; /* ctrlr-case only */
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

} CtrlrNonvolatiles;

typedef struct _BadWordsState {
    BadWordInfo bwi;
    XP_UCHAR* dictName;
} BadWordsState;

struct CtrlrCtxt {
    CtrlrVolatiles vol;
    CtrlrNonvolatiles nv;

    PoolContext* pool;

    BadWordsState bws;

    XP_U16 lastMoveSource;

    CtrlrPlayer srvPlyrs[MAX_NUM_PLAYERS];
    XP_Bool ctrlrDoing;
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

# define dupe_timerRunning()    ctrl_canPause(ctrlr)

#define ENABLE_LOGFFV
# ifdef ENABLE_LOGFFV
#  define SRVR_LOGFFV SRVR_LOGFF
# else
#  define SRVR_LOGFFV(...)
# endif

#ifdef DEBUG
# define SRVR_LOGFF( ... ) {                           \
        XP_U32 gameID = ctrlr->vol.gi->gameID;        \
        XP_GID_LOGFF( gameID, __VA_ARGS__ );           \
    }
#else
# define SRVR_LOGFF( ... )
#endif


#define NPASSES_OK(s) model_recentPassCountOk((s)->vol.model)

/******************************* prototypes *******************************/
static XP_Bool assignTilesToAll( CtrlrCtxt* ctrlr, XWEnv xwe );
static void makePoolOnce( CtrlrCtxt* ctrlr );

static XP_S8 getIndexForDevice( const CtrlrCtxt* ctrlr,
                                XP_PlayerAddr channelNo );
static XP_S8 getIndexForStream( const CtrlrCtxt* ctrlr,
                                const XWStreamCtxt* stream );

static void nextTurn( CtrlrCtxt* ctrlr, XWEnv xwe, XP_S16 nxtTurn );

static void doEndGame( CtrlrCtxt* ctrlr, XWEnv xwe, XP_S16 quitter );
static void endGameInternal( CtrlrCtxt* ctrlr, XWEnv xwe,
                             GameEndReason why, XP_S16 quitter );
static void badWordMoveUndoAndTellUser( CtrlrCtxt* ctrlr, XWEnv xwe,
                                        const BadWordsState* bws );
static XP_Bool tileCountsOk( const CtrlrCtxt* ctrlr );
static void setTurn( CtrlrCtxt* ctrlr, XWEnv xwe, XP_S16 turn );
static XWStreamCtxt* mkCtrlrStream( const CtrlrCtxt* ctrlr, XP_U8 version );
static XWStreamCtxt* mkCtrlrStream0( const CtrlrCtxt* ctrlr );
static void fetchTiles( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 playerNum,
                        XP_U16 nToFetch, TrayTileSet* resultTiles,
                        XP_Bool forceCanPlay );
static void finishMove( CtrlrCtxt* ctrlr, XWEnv xwe,
                        TrayTileSet* newTiles, XP_U16 turn );
static XP_Bool dupe_checkTurns( CtrlrCtxt* ctrlr, XWEnv xwe );
static void dupe_forceCommits( CtrlrCtxt* ctrlr, XWEnv xwe );

static void dupe_clearState( CtrlrCtxt* ctrlr );
static XP_U16 dupe_nextTurn( const CtrlrCtxt* ctrlr );
static void dupe_commitAndReportMove( CtrlrCtxt* ctrlr, XWEnv xwe,
                                      XP_U16 winner, XP_U16 nPlayers,
                                      XP_U16* scores, XP_U16 nTiles );
static XP_Bool commitMoveImpl( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 player,
                               TrayTileSet* newTilesP, XP_Bool forced );
static void dupe_makeAndReportTrade( CtrlrCtxt* ctrlr, XWEnv xwe );
static void dupe_transmitPause( CtrlrCtxt* ctrlr, XWEnv xwe, DupPauseType typ,
                                XP_U16 turn, const XP_UCHAR* msg,
                                XP_S16 skipDev );
static void dupe_resetTimer( CtrlrCtxt* ctrlr, XWEnv xwe );
static void resetDupeTimerIf( CtrlrCtxt* ctrlr, XWEnv xwe );
static XP_Bool setDupCheckTimer( CtrlrCtxt* ctrlr, XWEnv xwe );
static void sortTilesIf( CtrlrCtxt* ctrlr, XP_S16 turn );
static XP_Bool doImpl( CtrlrCtxt* ctrlr, XWEnv xwe );
static XWStreamCtxt* messageStreamWithHeader( const CtrlrCtxt* ctrlr, XP_U16 devIndex,
                                              XW_Proto code );
static void closeAndSend( const CtrlrCtxt* ctrlr, XWEnv xwe, XWStreamCtxt* stream );
static XP_Bool handleRegistrationMsg( CtrlrCtxt* ctrlr, XWEnv xwe,
                                      XWStreamCtxt* stream );
static XP_S8 registerRemotePlayer( CtrlrCtxt* ctrlr, XWEnv xwe,
                                   XWStreamCtxt* stream );
static void sendInitialMessage( CtrlrCtxt* ctrlr, XWEnv xwe );
static void sendBadWordMsgs( CtrlrCtxt* ctrlr, XWEnv xwe );
static XP_Bool handleIllegalWord( CtrlrCtxt* ctrlr, XWEnv xwe,
                                  XWStreamCtxt* incoming );
static void tellMoveWasLegal( CtrlrCtxt* ctrlr, XWEnv xwe );
static void writeProto( const CtrlrCtxt* ctrlr, XWStreamCtxt* stream, 
                        XW_Proto proto );
static void readGuestAddrs( CtrlrCtxt* ctrlr, XWStreamCtxt* stream,
                            XP_U8 streamVersion );
static XP_Bool getRematchInfoImpl( const CtrlrCtxt* ctrlr, XWEnv xwe,
                                   CurGameInfo* newGI, const NewOrder* nop,
                                   RematchInfo** ripp );

static void ri_fromStream( RematchInfo* rip, XWStreamCtxt* stream,
                           const CtrlrCtxt* ctrlr );
static void ri_toStream( XWStreamCtxt* stream, const RematchInfo* rip,
                         const CtrlrCtxt* ctrlr );
static void ri_addAddrAt( XW_DUtilCtxt* dutil, XWEnv xwe, RematchInfo* rip,
                          const CommsAddrRec* addr, XP_U16 playerIndex );
static void ri_addHostAddrs( RematchInfo* rip, const CtrlrCtxt* ctrlr );
static void ri_addLocal( RematchInfo* rip );

#ifdef DEBUG
static void assertRI( const RematchInfo* rip, const CurGameInfo* gi );
static void log_ri( const CtrlrCtxt* ctrlr, const RematchInfo* rip,
                    const char* caller, int line );
# define LOG_RI(RIP) log_ri(ctrlr, (RIP), __func__, __LINE__ )
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
        CASESTR(XWSTATE_RECEIVED_ALL_REG);
        CASESTR(XWSTATE_NEEDSEND_BADWORD_INFO);
        CASESTR(XWSTATE_MOVE_CONFIRM_WAIT);
        CASESTR(XWSTATE_MOVE_CONFIRM_MUSTSEND);
        CASESTR(XWSTATE_NEEDSEND_ENDGAME);
        CASESTR(XWSTATE_INTURN);
        CASESTR(XWSTATE_GAMEOVER);
    default:
        // XP_ASSERT(0);
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
# define SETSTATE( ctrlr, st ) {                                   \
        XW_State old = (ctrlr)->nv.gameState;                      \
        (ctrlr)->nv.gameState = (st);                              \
        logNewState( old, st, __func__);                            \
    }
#else
# define SETSTATE( s, st ) (s)->nv.gameState = (st)
#endif

static XP_Bool
inDuplicateMode( const CtrlrCtxt* ctrlr )
{
    XP_Bool result = ctrlr->vol.gi->inDuplicateMode;
    // LOG_RETURNF( "%s", boolToStr(result) );
    return result;
}

/*****************************************************************************
 *
 ****************************************************************************/
static void
syncPlayers( CtrlrCtxt* ctrlr )
{
    const CurGameInfo* gi = ctrlr->vol.gi;
    for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
        const LocalPlayer* lp = &gi->players[ii];
        if ( !lp->isLocal/*  && !lp->name */ ) {
            ++ctrlr->nv.pendingRegistrations;
        }
        CtrlrPlayer* player = &ctrlr->srvPlyrs[ii];
        player->deviceIndex = lp->isLocal? HOST_DEVICE : UNKNOWN_DEVICE;
    }
}

static XP_Bool
amHost( const CtrlrCtxt* ctrlr )
{
    XP_Bool result = ROLE_ISHOST == ctrlr->vol.gi->deviceRole;
    // LOG_RETURNF( "%d (seed=%d)", result, comms_getChannelSeed( ctrlr->vol.comms ) );
    return result;
}

#ifdef DEBUG
XP_Bool ctrl_getIsHost( const CtrlrCtxt* ctrlr ) { return amHost(ctrlr); }
#endif

static void
initCtrlr( CtrlrCtxt* ctrlr, XWEnv xwe )
{
    SRVR_LOGFF(" ");
    setTurn( ctrlr, xwe, -1 ); /* game isn't under way yet */

    if ( ctrlr->vol.gi->deviceRole == ROLE_ISGUEST ) {
        SETSTATE( ctrlr, XWSTATE_NONE );
    } else {
        SETSTATE( ctrlr, XWSTATE_BEGIN );
    }

    syncPlayers( ctrlr );

    ctrlr->nv.nDevices = 1; /* local device (0) is always there */
#ifdef STREAM_VERS_BIGBOARD
    ctrlr->nv.streamVersion = STREAM_SAVE_PREVWORDS; /* default to old */
#endif
    ctrlr->nv.quitter = -1;
} /* initCtrlr */

CtrlrCtxt* 
ctrl_make( XWEnv xwe, ModelCtxt* model, CommsCtxt* comms, XW_UtilCtxt** utilp )
{
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = util_getMemPool( *utilp, xwe );
#endif
    CtrlrCtxt* result = (CtrlrCtxt*)XP_CALLOC( mpool, sizeof(*result) );
    if ( result != NULL ) {
        MPASSIGN(result->mpool, mpool);

        result->vol.model = model;
        result->vol.comms = comms;
        result->vol.utilp = utilp;
        result->vol.dutil = util_getDevUtilCtxt( *utilp );
        GameRef gr = (*utilp)->gr;
        result->vol.gr = gr;
        result->vol.gi = gr_getGI( result->vol.dutil, gr, xwe );
        XP_ASSERT( gr == gi_formatGR(result->vol.gi) );

        initCtrlr( result, xwe );
    }
    return result;
} /* ctrl_make */

static void
getNV( XWStreamCtxt* stream, CtrlrNonvolatiles* nv, XP_U16 nPlayers )
{
    XP_U16 ii;
    XP_U16 version = strm_getVersion( stream );

    XP_ASSERT( 0 == nv->flags );
    if ( STREAM_VERS_REMATCHORDER <= version ) {
        nv->flags = strm_getU32VL( stream );
    }

    if ( STREAM_VERS_DICTNAME <= version ) {
        nv->lastMoveTime = strm_getU32( stream );
    }
    if ( STREAM_VERS_DUPLICATE <= version ) {
        nv->dupTimerExpires = strm_getU32( stream );
    }

    if ( version < STREAM_VERS_HOST_SAVES_TOSHOW ) {
        /* no longer used */
        (void)strm_getBits( stream, 3 ); /* was npassesinrow */
    }

    nv->nDevices = (XP_U8)strm_getBits( stream, NDEVICES_NBITS );
    if ( version > STREAM_VERS_41B4 ) {
        ++nv->nDevices;
    }

    XP_ASSERT( XWSTATE_LAST <= 1<<4 );
    nv->gameState = (XW_State)strm_getBits( stream, XWSTATE_NBITS );
    XP_LOGFF( "read state: %s", getStateStr(nv->gameState) );
    if ( version >= STREAM_VERS_HOST_SAVES_TOSHOW && version < STREAM_VERS_BIGGERGI ) {
        strm_getBits( stream, XWSTATE_NBITS );
    }

    nv->currentTurn = (XP_S8)strm_getBits( stream, NPLAYERS_NBITS ) - 1;
    if ( STREAM_VERS_DICTNAME <= version ) {
        nv->quitter = (XP_S8)strm_getBits( stream, NPLAYERS_NBITS ) - 1;
    }
    nv->pendingRegistrations = (XP_U8)strm_getBits( stream, NPLAYERS_NBITS );

    for ( ii = 0; ii < nPlayers; ++ii ) {
        nv->addresses[ii].channelNo =
            (XP_PlayerAddr)strm_getBits( stream, 16 );
#ifdef STREAM_VERS_BIGBOARD
        nv->addresses[ii].streamVersion = STREAM_VERS_BIGBOARD <= version ?
            strm_getBits( stream, 8 ) : STREAM_SAVE_PREVWORDS;
#endif
    }
#ifdef STREAM_VERS_BIGBOARD
    if ( STREAM_SAVE_PREVWORDS < version ) {
        nv->streamVersion = strm_getU8 ( stream );
    }
    /* XP_LOGFF( "read streamVersion: 0x%x", nv->streamVersion ); */
#endif

    if ( version >= STREAM_VERS_DUPLICATE ) {
        for ( ii = 0; ii < nPlayers; ++ii ) {
            nv->dupTurnsMade[ii] = strm_getBits( stream, 1 );
            // XP_LOGFF( "dupTurnsMade[%d]: %d", ii, nv->dupTurnsMade[ii] );
            nv->dupTurnsForced[ii] = strm_getBits( stream, 1 );
        }
        nv->dupTurnsSent = strm_getBits( stream, 1 );
    }
} /* getNV */

static void
putNV( XWStreamCtxt* stream, const CtrlrNonvolatiles* nv, XP_U16 nPlayers )
{
    strm_putU32VL( stream, nv->flags );
    strm_putU32( stream, nv->lastMoveTime );
    strm_putU32( stream, nv->dupTimerExpires );

    /* number of players is upper limit on device count */
    strm_putBits( stream, NDEVICES_NBITS, nv->nDevices-1 );

    XP_ASSERT( XWSTATE_LAST <= 1<<4 );
    strm_putBits( stream, XWSTATE_NBITS, nv->gameState );
    XP_LOGFF( "wrote state: %s", getStateStr(nv->gameState) );

    /* +1: make -1 (NOTURN) into a positive number */
    XP_ASSERT( -1 <= nv->currentTurn && nv->currentTurn < MAX_NUM_PLAYERS );
    strm_putBits( stream, NPLAYERS_NBITS, nv->currentTurn+1 );
    strm_putBits( stream, NPLAYERS_NBITS, nv->quitter+1 );
    strm_putBits( stream, NPLAYERS_NBITS, nv->pendingRegistrations );

    for ( int ii = 0; ii < nPlayers; ++ii ) {
        strm_putBits( stream, 16, nv->addresses[ii].channelNo );
#ifdef STREAM_VERS_BIGBOARD
        strm_putBits( stream, 8, nv->addresses[ii].streamVersion );
#endif
    }
#ifdef STREAM_VERS_BIGBOARD
    strm_putU8( stream, nv->streamVersion );
    /* XP_LOGFF( "wrote streamVersion: 0x%x", nv->streamVersion ); */
#endif

    for ( int ii = 0; ii < nPlayers; ++ii ) {
        strm_putBits( stream, 1, nv->dupTurnsMade[ii] );
        strm_putBits( stream, 1, nv->dupTurnsForced[ii] );
    }
    strm_putBits( stream, 1, nv->dupTurnsSent );
} /* putNV */

static XWStreamCtxt*
readStreamIf( CtrlrCtxt* ctrlr, XWStreamCtxt* in )
{
    XWStreamCtxt* result = NULL;
    XP_U16 len = strm_getU16( in );
    if ( 0 < len ) {
        result = mkCtrlrStream0( ctrlr );
        strm_getFromStream( result, in, len );
    }
    return result;
}

static void
writeStreamIf( XWStreamCtxt* dest, XWStreamCtxt* src )
{
    XP_U16 len = !!src ? strm_getSize( src ) : 0;
    strm_putU16( dest, len );
    if ( 0 < len ) {
        XWStreamPos pos = strm_getPos( src, POS_READ );
        strm_getFromStream( dest, src, len );
        (void)strm_setPos( src, POS_READ, pos );
    }
}

static void
informMissing( const CtrlrCtxt* XP_UNUSED(ctrlr), XWEnv XP_UNUSED(xwe) )
{
/*     const XP_Bool isHost = amHost( ctrlr ); */
/*     RELCONST CommsCtxt* comms = ctrlr->vol.comms; */
/*     const CurGameInfo* gi = ctrlr->vol.gi; */
/*     XP_U16 nInvited = 0; */
/*     CommsAddrRec selfAddr; */
/*     CommsAddrRec* selfAddrP = NULL; */
/*     CommsAddrRec hostAddr; */
/*     CommsAddrRec* hostAddrP = NULL; */
/*     if ( !!comms ) { */
/*         selfAddrP = &selfAddr; */
/*         comms_getSelfAddr( comms, selfAddrP ); */
/*         if ( comms_getHostAddr( comms, &hostAddr ) ) { */
/*             hostAddrP = &hostAddr; */
/*         } */
/*     } */

/*     XP_U16 nDevs = 0; */
/*     XP_U16 nPending = 0; */
/*     XP_Bool fromRematch = XP_FALSE; */
/*     if ( XWSTATE_BEGIN < ctrlr->nv.gameState ) { */
/*         /\* do nothing *\/ */
/*     } else if ( isHost ) { */
/*         nPending = ctrlr->nv.pendingRegistrations; */
/*         nDevs = ctrlr->nv.nDevices - 1; */
/*         if ( 0 < nPending ) { */
/*             comms_getInvited( comms, &nInvited ); */
/*             if ( nPending < nInvited ) { */
/*                 nInvited = nPending; */
/*             } */
/*         } */
/*         fromRematch = ctrl_isFromRematch( ctrlr ); */
/*     } else if ( ROLE_ISGUEST == gi->deviceRole ) { */
/*         nPending = gi->nPlayers - gi_countLocalPlayers( gi, XP_FALSE); */
/*     } */
    XP_LOGFF( "not calling informMissing()" );
    /* util_informMissing( *ctrlr->vol.utilp, xwe, isHost, */
    /*                     hostAddrP, selfAddrP, nDevs, nPending, nInvited, */
    /*                     fromRematch ); */
}

XP_U16
ctrl_getPendingRegs( const CtrlrCtxt* ctrlr )
{
    XP_U16 nPending = amHost( ctrlr ) ? ctrlr->nv.pendingRegistrations : 0;
    return nPending;
}

CtrlrCtxt*
ctrl_makeFromStream( XWEnv xwe, XWStreamCtxt* stream, ModelCtxt* model,
                       CommsCtxt* comms, XW_UtilCtxt** utilp, XP_U16 nPlayers )
{
    CtrlrCtxt* ctrlr;
    XP_U16 version = strm_getVersion( stream );

    ctrlr = ctrl_make( xwe, model, comms, utilp );
    getNV( stream, &ctrlr->nv, nPlayers );
    
    if ( strm_getBits(stream, 1) != 0 ) {
#ifdef MEM_DEBUG
        MemPoolCtx* mpool = util_getMemPool( *utilp, xwe );
#endif
        ctrlr->pool = pool_makeFromStream( MPPARM(mpool) stream );
    }

    for ( int ii = 0; ii < nPlayers; ++ii ) {
        CtrlrPlayer* player = &ctrlr->srvPlyrs[ii];

        player->deviceIndex = strm_getU8( stream );

        if ( strm_getU8( stream ) != 0 ) {
            player->engine = engine_makeFromStream( xwe, utilp, stream );
        }
    }

    if ( STREAM_VERS_ALWAYS_MULTI <= version
#ifndef PREV_WAS_STANDALONE_ONLY
         || XP_TRUE
#endif
         ) { 
        ctrlr->lastMoveSource = (XP_U16)strm_getBits( stream, 2 );
    }

    if ( version >= STREAM_SAVE_PREVMOVE ) {
        ctrlr->nv.prevMoveStream = readStreamIf( ctrlr, stream );
    }
    if ( version >= STREAM_SAVE_PREVWORDS ) {
        ctrlr->nv.prevWordsStream = readStreamIf( ctrlr, stream );
    }

    if ( ctrlr->vol.gi->deviceRole == ROLE_ISGUEST
         && 2 < nPlayers ) {
        readGuestAddrs( ctrlr, stream, ctrlr->nv.streamVersion );
    }

    if ( 0 != (ctrlr->nv.flags & MASK_HAVE_RIP_INFO) ) {
        struct RematchInfo ri;
        ri_fromStream( &ri, stream, ctrlr );
        ctrl_setRematchOrder( ctrlr, &ri );
    }

    /* Hack alert: recovering from an apparent bug that leaves the game
       thinking it's a client but being in the host-only XWSTATE_BEGIN
       state. */
    if ( ctrlr->nv.gameState == XWSTATE_BEGIN &&
         ctrlr->vol.gi->deviceRole == ROLE_ISGUEST ) {
        SRVR_LOGFF( "fixing state" );
        SETSTATE( ctrlr, XWSTATE_NONE );
    }

    informMissing( ctrlr, xwe );
    return ctrlr;
} /* ctrl_makeFromStream */

void
ctrl_writeToStream( const CtrlrCtxt* ctrlr, XWStreamCtxt* stream )
{
    const XP_U16 nPlayers = ctrlr->vol.gi->nPlayers;

    putNV( stream, &ctrlr->nv, nPlayers );

    strm_putBits( stream, 1, ctrlr->pool != NULL );
    if ( ctrlr->pool != NULL ) {
        pool_writeToStream( ctrlr->pool, stream );
    }

    for ( int ii = 0; ii < nPlayers; ++ii ) {
        const CtrlrPlayer* player = &ctrlr->srvPlyrs[ii];

        strm_putU8( stream, player->deviceIndex );

        strm_putU8( stream, (XP_U8)(player->engine != NULL) );
        if ( player->engine != NULL ) {
            engine_writeToStream( player->engine, stream );
        }
    }

    strm_putBits( stream, 2, ctrlr->lastMoveSource );

    writeStreamIf( stream, ctrlr->nv.prevMoveStream );
    writeStreamIf( stream, ctrlr->nv.prevWordsStream );

    if ( ctrlr->vol.gi->deviceRole == ROLE_ISGUEST
         && 2 < nPlayers ) {
        XP_U16 len = ctrlr->nv.rematch.addrsLen;
        strm_putU32VL( stream, len );
        strm_putBytes( stream, ctrlr->nv.rematch.addrs, len );
    }

    if ( 0 != (ctrlr->nv.flags & MASK_HAVE_RIP_INFO) ) {
        XP_ASSERT( !!ctrlr->nv.rematch.order );
        ri_toStream( stream, ctrlr->nv.rematch.order, ctrlr );
    }
} /* ctrl_writeToStream */

#ifdef XWFEATURE_RELAY
void
ctrl_onRoleChanged( CtrlrCtxt* ctrlr, XWEnv xwe, XP_Bool amNowGuest )
{
    if ( amNowGuest == amHost(ctrlr) ) { /* do I need to change */
        XP_ASSERT ( amNowGuest );
        if ( amNowGuest ) {
            ctrlr->vol.gi->deviceRole = ROLE_ISGUEST;
            ctrl_reset( ctrlr, xwe, ctrlr->vol.comms );

            SETSTATE( ctrlr, XWSTATE_NEWCLIENT );
            ctrl_addIdle( ctrlr, xwe );
        }
    }
}
#endif

static void
cleanupCtrlr( CtrlrCtxt* ctrlr )
{
    for ( XP_U16 ii = 0; ii < VSIZE(ctrlr->srvPlyrs); ++ii ){
        CtrlrPlayer* player = &ctrlr->srvPlyrs[ii];
        if ( player->engine != NULL ) {
            engine_destroy( player->engine );
        }
    }
    XP_MEMSET( ctrlr->srvPlyrs, 0, sizeof(ctrlr->srvPlyrs) );

    if ( ctrlr->pool != NULL ) {
        pool_destroy( ctrlr->pool );
        ctrlr->pool = (PoolContext*)NULL;
    }

    strm_destroyp( &ctrlr->nv.prevMoveStream );
    strm_destroyp( &ctrlr->nv.prevWordsStream );

    XP_FREEP( ctrlr->mpool, &ctrlr->nv.rematch.addrs );
    XP_FREEP( ctrlr->mpool, &ctrlr->nv.rematch.order );

    XP_MEMSET( &ctrlr->nv, 0, sizeof(ctrlr->nv) );
} /* cleanupCtrlr */

void
ctrl_reset( CtrlrCtxt* ctrlr, XWEnv xwe, CommsCtxt* comms )
{
    SRVR_LOGFF(" ");
    CtrlrVolatiles vol = ctrlr->vol;

    cleanupCtrlr( ctrlr );

    vol.comms = comms;
    ctrlr->vol = vol;

    initCtrlr( ctrlr, xwe );
} /* ctrl_reset */

void
ctrl_destroyp( CtrlrCtxt** ctrlrp )
{
    if ( !!*ctrlrp ) {
        CtrlrCtxt* ctrlr = *ctrlrp;
        cleanupCtrlr( ctrlr );

        XP_FREEP( ctrlr->mpool, ctrlrp );
    }
} /* ctrl_destroy */

#ifdef XWFEATURE_SLOW_ROBOT
static int
figureSleepTime( const CtrlrCtxt* ctrlr )
{
    int result = 0;
    XP_U16 min = ctrlr->nv.robotThinkMin;
    XP_U16 max = ctrlr->nv.robotThinkMax;
    if ( min < max ) {
        int diff = max - min + 1;
        result = XP_RANDOM() % diff;
    }
    result += min;

    return result;
}
#endif

void
ctrl_prefsChanged( CtrlrCtxt* ctrlr, const CommonPrefs* cp )
{
    ctrlr->nv.showRobotScores = cp->showRobotScores;
    ctrlr->nv.sortNewTiles = cp->sortNewTiles;
    ctrlr->nv.skipMQTTAdd = cp->skipMQTTAdd;
#ifdef XWFEATURE_SLOW_ROBOT
    ctrlr->nv.robotThinkMin = cp->robotThinkMin;
    ctrlr->nv.robotThinkMax = cp->robotThinkMax;
    ctrlr->nv.robotTradePct = cp->robotTradePct;
#endif
#ifdef XWFEATURE_ROBOTPHONIES
    ctrlr->nv.makePhonyPct = cp->makePhonyPct;
#endif
} /* ctrl_prefsChanged */

XP_S16
ctrl_countTilesInPool( CtrlrCtxt* ctrlr )
{
    XP_S16 result = -1;
    PoolContext* pool = ctrlr->pool;
    if ( !!pool ) {
        result = pool_getNTilesLeft( pool );
    }
    return result;
} /* ctrl_countTilesInPool */

/* I'm a client device.  It's my job to start the whole conversation by
 * contacting the ctrlr and telling him that I exist (and some other stuff,
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
addMQTTDevIDIf( CtrlrCtxt* ctrlr, XWEnv xwe, XWStreamCtxt* stream )
{
    CommsAddrRec selfAddr = {};
    comms_getSelfAddr( ctrlr->vol.comms, &selfAddr );
    if ( addr_hasType( &selfAddr, COMMS_CONN_MQTT ) ) {
        MQTTDevID devID;
        dvc_getMQTTDevID( ctrlr->vol.dutil, xwe, &devID );

        XP_UCHAR buf[32];
        formatMQTTDevID( &devID, buf, VSIZE(buf) );
        stringToStream( stream, buf );
    }
}

static void
readMQTTDevID( CtrlrCtxt* ctrlr, XWStreamCtxt* stream )
{
    if ( 0 < strm_getSize( stream ) ) {
        XP_UCHAR buf[32];
        stringFromStreamHere( stream, buf, VSIZE(buf) );

        MQTTDevID devID;
        if ( strToMQTTCDevID( buf, &devID ) ) {
            if ( ctrlr->nv.skipMQTTAdd ) {
                SRVR_LOGFF( "skipMQTTAdd: %s", boolToStr(ctrlr->nv.skipMQTTAdd) );
            } else {
                XP_PlayerAddr channelNo = strm_getAddress( stream );
                comms_addMQTTDevID( ctrlr->vol.comms, channelNo, &devID );
            }
        }
    }
}

/* Build a RematchInfo from the perspective of the guest we're sending it to,
   with the addresses of all the devices not it. Rather than include my
   address, which the guest knows already, add an empty address as a
   placeholder. Guest will replace it if needed. */
static void
buildGuestRI( const CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 guestIndex, RematchInfo* rip )
{
    XP_MEMSET( rip, 0, sizeof(*rip) );

    const CurGameInfo* gi = ctrlr->vol.gi;
    for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
        const LocalPlayer* lp = &gi->players[ii];
        if ( lp->isLocal ) {    /* that's me, the host */
            CommsAddrRec addr = {};
            ri_addAddrAt( ctrlr->vol.dutil, xwe, rip, &addr, ii );
        } else {
            XP_S8 deviceIndex = ctrlr->srvPlyrs[ii].deviceIndex;
            if ( guestIndex == deviceIndex ) {
                ri_addLocal( rip );
            } else {
                XP_PlayerAddr channelNo
                    = ctrlr->nv.addresses[deviceIndex].channelNo;
                CommsAddrRec addr;
                comms_getChannelAddr( ctrlr->vol.comms, channelNo, &addr );
                ri_addAddrAt( ctrlr->vol.dutil, xwe, rip, &addr, ii );
            }
        }
    }
    LOG_RI(rip);
}

static void
loadRemoteRI( const CtrlrCtxt* ctrlr, const CurGameInfo* XP_UNUSED_DBG(gi),
              RematchInfo* rip )
{
    XWStreamCtxt* tmpStream = mkCtrlrStream( ctrlr, ctrlr->nv.streamVersion );
    strm_putBytes( tmpStream, ctrlr->nv.rematch.addrs, ctrlr->nv.rematch.addrsLen );

    ri_fromStream( rip, tmpStream, ctrlr );
    strm_destroy( tmpStream );

    /* Now find the unaddressed host and add its address */
    XP_ASSERT( rip->nPlayers == gi->nPlayers );

    ri_addHostAddrs( rip, ctrlr );

    LOG_RI( rip );
}

static void
addGuestAddrsIf( const CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 sendee,
                 XWStreamCtxt* stream )
{
    SRVR_LOGFF("(sendee: %d)", sendee );
    XP_ASSERT( amHost( ctrlr ) );
    XP_U16 version = strm_getVersion( stream );
    XP_ASSERT( version == ctrlr->nv.streamVersion );
    if ( STREAM_VERS_REMATCHADDRS <= version
         /* Not needed for two-device games */
         && 2 < ctrlr->nv.nDevices ) {
        XWStreamCtxt* tmpStream = mkCtrlrStream( ctrlr, version );
        XP_Bool skipIt = XP_FALSE;

        if ( STREAM_VERS_REMATCHORDER <= version ) {
            RematchInfo ri;
            buildGuestRI( ctrlr, xwe, sendee, &ri );
            ri_toStream( tmpStream, &ri, ctrlr );

            /* Old version requires no two-player devices  */
        } else if ( ctrlr->nv.nDevices == ctrlr->vol.gi->nPlayers ) {
            for ( XP_U16 devIndex = 1; devIndex < ctrlr->nv.nDevices; ++devIndex ) {
                if ( devIndex == sendee ) {
                    continue;
                }
                XP_PlayerAddr channelNo
                    = ctrlr->nv.addresses[devIndex].channelNo;
                CommsAddrRec addr;
                comms_getChannelAddr( ctrlr->vol.comms, channelNo, &addr );
                addrToStream( tmpStream, &addr );
            }
        } else {
            skipIt = XP_TRUE;
        }
        if ( !skipIt ) {
            XP_U16 len = strm_getSize( tmpStream );
            strm_putU32VL( stream, len );
            strm_putBytes( stream, strm_getPtr(tmpStream), len );
        }
        strm_destroy( tmpStream );
    }
}

static void
readGuestAddrs( CtrlrCtxt* ctrlr, XWStreamCtxt* stream, XP_U8 streamVersion )
{
    SRVR_LOGFFV( "version: 0x%X", streamVersion );
    if ( STREAM_VERS_REMATCHADDRS <= streamVersion && 0 < strm_getSize(stream) ) {
        XP_U16 len = ctrlr->nv.rematch.addrsLen = strm_getU32VL( stream );
        SRVR_LOGFFV( "rematch.addrsLen: %d", ctrlr->nv.rematch.addrsLen );
        if ( 0 < len ) {
            XP_ASSERT( !ctrlr->nv.rematch.addrs );
            ctrlr->nv.rematch.addrs = XP_MALLOC( ctrlr->mpool, len );
            strm_getBytes( stream, ctrlr->nv.rematch.addrs, len );
            SRVR_LOGFFV( "loaded %d bytes of rematch.addrs", len );
#ifdef DEBUG
            XWStreamCtxt* tmpStream = mkCtrlrStream( ctrlr, streamVersion );
            strm_putBytes( tmpStream, ctrlr->nv.rematch.addrs,
                             ctrlr->nv.rematch.addrsLen );

            if ( STREAM_VERS_REMATCHORDER <= streamVersion ) {
                RematchInfo ri;
                ri_fromStream( &ri, tmpStream, ctrlr );
                for ( int ii = 0; ii < ri.nAddrs; ++ii ) {
                    SRVR_LOGFFV( "got an address" );
                    logAddr( ctrlr->vol.dutil, &ri.addrs[ii], __func__ );
                }
            } else {
                while ( 0 < strm_getSize(tmpStream) ) {
                    CommsAddrRec addr = {};
                    addrFromStream( &addr, tmpStream );
                    SRVR_LOGFFV( "got an address" );
                    logAddr( ctrlr->vol.dutil, &addr, __func__ );
                }
            }
            strm_destroy( tmpStream );
#endif
        }
    }
    LOG_RETURN_VOID();
}

XP_Bool
ctrl_initClientConnection( CtrlrCtxt* ctrlr, XWEnv xwe )
{
    SRVR_LOGFFV( "gameState: %s; ", getStateStr(ctrlr->nv.gameState) );
    const CurGameInfo* gi = ctrlr->vol.gi;

    XP_ASSERT( gi->deviceRole == ROLE_ISGUEST );
    XP_Bool result = ctrlr->nv.gameState == XWSTATE_NONE;
    if ( result ) {
        XWStreamCtxt* stream = messageStreamWithHeader( ctrlr, HOST_DEVICE,
                                                        XWPROTO_DEVICE_REGISTRATION );
        XP_U16 nPlayers = gi->nPlayers;
        XP_ASSERT( nPlayers > 0 );
        XP_U16 localPlayers = gi_countLocalPlayers( gi, XP_FALSE);
        XP_ASSERT( 0 < localPlayers );
        strm_putBits( stream, NPLAYERS_NBITS, localPlayers );

        for ( int ii = 0; ii < nPlayers; ++ii ) {
            const LocalPlayer* lp = &gi->players[ii];

#ifdef DEBUG
            XP_ASSERT( ii < MAX_NUM_PLAYERS );
#endif
            if ( !lp->isLocal ) {
                continue;
            }

            strm_putBits( stream, 1, LP_IS_ROBOT(lp) ); /* better not to
                                                             send this */
            /* The first nPlayers players are the ones we'll use.  The local flag
               doesn't matter when for ROLE_ISGUEST. */
            const XP_UCHAR* name = emptyStringIfNull(lp->name);
            XP_U16 len = XP_STRLEN(name);
            if ( len > MAX_NAME_LEN ) {
                len = MAX_NAME_LEN;
            }
            strm_putBits( stream, NAME_LEN_NBITS, len );
            strm_putBytes( stream, name, len );
            SRVR_LOGFFV( "wrote local name %s", name );
        }
#ifdef STREAM_VERS_BIGBOARD
        strm_putU8( stream, CUR_STREAM_VERS );
#endif
        closeAndSend( ctrlr, xwe, stream );
    } else {
        SRVR_LOGFF( "wierd state: %s (expected XWSTATE_NONE); doing nothing",
                    getStateStr(ctrlr->nv.gameState) );
    }
    SRVR_LOGFF( "=> %s", boolToStr(result) );
    return result;
} /* ctrl_initClientConnection */

static void
sendChatTo( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 devIndex, const XP_UCHAR* msg,
            XP_S8 from, XP_U32 timestamp )
{
    if ( comms_canChat( ctrlr->vol.comms ) ) {
        XWStreamCtxt* stream = messageStreamWithHeader( ctrlr, devIndex,
                                                        XWPROTO_CHAT );
        stringToStream( stream, msg );
        strm_putU8( stream, from );
        strm_putU32( stream, timestamp );
        closeAndSend( ctrlr, xwe, stream );
    } else {
        SRVR_LOGFF( "dropping chat %s; queue too full?", msg );
    }
}

static void
sendChatToClientsExcept( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 skip,
                         const XP_UCHAR* msg, XP_S8 from, XP_U32 timestamp )
{
    for ( XP_U16 devIndex = 1; devIndex < ctrlr->nv.nDevices; ++devIndex ) {
        if ( devIndex != skip ) {
            sendChatTo( ctrlr, xwe, devIndex, msg, from, timestamp );
        }
    }
}

void
ctrl_sendChat( CtrlrCtxt* ctrlr, XWEnv xwe, const XP_UCHAR* msg,
                 XP_S16 fromHint )
{
    const CurGameInfo* gi = ctrlr->vol.gi;
    /* The player sending must be local. Caller (likely board) tells us what
       player is selected, which is who the sender should be IFF it's a local
       player, but once the game's over it might not be. */
    fromHint = gi_getLocalPlayer( gi, fromHint );

    XP_ASSERT( gi->players[fromHint].isLocal );
    XP_U32 timestamp = dutil_getCurSeconds( ctrlr->vol.dutil, xwe );
    if ( gi->deviceRole == ROLE_ISGUEST ) {
        sendChatTo( ctrlr, xwe, HOST_DEVICE, msg, fromHint, timestamp );
    } else {
        sendChatToClientsExcept( ctrlr, xwe, HOST_DEVICE, msg, fromHint,
                                 timestamp );
    }
    model_addChat( ctrlr->vol.model, xwe, msg, fromHint, timestamp );
}

static XP_Bool
receiveChat( CtrlrCtxt* ctrlr, XWEnv xwe, XWStreamCtxt* incoming,
             XP_Bool* needsChatNotifyP )
{
    XP_UCHAR* msg = stringFromStream( ctrlr->mpool, incoming );
    XP_S16 from = -1;
    if ( 1 <= strm_getSize( incoming ) ) {
        from = strm_getU8( incoming );
        if ( ctrlr->vol.gi->players[from].isLocal ) { /* fail.... */
            from = -1;          /* means it's wrong/unknown */
        }
    }

    XP_U32 timestamp = sizeof(timestamp) <= strm_getSize( incoming )
        ? strm_getU32( incoming ) : 0;
    if ( amHost( ctrlr ) ) {
        XP_U16 sourceClientIndex = getIndexForStream( ctrlr, incoming );
        sendChatToClientsExcept( ctrlr, xwe, sourceClientIndex, msg, from,
                                 timestamp );
    }

    model_chatReceived( ctrlr->vol.model, xwe, msg, from, timestamp );
    if ( !util_showChat( *ctrlr->vol.utilp, xwe, msg, from, timestamp ) ) {
        *needsChatNotifyP = XP_TRUE;
        gr_postEvents( ctrlr->vol.dutil, xwe, ctrlr->vol.gr, GCE_CHAT_ARRIVED );
    }
    XP_FREE( ctrlr->mpool, msg );
    return XP_TRUE;
}

static void
callTurnChangeListener( const CtrlrCtxt* ctrlr, XWEnv xwe )
{
    if ( ctrlr->vol.turnChangeListener != NULL ) {
        (*ctrlr->vol.turnChangeListener)( xwe, ctrlr->vol.turnChangeData );
    }
} /* callTurnChangeListener */

static void
callDupTimerListener( const CtrlrCtxt* ctrlr, XWEnv xwe, XP_S32 oldVal, XP_S32 newVal )
{
    if ( ctrlr->vol.timerChangeListener != NULL ) {
        (*ctrlr->vol.timerChangeListener)( xwe, ctrlr->vol.timerChangeData,
                                            ctrlr->vol.gi->gameID, oldVal, newVal );
    } else {
        SRVR_LOGFF( "no listener!!" );
    }
}

# ifdef STREAM_VERS_BIGBOARD
static void
setStreamVersion( CtrlrCtxt* ctrlr, XWEnv xwe )
{
    XP_ASSERT( amHost( ctrlr ) );
    XP_U8 streamVersion = CUR_STREAM_VERS;
    for ( XP_U16 devIndex = 1; devIndex < ctrlr->nv.nDevices; ++devIndex ) {
        XP_U8 devVersion = ctrlr->nv.addresses[devIndex].streamVersion;
        if ( devVersion < streamVersion ) {
            streamVersion = devVersion;
        }
    }
    SRVR_LOGFFV( "setting streamVersion: 0x%x", streamVersion );
    ctrlr->nv.streamVersion = streamVersion;

    /* If we're downgrading stream to accomodate an older guest, we need to
       re-write gi in that version in case there are newer features the game
       can't support, e.g. allowing to trade with less than a full tray left
       in the pool. */
    if ( CUR_STREAM_VERS != streamVersion ) {
        const CurGameInfo* gip = ctrlr->vol.gi;
        XP_U16 oldTraySize = gip->traySize;
        XP_U16 oldBingoMin = gip->bingoMin;

        XWStreamCtxt* tmp = mkCtrlrStream( ctrlr, streamVersion );
        gi_writeToStream( tmp, gip );
        CurGameInfo tmpGi = gi_readFromStream2( tmp );
        strm_destroy( tmp );
        /* If downgrading forced tray size change, model needs to know. BUT:
           the guest would have to be >two years old now for this to happen. */
        if ( oldTraySize != tmpGi.traySize || oldBingoMin != tmpGi.bingoMin ) {
            XP_ASSERT( 7 == tmpGi.traySize && 7 == tmpGi.bingoMin );
            if ( 7 == tmpGi.traySize && 7 == tmpGi.bingoMin ) {
                model_forceStack7Tiles( ctrlr->vol.model );

                CurGameInfo updated = {};
                gi_copy( &updated, gip );
                updated.traySize = updated.bingoMin = 7;
                gr_setGI( ctrlr->vol.dutil, ctrlr->vol.gr, xwe, &updated );
            }
        }
    }
}

static void
checkResizeBoard( CtrlrCtxt* ctrlr, XWEnv xwe )
{
    const CurGameInfo* gip = ctrlr->vol.gi;
    if ( STREAM_VERS_BIGBOARD > ctrlr->nv.streamVersion && gip->boardSize > 15) {
        CurGameInfo gi;
        gi_copy( &gi, gip );
        SRVR_LOGFF( "dropping board size from %d to 15", gip->boardSize );
        gi.boardSize = 15;
        model_setSize( ctrlr->vol.model, 15 );
        gr_setGI( ctrlr->vol.dutil, ctrlr->vol.gr, xwe, &gi );
    }
}
# else
#  define setStreamVersion(s, xwe)
#  define checkResizeBoard(s, xwe)
# endif

static XP_Bool
handleRegistrationMsg( CtrlrCtxt* ctrlr, XWEnv xwe, XWStreamCtxt* stream )
{
    XP_Bool success = XP_TRUE;
    XP_U16 playersInMsg;
    XP_S8 clientIndex = 0;      /* quiet compiler */
    XP_U16 ii = 0;
    LOG_FUNC();

    /* code will have already been consumed */
    playersInMsg = (XP_U16)strm_getBits( stream, NPLAYERS_NBITS );
    XP_ASSERT( playersInMsg > 0 );

    if ( ctrlr->nv.pendingRegistrations < playersInMsg ) {
        SRVR_LOGFF( "got %d players but missing only %d",
                    playersInMsg, ctrlr->nv.pendingRegistrations );
        util_userError( *ctrlr->vol.utilp, xwe, ERR_REG_UNEXPECTED_USER );
        sts_increment( ctrlr->vol.dutil, xwe, STAT_REG_NOROOM );
        success = XP_FALSE;
    } else {
#ifdef DEBUG
        XP_S8 prevIndex = -1;
#endif
        for ( ; ii < playersInMsg; ++ii ) {
            clientIndex = registerRemotePlayer( ctrlr, xwe, stream );
            if ( -1 == clientIndex ) {
                success = XP_FALSE;
                break;
            }

            /* This is abusing the semantics of turn change -- at least in the
               case where there is another device yet to register -- but we
               need to let the board know to redraw the scoreboard with more
               players there. */
            callTurnChangeListener( ctrlr, xwe );
#ifdef DEBUG
            XP_ASSERT( ii == 0 || prevIndex == clientIndex );
            prevIndex = clientIndex;
#endif
        }

    }

    if ( success ) {
#ifdef STREAM_VERS_BIGBOARD
        if ( 0 < strm_getSize(stream) ) {
            XP_U8 streamVersion = strm_getU8( stream );
            if ( streamVersion >= STREAM_VERS_BIGBOARD ) {
                SRVR_LOGFF( "setting addresses[%d] streamVersion to 0x%x "
                            "(CUR_STREAM_VERS is 0x%x)",
                            clientIndex, streamVersion, CUR_STREAM_VERS );
                ctrlr->nv.addresses[clientIndex].streamVersion = streamVersion;
            }
        }
#endif

        if ( ctrlr->nv.pendingRegistrations == 0 ) {
            XP_ASSERT( ii == playersInMsg ); /* otherwise malformed */
            setStreamVersion( ctrlr, xwe );
            checkResizeBoard( ctrlr, xwe );
            (void)assignTilesToAll( ctrlr, xwe );
            /* We won't need this any more */
            XP_FREEP( ctrlr->mpool, &ctrlr->nv.rematch.order );
            ctrlr->nv.flags &= ~MASK_HAVE_RIP_INFO;
            SETSTATE( ctrlr, XWSTATE_RECEIVED_ALL_REG );
        }
        informMissing( ctrlr, xwe );
    }

    return success;
} /* handleRegistrationMsg */

static XP_U16
bitsPerTile( CtrlrCtxt* ctrlr )
{
    if ( 0 == ctrlr->vol.bitsPerTile ) {
        const DictionaryCtxt* dict = model_getDictionary( ctrlr->vol.model );
        XP_U16 nFaces = dict_numTileFaces( dict );
        ctrlr->vol.bitsPerTile = nFaces <= 32? 5 : 6;
    }
    return ctrlr->vol.bitsPerTile;
}

static void
dupe_setupShowTrade( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 nTiles )
{
    XP_ASSERT( inDuplicateMode(ctrlr) );
    XP_ASSERT( !ctrlr->nv.prevMoveStream );

    XP_UCHAR buf[128];
    const XP_UCHAR* fmt = dutil_getUserString( ctrlr->vol.dutil, xwe, STRD_DUP_TRADED );
    XP_SNPRINTF( buf, VSIZE(buf), fmt, nTiles );

    XWStreamCtxt* stream = mkCtrlrStream0( ctrlr );
    strm_catString( stream, buf );

    ctrlr->nv.prevMoveStream = stream;
    ctrlr->vol.showPrevMove = XP_TRUE;
}

static void
dupe_setupShowMove( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16* scores )
{
    XP_ASSERT( inDuplicateMode(ctrlr) );

    const CurGameInfo* gi = ctrlr->vol.gi;
    const XP_U16 nPlayers = gi->nPlayers;

    XWStreamCtxt* stream = mkCtrlrStream0( ctrlr );

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
        const XP_UCHAR* fmt = dutil_getUserString( ctrlr->vol.dutil, xwe,
                                                   STRSD_DUP_ONESCORE );
        for ( XP_U16 ii = 0; ii < nPlayers; ++ii ) {
            if ( scores[ii] == thisMax ) {
                ++nDone;
                XP_UCHAR buf[128];
                XP_SNPRINTF( buf, VSIZE(buf), fmt, gi->players[ii].name, scores[ii] );
                strm_catString( stream, buf );
            }
        }
        lastMax = thisMax;
    }

    XP_ASSERT( !ctrlr->nv.prevMoveStream );
    ctrlr->nv.prevMoveStream = stream;
    ctrlr->vol.showPrevMove = XP_TRUE;
}

static void
addDupeStuffMark( XWStreamCtxt* stream, DUPE_STUFF typ )
{
    strm_putBits( stream, 3, typ );
}

static DUPE_STUFF
getDupeStuffMark( XWStreamCtxt* stream )
{
    return (DUPE_STUFF)strm_getBits( stream, 3 );
}

/* Called on ctrlr when client has sent a message giving its local players'
   duplicate moves for a single turn. */
static XP_Bool
dupe_handleClientMoves( CtrlrCtxt* ctrlr, XWEnv xwe, XWStreamCtxt* stream )
{
    LOG_FUNC();
    ModelCtxt* model = ctrlr->vol.model;
    XP_Bool success = XP_TRUE;

    XP_U16 movesInMsg = (XP_U16)strm_getBits( stream, NPLAYERS_NBITS );
    SRVR_LOGFF( "reading %d moves", movesInMsg );
    for ( XP_U16 ii = 0; success && ii < movesInMsg; ++ii ) {
        XP_U16 turn = (XP_U16)strm_getBits( stream, PLAYERNUM_NBITS );
        XP_Bool forced = (XP_Bool)strm_getBits( stream, 1 );

        model_resetCurrentTurn( model, xwe, turn );
        success = model_makeTurnFromStream( model, xwe, turn, stream );
        XP_ASSERT( success );   /* shouldn't fail in duplicate case */
        if ( success ) {
            XP_ASSERT( !ctrlr->nv.dupTurnsMade[turn] ); /* firing */
            XP_ASSERT( !ctrlr->vol.gi->players[turn].isLocal );
            ctrlr->nv.dupTurnsMade[turn] = XP_TRUE;
            ctrlr->nv.dupTurnsForced[turn] = forced;
        }
    }

    if ( success ) {
        dupe_checkTurns( ctrlr, xwe );
        nextTurn( ctrlr, xwe, PICK_NEXT );
    }

    LOG_RETURNF( "%d", success );
    return success;
}

static void
updateOthersTiles( CtrlrCtxt* ctrlr, XWEnv xwe )
{
    sortTilesIf( ctrlr, DUP_PLAYER );
    model_cloneDupeTrays( ctrlr->vol.model, xwe );
}

static XP_Bool
checkDupTimerProc( void* closure, XWEnv xwe, XWTimerReason XP_UNUSED_DBG(XP_why) )
{
    XP_ASSERT( XP_why == TIMER_DUP_TIMERCHECK );
    CtrlrCtxt* ctrlr = (CtrlrCtxt*)closure;
    XP_ASSERT( inDuplicateMode( ctrlr ) );
    // Don't call ctrl_do() if the timer hasn't fired yet
    return setDupCheckTimer( ctrlr, xwe ) || doImpl( ctrlr, xwe );
}

static XP_Bool
setDupCheckTimer( CtrlrCtxt* ctrlr, XWEnv xwe )
{
    XP_Bool set = XP_FALSE;
    XP_U32 now = dutil_getCurSeconds( ctrlr->vol.dutil, xwe );
    if ( ctrlr->nv.dupTimerExpires > 0 && ctrlr->nv.dupTimerExpires > now ) {
        XP_U32 diff = ctrlr->nv.dupTimerExpires - now;
        XP_ASSERT( diff <= 0x7FFF );
        XP_U16 whenSeconds = (XP_U16) diff;
        util_setTimer( *ctrlr->vol.utilp, xwe, TIMER_DUP_TIMERCHECK,
                       whenSeconds, checkDupTimerProc, ctrlr );
        set = XP_TRUE;
    }
    return set;
}

static void
setDupTimerExpires( CtrlrCtxt* ctrlr, XWEnv xwe, XP_S32 newVal )
{
    SRVR_LOGFF( "(%d)", newVal );
    if ( newVal != ctrlr->nv.dupTimerExpires ) {
        XP_S32 oldVal = ctrlr->nv.dupTimerExpires;
        ctrlr->nv.dupTimerExpires = newVal;
        callDupTimerListener( ctrlr, xwe, oldVal, newVal );
    }
}

static void
resetDupeTimerIf( CtrlrCtxt* ctrlr, XWEnv xwe )
{
    if ( inDuplicateMode( ctrlr ) ) {
        dupe_resetTimer( ctrlr, xwe );
    }
}

static void
dupe_resetTimer( CtrlrCtxt* ctrlr, XWEnv xwe )
{
    XP_S32 newVal = 0;
    if ( ctrlr->vol.gi->timerEnabled && 0 < ctrlr->vol.gi->gameSeconds ) {
        XP_U32 now = dutil_getCurSeconds( ctrlr->vol.dutil, xwe );
        newVal = now + ctrlr->vol.gi->gameSeconds;
    } else {
        SRVR_LOGFF( "doing nothing because timers disabled" );
    }

    if ( ctrl_canUnpause( ctrlr ) ) {
        XP_U32 now = dutil_getCurSeconds( ctrlr->vol.dutil, xwe );
        newVal = -(newVal - now);
    }
    setDupTimerExpires( ctrlr, xwe, newVal );

    setDupCheckTimer( ctrlr, xwe );
}

XP_S32
ctrl_getDupTimerExpires( const CtrlrCtxt* ctrlr )
{
    return ctrlr->nv.dupTimerExpires;
}

/* If we're in dup mode, this is 0 if no timer otherwise the number of seconds
   left. */
XP_S16
ctrl_getTimerSeconds( const CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 turn )
{
    XP_S16 result;
    if ( inDuplicateMode( ctrlr ) ) {
        XP_S32 dupTimerExpires = ctrlr->nv.dupTimerExpires;
        if ( dupTimerExpires <= 0 ) {
            result = (XP_S16)-dupTimerExpires;
        } else {
            XP_U32 now = dutil_getCurSeconds( ctrlr->vol.dutil, xwe );
            result = dupTimerExpires > now ? dupTimerExpires - now : 0;
        }
        XP_ASSERT( result >= 0 ); /* should never go negative */
    } else {
        const CurGameInfo* gi = ctrlr->vol.gi;
        XP_U16 secondsUsed = model_getSecondsUsed( ctrlr->vol.model, turn );
        XP_U16 secondsAvailable = gi->gameSeconds / gi->nPlayers;
        XP_ASSERT( gi->timerEnabled );
        result = secondsAvailable - secondsUsed;
    }
    return result;
}

XP_Bool
ctrl_canPause( const CtrlrCtxt* ctrlr )
{
    XP_Bool result = inDuplicateMode( ctrlr )
        && 0 < ctrl_getDupTimerExpires( ctrlr );
    /* LOG_RETURNF( "%d", result ); */
    return result;
}

XP_Bool
ctrl_canUnpause( const CtrlrCtxt* ctrlr )
{
    XP_Bool result = inDuplicateMode( ctrlr )
        && 0 > ctrl_getDupTimerExpires( ctrlr );
    /* LOG_RETURNF( "%d", result ); */
    return result;
}

static void
pauseImpl( CtrlrCtxt* ctrlr, XWEnv xwe )
{
    XP_ASSERT( ctrl_canPause( ctrlr ) );
    /* Figure out how many seconds are left on the timer, and set timer to the
       negative of that (since negative is the flag) */
    XP_U32 now = dutil_getCurSeconds( ctrlr->vol.dutil, xwe );
    setDupTimerExpires( ctrlr, xwe, -(ctrlr->nv.dupTimerExpires - now) );
    XP_ASSERT( 0 > ctrlr->nv.dupTimerExpires );
    XP_ASSERT( ctrl_canUnpause( ctrlr ) );
}

void
ctrl_pause( CtrlrCtxt* ctrlr, XWEnv xwe, XP_S16 turn, const XP_UCHAR* msg )
{
    SRVR_LOGFF( "(turn=%d)", turn );
    pauseImpl( ctrlr, xwe );
    /* Figure out how many seconds are left on the timer, and set timer to the
       negative of that (since negative is the flag) */
    dupe_transmitPause( ctrlr, xwe, PAUSED, turn, msg, -1 );
    model_noteDupePause( ctrlr->vol.model, xwe, PAUSED, turn, msg );
    LOG_RETURN_VOID();
}

static void
dupe_autoPause( CtrlrCtxt* ctrlr, XWEnv xwe )
{
    /* Reset timer: we're starting turn over */
    dupe_resetTimer( ctrlr, xwe );
    dupe_clearState( ctrlr );

    /* Then pause us */
    pauseImpl( ctrlr, xwe );

    dupe_transmitPause( ctrlr, xwe, AUTOPAUSED, 0, NULL, -1 );
    model_noteDupePause( ctrlr->vol.model, xwe, AUTOPAUSED, -1, NULL );
    LOG_RETURN_VOID();
}

void
ctrl_unpause( CtrlrCtxt* ctrlr, XWEnv xwe, XP_S16 turn, const XP_UCHAR* msg )
{
    SRVR_LOGFF( "(turn=%d)", turn );
    XP_ASSERT( ctrl_canUnpause( ctrlr ) );
    XP_U32 now = dutil_getCurSeconds( ctrlr->vol.dutil, xwe );
    /* subtract because it's negative */
    setDupTimerExpires( ctrlr, xwe, now - ctrlr->nv.dupTimerExpires );
    XP_ASSERT( ctrl_canPause( ctrlr ) );
    dupe_transmitPause( ctrlr, xwe, UNPAUSED, turn, msg, -1 );
    model_noteDupePause( ctrlr->vol.model, xwe, UNPAUSED, turn, msg );
    LOG_RETURN_VOID();
}

/* Called on client. Unpacks DUP move data and applies it. */
static XP_Bool
dupe_handleCtrlrMoves( CtrlrCtxt* ctrlr, XWEnv xwe, XWStreamCtxt* stream )
{
    LOG_FUNC();
    MoveInfo moveInfo;
    moveInfoFromStream( stream, &moveInfo, bitsPerTile(ctrlr) );
    TrayTileSet newTiles;
    traySetFromStream( stream, &newTiles );
    XP_ASSERT( newTiles.nTiles <= moveInfo.nTiles );
    XP_ASSERT( pool_containsTiles( ctrlr->pool, &newTiles ) );

    XP_U16 nScores = strm_getBits( stream, NPLAYERS_NBITS );
    XP_U16 scores[MAX_NUM_PLAYERS];
    XP_ASSERT( nScores <= MAX_NUM_PLAYERS );
    scoresFromStream( stream, nScores, scores );

    dupe_resetTimer( ctrlr, xwe );

    pool_removeTiles( ctrlr->pool, &newTiles );
    model_commitDupeTurn( ctrlr->vol.model, xwe, &moveInfo, nScores, scores,
                          &newTiles );

    /* Need to remove the played tiles from all local trays */
    updateOthersTiles( ctrlr, xwe );

    dupe_setupShowMove( ctrlr, xwe, scores );

    dupe_clearState( ctrlr );
    nextTurn( ctrlr, xwe, PICK_NEXT );
    LOG_RETURN_VOID();
    return XP_TRUE;
} /* dupe_handleCtrlrMoves */

static XP_Bool
dupe_handleCtrlrTrade( CtrlrCtxt* ctrlr, XWEnv xwe, XWStreamCtxt* stream )
{
    TrayTileSet oldTiles, newTiles;
    traySetFromStream( stream, &oldTiles );
    traySetFromStream( stream, &newTiles );

    ModelCtxt* model = ctrlr->vol.model;
    model_resetCurrentTurn( model, xwe, DUP_PLAYER );
    model_removePlayerTiles( model, DUP_PLAYER, &oldTiles );
    pool_replaceTiles( ctrlr->pool, &oldTiles );
    pool_removeTiles( ctrlr->pool, &newTiles );

    model_commitDupeTrade( model, &oldTiles, &newTiles );

    model_addNewTiles( model, DUP_PLAYER, &newTiles );
    updateOthersTiles( ctrlr, xwe );

    dupe_resetTimer( ctrlr, xwe );
    dupe_setupShowTrade( ctrlr, xwe, newTiles.nTiles );

    dupe_clearState( ctrlr );
    nextTurn( ctrlr, xwe, PICK_NEXT );
    return XP_TRUE;
}

/* Just for grins....trade in all the tiles that weren't used in the
 * move the robot manage to make.  This is not meant to be strategy, but
 * rather to force me to make the trade-communication stuff work well.
 */
#if 0
static void
robotTradeTiles( CtrlrCtxt* ctrlr, MoveInfo* newMove )
{
    Tile tradeTiles[MAX_TRAY_TILES];
    XP_S16 turn = ctrlr->nv.currentTurn;
    Tile* curTiles = model_getPlayerTiles( ctrlr->model, turn );
    XP_U16 numInTray = model_getNumPlayerTiles( ctrlr->model, turn );
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
mkCtrlrStream0( const CtrlrCtxt* ctrlr )
{
    return mkCtrlrStream( ctrlr, 0 );
}

static XWStreamCtxt*
mkCtrlrStream( const CtrlrCtxt* ctrlr, XP_U8 version )
{
    XWStreamCtxt* stream =
        strm_make_raw( MPPARM_NOCOMMA(ctrlr->mpool) );
    XP_ASSERT( !!stream );
    strm_setVersion( stream, version );
    return stream;
} /* mkCtrlrStream */


static void
setOrReplace( XWStreamCtxt** loc, XWStreamCtxt* stream )
{
    if ( !!*loc ) {
        XP_LOGFF( "dropping stream -- fix this" );
        strm_destroy( *loc );
    }
    *loc = stream;
}

static XP_Bool
makeRobotMove( CtrlrCtxt* ctrlr, XWEnv xwe )
{
    LOG_FUNC();
    XP_Bool result = XP_FALSE;
    XP_Bool searchComplete = XP_FALSE;
    MoveInfo newMove = {};
    ModelCtxt* model = ctrlr->vol.model;
    const CurGameInfo* gi = ctrlr->vol.gi;
    XP_Bool canMove;
    XW_DUtilCtxt* dutil = ctrlr->vol.dutil;
    XP_Bool forceTrade = XP_FALSE;
    XP_U32 time = gi->timerEnabled ? dutil_getCurSeconds( dutil, xwe ) : 0L;

#ifdef XWFEATURE_SLOW_ROBOT
    if ( 0 != ctrlr->nv.robotTradePct ) {
        XP_ASSERT( ! inDuplicateMode( ctrlr ) );
        if ( ctrl_countTilesInPool( ctrlr ) >= gi->traySize ) {
            XP_U16 pct = XP_RANDOM() % 100;
            forceTrade = pct < ctrlr->nv.robotTradePct ;
        }
    }
#endif

    XP_S16 turn = ctrlr->nv.currentTurn;
    XP_ASSERT( turn >= 0 );

    /* If the player's been recently turned into a robot while he had some
       pending tiles on the board we'll have problems.  It'd be best to detect
       this and put 'em back when that happens.  But for now we'll just be
       paranoid.  PENDING(ehouse) */
    model_resetCurrentTurn( model, xwe, turn );

    if ( !forceTrade ) {
        const TrayTileSet* tileSet = model_getPlayerTiles( model, turn );
#ifdef XWFEATURE_BONUSALL
        XP_U16 allTilesBonus = ctrl_figureFinishBonus( ctrlr, turn );
#endif
        XP_ASSERT( !!ctrl_getEngineFor( ctrlr, xwe, turn ) );
        searchComplete = engine_findMove( ctrl_getEngineFor( ctrlr, xwe, turn ),
                                          xwe, model, turn, XP_FALSE,
#ifdef XWFEATURE_STOP_ENGINE
                                          XP_TRUE,
#endif
                                          tileSet, XP_FALSE,
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
             (ctrl_countTilesInPool( ctrlr ) >= gi->traySize));

        ctrlr->vol.showPrevMove = XP_TRUE;
        if ( inDuplicateMode(ctrlr) || ctrlr->nv.showRobotScores ) {
            stream = mkCtrlrStream0( ctrlr );
        }

        /* trade if unable to find a move */
        if ( trade ) {
            TrayTileSet oldTiles = *model_getPlayerTiles( model, turn );
            SRVR_LOGFF( "robot trading %d tiles", oldTiles.nTiles );
            result = ctrl_commitTrade( ctrlr, xwe, &oldTiles, NULL );

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

                strm_catString( stream, buf );
                XP_ASSERT( !ctrlr->nv.prevMoveStream );
                ctrlr->nv.prevMoveStream = stream;
            }
        } else { 
            /* if canMove is false, this is a fake move, a pass */

            if ( canMove || NPASSES_OK(ctrlr) ) {
#ifdef XWFEATURE_ROBOTPHONIES
                if ( ctrlr->nv.makePhonyPct > XP_RANDOM() % 100 ) {
                    reverseTiles( &newMove );
                }
#endif
                juggleMoveIfDebug( &newMove );
                model_makeTurnFromMoveInfo( model, xwe, turn, &newMove );
                SRVR_LOGFF( "robot making %d tile move for player %d",
                            newMove.nTiles, turn );

                if ( !!stream ) {
                    XWStreamCtxt* wordsStream = mkCtrlrStream0( ctrlr );
                    WordNotifierInfo* ni = 
                        model_initWordCounter( model, wordsStream );
                    (void)model_checkMoveLegal( model, xwe, turn, stream, ni );

                    setOrReplace( &ctrlr->nv.prevMoveStream, stream );
                    setOrReplace( &ctrlr->nv.prevWordsStream, wordsStream );
                }
                result = ctrl_commitMove( ctrlr, xwe, turn, NULL );
            } else {
                result = XP_FALSE;
            }
        }
    }

    if ( gi->timerEnabled ) {
        XP_U16 seconds = (XP_U16)(dutil_getCurSeconds( dutil, xwe ) - time);
        model_augmentSecondsUsed( ctrlr->vol.model, turn, seconds );
        /* gi->players[turn].secondsUsed +=  */
    } else {
        XP_ASSERT( 0 == model_getSecondsUsed(ctrlr->vol.model, turn ) );
    }

    LOG_RETURNF( "%s", boolToStr(result) );
    return result; /* always return TRUE after robot move? */
} /* makeRobotMove */

#ifdef XWFEATURE_SLOW_ROBOT
static XP_Bool 
wakeRobotProc( void* closure, XWEnv xwe, XWTimerReason XP_UNUSED_DBG(why) )
{
    XP_ASSERT( TIMER_SLOWROBOT == why );
    CtrlrCtxt* ctrlr = (CtrlrCtxt*)closure;
    XP_ASSERT( ROBOTWAITING(ctrlr) );
    ctrlr->robotWaiting = XP_FALSE;
    ctrl_addIdle( ctrlr, xwe );
    return XP_FALSE;
}
#endif

static XP_Bool
robotMovePending( const CtrlrCtxt* ctrlr )
{
    XP_Bool result = XP_FALSE;
    XP_S16 turn = ctrlr->nv.currentTurn;
    if ( turn >= 0 && tileCountsOk(ctrlr) && NPASSES_OK(ctrlr) ) {
        const CurGameInfo* gi = ctrlr->vol.gi;
        const LocalPlayer* player = &gi->players[turn];
        result = LP_IS_ROBOT(player) && LP_IS_LOCAL(player);
    }
    return result;
} /* robotMovePending */

#ifdef XWFEATURE_SLOW_ROBOT
static XP_Bool
postponeRobotMove( CtrlrCtxt* ctrlr, XWEnv xwe )
{
    XP_Bool result = XP_FALSE;
    XP_ASSERT( robotMovePending(ctrlr) );

    if ( !ROBOTWAITING(ctrlr) ) {
        XP_U16 sleepTime = figureSleepTime(ctrlr);
        if ( 0 != sleepTime ) {
            ctrlr->robotWaiting = XP_TRUE;
            util_setTimer( *ctrlr->vol.utilp, xwe, TIMER_SLOWROBOT, sleepTime,
                           wakeRobotProc, ctrlr );
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
showPrevScore( CtrlrCtxt* ctrlr, XWEnv xwe, XP_S16 prevTurn )
{
    XP_ASSERT( inDuplicateMode( ctrlr ) || ctrlr->nv.showRobotScores );
    XP_UCHAR buf[128];
    const CurGameInfo* gi = ctrlr->vol.gi;
    const LocalPlayer* lp = &gi->players[prevTurn];

    XP_U16 stringCode;
    if ( inDuplicateMode( ctrlr ) ) {
        stringCode = STR_DUP_MOVED;
    } else if ( LP_IS_LOCAL(lp) ) {
        stringCode = STR_ROBOT_MOVED;
    } else {
        stringCode = STRS_REMOTE_MOVED;
    }
    XW_DUtilCtxt* dutil = ctrlr->vol.dutil;
    const XP_UCHAR* str = dutil_getUserString( dutil, xwe, stringCode );
    XP_SNPRINTF( buf, sizeof(buf), str, lp->name );
    str = buf;

    XWStreamCtxt* stream = mkCtrlrStream0( ctrlr );
    strm_catString( stream, str );

    XWStreamCtxt* prevStream = ctrlr->nv.prevMoveStream;
    if ( !!prevStream ) {
        ctrlr->nv.prevMoveStream = NULL;

        XP_U16 len = strm_getSize( prevStream );
        strm_putBytes( stream, strm_getPtr( prevStream ), len );
        strm_destroy( prevStream );
    }

    dutil_informMove( dutil, xwe, ctrlr->vol.gr, prevTurn, stream,
                      ctrlr->nv.prevWordsStream );
    strm_destroy( stream );

    strm_destroyp( &ctrlr->nv.prevWordsStream );
} /* showPrevScore */

void
ctrl_tilesPicked( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 player,
                    const TrayTileSet* newTilesP )
{
    XP_ASSERT( 0 == model_getNumTilesInTray( ctrlr->vol.model, player ) );
    XP_ASSERT( ctrlr->vol.pickTilesCalled[player] );
    ctrlr->vol.pickTilesCalled[player] = XP_FALSE;

    TrayTileSet newTiles = *newTilesP;
    pool_removeTiles( ctrlr->pool, &newTiles );

    fetchTiles( ctrlr, xwe, player, ctrlr->vol.gi->traySize,
                &newTiles, XP_FALSE );
    XP_ASSERT( !inDuplicateMode(ctrlr) );
    model_assignPlayerTiles( ctrlr->vol.model, player, &newTiles );

    ctrl_addIdle( ctrlr, xwe );
}

static XP_Bool
informNeedPickTiles( CtrlrCtxt* ctrlr, XWEnv xwe, XP_Bool initial,
                     XP_U16 turn, XP_U16 nToPick )
{
    ModelCtxt* model = ctrlr->vol.model;
    const DictionaryCtxt* dict = model_getDictionary(model);
    XP_U16 nFaces = dict_numTileFaces( dict );
    XP_U16 counts[MAX_UNIQUE_TILES];
    const XP_UCHAR* faces[MAX_UNIQUE_TILES];

    XP_U16 nLeft = pool_getNTilesLeft( ctrlr->pool );
    if ( nLeft < nToPick ) {
        nToPick = nLeft;
    }
    XP_Bool asking = nToPick > 0;

    if ( asking ) {
        /* We need to make sure we only call util_informNeedPickTiles once
           without it returning. Even if ctrl_do() is called a lot. */
        if ( ctrlr->vol.pickTilesCalled[turn] ) {
            SRVR_LOGFF( "already asking for player[%d]", turn );
        } else {
            ctrlr->vol.pickTilesCalled[turn] = XP_TRUE;
            for ( Tile tile = 0; tile < nFaces; ++tile ) {
                faces[tile] = dict_getTileString( dict, tile );
                counts[tile] = pool_getNTilesLeftFor( ctrlr->pool, tile );
            }
            util_informNeedPickTiles( *ctrlr->vol.utilp, xwe, initial, turn,
                                      nToPick, nFaces, faces, counts );
        }
    }
    return asking;
}

static XP_Bool
doImpl( CtrlrCtxt* ctrlr, XWEnv xwe )
{
    XP_Bool result = XP_TRUE;

    if ( ctrlr->ctrlrDoing ) {
        result = XP_FALSE;
    } else {
        XP_Bool moreToDo = XP_FALSE;
        ctrlr->ctrlrDoing = XP_TRUE;
        SRVR_LOGFFV( "gameState: %s", getStateStr(ctrlr->nv.gameState) );
        switch( ctrlr->nv.gameState ) {
        case XWSTATE_BEGIN:
            if ( ctrlr->nv.pendingRegistrations == 0 ) { /* all players on
                                                             device */
                if ( assignTilesToAll( ctrlr, xwe ) ) {
                    SETSTATE( ctrlr, XWSTATE_INTURN );
                    setTurn( ctrlr, xwe, 0 );
                    resetDupeTimerIf( ctrlr, xwe );
                    moreToDo = XP_TRUE;
                }
            }
            break;

        case XWSTATE_NEWCLIENT:
            XP_ASSERT(0);       /* no longer happens, I think */
            XP_ASSERT( !amHost( ctrlr ) );
            SETSTATE( ctrlr, XWSTATE_NONE ); /* ctrl_initClientConnection expects this */
            ctrl_initClientConnection( ctrlr, xwe );
            break;

        case XWSTATE_NEEDSEND_BADWORD_INFO:
            XP_ASSERT( ctrlr->vol.gi->deviceRole == ROLE_ISHOST );
            badWordMoveUndoAndTellUser( ctrlr, xwe, &ctrlr->bws );
            sendBadWordMsgs( ctrlr, xwe );
            nextTurn( ctrlr, xwe, PICK_NEXT );
            //moreToDo = XP_TRUE;   /* why? */
            break;

        case XWSTATE_RECEIVED_ALL_REG:
            sendInitialMessage( ctrlr, xwe );
            /* PENDING isn't INTURN_OFFDEVICE possible too?  Or just
               INTURN?  */
            SETSTATE( ctrlr, XWSTATE_INTURN );
            setTurn( ctrlr, xwe, 0 );
            moreToDo = XP_TRUE;
            ctrlr->nv.flags |= FLAG_HARVEST_READY;
            break;

        case XWSTATE_MOVE_CONFIRM_MUSTSEND:
            XP_ASSERT( ctrlr->vol.gi->deviceRole == ROLE_ISHOST );
            tellMoveWasLegal( ctrlr, xwe ); /* sets state */
            nextTurn( ctrlr, xwe, PICK_NEXT );
            break;

        case XWSTATE_NEEDSEND_ENDGAME:
            endGameInternal( ctrlr, xwe, END_REASON_OUT_OF_TILES, -1 );
            break;

        case XWSTATE_INTURN:
            if ( inDuplicateMode( ctrlr ) ) {
                /* For now, anyway; makes dev easier */
                dupe_forceCommits( ctrlr, xwe );
                dupe_checkTurns( ctrlr, xwe );
            }

            if ( robotMovePending( ctrlr ) && !ROBOTWAITING(ctrlr) ) {
                result = makeRobotMove( ctrlr, xwe );
                /* if robot was interrupted, we need to schedule again */
                moreToDo = !result || 
                    (robotMovePending( ctrlr ) && !POSTPONEROBOTMOVE(ctrlr, xwe));
            }
            break;

        default:
            XP_LOGFF( "unexpected state" );
            result = XP_FALSE;
            break;
        } /* switch */

        if ( moreToDo ) {
            ctrl_addIdle( ctrlr, xwe );
        }

        ctrlr->ctrlrDoing = XP_FALSE;
    }
    return result;
} /* ctrl_do */

static void
idleProc( XW_DUtilCtxt* XP_UNUSED(dutil), XWEnv xwe, void* closure,
          TimerKey XP_UNUSED(key), XP_Bool fired )
{
    if ( fired ) {
        CtrlrCtxt* ctrlr = (CtrlrCtxt*)closure;
        doImpl( ctrlr, xwe );
    }
}

void
ctrl_addIdle( CtrlrCtxt* ctrlr, XWEnv xwe )
{
    XW_DUtilCtxt* dutil = ctrlr->vol.dutil;
    tmr_setIdle( dutil, xwe, idleProc, ctrlr );
}

static XP_S8
getIndexForStream( const CtrlrCtxt* ctrlr, const XWStreamCtxt* stream )
{
    XP_PlayerAddr channelNo = strm_getAddress( stream );
    return getIndexForDevice( ctrlr, channelNo );
}

static XP_S8
getIndexForDevice( const CtrlrCtxt* ctrlr, XP_PlayerAddr channelNo )
{
    XP_S8 result = -1;

    for ( int ii = 0; ii < ctrlr->nv.nDevices; ++ii ) {
        const RemoteAddress* addr = &ctrlr->nv.addresses[ii];
        if ( addr->channelNo == channelNo ) {
            result = (XP_S8)ii;
            break;
        }
    }

    SRVR_LOGFFV( "(%x)=>%d", channelNo, result );
    return result;
} /* getIndexForDevice */

static XP_Bool
findFirstPending( CtrlrCtxt* ctrlr, CtrlrPlayer** spp, XP_S16* lpTurn )
{
    /* We want to find the local player and srvPlyrs slot for this
       connection. There's a srvPlyrs slot for each client that will
       register. For each we find in use, skip a non-local slot in the gi. */
    const CurGameInfo* gi = ctrlr->vol.gi;
    XP_Bool success = XP_FALSE;
    for ( int ii = 0; !success && ii < gi->nPlayers; ++ii ) {
        const LocalPlayer* lp = &gi->players[ii];
        if ( !lp->isLocal ) {
            CtrlrPlayer* sp = &ctrlr->srvPlyrs[ii];
            XP_ASSERT( HOST_DEVICE != sp->deviceIndex );
            if ( UNKNOWN_DEVICE == sp->deviceIndex ) {
                success = XP_TRUE;
                *lpTurn = ii;
                *spp = sp;
            }
        }
    }
    return success;
} /* findFirstPending */

static XP_Bool
findOrderedSlot( CtrlrCtxt* ctrlr, XWEnv xwe, XWStreamCtxt* stream,
                 CtrlrPlayer** spp, XP_S16* lpTurn )
{
    LOG_FUNC();
    const CurGameInfo* gi = ctrlr->vol.gi;
    XP_PlayerAddr channelNo = strm_getAddress( stream );
    CommsAddrRec guestAddr;
    const CommsCtxt* comms = ctrlr->vol.comms;
    comms_getChannelAddr( comms, channelNo, &guestAddr );

    const RematchInfo* rip = ctrlr->nv.rematch.order;
    LOG_RI(rip);

    XP_Bool success = XP_FALSE;

    /* We have an incoming player with an address. We want to find the first
       open slot (a srvPlyrs entry where deviceIndex is -1) where the
       corresponding entry in the RematchInfo points to the same address.
    */

    for ( int ii = 0; !success && ii < gi->nPlayers; ++ii ) {
        CtrlrPlayer* sp = &ctrlr->srvPlyrs[ii];
        SRVR_LOGFFV( "ii: %d; deviceIndex: %d", ii, sp->deviceIndex );
        if ( UNKNOWN_DEVICE == sp->deviceIndex ) {
            int addrIndx = rip->addrIndices[ii];
            if ( addrsAreSame( ctrlr->vol.dutil, xwe, &guestAddr,
                               &rip->addrs[addrIndx] ) ) {
                *spp = sp;
                *lpTurn = ii;
                XP_ASSERT( !gi->players[ii].isLocal );
                success = XP_TRUE;
            }
        }
    }

    SRVR_LOGFFV( "()=>%s", boolToStr(success) );
    return success;
}

static XP_S8
registerRemotePlayer( CtrlrCtxt* ctrlr, XWEnv xwe, XWStreamCtxt* stream )
{
    XP_S8 deviceIndex = -1;

    /* The player must already be there with a null name, or it's an error.
       Take the first empty slot. */
    XP_ASSERT( ctrlr->nv.pendingRegistrations > 0 );

    /* find the slot to use */
    XP_S16 turn = -1;
    CtrlrPlayer* sp;
    XP_Bool success;
    if ( ctrl_isFromRematch( ctrlr ) ) {
        success = findOrderedSlot( ctrlr, xwe, stream, &sp, &turn );
    } else {
        success = findFirstPending( ctrlr, &sp, &turn );
    }

    if ( success ) {
        CurGameInfo gi = {};
        gi_copy( &gi, ctrlr->vol.gi );
        XP_ASSERT( ROLE_ISHOST == gi.deviceRole );
        LocalPlayer* lp = &gi.players[turn];
        /* get data from stream */
        lp->robotIQ = 1 == strm_getBits( stream, 1 )? 1 : 0;
        XP_U16 nameLen = strm_getBits( stream, NAME_LEN_NBITS );
        XP_UCHAR name[nameLen + 1];
        strm_getBytes( stream, name, nameLen );
        name[nameLen] = '\0';
        SRVR_LOGFF( "read remote name: %s", name );

        str2ChrArray( lp->name, name );

        gr_setGI( ctrlr->vol.dutil, ctrlr->vol.gr, xwe, &gi );

        XP_PlayerAddr channelNo = strm_getAddress( stream );
        deviceIndex = getIndexForDevice( ctrlr, channelNo );

        --ctrlr->nv.pendingRegistrations;

        if ( deviceIndex == -1 ) {
            RemoteAddress* addr = &ctrlr->nv.addresses[ctrlr->nv.nDevices];

            XP_ASSERT( channelNo != 0 );
            addr->channelNo = channelNo;
            SRVR_LOGFF( "set channelNo to %x for device %d",
                        channelNo, ctrlr->nv.nDevices );

            deviceIndex = ctrlr->nv.nDevices++;
#ifdef STREAM_VERS_BIGBOARD
            addr->streamVersion = STREAM_SAVE_PREVWORDS;
#endif
        } else {
            SRVR_LOGFF( "deviceIndex already set" );
        }

        sp->deviceIndex = deviceIndex;

        informMissing( ctrlr, xwe );
    }
    return deviceIndex;
} /* registerRemotePlayer */

static void
sortTilesIf( CtrlrCtxt* ctrlr, XP_S16 turn )
{
    if ( ctrlr->nv.sortNewTiles ) {
        model_sortTiles( ctrlr->vol.model, turn );
    }
}

/* Called in response to message from ctrlr listing all the names of
 * players in the game (in ctrlr-assigned order) and their initial
 * tray contents.
 */
static XP_Bool
client_readInitialMessage( CtrlrCtxt* ctrlr, XWEnv xwe, XWStreamCtxt* stream )
{
    LOG_FUNC();
    XP_Bool accepted = 0 == ctrlr->nv.addresses[0].channelNo;
    XP_ASSERT( accepted );

    /* We should never get this message a second time, but very rarely we do.
       Drop it in that case. */
    if ( accepted ) {
        ModelCtxt* model = ctrlr->vol.model;
        CommsCtxt* comms = ctrlr->vol.comms;
        const CurGameInfo* gi = ctrlr->vol.gi;
        PoolContext* pool;
#ifdef STREAM_VERS_BIGBOARD
        XP_UCHAR rmtDictName[128];
        XP_UCHAR rmtDictSum[64];
#endif

        /* version; any dependencies here? */
        XP_U8 streamVersion = strm_getU8( stream );
        SRVR_LOGFF( "set streamVersion to 0x%X", streamVersion );
        strm_setVersion( stream, streamVersion );
        if ( STREAM_VERS_NINETILES > streamVersion ) {
            model_forceStack7Tiles( ctrlr->vol.model );
        }
        // XP_ASSERT( streamVersion <= CUR_STREAM_VERS ); /* else do what? */

        XP_U32 gameID = streamVersion < STREAM_VERS_REMATCHORDER
            ? strm_getU32( stream ) : 0;
        CurGameInfo myNewGI = gi_readFromStream2( stream );
        XP_ASSERT( gameID == 0 || gameID == myNewGI.gameID );
        gameID = myNewGI.gameID;
        myNewGI.deviceRole = ROLE_ISGUEST;

        /* never seems to replace anything -- gi is already correct on guests
           apparently. How? Will have come in with invitation, of course. */
        SRVR_LOGFFV( "read gameID of %08X; calling comms_setConnID (replacing %08X)",
                    gameID, gi->gameID );
        XP_ASSERT( gi->gameID == gameID );
        comms_setConnID( comms, gameID, streamVersion );

        str2ChrArray( myNewGI.dictName, gi->dictName );

        ctrlr->nv.flags |= FLAG_HARVEST_READY;

        if ( streamVersion < STREAM_VERS_NOEMPTYDICT ) {
            SRVR_LOGFF( "loading and dropping empty dict" );
            DictionaryCtxt* empty = dutil_makeEmptyDict( ctrlr->vol.dutil, xwe );
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

        XP_PlayerAddr channelNo = strm_getAddress( stream );
        XP_ASSERT( channelNo != 0 );
        ctrlr->nv.addresses[0].channelNo = channelNo;
        SRVR_LOGFF( "assigning channelNo %x for 0", channelNo );

        model_setSize( model, myNewGI.boardSize );

        XP_U16 nPlayers = myNewGI.nPlayers;
        SRVR_LOGFFV( "reading in %d players", myNewGI.nPlayers );

        model_setNPlayers( model, nPlayers );

        const DictionaryCtxt* curDict = model_getDictionary( model );

        if ( curDict == NULL ) {
            // XP_ASSERT(0);
        } else {
            /* keep the dict the local user installed */
#ifdef STREAM_VERS_BIGBOARD
            if ( '\0' != rmtDictName[0] ) {
                const XP_UCHAR* ourName = dict_getShortName( curDict );
                util_informNetDict( *ctrlr->vol.utilp, xwe,
                                    dict_getISOCode( curDict ),
                                    ourName, rmtDictName,
                                    rmtDictSum, myNewGI.phoniesAction );
            }
#endif
        }

        gr_setGI( ctrlr->vol.dutil, ctrlr->vol.gr, xwe, &myNewGI );

        XP_ASSERT( !ctrlr->pool );
        makePoolOnce( ctrlr );
        pool = ctrlr->pool;

        /* now read the assigned tiles for each player from the stream, and
           remove them from the newly-created local pool. */
        TrayTileSet tiles;
        for ( XP_U16 ii = 0; ii < nPlayers; ++ii ) {

            /* Pull/remove only once if duplicate-mode game */
            if ( ii == 0 || !inDuplicateMode(ctrlr) ) {
                traySetFromStream( stream, &tiles );
                XP_ASSERT( tiles.nTiles <= MAX_TRAY_TILES );
                /* remove what the ctrlr's assigned so we won't conflict
                   later. */
                pool_removeTiles( pool, &tiles );
            }
            SRVR_LOGFFV( "got %d tiles for player %d", tiles.nTiles, ii );

            if ( inDuplicateMode(ctrlr ) ) {
                model_assignDupeTiles( model, xwe, &tiles );
                break;
            } else {
                model_assignPlayerTiles( model, ii, &tiles );
            }

            sortTilesIf( ctrlr, ii );
        }

        readMQTTDevID( ctrlr, stream );
        readGuestAddrs( ctrlr, stream, strm_getVersion( stream ) );

        syncPlayers( ctrlr );

        SETSTATE( ctrlr, XWSTATE_INTURN );

        /* Give board a chance to redraw self with the full compliment of known
           players */
        informMissing( ctrlr, xwe );
        setTurn( ctrlr, xwe, 0 );
        resetDupeTimerIf( ctrlr, xwe );
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
makeSendableGICopy( CtrlrCtxt* ctrlr, CurGameInfo* giCopy, 
                    XP_U16 deviceIndex )
{
    XP_MEMCPY( giCopy, ctrlr->vol.gi, sizeof(*giCopy) );

    for ( int ii = 0; ii < giCopy->nPlayers; ++ii ) {
        LocalPlayer* lp = &giCopy->players[ii];

        /* adjust isLocal to client's perspective */
        lp->isLocal = ctrlr->srvPlyrs[ii].deviceIndex == deviceIndex;
    }

    giCopy->forceChannel = deviceIndex;
    SRVR_LOGFFV( "assigning forceChannel from deviceIndex: %d",
                giCopy->forceChannel );

    giCopy->dictName[0] = '\0';
    LOG_GI( giCopy, "after" );
} /* makeSendableGICopy */

static void
sendInitialMessage( CtrlrCtxt* ctrlr, XWEnv xwe )
{
    ModelCtxt* model = ctrlr->vol.model;
    XP_U16 nPlayers = ctrlr->vol.gi->nPlayers;
    XP_U32 gameID = ctrlr->vol.gi->gameID;
#ifdef STREAM_VERS_BIGBOARD
    XP_U8 streamVersion = ctrlr->nv.streamVersion;
    SRVR_LOGFF( "streamVersion: 0x%X", streamVersion );
#endif

    XP_ASSERT( ctrlr->nv.nDevices > 1 );
    for ( XP_U16 deviceIndex = 1; deviceIndex < ctrlr->nv.nDevices;
          ++deviceIndex ) {
        XWStreamCtxt* stream = messageStreamWithHeader( ctrlr, deviceIndex,
                                                        XWPROTO_CLIENT_SETUP );
        XP_ASSERT( !!stream );

#ifdef STREAM_VERS_BIGBOARD
        XP_ASSERT( 0 < streamVersion );
        strm_putU8( stream, streamVersion );
#else
        strm_putU8( stream, CUR_STREAM_VERS );
#endif

        if ( streamVersion < STREAM_VERS_REMATCHORDER ) {
            strm_putU32( stream, gameID );
        }

        CurGameInfo localGI;
        makeSendableGICopy( ctrlr, &localGI, deviceIndex );
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
            if ( inDuplicateMode(ctrlr) ) {
                break;
            }
        }

        addMQTTDevIDIf( ctrlr, xwe, stream );
        addGuestAddrsIf( ctrlr, xwe, deviceIndex, stream );

        closeAndSend( ctrlr, xwe, stream );
    }

    /* Set after messages are built so their connID will be 0, but all
       non-initial messages will have a non-0 connID. */
    comms_setConnID( ctrlr->vol.comms, gameID, streamVersion );

    resetDupeTimerIf( ctrlr, xwe );
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

    strm_putBits( stream, 4, nWords );
    if ( STREAM_VERS_DICTNAME <= strm_getVersion( stream ) ) {
        stringToStream( stream, bws->dictName );
    }
    for ( int ii = 0; ii < nWords; ++ii ) {
        stringToStream( stream, bws->bwi.words[ii] );
    }

} /* bwsToStream */

static void
bwsFromStream( MPFORMAL XWStreamCtxt* stream, BadWordsState* bws )
{
    XP_U16 nWords = strm_getBits( stream, 4 );
    XP_ASSERT( nWords < VSIZE(bws->bwi.words) - 1 );

    bws->bwi.nWords = nWords;
    if ( STREAM_VERS_DICTNAME <= strm_getVersion( stream ) ) {
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
        caseStr( XWPROTO_MOVEMADE_INFO_GUEST );
        caseStr( XWPROTO_MOVEMADE_INFO_HOST );
        caseStr( XWPROTO_UNDO_INFO_CLIENT );
        caseStr( XWPROTO_UNDO_INFO_CTRLR );
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
messageStreamWithHeader( const CtrlrCtxt* ctrlr, XP_U16 devIndex, XW_Proto code )
{
    XP_PlayerAddr channelNo = ctrlr->nv.addresses[devIndex].channelNo;

    XWStreamCtxt* stream = strm_make_raw( MPPARM_NOCOMMA(ctrlr->mpool) );
    strm_setAddress( stream, channelNo );
    PRINTCODE( "making", code );

    writeProto( ctrlr, stream, code );

    return stream;
} /* messageStreamWithHeader */

static void
closeAndSend( const CtrlrCtxt* ctrlr, XWEnv xwe, XWStreamCtxt* stream )
{
    comms_send( ctrlr->vol.comms, xwe, stream );
    strm_destroy( stream );
}

static void
sendBadWordMsgs( CtrlrCtxt* ctrlr, XWEnv xwe )
{
    XP_ASSERT( ctrlr->bws.bwi.nWords > 0 );

    if ( ctrlr->bws.bwi.nWords > 0 ) { /* fail gracefully */
        XWStreamCtxt* stream = 
            messageStreamWithHeader( ctrlr, ctrlr->lastMoveSource,
                                     XWPROTO_BADWORD_INFO );
        strm_putBits( stream, PLAYERNUM_NBITS, ctrlr->nv.currentTurn );

        bwsToStream( stream, &ctrlr->bws );

        /* XP_U32 hash = model_getHash( ctrlr->vol.model ); */
        /* strm_putU32( stream, hash ); */
        /* XP_LOGFF( "wrote hash: %X", hash ); */

        closeAndSend( ctrlr, xwe, stream );

        freeBWS( MPPARM(ctrlr->mpool) &ctrlr->bws );
    }
    SETSTATE( ctrlr, XWSTATE_INTURN );
} /* sendBadWordMsgs */

static void
badWordMoveUndoAndTellUser( CtrlrCtxt* ctrlr, XWEnv xwe,
                            const BadWordsState* bws )
{
    XP_U16 turn;
    ModelCtxt* model = ctrlr->vol.model;
    /* I'm the host.  I need to send a message to everybody else telling
       them the move's rejected.  Then undo it on this side, replacing it with
       model_commitRejectedPhony(); */

    model_rejectPreviousMove( model, xwe, ctrlr->pool, &turn );

    util_notifyIllegalWords( *ctrlr->vol.utilp, xwe, &bws->bwi,
                             bws->dictName, turn, XP_TRUE, 0 );
} /* badWordMoveUndoAndTellUser */

EngineCtxt*
ctrl_getEngineFor( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 playerNum )
{
    const CurGameInfo* gi = ctrlr->vol.gi;
    XP_ASSERT( playerNum < gi->nPlayers );

    CtrlrPlayer* player = &ctrlr->srvPlyrs[playerNum];
    EngineCtxt* engine = player->engine;
    if ( !engine &&
         (inDuplicateMode(ctrlr) || gi->players[playerNum].isLocal) ) {
        engine = engine_make( xwe, ctrlr->vol.utilp );
        player->engine = engine;
    }

    return engine;
} /* ctrl_getEngineFor */

void
ctrl_resetEngine( CtrlrCtxt* ctrlr, XP_U16 playerNum )
{
    CtrlrPlayer* player = &ctrlr->srvPlyrs[playerNum];
    if ( !!player->engine ) {
        XP_ASSERT( player->deviceIndex == 0 || inDuplicateMode(ctrlr) );
        engine_reset( player->engine );
    }
} /* ctrl_resetEngine */

void
ctrl_resetEngines( CtrlrCtxt* ctrlr )
{
    XP_U16 nPlayers = ctrlr->vol.gi->nPlayers;
    for ( XP_U16 ii = 0; ii < nPlayers; ++ii ) {
        ctrl_resetEngine( ctrlr, ii );
    }
} /* resetEngines */

#ifdef TEST_ROBOT_TRADE
static void
makeNotAVowel( CtrlrCtxt* ctrlr, Tile* newTile )
{
    char face[4];
    Tile tile = *newTile;
    PoolContext* pool = ctrlr->pool;
    TrayTileSet set;
    const DictionaryCtxt* dict = model_getDictionary( ctrlr->vol.model );
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
 * tiles as are needed that's cool: ctrlr code will later interpret that as
 * meaning the remainder should be assigned randomly as usual.
 */
XP_Bool
ctrl_askPickTiles( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 turn,
                     TrayTileSet* newTiles, XP_U16 nToPick )
{
    /* May want to allow the host to pick tiles even in duplicate mode. Not
       sure how that'll work! PENDING */
    XP_Bool asked = newTiles == NULL && !inDuplicateMode(ctrlr)
        && ctrlr->vol.gi->allowPickTiles;
    if ( asked ) {
        asked = informNeedPickTiles( ctrlr, xwe, XP_FALSE, turn, nToPick );
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
trayAllowsMoves( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 turn,
                 const Tile* tiles, XP_U16 nTiles )
{
    ModelCtxt* model = ctrlr->vol.model;
    XP_U16 nInTray = model_getNumTilesInTray( model, turn );
    SRVR_LOGFF( "(nTiles=%d): nInTray: %d", nTiles, nInTray );
    XP_ASSERT( nInTray + nTiles <= MAX_TRAY_TILES ); /* fired again! */
    TrayTileSet tts = *model_getPlayerTiles( model, turn );
    XP_ASSERT( nInTray == tts.nTiles );
    XP_MEMCPY( &tts.tiles[nInTray], &tiles[0], nTiles * sizeof(tts.tiles[0]) );
    tts.nTiles += nTiles;

    /* XP_LOGFF( "%s(nTiles=%d)", __func__, nTiles ); */
    EngineCtxt* tmpEngine = NULL;
    EngineCtxt* engine = ctrl_getEngineFor( ctrlr, xwe, turn );
    if ( !engine ) {
        tmpEngine = engine = engine_make( xwe, ctrlr->vol.utilp );
    }
    XP_Bool canMove;
    MoveInfo newMove = {};
    XP_U16 score = 0;
    XP_Bool result = engine_findMove( engine, xwe, ctrlr->vol.model, turn,
                                      XP_TRUE,
#ifdef XWFEATURE_STOP_ENGINE
                                      XP_TRUE,
#endif
                                      &tts, XP_FALSE, 0,
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
        ctrl_resetEngine( ctrlr, turn );
    }

    return result;
}

static void
fetchTiles( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 playerNum, XP_U16 nToFetch,
            TrayTileSet* resultTiles, XP_Bool forceCanPlay /* First player shouldn't have unplayable rack*/ )
{
    XP_ASSERT( ctrlr->vol.gi->deviceRole != ROLE_ISGUEST || !inDuplicateMode(ctrlr) );
    XP_U16 nSoFar = resultTiles->nTiles;
    PoolContext* pool = ctrlr->pool;

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

            if ( !inDuplicateMode( ctrlr ) && !forceCanPlay ) {
                break;
            } else if ( trayAllowsMoves( ctrlr, xwe, playerNum, &resultTiles->tiles[0],
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
makePoolOnce( CtrlrCtxt* ctrlr )
{
    ModelCtxt* model = ctrlr->vol.model;
    XP_ASSERT( model_getDictionary(model) != NULL );
    if ( ctrlr->pool == NULL ) {
        ctrlr->pool = pool_make( MPPARM_NOCOMMA(ctrlr->mpool) );
        XP_STATUSF( "%s(): initing pool", __func__ );
        pool_initFromDict( ctrlr->pool, model_getDictionary(model),
                           ctrlr->vol.gi->boardSize );
    }
}

static XP_Bool
assignTilesToAll( CtrlrCtxt* ctrlr, XWEnv xwe )
{
    LOG_FUNC();
    XP_Bool allDone = XP_TRUE;
    XP_U16 numAssigned;
    XP_U16 ii;
    ModelCtxt* model = ctrlr->vol.model;
    const CurGameInfo* gi = ctrlr->vol.gi;
    XP_U16 nPlayers = gi->nPlayers;

    XP_ASSERT( gi->deviceRole != ROLE_ISGUEST );
    makePoolOnce( ctrlr );

    XP_STATUSF( "assignTilesToAll" );

    model_setNPlayers( model, nPlayers );

    numAssigned = pool_getNTilesLeft( ctrlr->pool ) / nPlayers;
    if ( numAssigned > gi->traySize ) {
        numAssigned = gi->traySize;
    }

    /* Loop through all the players. If picking tiles is on, stop for each
       local player that doesn't have tiles yet. Return TRUE if we get all the
       way through without doing that. */

    XP_Bool pickingTiles = gi->deviceRole == ROLE_STANDALONE
        && gi->allowPickTiles;
    TrayTileSet newTiles;
    for ( ii = 0; ii < nPlayers; ++ii ) {
        if ( 0 == model_getNumTilesInTray( model, ii ) ) {
            if ( pickingTiles && !LP_IS_ROBOT(&gi->players[ii])
                 && informNeedPickTiles( ctrlr, xwe, XP_TRUE, ii,
                                         gi->traySize ) ) {
                allDone = XP_FALSE;
                break;
            }
            if ( 0 == ii || !gi->inDuplicateMode ) {
                newTiles.nTiles = 0;
                fetchTiles( ctrlr, xwe, ii, numAssigned, &newTiles, ii == 0 );
            }

            if ( gi->inDuplicateMode ) {
                XP_ASSERT( ii == DUP_PLAYER );
                model_assignDupeTiles( model, xwe, &newTiles );
                break;
            } else {
                model_assignPlayerTiles( model, ii, &newTiles );
            }
        }
        sortTilesIf( ctrlr, ii );
    }
    LOG_RETURNF( "%d", allDone );
    return allDone;
} /* assignTilesToAll */

static void
getPlayerTime( CtrlrCtxt* ctrlr, XWStreamCtxt* stream, XP_U16 turn )
{
    const CurGameInfo* gi = ctrlr->vol.gi;
    if ( gi->timerEnabled ) {
        XP_U32 secondsUsed;
        if ( STREAM_VERS_BIGGERGI <= strm_getVersion(stream) ) {
            secondsUsed = strm_getU32VL( stream );
        } else {
            secondsUsed = strm_getU16( stream );
        }
        model_setSecondsUsed( ctrlr->vol.model, turn, secondsUsed );
    }
} /* getPlayerTime */

typedef struct _PSIData {
    CtrlrCtxt* ctrlr;
    XP_S16 prevTurn;
} PSIData;

static void
showPMIdle( XW_DUtilCtxt* XP_UNUSED(dutil), XWEnv xwe, void* closure,
            TimerKey XP_UNUSED(key), XP_Bool fired )
{
    PSIData* psid = (PSIData*)closure;
    if ( fired ) {
        showPrevScore( psid->ctrlr, xwe, psid->prevTurn );
    }
    XP_FREE( psid->ctrlr->mpool, closure );
}

static void
setShowPrevScoreIdle( CtrlrCtxt* ctrlr, XWEnv xwe )
{
    PSIData* psid = XP_CALLOC( ctrlr->mpool, sizeof(*psid));
    psid->ctrlr = ctrlr;
    const XP_U16 nPlayers = ctrlr->vol.gi->nPlayers;
    psid->prevTurn = (ctrlr->nv.currentTurn + nPlayers - 1) % nPlayers;
    tmr_setIdle( ctrlr->vol.dutil, xwe, showPMIdle, psid );
}

static void
nextTurn( CtrlrCtxt* ctrlr, XWEnv xwe, XP_S16 nxtTurn )
{
    SRVR_LOGFFV( "(nxtTurn=%d)", nxtTurn );
    const CurGameInfo* gi = ctrlr->vol.gi;
    XP_S16 currentTurn = ctrlr->nv.currentTurn;
    XP_Bool moreToDo = XP_FALSE;

    if ( nxtTurn == PICK_CUR ) {
        nxtTurn = model_getNextTurn( ctrlr->vol.model );
    } else if ( nxtTurn == PICK_NEXT ) {
        // XP_ASSERT( ctrlr->nv.gameState == XWSTATE_INTURN );
        if ( ctrlr->nv.gameState != XWSTATE_INTURN ) {
            SRVR_LOGFF( "doing nothing; state %s != XWSTATE_INTURN",
                        getStateStr(ctrlr->nv.gameState) );
            XP_ASSERT( !moreToDo );
            goto exit;
        } else if ( currentTurn >= 0 ) {
            if ( inDuplicateMode(ctrlr) ) {
                nxtTurn = dupe_nextTurn( ctrlr );
            } else {
                nxtTurn = model_getNextTurn( ctrlr->vol.model );
            }
        } else {
            SRVR_LOGFF( "turn == -1 so dropping" );
        }
    } else {
        /* We're doing an undo, and so won't bother figuring out who the
           previous turn was or how many tiles he had: it's a sure thing he
           "has" enough to be allowed to take the turn just undone. */
        XP_ASSERT( nxtTurn == model_getNextTurn( ctrlr->vol.model ) );
    }
    XP_Bool playerTilesLeft = tileCountsOk( ctrlr );
    SETSTATE( ctrlr, XWSTATE_INTURN ); /* even if game over, if undoing */

    if ( playerTilesLeft && NPASSES_OK(ctrlr) ){
        setTurn( ctrlr, xwe, nxtTurn );
    } else {
        /* I discover that the game should end.  If I'm the client,
           though, should I wait for the ctrlr to deduce this and send
           out a message?  I think so.  Yes, but be sure not to compute
           another PASS move.  Just don't do anything! */
        if ( gi->deviceRole != ROLE_ISGUEST ) {
            SETSTATE( ctrlr, XWSTATE_NEEDSEND_ENDGAME ); /* this is it */
            moreToDo = XP_TRUE;
        } else if ( currentTurn >= 0 ) {
            SRVR_LOGFF( "Doing nothing; waiting for ctrlr to end game" );
            setTurn( ctrlr, xwe, -1 );
            /* I'm the client. Do ++nothing++. */
        }
    }

    if ( ctrlr->vol.showPrevMove ) {
        ctrlr->vol.showPrevMove = XP_FALSE;
        if ( inDuplicateMode(ctrlr) || ctrlr->nv.showRobotScores ) {
            setShowPrevScoreIdle( ctrlr, xwe );
        }
    }

    /* It's safer, if perhaps not always necessary, to do this here. */
    ctrl_resetEngines( ctrlr );

    XP_ASSERT( ctrlr->nv.gameState != XWSTATE_GAMEOVER );
    callTurnChangeListener( ctrlr, xwe );
    util_turnChanged( *ctrlr->vol.utilp, xwe, ctrlr->nv.currentTurn );

    if ( robotMovePending(ctrlr) && !POSTPONEROBOTMOVE(ctrlr, xwe) ) {
        moreToDo = XP_TRUE;
    }

 exit:
    if ( moreToDo ) {
        ctrl_addIdle( ctrlr, xwe );
    }
} /* nextTurn */

void
ctrl_setTurnChangeListener( CtrlrCtxt* ctrlr, TurnChangeListener tl,
                              void* data )
{
    XP_ASSERT( !ctrlr->vol.turnChangeListener
               || ctrlr->vol.turnChangeListener == tl );
    ctrlr->vol.turnChangeListener = tl;
    ctrlr->vol.turnChangeData = data;
} /* ctrl_setTurnChangeListener */

void
ctrl_setTimerChangeListener( CtrlrCtxt* ctrlr, TimerChangeListener tl,
                               void* data )
{
    XP_ASSERT( !ctrlr->vol.timerChangeListener );
    ctrlr->vol.timerChangeListener = tl;
    ctrlr->vol.timerChangeData = data;
}

void
ctrl_setGameOverListener( CtrlrCtxt* ctrlr, GameOverListener gol,
                            void* data )
{
    ctrlr->vol.gameOverListener = gol;
    ctrlr->vol.gameOverData = data;
} /* ctrl_setGameOverListener */

static void
storeBadWords( const WNParams* wnp, void* closure )
{
    if ( !wnp->isLegal ) {
        CtrlrCtxt* ctrlr = (CtrlrCtxt*)closure;
        const XP_UCHAR* dictName = dict_getShortName( wnp->dict );

        XP_LOGFF( "storeBadWords called with \"%s\" (name=%s)", wnp->word,
                  dictName );
        if ( NULL == ctrlr->bws.dictName ) {
            ctrlr->bws.dictName = copyString( ctrlr->mpool, dictName );
        }
        BadWordInfo* bwi = &ctrlr->bws.bwi;
        bwi->words[bwi->nWords++] = copyString( ctrlr->mpool, wnp->word );
    }
} /* storeBadWords */

static XP_Bool
checkMoveAllowed( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 playerNum )
{
    const CurGameInfo* gi = ctrlr->vol.gi;
    XP_ASSERT( ctrlr->bws.bwi.nWords == 0 );

    if ( gi->phoniesAction == PHONIES_DISALLOW ) {
        WordNotifierInfo info;
        info.proc = storeBadWords;
        info.closure = ctrlr;
        (void)model_checkMoveLegal( ctrlr->vol.model, xwe, playerNum,
                                    (XWStreamCtxt*)NULL, &info );
    }

    return ctrlr->bws.bwi.nWords == 0;
} /* checkMoveAllowed */

static void
sendMoveTo( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 devIndex, XP_U16 turn,
            XP_Bool legal, TrayTileSet* newTiles, 
            const TrayTileSet* tradedTiles ) /* null if a move, set if a trade */
{
    XP_Bool isTrade = !!tradedTiles;
    const CurGameInfo* gi = ctrlr->vol.gi;
    XW_Proto code = gi->deviceRole == ROLE_ISGUEST?
        XWPROTO_MOVEMADE_INFO_GUEST : XWPROTO_MOVEMADE_INFO_HOST;
    ModelCtxt* model = ctrlr->vol.model;

    XWStreamCtxt* stream = messageStreamWithHeader( ctrlr, devIndex, code );

#ifdef STREAM_VERS_BIGBOARD
    XP_U16 version = strm_getVersion( stream );
    if ( STREAM_VERS_BIGBOARD <= version ) {
        XP_ASSERT( version == ctrlr->nv.streamVersion );
        XP_U32 hash = model_getHash( model );
#ifdef DEBUG_HASHING
        SRVR_LOGFF( "adding hash %X", (unsigned int)hash );
#endif
        strm_putU32( stream, hash );
    }
#endif

    strm_putBits( stream, PLAYERNUM_NBITS, turn ); /* who made the move */

    traySetToStream( stream, newTiles );

    strm_putBits( stream, 1, isTrade );

    if ( isTrade ) {
        traySetToStream( stream, tradedTiles );
    } else {
        strm_putBits( stream, 1, legal );

        model_currentMoveToStream( model, turn, stream );

        XP_U32 secondsUsed = model_getSecondsUsed( model, turn );
        if ( gi->timerEnabled ) {
            if ( STREAM_VERS_BIGGERGI <= version ) {
                strm_putU32VL( stream, secondsUsed );
            } else {
                strm_putU16( stream, secondsUsed );
            }
            SRVR_LOGFFV( "wrote secondsUsed for player %d: %d",
                         turn, secondsUsed );
        } else {
            XP_ASSERT( secondsUsed == 0 );
        }

        if ( !legal ) {
            XP_ASSERT( ctrlr->bws.bwi.nWords > 0 );
            strm_putBits( stream, PLAYERNUM_NBITS, turn );
            bwsToStream( stream, &ctrlr->bws );
        }
    }

    closeAndSend( ctrlr, xwe, stream );
} /* sendMoveTo */

static XP_Bool
readMoveInfo( CtrlrCtxt* ctrlr, XWEnv xwe, XWStreamCtxt* stream,
              XP_U16* whoMovedP, XP_Bool* isTradeP,
              TrayTileSet* newTiles, TrayTileSet* tradedTiles, 
              XP_Bool* legalP, XP_Bool* badStackP )
{
    LOG_FUNC();
    XP_Bool success = XP_TRUE;
    XP_Bool legalMove = XP_TRUE;
    XP_Bool isTrade;

#ifdef STREAM_VERS_BIGBOARD
    if ( STREAM_VERS_BIGBOARD <= strm_getVersion( stream ) ) {
        XP_U32 hashReceived = strm_getU32( stream );
        success = model_hashMatches( ctrlr->vol.model, hashReceived )
            || model_popToHash( ctrlr->vol.model, xwe, hashReceived, ctrlr->pool );

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
        XP_U16 whoMoved = strm_getBits( stream, PLAYERNUM_NBITS );
        traySetFromStream( stream, newTiles );
        success = pool_containsTiles( ctrlr->pool, newTiles );
        XP_ASSERT( success );
        if ( success ) {
            isTrade = strm_getBits( stream, 1 );

            if ( isTrade ) {
                traySetFromStream( stream, tradedTiles );
                SRVR_LOGFFV( "got trade of %d tiles", tradedTiles->nTiles );
            } else {
                legalMove = strm_getBits( stream, 1 );
                success = model_makeTurnFromStream( ctrlr->vol.model, 
                                                    xwe, whoMoved, stream );
                getPlayerTime( ctrlr, stream, whoMoved );
            }

            if ( success ) {
                pool_removeTiles( ctrlr->pool, newTiles );

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
sendMoveToClientsExcept( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 whoMoved, XP_Bool legal,
                         TrayTileSet* newTiles, const TrayTileSet* tradedTiles,
                         XP_U16 skip )
{
    for ( XP_U16 devIndex = 1; devIndex < ctrlr->nv.nDevices; ++devIndex ) {
        if ( devIndex != skip ) {
            sendMoveTo( ctrlr, xwe, devIndex, whoMoved, legal,
                        newTiles, tradedTiles );
        }
    }
} /* sendMoveToClientsExcept */

static XWStreamCtxt*
makeTradeReportIf( CtrlrCtxt* ctrlr, XWEnv xwe, const TrayTileSet* tradedTiles )
{
    XWStreamCtxt* stream = NULL;
    if ( inDuplicateMode(ctrlr) || ctrlr->nv.showRobotScores ) {
        XP_UCHAR tradeBuf[64];
        const XP_UCHAR* tradeStr = 
            dutil_getUserQuantityString( ctrlr->vol.dutil, xwe, STRD_ROBOT_TRADED,
                                         tradedTiles->nTiles );
        XP_SNPRINTF( tradeBuf, sizeof(tradeBuf), tradeStr, 
                     tradedTiles->nTiles );
        stream = mkCtrlrStream0( ctrlr );
        strm_catString( stream, tradeBuf );
    }
    return stream;
} /* makeTradeReportIf */

static XWStreamCtxt*
makeMoveReportIf( CtrlrCtxt* ctrlr, XWEnv xwe, XWStreamCtxt** wordsStream )
{
    XWStreamCtxt* stream = NULL;
    if ( inDuplicateMode(ctrlr) || ctrlr->nv.showRobotScores ) {
        ModelCtxt* model = ctrlr->vol.model;
        stream = mkCtrlrStream0( ctrlr );
        *wordsStream = mkCtrlrStream0( ctrlr );
        WordNotifierInfo* ni = model_initWordCounter( model, *wordsStream );
        (void)model_checkMoveLegal( model, xwe, ctrlr->nv.currentTurn, stream, ni );
    }
    return stream;
} /* makeMoveReportIf */

/* Client is reporting a move made, complete with new tiles and time taken by
 * the player.  Update the model with that information as a tentative move,
 * then sent info about it to all the clients, and finally commit the move
 * here.
 */
static XP_Bool
reflectMoveAndInform( CtrlrCtxt* ctrlr, XWEnv xwe, XWStreamCtxt* stream )
{
    XP_Bool success;
    ModelCtxt* model = ctrlr->vol.model;
    XP_U16 whoMoved;
    XP_U16 nTilesMoved = 0; /* trade case */
    XP_Bool isTrade;
    XP_Bool isLegalMove;
    XP_Bool doRequest = XP_FALSE;
    TrayTileSet newTiles;
    TrayTileSet tradedTiles;
    const CurGameInfo* gi = ctrlr->vol.gi;
    XP_U16 sourceClientIndex = getIndexForStream( ctrlr, stream );
    XWStreamCtxt* mvStream = NULL;
    XWStreamCtxt* wordsStream = NULL;

    XP_ASSERT( gi->deviceRole == ROLE_ISHOST );

    XP_Bool badStack = XP_FALSE;
    success = readMoveInfo( ctrlr, xwe, stream, &whoMoved, &isTrade, &newTiles,
                            &tradedTiles, &isLegalMove, &badStack ); /* modifies model */
    XP_ASSERT( !success || isLegalMove ); /* client should always report as true */
    isLegalMove = XP_TRUE;

    if ( success ) {
        if ( isTrade ) {
            sendMoveToClientsExcept( ctrlr, xwe, whoMoved, XP_TRUE, &newTiles,
                                     &tradedTiles, sourceClientIndex );

            model_makeTileTrade( model, whoMoved,
                                 &tradedTiles, &newTiles );
            pool_replaceTiles( ctrlr->pool, &tradedTiles );

            ctrlr->vol.showPrevMove = XP_TRUE;
            mvStream = makeTradeReportIf( ctrlr, xwe, &tradedTiles );

        } else {
            nTilesMoved = model_getCurrentMoveCount( model, whoMoved );
            isLegalMove = (nTilesMoved == 0)
                || checkMoveAllowed( ctrlr, xwe, whoMoved );

            /* I don't think this will work if there are more than two devices in
               a palm game; need to change state and get out of here before
               returning to send additional messages.  PENDING(ehouse) */
            sendMoveToClientsExcept( ctrlr, xwe, whoMoved, isLegalMove, &newTiles,
                                     (TrayTileSet*)NULL, sourceClientIndex );

            ctrlr->vol.showPrevMove = XP_TRUE;
            if ( isLegalMove ) {
                mvStream = makeMoveReportIf( ctrlr, xwe, &wordsStream );
            }

            success = model_commitTurn( model, xwe, whoMoved, &newTiles );
            ctrl_resetEngines( ctrlr );
        }

        if ( success && isLegalMove ) {
            XP_U16 nTilesLeft = model_getNumTilesTotal( model, whoMoved );

            if ( (gi->phoniesAction == PHONIES_DISALLOW) && (nTilesMoved > 0) ) {
                ctrlr->lastMoveSource = sourceClientIndex;
                SETSTATE( ctrlr, XWSTATE_MOVE_CONFIRM_MUSTSEND );
                doRequest = XP_TRUE;
            } else if ( nTilesLeft > 0 ) {
                nextTurn( ctrlr, xwe, PICK_NEXT );
            } else {
                SETSTATE(ctrlr, XWSTATE_NEEDSEND_ENDGAME );
                doRequest = XP_TRUE;
            }

            if ( !!mvStream ) {
                XP_ASSERT( !ctrlr->nv.prevMoveStream );
                ctrlr->nv.prevMoveStream = mvStream;
                XP_ASSERT( !ctrlr->nv.prevWordsStream );
                ctrlr->nv.prevWordsStream = wordsStream;
            }
        } else {
            /* The client from which the move came still needs to be told.  But we
               can't send a message now since we're burried in a message handler.
               (Palm, at least, won't manage.)  So set up state to tell that
               client again in a minute. */
            SETSTATE( ctrlr, XWSTATE_NEEDSEND_BADWORD_INFO );
            ctrlr->lastMoveSource = sourceClientIndex;
            doRequest = XP_TRUE;
        }

        if ( doRequest ) {
            ctrl_addIdle( ctrlr, xwe );
        }
    } else if ( badStack ) {
        success = XP_TRUE;      /* so we don't reject the move forever */
    }
    return success;
} /* reflectMoveAndInform */

static XP_Bool
reflectMove( CtrlrCtxt* ctrlr, XWEnv xwe, XWStreamCtxt* stream )
{
    XP_Bool isTrade;
    XP_Bool isLegal;
    XP_U16 whoMoved;
    TrayTileSet newTiles;
    TrayTileSet tradedTiles;
    ModelCtxt* model = ctrlr->vol.model;
    XWStreamCtxt* mvStream = NULL;
    XWStreamCtxt* wordsStream = NULL;

    XP_Bool badStack = XP_FALSE;
    XP_Bool moveOk = XP_FALSE;

    if ( XWSTATE_INTURN != ctrlr->nv.gameState ) {
        SRVR_LOGFF( "BAD: game state: %s, not XWSTATE_INTURN", getStateStr(ctrlr->nv.gameState ) );
    } else if ( ctrlr->nv.currentTurn < 0 ) {
        SRVR_LOGFF( "BAD: currentTurn %d < 0", ctrlr->nv.currentTurn );
    } else if ( ! readMoveInfo( ctrlr, xwe, stream, &whoMoved, &isTrade,
                                &newTiles, &tradedTiles, &isLegal, &badStack ) ) { /* modifies model */
        SRVR_LOGFF( "BAD: readMoveInfo() failed" );
    } else {
        moveOk = XP_TRUE;
    }

    if ( moveOk ) {
        if ( isTrade ) {
            model_makeTileTrade( model, whoMoved, &tradedTiles, &newTiles );
            pool_replaceTiles( ctrlr->pool, &tradedTiles );

            ctrlr->vol.showPrevMove = XP_TRUE;
            mvStream = makeTradeReportIf( ctrlr, xwe, &tradedTiles );
        } else {
            ctrlr->vol.showPrevMove = XP_TRUE;
            mvStream = makeMoveReportIf( ctrlr, xwe, &wordsStream );
            model_commitTurn( model, xwe, whoMoved, &newTiles );
        }

        if ( !!mvStream ) {
            setOrReplace( &ctrlr->nv.prevMoveStream, mvStream );
            setOrReplace( &ctrlr->nv.prevWordsStream, wordsStream );
        }

        ctrl_resetEngines( ctrlr );

        if ( !isLegal ) {
            XP_ASSERT( ctrlr->vol.gi->deviceRole == ROLE_ISGUEST );
            handleIllegalWord( ctrlr, xwe, stream );
        }
    } else if ( badStack ) {
        moveOk = XP_TRUE;
    }
    return moveOk;
} /* reflectMove */

static void
dupe_chooseMove( const CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 nPlayers,
                 XP_U16 scores[], XP_U16* winner, XP_U16* winningNTiles )
{
    ModelCtxt* model = ctrlr->vol.model;
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
dupe_allForced( const CtrlrCtxt* ctrlr )
{
    XP_Bool result = XP_TRUE;
    for ( XP_U16 ii = 0; result && ii < ctrlr->vol.gi->nPlayers; ++ii ) {
        result = ctrlr->nv.dupTurnsForced[ii];
    }
    LOG_RETURNF( "%d", result );
    return result;
}

/* Called for host or standalone case when all moves for the turn are
   present. Pick the best one and commit locally. In ctrlr case, transmit to
   each guest device as well. */
static void
dupe_commitAndReport( CtrlrCtxt* ctrlr, XWEnv xwe )
{
    const XP_U16 nPlayers = ctrlr->vol.gi->nPlayers;
    XP_U16 scores[nPlayers];

    XP_U16 winner;
    XP_U16 nTiles;
    dupe_chooseMove( ctrlr, xwe, nPlayers, scores, &winner, &nTiles );

    /* If nobody can move AND there are tiles left, trade instead of recording
       a 0. Unless we're running a timer, in which case it's most likely
       noboby's playing, so pause the game instead. */
    if ( 0 == scores[winner] && 0 < pool_getNTilesLeft(ctrlr->pool) ) {
        if ( dupe_timerRunning() && dupe_allForced( ctrlr ) ) {
            dupe_autoPause( ctrlr, xwe );
        } else {
            dupe_makeAndReportTrade( ctrlr, xwe );
        }
    } else {
        dupe_commitAndReportMove( ctrlr, xwe, winner, nPlayers, scores, nTiles );
    }
    dupe_clearState( ctrlr );
} /* dupe_commitAndReport */

static void
sendStreamToDev( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 dev, XW_Proto code,
                 XWStreamCtxt* data )
{
    XWStreamCtxt* stream = messageStreamWithHeader( ctrlr, dev, code );
    const XP_U16 dataLen = strm_getSize( data );
    const XP_U8* dataPtr = strm_getPtr( data );
    strm_putBytes( stream, dataPtr, dataLen );
    closeAndSend( ctrlr, xwe, stream );
}

/* Called in the case where nobody was able to move, does a trade. The goal is
   to make it more likely that folks will be able to move with the next set of
   tiles. For now I'll put them back first so there's a chance of getting some
   of the same back.
*/
static void
dupe_makeAndReportTrade( CtrlrCtxt* ctrlr, XWEnv xwe )
{
    LOG_FUNC();
    PoolContext* pool = ctrlr->pool;
    ModelCtxt* model = ctrlr->vol.model;

    model_resetCurrentTurn( model, xwe, DUP_PLAYER );

    TrayTileSet oldTiles = *model_getPlayerTiles( model, DUP_PLAYER );
    model_removePlayerTiles( model, DUP_PLAYER, &oldTiles );
    pool_replaceTiles( pool, &oldTiles );

    TrayTileSet newTiles = {};
    fetchTiles( ctrlr, xwe, DUP_PLAYER, oldTiles.nTiles, &newTiles, XP_FALSE );

    model_commitDupeTrade( model, &oldTiles, &newTiles );

    model_addNewTiles( model, DUP_PLAYER, &newTiles );
    updateOthersTiles( ctrlr, xwe );

    if ( ctrlr->vol.gi->deviceRole == ROLE_ISHOST ) {
        XWStreamCtxt* tmpStream =
            strm_make_raw( MPPARM_NOCOMMA(ctrlr->mpool) );

        addDupeStuffMark( tmpStream, DUPE_STUFF_TRADES_CTRLR );

        traySetToStream( tmpStream, &oldTiles );
        traySetToStream( tmpStream, &newTiles );

        /* Send it to each one */
        for ( XP_U16 dev = 1; dev < ctrlr->nv.nDevices; ++dev ) {
            sendStreamToDev( ctrlr, xwe, dev, XWPROTO_DUPE_STUFF, tmpStream );
        }

        strm_destroy( tmpStream );
    }

    dupe_resetTimer( ctrlr, xwe );

    dupe_setupShowTrade( ctrlr, xwe, newTiles.nTiles );
    LOG_RETURN_VOID();
} /* dupe_makeAndReportTrade */

static void
dupe_transmitPause( CtrlrCtxt* ctrlr, XWEnv xwe, DupPauseType typ, XP_U16 turn,
                    const XP_UCHAR* msg, XP_S16 skipDev )
{
    SRVR_LOGFF( "(type=%d, msg=%s)", typ, msg );
    const CurGameInfo* gi = ctrlr->vol.gi;
    if ( gi->deviceRole != ROLE_STANDALONE ) {
        XP_Bool amClient = ROLE_ISGUEST == gi->deviceRole;
        XWStreamCtxt* tmpStream =
            strm_make_raw( MPPARM_NOCOMMA(ctrlr->mpool) );

        addDupeStuffMark( tmpStream, DUPE_STUFF_PAUSE );

        strm_putBits( tmpStream, 1, amClient );
        strm_putBits( tmpStream, 2, typ );
        if ( AUTOPAUSED != typ ) {
            strm_putBits( tmpStream, PLAYERNUM_NBITS, turn );
        }
        strm_putU32( tmpStream, ctrlr->nv.dupTimerExpires );
        if ( AUTOPAUSED != typ ) {
            stringToStream( tmpStream, msg );
        }

        if ( amClient ) {
            sendStreamToDev( ctrlr, xwe, HOST_DEVICE, XWPROTO_DUPE_STUFF, tmpStream );
        } else {
            for ( XP_U16 dev = 1; dev < ctrlr->nv.nDevices; ++dev ) {
                if ( dev != skipDev ) {
                    sendStreamToDev( ctrlr, xwe, dev, XWPROTO_DUPE_STUFF, tmpStream );
                }
            }
        }
        strm_destroy( tmpStream );
    }
}

static XP_Bool
dupe_receivePause( CtrlrCtxt* ctrlr, XWEnv xwe, XWStreamCtxt* stream )
{
    LOG_FUNC();
    XP_Bool isClient = (XP_Bool)strm_getBits( stream, 1 );
    XP_Bool accept = isClient == amHost( ctrlr );
    if ( accept ) {
        const CurGameInfo* gi = ctrlr->vol.gi;
        DupPauseType pauseType = (DupPauseType)strm_getBits( stream, 2 );
        XP_S16 turn = -1;
        if ( AUTOPAUSED != pauseType ) {
            turn = (XP_S16)strm_getBits( stream, PLAYERNUM_NBITS );
            XP_ASSERT( 0 <= turn );
        } else {
            dupe_clearState( ctrlr );
        }

        setDupTimerExpires( ctrlr, xwe, (XP_S32)strm_getU32( stream ) );

        XP_UCHAR* msg = NULL;
        if ( AUTOPAUSED != pauseType ) {
            msg = stringFromStream( ctrlr->mpool, stream );
            SRVR_LOGFF( "pauseType: %d; guiltyParty: %d; msg: %s",
                        pauseType, turn, msg );
        }

        if ( amHost( ctrlr ) ) {
            XP_U16 senderDev = getIndexForStream( ctrlr, stream );
            dupe_transmitPause( ctrlr, xwe, pauseType, turn, msg, senderDev );
        }

        model_noteDupePause( ctrlr->vol.model, xwe, pauseType, turn, msg );
        callTurnChangeListener( ctrlr, xwe );

        const XP_UCHAR* name = NULL;
        if ( AUTOPAUSED != pauseType ) {
            name = gi->players[turn].name;
        }
        dutil_notifyPause( ctrlr->vol.dutil, xwe, ctrlr->vol.gr,
                           pauseType, turn, name, msg );

        XP_FREEP( ctrlr->mpool, &msg );
    }
    LOG_RETURNF( "%d", accept );
    return accept;
}

static XP_Bool
dupe_handleStuff( CtrlrCtxt* ctrlr, XWEnv xwe, XWStreamCtxt* stream )
{
    XP_Bool accepted;
    XP_Bool isHost = amHost( ctrlr );
    DUPE_STUFF typ = getDupeStuffMark( stream );
    switch ( typ ) {
    case DUPE_STUFF_MOVE_CLIENT:
        accepted = isHost && dupe_handleClientMoves( ctrlr, xwe, stream );
        break;
    case DUPE_STUFF_MOVES_CTRLR:
        accepted = !isHost && dupe_handleCtrlrMoves( ctrlr, xwe, stream );
        break;
    case DUPE_STUFF_TRADES_CTRLR:
        accepted = !isHost && dupe_handleCtrlrTrade( ctrlr, xwe, stream );
        break;
    case DUPE_STUFF_PAUSE:
        accepted = dupe_receivePause( ctrlr, xwe, stream );
        break;
    default:
        XP_ASSERT(0);
        accepted = XP_FALSE;
    }
    return accepted;
}

static void
dupe_commitAndReportMove( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 winner,
                          XP_U16 nPlayers, XP_U16* scores,
                          XP_U16 nTiles )
{
    ModelCtxt* model = ctrlr->vol.model;

    /* The winning move is the one we'll commit everywhere. Get it. Reset
       everybody else then commit it there. */
    MoveInfo moveInfo = {};
    model_currentMoveToMoveInfo( model, winner, &moveInfo );

    TrayTileSet newTiles = {};
    fetchTiles( ctrlr, xwe, winner, nTiles, &newTiles, XP_FALSE );

    for ( XP_U16 player = 0; player < nPlayers; ++player ) {
        model_resetCurrentTurn( model, xwe, player );
    }

    model_commitDupeTurn( model, xwe, &moveInfo, nPlayers,
                          scores, &newTiles );

    updateOthersTiles( ctrlr, xwe );

    if ( ctrlr->vol.gi->deviceRole == ROLE_ISHOST ) {
        XWStreamCtxt* tmpStream =
            strm_make_raw( MPPARM_NOCOMMA(ctrlr->mpool) );
        /* tilesNBits, in moveInfoToStream(), needs version */
        strm_setVersion( tmpStream, ctrlr->nv.streamVersion );

        addDupeStuffMark( tmpStream, DUPE_STUFF_MOVES_CTRLR );

        moveInfoToStream( tmpStream, &moveInfo, bitsPerTile(ctrlr) );
        traySetToStream( tmpStream, &newTiles );

        /* Now write all the scores */
        strm_putBits( tmpStream, NPLAYERS_NBITS, nPlayers );
        scoresToStream( tmpStream, nPlayers, scores );

        /* Send it to each one */
        for ( XP_U16 dev = 1; dev < ctrlr->nv.nDevices; ++dev ) {
            sendStreamToDev( ctrlr, xwe, dev, XWPROTO_DUPE_STUFF, tmpStream );
        }

        strm_destroy( tmpStream );
    }

    dupe_resetTimer( ctrlr, xwe );

    dupe_setupShowMove( ctrlr, xwe, scores );
} /* dupe_commitAndReportMove */

static void
dupe_forceCommits( CtrlrCtxt* ctrlr, XWEnv xwe )
{
    if ( dupe_timerRunning() ) {
        XP_U32 now = dutil_getCurSeconds( ctrlr->vol.dutil, xwe );
        if ( ctrlr->nv.dupTimerExpires <= now  ) {

            ModelCtxt* model = ctrlr->vol.model;
            for ( XP_U16 ii = 0; ii < ctrlr->vol.gi->nPlayers; ++ii ) {
                if ( ctrlr->vol.gi->players[ii].isLocal
                     && !ctrlr->nv.dupTurnsMade[ii] ) {
                    if ( !model_checkMoveLegal( model, xwe, ii, (XWStreamCtxt*)NULL,
                                                (WordNotifierInfo*)NULL ) ) {
                        model_resetCurrentTurn( model, xwe, ii );
                    }
                    commitMoveImpl( ctrlr, xwe, ii, NULL, XP_TRUE );
                }
            }
        }
    }
}

/* Figure out whether everything we care about is done for this turn. If I'm a
   guest, I care only about local players. If I'm a host or standalone, I care
   about everything.  */
static void
dupe_checkWhatsDone( const CtrlrCtxt* ctrlr, XP_Bool amHost,
                     XP_Bool* allDoneP, XP_Bool* allLocalsDoneP )
{
    XP_Bool allDone = XP_TRUE;
    XP_Bool allLocalsDone = XP_TRUE;
    for ( XP_U16 ii = 0;
          (allLocalsDone || allDone) && ii < ctrlr->vol.gi->nPlayers;
          ++ii ) {
        XP_Bool done = ctrlr->nv.dupTurnsMade[ii];
        XP_Bool isLocal = ctrlr->vol.gi->players[ii].isLocal;
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
ctrl_dupTurnDone( const CtrlrCtxt* ctrlr, XP_U16 turn )
{
    return ctrlr->vol.gi->players[turn].isLocal
        && ctrlr->nv.dupTurnsMade[turn];
}

static XP_Bool
dupe_checkTurns( CtrlrCtxt* ctrlr, XWEnv xwe )
{
    /* If all local players have made moves, it's time to commit the moves
       locally or notifiy the host */
    XP_Bool allDone = XP_TRUE;
    XP_Bool allLocalsDone = XP_TRUE;
    XP_Bool amHost = ctrlr->vol.gi->deviceRole == ROLE_ISHOST
        || ctrlr->vol.gi->deviceRole == ROLE_STANDALONE;
    dupe_checkWhatsDone( ctrlr, amHost, &allDone, &allLocalsDone );

    SRVR_LOGFF( "allDone: %d", allDone );

    if ( allDone ) {            /* Yep: commit time */
        if ( amHost ) {       /* I now have everything I need to move the
                                   game foreward */
            dupe_commitAndReport( ctrlr, xwe );
        } else if ( ! ctrlr->nv.dupTurnsSent ) { /* I need to send info for
                                                     local players to host */
            XWStreamCtxt* stream =
                messageStreamWithHeader( ctrlr, HOST_DEVICE,
                                         XWPROTO_DUPE_STUFF );

            addDupeStuffMark( stream, DUPE_STUFF_MOVE_CLIENT );

            /* XP_U32 hash = model_getHash( ctrlr->vol.model ); */
            /* strm_putU32( stream, hash ); */

            XP_U16 localCount = gi_countLocalPlayers( ctrlr->vol.gi, XP_FALSE );
            SRVR_LOGFF( "writing %d moves", localCount );
            strm_putBits( stream, NPLAYERS_NBITS, localCount );
            for ( XP_U16 ii = 0; ii < ctrlr->vol.gi->nPlayers; ++ii ) {
                if ( ctrlr->vol.gi->players[ii].isLocal ) {
                    strm_putBits( stream, PLAYERNUM_NBITS, ii );
                    strm_putBits( stream, 1, ctrlr->nv.dupTurnsForced[ii] );
                    model_currentMoveToStream( ctrlr->vol.model, ii, stream );
                    SRVR_LOGFF( "wrote move %d ", ii );
                }
            }

            closeAndSend( ctrlr, xwe, stream );
            ctrlr->nv.dupTurnsSent = XP_TRUE;
        }
    }
    return allDone;
} /* dupe_checkTurns */

static void
dupe_postStatus( const CtrlrCtxt* ctrlr, XWEnv xwe, XP_Bool allDone )
{
    /* Standalone case: say nothing here. Should be self evident what's
       up.*/
    /* If I'm a client and it's NOT a local turn, tell user that his
       turn's been sent off and he has to wait.
       *
       * If I'm a ctrlr, tell user how many of the expected moves have not
       * yet been received. If all have been, say nothing.
       */

    XP_UCHAR buf[256] = {};
    XP_Bool amHost = XP_FALSE;
    switch ( ctrlr->vol.gi->deviceRole ) {
    case ROLE_STANDALONE:
        /* do nothing */
        break;
    case ROLE_ISGUEST:
        if ( allDone ) {
            const XP_UCHAR* fmt = dutil_getUserString( ctrlr->vol.dutil, xwe,
                                                       STR_DUP_CLIENT_SENT );
            XP_SNPRINTF( buf, VSIZE(buf), "%s", fmt );
        }
        break;
    case ROLE_ISHOST:
        amHost = XP_TRUE;
        if ( !allDone ) {
            XP_U16 nHere = 0;
            for ( XP_U16 ii = 0; ii < ctrlr->vol.gi->nPlayers; ++ii ) {
                if ( ctrlr->nv.dupTurnsMade[ii] ) {
                    ++nHere;
                }
            }
            const XP_UCHAR* fmt = dutil_getUserString( ctrlr->vol.dutil, xwe,
                                                       STRDD_DUP_HOST_RECEIVED );
            XP_SNPRINTF( buf, VSIZE(buf), fmt, nHere, ctrlr->vol.gi->nPlayers );
        }
    }

    if ( !!buf[0] ) {
        SRVR_LOGFF( "msg=%s", buf );
        util_notifyDupStatus( *ctrlr->vol.utilp, xwe, amHost, buf );
    }
}

/* Called on client only? */
static void
dupe_storeTurn( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 turn, XP_Bool forced )
{
    XP_ASSERT( !ctrlr->nv.dupTurnsMade[turn] );
    XP_ASSERT( ctrlr->vol.gi->players[turn].isLocal ); /* not if I'm the host! */
    ctrlr->nv.dupTurnsMade[turn] = XP_TRUE;
    ctrlr->nv.dupTurnsForced[turn] = forced;

    XP_Bool allDone = dupe_checkTurns( ctrlr, xwe );
    dupe_postStatus( ctrlr, xwe, allDone );
    nextTurn( ctrlr, xwe, PICK_NEXT );

    SRVR_LOGFF( "player %d now has %d tiles", turn,
                model_getNumTilesInTray( ctrlr->vol.model, turn ) );
}

static void
dupe_clearState( CtrlrCtxt* ctrlr )
{
    for ( XP_U16 ii = 0; ii < ctrlr->vol.gi->nPlayers; ++ii ) {
        ctrlr->nv.dupTurnsMade[ii] = XP_FALSE;
        ctrlr->nv.dupTurnsForced[ii] = XP_FALSE;
    }
    ctrlr->nv.dupTurnsSent = XP_FALSE;
}

/* Make it the "turn" of the next local player who hasn't yet submitted a
   turn. If all have, make it a non-local player's turn. */
static XP_U16
dupe_nextTurn( const CtrlrCtxt* ctrlr )
{
    const CurGameInfo* gi = ctrlr->vol.gi;
    XP_S16 result = -1;
    XP_U16 nextNonLocal = DUP_PLAYER;
    for ( XP_U16 ii = 0; ii < gi->nPlayers; ++ii ) {
        if ( !ctrlr->nv.dupTurnsMade[ii] ) {
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
 * the move to the ctrlr (after which a response should be coming soon.)
 * If the ctrlr, then that step can be skipped: go straight to doing what
 * the ctrlr does after learning of a move on a remote device.
 *
 * Second cut.  Whether ctrlr or client, be responsible for checking the
 * basic legality of the move and taking new tiles out of the pool.  If
 * client, send the move and new tiles to the ctrlr.  If the ctrlr, fall
 * back to what will do after hearing from client: tell everybody who doesn't
 * already know what's happened: move and new tiles together.
 *
 * What about phonies when DISALLOW is set?  The ctrlr's ultimately
 * responsible, since it has the dictionary, so the client can't check.  So
 * when ctrlr, check and send move together with a flag indicating legality.
 * Client is responsible for first posting the move to the model and then
 * undoing it.  When client, send the move and go into a state waiting to hear
 * if it was legal -- but only if DISALLOW is set.
 */
static XP_Bool
commitMoveImpl( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 player,
                TrayTileSet* newTilesP, XP_Bool forced )
{
    XP_Bool inDupeMode = inDuplicateMode(ctrlr);
    XP_ASSERT( ctrlr->nv.currentTurn == player || inDupeMode );
    XP_S16 turn = player;
    TrayTileSet newTiles = {};

    if ( !!newTilesP ) {
        newTiles = *newTilesP;
    }

#ifdef DEBUG
    const CurGameInfo* gi = ctrlr->vol.gi;
    if ( LP_IS_ROBOT( &gi->players[turn] ) ) {
        ModelCtxt* model = ctrlr->vol.model;
        XP_ASSERT( model_checkMoveLegal( model, xwe, turn, (XWStreamCtxt*)NULL,
                                         (WordNotifierInfo*)NULL ) );
    }
#endif

    /* commit the move.  get new tiles.  if ctrlr, send to everybody.
       if client, send to ctrlr.  */
    XP_ASSERT( turn >= 0 );

    if ( inDupeMode ) {
        dupe_storeTurn( ctrlr, xwe, turn, forced );
    } else {
        finishMove( ctrlr, xwe, &newTiles, turn );
    }

    return XP_TRUE;
}

XP_Bool
ctrl_commitMove( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 player,
                   TrayTileSet* newTilesP )
{
    return commitMoveImpl( ctrlr, xwe, player, newTilesP, XP_FALSE );
}

static void
finishMove( CtrlrCtxt* ctrlr, XWEnv xwe, TrayTileSet* newTiles, XP_U16 turn )
{
    ModelCtxt* model = ctrlr->vol.model;
    const CurGameInfo* gi = ctrlr->vol.gi;

    pool_removeTiles( ctrlr->pool, newTiles );
    ctrlr->vol.pickTilesCalled[turn] = XP_FALSE;

    XP_U16 nTilesMoved = model_getCurrentMoveCount( model, turn );
    fetchTiles( ctrlr, xwe, turn, nTilesMoved, newTiles, XP_FALSE );

    XP_Bool isClient = gi->deviceRole == ROLE_ISGUEST;
    XP_Bool isLegalMove = XP_TRUE;
    if ( isClient ) {
        /* just send to host */
        sendMoveTo( ctrlr, xwe, HOST_DEVICE, turn, XP_TRUE, newTiles,
                    (TrayTileSet*)NULL );
    } else {
        isLegalMove = checkMoveAllowed( ctrlr, xwe, turn );
        sendMoveToClientsExcept( ctrlr, xwe, turn, isLegalMove, newTiles,
                                 (TrayTileSet*)NULL, HOST_DEVICE );
    }

    model_commitTurn( model, xwe, turn, newTiles );
    sortTilesIf( ctrlr, turn );

    if ( !isLegalMove && !isClient ) {
        badWordMoveUndoAndTellUser( ctrlr, xwe, &ctrlr->bws );
        /* It's ok to free these guys.  I'm the ctrlr, and the move was made
           here, so I've notified all clients already by setting the flag (and
           passing the word) in sendMoveToClientsExcept. */
        freeBWS( MPPARM(ctrlr->mpool) &ctrlr->bws );
    }

    if (isClient && (gi->phoniesAction == PHONIES_DISALLOW)
               && nTilesMoved > 0 ) {
        SETSTATE( ctrlr, XWSTATE_MOVE_CONFIRM_WAIT );
        setTurn( ctrlr, xwe, -1 );
    } else {
        nextTurn( ctrlr, xwe, PICK_NEXT );
    }
    /* SRVR_LOGFF( "player %d now has %d tiles", turn, */
    /*           model_getNumTilesInTray( model, turn ) ); */
} /* finishMove */
    
XP_Bool
ctrl_commitTrade( CtrlrCtxt* ctrlr, XWEnv xwe, const TrayTileSet* oldTiles,
                    TrayTileSet* newTilesP )
{
    TrayTileSet newTiles = {};
    if ( !!newTilesP ) {
        newTiles = *newTilesP;
    }
    XP_U16 turn = ctrlr->nv.currentTurn;

    fetchTiles( ctrlr, xwe, turn, oldTiles->nTiles, &newTiles, XP_FALSE );

    switch ( ctrlr->vol.gi->deviceRole ) {
    case ROLE_ISGUEST:
        /* just send to ctrlr */
        sendMoveTo(ctrlr, xwe, HOST_DEVICE, turn, XP_TRUE, &newTiles, oldTiles);
        break;
    case ROLE_ISHOST:
        sendMoveToClientsExcept( ctrlr, xwe, turn, XP_TRUE, &newTiles, oldTiles,
                                 HOST_DEVICE );
        break;
    }

    ctrlr->vol.pickTilesCalled[turn] = XP_FALSE; /* in case in pick-tiles-mode */

    pool_replaceTiles( ctrlr->pool, oldTiles );
    XP_ASSERT( turn == ctrlr->nv.currentTurn );
    model_makeTileTrade( ctrlr->vol.model, turn, oldTiles, &newTiles );
    sortTilesIf( ctrlr, turn );

    nextTurn( ctrlr, xwe, PICK_NEXT );
    return XP_TRUE;
} /* ctrl_commitTrade */

XP_S16
ctrl_getCurrentTurn( const CtrlrCtxt* ctrlr, XP_Bool* isLocal )
{
    XP_S16 turn = ctrlr->nv.currentTurn;
    if ( NULL != isLocal && turn >= 0 ) {
        *isLocal = ctrlr->vol.gi->players[turn].isLocal;
    }
    return turn;
} /* ctrl_getCurrentTurn */

XP_Bool
ctrl_isPlayersTurn( const CtrlrCtxt* ctrlr, XP_U16 turn )
{
    XP_Bool result = XP_FALSE;

    if ( inDuplicateMode(ctrlr) ) {
        if ( ctrlr->vol.gi->players[turn].isLocal
             && ! ctrlr->nv.dupTurnsMade[turn] ) {
            result = XP_TRUE;
        }
    } else {
        result = turn == ctrl_getCurrentTurn( ctrlr, NULL );
    }

    // XP_LOGFF( "(%d) => %d", turn, result );
    return result;
}

XP_Bool
ctrl_getGameIsOver( const CtrlrCtxt* ctrlr )
{
    return ctrlr->nv.gameState == XWSTATE_GAMEOVER;
} /* ctrl_getGameIsOver */

/* This is completely wrong */
XP_Bool
ctrl_getGameIsConnected( const CtrlrCtxt* ctrlr )
{
    return ctrlr->nv.gameState >= XWSTATE_NEWCLIENT;
} /* ctrl_getGameIsConnected */

XP_U16
ctrl_getMissingPlayers( const CtrlrCtxt* ctrlr )
{
    /* list players for which we're reserving slots that haven't shown up yet.
     * If I'm a guest and haven't received the registration message and set
     * ctrlr->nv.addresses[0].channelNo, all non-local players are missing.
     * If I'm a host, players whose deviceIndex is -1 are missing.
    */

    XP_U16 result = 0;
    XP_U16 ii;
    switch( ctrlr->vol.gi->deviceRole ) {
    case ROLE_ISGUEST:
        if ( 0 == ctrlr->nv.addresses[0].channelNo ) {
            const CurGameInfo* gi = ctrlr->vol.gi;
            const LocalPlayer* lp = gi->players;
            for ( ii = 0; ii < gi->nPlayers; ++ii ) {
                if ( !lp->isLocal ) {
                    result |= 1 << ii;
                }
                ++lp;
            }
        }
        break;
    case ROLE_ISHOST:
        if ( 0 < ctrlr->nv.pendingRegistrations ) {
            XP_U16 nPlayers = ctrlr->vol.gi->nPlayers;
            const CtrlrPlayer* players = ctrlr->srvPlyrs;
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
ctrl_getOpenChannel( const CtrlrCtxt* ctrlr, XP_U16* channel )
{
    XP_Bool result = XP_FALSE;
    XP_ASSERT( amHost( ctrlr ) );
    if ( amHost( ctrlr ) && 0 < ctrlr->nv.pendingRegistrations ) {
        XP_PlayerAddr channelNo = 1;
        const XP_U16 nPlayers = ctrlr->vol.gi->nPlayers;
        const CtrlrPlayer* players = ctrlr->srvPlyrs;
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
ctrl_canRematch( const CtrlrCtxt* ctrlr, XP_Bool* canOrderP )
{
    SRVR_LOGFFV( "nDevices: %d; nPlayers: %d",
                 ctrlr->nv.nDevices, ctrlr->vol.gi->nPlayers );
    const CurGameInfo* gi = ctrlr->vol.gi;
    XP_Bool result;
    XP_Bool canOrder = XP_TRUE;
    switch ( gi->deviceRole ) {
    case ROLE_STANDALONE:
        result = XP_TRUE;       /* can always rematch a local game */
        break;
    case ROLE_ISHOST:
        /* have all expected clients connected? */
        result = XWSTATE_RECEIVED_ALL_REG <= ctrlr->nv.gameState
            && ctrlr->nv.nDevices == ctrlr->vol.gi->nPlayers;
        break;
    case ROLE_ISGUEST:
        if ( 2 == gi->nPlayers ) {
            result = XP_TRUE;
        } else {
            result = 0 < ctrlr->nv.rematch.addrsLen;
            canOrder = STREAM_VERS_REMATCHORDER <= ctrlr->nv.streamVersion;
        }
        break;
    }

    if ( !!canOrderP ) {
        *canOrderP = canOrder;
    }

    /* LOG_RETURNF( "%s", boolToStr(result) ); */
    return result;
}

void
ctrl_setReMissing( const CtrlrCtxt* ctrlr, GameSummary* gs )
{
    const CurGameInfo* gi = ctrlr->vol.gi;
    XP_U16 nInvited = 0;
    XP_U16 nMissing = 0;
    if ( XWSTATE_BEGIN < ctrlr->nv.gameState ) {
        /* do nothing */
    } else if ( amHost( ctrlr ) ) {
        nMissing = ctrlr->nv.pendingRegistrations;
        if ( 0 < nMissing ) {
            comms_getInvited( ctrlr->vol.comms, &nInvited );
            if ( nMissing < nInvited ) {
                nInvited = nMissing;
            }
        }
    } else if ( ROLE_ISGUEST == gi->deviceRole ) {
        nMissing = gi->nPlayers - gi_countLocalPlayers( gi, XP_FALSE);
    }

    gs->nInvited = nInvited;
    gs->nMissing = nMissing;
}

/* Modify the RematchInfo data to be consistent with the order we'll enforce
   as invitees join the new game.
 */
static void
sortBySame( const CtrlrCtxt* ctrlr, NewOrder* nop )
{
    const CurGameInfo* gi = ctrlr->vol.gi;
    for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
        nop->order[ii] = ii;
    }
}

static void
sortByScoreLow( const CtrlrCtxt* ctrlr, NewOrder* nop )
{
    const CurGameInfo* gi = ctrlr->vol.gi;

    ScoresArray scores = {};
    model_getCurScores( ctrlr->vol.model, &scores, ctrl_getGameIsOver(ctrlr) );

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
sortByScoreHigh( const CtrlrCtxt* ctrlr, NewOrder* nop )
{
    sortByScoreLow( ctrlr, nop );

    const CurGameInfo* gi = ctrlr->vol.gi;
    for ( int ii = 0, jj = gi->nPlayers - 1; ii < jj; ++ii, --jj ) {
        int tmp = nop->order[ii];
        nop->order[ii] = nop->order[jj];
        nop->order[jj] = tmp;
    }
}

static void
sortByRandom( const CtrlrCtxt* ctrlr, NewOrder* nop )
{
    const CurGameInfo* gi = ctrlr->vol.gi;
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
sortByName( const CtrlrCtxt* ctrlr, NewOrder* nop )
{
    const CurGameInfo* gi = ctrlr->vol.gi;
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
setPlayerOrder( const CtrlrCtxt* XP_UNUSED_DBG(ctrlr), const NewOrder* nop,
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

    LOG_GI( gi, "end" );
    if ( !!rip ) {
        LOG_RI( rip );
    }

    return XP_TRUE;
} /* setPlayerOrder */

XP_Bool
ctrl_getRematchInfo( const CtrlrCtxt* ctrlr, XWEnv xwe,
                       RematchOrder ro, CurGameInfo* newGI, RematchInfo** ripp )
{
    NewOrder no;
    ctrl_figureOrder( ctrlr, ro, &no );

    XP_Bool success = ctrl_canRematch( ctrlr, NULL );
    if ( success ) {
        XP_ASSERT( 0 != newGI->gameID );
        if ( ROLE_ISGUEST == newGI->deviceRole ) {
            newGI->deviceRole = ROLE_ISHOST; /* we'll be inviting */
            newGI->forceChannel = 0;
        }
        LOG_GI( newGI, "ready to invite" );

        success = getRematchInfoImpl( ctrlr, xwe, newGI, &no, ripp );
    }
    return success;
}

static XP_Bool
getRematchInfoImpl( const CtrlrCtxt* ctrlr, XWEnv xwe, CurGameInfo* newGI,
                    const NewOrder* nop, RematchInfo** ripp )
{
    XP_Bool success = XP_TRUE;
    RematchInfo ri = {};
    const CommsCtxt* comms = ctrlr->vol.comms;
    /* Now build the address list. Simple cases are STANDALONE, when I'm
       the host, or when there are only two devices/players. If I'm guest
       and there is another guest, I count on the host having sent rematch
       info, and *that* info has an old and a new format. Sheesh. */
    XP_Bool canOrder = XP_TRUE;
    if ( !comms ) {
        /* no addressing to do!! */
    } else if ( amHost( ctrlr ) || 2 == newGI->nPlayers ) {
        for ( int ii = 0; ii < newGI->nPlayers; ++ii ) {
            if ( newGI->players[ii].isLocal ) {
                ri_addLocal( &ri );
            } else {
                CommsAddrRec addr;
                if ( amHost(ctrlr) ) {
                    XP_S8 deviceIndex = ctrlr->srvPlyrs[ii].deviceIndex;
                    XP_ASSERT( deviceIndex != RIP_LOCAL_INDX );
                    XP_PlayerAddr channelNo =
                        ctrlr->nv.addresses[deviceIndex].channelNo;
                    comms_getChannelAddr( comms, channelNo, &addr );
                } else {
                    comms_getHostAddr( comms, &addr );
                }
                ri_addAddrAt( ctrlr->vol.dutil, xwe, &ri, &addr, ii );
            }
        }
    } else if ( !!ctrlr->nv.rematch.addrs ) {
        XP_U16 streamVersion = ctrlr->nv.streamVersion;
        if ( STREAM_VERS_REMATCHORDER <= streamVersion ) {
            loadRemoteRI( ctrlr, newGI, &ri );

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

            XWStreamCtxt* stream = mkCtrlrStream( ctrlr,
                                                   ctrlr->nv.streamVersion );
            strm_putBytes( stream, ctrlr->nv.rematch.addrs,
                             ctrlr->nv.rematch.addrsLen );
            while ( 0 < strm_getSize( stream ) ) {
                XP_ASSERT( nAddrs < VSIZE(addrs) );
                addrFromStream( &addrs[nAddrs++], stream );
            }
            strm_destroy( stream );

            int nextRemote = 0;
            for ( int ii = 0; success && ii < newGI->nPlayers; ++ii ) {
                if ( newGI->players[ii].isLocal ) {
                    ri_addLocal( &ri );
                } else if ( nextRemote < nAddrs ) {
                    ri_addAddrAt( ctrlr->vol.dutil, xwe, &ri,
                                  &addrs[nextRemote++], ii );
                } else {
                    SRVR_LOGFF( "ERROR: not enough addresses for all"
                                " remote players" );
                    success = XP_FALSE;
                }
            }
            if ( success ) {
                success = nextRemote == nAddrs;
                if ( !success ) {
                    XP_LOGFF( "addr count mismatch: nextRemote: %d vs nAddrs: %d",
                              nextRemote, nAddrs );
                }
            }
        }
    } else {
        success = XP_FALSE;
        XP_LOGFF( "rematch.addrs not set" );
    }

    if ( success && canOrder ) {
        if ( !!comms ) {
            assertRI( &ri, newGI );
        }
        success = setPlayerOrder( ctrlr, nop, newGI, !!comms ? &ri : NULL );
        if ( !success ) {
            XP_LOGFF( "setPlayerOrder failed" );
        }
    }

    if ( success && !!comms ) {
        LOG_RI( &ri );
        assertRI( &ri, newGI );
        XP_ASSERT( success );
        *ripp = XP_MALLOC(ctrlr->mpool, sizeof(**ripp));
        **ripp = ri;
    } else {
        *ripp = NULL;
    }
    // XP_ASSERT( success );       /* this is firing */

    LOG_RETURNF( "%s", boolToStr(success)  );
    return success;
} /* getRematchInfoImpl */

void
ctrl_disposeRematchInfo( CtrlrCtxt* XP_UNUSED_DBG(ctrlr), RematchInfo* rip )
{
    SRVR_LOGFFV( "(%p)", rip );
    if ( !!rip ) {
        LOG_RI( rip );
        XP_FREE( ctrlr->mpool, rip );
    }
}

XP_Bool
ctrl_ri_getAddr( const RematchInfo* rip, XP_U16 nth,
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
ctrl_figureOrder( const CtrlrCtxt* ctrlr, RematchOrder ro, NewOrder* nop )
{
    XP_MEMSET( nop, 0, sizeof(*nop) );

    void (*proc)(const CtrlrCtxt*, NewOrder*) = NULL;
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

    (*proc)( ctrlr, nop );
}

/* Record the desired order, which is already set in the RematchInfo passed
   in, so we can enforce it as clients register. */
void
ctrl_setRematchOrder( CtrlrCtxt* ctrlr, const RematchInfo* rip )
{
    if ( amHost( ctrlr ) ) {   /* standalones can call without harm.... */
        XP_ASSERT( !!rip );
        XP_ASSERT( !ctrlr->nv.rematch.order );
        ctrlr->nv.rematch.order = XP_MALLOC( ctrlr->mpool, sizeof(*rip) );
        *ctrlr->nv.rematch.order = *rip;
        ctrlr->nv.flags |= MASK_HAVE_RIP_INFO + MASK_IS_FROM_REMATCH;
    }
}

XP_Bool
ctrl_isFromRematch( const CtrlrCtxt* ctrlr )
{
    return 0 != (ctrlr->nv.flags & MASK_IS_FROM_REMATCH);
}

void
ctrl_gatherPlayers( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U32 created )
{
    XP_Bool flagSet = 0 != (ctrlr->nv.flags & FLAG_HARVEST_READY);
    if ( flagSet ) {
        const CurGameInfo* gi = ctrlr->vol.gi;
        XW_DUtilCtxt* dutil = ctrlr->vol.dutil;

        NewOrder no;
        ctrl_figureOrder( ctrlr, RO_SAME, &no );

        CurGameInfo tmpGi = *gi;
        RematchInfo* ripp;
        if ( getRematchInfoImpl( ctrlr, xwe, &tmpGi, &no, &ripp ) ) {
            for ( int ii = 0, nRemotes = 0; ii < gi->nPlayers; ++ii ) {
                const LocalPlayer* lp = &gi->players[ii];
                /* order unchanged? */
                XP_ASSERT( lp->name == gi->players[ii].name );
                if ( !lp->isLocal ) {
                    CommsAddrRec addr;
                    XP_U16 nPlayersH;
                    if ( !ctrl_ri_getAddr( ripp, nRemotes++, &addr, &nPlayersH ) ) {
                        break;
                    }
                    XP_ASSERT( 1 == nPlayersH ); /* else fixme... */
                    kplr_addAddr( dutil, xwe, &addr, lp->name, created );
                }
            }

            ctrl_disposeRematchInfo( ctrlr, ripp );
        }
    }
}

#ifdef DEBUG
static void
log_ri( const CtrlrCtxt* ctrlr, const RematchInfo* rip,
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
            /* XP_SNPRINTF( buf, VSIZE(buf), "[%d of %d]: %s from %s", */
            /*              ii, rip->nAddrs, __func__, caller ); */
            logAddr( ctrlr->vol.dutil, &rip->addrs[ii], __func__ );
        }
    }
}
#endif

static void
ri_toStream( XWStreamCtxt* stream, const RematchInfo* rip,
             const CtrlrCtxt* XP_UNUSED_DBG(ctrlr) )
{
    LOG_RI(rip);
    XP_U16 nPlayers = !!rip ? rip->nPlayers : 0;
    for ( int ii = 0; ii < nPlayers; ++ii ) {
        XP_S8 indx = rip->addrIndices[ii];
        if ( RIP_LOCAL_INDX != indx ) {
            strm_putBits( stream, PLAYERNUM_NBITS, indx );
        }
    }

    for ( int ii = 0; ii < rip->nAddrs; ++ii ) {
        addrToStream( stream, &rip->addrs[ii] );
    }
}

static void
ri_fromStream( RematchInfo* rip, XWStreamCtxt* stream,
               const CtrlrCtxt* ctrlr )
{
    const CurGameInfo* gi = ctrlr->vol.gi;
    XP_MEMSET( rip, 0, sizeof(*rip) );
    rip->nPlayers = gi->nPlayers;

    for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
        if ( gi->players[ii].isLocal ) {
            rip->addrIndices[ii] = RIP_LOCAL_INDX;
        } else {
            XP_U16 indx = strm_getBits( stream, PLAYERNUM_NBITS );
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
ri_addHostAddrs( RematchInfo* rip, const CtrlrCtxt* ctrlr )
{
    for ( int ii = 0; ii < rip->nAddrs; ++ii ) {
        if ( addr_isEmpty( &rip->addrs[ii] ) ) {
            comms_getHostAddr( ctrlr->vol.comms, &rip->addrs[ii] );
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
ctrl_getLastMoveTime( const CtrlrCtxt* ctrlr )
{
    return ctrlr->nv.lastMoveTime;
}

static void
doEndGame( CtrlrCtxt* ctrlr, XWEnv xwe, XP_S16 quitter )
{
    XP_ASSERT( quitter < ctrlr->vol.gi->nPlayers );
    SETSTATE( ctrlr, XWSTATE_GAMEOVER );
    setTurn( ctrlr, xwe, -1 );
    ctrlr->nv.quitter = quitter;

    (*ctrlr->vol.gameOverListener)( xwe, ctrlr->vol.gameOverData, quitter );
} /* doEndGame */

static void 
putQuitter( const CtrlrCtxt* ctrlr, XWStreamCtxt* stream, XP_S16 quitter )
{
    if ( STREAM_VERS_DICTNAME <= ctrlr->nv.streamVersion ) {
        strm_putU8( stream, quitter );
    }
}

static void
getQuitter( const CtrlrCtxt* ctrlr, XWStreamCtxt* stream, XP_S8* quitter )
{
    *quitter = STREAM_VERS_DICTNAME <= ctrlr->nv.streamVersion
            ? strm_getU8( stream ) : -1;
}

/* Somebody wants to end the game.
 *
 * If I'm the ctrlr, I send a END_GAME message to all clients.  If I'm a
 * client, I send the GAME_OVER_REQUEST message to the ctrlr.  If I'm the
 * ctrlr and this is called in response to a GAME_OVER_REQUEST, send the
 * GAME_OVER message to all clients including the one that requested it.
 */
static void
endGameInternal( CtrlrCtxt* ctrlr, XWEnv xwe, GameEndReason XP_UNUSED(why),
                 XP_S16 quitter )
{
    XP_ASSERT( ctrlr->nv.gameState != XWSTATE_GAMEOVER );
    XP_ASSERT( quitter < ctrlr->vol.gi->nPlayers );

    if ( ctrlr->vol.gi->deviceRole != ROLE_ISGUEST ) {

        XP_U16 devIndex;
        for ( devIndex = 1; devIndex < ctrlr->nv.nDevices; ++devIndex ) {
            XWStreamCtxt* stream;
            stream = messageStreamWithHeader( ctrlr, devIndex,
                                              XWPROTO_END_GAME );
            putQuitter( ctrlr, stream, quitter );
            closeAndSend( ctrlr, xwe, stream );
        }
        doEndGame( ctrlr, xwe, quitter );

    } else {
        XWStreamCtxt* stream;
        stream = messageStreamWithHeader( ctrlr, HOST_DEVICE,
                                          XWPROTO_CLIENT_REQ_END_GAME );
        putQuitter( ctrlr, stream, quitter );
        closeAndSend( ctrlr, xwe, stream );

        /* Do I want to change the state I'm in?  I don't think so.... */
    }
} /* endGameInternal */

void
ctrl_endGame( CtrlrCtxt* ctrlr, XWEnv xwe )
{
    XW_State gameState = ctrlr->nv.gameState;
    if ( gameState < XWSTATE_GAMEOVER && gameState >= XWSTATE_INTURN ) {
        endGameInternal( ctrlr, xwe, END_REASON_USER_REQUEST, ctrlr->nv.currentTurn );
    }
} /* ctrl_endGame */

void
ctrl_inviteeName( const CtrlrCtxt* ctrlr,
                  XWEnv xwe, XP_U16 playerPosn,
                  XP_UCHAR* buf, XP_U16* bufLen )
{
    int nameIndx = 0;
    for ( int ii = 0; ii <= playerPosn; ++ii ) {
        const CtrlrPlayer* sp = &ctrlr->srvPlyrs[ii];
        if ( -1 == sp->deviceIndex ) { /* not yet claimed */
            if ( playerPosn == ii ) {

                CommsCtxt* comms = ctrlr->vol.comms;
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
tileCountsOk( const CtrlrCtxt* ctrlr )
{
    XP_Bool maybeOver = 0 == pool_getNTilesLeft( ctrlr->pool );
    if ( maybeOver ) {
        ModelCtxt* model = ctrlr->vol.model;
        XP_U16 nPlayers = ctrlr->vol.gi->nPlayers;
        XP_Bool inDupMode = inDuplicateMode( ctrlr );
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
setTurn( CtrlrCtxt* ctrlr, XWEnv xwe, XP_S16 turn )
{
    XP_ASSERT( -1 == turn
               || (!amHost(ctrlr) || (0 == ctrlr->nv.pendingRegistrations)));
    XP_Bool inDupMode = inDuplicateMode( ctrlr );
    if ( inDupMode || ctrlr->nv.currentTurn != turn || 1 == ctrlr->vol.gi->nPlayers ) {
        if ( DUP_PLAYER == turn && inDupMode ) {
            turn = dupe_nextTurn( ctrlr );
        } else if ( PICK_CUR == turn ) {
            XP_ASSERT(0);       /* this should never happen */
        } else if ( 0 <= turn && !inDupMode ) {
            XP_ASSERT( turn == model_getNextTurn( ctrlr->vol.model ) );
        }
        ctrlr->nv.currentTurn = turn;
        ctrlr->nv.lastMoveTime = dutil_getCurSeconds( ctrlr->vol.dutil, xwe );
        callTurnChangeListener( ctrlr, xwe );
        gr_postEvents( ctrlr->vol.dutil, xwe, ctrlr->vol.gr, GCE_TURN_CHANGED );
    }
}

static void
tellMoveWasLegal( CtrlrCtxt* ctrlr, XWEnv xwe )
{
    XWStreamCtxt* stream =
        messageStreamWithHeader( ctrlr, ctrlr->lastMoveSource,
                                 XWPROTO_MOVE_CONFIRM );

    closeAndSend( ctrlr, xwe, stream );

    SETSTATE( ctrlr, XWSTATE_INTURN );
} /* tellMoveWasLegal */

static XP_Bool
handleIllegalWord( CtrlrCtxt* ctrlr, XWEnv xwe, XWStreamCtxt* incoming )
{
    BadWordsState bws = {{}};

    (void)strm_getBits( incoming, PLAYERNUM_NBITS );
    bwsFromStream( MPPARM(ctrlr->mpool) incoming, &bws );

    badWordMoveUndoAndTellUser( ctrlr, xwe, &bws );

    freeBWS( MPPARM(ctrlr->mpool) &bws );

    SETSTATE( ctrlr, XWSTATE_INTURN );
    return XP_TRUE;
} /* handleIllegalWord */

static XP_Bool
handleMoveOk( CtrlrCtxt* ctrlr, XWEnv xwe, XWStreamCtxt* XP_UNUSED(incoming) )
{
    XP_ASSERT( ctrlr->vol.gi->deviceRole == ROLE_ISGUEST );
    XP_Bool accepted = ctrlr->nv.gameState == XWSTATE_MOVE_CONFIRM_WAIT;
    if ( accepted ) {
        SETSTATE( ctrlr, XWSTATE_INTURN );
        nextTurn( ctrlr, xwe, PICK_CUR );
    }
    return accepted;
} /* handleMoveOk */

static void
sendUndoTo( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 devIndex, XP_U16 nUndone,
            XP_U16 lastUndone, XP_U32 newHash )
{
    XWStreamCtxt* stream;
    const CurGameInfo* gi = ctrlr->vol.gi;
    XW_Proto code = gi->deviceRole == ROLE_ISGUEST?
        XWPROTO_UNDO_INFO_CLIENT : XWPROTO_UNDO_INFO_CTRLR;

    stream = messageStreamWithHeader( ctrlr, devIndex, code );

    strm_putU16( stream, nUndone );
    strm_putU16( stream, lastUndone );
    strm_putU32( stream, newHash );

    closeAndSend( ctrlr, xwe, stream );
} /* sendUndoTo */

static void
sendUndoToClientsExcept( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 skip, XP_U16 nUndone,
                         XP_U16 lastUndone, XP_U32 newHash )
{
    XP_U16 devIndex;

    for ( devIndex = 1; devIndex < ctrlr->nv.nDevices; ++devIndex ) {
        if ( devIndex != skip ) {
            sendUndoTo( ctrlr, xwe, devIndex, nUndone, lastUndone, newHash );
        }
    }
} /* sendUndoToClientsExcept */

static XP_Bool
reflectUndos( CtrlrCtxt* ctrlr, XWEnv xwe, XWStreamCtxt* stream, XW_Proto code )
{
    LOG_FUNC();
    XP_U16 nUndone;
    XP_S16 lastUndone;
    XP_U16 turn;
    ModelCtxt* model = ctrlr->vol.model;
    XP_Bool success = XP_TRUE;

    nUndone = strm_getU16( stream );
    lastUndone = strm_getU16( stream );
    XP_U32 newHash = 0;
    if ( 0 < strm_getSize( stream ) ) {
        newHash = strm_getU32( stream );
    }
    XP_ASSERT( 0 == strm_getSize( stream ) );

    if ( 0 == newHash ) {
        success = model_undoLatestMoves( model, xwe, ctrlr->pool, nUndone, &turn,
                                         &lastUndone );
        XP_ASSERT( turn == model_getNextTurn( model ) );
    } else {
        success = model_popToHash( model, xwe, newHash, ctrlr->pool );
        turn = model_getNextTurn( model );
    }

    if ( success ) {
        SRVR_LOGFF( "popped down to %X", model_getHash( model ) );
        sortTilesIf( ctrlr, turn );

        if ( code == XWPROTO_UNDO_INFO_CLIENT ) { /* need to inform */
            XP_U16 sourceClientIndex = getIndexForStream( ctrlr, stream );

            sendUndoToClientsExcept( ctrlr, xwe, sourceClientIndex, nUndone,
                                     lastUndone, newHash );
        }

        util_informUndo( *ctrlr->vol.utilp, xwe );
        nextTurn( ctrlr, xwe, turn );
    } else {
        SRVR_LOGFF( "unable to pop to hash %X; dropping", newHash );
        // XP_ASSERT(0);
        success = XP_TRUE;      /* Otherwise we'll stall */
    }

    LOG_RETURNF( "%s", boolToStr(success) );
    return success;
} /* reflectUndos */

XP_Bool
ctrl_handleUndo( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 limit )
{
    LOG_FUNC();
    XP_Bool result = XP_FALSE;
    XP_U16 lastTurnUndone = 0; /* quiet compiler good */
    XP_U16 nUndone = 0;
    XP_U16 lastUndone = 0xFFFF;

    ModelCtxt* model = ctrlr->vol.model;
    const CurGameInfo* gi = ctrlr->vol.gi;
    XP_ASSERT( !!model );

    /* Undo until we find we've just undone a non-robot move.  The point is
       not to stop with a robot about to act (since that's a bit pointless.)
       The exception is that if the first move was a robot move we'll stop
       there, and it will immediately move again. */
    for ( ; ; ) {
        XP_S16 moveNum = -1; /* don't need it checked */
        if ( !model_undoLatestMoves( model, xwe, ctrlr->pool, 1, &lastTurnUndone,
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
        if ( ctrlr->vol.gi->deviceRole == ROLE_ISGUEST ) {
            sendUndoTo( ctrlr, xwe, HOST_DEVICE, nUndone, lastUndone, newHash );
        } else {
            sendUndoToClientsExcept( ctrlr, xwe, HOST_DEVICE, nUndone,
                                     lastUndone, newHash );
        }
        sortTilesIf( ctrlr, lastTurnUndone );
        nextTurn( ctrlr, xwe, lastTurnUndone );
    } else {
        /* I'm a bit nervous about this.  Is this the ONLY thing that cause
           nUndone to come back 0? */
        util_userError( *ctrlr->vol.utilp, xwe, ERR_CANT_UNDO_TILEASSIGN );
    }

    LOG_RETURNF( "%s", boolToStr(result) );
    return result;
} /* ctrl_handleUndo */

static void
writeProto( const CtrlrCtxt* ctrlr, XWStreamCtxt* stream, XW_Proto proto )
{
#ifdef STREAM_VERS_BIGBOARD
    XP_ASSERT( ctrlr->nv.streamVersion > 0 );
    if ( STREAM_SAVE_PREVWORDS < ctrlr->nv.streamVersion ) {
        strm_putBits( stream, XWPROTO_NBITS, XWPROTO_NEW_PROTO );
        strm_putBits( stream, 8, ctrlr->nv.streamVersion );
    }
    strm_setVersion( stream, ctrlr->nv.streamVersion );
#else
    XP_USE(ctrlr);
#endif
    strm_putBits( stream, XWPROTO_NBITS, proto );
}

static XW_Proto
readProto( CtrlrCtxt* ctrlr, XWStreamCtxt* stream )
{
    XW_Proto proto = (XW_Proto)strm_getBits( stream, XWPROTO_NBITS );
    XP_U8 version = STREAM_SAVE_PREVWORDS; /* version prior to fmt change */
#ifdef STREAM_VERS_BIGBOARD
    if ( XWPROTO_NEW_PROTO == proto ) {
        version = strm_getBits( stream, 8 );
        proto = (XW_Proto)strm_getBits( stream, XWPROTO_NBITS );
    }
    ctrlr->nv.streamVersion = version;
#else
    XP_USE(ctrlr);
#endif
    strm_setVersion( stream, version );
    return proto;
}

XP_Bool
ctrl_receiveMessage( CtrlrCtxt* ctrlr, XWEnv xwe, XWStreamCtxt* incoming,
                     XP_Bool* needsChatNotifyP )
{
    LOG_FUNC();
    XP_Bool accepted = XP_FALSE;
    XP_Bool isHost = amHost( ctrlr );
    const XW_Proto code = readProto( ctrlr, incoming );
    SRVR_LOGFFV( "code=%s", codeToStr(code) );

    switch ( code ) {
    case XWPROTO_DEVICE_REGISTRATION:
        accepted = isHost;
        if ( accepted ) {
        /* This message is special: doesn't have the header that's possible
           once the game's in progress and communication's been
           established. */
            SRVR_LOGFF( "somebody's registering!!!" );
            accepted = handleRegistrationMsg( ctrlr, xwe, incoming );
        } else {
            SRVR_LOGFF( "WTF: I'm not a host!!" );
        }
        break;
    case XWPROTO_CLIENT_SETUP:
        accepted = !isHost
            && XWSTATE_NONE == ctrlr->nv.gameState
            && client_readInitialMessage( ctrlr, xwe, incoming );
        break;
    case XWPROTO_CHAT:
        accepted = receiveChat( ctrlr, xwe, incoming, needsChatNotifyP );
        break;
    case XWPROTO_MOVEMADE_INFO_GUEST: /* client is reporting a move */
        if ( XWSTATE_INTURN == ctrlr->nv.gameState ) {
            accepted = reflectMoveAndInform( ctrlr, xwe, incoming );
        } else {
            SRVR_LOGFF( "bad state: %s; dropping", getStateStr( ctrlr->nv.gameState ) );
            accepted = XP_TRUE;
        }
        break;

    case XWPROTO_MOVEMADE_INFO_HOST: /* ctrlr telling me about a move */
        if ( isHost ) {
            SRVR_LOGFFV( "%s received by ctrlr!", codeToStr(code) );
            accepted = XP_FALSE;
        } else {
            accepted = reflectMove( ctrlr, xwe, incoming );
        }
        if ( accepted ) {
            nextTurn( ctrlr, xwe, PICK_NEXT );
        } else {
            XP_ASSERT(0);       /* understand why */
            // accepted = XP_TRUE; /* don't stall.... */
            accepted = XP_FALSE; /* otherwise move won't be resent, and we'll stall!! */
            SRVR_LOGFF( "dropping move: state=%s", getStateStr(ctrlr->nv.gameState ) );
        }
        break;

    case XWPROTO_UNDO_INFO_CLIENT:
    case XWPROTO_UNDO_INFO_CTRLR:
        accepted = reflectUndos( ctrlr, xwe, incoming, code );
        /* nextTurn is called by reflectUndos */
        break;

    case XWPROTO_BADWORD_INFO:
        accepted = handleIllegalWord( ctrlr, xwe, incoming );
        if ( accepted && ctrlr->nv.gameState != XWSTATE_GAMEOVER ) {
            nextTurn( ctrlr, xwe, PICK_CUR );
        }
        break;

    case XWPROTO_MOVE_CONFIRM:
        accepted = handleMoveOk( ctrlr, xwe, incoming );
        break;

    case XWPROTO_CLIENT_REQ_END_GAME: {
        XP_S8 quitter;
        getQuitter( ctrlr, incoming, &quitter );
        endGameInternal( ctrlr, xwe, END_REASON_USER_REQUEST, quitter );
        accepted = XP_TRUE;
    }
        break;
    case XWPROTO_END_GAME: {
        XP_S8 quitter;
        getQuitter( ctrlr, incoming, &quitter );
        doEndGame( ctrlr, xwe, quitter );
        accepted = XP_TRUE;
    }
        break;
    case XWPROTO_DUPE_STUFF:
        accepted = dupe_handleStuff( ctrlr, xwe, incoming );
        break;
    default:
        XP_WARNF( "%s: Unknown code on incoming message: %d\n",
                  __func__, code );
        // will happen e.g. if we don't support chat and remote sends. Is ok.
        break;
    } /* switch */

    XP_ASSERT( isHost == amHost( ctrlr ) ); /* caching value is ok? */

    SRVR_LOGFF( "=> %s (code=%s)", boolToStr(accepted), codeToStr(code) );
    // XP_ASSERT( accepted );      /* do not commit!!! */
    LOG_RETURNF( "%s", boolToStr(accepted) );
    return accepted;
} /* ctrl_receiveMessage */

XWStreamCtxt*
ctrl_formatDictCounts( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 nCols,
                       XP_Bool allFaces )
{
    XWStreamCtxt* stream = dvc_makeStream( ctrlr->vol.dutil );
    const DictionaryCtxt* dict;
    Tile tile;
    XP_U16 nChars, nPrinted;
    XP_UCHAR buf[48];
    const XP_UCHAR* fmt = dutil_getUserString( ctrlr->vol.dutil, xwe,
                                               STRS_VALUES_HEADER );
    const XP_UCHAR* langName;

    XP_ASSERT( !!ctrlr->vol.model );

    dict = model_getDictionary( ctrlr->vol.model );
    langName = dict_getLangName( dict );
    XP_SNPRINTF( buf, sizeof(buf), fmt, langName );
    strm_catString( stream, buf );

    nChars = dict_numTileFaces( dict );
    XP_U16 boardSize = ctrlr->vol.gi->boardSize;
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
            strm_catString( stream, buf );
        }

        if ( ++tile >= nChars ) {
            break;
        } else if ( count > 0 ) {
            if ( ++nPrinted % nCols == 0 ) {
                strm_catString( stream, XP_CR );
            } else {
                strm_catString( stream, (void*)"   " );
            }
        }
    }
    return stream;
} /* ctrl_formatDictCounts */

/* Print the faces of all tiles left in the pool, including those currently in
 * trays !unless! player is >= 0, in which case his tiles get removed from the
 * pool.  The idea is to show him what tiles are left in play.
 */
XWStreamCtxt*
ctrl_formatRemainingTiles( CtrlrCtxt* ctrlr, XWEnv xwe, XP_S16 player )
{
    XWStreamCtxt* stream = dvc_makeStream( ctrlr->vol.dutil );
    PoolContext* pool = ctrlr->pool;
    if ( !!pool ) {
        XP_UCHAR buf[128];
        const DictionaryCtxt* dict = model_getDictionary( ctrlr->vol.model );
        Tile tile;
        XP_U16 nChars = dict_numTileFaces( dict );
        XP_U16 offset;
        XP_U16 counts[MAX_UNIQUE_TILES+1]; /* 1 for the blank */
        XP_U16 nLeft = pool_getNTilesLeft( pool );
        XP_UCHAR cntsBuf[512];

        XP_ASSERT( !!ctrlr->vol.model );

        const XP_UCHAR* fmt = dutil_getUserQuantityString( ctrlr->vol.dutil, xwe,
                                                           STRD_REMAINS_HEADER,
                                                           nLeft );
        XP_SNPRINTF( buf, sizeof(buf), fmt, nLeft );
        strm_catString( stream, buf );
        strm_catString( stream, "\n\n" );

        XP_MEMSET( counts, 0, sizeof(counts) );
        model_countAllTrayTiles( ctrlr->vol.model, counts, player );

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

        fmt = dutil_getUserQuantityString( ctrlr->vol.dutil, xwe, STRD_REMAINS_EXPL,
                                           nLeft );
        XP_SNPRINTF( buf, sizeof(buf), fmt, nLeft );
        strm_catString( stream, buf );

        strm_catString( stream, cntsBuf );
    }
    return stream;
} /* ctrl_formatRemainingTiles */

#ifdef XWFEATURE_BONUSALL
XP_U16
ctrl_figureFinishBonus( const CtrlrCtxt* ctrlr, XP_U16 turn )
{
    XP_U16 result = 0;
    if ( 0 == pool_getNTilesLeft( ctrlr->pool ) ) {
        XP_U16 nOthers = ctrlr->vol.gi->nPlayers - 1;
        if ( 0 < nOthers ) {
            Tile tile;
            const DictionaryCtxt* dict = model_getDictionary( ctrlr->vol.model );
            XP_U16 counts[dict_numTileFaces( dict )];
            XP_MEMSET( counts, 0, sizeof(counts) );
            model_countAllTrayTiles( ctrlr->vol.model, counts, turn );
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
printPlayer( const CtrlrCtxt* ctrlr, XWStreamCtxt* stream, XP_U16 index, 
             const XP_UCHAR* placeBuf, ScoresArray* scores, 
             ScoresArray* tilePenalties, XP_U16 place )
{
    XP_UCHAR buf[128];
    const CurGameInfo* gi = ctrlr->vol.gi;
    ModelCtxt* model = ctrlr->vol.model;
    XP_Bool firstDone = model_getNumTilesTotal( model, index ) == 0;
    XP_UCHAR tmpbuf[48];
    XP_U16 addSubKey = firstDone? STRD_REMAINING_TILES_ADD : STRD_UNUSED_TILES_SUB;
    const XP_UCHAR* addSubString = util_getUserString( *ctrlr->vol.utilp, xwe, addSubKey );
    XP_UCHAR* timeStr = (XP_UCHAR*)"";
    XP_UCHAR timeBuf[16];
    if ( gi->timerEnabled ) {
        XP_U16 penalty = player_timePenalty( gi, index );
        if ( penalty > 0 ) {
            XP_SNPRINTF( timeBuf, sizeof(timeBuf), 
                         util_getUserString( *ctrlr->vol.utilp, xwe,
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
        strm_catString( stream, XP_CR );
    }
    strm_catString( stream, buf );
} /* printPlayer */
#endif

void
ctrl_writeFinalScores( CtrlrCtxt* ctrlr, XWEnv xwe, XWStreamCtxt* stream )
{
    ScoresArray scores;
    ScoresArray tilePenalties;
    XP_U16 place;
    XP_S16 quitter = ctrlr->nv.quitter;
    XP_Bool quitterDone = XP_FALSE;
    ModelCtxt* model = ctrlr->vol.model;
    const XP_UCHAR* addString = dutil_getUserString( ctrlr->vol.dutil, xwe,
                                                     STRD_REMAINING_TILES_ADD );
    const XP_UCHAR* subString = dutil_getUserString( ctrlr->vol.dutil, xwe,
                                                     STRD_UNUSED_TILES_SUB );
    XP_UCHAR* timeStr;
    const CurGameInfo* gi = ctrlr->vol.gi;
    const XP_U16 nPlayers = gi->nPlayers;

    XP_ASSERT( ctrlr->nv.gameState == XWSTATE_GAMEOVER );

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
            XP_U16 penalty = model_timePenalty( model, thisIndex );
            if ( penalty > 0 ) {
                XP_SNPRINTF( timeBuf, sizeof(timeBuf), 
                             dutil_getUserString( ctrlr->vol.dutil, xwe,
                                                  STRD_TIME_PENALTY_SUB ),
                             penalty ); /* positive for formatting */
                timeStr = timeBuf;
            }
        }

        XP_UCHAR tmpbuf[48] = {};
        if ( !inDuplicateMode( ctrlr ) ) {
            firstDone = model_getNumTilesTotal( model, thisIndex) == 0;
            XP_SNPRINTF( tmpbuf, sizeof(tmpbuf),
                         (firstDone? addString:subString),
                         firstDone?
                         tilePenalties.arr[thisIndex]:
                         -tilePenalties.arr[thisIndex] );
        }

        const XP_UCHAR* name = emptyStringIfNull(gi->players[thisIndex].name);
        if ( 0 == placeKey ) {
            const XP_UCHAR* fmt = dutil_getUserString( ctrlr->vol.dutil, xwe,
                                                      STRDSD_PLACER );
            XP_SNPRINTF( buf, sizeof(buf), fmt, place,
                         name, scores.arr[thisIndex] );
        } else {
            const XP_UCHAR* fmt = dutil_getUserString( ctrlr->vol.dutil, xwe,
                                                       placeKey );
            XP_SNPRINTF( buf, sizeof(buf), fmt, name,
                         scores.arr[thisIndex] );
        }

        if ( !inDuplicateMode( ctrlr ) ) {
            XP_UCHAR buf2[128];
            XP_SNPRINTF( buf2, sizeof(buf2), XP_CR "  (%d %s%s)",
                         model_getPlayerScore( model, thisIndex ),
                         tmpbuf, timeStr );
            XP_STRCAT( buf, buf2 );
        }

        if ( 1 < place ) {
            strm_catString( stream, XP_CR );
        }
        strm_catString( stream, buf );

        /* Don't consider this one next time around */
        scores.arr[thisIndex] = IMPOSSIBLY_LOW_SCORE;
    }
} /* ctrl_writeFinalScores */

#ifdef CPLUS
}
#endif
