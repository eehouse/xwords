/* -*-mode: C; fill-column: 76; c-basic-offset: 4; -*- */
/* 
 * Copyright 2001 by Eric House (fixin@peak.org).  All rights reserved.
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
checkServerRole( CurGameInfo* gi )
{
    if ( !!gi ) {

        if ( gi->serverRole != SERVER_ISCLIENT ) {
            XP_Bool standAlone = gi->serverRole == SERVER_STANDALONE;
            XP_U16 i, remoteCount = 0;

            for ( i = 0; i < gi->nPlayers; ++i ) {
                LocalPlayer* player = &gi->players[i];
                if ( !player->isLocal ) {
                    ++remoteCount;
                    if ( standAlone ) {
                        player->isLocal = XP_TRUE;
                    }
                }
            }
            if ( remoteCount == 0 ) {
                gi->serverRole = SERVER_STANDALONE;
            }
        }
    }
} /* checkServerRole */

void
game_makeNewGame( MPFORMAL XWGame* game, CurGameInfo* gi,
                  XW_UtilCtxt* util, DrawCtx* draw, 
                  XP_U16 gameID, CommonPrefs* cp,
                  TransportSend sendproc, void* closure )
{
    assertUtilOK( util );
    checkServerRole( gi );

    gi->gameID = gameID;

    game->model = model_make( MPPARM(mpool) (DictionaryCtxt*)NULL, util, 
                              gi->boardSize, gi->boardSize );

#ifndef XWFEATURE_STANDALONE_ONLY
    if ( !!sendproc && gi->serverRole != SERVER_STANDALONE ) {
        game->comms = comms_make( MPPARM(mpool) util,
                                  gi->serverRole != SERVER_ISCLIENT, 
                                  sendproc, closure );
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
game_reset( MPFORMAL XWGame* game, CurGameInfo* gi, XP_U16 gameID,
            CommonPrefs* cp, TransportSend sendproc, void* closure )
{
    XP_U16 i;

    XP_ASSERT( !!game->model );
    XP_ASSERT( !!gi );

    checkServerRole( gi );
    gi->gameID = gameID;

#ifndef XWFEATURE_STANDALONE_ONLY
    if ( !!game->comms ) {
        if ( gi->serverRole == SERVER_STANDALONE ) {
            comms_destroy( game->comms );
            game->comms = NULL;
        } else {
            comms_reset( game->comms, gi->serverRole != SERVER_ISCLIENT );
        }
    }
#endif

    model_init( game->model, gi->boardSize, gi->boardSize );
    server_reset( game->server, game->comms );
    board_reset( game->board );

    for ( i = 0; i < gi->nPlayers; ++i ) {
        LocalPlayer* player = &gi->players[i];
        XP_Bool isLocal = player->isLocal;
        if ( !isLocal ) {
            player->name = (XP_UCHAR*)NULL;
        }
        player->secondsUsed = 0;
    }

    server_prefsChanged( game->server, cp );
    board_prefsChanged( game->board, cp );
} /* game_reset */

void
game_makeFromStream( MPFORMAL XWStreamCtxt* stream, XWGame* game, 
                     CurGameInfo* gi, DictionaryCtxt* dict, 
                     XW_UtilCtxt* util, DrawCtx* draw, CommonPrefs* cp, 
                     TransportSend sendProc, void* closure )
{
    XP_U8 strVersion;
#ifndef XWFEATURE_STANDALONE_ONLY
    XP_Bool hasComms;
#endif
    strVersion = stream_getU8( stream );
    XP_DEBUGF( "strVersion = %d", (XP_U16)strVersion );

    stream_setVersion( stream, strVersion );

    gi_readFromStream( MPPARM(mpool) stream, gi );

#ifndef XWFEATURE_STANDALONE_ONLY
    hasComms = stream_getU8( stream );
    game->comms = hasComms? 
        comms_makeFromStream( MPPARM(mpool) stream, util, sendProc, closure ):
        (CommsCtxt*)NULL;
#endif
    game->model = model_makeFromStream( MPPARM(mpool) stream, dict, util );

    game->server = server_makeFromStream( MPPARM(mpool) stream, 
                                          game->model, 
#ifndef XWFEATURE_STANDALONE_ONLY
                                          game->comms, 
#else
                                          (CommsCtxt*)NULL,
#endif
                                          util, gi->nPlayers );

    game->board = board_makeFromStream( MPPARM(mpool) stream, game->model, 
                                        game->server, draw, util, 
                                        gi->nPlayers );
    server_prefsChanged( game->server, cp );
    board_prefsChanged( game->board, cp );
} /* game_makeFromStream */

void
game_saveToStream( XWGame* game, CurGameInfo* gi, XWStreamCtxt* stream )
{
    stream_putU8( stream, CUR_STREAM_VERS );

    gi_writeToStream( stream, gi );

#ifndef XWFEATURE_STANDALONE_ONLY
    stream_putU8( stream, (XP_U8)!!game->comms );
    if ( !!game->comms ) {
        comms_writeToStream( game->comms, stream );
    }
#endif

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
    }

#ifndef XWFEATURE_STANDALONE_ONLY
    if ( !!game->comms ) { 
        comms_destroy( game->comms ); 
    }
#endif
    if ( !!game->model ) { 
        DictionaryCtxt* dict = model_getDictionary( game->model );
        if ( !!dict ) { 
            dict_destroy( dict );
        }
        model_destroy( game->model );
    }
    if ( !!game->server ) {
        server_destroy( game->server ); 
    }
} /* game_dispose */

void
gi_initPlayerInfo( MPFORMAL CurGameInfo* gi, XP_UCHAR* nameTemplate )
{
    XP_U16 i;

    XP_MEMSET( gi, 0, sizeof(*gi) );
    gi->nPlayers = 2;
    gi->boardSize = 15;
    gi->robotSmartness = SMART_ROBOT;
    gi->gameSeconds = 25 * 60;	/* 25 minute game is common? */

    for ( i = 0; i < MAX_NUM_PLAYERS; ++i ) {
        XP_UCHAR buf[20];
        LocalPlayer* fp = &gi->players[i];

        if ( !!nameTemplate ) {
            XP_SNPRINTF( buf, sizeof(buf), nameTemplate, i+1 );
            XP_ASSERT( fp->name == NULL );
            fp->name = copyString( MPPARM(mpool) buf );
        }

        fp->isRobot = (i == 0);	/* one robot */
        fp->isLocal = XP_TRUE;
        fp->secondsUsed = 0;
    }
} /* game_initPlayerInfo */

static void
disposePlayerInfoInt( MPFORMAL CurGameInfo* gi, XP_Bool namesToo )
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
    disposePlayerInfoInt( MPPARM(mpool) gi, XP_TRUE );

    if ( !!gi->dictName ) {
        XP_FREE( mpool, gi->dictName );
        gi->dictName = (XP_UCHAR*)NULL;
    }
} /* gi_disposePlayerInfo */

