/* 
 * Copyright 2024 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _GAMEMGR_H_
#define _GAMEMGR_H_

#include "comtypes.h"
#include "gameinfo.h"
#include "model.h"
#include "board.h"
#include "comms.h"
#include "contrlrp.h"
#include "util.h"
#include "gameref.h"

#ifdef CPLUS
extern "C" {
#endif


    /* GameRef. Idea is it's the only reference platform code has to a game --
       BoardCtxt etc are gone. There's an API here that lets the platforms
       populate a list-type UI with games sorted however, including within
       groups. */

#if 0
XP_U16 gmgr_countGroups( XW_DUtilCtxt* dutil, XWEnv xwe );
typedef XP_U16 XW_GroupID;
void gmgr_getGroups( XW_DUtilCtxt* dutil, XWEnv xwe, XW_GroupID ids[],
                     XP_U16* countP );
XP_U16 gmgr_countGames( XW_DUtilCtxt* dutil, XWEnv xwe, XW_GroupID group );
#endif

typedef enum {
    SO_TURNLOCAL,               /* boolean: my turn? */
    SO_CREATED,
    SO_LASTMOVE,
    SO_LASTMOVE_TS,
    SO_OTHERS_NAMES,            /* other player names */
    SO_GAMENAME,
    SO_CREATED_TS,
    SO_GAMESTATE,               /* Game created, in-play, or over */

    SO_NSOS,
} SORT_ORDER;

// Keep in sync with values in GameMgr.kt
#define GROUP_DEFAULT ((GroupRef)0xFF)
#define GROUP_ARCHIVE ((GroupRef)0xFE)

GameRef gmgr_newFor( XW_DUtilCtxt* dutil, XWEnv xwe, GroupRef grp,
                     const CurGameInfo* gi, const CommsAddrRec* invitee );
GameRef gmgr_addForInvite( XW_DUtilCtxt* dutil, XWEnv xwe, GroupRef grp,
                           const NetLaunchInfo* nli );
void gmgr_getForGID( XW_DUtilCtxt* dutil, XWEnv xwe, XP_U32 gameID,
                     GameRef refs[], XP_U16* nRefs );
void gmgr_setSortOrder( XW_DUtilCtxt* dutil, XWEnv xwe, SORT_ORDER* sos );

void gmgr_deleteGame( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr );

XP_U16 gmgr_countGroups(XW_DUtilCtxt* duc, XWEnv xwe);
GroupRef gmgr_getNthGroup(XW_DUtilCtxt* duc, XWEnv xwe, XP_U16 indx);
#ifdef DEBUG
XP_U16 gmgr_countGames(XW_DUtilCtxt* duc, XWEnv xwe);
GameRef gmgr_getNthGame(XW_DUtilCtxt* duc, XWEnv xwe, XP_U16 indx);
#endif

XWArray* gmgr_getPositions(XW_DUtilCtxt* duc, XWEnv xwe);

XP_Bool gmgr_isGame(GLItemRef ir);
GameRef gmgr_toGame(GLItemRef ir);
GroupRef gmgr_toGroup(GLItemRef ir);

XP_Bool gmgr_getGroupCollapsed( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp );
XP_U32 gmgr_getGroupGamesCount(XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp);
void gmgr_onMessageReceived( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 gameID,
                             const CommsAddrRec* from, XP_U8* msgBuf,
                             XP_U16 msgLen, const MsgCountState* mcs );
void gmgr_clearThumbnails( XW_DUtilCtxt* duc, XWEnv xwe );

    /* Groups */
GroupRef gmgr_addGroup( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* name );
#ifdef XWFEATURE_GAMEREF_CONVERT
GroupRef gmgr_getGroup( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* name );
GameRef gmgr_convertGame( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp,
                          const XP_UCHAR* name, XWStreamCtxt* stream );
XP_Bool gmgr_gameExists( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr );
GameRef gmgr_figureGR( XW_DUtilCtxt* duc, XWEnv xwe, XWStreamCtxt* stream );
#endif
void gmgr_deleteGroup( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp );
void gmgr_raiseGroup( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp );
void gmgr_lowerGroup( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp );
void gmgr_getGroupName( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp,
                        XP_UCHAR buf[], XP_U16 bufLen );
void gmgr_setGroupName( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp,
                        const XP_UCHAR* name );
void gmgr_setGroupCollapsed( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp,
                             XP_Bool collapsed );
void gmgr_makeGroupDefault( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp );
GroupRef gmgr_getDefaultGroup( XW_DUtilCtxt* duc );
void gmgr_moveGames( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp,
                     GameRef games[], XP_U16 nGames  );

#ifdef CPLUS
}
#endif

#endif
