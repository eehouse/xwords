/* -*- fill-column: 78; compile-command: "cd ../linux && make -j3 MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 1997 - 2006 by Eric House (xwords@eehouse.org).  All rights
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

#ifdef CPLUS
extern "C" {
#endif

#define eEND 0x63454e44

typedef XP_U8 Engine_rack[MAX_UNIQUE_TILES+1];

#define NUM_SAVED_MOVES 10

typedef struct BlankTuple {
    short col;
    Tile tile;
} BlankTuple;

typedef struct PossibleMove {
    XP_U16 score; /* Because I'm doing a memcmp to sort these things,
                     the comparison must be done differently on
                           little-endian platforms. */
    MoveInfo moveInfo;
    //XP_U16 whichBlanks; /* flags */
    Tile blankVals[MAX_COLS]; /* the faces for which we've substituted
                                 blanks */
} PossibleMove;

typedef struct MoveIterationData {
    PossibleMove savedMoves[NUM_SAVED_MOVES];
    XP_U16 lowestSavedScore;
    PossibleMove lastSeenMove;
    XP_U16 leftInMoveCache;
} MoveIterationData;

/* one bit per tile that's possible here *\/ */
typedef XP_U32 CrossBits;
typedef struct Crosscheck { CrossBits bits[2]; } Crosscheck;

struct EngineCtxt {
    const ModelCtxt* model;
    const DictionaryCtxt* dict;
    XW_UtilCtxt* util;

    Engine_rack rack;
    Tile blankTile;
    XP_Bool searchInProgress;
    XP_Bool searchHorizontal;
    XP_Bool isRobot;
    XP_Bool isFirstMove;
    XP_U16 numRows, numCols;
    XP_U16 curRow;
    XP_U16 blankCount;
    XP_U16 targetScore;
    XP_U16 star_row;
    XP_Bool returnNOW;
    MoveIterationData miData;

    XP_S16 blankValues[MAX_TRAY_TILES];
    Crosscheck rowChecks[MAX_ROWS]; // also used in xwscore
    XP_U16 scoreCache[MAX_ROWS];

    XP_U16 nTilesMax;
#ifdef XWFEATURE_SEARCHLIMIT
    XP_U16 nTilesMin;
    XP_U16 nTilesMinUser, nTilesMaxUser;
    XP_Bool tileLimitsKnown;
    const BdHintLimits* searchLimits;
#endif
    XP_U16 lastRowToFill;

#ifdef DEBUG
    XP_U16 curLimit;
#endif
    MPSLOT
}; /* EngineCtxt */

static void findMovesOneRow( EngineCtxt* engine );
static Tile localGetBoardTile( EngineCtxt* engine, XP_U16 col, 
                               XP_U16 row, XP_Bool substBlank );
static array_edge* edge_with_tile( const DictionaryCtxt* dict, 
                                   array_edge* from, Tile tile );
static XP_Bool scoreQualifies( EngineCtxt* engine, XP_U16 score );
static void findMovesForAnchor( EngineCtxt* engine, XP_S16* prevAnchor, 
                                XP_U16 col, XP_U16 row ) ;
static void figureCrosschecks( EngineCtxt* engine, XP_U16 col, 
                               XP_U16 row, XP_U16* scoreP,
                               Crosscheck* check );
static XP_Bool isAnchorSquare( EngineCtxt* engine, XP_U16 col, XP_U16 row );
static array_edge* follow( const DictionaryCtxt* dict, array_edge* in );
static array_edge* edge_from_tile( const DictionaryCtxt* dict, 
                                   array_edge* from, Tile tile );
static void leftPart( EngineCtxt* engine, Tile* tiles, XP_U16 tileLength, 
                      array_edge* edge, XP_U16 limit, XP_U16 firstCol,
                      XP_U16 anchorCol, XP_U16 row );
static void extendRight( EngineCtxt* engine, Tile* tiles, XP_U16 tileLength, 
                         array_edge* edge, XP_Bool accepting,
                         XP_U16 firstCol, XP_U16 col, XP_U16 row );
static array_edge* consumeFromLeft( EngineCtxt* engine, array_edge* edge, 
                                    short col, short row );
