/* 
 * Copyright 1997 - 2015 by Eric House (xwords@eehouse.org).  All rights
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

#include "comtypes.h"
#include "engine.h"
#include "dictnry.h"
#include "util.h"
#include "dbgutil.h"

#ifdef CPLUS
extern "C" {
#endif

typedef XP_U8 Engine_rack[MAX_UNIQUE_TILES+1];

#ifndef NUM_SAVED_ENGINE_MOVES
# define NUM_SAVED_ENGINE_MOVES 10
#endif

typedef struct BlankTuple {
    short col;
    Tile tile;
} BlankTuple;

typedef struct _PossibleMove {
    XP_U16 score; /* Because I'm doing a memcmp to sort these things, the
                     comparison must be done differently on little-endian
                     platforms. */
    XP_U16 nBlanks;
    MoveInfo moveInfo;
    Tile blankVals[MAX_COLS]; /* the faces for which we've substituted
                                 blanks */
} PossibleMove;

/* MoveIterationData is a cache of moves so that next and prev searches don't
 * always trigger an actual search.  Instead we save up to
 * NUM_SAVED_ENGINE_MOVES moves that sort together; then iteration is just
 * returning the next or previous in the cache.  The cache, savedMoves[], is
 * sorted in increasing order, with any unused entries at the low end (since
 * they sort as if score == 0).  nInMoveCache is the actual number of entries.
 * curCacheIndex is the index of the move most recently returned, or outside
 * the range if nothing's been returned yet from the current cache.
 *
 * The cache is empty if nInMoveCache == 0, or if curCacheIndex is in a
 * position that, given engine->usePrev, indicates it's been walked through
 * the cache already rather than being poised to enter it.
 */

typedef struct MoveIterationData {
    /* savedMoves: if any entries are unused (because result set doesn't fill,
       they're at the low end (where sort'll put 'em) */
    PossibleMove savedMoves[NUM_SAVED_ENGINE_MOVES];
    //XP_U16 lowestSavedScore;
    PossibleMove lastSeenMove;
    XP_U16 nInMoveCache; /* num entries, 
                            0 <= nInMoveCache < NUM_SAVED_ENGINE_MOVES */
    XP_U16 bottom;   /* lowest non-0 entry */
    XP_S16 curCacheIndex;       /* what we last returned */
#ifdef DEBUG
    XP_U32 modelHash;
#endif
} MoveIterationData;

/* one bit per tile that's possible here *\/ */
typedef XP_U32 CrossBits;
typedef struct Crosscheck { CrossBits bits[2]; } Crosscheck;

struct EngineCtxt {
    const ModelCtxt* model;
    const DictionaryCtxt* dict;
    XW_UtilCtxt** utilp;
    XP_S16 turn;

    Engine_rack rack;
    Tile blankTile;
    XP_Bool usePrev;
    XP_Bool searchInProgress;
    XP_Bool searchHorizontal;
    XP_Bool isFirstMove;
    XP_U16 numRows, numCols;
    XP_U16 curRow;
    XP_U16 blankCount;
    XP_U16 nMovesToSave;
    XP_U16 star_row;
    XP_Bool returnNOW;
    XP_Bool skipProgressCallback;
    XP_Bool isRobot;
    XP_Bool includePending;
    MoveIterationData miData;

    XP_S16 blankValues[MAX_TRAY_TILES];
    Crosscheck rowChecks[MAX_ROWS]; // also used in xwscore
    XP_U16 scoreCache[MAX_ROWS];

    XP_U16 nTilesMax;
#ifdef XWFEATURE_BONUSALL
    XP_U16 allTilesBonus;
#endif
#ifdef XWFEATURE_SEARCHLIMIT
    XP_U16 nTilesMin;
    XP_U16 nTilesMinUser, nTilesMaxUser;
    XP_Bool tileLimitsKnown;
    const BdHintLimits* searchLimits;
#endif
    XP_U16 lastRowToFill;

#ifdef DEBUG
    XP_U16 curLimit;
    TrayTileSet tts;
#endif
    MPSLOT
}; /* EngineCtxt */

static void findMovesOneRow( EngineCtxt* engine, XWEnv xwe );
static Tile localGetBoardTile( EngineCtxt* engine, XP_U16 col, 
                               XP_U16 row, XP_Bool substBlank );
static void findMovesForAnchor( EngineCtxt* engine, XWEnv xwe,
                                XP_S16* prevAnchor, XP_U16 col, XP_U16 row ) ;
static void figureCrosschecks( EngineCtxt* engine, XP_U16 col, 
                               XP_U16 row, XP_U16* scoreP,
                               Crosscheck* check );
static XP_Bool isAnchorSquare( EngineCtxt* engine, XP_U16 col, XP_U16 row );
static array_edge* edge_from_tile( const DictionaryCtxt* dict, 
                                   array_edge* from, Tile tile );
static void leftPart( EngineCtxt* engine, XWEnv xwe, Tile* tiles, XP_U16 tileLength,
                      array_edge* edge, XP_U16 limit, XP_U16 firstCol,
                      XP_U16 anchorCol, XP_U16 row );
static void extendRight( EngineCtxt* engine, XWEnv xwe, Tile* tiles, XP_U16 tileLength,
                         array_edge* edge, XP_Bool accepting,
                         XP_U16 firstCol, XP_U16 col, XP_U16 row );
static array_edge* consumeFromLeft( EngineCtxt* engine, array_edge* edge, 
                                    short col, short row );
static XP_Bool rack_remove( EngineCtxt* engine, Tile tile, XP_Bool* isBlank );
static void rack_replace( EngineCtxt* engine, Tile tile, XP_Bool isBlank );
static void considerMove( EngineCtxt* engine, XWEnv xwe, Tile* tiles, short tileLength,
                          short firstCol, short lastRow );
static void considerScoreWordHasBlanks( EngineCtxt* engine, XWEnv xwe, XP_U16 blanksLeft,
                                        PossibleMove* posmove,
                                        XP_U16 lastRow,
                                        BlankTuple* usedBlanks,
                                        XP_U16 usedBlanksCount );
static void saveMoveIfQualifies( EngineCtxt* engine, PossibleMove* posmove );
static XP_Bool move_cache_empty( const EngineCtxt* engine );
static void init_move_cache( EngineCtxt* engine );
static PossibleMove* next_from_cache( EngineCtxt* engine );
static void set_search_limits( EngineCtxt* engine );

#ifdef DEBUG
static void assertPMTilesInTiles( const EngineCtxt* engine,
                                  const PossibleMove* pm );
