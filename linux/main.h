/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2001 by Eric House (fixin@peak.org).  All rights reserved.
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

#ifndef _MAIN_H_
#define _MAIN_H_

#include "comtypes.h"
#include "util.h"
#include "game.h"
#include "vtabmgr.h"

typedef struct ServerInfo {
    XP_U16 nRemotePlayers;
/*     CommPipeCtxt* pipe; */
} ServerInfo;

typedef struct ClientInfo {
} ClientInfo;

typedef struct LinuxUtilCtxt {
    UtilVtable* vtable;
} LinuxUtilCtxt;

typedef struct LaunchParams {
/*     CommPipeCtxt* pipe; */
    XW_UtilCtxt* util;
    DictionaryCtxt* dict;
    CurGameInfo gi;
    char* fileName;
    VTableMgr* vtMgr;
    XP_U16 nLocalPlayers;
    XP_Bool trayOverlaps;	/* probably only interesting for GTK case */
    XP_Bool askNewGame;
    XP_Bool quitAfter;
    XP_Bool sleepOnAnchor;
    XP_Bool printHistory;
    XP_Bool undoWhenDone;
    XP_Bool verticalScore;
    //    XP_Bool mainParams;
    XP_Bool skipWarnings;
    XP_Bool showRobotScores;

    Connectedness serverRole;

    char* relayName;
    union {
        ServerInfo serverInfo;
        ClientInfo clientInfo;
    } info;

    short defaultSendPort;
    short defaultListenPort;

} LaunchParams;

typedef struct CommonGlobals {
    LaunchParams* params;

    XWGame game;
    XP_U16 lastNTilesToUse;
    /* UDP comms stuff */
    char* defaultServerName;
    int socket;
} CommonGlobals;

#endif
