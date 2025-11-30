/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2001 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _MOVESTAK_H_
#define _MOVESTAK_H_

#include "comtypes.h"
#include "model.h"
#include "dutil.h"

#ifdef CPLUS
extern "C" {
#endif

enum { ASSIGN_TYPE, MOVE_TYPE, TRADE_TYPE, PHONY_TYPE, PAUSE_TYPE,
       /* used for debugging, and can be changed because never stored: */
       __BOGUS,
};
typedef XP_U8 StackMoveType;

#define DUP_PLAYER 0

typedef struct AssignRec {
    TrayTileSet tiles;
} AssignRec;

typedef struct TradeRec {
    TrayTileSet oldTiles;
    TrayTileSet newTiles;
} TradeRec;

typedef struct MoveRec {
    MoveInfo moveInfo;
    TrayTileSet newTiles;
    struct {
        XP_U16 nScores;
        XP_U16 scores[MAX_NUM_PLAYERS];
    } dup;
} MoveRec;

typedef struct PhonyRec {
    MoveInfo moveInfo;
} PhonyRec;

typedef struct _PauseRec {
    DupPauseType pauseType;
    XP_U32 when;
    const XP_UCHAR* msg;        /* requires stack_freeEntry() */
} PauseRec;

typedef union _EntryData {
    AssignRec assign;
    TradeRec trade;
    MoveRec move;
    PhonyRec phony;
    PauseRec pause;
} EntryData;

typedef struct _StackEntry {
    StackMoveType moveType;
    XP_U8 playerNum;
    XP_U8 moveNum;
    EntryData u;
} StackEntry;

typedef struct StackCtxt StackCtxt;

StackCtxt* stack_make( MPFORMAL XP_U16 nPlayers, XP_Bool inDuplicateMode );
void stack_destroy( StackCtxt* stack );

void stack_init( StackCtxt* stack, XP_U16 nPlayers, XP_Bool inDuplicateMode );
void stack_set7Tiles( StackCtxt* stack );
XP_U16 stack_getVersion( const StackCtxt* stack );
XP_U32 stack_getHash( const StackCtxt* stack );
void stack_setBitsPerTile( StackCtxt* stack, XP_U16 bitsPerTile );

XP_Bool stack_loadFromStream( StackCtxt* stack, XWStreamCtxt* stream );
void stack_writeToStream( const StackCtxt* stack, XWStreamCtxt* stream );
StackCtxt* stack_copy( const StackCtxt* stack );

void stack_addMove( StackCtxt* stack, XP_U16 turn, const MoveInfo* moveInfo, 
                    const TrayTileSet* newTiles );
void stack_addDupMove( StackCtxt* stack, const MoveInfo* moveInfo,
                       XP_U16 nScores, XP_U16* scores,
                       const TrayTileSet* tiles );
void stack_addPhony( StackCtxt* stack, XP_U16 turn, const MoveInfo* moveInfo );
void stack_addTrade( StackCtxt* stack, XP_U16 turn, 
                     const TrayTileSet* oldTiles, 
                     const TrayTileSet* newTiles );
void stack_addDupTrade( StackCtxt* stack, const TrayTileSet* oldTiles,
                        const TrayTileSet* newTiles );

void stack_addAssign( StackCtxt* stack, XP_U16 turn, 
                      const TrayTileSet* tiles );

void stack_addPause( StackCtxt* stack, DupPauseType pauseTYpe, XP_S16 turn,
                     XP_U32 when, const XP_UCHAR* msg );

XP_U16 stack_getNEntries( const StackCtxt* stack );

XP_Bool stack_getNthEntry( StackCtxt* stack, XP_U16 n, StackEntry* entry );

XP_Bool stack_popEntry( StackCtxt* stack, StackEntry* entry );
XP_S16 stack_getNextTurn( StackCtxt* stack );
XP_Bool stack_redo( StackCtxt* stack, StackEntry* entry );

void stack_freeEntry( StackCtxt* stack, StackEntry* entry );
    
#ifdef CPLUS
}
#endif

#endif
