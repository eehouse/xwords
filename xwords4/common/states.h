/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2000 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _STATES_H_
#define _STATES_H_





typedef enum {
    XWSTATE_NONE,
    XWSTATE_BEGIN,
    __UNUSED1,              /*XWSTATE_POOL_INITED,*/
    XWSTATE_NEED_SHOWSCORE,       /* client-only */
    XWSTATE_WAITING_ALL_REG,      /* includes waiting for dict from server */
    XWSTATE_RECEIVED_ALL_REG,     /* includes waiting for dict from server */
    XWSTATE_NEEDSEND_BADWORD_INFO,
    XWSTATE_MOVE_CONFIRM_WAIT,    /* client's waiting to hear back */
    XWSTATE_MOVE_CONFIRM_MUSTSEND, /* server should tell client asap */
    XWSTATE_NEEDSEND_ENDGAME,
    XWSTATE_INTURN,
    XWSTATE_GAMEOVER
    
} XW_State;

/* Game starts out in BEGIN.  If the server expects other players, it goes
 * into XWSTATE_WAITING_ALL_REG.  Likewise goes any client waiting to hear
 * from the server after sending off its info.  A stand-alone game (server)
 * goes immediately from BEGIN to WAITING_INFO.
 *
 * When a device gets tiles for all players (which happens in a single
 * message where there's communication involved) it moves to INTURN (either
 * ONDEVICE or OFFDEVICE).  ONDEVICE changes to WAITING_INFO when the device
 * sends its move to the server; OFFDEVICE changes to ONDEVICE if a
 * notification that a move's been made is received and it's now a local
 * player's turn; otherwise that notification may arrive with no change in
 * XW_State (but a change in whose turn it is.)

After a move is made (current player's device
 * sends move
 */


#endif
