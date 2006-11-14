/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2006 by Eric House (xwords@eehouse.org).  All rights reserved.
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

typedef struct DrawScoreData {
    DrawScoreInfo dsi;
    XP_U16 height;
    XP_U16 width;
} DrawScoreData;

void
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
#ifdef KEYBOARD_NAV
            XP_Rect cursorRect;
            XP_Rect* cursorRectP = NULL;
            XP_Bool focusAll = XP_FALSE;
            XP_S16 cursorIndex = -1;
            if ( board->focussed == OBJ_SCORE ) {
                focusAll = !board->focusHasDived;
                if ( !focusAll ) {
                    cursorIndex = board->scoreCursorLoc;
                }
            }
#endif
            draw_scoreBegin( board->draw, &board->scoreBdBounds, nPlayers, 
                             dfsFor( board, OBJ_SCORE ) );

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
            XP_MEMSET( &datum, 0, sizeof(datum) );
            for ( dp = datum, i = 0; i < nPlayers; ++i, ++dp ) {
                LocalPlayer* lp = &board->gi->players[i];

                /* This is a hack! */
                dp->dsi.lsc = board_ScoreCallback;
                dp->dsi.lscClosure = model;
#ifdef KEYBOARD_NAV
                if ( (i == cursorIndex) || focusAll ) {
                    dp->dsi.flags |= CELL_ISCURSOR;
                }
#endif
                dp->dsi.playerNum = i;
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
            XP_ASSERT( *adjustDim >= totalDim );
            extra = (*adjustDim - totalDim) / nShares;

            /* at this point, the scoreRect should be anchored at the
               scoreboard rect's upper left.  */

            if ( remDim > 0 ) {
                *adjustDim = remDim;

                draw_drawRemText( board->draw, &scoreRect, &scoreRect, 
                                  nTilesInPool );

                *adjustPt += remDim;
            }

            board->remDim = remDim;	/* save now so register can be reused */

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

                draw_score_drawPlayer( board->draw, &innerRect, &scoreRect,
                                       &dp->dsi );
#ifdef KEYBOARD_NAV
                if ( i == cursorIndex ) {
                    cursorRect = scoreRect;
                    cursorRectP = &cursorRect;
                }
#endif
                *adjustPt += *adjustDim;
            }

#ifdef KEYBOARD_NAV
            if ( !!cursorRectP ) {
                draw_drawCursor( board->draw, OBJ_SCORE, cursorRectP );
            }
#endif

            draw_objFinished( board->draw, OBJ_SCORE, &board->scoreBdBounds, 
                              dfsFor( board, OBJ_SCORE ) );
        }

        board->scoreBoardInvalid = XP_FALSE;
    }
	    
    drawTimer( board );
} /* drawScoreBoard */

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

        draw_drawTimer( board->draw, &board->timerBounds, &board->timerBounds, 
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
XP_Bool
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

#ifdef KEYBOARD_NAV
XP_Bool
moveScoreCursor( BoardCtxt* board, XP_Key key, XP_Bool* pUp )
{
    XP_Bool result = XP_TRUE;
    XP_U16 nPlayers = board->gi->nPlayers;
    XP_S16 scoreCursorLoc = board->scoreCursorLoc;
    XP_Bool up = XP_FALSE;

    /* Depending on scoreboard layout, keys move cursor or leave. */
    key = flipKey( key, board->scoreSplitHor );

    switch ( key ) {
    case XP_CURSOR_KEY_DOWN:
        ++scoreCursorLoc;
        break;
    case XP_CURSOR_KEY_RIGHT:
        up = XP_TRUE;
        break;
    case XP_CURSOR_KEY_UP:
        --scoreCursorLoc;
        break;
    case XP_CURSOR_KEY_LEFT:
        up = XP_TRUE;
        break;
    default:
        result = XP_FALSE;
    }
    if ( !up && ((scoreCursorLoc < 0) || (scoreCursorLoc >= nPlayers)) ) {
        up = XP_TRUE;
    } else {
        board->scoreCursorLoc = scoreCursorLoc;
        board->scoreBoardInvalid = XP_TRUE;
    }

    *pUp = up;

    return result;
} /* moveScoreCursor */
#endif /* KEYBOARD_NAV */

#ifdef CPLUS
}
#endif
