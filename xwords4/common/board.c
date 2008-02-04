/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2007 by Eric House (xwords@eehouse.org).  All rights
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
#include "comms.h"		/* for CHANNEL_NONE */
#include "dictnry.h"
#include "draw.h"
#include "engine.h"
#include "util.h"
#include "mempool.h"		/* debug only */
#include "memstream.h"
#include "strutils.h"
#include "LocalizedStrIncludes.h"

#include "boardp.h"
#include "dbgutil.h"

#define bEND 0x62454e44

#ifdef CPLUS
extern "C" {
#endif

/****************************** prototypes ******************************/
static XP_Bool getCellRect( BoardCtxt* board, XP_U16 col, XP_U16 row, 
                            XP_Rect* rect);
static XP_Bool coordToCell( BoardCtxt* board, XP_U16 x, XP_U16 y, 
                            XP_U16* colP, XP_U16* rowP );
static XP_Bool drawCell( BoardCtxt* board, XP_U16 col, XP_U16 row, 
                         XP_Bool skipBlanks );
static void figureBoardRect( BoardCtxt* board );

static void drawBoard( BoardCtxt* board );
static void invalCell( BoardCtxt* board, XP_U16 col, XP_U16 row );
static void invalCellsUnderRect( BoardCtxt* board, XP_Rect* rect );

static XP_Bool moveTileToBoard( BoardCtxt* board, XP_U16 col, XP_U16 row, 
                                XP_U16 tileIndex, Tile blankFace );
static XP_Bool rectContainsRect( XP_Rect* rect1, XP_Rect* rect2 );
static void boardCellChanged( void* board, XP_U16 turn, XP_U16 col, 
                              XP_U16 row, XP_Bool added );
static void boardTileChanged( void* board, XP_U16 turn, TileBit bits );
static void boardTurnChanged( void* board );
static void boardGameOver( void* board );
static void setArrow( BoardCtxt* board, XP_U16 row, XP_U16 col );
static void setArrowFor( BoardCtxt* board, XP_U16 player, XP_U16 col, 
                         XP_U16 row );
static XP_Bool setArrowVisible( BoardCtxt* board, XP_Bool visible );

static XP_Bool cellOccupied( BoardCtxt* board, XP_U16 col, XP_U16 row, 
                             XP_Bool inclPending );
static void makeMiniWindowForTrade( BoardCtxt* board );
static void makeMiniWindowForText( BoardCtxt* board, const XP_UCHAR* text, 
                                   MiniWindowType winType );
static void invalTradeWindow( BoardCtxt* board, XP_S16 turn, XP_Bool redraw );
static void invalSelTradeWindow( BoardCtxt* board );
static void setTimerIf( BoardCtxt* board );
static void p_board_timerFired( void* closure, XWTimerReason why );

static XP_Bool replaceLastTile( BoardCtxt* board );
static XP_Bool setTrayVisState( BoardCtxt* board, XW_TrayVisState newState );
static XP_Bool advanceArrow( BoardCtxt* board );
static XP_Bool exitTradeMode( BoardCtxt* board );

static XP_Bool getArrow( BoardCtxt* board, XP_U16* col, XP_U16* row );
static XP_Bool setArrowVisibleFor( BoardCtxt* board, XP_U16 player, 
                                   XP_Bool visible );
static XP_Bool board_moveArrow( BoardCtxt* board, XP_Key cursorKey );
static void flipIf( const BoardCtxt* board, XP_U16 col, XP_U16 row, 
                    XP_U16* fCol, XP_U16* fRow );

#ifdef KEY_SUPPORT
static XP_Bool moveKeyTileToBoard( BoardCtxt* board, XP_Key cursorKey,
                                   XP_Bool* gotArrow );
#endif

#ifdef KEYBOARD_NAV
static XP_Bool board_moveCursor( BoardCtxt* board, XP_Key cursorKey, 
                                 XP_Bool preflightOnly, XP_Bool* up );
static XP_Bool invalFocusOwner( BoardCtxt* board );
#endif
#ifdef XWFEATURE_SEARCHLIMIT
static HintAtts figureHintAtts( BoardCtxt* board, XP_U16 col, XP_U16 row );
static void invalCurHintRect( BoardCtxt* board, XP_U16 player );
static void clearCurHintRect( BoardCtxt* board );

#else
# define figureHintAtts(b,c,r) HINT_BORDER_NONE
#endif

/*****************************************************************************
 *
 ****************************************************************************/
BoardCtxt*
board_make( MPFORMAL ModelCtxt* model, ServerCtxt* server, DrawCtx* draw, 
            XW_UtilCtxt* util )
{
    BoardCtxt* result = (BoardCtxt*)XP_MALLOC( mpool, sizeof( *result ) );
    XP_ASSERT( !!draw );
    XP_ASSERT( !!server );
    XP_ASSERT( !!util );
    XP_ASSERT( !!model );

    if ( result != NULL ) {
	
        XP_MEMSET( result, 0, sizeof( *result ) );

        MPASSIGN(result->mpool, mpool);

        result->model = model;
        result->server = server;

        result->draw = draw;
        result->util = util;
        result->gi = util->gameInfo;
        XP_ASSERT( !!result->gi );

        result->trayVisState = TRAY_HIDDEN;

        result->star_row = (XP_U16)(model_numRows(model) / 2);

        /* could just pass in invalCell.... PENDING(eeh) */
        model_setBoardListener( model, boardCellChanged, result );
        model_setTrayListener( model, boardTileChanged, result );
        server_setTurnChangeListener( server, boardTurnChanged, result );
        server_setGameOverListener( server, boardGameOver, result );

        setTimerIf( result );

#ifdef KEYBOARD_NAV     
        {
            /* set up some useful initial values */
            XP_U16 i;
            for ( i = 0; i < MAX_NUM_PLAYERS; ++i ) {
                result->trayCursorLoc[i] = 1;
                result->bdCursor[i].col = 5;
                result->bdCursor[i].row = 7;
            }
        }
#endif

    }
    return result;
} /* board_make */

void
board_destroy( BoardCtxt* board )
{
    XP_FREE( board->mpool, board );
} /* board_destroy */

BoardCtxt* 
board_makeFromStream( MPFORMAL XWStreamCtxt* stream, ModelCtxt* model,
                      ServerCtxt* server, DrawCtx* draw, XW_UtilCtxt* util,
                      XP_U16 nPlayers )
{
    BoardCtxt* board;
    XP_U16 i;
    XP_U16 version = stream_getVersion( stream );

    board = board_make( MPPARM(mpool) model, server, draw, util );

    /* This won't be enough for 'doze case: square with the SIP visible */
    board->yOffset = (XP_U16)stream_getBits( stream, 2 );
    board->isFlipped = (XP_Bool)stream_getBits( stream, 1 );
    board->gameOver = (XP_Bool)stream_getBits( stream, 1 );
    board->showColors = (XP_Bool)stream_getBits( stream, 1 );
    board->showCellValues = (XP_Bool)stream_getBits( stream, 1 );
#ifdef KEYBOARD_NAV
    if ( version >= STREAM_VERS_KEYNAV ) {
        board->focussed = (BoardObjectType)stream_getBits( stream, 2 );
        board->focusHasDived = (BoardObjectType)stream_getBits( stream, 1 );
        board->scoreCursorLoc = (BoardObjectType)stream_getBits( stream, 2 );
    }
#endif

    XP_ASSERT( !!server );

    for ( i = 0; i < nPlayers; ++i ) {
        BoardArrow* arrow = &board->boardArrow[i];
        arrow->col = (XP_U8)stream_getBits( stream, NUMCOLS_NBITS );
        arrow->row = (XP_U8)stream_getBits( stream, NUMCOLS_NBITS );
        arrow->vert = (XP_Bool)stream_getBits( stream, 1 );
        arrow->visible = (XP_Bool)stream_getBits( stream, 1 );

        board->dividerLoc[i] = (XP_U8)stream_getBits( stream, NTILES_NBITS );
        board->traySelBits[i] = (TileBit)stream_getBits( stream, 
                                                         MAX_TRAY_TILES );
        board->tradeInProgress[i] = (XP_Bool)stream_getBits( stream, 1 );
#ifdef KEYBOARD_NAV 
        if ( version >= STREAM_VERS_KEYNAV ) {
            board->bdCursor[i].col = stream_getBits( stream, 4 );
            board->bdCursor[i].row = stream_getBits( stream, 4 );
            board->trayCursorLoc[i] = stream_getBits( stream, 3 );
        }
#endif

#ifdef XWFEATURE_SEARCHLIMIT
        if ( version >= STREAM_VERS_41B4 ) {
            board->hasHintRect[i] = stream_getBits( stream, 1 );
        } else {
            board->hasHintRect[i] = XP_FALSE;
        }
        if ( board->hasHintRect[i] ) {
            board->limits[i].left = stream_getBits( stream, 4 );
            board->limits[i].top = stream_getBits( stream, 4 );
            board->limits[i].right = stream_getBits( stream, 4 );
            board->limits[i].bottom =  stream_getBits( stream, 4 );
        }
#endif

    }

    board->selPlayer = (XP_U8)stream_getBits( stream, PLAYERNUM_NBITS );
    board->trayVisState = (XW_TrayVisState)stream_getBits( stream, 2 );

    XP_ASSERT( stream_getU32( stream ) == bEND );
    return board;
} /* board_makeFromStream */

void
board_writeToStream( BoardCtxt* board, XWStreamCtxt* stream )
{
    XP_U16 nPlayers, i;
    
    stream_putBits( stream, 2, board->yOffset );
    stream_putBits( stream, 1, board->isFlipped );
    stream_putBits( stream, 1, board->gameOver );
    stream_putBits( stream, 1, board->showColors );
    stream_putBits( stream, 1, board->showCellValues );
#ifdef KEYBOARD_NAV
    stream_putBits( stream, 2, board->focussed );
    stream_putBits( stream, 1, board->focusHasDived );
    stream_putBits( stream, 2, board->scoreCursorLoc );
#endif

    XP_ASSERT( !!board->server );
    nPlayers = board->gi->nPlayers;

    for ( i = 0; i < nPlayers; ++i ) {
        BoardArrow* arrow = &board->boardArrow[i];
        stream_putBits( stream, NUMCOLS_NBITS, arrow->col );
        stream_putBits( stream, NUMCOLS_NBITS, arrow->row );
        stream_putBits( stream, 1, arrow->vert );
        stream_putBits( stream, 1, arrow->visible );

        stream_putBits( stream, NTILES_NBITS, board->dividerLoc[i] );
        stream_putBits( stream, MAX_TRAY_TILES, board->traySelBits[i] );
        stream_putBits( stream, 1, board->tradeInProgress[i] );
#ifdef KEYBOARD_NAV 
        stream_putBits( stream, 4, board->bdCursor[i].col );
        stream_putBits( stream, 4, board->bdCursor[i].row );
        stream_putBits( stream, 3, board->trayCursorLoc[i] );
#endif

#ifdef XWFEATURE_SEARCHLIMIT
        stream_putBits( stream, 1, board->hasHintRect[i] );
        if ( board->hasHintRect[i] ) {
            stream_putBits( stream, 4, board->limits[i].left );
            stream_putBits( stream, 4, board->limits[i].top );
            stream_putBits( stream, 4, board->limits[i].right );
            stream_putBits( stream, 4, board->limits[i].bottom );
        }
#endif
    }

    stream_putBits( stream, PLAYERNUM_NBITS, board->selPlayer );
    stream_putBits( stream, 2, board->trayVisState );

#ifdef DEBUG
    stream_putU32( stream, bEND );
#endif
} /* board_writeToStream */

void
board_reset( BoardCtxt* board )
{
    XP_U16 i;
    XW_TrayVisState newState;

    XP_ASSERT( !!board->model );

    /* This is appropriate for a new game *ONLY*.  reset */
    for ( i = 0; i < MAX_NUM_PLAYERS; ++i ) {
        board->traySelBits[i] = 0;
        board->tradeInProgress[i] = XP_FALSE;
        board->dividerLoc[i] = 0;
    }
    XP_MEMSET( &board->boardArrow, 0, sizeof(board->boardArrow) );
    board->gameOver = XP_FALSE;
    board->selPlayer = 0;
    board->star_row = (XP_U16)(model_numRows(board->model) / 2);

    newState = board->boardObscuresTray? TRAY_HIDDEN:TRAY_REVERSED;
    setTrayVisState( board, newState );

    board_invalAll( board );

    setTimerIf( board );
} /* board_reset */

void
board_setPos( BoardCtxt* board, XP_U16 left, XP_U16 top, 
              XP_Bool leftHanded )
{
    board->boardBounds.left = left;
    board->boardBounds.top = top;
    board->leftHanded = leftHanded;

    figureBoardRect( board );
} /* board_setPos */

void
board_setTimerLoc( BoardCtxt* board, 
		   XP_U16 timerLeft, XP_U16 timerTop,
		   XP_U16 timerWidth, XP_U16 timerHeight )
{
    board->timerBounds.left = timerLeft;
    board->timerBounds.top = timerTop;
    board->timerBounds.width = timerWidth;
    board->timerBounds.height = timerHeight;
} /* board_setTimerLoc */

void
board_setScale( BoardCtxt* board, XP_U16 hScale, XP_U16 vScale )
{
    if ( hScale != board->boardHScale || vScale != board->boardVScale ) {
        board->boardVScale = vScale;
        board->boardHScale = hScale;
        figureBoardRect( board );
        board_invalAll( board );
    }
} /* board_setScale */

void 
board_getScale( BoardCtxt* board, XP_U16* hScale, XP_U16* vScale )
{
    *hScale = board->boardHScale;
    *vScale = board->boardVScale;
} /* board_getScale */

XP_Bool
board_prefsChanged( BoardCtxt* board, CommonPrefs* cp )
{
    XP_Bool showArrowChanged;
    XP_Bool hideValChanged;

    showArrowChanged = cp->showBoardArrow == board->disableArrow;
    hideValChanged = cp->hideTileValues != board->hideValsInTray;

    board->disableArrow = !cp->showBoardArrow;
    board->hideValsInTray = cp->hideTileValues;

    if ( showArrowChanged ) {
        showArrowChanged = setArrowVisible( board, XP_FALSE );
    }

#ifdef XWFEATURE_SEARCHLIMIT
    if ( !board->gi->allowHintRect
         && board->hasHintRect[board->selPlayer] ) {

        EngineCtxt* engine = server_getEngineFor( board->server, 
                                                  board->selPlayer );
        if ( !!engine ) {
            engine_reset( engine );
        }

        clearCurHintRect( board );
    }
#endif

    return showArrowChanged || hideValChanged;
} /* board_prefsChanged */

XP_Bool
board_setYOffset( BoardCtxt* board, XP_U16 offset )
{
    XP_U16 oldOffset = board->yOffset;
    XP_Bool result = oldOffset != offset;

    if ( result ) {
        /* check if scrolling makes sense for this board in its current
           state. */
        XP_U16 visibleHeight = board->boardBounds.height;
        XP_U16 fullHeight = model_numRows(board->model) * board->boardVScale;
        result = visibleHeight < fullHeight;

        if ( result ) {
            invalSelTradeWindow( board );
            board->yOffset = offset;
            figureBoardRect( board );
            util_yOffsetChange( board->util, oldOffset, offset );
            invalSelTradeWindow( board );
            board->needsDrawing = XP_TRUE;
        }
    }

    return result;
} /* board_setYOffset */

XP_U16
board_getYOffset( BoardCtxt* board )
{
    return board->yOffset;
} /* board_getYOffset */

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
    BoardArrow* arrow = &board->boardArrow[board->selPlayer];
    invalCell( board, arrow->col, arrow->row );
} /* invalArrowCell */

