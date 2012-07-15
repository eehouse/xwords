/* -*-mode: C; fill-column: 78; compile-command: "cd ../linux && make MEMDEBUG=TRUE"; -*- */
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

#include "scorebdp.h"
#include "boardp.h"
#include "model.h"
#include "game.h"
#include "strutils.h"
#include "dbgutil.h"

#ifdef CPLUS
extern "C" {
#endif

static XP_Bool
board_ScoreCallback( void* closure, XP_S16 player, XP_UCHAR* expl, 
                     XP_U16* explLen)
{
    ModelCtxt* model = (ModelCtxt*)closure;
    return model_getPlayersLastScore( model, player,
                                      expl, explLen );
} /* board_ScoreCallback */

#ifdef XWFEATURE_SCOREONEPASS
void
drawScoreBoard( BoardCtxt* board )
{
    if ( board->scoreBoardInvalid ) {
        XP_U16 ii;
        XP_U16 nPlayers = board->gi->nPlayers;
        DrawFocusState dfs = dfsFor( board, OBJ_SCORE );
        ScoresArray scores;
        ModelCtxt* model = board->model;
        XP_S16 nTilesInPool = server_countTilesInPool( board->server );
	    
        if ( board->gameOver ) {
            model_figureFinalScores( model, &scores, NULL );
        } else {
            for ( ii = 0; ii < nPlayers; ++ii ) {
                scores.arr[ii] = model_getPlayerScore( model, ii );
            }
        }

        if ( draw_scoreBegin( board->draw, &board->scoreBdBounds, nPlayers, 
                              scores.arr, nTilesInPool, dfs ) ) {
            XP_S16 curTurn = server_getCurrentTurn( board->server );
            XP_U16 selPlayer = board->selPlayer;
            XP_Rect scoreRect;
            XP_Rect playerRects[nPlayers];
            XP_U16 remDim;
            XP_Bool isVertical = !board->scoreSplitHor;
            XP_Bool remFocussed = XP_FALSE;
            XP_Bool focusAll = XP_FALSE;
            DrawScoreInfo data[nPlayers];
#ifdef KEYBOARD_NAV
            XP_S16 cursorIndex = -1;

            if ( (board->focussed == OBJ_SCORE) && !board->hideFocus ) {
                focusAll = !board->focusHasDived;
                if ( !focusAll ) {
                    cursorIndex = board->scoreCursorLoc;
                    remFocussed = CURSOR_LOC_REM == cursorIndex;                
                    --cursorIndex;                                              
                }
            }
#endif

            XP_MEMSET( playerRects, 0, sizeof(playerRects) );
            XP_MEMSET( data, 0, sizeof(data) );

            scoreRect = board->scoreBdBounds;
            if ( !draw_drawRemText( board->draw, nTilesInPool, 
                                    focusAll || remFocussed, 
                                    &scoreRect ) ) {
                scoreRect.height = scoreRect.width = 0;
            }
            XP_ASSERT( rectContainsRect( &board->scoreBdBounds, &scoreRect ) );
            remDim = isVertical? scoreRect.height : scoreRect.width;
            board->remDim = remDim;
#ifdef KEYBOARD_NAV
            board->remRect = scoreRect;
            if ( 0 == remDim && board->scoreCursorLoc == CURSOR_LOC_REM ) {
                board->scoreCursorLoc = selPlayer + 1;
            }
#endif
            scoreRect = board->scoreBdBounds;
            if ( isVertical ) {
                scoreRect.height -= remDim;
                scoreRect.top += remDim;
            } else {
                scoreRect.width -= remDim;
                scoreRect.left += remDim;
            }

            for ( ii = 0; ii < nPlayers; ++ii ) {
                DrawScoreInfo* dsi = &data[ii];
                LocalPlayer* lp = &board->gi->players[ii];
                dsi->lsc = board_ScoreCallback;
                dsi->lscClosure = model;
#ifdef KEYBOARD_NAV
                if ( (ii == cursorIndex) || focusAll ) {
                    dsi->flags |= CELL_ISCURSOR;
                }
#endif
                dsi->playerNum = ii;
                dsi->totalScore = scores.arr[ii];
                dsi->isTurn = (ii == curTurn);
                dsi->name = emptyStringIfNull(lp->name);
                dsi->selected = board->trayVisState != TRAY_HIDDEN
                    && ii==selPlayer;
                dsi->isRobot = LP_IS_ROBOT(lp);
                dsi->isRemote = !lp->isLocal;
                dsi->nTilesLeft = (nTilesInPool > 0)? -1:
                    model_getNumTilesTotal( model, ii );
            }

            draw_score_drawPlayers( board->draw, &scoreRect, nPlayers, data, 
                                    playerRects );
            for ( ii = 0; ii < nPlayers; ++ii ) {
                XP_Rect* rp = &playerRects[ii];
                board->pti[ii].scoreDims = isVertical ? rp->height : rp->width;
#ifdef KEYBOARD_NAV
                XP_MEMCPY( &board->pti[ii].scoreRects, rp,
                           sizeof(board->pti[ii].scoreRects) );
#endif
            }
            draw_objFinished( board->draw, OBJ_SCORE, 
                              &board->scoreBdBounds, dfs );

            board->scoreBoardInvalid = XP_FALSE;
        }
    }

    drawTimer( board );
} /* drawScoreBoard */
#else

static void
centerIn( XP_Rect* rInner, const XP_Rect* rOuter, XP_U16 width, XP_U16 height )
{
    rInner->width = width;
    rInner->height = height;
    XP_ASSERT( width <= rOuter->width );
    rInner->left = rOuter->left + ( (rOuter->width - width) / 2 );
    XP_ASSERT( height <= rOuter->height );
    rInner->top = rOuter->top + ( (rOuter->height - height) / 2 );
}

typedef struct DrawScoreData {
    DrawScoreInfo dsi;
    XP_U16 height;
    XP_U16 width;
} DrawScoreData;

void
drawScoreBoard( BoardCtxt* board )
{
    if ( board->scoreBoardInvalid ) {
        short ii;

        XP_U16 nPlayers = board->gi->nPlayers;
        XP_ASSERT( nPlayers <= MAX_NUM_PLAYERS );
        if ( nPlayers > 0 ) {
            ModelCtxt* model = board->model;
            XP_S16 curTurn = server_getCurrentTurn( board->server );
            XP_U16 selPlayer = board->selPlayer;
            XP_S16 nTilesInPool = server_countTilesInPool( board->server );
            XP_Rect scoreRect = board->scoreBdBounds;
            XP_S16* adjustDim;
            XP_S16* adjustPt;
            XP_U16 remWidth, remHeight, remDim;
            DrawScoreData* dp;
            DrawScoreData datum[MAX_NUM_PLAYERS];
            ScoresArray scores;
            XP_Bool isVertical = !board->scoreSplitHor;
            XP_Bool remFocussed = XP_FALSE;
            XP_Bool focusAll = XP_FALSE;
#ifdef KEYBOARD_NAV
            XP_S16 cursorIndex = -1;
            if ( (board->focussed == OBJ_SCORE) && !board->hideFocus ) {
                focusAll = !board->focusHasDived;
                if ( !focusAll ) {
                    cursorIndex = board->scoreCursorLoc;
                    remFocussed = CURSOR_LOC_REM == cursorIndex;                
                    --cursorIndex;                                              
                }
            }
#endif
            /* Get the scores from the model or by calculating them based on
               the end-of-game state. */
            if ( board->gameOver ) {
                model_figureFinalScores( model, &scores, NULL );
            } else {
                for ( ii = 0; ii < nPlayers; ++ii ) {
                    scores.arr[ii] = model_getPlayerScore( model, ii );
                }
            }

            if ( draw_scoreBegin( board->draw, &board->scoreBdBounds, nPlayers, 
                                  scores.arr, nTilesInPool, 
                                  dfsFor( board, OBJ_SCORE ) ) ) {
                XP_U16 totalDim = 0; /* not counting rem */
                XP_U16 gotPct;

                /* Let platform decide whether the rem: string should be given
                   any space once there are no tiles left.  On Palm that space
                   is clickable to drop a menu, so will probably leave it. */
                if ( !draw_measureRemText( board->draw, &board->scoreBdBounds, 
                                           nTilesInPool, &remWidth, 
                                           &remHeight ) ) {
                    remWidth = remHeight = 0;
                }
                XP_ASSERT( remWidth <= board->scoreBdBounds.width );
                XP_ASSERT( remHeight <= board->scoreBdBounds.height );
                remDim = isVertical? remHeight : remWidth;

                if ( isVertical ) {
                    adjustPt = &scoreRect.top;
                    adjustDim = &scoreRect.height;
                } else {
                    adjustPt = &scoreRect.left;
                    adjustDim = &scoreRect.width;
                }
                *adjustDim -= remDim;

                /* Give as much room as possible to the entry for the player
                   whose turn it is so name can be drawn.  Do that by
                   formatting that player's score last, and passing each time
                   the amount of space left.  Platform code can then fill that
                   space.
                */

                /* figure spacing for each scoreboard entry */
                XP_MEMSET( &datum, 0, sizeof(datum) );
                totalDim = 0;
                for ( dp = datum, ii = 0; ii < nPlayers; ++ii, ++dp ) {
                    LocalPlayer* lp = &board->gi->players[ii];

                    /* This is a hack! */
                    dp->dsi.lsc = board_ScoreCallback;
                    dp->dsi.lscClosure = model;
#ifdef KEYBOARD_NAV
                    if ( (ii == cursorIndex) || focusAll ) {
                        dp->dsi.flags |= CELL_ISCURSOR;
                    }
#endif
                    dp->dsi.playerNum = ii;
                    dp->dsi.totalScore = scores.arr[ii];
                    dp->dsi.isTurn = (ii == curTurn);
                    dp->dsi.name = emptyStringIfNull(lp->name);
                    dp->dsi.selected = board->trayVisState != TRAY_HIDDEN
                        && ii==selPlayer;
                    dp->dsi.isRobot = LP_IS_ROBOT(lp);
                    dp->dsi.isRemote = !lp->isLocal;
                    dp->dsi.nTilesLeft = (nTilesInPool > 0)? -1:
                        model_getNumTilesTotal( model, ii );

                    draw_measureScoreText( board->draw, &scoreRect,
                                           &dp->dsi, &dp->width, 
                                           &dp->height );

                    XP_ASSERT( dp->width <= scoreRect.width );
                    XP_ASSERT( dp->height <= scoreRect.height );
                    totalDim += isVertical ? dp->height : dp->width;
                }

                gotPct = (*adjustDim * 100) / totalDim;
                for ( dp = datum, ii = 0; ii < nPlayers; ++ii, ++dp ) {
                    if ( isVertical ) {
                        dp->height = (dp->height * gotPct) / 100;
                    } else {
                        dp->width = (dp->width * gotPct) / 100;
                    }
                }

                scoreRect = board->scoreBdBounds; /* reset */

                /* at this point, the scoreRect should be anchored at the
                   scoreboard rect's upper left.  */

                if ( remDim > 0 ) {
                    XP_Rect innerRect;
                    *adjustDim = remDim;
                    centerIn( &innerRect, &scoreRect, remWidth, remHeight );
                    draw_drawRemText( board->draw, &innerRect, &scoreRect, 
                                      nTilesInPool, focusAll || remFocussed );
                    *adjustPt += remDim;
#ifdef KEYBOARD_NAV
                    board->remRect = scoreRect;
                    /* Hack: don't let the cursor disappear if Rem: goes
                       away */
                } else if ( board->scoreCursorLoc == CURSOR_LOC_REM ) {
                    board->scoreCursorLoc = selPlayer + 1;
#endif
                }

                board->remDim = remDim;

                for ( dp = datum, ii = 0; ii < nPlayers; ++dp, ++ii ) {
                    XP_Rect innerRect;
                    XP_U16 dim = isVertical? dp->height:dp->width;
                    *adjustDim = board->pti[ii].scoreDims = dim;

                    centerIn( &innerRect, &scoreRect, dp->width, dp->height );
                    draw_score_drawPlayer( board->draw, &innerRect, &scoreRect,
                                           gotPct, &dp->dsi );
#ifdef KEYBOARD_NAV
                    XP_MEMCPY( &board->pti[ii].scoreRects, &scoreRect, 
                               sizeof(scoreRect) );
#endif
                    *adjustPt += *adjustDim;
                }

                draw_objFinished( board->draw, OBJ_SCORE, 
                                  &board->scoreBdBounds, 
                                  dfsFor( board, OBJ_SCORE ) );

                board->scoreBoardInvalid = XP_FALSE;
            }
        }
    }

    drawTimer( board );
} /* drawScoreBoard */
#endif

static XP_S16
figureSecondsLeft( BoardCtxt* board )
{
    CurGameInfo* gi = board->gi;
    XP_U16 secondsUsed = gi->players[board->selPlayer].secondsUsed;
    XP_U16 secondsAvailable = gi->gameSeconds / gi->nPlayers;
    XP_ASSERT( gi->timerEnabled );
    return secondsAvailable - secondsUsed;
} /* figureSecondsLeft */

void
drawTimer( BoardCtxt* board )
{
    if ( board->gi->timerEnabled ) {
        XP_S16 secondsLeft = figureSecondsLeft( board );

        draw_drawTimer( board->draw, &board->timerBounds,
                        board->selPlayer, secondsLeft );
    }
} /* drawTimer */

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

XP_S16
figureScoreRectTapped( const BoardCtxt* board, XP_U16 xx, XP_U16 yy )
{
    XP_S16 result = -1;
    XP_S16 left;
    XP_U16 nPlayers = board->gi->nPlayers;

    if ( board->scoreSplitHor ) {
        left = xx - board->scoreBdBounds.left;
    } else {
        left = yy - board->scoreBdBounds.top;
    }

    left -= board->remDim;
    if ( left < 0 ) {
        result = CURSOR_LOC_REM;
    } else {
        for ( result = 0; result < nPlayers; ) {
            left -= board->pti[result].scoreDims;
            ++result;           /* increment before test to skip REM */
            if ( left < 0 ) { 
                break;          /* found it! */
            }
        }
        if ( result > nPlayers ) {
            result = -1;
        }
    }
    return result;
} /* figureScoreRectTapped */

/* If the pen also went down on the scoreboard, make the selected player the
 * one closest to the mouse up loc.
 */
#if defined POINTER_SUPPORT || defined KEYBOARD_NAV
XP_Bool
handlePenUpScore( BoardCtxt* board, XP_U16 xx, XP_U16 yy )
{
    XP_Bool result = XP_TRUE;

    XP_S16 rectNum = figureScoreRectTapped( board, xx, yy );

    if ( rectNum == CURSOR_LOC_REM ) {
        util_remSelected( board->util );
    } else if ( --rectNum >= 0 ) {
        board_selectPlayer( board, rectNum, board->allowPeek );
    } else {
        result = XP_FALSE;
    }
    return result;
} /* handlePenUpScore */
#endif

#ifdef KEYBOARD_NAV
static XP_Key
flipKey( XP_Key key, XP_Bool flip ) 
{
    XP_Key result = key;
    if ( flip ) {
        switch( key ) {
        case XP_CURSOR_KEY_DOWN:
            result = XP_CURSOR_KEY_RIGHT; break;
        case XP_CURSOR_KEY_ALTDOWN:
            result = XP_CURSOR_KEY_ALTRIGHT; break;
        case XP_CURSOR_KEY_UP:
            result = XP_CURSOR_KEY_LEFT; break;
        case XP_CURSOR_KEY_ALTUP:
            result = XP_CURSOR_KEY_ALTLEFT; break;
        case XP_CURSOR_KEY_LEFT:
            result = XP_CURSOR_KEY_UP; break;
        case XP_CURSOR_KEY_ALTLEFT:
            result = XP_CURSOR_KEY_ALTUP; break;
        case XP_CURSOR_KEY_RIGHT:
            result = XP_CURSOR_KEY_DOWN; break;
        case XP_CURSOR_KEY_ALTRIGHT:
            result = XP_CURSOR_KEY_ALTDOWN; break;
        default:
            /* not en error -- but we don't modify the key */
            break;
        }
    }
    return result;
} /* flipKey */

XP_Bool
moveScoreCursor( BoardCtxt* board, XP_Key key, XP_Bool preflightOnly, 
                 XP_Bool* pUp )
{
    XP_Bool result = XP_TRUE;
    XP_S16 scoreCursorLoc = board->scoreCursorLoc;
    XP_U16 top = board->gi->nPlayers;
    /* Don't let cursor be 0 if rem square's not shown */
    XP_U16 bottom = (board->remDim > 0) ? 0 : 1;
    XP_Bool up = XP_FALSE;

    /* Depending on scoreboard layout, keys move cursor or leave. */
    key = flipKey( key, board->scoreSplitHor );

    switch ( key ) {
    case XP_CURSOR_KEY_RIGHT:
    case XP_CURSOR_KEY_LEFT:
        up = XP_TRUE;
        break;
    case XP_CURSOR_KEY_DOWN:
        ++scoreCursorLoc;
        break;
    case XP_CURSOR_KEY_UP:
        --scoreCursorLoc;
        break;
    default:
        result = XP_FALSE;
    }
    if ( !up && ((scoreCursorLoc < bottom) || (scoreCursorLoc > top)) ) {
        up = XP_TRUE;
    } else if ( !preflightOnly ) {
        board->scoreCursorLoc = scoreCursorLoc;
        board->scoreBoardInvalid = result;
    }

    *pUp = up;

    return result;
} /* moveScoreCursor */
#endif /* KEYBOARD_NAV */

#ifdef CPLUS
}
#endif
