/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2010 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef __BOARD_H__
#define __BOARD_H__

#include "comtypes.h"
#include "model.h"
#include "gameinfo.h"
#include "serverp.h"
#include "draw.h"
#include "xwstream.h"

#ifdef CPLUS
extern "C" {
#endif

#define BONUS_HINT_INTERVAL 15 /* stolen from xwords.c */

/* typedef struct BoardCtxt BoardCtxt; */

BoardCtxt* board_make( XWEnv xwe, ModelCtxt* model, ServerCtxt* server,
                       DrawCtx* draw, XW_UtilCtxt** utilp );
BoardCtxt* board_makeFromStream( XWEnv xwe, XWStreamCtxt* stream,
                                 ModelCtxt* model, ServerCtxt* server, 
                                 DrawCtx* draw, XW_UtilCtxt** utilp,
                                 XP_U16 nPlayers );
void board_setCallbacks( BoardCtxt* board, XWEnv xwe );
void board_setDraw( BoardCtxt* board, XWEnv xwe, DrawCtx* draw );
DrawCtx* board_getDraw( const BoardCtxt* board );

void board_destroy( BoardCtxt* board, XWEnv xwe, XP_Bool ownsUtil );

void board_writeToStream( const BoardCtxt* board, XWStreamCtxt* stream );

void board_reset( BoardCtxt* board, XWEnv xwe );

void board_drawThumb( const BoardCtxt* board, XWEnv xwe, DrawCtx* dctx );

    /* Layout.  Either done internally or by client */
#ifdef COMMON_LAYOUT

void board_figureLayout( BoardCtxt* board, XWEnv xwe, const CurGameInfo* gi,
                         XP_U16 bLeft, XP_U16 bTop, XP_U16 bWidth, XP_U16 bHeight,
                         XP_U16 colPctMax, XP_U16 scorePct, XP_U16 trayPct,
                         XP_U16 scoreWidth, XP_U16 fontWidth, XP_U16 fontHt,
                         XP_Bool squareTiles, /* out */ BoardDims* dimsp );
void board_applyLayout( BoardCtxt* board, XWEnv xwe, const BoardDims* dims );

#endif

/* These four aren't needed if COMMON_LAYOUT defined */
#ifndef COMMON_LAYOUT
void board_setPos( BoardCtxt* board, XWEnv xwe, XP_U16 left, XP_U16 top,
                   XP_U16 width, XP_U16 height, XP_U16 maxCellSize, 
                   XP_Bool leftHanded );
void board_setScoreboardLoc( BoardCtxt* board, 
                             XP_U16 scoreLeft, XP_U16 scoreTop,
                             XP_U16 scoreWidth, XP_U16 scoreHeight,
                             XP_Bool divideHorizontally );
void board_setTrayLoc( BoardCtxt* board, XWEnv xwe,
                       XP_U16 trayLeft, XP_U16 trayTop,
                       XP_U16 trayWidth, XP_U16 trayHeight, XP_U16 nTiles );
#endif

/* Vertical scroll support; offset is in rows, not pixels */
XP_Bool board_setYOffset( BoardCtxt* board, XWEnv xwe, XP_U16 newOffset );
XP_U16 board_getYOffset( const BoardCtxt* board );
void board_selectPlayer( BoardCtxt* board, XWEnv xwe, XP_U16 newPlayer,
                         XP_Bool canPeek );
XP_Bool board_curTurnSelected( const BoardCtxt* board );
XP_U16 board_visTileCount( const BoardCtxt* board );
void board_pause( BoardCtxt* board, XWEnv xwe, const XP_UCHAR* msg );
void board_unpause( BoardCtxt* board, XWEnv xwe, const XP_UCHAR* msg );
XP_Bool board_canShuffle( const BoardCtxt* board );
XP_Bool board_canHideRack( const BoardCtxt* board );
XP_Bool board_canTrade( BoardCtxt* board, XWEnv xwe );
XP_Bool board_canTogglePending( const BoardCtxt* board );
XP_Bool board_canHint( const BoardCtxt* board );
#ifdef XWFEATURE_CHAT
void board_sendChat( const BoardCtxt* board, XWEnv xwe, const XP_UCHAR* msg );
#endif
/* zoomBy: >0: zoom in; < 0: zoom out; 0: query only */
XP_Bool board_zoom( BoardCtxt* board, XWEnv xwe, XP_S16 zoomBy, XP_Bool* canInOut );

void board_invalAll( BoardCtxt* board );
void board_invalRect( BoardCtxt* board, XP_Rect* rect );
#ifdef XWFEATURE_ACTIVERECT
XP_Bool board_getActiveRect( const BoardCtxt* board, XP_Rect* rect,
                             XP_U16* nCols, XP_U16* nRows );
#endif

XP_Bool board_draw( BoardCtxt* board, XWEnv xwe );

XP_Bool board_get_flipped( const BoardCtxt* board );
XP_Bool board_flip( BoardCtxt* board );
XP_Bool board_inTrade( const BoardCtxt* board, XP_Bool* anySelected );
XP_Bool board_replaceTiles( BoardCtxt* board, XWEnv xwe );
XP_Bool board_redoReplacedTiles( BoardCtxt* board, XWEnv xwe );

XP_U16 board_getLikelyChatter( const BoardCtxt* board );
XP_Bool board_passwordProvided( BoardCtxt* board, XWEnv xwe,
                                XP_U16 player, const XP_UCHAR* pass );

XP_Bool board_requestHint( BoardCtxt* board, XWEnv xwe,
#ifdef XWFEATURE_SEARCHLIMIT
                           XP_Bool useTileLimits,
#endif
                           XP_Bool usePrev, XP_Bool* workRemainsP );

XP_Bool board_prefsChanged( BoardCtxt* board, XWEnv xwe,
                            const CommonPrefs* cp );

BoardObjectType board_getFocusOwner( BoardCtxt* board );

void board_hiliteCellAt( BoardCtxt* board, XWEnv xwe, XP_U16 col, XP_U16 row );
XP_Bool board_setBlankValue( BoardCtxt* board, XP_U16 XP_UNUSED(player),
                             XP_U16 col, XP_U16 row, XP_U16 tileIndex );

void board_resetEngine( BoardCtxt* board );

XP_Bool board_commitTurn( BoardCtxt* board, XWEnv xwe, const PhoniesConf* pc,
                          XP_Bool turnConfirmed, TrayTileSet* newTiles );

void board_pushTimerSave( BoardCtxt* board, XWEnv xwe );
void board_popTimerSave( BoardCtxt* board, XWEnv xwe );

void board_formatRemainingTiles( BoardCtxt* board, XWEnv xwe,
                                 XWStreamCtxt* stream );

#ifdef POINTER_SUPPORT
XP_Bool board_handlePenDown( BoardCtxt* board, XWEnv xwe, XP_U16 xx,
                             XP_U16 yy, XP_Bool* handled );
XP_Bool board_handlePenMove( BoardCtxt* board, XWEnv xwe, XP_U16 x, XP_U16 y );
XP_Bool board_handlePenUp( BoardCtxt* board, XWEnv xwe, XP_U16 x, XP_U16 y );
XP_Bool board_containsPt( const BoardCtxt* board, XP_U16 xx, XP_U16 yy );
#endif

#ifdef KEY_SUPPORT
XP_Bool board_handleKey( BoardCtxt* board, XWEnv xwe, XP_Key key, XP_Bool* handled );

XP_Bool board_handleKeyUp( BoardCtxt* board, XWEnv xwe, XP_Key key, XP_Bool* handled );
XP_Bool board_handleKeyDown( BoardCtxt* board, XWEnv xwe, XP_Key key, XP_Bool* handled );
XP_Bool board_handleKeyRepeat( BoardCtxt* board, XWEnv xwe, XP_Key key, XP_Bool* handled );
# ifdef KEYBOARD_NAV
XP_Bool board_focusChanged( BoardCtxt* board, XWEnv xwe, BoardObjectType typ,
                            XP_Bool gained );
# endif
#endif

/******************** Tray methods ********************/
#define NO_TILES ((TileBit)0)

XP_Bool board_hideTray( BoardCtxt* board, XWEnv xwe );
XP_Bool board_showTray( BoardCtxt* board, XWEnv xwe );
XW_TrayVisState board_getTrayVisState( const BoardCtxt* board );

void board_invalTrayTiles( BoardCtxt* board, TileBit what );
XP_Bool board_juggleTray( BoardCtxt* board, XWEnv xwe );
XP_Bool board_beginTrade( BoardCtxt* board, XWEnv xwe );
XP_Bool board_endTrade( BoardCtxt* board );

#if defined FOR_GREMLINS
XP_Bool board_moveDivider( BoardCtxt* board, XP_Bool right );
#endif


#ifdef CPLUS
}
#endif

#endif
