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
#include "mempool.h" /* debug only */

#ifdef CPLUS
extern "C" {
#endif

typedef struct _DragObjInfo {
    BoardObjectType obj;
    union {
        struct {
            XP_U16 col;
            XP_U16 row;
        } board;
        struct {
            XP_U16 index;
        } tray;
    } u;
} DragObjInfo;

typedef enum {
    DT_NONE
    ,DT_DIVIDER
    ,DT_TILE
#ifdef XWFEATURE_SEARCHLIMIT
    ,DT_HINTRGN
#endif
    ,DT_BOARD
} DragType;


typedef struct DragState {
    DragType dtype;
    XP_Bool didMove;            /* there was change during the drag; not a
                                   tap */
    XP_Bool scrollTimerSet;
    XP_Bool isBlank;            /* cache rather than lookup in model */
    Tile tile;                  /* cache rather than lookup in model */
    DragObjInfo start;
    DragObjInfo cur;
} DragState;

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
    const XP_UCHAR* text;
    XP_Rect rect;
} MiniWindowStuff;

enum { MINIWINDOW_VALHINT, MINIWINDOW_TRADING };
typedef XP_U16 MiniWindowType; /* one of the two above */

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
    XP_U16 prevYScrollOffset; /* represents where the last draw took place;
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
    XP_Bool scoreSplitHor;/* how to divide the scoreboard? */

    XP_U16 star_row;

    /* Unless KEYBOARD_NAV is defined, this does not change */
    BoardObjectType focussed;

    BoardArrow boardArrow[MAX_NUM_PLAYERS];
#ifdef KEYBOARD_NAV
    XP_Rect scoreRects[MAX_NUM_PLAYERS];
    BdCursorLoc bdCursor[MAX_NUM_PLAYERS];
    XP_Bool focusHasDived;
#endif
    XP_U8 dividerLoc[MAX_NUM_PLAYERS]; /* 0 means left of 0th tile, etc. */

    XP_U16 scoreDims[MAX_NUM_PLAYERS];

    /* scoreboard state */
    XP_Rect scoreBdBounds;
    XP_Rect timerBounds;
    XP_U8 selPlayer; /* which player is selected (!= turn) */

    /* tray state */
    XP_U8 trayScaleH;
    XP_U8 trayScaleV;
    XP_Rect trayBounds;
    XP_U16 remDim;      /* width (or ht) of the "rem:" string in scoreboard */
    XP_U8 dividerWidth; /* 0 would mean invisible */
    XP_Bool dividerInvalid;

    XP_Bool scoreBoardInvalid;
    DragState dragState;

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
XP_Bool handlePenUpTray( BoardCtxt* board, XP_U16 x, XP_U16 y );
void drawTray( BoardCtxt* board );
XP_Bool moveTileToArrowLoc( BoardCtxt* board, XP_U8 index );
XP_U16 indexForBits( XP_U8 bits );
XP_Bool rectContainsPt( XP_Rect* rect1, XP_S16 x, XP_S16 y );
XP_Bool checkRevealTray( BoardCtxt* board );
void figureTrayTileRect( BoardCtxt* board, XP_U16 index, XP_Rect* rect );
XP_Bool rectsIntersect( const XP_Rect* rect1, const XP_Rect* rect2 );
XP_S16 pointToTileIndex( BoardCtxt* board, XP_U16 x, XP_U16 y, 
                         XP_Bool* onDividerP );
void board_selectPlayer( BoardCtxt* board, XP_U16 newPlayer );
void flipIf( const BoardCtxt* board, XP_U16 col, XP_U16 row, 
             XP_U16* fCol, XP_U16* fRow );
XP_Bool pointOnSomething( BoardCtxt* board, XP_U16 x, XP_U16 y, 
                          BoardObjectType* wp );
XP_Bool coordToCell( BoardCtxt* board, XP_U16 x, XP_U16 y, XP_U16* colP, 
                     XP_U16* rowP );
XP_Bool cellOccupied( BoardCtxt* board, XP_U16 col, XP_U16 row, 
                      XP_Bool inclPending );
XP_Bool holdsPendingTile( BoardCtxt* board, XP_U16 pencol, XP_U16 penrow );

XP_Bool moveTileToBoard( BoardCtxt* board, XP_U16 col, XP_U16 row, 
                         XP_U16 tileIndex, Tile blankFace );

void invalTilesUnderRect( BoardCtxt* board, const XP_Rect* rect );
void invalCellRegion( BoardCtxt* board, XP_U16 colA, XP_U16 rowA, XP_U16 colB, 
                      XP_U16 rowB );
void invalDragObj( BoardCtxt* board, const DragObjInfo* di );
void invalTrayTilesAbove( BoardCtxt* board, XP_U16 tileIndex );
void invalTrayTilesBetween( BoardCtxt* board, XP_U16 tileIndex1, 
                            XP_U16 tileIndex2 );
#ifdef XWFEATURE_SEARCHLIMIT
void invalCurHintRect( BoardCtxt* board, XP_U16 player );
#endif

void hideMiniWindow( BoardCtxt* board, XP_Bool destroy,
                     MiniWindowType winType );

void moveTileInTray( BoardCtxt* board, XP_U16 moveTo, XP_U16 moveFrom );
XP_Bool handleTrayDuringTrade( BoardCtxt* board, XP_S16 index );

XP_UCHAR* getTileDrawInfo( const BoardCtxt* board, Tile tile, XP_Bool isBlank,
                           XP_Bitmap* bitmap, XP_S16* value, 
                           XP_UCHAR* buf, XP_U16 len );
XP_Bool dividerMoved( BoardCtxt* board, XP_U8 newLoc );

XP_Bool checkScrollCell( BoardCtxt* board, XP_U16 col, XP_U16 row );
XP_Bool onBorderCanScroll( const BoardCtxt* board, XP_U16 row, XP_S16* change );
XP_Bool adjustYOffset( BoardCtxt* board, XP_S16 moveBy );

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
