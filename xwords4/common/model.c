/* -*- compile-command: "cd ../linux && make -j3 MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 2000 - 2022 by Eric House (xwords@eehouse.org).  All rights
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

/* #include <assert.h> */

#include "comtypes.h"
#include "modelp.h"
#include "xwstream.h"
#include "util.h"
#include "pool.h"
#include "game.h"
#include "dbgutil.h"
#include "memstream.h"
#include "strutils.h"
#include "LocalizedStrIncludes.h"

#ifdef CPLUS
extern "C" {
#endif

#define MAX_PASSES 2 /* how many times can all players pass? */

/****************************** prototypes ******************************/
typedef void (*MovePrintFuncPre)(ModelCtxt*, XWEnv, XP_U16, const StackEntry*, void*);
typedef void (*MovePrintFuncPost)(ModelCtxt*, XWEnv, XP_U16, const StackEntry*,
                                  XP_S16, void*);

static void incrPendingTileCountAt( ModelCtxt* model, XP_U16 col, 
                                    XP_U16 row );
static void decrPendingTileCountAt( ModelCtxt* model, XP_U16 col, 
                                    XP_U16 row );
static void notifyBoardListeners( ModelCtxt* model, XWEnv xwe, XP_U16 turn,
                                  XP_U16 col, XP_U16 row, XP_Bool added );
static void notifyTrayListeners( ModelCtxt* model, XP_U16 turn, 
                                 XP_S16 index1, XP_S16 index2 );
static void notifyDictListeners( ModelCtxt* model, XWEnv xwe, XP_S16 playerNum,
                                 const DictionaryCtxt* oldDict,
                                 const DictionaryCtxt* newDict );
static void model_unrefDicts( ModelCtxt* model, XWEnv xwe );

static CellTile getModelTileRaw( const ModelCtxt* model, XP_U16 col, 
                                 XP_U16 row );
static void setModelTileRaw( ModelCtxt* model, XP_U16 col, XP_U16 row, 
                             CellTile tile );
static void makeTileTrade( ModelCtxt* model, XP_S16 player, 
                           const TrayTileSet* oldTiles, 
                           const TrayTileSet* newTiles );
static XP_S16 commitTurn( ModelCtxt* model, XWEnv xwe, XP_S16 turn,
                          const TrayTileSet* newTiles, XWStreamCtxt* stream, 
                          WordNotifierInfo* wni, XP_Bool useStack );
static void buildModelFromStack( ModelCtxt* model, XWEnv xwe, StackCtxt* stack,
                                 XP_Bool useStack, XP_U16 initial, 
                                 XWStreamCtxt* stream, WordNotifierInfo* wni, 
                                 MovePrintFuncPre mpfpr, 
                                 MovePrintFuncPost mpfpo, void* closure );
static void setPendingCounts( ModelCtxt* model, XP_S16 turn );
static XP_S16 setContains( const TrayTileSet* tiles, Tile tile );
static void loadPlayerCtxt( const ModelCtxt* model, XWStreamCtxt* stream, 
                            XP_U16 version, PlayerCtxt* pc );
static void writePlayerCtxt( const ModelCtxt* model, XWStreamCtxt* stream, 
                             const PlayerCtxt* pc );
static XP_U16 model_getRecentPassCount( ModelCtxt* model );
static void recordWord( const WNParams* wnp, void *closure );
#ifdef DEBUG 
typedef struct _DiffTurnState {
    XP_S16 lastPlayerNum;
    XP_S16 lastMoveNum;
} DiffTurnState;
static void assertDiffTurn( ModelCtxt* model, XWEnv xwe, XP_U16 turn,
                            const StackEntry* entry, void* closure);
#endif

/*****************************************************************************
 *
 ****************************************************************************/
ModelCtxt*
model_make( MPFORMAL XWEnv xwe, const DictionaryCtxt* dict,
            const PlayerDicts* dicts, XW_UtilCtxt* util, XP_U16 nCols )
{
    ModelCtxt* result = (ModelCtxt*)XP_MALLOC( mpool, sizeof( *result ) );
    if ( result != NULL ) {
        XP_MEMSET( result, 0, sizeof(*result) );
        MPASSIGN(result->vol.mpool, mpool);

        result->vol.util = util;
        result->vol.dutil = util_getDevUtilCtxt( util, xwe );
        result->vol.wni.proc = recordWord;
        result->vol.wni.closure = &result->vol.rwi;

        XP_ASSERT( !!util->gameInfo );
        result->vol.gi = util->gameInfo;

        model_setSize( result, nCols );

        model_setDictionary( result, xwe, dict );
        model_setPlayerDicts( result, xwe, dicts );
    }

    return result;
} /* model_make */

ModelCtxt* 
model_makeFromStream( MPFORMAL XWEnv xwe, XWStreamCtxt* stream,
                      const DictionaryCtxt* dict, const PlayerDicts* dicts,
                      XW_UtilCtxt* util )
{
    ModelCtxt* model;
    XP_U16 nCols;
    XP_U16 version = stream_getVersion( stream );

    XP_ASSERT( !!dict || !!dicts );

    if ( 0 ) {
#ifdef STREAM_VERS_BIGBOARD
    } else if ( STREAM_VERS_BIGBOARD <= version ) {
        nCols = (XP_U16)stream_getBits( stream, NUMCOLS_NBITS_5 );
#endif
    } else {
        nCols = (XP_U16)stream_getBits( stream, NUMCOLS_NBITS_4 );
        (void)stream_getBits( stream, NUMCOLS_NBITS_4 );
    }
    XP_ASSERT( MAX_COLS >= nCols );

    XP_U16 nPlayers = (XP_U16)stream_getBits( stream, NPLAYERS_NBITS );

    model = model_make( MPPARM(mpool) xwe, dict, dicts, util, nCols );
    model->nPlayers = nPlayers;

#ifdef STREAM_VERS_BIGBOARD
    if ( STREAM_VERS_BIGBOARD <= version ) {
        model->vol.nBonuses = stream_getBits( stream, 7 );
        if ( 0 < model->vol.nBonuses ) {
            model->vol.bonuses = 
                XP_MALLOC( model->vol.mpool,
                           model->vol.nBonuses * sizeof( model->vol.bonuses[0] ) );
            for ( int ii = 0; ii < model->vol.nBonuses; ++ii ) {
                model->vol.bonuses[ii] = stream_getBits( stream, 4 );
            }
        }
    }
#endif

    stack_loadFromStream( model->vol.stack, stream );

    MovePrintFuncPre pre = NULL;
    void* closure = NULL;
#ifdef DEBUG
    pre = assertDiffTurn;
    DiffTurnState state = { -1, -1 };
    closure = &state;
#endif

    buildModelFromStack( model, xwe, model->vol.stack, XP_FALSE, 0,
                         (XWStreamCtxt*)NULL, (WordNotifierInfo*)NULL,
                         pre, (MovePrintFuncPost)NULL, closure );

    for ( int ii = 0; ii < model->nPlayers; ++ii ) {
        loadPlayerCtxt( model, stream, version, &model->players[ii] );
        setPendingCounts( model, ii );
        invalidateScore( model, ii );
    }

    return model;
} /* model_makeFromStream */

void
model_writeToStream( const ModelCtxt* model, XWStreamCtxt* stream )
{
    XP_U16 ii;
#ifdef STREAM_VERS_BIGBOARD
    XP_ASSERT( STREAM_VERS_BIGBOARD <= stream_getVersion( stream ) );
    stream_putBits( stream, NUMCOLS_NBITS_5, model->nCols );
#else
    stream_putBits( stream, NUMCOLS_NBITS_4, model->nCols );
    stream_putBits( stream, NUMCOLS_NBITS_4, model->nRows );
#endif

    /* we have two bits for nPlayers, so range must be 0..3, not 1..4 */
    stream_putBits( stream, NPLAYERS_NBITS, model->nPlayers );

#ifdef STREAM_VERS_BIGBOARD
    stream_putBits( stream, 7, model->vol.nBonuses );
    for ( ii = 0; ii < model->vol.nBonuses; ++ii ) {
        stream_putBits( stream, 4, model->vol.bonuses[ii] );
    }
#endif

    stack_writeToStream( model->vol.stack, stream );

    for ( ii = 0; ii < model->nPlayers; ++ii ) {
        writePlayerCtxt( model, stream, &model->players[ii] );
    }
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
model_setSize( ModelCtxt* model, XP_U16 nCols )
{
    ModelVolatiles saveVol = model->vol; /* save vol so we don't wipe it out */
    XP_U16 oldSize = model->nCols;   /* zero when called from model_make() */

    XP_ASSERT( MAX_COLS >= nCols );
    XP_ASSERT( model != NULL );
    XP_MEMSET( model, 0, sizeof( *model ) );

    model->nCols = nCols;
    model->nRows = nCols;
    model->vol = saveVol;

    ModelVolatiles* vol = &model->vol;
    if ( oldSize != nCols ) {
        if ( !!vol->tiles ) {
            XP_FREE( vol->mpool, vol->tiles );
        }
        vol->tiles = XP_MALLOC( vol->mpool, TILES_SIZE(model, nCols) );
    }
    XP_MEMSET( vol->tiles, TILE_EMPTY_BIT, TILES_SIZE(model, nCols) );

    if ( !!vol->stack ) {
        stack_init( vol->stack, vol->gi->nPlayers, vol->gi->inDuplicateMode );
    } else {
        vol->stack = stack_make( MPPARM(vol->mpool)
                                 dutil_getVTManager(vol->dutil),
                                 vol->gi->nPlayers, vol->gi->inDuplicateMode );
    }
} /* model_setSize */

void
model_forceStack7Tiles( ModelCtxt* model )
{
    stack_set7Tiles( model->vol.stack );
}

void
model_destroy( ModelCtxt* model, XWEnv xwe )
{
    model_unrefDicts( model, xwe );
    stack_destroy( model->vol.stack );
    /* is this it!? */
    if ( !!model->vol.bonuses ) {
        XP_FREE( model->vol.mpool, model->vol.bonuses );
    }
    XP_FREE( model->vol.mpool, model->vol.tiles );
    XP_FREE( model->vol.mpool, model );
} /* model_destroy */

XP_U32
model_getHash( const ModelCtxt* model )
{
#ifndef STREAM_VERS_HASHSTREAM
    XP_USE(version);
#endif
    StackCtxt* stack = model->vol.stack;
    XP_ASSERT( !!stack );
    return stack_getHash( stack );
}

XP_Bool
model_hashMatches( const ModelCtxt* model, const XP_U32 hash )
{
    StackCtxt* stack = model->vol.stack;
    XP_Bool matches = hash == stack_getHash( stack );
    XP_LOGFF( "(hash=%X) => %s", hash, boolToStr(matches) );
    return matches;
}

XP_Bool
model_popToHash( ModelCtxt* model, XWEnv xwe, const XP_U32 hash, PoolContext* pool )
{
    LOG_FUNC();
    XP_U16 nPopped = 0;
    StackCtxt* stack = model->vol.stack;
    const XP_U16 nEntries = stack_getNEntries( stack );
    StackEntry entries[nEntries];
    XP_S16 foundAt = -1;

    for ( XP_U16 ii = 0; ii < nEntries; ++ii ) {
        XP_U32 hash1 = stack_getHash( stack );
        XP_LOGFF( "comparing %X with entry #%d %X", hash, nEntries - ii, hash1 );
        if ( hash == hash1 ) {
            foundAt = ii;
            break;
        }

        if ( ! stack_popEntry( stack, &entries[ii] ) ) {
            XP_LOGFF( "stack_popEntry(%d) failed", ii );
            XP_ASSERT(0);
            break;
        }
        ++nPopped;
    }

    for ( XP_S16 ii = nPopped - 1; ii >= 0; --ii ) {
        stack_redo( stack, &entries[ii] );
        stack_freeEntry( stack, &entries[ii] );
    }

    XP_Bool found = -1 != foundAt;
    if ( found ) {
        if ( 0 < foundAt ) {    /* if 0, nothing to do */
            XP_LOGFF( "undoing %d turns to match hash %X",
                      foundAt, hash );
#ifdef DEBUG
            XP_Bool success =
#endif
                model_undoLatestMoves( model, xwe, pool, foundAt, NULL, NULL );
            XP_ASSERT( success );
        }
        /* Assert not needed for long */
        XP_ASSERT( hash == stack_getHash( model->vol.stack ) );
    } else {
        XP_ASSERT( nEntries == stack_getNEntries(stack) );
    }

    LOG_RETURNF( "%s (hash=%X, nEntries=%d)", boolToStr(found), hash, nEntries );
    return found;
} /* model_popToHash */

#ifdef STREAM_VERS_BIGBOARD
void
model_setSquareBonuses( ModelCtxt* XP_UNUSED(model), XWBonusType* XP_UNUSED(bonuses),
                        XP_U16 XP_UNUSED(nBonuses) )
{
    XP_LOGFF( "doing nothing" );
}
#endif

#define EM BONUS_NONE
#define DL BONUS_DOUBLE_LETTER
#define DW BONUS_DOUBLE_WORD
#define TL BONUS_TRIPLE_LETTER
#define TW BONUS_TRIPLE_WORD
#define QL BONUS_QUAD_LETTER
#define QW BONUS_QUAD_WORD

static XWBonusType sTwentyOne[] = {
    QW,
    EM, DW,
    EM, EM, DW,
    DL, EM, EM, TW,
    EM, TL, EM, EM, DW,
    EM, EM, QL, EM, EM, DW,
    EM, EM, EM, DL, EM, EM, DW,
    TW, EM, EM, EM, EM, EM, EM, DW,
    EM, DW, EM, EM, TL, EM, EM, EM, TL,
    EM, EM, DW, EM, EM, DL, EM, EM, EM, DL,
    DL, EM, EM, TW, EM, EM, DL, EM, EM, EM, DW,
}; /* sTwentyOne */

static XWBonusType
getSquareBonus( XP_U16 nCols, XP_U16 col, XP_U16 row )
{
    if ( col > (nCols/2) ) {
        col = nCols - 1 - col;
    }
    if ( row > (nCols/2) ) {
        row = nCols - 1 - row;
    }
    if ( col > row ) {
        XP_U16 tmp = col;
        col = row;
        row = tmp;
    }

    /* For a smaller board, skip the outer "rings." For larger,
       outer rings are empty */
    XWBonusType result = BONUS_NONE;
    XP_U16 adj = (21 - nCols) / 2;
    if ( 0 <= adj ) {
        col += adj;
        row += adj;

        if ( col <= 21 && row <= 21 ) {
            XP_U16 index = col;
            for ( XP_U16 ii = 1; ii <= row; ++ii ) {
                index += ii;
            }

            if ( index < VSIZE(sTwentyOne)) {
                result = sTwentyOne[index];
            }
        }
    }
    return result;
}

XWBonusType
model_getSquareBonus( const ModelCtxt* model, XP_U16 col, XP_U16 row )
{
    XWBonusType result = BONUS_NONE;
#ifdef STREAM_VERS_BIGBOARD
    const ModelCtxt* bonusOwner = model->loaner? model->loaner : model;
#endif

    if ( 0 ) {
#ifdef STREAM_VERS_BIGBOARD
    } else if ( !!bonusOwner->vol.bonuses ) {
        XP_U16 nCols = model_numCols( model );
        XP_U16 ii;
        if ( col > (nCols/2) ) {
            col = nCols - 1 - col;
        }
        if ( row > (nCols/2) ) {
            row = nCols - 1 - row;
        }
        if ( col > row ) {
            XP_U16 tmp = col;
            col = row;
            row = tmp;
        }
        for ( ii = 1; ii <= row; ++ii ) {
            col += ii;
        }

        if ( col < bonusOwner->vol.nBonuses ) {
            result = bonusOwner->vol.bonuses[col];
        }
#endif
    } else {
        result = getSquareBonus( model_numRows(model), col, row );
    }
    return result;
}

static XP_U16
makeAndCommit( ModelCtxt* model, XWEnv xwe, XP_U16 turn, const MoveInfo* mi,
               const TrayTileSet* tiles, XWStreamCtxt* stream,
               XP_Bool useStack, WordNotifierInfo* wni )
{
    model_makeTurnFromMoveInfo( model, xwe, turn, mi );
    XP_U16 moveScore = commitTurn( model, xwe, turn, tiles,
                                   stream, wni, useStack );
    return moveScore;
}

static void
dupe_adjustScores( ModelCtxt* model, XP_Bool add, XP_U16 nScores, const XP_U16* scores )
{
    XP_S16 mult = add ? 1 : -1;
    for ( XP_U16 ii = 0; ii < nScores; ++ii ) {
        model->players[ii].score += mult * scores[ii];
    }
}

void
model_cloneDupeTrays( ModelCtxt* model, XWEnv xwe )
{
    XP_ASSERT( model->vol.gi->inDuplicateMode );
    XP_U16 nTiles = model->players[DUP_PLAYER].trayTiles.nTiles;
    for ( XP_U16 ii = 0; ii < model->nPlayers; ++ii ) {
        if ( ii != DUP_PLAYER ) {
            model_resetCurrentTurn( model, xwe, ii );
            model->players[ii].trayTiles = model->players[DUP_PLAYER].trayTiles;
            notifyTrayListeners( model, ii, 0, nTiles );
        }
    }
}

static void
modelAddEntry( ModelCtxt* model, XWEnv xwe, XP_U16 indx, const StackEntry* entry,
               XP_Bool useStack, XWStreamCtxt* stream, 
               WordNotifierInfo* wni, MovePrintFuncPre mpf_pre,
               MovePrintFuncPost mpf_post, void* closure )
{
    XP_S16 moveScore = 0; /* keep compiler happy */
    if ( !!mpf_pre ) {
        (*mpf_pre)( model, xwe, indx, entry, closure );
    }

    switch ( entry->moveType ) {
    case MOVE_TYPE:
        moveScore = makeAndCommit( model, xwe, entry->playerNum, &entry->u.move.moveInfo,
                                   &entry->u.move.newTiles, stream, useStack, wni );
        if ( model->vol.gi->inDuplicateMode ) {
            XP_ASSERT( DUP_PLAYER == entry->playerNum );
            dupe_adjustScores( model, XP_TRUE, entry->u.move.dup.nScores,
                               entry->u.move.dup.scores );
            model_cloneDupeTrays( model, xwe );
        }
        break;
    case TRADE_TYPE:
        makeTileTrade( model, entry->playerNum, &entry->u.trade.oldTiles,
                       &entry->u.trade.newTiles );
        if ( model->vol.gi->inDuplicateMode ) {
            XP_ASSERT( DUP_PLAYER == entry->playerNum );
            model_cloneDupeTrays( model, xwe );
        }
        break;
    case ASSIGN_TYPE:
        model_addNewTiles( model, entry->playerNum, &entry->u.assign.tiles );
        if ( model->vol.gi->inDuplicateMode ) {
            XP_ASSERT( DUP_PLAYER == entry->playerNum );
            model_cloneDupeTrays( model, xwe );
        }
        break;
    case PHONY_TYPE: /* nothing to add */
        model_makeTurnFromMoveInfo( model, xwe, entry->playerNum,
                                    &entry->u.phony.moveInfo);
        /* do something here to cause it to print */
        (void)getCurrentMoveScoreIfLegal( model, xwe, entry->playerNum, stream,
                                          wni, &moveScore );
        moveScore = 0;
        model_resetCurrentTurn( model, xwe, entry->playerNum );

        break;
    case PAUSE_TYPE:
        // XP_LOGF( "%s(): nothing to do with PAUSE_TYPE", __func__ );
        break;
    default:
        XP_ASSERT(0);
    }

    if ( !!mpf_post ) {
        (*mpf_post)( model, xwe, indx, entry, moveScore, closure );
    }
} /* modelAddEntry */

static void
buildModelFromStack( ModelCtxt* model, XWEnv xwe, StackCtxt* stack, XP_Bool useStack,
                     XP_U16 initial, XWStreamCtxt* stream, 
                     WordNotifierInfo* wni, MovePrintFuncPre mpf_pre, 
                     MovePrintFuncPost mpf_post, void* closure )
{
    StackEntry entry;
    for ( XP_U16 ii = initial; stack_getNthEntry( stack, ii, &entry ); ++ii ) {
        modelAddEntry( model, xwe, ii, &entry, useStack, stream, wni,
                       mpf_pre, mpf_post, closure );
        stack_freeEntry( stack, &entry );
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
model_setDictionary( ModelCtxt* model, XWEnv xwe, const DictionaryCtxt* dict )
{
    const DictionaryCtxt* oldDict = model->vol.dict;
    model->vol.dict = dict_ref( dict, xwe );
    
    if ( !!dict ) {
        setStackBits( model, dict );
    }

    notifyDictListeners( model, xwe, -1, oldDict, dict );
    dict_unref( oldDict, xwe );
} /* model_setDictionary */

void
model_setPlayerDicts( ModelCtxt* model, XWEnv xwe, const PlayerDicts* dicts )
{
    if ( !!dicts ) {
        XP_U16 ii;
#ifdef DEBUG
        const DictionaryCtxt* gameDict = model_getDictionary( model );
#endif
        for ( ii = 0; ii < VSIZE(dicts->dicts); ++ii ) {
            const DictionaryCtxt* oldDict = model->vol.dicts.dicts[ii];
            const DictionaryCtxt* newDict = dicts->dicts[ii];
            if ( oldDict != newDict ) {
                XP_ASSERT( NULL == newDict || NULL == gameDict 
                           || dict_tilesAreSame( gameDict, newDict ) );
                model->vol.dicts.dicts[ii] = dict_ref( newDict, xwe );

                notifyDictListeners( model, xwe, ii, oldDict, newDict );
                setStackBits( model, newDict );

                dict_unref( oldDict, xwe );
            }
        }
    }
}           

const DictionaryCtxt*
model_getDictionary( const ModelCtxt* model )
{
    XP_U16 ii;
    const DictionaryCtxt* result = model->vol.dict;
    for ( ii = 0; !result && ii < VSIZE(model->vol.dicts.dicts); ++ii ) {
        result = model->vol.dicts.dicts[ii];
    }
    return result;
} /* model_getDictionary */

const DictionaryCtxt*
model_getPlayerDict( const ModelCtxt* model, XP_S16 playerNum )
{
    const DictionaryCtxt* dict = NULL;
    if ( 0 <= playerNum && playerNum < VSIZE(model->vol.dicts.dicts) ) {
        dict = model->vol.dicts.dicts[playerNum];
    }
    if ( NULL == dict ) {
        dict = model->vol.dict;
    }
    XP_ASSERT( !!dict );
    return dict;
}

static void
model_unrefDicts( ModelCtxt* model, XWEnv xwe )
{
    XP_U16 ii;
    for ( ii = 0; ii < VSIZE(model->vol.dicts.dicts); ++ii ) {
        dict_unref( model->vol.dicts.dicts[ii], xwe );
        model->vol.dicts.dicts[ii] = NULL;
    }
    dict_unref( model->vol.dict, xwe );
    model->vol.dict = NULL;
}

static XP_Bool
getPendingTileFor( const ModelCtxt* model, XP_U16 turn, XP_U16 col, XP_U16 row,
                   CellTile* cellTile )
{
    XP_Bool found = XP_FALSE;
    const PlayerCtxt* player;
    const PendingTile* pendings;
    XP_U16 ii;

    XP_ASSERT( turn < VSIZE(model->players) );

    player = &model->players[turn];
    pendings = player->pendingTiles;
    for ( ii = 0; ii < player->nPending; ++ii ) {

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
    XP_Bool inUse = XP_TRUE;
    
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
    inUse = 0 == (cellTile & TILE_EMPTY_BIT);
    if ( inUse ) {
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
    }
    return inUse;
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
model_foreachPrevCell( ModelCtxt* model, XWEnv xwe, BoardListener bl, void* closure )
{
    XP_U16 col, row;

    for ( col = 0; col < model->nCols; ++col ) {
        for ( row = 0; row < model->nRows; ++row) {
            CellTile tile = getModelTileRaw( model, col, row );
            if ( (tile & PREV_MOVE_BIT) != 0 ) {
                (*bl)( xwe, closure, (XP_U16)CELL_OWNER(tile), col, row, XP_FALSE );
            }
        }
    }
} /* model_foreachPrevCell */

static void
clearAndNotify( XWEnv xwe, void* closure, XP_U16 XP_UNUSED(turn),
                XP_U16 col, XP_U16 row, XP_Bool XP_UNUSED(added) )
{
    ModelCtxt* model = (ModelCtxt*)closure;
    CellTile tile = getModelTileRaw( model, col, row );
    setModelTileRaw( model, col, row, (CellTile)(tile & ~PREV_MOVE_BIT) );
    
    notifyBoardListeners( model, xwe, (XP_U16)CELL_OWNER(tile), col, row,
                          XP_FALSE );
} /* clearAndNotify */

static void
clearLastMoveInfo( ModelCtxt* model, XWEnv xwe )
{
    model_foreachPrevCell( model, xwe, clearAndNotify, model );
} /* clearLastMoveInfo */

static void
invalLastMove( ModelCtxt* model, XWEnv xwe )
{
    if ( !!model->vol.boardListenerFunc ) {
        model_foreachPrevCell( model, xwe, model->vol.boardListenerFunc,
                               model->vol.boardListenerData );
    }
} /* invalLastMove */

void
model_foreachPendingCell( ModelCtxt* model, XWEnv xwe, XP_S16 turn,
                          BoardListener bl, void* closure )
{
    PendingTile* pt;
    PlayerCtxt* player;
    XP_S16 count;

    XP_ASSERT( turn >= 0 );
    player = &model->players[turn];
    count = player->nPending;

    for ( pt = player->pendingTiles; count--; ++pt ) {
        (*bl)( xwe, closure, turn, pt->col, pt->row, XP_FALSE );
    }
} /* model_foreachPendingCell */

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
    XP_ASSERT( col < model->nCols );
    XP_ASSERT( row < model->nRows );
    model->vol.tiles[(row*model->nCols) + col] = tile;
} /* model_setTile */

static CellTile 
getModelTileRaw( const ModelCtxt* model, XP_U16 col, XP_U16 row )
{
    XP_U16 nCols = model->nCols;
    XP_ASSERT( model->nRows == nCols );
    XP_ASSERT( col < nCols );
    XP_ASSERT( row < nCols );
    return model->vol.tiles[(row*nCols) + col];
} /* getModelTileRaw */

static void
undoFromMove( ModelCtxt* model, XWEnv xwe, XP_U16 turn, Tile blankTile, MoveRec* move )
{
    const MoveInfo* mi = &move->moveInfo;

    XP_U16 col, row;
    col = row = mi->commonCoord;
    XP_U16* other = mi->isHorizontal? &col: &row;

    const MoveInfoTile* tinfo;
    XP_U16 ii;
    for ( tinfo = mi->tiles, ii = 0; ii < mi->nTiles; ++tinfo, ++ii ) {
        Tile tile = tinfo->tile;
        *other = tinfo->varCoord;

        setModelTileRaw( model, col, row, EMPTY_TILE );
        notifyBoardListeners( model, xwe, turn, col, row, XP_FALSE );
        --model->vol.nTilesOnBoard;

        if ( IS_BLANK(tile) ) {
            tile = blankTile;
        }
        model_addPlayerTile( model, turn, -1, tile );
    }

    if ( model->vol.gi->inDuplicateMode ) {
        dupe_adjustScores( model, XP_FALSE, move->dup.nScores, move->dup.scores );
    } else {
        adjustScoreForUndone( model, xwe, mi, turn );
    }
} /* undoFromMove */

/* Remove tiles in a set from tray and put them back in the pool.
 */
static void
replaceNewTiles( ModelCtxt* model, PoolContext* pool, XP_U16 turn,
                 TrayTileSet* tileSet )
{
    Tile* t;
    XP_U16 ii, nTiles;

    for ( t = tileSet->tiles, ii = 0, nTiles = tileSet->nTiles;
          ii < nTiles; ++ii ) {
        XP_S16 index;
        Tile tile = *t++;

        index = model_trayContains( model, turn, tile );
        XP_ASSERT( index >= 0 );
        model_removePlayerTile( model, turn, index );
    }
    if ( !!pool ) {
        pool_replaceTiles( pool, tileSet );
    }
} /* replaceNewTiles */

/* Turn the most recent move into a phony.
 */
void
model_rejectPreviousMove( ModelCtxt* model, XWEnv xwe,
                          PoolContext* pool, XP_U16* turn )
{
    StackCtxt* stack = model->vol.stack;
    StackEntry entry;
    Tile blankTile = dict_getBlankTile( model_getDictionary(model) );

    stack_popEntry( stack, &entry );
    XP_ASSERT( entry.moveType == MOVE_TYPE );

    model_resetCurrentTurn( model, xwe, entry.playerNum );

    replaceNewTiles( model, pool, entry.playerNum, &entry.u.move.newTiles );
    XP_ASSERT( !model->vol.gi->inDuplicateMode );
    undoFromMove( model, xwe, entry.playerNum, blankTile, &entry.u.move );

    stack_addPhony( stack, entry.playerNum, &entry.u.phony.moveInfo );

    *turn = entry.playerNum;
    stack_freeEntry( stack, &entry );
} /* model_rejectPreviousMove */

XP_Bool
model_canUndo( const ModelCtxt* model )
{
    XP_Bool inDuplicateMode = model->vol.gi->inDuplicateMode;
    /* I'm turning off undo for duplicate mode for now to avoid
       crashes. Ideally a duplicate mode player could change his mind until
       the timer fires, so try to fix this. PENDING*/
    XP_Bool result = !inDuplicateMode;
    if ( result ) {
        const StackCtxt* stack = model->vol.stack;
        XP_U16 nStackEntries = stack_getNEntries( stack );

        /* More than just tile assignment? */
        XP_U16 assignCount = inDuplicateMode ? 1 : model->nPlayers;
        result = nStackEntries > assignCount;
    }
    return result;
}

/* Undo a move, but only if it's the move we're expecting to undo (as
 * indicated by *moveNumP, if >= 0).
 */
XP_Bool
model_undoLatestMoves( ModelCtxt* model, XWEnv xwe, PoolContext* pool,
                       XP_U16 nMovesSought, XP_U16* turnP, XP_S16* moveNumP )
{
    XP_ASSERT( 0 < nMovesSought ); /* this case isn't handled correctly */
    StackCtxt* stack = model->vol.stack;
    XP_Bool success;
    XP_S16 moveSought = !!moveNumP ? *moveNumP : -1;
    XP_U16 nStackEntries = stack_getNEntries( stack );
    const XP_U16 assignCount = model->vol.gi->inDuplicateMode
        ? 1 : model->nPlayers;

    if ( 0 <= moveSought && moveSought >= nStackEntries ) {
        XP_LOGFF( "BAD: moveSought (%d) >= nStackEntries (%d)", moveSought,
                  nStackEntries );
        success = XP_FALSE;
    } else if ( nStackEntries < nMovesSought ) {
        XP_LOGFF( "BAD: nStackEntries (%d) < nMovesSought (%d)", nStackEntries,
                  nMovesSought );
        success = XP_FALSE;
    } else if ( nStackEntries <= assignCount ) {
        XP_LOGFF( "BAD: nStackEntries (%d) <= assignCount (%d)", nStackEntries,
                  assignCount );
        success = XP_FALSE;
    } else {
        XP_U16 nMovesUndone = 0;
        XP_U16 turn;
        StackEntry entry;
        Tile blankTile = dict_getBlankTile( model_getDictionary(model) );

        for ( ; ; ) {
            success = stack_popEntry( stack, &entry );
            if ( !success ) {
                break;
            }
            ++nMovesUndone;

            turn = entry.playerNum;
            model_resetCurrentTurn( model, xwe, turn );

            if ( entry.moveType == MOVE_TYPE ) {
                /* get the tiles out of player's tray and back into the
                   pool */
                replaceNewTiles( model, pool, turn, &entry.u.move.newTiles );

                undoFromMove( model, xwe, turn, blankTile, &entry.u.move );
                model_sortTiles( model, turn );

                if ( model->vol.gi->inDuplicateMode ) {
#ifdef DEBUG
                    if ( DUP_PLAYER != turn ) {
                        XP_LOGFF( "turn: %d", turn );
                        XP_ASSERT( 0 );
                    }
#endif
                    model_cloneDupeTrays( model, xwe );
                }
            } else if ( entry.moveType == TRADE_TYPE ) {
                replaceNewTiles( model, pool, turn, 
                                 &entry.u.trade.newTiles );
                if ( pool != NULL ) {
                    pool_removeTiles( pool, &entry.u.trade.oldTiles );
                }
                model_addNewTiles( model, turn, &entry.u.trade.oldTiles );
            } else if ( entry.moveType == PHONY_TYPE ) {
                /* nothing to do, since nothing happened */
            } else {
                XP_ASSERT( entry.moveType == ASSIGN_TYPE );
                success = XP_FALSE;
                break;
            }

            /* exit if we've undone what we're supposed to.  If the sought
               stack height has been provided, use that.  Otherwise go by move
               count. */
            if ( 0 <= moveSought ) {
                if ( moveSought == entry.moveNum ) {
                    break;
                }
            } else if ( nMovesSought == nMovesUndone ) {
                break;
            }
            stack_freeEntry( stack, &entry );
        }

        /* Find the first MOVE still on the stack and highlight its tiles since
           they're now the most recent move. Trades and lost turns ignored.  */
        for ( nStackEntries = stack_getNEntries( stack ); 
              0 < nStackEntries; --nStackEntries ) {
            StackEntry entry;
            if ( !stack_getNthEntry( stack, nStackEntries - 1, &entry ) ) {
                break;
            }
            if ( entry.moveType == ASSIGN_TYPE ) {
                break;
            } else if ( entry.moveType == MOVE_TYPE ) {
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
                    notifyBoardListeners( model, xwe, entry.playerNum, col, row,
                                          XP_FALSE );
                }
                break;
            }
            stack_freeEntry( stack, &entry );
        }

        /* We fail if we didn't undo as many as requested UNLESS the lower
           limit is the trumping target/test. */
        if ( 0 <= moveSought ) {
            if ( moveSought != entry.moveNum ) {
                success = XP_FALSE;
            }
        } else {
            if ( nMovesUndone != nMovesSought ) {
                success = XP_FALSE;
            }
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
                /* redo isn't enough here: pool's got tiles in it!! */
                XP_ASSERT( 0 );
                (void)stack_redo( stack, NULL );
            }
        }
    }

    return success;
} /* model_undoLatestMoves */

void
model_trayToStream( const ModelCtxt* model, XP_S16 turn, XWStreamCtxt* stream )
{
    XP_ASSERT( turn >= 0 );
    const PlayerCtxt* player = &model->players[turn];

    traySetToStream( stream, &player->trayTiles );
} /* model_trayToStream */

void
model_currentMoveToMoveInfo( const ModelCtxt* model, XP_S16 turn,
                             MoveInfo* moveInfo )
{
    XP_ASSERT( turn >= 0 );
    const XP_S16 numTiles = model->players[turn].nPending;
    moveInfo->nTiles = numTiles;

    XP_U16 cols[MAX_TRAY_TILES];
    XP_U16 rows[MAX_TRAY_TILES];
    for ( XP_S16 ii = 0; ii < numTiles; ++ii ) {
        XP_Bool isBlank;
        Tile tile;
        model_getCurrentMoveTile( model, turn, &ii, &tile,
                                  &cols[ii], &rows[ii], &isBlank );
        if ( isBlank ) {
            tile |= TILE_BLANK_BIT;
        }
        moveInfo->tiles[ii].tile = tile;
    }

    XP_Bool isHorizontal = XP_TRUE;
    if ( 1 == numTiles ) {       /* horizonal/vertical makes no sense */
        moveInfo->tiles[0].varCoord = cols[0];
        moveInfo->commonCoord = rows[0];
    } else if ( 1 < numTiles ) {
        isHorizontal = rows[0] == rows[1];
        moveInfo->commonCoord = isHorizontal ? rows[0] : cols[0];
        for ( XP_U16 ii = 0; ii < numTiles; ++ii ) {
            moveInfo->tiles[ii].varCoord =
                isHorizontal ? cols[ii] : rows[ii];
            /* MoveInfo assumes legal moves! Check here */
            if ( isHorizontal ) {
                XP_ASSERT( rows[ii] == rows[0] );
            } else {
                XP_ASSERT( cols[ii] == cols[0] );
            }
        }
    }
    moveInfo->isHorizontal = isHorizontal;

    normalizeMI( moveInfo, moveInfo );
}

void
model_currentMoveToStream( const ModelCtxt* model, XP_S16 turn,
                           XWStreamCtxt* stream )
{
#ifdef STREAM_VERS_BIGBOARD
    XP_U16 nColsNBits = 16 <= model_numCols( model ) ? NUMCOLS_NBITS_5
        : NUMCOLS_NBITS_4;
#else
    XP_U16 nColsNBits = NUMCOLS_NBITS_4;
#endif

    XP_ASSERT( turn >= 0 );
    XP_S16 numTiles = model->players[turn].nPending;

    stream_putBits( stream, tilesNBits(stream), numTiles );

    while ( numTiles-- ) {
        Tile tile;
        XP_U16 col, row;
        XP_Bool isBlank;

        model_getCurrentMoveTile( model, turn, &numTiles, &tile,
                                  &col, &row, &isBlank );
        XP_ASSERT( numTiles >= 0 );
        stream_putBits( stream, TILE_NBITS, tile );
        stream_putBits( stream, nColsNBits, col );
        stream_putBits( stream, nColsNBits, row );
        stream_putBits( stream, 1, isBlank );
    }
} /* model_currentMoveToStream */

/* Take stream as the source of info about what tiles to move from tray to
 * board.  Undo any current move first -- a player on this device might be
 * using the board as scratch during another player's turn.  For each tile,
 * assert that it's in the tray, remove it from the tray, and place it on the
 * board.
 */
XP_Bool
model_makeTurnFromStream( ModelCtxt* model, XWEnv xwe, XP_U16 playerNum,
                          XWStreamCtxt* stream )
{
    Tile blank = dict_getBlankTile( model_getDictionary(model) );
    XP_U16 nColsNBits =
#ifdef STREAM_VERS_BIGBOARD
        16 <= model_numCols( model ) ? NUMCOLS_NBITS_5 : NUMCOLS_NBITS_4
#else
        NUMCOLS_NBITS_4
#endif
        ;

    model_resetCurrentTurn( model, xwe, playerNum );

    XP_U16 numTiles = (XP_U16)stream_getBits( stream, tilesNBits(stream) );
    XP_LOGFF( "numTiles=%d", numTiles );

    Tile tileFaces[numTiles];
    XP_U16 cols[numTiles];
    XP_U16 rows[numTiles];
    XP_Bool isBlanks[numTiles];
    Tile moveTiles[numTiles];
    TrayTileSet curTiles = *model_getPlayerTiles( model, playerNum );

    XP_Bool success = XP_TRUE;
    for ( XP_U16 ii = 0; success && ii < numTiles; ++ii ) {
        tileFaces[ii] = (Tile)stream_getBits( stream, TILE_NBITS );
        cols[ii] = (XP_U16)stream_getBits( stream, nColsNBits );
        rows[ii] = (XP_U16)stream_getBits( stream, nColsNBits );
        isBlanks[ii] = stream_getBits( stream, 1 );

        if ( isBlanks[ii] ) {
            moveTiles[ii] = blank;
        } else {
            moveTiles[ii] = tileFaces[ii];
        }

        XP_S16 index = setContains( &curTiles, moveTiles[ii] );
        if ( 0 <= index ) {
            removeTile( &curTiles, index );
        } else {
            success = XP_FALSE;
        }
    }

    if ( success ) {
        for ( XP_U16 ii = 0; ii < numTiles; ++ii ) {
            XP_S16 foundAt = model_trayContains( model, playerNum, moveTiles[ii] );
            if ( foundAt == -1 ) {
                XP_ASSERT( EMPTY_TILE == model_getPlayerTile(model, playerNum,
                                                             0));
                /* Does this ever happen? */
                XP_LOGFF( "found empty tile and it's ok" );

                (void)model_removePlayerTile( model, playerNum, -1 );
                model_addPlayerTile( model, playerNum, -1, moveTiles[ii] );
            }

            model_moveTrayToBoard( model, xwe, playerNum, cols[ii], rows[ii], foundAt,
                                   tileFaces[ii] );
        }
    }
    return success;
} /* model_makeTurnFromStream */

#ifdef DEBUG
void
juggleMoveIfDebug( MoveInfo* move )
{
    XP_U16 nTiles = move->nTiles;
    // XP_LOGF( "%s(): move len: %d", __func__, nTiles );
    MoveInfoTile tiles[MAX_TRAY_TILES];
    XP_MEMCPY( tiles, move->tiles, sizeof(tiles) );

    for ( int ii = 0; ii < nTiles; ++ii ) {
        int last = nTiles - ii;
        int choice = XP_RANDOM() % last;
        move->tiles[ii] = tiles[choice];
        // XP_LOGF( "%s(): setting %d to %d", __func__, ii, choice );
        if ( choice != --last ) {
            tiles[choice] = tiles[last];
            // XP_LOGF( "%s(): replacing %d with %d", __func__, choice, last );
        }
    }
}
#endif

/* Reverse the *letters on* the tiles */
#ifdef XWFEATURE_ROBOTPHONIES
void
reverseTiles( MoveInfo* move )
{
    MoveInfoTile* start = &move->tiles[0];
    MoveInfoTile* end = start + move->nTiles - 1;
    while ( start < end ) {
        Tile tmp = start->tile;
        start->tile = end->tile;
        end->tile = tmp;
        --end; ++start;
    }
}
#endif

void
model_makeTurnFromMoveInfo( ModelCtxt* model, XWEnv xwe, XP_U16 playerNum,
                            const MoveInfo* newMove )
{
    XP_U16 col, row, ii;
    XP_U16* other;
    const MoveInfoTile* tinfo;
    Tile blank = dict_getBlankTile( model_getDictionary( model ) );
    XP_U16 numTiles = newMove->nTiles;

    col = row = newMove->commonCoord; /* just assign both */
    other = newMove->isHorizontal? &col: &row;

    for ( tinfo = newMove->tiles, ii = 0; ii < numTiles; ++ii, ++tinfo ) {
        XP_S16 tileIndex;
        Tile tile = tinfo->tile;

        if ( IS_BLANK(tile) ) {
            tile = blank;
        }

        tileIndex = model_trayContains( model, playerNum, tile );

        XP_ASSERT( tileIndex >= 0 );

        *other = tinfo->varCoord;
        model_moveTrayToBoard( model, xwe, (XP_S16)playerNum, col, row, tileIndex,
                               (Tile)(tinfo->tile & TILE_VALUE_MASK) );
    }
} /* model_makeTurnFromMoveInfo */

void
model_countAllTrayTiles( ModelCtxt* model, XP_U16* counts, 
                         XP_S16 excludePlayer )
{
    PlayerCtxt* player;
    XP_S16 nPlayers = model->nPlayers;
    XP_S16 ii;
    Tile blank;

    blank = dict_getBlankTile( model_getDictionary(model) );

    for ( ii = 0, player = model->players; ii < nPlayers; ++ii, ++player ) {
        if ( ii != excludePlayer ) {
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
    XP_ASSERT( turn >= 0 );
    XP_ASSERT( turn < model->nPlayers );

    const TrayTileSet* tiles = model_getPlayerTiles( model, turn );
    return setContains( tiles, tile );
} /* model_trayContains */

XP_U16
model_getCurrentMoveCount( const ModelCtxt* model, XP_S16 turn )
{
    const PlayerCtxt* player;
    XP_ASSERT( turn >= 0 );
    player = &model->players[turn];
    return player->nPending;
} /* model_getCurrentMoveCount */

XP_Bool
model_getCurrentMoveIsVertical( const ModelCtxt* model, XP_S16 turn,
                                XP_Bool* isVertical )
{
    XP_ASSERT( turn >= 0 );
    const PlayerCtxt* player = &model->players[turn];
    XP_U16 nPending = player->nPending;
    XP_Bool known = 2 <= nPending;
    if ( known ) {
        --nPending;
        if ( player->pendingTiles[nPending].col
             == player->pendingTiles[nPending-1].col ) {
            *isVertical = XP_TRUE;
        } else if ( player->pendingTiles[nPending].row
             == player->pendingTiles[nPending-1].row ) {
            *isVertical = XP_FALSE;
        } else {
            known = XP_FALSE;
        }
    }
    return known;
}

void
model_getCurrentMoveTile( const ModelCtxt* model, XP_S16 turn, XP_S16* index,
                          Tile* tile, XP_U16* col, XP_U16* row, 
                          XP_Bool* isBlank )
{
    XP_ASSERT( turn >= 0 );

    const PlayerCtxt* player = &model->players[turn];
    XP_ASSERT( *index < player->nPending );
    
    if ( *index < 0 ) {
        *index = player->nPending - 1;
    }

    const PendingTile* pt = &player->pendingTiles[*index];

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
model_removePlayerTiles( ModelCtxt* model, XP_S16 turn, const TrayTileSet* tiles )
{
    XP_ASSERT( turn >= 0 );
    PlayerCtxt* player = &model->players[turn];
    for ( XP_U16 ii = 0; ii < tiles->nTiles; ++ii ) {
        Tile tile = tiles->tiles[ii];
        XP_S16 index = -1;
        for ( XP_U16 jj = 0; index < 0 && jj < player->trayTiles.nTiles; ++jj ) {
            if ( tile == player->trayTiles.tiles[jj] ) {
                index = jj;
            }
        }
        XP_ASSERT( index >= 0 );
        model_removePlayerTile( model, turn, index );
    }
}

void
model_packTilesUtil( ModelCtxt* model, PoolContext* pool,
                     XP_Bool includeBlank, 
                     XP_U16* nUsed, const XP_UCHAR** texts,
                     Tile* tiles )
{
    const DictionaryCtxt* dict = model_getDictionary(model);
    XP_U16 nFaces = dict_numTileFaces( dict );
    Tile blankFace = dict_getBlankTile( dict );
    XP_U16 nFacesAvail = 0;

    XP_ASSERT( nFaces <= *nUsed );

    for ( Tile tile = 0; tile < nFaces; ++tile ) {
        if ( includeBlank ) {
            XP_ASSERT( !!pool );
            if ( 0 == pool_getNTilesLeftFor( pool, tile ) ) {
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

/* setup async query for blank value, but while at it return a reasonable
   default.  */
Tile
model_askBlankTile( ModelCtxt* model, XWEnv xwe, XP_U16 turn, XP_U16 col, XP_U16 row )
{
    XP_U16 nUsed = MAX_UNIQUE_TILES;
    const XP_UCHAR* tfaces[MAX_UNIQUE_TILES];
    Tile tiles[MAX_UNIQUE_TILES];

    model_packTilesUtil( model, NULL, XP_FALSE,
                         &nUsed, tfaces, tiles );

    util_notifyPickTileBlank( model->vol.util, xwe, turn, col, row,
                              tfaces, nUsed );
    return tiles[0];
} /* model_askBlankTile */

void
model_moveTrayToBoard( ModelCtxt* model, XWEnv xwe, XP_S16 turn, XP_U16 col, XP_U16 row,
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
            tile = TILE_BLANK_BIT
                | model_askBlankTile( model, xwe, (XP_U16)turn, col, row );
        }
        tile |= TILE_BLANK_BIT;
    }
    
    player = &model->players[turn];

    if ( player->nPending == 0 ) {
        invalLastMove( model, xwe );
    }

    player->nUndone = 0;
    pt = &player->pendingTiles[player->nPending++];
    XP_ASSERT( player->nPending <= MAX_TRAY_TILES );

    pt->tile = tile;
    pt->col = (XP_U8)col;
    pt->row = (XP_U8)row;

    invalidateScore( model, turn );
    incrPendingTileCountAt( model, col, row );

    notifyBoardListeners( model, xwe, turn, col, row, XP_TRUE );
} /* model_moveTrayToBoard */

XP_Bool
model_setBlankValue( ModelCtxt* model, XP_U16 turn,
                     XP_U16 col, XP_U16 row, XP_U16 newIndex )
{
    XP_Bool found = XP_FALSE;
    PlayerCtxt* player = &model->players[turn];
    for ( int ii = 0; ii < player->nPending; ++ii ) {
        PendingTile* pt = &player->pendingTiles[ii];
        found = pt->col == col && pt->row == row;
        if ( found ) {
            XP_ASSERT( (pt->tile & TILE_BLANK_BIT) != 0 );
            if ( (pt->tile & TILE_BLANK_BIT) != 0 ) {
                XP_U16 nUsed = MAX_UNIQUE_TILES;
                const XP_UCHAR* tfaces[MAX_UNIQUE_TILES];
                Tile tiles[MAX_UNIQUE_TILES];
                model_packTilesUtil( model, NULL, XP_FALSE,
                                     &nUsed, tfaces, tiles );

                pt->tile = tiles[newIndex] | TILE_BLANK_BIT;

                /* force a recalc in case phonies==PHONIES_BLOCK */
                invalidateScore( model, turn );
            }
            break;
        }
    }
    return found;
}

XP_Bool
model_redoPendingTiles( ModelCtxt* model, XWEnv xwe, XP_S16 turn )
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
                model_moveTrayToBoard( model, xwe, turn, pt->col, pt->row,
                                        foundAt, pt->tile & ~TILE_BLANK_BIT );
                ++actualCnt;
            }
        }
    }
    return actualCnt > 0;
}

void
model_moveBoardToTray( ModelCtxt* model, XWEnv xwe, XP_S16 turn,
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
        notifyBoardListeners( model, xwe, turn, col, row, XP_FALSE );

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
            invalLastMove( model, xwe );
        }

        invalidateScore( model, turn );
    }
} /* model_moveBoardToTray */

XP_Bool
model_moveTileOnBoard( ModelCtxt* model, XWEnv xwe, XP_S16 turn, XP_U16 colCur,
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
                (void)model_askBlankTile( model, xwe, turn, colNew, rowNew );
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
model_resetCurrentTurn( ModelCtxt* model, XWEnv xwe, XP_S16 whose )
{
    PlayerCtxt* player;

    XP_ASSERT( whose >= 0 && whose < model->nPlayers );
    player = &model->players[whose];

    while ( player->nPending > 0 ) {
        model_moveBoardToTray( model, xwe, whose,
                               player->pendingTiles[0].col,
                               player->pendingTiles[0].row,
                               -1 );
    }
} /* model_resetCurrentTurn */

XP_S16
model_getNMoves( const ModelCtxt* model )
{
    XP_U16 nAssigns = model->vol.gi->inDuplicateMode ? 1 : model->nPlayers;
    XP_U16 result = stack_getNEntries( model->vol.stack ) - nAssigns;
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
putBackOtherPlayersTiles( ModelCtxt* model, XWEnv xwe, XP_U16 notMyTurn,
                          XP_U16 col, XP_U16 row )
{
    XP_U16 turn;

    for ( turn = 0; turn < model->nPlayers; ++turn ) {
        if ( turn == notMyTurn ) {
            continue;
        }
        model_moveBoardToTray( model, xwe, turn, col, row, -1 );
    }
} /* putBackOtherPlayersTiles */

static void
invalidateScores( ModelCtxt* model )
{
    for ( int ii = 0; ii < model->nPlayers; ++ii ) {
        invalidateScore( model, ii );
    }
}

/* Make those tiles placed by 'turn' a permanent part of the board.  If any
 * other players have placed pending tiles on those same squares, replace them
 * in their trays.
 */
static XP_S16
commitTurn( ModelCtxt* model, XWEnv xwe, XP_S16 turn, const TrayTileSet* newTiles,
            XWStreamCtxt* stream, WordNotifierInfo* wni, XP_Bool useStack )
{
    XP_S16 score = -1;

#ifdef DEBUG
    XP_ASSERT( getCurrentMoveScoreIfLegal( model, xwe, turn, (XWStreamCtxt*)NULL,
                                           (WordNotifierInfo*)NULL, &score ) );
    invalidateScore( model, turn );
#endif

    XP_ASSERT( turn >= 0 && turn < MAX_NUM_PLAYERS);

    clearLastMoveInfo( model, xwe );

    PlayerCtxt* player = &model->players[turn];

    if ( useStack ) {
        XP_Bool isHorizontal;
#ifdef DEBUG
        XP_Bool inLine = 
#endif
            tilesInLine( model, turn, &isHorizontal );
        XP_ASSERT( inLine );
        MoveInfo moveInfo = {0};
        normalizeMoves( model, turn, isHorizontal, &moveInfo );

        stack_addMove( model->vol.stack, turn, &moveInfo, newTiles );
    }

    /* Where's it removed from tray? Need to assert there! */
    for ( int ii = 0; ii < player->nPending; ++ii ) {
        const PendingTile* pt = &player->pendingTiles[ii];
        XP_U16 col = pt->col;
        XP_U16 row = pt->row;
        CellTile tile = getModelTileRaw( model, col, row );

        XP_ASSERT( (tile & TILE_PENDING_BIT) != 0 );

        XP_U16 val = tile & TILE_VALUE_MASK;
        if ( val > 1 ) { /* somebody else is using this square too! */
            putBackOtherPlayersTiles( model, xwe, turn, col, row );
        }

        tile = pt->tile;
        tile |= PREV_MOVE_BIT;
        tile |= turn << CELL_OWNER_OFFSET;

        setModelTileRaw( model, col, row, tile );

        notifyBoardListeners( model, xwe, turn, col, row, XP_FALSE );

        ++model->vol.nTilesOnBoard;
    }

    (void)getCurrentMoveScoreIfLegal( model, xwe, turn, stream, wni, &score );
    XP_ASSERT( score >= 0 );
    if ( ! model->vol.gi->inDuplicateMode ) {
        player->score += score;
    }

    invalidateScores( model );

    player->nPending = 0;
    player->nUndone = 0;

    /* Move new tiles into tray */
    for ( int ii = newTiles->nTiles - 1; ii >= 0; --ii ) {
        model_addPlayerTile( model, turn, -1, newTiles->tiles[ii] );
    }

    return score;
} /* commitTurn */

XP_Bool
model_commitTurn( ModelCtxt* model, XWEnv xwe, XP_S16 turn, TrayTileSet* newTiles )
{
    XP_S16 score = commitTurn( model, xwe, turn, newTiles, NULL, NULL, XP_TRUE );
    return 0 <= score;
} /* model_commitTurn */

void
model_commitDupeTurn( ModelCtxt* model, XWEnv xwe, const MoveInfo* moveInfo,
                      XP_U16 nScores, XP_U16* scores, TrayTileSet* newTiles )
{
    model_resetCurrentTurn( model, xwe, DUP_PLAYER );
    model_makeTurnFromMoveInfo( model, xwe, DUP_PLAYER, moveInfo );
    (void)commitTurn( model, xwe, DUP_PLAYER, newTiles, NULL, NULL, XP_FALSE );
    dupe_adjustScores( model, XP_TRUE, nScores, scores );
    invalidateScores( model );

    stack_addDupMove( model->vol.stack, moveInfo, nScores, scores, newTiles );
}

void
model_commitDupeTrade( ModelCtxt* model, const TrayTileSet* oldTiles,
                       const TrayTileSet* newTiles )
{
    stack_addDupTrade( model->vol.stack, oldTiles, newTiles );
}

void
model_noteDupePause( ModelCtxt* model, XWEnv xwe, DupPauseType typ, XP_S16 turn,
                     const XP_UCHAR* msg )
{
    XP_U32 when = dutil_getCurSeconds( model->vol.dutil, xwe );
    stack_addPause( model->vol.stack, typ, turn, when, msg );
}

/* Given a rack of new tiles and of old, remove all the old from the tray and
 * replace them with new.  Replace in the same place so that user sees an
 * in-place change.
 */
static void
makeTileTrade( ModelCtxt* model, XP_S16 player, const TrayTileSet* oldTiles, 
               const TrayTileSet* newTiles )
{
    XP_ASSERT( newTiles->nTiles == oldTiles->nTiles );
    XP_ASSERT( oldTiles != &model->players[player].trayTiles );

    const XP_U16 nTiles = newTiles->nTiles;
    for ( XP_U16 ii = 0; ii < nTiles; ++ii ) {
        Tile oldTile = oldTiles->tiles[ii];

        XP_S16 tileIndex = model_trayContains( model, player, oldTile );
        XP_ASSERT( tileIndex >= 0 );
        model_removePlayerTile( model, player, tileIndex );
        model_addPlayerTile( model, player, tileIndex, newTiles->tiles[ii] );
    }
} /* makeTileTrade */

void
model_makeTileTrade( ModelCtxt* model, XP_S16 player,
                     const TrayTileSet* oldTiles, const TrayTileSet* newTiles )
{
    stack_addTrade( model->vol.stack, player, oldTiles, newTiles );

    makeTileTrade( model, player, oldTiles, newTiles );
} /* model_makeTileTrade */

Tile
model_getPlayerTile( const ModelCtxt* model, XP_S16 turn, XP_S16 index )
{
    const PlayerCtxt* player;
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

#ifdef DEBUG
XP_UCHAR*
formatTileSet( const TrayTileSet* tiles, XP_UCHAR* buf, XP_U16 len )
{
    XP_U16 ii, used;
    for ( ii = 0, used = 0; ii < tiles->nTiles && used < len; ++ii ) {
        used += XP_SNPRINTF( &buf[used], len - used, "%d,", tiles->tiles[ii] );
    }
    if ( used > len ) {
        buf[len-1] = '\0';
    }
    return buf;
}
#endif

static void
addPlayerTile( ModelCtxt* model, XP_S16 turn, XP_S16 index, const Tile tile )
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

XP_U16
model_getDividerLoc( const ModelCtxt* model, XP_S16 turn )
{
    XP_ASSERT( turn >= 0 );
    const PlayerCtxt* player = &model->players[turn];
    return player->dividerLoc;
}

void
model_setDividerLoc( ModelCtxt* model, XP_S16 turn, XP_U16 loc )
{
    XP_ASSERT( turn >= 0 );
    PlayerCtxt* player = &model->players[turn];
    XP_ASSERT( loc < 0xFF );
    player->dividerLoc = (XP_U8)loc;
}

void
model_addNewTiles( ModelCtxt* model, XP_S16 turn, const TrayTileSet* tiles )
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
    XP_ASSERT( turn == DUP_PLAYER || !model->vol.gi->inDuplicateMode );
    TrayTileSet sorted;
    sortTiles( &sorted, tiles, 0 );
    stack_addAssign( model->vol.stack, turn, &sorted );

    model_addNewTiles( model, turn, &sorted );
} /* model_assignPlayerTiles */

XP_S16
model_getNextTurn( const ModelCtxt* model )
{
    XP_S16 result = stack_getNextTurn( model->vol.stack );
    // LOG_RETURNF( "%d", result );
    return result;
}

void
model_assignDupeTiles( ModelCtxt* model, XWEnv xwe, const TrayTileSet* tiles )
{
    model_assignPlayerTiles( model, DUP_PLAYER, tiles );
    model_cloneDupeTrays( model, xwe );
}

void
model_sortTiles( ModelCtxt* model, XP_S16 turn )
{
    XP_U16 dividerLoc = model_getDividerLoc( model, turn );
    const TrayTileSet* curTiles = model_getPlayerTiles( model, turn );
    if ( curTiles->nTiles >= dividerLoc) { /* any to sort? */
        TrayTileSet sorted;
        sortTiles( &sorted, curTiles, dividerLoc );

        for ( XP_S16 nTiles = sorted.nTiles; nTiles > 0; ) {
            removePlayerTile( model, turn, --nTiles );
        }

        model_addNewTiles( model, turn, &sorted );
    }
} /* model_sortTiles */

XP_U16
model_getNumTilesInTray( ModelCtxt* model, XP_S16 turn )
{
    PlayerCtxt* player;
    XP_ASSERT( turn >= 0 );
    player = &model->players[turn];
    XP_U16 result = player->trayTiles.nTiles;
    // XP_LOGFF( "(turn=%d) => %d", turn, result );
    return result;
} /* model_getNumTilesInTray */

XP_U16
model_getNumTilesTotal( ModelCtxt* model, XP_S16 turn )
{
    XP_ASSERT( turn >= 0 );
    PlayerCtxt* player = &model->players[turn];
    XP_U16 result = player->trayTiles.nTiles + player->nPending;
    return result;
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
notifyBoardListeners( ModelCtxt* model, XWEnv xwe, XP_U16 turn, XP_U16 col,
                      XP_U16 row, XP_Bool added )
{
    if ( model->vol.boardListenerFunc != NULL ) {
        (*model->vol.boardListenerFunc)( xwe, model->vol.boardListenerData,
                                         turn, col, row, added );
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
notifyDictListeners( ModelCtxt* model, XWEnv xwe, XP_S16 playerNum,
                     const DictionaryCtxt* oldDict, const DictionaryCtxt* newDict )
{
    if ( model->vol.dictListenerFunc != NULL ) {
        (*model->vol.dictListenerFunc)( model->vol.dictListenerData, xwe,
                                        playerNum, oldDict, newDict );
    }
} /* notifyDictListeners */

static void
printString( XWStreamCtxt* stream, const XP_UCHAR* str )
{
    stream_catString( stream, str );
} /* printString */

static XP_UCHAR*
formatTray( const TrayTileSet* tiles, const DictionaryCtxt* dict,
            XP_UCHAR* buf, XP_U16 bufSize, XP_Bool keepHidden )
{
    if ( keepHidden ) {
        XP_U16 ii;
        for ( ii = 0; ii < tiles->nTiles; ++ii ) {
            buf[ii] = '?';
        }
        buf[ii] = '\0';
    } else {
        dict_tilesToString( dict, (Tile*)tiles->tiles, tiles->nTiles, 
                            buf, bufSize, NULL );
    }

    return buf;
} /* formatTray */

typedef struct MovePrintClosure {
    XWStreamCtxt* stream;
    const DictionaryCtxt* dict;
    XP_U16 nPrinted;
    XP_Bool keepHidden;
    XP_U32 lastPauseWhen;
} MovePrintClosure;

static void
printMovePre( ModelCtxt* model, XWEnv xwe, XP_U16 XP_UNUSED(moveN),
              const StackEntry* entry, void* p_closure )
{
    if ( entry->moveType != ASSIGN_TYPE ) {
        const XP_UCHAR* format;
        XP_UCHAR buf[64];
        XP_UCHAR traybuf[32];
        MovePrintClosure* closure = (MovePrintClosure*)p_closure;
        XWStreamCtxt* stream = closure->stream;

        XP_SNPRINTF( buf, sizeof(buf), (XP_UCHAR*)"%d:%d ", ++closure->nPrinted, 
                     entry->playerNum+1 );
        printString( stream, (XP_UCHAR*)buf );

        switch ( entry->moveType ) {
        case TRADE_TYPE:
        case PAUSE_TYPE:
            break;
        default: {
            XP_UCHAR letter[2] = {'\0','\0'};
            XP_Bool isHorizontal = entry->u.move.moveInfo.isHorizontal;
            XP_U16 col, row;
            const MoveInfo* mi;
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
                format = dutil_getUserString( model->vol.dutil, xwe, STR_PASS );
                XP_SNPRINTF( buf, VSIZE(buf), "%s", format );
            } else {
                if ( isHorizontal ) {
                    format = dutil_getUserString( model->vol.dutil, xwe, STRS_MOVE_ACROSS );
                } else {
                    format = dutil_getUserString( model->vol.dutil, xwe, STRS_MOVE_DOWN );
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

            if ( !closure->keepHidden ) {
                format = dutil_getUserString( model->vol.dutil, xwe, STRS_TRAY_AT_START );
                formatTray( model_getPlayerTiles( model, entry->playerNum ),
                            closure->dict, (XP_UCHAR*)traybuf, sizeof(traybuf),
                            XP_FALSE );
                XP_SNPRINTF( buf, sizeof(buf), format, traybuf );
                printString( stream, buf );
            }
        }
            break;
        }
    }
} /* printMovePre */

static void
printMovePost( ModelCtxt* model, XWEnv xwe, XP_U16 XP_UNUSED(moveN),
               const StackEntry* entry, XP_S16 XP_UNUSED(score),
               void* p_closure )
{
    if ( entry->moveType != ASSIGN_TYPE ) {
        MovePrintClosure* closure = (MovePrintClosure*)p_closure;
        XWStreamCtxt* stream = closure->stream;
        const DictionaryCtxt* dict = closure->dict;
        const XP_UCHAR* format;
        XP_U16 nTiles;

        XP_UCHAR buf[100];
        XP_UCHAR traybuf1[32];
        XP_UCHAR traybuf2[32];
        const MoveInfo* mi;
        XP_S16 totalScore = model_getPlayerScore( model, entry->playerNum );
        XP_Bool addCR = XP_FALSE;

        switch( entry->moveType ) {
        case TRADE_TYPE:
            XP_ASSERT( entry->u.trade.oldTiles.nTiles == entry->u.trade.newTiles.nTiles );
            formatTray( (const TrayTileSet*)&entry->u.trade.oldTiles, 
                        dict, traybuf1, sizeof(traybuf1), closure->keepHidden );
            formatTray( (const TrayTileSet*) &entry->u.trade.newTiles, 
                        dict, traybuf2, sizeof(traybuf2), closure->keepHidden );

            format = dutil_getUserString( model->vol.dutil, xwe, STRSS_TRADED_FOR );
            XP_SNPRINTF( buf, sizeof(buf), format, traybuf1, traybuf2 );
            printString( stream, buf );
            addCR = XP_TRUE;
            break;

        case PHONY_TYPE:
            format = dutil_getUserString( model->vol.dutil, xwe, STR_PHONY_REJECTED );
            printString( stream, format );
            /* FALLTHRU */
        case MOVE_TYPE:
            /* Duplicate case */
            if ( model->vol.gi->inDuplicateMode ) {
                XP_U16 offset = 0; // XP_SNPRINTF( buf, VSIZE(buf), "%s", format );
                for ( XP_U16 ii = 0; ii < entry->u.move.dup.nScores; ++ii ) {
                    offset += XP_SNPRINTF( &buf[offset], VSIZE(buf) - offset, "%d,",
                                           entry->u.move.dup.scores[ii] );
                }
                buf[offset-1] = '\0'; /* replace last ',' */

                XP_UCHAR buf2[256];
                format = dutil_getUserString( model->vol.dutil, xwe,
                                              STRS_DUP_ALLSCORES );
                XP_SNPRINTF( buf2, sizeof(buf2), format, buf );
                XP_STRCAT( buf2, "\n" );
                printString( stream, buf2 );
            }

            /* This is wrong */
            format = dutil_getUserString( model->vol.dutil, xwe, STRD_CUMULATIVE_SCORE );
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
                    format = dutil_getUserString( model->vol.dutil, xwe, STRS_NEW_TILES );
                    XP_SNPRINTF( buf, sizeof(buf), format,
                                 formatTray( &entry->u.move.newTiles, dict,
                                             traybuf1, sizeof(traybuf1),
                                             XP_FALSE ) );
                    printString( stream, buf );
                    addCR = XP_TRUE;
                }
            }

            break;
        case PAUSE_TYPE:
            util_formatPauseHistory( model->vol.util, xwe, stream, entry->u.pause.pauseType,
                                     entry->playerNum, closure->lastPauseWhen,
                                     entry->u.pause.when, entry->u.pause.msg );
            closure->lastPauseWhen = entry->u.pause.when;
            addCR = XP_TRUE;
            break;

        default:
            XP_ASSERT( 0 );
        }

        if ( addCR ) {
            printString( stream, (XP_UCHAR*)XP_CR );
        }

        printString( stream, (XP_UCHAR*)XP_CR );
    }
} /* printMovePost */

static void
copyStack( const ModelCtxt* model, StackCtxt* destStack,
           const StackCtxt* srcStack )
{
    XWStreamCtxt* stream =
        mem_stream_make_raw( MPPARM(model->vol.mpool)
                             dutil_getVTManager(model->vol.dutil) );

    stream_setVersion( stream, stack_getVersion(srcStack) );
    stack_writeToStream( srcStack, stream );
    stack_loadFromStream( destStack, stream );
    XP_ASSERT( stack_getVersion(destStack) == stack_getVersion( srcStack ) );

    stream_destroy( stream );
} /* copyStack */

static ModelCtxt*
makeTmpModel( const ModelCtxt* model, XWEnv xwe, XWStreamCtxt* stream,
              MovePrintFuncPre mpf_pre, MovePrintFuncPost mpf_post, 
              void* closure )
{
    ModelCtxt* tmpModel = model_make( MPPARM(model->vol.mpool) 
                                      xwe, model_getDictionary(model), NULL,
                                      model->vol.util, model_numCols(model) );
    tmpModel->loaner = model;
    model_setNPlayers( tmpModel, model->nPlayers );

    buildModelFromStack( tmpModel, xwe, model->vol.stack, XP_FALSE, 0, stream,
                         (WordNotifierInfo*)NULL, mpf_pre, mpf_post, closure );
    
    return tmpModel;
} /* makeTmpModel */

void
model_writeGameHistory( ModelCtxt* model, XWEnv xwe, XWStreamCtxt* stream,
                        ServerCtxt* server, XP_Bool gameOver )
{
    MovePrintClosure closure = {
        .stream = stream,
        .dict = model_getDictionary( model ),
        .keepHidden = !gameOver && !model->vol.gi->inDuplicateMode,
        .nPrinted = 0
    };

    ModelCtxt* tmpModel = makeTmpModel( model, xwe, stream, printMovePre,
                                        printMovePost, &closure );
    model_destroy( tmpModel, xwe );

    if ( gameOver ) {
        /* if the game's over, it shouldn't matter which model I pass to this
           method */
        server_writeFinalScores( server, xwe, stream );
    }
} /* model_writeGameHistory */

typedef struct _FirstWordData {
    XP_UCHAR word[32];
} FirstWordData;

static void
getFirstWord( const WNParams* wnp, void* closure )
{
    FirstWordData* data = (FirstWordData*)closure;
    if ( '\0' == data->word[0] && '\0' != wnp->word[0] ) {
        XP_STRCAT( data->word, wnp->word );
    }
}

static void
scoreLastMove( ModelCtxt* model, XWEnv xwe, MoveInfo* moveInfo, XP_U16 howMany,
               LastMoveInfo* lmi )
{
    XP_U16 score;
    WordNotifierInfo notifyInfo;
    FirstWordData data;

    ModelCtxt* tmpModel = makeTmpModel( model, xwe, NULL, NULL, NULL, NULL );
    XP_U16 turn;
    XP_S16 moveNum = -1;

    copyStack( model, tmpModel->vol.stack, model->vol.stack );

    if ( !model_undoLatestMoves( tmpModel, xwe, NULL, howMany, &turn,
                                 &moveNum ) ) {
        XP_ASSERT( 0 );
    }

    data.word[0] = '\0';
    notifyInfo.proc = getFirstWord;
    notifyInfo.closure = &data;
    score = figureMoveScore( tmpModel, xwe, turn, moveInfo, (EngineCtxt*)NULL,
                             (XWStreamCtxt*)NULL, &notifyInfo );

    model_destroy( tmpModel, xwe );

    lmi->score = score;
    XP_SNPRINTF( lmi->word, VSIZE(lmi->word), "%s", data.word );
} /* scoreLastMove */

static XP_U16
model_getRecentPassCount( ModelCtxt* model )
{
    StackCtxt* stack = model->vol.stack;
    XP_U16 nPasses = 0;

    XP_ASSERT( !!stack );

    XP_S16 nEntries = stack_getNEntries( stack );
    for ( XP_S16 which = nEntries - 1; which >= 0; --which ) {
        StackEntry entry;
        if ( !stack_getNthEntry( stack, which, &entry ) ) {
            break;
        }
        switch ( entry.moveType ) {
        case MOVE_TYPE:
            if ( entry.u.move.moveInfo.nTiles == 0 ) {
                ++nPasses;
            }
            break;
        default:
            break;
        }
        stack_freeEntry( stack, &entry );
    }
    return nPasses;
} /* model_getRecentPassCount */

XP_Bool
model_recentPassCountOk( ModelCtxt* model )
{
    XP_U16 count = model_getRecentPassCount( model );
    XP_U16 okCount = MAX_PASSES;
    if ( !model->vol.gi->inDuplicateMode ) {
        okCount *= model->nPlayers;
    }
    XP_ASSERT( count <= okCount ); /* should never be more than 1 over */
    return count < okCount;
}

static void
appendWithCR( XWStreamCtxt* stream, const XP_UCHAR* word, XP_U16* counter )
{
    if ( 0 < (*counter)++ ) {
        stream_putU8( stream, '\n' );
    }
    stream_catString( stream, word );
}

static void
recordWord( const WNParams* wnp, void* closure )
{
    RecordWordsInfo* info = (RecordWordsInfo*)closure;
    appendWithCR( info->stream, wnp->word, &info->nWords );
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

#ifdef XWFEATURE_BOARDWORDS
typedef struct _ListWordsThroughInfo {
    XWStreamCtxt* stream;
    XP_U16 col, row;
    XP_U16 nWords;
} ListWordsThroughInfo;

static void
listWordsThrough( const WNParams* wnp, void* closure )
{
    ListWordsThroughInfo* info = (ListWordsThroughInfo*)closure;
    const MoveInfo* movei = wnp->movei;

    XP_Bool contained = XP_FALSE;
    if ( movei->isHorizontal && movei->commonCoord == info->row ) {
        contained = wnp->start <= info->col && wnp->end >= info->col;
    } else if ( !movei->isHorizontal && movei->commonCoord == info->col ) {
        contained = wnp->start <= info->row && wnp->end >= info->row;
    }

    if ( contained ) {
        appendWithCR( info->stream, wnp->word, &info->nWords );
    }
}

/* List every word played that includes the tile on {col,row}.
 *
 * How?   Undo backwards until we find the move that placed that tile.*/
XP_Bool
model_listWordsThrough( ModelCtxt* model, XWEnv xwe, XP_U16 col, XP_U16 row,
                        XP_S16 turn, XWStreamCtxt* stream )
{
    XP_Bool found = XP_FALSE;
    ModelCtxt* tmpModel = makeTmpModel( model, xwe, NULL, NULL, NULL, NULL );
    copyStack( model, tmpModel->vol.stack, model->vol.stack );

    XP_Bool isHorizontal;
    if ( tilesInLine( model, turn, &isHorizontal ) ) {
        MoveInfo moveInfo = {0};
        normalizeMoves( model, turn, isHorizontal, &moveInfo );
        model_makeTurnFromMoveInfo( tmpModel, xwe, turn, &moveInfo );

        /* Might not be a legal move. If isn't, don't add it! */
        if ( getCurrentMoveScoreIfLegal( tmpModel, xwe, turn, (XWStreamCtxt*)NULL,
                                         (WordNotifierInfo*)NULL, NULL ) ) {
            TrayTileSet newTiles = {.nTiles = 0};
            commitTurn( tmpModel, xwe, turn, &newTiles, NULL, NULL, XP_TRUE );
        } else {
            model_resetCurrentTurn( tmpModel, xwe, turn );
        }
    }

    XP_ASSERT( !!stream );
    StackCtxt* stack = tmpModel->vol.stack;
    XP_U16 nEntriesBefore = stack_getNEntries( stack );
    XP_U16 nEntriesAfter;

    /* Loop until we undo the move that placed the tile. */
    while ( model_undoLatestMoves( tmpModel, xwe, NULL, 1, NULL, NULL ) ) {
        if ( 0 != (TILE_EMPTY_BIT & getModelTileRaw( tmpModel, col, row ) ) ) {
            break;
        }
    }

    nEntriesAfter = stack_getNEntries( stack );
    XP_ASSERT( nEntriesAfter < nEntriesBefore );
    if ( nEntriesAfter < nEntriesBefore ) {
        ListWordsThroughInfo lwtInfo = { .stream = stream, .col = col,
                                         .row = row, .nWords = 0,
        };
        WordNotifierInfo ni = { .proc = listWordsThrough, .closure = &lwtInfo };
        /* Now push the undone moves back into the model one at a time.
           recordWord() will add each played word to the stream as it's
           scored */
        while ( nEntriesAfter < nEntriesBefore ) {
            StackEntry entry;
            if ( ! stack_redo( stack, &entry ) ) {
                XP_ASSERT( 0 );
                break;
            }
            modelAddEntry( tmpModel, xwe, nEntriesAfter++, &entry, XP_FALSE, NULL, &ni,
                           NULL, NULL, NULL );
        }
        XP_LOGFF( "nWords: %d", lwtInfo.nWords );
        found = 0 < lwtInfo.nWords;
    }
    stream_putU8( stream, '\0' ); /* null-terminate for good luck */

    model_destroy( tmpModel, xwe );
    return found;
} /* model_listWordsThrough */
#endif

/* Set array of 1-4 (>1 in case of tie) with highest scores' owners */
static void
listHighestScores( const ModelCtxt* model, LastMoveInfo* lmi, MoveRec* move )
{
    /* find highest */
    XP_U16 max = 0;
    lmi->nWinners = 0;
    for ( XP_U16 ii = 0; ii < move->dup.nScores; ++ii ) {
        XP_U16 score = move->dup.scores[ii];
        if ( 0 == score || score < max ) {
            continue;
        } else if ( score > max ) {
            max = score;
            lmi->nWinners = 0;
            lmi->score = score;
        }
        lmi->names[lmi->nWinners++] = model->vol.gi->players[ii].name;
    }
}

XP_Bool
model_getPlayersLastScore( ModelCtxt* model, XWEnv xwe,
                           XP_S16 player, LastMoveInfo* lmi )
{
    StackCtxt* stack = model->vol.stack;
    XP_S16 nEntries, which;
    StackEntry entry;
    XP_Bool found = XP_FALSE;
    XP_Bool inDuplicateMode = model->vol.gi->inDuplicateMode;
    XP_MEMSET( lmi, 0, sizeof(*lmi) );

    XP_ASSERT( !!stack );

    nEntries = stack_getNEntries( stack );

    for ( which = nEntries; which >= 0; ) {
        if ( stack_getNthEntry( stack, --which, &entry ) ) {
            if ( -1 == player || inDuplicateMode || entry.playerNum == player ) {
                found = XP_TRUE;
                break;
            }
        }
        stack_freeEntry( stack, &entry );
    }

    if ( found ) { /* success? */
        XP_ASSERT( -1 == player || inDuplicateMode || player == entry.playerNum );


        XP_LOGFF( "found move %d", which );
        lmi->names[0] = model->vol.gi->players[entry.playerNum].name;
        lmi->nWinners = 1;
        lmi->moveType = entry.moveType;
        lmi->inDuplicateMode = inDuplicateMode;

        switch ( entry.moveType ) {
        case MOVE_TYPE:
            XP_ASSERT( !inDuplicateMode || entry.playerNum == DUP_PLAYER );
            lmi->nTiles = entry.u.move.moveInfo.nTiles;
            if ( 0 < entry.u.move.moveInfo.nTiles ) {
                scoreLastMove( model, xwe, &entry.u.move.moveInfo,
                               nEntries - which, lmi );
                if ( inDuplicateMode ) {
                    listHighestScores( model, lmi, &entry.u.move );
                }
            }
            break;
        case TRADE_TYPE:
            XP_ASSERT( !inDuplicateMode || entry.playerNum == DUP_PLAYER );
            lmi->nTiles = entry.u.trade.oldTiles.nTiles;
            break;
        case PHONY_TYPE:
        case ASSIGN_TYPE:
        case PAUSE_TYPE:
            break;
        default:
            XP_ASSERT( 0 );
        }
    }

    return found;
} /* model_getPlayersLastScore */

static void
loadPlayerCtxt( const ModelCtxt* model, XWStreamCtxt* stream, XP_U16 version, 
                PlayerCtxt* pc )
{
    XP_U16 nColsNBits;
#ifdef STREAM_VERS_BIGBOARD
    nColsNBits = 16 <= model_numCols( model ) ? NUMCOLS_NBITS_5
        : NUMCOLS_NBITS_4;
#else
    XP_USE(model);
    nColsNBits = NUMCOLS_NBITS_4;
#endif

    pc->curMoveValid = stream_getBits( stream, 1 );

    traySetFromStream( stream, &pc->trayTiles );

    const XP_U16 nTileBits = tilesNBits(stream);
    pc->nPending = (XP_U8)stream_getBits( stream, nTileBits );
    if ( STREAM_VERS_NUNDONE <= version ) {
        pc->nUndone = (XP_U8)stream_getBits( stream, nTileBits );
    } else {
        XP_ASSERT( 0 == pc->nUndone );
    }
    XP_ASSERT( 0 == pc->dividerLoc );
    if ( STREAM_VERS_MODELDIVIDER <= version ) {
        pc->dividerLoc = stream_getBits( stream, nTileBits );
    }

    XP_U16 nTiles = pc->nPending + pc->nUndone;
    for ( PendingTile* pt = pc->pendingTiles; nTiles-- > 0; ++pt ) {
        pt->col = (XP_U8)stream_getBits( stream, nColsNBits );
        pt->row = (XP_U8)stream_getBits( stream, nColsNBits );

        XP_U16 nBits = (version <= STREAM_VERS_RELAY) ? 6 : 7;
        pt->tile = (Tile)stream_getBits( stream, nBits );
    }

} /* loadPlayerCtxt */

static void
writePlayerCtxt( const ModelCtxt* model, XWStreamCtxt* stream, 
                 const PlayerCtxt* pc )
{
    XP_U16 nTiles;
    const PendingTile* pt;
    XP_U16 nColsNBits;
#ifdef STREAM_VERS_BIGBOARD
    nColsNBits = 16 <= model_numCols( model ) ? NUMCOLS_NBITS_5
        : NUMCOLS_NBITS_4;
#else
    XP_USE(model);
    nColsNBits = NUMCOLS_NBITS_4;
#endif

    stream_putBits( stream, 1, pc->curMoveValid );

    traySetToStream( stream, &pc->trayTiles );

    XP_U16 nBits = tilesNBits( stream );
    stream_putBits( stream, nBits, pc->nPending );
    stream_putBits( stream, nBits, pc->nUndone );
    stream_putBits( stream, nBits, pc->dividerLoc );

    nTiles = pc->nPending + pc->nUndone;
    for ( pt = pc->pendingTiles; nTiles-- > 0; ++pt ) {
        stream_putBits( stream, nColsNBits, pt->col );
        stream_putBits( stream, nColsNBits, pt->row );
        stream_putBits( stream, 7, pt->tile );
    }
} /* writePlayerCtxt */

static XP_S16
setContains( const TrayTileSet* tiles, Tile tile )
{
    XP_S16 result = -1;
    XP_S16 ii;
    /* search from top down so don't pull out of below divider */
    for ( ii = tiles->nTiles - 1; ii >= 0 ; --ii ) {
        Tile playerTile = tiles->tiles[ii];
        if ( playerTile == tile ) {
            result = ii;
            break;
        }
    }
    return result;
}

#ifdef DEBUG 
static void 
assertDiffTurn( ModelCtxt* model, XWEnv XP_UNUSED(xwe),
                XP_U16 XP_UNUSED(turn), const StackEntry* entry,
                void* closure )
{
    if ( 1 < model->nPlayers && ! model->vol.gi->inDuplicateMode ) {
        DiffTurnState* state = (DiffTurnState*)closure;
        if ( -1 != state->lastPlayerNum ) {
            XP_ASSERT( state->lastPlayerNum != entry->playerNum );
            XP_ASSERT( state->lastMoveNum + 1 == entry->moveNum );
        }
        state->lastPlayerNum = entry->playerNum;
        state->lastMoveNum = entry->moveNum;
    }
}

void
model_printTrays( const ModelCtxt* model )
{
    for ( XP_U16 ii = 0; ii < model->nPlayers; ++ii ) {
        const PlayerCtxt* player = &model->players[ii];
        XP_UCHAR buf[128];
        XP_LOGFF( "player %d: %s", ii,
                  formatTileSet( &player->trayTiles, buf, VSIZE(buf) ) );
    }
}

void
model_dumpSelf( const ModelCtxt* model, const XP_UCHAR* msg )
{
    XP_LOGFF( "(msg=%s)", msg );

    XP_UCHAR buf[256];
    XP_U16 offset = 0;

    for ( XP_U16 col = 0; col < model_numCols( model ); ++col ) {
        offset += XP_SNPRINTF( &buf[offset], VSIZE(buf) - offset,
                               "%.2d ", col );
    }
    XP_LOGF( "    %s", buf );

    for ( XP_U16 row = 0; row < model_numRows( model ); ++row ) {
        XP_UCHAR buf[256];
        XP_U16 offset = 0;

        for ( XP_U16 col = 0; col < model_numCols( model ); ++col ) {
            Tile tile = getModelTileRaw( model, col, row );
            offset += XP_SNPRINTF( &buf[offset], VSIZE(buf) - offset,
                                   "%.2x ", tile );
        }
        XP_LOGF( "%.2d: %s", row, buf );
    }
}
#endif

#ifdef CPLUS
}
#endif
