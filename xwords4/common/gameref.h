/* 
 * Copyright 2024 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _GAMEREF_H_
#define _GAMEREF_H_

#include "comtypes.h"
#include "commstyp.h"

#include "game.h"               /* needs GameRef defined.... */

#define DUTIL_GR_XWE XW_DUtilCtxt* duc, GameRef gr, XWEnv xwe

XW_UtilCtxt* gr_getUtil( XW_DUtilCtxt* dutil, GameRef gr, XWEnv xwe );
void gr_setUtil( XW_DUtilCtxt* dutil, GameRef gr, XWEnv xwe, XW_UtilCtxt* util );

/* Game */
XP_U32 gr_getCreated( DUTIL_GR_XWE );
XP_Bool gr_getSafeToOpen( DUTIL_GR_XWE );
void gr_setSafeToOpen( DUTIL_GR_XWE, XP_Bool safe );
const CurGameInfo* gr_getGI( DUTIL_GR_XWE );
void gr_setGI( DUTIL_GR_XWE, const CurGameInfo* gip );
void gr_setGameName( DUTIL_GR_XWE, const XP_UCHAR* newName );
void gr_setCollapsed( DUTIL_GR_XWE, XP_Bool collapsed );
XP_Bool gr_makeFromInvite( DUTIL_GR_XWE, const NetLaunchInfo* nli,
                           const CommsAddrRec* selfAddr,
                           XW_UtilCtxt* util, DrawCtx* draw,
                           CommonPrefs* cp );
GameRef gr_makeRematch( DUTIL_GR_XWE, const XP_UCHAR* newName, RematchOrder ro,
                        XP_Bool archiveAfter, XP_Bool deleteAfter );
#ifdef XWFEATURE_CHANGEDICT
void gr_changeDict( DUTIL_GR_XWE, DictionaryCtxt* dict );
#endif
GameStateInfo gr_getState( DUTIL_GR_XWE );
const GameSummary* gr_getSummary( DUTIL_GR_XWE );
GroupRef gr_getGroup( DUTIL_GR_XWE );

XP_U32 gr_getGameID( GameRef gr );
// void gr_writeToStream( XW_DUtilCtxt* duc, GameRef gr,
// XWStreamCtxt* stream, XP_U16 saveToken );

void gr_giToStream( XW_DUtilCtxt* duc, GameRef gr, XWEnv xwe, XWStreamCtxt* stream );

/* board */
XWStreamCtxt* gr_getThumbData( DUTIL_GR_XWE );
void gr_invalAll( DUTIL_GR_XWE );
void gr_draw( DUTIL_GR_XWE );
void gr_figureLayout( DUTIL_GR_XWE,
                      XP_U16 bLeft, XP_U16 bTop, XP_U16 bWidth, XP_U16 bHeight,
                      XP_U16 colPctMax, XP_U16 scorePct, XP_U16 trayPct, XP_U16 scoreWidth,
                      XP_U16 fontWidth, XP_U16 fontHt, XP_Bool squareTiles,
                      BoardDims* dimsp /* out param */ );
void gr_applyLayout( DUTIL_GR_XWE, const BoardDims* dimsp );
void gr_sendChat( DUTIL_GR_XWE, const XP_UCHAR* msg );
XP_U16 gr_getChatCount( DUTIL_GR_XWE );
void gr_getNthChat( DUTIL_GR_XWE, XP_U16 nn, XP_UCHAR* buf, XP_U16* bufLen,
                    XP_S16* from, XP_U32* timestamp,
                    XP_Bool markShown );
void gr_deleteChats( DUTIL_GR_XWE );
#ifdef XWFEATURE_GAMEREF_CONVERT
void gr_addConvertChat( DUTIL_GR_XWE, const XP_UCHAR* msg, XP_U16 player,
                        XP_U32 timestamp );
#endif

void gr_getPlayerName( DUTIL_GR_XWE, XP_U16 nn,
                       XP_UCHAR* buf, XP_U16* bufLen );
void gr_commitTurn( DUTIL_GR_XWE, const PhoniesConf* pc,
                    XP_Bool turnConfirmed, TrayTileSet* newTiles );
void gr_flip( DUTIL_GR_XWE );
void gr_replaceTiles( DUTIL_GR_XWE );
BoardObjectType gr_getFocusOwner( DUTIL_GR_XWE );
#ifdef KEY_SUPPORT
void gr_handleKey( DUTIL_GR_XWE, XP_Key key, XP_Bool* handled );

