/* 
 * Copyright 2001 - 2022 by Eric House (xwords@eehouse.org).  All rights
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

#include "comtypes.h"
#include "gameinfo.h"
#include "model.h"
#include "board.h"
#include "comms.h"
#include "contrlrp.h"
#include "util.h"
#include "gameref.h"

#ifdef CPLUS
extern "C" {
#endif

typedef struct _XWGame {
    XW_UtilCtxt* util;
    BoardCtxt* board;
    ModelCtxt* model;
    CtrlrCtxt* ctrlr;
    CommsCtxt* comms;
    XP_U32 created;     /* dutil_getCurSeconds() of creation */
} XWGame;

GameRef game_makeNewGame( XWEnv xwe, CurGameInfo* gi,
                           const CommsAddrRec* hostAddr,
                           XW_UtilCtxt* util, DrawCtx* draw,
                           const CommonPrefs* cp );

GameRef game_makeRematch( GameRef game, XWEnv xwe, XW_UtilCtxt* util,
                           const CommonPrefs* cp,
                           const XP_UCHAR* newName, NewOrder* no );

void game_changeDict( MPFORMAL XWGame* game, XWEnv xwe, CurGameInfo* gi,
                      DictionaryCtxt* dict );

XP_Bool game_makeFromStream( MPFORMAL XWEnv xwe, XWStreamCtxt* stream,
                             CurGameInfo* gi, GameRef* grOut,
                             XW_UtilCtxt* util, DrawCtx* draw,
                             CommonPrefs* cp );

GameRef game_makeFromInvite( XWEnv xwe, const NetLaunchInfo* nli,
                             const CommsAddrRec* selfAddr,
                             XW_UtilCtxt* util, DrawCtx* draw,
                             CommonPrefs* cp );

void game_saveToStream( const GameRef gr, const CurGameInfo* gi,
                        XWStreamCtxt* stream, XP_U16 saveToken );
void game_saveSucceeded( const GameRef gr, XWEnv xwe, XP_U16 saveToken );

XP_Bool game_receiveMessage( XW_DUtilCtxt* duc, GameRef gr, XWEnv xwe,
                             XWStreamCtxt* stream, const CommsAddrRec* retAddr );

void game_dispose( XWGame* game, XWEnv xwe );

void game_summarize( const XWGame* game, const CurGameInfo* gi, GameSummary* summary );
void game_getState( const XWGame* game, XWEnv xwe, GameStateInfo* gsi );
XP_Bool game_getIsHost( const XWGame* game );
void gi_setNPlayers( XW_DUtilCtxt* dutil, XWEnv xwe, CurGameInfo* gi,
                     XP_U16 nTotal, XP_U16 nHere );
void gi_writeToStream( XWStreamCtxt* stream, const CurGameInfo* gi );
void gi_readFromStream( XWStreamCtxt* stream, CurGameInfo* gi );
XP_Bool gi_gotFromStream( XWStreamCtxt* stream, CurGameInfo* gi );
CurGameInfo gi_readFromStream2( XWStreamCtxt* stream );
void gi_copy( CurGameInfo* destGI, const CurGameInfo* srcGi );
XP_Bool gi_equal( const CurGameInfo* gi1, const CurGameInfo* gi2 );
XP_U16 gi_countLocalPlayers( const CurGameInfo* gi, XP_Bool humanOnly );
XP_U16 gi_getLocalPlayer( const CurGameInfo* gi, XP_S16 fromHint );

XP_Bool player_hasPasswd( const LocalPlayer* player );
XP_Bool player_passwordMatches( const LocalPlayer* player, const XP_UCHAR* pwd );

#ifdef CPLUS
}
#endif

#endif
