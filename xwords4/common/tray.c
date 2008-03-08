/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2008 by Eric House (xwords@eehouse.org).  All rights reserved.
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
#include "dragdrpp.h"
#include "engine.h"
#include "draw.h"
#include "strutils.h"

#ifdef CPLUS
extern "C" {
#endif

/****************************** prototypes ******************************/
static void figureDividerRect( BoardCtxt* board, XP_Rect* rect );
static void drawPendingScore( BoardCtxt* board, XP_Bool hasCursor );
static XP_U16 countTilesToShow( BoardCtxt* board );

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

XP_S16
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

void
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

/* When drawing tray mid-drag:
 *
 * Rule is not to touch the model.  
 *
 * Cases: Tile's been dragged into tray (but not yet dropped.); tile's been
 * dragged out of tray (but not yet dropped); and tile's been dragged within
 * tray.  More's the point, there's an added tile and a removed one.  We draw
 * the added tile extra, and skip the removed one.
 *
 * We're walking two arrays at once, backwards.  The first is the tile rects
 * themselves.  If the dirty bit is set, something must get drawn.  The second
 * is the model's view of tiles augmented by drag-and-drop.  D-n-d may have
 * removed a tile from the tray (for drawing purposes only), have added one,
 * or both (drag-within-tray case).  Since a drag lasts only until pen-up,
 * there's never more than one tile involved.  Adjustment is never by more
 * than one.
 *
 * So while one counter (i) walks the array of rects, we can't use it
 * unmodified to fetch from the model.  Instead we increment or decrement it
 * based on the drag state.
 */

void 
drawTray( BoardCtxt* board )
{
    XP_Rect tileRect;

    if ( (board->trayInvalBits != 0) || board->dividerInvalid ) {
        XP_S16 turn = board->selPlayer;

        if ( draw_trayBegin( board->draw, &board->trayBounds, turn,
                             dfsFor( board, OBJ_TRAY ) ) ) {
            DictionaryCtxt* dictionary = model_getDictionary( board->model );
            XP_S16 cursorBits = 0;
#ifdef KEYBOARD_NAV
            if ( board->focussed == OBJ_TRAY ) {
                if ( board->focusHasDived ) {
                    cursorBits = 1 << board->trayCursorLoc[turn];
                } else {
                    cursorBits = ALLTILES;
                }
            }
#endif

            if ( (board->trayVisState != TRAY_HIDDEN) && dictionary != NULL ) {
                XP_Bool showFaces = board->trayVisState == TRAY_REVEALED;
                Tile blank = dict_getBlankTile( dictionary );

                if ( turn >= 0 ) {
                    XP_S16 i; /* which tile slot are we drawing in */
                    XP_U16 ddAddedIndx, ddRmvdIndx;
                    XP_U16 numInTray = countTilesToShow( board );
                    XP_Bool isBlank;
                    XP_Bool isADrag = dragDropInProgress( board );
                    
                    dragDropGetTrayChanges( board, &ddRmvdIndx, &ddAddedIndx );

                    /* draw in reverse order so drawing happens after
                       erasing */
                    for ( i = MAX_TRAY_TILES - 1; 
                          i >= 0; --i ) {
                        CellFlags flags = CELL_NONE;
                        XP_U16 mask = 1 << i;

                        if ( (board->trayInvalBits & mask) == 0 ) {
                            continue;
                        }
#ifdef KEYBOARD_NAV
                        if ( (cursorBits & mask) != 0 ) {
                            flags |= CELL_ISCURSOR;
                        }
#endif
                        figureTrayTileRect( board, i, &tileRect );

                        if ( i >= numInTray ) {
                            draw_drawTile( board->draw, &tileRect, NULL,
                                           NULL, -1, flags | CELL_ISEMPTY );
                        } else if ( showFaces ) {
                            XP_UCHAR buf[4];
                            XP_Bitmap bitmap = NULL;
                            XP_UCHAR* textP = (XP_UCHAR*)NULL;
                            XP_U8 traySelBits = board->traySelBits[turn];
                            XP_S16 value;
                            Tile tile;

                            if ( ddAddedIndx == i ) {
                                dragDropTileInfo( board, &tile, &isBlank );
                            } else {
                                XP_U16 modIndex = i;
                                if ( ddAddedIndx < i ) {
                                    --modIndex;
                                }
                                /* while we're right of the removal area,
                                   draw the one from the right to cover. */
                                if ( ddRmvdIndx <= modIndex /*slotIndx*/ ) {
                                    ++modIndex;
                                }
                                tile = model_getPlayerTile( board->model, 
                                                            turn, modIndex );
                                isBlank = tile == blank;
                            }

                            textP = getTileDrawInfo( board, tile, isBlank,
                                                     &bitmap, &value,
                                                     buf, sizeof(buf) );
                            if ( board->hideValsInTray 
                                 && !board->showCellValues ) {
                                value = -1;
                            }

                            if ( isADrag ) {
                                if ( ddAddedIndx == i ) {
                                    flags |= CELL_HIGHLIGHT;
                                }
                            } else if ( (traySelBits & (1<<i)) != 0 ) {
                                flags |= CELL_HIGHLIGHT;
                            }
                            if ( isBlank ) {
                                flags |= CELL_ISBLANK;
                            }

                            draw_drawTile( board->draw, &tileRect, textP, 
                                           bitmap, value, flags );
                        } else {
                            draw_drawTileBack( board->draw, &tileRect, 
                                               flags );
                        }
                    }
                }

                if ( (board->dividerWidth > 0) && board->dividerInvalid ) {
                    XP_Rect divider;
                    figureDividerRect( board, &divider );
                    draw_drawTrayDivider( board->draw, &divider, 
                                          dragDropIsDividerDrag(board) );
                    board->dividerInvalid = XP_FALSE;
                }

                drawPendingScore( board, 
                                  (cursorBits & (1<<(MAX_TRAY_TILES-1))) != 0);
            }

            draw_objFinished( board->draw, OBJ_TRAY, &board->trayBounds,
                              dfsFor( board, OBJ_TRAY ) );

            board->trayInvalBits = 0;
        }
    }

} /* drawTray */

XP_UCHAR*
getTileDrawInfo( const BoardCtxt* board, Tile tile, XP_Bool isBlank,
                 XP_Bitmap* bitmap, XP_S16* value, XP_UCHAR* buf, XP_U16 len )
{
    XP_UCHAR* face = NULL;
    DictionaryCtxt* dict = model_getDictionary( board->model );
    if ( isBlank ) {
        tile = dict_getBlankTile( dict );
    }
    *value = dict_getTileValue( dict, tile );
    if ( dict_faceIsBitmap( dict, tile ) ) {
        *bitmap = dict_getFaceBitmap( dict, tile, XP_TRUE );
    } else {
        dict_tilesToString( dict, &tile, 1, buf, len );
        face = buf;
    }
    return face;
}

static XP_U16
countTilesToShow( BoardCtxt* board )
{
    XP_U16 numToShow;
    XP_S16 selPlayer = board->selPlayer;
    XP_U16 ddAddedIndx, ddRemovedIndx;

    XP_ASSERT( selPlayer >= 0 );
    if ( board->trayVisState == TRAY_REVEALED ) {
        numToShow = model_getNumTilesInTray( board->model, selPlayer );
    } else {
        numToShow = model_getNumTilesTotal( board->model, selPlayer );
    }

    dragDropGetTrayChanges( board, &ddRemovedIndx, &ddAddedIndx );
    if ( ddAddedIndx < MAX_TRAY_TILES ) {
        ++numToShow;
    }
    if ( ddRemovedIndx < MAX_TRAY_TILES ) {
        --numToShow;
    }

    XP_ASSERT( numToShow <= MAX_TRAY_TILES );
    return numToShow;
} /* countTilesToShow */

static void
drawPendingScore( BoardCtxt* board, XP_Bool hasCursor )
{
    /* Draw the pending score down in the last tray's rect */
    if ( countTilesToShow( board ) < MAX_TRAY_TILES ) {
        XP_U16 selPlayer = board->selPlayer;
        XP_S16 turnScore = 0;
        XP_Rect lastTileR;

        (void)getCurrentMoveScoreIfLegal( board->model, selPlayer,
                                          (XWStreamCtxt*)NULL, &turnScore );
        figureTrayTileRect( board, MAX_TRAY_TILES-1, &lastTileR );
        draw_score_pendingScore( board->draw, &lastTileR, turnScore, 
                                 selPlayer, 
                                 hasCursor?CELL_ISCURSOR:CELL_NONE );
    }
} /* drawPendingScore */

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
handleActionInTray( BoardCtxt* board, XP_S16 index, XP_Bool onDivider )
{
    XP_Bool result = XP_FALSE;
    XP_U16 selPlayer = board->selPlayer;

    if ( onDivider ) {
        /* do nothing */
    } else if ( board->tradeInProgress[selPlayer] ) {
        if ( index >= 0 ) {
            result = handleTrayDuringTrade( board, index );
        }
    } else if ( index >= 0 ) {
        result = moveTileToArrowLoc( board, (XP_U8)index );
        if ( !result ) {
            TileBit newBits = 1 << index;
            XP_U8 selBits = board->traySelBits[selPlayer];
            /* Tap on selected tile unselects.  If we don't do this,
               then there's no way to unselect and so no way to turn
               off the placement arrow */
            if ( newBits == selBits ) {
                board_invalTrayTiles( board, selBits );
                board->traySelBits[selPlayer] = NO_TILES;
            } else if ( selBits != 0 ) {
                XP_U16 selIndex = indexForBits( selBits );
                model_moveTileOnTray( board->model, board->selPlayer,
                                      selIndex, index );
                board->traySelBits[selPlayer] = NO_TILES;
            } else {
                 board_invalTrayTiles( board, newBits );
                 board->traySelBits[selPlayer] = newBits;
            }
            result = XP_TRUE;
        }
    } else if ( index == -(MAX_TRAY_TILES) ) { /* pending score tile */
        result = board_commitTurn( board );
    } else if ( index < 0 ) {		/* other empty area */
        /* it better be true */
        (void)board_replaceTiles( board );
        result = XP_TRUE;
    }
    return result;
} /* handleActionInTray */

XP_Bool
handlePenUpTray( BoardCtxt* board, XP_U16 x, XP_U16 y )
{
    XP_Bool onDivider;
    XP_S16 index = pointToTileIndex( board, x, y, &onDivider );
    return handleActionInTray( board, index, onDivider );
} /* handlePenUpTray */

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

XP_Bool
dividerMoved( BoardCtxt* board, XP_U8 newLoc )
{
    XP_U8 oldLoc = board->dividerLoc[board->selPlayer];
    XP_Bool moved = oldLoc != newLoc;
    if ( moved ) {
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
    }
    return moved;
} /* dividerMoved */

void
board_invalTrayTiles( BoardCtxt* board, TileBit what )
{
    board->trayInvalBits |= what;
} /* invalTrayTiles */

void
invalTrayTilesAbove( BoardCtxt* board, XP_U16 tileIndex )
{
    TileBit bits = 0;
    while ( tileIndex < MAX_TRAY_TILES ) {
        bits |= 1 << tileIndex++;
    }
    board_invalTrayTiles( board, bits );
}

void
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
            XP_U16 newT[MAX_TRAY_TILES];

            /* loop until there'll be change */
            while ( !randIntArray( newT, nTiles ) ) {
            }

            /* save copies of the tiles in juggled order */
            for ( i = 0; i < nTiles; ++i ) {
                tmpT[i] = model_getPlayerTile( model, turn, 
                                               (Tile)(dividerLoc + newT[i]) );
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
tray_moveCursor( BoardCtxt* board, XP_Key cursorKey, XP_Bool preflightOnly,
                 XP_Bool* pUp )
{
    XP_Bool draw = XP_FALSE;
    XP_Bool up = XP_FALSE;
    XP_U16 selPlayer = board->selPlayer;
    XP_S16 pos;
    TileBit what = 0;

    switch ( cursorKey ) {
    case XP_CURSOR_KEY_UP:
    case XP_CURSOR_KEY_DOWN:
        up = XP_TRUE;
        break;
        /* moving the divider needs to be hard to do accidentally since it
           confuses users when juggle and hint stop working.  But all things
           must be possible via keyboard on devices that don't have
           touchscreens.  Probably need a new keytype XP_CURSOR_KEY_ALTDOWN
           etc. */
    case XP_CURSOR_KEY_ALTRIGHT:
    case XP_CURSOR_KEY_ALTLEFT:
        draw = preflightOnly
            || board_moveDivider( board, cursorKey == XP_CURSOR_KEY_ALTRIGHT );
        break;
    case XP_CURSOR_KEY_RIGHT:
    case XP_CURSOR_KEY_LEFT:
        what = what | (1 << board->trayCursorLoc[selPlayer]);
        pos = board->trayCursorLoc[selPlayer];
        /* Loop in order to skip all empty tile slots but one */
        for ( ; ; ) {
            pos += (cursorKey == XP_CURSOR_KEY_RIGHT ? 1 : -1);
            if ( pos < 0 || pos >= MAX_TRAY_TILES ) {
                up = XP_TRUE;
            } else {
                /* Revisit this when able to never draw the cursor in a place
                   this won't allow it, e.g. when the tiles move after a
                   hint */
                if ( board->trayVisState == TRAY_REVEALED ) {
                    XP_U16 count = model_getNumTilesInTray( board->model,
                                                            selPlayer );
                    if ( (pos > count) && (pos < MAX_TRAY_TILES-1) ) {
                        continue;
                    }
                }
                if ( !preflightOnly ) {
                    board->trayCursorLoc[selPlayer] = pos;
                    what = what | (1 << pos);
                }
            }
            break;
        }
        if ( !preflightOnly ) {
            what = what | (1 << board->trayCursorLoc[selPlayer]);
            board_invalTrayTiles( board, what );
        }
        draw = XP_TRUE;
        break;
    default:
        draw = XP_FALSE;
        break;
    }

    *pUp = up;
    return draw;
} /* tray_moveCursor */

#endif /* KEYBOARD_NAV */

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

        (void)dividerMoved( board, loc );
    }
    return result;
} /* board_moveDivider */
#endif

#ifdef CPLUS
}
#endif
