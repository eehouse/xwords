/* -*- compile-command: "make MEMDEBUG=TRUE -j5"; -*- */
/* 
 * Copyright 2024 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _EXTCMDS_H_
#define _EXTCMDS_H_

#include <gio/gio.h>

#include "main.h"

typedef struct _CmdWrapper {
    struct {
        void (*quit)( void* closure );
        XP_Bool (*newGame)( void* closure, CurGameInfo* gi, XP_U32* newGameID );
        void (*addInvites)( void* closure, XP_U32 gameID, XP_U16 nRemotes,
                            const CommsAddrRec destAddrs[] );
        void (*newGuest)( void* closure, const NetLaunchInfo* nli );
        XP_Bool (*makeMoveIf)( void* closure, XP_U32 gameID, XP_Bool tryTrade );
        const CommonGlobals* (*getForGameID)( void* closure, XP_U32 gameID );
        XP_Bool (*makeRematch)( void* closure, XP_U32 gameID, RematchOrder ro,
                                XP_U32* newGameIDP );
        XP_Bool (*sendChat)( void* closure, XP_U32 gameID, const char* msg );
        XP_Bool (*undoMove)( void* closure, XP_U32 gameID );
        XP_Bool (*resign)( void* closure, XP_U32 gameID );
        cJSON* (*getKPs)( void* closure );
    } procs;
    LaunchParams* params;
    void* closure;
} CmdWrapper;

GSocketService* cmds_addCmdListener( const CmdWrapper* wr );

#endif
