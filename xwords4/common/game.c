/* -*- compile-command: "cd ../linux && make -j3 MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 2001-2011 by Eric House (xwords@eehouse.org).  All rights
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

#include "game.h"
#include "dictnry.h"
#include "strutils.h"
#include "nli.h"
#include "dbgutil.h"

#ifdef CPLUS
extern "C" {
#endif

#define FLAG_HASCOMMS 0x01

#ifdef DEBUG
static void
assertUtilOK( XW_UtilCtxt* util )
{
    UtilVtable* vtable;
    XP_U16 nSlots;
    XP_ASSERT( !!util );
    vtable = util->vtable;
    nSlots = sizeof(vtable) / 4;
    while ( nSlots-- ) {
        void* fptr = ((void**)vtable)[nSlots];
        XP_ASSERT( !!fptr );
    }
} /* assertUtilOK */
#else
# define assertUtilOK(u)
#endif

#ifdef XWFEATURE_CHANGEDICT
static void gi_setDict( MPFORMAL CurGameInfo* gi, const DictionaryCtxt* dict );
#endif

static void
checkServerRole( CurGameInfo* gi, XP_U16* nPlayersHere, 
                 XP_U16* nPlayersTotal )
{
    if ( !!gi ) {
        XP_U16 ii, remoteCount = 0;

        if ( SERVER_STANDALONE != gi->serverRole ) {
            for ( ii = 0; ii < gi->nPlayers; ++ii ) {
                if ( !gi->players[ii].isLocal ) {
                    ++remoteCount;
                }
            }

            /* I think this error is caught in nwgamest.c now */
            XP_ASSERT( remoteCount > 0 );
            if ( remoteCount == 0 ) {
                gi->serverRole = SERVER_STANDALONE;
            }
        }

        *nPlayersHere = gi->nPlayers - remoteCount;
        *nPlayersTotal = gi->nPlayers;
    }
} /* checkServerRole */

static XP_U32
makeGameID( XW_UtilCtxt* XP_UNUSED_DBG(util) )
{
    XP_U32 gameID = 0;
    assertUtilOK( util );
    while ( 0 == gameID ) {
        /* High bit never set by XP_RANDOM() alone */
        gameID = (XP_RANDOM() << 16) ^ XP_RANDOM();
        /* But let's clear it -- set high-bit causes problems for existing
           postgres DB where INTEGER is apparently a signed 32-bit */
        gameID &= 0x7FFFFFFF;
    }
    LOG_RETURNF( "%X/%d", gameID, gameID );
    return gameID;
}

static void
timerChangeListener( XWEnv xwe, void* data, const XP_U32 gameID,
                     XP_S32 oldVal, XP_S32 newVal )
{
    XWGame* game = (XWGame*)data;
    XP_ASSERT( game->util->gameInfo->gameID == gameID );
    XP_LOGF( "%s(oldVal=%d, newVal=%d, id=%d)", __func__, oldVal, newVal, gameID );
    dutil_onDupTimerChanged( util_getDevUtilCtxt( game->util, xwe ), xwe,
                             gameID, oldVal, newVal );
}

#ifdef XWFEATURE_RELAY
static void
onRoleChanged( XWEnv xwe, void* closure, XP_Bool amNowGuest  )
{
    XP_ASSERT( amNowGuest );
    XWGame* game = (XWGame*)closure;
    server_onRoleChanged( game->server, xwe, amNowGuest );
}
#endif

static void
setListeners( XWGame* game, const CommonPrefs* cp )
{
    server_prefsChanged( game->server, cp );
    board_prefsChanged( game->board, cp );
    server_setTimerChangeListener( game->server, timerChangeListener, game );
}

static const DictionaryCtxt*
getDicts( const CurGameInfo* gi, XW_UtilCtxt* util, XWEnv xwe,
          PlayerDicts* playerDicts )
{
    const XP_UCHAR* isoCode = gi->isoCodeStr;
    const DictionaryCtxt* result = util_getDict( util, xwe, isoCode, gi->dictName );
    XP_MEMSET( playerDicts, 0, sizeof(*playerDicts) );
    if ( !!result ) {
        for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
            const LocalPlayer* lp = &gi->players[ii];
            if ( lp->isLocal && !!lp->dictName && lp->dictName[0] ) {
                playerDicts->dicts[ii] = util_getDict( util, xwe, isoCode,
                                                       lp->dictName );
            }
        }
    }
    return result;
}

static void
unrefDicts( XWEnv xwe, const DictionaryCtxt* dict, PlayerDicts* playerDicts )
{
    if ( !!dict ) {
        dict_unref( dict, xwe );
    }
    for ( int ii = 0; ii < VSIZE(playerDicts->dicts); ++ii ) {
        const DictionaryCtxt* dict = playerDicts->dicts[ii];
        if ( !!dict ) {
            dict_unref( dict, xwe );
        }
    }
}

