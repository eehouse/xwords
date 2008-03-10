/* -*-mode: C; fill-column: 78; c-basic-offset: 4; compile-command: "cd ../linux && make MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 1997 - 2008 by Eric House (xwords@eehouse.org).  All rights
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

#ifdef CPLUS
extern "C" {
#endif

#include "dragdrpp.h"

static XP_Bool dragDropContinueImpl( BoardCtxt* board, XP_U16 xx, XP_U16 yy,
                                     BoardObjectType* onWhichP );

XP_Bool
dragDropInProgress( const BoardCtxt* board )
{
    const DragState* ds = &board->dragState;
/*     LOG_RETURNF( "%d", ds->dragInProgress ); */
    return ds->dragInProgress;
}

static XP_Bool
ddStartBoard( BoardCtxt* board, XP_U16 col, XP_U16 row )
{
    DragState* ds = &board->dragState;
    XP_Bool ignore, found;
    XP_U16 modelc, modelr;

    ds->start.u.board.col = col;
    ds->start.u.board.row = row;

    flipIf( board, col, row, &modelc, &modelr );

    found = model_getTile( board->model, modelc, modelr, XP_TRUE, 
                           board->selPlayer, &ds->tile, &ds->isBlank, 
                           &ignore, &ignore );
    XP_ASSERT( found );

    return XP_TRUE;
} /* ddStartBoard */

static XP_Bool
ddStartTray( BoardCtxt* board, XP_U16 x, XP_U16 y )
{
    XP_Bool canDrag;
    DragState* ds = &board->dragState;

    XP_Bool onDivider;
    XP_S16 index = pointToTileIndex( board, x, y, &onDivider );
    canDrag = onDivider || index >= 0;
    if ( canDrag ) {
        XP_S16 selPlayer = board->selPlayer;
        if ( onDivider ) {
            ds->dividerOnly = XP_TRUE;
            board->dividerInvalid = XP_TRUE;
            ds->start.u.tray.index = board->dividerLoc[selPlayer];
        } else {
            Tile tile;
            tile = model_getPlayerTile( board->model, selPlayer, index ); 
            ds->isBlank = 
                tile == dict_getBlankTile( model_getDictionary(board->model) );
            ds->tile = tile;

            ds->start.u.tray.index = index;

            /* during drag the moving tile is drawn as selected, so inval
               currently selected tile. */
            board_invalTrayTiles( board, board->traySelBits[selPlayer] );
        }
    }

    return canDrag;
} /* ddStartTray */

/* x and y, in board coordinates (not model, should the board be flipped), are
 * already col,row (cells) in the board case, but real x/y (pixels) on the
 * tray.
*/
XP_Bool
dragDropStart( BoardCtxt* board, BoardObjectType obj, XP_U16 x, XP_U16 y )
{
    XP_Bool result = XP_FALSE;
    DragState* ds = &board->dragState;
    XP_ASSERT( !dragDropInProgress(board) );
    XP_MEMSET( ds, 0, sizeof(*ds) );

    ds->start.obj = obj;

    if ( OBJ_BOARD == obj ) {
        result = ddStartBoard( board, x, y );
    } else if ( OBJ_TRAY == obj ) {
        result = ddStartTray( board, x, y );
    } else {
        XP_ASSERT(0);
    }

    if ( result ) {
        ds->cur = ds->start;
        invalDragObj( board, &ds->start );
        ds->dragInProgress = XP_TRUE;
    }

    return result;
} /* dragDropStart */

/* We're potentially dragging.  If we're leaving one object, inval it.
 * If we're entering another, inval it.  Track where we are so we don't inval
 * again.  If we didn't really move, don't inval anything!
 *
 * Our overriding concern must be to preserve data integrity in the face of
 * buggy OSes that may drop events (esp. MouseUp).  So never own a tile: keep
 * it attached either to the tray or the board.  If the state of the board is
 * to change as dragging happens, that's for the board (view) to display, not
 * the model.  So drawing has to be aware of drag-and-drop, but we don't
 * change the model until the pen's actually lifted.  If we never get the
 * pen-up event at the worst the board's drawn incorrectly for a bit.
 *
 * Exception: since divider location is in the board rather than the model
 * just change it every time we can.
 */
