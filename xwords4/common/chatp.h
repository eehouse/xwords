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

#ifndef _CHAT_H_
#define _CHAT_H_

#include "comtypes.h"

typedef struct ChatState ChatState;

ChatState* cht_init( XWEnv xwe, XW_UtilCtxt** utilp );
void cht_destroy( ChatState* chat );

XP_Bool cht_haveToWrite( const ChatState* chat );
void cht_loadFromStream( ChatState* chat, XWEnv xwe, XWStreamCtxt* stream );
void cht_writeToStream( const ChatState* state, XWEnv xwe, XWStreamCtxt* stream );

void cht_chatReceived( ChatState* state, XWEnv xwe, XP_UCHAR* msg,
                       XP_S16 from, XP_U32 timestamp );
XP_U16 cht_countChats( ChatState* state );
void cht_getChat( ChatState* state, XP_U16 nn, XP_UCHAR* buf, XP_U16* bufLen,
                  XP_S16* from, XP_U32* timestamp );
void cht_addChat( ChatState* state, XWEnv xwe, const XP_UCHAR* msg, XP_S16 from,
                  XP_U32 timestamp );
void cht_deleteAll( ChatState* state );
#endif