XP_Bool
game_makeNewGame( MPFORMAL XWEnv xwe, XWGame* game, CurGameInfo* gi,
                  const CommsAddrRec* selfAddr, const CommsAddrRec* hostAddr,
                  XW_UtilCtxt* util,
                  DrawCtx* draw, const CommonPrefs* cp,
                  const TransportProcs* procs
#ifdef SET_GAMESEED
                  ,XP_U16 gameSeed 
#endif
                  )
{
    XP_ASSERT( gi == util->gameInfo ); /* if holds, remove gi param */
#ifndef XWFEATURE_STANDALONE_ONLY
    XP_U16 nPlayersHere = 0;
    XP_U16 nPlayersTotal = 0;
    checkServerRole( gi, &nPlayersHere, &nPlayersTotal );
#endif
    assertUtilOK( util );

    if ( 0 == gi->gameID ) {
        gi->gameID = makeGameID( util );
    }
    game->created = dutil_getCurSeconds( util_getDevUtilCtxt( util, xwe ), xwe );
    game->util = util;

    PlayerDicts playerDicts;
    const DictionaryCtxt* dict = getDicts( gi, util, xwe, &playerDicts );
    XP_Bool success = !!dict;

    if ( success ) {
        XP_STRNCPY( gi->isoCodeStr, dict_getISOCode( dict ), VSIZE(gi->isoCodeStr) );
        XP_ASSERT( !!gi->isoCodeStr[0] );
        game->model = model_make( MPPARM(mpool) xwe, (DictionaryCtxt*)NULL,
                                  NULL, util, gi->boardSize );

        model_setDictionary( game->model, xwe, dict );
        model_setPlayerDicts( game->model, xwe, &playerDicts );

#ifndef XWFEATURE_STANDALONE_ONLY
        if ( gi->serverRole != SERVER_STANDALONE ) {
            game->comms = comms_make( MPPARM(mpool) xwe, util,
                                      gi->serverRole != SERVER_ISCLIENT,
                                      selfAddr, hostAddr, procs,
#ifdef XWFEATURE_RELAY
                                      nPlayersHere, nPlayersTotal,
                                      onRoleChanged, game,
#endif
                                      gi->forceChannel
#ifdef SET_GAMESEED
                                      , gameSeed
#endif
                                      );
        } else {
            game->comms = (CommsCtxt*)NULL;
        }


#endif
        game->server = server_make( MPPARM(mpool) xwe, game->model,
#ifndef XWFEATURE_STANDALONE_ONLY
                                    game->comms,
#else
                                    (CommsCtxt*)NULL,
#endif
                                    util );
        game->board = board_make( MPPARM(mpool) xwe, game->model, game->server,
                                  NULL, util );
        board_setCallbacks( game->board, xwe );

        board_setDraw( game->board, xwe, draw );
        setListeners( game, cp );
    }

    unrefDicts( xwe, dict, &playerDicts );

    return success;
} /* game_makeNewGame */

XP_Bool
game_makeRematch( const XWGame* oldGame, XWEnv xwe, XW_UtilCtxt* newUtil,
                  const CommonPrefs* newCp, const TransportProcs* procs,
                  XWGame* newGame, const XP_UCHAR* newName )
{
    XP_Bool success = XP_FALSE;
    XP_LOGFF( "(newName=%s)", newName );

    RematchAddrs ra;
    if ( server_getRematchInfo( oldGame->server, newUtil,
                                makeGameID( newUtil ), &ra ) ) {
        CommsAddrRec* selfAddrP = NULL;
        CommsAddrRec selfAddr;
        if ( !!oldGame->comms ) {
            comms_getSelfAddr( oldGame->comms, &selfAddr );
            selfAddrP = &selfAddr;
        }

        if ( game_makeNewGame( MPPARM(newUtil->mpool) xwe, newGame,
                               newUtil->gameInfo, selfAddrP, (CommsAddrRec*)NULL,
                               newUtil, (DrawCtx*)NULL, newCp, procs ) ) {

            const CurGameInfo* newGI = newUtil->gameInfo;
            for ( int ii = 0; ii < ra.nAddrs; ++ii ) {
                NetLaunchInfo nli;
                /* hard-code one player per device -- for now */
                nli_init( &nli, newGI, selfAddrP, 1, ii + 1 );
                if ( !!newName ) {
                    nli_setGameName( &nli, newName );
                }
                LOGNLI( &nli );
                comms_invite( newGame->comms, xwe, &nli, &ra.addrs[ii], XP_TRUE );
            }
            success = XP_TRUE;
        }
    }
    LOG_RETURNF( "%s", boolToStr(success) );
    return success;
}

#ifdef XWFEATURE_CHANGEDICT
void
game_changeDict( MPFORMAL XWGame* game, XWEnv xwe, CurGameInfo* gi, DictionaryCtxt* dict )
{
    model_setDictionary( game->model, xwe, dict );
    gi_setDict( MPPARM(mpool) gi, dict );
    server_resetEngines( game->server );
}
#endif