static XP_Bool rack_remove( EngineCtxt* engine, Tile tile, XP_Bool* isBlank );
static void rack_replace( EngineCtxt* engine, Tile tile, XP_Bool isBlank );
static void considerMove( EngineCtxt* engine, Tile* tiles, short tileLength,
                          short firstCol, short lastRow );
static void considerScoreWordHasBlanks( EngineCtxt* engine, XP_U16 blanksLeft,
                                        PossibleMove* posmove,
                                        XP_U16 lastRow,
                                        BlankTuple* usedBlanks,
                                        XP_U16 usedBlanksCount );
static void saveMoveIfQualifies( EngineCtxt* engine, PossibleMove* posmove );



#if defined __LITTLE_ENDIAN
static XP_S16 cmpMoves( PossibleMove* m1, PossibleMove* m2 );
# define CMPMOVES( m1, m2 )     cmpMoves( m1, m2 )
#elif defined __BIG_ENDIAN
# define CMPMOVES( m1, m2 )    XP_MEMCMP( m1, m2, sizeof(*(m1)))
#else
    error: need to pick one!!!
#endif

#ifdef NODE_CAN_4
# define ISACCEPTING(d,e) \
    ((ACCEPTINGMASK_NEW & ((array_edge_old*)(e))->bits) != 0)
# define IS_LAST_EDGE(d,e) \
    ((LASTEDGEMASK_NEW & ((array_edge_old*)(e))->bits) != 0)
# define EDGETILE(e,edge) \
    ((Tile)(((array_edge_old*)(edge))->bits & \
            ((e)->is_4_byte?LETTERMASK_NEW_4:LETTERMASK_NEW_3)))
#else
# define ISACCEPTING(d,e) \
    ((ACCEPTINGMASK_OLD & ((array_edge_old*)(e))->bits) != 0)
# define IS_LAST_EDGE(d,e) \
    ((LASTEDGEMASK_OLD & ((array_edge_old*)(e))->bits) != 0)
# define EDGETILE(d,edge) \
    ((Tile)(((array_edge_old*)(edge))->bits & LETTERMASK_OLD))
#endif

/* #define CROSSCHECK_CONTAINS(chk,tile) (((chk) & (1L<<(tile))) != 0) */
#define CROSSCHECK_CONTAINS(chk,tile) checkIsSet( (chk), (tile) )

#define HILITE_CELL( engine, col, row ) \
    util_hiliteCell( (engine)->util, (col), (row) )

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
engine_make( MPFORMAL XW_UtilCtxt* util, XP_Bool isRobot )
{
    EngineCtxt* result = (EngineCtxt*)XP_MALLOC( mpool, sizeof(*result) );
    XP_MEMSET( result, 0, sizeof(*result) );

    MPASSIGN(result->mpool, mpool);

    result->util = util;

    result->isRobot = isRobot;

    engine_reset( result );

    return result;
} /* engine_make */

void
engine_writeToStream( EngineCtxt* XP_UNUSED(ctxt), 
                      XWStreamCtxt* XP_UNUSED_DBG(stream) )
{
    /* nothing to save; see comment below */
#ifdef DEBUG
    stream_putU32( stream, eEND );
#endif
} /* engine_writeToStream */

EngineCtxt* 
engine_makeFromStream( MPFORMAL XWStreamCtxt* XP_UNUSED_DBG(stream), 
                       XW_UtilCtxt* util, XP_Bool isRobot )
{
    EngineCtxt* engine = engine_make( MPPARM(mpool) util, isRobot );

    /* All the engine's data seems to be used only in the process of finding a
       move.  So if we're willing to have the set of moves found lost across
       a save, there's nothing to do! */

    XP_ASSERT( stream_getU32( stream ) == eEND );

    return engine;
} /* engine_makeFromStream */