#else
# define assertPMTilesInTiles( engine, pm )
#endif

static XP_S16 cmpMoves( PossibleMove* m1, PossibleMove* m2 );

/* #define CROSSCHECK_CONTAINS(chk,tile) (((chk) & (1L<<(tile))) != 0) */
#define CROSSCHECK_CONTAINS(chk,tile) checkIsSet( (chk), (tile) )

#define HILITE_CELL( engine, xwe, col, row )         \
    util_hiliteCell( *(engine)->utilp, (xwe), (col), (row) )

/* not implemented yet */
XP_U16
engine_getScoreCache( EngineCtxt* engine, XP_U16 row )
{
    return engine->scoreCache[row];
} /* engine_getScoreCache */

/*****************************************************************************
 * This should be the first executable code in the file in case I want to
 * turn it into a separate code module later.
 ****************************************************************************/ 
EngineCtxt*
engine_make( XWEnv XP_UNUSED_DBG(xwe), XW_UtilCtxt** utilp )
{
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = util_getMemPool( *utilp, xwe );
#endif
    EngineCtxt* result = (EngineCtxt*)XP_MALLOC( mpool, sizeof(*result) );
    XP_MEMSET( result, 0, sizeof(*result) );

    MPASSIGN(result->mpool, mpool);

    result->utilp = utilp;

    engine_reset( result );

    return result;
} /* engine_make */

void
engine_writeToStream( EngineCtxt* XP_UNUSED(ctxt), 
                      XWStreamCtxt* XP_UNUSED(stream) )
{
    /* nothing to save; see comment below */
} /* engine_writeToStream */

EngineCtxt* 
engine_makeFromStream( XWEnv xwe, XW_UtilCtxt** utilp,
                       XWStreamCtxt* XP_UNUSED(stream) )
{
    EngineCtxt* engine = engine_make( xwe, utilp );

    /* All the engine's data seems to be used only in the process of finding a
       move.  So if we're willing to have the set of moves found lost across
       a save, there's nothing to do! */

    return engine;
} /* engine_makeFromStream */

void
engine_reset( EngineCtxt* engine )
{
    XP_MEMSET( &engine->miData, 0, sizeof(engine->miData) );
    /* set last score to max possible */
    engine->miData.lastSeenMove.score = engine->usePrev? 0 : 0xffff;
    engine->searchInProgress = XP_FALSE;
#ifdef XWFEATURE_SEARCHLIMIT
    engine->tileLimitsKnown = XP_FALSE;      /* indicates not set */
    if ( engine->nTilesMin == 0 ) {
        engine->nTilesMinUser = engine->nTilesMin = 1;
        engine->nTilesMaxUser = engine->nTilesMax = MAX_TRAY_TILES;
    }
#endif
} /* engine_reset */

void
engine_destroy( EngineCtxt* engine )
{
    XP_ASSERT( engine != NULL );
    XP_FREE( engine->mpool, engine );
} /* engine_destroy */

static XP_Bool
initTray( EngineCtxt* engine, const TrayTileSet* tts )
{
#ifdef DEBUG
    engine->tts = *tts;
#endif
    XP_Bool result = tts->nTiles > 0;

    if ( result ) {
        XP_MEMSET( engine->rack, 0, sizeof(engine->rack) );
        for ( int ii = 0; ii < tts->nTiles; ++ii ) {
            Tile tile = tts->tiles[ii];
            XP_ASSERT( tile < MAX_UNIQUE_TILES );
            ++engine->rack[tile];
        }
    }

    return result;
} /* initTray */

static XP_S16
cmpMoves( PossibleMove* m1, PossibleMove* m2 )
{
    XP_S16 result;
    if ( m1->score != m2->score ) {
        result = m1->score > m2->score ? 1 : -1;
    } else if ( m1->nBlanks != m2->nBlanks ) {
        result = m1->nBlanks > m2->nBlanks ? -1 : 1;
    } else {
        result = XP_MEMCMP( &m1->moveInfo, &m2->moveInfo,
                            sizeof(m1->moveInfo) );
        if ( 0 == result ) {
            result = XP_MEMCMP( &m1->blankVals, &m2->blankVals,
                                sizeof(m1->blankVals) );
        }
    }
    return result;
} /* cmpMoves */

#if 0
static void
print_savedMoves( const EngineCtxt* engine, const char* label )
{
    int ii;
    int pos = 0;
    char buf[(NUM_SAVED_ENGINE_MOVES*10) + 3] = {};
    for ( ii = 0; ii < engine->nMovesToSave; ++ii ) {
        if ( 0 < engine->miData.savedMoves[ii].score ) {
            pos += XP_SNPRINTF( &buf[pos], VSIZE(buf)-pos, "[%d]: %d; ", 
                                ii, engine->miData.savedMoves[ii].score );
        }
    }
    XP_LOGF( "%s: %s", label, buf );
}
#else
# define print_savedMoves( engine, label )
#endif

static XP_Bool
chooseMove( EngineCtxt* engine, PossibleMove** move ) 
{
    XP_U16 ii;
    PossibleMove* chosen = NULL;
    XP_Bool result;
    XP_Bool done;

    print_savedMoves( engine, "unsorted moves" );

    /* First, sort 'em.  Put the higher-scoring moves at the top where they'll
       get picked up first.  Don't sort if we're working for a robot; we've
       only been saving the single best move anyway.  At least not until we
       start applying other criteria than score to moves. */

    done = !move_cache_empty( engine );
    while ( !done ) { /* while so can break */
        done = XP_TRUE;
        PossibleMove* cur = engine->miData.savedMoves;
        for ( ii = 0; ii < engine->nMovesToSave-1; ++ii ) {
            PossibleMove* next = cur + 1;
            if ( cmpMoves( cur, next ) > 0 ) {
                PossibleMove tmp;
                XP_MEMCPY( &tmp, cur, sizeof(tmp) );
                XP_MEMCPY( cur, next, sizeof(*cur) );
                XP_MEMCPY( next, &tmp, sizeof(*next) );
                done = XP_FALSE;
            }
            cur = next;
        }

        if ( done ) {
            if ( !engine->isRobot ) {
                init_move_cache( engine );
            }
            print_savedMoves( engine, "sorted moves" );
        }
    }

    /* now pick the one we're supposed to return */
    if ( engine->isRobot ) {
        XP_ASSERT( engine->miData.nInMoveCache <= NUM_SAVED_ENGINE_MOVES );
        XP_ASSERT( engine->miData.nInMoveCache <= engine->nMovesToSave );
        /* PENDING not nInMoveCache-1 below?? */
        chosen = &engine->miData.savedMoves[engine->miData.nInMoveCache];
    } else {
        chosen = next_from_cache( engine );
    }

    *move = chosen; /* set either way */

    result = (NULL != chosen) && (chosen->score > 0);

    return result;
} /* chooseMove */