static void
flipArrow( BoardCtxt* board )
{
    BoardArrow* arrow = &board->boardArrow[board->selPlayer];
    XP_U16 tmp = arrow->col;
    arrow->col = arrow->row;
    arrow->row = tmp;
    arrow->vert = !arrow->vert;
} /* flipArrow */

#ifdef KEYBOARD_NAV
static void
invalCursorCell( BoardCtxt* board )
{
    BdCursorLoc loc = board->bdCursor[board->selPlayer];
    invalCell( board, loc.col, loc.row );
} /* invalCursorCell */
#endif

static void
invalTradeWindow( BoardCtxt* board, XP_S16 turn, XP_Bool redraw )
{
    XP_ASSERT( turn >= 0 );

    if ( board->tradeInProgress[turn] ) {
        MiniWindowStuff* stuff = &board->miniWindowStuff[MINIWINDOW_TRADING];
        invalCellsUnderRect( board, &stuff->rect );
        if ( redraw ) {
            board->tradingMiniWindowInvalid = XP_TRUE;
        }
    }
} /* invalTradeWindow */

static void
invalSelTradeWindow( BoardCtxt* board )
{
    invalTradeWindow( board, board->selPlayer, 
                      board->trayVisState == TRAY_REVEALED );
} /* invalSelTradeWindow */

#if defined POINTER_SUPPORT || defined KEYBOARD_NAV
static void
hideMiniWindow( BoardCtxt* board, XP_Bool destroy, MiniWindowType winType )
{
    MiniWindowStuff* stuff = &board->miniWindowStuff[winType];

    board_invalRect( board, &stuff->rect );

    if ( destroy ) {
        stuff->text = (XP_UCHAR*)NULL;
    }
} /* hideMiniWindow */
#endif

static XP_Bool
warnBadWords( XP_UCHAR* word, void* closure )
{
    BadWordInfo bwi;
    XP_Bool ok;
    BoardCtxt* board = (BoardCtxt*)closure;
    XP_S16 turn = server_getCurrentTurn( board->server );

    bwi.nWords = 1;
    bwi.words[0] = word;

    ok = util_warnIllegalWord( board->util, &bwi, turn, XP_FALSE );
    board->badWordRejected = !ok || board->badWordRejected;

    return ok;
} /* warnBadWords */

