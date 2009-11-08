/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2001-2007 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _MAIN_H_
#define _MAIN_H_

#ifdef XWFEATURE_BLUETOOTH
# include <bluetooth/bluetooth.h> /* for bdaddr_t, which should move */
#endif

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
    XP_U16 nHidden;
    XP_Bool askNewGame;
    XP_S16 quitAfter;
    XP_Bool sleepOnAnchor;
    XP_Bool printHistory;
    XP_Bool undoWhenDone;
    XP_Bool verticalScore;
    XP_Bool hideValues;
    XP_Bool skipCommitConfirm;
    XP_Bool needsNewGame;
    //    XP_Bool mainParams;
    XP_Bool skipWarnings;
    XP_Bool showRobotScores;
    XP_Bool noHeartbeat;
    XP_Bool duplicatePackets;
#ifdef XWFEATURE_SEARCHLIMIT
    XP_Bool allowHintRect;
#endif

#ifdef XWFEATURE_SLOW_ROBOT
    XP_U16 robotThinkMin, robotThinkMax;
#endif

    DeviceRole serverRole;

    CommsConnType conType;
    struct {
#ifdef XWFEATURE_RELAY
        struct {
            char* relayName;
            char* invite;
            short defaultSendPort;
        } relay;
#endif
#ifdef XWFEATURE_BLUETOOTH
        struct {
            bdaddr_t hostAddr;      /* unused if a host */
        } bt;
#endif
#ifdef XWFEATURE_IP_DIRECT
        struct {
            const char* hostName;
            int port;
        } ip;
#endif
#ifdef XWFEATURE_SMS
        struct {
            const char* serverPhone;
            int port;
        } sms;
#endif
    } connInfo;

    union {
        ServerInfo serverInfo;
        ClientInfo clientInfo;
    } info;

} LaunchParams;

typedef struct CommonGlobals CommonGlobals;

typedef void (*SocketChangedFunc)(void* closure, int oldsock, int newsock,
                                  void** storage );
typedef XP_Bool (*Acceptor)( int sock, void* ctxt );
typedef void (*AddAcceptorFunc)(int listener, Acceptor func, 
                                CommonGlobals* globals, void** storage );

#ifdef XWFEATURE_SMS
typedef struct LinSMSData LinSMSData;
#endif

typedef struct _TimerInfo {
    XWTimerProc proc;
    void* closure;
    XP_U32 when;                /* used only for ncurses */
} TimerInfo;

struct CommonGlobals {
    LaunchParams* params;

    XWGame game;
    XP_U16 lastNTilesToUse;

    SocketChangedFunc socketChanged;
    void* socketChangedClosure;

    CommsRelayState state;

    /* Allow listener sockets to be installed in either gtk or ncurses'
     * polling mechanism.*/
    AddAcceptorFunc addAcceptor;
    Acceptor acceptor;

#ifdef XWFEATURE_RELAY
    int socket;                 /* relay */
    void* storage;
    char* defaultServerName;
#endif

#if defined XWFEATURE_BLUETOOTH
    struct LinBtStuff* btStuff;
#endif
#if defined XWFEATURE_IP_DIRECT
    struct LinUDPStuff* udpStuff;
#endif
#ifdef XWFEATURE_SMS
    LinSMSData* smsData;
#endif

    TimerInfo timerInfo[NUM_TIMERS_PLUS_ONE];
};

#endif
