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

void linux_sms_init( CommonGlobals* globals, const CommsAddrRec* addr );
void linux_sms_close( CommonGlobals* globals );

XP_S16 linux_sms_send( CommonGlobals* globals, const XP_U8* buf,
                       XP_U16 buflen, const XP_UCHAR* phone, XP_U16 port );
XP_S16 linux_sms_receive( CommonGlobals* globals, int sock, 
                          XP_U8* buf, XP_U16 buflen, CommsAddrRec* addr );

#endif /* XWFEATURE_SMS */
#endif /* #ifndef _LINUXSMS_H_ */