/* Robot smartness is a number between 0 and 100, inclusive.  0 means a human
 * player who may want to iterate, so save all moves.  If a robot player, we
 * want a random move within a range proportional to the 1-100 range, so we
 * figure out now what we'll be picking, save only that many moves and take
 * the worst of 'em in chooseMove().
 */
static void
normalizeIQ( EngineCtxt* engine, XP_U16 iq )
{
    engine->isRobot = 0 < iq;
    if ( 0 == iq ) {            /* human */
        engine->nMovesToSave = NUM_SAVED_ENGINE_MOVES; /* save 'em all */
    } else if ( 1 == iq ) {            /* smartest robot */
        engine->nMovesToSave = 1;
    } else {
        XP_U16 count = NUM_SAVED_ENGINE_MOVES * iq / 100;
        engine->nMovesToSave = 1;
        if ( count > 0 ) {
            engine->nMovesToSave += XP_RANDOM() % count;
        }
    }
}

/* Return of XP_TRUE means that we ran to completion.  XP_FALSE means we were
 * interrupted.  Whether an actual move was found is indicated by what's
 * filled in in *newMove.
 */
XP_Bool
engine_findMove( EngineCtxt* engine, XWEnv xwe, const ModelCtxt* model,
                 XP_S16 turn, XP_Bool includePending, XP_Bool skipCallback,
                 const TrayTileSet* tts, XP_Bool usePrev,
#ifdef XWFEATURE_BONUSALL
                 XP_U16 allTilesBonus,
#endif
#ifdef XWFEATURE_SEARCHLIMIT
                 const BdHintLimits* searchLimits,
                 XP_Bool useTileLimits,
#endif
                 XP_U16 robotIQ, XP_Bool* canMoveP, MoveInfo* newMove,
                 XP_U16* score )
{
    XP_Bool result = XP_TRUE;
    XP_U16 star_row;
    XP_Bool canMove = XP_FALSE;
    XP_Bool isRetry = XP_FALSE;

#ifdef DEBUG
    XP_U32 hash = model_getHash( model );
    XP_ASSERT( engine->miData.modelHash == 0 || engine->miData.modelHash == hash );
    engine->miData.modelHash = hash;
#endif

 retry:
    engine->nTilesMax = XP_MIN( MAX_TRAY_TILES, tts->nTiles );
#ifdef XWFEATURE_BONUSALL
    engine->allTilesBonus = allTilesBonus;
#endif
#ifdef XWFEATURE_SEARCHLIMIT
    if ( useTileLimits ) {
        /* We'll want to use the numbers we've been using already unless
           there's been a reset.  In that case, though, provide the old
           ones as defaults */
        if ( !engine->tileLimitsKnown ) {

            XP_U16 nTilesMin = engine->nTilesMinUser;
            XP_U16 nTilesMax = engine->nTilesMaxUser;

            if ( util_getTraySearchLimits( *engine->utilp, xwe,
                                           &nTilesMin, &nTilesMax ) ) {
                engine->tileLimitsKnown = XP_TRUE;
                engine->nTilesMinUser = nTilesMin;
                engine->nTilesMaxUser = nTilesMax;
            } else {
                *canMoveP = XP_FALSE;                
                result = XP_TRUE;
                goto exit;
            }
        }

        engine->nTilesMin = engine->nTilesMinUser;
        engine->nTilesMax = engine->nTilesMaxUser;
    } else {
        engine->nTilesMin = 1;
    }
#endif

    engine->model = model;
    engine->dict = model_getPlayerDict( model, turn );
    engine->turn = turn;
    engine->includePending = includePending;
    engine->usePrev = usePrev;
    engine->blankTile = dict_getBlankTile( engine->dict );
    engine->returnNOW = XP_FALSE;
    engine->skipProgressCallback = skipCallback;
#ifdef XWFEATURE_SEARCHLIMIT
    engine->searchLimits = searchLimits;
#endif

    engine->star_row = star_row = model_numRows(model) / 2;
    engine->isFirstMove = 
        EMPTY_TILE == localGetBoardTile( engine, star_row, 
                                         star_row, XP_FALSE );

    /* If we've been asked to generate a move but can't because the
       dictionary's emtpy or there are no tiles, still return TRUE so we don't
       get scheduled again.  Fixes infinite loop with empty dict and a
       robot. */
    canMove = NULL != dict_getTopEdge(engine->dict)
        && initTray( engine, tts );
    if ( canMove  ) {
        util_engineStarting( engine->util, xwe,
                             engine->rack[engine->blankTile] );

        normalizeIQ( engine, robotIQ );

        if ( move_cache_empty( engine ) ) {
            set_search_limits( engine );

            XP_MEMSET( engine->miData.savedMoves, 0,
                       sizeof(engine->miData.savedMoves) );

            if ( engine->searchInProgress ) {
                goto resumePoint;
            } else {
                engine->searchHorizontal = XP_TRUE;
                engine->searchInProgress = XP_TRUE;
            }
            for ( ; ; ) {
                XP_U16 firstRowToFill = 0;
                engine->numRows = model_numRows(engine->model);
                engine->numCols = model_numCols(engine->model);
                if ( !engine->searchHorizontal ) {
                    XP_U16 tmp = engine->numRows;
                    engine->numRows = engine->numCols;
                    engine->numCols = tmp;
                }

                if ( 0 ) {
#ifdef XWFEATURE_SEARCHLIMIT
                } else if ( !!searchLimits ) {
                    if ( engine->searchHorizontal ) {
                        firstRowToFill = searchLimits->top;
                        engine->lastRowToFill = searchLimits->bottom;
                    } else {
                        firstRowToFill = searchLimits->left;
                        engine->lastRowToFill = searchLimits->right;
                    }
#endif
                } else {
                    engine->lastRowToFill = engine->numRows - 1;
                }

                for ( engine->curRow = firstRowToFill;
                      engine->curRow <= engine->lastRowToFill;
                      ++engine->curRow ) {
                resumePoint:
                    if ( engine->isFirstMove && (engine->curRow != star_row)) {
                        continue;
                    }
                    findMovesOneRow( engine, xwe );
                    if ( engine->returnNOW ) {
                        goto outer;
                    }
                }

                if ( !engine->searchHorizontal 
#ifdef XWFEATURE_SEARCHLIMIT
                     || (engine->isFirstMove && !searchLimits) 
#endif
                     ) {
                    engine->searchInProgress = XP_FALSE;
                    break;
                } else {
                    engine->searchHorizontal = XP_FALSE;
                }
            } /* forever */
        }
    outer:
        /* Search is finished.  Choose (or just return) the best move found. */
        if ( engine->returnNOW ) {
            result = XP_FALSE;
        } else {
            PossibleMove* move;
            if ( chooseMove( engine, &move ) ) {
                XP_ASSERT( !!newMove );
                XP_MEMCPY( newMove, &move->moveInfo, sizeof(*newMove) );
                if ( !!score ) {
                    *score = move->score;
                }
            } else {
                newMove->nTiles = 0;
                canMove = XP_FALSE;
            }
            XP_ASSERT( result );
        }

        util_engineStopping( engine->util, xwe );
    } else {
        /* set up a PASS.  I suspect the caller should be deciding how to
           handle this case itself, but this doesn't preclude its doing
           so.  */
        newMove->nTiles = 0;
    }

    /* Gross hack alert: there's an elusive bug in move cacheing that means
       when we move forward or back from the highest-scoring move to the
       lowest (or vice-versa) no move is found. But the next try succeeds,
       because an engine_reset clears the state that makes that happen. So as
       a workaround, try doing that when no moves are found. If none is found
       for some other reason, e.g. no tiles, at least the search should be
       quick. */
    if ( !canMove ) {
        engine_reset( engine ); 
        if ( !isRetry ) {
            isRetry = XP_TRUE;
            XP_LOGFF( "no moves found so retrying" );
            goto retry;
        }
    }

    *canMoveP = canMove;
#ifdef XWFEATURE_SEARCHLIMIT
 exit:
#endif
    LOG_RETURNF( "%s", boolToStr(result) );
    return result;
} /* engine_findMove */

