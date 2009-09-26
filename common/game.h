/* -*-mode: C; fill-column: 76; c-basic-offset: 4; -*- */
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

#define STREAM_VERS_CHANNELSEED 0x09 /* new short in relay connect must be
                                        saved in comms */
#define STREAM_VERS_UTF8 0x08
#define STREAM_VERS_ALWAYS_MULTI 0x07 /* stream format same for multi and
                                         one-device game builds */
#define STREAM_VERS_MODEL_NO_DICT 0x06
#define STREAM_VERS_BLUETOOTH 0x05
#define STREAM_VERS_KEYNAV 0x04
#define STREAM_VERS_RELAY 0x03
#define STREAM_VERS_41B4 0x02
#define STREAM_VERS_405  0x01

#define CUR_STREAM_VERS STREAM_VERS_CHANNELSEED

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
    XP_U16 gameID;      /* uniquely identifies game */
    XP_U16 gameSeconds; /* for timer */
    XP_U8 nPlayers;
    XP_U8 boardSize;
    DeviceRole serverRole;

    XP_Bool hintsNotAllowed;
    XP_Bool timerEnabled;
    XP_Bool allowPickTiles;
    XP_Bool allowHintRect;
    XP_U8 robotSmartness;
    XWPhoniesChoice phoniesAction;
    XP_Bool confirmBTConnect;   /* only used for BT */
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
                       XW_UtilCtxt* util, DrawCtx* draw, XP_U16 gameID,
                       CommonPrefs* cp, const TransportProcs* procs );
void game_reset( MPFORMAL XWGame* game, CurGameInfo* gi, XW_UtilCtxt* util, 
                 XP_U16 gameID, CommonPrefs* cp, const TransportProcs* procs );

XP_Bool game_makeFromStream( MPFORMAL XWStreamCtxt* stream, XWGame* game, 
                             CurGameInfo* gi, 
                             DictionaryCtxt* dict, XW_UtilCtxt* util, 
                             DrawCtx* draw, CommonPrefs* cp,
                             const TransportProcs* procs );

void game_saveToStream( const XWGame* game, const CurGameInfo* gi, 
                        XWStreamCtxt* stream );
void game_dispose( XWGame* game );
void gi_initPlayerInfo( MPFORMAL CurGameInfo* gi, 
                        const XP_UCHAR* nameTemplate );
void gi_disposePlayerInfo( MPFORMAL CurGameInfo* gi );
void gi_writeToStream( XWStreamCtxt* stream, const CurGameInfo* gi );
void gi_readFromStream( MPFORMAL XWStreamCtxt* stream, CurGameInfo* gi );
void gi_copy( MPFORMAL CurGameInfo* destGI, const CurGameInfo* srcGi );
XP_U16 gi_countLocalHumans( const CurGameInfo* gi );

XP_Bool player_hasPasswd( LocalPlayer* player );
XP_Bool player_passwordMatches( LocalPlayer* player, XP_U8* buf, XP_U16 len );
XP_U16 player_timePenalty( CurGameInfo* gi, XP_U16 playerNum );

#ifdef CPLUS
}
#endif

#endif
