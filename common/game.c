/* -*- compile-command: "cd ../linux && make MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 2001-2009 by Eric House (xwords@eehouse.org).  All rights
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
    UtilVtable* vtable = util->vtable;
    XP_U16 nSlots = sizeof(vtable) / 4;
    while ( nSlots-- ) {
        void* fptr = ((void**)vtable)[nSlots];
        XP_ASSERT( !!fptr );
    }
} /* assertUtilOK */
#else
# define assertUtilOK(u)
#endif

static void
checkServerRole( CurGameInfo* gi, XP_U16* nPlayersHere, XP_U16* nPlayersTotal )
{
    if ( !!gi ) {
        XP_U16 ii, remoteCount = 0;

        if ( SERVER_ISSERVER == gi->serverRole ) {
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
        if ( SERVER_ISCLIENT == gi->serverRole ) {
            *nPlayersTotal = 0;
        } else {
            *nPlayersTotal = gi->nPlayers;
        }
    }
} /* checkServerRole */

void
game_makeNewGame( MPFORMAL XWGame* game, CurGameInfo* gi,
                  XW_UtilCtxt* util, DrawCtx* draw, 
                  XP_U16 gameID, CommonPrefs* cp,
                  const TransportProcs* procs )
{
    XP_U16 nPlayersHere, nPlayersTotal;

    assertUtilOK( util );
    checkServerRole( gi, &nPlayersHere, &nPlayersTotal );

    gi->gameID = gameID;

    game->model = model_make( MPPARM(mpool) (DictionaryCtxt*)NULL, util, 
                              gi->boardSize, gi->boardSize );

#ifndef XWFEATURE_STANDALONE_ONLY
    if ( !!procs && gi->serverRole != SERVER_STANDALONE ) {
        game->comms = comms_make( MPPARM(mpool) util,
                                  gi->serverRole != SERVER_ISCLIENT, 
                                  nPlayersHere, nPlayersTotal, procs );
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
                              draw, util );

    server_prefsChanged( game->server, cp );
    board_prefsChanged( game->board, cp );
} /* game_makeNewGame */

void
game_reset( MPFORMAL XWGame* game, CurGameInfo* gi, 
            XW_UtilCtxt* XP_UNUSED_STANDALONE(util), 
            XP_U16 gameID, CommonPrefs* cp, 
            const TransportProcs* procs )
{
    XP_U16 i;
    XP_U16 nPlayersHere, nPlayersTotal;

    XP_ASSERT( !!game->model );
    XP_ASSERT( !!gi );

    checkServerRole( gi, &nPlayersHere, &nPlayersTotal );
    while ( 0 == gameID ) {
        gameID = util_getCurSeconds( util );
    }
    gi->gameID = gameID;

#ifndef XWFEATURE_STANDALONE_ONLY
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
                                  nPlayersHere, nPlayersTotal, procs );
    }
#else
# ifdef DEBUG
    mpool = mpool;              /* quash unused formal warning */
# endif
#endif

    model_init( game->model, gi->boardSize, gi->boardSize );
    server_reset( game->server, 
#ifndef XWFEATURE_STANDALONE_ONLY
                  game->comms
#else
                  NULL
#endif
                  );
    board_reset( game->board );

    for ( i = 0; i < gi->nPlayers; ++i ) {
        gi->players[i].secondsUsed = 0;
    }

    server_prefsChanged( game->server, cp );
    board_prefsChanged( game->board, cp );
} /* game_reset */

XP_Bool
game_makeFromStream( MPFORMAL XWStreamCtxt* stream, XWGame* game, 
                     CurGameInfo* gi, DictionaryCtxt* dict, 
                     XW_UtilCtxt* util, DrawCtx* draw, CommonPrefs* cp, 
                     const TransportProcs* procs )
{
    XP_Bool success = XP_FALSE;
    XP_U8 strVersion;
#ifndef XWFEATURE_STANDALONE_ONLY
    XP_Bool hasComms;
#endif
    strVersion = stream_getU8( stream );
    XP_DEBUGF( "strVersion = %d", (XP_U16)strVersion );

    if ( strVersion <= CUR_STREAM_VERS ) {
        stream_setVersion( stream, strVersion );

        gi_readFromStream( MPPARM(mpool) stream, gi );

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
                                                procs );
        } else {
            game->comms = NULL;
        }

        XP_ASSERT( !!dict );
        game->model = model_makeFromStream( MPPARM(mpool) stream, dict, util );

        game->server = server_makeFromStream( MPPARM(mpool) stream, 
                                              game->model, game->comms, 
                                              util, gi->nPlayers );

        game->board = board_makeFromStream( MPPARM(mpool) stream, game->model, 
                                            game->server, draw, util, 
                                            gi->nPlayers );
        server_prefsChanged( game->server, cp );
        board_prefsChanged( game->board, cp );
        draw_dictChanged( draw, dict );
        success = XP_TRUE;
    } else {
        XP_LOGF( "%s: aborting; stream version too new!", __func__ );
    }
    return success;
} /* game_makeFromStream */

