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

enum { XWRELAY_NONE             /* 0 is an illegal value */

       , XWRELAY_CONNECT
       /* Sent from device to relay to establish connection to relay.  Format:
          flags: 1; cookieLen: 1; cookie: <cookieLen>; hostID:
          2. connectionID: 2.  If connectionID is not 0, host may be
          attempting to reconnect to an existing game. */

       , XWRELAY_CONNECTRESP
       /* Sent from relay to device in response to XWRELAY_CONNECT.
          Format: heartbeat_seconds: 2; connectionID: 2; */

       /* The relay says go away.  Format: reason code: 1 */
       , XWRELAY_CONNECTDENIED

       , XWRELAY_HEARTBEAT
       /* Sent in either direction.  Format: cookieID: 2; srcID: 2 */

       , XWRELAY_MSG_FROMRELAY
       /* Sent from relay to device.  Format: cookieID: 2; src_hostID: 2;
          dest_hostID: 2; data <len-headerLen> */

       , XWRELAY_MSG_TORELAY
       /* Sent from device to relay.  Format: connectionID: 2; src_hostID:
          2; dest_hostID: 2 */
};

#ifndef CANT_DO_TYPEDEF
typedef unsigned char XWRELAY_Cmd;
#endif

#define HOST_ID_NONE   0
#define HOST_ID_SERVER 1

#define MAX_COOKIE_LEN 15
#define MAX_MSG_LEN    256      /* 100 is more like it */

#define XWRELAY_PROTO_VERSION 0x01

/* Errors passed with denied  */
enum {
    XWRELAY_ERROR_NONE
    ,XWRELAY_ERROR_BADPROTO
    ,XWRELAY_ERROR_RELAYBUSY
    ,XWRELAY_ERROR_COOKIEINUSE
};

#endif
