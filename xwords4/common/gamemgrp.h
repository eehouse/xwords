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

#ifndef _GAMEMGRP_H_
#define _GAMEMGRP_H_

#include "gamemgr.h"
#include "gamerefp.h"

#ifdef CPLUS
extern "C" {
#endif

void gmgr_init( XW_DUtilCtxt* duc );
void gmgr_cleanup( XW_DUtilCtxt* duc, XWEnv xwe );

void scheduleOnGameAdded( XW_DUtilCtxt* dutil, XWEnv xwe, GameRef gr );
GameData* gmgr_getForRef( XW_DUtilCtxt* dutil, XWEnv xwe, GameRef gr,
                          XP_Bool* deleted );
void gmgr_addGame( XW_DUtilCtxt* dutil, XWEnv xwe, GameData* gd, GameRef gr );
void gmgr_setGD(XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr, GameData* gd );
void gmgr_saveGame( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr );
XWStreamCtxt* gmgr_loadGI( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr );
void gmgr_saveGI( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr );
XWStreamCtxt* gmgr_loadData( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr );
XWStreamCtxt* gmgr_loadSum( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr );
void gmgr_storeSum( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr, XWStreamCtxt* stream );

void gmgr_rmFromGroup( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr, GroupRef grp );
void gmgr_addToGroup( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr, GroupRef grp );
GroupRef gmgr_loadGroupRef( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr );

XP_Bool gmgr_haveGame( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 gameID, DeviceRole role );
void gmgr_onDictAdded( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* dictName );
void gmgr_onDictRemoved( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* dictName );
GroupRef gmgr_getArchiveGroup( XW_DUtilCtxt* duc );

#ifdef CPLUS
}
#endif

#endif
