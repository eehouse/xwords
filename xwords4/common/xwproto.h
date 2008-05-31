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

#ifndef _XWPROTO_H_
#define _XWPROTO_H_




typedef enum {
    XWPROTO_ERROR = 0, /* illegal value */
    XWPROTO_CHAT,      /* reserved... */
    XWPROTO_DEVICE_REGISTRATION, /* client's first message to server */
    XWPROTO_CLIENT_SETUP, /* server's first message to client */
    XWPROTO_MOVEMADE_INFO_CLIENT, /* client reports a move it made */
    XWPROTO_MOVEMADE_INFO_SERVER, /* server tells all clients about a move
                                     made by it or another client */
    XWPROTO_UNDO_INFO_CLIENT,    /* client reports undo[s] on the device */
    XWPROTO_UNDO_INFO_SERVER,    /* server reports undos[s] happening
                                  elsewhere*/
    //XWPROTO_CLIENT_MOVE_INFO,  /* client says "I made this move" */
    //XWPROTO_SERVER_MOVE_INFO,  /* server says "Player X made this move" */
/*     XWPROTO_CLIENT_TRADE_INFO, */
/*     XWPROTO_TRADEMADE_INFO, */
    XWPROTO_BADWORD_INFO,
    XWPROTO_MOVE_CONFIRM,  /* server tells move sender that move was
                              legal */
    //XWPROTO_MOVEMADE_INFO,       /* info about tiles placed and received */
    XWPROTO_CLIENT_REQ_END_GAME,   /* non-server wants to end the game */
    XWPROTO_END_GAME               /* server says to end game */

    
} XW_Proto;

#define XWPROTO_NBITS 4

#endif
