/* -*- compile-command: "cd ../linux && make -j5 MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 1997 - 2014 by Eric House (xwords@eehouse.org).  All rights
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

/* Re: boards that can't fit on the screen.  Let's have an assumption, that
 * the tray is always either below the board or overlapping its bottom.  There
 * is never any board visible below the tray.  But it's possible to have a
 * board small enough that scrolling is necessary even with the tray hidden.
 *
 * Currently we don't specify the board bounds.  We give top,left and the size
 * of cells, and the board figures out the bounds.  That's probably a mistake.
 * Better to give bounds, and maybe a min scale, and let it figure out how
 * many cells can be visible.  Could it also decide if the tray should overlap
 * or be below?  Some platforms have to own that decision since the tray is
 * narrower than the board.  So give them separate bounds-setting functions,
 * and let the board code figure out if they overlap.
 *
 * Problem: the board size must always be a multiple of the scale.  The
 * platform-specific code has an easy time doing that math.  The board can't:
 * it'd have to take bounds, then spit them back out slightly modified.  It'd
 * also have to refuse to work (maybe just assert) if asked to take bounds
 * before it had a min_scale.
 *
 * Another way of looking at it closer to the current: the board's position
 * and the tray's bounds determine the board's bounds.  If the board's vScale
 * times the number of rows places its would-be bottom at or above the bottom
 * of the tray, then it's potentially visible.  If its would-be bottom is
 * above the top of the tray, no scrolling is needed.  But if it's below the
 * tray entirely then scrolling will happen even with the tray hidden.  As
 * above, we assume the board never appears below the tray.
 */

#include "comtypes.h"
#include "board.h"
#include "scorebdp.h"
#include "game.h"
#include "server.h"
#include "comms.h" /* for CHANNEL_NONE */
#include "dictnry.h"
#include "draw.h"
#include "device.h"
#include "engine.h"
#include "util.h"
#include "mempool.h" /* debug only */
#include "memstream.h"
#include "strutils.h"
#include "LocalizedStrIncludes.h"

#include "boardp.h"
#include "dragdrpp.h"
#include "dbgutil.h"

#ifndef MAX_BOARD_ZOOM          /* width in cells visible at max zoom */
/* too big looks bad */
# define MAX_BOARD_ZOOM 4
#endif

#ifndef DIVIDER_RATIO
# define DIVIDER_RATIO 3        /* 1/3 tray tile width */
#endif
#define DIVIDER_WIDTH(trayWidth) \
    ((trayWidth) / (1 + (MAX_TRAY_TILES*DIVIDER_RATIO)))