XP_Bool
game_makeFromStream( MPFORMAL XWEnv xwe, XWStreamCtxt* stream,
                     XWGame* game, CurGameInfo* gi,
                     XW_UtilCtxt* util, DrawCtx* draw, CommonPrefs* cp,
                     const TransportProcs* procs )
{
    XP_ASSERT( NULL == util || gi == util->gameInfo );
    XP_Bool success = XP_FALSE;
    XP_U8 strVersion;
#ifndef XWFEATURE_STANDALONE_ONLY
    XP_Bool hasComms;
#endif
    strVersion = stream_getU8( stream );
    XP_LOGFF( "strVersion = 0x%x", (XP_U16)strVersion );

    if ( strVersion > CUR_STREAM_VERS ) {
        XP_LOGFF( "aborting; stream version too new (%d > %d)!",
                 strVersion, CUR_STREAM_VERS );
    } else {
        do { /* do..while so can break */
            stream_setVersion( stream, strVersion );

            gi_readFromStream( MPPARM(mpool) stream, gi );
            if ( !game ) {
                success = XP_TRUE;
                break;
            } else if ( stream_getSize(stream) == 0 ) {
                XP_LOGFF( "gi was all we got; failing." );
                break;
            }
            game->util = util;
            game->created = strVersion < STREAM_VERS_GICREATED
                ? 0 : stream_getU32( stream );

            PlayerDicts playerDicts;
            const DictionaryCtxt* dict = getDicts( gi, util, xwe, &playerDicts );
            if ( !dict ) {
                break;
            }

            /* Previous stream versions didn't save anything if built
             * standalone.  Now we always save something.  But we need to know
             * if the previous version didn't save. PREV_WAS_STANDALONE_ONLY
             * tells us that.
             */
            hasComms = XP_FALSE;
            if ( STREAM_VERS_ALWAYS_MULTI <= strVersion  /* new stream */
#ifndef PREV_WAS_STANDALONE_ONLY
                 || XP_TRUE                        /* old, but saved this anyway */
#endif
                 ) {
                if ( strVersion < STREAM_VERS_GICREATED ) {
                    hasComms = stream_getU8( stream );
                } else {
                    XP_U8 flags = stream_getU8( stream );
                    hasComms = flags & FLAG_HASCOMMS;
                }
            }

            XP_ASSERT( hasComms == (SERVER_STANDALONE != gi->serverRole) );
            if ( hasComms ) {
                game->comms = comms_makeFromStream( MPPARM(mpool) xwe, stream, util,
                                                    gi->serverRole != SERVER_ISCLIENT,
                                                    procs,
#ifdef XWFEATURE_RELAY
                                                    onRoleChanged, game,
#endif
                                                    gi->forceChannel );
            } else {
                XP_ASSERT( NULL == game->comms );
                game->comms = NULL;
            }

            game->model = model_makeFromStream( MPPARM(mpool) xwe, stream, dict,
                                                &playerDicts, util );

            game->server = server_makeFromStream( MPPARM(mpool) xwe, stream,
                                                  game->model, game->comms, 
                                                  util, gi->nPlayers );

            game->board = board_makeFromStream( MPPARM(mpool) xwe, stream,
                                                game->model, game->server, 
                                                NULL, util, gi->nPlayers );
            setListeners( game, cp );
            board_setDraw( game->board, xwe, draw );
            success = XP_TRUE;
            unrefDicts( xwe, dict, &playerDicts );
        } while( XP_FALSE );
    }

    if ( success && !!game && !!game->comms ) {
        XP_ASSERT( comms_getIsServer(game->comms) == server_getIsServer(game->server) );

#ifdef XWFEATURE_KNOWNPLAYERS
        const XP_U32 created = game->created;
        if ( 0 != created
             && server_getGameIsConnected( game->server ) ) {
            comms_gatherPlayers( game->comms, xwe, created );
        }
#endif
    }

    return success;
} /* game_makeFromStream */

/* This is a gross hack. Fix it someday. */
static void
runServer( ServerCtxt* server, XWEnv xwe )
{
    for ( int ii = 0; ii < 5; ++ii ) {
        (void)server_do( server, xwe );
    }
}

XP_Bool
game_makeFromInvite( XWGame* newGame, XWEnv xwe, const NetLaunchInfo* nli,
                     const CommsAddrRec* selfAddr, XW_UtilCtxt* util,
                     DrawCtx* draw, CommonPrefs* cp, const TransportProcs* procs )
{
    LOG_FUNC();
    XP_U32 gameID = nli->gameID;
    XP_U8 forceChannel = nli->forceChannel;
    XW_DUtilCtxt* duc = util_getDevUtilCtxt( util, xwe );
    XP_Bool success = !dutil_haveGame( duc, xwe, gameID, forceChannel );
    if ( success ) {
        CurGameInfo* gi = util->gameInfo;
        XP_ASSERT( !!gi );
        nliToGI( nli, xwe, util, gi );

        CommsAddrRec hostAddr;
        nli_makeAddrRec( nli, &hostAddr );

        success = game_makeNewGame( MPPARM(util->mpool) xwe, newGame,
                                    gi, selfAddr, &hostAddr, util,
                                    draw, cp, procs );
        if ( success && server_initClientConnection( newGame->server, xwe ) ) {
            runServer( newGame->server, xwe );
        }
    }
    LOG_RETURNF( "%s", boolToStr(success) );
    return success;
}

