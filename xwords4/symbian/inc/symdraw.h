// -*-mode: C; fill-column: 78; c-basic-offset: 4; -*-
/* 
 * Copyright 2005 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _SYMDRAW_H_
#define _SYMDRAW_H_

extern "C" {

#include "draw.h"
#include "board.h"

} /* extern "C" */

#if defined SERIES_60
# include <coemain.h>
# include <aknenv.h>
#elif defined SERIES_80
# include <eikenv.h>
#endif

#define scaleBoardV 13
#define scaleBoardH 15
#define scaleTrayV 37
#define scaleTrayH 32

#define CUR_PREFS_VERS 0x0405

DrawCtx* sym_drawctxt_make( MPFORMAL CWindowGc* gc, CCoeEnv* aCoeEnv, 
                            CEikonEnv* aEikonEnv, CEikApplication* aApp );

#endif