void gr_handleKeyUp( DUTIL_GR_XWE, XP_Key key, XP_Bool* handled );
void gr_handleKeyDown( DUTIL_GR_XWE, XP_Key key, XP_Bool* handled );
void gr_handleKeyRepeat( DUTIL_GR_XWE, XP_Key key, XP_Bool* handled );
# ifdef KEYBOARD_NAV
XP_Bool gr_focusChanged( DUTIL_GR_XWE, BoardObjectType typ,
                         XP_Bool gained );
# endif
#endif
void gr_zoom( DUTIL_GR_XWE, XP_S16 zoomBy,
              XP_Bool* canInOut );
XP_U16 gr_getLikelyChatter( DUTIL_GR_XWE );

#ifdef POINTER_SUPPORT
void gr_handlePenDown( DUTIL_GR_XWE, XP_U16 xx,
                          XP_U16 yy, XP_Bool* handled );
void gr_handlePenMove( DUTIL_GR_XWE, XP_U16 x, XP_U16 y );
void gr_handlePenUp( DUTIL_GR_XWE, XP_U16 x, XP_U16 y );
XP_Bool gr_containsPt( DUTIL_GR_XWE, XP_U16 xx, XP_U16 yy );
#endif


void gr_juggleTray( DUTIL_GR_XWE );
void gr_beginTrade( DUTIL_GR_XWE );
void gr_endTrade( DUTIL_GR_XWE );
XP_Bool gr_hideTray( DUTIL_GR_XWE );
XP_Bool gr_showTray( DUTIL_GR_XWE );
void gr_toggleTray( DUTIL_GR_XWE );
XP_Bool gr_requestHint( DUTIL_GR_XWE,
#ifdef XWFEATURE_SEARCHLIMIT
                        XP_Bool useTileLimits,
#endif
                        XP_Bool usePrev, XP_Bool* workRemainsP );


XW_TrayVisState gr_getTrayVisState( DUTIL_GR_XWE );
XP_U16 gr_visTileCount( DUTIL_GR_XWE );
XP_Bool gr_canTogglePending( DUTIL_GR_XWE );
XP_Bool gr_prefsChanged( DUTIL_GR_XWE, const CommonPrefs* cp );
void gr_resetEngine( DUTIL_GR_XWE );
void gr_pause( DUTIL_GR_XWE, const XP_UCHAR* msg );
void gr_unpause( DUTIL_GR_XWE, const XP_UCHAR* msg );
XP_Bool gr_setYOffset( DUTIL_GR_XWE, XP_U16 newOffset );
XP_U16 gr_getYOffset( DUTIL_GR_XWE );
XP_Bool gr_passwordProvided( DUTIL_GR_XWE,
                             XP_U16 player, const XP_UCHAR* pass );
void gr_hiliteCellAt( DUTIL_GR_XWE, XP_U16 col, XP_U16 row );
void gr_selectPlayer( DUTIL_GR_XWE, XP_U16 newPlayer,
                      XP_Bool canPeek );
XP_Bool gr_canTrade( DUTIL_GR_XWE );
XP_Bool gr_canHint( DUTIL_GR_XWE );

/* server */
XP_Bool gr_getGameIsConnected( DUTIL_GR_XWE );
XP_Bool gr_getGameIsOver( DUTIL_GR_XWE );
XP_S16 gr_getCurrentTurn( DUTIL_GR_XWE,
                          XP_Bool* isLocal );
XP_U32 gr_getLastMoveTime( DUTIL_GR_XWE );
XP_U32 gr_getDupTimerExpires( DUTIL_GR_XWE );
XP_U16 gr_getMissingPlayers( DUTIL_GR_XWE );
XP_S16 gr_countTilesInPool( DUTIL_GR_XWE );
XP_Bool gr_handleUndo( DUTIL_GR_XWE, XP_U16 limit );
void gr_endGame( DUTIL_GR_XWE );
void gr_writeFinalScores( DUTIL_GR_XWE, XWStreamCtxt* stream );
void gr_figureOrder( DUTIL_GR_XWE, RematchOrder ro, NewOrder* nop );
XWStreamCtxt* gr_formatDictCounts( DUTIL_GR_XWE, XP_U16 nCols,
                                   XP_Bool allFaces );
