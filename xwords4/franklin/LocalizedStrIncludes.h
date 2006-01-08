/* 
 * Copyright 1997 - 2002 by Eric House (xwords@eehouse.org). All rights reserved.
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
 
/*
 * For faking out franklin compiler for now.
 */

enum {
    STRD_REMAINING_TILES_ADD,
    STRD_UNUSED_TILES_SUB,
    STR_COMMIT_CONFIRM,
    STR_BONUS_ALL,
    STRD_TURN_SCORE,
    STR_NONLOCAL_NAME,
    STR_LOCAL_NAME,
    STRD_TIME_PENALTY_SUB,

    STRD_CUMULATIVE_SCORE,
    STRS_TRAY_AT_START,
    STRS_MOVE_DOWN,
    STRS_MOVE_ACROSS,
    STRS_NEW_TILES,
    STRSS_TRADED_FOR,
    STR_PASS,
    STR_PHONY_REJECTED,
    STRD_ROBOT_TRADED,
    STR_ROBOT_MOVED,
    STR_REMOTE_MOVED,

    STRD_TRADED,
    STRSD_SUMMARYSCORED,
    STR_PASSED,
    STR_LOSTTURN,

    STRS_VALUES_HEADER,

    STR_LAST
};
