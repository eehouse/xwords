/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2005-2009 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _CESOCKWR_H_
#define _CESOCKWR_H_

#include "comms.h"
#include "mempool.h"

typedef enum {
    CE_IPST_START
    ,CE_IPST_RESOLVINGHOST
    ,CE_IPST_HOSTRESOLVED
    ,CE_IPST_CONNECTING
    ,CE_IPST_CONNECTED
} CeConnState;

typedef struct CeSocketWrapper CeSocketWrapper;      /* forward */
typedef XP_Bool (*DataRecvProc)( XP_U8* data, XP_U16 len, void* closure );
typedef void (*StateChangeProc)( void* closure, CeConnState state );

CeSocketWrapper* ce_sockwrap_new( MPFORMAL HWND hWnd, DataRecvProc dataCB, 
                                  StateChangeProc stateCB, 
                                  void* globals );
void ce_sockwrap_delete( CeSocketWrapper* self );

void ce_sockwrap_hostname( CeSocketWrapper* self, WPARAM wParam, LPARAM lParam );
XP_Bool ce_sockwrap_event( CeSocketWrapper* self, WPARAM wParam, LPARAM lParam );

XP_S16 ce_sockwrap_send( CeSocketWrapper* self, const XP_U8* buf, XP_U16 len, 
                         const CommsAddrRec* addr );

#endif
