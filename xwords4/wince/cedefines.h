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

#define CE_BOARD_SCALEH 16
#define CE_BOARD_SCALEV 15
#define CE_MAX_ROWS 15
#define CE_BOARD_LEFT_LH 8
#define CE_BOARD_LEFT_RH 0
#define CE_BOARD_WIDTH (CE_MAX_ROWS*CE_BOARD_SCALEH)
#define CE_TRAY_LEFT_RH 0
#define CE_TRAY_LEFT_LH 0
#define CE_DIVIDER_WIDTH 3
#define CE_TIMER_WIDTH 35
#define CE_SCORE_WIDTH (4 * 51)

#define CE_TIMER_HT_HORIZ CE_SCORE_HEIGHT
#define CE_TIMER_HT_VERT CE_SCORE_WIDTH

#define CE_SCORE_TOP 0
#define CE_SCORE_HEIGHT 12
#define CE_BOARD_TOP (CE_SCORE_TOP + CE_SCORE_HEIGHT)
#define CE_SCORE_LEFT CE_BOARD_LEFT_RH
#define CE_TRAY_TOP (CE_BOARD_TOP + (CE_MAX_ROWS*CE_BOARD_SCALEV))
#define CE_TRAY_TOP_MAX CE_TRAY_TOP

#if defined TARGET_OS_WINCE
# define CE_TRAY_SCALEH 34
# define CE_TRAY_SCALEV 27
#elif defined TARGET_OS_WIN32
# define CE_TRAY_SCALEH 68
# define CE_TRAY_SCALEV 54
#endif

#define BUTTON_WIDTH 20
#define BUTTON_HEIGHT BUTTON_WIDTH

#define CE_TIMER_LEFT (CE_SCORE_WIDTH + CE_SCORE_LEFT)
#define CE_TIMER_TOP CE_SCORE_TOP
#define CE_TIMER_HEIGHT CE_SCORE_HEIGHT


#define MIN_CELL_WIDTH 10
#define MIN_CELL_HEIGHT 10
#define MIN_TRAY_HEIGHT 28
#define CE_MIN_SCORE_WIDTH 24    /* for vertical score case */

#endif
