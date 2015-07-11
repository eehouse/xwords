/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2001 - 2015 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _INVIT_H_
#define _INVIT_H_

// #include "comms.h"
#include "xwstream.h"
#include "game.h"

#define MAX_GAME_NAME_LEN 64
#define MAX_DICT_NAME_LEN 32

typedef enum {OSType_NONE, OSType_LINUX, OSType_ANDROID, } XP_OSType;

/* InviteInfo
 *
 * A representation of return addresses sent with an invitation so that the
 * recipient has all it needs to create a game and connect back.
 */

typedef struct _InviteInfo {
    XP_U8 version;              /* struct version for backward compatibility */
    XP_U16 _conTypes;

    XP_UCHAR gameName[MAX_GAME_NAME_LEN];
    XP_UCHAR dict[MAX_DICT_NAME_LEN];
    XP_LangCode lang;
    XP_U8 forceChannel;
    XP_U8 nPlayersT;
    XP_U8 nPlayersH;

    /* Relay */
    XP_UCHAR room[MAX_INVITE_LEN + 1];
    XP_U32 devID;

    /* BT */
    XP_UCHAR btName[32];
    XP_UCHAR btAddress[32];

    // SMS
    XP_UCHAR phone[32];
    XP_Bool isGSM;
    XP_OSType osType;
    XP_U32 osVers;

    XP_U32 gameID;
    XP_UCHAR inviteID[32];
} NetLaunchInfo;

void
nli_init( NetLaunchInfo* invit, const CurGameInfo* gi, const CommsAddrRec* addr,
          XP_U16 nPlayers, XP_U16 forceChannel );


XP_Bool nli_makeFromStream( NetLaunchInfo* invit, XWStreamCtxt* stream );
void nli_saveToStream( const NetLaunchInfo* invit, XWStreamCtxt* stream );

void nli_makeAddrRec( const NetLaunchInfo* invit, CommsAddrRec* addr );

void nli_setDevID( NetLaunchInfo* invit, XP_U32 devID );


#endif
