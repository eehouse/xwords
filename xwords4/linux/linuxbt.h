/* 
 * Copyright 2006 by Eric House (xwords@eehouse.org).  All rights
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
#ifndef _LINUXBT_H_
#define _LINUXBT_H_

#ifdef XWFEATURE_BLUETOOTH

#include "main.h"

void lbt_init( LaunchParams* params );
void lbt_destroy( LaunchParams* params );

void lbt_reset( LaunchParams* params );
void lbt_close( LaunchParams* params );

XP_S16 lbt_send( LaunchParams* params, const XP_U8* buf, XP_U16 len,
                 const XP_UCHAR* hostName, const XP_BtAddrStr* btAddr );
XP_S16 lbt_receive( int sock, XP_U8* buf, XP_U16 buflen );

void lbt_socketclosed( LaunchParams* params, int sock );

// lbt_scan returns a GSList* of these:
typedef struct _BTHostPair {
    XP_UCHAR hostName[64];
    XP_BtAddrStr btAddr;
} BTHostPair;

GSList* lbt_scan( LaunchParams* params );
void lbt_freeScan( LaunchParams* params, GSList* list );

XP_Bool nameToBtAddr( const char* name, bdaddr_t* ba );

#endif /* XWFEATURE_BLUETOOTH */
#endif /* #ifndef _LINUXBT_H_ */
