/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2000 by Eric House (fixin@peak.org).  All rights reserved.
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

#include "boardp.h"
#include "engine.h"

#ifdef CPLUS
extern "C" {
#endif

/****************************** prototypes ******************************/
static XP_Bool startDividerDrag( BoardCtxt* board );
static XP_Bool startTileDrag( BoardCtxt* board, XP_U8 startIndex );
static void figureDividerRect( BoardCtxt* board, XP_Rect* rect );
static void drawPendingScore( BoardCtxt* board );
static void invalTrayTilesBetween( BoardCtxt* board, XP_U16 tileIndex1, 
                                   XP_U16 tileIndex2 );
static XP_Bool endTileDragIndex( BoardCtxt* board, TileBit last );

static XP_S16
trayLocToIndex( BoardCtxt* board, XP_U16 loc )
{
    if ( loc >= model_getNumTilesInTray( board->model, 
                                         board->selPlayer ) ) {
        loc *= -1;
        /* (0 * -1) is still 0, so reduce by 1.  Will need to adjust
           below.  NOTE: this is something of a hack.*/
        --loc;
    }
    return loc;
} /* trayLocToIndex */

static XP_S16
pointToTileIndex( BoardCtxt* board, XP_U16 x, XP_U16 y, XP_Bool* onDividerP )
{
    XP_S16 result = -1;		/* not on a tile */
    XP_Rect divider;
    XP_Rect biggerRect;
    XP_Bool onDivider;

    figureDividerRect( board, &divider );

    /* The divider rect is narrower and kinda hard to tap on.  Let's expand
       it just for this test */
    biggerRect = divider;
    biggerRect.left -= 2;
    biggerRect.width += 4;
    onDivider = rectContainsPt( &biggerRect, x, y );

    if ( !onDivider ) {
        if ( x > divider.left ) {
            XP_ASSERT( divider.width == board->dividerWidth );
            x -= divider.width;
        }

        XP_ASSERT( x >= board->trayBounds.left );
        x -= board->trayBounds.left;
        result = x / board->trayScaleH;

        result = trayLocToIndex( board, result );
    }

    if ( onDividerP != NULL ) {
        *onDividerP = onDivider;
    }

    return result;
} /* pointToTileIndex */

static void
figureTrayTileRect( BoardCtxt* board, XP_U16 index, XP_Rect* rect )
{
    rect->left = board->trayBounds.left + (index * board->trayScaleH);
    rect->top = board->trayBounds.top/*  + 1 */;

    rect->width = board->trayScaleH;
    rect->height = board->trayScaleV;

    if ( board->dividerLoc[board->selPlayer] <= index ) {
        rect->left += board->dividerWidth;
    }
} /* figureTileRect */

void 
drawTray( BoardCtxt* board, XP_Bool focussed )
{
    XP_Rect tileRect;
    short i;

    if ( (board->trayInvalBits != 0) || board->dividerInvalid ) {
        XP_S16 turn = board->selPlayer;

        if ( draw_trayBegin( board->draw, &board->trayBounds, turn,
                             focussed ) ) {
            DictionaryCtxt* dictionary = model_getDictionary( board->model );

            if ( board->eraseTray ) {
                draw_clearRect( board->draw, &board->trayBounds );
                board->eraseTray = XP_FALSE;
            }

            if ( (board->trayVisState != TRAY_HIDDEN) && dictionary != NULL ) {
                XP_Bool showFaces = board->trayVisState == TRAY_REVEALED;

                if ( turn >= 0 ) {
                    XP_U16 numInTray = showFaces?
                        model_getNumTilesInTray( board->model, turn ):
                        model_getNumTilesTotal( board->model, turn );

                    /* draw in reverse order so drawing happens after
                       erasing */
                    for ( i = MAX_TRAY_TILES - 1; i >= 0; --i ) {

                        if ( (board->trayInvalBits & (1 << i)) == 0 ) {
                            continue;
                        }

                        figureTrayTileRect( board, i, &tileRect );

                        if ( i >= numInTray/*  && showFace */ ) {
                            draw_clearRect( board->draw, &tileRect );
                        } else if ( showFaces ) {
                            XP_UCHAR buf[4];
                            XP_Bitmap bitmap = NULL;
                            XP_UCHAR* textP = (XP_UCHAR*)NULL;
                            XP_U8 flags = board->traySelBits[turn];
                            XP_Bool highlighted = (flags & (1<<i)) != 0;
                            Tile tile = model_getPlayerTile( board->model, 
                                                             turn, i );
                            XP_S16 value;

                            if ( dict_faceIsBitmap( dictionary, tile ) ) {
                                bitmap = dict_getFaceBitmap( dictionary, tile, 
                                                             XP_TRUE );
                            } else {
                                textP = buf;
                                dict_tilesToString( dictionary, &tile, 1, 
                                                    textP );
                            }
                            value = dict_getTileValue( dictionary, tile );

                            draw_drawTile( board->draw, &tileRect, textP, 
                                           bitmap, value, highlighted );
                        } else {
                            draw_drawTileBack( board->draw, &tileRect );
                        }
                    }
                }

                if ( (board->dividerWidth > 0) && board->dividerInvalid ) {
                    XP_Rect divider;
                    figureDividerRect( board, &divider );
                    draw_drawTrayDivider( board->draw, &divider, 
                                          board->divDragState.dragInProgress );
                    board->dividerInvalid = XP_FALSE;
                }

#ifdef KEYBOARD_NAV
                if ( showFaces ) {
                    TileBit cursorLoc = board->trayCursorLoc[turn];
                    if ( !!cursorLoc ) {
                        XP_U16 index = indexForBits( cursorLoc );
                        figureTrayTileRect( board, index, &tileRect );
                        draw_drawTrayCursor( board->draw, &tileRect );
                    }
                }
#endif
            }

            draw_trayFinished(board->draw);

            board->trayInvalBits = 0;
        }
    }

    drawPendingScore( board );
} /* drawTray */

static void
drawPendingScore( BoardCtxt* board )
{
    /* Draw the pending score down in the last tray's rect */
    XP_U16 selPlayer = board->selPlayer;
    if ( board->trayVisState == TRAY_REVEALED ) {
        XP_U16 tilesInTray = model_getNumTilesInTray( board->model, selPlayer );
        if ( tilesInTray < MAX_TRAY_TILES ) {

            XP_S16 turnScore = 0;
            XP_Rect lastTileR;

            (void)getCurrentMoveScoreIfLegal( board->model, selPlayer,
                                              (XWStreamCtxt*)NULL, &turnScore );
            figureTrayTileRect( board, MAX_TRAY_TILES-1, &lastTileR );
            draw_score_pendingScore( board->draw, &lastTileR, turnScore, 
                                     selPlayer );
        }
    }
} /* drawPendingScore */

#ifdef DEBUG
static XP_U16
countSelectedTiles( XP_U8 ti )
{
    XP_U16 result = 0;

    while ( ti != 0 ) {
        ++result;
        ti &= ti-1;
    }
    return result;
} /* countSelectedTiles */
#endif

static void
figureDividerRect( BoardCtxt* board, XP_Rect* rect )
{
    figureTrayTileRect( board, board->dividerLoc[board->selPlayer], rect );
    rect->left -= board->dividerWidth;
    rect->width = board->dividerWidth;
} /* figureDividerRect */

void
invalTilesUnderRect( BoardCtxt* board, XP_Rect* rect )
{
    XP_U16 i;
    XP_Rect locRect;

    for ( i = 0; i < MAX_TRAY_TILES; ++i ) {
        figureTrayTileRect( board, i, &locRect );
        if ( rectsIntersect( rect, &locRect ) ) {
            board_invalTrayTiles( board, (TileBit)(1 << i) );
        }
    }

    figureDividerRect( board, &locRect );
    if ( rectsIntersect( rect, &locRect ) ) {
        board->dividerInvalid = XP_TRUE;
    }
} /* invalTilesUnderRect */

static XP_Bool
handleTrayDuringTrade( BoardCtxt* board, XP_S16 index )
{
    TileBit bits;

    XP_ASSERT( index >= 0 );

    bits = 1 << index;
    board->traySelBits[board->selPlayer] ^= bits;
    board_invalTrayTiles( board, bits );
    return XP_TRUE;
} /* handleTrayDuringTrade */

static XP_Bool
handleActionInTray( BoardCtxt* board, XP_S16 index, XP_Bool onDivider,
                    XP_Bool waitPenUp )
{
    XP_Bool result = XP_FALSE;
    XP_U16 selPlayer = board->selPlayer;

    if ( onDivider ) {
        result = startDividerDrag( board );
    } else if ( board->tradeInProgress[selPlayer]
                /*  && MY_TURN(board) */ ) {
        if ( index >= 0 ) {
            result = handleTrayDuringTrade( board, index );
        }
    } else if ( index >= 0 ) {
        TileBit newIndex = 1 << index;
        BoardArrow* arrow = &board->boardArrow[selPlayer];
	    
        if ( !arrow->visible ) {
            XP_U8 selFlags = board->traySelBits[selPlayer];
            /* Tap on selected tile unselects.  If we don't do this,
               then there's no way to unselect and so no way to turn
               off the placement arrow */
            if ( !waitPenUp && newIndex == selFlags ) {
                board_invalTrayTiles( board, selFlags );
                selFlags = NO_TILES;
                board->traySelBits[selPlayer] = selFlags;
                result = XP_TRUE;
            } else {
                result = startTileDrag( board, newIndex );
                if ( !waitPenUp ) {
                    /* key interface means pen up and down happen in the same
                       event.  No dragging. */
                    result = endTileDragIndex( board, newIndex ) || result;
                }
            }
        }
    }
    return result;
} /* handleActionInTray */

XP_Bool
handlePenDownInTray( BoardCtxt* board, XP_U16 x, XP_U16 y )
{
    XP_Bool onDivider = XP_FALSE;
    XP_S16 index = pointToTileIndex( board, x, y, &onDivider );
   
    return handleActionInTray( board, index, onDivider, XP_TRUE );
} /* handlePenDownInTray */

static XP_Bool
handlePenUpTrayInt( BoardCtxt* board, XP_S16 index )
{
    XP_Bool result = XP_FALSE;

    if ( index >= 0 ) {
        XP_U16 selPlayer = board->selPlayer;
        BoardArrow* arrow = &board->boardArrow[selPlayer];
	    
        if ( arrow->visible ) {
            result = moveTileToArrowLoc( board, (XP_U8)index );
        }
    } else if ( index == -(MAX_TRAY_TILES) ) { /* pending score tile */
        result = board_commitTurn( board );
    } else if ( index < 0 ) {		/* other empty area */
        /* it better be true */
        (void)board_replaceTiles( board );
        result = XP_TRUE;
    }

    return result;
} /* handlePenUpTray */

XP_Bool
handlePenUpTray( BoardCtxt* board, XP_U16 x, XP_U16 y )
{
    XP_Bool ignore;
    XP_S16 index = pointToTileIndex( board, x, y, &ignore );
    return handlePenUpTrayInt( board, index );
} /* handlePenUpTray */

static XP_Bool
startTileDrag( BoardCtxt* board, TileBit startBit/* , XP_U16 x, XP_U16 y */ )
{
    XP_Bool result = XP_FALSE;
    XP_U16 turn = board->selPlayer;
    XP_U8 startSel = board->traySelBits[turn];
    TileDragState* state = &board->tileDragState;

    XP_ASSERT( countSelectedTiles( startBit ) == 1 );
    XP_ASSERT( !state->dragInProgress );

    state->wasHilited = startSel == startBit;
    state->selectionAtStart = startSel;
    state->movePending = XP_TRUE;

    state->dragInProgress = XP_TRUE;
    state->prevIndex = board->traySelBits[turn] = startBit;

    if ( !state->wasHilited ) {
        board_invalTrayTiles( board, (TileBit)(startBit | startSel) );
        result = XP_TRUE;
    }
    return result;
} /* startTileDrag */

static void
moveTileInTray( BoardCtxt* board, TileBit prevTile, TileBit newTile )
{
    XP_S16 selPlayer = board->selPlayer;
    ModelCtxt* model = board->model;
    XP_U16 moveTo = indexForBits( prevTile );
    XP_U16 moveFrom = indexForBits( newTile );
    Tile tile;
    XP_U16 dividerLoc;

    tile = model_removePlayerTile( model, selPlayer, moveFrom );
    model_addPlayerTile( model, selPlayer, moveTo, tile );
    
    dividerLoc = board->dividerLoc[selPlayer];
    if ( moveTo < dividerLoc || moveFrom < dividerLoc ) {
        server_resetEngine( board->server, selPlayer );
    }
} /* moveTileInTray */

TileBit
continueTileDrag( BoardCtxt* board, XP_U16 x, XP_U16 y )
{
    TileDragState* state = &board->tileDragState;
    TileBit overTile = 0;
    XP_S16 index = pointToTileIndex( board, x, y, (XP_Bool*)NULL );

    if ( index >= 0 ) {

        overTile = 1 << index;

        if ( overTile != state->prevIndex ) {

            moveTileInTray( board, overTile, state->prevIndex );

            state->movePending = XP_FALSE;
            state->wasHilited = XP_FALSE; // so we won't deselect
            state->prevIndex = board->traySelBits[board->selPlayer] = overTile;
        }
    }
    return overTile;
} /* continueTileDrag */

XP_U16 
indexForBits( XP_U8 bits )
{
    XP_U16 result = 0;
    XP_U16 mask = 1;

    XP_ASSERT( bits != 0 );	/* otherwise loops forever */
    
    while ( (mask & bits) == 0 ) {
        ++result;
        mask <<= 1;
    }
    return result;
} /* indexForBits */

static XP_Bool
endTileDragIndex( BoardCtxt* board, TileBit last )
{
    XP_Bool result = XP_FALSE;
    XP_U16 selPlayer = board->selPlayer;

    TileDragState* state = &board->tileDragState;

    if ( state->movePending ) { /* no drag took place */
	
        if ( state->wasHilited ) {  /* if the user just clicked; deselect */
            board_invalTrayTiles( board, state->selectionAtStart );
            board->traySelBits[selPlayer] = NO_TILES;
            result = XP_TRUE;
        } else if ( (last > 0)
                    && !board->boardArrow[selPlayer].visible
                    && (state->selectionAtStart != NO_TILES ) ) {

            if ( model_getCurrentMoveCount( board->model, selPlayer) == 0 ) {
                moveTileInTray( board, last, state->selectionAtStart );
                board->traySelBits[selPlayer] = NO_TILES;
            } else {
                board_invalTrayTiles( 
                     board, 
                     (TileBit)(state->selectionAtStart|last) );
                board->traySelBits[selPlayer] = last;
            }
            result = XP_TRUE;
        }
    } else {
        board_invalTrayTiles( board, state->prevIndex );
        board->traySelBits[selPlayer] = NO_TILES;
        result = XP_TRUE;
    }

    state->dragInProgress = XP_FALSE;
    return result;
} /* endTileDragIndex */

XP_Bool
endTileDrag( BoardCtxt* board, XP_U16 x, XP_U16 y )
{
    TileBit newTile = continueTileDrag( board, x, y );
    return endTileDragIndex( board, newTile );
} /* endTileDrag */

static XP_Bool
startDividerDrag( BoardCtxt* board )
{
    board->divDragState.dragInProgress = XP_TRUE;
    board->dividerInvalid = XP_TRUE;
    return XP_TRUE;
} /* startDividerDrag */

static void
dividerMoved( BoardCtxt* board, XP_U8 newLoc )
{
    XP_U8 oldLoc = board->dividerLoc[board->selPlayer];
    board->dividerLoc[board->selPlayer] = newLoc;

    /* This divider's index corresponds to the tile it's to the left of, and
       there's no need to invalidate any tiles to the left of the uppermore
       divider position. */
    if ( oldLoc > newLoc ) {
        --oldLoc;
    } else {
        --newLoc;
    }
    invalTrayTilesBetween( board, newLoc, oldLoc );

    board->dividerInvalid = XP_TRUE;
    /* changed number of available tiles */
    board_resetEngine( board );
} /* dividerMoved */

XP_Bool
continueDividerDrag( BoardCtxt* board, XP_U16 x, XP_U16 y )
{
    XP_U8 newOffset;
    XP_U16 trayScale = board->trayScaleH;
    XP_Bool result = XP_FALSE;

    XP_ASSERT( board->divDragState.dragInProgress );

    /* Pen might have been dragged out of the tray */
    if ( rectContainsPt( &board->trayBounds, x, y ) ) {
        x -= board->trayBounds.left;
        newOffset = x / trayScale;
        if ( (x % trayScale) > (trayScale/2) ) {
            ++newOffset;
        }

        result = newOffset != board->dividerLoc[board->selPlayer];
        if ( result ) {
            dividerMoved( board, newOffset );
        }
    }
    return result;
} /* continueDividerDrag */

XP_Bool
endDividerDrag( BoardCtxt* board, XP_U16 x, XP_U16 y )
{
    XP_Bool result = XP_TRUE;	/* b/c hilited state looks different */
    (void)continueDividerDrag( board, x, y );
    board->dividerInvalid = XP_TRUE;
    board->divDragState.dragInProgress = XP_FALSE;
    return result;
} /* endDividerDrag */

void
board_invalTrayTiles( BoardCtxt* board, TileBit what )
{
    board->trayInvalBits |= what;
} /* invalTrayTiles */

static void
invalTrayTilesBetween( BoardCtxt* board, XP_U16 tileIndex1, 
                       XP_U16 tileIndex2 )
{
    TileBit bits = 0;

    if ( tileIndex1 > tileIndex2 ) {
        XP_U16 tmp = tileIndex1;
        tileIndex1 = tileIndex2;
        tileIndex2 = tmp;
    }

    while ( tileIndex1 <= tileIndex2 ) {
        bits |= (1 << tileIndex1);
        ++tileIndex1;
    }
    board_invalTrayTiles( board, bits );
} /* invalTrayTilesBetween */

static void
juggleTiles( Tile* tiles, XP_U16 nTiles )
{
    Tile newTiles[MAX_TRAY_TILES];

    XP_ASSERT( nTiles <= MAX_TRAY_TILES );
    XP_MEMCPY( newTiles, tiles, nTiles * sizeof(newTiles[0]) );

    /* Pull out a tile at random, and swap the top tile down into its place so
       the array doesn't get sparse. */
    while ( nTiles > 0 ) {
        XP_U16 rIndex = ((XP_U16)XP_RANDOM()) % nTiles;
        *tiles++ = newTiles[rIndex];
        newTiles[rIndex] = newTiles[--nTiles];
    }

} /* juggleTiles */

XP_Bool
board_juggleTray( BoardCtxt* board )
{
    XP_Bool result = XP_FALSE;
    XP_S16 turn = board->selPlayer;
    

    if ( checkRevealTray( board ) ) {
        XP_S16 nTiles;
        XP_U16 dividerLoc = board->dividerLoc[board->selPlayer];
        ModelCtxt* model = board->model;

        nTiles = model_getNumTilesInTray( model, turn ) - dividerLoc;
        if ( nTiles > 1 ) {
            XP_S16 i;
            Tile tmpT[MAX_TRAY_TILES];
            Tile newT[MAX_TRAY_TILES];

            /* create unique indices to be juggled; then juggle 'em until
               changed. */
            for ( i = 0; i < nTiles; ++i ) {
                tmpT[i] = newT[i] = (Tile)i;
            }
            do {
                juggleTiles( newT, nTiles );
            } while ( XP_MEMCMP( newT, tmpT, nTiles * sizeof(newT[0]) ) == 0 );

            /* save copies of the tiles in juggled order */
            for ( i = 0; i < nTiles; ++i ) {
                tmpT[i] = model_getPlayerTile( model, turn, 
                                               dividerLoc + newT[i] );
            }

            /* delete tiles off right end; put juggled ones back on the other */
            for ( i = nTiles - 1; i >= 0; --i ) {
                (void)model_removePlayerTile( model, turn, -1 );
                model_addPlayerTile( model, turn, dividerLoc, tmpT[i] );
            }
            board->traySelBits[turn] = 0;
            result = XP_TRUE;
        }
    }
    return result;
} /* board_juggleTray */

#ifdef KEYBOARD_NAV
XP_Bool
tray_moveCursor( BoardCtxt* board, XP_Key cursorKey )
{
    XP_U16 selPlayer = board->selPlayer;
    XP_U16 numTrayTiles = model_getNumTilesInTray( board->model, 
                                                   selPlayer );
    XP_U16 pos;
    TileBit newSel;
    TileBit oldSel = board->trayCursorLoc[selPlayer];

    numTrayTiles = MAX_TRAY_TILES;

    if ( oldSel == 0 ) {
        if ( cursorKey == XP_CURSOR_KEY_LEFT ) {
            newSel = 1 << (numTrayTiles - 1);
        } else {
            XP_ASSERT( cursorKey == XP_CURSOR_KEY_RIGHT );
            newSel = 1;
        }
    } else {
        pos = indexForBits( oldSel );

        pos += numTrayTiles;  /* add what we'll mod by below: makes circular */
        if ( cursorKey == XP_CURSOR_KEY_LEFT ) {
            --pos;
        } else if ( cursorKey == XP_CURSOR_KEY_RIGHT ) {
            ++pos;
        }

        newSel = 1 << (pos % numTrayTiles);
    }
    board->trayCursorLoc[selPlayer] = newSel;
    board_invalTrayTiles( board, newSel | oldSel );
    return XP_TRUE;
} /* tray_moveCursor */

XP_Bool
tray_keyAction( BoardCtxt* board )
{
    TileBit cursor = board->trayCursorLoc[board->selPlayer];
    XP_Bool result;
    if ( !!cursor ) {
        XP_S16 index = trayLocToIndex( board, indexForBits( cursor ) );
        result = handleActionInTray( board, index, XP_FALSE, XP_FALSE )
            || handlePenUpTrayInt( board, index );
    } else {
        result = XP_FALSE;
    }

    return result;
} /* tray_keyAction */
#endif

#if defined FOR_GREMLINS || defined KEYBOARD_NAV
XP_Bool
board_moveDivider( BoardCtxt* board, XP_Bool right )
{
    XP_Bool result = board->trayVisState == TRAY_REVEALED;
    if ( result ) {
        XP_U8 loc = board->dividerLoc[board->selPlayer];
        loc += MAX_TRAY_TILES + 1;
        loc += right? 1:-1;
        loc %= MAX_TRAY_TILES + 1;

        dividerMoved( board, loc );
    }
    return result;
} /* board_moveDivider */
#endif

#ifdef CPLUS
}
#endif