#ifdef CPLUS
extern "C" {
#endif

/****************************** prototypes ******************************/
static void figureBoardRect( BoardCtxt* board, XWEnv xwe );
static void forceRectToBoard( const BoardCtxt* board, XP_Rect* rect );

static void boardCellChanged( XWEnv xwe, void* closure, XP_U16 turn, XP_U16 col,
                              XP_U16 row, XP_Bool added );
static void boardTilesChanged( void* board, XP_U16 turn, XP_S16 index1, 
                               XP_S16 index2 );
static void dictChanged( void* p_board, XWEnv xwe, XP_S16 playerNum,
                         const DictionaryCtxt* oldDict, 
                         const DictionaryCtxt* newDict );

static void boardTurnChanged( XWEnv xwe, void* closure );
static XP_S16 chooseBestSelPlayer( const BoardCtxt* board );

static void boardGameOver( XWEnv xwe, void* closure, XP_S16 quitter );
static void setArrow( BoardCtxt* board, XWEnv xwe, XP_U16 row,
                      XP_U16 col, XP_Bool* vp );
static XP_Bool setArrowVisible( BoardCtxt* board, XP_Bool visible );
static void board_setTimerLoc( BoardCtxt* board,
                               XP_U16 timerLeft, XP_U16 timerTop,
                               XP_U16 timerWidth, XP_U16 timerHeight );
#ifdef XWFEATURE_MINIWIN
static void invalTradeWindow( BoardCtxt* board, XP_S16 turn, XP_Bool redraw );
#else
# define invalTradeWindow(b,t,r)
#endif
static XP_Bool invalCellsWithTiles( BoardCtxt* board );

static void setTimerIf( BoardCtxt* board, XWEnv xwe );

static XP_Bool p_board_timerFired( void* closure, XWEnv xwe, XWTimerReason why );

static XP_Bool replaceLastTile( BoardCtxt* board, XWEnv xwe );
static XP_Bool setTrayVisState( BoardCtxt* board, XWEnv xwe,
                                XW_TrayVisState newState );
static XP_Bool advanceArrow( BoardCtxt* board, XWEnv xwe );
static XP_Bool exitTradeMode( BoardCtxt* board );

static XP_Bool getArrow( const BoardCtxt* board, XP_U16* col, XP_U16* row );
static XP_Bool setArrowVisibleFor( BoardCtxt* board, XP_U16 player, 
                                   XP_Bool visible );
static XP_Bool board_moveArrow( BoardCtxt* board, XWEnv xwe, XP_Key cursorKey );

static XP_Bool board_setXOffset( BoardCtxt* board, XP_U16 offset );
static XP_Bool preflight( BoardCtxt* board, XWEnv xwe, XP_Bool reveal );

#ifdef KEY_SUPPORT
static XP_Bool moveKeyTileToBoard( BoardCtxt* board, XWEnv xwe,
                                   XP_Key cursorKey, XP_Bool* gotArrow );
static XP_S16 keyToIndex( BoardCtxt* board, XP_Key key, Tile* blankFace );
#endif

#ifdef COMMON_LAYOUT
static void board_setPos( BoardCtxt* board, XWEnv xwe, XP_U16 left, XP_U16 top,
                          XP_U16 width, XP_U16 height, XP_U16 maxCellSize,
                          XP_Bool leftHanded );
static void board_setTrayLoc( BoardCtxt* board, XWEnv xwe, XP_U16 trayLeft, XP_U16 trayTop,
                              XP_U16 trayWidth, XP_U16 trayHeight, XP_U16 nTiles );
#endif

#ifdef KEYBOARD_NAV
static XP_Bool board_moveCursor( BoardCtxt* board, XWEnv xwe, XP_Key cursorKey,
                                 XP_Bool preflightOnly, XP_Bool* up );
static XP_Bool invalFocusOwner( BoardCtxt* board, XWEnv xwe );
#else
# define invalFocusOwner(board, xwe) 0
#endif
#ifdef XWFEATURE_SEARCHLIMIT
static void clearCurHintRect( BoardCtxt* board );

#else
# define figureHintAtts(b,c,r) HINT_BORDER_NONE
#endif

/*****************************************************************************
 *
 ****************************************************************************/
BoardCtxt*
board_make( MPFORMAL XWEnv xwe, ModelCtxt* model, ServerCtxt* server,
            DrawCtx* draw, XW_UtilCtxt* util )
{
    BoardCtxt* result = (BoardCtxt*)XP_MALLOC( mpool, sizeof( *result ) );
    XP_ASSERT( !!server );
    XP_ASSERT( !!util );
    XP_ASSERT( !!model );

    if ( result != NULL ) {

        XP_MEMSET( result, 0, sizeof( *result ) );
        result->selInfo = result->pti; /* equates to selPlayer == 0 */

        MPASSIGN(result->mpool, mpool);

        result->model = model;
        result->server = server;

        result->draw = draw;
        result->util = util;
        result->dutil = util_getDevUtilCtxt( util, xwe );
        result->gi = util->gameInfo;
        XP_ASSERT( !!result->gi );

        result->trayVisState = TRAY_HIDDEN;

        result->star_row = (XP_U16)(model_numRows(model) / 2);

#ifdef KEYBOARD_NAV     
        {
            /* set up some useful initial values */
            XP_U16 ii;
            for ( ii = 0; ii < MAX_NUM_PLAYERS; ++ii ) {
                PerTurnInfo* pti = result->pti + ii;
                pti->trayCursorLoc = 1;
                pti->bdCursor.col = 5;
                pti->bdCursor.row = 7;
            }
        }
#endif

    }
    return result;
} /* board_make */

void
board_destroy( BoardCtxt* board, XWEnv xwe, XP_Bool ownsUtil )
{
    if ( ownsUtil ) {
        util_clearTimer( board->util, xwe, TIMER_TIMERTICK );
    }
    XP_FREE( board->mpool, board );
} /* board_destroy */

BoardCtxt* 
board_makeFromStream( MPFORMAL XWEnv xwe, XWStreamCtxt* stream, ModelCtxt* model,
                      ServerCtxt* server, DrawCtx* draw, XW_UtilCtxt* util,
                      XP_U16 nPlayers )
{
    BoardCtxt* board;
    XP_U16 version = stream_getVersion( stream );
    XP_U16 nColsNBits;
#ifdef STREAM_VERS_BIGBOARD
    nColsNBits = 16 > model_numCols(model) ? NUMCOLS_NBITS_4 : NUMCOLS_NBITS_5;
#else
    nColsNBits = NUMCOLS_NBITS_4;
#endif

    board = board_make( MPPARM(mpool) xwe, model, server, draw, util );
    board_setCallbacks( board, xwe );

    if ( version >= STREAM_VERS_4YOFFSET) {
        board->sd[SCROLL_H].offset = (XP_U16)stream_getBits( stream, nColsNBits );
        board->zoomCount = (XP_U16)stream_getBits( stream, nColsNBits );
    }
    board->sd[SCROLL_V].offset = (XP_U16)
        stream_getBits( stream, (version < STREAM_VERS_4YOFFSET) ? 2 : nColsNBits );
    board->isFlipped = (XP_Bool)stream_getBits( stream, 1 );
    board->gameOver = (XP_Bool)stream_getBits( stream, 1 );
    board->showColors = (XP_Bool)stream_getBits( stream, 1 );
    if ( version < STREAM_VERS_NOEMPTYDICT ) {
        (void)stream_getBits( stream, 1 );
    }

    if ( version >= STREAM_VERS_KEYNAV ) {
        board->focussed = (BoardObjectType)stream_getBits( stream, 2 );
#ifdef KEYBOARD_NAV
        board->focusHasDived = (BoardObjectType)stream_getBits( stream, 1 );
        board->scoreCursorLoc = (XP_U8)
            stream_getBits( stream, (version < STREAM_VERS_MODEL_NO_DICT? 2:3));
#else
        (void)stream_getBits( stream, 
                              version < STREAM_VERS_MODEL_NO_DICT? 3:4 );
#endif
    }

    XP_ASSERT( !!server );

    for ( int ii = 0; ii < nPlayers; ++ii ) {
        PerTurnInfo* pti = &board->pti[ii];
        BoardArrow* arrow = &pti->boardArrow;
        arrow->col = (XP_U8)stream_getBits( stream, nColsNBits );
        arrow->row = (XP_U8)stream_getBits( stream, nColsNBits );
        arrow->vert = (XP_Bool)stream_getBits( stream, 1 );
        arrow->visible = (XP_Bool)stream_getBits( stream, 1 );

        if ( STREAM_VERS_MODELDIVIDER > version ) {
            (void)stream_getBits( stream, NTILES_NBITS_7 );
        }
        XP_U16 nBits = STREAM_VERS_NINETILES <= version ? MAX_TRAY_TILES : 7;
        pti->traySelBits = (TileBit)stream_getBits( stream, nBits );
        pti->tradeInProgress = (XP_Bool)stream_getBits( stream, 1 );

        if ( version >= STREAM_VERS_KEYNAV ) {
#ifdef KEYBOARD_NAV
            pti->bdCursor.col = stream_getBits( stream, 4 );
            pti->bdCursor.row = stream_getBits( stream, 4 );
            pti->trayCursorLoc = stream_getBits( stream, 3 );
#else
            (void)stream_getBits( stream, 4+4+3 );
#endif
        }

#ifdef XWFEATURE_SEARCHLIMIT
        if ( version >= STREAM_VERS_41B4 ) {
            pti->hasHintRect = stream_getBits( stream, 1 );
        } else {
            pti->hasHintRect = XP_FALSE;
        }
        if ( pti->hasHintRect ) {
            pti->limits.left = stream_getBits( stream, 4 );
            pti->limits.top = stream_getBits( stream, 4 );
            pti->limits.right = stream_getBits( stream, 4 );
            pti->limits.bottom =  stream_getBits( stream, 4 );
        }
#endif
    }

    board->selPlayer = (XP_U8)stream_getBits( stream, PLAYERNUM_NBITS );
    board->selInfo = &board->pti[board->selPlayer];
    board->trayVisState = (XW_TrayVisState)stream_getBits( stream, 2 );

    return board;
} /* board_makeFromStream */

void
board_setDraw( BoardCtxt* board, XWEnv xwe, DrawCtx* draw )
{
    board->draw = draw;
    if ( !!draw ) {
        const DictionaryCtxt* langDict = model_getDictionary( board->model );
        draw_dictChanged( draw, xwe, -1, langDict );
    }
}

DrawCtx*
board_getDraw( const BoardCtxt* board )
{
    return board->draw;
}

void
board_writeToStream( const BoardCtxt* board, XWStreamCtxt* stream )
{
    XP_U16 nColsNBits;
#ifdef STREAM_VERS_BIGBOARD
    nColsNBits = 16 > model_numCols(board->model) ? NUMCOLS_NBITS_4
        : NUMCOLS_NBITS_5;
#else
    nColsNBits = NUMCOLS_NBITS_4;
#endif

    stream_putBits( stream, nColsNBits, board->sd[SCROLL_H].offset );
    stream_putBits( stream, nColsNBits, board->zoomCount );
    stream_putBits( stream, nColsNBits, board->sd[SCROLL_V].offset );
    stream_putBits( stream, 1, board->isFlipped );
    stream_putBits( stream, 1, board->gameOver );
    stream_putBits( stream, 1, board->showColors );
    stream_putBits( stream, 2, board->focussed );
#ifdef KEYBOARD_NAV
    stream_putBits( stream, 1, board->focusHasDived );
    stream_putBits( stream, 3, board->scoreCursorLoc );
#else
    stream_putBits( stream, 4, 0 );
#endif

    XP_ASSERT( !!board->server );
    XP_U16 nPlayers = board->gi->nPlayers;

    for ( int ii = 0; ii < nPlayers; ++ii ) {
        const PerTurnInfo* pti = &board->pti[ii];
        const BoardArrow* arrow = &pti->boardArrow;
        stream_putBits( stream, nColsNBits, arrow->col );
        stream_putBits( stream, nColsNBits, arrow->row );
        stream_putBits( stream, 1, arrow->vert );
        stream_putBits( stream, 1, arrow->visible );

        XP_ASSERT( CUR_STREAM_VERS == stream_getVersion(stream) );
        stream_putBits( stream, MAX_TRAY_TILES, pti->traySelBits );
        stream_putBits( stream, 1, pti->tradeInProgress );

#ifdef KEYBOARD_NAV
        stream_putBits( stream, 4, pti->bdCursor.col );
        stream_putBits( stream, 4, pti->bdCursor.row );
        stream_putBits( stream, 3, pti->trayCursorLoc );
#else
        stream_putBits( stream, 4+4+3, 0 );
#endif

#ifdef XWFEATURE_SEARCHLIMIT
        stream_putBits( stream, 1, pti->hasHintRect );
        if ( pti->hasHintRect ) {
            stream_putBits( stream, 4, pti->limits.left );
            stream_putBits( stream, 4, pti->limits.top );
            stream_putBits( stream, 4, pti->limits.right );
            stream_putBits( stream, 4, pti->limits.bottom );
        }
#endif
    }

    stream_putBits( stream, PLAYERNUM_NBITS, board->selPlayer );
    stream_putBits( stream, 2, board->trayVisState );
} /* board_writeToStream */

void
board_reset( BoardCtxt* board, XWEnv xwe )
{
    XP_U16 ii;
    XW_TrayVisState newState;

    XP_ASSERT( !!board->model );

    /* This is appropriate for a new game *ONLY*.  reset */
    for ( ii = 0; ii < MAX_NUM_PLAYERS; ++ii ) {
        PerTurnInfo* pti = &board->pti[ii];
        pti->traySelBits = 0;
        pti->tradeInProgress = XP_FALSE;
        XP_MEMSET( &pti->boardArrow, 0, sizeof(pti->boardArrow) );
    }
    board->gameOver = XP_FALSE;
    board->selPlayer = 0;
    board->selInfo = board->pti;
    board->star_row = (XP_U16)(model_numRows(board->model) / 2);

    newState = board->boardObscuresTray? TRAY_HIDDEN:TRAY_REVERSED;
    setTrayVisState( board, xwe, newState );

    board_invalAll( board );

    setTimerIf( board, xwe );
} /* board_reset */

void
board_drawSnapshot( const BoardCtxt* curBoard, XWEnv xwe, DrawCtx* dctx,
                    XP_U16 width, XP_U16 height )
{
    BoardCtxt* newBoard = board_make( MPPARM(curBoard->mpool) xwe,
                                      curBoard->model,
                                      curBoard->server, dctx, curBoard->util );
    board_setDraw( newBoard, xwe, dctx ); /* so draw_dictChanged() will get called */
    XP_U16 fontWidth = width / curBoard->gi->boardSize;
    board_figureLayout( newBoard, xwe, curBoard->gi, 0, 0, width, height,
                        100, 0, 0, 0, fontWidth, width, XP_FALSE, NULL );

    newBoard->showColors = curBoard->showColors;
    newBoard->showGrid = curBoard->showGrid;

    board_draw( newBoard, xwe );
    board_destroy( newBoard, xwe, XP_FALSE );
}

#ifdef COMMON_LAYOUT
# if 0
static void
printDims( const BoardDims* dimsp )
{
    XP_LOGF( "dims.left: %d", dimsp->left );
    XP_LOGF( "dims.top: %d", dimsp->top );
    XP_LOGF( "dims.width: %d", dimsp->width );
    XP_LOGF( "dims.height: %d", dimsp->height );


    XP_LOGF( "dims.scoreWidth: %d", dimsp->scoreWidth );
    XP_LOGF( "dims.scoreHt: %d", dimsp->scoreHt );

    XP_LOGF( "dims.boardWidth: %d", dimsp->boardWidth );
    XP_LOGF( "dims.boardHt: %d", dimsp->boardHt );

    XP_LOGF( "dims.trayLeft: %d", dimsp->trayLeft );
    XP_LOGF( "dims.trayTop: %d", dimsp->trayTop );
    XP_LOGF( "dims.trayWidth: %d", dimsp->trayWidth );
    XP_LOGF( "dims.trayHt: %d", dimsp->trayHt );

    /* XP_U16 cellSize, maxCellSize; */
    /* XP_U16 timerWidth; */
}
# else
#  define printDims( ldims )
# endif

/* For debugging the special case of square board */
// #define FORCE_SQUARE

void
board_figureLayout( BoardCtxt* board, XWEnv xwe, const CurGameInfo* gi,
                    XP_U16 bLeft, XP_U16 bTop,
                    XP_U16 bWidth, XP_U16 bHeight,
                    XP_U16 colPctMax, XP_U16 scorePct, XP_U16 trayPct,
                    XP_U16 scoreWidth, XP_U16 fontWidth, XP_U16 fontHt,
                    XP_Bool squareTiles, BoardDims* dimsp )
{
    BoardDims ldims = {0};

    XP_U16 nCells = gi->boardSize;
    XP_U16 maxCellSize = 8 * fontHt;
    XP_U16 trayHt;
    XP_U16 scoreHt;
    XP_U16 wantHt;
    XP_U16 nToScroll;

#ifdef FORCE_SQUARE
    if ( bWidth > bHeight ) {
        bWidth = bHeight;
    } else {
        bHeight = bWidth;
    }
#endif

    ldims.left = bLeft;
    ldims.top = bTop;
    ldims.width = bWidth;

    ldims.boardWidth = bWidth;
    for ( XP_Bool firstPass = XP_TRUE; ; ) {
        XP_U16 cellSize = ldims.boardWidth / nCells;
        if ( cellSize > maxCellSize ) {
            cellSize = maxCellSize;
            ldims.boardWidth = nCells * cellSize;
        }
        ldims.maxCellSize = maxCellSize;

        // Now determine if vertical scrolling will be necessary.
        // There's a minimum tray and scoreboard height.  If we can
        // fit them and all cells no scrolling's needed.  Otherwise
        // determine the minimum number that must be hidden to fit.
        // Finally grow scoreboard and tray to use whatever's left.
        scoreHt = (scorePct * cellSize) / 100;
        trayHt = (trayPct * cellSize) / 100;
        wantHt = trayHt + scoreHt + (cellSize * nCells);
        if ( wantHt <= bHeight ) {
            nToScroll = 0;
        } else {
            // Scrolling's required if we use cell width sufficient to
            // fill the screen.  But perhaps we don't need to.
            int cellWidth = 2 * (bHeight / ( 4 + 3 + (2*nCells)));
            if ( cellWidth < fontWidth ) {
                cellWidth = fontWidth;
            }
            if ( firstPass && cellWidth >= fontHt ) {
                firstPass = XP_FALSE;
                ldims.boardWidth = nCells * cellWidth;
                continue;
            } else {
                nToScroll = nCells - 
                    ((bHeight - trayHt - scoreHt) / cellSize);
            }
        }

        XP_U16 heightUsed = trayHt + scoreHt + (nCells - nToScroll) * cellSize;
        XP_U16 heightLeft = bHeight - heightUsed;
        if ( 0 < heightLeft ) {
            if ( heightLeft > (cellSize * 3 / 2) ) {
                heightLeft = cellSize * 3 / 2;
            }
            heightLeft /= 3;
            if ( 0 < scorePct ) {
                scoreHt += heightLeft;
            }
    
            if ( 0 < trayPct ) {
                trayHt += heightLeft * 2;
            }
            if ( squareTiles && trayHt > (bWidth / 7) ) {
                trayHt = bWidth / 7;
            }
            heightUsed = trayHt + scoreHt + ((nCells - nToScroll) * cellSize);
        }

        /* Figure tray width.  Tray tiles can be taller than wide, but not
           vice-versa. */
        XP_U16 tileWidth = DIVIDER_RATIO * DIVIDER_WIDTH(ldims.width);
        if ( tileWidth > trayHt ) {
            tileWidth = trayHt;
        }

        ldims.trayHt = trayHt;
        ldims.trayWidth = (tileWidth * MAX_TRAY_TILES) + (tileWidth / DIVIDER_RATIO);
        /* But: tray is never narrower than board */
        if ( ldims.trayWidth < ldims.boardWidth ) {
            ldims.trayWidth = ldims.boardWidth;
        }
        ldims.trayLeft = ldims.left + ((ldims.width - ldims.trayWidth) / 2);
        
        // totally arbitrary: don't let the scoreboard be more than 20% wider
        // than the board
        XP_U16 maxScoreWidth = (ldims.boardWidth * 5) / 4;
        if ( scoreWidth > maxScoreWidth ) {
            scoreWidth = maxScoreWidth;
        }
        ldims.scoreWidth = scoreWidth;
        ldims.scoreLeft = ldims.left + ((ldims.width - scoreWidth) / 2);
        ldims.scoreHt = scoreHt;

        ldims.boardHt = cellSize * nCells;
        ldims.trayTop = ldims.top + scoreHt + (cellSize * (nCells-nToScroll));
        ldims.traySize = gi->traySize;
        ldims.height =
#ifdef FORCE_SQUARE
            ldims.width
#else
            heightUsed
#endif
            ;
        ldims.cellSize = cellSize;

        if ( gi->timerEnabled ) {
            ldims.timerWidth = fontWidth * XP_STRLEN("-00:00");
            ldims.scoreWidth -= ldims.timerWidth;
        }
        break;
    }

#ifdef XWFEATURE_WIDER_COLS
    ldims.boardWidth = (ldims.boardWidth * colPctMax) / 100;
    if ( ldims.boardWidth > bWidth ) {
        ldims.boardWidth = bWidth;
    }

    if ( 0 == nToScroll &&
         bHeight > (ldims.scoreHt + ldims.boardHt + ldims.trayHt) ) {
        XP_U16 oldTop = ldims.trayTop;
        ldims.trayTop = ldims.scoreHt + (ldims.boardHt * colPctMax) / 100;
        if ( ldims.trayTop + ldims.trayHt > bHeight ) {
            ldims.trayTop = bHeight - ldims.trayHt;
        }
        XP_ASSERT( oldTop <= ldims.trayTop );
        ldims.height += ldims.trayTop - oldTop;
    }
#else
    XP_USE(colPctMax);
#endif

    printDims( &ldims );

    if ( !!dimsp ) {
        *dimsp = ldims;
    } else {
        board_applyLayout( board, xwe, &ldims );
    }
} /* board_figureLayout */

void
board_applyLayout( BoardCtxt* board, XWEnv xwe, const BoardDims* dims )
{
    XP_U16 margin = (dims->width - dims->boardWidth) / 2;
    board_setPos( board, xwe, dims->left + margin, dims->top + dims->scoreHt,
                  dims->boardWidth, dims->boardHt,
                  dims->maxCellSize, XP_FALSE );

    board_setScoreboardLoc( board, dims->scoreLeft, dims->top,
                            dims->scoreWidth, dims->scoreHt, XP_TRUE );

    board_setTimerLoc( board, dims->scoreLeft + dims->scoreWidth,
                       dims->top, dims->timerWidth, dims->scoreHt );

    board_setTrayLoc( board, xwe, dims->trayLeft, dims->trayTop,
                      dims->trayWidth, dims->trayHt, dims->traySize );
}
#endif

void
board_setCallbacks( BoardCtxt* board, XWEnv xwe )
{
    model_setBoardListener( board->model, boardCellChanged, board );
    model_setTrayListener( board->model, boardTilesChanged, board );
    model_setDictListener( board->model, dictChanged, board );
    server_setTurnChangeListener( board->server, boardTurnChanged, board );
    server_setGameOverListener( board->server, boardGameOver, board );

    setTimerIf( board, xwe );
}

static void
board_setPos( BoardCtxt* board, XWEnv xwe, XP_U16 left, XP_U16 top,
              XP_U16 width, XP_U16 height, XP_U16 maxCellSz,
              XP_Bool leftHanded )
{
    /* XP_LOGF( "%s(%d,%d,%d,%d)", __func__, left, top, width, height ); */

    board->boardBounds.left = left;
    board->boardBounds.top = top;
    board->boardBounds.width = width;
    board->heightAsSet = height;
    board->maxCellSz = maxCellSz;
    board->leftHanded = leftHanded;

    figureBoardRect( board, xwe );
} /* board_setPos */

static void
board_setTimerLoc( BoardCtxt* board, 
                   XP_U16 timerLeft, XP_U16 timerTop,
                   XP_U16 timerWidth, XP_U16 timerHeight )
{
    board->timerBounds.left = timerLeft;
    board->timerBounds.top = timerTop;
    board->timerBounds.width = timerWidth;
    board->timerBounds.height = timerHeight;
} /* board_setTimerLoc */

#if 0
void 
board_getScale( BoardCtxt* board, XP_U16* hScale, XP_U16* vScale )
{
    *hScale = board->sd[SCROLL_H].scale;
    *vScale = board->sd[SCROLL_V].scale;
} /* board_getScale */
#endif

XP_Bool
board_prefsChanged( BoardCtxt* board, const CommonPrefs* cp )
{
    XP_Bool showArrowChanged = cp->showBoardArrow == board->disableArrow;
    XP_Bool hideValChanged = cp->hideTileValues != board->hideValsInTray;
    XP_Bool showColorsChanged = board->showColors != cp->showColors;
    XP_Bool showValsChanged = board->tvType != cp->tvType;

    board->disableArrow = !cp->showBoardArrow;
    board->hideValsInTray = cp->hideTileValues;
    board->skipCommitConfirm = cp->skipCommitConfirm;
    board->showColors = cp->showColors;
    board->allowPeek = cp->allowPeek;
#ifdef XWFEATURE_CROSSHAIRS
    board->hideCrosshairs = cp->hideCrosshairs;
#endif
    board->tvType = cp->tvType;

    if ( showArrowChanged ) {
        showArrowChanged = setArrowVisible( board, XP_FALSE );
    }
    if ( hideValChanged ) {
        board_invalTrayTiles( board, ALLTILES );
    }
    if ( showColorsChanged || showValsChanged ) {
        board->scoreBoardInvalid = XP_TRUE;
        showColorsChanged = invalCellsWithTiles( board );
    }

#ifdef XWFEATURE_SEARCHLIMIT
    if ( !board->gi->allowHintRect && board->selInfo->hasHintRect ) {

        EngineCtxt* engine = server_getEngineFor( board->server, 
                                                  board->selPlayer );
        if ( !!engine ) {
            engine_reset( engine );
        }

        clearCurHintRect( board );
    }
#endif

    return showArrowChanged || hideValChanged || showColorsChanged;
} /* board_prefsChanged */

XP_Bool
adjustXOffset( BoardCtxt* board, XP_S16 moveBy )
{
    XP_Bool changed = XP_FALSE;
    if ( 0 != moveBy ) {
        XP_U16 nCols = model_numCols(board->model);
        XP_U16 nVisible = nCols - board->zoomCount;
        ScrollData* hsd = &board->sd[SCROLL_H];
        XP_S16 newOffset = hsd->offset - moveBy;

        if ( newOffset < 0 ) {
            newOffset = 0;
        } else if ( newOffset + nVisible > nCols ) {
            newOffset = nCols - nVisible;
        }

        changed = board_setXOffset( board, newOffset );
    }
    return changed;
} /* adjustXOffset */

XP_Bool
adjustYOffset( BoardCtxt* board, XWEnv xwe, XP_S16 moveBy )
{
    ScrollData* vsd = &board->sd[SCROLL_V];
    XP_U16 nVisible = vsd->lastVisible - vsd->offset + 1;
    XP_U16 nRows = model_numRows(board->model);
    XP_S16 newOffset = vsd->offset - moveBy;

    if ( newOffset < 0 ) {
        newOffset = 0;
    } else if ( newOffset + nVisible > nRows ) {
        newOffset = nRows - nVisible;
    }

    return board_setYOffset( board, xwe, newOffset );
} /* adjustYOffset */

static XP_Bool
board_setXOffset( BoardCtxt* board, XP_U16 offset )
{
    ScrollData* hsd = &board->sd[SCROLL_H];
    XP_Bool changed = offset != hsd->offset;
    if ( changed ) {
        hsd->offset = offset;
        hsd->lastVisible = model_numCols(board->model)
            - board->zoomCount + offset - 1;
        board_invalAll( board );
    }
    return changed;
}

XP_Bool
board_setYOffset( BoardCtxt* board, XWEnv xwe, XP_U16 offset )
{
    ScrollData* vsd = &board->sd[SCROLL_V];
    XP_U16 oldOffset = vsd->offset;
    XP_Bool result = oldOffset != offset;

    if ( result ) {
        XP_U16 nVisible = vsd->lastVisible - vsd->offset + 1;
        XP_U16 nRows = model_numRows(board->model);

        result = offset <= nRows - nVisible;
        if ( result ) {
            invalSelTradeWindow( board );
            vsd->offset = offset;
            figureBoardRect( board, xwe );
            util_yOffsetChange( board->util, xwe, vsd->maxOffset,
                                oldOffset, offset );
            invalSelTradeWindow( board );
            board->needsDrawing = XP_TRUE;
        }
    }

    return result;
} /* board_setYOffset */

XP_U16
board_getYOffset( const BoardCtxt* board )
{
    const ScrollData* vsd = &board->sd[SCROLL_V];
    return vsd->offset;
} /* board_getYOffset */

XP_Bool
board_curTurnSelected( const BoardCtxt* board )
{
    return server_isPlayersTurn( board->server, board->selPlayer );
}

XP_U16
board_visTileCount( const BoardCtxt* board )
{
    return model_visTileCount( board->model, board->selPlayer, 
                               TRAY_REVEALED == board->trayVisState );
}

void
board_pause( BoardCtxt* board, XWEnv xwe, const XP_UCHAR* msg )
{
    server_pause( board->server, xwe, board->selPlayer, msg );
    board_invalAll( board );
}

void
board_unpause( BoardCtxt* board, XWEnv xwe, const XP_UCHAR* msg )
{
    server_unpause( board->server, xwe, board->selPlayer, msg );
    setTimerIf( board, xwe );
    board_invalAll( board );
}

XP_Bool
board_canShuffle( const BoardCtxt* board )
{
    return model_canShuffle( board->model, board->selPlayer, 
                             TRAY_REVEALED == board->trayVisState );
}

XP_Bool
board_canHideRack( const BoardCtxt* board )
{
    XP_Bool result = 0 <= server_getCurrentTurn( board->server, NULL )
        && (board->boardObscuresTray || !board->gameOver);
    return result;
}

XP_Bool
board_canTrade( BoardCtxt* board, XWEnv xwe )
{
    XP_Bool result = preflight( board, xwe, XP_FALSE )
        && !board->gi->inDuplicateMode
        && MIN_TRADE_TILES(board->gi) <= server_countTilesInPool( board->server );
    return result;
}

XP_Bool
board_canTogglePending( const BoardCtxt* board )
{
    return TRAY_REVEALED == board->trayVisState
        && model_canTogglePending( board->model, board->selPlayer );
}

XP_Bool
board_canHint( const BoardCtxt* board )
{
    XP_Bool canHint = !board->gi->hintsNotAllowed
        && 0 < model_getNumTilesTotal( board->model, board->selPlayer )
        && ! board->pti[board->selPlayer].tradeInProgress;
    if ( canHint ) {
        const LocalPlayer* lp = &board->gi->players[board->selPlayer];
        canHint = lp->isLocal && !LP_IS_ROBOT(lp);
    }
    return canHint;
}

#ifdef XWFEATURE_CHAT
void
board_sendChat( const BoardCtxt* board, XWEnv xwe, const XP_UCHAR* msg )
{
    server_sendChat( board->server, xwe, msg, board->selPlayer );
}
#endif

static XP_U16
adjustOffset( XP_U16 curOffset, XP_S16 zoomBy )
{
    XP_S16 offset = curOffset;
    offset += zoomBy / 2;
    if ( offset < 0 ) {
        offset = 0;
    }
    return offset;
}

static XP_Bool
canZoomIn( const BoardCtxt* board, XP_S16 newCount )
{
    XP_Bool canZoom = XP_TRUE;
    XP_U16 nVisCols = model_numCols( board->model ) - newCount;
    XP_U16 scale = board->boardBounds.width / nVisCols;
    if ( scale > board->maxCellSz ) {
        canZoom = XP_FALSE;
    }
    return canZoom;
}

XP_Bool
board_zoom( BoardCtxt* board, XWEnv xwe, XP_S16 zoomBy, XP_Bool* canInOut )
{
    XP_Bool changed;
    XP_S16 zoomCount = board->zoomCount;
    ScrollData* hsd = &board->sd[SCROLL_H];
    ScrollData* vsd = &board->sd[SCROLL_V];

    XP_U16 maxCount = model_numCols( board->model ) - MAX_BOARD_ZOOM;
    if ( board->boardBounds.width > board->boardBounds.height ) {
        XP_U16 ratio = board->boardBounds.width / board->boardBounds.height;
        maxCount -= ratio;
    }

    zoomCount += zoomBy;
    if ( zoomCount < 0 ) {
        zoomCount = 0;
    } else if ( zoomCount > maxCount ) {
        zoomCount = maxCount;
    }

    changed = zoomCount != board->zoomCount;

    /* If we're zooming in, make sure we'll stay inside the limit */
    if ( changed && zoomBy > 0 ) {
        while ( zoomCount > 0 && !canZoomIn( board, zoomCount ) ) {
            --zoomCount;
        }
    }
    changed = zoomCount != board->zoomCount;

    if ( changed ) {
        /* Try to distribute the zoom */
        hsd->offset = adjustOffset( hsd->offset, zoomBy );
        vsd->offset = adjustOffset( vsd->offset, zoomBy );

        board->zoomCount = zoomCount;
        figureBoardRect( board, xwe );
        board_invalAll( board );
    }

    if ( !!canInOut ) {
        canInOut[0] = canZoomIn( board, zoomCount + zoomBy );
        canInOut[1] = zoomCount > 0;
    }

    return changed;
} /* board_zoom */

#ifdef KEYBOARD_NAV
/* called by ncurses version only */
BoardObjectType
board_getFocusOwner( BoardCtxt* board )
{
    return board->focussed;
} /* board_getFocusOwner */
#endif

static void
invalArrowCell( BoardCtxt* board )
{
    BoardArrow* arrow = &board->selInfo->boardArrow;
    invalCell( board, arrow->col, arrow->row );
} /* invalArrowCell */

static void
flipArrow( BoardCtxt* board )
{
    BoardArrow* arrow = &board->selInfo->boardArrow;
    XP_U16 tmp = arrow->col;
    arrow->col = arrow->row;
    arrow->row = tmp;
    arrow->vert = !arrow->vert;
} /* flipArrow */

#ifdef KEYBOARD_NAV
static void
invalCursorCell( BoardCtxt* board )
{
    BdCursorLoc loc = board->selInfo->bdCursor;
    invalCell( board, loc.col, loc.row );
} /* invalCursorCell */
#endif

#ifdef XWFEATURE_MINIWIN
static void
invalTradeWindow( BoardCtxt* board, XP_S16 turn, XP_Bool redraw )
{
    XP_ASSERT( turn >= 0 );

    if ( board->pti[turn].tradeInProgress ) {
        MiniWindowStuff* stuff = &board->miniWindowStuff[MINIWINDOW_TRADING];
        invalCellsUnderRect( board, &stuff->rect );
        if ( redraw ) {
            board->tradingMiniWindowInvalid = XP_TRUE;
        }
    }
} /* invalTradeWindow */

void
invalSelTradeWindow( BoardCtxt* board )
{
    invalTradeWindow( board, board->selPlayer, 
                      board->trayVisState == TRAY_REVEALED );
} /* invalSelTradeWindow */

#if defined POINTER_SUPPORT || defined KEYBOARD_NAV
void
hideMiniWindow( BoardCtxt* board, XP_Bool destroy, MiniWindowType winType )
{
    MiniWindowStuff* stuff = &board->miniWindowStuff[winType];

    invalCellsUnderRect( board, &stuff->rect );

    if ( destroy ) {
        stuff->text = (XP_UCHAR*)NULL;
    }
} /* hideMiniWindow */
#endif
#endif

static void
saveBadWords( const WNParams* wnp, void* closure )
{
    if ( !wnp->isLegal ) {
        BadWordList* bwlp = (BadWordList*)closure;
        bwlp->bwi.words[bwlp->bwi.nWords] = &bwlp->buf[bwlp->index];
        XP_STRCAT( &bwlp->buf[bwlp->index], wnp->word );
        bwlp->index += XP_STRLEN(wnp->word) + 1;
        ++bwlp->bwi.nWords;
    }
} /* saveBadWords */

static void
boardNotifyTrade( BoardCtxt* board, XWEnv xwe, const TrayTileSet* tiles )
{
    const XP_UCHAR* tfaces[MAX_TRAY_TILES];
    XP_U16 ii;
    const DictionaryCtxt* dict = model_getDictionary( board->model );

    for ( ii = 0; ii < tiles->nTiles; ++ii ) {
        tfaces[ii] = dict_getTileString( dict, tiles->tiles[ii] );
    }

    util_notifyTrade( board->util, xwe, tfaces, tiles->nTiles );
}

XP_Bool
board_commitTurn( BoardCtxt* board, XWEnv xwe,
                  const PhoniesConf* pconf,
                  XP_Bool turnConfirmed /* includes trade */,
                  TrayTileSet* newTiles )
{
    XP_Bool result = XP_FALSE;
    const XP_S16 turn = server_getCurrentTurn( board->server, NULL );
    const XP_U16 selPlayer = board->selPlayer;
    ModelCtxt* model = board->model;
    const XP_Bool phoniesConfirmed = !!pconf && pconf->confirmed;

    if ( board->gameOver || turn < 0 ) {
        /* do nothing */
    } else if ( !server_isPlayersTurn( board->server, selPlayer ) ) {
        util_userError( board->util, xwe, ERR_NOT_YOUR_TURN );
    } else if ( 0 == model_getNumTilesTotal( model, selPlayer ) ) {
        /* game's over but still undoable so turn hasn't changed; do
           nothing */
    } else if ( phoniesConfirmed || turnConfirmed || checkRevealTray( board, xwe ) ) {
        const DictionaryCtxt* dict = model_getPlayerDict( model, selPlayer );
        BadWordList* bwl = &board->bwl;

        if ( phoniesConfirmed && 0 != pconf->key ) {
            XP_ASSERT( bwl->key == pconf->key );
            if ( bwl->key == pconf->key ) {
                const BadWordInfo* bwi = &bwl->bwi;
                const XP_UCHAR* isoCode = dict_getISOCode( dict );

                for ( int ii = 0; ii < bwi->nWords; ++ii ) {
                    dvc_addLegalPhony( board->dutil, xwe, isoCode,
                                       bwi->words[ii] );
                }
            }
        }

        PerTurnInfo* pti = &board->pti[selPlayer];
        if ( pti->tradeInProgress ) {
            TileBit traySelBits = pti->traySelBits;
            int count = 0;
            for ( TileBit tmp = traySelBits; tmp != 0; tmp &= tmp - 1 ) {
                ++count;
            }
            result = XP_TRUE; /* there's at least the window to clean up
                                 after */

            if ( NO_TILES == traySelBits ) {
                util_userError( board->util, xwe, ERR_NO_EMPTY_TRADE );
            } else if ( count > server_countTilesInPool(board->server) ) {
                util_userError( board->util, xwe, ERR_TOO_MANY_TRADE );
            } else {
                TrayTileSet selTiles;
                getSelTiles( board, traySelBits, &selTiles );
                if ( turnConfirmed ) {
                    if ( !server_askPickTiles( board->server, xwe, selPlayer, newTiles,
                                               selTiles.nTiles ) ) {
                        /* server_commitTrade() changes selPlayer, so board_endTrade
                           must be called first() */
                        (void)board_endTrade( board );

                        (void)server_commitTrade( board->server, xwe, &selTiles,
                                                  newTiles );
                    }
                } else {
                    boardNotifyTrade( board, xwe, &selTiles );
                }
            }
        } else {
            XWStreamCtxt* stream = NULL;
            XP_Bool legal = turnConfirmed;
            
            if ( !legal ) {
                stream = mem_stream_make_raw( MPPARM(board->mpool)
                                              dutil_getVTManager(board->dutil) );

                XP_U16 stringCode = board->gi->inDuplicateMode
                    ? STR_SUBMIT_CONFIRM : STR_COMMIT_CONFIRM;
                const XP_UCHAR* str = dutil_getUserString( board->dutil, xwe,
                                                           stringCode );
                stream_catString( stream, str );

                XP_Bool warn = board->util->gameInfo->phoniesAction == PHONIES_WARN;
                WordNotifierInfo info;
                if ( warn ) {
                    XP_MEMSET( bwl, 0, sizeof(*bwl) );
                    bwl->key = 0x7FFFFFFF & XP_RANDOM(); /* clear high bit so can be signed */
                    info.proc = saveBadWords;
                    info.closure = bwl;
                }
                legal = model_checkMoveLegal( model, xwe, selPlayer, stream,
                                              warn? &info:(WordNotifierInfo*)NULL);
            }

            if ( 0 < bwl->bwi.nWords && !phoniesConfirmed ) {
                const XP_UCHAR* dictName = dict_getShortName( dict );
                util_notifyIllegalWords( board->util, xwe, &bwl->bwi, dictName,
                                         selPlayer, XP_FALSE, bwl->key );
            } else if ( legal ) {
                /* Hide the tray so no peeking.  Leave it hidden even if user
                   cancels as otherwise another player could get around
                   passwords and peek at tiles. */
                if ( !turnConfirmed
                     && gi_countLocalPlayers( board->gi, XP_TRUE ) > 1 ) {
                    result = board_hideTray( board, xwe );
                }

                if ( board->skipCommitConfirm || turnConfirmed ) {
                    XP_U16 nToPick = board->gi->traySize
                        - model_getNumTilesInTray( model, selPlayer );
                    if ( !server_askPickTiles( board->server, xwe, selPlayer, newTiles,
                                               nToPick ) ) {
                        result = server_commitMove( board->server, xwe, selPlayer,
                                                    newTiles )
                            || result;
                        /* invalidate all tiles in case we'll be drawing this tray
                           again rather than some other -- as is the case in a
                           two-player game where one's a robot. We really only
                           need the selected tiles and the rightmost (in case it's
                           showing points-this-turn), but this is easier. */
                        board_invalTrayTiles( board, ALLTILES );
                        pti->traySelBits = 0x00;
                    }
                } else {
                    util_notifyMove( board->util, xwe, stream );
                }
            }

            if ( NULL != stream ) {
                stream_destroy( stream );
            }

            if ( result ) {
                setArrowVisibleFor( board, selPlayer, XP_FALSE );
            }
        }
    }
    return result;
} /* board_commitTurn */

/* Make all the changes necessary for the board to reflect the currently
 * selected player.  Many slots are duplicated on a per-player basis, e.g.
 * cursor and tray traySelBits.  Others, such as the miniwindow stuff, are
 * singletons that may have to be hidden or shown.
 */
static void
selectPlayerImpl( BoardCtxt* board, XWEnv xwe, XP_U16 newPlayer, XP_Bool reveal,
                  XP_Bool canPeek )
{
    XP_Bool isLocal;
    XP_S16 curTurn = server_getCurrentTurn( board->server, &isLocal );
    if ( !board->gameOver && curTurn < 0 ) {
        /* game not started yet; do nothing */
    } else if ( board->selPlayer == newPlayer ) {
        if ( reveal ) {
            checkRevealTray( board, xwe );
        }
    } else if ( canPeek || ((newPlayer == curTurn) && isLocal)) {
        PerTurnInfo* newInfo = &board->pti[newPlayer];
        XP_U16 oldPlayer = board->selPlayer;
        model_foreachPendingCell( board->model, xwe, newPlayer,
                                  boardCellChanged, board );
        model_foreachPendingCell( board->model, xwe, oldPlayer,
                                  boardCellChanged, board );

        /* if there are pending cells on one view and not the other, then the
           previous move will be drawn highlighted on one and not the other
           and so needs to be invalidated so it'll get redrawn.*/
        if ( (0 == model_getCurrentMoveCount( board->model, newPlayer ))
             != (0 == model_getCurrentMoveCount( board->model, oldPlayer )) ) {
            model_foreachPrevCell( board->model, xwe, boardCellChanged, board );
        }

        /* Just in case somebody started a trade when it wasn't his turn and
           there were plenty of tiles but now there aren't. */
        if ( newInfo->tradeInProgress && 
             server_countTilesInPool(board->server) < MIN_TRADE_TILES(board->gi) ) {
            newInfo->tradeInProgress = XP_FALSE;
            newInfo->traySelBits = 0x00; /* clear any selected */
        }

        invalTradeWindow( board, oldPlayer, newInfo->tradeInProgress );

#ifdef XWFEATURE_SEARCHLIMIT
        if ( board->pti[oldPlayer].hasHintRect ) {
            invalCurHintRect( board, oldPlayer );
        }
        if ( newInfo->hasHintRect ) {
            invalCurHintRect( board, newPlayer );
        }
#endif

        invalArrowCell( board );
        board->selPlayer = (XP_U8)newPlayer;
        board->selInfo = newInfo;
        invalArrowCell( board );

        board_invalTrayTiles( board, ALLTILES );
        board->dividerInvalid = XP_TRUE;

        setTrayVisState( board, xwe, TRAY_REVERSED );
    }
    board->scoreBoardInvalid = XP_TRUE; /* if only one player, number of
                                           tiles remaining may have changed*/
} /* selectPlayerImpl */

void
board_selectPlayer( BoardCtxt* board, XWEnv xwe, XP_U16 newPlayer, XP_Bool canSwitch )
{
    selectPlayerImpl( board, xwe, newPlayer, XP_TRUE, canSwitch );
} /* board_selectPlayer */

void
board_hiliteCellAt( BoardCtxt* board, XWEnv xwe, XP_U16 col, XP_U16 row )
{
    XP_Rect cellRect;

    flipIf( board, col, row, &col, &row );
    if ( getCellRect( board, col, row, &cellRect ) ) {
        draw_invertCell( board->draw, xwe, &cellRect );
        invalCell( board, col, row );
    }
    /*     sleep(1); */
} /* board_hiliteCellAt */

void
board_resetEngine( BoardCtxt* board )
{
    server_resetEngine( board->server, board->selPlayer );
} /* board_resetEngine */

XP_Bool
board_setBlankValue( BoardCtxt* board, XP_U16 player, XP_U16 col, XP_U16 row,
                     XP_U16 tileIndex )
{
    XP_Bool draw = model_setBlankValue( board->model, player, col, row,
                                        tileIndex );
    if ( draw ) {
        invalCell( board, col, row );
    }
    return draw;
}

#ifdef XWFEATURE_MINIWIN
/* Find a rectangle either centered on the board or pinned to the point at
 * which the mouse went down.
 */
static void
positionMiniWRect( BoardCtxt* board, XP_Rect* rect, XP_Bool center )
{
    if ( center ) {
        XP_Rect boardBounds = board->boardBounds;
        rect->top = boardBounds.top + ((boardBounds.height - rect->height)/2);
        rect->left = boardBounds.left + ((boardBounds.width - rect->width)/2);
    } else {
        XP_S16 x = board->penDownX;
        XP_S16 y = board->penDownY;

        if ( board->leftHanded ) {
            rect->left = (x + rect->width <= board->boardBounds.width)?
                x : x - rect->width;
        } else {
            rect->left = x - rect->width > 0? x - rect->width : x;
        }
        rect->left = XP_MAX( board->boardBounds.left + 1, rect->left );
        rect->top = XP_MAX( board->boardBounds.top + 1, y - rect->height );
    }
    forceRectToBoard( board, rect );
} /*positionMiniWRect */
#endif

static XP_Bool
timerFiredForPen( BoardCtxt* board, XWEnv xwe )
{
    XP_Bool draw = XP_FALSE;
    const XP_UCHAR* text = (XP_UCHAR*)NULL;
#ifdef XWFEATURE_MINIWIN
    XP_UCHAR buf[80];
#endif

    if ( board->penDownObject == OBJ_BOARD ) {
        if ( board_inTrade( board, NULL ) ) {
            /* do nothing */
        } else if ( !dragDropInProgress( board ) 
                    || !dragDropHasMoved( board ) ) {
            XP_U16 col, row;
            coordToCell( board, board->penDownX, board->penDownY, &col, &row );

            if ( dragDropIsBeingDragged( board, col, row, NULL ) ) {
                /* even if we aren't calling dragDropSetAdd we want to avoid
                   putting up a square bonus if we're on a square with
                   something that can be dragged */
#ifdef XWFEATURE_RAISETILE
                draw = dragDropSetAdd( board, xwe );
#endif
            }

            /* We calculate words even for a pending tile set, meaning
               dragDrop might be happening too. */
            XP_Bool listWords = XP_FALSE;
#ifdef XWFEATURE_BOARDWORDS
            XP_U16 modelCol, modelRow;
            flipIf( board, col, row, &modelCol, &modelRow );
            listWords = model_getTile( board->model, modelCol, modelRow,
                                       XP_TRUE, board->selPlayer, NULL,
                                       NULL, NULL, NULL );
            if ( listWords ) {
                XWStreamCtxt* stream =
                    mem_stream_make_raw( MPPARM(board->mpool)
                                         dutil_getVTManager(board->dutil) );
                listWords = model_listWordsThrough( board->model, xwe, modelCol, modelRow,
                                                    board->selPlayer, stream );
                if ( listWords ) {
                    util_cellSquareHeld( board->util, xwe, stream );
                    if ( dragDropInProgress( board ) ) {
                        dragDropEnd( board, xwe, board->penDownX, board->penDownY, NULL );
                    }
                }
                stream_destroy( stream );
            }
#endif
            if ( !listWords ) {
                XWBonusType bonus;
                bonus = model_getSquareBonus( board->model, col, row );
                if ( bonus != BONUS_NONE ) {
#ifdef XWFEATURE_MINIWIN
                    text = draw_getMiniWText( board->draw,
                                              (XWMiniTextType)bonus );
#else
                    util_bonusSquareHeld( board->util, xwe, bonus );
#endif
                }
            }
            board->penTimerFired = XP_TRUE;
        }
    } else if ( board->penDownObject == OBJ_SCORE ) {
        penTimerFiredScore( board, xwe );
        board->penTimerFired = XP_TRUE;
    }

    if ( !!text ) {
#ifdef XWFEATURE_MINIWIN
        MiniWindowStuff* stuff = &board->miniWindowStuff[MINIWINDOW_VALHINT];
        makeMiniWindowForText( board, text, MINIWINDOW_VALHINT );
        XP_ASSERT( stuff->text == text );
        draw_drawMiniWindow(board->draw, text, &stuff->rect, 
                            &stuff->closure);
        if ( dragDropInProgress( board ) ) {
            XP_Bool dragged;
            dragDropEnd( board, board->penDownX, board->penDownY, &dragged );
            XP_ASSERT( !dragged );
        }
#endif
    }
    return draw;
} /* timerFiredForPen */

static void
setTimerIf( BoardCtxt* board, XWEnv xwe )
{
    XP_Bool timerWanted = board->gi->timerEnabled
        && !board->gameOver
        && !server_canUnpause( board->server );

    if ( timerWanted && !board->timerPending ) {
        util_setTimer( board->util, xwe, TIMER_TIMERTICK, 0,
                       p_board_timerFired, board ); 
        board->timerPending = XP_TRUE;
    }
} /* setTimerIf */

static void
timerFiredForTimer( BoardCtxt* board, XWEnv xwe )
{
    board->timerPending = XP_FALSE;
    if ( !board->gameOver || !server_canUnpause( board->server ) ) {
        XP_Bool doDraw = board->gi->inDuplicateMode;
        if ( !doDraw ) {
            XP_S16 turn = server_getCurrentTurn( board->server, NULL );

            if ( turn >= 0 ) {
                ++board->gi->players[turn].secondsUsed;

                doDraw = turn == board->selPlayer;
            }
        }
        if ( doDraw ) {
            drawTimer( board, xwe );
        }
    }
    setTimerIf( board, xwe );
} /* timerFiredForTimer */

static XP_Bool
p_board_timerFired( void* closure, XWEnv xwe, XWTimerReason why )
{
    XP_Bool draw = XP_FALSE;
    BoardCtxt* board = (BoardCtxt*)closure;
    if ( why == TIMER_PENDOWN ) {
        draw = timerFiredForPen( board, xwe );
    } else {
        XP_ASSERT( why == TIMER_TIMERTICK );
        timerFiredForTimer( board, xwe );
    }
    return draw;
} /* board_timerFired */

#ifdef XWFEATURE_RAISETILE
static XP_Bool
p_tray_timerFired( void* closure, XWEnv xwe, XWTimerReason why )
{
    XP_Bool draw = XP_FALSE;
    BoardCtxt* board = (BoardCtxt*)closure;
    if ( why == TIMER_PENDOWN ) {
        draw = dragDropSetAdd( board, xwe );
    }
    return draw;
}
#endif

void
board_pushTimerSave( BoardCtxt* board, XWEnv xwe )
{
    if ( board->gi->timerEnabled ) {
        if ( board->timerSaveCount++ == 0 ) {
            board->timerStoppedTime = dutil_getCurSeconds( board->dutil, xwe );
#ifdef DEBUG
            board->timerStoppedTurn = server_getCurrentTurn( board->server,
                                                             NULL );
#endif
        }
    }
} /* board_pushTimerSave */

void
board_popTimerSave( BoardCtxt* board, XWEnv xwe )
{
    if ( board->gi->timerEnabled ) {

        /* it's possible for the count to be 0, if e.g. the timer was enabled
           between calls to board_pushTimerSave and this call, as can happen on
           franklin.  So that's not an error. */
        if ( board->timerSaveCount > 0 ) {
            XP_S16 turn = server_getCurrentTurn( board->server, NULL );

            XP_ASSERT( board->timerStoppedTurn == turn );

            if ( --board->timerSaveCount == 0 && turn >= 0 ) {
                XP_U32 curTime = dutil_getCurSeconds( board->dutil, xwe );
                XP_U32 elapsed;

                XP_ASSERT( board->timerStoppedTime != 0 );
                elapsed = curTime - board->timerStoppedTime;
                XP_LOGF( "board_popTimerSave: elapsed=%d", elapsed );
                XP_ASSERT( elapsed <= 0xFFFF );
                board->gi->players[turn].secondsUsed += (XP_U16)elapsed;
            }
        }
    }
} /* board_popTimerSave */

/* Figure out if the current player's tiles should be excluded, then call
 * server to format.
 */
void
board_formatRemainingTiles( BoardCtxt* board, XWEnv xwe, XWStreamCtxt* stream )
{
    XP_S16 curPlayer = board->selPlayer;
    if ( board->trayVisState != TRAY_REVEALED ) {
        curPlayer = -1;
    }
    server_formatRemainingTiles( board->server, xwe, stream, curPlayer );
} /* board_formatRemainingTiles */

static void
board_invalAllTiles( BoardCtxt* board )
{
    XP_U16 lastRow = model_numRows( board->model );
    while ( lastRow-- ) {
        board->redrawFlags[lastRow] = ~0;
    }
#ifdef XWFEATURE_MINIWIN
    board->tradingMiniWindowInvalid = XP_TRUE;
#endif

    board->needsDrawing = XP_TRUE;
} /* board_invalAllTiles */

#ifdef KEYBOARD_NAV
#ifdef PERIMETER_FOCUS
static void
invalPerimeter( BoardCtxt* board )
{
    ScrollData* hsd = &board->sd[SCROLL_H];
    XP_U16 firstCol = hsd->offset;
    XP_U16 lastCol = hsd->lastVisible;
    RowFlags firstAndLast = (1 << lastCol) | (1 << firstCol);
    ScrollData* vsd = &board->sd[SCROLL_V];
    XP_U16 firstRow = vsd->offset;
    XP_U16 lastRow = vsd->lastVisible;

    /* top and bottom rows */
    board->redrawFlags[firstRow] = ~0;
    board->redrawFlags[lastRow] = ~0;
    
    while ( --lastRow > firstRow ) {
        board->redrawFlags[lastRow] |= firstAndLast;
    }
} /* invalPerimeter */
#endif
#endif

void
board_invalAll( BoardCtxt* board )
{
    board_invalAllTiles( board );

    board_invalTrayTiles( board, ALLTILES );
    board->dividerInvalid = XP_TRUE;
    board->scoreBoardInvalid = XP_TRUE;
} /* board_invalAll */

void
flipIf( const BoardCtxt* board, XP_U16 col, XP_U16 row, 
        XP_U16* fCol, XP_U16* fRow )
{
    XP_U16 tmp = col;           /* might be the same */
    if ( board->isFlipped ) {
        *fCol = row;
        *fRow = tmp;
    } else {
        *fRow = row;
        *fCol = tmp;
    }
} /* flipIf */

#ifdef XWFEATURE_SEARCHLIMIT
static void
flipLimits( BdHintLimits* lim )
{
    XP_U16 tmp = lim->left;
    lim->left = lim->top;
    lim->top = tmp;
    tmp = lim->right;
    lim->right = lim->bottom;
    lim->bottom = tmp;
}

static void
flipAllLimits( BoardCtxt* board )
{
    XP_U16 nPlayers = board->gi->nPlayers;
    XP_U16 ii;
    for ( ii = 0; ii < nPlayers; ++ii ) {
        flipLimits( &board->pti[ii].limits );
    }
}
#endif

/* 
 * invalidate all cells that contain a tile.  Return TRUE if any invalidation
 * actually happens.
 */
static XP_Bool
invalCellsWithTiles( BoardCtxt* board )
{
    ModelCtxt* model = board->model;
    XP_S16 turn = board->selPlayer;
    XP_Bool includePending = board->trayVisState == TRAY_REVEALED;
    XP_S16 col, row;

    /* For each col,row pair, invalidate it if it holds a tile.  Previously we
     * optimized by checking that the tile was actually visible, but with
     * flipping and now boards obscured by more than just the tray that's hard
     * to get right.  The cell drawing code is pretty good about exiting
     * quickly when its cell is off the visible board.  We'll count on that
     * for now.
     */
    for ( row = model_numRows( model )-1; row >= 0; --row ) {
        for ( col = model_numCols( model )-1; col >= 0; --col ) {
            if ( model_getTile( model, col, row, includePending,
                                turn, NULL, NULL, NULL, NULL ) ) {
                XP_U16 boardCol, boardRow;
                flipIf( board, col, row, &boardCol, &boardRow );
                invalCell( board, boardCol, boardRow );
            }
        }
    }
    return board->needsDrawing;
} /* invalCellsWithTiles */

static XP_S16
figureOffset(const BoardCtxt* board, SDIndex indx, XP_U16 col )
{
    XP_S16 offset = 0;
    const ScrollData* sd = &board->sd[indx];
    if ( col < sd->offset ) {
        offset = sd->offset - col;
    } else if ( col > sd->lastVisible ) {
        offset = sd->lastVisible - col;
    }
    return offset;
}

XP_Bool
scrollIntoView( BoardCtxt* board, XWEnv xwe, XP_U16 col, XP_U16 row )
{
    XP_Bool moved;
    XP_S16 newOffset;

    newOffset = figureOffset( board, SCROLL_H, col );
    moved = adjustXOffset( board, newOffset );

    newOffset = figureOffset( board, SCROLL_V, row );
    moved = adjustYOffset( board, xwe, newOffset ) || moved;

    return moved;
} /* scrollIntoView */

XP_Bool
onBorderCanScroll( const BoardCtxt* board, SDIndex indx, 
                   XP_U16 row /*or col */, XP_S16* changeP )
{
    XP_Bool result;
    XP_S16 change = 0;
    const ScrollData* sd = &board->sd[indx];
    XP_U16 offset = sd->offset;

    if ( offset > 0 && row == offset  ) {
        change = -offset;
    } else if ( row == sd->lastVisible ) {
        XP_U16 lastRow = model_numRows(board->model) - 1;
        change = lastRow - row;
    }

    result = change != 0;
    if ( result ) {
        *changeP = change;
    }
    return result;
}

static void
board_setTrayLoc( BoardCtxt* board, XWEnv xwe, XP_U16 trayLeft, XP_U16 trayTop,
                  XP_U16 trayWidth, XP_U16 trayHeight, XP_U16 nTiles )
{
    /* XP_LOGF( "%s(%d,%d,%d,%d)", __func__, trayLeft, trayTop,  */
             /* trayWidth, trayHeight ); */

    XP_U16 dividerWidth = DIVIDER_WIDTH( trayWidth );
    /* XP_U16 boardBottom, boardRight; */
    /* XP_Bool boardHidesTray; */

    board->trayBounds.left = trayLeft;
    board->trayBounds.top = trayTop;
    board->trayBounds.width = trayWidth;
    board->trayBounds.height = trayHeight;

    dividerWidth = dividerWidth + 
        ((trayWidth - dividerWidth) % MAX_TRAY_TILES);

    board->trayScaleH = (trayWidth - dividerWidth) / nTiles;
    board->trayScaleV = trayHeight;

    board->dividerWidth = dividerWidth;

    figureBoardRect( board, xwe );
} /* board_setTrayLoc */

void
invalCellsUnderRect( BoardCtxt* board, const XP_Rect* rect )
{
    if ( rectsIntersect( rect, &board->boardBounds ) ) {
        XP_Rect lr = *rect;
        XP_U16 left, top, right, bottom;
        XP_U16 col, row;

        if ( !coordToCell( board, lr.left, lr.top, &left, &top ) ) {
            left = top = 0;
        }
        if ( !coordToCell( board, lr.left+lr.width, lr.top+lr.height, 
                           &right, &bottom ) ) {
            right = bottom = model_numCols( board->model );
        }

        for ( row = top; row <= bottom; ++row ) {
            for ( col = left; col <= right; ++col ) {
                invalCell( board, col, row );
            }
        }
    }
} /* invalCellsUnderRect */

#ifdef XWFEATURE_CROSSHAIRS
void
invalCol( BoardCtxt* board, XP_U16 col )
{
#ifdef XWFEATURE_CROSSHAIRS
    XP_ASSERT( !board->hideCrosshairs );
#endif
    XP_U16 row;
    XP_U16 nCols = model_numCols(board->model);
    for ( row = 0; row < nCols; ++row ) {
        invalCell( board, col, row );
    }
}

void
invalRow( BoardCtxt* board, XP_U16 row )
{
#ifdef XWFEATURE_CROSSHAIRS
    XP_ASSERT( !board->hideCrosshairs );
#endif
    XP_U16 col;
    XP_U16 nCols = model_numCols(board->model);
    for ( col = 0; col < nCols; ++col ) {
        invalCell( board, col, row );
    }
}
#endif

void
board_invalRect( BoardCtxt* board, XP_Rect* rect )
{
    if ( rectsIntersect( rect, &board->boardBounds ) ) {
        invalCellsUnderRect( board, rect );
    }
    
    if ( rectsIntersect( rect, &board->trayBounds ) ) {
        invalTilesUnderRect( board, rect );
    }

    if ( rectsIntersect( rect, &board->scoreBdBounds ) ) {
        board->scoreBoardInvalid = XP_TRUE;
    }
} /* board_invalRect */

#ifdef XWFEATURE_ACTIVERECT
XP_Bool
board_getActiveRect( const BoardCtxt* board, XP_Rect* rect, 
                     XP_U16* nColsP, XP_U16* nRowsP )
{
    XP_Bool found = XP_FALSE;
    XP_USE( rect );
    XP_U16 minCol = 1000;
    XP_U16 maxCol = 0;
    XP_U16 minRow = 1000;
    XP_U16 maxRow = 0;
    const ModelCtxt* model = board->model;
    XP_U16 nCols = model_numCols( board->model );
    XP_S16 turn = board->selPlayer;
    XP_U16 col, row;

    for ( col = 0; col < nCols; ++col ) {
        for ( row = 0; row < nCols; ++row ) {
            if ( model_getTile( model, col, row, XP_TRUE, turn, NULL, NULL, 
                                NULL, NULL ) ) {
                found = XP_TRUE;
                if ( row < minRow ) {
                    minRow = row;
                }
                if ( row > maxRow ) {
                    maxRow = row;
                }
                if ( col < minCol ) {
                    minCol = col;
                }
                if ( col > maxCol ) {
                    maxCol = col;
                }
            }
        }
    }

    if ( !found ) {
        XP_U16 middle = nCols / 2;
        XP_ASSERT( 0 < middle );
        minCol = minRow = middle - 1;
        maxCol = maxRow = middle + 1;
        found = XP_TRUE;
    }

    XP_Rect upperLeft, lowerRight;
    getCellRect( board, minCol, minRow, &upperLeft );
    getCellRect( board, maxCol, maxRow, &lowerRight );
    rect->left = upperLeft.left;
    rect->top = upperLeft.top;
    rect->width = (lowerRight.left + lowerRight.width) - upperLeft.left;
    rect->height = (lowerRight.top + lowerRight.height) - upperLeft.top;

    *nColsP = maxCol - minCol + 1;
    *nRowsP = maxRow - minRow + 1;

    return found;
}
#endif

/* When the tray's hidden, check if it overlaps where the board wants to be,
 * and adjust the board's rect if needed so that hit-testing will work
 * "through" where the tray used to be.
 *
 * Visible here means different things depending on whether the tray can
 * overlap the board.  If not, then we never "hide" the tray; rather, we turn
 * it over.  So figure out what hidden means and then change the state if it's
 * not already there.
 *
 * But remember that revealing the tray in an overlapping situation is a
 * two-step process.  First anyone can cause the tray to be drawn over the top
 * of the board.  But then a second gesture is required to cause the tray to
 * be drawn with tiles face-up.
 */
XP_Bool
board_hideTray( BoardCtxt* board, XWEnv xwe )
{
    XW_TrayVisState soughtState;
    if ( board->boardObscuresTray ) {
        soughtState = TRAY_HIDDEN;
    } else {
        soughtState = TRAY_REVERSED;
    }
    return setTrayVisState( board, xwe, soughtState );
} /* board_hideTray */

static XP_S16
chooseBestSelPlayer( const BoardCtxt* board )
{
    ServerCtxt* server = board->server;

    if ( board->gameOver ) {
        return board->selPlayer;
    } else {

        XP_S16 curTurn = server_getCurrentTurn( server, NULL );

        if ( curTurn >= 0 ) {
            XP_U16 nPlayers = board->gi->nPlayers;
            XP_U16 ii;

            for ( ii = 0; ii < nPlayers; ++ii ) {
                LocalPlayer* lp = &board->gi->players[curTurn];

                if ( !LP_IS_ROBOT(lp) && lp->isLocal ) {
                    return curTurn;
                }
                curTurn = (curTurn + 1) % nPlayers;
            }
        }
    }

    return -1;
} /* chooseBestSelPlayer */

/* Reveal the tray of the currently selected player.  If that's a robot other
 * code should flag the error.
 */
XP_Bool
board_showTray( BoardCtxt* board, XWEnv xwe )
{
    return checkRevealTray( board, xwe );
} /* board_showTray */

static XP_Bool
trayOnTop( const BoardCtxt* board )
{
    /* The tray should be drawn on top of the board IFF it's not HIDDEN or if
       it has non-dived focus. */
    return (board->trayVisState != TRAY_HIDDEN)
        || ( (board->focussed == OBJ_TRAY)
#ifdef KEYBOARD_NAV
             && (board->focusHasDived == XP_FALSE)
#endif
             );
} /* trayOnTop */

XW_TrayVisState
board_getTrayVisState( const BoardCtxt* board )
{
    return board->trayVisState;
} /* board_getTrayVisible */

static XP_Bool
setTrayVisState( BoardCtxt* board, XWEnv xwe, XW_TrayVisState newState )
{
    XP_Bool changed;

    if ( newState == TRAY_REVERSED && board->gameOver ) {
        newState = TRAY_REVEALED;
    }

    changed = board->trayVisState != newState;
    if ( changed ) {
        XP_Bool nowHidden = newState == TRAY_HIDDEN;
        XP_U16 selPlayer = board->selPlayer;
        XP_U16 nVisible;
        ScrollData* vsd = &board->sd[SCROLL_V];

        /* redraw cells that are pending; whether tile is visible may
           change */
        model_foreachPendingCell( board->model, xwe, selPlayer,
                                  boardCellChanged, board );
        /* ditto -- if there's a pending move */
        model_foreachPrevCell( board->model, xwe, boardCellChanged, board );

        board_invalTrayTiles( board, ALLTILES );
        board->dividerInvalid = XP_TRUE;

        board->trayVisState = newState;

        invalSelTradeWindow( board );
        (void)invalFocusOwner( board, xwe ); /* must be done before and after rect
                                                recalculated */
        figureBoardRect( board, xwe ); /* comes before setYOffset since that
                                          uses rects to calc scroll */
        (void)invalFocusOwner( board, xwe );

        if ( board->boardObscuresTray ) {
            if ( nowHidden && !trayOnTop(board) ) { 
                board->preHideYOffset = board_getYOffset( board );
                board_setYOffset( board, xwe, 0 );
            } else {
                board_setYOffset( board, xwe, board->preHideYOffset );
            }
        }

        if ( nowHidden ) {
            /* Can't always set this to TRUE since in obscuring case tray will
             * get erased after the cells that are supposed to be revealed
             * get drawn. */
            board->eraseTray = !board->boardHidesTray;
            invalCellsUnderRect( board, &board->trayBounds );
        }
        invalArrowCell( board );
        board->scoreBoardInvalid = XP_TRUE; /* b/c what's bold may change */
#ifdef XWFEATURE_SEARCHLIMIT
        invalCurHintRect( board, selPlayer );
#endif

        nVisible = vsd->lastVisible - vsd->offset + 1;
        util_trayHiddenChange( board->util, xwe, board->trayVisState, nVisible );
    }
    return changed;
} /* setTrayVisState */

static void
invalReflection( BoardCtxt* board )
{
    XP_U16 nRows = model_numRows( board->model );
    XP_U16 saveCols = model_numCols( board->model );

    while ( nRows-- ) {
        XP_U16 nCols;
        RowFlags redrawFlag = board->redrawFlags[nRows];
        if ( !redrawFlag ) {
            continue;           /* nothing set this row */
        }
        nCols = saveCols;
        while ( nCols-- ) {
            if ( 0 != (redrawFlag & (1<<nCols)) ) {
                invalCell( board, nRows, nCols );
            }
        }
    }
} /* invalReflection */

XP_Bool
board_get_flipped( const BoardCtxt* board )
{
    return board->isFlipped;
}

XP_U16
board_getLikelyChatter( const BoardCtxt* board )
{
    return gi_getLocalPlayer( board->gi, board->selPlayer );
}

XP_Bool
board_flip( BoardCtxt* board )
{
    invalArrowCell( board );
    flipArrow( board );
#ifdef KEYBOARD_NAV
    invalCursorCell( board );
#endif

    if ( board->boardObscuresTray ) {
        invalCellsUnderRect( board, &board->trayBounds );
    }
    invalCellsWithTiles( board );

#ifdef XWFEATURE_SEARCHLIMIT
    invalCurHintRect( board, board->selPlayer );
    flipAllLimits( board );
#endif

    board->isFlipped = !board->isFlipped;

    invalReflection( board ); /* For every x,y set, also set y,x */
    
    return board->needsDrawing;
} /* board_flip */

XP_Bool
board_inTrade( const BoardCtxt* board, XP_Bool* anySelected )
{
    const PerTurnInfo* pti = &board->pti[board->selPlayer];
    if ( !!anySelected ) {
        *anySelected = 0 != pti->traySelBits;
    }
    return pti->tradeInProgress;
}

XP_Bool
board_replaceNTiles( BoardCtxt* board, XWEnv xwe, XP_U16 nTiles )
{
    XP_Bool result = XP_FALSE;
    while ( 0 < nTiles-- && replaceLastTile( board, xwe ) ) {
        result = XP_TRUE;
    } 

    return result;
}

XP_Bool
board_replaceTiles( BoardCtxt* board, XWEnv xwe )
{
    return board_replaceNTiles( board, xwe, MAX_TRAY_TILES );
} /* board_replaceTiles */

XP_Bool
board_redoReplacedTiles( BoardCtxt* board, XWEnv xwe )
{
    return model_redoPendingTiles( board->model, xwe, board->selPlayer );
}

/* There are a few conditions that must be true for any of several actions
   to be allowed.  Check them here.  */
static XP_Bool
preflight( BoardCtxt* board, XWEnv xwe, XP_Bool reveal )
{
    return !board->gameOver
        && server_getCurrentTurn( board->server, NULL) >= 0
        && ( !reveal || checkRevealTray( board, xwe ) )
        && !TRADE_IN_PROGRESS(board);
} /* preflight */

/* Refuse with error message if any tiles are currently on board in this turn.
 * Then call the engine, and display the first move.  Return true if there's
 * any redrawing to be done.
 */
XP_Bool
board_requestHint( BoardCtxt* board, XWEnv xwe,
#ifdef XWFEATURE_SEARCHLIMIT
                   XP_Bool useTileLimits,
#endif
                   XP_Bool usePrev, XP_Bool* workRemainsP )
{
    XP_Bool result = XP_FALSE;
    XP_Bool redraw = XP_FALSE;

    *workRemainsP = XP_FALSE; /* in case we exit without calling engine */

    if ( board->gi->hintsNotAllowed ) {
        util_userError( board->util, xwe, ERR_CANT_HINT_WHILE_DISABLED );
    } else {
        MoveInfo newMove;
        XP_S16 nTiles;
        const Tile* tiles;
        XP_Bool searchComplete = XP_TRUE;
        const XP_U16 selPlayer = board->selPlayer;
#ifdef XWFEATURE_SEARCHLIMIT
        PerTurnInfo* pti = board->selInfo;
#endif
        EngineCtxt* engine = server_getEngineFor( board->server, selPlayer );
        const TrayTileSet* tileSet;
        ModelCtxt* model = board->model;
        XP_U16 dividerLoc = model_getDividerLoc( model, selPlayer );

        if ( !!engine && preflight( board, xwe, XP_TRUE ) ) {

            /* undo any current move.  otherwise we won't pass the full tray
               to the engine.  Would it be better, though, to pass the whole
               tray regardless where its contents are? */
            if ( model_getCurrentMoveCount( model, selPlayer ) > 0 ) {
                model_resetCurrentTurn( model, xwe, selPlayer );
                /* Draw's a no-op on Wince with a null hdc, but it'll draw again.
                   Should probably define OS_INITS_DRAW on Wince...*/
#ifdef OS_INITS_DRAW
                /* On symbian, it's illegal to draw except from inside the
                   Draw method.  But the move search will probably be so fast
                   that it's ok to wait until we've found the move anyway. */
                redraw = XP_TRUE;
#else
                board_draw( board, xwe );
#endif
            }

            tileSet = model_getPlayerTiles( model, selPlayer );
            nTiles = tileSet->nTiles - dividerLoc;
            result = nTiles > 0;
        }

        XP_Bool canMove = XP_FALSE;
        if ( result ) {
#ifdef XWFEATURE_SEARCHLIMIT
            BdHintLimits limits;
            BdHintLimits* lp = NULL;
#endif
            XP_Bool wasVisible;

            wasVisible = setArrowVisible( board, XP_FALSE );

            (void)board_replaceTiles( board, xwe );

            tiles = tileSet->tiles + dividerLoc;

            board_pushTimerSave( board, xwe );

#ifdef XWFEATURE_SEARCHLIMIT
            XP_ASSERT( board->gi->allowHintRect || !pti->hasHintRect );
            if ( board->gi->allowHintRect && pti->hasHintRect ) {
                limits = pti->limits;
                lp = &limits;
                if ( board->isFlipped ) {
                    flipLimits( lp );
                }
            }
#endif
#ifdef XWFEATURE_BONUSALL
            XP_U16 allTilesBonus = 0;
# ifdef XWFEATURE_BONUSALLHINT
            if ( 0 == dividerLoc ) {
                allTilesBonus = server_figureFinishBonus( board->server, 
                                                          selPlayer );
            }
# endif
#endif
            searchComplete = 
                engine_findMove( engine, xwe, model, selPlayer,
                                 XP_FALSE, XP_FALSE, tiles, nTiles, usePrev,
#ifdef XWFEATURE_BONUSALL
                                 allTilesBonus, 
#endif
#ifdef XWFEATURE_SEARCHLIMIT
                                 lp, useTileLimits,
#endif
                                 0, /* 0: not a robot */
                                 &canMove, &newMove, NULL );
            board_popTimerSave( board, xwe );

            if ( searchComplete && canMove ) {
                // assertTilesInTiles( board, &newMove, tiles, nTiles );
                juggleMoveIfDebug( &newMove );
                model_makeTurnFromMoveInfo( model, xwe, selPlayer, &newMove );
            } else {
                result = XP_FALSE;
                XP_STATUSF( "unable to complete hint request\n" );
            }
            *workRemainsP = !searchComplete;

            /* hide the cursor if it's been obscured by the new move  */
            if ( wasVisible ) {
                XP_U16 col, row;

                getArrow( board, &col, &row );
                if ( cellOccupied( board, col, row, XP_TRUE ) ) {
                    wasVisible = XP_FALSE;
                }
                setArrowVisible( board, wasVisible );
            }
        }

        if ( !canMove ) {
            util_userError( board->util, xwe, ERR_NO_HINT_FOUND );
        }
    }
    return result || redraw;
} /* board_requestHint */

static XP_Bool
figureDims( XP_U16* edges, XP_U16 len, XP_U16 nVisible, 
            XP_U16 increment, XP_U16 extra )
{
    XP_Bool changed = XP_FALSE;
    XP_U16 ii;
    XP_U16 nAtStart = extra % nVisible;

    increment += extra / nVisible;

    for ( ii = 0; ii < len; ++ii ) {
        XP_U16 newVal = increment;
        if ( ii % nVisible < nAtStart ) {
            ++newVal;
        }
        if ( edges[ii] != newVal ) {
            edges[ii] = newVal;
            changed = XP_TRUE;
        }
    }
    return changed;
} /* figureDims */

static XP_U16
figureScale( BoardCtxt* board, XP_U16 count, XP_U16 dimension, ScrollData* sd )
{
    XP_U16 nVis = count - board->zoomCount;
    XP_U16 scale = dimension / nVis;
    XP_U16 spares = dimension % nVis;

    XP_U16 maxOffset = count - nVis;
    if ( sd->offset > maxOffset ) {
        sd->offset = maxOffset;
    }

    sd->lastVisible = count - board->zoomCount + sd->offset - 1;

    if ( figureDims( sd->dims, VSIZE(sd->dims), nVis,
                     scale, spares ) ) {
        board_invalAll( board );
    }
    return scale;
}

static void
figureScales( BoardCtxt* board, XP_U16* scaleHP, XP_U16* scaleVP )
{
    while ( !canZoomIn( board, board->zoomCount ) ) {
        XP_ASSERT( board->zoomCount >= 1 );
        --board->zoomCount;
    }

    /* Horizontal scale */
    *scaleHP = figureScale( board, model_numCols( board->model ),
                            board->boardBounds.width, &board->sd[SCROLL_H] );

#ifdef XWFEATURE_WIDER_COLS
    *scaleVP = figureScale( board, model_numCols( board->model ),
                            board->boardBounds.height,
                            &board->sd[SCROLL_V] );
#else
    *scaleVP = *scaleHP;
#endif
} /* figureScales */

static void
figureBoardRect( BoardCtxt* board, XWEnv xwe )
{
    if ( board->boardBounds.width > 0 && board->trayBounds.width > 0 ) {
        XP_Rect boardBounds = board->boardBounds;
        XP_U16 nVisible;
        XP_U16 nRows = model_numRows( board->model );
        ScrollData* hsd = &board->sd[SCROLL_H];
        ScrollData* vsd = &board->sd[SCROLL_V];
        XP_U16 boardHScale, boardVScale;
        figureScales( board, &boardHScale, &boardVScale );

        if ( boardHScale != hsd->scale ) {
            board_invalAll( board );
            hsd->scale = boardHScale;
        }

        /* Figure height of board.  Max height is with all rows visible and
           each row as tall as boardScale.  But that may make it overlap tray,
           if it's visible, or the bottom of the board as set in board_setPos.
           So we check those two possibilities. */

        XP_U16 maxHeight, wantHeight = nRows * boardVScale;
        XP_Bool trayHidden = board->trayVisState == TRAY_HIDDEN;
        if ( trayHidden ) {
            maxHeight = board->heightAsSet;
        } else {
            maxHeight = board->trayBounds.top - board->boardBounds.top;
        }
        XP_U16 extra;
        XP_U16 oldYOffset = vsd->offset;
        if ( wantHeight <= maxHeight ) { /* yay!  No need to scale */
            vsd->scale = maxHeight / nRows;
            extra = maxHeight % nRows;
            boardBounds.height = maxHeight;
            board->boardObscuresTray = XP_FALSE;
            vsd->offset = 0;
            nVisible = nRows;
        } else {
            XP_S16 maxYOffset;
            /* Need to hide rows etc. */
            boardBounds.height = maxHeight;
            vsd->scale = boardVScale;

            nVisible = maxHeight / boardVScale;
            extra = maxHeight % boardVScale;

            maxYOffset = nRows - nVisible;
            if ( vsd->offset > maxYOffset ) {
                vsd->offset = maxYOffset;
            } else if ( maxYOffset != vsd->maxOffset ) {
                vsd->maxOffset = maxYOffset;
            }

            board->boardObscuresTray = board->trayBounds.top < wantHeight
                && board->trayBounds.left < (boardBounds.left + boardBounds.width);
        }
        util_yOffsetChange( board->util, xwe, nRows - nVisible, oldYOffset,
                            vsd->offset );

        vsd->lastVisible = nVisible + vsd->offset - 1;

        if ( figureDims( vsd->dims, VSIZE(vsd->dims), nVisible, 
                         vsd->scale, extra ) ) {
            board_invalAll( board );
        }

        board->boardBounds = boardBounds;
    }
} /* figureBoardRect */

/* Previously this function returned the *model* column,row under coordinates.
 * That is, it adjusted for scrolling by pretending yy was a larger value when
 * scrolling had occurred.  Calculations by callers are all done based on the
 * model, and then at draw time we convert back to coordinates that take
 * scrolling into account.
 *
 * The extra-pixel distribution then needs to slide depending on what row is
 * topmost.  The top visible row always gets n visible pixels -- always starts
 * at a given coordinate.  But what model cell maps to it changes.  This
 * implies that the decision is made based on coordinates and that offsets are
 * added in at the end.
 */
XP_Bool
coordToCell( const BoardCtxt* board, XP_S16 xx, XP_S16 yy, XP_U16* colP, 
             XP_U16* rowP )
{
    const XP_U16 maxCols = model_numCols( board->model );
    XP_S16 gotCol = -1;
    XP_S16 gotRow = -1;

    xx -= board->boardBounds.left;
    yy -= board->boardBounds.top;

    if ( xx >= 0 && yy >= 0 ) {
        const ScrollData* hsd = &board->sd[SCROLL_H];
        for ( XP_U16 col = hsd->offset; col < maxCols; ++col ) {
            xx -= hsd->dims[col];
            if ( xx <= 0 ) {
                gotCol = col;
                break;
            }
        }

        const ScrollData* vsd = &board->sd[SCROLL_V];
        for ( XP_U16 row = vsd->offset; row < maxCols; ++row ) {
            yy -= vsd->dims[row];
            if ( yy <= 0 ) {
                gotRow = row;
                break;
            }
        }
    }

    /* XP_LOGF( "%s=>%d,%d", __func__, gotCol, gotRow ); */

    *colP = gotCol;
    *rowP = gotRow;
    return gotRow != -1 && gotCol != -1;
} /* coordToCell */

/* Like coordToCell, getCellRect takes model values and returns screen
 * coords. 
 *
 * But the dimensions of the cells need to stick with the column,row values to
 * allow bitblit-based scrolling.  That is, column 1 has the same width
 * whether drawn in position 0 or 1.
 *
 * This suggests that we don't want board dimensions built into the edge
 * arrays.
 */
XP_Bool
getCellRect( const BoardCtxt* board, XP_U16 col, XP_U16 row, XP_Rect* rect )
{
    XP_U16 cur;
    const ScrollData* hsd = &board->sd[SCROLL_H];
    const ScrollData* vsd = &board->sd[SCROLL_V];
    XP_Bool onBoard = col >= hsd->offset && row >= vsd->offset
        && col <= hsd->lastVisible && row <= vsd->lastVisible;

    XP_U16 left = board->boardBounds.left;
    for ( cur = hsd->offset; cur < col; ++cur ) {
        left += hsd->dims[cur];
    }
    rect->left = left;
    rect->width = hsd->dims[col];

    XP_U16 top = board->boardBounds.top;
    for ( cur = vsd->offset; cur < row; ++cur ) {
        top += vsd->dims[cur];
    }
    rect->top = top;
    rect->height = vsd->dims[row];

    return onBoard;
} /* getCellRect */

void
invalCell( BoardCtxt* board, XP_U16 col, XP_U16 row )
{
    board->redrawFlags[row] |= 1 << col;

    XP_ASSERT( col < MAX_ROWS );
    XP_ASSERT( row < MAX_ROWS );

#ifdef XWFEATURE_MINIWIN
    /* if the trade window is up and this cell intersects it, set up to draw
       it again */
    if ( (board->trayVisState != TRAY_HIDDEN) && TRADE_IN_PROGRESS(board) ) {
        XP_Rect rect;
        if ( getCellRect( board, col, row, &rect ) ) { /* cell's visible */
            XP_Rect windowR = board->miniWindowStuff[MINIWINDOW_TRADING].rect;
            if ( rectsIntersect( &rect, &windowR ) ) {
                board->tradingMiniWindowInvalid = XP_TRUE;
            }
        }
    }
#endif
    
    board->needsDrawing = XP_TRUE;
} /* invalCell */

#if defined POINTER_SUPPORT || defined KEYBOARD_NAV
XP_Bool
pointOnSomething( const BoardCtxt* board, XP_U16 xx, XP_U16 yy,
                  BoardObjectType* wp )
{
    XP_Bool result = XP_TRUE;

    /* Test the board first in case it overlaps.  When tray is visible
       boardBounds is shortened so it does not overlap. */
    if ( rectContainsPt( &board->boardBounds, xx, yy ) ) {
        *wp = OBJ_BOARD;
    } else if ( rectContainsPt( &board->trayBounds, xx, yy ) ) {
        *wp = OBJ_TRAY;
    } else if ( rectContainsPt( &board->scoreBdBounds, xx, yy ) ) {
        *wp = OBJ_SCORE;
    } else if ( board->gi->timerEnabled
                && rectContainsPt( &board->timerBounds, xx, yy ) ) {
        *wp = OBJ_TIMER;
    } else {
        *wp = OBJ_NONE;
        result = XP_FALSE;
    }

    return result;
} /* pointOnSomething */

/* Move the given tile to the board.  If it's a blank, we need to ask the user
 * what to call it first.
 */
XP_Bool
moveTileToArrowLoc( BoardCtxt* board, XWEnv xwe, XP_U8 index )
{
    XP_Bool result;
    BoardArrow* arrow = &board->selInfo->boardArrow;
    if ( arrow->visible ) {
        result = moveTileToBoard( board, xwe, arrow->col, arrow->row,
                                  (XP_U16)index, EMPTY_TILE );
        if ( result ) {
            XP_Bool moved = advanceArrow( board, xwe );
            if ( !moved ) {
                /* If the arrow didn't move, we can't leave it in place or
                   it'll get drawn over the new tile. */
                setArrowVisible( board, XP_FALSE );
            }
        }
    } else {
        result = XP_FALSE;
    }
    return result;
} /* moveTileToArrowLoc */
#endif

#ifdef XWFEATURE_MINIWIN
void
makeMiniWindowForText( BoardCtxt* board, const XP_UCHAR* text, 
                       MiniWindowType winType )
{
    XP_Rect rect;
    MiniWindowStuff* stuff = &board->miniWindowStuff[winType];

    draw_measureMiniWText( board->draw, text, 
                           (XP_U16*)&rect.width,
                           (XP_U16*)&rect.height );
    positionMiniWRect( board, &rect, winType == MINIWINDOW_TRADING );
    stuff->rect = rect;
    stuff->text = text;
} /* makeMiniWindowForText */
#endif

XP_Bool
board_beginTrade( BoardCtxt* board, XWEnv xwe )
{
    XP_Bool result;

    result = preflight( board, xwe, XP_TRUE );
    if ( result ) {
        XP_S16 tilesLeft = server_countTilesInPool(board->server);
        if ( tilesLeft < MIN_TRADE_TILES( board->gi ) ) {
            util_userError( board->util, xwe, ERR_TOO_FEW_TILES_LEFT_TO_TRADE );
        } else {
            model_resetCurrentTurn( board->model, xwe, board->selPlayer );
            XP_ASSERT( 0 == model_getCurrentMoveCount( board->model, 
                                                       board->selPlayer ) );
#ifdef XWFEATURE_MINIWIN
            board->tradingMiniWindowInvalid = XP_TRUE;
#endif
            board->needsDrawing = XP_TRUE;
            board->selInfo->tradeInProgress = XP_TRUE;
            setArrowVisible( board, XP_FALSE );
            result = XP_TRUE;
        }
    }
    return result;
} /* board_beginTrade */

XP_Bool
board_endTrade( BoardCtxt* board )
{
    XP_Bool result = board_inTrade( board, NULL );
    if ( result ) {
        PerTurnInfo* pti = board->selInfo;
        invalSelTradeWindow( board );
        pti->tradeInProgress = XP_FALSE;
        pti->traySelBits = NO_TILES;
        board_invalTrayTiles( board, ALLTILES );
    }
    return result;
}


#if defined POINTER_SUPPORT || defined KEYBOARD_NAV
# ifdef XWFEATURE_MINIWIN
static XP_Bool
ptOnTradeWindow( BoardCtxt* board, XP_U16 x, XP_U16 y )
{
    XP_Rect* windowR = &board->miniWindowStuff[MINIWINDOW_TRADING].rect;
    return rectContainsPt( windowR, x, y );
} /* ptOnTradeWindow */
# else
#  define ptOnTradeWindow(b,x,y) XP_FALSE
# endif

#ifdef XWFEATURE_SEARCHLIMIT

void
invalCellRegion( BoardCtxt* board, XP_U16 colA, XP_U16 rowA, XP_U16 colB, 
                 XP_U16 rowB )
{
        XP_U16 col, row;
        XP_U16 firstCol, lastCol, firstRow, lastRow;

        if ( colA <= colB ) {
            firstCol = colA;
            lastCol = colB;
        } else {
            firstCol = colB;
            lastCol = colA;
        }
        if ( rowA <= rowB ) {
            firstRow = rowA;
            lastRow = rowB;
        } else {
            firstRow = rowB;
            lastRow = rowA;
        }

        for ( row = firstRow; row <= lastRow; ++row ) {
            for ( col = firstCol; col <= lastCol; ) {
                invalCell( board, col, row );
                ++col;
#ifndef XWFEATURE_SEARCHLIMIT_DOCENTERS
                if ( row > firstRow && row < lastRow && (col < lastCol) ) {
                    col = lastCol;
                }
#endif
            }
        }
} /* invalCellRegion */

void
invalCurHintRect( BoardCtxt* board, XP_U16 player )
{
    BdHintLimits* limits = &board->pti[player].limits;    
    invalCellRegion( board, limits->left, limits->top, 
                     limits->right, limits->bottom );
} /* invalCurHintRect */

static void
clearCurHintRect( BoardCtxt* board )
{
    invalCurHintRect( board, board->selPlayer );
    board->selInfo->hasHintRect = XP_FALSE;
} /* clearCurHintRect */
#endif /* XWFEATURE_SEARCHLIMIT */

static XP_Bool
handlePenDownOnBoard( BoardCtxt* board, XWEnv xwe, XP_U16 xx, XP_U16 yy )
{
    XP_Bool result = XP_FALSE;

    if ( TRADE_IN_PROGRESS(board) && ptOnTradeWindow( board, xx, yy ) ) {
        /* do nothing */
    } else {
        util_setTimer( board->util, xwe, TIMER_PENDOWN, 0,
                       p_board_timerFired, board );

        if ( !board->selInfo->tradeInProgress ) {
            result = dragDropStart( board, xwe, OBJ_BOARD, xx, yy );
        }
    }

    return result;
} /* handlePenDownOnBoard */

/* If there's a password, ask it; if they match, change the state of the tray
 * to TRAY_REVEALED (unless we're not supposed to show the tiles).  Return
 * value talks about whether the tray needs to be redrawn, not the success of
 * the password compare.
 */
static XP_Bool
askRevealTray( BoardCtxt* board, XWEnv xwe )
{
    XP_Bool revealed = XP_FALSE;
    XP_Bool reversed = board->trayVisState == TRAY_REVERSED;
    XP_U16 selPlayer = board->selPlayer;
    LocalPlayer* lp = &board->gi->players[selPlayer];
    XP_Bool justReverse = XP_FALSE;

    if ( board->gameOver ) {
        revealed = XP_TRUE;
    } else if ( server_getCurrentTurn( board->server, NULL ) < 0 ) {
        revealed = XP_FALSE;
    } else if ( !lp->isLocal ) {
        util_userError( board->util, xwe, ERR_NO_PEEK_REMOTE_TILES );
    } else if ( LP_IS_ROBOT(lp) ) {
        if ( reversed ) {
            util_userError( board->util, xwe, ERR_NO_PEEK_ROBOT_TILES );
        } else {
            justReverse = XP_TRUE;
        }
    } else {
        revealed = !player_hasPasswd( lp );

        if ( !revealed ) {
            util_informNeedPassword( board->util, xwe, selPlayer, lp->name );
        }
    }

    if ( revealed ) {
        setTrayVisState( board, xwe, TRAY_REVEALED );
    } else if ( justReverse ) {
        setTrayVisState( board, xwe, TRAY_REVERSED );
    }
    return justReverse || revealed;
} /* askRevealTray */

XP_Bool
board_passwordProvided( BoardCtxt* board, XWEnv xwe, XP_U16 player,
                        const XP_UCHAR* passwd )
{
    LocalPlayer* lp = &board->gi->players[player];
    XP_Bool draw = player_passwordMatches( lp, passwd );
    if ( draw ) {
        setTrayVisState( board, xwe, TRAY_REVEALED );
    } else {
        util_informNeedPassword( board->util, xwe, player, lp->name );
    }
    return draw;
}

XP_Bool
checkRevealTray( BoardCtxt* board, XWEnv xwe )
{
    XP_Bool result = board->trayVisState == TRAY_REVEALED;
    if ( !result ) {
        result = askRevealTray( board, xwe );
    }
    return result;
} /* checkRevealTray */

static XP_Bool
handleLikeDown( BoardCtxt* board, XWEnv xwe, BoardObjectType onWhich,
                XP_U16 xx, XP_U16 yy )
{
    XP_Bool result = XP_FALSE;

    switch ( onWhich ) {
    case OBJ_BOARD:
        result = handlePenDownOnBoard( board, xwe, xx, yy ) || result;
        break;

    case OBJ_TRAY:
      if ( (board->trayVisState == TRAY_REVEALED)
           && !board->selInfo->tradeInProgress ) {
#ifdef XWFEATURE_RAISETILE
          util_setTimer( board->util, xwe, TIMER_PENDOWN, 0,
                         p_tray_timerFired, board );
#endif
          result = dragDropStart( board, xwe, OBJ_TRAY, xx, yy ) || result;
        }
        break;

    case OBJ_SCORE:
        if ( figureScoreRectTapped( board, xx, yy ) > CURSOR_LOC_REM ) {
            util_setTimer( board->util, xwe, TIMER_PENDOWN, 0,
                           p_board_timerFired, board );
        }
        break;
    default:
        break;
    }

    board->penDownX = xx;
    board->penDownY = yy;
    board->penDownObject = onWhich;

    return result;
} /* handleLikeDown */

#ifdef POINTER_SUPPORT
XP_Bool
board_handlePenDown( BoardCtxt* board, XWEnv xwe, XP_U16 xx,
                     XP_U16 yy, XP_Bool* handled )
{
    XP_Bool result = XP_FALSE;
    XP_Bool penDidSomething;
    BoardObjectType onWhich;

    board->srcIsPen = XP_TRUE; 

    penDidSomething = pointOnSomething( board, xx, yy, &onWhich );

    if ( !penDidSomething ) {
        board->penDownObject = OBJ_NONE;
    } else {

#ifdef KEYBOARD_NAV
        /* clear focus as soon as pen touches board */
        result = invalFocusOwner( board, xwe );
        board->hideFocus = XP_TRUE;
        if ( board->boardObscuresTray ) {
            figureBoardRect( board, xwe );
        }
#endif

        result = handleLikeDown( board, xwe, onWhich, xx, yy ) || result;
    }
    *handled = penDidSomething;

    return result;
} /* board_handlePenDown */
#endif

XP_Bool
board_handlePenMove( BoardCtxt* board, XWEnv xwe, XP_U16 xx, XP_U16 yy )
{
    XP_Bool result = dragDropInProgress(board)
        && dragDropContinue( board, xwe, xx, yy );
    return result;
} /* board_handlePenMove */

#ifndef DISABLE_TILE_SEL
/* Called when user taps on the board and a tray tile's selected.
 */
static XP_Bool
moveSelTileToBoardXY( BoardCtxt* board, XP_U16 col, XP_U16 row )
{
    XP_Bool result;
    XP_U16 selPlayer = board->selPlayer;
    TileBit bits = board->selInfo->traySelBits;
    XP_U16 tileIndex;

    if ( bits == NO_TILES ) {
        return XP_FALSE;
    }

    tileIndex = indexForBits( bits );

    result = tileIndex < model_getNumTilesInTray( board->model, selPlayer )
        && moveTileToBoard( board, col, row, tileIndex, EMPTY_TILE );

    if ( result ) {
        XP_U16 leftInTray;

        leftInTray = model_getNumTilesInTray( board->model, selPlayer );
        if ( leftInTray == 0 ) {
            bits = NO_TILES;
        } else if ( leftInTray <= tileIndex ) {
            bits = 1 << (tileIndex-1);
            board_invalTrayTiles( board, bits );
        }
        board->selInfo->traySelBits = bits;
    }

    return result;
} /* moveSelTileToBoardXY */
#endif

XP_Bool
cellOccupied( const BoardCtxt* board, XP_U16 col, XP_U16 row, 
              XP_Bool inclPending )
{
    Tile tile;
    XP_Bool result;

    flipIf( board, col, row, &col, &row );
    result = model_getTile( board->model, col, row, inclPending,
                            board->selPlayer, &tile, 
                            NULL, NULL, NULL );
    return result;
} /* cellOccupied */

/* If I tap on the arrow, transform it.  If I tap in an empty square where
 * it's not, move it there, making it visible if it's not already.
 */
static XP_Bool
tryMoveArrow( BoardCtxt* board, XP_U16 col, XP_U16 row )
{
    XP_Bool result = XP_FALSE;

    if ( !cellOccupied( board, col, row, 
                        board->trayVisState == TRAY_REVEALED ) ) {

        BoardArrow* arrow = &board->selInfo->boardArrow;

        if ( arrow->visible && arrow->col == col && arrow->row == row ) {
            /* change it; if vertical, hide; else if horizontal make
               vertical */
            if ( arrow->vert ) {
                arrow->visible = XP_FALSE;
            } else {
                arrow->vert = XP_TRUE;
            }
        } else {
            /* move it/reveal it */
            if ( !arrow->visible && !board->disableArrow ) {
                arrow->visible = XP_TRUE;
                arrow->vert = XP_FALSE;
            } else {
                invalArrowCell( board );
            }
            arrow->col = (XP_U8)col;
            arrow->row = (XP_U8)row;
        }
        invalCell( board, col, row );
        result = XP_TRUE;
    }
    return result;
} /* tryMoveArrow */

static XP_Bool
tryChangeBlank( const BoardCtxt* board, XWEnv xwe, XP_U16 col, XP_U16 row )
{
    XP_Bool handled = XP_FALSE;
    XP_Bool isBlank, isPending;
    model_getTile( board->model, col, row, XP_TRUE, board->selPlayer, NULL,
                   &isBlank, &isPending, NULL );
    handled = isBlank && isPending;
    if ( handled ) {
        (void)model_askBlankTile( board->model, xwe, board->selPlayer,
                                  col, row );
    }

    return handled;
}

XP_Bool
holdsPendingTile( BoardCtxt* board, XP_U16 pencol, XP_U16 penrow )
{
    Tile tile;
    XP_Bool isPending;
    XP_U16 modcol, modrow;
    flipIf( board, pencol, penrow, &modcol, &modrow );

    return model_getTile( board->model, modcol, modrow, XP_TRUE,
                          board->selPlayer, &tile, NULL, &isPending, 
                          NULL )
        && isPending;
} /* holdsPendingTile */

/* Did I tap on a tile on the board that I have not yet committed?  If so,
 * return it to the tray.  But don't do this in drag-and-drop case since it's
 * too easy to accidentally tap and there are better ways.
 */
static XP_Bool
tryReplaceTile( BoardCtxt* board, XWEnv xwe, XP_U16 pencol, XP_U16 penrow )
{
    XP_Bool result = XP_FALSE;

    if ( holdsPendingTile( board, pencol, penrow ) ) {
        XP_U16 modcol, modrow;
        flipIf( board, pencol, penrow, &modcol, &modrow );

        model_moveBoardToTray( board->model, xwe, board->selPlayer,
                               modcol, modrow, -1 );
        setArrow( board, xwe, pencol, penrow, NULL );
        result = XP_TRUE;

    }
    return result;
} /* tryReplaceTile */

static XP_Bool
handleActionInCell( BoardCtxt* board, XWEnv xwe, XP_U16 col, XP_U16 row, XP_Bool isPen )
{
    return XP_FALSE
#ifndef DISABLE_TILE_SEL
        || moveSelTileToBoardXY( board, col, row )
#endif
        || tryMoveArrow( board, col, row )
        || tryChangeBlank( board, xwe, col, row )
        || (!isPen && tryReplaceTile( board, xwe, col, row ))
        ;
} /* handleActionInCell */
#endif /* POINTER_SUPPORT || KEYBOARD_NAV */

static XP_Bool
exitTradeMode( BoardCtxt* board )
{
    PerTurnInfo* pti = board->selInfo;
    invalSelTradeWindow( board );
    pti->tradeInProgress = XP_FALSE;
    board_invalTrayTiles( board, pti->traySelBits );
    pti->traySelBits = 0x00;
    return XP_TRUE;
} /* exitTradeMode */

#if defined POINTER_SUPPORT || defined KEYBOARD_NAV

static XP_Bool
penMoved( const BoardCtxt* board, XP_U16 curCol, XP_U16 curRow )
{
    XP_Bool moved = XP_FALSE;
    XP_U16 startCol, startRow;
    
    if ( coordToCell( board, board->penDownX, board->penDownY,
                      &startCol, &startRow ) ) {
        moved = curCol != startCol || curRow != startRow;
    }
    return moved;
}

static XP_Bool
handlePenUpInternal( BoardCtxt* board, XWEnv xwe, XP_U16 xx, XP_U16 yy,
                     XP_Bool isPen, XP_Bool altDown )
{
    XP_Bool draw = XP_FALSE;
    BoardObjectType prevObj = board->penDownObject;

    /* prevent timer from firing after pen lifted.  Set now rather than later
       in case we put up a modal alert/dialog that must be dismissed before
       exiting this function (which might give timer time to fire. */
    board->penDownObject = OBJ_NONE;

    XP_Bool dragged = XP_FALSE;
    if ( dragDropInProgress(board) ) {
        draw = dragDropEnd( board, xwe, xx, yy, &dragged );
    }
    if ( dragged ) {
        /* do nothing further */
    } else if ( board->penTimerFired ) {
#ifdef XWFEATURE_MINIWIN
        if ( valHintMiniWindowActive( board ) ) {
            hideMiniWindow( board, XP_TRUE, MINIWINDOW_VALHINT );
        }
#endif
        draw = XP_TRUE;         /* might have cancelled a drag */
        /* Need to clean up if there's been any dragging happening */
        board->penTimerFired = XP_FALSE;
    } else {
        BoardObjectType onWhich;
        if ( pointOnSomething( board, xx, yy, &onWhich ) ) {

            switch( onWhich ) {
            case OBJ_SCORE:
                if ( prevObj == OBJ_SCORE ) {
                    draw = handlePenUpScore( board, xwe, xx,
                                             yy, altDown ) || draw;
                }
                break;
            case OBJ_BOARD:
                if ( prevObj == OBJ_BOARD && checkRevealTray( board, xwe ) ) {

                    if ( TRADE_IN_PROGRESS(board) ) {
                        if ( ptOnTradeWindow( board, xx, yy )) {
                            draw = exitTradeMode( board ) || draw;
                        }
                    } else {
                        XP_U16 col, row;
                        coordToCell( board, xx, yy, &col, &row );
                        if ( !penMoved( board, col, row ) ) {
                            draw = handleActionInCell( board, xwe, col, row,
                                                       isPen ) || draw;
                        }
                    }
                }
                break;
            case OBJ_TRAY:
                if ( board->trayVisState != TRAY_REVEALED ) {
                    draw = askRevealTray( board, xwe ) || draw;
                } else {
                    draw = handlePenUpTray( board, xwe, xx, yy ) || draw;
                }
                break;
            case OBJ_TIMER:
                util_timerSelected( board->util, xwe, board->gi->inDuplicateMode,
                                    server_canPause( board->server ) );
                break;
            default:
                XP_ASSERT( XP_FALSE );
            }
        }
    }

    return draw;
} /* handlePenUpInternal */

XP_Bool
board_handlePenUp( BoardCtxt* board, XWEnv xwe, XP_U16 xx, XP_U16 yy )
{
    return handlePenUpInternal( board, xwe, xx, yy, XP_TRUE, XP_FALSE );
}

XP_Bool
board_containsPt( const BoardCtxt* board, XP_U16 xx, XP_U16 yy )
{
    BoardObjectType wp;
    return pointOnSomething( board, xx, yy, &wp );
}

#endif /* #ifdef POINTER_SUPPORT */

#ifdef KEYBOARD_NAV
void
getRectCenter( const XP_Rect* rect, XP_U16* xp, XP_U16* yp )
{
    *xp = rect->left + ( rect->width >> 1 );
    *yp = rect->top + ( rect->height >> 1 );
}

static void
getFocussedCellCenter( BoardCtxt* board, XP_U16* xp, XP_U16* yp )
{
    XP_Rect rect;
    BdCursorLoc* cursorLoc = &board->selInfo->bdCursor;

    getCellRect( board, cursorLoc->col, cursorLoc->row, &rect );
    getRectCenter( &rect, xp, yp );
}

static void
getFocussedScoreCenter( BoardCtxt* board, XP_U16* xp, XP_U16* yp )
{
    XP_Rect* rectPtr;
    XP_S16 scoreCursorLoc = board->scoreCursorLoc;
    if ( CURSOR_LOC_REM == scoreCursorLoc ) {
        rectPtr = &board->remRect;
    } else {
        rectPtr = &board->pti[scoreCursorLoc-1].scoreRects;
    }
    getRectCenter( rectPtr, xp, yp );
}

static XP_Bool
focusToCoords( BoardCtxt* board, XP_U16* xp, XP_U16* yp )
{
    XP_Bool result = board->focusHasDived;
    if ( result ) {
        switch( board->focussed ) {
        case OBJ_NONE:
        case OBJ_TIMER:
            result = XP_FALSE;
            break;
        case OBJ_BOARD:
            getFocussedCellCenter( board, xp, yp );
            break;
        case OBJ_TRAY:
            getFocussedTileCenter( board, xp, yp );
            break;
        case OBJ_SCORE:
            getFocussedScoreCenter( board, xp, yp );
            break;
        }
    }

    return result;
} /* focusToCoords */

/* The focus keys are special because they can cause focus to leave one object
 * and move to another (which the platform needs to be involved with).  On
 * palm, that only works if the keyDown handler claims not to have handled the
 * event.  So we must preflight, determining if the keyUp handler will handle
 * the event should it get to it.  If it wouldn't, then the platform wants a
 * chance not to generate a keyUp event at all.
 */
static XP_Bool
handleFocusKeyUp( BoardCtxt* board, XWEnv xwe, XP_Key key, XP_Bool preflightOnly,
                  XP_Bool* pHandled )
{
    XP_Bool redraw = XP_FALSE;
    if ( board->focusHasDived ) {
        XP_Bool up = XP_FALSE;
        if ( board->focussed == OBJ_BOARD ) {
            redraw = board_moveCursor( board, xwe, key, preflightOnly, &up );
        } else if ( board->focussed == OBJ_SCORE ) {
            redraw = moveScoreCursor( board, key, preflightOnly, &up );
        } else if ( board->focussed == OBJ_TRAY/* && checkRevealTray(board)*/ ) {
            redraw = tray_moveCursor( board, key, preflightOnly, &up );
        }
        if ( up ) {
            if ( !preflightOnly ) {
                (void)invalFocusOwner( board, xwe );
                board->focusHasDived = XP_FALSE;
                (void)invalFocusOwner( board, xwe );
            }
        } else {
            *pHandled = redraw;
        }
    } else {
        /* Do nothing.  We don't handle transition among top-level
           focussed objects.  Platform must.  */
    }
    return redraw;
} /* handleFocusKeyUp */

XP_Bool
board_handleKeyRepeat( BoardCtxt* board, XWEnv xwe, XP_Key key, XP_Bool* handled )
{
    XP_Bool draw;

    if ( key == XP_RETURN_KEY ) {
        *handled = XP_FALSE;
        draw = XP_FALSE;
    } else {
        XP_Bool upHandled, downHandled;
        draw = board_handleKeyUp( board, xwe, key, &upHandled );
        draw = board_handleKeyDown( board, xwe, key, &downHandled ) || draw;
        *handled = upHandled || downHandled;
    }
    return draw;
}

static XP_Bool
unhideFocus( BoardCtxt* board, XWEnv xwe )
{
    XP_Bool changing = board->hideFocus;
    if ( changing ) {
        board->hideFocus = XP_FALSE;
        (void)invalFocusOwner( board, xwe );
    }
    return changing;
}
#endif /* KEYBOARD_NAV */

#ifdef KEY_SUPPORT
XP_Bool
board_handleKeyDown( BoardCtxt* XP_UNUSED_KEYBOARD_NAV(board),
                     XWEnv xwe, XP_Key XP_UNUSED_KEYBOARD_NAV(key),
                     XP_Bool* XP_UNUSED_KEYBOARD_NAV(pHandled) )
{
    XP_Bool draw = XP_FALSE;
#ifdef KEYBOARD_NAV
    XP_U16 xx, yy;

    board->srcIsPen = XP_FALSE;

    *pHandled = XP_FALSE;

    if ( key == XP_RETURN_KEY || key == XP_ALTRETURN_KEY ) {
        if ( focusToCoords( board, &xx, &yy ) ) {
            draw = handleLikeDown( board, xwe, board->focussed, xx, yy );
            *pHandled = draw;
        }
    } else if ( board->focussed != OBJ_NONE ) {
        if ( board->focusHasDived && (key == XP_RAISEFOCUS_KEY) ) {
            *pHandled = XP_TRUE;
        } else {
            draw = handleFocusKeyUp( board, xwe, key, XP_TRUE, pHandled ) || draw;
        }
    }
#endif
    return draw;
} /* board_handleKeyDown */

XP_Bool
board_handleKeyUp( BoardCtxt* board, XWEnv xwe, XP_Key key, XP_Bool* pHandled )
{
    XP_Bool redraw = XP_FALSE;
    XP_Bool handled = XP_FALSE;
    XP_Bool trayVisible = board->trayVisState == TRAY_REVEALED;

    switch( key ) {
#ifdef KEYBOARD_NAV
    case XP_CURSOR_KEY_DOWN:
    case XP_CURSOR_KEY_ALTDOWN:
    case XP_CURSOR_KEY_UP:
    case XP_CURSOR_KEY_ALTUP:
    case XP_CURSOR_KEY_LEFT:
    case XP_CURSOR_KEY_ALTLEFT:
    case XP_CURSOR_KEY_RIGHT:
    case XP_CURSOR_KEY_ALTRIGHT:
        /* If focus is hidden, all we do is show it */
        if ( unhideFocus( board, xwe ) ) {
            redraw = handled = XP_TRUE;
        } else {
            redraw = handleFocusKeyUp( board, xwe, key, XP_FALSE, &handled );
        }
        break;
#endif

    case XP_CURSOR_KEY_DEL:
        if ( trayVisible ) {
            handled = redraw = replaceLastTile( board, xwe );
        }
        break;

#ifdef KEYBOARD_NAV
    case XP_RAISEFOCUS_KEY:
        if ( unhideFocus( board, xwe ) ) {
            /* do nothing */
        } else if ( board->focussed != OBJ_NONE && board->focusHasDived ) {
            (void)invalFocusOwner( board, xwe );
            board->focusHasDived = XP_FALSE;
            (void)invalFocusOwner( board, xwe );
        } else {
            break;              /* skip setting handled */
        }
        handled = redraw = XP_TRUE;
        break;

    case XP_RETURN_KEY:
    case XP_ALTRETURN_KEY: {
        XP_Bool altDown = XP_ALTRETURN_KEY == key;
        if ( unhideFocus( board, xwe ) ) {
            handled = XP_TRUE;
        } else if ( board->focussed != OBJ_NONE ) {
            if ( board->focusHasDived ) {
                XP_U16 xx, yy;
                if ( focusToCoords( board, &xx, &yy ) ) {
                    redraw = handlePenUpInternal( board, xwe, xx, yy, XP_FALSE, altDown );
                    handled = XP_TRUE;
                }
            } else {
                (void)invalFocusOwner( board, xwe );
                board->focusHasDived = XP_TRUE;
                redraw = invalFocusOwner( board, xwe );
                handled = XP_TRUE;
            }
        }
    }
        break;
#endif

    default:
        XP_ASSERT( key >= XP_KEY_LAST );

        if ( trayVisible ) {
            if ( TRADE_IN_PROGRESS( board ) ) {
                XP_S16 tileIndex = keyToIndex( board, key, NULL );
                handled = (tileIndex >= 0)
                    && handleTrayDuringTrade( board, tileIndex );
            } else {
                XP_Bool gotArrow;
                handled = moveKeyTileToBoard( board, xwe, key, &gotArrow );
                if ( handled && gotArrow && !advanceArrow( board, xwe ) ) {
                    setArrowVisible( board, XP_FALSE );
                }
            }
        }
        redraw = handled;
    } /* switch */

    if ( !!pHandled ) {
        *pHandled = handled;
    }
    return redraw;
} /* board_handleKeyUp */

XP_Bool
board_handleKey( BoardCtxt* board, XWEnv xwe, XP_Key key, XP_Bool* handled )
{
    XP_Bool handled1;
    XP_Bool handled2;
    XP_Bool draw;

    draw = board_handleKeyDown( board, xwe, key, &handled1 );
    draw = board_handleKeyUp( board, xwe, key, &handled2 ) || draw;
    if ( !!handled ) {
        *handled = handled1 || handled2;
    }

    return draw;
} /* board_handleKey */
#endif /* KEY_SUPPORT */

#ifdef KEYBOARD_NAV
static XP_Bool
invalFocusOwner( BoardCtxt* board, XWEnv xwe )
{
    XP_Bool draw = XP_TRUE;
    PerTurnInfo* pti = board->selInfo;
    switch( board->focussed ) {
    case OBJ_SCORE:
        board->scoreBoardInvalid = XP_TRUE;
        break;
    case OBJ_BOARD:
        if ( board->focusHasDived ) {
            BdCursorLoc loc = pti->bdCursor;
            invalCell( board, loc.col, loc.row );
            scrollIntoView( board, xwe, loc.col, loc.row );
        } else {
#ifdef PERIMETER_FOCUS
            invalPerimeter( board );
#else
            board_invalAllTiles( board );
#endif
        }
        break;
    case OBJ_TRAY:
        if ( board->focusHasDived ) {
            XP_S16 loc = pti->trayCursorLoc;
            if ( loc == model_getDividerLoc(board->model, board->selPlayer)) {
                board->dividerInvalid = XP_TRUE;
            } else {
                adjustForDivider( board, &loc );
                board_invalTrayTiles( board, 1 << loc );
            }
        } else {
            board_invalTrayTiles( board, ALLTILES );
            invalCellsUnderRect( board, &board->trayBounds );
            board->dividerInvalid = XP_TRUE;
        }
        break;
    case OBJ_NONE:
    case OBJ_TIMER:
        draw = XP_FALSE;
        break;
    }
    board->needsDrawing = draw || board->needsDrawing;
    return draw;
} /* invalFocusOwner */

XP_Bool
board_focusChanged( BoardCtxt* board, XWEnv xwe,
                    BoardObjectType typ, XP_Bool gained )
{
    XP_Bool draw = XP_FALSE;
    /* Called when there's been a decision to advance the focus to a new
       object, or when an object will lose it.  Need to update internal data
       structures, but also to communicate to client draw code in a way that
       doesn't assume how it's representing focus.

       Should pop focus to top on gaining it.  Calling code should not be
       seeing internal movement as focus events, but as key events we handle

       One rule: each object must draw focus indicator entirely within its own
       space.  No interdependencies.  So handling updating of focus indication
       within the tray drawing process, for example, is ok.

       Hidden tray: there's no such thing as a hidden, focussed tray.  It's 
       made TRAY_REVERSED when it gets focus.

       Problem: on palm at least take and lost are inverted: you get a take on
       the new object before a lose on the previous one.  So we want to ignore
       lost events *except* when it's a loss of something we have currently --
       meaning the focus is moving to soemthing we don't control (a
       platform-specific object)
    */

    if ( gained ) {
        /* prefer to get !gained followed by gained.  If caller doesn't do
           that, do it for 'em. */
        if ( board->focussed != OBJ_NONE ) {
            draw = board_focusChanged( board, xwe, board->focussed, XP_FALSE );
        }

        /* Are we losing focus we currently have elsewhere? */
        if ( typ != board->focussed ) {
            draw = invalFocusOwner( board, xwe ) || draw;
        }
        board->focussed = typ;
        board->focusHasDived = XP_FALSE;
        if ( OBJ_TRAY == typ) {
            board->trayHiddenPreFocus = board->trayVisState == TRAY_HIDDEN;
            if ( board->trayHiddenPreFocus ) {
                setTrayVisState( board, xwe, TRAY_REVERSED );
            }
        }
        draw = invalFocusOwner( board, xwe ) || draw;
    } else {
        /* we're losing it; inval and clear IFF we currently have same focus,
           otherwise ignore */
        if ( typ == board->focussed ) {
            draw = invalFocusOwner( board, xwe ) || draw;
            board->focussed = OBJ_NONE;

            if ( (OBJ_TRAY == typ) && (board->trayVisState == TRAY_REVERSED)
                 && board->trayHiddenPreFocus ) {
                setTrayVisState( board, xwe, TRAY_HIDDEN );
            }
        }
    }

    if ( draw ) {
        figureBoardRect( board, xwe );
    }

    return draw;
} /* board_focusChanged */

#endif /* KEYBOARD_NAV */

static XP_Bool
advanceArrow( BoardCtxt* board, XWEnv xwe )
{
    XP_Key key = board->selInfo->boardArrow.vert ?
        XP_CURSOR_KEY_DOWN :  XP_CURSOR_KEY_RIGHT;

    XP_ASSERT( board->trayVisState == TRAY_REVEALED );

    return board_moveArrow( board, xwe, key );
} /* advanceArrow */

static XP_Bool
figureNextLoc( const BoardCtxt* board, XP_Key cursorKey, 
               XP_Bool inclPending, XP_Bool forceFirst, 
               XP_U16* colP, XP_U16* rowP, 
               XP_Bool* XP_UNUSED_KEYBOARD_NAV(pUp) )
{
    XP_S16 max;
    XP_S16* useWhat = NULL;     /* make compiler happy */
    XP_S16 end = 0;
    XP_S16 incr = 0;
    XP_U16 numCols, numRows;
    XP_Bool result = XP_FALSE;

    /*     XP_ASSERT( board->focussed == OBJ_BOARD ); */
    /* don't allow cursor's jumps to reveal hidden tiles */
    if ( cursorKey != XP_KEY_NONE ) {

        numRows = model_numRows( board->model );
        numCols = model_numCols( board->model );

        switch ( cursorKey ) {

        case XP_CURSOR_KEY_DOWN:
            incr = 1;
            useWhat = (XP_S16*)rowP;
            max = numRows - 1;
            end = max;
            break;
        case XP_CURSOR_KEY_UP:
            incr = -1;
            useWhat = (XP_S16*)rowP;
            max = numRows - 1;
            end = 0;
            break;
        case XP_CURSOR_KEY_LEFT:
            incr = -1;
            useWhat = (XP_S16*)colP;
            max = numCols - 1;
            end = 0;
            break;
        case XP_CURSOR_KEY_RIGHT:
            incr = 1;
            useWhat = (XP_S16*)colP;
            max = numCols - 1;
            end = max;
            break;
        default:
            XP_LOGFF( "odd cursor key: %d", cursorKey );
        }

        if ( incr != 0 ) {
            for ( ; ; ) {
                if ( *useWhat == end ) {
#ifdef KEYBOARD_NAV
                    if ( !!pUp ) {
                        *pUp = XP_TRUE;
                    }
#endif
                    break;
                }
                *useWhat += incr;
                if ( forceFirst
                     || !cellOccupied( board, *colP, *rowP, inclPending ) ) {
                    result = XP_TRUE;
                    break;
                }
            }
        }
    }

    return result;
} /* figureNextLoc */

static XP_Bool
board_moveArrow( BoardCtxt* board, XWEnv xwe, XP_Key cursorKey )
{
    XP_U16 col, row;
    XP_Bool changed;

    setArrowVisible( board, XP_TRUE );
    (void)getArrow( board, &col, &row );
    changed = figureNextLoc( board, cursorKey, XP_TRUE, XP_FALSE, 
                             &col, &row, NULL );
    if ( changed ) {
        (void)setArrow( board, xwe, col, row, NULL );
    }
    return changed;
} /* board_moveArrow */

#ifdef KEYBOARD_NAV
static XP_Key
stripAlt( XP_Key key, XP_Bool* wasAlt )
{
    XP_Bool alt = XP_FALSE;
    switch ( key ) {
    case XP_CURSOR_KEY_ALTDOWN:
    case XP_CURSOR_KEY_ALTRIGHT:
    case XP_CURSOR_KEY_ALTUP:
    case XP_CURSOR_KEY_ALTLEFT:
        alt = XP_TRUE;
        --key;
    default:
        break;
    }

    if ( !!wasAlt ) {
        *wasAlt = alt;
    }
    return key;
} /* stripAlt */

static XP_Bool
board_moveCursor( BoardCtxt* board, XWEnv xwe, XP_Key cursorKey,
                  XP_Bool preflightOnly, XP_Bool* up )
{
    PerTurnInfo* pti = board->selInfo;
    BdCursorLoc loc = pti->bdCursor;
    XP_U16 col = loc.col;
    XP_U16 row = loc.row;
    XP_Bool changed;

    XP_Bool altSet;
    cursorKey = stripAlt( cursorKey, &altSet );

    changed = figureNextLoc( board, cursorKey, XP_FALSE, !altSet,
                             &col, &row, up );
    if ( changed && !preflightOnly ) {
        invalCell( board, loc.col, loc.row );
        invalCell( board, col, row );
        loc.col = col;
        loc.row = row;
        pti->bdCursor = loc;
        scrollIntoView( board, xwe, col, row );
    }
    return changed;
} /* board_moveCursor */
#endif

XP_Bool
rectContainsPt( const XP_Rect* rect, XP_S16 xx, XP_S16 yy )
{
    /* 7/4 Made <= into <, etc., because a tap on the right boundary of the
       board was still mapped onto the board but dividing by scale put it in
       the 15th column.  If this causes other problems and the '=' chars have
       to be put back then deal with that, probably by forcing an
       out-of-bounds col/row to the nearest possible. */
    XP_Bool result = ( rect->top <= yy
                       && rect->left <= xx
                       && (rect->top + rect->height) >= yy
                       && (rect->left + rect->width) >= xx );
    return result;
} /* rectContainsPt */

XP_Bool
rectsIntersect( const XP_Rect* rect1, const XP_Rect* rect2 )
{
    XP_Bool intersect = XP_TRUE;
    if ( rect1->top >= rect2->top + rect2->height ) {
        intersect = XP_FALSE;
    } else if ( rect1->left >= rect2->left + rect2->width ) {
        intersect = XP_FALSE;
    } else if ( rect2->top >= rect1->top + rect1->height ) {
        intersect = XP_FALSE;
    } else if ( rect2->left >= rect1->left + rect1->width ) {
        intersect = XP_FALSE;
    }
    return intersect;
} /* rectsIntersect */

static XP_Bool
replaceLastTile( BoardCtxt* board, XWEnv xwe )
{
    XP_Bool result = XP_FALSE;
    XP_S16 turn = board->selPlayer;
    XP_U16 tilesInMove = model_getCurrentMoveCount( board->model, turn );
    if ( tilesInMove > 0 ) {
        XP_U16 col, row;
        Tile tile;
        XP_Bool isBlank;
        XP_S16 index;
        XP_Bool isVertical;
        XP_Bool directionKnown = 
            model_getCurrentMoveIsVertical( board->model, turn, &isVertical );
        if ( directionKnown && board->isFlipped ) {
            isVertical = !isVertical;
        }

        index = -1;
        model_getCurrentMoveTile( board->model, board->selPlayer, &index,
                                  &tile, &col, &row, &isBlank );
        model_moveBoardToTray( board->model, xwe, board->selPlayer, col, row, -1 );

        flipIf( board, col, row, &col, &row );
        setArrow( board, xwe, col, row, directionKnown? &isVertical : NULL );
        result = XP_TRUE;
    }

    return result;
} /* replaceLastTile */

XP_Bool
moveTileToBoard( BoardCtxt* board, XWEnv xwe, XP_U16 col, XP_U16 row,
                 XP_U16 tileIndex, Tile blankFace )
{
    if ( cellOccupied( board, col, row, XP_TRUE ) ) {
        return XP_FALSE;
    }

    flipIf( board, col, row, &col, &row );
    model_moveTrayToBoard( board->model, xwe, board->selPlayer, col, row,
                           tileIndex, blankFace );

    return XP_TRUE;
} /* moveTileToBoard */

#ifdef KEY_SUPPORT

typedef struct _FTData {
    Tile tile;
    XP_Bool found;
} FTData;

static XP_Bool
foundTiles( void* closure, const Tile* tiles, int XP_UNUSED_DBG(len) )
{
    XP_ASSERT( 1 == len );
    FTData* ftp = (FTData*)closure;
    ftp->tile = tiles[0];
    ftp->found = XP_TRUE;
    return XP_FALSE;
}

/* Return number between 0 and MAX_TRAY_TILES-1 for valid index, < 0 otherwise */
static XP_S16
keyToIndex( BoardCtxt* board, XP_Key key, Tile* blankFace )
{
    /* Map numbers 1-7 to tiles in tray.  This is a hack to workaround
       temporary lack of key input on smartphone.  */
    ModelCtxt* model = board->model;
    XP_S16 tileIndex = -1;
# ifdef NUMBER_KEY_AS_INDEX
    tileIndex = key - '0' - 1; /* user's model is 1-based; ours is 0-based */
    if (tileIndex >= model_getNumTilesInTray( model, board->selPlayer ) ) {
        tileIndex = -1;         /* error */
    }
# endif

    if ( tileIndex < 0 ) {
        const DictionaryCtxt* dict = model_getDictionary( model );
        XP_UCHAR buf[2] = { key, '\0' };

        /* Figure out if we have the tile in the tray  */
        FTData ftd = {0};
        dict_tilesForString( dict, buf, 0, foundTiles, &ftd );
        if ( ftd.found ) {
            XP_S16 turn = board->selPlayer;
            tileIndex = model_trayContains( model, turn, ftd.tile );
            if ( tileIndex < 0 ) {
                Tile blankTile = dict_getBlankTile( dict );
                tileIndex = model_trayContains( model, turn, blankTile );
                if ( tileIndex >= 0 && !!blankFace ) { /* there's a blank for it */
                    *blankFace = ftd.tile;
                }
            }
        }
    }

    return tileIndex;
} /* keyToIndex */

static XP_Bool
moveKeyTileToBoard( BoardCtxt* board, XWEnv xwe, XP_Key cursorKey, XP_Bool* gotArrow )
{
    XP_U16 col, row;
    XP_Bool haveDest;

    XP_ASSERT( !TRADE_IN_PROGRESS( board ) );

    /* Is there a cursor at all? */
    haveDest = getArrow( board, &col, &row );
    *gotArrow = haveDest;
#ifdef KEYBOARD_NAV
    if ( !haveDest && (board->focussed == OBJ_BOARD) && board->focusHasDived ) {
        BdCursorLoc loc = board->selInfo->bdCursor;
        col = loc.col;
        row = loc.row;
        haveDest = XP_TRUE;
    }
#endif

    if ( haveDest ) {
        Tile blankFace = EMPTY_TILE;
        XP_S16 tileIndex = keyToIndex( board, cursorKey, &blankFace );

        haveDest = (tileIndex >= 0)
            && moveTileToBoard( board, xwe, col, row, tileIndex, blankFace );
    }

    return haveDest;
} /* moveKeyTileToBoard */
#endif  /* #ifdef KEY_SUPPORT */

static void
setArrow( BoardCtxt* board, XWEnv xwe, XP_U16 col, XP_U16 row, XP_Bool* vertp )
{
    XP_U16 player = board->selPlayer;
    BoardArrow* arrow = &board->pti[player].boardArrow;
    invalCell( board, arrow->col, arrow->row );
    invalCell( board, col, row );

    arrow->col = (XP_U8)col;
    arrow->row = (XP_U8)row;
    if ( !!vertp ) {
        arrow->vert = *vertp;
    }

    scrollIntoView( board, xwe, col, row );
} /* setArrow */

static XP_Bool
getArrowFor( const BoardCtxt* board, XP_U16 player, XP_U16* col, XP_U16* row )
{
    const BoardArrow* arrow = &board->pti[player].boardArrow;
    *col = arrow->col;
    *row = arrow->row;
    return arrow->visible;
} /* getArrowFor */

static XP_Bool
getArrow( const BoardCtxt* board, XP_U16* col, XP_U16* row )
{
    return getArrowFor( board, board->selPlayer, col, row );
} /* getArrow */

static XP_Bool
setArrowVisible( BoardCtxt* board, XP_Bool visible )
{
    return setArrowVisibleFor( board, board->selPlayer, visible );
} /* setArrowVisible */

static XP_Bool
setArrowVisibleFor( BoardCtxt* board, XP_U16 player, XP_Bool visible )
{
    BoardArrow* arrow = &board->pti[player].boardArrow;
    XP_Bool result = arrow->visible;
    if ( arrow->visible != visible ) {
        arrow->visible = visible;
        invalArrowCell( board );
    }
    return result;
} /* setArrowVisibleFor */

/*****************************************************************************
 * Listener callbacks
 ****************************************************************************/
static void
boardCellChanged( XWEnv xwe, void* p_board, XP_U16 turn, XP_U16 modelCol,
                  XP_U16 modelRow, XP_Bool added )
{
    BoardCtxt* board = (BoardCtxt*)p_board;
    XP_Bool pending, found;
    XP_U16 col, row;

    flipIf( board, modelCol, modelRow, &col, &row );

    /* for each player, check if the tile overwrites the cursor */
    found = model_getTile( board->model, modelCol, modelRow, XP_TRUE, turn,
                           NULL, NULL, &pending, NULL );

    XP_ASSERT( !added || found ); /* if added is true so must found be */

    if ( !added && !found ) {
        /* nothing to do */
    } else {
        XP_U16 ccol, crow;
        XP_U16 player, nPlayers;
    
        nPlayers = board->gi->nPlayers;

        /* This is a bit gross.  It's an attempt to combine two loops.  In one
           case, we've added a tile (tentative move, say) and need to hide the
           cursor for the player who added the tile.  In the second, we're
           changing the state of a tile (from tentative to permanent, say) and
           now need to worry about the players who turn it _isn't_ but whose
           cursor may be in the wrong place.  This latter case happens a lot
           in multi-device games when a turn shows up from elsewhere and plops
           down on the board. */
        for ( player = 0; player < nPlayers; ++player ) {
            if ( (added && (!pending || turn == board->selPlayer)) 
                 || (found && turn != board->selPlayer) ) {
                if ( getArrowFor( board, player, &ccol, &crow )
                     && ( (ccol == col) && (crow == row) ) ) {
                    setArrowVisibleFor( board, player, XP_FALSE );
                }
            }
        }

        scrollIntoView( board, xwe, col, row );
    }

    invalCell( (BoardCtxt*)p_board, col, row );
} /* boardCellChanged */

static void
boardTilesChanged( void* p_board, XP_U16 turn, XP_S16 index1, XP_S16 index2 )
{
    BoardCtxt* board = (BoardCtxt*)p_board;
    if ( turn == board->selPlayer ) {
        invalTrayTilesBetween( board, index1, index2 );

        /* If we're changing the set of ignored tiles, reset the engine */
        XP_U16 divLoc = model_getDividerLoc( board->model, turn );
        if ( index1 < divLoc && index2 < divLoc ) {
            /* both below; no need to reset */
        } else if ( index1 < divLoc || index2 < divLoc ) {
            board_resetEngine( board );
        }
    }
} /* boardTilesChanged */

static void
dictChanged( void* p_board, XWEnv xwe, XP_S16 playerNum,
             const DictionaryCtxt* oldDict,
             const DictionaryCtxt* newDict )
{
    BoardCtxt* board = (BoardCtxt*)p_board;
    if ( !!board->draw ) {
        if ( (NULL == oldDict) || (oldDict != newDict) ) {
            draw_dictChanged( board->draw, xwe, playerNum, newDict );
        }
    }
}

static void
boardTurnChanged( XWEnv xwe, void* p_board )
{
    BoardCtxt* board = (BoardCtxt*)p_board;
    XP_S16 nextPlayer;

    XP_ASSERT( board->timerSaveCount == 0 );

    board->gameOver = XP_FALSE;

    nextPlayer = chooseBestSelPlayer( board );
    if ( nextPlayer >= 0 ) {
        XP_U16 nHumans = gi_countLocalPlayers( board->gi, XP_TRUE );
        selectPlayerImpl( board, xwe, nextPlayer, nHumans <= 1, XP_TRUE );
    }

    setTimerIf( board, xwe );

    board->scoreBoardInvalid = XP_TRUE;
} /* boardTurnChanged */

static void
boardGameOver( XWEnv xwe, void* closure, XP_S16 quitter )
{
    BoardCtxt* board = (BoardCtxt*)closure;    
    board->scoreBoardInvalid = XP_TRUE; /* not sure if this will do it. */
    board->gameOver = XP_TRUE;
    util_notifyGameOver( board->util, xwe, quitter );
} /* boardGameOver */

static void
forceRectToBoard( const BoardCtxt* board, XP_Rect* rect )
{
    XP_Rect bounds = board->boardBounds;
    if ( rect->left < bounds.left ) {
        rect->left = bounds.left;
    }
    if ( rect->top < bounds.top ) {
        rect->top = bounds.top;
    }
    if ( (rect->left + rect->width) > (bounds.left + bounds.width) ) {
        rect->left -= (rect->left+rect->width) - (bounds.left+bounds.width);
    }
    if ( rect->top + rect->height > bounds.top + bounds.height ) {
        rect->top -= (rect->top+rect->height) - (bounds.top+bounds.height);
    }
} /* forceRectToBoard */

void
getDragCellRect( BoardCtxt* board, XP_U16 col, XP_U16 row, XP_Rect* rectP )
{
    XP_Rect rect;
    XP_U16 tmp;

    getCellRect( board, col, row, &rect );

    tmp = rect.width;
    rect.width = board->trayScaleH;
    rect.left -= (rect.width - tmp) / 2;

    tmp = rect.height;
    rect.height = board->trayScaleV;
    rect.top -= (rect.height - tmp) / 2;

    *rectP = rect;
    forceRectToBoard( board, rectP );
}

void
invalDragObj( BoardCtxt* board, const DragObjInfo* di )
{
    if ( OBJ_BOARD == di->obj ) {
        XP_Rect rect;
        getDragCellRect( board, di->u.board.col, di->u.board.row, &rect );
        invalCellsUnderRect( board, &rect );
    } else if ( OBJ_TRAY == di->obj ) {
        board_invalTrayTiles( board, 1 << di->u.tray.index );
    }
} /* invalCurObj */

#ifdef CPLUS
}
#endif
