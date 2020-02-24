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

#ifdef CPLUS
extern "C" {
#endif

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
makeGameID( XW_UtilCtxt* util )
{
    XP_U32 gameID = 0;
    assertUtilOK( util );
    while ( 0 == gameID ) {
        gameID = dutil_getCurSeconds( util_getDevUtilCtxt( util ) );
    }
    return gameID;
}

static void
timerChangeListener( void* data, const XP_U32 gameID,
                     XP_S32 oldVal, XP_S32 newVal )
{
    XWGame* game = (XWGame*)data;
    XP_ASSERT( game->util->gameInfo->gameID == gameID );
    XP_LOGF( "%s(oldVal=%d, newVal=%d, id=%d)", __func__, oldVal, newVal, gameID );
    dutil_onDupTimerChanged( util_getDevUtilCtxt( game->util ),
                             gameID, oldVal, newVal );
}

static void
setListeners( XWGame* game, const CommonPrefs* cp )
{
    server_prefsChanged( game->server, cp );
    board_prefsChanged( game->board, cp );
    server_setTimerChangeListener( game->server, timerChangeListener, game );
}

void
game_makeNewGame( MPFORMAL XWGame* game, CurGameInfo* gi,
                  XW_UtilCtxt* util, DrawCtx* draw, 
                  const CommonPrefs* cp, const TransportProcs* procs
#ifdef SET_GAMESEED
                  ,XP_U16 gameSeed 
#endif
                  )
{
#ifndef XWFEATURE_STANDALONE_ONLY
    XP_U16 nPlayersHere = 0;
    XP_U16 nPlayersTotal = 0;
    checkServerRole( gi, &nPlayersHere, &nPlayersTotal );
#endif
    assertUtilOK( util );

    if ( 0 == gi->gameID ) {
        gi->gameID = makeGameID( util );
    }

    game->util = util;

    game->model = model_make( MPPARM(mpool) (DictionaryCtxt*)NULL, NULL, util, 
                              gi->boardSize );

#ifndef XWFEATURE_STANDALONE_ONLY
    if ( gi->serverRole != SERVER_STANDALONE ) {
        game->comms = comms_make( MPPARM(mpool) util,
                                  gi->serverRole != SERVER_ISCLIENT, 
                                  nPlayersHere, nPlayersTotal, 
                                  procs, gi->forceChannel
#ifdef SET_GAMESEED
                                  , gameSeed
#endif
                                  );
    } else {
        game->comms = (CommsCtxt*)NULL;
    }
#endif
    game->server = server_make( MPPARM(mpool) game->model, 
#ifndef XWFEATURE_STANDALONE_ONLY
                                game->comms, 
#else
                                (CommsCtxt*)NULL,
#endif
                                util );
    game->board = board_make( MPPARM(mpool) game->model, game->server, 
                              NULL, util );
    board_setCallbacks( game->board );

    board_setDraw( game->board, draw );
    setListeners( game, cp );
} /* game_makeNewGame */

XP_Bool
game_reset( MPFORMAL XWGame* game, CurGameInfo* gi, XW_UtilCtxt* util,
            CommonPrefs* cp, const TransportProcs* procs )
{
    XP_ASSERT( util == game->util );
    XP_Bool result = XP_FALSE;
    XP_U16 ii;

    if ( !!game->model ) {
        XP_ASSERT( !!game->model );
        XP_ASSERT( !!gi );

        gi->gameID = makeGameID( util );

#ifndef XWFEATURE_STANDALONE_ONLY
        XP_U16 nPlayersHere = 0;
        XP_U16 nPlayersTotal = 0;
        checkServerRole( gi, &nPlayersHere, &nPlayersTotal );

        if ( !!game->comms ) {
            if ( gi->serverRole == SERVER_STANDALONE ) {
                comms_destroy( game->comms );
                game->comms = NULL;
            } else {
                comms_reset( game->comms, gi->serverRole != SERVER_ISCLIENT,
                             nPlayersHere, nPlayersTotal );
            }
        } else if ( gi->serverRole != SERVER_STANDALONE ) {
            game->comms = comms_make( MPPARM(mpool) util,
                                      gi->serverRole != SERVER_ISCLIENT, 
                                      nPlayersHere, nPlayersTotal, procs,
                                      gi->forceChannel
#ifdef SET_GAMESEED
                                      , 0
#endif
                                      );
        }
#else
# ifdef DEBUG
        mpool = mpool;              /* quash unused formal warning */
# endif
#endif

        model_setSize( game->model, gi->boardSize );
        server_reset( game->server, 
#ifndef XWFEATURE_STANDALONE_ONLY
                      game->comms
#else
                      NULL
#endif
                      );
        board_reset( game->board );

        for ( ii = 0; ii < gi->nPlayers; ++ii ) {
            gi->players[ii].secondsUsed = 0;
        }

        setListeners( game, cp );
        result = XP_TRUE;
    }
    return result;
} /* game_reset */

#ifdef XWFEATURE_CHANGEDICT
void
game_changeDict( MPFORMAL XWGame* game, CurGameInfo* gi, DictionaryCtxt* dict )
{
    model_setDictionary( game->model, dict );
    gi_setDict( MPPARM(mpool) gi, dict );
    server_resetEngines( game->server );
}
#endif

XP_Bool
game_makeFromStream( MPFORMAL XWStreamCtxt* stream, XWGame* game, 
                     CurGameInfo* gi, DictionaryCtxt* dict, 
                     const PlayerDicts* dicts, XW_UtilCtxt* util, 
                     DrawCtx* draw, CommonPrefs* cp, 
                     const TransportProcs* procs )
{
    XP_Bool success = XP_FALSE;
    XP_U8 strVersion;
#ifndef XWFEATURE_STANDALONE_ONLY
    XP_Bool hasComms;
#endif
    strVersion = stream_getU8( stream );
    XP_DEBUGF( "%s: strVersion = 0x%x", __func__, (XP_U16)strVersion );

    if ( strVersion > CUR_STREAM_VERS ) {
        XP_LOGF( "%s: aborting; stream version too new (%d > %d)!", __func__, 
                 strVersion, CUR_STREAM_VERS );
    } else {
        do { /* do..while so can break */
            stream_setVersion( stream, strVersion );

            gi_readFromStream( MPPARM(mpool) stream, gi );
            if ( !game ) {
                success = XP_TRUE;
                break;
            } else if ( stream_getSize(stream) == 0 ) {
                XP_LOGF( "%s: gi was all we got; failing.", __func__ );
                break;
            }
            game->util = util;

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
                hasComms = stream_getU8( stream );
            }

            if ( hasComms ) {
                game->comms = comms_makeFromStream( MPPARM(mpool) stream, util, 
                                                    procs, gi->forceChannel );
            } else {
                game->comms = NULL;
            }

            game->model = model_makeFromStream( MPPARM(mpool) stream, dict, 
                                                dicts, util );

            game->server = server_makeFromStream( MPPARM(mpool) stream, 
                                                  game->model, game->comms, 
                                                  util, gi->nPlayers );

            game->board = board_makeFromStream( MPPARM(mpool) stream, 
                                                game->model, game->server, 
                                                NULL, util, gi->nPlayers );
            setListeners( game, cp );
            board_setDraw( game->board, draw );
            success = XP_TRUE;
        } while( XP_FALSE );
    }
    return success;
} /* game_makeFromStream */

