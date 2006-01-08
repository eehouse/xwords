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

#ifndef _VTABMGR_H_
#define _VTABMGR_H_

#include "comtypes.h"
#include "mempool.h"

#ifdef CPLUS
extern "C" {
#endif

typedef enum {
    VTABLE_MEM_STREAM = 0,

    VTABLE_LAST_ENTRY
} VtableType;

typedef struct VTableMgr VTableMgr;

VTableMgr* make_vtablemgr( MPFORMAL_NOCOMMA );
void vtmgr_destroy( MPFORMAL VTableMgr* vtmgr );

void vtmgr_setVTable( VTableMgr* vtmgr, VtableType typ, void* vtable );
void* vtmgr_getVTable( VTableMgr* vtmgr, VtableType typ );

#ifdef CPLUS
}
#endif

#endif


