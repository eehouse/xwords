/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2018 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _NLITYP_H_
#define _NLITYP_H_

#include "comtypes.h"
#ifdef XWFEATURE_RELAY
# include "xwrelay.h"
#endif

typedef enum {OSType_NONE, OSType_LINUX, OSType_ANDROID, } XP_OSType;

typedef struct _NetLaunchInfo {
    ConnTypeSetBits _conTypes;

    XP_UCHAR gameName[MAX_GAMENAME_LEN+1];
    XP_UCHAR dict[MAX_DICTNAME_LEN+1];
    XP_UCHAR isoCodeStr[MAX_ISO_CODE_LEN+1];
    XP_U8 forceChannel;
    XP_U8 nPlayersT;
    XP_U8 nPlayersH;
    XP_Bool remotesAreRobots;
    XP_Bool inDuplicateMode;

    XP_U32 gameID;
    XP_UCHAR inviteID[32];      /* still used? */

    /* Relay */
    XP_UCHAR room[MAX_INVITE_LEN + 1];
    XP_U32 devID;               /* not used on android; remove?? */

    /* BT */
    XP_UCHAR btName[32];
    XP_UCHAR btAddress[32];

    // SMS
    XP_UCHAR phone[32];
    XP_Bool isGSM;
    XP_OSType osType;           /* used? */
    XP_U32 osVers;

    /* MQTT */
    XP_UCHAR mqttDevID[17];
} NetLaunchInfo;

#endif
