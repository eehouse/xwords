/* -*- fill-column: 78; compile-command: "cd ../linux && make -j3 MEMDEBUG=TRUE"; -*- */
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

#define bEND 0x62454e44

#ifdef CPLUS
extern "C" {
#endif

/****************************** prototypes ******************************/
static void figureBoardRect( BoardCtxt* board );
static void forceRectToBoard( const BoardCtxt* board, XP_Rect* rect );

static void boardCellChanged( void* board, XP_U16 turn, XP_U16 col, 
                              XP_U16 row, XP_Bool added );
static void boardTilesChanged( void* board, XP_U16 turn, XP_S16 index1, 
                               XP_S16 index2 );
static void dictChanged( void* p_board, const DictionaryCtxt* oldDict, 
                         const DictionaryCtxt* newDict );

static void boardTurnChanged( void* board );
static void boardGameOver( void* board );
static void setArrow( BoardCtxt* board, XP_U16 row, XP_U16 col );
static void setArrowFor( BoardCtxt* board, XP_U16 player, XP_U16 col, 
                         XP_U16 row );
static XP_Bool setArrowVisible( BoardCtxt* board, XP_Bool visible );

static void invalTradeWindow( BoardCtxt* board, XP_S16 turn, XP_Bool redraw );
static void setTimerIf( BoardCtxt* board );
static XP_Bool p_board_timerFired( void* closure, XWTimerReason why );

static XP_Bool replaceLastTile( BoardCtxt* board );
static XP_Bool setTrayVisState( BoardCtxt* board, XW_TrayVisState newState );
static XP_Bool advanceArrow( BoardCtxt* board );
static XP_Bool exitTradeMode( BoardCtxt* board );

static XP_Bool getArrow( BoardCtxt* board, XP_U16* col, XP_U16* row );
static XP_Bool setArrowVisibleFor( BoardCtxt* board, XP_U16 player, 
                                   XP_Bool visible );
static XP_Bool board_moveArrow( BoardCtxt* board, XP_Key cursorKey );

#ifdef KEY_SUPPORT
static XP_Bool moveKeyTileToBoard( BoardCtxt* board, XP_Key cursorKey,
                                   XP_Bool* gotArrow );
static XP_S16 keyToIndex( BoardCtxt* board, XP_Key key, Tile* blankFace );
#endif

#ifdef KEYBOARD_NAV
static XP_Bool board_moveCursor( BoardCtxt* board, XP_Key cursorKey, 
                                 XP_Bool preflightOnly, XP_Bool* up );
static XP_Bool invalFocusOwner( BoardCtxt* board );
#else
# define invalFocusOwner(board)
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
        result->selInfo = result->pti; /* equates to selPlayer == 0 */

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
        model_setTrayListener( model, boardTilesChanged, result );
        model_setDictListener( model, dictChanged, result );
        server_setTurnChangeListener( server, boardTurnChanged, result );
        server_setGameOverListener( server, boardGameOver, result );

        setTimerIf( result );

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
        board->scoreCursorLoc = (XP_U8)
            stream_getBits( stream, (version < STREAM_VERS_MODEL_NO_DICT? 2:3));
    }
