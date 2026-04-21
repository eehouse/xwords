/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */ 
/* 
 * Copyright 2026 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _CURWINSTK_H_
#define _CURWINSTK_H_

#include <panel.h>

#include "main.h"

WINDOW* cws_newwin( CursesAppGlobals* aGlobals, int nlines, int ncols,
                    int begin_y, int begin_x );
void cws_delwin( CursesAppGlobals* aGlobals, WINDOW** winp );

#endif