static void
findMovesOneRow( EngineCtxt* engine, XWEnv xwe )
{
    XP_U16 lastCol = engine->numCols - 1;
    XP_U16 row = engine->curRow;
    XP_U16 firstSearchCol, lastSearchCol;
#ifdef XWFEATURE_SEARCHLIMIT
    const BdHintLimits* searchLimits = engine->searchLimits;
#endif

    if ( 0 ) {
#ifdef XWFEATURE_SEARCHLIMIT
    } else if ( !!searchLimits ) {
        if ( engine->searchHorizontal ) {
            firstSearchCol = searchLimits->left;
            lastSearchCol = searchLimits->right;
        } else {
            firstSearchCol = searchLimits->top;
            lastSearchCol = searchLimits->bottom;
        }
#endif        
    } else {
        firstSearchCol = 0;
        lastSearchCol = lastCol;
    }

    XP_MEMSET( &engine->rowChecks, 0, sizeof(engine->rowChecks) ); /* clear */
    for ( XP_U16 col = 0; col <= lastCol; ++col ) {
        if ( col < firstSearchCol || col > lastSearchCol ) {
            engine->scoreCache[col] = 0;
        } else {
            figureCrosschecks( engine, col, row, 
                               &engine->scoreCache[col],
                               &engine->rowChecks[col]);
        }
    }

    XP_S16 prevAnchor = firstSearchCol - 1;
    for ( XP_U16 col = firstSearchCol; col <= lastSearchCol && !engine->returnNOW;
          ++col ) {
        if ( isAnchorSquare( engine, col, row ) ) { 
            findMovesForAnchor( engine, xwe, &prevAnchor, col, row );
        }
    }
} /* findMovesOneRow */

static XP_Bool
lookup( const DictionaryCtxt* dict, array_edge* edge, Tile* buf, 
        XP_U16 tileIndex, XP_U16 length ) 
{
    XP_Bool result = XP_FALSE;
    while ( edge != NULL ) {
        Tile targetTile = buf[tileIndex];
        edge = dict_edge_with_tile( dict, edge, targetTile );
        if ( edge == NULL ) { /* tile not available out of this node */
            break;
        } else {
            if ( ++tileIndex == length ) { /* is this the last tile? */
                result = ISACCEPTING(dict, edge);
                break;
            } else {
                edge = dict_follow( dict, edge );
                continue;
            }
        }
    }
    return result;
} /* lookup */

