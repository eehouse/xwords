/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */ 
/* 
 * Copyright 1997-2000 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _CURSESMAIN_H_
#define _CURSESMAIN_H_


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/unistd.h>
#include <sys/poll.h>

#include <ncurses.h>

#include "draw.h"
#include "main.h"
#include "board.h"
#include "model.h"
#include "dictnry.h"
#include "xwstream.h"
#include "comms.h"
#include "server.h"
#include "xwstate.h"
#include "util.h"
/* #include "compipe.h" */

typedef struct CursesAppGlobals CursesAppGlobals;

typedef XP_Bool (*EventFunc)(CursesAppGlobals* globals, int ch);
/* typedef void (*MenuDrawer)(CursesAppGlobals* globals); */

#define FD_MAX 6
#define FD_STDIN 0
#define FD_TIMEEVT 1
#define FD_FIRSTSOCKET 2

struct CursesAppGlobals {
    CommonGlobals cGlobals;

    struct CursesDrawCtx* draw;

    DictionaryCtxt* dictionary;
    EngineCtxt* engine;
    CommonPrefs cp;

    XP_Bool amServer;	/* this process acting as server */

    WINDOW* mainWin;
    WINDOW* menuWin;
    WINDOW* boardWin;

    XP_Bool timeToExit;
    XP_Bool doDraw;
    const struct MenuList* menuList;
    XP_U16 nLinesMenu;

    union {
        struct {
            XWStreamCtxt* stream; /* how we can reach the server */
        } client;
        struct {
            int serverSocket;
            XP_Bool socketOpen;
        } server;
    } csInfo;

    short statusLine;
    XWGameState state;

    struct sockaddr_in listenerSockAddr;
    short fdCount;
    struct pollfd fdArray[FD_MAX]; /* one for stdio, one for listening socket */

    int timepipe[2];		/* for reading/writing "user events" */

    XP_U32 nextTimer;
};


DrawCtx* cursesDrawCtxtMake( WINDOW* boardWin );

/* Ports: Client and server pick a port at startup on which they'll listen.
 * If both are to be on the same device using localhost as their ip address,
 * then they need to be listening on different ports.  Server finds out what
 * port client is listening on from the return address of the first message
 * client sends -- I think.  (I'm not sure that when I create a socket to use
 * to SEND to the server that I specify the port on which I'm listening.
 * Maybe I need to include that in a platform-specific part of the connect
 * message....   Clearly there will need to be such a thing.
 */


void cursesmain( XP_Bool isServer, LaunchParams* params );

#endif
