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
#include "dutil.h"

#ifdef CPLUS
extern "C" {
#endif

#define NUMCOLS_NBITS_4 4
#if 16 < MAX_COLS && MAX_COLS <= 32
# define NUMCOLS_NBITS_5 5
#endif

#define NTILES_NBITS_7 3
#define NTILES_NBITS_9 4

/* apply to CellTile */
#define TILE_VALUE_MASK 0x003F 
#define TILE_BLANK_BIT 0x0040
#define IS_BLANK(t)  (((t)&TILE_BLANK_BIT)!= 0)
#define TILE_EMPTY_BIT 0x0080
#define TILE_PENDING_BIT 0x0100
#define PREV_MOVE_BIT 0x200

#define CELL_OWNER_OFFSET 10
#define CELL_OWNER_MASK (0x0003 << CELL_OWNER_OFFSET)
#define CELL_OWNER(t) (((t)&CELL_OWNER_MASK) >> CELL_OWNER_OFFSET)

#define MAX_UNIQUE_TILES 64 /* max tile non-blank faces */
/* Portuguese has 3 for a 15x15 game; can go higher on larger boards */
#define MAX_NUM_BLANKS 6

typedef struct BlankQueue {
    XP_U16 nBlanks;
    XP_U8 col[MAX_NUM_BLANKS];
    XP_U8 row[MAX_NUM_BLANKS];
} BlankQueue;

#define ALLTILES ((TileBit)~(0xFF<<(MAX_TRAY_TILES)))

#define ILLEGAL_MOVE_SCORE (-1)

#define EMPTY_TILE TILE_EMPTY_BIT
#define TILE_IS_EMPTY(t) (((t)&TILE_EMPTY_BIT)!=0)
#define REVERSED_TILE TILE_PENDING_BIT /* reuse that bit for tile drawing
                                          only */


ModelCtxt* model_make( XWEnv xwe, const DictionaryCtxt* dict,
                       const PlayerDicts* dicts, XW_UtilCtxt** utilp, XP_U16 nCols );

ModelCtxt* model_makeFromStream( XWEnv xwe, XWStreamCtxt* stream,
                                 const DictionaryCtxt* dict, const PlayerDicts* dicts,
                                 XW_UtilCtxt** utilp );

void model_writeToStream( const ModelCtxt* model, XWEnv xwe, XWStreamCtxt* stream );

#ifdef TEXT_MODEL
void model_writeToTextStream( const ModelCtxt* model, XWStreamCtxt* stream );
#endif

void model_setSize( ModelCtxt* model, XP_U16 boardSize );
void model_forceStack7Tiles( ModelCtxt* model );
void model_destroy( ModelCtxt* model, XWEnv xwe );
XP_U32 model_getHash( const ModelCtxt* model );
XP_Bool model_hashMatches( const ModelCtxt* model, XP_U32 hash );
XP_Bool model_popToHash( ModelCtxt* model, XWEnv xwe, const XP_U32 hash,
                         PoolContext* pool );

void model_setNPlayers( ModelCtxt* model, XP_U16 numPlayers );
XP_U16 model_getNPlayers( const ModelCtxt* model );

void model_setDictionary( ModelCtxt* model, XWEnv xwe, const DictionaryCtxt* dict );
const DictionaryCtxt* model_getDictionary( const ModelCtxt* model );

void model_setPlayerDicts( ModelCtxt* model, XWEnv xwe, const PlayerDicts* dicts );
const DictionaryCtxt* model_getPlayerDict( const ModelCtxt* model, XP_S16 playerNum );

XP_Bool model_getTile( const ModelCtxt* model, XP_U16 col, XP_U16 row,
                       XP_Bool getPending, XP_S16 turn,
                       Tile* tile, XP_Bool* isBlank, 
                       XP_Bool* isPending, XP_Bool* isRecent );

void model_listPlacedBlanks( ModelCtxt* model, XP_U16 turn,
                             XP_Bool includePending, BlankQueue* bcp );

XP_U16 model_getCellOwner( ModelCtxt* model, XP_U16 col, XP_U16 row );
void model_addNewTiles( ModelCtxt* model, XP_S16 turn,
                        const TrayTileSet* tiles );
void model_assignPlayerTiles( ModelCtxt* model, XP_S16 turn, 
                              const TrayTileSet* tiles );
void model_assignDupeTiles( ModelCtxt* model, XWEnv xwe, const TrayTileSet* tiles );

Tile model_getPlayerTile( const ModelCtxt* model, XP_S16 turn, XP_S16 index );

void model_setSecondsUsed( ModelCtxt* model, XP_U16 turn, XP_U16 newSeconds );
void model_augmentSecondsUsed( ModelCtxt* model, XP_U16 turn, XP_U16 bySeconds );
XP_U32 model_getSecondsUsed( const ModelCtxt* model, XP_U16 turn );
XP_U16 model_timePenalty( const ModelCtxt* model, XP_U16 playerNum );

Tile model_removePlayerTile( ModelCtxt* model, XP_S16 turn, XP_S16 index );
void model_removePlayerTiles( ModelCtxt* model, XP_S16 turn, const TrayTileSet* tiles );
void model_addPlayerTile( ModelCtxt* model, XP_S16 turn, XP_S16 index,
                          Tile tile );
void model_moveTileOnTray( ModelCtxt* model, XP_S16 turn, XP_S16 indexCur,
                           XP_S16 indexNew );
XP_U16 model_getDividerLoc( const ModelCtxt* model, XP_S16 turn );
void model_setDividerLoc( ModelCtxt* model, XP_S16 turn, XP_U16 loc );

/* As an optimization, return a pointer to the model's array of tiles for a
   player.  Don't even think about modifying the array!!!! */
const TrayTileSet* model_getPlayerTiles( const ModelCtxt* model, XP_S16 turn );

#ifdef DEBUG
XP_UCHAR* formatTileSet( const TrayTileSet* tiles, XP_UCHAR* buf, XP_U16 len );
void model_printTrays( const ModelCtxt* model );
#endif

void model_sortTiles( ModelCtxt* model, XP_S16 turn );
XP_U16 model_getNumTilesInTray( ModelCtxt* model, XP_S16 turn );
XP_U16 model_getNumTilesTotal( ModelCtxt* model, XP_S16 turn );
void model_moveBoardToTray( ModelCtxt* model, XWEnv xwe, XP_S16 turn,
                            XP_U16 col, XP_U16 row, XP_U16 trayOffset );
void model_moveTrayToBoard( ModelCtxt* model, XWEnv xwe, XP_S16 turn, XP_U16 col,
                            XP_U16 row, XP_S16 tileIndex, Tile blankFace );
XP_Bool model_moveTileOnBoard( ModelCtxt* model, XWEnv xwe, XP_S16 turn,
                               XP_U16 colCur,  XP_U16 rowCur, XP_U16 colNew,
                               XP_U16 rowNew );
XP_Bool model_redoPendingTiles( ModelCtxt* model, XWEnv xwe, XP_S16 turn );
XP_Bool model_setBlankValue( ModelCtxt* model, XP_U16 XP_UNUSED(player),
                             XP_U16 col, XP_U16 row, XP_U16 tileIndex );
 
XP_S16 model_trayContains( ModelCtxt* model, XP_S16 turn, Tile tile );


XP_U16 model_numRows( const ModelCtxt* model );
XP_U16 model_numCols( const ModelCtxt* model );

/* XP_U16 model_numTilesCurrentTray( ModelCtxt* model ); */
/* Tile model_currentTrayTile( ModelCtxt* model, XP_U16 index ); */
void model_addToCurrentMove( ModelCtxt* model, XP_S16 turn, 
                             XP_U16 col, XP_U16 row, 
                             Tile tile, XP_Bool isBlank );
XP_U16 model_getCurrentMoveCount( const ModelCtxt* model, XP_S16 turn );

XP_Bool model_getCurrentMoveIsVertical( const ModelCtxt* model, XP_S16 turn,
                                        XP_Bool* isHorizontal );

void model_getCurrentMoveTile( const ModelCtxt* model, XP_S16 turn, XP_S16* index,
                               Tile* tile, XP_U16* col, XP_U16* row, 
                               XP_Bool* isBlank );

XP_Bool model_commitTurn( ModelCtxt* model, XWEnv xwe, XP_S16 player,
                          TrayTileSet* newTiles );
void model_commitDupeTurn( ModelCtxt* model, XWEnv xwe,
                           const MoveInfo* moveInfo,
                           XP_U16 nScores, XP_U16* scores,
                           TrayTileSet* newTiles );
void model_commitDupeTrade( ModelCtxt* model, const TrayTileSet* oldTiles,
                            const TrayTileSet* newTiles );
void model_noteDupePause( ModelCtxt* model, XWEnv xwe, DupPauseType typ,
                          XP_S16 turn, const XP_UCHAR* msg );
void model_cloneDupeTrays( ModelCtxt* model, XWEnv xwe );

void model_commitRejectedPhony( ModelCtxt* model, XP_S16 player );
void model_makeTileTrade( ModelCtxt* model, XP_S16 player,
                          const TrayTileSet* oldTiles, 
                          const TrayTileSet* newTiles );
XP_Bool model_canUndo( const ModelCtxt* model );
XP_Bool model_undoLatestMoves( ModelCtxt* model, XWEnv xwe, PoolContext* pool,
                               XP_U16 nMovesSought, XP_U16* turn, 
                               XP_S16* moveNum );
void model_rejectPreviousMove( ModelCtxt* model, XWEnv xwe,
                               PoolContext* pool, XP_U16* turn );

void model_trayToStream( const ModelCtxt* model, XP_S16 turn,
                         XWStreamCtxt* stream );
void model_currentMoveToStream( const ModelCtxt* model, XP_S16 turn,
                                XWStreamCtxt* stream );
void model_currentMoveToMoveInfo( const ModelCtxt* model, XP_S16 turn,
                                  MoveInfo* moveInfo );
XP_Bool model_makeTurnFromStream( ModelCtxt* model, XWEnv xwe, XP_U16 playerNum,
                                  XWStreamCtxt* stream );
void model_makeTurnFromMoveInfo( ModelCtxt* model, XWEnv xwe, XP_U16 playerNum,
                                 const MoveInfo* newMove );

void model_chatReceived( ModelCtxt* model, XWEnv xwe, XP_UCHAR* msg,
                         XP_S16 from, XP_U32 timestamp );
XP_U16 model_countChats( ModelCtxt* model );
void model_getChat( ModelCtxt* model, XP_U16 nn, XP_UCHAR* buf, XP_U16* bufLen,
                    XP_S16* from, XP_U32* timestamp );
void model_addChat( ModelCtxt* model, XWEnv xwe, const XP_UCHAR* msg, XP_S16 from,
                    XP_U32 timestamp );
void model_deleteChats( ModelCtxt* model );
#ifdef DEBUG
void juggleMoveIfDebug( MoveInfo* move );
void model_dumpSelf( const ModelCtxt* model, const XP_UCHAR* msg );
#else
# define juggleMoveIfDebug(newMove)
# define model_dumpSelf( model, msg )
#endif

#ifdef XWFEATURE_ROBOTPHONIES
void reverseTiles( MoveInfo* move );
#endif

void model_resetCurrentTurn( ModelCtxt* model, XWEnv xwe, XP_S16 turn );
XP_S16 model_getNMoves( const ModelCtxt* model );

/* Are there two or more tiles visible */
XP_U16 model_visTileCount( const ModelCtxt* model, XP_U16 turn, 
                           XP_Bool trayVisible );
XP_Bool model_canShuffle( const ModelCtxt* model, XP_U16 turn, 
                          XP_Bool trayVisible );
XP_Bool model_canTogglePending( const ModelCtxt* model, XP_U16 turn );

/********************* notification ********************/
typedef void (*BoardListener)( XWEnv xwe, void* data, XP_U16 turn, XP_U16 col,
                               XP_U16 row, XP_Bool added );
void model_setBoardListener( ModelCtxt* model, BoardListener bl, 
                             void* data );
typedef void (*TrayListener)( void* data, XP_U16 turn, 
                              XP_S16 index1, XP_S16 index2 );
void model_setTrayListener( ModelCtxt* model, TrayListener bl, 
                            void* data );
typedef void (*DictListener)( void* data, XWEnv xwe, XP_S16 playerNum,
                              const DictionaryCtxt* oldDict,
                              const DictionaryCtxt* newDict );
void model_setDictListener( ModelCtxt* model, DictListener dl, 
                            void* data );
void model_foreachPendingCell( ModelCtxt* model, XWEnv xwe, XP_S16 turn,
                               BoardListener bl, void* data );
void model_foreachPrevCell( ModelCtxt* model, XWEnv xwe, BoardListener bl, void* data );

void model_writeGameHistory( ModelCtxt* model, XWEnv xwe, XWStreamCtxt* stream,
                             CtrlrCtxt* ctrlr, /* for player names */
                             XP_Bool gameOver );

/* for the tile values dialog: total all the tiles in players trays and
   tentatively placed on the board. */
void model_countAllTrayTiles( ModelCtxt* model, XP_U16* counts, 
                              XP_S16 excludePlayer );

/********************* scoring ********************/

typedef struct _WNParams {
    const XP_UCHAR* word;
    XP_Bool isLegal;
    const DictionaryCtxt* dict;
#ifdef XWFEATURE_BOARDWORDS
    const MoveInfo* movei;
    XP_U16 start;
    XP_U16 end;
#endif
} WNParams;

typedef void (*WordNotifierProc)( const WNParams* wnp, void* closure );
typedef struct WordNotifierInfo {
    WordNotifierProc proc;
    void* closure;
} WordNotifierInfo;

XP_Bool getCurrentMoveScoreIfLegal( ModelCtxt* model, XWEnv xwe,
                                    XP_S16 turn, XWStreamCtxt* stream,
                                    WordNotifierInfo* wni, XP_S16* score );
XP_S16 model_getPlayerScore( const ModelCtxt* model, XP_S16 player );

XP_Bool model_getPlayersLastScore( ModelCtxt* model, XWEnv xwe, XP_S16 player,
                                   LastMoveInfo* info );
#ifdef XWFEATURE_BOARDWORDS
XP_Bool model_listWordsThrough( ModelCtxt* model, XWEnv xwe, XP_U16 col,
                                XP_U16 row, XP_S16 turn, XWStreamCtxt* stream );
#endif

/* Have there been too many passes (so game should end)? */
XP_Bool model_recentPassCountOk( ModelCtxt* model );

XWBonusType model_getSquareBonus( const ModelCtxt* model,
                                  XP_U16 col, XP_U16 row );
#ifdef STREAM_VERS_BIGBOARD
void model_setSquareBonuses( ModelCtxt* model, XWBonusType* bonuses, 
                             XP_U16 nBonuses );
#endif
                                  
XP_Bool model_checkMoveLegal( ModelCtxt* model, XWEnv xwe, XP_S16 player,
                              XWStreamCtxt* stream,
                              WordNotifierInfo* notifyInfo );

void model_figureFinalScores( const ModelCtxt* model, ScoresArray* scores,
                              ScoresArray* tilePenalties );

void model_getCurScores( const ModelCtxt* model, ScoresArray* scores,
                         XP_Bool gameOver );

/* figureMoveScore is meant only for the engine's use */
XP_U16 figureMoveScore( const ModelCtxt* model, XWEnv xwe, XP_U16 turn,
                        const MoveInfo* mvInfo, EngineCtxt* engine,
                        XWStreamCtxt* stream, WordNotifierInfo* notifyInfo );

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

Tile model_askBlankTile( ModelCtxt* model, XWEnv xwe, XP_U16 turn,
                         XP_U16 col, XP_U16 row);

XP_S16 model_getNextTurn( const ModelCtxt* model );

#ifdef CPLUS
}
#endif

#endif

