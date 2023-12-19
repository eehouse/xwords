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
#include "LocalizedStrIncludes.h"

#ifdef CPLUS
extern "C" {
#endif

static XP_Bool
board_ScoreCallback( void* closure, XWEnv xwe, XP_S16 player, LastMoveInfo* lmi )
{
    ModelCtxt* model = (ModelCtxt*)closure;
    return model_getPlayersLastScore( model, xwe, player, lmi );
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

        if ( draw_scoreBegin( board->draw, xwe, &board->scoreBdBounds, nPlayers,
                              scores.arr, nTilesInPool, dfs ) ) {
            XP_U16 selPlayer = board->selPlayer;
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

            XP_Rect scoreRect = board->scoreBdBounds;
            if ( !draw_drawRemText( board->draw, xwe, nTilesInPool,
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
                dsi->isTurn = server_isPlayersTurn( board->server, ii );
                dsi->name = emptyStringIfNull(lp->name);
                dsi->selected = board->trayVisState != TRAY_HIDDEN
                    && ii==selPlayer;
                dsi->isRobot = LP_IS_ROBOT(lp);
                dsi->isRemote = !lp->isLocal;
                dsi->nTilesLeft = (nTilesInPool > 0)? -1:
                    model_getNumTilesTotal( model, ii );
            }

            draw_score_drawPlayers( board->draw, xwe, &scoreRect, nPlayers,
                                    data, playerRects );
            for ( ii = 0; ii < nPlayers; ++ii ) {
                XP_Rect* rp = &playerRects[ii];
                board->pti[ii].scoreDims = isVertical ? rp->height : rp->width;
#ifdef KEYBOARD_NAV
                XP_MEMCPY( &board->pti[ii].scoreRects, rp,
                           sizeof(board->pti[ii].scoreRects) );
#endif
            }
            draw_objFinished( board->draw, xwe, OBJ_SCORE,
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

typedef struct _DrawScoreData {
    DrawScoreInfo dsi;
    XP_U16 height;
    XP_U16 width;
} DrawScoreData;

void
drawScoreBoard( BoardCtxt* board, XWEnv xwe )
{
    if ( board->scoreBoardInvalid ) {
        short ii;

        XP_U16 nPlayers = board->gi->nPlayers;
        XP_ASSERT( nPlayers <= MAX_NUM_PLAYERS );
        if ( nPlayers > 0 ) {
            ModelCtxt* model = board->model;
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
            model_getCurScores( model, &scores, board->gameOver );

            if ( draw_scoreBegin( board->draw, xwe, &board->scoreBdBounds,
                                  nPlayers, scores.arr, nTilesInPool,
                                  dfsFor( board, OBJ_SCORE ) ) ) {
                XP_U16 totalDim = 0; /* not counting rem */
                XP_U16 gotPct;

                /* Let platform decide whether the rem: string should be given
                   any space once there are no tiles left.  On Palm that space
                   is clickable to drop a menu, so will probably leave it. */
                if ( !draw_measureRemText( board->draw, xwe, &board->scoreBdBounds,
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
                XP_U16 missingPlayers = server_getMissingPlayers( board->server );
                for ( dp = datum, ii = 0; ii < nPlayers; ++ii, ++dp ) {
                    const LocalPlayer* lp = &board->gi->players[ii];
                    XP_Bool isMissing = 0 != ((1 << ii) & missingPlayers);

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
                    dp->dsi.isTurn = server_isPlayersTurn( board->server, ii );
                    dp->dsi.selected = board->trayVisState != TRAY_HIDDEN
                        && ii==selPlayer;
                    dp->dsi.isRobot = LP_IS_ROBOT(lp);
                    dp->dsi.isRemote = !lp->isLocal;
                    XP_ASSERT( !isMissing || dp->dsi.isRemote );
                    if ( dp->dsi.isRemote && isMissing ) {
                        XP_U16 len = VSIZE(dp->dsi.name);
                        util_getInviteeName( board->util, xwe, ii, dp->dsi.name, &len );
                        if ( !dp->dsi.name[0] || len == 0 ) {
                            const XP_UCHAR* tmp = dutil_getUserString( board->dutil, xwe,
                                                                       STR_PENDING_PLAYER );
                            XP_STRCAT( dp->dsi.name, tmp );
                        }
                    } else {
                        const XP_UCHAR* tmp = emptyStringIfNull( lp->name );
                        XP_STRCAT( dp->dsi.name, tmp );
                    }
                    dp->dsi.nTilesLeft = (nTilesInPool > 0)? -1:
                        model_getNumTilesTotal( model, ii );

                    draw_measureScoreText( board->draw, xwe, &scoreRect,
                                           &dp->dsi, &dp->width, 
                                           &dp->height );

                    XP_ASSERT( dp->width <= scoreRect.width );
                    XP_ASSERT( dp->height <= scoreRect.height );
                    totalDim += isVertical ? dp->height : dp->width;
                }

                if ( 0 < totalDim ) {
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
                        draw_drawRemText( board->draw, xwe, &innerRect, &scoreRect,
                                          nTilesInPool, 
                                          focusAll || remFocussed );
                        *adjustPt += remDim;
#ifdef KEYBOARD_NAV
                        board->remRect = innerRect;
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

                        centerIn( &innerRect, &scoreRect, dp->width, 
                                  dp->height );
                        draw_score_drawPlayer( board->draw, xwe, &innerRect,
                                               &scoreRect, gotPct, &dp->dsi );
#ifdef KEYBOARD_NAV
                        XP_MEMCPY( &board->pti[ii].scoreRects, &scoreRect, 
                                   sizeof(scoreRect) );
#endif
                        *adjustPt += *adjustDim;
                    }
                }

                draw_objFinished( board->draw, xwe, OBJ_SCORE,
                                  &board->scoreBdBounds, 
                                  dfsFor( board, OBJ_SCORE ) );

                board->scoreBoardInvalid = XP_FALSE;
            }
        }
    }

    drawTimer( board, xwe );
} /* drawScoreBoard */
#endif

void
drawTimer( const BoardCtxt* board, XWEnv xwe )
{
    if ( !!board->draw && board->gi->timerEnabled ) {
        XP_S16 secondsLeft = server_getTimerSeconds( board->server, xwe,
                                                     board->selPlayer );
        XP_Bool turnDone = board->gi->inDuplicateMode
            ? server_dupTurnDone( board->server, board->selPlayer )
            : XP_FALSE;
        draw_drawTimer( board->draw, xwe, &board->timerBounds,
                        board->selPlayer, secondsLeft, turnDone );
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
handlePenUpScore( BoardCtxt* board, XWEnv xwe, XP_U16 xx, XP_U16 yy, XP_Bool altDown )
{
    XP_Bool result = XP_TRUE;

    XP_S16 rectNum = figureScoreRectTapped( board, xx, yy );

    if ( rectNum == CURSOR_LOC_REM ) {
        util_remSelected( board->util, xwe );
    } else if ( --rectNum >= 0 ) {
        XP_Bool canSwitch = board->gameOver || board->allowPeek;
        if ( altDown || !canSwitch ) {
            penTimerFiredScore( board, xwe );
        } else {
            board_selectPlayer( board, xwe, rectNum, XP_TRUE );
        }
    } else {
        result = XP_FALSE;
    }
    return result;
} /* handlePenUpScore */
#endif

void
penTimerFiredScore( const BoardCtxt* board, XWEnv xwe )
{
    XP_S16 scoreIndex = figureScoreRectTapped( board, board->penDownX, 
                                               board->penDownY );
    /* I've seen this assert fire on simulator.  No log is kept so I can't
       tell why, but might want to test and do nothing in this case.  */
    /* XP_ASSERT( player >= 0 ); */
    if ( scoreIndex > CURSOR_LOC_REM ) {
        XP_U16 player = scoreIndex - 1;
#ifdef XWFEATURE_MINIWIN
        const XP_UCHAR* format;
        XP_UCHAR scoreExpl[48];
        XP_U16 explLen;
        LocalPlayer* lp = &board->gi->players[player];
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
#else
        util_playerScoreHeld( board->util, xwe, player );
#endif
    }

}

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
