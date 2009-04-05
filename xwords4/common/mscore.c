/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1998-2001 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "modelp.h"
#include "util.h"
#include "engine.h"
#include "game.h"
#include "LocalizedStrIncludes.h"

#ifdef CPLUS
extern "C" {
#endif

#define IMPOSSIBLY_LOW_PENALTY (-20*MAX_TRAY_TILES)

/****************************** prototypes ******************************/
static XP_Bool isLegalMove( ModelCtxt* model, MoveInfo* moves, XP_Bool silent );
static XP_U16 word_multiplier( const ModelCtxt* model, 
                               XP_U16 col, XP_U16 row );
static XP_U16 find_end( const ModelCtxt* model, XP_U16 col, XP_U16 row, 
                        XP_Bool isHorizontal );
static XP_U16 find_start( const ModelCtxt* model, XP_U16 col, XP_U16 row, 
                          XP_Bool isHorizontal );
static XP_S16 checkScoreMove( ModelCtxt* model, XP_S16 turn, 
                              EngineCtxt* engine, XWStreamCtxt* stream, 
                              XP_Bool silent, WordNotifierInfo* notifyInfo );
static XP_U16 scoreWord( const ModelCtxt* model, MoveInfo* movei,
                         EngineCtxt* engine, XWStreamCtxt* stream, 
                         WordNotifierInfo* notifyInfo, XP_UCHAR* mainWord,
                         XP_U16 mainWordLen );

/* for formatting when caller wants an explanation of the score.  These live
   in separate function called only when stream != NULL so that they'll have
   as little impact as possible on the speed when the robot's looking for FAST
   scoring */
typedef struct WordScoreFormatter {
    DictionaryCtxt* dict;

    XP_UCHAR fullBuf[80];
    XP_UCHAR wordBuf[MAX_ROWS+1];
    XP_U16 bufLen, nTiles;

    XP_Bool firstPass;
} WordScoreFormatter;
static void wordScoreFormatterInit( WordScoreFormatter* fmtr, 
                                    DictionaryCtxt* dict );
static void wordScoreFormatterAddTile( WordScoreFormatter* fmtr, Tile tile, 
                                       XP_U16 tileMultiplier, 
                                       XP_Bool isBlank );
static void wordScoreFormatterFinish( WordScoreFormatter* fmtr, Tile* word, 
                                      XWStreamCtxt* stream, XP_UCHAR* mainWord,
                                      XP_U16 mainWordLen );
static void formatWordScore( XWStreamCtxt* stream, XP_U16 wordScore, 
                             XP_U16 moveMultiplier );
static void formatSummary( XWStreamCtxt* stream, const ModelCtxt* model, 
                           XP_U16 score );


/* Calculate the score of the current move as it stands.  Flag the score
 * current so we won't have to do this again until something changes to
 * invalidate the score.
 */
static void
scoreCurrentMove( ModelCtxt* model, XP_S16 turn, XWStreamCtxt* stream )
{
    PlayerCtxt* player = &model->players[turn];
    XP_S16 score;

    XP_ASSERT( !player->curMoveValid );

    /* recalc goes here */
    score = checkScoreMove( model, turn, (EngineCtxt*)NULL, stream,
                            XP_TRUE, (WordNotifierInfo*)NULL );
    XP_ASSERT( score >= 0 || score == ILLEGAL_MOVE_SCORE );

    player->curMoveScore = score;
    player->curMoveValid = XP_TRUE;
} /* scoreCurrentMove */

void
adjustScoreForUndone( ModelCtxt* model, MoveInfo* mi, XP_U16 turn )
{
    XP_U16 moveScore;
    PlayerCtxt* player = &model->players[turn];

    if ( mi->nTiles == 0 ) {
        moveScore = 0;
    } else {
        moveScore = figureMoveScore( model, mi, (EngineCtxt*)NULL, 
                                     (XWStreamCtxt*)NULL, 
                                     (WordNotifierInfo*)NULL, NULL, 0 );
    }
    player->score -= moveScore;
    player->curMoveScore = 0;
    player->curMoveValid = XP_TRUE;
} /* adjustScoreForUndone */

XP_Bool
model_checkMoveLegal( ModelCtxt* model, XP_S16 turn, XWStreamCtxt* stream,
                      WordNotifierInfo* notifyInfo )
{
    XP_S16 score;
    score = checkScoreMove( model, turn, (EngineCtxt*)NULL, stream, XP_FALSE, 
                            notifyInfo );
    return score != ILLEGAL_MOVE_SCORE;
} /* model_checkMoveLegal */

void
invalidateScore( ModelCtxt* model, XP_S16 turn )
{
    model->players[turn].curMoveValid = XP_FALSE;
} /* invalidateScore */

XP_Bool
getCurrentMoveScoreIfLegal( ModelCtxt* model, XP_S16 turn,
                            XWStreamCtxt* stream, XP_S16* score )
{
    PlayerCtxt* player = &model->players[turn];
    if ( !player->curMoveValid ) {
        scoreCurrentMove( model, turn, stream );
    }

    *score = player->curMoveScore;
    return player->curMoveScore != ILLEGAL_MOVE_SCORE;
} /* getCurrentMoveScoreIfLegal */

XP_S16
model_getPlayerScore( ModelCtxt* model, XP_S16 player )
{
    return model->players[player].score;
} /* model_getPlayerScore */

/* Based on the current scores based on tiles played and the tiles left in the
 * tray, return an array giving the left-over-tile-adjusted scores for each
 * player.
 */
void
model_figureFinalScores( ModelCtxt* model, ScoresArray* finalScoresP,
                         ScoresArray* tilePenaltiesP )
{
    XP_S16 ii, jj;
    XP_S16 penalties[MAX_NUM_PLAYERS];
    XP_S16 totalPenalty;
    XP_U16 nPlayers = model->nPlayers;
    XP_S16 firstDoneIndex = -1; /* not set unless FIRST_DONE_BONUS is set */
    const TrayTileSet* tray;
    PlayerCtxt* player;
    DictionaryCtxt* dict = model_getDictionary( model );
    CurGameInfo* gi = model->vol.gi;

    if ( !!finalScoresP ) {
        XP_MEMSET( finalScoresP, 0, sizeof(*finalScoresP) );
    }

    totalPenalty = 0;
    for ( player = model->players, ii = 0; ii < nPlayers; ++player, ++ii ) {
        tray = model_getPlayerTiles( model, ii );

        penalties[ii] = 0;

        /* if there are no tiles left and this guy's the first done, make a
           note of it in case he's to get a bonus.  Note that this assumes
           only one player can be out of tiles. */
        if ( (tray->nTiles == 0) && (firstDoneIndex == -1) ) {
            firstDoneIndex = ii;
        } else {
            for ( jj = tray->nTiles-1; jj >= 0; --jj ) {
                penalties[ii] += dict_getTileValue( dict, tray->tiles[jj] );
            }
        }

        /* include tiles in pending move too for the player whose turn it
           is. */
        for ( jj = player->nPending - 1; jj >= 0; --jj ) {
            Tile tile = player->pendingTiles[jj].tile;
            penalties[ii] += dict_getTileValue(dict, 
                                               (Tile)(tile & TILE_VALUE_MASK));
        }
        totalPenalty += penalties[ii];
    }

    /* now total everybody's scores */
    for ( ii = 0; ii < nPlayers; ++ii ) {
        XP_S16 penalty = (ii == firstDoneIndex)? totalPenalty: -penalties[ii];

        if ( !!finalScoresP ) {
            XP_S16 score = model_getPlayerScore( model, ii );
            if ( gi->timerEnabled ) {
                score -= player_timePenalty( gi, ii );
            }
            finalScoresP->arr[ii] = score + penalty;
        }

        if ( !!tilePenaltiesP ) {
            tilePenaltiesP->arr[ii] = penalty;
        }
    }
} /* model_figureFinalScores */

/* checkScoreMove.
 * Negative score means illegal.
 */
static XP_S16
checkScoreMove( ModelCtxt* model, XP_S16 turn, EngineCtxt* engine, 
                XWStreamCtxt* stream, XP_Bool silent, 
                WordNotifierInfo* notifyInfo ) 
{
    XP_Bool isHorizontal;
    XP_S16 score = ILLEGAL_MOVE_SCORE;
    PlayerCtxt* player = &model->players[turn];

    XP_ASSERT( player->nPending <= MAX_TRAY_TILES );

    if ( player->nPending == 0 ) {
        score = 0;

        if ( !!stream ) {
            formatSummary( stream, model, 0 );
        }

    } else if ( tilesInLine( model, turn, &isHorizontal ) ) {
        MoveInfo moveInfo;

        normalizeMoves( model, turn, isHorizontal, &moveInfo );

        if ( isLegalMove( model, &moveInfo, silent ) ) {
            score = figureMoveScore( model, &moveInfo, engine, stream, 
                                     notifyInfo, NULL, 0 );
        }
    } else if ( !silent ) { /* tiles out of line */
        util_userError( model->vol.util, ERR_TILES_NOT_IN_LINE );
    }
    return score;
} /* checkScoreMove */

XP_Bool
tilesInLine( ModelCtxt* model, XP_S16 turn, XP_Bool* isHorizontal ) 
{
    XP_Bool xIsCommon, yIsCommon;
    PlayerCtxt* player = &model->players[turn];
    PendingTile* pt = player->pendingTiles;
    XP_U16 commonX = pt->col;
    XP_U16 commonY = pt->row;
    short i;

    xIsCommon = yIsCommon = XP_TRUE;

    for ( i = 1; ++pt, i < player->nPending; ++i ) {
        // test the boolean first in case it's already been made false
        // (to save time)
        if ( xIsCommon && (pt->col != commonX) ) {
            xIsCommon = XP_FALSE;
        }
        if ( yIsCommon && (pt->row != commonY) ) {
            yIsCommon = XP_FALSE;
        }
    }
    *isHorizontal = !xIsCommon; // so will be vertical if both true
    return xIsCommon || yIsCommon;
} /* tilesInLine */

void
normalizeMoves( ModelCtxt* model, XP_S16 turn, XP_Bool isHorizontal,
                MoveInfo* moveInfo )
{
    XP_S16 lowCol, i, j, thisCol; /* unsigned is a problem on palm */
    PlayerCtxt* player = &model->players[turn];
    XP_U16 nTiles = player->nPending;
    XP_S16 lastTaken;
    short lowIndex = 0;
    PendingTile* pt;

    moveInfo->isHorizontal = isHorizontal;
    moveInfo->nTiles = (XP_U8)nTiles;

    lastTaken = -1;
    for ( i = 0; i < nTiles; ++i ) {
        lowCol = 100; /* high enough to always be changed */
        for ( j = 0; j < nTiles; ++j ) {
            pt = &player->pendingTiles[j];
            thisCol = isHorizontal? pt->col:pt->row;
            if (thisCol < lowCol && thisCol > lastTaken ) {
                lowCol = thisCol;
                lowIndex = j;
            }
        }
        /* we've found the next to transfer (4 bytes smaller without a temp
           local ptr. */
        pt = &player->pendingTiles[lowIndex];
        lastTaken = lowCol;
        moveInfo->tiles[i].varCoord = (XP_U8)lastTaken;

        moveInfo->tiles[i].tile = pt->tile;
    }

    pt = &player->pendingTiles[0];
    moveInfo->commonCoord = isHorizontal? pt->row:pt->col;
} /* normalizeMoves */

static XP_Bool
modelIsEmptyAt( const ModelCtxt* model, XP_U16 col, XP_U16 row )
{
    Tile tile;
    XP_Bool ignore;
    XP_Bool found;

    found = model_getTile( model, col, row, XP_FALSE, -1, &tile,
                           &ignore, &ignore, (XP_Bool*)NULL );
    return !found;
} /* modelIsEmptyAt */

/*****************************************************************************
 * Called only after moves have been confirmed to be in the same row, this
 * function works whether the word is horizontal or vertical.
 *
 * For a move to be legal, either of the following must be true: a)
 * if there are squares between those added in this move they must be occupied
 * by previously placed pieces; or b) if these pieces are contiguous then at
 * least one must touch a previously played piece (unless this is the first
 * move) NOTE: this function does not verify that a newly placed piece is on an
 * empty square.  It's assumed that the calling code, most likely that which
 * handles dragging the tiles, will have taken care of that.
 ****************************************************************************/
static XP_Bool
isLegalMove( ModelCtxt* model, MoveInfo* mInfo, XP_Bool silent )
{
    XP_S16 high, low;
    XP_S16 col, row;
    XP_S16* incr;
    XP_S16* commonP;
    XP_U16 star_row = model_numRows(model) / 2;

    XP_S16 nTiles = mInfo->nTiles;
    MoveInfoTile* moves = mInfo->tiles;
    XP_U16 commonCoord = mInfo->commonCoord;

    /* First figure out what the low and high coordinates are in the dimension
       not in common */
    low = moves[0].varCoord;
    high = moves[nTiles-1].varCoord;
    XP_ASSERT( (nTiles == 1) || (low < high) );

    if ( mInfo->isHorizontal ) {
        row = commonCoord;
        incr = &col;
        commonP = &row;
    } else {
        col = commonCoord;
        incr = &row;
        commonP = &col;
    }

    /* are we looking at 2a above? */
    if ( (high - low + 1) > nTiles ) {
        /* there should be no empty tiles between the ends */
        MoveInfoTile* newTile = moves; /* the newly placed tile to be checked */
        for ( *incr = low; *incr <= high; ++*incr ) {
            if ( newTile->varCoord == *incr ) {
                ++newTile;
            } else if ( modelIsEmptyAt( model, col, row ) ) {
                if ( !silent ) {
                    util_userError( model->vol.util, ERR_NO_EMPTIES_IN_TURN );
                }
                return XP_FALSE;
            }
        }
        XP_ASSERT( newTile == &moves[nTiles] );
        return XP_TRUE;

        /* else we're looking at 2b: make sure there's some contact UNLESS
           this is the first move */
    } else {
        /* check the ends first */
        if ( low != 0 ) {
            *incr = low - 1;
            if ( !modelIsEmptyAt( model, col, row ) ) {
                return XP_TRUE;
            }
        }
        if ( high != MAX_ROWS-1 ) {
            *incr = high+1;
            if ( !modelIsEmptyAt( model, col, row ) ) {
                return XP_TRUE;
            }
        }
        /* now the neighbors above... */
        if ( commonCoord != 0 ) {
            --*commonP; /* decrement whatever's not being looped over */
            for ( *incr = low; *incr <= high; ++*incr ) {
                if ( !modelIsEmptyAt( model, col, row ) ) {
                    return XP_TRUE;
                }
            }
            ++*commonP;/* undo the decrement */
        }
        /* ...and below */
        if ( commonCoord <= MAX_ROWS - 1 ) {
            ++*commonP;
            for ( *incr = low; *incr <= high; ++*incr ) {
                if ( !modelIsEmptyAt( model, col, row ) ) {
                    return XP_TRUE;
                }
            }
            --*commonP;
        }

        /* if we got here, it's illegal unless this is the first move -- i.e.
           unless one of the tiles is on the STAR */
        if ( ( commonCoord == star_row) && 
             ( low <= star_row) && ( high >= star_row ) ) {
            if ( nTiles > 1 ) {
                return XP_TRUE;
            } else {
                if ( !silent ) {
                    util_userError(model->vol.util, ERR_TWO_TILES_FIRST_MOVE);
                }
                return XP_FALSE;
            }
        } else {
            if ( !silent ) {
                util_userError( model->vol.util, ERR_TILES_MUST_CONTACT );
            }
            return XP_FALSE;
        }
    }
    XP_ASSERT( XP_FALSE );
    return XP_FALSE; /* keep compiler happy */
} /* isLegalMove */

XP_U16
figureMoveScore( const ModelCtxt* model, MoveInfo* moveInfo, 
                 EngineCtxt* engine, XWStreamCtxt* stream, 
                 WordNotifierInfo* notifyInfo, XP_UCHAR* mainWord,
                 XP_U16 mainWordLen )
{
    XP_U16 col, row;
    XP_U16* incr;
    XP_U16 oneScore;
    XP_U16 score = 0;
    short i;
    short moveMultiplier = 1;
    short multipliers[MAX_TRAY_TILES];
    MoveInfo tmpMI;
    MoveInfoTile* tiles;
    XP_U16 nTiles = moveInfo->nTiles;

    XP_ASSERT( nTiles > 0 );

    if ( moveInfo->isHorizontal ) {
        row = moveInfo->commonCoord;
        incr = &col;
    } else {
        col = moveInfo->commonCoord;
        incr = &row;
    }

    for ( i = 0; i < nTiles; ++i ) {
        *incr = moveInfo->tiles[i].varCoord;
        moveMultiplier *= multipliers[i] = word_multiplier( model, col, row );
    }

    oneScore = scoreWord( model, moveInfo, (EngineCtxt*)NULL, stream,
                          notifyInfo, mainWord, mainWordLen );
    if ( !!stream ) {
        formatWordScore( stream, oneScore, moveMultiplier );
    }
    oneScore *= moveMultiplier;
    score += oneScore;

    /* set up the invariant slots in tmpMI */
    tmpMI.isHorizontal = !moveInfo->isHorizontal;
    tmpMI.nTiles = 1;
    tmpMI.tiles[0].varCoord = moveInfo->commonCoord;

    for ( i = 0, tiles = moveInfo->tiles; i < nTiles; ++i, ++tiles ) {

        /* Moves using only one tile will sometimes score only in the
           crosscheck direction.  Score may still be 0 after the call to
           scoreWord above.  Keep trying to get some text in mainWord until
           something's been scored. */
        if ( score > 0 ) {
            mainWord = NULL;
        }

        tmpMI.commonCoord = tiles->varCoord;
        tmpMI.tiles[0].tile = tiles->tile;

        oneScore = scoreWord( model, &tmpMI, engine, stream, 
                              notifyInfo, mainWord, mainWordLen );
        if ( !!stream ) {
            formatWordScore( stream, oneScore, multipliers[i] );
        }
        oneScore *= multipliers[i];
        score += oneScore;
    }

    /* did he use all 7 tiles? */
    if ( nTiles == MAX_TRAY_TILES ) {
        score += EMPTIED_TRAY_BONUS;

        if ( !!stream ) {
            const XP_UCHAR* bstr = util_getUserString( model->vol.util, 
                                                       STR_BONUS_ALL );
            stream_catString( stream, bstr );
        }
    }

    if ( !!stream ) {
        formatSummary( stream, model, score );
    }

    return score;
} /* figureMoveScore */

static XP_U16
word_multiplier( const ModelCtxt* model, XP_U16 col, XP_U16 row )
{
    XWBonusType bonus = util_getSquareBonus( model->vol.util, model, col, row );
    switch ( bonus ) {
    case BONUS_DOUBLE_WORD:
        return 2;
    case BONUS_TRIPLE_WORD:
        return 3;
    default:
        return 1;
    }
} /* word_multiplier */

static XP_U16
tile_multiplier( const ModelCtxt* model, XP_U16 col, XP_U16 row )
{
    XWBonusType bonus = util_getSquareBonus( model->vol.util, model,
                                             col, row );
    switch ( bonus ) {
    case BONUS_DOUBLE_LETTER:
        return 2;
    case BONUS_TRIPLE_LETTER:
        return 3;
    default:
        return 1;
    }
} /* tile_multiplier */

static XP_U16
scoreWord( const ModelCtxt* model, MoveInfo* movei, /* new tiles */
           EngineCtxt* engine,/* for crosswise caching */
           XWStreamCtxt* stream, 
           WordNotifierInfo* notifyInfo,
           XP_UCHAR* mainWord, XP_U16 mainWordLen )
{
    XP_U16 tileMultiplier;
    XP_U16 restScore = 0;
    XP_U16 scoreFromCache;
    XP_U16 thisTileValue;
    XP_U16 nTiles = movei->nTiles;
    Tile tile;
    XP_U16 start, end;
    XP_U16* incr;
    XP_U16 col, row;
    MoveInfoTile* tiles = movei->tiles;
    XP_U16 firstCoord = tiles->varCoord;
    DictionaryCtxt* dict = model->vol.dict;

    if ( movei->isHorizontal ) {
        row = movei->commonCoord;
        incr = &col;
    } else {
        col = movei->commonCoord;
        incr = &row;
    }

    *incr = tiles[nTiles-1].varCoord;
    end = find_end( model, col, row, movei->isHorizontal );

    /* This is the value *incr needs to start with below */
    *incr = tiles[0].varCoord;
    start = find_start( model, col, row, movei->isHorizontal );

    if ( (end - start) >= 1 ) { /* one-letter word: score 0 */
        WordScoreFormatter fmtr;
        if ( !!stream || !!mainWord ) {
            wordScoreFormatterInit( &fmtr, dict );
        }

        if ( IS_BLANK(tiles->tile) ) {
            tile = dict_getBlankTile( dict );
        } else {
            tile = tiles->tile & TILE_VALUE_MASK;
        }
        thisTileValue = dict_getTileValue( dict, tile );

        XP_ASSERT( *incr == tiles[0].varCoord );
        thisTileValue *= tile_multiplier( model, col, row );

        XP_ASSERT( engine == NULL || nTiles == 1 );

        if ( engine != NULL ) {
            XP_ASSERT( nTiles==1 );
            scoreFromCache = engine_getScoreCache( engine, 
                                                   movei->commonCoord );
        }

        /* for a while, at least, calculate and use the cached crosscheck score
         * each time through in the debug case */
        if ( 0 ) { /* makes keeping parens balanced easier */
#ifdef DEBUG
        } else if ( 1 ) {
#else
        } else if ( engine == NULL ) {
#endif
            Tile checkWordBuf[MAX_ROWS];
            Tile* curTile = checkWordBuf;

            for ( *incr = start; *incr <= end; ++*incr ) {
                XP_U16 tileScore = 0;
                XP_Bool isBlank;

                /* a new move? */
                if ( (nTiles > 0) && (*incr == tiles->varCoord) ) {
                    tile = tiles->tile & TILE_VALUE_MASK;
                    isBlank = IS_BLANK(tiles->tile);
                    /* don't call localGetBlankTile when in silent (robot called)
                     * mode, as the blank won't be known there.  (Assert will
                     * fail.) */

                    tileMultiplier = tile_multiplier( model, col, row );
                    ++tiles;
                    --nTiles;
                } else { /* placed on the board before this move */
                    XP_Bool ignore;
                    tileMultiplier = 1;

                    (void)model_getTile( model, col, row, XP_FALSE, -1, &tile,
                                         &isBlank, &ignore, (XP_Bool*)NULL );

                    XP_ASSERT( (tile & TILE_VALUE_MASK) == tile );
                }

                *curTile++ = tile; /* save in case we're checking phonies */

                if ( !!stream || !!mainWord ) {
                    wordScoreFormatterAddTile( &fmtr, tile, tileMultiplier, 
                                               isBlank );
                }

                if ( isBlank ) {
                    tile = dict_getBlankTile( dict );
                }
                tileScore = dict_getTileValue( dict, tile );

                /* The first tile in the move is already accounted for in
                   thisTileValue, so skip it here. */
                if ( *incr != firstCoord ) {
                    restScore += tileScore * tileMultiplier;
                }
            } /* for each tile */

            if ( !!notifyInfo ) {
                XP_U16 len = curTile - checkWordBuf;
                XP_Bool legal = engine_check( dict, checkWordBuf, len );

                if ( !legal ) {
                    XP_UCHAR buf[(MAX_ROWS*2)+1];
                    dict_tilesToString( dict, checkWordBuf, len, buf, 
                                        sizeof(buf) );
                    (*notifyInfo->proc)( buf, notifyInfo->closure );
                }
            }

            if ( !!stream || !!mainWord ) {
                wordScoreFormatterFinish( &fmtr, checkWordBuf, stream, 
                                          mainWord, mainWordLen );
            }
#ifdef DEBUG

        } else if ( engine != NULL ) {
#else
        } else { /* non-debug case we know it's non-null */
#endif
            XP_ASSERT( nTiles==1 );
            XP_ASSERT( engine_getScoreCache( engine, movei->commonCoord ) 
                       == restScore );
            restScore = engine_getScoreCache( engine, movei->commonCoord );
        }

        restScore += thisTileValue;
    }

    return restScore;
} /* scoreWord */

static XP_U16
find_start( const ModelCtxt* model, XP_U16 col, XP_U16 row, 
            XP_Bool isHorizontal )
{
    XP_U16* incr = isHorizontal? &col: &row;

    for ( ; ; ) {
        if ( *incr == 0 ) {
            return 0;
        } else {
            --*incr;
            if ( modelIsEmptyAt( model, col, row ) ) {
                return *incr + 1;
            }
        }
    }
} /* find_start */

static XP_U16
find_end( const ModelCtxt* model, XP_U16 col, XP_U16 row, 
          XP_Bool isHorizontal ) 
{
    XP_U16* incr = isHorizontal? &col: &row;
    XP_U16 limit = isHorizontal? MAX_COLS-1:MAX_ROWS-1;
    XP_U16 lastGood = *incr;

    XP_ASSERT( col < MAX_COLS );
    XP_ASSERT( row < MAX_ROWS );

    for ( ; ; ) {
        XP_ASSERT( *incr <= limit );
        if ( *incr == limit ) {
            return limit;
        } else {
            ++*incr;
            if ( modelIsEmptyAt( model, col, row ) ) {
                return lastGood;
            } else {
                lastGood = *incr;
            }
        }
    }
} /* find_end */

static void
wordScoreFormatterInit( WordScoreFormatter* fmtr, DictionaryCtxt* dict )
{
    XP_MEMSET( fmtr, 0, sizeof(*fmtr) );

    fmtr->dict = dict;

    fmtr->firstPass = XP_TRUE;
} /* initWordScoreFormatter */

static void
wordScoreFormatterAddTile( WordScoreFormatter* fmtr, Tile tile, 
                           XP_U16 tileMultiplier, XP_Bool isBlank )
{
    const XP_UCHAR* face;
    XP_UCHAR* fullBufPtr;
    XP_UCHAR* prefix;
    XP_U16 tileScore;

    ++fmtr->nTiles;

    face = dict_getTileString( fmtr->dict, tile );
    XP_ASSERT( XP_STRLEN(fmtr->wordBuf) + XP_STRLEN(face)
               < sizeof(fmtr->wordBuf) );
    XP_STRCAT( fmtr->wordBuf, face );
    if ( isBlank ) {
        tile = dict_getBlankTile( fmtr->dict );
    }

    tileScore = dict_getTileValue( fmtr->dict, tile );

    if ( fmtr->firstPass ) {
        prefix = (XP_UCHAR*)" [";
        fmtr->firstPass = XP_FALSE;
    } else {
        prefix = (XP_UCHAR*)"+";
    }

    fullBufPtr = fmtr->fullBuf + fmtr->bufLen;
    fmtr->bufLen += 
        XP_SNPRINTF( fullBufPtr, 
                     (XP_U16)(sizeof(fmtr->fullBuf) - fmtr->bufLen),
                     (XP_UCHAR*)(tileMultiplier > 1?"%s(%dx%d)":"%s%d"), 
                     prefix, tileScore, tileMultiplier );
    
    XP_ASSERT( XP_STRLEN(fmtr->fullBuf)  == fmtr->bufLen );
    XP_ASSERT( fmtr->bufLen  < sizeof(fmtr->fullBuf) );
} /* wordScoreFormatterAddTile */

static void
wordScoreFormatterFinish( WordScoreFormatter* fmtr, Tile* word, 
                          XWStreamCtxt* stream, XP_UCHAR* mainWord, 
                          XP_U16 mainWordLen )
{
    XP_UCHAR buf[(MAX_ROWS*2)+1];
    XP_U16 len = dict_tilesToString( fmtr->dict, word, fmtr->nTiles, 
                                     buf, sizeof(buf) );

    if ( !!stream ) {
        stream_putBytes( stream, buf, len );

        stream_putBytes( stream, fmtr->fullBuf, fmtr->bufLen );
        stream_putU8( stream, ']' );
    }

    if ( !!mainWord ) {
        XP_STRNCPY( mainWord, fmtr->wordBuf, mainWordLen );
    }

} /* wordScoreFormatterFinish */

static void
formatWordScore( XWStreamCtxt* stream, XP_U16 wordScore, 
                 XP_U16 moveMultiplier )
{
    if ( wordScore > 0 ) {
        XP_U16 multipliedScore = wordScore * moveMultiplier;
        XP_UCHAR tmpBuf[40];
        if ( moveMultiplier > 1 ) {
            XP_SNPRINTF( tmpBuf, sizeof(tmpBuf), 
                         (XP_UCHAR*)" => %d x %d = %d" XP_CR,
                         wordScore, moveMultiplier, multipliedScore );
        } else {
            XP_SNPRINTF( tmpBuf, sizeof(tmpBuf), (XP_UCHAR*)" = %d" XP_CR, 
                         multipliedScore );
        }
        XP_ASSERT( XP_STRLEN(tmpBuf) < sizeof(tmpBuf) );

        stream_catString( stream, tmpBuf );
    }
} /* formatWordScore */

static void
formatSummary( XWStreamCtxt* stream, const ModelCtxt* model, XP_U16 score )
{
    XP_UCHAR buf[60];
    XP_SNPRINTF(buf, sizeof(buf),
                util_getUserString(model->vol.util, STRD_TURN_SCORE), 
                score);
    XP_ASSERT( XP_STRLEN(buf) < sizeof(buf) );
    stream_catString( stream, buf );
} /* formatSummary */

#ifdef CPLUS
}
#endif
