/* -*- fill-column: 78; compile-command: "cd ../linux && make -j3 MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 1997 - 2009 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _MODEL_H_
#define _MODEL_H_

#include "comtypes.h"
#include "dictnry.h"
#include "mempool.h"

#ifdef CPLUS
extern "C" {
#endif

#define NUMCOLS_NBITS_4 4
#if 16 < MAX_COLS && MAX_COLS <= 32
# define NUMCOLS_NBITS_5 5
#endif

#ifdef EIGHT_TILES
# define NTILES_NBITS 4
#else
# define NTILES_NBITS 3
#endif

/* Try making this 0, as some local rules, e.g. Spanish, allow.  Will need to
 * add UI to limit the number of tiles selected to that remaining in the pool.
 */
#define MIN_TRADE_TILES MAX_TRAY_TILES

/* apply to CellTile */
#define TILE_VALUE_MASK 0x003F 
#define TILE_BLANK_BIT 0x0040
#define IS_BLANK(t)  (((t)&TILE_BLANK_BIT)!= 0)
#define TILE_EMPTY_BIT 0x0080
#define TILE_PENDING_BIT 0x0100
#define PREV_MOVE_BIT 0x200

#define CELL_OWNER_MASK 0x0C00
#define CELL_OWNER_OFFSET 10
#define CELL_OWNER(t) (((t)&CELL_OWNER_MASK) >> CELL_OWNER_OFFSET)

#define MAX_UNIQUE_TILES 64 /* max tile non-blank faces */
#define MAX_NUM_BLANKS 4

/* Used by scoring code and engine as fast representation of moves. */
typedef struct MoveInfoTile {
    XP_U8 varCoord; /* 5 bits ok (0-16 for 17x17 board) */
    Tile tile;      /* 6 bits will do */
} MoveInfoTile;

typedef struct MoveInfo {
    XP_U8 nTiles;         /* 4 bits: 0-7 */
    XP_U8 commonCoord;    /* 5 bits: 0-16 if 17x17 possible */
    XP_Bool isHorizontal; /* 1 bit */
    /* If this is to go on an undo stack, we need player num here, or the code
       has to keep track of it *and* there must be exactly one entry per
       player per turn. */
    MoveInfoTile tiles[MAX_TRAY_TILES];
} MoveInfo;

typedef XP_U8 TrayTile;
typedef struct TrayTileSet {
    XP_U8 nTiles;
    TrayTile tiles[MAX_TRAY_TILES];
} TrayTileSet;

typedef struct BlankQueue {
    XP_U16 nBlanks;
    XP_U8 col[MAX_NUM_BLANKS];
    XP_U8 row[MAX_NUM_BLANKS];
} BlankQueue;

typedef XP_U8 TileBit;    /* bits indicating selection of tiles in tray */
#define ALLTILES ((TileBit)~(0xFF<<(MAX_TRAY_TILES)))

#define ILLEGAL_MOVE_SCORE (-1)

#define EMPTY_TILE TILE_EMPTY_BIT
#define TILE_IS_EMPTY(t) (((t)&TILE_EMPTY_BIT)!=0)
#define REVERSED_TILE TILE_PENDING_BIT /* reuse that bit for tile drawing
                                          only */


ModelCtxt* model_make( MPFORMAL DictionaryCtxt* dict, const PlayerDicts* dicts,
                       XW_UtilCtxt* util, XP_U16 nCols );

ModelCtxt* model_makeFromStream( MPFORMAL XWStreamCtxt* stream, 
                                 DictionaryCtxt* dict, const PlayerDicts* dicts,
                                 XW_UtilCtxt* util );

void model_writeToStream( ModelCtxt* model, XWStreamCtxt* stream );

#ifdef TEXT_MODEL
void model_writeToTextStream( const ModelCtxt* model, XWStreamCtxt* stream );
#endif

void model_setSize( ModelCtxt* model, XP_U16 boardSize );
void model_destroy( ModelCtxt* model );
void model_setNPlayers( ModelCtxt* model, XP_U16 numPlayers );
XP_U16 model_getNPlayers( const ModelCtxt* model );

void model_setDictionary( ModelCtxt* model, DictionaryCtxt* dict );
DictionaryCtxt* model_getDictionary( const ModelCtxt* model );

void model_setPlayerDicts( ModelCtxt* model, const PlayerDicts* dicts );
DictionaryCtxt* model_getPlayerDict( const ModelCtxt* model, XP_U16 playerNum );
void model_destroyDicts( ModelCtxt* model );