XP_Bool
dragDropContinue( BoardCtxt* board, XP_U16 xx, XP_U16 yy )
{
    BoardObjectType ignore;
    XP_ASSERT( dragDropInProgress(board) );

    return dragDropContinueImpl( board, xx, yy, &ignore );
}

XP_Bool
dragDropEnd( BoardCtxt* board, XP_U16 xx, XP_U16 yy, XP_Bool* dragged )
{
    DragState* ds = &board->dragState;
    BoardObjectType newObj;

    XP_ASSERT( dragDropInProgress(board) );

    (void)dragDropContinueImpl( board, xx, yy, &newObj );
    *dragged = ds->didMove;

    /* If we've dropped on something, put the tile there!  Since we
       don't remove it from its earlier location until it's dropped,
       we need to specify where it's coming from. */
    if ( ds->dividerOnly ) {
        board->dividerInvalid = XP_TRUE;
        /* nothing to do */
    } else {
        XP_U16 mod_startc, mod_startr;

        flipIf( board, ds->start.u.board.col, ds->start.u.board.row,
                &mod_startc, &mod_startr );

        if ( newObj == OBJ_TRAY ) {
            if ( ds->start.obj == OBJ_BOARD ) { /* board->tray is pending */
                model_moveBoardToTray( board->model, board->selPlayer, 
                                       mod_startc, mod_startr, 
                                       ds->cur.u.tray.index );
            } else {
                model_moveTileOnTray( board->model, board->selPlayer, 
                                      ds->start.u.tray.index,
                                      ds->cur.u.tray.index );
            }
        } else if ( newObj == OBJ_BOARD ) {
            if ( !cellOccupied( board, ds->cur.u.board.col, 
                                ds->cur.u.board.row, XP_TRUE ) ) {
                if ( ds->start.obj == OBJ_TRAY ) {
                    /* moveTileToBoard flips its inputs */
                    (void)moveTileToBoard( board, ds->cur.u.board.col, 
                                           ds->cur.u.board.row,
                                           ds->start.u.tray.index, EMPTY_TILE );
                } else if ( ds->start.obj == OBJ_BOARD ) {
                    XP_U16 mod_curc, mod_curr;
                    flipIf( board, ds->cur.u.board.col, ds->cur.u.board.row,
                            &mod_curc, &mod_curr );
                    model_moveTileOnBoard( board->model, board->selPlayer, 
                                           mod_startc, mod_startr, mod_curc, 
                                           mod_curr );
                    /* inval points tile in case score changed */
                    board_invalTrayTiles( board, 1 << (MAX_TRAY_TILES-1) );
                }
            }
        } else {
            /* We're returning it to start, so will be re-inserted in tray */
            if ( OBJ_TRAY == ds->start.obj ) {
                invalTrayTilesAbove( board, ds->start.u.tray.index );
            }
        }

        /* These may change appearance, e.g. from big tile to dropped cell. */
        invalDragObj( board, &ds->cur );
        invalDragObj( board, &ds->start );
    }
    ds->dragInProgress = XP_FALSE;

    return XP_TRUE;
} /* dragDropEnd */

XP_Bool
dragDropGetBoardTile( const BoardCtxt* board, XP_U16* col, XP_U16* row )
{
    const DragState* ds = &board->dragState;
    XP_Bool found = !ds->dividerOnly && ds->cur.obj == OBJ_BOARD;
    if ( found ) {
        *col = ds->cur.u.board.col;
        *row = ds->cur.u.board.row;
    }
    return found;
}

