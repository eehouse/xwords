/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2001 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _MEMSTREAM_H_
#define _MEMSTREAM_H_


#include "comtypes.h"
#include "mempool.h"
#include "vtabmgr.h"

#ifdef CPLUS
extern "C" {
#endif

typedef void (*MemStreamCloseCallback)( XWStreamCtxt* stream,
                                        XWEnv env, void* closure );

XWStreamCtxt* mem_stream_make_raw( MPFORMAL VTableMgr* vtmgr);

XWStreamCtxt* mem_stream_make( MPFORMAL VTableMgr* vtmgr, 
                               void* closure, 
                               XP_PlayerAddr addr,    /* should be in a
                                                         subclass */
                               MemStreamCloseCallback onCloseWritten,
                               XWEnv xwe
                               );

XWStreamCtxt* mem_stream_make_sized( MPFORMAL VTableMgr* vtmgr, 
                                     XP_U16 initialSize,
                                     void* closure, XP_PlayerAddr addr,
                                     MemStreamCloseCallback onCloseWritten,
                                     XWEnv xwe );

#ifdef CPLUS
}
#endif

#endif
