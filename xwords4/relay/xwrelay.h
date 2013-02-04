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

#ifndef _XWRELAY_H_
#define _XWRELAY_H_

/* This file is meant to be included by both linux code that doesn't
   include xptypes and from Crosswords client code.*/

/* Set if device is acting a server; cleared if as client */
#define FLAGS_SERVER_BIT 0x01

/* message types for the udp-based per-device (not per-game) protocol 
 *
 * A number of these rely on a "clientToken", which is a 32-bit value the
 * client provides and that it guarantees uniquely identifies a game on the
 * device.  A database rowid works great as long as they aren't reused.
 */
#define XWPDEV_PROTO_VERSION 0
#ifndef CANT_DO_TYPEDEF
typedef
#endif
enum { XWPDEV_NONE             /* 0 is an illegal value */
       /* All messages have the following six-byte header 
        *    proto: 1 byte
        *    msgID: 4 byte unsigned long, 0 an illegal value
        *    cmd:   1 byte, one of the values below.
        */

       ,XWPDEV_ALERT           /* relay->device: provides a string message to
                                  present to the user (with device allowed not
                                  to present the same string more than once)
                                  format: header, null-terminnated string: varies */
       ,XWPDEV_REG             /* dev->relay: device registers self and
                                  self-selected (e.g. gcm) or assigned devid
                                  format: header, idType: 1,
                                  idLen: 2, id: <idLen> */

       ,XWPDEV_REGRSP          /* relay->device: if non-relay-assigned devid
                                  type was given, this gives the
                                  relay-assigned one to be used from now on.
                                  format: header, idLen: 2, id: <idLen>
                                */

       ,XWPDEV_PING             /* device->relay: keep the UDP connection
                                   open.  header. */

       ,XWPDEV_HAVEMSGS         /* Relay->device: check messages for this
                                   game. format: header */

       ,XWPDEV_RQSTMSGS         /* device->relay: got any messages for me?
                                   format: header, devID: 4 [, clientToken: 4]
                                 */

       ,XWPDEV_MSG             /* dev->relay and relay->dev: norm: a message from a game to
                                  the relay format: header, clientToken: 4, message<varies>*/

       ,XWPDEV_MSGNOCONN        /* dev->relay in the proxy format that
                                   includes relayID (connname:hid) and seems
                                   to be reserved for relay FWD messages.
                                   format: header, clientToken: 4; <cr>-terminated-connname:
                                   varies, message: varies */

       ,XWPDEV_MSGRSP           /* relay->dev: conveys error on receipt of XWPDEV_MSG */

       ,XWPDEV_BADREG           /* relay->dev.  You sent me a relayID via
                                   XWPDEV_REG but I've never heard of it */

       ,XWPDEV_ACK              /* relay->dev (maybe) and dev->relay
                                   (definitely). Tells recipient its message
                                   has been received.  This is for debugging,
                                   and maybe later for timing keepAlives based
                                   on firewall timeouts.  format: header,
                                   msgID: 4
                                */

       ,XWPDEV_DELGAME          /* dev->relay: game's been deleted.  format:
                                   header, relayid: 4, clientToken: 4 */

}
#ifndef CANT_DO_TYPEDEF
 XWRelayReg
#endif
;

#ifndef CANT_DO_TYPEDEF
typedef
#endif
enum { XWRELAY_NONE             /* 0 is an illegal value */

       , XWRELAY_GAME_CONNECT
       /* Sent from device to relay to establish connection to relay.  Format:
          flags: 1; cookieLen: 1; cookie: <cookieLen>; hostID: 1; nPlayers: 1;
          nPlayersTotal: 1 */

       , XWRELAY_GAME_RECONNECT /* 1 */
       /* Connect using connName as well as cookie.  Used by a device that's
          lost its connection to a game in progress.  Once a game is locked
          this is the only way a host can get (back) in. Format: flags: 1;
          cookieLen: 1; cookie: <cookieLen>; hostID: 1; nPlayers: 1;
          nPlayersTotal: 1; connNameLen: 1; connName<connNameLen>*/

       , XWRELAY_ACK            /* 2 */

       , XWRELAY_GAME_DISCONNECT /* 3 */
       /* Tell the relay that we're gone for this game.  After this message is
          sent, the host can reconnect on the same socket for a new game.
          Format: cookieID: 2; srcID: 1 */

       , XWRELAY_CONNECT_RESP   /* 4 */
       /* Sent from relay to device in response to XWRELAY_CONNECT.  Format:
          heartbeat_seconds: 2; players_here: 1; players_sought: 1; */

       , XWRELAY_RECONNECT_RESP /* 5 */
       /* Sent from relay to device in response to XWRELAY_RECONNECT.  Format:
          same as for XWRELAY_CONNECT_RESP */

