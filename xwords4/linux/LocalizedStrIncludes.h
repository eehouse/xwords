/* Copyright 2001 by Eric House (xwords@eehouse.org) (fixin@peak.org).  All rights reserved.
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


/* This is the linux version of what's always been a palm file.  There's
 * probably a better way of doing this, but this is it for now.
 */

#ifndef _LOCALIZEDSTRINCLUDES_H_
#define _LOCALIZEDSTRINCLUDES_H_

enum {
    STRD_REMAINING_TILES_ADD,
    STRD_UNUSED_TILES_SUB,
    STR_COMMIT_CONFIRM,
    STR_SUBMIT_CONFIRM,
    STRD_TURN_SCORE,
    STR_BONUS_ALL,
    STR_PENDING_PLAYER,
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
    STRS_REMOTE_MOVED,

    STRS_VALUES_HEADER,
    STRD_REMAINS_HEADER,
    STRD_REMAINS_EXPL,

    STRSD_RESIGNED,
    STRSD_WINNER,
    STRDSD_PLACER,

    STR_DUP_CLIENT_SENT,
    STRDD_DUP_HOST_RECEIVED,
    STR_DUP_MOVED,
    STRD_DUP_TRADED,
    STRSD_DUP_ONESCORE,

    STR_BONUS_ALL_SUB,
    STRS_DUP_ALLSCORES,

    /* These three aren't in Android */
    STR_LOCALPLAYERS,
    STR_TOTALPLAYERS,
    STR_REMOTE,

    STR_LAST
};


#endif