void
game_saveToStream( const XWGame* game, const CurGameInfo* gi, 
                   XWStreamCtxt* stream )
{
    stream_putU8( stream, CUR_STREAM_VERS );

    gi_writeToStream( stream, gi );

    stream_putU8( stream, (XP_U8)!!game->comms );
#ifdef XWFEATURE_STANDALONE_ONLY
    XP_ASSERT( !game->comms );
#endif
    if ( !!game->comms ) {
        comms_writeToStream( game->comms, stream );
    }

    model_writeToStream( game->model, stream );
    server_writeToStream( game->server, stream );
    board_writeToStream( game->board, stream );
} /* game_saveToStream */

void
game_dispose( XWGame* game )
{
    /* The board should be reused!!! PENDING(ehouse) */
    if ( !!game->board ) { 
        board_destroy( game->board ); 
        game->board = NULL;
    }

#ifndef XWFEATURE_STANDALONE_ONLY
    if ( !!game->comms ) {
        comms_destroy( game->comms );
        game->comms = NULL;
    }
#endif
    if ( !!game->model ) { 
        DictionaryCtxt* dict = model_getDictionary( game->model );
        if ( !!dict ) { 
            dict_destroy( dict );
        }
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
    XP_U16 i;

    XP_MEMSET( gi, 0, sizeof(*gi) );
    gi->serverRole = SERVER_STANDALONE;
    gi->nPlayers = 2;
    gi->boardSize = 15;
    gi->robotSmartness = SMART_ROBOT;
    gi->gameSeconds = 25 * 60; /* 25 minute game is common? */
    
    gi->confirmBTConnect = XP_TRUE;

    for ( i = 0; i < MAX_NUM_PLAYERS; ++i ) {
        XP_UCHAR buf[20];
        LocalPlayer* fp = &gi->players[i];

        if ( !!nameTemplate ) {
            XP_SNPRINTF( buf, sizeof(buf), nameTemplate, i+1 );
            XP_ASSERT( fp->name == NULL );
            fp->name = copyString( mpool, buf );
        }

        fp->isRobot = (i == 0); /* one robot */
        fp->isLocal = XP_TRUE;
        fp->secondsUsed = 0;
    }
} /* game_initPlayerInfo */

static void
disposePlayerInfoInt( MPFORMAL CurGameInfo* gi )
{
    XP_U16 i;
    LocalPlayer* lp;

    for ( lp = gi->players, i = 0; i < MAX_NUM_PLAYERS; ++lp, ++i ) {
        if ( !!lp->name ) {
            XP_FREE( mpool, lp->name );
            lp->name = (XP_UCHAR*)NULL;
        }
        if ( !!lp->password ) {
            XP_FREE( mpool, lp->password );
            lp->password = (XP_UCHAR*)NULL;
        }
    }
} /* disposePlayerInfoInt */

void
gi_disposePlayerInfo( MPFORMAL CurGameInfo* gi )
{
    disposePlayerInfoInt( MPPARM(mpool) gi );

    if ( !!gi->dictName ) {
        XP_FREE( mpool, gi->dictName );
        gi->dictName = (XP_UCHAR*)NULL;
    }
} /* gi_disposePlayerInfo */

void
gi_copy( MPFORMAL CurGameInfo* destGI, const CurGameInfo* srcGI )
{
    XP_U16 nPlayers, i;
    const LocalPlayer* srcPl;
    LocalPlayer* destPl;

    replaceStringIfDifferent( mpool, &destGI->dictName, 
                              srcGI->dictName );

    destGI->gameID = srcGI->gameID;
    destGI->gameSeconds = srcGI->gameSeconds;
    destGI->nPlayers = (XP_U8)srcGI->nPlayers;
    nPlayers = srcGI->nPlayers;
    destGI->boardSize = (XP_U8)srcGI->boardSize;
    destGI->serverRole = srcGI->serverRole;

    destGI->hintsNotAllowed = srcGI->hintsNotAllowed;
    destGI->timerEnabled = srcGI->timerEnabled;
    destGI->robotSmartness = (XP_U8)srcGI->robotSmartness;
    destGI->phoniesAction = srcGI->phoniesAction;
    destGI->allowPickTiles = srcGI->allowPickTiles;

    for ( srcPl = srcGI->players, destPl = destGI->players, i = 0; 
          i < nPlayers; ++srcPl, ++destPl, ++i ) {

        replaceStringIfDifferent( mpool, &destPl->name, srcPl->name );
        replaceStringIfDifferent( mpool, &destPl->password, 
                                  srcPl->password );
        destPl->secondsUsed = srcPl->secondsUsed;
        destPl->isRobot = srcPl->isRobot;
        destPl->isLocal = srcPl->isLocal;
    }
} /* gi_copy */

XP_U16
gi_countLocalHumans( const CurGameInfo* gi )
{
    XP_U16 count = 0;
    XP_U16 nPlayers = gi->nPlayers;
    const LocalPlayer* lp = gi->players;
    while ( nPlayers-- ) {
        if ( lp->isLocal && !lp->isRobot ) {
            ++count;
        }
        ++lp;
    }
    return count;
} /* gi_countLocalHumans */

void
gi_readFromStream( MPFORMAL XWStreamCtxt* stream, CurGameInfo* gi )
{
    LocalPlayer* pl;
    XP_U16 i;
    XP_UCHAR* str;
    XP_U16 strVersion = stream_getVersion( stream );

    str = stringFromStream( mpool, stream );
    replaceStringIfDifferent( mpool, &gi->dictName, str );
    if ( !!str ) {
        XP_FREE( mpool, str );
    }

    gi->nPlayers = (XP_U8)stream_getBits( stream, NPLAYERS_NBITS );
    gi->boardSize = (XP_U8)stream_getBits( stream, 4 );
    gi->serverRole = (DeviceRole)stream_getBits( stream, 2 );
    gi->hintsNotAllowed = stream_getBits( stream, 1 );
    gi->robotSmartness = (XP_U8)stream_getBits( stream, 2 );
    gi->phoniesAction = (XWPhoniesChoice)stream_getBits( stream, 2 );
    gi->timerEnabled = stream_getBits( stream, 1 );

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

    gi->gameID = stream_getU16( stream );
    if ( gi->timerEnabled ) {
        gi->gameSeconds = stream_getU16( stream );
    }

    for ( pl = gi->players, i = 0; i < gi->nPlayers; ++pl, ++i ) {
        str = stringFromStream( mpool, stream );
        replaceStringIfDifferent( mpool, &pl->name, str );
        if ( !!str ) {
            XP_FREE( mpool, str );
        }

        str = stringFromStream( mpool, stream );
        replaceStringIfDifferent( mpool, &pl->password, str );
        if ( !!str ) {
            XP_FREE( mpool, str );
        }

        pl->secondsUsed = stream_getU16( stream );
        pl->isRobot = stream_getBits( stream, 1 );
        pl->isLocal = stream_getBits( stream, 1 );
    }
} /* gi_readFromStream */

void
gi_writeToStream( XWStreamCtxt* stream, const CurGameInfo* gi )
{
    const LocalPlayer* pl;
    XP_U16 i;

    stringToStream( stream, gi->dictName );

    stream_putBits( stream, NPLAYERS_NBITS, gi->nPlayers );
    stream_putBits( stream, 4, gi->boardSize );
    stream_putBits( stream, 2, gi->serverRole );
    stream_putBits( stream, 1, gi->hintsNotAllowed );
    stream_putBits( stream, 2, gi->robotSmartness );
    stream_putBits( stream, 2, gi->phoniesAction );
    stream_putBits( stream, 1, gi->timerEnabled );
    stream_putBits( stream, 1, gi->allowPickTiles );
    stream_putBits( stream, 1, gi->allowHintRect );
    stream_putBits( stream, 1, gi->confirmBTConnect );

    stream_putU16( stream, gi->gameID );
    if ( gi->timerEnabled) {
        stream_putU16( stream, gi->gameSeconds );
    }
    
    for ( pl = gi->players, i = 0; i < gi->nPlayers; ++pl, ++i ) {
        stringToStream( stream, pl->name );
        stringToStream( stream, pl->password );
        stream_putU16( stream, pl->secondsUsed );
        stream_putBits( stream, 1, pl->isRobot );
        stream_putBits( stream, 1, pl->isLocal );
    }
} /* gi_writeToStream */

XP_Bool
player_hasPasswd( LocalPlayer* player )
{
    XP_UCHAR* password = player->password;
    /*     XP_ASSERT( player->isLocal ); */
    return !!password && *password != '\0';
} /* player_hasPasswd */

XP_Bool
player_passwordMatches( LocalPlayer* player, XP_U8* buf, XP_U16 len )
{
    XP_ASSERT( player->isLocal );

    return (XP_STRLEN(player->password) == len)
        && (0 == XP_STRNCMP( player->password, (XP_UCHAR*)buf, len ));
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

#ifdef CPLUS
}
#endif
