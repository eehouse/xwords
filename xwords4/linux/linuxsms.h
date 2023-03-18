/* -*-mode: C; fill-column: 78; c-basic-offset: 4; compile-command: "make MEMDEBUG=TRUE";-*- */ 
/* 
 * Copyright 2006-2008 by Eric House (xwords@eehouse.org).  All rights
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
#ifndef _LINUXSMS_H_
#define _LINUXSMS_H_

#ifdef XWFEATURE_SMS

#include "main.h"
#include "nli.h"

typedef struct _SMSProcs {
    void (*inviteReceived)( void* closure, const NetLaunchInfo* nli );
    void (*msgReceived)( void* closure, const CommsAddrRec* from, XP_U32 gameID,
                         const XP_U8* buf, XP_U16 len );
    void (*msgNoticeReceived)( void* closure );
    void (*devIDReceived)( void* closure, const XP_UCHAR* devID, 
                           XP_U16 maxInterval );
    void (*msgErrorMsg)( void* closure, const XP_UCHAR* msg );
} SMSProcs;


void linux_sms_init( LaunchParams* params, const gchar* phone, 
                     XP_U16 port, const SMSProcs* procs, void* procClosure );
XP_S16 linux_sms_send( LaunchParams* params, XP_U16 count, SendMsgsPacket msgs[],
                       const XP_UCHAR* phone, XP_U16 port,
                       XP_U32 gameID );
void linux_sms_invite( LaunchParams* params, const NetLaunchInfo* nli,
                       const gchar* phone, int port );
void linux_sms_cleanup( LaunchParams* params );

#endif /* XWFEATURE_SMS */
#endif /* #ifndef _LINUXSMS_H_ */
