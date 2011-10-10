/* -*- compile-command: "cd ../linux && make -j3 MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 2000-2009 by Eric House (xwords@eehouse.org).  All rights reserved.
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

/* #include <assert.h> */

#include "comtypes.h"
#include "modelp.h"
#include "xwstream.h"
#include "util.h"
#include "pool.h"
#include "game.h"
#include "memstream.h"
#include "strutils.h"
#include "LocalizedStrIncludes.h"

#ifdef CPLUS
extern "C" {
#endif

#define mEND 0x6d454e44

#define MAX_PASSES 2 /* how many times can all players pass? */

/****************************** prototypes ******************************/
typedef void (*MovePrintFuncPre)(ModelCtxt*, XP_U16, StackEntry*, void*);
typedef void (*MovePrintFuncPost)(ModelCtxt*, XP_U16, StackEntry*, XP_S16, 
                                  void*);

static void incrPendingTileCountAt( ModelCtxt* model, XP_U16 col, 
                                    XP_U16 row );
static void decrPendingTileCountAt( ModelCtxt* model, XP_U16 col, 
                                    XP_U16 row );
static void notifyBoardListeners( ModelCtxt* model, XP_U16 turn, 
                                  XP_U16 col, XP_U16 row, XP_Bool added );
static void notifyTrayListeners( ModelCtxt* model, XP_U16 turn, 
                                 XP_S16 index1, XP_S16 index2 );
static void notifyDictListeners( ModelCtxt* model, XP_S16 playerNum, 
                                 DictionaryCtxt* oldDict,
                                 DictionaryCtxt* newDict );

static CellTile getModelTileRaw( const ModelCtxt* model, XP_U16 col, 
                                 XP_U16 row );
static void setModelTileRaw( ModelCtxt* model, XP_U16 col, XP_U16 row, 
                             CellTile tile );
static void assignPlayerTiles( ModelCtxt* model, XP_S16 turn, 
                               const TrayTileSet* tiles );
static void makeTileTrade( ModelCtxt* model, XP_S16 player, 
                           TrayTileSet* oldTiles, TrayTileSet* newTiles );
static XP_S16 commitTurn( ModelCtxt* model, XP_S16 turn, 
                          TrayTileSet* newTiles, XWStreamCtxt* stream, 
                          WordNotifierInfo* wni, XP_Bool useStack );
static void buildModelFromStack( ModelCtxt* model, StackCtxt* stack, 
                                 XP_Bool useStack, XP_U16 initial, 
                                 XWStreamCtxt* stream, WordNotifierInfo* wni, 
                                 MovePrintFuncPre mpfpr, 
                                 MovePrintFuncPost mpfpo, void* closure );
static void setPendingCounts( ModelCtxt* model, XP_S16 turn );
static void loadPlayerCtxt( XWStreamCtxt* stream, XP_U16 version, 
                            PlayerCtxt* pc );
static void writePlayerCtxt( XWStreamCtxt* stream, PlayerCtxt* pc );
static XP_U16 model_getRecentPassCount( ModelCtxt* model );
static XP_Bool recordWord( const XP_UCHAR* word, XP_Bool isLegal, void* clsur );

/*****************************************************************************
 *
 ****************************************************************************/
ModelCtxt*
model_make( MPFORMAL DictionaryCtxt* dict,
            const PlayerDicts* dicts, XW_UtilCtxt* util, XP_U16 nCols, 
            XP_U16 nRows )
{
    ModelCtxt* result = (ModelCtxt*)XP_MALLOC( mpool, sizeof( *result ) );
    if ( result != NULL ) {
        XP_MEMSET( result, 0, sizeof(*result) );
        MPASSIGN(result->vol.mpool, mpool);

        result->vol.util = util;
        result->vol.wni.proc = recordWord;
        result->vol.wni.closure = &result->vol.rwi;

        model_init( result, nCols, nRows );

        XP_ASSERT( !!util->gameInfo );
        result->vol.gi = util->gameInfo;

        model_setDictionary( result, dict );
        model_setPlayerDicts( result, dicts );
    }

    return result;
} /* model_make */

ModelCtxt* 
model_makeFromStream( MPFORMAL XWStreamCtxt* stream, DictionaryCtxt* dict, 
                      const PlayerDicts* dicts, XW_UtilCtxt* util )
{
    ModelCtxt* model;
    XP_U16 nCols, nRows;
    short i;
    XP_Bool hasDict;
    XP_U16 nPlayers;
    XP_U16 version = stream_getVersion( stream );

    XP_ASSERT( !!dict || !!dicts );

    nCols = (XP_U16)stream_getBits( stream, NUMCOLS_NBITS );
    nRows = (XP_U16)stream_getBits( stream, NUMCOLS_NBITS );

    hasDict = (version >= STREAM_VERS_MODEL_NO_DICT)
        ? XP_FALSE : stream_getBits( stream, 1 );
    nPlayers = (XP_U16)stream_getBits( stream, NPLAYERS_NBITS );

    if ( hasDict ) {
        DictionaryCtxt* savedDict = util_makeEmptyDict( util );
        dict_loadFromStream( savedDict, stream );
        dict_destroy( savedDict );
    }

    model = model_make( MPPARM(mpool) dict, dicts, util, nCols, nRows );
    model->nPlayers = nPlayers;

    stack_loadFromStream( model->vol.stack, stream );

    buildModelFromStack( model, model->vol.stack, XP_FALSE, 0, 
                         (XWStreamCtxt*)NULL, (WordNotifierInfo*)NULL,
                         (MovePrintFuncPre)NULL, (MovePrintFuncPost)NULL, NULL );

    for ( i = 0; i < model->nPlayers; ++i ) {
        loadPlayerCtxt( stream, version, &model->players[i] );
        setPendingCounts( model, i );
        invalidateScore( model, i );
    }

    XP_ASSERT( stream_getU32( stream ) == mEND );

    return model;
} /* model_makeFromStream */

void
model_writeToStream( ModelCtxt* model, XWStreamCtxt* stream )
{
    short i;

    stream_putBits( stream, NUMCOLS_NBITS, model->nCols );
    stream_putBits( stream, NUMCOLS_NBITS, model->nRows );

    /* we have two bits for nPlayers, so range must be 0..3, not 1..4 */
    stream_putBits( stream, NPLAYERS_NBITS, model->nPlayers );

    stack_writeToStream( model->vol.stack, stream );

    for ( i = 0; i < model->nPlayers; ++i ) {
        writePlayerCtxt( stream, &model->players[i] );
    }

#ifdef DEBUG
    stream_putU32( stream, mEND );
#endif
} /* model_writeToStream */

#ifdef TEXT_MODEL
void
model_writeToTextStream( const ModelCtxt* model, XWStreamCtxt* stream )
{
    const DictionaryCtxt* dict = model_getDictionary( model );
    int width = dict_getMaxWidth( dict );
    XP_UCHAR empty[4] = {0};
    XP_U16 ii;
    XP_U16 col, row;

    for ( ii = 0; ii < width; ++ii ) {
        empty[ii] = '.';
    }

    for ( row = 0; row < model->nRows; ++row ) {
        XP_UCHAR buf[256];
        XP_U16 len = 0;
        for ( col = 0; col < model->nCols; ++col ) {
            XP_Bool isBlank;
            Tile tile;
            if ( model_getTile( model, col, row,  XP_FALSE, -1, &tile,
                                &isBlank, NULL, NULL ) ) {
                const XP_UCHAR* face = dict_getTileString( dict, tile );
                XP_UCHAR lower[1+XP_STRLEN(face)];
                XP_STRNCPY( lower, face, sizeof(lower) );
                if ( isBlank ) {
                    XP_LOWERSTR( lower );
                }
                len += snprintf( &buf[len], sizeof(buf) - len, "%*s", 
                                 width, lower );
            } else {
                len += snprintf( &buf[len], sizeof(buf) - len, "%s", empty );
            }
        }
        buf[len++] = '\n';
        buf[len] = '\0';
        stream_catString( stream, buf );
    }
}
#endif

void
model_init( ModelCtxt* model, XP_U16 nCols, XP_U16 nRows )
{
    ModelVolatiles vol = model->vol; /* save vol so we don't wipe it out */

    XP_ASSERT( model != NULL );
    XP_MEMSET( model, 0, sizeof( *model ) );
    XP_MEMSET( &model->tiles, TILE_EMPTY_BIT, sizeof(model->tiles) );

    model->nCols = nCols;
    model->nRows = nRows;

    model->vol = vol;

    if ( !!model->vol.stack ) {
        stack_init( model->vol.stack );
    } else {
        model->vol.stack = stack_make( MPPARM(model->vol.mpool)
                                       util_getVTManager(model->vol.util));
    }
} /* model_init */

void
model_destroy( ModelCtxt* model )
{
    stack_destroy( model->vol.stack );
    /* is this it!? */
    XP_FREE( model->vol.mpool, model );
} /* model_destroy */

static void
buildModelFromStack( ModelCtxt* model, StackCtxt* stack, XP_Bool useStack, 
                     XP_U16 initial, XWStreamCtxt* stream, 
                     WordNotifierInfo* wni, MovePrintFuncPre mpf_pre, 
                     MovePrintFuncPost mpf_post, void* closure )
{
    StackEntry entry;
    XP_U16 ii;
    XP_S16 moveScore = 0; /* keep compiler happy */

    for ( ii = initial; stack_getNthEntry( stack, ii, &entry ); ++ii ) {

        if ( !!mpf_pre ) {
            (*mpf_pre)( model, ii, &entry, closure );
        }

        switch ( entry.moveType ) {
        case MOVE_TYPE:

            model_makeTurnFromMoveInfo( model, entry.playerNum,
                                        &entry.u.move.moveInfo);
            moveScore = commitTurn( model, entry.playerNum, 
                                    &entry.u.move.newTiles, 
                                    stream, wni, useStack );
            break;
        case TRADE_TYPE:
            makeTileTrade( model, entry.playerNum, &entry.u.trade.oldTiles,
                           &entry.u.trade.newTiles );
            break;
        case ASSIGN_TYPE:
            assignPlayerTiles( model, entry.playerNum, 
                               &entry.u.assign.tiles );
            break;
        case PHONY_TYPE: /* nothing to add */
            model_makeTurnFromMoveInfo( model, entry.playerNum,
                                        &entry.u.phony.moveInfo);
            /* do something here to cause it to print */
            (void)getCurrentMoveScoreIfLegal( model, entry.playerNum, stream,
                                              wni, &moveScore );
            moveScore = 0;
            model_resetCurrentTurn( model, entry.playerNum );

            break;
        default:
            XP_ASSERT(0);
        }

        if ( !!mpf_post ) {
            (*mpf_post)( model, ii, &entry, moveScore, closure );
        }
    }
} /* buildModelFromStack */

void
model_setNPlayers( ModelCtxt* model, XP_U16 nPlayers )
{
    model->nPlayers = nPlayers;
} /* model_setNPlayers */

XP_U16
model_getNPlayers( const ModelCtxt* model )
{
    return model->nPlayers;
}

static void
setStackBits( ModelCtxt* model, const DictionaryCtxt* dict )
{
    if ( NULL != dict ) {
        XP_U16 nFaces = dict_numTileFaces( dict );
        XP_ASSERT( !!model->vol.stack );
        stack_setBitsPerTile( model->vol.stack, nFaces <= 32? 5 : 6 );
    }
}

void
model_setDictionary( ModelCtxt* model, DictionaryCtxt* dict )
{
    DictionaryCtxt* oldDict = model->vol.dict;
    model->vol.dict = dict;

    if ( !!dict ) {
        setStackBits( model, dict );
        notifyDictListeners( model, -1, oldDict, dict );
    }
} /* model_setDictionary */

void
model_setPlayerDicts( ModelCtxt* model, const PlayerDicts* dicts )
{
    if ( !!dicts ) {
        XP_U16 ii;
#ifdef DEBUG
        DictionaryCtxt* gameDict = model_getDictionary( model );
#endif
        for ( ii = 0; ii < VSIZE(dicts->dicts); ++ii ) {
            DictionaryCtxt* oldDict = model->vol.dicts.dicts[ii];
            DictionaryCtxt* newDict = dicts->dicts[ii];
            if ( oldDict != newDict ) {
                XP_ASSERT( NULL == newDict || NULL == gameDict 
                           || dict_tilesAreSame( gameDict, newDict ) );
                model->vol.dicts.dicts[ii] = newDict;
                notifyDictListeners( model, ii, oldDict, newDict );
                setStackBits( model, newDict );
            }
        }
    }
}           

DictionaryCtxt*
model_getDictionary( const ModelCtxt* model )
{
    XP_U16 ii;
    DictionaryCtxt* result = model->vol.dict;
    for ( ii = 0; !result && ii < VSIZE(model->vol.dicts.dicts); ++ii ) {
        result = model->vol.dicts.dicts[ii];
    }
    return result;
} /* model_getDictionary */

DictionaryCtxt*
model_getPlayerDict( const ModelCtxt* model, XP_U16 playerNum )
{
    DictionaryCtxt* dict = model->vol.dicts.dicts[playerNum];
    if ( NULL == dict ) {
        dict = model->vol.dict;
    }
    XP_ASSERT( !!dict );
    return dict;
}

static void
destroyNotNull( DictionaryCtxt** dictp )
{
    if ( !!*dictp ) {
        dict_destroy( *dictp );
        *dictp = NULL;
    }
}

void
model_destroyDicts( ModelCtxt* model )
{
    XP_U16 ii;
    for ( ii = 0; ii < VSIZE(model->vol.dicts.dicts); ++ii ) {
        destroyNotNull( &model->vol.dicts.dicts[ii] );
    }
    destroyNotNull( &model->vol.dict );
}

static XP_Bool
getPendingTileFor( const ModelCtxt* model, XP_U16 turn, XP_U16 col, XP_U16 row,
                   CellTile* cellTile )
{
    XP_Bool found = XP_FALSE;
    const PlayerCtxt* player;
    const PendingTile* pendings;
    XP_U16 i;

    XP_ASSERT( turn < VSIZE(model->players) );

    player = &model->players[turn];
    pendings = player->pendingTiles;
    for ( i = 0; i < player->nPending; ++i ) {

        if ( (pendings->col == col) && (pendings->row == row) ) {
            *cellTile = pendings->tile;
            found = XP_TRUE;
            XP_ASSERT ( (*cellTile & TILE_EMPTY_BIT) == 0 );
            break;
        }
        ++pendings;
    }
    
    return found;
} /* getPendingTileFor */

XP_Bool
model_getTile( const ModelCtxt* model, XP_U16 col, XP_U16 row, 
               XP_Bool getPending, XP_S16 turn, Tile* tileP, XP_Bool* isBlank, 
               XP_Bool* pendingP, XP_Bool* recentP )
{
    CellTile cellTile = getModelTileRaw( model, col, row );
    XP_Bool pending = XP_FALSE;
    
    if ( (cellTile & TILE_PENDING_BIT) != 0 ) {
        if ( getPending
             && getPendingTileFor( model, turn, col, row, &cellTile ) ) {

            /* it's pending, but caller doesn't want to see it */
            pending = XP_TRUE;
        } else {
            cellTile = EMPTY_TILE;
        }
    }

    /* this needs to happen after the above b/c cellTile gets changed */
    if ( (cellTile & TILE_EMPTY_BIT) != 0 ) {
        return XP_FALSE;
    }
    
    if ( NULL != tileP ) {
        *tileP = cellTile & TILE_VALUE_MASK;
    }
    if ( NULL != isBlank ) {
        *isBlank = IS_BLANK(cellTile);
    }
    if ( NULL != pendingP ) {
        *pendingP = pending;
    }
    if ( !!recentP ) {
        *recentP = (cellTile & PREV_MOVE_BIT) != 0;
    }

    return XP_TRUE;
} /* model_getTile */

void
model_listPlacedBlanks( ModelCtxt* model, XP_U16 turn,
                        XP_Bool includePending, BlankQueue* bcp )
{
    XP_U16 nCols = model_numCols( model );
    XP_U16 nRows = model_numRows( model );
    XP_U16 col, row;

    XP_U16 nBlanks = 0;

    for ( row = 0; row < nRows; ++row ) {
        for ( col = 0; col < nCols; ++col ) {
            CellTile cellTile = getModelTileRaw( model, col, row );

            if ( (cellTile & TILE_PENDING_BIT) != 0 ) {
                if ( !includePending ||
                     !getPendingTileFor( model, turn, col, row, &cellTile ) ) {
                    continue;
                }
            }

            if ( (cellTile & TILE_BLANK_BIT) != 0 ) {
                bcp->col[nBlanks] = (XP_U8)col;
                bcp->row[nBlanks] = (XP_U8)row;
                ++nBlanks;
            }
        }
    }

    bcp->nBlanks = nBlanks;
} /* model_listPlacedBlanks */

void
model_foreachPrevCell( ModelCtxt* model, BoardListener bl, void* closure )
{
    XP_U16 col, row;

    for ( col = 0; col < model->nCols; ++col ) {
        for ( row = 0; row < model->nRows; ++row) {
            CellTile tile = getModelTileRaw( model, col, row );
            if ( (tile & PREV_MOVE_BIT) != 0 ) {
                (*bl)( closure, (XP_U16)CELL_OWNER(tile), col, row, XP_FALSE );
            }
        }
    }
} /* model_foreachPrevCell */

static void
clearAndNotify( void* closure, XP_U16 XP_UNUSED(turn), XP_U16 col, XP_U16 row, 
                XP_Bool XP_UNUSED(added) )
{
    ModelCtxt* model = (ModelCtxt*)closure;
    CellTile tile = getModelTileRaw( model, col, row );
    setModelTileRaw( model, col, row, (CellTile)(tile & ~PREV_MOVE_BIT) );
    
    notifyBoardListeners( model, (XP_U16)CELL_OWNER(tile), col, row, 
                          XP_FALSE );
} /* clearAndNotify */

static void
clearLastMoveInfo( ModelCtxt* model )
{
    model_foreachPrevCell( model, clearAndNotify, model );
} /* clearLastMoveInfo */

static void
invalLastMove( ModelCtxt* model )
{
    if ( !!model->vol.boardListenerFunc ) {
        model_foreachPrevCell( model, model->vol.boardListenerFunc,
                               model->vol.boardListenerData );
    }
} /* invalLastMove */

void
model_foreachPendingCell( ModelCtxt* model, XP_S16 turn,
                          BoardListener bl, void* closure )
{
    PendingTile* pt;
    PlayerCtxt* player;
    XP_S16 count;

    XP_ASSERT( turn >= 0 );
    player = &model->players[turn];
    count = player->nPending;

    for ( pt = player->pendingTiles; count--; ++pt ) {
        XP_U16 col, row;

        col = pt->col;
        row = pt->row;

        (*bl)( closure, turn, pt->col, pt->row, XP_FALSE );
    }
} /* model_invalPendingCells */

XP_U16 
model_getCellOwner( ModelCtxt* model, XP_U16 col, XP_U16 row )
{
    CellTile tile;
    XP_U16 result;

    tile = getModelTileRaw( model, col, row );

    result = CELL_OWNER(tile);

    return result;
} /* model_getCellOwner */

static void
setModelTileRaw( ModelCtxt* model, XP_U16 col, XP_U16 row, CellTile tile )
{
    XP_ASSERT( col < MAX_COLS );
    XP_ASSERT( row < MAX_ROWS );
    model->tiles[col][row] = tile;
} /* model_setTile */

static CellTile 
getModelTileRaw( const ModelCtxt* model, XP_U16 col, XP_U16 row )
{
    XP_ASSERT( col < MAX_COLS );
    XP_ASSERT( row < MAX_ROWS );
    return model->tiles[col][row];
} /* getModelTileRaw */

static void
undoFromMoveInfo( ModelCtxt* model, XP_U16 turn, Tile blankTile, MoveInfo* mi )
{
    XP_U16 col, row, i;
    XP_U16* other;
    MoveInfoTile* tinfo;

    col = row = mi->commonCoord;
    other = mi->isHorizontal? &col: &row;

    for ( tinfo = mi->tiles, i = 0; i < mi->nTiles; ++tinfo, ++i ) {
        Tile tile;

        *other = tinfo->varCoord;
        tile = tinfo->tile;

        setModelTileRaw( model, col, row, EMPTY_TILE );
        notifyBoardListeners( model, turn, col, row, XP_FALSE );
        --model->vol.nTilesOnBoard;

        if ( IS_BLANK(tile) ) {
            tile = blankTile;
        }
        model_addPlayerTile( model, turn, -1, tile );
    }
    adjustScoreForUndone( model, mi, turn );
} /* undoFromMoveInfo */

/* Remove tiles in a set from tray and put them back in the pool.
 */
static void
replaceNewTiles( ModelCtxt* model, PoolContext* pool, XP_U16 turn,
                 TrayTileSet* tileSet )
{
    Tile* t;
    XP_U16 i, nTiles;

    for ( t = tileSet->tiles, i = 0, nTiles = tileSet->nTiles;
          i < nTiles; ++i ) {
        XP_S16 index;
        Tile tile = *t++;

        index = model_trayContains( model, turn, tile );
        XP_ASSERT( index >= 0 );
        model_removePlayerTile( model, turn, index );
    }
    if ( !!pool ) {
        pool_replaceTiles( pool, tileSet);
    }
} /* replaceNewTiles */

/* Turn the most recent move into a phony.
 */
void
model_rejectPreviousMove( ModelCtxt* model, PoolContext* pool, XP_U16* turn )
{
    StackCtxt* stack = model->vol.stack;
    StackEntry entry;
    Tile blankTile = dict_getBlankTile( model_getDictionary(model) );

    stack_popEntry( stack, &entry );
    XP_ASSERT( entry.moveType == MOVE_TYPE );

    replaceNewTiles( model, pool, entry.playerNum, &entry.u.move.newTiles );
    undoFromMoveInfo( model, entry.playerNum, blankTile,
                      &entry.u.move.moveInfo );

    stack_addPhony( stack, entry.playerNum, &entry.u.phony.moveInfo );

    *turn = entry.playerNum;
} /* model_rejectPreviousMove */

/* Undo a move, but only if it's the move we're expecting to undo (as
 * indicated by *moveNumP, if >= 0).
 */
XP_Bool
model_undoLatestMoves( ModelCtxt* model, PoolContext* pool, 
                       XP_U16 nMovesSought, XP_U16* turnP, XP_S16* moveNumP )
{
    StackCtxt* stack = model->vol.stack;
    StackEntry entry;
    XP_U16 turn = 0;
    Tile blankTile = dict_getBlankTile( model_getDictionary(model) );
    XP_Bool success = XP_TRUE;
    XP_S16 moveSought = !!moveNumP ? *moveNumP : -1;
    XP_U16 nMovesUndone;
    XP_U16 nStackEntries;

    nStackEntries = stack_getNEntries( stack );
    if ( nStackEntries < nMovesSought ) {
        return XP_FALSE;
    } else if ( nStackEntries <= model->nPlayers ) {
        return XP_FALSE;
    }

    for ( nMovesUndone = 0; success && nMovesUndone < nMovesSought; ) {

        success = stack_popEntry( stack, &entry );
        if ( success ) {
            ++nMovesUndone;

            if ( moveSought < 0 ) {
                moveSought = entry.moveNum - 1;
            } else if ( moveSought-- != entry.moveNum ) {
                success = XP_FALSE;
                break;
            }

            turn = entry.playerNum;
            model_resetCurrentTurn( model, turn );

            if ( entry.moveType == MOVE_TYPE ) {

                /* get the tiles out of player's tray and back into the
                   pool */
                replaceNewTiles( model, pool, turn, &entry.u.move.newTiles);

                undoFromMoveInfo( model, turn, blankTile, 
                                  &entry.u.move.moveInfo );
            } else if ( entry.moveType == TRADE_TYPE ) {

                if ( pool != NULL ) {
                    /* If there's no pool, assume we're doing this for
                       scoring purposes only. */
                    replaceNewTiles( model, pool, turn, 
                                     &entry.u.trade.newTiles );

                    pool_removeTiles( pool, &entry.u.trade.oldTiles );
                    assignPlayerTiles( model, turn, &entry.u.trade.oldTiles );
                }

            } else if ( entry.moveType == PHONY_TYPE ) {

                /* nothing to do, since nothing happened */

            } else {
                XP_ASSERT( entry.moveType == ASSIGN_TYPE );
                success = XP_FALSE;
                break;
            }
        }
    }

    /* Find the first MOVE still on the stack and highlight its tiles since
       they're now the most recent move. Trades and lost turns ignored.  */
    nStackEntries = stack_getNEntries( stack );
    for ( ; ; ) {
        StackEntry entry;
        if ( nStackEntries == 0 ||
             !stack_getNthEntry( stack, nStackEntries - 1, &entry ) ) {
            break;
        }
        if ( entry.moveType == MOVE_TYPE ) {
            XP_U16 nTiles = entry.u.move.moveInfo.nTiles;
            XP_U16 col, row;
            XP_U16* varies;

            if ( entry.u.move.moveInfo.isHorizontal ) {
                row = entry.u.move.moveInfo.commonCoord;
                varies = &col;
            } else {
                col = entry.u.move.moveInfo.commonCoord;
                varies = &row;
            }

            while ( nTiles-- ) {
                CellTile tile;

                *varies = entry.u.move.moveInfo.tiles[nTiles].varCoord;
                tile = getModelTileRaw( model, col, row );
                setModelTileRaw( model, col, row, 
                                 (CellTile)(tile | PREV_MOVE_BIT) );
                notifyBoardListeners( model, entry.playerNum, col, row, 
                                      XP_FALSE );
            }
            break;
        } else if ( entry.moveType == ASSIGN_TYPE ) {
            break;
        } else {
            --nStackEntries;        /* look at the next one */
        }
    }

    if ( nMovesUndone != nMovesSought ) {
        success = XP_FALSE;
    }

    if ( success ) {
        if ( !!turnP ) {
            *turnP = turn;
        }
        if ( !!moveNumP ) {
            *moveNumP = entry.moveNum;
        }
    } else {
        while ( nMovesUndone-- ) {
            /* undo isn't enough here: pool's got tiles in it!! */
            XP_ASSERT( 0 );
            stack_redo( stack );
        }
    }

    return success;
} /* model_undoLatestMoves */

void
model_trayToStream( ModelCtxt* model, XP_S16 turn, XWStreamCtxt* stream )
{
    PlayerCtxt* player;
    XP_ASSERT( turn >= 0 );
    player = &model->players[turn];

    traySetToStream( stream, &player->trayTiles );
} /* model_trayToStream */

void
model_currentMoveToStream( ModelCtxt* model, XP_S16 turn, 
                           XWStreamCtxt* stream )
{
    PlayerCtxt* player;
    XP_S16 numTiles;

    XP_ASSERT( turn >= 0 );
    player = &model->players[turn];
    numTiles = player->nPending;

    stream_putBits( stream, NTILES_NBITS, numTiles );

    while ( numTiles-- ) {
        Tile tile;
        XP_U16 col, row;
        XP_Bool isBlank;

        model_getCurrentMoveTile( model, turn, &numTiles, &tile,
                                  &col, &row, &isBlank );
        XP_ASSERT( numTiles >= 0 );
        stream_putBits( stream, TILE_NBITS, tile );
        stream_putBits( stream, NUMCOLS_NBITS, col );
        stream_putBits( stream, NUMCOLS_NBITS, row );
        stream_putBits( stream, 1, isBlank );
    }
} /* model_turnToStream */

/* Take stream as the source of info about what tiles to move from tray to
 * board.  Undo any current move first -- a player on this device might be
 * using the board as scratch during another player's turn.  For each tile,
 * assert that it's in the tray, remove it from the tray, and place it on the
 * board.
 */
void
model_makeTurnFromStream( ModelCtxt* model, XP_U16 playerNum,
                          XWStreamCtxt* stream )
{
    XP_U16 numTiles;
    Tile blank = dict_getBlankTile( model_getDictionary(model) );

    model_resetCurrentTurn( model, playerNum );

    numTiles = (XP_U16)stream_getBits( stream, NTILES_NBITS );

    XP_LOGF( "%s: numTiles=%d", __func__, numTiles );

    while ( numTiles-- ) {
        XP_S16 foundAt;
        Tile moveTile;
        Tile tileFace = (Tile)stream_getBits( stream, TILE_NBITS );
        XP_U16 col = (XP_U16)stream_getBits( stream, NUMCOLS_NBITS );
        XP_U16 row = (XP_U16)stream_getBits( stream, NUMCOLS_NBITS );
        XP_Bool isBlank = stream_getBits( stream, 1 );

        /* This code gets called both for the server, which has all the
           tiles in its tray, and for a client, which has "EMPTY" tiles
           only.  If it's the empty case, we stuff a real tile into the
           tray before falling through to the normal case */

        if ( isBlank ) {
            moveTile = blank;
        } else {
            moveTile = tileFace;
        }

        foundAt = model_trayContains( model, playerNum, moveTile );
        if ( foundAt == -1 ) {
            XP_ASSERT( EMPTY_TILE==model_getPlayerTile(model, playerNum, 0));

            (void)model_removePlayerTile( model, playerNum, -1 );
            model_addPlayerTile( model, playerNum, -1, moveTile );
        }

        model_moveTrayToBoard( model, playerNum, col, row, foundAt, tileFace);
    }
} /* model_makeMoveFromStream */

void
model_makeTurnFromMoveInfo( ModelCtxt* model, XP_U16 playerNum, 
                            const MoveInfo* newMove )
{
    XP_U16 col, row, i;
    XP_U16* other;
    const MoveInfoTile* tinfo;
    Tile blank;
    XP_U16 numTiles;

    blank = dict_getBlankTile( model_getDictionary( model ) );
    numTiles = newMove->nTiles;

    col = row = newMove->commonCoord; /* just assign both */
    other = newMove->isHorizontal? &col: &row;

    for ( tinfo = newMove->tiles, i = 0; i < numTiles; ++i, ++tinfo ) {
        XP_S16 tileIndex;
        Tile tile = tinfo->tile;

        if ( IS_BLANK(tile) ) {
            tile = blank;
        }

        tileIndex = model_trayContains( model, playerNum, tile );

        XP_ASSERT( tileIndex >= 0 );

        *other = tinfo->varCoord;
        model_moveTrayToBoard( model, (XP_S16)playerNum, col, row, tileIndex, 
                               (Tile)(tinfo->tile & TILE_VALUE_MASK) );
    }
} /* model_makeTurnFromMoveInfo */

void
model_countAllTrayTiles( ModelCtxt* model, XP_U16* counts, 
                         XP_S16 excludePlayer )
{
    PlayerCtxt* player;
    XP_S16 nPlayers = model->nPlayers;
    XP_S16 i;
    Tile blank;

    blank = dict_getBlankTile( model_getDictionary(model) );

    for ( i = 0, player = model->players; i < nPlayers; ++i, ++player ) {
        if ( i != excludePlayer ) {
            XP_U16 nTiles = player->nPending;
            PendingTile* pt;
            Tile* tiles;

            /* first the pending tiles */
            for ( pt = player->pendingTiles; nTiles--; ++pt ) {
                Tile tile = pt->tile;
                if ( IS_BLANK(tile) ) {
                    tile = blank;
                } else {
                    tile &= TILE_VALUE_MASK;
                }
                XP_ASSERT( tile <= MAX_UNIQUE_TILES );
                ++counts[tile];
            }

            /* then the tiles still in the tray */
            nTiles = player->trayTiles.nTiles;
            tiles = player->trayTiles.tiles;
            while ( nTiles-- ) {
                ++counts[*tiles++];
            }
        }
    }
} /* model_countAllTrayTiles */

XP_S16
model_trayContains( ModelCtxt* model, XP_S16 turn, Tile tile )
{
    PlayerCtxt* player;
    XP_S16 i;
    XP_S16 result = -1;

    XP_ASSERT( turn >= 0 );
    XP_ASSERT( turn < model->nPlayers );

    player = &model->players[turn];

    /* search from top down so don't pull out of below divider */
    for ( i = player->trayTiles.nTiles - 1; i >= 0 ; --i ) {
        Tile playerTile = player->trayTiles.tiles[i];
        if ( playerTile == tile ) {
            result = i;
            break;
        }
    }

    return result;
} /* model_trayContains */

XP_U16
model_getCurrentMoveCount( const ModelCtxt* model, XP_S16 turn )
{
    const PlayerCtxt* player;
    XP_ASSERT( turn >= 0 );
    player = &model->players[turn];
    return player->nPending;
} /* model_getCurrentMoveCount */

void
model_getCurrentMoveTile( ModelCtxt* model, XP_S16 turn, XP_S16* index,
                          Tile* tile, XP_U16* col, XP_U16* row, 
                          XP_Bool* isBlank )
{
    PlayerCtxt* player;
    PendingTile* pt;
    XP_ASSERT( turn >= 0 );

    player = &model->players[turn];
    XP_ASSERT( *index < player->nPending );
    
    if ( *index < 0 ) {
        *index = player->nPending - 1;
    }

    pt = &player->pendingTiles[*index];

    *col = pt->col;
    *row = pt->row;
    *isBlank = (pt->tile & TILE_BLANK_BIT) != 0;
    *tile = pt->tile & TILE_VALUE_MASK;
} /* model_getCurrentMoveTile */

static Tile
removePlayerTile( ModelCtxt* model, XP_S16 turn, XP_S16 index )
{
    PlayerCtxt* player;
    Tile tile;
    short ii;

    XP_ASSERT( turn >= 0 );
    player = &model->players[turn];

    XP_ASSERT( index < player->trayTiles.nTiles );

    tile = player->trayTiles.tiles[index];

    --player->trayTiles.nTiles;
    for ( ii = index; ii < player->trayTiles.nTiles; ++ii ) {
        player->trayTiles.tiles[ii] = player->trayTiles.tiles[ii+1];
    }

    return tile;
} /* removePlayerTile */

Tile
model_removePlayerTile( ModelCtxt* model, XP_S16 turn, XP_S16 index )
{
    Tile tile;
    PlayerCtxt* player;

    XP_ASSERT( turn >= 0 );
    player = &model->players[turn];
    if ( index < 0 ) {
        index = player->trayTiles.nTiles - 1;
    }
    tile = removePlayerTile( model, turn, index );
    notifyTrayListeners( model, turn, index, player->trayTiles.nTiles );
    return tile;
} /* model_removePlayerTile */

void
model_packTilesUtil( ModelCtxt* model, PoolContext* pool,
                     XP_Bool includeBlank, 
                     XP_U16* nUsed, const XP_UCHAR** texts,
                     Tile* tiles )
{
    DictionaryCtxt* dict = model_getDictionary(model);
    XP_U16 nFaces = dict_numTileFaces( dict );
    Tile blankFace = dict_getBlankTile( dict );
    Tile tile;
    XP_U16 nFacesAvail = 0;

    XP_ASSERT( nFaces <= *nUsed );

    for ( tile = 0; tile < nFaces; ++tile ) {
        if ( includeBlank ) {
            XP_ASSERT( !!pool );
            if ( pool_getNTilesLeftFor( pool, tile ) == 0 ) {
                continue;
            }
        } else if ( tile == blankFace ) {
            continue;
        }
            
        tiles[nFacesAvail] = tile;
        texts[nFacesAvail] = dict_getTileString( dict, tile );
        ++nFacesAvail;
    }

    *nUsed = nFacesAvail;

} /* model_packTilesUtil */

static Tile
askBlankTile( ModelCtxt* model, XP_U16 turn )
{
    XP_U16 nUsed = MAX_UNIQUE_TILES;
    XP_S16 chosen;
    const XP_UCHAR* tfaces[MAX_UNIQUE_TILES];
    Tile tiles[MAX_UNIQUE_TILES];
    PickInfo pi;

    pi.why = PICK_FOR_BLANK;
    pi.nTotal = 1;
    pi.thisPick = 1;

    model_packTilesUtil( model, NULL, XP_FALSE,
                         &nUsed, tfaces, tiles );

    chosen = util_userPickTile( model->vol.util, &pi,
                                turn, tfaces, nUsed );

    if ( chosen < 0 ) {
        chosen = 0;
    }
    return tiles[chosen];
} /* askBlankTile */

void
model_moveTrayToBoard( ModelCtxt* model, XP_S16 turn, XP_U16 col, XP_U16 row,
                       XP_S16 tileIndex, Tile blankFace )
{
    PlayerCtxt* player;
    PendingTile* pt;

    Tile tile = model_removePlayerTile( model, turn, tileIndex );

    if ( tile == dict_getBlankTile(model_getDictionary(model)) ) {
        if ( blankFace != EMPTY_TILE ) {
            tile = blankFace;
        } else {
            XP_ASSERT( turn >= 0 );
            tile = askBlankTile( model, (XP_U16)turn );
        }
        tile |= TILE_BLANK_BIT;
    }
    
    player = &model->players[turn];

    if ( player->nPending == 0 ) {
        invalLastMove( model );
    }

    player->nUndone = 0;
    pt = &player->pendingTiles[player->nPending++];
    XP_ASSERT( player->nPending <= MAX_TRAY_TILES );

    pt->tile = tile;
    pt->col = (XP_U8)col;
    pt->row = (XP_U8)row;

    invalidateScore( model, turn );
    incrPendingTileCountAt( model, col, row );

    notifyBoardListeners( model, turn, col, row, XP_TRUE );
} /* model_moveTrayToBoard */

XP_Bool
model_redoPendingTiles( ModelCtxt* model, XP_S16 turn )
{
    XP_U16 actualCnt = 0;
    PlayerCtxt* player;
    XP_U16 nUndone;

    XP_ASSERT( turn >= 0 );

    player = &model->players[turn];
    nUndone = player->nUndone;
    if ( nUndone > 0 ) {
        PendingTile pendingTiles[nUndone];
        PendingTile* pt = pendingTiles;

        XP_MEMCPY( pendingTiles, &player->pendingTiles[player->nPending],
                   nUndone * sizeof(pendingTiles[0]) );

        /* Now we have info about each tile, but don't know where in the
           tray they are.  So find 'em. */
        for ( pt = pendingTiles; nUndone-- > 0; ++pt ) {
            Tile tile = pt->tile;
            XP_Bool isBlank = 0 != (tile & TILE_BLANK_BIT);
            XP_S16 foundAt;

            if ( isBlank ) {
                tile = dict_getBlankTile( model_getDictionary(model) );
            }
            foundAt = model_trayContains( model, turn, tile );
            XP_ASSERT( foundAt >= 0 );

            if ( !model_getTile( model, pt->col, pt->row, XP_FALSE, turn, 
                                 NULL, NULL, NULL, NULL ) ) {
                 model_moveTrayToBoard( model, turn, pt->col, pt->row,
                                        foundAt, pt->tile & ~TILE_BLANK_BIT );
                 ++actualCnt;
            }
        }
    }
    return actualCnt > 0;
}

void
model_moveBoardToTray( ModelCtxt* model, XP_S16 turn, 
                       XP_U16 col, XP_U16 row, XP_U16 trayOffset )
{
    XP_S16 index;
    PlayerCtxt* player;
    short ii;
    PendingTile* pt;
    Tile tile;

    XP_ASSERT( turn >= 0 );
    player = &model->players[turn];
    for ( pt = player->pendingTiles, index = 0; 
          index < player->nPending;
          ++index, ++pt ) {
        if ( pt->col == col && pt->row == row ) {
            break;
        }
    }    

    /* if we're called from putBackOtherPlayersTiles there may be nothing
       here */
    if ( index < player->nPending ) {
        PendingTile tmpPending;
        decrPendingTileCountAt( model, col, row );
        notifyBoardListeners( model, turn, col, row, XP_FALSE );

        tile = pt->tile;
        if ( (tile & TILE_BLANK_BIT) != 0 ) {
            tile = dict_getBlankTile( model_getDictionary(model) );
        }

        model_addPlayerTile( model, turn, trayOffset, tile );

        --player->nPending;
        tmpPending = player->pendingTiles[index];
        for ( ii = index; ii < player->nPending; ++ii ) {
            player->pendingTiles[ii] = player->pendingTiles[ii+1];
        }
        player->pendingTiles[player->nPending] = tmpPending;

        ++player->nUndone;
        //XP_LOGF( "%s: nUndone(%d): %d", __func__, turn, player->nUndone );

        if ( player->nPending == 0 ) {
            invalLastMove( model );
        }

        invalidateScore( model, turn );
    }
} /* model_moveBoardToTray */

XP_Bool
model_moveTileOnBoard( ModelCtxt* model, XP_S16 turn, XP_U16 colCur, 
                       XP_U16 rowCur, XP_U16 colNew, XP_U16 rowNew )
{
    XP_Bool found = XP_FALSE;
    PlayerCtxt* player;
    XP_S16 index;

    XP_ASSERT( turn >= 0 );

    player = &model->players[turn];
    index = player->nPending;

    while ( index-- ) {
        Tile tile;
        XP_U16 tcol, trow;
        XP_Bool isBlank;
        model_getCurrentMoveTile( model, turn, &index, &tile, &tcol, &trow, 
                                  &isBlank );
        if ( colCur == tcol && rowCur == trow ) {
            PendingTile* pt = &player->pendingTiles[index];
            pt->col = colNew;
            pt->row = rowNew;
            if ( isBlank ) {
                pt->tile = TILE_BLANK_BIT | askBlankTile( model, turn );
            }

            decrPendingTileCountAt( model, colCur, rowCur );
            incrPendingTileCountAt( model, colNew, rowNew );

            invalidateScore( model, turn );
            found = XP_TRUE;
            break;
        }
    }
    return found;
} /* model_moveTileOnBoard */

void
model_resetCurrentTurn( ModelCtxt* model, XP_S16 whose )
{
    PlayerCtxt* player;

    XP_ASSERT( whose >= 0 && whose < model->nPlayers );
    player = &model->players[whose];

    while ( player->nPending > 0 ) {
        model_moveBoardToTray( model, whose, 
                               player->pendingTiles[0].col,
                               player->pendingTiles[0].row,
                               -1 );
    }
} /* model_resetCurrentTurn */

XP_S16
model_getNMoves( const ModelCtxt* model )
{
    XP_U16 result = stack_getNEntries( model->vol.stack );
    result -= model->nPlayers;  /* tile assignment doesn't count */
    return result;
}

XP_U16
model_visTileCount( const ModelCtxt* model, XP_U16 turn, XP_Bool trayVisible )
{
    XP_U16 count = model->vol.nTilesOnBoard;
    if ( trayVisible ) {
        count += model_getCurrentMoveCount( model, turn );
    }
    return count;
}

XP_Bool
model_canShuffle( const ModelCtxt* model, XP_U16 turn, XP_Bool trayVisible )
{
    return trayVisible
        && model_getPlayerTiles( model, turn )->nTiles > 1;
}

XP_Bool
model_canTogglePending( const ModelCtxt* model, XP_U16 turn )
{
    const PlayerCtxt* player = &model->players[turn];
    return 0 < player->nPending || 0 < player->nUndone;
}

static void
incrPendingTileCountAt( ModelCtxt* model, XP_U16 col, XP_U16 row )
{
    XP_U16 val = getModelTileRaw( model, col, row );

    if ( TILE_IS_EMPTY(val) ) {
        val = 0;
    } else {
        XP_ASSERT( (val & TILE_PENDING_BIT) != 0 );
        XP_ASSERT( (val & TILE_VALUE_MASK) > 0 );
    }

    ++val;
    XP_ASSERT( (val & TILE_VALUE_MASK) > 0 && 
               (val & TILE_VALUE_MASK) <= MAX_NUM_PLAYERS );
    setModelTileRaw( model, col, row, (CellTile)(val | TILE_PENDING_BIT) );
} /* incrPendingTileCountAt */

static void
setPendingCounts( ModelCtxt* model, XP_S16 turn )
{
    PlayerCtxt* player;
    PendingTile* pending;
    XP_U16 nPending;

    XP_ASSERT( turn >= 0 );
    player = &model->players[turn];
    pending = player->pendingTiles;

    for ( nPending = player->nPending; nPending--; ) {
        incrPendingTileCountAt( model, pending->col, pending->row );
        ++pending;
    }

} /* setPendingCounts */

static void
decrPendingTileCountAt( ModelCtxt* model, XP_U16 col, XP_U16 row )
{
    XP_U16 val = getModelTileRaw( model, col, row );

    /* for pending tiles, the value is defined in the players array, so what
       we keep here is a refcount of how many players have put tiles there. */
    val &= TILE_VALUE_MASK;             /* the refcount */
    XP_ASSERT( val <= MAX_NUM_PLAYERS && val > 0 );
    if ( --val > 0 ) {
        val |= TILE_PENDING_BIT;
    } else {
        val = EMPTY_TILE;
    }
    setModelTileRaw( model, col, row, val );
} /* decrPendingTileCountAt */

static void
putBackOtherPlayersTiles( ModelCtxt* model, XP_U16 notMyTurn, 
                          XP_U16 col, XP_U16 row )
{
    XP_U16 turn;

    for ( turn = 0; turn < model->nPlayers; ++turn ) {
        if ( turn == notMyTurn ) {
            continue;
        }
        model_moveBoardToTray( model, turn, col, row, -1 );
    }
} /* putBackOtherPlayersTiles */

/* Make those tiles placed by 'turn' a permanent part of the board.  If any
 * other players have placed pending tiles on those same squares, replace them
 * in their trays.
 */
static XP_S16
commitTurn( ModelCtxt* model, XP_S16 turn, TrayTileSet* newTiles, 
            XWStreamCtxt* stream, WordNotifierInfo* wni, XP_Bool useStack )
{
    XP_U16 ii;
    PlayerCtxt* player;
    PendingTile* pt;
    XP_S16 score;
    XP_Bool inLine, isHorizontal;
    Tile* newTilesP;
    XP_U16 nTiles;

    nTiles = newTiles->nTiles;

#ifdef DEBUG
    XP_ASSERT( getCurrentMoveScoreIfLegal( model, turn, (XWStreamCtxt*)NULL,
                                           (WordNotifierInfo*)NULL, &score ) );
    invalidateScore( model, turn );
#endif

    XP_ASSERT( turn >= 0 && turn < MAX_NUM_PLAYERS);

    clearLastMoveInfo( model );

    player = &model->players[turn];

    if ( useStack ) {
        MoveInfo moveInfo;
        inLine = tilesInLine( model, turn, &isHorizontal );
        XP_ASSERT( inLine );
        normalizeMoves( model, turn, isHorizontal, &moveInfo );
    
        stack_addMove( model->vol.stack, turn, &moveInfo, newTiles );
    }

    for ( ii = 0, pt=player->pendingTiles; ii < player->nPending; 
          ++ii, ++pt ) {
        XP_U16 col, row;
        CellTile tile;
        XP_U16 val;

        col = pt->col;
        row = pt->row;
        tile = getModelTileRaw( model, col, row );

        XP_ASSERT( (tile & TILE_PENDING_BIT) != 0 );

        val = tile & TILE_VALUE_MASK;
        if ( val > 1 ) { /* somebody else is using this square too! */
            putBackOtherPlayersTiles( model, turn, col, row );
        }

        tile = pt->tile;
        tile |= PREV_MOVE_BIT;
        tile |= turn << CELL_OWNER_OFFSET;

        setModelTileRaw( model, col, row, tile );

        notifyBoardListeners( model, turn, col, row, XP_FALSE );

        ++model->vol.nTilesOnBoard;
    }

    (void)getCurrentMoveScoreIfLegal( model, turn, stream, wni, &score );
    XP_ASSERT( score >= 0 );
    player->score += score;

    /* Why is this next loop necessary? */
    for ( ii = 0; ii < model->nPlayers; ++ii ) {
        invalidateScore( model, ii );
    }

    player->nPending = 0;
    player->nUndone = 0;

    newTilesP = newTiles->tiles;
    while ( nTiles-- ) {
        model_addPlayerTile( model, turn, -1, *newTilesP++ );
    }

    return score;
} /* commitTurn */

void
model_commitTurn( ModelCtxt* model, XP_S16 turn, TrayTileSet* newTiles )
{
    (void)commitTurn( model, turn, newTiles, (XWStreamCtxt*)NULL, 
                      (WordNotifierInfo*)NULL, XP_TRUE );
} /* model_commitTurn */

/* Given a rack of new tiles and of old, remove all the old from the tray and
 * replace them with new.  Replace in the same place so that user sees an
 * in-place change.
 */
static void
makeTileTrade( ModelCtxt* model, XP_S16 player, TrayTileSet* oldTiles, 
               TrayTileSet* newTiles )
{
    XP_U16 i;
    XP_U16 nTiles;

    XP_ASSERT( newTiles->nTiles == oldTiles->nTiles );

    for ( nTiles = newTiles->nTiles, i = 0; i < nTiles; ++i ) {
        Tile oldTile = oldTiles->tiles[i];

        XP_S16 tileIndex = model_trayContains( model, player, oldTile );
        XP_ASSERT( tileIndex >= 0 );
        model_removePlayerTile( model, player, tileIndex );
        model_addPlayerTile( model, player, tileIndex, newTiles->tiles[i] );
    }
} /* makeTileTrade */

void
model_makeTileTrade( ModelCtxt* model, XP_S16 player,
                     TrayTileSet* oldTiles, TrayTileSet* newTiles )
{
    stack_addTrade( model->vol.stack, player, oldTiles, newTiles );

    makeTileTrade( model, player, oldTiles, newTiles );
} /* model_makeTileTrade */

Tile
model_getPlayerTile( ModelCtxt* model, XP_S16 turn, XP_S16 index )
{
    PlayerCtxt* player;
    XP_ASSERT( turn >= 0 );
    player = &model->players[turn];

    if ( index < 0 ) {
        index = player->trayTiles.nTiles-1;
    }

    XP_ASSERT( index < player->trayTiles.nTiles );

    return player->trayTiles.tiles[index];
} /* model_getPlayerTile */

const TrayTileSet*
model_getPlayerTiles( const ModelCtxt* model, XP_S16 turn )
{
    const PlayerCtxt* player;
    XP_ASSERT( turn >= 0 );
    player = &model->players[turn];

    return (const TrayTileSet*)&player->trayTiles;
} /* model_getPlayerTile */

static void
addPlayerTile( ModelCtxt* model, XP_S16 turn, XP_S16 index, Tile tile )
{
    PlayerCtxt* player;
    short ii;

    XP_ASSERT( turn >= 0 );
    player = &model->players[turn];

    XP_ASSERT( player->trayTiles.nTiles < MAX_TRAY_TILES );
    XP_ASSERT( index >= 0 );

    /* move tiles up to make room */
    for ( ii = player->trayTiles.nTiles; ii > index; --ii ) {
        player->trayTiles.tiles[ii] = player->trayTiles.tiles[ii-1];
    }
    ++player->trayTiles.nTiles;
    player->trayTiles.tiles[index] = tile;
} /* addPlayerTile */

void
model_addPlayerTile( ModelCtxt* model, XP_S16 turn, XP_S16 index, Tile tile )
{
    PlayerCtxt* player;

    XP_ASSERT( turn >= 0 );
    player = &model->players[turn];
    if ( index < 0 ) {
        index = player->trayTiles.nTiles;
    }
    addPlayerTile( model, turn, index, tile );

    notifyTrayListeners( model, turn, index, player->trayTiles.nTiles );
} /* model_addPlayerTile */

void
model_moveTileOnTray( ModelCtxt* model, XP_S16 turn, XP_S16 indexCur, 
                      XP_S16 indexNew )
{
    Tile tile = removePlayerTile( model, turn, indexCur );
    addPlayerTile( model, turn, indexNew, tile );
    notifyTrayListeners( model, turn, indexCur, indexNew );
} /* model_moveTileOnTray */

static void
assignPlayerTiles( ModelCtxt* model, XP_S16 turn, const TrayTileSet* tiles )
{
    const Tile* tilep = tiles->tiles;
    XP_U16 nTiles = tiles->nTiles;
    while ( nTiles-- ) {
        model_addPlayerTile( model, turn, -1, *tilep++ );
    }
} /* assignPlayerTiles */

void
model_assignPlayerTiles( ModelCtxt* model, XP_S16 turn, 
                         const TrayTileSet* tiles )
{
    XP_ASSERT( turn >= 0 );
    stack_addAssign( model->vol.stack, turn, tiles );

    assignPlayerTiles( model, turn, tiles );
} /* model_assignPlayerTiles */

void
model_sortTiles( ModelCtxt* model, XP_S16 turn )
{
    PlayerCtxt* player;
    XP_S16 ii;
    TrayTileSet tiles = { .nTiles = 0 };
    XP_S16 nTiles;

    XP_ASSERT( turn >= 0 );
    player = &model->players[turn];

    for ( nTiles = model_getNumTilesInTray( model, turn ) - 1;
          nTiles >= 0; --nTiles ) {
        Tile min = 0xFF;
        XP_U16 minIndex = 0;
        for ( ii = 0; ii <= nTiles; ++ii ) {
            Tile tile = player->trayTiles.tiles[ii];
            if ( tile < min ) {
                min = tile;
                minIndex = ii;
            }
        }
        tiles.tiles[tiles.nTiles++] = 
            removePlayerTile( model, turn, minIndex );
    }

    assignPlayerTiles( model, turn, &tiles );
} /* model_sortTiles */

XP_U16
model_getNumTilesInTray( ModelCtxt* model, XP_S16 turn )
{
    PlayerCtxt* player;
    XP_ASSERT( turn >= 0 );
    player = &model->players[turn];
    return player->trayTiles.nTiles;
} /* model_getNumTilesInTray */

XP_U16
model_getNumTilesTotal( ModelCtxt* model, XP_S16 turn )
{
    PlayerCtxt* player;
    XP_ASSERT( turn >= 0 );
    player = &model->players[turn];
    return player->trayTiles.nTiles + player->nPending;
} /* model_getNumTilesTotal */

XP_U16
model_numRows( const ModelCtxt* model )
{
    return model->nRows;
} /* model_numRows */

XP_U16
model_numCols( const ModelCtxt* model )
{
    return model->nCols;
} /* model_numCols */

void
model_setBoardListener( ModelCtxt* model, BoardListener bl, void* data )
{
    model->vol.boardListenerFunc = bl;
    model->vol.boardListenerData = data;
} /* model_setBoardListener */

void
model_setTrayListener( ModelCtxt* model, TrayListener tl, void* data )
{
    model->vol.trayListenerFunc = tl;
    model->vol.trayListenerData = data;
} /* model_setTrayListener */

void
model_setDictListener( ModelCtxt* model, DictListener dl, void* data )
{
    model->vol.dictListenerFunc = dl;
    model->vol.dictListenerData = data;
} /* model_setDictListener */

static void
notifyBoardListeners( ModelCtxt* model, XP_U16 turn, XP_U16 col, XP_U16 row,
                      XP_Bool added )
{
    if ( model->vol.boardListenerFunc != NULL ) {
        (*model->vol.boardListenerFunc)( model->vol.boardListenerData, turn, 
                                         col, row, added );
    }
} /* notifyBoardListeners */

static void
notifyTrayListeners( ModelCtxt* model, XP_U16 turn, XP_S16 index1, 
                     XP_S16 index2 )
{
    if ( model->vol.trayListenerFunc != NULL ) {
        (*model->vol.trayListenerFunc)( model->vol.trayListenerData, turn, 
                                        index1, index2 );
    }
} /* notifyTrayListeners */

static void
notifyDictListeners( ModelCtxt* model, XP_S16 playerNum,
                     DictionaryCtxt* oldDict, DictionaryCtxt* newDict )
{
    XP_ASSERT( !!newDict );
    if ( model->vol.dictListenerFunc != NULL ) {
        (*model->vol.dictListenerFunc)( model->vol.dictListenerData, playerNum, 
                                        oldDict, newDict );
    }
} /* notifyDictListeners */

static void
printString( XWStreamCtxt* stream, const XP_UCHAR* str )
{
    stream_catString( stream, str );
} /* printString */

static XP_UCHAR*
formatTray( const TrayTileSet* tiles, DictionaryCtxt* dict, XP_UCHAR* buf, 
            XP_U16 bufSize, XP_Bool keepHidden )
{
    if ( keepHidden ) {
        XP_U16 i;
        for ( i = 0; i < tiles->nTiles; ++i ) {
            buf[i] = '?';
        }
        buf[i] = '\0';
    } else {
        dict_tilesToString( dict, (Tile*)tiles->tiles, tiles->nTiles, 
                            buf, bufSize );
    }

    return buf;
} /* formatTray */

typedef struct MovePrintClosure {
    XWStreamCtxt* stream;
    DictionaryCtxt* dict;
    XP_U16 nPrinted;
    XP_Bool keepHidden;
} MovePrintClosure;

static void
printMovePre( ModelCtxt* model, XP_U16 XP_UNUSED(moveN), StackEntry* entry, 
              void* p_closure )
{
    XWStreamCtxt* stream;
    const XP_UCHAR* format;
    XP_UCHAR buf[32];
    XP_UCHAR traybuf[MAX_TRAY_TILES+1];
    MovePrintClosure* closure = (MovePrintClosure*)p_closure;

    if ( entry->moveType == ASSIGN_TYPE ) {
        return;
    }

    stream = closure->stream;

    XP_SNPRINTF( buf, sizeof(buf), (XP_UCHAR*)"%d:%d ", ++closure->nPrinted, 
                 entry->playerNum+1 );
    printString( stream, (XP_UCHAR*)buf );

    if ( entry->moveType == TRADE_TYPE ) {
    } else {
        XP_UCHAR letter[2] = {'\0','\0'};
        XP_Bool isHorizontal = entry->u.move.moveInfo.isHorizontal;
        XP_U16 col, row;
        MoveInfo* mi;
        XP_Bool isPass = XP_FALSE;

        if ( entry->moveType == PHONY_TYPE ) {
            mi = &entry->u.phony.moveInfo;
        } else {
            mi = &entry->u.move.moveInfo;
            if ( mi->nTiles == 0 ) {
                isPass = XP_TRUE;
            }
        }

        if ( isPass ) { 
            format = util_getUserString( model->vol.util, STR_PASS );
            XP_SNPRINTF( buf, VSIZE(buf), "%s", format );
        } else {
            if ( isHorizontal ) {
                format = util_getUserString( model->vol.util, STRS_MOVE_ACROSS );
            } else {
                format = util_getUserString( model->vol.util, STRS_MOVE_DOWN );
            }

            row = mi->commonCoord;
            col = mi->tiles[0].varCoord;
            if ( !isHorizontal ) {
                XP_U16 tmp = col; col = row; row = tmp;
            }
            letter[0] = 'A' + col;

            XP_SNPRINTF( traybuf, sizeof(traybuf), (XP_UCHAR *)"%s%d", 
                         letter, row + 1 );
            XP_SNPRINTF( buf, sizeof(buf), format, traybuf );
        }
        printString( stream, (XP_UCHAR*)buf );
    }

    if ( !closure->keepHidden ) {
        format = util_getUserString( model->vol.util, STRS_TRAY_AT_START );
        formatTray( model_getPlayerTiles( model, entry->playerNum ),
                    closure->dict, (XP_UCHAR*)traybuf, sizeof(traybuf),
                    XP_FALSE );
        XP_SNPRINTF( buf, sizeof(buf), format, traybuf );
        printString( stream, buf );
    }

} /* printMovePre */

static void
printMovePost( ModelCtxt* model, XP_U16 XP_UNUSED(moveN), StackEntry* entry, 
               XP_S16 XP_UNUSED(score), void* p_closure )
{
    MovePrintClosure* closure = (MovePrintClosure*)p_closure;
    XWStreamCtxt* stream = closure->stream;
    DictionaryCtxt* dict = closure->dict;
    const XP_UCHAR* format;
    XP_U16 nTiles;
    XP_S16 totalScore;
    XP_UCHAR buf[100];
    XP_UCHAR traybuf1[MAX_TRAY_TILES+1];
    XP_UCHAR traybuf2[MAX_TRAY_TILES+1];
    MoveInfo* mi;

    if ( entry->moveType == ASSIGN_TYPE ) {
        return;
    }

    totalScore = model_getPlayerScore( model, entry->playerNum );

    switch( entry->moveType ) {
    case TRADE_TYPE:
        formatTray( (const TrayTileSet*)&entry->u.trade.oldTiles, 
                    dict, traybuf1, sizeof(traybuf1), closure->keepHidden );
        formatTray( (const TrayTileSet*) &entry->u.trade.newTiles, 
                    dict, traybuf2, sizeof(traybuf2), closure->keepHidden );

        format = util_getUserString( model->vol.util, STRSS_TRADED_FOR );
        XP_SNPRINTF( buf, sizeof(buf), format, traybuf1, traybuf2 );
        printString( stream, buf );
        printString( stream, (XP_UCHAR*)XP_CR );
        break;

    case PHONY_TYPE:
        format = util_getUserString( model->vol.util, STR_PHONY_REJECTED );
        printString( stream, format );
    case MOVE_TYPE:
        format = util_getUserString( model->vol.util, STRD_CUMULATIVE_SCORE );
        XP_SNPRINTF( buf, sizeof(buf), format, totalScore );
        printString( stream, buf );

        if ( entry->moveType == PHONY_TYPE ) {
            mi = &entry->u.phony.moveInfo;
        } else {
            mi = &entry->u.move.moveInfo;
        }
        nTiles = mi->nTiles;
        if ( nTiles > 0 ) {

            if ( entry->moveType == PHONY_TYPE ) {
                /* printString( stream, (XP_UCHAR*)"phony rejected " ); */
            } else if ( !closure->keepHidden ) {
                format = util_getUserString(model->vol.util, STRS_NEW_TILES);
                XP_SNPRINTF( buf, sizeof(buf), format,
                             formatTray( &entry->u.move.newTiles, dict, 
                                         traybuf1, sizeof(traybuf1), 
                                         XP_FALSE ) );
                printString( stream, buf );
            }
        }

        break;
    }

    printString( stream, (XP_UCHAR*)XP_CR );
} /* printMovePost */

static void
copyStack( ModelCtxt* model, StackCtxt* destStack, const StackCtxt* srcStack )
{
    XWStreamCtxt* stream = mem_stream_make( MPPARM(model->vol.mpool) 
                                            util_getVTManager(model->vol.util),
                                            NULL, 0, NULL );

    stack_writeToStream( (StackCtxt*)srcStack, stream );
    stack_loadFromStream( destStack, stream );

    stream_destroy( stream );
} /* copyStack */

static ModelCtxt*
makeTmpModel( ModelCtxt* model, XWStreamCtxt* stream,
              MovePrintFuncPre mpf_pre, MovePrintFuncPost mpf_post, 
              void* closure )
{
    ModelCtxt* tmpModel = model_make( MPPARM(model->vol.mpool) 
                                      model_getDictionary(model), NULL,
                                      model->vol.util, model_numCols(model),
                                      model_numRows(model));
    model_setNPlayers( tmpModel, model->nPlayers );

    buildModelFromStack( tmpModel, model->vol.stack, XP_FALSE, 0, stream, 
                         (WordNotifierInfo*)NULL, mpf_pre, mpf_post, closure );
    
    return tmpModel;
} /* makeTmpModel */

void
model_writeGameHistory( ModelCtxt* model, XWStreamCtxt* stream,
                        ServerCtxt* server, XP_Bool gameOver )
{
    ModelCtxt* tmpModel;
    MovePrintClosure closure;

    closure.stream = stream;
    closure.dict = model_getDictionary( model );
    closure.keepHidden = !gameOver;
    closure.nPrinted = 0;

    tmpModel = makeTmpModel( model, stream, printMovePre, printMovePost, 
                             &closure );
    model_destroy( tmpModel );

    if ( gameOver ) {
        /* if the game's over, it shouldn't matter which model I pass to this
           method */
        server_writeFinalScores( server, stream ); 
    }
} /* model_writeGameHistory */

typedef struct _FirstWordData {
    XP_UCHAR word[32];
} FirstWordData;

static XP_Bool
getFirstWord( const XP_UCHAR* word, XP_Bool isLegal, void* closure )
{
    LOG_FUNC();
    if ( isLegal ) {
        FirstWordData* data = (FirstWordData*)closure;
        if ( '\0' == data->word[0] && '\0' != word[0] ) {
            XP_STRCAT( data->word, word );
        }
    }
    return XP_TRUE;
}

static void
scoreLastMove( ModelCtxt* model, MoveInfo* moveInfo, XP_U16 howMany, 
               XP_UCHAR* buf, XP_U16* bufLen )
{

    if ( moveInfo->nTiles == 0 ) {
        const XP_UCHAR* str = util_getUserString( model->vol.util, STR_PASSED );
        XP_U16 len = XP_STRLEN( str );
        *bufLen = len;
        XP_STRNCPY( buf, str, len + 1 );
    } else {
        XP_U16 score;
        const XP_UCHAR* format;
        WordNotifierInfo notifyInfo;
        FirstWordData data;

        ModelCtxt* tmpModel = makeTmpModel( model, NULL, NULL, NULL, NULL );
        XP_U16 turn;
        XP_S16 moveNum = -1;
        
        copyStack( model, tmpModel->vol.stack, model->vol.stack );

        if ( !model_undoLatestMoves( tmpModel, NULL, howMany, &turn, 
                                     &moveNum ) ) {
            XP_ASSERT( 0 );
        }

        data.word[0] = '\0';
        notifyInfo.proc = getFirstWord;
        notifyInfo.closure = &data;
        score = figureMoveScore( tmpModel, turn, moveInfo, (EngineCtxt*)NULL, 
                                 (XWStreamCtxt*)NULL, &notifyInfo );

        model_destroy( tmpModel );

        format = util_getUserString( model->vol.util, STRSD_SUMMARYSCORED );
        *bufLen = XP_SNPRINTF( buf, *bufLen, format, data.word, score );
    }
} /* scoreLastMove */

static XP_U16
model_getRecentPassCount( ModelCtxt* model )
{
    StackCtxt* stack = model->vol.stack;
    XP_U16 nPasses = 0;
    XP_S16 nEntries, which;
    StackEntry entry;

    XP_ASSERT( !!stack );

    nEntries = stack_getNEntries( stack );
    for ( which = nEntries - 1; which >= 0; --which ) {
        if ( stack_getNthEntry( stack, which, &entry ) ) {
            if ( entry.moveType == MOVE_TYPE
                    && entry.u.move.moveInfo.nTiles == 0 ) {
                ++nPasses;
            } else {
                break;
            }
        } else {
            break;
        }
    }
    return nPasses;
} /* model_getRecentPassCount */

XP_Bool
model_recentPassCountOk( ModelCtxt* model )
{
    XP_U16 count = model_getRecentPassCount( model );
    XP_U16 okCount = model->nPlayers * MAX_PASSES;
    XP_ASSERT( count <= okCount ); /* should never be more than 1 over */
    return count < okCount;
}

static XP_Bool
recordWord( const XP_UCHAR* word, XP_Bool isLegal, void* closure )
{
    RecordWordsInfo* info = (RecordWordsInfo*)closure;
    XWStreamCtxt* stream = info->stream;
    XP_LOGF( "%s(%s)", __func__, word );
    if ( 0 < info->nWords++ ) {
        stream_putU8( stream, '\n' );
    }
    stream_catString( stream, word );
    return XP_TRUE;
}

WordNotifierInfo* 
model_initWordCounter( ModelCtxt* model, XWStreamCtxt* stream )
{
    XP_ASSERT( model->vol.wni.proc == recordWord );
    XP_ASSERT( model->vol.wni.closure == &model->vol.rwi );
    model->vol.rwi.stream = stream;
    model->vol.rwi.nWords = 0;
    return &model->vol.wni;
}

void
model_getWordsPlayed( ModelCtxt* model, XP_U16 nTurns, XWStreamCtxt* stream )
{
    XP_ASSERT( !!stream );
    StackCtxt* stack = model->vol.stack;
    StackCtxt* tmpStack = stack_copy( stack );

    XP_U16 nPlayers = model->nPlayers;
    XP_U16 nEntries = stack_getNEntries( stack );
    nEntries -= nPlayers; /* skip assignments */
    if ( nTurns > nEntries ) {
        nTurns = nEntries;
    }

    if ( model_undoLatestMoves( model, NULL, nTurns, NULL, NULL ) ) {
        WordNotifierInfo* ni = model_initWordCounter( model, stream );
        /* Now push the undone moves back into the model one at a time.
           recordWord() will add each played word to the stream as it's
           scored */
        buildModelFromStack( model, tmpStack, XP_TRUE, 
                             nEntries - nTurns + nPlayers,/* skip assignments */
                             (XWStreamCtxt*)NULL, ni, (MovePrintFuncPre)NULL, 
                             (MovePrintFuncPost)NULL, NULL );
    }
    stack_destroy( tmpStack );
}

XP_Bool
model_getPlayersLastScore( ModelCtxt* model, XP_S16 player,
                           XP_UCHAR* expl, XP_U16* explLen )
{
    StackCtxt* stack = model->vol.stack;
    XP_S16 nEntries, which;
    StackEntry entry;
    XP_Bool found = XP_FALSE;

    XP_ASSERT( !!stack );
    XP_ASSERT( player >= 0 );

    nEntries = stack_getNEntries( stack );

    for ( which = nEntries; which >= 0; ) {
        if ( stack_getNthEntry( stack, --which, &entry ) ) {
            if ( entry.playerNum == player ) {
                found = XP_TRUE;
                break;
            }
        }
    }

    if ( found ) { /* success? */
        const XP_UCHAR* format;
        XP_U16 nTiles;
        switch ( entry.moveType ) {
        case MOVE_TYPE:
            scoreLastMove( model, &entry.u.move.moveInfo, 
                           nEntries - which - 1, expl, explLen );
            break;
        case TRADE_TYPE:
            nTiles = entry.u.trade.oldTiles.nTiles;
            format = util_getUserString( model->vol.util, STRD_TRADED );
            *explLen = XP_SNPRINTF( expl, *explLen, format, nTiles );
            break;
        case PHONY_TYPE:
            format = util_getUserString( model->vol.util, STR_LOSTTURN );
            *explLen = XP_STRLEN( format );
            XP_STRCAT( expl, format );
            break;
        case ASSIGN_TYPE:
            found = XP_FALSE;
            break;
        }
    }

    return found;
} /* model_getPlayersLastScore */

static void
loadPlayerCtxt( XWStreamCtxt* stream, XP_U16 version, PlayerCtxt* pc )
{
    PendingTile* pt;
    XP_U16 nTiles;

    pc->curMoveValid = stream_getBits( stream, 1 );

    traySetFromStream( stream, &pc->trayTiles );
    
    pc->nPending = (XP_U8)stream_getBits( stream, NTILES_NBITS );
    if ( STREAM_VERS_NUNDONE <= version ) {
        pc->nUndone = (XP_U8)stream_getBits( stream, NTILES_NBITS );
    } else {
        XP_ASSERT( 0 == pc->nUndone );
    }

    nTiles = pc->nPending + pc->nUndone;
    for ( pt = pc->pendingTiles; nTiles-- > 0; ++pt ) {
        XP_U16 nBits;
        pt->col = (XP_U8)stream_getBits( stream, NUMCOLS_NBITS );
        pt->row = (XP_U8)stream_getBits( stream, NUMCOLS_NBITS );

        nBits = (version <= STREAM_VERS_RELAY) ? 6 : 7;
        pt->tile = (Tile)stream_getBits( stream, nBits );
    }

} /* loadPlayerCtxt */

static void
writePlayerCtxt( XWStreamCtxt* stream, PlayerCtxt* pc )
{
    XP_U16 nTiles;
    PendingTile* pt;

    stream_putBits( stream, 1, pc->curMoveValid );

    traySetToStream( stream, &pc->trayTiles );
    
    stream_putBits( stream, NTILES_NBITS, pc->nPending );
    stream_putBits( stream, NTILES_NBITS, pc->nUndone );

    nTiles = pc->nPending + pc->nUndone;
    for ( pt = pc->pendingTiles; nTiles-- > 0; ++pt ) {
        stream_putBits( stream, NUMCOLS_NBITS, pt->col );
        stream_putBits( stream, NUMCOLS_NBITS, pt->row );
        stream_putBits( stream, 7, pt->tile );
    }
} /* writePlayerCtxt */

#ifdef CPLUS
}
#endif
