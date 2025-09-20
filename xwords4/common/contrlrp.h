 /* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2025 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _CONTRLRP_H_
#define _CONTRLRP_H_

#include "comtypes.h" /* that's *common* types */

#include "model.h"
#include "gameinfo.h"

#ifdef CPLUS
extern "C" {
#endif

CtrlrCtxt* ctrl_make( XWEnv xwe, ModelCtxt* model, CommsCtxt* comms,
                         XW_UtilCtxt** utilp );

CtrlrCtxt* ctrl_makeFromStream( XWEnv xwe, XWStreamCtxt* stream,
                                   ModelCtxt* model, CommsCtxt* comms,
                                   XW_UtilCtxt** utilp, XP_U16 nPlayers );

void ctrl_writeToStream( const CtrlrCtxt* ctrlr, XWStreamCtxt* stream );

void ctrl_setUtil( CtrlrCtxt* ctrlr, XWEnv xwe, XW_UtilCtxt* util );
void ctrl_reset( CtrlrCtxt* ctrlr, XWEnv xwe, CommsCtxt* comms );
void ctrl_destroy( CtrlrCtxt* ctrlr );

void ctrl_prefsChanged( CtrlrCtxt* ctrlr, const CommonPrefs* cp );
#ifdef XWFEATURE_RELAY
void ctrl_onRoleChanged( CtrlrCtxt* ctrlr, XWEnv xwe, XP_Bool amNowGuest );
#endif

typedef void (*TurnChangeListener)( XWEnv xwe, void* data );
void ctrl_setTurnChangeListener( CtrlrCtxt* ctrlr, TurnChangeListener tl,
                                   void* data );

typedef void (*TimerChangeListener)( XWEnv xwe, void* data, GameRef gr,
                                     XP_S32 oldVal, XP_S32 newVal );
void ctrl_setTimerChangeListener( CtrlrCtxt* ctrlr, TimerChangeListener tl,
                                    void* data );

typedef void (*GameOverListener)( XWEnv xwe, void* data, XP_S16 quitter );
void ctrl_setGameOverListener( CtrlrCtxt* ctrlr, GameOverListener gol,
                                 void* data );

/* support random assignment by telling creator of new player what it's
 * number will be */
/* XP_U16 ctrl_assignNum( CtrlrCtxt* ctrlr ); */

EngineCtxt* ctrl_getEngineFor( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 playerNum );
void ctrl_resetEngine( CtrlrCtxt* ctrlr, XP_U16 playerNum );
void ctrl_resetEngines( CtrlrCtxt* ctrlr );

XP_U16 ctrl_secondsUsedBy( CtrlrCtxt* ctrlr, XP_U16 playerNum );

/* It might make more sense to have the board supply the undo method clients
   call... */
XP_Bool ctrl_handleUndo( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 limit );

/* signed because negative number means nobody's turn yet */
XP_S16 ctrl_getCurrentTurn( const CtrlrCtxt* ctrlr, XP_Bool* isLocal );
XP_Bool ctrl_isPlayersTurn( const CtrlrCtxt* ctrlr, XP_U16 turn );
XP_Bool ctrl_getGameIsOver( const CtrlrCtxt* ctrlr );
XP_Bool ctrl_getGameIsConnected( const CtrlrCtxt* ctrlr );
XP_S32 ctrl_getDupTimerExpires( const CtrlrCtxt* ctrlr );
XP_S16 ctrl_getTimerSeconds( const CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 turn );
XP_Bool ctrl_dupTurnDone( const CtrlrCtxt* ctrlr, XP_U16 turn );
XP_Bool ctrl_canPause( const CtrlrCtxt* ctrlr );
XP_Bool ctrl_canUnpause( const CtrlrCtxt* ctrlr );
XP_Bool ctrl_canRematch( const CtrlrCtxt* ctrlr, XP_Bool* canOrder );
void ctrl_setReMissing( const CtrlrCtxt* ctrlr, GameSummary* gs );

void ctrl_pause( CtrlrCtxt* ctrlr, XWEnv xwe, XP_S16 turn, const XP_UCHAR* msg );
void ctrl_unpause( CtrlrCtxt* ctrlr, XWEnv xwe, XP_S16 turn, const XP_UCHAR* msg );

/* return bitvector marking players still not arrived in networked game */
XP_U16 ctrl_getMissingPlayers( const CtrlrCtxt* ctrlr );
XP_Bool ctrl_getOpenChannel( const CtrlrCtxt* ctrlr, XP_U16* channel );
XP_U32 ctrl_getLastMoveTime( const CtrlrCtxt* ctrlr );
/* Signed in case no dictionary available */
XP_S16 ctrl_countTilesInPool( CtrlrCtxt* ctrlr );

XP_Bool ctrl_askPickTiles( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 player,
                             TrayTileSet* newTiles, XP_U16 nToPick );
void ctrl_tilesPicked( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 player,
                         const TrayTileSet* newTiles );

XP_U16 ctrl_getPendingRegs( const CtrlrCtxt* ctrlr );

void ctrl_addIdle( CtrlrCtxt* ctrlr, XWEnv xwe );
XP_Bool ctrl_commitMove( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U16 player,
                           TrayTileSet* newTiles );
XP_Bool ctrl_commitTrade( CtrlrCtxt* ctrlr, XWEnv xwe,
                            const TrayTileSet* oldTiles,
                            TrayTileSet* newTiles );

/* call this when user wants to end the game */
void ctrl_endGame( CtrlrCtxt* ctrlr, XWEnv xwe );

void ctrl_inviteeName( const CtrlrCtxt* ctrlr,
                         XWEnv xwe, XP_U16 channelNo,
                         XP_UCHAR* buf, XP_U16* bufLen );

/* called when running as either client or server */
XP_Bool ctrl_receiveMessage( CtrlrCtxt* ctrlr, XWEnv xwe, XWStreamCtxt* incoming );

/* client-side messages.  Client (platform code)owns the stream used to talk
 * to the server, and passes it in. */
XP_Bool ctrl_initClientConnection( CtrlrCtxt* ctrlr, XWEnv xwe );

void ctrl_sendChat( CtrlrCtxt* ctrlr, XWEnv xwe,
                      const XP_UCHAR* msg, XP_S16 from );

void ctrl_formatDictCounts( CtrlrCtxt* ctrlr, XWEnv xwe, XWStreamCtxt* stream,
                              XP_U16 nCols, XP_Bool allFaces );
void ctrl_formatRemainingTiles( CtrlrCtxt* ctrlr, XWEnv xwe,
                                  XWStreamCtxt* stream, XP_S16 player );

void ctrl_writeFinalScores( CtrlrCtxt* ctrlr, XWEnv xwe, XWStreamCtxt* stream );

#ifdef XWFEATURE_BONUSALL
XP_U16 ctrl_figureFinishBonus( const CtrlrCtxt* ctrlr, XP_U16 turn );
#endif

#ifdef DEBUG
XP_Bool ctrl_getIsHost( const CtrlrCtxt* ctrlr );
#endif

#ifdef DEBUG
const XP_UCHAR* RO2Str(RematchOrder ro);
#endif


/* Info about remote addresses that lets us determine an order for invited
   players as they arrive. It stores the addresses of all remote devices, and
   for each a mask of which players will come from that address.

   No need for a count: once we find a playersMask == 0 we're done
*/

/* Figure the order of players from the current game per the RematchOrder
   provided. */
void ctrl_figureOrder( const CtrlrCtxt* ctrlr, RematchOrder ro,
                         NewOrder* nop );

/* Sets up newUtil->gameInfo correctly, and returns with a set of
   addresses to which invitation should be sent. But: meant to be called
   only from game.c anyway.
*/
XP_Bool ctrl_getRematchInfo(const CtrlrCtxt* ctrlr, XWEnv xwe,
                              RematchOrder ro, CurGameInfo* newGI,
                              RematchInfo** ripp);
void ctrl_disposeRematchInfo( CtrlrCtxt* ctrlr, RematchInfo* ri );
XP_Bool ctrl_ri_getAddr( const RematchInfo* ri, XP_U16 nth,
                           CommsAddrRec* addr, XP_U16* nPlayersH );

/* Pass in the info the server will need to hang onto until all invitees have
   registered, at which point it can set and communicate the player order for
   the game. To be called only from game.c! */
void ctrl_setRematchOrder( CtrlrCtxt* ctrlr, const RematchInfo* ri );

XP_Bool ctrl_isFromRematch( const CtrlrCtxt* ctrlr );

#ifdef XWFEATURE_KNOWNPLAYERS
void ctrl_gatherPlayers( CtrlrCtxt* ctrlr, XWEnv xwe, XP_U32 created );
#endif

#ifdef CPLUS
}
#endif

#endif