void
gi_copy( MPFORMAL CurGameInfo* destGI, CurGameInfo* srcGI )
{
    XP_U16 nPlayers, i;
    LocalPlayer* srcPl;
    LocalPlayer* destPl;

    replaceStringIfDifferent( MPPARM(mpool) &destGI->dictName, 
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
#ifdef FEATURE_TRAY_EDIT
    destGI->allowPickTiles = srcGI->allowPickTiles;
#endif

    for ( srcPl = srcGI->players, destPl = destGI->players, i = 0; 
          i < nPlayers; ++srcPl, ++destPl, ++i ) {

        replaceStringIfDifferent( MPPARM(mpool) &destPl->name, srcPl->name );
        replaceStringIfDifferent( MPPARM(mpool) &destPl->password, 
                                  srcPl->password );
        destPl->secondsUsed = srcPl->secondsUsed;
        destPl->isRobot = srcPl->isRobot;
        destPl->isLocal = srcPl->isLocal;
    }
} /* gi_copy */

XP_U16
gi_countHumans( CurGameInfo* gi )
{
    XP_U16 count = 0;
    XP_U16 nPlayers = gi->nPlayers;
    while ( nPlayers-- ) {
        if ( !gi->players[nPlayers].isRobot ) {
            ++count;
        }
    }
    return count;
} /* gi_countHumans */

void
gi_readFromStream( MPFORMAL XWStreamCtxt* stream, CurGameInfo* gi )
{
    LocalPlayer* pl;
    XP_U16 i;
    XP_UCHAR* str;
    XP_U16 strVersion = stream_getVersion( stream );

    str = stringFromStream( MPPARM(mpool) stream );
    replaceStringIfDifferent( MPPARM(mpool) &gi->dictName, str );
    if ( !!str ) {
        XP_FREE( mpool, str );
    }

    gi->nPlayers = (XP_U8)stream_getBits( stream, NPLAYERS_NBITS );
    gi->boardSize = (XP_U8)stream_getBits( stream, 4 );
    gi->serverRole = (Connectedness)stream_getBits( stream, 2 );
    gi->hintsNotAllowed = stream_getBits( stream, 1 );
    gi->robotSmartness = (XP_U8)stream_getBits( stream, 2 );
    gi->phoniesAction = (XWPhoniesChoice)stream_getBits( stream, 2 );
    gi->timerEnabled = stream_getBits( stream, 1 );

    if ( strVersion >= CUR_STREAM_VERS ) {
        gi->allowPickTiles = stream_getBits( stream, 1 );
    } else {
        gi->allowPickTiles = XP_FALSE;
    }

    if ( strVersion >= CUR_STREAM_VERS ) {
        gi->allowHintRect = stream_getBits( stream, 1 );
    } else {
        gi->allowHintRect = XP_FALSE;
    }


    gi->gameID = stream_getU16( stream );
    if ( gi->timerEnabled ) {
        gi->gameSeconds = stream_getU16( stream );
    }

    for ( pl = gi->players, i = 0; i < gi->nPlayers; ++pl, ++i ) {
        str = stringFromStream( MPPARM(mpool) stream );
        replaceStringIfDifferent( MPPARM(mpool) &pl->name, str );
        if ( !!str ) {
            XP_FREE( mpool, str );
        }

        str = stringFromStream( MPPARM(mpool) stream );
        replaceStringIfDifferent( MPPARM(mpool) &pl->password, str );
        if ( !!str ) {
            XP_FREE( mpool, str );
        }

        pl->secondsUsed = stream_getU16( stream );
        pl->isRobot = stream_getBits( stream, 1 );
        pl->isLocal = stream_getBits( stream, 1 );
    }
} /* gi_readFromStream */

void
gi_writeToStream( XWStreamCtxt* stream, CurGameInfo* gi )
{
    LocalPlayer* pl;
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
