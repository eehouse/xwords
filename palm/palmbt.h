/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2006 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _PALMBT_H_
#define _PALMBT_H_

#ifdef XWFEATURE_BLUETOOTH

#include "comms.h"
#include "palmmain.h"

/**
 * palm_bt_appendWaitTicks
 * reduce waitTicks if have work to do
 */
void palm_bt_amendWaitTicks( PalmAppGlobals* globals, Int32* result );
XP_Bool palm_bt_doWork( PalmAppGlobals* globals );

typedef void (*DataCb)( PalmAppGlobals* globals, 
                        const XP_U8* data, XP_U16 len );

Err palm_bt_init( PalmAppGlobals* globals, DataCb cb );
void palm_bt_close( PalmAppGlobals* globals );

void palm_bt_addrString( PalmAppGlobals* globals, XP_BtAddr* btAddr, 
                         XP_BtAddrStr* str );

XP_S16 palm_bt_send( const XP_U8* buf, XP_U16 len, const CommsAddrRec* addr,
                     DataCb cb, PalmAppGlobals* globals );

XP_Bool palm_bt_browse_device( PalmAppGlobals* globals, XP_BtAddr* btAddr,
                               XP_UCHAR* out,XP_U16 len );

#else
* define palm_bt_appendWaitTicks( g, r )
#endif /* XWFEATURE_BLUETOOTH */
#endif
