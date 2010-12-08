/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2000-2001 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _POOL_H_
#define _POOL_H_

#include "comtypes.h"
#include "mempool.h"
#include "model.h"

void pool_requestTiles( PoolContext* pool, Tile* tiles, 
                        /*in out*/ XP_U8* maxNum );
void pool_replaceTiles( PoolContext* pool, TrayTileSet* tiles );
void pool_removeTiles( PoolContext* pool, TrayTileSet* tiles );

XP_U16 pool_getNTilesLeft( PoolContext* pool );
XP_U16 pool_getNTilesLeftFor( PoolContext* pool, Tile tile );

PoolContext* pool_make( MPFORMAL_NOCOMMA );

void pool_destroy( PoolContext* pool );
void pool_initFromDict( PoolContext* pool, DictionaryCtxt* dict );

void pool_writeToStream( PoolContext* pool, XWStreamCtxt* stream );
PoolContext* pool_makeFromStream( MPFORMAL XWStreamCtxt* stream );

#endif