static void
figureCrosschecks( EngineCtxt* engine, XP_U16 x, XP_U16 y, XP_U16* scoreP,
                   Crosscheck* check )
{
    XP_S16 startY, maybeY;
    XP_U16 numRows = engine->numRows;
    Tile tile;
    array_edge* in_edge;
    array_edge* candidateEdge;
    Tile tiles[MAX_COLS];
    XP_U16 tilesAfter;
    XP_U16 checkScore = 0;
    const DictionaryCtxt* dict = engine->dict;

    if ( localGetBoardTile( engine, x, y, XP_FALSE ) == EMPTY_TILE ) {

        /* find the first tile of any prefix */
        startY = (XP_S16)y;
        for ( ; ; ) {
            maybeY = startY - 1;
            if ( maybeY < 0 ) {
                break;
            }
            if ( localGetBoardTile( engine, x, maybeY, XP_FALSE )
                 == EMPTY_TILE ) {
                break;
            }
            startY = maybeY;
        }

        /* Take care of the "special case" where the square has no neighbors
           in either crosscheck direction */
        if ( (y == startY) &&
             ((y == numRows-1) ||
              (localGetBoardTile( engine, x, y+1, XP_FALSE ) == EMPTY_TILE))){
            /* all tiles legal and checkScore remains 0, as there are no
               neighbors */
            XP_MEMSET( check, 0xFF, sizeof(*check) );
            goto outer;
        }

        /* now walk the DAWG consuming any prefix.  We want in_edge to wind up
           holding the edge that leads to {x,y}, which will be the root edge
           if there's no prefix.  I can't use consumeFromLeft() here because
           here I'm consuming upward.  But I could generalize it.... */
        in_edge = dict_getTopEdge( dict );
        while ( startY < y ) {
            tile = localGetBoardTile( engine, x, startY, XP_TRUE );
            XP_ASSERT( tile != EMPTY_TILE );
            checkScore += dict_getTileValue( dict, tile );
            tile = localGetBoardTile( engine, x, startY, XP_FALSE );
            in_edge = edge_from_tile( dict, in_edge, tile );
            /* If we run into a null edge here we have a prefix that by the
               dictionary is an illegal word.  One way it could have gotten
               there is by being placed by a human.  So it's not something to
               flag here, but we won't be able to put anything in the spot so
               the crosscheck is empty.  And the ASSERT goes.

               Note that if we were disallowing words not in the dictionary
               (as in a robot-only game) then the assertion would be valid:
               only if there's a single letter as the "prefix" of our
               crosscheck would it make sense for there to be no edge leading
               out of it.  But when can that happen?  I.e. what letters don't
               begin words in any reasonable word list? */
            if ( in_edge == NULL ) {
                /* Only way to have gotten here is if a user's played a word
                   not in this dict.  We'll not be able to build on it! */
                XP_ASSERT( check->bits[0] == 0L && check->bits[1] == 0L );
                goto outer;
            }
            ++startY;
        }

        /* now in_edge points to the array of candidate edges.  We'll build up
           a buffer of the Tiles following the candidate square on the board,
           then put each candidate edge's Tile in place and do a lookup
           beginning at in_edge.  Successful candidate tiles get added to the
           Crosscheck */
        for ( tilesAfter = 1, maybeY = y + 1; maybeY < numRows; ++maybeY ) {
            tile = localGetBoardTile( engine, x, maybeY, XP_TRUE );
            if ( tile == EMPTY_TILE ) {
                break;
            } else {
                checkScore += dict_getTileValue( dict, tile );
                tiles[tilesAfter++] = localGetBoardTile( engine, x, maybeY,
                                                         XP_FALSE );
            }
        }

        /* <eeh> would it be possible to use extendRight here?  With an empty
           tray?  No: it calls considerMove etc. */
        candidateEdge = in_edge;
        for ( ; ; ) {
            tile = EDGETILE( dict, candidateEdge ); 
            XP_ASSERT( tile < MAX_UNIQUE_TILES );
            tiles[0] = tile;
            if ( lookup( dict, in_edge, tiles, 0, tilesAfter ) ) {
                XP_ASSERT( (tile >> 5)
                           < (VSIZE(check->bits)) );
                check->bits[tile>>5] |= (1L << (tile & 0x1F));
            }

            if ( IS_LAST_EDGE(dict,candidateEdge ) ) {
                break;
            }
            candidateEdge += dict->nodeSize;
        }
    }
 outer:
    if ( scoreP != NULL ) { 
        *scoreP = checkScore;
    }
} /* figureCrosschecks */

XP_Bool
engine_check( const DictionaryCtxt* dict, Tile* tiles, XP_U16 nTiles )
{
    array_edge* in_edge = dict_getTopEdge( dict );

    return lookup( dict, in_edge, tiles, 0, nTiles );
} /* engine_check */

static Tile
localGetBoardTile( EngineCtxt* engine, XP_U16 col, XP_U16 row, 
                   XP_Bool substBlank )
{
    Tile result;
    XP_Bool isBlank;

    if ( !engine->searchHorizontal ) {
        XP_U16 tmp = col;
        col = row;
        row = tmp;
    }

    if ( model_getTile( engine->model, col, row, engine->includePending,
                        engine->turn, &result, &isBlank, NULL, NULL ) ) {
        if ( isBlank && substBlank ) {
            result = engine->blankTile;
        }
    } else {
        result = EMPTY_TILE;
    }
    return result;
} /* localGetBoardTile */

/*****************************************************************************
 * Return true if the tile is empty and has a filled-in square on any of the
 * four sides.  First move is a special case: empty and 7,7
 ****************************************************************************/
static XP_Bool
isAnchorSquare( EngineCtxt* engine, XP_U16 col, XP_U16 row ) 
{
    if ( localGetBoardTile( engine, col, row, XP_FALSE ) != EMPTY_TILE ) {
        return XP_FALSE;
    }

    if ( engine->isFirstMove ) {
        return col == engine->star_row && row == engine->star_row;
    }

    if ( (col != 0) && 
         localGetBoardTile( engine, col-1, row, XP_FALSE ) != EMPTY_TILE ) {
        return XP_TRUE;
    }
    if ( (col < engine->numCols-1) 
         && localGetBoardTile( engine, col+1, row, XP_FALSE ) != EMPTY_TILE) {
        return XP_TRUE;
    }
    if ( (row != 0)
         && localGetBoardTile( engine, col, row-1, XP_FALSE) != EMPTY_TILE ) {
        return XP_TRUE;
    }
    if ( (row < engine->numRows-1)
         && localGetBoardTile( engine, col, row+1, XP_FALSE ) != EMPTY_TILE ){
        return XP_TRUE;
    }
    return XP_FALSE;
} /* isAnchorSquare */

#ifdef XWFEATURE_HILITECELL
static void
hiliteForAnchor( EngineCtxt* engine, XWEnv xwe, XP_U16 col, XP_U16 row )
{
    if ( !engine->searchHorizontal ) {
        XP_U16 tmp = col;
        col = row;
        row = tmp;
    }

    if ( !HILITE_CELL( engine, xwe, col, row ) ) {
        engine->returnNOW = XP_TRUE;
    }
} /* hiliteForAnchor */
#else
# define hiliteForAnchor( engine, xwe, col, row )
#endif

static void
findMovesForAnchor( EngineCtxt* engine, XWEnv xwe, XP_S16* prevAnchor,
                    XP_U16 col, XP_U16 row ) 
{
    XP_S16 limit;
    array_edge* edge;
    array_edge* topEdge;
    Tile tiles[MAX_ROWS];

    hiliteForAnchor( engine, xwe, col, row );

    if ( engine->returnNOW ) {
        /* time to bail */
    } else {
        limit = col - *prevAnchor - 1;
#ifdef TEST_MINLIMIT
        if ( limit >= MAX_TRAY_TILES ) {
            limit = MAX_TRAY_TILES - 1;
        }
#endif
        topEdge = dict_getTopEdge( engine->dict );
        if ( col == 0 ) {
            edge = topEdge;
        } else if ( localGetBoardTile( engine, col-1, row, XP_FALSE ) 
                    == EMPTY_TILE ) {
            leftPart( engine, xwe, tiles, 0, topEdge, limit, col, col, row );
            goto done;
        } else {
            edge = consumeFromLeft( engine, topEdge, col, row );
        }
        DEBUG_ASSIGN(engine->curLimit, 0);
        extendRight( engine, xwe, tiles, 0, edge,
                     XP_FALSE, // can't accept without the anchor square
                     col-limit, col, row );

    done:
        *prevAnchor = col;
    }
} /* findMovesForAnchor */