void
game_saveNewGame( MPFORMAL const CurGameInfo* gi, XW_UtilCtxt* util,
                  const CommonPrefs* cp, XWStreamCtxt* out )
{
    XWGame newGame = {0};
    CurGameInfo newGI = {0};
    gi_copy( MPPARM(mpool) &newGI, gi );

    game_makeNewGame( MPPARM(mpool) &newGame, &newGI, util, 
                      NULL, /* DrawCtx*,  */
                      cp, NULL /* TransportProcs* procs */
#ifdef SET_GAMESEED
                      ,0 
#endif
                      );

    game_saveToStream( &newGame, &newGI, out, 1 );
    game_saveSucceeded( &newGame, 1 );
    game_dispose( &newGame );
    gi_disposePlayerInfo( MPPARM(mpool) &newGI );
}

void
game_saveToStream( const XWGame* game, const CurGameInfo* gi, 
                   XWStreamCtxt* stream, XP_U16 saveToken )
{
    stream_putU8( stream, CUR_STREAM_VERS );
    stream_setVersion( stream, CUR_STREAM_VERS );

    gi_writeToStream( stream, gi );

    if ( !!game ) {
        XP_ASSERT( 0 != saveToken );
        stream_putU8( stream, (XP_U8)!!game->comms );
#ifdef XWFEATURE_STANDALONE_ONLY
        XP_ASSERT( !game->comms );
#endif
        if ( !!game->comms ) {
            comms_writeToStream( game->comms, stream, saveToken );
        }

        model_writeToStream( game->model, stream );
        server_writeToStream( game->server, stream );
        board_writeToStream( game->board, stream );
    }
} /* game_saveToStream */

