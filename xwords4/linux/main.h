/* -*- compile-command: "make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 2001-2013 by Eric House (xwords@eehouse.org).  All rights
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

#include <sqlite3.h>

#include "comtypes.h"
#include "util.h"
#include "game.h"
#include "vtabmgr.h"
#include "dictmgr.h"
#include "dutil.h"

typedef struct ServerInfo {
    XP_U16 nRemotePlayers;
/*     CommPipeCtxt* pipe; */
} ServerInfo;

typedef struct ClientInfo {
} ClientInfo;

typedef struct LinuxUtilCtxt {
    UtilVtable* vtable;
} LinuxUtilCtxt;

typedef void (*SockReceiver)( void* closure, int socket );
typedef void (*NewSocketProc)( void* closure, int newSock, int oldSock, 
                               SockReceiver proc, void* procClosure );

typedef struct LaunchParams {
/*     CommPipeCtxt* pipe; */
    CurGameInfo pgi;

    GSList* dictDirs;
    char* fileName;
    char* dbName;
    sqlite3* pDb;               /* null unless opened */
    XP_U16 saveFailPct;
    const XP_UCHAR* playerDictNames[MAX_NUM_PLAYERS];
#ifdef USE_SQLITE
    char* dbFileName;
    XP_U32 dbFileID;
#endif
    void* relayConStorage;      /* opaque outside of relaycon.c */
#ifdef XWFEATURE_SMS
    void* smsStorage;
#endif
    char* pipe;
    char* nbs;
    char* bonusFile;
#ifdef XWFEATURE_DEVID
    char* lDevID;
    XP_Bool noAnonDevid;
    XP_UCHAR devIDStore[32];
#endif
    VTableMgr* vtMgr;
    DictMgrCtxt* dictMgr;
    XW_DUtilCtxt* dutil;
    XP_U16 nLocalPlayers;
    XP_U16 nHidden;
    XP_U16 gameSeed;
    XP_S16 dropNthRcvd;         /* negative means use for random calc */
    XP_U16 nPacketsRcvd;        /* toward dropNthRcvd */
    XP_U16 undoRatio;
    XP_Bool askNewGame;
    XP_S16 quitAfter;
    XP_Bool sleepOnAnchor;
    XP_Bool printHistory;
    XP_Bool undoWhenDone;
    XP_Bool verticalScore;
    XP_Bool hideValues;
    XP_Bool showColors;
    XP_Bool allowPeek;
    XP_Bool sortNewTiles;
    XP_Bool skipCommitConfirm;
    XP_Bool needsNewGame;
    //    XP_Bool mainParams;
    XP_Bool skipWarnings;
    XP_Bool showRobotScores;
    XP_Bool noHeartbeat;
    XP_Bool duplicatePackets;
    XP_Bool skipGameOver;
    XP_Bool useMmap;
    XP_Bool closeStdin;
    XP_Bool useCurses;
    XP_Bool useUdp;
    XP_Bool useHTTP;
    XP_Bool noHTTPAuto;
    XP_U16 splitPackets;
    XP_U16 chatsInterval;       /* 0 means disabled */
    XP_U16 askTimeout;
#ifdef XWFEATURE_SEARCHLIMIT
    XP_Bool allowHintRect;
#endif
#ifdef XWFEATURE_CROSSHAIRS
    XP_Bool hideCrosshairs;
#endif

#ifdef XWFEATURE_SLOW_ROBOT
    XP_U16 robotThinkMin, robotThinkMax;
    XP_U16 robotTradePct;
#endif
    XP_Bool commsDisableds[COMMS_CONN_NTYPES][2];

    DeviceRole serverRole;

    CommsAddrRec  addr;
    struct {
#ifdef XWFEATURE_RELAY
        struct {
            char* relayName;
            char* invite;
            short defaultSendPort;
            XP_Bool seeksPublicRoom;
            XP_Bool advertiseRoom;
        } relay;
#endif
#ifdef XWFEATURE_BLUETOOTH
        struct {
            bdaddr_t hostAddr;      /* unused if a host */
        } bt;
#endif
#if defined XWFEATURE_IP_DIRECT || defined XWFEATURE_DIRECTIP
        struct {
            const char* hostName;
            int hostPort;
            int myPort;
        } ip;
#endif
#ifdef XWFEATURE_SMS
        struct {
            const char* phone;
            int port;
        } sms;
#endif
    } connInfo;

    union {
        ServerInfo serverInfo;
        ClientInfo clientInfo;
    } info;
    MPSLOT
} LaunchParams;