void
engine_reset( EngineCtxt* engine )
{
    XP_MEMSET( &engine->miData, 0, sizeof(engine->miData) );   
    engine->miData.lastSeenMove.score = 0xffff; /* max possible */
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
initTray( EngineCtxt* engine, const Tile* tiles, XP_U16 numTiles ) 
{
    XP_Bool result = numTiles > 0;
    XP_U16 i;

    if ( result ) {
        XP_MEMSET( engine->rack, 0, sizeof(engine->rack) );
        for ( i = 0; i < numTiles; ++i ) {
            Tile tile = *tiles++;
            XP_ASSERT( tile < MAX_UNIQUE_TILES );
            ++engine->rack[tile];
        }
    }

    return result;
} /* initTray */

#if defined __LITTLE_ENDIAN
static XP_S16
cmpMoves( PossibleMove* m1, PossibleMove* m2 )
{
    if ( m1->score == m2->score ) {
        return XP_MEMCMP( &m1->moveInfo, &m2->moveInfo, 
                          sizeof(*m1) - sizeof( m1->score ) );
    } else if ( m1->score < m2->score ) {
        return -1;
    } else {
        return 1;
    }
} /* cmpMoves */
#endif

static XP_Bool
chooseMove( EngineCtxt* engine, PossibleMove** move ) 
{
    XP_U16 i;
    PossibleMove* chosen;

    /* First, sort 'em.  Put the higher-scoring moves at the top where they'll
       get picked up first.  Don't sort if we're working for a robot; we've
       only been saving the single best move anyway.  At least not until we
       start applying other criteria than score to moves. */
    if ( engine->isRobot ) {
        chosen = &engine->miData.savedMoves[0];
    } else {
        while ( engine->miData.leftInMoveCache == 0 ) {
            XP_Bool done = XP_TRUE;
            for ( i = 0; i < NUM_SAVED_MOVES-1; ++i ) {
                if ( CMPMOVES( &engine->miData.savedMoves[i],
                               &engine->miData.savedMoves[i+1]) > 0 ) {
                    PossibleMove tmp;
                    XP_MEMCPY( &tmp, &engine->miData.savedMoves[i],
                               sizeof(tmp) );
                    XP_MEMCPY( &engine->miData.savedMoves[i],
                               &engine->miData.savedMoves[i+1],
                               sizeof(engine->miData.savedMoves[i]) );
                    XP_MEMCPY( &engine->miData.savedMoves[i+1], &tmp,
                               sizeof(engine->miData.savedMoves[i+1]) );
                    done = XP_FALSE;
                }
            }
            if ( done ) {
                engine->miData.leftInMoveCache = NUM_SAVED_MOVES;
            }
#if 0
            XP_DEBUGF( "sorted moves; scores are: " );
            for ( i = 0; i < NUM_SAVED_MOVES; ++i ) {
                XP_DEBUGF( "%d; ", engine->miData.savedMoves[i].score );
            }
            XP_DEBUGF( "\n" );
#endif
        }
        /* now pick the one we're supposed to return */
        chosen = &engine->miData.savedMoves[--engine->miData.leftInMoveCache];
    }

    *move = chosen; /* set either way */

    if ( chosen->score > 0 ) {

        if ( engine->miData.leftInMoveCache == 0 ) {
            XP_MEMCPY( &engine->miData.lastSeenMove, 
                       &engine->miData.savedMoves[0], 
                       sizeof(engine->miData.lastSeenMove) );
            engine->miData.lowestSavedScore = 0;
        }

        return XP_TRUE;
    } else {
        engine_reset( engine ); 
        return XP_FALSE;
    }
} /* chooseMove */

/* Return of XP_TRUE means that we ran to completion.  XP_FALSE means we were
 * interrupted.  Whether an actual move was found is indicated by what's
 * filled in in *newMove.
 */
XP_Bool
engine_findMove( EngineCtxt* engine, const ModelCtxt* model, 
                 const DictionaryCtxt* dict, const Tile* tiles,
                 XP_U16 nTiles, 
#ifdef XWFEATURE_SEARCHLIMIT
                 const BdHintLimits* searchLimits,
                 XP_Bool useTileLimits,
#endif
                 XP_U16 targetScore, XP_Bool* canMoveP, 
                 MoveInfo* newMove )
{
    XP_Bool result = XP_TRUE;
    XP_U16 star_row;

    engine->nTilesMax = MAX_TRAY_TILES;
#ifdef XWFEATURE_SEARCHLIMIT
    if ( useTileLimits ) {
        /* We'll want to use the numbers we've been using already unless
           there's been a reset.  In that case, though, provide the old
           ones as defaults */
        if ( !engine->tileLimitsKnown ) {

            XP_U16 nTilesMin = engine->nTilesMinUser;
            XP_U16 nTilesMax = engine->nTilesMaxUser;

            if ( util_getTraySearchLimits( engine->util, 
                                           &nTilesMin, &nTilesMax ) ) {
                engine->tileLimitsKnown = XP_TRUE;
                engine->nTilesMinUser = nTilesMin;
                engine->nTilesMaxUser = nTilesMax;
            } else {
                *canMoveP = XP_FALSE;                
                return XP_TRUE;
            }
        }

        engine->nTilesMin = engine->nTilesMinUser;
        engine->nTilesMax = engine->nTilesMaxUser;
    } else {
        engine->nTilesMin = 1;
    }
#endif

    engine->model = model;
    engine->dict = dict;
    engine->blankTile = dict_getBlankTile( dict );
    engine->returnNOW = XP_FALSE;
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
    *canMoveP = dict_getTopEdge(dict) != NULL && initTray( engine, tiles, 
                                                           nTiles );
    if ( *canMoveP  ) {

        util_engineStarting( engine->util, 
                             engine->rack[engine->blankTile] );

        engine->targetScore = targetScore;

        if ( engine->miData.leftInMoveCache == 0 ) {

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
                    findMovesOneRow( engine );
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
        outer:
            result = result; /* c++ wants a statement after the label */
        }
        /* Search is finished.  Choose (or just return) the best move found. */
        if ( engine->returnNOW ) {
            result = XP_FALSE;
        } else {
            PossibleMove* move;

            result = XP_TRUE;

            (void)chooseMove( engine, &move );
            XP_ASSERT( !!newMove );
            XP_MEMCPY( newMove, &move->moveInfo, sizeof(*newMove) );
        }

        util_engineStopping( engine->util );
    } else {
        /* set up a PASS.  I suspect the caller should be deciding how to
           handle this case itself, but this doesn't preclude its doing
           so.  */
        newMove->nTiles = 0;
    }

    return result;
} /* engine_findMove */

static void
findMovesOneRow( EngineCtxt* engine )
{
    XP_U16 lastCol = engine->numCols - 1;
    XP_U16 col, row = engine->curRow;
    XP_S16 prevAnchor;
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
    for ( col = 0; col <= lastCol; ++col ) {
        if ( col < firstSearchCol || col > lastSearchCol ) {
            engine->scoreCache[col] = 0;
        } else {
            figureCrosschecks( engine, col, row, 
                               &engine->scoreCache[col],
                               &engine->rowChecks[col]);
        }
    }

    prevAnchor = firstSearchCol - 1;
    for ( col = firstSearchCol; col <= lastSearchCol && !engine->returnNOW; 
          ++col ) {
        if ( isAnchorSquare( engine, col, row ) ) { 
            findMovesForAnchor( engine, &prevAnchor, col, row );
        }
    }
} /* findMovesOneRow */

static XP_Bool
lookup( const DictionaryCtxt* dict, array_edge* edge, Tile* buf, 
        XP_U16 tileIndex, XP_U16 length ) 
{
    while ( edge != NULL ) {
        Tile targetTile = buf[tileIndex];
        edge = edge_with_tile( dict, edge, targetTile );
        if ( edge == NULL ) { /* tile not available out of this node */
            return XP_FALSE;
        } else {
            if ( ++tileIndex == length ) { /* is this the last tile? */
                return ISACCEPTING(dict, edge);
            } else {
                edge = follow( dict, edge );
                continue;
            }
        }
    }
    return XP_FALSE;
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
#ifdef NODE_CAN_4
            candidateEdge += dict->nodeSize;
#else
            candidateEdge += 3;
#endif
        }
    }
 outer:
    if ( scoreP != NULL ) { 
        *scoreP = checkScore;
    }
} /* figureCrosschecks */