void
game_saveToStream( const XWGame* game, const CurGameInfo* gi,
                   XWStreamCtxt* stream, XP_U16 saveToken )
{
    XP_ASSERT( gi_equal( gi, game->util->gameInfo ) );
    stream_putU8( stream, CUR_STREAM_VERS );
    stream_setVersion( stream, CUR_STREAM_VERS );

    gi_writeToStream( stream, gi );

    if ( !!game ) {
        const XP_U32 created = game->created;
        stream_putU32( stream, created );
        XP_ASSERT( 0 != saveToken );

        XP_U8 flags = NULL == game->comms ? 0 : FLAG_HASCOMMS;
        stream_putU8( stream, flags );
#ifdef XWFEATURE_STANDALONE_ONLY
        XP_ASSERT( !game->comms );
#endif
        if ( NULL != game->comms ) {
            comms_writeToStream( game->comms, stream, saveToken );
        }

        model_writeToStream( game->model, stream );
        server_writeToStream( game->server, stream );
        board_writeToStream( game->board, stream );
    }
} /* game_saveToStream */

void
game_saveSucceeded( const XWGame* game, XWEnv xwe, XP_U16 saveToken )
{
    if ( !!game->comms ) {
        comms_saveSucceeded( game->comms, xwe, saveToken );
    }
}

XP_Bool
game_receiveMessage( XWGame* game, XWEnv xwe, XWStreamCtxt* stream,
                     const CommsAddrRec* retAddr )
{
    ServerCtxt* server = game->server;
    CommsMsgState commsState;
    XP_Bool result = NULL != game->comms;
    if ( result ) {
        result = comms_checkIncomingStream( game->comms, xwe, stream, retAddr,
                                            &commsState );
    } else {
        XP_LOGFF( "ERROR: comms NULL!" );
    }
    if ( result ) {
        (void)server_do( server, xwe );

        result = server_receiveMessage( server, xwe, stream );
    }
    comms_msgProcessed( game->comms, xwe, &commsState, !result );

    if ( result ) {
        /* in case MORE work's pending.  Multiple calls are required in at
           least one case, where I'm a host handling client registration *AND*
           I'm a robot.  Only one server_do and I'll never make that first
           robot move.  That's because comms can't detect a duplicate initial
           packet (in validateInitialMessage()). */
        runServer( server, xwe );
    }

    LOG_RETURNF( "%s", boolToStr(result) );
    return result;
}

void
game_getState( const XWGame* game, XWEnv xwe, GameStateInfo* gsi )
{
    const ServerCtxt* server = game->server;
    BoardCtxt* board = game->board;

    XP_Bool gameOver = server_getGameIsOver( server );
    gsi->curTurnSelected = board_curTurnSelected( board );
    gsi->trayVisState = board_getTrayVisState( board );
    gsi->visTileCount = board_visTileCount( board );
    gsi->canHint = !gameOver && board_canHint( board );
    gsi->canUndo = model_canUndo( game->model );
    gsi->canRedo = board_canTogglePending( board );
    gsi->inTrade = board_inTrade( board, &gsi->tradeTilesSelected );
    gsi->canChat = !!game->comms && comms_canChat( game->comms );
    gsi->canShuffle = board_canShuffle( board );
    gsi->canHideRack = board_canHideRack( board );
    gsi->canTrade = board_canTrade( board, xwe );
    gsi->nPendingMessages = !!game->comms ? 
        comms_countPendingPackets(game->comms, NULL) : 0;

    gsi->canRematch = server_canRematch( server );
    gsi->canPause = server_canPause( server );
    gsi->canUnpause = server_canUnpause( server );
}

void
game_summarize( const XWGame* game, const CurGameInfo* gi, GameSummary* summary )
{
    XP_MEMSET( summary, 0, sizeof(*summary) );
    ServerCtxt* server = game->server;
    summary->turn = server_getCurrentTurn( server, &summary->turnIsLocal );
    summary->lastMoveTime = server_getLastMoveTime(server);
    XP_STRNCPY( summary->isoCodeStr, gi->isoCodeStr, VSIZE(summary->isoCodeStr)-1 );
    summary->gameOver = server_getGameIsOver( server );
    summary->nMoves = model_getNMoves( game->model );
    summary->dupTimerExpires = server_getDupTimerExpires( server );

    for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
        const LocalPlayer* lp  = &gi->players[ii];
        if ( LP_IS_ROBOT(lp) || !LP_IS_LOCAL(lp) ) {
            if ( '\0' != summary->opponents[0] ) {
                XP_STRCAT( summary->opponents, ", " );
            }
            XP_STRCAT( summary->opponents, lp->name );
        }
    }
    if ( !!game->comms ) {
        summary->missingPlayers = server_getMissingPlayers( server );
        summary->nPacketsPending =
            comms_countPendingPackets( game->comms, &summary->quashed );
        summary->channelNo = gi->forceChannel;
    }
}

