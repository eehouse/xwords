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

enum {
    PHONIES_IGNORE,
    PHONIES_WARN,
    PHONIES_DISALLOW
};
typedef XP_U8 XWPhoniesChoice;

enum {
    SERVER_STANDALONE,
    SERVER_ISSERVER,
    SERVER_ISCLIENT
};
typedef XP_U8 DeviceRole;

/* typedef struct ServerCtxt ServerCtxt; */

/* typedef struct ServerVtable { */

/*   void (*m_registerPlayer)( ServerCtxt* server, XP_U16 playerNum, */
/*   XP_PlayerSocket socket ); */

/*   void (*m_getTileValueInfo)( ServerCtxt* server, void* valueBuf ); */

/* } ServerVtable; */

ServerCtxt* server_make( MPFORMAL ModelCtxt* model, CommsCtxt* comms,
                         XW_UtilCtxt* util );

ServerCtxt* server_makeFromStream( MPFORMAL XWStreamCtxt* stream, 
                                   ModelCtxt* model, CommsCtxt* comms,
                                   XW_UtilCtxt* util, XP_U16 nPlayers );

void server_writeToStream( ServerCtxt* server, XWStreamCtxt* stream );

void server_reset( ServerCtxt* server, CommsCtxt* comms );
void server_destroy( ServerCtxt* server );

void server_prefsChanged( ServerCtxt* server, CommonPrefs* cp );

typedef void (*TurnChangeListener)( void* data );
void server_setTurnChangeListener( ServerCtxt* server, TurnChangeListener tl,
                                   void* data );

typedef void (*GameOverListener)( void* data );
void server_setGameOverListener( ServerCtxt* server, GameOverListener gol,
                                 void* data );

/* support random assignment by telling creator of new player what it's
 * number will be */
/* XP_U16 server_assignNum( ServerCtxt* server ); */

EngineCtxt* server_getEngineFor( ServerCtxt* server, XP_U16 playerNum );
void server_resetEngine( ServerCtxt* server, XP_U16 playerNum );

XP_U16 server_secondsUsedBy( ServerCtxt* server, XP_U16 playerNum );

/* It might make more sense to have the board supply the undo method clients
   call... */
XP_Bool server_handleUndo( ServerCtxt* server );

/* signed because negative number means nobody's turn yet */
XP_S16 server_getCurrentTurn( ServerCtxt* server );
XP_Bool server_getGameIsOver( ServerCtxt* server );
/* Signed in case no dictionary available */
XP_S16 server_countTilesInPool( ServerCtxt* server );

XP_Bool server_do( ServerCtxt* server );

XP_Bool server_commitMove( ServerCtxt* server );
XP_Bool server_commitTrade( ServerCtxt* server, TileBit bits );

/* call this when user wants to end the game */
void server_endGame( ServerCtxt* server );

/* called when running as either client or server */
XP_Bool server_receiveMessage( ServerCtxt* server, XWStreamCtxt* incomming );

/* client-side messages.  Client (platform code)owns the stream used to talk
 * to the server, and passes it in. */
#ifndef XWFEATURE_STANDALONE_ONLY
void server_initClientConnection( ServerCtxt* server, XWStreamCtxt* stream );
#endif

void server_formatDictCounts( ServerCtxt* server, XWStreamCtxt* stream,
                              XP_U16 nCols );
void server_formatRemainingTiles( ServerCtxt* server, XWStreamCtxt* stream,
                                  XP_S16 player );

void server_writeFinalScores( ServerCtxt* server, XWStreamCtxt* stream );

#ifdef CPLUS
}
#endif

#endif
