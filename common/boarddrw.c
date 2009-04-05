/* -*-mode: C; fill-column: 78; compile-command: "cd ../linux && make MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 1997 - 2009 by Eric House (xwords@eehouse.org).  All rights
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
#include "engine.h"
#include "util.h"
#include "mempool.h" /* debug only */
#include "memstream.h"
#include "strutils.h"
#include "LocalizedStrIncludes.h"

#include "boardp.h"
#include "dragdrpp.h"
#include "dbgutil.h"

#ifdef CPLUS
extern "C" {
#endif

static XP_Bool drawCell( BoardCtxt* board, XP_U16 col, XP_U16 row, 
                         XP_Bool skipBlanks );
static void drawBoard( BoardCtxt* board );
static void scrollIfCan( BoardCtxt* board );
static XP_Bool cellFocused( const BoardCtxt* board, XP_U16 col, XP_U16 row );
static void drawTradeWindowIf( BoardCtxt* board );


#ifdef XWFEATURE_SEARCHLIMIT
static HintAtts figureHintAtts( BoardCtxt* board, XP_U16 col, XP_U16 row );
#else
# define figureHintAtts(b,c,r) HINT_BORDER_NONE
#endif


#ifdef POINTER_SUPPORT
static void drawDragTileIf( BoardCtxt* board );
#endif

#ifdef KEYBOARD_NAV
#ifdef PERIMETER_FOCUS
static void
invalOldPerimeter( BoardCtxt* board )
{
    /* We need to inval the center of the row that's moving into the center
       from a border (at which point it got borders drawn on it.) */
    XP_S16 diff = board->yOffset - board->prevYScrollOffset;
    XP_U16 firstRow, lastRow;
    XP_ASSERT( diff != 0 );
    if ( diff < 0 ) {
        /* moving up; inval row previously on bottom */
        firstRow = board->yOffset + 1;
        lastRow = board->prevYScrollOffset;
    } else {
        XP_U16 nVisible = board->lastVisibleRow - board->yOffset + 1;
        lastRow = board->prevYScrollOffset + nVisible - 1;
        firstRow = lastRow - diff + 1;
    }
    XP_ASSERT( firstRow <= lastRow );
    while ( firstRow <= lastRow ) {
        board->redrawFlags[firstRow] |= ~0;
        ++firstRow;
    }
} /* invalOldPerimeter */
#endif
#endif

/* if any of a blank's neighbors is invalid, so must the blank become (since
 * they share a border and drawing the neighbor will redraw the blank's border
 * too) We'll want to redraw only those blanks that are themselves already
 * invalid *OR* that become invalid this way, and so we'll build a new
 * BlankQueue of them and replace the old.
 *
 * I'm not sure what happens if two blanks are neighbors.
 */
#define INVAL_BIT_SET(b,c,r) (((b)->redrawFlags[(r)] & (1 <<(c))) != 0)
static void
invalBlanksWithNeighbors( BoardCtxt* board, BlankQueue* bqp ) 
{
    XP_U16 i;
    XP_U16 lastCol, lastRow;
    BlankQueue invalBlanks;
    XP_U16 nInvalBlanks = 0;

    lastCol = model_numCols(board->model) - 1;
    lastRow = model_numRows(board->model) - 1;

    for ( i = 0; i < bqp->nBlanks; ++i ) {
        XP_U16 modelCol = bqp->col[i];
        XP_U16 modelRow = bqp->row[i];
        XP_U16 col, row;

        flipIf( board, modelCol, modelRow, &col, &row );

        if ( INVAL_BIT_SET( board, col, row )
             || (col > 0 && INVAL_BIT_SET( board, col-1, row ))
             || (col < lastCol && INVAL_BIT_SET( board, col+1, row ))
             || (row > 0 && INVAL_BIT_SET( board, col, row-1 ))
             || (row < lastRow && INVAL_BIT_SET( board, col, row+1 )) ) {

            invalCell( board, col, row );

            invalBlanks.col[nInvalBlanks] = (XP_U8)col;
            invalBlanks.row[nInvalBlanks] = (XP_U8)row;
            ++nInvalBlanks;
        }
    }
    invalBlanks.nBlanks = nInvalBlanks;
    XP_MEMCPY( bqp, &invalBlanks, sizeof(*bqp) );
} /* invalBlanksWithNeighbors */


#ifdef XWFEATURE_SEARCHLIMIT
static HintAtts
figureHintAtts( BoardCtxt* board, XP_U16 col, XP_U16 row )
{
    HintAtts result = HINT_BORDER_NONE;

    /* while lets us break to exit... */
    while ( board->trayVisState == TRAY_REVEALED
            && !board->gi->hintsNotAllowed
            && board->gi->allowHintRect ) {
        BdHintLimits limits;
        if ( dragDropGetHintLimits( board, &limits ) ) {
            /* do nothing */
        } else if ( board->selInfo->hasHintRect ) {
            limits = board->selInfo->limits;
        } else {
            break;
        }

        if ( col < limits.left ) break;
        if ( row < limits.top ) break;
        if ( col > limits.right ) break;
        if ( row > limits.bottom ) break;

        if ( col == limits.left ) {
            result |= HINT_BORDER_LEFT;
        }
        if ( col == limits.right ) {
            result |= HINT_BORDER_RIGHT;
        }
        if ( row == limits.top) {
            result |= HINT_BORDER_TOP;
        }
        if ( row == limits.bottom ) {
            result |= HINT_BORDER_BOTTOM;
        }
#ifndef XWFEATURE_SEARCHLIMIT_DOCENTERS
        if ( result == HINT_BORDER_NONE ) {
            result = HINT_BORDER_CENTER;
        }
#endif
        break;
    }

    return result;
} /* figureHintAtts */
#endif

static XP_Bool
rectContainsRect( XP_Rect* rect1, XP_Rect* rect2 )
{
    return ( rect1->top <= rect2->top
             && rect1->left <= rect2->left
             && rect1->top + rect1->height >= rect2->top + rect2->height
             && rect1->left + rect1->width >= rect2->left + rect2->width );
} /* rectContainsRect */

static void
makeMiniWindowForTrade( BoardCtxt* board )
{
    const XP_UCHAR* text;

    text = draw_getMiniWText( board->draw, INTRADE_MW_TEXT );

    makeMiniWindowForText( board, text, MINIWINDOW_TRADING );
} /* makeMiniWindowForTrade */

static void
drawBoard( BoardCtxt* board )
{
    if ( board->needsDrawing 
         && draw_boardBegin( board->draw, 
                             &board->boardBounds, 
                             dfsFor( board, OBJ_BOARD ) ) ) {

        XP_Bool allDrawn = XP_TRUE;
        XP_S16 lastCol, i;
        XP_S16 row;
        ModelCtxt* model = board->model;
        BoardArrow const* arrow = NULL;
        BlankQueue bq;

        scrollIfCan( board ); /* this must happen before we count blanks
                                 since it invalidates squares */

        /* This is freaking expensive!!!! PENDING FIXME Can't we start from
           what's invalid rather than scanning the entire model every time
           somebody dirties a single cell? */
        model_listPlacedBlanks( model, board->selPlayer, 
                                board->trayVisState == TRAY_REVEALED, &bq );
        invalBlanksWithNeighbors( board, &bq );

        /* figure out now, before clearing inval bits, if we'll need to draw
           the arrow later */
        if ( board->trayVisState == TRAY_REVEALED ) {
            BoardArrow const* tmpArrow = &board->selInfo->boardArrow;
            if ( tmpArrow->visible ) {
                XP_U16 col = tmpArrow->col;
                XP_U16 row = tmpArrow->row;
                if ( INVAL_BIT_SET( board, col, row )
                     && !cellOccupied( board, col, row, XP_TRUE ) ) {
                    arrow = tmpArrow;
                }
            }
        }

        for ( row = board->yOffset; row <= board->lastVisibleRow; ++row ) {
            XP_U16 rowFlags = board->redrawFlags[row];
            if ( rowFlags != 0 ) {
                XP_U16 colMask;
                XP_U16 failedBits = 0;
                lastCol = model_numCols( model );
                for ( colMask = 1<<(lastCol-1); lastCol--; colMask >>= 1 ) {
                    if ( (rowFlags & colMask) != 0 ) {
                        if ( !drawCell( board, lastCol, row, XP_TRUE )) {
                            failedBits |= colMask;
                            allDrawn = XP_FALSE;
                        }
                    }
                }
                board->redrawFlags[row] = failedBits;
            }
        }

        /* draw the blanks we skipped before */
        for ( i = 0; i < bq.nBlanks; ++i ) {
            if ( !drawCell( board, bq.col[i], bq.row[i], XP_FALSE ) ) {
                allDrawn = XP_FALSE;
            }
        }

        if ( !!arrow ) {
            XP_U16 col = arrow->col;
            XP_U16 row = arrow->row;
            XP_Rect arrowRect;
            if ( getCellRect( board, col, row, &arrowRect ) ) {
                XWBonusType bonus;
                HintAtts hintAtts;
                CellFlags flags = CELL_NONE;
                bonus = util_getSquareBonus( board->util, model, col, row );
                hintAtts = figureHintAtts( board, col, row );
#ifdef KEYBOARD_NAV
                if ( cellFocused( board, col, row ) ) {
                    flags |= CELL_ISCURSOR;
                }
#endif
                draw_drawBoardArrow( board->draw, &arrowRect, bonus, 
                                     arrow->vert, hintAtts, flags );
            }
        }

        /* I doubt the two of these can happen at the same time */
        drawTradeWindowIf( board );
#ifdef POINTER_SUPPORT
        drawDragTileIf( board );
#endif
        draw_objFinished( board->draw, OBJ_BOARD, &board->boardBounds, 
                          dfsFor( board, OBJ_BOARD ) );

        board->needsDrawing = !allDrawn;
    }
} /* drawBoard */


static XP_Bool
drawCell( BoardCtxt* board, XP_U16 col, XP_U16 row, XP_Bool skipBlanks )
{
    XP_Bool success = XP_TRUE;
    XP_Rect cellRect;
    Tile tile;
    XP_Bool isBlank, isEmpty, recent, pending = XP_FALSE;
    XWBonusType bonus;
    ModelCtxt* model = board->model;
    DictionaryCtxt* dict = model_getDictionary( model );
    XP_U16 modelCol, modelRow;

    if ( dict != NULL && getCellRect( board, col, row, &cellRect ) ) {

        /* We want to invert EITHER the current pending tiles OR the most recent
         * move.  So if the tray is visible AND there are tiles missing from it,
         * show them.  Otherwise show the most recent move.
         */
        XP_U16 selPlayer = board->selPlayer;
        XP_U16 curCount = model_getCurrentMoveCount( model, selPlayer );
        XP_Bool showPending = board->trayVisState == TRAY_REVEALED
            && curCount > 0;

        flipIf( board, col, row, &modelCol, &modelRow );

        /* This 'while' is only here so I can 'break' below */
        while ( board->trayVisState == TRAY_HIDDEN ||
                !rectContainsRect( &board->trayBounds, &cellRect ) ) {
            XP_UCHAR ch[4] = {'\0'};
            XP_S16 owner = -1;
            XP_Bool invert = XP_FALSE;
            XP_Bitmaps bitmaps;
            XP_Bitmaps* bptr = NULL;
            const XP_UCHAR* textP = NULL;
            HintAtts hintAtts;
            CellFlags flags = CELL_NONE;
            XP_Bool isOrigin;

            isEmpty = !model_getTile( model, modelCol, modelRow, showPending,
                                        selPlayer, &tile, &isBlank,
                                        &pending, &recent );
            if ( dragDropIsBeingDragged( board, col, row, &isOrigin ) ) {
                flags |= isOrigin? CELL_DRAGSRC : CELL_DRAGCUR;
                if ( isEmpty && !isOrigin ) {
                    dragDropTileInfo( board, &tile, &isBlank );
                    pending = XP_TRUE;
                    recent = XP_FALSE;
                    isEmpty = XP_FALSE;
                }
            }

            if ( isEmpty ) {
                isBlank = XP_FALSE;
                flags |= CELL_ISEMPTY;
            } else if ( isBlank && skipBlanks ) {
                break;
            } else {
                if ( board->showColors ) {
                    owner = (XP_S16)model_getCellOwner( model, modelCol, 
                                                        modelRow );
                }

                invert = showPending? pending : recent;

                if ( board->showCellValues ) {
                    Tile valTile = isBlank? dict_getBlankTile( dict ) : tile;
                    XP_U16 val = dict_getTileValue( dict, valTile );
                    XP_SNPRINTF( ch, VSIZE(ch), "%d", val );
                    textP = ch;
                } else {
                    if ( dict_faceIsBitmap( dict, tile ) ) {
                        dict_getFaceBitmaps( dict, tile, &bitmaps );
                        bptr = &bitmaps;
                    }
                    textP = dict_getTileString( dict, tile );
                }
            }
            bonus = util_getSquareBonus( board->util, model, col, row );
            hintAtts = figureHintAtts( board, col, row );

            if ( (col==board->star_row) && (row==board->star_row) ) {
                flags |= CELL_ISSTAR;
            }
            if ( invert ) {
                flags |= CELL_HIGHLIGHT;
            }
            if ( isBlank ) {
                flags |= CELL_ISBLANK;
            }
#ifdef KEYBOARD_NAV
            if ( cellFocused( board, col, row ) ) {
                flags |= CELL_ISCURSOR;
            }
#endif

            success = draw_drawCell( board->draw, &cellRect, textP, bptr,
                                     tile, owner, bonus, hintAtts, flags );
            break;
        }
    }
    return success;
} /* drawCell */

#ifdef KEYBOARD_NAV
DrawFocusState
dfsFor( BoardCtxt* board, BoardObjectType obj )
{
    DrawFocusState dfs;
    if ( (board->focussed == obj) && !board->hideFocus ) {
        if ( board->focusHasDived ) {
            dfs = DFS_DIVED;
        } else {
            dfs = DFS_TOP;
        }
    } else {
        dfs = DFS_NONE;
    }
    return dfs;
} /* dfsFor */

static XP_Bool
cellFocused( const BoardCtxt* board, XP_U16 col, XP_U16 row )
{
    XP_Bool focussed = XP_FALSE;

    if ( (board->focussed == OBJ_BOARD) && !board->hideFocus ) {
        if ( board->focusHasDived ) {
            if ( (col == board->selInfo->bdCursor.col)
                 && (row == board->selInfo->bdCursor.row) ) {
                focussed = XP_TRUE;
            }
        } else {
#ifdef PERIMETER_FOCUS
            focussed = (col == 0)
                || (col == model_numCols(board->model) - 1)
                || (row == board->yOffset)
                || (row == board->lastVisibleRow);
#else
            focussed = XP_TRUE;
#endif
        }
    }
    return focussed;
} /* cellFocused */
#endif

#ifdef POINTER_SUPPORT
static void
drawDragTileIf( BoardCtxt* board )
{
    if ( dragDropInProgress( board ) ) {
        XP_U16 col, row;
        if ( dragDropGetBoardTile( board, &col, &row ) ) {
            XP_Rect rect;
            Tile tile;
            XP_Bool isBlank;
            const XP_UCHAR* face;
            XP_Bitmaps bitmaps;
            XP_S16 value;
            CellFlags flags;

            getDragCellRect( board, col, row, &rect );

            dragDropTileInfo( board, &tile, &isBlank );

            face = getTileDrawInfo( board, tile, isBlank, &bitmaps, 
                                    &value );

            flags = CELL_DRAGCUR;
            if ( isBlank ) {
                flags |= CELL_ISBLANK;
            }
            if ( board->hideValsInTray && !board->showCellValues ) {
                flags |= CELL_VALHIDDEN;
            }
            draw_drawTileMidDrag( board->draw, &rect, face, 
                                  bitmaps.nBitmaps > 0 ? &bitmaps : NULL, 
                                  value, board->selPlayer, flags );
        }
    }
} /* drawDragTileIf */
#endif

static void
scrollIfCan( BoardCtxt* board )
{
    if ( board->yOffset != board->prevYScrollOffset ) {
        XP_Rect scrollR = board->boardBounds;
        XP_Bool scrolled;
        XP_S16 dist;

#ifdef PERIMETER_FOCUS
        if ( (board->focussed == OBJ_BOARD)
             && !board->focusHasDived 
             && !board->hideFocus ) {
            invalOldPerimeter( board );
        }
#endif
        invalSelTradeWindow( board );
        dist = (board->yOffset - board->prevYScrollOffset)
            * board->boardVScale;

        scrolled = draw_vertScrollBoard( board->draw, &scrollR, dist, 
                                         dfsFor( board, OBJ_BOARD ) );

        if ( scrolled ) {
            /* inval the rows that have been scrolled into view.  I'm cheating
               making the client figure the inval rect, but Palm's the first
               client and it does it so well.... */
            invalCellsUnderRect( board, &scrollR );
        } else {
            board_invalAll( board );
        }
        board->prevYScrollOffset = board->yOffset;
    }
} /* scrollIfCan */

static void
drawTradeWindowIf( BoardCtxt* board )
{
    if ( board->tradingMiniWindowInvalid &&
         TRADE_IN_PROGRESS(board) && board->trayVisState == TRAY_REVEALED ) {
        MiniWindowStuff* stuff;

        makeMiniWindowForTrade( board );

        stuff = &board->miniWindowStuff[MINIWINDOW_TRADING];
        draw_drawMiniWindow( board->draw, stuff->text,
                             &stuff->rect, (void**)NULL );

        board->tradingMiniWindowInvalid = XP_FALSE;
    }
} /* drawTradeWindowIf */

XP_Bool
board_draw( BoardCtxt* board )
{
    if ( board->boardBounds.width > 0 ) {

        drawScoreBoard( board );

        drawTray( board );

        drawBoard( board );
    }
    return !board->needsDrawing;
} /* board_draw */

#ifdef CPLUS
}
#endif