XP_Bool
game_getIsServer( const XWGame* game )
{
    XP_Bool result = comms_getIsServer( game->comms );
    return result;
}

void
game_dispose( XWGame* game, XWEnv xwe )
{
#ifdef XWFEATURE_KNOWNPLAYERS
    const XP_U32 created = game->created;
    if ( !!game->comms && 0 != created
         && server_getGameIsConnected( game->server ) ) {
        comms_gatherPlayers( game->comms, xwe, created );
    }
#endif

    /* The board should be reused!!! PENDING(ehouse) */
    if ( !!game->board ) {
        board_destroy( game->board, xwe, XP_TRUE );
        game->board = NULL;
    }

#ifndef XWFEATURE_STANDALONE_ONLY
    if ( !!game->comms ) {
        comms_stop( game->comms
#ifdef XWFEATURE_RELAY
                    , xwe
#endif
                    );
        comms_destroy( game->comms, xwe );
        game->comms = NULL;
    }
#endif
    if ( !!game->model ) { 
        model_destroy( game->model, xwe );
        game->model = NULL;
    }
    if ( !!game->server ) {
        server_destroy( game->server );
        game->server = NULL;
    }
} /* game_dispose */

static void
disposePlayerInfoInt( MPFORMAL CurGameInfo* gi )
{
    for ( int ii = 0; ii < VSIZE(gi->players); ++ii ) {
        LocalPlayer* lp = &gi->players[ii];
        XP_FREEP( mpool, &lp->name );
        XP_FREEP( mpool, &lp->password );
        XP_FREEP( mpool, &lp->dictName );
    }
} /* disposePlayerInfoInt */

void
gi_disposePlayerInfo( MPFORMAL CurGameInfo* gi )
{
    disposePlayerInfoInt( MPPARM(mpool) gi );

    XP_FREEP( mpool, &gi->dictName );
} /* gi_disposePlayerInfo */

void
gi_copy( MPFORMAL CurGameInfo* destGI, const CurGameInfo* srcGI )
{
    replaceStringIfDifferent( mpool, &destGI->dictName, 
                              srcGI->dictName );
    XP_STRNCPY( destGI->isoCodeStr, srcGI->isoCodeStr, VSIZE(destGI->isoCodeStr)-1 );
    destGI->gameID = srcGI->gameID;
    destGI->gameSeconds = srcGI->gameSeconds;
    destGI->nPlayers = (XP_U8)srcGI->nPlayers;
    XP_U16 nPlayers = srcGI->nPlayers;
    destGI->boardSize = (XP_U8)srcGI->boardSize;
    destGI->traySize = srcGI->traySize;
    destGI->bingoMin = srcGI->bingoMin;
    destGI->serverRole = srcGI->serverRole;

    destGI->hintsNotAllowed = srcGI->hintsNotAllowed;
    destGI->timerEnabled = srcGI->timerEnabled;
    destGI->phoniesAction = srcGI->phoniesAction;
    destGI->allowPickTiles = srcGI->allowPickTiles;
    destGI->forceChannel = srcGI->forceChannel;
    destGI->inDuplicateMode = srcGI->inDuplicateMode;
    XP_LOGF( "%s: copied forceChannel: %d; inDuplicateMode: %d", __func__,
             destGI->forceChannel, destGI->inDuplicateMode );

    const LocalPlayer* srcPl;
    LocalPlayer* destPl;
    int ii;
    for ( srcPl = srcGI->players, destPl = destGI->players, ii = 0; 
          ii < nPlayers; ++srcPl, ++destPl, ++ii ) {

        replaceStringIfDifferent( mpool, &destPl->name, srcPl->name );
        replaceStringIfDifferent( mpool, &destPl->password, 
                                  srcPl->password );
        replaceStringIfDifferent( mpool, &destPl->dictName,
                                  srcPl->dictName );
        destPl->secondsUsed = srcPl->secondsUsed;
        destPl->robotIQ = srcPl->robotIQ;
        destPl->isLocal = srcPl->isLocal;
    }
} /* gi_copy */

#ifdef DEBUG
static XP_Bool
strEq( const XP_UCHAR* str1, const XP_UCHAR* str2 )
{
    if ( NULL == str1 ) {
        str1 = "";
    }
    if ( NULL == str2 ) {
        str2 = "";
    }
    return 0 == XP_STRCMP(str1, str2);
}

