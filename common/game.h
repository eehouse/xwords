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

#ifndef _GAME_H_
#define _GAME_H_

#include "model.h"
#include "board.h"
#include "comms.h"
#include "server.h"
#include "util.h"

#ifdef CPLUS
extern "C" {
#endif

#define CUR_STREAM_VERS 0x02

#define STREAM_VERS_405  0x01

typedef struct LocalPlayer {
    XP_UCHAR* name;
    XP_UCHAR* password;
    XP_U16 secondsUsed;
    XP_Bool isRobot;
    XP_Bool isLocal;
} LocalPlayer;

#define DUMB_ROBOT 0
#define SMART_ROBOT 1

typedef struct CurGameInfo {
    XP_UCHAR* dictName;
    LocalPlayer players[MAX_NUM_PLAYERS];
    XP_U16 gameID;		/* uniquely identifies game */
    XP_U16 gameSeconds;		/* for timer */
    XP_U8 nPlayers;
    XP_U8 boardSize;
    Connectedness serverRole;

    XP_Bool hintsNotAllowed;
    XP_Bool timerEnabled;
    XP_Bool allowPickTiles;
    XP_Bool allowHintRect;
    XP_U8 robotSmartness;
    XWPhoniesChoice phoniesAction;

} CurGameInfo;

typedef struct XWGame {
    BoardCtxt* board;
    ModelCtxt* model;
    ServerCtxt* server;
#ifndef XWFEATURE_STANDALONE_ONLY
    CommsCtxt* comms;
#endif
} XWGame;

void game_makeNewGame( MPFORMAL XWGame* game, CurGameInfo* gi, 
                       XW_UtilCtxt* util, DrawCtx* draw, CommonPrefs* cp,
                       TransportSend sendproc, void* closure );
void game_reset( MPFORMAL XWGame* game, CurGameInfo* gi, XP_U16 gameID,
                 CommonPrefs* cp, TransportSend sendproc, void* closure );

void game_makeFromStream( MPFORMAL XWStreamCtxt* stream, XWGame* game, 
                          CurGameInfo* gi, 
                          DictionaryCtxt* dict, XW_UtilCtxt* util, 
                          DrawCtx* draw, CommonPrefs* cp,
                          TransportSend sendProc, void* closure );

void game_saveToStream( XWGame* game, CurGameInfo* gi, XWStreamCtxt* stream );
void game_dispose( XWGame* game );
void gi_initPlayerInfo( MPFORMAL CurGameInfo* gi, XP_UCHAR* nameTemplate );
void gi_disposePlayerInfo( MPFORMAL CurGameInfo* gi );
void gi_writeToStream( XWStreamCtxt* stream, CurGameInfo* gi );
void gi_readFromStream( MPFORMAL XWStreamCtxt* stream, CurGameInfo* gi );
void gi_copy( MPFORMAL CurGameInfo* destGI, CurGameInfo* srcGi );
XP_U16 gi_countHumans( CurGameInfo* gi );

XP_Bool player_hasPasswd( LocalPlayer* player );
XP_Bool player_passwordMatches( LocalPlayer* player, XP_U8* buf, XP_U16 len );
XP_U16 player_timePenalty( CurGameInfo* gi, XP_U16 playerNum );

#ifdef CPLUS
}
#endif

#endif