void
game_saveSucceeded( const XWGame* game, XP_U16 saveToken )
{
    if ( !!game->comms ) {
        comms_saveSucceeded( game->comms, saveToken );
    }
}

XP_Bool
game_receiveMessage( XWGame* game, XWStreamCtxt* stream,
                     const CommsAddrRec* retAddr )
{
    ServerCtxt* server = game->server;
    CommsMsgState commsState;
    XP_Bool result = comms_checkIncomingStream( game->comms, stream, retAddr,
                                                &commsState );
    if ( result ) {
        (void)server_do( server );

        result = server_receiveMessage( server, stream );
    }
    comms_msgProcessed( game->comms, &commsState, !result );

    if ( result ) {
        /* in case MORE work's pending.  Multiple calls are required in at
           least one case, where I'm a host handling client registration *AND*
           I'm a robot.  Only one server_do and I'll never make that first
           robot move.  That's because comms can't detect a duplicate initial
           packet (in validateInitialMessage()). */
        for ( int ii = 0; ii < 5; ++ii ) {
            (void)server_do( server );
        }
    }

    return result;
}

void
game_getState( const XWGame* game, GameStateInfo* gsi )
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
    gsi->canTrade = board_canTrade( board );
    gsi->nPendingMessages = !!game->comms ? 
        comms_countPendingPackets(game->comms) : 0;

    gsi->canPause = server_canPause( server );
    gsi->canUnpause = server_canUnpause( server );
}

XP_Bool
game_getIsServer( const XWGame* game )
{
    XP_Bool result = comms_getIsServer( game->comms );
    return result;
}

void
game_dispose( XWGame* game )
{
    /* The board should be reused!!! PENDING(ehouse) */
    if ( !!game->board ) {
        board_destroy( game->board, XP_TRUE );
        game->board = NULL;
    }

#ifndef XWFEATURE_STANDALONE_ONLY
    if ( !!game->comms ) {
        comms_stop( game->comms );
        comms_destroy( game->comms );
        game->comms = NULL;
    }
#endif
    if ( !!game->model ) { 
        model_destroy( game->model );
        game->model = NULL;
    }
    if ( !!game->server ) {
        server_destroy( game->server ); 
        game->server = NULL;
    }
} /* game_dispose */

void
gi_initPlayerInfo( MPFORMAL CurGameInfo* gi, const XP_UCHAR* nameTemplate )
{
    XP_U16 ii;

    XP_MEMSET( gi, 0, sizeof(*gi) );
    gi->serverRole = SERVER_STANDALONE;
    gi->nPlayers = 2;
    gi->boardSize = 15;
    gi->gameSeconds = 25 * 60; /* 25 minute game is common? */
    
    gi->confirmBTConnect = XP_TRUE;

    for ( ii = 0; ii < MAX_NUM_PLAYERS; ++ii ) {
        XP_UCHAR buf[20];
        LocalPlayer* fp = &gi->players[ii];

        if ( !!nameTemplate ) {
            XP_SNPRINTF( buf, sizeof(buf), nameTemplate, ii+1 );
            XP_ASSERT( fp->name == NULL );
            fp->name = copyString( mpool, buf );
        }

        fp->robotIQ = (ii == 0) ? 1 : 0; /* one robot */
        fp->isLocal = XP_TRUE;
        fp->secondsUsed = 0;
    }
} /* game_initPlayerInfo */