XP_Bool
gi_equal( const CurGameInfo* gi1, const CurGameInfo* gi2 )
{
    XP_Bool equal = XP_FALSE;
    int ii;
    for ( ii = 0; ; ++ii ) {
        switch ( ii ) {
        case 0:
            equal = gi1->gameID == gi2->gameID;
            break;
        case 1:
            equal = gi1->gameSeconds == gi2->gameSeconds;
            break;
        case 2:
            equal = gi1->nPlayers == gi2->nPlayers;
            break;
        case 3:
            equal = gi1->boardSize == gi2->boardSize;
            break;
        case 4:
            equal = gi1->traySize == gi2->traySize;
            break;
        case 5:
            equal = gi1->bingoMin == gi2->bingoMin;
            break;
        case 6:
            equal = gi1->forceChannel == gi2->forceChannel;
            break;
        case 7:
            equal = gi1->serverRole == gi2->serverRole;
            break;
        case 8:
            equal = gi1->hintsNotAllowed == gi2->hintsNotAllowed;
            break;
        case 9:
            equal = gi1->timerEnabled == gi2->timerEnabled;
            break;
        case 10:
            equal = gi1->allowPickTiles == gi2->allowPickTiles;
            break;
        case 11:
            equal = gi1->allowHintRect == gi2->allowHintRect;
            break;
        case 12:
            equal = gi1->inDuplicateMode == gi2->inDuplicateMode;
            break;
        case 13:
            equal = gi1->phoniesAction == gi2->phoniesAction;
            break;
        case 14:
            equal = gi1->confirmBTConnect == gi2->confirmBTConnect;
            break;
        case 15:
            equal = strEq( gi1->dictName, gi2->dictName );
            break;
        case 16:
            equal = strEq( gi1->isoCodeStr, gi2->isoCodeStr );
            break;
        case 17:
            for ( int jj = 0; equal && jj < gi1->nPlayers; ++jj ) {
                const LocalPlayer* lp1 = &gi1->players[jj];
                const LocalPlayer* lp2 = &gi2->players[jj];
                equal = strEq( lp1->name, lp2->name )
                    && strEq( lp1->password, lp2->password )
                    && strEq( lp1->dictName, lp2->dictName )
                    && lp1->secondsUsed == lp2->secondsUsed
                    && lp1->isLocal == lp2->isLocal
                    && lp1->robotIQ == lp2->robotIQ
                    ;
            }
            break;
        default:
            goto done;
            break;
        }
        if ( !equal ) {
            break;
        }
    }
 done:
    if ( !equal ) {
        XP_LOGFF( "exited when ii = %d", ii );
        LOGGI( gi1, "gi1" );
        LOGGI( gi2, "gi2" );
    }

    return equal;
}
#endif


void
gi_setNPlayers( CurGameInfo* gi, XWEnv xwe, XW_UtilCtxt* util,
                XP_U16 nTotal, XP_U16 nHere )
{
    LOGGI( gi, "before" );
    XP_ASSERT( nTotal <= MAX_NUM_PLAYERS );
    XP_ASSERT( nHere < nTotal );

    gi->nPlayers = nTotal;

    XP_U16 curLocal = 0;
    for ( XP_U16 ii = 0; ii < nTotal; ++ii ) {
        if ( gi->players[ii].isLocal ) {
            ++curLocal;
        }
    }

    if ( nHere != curLocal ) {
        /* This will happen when a device has more than one player. Not sure I
           handle that correctly, but don't assert for now. */
        XP_LOGFF( "nHere: %d; curLocal: %d; a problem?", nHere, curLocal );
        for ( XP_U16 ii = 0; ii < nTotal; ++ii ) {
            if ( !gi->players[ii].isLocal ) {
                gi->players[ii].isLocal = XP_TRUE;
                XP_LOGFF( "making player #%d local when wasn't before", ii );
                ++curLocal;
                XP_ASSERT( curLocal <= nHere );
                if ( curLocal == nHere ) {
                    break;
                }
            }
        }
    }

    for ( XP_U16 ii = 0; ii < nTotal; ++ii ) {
        LocalPlayer* lp = &gi->players[ii];
        if ( !lp->name || !lp->name[0] ) {
            XP_UCHAR name[32];
            XP_U16 len = VSIZE(name);
            dutil_getUsername( util_getDevUtilCtxt( util, xwe ),
                               xwe, ii, LP_IS_LOCAL(lp),
                               LP_IS_ROBOT(lp), name, &len );
            replaceStringIfDifferent( util->mpool, &lp->name, name );
        }
    }

    LOGGI( gi, "after" );
}

XP_U16
gi_countLocalPlayers( const CurGameInfo* gi, XP_Bool humanOnly )
{
    XP_U16 count = 0;
    XP_U16 nPlayers = gi->nPlayers;
    const LocalPlayer* lp = gi->players;
    while ( nPlayers-- ) {
        if ( lp->isLocal ) {
            if ( humanOnly && LP_IS_ROBOT(lp) ) {
                // skip
            } else {
                ++count;
            }
        }
        ++lp;
    }
    return count;
} /* gi_countLocalPlayers */

