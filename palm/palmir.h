/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 1999 - 2001 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _PALMIR_H_
#define _PALMIR_H_

#include "comms.h"
#include "palmmain.h"

/* IR strategy.  We'll attempt to keep the IR link up, but won't bring it up
 * in the first place, or fix it once it goes down, until there's something
 * to send.
 *
 * So when there's a packet to send, if the link is up
 * (state==IR_STATE_CANDODATA) we just send it.  Otherwise we do what's
 * needed to get to that state.  What state we're actually in (in the
 * difficult case where we brought the link up before but the players have
 * had their machines separated and the link's died) will, I hope, be
 * tracable by the messages passed to each device's callback.
 *
 * This isn't going to make a game between three or four devices very easy
 * to manage, but perhaps that will have to wait until whatever's wrong with
 * shutting down and restarting links gets fixed.
 */
enum {				/* values for IR_STATE */
    IR_STATE_NONE = 0,		/* nothing's up or been done */
    IR_STATE_DISCOVERY_COMPLETE = 1,/* nothing to do til there's a message to
				   send */
    /* Discovery */
    IR_STATE_NOTHING_TO_DO = 2,	/* exists just for testing against */
    IR_STATE_NO_OTHER_FOUND = 3,	/* first discovery attempt failed */
    IR_STATE_DO_DISCOVERY = 4,
    IR_STATE_REDO_DISCOVERY = 5,
    IR_STATE_DISCOVERY_SENT = 6,
    IR_STATE_DOLAP = 7,

    /* IR Lap */
    IR_STATE_LAP_SENT = 8,
    IR_STATE_LAP_ESTAB = 9,//was    IR_STATE_DOLMP,

    /* LMP */
/*     IR_STATE_CONNECTREQ_SENT, */
    IR_STATE_LMPREQ_SENT = 10,	/* was IR_STATE_CONNECTREQ_SENT */
    IR_STATE_LMP_ESTAB = 11, //was    IR_STATE_SEND_READY,

    /* Send */
    IR_STATE_SEND_DONE = 12,
    IR_STATE_CAN_DISCONNECT = 13,

    /* receive (doesn't require even discovery) */
    IR_STATE_CONN_RECD = 14,		/* triggered by LEVENT_LAP_CON_IND */
    IR_STATE_LAP_RCV = 15,           //was    IR_STATE_DOLMP_RCV,
    IR_STATE_LMPRCV_REQ_SENT = 16,
    IR_STATE_CONN_INCOMMING = 17,    /* triggered by LEVENT_LM_CON_IND */
    IR_STATE_MESSAGE_RECD = 18
};

#define ir_work_exists(g) \
       ((g)->ir_state > IR_STATE_NOTHING_TO_DO || (getSendQueueHead(g)!=NULL))

MyIrPacket* getSendQueueHead( PalmAppGlobals* globals );

void ir_callback_out( IrConnect* con, IrCallBackParms* parms );

Boolean ir_do_work( PalmAppGlobals* globals );
void ir_show_status( PalmAppGlobals* globals );
void ir_cleanup( PalmAppGlobals* globals );

void receiveMove( ExgSocketPtr cmdPBP );
XP_Bool loadReceivedMove( PalmAppGlobals* globals, MemHandle moveData );

void palm_ir_receiveMove( PalmAppGlobals* globals, ExgSocketPtr socket );

XP_S16 palm_ir_send( const XP_U8* buf, XP_U16 len, PalmAppGlobals* globals );

#ifdef XWFEATURE_STANDALONE_ONLY
# define palm_ir_send (TransportSend)NULL
#endif

#endif