#endif

    XP_ASSERT( !!server );

    for ( i = 0; i < nPlayers; ++i ) {
        PerTurnInfo* pti = &board->pti[i];
        BoardArrow* arrow = &pti->boardArrow;
        arrow->col = (XP_U8)stream_getBits( stream, NUMCOLS_NBITS );
        arrow->row = (XP_U8)stream_getBits( stream, NUMCOLS_NBITS );
        arrow->vert = (XP_Bool)stream_getBits( stream, 1 );
        arrow->visible = (XP_Bool)stream_getBits( stream, 1 );

        pti->dividerLoc = (XP_U8)stream_getBits( stream, NTILES_NBITS );
        pti->traySelBits = (TileBit)stream_getBits( stream, 
                                                    MAX_TRAY_TILES );
        pti->tradeInProgress = (XP_Bool)stream_getBits( stream, 1 );
#ifdef KEYBOARD_NAV 
        if ( version >= STREAM_VERS_KEYNAV ) {
            pti->bdCursor.col = stream_getBits( stream, 4 );
            pti->bdCursor.row = stream_getBits( stream, 4 );
            pti->trayCursorLoc = stream_getBits( stream, 3 );
        }
#endif

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
    stream_putBits( stream, 3, board->scoreCursorLoc );
#endif

    XP_ASSERT( !!board->server );
    nPlayers = board->gi->nPlayers;

    for ( i = 0; i < nPlayers; ++i ) {
        PerTurnInfo* pti = &board->pti[i];
        BoardArrow* arrow = &pti->boardArrow;
        stream_putBits( stream, NUMCOLS_NBITS, arrow->col );
        stream_putBits( stream, NUMCOLS_NBITS, arrow->row );
        stream_putBits( stream, 1, arrow->vert );
        stream_putBits( stream, 1, arrow->visible );

        stream_putBits( stream, NTILES_NBITS, pti->dividerLoc );
        stream_putBits( stream, MAX_TRAY_TILES, pti->traySelBits );
        stream_putBits( stream, 1, pti->tradeInProgress );
#ifdef KEYBOARD_NAV 
        stream_putBits( stream, 4, pti->bdCursor.col );
        stream_putBits( stream, 4, pti->bdCursor.row );
        stream_putBits( stream, 3, pti->trayCursorLoc );
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
        PerTurnInfo* pti = &board->pti[i];
        pti->traySelBits = 0;
        pti->tradeInProgress = XP_FALSE;
        pti->dividerLoc = 0;
        XP_MEMSET( &pti->boardArrow, 0, sizeof(pti->boardArrow) );
    }
    board->gameOver = XP_FALSE;
    board->selPlayer = 0;
    board->selInfo = board->pti;
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
    board->skipCommitConfirm = cp->skipCommitConfirm;

    if ( showArrowChanged ) {
        showArrowChanged = setArrowVisible( board, XP_FALSE );
    }
    if ( hideValChanged ) {
        board_invalTrayTiles( board, ALLTILES );
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

    return showArrowChanged || hideValChanged;
} /* board_prefsChanged */

XP_Bool
adjustYOffset( BoardCtxt* board, XP_S16 moveBy )
{
    XP_U16 nVisible = board->lastVisibleRow - board->yOffset + 1;
    XP_U16 nRows = model_numRows(board->model);
    XP_S16 newOffset = board->yOffset - moveBy;

    if ( newOffset < 0 ) {
        newOffset = 0;
    } else if ( newOffset + nVisible > nRows ) {
        newOffset = nRows - nVisible;
    }

    return board_setYOffset( board, newOffset );
} /* adjustYOffset */

XP_Bool
board_setYOffset( BoardCtxt* board, XP_U16 offset )
{
    XP_U16 oldOffset = board->yOffset;
    XP_Bool result = oldOffset != offset;

    if ( result ) {
        XP_U16 nVisible = board->lastVisibleRow - board->yOffset + 1;
        XP_U16 nRows = model_numRows(board->model);

        result = offset <= nRows - nVisible;
        if ( result ) {
            /* check if scrolling makes sense for this board in its current
               state. */
            XP_U16 visibleHeight = board->boardBounds.height;
            XP_U16 fullHeight = nRows * board->boardVScale;
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
    }

    return result;
} /* board_setYOffset */

XP_U16
board_getYOffset( const BoardCtxt* board )
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
    const XP_S16 turn = server_getCurrentTurn( board->server );
    PerTurnInfo* pti = board->pti + turn;

    if ( board->gameOver || turn < 0 ) {
        /* do nothing */
    } else if ( turn != board->selPlayer ) {
        if ( board->selInfo->tradeInProgress ) {
            result = exitTradeMode( board );
        } else {
            util_userError( board->util, ERR_NOT_YOUR_TURN );
        }
    } else if ( checkRevealTray( board ) ) {
        if ( pti->tradeInProgress ) {
            result = XP_TRUE; /* there's at least the window to clean up
                                 after */

            invalSelTradeWindow( board );
            pti->tradeInProgress = XP_FALSE;

            if ( util_userQuery( board->util, QUERY_COMMIT_TRADE,
                                 (XWStreamCtxt*)NULL ) ) {
                result = server_commitTrade( board->server, 
                                             pti->traySelBits );
                /* XP_DEBUGF( "server_commitTrade returned %d\n", result ); */
            }
            pti->traySelBits = 0x00;
        } else {
            XP_Bool warn, legal;
            WordNotifierInfo info;
            XWStreamCtxt* stream = 
                mem_stream_make( MPPARM(board->mpool) 
                                 util_getVTManager(board->util), NULL,
                                 CHANNEL_NONE, (MemStreamCloseCallback)NULL );

            const XP_UCHAR* str = util_getUserString(board->util, 
                                                     STR_COMMIT_CONFIRM);
            stream_catString( stream, str );
            
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
                if ( gi_countLocalHumans( board->gi ) > 1 ) {
                    result = board_hideTray( board );
                }

                if ( board->skipCommitConfirm
                     || util_userQuery( board->util, QUERY_COMMIT_TURN,
                                        stream ) ) {
                    result = server_commitMove( board->server ) || result;
                    /* invalidate any selected tiles in case we'll be drawing
                       this tray again rather than some other -- as is the
                       case in a two-player game where one's a robot. */
                    board_invalTrayTiles( board, pti->traySelBits );
                    pti->traySelBits = 0x00;
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
        PerTurnInfo* newInfo = &board->pti[newPlayer];
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
        if ( newInfo->tradeInProgress && 
             server_countTilesInPool(board->server) < MIN_TRADE_TILES ) {
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

        setTrayVisState( board, TRAY_REVERSED );
    }
    board->scoreBoardInvalid = XP_TRUE; /* if only one player, number of
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
    forceRectToBoard( board, rect );
} /*positionMiniWRect */

static void
timerFiredForPen( BoardCtxt* board ) 
{
    const XP_UCHAR* text = (XP_UCHAR*)NULL;
    XP_UCHAR buf[80];

    if ( board->penDownObject == OBJ_BOARD ) {
        if ( !dragDropInProgress( board ) || !dragDropHasMoved( board ) ) {
            XP_U16 col, row;
            XWBonusType bonus;

            coordToCell( board, board->penDownX, board->penDownY, &col, 
                         &row );
            bonus = util_getSquareBonus(board->util, board->model, col, row);
            if ( bonus != BONUS_NONE ) {
                text = draw_getMiniWText( board->draw, (XWMiniTextType)bonus );
            }
            board->penTimerFired = XP_TRUE;
        }
    } else if ( board->penDownObject == OBJ_SCORE ) {
        LocalPlayer* lp;
        XP_S16 scoreIndex = figureScoreRectTapped( board, board->penDownX, 
                                                   board->penDownY );
        /* I've seen this assert fire on simulator.  No log is kept so I can't
           tell why, but might want to test and do nothing in this case.  */
        /* XP_ASSERT( player >= 0 ); */
        if ( scoreIndex > CURSOR_LOC_REM ) {
            XP_U16 player = scoreIndex - 1;
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

static XP_Bool
p_board_timerFired( void* closure, XWTimerReason why )
{
    BoardCtxt* board = (BoardCtxt*)closure;
    if ( why == TIMER_PENDOWN ) {
        timerFiredForPen( board );
    } else {
        XP_ASSERT( why == TIMER_TIMERTICK );
        timerFiredForTimer( board );
    }
    return XP_FALSE;
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
invalPerimeter( BoardCtxt* board )
{
    XP_U16 lastCol = model_numCols( board->model ) - 1;
    XP_U16 firstAndLast = (1 << lastCol) | 1;
    XP_U16 firstRow = board->yOffset;
    XP_U16 lastRow = board->lastVisibleRow;

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
    XP_U16 i;
    for ( i = 0; i < nPlayers; ++i ) {
        flipLimits( &board->pti[i].limits );
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

XP_Bool
checkScrollCell( BoardCtxt* board, XP_U16 col, XP_U16 row )
{
    XP_Rect rect;
    XP_Bool moved = XP_FALSE;

    if ( board->boardObscuresTray && board->trayVisState != TRAY_HIDDEN ) {
        /* call getCellRect until the cell's on the board. */
        while ( !getCellRect( board, col, row, &rect ) ) {
            XP_S16 moveBy = 1;
            if ( rect.top < board->boardBounds.top ) {
                /* do nothing; set to 1 above to prevent warning */
            } else if ( rect.top + rect.height > 
                        board->boardBounds.top + board->boardBounds.height ) {
                moveBy = -1;
            } else {
                XP_ASSERT( 0 );
            }
            moved = adjustYOffset( board, moveBy );
            XP_ASSERT( moved );
        }
    }    
    return moved;
} /* checkScrollCell */

XP_Bool
onBorderCanScroll( const BoardCtxt* board, XP_U16 row, XP_S16* changeP )
{
    XP_Bool result;
    XP_S16 change = 0;
    XP_U16 yOffset = board_getYOffset( board );

    if ( yOffset > 0 && row == yOffset  ) {
        change = -yOffset;
    } else if ( row == board->lastVisibleRow ) {
        XP_U16 lastRow = model_numRows(board->model) - 1;
        change = lastRow - row;
    }

    result = change != 0;
    if ( result ) {
        *changeP = change;
    }
    return result;
}

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

static XP_Bool
trayOnTop( const BoardCtxt* board )
{
    /* The tray should be drawn on top of the board IFF it's not HIDDEN or if
       it has non-dived focus. */
    return (board->trayVisState != TRAY_HIDDEN)
        || ( (board->focussed == OBJ_TRAY)
             && (board->focusHasDived == XP_FALSE));
} /* trayOnTop */

XW_TrayVisState
board_getTrayVisState( const BoardCtxt* board )
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
        (void)invalFocusOwner( board ); /* must be done before and after rect
                                           recalculated */
        figureBoardRect( board ); /* comes before setYOffset since that
                                     uses rects to calc scroll */
        (void)invalFocusOwner( board );

        if ( board->boardObscuresTray ) {
            if ( nowHidden && !trayOnTop(board) ) { 
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

        nVisible = board->lastVisibleRow - board->yOffset + 1;
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
board_get_flipped( const BoardCtxt* board )
{
    return board->isFlipped;
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
board_get_showValues( const BoardCtxt* board )
{
    return board->showCellValues;
}

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
    return !board->gameOver
        && server_getCurrentTurn(board->server) >= 0
        && checkRevealTray( board ) 
        && !TRADE_IN_PROGRESS(board);
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
    XP_Bool result = XP_FALSE;
    XP_Bool redraw = XP_FALSE;

    *workRemainsP = XP_FALSE; /* in case we exit without calling engine */

    if ( board->gi->hintsNotAllowed ) {
        util_userError( board->util, ERR_CANT_HINT_WHILE_DISABLED );
    } else {
        MoveInfo newMove;
        XP_S16 nTiles;
        const Tile* tiles;
        XP_Bool searchComplete = XP_TRUE;
        const XP_U16 selPlayer = board->selPlayer;
        PerTurnInfo* pti = board->selInfo;
        EngineCtxt* engine = server_getEngineFor( board->server, selPlayer );
        const TrayTileSet* tileSet;
        ModelCtxt* model = board->model;

        if ( !!engine && preflight( board ) ) {

            /* undo any current move.  otherwise we won't pass the full tray
               to the engine.  Would it be better, though, to pass the whole
               tray regardless where its contents are? */
            if ( model_getCurrentMoveCount( model, selPlayer ) > 0 ) {
                model_resetCurrentTurn( model, selPlayer );
                /* Draw's a no-op on Wince with a null hdc, but it'll draw again.
                   Should probably define OS_INITS_DRAW on Wince...*/
#ifdef OS_INITS_DRAW
                /* On symbian, it's illegal to draw except from inside the
                   Draw method.  But the move search will probably be so fast
                   that it's ok to wait until we've found the move anyway. */
                redraw = XP_TRUE;
#else
                board_draw( board );
#endif
            }

            tileSet = model_getPlayerTiles( model, selPlayer );
            nTiles = tileSet->nTiles - pti->dividerLoc;
            result = nTiles > 0;
        }

        if ( result ) {
#ifdef XWFEATURE_SEARCHLIMIT
            BdHintLimits limits;
            BdHintLimits* lp = NULL;
#endif
            XP_Bool wasVisible;
            XP_Bool canMove;

            wasVisible = setArrowVisible( board, XP_FALSE );

            (void)board_replaceTiles( board );

            tiles = tileSet->tiles + pti->dividerLoc;

            board_pushTimerSave( board );

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
            if ( trayOnTop( board ) ) {
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
        board->lastVisibleRow = nVisible + board->yOffset - 1;

        board->boardBounds = boardBounds;
    }
} /* figureBoardRect */

XP_Bool
coordToCell( BoardCtxt* board, XP_S16 xx, XP_S16 yy, XP_U16* colP, 
             XP_U16* rowP )
{
    XP_U16 col, row, max;
    XP_Bool onBoard = XP_TRUE;

    xx -= board->boardBounds.left;

    yy -= board->boardBounds.top;
    yy += board->boardVScale * board->yOffset;

    col = xx / board->boardHScale;
    row = yy / board->boardVScale;

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

XP_Bool
getCellRect( const BoardCtxt* board, XP_U16 col, XP_U16 row, XP_Rect* rect )
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

void
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
XP_Bool
pointOnSomething( BoardCtxt* board, XP_U16 xx, XP_U16 yy, BoardObjectType* wp )
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
    BoardArrow* arrow = &board->selInfo->boardArrow;
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
#endif

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
        } else if ( server_countTilesInPool(board->server) < MIN_TRADE_TILES){
            util_userError( board->util, ERR_TOO_FEW_TILES_LEFT_TO_TRADE );
        } else {
            board->tradingMiniWindowInvalid = XP_TRUE;
            board->needsDrawing = XP_TRUE;
            board->selInfo->tradeInProgress = XP_TRUE;
            setArrowVisible( board, XP_FALSE );
            result = XP_TRUE;
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
handlePenDownOnBoard( BoardCtxt* board, XP_U16 xx, XP_U16 yy )
{
    XP_Bool result = XP_FALSE;

    if ( TRADE_IN_PROGRESS(board) && ptOnTradeWindow( board, xx, yy ) ) {
        /* do nothing */
    } else {
        util_setTimer( board->util, TIMER_PENDOWN, 0, 
                       p_board_timerFired, board );

        if ( !board->selInfo->tradeInProgress ) {
            result = dragDropStart( board, OBJ_BOARD, xx, yy );
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
      if ( (board->trayVisState == TRAY_REVEALED)
           && !board->selInfo->tradeInProgress ) {
            result = dragDropStart( board, OBJ_TRAY, x, y ) || result;
        }
        break;

    case OBJ_SCORE:
        if ( figureScoreRectTapped( board, x, y ) > CURSOR_LOC_REM ) {
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

    board->srcIsPen = XP_TRUE; 

    penDidSomething = pointOnSomething( board, x, y, &onWhich );

    if ( !penDidSomething ) {
        board->penDownObject = OBJ_NONE;
    } else {

#ifdef KEYBOARD_NAV
        /* clear focus as soon as pen touches board */
        result = invalFocusOwner( board );
        board->hideFocus = XP_TRUE;
        if ( board->boardObscuresTray ) {
            figureBoardRect( board );
        }
#endif

        result = handleLikeDown( board, onWhich, x, y ) || result;
    }
    *handled = penDidSomething;

    return result;
} /* board_handlePenDown */
#endif

XP_Bool
board_handlePenMove( BoardCtxt* board, XP_U16 xx, XP_U16 yy )
{
    XP_Bool result = dragDropInProgress(board)
        && dragDropContinue( board, xx, yy );
    return result;
} /* board_handlePenMove */

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

XP_Bool
cellOccupied( const BoardCtxt* board, XP_U16 col, XP_U16 row, 
              XP_Bool inclPending )
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

XP_Bool
holdsPendingTile( BoardCtxt* board, XP_U16 pencol, XP_U16 penrow )
{
    Tile tile;
    XP_Bool ignore, isPending;
    XP_U16 modcol, modrow;
    flipIf( board, pencol, penrow, &modcol, &modrow );

    return model_getTile( board->model, modcol, modrow, XP_TRUE,
                          board->selPlayer, &tile, &ignore, &isPending, 
                          (XP_Bool*)NULL )
        && isPending;
} /* holdsPendingTile */

/* Did I tap on a tile on the board that I have not yet committed?  If so,
 * return it to the tray.  But don't do this in drag-and-drop case since it's
 * too easy to accidentally tap and there are better ways.
 */
static XP_Bool
tryReplaceTile( BoardCtxt* board, XP_U16 pencol, XP_U16 penrow )
{
    XP_Bool result = XP_FALSE;

    if ( holdsPendingTile( board, pencol, penrow ) ) {
        XP_U16 modcol, modrow;
        flipIf( board, pencol, penrow, &modcol, &modrow );

        model_moveBoardToTray( board->model, board->selPlayer, 
                               modcol, modrow, -1 );
        setArrow( board, pencol, penrow );
        result = XP_TRUE;

    }
    return result;
} /* tryReplaceTile */

static XP_Bool
handleActionInCell( BoardCtxt* board, XP_U16 col, XP_U16 row, XP_Bool isPen )
{
    return moveSelTileToBoardXY( board, col, row )
        || tryMoveArrow( board, col, row )
        || (!isPen && tryReplaceTile( board, col, row ))
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
handlePenUpInternal( BoardCtxt* board, XP_U16 xx, XP_U16 yy, XP_Bool isPen )
{
    XP_Bool draw = XP_FALSE;
    XP_Bool dragged = XP_FALSE;
    BoardObjectType prevObj = board->penDownObject;

    /* prevent timer from firing after pen lifted.  Set now rather than later
       in case we put up a modal alert/dialog that must be dismissed before
       exiting this function (which might give timer time to fire. */
    board->penDownObject = OBJ_NONE;

    if ( dragDropInProgress(board) ) {
        draw = dragDropEnd( board, xx, yy, &dragged );
    }
    if ( dragged ) {
        /* do nothing further */
    } else if ( board->penTimerFired ) {
        if ( valHintMiniWindowActive( board ) ) {
            hideMiniWindow( board, XP_TRUE, MINIWINDOW_VALHINT );
        }
        draw = XP_TRUE;         /* might have cancelled a drag */
        /* Need to clean up if there's been any dragging happening */
        board->penTimerFired = XP_FALSE;
    } else {
        BoardObjectType onWhich;
        if ( pointOnSomething( board, xx, yy, &onWhich ) ) {

            switch( onWhich ) {
            case OBJ_SCORE:
                if ( prevObj == OBJ_SCORE ) {
                    draw = handlePenUpScore( board, xx, yy ) || draw;
                }
                break;
            case OBJ_BOARD:
                if ( prevObj == OBJ_BOARD && checkRevealTray(board) ) {

                    if ( TRADE_IN_PROGRESS(board) ) {
                        if ( ptOnTradeWindow( board, xx, yy )) {
                            draw = exitTradeMode( board ) || draw;
                        }
                    } else {
                        XP_U16 col, row;
                        coordToCell( board, board->penDownX, board->penDownY,
                                     &col, &row );
                        draw = handleActionInCell( board, col, row, 
                                                   isPen ) || draw;
                    }
                }
                break;
            case OBJ_TRAY:
                if ( board->trayVisState != TRAY_REVEALED ) {
                    draw = askRevealTray( board ) || draw;
                } else {
                    draw = handlePenUpTray( board, xx, yy ) || draw;
                }
                break;
            default:
                XP_ASSERT( XP_FALSE );
            }
        }
    }

    return draw;
} /* handlePenUpInternal */

XP_Bool
board_handlePenUp( BoardCtxt* board, XP_U16 x, XP_U16 y )
{
    return handlePenUpInternal( board, x, y, XP_TRUE );
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
        } else if ( board->focussed == OBJ_TRAY/* && checkRevealTray(board)*/ ) {
            redraw = tray_moveCursor( board, key, preflightOnly, &up );
        }
        if ( up ) {
            if ( !preflightOnly ) {
                (void)invalFocusOwner( board );
                board->focusHasDived = XP_FALSE;
                (void)invalFocusOwner( board );
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

static XP_Bool
unhideFocus( BoardCtxt* board )
{
    XP_Bool changing = board->hideFocus;
    if ( changing ) {
        board->hideFocus = XP_FALSE;
        (void)invalFocusOwner( board );
    }
    return changing;
}
#endif /* KEYBOARD_NAV */

#ifdef KEY_SUPPORT
XP_Bool
board_handleKeyDown( BoardCtxt* board, XP_Key key, XP_Bool* pHandled )
{
    XP_Bool draw = XP_FALSE;
#ifdef KEYBOARD_NAV
    XP_U16 x, y;

    board->srcIsPen = XP_FALSE;

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
        if ( unhideFocus( board ) ) {
            redraw = handled = XP_TRUE;
        } else {
            redraw = handleFocusKeyUp( board, key, XP_FALSE, &handled );
        }
        break;
#endif

    case XP_CURSOR_KEY_DEL:
        if ( trayVisible ) {
            handled = redraw = replaceLastTile( board );
        }
        break;

#ifdef KEYBOARD_NAV
    case XP_RAISEFOCUS_KEY:
        if ( unhideFocus( board ) ) {
            /* do nothing */
        } else if ( board->focussed != OBJ_NONE && board->focusHasDived ) {
            (void)invalFocusOwner( board );
            board->focusHasDived = XP_FALSE;
            (void)invalFocusOwner( board );
        } else {
            break;              /* skip setting handled */
        }
        handled = redraw = XP_TRUE;
        break;

    case XP_RETURN_KEY:
        if ( unhideFocus( board ) ) {
            handled = XP_TRUE;
        } else if ( board->focussed != OBJ_NONE ) {
            if ( board->focusHasDived ) {
                XP_U16 xx, yy;
                if ( focusToCoords( board, &xx, &yy ) ) {
                    redraw = handlePenUpInternal( board, xx, yy, XP_FALSE );
                    handled = XP_TRUE;
                }
            } else {
                (void)invalFocusOwner( board );
                board->focusHasDived = XP_TRUE;
                redraw = invalFocusOwner( board );
                handled = XP_TRUE;
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
                handled = moveKeyTileToBoard( board, key, &gotArrow );
                if ( handled && gotArrow && !advanceArrow( board ) ) {
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
    PerTurnInfo* pti = board->selInfo;
    switch( board->focussed ) {
    case OBJ_SCORE:
        board->scoreBoardInvalid = XP_TRUE;
        break;
    case OBJ_BOARD:
        if ( board->focusHasDived ) {
            BdCursorLoc loc = pti->bdCursor;
            invalCell( board, loc.col, loc.row );
            checkScrollCell( board, loc.col, loc.row );
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
            if ( loc == pti->dividerLoc ) {
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
            draw = board_focusChanged( board, board->focussed, XP_FALSE );
        }

        /* Are we losing focus we currently have elsewhere? */
        if ( typ != board->focussed ) {
            draw = invalFocusOwner( board ) || draw;
        }
        board->focussed = typ;
        board->focusHasDived = XP_FALSE;
        if ( OBJ_TRAY == typ) {
            board->trayHiddenPreFocus = board->trayVisState == TRAY_HIDDEN;
            if ( board->trayHiddenPreFocus ) {
                setTrayVisState( board, TRAY_REVERSED );
            }
        }
        draw = invalFocusOwner( board ) || draw;
    } else {
        /* we're losing it; inval and clear IFF we currently have same focus,
           otherwise ignore */
        if ( typ == board->focussed ) {
            draw = invalFocusOwner( board ) || draw;
            board->focussed = OBJ_NONE;

            if ( (OBJ_TRAY == typ) && (board->trayVisState == TRAY_REVERSED)
                 && board->trayHiddenPreFocus ) {
                setTrayVisState( board, TRAY_HIDDEN );
            }
        }
    }

    if ( draw ) {
        figureBoardRect( board );
    }

    return draw;
} /* board_focusChanged */

XP_Bool
board_toggle_arrowDir( BoardCtxt* board )
{
    BoardArrow* arrow = &board->selInfo->boardArrow;
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
    XP_Key key = board->selInfo->boardArrow.vert ?
        XP_CURSOR_KEY_DOWN :  XP_CURSOR_KEY_RIGHT;

    XP_ASSERT( board->trayVisState == TRAY_REVEALED );

    return board_moveArrow( board, key );
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
        checkScrollCell( board, col, row );
    }
    return changed;
} /* board_moveCursor */
#endif

XP_Bool
rectContainsPt( const XP_Rect* rect, XP_S16 x, XP_S16 y )
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
        model_moveBoardToTray( board->model, board->selPlayer, col, row, -1 );

        flipIf( board, col, row, &col, &row );
        setArrow( board, col, row );

        result = XP_TRUE;
    }

    return result;
} /* replaceLastTile */

XP_Bool
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
        DictionaryCtxt* dict = model_getDictionary( model );
        Tile tile;
        XP_UCHAR buf[2] = { key, '\0' };

        /* Figure out if we have the tile in the tray  */
        tile = dict_tileForString( dict, buf );
        if ( tile != EMPTY_TILE ) { /* in dict? */
            XP_S16 turn = board->selPlayer;
            tileIndex = model_trayContains( model, turn, tile );
            if ( tileIndex < 0 ) {
                Tile blankTile = dict_getBlankTile( dict );
                tileIndex = model_trayContains( model, turn, blankTile );
                if ( tileIndex >= 0 && !!blankFace ) { /* there's a blank for it */
                    *blankFace = tile;
                }
            }
        }
    }

    return tileIndex;
} /* keyToIndex */

static XP_Bool
moveKeyTileToBoard( BoardCtxt* board, XP_Key cursorKey, XP_Bool* gotArrow )
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
            && moveTileToBoard( board, col, row, tileIndex, blankFace );
    }

    return haveDest;
} /* moveKeyTileToBoard */
#endif  /* #ifdef KEY_SUPPORT */

static void
setArrowFor( BoardCtxt* board, XP_U16 player, XP_U16 col, XP_U16 row )
{
    BoardArrow* arrow = &board->pti[player].boardArrow;
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
    BoardArrow* arrow = &board->pti[player].boardArrow;
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
boardTilesChanged( void* p_board, XP_U16 turn, XP_S16 index1, XP_S16 index2 )
{
    BoardCtxt* board = (BoardCtxt*)p_board;
    if ( turn == board->selPlayer ) {
        invalTrayTilesBetween( board, index1, index2 );
    }
} /* boardTilesChanged */

static void
dictChanged( void* p_board, const DictionaryCtxt* oldDict, 
             const DictionaryCtxt* newDict )
{
    BoardCtxt* board = (BoardCtxt*)p_board;
    XP_ASSERT( !!board->draw );
    if ( (NULL == oldDict) || (oldDict != newDict) ) {
        draw_dictChanged( board->draw, newDict );
    }
}

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

    util_turnChanged( board->util );
} /* boardTurnChanged */

static void
boardGameOver( void* closure )
{
    BoardCtxt* board = (BoardCtxt*)closure;    
    board->scoreBoardInvalid = XP_TRUE; /* not sure if this will do it. */
    board->gameOver = XP_TRUE;
    util_notifyGameOver( board->util );
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
