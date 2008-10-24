/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2007 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _SCOREBDP_H_
#define _SCOREBDP_H_

#include "boardp.h"

void drawScoreBoard( BoardCtxt* board );
XP_S16 figureScoreRectTapped( const BoardCtxt* board, XP_U16 x, XP_U16 y );
void drawTimer( BoardCtxt* board );

#if defined POINTER_SUPPORT || defined KEYBOARD_NAV
XP_Bool handlePenUpScore( BoardCtxt* board, XP_U16 x, XP_U16 y );
#endif

#ifdef KEYBOARD_NAV
XP_Bool moveScoreCursor( BoardCtxt* board, XP_Key key, XP_Bool preflightOnly,
                         XP_Bool* up );
#endif

#endif
