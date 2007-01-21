/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2007 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _BOARDP_H_
#define _BOARDP_H_

#include "comtypes.h"
#include "model.h"
#include "board.h"
#include "engine.h"
#include "mempool.h"		/* debug only */

#ifdef CPLUS
extern "C" {
#endif

typedef struct TileDragState {
    XP_Bool dragInProgress;

    XP_Bool wasHilited;
    TileBit selectionAtStart;
    XP_Bool movePending;
    TileBit prevIndex;
} TileDragState;

typedef struct DividerDragState {
    XP_Bool dragInProgress;
} DividerDragState;

typedef struct BoardArrow { /* gets flipped along with board */
	XP_U8 col;
	XP_U8 row;
	XP_Bool vert;
	XP_Bool visible;
} BoardArrow;

#ifdef KEYBOARD_NAV
typedef struct BdCursorLoc {
    XP_U8 col;
    XP_U8 row;
} BdCursorLoc;
#endif

/* We only need two of these, one for the value hint and the other for the
   trading window.  There's never more than of the former since it lives only
   as long as the pen is down.  There are, in theory, as many trading windows
   as there are (local) players, but they can all use the same window. */
typedef struct MiniWindowStuff {
    void* closure;
    XP_UCHAR* text;
    XP_Rect rect;
} MiniWindowStuff;

enum { MINIWINDOW_VALHINT, MINIWINDOW_TRADING };
typedef XP_U16 MiniWindowType;	/* one of the two above */

struct BoardCtxt {
/*     BoardVTable* vtable; */
    ModelCtxt* model;
    ServerCtxt* server;
    DrawCtx* draw;
    XW_UtilCtxt* util;

    struct CurGameInfo* gi;

    XP_U16 boardHScale;
    XP_U16 boardVScale;
    XP_U16 yOffset;
    XP_U16 lastVisibleRow;
    XP_U16 preHideYOffset;
    XP_U16 prevYScrollOffset;	/* represents where the last draw took place;
				   used to see if bit scrolling can be used */
    XP_U16 penDownX;
    XP_U16 penDownY;

    XP_U32 timerStoppedTime;
    XP_U16 timerSaveCount;
#ifdef DEBUG    
    XP_S16 timerStoppedTurn;
#endif

    XP_U16 redrawFlags[MAX_ROWS];

    XP_Rect boardBounds;

    BoardObjectType penDownObject;

    XP_Bool needsDrawing;
    XP_Bool isFlipped;
    XP_Bool showGrid;
    XP_Bool gameOver;
    XP_Bool leftHanded;
    XP_Bool badWordRejected;
    XP_Bool timerPending;
    XP_Bool disableArrow;
    XP_Bool hideValsInTray;

    XP_Bool tradeInProgress[MAX_NUM_PLAYERS];
    XP_Bool eraseTray;
    XP_Bool boardObscuresTray;
    XP_Bool boardHidesTray;
    XP_Bool scoreSplitHor;	/* how to divide the scoreboard? */

    XP_U16 star_row;

    /* Unless KEYBOARD_NAV is defined, this does not change */
    BoardObjectType focussed;

    BoardArrow boardArrow[MAX_NUM_PLAYERS];
#ifdef KEYBOARD_NAV
    XP_Rect scoreRects[MAX_NUM_PLAYERS];
    BdCursorLoc bdCursor[MAX_NUM_PLAYERS];
    XP_Bool focusHasDived;
#endif
    XP_U8 dividerLoc[MAX_NUM_PLAYERS];	/* 0 means left of 0th tile, etc. */

    XP_U16 scoreDims[MAX_NUM_PLAYERS];

    /* scoreboard state */
    XP_Rect scoreBdBounds;
    XP_Rect timerBounds;
    XP_U8 selPlayer;	/* which player is selected (!= turn) */

    /* tray state */
    XP_U8 trayScaleH;
    XP_U8 trayScaleV;
    XP_Rect trayBounds;
    XP_U16 remDim;	/* width (or ht) of the "rem:" string in scoreboard */
    XP_U8 dividerWidth;		/* 0 would mean invisible */
    XP_Bool dividerInvalid;

    XP_Bool scoreBoardInvalid;
    TileDragState tileDragState;
    DividerDragState divDragState;

    MiniWindowStuff miniWindowStuff[2];
    XP_Bool tradingMiniWindowInvalid;

    TileBit trayInvalBits;
    TileBit traySelBits[MAX_NUM_PLAYERS];
#ifdef KEYBOARD_NAV
    XP_U8   trayCursorLoc[MAX_NUM_PLAYERS];
    XP_U8   scoreCursorLoc;
#endif

    XW_TrayVisState trayVisState;
    XP_Bool penTimerFired;
    XP_Bool showCellValues;
    XP_Bool showColors;

#ifdef XWFEATURE_SEARCHLIMIT
    XP_U16 hintDragStartCol, hintDragStartRow;
    XP_U16 hintDragCurCol, hintDragCurRow;
    XP_Bool hintDragInProgress;

    XP_Bool hasHintRect[MAX_NUM_PLAYERS];
    BdHintLimits limits[MAX_NUM_PLAYERS];
#endif

    MPSLOT
};

#define valHintMiniWindowActive( board ) \
     ((XP_Bool)((board)->miniWindowStuff[MINIWINDOW_VALHINT].text != NULL))
#define MY_TURN(b) ((b)->selPlayer == server_getCurrentTurn( (b)->server ))
#define TRADE_IN_PROGRESS(b) ((b)->tradeInProgress[(b)->selPlayer]==XP_TRUE)

/* tray-related functions */
XP_Bool handlePenDownInTray( BoardCtxt* board, XP_U16 x, XP_U16 y );
XP_Bool handlePenUpTray( BoardCtxt* board, XP_U16 x, XP_U16 y );
void drawTray( BoardCtxt* board );
TileBit continueTileDrag( BoardCtxt* board, XP_U16 x, XP_U16 y );
XP_Bool endTileDrag( BoardCtxt* board, XP_U16 x, XP_U16 y );
XP_Bool continueDividerDrag( BoardCtxt* board, XP_U16 x, XP_U16 y );
XP_Bool endDividerDrag( BoardCtxt* board, XP_U16 x, XP_U16 y );
XP_Bool moveTileToArrowLoc( BoardCtxt* board, XP_U8 index );
XP_U16 indexForBits( XP_U8 bits );
XP_Bool rectContainsPt( XP_Rect* rect1, XP_S16 x, XP_S16 y );
XP_Bool checkRevealTray( BoardCtxt* board );
void invalTilesUnderRect( BoardCtxt* board, XP_Rect* rect );
void figureTrayTileRect( BoardCtxt* board, XP_U16 index, XP_Rect* rect );
XP_Bool rectsIntersect( const XP_Rect* rect1, const XP_Rect* rect2 );

void board_selectPlayer( BoardCtxt* board, XP_U16 newPlayer );

#ifdef KEYBOARD_NAV
XP_Bool tray_moveCursor( BoardCtxt* board, XP_Key cursorKey, 
                         XP_Bool preflightOnly, XP_Bool* up );
XP_Bool tray_keyAction( BoardCtxt* board );
DrawFocusState dfsFor( BoardCtxt* board, BoardObjectType obj );
void shiftFocusUp( BoardCtxt* board, XP_Key key );
#else
# define dfsFor( board, obj ) DFS_NONE
#endif

#ifdef CPLUS
}
#endif

#endif