static array_edge*
consumeFromLeft( EngineCtxt* engine, array_edge* edge, short col, short row )
{
    XP_S16 maybeX;
    Tile tile;
    Tile tiles[MAX_ROWS];
    XP_U16 numTiles;

    /* Back up to the left until an empty tile or board edge is reached, saving
       the tiles for cheaper retrieval as we walk forward through the DAWG. */
    for ( numTiles = 0, maybeX = col - 1; maybeX >= 0; --maybeX ) {
        tile = localGetBoardTile( engine, maybeX, row, XP_FALSE );
        if ( tile == EMPTY_TILE ) {
            break;
        }
        tiles[numTiles++] = tile; /* we're building the word backwards */
    }
    XP_ASSERT( numTiles > 0 ); /* we should consume *something* */

    /* <eeh> could I just call lookup() here?  Only if I fixed it to
       communicate back the edge it's at after finishing. */
    while ( numTiles-- ) {
        XP_ASSERT( tiles[numTiles] != EMPTY_TILE );

        edge = edge_from_tile( engine->dict, edge, tiles[numTiles] );
        if ( edge == NULL ) {
            break;
        }
    }
    return edge;
} /* consumeFromLeft */

static void
leftPart( EngineCtxt* engine, XWEnv xwe, Tile* tiles, XP_U16 tileLength,
          array_edge* edge, XP_U16 limit, XP_U16 firstCol,
          XP_U16 anchorCol, XP_U16 row )
{
    DEBUG_ASSIGN( engine->curLimit, tileLength );

    extendRight( engine, xwe, tiles, tileLength, edge, XP_FALSE, firstCol,
                 anchorCol, row );
    if ( !engine->returnNOW ) {
        if ( (limit > 0) && (edge != NULL) ) {
            XP_U16 nodeSize = engine->dict->nodeSize;
            if ( engine->nTilesMax > 0 ) {
                for ( ; ; ) {
                    XP_Bool isBlank;
                    Tile tile = EDGETILE( engine->dict, edge );
                    if ( rack_remove( engine, tile, &isBlank ) ) {
                        tiles[tileLength] = tile;
                        leftPart( engine, xwe, tiles, tileLength+1,
                                  dict_follow( engine->dict, edge ), 
                                  limit-1, firstCol-1, anchorCol, row );
                        rack_replace( engine, tile, isBlank );
                    }

                    if ( IS_LAST_EDGE( dict, edge ) || engine->returnNOW ) {
                        break;
                    }
                    edge += nodeSize;
                }
            }
        }
    }
} /* leftPart */

static void
extendRight( EngineCtxt* engine, XWEnv xwe, Tile* tiles, XP_U16 tileLength,
             array_edge* edge, XP_Bool accepting,
             XP_U16 firstCol, XP_U16 col, XP_U16 row )
{
    Tile tile;
    const DictionaryCtxt* dict = engine->dict;

    if ( col == engine->numCols ) { /* we're off the board */
        goto check_exit;
    }
    tile = localGetBoardTile( engine, col, row, XP_FALSE );

    if ( edge == NULL ) { // we're off the dictionary
        if ( tile != EMPTY_TILE ) {
            goto no_check; // don't check at the end
        }
    } else if ( tile == EMPTY_TILE ) {
        if ( engine->nTilesMax > 0 ) {
            CrossBits check = engine->rowChecks[col].bits[0];
            XP_Bool advanced = XP_FALSE;
            for ( ; ; ) {
                XP_Bool contains;
                tile = EDGETILE( dict, edge );

                /* If it's bigger than 32, use the second crosscheck.  This is
                   a hack to optimize for the vastly more common case.  Even
                   with languages that have more than 32 tiles at least half
                   will be less than 32 in value. */
                if ( (tile & ~0x1F) != 0 ) {
                    if ( !advanced ) {
                        check = engine->rowChecks[col].bits[1];
                        advanced = XP_TRUE;
                    }
                    contains = (check & (1L << (tile-32))) != 0;
                } else {
                    contains = (check & (1L << tile)) != 0;
                }

                if ( contains ) {
                    XP_Bool isBlank;
                    if ( rack_remove( engine, tile, &isBlank ) ) {
                        tiles[tileLength] = tile;
                        extendRight( engine, xwe, tiles, tileLength+1,
                                     edge_from_tile( dict, edge, tile ), 
                                     ISACCEPTING( dict, edge ), firstCol, 
                                     col+1, row );
                        rack_replace( engine, tile, isBlank );
                        if ( engine->returnNOW ) {
                            goto no_check;
                        }
                    }
                }

                if ( IS_LAST_EDGE( dict, edge ) ) {
                    break;
                }
                edge += dict->nodeSize;
            }
        }

    } else if ( (edge = dict_edge_with_tile( dict, edge, tile ) ) != NULL ) {
        accepting = ISACCEPTING( dict, edge );
        extendRight( engine, xwe, tiles, tileLength, dict_follow(dict, edge),
                     accepting, firstCol, col+1, row );
        goto no_check; /* don't do the check at the end */
    } else {
        goto no_check;
    }
 check_exit:
    if ( accepting
#ifdef XWFEATURE_SEARCHLIMIT
         && tileLength >= engine->nTilesMin
#endif
         ) {
        considerMove( engine, xwe, tiles, tileLength, firstCol, row );
    }
 no_check:
    return;
} /* extendRight */

static XP_Bool
rack_remove( EngineCtxt* engine, Tile tile, XP_Bool* isBlank )
{
    Tile blankIndex = engine->blankTile;

    XP_ASSERT( tile < MAX_UNIQUE_TILES );
    XP_ASSERT( tile != blankIndex );
    XP_ASSERT( engine->nTilesMax > 0 );

    XP_Bool found = XP_TRUE;
    if ( engine->rack[(short)tile] > 0 ) { /* we have the tile itself */
        --engine->rack[(short)tile];
        *isBlank = XP_FALSE;
    } else if ( engine->rack[blankIndex] > 0 ) { /* we have and must use a
                                                    blank */
        --engine->rack[(short)blankIndex];
        engine->blankValues[engine->blankCount++] = tile;
        *isBlank = XP_TRUE;
    } else { /* we can't satisfy the request */
        found = XP_FALSE;        /* FIXME */
    }

    if ( found ) {
        --engine->nTilesMax;
    }
    return found;
} /* rack_remove */

