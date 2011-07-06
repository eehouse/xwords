/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2005-2009 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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
    ,XWS_SAME                 /* wildcard: means use same state as current */

    /* ,XWS_CHKCOUNTS_INIT */       /* from initial state, check if all players
                                   are here.  Success should be an error,
                                   actually: 1-device game.  */

    ,XWS_WAITING_ACKS

    /* ,XWS_CHK_ALLHERE */          /* Need to see if all expected devices/players
                                   are on board. */

    /* ,XWS_CHK_ALLHERE_2 */        /* same as above, but triggered by a reconnect
                                   rather than a connect request */

    ,XWS_EMPTY                  /* Relay's running and the object's been
                                   created, and nobody's signed up yet.  The
                                   object should never be in this state except
                                   immediately after created, just before
                                   deleted, or transitionally, as after a
                                   device is removed prior to replacing its
                                   record with another.  */

    ,XWS_WAITMORE             /* At least one device has connected, but no
                                   packets have yet arrived to be
                                   forwarded. */

    ,XWS_ALLCONND             /* All devices are connected and ready for the
                                   relay to do its work.  This is the state
                                   we're in most of the time.  */

    /* ,XWS_MISSING */              /* We've been fully connected before but lost
                                   somebody.  Once [s]he's back we can be
                                   fully connected again. */

    ,XWS_MSGONLY              /* We have no connections but still messages to
                                 send */

    /* ,XWS_ROOMCHK */              /* do we have room for as many players as are
                                 being provided */
} XW_RELAY_STATE;


/* events */
typedef enum {
    XWE_NONE

    ,XWE_ALLHERE           /* notify that all expected players are arrived */
    /* ,XWE_SOMEMISSING */       /* notify that some expected players are still missing */
    ,XWE_ALLGONE
    /* ,XWE_HAVE_ROOM */
    /* ,XWE_TOO_MANY */

    ,XWE_DEVCONNECT        /* A device is connecting using the cookie for */
    /*,XWE_HOSTCONNECT*/   /* this object, as host or guest */

    ,XWE_RECONNECT         /* A device is re-connecting using the connID for
                               this object */

    ,XWE_GOTONEACK
    ,XWE_GOTLASTACK
    ,XWE_ACKTIMEOUT

    ,XWE_DISCONN           /* disconnect socket from this game/cref */

    /* DEVGONE: a device tells us it will never connect again.  It may be the
       only device in the game or may not */
    ,XWE_DEVGONE
    /* Joined a game where a device has already generated DEVGONE */
    ,XWE_GAMEDEAD
    ,XWE_FORWARDMSG        /* A message needs forwarding */

#ifdef RELAY_HEARTBEAT
    ,XWE_HEARTRCVD         /* A heartbeat message arrived */
    ,XWE_HEARTFAILED
#endif
    ,XWE_CONNTIMER         /* timer for did we get all players hooked
                              up  */
    ,XWE_REMOVESOCKET      /* Need to remove socket from this cref */

    ,XWE_NOTIFYDISCON      /* Send a discon */

    ,XWE_NOMORESOCKETS     /* last socket's been removed */

    ,XWE_NOMOREMSGS        /* No messages are stored here for disconnected
                              hosts */
    ,XWE_SHUTDOWN          /* shutdown this game */

    ,XWE_ANY               /* wildcard; matches all */
} XW_RELAY_EVENT;


/* actions */
typedef enum {
    XWA_NONE

    ,XWA_SEND_DUP_ROOM          /* host comes in while game open */
    ,XWA_SEND_NO_ROOM           /* guest comes in when no game open */
    ,XWA_SEND_TOO_MANY

    // ,XWA_ADDDEVICE              /* got ack, so device is in for sure */
    ,XWA_NOTEACK
    ,XWA_DROPDEVICE             /* no ack; remove all traces of device */

    ,XWA_SEND_INITRSP           /* response to first to connect */
    ,XWA_SEND_CONNRSP           /* response to rest that connect */

    ,XWA_SEND_RERSP
    ,XWA_GOTALLACKS

    ,XWA_CHECK_HAVE_ROOM        /* check for number of players still sought */

    ,XWA_SENDALLHERE     /* Let all devices know we're in business */
    ,XWA_SNDALLHERE_2    /* Ditto, but for a reconnect */

    ,XWA_FWD             /* Forward a message */

    ,XWA_NOTEHEART       /* Record heartbeat received */

    ,XWA_NOTE_EMPTY      /* No sockets left; check if can delete */

    ,XWA_TIMERDISCONN  /* disconnect all because of a timer */

    ,XWA_DISCONNECT

    ,XWA_RMDEV                  /* remove the resigning */
    ,XWA_TELLGAMEDEAD         /* tell the device */

    ,XWA_NOTIFYDISCON

    ,XWA_REMOVESOCK_1           /* remove when not yet in allcond state */
    ,XWA_REMOVESOCK_2           /* remove after have reached allCond */

    ,XWA_HEARTDISCONN

    ,XWA_SHUTDOWN

} XW_RELAY_ACTION;

bool getFromTable( XW_RELAY_STATE curState, XW_RELAY_EVENT curEvent,
                   XW_RELAY_ACTION* takeAction, XW_RELAY_STATE* nextState );


const char* stateString( XW_RELAY_STATE state );
const char* eventString( XW_RELAY_EVENT evt );
const char* actString( XW_RELAY_ACTION act );

#endif
