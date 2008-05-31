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
#include "vtabmgr.h"

#ifdef CPLUS
extern "C" {
#endif

enum { ASSIGN_TYPE, MOVE_TYPE, TRADE_TYPE, PHONY_TYPE };
typedef XP_U8 StackMoveType;

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
} MoveRec;

typedef struct PhonyRec {
    MoveInfo moveInfo;
} PhonyRec;

typedef union EntryData {
    AssignRec assign;
    TradeRec trade;
    MoveRec move;
    PhonyRec phony;
} EntryData;

typedef struct StackEntry {
    StackMoveType moveType;
    XP_U8 playerNum;
    XP_U8 moveNum;
    EntryData u;
} StackEntry;

typedef struct StackCtxt StackCtxt;

StackCtxt* stack_make( MPFORMAL VTableMgr* vtmgr );
void stack_destroy( StackCtxt* stack );

void stack_init( StackCtxt* stack );
void stack_setBitsPerTile( StackCtxt* stack, XP_U16 bitsPerTile );

void stack_loadFromStream( StackCtxt* stack, XWStreamCtxt* stream );
void stack_writeToStream( StackCtxt* stack, XWStreamCtxt* stream );

void stack_addMove( StackCtxt* stack, XP_U16 turn, MoveInfo* moveInfo, 
                    TrayTileSet* newTiles );
void stack_addPhony( StackCtxt* stack, XP_U16 turn, MoveInfo* moveInfo );
void stack_addTrade( StackCtxt* stack, XP_U16 turn, 
                     TrayTileSet* oldTiles, TrayTileSet* newTiles );
void stack_addAssign( StackCtxt* stack, XP_U16 turn, TrayTileSet* tiles );

XP_U16 stack_getNEntries( StackCtxt* stack );

XP_Bool stack_getNthEntry( StackCtxt* stack, XP_U16 n, StackEntry* entry );

XP_Bool stack_popEntry( StackCtxt* stack, StackEntry* entry );
void stack_redo( StackCtxt* stack );

#ifdef CPLUS
}
#endif

#endif