static void
disposePlayerInfoInt( MPFORMAL CurGameInfo* gi )
{
    XP_U16 ii;
    LocalPlayer* lp;

    for ( lp = gi->players, ii = 0; ii < MAX_NUM_PLAYERS; ++lp, ++ii ) {
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
    XP_U16 nPlayers, ii;
    const LocalPlayer* srcPl;
    LocalPlayer* destPl;

    replaceStringIfDifferent( mpool, &destGI->dictName, 
                              srcGI->dictName );

    destGI->dictLang = srcGI->dictLang;
    destGI->gameID = srcGI->gameID;
    destGI->gameSeconds = srcGI->gameSeconds;
    destGI->nPlayers = (XP_U8)srcGI->nPlayers;
    nPlayers = srcGI->nPlayers;
    destGI->boardSize = (XP_U8)srcGI->boardSize;
    destGI->serverRole = srcGI->serverRole;

    destGI->hintsNotAllowed = srcGI->hintsNotAllowed;
    destGI->timerEnabled = srcGI->timerEnabled;
    destGI->phoniesAction = srcGI->phoniesAction;
    destGI->allowPickTiles = srcGI->allowPickTiles;
    destGI->forceChannel = srcGI->forceChannel;
    destGI->inDuplicateMode = srcGI->inDuplicateMode;
    XP_LOGF( "%s: copied forceChannel: %d; inDuplicateMode: %d", __func__,
             destGI->forceChannel, destGI->inDuplicateMode );

    for ( srcPl = srcGI->players, destPl = destGI->players, ii = 0; 
          ii < nPlayers; ++srcPl, ++destPl, ++ii ) {

        replaceStringIfDifferent( mpool, &destPl->name, srcPl->name );
        replaceStringIfDifferent( mpool, &destPl->password, 
                                  srcPl->password );
        destPl->secondsUsed = srcPl->secondsUsed;
        destPl->robotIQ = srcPl->robotIQ;
        destPl->isLocal = srcPl->isLocal;
    }
} /* gi_copy */

void
gi_setNPlayers( CurGameInfo* gi, XP_U16 nTotal, XP_U16 nHere )
{
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
        /* This will happen when a device has more than on player. Not sure I
           handle that correctly, but don't assert for now. */
        XP_LOGFF( "nHere: %d; curLocal: %d; a problem?", nHere, curLocal );
        /* for ( XP_U16 ii = 0; ii < nTotal; ++ii ) { */
        /*     gi->players[ii].isLocal = ii < nHere; */
        /* } */
    }
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
        /* XP_LOGF( "%s: loaded forceChannel: %d", __func__, gi->forceChannel ); */
    }

    gi->gameID = strVersion < STREAM_VERS_BLUETOOTH2 ? 
        stream_getU16( stream ) : stream_getU32( stream );
    gi->dictLang =
        strVersion >= STREAM_VERS_DICTLANG ? stream_getU8( stream ) : 0;
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
    const LocalPlayer* pl;
    XP_U16 ii;
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
    stream_putBits( stream, 2, gi->serverRole );
    stream_putBits( stream, 1, gi->hintsNotAllowed );
    stream_putBits( stream, 2, gi->phoniesAction );
    stream_putBits( stream, 1, gi->timerEnabled );
    stream_putBits( stream, 1, gi->inDuplicateMode );
    stream_putBits( stream, 1, gi->allowPickTiles );
    stream_putBits( stream, 1, gi->allowHintRect );
    stream_putBits( stream, 1, gi->confirmBTConnect );
    stream_putBits( stream, 2, gi->forceChannel );
    /* XP_LOGF( "%s: wrote forceChannel: %d", __func__, gi->forceChannel ); */

    if ( 0 ) {
#ifdef STREAM_VERS_BIGBOARD
    } else if ( STREAM_VERS_BIGBOARD <= strVersion ) {
        stream_putU32( stream, gi->gameID );
#endif
    } else {
        stream_putU16( stream, gi->gameID );
    }

    stream_putU8( stream, gi->dictLang );
    stream_putU16( stream, gi->gameSeconds );

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

void
game_logGI( const CurGameInfo* gi, const char* msg )
{
    XP_LOGFF( "msg: %s", msg );

    XP_LOGF( "  nPlayers: %d", gi->nPlayers );
    for ( XP_U16 ii = 0; ii < gi->nPlayers; ++ii ) {
        const LocalPlayer* lp = &gi->players[ii];
        XP_LOGF( "  player[%d]: local: %d; robotIQ: %d; name: %s", ii,
                 lp->isLocal, lp->robotIQ, lp->name );
    }
    XP_LOGF( "  forceChannel: %d", gi->forceChannel );
    XP_LOGF( "  serverRole: %d", gi->serverRole );
    XP_LOGF( "  gameID: %d", gi->gameID );
    XP_LOGF( "  dictName: %s", gi->dictName );
}

#ifdef CPLUS
}
#endif
