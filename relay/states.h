/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2005 by Eric House (fixin@peak.org).  All rights reserved.
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


/* states */
typedef
enum {
    XW_ST_NONE

    ,XW_ST_CHKCOUNTS_INIT       /* from initial state, check if all players
                                   are here.  Success should be an error,
                                   actually: 1-device game.  */

    ,XW_ST_CHKCOUNTS_MISS       /* from the missing state */
    ,XW_ST_CHKCOUNTS            /* check from any other state */

    ,XW_ST_CHK_ALLHERE          /* Need to see if all expected devices/players
                                   are on board. */

    ,XW_ST_CHK_2ND_ALLHERE      /* same as above, but triggered by a reconnect
                                   rather than a connect request */

    ,XW_ST_INITED               /* Relay's running and the object's been
                                   created, but nobody's signed up yet.  This
                                   is a very short-lived state since an
                                   incoming connection is why the object was
                                   created.  */

    ,XW_ST_CONNECTING           /* At least one device has connected, but no
                                   packets have yet arrived to be
                                   forwarded. */

    ,XW_ST_CHECKING_CONN        /* While we're still not fully connected a
                                   message comes in */

    ,XW_ST_ALLCONNECTED         /* All devices are connected and ready for the
                                   relay to do its work.  This is the state
                                   we're in most of the time.  */

    ,XW_ST_MISSING              /* We've been fully connected before but lost
                                   somebody.  Once [s]he's back we can be
                                   fully connected again. */

    ,XW_ST_WAITING_RECON        /* At least one device has been timed out or
                                   sent a disconnect message.  We can't flow
                                   messages in this state, and will be killing
                                   all connections if we don't hear back from
                                   the missing guy soon.  */

    ,XW_ST_CHECKINGDEST         /* Checking for valid socket */

    ,XW_ST_CHECKING_CAN_LOCK    /* Is this message one that implies all
                                   players are present? */

    ,XW_ST_DEAD                 /* About to kill the object */
} XW_RELAY_STATE;


/* events */
typedef enum {
    XW_EVT_NONE

    ,XW_EVT_OKTOSEND
    ,XW_EVT_COUNTSBAD

    ,XW_EVT_ALLHERE             /* notify that all expected players are arrived */
    ,XW_EVT_SOMEMISSING         /* notify that some expected players are still missing */

    ,XW_EVT_CONNECTMSG        /* A device is connecting using the cookie for
                                   this object */

    ,XW_EVT_RECONNECTMSG      /* A device is re-connecting using the
                                   connID for this object */

    ,XW_EVT_DISCONNECTMSG     /* disconnect socket from this game/cref */

    ,XW_EVT_FORWARDMSG        /* A message needs forwarding */

    ,XW_EVT_HEARTRCVD         /* A heartbeat message arrived */

    ,XW_EVT_CONNTIMER         /* timer for did we get all players hooked
                                   up  */

    ,XW_EVT_DESTOK

    ,XW_EVT_DESTBAD

    ,XW_EVT_CAN_LOCK          /* ready to stop allowing new connections */
    ,XW_EVT_CANT_LOCK         /* can't disallow new connections yet  */

    ,XW_EVT_HEARTFAILED

    ,XW_EVT_REMOVESOCKET      /* Need to remove socket from this cref */

    ,XW_EVT_NOTIFYDISCON      /* Send a discon */

    ,XW_EVT_NOMORESOCKETS     /* last socket's been removed */

    ,XW_EVT_ANY               /* wildcard; matches all */
} XW_RELAY_EVENT;


/* actions */
typedef enum {
    XW_ACT_NONE

    ,XW_ACT_SEND_1ST_RSP
    ,XW_ACT_SEND_1ST_RERSP

    ,XW_ACT_CHKCOUNTS

    ,XW_ACT_REJECT

    ,XW_ACT_SEND_RSP        /* Send a connection response */
    ,XW_ACT_SEND_RERSP

    ,XW_ACT_SENDALLHERE     /* Let all devices know we're in business */
    ,XW_ACT_2ND_SNDALLHERE

    ,XW_ACT_FWD             /* Forward a message */

    ,XW_ACT_NOTEHEART       /* Record heartbeat received */

    ,XW_ACT_DISCONNECTALL
    ,XW_ACT_TIMERDISCONNECT  /* disconnect all because of a timer */

    ,XW_ACT_CHECKDEST        /* check that a given hostID has a socket */

    ,XW_ACT_DISCONNECT

    ,XW_ACT_NOTIFYDISCON

    ,XW_ACT_REMOVESOCKET

    ,XW_ACT_CHECK_CAN_LOCK    /* check whether this message implies all
                                    expected players present */

    ,XW_ACT_HEARTDISCONNECT

} XW_RELAY_ACTION;

int getFromTable( XW_RELAY_STATE curState, XW_RELAY_EVENT curEvent,
                  XW_RELAY_ACTION* takeAction, XW_RELAY_STATE* nextState );


char* stateString( XW_RELAY_STATE state );
char* eventString( XW_RELAY_EVENT evt );
char* actString( XW_RELAY_ACTION act );

#endif
