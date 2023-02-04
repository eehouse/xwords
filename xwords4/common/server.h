 /* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2000 by Eric House (xwords@eehouse.org).  All rights reserved.
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


#ifndef _SERVER_H_
#define _SERVER_H_

#include "comtypes.h" /* that's *common* types */

#include "commmgr.h"
#include "model.h"

#ifdef CPLUS
extern "C" {
#endif

ServerCtxt* server_make( MPFORMAL XWEnv xwe, ModelCtxt* model, CommsCtxt* comms,
                         XW_UtilCtxt* util );

ServerCtxt* server_makeFromStream( MPFORMAL XWEnv xwe, XWStreamCtxt* stream,
                                   ModelCtxt* model, CommsCtxt* comms,
                                   XW_UtilCtxt* util, XP_U16 nPlayers );

void server_writeToStream( const ServerCtxt* server, XWStreamCtxt* stream );

void server_reset( ServerCtxt* server, XWEnv xwe, CommsCtxt* comms );
void server_destroy( ServerCtxt* server );

void server_prefsChanged( ServerCtxt* server, const CommonPrefs* cp );
void server_onRoleChanged( ServerCtxt* server, XWEnv xwe, XP_Bool amNowGuest );

typedef void (*TurnChangeListener)( XWEnv xwe, void* data );
void server_setTurnChangeListener( ServerCtxt* server, TurnChangeListener tl,
                                   void* data );

typedef void (*TimerChangeListener)( XWEnv xwe, void* data, XP_U32 gameID,
                                     XP_S32 oldVal, XP_S32 newVal );
void server_setTimerChangeListener( ServerCtxt* server, TimerChangeListener tl,
                                    void* data );

typedef void (*GameOverListener)( XWEnv xwe, void* data, XP_S16 quitter );
void server_setGameOverListener( ServerCtxt* server, GameOverListener gol,
                                 void* data );

/* support random assignment by telling creator of new player what it's
 * number will be */
/* XP_U16 server_assignNum( ServerCtxt* server ); */

EngineCtxt* server_getEngineFor( ServerCtxt* server, XP_U16 playerNum );
void server_resetEngine( ServerCtxt* server, XP_U16 playerNum );
#ifdef XWFEATURE_CHANGEDICT
void server_resetEngines( ServerCtxt* server );
#endif

XP_U16 server_secondsUsedBy( ServerCtxt* server, XP_U16 playerNum );

/* It might make more sense to have the board supply the undo method clients
   call... */
XP_Bool server_handleUndo( ServerCtxt* server, XWEnv xwe, XP_U16 limit );

/* signed because negative number means nobody's turn yet */
XP_S16 server_getCurrentTurn( const ServerCtxt* server, XP_Bool* isLocal );
XP_Bool server_isPlayersTurn( const ServerCtxt* server, XP_U16 turn );
XP_Bool server_getGameIsOver( const ServerCtxt* server );
XP_Bool server_getGameIsConnected( const ServerCtxt* server );

XP_S32 server_getDupTimerExpires( const ServerCtxt* server );
XP_S16 server_getTimerSeconds( const ServerCtxt* server, XWEnv xwe, XP_U16 turn );
XP_Bool server_dupTurnDone( const ServerCtxt* server, XP_U16 turn );
XP_Bool server_canPause( const ServerCtxt* server );
XP_Bool server_canUnpause( const ServerCtxt* server );
void server_pause( ServerCtxt* server, XWEnv xwe, XP_S16 turn, const XP_UCHAR* msg );
void server_unpause( ServerCtxt* server, XWEnv xwe, XP_S16 turn, const XP_UCHAR* msg );

/* return bitvector marking players still not arrived in networked game */
XP_U16 server_getMissingPlayers( const ServerCtxt* server );
XP_U32 server_getLastMoveTime( const ServerCtxt* server );
/* Signed in case no dictionary available */
XP_S16 server_countTilesInPool( ServerCtxt* server );

XP_Bool server_askPickTiles( ServerCtxt* server, XWEnv xwe, XP_U16 player,
                             TrayTileSet* newTiles, XP_U16 nToPick );
void server_tilesPicked( ServerCtxt* server, XWEnv xwe, XP_U16 player,
                         const TrayTileSet* newTiles );

XP_U16 server_getPendingRegs( const ServerCtxt* server );

XP_Bool server_do( ServerCtxt* server, XWEnv xwe );

XP_Bool server_commitMove( ServerCtxt* server, XWEnv xwe, XP_U16 player,
                           TrayTileSet* newTiles );
XP_Bool server_commitTrade( ServerCtxt* server, XWEnv xwe,
                            const TrayTileSet* oldTiles,
                            TrayTileSet* newTiles );

/* call this when user wants to end the game */
void server_endGame( ServerCtxt* server, XWEnv xwe );

/* called when running as either client or server */
XP_Bool server_receiveMessage( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* incoming );

/* client-side messages.  Client (platform code)owns the stream used to talk
 * to the server, and passes it in. */
#ifndef XWFEATURE_STANDALONE_ONLY
XP_Bool server_initClientConnection( ServerCtxt* server, XWEnv xwe );
#endif

#ifdef XWFEATURE_CHAT
void server_sendChat( ServerCtxt* server, XWEnv xwe,
                      const XP_UCHAR* msg, XP_S16 from );
#endif

void server_formatDictCounts( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* stream,
                              XP_U16 nCols, XP_Bool allFaces );
void server_formatRemainingTiles( ServerCtxt* server, XWEnv xwe,
                                  XWStreamCtxt* stream, XP_S16 player );

void server_writeFinalScores( ServerCtxt* server, XWEnv xwe, XWStreamCtxt* stream );

#ifdef XWFEATURE_BONUSALL
XP_U16 server_figureFinishBonus( const ServerCtxt* server, XP_U16 turn );
#endif

#ifdef DEBUG
XP_Bool server_getIsServer( const ServerCtxt* server );
#endif

#ifdef CPLUS
}
#endif

#endif