static void
rack_replace( EngineCtxt* engine, Tile tile, XP_Bool isBlank ) 
{
    if ( isBlank ) {
        --engine->blankCount;
        tile = engine->blankTile;
    }
    ++engine->rack[(short)tile];

    ++engine->nTilesMax;
} /* rack_replace */

static void
considerMove( EngineCtxt* engine, XWEnv xwe, Tile* tiles, XP_S16 tileLength,
              XP_S16 firstCol, XP_S16 lastRow )
{
    if ( !engine->skipProgressCallback
         && !util_engineProgressCallback( *engine->utilp, xwe ) ) {
        engine->returnNOW = XP_TRUE;
    } else {

        /* if this never gets hit then the top-level caller of leftPart should
           never pass a value greater than 7 for limit.  I think we're always
           guaranteed to run out of tiles before finding a legal move with
           larger values but that it's expensive to look only to fail. */
        XP_ASSERT( engine->curLimit < MAX_TRAY_TILES );

        PossibleMove posmove = {};
        MoveInfo* mip = &posmove.moveInfo;
        mip->isHorizontal = engine->searchHorizontal;
        mip->commonCoord = (XP_U8)lastRow;
        for ( XP_U16 col = firstCol; mip->nTiles < tileLength; ++col ) {
            /* is it one of the new ones? */
            if ( localGetBoardTile( engine, col, lastRow, XP_FALSE )
                 == EMPTY_TILE ) { 
                mip->tiles[mip->nTiles].tile = tiles[mip->nTiles];
                mip->tiles[mip->nTiles].varCoord = (XP_U8)col;
                ++mip->nTiles;
            }
        }

        BlankTuple blankTuples[MAX_NUM_BLANKS];
        considerScoreWordHasBlanks( engine, xwe, engine->blankCount, &posmove,
                                    lastRow, blankTuples, 0 );
    }
} /* considerMove */

static void
countWords( const WNParams* wnp, void* closure )
{
    XP_U16* wcp = (XP_U16*)closure;
    if ( wnp->isLegal ) {
        ++*wcp;
    }
}

static void
considerScoreWordHasBlanks( EngineCtxt* engine, XWEnv xwe, XP_U16 blanksLeft,
                            PossibleMove* posmove,
                            XP_U16 lastRow, BlankTuple* usedBlanks,
                            const XP_U16 usedBlanksCount )
{
    XP_U16 ii;

    if ( blanksLeft == 0 ) {
        /* Hack: When a single-tile move involves two words it'll be found by
           both the horizontal and vertical passes. Since it's really the same
           move both times we don't want both. It'd be better I think to
           change the move comparison code to detect it as a duplicate, but
           that's a lot of work. Instead, add a callback in the single-tile
           vertical case to count words, and when the count it > 1 drop the
           move.*/
        WordNotifierInfo* wiip = NULL;
        WordNotifierInfo wii;
        XP_U16 singleTileWordCount = 0;
        if ( !engine->searchHorizontal && 1 == posmove->moveInfo.nTiles ) {
            wii.proc = countWords;
            wii.closure = &singleTileWordCount;
            wiip = &wii;
        }

        XP_U16 score = figureMoveScore( engine->model, xwe, engine->turn,
                                        &posmove->moveInfo,
                                        engine, (XWStreamCtxt*)NULL, wiip );

        if ( singleTileWordCount > 1  ) { /* only set by special-case code above */
            XP_ASSERT( singleTileWordCount == 2 ); /* I think this is the limit */
            // XP_LOGF( "%s(): dropping", __func__ );
        } else {
#ifdef XWFEATURE_BONUSALL
            if ( 0 != engine->allTilesBonus && 0 == engine->nTilesMax ) {
                XP_LOGFF( "adding bonus: %d becoming %d", score,
                         score + engine->allTilesBonus );
                score += engine->allTilesBonus;
            }
#endif
            posmove->score = score;
            posmove->nBlanks = usedBlanksCount;
            XP_MEMSET( &posmove->blankVals, 0, sizeof(posmove->blankVals) );
            for ( ii = 0; ii < usedBlanksCount; ++ii ) {
                short col = usedBlanks[ii].col;
                posmove->blankVals[col] = usedBlanks[ii].tile;
            }
            XP_ASSERT( posmove->moveInfo.isHorizontal ==
                       engine->searchHorizontal );
            posmove->moveInfo.commonCoord = (XP_U8)lastRow;
            saveMoveIfQualifies( engine, posmove );
        }
    } else {
        Tile bTile;
        BlankTuple* bt;

        --blanksLeft;
        XP_ASSERT( engine->blankValues[blanksLeft] < 128 );
        bTile = (Tile)engine->blankValues[blanksLeft];
        bt = &usedBlanks[usedBlanksCount];

        /* for each letter for which the blank might be standing in... */
        for ( ii = 0; ii < posmove->moveInfo.nTiles; ++ii ) {
            CellTile tile = posmove->moveInfo.tiles[ii].tile;
            if ( (tile & TILE_VALUE_MASK) == bTile && !IS_BLANK(tile) ) {
                posmove->moveInfo.tiles[ii].tile |= TILE_BLANK_BIT;
                bt->col = ii;
                bt->tile = bTile;
                considerScoreWordHasBlanks( engine, xwe, blanksLeft,
                                            posmove, lastRow,
                                            usedBlanks,
                                            usedBlanksCount + 1 );
                /* now put things back */
                posmove->moveInfo.tiles[ii].tile &= ~TILE_BLANK_BIT;
            }
        }
    }
} /* considerScoreWordHasBlanks */

