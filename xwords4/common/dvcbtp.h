/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2020 - 2025 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _DVCBTP_H_
#define _DVCBTP_H_

#include "dutil.h"


void parseBTPacket( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_U8* buf, XP_U16 len,
                    const XP_UCHAR* fromName, const XP_UCHAR* fromAddr );

void sendInviteViaBT( XW_DUtilCtxt* dutil, XWEnv xwe, const NetLaunchInfo* nli,
                      const XP_UCHAR* hostName, const XP_BtAddrStr* btAddr );
void sendMsgsViaBT( XW_DUtilCtxt* dutil, XWEnv xwe, const SendMsgsPacket* const packets,
                    const CommsAddrRec* addr, XP_U32 gameID );

#endif
