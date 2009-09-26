/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997-2009 by Eric House (xwords@eehouse.org).  All rights
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

/* #include <assert.h> */

#include "comtypes.h"
#include "server.h"
#include "util.h"
#include "model.h"
#include "comms.h"
#include "memstream.h"
#include "game.h"
/* #include "board.h" */
#include "states.h"
#include "xwproto.h"
#include "util.h"
#include "pool.h"
#include "engine.h"
#include "strutils.h"

#include "LocalizedStrIncludes.h"

#ifdef CPLUS
extern "C" {
#endif

#define sEND 0x73454e44

#define LOCAL_ADDR NULL

#define IS_ROBOT(p) ((p)->isRobot)
#define IS_LOCAL(p) ((p)->isLocal)

enum {
    END_REASON_USER_REQUEST,
    END_REASON_OUT_OF_TILES,
    END_REASON_TOO_MANY_PASSES
};
typedef XP_U8 GameEndReason;

typedef struct ServerPlayer {
    EngineCtxt* engine; /* each needs his own so don't interfere each other */
    XP_S8 deviceIndex;  /* 0 means local, -1 means unknown */
} ServerPlayer;

#define UNKNOWN_DEVICE -1
#define SERVER_DEVICE 0

typedef struct RemoteAddress {
    XP_PlayerAddr channelNo;
} RemoteAddress;

/* These are the parts of the server's state that needs to be preserved
   across a reset/new game */
typedef struct ServerVolatiles {
    ModelCtxt* model;
    CommsCtxt* comms;
    XW_UtilCtxt* util;
    CurGameInfo* gi;
    TurnChangeListener turnChangeListener;
    void* turnChangeData;
    GameOverListener gameOverListener;
    void* gameOverData;
    XWStreamCtxt* prevMoveStream;     /* save it to print later */
    XW_State stateAfterShow;          /* do I need to serialize this?  What if
                                         someone quits before I can show the
                                         scores?  PENDING(ehouse) */
    XP_Bool showPrevMove;
} ServerVolatiles;

typedef struct ServerNonvolatiles {
    XP_U8 nDevices;
    XW_State gameState;
    XP_S8 currentTurn; /* invalid when game is over */
    XP_U8 pendingRegistrations;
    XP_Bool showRobotScores;

#ifdef XWFEATURE_SLOW_ROBOT
    XP_U16 robotThinkMin, robotThinkMax;   /* not saved (yet) */
#endif

    RemoteAddress addresses[MAX_NUM_PLAYERS];
} ServerNonvolatiles;

struct ServerCtxt {
    ServerVolatiles vol;
    ServerNonvolatiles nv;

    PoolContext* pool;

    BadWordInfo illegalWordInfo;
#ifndef XWFEATURE_STANDALONE_ONLY
    XP_U16 lastMoveSource;
#endif

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


#define NPASSES_OK(s) model_recentPassCountOk((s)->vol.model)

/******************************* prototypes *******************************/
static void assignTilesToAll( ServerCtxt* server );
static void resetEngines( ServerCtxt* server );
static void nextTurn( ServerCtxt* server, XP_S16 nxtTurn );

static void doEndGame( ServerCtxt* server );
static void endGameInternal( ServerCtxt* server, GameEndReason why );
static void badWordMoveUndoAndTellUser( ServerCtxt* server, 
                                        BadWordInfo* bwi );
static XP_Bool tileCountsOk( const ServerCtxt* server );
static void setTurn( ServerCtxt* server, XP_S16 turn );

#ifndef XWFEATURE_STANDALONE_ONLY
static XP_Bool handleRegistrationMsg( ServerCtxt* server, 
                                      XWStreamCtxt* stream );
static void registerRemotePlayer( ServerCtxt* server, XWStreamCtxt* stream );
static void server_sendInitialMessage( ServerCtxt* server );
static void sendBadWordMsgs( ServerCtxt* server );
static XP_Bool handleIllegalWord( ServerCtxt* server, 
                                  XWStreamCtxt* incoming );
static void tellMoveWasLegal( ServerCtxt* server );
#endif

#define PICK_NEXT -1

#if defined DEBUG && ! defined XWFEATURE_STANDALONE_ONLY
static char*
getStateStr( XW_State st )
{
#   define CASESTR(c) case c: return #c
    switch( st ) {
        CASESTR(XWSTATE_NONE);
        CASESTR(XWSTATE_BEGIN);
        CASESTR(XWSTATE_NEED_SHOWSCORE);
        CASESTR(XWSTATE_WAITING_ALL_REG);
        CASESTR(XWSTATE_RECEIVED_ALL_REG);
        CASESTR(XWSTATE_NEEDSEND_BADWORD_INFO);
        CASESTR(XWSTATE_MOVE_CONFIRM_WAIT);
        CASESTR(XWSTATE_MOVE_CONFIRM_MUSTSEND);
        CASESTR(XWSTATE_NEEDSEND_ENDGAME);
        CASESTR(XWSTATE_INTURN);
        CASESTR(XWSTATE_GAMEOVER);
    default:
        return "unknown";
    }
#   undef CASESTR
}
#endif

#if 0
static void
logNewState( XW_State old, XW_State newst )
{
    if ( old != newst ) {
        char* oldStr = getStateStr(old);
        char* newStr = getStateStr(newst);
        XP_LOGF( "state transition %s => %s", oldStr, newStr );
    }
}
# define    SETSTATE( s, st ) { XW_State old = (s)->nv.gameState; \
                                (s)->nv.gameState = (st); \
                                logNewState(old, st); }
#else
# define    SETSTATE( s, st ) (s)->nv.gameState = (st)
#endif

/*****************************************************************************
 *
 ****************************************************************************/
static void
initServer( ServerCtxt* server )
{
    setTurn( server, -1 ); /* game isn't under way yet */

    if ( 0 ) {
#ifndef XWFEATURE_STANDALONE_ONLY
    } else if ( server->vol.gi->serverRole == SERVER_ISCLIENT ) {
        SETSTATE( server, XWSTATE_NONE );
#endif
    } else {
        SETSTATE( server, XWSTATE_BEGIN );
    }

#ifndef XWFEATURE_STANDALONE_ONLY
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
#endif

    server->nv.nDevices = 1; /* local device (0) is always there */
} /* initServer */

ServerCtxt* 
server_make( MPFORMAL ModelCtxt* model, CommsCtxt* comms, XW_UtilCtxt* util )
{
    ServerCtxt* result = (ServerCtxt*)XP_MALLOC( mpool, sizeof(*result) );

    if ( result != NULL ) {
        XP_MEMSET( result, 0, sizeof(*result) );

        MPASSIGN(result->mpool, mpool);

        result->vol.model = model;
        result->vol.comms = comms;
        result->vol.util = util;
        result->vol.gi = util->gameInfo;

        initServer( result );
    }
    return result;
} /* server_make */

static void
getNV( XWStreamCtxt* stream, ServerNonvolatiles* nv, XP_U16 nPlayers )
{
    XP_U16 i;

    /* This should go away when stream format changes */
    (void)stream_getBits( stream, 3 ); /* was npassesinrow */

    nv->nDevices = (XP_U8)stream_getBits( stream, NDEVICES_NBITS );
    if ( stream_getVersion( stream ) > STREAM_VERS_41B4 ) {
        ++nv->nDevices;
    }

    XP_ASSERT( XWSTATE_GAMEOVER < 1<<4 );
    nv->gameState = (XW_State)stream_getBits( stream, 4 );

    nv->currentTurn = (XP_S8)stream_getBits( stream, NPLAYERS_NBITS ) - 1;
    nv->pendingRegistrations = (XP_U8)stream_getBits( stream, NPLAYERS_NBITS );

    for ( i = 0; i < nPlayers; ++i ) {
        nv->addresses[i].channelNo = (XP_PlayerAddr)stream_getBits( stream,
                                                                    16 );
    }
} /* getNV */

static void
putNV( XWStreamCtxt* stream, ServerNonvolatiles* nv, XP_U16 nPlayers )
{
    XP_U16 i;

    stream_putBits( stream, 3, 0 ); /* was nPassesInRow */
    /* number of players is upper limit on device count */
    stream_putBits( stream, NDEVICES_NBITS, nv->nDevices-1 );

    XP_ASSERT( XWSTATE_GAMEOVER < 1<<4 );
    stream_putBits( stream, 4, nv->gameState );

    /* +1: make -1 (NOTURN) into a positive number */
    stream_putBits( stream, NPLAYERS_NBITS, nv->currentTurn+1 );
    stream_putBits( stream, NPLAYERS_NBITS, nv->pendingRegistrations );

    for ( i = 0; i < nPlayers; ++i ) {
        stream_putBits( stream, 16, nv->addresses[i].channelNo );
    }
} /* putNV */

ServerCtxt*
server_makeFromStream( MPFORMAL XWStreamCtxt* stream, ModelCtxt* model, 
                       CommsCtxt* comms, XW_UtilCtxt* util, XP_U16 nPlayers )
{
    ServerCtxt* server;
    short i;
    CurGameInfo* gi = util->gameInfo;

    server = server_make( MPPARM(mpool) model, comms, util );
    getNV( stream, &server->nv, nPlayers );
    
    if ( stream_getBits(stream, 1) != 0 ) {
        server->pool = pool_makeFromStream( MPPARM(mpool) stream );
    }

    for ( i = 0; i < nPlayers; ++i ) {
        ServerPlayer* player = &server->players[i];

        player->deviceIndex = stream_getU8( stream );

        if ( stream_getU8( stream ) != 0 ) {
            LocalPlayer* lp = &gi->players[i];
            player->engine = engine_makeFromStream( MPPARM(mpool)
                                                    stream, util, 
                                                    lp->isRobot );
        }
    }

    if ( STREAM_VERS_ALWAYS_MULTI <= stream_getVersion(stream)
#ifndef PREV_WAS_STANDALONE_ONLY
         || XP_TRUE
#endif
         ) { 
        server->lastMoveSource = (XP_U16)stream_getBits( stream, 2 );
    }

    XP_ASSERT( stream_getU32( stream ) == sEND );

    return server;
} /* server_makeFromStream */

void
server_writeToStream( ServerCtxt* server, XWStreamCtxt* stream )
{
    XP_U16 i;
    XP_U16 nPlayers = server->vol.gi->nPlayers;

    putNV( stream, &server->nv, nPlayers );

    stream_putBits( stream, 1, server->pool != NULL );
    if ( server->pool != NULL ) {
        pool_writeToStream( server->pool, stream );
    }

    for ( i = 0; i < nPlayers; ++i ) {
        ServerPlayer* player = &server->players[i];

        stream_putU8( stream, player->deviceIndex );

        stream_putU8( stream, (XP_U8)(player->engine != NULL) );
        if ( player->engine != NULL ) {
            engine_writeToStream( player->engine, stream );
        }
    }

#ifndef XWFEATURE_STANDALONE_ONLY
    stream_putBits( stream, 2, server->lastMoveSource );
#else
    stream_putBits( stream, 2, 0 );
#endif

#ifdef DEBUG
    stream_putU32( stream, sEND );
#endif

} /* server_writeToStream */

static void
cleanupServer( ServerCtxt* server )
{
    XP_U16 i;
    for ( i = 0; i < VSIZE(server->players); ++i ){
        ServerPlayer* player = &server->players[i];
        if ( player->engine != NULL ) {
            engine_destroy( player->engine );
        }
    }
    XP_MEMSET( server->players, 0, sizeof(server->players) );

    if ( server->pool != NULL ) {
        pool_destroy( server->pool );
        server->pool = (PoolContext*)NULL;
    }

    XP_MEMSET( &server->nv, 0, sizeof(server->nv) );

    if ( !!server->vol.prevMoveStream ) {
        stream_destroy( server->vol.prevMoveStream );
        server->vol.prevMoveStream = NULL;
    }
} /* cleanupServer */

void
server_reset( ServerCtxt* server, CommsCtxt* comms )
{
    ServerVolatiles vol = server->vol;

    cleanupServer( server );

    vol.comms = comms;
    server->vol = vol;

    initServer( server );
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
server_prefsChanged( ServerCtxt* server, CommonPrefs* cp )
{
    server->nv.showRobotScores = cp->showRobotScores;
#ifdef XWFEATURE_SLOW_ROBOT
    server->nv.robotThinkMin = cp->robotThinkMin;
    server->nv.robotThinkMax = cp->robotThinkMax;
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
void
server_initClientConnection( ServerCtxt* server, XWStreamCtxt* stream )
{
    CurGameInfo* gi = server->vol.gi;
    XP_U16 nPlayers;
    LocalPlayer* lp;
#ifdef DEBUG
    XP_U16 i = 0;
#endif

    XP_ASSERT( gi->serverRole == SERVER_ISCLIENT );
    XP_ASSERT( stream != NULL );
    XP_ASSERT( server->nv.gameState == XWSTATE_NONE );

    stream_open( stream );

    stream_putBits( stream, XWPROTO_NBITS, XWPROTO_DEVICE_REGISTRATION );

    nPlayers = gi->nPlayers;
    XP_ASSERT( nPlayers > 0 );
    stream_putBits( stream, NPLAYERS_NBITS, nPlayers );

    for ( lp = gi->players; nPlayers-- > 0; ++lp ) {
        XP_UCHAR* name;
        XP_U8 len;

        XP_ASSERT( i++ < MAX_NUM_PLAYERS );

        stream_putBits( stream, 1, lp->isRobot ); /* better not to send this */

        /* The first nPlayers players are the ones we'll use.  The local flag
           doesn't matter when for SERVER_ISCLIENT. */
        name = emptyStringIfNull(lp->name);
        len = XP_STRLEN(name);
        if ( len > MAX_NAME_LEN ) {
            len = MAX_NAME_LEN;
        }
        stream_putBits( stream, NAME_LEN_NBITS, len );
        stream_putBytes( stream, name, len );
    }

    stream_destroy( stream );
} /* server_initClientConnection */
#endif

static void
callTurnChangeListener( ServerCtxt* server )
{
    if ( server->vol.turnChangeListener != NULL ) {
        (*server->vol.turnChangeListener)( server->vol.turnChangeData );
    }
} /* callTurnChangeListener */

#ifndef XWFEATURE_STANDALONE_ONLY
static XP_Bool
handleRegistrationMsg( ServerCtxt* server, XWStreamCtxt* stream )
{
    XP_Bool success = XP_TRUE;
    XP_U16 playersInMsg, i;
    LOG_FUNC();

    /* code will have already been consumed */
    playersInMsg = (XP_U16)stream_getBits( stream, NPLAYERS_NBITS );
    XP_ASSERT( playersInMsg > 0 );

    if ( server->nv.pendingRegistrations < playersInMsg ) {
        util_userError( server->vol.util, ERR_REG_UNEXPECTED_USER );
        success = XP_FALSE;
    } else {
        for ( i = 0; i < playersInMsg; ++i ) {
            registerRemotePlayer( server, stream );

            /* This is abusing the semantics of turn change -- at least in the
               case where there is another device yet to register -- but we
               need to let the board know to redraw the scoreboard with more
               players there. */
            callTurnChangeListener( server );
        }

        if ( server->nv.pendingRegistrations == 0 ) {
            assignTilesToAll( server );
            SETSTATE( server, XWSTATE_RECEIVED_ALL_REG );
        }
    }
    return success;
} /* handleRegistrationMsg */
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

    for ( i = 0; i < numInTray; ++i ) { /* for each tile in tray */
        XP_Bool keep = XP_FALSE;
        for ( j = 0; j < newMove->numTiles; ++j ) { /* for each in move */
            Tile movedTile = newMove->tiles[j].tile;
            if ( newMove->tiles[j].isBlank ) {
                movedTile |= TILE_BLANK_BIT;
            }
            if ( movedTile == curTiles[i] ) { /* it's in the move */
                keep = XP_TRUE;
                break;
            }
        }
        if ( !keep ) {
            tradeTiles[numToTrade++] = curTiles[i];
        }
    }

    
} /* robotTradeTiles */
#endif

#define FUDGE_RANGE 10
#define MINIMUM_SCORE 5
static XP_U16
figureTargetScore( ServerCtxt* server, XP_U16 turn )
{
    XP_S16 result = 1000;
    XP_S16 highScore = 0;
    ModelCtxt* model = server->vol.model;
    XP_U16 nPlayers = server->vol.gi->nPlayers;
    XP_U16 i;

    XP_ASSERT( IS_ROBOT(&server->vol.gi->players[turn]) );

    if ( 1 /* server->nHumanPlayers > 0 */ ) {
        result = 0;

        /* find the highest score anybody but the current player has */
        for ( i = 0; i < nPlayers; ++i ) {
            if ( i != turn ) {
                XP_S16 score = model_getPlayerScore( model, i );
                XP_ASSERT( score >= 0 );
                if ( highScore < score ) {
                    highScore = score;
                }
            }
        }

        result = (XP_S16)(highScore - model_getPlayerScore( model, turn )
            + (FUDGE_RANGE-(XP_RANDOM() % (FUDGE_RANGE*2))));
        if ( result < 0 ) {
            result = MINIMUM_SCORE;
        }
    }
    
    return result;
} /* figureTargetScore */

static XWStreamCtxt*
mkServerStream( ServerCtxt* server )
{
    XWStreamCtxt* stream;
    stream = mem_stream_make( MPPARM(server->mpool) 
                              util_getVTManager(server->vol.util), 
                              NULL, CHANNEL_NONE, 
                              (MemStreamCloseCallback)NULL );
    XP_ASSERT( !!stream );
    return stream;
} /* mkServerStream */

static XP_Bool
makeRobotMove( ServerCtxt* server )
{
    XP_Bool result = XP_FALSE;
    XP_Bool finished;
    XP_S16 turn;
    const TrayTileSet* tileSet;
    MoveInfo newMove;
    ModelCtxt* model = server->vol.model;
    CurGameInfo* gi = server->vol.gi;
    XP_Bool timerEnabled = gi->timerEnabled;
    XP_Bool canMove;
    XP_U32 time = 0L; /* stupid compiler.... */
    XP_U16 targetScore = NO_SCORE_LIMIT;
    XW_UtilCtxt* util = server->vol.util;
    
    if ( timerEnabled ) {
        time = util_getCurSeconds( util );
    }

    turn = server->nv.currentTurn;
    XP_ASSERT( turn >= 0 );

    /* If the player's been recently turned into a robot while he had some
       pending tiles on the board we'll have problems.  It'd be best to detect
       this and put 'em back when that happens.  But for now we'll just be
       paranoid.  PENDING(ehouse) */
    model_resetCurrentTurn( model, turn );

    tileSet = model_getPlayerTiles( model, turn );

    if ( gi->robotSmartness == DUMB_ROBOT ) {
        targetScore = figureTargetScore( server, turn );
    }

    XP_ASSERT( !!server_getEngineFor( server, turn ) );
    finished = engine_findMove( server_getEngineFor( server, turn ),
                                model, model_getDictionary( model ), 
                                tileSet->tiles, tileSet->nTiles,
#ifdef XWFEATURE_SEARCHLIMIT
                                NULL, XP_FALSE,
#endif
                                targetScore, &canMove, &newMove );
    if ( finished ) {
        const XP_UCHAR* str;
        XWStreamCtxt* stream = NULL;

        XP_Bool trade = (newMove.nTiles == 0) && canMove &&
                         (server_countTilesInPool( server ) >= MAX_TRAY_TILES);

        server->vol.showPrevMove = XP_TRUE;
        if ( server->nv.showRobotScores ) {
            stream = mkServerStream( server );
        }

        /* trade if unable to find a move */
        if ( trade ) {
            result = server_commitTrade( server, ALLTILES );

            /* Quick hack to fix gremlin bug where all-robot game seen none
               able to trade for tiles to move and blowing the undo stack.
               This will stop them, and should have no effect if there are any
               human players making real moves. */

            if ( !!stream ) {
                XP_UCHAR buf[64];
                str = util_getUserString(util, STRD_ROBOT_TRADED);
                XP_SNPRINTF( buf, sizeof(buf), str, MAX_TRAY_TILES );

                stream_catString( stream, buf );
                XP_ASSERT( !server->vol.prevMoveStream );
                server->vol.prevMoveStream = stream;
            }
        } else { 
            /* if canMove is false, this is a fake move, a pass */

            if ( canMove || NPASSES_OK(server) ) {
                model_makeTurnFromMoveInfo( model, turn, &newMove );

                if ( !!stream ) {
                    (void)model_checkMoveLegal( model, turn, stream, NULL );
                    XP_ASSERT( !server->vol.prevMoveStream );
                    server->vol.prevMoveStream = stream;
                }
                result = server_commitMove( server );
            } else {
                result = XP_FALSE;
            }
        }
    }

    if ( timerEnabled ) {
        gi->players[turn].secondsUsed += 
            (XP_U16)(util_getCurSeconds( util ) - time);
    } else {
        XP_ASSERT( gi->players[turn].secondsUsed == 0 );
    }

    return result; /* always return TRUE after robot move? */
} /* makeRobotMove */

#ifdef XWFEATURE_SLOW_ROBOT
static XP_Bool 
wakeRobotProc( void* closure, XWTimerReason XP_UNUSED_DBG(why) )
{
    XP_ASSERT( TIMER_SLOWROBOT == why );
    ServerCtxt* server = (ServerCtxt*)closure;
    XP_ASSERT( ROBOTWAITING(server) );
    server->robotWaiting = XP_FALSE;
    util_requestTime( server->vol.util );
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
        result = IS_ROBOT(player) && IS_LOCAL(player);
    }
    return result;
} /* robotMovePending */

#ifdef XWFEATURE_SLOW_ROBOT
static XP_Bool
postponeRobotMove( ServerCtxt* server )
{
    XP_Bool result = XP_FALSE;
    XP_ASSERT( robotMovePending(server) );

    if ( !ROBOTWAITING(server) ) {
        XP_U16 sleepTime = figureSleepTime(server);
        if ( 0 != sleepTime ) {
            server->robotWaiting = XP_TRUE;
            util_setTimer( server->vol.util, TIMER_SLOWROBOT, sleepTime,
                           wakeRobotProc, server );
            result = XP_TRUE;
        }
    }
    return result;
}
# define POSTPONEROBOTMOVE(s) postponeRobotMove(s)
#else
# define POSTPONEROBOTMOVE(s) XP_FALSE
#endif

static void
showPrevScore( ServerCtxt* server )
{
    XW_UtilCtxt* util = server->vol.util;
    XWStreamCtxt* stream;
    const XP_UCHAR* str;
    CurGameInfo* gi = server->vol.gi;
    XP_U16 nPlayers = gi->nPlayers;
    XP_U16 prevTurn;
    XP_U16 strCode;
    LocalPlayer* lp;
    XP_Bool wasRobot;
    XP_Bool wasLocal;

    XP_ASSERT( server->nv.showRobotScores );

    prevTurn = (server->nv.currentTurn + nPlayers - 1) % nPlayers;
    lp = &gi->players[prevTurn];
    wasRobot = lp->isRobot;
    wasLocal = lp->isLocal;

    if ( wasLocal ) {
        XP_ASSERT( wasRobot );
        strCode = STR_ROBOT_MOVED;
    } else {
        strCode = STR_REMOTE_MOVED;
    }

    stream = mkServerStream( server );

    str = util_getUserString( util, strCode );
    stream_catString( stream, str );

    if ( !!server->vol.prevMoveStream ) {
        XWStreamCtxt* prevStream = server->vol.prevMoveStream;
        XP_U16 len = stream_getSize( prevStream );
        XP_UCHAR* buf = XP_MALLOC( server->mpool, len );

        stream_getBytes( prevStream, buf, len );
        stream_destroy( prevStream );
        server->vol.prevMoveStream = NULL;

        stream_putBytes( stream, buf, len );
        XP_FREE( server->mpool, buf );
    }

    (void)util_userQuery( util, QUERY_ROBOT_MOVE, stream );
    stream_destroy( stream );

    SETSTATE( server, server->vol.stateAfterShow );
} /* showPrevScore */

XP_Bool
server_do( ServerCtxt* server )
{
    XP_Bool result = XP_TRUE;
    XP_Bool moreToDo = XP_FALSE;

    if ( server->serverDoing ) {
        return XP_FALSE;
    }
    server->serverDoing = XP_TRUE;

    switch( server->nv.gameState ) {
    case XWSTATE_BEGIN:
        if ( server->nv.pendingRegistrations == 0 ) { /* all players on device */
            assignTilesToAll( server );
            SETSTATE( server, XWSTATE_INTURN );
            setTurn( server, 0 );
            moreToDo = XP_TRUE;
        }
        break;

    case XWSTATE_NEEDSEND_BADWORD_INFO:
        XP_ASSERT( server->vol.gi->serverRole == SERVER_ISSERVER );
        badWordMoveUndoAndTellUser( server, &server->illegalWordInfo );
#ifndef XWFEATURE_STANDALONE_ONLY
        sendBadWordMsgs( server );
#endif
        nextTurn( server, PICK_NEXT ); /* sets server->nv.gameState */
        //moreToDo = XP_TRUE;   /* why? */
        break;

#ifndef XWFEATURE_STANDALONE_ONLY
    case XWSTATE_RECEIVED_ALL_REG:
        server_sendInitialMessage( server ); 
        /* PENDING isn't INTURN_OFFDEVICE possible too?  Or just INTURN?  */
        SETSTATE( server, XWSTATE_INTURN );
        setTurn( server, 0 );
        moreToDo = XP_TRUE;
        break;

    case XWSTATE_MOVE_CONFIRM_MUSTSEND:
        XP_ASSERT( server->vol.gi->serverRole == SERVER_ISSERVER );
        tellMoveWasLegal( server );
        nextTurn( server, PICK_NEXT );
        break;

#endif /* XWFEATURE_STANDALONE_ONLY */

    case XWSTATE_NEEDSEND_ENDGAME:
        endGameInternal( server, END_REASON_OUT_OF_TILES );
        break;

    case XWSTATE_NEED_SHOWSCORE:
        showPrevScore( server );
        moreToDo = XP_TRUE;     /* either process turn or end game... */
        break;
    case XWSTATE_INTURN:
        if ( robotMovePending( server ) && !ROBOTWAITING(server) ) {
            result = makeRobotMove( server );
            /* if robot was interrupted, we need to schedule again */
            moreToDo = !result || 
                (robotMovePending( server ) && !POSTPONEROBOTMOVE(server));
        }
        break;

    default:
        result = XP_FALSE;
        break;
    }
    if ( moreToDo ) {
        util_requestTime( server->vol.util );
    }

    server->serverDoing = XP_FALSE;
    return result;
} /* server_do */

#ifndef XWFEATURE_STANDALONE_ONLY
static XP_S8
getIndexForDevice( ServerCtxt* server, XP_PlayerAddr channelNo )
{
    short i;
    XP_S8 result = -1;

    for ( i = 0; i < server->nv.nDevices; ++i ) {
        RemoteAddress* addr = &server->nv.addresses[i];
        if ( addr->channelNo == channelNo ) {
            result = i;
            break;
        }
    }

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
    XP_ASSERT( lp >= gi->players ); /* did we find a slot? */
    *playerP = server->players + nPlayers;
    return lp;
} /* findFirstPending */

static void
registerRemotePlayer( ServerCtxt* server, XWStreamCtxt* stream )
{
    XP_S8 deviceIndex;
    XP_PlayerAddr channelNo;
    XP_UCHAR* name;
    XP_U16 nameLen;
    LocalPlayer* lp;
    ServerPlayer* player = (ServerPlayer*)NULL;

    /* The player must already be there with a null name, or it's an error.
       Take the first empty slot. */
    XP_ASSERT( server->nv.pendingRegistrations > 0 );

    /* find the slot to use */
    lp = findFirstPending( server, &player );

    /* get data from stream */
    lp->isRobot = stream_getBits( stream, 1 );
    nameLen = stream_getBits( stream, NAME_LEN_NBITS );
    name = (XP_UCHAR*)XP_MALLOC( server->mpool, nameLen + 1 );
    stream_getBytes( stream, name, nameLen );
    name[nameLen] = '\0';

    replaceStringIfDifferent( server->mpool, &lp->name, name );
    XP_FREE( server->mpool, name );

    channelNo = stream_getAddress( stream );
    deviceIndex = getIndexForDevice( server, channelNo );

    --server->nv.pendingRegistrations;

    if ( deviceIndex == -1 ) {
        RemoteAddress* addr; 
        addr = &server->nv.addresses[server->nv.nDevices];

        deviceIndex = server->nv.nDevices++;

        XP_ASSERT( channelNo != 0 );
        addr->channelNo = channelNo;
    }

    player->deviceIndex = deviceIndex;

} /* registerRemotePlayer */

static void
clearLocalRobots( ServerCtxt* server )
{
    XP_U16 i;
    CurGameInfo* gi = server->vol.gi;
    XP_U16 nPlayers = gi->nPlayers;

    for ( i = 0; i < nPlayers; ++i ) {
        LocalPlayer* player = &gi->players[i];
        if ( IS_LOCAL( player ) ) {
            player->isRobot = XP_FALSE;
        }
    }
} /* clearLocalRobots */
#endif

/* Called in response to message from server listing all the names of
 * players in the game (in server-assigned order) and their initial
 * tray contents.
 */
#ifndef XWFEATURE_STANDALONE_ONLY
static XP_Bool
client_readInitialMessage( ServerCtxt* server, XWStreamCtxt* stream )
{
    XP_Bool accepted = 0 == server->nv.addresses[0].channelNo;

    /* We should never get this message a second time, but very rarely we do.
       Drop it in that case. */
    if ( accepted ) {
        DictionaryCtxt* newDict;
        DictionaryCtxt* curDict;
        XP_U16 nPlayers, nCols;
        XP_PlayerAddr channelNo;
        short i;
        ModelCtxt* model = server->vol.model;
        CurGameInfo* gi = server->vol.gi;
        CurGameInfo localGI;
        XP_U32 gameID;
        PoolContext* pool;

        /* version */
        XP_U8 streamVersion = stream_getU8( stream );
        XP_ASSERT( streamVersion == STREAM_VERS_41B4
                   || streamVersion == STREAM_VERS_UTF8);
        if ( (streamVersion != STREAM_VERS_41B4)
            && (streamVersion != STREAM_VERS_UTF8) ) {
            return XP_FALSE;
        }
        stream_setVersion( stream, streamVersion );

        gameID = stream_getU32( stream );
        XP_STATUSF( "read gameID of %lx; calling comms_setConnID", gameID );
        server->vol.gi->gameID = gameID;
        comms_setConnID( server->vol.comms, gameID );

        XP_MEMSET( &localGI, 0, sizeof(localGI) );
        gi_readFromStream( MPPARM(server->mpool) stream, &localGI );
        localGI.serverRole = SERVER_ISCLIENT;

        /* so it's not lost (HACK!).  Without this, a client won't have a default
           dict name when a new game is started. */
        localGI.dictName = copyString( server->mpool, gi->dictName );
        gi_copy( MPPARM(server->mpool) gi, &localGI );

        nCols = localGI.boardSize;

        newDict = util_makeEmptyDict( server->vol.util );
        dict_loadFromStream( newDict, stream );

        channelNo = stream_getAddress( stream );
        XP_ASSERT( channelNo != 0 );
        server->nv.addresses[0].channelNo = channelNo;

        /* PENDING init's a bit harsh for setting the size */
        model_init( model, nCols, nCols );

        nPlayers = localGI.nPlayers;
        XP_STATUSF( "reading in %d players", localGI.nPlayers );

        gi_disposePlayerInfo( MPPARM(server->mpool) &localGI );

        gi->nPlayers = nPlayers;
        model_setNPlayers( model, nPlayers );

        curDict = model_getDictionary( model );

        XP_ASSERT( !!newDict );

        if ( curDict == NULL ) {
            model_setDictionary( model, newDict );
        } else if ( dict_tilesAreSame( newDict, curDict ) ) {
            /* keep the dict the local user installed */
            dict_destroy( newDict );
        } else {
            dict_destroy( curDict );
            model_setDictionary( model, newDict );
            util_userError( server->vol.util, ERR_SERVER_DICT_WINS );
            clearLocalRobots( server );
        }

        XP_ASSERT( !server->pool );
        pool = server->pool = pool_make( MPPARM_NOCOMMA(server->mpool) );
        pool_initFromDict( server->pool, model_getDictionary(model));

        /* now read the assigned tiles for each player from the stream, and
           remove them from the newly-created local pool. */
        for ( i = 0; i < nPlayers; ++i ) {
            TrayTileSet tiles;

            traySetFromStream( stream, &tiles );
            XP_ASSERT( tiles.nTiles <= MAX_TRAY_TILES );

            XP_STATUSF( "got %d tiles for player %d", tiles.nTiles, i );

            model_assignPlayerTiles( model, i, &tiles );

            /* remove what the server's assigned so we won't conflict
               later. */
            pool_removeTiles( pool, &tiles );
        }

        SETSTATE( server, XWSTATE_INTURN );

        /* Give board a chance to redraw self with the full compliment of known
           players */
        setTurn( server, 0 );
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
    XP_U16 i;
    XP_MEMCPY( giCopy, server->vol.gi, sizeof(*giCopy) );

    nPlayers = giCopy->nPlayers;

    for ( clientPl = giCopy->players, i = 0;
          i < nPlayers; ++clientPl, ++i ) {
        /* adjust isLocal to client's perspective */
        clientPl->isLocal = server->players[i].deviceIndex == deviceIndex;
    }

    giCopy->dictName = (XP_UCHAR*)NULL; /* so we don't sent the bytes; Isn't this
                                           a leak? PENDING */
} /* makeSendableGICopy */

static void
server_sendInitialMessage( ServerCtxt* server )
{
    short i;
    XP_U16 deviceIndex;
    ModelCtxt* model = server->vol.model;
    XP_U16 nPlayers = server->vol.gi->nPlayers;
    CurGameInfo localGI;
    XP_U32 gameID = server->vol.gi->gameID;

    XP_STATUSF( "server_sendInitialMessage" );

    for ( deviceIndex = 1; deviceIndex < server->nv.nDevices;
          ++deviceIndex ) {
        RemoteAddress* addr = &server->nv.addresses[deviceIndex];
        XWStreamCtxt* stream = util_makeStreamFromAddr( server->vol.util, 
                                                        addr->channelNo );
        DictionaryCtxt* dict = model_getDictionary(model);
        XP_ASSERT( !!stream );
        stream_open( stream );
        stream_putBits( stream, XWPROTO_NBITS, XWPROTO_CLIENT_SETUP );

        /* write version for server's benefit; use old version until format
           changes */
        stream_putU8( stream, dict_isUTF8(dict)? 
                      STREAM_VERS_UTF8:STREAM_VERS_41B4 );

        XP_STATUSF( "putting gameID %lx into msg", gameID );
        stream_putU32( stream, gameID );

        makeSendableGICopy( server, &localGI, deviceIndex );
        gi_writeToStream( stream, &localGI );

        dict_writeToStream( dict, stream );

        /* send tiles currently in tray */
        for ( i = 0; i < nPlayers; ++i ) {
            model_trayToStream( model, i, stream );
        }

        stream_destroy( stream );
    }

    /* Set after messages are built so their connID will be 0, but all
       non-initial messages will have a non-0 connID. */
    comms_setConnID( server->vol.comms, gameID );
} /* server_sendInitialMessage */
#endif

static void
freeBWI( MPFORMAL BadWordInfo* bwi )
{
    /*     BadWordInfo* bwi = &server->illegalWordInfo; */
    XP_U16 nWords = bwi->nWords;

    while ( nWords-- ) {
        XP_FREE( mpool, bwi->words[nWords] );
        bwi->words[nWords] = (XP_UCHAR*)NULL;
    }

    bwi->nWords = 0;
} /* freeBWI */

#ifndef XWFEATURE_STANDALONE_ONLY
static void
bwiToStream( XWStreamCtxt* stream, BadWordInfo* bwi )
{
    XP_U16 nWords = bwi->nWords;
    XP_UCHAR** sp;

    stream_putBits( stream, 4, nWords );
    
    for ( sp = bwi->words; nWords > 0; --nWords, ++sp ) {
        stringToStream( stream, *sp );
    }

} /* bwiToStream */

static void
bwiFromStream( MPFORMAL XWStreamCtxt* stream, BadWordInfo* bwi )
{
    XP_U16 nWords = stream_getBits( stream, 4 );
    XP_UCHAR** sp = bwi->words;

    bwi->nWords = nWords;
    for ( sp = bwi->words; nWords; ++sp, --nWords ) {
        *sp = stringFromStream( mpool, stream );
    }
} /* bwiFromStream */

#ifdef DEBUG
#define caseStr(var, s) case s: var = #s; break;
static void
printCode(char* intro, XW_Proto code)
{
    char* str = (char*)NULL;

    switch( code ) {
        caseStr( str, XWPROTO_ERROR );
        caseStr( str, XWPROTO_CHAT );
        caseStr( str, XWPROTO_DEVICE_REGISTRATION );
        caseStr( str, XWPROTO_CLIENT_SETUP );
        caseStr( str, XWPROTO_MOVEMADE_INFO_CLIENT );
        caseStr( str, XWPROTO_MOVEMADE_INFO_SERVER );
        caseStr( str, XWPROTO_UNDO_INFO_CLIENT );
        caseStr( str, XWPROTO_UNDO_INFO_SERVER );
        caseStr( str, XWPROTO_BADWORD_INFO );
        caseStr( str, XWPROTO_MOVE_CONFIRM );
        caseStr( str, XWPROTO_CLIENT_REQ_END_GAME );
        caseStr( str, XWPROTO_END_GAME );
    }

    XP_STATUSF( "\t%s for %s", intro, str );
} /* printCode */
#undef caseStr
#else
#define printCode(intro, code)
#endif

static XWStreamCtxt*
messageStreamWithHeader( ServerCtxt* server, XP_U16 devIndex, XW_Proto code )
{
    XWStreamCtxt* stream;
    XP_PlayerAddr channelNo = server->nv.addresses[devIndex].channelNo;

    printCode("making", code);

    stream = util_makeStreamFromAddr( server->vol.util, channelNo );

    stream_open( stream );
    stream_putBits( stream, XWPROTO_NBITS, code );

    return stream;
} /* messageStreamWithHeader */

/* Check that the message belongs to this game, whatever.  Pull out the data
 * put in by messageStreamWithHeader -- except for the code, which will have
 * already come out.
 */
static XP_Bool
readStreamHeader( ServerCtxt* XP_UNUSED(server), 
                  XWStreamCtxt* XP_UNUSED(stream) )
{
    return XP_TRUE;
} /* readStreamHeader */

static void
sendBadWordMsgs( ServerCtxt* server )
{
    XWStreamCtxt* stream;

    stream = messageStreamWithHeader( server, server->lastMoveSource, 
                                      XWPROTO_BADWORD_INFO );
    stream_putBits( stream, PLAYERNUM_NBITS, server->nv.currentTurn );

    XP_ASSERT( server->illegalWordInfo.nWords > 0 );
    bwiToStream( stream, &server->illegalWordInfo );

    stream_destroy( stream );

    freeBWI( MPPARM(server->mpool) &server->illegalWordInfo );
} /* sendBadWordMsgs */
#endif

static void
badWordMoveUndoAndTellUser( ServerCtxt* server, BadWordInfo* bwi )
{
    XP_U16 turn;
    ModelCtxt* model = server->vol.model;
    /* I'm the server.  I need to send a message to everybody else telling
       them the move's rejected.  Then undo it on this side, replacing it with
       model_commitRejectedPhony(); */

    model_rejectPreviousMove( model, server->pool, &turn );

    util_warnIllegalWord( server->vol.util, bwi, turn, XP_TRUE );
} /* badWordMoveUndoAndTellUser */

EngineCtxt*
server_getEngineFor( ServerCtxt* server, XP_U16 playerNum )
{
    ServerPlayer* player;
    EngineCtxt* engine;

    XP_ASSERT( playerNum < server->vol.gi->nPlayers );

    player = &server->players[playerNum];
    engine = player->engine;
    if ( !engine && server->vol.gi->players[playerNum].isLocal ) {
        engine = engine_make( MPPARM(server->mpool)
                              server->vol.util, 
                              server->vol.gi->players[playerNum].isRobot );
        player->engine = engine;
    }

    return engine;
} /* server_getEngineFor */

void
server_resetEngine( ServerCtxt* server, XP_U16 playerNum )
{
    ServerPlayer* player = &server->players[playerNum];
    if ( !!player->engine ) {
        XP_ASSERT( player->deviceIndex == 0 );
        engine_reset( player->engine );
    }
} /* server_resetEngine */

static void
resetEngines( ServerCtxt* server )
{
    XP_U16 i;
    XP_U16 nPlayers = server->vol.gi->nPlayers;

    for ( i = 0; i < nPlayers; ++i ) {
        server_resetEngine( server, i );
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
    DictionaryCtxt* dict = model_getDictionary( server->vol.model );
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
    DictionaryCtxt* dict = model_getDictionary( server->vol.model );
    XP_U16 i, j;
    XP_U16 size = tileSet->nTiles;
    const Tile* tp = tileSet->tiles;
    XP_U16 tradedTiles[MAX_TRAY_TILES];
    XP_U16 nNotInTray = 0;
    XP_U16 nUsed = 0;

    XP_MEMSET( tradedTiles, 0xFF, sizeof(tradedTiles) );
    if ( !!notInTray ) {
        const Tile* tp = notInTray->tiles;
        nNotInTray = notInTray->nTiles;
        for ( i = 0; i < nNotInTray; ++i ) {
            tradedTiles[i] = *tp++;
        }
    }

    for ( i = 0; i < size; ++i ) {
        Tile tile = *tp++;
        XP_Bool toBeTraded = XP_FALSE;

        for ( j = 0; j < nNotInTray; ++j ) {
            if ( tradedTiles[j] == tile ) {
                tradedTiles[j] = 0xFFFF;
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

/* Get tiles for one user.  If picking is available, let user pick until
 * cancels.  Otherwise, and after cancel, pick for 'im.
 */
static void
fetchTiles( ServerCtxt* server, XP_U16 playerNum, XP_U16 nToFetch, 
            const TrayTileSet* tradedTiles, TrayTileSet* resultTiles )
{
    XP_Bool ask;
    XP_U16 nSoFar = 0;
    XP_U16 nLeft;
    PoolContext* pool = server->pool;
    TrayTileSet oneTile;
    PickInfo pi;
    const XP_UCHAR* curTray[MAX_TRAY_TILES];
#ifdef FEATURE_TRAY_EDIT
    DictionaryCtxt* dict = model_getDictionary( server->vol.model );
#endif

    XP_ASSERT( !!pool );
#ifdef FEATURE_TRAY_EDIT
    ask = server->vol.gi->allowPickTiles
        && !server->vol.gi->players[playerNum].isRobot;
#else
    ask = XP_FALSE;
#endif
    
    nLeft = pool_getNTilesLeft( pool );
    if ( nLeft < nToFetch ) {
        nToFetch = nLeft;
    }

    oneTile.nTiles = 1;

    pi.why = PICK_FOR_CHEAT;
    pi.nTotal = nToFetch;
    pi.thisPick = 0;
    pi.curTiles = curTray;

    curTrayAsTexts( server, playerNum, tradedTiles, &pi.nCurTiles, curTray );

#ifdef FEATURE_TRAY_EDIT        /* good compiler would note ask==0, but... */
    /* First ask until cancelled */
    for ( nSoFar = 0; ask && nSoFar < nToFetch;  ) {
        const XP_UCHAR* texts[MAX_UNIQUE_TILES];
        Tile tiles[MAX_UNIQUE_TILES];
        XP_S16 chosen;
        XP_U16 nUsed = MAX_UNIQUE_TILES;

        model_packTilesUtil( server->vol.model, pool,
                             XP_TRUE, &nUsed, texts, tiles );

        chosen = util_userPickTile( server->vol.util, &pi, playerNum,
                                    texts, nUsed );

        if ( chosen == PICKER_PICKALL ) {
            ask = XP_FALSE;
        } else if ( chosen == PICKER_BACKUP ) {
            if ( nSoFar > 0 ) {
                TrayTileSet tiles;
                tiles.nTiles = 1;
                tiles.tiles[0] = resultTiles->tiles[--nSoFar];
                pool_replaceTiles( server->pool, &tiles );
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

    /* Then fetch the rest without asking */
    if ( nSoFar < nToFetch ) {
        XP_U8 nLeft = nToFetch - nSoFar;
        Tile tiles[MAX_TRAY_TILES];

        pool_requestTiles( pool, tiles, &nLeft );

        XP_MEMCPY( &resultTiles->tiles[nSoFar], tiles, 
                   nLeft * sizeof(resultTiles->tiles[0]) );
        nSoFar += nLeft;
    }
   
    XP_ASSERT( nSoFar < 0x00FF );
    resultTiles->nTiles = (XP_U8)nSoFar;
} /* fetchTiles */

static void
assignTilesToAll( ServerCtxt* server )
{
    XP_U16 numAssigned;
    short i;
    ModelCtxt* model = server->vol.model;
    XP_U16 nPlayers = server->vol.gi->nPlayers;

    XP_ASSERT( server->vol.gi->serverRole != SERVER_ISCLIENT );
    XP_ASSERT( model_getDictionary(model) != NULL );
    if ( server->pool == NULL ) {
        server->pool = pool_make( MPPARM_NOCOMMA(server->mpool) );
        XP_STATUSF( "initing pool" );
        pool_initFromDict( server->pool, model_getDictionary(model));
    }

    XP_STATUSF( "assignTilesToAll" );

    model_setNPlayers( model, nPlayers );

    numAssigned = pool_getNTilesLeft( server->pool ) / nPlayers;
    if ( numAssigned > MAX_TRAY_TILES ) {
        numAssigned = MAX_TRAY_TILES;
    }
    for ( i = 0; i < nPlayers; ++i ) {
        TrayTileSet newTiles;
        fetchTiles( server, i, numAssigned, NULL, &newTiles );
        model_assignPlayerTiles( model, i, &newTiles );
    }

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
nextTurn( ServerCtxt* server, XP_S16 nxtTurn )
{
    ServerPlayer* player;
    XP_U16 nPlayers = server->vol.gi->nPlayers;
    XP_U16 playerTilesLeft;
    XP_S16 currentTurn = server->nv.currentTurn;
    XP_Bool moreToDo = XP_FALSE;

    if ( nxtTurn == PICK_NEXT ) {
        XP_ASSERT( currentTurn >= 0 );
        playerTilesLeft = model_getNumTilesTotal(server->vol.model, 
                                                 currentTurn);
        nxtTurn = (currentTurn+1) % nPlayers;
    } else {
        /* We're doing an undo, and so won't bother figuring out who the
           previous turn was or how many tiles he had: it's a sure thing he
           "has" enough to be allowed to take the turn just undone. */
        playerTilesLeft = MAX_TRAY_TILES;
    }
    SETSTATE( server, XWSTATE_INTURN ); /* even if game over, if undoing */

    if ( (playerTilesLeft > 0) && tileCountsOk(server) && NPASSES_OK(server) ){

        player = &server->players[nxtTurn];
        setTurn( server, nxtTurn );

    } else {
        /* I discover that the game should end.  If I'm the client,
           though, should I wait for the server to deduce this and send
           out a message?  I think so.  Yes, but be sure not to compute
           another PASS move.  Just don't do anything! */
        if ( server->vol.gi->serverRole != SERVER_ISCLIENT ) {
            SETSTATE( server, XWSTATE_NEEDSEND_ENDGAME );
            moreToDo = XP_TRUE;
        } else {
            XP_LOGF( "Doing nothing; waiting for server to end game." );
            /* I'm the client. Do ++nothing++. */
        }
    }

    if ( server->vol.showPrevMove ) {
        server->vol.showPrevMove = XP_FALSE;
        if ( server->nv.showRobotScores ) {
            server->vol.stateAfterShow = server->nv.gameState;
            SETSTATE( server, XWSTATE_NEED_SHOWSCORE );
            moreToDo = XP_TRUE;
        }
    }

    /* It's safer, if perhaps not always necessary, to do this here. */
    resetEngines( server );

    XP_ASSERT( server->nv.gameState != XWSTATE_GAMEOVER );
    callTurnChangeListener( server );

    if ( robotMovePending(server) && !POSTPONEROBOTMOVE(server) ) {
        moreToDo = XP_TRUE;
    }

    if ( moreToDo ) {
        util_requestTime( server->vol.util );
    }
} /* nextTurn */

void
server_setTurnChangeListener( ServerCtxt* server, TurnChangeListener tl,
                              void* data )
{
    server->vol.turnChangeListener = tl;
    server->vol.turnChangeData = data;
} /* server_setTurnChangeListener */

void
server_setGameOverListener( ServerCtxt* server, GameOverListener gol,
                            void* data )
{
    server->vol.gameOverListener = gol;
    server->vol.gameOverData = data;
} /* server_setTurnChangeListener */

static XP_Bool
storeBadWords( XP_UCHAR* word, void* closure )
{
    ServerCtxt* server = (ServerCtxt*)closure;

    XP_STATUSF( "storeBadWords called with \"%s\"", word );

    server->illegalWordInfo.words[server->illegalWordInfo.nWords++]
        = copyString( server->mpool, word );

    return XP_TRUE;
} /* storeBadWords */

static XP_Bool
checkMoveAllowed( ServerCtxt* server, XP_U16 playerNum )
{
    CurGameInfo* gi = server->vol.gi;
    XP_ASSERT( server->illegalWordInfo.nWords == 0 );

    if ( gi->phoniesAction == PHONIES_DISALLOW ) {
        WordNotifierInfo info;
        info.proc = storeBadWords;
        info.closure = server;
        (void)model_checkMoveLegal( server->vol.model, playerNum, 
                                    (XWStreamCtxt*)NULL, &info );
    }

    return server->illegalWordInfo.nWords == 0;
} /* checkMoveAllowed */

#ifndef XWFEATURE_STANDALONE_ONLY
static void
sendMoveTo( ServerCtxt* server, XP_U16 devIndex, XP_U16 turn,
            XP_Bool legal, TrayTileSet* newTiles, 
            TrayTileSet* tradedTiles ) /* null if a move, set if a trade */
{
    XWStreamCtxt* stream;
    XP_Bool isTrade = !!tradedTiles;
    CurGameInfo* gi = server->vol.gi;
    XW_Proto code = gi->serverRole == SERVER_ISCLIENT?
        XWPROTO_MOVEMADE_INFO_CLIENT : XWPROTO_MOVEMADE_INFO_SERVER;

    stream = messageStreamWithHeader( server, devIndex, code );

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
            XP_STATUSF("*** wrote secondsUsed for player %d: %d", 
                       turn, gi->players[turn].secondsUsed );
        } else {
            XP_ASSERT( gi->players[turn].secondsUsed == 0 );
        }

        if ( !legal ) {
            XP_ASSERT( server->illegalWordInfo.nWords > 0 );
            bwiToStream( stream, &server->illegalWordInfo );
        }
    }

    stream_destroy( stream );
} /* sendMoveTo */

static void
readMoveInfo( ServerCtxt* server, XWStreamCtxt* stream,
              XP_U16* whoMovedP, XP_Bool* isTradeP,
              TrayTileSet* newTiles, TrayTileSet* tradedTiles, 
              XP_Bool* legalP )
{
    XP_U16 whoMoved = stream_getBits( stream, PLAYERNUM_NBITS );
    XP_Bool legalMove = XP_TRUE;
    XP_Bool isTrade;

    traySetFromStream( stream, newTiles );
    isTrade = stream_getBits( stream, 1 );

    if ( isTrade ) {
        traySetFromStream( stream, tradedTiles );
    } else {
        legalMove = stream_getBits( stream, 1 );
        model_makeTurnFromStream( server->vol.model, whoMoved, stream );

        getPlayerTime( server, stream, whoMoved );
    }

    pool_removeTiles( server->pool, newTiles );

    *whoMovedP = whoMoved;
    *isTradeP = isTrade;
    *legalP = legalMove;
} /* readMoveInfo */

static void
sendMoveToClientsExcept( ServerCtxt* server, XP_U16 whoMoved, XP_Bool legal,
                         TrayTileSet* newTiles, TrayTileSet* tradedTiles,
                         XP_U16 skip )
{
    XP_U16 devIndex;

    for ( devIndex = 1; devIndex < server->nv.nDevices; ++devIndex ) {
        if ( devIndex != skip ) {
            sendMoveTo( server, devIndex, whoMoved, legal, 
                        newTiles, tradedTiles );
        }
    }
} /* sendMoveToClientsExcept */

static XWStreamCtxt*
makeTradeReportIf( ServerCtxt* server, const TrayTileSet* tradedTiles )
{
    XWStreamCtxt* stream = NULL;
    if ( server->nv.showRobotScores ) {
        XP_UCHAR tradeBuf[64];
        const XP_UCHAR* tradeStr = util_getUserString( server->vol.util,
                                                       STRD_ROBOT_TRADED );
        XP_SNPRINTF( tradeBuf, sizeof(tradeBuf), tradeStr, 
                     tradedTiles->nTiles );
        stream = mkServerStream( server );
        stream_catString( stream, tradeBuf );
    }
    return stream;
} /* makeTradeReportIf */

static XWStreamCtxt*
makeMoveReportIf( ServerCtxt* server )
{
    XWStreamCtxt* stream = NULL;
    if ( server->nv.showRobotScores ) {
        stream = mkServerStream( server );
        (void)model_checkMoveLegal( server->vol.model, 
                                    server->nv.currentTurn, stream,
                                    NULL );
    }
    return stream;
} /* makeMoveReportIf */

/* Client is reporting a move made, complete with new tiles and time taken by
 * the player.  Update the model with that information as a tentative move,
 * then sent info about it to all the clients, and finally commit the move
 * here.
 */
static XP_Bool
reflectMoveAndInform( ServerCtxt* server, XWStreamCtxt* stream )
{
    ModelCtxt* model = server->vol.model;
    XP_U16 whoMoved;
    XP_U16 nTilesMoved = 0; /* trade case */
    XP_Bool isTrade;
    XP_Bool isLegalMove;
    XP_Bool doRequest = XP_FALSE;
    TrayTileSet newTiles;
    TrayTileSet tradedTiles;
    CurGameInfo* gi = server->vol.gi;
    XP_U16 sourceClientIndex = 
        getIndexForDevice( server, stream_getAddress( stream ) );
    XWStreamCtxt* mvStream = NULL;

    XP_ASSERT( gi->serverRole == SERVER_ISSERVER );

    readMoveInfo( server, stream, &whoMoved, &isTrade, &newTiles,
                  &tradedTiles, &isLegalMove ); /* modifies model */
    XP_ASSERT( isLegalMove ); /* client should always report as true */
    isLegalMove = XP_TRUE;

    if ( isTrade ) {

        sendMoveToClientsExcept( server, whoMoved, XP_TRUE, &newTiles, 
                                 &tradedTiles, sourceClientIndex );

        model_makeTileTrade( model, whoMoved,
                             &tradedTiles, &newTiles );
        pool_replaceTiles( server->pool, &tradedTiles );

        server->vol.showPrevMove = XP_TRUE;
        mvStream = makeTradeReportIf( server, &tradedTiles );

    } else {
        nTilesMoved = model_getCurrentMoveCount( model, whoMoved );
        isLegalMove = (nTilesMoved == 0)
            || checkMoveAllowed( server, whoMoved );

        /* I don't think this will work if there are more than two devices in
           a palm game; need to change state and get out of here before
           returning to send additional messages.  PENDING(ehouse) */
        sendMoveToClientsExcept( server, whoMoved, isLegalMove, &newTiles, 
                                 (TrayTileSet*)NULL, sourceClientIndex );

        server->vol.showPrevMove = XP_TRUE;
        mvStream = makeMoveReportIf( server );

        model_commitTurn( model, whoMoved, &newTiles );
        resetEngines( server );
    }

    if ( isLegalMove ) {
        XP_U16 nTilesLeft = model_getNumTilesTotal( model, whoMoved );

        if ( (gi->phoniesAction == PHONIES_DISALLOW) && (nTilesMoved > 0) ) {
            server->lastMoveSource = sourceClientIndex;
            SETSTATE( server, XWSTATE_MOVE_CONFIRM_MUSTSEND );
            doRequest = XP_TRUE;
        } else if ( nTilesLeft > 0 ) {
            nextTurn( server, PICK_NEXT );
        } else {
            SETSTATE(server, XWSTATE_NEEDSEND_ENDGAME );
            doRequest = XP_TRUE;
        }

        if ( !!mvStream ) {
            XP_ASSERT( !server->vol.prevMoveStream );
            server->vol.prevMoveStream = mvStream;
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
        util_requestTime( server->vol.util );
    }

    return XP_TRUE;
} /* reflectMoveAndInform */

static XP_Bool
reflectMove( ServerCtxt* server, XWStreamCtxt* stream )
{
    XP_Bool moveOk;
    XP_Bool isTrade;
    XP_Bool isLegal;
    XP_U16 whoMoved;
    TrayTileSet newTiles;
    TrayTileSet tradedTiles;
    ModelCtxt* model = server->vol.model;
    XWStreamCtxt* mvStream = NULL;

    moveOk = XWSTATE_INTURN == server->nv.gameState;
    if ( moveOk ) {
        readMoveInfo( server, stream, &whoMoved, &isTrade, &newTiles, 
                      &tradedTiles, &isLegal ); /* modifies model */

        if ( isTrade ) {
            model_makeTileTrade( model, whoMoved, &tradedTiles, &newTiles );
            pool_replaceTiles( server->pool, &tradedTiles );

            server->vol.showPrevMove = XP_TRUE;
            mvStream = makeTradeReportIf( server, &tradedTiles );
        } else {
            server->vol.showPrevMove = XP_TRUE;
            mvStream = makeMoveReportIf( server );
            model_commitTurn( model, whoMoved, &newTiles );
        }

        if ( !!mvStream ) {
            XP_ASSERT( !server->vol.prevMoveStream );
            server->vol.prevMoveStream = mvStream;
        }

        resetEngines( server );

        if ( !isLegal ) {
            XP_ASSERT( server->vol.gi->serverRole == SERVER_ISCLIENT );
            handleIllegalWord( server, stream );
        }
    } else {
        XP_LOGF( "%s: dropping move: state=%s", __func__,
                 getStateStr(server->nv.gameState ) );
    }
    return moveOk;
} /* reflectMove */
#endif /* XWFEATURE_STANDALONE_ONLY */

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
XP_Bool
server_commitMove( ServerCtxt* server )
{
    XP_S16 turn = server->nv.currentTurn;
    ModelCtxt* model = server->vol.model;
    CurGameInfo* gi = server->vol.gi;
    TrayTileSet newTiles;
    XP_U16 nTilesMoved;
    XP_Bool isLegalMove = XP_TRUE;
    XP_Bool isClient = gi->serverRole == SERVER_ISCLIENT;

#ifdef DEBUG
    if ( IS_ROBOT( &gi->players[turn] ) ) {
        XP_ASSERT( model_checkMoveLegal( model, turn, (XWStreamCtxt*)NULL,
                                         (WordNotifierInfo*)NULL ) );
    }
#endif

    /* commit the move.  get new tiles.  if server, send to everybody.
       if client, send to server.  */
    XP_ASSERT( turn >= 0 );

    nTilesMoved = model_getCurrentMoveCount( model, turn );
    fetchTiles( server, turn, nTilesMoved, NULL, &newTiles );

#ifndef XWFEATURE_STANDALONE_ONLY
    if ( isClient ) {
        /* just send to server */
        sendMoveTo( server, SERVER_DEVICE, turn, XP_TRUE, &newTiles,
                    (TrayTileSet*)NULL );
    } else {
        isLegalMove = checkMoveAllowed( server, turn );
        sendMoveToClientsExcept( server, turn, isLegalMove, &newTiles,
                                 (TrayTileSet*)NULL, SERVER_DEVICE );
    }
#else
    isLegalMove = checkMoveAllowed( server, turn );
#endif

    model_commitTurn( model, turn, &newTiles );

    if ( !isLegalMove && !isClient ) {
        badWordMoveUndoAndTellUser( server, &server->illegalWordInfo );
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
#endif
    } else {
        nextTurn( server, PICK_NEXT );
    }
    
    return XP_TRUE;
} /* server_commitMove */

static void
removeTradedTiles( ServerCtxt* server, TileBit selBits, TrayTileSet* tiles )
{
    XP_U8 nTiles = 0;
    XP_S16 index;
    XP_S16 turn = server->nv.currentTurn;

    /* selBits: It's gross that server knows this much about tray's
       implementation.  PENDING(ehouse) */

    for ( index = 0; selBits != 0; selBits >>= 1, ++index ) {
        if ( (selBits & 0x01) != 0 ) {
            Tile tile = model_getPlayerTile( server->vol.model, turn, index );
            tiles->tiles[nTiles++] = tile;
        }
    }
    tiles->nTiles = nTiles;
} /* saveTradedTiles */

XP_Bool
server_commitTrade( ServerCtxt* server, TileBit selBits )
{
    TrayTileSet oldTiles;
    TrayTileSet newTiles;
    XP_U16 turn = server->nv.currentTurn;

    removeTradedTiles( server, selBits, &oldTiles );

    fetchTiles( server, turn, oldTiles.nTiles, &oldTiles, &newTiles );

#ifndef XWFEATURE_STANDALONE_ONLY
    if ( server->vol.gi->serverRole == SERVER_ISCLIENT ) {
        /* just send to server */
        sendMoveTo(server, SERVER_DEVICE, turn, XP_TRUE, &newTiles, &oldTiles);
    } else {
        sendMoveToClientsExcept( server, turn, XP_TRUE, &newTiles, &oldTiles, 
                                 SERVER_DEVICE );
    }
#endif

    pool_replaceTiles( server->pool, &oldTiles );
    model_makeTileTrade( server->vol.model, server->nv.currentTurn,
                         &oldTiles, &newTiles );
    nextTurn( server, PICK_NEXT );
    return XP_TRUE;
} /* server_commitTrade */

XP_S16
server_getCurrentTurn( ServerCtxt* server )
{
    return server->nv.currentTurn;
} /* server_getCurrentTurn */

XP_Bool
server_getGameIsOver( ServerCtxt* server )
{
    return server->nv.gameState == XWSTATE_GAMEOVER;
} /* server_getGameIsOver */

static void
doEndGame( ServerCtxt* server )
{
    SETSTATE( server, XWSTATE_GAMEOVER );
    setTurn( server, -1 );

    (*server->vol.gameOverListener)( server->vol.gameOverData );
} /* doEndGame */

/* Somebody wants to end the game.
 *
 * If I'm the server, I send a END_GAME message to all clients.  If I'm a
 * client, I send the GAME_OVER_REQUEST message to the server.  If I'm the
 * server and this is called in response to a GAME_OVER_REQUEST, send the
 * GAME_OVER message to all clients including the one that requested it.
 */
static void
endGameInternal( ServerCtxt* server, GameEndReason XP_UNUSED(why) )
{
    XP_ASSERT( server->nv.gameState != XWSTATE_GAMEOVER );

    if ( server->vol.gi->serverRole != SERVER_ISCLIENT ) {

#ifndef XWFEATURE_STANDALONE_ONLY
        XP_U16 devIndex;
        for ( devIndex = 1; devIndex < server->nv.nDevices; ++devIndex ) {
            XWStreamCtxt* stream;
            stream = messageStreamWithHeader( server, devIndex,
                                              XWPROTO_END_GAME );
            stream_destroy( stream );
        }
#endif
        doEndGame( server );

#ifndef XWFEATURE_STANDALONE_ONLY
    } else {
        XWStreamCtxt* stream;
        stream = messageStreamWithHeader( server, SERVER_DEVICE,
                                          XWPROTO_CLIENT_REQ_END_GAME );
        stream_destroy( stream );

        /* Do I want to change the state I'm in?  I don't think so.... */
#endif
    }
} /* endGameInternal */

void
server_endGame( ServerCtxt* server )
{
    XW_State gameState = server->nv.gameState;
    if ( gameState < XWSTATE_GAMEOVER && gameState >= XWSTATE_INTURN ) {
        endGameInternal( server, END_REASON_USER_REQUEST );
    }
} /* server_endGame */

/* If game is about to end because one player's out of tiles, we don't want to
 * keep trying to move */
static XP_Bool
tileCountsOk( const ServerCtxt* server )
{
    XP_Bool maybeOver = 0 == pool_getNTilesLeft( server->pool );
    if ( maybeOver ) {
        ModelCtxt* model = server->vol.model;
        XP_U16 nPlayers = server->vol.gi->nPlayers;
        XP_Bool zeroFound = XP_FALSE;

        while ( nPlayers-- ) {
            XP_U16 count = model_getNumTilesTotal( model, nPlayers );
            if ( count == 0 ) {
                zeroFound = XP_TRUE;
                break;
            }
        }
        maybeOver = zeroFound;
    }
    return !maybeOver;
} /* tileCountsOk */

static void
setTurn( ServerCtxt* server, XP_S16 turn )
{
    if ( server->nv.currentTurn != turn ) {
        server->nv.currentTurn = turn;
        callTurnChangeListener( server );
    }
}

#ifndef XWFEATURE_STANDALONE_ONLY
static void
tellMoveWasLegal( ServerCtxt* server )
{
    XWStreamCtxt* stream;

    stream = messageStreamWithHeader( server, server->lastMoveSource,
                                      XWPROTO_MOVE_CONFIRM );
    stream_destroy( stream );
} /* tellMoveWasLegal */

static XP_Bool
handleIllegalWord( ServerCtxt* server, XWStreamCtxt* incoming )
{
    BadWordInfo bwi;
    XP_U16 whichPlayer;

    whichPlayer = stream_getBits( incoming, PLAYERNUM_NBITS );
    bwiFromStream( MPPARM(server->mpool) incoming, &bwi );

    badWordMoveUndoAndTellUser( server, &bwi );

    freeBWI( MPPARM(server->mpool) &bwi );

    return XP_TRUE;
} /* handleIllegalWord */

static XP_Bool
handleMoveOk( ServerCtxt* server, XWStreamCtxt* XP_UNUSED(incoming) )
{
    XP_Bool accepted = XP_TRUE;
    XP_ASSERT( server->vol.gi->serverRole == SERVER_ISCLIENT );
    XP_ASSERT( server->nv.gameState == XWSTATE_MOVE_CONFIRM_WAIT );

    nextTurn( server, PICK_NEXT );

    return accepted;
} /* handleMoveOk */

static void
sendUndoTo( ServerCtxt* server, XP_U16 devIndex, XP_U16 nUndone, 
            XP_U16 lastUndone )
{
    XWStreamCtxt* stream;
    CurGameInfo* gi = server->vol.gi;
    XW_Proto code = gi->serverRole == SERVER_ISCLIENT?
        XWPROTO_UNDO_INFO_CLIENT : XWPROTO_UNDO_INFO_SERVER;

    stream = messageStreamWithHeader( server, devIndex, code );

    stream_putU16( stream, nUndone );
    stream_putU16( stream, lastUndone );

    stream_destroy( stream );
} /* sendUndoTo */

static void
sendUndoToClientsExcept( ServerCtxt* server, XP_U16 skip, 
                         XP_U16 nUndone, XP_U16 lastUndone )
{
    XP_U16 devIndex;

    for ( devIndex = 1; devIndex < server->nv.nDevices; ++devIndex ) {
        if ( devIndex != skip ) {
            sendUndoTo( server, devIndex, nUndone, lastUndone );
        }
    }
} /* sendUndoToClientsExcept */

static XP_Bool
reflectUndos( ServerCtxt* server, XWStreamCtxt* stream, XW_Proto code )
{
    XP_U16 nUndone, lastUndone;
    XP_S16 moveNum;
    XP_U16 turn;
    ModelCtxt* model = server->vol.model;
    XP_Bool success = XP_TRUE;

    nUndone = stream_getU16( stream );
    lastUndone = stream_getU16( stream );

    moveNum = lastUndone + nUndone - 1;

    success = model_undoLatestMoves(model, server->pool, nUndone, &turn, 
                                    &moveNum);

    if ( success ) {
        XP_ASSERT( moveNum == lastUndone );

        if ( code == XWPROTO_UNDO_INFO_CLIENT ) { /* need to inform */
            XP_U16 sourceClientIndex = 
                getIndexForDevice( server, stream_getAddress( stream ) );

            sendUndoToClientsExcept( server, sourceClientIndex, nUndone, 
                                     lastUndone );

        }
        nextTurn( server, turn );
    }

    return success;
} /* reflectUndos */
#endif

XP_Bool
server_handleUndo( ServerCtxt* server )
{
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
        if ( !model_undoLatestMoves( model, server->pool, 1, &lastTurnUndone, 
                                     &moveNum ) ) {
            break;
        }
        ++nUndone;
        XP_ASSERT( moveNum >= 0 );
        lastUndone = moveNum;
        if ( !IS_ROBOT(&gi->players[lastTurnUndone]) ) {
            break;
        }
    }

    result = nUndone > 0 ;
    if ( result ) {
#ifndef XWFEATURE_STANDALONE_ONLY
        XP_ASSERT( lastUndone != 0xFFFF );
        if ( server->vol.gi->serverRole == SERVER_ISCLIENT ) {
            sendUndoTo( server, SERVER_DEVICE, nUndone, lastUndone );
        } else {
            sendUndoToClientsExcept( server, SERVER_DEVICE, nUndone, 
                                     lastUndone );
        }
#endif
        nextTurn( server, lastTurnUndone );
    } else {
        /* I'm a bit nervous about this.  Is this the ONLY thing that cause
           nUndone to come back 0? */
        util_userError( server->vol.util, ERR_CANT_UNDO_TILEASSIGN );
    }

    return result;
} /* server_handleUndo */

#ifndef XWFEATURE_STANDALONE_ONLY
XP_Bool
server_receiveMessage( ServerCtxt* server, XWStreamCtxt* incoming )
{
    XW_Proto code;
    XP_Bool accepted = XP_FALSE;

    code = (XW_Proto)stream_getBits( incoming, XWPROTO_NBITS );

    printCode("Receiving", code);

    if ( code == XWPROTO_DEVICE_REGISTRATION ) {
        /* This message is special: doesn't have the header that's possible
           once the game's in progress and communication's been
           established. */
        XP_LOGF( "%s: somebody's registering!!!", __func__ );
        accepted = handleRegistrationMsg( server, incoming );

    } else if ( code == XWPROTO_CLIENT_SETUP ) {

        XP_STATUSF( "client got XWPROTO_CLIENT_SETUP" );
        XP_ASSERT( server->vol.gi->serverRole == SERVER_ISCLIENT );
        accepted = client_readInitialMessage( server, incoming );

    } else if ( readStreamHeader( server, incoming ) ) {

        switch( code ) {
/*         case XWPROTO_MOVEMADE_INFO: */
/*             accepted = client_reflectMoveMade( server, incoming ); */
/*             if ( accepted ) { */
/*                 nextTurn( server ); */
/*             } */
/*             break; */
/*         case XWPROTO_TRADEMADE_INFO: */
/*             accepted = client_reflectTradeMade( server, incoming ); */
/*             if ( accepted ) { */
/*                 nextTurn( server ); */
/*             } */
/*             break; */
/*         case XWPROTO_CLIENT_MOVE_INFO: */
/*             accepted = handleClientMoved( server, incoming ); */
/*             break; */
/*         case XWPROTO_CLIENT_TRADE_INFO: */
/*             accepted = handleClientTraded( server, incoming ); */
/*             break; */

        case XWPROTO_MOVEMADE_INFO_CLIENT: /* client is reporting a move */
            accepted = (XWSTATE_INTURN == server->nv.gameState)
                && reflectMoveAndInform( server, incoming );
            break;

        case XWPROTO_MOVEMADE_INFO_SERVER: /* server telling me about a move */
            accepted = reflectMove( server, incoming );
            if ( accepted ) {
                nextTurn( server, PICK_NEXT );
            }
            break;

        case XWPROTO_UNDO_INFO_CLIENT:
        case XWPROTO_UNDO_INFO_SERVER:
            accepted = reflectUndos( server, incoming, code );
            /* nextTurn is called by reflectUndos */
            break;

        case XWPROTO_BADWORD_INFO:
            accepted = handleIllegalWord( server, incoming );
            if ( accepted && server->nv.gameState != XWSTATE_GAMEOVER ) {
                nextTurn( server, PICK_NEXT );
            }
            break;

        case XWPROTO_MOVE_CONFIRM:
            accepted = handleMoveOk( server, incoming );
            break;

        case XWPROTO_CLIENT_REQ_END_GAME:
            endGameInternal( server, END_REASON_USER_REQUEST );
            accepted = XP_TRUE;
            break;
        case XWPROTO_END_GAME:
            doEndGame( server );
            accepted = XP_TRUE;
            break;
        default:
            XP_WARNF( "Unknown code on incoming message: %d\n", code );
            break;
        } /* switch */
    }

    stream_close( incoming );
    return accepted;
} /* server_receiveMessage */
#endif

void 
server_formatDictCounts( ServerCtxt* server, XWStreamCtxt* stream,
                         XP_U16 nCols )
{
    DictionaryCtxt* dict;
    Tile tile;
    XP_U16 nChars, nPrinted;
    XP_UCHAR buf[48];
    const XP_UCHAR* fmt = util_getUserString( server->vol.util, 
                                              STRS_VALUES_HEADER );
    const XP_UCHAR* dname;

    XP_ASSERT( !!server->vol.model );

    dict = model_getDictionary( server->vol.model );
    dname = dict_getShortName( dict );
    XP_SNPRINTF( buf, sizeof(buf), fmt, dname );
    stream_catString( stream, buf );

    nChars = dict_numTileFaces( dict );

    for ( tile = 0, nPrinted = 0; ; ) {
        XP_UCHAR buf[24];
        XP_U16 count, value;

        count = dict_numTiles( dict, tile );

        if ( count > 0 ) {
            const XP_UCHAR* face = dict_getTileString( dict, tile );
            value = dict_getTileValue( dict, tile );

            XP_SNPRINTF( buf, sizeof(buf), (XP_UCHAR*)"%s: %d/%d", 
                         face, count, value );
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
server_formatRemainingTiles( ServerCtxt* server, XWStreamCtxt* stream,
                             XP_S16 player )
{
    DictionaryCtxt* dict;
    Tile tile;
    XP_U16 nChars;
    XP_U16 counts[MAX_UNIQUE_TILES+1]; /* 1 for the blank */
    PoolContext* pool = server->pool;

    if ( !pool ) {
        return;  /* might want to print an explanation in the stream */
    }

    XP_ASSERT( !!server->vol.model );

    XP_MEMSET( counts, 0, sizeof(counts) );
    model_countAllTrayTiles( server->vol.model, counts, player );

    dict = model_getDictionary( server->vol.model );
    nChars = dict_numTileFaces( dict );

    for ( tile = 0; ; ) {
        XP_U16 count = pool_getNTilesLeftFor( pool, tile ) + counts[tile];
        XP_Bool hasCount = count > 0;

        if ( hasCount ) {
            const XP_UCHAR* face = dict_getTileString( dict, tile );

            for ( ; ; ) {
                stream_catString( stream, face );
                if ( --count == 0 ) {
                    break;
                }
                stream_catString( stream, "." );
            }
        }

        if ( ++tile >= nChars ) {
            break;
        } else if ( hasCount ) {
            stream_catString( stream, (void*)"   " );
        }
    }
} /* server_formatRemainingTiles */

#define IMPOSSIBLY_LOW_SCORE -1000
void
server_writeFinalScores( ServerCtxt* server, XWStreamCtxt* stream )
{
    ScoresArray scores;
    ScoresArray tilePenalties;
    XP_S16 highestIndex;
    XP_S16 highestScore;
    XP_U16 place, nPlayers, ii;
    XP_S16 curScore;
    ModelCtxt* model = server->vol.model;
    const XP_UCHAR* addString = util_getUserString( server->vol.util,
                                                    STRD_REMAINING_TILES_ADD );
    const XP_UCHAR* subString = util_getUserString( server->vol.util,
                                                    STRD_UNUSED_TILES_SUB );
    XP_UCHAR timeBuf[16];
    XP_UCHAR* timeStr;
    CurGameInfo* gi = server->vol.gi;

    XP_ASSERT( server->nv.gameState == XWSTATE_GAMEOVER );

    model_figureFinalScores( model, &scores, &tilePenalties );

    nPlayers = gi->nPlayers;

    for ( place = 1; ; ++place ) {
        XP_UCHAR tmpbuf[48];
        XP_UCHAR buf[128];
        XP_Bool firstDone;

        highestScore = IMPOSSIBLY_LOW_SCORE;
        highestIndex = -1;

        for ( ii = 0; ii < nPlayers; ++ii ) {
            if ( scores.arr[ii] > highestScore ) {
                highestIndex = ii;
                highestScore = scores.arr[ii];
            }
        }

        if ( highestIndex == -1 ) {
            break; /* we're done */
        } else if ( place > 1 ) {
            stream_catString( stream, XP_CR );
        }
        scores.arr[highestIndex] = IMPOSSIBLY_LOW_SCORE;

        curScore = model_getPlayerScore( model, highestIndex );

        timeStr = (XP_UCHAR*)"";
        if ( gi->timerEnabled ) {
            XP_U16 penalty = player_timePenalty( gi, highestIndex );
            if ( penalty > 0 ) {
                XP_SNPRINTF( timeBuf, sizeof(timeBuf), 
                             util_getUserString( 
                                                server->vol.util,
                                                STRD_TIME_PENALTY_SUB ),
                             penalty ); /* positive for formatting */
                timeStr = timeBuf;
            }
        }

        firstDone = model_getNumTilesTotal( model, highestIndex) == 0;
        XP_SNPRINTF( tmpbuf, sizeof(tmpbuf), 
                     (firstDone? addString:subString),
                     firstDone? 
                     tilePenalties.arr[highestIndex]:
                     -tilePenalties.arr[highestIndex] );

        XP_SNPRINTF( buf, sizeof(buf), 
                     (XP_UCHAR*)"[%d] %s: %d" XP_CR "  (%d %s%s)",
                     place, 
                     emptyStringIfNull(gi->players[highestIndex].name),
                     highestScore, curScore, tmpbuf, timeStr );
        stream_catString( stream, buf );
    }
} /* server_writeFinalScores */

#ifdef CPLUS
}
#endif