XWStreamCtxt* gr_formatRemainingTiles( DUTIL_GR_XWE );
XP_Bool gr_canRematch( DUTIL_GR_XWE, XP_Bool* canOrder );
XP_U16 gr_getPendingRegs( DUTIL_GR_XWE );
XP_Bool gr_isFromRematch( DUTIL_GR_XWE );
XP_Bool gr_getOpenChannel( DUTIL_GR_XWE, XP_U16* channel );
void gr_setBlankValue( DUTIL_GR_XWE, XP_U16 player,
                       XP_U16 col, XP_U16 row, XP_U16 tileIndex );
void gr_tilesPicked( DUTIL_GR_XWE, XP_U16 player,
                     const TrayTileSet* newTiles );
XP_Bool gr_commitTrade( DUTIL_GR_XWE,
                        const TrayTileSet* oldTiles,
                        TrayTileSet* newTiles );
void gr_setRematchOrder( DUTIL_GR_XWE, RematchInfo* rip );
/* model */
XP_S16 gr_getNMoves( DUTIL_GR_XWE  );
void gr_figureFinalScores( DUTIL_GR_XWE,
                           ScoresArray* scores, ScoresArray* tilePenalties );
XP_S16 gr_getPlayerScore( DUTIL_GR_XWE, XP_S16 player );
void gr_missingDicts( DUTIL_GR_XWE, const XP_UCHAR* missingNames[],
                      XP_U16* count );
void gr_replaceDicts( DUTIL_GR_XWE, const XP_UCHAR* oldName,
                      const XP_UCHAR* newName );
const DictionaryCtxt* gr_getDictionary( DUTIL_GR_XWE );
XP_Bool gr_getPlayersLastScore( DUTIL_GR_XWE, XP_S16 player,
                                LastMoveInfo* info );
void gr_writeToTextStream( DUTIL_GR_XWE, XWStreamCtxt* stream );
const TrayTileSet* gr_getPlayerTiles( DUTIL_GR_XWE, XP_S16 turn );
XWStreamCtxt* gr_writeGameHistory( DUTIL_GR_XWE, XP_Bool gameOver );

XP_Bool gr_haveData( DUTIL_GR_XWE );
XP_U16 gr_getNumTilesInTray( DUTIL_GR_XWE, XP_S16 turn );

/* comms */
XP_Bool gr_haveComms( DUTIL_GR_XWE );
void gr_start( DUTIL_GR_XWE );
void gr_stop( DUTIL_GR_XWE );
void gr_save( DUTIL_GR_XWE );
void gr_saveSucceeded( DUTIL_GR_XWE,
                       XP_U16 saveToken );
void gr_ackAny( DUTIL_GR_XWE );
void gr_getAddrs( DUTIL_GR_XWE, CommsAddrRec addr[],
                  XP_U16* nRecs );
XP_U16 gr_getChannelSeed( DUTIL_GR_XWE );
XP_U16 gr_countPendingPackets( DUTIL_GR_XWE,
                               XP_Bool* quashed );
XWStreamCtxt* gr_inviteUrl( DUTIL_GR_XWE, const XP_UCHAR* host,
                            const XP_UCHAR* prefix );
void gr_getSelfAddr( DUTIL_GR_XWE, CommsAddrRec* addr );
XP_S16 gr_resendAll( DUTIL_GR_XWE,
                     CommsConnType filter, XP_Bool force );
XP_Bool gr_getIsHost( DUTIL_GR_XWE );
void gr_invite( DUTIL_GR_XWE,
                const NetLaunchInfo* nli, const CommsAddrRec* destAddr,
                XP_Bool sendNow );
void gr_setAddrDisabled( DUTIL_GR_XWE, CommsConnType typ, 
                         XP_Bool send, XP_Bool disabled );
XP_Bool gr_getAddrDisabled( DUTIL_GR_XWE, CommsConnType typ, 
                            XP_Bool send );
XP_Bool gr_checkIncomingStream( DUTIL_GR_XWE,
                                XWStreamCtxt* stream,
                                const CommsAddrRec* addr, 
                                CommsMsgState* state );
void gr_setQuashed(DUTIL_GR_XWE, XP_Bool set);
void gr_setDraw( DUTIL_GR_XWE, DrawCtx* dctx, XW_UtilCtxt* util );

XP_Bool gr_isArchived( DUTIL_GR_XWE );

XWStreamCtxt* gr_getPendingPacketsFor( DUTIL_GR_XWE, const CommsAddrRec* addr,
                                       const XP_UCHAR* host,
                                       const XP_UCHAR* prefix );
void gr_parsePendingPackets( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr,
                             XWStreamCtxt* stream );

# ifdef DEBUG
XWStreamCtxt* gr_getStats( DUTIL_GR_XWE );
# endif


#endif

