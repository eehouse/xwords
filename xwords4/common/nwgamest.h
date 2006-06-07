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

#ifndef _NWGAMEST_H_
#define _NWGAMEST_H_

/* The new game/game info dialog is complicated, especially in non
 * XWFEATURE_STANDALONE_ONLY case.  The number of rows must be changed
 * as the number of players changes, and whether the password field is
 * enabled changes with the robot status etc.  This file encapsulates
 * all that logic, reducint the platform's role to reporting UI events
 * and reflecting state changes, as reported by callbacks, in the
 * platform's widgets.
 */

#include "comtypes.h"
EXTERN_C_START

#include "mempool.h"
#include "server.h"
#include "comms.h"
#include "game.h"

typedef struct NewGameCtx NewGameCtx;


typedef enum {
#ifndef XWFEATURE_STANDALONE_ONLY
    NG_COL_LOCAL
#endif
    ,NG_COL_NAME
    ,NG_COL_ROBOT
    ,NG_COL_PASSWD
    ,NG_NUM_COLS
} NewGameColumn;

typedef enum {
    NG_ATTR_NPLAYERS
    ,NG_ATTR_ROLE
} NewGameAttr;

typedef union NGValue {
    const XP_UCHAR* ng_cp;
    XP_U16 ng_u16;
    XP_Bool ng_bool;
    Connectedness ng_role;
} NGValue;

/* Enable or disable (show or hide) controls */
typedef void (*NewGameEnableColProc)( void* closure, XP_U16 player, 
                                      NewGameColumn col, XP_Bool enable );
typedef void (*NewGameEnableAttrProc)( void* closure, NewGameAttr attr, 
                                       XP_Bool enable );
/* Get the contents of a control.  Type of param "value" is either
   boolean or char* */
typedef void (*NgCpCallbk)( NGValue value, const void* cpClosure );
typedef void (*NewGameGetColProc)( void* closure, XP_U16 player, 
                                   NewGameColumn col, 
                                   NgCpCallbk cpcb, const void* cbClosure );
/* Set the contents of a control.  Type of param "value" is either
   boolean or char* */
typedef void (*NewGameSetColProc)( void* closure, XP_U16 player, 
                                   NewGameColumn col, const NGValue value );

typedef void (*NewGameSetAttrProc)(void* closure, NewGameAttr attr,
                                   const NGValue value );


NewGameCtx* newg_make( MPFORMAL XP_Bool isNewGame, 
                       NewGameEnableColProc enableColProc, 
                       NewGameEnableAttrProc enableAttrProc, 
                       NewGameGetColProc getColProc,
                       NewGameSetColProc setColProc,
                       NewGameSetAttrProc setAttrProc,
                       void* closure );
void newg_destroy( NewGameCtx* ngc );

void newg_load( NewGameCtx* ngc, const CurGameInfo* gi );
void newg_store( NewGameCtx* ngc, CurGameInfo* gi );

void newg_colChanged( NewGameCtx* ngc, XP_U16 player, NewGameColumn col, 
                      NGValue value );
void newg_attrChanged( NewGameCtx* ngc, NewGameAttr attr, 
                       NGValue value );

void newg_juggle( NewGameCtx* ngc );

EXTERN_C_END

#endif /*  _NWGAMEST_H_ */
