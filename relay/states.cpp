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

#include "assert.h"
#include "states.h"
#include "xwrelay_priv.h"

typedef struct StateTable {
    XW_RELAY_STATE   stateStart;
    XW_RELAY_EVENT   stateEvent;
    XW_RELAY_ACTION  stateAction;
    XW_RELAY_STATE   stateEnd;  /* Do I need this?  Or does the code that
                                   performs the action determine the state? */
} StateTable;

/* Connecting.  The problem is that we don't know how many devices to expect.
   So hosts connect and get a response.  Hosts send messages to be forwarded.
   We may or may not have an ID for the recipient host.  If we do, we forward.
   If we don't, we drop.  No big deal.  So is there any difference between
   CONNECTING and CONNECTED?  I don't think so.  Messages come in and we try
   to forward.  Connection requests come in and we accept them if the host in
   question is unknown.  NOT QUITE.  There's a DOS vulnerability there.  It's
   best if we can put the game in a state where others can't connect, if the
   window where new devices can sign in using a given cookie is fairly small.

   New rules on accepting connections and reconnections:

   - Connect and reconnect messages contain nPlayersHere (local) and
     nPlayersTotal params.

   - On connect action, we either note the total expected, or increase the
     total we have.  Ditto on reconnect.  On disconnect, we take the departing
     hosts marbles away from the game.

   - We only accept [re]connections when we're missing players, and we only
     forward messages when all devices/players are accounted for.  There's a
     notification sent when the last shows up, so devices can send pending
     messages at that time.

   - There's at least one bug with this scheme: we don't know how many players
     to expect until the server shows up, so we'll keep accepting clients
     until that happens.  Probably need a config variable that sets an upper
     bound on the number of players.
 */

StateTable g_stateTable[] = {

{ XWS_INITED,         XWE_CONNECTMSG,    XWA_CHKCOUNTS,    XWS_CHKCOUNTS_INIT },
{ XWS_CHKCOUNTS_INIT, XWE_OKTOSEND,      XWA_SEND_1ST_RSP, XWS_CHK_ALLHERE },
{ XWS_CHKCOUNTS_INIT, XWE_COUNTSBAD,     XWA_REJECT,       XWS_INITED },
{ XWS_CONNECTING,     XWE_CONNECTMSG,    XWA_CHKCOUNTS,    XWS_CHKCOUNTS },
{ XWS_CHKCOUNTS,      XWE_OKTOSEND,      XWA_SEND_1ST_RSP, XWS_CHK_ALLHERE },
{ XWS_CHKCOUNTS,      XWE_COUNTSBAD,     XWA_REJECT,       XWS_CONNECTING },

{ XWS_MISSING,        XWE_CONNECTMSG,    XWA_CHKCOUNTS,    XWS_CHKCOUNTS_MISS },
{ XWS_CHKCOUNTS_MISS, XWE_OKTOSEND,      XWA_SEND_1ST_RSP, XWS_CHK_ALLHERE_2 },
{ XWS_CHKCOUNTS_MISS, XWE_COUNTSBAD,     XWA_REJECT,       XWS_MISSING },


{ XWS_CONNECTING,     XWE_CONNECTMSG,    XWA_SEND_RSP,     XWS_CHK_ALLHERE },
{ XWS_CHK_ALLHERE,    XWE_ALLHERE,       XWA_SENDALLHERE,  XWS_ALLCONNECTED },
{ XWS_CHK_ALLHERE,    XWE_SOMEMISSING,   XWA_NONE,         XWS_CONNECTING },

{ XWS_ALLCONNECTED,   XWE_DISCONNMSG,    XWA_DISCONNECT,   XWS_MISSING },
{ XWS_CONNECTING,     XWE_DISCONNMSG,    XWA_DISCONNECT,   XWS_CONNECTING },
{ XWS_MISSING,        XWE_DISCONNMSG,    XWA_DISCONNECT,   XWS_MISSING },
{ XWS_MISSING,        XWE_NOMORESOCKETS, XWA_NONE,         XWS_DEAD },
{ XWS_CONNECTING,     XWE_NOMORESOCKETS, XWA_NONE,         XWS_DEAD },

{ XWS_INITED,         XWE_RECONNECTMSG,  XWA_SEND_RERSP,   XWS_CHK_ALLHERE_2 },
{ XWS_MISSING,        XWE_RECONNECTMSG,  XWA_SEND_RERSP,   XWS_CHK_ALLHERE_2 },
{ XWS_CHK_ALLHERE_2,  XWE_ALLHERE,       XWA_SNDALLHERE_2, XWS_ALLCONNECTED },
{ XWS_CHK_ALLHERE_2,  XWE_SOMEMISSING,   XWA_NONE,         XWS_MISSING },

{ XWS_CONNECTING,     XWE_REMOVESOCKET,  XWA_REMOVESOCKET, XWS_CONNECTING },
{ XWS_ALLCONNECTED,   XWE_REMOVESOCKET,  XWA_REMOVESOCKET, XWS_MISSING },
{ XWS_MISSING,        XWE_REMOVESOCKET,  XWA_REMOVESOCKET, XWS_MISSING },

{ XWS_ALLCONNECTED,   XWE_HEARTFAILED,   XWA_HEARTDISCONN, XWS_MISSING },
{ XWS_CONNECTING,     XWE_HEARTFAILED,   XWA_HEARTDISCONN, XWS_CONNECTING },
{ XWS_MISSING,        XWE_HEARTFAILED,   XWA_HEARTDISCONN, XWS_MISSING },

    /* Heartbeat arrived */
{ XWS_CONNECTING,     XWE_HEARTRCVD,     XWA_NOTEHEART,    XWS_CONNECTING },
{ XWS_ALLCONNECTED,   XWE_HEARTRCVD,     XWA_NOTEHEART,    XWS_ALLCONNECTED },
{ XWS_MISSING,        XWE_HEARTRCVD,     XWA_NOTEHEART,    XWS_MISSING },

    /* Connect timer */
{ XWS_CONNECTING,     XWE_CONNTIMER,     XWA_TIMERDISCONN, XWS_DEAD },
{ XWS_ALLCONNECTED,   XWE_CONNTIMER,     XWA_NONE,         XWS_ALLCONNECTED },

{ XWS_CONNECTING,     XWE_NOTIFYDISCON,  XWA_NOTIFYDISCON, XWS_CONNECTING },
{ XWS_ALLCONNECTED,   XWE_NOTIFYDISCON,  XWA_NOTIFYDISCON, XWS_MISSING },
{ XWS_MISSING,        XWE_NOTIFYDISCON,  XWA_NOTIFYDISCON, XWS_DEAD },
{ XWS_DEAD,           XWE_NOTIFYDISCON,  XWA_NOTIFYDISCON, XWS_DEAD },

    /* This is our bread-n-butter */
{ XWS_ALLCONNECTED,   XWE_FORWARDMSG,    XWA_FWD,           XWS_ALLCONNECTED },

{ XWS_DEAD,           XWE_REMOVESOCKET,  XWA_REMOVESOCKET,  XWS_DEAD },

    /* Marks end of table */
{ XWS_NONE,           XWE_NONE,          XWA_NONE,          XWS_NONE }
};


