/* 
 * Copyright 2001 - 2022 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _GAME_H_
#define _GAME_H_

#include "comtypes.h"
#include "gameinfo.h"
#include "model.h"
#include "board.h"
#include "comms.h"
#include "contrlrp.h"
#include "util.h"
#include "gameref.h"

#ifdef CPLUS
extern "C" {
#endif

void gi_setNPlayers( XW_DUtilCtxt* dutil, XWEnv xwe, CurGameInfo* gi,
                     XP_U16 nTotal, XP_U16 nHere );
void gi_writeToStream( XWStreamCtxt* stream, const CurGameInfo* gi );
void gi_readFromStream( XWStreamCtxt* stream, CurGameInfo* gi );
XP_Bool gi_gotFromStream( XWStreamCtxt* stream, CurGameInfo* gi );
CurGameInfo gi_readFromStream2( XWStreamCtxt* stream );
void gi_copy( CurGameInfo* destGI, const CurGameInfo* srcGi );
XP_Bool gi_equal( const CurGameInfo* gi1, const CurGameInfo* gi2 );
XP_U16 gi_countLocalPlayers( const CurGameInfo* gi, XP_Bool humanOnly );
XP_U16 gi_getLocalPlayer( const CurGameInfo* gi, XP_S16 fromHint );

XP_Bool player_hasPasswd( const LocalPlayer* player );
XP_Bool player_passwordMatches( const LocalPlayer* player, const XP_UCHAR* pwd );

#ifdef CPLUS
}
#endif

#endif