static void
saveMoveIfQualifies( EngineCtxt* engine, PossibleMove* posmove )
{
    XP_S16 mostest;
    XP_S16 cmpVal;
    XP_Bool usePrev = engine->usePrev;
    XP_Bool foundEmpty = XP_FALSE;
    MoveIterationData* miData = &engine->miData;

    assertPMTilesInTiles( engine, posmove );

    if ( 1 == engine->nMovesToSave ) { /* only saving one */
        mostest = 0;
    } else {
        mostest = -1;
        /* we're not interested if we've seen this */
        cmpVal = cmpMoves( posmove, &miData->lastSeenMove );
        if ( !usePrev && cmpVal >= 0 ) {
            /* XP_LOGF( "%s: dropping %d: >= %d", __func__, */
            /*          posmove->score, miData->lastSeenMove.score ); */
        } else if ( usePrev && cmpVal <= 0 ) {
            /* XP_LOGF( "%s: dropping %d: <= %d", __func__, */
            /*          posmove->score, miData->lastSeenMove.score ); */
        } else {
            XP_S16 ii;
            /* terminate i at 1 because mostest starts at 0 */
            for ( ii = 0; ii < engine->nMovesToSave; ++ii ) {
                /* Find the mostest value move and overwrite it.  Note that
                   there might not be one, as all may have the same or higher
                   scores and those that have the same score may compare
                   higher.

                   <eeh> can't have this asssertion until I start noting the
                   mostest saved score (setting miData.mostestSavedScore)
                   below. */
                /* 1/20/2001  I don't see that this assertion is valid.  I
                   simply don't understand why it isn't tripped all the time
                   in the old crosswords. */
                /* XP_ASSERT( (miData->lastSeenMove.score == 0x7fff) */
                /*    || (miData->savedMoves[i].score */
                /*        <= posmove->score) ); */

                if ( 0 == miData->savedMoves[ii].score ) {
                    foundEmpty = XP_TRUE;
                    mostest = ii;
                    break;
                } else if ( -1 == mostest ) {
                    mostest = ii;
                } else {
                    cmpVal = cmpMoves( &miData->savedMoves[mostest],
                                       &miData->savedMoves[ii] );
                    if ( !usePrev && cmpVal > 0 ) {
                        mostest = ii;
                    } else if ( usePrev && cmpVal < 0 ) {
                        mostest = ii;
                    }
                }
            }
        }
    }

    while ( mostest >= 0 ) {     /* while: so we can break */
        /* record the score we're dumping.  No point in considering any scores
           lower than this for the rest of this round. */
        /* miData->lowestSavedScore =  */
        /*     miData->savedMoves[lowest].score; */
        /* XP_DEBUGF( "lowestSavedScore now %d\n",  */
        /* miData->lowestSavedScore ); */
        if ( foundEmpty ) {
            /* we're good */
        } else {
            cmpVal = cmpMoves( posmove, &miData->savedMoves[mostest]);
            if ( !usePrev && cmpVal <= 0 ) {
                break;
            } else if ( usePrev && cmpVal >= 0 ) {
                break;
            }
        }
        /* XP_LOGF( "saving move with score %d at %d (replacing %d)\n", */
        /*          posmove->score, mostest, */
        /*          miData->savedMoves[mostest].score ); */
        XP_MEMCPY( &miData->savedMoves[mostest], posmove,
                   sizeof(miData->savedMoves[mostest]) );
        break;
    }
} /* saveMoveIfQualifies */

static void
set_search_limits( EngineCtxt* engine )
{
    MoveIterationData* miData = &engine->miData;
    /* If we're going to be searching backwards we want our highest cached
       move as the limit; otherwise the lowest */
    if ( 0 < miData->nInMoveCache ) {
        XP_U16 srcIndx = engine->usePrev
            ? engine->nMovesToSave-1 : miData->bottom;
        XP_MEMCPY( &miData->lastSeenMove, 
                   &miData->savedMoves[srcIndx],
                   sizeof(miData->lastSeenMove) );
        //miData->lowestSavedScore = 0;
    } else {
        /* we're doing this for first time */
        engine_reset( engine );
    }
}

static void
init_move_cache( EngineCtxt* engine )
{
    XP_U16 nInMoveCache = engine->nMovesToSave;
    MoveIterationData* miData = &engine->miData;
    XP_U16 ii;

    XP_ASSERT( engine->nMovesToSave == NUM_SAVED_ENGINE_MOVES );

    for ( ii = 0; ii < NUM_SAVED_ENGINE_MOVES; ++ii ) {
        if ( 0 == miData->savedMoves[ii].score ) {
            --nInMoveCache;
        } else {
            break;
        }
    }
    miData->nInMoveCache = nInMoveCache;
    miData->bottom = NUM_SAVED_ENGINE_MOVES - nInMoveCache;

    miData->curCacheIndex = engine->usePrev
        ? NUM_SAVED_ENGINE_MOVES - nInMoveCache - 1
        : NUM_SAVED_ENGINE_MOVES;
}

static PossibleMove*
next_from_cache( EngineCtxt* engine )
{
    PossibleMove* move = NULL;
    if ( !move_cache_empty( engine ) ) {
        MoveIterationData* miData = &engine->miData;
        if ( engine->usePrev ) {
            ++miData->curCacheIndex;
        } else {
            --miData->curCacheIndex;
        }
        move = &miData->savedMoves[miData->curCacheIndex];
    }
    return move;
}

static XP_Bool
move_cache_empty( const EngineCtxt* engine )
{
    XP_Bool empty;
    const MoveIterationData* miData = &engine->miData;

    if ( 0 == miData->nInMoveCache ) {
        empty = XP_TRUE;
    } else if ( engine->usePrev ) {
        empty = miData->curCacheIndex >= NUM_SAVED_ENGINE_MOVES - 1;
    } else {
        empty = miData->curCacheIndex <= miData->bottom;
    }
    return empty;
}

static array_edge*
edge_from_tile( const DictionaryCtxt* dict, array_edge* from, Tile tile ) 
{
    array_edge* edge = dict_edge_with_tile( dict, from, tile );
    if ( edge != NULL ) {
        edge = dict_follow( dict, edge );
    }
    return edge;
} /* edge_from_tile */

#ifdef DEBUG
static void
assertPMTilesInTiles( const EngineCtxt* engine, const PossibleMove* pm )
{
    TrayTileSet tts = engine->tts;
    XP_U16 nBlanks = 0;
    for ( int ii = 0; ii < pm->moveInfo.nTiles; ++ii ) {
        Tile moveTile = pm->moveInfo.tiles[ii].tile;
        if ( moveTile & TILE_BLANK_BIT ) {
            moveTile = engine->blankTile;
            ++nBlanks;
        }

        XP_Bool found = XP_FALSE;
        for ( int jj = 0; !found && jj < tts.nTiles; ++jj ) {
            found = moveTile == tts.tiles[jj];
            if ( found ) {
                // remove it from consideration for next tile
                tts.tiles[jj] = tts.tiles[--tts.nTiles];
            }
        }
        if ( !found ) {
            XP_LOGFF( "move tile with val %d not in tray", moveTile );
            XP_ASSERT(0);
        }
    }
    XP_ASSERT( nBlanks == pm->nBlanks );
}
#endif

#ifdef CPLUS
}
#endif