typedef struct CommonGlobals CommonGlobals;

typedef void (*SocketAddedFunc)( void* closure, int newsock, GIOFunc func );
typedef XP_Bool (*Acceptor)( int sock, void* ctxt );
typedef void (*AddAcceptorFunc)(int listener, Acceptor func, 
                                CommonGlobals* globals, void** storage );

typedef struct _TimerInfo {
    XWTimerProc proc;
    void* closure;
#ifdef USE_GLIBLOOP
    struct CommonGlobals* globals;
#else
    XP_U32 when;                /* used only for ncurses */
#endif
} TimerInfo;

typedef void (*OnSaveFunc)( void* closure, sqlite3_int64 rowid,
                            XP_Bool firstTime );

struct CommonGlobals {
    LaunchParams* params;
    CommonPrefs cp;
    XW_UtilCtxt* util;

    XWGame game;
    CurGameInfo* gi;
    CommsAddrRec addr;
    DictionaryCtxt* dict;
    PlayerDicts dicts;
    XP_U16 lastNTilesToUse;
    XP_U16 lastStreamSize;
    XP_U16 nMissing;
    XP_Bool manualFinal;        /* use asked for final scores */
    sqlite3* pDb;
    sqlite3_int64 selRow;

    SocketAddedFunc socketAdded;
    void* socketAddedClosure;
    OnSaveFunc onSave;
    void* onSaveClosure;
    GSList* packetQueue;
    XP_U32 nextPacketID;        /* for debugging */

    CommsRelayState state;

    /* Allow listener sockets to be installed in either gtk or ncurses'
     * polling mechanism.*/
    AddAcceptorFunc addAcceptor;
    Acceptor acceptor;

    /* hash by relayID of lists of messages */
    GHashTable* noConnMsgs;

    /* Saved state from util method to response method */
    XP_U16 selPlayer;
    char question[256*4];
    const XP_UCHAR* askPassName;
    XP_U16 nTiles;
    XP_U16 nToPick;
    XP_U16 blankCol;
    XP_U16 blankRow;
    XP_Bool pickIsInitial;

    const XP_UCHAR* tiles[MAX_UNIQUE_TILES];
    XP_U16 tileCounts[MAX_UNIQUE_TILES];

#ifdef XWFEATURE_RELAY
    int relaySocket;                 /* tcp connection to relay */
    void* storage;
    char* defaultServerName;
#endif

#if defined XWFEATURE_BLUETOOTH
    struct LinBtStuff* btStuff;
#endif
#if defined XWFEATURE_IP_DIRECT
    struct LinUDPStuff* udpStuff;
#endif

    TimerInfo timerInfo[NUM_TIMERS_PLUS_ONE];

    XP_U16 curSaveToken;
};

typedef struct _SourceData {
    GIOChannel* channel;
    gint watch;
    SockReceiver proc;
    void* procClosure;
} SourceData;

#ifdef PLATFORM_GTK
typedef struct _GtkAppGlobals {
    GArray* selRows;
    LaunchParams* params;
    GSList* globalsList;
    GList* sources;
    GtkWidget* window;
    GtkWidget* listWidget;
    GtkWidget* openButton;
    GtkWidget* rematchButton;
    GtkWidget* deleteButton;
    /* save window position */
    GdkEventConfigure lastConfigure;
} GtkAppGlobals;
#endif

#endif
