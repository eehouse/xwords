/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2002 by Eric House (xwords@eehouse.org).  All rights reserved.
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
 
#ifndef _CEDEFINES_H_
#define _CEDEFINES_H_

#define CE_MAX_ROWS 15

#define TRAY_BORDER 6
#define CELL_BORDER 3

#define MIN_CELL_WIDTH 10
#define MIN_CELL_HEIGHT 13
/* 1 below for what's subtracted from bparms->trayHeight */
#define MIN_TRAY_HEIGHT (((MIN_CELL_HEIGHT) + (TRAY_BORDER - CELL_BORDER)) + 1)

/* Favor the tray over the scoreboard. */
#define SCORE_TWEAK 2


#endif
