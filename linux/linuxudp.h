/* -*-mode: C; fill-column: 78; c-basic-offset: 4; compile-command: "make MEMDEBUG=TRUE";-*- */ 
/* 
 * Copyright 2007 by Eric House (xwords@eehouse.org).  All rights
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
#ifndef _LINUXUDP_H_
#define _LINUXUD_H_

#ifdef XWFEATURE_IP_DIRECT

#include "main.h"

void linux_udp_open( CommonGlobals* globals, const CommsAddrRec* newAddr );
void linux_udp_reset( CommonGlobals* globals );
void linux_udp_close( CommonGlobals* globals );

XP_S16 linux_udp_send( const XP_U8* buf, XP_U16 buflen, 
                       const CommsAddrRec* addrRec, 
                       CommonGlobals* globals );
XP_S16 linux_udp_receive( int sock, XP_U8* buf, XP_U16 buflen, CommsAddrRec* addr,
                          CommonGlobals* globals );

void linux_udp_socketclosed( CommonGlobals* globals, int sock );

#endif /* XWFEATURE_IP_DIRECT */
#endif /* #ifndef _LINUXUD_H_ */
