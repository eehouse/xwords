/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2002 by Eric House (fixin@peak.org).  All rights reserved.
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

#if 0                          /* Palm-like case */

#define CE_BOARD_SCALEH 14
#define CE_BOARD_SCALEV 14
#define CE_MAX_ROWS 15
#define CE_BOARD_LEFT_LH 8
#define CE_BOARD_LEFT_RH 1
#define CE_SCORE_TOP 25
#define CE_SCORE_HEIGHT 15
#define CE_BOARD_TOP (CE_SCORE_TOP + CE_SCORE_HEIGHT)
#define CE_SCORE_LEFT CE_BOARD_LEFT_RH
#define CE_TRAY_TOP (CE_BOARD_TOP + (CE_MAX_ROWS*CE_BOARD_SCALEV))
#define CE_TRAY_TOP_MAX CE_TRAY_TOP
#define CE_BOARD_WIDTH (CE_MAX_ROWS*CE_BOARD_HSCALE)

#define CE_TRAY_LEFT_RH 0
#define CE_TRAY_LEFT_LH 0
#define CE_DIVIDER_WIDTH 3
#define CE_TRAY_SCALEH 31
#define CE_TRAY_SCALEV CE_TRAY_SCALEH

#define BUTTON_WIDTH 20
#define BUTTON_HEIGHT BUTTON_WIDTH

#define CE_TIMER_WIDTH 35
#define CE_TIMER_LEFT (CE_BOARD_WIDTH + BUTTON_WIDTH - CE_TIMER_WIDTH)
#define CE_TIMER_TOP CE_SCORE_TOP
#define CE_TIMER_HEIGHT CE_SCORE_HEIGHT

#else  /* tweaked for ce's stupid keyboard button: score on bottom */

#define CE_BOARD_SCALEH 16
#define CE_BOARD_SCALEV 15
#define CE_MAX_ROWS 15
#define CE_BOARD_LEFT_LH 8
#define CE_BOARD_LEFT_RH 0
#define CE_BOARD_TOP 0
#define CE_BOARD_WIDTH (CE_MAX_ROWS*CE_BOARD_SCALEH)
#define CE_BOARD_HEIGHT (CE_MAX_ROWS*CE_BOARD_SCALEV)

#define CE_TRAY_TOP (CE_BOARD_TOP + CE_BOARD_HEIGHT)
#define CE_TRAY_SCALEH 34
#define CE_TRAY_SCALEV 27
#define CE_TRAY_TOP_MAX CE_TRAY_TOP
#define CE_TRAY_LEFT_RH 0
#define CE_TRAY_LEFT_LH 0
#define CE_DIVIDER_WIDTH 3

#define CE_SCORE_TOP (CE_TRAY_TOP + CE_TRAY_SCALEV)
#define CE_SCORE_HEIGHT 18
#define CE_SCORE_LEFT CE_BOARD_LEFT_RH
#define CE_SCORE_WIDTH (4 * 51)

#define CE_TIMER_WIDTH 30
#define CE_TIMER_LEFT (CE_SCORE_WIDTH + CE_SCORE_LEFT)
#define CE_TIMER_RIGHT (CE_TIMER_LEFT + CE_TIMER_WIDTH)
#define CE_TIMER_TOP CE_SCORE_TOP
#define CE_TIMER_HEIGHT CE_SCORE_HEIGHT

#endif

#endif
