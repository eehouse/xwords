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

#ifndef _XWRELAY_H_
#define _XWRELAY_H_

/* This file is meant to be included by both linux code that doesn't
   include xptypes and from Crosswords client code.*/

/* Set if device is acting a server; cleared if as client */
#define FLAGS_SERVER_BIT 0x01

#ifndef CANT_DO_TYPEDEF
typedef
#endif
enum { XWRELAY_NONE             /* 0 is an illegal value */

       , XWRELAY_GAME_CONNECT
       /* Sent from device to relay to establish connection to relay.  Format:
          flags: 1; cookieLen: 1; cookie: <cookieLen>; hostID: 1; nPlayers: 1;
          nPlayersTotal: 1 */

       , XWRELAY_GAME_RECONNECT
       /* Connect using connName rather than cookie.  Used by a device that's
          lost its connection to a game in progress.  Once a game is locked
          this is the only way a host can get (back) in. Format: flags: 1;
          hostID: 1; nPlayers: 1; nPlayersTotal: 1; connNameLen: 1;
          connName<connNameLen>*/

       , XWRELAY_GAME_DISCONNECT
       /* Tell the relay that we're gone for this game.  After this message is
          sent, the host can reconnect on the same socket for a new game.
          Format: cookieID: 2; srcID: 1 */

       , XWRELAY_CONNECT_RESP
       /* Sent from relay to device in response to XWRELAY_CONNECT.  Format:
          heartbeat_seconds: 2; connectionID: 2; assignedHostID: 1 */

       , XWRELAY_RECONNECT_RESP
       /* Sent from relay to device in response to XWRELAY_RECONNECT.  Format:
          heartbeat_seconds: 2; connectionID: 2; */

       , XWRELAY_ALLHERE
       /* Sent from relay when it enters the state where all expected devices
          are here (at start of new game or after having been gone for a
          while).  Devices should not attempt to forward messages before this
          message is received or after XWRELAY_DISCONNECT_OTHER is received.
          Format: hasName: 1; [nameLen: 1; connName: <nameLen> */

       , XWRELAY_DISCONNECT_YOU
       /* Sent from relay when existing connection is terminated.  
          Format: errorCode: 1 */

       , XWRELAY_DISCONNECT_OTHER
       /* Another device has left the game. 
          Format: errorCode: 1; lostHostId: 1 */

       , XWRELAY_CONNECTDENIED
       /* The relay says go away.  Format: reason code: 1 */

       , XWRELAY_HEARTBEAT
       /* Sent in either direction.  Format: cookieID: 2; srcID: 1 */

       , XWRELAY_MSG_FROMRELAY
       /* Sent from relay to device.  Format: cookieID: 2; src_hostID: 1;
          dest_hostID: 1; data <len-headerLen> */

       , XWRELAY_MSG_TORELAY
       /* Sent from device to relay.  Format: connectionID: 2; src_hostID:
          1; dest_hostID: 1 */
}
#ifndef CANT_DO_TYPEDEF
 XWRelayMsg
#endif
;

#ifndef CANT_DO_TYPEDEF
typedef unsigned char XWRELAY_Cmd;
#endif

#define HOST_ID_NONE   0
#define HOST_ID_SERVER 1

#define MAX_COOKIE_LEN 15
#define MAX_MSG_LEN    256      /* 100 is more like it */
#define MAX_CONNNAME_LEN 35     /* host id plus a small integer, typically */

#define XWRELAY_PROTO_VERSION 0x01

/* Errors passed with denied  */
#ifndef CANT_DO_TYPEDEF
typedef 
#endif
enum {
    XWRELAY_ERROR_NONE
    ,XWRELAY_ERROR_OLDFLAGS    /* You have the wrong flags */
    ,XWRELAY_ERROR_BADPROTO
    ,XWRELAY_ERROR_RELAYBUSY
    ,XWRELAY_ERROR_SHUTDOWN    /* relay's going down */
    ,XWRELAY_ERROR_TIMEOUT     /* Other players didn't show */
    ,XWRELAY_ERROR_HEART_YOU   /* Haven't heard from somebody in too long */
    ,XWRELAY_ERROR_HEART_OTHER /* Haven't heard from other in too long */
    ,XWRELAY_ERROR_LOST_OTHER  /* Generic other-left-we-dunno-why error */

    ,XWRELAY_ERROR_LASTERR
}
#ifndef CANT_DO_TYPEDEF
XWREASON
#endif
;

#ifndef CANT_DO_TYPEDEF
typedef unsigned short CookieID;
#endif

#define COOKIE_ID_NONE 0L

#endif
