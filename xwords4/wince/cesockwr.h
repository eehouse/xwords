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
#include "ceconnmg.h"

typedef enum {
    CE_IPST_START
#ifdef _WIN32_WCE
    ,CE_IPST_OPENING_NETWORK
    ,CE_IPST_NETWORK_OPENED
#endif
    ,CE_IPST_RESOLVINGHOST
    ,CE_IPST_HOSTRESOLVED
    ,CE_IPST_CONNECTING
    ,CE_IPST_CONNECTED
} CeConnState;

/* Errors to tell the user about */
typedef enum {
    CONN_ERR_NONE
    ,CONN_ERR_PHONE_OFF
    ,CONN_ERR_NONET
} ConnMgrErr;

typedef struct CeSocketWrapper CeSocketWrapper;      /* forward */
typedef XP_Bool (*DataRecvProc)( XP_U8* data, XP_U16 len, void* closure );
typedef void (*StateChangeProc)( void* closure, CeConnState oldState, 
                                 CeConnState newState );

CeSocketWrapper* ce_sockwrap_new( MPFORMAL HWND hWnd, DataRecvProc dataCB, 
                                  StateChangeProc stateCB, 
#if defined _WIN32_WCE && ! defined CEGCC_DOES_CONNMGR
                                  const CMProcs* cmProcs, 
#endif
                                  void* globals );
void ce_sockwrap_delete( CeSocketWrapper* self );

void ce_sockwrap_hostname( CeSocketWrapper* self, WPARAM wParam, 
                           LPARAM lParam );
XP_Bool ce_sockwrap_event( CeSocketWrapper* self, WPARAM wParam, LPARAM lParam );

XP_S16 ce_sockwrap_send( CeSocketWrapper* self, const XP_U8* buf, XP_U16 len, 
                         const CommsAddrRec* addr );
void ce_connmgr_event( CeSocketWrapper* self, WPARAM wParam, 
                       ConnMgrErr* userErr );

#endif