XP_Bool
board_commitTurn( BoardCtxt* board ) 
{
    XP_Bool result = XP_FALSE;
    XP_S16 turn = server_getCurrentTurn( board->server );

    if ( board->gameOver || turn < 0 ) {
        /* do nothing */
    } else if ( turn != board->selPlayer ) {
        if ( board->tradeInProgress[board->selPlayer] ) {
            result = exitTradeMode( board );
        } else {
            util_userError( board->util, ERR_NOT_YOUR_TURN );
        }
    } else if ( checkRevealTray( board ) ) {
        if ( board->tradeInProgress[turn] ) {
            result = XP_TRUE;	/* there's at least the window to clean up
                                   after */

            invalSelTradeWindow( board );
            board->tradeInProgress[turn] = XP_FALSE;

            if ( util_userQuery( board->util, QUERY_COMMIT_TRADE,
                                 (XWStreamCtxt*)NULL ) ) {
                result = server_commitTrade( board->server, 
                                             board->traySelBits[turn] );
                /* 		XP_DEBUGF( "server_commitTrade returned %d\n", result ); */
            }
            board->traySelBits[turn] = 0x00;
        } else {
            XP_Bool warn, legal;
            WordNotifierInfo info;
            XWStreamCtxt* stream = 
                mem_stream_make( MPPARM(board->mpool) 
                                 util_getVTManager(board->util), NULL,
                                 CHANNEL_NONE, (MemStreamCloseCallback)NULL );

            const XP_UCHAR* str = util_getUserString(board->util, 
                                                     STR_COMMIT_CONFIRM);

            stream_putBytes( stream, (void*)str, 
                             (XP_U16)XP_STRLEN((const char*)str) );

            warn = board->util->gameInfo->phoniesAction == PHONIES_WARN;

            board->badWordRejected = XP_FALSE;
            info.proc = warnBadWords;
            info.closure = board;
            legal = model_checkMoveLegal( board->model, turn, stream,
                                          warn? &info:(WordNotifierInfo*)NULL);

            if ( !legal || board->badWordRejected ) {
                result = XP_FALSE;
            } else {
                /* Hide the tray so no peeking.  Leave it hidden even if user
                   cancels as otherwise another player could get around
                   passwords and peek at tiles. */
                result = board_hideTray( board );

                if ( util_userQuery( board->util, QUERY_COMMIT_TURN,
                                     stream ) ) {
                    result = server_commitMove( board->server ) || result;
                    /* invalidate any selected tiles in case we'll be drawing
                       this tray again rather than some other -- as is the
                       case in a two-player game where one's a robot. */
                    board_invalTrayTiles( board, board->traySelBits[turn] );
                    board->traySelBits[turn] = 0x00;
                }
            }
            stream_destroy( stream );

            if ( result ) {
                setArrowVisibleFor( board, turn, XP_FALSE );
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
selectPlayerImpl( BoardCtxt* board, XP_U16 newPlayer, XP_Bool reveal )
{
    if ( !board->gameOver && server_getCurrentTurn(board->server) < 0 ) {
        /* game not started yet; do nothing */
    } else if ( board->selPlayer == newPlayer ) {
        if ( reveal ) {
            checkRevealTray( board );
        }
    } else {
        XP_U16 oldPlayer = board->selPlayer;
        model_foreachPendingCell( board->model, newPlayer,
                                  boardCellChanged, board );
        model_foreachPendingCell( board->model, oldPlayer,
                                  boardCellChanged, board );

        /* if there are pending cells on one view and not the other, then the
           previous move will be drawn highlighted on one and not the other
           and so needs to be invalidated so it'll get redrawn.*/
        if ( (0 == model_getCurrentMoveCount( board->model, newPlayer ))
             != (0 == model_getCurrentMoveCount( board->model, oldPlayer )) ) {
            model_foreachPrevCell( board->model, boardCellChanged, board );
        }

        /* Just in case somebody started a trade when it wasn't his turn and
           there were plenty of tiles but now there aren't. */
        if ( board->tradeInProgress[newPlayer] && 
             server_countTilesInPool(board->server) < MIN_TRADE_TILES ) {
            board->tradeInProgress[newPlayer] = XP_FALSE;
            board->traySelBits[newPlayer] = 0x00; /* clear any selected */
        }

        invalTradeWindow( board, oldPlayer, 
                          board->tradeInProgress[newPlayer] );

#ifdef XWFEATURE_SEARCHLIMIT
        if ( board->hasHintRect[oldPlayer] ) {
            invalCurHintRect( board, oldPlayer );
        }
        if ( board->hasHintRect[newPlayer] ) {
            invalCurHintRect( board, newPlayer );
        }
#endif

        invalArrowCell( board );
        board->selPlayer = (XP_U8)newPlayer;
        invalArrowCell( board );

        board_invalTrayTiles( board, ALLTILES );
        board->dividerInvalid = XP_TRUE;

        setTrayVisState( board, TRAY_REVERSED );
    }
    board->scoreBoardInvalid = XP_TRUE;	/* if only one player, number of
                                           tiles remaining may have changed*/
} /* selectPlayerImpl */

void
board_selectPlayer( BoardCtxt* board, XP_U16 newPlayer )
{
    selectPlayerImpl( board, newPlayer, XP_TRUE );
} /* board_selectPlayer */

void
board_hiliteCellAt( BoardCtxt* board, XP_U16 col, XP_U16 row )
{
    XP_Rect cellRect;

    flipIf( board, col, row, &col, &row );
    if ( getCellRect( board, col, row, &cellRect ) ) {
        draw_invertCell( board->draw, &cellRect );
        invalCell( board, col, row );
    }
    /*     sleep(1); */
} /* board_hiliteCellAt */

void
board_resetEngine( BoardCtxt* board )
{
    server_resetEngine( board->server, board->selPlayer );
} /* board_resetEngine */

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
} /* positionBonusRect */

static void
timerFiredForPen( BoardCtxt* board ) 
{
    const XP_UCHAR* text = (XP_UCHAR*)NULL;
    XP_UCHAR buf[80];

    if ( board->penDownObject == OBJ_BOARD 
#ifdef XWFEATURE_SEARCHLIMIT
         && !board->hintDragInProgress 
#endif
         ) {
        XP_U16 col, row;
        XWBonusType bonus;

        coordToCell( board, board->penDownX, board->penDownY, &col, 
                     &row );
        bonus = util_getSquareBonus(board->util, board->model, col, row);
        if ( bonus != BONUS_NONE ) {
            text = draw_getMiniWText( board->draw, (XWMiniTextType)bonus );
        }
        board->penTimerFired = XP_TRUE;

    } else if ( board->penDownObject == OBJ_SCORE ) {
        XP_S16 player;
        LocalPlayer* lp;
        player = figureScorePlayerTapped( board, board->penDownX, 
                                          board->penDownY );
        /* I've seen this assert fire on simulator.  No log is kept so I can't
           tell why, but might want to test and do nothing in this case.  */
        /* XP_ASSERT( player >= 0 ); */
        if ( player >= 0 ) {
            const XP_UCHAR* format;
            XP_UCHAR scoreExpl[48];
            XP_U16 explLen;

            lp = &board->gi->players[player];
            format = util_getUserString( board->util, lp->isLocal? 
                                         STR_LOCAL_NAME: STR_NONLOCAL_NAME );
            XP_SNPRINTF( buf, sizeof(buf), format, emptyStringIfNull(lp->name) );

            explLen = sizeof(scoreExpl);
            if ( model_getPlayersLastScore( board->model, player, scoreExpl, 
                                            &explLen ) ) {
                XP_STRCAT( buf, XP_CR );
                XP_ASSERT( XP_STRLEN(buf) + explLen < sizeof(buf) );
                XP_STRCAT( buf, scoreExpl );
            }

            text = buf;
        }

        board->penTimerFired = XP_TRUE;
    }

    if ( !!text ) {
        MiniWindowStuff* stuff = &board->miniWindowStuff[MINIWINDOW_VALHINT];
        makeMiniWindowForText( board, text, MINIWINDOW_VALHINT );
        XP_ASSERT( stuff->text == text );
        draw_drawMiniWindow(board->draw, text, &stuff->rect, 
                            &stuff->closure);
    }
} /* timerFiredForPen */

static void
setTimerIf( BoardCtxt* board )
{
    XP_Bool timerWanted = board->gi->timerEnabled && !board->gameOver;

    if ( timerWanted && !board->timerPending ) {
        util_setTimer( board->util, TIMER_TIMERTICK, 0, 
                       p_board_timerFired, board ); 
        board->timerPending = XP_TRUE;
    }
} /* setTimerIf */

static void
timerFiredForTimer( BoardCtxt* board )
{
    board->timerPending = XP_FALSE;
    if ( !board->gameOver ) {
        XP_S16 turn = server_getCurrentTurn( board->server );

        if ( turn >= 0 ) {
            ++board->gi->players[turn].secondsUsed;

            if ( turn == board->selPlayer ) {
                drawTimer( board );
            }
        }
    }
    setTimerIf( board );
} /* timerFiredForTimer */

static void
p_board_timerFired( void* closure, XWTimerReason why )
{
    BoardCtxt* board = (BoardCtxt*)closure;
    if ( why == TIMER_PENDOWN ) {
        timerFiredForPen( board );
    } else {
        XP_ASSERT( why == TIMER_TIMERTICK );
        timerFiredForTimer( board );
    }
} /* board_timerFired */

void
board_pushTimerSave( BoardCtxt* board )
{
    if ( board->gi->timerEnabled ) {
        if ( board->timerSaveCount++ == 0 ) {
            board->timerStoppedTime = util_getCurSeconds( board->util );
#ifdef DEBUG
            board->timerStoppedTurn = server_getCurrentTurn( board->server );
#endif
        }
    }
} /* board_pushTimerSave */

void
board_popTimerSave( BoardCtxt* board )
{
    if ( board->gi->timerEnabled ) {

        /* it's possible for the count to be 0, if e.g. the timer was enabled
           between calls to board_pushTimerSave and this call, as can happen on
           franklin.  So that's not an error. */
        if ( board->timerSaveCount > 0 ) {
            XP_S16 turn = server_getCurrentTurn( board->server );

            XP_ASSERT( board->timerStoppedTurn == turn );

            if ( --board->timerSaveCount == 0 && turn >= 0 ) {
                XP_U32 curTime = util_getCurSeconds( board->util );
                XP_U32 elapsed;

                XP_ASSERT( board->timerStoppedTime != 0 );
                elapsed = curTime - board->timerStoppedTime;
                XP_LOGF( "board_popTimerSave: elapsed=%ld", elapsed );
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
board_formatRemainingTiles( BoardCtxt* board, XWStreamCtxt* stream )
{
    XP_S16 curPlayer = board->selPlayer;
    if ( board->trayVisState != TRAY_REVEALED ) {
        curPlayer = -1;
    }
    server_formatRemainingTiles( board->server, stream, curPlayer );
} /* board_formatRemainingTiles */

static void
board_invalAllTiles( BoardCtxt* board )
{
    XP_U16 lastRow = model_numRows( board->model );
    while ( lastRow-- ) {
        board->redrawFlags[lastRow] = ~0;
    }
    board->tradingMiniWindowInvalid = XP_TRUE;

    board->needsDrawing = XP_TRUE;
} /* board_invalAllTiles */

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
        XP_U16 nVisible = board->lastVisibleRow - board->yOffset;
        lastRow = board->prevYScrollOffset + nVisible - 1;
        firstRow = lastRow - diff + 1;
    }
    XP_ASSERT( firstRow <= lastRow );
    while ( firstRow <= lastRow ) {
        board->redrawFlags[firstRow] |= ~0;
        ++firstRow;
    }
} /* invalOldPerimeter */

static void
invalPerimeter( BoardCtxt* board )
{
    XP_U16 lastCol = model_numCols( board->model ) - 1;
    XP_U16 firstAndLast = (1 << lastCol) | 1;
    XP_U16 firstRow = board->yOffset;
    XP_U16 lastRow = board->lastVisibleRow - 1;

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

static void
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
    XP_U16 i;
    for ( i = 0; i < nPlayers; ++i ) {
        flipLimits( &board->limits[i] );
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
            Tile tile;
            XP_Bool ignore;
            if ( model_getTile( model, col, row, includePending,
                                turn, &tile, &ignore, &ignore, &ignore ) ) {
                XP_U16 boardCol, boardRow;
                flipIf( board, col, row, &boardCol, &boardRow );
                invalCell( board, boardCol, boardRow );
            }
        }
    }
    return board->needsDrawing;
} /* invalCellsWithTiles */

static void
checkScrollCell( void* p_board, XP_U16 col, XP_U16 row )
{
    BoardCtxt* board = (BoardCtxt*)p_board;
    XP_Rect rect;

    if ( board->boardObscuresTray && board->trayVisState != TRAY_HIDDEN ) {

        while ( !getCellRect( board, col, row, &rect ) ) {
            XP_U16 oldOffset = board_getYOffset( board );
            if ( rect.top < board->boardBounds.top ) {
                --oldOffset;
            } else if ( rect.top + rect.height > 
                        board->boardBounds.top + board->boardBounds.height ) {
                ++oldOffset;
            } else {
                XP_ASSERT( 0 );
            }
            board_setYOffset( board, oldOffset );
        }
    }    
} /* checkScrollCell */

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

static void
scrollIfCan( BoardCtxt* board )
{
    if ( board->yOffset != board->prevYScrollOffset ) {
        XP_Rect scrollR = board->boardBounds;
        XP_Bool scrolled;
        XP_S16 dist;

#ifdef PERIMETER_FOCUS
        if ( (board->focussed == OBJ_BOARD) && !board->focusHasDived ) {
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
               making the client figure the inval rect, but Palm's the only
               client now and it does it so well.... */
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

#ifdef KEYBOARD_NAV
static XP_Bool
cellFocused( const BoardCtxt* board, XP_U16 col, XP_U16 row )
{
    XP_Bool focussed = XP_FALSE;

    if ( board->focussed == OBJ_BOARD ) {
        if ( board->focusHasDived ) {
            if ( (col == board->bdCursor[board->selPlayer].col)
                 && (row == board->bdCursor[board->selPlayer].row) ) {
                focussed = XP_TRUE;
            }
        } else {
#ifdef PERIMETER_FOCUS
            focussed = (col == 0)
                || (col == model_numCols(board->model) - 1)
                || (row == board->yOffset)
                || (row == board->lastVisibleRow - 1);
#else
            focussed = XP_TRUE;
#endif
        }
    }
    return focussed;
} /* cellFocused */
#endif

static void
drawBoard( BoardCtxt* board )
{
    if ( board->needsDrawing 
         && draw_boardBegin( board->draw, 
                             model_getDictionary( board->model ),
                             &board->boardBounds, 
                             dfsFor( board, OBJ_BOARD ) ) ) {

        XP_Bool allDrawn = XP_TRUE;
        XP_S16 lastCol, i;
        XP_S16 row;
        ModelCtxt* model = board->model;
        BlankQueue bq;
        XP_Rect arrowRect;

        scrollIfCan( board );	/* this must happen before we count blanks
                                   since it invalidates squares */

        /* This is freaking expensive!!!! PENDING FIXME Can't we start from
           what's invalid rather than scanning the entire model every time
           somebody dirties a single cell? */
        model_listPlacedBlanks( model, board->selPlayer, 
                                board->trayVisState == TRAY_REVEALED, &bq );
        invalBlanksWithNeighbors( board, &bq );

        for ( row = board->yOffset; row < board->lastVisibleRow; ++row ) {
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

        if ( board->trayVisState == TRAY_REVEALED ) {
            BoardArrow* arrow = &board->boardArrow[board->selPlayer];

            if ( arrow->visible ) {
                XP_U16 col = arrow->col;
                XP_U16 row = arrow->row;
                if ( getCellRect( board, col, row, &arrowRect ) ) {
                    XWBonusType bonus;
                    HintAtts hintAtts;
                    CellFlags flags = CELL_NONE;
                    bonus = util_getSquareBonus( board->util, model, 
                                                 col, row );
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
        }

        drawTradeWindowIf( board );

        draw_objFinished( board->draw, OBJ_BOARD, &board->boardBounds, 
                          dfsFor( board, OBJ_BOARD ) );

        board->needsDrawing = !allDrawn;
    }
} /* drawBoard */

#ifdef KEYBOARD_NAV
DrawFocusState
dfsFor( BoardCtxt* board, BoardObjectType obj )
{
    DrawFocusState dfs;
    if ( board->focussed == obj ) {
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
#endif

void
board_setTrayLoc( BoardCtxt* board, XP_U16 trayLeft, XP_U16 trayTop, 
                  XP_U16 trayWidth, XP_U16 trayHeight,
                  XP_U16 minDividerWidth )
{
    XP_U16 dividerWidth;
    XP_U16 boardBottom, boardRight;
    XP_Bool boardHidesTray;
    board->trayBounds.left = trayLeft;
    board->trayBounds.top = trayTop;
    /* what's this +1 for? */

    board->trayBounds.width = trayWidth;
    board->trayBounds.height = trayHeight;

    dividerWidth = minDividerWidth + 
        ((trayWidth - minDividerWidth) % MAX_TRAY_TILES);

    board->trayScaleH = (trayWidth - dividerWidth) / MAX_TRAY_TILES;
    board->trayScaleV = trayHeight;

    board->dividerWidth = dividerWidth;

    /* boardObscuresTray is about whether they *can* overlap, not just about
     * they do given the current scroll position of the board. Remember
     * (e.g. for curses version) that vertical intersection isn't enough.*/
    boardBottom = board->boardBounds.top
        + (board->boardVScale * model_numRows( board->model ));
    boardRight = board->boardBounds.left
        + (board->boardHScale * model_numCols( board->model ));
    board->boardObscuresTray = (trayTop < boardBottom)
        && (trayLeft < boardRight);

    boardHidesTray = board->boardObscuresTray;
    if ( boardHidesTray ) { /* can't hide if doesn't obscure */
        if ( (trayTop + trayHeight) > boardBottom ) {
            boardHidesTray = XP_FALSE;
        } else if ( (trayLeft + trayWidth) > boardRight ) {
            boardHidesTray = XP_FALSE;
        }
    }
    board->boardHidesTray = boardHidesTray;

    if ( board->trayVisState == TRAY_HIDDEN ) {
        if ( !board->boardHidesTray ) {
            XW_TrayVisState state = TRAY_REVERSED;
            setTrayVisState( board, state );
        }
    }
    figureBoardRect( board );
} /* board_setTrayLoc */

static void
invalCellsUnderRect( BoardCtxt* board, XP_Rect* rect )
{
    XP_U16 lastCol, lastRow;

    lastRow = model_numRows( board->model );
    while ( lastRow-- ) {
        lastCol = model_numCols( board->model );
        while ( lastCol-- ) {
            XP_Rect cell;

            if ( getCellRect( board, lastCol, lastRow, &cell ) &&
                 rectsIntersect( rect, &cell ) ) {
                invalCell( board, lastCol, lastRow );
            }
        }
    }
} /* invalCellsUnderRect */

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
board_hideTray( BoardCtxt* board )
{
    XW_TrayVisState soughtState;
    if ( board->boardObscuresTray ) {
        soughtState = TRAY_HIDDEN;
    } else {
        soughtState = TRAY_REVERSED;
    }
    return setTrayVisState( board, soughtState );
} /* board_hideTray */

static XP_S16
chooseBestSelPlayer( BoardCtxt* board )
{
    ServerCtxt* server = board->server;

    if ( board->gameOver ) {
        return board->selPlayer;
    } else {

        XP_S16 curTurn = server_getCurrentTurn( server );

        if ( curTurn >= 0 ) {
            XP_U16 nPlayers = board->gi->nPlayers;
            XP_U16 i;

            for ( i = 0; i < nPlayers; ++i ) {
                LocalPlayer* lp = &board->gi->players[curTurn];
		
                if ( !lp->isRobot && lp->isLocal ) {
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
board_showTray( BoardCtxt* board )
{
    return checkRevealTray( board );
} /* board_showTray */

XW_TrayVisState
board_getTrayVisState( BoardCtxt* board )
{
    return board->trayVisState;
} /* board_getTrayVisible */

static XP_Bool
setTrayVisState( BoardCtxt* board, XW_TrayVisState newState )
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

        /* redraw cells that are pending; whether tile is visible may
           change */
        model_foreachPendingCell( board->model, selPlayer,
                                  boardCellChanged, board );
        /* ditto -- if there's a pending move */
        model_foreachPrevCell( board->model, boardCellChanged, board );

        board_invalTrayTiles( board, ALLTILES );
        board->dividerInvalid = XP_TRUE;

        board->trayVisState = newState;

        invalSelTradeWindow( board );

        figureBoardRect( board ); /* comes before setYOffset since that
                                     uses rects to calc scroll */

        if ( board->boardObscuresTray ) {
            if ( nowHidden ) {
                board->preHideYOffset = board_getYOffset( board );
                board_setYOffset( board, 0 );
            } else {
                board_setYOffset( board, board->preHideYOffset );
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

        nVisible = board->lastVisibleRow - board->yOffset;
        util_trayHiddenChange( board->util, board->trayVisState, nVisible );
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
        XP_U16 redrawFlag = board->redrawFlags[nRows];
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
board_toggle_showValues( BoardCtxt* board )
{
    XP_Bool changed;
    board->showCellValues = !board->showCellValues;

    /* We show the tile values when showCellValues is set even if
       hideValsInTray is set.  So inval the tray if there will be a change.
       And set changed to true in case there are no tiles on the baord yet. 
    */
    changed = board->hideValsInTray && (board->trayVisState == TRAY_REVEALED);
    if ( changed ) {
        board_invalTrayTiles( board, ALLTILES );
    }
    return invalCellsWithTiles( board ) || changed;
} /* board_toggle_showValues */

XP_Bool
board_setShowColors( BoardCtxt* board, XP_Bool showColors )
{
    board->showColors = showColors;
    board->scoreBoardInvalid = XP_TRUE;
    return invalCellsWithTiles( board );
} /* board_setShowColors */

XP_Bool
board_replaceTiles( BoardCtxt* board )
{
    XP_Bool result = XP_FALSE;
    while ( replaceLastTile( board ) ) {
        result = XP_TRUE;
    } 

    if ( result ) {
        (void)setArrowVisible( board, XP_FALSE );
    }

    return result;
} /* board_replaceTiles */

/* There are a few conditions that must be true for any of several actions
   to be allowed.  Check them here.  */
static XP_Bool
preflight( BoardCtxt* board )
{
    return !board->gameOver && !TRADE_IN_PROGRESS(board)
        && server_getCurrentTurn(board->server) >= 0
        && checkRevealTray( board );
} /* preflight */

/* Refuse with error message if any tiles are currently on board in this turn.
 * Then call the engine, and display the first move.  Return true if there's
 * any redrawing to be done.
 */
XP_Bool
board_requestHint( BoardCtxt* board, 
#ifdef XWFEATURE_SEARCHLIMIT
                   XP_Bool useTileLimits,
#endif
                   XP_Bool* workRemainsP )
{
    MoveInfo newMove;
    XP_Bool result = XP_FALSE;
    XP_S16 nTiles;
    const Tile* tiles;
    XP_U16 selPlayer;
    EngineCtxt* engine;
    XP_Bool searchComplete = XP_TRUE;
    XP_Bool redraw = XP_FALSE;
    
    *workRemainsP = XP_FALSE;	/* in case we exit without calling engine */

    selPlayer = board->selPlayer;
    engine = server_getEngineFor( board->server, selPlayer );
    /* engine may be null, if e.g. hint menu's chosen for a remote player */
    result = !!engine && !board->gi->hintsNotAllowed && preflight( board );

    if ( result ) {
        const TrayTileSet* tileSet;
        ModelCtxt* model = board->model;

        /* undo any current move.  otherwise we won't pass the full tray to
           the engine.  Would it be better, though, to pass the whole tray
           regardless where its contents are? */
        if ( model_getCurrentMoveCount( model, selPlayer ) > 0 ) {
            model_resetCurrentTurn( model, selPlayer );
            /* Draw's a no-op on Wince with a null hdc, but it'll draw again.
               Should probably define OS_INITS_DRAW on Wince...*/
#ifdef OS_INITS_DRAW
            /* On symbian, it's illegal to draw except from inside the Draw
               method.  But the move search will probably be so fast that it's
               ok to wait until we've found the move anyway. */
            redraw = XP_TRUE;
#else
            board_draw( board );
#endif
        }

        tileSet = model_getPlayerTiles( model, selPlayer );
        nTiles = tileSet->nTiles - board->dividerLoc[selPlayer];
        result = nTiles > 0;
        if ( result ) {
#ifdef XWFEATURE_SEARCHLIMIT
            BdHintLimits limits;
            BdHintLimits* lp = NULL;
#endif
            XP_Bool wasVisible;
            XP_Bool canMove;

            wasVisible = setArrowVisible( board, XP_FALSE );

            (void)board_replaceTiles( board );

            tiles = tileSet->tiles + board->dividerLoc[selPlayer];

            board_pushTimerSave( board );

#ifdef XWFEATURE_SEARCHLIMIT
            XP_ASSERT( board->gi->allowHintRect
                       || !board->hasHintRect[selPlayer] );
            if ( board->gi->allowHintRect && board->hasHintRect[selPlayer] ) {
                limits = board->limits[selPlayer];
                lp = &limits;
                if ( board->isFlipped ) {
                    flipLimits( lp );
                }
            }
#endif
            searchComplete = engine_findMove(engine, model, 
                                             model_getDictionary(model),
                                             tiles, nTiles,
#ifdef XWFEATURE_SEARCHLIMIT
                                             lp, useTileLimits,
#endif
                                             NO_SCORE_LIMIT, 
                                             &canMove, &newMove );
            board_popTimerSave( board );

            if ( searchComplete && canMove ) {
                model_makeTurnFromMoveInfo( model, selPlayer, &newMove);
            } else {
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
    }
    return result || redraw;
} /* board_requestHint */

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
            XP_Bitmap bitmap = NULL;
            XP_UCHAR* textP = (XP_UCHAR*)ch;
            HintAtts hintAtts;
            CellFlags flags = CELL_NONE;

            isEmpty = !model_getTile( model, modelCol, modelRow, showPending,
                                      selPlayer, &tile, &isBlank,
                                      &pending, &recent );

            if ( isEmpty ) {
                isBlank = XP_FALSE;
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
                    XP_SNPRINTF( ch, sizeof(ch), (XP_UCHAR*)"%d", val );
                } else if ( dict_faceIsBitmap( dict, tile ) ) {
                    bitmap = dict_getFaceBitmap( dict, tile, XP_FALSE );
                    XP_ASSERT( !!bitmap );
                    textP = (XP_UCHAR*)NULL;
                } else {
                    (void)dict_tilesToString( dict, &tile, 1, ch, sizeof(ch) );
                }
            }
            bonus = util_getSquareBonus( board->util, model, col, row );
            hintAtts = figureHintAtts( board, col, row );

            if ( isEmpty && (col==board->star_row)
                 && (row==board->star_row ) ) {
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

            success = draw_drawCell( board->draw, &cellRect, textP, bitmap, 
                                     tile, owner, bonus, hintAtts, flags );
            break;
        }
    }
    return success;
} /* drawCell */

static void
figureBoardRect( BoardCtxt* board )
{
    XP_U16 boardVScale = board->boardVScale;
    if ( boardVScale > 0 ) {
        XP_Rect boardBounds = board->boardBounds;
        XP_U16 nVisible;

        boardBounds.width = model_numCols( board->model ) * board->boardHScale;
        boardBounds.height = (model_numRows( board->model ) - board->yOffset)
            * board->boardVScale;

        if ( board->boardObscuresTray ) {
            if ( board->trayVisState != TRAY_HIDDEN ) {
                boardBounds.height = board->trayBounds.top - boardBounds.top;
            } else {
                XP_U16 trayBottom;
                trayBottom = board->trayBounds.top + board->trayBounds.height;
                if ( trayBottom < boardBounds.top + boardBounds.height ) {
                    boardBounds.height = trayBottom - boardBounds.top;
                }
            }
        }
        /* round down */
        nVisible = boardBounds.height / boardVScale;
        boardBounds.height = nVisible * boardVScale;
        board->lastVisibleRow = nVisible + board->yOffset;

        board->boardBounds = boardBounds;
    }
} /* figureBoardRect */

static XP_Bool
coordToCell( BoardCtxt* board, XP_U16 x, XP_U16 y, XP_U16* colP, XP_U16* rowP )
{
    XP_U16 col, row, max;
    XP_Bool onBoard = XP_TRUE;

    x -= board->boardBounds.left;

    y -= board->boardBounds.top;
    y += board->boardVScale * board->yOffset;

    col = x / board->boardHScale;
    row = y / board->boardVScale;

    max = model_numCols( board->model ) - 1;
    /* I don't deal with non-square boards yet. */
    XP_ASSERT( max + 1 == model_numRows( board->model ) );
    if ( col > max ) {
        col = max;
        onBoard = XP_FALSE;
    }
    if ( row > max ) {
        row = max;
        onBoard = XP_FALSE;
    }

    *colP = col;
    *rowP = row;
    return onBoard;
} /* coordToCell */

static XP_Bool
getCellRect( BoardCtxt* board, XP_U16 col, XP_U16 row, XP_Rect* rect )
{
    XP_S16 top;
    XP_Bool onBoard = XP_TRUE;

    if ( row < board->yOffset ) {
        onBoard = XP_FALSE;
    }

    rect->left = board->boardBounds.left + (col * board->boardHScale);
    top = board->boardBounds.top + 
        ((row - board->yOffset) * board->boardVScale);
    if ( top >= (board->boardBounds.top + board->boardBounds.height) ) {
        onBoard = XP_FALSE;
    }
    rect->top = top;

    rect->width = board->boardHScale;
    rect->height = board->boardVScale;
    return onBoard;
} /* getCellRect */

static void
invalCell( BoardCtxt* board, XP_U16 col, XP_U16 row )
{
    board->redrawFlags[row] |= 1 << col;

    XP_ASSERT( col < MAX_ROWS );
    XP_ASSERT( row < MAX_ROWS );

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
    
    board->needsDrawing = XP_TRUE;
} /* invalCell */

#if defined POINTER_SUPPORT || defined KEYBOARD_NAV
static XP_Bool
pointOnSomething( BoardCtxt* board, XP_U16 x, XP_U16 y, BoardObjectType* wp )
{
    XP_Bool result = XP_TRUE;
    if ( board->trayVisState != TRAY_HIDDEN
         && rectContainsPt( &board->trayBounds, x, y ) ) {
        *wp = OBJ_TRAY;
    } else if ( rectContainsPt( &board->boardBounds, x, y ) ) {
        *wp = OBJ_BOARD;
    } else if ( rectContainsPt( &board->scoreBdBounds, x, y ) ) {
        *wp = OBJ_SCORE;
    } else {
        result = XP_FALSE;
    }

    return result;
} /* pointOnSomething */

/* Move the given tile to the board.  If it's a blank, we need to ask the user
 * what to call it first.
 */
XP_Bool
moveTileToArrowLoc( BoardCtxt* board, XP_U8 index )
{
    XP_Bool result;
    BoardArrow* arrow = &board->boardArrow[board->selPlayer];
    if ( arrow->visible ) {
        result = moveTileToBoard( board, arrow->col, arrow->row,
                                  (XP_U16)index, EMPTY_TILE );
        if ( result ) {
            XP_Bool moved = advanceArrow( board );
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

static void
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

static void
makeMiniWindowForTrade( BoardCtxt* board )
{
    const XP_UCHAR* text;

    text = draw_getMiniWText( board->draw, INTRADE_MW_TEXT );

    makeMiniWindowForText( board, text, MINIWINDOW_TRADING );
} /* makeMiniWindowForTrade */

XP_Bool
board_beginTrade( BoardCtxt* board )
{
    XP_Bool result;

    result = preflight( board );
    if ( result ) {
        /* check turn before tradeInProgress so I can't tell my opponent's in a
           trade */
        if ( 0 != model_getCurrentMoveCount( board->model, board->selPlayer )){
            util_userError( board->util, ERR_CANT_TRADE_MID_MOVE );
        } else {
            if ( server_countTilesInPool(board->server) < MIN_TRADE_TILES){
                util_userError( board->util, ERR_TOO_FEW_TILES_LEFT_TO_TRADE );
            } else {
                board->tradingMiniWindowInvalid = XP_TRUE;
                board->needsDrawing = XP_TRUE;
                board->tradeInProgress[board->selPlayer] = XP_TRUE;
                setArrowVisible( board, XP_FALSE );
                result = XP_TRUE;
            }
        }
    }
    return result;
} /* board_beginTrade */

#if defined POINTER_SUPPORT || defined KEYBOARD_NAV
static XP_Bool
ptOnTradeWindow( BoardCtxt* board, XP_U16 x, XP_U16 y )
{
    XP_Rect* windowR = &board->miniWindowStuff[MINIWINDOW_TRADING].rect;
    return rectContainsPt( windowR, x, y );
} /* ptOnTradeWindow */

#ifdef XWFEATURE_SEARCHLIMIT
static HintAtts
figureHintAtts( BoardCtxt* board, XP_U16 col, XP_U16 row )
{
    HintAtts result = HINT_BORDER_NONE;

    if ( board->trayVisState == TRAY_REVEALED && board->gi->allowHintRect ) {
        BdHintLimits limits = board->limits[board->selPlayer];

        /* while lets us break to exit... */
        while ( board->hasHintRect[board->selPlayer]
                || board->hintDragInProgress ) {
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
    }

    return result;
} /* figureHintAtts */

static XP_Bool
startHintRegionDrag( BoardCtxt* board, XP_U16 x, XP_U16 y )
{
    XP_Bool needsRedraw = XP_FALSE;
    XP_U16 col, row;

    coordToCell( board, x, y, &col, &row );
    board->hintDragStartCol = board->hintDragCurCol = col;
    board->hintDragStartRow = board->hintDragCurRow = row;

    return needsRedraw;
} /* startHintRegionDrag */

static void
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

static void
invalCurHintRect( BoardCtxt* board, XP_U16 player )
{
    BdHintLimits* limits = &board->limits[player];    
    invalCellRegion( board, limits->left, limits->top, 
                     limits->right, limits->bottom );
} /* invalCurHintRect */

static void
clearCurHintRect( BoardCtxt* board )
{
    invalCurHintRect( board, board->selPlayer );
    board->hasHintRect[board->selPlayer] = XP_FALSE;
} /* clearCurHintRect */

static void
setHintRect( BoardCtxt* board )
{
    BdHintLimits limits;
    if ( board->hintDragStartRow < board->hintDragCurRow ) {
        limits.top = board->hintDragStartRow;
        limits.bottom = board->hintDragCurRow;
    } else {
        limits.top =  board->hintDragCurRow;
        limits.bottom = board->hintDragStartRow;
    }
    if ( board->hintDragStartCol < board->hintDragCurCol ) {
        limits.left = board->hintDragStartCol;
        limits.right = board->hintDragCurCol;
    } else {
        limits.left =  board->hintDragCurCol;
        limits.right = board->hintDragStartCol;
    }

    board->limits[board->selPlayer] = limits;
    board->hasHintRect[board->selPlayer] = XP_TRUE;
} /* setHintRect */

static void
invalHintRectDiffs( BoardCtxt* board, BdHintLimits* newLim, 
                    BdHintLimits* oldLim )
{
    /* These two regions will generally have close to 50% of their borders in
       common.  Try not to inval what needn't be inval'd.  But at the moment
       performance seems good enough without adding the complexity and new
       bugs... */
    invalCellRegion( board, newLim->left, newLim->top, 
                     newLim->right, newLim->bottom );
    invalCellRegion( board, oldLim->left, oldLim->top, 
                     oldLim->right, oldLim->bottom );

    /* The challenge in doing a smarter diff is that some squares need to be
       invalidated even if they're part of the borders of both limits rects,
       in particular if one is a corner of one and just a side of another.
       One simple but expensive way of accounting for this would be to call
       figureHintAtts() on each square in the borders of both rects and
       invalidate when the hintAttributes aren't the same for both.  That
       misses an opportunity to avoid doing any calculations on those border
       squares that clearly haven't changed at all.
    */
} /* invalHintRectDiffs */

static XP_Bool
continueHintRegionDrag( BoardCtxt* board, XP_U16 x, XP_U16 y )
{
    XP_Bool needsRedraw = XP_FALSE;

    XP_U16 col, row;
    if ( coordToCell( board, x, y, &col, &row ) ) {
        XP_U16 selPlayer = board->selPlayer;

        checkScrollCell( board, col, row );

        if ( col != board->hintDragCurCol || row != board->hintDragCurRow ) {
            BdHintLimits oldHL;

            needsRedraw = XP_TRUE;

            board->hintDragInProgress = XP_TRUE;

            /* Now that we've moved, this isn't a timer thing.  Clean up any
               artifacts. */
            board->penTimerFired = XP_FALSE;
            if ( valHintMiniWindowActive( board ) ) {
                hideMiniWindow( board, XP_TRUE, MINIWINDOW_VALHINT );
            }

            board->hintDragCurCol = col;
            board->hintDragCurRow = row;

            oldHL = board->limits[selPlayer];
            setHintRect( board );
            invalHintRectDiffs( board, &board->limits[selPlayer], &oldHL );
        }
    }

    return needsRedraw;
} /* continueHintRegionDrag */

static XP_Bool
finishHintRegionDrag( BoardCtxt* board, XP_U16 x, XP_U16 y )
{
    XP_Bool needsRedraw = XP_FALSE;
    XP_Bool makeActive;

    XP_ASSERT( board->hintDragInProgress );
    needsRedraw = continueHintRegionDrag( board, x, y );

    /* Now check if the whole drag ended above where it started.  If yes, it
       means erase! */
    makeActive = board->hintDragStartRow <= board->hintDragCurRow;

    board->hasHintRect[board->selPlayer] = makeActive;
    if ( !makeActive ) {
        invalCurHintRect( board, board->selPlayer );
        needsRedraw = XP_TRUE;
    }    
    board_resetEngine( board );

    return needsRedraw;
} /* finishHintRegionDrag */
#endif

static XP_Bool
handlePenDownOnBoard( BoardCtxt* board, XP_U16 x, XP_U16 y )
{
    XP_Bool result = XP_FALSE;
    /* Start a timer no matter what.  After it fires we'll decide whether it's
       appropriate to handle it.   No.  That's too expensive */
    if ( TRADE_IN_PROGRESS(board) && ptOnTradeWindow( board, x, y ) ) {
        return XP_FALSE;
    }
    util_setTimer( board->util, TIMER_PENDOWN, 0, p_board_timerFired, board );
#ifdef XWFEATURE_SEARCHLIMIT
    if ( board->gi->allowHintRect && board->trayVisState == TRAY_REVEALED ) {
        result = startHintRegionDrag( board, x, y );
    }
#endif

    return result;
} /* handlePenDownOnBoard */
#endif /* POINTER_SUPPORT */

/* If there's a password, ask it; if they match, change the state of the tray
 * to TRAY_REVEALED (unless we're not supposed to show the tiles).  Return
 * value talks about whether the tray needs to be redrawn, not the success of
 * the password compare.
 */
static XP_Bool
askRevealTray( BoardCtxt* board )
{
    XP_Bool revealed = XP_FALSE;
    XP_Bool reversed = board->trayVisState == TRAY_REVERSED;
    XP_U16 selPlayer = board->selPlayer;
    LocalPlayer* lp = &board->gi->players[selPlayer];
    XP_Bool justReverse = XP_FALSE;

    if ( board->gameOver ) {
        revealed = XP_TRUE;
    } else if ( server_getCurrentTurn( board->server ) < 0 ) {
        revealed = XP_FALSE;
#ifndef XWFEATURE_STANDALONE_ONLY
    } else if ( !lp->isLocal ) {
        util_userError( board->util, ERR_NO_PEEK_REMOTE_TILES );
#endif
    } else if ( lp->isRobot ) {
        if ( reversed ) {
            util_userError( board->util, ERR_NO_PEEK_ROBOT_TILES );
        } else {
            justReverse = XP_TRUE;
        }
    } else {
		revealed = !player_hasPasswd( lp );

		if ( !revealed ) {
			const XP_UCHAR* name = emptyStringIfNull(lp->name);

			/* repeat until player gets passwd right or hits cancel */
            for ( ; ; ) {
                XP_UCHAR buf[16];
                XP_U16 buflen = sizeof(buf);
                if ( !util_askPassword( board->util, name, buf, &buflen ) ) {
                    break;
                }
                if ( buflen > 0 ) {
                    if ( player_passwordMatches( lp, (XP_U8*)buf, buflen ) ) {
                        revealed = XP_TRUE;
                        break;
                    }
				}
			}
		}
	}

    if ( revealed ) {
        setTrayVisState( board, TRAY_REVEALED );
    } else if ( justReverse ) {
        setTrayVisState( board, TRAY_REVERSED );
    }
    return justReverse || revealed;
} /* askRevealTray */

XP_Bool
checkRevealTray( BoardCtxt* board )
{
    XP_Bool result = board->trayVisState == TRAY_REVEALED;
    if ( !result ) {
        result = askRevealTray( board );
    }
    return result;
} /* checkRevealTray */

static XP_Bool
handleLikeDown( BoardCtxt* board, BoardObjectType onWhich, XP_U16 x, XP_U16 y )
{
    XP_Bool result = XP_FALSE;

    switch ( onWhich ) {
    case OBJ_BOARD:
        result = handlePenDownOnBoard( board, x, y ) || result;
        break;

    case OBJ_TRAY:
        XP_ASSERT( board->trayVisState != TRAY_HIDDEN );

        if ( board->trayVisState != TRAY_REVERSED ) {
            result = handlePenDownInTray( board, x, y ) || result;
        }
        break;

    case OBJ_SCORE:
        if ( figureScorePlayerTapped( board, x, y ) >= 0 ) {
            util_setTimer( board->util, TIMER_PENDOWN, 0, 
                           p_board_timerFired, board );
        }
        break;
    default:
        break;
    }

    board->penDownX = x;
    board->penDownY = y;
    board->penDownObject = onWhich;

    return result;
} /* handleLikeDown */

#ifdef POINTER_SUPPORT
XP_Bool
board_handlePenDown( BoardCtxt* board, XP_U16 x, XP_U16 y, XP_Bool* handled )
{
    XP_Bool result = XP_FALSE;
    XP_Bool penDidSomething;
    BoardObjectType onWhich;

    penDidSomething = pointOnSomething( board, x, y, &onWhich );

    if ( !penDidSomething ) {
        board->penDownObject = OBJ_NONE;
    } else {

#ifdef KEYBOARD_NAV
        /* clear focus as soon as pen touches board */
        result = invalFocusOwner( board );
        board->focussed = OBJ_NONE;
        board->focusHasDived = XP_FALSE;
#endif

        result = handleLikeDown( board, onWhich, x, y );
    }
    *handled = penDidSomething;

    return result;		/* no redraw needed */
} /* board_handlePenDown */
#endif

XP_Bool
board_handlePenMove( BoardCtxt* board, XP_U16 x, XP_U16 y )
{
    XP_Bool result = XP_FALSE;

    if ( board->tileDragState.dragInProgress ) {
        result = continueTileDrag( board, x, y ) != 0;
    } else if ( board->divDragState.dragInProgress ) {
        result = continueDividerDrag( board, x, y ) != 0;
#ifdef XWFEATURE_SEARCHLIMIT
    } else if ( board->gi->allowHintRect 
                && board->trayVisState == TRAY_REVEALED ) {
        result = continueHintRegionDrag( board, x, y );
#endif
    }

    return result;
} /* board_handlePenMove */

/* Called when user taps on the board and a tray tile's selected.
 */
static XP_Bool
moveSelTileToBoardXY( BoardCtxt* board, XP_U16 col, XP_U16 row )
{
    XP_Bool result;
    XP_U16 selPlayer = board->selPlayer;
    TileBit bits = board->traySelBits[selPlayer];
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
        board->traySelBits[selPlayer] = bits;
    }

    return result;
} /* moveSelTileToBoardXY */

static XP_Bool
cellOccupied( BoardCtxt* board, XP_U16 col, XP_U16 row, XP_Bool inclPending )
{
    Tile tile;
    XP_Bool ignr;
    XP_Bool result;

    flipIf( board, col, row, &col, &row );
    result = model_getTile( board->model, col, row, inclPending,
                            board->selPlayer, &tile, 
                            &ignr, &ignr, &ignr );
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

        BoardArrow* arrow = &board->boardArrow[board->selPlayer];
	
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

/* Did I tap on a tile on the board that I have not yet committed?  If so,
 * return it to the tray.
 */
static XP_Bool
tryReplaceTile( BoardCtxt* board, XP_U16 pencol, XP_U16 penrow )
{
    XP_Bool result = XP_FALSE;
    XP_S16 index;
    XP_U16 col, row;
    Tile tile;
    XP_Bool ignore, isPending;
    XP_U16 modcol, modrow;

    flipIf( board, pencol, penrow, &modcol, &modrow );
    if ( model_getTile( board->model, modcol, modrow, XP_TRUE,
                        board->selPlayer, &tile, &ignore, &isPending, 
                        (XP_Bool*)NULL )
         && isPending ) {

        XP_S16 count = model_getCurrentMoveCount( board->model, 
                                                  board->selPlayer );
        while ( count-- ) {
            index = count;
            model_getCurrentMoveTile( board->model, board->selPlayer, 
                                      &index, &tile, &col, &row, &ignore );
            if ( col == modcol && row == modrow ) {
                model_moveBoardToTray( board->model, board->selPlayer, 
                                       index );
                /* the cursor should show up where the tile used to be so it's
                   easy to replace it. */
                setArrow( board, pencol, penrow );
                result = XP_TRUE;
                break;
            }
        }
    }
    return result;
} /* tryReplaceTile */

static XP_Bool
handleActionInCell( BoardCtxt* board, XP_U16 col, XP_U16 row )
{
    return moveSelTileToBoardXY( board, col, row )
        || tryMoveArrow( board, col, row )
        || tryReplaceTile( board, col, row );
} /* handleActionInCell */
#endif /* POINTER_SUPPORT || KEYBOARD_NAV */

static XP_Bool
exitTradeMode( BoardCtxt* board )
{
    XP_U16 selPlayer = board->selPlayer;
    invalSelTradeWindow( board );
    board->tradeInProgress[selPlayer] = XP_FALSE;
    board_invalTrayTiles( board, board->traySelBits[selPlayer] );
    board->traySelBits[selPlayer] = 0x00;
    return XP_TRUE;
} /* exitTradeMode */

#if defined POINTER_SUPPORT || defined KEYBOARD_NAV
XP_Bool
board_handlePenUp( BoardCtxt* board, XP_U16 x, XP_U16 y )
{
    XP_Bool result = XP_FALSE;
    BoardObjectType prevObj = board->penDownObject;

    /* prevent timer from firing after pen lifted.  Set now rather than later
       in case we put up a modal alert/dialog that must be dismissed before
       exiting this function (which might give timer time to fire. */
    board->penDownObject = OBJ_NONE;

    if ( board->tileDragState.dragInProgress ) {
        result = endTileDrag( board, x, y );
    } else if ( board->divDragState.dragInProgress ) {
        result = endDividerDrag( board, x, y );
#ifdef XWFEATURE_SEARCHLIMIT
    } else if ( board->hintDragInProgress ) {
        XP_ASSERT( board->gi->allowHintRect );
        result = finishHintRegionDrag( board, x, y );
#endif
    } else if ( board->penTimerFired ) {
        if ( valHintMiniWindowActive( board ) ) {
            hideMiniWindow( board, XP_TRUE, MINIWINDOW_VALHINT );
            result = XP_TRUE;
        }
        /* Need to clean up if there's been any dragging happening */
        board->penTimerFired = XP_FALSE;
    } else {
        BoardObjectType onWhich;
        if ( pointOnSomething( board, x, y, &onWhich ) ) {

            switch( onWhich ) {
            case OBJ_SCORE:
                if ( prevObj == OBJ_SCORE ) {
                    result = handlePenUpScore( board, x, y );
                }
                break;
            case OBJ_BOARD:
                if ( prevObj == OBJ_BOARD
                     && board->trayVisState == TRAY_REVEALED ) {

                    if ( TRADE_IN_PROGRESS(board) ) {
                        if ( ptOnTradeWindow( board, x, y )) {
                            result = exitTradeMode( board );
                        }
                    } else {
                        XP_U16 col, row;
                        coordToCell( board, board->penDownX, board->penDownY,
                                     &col, &row );
                        result = handleActionInCell( board, col, row );
                    }
                }
                break;
            case OBJ_TRAY:
                if ( board->trayVisState == TRAY_REVERSED ) {
                    result = askRevealTray( board );
                } else {
                    result = handlePenUpTray( board, x, y );
                }
                break;
            default:
                XP_ASSERT( XP_FALSE );
            }
        }
    }

#ifdef XWFEATURE_SEARCHLIMIT
    board->hintDragInProgress = XP_FALSE;
#endif
    return result;
} /* board_handlePenUp */
#endif /* #ifdef POINTER_SUPPORT */

#ifdef KEYBOARD_NAV
static void
getRectCenter( const XP_Rect* rect, XP_U16* xp, XP_U16* yp )
{
    *xp = rect->left + ( rect->width >> 1 );
    *yp = rect->top + ( rect->height >> 1 );
}

static void
getFocussedCellCenter( BoardCtxt* board, XP_U16* xp, XP_U16* yp )
{
    XP_Rect rect;
    XP_U16 selPlayer = board->selPlayer;
    BdCursorLoc* cursorLoc = &board->bdCursor[selPlayer];

    getCellRect( board, cursorLoc->col, cursorLoc->row, &rect );
    getRectCenter( &rect, xp, yp );
}

static void
getFocussedTileCenter( BoardCtxt* board, XP_U16* xp, XP_U16* yp )
{
    XP_Rect rect;
    XP_U16 indx = board->trayCursorLoc[board->selPlayer];
    figureTrayTileRect( board, indx, &rect );
    getRectCenter( &rect, xp, yp );
}

static void
getFocussedScoreCenter( BoardCtxt* board, XP_U16* xp, XP_U16* yp )
{
    getRectCenter( &board->scoreRects[board->scoreCursorLoc], xp, yp );
}

static XP_Bool
focusToCoords( BoardCtxt* board, XP_U16* xp, XP_U16* yp )
{
    XP_Bool result = board->focusHasDived;
    if ( result ) {
        switch( board->focussed ) {
        case OBJ_NONE:
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
handleFocusKeyUp( BoardCtxt* board, XP_Key key, XP_Bool preflightOnly,
                  XP_Bool* pHandled )
{
    XP_Bool redraw = XP_FALSE;
    if ( board->focusHasDived ) {
        XP_Bool up = XP_FALSE;
        if ( board->focussed == OBJ_BOARD ) {
            redraw = board_moveCursor( board, key, preflightOnly, &up );
        } else if ( board->focussed == OBJ_SCORE ) {
            redraw = moveScoreCursor( board, key, preflightOnly, &up );
        } else if ( board->focussed == OBJ_TRAY ) {
            redraw = tray_moveCursor( board, key, preflightOnly, &up );
        }
        if ( up ) {
            if ( !preflightOnly ) {
                invalFocusOwner( board );
                board->focusHasDived = XP_FALSE;
                invalFocusOwner( board );
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
board_handleKeyRepeat( BoardCtxt* board, XP_Key key, XP_Bool* handled )
{
    XP_Bool draw;

    if ( key == XP_RETURN_KEY ) {
        *handled = XP_FALSE;
        draw = XP_FALSE;
    } else {
        XP_Bool upHandled, downHandled;
        draw = board_handleKeyUp( board, key, &upHandled );
        draw = board_handleKeyDown( board, key, &downHandled ) || draw;
        *handled = upHandled || downHandled;
    }
    return draw;
}
#endif /* KEYBOARD_NAV */

#ifdef KEY_SUPPORT
XP_Bool
board_handleKeyDown( BoardCtxt* board, XP_Key key, XP_Bool* pHandled )
{
    XP_Bool draw = XP_FALSE;
#ifdef KEYBOARD_NAV
    XP_U16 x, y;

    *pHandled = XP_FALSE;

    if ( key == XP_RETURN_KEY ) {
        if ( focusToCoords( board, &x, &y ) ) {
            draw = handleLikeDown( board, board->focussed, x, y );
            *pHandled = draw;
        }
    } else if ( board->focussed != OBJ_NONE ) {
        if ( board->focusHasDived && (key == XP_RAISEFOCUS_KEY) ) {
            *pHandled = XP_TRUE;
        } else {
            draw = handleFocusKeyUp( board, key, XP_TRUE, pHandled ) || draw;
        }
    }
#endif
    return draw;
} /* board_handleKeyDown */

XP_Bool
board_handleKeyUp( BoardCtxt* board, XP_Key key, XP_Bool* pHandled )
{
    XP_Bool redraw = XP_FALSE;
    XP_Bool handled = XP_FALSE;
    XP_Bool trayVisible = board->trayVisState == TRAY_REVEALED;
    XP_Bool gotArrow;

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
        redraw = handleFocusKeyUp( board, key, XP_FALSE, &handled );
        break;
#endif

    case XP_CURSOR_KEY_DEL:
        if ( trayVisible ) {
            handled = redraw = replaceLastTile( board );
        }
        break;

#ifdef KEYBOARD_NAV
    case XP_RAISEFOCUS_KEY:
        if ( board->focussed != OBJ_NONE && board->focusHasDived ) {
            invalFocusOwner( board );
            board->focusHasDived = XP_FALSE;
            invalFocusOwner( board );
            handled = redraw = XP_TRUE;
        }
        break;

    case XP_RETURN_KEY:
        if ( board->focusHasDived ) {
            XP_U16 x, y;
            if ( focusToCoords( board, &x, &y ) ) {
                redraw = board_handlePenUp( board, x, y );
                handled = XP_TRUE;
            }
        } else if ( board->focussed != OBJ_NONE ) {
            redraw = invalFocusOwner( board );
            board->focusHasDived = XP_TRUE;
            redraw = invalFocusOwner( board );
            handled = XP_TRUE;
        }
        break;
#endif

    default:
        XP_ASSERT( key >= XP_KEY_LAST );

        handled = redraw = trayVisible
            && moveKeyTileToBoard( board, key, &gotArrow );

        if ( handled && gotArrow && !advanceArrow( board ) ) {
            setArrowVisible( board, XP_FALSE );
        }
    } /* switch */

    if ( !!pHandled ) {
        *pHandled = handled;
    }
    return redraw;
} /* board_handleKeyUp */

XP_Bool
board_handleKey( BoardCtxt* board, XP_Key key, XP_Bool* handled )
{
    XP_Bool handled1;
    XP_Bool handled2;
    XP_Bool draw;

    draw = board_handleKeyDown( board, key, &handled1 );
    draw = board_handleKeyUp( board, key, &handled2 ) || draw;
    if ( !!handled ) {
        *handled = handled1 || handled2;
    }

    return draw;
} /* board_handleKey */
#endif /* KEY_SUPPORT */

#ifdef KEYBOARD_NAV
static XP_Bool
invalFocusOwner( BoardCtxt* board )
{
    XP_Bool draw = XP_TRUE;
    XP_S16 selPlayer = board->selPlayer;
    switch( board->focussed ) {
    case OBJ_SCORE:
        board->scoreBoardInvalid = XP_TRUE;
        break;
    case OBJ_BOARD:
        if ( board->focusHasDived ) {
            BdCursorLoc loc = board->bdCursor[selPlayer];
            invalCell( board, loc.col, loc.row );
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
            XP_U16 loc = board->trayCursorLoc[selPlayer];
            board_invalTrayTiles( board, 1 << loc );
        } else {
            board_invalTrayTiles( board, ALLTILES );
            board->dividerInvalid = XP_TRUE;
        }
        break;
    case OBJ_NONE:
        draw = XP_FALSE;
        break;
    }
    board->needsDrawing = draw || board->needsDrawing;
    return draw;
} /* invalFocusOwner */

XP_Bool
board_focusChanged( BoardCtxt* board, BoardObjectType typ, XP_Bool gained )
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

       Problem: on palm at least take and lost are inverted: you get a take on
       the new object before a lose on the previous one.  So we want to ignore
       lost events *except* when it's a loss of something we have currently --
       meaning the focus is moving to soemthing we don't control (a
       platform-specific object)
    */

    if ( gained ) {
        /* Are we losing focus we currently have elsewhere? */
        if ( typ != board->focussed ) {
            draw = invalFocusOwner( board ) || draw;
        }
        board->focussed = typ;
        XP_LOGF( "%s: set focussed to %s", __func__, 
                 BoardObjectType_2str(typ) );
        board->focusHasDived = XP_FALSE;
        draw = invalFocusOwner( board ) || draw;
    } else {
        /* we're losing it; inval and clear IFF we currently have same focus,
           otherwise ignore */
        if ( typ == board->focussed ) {
            draw = invalFocusOwner( board ) || draw;
            board->focussed = OBJ_NONE;
        }
    }

    return draw;
} /* board_focusChanged */

XP_Bool
board_toggle_arrowDir( BoardCtxt* board )
{
    BoardArrow* arrow = &board->boardArrow[board->selPlayer];
    if ( arrow->visible ) {
        arrow->vert = !arrow->vert;
        invalArrowCell( board );
        return XP_TRUE;
    } else {
        return XP_FALSE;
    }
} /* board_toggle_cursorDir */

#endif /* KEYBOARD_NAV */

static XP_Bool
advanceArrow( BoardCtxt* board )
{
    XP_Key key = board->boardArrow[board->selPlayer].vert ?
        XP_CURSOR_KEY_DOWN :  XP_CURSOR_KEY_RIGHT;
	    
    XP_ASSERT( board->trayVisState == TRAY_REVEALED );

    return board_moveArrow( board, key );
} /* advanceArrow */

static XP_Bool
figureNextLoc( BoardCtxt* board, XP_Key cursorKey, 
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
            XP_LOGF( "%s: odd cursor key: %d", __func__, cursorKey ); 
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
board_moveArrow( BoardCtxt* board, XP_Key cursorKey )
{
    XP_U16 col, row;
    XP_Bool changed;

    setArrowVisible( board, XP_TRUE );
    (void)getArrow( board, &col, &row );
    changed = figureNextLoc( board, cursorKey, XP_TRUE, XP_FALSE, 
                             &col, &row, NULL );
    if ( changed ) {
        (void)setArrow( board, col, row );
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
board_moveCursor( BoardCtxt* board, XP_Key cursorKey, XP_Bool preflightOnly,
                  XP_Bool* up )
{
    BdCursorLoc loc = board->bdCursor[board->selPlayer];
    XP_U16 col = loc.col;
    XP_U16 row = loc.row;
    XP_Bool changed;

    XP_Bool altSet;
    cursorKey = stripAlt( cursorKey, &altSet );

    changed = figureNextLoc( board, cursorKey, XP_FALSE, altSet,
                             &col, &row, up );
    if ( changed && !preflightOnly ) {
        invalCell( board, loc.col, loc.row );
        invalCell( board, col, row );
        loc.col = col;
        loc.row = row;
        board->bdCursor[board->selPlayer] = loc;
        checkScrollCell( board, col, row );
    }
    return changed;
} /* board_moveCursor */
#endif

static XP_Bool
rectContainsRect( XP_Rect* rect1, XP_Rect* rect2 )
{
    return ( rect1->top <= rect2->top
             && rect1->left <= rect2->left
             && rect1->top + rect1->height >= rect2->top + rect2->height
             && rect1->left + rect1->width >= rect2->left + rect2->width );
} /* rectContainsRect */

XP_Bool
rectContainsPt( XP_Rect* rect, XP_S16 x, XP_S16 y )
{
    /* 7/4 Made <= into <, etc., because a tap on the right boundary of the
       board was still mapped onto the board but dividing by scale put it in
       the 15th column.  If this causes other problems and the '=' chars have
       to be put back then deal with that, probably by forcing an
       out-of-bounds col/row to the nearest possible. */
    return ( rect->top <= y
             && rect->left <= x
             && (rect->top + rect->height) >= y
             && (rect->left + rect->width) >= x );
} /* rectContainsPt */

XP_Bool
rectsIntersect( const XP_Rect* rect1, const XP_Rect* rect2 )
{
    if ( rect1->top >= rect2->top + rect2->height ) {
        return XP_FALSE;
    } else if ( rect1->left >= rect2->left + rect2->width ) {
        return XP_FALSE;
    } else if ( rect2->top >= rect1->top + rect1->height ) {
        return XP_FALSE;
    } else if ( rect2->left >= rect1->left + rect1->width ) {
        return XP_FALSE;
    } else {
        return XP_TRUE;
    }
} /* rectsIntersect */

static XP_Bool
replaceLastTile( BoardCtxt* board )
{
    XP_Bool result = XP_FALSE;
    XP_U16 tilesInMove = model_getCurrentMoveCount( board->model, 
                                                    board->selPlayer );

    if ( tilesInMove > 0 ) {
        XP_U16 col, row;
        Tile tile;
        XP_Bool isBlank;
        XP_S16 index;

        index = -1;
        model_getCurrentMoveTile( board->model, board->selPlayer, &index,
                                  &tile, &col, &row, &isBlank );

        model_moveBoardToTray( board->model, board->selPlayer, index );

        flipIf( board, col, row, &col, &row );
        setArrow( board, col, row );

        result = XP_TRUE;
    }

    return result;
} /* replaceLastTile */

static XP_Bool
moveTileToBoard( BoardCtxt* board, XP_U16 col, XP_U16 row, XP_U16 tileIndex,
		 Tile blankFace )
{
    if ( cellOccupied( board, col, row, XP_TRUE ) ) {
        return XP_FALSE;
    }

    flipIf( board, col, row, &col, &row );
    model_moveTrayToBoard( board->model, board->selPlayer, col, row, 
                           tileIndex, blankFace );

    return XP_TRUE;
} /* moveTileToBoard */

#ifdef KEY_SUPPORT
static XP_Bool
moveKeyTileToBoard( BoardCtxt* board, XP_Key cursorKey, XP_Bool* gotArrow )
{
    /* keep compiler happy: assign defaults */
    Tile tile, blankFace = EMPTY_TILE; /* make compiler happy */
    XP_U16 col, row;
    DictionaryCtxt* dict = model_getDictionary( board->model );
    XP_S16 turn = board->selPlayer;
    XP_S16 tileIndex;
    XP_UCHAR buf[2];
    XP_Bool success;

    /* Is there a cursor at all? */
    *gotArrow = success = getArrow( board, &col, &row );
#ifdef KEYBOARD_NAV
    if ( !success && (board->focussed == OBJ_BOARD) && board->focusHasDived ) {
        BdCursorLoc loc = board->bdCursor[turn];
        col = loc.col;
        row = loc.row;
        success = XP_TRUE;
    }
#endif

    if ( success ) {
        XP_ASSERT( !TRADE_IN_PROGRESS(board) );

        /* Figure out if we have the tile in the tray  */
        buf[0] = cursorKey;
        buf[1] = '\0';
        tile = dict_tileForString( dict, buf );
        if ( tile == EMPTY_TILE ) { /* not found in dict */
            success = XP_FALSE;
        }
    }

    if ( success ) {
        tileIndex = model_trayContains( board->model, turn, tile );
        if ( tileIndex >= 0 ) {
            // blankFace = EMPTY_TILE;	/* already set (and will be ignored) */
        } else {
            Tile blankTile = dict_getBlankTile( dict );
            tileIndex = model_trayContains( board->model, turn, blankTile );
            if ( tileIndex >= 0 ) {	/* there's a blank for it */
                blankFace = tile;
            } else {
                success = XP_FALSE;
            }
        }
    }

    if ( success ) {
        success = moveTileToBoard( board, col, row, tileIndex, blankFace );
    }

    return success;
} /* moveKeyTileToBoard */
#endif

static void
setArrowFor( BoardCtxt* board, XP_U16 player, XP_U16 col, XP_U16 row )
{
    BoardArrow* arrow = &board->boardArrow[player];
    invalCell( board, arrow->col, arrow->row );
    invalCell( board, col, row );

    arrow->col = (XP_U8)col;
    arrow->row = (XP_U8)row;

    checkScrollCell( board, col, row );
} /* setArrowFor */

static void
setArrow( BoardCtxt* board, XP_U16 col, XP_U16 row )
{
    setArrowFor( board, board->selPlayer, col, row );
} /* setArrow */

static XP_Bool
getArrowFor( BoardCtxt* board, XP_U16 player, XP_U16* col, XP_U16* row )
{
    BoardArrow* arrow = &board->boardArrow[player];
    *col = arrow->col;
    *row = arrow->row;
    return arrow->visible;
} /* getArrowFor */

static XP_Bool
getArrow( BoardCtxt* board, XP_U16* col, XP_U16* row )
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
    BoardArrow* arrow = &board->boardArrow[player];
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
boardCellChanged( void* p_board, XP_U16 turn, XP_U16 modelCol, XP_U16 modelRow,
                  XP_Bool added )
{
    BoardCtxt* board = (BoardCtxt*)p_board;
    XP_Bool pending, found, ignoreBlank;
    Tile ignoreTile;
    XP_U16 col, row;

    flipIf( board, modelCol, modelRow, &col, &row );

    /* for each player, check if the tile overwrites the cursor */
    found = model_getTile( board->model, modelCol, modelRow, XP_TRUE, turn,
                           &ignoreTile, &ignoreBlank, &pending, 
                           (XP_Bool*)NULL );

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

        checkScrollCell( board, col, row );
    }

    invalCell( (BoardCtxt*)p_board, col, row );
} /* boardCellChanged */

static void
boardTileChanged( void* p_board, XP_U16 turn, TileBit bits )
{
    BoardCtxt* board = (BoardCtxt*)p_board;
    if ( turn == board->selPlayer ) {
        board_invalTrayTiles( board, bits );
    }
} /* boardTileChanged */

static void
boardTurnChanged( void* p_board )
{
    BoardCtxt* board = (BoardCtxt*)p_board;
    XP_S16 nextPlayer;

    XP_ASSERT( board->timerSaveCount == 0 );

    board->gameOver = XP_FALSE;

    nextPlayer = chooseBestSelPlayer( board );
    if ( nextPlayer >= 0 ) {
        XP_U16 nHumans = gi_countLocalHumans( board->gi );
        selectPlayerImpl( board, nextPlayer, nHumans <= 1 );
    }

    setTimerIf( board );

    board->scoreBoardInvalid = XP_TRUE;
} /* boardTurnChanged */

static void
boardGameOver( void* closure )
{
    BoardCtxt* board = (BoardCtxt*)closure;    
    board->scoreBoardInvalid = XP_TRUE;	/* not sure if this will do it. */
    board->gameOver = XP_TRUE;
    util_notifyGameOver( board->util );
} /* boardGameOver */

#ifdef CPLUS
}
#endif