XP_Bool model_getTile( const ModelCtxt* model, XP_U16 col, XP_U16 row,
                       XP_Bool getPending, XP_S16 turn,
                       Tile* tile, XP_Bool* isBlank, 
                       XP_Bool* isPending, XP_Bool* isRecent );

void model_listPlacedBlanks( ModelCtxt* model, XP_U16 turn,
                             XP_Bool includePending, BlankQueue* bcp );

XP_U16 model_getCellOwner( ModelCtxt* model, XP_U16 col, XP_U16 row );

void model_assignPlayerTiles( ModelCtxt* model, XP_S16 turn, 
                              const TrayTileSet* tiles );
Tile model_getPlayerTile( const ModelCtxt* model, XP_S16 turn, XP_S16 index );

Tile model_removePlayerTile( ModelCtxt* model, XP_S16 turn, XP_S16 index );
void model_addPlayerTile( ModelCtxt* model, XP_S16 turn, XP_S16 index,
                          Tile tile );
void model_moveTileOnTray( ModelCtxt* model, XP_S16 turn, XP_S16 indexCur,
                           XP_S16 indexNew );

/* As an optimization, return a pointer to the model's array of tiles for a
   player.  Don't even think about modifying the array!!!! */
const TrayTileSet* model_getPlayerTiles( const ModelCtxt* model, XP_S16 turn );

#ifdef DEBUG
XP_UCHAR* formatTileSet( const TrayTileSet* tiles, XP_UCHAR* buf, XP_U16 len );
#endif

void model_sortTiles( ModelCtxt* model, XP_S16 turn );
XP_U16 model_getNumTilesInTray( ModelCtxt* model, XP_S16 turn );
XP_U16 model_getNumTilesTotal( ModelCtxt* model, XP_S16 turn );
void model_moveBoardToTray( ModelCtxt* model, XP_S16 turn, 
                            XP_U16 col, XP_U16 row, XP_U16 trayOffset );
void model_moveTrayToBoard( ModelCtxt* model, XP_S16 turn, XP_U16 col, 
                            XP_U16 row, XP_S16 tileIndex, Tile blankFace );
XP_Bool model_moveTileOnBoard( ModelCtxt* model, XP_S16 turn, XP_U16 colCur, 
                               XP_U16 rowCur, XP_U16 colNew, XP_U16 rowNew );
XP_Bool model_redoPendingTiles( ModelCtxt* model, XP_S16 turn );

 
XP_S16 model_trayContains( ModelCtxt* model, XP_S16 turn, Tile tile );


XP_U16 model_numRows( const ModelCtxt* model );
XP_U16 model_numCols( const ModelCtxt* model );

/* XP_U16 model_numTilesCurrentTray( ModelCtxt* model ); */
/* Tile model_currentTrayTile( ModelCtxt* model, XP_U16 index ); */
void model_addToCurrentMove( ModelCtxt* model, XP_S16 turn, 
                             XP_U16 col, XP_U16 row, 
                             Tile tile, XP_Bool isBlank );
XP_U16 model_getCurrentMoveCount( const ModelCtxt* model, XP_S16 turn );

void model_getCurrentMoveTile( ModelCtxt* model, XP_S16 turn, XP_S16* index,
                               Tile* tile, XP_U16* col, XP_U16* row, 
                               XP_Bool* isBlank );

void model_commitTurn( ModelCtxt* model, XP_S16 player, 
                       TrayTileSet* newTiles );
void model_commitRejectedPhony( ModelCtxt* model, XP_S16 player );
void model_makeTileTrade( ModelCtxt* model, XP_S16 player,
                          const TrayTileSet* oldTiles, 
                          const TrayTileSet* newTiles );

XP_Bool model_undoLatestMoves( ModelCtxt* model, PoolContext* pool, 
                               XP_U16 nMovesSought, XP_U16* turn, 
                               XP_S16* moveNum );
void model_rejectPreviousMove( ModelCtxt* model, PoolContext* pool,
                               XP_U16* turn );

void model_trayToStream( ModelCtxt* model, XP_S16 turn, 
                         XWStreamCtxt* stream );
void model_currentMoveToStream( ModelCtxt* model, XP_S16 turn, 
                                XWStreamCtxt* stream);
void model_makeTurnFromStream( ModelCtxt* model, XP_U16 playerNum,
                               XWStreamCtxt* stream );
void model_makeTurnFromMoveInfo( ModelCtxt* model, XP_U16 playerNum, 
                                 const MoveInfo* newMove );

void model_resetCurrentTurn( ModelCtxt* model, XP_S16 turn );
XP_S16 model_getNMoves( const ModelCtxt* model );