void
gi_readFromStream( MPFORMAL XWStreamCtxt* stream, CurGameInfo* gi )
{
    LocalPlayer* pl;
    XP_U16 ii;
    XP_UCHAR* str;
    XP_U16 strVersion = stream_getVersion( stream );
    XP_U16 nColsNBits;
    XP_ASSERT( 0 < strVersion );
#ifdef STREAM_VERS_BIGBOARD
    nColsNBits = STREAM_VERS_BIGBOARD > strVersion ? NUMCOLS_NBITS_4
        : NUMCOLS_NBITS_5;
#else
    nColsNBits = NUMCOLS_NBITS_4;
#endif

    str = stringFromStream( mpool, stream );
    replaceStringIfDifferent( mpool, &gi->dictName, str );
    XP_FREEP( mpool, &str );

    gi->nPlayers = (XP_U8)stream_getBits( stream, NPLAYERS_NBITS );
    gi->boardSize = (XP_U8)stream_getBits( stream, nColsNBits );
    if ( STREAM_VERS_NINETILES <= strVersion ) {
        gi->traySize = (XP_U8)stream_getBits( stream, NTILES_NBITS_9 );
        gi->bingoMin = (XP_U8)stream_getBits( stream, NTILES_NBITS_9 );
    } else {
        gi->traySize = gi->bingoMin = 7;
    }
    gi->serverRole = (DeviceRole)stream_getBits( stream, 2 );
    /* XP_LOGF( "%s: read serverRole of %d", __func__, gi->serverRole ); */
    gi->hintsNotAllowed = stream_getBits( stream, 1 );
    if ( strVersion < STREAM_VERS_ROBOTIQ ) {
        (void)stream_getBits( stream, 2 );
    }
    gi->phoniesAction = (XWPhoniesChoice)stream_getBits( stream, 2 );
    gi->timerEnabled = stream_getBits( stream, 1 );

    gi->inDuplicateMode = strVersion >= STREAM_VERS_DUPLICATE
        ? stream_getBits( stream, 1 )
        : XP_FALSE;
    if ( strVersion >= STREAM_VERS_41B4 ) {
        gi->allowPickTiles = stream_getBits( stream, 1 );
        gi->allowHintRect = stream_getBits( stream, 1 );
    } else {
        gi->allowPickTiles = XP_FALSE;
        gi->allowHintRect = XP_FALSE;
    }

    if ( strVersion >= STREAM_VERS_BLUETOOTH ) {
        gi->confirmBTConnect = stream_getBits( stream, 1 );
    } else {
        gi->confirmBTConnect = XP_TRUE; /* safe given all the 650s out there. */
    }

    if ( STREAM_VERS_MULTIADDR <= strVersion ) {
        gi->forceChannel = stream_getBits( stream, 2 );
    }
    gi->gameID = strVersion < STREAM_VERS_BLUETOOTH2 ? 
        stream_getU16( stream ) : stream_getU32( stream );
    // XP_LOGFF( "read forceChannel: %d for gid %X", gi->forceChannel, gi->gameID );

    if ( STREAM_VERS_GI_ISO <= strVersion ) {
        stringFromStreamHere( stream, gi->isoCodeStr, VSIZE(gi->isoCodeStr) );
    } else if ( STREAM_VERS_DICTLANG <= strVersion ) {
        XP_LangCode dictLang = stream_getU8( stream );
        const XP_UCHAR* isoCode = lcToLocale( dictLang );
        XP_ASSERT( !!isoCode );
        XP_STRNCPY( gi->isoCodeStr, isoCode, VSIZE(gi->isoCodeStr) );
        XP_LOGFF( "upgrading; faked isoCode: %s", gi->isoCodeStr );
    }

    if ( gi->timerEnabled || strVersion >= STREAM_VERS_GAMESECONDS ) {
        gi->gameSeconds = stream_getU16( stream );
    }

    for ( pl = gi->players, ii = 0; ii < gi->nPlayers; ++pl, ++ii ) {
        str = stringFromStream( mpool, stream );
        replaceStringIfDifferent( mpool, &pl->name, str );
        XP_FREEP( mpool, &str );

        str = stringFromStream( mpool, stream );
        replaceStringIfDifferent( mpool, &pl->password, str );
        XP_FREEP( mpool, &str );

        if ( strVersion >= STREAM_VERS_PLAYERDICTS ) {
            str = stringFromStream( mpool, stream );
            replaceStringIfDifferent( mpool, &pl->dictName, str );
            XP_FREEP( mpool, &str );
        }

        pl->secondsUsed = stream_getU16( stream );
        pl->robotIQ = ( strVersion < STREAM_VERS_ROBOTIQ )
            ? (XP_U8)stream_getBits( stream, 1 )
            : stream_getU8( stream );
        pl->isLocal = stream_getBits( stream, 1 );
    }
} /* gi_readFromStream */