XP_Bool
dragDropIsBeingDragged( const BoardCtxt* board, XP_U16 col, XP_U16 row, 
                        XP_Bool* isOrigin )
{
    const DragState* ds = &board->dragState;
    XP_Bool result = dragDropInProgress( board ) && !ds->dividerOnly
        && ds->cur.obj == OBJ_BOARD;
    if ( result ) {
        const DragState* ds = &board->dragState;
        if ( (ds->cur.obj == OBJ_BOARD) && (ds->cur.u.board.col == col)
             && (ds->cur.u.board.row == row) ) {
            *isOrigin = XP_FALSE;
        } else if ( (ds->start.obj == OBJ_BOARD)
                    && (ds->start.u.board.col == col)
             && (ds->start.u.board.row == row) ) {
            *isOrigin = XP_TRUE;
        } else {
            result = XP_FALSE;
        }

    }
    return result;
}

void
dragDropGetTrayChanges( const BoardCtxt* board, XP_U16* rmvdIndx, 
                        XP_U16* addedIndx )
{
    const DragState* ds = &board->dragState;
    *addedIndx = *rmvdIndx = MAX_TRAY_TILES; /* too big means ignore me */
    if ( dragDropInProgress( board ) && !ds->dividerOnly ) {
        if ( OBJ_TRAY == ds->start.obj ) {
            *rmvdIndx = ds->start.u.tray.index;
        }
        if ( OBJ_TRAY == ds->cur.obj ) {
            *addedIndx = ds->cur.u.tray.index;
        }
    }
}

XP_Bool
dragDropIsDividerDrag( const BoardCtxt* board )
{
    return dragDropInProgress( board ) && board->dragState.dividerOnly;
}

void 
dragDropTileInfo( const BoardCtxt* board, Tile* tile, XP_Bool* isBlank )
{
    const DragState* ds = &board->dragState;
    XP_ASSERT( dragDropInProgress( board ) );
    XP_ASSERT( !ds->dividerOnly );
    XP_ASSERT ( OBJ_BOARD == ds->start.obj || OBJ_TRAY == ds->start.obj );
    *tile = ds->tile;
    *isBlank = ds->isBlank;
}

static XP_Bool
dragDropContinueImpl( BoardCtxt* board, XP_U16 xx, XP_U16 yy,
                      BoardObjectType* onWhichP )
{
    XP_Bool moving = XP_FALSE;
    DragObjInfo newInfo;
    DragState* ds = &board->dragState;

    if ( !pointOnSomething( board, xx, yy, &newInfo.obj ) ) {
        newInfo.obj = OBJ_NONE;
    }
    *onWhichP = newInfo.obj;

    if ( ds->dividerOnly ) {
        if ( OBJ_TRAY == newInfo.obj ) {
            XP_U16 newloc;
            XP_U16 scale = board->trayScaleH;
            xx -= board->trayBounds.left;
            newloc = xx / scale;
            if ( (xx % scale) > (scale/2)) {
                ++newloc;
            }
            moving = dividerMoved( board, newloc );
        }
    } else {
        if ( newInfo.obj == OBJ_BOARD ) {
            (void)coordToCell( board, xx, yy, &newInfo.u.board.col, 
                               &newInfo.u.board.row );
            moving = (OBJ_TRAY == ds->cur.obj)
                || (newInfo.u.board.col != ds->cur.u.board.col)
                || (newInfo.u.board.row != ds->cur.u.board.row);
        } else if ( newInfo.obj == OBJ_TRAY ) {
            XP_Bool onDivider;
            XP_S16 index = pointToTileIndex( board, xx, yy, &onDivider );
            if ( !onDivider ) {
                if ( index < 0 ) { /* negative means onto empty part of tray.
                                      Force left. */
                    index = model_getNumTilesInTray( board->model, 
                                                     board->selPlayer );
                    if ( OBJ_TRAY == ds->start.obj ) {
                        --index; /* dragging right into space */
                    }
                }
                moving = (OBJ_BOARD == ds->cur.obj)
                    || (index != ds->cur.u.tray.index);
                if ( moving ) {
                    newInfo.u.tray.index = index;
                }
            }
        }

        if ( moving ) {
            invalDragObjRange( board, &ds->cur, &newInfo );
            XP_MEMCPY( &ds->cur, &newInfo, sizeof(ds->cur) );
        }
    }

    if ( moving ) {
        ds->didMove = XP_TRUE;
    }

    return moving;
} /* dragDropContinueImpl */

#ifdef CPLUS
}
#endif
