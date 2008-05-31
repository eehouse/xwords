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

/* The Communications manager brokers messages between controllers  It
 * maps players to devices.
 *
 * Messages for players go through it.  If the player is remote, then the
 * message is queued.  If local, it's a simple function call executed
 * immediately.  When the caller is finished, it calls comms_okToSend()
 * or somesuch, and all the queued messages are combined into a single
 * message for each device represented, and sent.
 *
 * The problem of duplicate messages: Say there are two players A and B on
 * remote device D.  A has just made a move {{7,6,'A'},{7,7,'T'}}.  The
 * server wants to tell each player about A's move.  It will want to send the
 * same message to every player but A, yet there's no point in sending to B's
 * device since the information is already there.
 *
 * There are three possiblities: put message codes into classes -- e.g.
 */

#ifndef _COMMMGR_H_
#define _COMMMGR_H_

#include "comtypes.h" /* that's *common* types */
#include "xwstream.h"
#include "server.h"

/* typedef struct CommMgrCtxt CommMgrCtxt; */

CommMgrCtxt* commmgr_make( XWStreamCtxt* serverStream );
void commmgr_setServer( CommMgrCtxt* commMgr, ServerCtxt* server );

XWStreamCtxt* commmgr_getServerStream( CommMgrCtxt* commmgr );

void commmgr_receiveMessage( CommMgrCtxt* commmgr, XWStreamCtxt* incomming );

#endif
