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
    XWS_NONE
    ,XWS_ANY                  /* wildcard */

    ,XWS_CHKCOUNTS_INIT       /* from initial state, check if all players
                                   are here.  Success should be an error,
                                   actually: 1-device game.  */

    ,XWS_CHKCOUNTS_MISS       /* from the missing state */
    ,XWS_CHKCOUNTS            /* check from any other state */

    ,XWS_CHK_ALLHERE          /* Need to see if all expected devices/players
                                   are on board. */

    ,XWS_CHK_ALLHERE_2        /* same as above, but triggered by a reconnect
                                   rather than a connect request */

    ,XWS_INITED               /* Relay's running and the object's been
                                   created, but nobody's signed up yet.  This
                                   is a very short-lived state since an
                                   incoming connection is why the object was
                                   created.  */

    ,XWS_CONNECTING           /* At least one device has connected, but no
                                   packets have yet arrived to be
                                   forwarded. */

    ,XWS_CHECKING_CONN        /* While we're still not fully connected a
                                   message comes in */

    ,XWS_ALLCONNECTED         /* All devices are connected and ready for the
                                   relay to do its work.  This is the state
                                   we're in most of the time.  */

    ,XWS_MISSING              /* We've been fully connected before but lost
                                   somebody.  Once [s]he's back we can be
                                   fully connected again. */

    ,XWS_WAITING_RECON        /* At least one device has been timed out or
                                   sent a disconnect message.  We can't flow
                                   messages in this state, and will be killing
                                   all connections if we don't hear back from
                                   the missing guy soon.  */

    ,XWS_CHECKINGDEST         /* Checking for valid socket */

    ,XWS_CHECKING_CAN_LOCK    /* Is this message one that implies all
                                   players are present? */

    ,XWS_DEAD                 /* About to kill the object */
} XW_RELAY_STATE;


/* events */
typedef enum {
    XWE_NONE

    ,XWE_OKTOSEND
    ,XWE_COUNTSBAD

    ,XWE_ALLHERE             /* notify that all expected players are arrived */
    ,XWE_SOMEMISSING         /* notify that some expected players are still missing */

    ,XWE_CONNECTMSG        /* A device is connecting using the cookie for
                                   this object */

    ,XWE_RECONNECTMSG      /* A device is re-connecting using the
                                   connID for this object */

    ,XWE_DISCONNMSG     /* disconnect socket from this game/cref */

    ,XWE_FORWARDMSG        /* A message needs forwarding */

    ,XWE_HEARTRCVD         /* A heartbeat message arrived */

    ,XWE_CONNTIMER         /* timer for did we get all players hooked
                                   up  */
    ,XWE_HEARTFAILED

    ,XWE_REMOVESOCKET      /* Need to remove socket from this cref */

    ,XWE_NOTIFYDISCON      /* Send a discon */

    ,XWE_NOMORESOCKETS     /* last socket's been removed */

    ,XWE_SHUTDOWN          /* shutdown this game */

    ,XWE_ANY               /* wildcard; matches all */
} XW_RELAY_EVENT;


/* actions */
typedef enum {
    XWA_NONE

    ,XWA_SEND_1ST_RSP
    ,XWA_SEND_1ST_RERSP

    ,XWA_CHKCOUNTS

    ,XWA_REJECT

    ,XWA_SEND_RSP        /* Send a connection response */
    ,XWA_SEND_RERSP

    ,XWA_SENDALLHERE     /* Let all devices know we're in business */
    ,XWA_SNDALLHERE_2    /* Ditto, but for a reconnect */

    ,XWA_FWD             /* Forward a message */

    ,XWA_NOTEHEART       /* Record heartbeat received */

    ,XWA_TIMERDISCONN  /* disconnect all because of a timer */

    ,XWA_DISCONNECT

    ,XWA_NOTIFYDISCON

    ,XWA_REMOVESOCKET

    ,XWA_HEARTDISCONN

    ,XWA_SHUTDOWN

} XW_RELAY_ACTION;

int getFromTable( XW_RELAY_STATE curState, XW_RELAY_EVENT curEvent,
                  XW_RELAY_ACTION* takeAction, XW_RELAY_STATE* nextState );


char* stateString( XW_RELAY_STATE state );
char* eventString( XW_RELAY_EVENT evt );
char* actString( XW_RELAY_ACTION act );

#endif