/* Are there two or more tiles visible */
XP_U16 model_visTileCount( const ModelCtxt* model, XP_U16 turn, 
                           XP_Bool trayVisible );
XP_Bool model_canShuffle( const ModelCtxt* model, XP_U16 turn, 
                          XP_Bool trayVisible );
XP_Bool model_canTogglePending( const ModelCtxt* model, XP_U16 turn );

/********************* notification ********************/
typedef void (*BoardListener)(void* data, XP_U16 turn, XP_U16 col, 
                              XP_U16 row, XP_Bool added );
void model_setBoardListener( ModelCtxt* model, BoardListener bl, 
                             void* data );
typedef void (*TrayListener)( void* data, XP_U16 turn, 
                              XP_S16 index1, XP_S16 index2 );
void model_setTrayListener( ModelCtxt* model, TrayListener bl, 
                            void* data );
typedef void (*DictListener)( void* data, XP_S16 playerNum, 
                              const DictionaryCtxt* oldDict,
                              const DictionaryCtxt* newDict );
void model_setDictListener( ModelCtxt* model, DictListener dl, 
                            void* data );
void model_foreachPendingCell( ModelCtxt* model, XP_S16 turn,
                               BoardListener bl, void* data );
void model_foreachPrevCell( ModelCtxt* model, BoardListener bl, void* data );

void model_writeGameHistory( ModelCtxt* model, XWStreamCtxt* stream,
                             ServerCtxt* server, /* for player names */
                             XP_Bool gameOver );

/* for the tile values dialog: total all the tiles in players trays and
   tentatively placed on the board. */
void model_countAllTrayTiles( ModelCtxt* model, XP_U16* counts, 
                              XP_S16 excludePlayer );

/********************* scoring ********************/

typedef XP_Bool (*WordNotifierProc)( const XP_UCHAR* word, XP_Bool isLegal, 
#ifdef XWFEATURE_BOARDWORDS
                                     const MoveInfo* movei, XP_U16 start, 
                                     XP_U16 end,
#endif
                                     void* closure );
typedef struct WordNotifierInfo {
    WordNotifierProc proc;
    void* closure;
} WordNotifierInfo;

XP_Bool getCurrentMoveScoreIfLegal( ModelCtxt* model, XP_S16 turn, 
                                    XWStreamCtxt* stream, 
                                    WordNotifierInfo* wni, XP_S16* score );
XP_S16 model_getPlayerScore( ModelCtxt* model, XP_S16 player );

XP_Bool model_getPlayersLastScore( ModelCtxt* model, XP_S16 player,
                                   XP_UCHAR* expl, XP_U16* explLen );
#ifdef XWFEATURE_BOARDWORDS
void model_listWordsThrough( ModelCtxt* model, XP_U16 col, XP_U16 row,
                             XWStreamCtxt* stream );
#endif

/* Have there been too many passes (so game should end)? */
XP_Bool model_recentPassCountOk( ModelCtxt* model );

XWBonusType model_getSquareBonus( const ModelCtxt* model,
                                  XP_U16 col, XP_U16 row );
#ifdef STREAM_VERS_BIGBOARD
void model_setSquareBonuses( ModelCtxt* model, XWBonusType* bonuses, 
                             XP_U16 nBonuses );
#endif
                                  
XP_Bool model_checkMoveLegal( ModelCtxt* model, XP_S16 player, 
                              XWStreamCtxt* stream,
                              WordNotifierInfo* notifyInfo );

typedef struct _ScoresArray { XP_S16 arr[MAX_NUM_PLAYERS]; } ScoresArray;
void model_figureFinalScores( ModelCtxt* model, ScoresArray* scores,
                              ScoresArray* tilePenalties );

/* figureMoveScore is meant only for the engine's use */
XP_U16 figureMoveScore( const ModelCtxt* model, XP_U16 turn, MoveInfo* mvInfo, 
                        EngineCtxt* engine, XWStreamCtxt* stream, 
                        WordNotifierInfo* notifyInfo );

/* tap into internal WordNotifierInfo */
WordNotifierInfo* model_initWordCounter( ModelCtxt* model, XWStreamCtxt* stream );

/********************* persistence ********************/
#ifdef INCLUDE_IO_SUPPORT
void model_load( ModelCtxt* model, XP_Stream* inStream );
void model_store( ModelCtxt* model, XP_Stream* outStream );
#endif


/* a utility function needed by server too.  Not a clean design, this. */
void model_packTilesUtil( ModelCtxt* model, PoolContext* pool,
                          XP_Bool includeBlank, 
                          XP_U16* nUsed, const XP_UCHAR** texts,
                          Tile* tiles );


#ifdef CPLUS
}
#endif

#endif

