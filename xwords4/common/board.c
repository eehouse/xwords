/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2002 by Eric House (fixin@peak.org).  All rights reserved.
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

#include "comtypes.h"
#include "board.h"
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

static void drawTimer( BoardCtxt* board );
static void drawScoreBoard( BoardCtxt* board );
static void invalCell( BoardCtxt* board, XP_U16 col, XP_U16 row, 
                       XP_Bool doMirror );
static void
invalCellsUnderRect( BoardCtxt* board, XP_Rect* rect, XP_Bool doMirror );

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
static XP_S16 figureScorePlayerTapped( BoardCtxt* board, XP_U16 x, XP_U16 y );
static XP_Bool cellOccupied( BoardCtxt* board, XP_U16 col, XP_U16 row, 
                             XP_Bool inclPending );
static void makeMiniWindowForTrade( BoardCtxt* board );
static void makeMiniWindowForText( BoardCtxt* board, XP_UCHAR* text, 
                                   MiniWindowType winType );
static void invalTradeWindow( BoardCtxt* board, XP_S16 turn, XP_Bool redraw );
static void invalSelTradeWindow( BoardCtxt* board );
static void setTimerIf( BoardCtxt* board );
static XP_Bool replaceLastTile( BoardCtxt* board );
static XP_Bool setTrayVisState( BoardCtxt* board, XW_TrayVisState newState );
static XP_Bool advanceArrow( BoardCtxt* board );

#ifdef KEY_SUPPORT
static XP_Bool getArrow( BoardCtxt* board, XP_U16* col, XP_U16* row );
static XP_Bool board_moveArrow( BoardCtxt* board, XP_Key cursorKey, 
                                XP_Bool canCycle );

static XP_Bool setArrowVisibleFor( BoardCtxt* board, XP_U16 player, 
                                   XP_Bool visible );
static XP_Bool moveKeyTileToBoard( BoardCtxt* board, XP_Key cursorKey );
#endif

#ifdef KEYBOARD_NAV
static XP_Bool moveScoreCursor( BoardCtxt* board, XP_Key key );
static XP_Bool board_moveCursor( BoardCtxt* board, XP_Key cursorKey );
#endif
#ifdef XWFEATURE_SEARCHLIMIT
static HintAtts figureHintAtts( BoardCtxt* board, XP_U16 col, XP_U16 row );
static void invalCurHintRect( BoardCtxt* board, XP_U16 player,
                              XP_Bool doMirrow );
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

    board = board_make( MPPARM(mpool) model, server, draw, util );

    board->yOffset = (XP_U16)stream_getBits( stream, 2 );
    board->isFlipped = (XP_Bool)stream_getBits( stream, 1 );
    board->gameOver = (XP_Bool)stream_getBits( stream, 1 );
    board->showColors = (XP_Bool)stream_getBits( stream, 1 );
    board->showCellValues = (XP_Bool)stream_getBits( stream, 1 );
