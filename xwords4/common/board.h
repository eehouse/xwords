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

#ifndef __BOARD_H__
#define __BOARD_H__

#include "comtypes.h"
#include "model.h"
#include "server.h"
#include "draw.h"
#include "xwstream.h"

/* typedef struct BoardVTable { */
/* } BoardVTable; */

#ifdef CPLUS
extern "C" {
#endif

typedef enum {
    TRAY_HIDDEN,	/* doesn't happen unless tray overlaps board */
    TRAY_REVERSED,
    TRAY_REVEALED
} XW_TrayVisState;

typedef enum {
    OBJ_NONE,
    OBJ_BOARD,
    OBJ_SCORE,
    OBJ_TRAY
} BoardObjectType;

typedef enum {
    /* keep these three together: for the cursor */
    XP_KEY_NONE,
    XP_CURSOR_KEY_DOWN,
    XP_CURSOR_KEY_RIGHT,

    XP_CURSOR_KEY_UP,
    XP_CURSOR_KEY_LEFT,
    XP_CURSOR_KEY_DEL,
    XP_FOCUSCHANGE_KEY,
    XP_RETURN_KEY,

    XP_KEY_LAST
} XP_Key;

#define BONUS_HINT_INTERVAL 15	/* stolen from xwords.c */

/* typedef struct BoardCtxt BoardCtxt; */


BoardCtxt* board_make( MPFORMAL ModelCtxt* model, ServerCtxt* server, 
		       DrawCtx* draw, XW_UtilCtxt* util );
BoardCtxt* board_makeFromStream( MPFORMAL XWStreamCtxt* stream, 
                                 ModelCtxt* model, ServerCtxt* server, 
                                 DrawCtx* draw, XW_UtilCtxt* util,
                                 XP_U16 nPlayers );

void board_destroy( BoardCtxt* board );

void board_writeToStream( BoardCtxt* board, XWStreamCtxt* stream );

void board_setPos( BoardCtxt* board, XP_U16 left, XP_U16 top, 
                   XP_Bool leftHanded );
void board_reset( BoardCtxt* board );

/* Vertical scroll support; offset is in rows, not pixels */
XP_Bool board_setYOffset( BoardCtxt* board, XP_U16 newOffset, 
			  XP_Bool invalRevealed );
XP_U16 board_getYOffset( BoardCtxt* board );

void board_setScoreboardLoc( BoardCtxt* board, 
			     XP_U16 scoreLeft, XP_U16 scoreTop,
			     XP_U16 scoreWidth, XP_U16 scoreHeight,
			     XP_Bool divideHorizontally );
void board_setTimerLoc( BoardCtxt* board, 
			XP_U16 timerLeft, XP_U16 timerTop,
			XP_U16 timerWidth, XP_U16 timerHeight );
void board_invalAll( BoardCtxt* board );
void board_invalRect( BoardCtxt* board, XP_Rect* rect );

XP_Bool board_draw( BoardCtxt* board );

XP_Bool board_flip( BoardCtxt* board );
XP_Bool board_toggle_showValues( BoardCtxt* board );
XP_Bool board_getShowColors( BoardCtxt* board );
XP_Bool board_setShowColors( BoardCtxt* board, XP_Bool showColors );
XP_Bool board_replaceTiles( BoardCtxt* board );

XP_Bool board_requestHint( BoardCtxt* board, 
#ifdef XWFEATURE_SEARCHLIMIT
                           XP_Bool useTileLimits,
#endif
                           XP_Bool* workRemainsP );

void board_setScale( BoardCtxt* board, XP_U16 hScale, XP_U16 vScale );
void board_getScale( BoardCtxt* board, XP_U16* hScale, XP_U16* vScale );

XP_Bool board_prefsChanged( BoardCtxt* board, CommonPrefs* cp );

BoardObjectType board_getFocusOwner( BoardCtxt* board );

void board_hiliteCellAt( BoardCtxt* board, XP_U16 col, XP_U16 row );

void board_resetEngine( BoardCtxt* board );
void board_timerFired( BoardCtxt* board, XWTimerReason why );

XP_Bool board_commitTurn( BoardCtxt* board );

void board_pushTimerSave( BoardCtxt* board );
void board_popTimerSave( BoardCtxt* board );

void board_formatRemainingTiles( BoardCtxt* board, XWStreamCtxt* stream );

#ifdef POINTER_SUPPORT
XP_Bool board_handlePenDown( BoardCtxt* board, XP_U16 x, XP_U16 y,
                             XP_Time when, XP_Bool* handled );
XP_Bool board_handlePenMove( BoardCtxt* board, XP_U16 x, XP_U16 y );
XP_Bool board_handlePenUp( BoardCtxt* board, XP_U16 x, XP_U16 y, XP_Time when );
#endif

XP_Bool board_handleKey( BoardCtxt* board, XP_Key key );

#ifdef KEYBOARD_NAV
/* void board_focusChange( BoardCtxt* board ); */
XP_Bool board_toggle_arrowDir( BoardCtxt* board );
#endif

/******************** Tray methods ********************/
#define NO_TILES ((TileBit)0)

void board_setTrayLoc( BoardCtxt* board, XP_U16 trayLeft, XP_U16 trayTop, 
                       XP_U8 trayScaleH, XP_U8 trayScaleV, 
                       XP_U8 dividerWidth );
XP_Bool board_hideTray( BoardCtxt* board );
XP_Bool board_showTray( BoardCtxt* board );
XW_TrayVisState board_getTrayVisState( BoardCtxt* board );

void board_invalTrayTiles( BoardCtxt* board, TileBit what );
XP_Bool board_juggleTray( BoardCtxt* board );
XP_Bool board_beginTrade( BoardCtxt* board );

#if defined FOR_GREMLINS || defined KEYBOARD_NAV
XP_Bool board_moveDivider( BoardCtxt* board, XP_Bool right );
#endif


#ifdef CPLUS
}
#endif

#endif