XP_Bool
engine_check( DictionaryCtxt* dict, Tile* tiles, XP_U16 nTiles )
{
    array_edge* in_edge = dict_getTopEdge( dict );

    return lookup( dict, in_edge, tiles, 0, nTiles );
} /* engine_check */

static Tile
localGetBoardTile( EngineCtxt* engine, XP_U16 col, XP_U16 row, 
                   XP_Bool substBlank )
{
    Tile result;
    XP_Bool isBlank, ignore;

    if ( !engine->searchHorizontal ) {
        XP_U16 tmp = col;
        col = row;
        row = tmp;
    }

    if ( model_getTile( engine->model, col, row, XP_FALSE,
                        0, /* don't get pending, so turn doesn't matter */
                        &result, &isBlank, &ignore, (XP_Bool*)NULL ) ) {
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

static void
hiliteForAnchor( EngineCtxt* engine, XP_U16 col, XP_U16 row )
{
    if ( !engine->searchHorizontal ) {
        XP_U16 tmp = col;
        col = row;
        row = tmp;
    }

    if ( !HILITE_CELL( engine, col, row ) ) {
        engine->returnNOW = XP_TRUE;
    }
} /* hiliteForAnchor */

static void
findMovesForAnchor( EngineCtxt* engine, XP_S16* prevAnchor, 
                    XP_U16 col, XP_U16 row ) 
{
    XP_S16 limit;
    array_edge* edge;
    array_edge* topEdge;
    Tile tiles[MAX_ROWS];

    hiliteForAnchor( engine, col, row );

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
            leftPart( engine, tiles, 0, topEdge, limit, col, col, row );
            goto done;
        } else {
            edge = consumeFromLeft( engine, topEdge, col, row );
        }
        DEBUG_ASSIGN(engine->curLimit, 0);
        extendRight( engine, tiles, 0, edge,
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
leftPart( EngineCtxt* engine, Tile* tiles, XP_U16 tileLength, 
          array_edge* edge, XP_U16 limit, XP_U16 firstCol,
          XP_U16 anchorCol, XP_U16 row )
{
    DEBUG_ASSIGN( engine->curLimit, tileLength );

    extendRight( engine, tiles, tileLength, edge, XP_FALSE, firstCol, 
                 anchorCol, row );
    if ( !engine->returnNOW ) {
        if ( (limit > 0) && (edge != NULL) ) {
#ifdef NODE_CAN_4
            XP_U16 nodeSize = engine->dict->nodeSize;
#endif
            if ( engine->nTilesMax > 0 ) {
                for ( ; ; ) {
                    XP_Bool isBlank;
                    Tile tile = EDGETILE( engine->dict, edge );
                    if ( rack_remove( engine, tile, &isBlank ) ) {
                        tiles[tileLength] = tile;
                        leftPart( engine, tiles, tileLength+1, 
                                  follow( engine->dict, edge ), 
                                  limit-1, firstCol-1, anchorCol, row );
                        rack_replace( engine, tile, isBlank );
                    }

                    if ( IS_LAST_EDGE( dict, edge ) || engine->returnNOW ) {
                        break;
                    }
#ifdef NODE_CAN_4
                    edge += nodeSize;
#else
                    edge += 3;
#endif

                }
            }
        }
    }
} /* leftPart */

static void
extendRight( EngineCtxt* engine, Tile* tiles, XP_U16 tileLength, 
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
                        extendRight( engine, tiles, tileLength+1, 
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
#ifdef NODE_CAN_4
                edge += dict->nodeSize;
#else
                edge += 3;
#endif
            }
        }

    } else if ( (edge = edge_with_tile( dict, edge, tile ) ) != NULL ) {
        accepting = ISACCEPTING( dict, edge );
        extendRight( engine, tiles, tileLength, follow(dict, edge), 
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
        considerMove( engine, tiles, tileLength, firstCol, row );
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

    if ( engine->rack[(short)tile] > 0 ) { /* we have the tile itself */
        --engine->rack[(short)tile];
        *isBlank = XP_FALSE;
    } else if ( engine->rack[blankIndex] > 0 ) { /* we have and must use a
                                                    blank */
        --engine->rack[(short)blankIndex];
        engine->blankValues[engine->blankCount++] = tile;
        *isBlank = XP_TRUE;
    } else { /* we can't satisfy the request */
        return XP_FALSE;
    }

    --engine->nTilesMax;
    return XP_TRUE;
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
considerMove( EngineCtxt* engine, Tile* tiles, XP_S16 tileLength,
              XP_S16 firstCol, XP_S16 lastRow )
{
    PossibleMove posmove;
    short col;
    BlankTuple blankTuples[MAX_NUM_BLANKS];

    if ( !util_engineProgressCallback( engine->util ) ) {
        engine->returnNOW = XP_TRUE;
    } else {

        /* if this never gets hit then the top-level caller of leftPart should
           never pass a value greater than 7 for limit.  I think we're always
           guaranteed to run out of tiles before finding a legal move with
           larger values but that it's expensive to look only to fail. */
        XP_ASSERT( engine->curLimit < MAX_TRAY_TILES );

        XP_MEMSET( &posmove, 0, sizeof(posmove) );

        for ( col = firstCol; posmove.moveInfo.nTiles < tileLength; ++col ) {
            /* is it one of the new ones? */
            if ( localGetBoardTile( engine, col, lastRow, XP_FALSE )
                 == EMPTY_TILE ) { 
                posmove.moveInfo.tiles[posmove.moveInfo.nTiles].tile = 
                    tiles[posmove.moveInfo.nTiles];
                posmove.moveInfo.tiles[posmove.moveInfo.nTiles].varCoord 
                    = (XP_U8)col;
                ++posmove.moveInfo.nTiles;
            }
        }
        posmove.moveInfo.isHorizontal = engine->searchHorizontal;
        posmove.moveInfo.commonCoord = (XP_U8)lastRow;


        considerScoreWordHasBlanks( engine, engine->blankCount, &posmove, 
                                    lastRow, blankTuples, 0 );
    }
} /* considerMove */

static void
considerScoreWordHasBlanks( EngineCtxt* engine, XP_U16 blanksLeft,
                            PossibleMove* posmove,
                            XP_U16 lastRow, BlankTuple* usedBlanks,
                            XP_U16 usedBlanksCount )
{
    XP_U16 i;

    if ( blanksLeft == 0 ) {
        XP_U16 score;

        score = figureMoveScore( engine->model,
                                 &posmove->moveInfo,
                                 engine, (XWStreamCtxt*)NULL,
                                 (WordNotifierInfo*)NULL, NULL, 0 );

        /* First, check that the score is even what we're interested in.  If
           it is, then go to the expense of filling in a PossibleMove to be
           compared in full */
        if ( scoreQualifies( engine, score ) ) {
            posmove->score = score;
            XP_MEMSET( &posmove->blankVals, 0, sizeof(posmove->blankVals) );
            for ( i = 0; i < usedBlanksCount; ++i ) {
                short col = usedBlanks[i].col;
                posmove->blankVals[col] = usedBlanks[i].tile;
            }
            XP_ASSERT( posmove->moveInfo.isHorizontal==
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
        bt = &usedBlanks[usedBlanksCount++];

        /* for each letter for which the blank might be standing in... */
        for ( i = 0; i < posmove->moveInfo.nTiles; ++i ) {
            CellTile tile = posmove->moveInfo.tiles[i].tile;
            if ( (tile & TILE_VALUE_MASK) == bTile && !IS_BLANK(tile) ) {
                posmove->moveInfo.tiles[i].tile |= TILE_BLANK_BIT;
                bt->col = i;
                bt->tile = bTile;
                considerScoreWordHasBlanks( engine, blanksLeft,
                                            posmove, lastRow,
                                            usedBlanks,
                                            usedBlanksCount );
                /* now put things back */
                posmove->moveInfo.tiles[i].tile &= ~TILE_BLANK_BIT;
            }
        }
    }
} /* considerScoreWordHasBlanks */

static void
saveMoveIfQualifies( EngineCtxt* engine, PossibleMove* posmove )
{
    XP_S16 lowest = 0;

    if ( !engine->isRobot ) { /* robot doesn't ask for next hint.... */

        /* we're not interested if we've seen this */
        if ( CMPMOVES( posmove, &engine->miData.lastSeenMove ) >= 0 ) {
            lowest = -1;
        } else {
            XP_S16 i;
            /* terminate i at 1 because lowest starts at 0 */
            for ( lowest = NUM_SAVED_MOVES-1, i = lowest - 1; i >= 0; --i ) { 
                /* Find the lowest value move and overwrite it.  Note that
                   there might not be one, as all may have the same or higher
                   scores and those that have the same score may compare
                   higher.

                   <eeh> can't have this asssertion until I start noting the
                   lowest saved score (setting miData.lowestSavedScore)
                   below. */
                /* 1/20/2001  I don't see that this assertion is valid.  I
                   simply don't understand why it isn't tripped all the time
                   in the old crosswords. */
                /* XP_ASSERT( (engine->miData.lastSeenMove.score == 0x7fff) */
                /*    || (engine->miData.savedMoves[i].score */
                /*        <= posmove->score) ); */

                if ( CMPMOVES( &engine->miData.savedMoves[lowest], 
                               &engine->miData.savedMoves[i] ) > 0 ) {
                    lowest = i;
                }
            }
        }
    }
    if ( lowest >= 0) {
        /* record the score we're dumping.  No point in considering any scores
           lower than this for the rest of this round. */
        engine->miData.lowestSavedScore = 
            engine->miData.savedMoves[lowest].score;
        /* XP_DEBUGF( "lowestSavedScore now %d\n",  */
        /* engine->miData.lowestSavedScore ); */
        if ( CMPMOVES( posmove, &engine->miData.savedMoves[lowest]) > 0 ) {
            XP_MEMCPY( &engine->miData.savedMoves[lowest], posmove,
                       sizeof(engine->miData.savedMoves[lowest]) );
            /*     XP_DEBUGF( "just saved move with score %d\n",  */
            /*      engine->miData.savedMoves[lowest].score ); */
        }
    }
} /* saveMoveIfQualifies */

static XP_Bool
scoreQualifies( EngineCtxt* engine, XP_U16 score )
{
    XP_Bool qualifies = XP_FALSE;

    if ( (score > engine->miData.lastSeenMove.score) 
         || (score > engine->targetScore)
         || (score < engine->miData.lowestSavedScore) ) {
        /* do nothing */
    } else {
        XP_S16 i;
        /* Look at each saved score, and return true as soon as one's found
           with a lower or equal score to this.  <eeh> As an optimization,
           consider remembering what the lowest score is *once there are
           NUM_SAVED_MOVES moves in here* and doing a quick test on that. Or
           better, keeping the list in sorted order. */
        for ( i = engine->isRobot? 0: NUM_SAVED_MOVES-1; i >= 0; --i ) {
            if ( score >= engine->miData.savedMoves[i].score ) {
                qualifies = XP_TRUE;
                break;
            }
        }
    }
    return qualifies;
} /* scoreQualifies */

static array_edge*
edge_with_tile( const DictionaryCtxt* dict, array_edge* from, Tile tile ) 
{
    for ( ; ; ) {
        Tile candidate = EDGETILE(dict,from);
        if ( candidate == tile ) {
            break;
        }

        if ( IS_LAST_EDGE(dict, from ) ) {
            from = NULL;
            break;
        }
#ifdef NODE_CAN_4
        from += dict->nodeSize;
#else
        from += 3;
#endif

    }

    return from;
} /* edge_with_tile */

static unsigned long
index_from( const DictionaryCtxt* dict, array_edge* p_edge ) 
{
    unsigned long result;

#ifdef NODE_CAN_4
    array_edge_new* edge = (array_edge_new*)p_edge;
    result = ((edge->highByte << 8) | edge->lowByte) & 0x0000FFFF;

    if ( dict->is_4_byte ) {
        result |= ((XP_U32)edge->moreBits) << 16;
    } else {
        XP_ASSERT( dict->nodeSize == 3 );
        if ( (edge->bits & EXTRABITMASK_NEW) != 0 ) { 
            result |= 0x00010000; /* using | instead of + saves 4 bytes */
        }
    }
#else
    array_edge_old* edge = (array_edge_old*)p_edge;
    result = ((edge->highByte << 8) | edge->lowByte) & 0x0000FFFF;
    if ( (edge->bits & EXTRABITMASK_OLD) != 0 ) { 
        result |= 0x00010000; /* using | instead of + saves 4 bytes */
    }
#endif

    return result;
} /* index_from */

static array_edge*
follow( const DictionaryCtxt* dict, array_edge* in ) 
{
    XP_U32 index = index_from( dict, in );
    array_edge* result = index > 0? 
        dict_edge_for_index( dict, index ): (array_edge*)NULL;
    return result;
} /* follow */

static array_edge*
edge_from_tile( const DictionaryCtxt* dict, array_edge* from, Tile tile ) 
{
    array_edge* edge = edge_with_tile( dict, from, tile );
    if ( edge != NULL ) {
        edge = follow( dict, edge );
    }
    return edge;
} /* edge_from_tile */

#ifdef CPLUS
}
#endif

