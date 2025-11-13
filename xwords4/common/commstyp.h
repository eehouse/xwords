/* 
 * Copyright 2001 - 2020 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _COMMSTYP_H_
#define _COMMSTYP_H_

#include "comtypes.h"
#ifdef XWFEATURE_RELAY
# include "xwrelay.h"
#endif

#define MAX_HOSTNAME_LEN 63
#define MAX_PHONE_LEN    31
#define MAX_P2P_MAC_LEN 17

/* on Palm BtLibDeviceAddressType is a 48-bit quantity.  Linux's typeis the
   same size.  Goal is something all platforms support */
typedef struct XP_BtAddr { XP_U8 bits[6]; } XP_BtAddr;
typedef struct XP_BtAddrStr { XP_UCHAR chars[18]; } XP_BtAddrStr;

typedef struct _IpRelay {
    XP_UCHAR invite[MAX_INVITE_LEN + 1]; /* room!!!! */
    XP_UCHAR hostName[MAX_HOSTNAME_LEN + 1];
    XP_U32 ipAddr;      /* looked up from above */
    XP_U16 port;
    XP_Bool seeksPublicRoom;
    XP_Bool advertiseRoom;
} IpRelay;

typedef enum {
    COMMS_CONN_NONE           /* I want errors on uninited case */
    ,COMMS_CONN_IR            /* 1 */
    ,COMMS_CONN_IP_DIRECT       /* 2 */
    ,COMMS_CONN_RELAY           /* 3 */
    ,COMMS_CONN_BT              /* 4 */
    ,COMMS_CONN_SMS             /* 5 */
    ,COMMS_CONN_P2P             /* a.k.a. Wifi direct */
    ,COMMS_CONN_NFC             /* 7 */
    ,COMMS_CONN_MQTT            /* 8 */

    ,COMMS_CONN_NTYPES
} CommsConnType;

typedef struct _CommsAddrRec {
    ConnTypeSetBits _conTypes;

    struct {
        struct {
            XP_UCHAR hostName_ip[MAX_HOSTNAME_LEN + 1];
            XP_U32 ipAddr_ip;      /* looked up from above */
            XP_U16 port_ip;
        } ip;
#ifdef XWFEATURE_RELAY
        IpRelay ip_relay;
#endif
        struct {
            /* nothing? */
            XP_UCHAR foo;       /* wince doesn't like nothing here */
        } ir;
        struct {
            /* guests can browse for the host to connect to */
            XP_UCHAR hostName[MAX_HOSTNAME_LEN + 1];
            XP_BtAddrStr btAddr;
        } bt;
        struct {
            XP_UCHAR phone[MAX_PHONE_LEN + 1];
            XP_U16   port;
        } sms;
        struct {
            MQTTDevID devID;
        } mqtt;
        struct {
            XP_UCHAR mac_addr[MAX_P2P_MAC_LEN + 1];
        } p2p;
    } u;
} CommsAddrRec;

typedef struct _MsgCountState {
    XP_U16 cur;
    XP_U16 last;
} MsgCountState;


#endif