int
getFromTable( XW_RELAY_STATE curState, XW_RELAY_EVENT curEvent,
              XW_RELAY_ACTION* takeAction, XW_RELAY_STATE* nextState )
{
    StateTable* stp = g_stateTable;
    while ( stp->stateStart != XWS_NONE ) {
        if ( stp->stateStart == curState ) {
            if ( stp->stateEvent == curEvent || stp->stateEvent == XWE_ANY ) {
                *takeAction = stp->stateAction;
                *nextState = stp->stateEnd;
                return 1;
            }
        }
        ++stp;
    }

    logf( XW_LOGERROR, "==> ERROR :: unable to find transition from %s on event %s",
          stateString(curState), eventString(curEvent) );

    return 0;
} /* getFromTable */

#define CASESTR(s) case s: return #s

char*
stateString( XW_RELAY_STATE state )
{
    switch( state ) {
        CASESTR(XWS_NONE);
        CASESTR(XWS_INITED);
        CASESTR(XWS_CONNECTING);
        CASESTR(XWS_ALLCONNECTED);
        CASESTR(XWS_WAITING_RECON);
        CASESTR(XWS_DEAD);
        CASESTR(XWS_CHECKING_CONN);
        CASESTR(XWS_CHECKINGDEST);
        CASESTR(XWS_CHECKING_CAN_LOCK);
        CASESTR(XWS_MISSING);
        CASESTR(XWS_CHK_ALLHERE);
        CASESTR(XWS_CHK_ALLHERE_2);
        CASESTR(XWS_CHKCOUNTS_INIT);
        CASESTR(XWS_CHKCOUNTS_MISS);
        CASESTR(XWS_CHKCOUNTS);
    }
    assert(0);
    return "";
}

char* 
eventString( XW_RELAY_EVENT evt )
{
    switch( evt ) {
        CASESTR(XWE_NONE);
        CASESTR(XWE_CONNECTMSG);
        CASESTR(XWE_RECONNECTMSG);
        CASESTR(XWE_DISCONNMSG);
        CASESTR(XWE_FORWARDMSG);
        CASESTR(XWE_HEARTRCVD);
        CASESTR(XWE_CONNTIMER);
        CASESTR(XWE_HEARTFAILED);
        CASESTR(XWE_DESTOK);
        CASESTR(XWE_DESTBAD);
        CASESTR(XWE_CAN_LOCK);
        CASESTR(XWE_CANT_LOCK);
        CASESTR(XWE_ANY);
        CASESTR(XWE_REMOVESOCKET);
        CASESTR(XWE_NOMORESOCKETS);
        CASESTR(XWE_NOTIFYDISCON);
        CASESTR(XWE_ALLHERE);
        CASESTR(XWE_SOMEMISSING);
        CASESTR(XWE_OKTOSEND);
        CASESTR(XWE_COUNTSBAD);
    }
    assert(0);
    return "";
}

char*
actString( XW_RELAY_ACTION act ) 
{
    switch ( act ) {
        CASESTR(XWA_NONE);
        CASESTR(XWA_SEND_1ST_RSP);
        CASESTR(XWA_SEND_1ST_RERSP);
        CASESTR(XWA_CHKCOUNTS);
        CASESTR(XWA_REJECT);
        CASESTR(XWA_SEND_RSP);
        CASESTR(XWA_SEND_RERSP);
        CASESTR(XWA_SENDALLHERE);
        CASESTR(XWA_SNDALLHERE_2);
        CASESTR(XWA_FWD);
        CASESTR(XWA_NOTEHEART);
        CASESTR(XWA_DISCONNECTALL);
        CASESTR(XWA_TIMERDISCONN);
        CASESTR(XWA_CHECKDEST);
        CASESTR(XWA_DISCONNECT);
        CASESTR(XWA_NOTIFYDISCON);
        CASESTR(XWA_REMOVESOCKET);
        CASESTR(XWA_CHECK_CAN_LOCK);
        CASESTR(XWA_HEARTDISCONN);
    }
    assert(0);
    return "";
}
#undef CASESTR
