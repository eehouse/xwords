/* 
 * Copyright 2025 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _GAMEREFP_H_
#define _GAMEREFP_H_

#include "gameref.h"

void gr_dataToStream( DUTIL_GR_XWE, XWStreamCtxt* commsStream,
                      XWStreamCtxt* stream, XP_U16 saveToken );

typedef struct GameData GameData;

GameRef gr_makeForGI( XW_DUtilCtxt* dutil, XWEnv xwe, GroupRef* grp,
                      const CurGameInfo* gi, const CommsAddrRec* hostAddr );
#ifdef XWFEATURE_GAMEREF_CONVERT
GameRef gr_convertGame( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef* grp,
                        const XP_UCHAR* gameName, XWStreamCtxt* stream );
#endif

void gr_freeData( XW_DUtilCtxt* dutil, GameRef gr, XWEnv xwe, GameData* data );
void gr_checkNewDict( XW_DUtilCtxt* dutil, XWEnv xwe, GameData* gd,
                      const DictionaryCtxt* dict );
void gr_checkGoneDict( XW_DUtilCtxt* dutil, XWEnv xwe, GameData* gd,
                       const XP_UCHAR* dictName );
void gr_onMessageReceived( DUTIL_GR_XWE,
                           const CommsAddrRec* from, const XP_U8* msgBuf,
                           XP_U16 msgLen, const MsgCountState* mcs );
void gr_clearThumb( GameData* gd );
void gr_setGroup( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr, GroupRef grp );
void gr_postEvents( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr, GameChangeEvents gces );


# ifdef MEM_DEBUG
MemPoolCtx* gr_getMemPool( DUTIL_GR_XWE );
# endif
#endif
