/* 
 * Copyright 2020 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _CURGAMLISTWIN_H_
#define _CURGAMLISTWIN_H_

#include <stdbool.h>

#include "gamesdb.h"

typedef struct CursGameList CursGameList;

CursGameList* cgl_init( LaunchParams* params, int width, int height );
void cgl_destroy( CursGameList* cgl );

void cgl_resized( CursGameList* cgl, int width, int height );

void cgl_refresh( CursGameList* cgl );
void cgl_refreshOne( CursGameList* cgl, GameRef gr, bool select );
void cgl_remove( CursGameList* cgl, GameRef gr );

void cgl_moveSel( CursGameList* cgl, bool down );

void cgl_draw( CursGameList* cgl );

const GameRef cgl_getSel( CursGameList* cgl );
void cgl_setSel( CursGameList* cgl, int sel );
int cgl_getNGames( CursGameList* cgl );

#endif