void
gi_writeToStream( XWStreamCtxt* stream, const CurGameInfo* gi )
{
    XP_U16 nColsNBits;
#ifdef STREAM_VERS_BIGBOARD
    XP_U16 strVersion = stream_getVersion( stream );
    /* XP_LOGF( "%s: strVersion = 0x%x", __func__, strVersion ); */
    XP_ASSERT( STREAM_SAVE_PREVWORDS <= strVersion );
    nColsNBits = STREAM_VERS_BIGBOARD > strVersion ? NUMCOLS_NBITS_4
        : NUMCOLS_NBITS_5;
#else
    nColsNBits = NUMCOLS_NBITS_4;
#endif

    stringToStream( stream, gi->dictName );

    stream_putBits( stream, NPLAYERS_NBITS, gi->nPlayers );
    stream_putBits( stream, nColsNBits, gi->boardSize );

    if ( STREAM_VERS_NINETILES <= strVersion ) {
        XP_ASSERT( 0 < gi->traySize );
        stream_putBits( stream, NTILES_NBITS_9, gi->traySize );
        stream_putBits( stream, NTILES_NBITS_9, gi->bingoMin );
    } else {
        XP_LOGFF( "strVersion: %d so not writing traySize", strVersion );
    }
    stream_putBits( stream, 2, gi->serverRole );
    stream_putBits( stream, 1, gi->hintsNotAllowed );
    stream_putBits( stream, 2, gi->phoniesAction );
    stream_putBits( stream, 1, gi->timerEnabled );
    stream_putBits( stream, 1, gi->inDuplicateMode );
    stream_putBits( stream, 1, gi->allowPickTiles );
    stream_putBits( stream, 1, gi->allowHintRect );
    stream_putBits( stream, 1, gi->confirmBTConnect );
    stream_putBits( stream, 2, gi->forceChannel );
    // XP_LOGFF( "wrote forceChannel: %d for gid %X", gi->forceChannel, gi->gameID );

    if ( 0 ) {
#ifdef STREAM_VERS_BIGBOARD
    } else if ( STREAM_VERS_BIGBOARD <= strVersion ) {
        stream_putU32( stream, gi->gameID );
#endif
    } else {
        stream_putU16( stream, gi->gameID );
    }

    if ( STREAM_VERS_GI_ISO <= strVersion ) {
        stringToStream( stream, gi->isoCodeStr );
    } else {
        XP_LangCode code;
        if ( haveLocaleToLc( gi->isoCodeStr, &code ) ) {
            stream_putU8( stream, code );
        } else {
            XP_ASSERT( 0 );
        }
    }
    stream_putU16( stream, gi->gameSeconds );

    int ii;
    const LocalPlayer* pl;
    for ( pl = gi->players, ii = 0; ii < gi->nPlayers; ++pl, ++ii ) {
        stringToStream( stream, pl->name );
        stringToStream( stream, pl->password );
        stringToStream( stream, pl->dictName );
        stream_putU16( stream, pl->secondsUsed );
        stream_putU8( stream, pl->robotIQ );
        stream_putBits( stream, 1, pl->isLocal );
    }
} /* gi_writeToStream */

#ifdef XWFEATURE_CHANGEDICT
static void
gi_setDict( MPFORMAL CurGameInfo* gi, const DictionaryCtxt* dict )
{
    XP_U16 ii;
    const XP_UCHAR* name = dict_getName( dict );
    replaceStringIfDifferent( mpool, &gi->dictName, name );
    for ( ii = 0; ii < gi->nPlayers; ++ii ) {
        const LocalPlayer* pl = &gi->players[ii];
        XP_FREEP( mpool, &pl->dictName );
    }    
}
#endif

XP_Bool
player_hasPasswd( const LocalPlayer* player )
{
    XP_UCHAR* password = player->password;
    /*     XP_ASSERT( player->isLocal ); */
    return !!password && *password != '\0';
} /* player_hasPasswd */

XP_Bool
player_passwordMatches( const LocalPlayer* player, const XP_UCHAR* buf )
{
    XP_ASSERT( player->isLocal );

    return 0 == XP_STRCMP( player->password, (XP_UCHAR*)buf );
} /* player_passwordMatches */

XP_U16
player_timePenalty( CurGameInfo* gi, XP_U16 playerNum )
{
    XP_S16 seconds = (gi->gameSeconds / gi->nPlayers);
    LocalPlayer* player = gi->players + playerNum;
    XP_U16 result = 0;

    seconds -= player->secondsUsed;
    if ( seconds < 0 ) {
        seconds = -seconds;
        seconds += 59;
        result = (seconds/60) * 10;
    }
    return result;
} /* player_timePenalty */

#ifdef DEBUG
void
game_logGI( const CurGameInfo* gi, const char* msg, const char* func, int line )
{
    XP_LOGFF( "msg: %s from %s() line %d; gameID: %X", msg, func, line,
              !!gi ? gi->gameID:0 );
    if ( !!gi ) {
        XP_LOGF( "  nPlayers: %d", gi->nPlayers );
        for ( XP_U16 ii = 0; ii < gi->nPlayers; ++ii ) {
            const LocalPlayer* lp = &gi->players[ii];
            XP_LOGF( "  player[%d]: local: %d; robotIQ: %d; name: %s; dict: %s; pwd: %s", ii,
                     lp->isLocal, lp->robotIQ, lp->name, lp->dictName, lp->password );
        }
        XP_LOGF( "  forceChannel: %d", gi->forceChannel );
        XP_LOGF( "  serverRole: %d", gi->serverRole );
        XP_LOGF( "  dictName: %s", gi->dictName );
        XP_LOGF( "  isoCode: %s", gi->isoCodeStr );
    }
}
#endif

#ifdef CPLUS
}
#endif
