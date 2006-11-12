/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
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

#ifndef _XWCOLORS_H_
#define _XWCOLORS_H_

#include <Window.h>

enum {
    COLOR_BLACK,
    COLOR_WHITE,

    COLOR_PLAYER1,
    COLOR_PLAYER2,
    COLOR_PLAYER3,
    COLOR_PLAYER4,

    COLOR_DBL_LTTR,
    COLOR_DBL_WORD,
    COLOR_TRPL_LTTR,
    COLOR_TRPL_WORD,

    COLOR_EMPTY,
    COLOR_TILE,
    COLOR_CURSOR,       /* not read from resource */

    COLOR_NCOLORS		/* 12 */
};

typedef struct DrawingPrefs {
    IndexedColorType drawColors[COLOR_NCOLORS];
} DrawingPrefs;

#endif
