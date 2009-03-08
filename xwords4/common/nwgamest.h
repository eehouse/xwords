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
#include "util.h"
#include "game.h"

typedef struct NewGameCtx NewGameCtx;


typedef enum {
#ifndef XWFEATURE_STANDALONE_ONLY
    NG_COL_REMOTE,
#endif
    NG_COL_NAME
    ,NG_COL_ROBOT
    ,NG_COL_PASSWD
} NewGameColumn;

typedef enum {
#ifndef XWFEATURE_STANDALONE_ONLY
    NG_ATTR_ROLE,
    NG_ATTR_CANCONFIG,
    NG_ATTR_REMHEADER,
#endif
    NG_ATTR_NPLAYERS
    ,NG_ATTR_NPLAYHEADER
    ,NG_ATTR_CANJUGGLE
} NewGameAttr;

typedef union NGValue {
    const XP_UCHAR* ng_cp;
    XP_U16 ng_u16;
    XP_Bool ng_bool;
    DeviceRole ng_role;
} NGValue;

/* Enable or disable (show or hide) controls */
typedef void (*NewGameEnableColProc)( void* closure, XP_U16 player, 
                                      NewGameColumn col, XP_TriEnable enable );
typedef void (*NewGameEnableAttrProc)( void* closure, NewGameAttr attr, 
                                       XP_TriEnable enable );
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
                       XW_UtilCtxt* util,
                       NewGameEnableColProc enableColProc, 
                       NewGameEnableAttrProc enableAttrProc, 
                       NewGameGetColProc getColProc,
                       NewGameSetColProc setColProc,
                       NewGameSetAttrProc setAttrProc,
                       void* closure );
void newg_destroy( NewGameCtx* ngc );

void newg_load( NewGameCtx* ngc, const CurGameInfo* gi );
XP_Bool newg_store( NewGameCtx* ngc, CurGameInfo* gi, XP_Bool warn );

void newg_colChanged( NewGameCtx* ngc, XP_U16 player );
void newg_attrChanged( NewGameCtx* ngc, NewGameAttr attr, 
                       NGValue value );

/** newg_juggle: Return XP_TRUE if a juggle happened, XP_FALSE if randomness
 * dictated that all players stay put.  Platforms can call repeatedly until
 * true if they want to force change.
 */
XP_Bool newg_juggle( NewGameCtx* ngc );

EXTERN_C_END

#endif /*  _NWGAMEST_H_ */