       , XWRELAY_ALLHERE        /* 6 */
       /* Sent from relay when it enters the state where all expected devices
          are here (at start of new game or after having been gone for a
          while).  Devices should not attempt to forward messages before this
          message is received or after XWRELAY_DISCONNECT_OTHER is received.
          Format: hostID: 1; connectionID: 2; connNameLen: 1;
          connName<connNameLen>; */

       , XWRELAY_ALLBACK__UNUSED /* 7 */
       /* Like XWRELAY_ALLHERE, but indicates a return to all devices being
          present rather than the first time that's achieved.  Has no real
          purpose now that the relay does store-and-forward, but at least lets
          devices tell users everybody's home. */

       , XWRELAY_DISCONNECT_YOU /* 8 */
       /* Sent from relay when existing connection is terminated.  
          Format: errorCode: 1 */

       , XWRELAY_DISCONNECT_OTHER /* 9 */
       /* Another device has left the game. 
          Format: errorCode: 1; lostHostId: 1 */

       , XWRELAY_CONNECTDENIED  /* 10 */
       /* The relay says go away.  Format: reason code: 1 */

       , XWRELAY_HEARTBEAT      /* 11 */
       /* Sent in either direction.  Format: cookieID: 2; srcID: 1 */

       , XWRELAY_MSG_FROMRELAY  /* 12 */
       /* Sent from relay to device.  Format: cookieID: 2; src_hostID: 1;
          dest_hostID: 1; data <len-headerLen> */

       , XWRELAY_MSG_TORELAY    /* 13 */
       /* Sent from device to relay.  Format: connectionID: 2; src_hostID:
          1; dest_hostID: 1 */

       , XWRELAY_MSG_STATUS     /* 14 message conveying status of some sort.
                                   Format: msgCode: 1; varies after that */
       , XWRELAY_MSG_TORELAY_NOCONN    /* 15 same as above, but no cookieID */
       , XWRELAY_MSG_FROMRELAY_NOCONN  /* 16 same as above, but no cookieID */
}
#ifndef CANT_DO_TYPEDEF
 XWRelayMsg
#endif
;

typedef enum {
    ID_TYPE_NONE
    ,ID_TYPE_RELAY              /* assigned by relay as replacement for one of the below */
    ,ID_TYPE_LINUX
    ,ID_TYPE_ANDROID_GCM
    ,ID_TYPE_ANDROID_OTHER
    ,ID_TYPE_ANON               /* please assign me one based on nothing */

    ,ID_TYPE_NTYPES
} DevIDType;

#ifndef CANT_DO_TYPEDEF
typedef unsigned char XWRELAY_Cmd;
#endif

#define HOST_ID_NONE   0
#define HOST_ID_SERVER 1

#define MAX_INVITE_LEN 31
#define MAX_MSG_LEN    2048     /* Used for proxy too! */
#define MAX_CONNNAME_LEN 48     /* host ID, boot time, and seeds as hex? */
#define MAX_DEVID_LEN 8         /* 32-bit number as hex */

#define XWRELAY_PROTO_VERSION_ORIG            0x01
#define XWRELAY_PROTO_VERSION_LATE_NAME       0x02
#define XWRELAY_PROTO_VERSION_LATE_COOKIEID   0x03
#define XWRELAY_PROTO_VERSION_NOCLIENT        0x04
#define XWRELAY_PROTO_VERSION_CLIENTVERS      0x05
#define XWRELAY_PROTO_VERSION_CLIENTID        0x06
#define XWRELAY_PROTO_VERSION XWRELAY_PROTO_VERSION_CLIENTID

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
    ,XWRELAY_ERROR_OTHER_DISCON  /* The other guy disconnected, maybe to start
                                    a new game? */
    ,XWRELAY_ERROR_NO_ROOM
    ,XWRELAY_ERROR_DUP_ROOM
    ,XWRELAY_ERROR_TOO_MANY
    ,XWRELAY_ERROR_DELETED
    ,XWRELAY_ERROR_NORECONN     /* you can't reconnect; reset and try CONNECTING again  */
    ,XWRELAY_ERROR_DEADGAME     /* Some device in this game has been deleted */
    ,XWRELAY_ERROR_LASTERR
}
#ifndef CANT_DO_TYPEDEF
XWREASON
#endif
;

#ifndef CANT_DO_TYPEDEF
typedef
#endif
enum { PRX_NONE             /* 0 is an illegal value */
       ,PRX_PUB_ROOMS       /* list all public rooms for lang/nPlayers */
       ,PRX_HAS_MSGS        /* return message counts for connName/devid array */
       ,PRX_DEVICE_GONE     /* return message counts for connName/devid array */
       ,PRX_GET_MSGS        /* return full messages for connName/devid array */
       ,PRX_PUT_MSGS        /* incoming set of messages with connName/devid header */
}
#ifndef CANT_DO_TYPEDEF
XWPRXYCMD
#endif
;

#ifndef CANT_DO_TYPEDEF
typedef unsigned short CookieID;
#endif

#define COOKIE_ID_NONE 0

#endif