#ifdef KEYBOARD_NAV
    board->focussed = (BoardObjectType)stream_getBits( stream, 2 );
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
#ifdef XWFEATURE_SEARCHLIMIT
        if ( stream_getVersion( stream ) >= CUR_STREAM_VERS ) {
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
board_setScoreboardLoc( BoardCtxt* board, XP_U16 scoreLeft, XP_U16 scoreTop,
                        XP_U16 scoreWidth, XP_U16 scoreHeight,
                        XP_Bool divideHorizontally )
{
    board->scoreBdBounds.left = scoreLeft;
    board->scoreBdBounds.top = scoreTop;
    board->scoreBdBounds.width = scoreWidth;
    board->scoreBdBounds.height = scoreHeight;
    board->scoreSplitHor = divideHorizontally;
} /* board_setScoreboardLoc */

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
    XP_Bool changed;
    XP_Bool oldVal = board->disableArrow;

    board->disableArrow = !cp->showBoardArrow;
    changed = oldVal != board->disableArrow;

    if ( changed ) {
        changed = setArrowVisible( board, XP_FALSE );
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

    return changed;
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
invalArrowCell( BoardCtxt* board, XP_Bool doMirror )
{
    BoardArrow* arrow = &board->boardArrow[board->selPlayer];
    invalCell( board, arrow->col, arrow->row, doMirror );
} /* invalArrowCell */

#ifdef KEYBOARD_NAV
static void
invalCursorCell( BoardCtxt* board, XP_Bool doMirror )
{
    BdCursorLoc loc = board->bdCursor[board->selPlayer];
    invalCell( board, loc.col, loc.row, doMirror );
} /* invalCursorCell */
#endif

static void
invalTradeWindow( BoardCtxt* board, XP_S16 turn, XP_Bool redraw )
{
    XP_ASSERT( turn >= 0 );

    if ( board->tradeInProgress[turn] ) {
        MiniWindowStuff* stuff = &board->miniWindowStuff[MINIWINDOW_TRADING];
        invalCellsUnderRect( board, &stuff->rect, XP_FALSE );
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

#ifdef POINTER_SUPPORT
static void
hideMiniWindow( BoardCtxt* board, XP_Bool destroy, MiniWindowType winType )
{
    XP_Bool invalCovers;
    MiniWindowStuff* stuff = &board->miniWindowStuff[winType];

    draw_eraseMiniWindow( board->draw, &stuff->rect,
                          destroy, &stuff->closure, &invalCovers );
    if ( invalCovers ) {
        board_invalRect( board, &stuff->rect );
    }

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
        util_userError( board->util, ERR_NOT_YOUR_TURN );
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
                board->traySelBits[turn] = 0x00;
            }
        } else {
            XP_Bool warn, legal;
            WordNotifierInfo info;
            XWStreamCtxt* stream = 
                mem_stream_make( MPPARM(board->mpool) 
                                 util_getVTManager(board->util), NULL,
                                 CHANNEL_NONE, (MemStreamCloseCallback)NULL );

            XP_UCHAR* str = util_getUserString(board->util, STR_COMMIT_CONFIRM);

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
board_selectPlayer( BoardCtxt* board, XP_U16 newPlayer )
{
    if ( !board->gameOver && server_getCurrentTurn(board->server) < 0 ) {
        /* game not started yet; do nothing */
    } else if ( board->selPlayer == newPlayer ) {
        checkRevealTray( board );
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
            invalCurHintRect( board, oldPlayer, XP_FALSE );
        }
        if ( board->hasHintRect[newPlayer] ) {
            invalCurHintRect( board, newPlayer, XP_FALSE );
        }
#endif

        invalArrowCell( board, XP_FALSE );
        board->selPlayer = (XP_U8)newPlayer;
        invalArrowCell( board, XP_FALSE );

        board_invalTrayTiles( board, ALLTILES );
        board->dividerInvalid = XP_TRUE;

        setTrayVisState( board, TRAY_REVERSED );
    }
    board->scoreBoardInvalid = XP_TRUE;	/* if only one player, number of
                                           tiles remaining may have changed*/
} /* board_selectPlayer */

void
board_hiliteCellAt( BoardCtxt* board, XP_U16 col, XP_U16 row )
{
    XP_Rect cellRect;

    if ( getCellRect( board, col, row, &cellRect ) ) {
        draw_invertCell( board->draw, &cellRect );
        invalCell( board, col, row, XP_FALSE );
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
        rect->top = XP_MAX( board->boardBounds.top + 1, y - rect->height );
    }
} /* positionBonusRect */

static void
timerFiredForPen( BoardCtxt* board ) 
{
    XP_UCHAR* text = (XP_UCHAR*)NULL;
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
            XP_UCHAR* format;
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
        util_setTimer( board->util, TIMER_TIMERTICK ); 
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

void
board_timerFired( BoardCtxt* board, XWTimerReason why )
{
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

void
board_invalAll( BoardCtxt* board )
{
    XP_U16 lastRow = model_numRows( board->model );
    while ( lastRow-- ) {
        board->redrawFlags[lastRow] = ~0;
    }
    board->needsDrawing = XP_TRUE;

    board_invalTrayTiles( board, ALLTILES );
    board->dividerInvalid = XP_TRUE;
    board->tradingMiniWindowInvalid = XP_TRUE;
    board->scoreBoardInvalid = XP_TRUE;
} /* board_invalAll */

/* 
 * invalidate all cells that contain a tile.  Return TRUE if any invalidation
 * actually happens.
 */
static XP_Bool
invalCellsWithTiles( BoardCtxt* board, XP_Bool doMirror )
{
    ModelCtxt* model = board->model;
    XP_S16 turn = board->selPlayer;
    XP_Bool includePending = board->trayVisState == TRAY_REVEALED;
    XP_S16 col, row;

    /* for each col,row pair, invalidate it if it holds a tile that's visible
       AND if the rect itself is visible */
    for ( row = model_numRows( model )-1; row >= 0; --row ) {
        for ( col = model_numCols( model )-1; col >= 0; --col ) {
            Tile tile;
            XP_Bool ignore;
            if ( model_getTile( model, col, row, includePending,
                                turn, &tile, &ignore, &ignore, &ignore ) ) {

                if ( board->boardObscuresTray ) {
                    XP_Rect cellR;
                    if ( !getCellRect( board, col, row, &cellR ) 
                         || (board->trayVisState != TRAY_HIDDEN &&
                             rectContainsRect( &board->trayBounds, &cellR )) ) {
                        continue;
                    }
                }
                invalCell( board, col, row, doMirror );
            }
        }
    }
    return board->needsDrawing;
} /* invalCellsWithTiles */

static void
checkScrollCell( void* p_board, XP_U16 turn, XP_U16 col, XP_U16 row )
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
            board_setYOffset( board, oldOffset, XP_TRUE );
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
    ModelCtxt* model = board->model;
    XP_U16 i;
    XP_U16 lastCol, lastRow;
    BlankQueue invalBlanks;
    XP_U16 nInvalBlanks = 0;

    lastCol = model_numCols(model) - 1;
    lastRow = model_numRows(model) - 1;

    for ( i = 0; i < bqp->nBlanks; ++i ) {
        XP_U16 col = bqp->col[i];
        XP_U16 row = bqp->row[i];

        if ( INVAL_BIT_SET( board, col, row )
             || (col > 0 && INVAL_BIT_SET( board, col-1, row ))
             || (col < lastCol && INVAL_BIT_SET( board, col+1, row ))
             || (row > 0 && INVAL_BIT_SET( board, col, row-1 ))
             || (row < lastRow && INVAL_BIT_SET( board, col, row+1 )) ) {

            invalCell( board, col, row, XP_FALSE );

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

        invalSelTradeWindow( board );
        dist = (board->yOffset - board->prevYScrollOffset)
            * board->boardVScale;

        scrolled = draw_vertScrollBoard( board->draw, &scrollR, dist );

        if ( scrolled ) {
            /* inval the rows that have been scrolled into view.  I'm cheating
               making the client figure the inval rect, but Palm's the only
               client now and it does it so well.... */
            invalCellsUnderRect( board, &scrollR, XP_FALSE );
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
    drawScoreBoard( board );

    if ( board->needsDrawing 
         && draw_boardBegin( board->draw, &board->boardBounds, 
                             board->focussed == OBJ_BOARD ) ) {

        XP_Bool allDrawn = XP_TRUE;
        XP_S16 lastCol, lastRow, i;
        ModelCtxt* model = board->model;
        BlankQueue bq;


        scrollIfCan( board );	/* this must happen before we count blanks
                                   since it invalidates squares */

        model_listPlacedBlanks( model, board->selPlayer, 
                                board->trayVisState == TRAY_REVEALED, &bq );
        invalBlanksWithNeighbors( board, &bq );

        lastRow = model_numRows( model );
        while ( lastRow-- ) {
            XP_U16 rowFlags = board->redrawFlags[lastRow];
            if ( rowFlags != 0 ) {
                XP_U16 colMask;
                XP_U16 failedBits = 0;
                lastCol = model_numCols( model );
                for ( colMask = 1<<(lastCol-1); lastCol--; colMask >>= 1 ) {
                    if ( (rowFlags & colMask) != 0 ) {
                        if ( !drawCell( board, lastCol, lastRow, XP_TRUE ) ) {
                            failedBits |= colMask;
                            allDrawn = XP_FALSE;
                        }
                    }
                }
                board->redrawFlags[lastRow] = failedBits;
            }
        }

        /* draw the blanks we skipped before */
        for ( i = 0; i < bq.nBlanks; ++i ) {
            if ( !drawCell( board, bq.col[i], bq.row[i], XP_FALSE ) ) {
                allDrawn = XP_FALSE;
            }
        }

        if ( board->trayVisState == TRAY_REVEALED ) {
            XP_Rect cursorRect;
            BoardArrow* arrow = &board->boardArrow[board->selPlayer];

            if ( arrow->visible ) {
                XP_U16 col = arrow->col;
                XP_U16 row = arrow->row;
                XP_Bool drawVertical = 
                    (arrow->vert == XP_CURSOR_KEY_DOWN) ^ board->isFlipped;
                if ( getCellRect( board, col, row, &cursorRect ) ) {
                    XWBonusType bonus;
                    HintAtts hintAtts;
                    bonus = util_getSquareBonus( board->util, model, col, row );
                    hintAtts = figureHintAtts( board, col, row );
                    draw_drawBoardArrow( board->draw, &cursorRect, 
                                         bonus, drawVertical, hintAtts );
                }
            }
#ifdef KEYBOARD_NAV
            {
                BdCursorLoc loc = board->bdCursor[board->selPlayer];
                if ( getCellRect( board, loc.col, loc.row, &cursorRect ) ) {
                    draw_drawBoardCursor( board->draw, &cursorRect );
                }
            }
#endif
            
        }

        drawTradeWindowIf( board );

        draw_boardFinished( board->draw );

        board->needsDrawing = !allDrawn;
    }

    drawTray( board, board->focussed==OBJ_TRAY );

    return !board->needsDrawing;
} /* board_draw */

static XP_S16
figureSecondsLeft( BoardCtxt* board )
{
    CurGameInfo* gi = board->gi;
    XP_U16 secondsUsed = gi->players[board->selPlayer].secondsUsed;
    XP_U16 secondsAvailable = gi->gameSeconds / gi->nPlayers;
    XP_ASSERT( gi->timerEnabled );
    return secondsAvailable - secondsUsed;
} /* figureSecondsLeft */

static void
drawTimer( BoardCtxt* board )
{
    if ( board->gi->timerEnabled ) {
        XP_S16 secondsLeft = figureSecondsLeft( board );

        draw_drawTimer( board->draw, &board->timerBounds, &board->timerBounds, 
                        board->selPlayer, secondsLeft );
    }
} /* drawTimer */

static XP_Bool
board_ScoreCallback( void* closure, XP_S16 player, XP_UCHAR* expl, 
                     XP_U16* explLen)
{
    ModelCtxt* model = (ModelCtxt*)closure;
    return model_getPlayersLastScore( model, player,
                                      expl, explLen );
} /* board_ScoreCallback */

typedef struct DrawScoreData {
    DrawScoreInfo dsi;
    XP_U16 height;
    XP_U16 width;
} DrawScoreData;

static void
drawScoreBoard( BoardCtxt* board )
{
    if ( board->scoreBoardInvalid ) {
        short i;

        XP_U16 nPlayers = board->gi->nPlayers;

        if ( nPlayers > 0 ) {
            ModelCtxt* model = board->model;
            XP_S16 curTurn = server_getCurrentTurn( board->server );
            XP_U16 selPlayer = board->selPlayer;
            XP_S16 nTilesInPool = server_countTilesInPool( board->server );
            XP_Rect scoreRect = board->scoreBdBounds;
            XP_S16* adjustDim;
            XP_S16* adjustPt;
            XP_U16 totalDim, extra, nShares, remWidth, remHeight, remDim;
            DrawScoreData* dp;
            DrawScoreData datum[MAX_NUM_PLAYERS];
            XP_S16 scores[MAX_NUM_PLAYERS];
            XP_Bool isVertical = !board->scoreSplitHor;

            draw_scoreBegin( board->draw, &board->scoreBdBounds, nPlayers,
                             board->focussed==OBJ_SCORE );

            /* Let platform decide whether the rem: string should be given any
               space once there are no tiles left.  On Palm that space is
               clickable to drop a menu, so will probably leave it.  */
            draw_measureRemText( board->draw, &board->scoreBdBounds, 
                                 nTilesInPool, &remWidth, &remHeight );
            remDim = isVertical? remHeight : remWidth;

            if ( isVertical ) {
                adjustPt = &scoreRect.top;
                adjustDim = &scoreRect.height;
            } else {
                adjustPt = &scoreRect.left;
                adjustDim = &scoreRect.width;
            }

            /* Get the scores from the model or by calculating them based on
               the end-of-game state. */
            if ( board->gameOver ) {
                model_figureFinalScores( model, scores, (XP_S16*)NULL );
            } else {
                for ( i = 0; i < nPlayers; ++i ) {
                    scores[i] = model_getPlayerScore( model, i );
                }
            }

            totalDim = remDim;

            /* figure spacing for each scoreboard entry */
            for ( dp = datum, i = 0; i < nPlayers; ++i, ++dp ) {
                LocalPlayer* lp = &board->gi->players[i];

                /* This is a hack! */
                dp->dsi.lsc = board_ScoreCallback;
                dp->dsi.lscClosure = model;

                dp->dsi.score = scores[i];
                dp->dsi.isTurn = (i == curTurn);
                dp->dsi.name = emptyStringIfNull(lp->name);
                dp->dsi.selected = board->trayVisState != TRAY_HIDDEN
                    && i==selPlayer;
                dp->dsi.isRobot = lp->isRobot;
                dp->dsi.isRemote = !lp->isLocal;
                dp->dsi.nTilesLeft = (nTilesInPool > 0)? -1:
                    model_getNumTilesTotal( model, i );
                draw_measureScoreText( board->draw, &scoreRect,
                                       &dp->dsi, &dp->width, &dp->height );
                totalDim += isVertical ? dp->height : dp->width;
            }

            /* break extra space into chunks, one to follow REM and another to
               preceed the timer, and then one for each player.  Generally the
               player's score will be centered in the rect it's given, so in
               effect we're putting half the chunk on either side.  The goal
               here is for the scores to be closer to each other than they are
               to the rem: string and timer on the ends. */
            nShares = nPlayers;
            if ( remDim > 0 ) {
                ++nShares;
            }
            XP_ASSERT( *adjustDim >= totalDim );
/*             XP_LOGF( "*adjustDim=%d, totalDim=%d", *adjustDim, totalDim ); */
            extra = (*adjustDim - totalDim) / nShares;

            /* at this point, the scoreRect should be anchored at the
               scoreboard rect's upper left.  */

            if ( remDim > 0 ) {
                *adjustDim = remDim;

                draw_drawRemText( board->draw, &scoreRect, &scoreRect, 
                                  nTilesInPool );

                remDim += extra;
                *adjustPt += remDim;
            }

            XP_ASSERT( remDim <= 0x00FF );
            board->remDim = (XP_U8)remDim;	/* save now so register can be
                                               reused */
            for ( dp = datum, i = 0; i < nPlayers; ++dp, ++i ) {
                XP_Rect innerRect;
                XP_U16 dim = isVertical? dp->height:dp->width;
                *adjustDim = board->scoreDims[i] = dim + extra;

                innerRect.width = dp->width;
                innerRect.height = dp->height;
                innerRect.left = scoreRect.left + 
                    ((scoreRect.width - innerRect.width) / 2);
                innerRect.top = scoreRect.top + 
                    ((scoreRect.height - innerRect.height) / 2);

                draw_score_drawPlayer( board->draw, i, &innerRect, &scoreRect,
                                       &dp->dsi );
                *adjustPt += *adjustDim;
            }

            draw_scoreFinished( board->draw );
        }

        board->scoreBoardInvalid = XP_FALSE;
    }
	    
    drawTimer( board );
} /* drawScoreBoard */

void
board_setTrayLoc( BoardCtxt* board, XP_U16 left, XP_U16 top, 
                  XP_U8 trayScaleH, XP_U8 trayScaleV, 
                  XP_U8 dividerWidth )
{
    board->trayBounds.left = left;
    board->trayBounds.top = top;
    /* what's this +1 for? */
    board->trayBounds.width = (trayScaleH * MAX_TRAY_TILES) + 1;
    board->trayBounds.height = trayScaleV;

    board->trayScaleH = trayScaleH;
    board->trayScaleV = trayScaleV;

    board->dividerWidth = dividerWidth;
    board->trayBounds.width += (XP_U8)dividerWidth;

    /* boardObscuresTray is about whether they *can* overlap, not just about
     * they do given the current scroll position of the board. Remember
     * (e.g. for curses version) that vertical intersection isn't enough.*/
    board->boardObscuresTray = 
        (top < (board->boardBounds.top + 
                board->boardVScale * model_numRows( board->model )))
        && (left < (board->boardBounds.left + 
                    board->boardHScale * model_numCols( board->model )));
    if ( !board->boardObscuresTray && board->trayVisState == TRAY_HIDDEN ) {
        XW_TrayVisState state = TRAY_REVERSED;
        setTrayVisState( board, state );
    }
    figureBoardRect( board );
} /* board_setTrayLoc */

static void
invalCellsUnderRect( BoardCtxt* board, XP_Rect* rect, XP_Bool doMirror )
{
    XP_U16 lastCol, lastRow;

    lastRow = model_numRows( board->model );
    while ( lastRow-- ) {
        lastCol = model_numCols( board->model );
        while ( lastCol-- ) {
            XP_Rect cell;

            if ( getCellRect( board, lastCol, lastRow, &cell ) &&
                 rectsIntersect( rect, &cell ) ) {
                invalCell( board, lastCol, lastRow, doMirror );
            }
        }
    }
} /* invalCellsUnderRect */

void
board_invalRect( BoardCtxt* board, XP_U16 left, XP_U16 top,
                 XP_U16 right, XP_U16 bottom )
{
    XP_Rect rect;
    rect.left = left;
    rect.top = top;
    rect.width = right - left;
    rect.height = bottom - top;

    if ( rectsIntersect( &rect, &board->boardBounds ) ) {
        invalCellsUnderRect( board, &rect, XP_FALSE );
    }
    
    if ( rectsIntersect( &rect, &board->trayBounds ) ) {
        invalTilesUnderRect( board, &rect );
    }

    if ( rectsIntersect( &rect, &board->scoreBdBounds ) ) {
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
                board_setYOffset( board, 0, XP_FALSE );
            } else {
                board_setYOffset( board, board->preHideYOffset, XP_FALSE );
            }
        }

        if ( nowHidden ) {
            /* Can't always set this to TRUE since in obscuring case tray will
             * get erased after the cells that are supposed to be revealed
             * get drawn. */
            board->eraseTray = !board->boardObscuresTray;
            invalCellsUnderRect( board, &board->trayBounds, XP_FALSE );
        }
        invalArrowCell( board, XP_FALSE );
        board->scoreBoardInvalid = XP_TRUE; /* b/c what's bold may change */
#ifdef XWFEATURE_SEARCHLIMIT
        invalCurHintRect( board, selPlayer, XP_FALSE );
#endif

        util_trayHiddenChange( board->util, board->trayVisState );
    }
    return changed;
} /* setTrayVisState */

XP_Bool
board_flip( BoardCtxt* board )
{
    invalArrowCell( board, XP_TRUE );
#ifdef KEYBOARD_NAV
    invalCursorCell( board, XP_TRUE );
#endif

    if ( board->boardObscuresTray ) {
        invalCellsUnderRect( board, &board->trayBounds, XP_TRUE );
    }
    invalCellsWithTiles( board, XP_TRUE );

#ifdef XWFEATURE_SEARCHLIMIT
    invalCurHintRect( board, board->selPlayer, XP_TRUE );
#endif

    board->isFlipped = !board->isFlipped;

    return board->needsDrawing;
    /* invalCellsWithTiles won't work here because we need to invalidate
     * cells that are empty, or that will be empty after the flip. */
    /*     board_invalAll( board ); */
    /*     return XP_TRUE; */
} /* board_flip */

XP_Bool
board_toggle_showValues( BoardCtxt* board )
{
    board->showCellValues = !board->showCellValues;
    return invalCellsWithTiles( board, XP_FALSE );
} /* board_toggle_showValues */

XP_Bool
board_setShowColors( BoardCtxt* board, XP_Bool showColors )
{
    board->showColors = showColors;
    board->scoreBoardInvalid = XP_TRUE;
    return invalCellsWithTiles( board, XP_FALSE );
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
            XP_Bool wasVisible;
            XP_Bool canMove;

            wasVisible = setArrowVisible( board, XP_FALSE );

            (void)board_replaceTiles( board );

            tiles = tileSet->tiles + board->dividerLoc[selPlayer];

            board_pushTimerSave( board );

#ifdef XWFEATURE_SEARCHLIMIT
            XP_ASSERT( board->gi->allowHintRect || !board->hasHintRect[selPlayer] );
#endif
            searchComplete = engine_findMove(engine, model, 
                                             model_getDictionary(model),
                                             tiles, nTiles,
#ifdef XWFEATURE_SEARCHLIMIT
                                             (board->gi->allowHintRect &&
                                              board->hasHintRect[selPlayer])?
                                             &board->limits[selPlayer] : NULL,
                                             useTileLimits,
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
    XP_Rect cellRect;
    Tile tile;
    XP_UCHAR ch[4];
    XP_Bool isBlank, isEmpty, recent, pending = XP_FALSE;
    XWBonusType bonus;
    ModelCtxt* model = board->model;
    DictionaryCtxt* dict = model_getDictionary( model );
    XP_U16 selPlayer = board->selPlayer;
    /* We want to invert EITHER the current pending tiles OR the most recent
     * move.  So if the tray is visible AND there are tiles missing from it,
     * show them.  Otherwise show the most recent move.
     */
    XP_U16 curCount = model_getCurrentMoveCount( model, selPlayer );
    XP_Bool showPending = board->trayVisState == TRAY_REVEALED
        && curCount > 0;


    if ( dict != NULL && getCellRect( board, col, row, &cellRect ) ) {

        /* This 'while' is only here so I can 'break' below */
        while ( board->trayVisState == TRAY_HIDDEN ||
                !rectContainsRect( &board->trayBounds, &cellRect ) ) {
            XP_S16 owner = -1;
            XP_Bool invert = XP_FALSE;
            XP_Bitmap bitmap = NULL;
            XP_UCHAR* textP = (XP_UCHAR*)ch;
            HintAtts hintAtts;

            isEmpty = !model_getTile( model, col, row, showPending,
                                      selPlayer, &tile, &isBlank,
                                      &pending, &recent );

            *(unsigned long*)&ch[0] = 0L;
            if ( isEmpty ) {
                isBlank = XP_FALSE;
            } else if ( isBlank && skipBlanks ) {
                break;
            } else {
                if ( board->showColors ) {
                    owner = (XP_S16)model_getCellOwner( model, col, row );
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
                    dict_tilesToString( dict, &tile, 1, ch );
                }
            }
            bonus = util_getSquareBonus( board->util, model, col, row );
            hintAtts = figureHintAtts( board, col, row );
            return draw_drawCell( board->draw, &cellRect, textP, bitmap, 
                                  owner, bonus, hintAtts, isBlank, invert,
                                  (isEmpty && (col==board->star_row)
                                   && (row==board->star_row)));
        }
    }

    return XP_TRUE;
} /* drawCell */

static void
figureBoardRect( BoardCtxt* board )
{
    XP_Rect boardBounds = board->boardBounds;

    boardBounds.width = model_numCols( board->model ) * board->boardHScale;
    boardBounds.height = (model_numRows( board->model ) - board->yOffset)
        * board->boardVScale;

    if ( board->trayVisState != TRAY_HIDDEN && board->boardObscuresTray ) {
        boardBounds.height = board->trayBounds.top - boardBounds.top - 1;
    }

    board->boardBounds = boardBounds;
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

    if ( board->isFlipped ) {
        XP_U16 tmp = col;
        col = row;
        row = tmp;
    }

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

    if ( board->isFlipped ) {
        XP_U16 tmp = col;
        col = row;
        row = tmp;
    }

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
invalCell( BoardCtxt* board, XP_U16 col, XP_U16 row, XP_Bool doMirror )
{
    board->redrawFlags[row] |= 1 << col;

    if ( doMirror ) {
        board->redrawFlags[col] |= 1 << row;
    }

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

#ifdef KEYBOARD_NAV
static XP_Bool
focusNext( BoardCtxt* board )
{
    short numHolders = 3;	/* board */
    short tmp = (short)board->focussed; /* avoid franklin casting crap */

    tmp %= numHolders;
    board->focussed = (BoardObjectType)(tmp + 1); /* skip OBJ_NONE */

    return numHolders > 1;
} /* focusNext */
#endif

#ifdef POINTER_SUPPORT
static XP_Bool
pointOnSomething( BoardCtxt* board, XP_U16 x, XP_U16 y, BoardObjectType* wp )
{
    if ( board->trayVisState != TRAY_HIDDEN
         && rectContainsPt( &board->trayBounds, x, y ) ) {
        *wp = OBJ_TRAY;
    } else if ( rectContainsPt( &board->boardBounds, x, y ) ) {
        *wp = OBJ_BOARD;
    } else if ( rectContainsPt( &board->scoreBdBounds, x, y ) ) {
        *wp = OBJ_SCORE;
    } else {
        return XP_FALSE;
    }

    return XP_TRUE;
} /* pointOnSomething */
#endif

#if defined POINTER_SUPPORT || defined KEYBOARD_NAV
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
makeMiniWindowForText( BoardCtxt* board, XP_UCHAR* text, 
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
    XP_UCHAR* text;

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

#ifdef POINTER_SUPPORT
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
        BdHintLimits limits;
        XP_Bool isFlipped = board->isFlipped;
        
        limits = board->limits[board->selPlayer];

        /* while lets us break to exit... */
        while ( board->hasHintRect[board->selPlayer]
                || board->hintDragInProgress ) {
            if ( col < limits.left ) break;
            if ( row < limits.top ) break;
            if ( col > limits.right ) break;
            if ( row > limits.bottom ) break;

            if ( col == limits.left ) {
                result |= isFlipped? HINT_BORDER_TOP : HINT_BORDER_LEFT;
            }
            if ( col == limits.right ) {
                result |= isFlipped? HINT_BORDER_BOTTOM:HINT_BORDER_RIGHT;
            }
            if ( row == limits.top) {
                result |= isFlipped?HINT_BORDER_LEFT:HINT_BORDER_TOP;
            }
            if ( row == limits.bottom ) {
                result |= isFlipped? HINT_BORDER_RIGHT:HINT_BORDER_BOTTOM;
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
                 XP_U16 rowB, XP_Bool doMirror )
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
                invalCell( board, col, row, doMirror );
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
invalCurHintRect( BoardCtxt* board, XP_U16 player, XP_Bool doMirror )
{
    BdHintLimits* limits = &board->limits[player];    
    invalCellRegion( board, limits->left, limits->top, 
                     limits->right, limits->bottom, doMirror );
} /* invalCurHintRect */

static void
clearCurHintRect( BoardCtxt* board )
{
    invalCurHintRect( board, board->selPlayer, XP_FALSE );
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
                     newLim->right, newLim->bottom, XP_FALSE );
    invalCellRegion( board, oldLim->left, oldLim->top, 
                     oldLim->right, oldLim->bottom, XP_FALSE );

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

        checkScrollCell( board, selPlayer, col, row );

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
    if ( board->isFlipped ) {
        makeActive = board->hintDragStartCol <= board->hintDragCurCol;
    } else {
        makeActive = board->hintDragStartRow <= board->hintDragCurRow;
    }

    board->hasHintRect[board->selPlayer] = makeActive;
    if ( !makeActive ) {
        invalCurHintRect( board, board->selPlayer, XP_FALSE );
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
    util_setTimer( board->util, TIMER_PENDOWN );
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
			XP_UCHAR buf[16];
			XP_U16 buflen = sizeof(buf);
			XP_UCHAR* name = emptyStringIfNull(lp->name);

			/* repeat until player gets passwd right or hits cancel */
            while ( util_askPassword( board->util, name, buf, &buflen ) ) {
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

#ifdef POINTER_SUPPORT
XP_Bool
board_handlePenDown( BoardCtxt* board, XP_U16 x, XP_U16 y, XP_Time when,
                     XP_Bool* handled )
{
    XP_Bool result = XP_FALSE;
    XP_Bool penDidSomething;
    BoardObjectType onWhich;

    penDidSomething = pointOnSomething( board, x, y, &onWhich );

    if ( !penDidSomething ) {
        board->penDownObject = OBJ_NONE;
    } else {

        switch ( onWhich ) {

        case OBJ_BOARD:
            result = handlePenDownOnBoard( board, x, y );
            break;

        case OBJ_TRAY:
            /* 	XP_ASSERT( board->trayIsVisible ); */
            XP_ASSERT( board->trayVisState != TRAY_HIDDEN );

            if ( board->trayVisState != TRAY_REVERSED ) {
                result = handlePenDownInTray( board, x, y );
            }
            break;

        case OBJ_SCORE:
            if ( figureScorePlayerTapped( board, x, y ) >= 0 ) {
                util_setTimer( board->util, TIMER_PENDOWN );
            }
            break;
        default:
            break;
        }

        board->penDownX = x;
        board->penDownY = y;
        board->penDownTime = when;
        board->penDownObject = onWhich;
        /*     board->inDrag = XP_TRUE; */
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

static XP_S16
figureScorePlayerTapped( BoardCtxt* board, XP_U16 x, XP_U16 y )
{
    XP_S16 result = -1;
    XP_S16 left;
    XP_U16 nPlayers = board->gi->nPlayers;

    if ( board->scoreSplitHor ) {
        left = x - board->scoreBdBounds.left;
    } else {
        left = y - board->scoreBdBounds.top;
    }

    left -= board->remDim;
    if ( left >= 0 ) {
        for ( result = 0; result < nPlayers; ++result ) {
            if ( left < board->scoreDims[result] ) {
                break;
            }
            left -= board->scoreDims[result];
        }
    }
    if ( result >= nPlayers ) {
        result = -1;
    }
    return result;
} /* figureScorePlayerTapped */

/* If the pen also went down on the scoreboard, make the selected player the
 * one closest to the mouse up loc.
 */
#ifdef POINTER_SUPPORT
static XP_Bool
handlePenUpScore( BoardCtxt* board, XP_U16 x, XP_U16 y )
{
    XP_Bool result = XP_FALSE;

    XP_S16 playerNum = figureScorePlayerTapped( board, x, y );

    if ( playerNum >= 0 ) {
        board_selectPlayer( board, playerNum );

        result = XP_TRUE;
    }
    return result;
} /* handlePenUpScore */
#endif

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

    XP_Bool result = model_getTile( board->model, col, row, inclPending,
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
                invalArrowCell( board, XP_FALSE );
            }
            arrow->col = (XP_U8)col;
            arrow->row = (XP_U8)row;
        }
        invalCell( board, col, row, XP_FALSE );
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

    if ( model_getTile( board->model, pencol, penrow, XP_TRUE,
                        board->selPlayer, &tile, &ignore, &isPending, 
                        (XP_Bool*)NULL )
         && isPending ) {

        XP_S16 count = model_getCurrentMoveCount( board->model, 
                                                  board->selPlayer );
        while ( count-- ) {
            index = count;
            model_getCurrentMoveTile( board->model, board->selPlayer, 
                                      &index, &tile, &col, &row, &ignore );
            if ( col == pencol && row == penrow ) {
                model_moveBoardToTray( board->model, board->selPlayer, 
                                       index );
                /* the cursor should show up where the tile used to be so it's
                   easy to replace it. */
                setArrow( board, col, row );
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

#ifdef POINTER_SUPPORT
XP_Bool
board_handlePenUp( BoardCtxt* board, XP_U16 x, XP_U16 y, XP_Time when )
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
                            XP_U16 selPlayer = board->selPlayer;
                            invalSelTradeWindow( board );
                            board->tradeInProgress[selPlayer] = XP_FALSE;
                            board_invalTrayTiles( 
                                board, board->traySelBits[selPlayer] );
                            board->traySelBits[selPlayer] = 0x00;
                            result = XP_TRUE;
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
static XP_Key
flipKey( BoardCtxt* board, XP_Key key ) {
    if ( board->isFlipped ) {
        switch( key ) {
        case XP_CURSOR_KEY_DOWN:
            return XP_CURSOR_KEY_RIGHT;
        case XP_CURSOR_KEY_UP:
            return XP_CURSOR_KEY_LEFT;
        case XP_CURSOR_KEY_LEFT:
            return XP_CURSOR_KEY_UP;
        case XP_CURSOR_KEY_RIGHT:
            return XP_CURSOR_KEY_DOWN;
        default:
            XP_ASSERT(0);
        }
    }
    return key;
} /* flipKey */
#endif

XP_Bool
board_handleKey( BoardCtxt* board, XP_Key key )
{
    XP_Bool result = XP_FALSE;
    XP_Bool trayVisible = board->trayVisState == TRAY_REVEALED;

    switch( key ) {
#ifdef KEYBOARD_NAV
    case XP_CURSOR_KEY_DOWN:
    case XP_CURSOR_KEY_UP:
    case XP_CURSOR_KEY_LEFT:
    case XP_CURSOR_KEY_RIGHT:
        if ( board->focussed == OBJ_BOARD ) {
            result = trayVisible && board_moveCursor( board, 
                                                      flipKey(board,key) );
        } else if ( board->focussed == OBJ_SCORE ) {
            result = moveScoreCursor( board, key );
        } else if ( trayVisible && board->focussed == OBJ_TRAY ) {
            result = tray_moveCursor( board, key );
        }
        break;
#endif

    case XP_CURSOR_KEY_DEL:
        if ( trayVisible ) {
            replaceLastTile( board );
            result = XP_TRUE;
        }
        break;

#ifdef KEYBOARD_NAV
    case XP_FOCUSCHANGE_KEY:
        if ( focusNext( board ) ) {
            board_invalAll( board ); /* really just want to inval borders! */
            result = XP_TRUE;
        }
        break;

    case XP_RETURN_KEY:
        if ( board->focussed == OBJ_TRAY ) {
            if ( trayVisible ) {
                result = tray_keyAction( board );
            } else {
                result = askRevealTray( board );
            }
        } else if ( board->focussed == OBJ_BOARD ) {
            /* mimic pen-down/pen-up on cursor */
            BdCursorLoc loc = board->bdCursor[board->selPlayer];
            result = handleActionInCell( board, loc.col, loc.row );
        } else if ( board->focussed == OBJ_SCORE ) {
	    /* tap on what's already selected: reveal tray, etc. */
            board_selectPlayer( board, board->selPlayer );
	    result = XP_TRUE;
        }
        break;
#endif

    default:
        XP_ASSERT( key >= XP_KEY_LAST );

        result = trayVisible && moveKeyTileToBoard( board, key );

        if ( result && !advanceArrow( board ) ) {
            setArrowVisible( board, XP_FALSE );
        }
    } /* switch */

    return result;
} /* board_handleKey */

/* used by curses version only! */
#ifdef KEYBOARD_NAV
XP_Bool
board_toggle_arrowDir( BoardCtxt* board )
{
    BoardArrow* arrow = &board->boardArrow[board->selPlayer];
    if ( arrow->visible ) {
        arrow->vert = !arrow->vert;
        invalArrowCell( board, XP_FALSE );
        return XP_TRUE;
    } else {
        return XP_FALSE;
    }
} /* board_toggle_cursorDir */

static XP_Bool
moveScoreCursor( BoardCtxt* board, XP_Key key )
{
    XP_Bool result = XP_TRUE;
    XP_U16 nPlayers = board->gi->nPlayers;
    XP_U16 selPlayer = board->selPlayer + nPlayers;

    switch ( key ) {
    case XP_CURSOR_KEY_DOWN:
    case XP_CURSOR_KEY_RIGHT:
        ++selPlayer;
        break;
    case XP_CURSOR_KEY_UP:
    case XP_CURSOR_KEY_LEFT:
        --selPlayer;
        break;
    default:
        result = XP_FALSE;
    }
    board_selectPlayer( board, selPlayer % nPlayers );
    return result;
} /* moveScoreCursor */
#endif

static XP_Bool
advanceArrow( BoardCtxt* board )
{
    XP_Key key = board->boardArrow[board->selPlayer].vert ?
        XP_CURSOR_KEY_DOWN :  XP_CURSOR_KEY_RIGHT;
	    
    XP_ASSERT( board->trayVisState == TRAY_REVEALED );

    return board_moveArrow( board, key, XP_FALSE );
} /* advanceArrow */

static XP_Bool
figureNextLoc( BoardCtxt* board, XP_Key cursorKey, XP_Bool canCycle, 
               XP_Bool avoidOccupied, XP_U16* colP, XP_U16* rowP )
{
    XP_S16 max;
    XP_S16* useWhat;
    XP_S16 end = 0;
    XP_U16 counter = 0;
    XP_S16 incr = 0;
    XP_U16 numCols, numRows;
    XP_Bool result = XP_FALSE;

    /*     XP_ASSERT( board->focussed == OBJ_BOARD ); */
    /* don't allow cursor's jumps to reveal hidden tiles */
    if ( board->trayVisState != TRAY_REVEALED || cursorKey == XP_KEY_NONE ) {
        return XP_FALSE;
    }

    numRows = model_numRows( board->model );
    numCols = model_numCols( board->model );

    switch ( cursorKey ) {
	     
    case XP_CURSOR_KEY_DOWN:
        incr = 1;
        useWhat = (XP_S16*)rowP;
        max = numRows;
        end = max;
        break;
    case XP_CURSOR_KEY_UP:
        incr = -1;
        useWhat = (XP_S16*)rowP;
        max = numRows;
        end = -1;
        break;
    case XP_CURSOR_KEY_LEFT:
        incr = -1;
        useWhat = (XP_S16*)colP;
        max = numCols;
        end = -1;
        break;
    case XP_CURSOR_KEY_RIGHT:
        incr = 1;
        useWhat = (XP_S16*)colP;
        max = numCols;
        end = max;
        break;
    default:
        XP_ASSERT( XP_FALSE );
        return XP_FALSE;
    }

    XP_ASSERT( incr != 0 );

    for ( counter = max; ; --counter ) {

        *useWhat += incr;

        if ( (counter == 0) || (!canCycle && (*useWhat == end)) ) {
            result = XP_FALSE;
            break;
        }

        *useWhat = (*useWhat + max) % max;

        if ( !avoidOccupied 
             || !cellOccupied( board, *colP, *rowP, XP_TRUE ) ) {
            result = XP_TRUE;
            break;
        }
    }

    return result;
} /* figureNextLoc */

static XP_Bool
board_moveArrow( BoardCtxt* board, XP_Key cursorKey, XP_Bool canCycle )
{
    XP_U16 col, row;
    XP_Bool changed;

    setArrowVisible( board, XP_TRUE );
    (void)getArrow( board, &col, &row );
    changed = figureNextLoc( board, cursorKey, canCycle, XP_TRUE, &col, &row );
    if ( changed ) {
        (void)setArrow( board, col, row );
    }
    return changed;
} /* board_moveArrow */

#ifdef KEYBOARD_NAV
static XP_Bool
board_moveCursor( BoardCtxt* board, XP_Key cursorKey )
{
    BdCursorLoc loc = board->bdCursor[board->selPlayer];
    XP_U16 col = loc.col;
    XP_U16 row = loc.row;
    XP_Bool changed;

    changed = figureNextLoc( board, cursorKey, XP_TRUE, XP_FALSE, 
                             &col, &row );
    if ( changed ) {
        invalCell( board, loc.col, loc.row, XP_FALSE );
        invalCell( board, col, row, XP_FALSE );
        loc.col = col;
        loc.row = row;
        board->bdCursor[board->selPlayer] = loc;
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
rectsIntersect(XP_Rect* rectP1, XP_Rect* rectP2)
{
    XP_Rect rect1 = *rectP1;
    XP_Rect rect2 = *rectP2;

    if ( rect1.top >= rect2.top + rect2.height ) {
        return XP_FALSE;
    } else if ( rect1.left >= rect2.left + rect2.width ) {
        return XP_FALSE;
    } else if ( rect2.top >= rect1.top + rect1.height ) {
        return XP_FALSE;
    } else if ( rect2.left >= rect1.left + rect1.width ) {
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

    model_moveTrayToBoard( board->model, board->selPlayer, col, row, 
                           tileIndex, blankFace );

    return XP_TRUE;
} /* moveTileToBoard */

static XP_Bool
moveKeyTileToBoard( BoardCtxt* board, XP_Key cursorKey )
{
    /* keep compiler happy: assign defaults */
    Tile tile, blankFace;  
    XP_U16 col, row;
    DictionaryCtxt* dict = model_getDictionary( board->model );
    XP_S16 turn = board->selPlayer;
    XP_S16 tileIndex;
    XP_UCHAR buf[2];

    /* Is there a cursor at all? */
    if ( !getArrow( board, &col, &row ) ) {
        return XP_FALSE;
    }

    XP_ASSERT( !TRADE_IN_PROGRESS(board) );

    /* Figure out if we have the tile in the tray  */
    buf[0] = cursorKey;
    buf[1] = '\0';
    tile = dict_tileForString( dict, buf );
    if ( tile == EMPTY_TILE ) { /* not found in dict */
        return XP_FALSE;
    }

    tileIndex = model_trayContains( board->model, turn, tile );
    if ( tileIndex >= 0 ) {
        blankFace = EMPTY_TILE;	/* will be ignored */
    } else {
        Tile blankTile = dict_getBlankTile( dict );
        tileIndex = model_trayContains( board->model, turn, blankTile );
        if ( tileIndex >= 0 ) {	/* there's a blank for it */
            blankFace = tile;
        } else {
            return XP_FALSE;
        }
    }

    return moveTileToBoard( board, col, row, tileIndex, blankFace );
} /* moveKeyTileToBoard */

static void
setArrowFor( BoardCtxt* board, XP_U16 player, XP_U16 col, XP_U16 row )
{
    BoardArrow* arrow = &board->boardArrow[player];
    invalCell( board, arrow->col, arrow->row, XP_FALSE );
    invalCell( board, col, row, XP_FALSE );

    arrow->col = (XP_U8)col;
    arrow->row = (XP_U8)row;

    checkScrollCell( board, player, col, row );
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

XP_Bool
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
        invalArrowCell( board, XP_FALSE );
    }
    return result;
} /* setArrowVisibleFor */

/*****************************************************************************
 * Listener callbacks
 ****************************************************************************/
static void
boardCellChanged( void* p_board, XP_U16 turn, XP_U16 col, XP_U16 row,
                  XP_Bool added )
{
    BoardCtxt* board = (BoardCtxt*)p_board;
    XP_Bool pending, found, ignoreBlank;
    Tile ignoreTile;
    XP_U16 ccol, crow;

    /* for each player, check if the tile overwrites the cursor */
    found = model_getTile( board->model, col, row, XP_TRUE, turn,
                           &ignoreTile, &ignoreBlank, &pending, 
                           (XP_Bool*)NULL );

    XP_ASSERT( !added || found ); /* if added is true so must found be */

    if ( !added && !found ) {
        /* nothing to do */
    } else {
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

        checkScrollCell( board, turn, col, row );
    }

    invalCell( (BoardCtxt*)p_board, col, row, XP_FALSE );
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
        board_selectPlayer( board, nextPlayer );
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
