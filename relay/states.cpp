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

StateTable g_stateTable[] = {

    /* Initial msg comes in.  Managing object created in init state, sends response */
    { XW_ST_INITED,            XW_EVENT_CONNECTMSG,  XW_ACTION_SENDRSP,       XW_ST_CONNECTING },

    /* Another connect msg comes in */
    { XW_ST_CONNECTING,        XW_EVENT_CONNECTMSG,  XW_ACTION_SENDRSP,       XW_ST_CONNECTING },
    /* Fwd message comes in, tells us we're now fully connected */
    { XW_ST_CONNECTING,        XW_EVENT_FORWARDMSG,  XW_ACTION_FWD,           XW_ST_ALLCONNECTED },
    { XW_ST_CONNECTING,        XW_EVENT_HEARTMSG,    XW_ACTION_NOTEHEART,     XW_ST_CONNECTING },
    /* Timeout before all connected */
    { XW_ST_CONNECTING,        XW_EVENT_CONNTIMER,   XW_ACTION_DISCONNECTALL, XW_ST_DEAD },
    { XW_ST_CONNECTING,        XW_EVENT_HEARTTIMER,  XW_ACTION_CHECKHEART,    XW_ST_HEARTCHECK_CONNECTING },

    /* This is the entry we'll use most of the time */
    { XW_ST_ALLCONNECTED,      XW_EVENT_FORWARDMSG,  XW_ACTION_FWD,           XW_ST_ALLCONNECTED },
    /* Heartbeat arrived */
    { XW_ST_ALLCONNECTED,      XW_EVENT_HEARTMSG,    XW_ACTION_NOTEHEART,     XW_ST_ALLCONNECTED },

    /* Heartbeat timer means check for dead connections.  Post event to self if there's a problem. */
    { XW_ST_ALLCONNECTED,      XW_EVENT_HEARTTIMER,  XW_ACTION_CHECKHEART,    XW_ST_HEARTCHECK_CONNECTED },

    { XW_ST_HEARTCHECK_CONNECTING,   XW_EVENT_HEARTOK,     XW_ACTION_HEARTOK,       XW_ST_CONNECTING },
    { XW_ST_HEARTCHECK_CONNECTING,   XW_EVENT_HEARTFAILED, XW_ACTION_DISCONNECTALL, XW_ST_DEAD },
    { XW_ST_HEARTCHECK_CONNECTED,    XW_EVENT_HEARTOK,     XW_ACTION_HEARTOK,       XW_ST_ALLCONNECTED },
    { XW_ST_HEARTCHECK_CONNECTED,    XW_EVENT_HEARTFAILED, XW_ACTION_DISCONNECTALL, XW_ST_DEAD },

    /* Reconnect.  Just like a connect but cookieID is supplied.  Can it
       happen in the middle of a game when state is XW_ST_ALLCONNECTED? */


    /* Marks end of table */
    { XW_ST_NONE,              XW_EVENT_NONE,        XW_ACTION_NONE,          XW_ST_NONE }
};


int
getFromTable( XW_RELAY_STATE curState, XW_RELAY_EVENT curEvent,
              XW_RELAY_ACTION* takeAction, XW_RELAY_STATE* nextState )
{
    StateTable* stp = g_stateTable;
    while ( stp->stateStart != XW_ST_NONE ) {
        if ( stp->stateStart == curState && stp->stateEvent == curEvent ) {
            *takeAction = stp->stateAction;
            *nextState = stp->stateEnd;
            return 1;
        }
        ++stp;
    }

    logf( "unable to find transition from %s on event %s",
          stateString(curState), eventString(curEvent) );

    assert(0);
    return 0;
} /* getFromTable */

#define CASESTR(s) case s: return #s

char*
stateString( XW_RELAY_STATE state )
{
    switch( state ) {
        CASESTR(XW_ST_NONE);
        CASESTR(XW_ST_INITED);
        CASESTR(XW_ST_CONNECTING);
        CASESTR(XW_ST_ALLCONNECTED);
        CASESTR(XW_ST_WAITING_RECON);
        CASESTR(XW_ST_SENDING_DISCON);
        CASESTR(XW_ST_DISCONNECTED);
        CASESTR(XW_ST_HEARTCHECK_CONNECTING);
        CASESTR(XW_ST_HEARTCHECK_CONNECTED);
        CASESTR(XW_ST_DEAD);
    }
    assert(0);
    return "";
}

char* 
eventString( XW_RELAY_EVENT evt )
{
    switch( evt ) {
        CASESTR(XW_EVENT_NONE);
        CASESTR(XW_EVENT_CONNECTMSG);
        CASESTR(XW_EVENT_RECONNECTMSG);
        CASESTR(XW_EVENT_FORWARDMSG);
        CASESTR(XW_EVENT_HEARTMSG);
        CASESTR(XW_EVENT_HEARTTIMER);
        CASESTR(XW_EVENT_DISCONTIMER);
        CASESTR(XW_EVENT_CONNTIMER);
        CASESTR(XW_EVENT_HEARTOK);
        CASESTR(XW_EVENT_HEARTFAILED);
    }
    assert(0);
    return "";
}

#undef CASESTR
